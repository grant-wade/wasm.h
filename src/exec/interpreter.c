#include "interpreter.h"
#include <stdio.h>
#include "bulk.h"
#include "calls.h"
#include "control.h"
#include "exceptions.h"
#include "frames.h"
#include "gc.h"
#include "numeric.h"
#include "simd.h"
#include "atomic.h"
#include "../instructions.h"
#include "../internal/alloc.h"
#include "../memory_internal.h"
#include "../module.h"
#include "../portable.h"
#include "../reader.h"
#include "../runtime.h"
#include "../support.h"
#include "../types/canonical.h"
#include "../types/cross_module.h"
#include "../types/references.h"
#include "../values.h"
#include <string.h>

/* Immediates almost always fit in one LEB128 byte; longer ones take the general decoders, which
   also set ptr to end on failure, so ptr < end means no read has failed yet. */
WASM_INLINE uint32_t wasm__imm_u32(wasm__reader_t* r) {
    if (r->ptr < r->end && *r->ptr < 0x80u) return *r->ptr++;
    return wasm__read_leb128_u32(r);
}

WASM_INLINE int32_t wasm__imm_i32(wasm__reader_t* r) {
    if (r->ptr < r->end && *r->ptr < 0x80u) {
        uint32_t byte = *r->ptr++;

        return (int32_t)((byte & 0x40u) ? (byte | ~0x7Fu) : byte);
    }
    return wasm__read_leb128_i32(r);
}

/* Most memargs are two single-byte LEB128s: alignment flags without a memory index, then the offset. */
WASM_INLINE wasm_error_t wasm__imm_memarg(wasm__reader_t* r, wasm__memarg_t* memarg) {
    if (r->end - r->ptr >= 2 && r->ptr[0] < 0x40u && r->ptr[1] < 0x80u) {
        memarg->align_log2 = r->ptr[0];
        memarg->memory_index = 0;
        memarg->has_memory_index = 0;
        memarg->offset = r->ptr[1];
        r->ptr += 2;
        return WASM_OK;
    }
    return wasm__read_memarg(r, memarg);
}

static wasm_error_t wasm__invoke_host_func(wasm_module_t* mod,
                                           wasm_host_func_t host_func,
                                           const wasm_value_t* args,
                                           uint32_t num_args,
                                           wasm_value_t* results,
                                           uint32_t num_results,
                                           void* userdata) {
    if (!mod || !mod->rt || !host_func) return WASM_ERR_MALFORMED;
    return host_func(mod, args, num_args, results, num_results, userdata);
}

/* Hands an exception raised by a host import to the nearest Wasm handler. A tail call has
   already given up its frame, so that frame's handlers are skipped. Returns
   WASM__ERR_RESUME_FRAME once a handler is found. */
static wasm_error_t wasm__unwind_host_exception(wasm_module_t* mod,
                                                wasm__call_frame_t* cf,
                                                uint32_t base_frame_sp,
                                                int is_tail_call) {
    wasm_error_t err;

    if (is_tail_call) {
        while (cf->lsp > 0) {
            wasm__clear_label(&cf->labels[cf->lsp - 1]);
            cf->lsp--;
        }
    }
    err = wasm__handle_exception_frames(mod, base_frame_sp);
    return err == WASM_OK ? WASM__ERR_RESUME_FRAME : err;
}

/* The frame records where it is only when control leaves it: backtraces and host functions read
   that position, and storing it for every instruction would cost more than it saves. */
#define WASM__SYNC_OFFSET() (cf->current_offset = (uint32_t)(op_at - func->code))
#define WASM__LEAVE(value)   \
    do {                     \
        WASM__SYNC_OFFSET(); \
        return (value);      \
    } while (0)

WASM_PRIVATE void wasm__init_call_frame(wasm__call_frame_t* cf,
                                  wasm_module_t* mod,
                                  wasm_runtime_t* rt,
                                  uint32_t func_idx,
                                  wasm_func_t* func,
                                  wasm_functype_t* ft,
                                  wasm_value_t* locals,
                                  wasm__label_t* labels,
                                  wasm_value_t* results_dst,
                                  uint32_t num_results) {
    memset(cf, 0, sizeof(*cf));
    cf->module = mod;
    cf->func_idx = func_idx;
    cf->sp_base = rt->sp;
    cf->locals = locals;
    cf->labels = labels;
    cf->lsp = 0;
    cf->max_labels = func->max_label_depth ? func->max_label_depth : 1;
    cf->r.ptr = func->code;
    cf->r.end = func->code + func->code_len;
    cf->r.malformed = 0;
    cf->func = func;
    cf->ft = ft;
    cf->results_dst = results_dst;
    cf->num_results = num_results;
    cf->arena_saved = rt->frame_arena_offset;
    cf->current_offset = 0;

    labels[cf->lsp].continuation = cf->r.end;
    labels[cf->lsp].handler_ptr = NULL;
    labels[cf->lsp].sp_base = cf->sp_base;
    labels[cf->lsp].param_arity = 0;
    labels[cf->lsp].arity = ft->num_results;
    labels[cf->lsp].caught_tag = UINT32_MAX;
    labels[cf->lsp].caught_count = 0;
    labels[cf->lsp].caught_values = NULL;
    labels[cf->lsp].delegate_depth = UINT32_MAX;
    labels[cf->lsp].opcode = 0x02;
    labels[cf->lsp].active_catch = 0;
    cf->lsp++;
}

static wasm_error_t wasm__interp_loop(wasm_module_t* mod,
                                      wasm__call_frame_t* cf,
                                      wasm_module_t** next_module,
                                      uint32_t* next_func_idx,
                                      wasm_value_t** next_args,
                                      uint32_t* next_num_args,
                                      int* next_args_owned,
                                      wasm_value_t* tail_args_buf,
                                      uint32_t base_frame_sp) {
    wasm_runtime_t* rt = mod->rt;
    wasm_func_t* func;
    const uint8_t* op_at;
    uint32_t sp_base;
    uint32_t i;
    wasm_error_t err = WASM_OK;

    if (!cf) return WASM_ERR_MALFORMED;
    func = cf->func;
    sp_base = cf->sp_base;
    op_at = cf->r.ptr;

    while (cf->r.ptr < cf->r.end) {
        if (rt->fuel_enabled) {
            if (rt->fuel == 0) WASM__TRAP(WASM_ERR_OUT_OF_FUEL);
            rt->fuel--;
        }

        uint8_t op;

        op_at = cf->r.ptr;
        op = wasm__read_u8(&cf->r);
        switch (op) {
            case 0x00:
                WASM__TRAP(WASM_ERR_UNREACHABLE);
            case 0x01:
                break;

            case 0x02: { /* block */
                wasm__blocktype_t blocktype;
                err = wasm__read_blocktype(mod, &cf->r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                {
                    const wasm__control_target_t* target = wasm__control_target_at(func, op_at);
                    const uint8_t* be;

                    if (!target) WASM__TRAP(WASM_ERR_MALFORMED);
                    be = func->code + target->end_offset;
                    if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                    if (cf->lsp >= cf->max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                    cf->labels[cf->lsp].continuation = be;
                    cf->labels[cf->lsp].handler_ptr = NULL;
                    cf->labels[cf->lsp].sp_base = rt->sp - blocktype.param_arity;
                    cf->labels[cf->lsp].param_arity = blocktype.param_arity;
                    cf->labels[cf->lsp].arity = blocktype.result_arity;
                    cf->labels[cf->lsp].caught_tag = UINT32_MAX;
                    cf->labels[cf->lsp].caught_count = 0;
                    cf->labels[cf->lsp].caught_values = NULL;
                    cf->labels[cf->lsp].delegate_depth = UINT32_MAX;
                    cf->labels[cf->lsp].opcode = 0x02;
                    cf->labels[cf->lsp].active_catch = 0;
                    cf->lsp++;
                }
                break;
            }
            case 0x03: { /* loop */
                wasm__blocktype_t blocktype;
                const uint8_t* ls;
                err = wasm__read_blocktype(mod, &cf->r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                ls = cf->r.ptr;
                if (cf->lsp >= cf->max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                cf->labels[cf->lsp].continuation = ls;
                cf->labels[cf->lsp].handler_ptr = NULL;
                cf->labels[cf->lsp].sp_base = rt->sp - blocktype.param_arity;
                cf->labels[cf->lsp].param_arity = blocktype.param_arity;
                cf->labels[cf->lsp].arity = blocktype.result_arity;
                cf->labels[cf->lsp].caught_tag = UINT32_MAX;
                cf->labels[cf->lsp].caught_count = 0;
                cf->labels[cf->lsp].caught_values = NULL;
                cf->labels[cf->lsp].delegate_depth = UINT32_MAX;
                cf->labels[cf->lsp].opcode = 0x03;
                cf->labels[cf->lsp].active_catch = 0;
                cf->lsp++;
                break;
            }
            case 0x04: { /* if */
                wasm__blocktype_t blocktype;
                wasm_value_t cond = WASM__POP(rt);
                err = wasm__read_blocktype(mod, &cf->r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                {
                    const wasm__control_target_t* target = wasm__control_target_at(func, op_at);
                    const uint8_t* ep = NULL;
                    const uint8_t* be;

                    if (!target) WASM__TRAP(WASM_ERR_MALFORMED);
                    be = func->code + target->end_offset;
                    if (target->aux_offset != UINT32_MAX)
                        ep = func->code + target->aux_offset;
                    if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                    if (!cond.of.i32) {
                        if (!ep) {
                            cf->r.ptr = be;
                            break;
                        }
                        cf->r.ptr = ep;
                    }
                    if (cf->lsp >= cf->max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                    cf->labels[cf->lsp].continuation = be;
                    cf->labels[cf->lsp].handler_ptr = NULL;
                    cf->labels[cf->lsp].sp_base = rt->sp - blocktype.param_arity;
                    cf->labels[cf->lsp].param_arity = blocktype.param_arity;
                    cf->labels[cf->lsp].arity = blocktype.result_arity;
                    cf->labels[cf->lsp].caught_tag = UINT32_MAX;
                    cf->labels[cf->lsp].caught_count = 0;
                    cf->labels[cf->lsp].caught_values = NULL;
                    cf->labels[cf->lsp].delegate_depth = UINT32_MAX;
                    cf->labels[cf->lsp].opcode = 0x04;
                    cf->labels[cf->lsp].active_catch = 0;
                    cf->lsp++;
                }
                break;
            }
            case 0x06: { /* try */
                wasm__blocktype_t blocktype;
                const wasm__control_target_t* target;
                const uint8_t* be;
                const uint8_t* first_clause;

                err = wasm__read_blocktype(mod, &cf->r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                target = wasm__control_target_at(func, op_at);
                if (!target) WASM__TRAP(WASM_ERR_MALFORMED);
                be = func->code + target->end_offset;
                first_clause = (target->aux_offset == UINT32_MAX) ? NULL : (func->code + target->aux_offset);
                if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (cf->lsp >= cf->max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                cf->labels[cf->lsp].continuation = be;
                cf->labels[cf->lsp].handler_ptr = first_clause;
                cf->labels[cf->lsp].sp_base = rt->sp - blocktype.param_arity;
                cf->labels[cf->lsp].param_arity = blocktype.param_arity;
                cf->labels[cf->lsp].arity = blocktype.result_arity;
                cf->labels[cf->lsp].caught_tag = UINT32_MAX;
                cf->labels[cf->lsp].caught_count = 0;
                cf->labels[cf->lsp].caught_values = NULL;
                cf->labels[cf->lsp].delegate_depth = UINT32_MAX;
                cf->labels[cf->lsp].opcode = 0x06;
                cf->labels[cf->lsp].active_catch = 0;
                cf->lsp++;
                break;
            }
            case 0x05:
                if (cf->lsp > 0) {
                    err = wasm__leave_label(rt, &cf->labels[cf->lsp - 1]);
                    if (err != WASM_OK) goto cleanup;
                    wasm__clear_label(&cf->labels[cf->lsp - 1]);
                    cf->r.ptr = cf->labels[cf->lsp - 1].continuation;
                    cf->lsp--;
                }
                break;
            case 0x07:
            case 0x18:
            case 0x19:
                if (cf->lsp > 0 && cf->labels[cf->lsp - 1].opcode == 0x06) {
                    err = wasm__leave_label(rt, &cf->labels[cf->lsp - 1]);
                    if (err != WASM_OK) goto cleanup;
                    wasm__clear_label(&cf->labels[cf->lsp - 1]);
                    cf->r.ptr = cf->labels[cf->lsp - 1].continuation;
                    cf->lsp--;
                } else {
                    err = WASM_ERR_MALFORMED;
                    goto cleanup;
                }
                break;
            case 0x08: { /* throw */
                uint32_t tag_index = wasm__imm_u32(&cf->r);
                wasm_functype_t* tag_type;
                wasm_value_t payload_buf[8];
                wasm_value_t* payload = payload_buf;
                int j;

                if (tag_index >= mod->num_tags) WASM__TRAP(WASM_ERR_MALFORMED);
                tag_type = wasm__module_functype(mod, mod->tags[tag_index].type_idx);
                if (!tag_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if (tag_type->num_params > 8) {
                    payload = (wasm_value_t*)wasm__malloc_array(tag_type->num_params, sizeof(wasm_value_t));
                    if (!payload) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)tag_type->num_params - 1; j >= 0; j--) payload[j] = WASM__POP(rt);
                err = wasm__set_exception_state(&rt->pending_exception,
                                                tag_index,
                                                mod->tags[tag_index].identity,
                                                payload,
                                                tag_type->num_params);
                if (payload != payload_buf) WASM_FREE(payload);
                if (err != WASM_OK) goto cleanup;
                err = wasm__handle_exception_frames(mod, base_frame_sp);
                if (err == WASM_OK) WASM__LEAVE(WASM__ERR_RESUME_FRAME);
                if (err == WASM__ERR_EXCEPTION) WASM__LEAVE(err);
                goto cleanup;
            }
            case 0x09: { /* rethrow */
                uint32_t depth_index = wasm__imm_u32(&cf->r);
                uint32_t target_index;

                if (!wasm__find_rethrow_label(cf->labels, cf->lsp, depth_index, &target_index)) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__set_exception_state(&rt->pending_exception,
                                                cf->labels[target_index].caught_tag,
                                                cf->labels[target_index].caught_tag_identity,
                                                cf->labels[target_index].caught_values,
                                                cf->labels[target_index].caught_count);
                if (err != WASM_OK) goto cleanup;
                while (cf->lsp > target_index) {
                    wasm__clear_label(&cf->labels[cf->lsp - 1]);
                    rt->sp = cf->labels[cf->lsp - 1].sp_base;
                    cf->lsp--;
                }
                err = wasm__handle_exception_frames(mod, base_frame_sp);
                if (err == WASM_OK) WASM__LEAVE(WASM__ERR_RESUME_FRAME);
                if (err == WASM__ERR_EXCEPTION) WASM__LEAVE(err);
                goto cleanup;
            }
            case 0x0A: { /* throw_ref */
                wasm_value_t exnref_value = WASM__POP(rt);
                wasm__exception_object_t* exn = wasm__exception_object_from_exnref(&exnref_value);

                if (!exn) WASM__TRAP(WASM_ERR_TRAP);
                err = wasm__set_exception_state(&rt->pending_exception,
                                                exn->tag_index,
                                                exn->tag_identity,
                                                exn->values,
                                                exn->num_values);
                if (err != WASM_OK) goto cleanup;
                err = wasm__handle_exception_frames(mod, base_frame_sp);
                if (err == WASM_OK) WASM__LEAVE(WASM__ERR_RESUME_FRAME);
                if (err == WASM__ERR_EXCEPTION) WASM__LEAVE(err);
                goto cleanup;
            }
            case 0x0B:
                if (cf->lsp > 0) {
                    err = wasm__leave_label(rt, &cf->labels[cf->lsp - 1]);
                    if (err != WASM_OK) goto cleanup;
                    wasm__clear_label(&cf->labels[cf->lsp - 1]);
                    cf->lsp--;
                }
                break;

            case 0x0C: {
                uint32_t d = wasm__imm_u32(&cf->r);
                if (d >= cf->lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__do_branch(rt, cf->labels, &cf->labels[cf->lsp - 1 - d], &cf->r, &cf->lsp, d);
                if (err != WASM_OK) goto cleanup;
                break;
            }
            case 0x0D: {
                uint32_t d = wasm__imm_u32(&cf->r);
                wasm_value_t c = WASM__POP(rt);
                if (c.of.i32) {
                    if (d >= cf->lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                    err = wasm__do_branch(rt, cf->labels, &cf->labels[cf->lsp - 1 - d], &cf->r, &cf->lsp, d);
                    if (err != WASM_OK) goto cleanup;
                }
                break;
            }
            case 0x0E: { /* br_table */
                uint32_t nt = wasm__imm_u32(&cf->r), j, d = 0;
                uint32_t selected = (uint32_t)WASM__POP(rt).of.i32;

                if (selected > nt) selected = nt;
                for (j = 0; j <= nt; j++) {
                    uint32_t target_depth = wasm__imm_u32(&cf->r);

                    if (j == selected) d = target_depth;
                }
                if (d >= cf->lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__do_branch(rt, cf->labels, &cf->labels[cf->lsp - 1 - d], &cf->r, &cf->lsp, d);
                if (err != WASM_OK) goto cleanup;
                break;
            }
            case 0x0F: /* return */
                err = WASM__ERR_RETURN_FRAME;
                goto cleanup_frame;

            case 0x10: { /* call */
                uint32_t ci = wasm__imm_u32(&cf->r);
                wasm_functype_t* cft;
                wasm_value_t* call_args = tail_args_buf;
                wasm_value_t call_results_buf[8];
                wasm_value_t* call_results = call_results_buf;
                wasm_func_t* callee;
                int j;

                if (ci >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &mod->funcs[ci];
                cft = wasm__module_functype(mod, mod->funcs[ci].type_idx);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);
                if (cft->num_params > 8) {
                    call_args = (wasm_value_t*)wasm__malloc_array(cft->num_params, sizeof(wasm_value_t));
                    if (!call_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) call_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        call_results = (wasm_value_t*)wasm__malloc_array(cft->num_results, sizeof(wasm_value_t));
                        if (!call_results) {
                            if (call_args != tail_args_buf) WASM_FREE(call_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(call_results, 0, cft->num_results * sizeof(wasm_value_t));
                    WASM__SYNC_OFFSET();
                    err = wasm__invoke_host_func(mod, callee->host_func,
                                                 call_args, cft->num_params,
                                                 call_results, cft->num_results,
                                                 callee->host_userdata);
                    if (call_args != tail_args_buf) WASM_FREE(call_args);
                    if (err != WASM_OK) {
                        if (call_results != call_results_buf) WASM_FREE(call_results);
                        if (err == WASM__ERR_EXCEPTION) {
                            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 0);
                            if (err == WASM__ERR_RESUME_FRAME || err == WASM__ERR_EXCEPTION) WASM__LEAVE(err);
                        }
                        goto cleanup;
                    }
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, call_results[i]);
                    if (call_results != call_results_buf) WASM_FREE(call_results);
                    break;
                }

                if (next_module) *next_module = mod;
                *next_func_idx = ci;
                *next_args = call_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (call_args != tail_args_buf);
                WASM__LEAVE(WASM__ERR_CALL_FRAME);
                break;
            }
            case 0x11: { /* call_indirect */
                uint32_t ti = wasm__imm_u32(&cf->r), table_idx, ci;
                uint64_t tvi;
                wasm_module_t* callee_mod;
                wasm_functype_t* cft;
                wasm_func_t* callee;
                wasm_table_t* table;
                wasm_value_t call_results_buf[8], iv;
                wasm_value_t* call_args = tail_args_buf;
                wasm_value_t* call_results = call_results_buf;
                int j;
                table_idx = wasm__imm_u32(&cf->r);
                iv = WASM__POP(rt);
                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                tvi = wasm__table_index_value(table, &iv);
                if (table->reftype != WASM_TYPE_FUNCREF) WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (tvi >= table->size || !table->elems) WASM__TRAP(WASM_ERR_UNDEFINED_ELEMENT);
                if (wasm__is_null_ref(&table->elems[tvi])) WASM__TRAP(WASM_ERR_UNINITIALIZED_TABLE);
                if (!wasm__resolve_func_ref(rt, table->elems[tvi].of.funcref, &callee_mod, &ci))
                    WASM__TRAP(WASM_ERR_MALFORMED);
                if (!callee_mod || ci >= callee_mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &callee_mod->funcs[ci];
                cft = wasm__module_functype(mod, ti);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);
                if ((callee_mod != mod || callee->type_idx != ti) &&
                    !wasm__functype_is_subtype_across_modules(callee_mod, callee->type_idx, mod, ti))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (cft->num_params > 8) {
                    call_args = (wasm_value_t*)wasm__malloc_array(cft->num_params, sizeof(wasm_value_t));
                    if (!call_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) call_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        call_results = (wasm_value_t*)wasm__malloc_array(cft->num_results, sizeof(wasm_value_t));
                        if (!call_results) {
                            if (call_args != tail_args_buf) WASM_FREE(call_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(call_results, 0, cft->num_results * sizeof(wasm_value_t));
                    WASM__SYNC_OFFSET();
                    err = wasm__invoke_host_func(callee_mod, callee->host_func,
                                                 call_args, cft->num_params,
                                                 call_results, cft->num_results,
                                                 callee->host_userdata);
                    if (call_args != tail_args_buf) WASM_FREE(call_args);
                    if (err != WASM_OK) {
                        if (call_results != call_results_buf) WASM_FREE(call_results);
                        if (err == WASM__ERR_EXCEPTION) {
                            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 0);
                            if (err == WASM__ERR_RESUME_FRAME || err == WASM__ERR_EXCEPTION) WASM__LEAVE(err);
                        }
                        goto cleanup;
                    }
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, call_results[i]);
                    if (call_results != call_results_buf) WASM_FREE(call_results);
                    break;
                }

                if (next_module) *next_module = callee_mod;
                *next_func_idx = ci;
                *next_args = call_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (call_args != tail_args_buf);
                WASM__LEAVE(WASM__ERR_CALL_FRAME);
                break;
            }
            case 0x12: { /* return_call */
                uint32_t ci = wasm__imm_u32(&cf->r);
                wasm_functype_t* cft;
                wasm_value_t* tail_args = tail_args_buf;
                wasm_value_t tail_results_buf[8];
                wasm_value_t* tail_results = tail_results_buf;
                wasm_func_t* callee;
                int j;

                if (ci >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &mod->funcs[ci];
                cft = wasm__module_functype(mod, mod->funcs[ci].type_idx);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);
                if (cft->num_params > 8) {
                    tail_args = (wasm_value_t*)wasm__malloc_array(cft->num_params, sizeof(wasm_value_t));
                    if (!tail_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) tail_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        tail_results = (wasm_value_t*)wasm__malloc_array(cft->num_results, sizeof(wasm_value_t));
                        if (!tail_results) {
                            if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(tail_results, 0, cft->num_results * sizeof(wasm_value_t));
                    WASM__SYNC_OFFSET();
                    err = wasm__invoke_host_func(mod, callee->host_func,
                                                 tail_args, cft->num_params,
                                                 tail_results, cft->num_results,
                                                 callee->host_userdata);
                    if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                    if (err != WASM_OK) {
                        if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                        if (err == WASM__ERR_EXCEPTION) {
                            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 1);
                            if (err == WASM__ERR_RESUME_FRAME || err == WASM__ERR_EXCEPTION) WASM__LEAVE(err);
                        }
                        goto cleanup;
                    }

                    rt->sp = sp_base;
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, tail_results[i]);
                    if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                    err = WASM__ERR_RETURN_FRAME;
                    goto cleanup_frame;
                }

                rt->sp = sp_base;
                if (next_module) *next_module = mod;
                *next_func_idx = ci;
                *next_args = tail_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (tail_args != tail_args_buf);
                err = WASM__ERR_TAIL_CALL_FRAME;
                goto cleanup_frame;
            }
            case 0x13: { /* return_call_indirect */
                uint32_t ti = wasm__imm_u32(&cf->r), table_idx, ci;
                uint64_t tvi;
                wasm_module_t* callee_mod;
                wasm_functype_t* cft;
                wasm_table_t* table;
                wasm_value_t* tail_args = tail_args_buf;
                wasm_value_t tail_results_buf[8];
                wasm_value_t* tail_results = tail_results_buf;
                wasm_value_t iv;
                wasm_func_t* callee;
                int j;

                table_idx = wasm__imm_u32(&cf->r);
                iv = WASM__POP(rt);
                if (ti >= mod->num_types || table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                tvi = wasm__table_index_value(table, &iv);
                if (table->reftype != WASM_TYPE_FUNCREF) WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (tvi >= table->size || !table->elems) WASM__TRAP(WASM_ERR_UNDEFINED_ELEMENT);
                if (wasm__is_null_ref(&table->elems[tvi])) WASM__TRAP(WASM_ERR_UNINITIALIZED_TABLE);
                if (!wasm__resolve_func_ref(rt, table->elems[tvi].of.funcref, &callee_mod, &ci))
                    WASM__TRAP(WASM_ERR_MALFORMED);
                if (!callee_mod || ci >= callee_mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &callee_mod->funcs[ci];
                cft = wasm__module_functype(mod, ti);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);
                if ((callee_mod != mod || callee->type_idx != ti) &&
                    !wasm__functype_is_subtype_across_modules(callee_mod, callee->type_idx, mod, ti))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);

                if (cft->num_params > 8) {
                    tail_args = (wasm_value_t*)wasm__malloc_array(cft->num_params, sizeof(wasm_value_t));
                    if (!tail_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) tail_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        tail_results = (wasm_value_t*)wasm__malloc_array(cft->num_results, sizeof(wasm_value_t));
                        if (!tail_results) {
                            if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(tail_results, 0, cft->num_results * sizeof(wasm_value_t));
                    WASM__SYNC_OFFSET();
                    err = wasm__invoke_host_func(callee_mod, callee->host_func,
                                                 tail_args, cft->num_params,
                                                 tail_results, cft->num_results,
                                                 callee->host_userdata);
                    if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                    if (err != WASM_OK) {
                        if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                        if (err == WASM__ERR_EXCEPTION) {
                            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 1);
                            if (err == WASM__ERR_RESUME_FRAME || err == WASM__ERR_EXCEPTION) WASM__LEAVE(err);
                        }
                        goto cleanup;
                    }

                    rt->sp = sp_base;
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, tail_results[i]);
                    if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                    err = WASM__ERR_RETURN_FRAME;
                    goto cleanup_frame;
                }

                rt->sp = sp_base;
                if (next_module) *next_module = callee_mod;
                *next_func_idx = ci;
                *next_args = tail_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (tail_args != tail_args_buf);
                err = WASM__ERR_TAIL_CALL_FRAME;
                goto cleanup_frame;
            }
            case 0x14: { /* call_ref */
                uint32_t ti = wasm__imm_u32(&cf->r), ci;
                wasm_module_t* callee_mod;
                wasm_functype_t* cft;
                wasm_func_t* callee;
                wasm_value_t funcref_value;
                wasm_value_t* call_args = tail_args_buf;
                wasm_value_t call_results_buf[8];
                wasm_value_t* call_results = call_results_buf;
                int j;

                if (ti >= mod->num_types) WASM__TRAP(WASM_ERR_MALFORMED);
                cft = wasm__module_functype(mod, ti);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);

                funcref_value = WASM__POP(rt);
                if (wasm__is_null_ref(&funcref_value)) {
                    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null function reference");
                    err = WASM_ERR_TRAP;
                    goto cleanup;
                }
                if (!wasm__resolve_func_ref(rt, funcref_value.of.funcref, &callee_mod, &ci))
                    WASM__TRAP(WASM_ERR_MALFORMED);
                if (!callee_mod || ci >= callee_mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &callee_mod->funcs[ci];
                if ((callee_mod != mod || callee->type_idx != ti) &&
                    !wasm__functype_is_subtype_across_modules(callee_mod, callee->type_idx, mod, ti))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);

                if (cft->num_params > 8) {
                    call_args = (wasm_value_t*)wasm__malloc_array(cft->num_params, sizeof(wasm_value_t));
                    if (!call_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) call_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        call_results = (wasm_value_t*)wasm__malloc_array(cft->num_results, sizeof(wasm_value_t));
                        if (!call_results) {
                            if (call_args != tail_args_buf) WASM_FREE(call_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(call_results, 0, cft->num_results * sizeof(wasm_value_t));
                    WASM__SYNC_OFFSET();
                    err = wasm__invoke_host_func(callee_mod, callee->host_func,
                                                 call_args, cft->num_params,
                                                 call_results, cft->num_results,
                                                 callee->host_userdata);
                    if (call_args != tail_args_buf) WASM_FREE(call_args);
                    if (err != WASM_OK) {
                        if (call_results != call_results_buf) WASM_FREE(call_results);
                        if (err == WASM__ERR_EXCEPTION) {
                            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 0);
                            if (err == WASM__ERR_RESUME_FRAME || err == WASM__ERR_EXCEPTION) WASM__LEAVE(err);
                        }
                        goto cleanup;
                    }
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, call_results[i]);
                    if (call_results != call_results_buf) WASM_FREE(call_results);
                    break;
                }

                if (next_module) *next_module = callee_mod;
                *next_func_idx = ci;
                *next_args = call_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (call_args != tail_args_buf);
                WASM__LEAVE(WASM__ERR_CALL_FRAME);
            }
            case 0x15: { /* return_call_ref */
                uint32_t ti = wasm__imm_u32(&cf->r), ci;
                wasm_module_t* callee_mod;
                wasm_functype_t* cft;
                wasm_func_t* callee;
                wasm_value_t funcref_value;
                wasm_value_t* tail_args = tail_args_buf;
                wasm_value_t tail_results_buf[8];
                wasm_value_t* tail_results = tail_results_buf;
                int j;

                if (ti >= mod->num_types) WASM__TRAP(WASM_ERR_MALFORMED);
                cft = wasm__module_functype(mod, ti);
                if (!cft) WASM__TRAP(WASM_ERR_MALFORMED);

                funcref_value = WASM__POP(rt);
                if (wasm__is_null_ref(&funcref_value)) {
                    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null function reference");
                    err = WASM_ERR_TRAP;
                    goto cleanup;
                }
                if (!wasm__resolve_func_ref(rt, funcref_value.of.funcref, &callee_mod, &ci))
                    WASM__TRAP(WASM_ERR_MALFORMED);
                if (!callee_mod || ci >= callee_mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                callee = &callee_mod->funcs[ci];
                if ((callee_mod != mod || callee->type_idx != ti) &&
                    !wasm__functype_is_subtype_across_modules(callee_mod, callee->type_idx, mod, ti))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);

                if (cft->num_params > 8) {
                    tail_args = (wasm_value_t*)wasm__malloc_array(cft->num_params, sizeof(wasm_value_t));
                    if (!tail_args) WASM__TRAP(WASM_ERR_OOM);
                }
                for (j = (int)cft->num_params - 1; j >= 0; j--) tail_args[j] = WASM__POP(rt);

                if (callee->is_import) {
                    if (cft->num_results > 8) {
                        tail_results = (wasm_value_t*)wasm__malloc_array(cft->num_results, sizeof(wasm_value_t));
                        if (!tail_results) {
                            if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                            WASM__TRAP(WASM_ERR_OOM);
                        }
                    }
                    if (cft->num_results > 0)
                        memset(tail_results, 0, cft->num_results * sizeof(wasm_value_t));
                    WASM__SYNC_OFFSET();
                    err = wasm__invoke_host_func(callee_mod, callee->host_func,
                                                 tail_args, cft->num_params,
                                                 tail_results, cft->num_results,
                                                 callee->host_userdata);
                    if (tail_args != tail_args_buf) WASM_FREE(tail_args);
                    if (err != WASM_OK) {
                        if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                        if (err == WASM__ERR_EXCEPTION) {
                            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 1);
                            if (err == WASM__ERR_RESUME_FRAME || err == WASM__ERR_EXCEPTION) WASM__LEAVE(err);
                        }
                        goto cleanup;
                    }

                    rt->sp = sp_base;
                    for (i = 0; i < cft->num_results; i++) WASM__PUSH(rt, tail_results[i]);
                    if (tail_results != tail_results_buf) WASM_FREE(tail_results);
                    err = WASM__ERR_RETURN_FRAME;
                    goto cleanup_frame;
                }

                rt->sp = sp_base;
                if (next_module) *next_module = callee_mod;
                *next_func_idx = ci;
                *next_args = tail_args;
                *next_num_args = cft->num_params;
                *next_args_owned = (tail_args != tail_args_buf);
                err = WASM__ERR_TAIL_CALL_FRAME;
                goto cleanup_frame;
            }
            case 0x1F: { /* try_table */
                wasm__blocktype_t blocktype;
                const wasm__control_target_t* target;
                const uint8_t* be;
                const uint8_t* immediate_start = cf->r.ptr;

                err = wasm__read_blocktype(mod, &cf->r, &blocktype);
                if (err != WASM_OK) goto cleanup;
                {
                    uint32_t clause_count = wasm__imm_u32(&cf->r);
                    uint32_t clause_index;

                    for (clause_index = 0; clause_index < clause_count; clause_index++) {
                        wasm__try_table_clause_t clause;

                        err = wasm__read_try_table_clause(&cf->r, &clause);
                        if (err != WASM_OK) goto cleanup;
                    }
                }

                target = wasm__control_target_at(func, op_at);
                if (!target) WASM__TRAP(WASM_ERR_MALFORMED);
                be = func->code + target->end_offset;
                if (rt->sp < blocktype.param_arity) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (cf->lsp >= cf->max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                cf->labels[cf->lsp].continuation = be;
                cf->labels[cf->lsp].handler_ptr = immediate_start;
                cf->labels[cf->lsp].sp_base = rt->sp - blocktype.param_arity;
                cf->labels[cf->lsp].param_arity = blocktype.param_arity;
                cf->labels[cf->lsp].arity = blocktype.result_arity;
                cf->labels[cf->lsp].caught_tag = UINT32_MAX;
                cf->labels[cf->lsp].caught_count = 0;
                cf->labels[cf->lsp].caught_values = NULL;
                cf->labels[cf->lsp].delegate_depth = UINT32_MAX;
                cf->labels[cf->lsp].opcode = 0x1F;
                cf->labels[cf->lsp].active_catch = 0;
                cf->lsp++;
                break;
            }

            case 0xFB:
                err = wasm__exec_gc(mod, cf);
                if (err != WASM_OK) goto cleanup;
                break;

            case 0xFC:
                err = wasm__exec_bulk(mod, cf);
                if (err != WASM_OK) goto cleanup;
                break;

            case 0xFD:
                err = wasm__exec_simd(mod, cf);
                if (err != WASM_OK) goto cleanup;
                break;

            case 0xFE:
                err = wasm__exec_atomic(mod, cf);
                if (err != WASM_OK) goto cleanup;
                break;

            /* Parametric */
            case 0x1A: {
                wasm_value_t tmp = WASM__POP(rt);
                (void)tmp;
                break;
            }
            case 0x1B: {
                wasm_value_t c = WASM__POP(rt), v2 = WASM__POP(rt), v1 = WASM__POP(rt);
                WASM__PUSH(rt, c.of.i32 ? v1 : v2);
                break;
            }
            case 0x1C: {
                wasm_valtype_t select_type;
                wasm_value_t c, v2, v1;

                err = wasm__read_typed_select_immediate(mod, &cf->r, &select_type, NULL);
                if (err != WASM_OK) goto cleanup;
                c = WASM__POP(rt);
                v2 = WASM__POP(rt);
                v1 = WASM__POP(rt);
                if (!wasm__value_matches_type(&v1, select_type) || !wasm__value_matches_type(&v2, select_type))
                    WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                WASM__PUSH(rt, c.of.i32 ? v1 : v2);
                break;
            }

            /* Variables */
            case 0x20: {
                uint32_t x = wasm__imm_u32(&cf->r);
                if (wasm__is_numeric_valtype(func->locals[x])) {
                    WASM__PUSH(rt, cf->locals[x]);
                    break;
                }
                WASM__PUSH(rt, wasm__retag_value_for_declared_type(
                                   func->locals[x],
                                   func->local_reftypes ? &func->local_reftypes[x] : NULL,
                                   cf->locals[x]));
                break;
            }
            case 0x21: {
                uint32_t x = wasm__imm_u32(&cf->r);
                if (wasm__is_numeric_valtype(func->locals[x])) {
                    cf->locals[x] = WASM__POP(rt);
                    break;
                }
                cf->locals[x] = wasm__retag_value_for_declared_type(
                    func->locals[x],
                    func->local_reftypes ? &func->local_reftypes[x] : NULL,
                    WASM__POP(rt));
                break;
            }
            case 0x22: {
                uint32_t x = wasm__imm_u32(&cf->r);
                if (wasm__is_numeric_valtype(func->locals[x])) {
                    cf->locals[x] = WASM__PEEK(rt);
                    break;
                }
                cf->locals[x] = wasm__retag_value_for_declared_type(
                    func->locals[x],
                    func->local_reftypes ? &func->local_reftypes[x] : NULL,
                    WASM__PEEK(rt));
                break;
            }
            case 0x23: {
                uint32_t x = wasm__imm_u32(&cf->r);
                WASM__PUSH(rt, wasm__global_get_value(&mod->globals[x]));
                break;
            }
            case 0x24: {
                uint32_t x = wasm__imm_u32(&cf->r);
                wasm_value_t value;

                if (!mod->globals[x].is_mutable) {
                    WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "global %u is immutable", (unsigned)x);
                    err = WASM_ERR_TYPE_MISMATCH;
                    goto cleanup;
                }
                value = WASM__POP(rt);
                if (!wasm__runtime_value_matches_type(mod,
                                                      &value,
                                                      mod->globals[x].type,
                                                      &mod->globals[x].ref_type))
                    WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                wasm__global_set_value(&mod->globals[x], value);
                break;
            }
            case 0x25: {
                uint32_t table_idx = wasm__imm_u32(&cf->r);
                wasm_table_t* table;
                wasm_value_t index;
                uint64_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                index = WASM__POP(rt);
                element_index = wasm__table_index_value(table, &index);
                if (element_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__PUSH(rt, table->elems[element_index]);
                break;
            }
            case 0x26: {
                uint32_t table_idx = wasm__imm_u32(&cf->r);
                wasm_table_t* table;
                wasm_value_t value;
                wasm_value_t index;
                uint64_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                value = WASM__POP(rt);
                index = WASM__POP(rt);
                element_index = wasm__table_index_value(table, &index);
                if (!wasm__runtime_value_matches_type(mod,
                                                      &value,
                                                      table->reftype,
                                                      &table->reftype_info))
                    WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (element_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                table->elems[element_index] = value;
                break;
            }

            /* Memory load */
            case 0x28:
            case 0x29:
            case 0x2A:
            case 0x2B:
            case 0x2C:
            case 0x2D:
            case 0x2E:
            case 0x2F:
            case 0x30:
            case 0x31:
            case 0x32:
            case 0x33:
            case 0x34:
            case 0x35: {
                wasm__memarg_t memarg;
                wasm_value_t base;
                uint64_t addr, msz;
                wasm_memory_t* memory;

                err = wasm__imm_memarg(&cf->r, &memarg);
                if (err != WASM_OK) goto cleanup;
                base = WASM__POP(rt);
                memory = wasm__memory_at(mod, memarg.memory_index);
                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                if (!wasm__memory_effective_addr(memory, &base, memarg.offset, &addr)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                msz = wasm__memory_byte_size(memory);
                switch (op) {
                    case 0x28: {
                        int32_t v;
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int32_t)wasm__load_le32(memory->data + addr);
                        WASM__PUSH(rt, wasm_i32(v));
                        break;
                    }
                    case 0x29: {
                        int64_t v;
                        if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int64_t)wasm__load_le64(memory->data + addr);
                        WASM__PUSH(rt, wasm_i64(v));
                        break;
                    }
                    case 0x2A: {
                        uint32_t b;
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        b = wasm__load_le32(memory->data + addr);
                        WASM__PUSH(rt, wasm__f32_bits(b));
                        break;
                    }
                    case 0x2B: {
                        uint64_t b;
                        if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        b = wasm__load_le64(memory->data + addr);
                        WASM__PUSH(rt, wasm__f64_bits(b));
                        break;
                    }
                    case 0x2C:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__PUSH(rt, wasm_i32((int32_t)(int8_t)memory->data[addr]));
                        break;
                    case 0x2D:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__PUSH(rt, wasm_i32((int32_t)memory->data[addr]));
                        break;
                    case 0x2E: {
                        int16_t v;
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int16_t)wasm__load_le16(memory->data + addr);
                        WASM__PUSH(rt, wasm_i32((int32_t)v));
                        break;
                    }
                    case 0x2F: {
                        uint16_t v;
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = wasm__load_le16(memory->data + addr);
                        WASM__PUSH(rt, wasm_i32((int32_t)v));
                        break;
                    }
                    case 0x30:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__PUSH(rt, wasm_i64((int64_t)(int8_t)memory->data[addr]));
                        break;
                    case 0x31:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__PUSH(rt, wasm_i64((int64_t)memory->data[addr]));
                        break;
                    case 0x32: {
                        int16_t v;
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int16_t)wasm__load_le16(memory->data + addr);
                        WASM__PUSH(rt, wasm_i64((int64_t)v));
                        break;
                    }
                    case 0x33: {
                        uint16_t v;
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = wasm__load_le16(memory->data + addr);
                        WASM__PUSH(rt, wasm_i64((int64_t)v));
                        break;
                    }
                    case 0x34: {
                        int32_t v;
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int32_t)wasm__load_le32(memory->data + addr);
                        WASM__PUSH(rt, wasm_i64((int64_t)v));
                        break;
                    }
                    case 0x35: {
                        uint32_t v;
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = wasm__load_le32(memory->data + addr);
                        WASM__PUSH(rt, wasm_i64((int64_t)v));
                        break;
                    }
                    default:
                        break;
                }
                break;
            }

            /* Memory store */
            case 0x36:
            case 0x37:
            case 0x38:
            case 0x39:
            case 0x3A:
            case 0x3B:
            case 0x3C:
            case 0x3D:
            case 0x3E: {
                wasm__memarg_t memarg;
                wasm_value_t val, base;
                uint64_t addr, msz;
                wasm_memory_t* memory;

                err = wasm__imm_memarg(&cf->r, &memarg);
                if (err != WASM_OK) goto cleanup;
                val = WASM__POP(rt);
                base = WASM__POP(rt);
                memory = wasm__memory_at(mod, memarg.memory_index);
                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                if (!wasm__memory_effective_addr(memory, &base, memarg.offset, &addr)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                msz = wasm__memory_byte_size(memory);
                switch (op) {
                    case 0x36:
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le32(memory->data + addr, (uint32_t)val.of.i32);
                        break;
                    case 0x37:
                        if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le64(memory->data + addr, (uint64_t)val.of.i64);
                        break;
                    case 0x38: {
                        uint32_t b;
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        memcpy(&b, &val.of.f32, 4);
                        wasm__store_le32(memory->data + addr, b);
                        break;
                    }
                    case 0x39: {
                        uint64_t b;
                        if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        memcpy(&b, &val.of.f64, 8);
                        wasm__store_le64(memory->data + addr, b);
                        break;
                    }
                    case 0x3A:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        memory->data[addr] = (uint8_t)val.of.i32;
                        break;
                    case 0x3B:
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le16(memory->data + addr, (uint16_t)val.of.i32);
                        break;
                    case 0x3C:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        memory->data[addr] = (uint8_t)val.of.i64;
                        break;
                    case 0x3D:
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le16(memory->data + addr, (uint16_t)val.of.i64);
                        break;
                    case 0x3E:
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        wasm__store_le32(memory->data + addr, (uint32_t)val.of.i64);
                        break;
                    default:
                        break;
                }
                break;
            }

            case 0x3F: {
                uint32_t mem_idx = (mod->required_features & WASM_FEATURE_MULTI_MEMORY)
                                       ? wasm__imm_u32(&cf->r)
                                       : (uint32_t)wasm__read_u8(&cf->r);
                wasm_memory_t* memory = wasm__memory_at(mod, mem_idx);

                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                WASM__PUSH(rt, memory->is_64 ? wasm_i64((int64_t)wasm__memory_pages(memory))
                                             : wasm_i32((int32_t)wasm__memory_pages(memory)));
                break;
            }
            case 0x40: {
                wasm_value_t d;
                int64_t res;
                uint32_t mem_idx = (mod->required_features & WASM_FEATURE_MULTI_MEMORY)
                                       ? wasm__imm_u32(&cf->r)
                                       : (uint32_t)wasm__read_u8(&cf->r);
                wasm_memory_t* memory;
                d = WASM__POP(rt);
                memory = wasm__memory_at(mod, mem_idx);
                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                res = wasm_memory_grow_at(mod,
                                          mem_idx,
                                          memory->is_64 ? (uint64_t)d.of.i64
                                                        : (uint64_t)(uint32_t)d.of.i32);
                WASM__PUSH(rt, memory->is_64 ? wasm_i64(res) : wasm_i32((int32_t)res));
                break;
            }

            /* Reference instructions */
            case 0xD0: {
                wasm_valtype_t type;
                wasm_reftype_t reftype = { 0 };
                err = wasm__read_heaptype(mod, &cf->r, &reftype);
                if (err != WASM_OK) goto cleanup;
                type = reftype.type ? reftype.type : WASM_TYPE_ANYREF;
                WASM__PUSH(rt, wasm__typed_ref_null(type, &reftype));
                break;
            }
            case 0xD1: {
                wasm_value_t value = WASM__POP(rt);
                WASM__PUSH(rt, wasm_i32(wasm__is_null_ref(&value) ? 1 : 0));
                break;
            }
            case 0xD2: {
                uint32_t func_index = wasm__imm_u32(&cf->r);
                wasm_value_t funcref;
                if (func_index >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__make_funcref(mod, func_index, &funcref);
                if (err != WASM_OK) goto cleanup;
                WASM__PUSH(rt, funcref);
                break;
            }
            case 0xD3: {
                wasm_value_t rhs = WASM__POP(rt);
                wasm_value_t lhs = WASM__POP(rt);
                int equal;

                if (!wasm__is_ref_type(lhs.type) || !wasm__is_ref_type(rhs.type)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (wasm__uses_funcref_storage(lhs.type) || wasm__uses_funcref_storage(rhs.type))
                    equal = wasm__uses_funcref_storage(lhs.type) && wasm__uses_funcref_storage(rhs.type) &&
                            lhs.of.funcref == rhs.of.funcref;
                else if (wasm__uses_externref_storage(lhs.type) || wasm__uses_externref_storage(rhs.type))
                    equal = wasm__uses_externref_storage(lhs.type) && wasm__uses_externref_storage(rhs.type) &&
                            lhs.of.externref == rhs.of.externref;
                else
                    equal = lhs.of.gc_ref == rhs.of.gc_ref;
                WASM__PUSH(rt, wasm_i32(equal ? 1 : 0));
                break;
            }
            case 0xD4: {
                wasm_reftype_t actual_ref;
                wasm_value_t value = WASM__POP(rt);

                if (!wasm__runtime_value_reftype(mod, &value, &actual_ref)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (wasm__is_null_ref(&value)) {
                    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null reference");
                    err = WASM_ERR_TRAP;
                    goto cleanup;
                }
                actual_ref.nullable = 0;
                WASM__PUSH(rt, wasm__runtime_cast_success_value(&actual_ref, value));
                break;
            }
            case 0xD5:
            case 0xD6: {
                uint32_t depth = wasm__imm_u32(&cf->r);
                wasm_reftype_t source_ref;
                wasm_reftype_t target_ref;
                wasm_value_t value;
                int matches;

                if (depth >= cf->lsp) WASM__TRAP(WASM_ERR_MALFORMED);
                value = WASM__POP(rt);
                if (!wasm__runtime_value_reftype(mod, &value, &source_ref)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                source_ref.nullable = 1;
                target_ref = source_ref;
                target_ref.nullable = 0;
                matches = !wasm__is_null_ref(&value);
                if (matches) {
                    value = wasm__runtime_cast_success_value(&target_ref, value);
                    if (op == 0xD5) {
                        WASM__PUSH(rt, value);
                    } else {
                        WASM__PUSH(rt, value);
                        err = wasm__do_branch(rt,
                                              cf->labels,
                                              &cf->labels[cf->lsp - 1 - depth],
                                              &cf->r,
                                              &cf->lsp,
                                              depth);
                        if (err != WASM_OK) goto cleanup;
                    }
                } else if (op == 0xD5) {
                    if (op == 0xD5) {
                        err = wasm__do_branch(rt,
                                              cf->labels,
                                              &cf->labels[cf->lsp - 1 - depth],
                                              &cf->r,
                                              &cf->lsp,
                                              depth);
                        if (err != WASM_OK) goto cleanup;
                    }
                }
                break;
            }

            /* Constants */
            case 0x41:
                WASM__PUSH(rt, wasm_i32(wasm__imm_i32(&cf->r)));
                break;
            case 0x42:
                WASM__PUSH(rt, wasm_i64(wasm__read_leb128_i64(&cf->r)));
                break;
            case 0x43:
                WASM__PUSH(rt, wasm__read_f32(&cf->r));
                break;
            case 0x44:
                WASM__PUSH(rt, wasm__read_f64(&cf->r));
                break;

            default:
                if (op >= 0x45 && op <= 0xC4) {
                    err = wasm__exec_numeric(rt, op);
                    if (err != WASM_OK) goto cleanup;
                    break;
                }
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown opcode 0x%02X", op);
                err = WASM_ERR_MALFORMED;
                goto cleanup_frame;
        }

        if (cf->r.malformed) WASM__TRAP(WASM_ERR_MALFORMED);
    }

    err = WASM__ERR_RETURN_FRAME;

cleanup:
cleanup_frame:
    WASM__SYNC_OFFSET();
    while (cf->lsp > 0) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }

    return err;
}

#undef WASM__LEAVE
#undef WASM__SYNC_OFFSET

WASM_PRIVATE wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results) {
    wasm_runtime_t* rt = mod->rt;
    wasm_func_t* func;
    uint32_t base_frame_sp = rt->call_frame_sp;
    wasm_value_t request_args_buf[8];
    wasm_module_t* next_module = NULL;
    wasm_value_t* next_args = NULL;
    uint32_t next_func_idx = 0;
    uint32_t next_num_args = 0;
    int next_args_owned = 0;
    wasm_error_t err = WASM_OK;

    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import)
        return wasm__invoke_host_func(mod, func->host_func,
                                      args, num_args, results, num_results,
                                      func->host_userdata);

    err = wasm__push_call_frame(mod, func_idx, args, num_args, results, num_results);
    if (err != WASM_OK) goto cleanup;

    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];
        wasm_module_t* frame_mod = cf->module ? cf->module : mod;

        next_module = NULL;
        next_args = NULL;
        next_num_args = 0;
        next_args_owned = 0;
        next_func_idx = 0;

        err = wasm__interp_loop(frame_mod, cf,
                                &next_module,
                                &next_func_idx, &next_args, &next_num_args,
                                &next_args_owned, request_args_buf, base_frame_sp);

        if (err == WASM__ERR_CALL_FRAME) {
            err = wasm__push_call_frame(next_module ? next_module : frame_mod,
                                        next_func_idx, next_args, next_num_args,
                                        NULL, 0);
            if (next_args_owned && next_args) {
                WASM_FREE(next_args);
                next_args = NULL;
                next_args_owned = 0;
            }
            if (err != WASM_OK) goto cleanup;
            continue;
        }

        if (err == WASM__ERR_TAIL_CALL_FRAME) {
            wasm_value_t* next_results_dst = cf->results_dst;
            uint32_t next_result_count = cf->num_results;

            wasm__arena_reset(rt, cf->arena_saved);
            memset(cf, 0, sizeof(*cf));
            rt->call_frame_sp--;

            err = wasm__push_call_frame(next_module ? next_module : frame_mod,
                                        next_func_idx, next_args, next_num_args,
                                        next_results_dst, next_result_count);
            if (next_args_owned && next_args) {
                WASM_FREE(next_args);
                next_args = NULL;
                next_args_owned = 0;
            }
            if (err != WASM_OK) goto cleanup;
            continue;
        }

        if (next_args_owned && next_args) {
            WASM_FREE(next_args);
            next_args = NULL;
            next_args_owned = 0;
        }

        if (err == WASM__ERR_RETURN_FRAME) {
            err = wasm__finish_call_frame(rt, base_frame_sp);
            if (err != WASM_OK) goto cleanup;
            continue;
        }

        if (err == WASM__ERR_RESUME_FRAME) continue;

        if (err == WASM__ERR_EXCEPTION) goto cleanup;

        if (err != WASM_OK) goto cleanup;
    }

cleanup:
    if (err != WASM_OK && rt->call_frame_sp > base_frame_sp)
        wasm__capture_backtrace(rt, base_frame_sp);
    if (next_args_owned && next_args) WASM_FREE(next_args);
    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];
        wasm__clear_call_frame(cf);
        wasm__arena_reset(rt, cf->arena_saved);
        memset(cf, 0, sizeof(*cf));
        rt->call_frame_sp--;
    }
    return err;
}

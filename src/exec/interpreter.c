#include "interpreter.h"
#include <stdio.h>
#include "bulk.h"
#include "calls.h"
#include "control.h"
#include "exceptions.h"
#include "frames.h"
#include "gc.h"
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

/* Code runs only after validation has checked its immediates, so these decoders skip bounds checks. */
WASM_INLINE uint32_t wasm__fetch_u32(const uint8_t** pc) {
    const uint8_t* p = *pc;
    uint32_t result = *p++;

    if (result & 0x80u) {
        uint32_t shift = 7;
        uint32_t byte;

        result &= 0x7Fu;
        do {
            byte = *p++;
            result |= (byte & 0x7Fu) << shift;
            shift += 7;
        } while ((byte & 0x80u) && shift < 35);
    }
    *pc = p;
    return result;
}

WASM_INLINE uint64_t wasm__fetch_u64(const uint8_t** pc) {
    const uint8_t* p = *pc;
    uint64_t result = 0;
    uint32_t shift = 0;
    uint32_t byte;

    do {
        byte = *p++;
        result |= (uint64_t)(byte & 0x7Fu) << shift;
        shift += 7;
    } while ((byte & 0x80u) && shift < 70);
    *pc = p;
    return result;
}

WASM_INLINE int32_t wasm__fetch_i32(const uint8_t** pc) {
    const uint8_t* p = *pc;
    uint32_t result = 0;
    uint32_t shift = 0;
    uint32_t byte;

    do {
        byte = *p++;
        result |= (byte & 0x7Fu) << shift;
        shift += 7;
    } while ((byte & 0x80u) && shift < 35);
    if (shift < 32 && (byte & 0x40u)) result |= ~0u << shift;
    *pc = p;
    return (int32_t)result;
}

WASM_INLINE int64_t wasm__fetch_i64(const uint8_t** pc) {
    const uint8_t* p = *pc;
    uint64_t result = 0;
    uint32_t shift = 0;
    uint32_t byte;

    do {
        byte = *p++;
        result |= (uint64_t)(byte & 0x7Fu) << shift;
        shift += 7;
    } while ((byte & 0x80u) && shift < 70);
    if (shift < 64 && (byte & 0x40u)) result |= ~(uint64_t)0 << shift;
    *pc = p;
    return (int64_t)result;
}

WASM_INLINE uint64_t wasm__fetch_memarg(const uint8_t** pc, uint32_t* memory_index) {
    uint32_t flags = wasm__fetch_u32(pc);

    *memory_index = (flags & 0x40u) ? wasm__fetch_u32(pc) : 0u;
    return wasm__fetch_u64(pc);
}

/* Block types are almost always empty or one value type; the rest need the module's types. */
static wasm_error_t wasm__fetch_block_arity(wasm_module_t* mod,
                                            const wasm_func_t* func,
                                            const uint8_t** pc,
                                            uint32_t* param_arity,
                                            uint32_t* result_arity) {
    wasm__reader_t r;
    wasm__blocktype_t blocktype;
    wasm_error_t err;
    uint8_t lead = **pc;

    if (lead == 0x40) {
        (*pc)++;
        *param_arity = 0;
        *result_arity = 0;
        return WASM_OK;
    }
    if (lead >= 0x7B && lead <= 0x7F) {
        (*pc)++;
        *param_arity = 0;
        *result_arity = 1;
        return WASM_OK;
    }
    r.ptr = *pc;
    r.end = func->code + func->code_len;
    r.malformed = 0;
    err = wasm__read_blocktype(mod, &r, &blocktype);
    *pc = r.ptr;
    *param_arity = blocktype.param_arity;
    *result_arity = blocktype.result_arity;
    return err;
}

/* Frees what an exception handler left on popped labels; other labels hold nothing to release. */
WASM_INLINE void wasm__drop_labels(wasm__label_t* labels, uint32_t lsp, uint32_t count) {
    while (count-- > 0) {
        if (labels[lsp - 1].caught_values || labels[lsp - 1].active_catch) wasm__clear_label(&labels[lsp - 1]);
        lsp--;
    }
}

/* Polls the interrupt flag with a plain read and confirms it with an atomic one before acting. */
WASM_INLINE int wasm__interrupt_pending(wasm_runtime_t* rt) {
#if defined(__GNUC__) || defined(__clang__)
    if (__atomic_load_n(&rt->interrupt_requested, __ATOMIC_RELAXED) == 0u) return 0;
#else
    if (*(const volatile size_t*)&rt->interrupt_requested == 0u) return 0;
#endif
    return wasm__runtime_take_interrupt(rt);
}

/* Calls a host import of type ft, whose type indices belong to mod. Results start as typed
 * defaults, so a host may set only the payload, and must match ft when it returns. */
static wasm_error_t wasm__invoke_host_func(wasm_module_t* mod,
                                           const wasm_functype_t* ft,
                                           wasm_host_func_t host_func,
                                           const wasm_value_t* args,
                                           wasm_value_t* results,
                                           void* userdata) {
    wasm_error_t err;
    uint32_t i;

    if (!mod || !mod->rt || !ft || !host_func) return WASM_ERR_MALFORMED;
    for (i = 0; i < ft->num_results; i++) {
        const wasm_reftype_t* reftype = ft->result_reftypes ? &ft->result_reftypes[i] : NULL;

        results[i] = wasm__is_ref_type(ft->results[i]) ? wasm__typed_ref_null(ft->results[i], reftype)
                                                       : wasm__default_value(ft->results[i]);
    }
    err = host_func(mod, args, ft->num_params, results, ft->num_results, userdata);
    if (err != WASM_OK) return err;
    for (i = 0; i < ft->num_results; i++) {
        const wasm_reftype_t* reftype = ft->result_reftypes ? &ft->result_reftypes[i] : NULL;

        if (!wasm__runtime_value_matches_type(mod, &results[i], ft->results[i], reftype)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "host function result %u does not match its declared type", (unsigned)i);
            return WASM_ERR_TYPE_MISMATCH;
        }
    }
    return WASM_OK;
}

/* Calls host import func with the ft->num_params arguments at args, which sit just above rt->sp
   where a reentrant call could overwrite them, and pushes its results. */
static wasm_error_t wasm__call_host_from_stack(wasm_module_t* mod,
                                               const wasm_functype_t* ft,
                                               const wasm_func_t* func,
                                               const wasm_value_t* args) {
    wasm_runtime_t* rt = mod->rt;
    wasm_value_t args_buf[8];
    wasm_value_t results_buf[8];
    wasm_value_t* call_args = args_buf;
    wasm_value_t* call_results = results_buf;
    wasm_error_t err = WASM_ERR_OOM;

    if (ft->num_params > 8) call_args = (wasm_value_t*)wasm__malloc_array(ft->num_params, sizeof(wasm_value_t));
    if (ft->num_results > 8) call_results = (wasm_value_t*)wasm__malloc_array(ft->num_results, sizeof(wasm_value_t));
    if (call_args && call_results) {
        if (ft->num_params > 0) memcpy(call_args, args, ft->num_params * sizeof(wasm_value_t));
        err = wasm__invoke_host_func(mod, ft, func->host_func, call_args, call_results, func->host_userdata);
        if (err == WASM_OK && ft->num_results > rt->max_stack - rt->sp) err = WASM_ERR_STACK_OVERFLOW;
        if (err == WASM_OK && ft->num_results > 0) {
            memcpy(&rt->stack[rt->sp], call_results, ft->num_results * sizeof(wasm_value_t));
            rt->sp += ft->num_results;
        }
    }
    if (call_args != args_buf) WASM_FREE(call_args);
    if (call_results != results_buf) WASM_FREE(call_results);
    return err;
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

/* The loop keeps pc, sp, and the label count in locals. Before anything outside the loop can see
   the frame (calls, traps, allocation, backtraces) it stores them back with WASM__SAVE_STATE,
   including the offset of the current instruction, and reloads them with WASM__LOAD_STATE. */
#define WASM__SAVE_STATE()                                 \
    do {                                                   \
        cf->r.ptr = pc;                                    \
        rt->sp = (uint32_t)(sp - stack);                   \
        cf->lsp = lsp;                                     \
        cf->current_offset = (uint32_t)(op_at - func->code); \
    } while (0)
#define WASM__LOAD_STATE()                   \
    do {                                     \
        pc = cf->r.ptr;                      \
        sp = stack + rt->sp;                 \
        lsp = cf->lsp;                       \
    } while (0)
#define WASM__ENTER_FRAME()                                    \
    do {                                                       \
        cf = &rt->call_frames[rt->call_frame_sp - 1];          \
        mod = cf->module;                                      \
        func = cf->func;                                       \
        labels = cf->labels;                                   \
        locals = cf->locals;                                   \
        max_labels = cf->max_labels;                           \
        sp_base = cf->sp_base;                                 \
        op_at = func->code + cf->current_offset;               \
        WASM__LOAD_STATE();                                    \
    } while (0)
/* Runs an out-of-line handler that reads immediates through cf->r and uses rt->sp. */
#define WASM__CALL_OUT(expr)              \
    do {                                  \
        WASM__SAVE_STATE();               \
        err = (expr);                     \
        WASM__LOAD_STATE();               \
        if (err != WASM_OK) goto cleanup; \
    } while (0)
#define WASM__SP_PUSH(val)                 \
    do {                                   \
        if (sp == stack_end) {             \
            err = WASM_ERR_STACK_OVERFLOW; \
            goto cleanup;                  \
        }                                  \
        *sp++ = (val);                     \
    } while (0)
/* Pushes into a slot the same instruction has just popped, so it cannot overflow. */
#define WASM__SP_PUT(val) (*sp++ = (val))
#define WASM__SP_POP() (*--sp)
#define WASM__SP_TOP() (sp[-1])

#define WASM__PUSH_LABEL(cont, handler, params, results, opc) \
    do {                                                      \
        wasm__label_t* label_ = &labels[lsp++];               \
        label_->continuation = (cont);                        \
        label_->handler_ptr = (handler);                      \
        label_->sp_base = (uint32_t)(sp - stack) - (params);  \
        label_->param_arity = (params);                       \
        label_->arity = (results);                            \
        label_->caught_count = 0;                             \
        label_->caught_values = NULL;                         \
        label_->opcode = (opc);                               \
        label_->active_catch = 0;                             \
    } while (0)

/* Runs the frames above base_frame_sp until the lowest of them returns, which leaves its results
   on top of the stack and returns WASM__ERR_RETURN_FRAME. */
static wasm_error_t wasm__interp_loop(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    wasm_value_t* const stack = rt->stack;
    wasm_value_t* const stack_end = rt->stack + rt->max_stack;
    wasm__call_frame_t* cf;
    wasm_module_t* mod;
    wasm_func_t* func;
    wasm__label_t* labels;
    wasm_value_t* locals;
    wasm_value_t* sp;
    const uint8_t* pc;
    const uint8_t* op_at;
    wasm_module_t* call_mod;
    wasm_functype_t* call_type;
    uint32_t call_idx;
    uint32_t lsp;
    uint32_t max_labels;
    uint32_t branch_depth;
    uint32_t sp_base;
    uint32_t i;
    wasm_error_t err = WASM_OK;

    WASM__ENTER_FRAME();

    for (;;) {
        uint8_t op;

        if (rt->fuel_enabled) {
            if (rt->fuel == 0) WASM__TRAP(WASM_ERR_OUT_OF_FUEL);
            rt->fuel--;
        }
        op_at = pc;
        op = *pc++;
        switch (op) {
            case 0x00:
                WASM__TRAP(WASM_ERR_UNREACHABLE);
            case 0x01:
                break;

            case 0x02: { /* block */
                uint32_t params, results;

                err = wasm__fetch_block_arity(mod, func, &pc, &params, &results);
                if (err != WASM_OK) goto cleanup;
                if (lsp >= max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                WASM__PUSH_LABEL(func->code + func->control_targets[op_at - func->code].end_offset, NULL, params,
                                 results, 0x02);
                break;
            }
            case 0x03: { /* loop */
                uint32_t params, results;

                err = wasm__fetch_block_arity(mod, func, &pc, &params, &results);
                if (err != WASM_OK) goto cleanup;
                if (lsp >= max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                WASM__PUSH_LABEL(pc, NULL, params, results, 0x03);
                break;
            }
            case 0x04: { /* if */
                const wasm__control_target_t* target = &func->control_targets[op_at - func->code];
                uint32_t params, results;
                int32_t cond = WASM__SP_POP().of.i32;

                err = wasm__fetch_block_arity(mod, func, &pc, &params, &results);
                if (err != WASM_OK) goto cleanup;
                if (!cond) {
                    if (target->aux_offset == UINT32_MAX) {
                        pc = func->code + target->end_offset;
                        break;
                    }
                    pc = func->code + target->aux_offset;
                }
                if (lsp >= max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                WASM__PUSH_LABEL(func->code + target->end_offset, NULL, params, results, 0x04);
                break;
            }
            case 0x06: { /* try */
                const wasm__control_target_t* target = &func->control_targets[op_at - func->code];
                uint32_t params, results;

                err = wasm__fetch_block_arity(mod, func, &pc, &params, &results);
                if (err != WASM_OK) goto cleanup;
                if (lsp >= max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                WASM__PUSH_LABEL(func->code + target->end_offset,
                                 target->aux_offset == UINT32_MAX ? NULL : func->code + target->aux_offset,
                                 params, results, 0x06);
                break;
            }
            case 0x05: /* else */
                branch_depth = 0;
                goto do_branch;
            case 0x07:
            case 0x18:
            case 0x19:
                if (lsp > 1 && labels[lsp - 1].opcode == 0x06) {
                    branch_depth = 0;
                    goto do_branch;
                }
                WASM__TRAP(WASM_ERR_MALFORMED);
            case 0x08: { /* throw */
                uint32_t tag_index = wasm__fetch_u32(&pc);
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
                for (j = (int)tag_type->num_params - 1; j >= 0; j--) payload[j] = WASM__SP_POP();
                err = wasm__set_exception_state(&rt->pending_exception,
                                                tag_index,
                                                mod->tags[tag_index].identity,
                                                payload,
                                                tag_type->num_params);
                if (payload != payload_buf) WASM_FREE(payload);
                if (err != WASM_OK) goto cleanup;
                goto throw_pending;
            }
            case 0x09: { /* rethrow */
                uint32_t depth_index = wasm__fetch_u32(&pc);
                uint32_t target_index;

                if (!wasm__find_rethrow_label(labels, lsp, depth_index, &target_index)) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__set_exception_state(&rt->pending_exception,
                                                labels[target_index].caught_tag,
                                                labels[target_index].caught_tag_identity,
                                                labels[target_index].caught_values,
                                                labels[target_index].caught_count);
                if (err != WASM_OK) goto cleanup;
                while (lsp > target_index) {
                    wasm__clear_label(&labels[lsp - 1]);
                    sp = stack + labels[lsp - 1].sp_base;
                    lsp--;
                }
                goto throw_pending;
            }
            case 0x0A: { /* throw_ref */
                wasm_value_t exnref_value = WASM__SP_POP();
                wasm__exception_object_t* exn = wasm__exception_object_from_exnref(rt, &exnref_value);

                if (!exn) WASM__TRAP(WASM_ERR_TRAP);
                err = wasm__set_exception_state(&rt->pending_exception,
                                                exn->tag_index,
                                                exn->tag_identity,
                                                exn->values,
                                                exn->num_values);
                if (err != WASM_OK) goto cleanup;
                goto throw_pending;
            }
            case 0x0B: { /* end */
                wasm__label_t* label = &labels[lsp - 1];
                wasm_value_t* dst = stack + label->sp_base;

                if (lsp == 1) goto return_frame;
                if (label->arity > 0 && sp - label->arity != dst)
                    memmove(dst, sp - label->arity, label->arity * sizeof(wasm_value_t));
                sp = dst + label->arity;
                if (label->caught_values || label->active_catch) wasm__clear_label(label);
                lsp--;
                break;
            }

            case 0x0C: /* br */
                branch_depth = wasm__fetch_u32(&pc);
                goto do_branch;
            case 0x0D: /* br_if */
                branch_depth = wasm__fetch_u32(&pc);
                if (WASM__SP_POP().of.i32) goto do_branch;
                break;
            case 0x0E: { /* br_table */
                uint32_t count = wasm__fetch_u32(&pc), j;
                uint32_t selected = (uint32_t)WASM__SP_POP().of.i32;

                if (selected > count) selected = count;
                for (j = 0; j < selected; j++)
                    while (*pc++ & 0x80u) {
                    }
                branch_depth = wasm__fetch_u32(&pc);
                goto do_branch;
            }
            case 0x0F: /* return */
                goto return_frame;

            case 0x10: /* call */
            case 0x12: /* return_call */
                call_idx = wasm__fetch_u32(&pc);
                call_mod = mod;
                call_type = wasm__module_functype(mod, mod->funcs[call_idx].type_idx);
                if (!call_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if (op == 0x12) goto do_tail_call;
                goto do_call;
            case 0x11:   /* call_indirect */
            case 0x13: { /* return_call_indirect */
                uint32_t type_idx = wasm__fetch_u32(&pc);
                uint32_t table_idx = wasm__fetch_u32(&pc);
                wasm_value_t index_value = WASM__SP_POP();
                wasm_table_t* table;
                uint64_t elem_index;

                if (type_idx >= mod->num_types || table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                elem_index = wasm__table_index_value(table, &index_value);
                if (table->reftype != WASM_TYPE_FUNCREF) WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (elem_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_UNDEFINED_ELEMENT);
                if (wasm__is_null_ref(&table->elems[elem_index])) WASM__TRAP(WASM_ERR_UNINITIALIZED_TABLE);
                if (!wasm__resolve_func_ref(rt, table->elems[elem_index].of.funcref, &call_mod, &call_idx))
                    WASM__TRAP(WASM_ERR_MALFORMED);
                if (!call_mod || call_idx >= call_mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                call_type = wasm__module_functype(mod, type_idx);
                if (!call_type) WASM__TRAP(WASM_ERR_MALFORMED);
                if ((call_mod != mod || call_mod->funcs[call_idx].type_idx != type_idx) &&
                    !wasm__functype_is_subtype_across_modules(call_mod, call_mod->funcs[call_idx].type_idx, mod,
                                                              type_idx))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (op == 0x13) goto do_tail_call;
                goto do_call;
            }
            case 0x14:   /* call_ref */
            case 0x15: { /* return_call_ref */
                uint32_t type_idx = wasm__fetch_u32(&pc);
                wasm_value_t funcref_value;

                if (type_idx >= mod->num_types) WASM__TRAP(WASM_ERR_MALFORMED);
                call_type = wasm__module_functype(mod, type_idx);
                if (!call_type) WASM__TRAP(WASM_ERR_MALFORMED);
                funcref_value = WASM__SP_POP();
                if (wasm__is_null_ref(&funcref_value)) {
                    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null function reference");
                    WASM__TRAP(WASM_ERR_TRAP);
                }
                if (!wasm__resolve_func_ref(rt, funcref_value.of.funcref, &call_mod, &call_idx))
                    WASM__TRAP(WASM_ERR_MALFORMED);
                if (!call_mod || call_idx >= call_mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                if ((call_mod != mod || call_mod->funcs[call_idx].type_idx != type_idx) &&
                    !wasm__functype_is_subtype_across_modules(call_mod, call_mod->funcs[call_idx].type_idx, mod,
                                                              type_idx))
                    WASM__TRAP(WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
                if (op == 0x15) goto do_tail_call;
                goto do_call;
            }
            case 0x1F: { /* try_table */
                const uint8_t* immediate_start = pc;
                uint32_t params, results, clause_count, clause_index;
                wasm__reader_t r;

                err = wasm__fetch_block_arity(mod, func, &pc, &params, &results);
                if (err != WASM_OK) goto cleanup;
                clause_count = wasm__fetch_u32(&pc);
                r.ptr = pc;
                r.end = func->code + func->code_len;
                r.malformed = 0;
                for (clause_index = 0; clause_index < clause_count; clause_index++) {
                    wasm__try_table_clause_t clause;

                    err = wasm__read_try_table_clause(&r, &clause);
                    if (err != WASM_OK) goto cleanup;
                }
                pc = r.ptr;
                if (lsp >= max_labels) WASM__TRAP(WASM_ERR_STACK_OVERFLOW);
                WASM__PUSH_LABEL(func->code + func->control_targets[op_at - func->code].end_offset, immediate_start,
                                 params, results, 0x1F);
                break;
            }

            case 0xFB:
                WASM__CALL_OUT(wasm__exec_gc(mod, cf));
                break;
            case 0xFC:
                WASM__CALL_OUT(wasm__exec_bulk(mod, cf));
                break;
            case 0xFD:
                WASM__CALL_OUT(wasm__exec_simd(mod, cf));
                break;
            case 0xFE:
                WASM__CALL_OUT(wasm__exec_atomic(mod, cf));
                break;

            /* Parametric */
            case 0x1A: {
                wasm_value_t tmp = WASM__SP_POP();
                (void)tmp;
                break;
            }
            case 0x1B: {
                wasm_value_t c = WASM__SP_POP(), v2 = WASM__SP_POP(), v1 = WASM__SP_POP();
                WASM__SP_PUT(c.of.i32 ? v1 : v2);
                break;
            }
            case 0x1C: {
                wasm_valtype_t select_type;
                wasm_value_t c, v2, v1;

                cf->r.ptr = pc;
                err = wasm__read_typed_select_immediate(mod, &cf->r, &select_type, NULL);
                pc = cf->r.ptr;
                if (err != WASM_OK) goto cleanup;
                c = WASM__SP_POP();
                v2 = WASM__SP_POP();
                v1 = WASM__SP_POP();
                if (!wasm__value_matches_type(&v1, select_type) || !wasm__value_matches_type(&v2, select_type))
                    WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                WASM__SP_PUT(c.of.i32 ? v1 : v2);
                break;
            }

            /* Variables */
            case 0x20: {
                uint32_t x = wasm__fetch_u32(&pc);
                if (wasm__is_numeric_valtype(func->locals[x])) {
                    WASM__SP_PUSH(locals[x]);
                    break;
                }
                WASM__SP_PUSH(wasm__retag_value_for_declared_type(
                                   func->locals[x],
                                   func->local_reftypes ? &func->local_reftypes[x] : NULL,
                                   locals[x]));
                break;
            }
            case 0x21: {
                uint32_t x = wasm__fetch_u32(&pc);
                if (wasm__is_numeric_valtype(func->locals[x])) {
                    locals[x] = WASM__SP_POP();
                    break;
                }
                locals[x] = wasm__retag_value_for_declared_type(
                    func->locals[x],
                    func->local_reftypes ? &func->local_reftypes[x] : NULL,
                    WASM__SP_POP());
                break;
            }
            case 0x22: {
                uint32_t x = wasm__fetch_u32(&pc);
                if (wasm__is_numeric_valtype(func->locals[x])) {
                    locals[x] = WASM__SP_TOP();
                    break;
                }
                locals[x] = wasm__retag_value_for_declared_type(
                    func->locals[x],
                    func->local_reftypes ? &func->local_reftypes[x] : NULL,
                    WASM__SP_TOP());
                break;
            }
            case 0x23: {
                uint32_t x = wasm__fetch_u32(&pc);
                WASM__SP_PUSH(wasm__global_get_value(&mod->globals[x]));
                break;
            }
            case 0x24: {
                uint32_t x = wasm__fetch_u32(&pc);
                wasm_value_t value;

                if (!mod->globals[x].is_mutable) {
                    WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "global %u is immutable", (unsigned)x);
                    err = WASM_ERR_TYPE_MISMATCH;
                    goto cleanup;
                }
                value = WASM__SP_POP();
                if (!wasm__runtime_value_matches_type(mod,
                                                      &value,
                                                      mod->globals[x].type,
                                                      &mod->globals[x].ref_type))
                    WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                wasm__global_set_value(&mod->globals[x], value);
                break;
            }
            case 0x25: {
                uint32_t table_idx = wasm__fetch_u32(&pc);
                wasm_table_t* table;
                wasm_value_t index;
                uint64_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                index = WASM__SP_POP();
                element_index = wasm__table_index_value(table, &index);
                if (element_index >= table->size || !table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__SP_PUSH(table->elems[element_index]);
                break;
            }
            case 0x26: {
                uint32_t table_idx = wasm__fetch_u32(&pc);
                wasm_table_t* table;
                wasm_value_t value;
                wasm_value_t index;
                uint64_t element_index;

                if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
                table = wasm__table_at(mod, table_idx);
                if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
                value = WASM__SP_POP();
                index = WASM__SP_POP();
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

                memarg.offset = wasm__fetch_memarg(&pc, &memarg.memory_index);
                base = WASM__SP_POP();
                memory = wasm__memory_at(mod, memarg.memory_index);
                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                if (!wasm__memory_effective_addr(memory, &base, memarg.offset, &addr)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                msz = wasm__memory_byte_size(memory);
                switch (op) {
                    case 0x28: {
                        int32_t v;
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int32_t)wasm__load_le32(memory->data + addr);
                        WASM__SP_PUT(wasm_i32(v));
                        break;
                    }
                    case 0x29: {
                        int64_t v;
                        if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int64_t)wasm__load_le64(memory->data + addr);
                        WASM__SP_PUT(wasm_i64(v));
                        break;
                    }
                    case 0x2A: {
                        uint32_t b;
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        b = wasm__load_le32(memory->data + addr);
                        WASM__SP_PUT(wasm__f32_bits(b));
                        break;
                    }
                    case 0x2B: {
                        uint64_t b;
                        if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        b = wasm__load_le64(memory->data + addr);
                        WASM__SP_PUT(wasm__f64_bits(b));
                        break;
                    }
                    case 0x2C:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__SP_PUT(wasm_i32((int32_t)(int8_t)memory->data[addr]));
                        break;
                    case 0x2D:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__SP_PUT(wasm_i32((int32_t)memory->data[addr]));
                        break;
                    case 0x2E: {
                        int16_t v;
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int16_t)wasm__load_le16(memory->data + addr);
                        WASM__SP_PUT(wasm_i32((int32_t)v));
                        break;
                    }
                    case 0x2F: {
                        uint16_t v;
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = wasm__load_le16(memory->data + addr);
                        WASM__SP_PUT(wasm_i32((int32_t)v));
                        break;
                    }
                    case 0x30:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__SP_PUT(wasm_i64((int64_t)(int8_t)memory->data[addr]));
                        break;
                    case 0x31:
                        if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        WASM__SP_PUT(wasm_i64((int64_t)memory->data[addr]));
                        break;
                    case 0x32: {
                        int16_t v;
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int16_t)wasm__load_le16(memory->data + addr);
                        WASM__SP_PUT(wasm_i64((int64_t)v));
                        break;
                    }
                    case 0x33: {
                        uint16_t v;
                        if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = wasm__load_le16(memory->data + addr);
                        WASM__SP_PUT(wasm_i64((int64_t)v));
                        break;
                    }
                    case 0x34: {
                        int32_t v;
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = (int32_t)wasm__load_le32(memory->data + addr);
                        WASM__SP_PUT(wasm_i64((int64_t)v));
                        break;
                    }
                    case 0x35: {
                        uint32_t v;
                        if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                        v = wasm__load_le32(memory->data + addr);
                        WASM__SP_PUT(wasm_i64((int64_t)v));
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

                memarg.offset = wasm__fetch_memarg(&pc, &memarg.memory_index);
                val = WASM__SP_POP();
                base = WASM__SP_POP();
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
                uint32_t mem_idx = wasm__fetch_u32(&pc);
                wasm_memory_t* memory = wasm__memory_at(mod, mem_idx);

                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                WASM__SP_PUSH(memory->is_64 ? wasm_i64((int64_t)wasm__memory_pages(memory))
                                             : wasm_i32((int32_t)wasm__memory_pages(memory)));
                break;
            }
            case 0x40: {
                wasm_value_t d;
                int64_t res;
                uint32_t mem_idx = wasm__fetch_u32(&pc);
                wasm_memory_t* memory;
                d = WASM__SP_POP();
                memory = wasm__memory_at(mod, mem_idx);
                if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
                res = wasm_memory_grow_at(mod,
                                          mem_idx,
                                          memory->is_64 ? (uint64_t)d.of.i64
                                                        : (uint64_t)(uint32_t)d.of.i32);
                if (res >= 0) {
                    uint64_t page_size = memory->page_size ? memory->page_size : WASM_PAGE_SIZE;
                    uint64_t pages = memory->is_64 ? (uint64_t)d.of.i64 : (uint64_t)(uint32_t)d.of.i32;

                    WASM__CHARGE_BYTES(rt, pages * page_size);
                }
                WASM__SP_PUSH(memory->is_64 ? wasm_i64(res) : wasm_i32((int32_t)res));
                break;
            }

            /* Reference instructions */
            case 0xD0: {
                wasm_valtype_t type;
                wasm_reftype_t reftype = { 0 };
                cf->r.ptr = pc;
                err = wasm__read_heaptype(mod, &cf->r, &reftype);
                pc = cf->r.ptr;
                if (err != WASM_OK) goto cleanup;
                type = reftype.type ? reftype.type : WASM_TYPE_ANYREF;
                WASM__SP_PUSH(wasm__typed_ref_null(type, &reftype));
                break;
            }
            case 0xD1: {
                wasm_value_t value = WASM__SP_POP();
                WASM__SP_PUSH(wasm_i32(wasm__is_null_ref(&value) ? 1 : 0));
                break;
            }
            case 0xD2: {
                uint32_t func_index = wasm__fetch_u32(&pc);
                wasm_value_t funcref;
                if (func_index >= mod->num_funcs) WASM__TRAP(WASM_ERR_MALFORMED);
                WASM__CALL_OUT(wasm__make_funcref(mod, func_index, &funcref));
                WASM__SP_PUSH(funcref);
                break;
            }
            case 0xD3: {
                wasm_value_t rhs = WASM__SP_POP();
                wasm_value_t lhs = WASM__SP_POP();
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
                WASM__SP_PUSH(wasm_i32(equal ? 1 : 0));
                break;
            }
            case 0xD4: {
                wasm_reftype_t actual_ref;
                wasm_value_t value = WASM__SP_POP();

                if (!wasm__runtime_value_reftype(mod, &value, &actual_ref)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                if (wasm__is_null_ref(&value)) {
                    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "null reference");
                    err = WASM_ERR_TRAP;
                    goto cleanup;
                }
                actual_ref.nullable = 0;
                WASM__SP_PUSH(wasm__runtime_cast_success_value(&actual_ref, value));
                break;
            }
            case 0xD5:
            case 0xD6: {
                wasm_reftype_t target_ref;
                wasm_value_t value;

                branch_depth = wasm__fetch_u32(&pc);
                value = WASM__SP_POP();
                if (!wasm__runtime_value_reftype(mod, &value, &target_ref)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                target_ref.nullable = 0;
                if (wasm__is_null_ref(&value)) {
                    if (op == 0xD5) goto do_branch;
                    break;
                }
                WASM__SP_PUT(wasm__runtime_cast_success_value(&target_ref, value));
                if (op == 0xD6) goto do_branch;
                break;
            }

            /* Constants */
            case 0x41:
                WASM__SP_PUSH(wasm_i32(wasm__fetch_i32(&pc)));
                break;
            case 0x42:
                WASM__SP_PUSH(wasm_i64(wasm__fetch_i64(&pc)));
                break;
            case 0x43: {
                uint32_t bits = wasm__load_le32(pc);

                pc += 4;
                WASM__SP_PUSH(wasm__f32_bits(bits));
                break;
            }
            case 0x44: {
                uint64_t bits = wasm__load_le64(pc);

                pc += 8;
                WASM__SP_PUSH(wasm__f64_bits(bits));
                break;
            }

            /* i32 comparison */
            case 0x45: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 == 0 ? 1 : 0));
                break;
            }
            case 0x46: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 == b.of.i32 ? 1 : 0));
                break;
            }
            case 0x47: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 != b.of.i32 ? 1 : 0));
                break;
            }
            case 0x48: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 < b.of.i32 ? 1 : 0));
                break;
            }
            case 0x49: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint32_t)a.of.i32 < (uint32_t)b.of.i32 ? 1 : 0));
                break;
            }
            case 0x4A: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 > b.of.i32 ? 1 : 0));
                break;
            }
            case 0x4B: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint32_t)a.of.i32 > (uint32_t)b.of.i32 ? 1 : 0));
                break;
            }
            case 0x4C: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 <= b.of.i32 ? 1 : 0));
                break;
            }
            case 0x4D: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint32_t)a.of.i32 <= (uint32_t)b.of.i32 ? 1 : 0));
                break;
            }
            case 0x4E: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 >= b.of.i32 ? 1 : 0));
                break;
            }
            case 0x4F: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint32_t)a.of.i32 >= (uint32_t)b.of.i32 ? 1 : 0));
                break;
            }

            /* i64 comparison */
            case 0x50: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 == 0 ? 1 : 0));
                break;
            }
            case 0x51: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 == b.of.i64 ? 1 : 0));
                break;
            }
            case 0x52: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 != b.of.i64 ? 1 : 0));
                break;
            }
            case 0x53: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 < b.of.i64 ? 1 : 0));
                break;
            }
            case 0x54: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint64_t)a.of.i64 < (uint64_t)b.of.i64 ? 1 : 0));
                break;
            }
            case 0x55: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 > b.of.i64 ? 1 : 0));
                break;
            }
            case 0x56: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint64_t)a.of.i64 > (uint64_t)b.of.i64 ? 1 : 0));
                break;
            }
            case 0x57: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 <= b.of.i64 ? 1 : 0));
                break;
            }
            case 0x58: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint64_t)a.of.i64 <= (uint64_t)b.of.i64 ? 1 : 0));
                break;
            }
            case 0x59: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i64 >= b.of.i64 ? 1 : 0));
                break;
            }
            case 0x5A: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((uint64_t)a.of.i64 >= (uint64_t)b.of.i64 ? 1 : 0));
                break;
            }

            /* f32/f64 comparison */
            case 0x5B: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 == b.of.f32 ? 1 : 0));
                break;
            }
            case 0x5C: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 != b.of.f32 ? 1 : 0));
                break;
            }
            case 0x5D: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 < b.of.f32 ? 1 : 0));
                break;
            }
            case 0x5E: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 > b.of.f32 ? 1 : 0));
                break;
            }
            case 0x5F: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 <= b.of.f32 ? 1 : 0));
                break;
            }
            case 0x60: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f32 >= b.of.f32 ? 1 : 0));
                break;
            }
            case 0x61: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 == b.of.f64 ? 1 : 0));
                break;
            }
            case 0x62: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 != b.of.f64 ? 1 : 0));
                break;
            }
            case 0x63: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 < b.of.f64 ? 1 : 0));
                break;
            }
            case 0x64: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 > b.of.f64 ? 1 : 0));
                break;
            }
            case 0x65: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 <= b.of.f64 ? 1 : 0));
                break;
            }
            case 0x66: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.f64 >= b.of.f64 ? 1 : 0));
                break;
            }

            /* i32 arithmetic */
            case 0x67: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)wasm__clz32((uint32_t)a.of.i32)));
                break;
            }
            case 0x68: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)wasm__ctz32((uint32_t)a.of.i32)));
                break;
            }
            case 0x69: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)wasm__popcnt32((uint32_t)a.of.i32)));
                break;
            }
            case 0x6A: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits((uint32_t)a.of.i32 + (uint32_t)b.of.i32));
                break;
            }
            case 0x6B: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits((uint32_t)a.of.i32 - (uint32_t)b.of.i32));
                break;
            }
            case 0x6C: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits((uint32_t)a.of.i32 * (uint32_t)b.of.i32));
                break;
            }
            case 0x6D: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                if (a.of.i32 == INT32_MIN && b.of.i32 == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
                WASM__SP_PUT(wasm_i32(a.of.i32 / b.of.i32));
                break;
            }
            case 0x6E: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i32((int32_t)((uint32_t)a.of.i32 / (uint32_t)b.of.i32)));
                break;
            }
            case 0x6F: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i32((a.of.i32 == INT32_MIN && b.of.i32 == -1) ? 0 : a.of.i32 % b.of.i32));
                break;
            }
            case 0x70: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i32((int32_t)((uint32_t)a.of.i32 % (uint32_t)b.of.i32)));
                break;
            }
            case 0x71: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 & b.of.i32));
                break;
            }
            case 0x72: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 | b.of.i32));
                break;
            }
            case 0x73: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32(a.of.i32 ^ b.of.i32));
                break;
            }
            case 0x74: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits((uint32_t)a.of.i32 << ((uint32_t)b.of.i32 & 31u)));
                break;
            }
            case 0x75: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u32_bits(wasm__shr_s32_bits((uint32_t)a.of.i32,
                                                                  (uint32_t)b.of.i32)));
                break;
            }
            case 0x76: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)((uint32_t)a.of.i32 >> (b.of.i32 & 31))));
                break;
            }
            case 0x77: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t k = (uint32_t)b.of.i32 & 31u;
                WASM__SP_PUT(
                           wasm__u32_bits(((uint32_t)a.of.i32 << k) |
                                          ((uint32_t)a.of.i32 >> ((32u - k) & 31u))));
                break;
            }
            case 0x78: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t k = (uint32_t)b.of.i32 & 31u;
                WASM__SP_PUT(
                           wasm__u32_bits(((uint32_t)a.of.i32 >> k) |
                                          ((uint32_t)a.of.i32 << ((32u - k) & 31u))));
                break;
            }

            /* i64 arithmetic */
            case 0x79: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)wasm__clz64((uint64_t)a.of.i64)));
                break;
            }
            case 0x7A: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)wasm__ctz64((uint64_t)a.of.i64)));
                break;
            }
            case 0x7B: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)wasm__popcnt64((uint64_t)a.of.i64)));
                break;
            }
            case 0x7C: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits((uint64_t)a.of.i64 + (uint64_t)b.of.i64));
                break;
            }
            case 0x7D: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits((uint64_t)a.of.i64 - (uint64_t)b.of.i64));
                break;
            }
            case 0x7E: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits((uint64_t)a.of.i64 * (uint64_t)b.of.i64));
                break;
            }
            case 0x7F: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                if (a.of.i64 == INT64_MIN && b.of.i64 == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
                WASM__SP_PUT(wasm_i64(a.of.i64 / b.of.i64));
                break;
            }
            case 0x80: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i64((int64_t)((uint64_t)a.of.i64 / (uint64_t)b.of.i64)));
                break;
            }
            case 0x81: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i64((a.of.i64 == INT64_MIN && b.of.i64 == -1) ? 0 : a.of.i64 % b.of.i64));
                break;
            }
            case 0x82: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
                WASM__SP_PUT(wasm_i64((int64_t)((uint64_t)a.of.i64 % (uint64_t)b.of.i64)));
                break;
            }
            case 0x83: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64(a.of.i64 & b.of.i64));
                break;
            }
            case 0x84: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64(a.of.i64 | b.of.i64));
                break;
            }
            case 0x85: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64(a.of.i64 ^ b.of.i64));
                break;
            }
            case 0x86: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits((uint64_t)a.of.i64 << ((uint32_t)b.of.i64 & 63u)));
                break;
            }
            case 0x87: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm__u64_bits(wasm__shr_s64_bits((uint64_t)a.of.i64,
                                                                  (uint32_t)b.of.i64)));
                break;
            }
            case 0x88: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)((uint64_t)a.of.i64 >> (b.of.i64 & 63))));
                break;
            }
            case 0x89: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t k = (uint32_t)b.of.i64 & 63u;
                WASM__SP_PUT(
                           wasm__u64_bits(((uint64_t)a.of.i64 << k) |
                                          ((uint64_t)a.of.i64 >> ((64u - k) & 63u))));
                break;
            }
            case 0x8A: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t k = (uint32_t)b.of.i64 & 63u;
                WASM__SP_PUT(
                           wasm__u64_bits(((uint64_t)a.of.i64 >> k) |
                                          ((uint64_t)a.of.i64 << ((64u - k) & 63u))));
                break;
            }

            /* f32 arithmetic */
            case 0x8B: {
                wasm_value_t a = WASM__SP_POP();
                uint32_t bits;
                memcpy(&bits, &a.of.f32, sizeof(bits));
                WASM__SP_PUT(wasm__f32_bits(bits & 0x7FFFFFFFu));
                break;
            }
            case 0x8C: {
                wasm_value_t a = WASM__SP_POP();
                uint32_t bits;
                memcpy(&bits, &a.of.f32, sizeof(bits));
                WASM__SP_PUT(wasm__f32_bits(bits ^ 0x80000000u));
                break;
            }
            case 0x8D: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__ceil_f32(a.of.f32)));
                break;
            }
            case 0x8E: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__floor_f32(a.of.f32)));
                break;
            }
            case 0x8F: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__trunc_f32(a.of.f32)));
                break;
            }
            case 0x90: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__nearest_f32(a.of.f32)));
                break;
            }
            case 0x91: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__sqrt_f32(a.of.f32)));
                break;
            }
            case 0x92: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(a.of.f32 + b.of.f32));
                break;
            }
            case 0x93: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(a.of.f32 - b.of.f32));
                break;
            }
            case 0x94: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(a.of.f32 * b.of.f32));
                break;
            }
            case 0x95: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(a.of.f32 / b.of.f32));
                break;
            }
            case 0x96: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__f32_min(a.of.f32, b.of.f32)));
                break;
            }
            case 0x97: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32(wasm__f32_max(a.of.f32, b.of.f32)));
                break;
            }
            case 0x98: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint32_t ab, bb, rb;
                memcpy(&ab, &a.of.f32, 4);
                memcpy(&bb, &b.of.f32, 4);
                rb = (ab & 0x7FFFFFFFu) | (bb & 0x80000000u);
                WASM__SP_PUT(wasm__f32_bits(rb));
                break;
            }

            /* f64 arithmetic */
            case 0x99: {
                wasm_value_t a = WASM__SP_POP();
                uint64_t bits;
                memcpy(&bits, &a.of.f64, sizeof(bits));
                WASM__SP_PUT(wasm__f64_bits(bits & UINT64_C(0x7FFFFFFFFFFFFFFF)));
                break;
            }
            case 0x9A: {
                wasm_value_t a = WASM__SP_POP();
                uint64_t bits;
                memcpy(&bits, &a.of.f64, sizeof(bits));
                WASM__SP_PUT(wasm__f64_bits(bits ^ UINT64_C(0x8000000000000000)));
                break;
            }
            case 0x9B: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__ceil_f64(a.of.f64)));
                break;
            }
            case 0x9C: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__floor_f64(a.of.f64)));
                break;
            }
            case 0x9D: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__trunc_f64(a.of.f64)));
                break;
            }
            case 0x9E: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__nearest_f64(a.of.f64)));
                break;
            }
            case 0x9F: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__sqrt_f64(a.of.f64)));
                break;
            }
            case 0xA0: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(a.of.f64 + b.of.f64));
                break;
            }
            case 0xA1: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(a.of.f64 - b.of.f64));
                break;
            }
            case 0xA2: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(a.of.f64 * b.of.f64));
                break;
            }
            case 0xA3: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(a.of.f64 / b.of.f64));
                break;
            }
            case 0xA4: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__f64_min(a.of.f64, b.of.f64)));
                break;
            }
            case 0xA5: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64(wasm__f64_max(a.of.f64, b.of.f64)));
                break;
            }
            case 0xA6: {
                wasm_value_t b = WASM__SP_POP(), a = WASM__SP_POP();
                uint64_t ab, bb, rb;
                memcpy(&ab, &a.of.f64, 8);
                memcpy(&bb, &b.of.f64, 8);
                rb = (ab & (uint64_t)0x7FFFFFFFFFFFFFFFull) | (bb & (uint64_t)0x8000000000000000ull);
                WASM__SP_PUT(wasm__f64_bits(rb));
                break;
            }

            /* Conversions */
            case 0xA7: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i32((int32_t)a.of.i64));
                break;
            }
            case 0xA8: {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i32_s((double)a.of.f32, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                break;
            }
            case 0xA9: {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i32_u((double)a.of.f32, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                break;
            }
            case 0xAA: {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i32_s(a.of.f64, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                break;
            }
            case 0xAB: {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i32_u(a.of.f64, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                break;
            }
            case 0xAC: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)a.of.i32));
                break;
            }
            case 0xAD: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_i64((int64_t)(uint32_t)a.of.i32));
                break;
            }
            case 0xAE: {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i64_s((double)a.of.f32, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                break;
            }
            case 0xAF: {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i64_u((double)a.of.f32, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                break;
            }
            case 0xB0: {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i64_s(a.of.f64, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                break;
            }
            case 0xB1: {
                wasm_value_t a = WASM__SP_POP();
                wasm_value_t value;
                err = wasm__trunc_i64_u(a.of.f64, &value);
                if (err != WASM_OK) goto cleanup;
                WASM__SP_PUT(value);
                break;
            }
            case 0xB2: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)a.of.i32));
                break;
            }
            case 0xB3: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)(uint32_t)a.of.i32));
                break;
            }
            case 0xB4: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)a.of.i64));
                break;
            }
            case 0xB5: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)(uint64_t)a.of.i64));
                break;
            }
            case 0xB6: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f32((float)a.of.f64));
                break;
            }
            case 0xB7: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)a.of.i32));
                break;
            }
            case 0xB8: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)(uint32_t)a.of.i32));
                break;
            }
            case 0xB9: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)a.of.i64));
                break;
            }
            case 0xBA: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)(uint64_t)a.of.i64));
                break;
            }
            case 0xBB: {
                wasm_value_t a = WASM__SP_POP();
                WASM__SP_PUT(wasm_f64((double)a.of.f32));
                break;
            }

            /* Reinterpretations */
            case 0xBC: {
                wasm_value_t a = WASM__SP_POP();
                int32_t v;
                memcpy(&v, &a.of.f32, 4);
                WASM__SP_PUT(wasm_i32(v));
                break;
            }
            case 0xBD: {
                wasm_value_t a = WASM__SP_POP();
                int64_t v;
                memcpy(&v, &a.of.f64, 8);
                WASM__SP_PUT(wasm_i64(v));
                break;
            }
            case 0xBE: {
                wasm_value_t a = WASM__SP_POP();
                a.type = WASM_TYPE_F32;
                WASM__SP_PUT(a);
                break;
            }
            case 0xBF: {
                wasm_value_t a = WASM__SP_POP();
                a.type = WASM_TYPE_F64;
                WASM__SP_PUT(a);
                break;
            }

            /* Sign extension */
            case 0xC0: {
                wasm_value_t a = WASM__SP_POP();
                uint32_t value = (uint32_t)a.of.i32 & 0xFFu;
                if (value & 0x80u) value |= 0xFFFFFF00u;
                WASM__SP_PUT(wasm_i32((int32_t)value));
                break;
            }
            case 0xC1: {
                wasm_value_t a = WASM__SP_POP();
                uint32_t value = (uint32_t)a.of.i32 & 0xFFFFu;
                if (value & 0x8000u) value |= 0xFFFF0000u;
                WASM__SP_PUT(wasm_i32((int32_t)value));
                break;
            }
            case 0xC2: {
                wasm_value_t a = WASM__SP_POP();
                uint64_t value = (uint64_t)a.of.i64 & 0xFFu;
                if (value & 0x80u) value |= 0xFFFFFFFFFFFFFF00ull;
                WASM__SP_PUT(wasm_i64((int64_t)value));
                break;
            }
            case 0xC3: {
                wasm_value_t a = WASM__SP_POP();
                uint64_t value = (uint64_t)a.of.i64 & 0xFFFFu;
                if (value & 0x8000u) value |= 0xFFFFFFFFFFFF0000ull;
                WASM__SP_PUT(wasm_i64((int64_t)value));
                break;
            }
            case 0xC4: {
                wasm_value_t a = WASM__SP_POP();
                uint64_t value = (uint64_t)a.of.i64 & 0xFFFFFFFFull;
                if (value & 0x80000000ull) value |= 0xFFFFFFFF00000000ull;
                WASM__SP_PUT(wasm_i64((int64_t)value));
                break;
            }

            default:
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown opcode 0x%02X", op);
                WASM__TRAP(WASM_ERR_MALFORMED);
        }
        continue;

    do_branch: {
        wasm__label_t* target = &labels[lsp - 1 - branch_depth];
        wasm_value_t* dst = stack + target->sp_base;
        uint32_t arity;
        uint32_t pops;

        if (target == labels) goto return_frame;
        if (target->opcode == 0x03) {
            if (wasm__interrupt_pending(rt)) WASM__TRAP(WASM_ERR_INTERRUPTED);
            arity = target->param_arity;
            pops = branch_depth;
        } else {
            arity = target->arity;
            pops = branch_depth + 1;
        }
        if (arity > 0 && sp - arity != dst) memmove(dst, sp - arity, arity * sizeof(wasm_value_t));
        sp = dst + arity;
        pc = target->continuation;
        wasm__drop_labels(labels, lsp, pops);
        lsp -= pops;
        continue;
    }

    do_call:
        if (call_mod->funcs[call_idx].is_import) {
            sp -= call_type->num_params;
            WASM__SAVE_STATE();
            err = wasm__call_host_from_stack(call_mod, call_type, &call_mod->funcs[call_idx], sp);
            sp = stack + rt->sp;
            if (err == WASM_OK) continue;
            if (err != WASM__ERR_EXCEPTION) goto cleanup;
            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 0);
            if (err != WASM__ERR_RESUME_FRAME) return err;
            goto resume_frame;
        }
        sp -= call_type->num_params;
        WASM__SAVE_STATE();
        err = wasm__push_call_frame(call_mod, call_idx, sp, call_type->num_params, NULL, 0);
        if (err != WASM_OK) goto cleanup;
        WASM__ENTER_FRAME();
        continue;

    do_tail_call: {
        wasm_value_t* results_dst = cf->results_dst;
        uint32_t num_results = cf->num_results;
        uint32_t num_params = call_type->num_params;

        if (call_mod->funcs[call_idx].is_import) {
            sp -= num_params;
            WASM__SAVE_STATE();
            err = wasm__call_host_from_stack(call_mod, call_type, &call_mod->funcs[call_idx], sp);
            sp = stack + rt->sp;
            if (err == WASM_OK) goto return_frame;
            if (err != WASM__ERR_EXCEPTION) goto cleanup;
            err = wasm__unwind_host_exception(mod, cf, base_frame_sp, 1);
            if (err != WASM__ERR_RESUME_FRAME) return err;
            goto resume_frame;
        }
        if (num_params > 0 && sp - num_params != stack + sp_base)
            memmove(stack + sp_base, sp - num_params, num_params * sizeof(wasm_value_t));
        sp = stack + sp_base;
        wasm__drop_labels(labels, lsp, lsp);
        lsp = 0;
        WASM__SAVE_STATE();
        wasm__arena_reset(rt, cf->arena_saved);
        rt->call_frame_sp--;
        err = wasm__push_call_frame(call_mod, call_idx, sp, num_params, results_dst, num_results);
        if (err != WASM_OK) return err;
        WASM__ENTER_FRAME();
        continue;
    }

    throw_pending:
        WASM__SAVE_STATE();
        err = wasm__handle_exception_frames(mod, base_frame_sp);
        if (err != WASM_OK) return err;
    resume_frame:
        WASM__ENTER_FRAME();
        if (pc >= cf->r.end) goto return_frame;
        continue;

    return_frame:
        if (rt->call_frame_sp == base_frame_sp + 1u) {
            err = WASM__ERR_RETURN_FRAME;
            goto cleanup;
        }
        {
            const wasm_functype_t* ft = cf->ft;
            wasm_value_t* dst = stack + sp_base;
            wasm_value_t* src = sp - ft->num_results;

            for (i = 0; i < ft->num_results; i++) {
                dst[i] = wasm__is_numeric_valtype(ft->results[i])
                             ? src[i]
                             : wasm__retag_value_for_declared_type(
                                   ft->results[i], ft->result_reftypes ? &ft->result_reftypes[i] : NULL, src[i]);
            }
            sp = dst + ft->num_results;
            wasm__drop_labels(labels, lsp, lsp);
            cf->lsp = 0;
            wasm__arena_reset(rt, cf->arena_saved);
            rt->call_frame_sp--;
            rt->sp = (uint32_t)(sp - stack);
            WASM__ENTER_FRAME();
            continue;
        }
    }

cleanup:
    WASM__SAVE_STATE();
    while (cf->lsp > 0) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }

    return err;
}

#undef WASM__PUSH_LABEL
#undef WASM__SP_TOP
#undef WASM__SP_POP
#undef WASM__SP_PUT
#undef WASM__SP_PUSH
#undef WASM__CALL_OUT
#undef WASM__ENTER_FRAME
#undef WASM__LOAD_STATE
#undef WASM__SAVE_STATE

WASM_PRIVATE wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results) {
    wasm_runtime_t* rt = mod->rt;
    wasm_func_t* func;
    uint32_t base_frame_sp = rt->call_frame_sp;
    wasm_error_t err;

    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import)
        return wasm__invoke_host_func(mod, wasm__module_functype(mod, func->type_idx), func->host_func,
                                      args, results, func->host_userdata);

    err = wasm__push_call_frame(mod, func_idx, args, num_args, results, num_results);
    if (err != WASM_OK) return err;
    err = wasm__interp_loop(rt, base_frame_sp);
    if (err == WASM__ERR_RETURN_FRAME) err = wasm__finish_call_frame(rt, base_frame_sp);
    if (err != WASM_OK && rt->call_frame_sp > base_frame_sp)
        wasm__capture_backtrace(rt, base_frame_sp);
    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];
        wasm__clear_call_frame(cf);
        wasm__arena_reset(rt, cf->arena_saved);
        memset(cf, 0, sizeof(*cf));
        rt->call_frame_sp--;
    }
    return err;
}

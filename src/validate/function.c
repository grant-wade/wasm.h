#include "function.h"
#include "../module.h"
#include <stdio.h>
#include "state.h"
#include "../exec/control.h"
#include "../instructions.h"
#include "../internal/alloc.h"
#include "../memory_internal.h"
#include "../reader.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include "../types/references.h"
#include "gc.h"
#include "module.h"
#include "numeric.h"
#include "simd.h"
#include "stack.h"
#include "../values.h"

static wasm_error_t wasm__validate_function(wasm_module_t* mod, uint32_t func_idx) {
    wasm__validator_t v;
    wasm_func_t* func = &mod->funcs[func_idx];
    wasm_functype_t* ft = wasm__module_functype(mod, func->type_idx);
    wasm_error_t err;

    if (!ft) {
        return wasm__set_validation_error(mod->rt, func_idx, 0,
                                          "function %u type index %u is not a function type",
                                          (unsigned)func_idx,
                                          (unsigned)func->type_idx);
    }

    err = wasm__validator_init(&v, mod, func_idx);
    if (err != WASM_OK) {
        return wasm__set_validation_error(mod->rt, func_idx, 0,
                                          "failed to allocate validator state");
    }

    v.body_start = func->code;
    v.r.ptr = func->code;
    v.r.end = func->code + func->code_len;
    v.r.malformed = 0;
    v.frames[0].opcode = 0xFF;
    v.frames[0].height = 0;
    v.frames[0].param_types = NULL;
    v.frames[0].param_reftypes = NULL;
    v.frames[0].num_params = 0;
    v.frames[0].result_types = ft->results;
    v.frames[0].result_reftypes = ft->result_reftypes;
    v.frames[0].num_results = ft->num_results;
    wasm__clear_reftype(&v.frames[0].inline_result_reftype);
    v.frames[0].in_catch = 0;
    v.frames[0].seen_catch_all = 0;
    v.frames[0].seen_delegate = 0;
    v.fp = 1;
    v.max_fp = 1;

    while (v.r.ptr < v.r.end) {
        const uint8_t* at = v.r.ptr;
        uint8_t op = wasm__read_u8(&v.r);

        err = WASM_OK;

        switch (op) {
            case 0x00:
                wasm__validator_mark_unreachable(&v);
                break;
            case 0x01:
                break;
            case 0x02:
            case 0x03: {
                wasm__blocktype_t blocktype;
                uint32_t entry_height;

                err = wasm__read_blocktype(mod, &v.r, &blocktype);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "%s", mod->rt->error_msg);
                err = wasm__validator_check_types_typed(&v, at,
                                                        blocktype.params,
                                                        blocktype.param_reftypes,
                                                        blocktype.param_arity);
                if (err != WASM_OK) return err;
                entry_height = wasm__validator_stack_floor_after_checked(&v, blocktype.param_arity);
                if (v.fp >= v.frame_capacity)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = entry_height;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].param_reftypes = blocktype.param_reftypes;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].inline_result_reftype = blocktype.inline_result_reftype;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].result_reftypes =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result_reftype
                        : blocktype.result_reftypes;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                wasm__validator_enter_child_frame(&v, v.fp);
                v.fp++;
                if (v.fp > v.max_fp) v.max_fp = v.fp;
                break;
            }
            case 0x04: {
                wasm__blocktype_t blocktype;
                uint32_t entry_height;

                err = wasm__read_blocktype(mod, &v.r, &blocktype);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "%s", mod->rt->error_msg);
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_check_types_typed(&v, at,
                                                        blocktype.params,
                                                        blocktype.param_reftypes,
                                                        blocktype.param_arity);
                if (err != WASM_OK) return err;
                entry_height = wasm__validator_stack_floor_after_checked(&v, blocktype.param_arity);
                if (v.fp >= v.frame_capacity)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = entry_height;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].param_reftypes = blocktype.param_reftypes;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].inline_result_reftype = blocktype.inline_result_reftype;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].result_reftypes =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result_reftype
                        : blocktype.result_reftypes;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                wasm__validator_enter_child_frame(&v, v.fp);
                v.fp++;
                if (v.fp > v.max_fp) v.max_fp = v.fp;
                break;
            }
            case 0x05: {
                wasm__val_frame_t* frame;

                if (v.fp <= 1)
                    return wasm__validator_error(&v, at, "else without matching if");
                frame = wasm__validator_frame(&v);
                if (frame->opcode != 0x04 || frame->seen_else)
                    return wasm__validator_error(&v, at, "unexpected else");
                err = wasm__validator_check_frame_results(&v, at, frame,
                                                          "then branch leaves wrong stack height");
                if (err != WASM_OK) return err;
                v.sp = frame->height;
                err = wasm__validator_push_many_typed(&v, at,
                                                      frame->param_types,
                                                      frame->param_reftypes,
                                                      frame->num_params);
                if (err != WASM_OK) return err;
                wasm__validator_restore_entry_local_init(&v, frame);
                frame->unreachable = 0;
                frame->seen_else = 1;
                break;
            }
            case 0x06: {
                wasm__blocktype_t blocktype;
                uint32_t entry_height;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                err = wasm__read_blocktype(mod, &v.r, &blocktype);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "%s", mod->rt->error_msg);
                err = wasm__validator_check_types_typed(&v, at,
                                                        blocktype.params,
                                                        blocktype.param_reftypes,
                                                        blocktype.param_arity);
                if (err != WASM_OK) return err;
                entry_height = wasm__validator_stack_floor_after_checked(&v, blocktype.param_arity);
                if (v.fp >= v.frame_capacity)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = entry_height;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].param_reftypes = blocktype.param_reftypes;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].inline_result_reftype = blocktype.inline_result_reftype;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].result_reftypes =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result_reftype
                        : blocktype.result_reftypes;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                wasm__validator_enter_child_frame(&v, v.fp);
                v.fp++;
                if (v.fp > v.max_fp) v.max_fp = v.fp;
                break;
            }
            case 0x07: {
                uint32_t tag_index = wasm__read_leb128_u32(&v.r);
                wasm__val_frame_t* frame;
                wasm_functype_t* tag_type;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                if (v.fp <= 1)
                    return wasm__validator_error(&v, at, "catch without matching try");
                frame = wasm__validator_frame(&v);
                if (frame->opcode != 0x06 || frame->seen_catch_all || frame->seen_delegate)
                    return wasm__validator_error(&v, at, "unexpected catch");
                if (tag_index >= mod->num_tags)
                    return wasm__validator_error(&v, at, "tag %u out of range", (unsigned)tag_index);
                tag_type = wasm__module_functype(mod, mod->tags[tag_index].type_idx);
                if (!tag_type)
                    return wasm__validator_error(&v, at, "tag %u does not reference a function type",
                                                 (unsigned)tag_index);
                err = wasm__validator_finish_try_arm(&v, at, frame);
                if (err != WASM_OK) return err;
                err = wasm__validator_push_many_typed(&v, at,
                                                      tag_type->params,
                                                      tag_type->param_reftypes,
                                                      tag_type->num_params);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x08: {
                uint32_t tag_index = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* tag_type;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                if (tag_index >= mod->num_tags)
                    return wasm__validator_error(&v, at, "tag %u out of range", (unsigned)tag_index);
                tag_type = wasm__module_functype(mod, mod->tags[tag_index].type_idx);
                if (!tag_type)
                    return wasm__validator_error(&v, at, "tag %u does not reference a function type",
                                                 (unsigned)tag_index);
                err = wasm__validator_check_types_typed(&v, at,
                                                        tag_type->params,
                                                        tag_type->param_reftypes,
                                                        tag_type->num_params);
                if (err != WASM_OK) return err;
                wasm__validator_consume_checked(&v, tag_type->num_params);
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x09: {
                uint32_t depth = wasm__read_leb128_u32(&v.r);
                wasm__val_frame_t* target_frame;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                err = wasm__validator_find_rethrow_frame(&v, depth, &target_frame);
                if (err != WASM_OK) {
                    return wasm__validator_error(&v, at,
                                                 "rethrow depth %u does not reference an active catch",
                                                 (unsigned)depth);
                }
                (void)target_frame;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x0A:
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_EXNREF);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            case 0x0B: {
                wasm__val_frame_t frame;
                if (v.fp == 0)
                    return wasm__validator_error(&v, at, "unexpected end");
                frame = v.frames[v.fp - 1];
                if (frame.opcode == 0x04 && !frame.seen_else &&
                    !wasm__validator_same_types(frame.param_types, frame.param_reftypes,
                                                frame.num_params,
                                                frame.result_types, frame.result_reftypes,
                                                frame.num_results)) {
                    return wasm__validator_error(&v, at,
                                                 "if without else requires param and result types to match");
                }
                err = wasm__validator_check_frame_results(&v, at, &frame,
                                                          "type mismatch: block leaves wrong stack height");
                if (err != WASM_OK) return err;
                v.sp = frame.height;
                v.fp--;
                err = wasm__validator_push_many_typed(&v, at,
                                                      frame.result_types,
                                                      frame.result_reftypes,
                                                      frame.num_results);
                if (err != WASM_OK) return err;
                if (v.fp == 0) {
                    if (v.r.ptr != v.r.end)
                        return wasm__validator_error(&v, at, "instructions found after function end");
                    break;
                }
                break;
            }
            case 0x0C:
            case 0x0D: {
                uint32_t depth = wasm__read_leb128_u32(&v.r);
                const wasm__val_frame_t* target;
                const wasm_valtype_t* branch_types;
                const wasm_reftype_t* branch_reftypes;
                uint32_t branch_count;

                if (op == 0x0D) {
                    err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                }
                if (depth >= v.fp)
                    return wasm__validator_error(&v, at, "branch depth %u out of range", (unsigned)depth);
                target = &v.frames[v.fp - 1 - depth];
                wasm__validator_branch_signature(target, &branch_types, &branch_reftypes, &branch_count);
                if (op == 0x0D) {
                    err = wasm__validator_pop_and_push_types(&v, at,
                                                             branch_types,
                                                             branch_reftypes,
                                                             branch_count);
                } else {
                    err = wasm__validator_check_types_typed(&v, at,
                                                            branch_types,
                                                            branch_reftypes,
                                                            branch_count);
                }
                if (err != WASM_OK) return err;
                if (op == 0x0C) wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x0E: {
                uint32_t count = wasm__read_leb128_u32(&v.r);
                const wasm_valtype_t* branch_types = NULL;
                const wasm_reftype_t* branch_reftypes = NULL;
                uint32_t branch_count = 0;
                const wasm__val_frame_t* frame = wasm__validator_frame(&v);
                uint32_t depth;
                uint32_t i;

                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                depth = wasm__read_leb128_u32(&v.r);
                if (depth >= v.fp)
                    return wasm__validator_error(&v, at, "br_table depth %u out of range", (unsigned)depth);
                wasm__validator_branch_signature(&v.frames[v.fp - 1 - depth],
                                                 &branch_types,
                                                 &branch_reftypes,
                                                 &branch_count);

                {
                    wasm_valtype_t* merged_types = NULL;
                    wasm_reftype_t* merged_reftypes = NULL;

                    if (branch_count != 0) {
                        merged_types = (wasm_valtype_t*)wasm__malloc_array((size_t)branch_count, sizeof(*merged_types));
                        merged_reftypes = (wasm_reftype_t*)wasm__malloc_array((size_t)branch_count, sizeof(*merged_reftypes));
                        if (!merged_types || !merged_reftypes) {
                            WASM_FREE(merged_types);
                            WASM_FREE(merged_reftypes);
                            WASM__SET_ERR(v.mod->rt, WASM_ERR_OOM, "%s",
                                          "failed to allocate br_table validator state");
                            err = WASM_ERR_OOM;
                            goto done;
                        }
                    }

                    for (i = 0; i < branch_count; i++) {
                        merged_types[i] = branch_types[i];
                        if (branch_reftypes && wasm__has_reftype_info(&branch_reftypes[i]))
                            merged_reftypes[i] = branch_reftypes[i];
                        else
                            wasm__clear_reftype(&merged_reftypes[i]);
                    }

                    for (i = 1; i <= count; i++) {
                        depth = wasm__read_leb128_u32(&v.r);
                        {
                            const wasm_valtype_t* candidate_types;
                            const wasm_reftype_t* candidate_reftypes;
                            uint32_t candidate_count;

                            if (depth >= v.fp) {
                                WASM_FREE(merged_types);
                                WASM_FREE(merged_reftypes);
                                return wasm__validator_error(&v, at, "br_table depth %u out of range", (unsigned)depth);
                            }
                            wasm__validator_branch_signature(&v.frames[v.fp - 1 - depth],
                                                             &candidate_types,
                                                             &candidate_reftypes,
                                                             &candidate_count);
                            if (!wasm__validator_meet_types(v.mod,
                                                            merged_types,
                                                            merged_reftypes,
                                                            branch_count,
                                                            candidate_types,
                                                            candidate_reftypes,
                                                            candidate_count,
                                                            merged_types,
                                                            merged_reftypes)) {
                                if (!(frame->unreachable && v.sp == frame->height && branch_count == candidate_count)) {
                                    WASM_FREE(merged_types);
                                    WASM_FREE(merged_reftypes);
                                    return wasm__validator_error(&v, at,
                                                                 "br_table targets do not share the same signature");
                                }
                            }
                        }
                    }

                    err = wasm__validator_check_types_typed(&v, at,
                                                            merged_types,
                                                            merged_reftypes,
                                                            branch_count);
                    WASM_FREE(merged_types);
                    WASM_FREE(merged_reftypes);
                    if (err != WASM_OK) return err;
                }
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x0F:
                err = wasm__validator_check_types_typed(&v, at,
                                                        ft->results,
                                                        ft->result_reftypes,
                                                        ft->num_results);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            case 0x10: {
                uint32_t callee = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;
                if (callee >= mod->num_funcs)
                    return wasm__validator_error(&v, at, "call target %u out of range", (unsigned)callee);
                callee_type = wasm__module_functype(mod, mod->funcs[callee].type_idx);
                if (!callee_type)
                    return wasm__validator_error(&v, at, "call target %u does not use a function type",
                                                 (unsigned)callee);
                err = wasm__validator_check_types_typed(&v, at,
                                                        callee_type->params,
                                                        callee_type->param_reftypes,
                                                        callee_type->num_params);
                if (err != WASM_OK) return err;
                wasm__validator_consume_checked(&v, callee_type->num_params);
                err = wasm__validator_push_many_typed(&v, at,
                                                      callee_type->results,
                                                      callee_type->result_reftypes,
                                                      callee_type->num_results);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x11: {
                uint32_t type_idx = wasm__read_leb128_u32(&v.r);
                uint32_t table_idx = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;
                if (type_idx >= mod->num_types)
                    return wasm__validator_error(&v, at, "call_indirect type index %u out of range", (unsigned)type_idx);
                if (table_idx >= mod->num_tables)
                    return wasm__validator_error(&v, at, "call_indirect table index %u out of range", (unsigned)table_idx);
                if (table_idx != 0) {
                    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                    if (err != WASM_OK) return err;
                }
                if (mod->tables[table_idx].reftype != WASM_TYPE_FUNCREF)
                    return wasm__validator_error(&v, at, "call_indirect requires a funcref table");
                callee_type = wasm__module_functype(mod, type_idx);
                if (!callee_type)
                    return wasm__validator_error(&v, at, "call_indirect type index %u is not a function type",
                                                 (unsigned)type_idx);
                err = wasm__validator_pop_expect(&v, at, wasm__table_index_type(&mod->tables[table_idx]));
                if (err != WASM_OK) return err;
                err = wasm__validator_check_types_typed(&v, at,
                                                        callee_type->params,
                                                        callee_type->param_reftypes,
                                                        callee_type->num_params);
                if (err != WASM_OK) return err;
                wasm__validator_consume_checked(&v, callee_type->num_params);
                err = wasm__validator_push_many_typed(&v, at,
                                                      callee_type->results,
                                                      callee_type->result_reftypes,
                                                      callee_type->num_results);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x12: {
                uint32_t callee = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_TAIL_CALL);
                if (err != WASM_OK) return err;
                if (callee >= mod->num_funcs)
                    return wasm__validator_error(&v, at, "call target %u out of range", (unsigned)callee);
                callee_type = wasm__module_functype(mod, mod->funcs[callee].type_idx);
                if (!callee_type)
                    return wasm__validator_error(&v, at, "call target %u does not use a function type",
                                                 (unsigned)callee);
                err = wasm__validator_check_types_typed(&v, at,
                                                        callee_type->params,
                                                        callee_type->param_reftypes,
                                                        callee_type->num_params);
                if (err != WASM_OK) return err;
                wasm__validator_consume_checked(&v, callee_type->num_params);
                err = wasm__validator_check_tail_results(&v, at, ft, callee_type);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x13: {
                uint32_t type_idx = wasm__read_leb128_u32(&v.r);
                uint32_t table_idx = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_TAIL_CALL);
                if (err != WASM_OK) return err;
                if (type_idx >= mod->num_types)
                    return wasm__validator_error(&v, at, "call_indirect type index %u out of range", (unsigned)type_idx);
                if (table_idx >= mod->num_tables)
                    return wasm__validator_error(&v, at, "call_indirect table index %u out of range", (unsigned)table_idx);
                if (table_idx != 0) {
                    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                    if (err != WASM_OK) return err;
                }
                if (mod->tables[table_idx].reftype != WASM_TYPE_FUNCREF)
                    return wasm__validator_error(&v, at, "call_indirect requires a funcref table");
                callee_type = wasm__module_functype(mod, type_idx);
                if (!callee_type)
                    return wasm__validator_error(&v, at, "call_indirect type index %u is not a function type",
                                                 (unsigned)type_idx);
                err = wasm__validator_pop_expect(&v, at, wasm__table_index_type(&mod->tables[table_idx]));
                if (err != WASM_OK) return err;
                err = wasm__validator_check_types_typed(&v, at,
                                                        callee_type->params,
                                                        callee_type->param_reftypes,
                                                        callee_type->num_params);
                if (err != WASM_OK) return err;
                wasm__validator_consume_checked(&v, callee_type->num_params);
                err = wasm__validator_check_tail_results(&v, at, ft, callee_type);
                if (err != WASM_OK) return err;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x14:
            case 0x15: {
                uint32_t type_idx = wasm__read_leb128_u32(&v.r);
                wasm_functype_t* callee_type;
                wasm_valtype_t callee_ref_type;
                wasm_reftype_t callee_ref;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                if (op == 0x15) {
                    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_TAIL_CALL);
                    if (err != WASM_OK) return err;
                }
                if (type_idx >= mod->num_types)
                    return wasm__validator_error(&v, at, "call_ref type index %u out of range", (unsigned)type_idx);
                callee_type = wasm__module_functype(mod, type_idx);
                if (!callee_type)
                    return wasm__validator_error(&v, at, "call_ref type index %u is not a function type",
                                                 (unsigned)type_idx);
                err = wasm__validator_make_typeidx_reftype(&v, at, type_idx, 1,
                                                           &callee_ref_type, &callee_ref);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect_typed(&v, at, callee_ref_type, &callee_ref);
                if (err != WASM_OK) return err;
                err = wasm__validator_check_types_typed(&v, at,
                                                        callee_type->params,
                                                        callee_type->param_reftypes,
                                                        callee_type->num_params);
                if (err != WASM_OK) return err;
                wasm__validator_consume_checked(&v, callee_type->num_params);
                if (op == 0x14) {
                    err = wasm__validator_push_many_typed(&v, at,
                                                          callee_type->results,
                                                          callee_type->result_reftypes,
                                                          callee_type->num_results);
                    if (err != WASM_OK) return err;
                } else {
                    err = wasm__validator_check_tail_results(&v, at, ft, callee_type);
                    if (err != WASM_OK) return err;
                    wasm__validator_mark_unreachable(&v);
                }
                break;
            }
            case 0x1F: {
                wasm__blocktype_t blocktype;
                uint32_t entry_height;
                uint32_t clause_count;
                uint32_t clause_index;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                err = wasm__read_blocktype(mod, &v.r, &blocktype);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "%s", mod->rt->error_msg);
                err = wasm__validator_check_types_typed(&v, at,
                                                        blocktype.params,
                                                        blocktype.param_reftypes,
                                                        blocktype.param_arity);
                if (err != WASM_OK) return err;
                entry_height = wasm__validator_stack_floor_after_checked(&v, blocktype.param_arity);
                if (v.fp >= v.frame_capacity)
                    return wasm__validator_error(&v, at, "control stack overflow");
                v.frames[v.fp].opcode = op;
                v.frames[v.fp].height = entry_height;
                v.frames[v.fp].param_types = blocktype.params;
                v.frames[v.fp].param_reftypes = blocktype.param_reftypes;
                v.frames[v.fp].num_params = blocktype.param_arity;
                v.frames[v.fp].inline_result = blocktype.inline_result;
                v.frames[v.fp].inline_result_reftype = blocktype.inline_result_reftype;
                v.frames[v.fp].result_types =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result
                        : blocktype.results;
                v.frames[v.fp].result_reftypes =
                    (blocktype.result_arity == 1 && blocktype.results == &blocktype.inline_result)
                        ? &v.frames[v.fp].inline_result_reftype
                        : blocktype.result_reftypes;
                v.frames[v.fp].num_results = blocktype.result_arity;
                v.frames[v.fp].unreachable = 0;
                v.frames[v.fp].seen_else = 0;
                v.frames[v.fp].in_catch = 0;
                v.frames[v.fp].seen_catch_all = 0;
                v.frames[v.fp].seen_delegate = 0;
                wasm__validator_enter_child_frame(&v, v.fp);
                v.fp++;
                if (v.fp > v.max_fp) v.max_fp = v.fp;

                clause_count = wasm__read_leb128_u32(&v.r);
                for (clause_index = 0; clause_index < clause_count; clause_index++) {
                    wasm__try_table_clause_t clause;
                    const wasm__val_frame_t* target_frame;
                    const wasm_valtype_t* branch_types;
                    const wasm_reftype_t* branch_reftypes;
                    uint32_t branch_count;

                    err = wasm__read_try_table_clause(&v.r, &clause);
                    if (err != WASM_OK)
                        return wasm__validator_error(&v, at, "malformed try_table clause");
                    if (clause.label_depth >= v.fp - 1)
                        return wasm__validator_error(&v, at, "try_table label depth %u out of range",
                                                     (unsigned)clause.label_depth);

                    target_frame = &v.frames[v.fp - 2 - clause.label_depth];
                    wasm__validator_branch_signature(target_frame,
                                                     &branch_types,
                                                     &branch_reftypes,
                                                     &branch_count);

                    switch (clause.kind) {
                        case 0x00: {
                            wasm_functype_t* tag_type;

                            if (clause.tag_index >= mod->num_tags)
                                return wasm__validator_error(&v, at, "tag %u out of range",
                                                             (unsigned)clause.tag_index);
                            tag_type = wasm__module_functype(mod, mod->tags[clause.tag_index].type_idx);
                            if (!tag_type)
                                return wasm__validator_error(&v, at,
                                                             "tag %u does not reference a function type",
                                                             (unsigned)clause.tag_index);
                            if (!wasm__validator_types_match(&v,
                                                             tag_type->params,
                                                             tag_type->param_reftypes,
                                                             tag_type->num_params,
                                                             branch_types,
                                                             branch_reftypes,
                                                             branch_count)) {
                                return wasm__validator_error(&v, at,
                                                             "try_table catch target type mismatch");
                            }
                            break;
                        }
                        case 0x02:
                            if (branch_count != 0)
                                return wasm__validator_error(&v, at,
                                                             "try_table catch_all target type mismatch");
                            break;
                        case 0x01: {
                            wasm_functype_t* tag_type;
                            wasm_valtype_t exn_type = WASM_TYPE_NOEXN;
                            wasm_reftype_t exn_ref;

                            wasm__clear_reftype(&exn_ref);
                            exn_ref.type = WASM_TYPE_NOEXN;
                            exn_ref.nullable = 0;

                            if (clause.tag_index >= mod->num_tags)
                                return wasm__validator_error(&v, at, "tag %u out of range",
                                                             (unsigned)clause.tag_index);
                            tag_type = wasm__module_functype(mod, mod->tags[clause.tag_index].type_idx);
                            if (!tag_type)
                                return wasm__validator_error(&v, at,
                                                             "tag %u does not reference a function type",
                                                             (unsigned)clause.tag_index);
                            if (branch_count != tag_type->num_params + 1u ||
                                !wasm__validator_types_match(&v,
                                                             tag_type->params,
                                                             tag_type->param_reftypes,
                                                             tag_type->num_params,
                                                             branch_types,
                                                             branch_reftypes,
                                                             tag_type->num_params)) {
                                return wasm__validator_error(&v, at,
                                                             "try_table catch_ref target type mismatch");
                            }
                            if (!wasm__validator_type_matches(&v,
                                                              exn_type,
                                                              &exn_ref,
                                                              branch_types[tag_type->num_params],
                                                              branch_reftypes ? &branch_reftypes[tag_type->num_params] : NULL)) {
                                return wasm__validator_error(&v, at,
                                                             "try_table catch_ref target type mismatch");
                            }
                            break;
                        }
                        case 0x03: {
                            wasm_valtype_t exn_type = WASM_TYPE_NOEXN;
                            wasm_reftype_t exn_ref;

                            wasm__clear_reftype(&exn_ref);
                            exn_ref.type = WASM_TYPE_NOEXN;
                            exn_ref.nullable = 0;

                            if (branch_count != 1 ||
                                !wasm__validator_type_matches(&v,
                                                              exn_type,
                                                              &exn_ref,
                                                              branch_types[0],
                                                              branch_reftypes ? &branch_reftypes[0] : NULL))
                                return wasm__validator_error(&v, at,
                                                             "try_table catch_all_ref target type mismatch");
                            break;
                        }
                        default:
                            return wasm__validator_error(&v, at, "invalid try_table clause kind 0x%X",
                                                         (unsigned)clause.kind);
                    }
                }
                break;
            }
            case 0x18: {
                uint32_t depth = wasm__read_leb128_u32(&v.r);
                wasm__val_frame_t* frame;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                if (v.fp <= 1)
                    return wasm__validator_error(&v, at, "delegate without matching try");
                frame = wasm__validator_frame(&v);
                if (frame->opcode != 0x06 || frame->seen_delegate || frame->seen_catch_all)
                    return wasm__validator_error(&v, at, "unexpected delegate");
                if (depth >= v.fp - 1)
                    return wasm__validator_error(&v, at, "delegate depth %u out of range", (unsigned)depth);
                err = wasm__validator_finish_try_arm(&v, at, frame);
                if (err != WASM_OK) return err;
                frame->seen_delegate = 1;
                wasm__validator_mark_unreachable(&v);
                break;
            }
            case 0x1A: {
                wasm_valtype_t ignored;
                err = wasm__validator_pop_any(&v, at, &ignored);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x19: {
                wasm__val_frame_t* frame;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_EXCEPTIONS);
                if (err != WASM_OK) return err;
                if (v.fp <= 1)
                    return wasm__validator_error(&v, at, "catch_all without matching try");
                frame = wasm__validator_frame(&v);
                if (frame->opcode != 0x06 || frame->seen_catch_all || frame->seen_delegate)
                    return wasm__validator_error(&v, at, "unexpected catch_all");
                err = wasm__validator_finish_try_arm(&v, at, frame);
                if (err != WASM_OK) return err;
                frame->seen_catch_all = 1;
                break;
            }
            case 0x1B: {
                wasm_valtype_t lhs, rhs;
                wasm_reftype_t lhs_ref, rhs_ref;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_any_typed(&v, at, &rhs, &rhs_ref);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_any_typed(&v, at, &lhs, &lhs_ref);
                if (err != WASM_OK) return err;
                if (!wasm__validator_type_matches(&v, lhs, &lhs_ref, rhs, &rhs_ref) ||
                    ((lhs != WASM__TYPE_BOT && !wasm__is_number_or_vector_type(lhs)) ||
                     (rhs != WASM__TYPE_BOT && !wasm__is_number_or_vector_type(rhs)))) {
                    return wasm__validator_error(&v, at, "select requires matching numeric or vector operand types");
                }
                err = wasm__validator_push(&v, at, lhs == WASM__TYPE_BOT ? rhs : lhs);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x1C: {
                wasm_valtype_t select_type;
                wasm_reftype_t select_ref_type;

                err = wasm__validator_read_typed_select(&v, at, &select_type, &select_ref_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect_typed(&v, at, select_type, &select_ref_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect_typed(&v, at, select_type, &select_ref_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_push_typed(&v, at, select_type, &select_ref_type);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x20: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= func->num_locals)
                    return wasm__validator_error(&v, at, "local index %u out of range", (unsigned)index);
                if (v.local_init_words != 0 &&
                    !wasm__validator_frame(&v)->unreachable &&
                    wasm__validator_local_is_undefaulted(func, index) &&
                    !wasm__validator_local_init_get(wasm__validator_frame(&v)->local_init, index)) {
                    return wasm__validator_error(&v, at, "%s", "uninitialized local");
                }
                err = wasm__validator_push_typed(&v, at,
                                                 func->locals[index],
                                                 func->local_reftypes ? &func->local_reftypes[index] : NULL);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x21: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= func->num_locals)
                    return wasm__validator_error(&v, at, "local index %u out of range", (unsigned)index);
                err = wasm__validator_pop_expect_typed(&v, at,
                                                       func->locals[index],
                                                       func->local_reftypes ? &func->local_reftypes[index] : NULL);
                if (err != WASM_OK) return err;
                if (v.local_init_words != 0)
                    wasm__validator_local_init_set(wasm__validator_frame(&v)->local_init, index);
                break;
            }
            case 0x22: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= func->num_locals)
                    return wasm__validator_error(&v, at, "local index %u out of range", (unsigned)index);
                err = wasm__validator_pop_expect_typed(&v, at,
                                                       func->locals[index],
                                                       func->local_reftypes ? &func->local_reftypes[index] : NULL);
                if (err != WASM_OK) return err;
                if (v.local_init_words != 0)
                    wasm__validator_local_init_set(wasm__validator_frame(&v)->local_init, index);
                err = wasm__validator_push_typed(&v, at,
                                                 func->locals[index],
                                                 func->local_reftypes ? &func->local_reftypes[index] : NULL);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x23: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= mod->num_globals)
                    return wasm__validator_error(&v, at, "global index %u out of range", (unsigned)index);
                err = wasm__validator_push_typed(&v, at,
                                                 mod->globals[index].type,
                                                 &mod->globals[index].ref_type);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x24: {
                uint32_t index = wasm__read_leb128_u32(&v.r);
                if (index >= mod->num_globals)
                    return wasm__validator_error(&v, at, "global index %u out of range", (unsigned)index);
                if (!mod->globals[index].is_mutable)
                    return wasm__validator_error(&v, at, "global %u is immutable", (unsigned)index);
                err = wasm__validator_pop_expect_typed(&v, at,
                                                       mod->globals[index].type,
                                                       &mod->globals[index].ref_type);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x25: {
                uint32_t table_idx = wasm__read_leb128_u32(&v.r);
                wasm_table_t* table;
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                if (table_idx >= mod->num_tables)
                    return wasm__validator_error(&v, at, "unknown table %u", (unsigned)table_idx);
                table = &mod->tables[table_idx];
                err = wasm__validator_pop_expect(&v, at, wasm__table_index_type(table));
                if (err != WASM_OK) return err;
                err = wasm__validator_push_typed(&v, at,
                                                 table->reftype,
                                                 &table->reftype_info);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x26: {
                uint32_t table_idx = wasm__read_leb128_u32(&v.r);
                wasm_table_t* table;
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                if (table_idx >= mod->num_tables)
                    return wasm__validator_error(&v, at, "unknown table %u", (unsigned)table_idx);
                table = &mod->tables[table_idx];
                err = wasm__validator_pop_expect_typed(&v, at,
                                                       table->reftype,
                                                       &table->reftype_info);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, wasm__table_index_type(table));
                if (err != WASM_OK) return err;
                break;
            }
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
                wasm_valtype_t result_type = WASM_TYPE_I32;
                uint32_t max_align = 0;
                if (mod->num_memories == 0)
                    return wasm__validator_error(&v, at, "unknown memory 0");
                switch (op) {
                    case 0x28:
                        result_type = WASM_TYPE_I32;
                        max_align = 2;
                        break;
                    case 0x29:
                        result_type = WASM_TYPE_I64;
                        max_align = 3;
                        break;
                    case 0x2A:
                        result_type = WASM_TYPE_F32;
                        max_align = 2;
                        break;
                    case 0x2B:
                        result_type = WASM_TYPE_F64;
                        max_align = 3;
                        break;
                    case 0x2C:
                    case 0x2D:
                        max_align = 0;
                        break;
                    case 0x2E:
                    case 0x2F:
                        max_align = 1;
                        break;
                    case 0x30:
                    case 0x31:
                        result_type = WASM_TYPE_I64;
                        max_align = 0;
                        break;
                    case 0x32:
                    case 0x33:
                        result_type = WASM_TYPE_I64;
                        max_align = 1;
                        break;
                    case 0x34:
                    case 0x35:
                        result_type = WASM_TYPE_I64;
                        max_align = 2;
                        break;
                }
                err = wasm__validator_read_memarg(&v, at, max_align, &memarg);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_memory_index(&v, at, memarg.memory_index);
                if (err != WASM_OK) return err;
                err = wasm__validator_push(&v, at, result_type);
                if (err != WASM_OK) return err;
                break;
            }
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
                wasm_valtype_t value_type = WASM_TYPE_I32;
                uint32_t max_align = 0;
                if (mod->num_memories == 0)
                    return wasm__validator_error(&v, at, "unknown memory 0");
                switch (op) {
                    case 0x36:
                        value_type = WASM_TYPE_I32;
                        max_align = 2;
                        break;
                    case 0x37:
                        value_type = WASM_TYPE_I64;
                        max_align = 3;
                        break;
                    case 0x38:
                        value_type = WASM_TYPE_F32;
                        max_align = 2;
                        break;
                    case 0x39:
                        value_type = WASM_TYPE_F64;
                        max_align = 3;
                        break;
                    case 0x3A:
                        value_type = WASM_TYPE_I32;
                        max_align = 0;
                        break;
                    case 0x3B:
                        value_type = WASM_TYPE_I32;
                        max_align = 1;
                        break;
                    case 0x3C:
                        value_type = WASM_TYPE_I64;
                        max_align = 0;
                        break;
                    case 0x3D:
                        value_type = WASM_TYPE_I64;
                        max_align = 1;
                        break;
                    case 0x3E:
                        value_type = WASM_TYPE_I64;
                        max_align = 2;
                        break;
                }
                err = wasm__validator_read_memarg(&v, at, max_align, &memarg);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, value_type);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_memory_index(&v, at, memarg.memory_index);
                if (err != WASM_OK) return err;
                break;
            }
            case 0x3F: {
                uint32_t mem_idx = 0;
                if (mod->required_features & WASM_FEATURE_MULTI_MEMORY) {
                    mem_idx = wasm__read_leb128_u32(&v.r);
                } else {
                    uint8_t reserved = wasm__read_u8(&v.r);
                    if (v.r.malformed || reserved != 0)
                        return wasm__validator_error(&v, at, "%s", "zero byte expected");
                }
                if (mod->num_memories == 0)
                    return wasm__validator_error(&v, at, "unknown memory %u", (unsigned)mem_idx);
                if (mem_idx != 0) {
                    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_MULTI_MEMORY);
                    if (err != WASM_OK) return err;
                }
                if (mem_idx >= mod->num_memories)
                    return wasm__validator_error(&v, at, "unknown memory %u", (unsigned)mem_idx);
                err = wasm__validator_push(&v, at, wasm__memory_index_type(&mod->memories[mem_idx]));
                if (err != WASM_OK) return err;
                break;
            }
            case 0x40: {
                uint32_t mem_idx = 0;
                if (mod->required_features & WASM_FEATURE_MULTI_MEMORY) {
                    mem_idx = wasm__read_leb128_u32(&v.r);
                } else {
                    uint8_t reserved = wasm__read_u8(&v.r);
                    if (v.r.malformed || reserved != 0)
                        return wasm__validator_error(&v, at, "%s", "zero byte expected");
                }
                if (mod->num_memories == 0)
                    return wasm__validator_error(&v, at, "unknown memory %u", (unsigned)mem_idx);
                if (mem_idx != 0) {
                    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_MULTI_MEMORY);
                    if (err != WASM_OK) return err;
                }
                if (mem_idx >= mod->num_memories)
                    return wasm__validator_error(&v, at, "unknown memory %u", (unsigned)mem_idx);
                err = wasm__validator_pop_memory_index(&v, at, mem_idx);
                if (err != WASM_OK) return err;
                err = wasm__validator_push(&v, at, wasm__memory_index_type(&mod->memories[mem_idx]));
                if (err != WASM_OK) return err;
                break;
            }
            case 0x41:
                (void)wasm__read_leb128_i32(&v.r);
                err = wasm__validator_push(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            case 0x42:
                (void)wasm__read_leb128_i64(&v.r);
                err = wasm__validator_push(&v, at, WASM_TYPE_I64);
                if (err != WASM_OK) return err;
                break;
            case 0x43:
                (void)wasm__read_f32(&v.r);
                err = wasm__validator_push(&v, at, WASM_TYPE_F32);
                if (err != WASM_OK) return err;
                break;
            case 0x44:
                (void)wasm__read_f64(&v.r);
                err = wasm__validator_push(&v, at, WASM_TYPE_F64);
                if (err != WASM_OK) return err;
                break;
            case 0x45:
            case 0x46:
            case 0x47:
            case 0x48:
            case 0x49:
            case 0x4A:
            case 0x4B:
            case 0x4C:
            case 0x4D:
            case 0x4E:
            case 0x4F:
            case 0x50:
            case 0x51:
            case 0x52:
            case 0x53:
            case 0x54:
            case 0x55:
            case 0x56:
            case 0x57:
            case 0x58:
            case 0x59:
            case 0x5A:
            case 0x5B:
            case 0x5C:
            case 0x5D:
            case 0x5E:
            case 0x5F:
            case 0x60:
            case 0x61:
            case 0x62:
            case 0x63:
            case 0x64:
            case 0x65:
            case 0x66:
            case 0x67:
            case 0x68:
            case 0x69:
            case 0x6A:
            case 0x6B:
            case 0x6C:
            case 0x6D:
            case 0x6E:
            case 0x6F:
            case 0x70:
            case 0x71:
            case 0x72:
            case 0x73:
            case 0x74:
            case 0x75:
            case 0x76:
            case 0x77:
            case 0x78:
            case 0x79:
            case 0x7A:
            case 0x7B:
            case 0x7C:
            case 0x7D:
            case 0x7E:
            case 0x7F:
            case 0x80:
            case 0x81:
            case 0x82:
            case 0x83:
            case 0x84:
            case 0x85:
            case 0x86:
            case 0x87:
            case 0x88:
            case 0x89:
            case 0x8A:
            case 0x8B:
            case 0x8C:
            case 0x8D:
            case 0x8E:
            case 0x8F:
            case 0x90:
            case 0x91:
            case 0x92:
            case 0x93:
            case 0x94:
            case 0x95:
            case 0x96:
            case 0x97:
            case 0x98:
            case 0x99:
            case 0x9A:
            case 0x9B:
            case 0x9C:
            case 0x9D:
            case 0x9E:
            case 0x9F:
            case 0xA0:
            case 0xA1:
            case 0xA2:
            case 0xA3:
            case 0xA4:
            case 0xA5:
            case 0xA6:
            case 0xA7:
            case 0xA8:
            case 0xA9:
            case 0xAA:
            case 0xAB:
            case 0xAC:
            case 0xAD:
            case 0xAE:
            case 0xAF:
            case 0xB0:
            case 0xB1:
            case 0xB2:
            case 0xB3:
            case 0xB4:
            case 0xB5:
            case 0xB6:
            case 0xB7:
            case 0xB8:
            case 0xB9:
            case 0xBA:
            case 0xBB:
            case 0xBC:
            case 0xBD:
            case 0xBE:
            case 0xBF:
            case 0xC0:
            case 0xC1:
            case 0xC2:
            case 0xC3:
            case 0xC4:
                err = wasm__validator_validate_numeric(&v, at, op);
                if (err != WASM_OK) return err;
                break;
            case 0xD0: {
                wasm_valtype_t reftype;
                wasm_reftype_t type_info = { 0 };
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                err = wasm__read_heaptype(mod, &v.r, &type_info);
                if (err != WASM_OK) return wasm__validator_error(&v, at, "invalid heaptype");
                type_info.nullable = 1;
                reftype = type_info.type ? type_info.type : WASM_TYPE_ANYREF;
                err = wasm__validator_push_typed(&v, at, reftype, &type_info);
                if (err != WASM_OK) return err;
                break;
            }
            case 0xD1: {
                wasm_valtype_t ref_type;
                err = wasm__validator_pop_any(&v, at, &ref_type);
                if (err != WASM_OK) return err;
                if (ref_type != WASM__TYPE_BOT && !wasm__is_ref_type(ref_type))
                    return wasm__validator_error(&v, at, "ref.is_null requires a reference operand");
                err = wasm__validator_push(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            }
            case 0xD2: {
                uint32_t func_ref = wasm__read_leb128_u32(&v.r);
                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                if (func_ref >= mod->num_funcs)
                    return wasm__validator_error(&v, at, "ref.func index %u out of range", (unsigned)func_ref);
                if (!mod->funcs[func_ref].is_declared_ref)
                    return wasm__validator_error(&v, at, "%s", "undeclared function reference");
                {
                    wasm_reftype_t ref_type;
                    wasm__clear_reftype(&ref_type);
                    ref_type.type = WASM_TYPE_FUNCREF;
                    ref_type.has_type_index = 1;
                    ref_type.type_index = mod->funcs[func_ref].type_idx;
                    ref_type.nullable = 0;
                    err = wasm__validator_push_typed(&v, at, WASM_TYPE_FUNCREF, &ref_type);
                }
                if (err != WASM_OK) return err;
                break;
            }
            case 0xD3:
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_EQREF);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(&v, at, WASM_TYPE_EQREF);
                if (err != WASM_OK) return err;
                err = wasm__validator_push(&v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                break;
            case 0xD4: {
                wasm_valtype_t actual_type;
                wasm_reftype_t actual_ref;
                wasm_reftype_t pushed_ref;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_any_typed(&v, at, &actual_type, &actual_ref);
                if (err != WASM_OK) return err;
                if (actual_type == WASM__TYPE_BOT)
                    err = wasm__validator_push(&v, at, WASM__TYPE_BOT);
                else if (!wasm__validator_get_effective_reftype(actual_type, &actual_ref, &pushed_ref))
                    err = wasm__validator_error(&v, at, "instruction requires a reference operand");
                else {
                    pushed_ref.nullable = 0;
                    err = wasm__validator_push_typed(&v, at, actual_type, &pushed_ref);
                }
                if (err != WASM_OK) return err;
                break;
            }
            case 0xD5:
            case 0xD6: {
                uint32_t depth = wasm__read_leb128_u32(&v.r);
                const wasm__val_frame_t* target_frame;
                const wasm_valtype_t* branch_types;
                const wasm_reftype_t* branch_reftypes;
                uint32_t branch_count;
                uint32_t branch_arg_count;
                wasm_valtype_t actual_type;
                wasm_reftype_t source_ref;
                wasm_reftype_t target_ref;
                wasm_valtype_t branch_type;
                wasm_reftype_t branch_ref;

                err = wasm__validator_require_feature(&v, at, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) return err;
                if (depth >= v.fp)
                    return wasm__validator_error(&v, at, "branch depth %u out of range", (unsigned)depth);
                err = wasm__validator_pop_direct_non_null_branch_operand(&v, at,
                                                                         &actual_type,
                                                                         &source_ref,
                                                                         &target_ref);
                if (err != WASM_OK) return err;

                target_frame = &v.frames[v.fp - 1 - depth];
                wasm__validator_branch_signature(target_frame, &branch_types, &branch_reftypes, &branch_count);
                if (op == 0xD5) {
                    err = wasm__validator_pop_and_push_types(&v, at,
                                                             branch_types,
                                                             branch_reftypes,
                                                             branch_count);
                    if (err != WASM_OK) return err;
                    if (actual_type == WASM__TYPE_BOT)
                        err = wasm__validator_push(&v, at, WASM__TYPE_BOT);
                    else
                        err = wasm__validator_push_typed(&v, at, target_ref.type, &target_ref);
                    if (err != WASM_OK) return err;
                } else {
                    branch_arg_count = branch_count > 0 ? branch_count - 1 : 0;
                    err = wasm__validator_pop_and_push_types(&v, at,
                                                             branch_types,
                                                             branch_reftypes,
                                                             branch_arg_count);
                    if (err != WASM_OK) return err;
                    if (actual_type != WASM__TYPE_BOT) {
                        branch_type = target_ref.type;
                        branch_ref = target_ref;
                        if (branch_count > 0 &&
                            !wasm__validator_type_matches(&v,
                                                          branch_type,
                                                          &branch_ref,
                                                          branch_types[branch_count - 1],
                                                          branch_reftypes ? &branch_reftypes[branch_count - 1] : NULL)) {
                            return wasm__validator_error(&v, at, "branch target label type mismatch");
                        }
                    }
                }
                break;
            }
            case 0xFB:
                err = wasm__validator_validate_gc(&v, at);
                if (err != WASM_OK) return err;
                break;
            case 0xFC:
                err = wasm__validator_validate_prefixed(&v, at);
                if (err != WASM_OK) return err;
                break;
            case 0xFD:
                err = wasm__validator_validate_simd(&v, at);
                if (err != WASM_OK) return err;
                break;
            default:
                err = wasm__validator_error(&v, at, "unknown opcode 0x%02X", op);
                goto done;
        }

        if (v.r.malformed) {
            err = wasm__validator_error(&v, at, "malformed immediate");
            goto done;
        }
    }

    if (v.fp != 0) {
        err = wasm__validator_error(&v, v.r.end, "function body ended before all blocks closed");
        goto done;
    }
    if (v.sp != ft->num_results) {
        err = wasm__validator_error(&v, v.r.end, "function leaves %u stack values, expected %u",
                                    (unsigned)v.sp,
                                    (unsigned)ft->num_results);
        goto done;
    }
    func->max_label_depth = v.max_fp;
    err = WASM_OK;

done:
    wasm__validator_destroy(&v);
    return err;
}

WASM_PRIVATE wasm_error_t wasm__validate_code(wasm_module_t* mod) {
    uint32_t i;

    if (mod->rt && mod->rt->lazy_validation) return WASM_OK;

    for (i = mod->num_func_imports; i < mod->num_funcs; i++) {
        wasm_error_t err = wasm__validate_function(mod, i);
        if (err != WASM_OK) return err;
    }
    if (mod->uses_data_count_section && !mod->has_data_count_section) {
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                          "data count section is required when memory.init or data.drop is used");
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__prepare_function(wasm_module_t* mod, uint32_t func_idx) {
    wasm_func_t* func;
    wasm_error_t err;

    if (!mod || func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import) return WASM_OK;

    if (func->max_label_depth == 0) {
        err = wasm__validate_function(mod, func_idx);
        if (err != WASM_OK) return err;
    }

    if (mod->uses_data_count_section && !mod->has_data_count_section) {
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                          "data count section is required when memory.init or data.drop is used");
    }

    if (func->control_targets) return WASM_OK;

    err = wasm__build_control_targets(func);
    if (err == WASM_OK) return WASM_OK;

    if (mod->rt && mod->rt->last_error == WASM_OK) {
        if (err == WASM_ERR_OOM)
            WASM__SET_ERR(mod->rt, err, "%s", "failed to allocate control targets");
        else
            (void)wasm__set_validation_error(mod->rt, func_idx, 0,
                                             "failed to build control targets");
    }

    return err;
}

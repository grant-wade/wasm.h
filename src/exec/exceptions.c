#include "exceptions.h"
#include <stdio.h>
#include "calls.h"
#include "control.h"
#include "../instructions.h"
#include "../internal/alloc.h"
#include "../module.h"
#include "../reader.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include <string.h>

static wasm_error_t wasm__copy_exception_to_label(wasm__label_t* label,
                                                  const wasm__exception_state_t* ex) {
    wasm_value_t* copy = NULL;

    wasm__clear_label(label);
    if (ex->num_values > 0) {
        copy = (wasm_value_t*)wasm__malloc_array(ex->num_values, sizeof(wasm_value_t));
        if (!copy) return WASM_ERR_OOM;
        memcpy(copy, ex->values, ex->num_values * sizeof(wasm_value_t));
    }

    label->caught_tag = ex->tag_index;
    label->caught_tag_identity = ex->tag_identity;
    label->caught_values = copy;
    label->caught_count = ex->num_values;
    label->active_catch = 1;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__report_uncaught_exception(wasm_runtime_t* rt) {
    WASM__SET_ERR(rt, WASM_ERR_TRAP, "uncaught exception tag %u", (unsigned)rt->pending_exception.tag_index);
    wasm__free_exception_state(&rt->pending_exception);
    return WASM_ERR_TRAP;
}

WASM_PRIVATE int wasm__find_rethrow_label(const wasm__label_t* labels,
                                    uint32_t lsp,
                                    uint32_t depth,
                                    uint32_t* out_index) {
    uint32_t i;

    for (i = lsp; i > 0; i--) {
        const wasm__label_t* label = &labels[i - 1];

        if (label->opcode == 0x06 && label->active_catch) {
            if (depth == 0) {
                *out_index = i - 1;
                return 1;
            }
            depth--;
        }
    }

    return 0;
}

WASM_PRIVATE wasm_error_t wasm__handle_exception_in_frame(wasm_module_t* mod,
                                                    wasm__label_t* labels,
                                                    uint32_t* label_sp,
                                                    wasm__reader_t* r,
                                                    uint32_t sp_base) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err;
    uint32_t idx = *label_sp;

    while (idx > 0) {
        wasm__label_t* label = &labels[idx - 1];
        int delegated = 0;

        if (label->opcode == 0x06 && label->handler_ptr) {
            const uint8_t* clause_ptr = label->handler_ptr;

            while (clause_ptr && clause_ptr < label->continuation) {
                wasm__try_clause_t clause;

                if (wasm__read_try_clause(clause_ptr, label->continuation, &clause) != WASM_OK)
                    return WASM_ERR_MALFORMED;

                if (clause.opcode == 0x07 && clause.immediate < mod->num_tags &&
                    mod->tags[clause.immediate].identity == rt->pending_exception.tag_identity) {
                    uint32_t i;

                    while (*label_sp > idx) {
                        wasm__clear_label(&labels[*label_sp - 1]);
                        rt->sp = labels[*label_sp - 1].sp_base;
                        (*label_sp)--;
                    }
                    rt->sp = label->sp_base;
                    err = wasm__copy_exception_to_label(label, &rt->pending_exception);
                    if (err != WASM_OK) return err;
                    if (rt->sp + label->caught_count > rt->max_stack) return WASM_ERR_STACK_OVERFLOW;
                    for (i = 0; i < label->caught_count; i++) rt->stack[rt->sp++] = label->caught_values[i];
                    wasm__free_exception_state(&rt->pending_exception);
                    *label_sp = idx;
                    r->ptr = clause.body_start;
                    return WASM_OK;
                }

                if (clause.opcode == 0x19) {
                    while (*label_sp > idx) {
                        wasm__clear_label(&labels[*label_sp - 1]);
                        rt->sp = labels[*label_sp - 1].sp_base;
                        (*label_sp)--;
                    }
                    rt->sp = label->sp_base;
                    err = wasm__copy_exception_to_label(label, &rt->pending_exception);
                    if (err != WASM_OK) return err;
                    wasm__free_exception_state(&rt->pending_exception);
                    *label_sp = idx;
                    r->ptr = clause.body_start;
                    return WASM_OK;
                }

                if (clause.opcode == 0x18) {
                    uint32_t extra = clause.immediate;

                    wasm__clear_label(label);
                    rt->sp = label->sp_base;
                    idx--;
                    while (extra > 0 && idx > 0) {
                        wasm__clear_label(&labels[idx - 1]);
                        rt->sp = labels[idx - 1].sp_base;
                        idx--;
                        extra--;
                    }
                    delegated = 1;
                    break;
                }

                if (clause.next_clause >= label->continuation || *clause.next_clause == 0x0B) break;
                clause_ptr = clause.next_clause;
            }
        } else if (label->opcode == 0x1F && label->handler_ptr) {
            wasm__reader_t clause_reader;
            uint32_t clause_count;
            uint32_t clause_index;

            clause_reader.ptr = label->handler_ptr;
            clause_reader.end = label->continuation;
            clause_reader.malformed = 0;
            wasm__skip_blocktype(&clause_reader);
            clause_count = wasm__read_leb128_u32(&clause_reader);
            if (clause_reader.malformed) return WASM_ERR_MALFORMED;

            for (clause_index = 0; clause_index < clause_count; clause_index++) {
                wasm__try_table_clause_t clause;
                int matches = 0;
                uint32_t branch_depth;
                wasm__label_t* target;
                wasm_functype_t* tag_type = NULL;
                uint32_t i;

                if (wasm__read_try_table_clause(&clause_reader, &clause) != WASM_OK)
                    return WASM_ERR_MALFORMED;

                switch (clause.kind) {
                    case 0x00:
                        matches = (clause.tag_index < mod->num_tags &&
                                   mod->tags[clause.tag_index].identity == rt->pending_exception.tag_identity);
                        if (matches) {
                            if (clause.tag_index >= mod->num_tags) return WASM_ERR_MALFORMED;
                            tag_type = wasm__module_functype(mod, mod->tags[clause.tag_index].type_idx);
                            if (!tag_type) return WASM_ERR_MALFORMED;
                        }
                        break;
                    case 0x02:
                        matches = 1;
                        break;
                    case 0x01:
                        matches = (clause.tag_index < mod->num_tags &&
                                   mod->tags[clause.tag_index].identity == rt->pending_exception.tag_identity);
                        if (matches) {
                            if (clause.tag_index >= mod->num_tags) return WASM_ERR_MALFORMED;
                            tag_type = wasm__module_functype(mod, mod->tags[clause.tag_index].type_idx);
                            if (!tag_type) return WASM_ERR_MALFORMED;
                        }
                        break;
                    case 0x03:
                        matches = 1;
                        break;
                    default:
                        return WASM_ERR_MALFORMED;
                }

                if (!matches) continue;
                if (clause.label_depth >= idx - 1) return WASM_ERR_MALFORMED;

                while (*label_sp > idx) {
                    wasm__clear_label(&labels[*label_sp - 1]);
                    rt->sp = labels[*label_sp - 1].sp_base;
                    (*label_sp)--;
                }

                rt->sp = label->sp_base;
                if (clause.kind == 0x01) {
                    wasm_value_t exnref_value;

                    if (!tag_type) return WASM_ERR_MALFORMED;
                    if (rt->sp + tag_type->num_params + 1u > rt->max_stack) return WASM_ERR_STACK_OVERFLOW;
                    for (i = 0; i < tag_type->num_params; i++)
                        rt->stack[rt->sp++] = rt->pending_exception.values[i];
                    wasm__gc_collect_handles_if_needed(rt);
                    err = wasm__new_exception_ref(rt, &rt->pending_exception, &exnref_value);
                    if (err != WASM_OK) return err;
                    rt->stack[rt->sp++] = exnref_value;
                } else if (clause.kind == 0x03) {
                    wasm_value_t exnref_value;

                    wasm__gc_collect_handles_if_needed(rt);
                    err = wasm__new_exception_ref(rt, &rt->pending_exception, &exnref_value);
                    if (err != WASM_OK) return err;
                    if (rt->sp >= rt->max_stack) return WASM_ERR_STACK_OVERFLOW;
                    rt->stack[rt->sp++] = exnref_value;
                } else if (tag_type) {
                    if (rt->sp + tag_type->num_params > rt->max_stack) return WASM_ERR_STACK_OVERFLOW;
                    for (i = 0; i < tag_type->num_params; i++)
                        rt->stack[rt->sp++] = rt->pending_exception.values[i];
                }

                branch_depth = clause.label_depth + 1;
                target = &labels[idx - 1 - branch_depth];
                wasm__free_exception_state(&rt->pending_exception);
                err = wasm__do_branch(rt, labels, target, r, label_sp, branch_depth);
                if (err != WASM_OK) return err;
                return WASM_OK;
            }
        }

        if (delegated) continue;

        wasm__clear_label(label);
        rt->sp = label->sp_base;
        idx--;
    }

    *label_sp = 0;
    rt->sp = sp_base;
    return (wasm_error_t)-1;
}

WASM_PRIVATE wasm_error_t wasm__do_branch(wasm_runtime_t* rt,
                                    wasm__label_t* labels,
                                    wasm__label_t* target,
                                    wasm__reader_t* r,
                                    uint32_t* label_sp,
                                    uint32_t depth_l) {
    uint32_t branch_arity = (target->opcode == 0x03) ? target->param_arity : target->arity;
    uint32_t pop_count = (target->opcode == 0x03) ? depth_l : depth_l + 1;
    uint32_t i;

    if (target->opcode == 0x03 && wasm__runtime_take_interrupt(rt)) return WASM_ERR_INTERRUPTED;

    if (rt->sp - branch_arity != target->sp_base && branch_arity > 0)
        memmove(&rt->stack[target->sp_base], &rt->stack[rt->sp - branch_arity], branch_arity * sizeof(wasm_value_t));
    rt->sp = target->sp_base + branch_arity;
    r->ptr = target->continuation;

    for (i = 0; i < pop_count; i++) wasm__clear_label(&labels[*label_sp - 1 - i]);
    *label_sp -= pop_count;

    return WASM_OK;
}


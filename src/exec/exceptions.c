#include "exceptions.h"
#include <stdio.h>
#include "calls.h"
#include "translate.h"
#include "../internal/alloc.h"
#include "../module.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include <string.h>

static wasm_error_t wasm__copy_exception_to_label(const wasm_allocator_t* a,
                                                  wasm__label_t* label,
                                                  const wasm__exception_state_t* ex) {
    wasm_value_t* copy = NULL;

    wasm__clear_label(label);
    if (ex->num_values > 0) {
        copy = (wasm_value_t*)wasm__malloc_array(a, ex->num_values, sizeof(wasm_value_t));
        if (!copy) return WASM_ERR_OOM;
        memcpy(copy, ex->values, ex->num_values * sizeof(wasm_value_t));
    }

    label->caught_tag = ex->tag_index;
    label->caught_tag_identity = ex->tag_identity;
    label->caught_values = copy;
    label->caught_allocator = a;
    label->caught_count = ex->num_values;
    label->active_catch = 1;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__report_uncaught_exception(wasm_runtime_t* rt) {
    WASM__SET_ERR(rt, WASM_ERR_TRAP, "uncaught exception tag %u", (unsigned)rt->pending_exception.tag_index);
    wasm__free_exception_state(&rt->allocator, &rt->pending_exception);
    return WASM_ERR_TRAP;
}

static wasm_error_t wasm__branch_frame(wasm__call_frame_t* cf, const uint32_t* desc) {
    wasm_runtime_t* rt = cf->module->rt;
    uint32_t arity = desc[1];
    uint32_t pops = desc[3] >> WASM__BRANCH_POPS_SHIFT;
    uint32_t dst = cf->sp_base + desc[2];

    if (desc[3] & WASM__BRANCH_RETURN) {
        cf->pc = NULL;
        return WASM_OK;
    }
    if ((desc[3] & WASM__BRANCH_LOOP) && wasm__runtime_take_interrupt(rt)) return WASM_ERR_INTERRUPTED;
    if (arity > 0 && rt->sp - arity != dst)
        memmove(&rt->stack[dst], &rt->stack[rt->sp - arity], arity * sizeof(wasm_value_t));
    rt->sp = dst + arity;
    while (pops-- > 0 && cf->lsp > 0) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }
    cf->pc = cf->func->ops + desc[0];
    return WASM_OK;
}

static void wasm__unwind_labels(wasm__call_frame_t* cf, uint32_t keep) {
    while (cf->lsp > keep) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }
}

static int wasm__tag_matches(const wasm_module_t* mod, uint32_t tag_index, const wasm__exception_state_t* ex) {
    return tag_index < mod->num_tags && mod->tags[tag_index].identity == ex->tag_identity;
}

/* A legacy try running one of its catch clauses does not handle exceptions. */
static int wasm__catch_legacy(wasm__call_frame_t* cf, uint32_t idx, int* delegated, uint32_t* next_idx,
                              wasm_error_t* out_err) {
    wasm_module_t* mod = cf->module;
    wasm_runtime_t* rt = mod->rt;
    wasm__label_t* label = &cf->labels[idx - 1];
    const uint32_t* clause = label->handler + 1;
    uint32_t count = label->handler[0];
    uint32_t i;

    for (i = 0; i < count; i++, clause += 3) {
        uint32_t j;

        if (clause[0] == WASM__CLAUSE_DELEGATE) {
            uint32_t skip = clause[1];

            wasm__clear_label(label);
            idx--;
            while (skip > 0 && idx > 0) {
                wasm__clear_label(&cf->labels[idx - 1]);
                idx--;
                skip--;
            }
            *delegated = 1;
            *next_idx = idx;
            return 0;
        }
        if (clause[0] == WASM__CLAUSE_CATCH && !wasm__tag_matches(mod, clause[1], &rt->pending_exception)) continue;

        wasm__unwind_labels(cf, idx);
        rt->sp = label->sp_base;
        *out_err = wasm__copy_exception_to_label(&rt->allocator, label, &rt->pending_exception);
        if (*out_err != WASM_OK) return 1;
        if (clause[0] == WASM__CLAUSE_CATCH) {
            if (rt->sp + label->caught_count > rt->max_stack) {
                *out_err = WASM_ERR_STACK_OVERFLOW;
                return 1;
            }
            for (j = 0; j < label->caught_count; j++) rt->stack[rt->sp++] = label->caught_values[j];
        }
        wasm__free_exception_state(&rt->allocator, &rt->pending_exception);
        cf->pc = cf->func->ops + clause[2];
        *out_err = WASM_OK;
        return 1;
    }
    return 0;
}

static int wasm__catch_try_table(wasm__call_frame_t* cf, uint32_t idx, wasm_error_t* out_err) {
    wasm_module_t* mod = cf->module;
    wasm_runtime_t* rt = mod->rt;
    wasm__label_t* label = &cf->labels[idx - 1];
    const uint32_t* clause = label->handler + 1;
    uint32_t count = label->handler[0];
    uint32_t i;

    for (i = 0; i < count; i++, clause += 6) {
        wasm_functype_t* tag_type = NULL;
        wasm_value_t exnref_value;
        uint32_t j;

        if (clause[0] == 0x00 || clause[0] == 0x01) {
            if (!wasm__tag_matches(mod, clause[1], &rt->pending_exception)) continue;
            tag_type = wasm__module_functype(mod, mod->tags[clause[1]].type_idx);
            if (!tag_type) {
                *out_err = WASM_ERR_MALFORMED;
                return 1;
            }
        } else if (clause[0] != 0x02 && clause[0] != 0x03) {
            *out_err = WASM_ERR_MALFORMED;
            return 1;
        }

        wasm__unwind_labels(cf, idx - 1);
        rt->sp = label->sp_base;
        if (tag_type) {
            if (rt->sp + tag_type->num_params + 1u > rt->max_stack) {
                *out_err = WASM_ERR_STACK_OVERFLOW;
                return 1;
            }
            for (j = 0; j < tag_type->num_params; j++) rt->stack[rt->sp++] = rt->pending_exception.values[j];
        }
        if (clause[0] == 0x01 || clause[0] == 0x03) {
            wasm__gc_collect_handles_if_needed(rt);
            *out_err = wasm__new_exception_ref(rt, &rt->pending_exception, &exnref_value);
            if (*out_err != WASM_OK) return 1;
            if (rt->sp >= rt->max_stack) {
                *out_err = WASM_ERR_STACK_OVERFLOW;
                return 1;
            }
            rt->stack[rt->sp++] = exnref_value;
        }
        wasm__free_exception_state(&rt->allocator, &rt->pending_exception);
        *out_err = wasm__branch_frame(cf, clause + 2);
        return 1;
    }
    return 0;
}

WASM_PRIVATE wasm_error_t wasm__handle_exception_in_frame(wasm__call_frame_t* cf) {
    wasm_runtime_t* rt = cf->module->rt;
    wasm_error_t err = WASM_OK;
    uint32_t idx = cf->lsp;

    while (idx > 0) {
        wasm__label_t* label = &cf->labels[idx - 1];

        if (label->opcode == 0x06 && !label->active_catch) {
            int delegated = 0;
            uint32_t next_idx = idx;

            if (wasm__catch_legacy(cf, idx, &delegated, &next_idx, &err)) return err;
            if (delegated) {
                idx = next_idx;
                continue;
            }
        } else if (label->opcode == 0x1F) {
            if (wasm__catch_try_table(cf, idx, &err)) return err;
        }
        wasm__clear_label(label);
        rt->sp = label->sp_base;
        idx--;
    }

    cf->lsp = 0;
    rt->sp = cf->sp_base;
    return WASM__ERR_EXCEPTION;
}

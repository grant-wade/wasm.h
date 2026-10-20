#include "calls.h"
#include "../module.h"
#include <stdio.h>
#include "exceptions.h"
#include "frames.h"
#include "slots.h"
#include "stack.h"
#include "../internal/alloc.h"
#include "../link.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include "../types/cross_module.h"
#include "../types/references.h"
#include "../validate/function.h"
#include "../values.h"
#include <string.h>

WASM_PRIVATE void wasm__clear_label(wasm_runtime_t* rt, wasm__label_t* label) {
    if (label->caught) {
        wasm__exn_release(rt, label->caught);
        label->caught = NULL;
    }
    label->active_catch = 0;
}

WASM_PRIVATE void wasm__clear_call_frame(wasm_runtime_t* rt, wasm__call_frame_t* cf) {
    while (cf->lsp > 0) {
        wasm__clear_label(rt, &cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }
}

WASM_PRIVATE wasm_error_t wasm__push_call_frame(wasm_module_t* mod,
                                          uint32_t func_idx,
                                          int with_args,
                                          wasm_value_t* results_dst) {
    wasm_runtime_t* rt = mod->rt;
    wasm__exec_stack_t* exec = rt->exec;
    wasm_error_t err;
    wasm_func_t* func;
    wasm__call_frame_t* cf;
    wasm__slot_t* locals;
    wasm__arena_mark_t arena_saved;
    uint32_t i;

    if (exec->call_frame_sp >= exec->max_call_frames) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "call frame depth exceeded limit %u",
                      (unsigned)exec->max_call_frames);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }
    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import) return WASM_ERR_MALFORMED;
    if (!func->ops) {
        err = wasm__prepare_function(mod, func_idx);
        if (err != WASM_OK) return err;
    }
    if (!wasm__module_functype(mod, func->type_idx)) return WASM_ERR_MALFORMED;
    if (func->max_stack > exec->max_stack - exec->sp) {
        WASM__SET_ERR(rt, WASM_ERR_STACK_OVERFLOW, "operand stack limit %u exceeded", (unsigned)exec->max_stack);
        return WASM_ERR_STACK_OVERFLOW;
    }
    if (func->max_label_depth > rt->max_labels) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "function label depth %u exceeds limit %u",
                      (unsigned)func->max_label_depth,
                      (unsigned)rt->max_labels);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }
    arena_saved = wasm__arena_mark(exec);
    locals = NULL;
    if (func->frame_bytes > 0) {
        locals = (wasm__slot_t*)wasm__arena_alloc(rt, exec, func->frame_bytes);
        if (!locals) {
            wasm__arena_reset(rt, exec, arena_saved);
            WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "out of memory for call frame");
            return WASM_ERR_OOM;
        }
    }
    i = 0;
    if (with_args) {
        for (; i < func->param_slots; i++) locals[i] = exec->stack[exec->sp + i];
    }
    if (i < func->num_local_slots) memset(locals + i, 0, (func->num_local_slots - i) * sizeof(*locals));
    err = wasm__exec_stack_reserve(rt, exec, func->max_stack);
    if (err == WASM_OK) err = wasm__exec_stack_reserve_frame(rt, exec);
    if (err != WASM_OK) {
        wasm__arena_reset(rt, exec, arena_saved);
        return err;
    }

    cf = &exec->call_frames[exec->call_frame_sp++];
    cf->module = mod;
    cf->func_idx = func_idx;
    cf->sp_base = exec->sp;
    cf->locals = locals;
    cf->labels = func->max_handlers > 0 ? (wasm__label_t*)(locals + func->num_local_slots) : NULL;
    cf->lsp = 0;
    cf->pc = func->ops;
    cf->op_at = func->ops;
    cf->func = func;
    cf->results_dst = results_dst;
    cf->arena_saved = arena_saved;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__push_root_call_frame(wasm_module_t* mod,
                                               uint32_t func_idx,
                                               const wasm_value_t* args,
                                               uint32_t num_args,
                                               wasm_value_t* results_dst) {
    wasm_runtime_t* rt = mod->rt;
    wasm__call_frame_t* cf;
    const wasm_functype_t* ft;
    wasm_error_t err = wasm__push_call_frame(mod, func_idx, 0, results_dst);
    uint32_t i;

    if (err != WASM_OK) return err;
    cf = &rt->exec->call_frames[rt->exec->call_frame_sp - 1u];
    ft = wasm__frame_functype(cf);
    for (i = 0; i < ft->num_params && i < num_args; i++) {
        wasm_value_t value = args[i];

        err = wasm__anyref_from_host(rt, &value);
        if (err != WASM_OK) return err;
        wasm__slot_from_value(&cf->locals[cf->func->local_slots[i]], &value);
    }
    return WASM_OK;
}

/* callee is a function of call_mod. */
static WASM__COLD int wasm__callee_type_matches(const wasm_module_t* mod, uint32_t type_idx,
                                               const wasm_module_t* call_mod, const wasm_func_t* callee) {
    const wasm_module_t* type_mod = call_mod;
    uint32_t callee_type = callee->type_idx;

    if (callee->is_import)
        (void)wasm__func_dynamic_type(call_mod, (uint32_t)(callee - call_mod->funcs), &type_mod, &callee_type);
    return wasm__type_is_subtype_across_modules(type_mod, callee_type, mod, type_idx);
}

WASM_PRIVATE int wasm__indirect_callee_matches(const wasm_module_t* mod, uint32_t type_idx,
                                              const wasm_module_t* call_mod, wasm_func_t* callee) {
    if (callee->matched_module == mod->identity && callee->matched_type == type_idx) return 1;
    if (!wasm__callee_type_matches(mod, type_idx, call_mod, callee)) return 0;
    callee->matched_module = mod->identity;
    callee->matched_type = type_idx;
    return 1;
}

WASM_PRIVATE wasm_error_t wasm__finish_call_frame(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    wasm__exec_stack_t* exec = rt->exec;
    wasm__call_frame_t* cf;
    const wasm_functype_t* ft;
    wasm_value_t result_values_buf[8];
    wasm_value_t* result_values = result_values_buf;
    wasm_value_t* results_dst;
    uint32_t result_count;
    uint32_t i;
    int is_root;

    if (exec->call_frame_sp <= base_frame_sp) return WASM_ERR_MALFORMED;

    cf = &exec->call_frames[exec->call_frame_sp - 1];
    ft = wasm__frame_functype(cf);
    results_dst = cf->results_dst;
    result_count = ft->num_results;
    is_root = (exec->call_frame_sp == base_frame_sp + 1u);

    if (result_count > 8) {
        result_values = (wasm_value_t*)wasm__malloc_array(&rt->allocator, result_count, sizeof(wasm_value_t));
        if (!result_values) return WASM_ERR_OOM;
    }

    wasm__values_from_slots(rt, ft->results, ft->result_reftypes, result_count,
                            &exec->stack[exec->sp - cf->func->result_slots], result_values);

    exec->sp = cf->sp_base;
    wasm__arena_reset(rt, exec, cf->arena_saved);
    exec->call_frame_sp--;
    memset(cf, 0, sizeof(*cf));

    if (is_root) {
        for (i = 0; i < result_count; i++) results_dst[i] = wasm__anyref_to_host(rt, result_values[i]);
    } else {
        exec->sp += wasm__slots_from_values(&exec->stack[exec->sp], result_values, result_count);
    }

    if (result_values != result_values_buf) wasm__free(&rt->allocator, result_values);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__handle_exception_frames(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    wasm__exec_stack_t* exec = rt->exec;

    while (exec->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &exec->call_frames[exec->call_frame_sp - 1];

        if (cf->lsp > 0) {
            wasm_error_t err = wasm__handle_exception_in_frame(cf);

            if (err == WASM_OK) return WASM_OK;
            if (err != WASM__ERR_EXCEPTION) return err;
        }
        exec->sp = cf->sp_base;
        wasm__arena_reset(rt, exec, cf->arena_saved);
        exec->call_frame_sp--;
    }

    return WASM__ERR_EXCEPTION;
}

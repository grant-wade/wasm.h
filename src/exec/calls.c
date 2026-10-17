#include "calls.h"
#include "../module.h"
#include <stdio.h>
#include "exceptions.h"
#include "frames.h"
#include "interpreter.h"
#include "../internal/alloc.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include "../validate/function.h"
#include "../values.h"
#include <string.h>

WASM_PRIVATE void wasm__clear_label(wasm__label_t* label) {
    WASM_FREE(label->caught_values);
    label->caught_values = NULL;
    label->caught_tag = UINT32_MAX;
    label->caught_tag_identity = 0;
    label->caught_count = 0;
    label->active_catch = 0;
    label->delegate_depth = UINT32_MAX;
}

WASM_PRIVATE void wasm__clear_call_frame(wasm__call_frame_t* cf) {
    if (!cf) return;

    while (cf->lsp > 0) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }
}

WASM_PRIVATE wasm_error_t wasm__push_call_frame(wasm_module_t* mod,
                                          uint32_t func_idx,
                                          const wasm_value_t* args,
                                          uint32_t num_args,
                                          wasm_value_t* results_dst,
                                          uint32_t num_results) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err;
    wasm_func_t* func;
    wasm_functype_t* ft;
    wasm__call_frame_t* cf;
    wasm_value_t* locals = NULL;
    wasm__label_t* labels;
    uint32_t max_labels;
    size_t arena_saved;
    uint32_t i;

    if (rt->call_frame_sp == 0) {
        err = wasm__ensure_module_frame_arena(mod);
        if (err != WASM_OK) return err;
    }

    if (rt->call_frame_sp >= rt->max_call_frames) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "call frame depth exceeded limit %u",
                      (unsigned)rt->max_call_frames);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }
    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import) return WASM_ERR_MALFORMED;
    err = wasm__prepare_function(mod, func_idx);
    if (err != WASM_OK) return err;

    if (rt->call_frame_sp == 0) {
        err = wasm__ensure_module_frame_arena(mod);
        if (err != WASM_OK) return err;
    }

    ft = wasm__module_functype(mod, func->type_idx);
    if (!ft) return WASM_ERR_MALFORMED;
    max_labels = func->max_label_depth ? func->max_label_depth : 1;
    if (max_labels > rt->max_labels) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "function label depth %u exceeds limit %u",
                      (unsigned)max_labels,
                      (unsigned)rt->max_labels);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }
    arena_saved = rt->frame_arena_offset;

    if (func->num_locals > 0) {
        locals = (wasm_value_t*)wasm__arena_alloc(rt, func->num_locals * sizeof(wasm_value_t));
        if (!locals) {
            wasm__arena_reset(rt, arena_saved);
            WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED, "%s", "frame arena exhausted");
            return WASM_ERR_CALL_STACK_EXHAUSTED;
        }
    }

    labels = (wasm__label_t*)wasm__arena_alloc(rt, max_labels * sizeof(wasm__label_t));
    if (!labels) {
        wasm__arena_reset(rt, arena_saved);
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED, "%s", "frame arena exhausted");
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }

    cf = &rt->call_frames[rt->call_frame_sp++];
    wasm__init_call_frame(cf, mod, rt, func_idx, func, ft, locals, labels, results_dst, num_results);
    cf->arena_saved = arena_saved;

    for (i = 0; i < func->num_locals; i++) cf->locals[i] = wasm__default_value(func->locals[i]);
    for (i = 0; i < ft->num_params && i < num_args; i++) {
        cf->locals[i] = wasm__retag_value_for_declared_type(
            ft->params[i],
            ft->param_reftypes ? &ft->param_reftypes[i] : NULL,
            args[i]);
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__finish_call_frame(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    wasm__call_frame_t* cf;
    wasm_value_t result_values_buf[8];
    wasm_value_t* result_values = result_values_buf;
    wasm_value_t* results_dst;
    uint32_t result_count;
    uint32_t i;
    int is_root;

    if (rt->call_frame_sp <= base_frame_sp) return WASM_ERR_MALFORMED;

    cf = &rt->call_frames[rt->call_frame_sp - 1];
    results_dst = cf->results_dst;
    result_count = cf->ft->num_results;
    is_root = (rt->call_frame_sp == base_frame_sp + 1u);

    if (result_count > 8) {
        result_values = (wasm_value_t*)wasm__malloc_array(result_count, sizeof(wasm_value_t));
        if (!result_values) return WASM_ERR_OOM;
    }

    for (i = 0; i < result_count; i++) {
        result_values[i] = wasm__retag_value_for_declared_type(
            cf->ft->results[i],
            cf->ft->result_reftypes ? &cf->ft->result_reftypes[i] : NULL,
            rt->stack[rt->sp - result_count + i]);
    }

    rt->sp = cf->sp_base;
    wasm__arena_reset(rt, cf->arena_saved);
    rt->call_frame_sp--;
    memset(cf, 0, sizeof(*cf));

    if (is_root) {
        for (i = 0; i < result_count; i++) results_dst[i] = result_values[i];
    } else {
        for (i = 0; i < result_count; i++) rt->stack[rt->sp++] = result_values[i];
    }

    if (result_values != result_values_buf) WASM_FREE(result_values);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__handle_exception_frames(wasm_module_t* mod, uint32_t base_frame_sp) {
    wasm_runtime_t* rt = mod->rt;

    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];
        wasm_error_t err;

        err = wasm__handle_exception_in_frame(cf->module, cf->labels, &cf->lsp, &cf->r, cf->sp_base);
        if (err == WASM_OK) return WASM_OK;
        if (err != WASM__ERR_EXCEPTION) return err;

        wasm__clear_call_frame(cf);
        rt->sp = cf->sp_base;
        wasm__arena_reset(rt, cf->arena_saved);
        memset(cf, 0, sizeof(*cf));
        rt->call_frame_sp--;
    }

    return WASM__ERR_EXCEPTION;
}

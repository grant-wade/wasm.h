#include "calls.h"
#include "../module.h"
#include <stdio.h>
#include "exceptions.h"
#include "frames.h"
#include "slots.h"
#include "../internal/alloc.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include "../types/references.h"
#include "../validate/function.h"
#include "../values.h"
#include <string.h>

WASM_PRIVATE void wasm__clear_label(wasm__label_t* label) {
    if (label->caught_values) {
        wasm__free(label->caught_allocator, label->caught_values);
        label->caught_values = NULL;
    }
    label->caught_tag = UINT32_MAX;
    label->caught_tag_identity = 0;
    label->caught_count = 0;
    label->active_catch = 0;
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
                                          const wasm__slot_t* args,
                                          wasm_value_t* results_dst) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err;
    wasm_func_t* func;
    wasm__call_frame_t* cf;
    wasm__slot_t* locals;
    wasm__arena_mark_t arena_saved;
    uint32_t i;

    if (rt->call_frame_sp >= rt->max_call_frames) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "call frame depth exceeded limit %u",
                      (unsigned)rt->max_call_frames);
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
    if (func->max_stack > rt->max_stack - rt->sp) {
        WASM__SET_ERR(rt, WASM_ERR_STACK_OVERFLOW, "operand stack limit %u exceeded", (unsigned)rt->max_stack);
        return WASM_ERR_STACK_OVERFLOW;
    }
    if (func->max_label_depth > rt->max_labels) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "function label depth %u exceeds limit %u",
                      (unsigned)func->max_label_depth,
                      (unsigned)rt->max_labels);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }
    arena_saved = wasm__arena_mark(rt);
    locals = NULL;
    if (func->frame_bytes > 0) {
        locals = (wasm__slot_t*)wasm__arena_alloc(rt, func->frame_bytes);
        if (!locals) {
            wasm__arena_reset(rt, arena_saved);
            WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "out of memory for call frame");
            return WASM_ERR_OOM;
        }
    }

    cf = &rt->call_frames[rt->call_frame_sp++];
    cf->module = mod;
    cf->func_idx = func_idx;
    cf->sp_base = rt->sp;
    cf->locals = locals;
    cf->labels = func->max_handlers > 0 ? (wasm__label_t*)(locals + func->num_local_slots) : NULL;
    cf->lsp = 0;
    cf->pc = func->ops;
    cf->op_at = func->ops;
    cf->func = func;
    cf->results_dst = results_dst;
    cf->arena_saved = arena_saved;

    i = 0;
    if (args) {
        for (; i < func->param_slots; i++) locals[i] = args[i];
    }
    if (i < func->num_local_slots) memset(locals + i, 0, (func->num_local_slots - i) * sizeof(*locals));
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
    wasm_error_t err = wasm__push_call_frame(mod, func_idx, NULL, results_dst);
    uint32_t i;

    if (err != WASM_OK) return err;
    cf = &rt->call_frames[rt->call_frame_sp - 1u];
    ft = wasm__frame_functype(cf);
    for (i = 0; i < ft->num_params && i < num_args; i++) {
        wasm_value_t value = args[i];

        err = wasm__anyref_from_host(rt, &value);
        if (err != WASM_OK) return err;
        wasm__slot_from_value(&cf->locals[cf->func->local_slots[i]], &value);
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__finish_call_frame(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    wasm__call_frame_t* cf;
    const wasm_functype_t* ft;
    wasm_value_t result_values_buf[8];
    wasm_value_t* result_values = result_values_buf;
    wasm_value_t* results_dst;
    uint32_t result_count;
    uint32_t i;
    int is_root;

    if (rt->call_frame_sp <= base_frame_sp) return WASM_ERR_MALFORMED;

    cf = &rt->call_frames[rt->call_frame_sp - 1];
    ft = wasm__frame_functype(cf);
    results_dst = cf->results_dst;
    result_count = ft->num_results;
    is_root = (rt->call_frame_sp == base_frame_sp + 1u);

    if (result_count > 8) {
        result_values = (wasm_value_t*)wasm__malloc_array(&rt->allocator, result_count, sizeof(wasm_value_t));
        if (!result_values) return WASM_ERR_OOM;
    }

    wasm__values_from_slots(rt, ft->results, ft->result_reftypes, result_count,
                            &rt->stack[rt->sp - cf->func->result_slots], result_values);

    rt->sp = cf->sp_base;
    wasm__arena_reset(rt, cf->arena_saved);
    rt->call_frame_sp--;
    memset(cf, 0, sizeof(*cf));

    if (is_root) {
        for (i = 0; i < result_count; i++) results_dst[i] = wasm__anyref_to_host(rt, result_values[i]);
    } else {
        rt->sp += wasm__slots_from_values(&rt->stack[rt->sp], result_values, result_count);
    }

    if (result_values != result_values_buf) wasm__free(&rt->allocator, result_values);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__handle_exception_frames(wasm_module_t* mod, uint32_t base_frame_sp) {
    wasm_runtime_t* rt = mod->rt;

    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];
        wasm_error_t err;

        err = wasm__handle_exception_in_frame(cf);
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

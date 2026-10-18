#include "frames.h"
#include "../module.h"
#include "state.h"
#include "translate.h"
#include <stdio.h>
#include "../internal/alloc.h"
#include "../runtime.h"

WASM_PRIVATE wasm_error_t wasm__call_frame_stack_init(wasm_runtime_t* rt) {
    rt->call_frames = (wasm__call_frame_t*)WASM_CALLOC(rt->max_call_frames, sizeof(*rt->call_frames));
    if (!rt->call_frames) return WASM_ERR_OOM;
    rt->call_frame_sp = 0;
    return WASM_OK;
}

WASM_PRIVATE void wasm__call_frame_stack_destroy(wasm_runtime_t* rt) {
    WASM_FREE(rt->call_frames);
    rt->call_frames = NULL;
    rt->max_call_frames = 0;
    rt->call_frame_sp = 0;
}

WASM_PRIVATE void wasm__clear_backtrace(wasm_runtime_t* rt) {
    if (!rt) return;
    rt->backtrace_depth = 0;
}

static uint32_t wasm__call_frame_offset(const wasm__call_frame_t* cf) {
    if (!cf || !cf->func) return 0;
    return wasm__translated_offset(cf->func, cf->op_at);
}

WASM_PRIVATE void wasm__capture_backtrace(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    uint32_t depth;
    uint32_t i;

    if (!rt) return;
    if (rt->call_frame_sp <= base_frame_sp) {
        rt->backtrace_depth = 0;
        return;
    }

    depth = rt->call_frame_sp - base_frame_sp;
    if (depth > rt->max_call_frames) depth = rt->max_call_frames;
    rt->backtrace_depth = depth;
    for (i = 0; i < depth; i++) {
        const wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1u - i];

        rt->backtrace_func_indices[i] = cf->func_idx;
        rt->backtrace_modules[i] = cf->module;
        rt->backtrace_offsets[i] = wasm__call_frame_offset(cf);
    }
}

WASM_PRIVATE uint32_t wasm__visible_call_stack_depth(const wasm_runtime_t* rt) {
    if (!rt) return 0;
    if (rt->call_frame_sp > 0) return rt->call_frame_sp;
    return rt->backtrace_depth;
}

WASM_PRIVATE int wasm__visible_call_frame_info(const wasm_runtime_t* rt,
                                         uint32_t depth,
                                         wasm_module_t** out_module,
                                         uint32_t* out_func_idx,
                                         uint32_t* out_offset) {
    if (!rt) return 0;

    if (rt->call_frame_sp > 0) {
        const wasm__call_frame_t* cf;

        if (depth >= rt->call_frame_sp) return 0;
        cf = &rt->call_frames[rt->call_frame_sp - 1u - depth];
        if (out_module) *out_module = cf->module;
        if (out_func_idx) *out_func_idx = cf->func_idx;
        if (out_offset) *out_offset = wasm__call_frame_offset(cf);
        return 1;
    }

    if (depth >= rt->backtrace_depth) return 0;
    if (out_module) *out_module = rt->backtrace_modules[depth];
    if (out_func_idx) *out_func_idx = rt->backtrace_func_indices[depth];
    if (out_offset) *out_offset = rt->backtrace_offsets[depth];
    return 1;
}

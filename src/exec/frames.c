#include "frames.h"
#include "../module.h"
#include "state.h"
#include "translate.h"
#include <stdio.h>
#include "../handles.h"
#include "../internal/alloc.h"
#include "../internal/gc_types.h"
#include "../runtime.h"
#include <stdlib.h>

WASM_PRIVATE wasm_error_t wasm__call_frame_stack_init(wasm_runtime_t* rt) {
    rt->call_frames = (wasm__call_frame_t*)wasm__calloc(&rt->allocator, rt->max_call_frames, sizeof(*rt->call_frames));
    if (!rt->call_frames) return WASM_ERR_OOM;
    rt->call_frame_sp = 0;
    return WASM_OK;
}

WASM_PRIVATE void wasm__call_frame_stack_destroy(wasm_runtime_t* rt) {
    wasm__free(&rt->allocator, rt->call_frames);
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

WASM_PRIVATE const uint32_t* wasm__frame_stack_map(const wasm_runtime_t* rt, uint32_t frame_index, uint32_t* out_slots) {
    const wasm__call_frame_t* cf = &rt->call_frames[frame_index];
    const wasm_func_t* func = cf->func;
    const uint32_t* at = cf->op_at;
    uint32_t offset;
    uint32_t lo = 0;
    uint32_t hi;

    *out_slots = 0;
    if (!func || !func->ops || func->num_stack_maps == 0) return NULL;
    if (rt->suspended != WASM_OK && frame_index + 1u == rt->call_frame_sp && cf->pc) at = cf->pc;
    offset = wasm__translated_offset(func, at);
    hi = func->num_stack_maps;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;

        if (func->stack_maps[mid * 3u] < offset)
            lo = mid + 1u;
        else
            hi = mid;
    }
    if (lo == func->num_stack_maps || func->stack_maps[lo * 3u] != offset) return NULL;
    *out_slots = func->stack_maps[lo * 3u + 2u];
    return func->stack_map_bits + func->stack_maps[lo * 3u + 1u];
}

#ifndef NDEBUG
static void wasm__check_ref_slot(const wasm_runtime_t* rt, const wasm__call_frame_t* cf, const char* where,
                                 uint32_t slot, uint32_t kind, const wasm__slot_t* s) {
    uintptr_t bits = wasm__slot_ref(s);
    int valid = 1;

    if (bits == 0) return;
    if (kind == WASM__SLOT_KIND_ANY)
        valid = (bits & WASM__GC_I31_TAG) != 0 || wasm__gc_ptr_in_heap(rt, (const void*)bits) ||
                wasm__handle_set_contains(&rt->externref_boxes, (const void*)bits);
    else if (kind == WASM__SLOT_KIND_EXN)
        valid = wasm__handle_set_contains(&rt->exception_objects, (const void*)bits);
    if (valid) return;
    fprintf(stderr, "stack map mismatch in function %u at offset %u: %s slot %u of kind %u holds %p\n",
            (unsigned)cf->func_idx, (unsigned)wasm__translated_offset(cf->func, cf->op_at), where, (unsigned)slot,
            (unsigned)kind, (void*)bits);
    abort();
}

WASM_PRIVATE void wasm__check_stack_maps(const wasm_runtime_t* rt) {
    uint32_t i;

    for (i = 0; i < rt->call_frame_sp; i++) {
        const wasm__call_frame_t* cf = &rt->call_frames[i];
        uint32_t end = i + 1u < rt->call_frame_sp ? rt->call_frames[i + 1u].sp_base : rt->sp;
        uint32_t map_slots;
        const uint32_t* map;
        uint32_t j;

        if (!cf->func || !cf->func->ops) continue;
        map = wasm__frame_stack_map(rt, i, &map_slots);
        for (j = 0; map && j < map_slots && cf->sp_base + j < end; j++)
            wasm__check_ref_slot(rt, cf, "operand", j, wasm__stack_map_kind(map, j), &rt->stack[cf->sp_base + j]);
        for (j = 0; j < cf->func->num_locals && cf->locals; j++)
            wasm__check_ref_slot(rt, cf, "local", j, wasm__slot_kind(cf->func->locals[j]),
                                 &cf->locals[cf->func->local_slots[j]]);
    }
}
#endif

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

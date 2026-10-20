#include "frames.h"
#include "cont.h"
#include "exceptions.h"
#include "../module.h"
#include "state.h"
#include "translate.h"
#include <stdio.h>
#include "../handles.h"
#include "../internal/gc_types.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include <stdlib.h>

WASM_PRIVATE void wasm__clear_backtrace(wasm_runtime_t* rt) {
    if (!rt) return;
    rt->backtrace_depth = 0;
}

static uint32_t wasm__call_frame_offset(const wasm__call_frame_t* cf) {
    if (!cf || !cf->func) return 0;
    return wasm__translated_offset(cf->func, cf->op_at);
}

WASM_PRIVATE void wasm__capture_backtrace(wasm_runtime_t* rt, const wasm__exec_stack_t* base_stack,
                                          uint32_t base_frame_sp) {
    const wasm__exec_stack_t* s;
    uint32_t depth = 0;

    if (!rt) return;
    for (s = rt->exec; s && depth < rt->backtrace_capacity; s = s == base_stack ? NULL : s->parent) {
        uint32_t floor = s == base_stack ? base_frame_sp : 0;
        uint32_t i;

        for (i = s->call_frame_sp; i > floor && depth < rt->backtrace_capacity; i--, depth++) {
            const wasm__call_frame_t* cf = &s->call_frames[i - 1u];

            rt->backtrace_func_indices[depth] = cf->func_idx;
            rt->backtrace_modules[depth] = cf->module;
            rt->backtrace_offsets[depth] = wasm__call_frame_offset(cf);
        }
    }
    rt->backtrace_depth = depth;
}

WASM_PRIVATE const uint32_t* wasm__frame_stack_map(const wasm__exec_stack_t* s, uint32_t frame_index,
                                                   uint32_t* out_slots) {
    const wasm__call_frame_t* cf = &s->call_frames[frame_index];
    const wasm_func_t* func = cf->func;
    const uint32_t* at = cf->op_at;
    uint32_t offset;
    uint32_t lo = 0;
    uint32_t hi;

    *out_slots = 0;
    if (!func || !func->ops || func->num_stack_maps == 0) return NULL;
    if (s->parked && frame_index + 1u == s->call_frame_sp && cf->pc) at = cf->pc;
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

WASM_PRIVATE const wasm_functype_t* wasm__stack_start_type(const wasm__exec_stack_t* s) {
    if (s->call_frame_sp > 0 || !s->start_module) return NULL;
    return wasm__module_functype(s->start_module, s->start_module->funcs[s->start_func].type_idx);
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
    else if (kind == WASM__SLOT_KIND_HANDLE)
        valid = wasm__exn_issued(rt, bits) || wasm__cont_issued(rt, bits);
    if (valid) return;
    if (cf)
        fprintf(stderr, "stack map mismatch in function %u at offset %u: %s slot %u of kind %u holds %p\n",
                (unsigned)cf->func_idx, (unsigned)wasm__translated_offset(cf->func, cf->op_at), where,
                (unsigned)slot, (unsigned)kind, (void*)bits);
    else
        fprintf(stderr, "stack map mismatch in continuation arguments: slot %u of kind %u holds %p\n",
                (unsigned)slot, (unsigned)kind, (void*)bits);
    abort();
}

WASM_PRIVATE void wasm__check_stack_maps(const wasm_runtime_t* rt) {
    const wasm__exec_stack_t* s;
    uint32_t i;

    for (i = 0; i < rt->conts.count; i++) {
        const wasm__cont_object_t* k = wasm__cont_at(&rt->conts, i);
        const wasm_functype_t* start_type = k->in_use ? wasm__cont_start_type(k) : NULL;
        uint32_t j;

        for (j = 0; start_type && j < start_type->num_params; j++) {
            uint32_t slot = wasm__valtypes_slots(start_type->params, j);

            if (slot >= k->num_args) break;
            wasm__check_ref_slot(rt, NULL, "argument", slot, wasm__slot_kind(start_type->params[j]), &k->args[slot]);
        }
    }

    for (s = wasm__next_live_stack(rt, NULL); s; s = wasm__next_live_stack(rt, s)) {
        const wasm_functype_t* start_type = wasm__stack_start_type(s);

        for (i = 0; start_type && i < start_type->num_params; i++) {
            uint32_t slot = wasm__valtypes_slots(start_type->params, i);

            if (slot >= s->sp) break;
            wasm__check_ref_slot(rt, NULL, "argument", slot, wasm__slot_kind(start_type->params[i]), &s->stack[slot]);
        }
        for (i = 0; i < s->call_frame_sp; i++) {
            const wasm__call_frame_t* cf = &s->call_frames[i];
            uint32_t end = i + 1u < s->call_frame_sp ? s->call_frames[i + 1u].sp_base : s->sp;
            uint32_t map_slots;
            const uint32_t* map;
            uint32_t j;

            if (!cf->func || !cf->func->ops) continue;
            map = wasm__frame_stack_map(s, i, &map_slots);
            for (j = 0; map && j < map_slots && cf->sp_base + j < end; j++)
                wasm__check_ref_slot(rt, cf, "operand", j, wasm__stack_map_kind(map, j), &s->stack[cf->sp_base + j]);
            for (j = 0; j < cf->func->num_locals && cf->locals; j++)
                wasm__check_ref_slot(rt, cf, "local", j, wasm__slot_kind(cf->func->locals[j]),
                                     &cf->locals[cf->func->local_slots[j]]);
        }
    }
}
#endif

/* Returns the running frame depth frames below the innermost, or NULL. */
static const wasm__call_frame_t* wasm__running_frame(const wasm_runtime_t* rt, uint32_t depth) {
    const wasm__exec_stack_t* s;

    for (s = rt->exec; s; s = s->parent) {
        if (depth < s->call_frame_sp) return &s->call_frames[s->call_frame_sp - 1u - depth];
        depth -= s->call_frame_sp;
    }
    return NULL;
}

WASM_PRIVATE uint32_t wasm__visible_call_stack_depth(const wasm_runtime_t* rt) {
    const wasm__exec_stack_t* s;
    uint32_t depth = 0;

    if (!rt) return 0;
    for (s = rt->exec; s; s = s->parent) depth += s->call_frame_sp;
    return depth > 0 ? depth : rt->backtrace_depth;
}

WASM_PRIVATE int wasm__visible_call_frame_info(const wasm_runtime_t* rt,
                                         uint32_t depth,
                                         wasm_module_t** out_module,
                                         uint32_t* out_func_idx,
                                         uint32_t* out_offset) {
    if (!rt) return 0;

    if (wasm__running_frame(rt, 0)) {
        const wasm__call_frame_t* cf = wasm__running_frame(rt, depth);

        if (!cf) return 0;
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

#include "../internal.h"

WASM_PRIVATE int wasm__size_mul(size_t a, size_t b, size_t* out) {
    if (a != 0 && b > ((size_t)-1) / a) return 0;
    *out = a * b;
    return 1;
}

WASM_PRIVATE int wasm__size_add(size_t a, size_t b, size_t* out) {
    if (a > ((size_t)-1) - b) return 0;
    *out = a + b;
    return 1;
}

WASM_PRIVATE int wasm__size_align(size_t value, size_t alignment, size_t* out) {
    size_t remainder;

    if (alignment <= 1) {
        *out = value;
        return 1;
    }

    remainder = value % alignment;
    if (remainder == 0) {
        *out = value;
        return 1;
    }

    return wasm__size_add(value, alignment - remainder, out);
}

WASM_PRIVATE int wasm__frame_storage_size(uint32_t num_locals,
                                    uint32_t max_labels,
                                    size_t* out) {
    size_t locals_bytes = 0;
    size_t labels_bytes;
    size_t total_bytes;

    if (num_locals > 0) {
        if (!wasm__size_mul((size_t)num_locals, sizeof(wasm_value_t), &locals_bytes)) return 0;
        if (!wasm__size_align(locals_bytes, sizeof(void*), &locals_bytes)) return 0;
    }

    if (max_labels == 0) max_labels = 1;
    if (!wasm__size_mul((size_t)max_labels, sizeof(wasm__label_t), &labels_bytes)) return 0;
    if (!wasm__size_align(labels_bytes, sizeof(void*), &labels_bytes)) return 0;
    if (!wasm__size_add(locals_bytes, labels_bytes, &total_bytes)) return 0;

    *out = total_bytes;
    return 1;
}

WASM_PRIVATE int wasm__module_frame_arena_requirement(wasm_module_t* mod, size_t* out) {
    size_t max_frame_bytes = 0;
    size_t total_bytes;
    size_t minimum_size;
    uint32_t i;

    if (!mod || !out) return 0;

    for (i = mod->num_func_imports; i < mod->num_funcs; i++) {
        wasm_func_t* func = &mod->funcs[i];
        size_t frame_bytes;
        uint32_t max_labels = func->max_label_depth;

        if (max_labels == 0) {
            if (mod->rt && mod->rt->lazy_validation)
                max_labels = mod->rt->max_labels;
            else
                max_labels = 1u;
        }

        if (!wasm__frame_storage_size(func->num_locals, max_labels, &frame_bytes)) return 0;
        if (frame_bytes > max_frame_bytes) max_frame_bytes = frame_bytes;
    }

    minimum_size = mod->rt ? mod->rt->frame_arena_size : (size_t)WASM_FRAME_ARENA_SIZE;
    if (max_frame_bytes == 0) {
        *out = minimum_size;
        return 1;
    }

    if (!wasm__size_mul(max_frame_bytes,
                        (size_t)((mod->rt && mod->rt->max_call_frames) ? mod->rt->max_call_frames : WASM_MAX_CALL_DEPTH),
                        &total_bytes)) {
        return 0;
    }
    if (total_bytes < minimum_size) total_bytes = minimum_size;

    *out = total_bytes;
    return 1;
}

WASM_PRIVATE wasm_error_t wasm__reserve_frame_arena(wasm_runtime_t* rt, size_t size) {
    uint8_t* arena;

    if (size <= rt->frame_arena_size) return WASM_OK;
    if (rt->call_frame_sp != 0) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "cannot grow frame arena while %u call frames are active",
                      (unsigned)rt->call_frame_sp);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }

    arena = (uint8_t*)WASM_REALLOC(rt->frame_arena, size);
    if (!arena) {
        WASM__SET_ERR(rt, WASM_ERR_OOM,
                      "failed to reserve %zu-byte frame arena",
                      size);
        return WASM_ERR_OOM;
    }

    rt->frame_arena = arena;
    rt->frame_arena_size = size;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__ensure_module_frame_arena(wasm_module_t* mod) {
    size_t required_size;

    if (!mod || !mod->rt) return WASM_OK;
    if (!wasm__module_frame_arena_requirement(mod, &required_size)) {
        WASM__SET_ERR(mod->rt, WASM_ERR_OOM, "%s", "frame arena size overflow");
        return WASM_ERR_OOM;
    }

    return wasm__reserve_frame_arena(mod->rt, required_size);
}

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

WASM_PRIVATE uint32_t wasm__call_frame_offset(const wasm__call_frame_t* cf) {
    ptrdiff_t offset;

    if (!cf || !cf->func || !cf->func->code) return 0;
    if (cf->current_offset <= cf->func->code_len) return cf->current_offset;
    if (!cf->r.ptr || cf->r.ptr < cf->func->code) return 0;
    offset = cf->r.ptr - cf->func->code;
    if (offset < 0) return 0;
    if ((uint32_t)offset > cf->func->code_len) return cf->func->code_len;
    return (uint32_t)offset;
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
                                         uint32_t* out_func_idx,
                                         uint32_t* out_offset) {
    if (!rt) return 0;

    if (rt->call_frame_sp > 0) {
        const wasm__call_frame_t* cf;

        if (depth >= rt->call_frame_sp) return 0;
        cf = &rt->call_frames[rt->call_frame_sp - 1u - depth];
        if (out_func_idx) *out_func_idx = cf->func_idx;
        if (out_offset) *out_offset = wasm__call_frame_offset(cf);
        return 1;
    }

    if (depth >= rt->backtrace_depth) return 0;
    if (out_func_idx) *out_func_idx = rt->backtrace_func_indices[depth];
    if (out_offset) *out_offset = rt->backtrace_offsets[depth];
    return 1;
}

#include "stack.h"
#include "state.h"
#include "../internal/alloc.h"
#include "../runtime.h"
#include <string.h>

WASM_PRIVATE wasm_error_t wasm__exec_stack_init(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t max_stack,
                                                uint32_t max_call_frames, size_t frame_arena_size) {
    memset(s, 0, sizeof(*s));
    if (max_stack == 0 || max_call_frames == 0) return WASM_ERR_MALFORMED;
    if (sizeof(*s->stack) > SIZE_MAX / (size_t)max_stack) return WASM_ERR_OOM;
    s->stack = (wasm__slot_t*)wasm__alloc_sized(&rt->allocator, (size_t)max_stack * sizeof(*s->stack),
                                                WASM__ALIGNOF(wasm__slot_t));
    if (!s->stack) return WASM_ERR_OOM;
    s->max_stack = max_stack;
    s->call_frames = (wasm__call_frame_t*)wasm__calloc(&rt->allocator, max_call_frames, sizeof(*s->call_frames));
    if (!s->call_frames || wasm__arena_init(rt, s, frame_arena_size) != WASM_OK) {
        wasm__exec_stack_destroy(rt, s);
        return WASM_ERR_OOM;
    }
    s->max_call_frames = max_call_frames;
    return WASM_OK;
}

WASM_PRIVATE void wasm__exec_stack_destroy(wasm_runtime_t* rt, wasm__exec_stack_t* s) {
    wasm__arena_destroy(rt, s);
    wasm__free(&rt->allocator, s->call_frames);
    wasm__free_sized(&rt->allocator, s->stack, (size_t)s->max_stack * sizeof(*s->stack), WASM__ALIGNOF(wasm__slot_t));
    memset(s, 0, sizeof(*s));
}

WASM_PRIVATE wasm__exec_stack_t* wasm__next_live_stack(const wasm_runtime_t* rt, const wasm__exec_stack_t* s) {
    if (!s) return (wasm__exec_stack_t*)&rt->main_stack;
    return s == &rt->main_stack ? rt->cont_stacks : s->next;
}

static void wasm__arena_free_chunk(wasm_runtime_t* rt, wasm__arena_chunk_t* chunk) {
    if (chunk) wasm__free_sized(&rt->allocator, chunk, WASM__ARENA_HEADER + chunk->size, WASM__ARENA_ALIGN);
}

WASM_PRIVATE void wasm__arena_destroy(wasm_runtime_t* rt, wasm__exec_stack_t* s) {
    while (s->frame_arena) {
        wasm__arena_chunk_t* prev = s->frame_arena->prev;

        wasm__arena_free_chunk(rt, s->frame_arena);
        s->frame_arena = prev;
    }
    s->frame_arena_size = 0;
}

WASM_PRIVATE void wasm__arena_free_spare(wasm_runtime_t* rt) {
    wasm__arena_free_chunk(rt, rt->frame_arena_spare);
    rt->frame_arena_spare = NULL;
}

static wasm__arena_chunk_t* wasm__arena_new_chunk(wasm_runtime_t* rt, wasm__exec_stack_t* s, size_t min_size) {
    wasm__arena_chunk_t* chunk = rt->frame_arena_spare;
    size_t size = min_size > s->frame_arena_size ? min_size : s->frame_arena_size;

    if (chunk && chunk->size >= size) {
        rt->frame_arena_spare = NULL;
    } else {
        if (size > SIZE_MAX - WASM__ARENA_HEADER) return NULL;
        chunk = (wasm__arena_chunk_t*)wasm__alloc_sized(&rt->allocator, WASM__ARENA_HEADER + size, WASM__ARENA_ALIGN);
        if (!chunk) return NULL;
        chunk->size = size;
    }
    chunk->prev = s->frame_arena;
    chunk->used = 0;
    s->frame_arena = chunk;
    return chunk;
}

WASM_PRIVATE wasm_error_t wasm__arena_init(wasm_runtime_t* rt, wasm__exec_stack_t* s, size_t size) {
    wasm__arena_destroy(rt, s);
    s->frame_arena_size = size;
    if (size == 0) return WASM_OK;
    return wasm__arena_new_chunk(rt, s, size) ? WASM_OK : WASM_ERR_OOM;
}

WASM_PRIVATE void* wasm__arena_alloc(wasm_runtime_t* rt, wasm__exec_stack_t* s, size_t size) {
    wasm__arena_chunk_t* chunk = s->frame_arena;
    size_t aligned;

    if (size > SIZE_MAX - WASM__ARENA_ALIGN) return NULL;
    aligned = (size + WASM__ARENA_ALIGN - 1u) / WASM__ARENA_ALIGN * WASM__ARENA_ALIGN;
    if (!chunk || aligned > chunk->size - chunk->used) {
        chunk = wasm__arena_new_chunk(rt, s, aligned);
        if (!chunk) return NULL;
    }
    chunk->used += aligned;
    return (uint8_t*)chunk + WASM__ARENA_HEADER + (chunk->used - aligned);
}

WASM_PRIVATE wasm__arena_mark_t wasm__arena_mark(const wasm__exec_stack_t* s) {
    wasm__arena_mark_t mark;

    mark.chunk = s->frame_arena;
    mark.used = s->frame_arena ? s->frame_arena->used : 0u;
    return mark;
}

WASM_PRIVATE void wasm__arena_reset_slow(wasm_runtime_t* rt, wasm__exec_stack_t* s, wasm__arena_mark_t mark) {
    while (s->frame_arena && s->frame_arena != mark.chunk) {
        wasm__arena_chunk_t* chunk = s->frame_arena;

        s->frame_arena = chunk->prev;
        if (rt->frame_arena_spare && rt->frame_arena_spare->size >= chunk->size) {
            wasm__arena_free_chunk(rt, chunk);
        } else {
            wasm__arena_free_chunk(rt, rt->frame_arena_spare);
            rt->frame_arena_spare = chunk;
        }
    }
    if (s->frame_arena) s->frame_arena->used = mark.used;
}

#include "stack.h"
#include "state.h"
#include "../internal/alloc.h"
#include "../runtime.h"
#include <string.h>

/* Largest size of a frame arena's first block. */
#define WASM__ARENA_FIRST_SIZE ((size_t)1024u)

static size_t wasm__arena_first_size(const wasm__exec_stack_t* s) {
    return s->frame_arena_size < WASM__ARENA_FIRST_SIZE ? s->frame_arena_size : WASM__ARENA_FIRST_SIZE;
}

#ifdef WASM_STACK_STRESS
/* Byte that fills stack memory a stack does not hold. */
#define WASM__STACK_POISON 0xA5
#endif

/* Moves s's operand slots to a block of cap slots, keeping as many as fit, which must include those below s->sp;
 * returns 0 if out of memory. */
static int wasm__exec_stack_move_slots(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t cap) {
    size_t old_size = (size_t)s->cap_stack * sizeof(wasm__slot_t);
    size_t new_size;
    wasm__slot_t* slots;

    if (sizeof(wasm__slot_t) > SIZE_MAX / (size_t)cap) return 0;
    new_size = (size_t)cap * sizeof(wasm__slot_t);
#ifdef WASM_STACK_STRESS
    slots = (wasm__slot_t*)wasm__alloc_sized(&rt->allocator, new_size, WASM__ALIGNOF(wasm__slot_t));
    if (!slots) return 0;
    memset(slots, WASM__STACK_POISON, new_size);
    if (s->stack) {
        memcpy(slots, s->stack, old_size < new_size ? old_size : new_size);
        memset(s->stack, WASM__STACK_POISON, old_size);
    }
    wasm__free_sized(&rt->allocator, s->stack, old_size, WASM__ALIGNOF(wasm__slot_t));
#else
    slots = (wasm__slot_t*)wasm__resize_sized(&rt->allocator, s->stack, old_size, new_size,
                                               WASM__ALIGNOF(wasm__slot_t));
    if (!slots) return 0;
#endif
    s->stack = slots;
    s->cap_stack = cap;
    return 1;
}

/* Moves s's call frames to an array of cap frames, which must hold s->call_frame_sp; returns 0 if out of memory. */
static int wasm__exec_stack_move_frames(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t cap) {
    size_t old_size = (size_t)s->cap_call_frames * sizeof(wasm__call_frame_t);
    size_t new_size;
    wasm__call_frame_t* frames;

    if (sizeof(wasm__call_frame_t) > SIZE_MAX / (size_t)cap) return 0;
    new_size = (size_t)cap * sizeof(wasm__call_frame_t);
#ifdef WASM_STACK_STRESS
    frames = (wasm__call_frame_t*)wasm__alloc_sized(&rt->allocator, new_size, WASM__ALIGNOF(wasm__call_frame_t));
    if (!frames) return 0;
    memset(frames, WASM__STACK_POISON, new_size);
    if (s->call_frames) {
        memcpy(frames, s->call_frames, old_size < new_size ? old_size : new_size);
        memset(s->call_frames, WASM__STACK_POISON, old_size);
    }
    wasm__free_sized(&rt->allocator, s->call_frames, old_size, WASM__ALIGNOF(wasm__call_frame_t));
#else
    frames = (wasm__call_frame_t*)wasm__resize_sized(&rt->allocator, s->call_frames, old_size, new_size,
                                                      WASM__ALIGNOF(wasm__call_frame_t));
    if (!frames) return 0;
#endif
    s->call_frames = frames;
    s->cap_call_frames = cap;
    return 1;
}

/* Returns the capacity after cap that holds need, doubling up to max. */
static uint32_t wasm__exec_stack_next_cap(uint32_t cap, uint32_t need, uint32_t max) {
#ifdef WASM_STACK_STRESS
    (void)max;
    return need > cap ? need : cap;
#else
    uint32_t next = cap > max / 2u ? max : cap * 2u;

    return next < need ? need : next;
#endif
}

WASM_PRIVATE wasm_error_t wasm__exec_stack_init(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t init_stack,
                                                uint32_t max_stack, uint32_t init_call_frames,
                                                uint32_t max_call_frames, size_t frame_arena_size) {
    memset(s, 0, sizeof(*s));
    if (max_stack == 0 || max_call_frames == 0) return WASM_ERR_MALFORMED;
    if (init_stack == 0 || init_stack > max_stack) init_stack = max_stack;
    if (init_call_frames == 0 || init_call_frames > max_call_frames) init_call_frames = max_call_frames;
#ifdef WASM_STACK_STRESS
    init_stack = 1u;
    init_call_frames = 1u;
#endif
    s->max_stack = max_stack;
    s->max_call_frames = max_call_frames;
    if (!wasm__exec_stack_move_slots(rt, s, init_stack) || !wasm__exec_stack_move_frames(rt, s, init_call_frames) ||
        wasm__arena_init(rt, s, frame_arena_size) != WASM_OK) {
        wasm__exec_stack_destroy(rt, s);
        return WASM_ERR_OOM;
    }
    return WASM_OK;
}

WASM_PRIVATE void wasm__exec_stack_destroy(wasm_runtime_t* rt, wasm__exec_stack_t* s) {
    wasm__arena_destroy(rt, s);
    wasm__free_sized(&rt->allocator, s->call_frames, (size_t)s->cap_call_frames * sizeof(*s->call_frames),
                     WASM__ALIGNOF(wasm__call_frame_t));
    wasm__free_sized(&rt->allocator, s->stack, (size_t)s->cap_stack * sizeof(*s->stack), WASM__ALIGNOF(wasm__slot_t));
    memset(s, 0, sizeof(*s));
}

WASM_PRIVATE size_t wasm__exec_stack_bytes(const wasm__exec_stack_t* s) {
    return (size_t)s->cap_stack * sizeof(wasm__slot_t) + (size_t)s->cap_call_frames * sizeof(wasm__call_frame_t) +
           wasm__arena_first_size(s);
}

WASM_PRIVATE wasm_error_t wasm__exec_stack_grow(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t slots) {
    size_t old_bytes = wasm__exec_stack_bytes(s);

    if (slots > s->max_stack - s->sp) return WASM_ERR_STACK_OVERFLOW;
#ifdef WASM_STACK_STRESS
    if (slots == 0) return WASM_OK;
#else
    if (slots <= s->cap_stack - s->sp) return WASM_OK;
#endif
    if (!wasm__exec_stack_move_slots(rt, s, wasm__exec_stack_next_cap(s->cap_stack, s->sp + slots, s->max_stack))) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "out of memory for operand stack");
        return WASM_ERR_OOM;
    }
    if (s != &rt->main_stack) rt->cont_stack_bytes += wasm__exec_stack_bytes(s) - old_bytes;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__exec_stack_grow_frames(wasm_runtime_t* rt, wasm__exec_stack_t* s) {
    size_t old_bytes = wasm__exec_stack_bytes(s);

    if (s->call_frame_sp >= s->max_call_frames) return WASM_ERR_CALL_STACK_EXHAUSTED;
#ifndef WASM_STACK_STRESS
    if (s->call_frame_sp < s->cap_call_frames) return WASM_OK;
#endif
    if (!wasm__exec_stack_move_frames(
            rt, s, wasm__exec_stack_next_cap(s->cap_call_frames, s->call_frame_sp + 1u, s->max_call_frames))) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "%s", "out of memory for call frames");
        return WASM_ERR_OOM;
    }
    if (s != &rt->main_stack) rt->cont_stack_bytes += wasm__exec_stack_bytes(s) - old_bytes;
    return WASM_OK;
}

WASM_PRIVATE void wasm__exec_stack_trim(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t max_slots,
                                        uint32_t max_frames) {
    if (s->cap_stack > max_slots) (void)wasm__exec_stack_move_slots(rt, s, max_slots);
    if (s->cap_call_frames > max_frames) (void)wasm__exec_stack_move_frames(rt, s, max_frames);
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
    size_t size = wasm__arena_first_size(s);

    if (s->frame_arena) size = s->frame_arena->size > s->frame_arena_size / 2u ? s->frame_arena_size
                                                                                : s->frame_arena->size * 2u;
    if (size < min_size) size = min_size;

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
    return wasm__arena_new_chunk(rt, s, 0) ? WASM_OK : WASM_ERR_OOM;
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

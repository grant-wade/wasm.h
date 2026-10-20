#ifndef WASM_SRC_EXEC_STACK_H
#define WASM_SRC_EXEC_STACK_H

#include "../internal/config.h"
#include "../internal/slot.h"

/* One block of the frame arena. Blocks are added as calls nest deeper and dropped as they return;
 * the block's storage follows this header. */
typedef struct wasm__arena_chunk_t {
    struct wasm__arena_chunk_t* prev;
    size_t size;
    size_t used;
} wasm__arena_chunk_t;

/* 1 when every push that could grow a stack moves it to a new block (WASM_STACK_STRESS), else 0. */
#ifdef WASM_STACK_STRESS
#define WASM__STACK_STRESS 1
#else
#define WASM__STACK_STRESS 0
#endif

/* A position in the frame arena for wasm__arena_reset to return to. */
typedef struct wasm__arena_mark_t {
    wasm__arena_chunk_t* chunk;
    size_t used;
} wasm__arena_mark_t;

/* Block storage starts this far into a block, and allocations are aligned to it. */
#define WASM__ARENA_ALIGN ((size_t)16u)
#define WASM__ARENA_HEADER \
    ((sizeof(wasm__arena_chunk_t) + WASM__ARENA_ALIGN - 1u) / WASM__ARENA_ALIGN * WASM__ARENA_ALIGN)

/* The operand slots, call frames, and frame arena that one line of execution runs on. */
typedef struct wasm__exec_stack_t {
    wasm__slot_t* stack;
    uint32_t cap_stack; /* slots allocated */
    uint32_t max_stack; /* slots it may grow to */
    uint32_t sp;
    wasm__call_frame_t* call_frames;
    uint32_t cap_call_frames; /* call frames allocated */
    uint32_t max_call_frames; /* call frames it may grow to */
    uint32_t call_frame_sp;
    wasm__arena_chunk_t* frame_arena; /* innermost block */
    size_t frame_arena_size;          /* size blocks double up to, unless a frame needs more */
    int parked;                       /* nonzero while the top frame is stopped at its pc */
    uint32_t frame_floor;              /* call frames on it while the frame whose return ends its run is on top */
    struct wasm__exec_stack_t* parent; /* the stack whose top frame resumed this one, while it runs */
    const uint32_t* handlers;          /* handler table of that resume */
    struct wasm__exec_stack_t* next;   /* next continuation stack in use, or in the pool */
    struct wasm__exec_stack_t* prev;   /* previous continuation stack in use */
    wasm_module_t* start_module;       /* owner of start_func, while no frame has been pushed */
    uint32_t start_func;               /* function a continuation that has not started calls */
} wasm__exec_stack_t;

/* Allocates init_stack slots and init_call_frames frames, each clamped to its maximum, or the maximum if 0. */
WASM_PRIVATE wasm_error_t wasm__exec_stack_init(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t init_stack,
                                                uint32_t max_stack, uint32_t init_call_frames,
                                                uint32_t max_call_frames, size_t frame_arena_size);

WASM_PRIVATE void wasm__exec_stack_destroy(wasm_runtime_t* rt, wasm__exec_stack_t* s);

/* Grows s to hold slots more slots above s->sp, keeping its contents. Returns WASM_ERR_STACK_OVERFLOW
 * without setting rt's error if that passes s->max_stack, or WASM_ERR_OOM with it set. */
WASM_PRIVATE wasm_error_t wasm__exec_stack_grow(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t slots);

/* Grows s to hold one more call frame. Returns WASM_ERR_CALL_STACK_EXHAUSTED without setting rt's error if
 * s holds s->max_call_frames, or WASM_ERR_OOM with it set. */
WASM_PRIVATE wasm_error_t wasm__exec_stack_grow_frames(wasm_runtime_t* rt, wasm__exec_stack_t* s);

/* Shrinks s's arrays to at most max_slots slots and max_frames call frames, which must hold its contents. */
WASM_PRIVATE void wasm__exec_stack_trim(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t max_slots,
                                        uint32_t max_frames);

/* Returns the bytes of s's operand slots and call frames plus one frame arena block. */
WASM_PRIVATE size_t wasm__exec_stack_bytes(const wasm__exec_stack_t* s);

/* As wasm__exec_stack_grow, but returns WASM_OK at once when s already has the room. */
WASM_INLINE wasm_error_t wasm__exec_stack_reserve(wasm_runtime_t* rt, wasm__exec_stack_t* s, uint32_t slots) {
#ifndef WASM_STACK_STRESS
    if (slots <= s->cap_stack - s->sp) return WASM_OK;
#endif
    return wasm__exec_stack_grow(rt, s, slots);
}

/* As wasm__exec_stack_grow_frames, but returns WASM_OK at once when s already has the room. */
WASM_INLINE wasm_error_t wasm__exec_stack_reserve_frame(wasm_runtime_t* rt, wasm__exec_stack_t* s) {
#ifndef WASM_STACK_STRESS
    if (s->call_frame_sp < s->cap_call_frames) return WASM_OK;
#endif
    return wasm__exec_stack_grow_frames(rt, s);
}

/* Returns the live stack after s, or the first one if s is NULL. */
WASM_PRIVATE wasm__exec_stack_t* wasm__next_live_stack(const wasm_runtime_t* rt, const wasm__exec_stack_t* s);

WASM_PRIVATE void wasm__arena_destroy(wasm_runtime_t* rt, wasm__exec_stack_t* s);

WASM_PRIVATE void wasm__arena_free_spare(wasm_runtime_t* rt);

WASM_PRIVATE wasm_error_t wasm__arena_init(wasm_runtime_t* rt, wasm__exec_stack_t* s, size_t size);

WASM_PRIVATE void* wasm__arena_alloc(wasm_runtime_t* rt, wasm__exec_stack_t* s, size_t size);

WASM_PRIVATE wasm__arena_mark_t wasm__arena_mark(const wasm__exec_stack_t* s);

WASM_PRIVATE void wasm__arena_reset_slow(wasm_runtime_t* rt, wasm__exec_stack_t* s, wasm__arena_mark_t mark);

WASM_INLINE void wasm__arena_reset(wasm_runtime_t* rt, wasm__exec_stack_t* s, wasm__arena_mark_t mark) {
    if (s->frame_arena == mark.chunk) {
        if (mark.chunk) mark.chunk->used = mark.used;
        return;
    }
    wasm__arena_reset_slow(rt, s, mark);
}

#endif /* WASM_SRC_EXEC_STACK_H */

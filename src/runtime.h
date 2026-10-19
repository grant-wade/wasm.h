#ifndef WASM_RUNTIME_H
#define WASM_RUNTIME_H

#include "internal/config.h"
#include <stdio.h>
#include "internal/gc_types.h"
#include "internal/slot.h"
#include "handles.h"

/* ── Portable snprintf ────────────────────────────────────────────── */
#if defined(_MSC_VER) && _MSC_VER < 1900
#define wasm__snprintf _snprintf
#else
#define wasm__snprintf snprintf
#endif

#define WASM__SET_ERR(rt, code, ...)                                 \
    do {                                                             \
        if (rt) {                                                    \
            (rt)->last_error = (code);                               \
            wasm__snprintf((rt)->error_msg, sizeof((rt)->error_msg), \
                           __VA_ARGS__);                             \
        }                                                            \
    } while (0)

/* One block of the frame arena. Blocks are added as calls nest deeper and dropped as they return;
 * the block's storage follows this header. */
typedef struct wasm__arena_chunk_t {
    struct wasm__arena_chunk_t* prev;
    size_t size;
    size_t used;
} wasm__arena_chunk_t;

/* A position in the frame arena for wasm__arena_reset to return to. */
typedef struct wasm__arena_mark_t {
    wasm__arena_chunk_t* chunk;
    size_t used;
} wasm__arena_mark_t;

/* Block storage starts this far into a block, and allocations are aligned to it. */
#define WASM__ARENA_ALIGN ((size_t)16u)
#define WASM__ARENA_HEADER \
    ((sizeof(wasm__arena_chunk_t) + WASM__ARENA_ALIGN - 1u) / WASM__ARENA_ALIGN * WASM__ARENA_ALIGN)

#define WASM__HELD_BLOCK_SLOTS 64u

/* Values the host keeps alive with wasm_ref_hold; blocks never move, so slot addresses are stable. */
typedef struct wasm__held_slot_t {
    wasm_value_t value;
    int in_use;
} wasm__held_slot_t;

typedef struct wasm__held_block_t {
    struct wasm__held_block_t* next;
    wasm__held_slot_t slots[WASM__HELD_BLOCK_SLOTS];
} wasm__held_block_t;

/* Values a C caller keeps alive across GC allocations; the collector marks and updates them. */
typedef struct wasm__gc_root_t {
    struct wasm__gc_root_t* prev;
    wasm_value_t* const* values; /* address of the array, which may be reallocated while pushed */
    const uint32_t* count;
} wasm__gc_root_t;

/* A function export registered by wasm_register_module, called through a host trampoline. */
typedef struct wasm__linked_func_t {
    struct wasm__linked_func_t* next;
    struct wasm_module_t* module; /* NULL once the module is freed */
    uint32_t func_idx;
} wasm__linked_func_t;

struct wasm_func_ref_t {
    struct wasm_module_t* module;
    uint32_t func_idx;
};

struct wasm_runtime_t {
    wasm_allocator_t allocator;
    wasm_import_t* imports;
    uint32_t num_imports;
    uint32_t cap_imports;
    wasm_func_ref_t* func_refs;
    uint32_t num_func_refs;
    uint32_t cap_func_refs;
    wasm_global_import_t* global_imports;
    uint32_t num_global_imports;
    uint32_t cap_global_imports;
    wasm_table_import_t* table_imports;
    uint32_t num_table_imports;
    uint32_t cap_table_imports;
    wasm_memory_import_t* memory_imports;
    uint32_t num_memory_imports;
    uint32_t cap_memory_imports;
    wasm_tag_import_t* tag_imports;
    uint32_t num_tag_imports;
    uint32_t cap_tag_imports;
    uint64_t next_tag_identity;
    wasm__slot_t* stack;
    uint32_t max_stack;
    uint32_t sp;
    uint64_t fuel;
    int fuel_enabled;
    size_t interrupt_requested; /* set by wasm_runtime_interrupt from any thread; accessed atomically */
    uint32_t enabled_features;
    int lazy_validation;
    uint32_t max_function_locals;
    uint64_t max_memory_bytes;
    uint64_t max_total_memory_bytes;
    uint64_t max_table_elements;
    wasm_error_t last_error;
    char error_msg[256];
    int32_t exit_code;
    wasm__exception_state_t pending_exception;
    wasm__handle_set_t exception_objects;
    uint8_t* gc_heap;
    size_t gc_heap_size;
    size_t gc_heap_offset;
    size_t gc_heap_min_size;    /* size collections never shrink the heap below */
    uint64_t max_gc_heap_bytes;
    wasm_gc_header_t* gc_allocations;
    wasm_module_t* gc_modules;
    wasm_module_t* failed_modules;
    wasm__handle_set_t externref_boxes;
    wasm__held_block_t* held_values;
    wasm__gc_root_t* gc_roots; /* innermost root pushed by wasm__gc_push_root */
    wasm__linked_func_t* linked_funcs;
    uint32_t handle_gc_threshold; /* boxes plus exception objects that trigger a collection */
    wasm__arena_chunk_t* frame_arena;       /* innermost block */
    wasm__arena_chunk_t* frame_arena_spare; /* last dropped block, kept for reuse */
    size_t frame_arena_size;                /* minimum block size */
    wasm__call_frame_t* call_frames;
    uint32_t max_call_frames;
    uint32_t call_frame_sp;
    wasm_error_t suspended;               /* the code a suspended call stopped with, or WASM_OK */
    wasm_module_t* yield_module;          /* owner of yield_type's type indices */
    const wasm_functype_t* yield_type;    /* type of the host import a suspended call yielded in */
    uint32_t nonresumable_depth;          /* host functions and start functions running */
    wasm_valtype_t* validator_stack;
    wasm_reftype_t* validator_stack_reftypes;
    wasm__val_frame_t* validator_frames;
    uint64_t* validator_local_init_bits;
    uint64_t* validator_local_init_entry_bits;
    size_t validator_local_init_capacity;
    uint32_t* validator_stack_slots; /* slot height below each validator stack entry */
    uint32_t* validator_stack_refs;  /* reference entries below each validator stack entry */
    uint32_t* validator_heights; /* slot height before each byte offset of the body being validated */
    uint8_t* validator_operand_types; /* type drop and select operate on, at their byte offsets */
    uint32_t validator_heights_capacity;
    uint32_t* validator_maps;     /* stack maps of the body being validated, as in wasm_func_t */
    uint32_t validator_maps_capacity;
    uint32_t* validator_map_bits;
    uint32_t validator_map_bits_capacity;
    uint32_t backtrace_depth;
    uint32_t max_labels;
    uint32_t* backtrace_func_indices;
    wasm_module_t** backtrace_modules;
    uint32_t* backtrace_offsets;
    wasm__wasi_state_t* wasi_state;
    int emscripten_stubs_enabled;
    wasm__emscripten_state_t* emscripten_state;
};

WASM_PRIVATE wasm_error_t wasm__runtime_init(wasm_runtime_t* rt, const wasm_allocator_t* allocator,
                                             const wasm_config_t* config);

WASM_PRIVATE void wasm__runtime_destroy(wasm_runtime_t* rt);

WASM_PRIVATE wasm_global_import_t* wasm__find_global_import(wasm_runtime_t* rt,
                                                      const char* module,
                                                      const char* name);

WASM_PRIVATE int wasm__has_import_named(const wasm_runtime_t* rt,
                                  const char* module,
                                  const char* name);

WASM_PRIVATE wasm_table_import_t* wasm__find_table_import(wasm_runtime_t* rt,
                                                    const char* module,
                                                    const char* name);

WASM_PRIVATE wasm_memory_import_t* wasm__find_memory_import(wasm_runtime_t* rt,
                                                      const char* module,
                                                      const char* name);

WASM_PRIVATE wasm_tag_import_t* wasm__find_tag_import(wasm_runtime_t* rt,
                                                const char* module,
                                                const char* name);

WASM_PRIVATE void wasm__gc_register_module(wasm_runtime_t* rt, wasm_module_t* mod);

WASM_PRIVATE void wasm__gc_unregister_module(wasm_runtime_t* rt, wasm_module_t* mod);

WASM_PRIVATE int wasm__gc_ptr_in_heap(const wasm_runtime_t* rt, const void* ptr);

WASM_PRIVATE wasm_error_t wasm__gc_heap_init(wasm_runtime_t* rt, size_t size);

/* Roots the first *count values of *values until the matching wasm__gc_pop_root. Pops must
 * happen in reverse order of pushes. */
WASM_PRIVATE void wasm__gc_push_root(wasm_runtime_t* rt, wasm__gc_root_t* root, wasm_value_t* const* values,
                                     const uint32_t* count);

WASM_PRIVATE void wasm__gc_pop_root(wasm_runtime_t* rt, wasm__gc_root_t* root);

/* Collects garbage once enough externref boxes and exception objects have accumulated. Call it
 * only where every live reference is reachable from the runtime's roots. */
WASM_PRIVATE void wasm__gc_collect_handles_if_needed(wasm_runtime_t* rt);

/* Consumes a pending wasm_runtime_interrupt; returns nonzero, with the error set, if there was one. */
WASM_PRIVATE int wasm__runtime_take_interrupt(wasm_runtime_t* rt);

WASM_PRIVATE wasm_gc_struct_t* wasm__gc_alloc_struct_object(wasm_runtime_t* rt,
                                                      const wasm_module_t* mod,
                                                      uint32_t type_index,
                                                      const wasm_structtype_t* type);

WASM_PRIVATE wasm_gc_array_t* wasm__gc_alloc_array_object(wasm_runtime_t* rt,
                                                    const wasm_module_t* mod,
                                                    uint32_t type_index,
                                                    const wasm_arraytype_t* type,
                                                    uint32_t length);

WASM_PRIVATE void wasm__arena_destroy(wasm_runtime_t* rt);

WASM_PRIVATE wasm_error_t wasm__arena_init(wasm_runtime_t* rt, size_t size);

WASM_PRIVATE void* wasm__arena_alloc(wasm_runtime_t* rt, size_t size);

WASM_PRIVATE wasm__arena_mark_t wasm__arena_mark(const wasm_runtime_t* rt);

WASM_PRIVATE void wasm__arena_reset_slow(wasm_runtime_t* rt, wasm__arena_mark_t mark);

WASM_INLINE void wasm__arena_reset(wasm_runtime_t* rt, wasm__arena_mark_t mark) {
    if (rt->frame_arena == mark.chunk) {
        if (mark.chunk) mark.chunk->used = mark.used;
        return;
    }
    wasm__arena_reset_slow(rt, mark);
}

WASM_PRIVATE wasm_error_t wasm__register_import_internal(wasm_runtime_t* rt,
                                                   const wasm_import_t* imp,
                                                   int zeroed_type_is_unspecified);

#endif /* WASM_RUNTIME_H */

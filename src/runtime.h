#ifndef WASM_RUNTIME_H
#define WASM_RUNTIME_H

#include "internal/config.h"
#include <stdio.h>
#include "internal/gc_types.h"
#include "internal/slot.h"
#include "handles.h"
#include "exec/stack.h"

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
    wasm_value_t* mirror; /* if not NULL, values as the host sees them, updated where a collection moves values */
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
    wasm__exec_stack_t* exec; /* the stack execution is running on */
    wasm__exec_stack_t* loop_stack; /* the stack the innermost interpreter loop started on */
    uint64_t fuel;
    int fuel_enabled;
    size_t interrupt_requested; /* set by wasm_runtime_interrupt from any thread; accessed atomically */
    uint64_t features;
    int lazy_validation;
    uint32_t max_function_locals;
    uint64_t max_memory_bytes;
    uint64_t max_total_memory_bytes;
    uint64_t max_table_elements;
    wasm_error_t last_error;
    char error_msg[256];
    int32_t exit_code;
    wasm__exception_state_t pending_exception;
    wasm__exn_table_t exns;      /* exceptions exnrefs name */
    wasm__exn_table_t held_exns; /* exceptions legacy catch clauses hold */
    wasm__cont_table_t conts;
    wasm__exec_stack_t* cont_stacks;     /* continuation stacks in use */
    wasm__exec_stack_t* cont_stack_pool; /* released continuation stacks kept for reuse */
    uint32_t num_cont_stacks;
    uint32_t num_pooled_cont_stacks;
    uint32_t max_cont_stacks;
    size_t cont_stack_bytes;  /* bytes of the continuation stacks in use, by wasm__exec_stack_bytes */
    size_t cont_gc_threshold; /* cont_stack_bytes at which starting another continuation collects first */
    uint32_t cont_initial_stack_values;
    uint32_t cont_stack_values;
    uint32_t cont_initial_call_depth;
    uint32_t cont_call_depth;
    size_t cont_frame_arena_size;
    wasm_module_t* type_owners; /* types of freed modules that GC objects or continuations have, linked by gc_next */
    wasm__type_group_t** type_groups; /* hash buckets of interned rec groups */
    uint32_t type_group_buckets;
    uint32_t num_type_groups;
    uint32_t next_type_id; /* canonical id of the next interned type */
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
    uint32_t handle_gc_threshold; /* boxes and continuations in use that trigger a collection */
    wasm__exec_stack_t main_stack;
    wasm__arena_chunk_t* frame_arena_spare; /* last dropped block, kept for reuse */
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
    uint32_t backtrace_capacity;
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

/* Collects garbage once enough externref boxes and continuations in use have accumulated, or when the exception
 * or continuation table needs one. Call it only where every live reference is reachable from the runtime's
 * roots. */
WASM_PRIVATE void wasm__gc_collect_handles_if_needed(wasm_runtime_t* rt);

/* Collects garbage as wasm__gc_collect_handles_if_needed does, or when the continuation table is full. */
WASM_PRIVATE void wasm__gc_collect_handles_before_cont(wasm_runtime_t* rt);

/* Consumes a pending wasm_runtime_interrupt; returns nonzero, with the error set, if there was one. */
WASM_PRIVATE int wasm__runtime_take_interrupt(wasm_runtime_t* rt);

/* A struct of a type with a descriptor gets a null descriptor slot for the caller to fill. */
WASM_PRIVATE wasm_gc_struct_t* wasm__gc_alloc_struct_object(wasm_runtime_t* rt,
                                                      const wasm_module_t* mod,
                                                      uint32_t type_index,
                                                      const wasm_structtype_t* type);

WASM_PRIVATE wasm_gc_array_t* wasm__gc_alloc_array_object(wasm_runtime_t* rt,
                                                    const wasm_module_t* mod,
                                                    uint32_t type_index,
                                                    const wasm_arraytype_t* type,
                                                    uint32_t length);

WASM_PRIVATE wasm_error_t wasm__register_import_internal(wasm_runtime_t* rt,
                                                   const wasm_import_t* imp,
                                                   int zeroed_type_is_unspecified);

#endif /* WASM_RUNTIME_H */

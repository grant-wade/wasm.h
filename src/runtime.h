#ifndef WASM_RUNTIME_H
#define WASM_RUNTIME_H

#include "internal/config.h"
#include <stdio.h>
#include "internal/gc_types.h"

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

struct wasm_func_ref_t {
    struct wasm_module_t* module;
    uint32_t func_idx;
};

struct wasm_runtime_t {
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
    wasm_value_t* stack;
    uint32_t max_stack;
    uint32_t sp;
    uint64_t fuel;
    int fuel_enabled;
    uint32_t enabled_features;
    int lazy_validation;
    uint32_t max_function_locals;
    uint64_t max_memory_bytes;
    uint64_t max_table_elements;
    wasm_error_t last_error;
    char error_msg[256];
    int32_t exit_code;
    wasm__exception_state_t pending_exception;
    wasm__exception_object_t* exception_objects;
    uint8_t* gc_heap;
    size_t gc_heap_size;
    size_t gc_heap_offset;
    wasm_gc_header_t* gc_allocations;
    wasm_module_t* gc_modules;
    wasm_module_t* failed_modules;
    wasm__externref_box_t* externref_boxes;
    uint8_t* frame_arena;
    size_t frame_arena_size;
    size_t frame_arena_offset;
    wasm__call_frame_t* call_frames;
    uint32_t max_call_frames;
    uint32_t call_frame_sp;
    wasm_valtype_t* validator_stack;
    wasm_reftype_t* validator_stack_reftypes;
    wasm__val_frame_t* validator_frames;
    uint64_t* validator_local_init_bits;
    uint64_t* validator_local_init_entry_bits;
    size_t validator_local_init_capacity;
    uint32_t backtrace_depth;
    uint32_t max_labels;
    uint32_t* backtrace_func_indices;
    uint32_t* backtrace_offsets;
    wasm__wasi_state_t* wasi_state;
    int emscripten_stubs_enabled;
    wasm__emscripten_state_t* emscripten_state;
};

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

WASM_PRIVATE void wasm__arena_reset(wasm_runtime_t* rt, size_t saved_offset);

WASM_PRIVATE wasm_error_t wasm__register_import_internal(wasm_runtime_t* rt,
                                                   const wasm_import_t* imp,
                                                   int zeroed_type_is_unspecified);

#endif /* WASM_RUNTIME_H */

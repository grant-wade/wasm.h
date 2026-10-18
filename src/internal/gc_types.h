#ifndef WASM_INTERNAL_GC_TYPES_H
#define WASM_INTERNAL_GC_TYPES_H

#include "config.h"

/* Shared heap object layouts used by allocation, tracing, and execution. */
#define WASM__GC_I31_TAG ((uintptr_t)1u)

typedef enum wasm_gc_object_kind_t {
    WASM_GC_OBJ_STRUCT = 1,
    WASM_GC_OBJ_ARRAY = 2
} wasm_gc_object_kind_t;

struct wasm_gc_header_t {
    wasm_gc_header_t* next_alloc;
    size_t object_size;
    wasm_module_t* module;
    uint32_t type_index;
    uint8_t kind;
    uint8_t marked;
    uint16_t reserved;
};

typedef struct wasm_gc_struct_t {
    wasm_gc_header_t header;
    uint8_t payload[1];
} wasm_gc_struct_t;

typedef struct wasm_gc_array_t {
    wasm_gc_header_t header;
    uint32_t length;
    uint8_t payload[1];
} wasm_gc_array_t;

#define WASM__GC_STRUCT_HEADER_SIZE offsetof(wasm_gc_struct_t, payload)
#define WASM__GC_ARRAY_HEADER_SIZE offsetof(wasm_gc_array_t, payload)

struct wasm__exception_object_t {
    uint32_t tag_index;
    uint64_t tag_identity;
    wasm_value_t* values;
    uint32_t num_values;
    int marked;
};

struct wasm__externref_box_t {
    wasm_value_t value;
    int marked;
};

#endif /* WASM_INTERNAL_GC_TYPES_H */

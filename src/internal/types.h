#ifndef WASM_INTERNAL_TYPES_H
#define WASM_INTERNAL_TYPES_H

#include "../wasm.h"

typedef struct wasm__val_frame_t wasm__val_frame_t;
typedef struct wasm__externref_box_t wasm__externref_box_t;
typedef struct wasm_func_t wasm_func_t;
typedef struct wasm_func_ref_t wasm_func_ref_t;
typedef struct wasm__call_frame_t wasm__call_frame_t;
typedef struct wasm__exception_object_t wasm__exception_object_t;
typedef struct wasm__emscripten_state_t wasm__emscripten_state_t;
typedef struct wasm__wasi_state_t wasm__wasi_state_t;

typedef struct wasm_structtype_t {
    uint32_t num_fields;
    wasm_fieldtype_t* fields;
} wasm_structtype_t;

typedef struct wasm_arraytype_t {
    wasm_fieldtype_t field;
} wasm_arraytype_t;

typedef struct wasm_comptype_t {
    wasm_comptype_kind_t kind;
    uint32_t num_supertypes;
    uint32_t* supertypes;
    uint32_t rec_group;
    uint32_t canonical_id;
    int is_final;
    union {
        wasm_functype_t func;
        wasm_structtype_t struct_;
        wasm_arraytype_t array;
    } of;
} wasm_comptype_t;

typedef struct wasm_recgroup_t {
    uint32_t first_type;
    uint32_t num_types;
} wasm_recgroup_t;

typedef struct wasm_export_t {
    char* name;
    uint32_t name_len;
    wasm_export_kind_t kind;
    uint32_t index;
} wasm_export_t;

typedef struct wasm_global_t {
    wasm_valtype_t type;
    wasm_reftype_t ref_type;
    int is_mutable;
    int is_import;
    wasm_value_t* import_value;
    wasm_value_t value;
} wasm_global_t;

struct wasm_table_t {
    wasm_valtype_t reftype;
    wasm_reftype_t reftype_info;
    wasm_value_t* elems;
    wasm_value_t default_value;
    uint32_t size;
    uint64_t max_size;
    int is_64;
    int is_import;
    struct wasm_table_t* import_table;
    wasm_allocator_t allocator; /* source of elems */
};

typedef struct wasm_data_segment_t {
    uint8_t* bytes;
    uint32_t size;
    uint32_t memory_index;
    uint64_t offset;
    int is_passive;
    int is_dropped;
} wasm_data_segment_t;

typedef struct wasm_elem_segment_t {
    wasm_valtype_t elem_type;
    wasm_reftype_t elem_ref_type;
    wasm_value_t* elems;
    uint32_t num_elems;
    uint32_t table_index;
    uint64_t offset;
    int uses_expr;
    int is_passive;
    int is_declarative;
    int is_dropped;
} wasm_elem_segment_t;

struct wasm_memory_t {
    uint8_t* data;
    uint64_t pages;
    uint64_t max_pages;
    int is_64;
    int is_import;
    struct wasm_memory_t* import_memory;
    uint32_t page_size; /* bytes per page, 1 or WASM_PAGE_SIZE; 0 means WASM_PAGE_SIZE */
    int is_shared;          /* shared memories never move and may be used by runtimes on other threads */
    uint64_t reserved_bytes; /* shared memories: size of the fixed reservation backing data */
    size_t shared_size;      /* shared memories: current size in bytes, read atomically while others grow it */
    wasm_allocator_t allocator; /* source of data for memories that are not shared */
};

typedef struct wasm_tag_t {
    uint32_t type_idx;
    uint64_t identity;
} wasm_tag_t;

typedef struct wasm_custom_section_t {
    char* name;
    const uint8_t* data;
    uint32_t size;
} wasm_custom_section_t;

typedef struct wasm__exception_state_t {
    uint32_t tag_index;
    uint64_t tag_identity;
    wasm_value_t* values;
    uint32_t num_values;
    int is_pending;
} wasm__exception_state_t;

#endif /* WASM_INTERNAL_TYPES_H */

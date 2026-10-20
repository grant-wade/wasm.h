#ifndef WASM_INTERNAL_TYPES_H
#define WASM_INTERNAL_TYPES_H

#include "../wasm.h"

typedef struct wasm__val_frame_t wasm__val_frame_t;
typedef struct wasm__externref_box_t wasm__externref_box_t;
typedef struct wasm_func_t wasm_func_t;
typedef struct wasm_func_ref_t wasm_func_ref_t;
typedef struct wasm__call_frame_t wasm__call_frame_t;
typedef struct wasm__exception_object_t wasm__exception_object_t;
typedef struct wasm__cont_object_t wasm__cont_object_t;
typedef struct wasm__emscripten_state_t wasm__emscripten_state_t;
typedef struct wasm__wasi_state_t wasm__wasi_state_t;
typedef struct wasm__type_group_t wasm__type_group_t;

typedef struct wasm_structtype_t {
    uint32_t num_fields;
    uint32_t num_ref_fields;
    wasm_fieldtype_t* fields;
    uint32_t* field_offsets;    /* byte offset of each field in an object; ref_offsets and ref_kinds point into the same block */
    uint32_t* ref_offsets;      /* byte offsets of the fields the collector traces, ascending */
    uint32_t* ref_kinds;        /* the WASM__SLOT_KIND_* of each field in ref_offsets */
    uint32_t object_size;       /* bytes of an object, with its descriptor slot; 0 if too large to allocate */
    uint32_t descriptor_offset; /* byte offset of the descriptor slot, or 0 for a type without a descriptor */
} wasm_structtype_t;

typedef struct wasm_arraytype_t {
    wasm_fieldtype_t field;
    uint32_t data_offset;    /* byte offset of the first element in an object */
    uint8_t elem_size;       /* bytes each element occupies in an object */
    uint8_t elem_align;
    uint8_t elem_ref_kind;   /* WASM__SLOT_KIND_* of the elements */
} wasm_arraytype_t;

typedef struct wasm_conttype_t {
    uint32_t func_type; /* index of the function type the continuation takes and returns */
} wasm_conttype_t;

typedef struct wasm_comptype_t {
    wasm_comptype_kind_t kind;
    uint32_t num_supertypes;
    uint32_t* supertypes;
    uint32_t rec_group;
    uint32_t canonical_id; /* the same for equal types of a runtime's modules, or 0 */
    uint32_t depth;        /* supertypes above the type, if display is not NULL */
    uint32_t* display;     /* canonical ids of the type's supertypes by depth, ending with its own, or NULL */
    int is_final;
    uint32_t descriptor; /* index of the type describing this one, or UINT32_MAX */
    uint32_t describes;  /* index of the type this one describes, or UINT32_MAX */
    union {
        wasm_functype_t func;
        wasm_structtype_t struct_;
        wasm_arraytype_t array;
        wasm_conttype_t cont;
    } of;
} wasm_comptype_t;

typedef struct wasm_recgroup_t {
    uint32_t first_type;
    uint32_t num_types;
    wasm__type_group_t* canonical; /* the group interned in the runtime, or NULL */
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
    int is_host; /* made by wasm_table_new */
    struct wasm_table_t* import_table;
    const wasm_module_t* type_module; /* module whose type indices reftype_info has, or NULL */
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

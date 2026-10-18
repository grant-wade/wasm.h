#ifndef WASM_MODULE_H
#define WASM_MODULE_H

#include "internal/config.h"

typedef struct wasm__control_target_t {
    uint32_t end_offset;
    uint32_t aux_offset;
} wasm__control_target_t;

struct wasm_func_t {
    uint32_t type_idx;
    uint32_t num_locals;
    wasm_valtype_t* locals;
    wasm_reftype_t* local_reftypes;
    const uint8_t* code;
    uint32_t code_len;
    wasm__control_target_t* control_targets;
    uint32_t max_label_depth;
    uint32_t funcref_id;
    int is_declared_ref;
    int is_import;
    wasm_host_func_t host_func;
    void* host_userdata;
};

struct wasm_module_t {
    wasm_runtime_t* rt;
    void* host_userdata;
    uint8_t* module_bytes;
    size_t module_size;
    wasm_comptype_t* types;
    uint32_t num_types;
    wasm_recgroup_t* rec_groups;
    uint32_t num_rec_groups;
    wasm_func_t* funcs;
    uint32_t num_funcs;
    wasm_import_info_t* imports;
    uint32_t num_imports;
    wasm_export_t* exports;
    uint32_t num_exports;
    wasm_global_t* globals;
    uint32_t num_globals;
    wasm_tag_t* tags;
    uint32_t num_tags;
    wasm_table_t* tables;
    uint32_t num_tables;
    wasm_memory_t* memories;
    wasm_custom_section_t* custom_sections;
    uint32_t num_custom_sections;
    char** func_names; /* from the "name" section, read on first use; NULL entries are unnamed */
    char* module_name;
    int names_loaded;
    wasm_data_segment_t* data_segments;
    uint32_t num_data_segments;
    wasm_elem_segment_t* elem_segments;
    uint32_t num_elem_segments;
    uint32_t num_func_imports;
    uint32_t num_global_imports;
    uint32_t num_memories;
    uint32_t num_code_bodies;
    uint32_t declared_data_count;
    uint32_t required_features;
    int has_data_count_section;
    int uses_data_count_section;
    int preserve_on_failure;
    int is_compiled; /* from wasm_compile: validated against placeholder imports, never instantiated */
    int init_expr_read_placeholder; /* an init expression read a compiled module's placeholder global */
    uint32_t loaded_features;       /* runtime features enabled when the module was decoded */
    uint32_t start_func;
    int startup_ran;
    wasm_module_t* gc_prev;
    wasm_module_t* gc_next;
    wasm_module_t* failed_next;
};

WASM_PRIVATE wasm_error_t wasm__resolve_exported_global(wasm_module_t* mod,
                                                  const char* name,
                                                  wasm_global_t** out_global,
                                                  uint32_t* out_index);

WASM_PRIVATE wasm_value_t wasm__global_get_value(const wasm_global_t* global);

WASM_PRIVATE void wasm__global_set_value(wasm_global_t* global, wasm_value_t value);

WASM_PRIVATE int32_t wasm__wrap_i32_add(int32_t lhs, int32_t rhs);

WASM_PRIVATE int32_t wasm__wrap_i32_sub(int32_t lhs, int32_t rhs);

WASM_PRIVATE int32_t wasm__wrap_i32_mul(int32_t lhs, int32_t rhs);

WASM_PRIVATE int64_t wasm__wrap_i64_add(int64_t lhs, int64_t rhs);

WASM_PRIVATE int64_t wasm__wrap_i64_sub(int64_t lhs, int64_t rhs);

WASM_PRIVATE int64_t wasm__wrap_i64_mul(int64_t lhs, int64_t rhs);

WASM_PRIVATE wasm_error_t wasm__append_funcs(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE void wasm__free_import_info(wasm_import_info_t* info);

WASM_PRIVATE wasm_error_t wasm__append_import_infos(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE wasm_error_t wasm__append_global_slots(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE void wasm__free_exception_state(wasm__exception_state_t* ex);

WASM_PRIVATE wasm_error_t wasm__set_exception_state(wasm__exception_state_t* ex,
                                              uint32_t tag_index,
                                              uint64_t tag_identity,
                                              const wasm_value_t* values,
                                              uint32_t count);

WASM_PRIVATE uint64_t wasm__alloc_tag_identity(wasm_runtime_t* rt);

WASM_PRIVATE wasm_error_t wasm__new_exception_ref(wasm_runtime_t* rt,
                                            const wasm__exception_state_t* ex,
                                            wasm_value_t* out_value);

/* Returns NULL unless value refers to a live exception object of rt. */
WASM_PRIVATE wasm__exception_object_t* wasm__exception_object_from_exnref(const wasm_runtime_t* rt,
                                                                       const wasm_value_t* value);

WASM_PRIVATE wasm_error_t wasm__append_tags(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE void wasm__free_data_segment(wasm_data_segment_t* segment);

WASM_PRIVATE wasm_table_t* wasm__table_actual(wasm_table_t* table);

WASM_PRIVATE const wasm_table_t* wasm__table_actual_const(const wasm_table_t* table);

WASM_PRIVATE int wasm__resolve_func_ref(const wasm_runtime_t* rt,
                                  uint32_t ref_id,
                                  wasm_module_t** out_module,
                                  uint32_t* out_func_idx);

WASM_PRIVATE void wasm__declare_func_ref(wasm_module_t* mod, uint32_t func_idx);

WASM_PRIVATE wasm_error_t wasm__make_funcref(wasm_module_t* mod,
                                       uint32_t func_idx,
                                       wasm_value_t* out_value);

#endif /* WASM_MODULE_H */

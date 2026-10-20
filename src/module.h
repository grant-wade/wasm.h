#ifndef WASM_MODULE_H
#define WASM_MODULE_H

#include "internal/config.h"

struct wasm_func_t {
    uint32_t type_idx;
    uint32_t num_locals;
    wasm_valtype_t* locals;
    wasm_reftype_t* local_reftypes;
    const uint8_t* code;
    uint32_t code_len;
    uint32_t* ops;        /* translated body (exec/translate.h), built once the body validates */
    uint32_t num_ops;
    uint32_t* op_offsets; /* for each translated instruction, its index in ops and bytecode offset */
    uint32_t num_op_offsets;
    uint32_t max_handlers; /* deepest nesting of try and try_table blocks */
    uint32_t max_stack;    /* operand stack slots the body needs at most */
    uint32_t* local_slots; /* slot offset of each local in the frame's locals */
    uint32_t num_local_slots;
    uint32_t frame_bytes; /* frame arena bytes a call reserves for its locals, then its handler labels */
    uint32_t param_slots;
    uint32_t result_slots;
    uint32_t* stack_maps;     /* bytecode offset, bits index, and slot count of each map, by offset */
    uint32_t num_stack_maps;
    uint32_t* stack_map_bits; /* WASM__SLOT_KIND_BITS-bit slot kinds the maps index */
    uint32_t num_stack_map_bits;
    uint32_t max_label_depth;
    uint32_t funcref_id;
    uint64_t matched_module; /* identity of the last module whose type matched_type an indirect call matched */
    uint32_t matched_type;
    int is_declared_ref;
    int is_import;
    int is_inexact; /* an import whose actual type may be a subtype of type_idx */
    int is_bound_exactly; /* an import bound to a function of exactly type_idx */
    int traces_params; /* an import some of whose parameters the collector traces */
    wasm_host_func_t host_func;
    void* host_userdata;
};

struct wasm_module_t {
    wasm_runtime_t* rt;
    uint64_t identity; /* unique among the modules of rt, never reused */
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
    uint64_t required_features;
    int has_data_count_section;
    int uses_data_count_section;
    int preserve_on_failure;
    int is_compiled; /* from wasm_compile: validated with unbound imports, never instantiated */
    uint64_t loaded_features;       /* runtime features enabled when the module was decoded */
    uint32_t start_func;
    int startup_ran;
    wasm_module_t* gc_prev;
    wasm_module_t* gc_next;
    wasm_module_t* failed_next;
    int retired_types;      /* holds only the types of a freed module that GC objects or continuations have */
    int retired_types_used; /* during a collection, whether a live object or continuation has them */
};

WASM_PRIVATE wasm_error_t wasm__resolve_exported_global(const wasm_module_t* mod,
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

WASM_PRIVATE void wasm__free_import_info(const wasm_allocator_t* a, wasm_import_info_t* info);

WASM_PRIVATE wasm_error_t wasm__append_import_infos(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE wasm_error_t wasm__append_global_slots(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE void wasm__free_exception_state(const wasm_allocator_t* a, wasm__exception_state_t* ex);

WASM_PRIVATE wasm_error_t wasm__set_exception_state(const wasm_allocator_t* a,
                                              wasm__exception_state_t* ex,
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

WASM_PRIVATE void wasm__free_data_segment(const wasm_allocator_t* a, wasm_data_segment_t* segment);

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

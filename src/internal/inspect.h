#ifndef WASM_INTERNAL_INSPECT_H
#define WASM_INTERNAL_INSPECT_H

#include "exec.h"

/* ── Memory helpers ───────────────────────────────────────────────── */

uint32_t wasm_memory_count(wasm_module_t* mod);

wasm_error_t wasm_memory_read(wasm_module_t* mod, uint32_t memory_index,
                              uint64_t offset, void* dst, size_t len);

wasm_error_t wasm_memory_write(wasm_module_t* mod, uint32_t memory_index,
                               uint64_t offset, const void* src, size_t len);

wasm_error_t wasm_memory_get_string(wasm_module_t* mod, uint32_t memory_index,
                                    uint64_t offset, char* dst, size_t max_len);

uint8_t* wasm_memory_data_at(wasm_module_t* mod, uint32_t memory_index);

uint64_t wasm_memory_size_at(wasm_module_t* mod, uint32_t memory_index);

int64_t wasm_memory_grow_at(wasm_module_t* mod, uint32_t memory_index, uint64_t delta_pages);

wasm_memory_t* wasm_memory_ref_at(wasm_module_t* mod, uint32_t memory_index);

uint8_t* wasm_memory_data(wasm_module_t* mod);
uint64_t wasm_memory_size(wasm_module_t* mod);

int64_t wasm_memory_grow(wasm_module_t* mod, uint64_t delta_pages);

uint32_t wasm_table_count(wasm_module_t* mod);

wasm_table_t* wasm_table_ref_at(wasm_module_t* mod, uint32_t table_idx);

wasm_valtype_t wasm_table_type(wasm_module_t* mod, uint32_t table_idx);

const wasm_reftype_t* wasm_table_reftype(wasm_module_t* mod, uint32_t table_idx);

uint32_t wasm_global_count(wasm_module_t* mod);

wasm_valtype_t wasm_global_type(wasm_module_t* mod, uint32_t global_idx);

const wasm_reftype_t* wasm_global_reftype(wasm_module_t* mod, uint32_t global_idx);

int wasm_global_is_mutable(wasm_module_t* mod, uint32_t global_idx);

wasm_value_t* wasm_global_value_ref_at(wasm_module_t* mod, uint32_t global_idx);

wasm_error_t wasm_global_get(wasm_module_t* mod, const char* name, wasm_value_t* out_val);

wasm_error_t wasm_global_set(wasm_module_t* mod, const char* name, wasm_value_t val);

uint32_t wasm_export_count(wasm_module_t* mod);

uint32_t wasm_import_count(wasm_module_t* mod);

const wasm_import_info_t* wasm_import_info(wasm_module_t* mod, uint32_t index);

const char* wasm_export_name(wasm_module_t* mod, uint32_t index);

const uint8_t* wasm_export_name_bytes(wasm_module_t* mod, uint32_t index, size_t* out_len);

wasm_export_kind_t wasm_export_kind(wasm_module_t* mod, uint32_t index);

uint32_t wasm_export_index(wasm_module_t* mod, uint32_t export_index);

int wasm_find_export(wasm_module_t* mod, const char* name,
                     wasm_export_kind_t* out_kind, uint32_t* out_index);

int wasm_find_export_bytes(wasm_module_t* mod, const uint8_t* name, size_t name_len,
                           wasm_export_kind_t* out_kind, uint32_t* out_index);

uint32_t wasm_custom_section_count(wasm_module_t* mod);

const char* wasm_custom_section_name(wasm_module_t* mod, uint32_t index);

const uint8_t* wasm_custom_section_data(wasm_module_t* mod, uint32_t index, size_t* out_len);

const uint8_t* wasm_custom_section_find(wasm_module_t* mod, const char* name, size_t* out_len);

uint32_t wasm_type_count(wasm_module_t* mod);

wasm_comptype_kind_t wasm_type_kind(wasm_module_t* mod, uint32_t type_idx);

const wasm_functype_t* wasm_type_functype(wasm_module_t* mod, uint32_t type_idx);

uint32_t wasm_struct_type_field_count(wasm_module_t* mod, uint32_t type_idx);

const wasm_fieldtype_t* wasm_struct_type_field(wasm_module_t* mod, uint32_t type_idx,
                                               uint32_t field_idx);

const wasm_fieldtype_t* wasm_array_type_field(wasm_module_t* mod, uint32_t type_idx);

uint32_t wasm_func_count(wasm_module_t* mod);

const wasm_functype_t* wasm_func_functype(wasm_module_t* mod, uint32_t func_idx);

uint32_t wasm_func_param_count(wasm_module_t* mod, uint32_t func_idx);

uint32_t wasm_func_result_count(wasm_module_t* mod, uint32_t func_idx);

wasm_valtype_t wasm_func_param_type(wasm_module_t* mod, uint32_t func_idx,
                                    uint32_t param_idx);

wasm_valtype_t wasm_func_result_type(wasm_module_t* mod, uint32_t func_idx,
                                     uint32_t result_idx);

uint32_t wasm_tag_count(wasm_module_t* mod);

const wasm_functype_t* wasm_tag_functype(wasm_module_t* mod, uint32_t tag_idx);

WASM_PRIVATE const char* wasm__gc_kind_name(wasm_gc_object_kind_t kind);

WASM_PRIVATE wasm_error_t wasm__public_resolve_gc_object(wasm_module_t* mod,
                                                   const wasm_value_t* ref,
                                                   wasm_gc_object_kind_t expected_kind,
                                                   const wasm_gc_header_t** out_object);

wasm_error_t wasm_struct_get_field(wasm_module_t* mod, wasm_value_t struct_ref,
                                   uint32_t field_idx, wasm_value_t* out_val);

wasm_error_t wasm_struct_set_field(wasm_module_t* mod, wasm_value_t struct_ref,
                                   uint32_t field_idx, wasm_value_t val);

wasm_error_t wasm_array_length(wasm_module_t* mod, wasm_value_t array_ref,
                               uint32_t* out_len);

wasm_error_t wasm_array_get_elem(wasm_module_t* mod, wasm_value_t array_ref,
                                 uint32_t index, wasm_value_t* out_val);

wasm_error_t wasm_array_set_elem(wasm_module_t* mod, wasm_value_t array_ref,
                                 uint32_t index, wasm_value_t val);

void wasm_dump_backtrace(wasm_runtime_t* rt, char* buffer, size_t buffer_size);

uint32_t wasm_get_call_stack_depth(wasm_runtime_t* rt);

wasm_error_t wasm_get_call_frame_info(wasm_runtime_t* rt, uint32_t depth,
                                      uint32_t* out_func_idx, uint32_t* out_offset);

void wasm_set_fuel(wasm_runtime_t* rt, uint64_t fuel);

uint64_t wasm_get_fuel(wasm_runtime_t* rt);

const wasm_runtime_t* wasm_module_runtime(const wasm_module_t* mod);

void wasm_module_set_userdata(wasm_module_t* mod, void* userdata);

void* wasm_module_get_userdata(wasm_module_t* mod);

wasm_error_t wasm_struct_new(wasm_module_t* mod, uint32_t type_idx,
                             const wasm_value_t* field_values, uint32_t num_fields,
                             wasm_value_t* out_ref);

wasm_error_t wasm_array_new(wasm_module_t* mod, uint32_t type_idx,
                            const wasm_value_t* elem_values, uint32_t num_elems,
                            wasm_value_t* out_ref);

const char* wasm_error_string(wasm_error_t err);


#endif /* WASM_INTERNAL_INSPECT_H */

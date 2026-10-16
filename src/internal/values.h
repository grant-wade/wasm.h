#ifndef WASM_INTERNAL_VALUES_H
#define WASM_INTERNAL_VALUES_H

#include "common.h"

WASM_PRIVATE wasm_value_t wasm__f32_bits(uint32_t bits);
WASM_PRIVATE wasm_value_t wasm__f64_bits(uint64_t bits);

/* ── LEB128 decoding ─────────────────────────────────────────────── */
typedef struct wasm__reader_t {
    const uint8_t* ptr;
    const uint8_t* end;
    int malformed;
} wasm__reader_t;

WASM_PRIVATE int wasm__has(wasm__reader_t* r, size_t n);

WASM_PRIVATE uint8_t wasm__read_u8(wasm__reader_t* r);

WASM_PRIVATE uint32_t wasm__read_leb128_u32(wasm__reader_t* r);

WASM_PRIVATE int32_t wasm__read_leb128_i32(wasm__reader_t* r);

WASM_PRIVATE uint64_t wasm__read_leb128_u64(wasm__reader_t* r);

WASM_PRIVATE int64_t wasm__read_leb128_i64(wasm__reader_t* r);

WASM_PRIVATE wasm_value_t wasm__read_f32(wasm__reader_t* r);

WASM_PRIVATE wasm_value_t wasm__read_f64(wasm__reader_t* r);

WASM_PRIVATE int wasm__is_valid_utf8(const uint8_t* bytes, uint32_t len);

WASM_PRIVATE int wasm__bytes_equal(const void* lhs, size_t lhs_len, const void* rhs, size_t rhs_len);

WASM_PRIVATE wasm_error_t wasm__read_name_owned_len(wasm__reader_t* r, char** out_name, uint32_t* out_len);

WASM_PRIVATE wasm_error_t wasm__read_name_owned(wasm__reader_t* r, char** out_name);

WASM_PRIVATE int wasm__export_name_matches_bytes(const wasm_export_t* exp,
                                           const uint8_t* name,
                                           size_t name_len);

WASM_PRIVATE int wasm__export_name_matches_cstr(const wasm_export_t* exp, const char* name);

WASM_PRIVATE const char* wasm__export_name_cstr(const wasm_export_t* exp);

WASM_PRIVATE void wasm__free_export(wasm_export_t* exp);

WASM_PRIVATE char* wasm__strdup(const char* text);

WASM_PRIVATE wasm_value_t wasm__u32_bits(uint32_t value);

WASM_PRIVATE wasm_value_t wasm__u64_bits(uint64_t value);

#define WASM__GC_I31_TAG ((uintptr_t)1u)

WASM_PRIVATE uintptr_t wasm__gc_i31_encode(int32_t value);

WASM_PRIVATE int32_t wasm__gc_i31_get_s(uintptr_t bits);

WASM_PRIVATE uint32_t wasm__gc_i31_get_u(uintptr_t bits);

WASM_PRIVATE int wasm__runtime_value_matches_type(const wasm_module_t* mod,
                                            const wasm_value_t* value,
                                            wasm_valtype_t expected_type,
                                            const wasm_reftype_t* expected_ref);
WASM_PRIVATE wasm_value_t wasm__typed_ref_null(wasm_valtype_t type,
                                         const wasm_reftype_t* reftype);
WASM_PRIVATE int wasm__get_effective_reftype(const wasm_module_t* mod,
                                       wasm_valtype_t type,
                                       const wasm_reftype_t* ref_type,
                                       wasm_reftype_t* out_reftype);
WASM_PRIVATE int wasm__typed_valtype_is_subtype(const wasm_module_t* mod,
                                          wasm_valtype_t subtype,
                                          const wasm_reftype_t* subtype_ref,
                                          wasm_valtype_t supertype,
                                          const wasm_reftype_t* supertype_ref);
WASM_PRIVATE int wasm__typed_valtype_equal(const wasm_module_t* mod,
                                     wasm_valtype_t lhs,
                                     const wasm_reftype_t* lhs_ref,
                                     wasm_valtype_t rhs,
                                     const wasm_reftype_t* rhs_ref);
WASM_PRIVATE int wasm__functype_is_subtype(const wasm_module_t* mod,
                                     const wasm_functype_t* subtype,
                                     const wasm_functype_t* supertype);
WASM_PRIVATE int wasm__globaltype_matches_import(const wasm_module_t* mod,
                                           wasm_valtype_t actual_type,
                                           const wasm_reftype_t* actual_ref,
                                           int actual_mutable,
                                           wasm_valtype_t expected_type,
                                           const wasm_reftype_t* expected_ref,
                                           int expected_mutable);

WASM_PRIVATE size_t wasm__align_up_size(size_t value, size_t alignment);

WASM_PRIVATE size_t wasm__gc_field_storage_alignment(const wasm_fieldtype_t* field);

WASM_PRIVATE size_t wasm__gc_field_storage_size(const wasm_fieldtype_t* field);

WASM_PRIVATE size_t wasm__gc_struct_field_offset(const wasm_structtype_t* type, uint32_t field_index);

WASM_PRIVATE int wasm__gc_struct_size(const wasm_structtype_t* type, size_t* out_size);

WASM_PRIVATE size_t wasm__gc_array_data_offset(const wasm_arraytype_t* type);

WASM_PRIVATE int wasm__gc_array_total_size(const wasm_arraytype_t* type,
                                     uint32_t length,
                                     size_t* out_size);

WASM_PRIVATE wasm_value_t wasm__gc_field_default_value(const wasm_fieldtype_t* field);

WASM_PRIVATE wasm_error_t wasm__gc_load_field_bytes(const wasm_fieldtype_t* field,
                                              const void* src,
                                              int sign_extend,
                                              wasm_value_t* out_value);

WASM_PRIVATE wasm_error_t wasm__gc_store_field_bytes(const wasm_module_t* mod,
                                               const wasm_fieldtype_t* field,
                                               void* dst,
                                               wasm_value_t value);

WASM_PRIVATE void* wasm__gc_struct_field_ptr(wasm_gc_struct_t* object,
                                       const wasm_structtype_t* type,
                                       uint32_t field_index);

WASM_PRIVATE const void* wasm__gc_struct_field_const_ptr(const wasm_gc_struct_t* object,
                                                   const wasm_structtype_t* type,
                                                   uint32_t field_index);

WASM_PRIVATE_INLINE wasm_error_t wasm__gc_struct_get_field(const wasm_gc_struct_t* object,
                                                   const wasm_structtype_t* type,
                                                   uint32_t field_index,
                                                   int sign_extend,
                                                   wasm_value_t* out_value);

WASM_PRIVATE wasm_error_t wasm__gc_struct_set_field(wasm_gc_struct_t* object,
                                              const wasm_structtype_t* type,
                                              uint32_t field_index,
                                              wasm_value_t value);

WASM_PRIVATE wasm_error_t wasm__gc_struct_store_field(wasm_gc_struct_t* object,
                                                const wasm_structtype_t* type,
                                                uint32_t field_index,
                                                wasm_value_t value);

WASM_PRIVATE const void* wasm__gc_array_element_const_ptr(const wasm_gc_array_t* object,
                                                    const wasm_arraytype_t* type,
                                                    uint32_t index);

WASM_PRIVATE void* wasm__gc_array_element_ptr(wasm_gc_array_t* object,
                                        const wasm_arraytype_t* type,
                                        uint32_t index);

WASM_PRIVATE_INLINE wasm_error_t wasm__gc_array_get(const wasm_gc_array_t* object,
                                            const wasm_arraytype_t* type,
                                            uint32_t index,
                                            int sign_extend,
                                            wasm_value_t* out_value);

WASM_PRIVATE wasm_error_t wasm__gc_array_set(wasm_gc_array_t* object,
                                       const wasm_arraytype_t* type,
                                       uint32_t index,
                                       wasm_value_t value);

WASM_PRIVATE_INLINE uint32_t wasm__gc_array_len(const wasm_gc_array_t* object);

WASM_PRIVATE wasm_error_t wasm__gc_array_store_element(wasm_gc_array_t* object,
                                                 const wasm_arraytype_t* type,
                                                 uint32_t index,
                                                 wasm_value_t value);

WASM_PRIVATE wasm_value_t wasm__gc_type_ref_value(const wasm_module_t* mod,
                                            uint32_t type_index,
                                            wasm_gc_header_t* object);

WASM_PRIVATE int wasm__gc_array_data_element_size(const wasm_arraytype_t* type, size_t* out_size);

WASM_PRIVATE wasm_error_t wasm__gc_array_read_data_value(const wasm_arraytype_t* type,
                                                   const uint8_t* src,
                                                   wasm_value_t* out_value);

WASM_PRIVATE int wasm__is_gc_ref_type(wasm_valtype_t type);

WASM_PRIVATE int wasm__is_ref_type(wasm_valtype_t type);

WASM_PRIVATE int wasm__is_vector_type(wasm_valtype_t type);

WASM_PRIVATE int wasm__is_numeric_type(wasm_valtype_t type);

WASM_PRIVATE int wasm__is_number_or_vector_type(wasm_valtype_t type);

WASM_PRIVATE int wasm__is_value_type(wasm_valtype_t type);

WASM_PRIVATE int wasm__is_null_ref(const wasm_value_t* value);

WASM_PRIVATE int wasm__is_heap_gc_ref_value(const wasm_value_t* value);

WASM_PRIVATE int wasm__value_matches_type(const wasm_value_t* value, wasm_valtype_t type);

WASM_PRIVATE wasm_value_t wasm__typed_ref_null(wasm_valtype_t type,
                                         const wasm_reftype_t* reftype);

WASM_PRIVATE wasm_value_t wasm__retag_value_for_declared_type(wasm_valtype_t declared_type,
                                                        const wasm_reftype_t* declared_reftype,
                                                        wasm_value_t value);

WASM_PRIVATE wasm_value_t wasm__default_value(wasm_valtype_t type);

WASM_PRIVATE wasm_value_t wasm__trunc_sat_i32_s(double value);

WASM_PRIVATE wasm_value_t wasm__trunc_sat_i32_u(double value);

WASM_PRIVATE wasm_value_t wasm__trunc_sat_i64_s(double value);

WASM_PRIVATE wasm_value_t wasm__trunc_sat_i64_u(double value);

WASM_PRIVATE wasm_error_t wasm__trunc_i32_s(double value, wasm_value_t* out_value);

WASM_PRIVATE wasm_error_t wasm__trunc_i32_u(double value, wasm_value_t* out_value);

WASM_PRIVATE wasm_error_t wasm__trunc_i64_s(double value, wasm_value_t* out_value);

WASM_PRIVATE wasm_error_t wasm__trunc_i64_u(double value, wasm_value_t* out_value);

WASM_PRIVATE int wasm__f32_is_nan_bits(float value);

WASM_PRIVATE int wasm__f64_is_nan_bits(double value);

WASM_PRIVATE float wasm__quiet_f32_nan(float value);

WASM_PRIVATE double wasm__quiet_f64_nan(double value);

WASM_PRIVATE float wasm__f32_min(float lhs, float rhs);

WASM_PRIVATE float wasm__f32_max(float lhs, float rhs);

WASM_PRIVATE double wasm__f64_min(double lhs, double rhs);

WASM_PRIVATE double wasm__f64_max(double lhs, double rhs);

WASM_PRIVATE float wasm__ceil_f32(float value);

WASM_PRIVATE float wasm__floor_f32(float value);

WASM_PRIVATE float wasm__trunc_f32(float value);

WASM_PRIVATE float wasm__nearest_f32(float value);

WASM_PRIVATE float wasm__sqrt_f32(float value);

WASM_PRIVATE double wasm__ceil_f64(double value);

WASM_PRIVATE double wasm__floor_f64(double value);

WASM_PRIVATE double wasm__trunc_f64(double value);

WASM_PRIVATE double wasm__nearest_f64(double value);

WASM_PRIVATE double wasm__sqrt_f64(double value);

WASM_PRIVATE wasm_error_t wasm__require_valtype_feature(wasm_module_t* mod, wasm_valtype_t type);

WASM_PRIVATE void wasm__clear_reftype(wasm_reftype_t* reftype);

WASM_PRIVATE int wasm__is_absheaptype_byte(uint8_t byte);

WASM_PRIVATE int wasm__is_reftype_lead_byte(uint8_t byte);

WASM_PRIVATE wasm_valtype_t wasm__abstract_heap_byte_to_type(uint8_t byte);

WASM_PRIVATE wasm_error_t wasm__resolve_reftype(wasm_module_t* mod, wasm_reftype_t* reftype);

WASM_PRIVATE wasm_error_t wasm__resolve_valtype_reftype(wasm_module_t* mod,
                                                  wasm_valtype_t* type,
                                                  wasm_reftype_t* reftype);

WASM_PRIVATE int wasm__has_reftype_info(const wasm_reftype_t* reftype);

WASM_PRIVATE int wasm__reftype_equal(const wasm_reftype_t* lhs, const wasm_reftype_t* rhs);

WASM_PRIVATE int wasm__reftype_is_subtype(const wasm_module_t* mod,
                                    const wasm_reftype_t* subtype,
                                    const wasm_reftype_t* supertype);

WASM_PRIVATE const wasm_gc_header_t* wasm__runtime_get_gc_object(const wasm_module_t* mod,
                                                           const wasm_value_t* value);

WASM_PRIVATE wasm__externref_box_t* wasm__find_externref_box(const wasm_runtime_t* rt,
                                                       uintptr_t handle);

WASM_PRIVATE int wasm__is_host_anyref_value(const wasm_module_t* mod,
                                      const wasm_value_t* value);

WASM_PRIVATE wasm_error_t wasm__externref_from_anyref_value(wasm_runtime_t* rt,
                                                      wasm_module_t* mod,
                                                      wasm_value_t value,
                                                      wasm_value_t* out_value);

WASM_PRIVATE wasm_value_t wasm__anyref_from_externref_value(const wasm_runtime_t* rt,
                                                      wasm_value_t value);

WASM_PRIVATE int wasm__runtime_value_reftype(const wasm_module_t* mod,
                                       const wasm_value_t* value,
                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE int wasm__runtime_value_matches_type(const wasm_module_t* mod,
                                            const wasm_value_t* value,
                                            wasm_valtype_t expected_type,
                                            const wasm_reftype_t* expected_ref);

WASM_PRIVATE int wasm__elem_segment_values_match_type(const wasm_module_t* mod,
                                                const wasm_elem_segment_t* segment,
                                                wasm_valtype_t expected_type,
                                                const wasm_reftype_t* expected_ref);

WASM_PRIVATE int wasm__get_effective_reftype(const wasm_module_t* mod,
                                       wasm_valtype_t type,
                                       const wasm_reftype_t* ref_type,
                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE int wasm__typed_valtype_is_subtype(const wasm_module_t* mod,
                                          wasm_valtype_t subtype,
                                          const wasm_reftype_t* subtype_ref,
                                          wasm_valtype_t supertype,
                                          const wasm_reftype_t* supertype_ref);

WASM_PRIVATE int wasm__typed_valtype_equal(const wasm_module_t* mod,
                                     wasm_valtype_t lhs,
                                     const wasm_reftype_t* lhs_ref,
                                     wasm_valtype_t rhs,
                                     const wasm_reftype_t* rhs_ref);

WASM_PRIVATE int wasm__functype_is_subtype(const wasm_module_t* mod,
                                     const wasm_functype_t* subtype,
                                     const wasm_functype_t* supertype);

WASM_PRIVATE int wasm__globaltype_matches_import(const wasm_module_t* mod,
                                           wasm_valtype_t actual_type,
                                           const wasm_reftype_t* actual_ref,
                                           int actual_mutable,
                                           wasm_valtype_t expected_type,
                                           const wasm_reftype_t* expected_ref,
                                           int expected_mutable);

WASM_PRIVATE int wasm__limits_match_import(uint64_t actual_min,
                                     uint64_t actual_max,
                                     uint64_t expected_min,
                                     uint64_t expected_max);

WASM_PRIVATE int wasm__tabletype_matches_import(const wasm_module_t* mod,
                                          const wasm_table_t* actual_table,
                                          wasm_valtype_t expected_type,
                                          const wasm_reftype_t* expected_ref,
                                          uint64_t expected_min,
                                          uint64_t expected_max,
                                          int expected_is_64);

WASM_PRIVATE int wasm__memorytype_matches_import(const wasm_memory_t* actual_memory,
                                           uint64_t expected_min,
                                           uint64_t expected_max,
                                           int expected_is_64);

WASM_PRIVATE wasm_error_t wasm__read_heaptype(wasm_module_t* mod,
                                        wasm__reader_t* r,
                                        wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__runtime_make_typeidx_reftype(wasm_module_t* mod,
                                                       uint32_t type_idx,
                                                       int nullable,
                                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__runtime_read_heap_reftype(wasm_module_t* mod,
                                                    wasm__reader_t* r,
                                                    int nullable,
                                                    wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_value_t wasm__runtime_cast_success_value(const wasm_reftype_t* target,
                                                     wasm_value_t value);

WASM_PRIVATE wasm_value_t wasm__runtime_cast_difference_value(const wasm_reftype_t* source,
                                                        const wasm_reftype_t* target,
                                                        wasm_value_t value);

WASM_PRIVATE wasm_error_t wasm__read_reftype(wasm_module_t* mod,
                                       wasm__reader_t* r,
                                       wasm_valtype_t* out_type,
                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__read_valtype(wasm_module_t* mod,
                                       wasm__reader_t* r,
                                       wasm_valtype_t* out_type,
                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__read_unresolved_valtype(wasm_module_t* mod,
                                                  wasm__reader_t* r,
                                                  wasm_valtype_t* out_type,
                                                  wasm_reftype_t* out_reftype);

WASM_PRIVATE void wasm__skip_heaptype(wasm__reader_t* r);

WASM_PRIVATE void wasm__skip_reftype(wasm__reader_t* r);

WASM_PRIVATE void wasm__skip_valtype(wasm__reader_t* r);

WASM_PRIVATE int wasm__is_packedtype_byte(uint8_t byte);

WASM_PRIVATE int wasm__is_valtype_byte(uint8_t byte);

WASM_PRIVATE void wasm__free_functype(wasm_functype_t* ft);

WASM_PRIVATE int wasm__functype_is_unspecified(const wasm_functype_t* ft);

WASM_PRIVATE int wasm__functype_is_zeroed(const wasm_functype_t* ft);

WASM_PRIVATE void wasm__functype_mark_unspecified(wasm_functype_t* ft);

WASM_PRIVATE void wasm__free_structtype(wasm_structtype_t* st);

WASM_PRIVATE void wasm__free_comptype(wasm_comptype_t* type);

WASM_PRIVATE wasm_error_t wasm__copy_functype(wasm_functype_t* dst, const wasm_functype_t* src);

WASM_PRIVATE int wasm__functype_equal(const wasm_functype_t* lhs, const wasm_functype_t* rhs);

WASM_PRIVATE int wasm__storagetype_equal(const wasm_storagetype_t* lhs, const wasm_storagetype_t* rhs);

WASM_PRIVATE int wasm__canonical_type_equal_visit(const wasm_module_t* mod,
                                            uint32_t lhs_idx,
                                            uint32_t rhs_idx,
                                            uint8_t* memo);

WASM_PRIVATE int wasm__canonical_group_contains_type(const wasm_recgroup_t* group,
                                               uint32_t type_idx);

WASM_PRIVATE uint32_t wasm__canonical_group_type_offset(const wasm_recgroup_t* group,
                                                  uint32_t type_idx);

WASM_PRIVATE int wasm__canonical_type_index_equal_in_groups(const wasm_module_t* mod,
                                                      uint32_t lhs_idx,
                                                      uint32_t rhs_idx,
                                                      const wasm_recgroup_t* lhs_group,
                                                      const wasm_recgroup_t* rhs_group,
                                                      uint8_t* memo);

WASM_PRIVATE int wasm__canonical_reftype_equal_in_groups(const wasm_module_t* mod,
                                                   const wasm_reftype_t* lhs,
                                                   const wasm_reftype_t* rhs,
                                                   const wasm_recgroup_t* lhs_group,
                                                   const wasm_recgroup_t* rhs_group,
                                                   uint8_t* memo);

WASM_PRIVATE int wasm__canonical_functype_equal_in_groups(const wasm_module_t* mod,
                                                    const wasm_functype_t* lhs,
                                                    const wasm_functype_t* rhs,
                                                    const wasm_recgroup_t* lhs_group,
                                                    const wasm_recgroup_t* rhs_group,
                                                    uint8_t* memo);

WASM_PRIVATE int wasm__canonical_storagetype_equal_in_groups(const wasm_module_t* mod,
                                                       const wasm_storagetype_t* lhs,
                                                       const wasm_storagetype_t* rhs,
                                                       const wasm_recgroup_t* lhs_group,
                                                       const wasm_recgroup_t* rhs_group,
                                                       uint8_t* memo);

WASM_PRIVATE int wasm__canonical_fieldtype_equal_in_groups(const wasm_module_t* mod,
                                                     const wasm_fieldtype_t* lhs,
                                                     const wasm_fieldtype_t* rhs,
                                                     const wasm_recgroup_t* lhs_group,
                                                     const wasm_recgroup_t* rhs_group,
                                                     uint8_t* memo);

WASM_PRIVATE int wasm__comptype_body_equal_in_groups(const wasm_module_t* mod,
                                               const wasm_comptype_t* lhs,
                                               const wasm_comptype_t* rhs,
                                               const wasm_recgroup_t* lhs_group,
                                               const wasm_recgroup_t* rhs_group,
                                               uint8_t* memo);

WASM_PRIVATE int wasm__comptype_equal_visit_in_groups(const wasm_module_t* mod,
                                                uint32_t lhs_idx,
                                                uint32_t rhs_idx,
                                                const wasm_recgroup_t* lhs_group,
                                                const wasm_recgroup_t* rhs_group,
                                                uint8_t* memo);

WASM_PRIVATE int wasm__canonical_recgroup_equal(const wasm_module_t* mod,
                                          const wasm_recgroup_t* lhs_group,
                                          const wasm_recgroup_t* rhs_group,
                                          uint8_t* memo);

WASM_PRIVATE int wasm__canonical_type_index_equal_in_groups(const wasm_module_t* mod,
                                                      uint32_t lhs_idx,
                                                      uint32_t rhs_idx,
                                                      const wasm_recgroup_t* lhs_group,
                                                      const wasm_recgroup_t* rhs_group,
                                                      uint8_t* memo);

WASM_PRIVATE int wasm__type_equal(const wasm_module_t* mod, uint32_t lhs_idx, uint32_t rhs_idx);

WASM_PRIVATE int wasm__canonical_type_equal_visit(const wasm_module_t* mod,
                                            uint32_t lhs_idx,
                                            uint32_t rhs_idx,
                                            uint8_t* memo);

WASM_PRIVATE int wasm__comptype_equal(const wasm_module_t* mod, uint32_t lhs_idx, uint32_t rhs_idx);

WASM_PRIVATE wasm_error_t wasm__canonicalize_types(wasm_module_t* mod);

WASM_PRIVATE int wasm__type_equal(const wasm_module_t* mod, uint32_t lhs_idx, uint32_t rhs_idx);

WASM_PRIVATE int wasm__is_heap_subtype(const wasm_module_t* mod,
                                 wasm_valtype_t subtype,
                                 wasm_valtype_t supertype);

WASM_PRIVATE int wasm__is_reftype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype);

WASM_PRIVATE int wasm__is_valtype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype);

WASM_PRIVATE int wasm__is_storagetype_subtype(const wasm_module_t* mod,
                                        const wasm_storagetype_t* subtype,
                                        const wasm_storagetype_t* supertype);

WASM_PRIVATE int wasm__is_fieldtype_subtype(const wasm_module_t* mod,
                                      const wasm_fieldtype_t* subtype,
                                      const wasm_fieldtype_t* supertype);

WASM_PRIVATE int wasm__is_comptype_immediate_subtype(const wasm_module_t* mod,
                                               const wasm_comptype_t* subtype,
                                               const wasm_comptype_t* supertype);

WASM_PRIVATE_INLINE int wasm__is_subtype(const wasm_module_t* mod,
                                   uint32_t subtype_idx,
                                   uint32_t supertype_idx);

WASM_PRIVATE wasm_functype_t* wasm__type_as_functype(wasm_comptype_t* type);

WASM_PRIVATE const wasm_functype_t* wasm__type_as_const_functype(const wasm_comptype_t* type);

WASM_PRIVATE wasm_functype_t* wasm__module_functype(wasm_module_t* mod, uint32_t type_idx);

WASM_PRIVATE const wasm_functype_t* wasm__module_const_functype(const wasm_module_t* mod, uint32_t type_idx);

WASM_PRIVATE const wasm_structtype_t* wasm__type_as_const_structtype(const wasm_comptype_t* type);

WASM_PRIVATE const wasm_arraytype_t* wasm__type_as_const_arraytype(const wasm_comptype_t* type);

WASM_PRIVATE const wasm_structtype_t* wasm__module_const_structtype(const wasm_module_t* mod, uint32_t type_idx);

WASM_PRIVATE const wasm_arraytype_t* wasm__module_const_arraytype(const wasm_module_t* mod, uint32_t type_idx);

WASM_PRIVATE wasm_gc_struct_t* wasm__gc_alloc_struct_object(wasm_runtime_t* rt,
                                                      const wasm_module_t* mod,
                                                      uint32_t type_index,
                                                      const wasm_structtype_t* type);
WASM_PRIVATE wasm_gc_array_t* wasm__gc_alloc_array_object(wasm_runtime_t* rt,
                                                    const wasm_module_t* mod,
                                                    uint32_t type_index,
                                                    const wasm_arraytype_t* type,
                                                    uint32_t length);

WASM_PRIVATE wasm_error_t wasm__append_types(wasm_module_t* mod, uint32_t count, uint32_t* out_base);

WASM_PRIVATE wasm_error_t wasm__append_rec_groups(wasm_module_t* mod, uint32_t count, uint32_t* out_base);

WASM_PRIVATE wasm_error_t wasm__decode_functype(wasm_module_t* mod, wasm__reader_t* r, wasm_functype_t* ft);

WASM_PRIVATE wasm_error_t wasm__read_storage_type(wasm_module_t* mod,
                                            wasm__reader_t* r,
                                            wasm_storagetype_t* out_type);

WASM_PRIVATE wasm_error_t wasm__decode_fieldtype(wasm_module_t* mod,
                                           wasm__reader_t* r,
                                           wasm_fieldtype_t* field);

WASM_PRIVATE wasm_error_t wasm__decode_comptype_body(wasm_module_t* mod,
                                               wasm__reader_t* r,
                                               uint8_t opcode,
                                               wasm_comptype_t* type);

WASM_PRIVATE wasm_error_t wasm__decode_type_entry(wasm_module_t* mod,
                                            wasm__reader_t* r,
                                            uint8_t opcode,
                                            uint32_t rec_group_idx,
                                            wasm_comptype_t* type);

WASM_PRIVATE wasm_value_t wasm__global_get_value(const wasm_global_t* global);

WASM_PRIVATE void wasm__global_set_value(wasm_global_t* global, wasm_value_t value);

typedef struct wasm__init_expr_stack_t {
    wasm_value_t* values;
    uint32_t size;
    uint32_t cap;
} wasm__init_expr_stack_t;

WASM_PRIVATE wasm_error_t wasm__init_expr_stack_push(wasm__init_expr_stack_t* stack, wasm_value_t value);

WASM_PRIVATE wasm_error_t wasm__init_expr_stack_pop_expect(wasm_module_t* mod,
                                                     wasm__init_expr_stack_t* stack,
                                                     wasm_valtype_t expected_type,
                                                     const wasm_reftype_t* expected_ref,
                                                     wasm_value_t* out_value);

WASM_PRIVATE wasm_error_t wasm__init_expr_stack_pop_typed(wasm__init_expr_stack_t* stack,
                                                    wasm_valtype_t expected_type,
                                                    wasm_value_t* out_value);

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

WASM_PRIVATE wasm__exception_object_t* wasm__exception_object_from_exnref(const wasm_value_t* value);

WASM_PRIVATE wasm_error_t wasm__append_tags(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE void wasm__free_data_segment(wasm_data_segment_t* segment);

WASM_PRIVATE wasm_table_t* wasm__table_actual(wasm_table_t* table);

WASM_PRIVATE const wasm_table_t* wasm__table_actual_const(const wasm_table_t* table);

WASM_PRIVATE wasm_error_t wasm__runtime_register_func_ref(wasm_runtime_t* rt,
                                                    wasm_module_t* mod,
                                                    uint32_t func_idx,
                                                    uint32_t* out_ref_id);

WASM_PRIVATE wasm_error_t wasm__ensure_func_ref_id(wasm_module_t* mod,
                                             uint32_t func_idx,
                                             uint32_t* out_ref_id);

WASM_PRIVATE int wasm__resolve_func_ref(const wasm_runtime_t* rt,
                                  uint32_t ref_id,
                                  wasm_module_t** out_module,
                                  uint32_t* out_func_idx);

WASM_PRIVATE void wasm__declare_func_ref(wasm_module_t* mod, uint32_t func_idx);

WASM_PRIVATE wasm_error_t wasm__make_funcref(wasm_module_t* mod,
                                       uint32_t func_idx,
                                       wasm_value_t* out_value);

WASM_PRIVATE int wasm__cross_group_contains_type(const wasm_recgroup_t* group, uint32_t type_idx);

WASM_PRIVATE uint32_t wasm__cross_group_type_offset(const wasm_recgroup_t* group, uint32_t type_idx);

WASM_PRIVATE int wasm__cross_type_equal_visit(const wasm_module_t* lhs_mod,
                                        uint32_t lhs_idx,
                                        const wasm_module_t* rhs_mod,
                                        uint32_t rhs_idx,
                                        uint8_t* memo);

WASM_PRIVATE int wasm__cross_type_is_subtype_with_memo(const wasm_module_t* lhs_mod,
                                                 uint32_t lhs_idx,
                                                 const wasm_module_t* rhs_mod,
                                                 uint32_t rhs_idx,
                                                 uint8_t* memo);

WASM_PRIVATE int wasm__cross_type_index_equal_in_groups(const wasm_module_t* lhs_mod,
                                                  uint32_t lhs_idx,
                                                  const wasm_recgroup_t* lhs_group,
                                                  const wasm_module_t* rhs_mod,
                                                  uint32_t rhs_idx,
                                                  const wasm_recgroup_t* rhs_group,
                                                  uint8_t* memo);

WASM_PRIVATE int wasm__cross_reftype_equal_in_groups(const wasm_module_t* lhs_mod,
                                               const wasm_reftype_t* lhs,
                                               const wasm_recgroup_t* lhs_group,
                                               const wasm_module_t* rhs_mod,
                                               const wasm_reftype_t* rhs,
                                               const wasm_recgroup_t* rhs_group,
                                               uint8_t* memo);

WASM_PRIVATE int wasm__cross_typed_valtype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                     wasm_valtype_t lhs_type,
                                                     const wasm_reftype_t* lhs_ref,
                                                     const wasm_recgroup_t* lhs_group,
                                                     const wasm_module_t* rhs_mod,
                                                     wasm_valtype_t rhs_type,
                                                     const wasm_reftype_t* rhs_ref,
                                                     const wasm_recgroup_t* rhs_group,
                                                     uint8_t* memo);

WASM_PRIVATE int wasm__cross_functype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                const wasm_functype_t* lhs,
                                                const wasm_recgroup_t* lhs_group,
                                                const wasm_module_t* rhs_mod,
                                                const wasm_functype_t* rhs,
                                                const wasm_recgroup_t* rhs_group,
                                                uint8_t* memo);

WASM_PRIVATE int wasm__cross_storagetype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                   const wasm_storagetype_t* lhs,
                                                   const wasm_recgroup_t* lhs_group,
                                                   const wasm_module_t* rhs_mod,
                                                   const wasm_storagetype_t* rhs,
                                                   const wasm_recgroup_t* rhs_group,
                                                   uint8_t* memo);

WASM_PRIVATE int wasm__cross_fieldtype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                 const wasm_fieldtype_t* lhs,
                                                 const wasm_recgroup_t* lhs_group,
                                                 const wasm_module_t* rhs_mod,
                                                 const wasm_fieldtype_t* rhs,
                                                 const wasm_recgroup_t* rhs_group,
                                                 uint8_t* memo);

WASM_PRIVATE int wasm__cross_comptype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                uint32_t lhs_idx,
                                                const wasm_recgroup_t* lhs_group,
                                                const wasm_module_t* rhs_mod,
                                                uint32_t rhs_idx,
                                                const wasm_recgroup_t* rhs_group,
                                                uint8_t* memo);

WASM_PRIVATE int wasm__cross_recgroup_equal(const wasm_module_t* lhs_mod,
                                      const wasm_recgroup_t* lhs_group,
                                      const wasm_module_t* rhs_mod,
                                      const wasm_recgroup_t* rhs_group,
                                      uint8_t* memo);

WASM_PRIVATE int wasm__cross_type_equal_visit(const wasm_module_t* lhs_mod,
                                        uint32_t lhs_idx,
                                        const wasm_module_t* rhs_mod,
                                        uint32_t rhs_idx,
                                        uint8_t* memo);

WASM_PRIVATE int wasm__cross_type_is_subtype_with_memo(const wasm_module_t* lhs_mod,
                                                 uint32_t lhs_idx,
                                                 const wasm_module_t* rhs_mod,
                                                 uint32_t rhs_idx,
                                                 uint8_t* memo);

WASM_PRIVATE int wasm__functype_equal_across_modules(const wasm_module_t* lhs_mod,
                                               uint32_t lhs_type_idx,
                                               const wasm_module_t* rhs_mod,
                                               uint32_t rhs_type_idx);

WASM_PRIVATE int wasm__functype_is_subtype_across_modules(const wasm_module_t* lhs_mod,
                                                    uint32_t lhs_type_idx,
                                                    const wasm_module_t* rhs_mod,
                                                    uint32_t rhs_type_idx);

WASM_PRIVATE wasm_memory_t* wasm__memory_actual(wasm_memory_t* memory);

WASM_PRIVATE const wasm_memory_t* wasm__memory_actual_const(const wasm_memory_t* memory);

WASM_PRIVATE void wasm__free_table(wasm_table_t* table);

WASM_PRIVATE void wasm__free_memory(wasm_memory_t* memory);

WASM_PRIVATE wasm_error_t wasm__append_tables(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE wasm_error_t wasm__append_memories(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE wasm_table_t* wasm__table_at(wasm_module_t* mod, uint32_t table_index);

WASM_PRIVATE wasm_memory_t* wasm__memory_at(wasm_module_t* mod, uint32_t memory_index);

WASM_PRIVATE wasm_valtype_t wasm__memory_index_type(const wasm_memory_t* memory);

WASM_PRIVATE wasm_valtype_t wasm__table_index_type(const wasm_table_t* table);

WASM_PRIVATE wasm_valtype_t wasm__table_copy_size_type(const wasm_table_t* dst_table,
                                                 const wasm_table_t* src_table);

WASM_PRIVATE uint64_t wasm__table_index_value(const wasm_table_t* table, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__table_index_result(const wasm_table_t* table, uint64_t value);

WASM_PRIVATE uint64_t wasm__memory_page_limit(const wasm_memory_t* memory);

WASM_PRIVATE wasm_error_t wasm__set_validation_error(wasm_runtime_t* rt,
                                               uint32_t func_idx,
                                               uint32_t offset,
                                               const char* fmt,
                                               ...);

WASM_PRIVATE wasm_error_t wasm__validate_memory_limits(wasm_module_t* mod,
                                                 uint32_t memory_index,
                                                 const wasm_memory_t* memory);

WASM_PRIVATE uint64_t wasm__memory_byte_size(const wasm_memory_t* memory);

WASM_PRIVATE int wasm__memory_byte_size_fits_host(const wasm_memory_t* memory, size_t* out_size);

WASM_PRIVATE int wasm__memory_effective_addr(const wasm_memory_t* memory,
                                       const wasm_value_t* base,
                                       uint64_t offset,
                                       uint64_t* out_addr);

WASM_PRIVATE wasm_error_t wasm__init_memory_storage(wasm_memory_t* memory);

WASM_PRIVATE wasm_error_t wasm__init_table_storage(wasm_table_t* table);

WASM_PRIVATE wasm_error_t wasm__append_data_segments(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE void wasm__free_custom_section(wasm_custom_section_t* section);

WASM_PRIVATE wasm_error_t wasm__append_custom_sections(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE void wasm__free_elem_segment(wasm_elem_segment_t* segment);

WASM_PRIVATE wasm_error_t wasm__append_elem_segments(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE wasm_error_t wasm__read_elemkind(wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__read_typed_select_immediate(wasm_module_t* mod,
                                                      wasm__reader_t* r,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype);

WASM_PRIVATE int wasm__range_in_bounds_u64(uint64_t offset, uint64_t size, uint64_t limit);

WASM_PRIVATE int wasm__memory_access_in_bounds(uint64_t addr, uint64_t width, uint64_t mem_size);

typedef struct wasm__memarg_t {
    uint32_t align_log2;
    uint32_t memory_index;
    uint8_t has_memory_index;
    uint64_t offset;
} wasm__memarg_t;

WASM_PRIVATE wasm_error_t wasm__read_memarg(wasm__reader_t* r, wasm__memarg_t* out_memarg);


#endif /* WASM_INTERNAL_VALUES_H */

#ifndef WASM_VALUES_H
#define WASM_VALUES_H

#include "internal/config.h"
#include "internal/gc_types.h"

WASM_PRIVATE wasm_value_t wasm__f32_bits(uint32_t bits);

WASM_PRIVATE wasm_value_t wasm__f64_bits(uint64_t bits);

WASM_PRIVATE uintptr_t wasm__gc_i31_encode(int32_t value);

WASM_PRIVATE int32_t wasm__gc_i31_get_s(uintptr_t bits);

WASM_PRIVATE uint32_t wasm__gc_i31_get_u(uintptr_t bits);

WASM_PRIVATE size_t wasm__align_up_size(size_t value, size_t alignment);

WASM_PRIVATE size_t wasm__gc_struct_field_offset(const wasm_structtype_t* type, uint32_t field_index);

WASM_PRIVATE int wasm__gc_struct_size(const wasm_structtype_t* type, size_t* out_size);

WASM_PRIVATE size_t wasm__gc_array_data_offset(const wasm_arraytype_t* type);

WASM_PRIVATE int wasm__gc_array_total_size(const wasm_arraytype_t* type,
                                     uint32_t length,
                                     size_t* out_size);

WASM_PRIVATE wasm_value_t wasm__gc_field_default_value(const wasm_fieldtype_t* field);

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

WASM_PRIVATE int wasm__is_number_or_vector_type(wasm_valtype_t type);

WASM_PRIVATE int wasm__is_value_type(wasm_valtype_t type);

WASM_PRIVATE int wasm__is_null_ref(const wasm_value_t* value);

WASM_PRIVATE int wasm__is_heap_gc_ref_value(const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__typed_ref_null(wasm_valtype_t type,
                                         const wasm_reftype_t* reftype);

WASM_PRIVATE wasm_value_t wasm__retag_value_for_declared_type(wasm_valtype_t declared_type,
                                                        const wasm_reftype_t* declared_reftype,
                                                        wasm_value_t value);

WASM_PRIVATE wasm_value_t wasm__default_value(wasm_valtype_t type);

/* Numeric values already carry their declared type tag, so only references need retagging. */
WASM_PRIVATE_INLINE int wasm__is_numeric_valtype(wasm_valtype_t type);

WASM_PRIVATE int32_t wasm__trunc_sat_i32_s(double value);

WASM_PRIVATE uint32_t wasm__trunc_sat_i32_u(double value);

WASM_PRIVATE int64_t wasm__trunc_sat_i64_s(double value);

WASM_PRIVATE uint64_t wasm__trunc_sat_i64_u(double value);

WASM_PRIVATE wasm_error_t wasm__trunc_i32_s(double value, int32_t* out_value);

WASM_PRIVATE wasm_error_t wasm__trunc_i32_u(double value, uint32_t* out_value);

WASM_PRIVATE wasm_error_t wasm__trunc_i64_s(double value, int64_t* out_value);

WASM_PRIVATE wasm_error_t wasm__trunc_i64_u(double value, uint64_t* out_value);

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

#endif /* WASM_VALUES_H */

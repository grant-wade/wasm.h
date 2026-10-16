#ifndef WASM_INTERNAL_SIMD_H
#define WASM_INTERNAL_SIMD_H

#include "values.h"

/* ── SIMD helpers ────────────────────────────────────────────────── */

WASM_PRIVATE int8_t wasm__v128_get_i8(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE uint8_t wasm__v128_get_u8(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE int16_t wasm__v128_get_i16(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE uint16_t wasm__v128_get_u16(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE int32_t wasm__v128_get_i32(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE uint32_t wasm__v128_get_u32(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE int64_t wasm__v128_get_i64(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE uint64_t wasm__v128_get_u64(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE float wasm__v128_get_f32(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE double wasm__v128_get_f64(const wasm_value_t* value, uint32_t lane);

WASM_PRIVATE void wasm__v128_set_u8(wasm_value_t* value, uint32_t lane, uint32_t lane_value);

WASM_PRIVATE void wasm__v128_set_i16(wasm_value_t* value, uint32_t lane, int32_t lane_value);

WASM_PRIVATE void wasm__v128_set_u16(wasm_value_t* value, uint32_t lane, uint32_t lane_value);

WASM_PRIVATE void wasm__v128_set_i32(wasm_value_t* value, uint32_t lane, int32_t lane_value);

WASM_PRIVATE void wasm__v128_set_u32(wasm_value_t* value, uint32_t lane, uint32_t lane_value);

WASM_PRIVATE void wasm__v128_set_i64(wasm_value_t* value, uint32_t lane, int64_t lane_value);

WASM_PRIVATE void wasm__v128_set_u64(wasm_value_t* value, uint32_t lane, uint64_t lane_value);

WASM_PRIVATE void wasm__v128_set_f32(wasm_value_t* value, uint32_t lane, float lane_value);

WASM_PRIVATE void wasm__v128_set_f64(wasm_value_t* value, uint32_t lane, double lane_value);

WASM_PRIVATE int8_t wasm__sat_i8(int32_t value);

WASM_PRIVATE uint8_t wasm__sat_u8(int32_t value);

WASM_PRIVATE int16_t wasm__sat_i16(int32_t value);

WASM_PRIVATE uint16_t wasm__sat_u16(int32_t value);

WASM_PRIVATE uint8_t wasm__popcount_u8(uint8_t value);

WASM_PRIVATE int wasm__simd_any_true(const wasm_value_t* value);

WASM_PRIVATE int32_t wasm__simd_bitmask_i8x16(const wasm_value_t* value);

WASM_PRIVATE int32_t wasm__simd_bitmask_i16x8(const wasm_value_t* value);

WASM_PRIVATE int32_t wasm__simd_bitmask_i32x4(const wasm_value_t* value);

WASM_PRIVATE int32_t wasm__simd_bitmask_i64x2(const wasm_value_t* value);

WASM_PRIVATE int wasm__simd_all_true_i8x16(const wasm_value_t* value);

WASM_PRIVATE int wasm__simd_all_true_i16x8(const wasm_value_t* value);

WASM_PRIVATE int wasm__simd_all_true_i32x4(const wasm_value_t* value);

WASM_PRIVATE int wasm__simd_all_true_i64x2(const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_bitwise_not(const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_bitwise_binary(uint32_t subop,
                                              const wasm_value_t* lhs,
                                              const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_bitselect(const wasm_value_t* lhs,
                                         const wasm_value_t* rhs,
                                         const wasm_value_t* mask);

WASM_PRIVATE wasm_value_t wasm__simd_shuffle(const wasm_value_t* lhs,
                                       const wasm_value_t* rhs,
                                       const uint8_t lanes[16]);

WASM_PRIVATE wasm_value_t wasm__simd_swizzle(const wasm_value_t* lhs, const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_relaxed_madd(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs,
                                            const wasm_value_t* acc);

WASM_PRIVATE wasm_value_t wasm__simd_relaxed_dot_i8x16_i7x16_s(const wasm_value_t* lhs,
                                                         const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_relaxed_dot_i8x16_i7x16_add_s(const wasm_value_t* lhs,
                                                             const wasm_value_t* rhs,
                                                             const wasm_value_t* acc);

WASM_PRIVATE uint32_t wasm__simd_memory_align_log2(uint32_t subop);

WASM_PRIVATE uint32_t wasm__simd_lane_limit(uint32_t subop);

WASM_PRIVATE void wasm__skip_simd_immediates(wasm__reader_t* r);

WASM_PRIVATE wasm_value_t wasm__simd_cmp_i8x16(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_cmp_i16x8(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_cmp_i32x4(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_cmp_i64x2(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_cmp_f32x4(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_cmp_f64x2(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_i8x16_unary(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_i8x16_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount);

WASM_PRIVATE wasm_value_t wasm__simd_i8x16_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_i16x8_unary(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_i16x8_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount);

WASM_PRIVATE wasm_value_t wasm__simd_i16x8_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_i32x4_unary(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_i32x4_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount);

WASM_PRIVATE wasm_value_t wasm__simd_i32x4_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_i64x2_unary(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_i64x2_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount);

WASM_PRIVATE wasm_value_t wasm__simd_i64x2_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_f32x4_unary(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_f32x4_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_f64x2_unary(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_f64x2_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs);

WASM_PRIVATE wasm_value_t wasm__simd_demote_f64x2_zero(const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_promote_low_f32x4(const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_extadd_pairwise(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_trunc_sat_f32x4(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_convert_i32x4(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_trunc_sat_f64x2_zero(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__simd_convert_low_i32x4_to_f64x2(uint32_t subop, const wasm_value_t* value);

WASM_PRIVATE wasm_error_t wasm__apply_active_data_segments(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__apply_active_elem_segments(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__eval_init_expr(wasm_module_t* mod, wasm__reader_t* r,
                                         uint32_t max_global_index,
                                         wasm_value_t* out_value);

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


#endif /* WASM_INTERNAL_SIMD_H */

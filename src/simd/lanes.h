#ifndef WASM_SIMD_LANES_H
#define WASM_SIMD_LANES_H

#include "../internal/config.h"

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

#endif /* WASM_SIMD_LANES_H */

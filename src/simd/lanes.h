#ifndef WASM_SIMD_LANES_H
#define WASM_SIMD_LANES_H

#include "../internal/config.h"
#include "v128.h"

WASM_PRIVATE int8_t wasm__sat_i8(int32_t value);

WASM_PRIVATE uint8_t wasm__sat_u8(int32_t value);

WASM_PRIVATE int16_t wasm__sat_i16(int32_t value);

WASM_PRIVATE uint16_t wasm__sat_u16(int32_t value);

WASM_PRIVATE uint8_t wasm__popcount_u8(uint8_t value);

WASM_PRIVATE int wasm__simd_any_true(const wasm__v128_t* value);

WASM_PRIVATE int32_t wasm__simd_bitmask_i8x16(const wasm__v128_t* value);

WASM_PRIVATE int32_t wasm__simd_bitmask_i16x8(const wasm__v128_t* value);

WASM_PRIVATE int32_t wasm__simd_bitmask_i32x4(const wasm__v128_t* value);

WASM_PRIVATE int32_t wasm__simd_bitmask_i64x2(const wasm__v128_t* value);

WASM_PRIVATE int wasm__simd_all_true_i8x16(const wasm__v128_t* value);

WASM_PRIVATE int wasm__simd_all_true_i16x8(const wasm__v128_t* value);

WASM_PRIVATE int wasm__simd_all_true_i32x4(const wasm__v128_t* value);

WASM_PRIVATE int wasm__simd_all_true_i64x2(const wasm__v128_t* value);

WASM_PRIVATE wasm__v128_t wasm__simd_bitwise_not(const wasm__v128_t* value);

WASM_PRIVATE wasm__v128_t wasm__simd_bitwise_binary(uint32_t subop,
                                              const wasm__v128_t* lhs,
                                              const wasm__v128_t* rhs);

WASM_PRIVATE wasm__v128_t wasm__simd_bitselect(const wasm__v128_t* lhs,
                                         const wasm__v128_t* rhs,
                                         const wasm__v128_t* mask);

WASM_PRIVATE wasm__v128_t wasm__simd_shuffle(const wasm__v128_t* lhs,
                                       const wasm__v128_t* rhs,
                                       const uint8_t lanes[16]);

WASM_PRIVATE wasm__v128_t wasm__simd_swizzle(const wasm__v128_t* lhs, const wasm__v128_t* rhs);

WASM_PRIVATE wasm__v128_t wasm__simd_relaxed_madd(uint32_t subop,
                                            const wasm__v128_t* lhs,
                                            const wasm__v128_t* rhs,
                                            const wasm__v128_t* acc);

WASM_PRIVATE wasm__v128_t wasm__simd_relaxed_dot_i8x16_i7x16_s(const wasm__v128_t* lhs,
                                                         const wasm__v128_t* rhs);

WASM_PRIVATE wasm__v128_t wasm__simd_relaxed_dot_i8x16_i7x16_add_s(const wasm__v128_t* lhs,
                                                             const wasm__v128_t* rhs,
                                                             const wasm__v128_t* acc);

#endif /* WASM_SIMD_LANES_H */

#ifndef WASM_SIMD_INTEGER_WIDE_H
#define WASM_SIMD_INTEGER_WIDE_H

#include "../internal/config.h"
#include "v128.h"

WASM_PRIVATE wasm__v128_t wasm__simd_i32x4_unary(uint32_t subop, const wasm__v128_t* value);

WASM_PRIVATE wasm__v128_t wasm__simd_i32x4_shift(uint32_t subop,
                                           const wasm__v128_t* value,
                                           uint32_t amount);

WASM_PRIVATE wasm__v128_t wasm__simd_i32x4_binary(uint32_t subop,
                                            const wasm__v128_t* lhs,
                                            const wasm__v128_t* rhs);

WASM_PRIVATE wasm__v128_t wasm__simd_i64x2_unary(uint32_t subop, const wasm__v128_t* value);

WASM_PRIVATE wasm__v128_t wasm__simd_i64x2_shift(uint32_t subop,
                                           const wasm__v128_t* value,
                                           uint32_t amount);

WASM_PRIVATE wasm__v128_t wasm__simd_i64x2_binary(uint32_t subop,
                                            const wasm__v128_t* lhs,
                                            const wasm__v128_t* rhs);

#endif /* WASM_SIMD_INTEGER_WIDE_H */

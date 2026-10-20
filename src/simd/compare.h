#ifndef WASM_SIMD_COMPARE_H
#define WASM_SIMD_COMPARE_H

#include "../internal/config.h"
#include "v128.h"

WASM_PRIVATE wasm__v128_t wasm__simd_cmp_i8x16(uint32_t subop,
                                         const wasm__v128_t* lhs,
                                         const wasm__v128_t* rhs);

WASM_PRIVATE wasm__v128_t wasm__simd_cmp_i16x8(uint32_t subop,
                                         const wasm__v128_t* lhs,
                                         const wasm__v128_t* rhs);

WASM_PRIVATE wasm__v128_t wasm__simd_cmp_i32x4(uint32_t subop,
                                         const wasm__v128_t* lhs,
                                         const wasm__v128_t* rhs);

WASM_PRIVATE wasm__v128_t wasm__simd_cmp_i64x2(uint32_t subop,
                                         const wasm__v128_t* lhs,
                                         const wasm__v128_t* rhs);

WASM_PRIVATE wasm__v128_t wasm__simd_cmp_f32x4(uint32_t subop,
                                         const wasm__v128_t* lhs,
                                         const wasm__v128_t* rhs);

WASM_PRIVATE wasm__v128_t wasm__simd_cmp_f64x2(uint32_t subop,
                                         const wasm__v128_t* lhs,
                                         const wasm__v128_t* rhs);

#endif /* WASM_SIMD_COMPARE_H */

#ifndef WASM_SIMD_INTEGER_SMALL_H
#define WASM_SIMD_INTEGER_SMALL_H

#include "../internal/config.h"

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

#endif /* WASM_SIMD_INTEGER_SMALL_H */

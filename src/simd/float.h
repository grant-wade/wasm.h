#ifndef WASM_SIMD_FLOAT_H
#define WASM_SIMD_FLOAT_H

#include "../internal/config.h"

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

#endif /* WASM_SIMD_FLOAT_H */

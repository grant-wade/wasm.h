#ifndef WASM_SIMD_NATIVE_H
#define WASM_SIMD_NATIVE_H

#include "../internal/config.h"

#if defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#define WASM__SIMD_USE_NEON 1
#include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define WASM__SIMD_USE_SSE2 1
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

#if defined(WASM__SIMD_USE_NEON)
WASM_PRIVATE uint8x16_t wasm__neon_load_u8x16(const wasm_value_t* value);

WASM_PRIVATE int8x16_t wasm__neon_load_i8x16(const wasm_value_t* value);

WASM_PRIVATE uint16x8_t wasm__neon_load_u16x8(const wasm_value_t* value);

WASM_PRIVATE int16x8_t wasm__neon_load_i16x8(const wasm_value_t* value);

WASM_PRIVATE uint32x4_t wasm__neon_load_u32x4(const wasm_value_t* value);

WASM_PRIVATE int32x4_t wasm__neon_load_i32x4(const wasm_value_t* value);

WASM_PRIVATE uint64x2_t wasm__neon_load_u64x2(const wasm_value_t* value);

WASM_PRIVATE int64x2_t wasm__neon_load_i64x2(const wasm_value_t* value);

WASM_PRIVATE float32x4_t wasm__neon_load_f32x4(const wasm_value_t* value);

WASM_PRIVATE float64x2_t wasm__neon_load_f64x2(const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__neon_store_u8x16(uint8x16_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_u16x8(uint16x8_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_u32x4(uint32x4_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_u64x2(uint64x2_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_f32x4(float32x4_t value);

WASM_PRIVATE wasm_value_t wasm__neon_store_f64x2(float64x2_t value);

WASM_PRIVATE int wasm__neon_any_true_u8x16(uint8x16_t value);
#endif

#if defined(WASM__SIMD_USE_SSE2)
WASM_PRIVATE __m128i wasm__sse_load_si128(const wasm_value_t* value);

WASM_PRIVATE __m128 wasm__sse_load_ps128(const wasm_value_t* value);

WASM_PRIVATE __m128d wasm__sse_load_pd128(const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__sse_store_si128(__m128i value);

WASM_PRIVATE wasm_value_t wasm__sse_store_ps128(__m128 value);

WASM_PRIVATE wasm_value_t wasm__sse_store_pd128(__m128d value);

WASM_PRIVATE __m128i wasm__sse_select_si128(__m128i mask, __m128i if_true, __m128i if_false);

WASM_PRIVATE __m128i wasm__sse_cmpgt_epu8(__m128i lhs, __m128i rhs);

WASM_PRIVATE __m128i wasm__sse_cmpgt_epu16(__m128i lhs, __m128i rhs);

WASM_PRIVATE __m128i wasm__sse_cmpgt_epu32(__m128i lhs, __m128i rhs);

WASM_PRIVATE __m128i wasm__sse_cmpeq_epi64(__m128i lhs, __m128i rhs);

WASM_PRIVATE __m128 wasm__sse_abs_ps(__m128 value);

WASM_PRIVATE __m128 wasm__sse_neg_ps(__m128 value);

WASM_PRIVATE __m128d wasm__sse_abs_pd(__m128d value);

WASM_PRIVATE __m128d wasm__sse_neg_pd(__m128d value);
#endif

#endif /* WASM_SIMD_NATIVE_H */

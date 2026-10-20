#ifndef WASM_SIMD_NATIVE_H
#define WASM_SIMD_NATIVE_H

#include "../internal/config.h"
#include "v128.h"

#if defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#define WASM__SIMD_USE_NEON 1
#include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define WASM__SIMD_USE_SSE2 1
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

#if defined(WASM__SIMD_USE_NEON)
WASM_INLINE uint8x16_t wasm__neon_load_u8x16(const wasm__v128_t* value) {
    return vld1q_u8(value->bytes);
}

WASM_INLINE int8x16_t wasm__neon_load_i8x16(const wasm__v128_t* value) {
    return vreinterpretq_s8_u8(wasm__neon_load_u8x16(value));
}

WASM_INLINE uint16x8_t wasm__neon_load_u16x8(const wasm__v128_t* value) {
    return vreinterpretq_u16_u8(wasm__neon_load_u8x16(value));
}

WASM_INLINE int16x8_t wasm__neon_load_i16x8(const wasm__v128_t* value) {
    return vreinterpretq_s16_u8(wasm__neon_load_u8x16(value));
}

WASM_INLINE uint32x4_t wasm__neon_load_u32x4(const wasm__v128_t* value) {
    return vreinterpretq_u32_u8(wasm__neon_load_u8x16(value));
}

WASM_INLINE int32x4_t wasm__neon_load_i32x4(const wasm__v128_t* value) {
    return vreinterpretq_s32_u8(wasm__neon_load_u8x16(value));
}

WASM_INLINE uint64x2_t wasm__neon_load_u64x2(const wasm__v128_t* value) {
    return vreinterpretq_u64_u8(wasm__neon_load_u8x16(value));
}

WASM_INLINE int64x2_t wasm__neon_load_i64x2(const wasm__v128_t* value) {
    return vreinterpretq_s64_u8(wasm__neon_load_u8x16(value));
}

WASM_INLINE float32x4_t wasm__neon_load_f32x4(const wasm__v128_t* value) {
    return vreinterpretq_f32_u8(wasm__neon_load_u8x16(value));
}

WASM_INLINE float64x2_t wasm__neon_load_f64x2(const wasm__v128_t* value) {
    return vreinterpretq_f64_u8(wasm__neon_load_u8x16(value));
}

WASM_INLINE wasm__v128_t wasm__neon_store_u8x16(uint8x16_t value) {
    wasm__v128_t result;

    vst1q_u8(result.bytes, value);
    return result;
}

WASM_INLINE wasm__v128_t wasm__neon_store_u16x8(uint16x8_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_u16(value));
}

WASM_INLINE wasm__v128_t wasm__neon_store_u32x4(uint32x4_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_u32(value));
}

WASM_INLINE wasm__v128_t wasm__neon_store_u64x2(uint64x2_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_u64(value));
}

WASM_INLINE wasm__v128_t wasm__neon_store_f32x4(float32x4_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_f32(value));
}

WASM_INLINE wasm__v128_t wasm__neon_store_f64x2(float64x2_t value) {
    return wasm__neon_store_u8x16(vreinterpretq_u8_f64(value));
}

WASM_INLINE int wasm__neon_any_true_u8x16(uint8x16_t value) {
    uint64x2_t words = vreinterpretq_u64_u8(value);

    return (vgetq_lane_u64(words, 0) | vgetq_lane_u64(words, 1)) != 0u;
}
#endif

#if defined(WASM__SIMD_USE_SSE2)
WASM_INLINE __m128i wasm__sse_load_si128(const wasm__v128_t* value) {
    return _mm_loadu_si128((const __m128i*)(const void*)value->bytes);
}

WASM_INLINE __m128 wasm__sse_load_ps128(const wasm__v128_t* value) {
    return _mm_castsi128_ps(wasm__sse_load_si128(value));
}

WASM_INLINE __m128d wasm__sse_load_pd128(const wasm__v128_t* value) {
    return _mm_castsi128_pd(wasm__sse_load_si128(value));
}

WASM_INLINE wasm__v128_t wasm__sse_store_si128(__m128i value) {
    wasm__v128_t result;

    _mm_storeu_si128((__m128i*)(void*)result.bytes, value);
    return result;
}

WASM_INLINE wasm__v128_t wasm__sse_store_ps128(__m128 value) {
    return wasm__sse_store_si128(_mm_castps_si128(value));
}

WASM_INLINE wasm__v128_t wasm__sse_store_pd128(__m128d value) {
    return wasm__sse_store_si128(_mm_castpd_si128(value));
}

WASM_INLINE __m128i wasm__sse_select_si128(__m128i mask, __m128i if_true, __m128i if_false) {
    return _mm_or_si128(_mm_and_si128(mask, if_true), _mm_andnot_si128(mask, if_false));
}

WASM_INLINE __m128i wasm__sse_cmpgt_epu8(__m128i lhs, __m128i rhs) {
    __m128i bias = _mm_set1_epi8((char)-128);
    return _mm_cmpgt_epi8(_mm_xor_si128(lhs, bias), _mm_xor_si128(rhs, bias));
}

WASM_INLINE __m128i wasm__sse_cmpgt_epu16(__m128i lhs, __m128i rhs) {
    __m128i bias = _mm_set1_epi16((short)INT16_MIN);
    return _mm_cmpgt_epi16(_mm_xor_si128(lhs, bias), _mm_xor_si128(rhs, bias));
}

WASM_INLINE __m128i wasm__sse_cmpgt_epu32(__m128i lhs, __m128i rhs) {
    __m128i bias = _mm_set1_epi32(INT32_MIN);
    return _mm_cmpgt_epi32(_mm_xor_si128(lhs, bias), _mm_xor_si128(rhs, bias));
}

WASM_INLINE __m128i wasm__sse_cmpeq_epi64(__m128i lhs, __m128i rhs) {
    __m128i eq32 = _mm_cmpeq_epi32(lhs, rhs);
    __m128i swapped = _mm_shuffle_epi32(eq32, _MM_SHUFFLE(2, 3, 0, 1));
    return _mm_and_si128(eq32, swapped);
}

WASM_INLINE __m128 wasm__sse_abs_ps(__m128 value) {
    return _mm_andnot_ps(_mm_set1_ps(-0.0f), value);
}

WASM_INLINE __m128 wasm__sse_neg_ps(__m128 value) {
    return _mm_xor_ps(value, _mm_set1_ps(-0.0f));
}

WASM_INLINE __m128d wasm__sse_abs_pd(__m128d value) {
    return _mm_andnot_pd(_mm_set1_pd(-0.0), value);
}

WASM_INLINE __m128d wasm__sse_neg_pd(__m128d value) {
    return _mm_xor_pd(value, _mm_set1_pd(-0.0));
}
#endif

#endif /* WASM_SIMD_NATIVE_H */

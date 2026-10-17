#include "lanes.h"
#include "native.h"
#include "../portable.h"
#include <string.h>

WASM_PRIVATE int8_t wasm__v128_get_i8(const wasm_value_t* value, uint32_t lane) {
    return (int8_t)value->of.v128[lane];
}

WASM_PRIVATE uint8_t wasm__v128_get_u8(const wasm_value_t* value, uint32_t lane) {
    return value->of.v128[lane];
}

WASM_PRIVATE int16_t wasm__v128_get_i16(const wasm_value_t* value, uint32_t lane) {
    return (int16_t)wasm__load_le16(value->of.v128 + lane * 2u);
}

WASM_PRIVATE uint16_t wasm__v128_get_u16(const wasm_value_t* value, uint32_t lane) {
    return wasm__load_le16(value->of.v128 + lane * 2u);
}

WASM_PRIVATE int32_t wasm__v128_get_i32(const wasm_value_t* value, uint32_t lane) {
    return (int32_t)wasm__load_le32(value->of.v128 + lane * 4u);
}

WASM_PRIVATE uint32_t wasm__v128_get_u32(const wasm_value_t* value, uint32_t lane) {
    return wasm__load_le32(value->of.v128 + lane * 4u);
}

WASM_PRIVATE int64_t wasm__v128_get_i64(const wasm_value_t* value, uint32_t lane) {
    return (int64_t)wasm__load_le64(value->of.v128 + lane * 8u);
}

WASM_PRIVATE uint64_t wasm__v128_get_u64(const wasm_value_t* value, uint32_t lane) {
    return wasm__load_le64(value->of.v128 + lane * 8u);
}

WASM_PRIVATE float wasm__v128_get_f32(const wasm_value_t* value, uint32_t lane) {
    uint32_t bits = wasm__v128_get_u32(value, lane);
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

WASM_PRIVATE double wasm__v128_get_f64(const wasm_value_t* value, uint32_t lane) {
    uint64_t bits = wasm__v128_get_u64(value, lane);
    double result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

WASM_PRIVATE void wasm__v128_set_u8(wasm_value_t* value, uint32_t lane, uint32_t lane_value) {
    value->of.v128[lane] = (uint8_t)lane_value;
}

WASM_PRIVATE void wasm__v128_set_i16(wasm_value_t* value, uint32_t lane, int32_t lane_value) {
    wasm__store_le16(value->of.v128 + lane * 2u, (uint16_t)lane_value);
}

WASM_PRIVATE void wasm__v128_set_u16(wasm_value_t* value, uint32_t lane, uint32_t lane_value) {
    wasm__store_le16(value->of.v128 + lane * 2u, (uint16_t)lane_value);
}

WASM_PRIVATE void wasm__v128_set_i32(wasm_value_t* value, uint32_t lane, int32_t lane_value) {
    wasm__store_le32(value->of.v128 + lane * 4u, (uint32_t)lane_value);
}

WASM_PRIVATE void wasm__v128_set_u32(wasm_value_t* value, uint32_t lane, uint32_t lane_value) {
    wasm__store_le32(value->of.v128 + lane * 4u, lane_value);
}

WASM_PRIVATE void wasm__v128_set_i64(wasm_value_t* value, uint32_t lane, int64_t lane_value) {
    wasm__store_le64(value->of.v128 + lane * 8u, (uint64_t)lane_value);
}

WASM_PRIVATE void wasm__v128_set_u64(wasm_value_t* value, uint32_t lane, uint64_t lane_value) {
    wasm__store_le64(value->of.v128 + lane * 8u, lane_value);
}

WASM_PRIVATE void wasm__v128_set_f32(wasm_value_t* value, uint32_t lane, float lane_value) {
    uint32_t bits;
    memcpy(&bits, &lane_value, sizeof(bits));
    wasm__store_le32(value->of.v128 + lane * 4u, bits);
}

WASM_PRIVATE void wasm__v128_set_f64(wasm_value_t* value, uint32_t lane, double lane_value) {
    uint64_t bits;
    memcpy(&bits, &lane_value, sizeof(bits));
    wasm__store_le64(value->of.v128 + lane * 8u, bits);
}

WASM_PRIVATE int8_t wasm__sat_i8(int32_t value) {
    if (value < -128) return (int8_t)-128;
    if (value > 127) return (int8_t)127;
    return (int8_t)value;
}

WASM_PRIVATE uint8_t wasm__sat_u8(int32_t value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

WASM_PRIVATE int16_t wasm__sat_i16(int32_t value) {
    if (value < -32768) return (int16_t)-32768;
    if (value > 32767) return (int16_t)32767;
    return (int16_t)value;
}

WASM_PRIVATE uint16_t wasm__sat_u16(int32_t value) {
    if (value < 0) return 0;
    if (value > 65535) return 65535u;
    return (uint16_t)value;
}

WASM_PRIVATE uint8_t wasm__popcount_u8(uint8_t value) {
    uint8_t count = 0;

    while (value != 0) {
        count = (uint8_t)(count + (uint8_t)(value & 1u));
        value = (uint8_t)(value >> 1);
    }

    return count;
}

WASM_PRIVATE int wasm__simd_any_true(const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    return wasm__neon_any_true_u8x16(wasm__neon_load_u8x16(value));
#elif defined(WASM__SIMD_USE_SSE2)
    __m128i vec = wasm__sse_load_si128(value);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(vec, _mm_setzero_si128())) != 0xFFFF;
#endif
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        if (value->of.v128[lane] != 0) return 1;
    }

    return 0;
}

WASM_PRIVATE int32_t wasm__simd_bitmask_i8x16(const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_SSE2)
    return _mm_movemask_epi8(wasm__sse_load_si128(value));
#endif
    uint32_t lane;
    int32_t mask = 0;

    for (lane = 0; lane < 16; lane++) {
        if ((value->of.v128[lane] & 0x80u) != 0) mask |= (int32_t)(1u << lane);
    }

    return mask;
}

WASM_PRIVATE int32_t wasm__simd_bitmask_i16x8(const wasm_value_t* value) {
    uint32_t lane;
    int32_t mask = 0;

    for (lane = 0; lane < 8; lane++) {
        if (wasm__v128_get_i16(value, lane) < 0) mask |= (int32_t)(1u << lane);
    }

    return mask;
}

WASM_PRIVATE int32_t wasm__simd_bitmask_i32x4(const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    uint32x4_t sign = vshrq_n_u32(wasm__neon_load_u32x4(value), 31);
    return (int32_t)(vgetq_lane_u32(sign, 0) | (vgetq_lane_u32(sign, 1) << 1) |
                     (vgetq_lane_u32(sign, 2) << 2) | (vgetq_lane_u32(sign, 3) << 3));
#elif defined(WASM__SIMD_USE_SSE2)
    return _mm_movemask_ps(_mm_castsi128_ps(wasm__sse_load_si128(value)));
#endif
    uint32_t lane;
    int32_t mask = 0;

    for (lane = 0; lane < 4; lane++) {
        if (wasm__v128_get_i32(value, lane) < 0) mask |= (int32_t)(1u << lane);
    }

    return mask;
}

WASM_PRIVATE int32_t wasm__simd_bitmask_i64x2(const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    uint64x2_t sign = vshrq_n_u64(wasm__neon_load_u64x2(value), 63);
    return (int32_t)(vgetq_lane_u64(sign, 0) | (vgetq_lane_u64(sign, 1) << 1));
#elif defined(WASM__SIMD_USE_SSE2)
    return _mm_movemask_pd(_mm_castsi128_pd(wasm__sse_load_si128(value)));
#endif
    uint32_t lane;
    int32_t mask = 0;

    for (lane = 0; lane < 2; lane++) {
        if (wasm__v128_get_i64(value, lane) < 0) mask |= (int32_t)(1u << lane);
    }

    return mask;
}

WASM_PRIVATE int wasm__simd_all_true_i8x16(const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    return !wasm__neon_any_true_u8x16(vceqq_u8(wasm__neon_load_u8x16(value), vdupq_n_u8(0u)));
#elif defined(WASM__SIMD_USE_SSE2)
    __m128i vec = wasm__sse_load_si128(value);
    return _mm_movemask_epi8(_mm_cmpeq_epi8(vec, _mm_setzero_si128())) == 0;
#endif
    uint32_t lane;
    for (lane = 0; lane < 16; lane++) {
        if (wasm__v128_get_i8(value, lane) == 0) return 0;
    }
    return 1;
}

WASM_PRIVATE int wasm__simd_all_true_i16x8(const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    return !wasm__neon_any_true_u8x16(vreinterpretq_u8_u16(vceqq_u16(wasm__neon_load_u16x8(value), vdupq_n_u16(0u))));
#elif defined(WASM__SIMD_USE_SSE2)
    __m128i vec = wasm__sse_load_si128(value);
    return _mm_movemask_epi8(_mm_cmpeq_epi16(vec, _mm_setzero_si128())) == 0;
#endif
    uint32_t lane;
    for (lane = 0; lane < 8; lane++) {
        if (wasm__v128_get_i16(value, lane) == 0) return 0;
    }
    return 1;
}

WASM_PRIVATE int wasm__simd_all_true_i32x4(const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    return !wasm__neon_any_true_u8x16(vreinterpretq_u8_u32(vceqq_u32(wasm__neon_load_u32x4(value), vdupq_n_u32(0u))));
#elif defined(WASM__SIMD_USE_SSE2)
    __m128i vec = wasm__sse_load_si128(value);
    return _mm_movemask_epi8(_mm_cmpeq_epi32(vec, _mm_setzero_si128())) == 0;
#endif
    uint32_t lane;
    for (lane = 0; lane < 4; lane++) {
        if (wasm__v128_get_i32(value, lane) == 0) return 0;
    }
    return 1;
}

WASM_PRIVATE int wasm__simd_all_true_i64x2(const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    return !wasm__neon_any_true_u8x16(vreinterpretq_u8_u64(vceqq_u64(wasm__neon_load_u64x2(value), vdupq_n_u64(0u))));
#elif defined(WASM__SIMD_USE_SSE2)
    __m128i vec = wasm__sse_load_si128(value);
    return _mm_movemask_epi8(wasm__sse_cmpeq_epi64(vec, _mm_setzero_si128())) == 0;
#endif
    uint32_t lane;
    for (lane = 0; lane < 2; lane++) {
        if (wasm__v128_get_i64(value, lane) == 0) return 0;
    }
    return 1;
}

WASM_PRIVATE wasm_value_t wasm__simd_bitwise_not(const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    return wasm__neon_store_u8x16(vmvnq_u8(wasm__neon_load_u8x16(value)));
#elif defined(WASM__SIMD_USE_SSE2)
    return wasm__sse_store_si128(_mm_xor_si128(wasm__sse_load_si128(value), _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128())));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) result.of.v128[lane] = (uint8_t)~value->of.v128[lane];
    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_bitwise_binary(uint32_t subop,
                                              const wasm_value_t* lhs,
                                              const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        uint8x16_t a = wasm__neon_load_u8x16(lhs);
        uint8x16_t b = wasm__neon_load_u8x16(rhs);

        switch (subop) {
            case 0x4E:
                return wasm__neon_store_u8x16(vandq_u8(a, b));
            case 0x4F:
                return wasm__neon_store_u8x16(vbicq_u8(a, b));
            case 0x50:
                return wasm__neon_store_u8x16(vorrq_u8(a, b));
            default:
                return wasm__neon_store_u8x16(veorq_u8(a, b));
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128i a = wasm__sse_load_si128(lhs);
        __m128i b = wasm__sse_load_si128(rhs);

        switch (subop) {
            case 0x4E:
                return wasm__sse_store_si128(_mm_and_si128(a, b));
            case 0x4F:
                return wasm__sse_store_si128(_mm_andnot_si128(b, a));
            case 0x50:
                return wasm__sse_store_si128(_mm_or_si128(a, b));
            default:
                return wasm__sse_store_si128(_mm_xor_si128(a, b));
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        switch (subop) {
            case 0x4E:
                result.of.v128[lane] = (uint8_t)(lhs->of.v128[lane] & rhs->of.v128[lane]);
                break;
            case 0x4F:
                result.of.v128[lane] = (uint8_t)(lhs->of.v128[lane] & (uint8_t)~rhs->of.v128[lane]);
                break;
            case 0x50:
                result.of.v128[lane] = (uint8_t)(lhs->of.v128[lane] | rhs->of.v128[lane]);
                break;
            default:
                result.of.v128[lane] = (uint8_t)(lhs->of.v128[lane] ^ rhs->of.v128[lane]);
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_bitselect(const wasm_value_t* lhs,
                                         const wasm_value_t* rhs,
                                         const wasm_value_t* mask) {
#if defined(WASM__SIMD_USE_NEON)
    return wasm__neon_store_u8x16(vbslq_u8(wasm__neon_load_u8x16(mask),
                                           wasm__neon_load_u8x16(lhs),
                                           wasm__neon_load_u8x16(rhs)));
#elif defined(WASM__SIMD_USE_SSE2)
    return wasm__sse_store_si128(wasm__sse_select_si128(wasm__sse_load_si128(mask),
                                                        wasm__sse_load_si128(lhs),
                                                        wasm__sse_load_si128(rhs)));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        result.of.v128[lane] =
            (uint8_t)((lhs->of.v128[lane] & mask->of.v128[lane]) |
                      (rhs->of.v128[lane] & (uint8_t)~mask->of.v128[lane]));
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_shuffle(const wasm_value_t* lhs,
                                       const wasm_value_t* rhs,
                                       const uint8_t lanes[16]) {
#if defined(WASM__SIMD_USE_NEON)
    uint8x16x2_t table;

    table.val[0] = wasm__neon_load_u8x16(lhs);
    table.val[1] = wasm__neon_load_u8x16(rhs);
    return wasm__neon_store_u8x16(vqtbl2q_u8(table, vld1q_u8(lanes)));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint8_t combined[32];
    uint32_t lane;

    memcpy(combined, lhs->of.v128, 16);
    memcpy(combined + 16, rhs->of.v128, 16);
    for (lane = 0; lane < 16; lane++) result.of.v128[lane] = combined[lanes[lane]];
    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_swizzle(const wasm_value_t* lhs, const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    return wasm__neon_store_u8x16(vqtbl1q_u8(wasm__neon_load_u8x16(lhs), wasm__neon_load_u8x16(rhs)));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        uint8_t index = rhs->of.v128[lane];
        result.of.v128[lane] = (index < 16u) ? lhs->of.v128[index] : 0;
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_relaxed_madd(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs,
                                            const wasm_value_t* acc) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    if (subop == 0x105 || subop == 0x106) {
        for (lane = 0; lane < 4; lane++) {
            float product = wasm__v128_get_f32(lhs, lane) * wasm__v128_get_f32(rhs, lane);
            float sum = (subop == 0x106) ? (-product + wasm__v128_get_f32(acc, lane))
                                         : (product + wasm__v128_get_f32(acc, lane));
            wasm__v128_set_f32(&result, lane, sum);
        }
        return result;
    }

    for (lane = 0; lane < 2; lane++) {
        double product = wasm__v128_get_f64(lhs, lane) * wasm__v128_get_f64(rhs, lane);
        double sum = (subop == 0x108) ? (-product + wasm__v128_get_f64(acc, lane))
                                      : (product + wasm__v128_get_f64(acc, lane));
        wasm__v128_set_f64(&result, lane, sum);
    }
    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_relaxed_dot_i8x16_i7x16_s(const wasm_value_t* lhs,
                                                         const wasm_value_t* rhs) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 8; lane++) {
        uint32_t base = lane * 2u;
        int32_t dot = (int32_t)wasm__v128_get_i8(lhs, base) * (int32_t)wasm__v128_get_i8(rhs, base) +
                      (int32_t)wasm__v128_get_i8(lhs, base + 1u) * (int32_t)wasm__v128_get_i8(rhs, base + 1u);
        wasm__v128_set_i16(&result, lane, dot);
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_relaxed_dot_i8x16_i7x16_add_s(const wasm_value_t* lhs,
                                                             const wasm_value_t* rhs,
                                                             const wasm_value_t* acc) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        uint32_t base = lane * 4u;
        int32_t dot = (int32_t)wasm__v128_get_i8(lhs, base) * (int32_t)wasm__v128_get_i8(rhs, base) +
                      (int32_t)wasm__v128_get_i8(lhs, base + 1u) * (int32_t)wasm__v128_get_i8(rhs, base + 1u) +
                      (int32_t)wasm__v128_get_i8(lhs, base + 2u) * (int32_t)wasm__v128_get_i8(rhs, base + 2u) +
                      (int32_t)wasm__v128_get_i8(lhs, base + 3u) * (int32_t)wasm__v128_get_i8(rhs, base + 3u);
        wasm__v128_set_i32(&result, lane, dot + wasm__v128_get_i32(acc, lane));
    }

    return result;
}

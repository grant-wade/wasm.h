#include "../internal.h"

WASM_PRIVATE wasm_value_t wasm__simd_cmp_i8x16(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        int8x16_t a = wasm__neon_load_i8x16(lhs);
        int8x16_t b = wasm__neon_load_i8x16(rhs);
        uint8x16_t ua = vreinterpretq_u8_s8(a);
        uint8x16_t ub = vreinterpretq_u8_s8(b);

        switch (subop) {
            case 0x23:
                return wasm__neon_store_u8x16(vceqq_u8(ua, ub));
            case 0x24:
                return wasm__neon_store_u8x16(vmvnq_u8(vceqq_u8(ua, ub)));
            case 0x25:
                return wasm__neon_store_u8x16(vcltq_s8(a, b));
            case 0x26:
                return wasm__neon_store_u8x16(vcltq_u8(ua, ub));
            case 0x27:
                return wasm__neon_store_u8x16(vcgtq_s8(a, b));
            case 0x28:
                return wasm__neon_store_u8x16(vcgtq_u8(ua, ub));
            case 0x29:
                return wasm__neon_store_u8x16(vcleq_s8(a, b));
            case 0x2A:
                return wasm__neon_store_u8x16(vcleq_u8(ua, ub));
            case 0x2B:
                return wasm__neon_store_u8x16(vcgeq_s8(a, b));
            default:
                return wasm__neon_store_u8x16(vcgeq_u8(ua, ub));
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128i a = wasm__sse_load_si128(lhs);
        __m128i b = wasm__sse_load_si128(rhs);
        __m128i eq = _mm_cmpeq_epi8(a, b);

        switch (subop) {
            case 0x23:
                return wasm__sse_store_si128(eq);
            case 0x24:
                return wasm__sse_store_si128(_mm_andnot_si128(eq, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128())));
            case 0x25:
                return wasm__sse_store_si128(_mm_cmpgt_epi8(b, a));
            case 0x26:
                return wasm__sse_store_si128(wasm__sse_cmpgt_epu8(b, a));
            case 0x27:
                return wasm__sse_store_si128(_mm_cmpgt_epi8(a, b));
            case 0x28:
                return wasm__sse_store_si128(wasm__sse_cmpgt_epu8(a, b));
            case 0x29:
                return wasm__sse_store_si128(_mm_or_si128(eq, _mm_cmpgt_epi8(b, a)));
            case 0x2A:
                return wasm__sse_store_si128(_mm_or_si128(eq, wasm__sse_cmpgt_epu8(b, a)));
            case 0x2B:
                return wasm__sse_store_si128(_mm_or_si128(eq, _mm_cmpgt_epi8(a, b)));
            default:
                return wasm__sse_store_si128(_mm_or_si128(eq, wasm__sse_cmpgt_epu8(a, b)));
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        int8_t a = wasm__v128_get_i8(lhs, lane);
        int8_t b = wasm__v128_get_i8(rhs, lane);
        uint8_t ua = wasm__v128_get_u8(lhs, lane);
        uint8_t ub = wasm__v128_get_u8(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x23:
                truth = (ua == ub);
                break;
            case 0x24:
                truth = (ua != ub);
                break;
            case 0x25:
                truth = (a < b);
                break;
            case 0x26:
                truth = (ua < ub);
                break;
            case 0x27:
                truth = (a > b);
                break;
            case 0x28:
                truth = (ua > ub);
                break;
            case 0x29:
                truth = (a <= b);
                break;
            case 0x2A:
                truth = (ua <= ub);
                break;
            case 0x2B:
                truth = (a >= b);
                break;
            default:
                truth = (ua >= ub);
                break;
        }

        result.of.v128[lane] = truth ? 0xFFu : 0u;
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_cmp_i16x8(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        int16x8_t a = wasm__neon_load_i16x8(lhs);
        int16x8_t b = wasm__neon_load_i16x8(rhs);
        uint16x8_t ua = vreinterpretq_u16_s16(a);
        uint16x8_t ub = vreinterpretq_u16_s16(b);

        switch (subop) {
            case 0x2D:
                return wasm__neon_store_u16x8(vceqq_u16(ua, ub));
            case 0x2E:
                return wasm__neon_store_u16x8(vmvnq_u16(vceqq_u16(ua, ub)));
            case 0x2F:
                return wasm__neon_store_u16x8(vcltq_s16(a, b));
            case 0x30:
                return wasm__neon_store_u16x8(vcltq_u16(ua, ub));
            case 0x31:
                return wasm__neon_store_u16x8(vcgtq_s16(a, b));
            case 0x32:
                return wasm__neon_store_u16x8(vcgtq_u16(ua, ub));
            case 0x33:
                return wasm__neon_store_u16x8(vcleq_s16(a, b));
            case 0x34:
                return wasm__neon_store_u16x8(vcleq_u16(ua, ub));
            case 0x35:
                return wasm__neon_store_u16x8(vcgeq_s16(a, b));
            default:
                return wasm__neon_store_u16x8(vcgeq_u16(ua, ub));
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128i a = wasm__sse_load_si128(lhs);
        __m128i b = wasm__sse_load_si128(rhs);
        __m128i eq = _mm_cmpeq_epi16(a, b);

        switch (subop) {
            case 0x2D:
                return wasm__sse_store_si128(eq);
            case 0x2E:
                return wasm__sse_store_si128(_mm_andnot_si128(eq, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128())));
            case 0x2F:
                return wasm__sse_store_si128(_mm_cmpgt_epi16(b, a));
            case 0x30:
                return wasm__sse_store_si128(wasm__sse_cmpgt_epu16(b, a));
            case 0x31:
                return wasm__sse_store_si128(_mm_cmpgt_epi16(a, b));
            case 0x32:
                return wasm__sse_store_si128(wasm__sse_cmpgt_epu16(a, b));
            case 0x33:
                return wasm__sse_store_si128(_mm_or_si128(eq, _mm_cmpgt_epi16(b, a)));
            case 0x34:
                return wasm__sse_store_si128(_mm_or_si128(eq, wasm__sse_cmpgt_epu16(b, a)));
            case 0x35:
                return wasm__sse_store_si128(_mm_or_si128(eq, _mm_cmpgt_epi16(a, b)));
            default:
                return wasm__sse_store_si128(_mm_or_si128(eq, wasm__sse_cmpgt_epu16(a, b)));
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 8; lane++) {
        int16_t a = wasm__v128_get_i16(lhs, lane);
        int16_t b = wasm__v128_get_i16(rhs, lane);
        uint16_t ua = wasm__v128_get_u16(lhs, lane);
        uint16_t ub = wasm__v128_get_u16(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x2D:
                truth = (ua == ub);
                break;
            case 0x2E:
                truth = (ua != ub);
                break;
            case 0x2F:
                truth = (a < b);
                break;
            case 0x30:
                truth = (ua < ub);
                break;
            case 0x31:
                truth = (a > b);
                break;
            case 0x32:
                truth = (ua > ub);
                break;
            case 0x33:
                truth = (a <= b);
                break;
            case 0x34:
                truth = (ua <= ub);
                break;
            case 0x35:
                truth = (a >= b);
                break;
            default:
                truth = (ua >= ub);
                break;
        }

        wasm__v128_set_u16(&result, lane, truth ? 0xFFFFu : 0u);
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_cmp_i32x4(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        int32x4_t a = wasm__neon_load_i32x4(lhs);
        int32x4_t b = wasm__neon_load_i32x4(rhs);
        uint32x4_t ua = vreinterpretq_u32_s32(a);
        uint32x4_t ub = vreinterpretq_u32_s32(b);

        switch (subop) {
            case 0x37:
                return wasm__neon_store_u32x4(vceqq_u32(ua, ub));
            case 0x38:
                return wasm__neon_store_u32x4(vmvnq_u32(vceqq_u32(ua, ub)));
            case 0x39:
                return wasm__neon_store_u32x4(vcltq_s32(a, b));
            case 0x3A:
                return wasm__neon_store_u32x4(vcltq_u32(ua, ub));
            case 0x3B:
                return wasm__neon_store_u32x4(vcgtq_s32(a, b));
            case 0x3C:
                return wasm__neon_store_u32x4(vcgtq_u32(ua, ub));
            case 0x3D:
                return wasm__neon_store_u32x4(vcleq_s32(a, b));
            case 0x3E:
                return wasm__neon_store_u32x4(vcleq_u32(ua, ub));
            case 0x3F:
                return wasm__neon_store_u32x4(vcgeq_s32(a, b));
            default:
                return wasm__neon_store_u32x4(vcgeq_u32(ua, ub));
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128i a = wasm__sse_load_si128(lhs);
        __m128i b = wasm__sse_load_si128(rhs);
        __m128i eq = _mm_cmpeq_epi32(a, b);

        switch (subop) {
            case 0x37:
                return wasm__sse_store_si128(eq);
            case 0x38:
                return wasm__sse_store_si128(_mm_andnot_si128(eq, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128())));
            case 0x39:
                return wasm__sse_store_si128(_mm_cmpgt_epi32(b, a));
            case 0x3A:
                return wasm__sse_store_si128(wasm__sse_cmpgt_epu32(b, a));
            case 0x3B:
                return wasm__sse_store_si128(_mm_cmpgt_epi32(a, b));
            case 0x3C:
                return wasm__sse_store_si128(wasm__sse_cmpgt_epu32(a, b));
            case 0x3D:
                return wasm__sse_store_si128(_mm_or_si128(eq, _mm_cmpgt_epi32(b, a)));
            case 0x3E:
                return wasm__sse_store_si128(_mm_or_si128(eq, wasm__sse_cmpgt_epu32(b, a)));
            case 0x3F:
                return wasm__sse_store_si128(_mm_or_si128(eq, _mm_cmpgt_epi32(a, b)));
            default:
                return wasm__sse_store_si128(_mm_or_si128(eq, wasm__sse_cmpgt_epu32(a, b)));
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        int32_t a = wasm__v128_get_i32(lhs, lane);
        int32_t b = wasm__v128_get_i32(rhs, lane);
        uint32_t ua = wasm__v128_get_u32(lhs, lane);
        uint32_t ub = wasm__v128_get_u32(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x37:
                truth = (ua == ub);
                break;
            case 0x38:
                truth = (ua != ub);
                break;
            case 0x39:
                truth = (a < b);
                break;
            case 0x3A:
                truth = (ua < ub);
                break;
            case 0x3B:
                truth = (a > b);
                break;
            case 0x3C:
                truth = (ua > ub);
                break;
            case 0x3D:
                truth = (a <= b);
                break;
            case 0x3E:
                truth = (ua <= ub);
                break;
            case 0x3F:
                truth = (a >= b);
                break;
            default:
                truth = (ua >= ub);
                break;
        }

        wasm__v128_set_u32(&result, lane, truth ? UINT32_MAX : 0u);
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_cmp_i64x2(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        int64x2_t a = wasm__neon_load_i64x2(lhs);
        int64x2_t b = wasm__neon_load_i64x2(rhs);

        switch (subop) {
            case 0xD6:
                return wasm__neon_store_u64x2(vceqq_s64(a, b));
            case 0xD7:
                return wasm__neon_store_u64x2(vreinterpretq_u64_u8(vmvnq_u8(vreinterpretq_u8_u64(vceqq_s64(a, b)))));
            case 0xD8:
                return wasm__neon_store_u64x2(vcltq_s64(a, b));
            case 0xD9:
                return wasm__neon_store_u64x2(vcgtq_s64(a, b));
            case 0xDA:
                return wasm__neon_store_u64x2(vcleq_s64(a, b));
            default:
                return wasm__neon_store_u64x2(vcgeq_s64(a, b));
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        int64_t a = wasm__v128_get_i64(lhs, lane);
        int64_t b = wasm__v128_get_i64(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0xD6:
                truth = (a == b);
                break;
            case 0xD7:
                truth = (a != b);
                break;
            case 0xD8:
                truth = (a < b);
                break;
            case 0xD9:
                truth = (a > b);
                break;
            case 0xDA:
                truth = (a <= b);
                break;
            default:
                truth = (a >= b);
                break;
        }

        wasm__v128_set_u64(&result, lane, truth ? UINT64_MAX : 0u);
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_cmp_f32x4(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        float32x4_t a = wasm__neon_load_f32x4(lhs);
        float32x4_t b = wasm__neon_load_f32x4(rhs);

        switch (subop) {
            case 0x41:
                return wasm__neon_store_u32x4(vceqq_f32(a, b));
            case 0x42:
                return wasm__neon_store_u32x4(vmvnq_u32(vceqq_f32(a, b)));
            case 0x43:
                return wasm__neon_store_u32x4(vcltq_f32(a, b));
            case 0x44:
                return wasm__neon_store_u32x4(vcgtq_f32(a, b));
            case 0x45:
                return wasm__neon_store_u32x4(vcleq_f32(a, b));
            default:
                return wasm__neon_store_u32x4(vcgeq_f32(a, b));
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128 a = wasm__sse_load_ps128(lhs);
        __m128 b = wasm__sse_load_ps128(rhs);

        switch (subop) {
            case 0x41:
                return wasm__sse_store_ps128(_mm_cmpeq_ps(a, b));
            case 0x42:
                return wasm__sse_store_ps128(_mm_cmpneq_ps(a, b));
            case 0x43:
                return wasm__sse_store_ps128(_mm_cmplt_ps(a, b));
            case 0x44:
                return wasm__sse_store_ps128(_mm_cmpgt_ps(a, b));
            case 0x45:
                return wasm__sse_store_ps128(_mm_cmple_ps(a, b));
            default:
                return wasm__sse_store_ps128(_mm_cmpge_ps(a, b));
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        float a = wasm__v128_get_f32(lhs, lane);
        float b = wasm__v128_get_f32(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x41:
                truth = (a == b);
                break;
            case 0x42:
                truth = (a != b);
                break;
            case 0x43:
                truth = (a < b);
                break;
            case 0x44:
                truth = (a > b);
                break;
            case 0x45:
                truth = (a <= b);
                break;
            default:
                truth = (a >= b);
                break;
        }

        wasm__v128_set_u32(&result, lane, truth ? UINT32_MAX : 0u);
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_cmp_f64x2(uint32_t subop,
                                         const wasm_value_t* lhs,
                                         const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        float64x2_t a = wasm__neon_load_f64x2(lhs);
        float64x2_t b = wasm__neon_load_f64x2(rhs);

        switch (subop) {
            case 0x47:
                return wasm__neon_store_u64x2(vceqq_f64(a, b));
            case 0x48:
                return wasm__neon_store_u64x2(vreinterpretq_u64_u8(vmvnq_u8(vreinterpretq_u8_u64(vceqq_f64(a, b)))));
            case 0x49:
                return wasm__neon_store_u64x2(vcltq_f64(a, b));
            case 0x4A:
                return wasm__neon_store_u64x2(vcgtq_f64(a, b));
            case 0x4B:
                return wasm__neon_store_u64x2(vcleq_f64(a, b));
            default:
                return wasm__neon_store_u64x2(vcgeq_f64(a, b));
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128d a = wasm__sse_load_pd128(lhs);
        __m128d b = wasm__sse_load_pd128(rhs);

        switch (subop) {
            case 0x47:
                return wasm__sse_store_pd128(_mm_cmpeq_pd(a, b));
            case 0x48:
                return wasm__sse_store_pd128(_mm_cmpneq_pd(a, b));
            case 0x49:
                return wasm__sse_store_pd128(_mm_cmplt_pd(a, b));
            case 0x4A:
                return wasm__sse_store_pd128(_mm_cmpgt_pd(a, b));
            case 0x4B:
                return wasm__sse_store_pd128(_mm_cmple_pd(a, b));
            default:
                return wasm__sse_store_pd128(_mm_cmpge_pd(a, b));
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        double a = wasm__v128_get_f64(lhs, lane);
        double b = wasm__v128_get_f64(rhs, lane);
        int truth = 0;

        switch (subop) {
            case 0x47:
                truth = (a == b);
                break;
            case 0x48:
                truth = (a != b);
                break;
            case 0x49:
                truth = (a < b);
                break;
            case 0x4A:
                truth = (a > b);
                break;
            case 0x4B:
                truth = (a <= b);
                break;
            default:
                truth = (a >= b);
                break;
        }

        wasm__v128_set_u64(&result, lane, truth ? UINT64_MAX : 0u);
    }

    return result;
}

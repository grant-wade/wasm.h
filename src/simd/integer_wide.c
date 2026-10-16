#include "../internal.h"

WASM_PRIVATE wasm_value_t wasm__simd_i32x4_unary(uint32_t subop, const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    switch (subop) {
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA: {
            int16x8_t s16 = wasm__neon_load_i16x8(value);
            uint16x8_t u16 = vreinterpretq_u16_s16(s16);

            if (subop == 0xA7) return wasm__neon_store_u32x4(vreinterpretq_u32_s32(vmovl_s16(vget_low_s16(s16))));
            if (subop == 0xA8) return wasm__neon_store_u32x4(vreinterpretq_u32_s32(vmovl_s16(vget_high_s16(s16))));
            if (subop == 0xA9) return wasm__neon_store_u32x4(vmovl_u16(vget_low_u16(u16)));
            return wasm__neon_store_u32x4(vmovl_u16(vget_high_u16(u16)));
        }
        case 0xA0: {
            int32x4_t s32 = wasm__neon_load_i32x4(value);
            uint32x4_t neg = vsubq_u32(vdupq_n_u32(0), vreinterpretq_u32_s32(s32));
            return wasm__neon_store_u32x4(vbslq_u32(vcltq_s32(s32, vdupq_n_s32(0)), neg, vreinterpretq_u32_s32(s32)));
        }
        case 0xA1:
            return wasm__neon_store_u32x4(vsubq_u32(vdupq_n_u32(0), wasm__neon_load_u32x4(value)));
        default:
            break;
    }
#elif defined(WASM__SIMD_USE_SSE2)
    if (subop == 0xA0 || subop == 0xA1) {
        __m128i vec = wasm__sse_load_si128(value);
        __m128i neg = _mm_sub_epi32(_mm_setzero_si128(), vec);
        if (subop == 0xA1) return wasm__sse_store_si128(neg);
        return wasm__sse_store_si128(wasm__sse_select_si128(_mm_cmpgt_epi32(_mm_setzero_si128(), vec), neg, vec));
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA: {
            uint32_t base = (subop == 0xA8 || subop == 0xAA) ? 4u : 0u;
            for (lane = 0; lane < 4; lane++) {
                if (subop == 0xA7 || subop == 0xA8)
                    wasm__v128_set_i32(&result, lane, wasm__v128_get_i16(value, base + lane));
                else
                    wasm__v128_set_u32(&result, lane, wasm__v128_get_u16(value, base + lane));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 4; lane++) {
        int32_t signed_value = wasm__v128_get_i32(value, lane);
        uint32_t wrapped_value = (uint32_t)signed_value;

        switch (subop) {
            case 0xA0:
                wasm__v128_set_u32(&result, lane, signed_value < 0 ? (uint32_t)(0u - wrapped_value) : wrapped_value);
                break;
            default:
                wasm__v128_set_u32(&result, lane, (uint32_t)(0u - wrapped_value));
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i32x4_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount) {
#if defined(WASM__SIMD_USE_NEON)
    {
        amount &= 31u;
        if (subop == 0xAB)
            return wasm__neon_store_u32x4(vshlq_u32(wasm__neon_load_u32x4(value), vdupq_n_s32((int32_t)amount)));
        if (subop == 0xAC)
            return wasm__neon_store_u32x4(vreinterpretq_u32_s32(vshlq_s32(wasm__neon_load_i32x4(value), vdupq_n_s32(-(int32_t)amount))));
        return wasm__neon_store_u32x4(vshlq_u32(wasm__neon_load_u32x4(value), vdupq_n_s32(-(int32_t)amount)));
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128i vec = wasm__sse_load_si128(value);
        __m128i count;

        amount &= 31u;
        count = _mm_cvtsi32_si128((int)amount);
        if (subop == 0xAB) return wasm__sse_store_si128(_mm_sll_epi32(vec, count));
        if (subop == 0xAC) return wasm__sse_store_si128(_mm_sra_epi32(vec, count));
        return wasm__sse_store_si128(_mm_srl_epi32(vec, count));
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    amount &= 31u;
    for (lane = 0; lane < 4; lane++) {
        int32_t signed_value = wasm__v128_get_i32(value, lane);
        uint32_t unsigned_value = wasm__v128_get_u32(value, lane);

        switch (subop) {
            case 0xAB:
                wasm__v128_set_u32(&result, lane, unsigned_value << amount);
                break;
            case 0xAC:
                wasm__v128_set_u32(&result, lane, (uint32_t)(signed_value >> amount));
                break;
            default:
                wasm__v128_set_u32(&result, lane, unsigned_value >> amount);
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i32x4_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    switch (subop) {
        case 0xBA: {
            int32x4_t lo = vmull_s16(vget_low_s16(wasm__neon_load_i16x8(lhs)), vget_low_s16(wasm__neon_load_i16x8(rhs)));
            int32x4_t hi = vmull_s16(vget_high_s16(wasm__neon_load_i16x8(lhs)), vget_high_s16(wasm__neon_load_i16x8(rhs)));
            return wasm__neon_store_u32x4(vpaddq_u32(vreinterpretq_u32_s32(lo), vreinterpretq_u32_s32(hi)));
        }
        case 0xBC:
            return wasm__neon_store_u32x4(vreinterpretq_u32_s32(vmull_s16(vget_low_s16(wasm__neon_load_i16x8(lhs)), vget_low_s16(wasm__neon_load_i16x8(rhs)))));
        case 0xBD:
            return wasm__neon_store_u32x4(vreinterpretq_u32_s32(vmull_s16(vget_high_s16(wasm__neon_load_i16x8(lhs)), vget_high_s16(wasm__neon_load_i16x8(rhs)))));
        case 0xBE:
            return wasm__neon_store_u32x4(vmull_u16(vget_low_u16(wasm__neon_load_u16x8(lhs)), vget_low_u16(wasm__neon_load_u16x8(rhs))));
        case 0xBF:
            return wasm__neon_store_u32x4(vmull_u16(vget_high_u16(wasm__neon_load_u16x8(lhs)), vget_high_u16(wasm__neon_load_u16x8(rhs))));
        default:
            break;
    }
    {
        int32x4_t a_s32 = wasm__neon_load_i32x4(lhs);
        int32x4_t b_s32 = wasm__neon_load_i32x4(rhs);
        uint32x4_t a_u32 = vreinterpretq_u32_s32(a_s32);
        uint32x4_t b_u32 = vreinterpretq_u32_s32(b_s32);

        switch (subop) {
            case 0xAE:
                return wasm__neon_store_u32x4(vaddq_u32(a_u32, b_u32));
            case 0xB1:
                return wasm__neon_store_u32x4(vsubq_u32(a_u32, b_u32));
            case 0xB5:
                return wasm__neon_store_u32x4(vmulq_u32(a_u32, b_u32));
            case 0xB6:
                return wasm__neon_store_u32x4(vreinterpretq_u32_s32(vminq_s32(a_s32, b_s32)));
            case 0xB7:
                return wasm__neon_store_u32x4(vminq_u32(a_u32, b_u32));
            case 0xB8:
                return wasm__neon_store_u32x4(vreinterpretq_u32_s32(vmaxq_s32(a_s32, b_s32)));
            default:
                return wasm__neon_store_u32x4(vmaxq_u32(a_u32, b_u32));
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    switch (subop) {
        case 0xBA:
            return wasm__sse_store_si128(_mm_madd_epi16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0xAE:
            return wasm__sse_store_si128(_mm_add_epi32(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0xB1:
            return wasm__sse_store_si128(_mm_sub_epi32(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0xB6: {
            __m128i a = wasm__sse_load_si128(lhs);
            __m128i b = wasm__sse_load_si128(rhs);
            __m128i gt = _mm_cmpgt_epi32(a, b);
            return wasm__sse_store_si128(wasm__sse_select_si128(gt, b, a));
        }
        case 0xB7: {
            __m128i a = wasm__sse_load_si128(lhs);
            __m128i b = wasm__sse_load_si128(rhs);
            __m128i gt = wasm__sse_cmpgt_epu32(a, b);
            return wasm__sse_store_si128(wasm__sse_select_si128(gt, b, a));
        }
        case 0xB8: {
            __m128i a = wasm__sse_load_si128(lhs);
            __m128i b = wasm__sse_load_si128(rhs);
            __m128i gt = _mm_cmpgt_epi32(a, b);
            return wasm__sse_store_si128(wasm__sse_select_si128(gt, a, b));
        }
        case 0xB9: {
            __m128i a = wasm__sse_load_si128(lhs);
            __m128i b = wasm__sse_load_si128(rhs);
            __m128i gt = wasm__sse_cmpgt_epu32(a, b);
            return wasm__sse_store_si128(wasm__sse_select_si128(gt, a, b));
        }
        default:
            break;
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0xBA:
            for (lane = 0; lane < 4; lane++) {
                uint32_t base = lane * 2u;
                uint32_t lo = (uint32_t)((int32_t)wasm__v128_get_i16(lhs, base) *
                                         (int32_t)wasm__v128_get_i16(rhs, base));
                uint32_t hi = (uint32_t)((int32_t)wasm__v128_get_i16(lhs, base + 1u) *
                                         (int32_t)wasm__v128_get_i16(rhs, base + 1u));
                wasm__v128_set_u32(&result, lane, lo + hi);
            }
            return result;
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF: {
            uint32_t base = (subop == 0xBD || subop == 0xBF) ? 4u : 0u;
            for (lane = 0; lane < 4; lane++) {
                uint32_t index = base + lane;
                if (subop == 0xBC || subop == 0xBD)
                    wasm__v128_set_i32(&result, lane,
                                       (int32_t)wasm__v128_get_i16(lhs, index) *
                                           (int32_t)wasm__v128_get_i16(rhs, index));
                else
                    wasm__v128_set_u32(&result, lane,
                                       (uint32_t)wasm__v128_get_u16(lhs, index) *
                                           (uint32_t)wasm__v128_get_u16(rhs, index));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 4; lane++) {
        int32_t a = wasm__v128_get_i32(lhs, lane);
        int32_t b = wasm__v128_get_i32(rhs, lane);
        uint32_t ua = wasm__v128_get_u32(lhs, lane);
        uint32_t ub = wasm__v128_get_u32(rhs, lane);

        switch (subop) {
            case 0xAE:
                wasm__v128_set_u32(&result, lane, ua + ub);
                break;
            case 0xB1:
                wasm__v128_set_u32(&result, lane, ua - ub);
                break;
            case 0xB5:
                wasm__v128_set_u32(&result, lane, ua * ub);
                break;
            case 0xB6:
                wasm__v128_set_i32(&result, lane, (a < b) ? a : b);
                break;
            case 0xB7:
                wasm__v128_set_u32(&result, lane, (ua < ub) ? ua : ub);
                break;
            case 0xB8:
                wasm__v128_set_i32(&result, lane, (a > b) ? a : b);
                break;
            default:
                wasm__v128_set_u32(&result, lane, (ua > ub) ? ua : ub);
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i64x2_unary(uint32_t subop, const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    switch (subop) {
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA: {
            int32x4_t s32 = wasm__neon_load_i32x4(value);
            uint32x4_t u32 = vreinterpretq_u32_s32(s32);
            if (subop == 0xC7) return wasm__neon_store_u64x2(vreinterpretq_u64_s64(vmovl_s32(vget_low_s32(s32))));
            if (subop == 0xC8) return wasm__neon_store_u64x2(vreinterpretq_u64_s64(vmovl_s32(vget_high_s32(s32))));
            if (subop == 0xC9) return wasm__neon_store_u64x2(vmovl_u32(vget_low_u32(u32)));
            return wasm__neon_store_u64x2(vmovl_u32(vget_high_u32(u32)));
        }
        case 0xC0: {
            int64x2_t s64 = wasm__neon_load_i64x2(value);
            uint64x2_t neg = vsubq_u64(vdupq_n_u64(0), vreinterpretq_u64_s64(s64));
            return wasm__neon_store_u64x2(vbslq_u64(vcltq_s64(s64, vdupq_n_s64(0)), neg, vreinterpretq_u64_s64(s64)));
        }
        case 0xC1:
            return wasm__neon_store_u64x2(vsubq_u64(vdupq_n_u64(0), wasm__neon_load_u64x2(value)));
        default:
            break;
    }
#elif defined(WASM__SIMD_USE_SSE2)
    if (subop == 0xC1) return wasm__sse_store_si128(_mm_sub_epi64(_mm_setzero_si128(), wasm__sse_load_si128(value)));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA: {
            uint32_t base = (subop == 0xC8 || subop == 0xCA) ? 2u : 0u;
            for (lane = 0; lane < 2; lane++) {
                if (subop == 0xC7 || subop == 0xC8)
                    wasm__v128_set_i64(&result, lane, wasm__v128_get_i32(value, base + lane));
                else
                    wasm__v128_set_u64(&result, lane, wasm__v128_get_u32(value, base + lane));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 2; lane++) {
        int64_t signed_value = wasm__v128_get_i64(value, lane);
        uint64_t unsigned_value = wasm__v128_get_u64(value, lane);

        switch (subop) {
            case 0xC0:
                if (signed_value < 0)
                    wasm__v128_set_u64(&result, lane, (uint64_t)(0ull - unsigned_value));
                else
                    wasm__v128_set_u64(&result, lane, unsigned_value);
                break;
            default:
                wasm__v128_set_u64(&result, lane, (uint64_t)(0ull - unsigned_value));
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i64x2_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount) {
#if defined(WASM__SIMD_USE_NEON)
    {
        amount &= 63u;
        if (subop == 0xCB)
            return wasm__neon_store_u64x2(vshlq_u64(wasm__neon_load_u64x2(value), vdupq_n_s64((int64_t)amount)));
        if (subop == 0xCC)
            return wasm__neon_store_u64x2(vreinterpretq_u64_s64(vshlq_s64(wasm__neon_load_i64x2(value), vdupq_n_s64(-(int64_t)amount))));
        return wasm__neon_store_u64x2(vshlq_u64(wasm__neon_load_u64x2(value), vdupq_n_s64(-(int64_t)amount)));
    }
#elif defined(WASM__SIMD_USE_SSE2)
    if (subop != 0xCC) {
        __m128i vec = wasm__sse_load_si128(value);
        __m128i count;

        amount &= 63u;
        count = _mm_cvtsi32_si128((int)amount);
        if (subop == 0xCB) return wasm__sse_store_si128(_mm_sll_epi64(vec, count));
        return wasm__sse_store_si128(_mm_srl_epi64(vec, count));
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    amount &= 63u;
    for (lane = 0; lane < 2; lane++) {
        int64_t signed_value = wasm__v128_get_i64(value, lane);
        uint64_t unsigned_value = wasm__v128_get_u64(value, lane);

        switch (subop) {
            case 0xCB:
                wasm__v128_set_u64(&result, lane, unsigned_value << amount);
                break;
            case 0xCC:
                wasm__v128_set_u64(&result, lane, (uint64_t)(signed_value >> amount));
                break;
            default:
                wasm__v128_set_u64(&result, lane, unsigned_value >> amount);
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i64x2_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    switch (subop) {
        case 0xDC:
            return wasm__neon_store_u64x2(vreinterpretq_u64_s64(vmull_s32(vget_low_s32(wasm__neon_load_i32x4(lhs)), vget_low_s32(wasm__neon_load_i32x4(rhs)))));
        case 0xDD:
            return wasm__neon_store_u64x2(vreinterpretq_u64_s64(vmull_s32(vget_high_s32(wasm__neon_load_i32x4(lhs)), vget_high_s32(wasm__neon_load_i32x4(rhs)))));
        case 0xDE:
            return wasm__neon_store_u64x2(vmull_u32(vget_low_u32(wasm__neon_load_u32x4(lhs)), vget_low_u32(wasm__neon_load_u32x4(rhs))));
        case 0xDF:
            return wasm__neon_store_u64x2(vmull_u32(vget_high_u32(wasm__neon_load_u32x4(lhs)), vget_high_u32(wasm__neon_load_u32x4(rhs))));
        case 0xCE:
            return wasm__neon_store_u64x2(vaddq_u64(wasm__neon_load_u64x2(lhs), wasm__neon_load_u64x2(rhs)));
        case 0xD1:
            return wasm__neon_store_u64x2(vsubq_u64(wasm__neon_load_u64x2(lhs), wasm__neon_load_u64x2(rhs)));
        default:
            break;
    }
#elif defined(WASM__SIMD_USE_SSE2)
    if (subop == 0xCE) return wasm__sse_store_si128(_mm_add_epi64(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
    if (subop == 0xD1) return wasm__sse_store_si128(_mm_sub_epi64(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0xDC:
        case 0xDD:
        case 0xDE:
        case 0xDF: {
            uint32_t base = (subop == 0xDD || subop == 0xDF) ? 2u : 0u;
            for (lane = 0; lane < 2; lane++) {
                uint32_t index = base + lane;
                if (subop == 0xDC || subop == 0xDD)
                    wasm__v128_set_i64(&result, lane,
                                       (int64_t)wasm__v128_get_i32(lhs, index) *
                                           (int64_t)wasm__v128_get_i32(rhs, index));
                else
                    wasm__v128_set_u64(&result, lane,
                                       (uint64_t)wasm__v128_get_u32(lhs, index) *
                                           (uint64_t)wasm__v128_get_u32(rhs, index));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 2; lane++) {
        uint64_t ua = wasm__v128_get_u64(lhs, lane);
        uint64_t ub = wasm__v128_get_u64(rhs, lane);

        switch (subop) {
            case 0xCE:
                wasm__v128_set_u64(&result, lane, ua + ub);
                break;
            case 0xD1:
                wasm__v128_set_u64(&result, lane, ua - ub);
                break;
            default:
                wasm__v128_set_u64(&result, lane, ua * ub);
                break;
        }
    }

    return result;
}

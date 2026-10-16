#include "../internal.h"

/* Use unsigned NEON arithmetic for Wasm wrapping operations: GCC exposes
 * some signed intrinsics as C vector operations instrumented by UBSan. */

WASM_PRIVATE wasm_value_t wasm__simd_i8x16_unary(uint32_t subop, const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    {
        int8x16_t s = wasm__neon_load_i8x16(value);
        uint8x16_t u = vreinterpretq_u8_s8(s);

        switch (subop) {
            case 0x60:
                return wasm__neon_store_u8x16(vbslq_u8(vcltq_s8(s, vdupq_n_s8(0)),
                                                    vsubq_u8(vdupq_n_u8(0), u), u));
            case 0x61:
                return wasm__neon_store_u8x16(vsubq_u8(vdupq_n_u8(0), u));
            default:
                return wasm__neon_store_u8x16(vcntq_u8(u));
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    if (subop == 0x61) return wasm__sse_store_si128(_mm_sub_epi8(_mm_setzero_si128(), wasm__sse_load_si128(value)));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        int8_t signed_value = wasm__v128_get_i8(value, lane);
        uint8_t unsigned_value = wasm__v128_get_u8(value, lane);

        switch (subop) {
            case 0x60:
                wasm__v128_set_u8(&result, lane,
                                  (uint32_t)(uint8_t)(signed_value < 0 ? -signed_value : signed_value));
                break;
            case 0x61:
                wasm__v128_set_u8(&result, lane, (uint32_t)(uint8_t)(-signed_value));
                break;
            default:
                wasm__v128_set_u8(&result, lane, wasm__popcount_u8(unsigned_value));
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i8x16_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount) {
#if defined(WASM__SIMD_USE_NEON)
    {
        int8x16_t counts;

        amount &= 7u;
        if (subop == 0x6B) {
            counts = vdupq_n_s8((int8_t)amount);
            return wasm__neon_store_u8x16(vshlq_u8(wasm__neon_load_u8x16(value), counts));
        }

        counts = vdupq_n_s8((int8_t)(-(int32_t)amount));
        if (subop == 0x6C)
            return wasm__neon_store_u8x16(vreinterpretq_u8_s8(vshlq_s8(wasm__neon_load_i8x16(value), counts)));
        return wasm__neon_store_u8x16(vshlq_u8(wasm__neon_load_u8x16(value), counts));
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    amount &= 7u;
    for (lane = 0; lane < 16; lane++) {
        int8_t signed_value = wasm__v128_get_i8(value, lane);
        uint8_t unsigned_value = wasm__v128_get_u8(value, lane);

        switch (subop) {
            case 0x6B:
                wasm__v128_set_u8(&result, lane, (uint8_t)(unsigned_value << amount));
                break;
            case 0x6C:
                wasm__v128_set_u8(&result, lane, (uint8_t)(signed_value >> amount));
                break;
            default:
                wasm__v128_set_u8(&result, lane, (uint8_t)(unsigned_value >> amount));
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i8x16_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        int8x16_t a_s8 = wasm__neon_load_i8x16(lhs);
        int8x16_t b_s8 = wasm__neon_load_i8x16(rhs);
        uint8x16_t a_u8 = vreinterpretq_u8_s8(a_s8);
        uint8x16_t b_u8 = vreinterpretq_u8_s8(b_s8);

        switch (subop) {
            case 0x65:
                return wasm__neon_store_u8x16(vreinterpretq_u8_s8(vcombine_s8(vqmovn_s16(wasm__neon_load_i16x8(lhs)),
                                                                              vqmovn_s16(wasm__neon_load_i16x8(rhs)))));
            case 0x66:
                return wasm__neon_store_u8x16(vcombine_u8(vqmovun_s16(wasm__neon_load_i16x8(lhs)),
                                                          vqmovun_s16(wasm__neon_load_i16x8(rhs))));
            case 0x6E:
                return wasm__neon_store_u8x16(vaddq_u8(a_u8, b_u8));
            case 0x6F:
                return wasm__neon_store_u8x16(vreinterpretq_u8_s8(vqaddq_s8(a_s8, b_s8)));
            case 0x70:
                return wasm__neon_store_u8x16(vqaddq_u8(a_u8, b_u8));
            case 0x71:
                return wasm__neon_store_u8x16(vsubq_u8(a_u8, b_u8));
            case 0x72:
                return wasm__neon_store_u8x16(vreinterpretq_u8_s8(vqsubq_s8(a_s8, b_s8)));
            case 0x73:
                return wasm__neon_store_u8x16(vqsubq_u8(a_u8, b_u8));
            case 0x76:
                return wasm__neon_store_u8x16(vreinterpretq_u8_s8(vminq_s8(a_s8, b_s8)));
            case 0x77:
                return wasm__neon_store_u8x16(vminq_u8(a_u8, b_u8));
            case 0x78:
                return wasm__neon_store_u8x16(vreinterpretq_u8_s8(vmaxq_s8(a_s8, b_s8)));
            case 0x79:
                return wasm__neon_store_u8x16(vmaxq_u8(a_u8, b_u8));
            case 0x7B:
                return wasm__neon_store_u8x16(vrhaddq_u8(a_u8, b_u8));
            default:
                break;
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128i a = wasm__sse_load_si128(lhs);
        __m128i b = wasm__sse_load_si128(rhs);
        __m128i eq;
        __m128i gt;

        switch (subop) {
            case 0x65:
                return wasm__sse_store_si128(_mm_packs_epi16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
            case 0x66:
                return wasm__sse_store_si128(_mm_packus_epi16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
            case 0x6E:
                return wasm__sse_store_si128(_mm_add_epi8(a, b));
            case 0x6F:
                return wasm__sse_store_si128(_mm_adds_epi8(a, b));
            case 0x70:
                return wasm__sse_store_si128(_mm_adds_epu8(a, b));
            case 0x71:
                return wasm__sse_store_si128(_mm_sub_epi8(a, b));
            case 0x72:
                return wasm__sse_store_si128(_mm_subs_epi8(a, b));
            case 0x73:
                return wasm__sse_store_si128(_mm_subs_epu8(a, b));
            case 0x77:
                return wasm__sse_store_si128(_mm_min_epu8(a, b));
            case 0x79:
                return wasm__sse_store_si128(_mm_max_epu8(a, b));
            case 0x7B:
                return wasm__sse_store_si128(_mm_avg_epu8(a, b));
            case 0x76:
                gt = _mm_cmpgt_epi8(a, b);
                return wasm__sse_store_si128(wasm__sse_select_si128(gt, b, a));
            case 0x78:
                gt = _mm_cmpgt_epi8(a, b);
                return wasm__sse_store_si128(wasm__sse_select_si128(gt, a, b));
            default:
                eq = _mm_cmpeq_epi8(a, b);
                gt = wasm__sse_cmpgt_epu8(a, b);
                return wasm__sse_store_si128(wasm__sse_select_si128(_mm_or_si128(eq, gt), a, b));
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    if (subop == 0x65 || subop == 0x66) {
        for (lane = 0; lane < 8; lane++) {
            int16_t lo = wasm__v128_get_i16(lhs, lane);
            int16_t hi = wasm__v128_get_i16(rhs, lane);

            if (subop == 0x65) {
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_i8((int32_t)lo));
                wasm__v128_set_u8(&result, lane + 8u, (uint8_t)wasm__sat_i8((int32_t)hi));
            } else {
                wasm__v128_set_u8(&result, lane, wasm__sat_u8((int32_t)lo));
                wasm__v128_set_u8(&result, lane + 8u, wasm__sat_u8((int32_t)hi));
            }
        }
        return result;
    }

    for (lane = 0; lane < 16; lane++) {
        int8_t a = wasm__v128_get_i8(lhs, lane);
        int8_t b = wasm__v128_get_i8(rhs, lane);
        uint8_t ua = wasm__v128_get_u8(lhs, lane);
        uint8_t ub = wasm__v128_get_u8(rhs, lane);

        switch (subop) {
            case 0x6E:
                wasm__v128_set_u8(&result, lane, (uint8_t)(ua + ub));
                break;
            case 0x6F:
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_i8((int32_t)a + (int32_t)b));
                break;
            case 0x70:
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_u8((int32_t)ua + (int32_t)ub));
                break;
            case 0x71:
                wasm__v128_set_u8(&result, lane, (uint8_t)(ua - ub));
                break;
            case 0x72:
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_i8((int32_t)a - (int32_t)b));
                break;
            case 0x73:
                wasm__v128_set_u8(&result, lane, (uint8_t)wasm__sat_u8((int32_t)ua - (int32_t)ub));
                break;
            case 0x76:
                wasm__v128_set_u8(&result, lane, (uint8_t)((a < b) ? a : b));
                break;
            case 0x77:
                wasm__v128_set_u8(&result, lane, (ua < ub) ? ua : ub);
                break;
            case 0x78:
                wasm__v128_set_u8(&result, lane, (uint8_t)((a > b) ? a : b));
                break;
            case 0x79:
                wasm__v128_set_u8(&result, lane, (ua > ub) ? ua : ub);
                break;
            default:
                wasm__v128_set_u8(&result, lane, (uint8_t)(((uint32_t)ua + (uint32_t)ub + 1u) >> 1));
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i16x8_unary(uint32_t subop, const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    switch (subop) {
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A: {
            int8x16_t s8 = wasm__neon_load_i8x16(value);
            uint8x16_t u8 = vreinterpretq_u8_s8(s8);

            if (subop == 0x87) return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vmovl_s8(vget_low_s8(s8))));
            if (subop == 0x88) return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vmovl_s8(vget_high_s8(s8))));
            if (subop == 0x89) return wasm__neon_store_u16x8(vmovl_u8(vget_low_u8(u8)));
            return wasm__neon_store_u16x8(vmovl_u8(vget_high_u8(u8)));
        }
        case 0x80: {
            int16x8_t s16 = wasm__neon_load_i16x8(value);
            uint16x8_t u16 = vreinterpretq_u16_s16(s16);
            return wasm__neon_store_u16x8(vbslq_u16(vcltq_s16(s16, vdupq_n_s16(0)),
                                                  vsubq_u16(vdupq_n_u16(0), u16), u16));
        }
        case 0x81:
            return wasm__neon_store_u16x8(vsubq_u16(vdupq_n_u16(0), wasm__neon_load_u16x8(value)));
        default:
            break;
    }
#elif defined(WASM__SIMD_USE_SSE2)
    if (subop == 0x81) return wasm__sse_store_si128(_mm_sub_epi16(_mm_setzero_si128(), wasm__sse_load_si128(value)));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A: {
            uint32_t base = (subop == 0x88 || subop == 0x8A) ? 8u : 0u;
            for (lane = 0; lane < 8; lane++) {
                if (subop == 0x87 || subop == 0x88)
                    wasm__v128_set_i16(&result, lane, wasm__v128_get_i8(value, base + lane));
                else
                    wasm__v128_set_u16(&result, lane, wasm__v128_get_u8(value, base + lane));
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 8; lane++) {
        int16_t signed_value = wasm__v128_get_i16(value, lane);

        switch (subop) {
            case 0x80:
                wasm__v128_set_u16(&result, lane,
                                   (uint16_t)(signed_value < 0 ? -signed_value : signed_value));
                break;
            default:
                wasm__v128_set_u16(&result, lane, (uint16_t)(-signed_value));
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i16x8_shift(uint32_t subop,
                                           const wasm_value_t* value,
                                           uint32_t amount) {
#if defined(WASM__SIMD_USE_NEON)
    {
        amount &= 15u;
        if (subop == 0x8B)
            return wasm__neon_store_u16x8(vshlq_u16(wasm__neon_load_u16x8(value), vdupq_n_s16((int16_t)amount)));
        if (subop == 0x8C)
            return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vshlq_s16(wasm__neon_load_i16x8(value), vdupq_n_s16((int16_t)(-(int32_t)amount)))));
        return wasm__neon_store_u16x8(vshlq_u16(wasm__neon_load_u16x8(value), vdupq_n_s16((int16_t)(-(int32_t)amount))));
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128i vec = wasm__sse_load_si128(value);
        __m128i count = _mm_cvtsi32_si128((int)amount);

        amount &= 15u;
        count = _mm_cvtsi32_si128((int)amount);
        if (subop == 0x8B) return wasm__sse_store_si128(_mm_sll_epi16(vec, count));
        if (subop == 0x8C) return wasm__sse_store_si128(_mm_sra_epi16(vec, count));
        return wasm__sse_store_si128(_mm_srl_epi16(vec, count));
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    amount &= 15u;
    for (lane = 0; lane < 8; lane++) {
        int16_t signed_value = wasm__v128_get_i16(value, lane);
        uint16_t unsigned_value = wasm__v128_get_u16(value, lane);

        switch (subop) {
            case 0x8B:
                wasm__v128_set_u16(&result, lane, (uint16_t)(unsigned_value << amount));
                break;
            case 0x8C:
                wasm__v128_set_u16(&result, lane, (uint16_t)(signed_value >> amount));
                break;
            default:
                wasm__v128_set_u16(&result, lane, (uint16_t)(unsigned_value >> amount));
                break;
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_i16x8_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    switch (subop) {
        case 0x82:
            return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vqrdmulhq_s16(wasm__neon_load_i16x8(lhs),
                                                                              wasm__neon_load_i16x8(rhs))));
        case 0x85:
        case 0x86: {
            uint32x4_t a = wasm__neon_load_u32x4(lhs);
            uint32x4_t b = wasm__neon_load_u32x4(rhs);
            if (subop == 0x85)
                return wasm__neon_store_u16x8(vcombine_u16(vreinterpret_u16_s16(vqmovn_s32(vreinterpretq_s32_u32(a))),
                                                           vreinterpret_u16_s16(vqmovn_s32(vreinterpretq_s32_u32(b)))));
            return wasm__neon_store_u16x8(vcombine_u16(vqmovun_s32(vreinterpretq_s32_u32(a)),
                                                       vqmovun_s32(vreinterpretq_s32_u32(b))));
        }
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F: {
            int8x16_t a_s8 = wasm__neon_load_i8x16(lhs);
            int8x16_t b_s8 = wasm__neon_load_i8x16(rhs);
            uint8x16_t a_u8 = vreinterpretq_u8_s8(a_s8);
            uint8x16_t b_u8 = vreinterpretq_u8_s8(b_s8);
            if (subop == 0x9C) return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vmull_s8(vget_low_s8(a_s8), vget_low_s8(b_s8))));
            if (subop == 0x9D) return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vmull_s8(vget_high_s8(a_s8), vget_high_s8(b_s8))));
            if (subop == 0x9E) return wasm__neon_store_u16x8(vmull_u8(vget_low_u8(a_u8), vget_low_u8(b_u8)));
            return wasm__neon_store_u16x8(vmull_u8(vget_high_u8(a_u8), vget_high_u8(b_u8)));
        }
        default:
            break;
    }
    {
        int16x8_t a_s16 = wasm__neon_load_i16x8(lhs);
        int16x8_t b_s16 = wasm__neon_load_i16x8(rhs);
        uint16x8_t a_u16 = vreinterpretq_u16_s16(a_s16);
        uint16x8_t b_u16 = vreinterpretq_u16_s16(b_s16);
        uint16x8_t eq;
        uint16x8_t gt;

        switch (subop) {
            case 0x8E:
                return wasm__neon_store_u16x8(vaddq_u16(a_u16, b_u16));
            case 0x8F:
                return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vqaddq_s16(a_s16, b_s16)));
            case 0x90:
                return wasm__neon_store_u16x8(vqaddq_u16(a_u16, b_u16));
            case 0x91:
                return wasm__neon_store_u16x8(vsubq_u16(a_u16, b_u16));
            case 0x92:
                return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vqsubq_s16(a_s16, b_s16)));
            case 0x93:
                return wasm__neon_store_u16x8(vqsubq_u16(a_u16, b_u16));
            case 0x95:
                return wasm__neon_store_u16x8(vmulq_u16(a_u16, b_u16));
            case 0x96:
                return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vminq_s16(a_s16, b_s16)));
            case 0x97:
                return wasm__neon_store_u16x8(vminq_u16(a_u16, b_u16));
            case 0x98:
                return wasm__neon_store_u16x8(vreinterpretq_u16_s16(vmaxq_s16(a_s16, b_s16)));
            case 0x99:
                return wasm__neon_store_u16x8(vmaxq_u16(a_u16, b_u16));
            case 0x9B:
                return wasm__neon_store_u16x8(vrhaddq_u16(a_u16, b_u16));
            default:
                eq = vceqq_u16(a_u16, b_u16);
                gt = vcgtq_u16(a_u16, b_u16);
                return wasm__neon_store_u16x8(vbslq_u16(vorrq_u16(eq, gt), a_u16, b_u16));
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    switch (subop) {
        case 0x8E:
            return wasm__sse_store_si128(_mm_add_epi16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0x8F:
            return wasm__sse_store_si128(_mm_adds_epi16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0x90:
            return wasm__sse_store_si128(_mm_adds_epu16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0x91:
            return wasm__sse_store_si128(_mm_sub_epi16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0x92:
            return wasm__sse_store_si128(_mm_subs_epi16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0x93:
            return wasm__sse_store_si128(_mm_subs_epu16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0x95:
            return wasm__sse_store_si128(_mm_mullo_epi16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        case 0x96: {
            __m128i a = wasm__sse_load_si128(lhs);
            __m128i b = wasm__sse_load_si128(rhs);
            __m128i gt = _mm_cmpgt_epi16(a, b);
            return wasm__sse_store_si128(wasm__sse_select_si128(gt, b, a));
        }
        case 0x97: {
            __m128i a = wasm__sse_load_si128(lhs);
            __m128i b = wasm__sse_load_si128(rhs);
            __m128i gt = wasm__sse_cmpgt_epu16(a, b);
            return wasm__sse_store_si128(wasm__sse_select_si128(gt, b, a));
        }
        case 0x98: {
            __m128i a = wasm__sse_load_si128(lhs);
            __m128i b = wasm__sse_load_si128(rhs);
            __m128i gt = _mm_cmpgt_epi16(a, b);
            return wasm__sse_store_si128(wasm__sse_select_si128(gt, a, b));
        }
        case 0x99: {
            __m128i a = wasm__sse_load_si128(lhs);
            __m128i b = wasm__sse_load_si128(rhs);
            __m128i gt = wasm__sse_cmpgt_epu16(a, b);
            return wasm__sse_store_si128(wasm__sse_select_si128(gt, a, b));
        }
        case 0x9B:
            return wasm__sse_store_si128(_mm_avg_epu16(wasm__sse_load_si128(lhs), wasm__sse_load_si128(rhs)));
        default:
            break;
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0x82:
            for (lane = 0; lane < 8; lane++) {
                int32_t product = (int32_t)wasm__v128_get_i16(lhs, lane) * (int32_t)wasm__v128_get_i16(rhs, lane);
                int32_t rounded = (product + 0x4000) >> 15;
                wasm__v128_set_i16(&result, lane, wasm__sat_i16(rounded));
            }
            return result;
        case 0x85:
        case 0x86:
            for (lane = 0; lane < 4; lane++) {
                int32_t lo = wasm__v128_get_i32(lhs, lane);
                int32_t hi = wasm__v128_get_i32(rhs, lane);
                if (subop == 0x85) {
                    wasm__v128_set_i16(&result, lane, wasm__sat_i16(lo));
                    wasm__v128_set_i16(&result, lane + 4u, wasm__sat_i16(hi));
                } else {
                    wasm__v128_set_u16(&result, lane, wasm__sat_u16(lo));
                    wasm__v128_set_u16(&result, lane + 4u, wasm__sat_u16(hi));
                }
            }
            return result;
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F: {
            uint32_t base = (subop == 0x9D || subop == 0x9F) ? 8u : 0u;
            for (lane = 0; lane < 8; lane++) {
                uint32_t index = base + lane;
                if (subop == 0x9C || subop == 0x9D) {
                    int16_t product = (int16_t)((int32_t)wasm__v128_get_i8(lhs, index) *
                                                (int32_t)wasm__v128_get_i8(rhs, index));
                    wasm__v128_set_i16(&result, lane, product);
                } else {
                    uint16_t product = (uint16_t)((uint32_t)wasm__v128_get_u8(lhs, index) *
                                                  (uint32_t)wasm__v128_get_u8(rhs, index));
                    wasm__v128_set_u16(&result, lane, product);
                }
            }
            return result;
        }
        default:
            break;
    }

    for (lane = 0; lane < 8; lane++) {
        int16_t a = wasm__v128_get_i16(lhs, lane);
        int16_t b = wasm__v128_get_i16(rhs, lane);
        uint16_t ua = wasm__v128_get_u16(lhs, lane);
        uint16_t ub = wasm__v128_get_u16(rhs, lane);

        switch (subop) {
            case 0x8E:
                wasm__v128_set_u16(&result, lane, (uint16_t)(ua + ub));
                break;
            case 0x8F:
                wasm__v128_set_i16(&result, lane, wasm__sat_i16((int32_t)a + (int32_t)b));
                break;
            case 0x90:
                wasm__v128_set_u16(&result, lane, wasm__sat_u16((int32_t)ua + (int32_t)ub));
                break;
            case 0x91:
                wasm__v128_set_u16(&result, lane, (uint16_t)(ua - ub));
                break;
            case 0x92:
                wasm__v128_set_i16(&result, lane, wasm__sat_i16((int32_t)a - (int32_t)b));
                break;
            case 0x93:
                wasm__v128_set_u16(&result, lane, wasm__sat_u16((int32_t)ua - (int32_t)ub));
                break;
            case 0x95:
                wasm__v128_set_u16(&result, lane, (uint16_t)((uint32_t)ua * (uint32_t)ub));
                break;
            case 0x96:
                wasm__v128_set_i16(&result, lane, (a < b) ? a : b);
                break;
            case 0x97:
                wasm__v128_set_u16(&result, lane, (ua < ub) ? ua : ub);
                break;
            case 0x98:
                wasm__v128_set_i16(&result, lane, (a > b) ? a : b);
                break;
            case 0x99:
                wasm__v128_set_u16(&result, lane, (ua > ub) ? ua : ub);
                break;
            default:
                wasm__v128_set_u16(&result, lane, (uint16_t)(((uint32_t)ua + (uint32_t)ub + 1u) >> 1));
                break;
        }
    }

    return result;
}

#include "float.h"
#include "native.h"
#include "../values.h"
#include "lanes.h"
#include <math.h>

WASM_PRIVATE wasm_value_t wasm__simd_f32x4_unary(uint32_t subop, const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    if (subop == 0xE0) return wasm__neon_store_f32x4(vabsq_f32(wasm__neon_load_f32x4(value)));
    if (subop == 0xE1) return wasm__neon_store_f32x4(vnegq_f32(wasm__neon_load_f32x4(value)));
    if (subop == 0xE3) return wasm__neon_store_f32x4(vsqrtq_f32(wasm__neon_load_f32x4(value)));
#elif defined(WASM__SIMD_USE_SSE2)
    if (subop == 0xE0) return wasm__sse_store_ps128(wasm__sse_abs_ps(wasm__sse_load_ps128(value)));
    if (subop == 0xE1) return wasm__sse_store_ps128(wasm__sse_neg_ps(wasm__sse_load_ps128(value)));
    if (subop == 0xE3) return wasm__sse_store_ps128(_mm_sqrt_ps(wasm__sse_load_ps128(value)));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    /* abs and neg manipulate the representation, including signaling NaNs. */
    if (subop == 0xE0 || subop == 0xE1) {
        for (lane = 0; lane < 4; lane++) {
            uint32_t bits = wasm__v128_get_u32(value, lane);
            bits = subop == 0xE0 ? bits & 0x7FFFFFFFu : bits ^ 0x80000000u;
            wasm__v128_set_u32(&result, lane, bits);
        }
        return result;
    }

    for (lane = 0; lane < 4; lane++) {
        float current = wasm__v128_get_f32(value, lane);
        float out = current;

        switch (subop) {
            case 0x67:
                out = wasm__ceil_f32(current);
                break;
            case 0x68:
                out = wasm__floor_f32(current);
                break;
            case 0x69:
                out = wasm__trunc_f32(current);
                break;
            case 0x6A:
                out = wasm__nearest_f32(current);
                break;
            case 0xE0:
                out = fabsf(current);
                break;
            case 0xE1:
                out = -current;
                break;
            default:
                out = wasm__sqrt_f32(current);
                break;
        }

        wasm__v128_set_f32(&result, lane, out);
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_f32x4_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        float32x4_t a = wasm__neon_load_f32x4(lhs);
        float32x4_t b = wasm__neon_load_f32x4(rhs);

        switch (subop) {
            case 0xE4:
                return wasm__neon_store_f32x4(vaddq_f32(a, b));
            case 0xE5:
                return wasm__neon_store_f32x4(vsubq_f32(a, b));
            case 0xE6:
                return wasm__neon_store_f32x4(vmulq_f32(a, b));
            case 0xE7:
                return wasm__neon_store_f32x4(vdivq_f32(a, b));
            default:
                break;
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128 a = wasm__sse_load_ps128(lhs);
        __m128 b = wasm__sse_load_ps128(rhs);

        switch (subop) {
            case 0xE4:
                return wasm__sse_store_ps128(_mm_add_ps(a, b));
            case 0xE5:
                return wasm__sse_store_ps128(_mm_sub_ps(a, b));
            case 0xE6:
                return wasm__sse_store_ps128(_mm_mul_ps(a, b));
            case 0xE7:
                return wasm__sse_store_ps128(_mm_div_ps(a, b));
            default:
                break;
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        float a = wasm__v128_get_f32(lhs, lane);
        float b = wasm__v128_get_f32(rhs, lane);
        float out;

        switch (subop) {
            case 0xE4:
                out = a + b;
                break;
            case 0xE5:
                out = a - b;
                break;
            case 0xE6:
                out = a * b;
                break;
            case 0xE7:
                out = a / b;
                break;
            case 0xE8:
                out = wasm__f32_min(a, b);
                break;
            case 0xE9:
                out = wasm__f32_max(a, b);
                break;
            case 0xEA:
                out = (b < a) ? b : a;
                break;
            default:
                out = (a < b) ? b : a;
                break;
        }

        if (subop == 0xEA || subop == 0xEB) {
            const wasm_value_t* selected = (subop == 0xEA ? b < a : a < b) ? rhs : lhs;
            wasm__v128_set_u32(&result, lane, wasm__v128_get_u32(selected, lane));
        } else {
            wasm__v128_set_f32(&result, lane, out);
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_f64x2_unary(uint32_t subop, const wasm_value_t* value) {
#if defined(WASM__SIMD_USE_NEON)
    if (subop == 0xEC) return wasm__neon_store_f64x2(vabsq_f64(wasm__neon_load_f64x2(value)));
    if (subop == 0xED) return wasm__neon_store_f64x2(vnegq_f64(wasm__neon_load_f64x2(value)));
    if (subop == 0xEF) return wasm__neon_store_f64x2(vsqrtq_f64(wasm__neon_load_f64x2(value)));
#elif defined(WASM__SIMD_USE_SSE2)
    if (subop == 0xEC) return wasm__sse_store_pd128(wasm__sse_abs_pd(wasm__sse_load_pd128(value)));
    if (subop == 0xED) return wasm__sse_store_pd128(wasm__sse_neg_pd(wasm__sse_load_pd128(value)));
    if (subop == 0xEF) return wasm__sse_store_pd128(_mm_sqrt_pd(wasm__sse_load_pd128(value)));
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    /* abs and neg manipulate the representation, including signaling NaNs. */
    if (subop == 0xEC || subop == 0xED) {
        for (lane = 0; lane < 2; lane++) {
            uint64_t bits = wasm__v128_get_u64(value, lane);
            bits = subop == 0xEC ? bits & UINT64_C(0x7FFFFFFFFFFFFFFF) : bits ^ UINT64_C(0x8000000000000000);
            wasm__v128_set_u64(&result, lane, bits);
        }
        return result;
    }

    for (lane = 0; lane < 2; lane++) {
        double current = wasm__v128_get_f64(value, lane);
        double out = current;

        switch (subop) {
            case 0x74:
                out = wasm__ceil_f64(current);
                break;
            case 0x75:
                out = wasm__floor_f64(current);
                break;
            case 0x7A:
                out = wasm__trunc_f64(current);
                break;
            case 0x94:
                out = wasm__nearest_f64(current);
                break;
            case 0xEC:
                out = fabs(current);
                break;
            case 0xED:
                out = -current;
                break;
            default:
                out = wasm__sqrt_f64(current);
                break;
        }

        wasm__v128_set_f64(&result, lane, out);
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_f64x2_binary(uint32_t subop,
                                            const wasm_value_t* lhs,
                                            const wasm_value_t* rhs) {
#if defined(WASM__SIMD_USE_NEON)
    {
        float64x2_t a = wasm__neon_load_f64x2(lhs);
        float64x2_t b = wasm__neon_load_f64x2(rhs);

        switch (subop) {
            case 0xF0:
                return wasm__neon_store_f64x2(vaddq_f64(a, b));
            case 0xF1:
                return wasm__neon_store_f64x2(vsubq_f64(a, b));
            case 0xF2:
                return wasm__neon_store_f64x2(vmulq_f64(a, b));
            case 0xF3:
                return wasm__neon_store_f64x2(vdivq_f64(a, b));
            default:
                break;
        }
    }
#elif defined(WASM__SIMD_USE_SSE2)
    {
        __m128d a = wasm__sse_load_pd128(lhs);
        __m128d b = wasm__sse_load_pd128(rhs);

        switch (subop) {
            case 0xF0:
                return wasm__sse_store_pd128(_mm_add_pd(a, b));
            case 0xF1:
                return wasm__sse_store_pd128(_mm_sub_pd(a, b));
            case 0xF2:
                return wasm__sse_store_pd128(_mm_mul_pd(a, b));
            case 0xF3:
                return wasm__sse_store_pd128(_mm_div_pd(a, b));
            default:
                break;
        }
    }
#endif
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        double a = wasm__v128_get_f64(lhs, lane);
        double b = wasm__v128_get_f64(rhs, lane);
        double out;

        switch (subop) {
            case 0xF0:
                out = a + b;
                break;
            case 0xF1:
                out = a - b;
                break;
            case 0xF2:
                out = a * b;
                break;
            case 0xF3:
                out = a / b;
                break;
            case 0xF4:
                out = wasm__f64_min(a, b);
                break;
            case 0xF5:
                out = wasm__f64_max(a, b);
                break;
            case 0xF6:
                out = (b < a) ? b : a;
                break;
            default:
                out = (a < b) ? b : a;
                break;
        }

        if (subop == 0xF6 || subop == 0xF7) {
            const wasm_value_t* selected = (subop == 0xF6 ? b < a : a < b) ? rhs : lhs;
            wasm__v128_set_u64(&result, lane, wasm__v128_get_u64(selected, lane));
        } else {
            wasm__v128_set_f64(&result, lane, out);
        }
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_demote_f64x2_zero(const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();

    wasm__v128_set_f32(&result, 0, (float)wasm__v128_get_f64(value, 0));
    wasm__v128_set_f32(&result, 1, (float)wasm__v128_get_f64(value, 1));
    wasm__v128_set_u32(&result, 2, 0u);
    wasm__v128_set_u32(&result, 3, 0u);
    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_promote_low_f32x4(const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();

    wasm__v128_set_f64(&result, 0, (double)wasm__v128_get_f32(value, 0));
    wasm__v128_set_f64(&result, 1, (double)wasm__v128_get_f32(value, 1));
    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_extadd_pairwise(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    switch (subop) {
        case 0x7C:
        case 0x7D:
            for (lane = 0; lane < 8; lane++) {
                uint32_t base = lane * 2u;
                if (subop == 0x7C)
                    wasm__v128_set_i16(&result, lane,
                                       (int32_t)wasm__v128_get_i8(value, base) +
                                           (int32_t)wasm__v128_get_i8(value, base + 1u));
                else
                    wasm__v128_set_u16(&result, lane,
                                       (uint32_t)wasm__v128_get_u8(value, base) +
                                           (uint32_t)wasm__v128_get_u8(value, base + 1u));
            }
            break;
        default:
            for (lane = 0; lane < 4; lane++) {
                uint32_t base = lane * 2u;
                if (subop == 0x7E)
                    wasm__v128_set_i32(&result, lane,
                                       (int32_t)wasm__v128_get_i16(value, base) +
                                           (int32_t)wasm__v128_get_i16(value, base + 1u));
                else
                    wasm__v128_set_u32(&result, lane,
                                       (uint32_t)wasm__v128_get_u16(value, base) +
                                           (uint32_t)wasm__v128_get_u16(value, base + 1u));
            }
            break;
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_trunc_sat_f32x4(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        wasm_value_t converted =
            (subop == 0xF8) ? wasm__trunc_sat_i32_s((double)wasm__v128_get_f32(value, lane))
                            : wasm__trunc_sat_i32_u((double)wasm__v128_get_f32(value, lane));
        wasm__v128_set_i32(&result, lane, converted.of.i32);
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_convert_i32x4(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        if (subop == 0xFA)
            wasm__v128_set_f32(&result, lane, (float)wasm__v128_get_i32(value, lane));
        else
            wasm__v128_set_f32(&result, lane, (float)(double)wasm__v128_get_u32(value, lane));
    }

    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_trunc_sat_f64x2_zero(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        wasm_value_t converted =
            (subop == 0xFC) ? wasm__trunc_sat_i32_s(wasm__v128_get_f64(value, lane))
                            : wasm__trunc_sat_i32_u(wasm__v128_get_f64(value, lane));
        wasm__v128_set_i32(&result, lane, converted.of.i32);
    }
    wasm__v128_set_u32(&result, 2, 0u);
    wasm__v128_set_u32(&result, 3, 0u);
    return result;
}

WASM_PRIVATE wasm_value_t wasm__simd_convert_low_i32x4_to_f64x2(uint32_t subop, const wasm_value_t* value) {
    wasm_value_t result = wasm_v128_zero();

    if (subop == 0xFE) {
        wasm__v128_set_f64(&result, 0, (double)wasm__v128_get_i32(value, 0));
        wasm__v128_set_f64(&result, 1, (double)wasm__v128_get_i32(value, 1));
    } else {
        wasm__v128_set_f64(&result, 0, (double)wasm__v128_get_u32(value, 0));
        wasm__v128_set_f64(&result, 1, (double)wasm__v128_get_u32(value, 1));
    }

    return result;
}

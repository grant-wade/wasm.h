#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/simd/compare.h"
#include "../../../src/simd/float.h"
#include "../../../src/simd/integer_small.h"
#include "../../../src/simd/integer_wide.h"
#include "../../../src/simd/lanes.h"
#endif

static wasm_value_t wasm_test_v128_from_i8x16(const int8_t lanes[16]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) wasm__v128_set_u8(&value, lane, (uint8_t)lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_i16x8(const int16_t lanes[8]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 8; lane++) wasm__v128_set_i16(&value, lane, lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_i32x4(const int32_t lanes[4]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) wasm__v128_set_i32(&value, lane, lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_i64x2(const int64_t lanes[2]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) wasm__v128_set_i64(&value, lane, lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_f32x4(const float lanes[4]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) wasm__v128_set_f32(&value, lane, lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_f64x2(const double lanes[2]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) wasm__v128_set_f64(&value, lane, lanes[lane]);
    return value;
}

static void wasm_test_expect_i16x8(test_ctx* t,
                                   const wasm_value_t* actual,
                                   const int16_t expected[8],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 8; lane++) {
        CHECK_MSG(t,
                  wasm__v128_get_i16(actual, lane) == expected[lane],
                  "%s lane %u: expected %d, got %d",
                  label,
                  (unsigned)lane,
                  (int)expected[lane],
                  (int)wasm__v128_get_i16(actual, lane));
    }
}

static void wasm_test_expect_i32x4(test_ctx* t,
                                   const wasm_value_t* actual,
                                   const int32_t expected[4],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        CHECK_MSG(t,
                  wasm__v128_get_i32(actual, lane) == expected[lane],
                  "%s lane %u: expected %d, got %d",
                  label,
                  (unsigned)lane,
                  expected[lane],
                  wasm__v128_get_i32(actual, lane));
    }
}

static void wasm_test_expect_i64x2(test_ctx* t,
                                   const wasm_value_t* actual,
                                   const int64_t expected[2],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        CHECK_MSG(t,
                  wasm__v128_get_i64(actual, lane) == expected[lane],
                  "%s lane %u: expected %lld, got %lld",
                  label,
                  (unsigned)lane,
                  (long long)expected[lane],
                  (long long)wasm__v128_get_i64(actual, lane));
    }
}

static void wasm_test_expect_f32x4(test_ctx* t,
                                   const wasm_value_t* actual,
                                   const float expected[4],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        float got = wasm__v128_get_f32(actual, lane);
        CHECK_MSG(t,
                  fabsf(got - expected[lane]) <= 1e-6f,
                  "%s lane %u: expected %.6f, got %.6f",
                  label,
                  (unsigned)lane,
                  expected[lane],
                  got);
    }
}

static void wasm_test_expect_f64x2(test_ctx* t,
                                   const wasm_value_t* actual,
                                   const double expected[2],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        double got = wasm__v128_get_f64(actual, lane);
        CHECK_MSG(t,
                  fabs(got - expected[lane]) <= 1e-9,
                  "%s lane %u: expected %.9f, got %.9f",
                  label,
                  (unsigned)lane,
                  expected[lane],
                  got);
    }
}

TEST(test_simd_helper_ops) {
    static const int8_t i8_a[16] = { -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7 };
    static const int8_t i8_b[16] = { 7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, -8 };
    static const int16_t i16_a[8] = { -300, -200, -100, -1, 1, 100, 200, 300 };
    static const int16_t i16_b[8] = { 150, -150, 50, -50, 25, -25, 10, -10 };
    static const int32_t i32_a[4] = { -100000, -1000, 1000, 100000 };
    static const int32_t i32_b[4] = { 33333, -2000, 2000, -33333 };
    static const int64_t i64_a[2] = { -5000000000LL, 5000000000LL };
    static const int64_t i64_b[2] = { 3000000000LL, -3000000000LL };
    static const float f32_a[4] = { 1.25f, -2.5f, 3.75f, -4.0f };
    static const float f32_b[4] = { -0.5f, 4.0f, -1.25f, 8.0f };
    static const double f64_a[2] = { 1.5, -8.0 };
    static const double f64_b[2] = { -0.25, 2.0 };
    static const uint32_t i8_cmp_ops[] = { 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C };
    static const uint32_t i16_cmp_ops[] = { 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36 };
    static const uint32_t i32_cmp_ops[] = { 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40 };
    static const uint32_t i64_cmp_ops[] = { 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB };
    static const uint32_t f32_cmp_ops[] = { 0x41, 0x42, 0x43, 0x44, 0x45, 0x46 };
    static const uint32_t f64_cmp_ops[] = { 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C };
    static const uint32_t i8_binary_ops[] = { 0x65, 0x66, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x76, 0x77, 0x78, 0x79, 0x7B };
    static const uint32_t i16_binary_ops[] = { 0x82, 0x85, 0x86, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F };
    static const uint32_t i32_binary_ops[] = { 0xAE, 0xB1, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBC, 0xBD, 0xBE, 0xBF };
    static const uint32_t i64_binary_ops[] = { 0xCE, 0xD1, 0xD5, 0xDC, 0xDD, 0xDE, 0xDF };
    wasm_value_t vi8_a = wasm_test_v128_from_i8x16(i8_a);
    wasm_value_t vi8_b = wasm_test_v128_from_i8x16(i8_b);
    wasm_value_t vi16_a = wasm_test_v128_from_i16x8(i16_a);
    wasm_value_t vi16_b = wasm_test_v128_from_i16x8(i16_b);
    wasm_value_t vi32_a = wasm_test_v128_from_i32x4(i32_a);
    wasm_value_t vi32_b = wasm_test_v128_from_i32x4(i32_b);
    wasm_value_t vi64_a = wasm_test_v128_from_i64x2(i64_a);
    wasm_value_t vi64_b = wasm_test_v128_from_i64x2(i64_b);
    wasm_value_t vf32_a = wasm_test_v128_from_f32x4(f32_a);
    wasm_value_t vf32_b = wasm_test_v128_from_f32x4(f32_b);
    wasm_value_t vf64_a = wasm_test_v128_from_f64x2(f64_a);
    wasm_value_t vf64_b = wasm_test_v128_from_f64x2(f64_b);
    uint32_t index;

    /* abs/neg preserve the minimum signed lane modulo its width. The dot
     * products fit individually, but their pairwise sum wraps at 32 bits. */
    {
        wasm_value_t input = wasm_v128_zero();
        wasm_value_t actual;
        uint32_t lane;
        for (lane = 0; lane < 16; lane++) wasm__v128_set_u8(&input, lane, 0x80u);
        actual = wasm__simd_i8x16_unary(0x60, &input);
        CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i8x16.abs minimum");
        actual = wasm__simd_i8x16_unary(0x61, &input);
        CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i8x16.neg minimum");
        for (lane = 0; lane < 8; lane++) wasm__v128_set_u16(&input, lane, 0x8000u);
        actual = wasm__simd_i16x8_unary(0x80, &input);
        CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i16x8.abs minimum");
        actual = wasm__simd_i16x8_unary(0x81, &input);
        CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i16x8.neg minimum");
        actual = wasm__simd_i32x4_binary(0xBA, &input, &input);
        for (lane = 0; lane < 4; lane++) {
            CHECK_MSG(t, wasm__v128_get_u32(&actual, lane) == 0x80000000u, "%s", "i32x4.dot_i16x8_s wraps sum");
            wasm__v128_set_u32(&input, lane, 0x80000000u);
        }
        actual = wasm__simd_i32x4_unary(0xA0, &input);
        CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i32x4.abs minimum");
        actual = wasm__simd_i32x4_unary(0xA1, &input);
        CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i32x4.neg minimum");
        for (lane = 0; lane < 2; lane++) wasm__v128_set_u64(&input, lane, UINT64_C(0x8000000000000000));
        actual = wasm__simd_i64x2_unary(0xC0, &input);
        CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i64x2.abs minimum");
        actual = wasm__simd_i64x2_unary(0xC1, &input);
        CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i64x2.neg minimum");
    }

    {
        uint8_t bytes[16];
        uint8_t expected[16];
        wasm_value_t actual;
        wasm_value_t input;
        wasm_value_t zero = wasm_v128_zero();

        for (index = 0; index < 16; index++) bytes[index] = (uint8_t)index;
        input = wasm_v128(bytes);
        actual = wasm__simd_bitwise_not(&input);
        for (index = 0; index < 16; index++) expected[index] = (uint8_t)~bytes[index];
        wasm_test_expect_u8x16(t, &actual, expected, "v128.not");

        actual = wasm__simd_bitwise_binary(0x4E, &vi8_a, &vi8_b);
        for (index = 0; index < 16; index++) expected[index] = (uint8_t)i8_a[index] & (uint8_t)i8_b[index];
        wasm_test_expect_u8x16(t, &actual, expected, "v128.and");

        actual = wasm__simd_bitwise_binary(0x50, &vi8_a, &vi8_b);
        for (index = 0; index < 16; index++) expected[index] = (uint8_t)i8_a[index] | (uint8_t)i8_b[index];
        wasm_test_expect_u8x16(t, &actual, expected, "v128.or");

        CHECK_MSG(t, wasm__simd_any_true(&vi8_a), "%s", "expected non-zero vector to be any_true");
        CHECK_MSG(t, !wasm__simd_any_true(&zero), "%s", "expected zero vector to be falsey");
    }

    for (index = 0; index < (uint32_t)(sizeof(i8_cmp_ops) / sizeof(i8_cmp_ops[0])); index++) {
        uint8_t expected[16];
        wasm_value_t actual = wasm__simd_cmp_i8x16(i8_cmp_ops[index], &vi8_a, &vi8_b);
        uint32_t lane;

        for (lane = 0; lane < 16; lane++) {
            int8_t a = i8_a[lane];
            int8_t b = i8_b[lane];
            uint8_t ua = (uint8_t)a;
            uint8_t ub = (uint8_t)b;
            int cond = 0;
            switch (i8_cmp_ops[index]) {
                case 0x23:
                    cond = a == b;
                    break;
                case 0x24:
                    cond = a != b;
                    break;
                case 0x25:
                    cond = a < b;
                    break;
                case 0x26:
                    cond = ua < ub;
                    break;
                case 0x27:
                    cond = a > b;
                    break;
                case 0x28:
                    cond = ua > ub;
                    break;
                case 0x29:
                    cond = a <= b;
                    break;
                case 0x2A:
                    cond = ua <= ub;
                    break;
                case 0x2B:
                    cond = a >= b;
                    break;
                default:
                    cond = ua >= ub;
                    break;
            }
            expected[lane] = cond ? 0xFFu : 0u;
        }
        wasm_test_expect_u8x16(t, &actual, expected, "i8x16 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(i16_cmp_ops) / sizeof(i16_cmp_ops[0])); index++) {
        int16_t expected[8];
        wasm_value_t actual = wasm__simd_cmp_i16x8(i16_cmp_ops[index], &vi16_a, &vi16_b);
        uint32_t lane;

        for (lane = 0; lane < 8; lane++) {
            int cond = 0;
            uint16_t ua = (uint16_t)i16_a[lane];
            uint16_t ub = (uint16_t)i16_b[lane];
            switch (i16_cmp_ops[index]) {
                case 0x2D:
                    cond = i16_a[lane] == i16_b[lane];
                    break;
                case 0x2E:
                    cond = i16_a[lane] != i16_b[lane];
                    break;
                case 0x2F:
                    cond = i16_a[lane] < i16_b[lane];
                    break;
                case 0x30:
                    cond = ua < ub;
                    break;
                case 0x31:
                    cond = i16_a[lane] > i16_b[lane];
                    break;
                case 0x32:
                    cond = ua > ub;
                    break;
                case 0x33:
                    cond = i16_a[lane] <= i16_b[lane];
                    break;
                case 0x34:
                    cond = ua <= ub;
                    break;
                case 0x35:
                    cond = i16_a[lane] >= i16_b[lane];
                    break;
                default:
                    cond = ua >= ub;
                    break;
            }
            expected[lane] = cond ? (int16_t)-1 : 0;
        }
        wasm_test_expect_i16x8(t, &actual, expected, "i16x8 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(i32_cmp_ops) / sizeof(i32_cmp_ops[0])); index++) {
        int32_t expected[4];
        wasm_value_t actual = wasm__simd_cmp_i32x4(i32_cmp_ops[index], &vi32_a, &vi32_b);
        uint32_t lane;

        for (lane = 0; lane < 4; lane++) {
            int cond = 0;
            uint32_t ua = (uint32_t)i32_a[lane];
            uint32_t ub = (uint32_t)i32_b[lane];
            switch (i32_cmp_ops[index]) {
                case 0x37:
                    cond = i32_a[lane] == i32_b[lane];
                    break;
                case 0x38:
                    cond = i32_a[lane] != i32_b[lane];
                    break;
                case 0x39:
                    cond = i32_a[lane] < i32_b[lane];
                    break;
                case 0x3A:
                    cond = ua < ub;
                    break;
                case 0x3B:
                    cond = i32_a[lane] > i32_b[lane];
                    break;
                case 0x3C:
                    cond = ua > ub;
                    break;
                case 0x3D:
                    cond = i32_a[lane] <= i32_b[lane];
                    break;
                case 0x3E:
                    cond = ua <= ub;
                    break;
                case 0x3F:
                    cond = i32_a[lane] >= i32_b[lane];
                    break;
                default:
                    cond = ua >= ub;
                    break;
            }
            expected[lane] = cond ? -1 : 0;
        }
        wasm_test_expect_i32x4(t, &actual, expected, "i32x4 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(i64_cmp_ops) / sizeof(i64_cmp_ops[0])); index++) {
        int64_t expected[2];
        wasm_value_t actual = wasm__simd_cmp_i64x2(i64_cmp_ops[index], &vi64_a, &vi64_b);
        uint32_t lane;

        for (lane = 0; lane < 2; lane++) {
            int cond = 0;
            switch (i64_cmp_ops[index]) {
                case 0xD6:
                    cond = i64_a[lane] == i64_b[lane];
                    break;
                case 0xD7:
                    cond = i64_a[lane] != i64_b[lane];
                    break;
                case 0xD8:
                    cond = i64_a[lane] < i64_b[lane];
                    break;
                case 0xD9:
                    cond = i64_a[lane] > i64_b[lane];
                    break;
                case 0xDA:
                    cond = i64_a[lane] <= i64_b[lane];
                    break;
                default:
                    cond = i64_a[lane] >= i64_b[lane];
                    break;
            }
            expected[lane] = cond ? -1LL : 0LL;
        }
        wasm_test_expect_i64x2(t, &actual, expected, "i64x2 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(i8_binary_ops) / sizeof(i8_binary_ops[0])); index++) {
        wasm_value_t actual = wasm__simd_i8x16_binary(i8_binary_ops[index], &vi16_a, &vi16_b);
        uint8_t expected[16];
        uint32_t lane;

        memset(expected, 0, sizeof(expected));
        if (i8_binary_ops[index] == 0x65 || i8_binary_ops[index] == 0x66) {
            for (lane = 0; lane < 8; lane++) {
                int32_t lo = i16_a[lane];
                int32_t hi = i16_b[lane];
                expected[lane] = (i8_binary_ops[index] == 0x65) ? (uint8_t)wasm__sat_i8(lo) : wasm__sat_u8(lo);
                expected[lane + 8u] = (i8_binary_ops[index] == 0x65) ? (uint8_t)wasm__sat_i8(hi) : wasm__sat_u8(hi);
            }
        } else {
            actual = wasm__simd_i8x16_binary(i8_binary_ops[index], &vi8_a, &vi8_b);
            for (lane = 0; lane < 16; lane++) {
                int8_t a = i8_a[lane];
                int8_t b = i8_b[lane];
                uint8_t ua = (uint8_t)a;
                uint8_t ub = (uint8_t)b;
                switch (i8_binary_ops[index]) {
                    case 0x6E:
                        expected[lane] = (uint8_t)(ua + ub);
                        break;
                    case 0x6F:
                        expected[lane] = (uint8_t)wasm__sat_i8((int32_t)a + (int32_t)b);
                        break;
                    case 0x70:
                        expected[lane] = wasm__sat_u8((int32_t)ua + (int32_t)ub);
                        break;
                    case 0x71:
                        expected[lane] = (uint8_t)(ua - ub);
                        break;
                    case 0x72:
                        expected[lane] = (uint8_t)wasm__sat_i8((int32_t)a - (int32_t)b);
                        break;
                    case 0x73:
                        expected[lane] = wasm__sat_u8((int32_t)ua - (int32_t)ub);
                        break;
                    case 0x76:
                        expected[lane] = (uint8_t)((a < b) ? a : b);
                        break;
                    case 0x77:
                        expected[lane] = (ua < ub) ? ua : ub;
                        break;
                    case 0x78:
                        expected[lane] = (uint8_t)((a > b) ? a : b);
                        break;
                    case 0x79:
                        expected[lane] = (ua > ub) ? ua : ub;
                        break;
                    default:
                        expected[lane] = (uint8_t)(((uint32_t)ua + (uint32_t)ub + 1u) >> 1);
                        break;
                }
            }
        }
        wasm_test_expect_u8x16(t, &actual, expected, "i8x16 binary");
    }

    {
        uint8_t expected[16];
        wasm_value_t swizzle_indices =
            wasm_test_v128_from_i8x16((const int8_t[]){ 15, 14, 13, 12, 11, 10, 9, 8,
                                                        7, 6, 5, 4, 3, 2, 1, 0 });
        wasm_value_t actual = wasm__simd_shuffle(&vi8_a, &vi8_b,
                                                 (const uint8_t[]){ 0, 17, 2, 19, 4, 21, 6, 23,
                                                                    8, 25, 10, 27, 12, 29, 14, 31 });
        for (index = 0; index < 8; index++) {
            expected[index * 2u] = (uint8_t)i8_a[index * 2u];
            expected[index * 2u + 1u] = (uint8_t)i8_b[index * 2u + 1u];
        }
        wasm_test_expect_u8x16(t, &actual, expected, "i8x16.shuffle");

        actual = wasm__simd_swizzle(&vi8_a, &swizzle_indices);
        for (index = 0; index < 16; index++) expected[index] = (uint8_t)i8_a[15u - index];
        wasm_test_expect_u8x16(t, &actual, expected, "i8x16.swizzle");
    }

    {
        int16_t expected_i16[8] = { -15, -11, -7, -3, 1, 5, 9, 13 };
        int32_t expected_i32[4] = { -500, -101, 101, 500 };
        wasm_value_t actual_i16 = wasm__simd_extadd_pairwise(0x7C, &vi8_a);
        wasm_value_t actual_i32 = wasm__simd_extadd_pairwise(0x7E, &vi16_a);
        wasm_test_expect_i16x8(t, &actual_i16, expected_i16, "extadd pairwise i16x8");
        wasm_test_expect_i32x4(t, &actual_i32, expected_i32, "extadd pairwise i32x4");
    }

    for (index = 0; index < (uint32_t)(sizeof(i16_binary_ops) / sizeof(i16_binary_ops[0])); index++) {
        wasm_value_t actual;
        uint32_t lane;
        if (i16_binary_ops[index] == 0x82 || i16_binary_ops[index] == 0x8E || i16_binary_ops[index] == 0x8F ||
            i16_binary_ops[index] == 0x90 || i16_binary_ops[index] == 0x91 || i16_binary_ops[index] == 0x92 ||
            i16_binary_ops[index] == 0x93 || i16_binary_ops[index] == 0x95 || i16_binary_ops[index] == 0x96 ||
            i16_binary_ops[index] == 0x97 || i16_binary_ops[index] == 0x98 || i16_binary_ops[index] == 0x99 ||
            i16_binary_ops[index] == 0x9B) {
            int16_t expected[8];
            actual = wasm__simd_i16x8_binary(i16_binary_ops[index], &vi16_a, &vi16_b);
            for (lane = 0; lane < 8; lane++) {
                int16_t a = i16_a[lane];
                int16_t b = i16_b[lane];
                uint16_t ua = (uint16_t)a;
                uint16_t ub = (uint16_t)b;
                switch (i16_binary_ops[index]) {
                    case 0x82: {
                        int32_t product = (int32_t)a * (int32_t)b;
                        expected[lane] = wasm__sat_i16((product + 0x4000) >> 15);
                        break;
                    }
                    case 0x8E:
                        expected[lane] = (int16_t)(ua + ub);
                        break;
                    case 0x8F:
                        expected[lane] = wasm__sat_i16((int32_t)a + (int32_t)b);
                        break;
                    case 0x90:
                        expected[lane] = (int16_t)wasm__sat_u16((int32_t)ua + (int32_t)ub);
                        break;
                    case 0x91:
                        expected[lane] = (int16_t)(ua - ub);
                        break;
                    case 0x92:
                        expected[lane] = wasm__sat_i16((int32_t)a - (int32_t)b);
                        break;
                    case 0x93:
                        expected[lane] = (int16_t)wasm__sat_u16((int32_t)ua - (int32_t)ub);
                        break;
                    case 0x95:
                        expected[lane] = (int16_t)((uint32_t)ua * (uint32_t)ub);
                        break;
                    case 0x96:
                        expected[lane] = (a < b) ? a : b;
                        break;
                    case 0x97:
                        expected[lane] = (int16_t)((ua < ub) ? ua : ub);
                        break;
                    case 0x98:
                        expected[lane] = (a > b) ? a : b;
                        break;
                    case 0x99:
                        expected[lane] = (int16_t)((ua > ub) ? ua : ub);
                        break;
                    default:
                        expected[lane] = (int16_t)(((uint32_t)ua + (uint32_t)ub + 1u) >> 1);
                        break;
                }
            }
            wasm_test_expect_i16x8(t, &actual, expected, "i16x8 binary");
        }
    }

    for (index = 0; index < (uint32_t)(sizeof(i32_binary_ops) / sizeof(i32_binary_ops[0])); index++) {
        int32_t expected[4];
        wasm_value_t actual = ((i32_binary_ops[index] == 0xBA) ||
                               (i32_binary_ops[index] >= 0xBC && i32_binary_ops[index] <= 0xBF))
                                  ? wasm__simd_i32x4_binary(i32_binary_ops[index], &vi16_a, &vi16_b)
                                  : wasm__simd_i32x4_binary(i32_binary_ops[index], &vi32_a, &vi32_b);
        uint32_t lane;

        for (lane = 0; lane < 4; lane++) {
            int32_t a = i32_a[lane];
            int32_t b = i32_b[lane];
            uint32_t ua = (uint32_t)a;
            uint32_t ub = (uint32_t)b;
            switch (i32_binary_ops[index]) {
                case 0xAE:
                    expected[lane] = (int32_t)(ua + ub);
                    break;
                case 0xB1:
                    expected[lane] = (int32_t)(ua - ub);
                    break;
                case 0xB5:
                    expected[lane] = (int32_t)(ua * ub);
                    break;
                case 0xB6:
                    expected[lane] = (a < b) ? a : b;
                    break;
                case 0xB7:
                    expected[lane] = (int32_t)((ua < ub) ? ua : ub);
                    break;
                case 0xB8:
                    expected[lane] = (a > b) ? a : b;
                    break;
                case 0xB9:
                    expected[lane] = (int32_t)((ua > ub) ? ua : ub);
                    break;
                case 0xBA: {
                    uint32_t base = lane * 2u;
                    expected[lane] = (int32_t)i16_a[base] * (int32_t)i16_b[base] +
                                     (int32_t)i16_a[base + 1u] * (int32_t)i16_b[base + 1u];
                    break;
                }
                case 0xBC:
                case 0xBD:
                case 0xBE:
                default: {
                    uint32_t base = (i32_binary_ops[index] == 0xBD || i32_binary_ops[index] == 0xBF) ? 4u : 0u;
                    uint32_t src = base + lane;
                    if (i32_binary_ops[index] == 0xBC || i32_binary_ops[index] == 0xBD)
                        expected[lane] = (int32_t)i16_a[src] * (int32_t)i16_b[src];
                    else
                        expected[lane] = (int32_t)((uint32_t)(uint16_t)i16_a[src] * (uint32_t)(uint16_t)i16_b[src]);
                    break;
                }
            }
        }
        wasm_test_expect_i32x4(t, &actual, expected, "i32x4 binary");
    }

    for (index = 0; index < (uint32_t)(sizeof(i64_binary_ops) / sizeof(i64_binary_ops[0])); index++) {
        int64_t expected[2];
        wasm_value_t actual = (i64_binary_ops[index] >= 0xDC)
                                  ? wasm__simd_i64x2_binary(i64_binary_ops[index], &vi32_a, &vi32_b)
                                  : wasm__simd_i64x2_binary(i64_binary_ops[index], &vi64_a, &vi64_b);
        uint32_t lane;

        for (lane = 0; lane < 2; lane++) {
            uint64_t ua = (uint64_t)i64_a[lane];
            uint64_t ub = (uint64_t)i64_b[lane];
            switch (i64_binary_ops[index]) {
                case 0xCE:
                    expected[lane] = (int64_t)(ua + ub);
                    break;
                case 0xD1:
                    expected[lane] = (int64_t)(ua - ub);
                    break;
                case 0xD5:
                    expected[lane] = (int64_t)(ua * ub);
                    break;
                case 0xDC:
                case 0xDD:
                case 0xDE:
                default: {
                    uint32_t base = (i64_binary_ops[index] == 0xDD || i64_binary_ops[index] == 0xDF) ? 2u : 0u;
                    uint32_t src = base + lane;
                    if (i64_binary_ops[index] == 0xDC || i64_binary_ops[index] == 0xDD)
                        expected[lane] = (int64_t)i32_a[src] * (int64_t)i32_b[src];
                    else
                        expected[lane] = (int64_t)((uint64_t)(uint32_t)i32_a[src] * (uint64_t)(uint32_t)i32_b[src]);
                    break;
                }
            }
        }
        wasm_test_expect_i64x2(t, &actual, expected, "i64x2 binary");
    }

    for (index = 0; index < (uint32_t)(sizeof(f32_cmp_ops) / sizeof(f32_cmp_ops[0])); index++) {
        uint8_t expected[16];
        wasm_value_t actual = wasm__simd_cmp_f32x4(f32_cmp_ops[index], &vf32_a, &vf32_b);
        uint32_t lane;
        for (lane = 0; lane < 4; lane++) {
            int cond = 0;
            switch (f32_cmp_ops[index]) {
                case 0x41:
                    cond = f32_a[lane] == f32_b[lane];
                    break;
                case 0x42:
                    cond = f32_a[lane] != f32_b[lane];
                    break;
                case 0x43:
                    cond = f32_a[lane] < f32_b[lane];
                    break;
                case 0x44:
                    cond = f32_a[lane] > f32_b[lane];
                    break;
                case 0x45:
                    cond = f32_a[lane] <= f32_b[lane];
                    break;
                default:
                    cond = f32_a[lane] >= f32_b[lane];
                    break;
            }
            memset(expected + lane * 4u, cond ? 0xFF : 0x00, 4);
        }
        wasm_test_expect_u8x16(t, &actual, expected, "f32x4 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(f64_cmp_ops) / sizeof(f64_cmp_ops[0])); index++) {
        int64_t expected[2];
        wasm_value_t actual = wasm__simd_cmp_f64x2(f64_cmp_ops[index], &vf64_a, &vf64_b);
        uint32_t lane;
        for (lane = 0; lane < 2; lane++) {
            int cond = 0;
            switch (f64_cmp_ops[index]) {
                case 0x47:
                    cond = f64_a[lane] == f64_b[lane];
                    break;
                case 0x48:
                    cond = f64_a[lane] != f64_b[lane];
                    break;
                case 0x49:
                    cond = f64_a[lane] < f64_b[lane];
                    break;
                case 0x4A:
                    cond = f64_a[lane] > f64_b[lane];
                    break;
                case 0x4B:
                    cond = f64_a[lane] <= f64_b[lane];
                    break;
                default:
                    cond = f64_a[lane] >= f64_b[lane];
                    break;
            }
            expected[lane] = cond ? -1LL : 0LL;
        }
        wasm_test_expect_i64x2(t, &actual, expected, "f64x2 cmp");
    }

    {
        float expected_f32[4] = { 0.75f, 1.5f, 2.5f, 4.0f };
        double expected_f64[2] = { 1.75, -10.0 };
        wasm_value_t actual_f32 = wasm__simd_f32x4_binary(0xE4, &vf32_a, &vf32_b);
        wasm_value_t actual_f64 = wasm__simd_f64x2_binary(0xF1, &vf64_a, &vf64_b);
        wasm_test_expect_f32x4(t, &actual_f32, expected_f32, "f32x4.add");
        wasm_test_expect_f64x2(t, &actual_f64, expected_f64, "f64x2.sub");
    }

    {
        wasm_value_t trunc_f32_input =
            wasm_test_v128_from_f32x4((const float[]){ 1.9f, -2.1f, 3.0f, -4.9f });
        wasm_value_t trunc_f64_input = wasm_test_v128_from_f64x2((const double[]){ 5.75, -6.25 });
        wasm_value_t trunc_f32 = wasm__simd_trunc_sat_f32x4(0xF8, &trunc_f32_input);
        wasm_value_t convert_f32 = wasm__simd_convert_i32x4(0xFA, &vi32_b);
        wasm_value_t trunc_f64 = wasm__simd_trunc_sat_f64x2_zero(0xFC, &trunc_f64_input);
        wasm_value_t convert_f64 = wasm__simd_convert_low_i32x4_to_f64x2(0xFE, &vi32_a);
        int32_t expected_trunc_f32[4] = { 1, -2, 3, -4 };
        float expected_convert_f32[4] = { 33333.0f, -2000.0f, 2000.0f, -33333.0f };
        int32_t expected_trunc_f64[4] = { 5, -6, 0, 0 };
        double expected_convert_f64[2] = { -100000.0, -1000.0 };

        wasm_test_expect_i32x4(t, &trunc_f32, expected_trunc_f32, "i32x4.trunc_sat_f32x4_s");
        wasm_test_expect_f32x4(t, &convert_f32, expected_convert_f32, "f32x4.convert_i32x4_s");
        wasm_test_expect_i32x4(t, &trunc_f64, expected_trunc_f64, "i32x4.trunc_sat_f64x2_s_zero");
        wasm_test_expect_f64x2(t, &convert_f64, expected_convert_f64, "f64x2.convert_low_i32x4_s");
    }
}

static const test_case simd_lanes_cases[] = {
    { "SIMD helpers: arithmetic, compare, shuffle, and conversions", test_simd_helper_ops },
};
const test_suite simd_lanes_suite = { "simd.lanes", simd_lanes_cases, COUNTOF(simd_lanes_cases) };

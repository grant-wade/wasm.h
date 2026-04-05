volatile float g_f32_big = 1.0e30f;
volatile float g_f32_neg = -42.0f;
volatile double g_f64_big = 1.0e300;
volatile double g_f64_neg_big = -1.0e300;
volatile double g_f64_neg_small = -123.0;

int wl_case(void) {
    int mask = 0;

    if (__builtin_wasm_trunc_saturate_s_i32_f32(g_f32_big) == 2147483647)
        mask |= 1;
    if (__builtin_wasm_trunc_saturate_u_i32_f32(g_f32_big) == 0xFFFFFFFFu)
        mask |= 2;
    if (__builtin_wasm_trunc_saturate_s_i32_f64(g_f64_neg_big) == (-2147483647 - 1))
        mask |= 4;
    if (__builtin_wasm_trunc_saturate_u_i32_f64(g_f64_neg_small) == 0u)
        mask |= 8;
    if (__builtin_wasm_trunc_saturate_s_i64_f32(g_f32_big) == 9223372036854775807LL)
        mask |= 16;
    if (__builtin_wasm_trunc_saturate_u_i64_f32(g_f32_neg) == 0ull)
        mask |= 32;
    if (__builtin_wasm_trunc_saturate_s_i64_f64(g_f64_neg_big) == (-9223372036854775807LL - 1LL))
        mask |= 64;
    if (__builtin_wasm_trunc_saturate_u_i64_f64(g_f64_big) == 0xFFFFFFFFFFFFFFFFull)
        mask |= 128;

    return mask;
}
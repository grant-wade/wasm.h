int wl_case(void) {
    float value = 1.0e30f;

    return __builtin_wasm_trunc_saturate_s_i32_f32(value);
}

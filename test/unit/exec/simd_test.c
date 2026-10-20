#include "../support.h"

static void emit_simd_op(wasm_builder_t* b, uint32_t subop) {
    emit(b, 0xFD);
    emit_leb128_u32(b, subop);
}

static void emit_v128_const_bytes(wasm_builder_t* b, const uint8_t bytes[16]) {
    emit_simd_op(b, 0x0C);
    emit_bytes(b, bytes, 16);
}

TEST(test_simd_execution_and_feature_gate) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_value_t result;
    static const uint8_t add_lhs[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 250, 251, 252, 253, 254, 255, 9, 10 };
    static const uint8_t add_rhs[16] = { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
    static const uint8_t memory_bytes[16] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 255, 254, 253, 252, 251, 250 };

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7B);
        emit(&sec, 0x7B);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7B);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x7B);
        emit(&sec, 0x7B);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7B);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);
        emit_export_func(&sec, "addv", 0);
        emit_export_func(&sec, "pickv", 1);
        emit_export_func(&sec, "memlane", 2);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 3);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit_simd_op(&body, 0x6E);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x1B);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit_v128_const_bytes(&body, memory_bytes);
        emit_simd_op(&body, 0x0B);
        emit_memarg(&body, 4, 0, 0, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit_v128_const_bytes(&body, memory_bytes);
        emit_simd_op(&body, 0x58);
        emit_memarg(&body, 0, 0, 0, 0);
        emit(&body, 15);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x2D);
        emit_memarg(&body, 0, 0, 0, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit_simd_op(&body, 0x00);
        emit_memarg(&body, 4, 0, 0, 0);
        emit_simd_op(&body, 0x16);
        emit(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    {
        uint8_t expected[16];
        wasm_value_t args[] = { wasm_v128(add_lhs), wasm_v128(add_rhs) };
        uint32_t lane;

        err = wasm_call(m, "addv", args, 2, &result, 1);
        WASM_CHECK_OK(t, err);
        for (lane = 0; lane < 16; lane++) expected[lane] = (uint8_t)(add_lhs[lane] + add_rhs[lane]);
        if (err == WASM_OK) wasm_test_expect_u8x16(t, &result, expected, "simd addv");
    }

    {
        wasm_value_t args[] = { wasm_v128(add_lhs), wasm_v128(add_rhs), wasm_i32(1) };
        err = wasm_call(m, "pickv", args, 3, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) wasm_test_expect_u8x16(t, &result, add_lhs, "simd select true");

        args[2] = wasm_i32(0);
        err = wasm_call(m, "pickv", args, 3, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) wasm_test_expect_u8x16(t, &result, add_rhs, "simd select false");
    }

    err = wasm_call(m, "memlane", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, (int32_t)memory_bytes[0] + (int32_t)memory_bytes[15]);

    wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_init_features(&rt, WASM_FEATURES_ALL & ~WASM_FEATURE_SIMD);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected SIMD-disabled runtime to reject v128 module");
    CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", "expected malformed error for disabled SIMD feature");
    CHECK_MSG(t, strstr(rt.error_msg, "SIMD") != NULL, "%s", "expected SIMD feature error message");
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_simd_cases[] = {
    { "SIMD: execution path, memory ops, select, and feature gate", test_simd_execution_and_feature_gate },
};
const test_suite exec_simd_suite = { "exec.simd", exec_simd_cases, COUNTOF(exec_simd_cases) };

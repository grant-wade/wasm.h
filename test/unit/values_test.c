#include "support.h"

#ifdef WASM_TEST_MODULAR
#include "../../src/values.h"
#endif

static uint32_t wasm_test_gc_i31_get_u(uintptr_t bits) {
    return (uint32_t)(bits >> 1) & 0x7FFFFFFFu;
}

static void emit_f32(wasm_builder_t* b, float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    emit(b, (uint8_t)(bits & 0xFFu));
    emit(b, (uint8_t)((bits >> 8) & 0xFFu));
    emit(b, (uint8_t)((bits >> 16) & 0xFFu));
    emit(b, (uint8_t)((bits >> 24) & 0xFFu));
}

static void emit_f64(wasm_builder_t* b, double value) {
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    emit(b, (uint8_t)(bits & 0xFFu));
    emit(b, (uint8_t)((bits >> 8) & 0xFFu));
    emit(b, (uint8_t)((bits >> 16) & 0xFFu));
    emit(b, (uint8_t)((bits >> 24) & 0xFFu));
    emit(b, (uint8_t)((bits >> 32) & 0xFFu));
    emit(b, (uint8_t)((bits >> 40) & 0xFFu));
    emit(b, (uint8_t)((bits >> 48) & 0xFFu));
    emit(b, (uint8_t)((bits >> 56) & 0xFFu));
}

TEST(test_trunc_sat_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint32_t index;
    const char* names[8] = { "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7" };
    const int is_i64[8] = { 0, 0, 0, 0, 1, 1, 1, 1 };
    const int32_t expected_i32[8] = {
        INT32_MAX,
        -1,
        INT32_MIN,
        0,
        0,
        0,
        0,
        0
    };
    const int64_t expected_i64[8] = {
        0,
        0,
        0,
        0,
        INT64_MAX,
        0,
        INT64_MIN,
        -1
    };

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        for (index = 0; index < 8; index++) {
            emit_leb128_u32(&sec, 2);
            emit(&sec, names[index][0]);
            emit(&sec, names[index][1]);
            emit(&sec, 0x00);
            emit_leb128_u32(&sec, index);
        }
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 8);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x43);
        emit_f32(&body, 1.0e30f);
        emit(&body, 0xFC);
        emit(&body, 0x00);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x43);
        emit_f32(&body, 1.0e30f);
        emit(&body, 0xFC);
        emit(&body, 0x01);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x44);
        emit_f64(&body, -1.0e300);
        emit(&body, 0xFC);
        emit(&body, 0x02);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x44);
        emit_f64(&body, -123.0);
        emit(&body, 0xFC);
        emit(&body, 0x03);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x43);
        emit_f32(&body, 1.0e30f);
        emit(&body, 0xFC);
        emit(&body, 0x04);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x43);
        emit_f32(&body, -42.0f);
        emit(&body, 0xFC);
        emit(&body, 0x05);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x44);
        emit_f64(&body, -1.0e300);
        emit(&body, 0xFC);
        emit(&body, 0x06);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x44);
        emit_f64(&body, 1.0e300);
        emit(&body, 0xFC);
        emit(&body, 0x07);
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

    for (index = 0; index < 8; index++) {
        err = wasm_call(m, names[index], NULL, 0, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err != WASM_OK) {
            wasm_module_free(m);
            wasm_destroy(&rt);
            return;
        }
        if (!is_i64[index] && result.of.i32 != expected_i32[index]) {
            WASM_CHECK_I32(t, result.of.i32, expected_i32[index]);
            wasm_module_free(m);
            wasm_destroy(&rt);
            return;
        }
        if (is_i64[index] && result.of.i64 != expected_i64[index]) {
            WASM_CHECK_I64(t, result.of.i64, expected_i64[index]);
            wasm_module_free(m);
            wasm_destroy(&rt);
            return;
        }
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_float_min_max_semantics) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args_f32[2];
    wasm_value_t args_f64[2];
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7D);
        emit(&sec, 0x7D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7D);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7C);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        emit_export_func(&sec, "f32min", 0);
        emit_export_func(&sec, "f32max", 1);
        emit_export_func(&sec, "f64min", 2);
        emit_export_func(&sec, "f64max", 3);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        static const uint8_t ops[] = { 0x96, 0x97, 0xA4, 0xA5 };
        uint32_t i;
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);
        for (i = 0; i < 4; i++) {
            wasm_builder_t body = { 0 };
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 1);
            emit(&body, ops[i]);
            emit(&body, 0x0B);
            emit_leb128_u32(&sec, body.len);
            emit_bytes(&sec, body.buf, body.len);
        }

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    args_f32[0] = wasm_f32(-0.0f);
    args_f32[1] = wasm_f32(0.0f);
    err = wasm_call(m, "f32min", args_f32, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, result.of.f32 == 0.0f && signbit(result.of.f32), "%s", "expected f32.min(-0,+0) to return -0");
    }

    err = wasm_call(m, "f32max", args_f32, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, result.of.f32 == 0.0f && !signbit(result.of.f32), "%s", "expected f32.max(-0,+0) to return +0");
    }

    args_f32[0] = wasm_f32(NAN);
    args_f32[1] = wasm_f32(1.0f);
    err = wasm_call(m, "f32min", args_f32, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, isnan(result.of.f32), "%s", "expected f32.min(NaN, 1) to return NaN");
    }

    args_f64[0] = wasm_f64(-0.0);
    args_f64[1] = wasm_f64(0.0);
    err = wasm_call(m, "f64min", args_f64, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, result.of.f64 == 0.0 && signbit(result.of.f64), "%s", "expected f64.min(-0,+0) to return -0");
    }

    err = wasm_call(m, "f64max", args_f64, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, result.of.f64 == 0.0 && !signbit(result.of.f64), "%s", "expected f64.max(-0,+0) to return +0");
    }

    args_f64[0] = wasm_f64(1.0);
    args_f64[1] = wasm_f64(NAN);
    err = wasm_call(m, "f64max", args_f64, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, isnan(result.of.f64), "%s", "expected f64.max(1, NaN) to return NaN");
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_trapping_float_to_int_conversions) {
    static const char* export_names[] = {
        "i32_trunc_f32_s",
        "i32_trunc_f32_u",
        "i32_trunc_f64_s",
        "i32_trunc_f64_u",
        "i64_trunc_f32_s",
        "i64_trunc_f32_u",
        "i64_trunc_f64_s",
        "i64_trunc_f64_u"
    };
    static const uint8_t type_indices[] = { 0, 0, 1, 1, 2, 2, 3, 3 };
    static const uint8_t ops[] = { 0xA8, 0xA9, 0xAA, 0xAB, 0xAE, 0xAF, 0xB0, 0xB1 };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint32_t i;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        for (i = 0; i < 8; i++) emit_leb128_u32(&sec, type_indices[i]);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        for (i = 0; i < 8; i++) emit_export_func(&sec, export_names[i], i);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        for (i = 0; i < 8; i++) {
            wasm_builder_t body = { 0 };
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 0);
            emit(&body, ops[i]);
            emit(&body, 0x0B);
            emit_leb128_u32(&sec, body.len);
            emit_bytes(&sec, body.buf, body.len);
        }
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
        wasm_value_t args[] = { wasm_f32(123.75f) };
        err = wasm_call(m, "i32_trunc_f32_s", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 123);
    }

    {
        wasm_value_t args[] = { wasm_f64(42.9) };
        err = wasm_call(m, "i64_trunc_f64_u", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, 42);
    }

    {
        wasm_value_t args[] = { wasm_f32(NAN) };
        err = wasm_call(m, "i32_trunc_f32_s", args, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f32(-1.0f) };
        err = wasm_call(m, "i32_trunc_f32_u", args, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f64(2147483648.0) };
        err = wasm_call(m, "i32_trunc_f64_s", args, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f64(4294967296.0) };
        err = wasm_call(m, "i32_trunc_f64_u", args, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f32(NAN) };
        err = wasm_call(m, "i64_trunc_f32_s", args, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f32(-1.0f) };
        err = wasm_call(m, "i64_trunc_f32_u", args, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f64(9223372036854775808.0) };
        err = wasm_call(m, "i64_trunc_f64_s", args, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f64(18446744073709551616.0) };
        err = wasm_call(m, "i64_trunc_f64_u", args, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_trunc_allows_fractional_lower_edges) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);
        emit_export_func(&sec, "i32s", 0);
        emit_export_func(&sec, "i32u", 1);
        emit_export_func(&sec, "i64u", 2);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 3);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xAA);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xAB);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xB1);
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
        wasm_value_t args[] = { wasm_f64(-2147483648.75) };
        err = wasm_call(m, "i32s", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, INT32_MIN);
    }

    {
        wasm_value_t args[] = { wasm_f64(-0.75) };
        err = wasm_call(m, "i32u", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    }

    {
        wasm_value_t args[] = { wasm_f64(-0.75) };
        err = wasm_call(m, "i64u", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, 0);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_i31ref_uses_tagged_non_null_storage) {
    wasm_value_t null_i31 = wasm_ref_null(WASM_TYPE_I31REF);
    wasm_value_t minus_one = wasm_i31ref(-1);
    wasm_value_t positive = wasm_i31ref(123456789);

    CHECK_MSG(t, null_i31.type == WASM_TYPE_I31REF, "%s", "expected null i31ref to preserve its value type");
    CHECK_MSG(t, wasm__is_null_ref(&null_i31), "%s", "expected ref.null i31 to stay null");

    CHECK_MSG(t, !wasm__is_null_ref(&minus_one), "%s", "expected tagged i31ref payloads to be non-null");
    CHECK_MSG(t, wasm_test_value_is_i31ref(&minus_one), "%s", "expected wasm_i31ref to set the i31 tag bit");
    CHECK_MSG(t, minus_one.of.gc_ref == wasm__gc_i31_encode(-1), "%s", "expected wasm_i31ref to use the internal tagged encoding");
    WASM_CHECK_I32(t, wasm_test_gc_i31_get_s(minus_one.of.gc_ref), -1);
    CHECK_MSG(t, wasm_test_gc_i31_get_u(minus_one.of.gc_ref) == 0x7FFFFFFFu,
              "expected unsigned i31 decode to keep the low 31 bits, got %u",
              (unsigned)wasm_test_gc_i31_get_u(minus_one.of.gc_ref));
    WASM_CHECK_I32(t, wasm_test_gc_i31_get_s(positive.of.gc_ref), 123456789);
}

TEST(test_gc_accessors_handle_packed_fields_and_array_bounds) {
    wasm_runtime_t rt;
    wasm_module_t mod;
    wasm_comptype_t types[2];
    wasm_fieldtype_t struct_fields[3];
    wasm_gc_struct_t* struct_obj;
    wasm_gc_array_t* array_obj;
    wasm_value_t value = { 0 };
    wasm__gc_root_t root;
    wasm_value_t held;
    wasm_value_t* held_values = &held;
    uint32_t held_count = 1;

    memset(&mod, 0, sizeof(mod));
    memset(types, 0, sizeof(types));
    memset(struct_fields, 0, sizeof(struct_fields));

    wasm_init(&rt);

    struct_fields[0].storage.kind = WASM_STORAGE_PACKED;
    struct_fields[0].storage.of.packed_type = WASM_PACKED_I8;
    struct_fields[0].is_mutable = 1;

    struct_fields[1].storage.kind = WASM_STORAGE_PACKED;
    struct_fields[1].storage.of.packed_type = WASM_PACKED_I16;
    struct_fields[1].is_mutable = 1;

    struct_fields[2].storage.kind = WASM_STORAGE_VALTYPE;
    struct_fields[2].storage.of.valtype = WASM_TYPE_I32;
    struct_fields[2].is_mutable = 1;

    types[0].kind = WASM_COMP_STRUCT;

    types[0].descriptor = UINT32_MAX;
    types[0].of.struct_.num_fields = 3;
    types[0].of.struct_.fields = struct_fields;

    types[1].kind = WASM_COMP_ARRAY;
    types[1].of.array.field.storage.kind = WASM_STORAGE_PACKED;
    types[1].of.array.field.storage.of.packed_type = WASM_PACKED_I8;
    types[1].of.array.field.is_mutable = 1;

    mod.rt = &rt;
    mod.types = types;
    mod.num_types = 2;

    struct_obj = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    held = wasm__gc_type_ref_value(&mod, 0, struct_obj ? &struct_obj->header : NULL);
    wasm__gc_push_root(&rt, &root, &held_values, &held_count);
    array_obj = wasm__gc_alloc_array_object(&rt, &mod, 1, &types[1].of.array, 2);
    wasm__gc_pop_root(&rt, &root);
    struct_obj = (wasm_gc_struct_t*)held.of.gc_obj;

    CHECK_MSG(t, struct_obj != NULL, "%s", "expected struct allocation for accessor test to succeed");
    CHECK_MSG(t, array_obj != NULL, "%s", "expected array allocation for accessor test to succeed");
    if (!struct_obj || !array_obj) {
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, wasm__gc_struct_set_field(struct_obj, &types[0].of.struct_, 0, wasm_i32(0xFF)) == WASM_OK,
              "%s",
              "expected packed i8 struct store to succeed");
    CHECK_MSG(t, wasm__gc_struct_set_field(struct_obj, &types[0].of.struct_, 1, wasm_i32(0xFF80)) == WASM_OK,
              "%s",
              "expected packed i16 struct store to succeed");
    CHECK_MSG(t, wasm__gc_struct_set_field(struct_obj, &types[0].of.struct_, 2, wasm_i32(77)) == WASM_OK,
              "%s",
              "expected unpacked struct store to succeed");

    CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 0, 1, &value) == WASM_OK,
              "%s",
              "expected packed i8 signed load to succeed");
    WASM_CHECK_I32(t, value.of.i32, -1);
    CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 0, 0, &value) == WASM_OK,
              "%s",
              "expected packed i8 unsigned load to succeed");
    WASM_CHECK_I32(t, value.of.i32, 255);

    CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 1, 1, &value) == WASM_OK,
              "%s",
              "expected packed i16 signed load to succeed");
    WASM_CHECK_I32(t, value.of.i32, -128);
    CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 1, 0, &value) == WASM_OK,
              "%s",
              "expected packed i16 unsigned load to succeed");
    WASM_CHECK_I32(t, value.of.i32, 65408);

    CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 2, 0, &value) == WASM_OK,
              "%s",
              "expected unpacked struct load to succeed");
    CHECK_MSG(t, value.type == WASM_TYPE_I32 && value.of.i32 == 77,
              "%s",
              "expected unpacked struct accessor to preserve the stored value");

    CHECK_MSG(t, wasm__gc_struct_get_field(NULL, &types[0].of.struct_, 0, 0, &value) == WASM_ERR_TRAP,
              "%s",
              "expected null struct access to trap");
    CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 3, 0, &value) == WASM_ERR_MALFORMED,
              "%s",
              "expected invalid struct field access to be rejected");

    CHECK_MSG(t, wasm__gc_array_set(array_obj, &types[1].of.array, 0, wasm_i32(0x7F)) == WASM_OK,
              "%s",
              "expected first packed array store to succeed");
    CHECK_MSG(t, wasm__gc_array_set(array_obj, &types[1].of.array, 1, wasm_i32(0x80)) == WASM_OK,
              "%s",
              "expected second packed array store to succeed");
    CHECK_MSG(t, wasm__gc_array_len(array_obj) == 2,
              "expected array accessor length 2, got %u",
              (unsigned)wasm__gc_array_len(array_obj));

    CHECK_MSG(t, wasm__gc_array_get(array_obj, &types[1].of.array, 0, 1, &value) == WASM_OK,
              "%s",
              "expected first packed array load to succeed");
    WASM_CHECK_I32(t, value.of.i32, 127);
    CHECK_MSG(t, wasm__gc_array_get(array_obj, &types[1].of.array, 1, 1, &value) == WASM_OK,
              "%s",
              "expected signed packed array load to succeed");
    WASM_CHECK_I32(t, value.of.i32, -128);
    CHECK_MSG(t, wasm__gc_array_get(array_obj, &types[1].of.array, 1, 0, &value) == WASM_OK,
              "%s",
              "expected unsigned packed array load to succeed");
    WASM_CHECK_I32(t, value.of.i32, 128);

    CHECK_MSG(t, wasm__gc_array_get(array_obj, &types[1].of.array, 2, 0, &value) == WASM_ERR_OUT_OF_BOUNDS,
              "%s",
              "expected out-of-bounds array access to trap with bounds error");
    CHECK_MSG(t, wasm__gc_array_set(NULL, &types[1].of.array, 0, wasm_i32(0)) == WASM_ERR_TRAP,
              "%s",
              "expected null array store to trap");

    wasm_destroy(&rt);
}

static const test_case values_cases[] = {
    { "float min/max: NaN propagation and signed zero semantics", test_float_min_max_semantics },
    { "float trunc: trapping conversions reject NaN and overflow", test_trapping_float_to_int_conversions },
    { "float trunc: fractional lower-edge values still convert", test_trunc_allows_fractional_lower_edges },
    { "trunc-sat ops: all 0xFC 0x00-0x07 cases", test_trunc_sat_ops },
    { "wasmgc: i31ref values use tagged non-null storage", test_gc_i31ref_uses_tagged_non_null_storage },
    { "wasmgc: GC accessors handle packed fields and array bounds", test_gc_accessors_handle_packed_fields_and_array_bounds },
};
const test_suite values_suite = { "values", values_cases, COUNTOF(values_cases) };

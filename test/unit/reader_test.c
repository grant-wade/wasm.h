#include "support.h"

static void emit_malformed_uleb128_u32(wasm_builder_t* b) {
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x00);
}

static void emit_malformed_sleb128_i32(wasm_builder_t* b) {
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x10);
}

static void emit_malformed_sleb128_i64(wasm_builder_t* b) {
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x01);
}

TEST(test_validation_rejects_long_u32_leb128) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);
    emit(&mod, 0x01);
    emit_leb128_u32(&mod, 6);
    emit_malformed_uleb128_u32(&mod);

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected malformed long u32 LEB to fail load");
    CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_validation_rejects_long_i32_leb128) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "bad_i32", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_malformed_sleb128_i32(&body);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected malformed long i32 LEB to fail load");
    CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_validation_rejects_long_i64_leb128) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "bad_i64", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x42);
        emit_malformed_sleb128_i64(&body);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected malformed long i64 LEB to fail load");
    CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

static const test_case reader_cases[] = {
    { "validation: long u32 LEBs are rejected during load", test_validation_rejects_long_u32_leb128 },
    { "validation: long i32 LEBs are rejected during load", test_validation_rejects_long_i32_leb128 },
    { "validation: long i64 LEBs are rejected during load", test_validation_rejects_long_i64_leb128 },
};
const test_suite reader_suite = { "reader", reader_cases, COUNTOF(reader_cases) };

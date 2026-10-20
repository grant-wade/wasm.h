#include "support.h"

static int host_fmt_add_called = 0;

static wasm_error_t host_fmt_add(wasm_module_t* mod,
                                 const wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results,
                                 void* userdata) {
    int32_t bias = userdata ? *(const int32_t*)userdata : 0;

    (void)mod;
    host_fmt_add_called = 1;
    if (num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    results[0] = wasm_i32(args[0].of.i32 + args[1].of.i32 + bias);
    return WASM_OK;
}

static wasm_error_t host_module_userdata_add(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    int32_t bias = 0;

    (void)userdata;

    if (!mod || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (wasm_module_userdata(mod)) bias = *(const int32_t*)wasm_module_userdata(mod);

    results[0] = wasm_i32(args[0].of.i32 + args[1].of.i32 + bias);
    return WASM_OK;
}

static wasm_error_t host_peek_i32_from_memory(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata) {
    int32_t value = 0;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 1 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0) return WASM_ERR_TYPE_MISMATCH;

    err = wasm_memory_read(mod, 0, (uint32_t)args[0].of.i32, &value, sizeof(value));
    if (err != WASM_OK) return err;

    results[0] = wasm_i32(value);
    return WASM_OK;
}

TEST(test_format_call_api) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    int32_t sum_i32 = 0;
    float sum_f32 = 0.0f;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7D);
        emit(&sec, 0x7D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7D);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);
        emit_export_func(&sec, "add", 0);
        emit_export_func(&sec, "fadd", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        static const uint8_t ops[] = { 0x6A, 0x92 };
        uint32_t i;
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);
        for (i = 0; i < 2; i++) {
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

    err = wasm_call_fmt(m, "add", "ii(i)", (int32_t)3, (int32_t)7, &sum_i32);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, sum_i32, 10);
    }

    err = wasm_call_fmt(m, "fadd", "ff(f)", 1.5, 2.5, &sum_f32);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, sum_f32 == 4.0f, "%s", "expected f32 format-call result 4.0");
    }

    err = wasm_call_fmt(m, "add", "ii(v)", (int32_t)1, (int32_t)2);
    CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected signature mismatch rejection");

    err = wasm_call_fmt(m, "add", "ii(i", (int32_t)1, (int32_t)2, &sum_i32);
    CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected malformed format string rejection");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_bind_host_func_api) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    int32_t bias = 5;
    int32_t result = 0;

    host_fmt_add_called = 0;
    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, 'd');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    err = wasm_bind_host_func(&rt, "env", "bad", "i(vv)", host_fmt_add, &bias);
    CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected malformed format rejection during bind");

    err = wasm_bind_host_func(&rt, "env", "add", "ii(i)", host_fmt_add, &bias);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call_fmt(m, "run", "(i)", &result);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, host_fmt_add_called, "%s", "expected bound host function to run");
        WASM_CHECK_I32(t, result, 18);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_bind_host_func_uses_module_userdata) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    int32_t bias = 6;
    int32_t result = 0;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, 'd');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    err = wasm_bind_host_func(&rt, "env", "add", "ii(i)", host_module_userdata_add, NULL);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call_fmt(m, "run", "(i)", &result);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result, 13);
    }

    wasm_module_set_userdata(m, &bias);
    err = wasm_call_fmt(m, "run", "(i)", &result);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result, 19);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_host_callback_module_reads_memory) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_import_t imp;
    wasm_error_t err;
    int32_t result = 0;
    static const uint8_t data_bytes[4] = { 0x78, 0x56, 0x34, 0x12 };

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'p');
        emit(&sec, 'e');
        emit(&sec, 'e');
        emit(&sec, 'k');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
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
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "peek", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 8);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "peek";
    imp.func = host_peek_i32_from_memory;
    err = wasm_register_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call_fmt(m, "peek", "i(i)", (int32_t)8, &result);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result, 0x12345678);
    }

    err = wasm_call_fmt(m, "peek", "i(i)", (int32_t)(WASM_PAGE_SIZE - 2u), &result);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS,
              "%s", "expected host callback memory read to preserve out-of-bounds errors");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_large_param_call) {
    wasm_builder_t mod = { 0 };
    wasm_value_t args[17];
    wasm_value_t result;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    uint32_t index;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 17);
        for (index = 0; index < 17; index++) emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'l');
        emit(&sec, 'l');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 16);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        for (index = 0; index < 17; index++) {
            emit(&body, 0x20);
            emit_leb128_u32(&body, index);
        }
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    for (index = 0; index < 17; index++) args[index] = wasm_i32((int32_t)index + 1);

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "call", args, 17, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 17);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_multi_result_call) {
    wasm_builder_t mod = { 0 };
    wasm_value_t results[5];
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 5);
        emit(&sec, 'm');
        emit(&sec, 'u');
        emit(&sec, 'l');
        emit(&sec, 't');
        emit(&sec, 'i');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    memset(results, 0, sizeof(results));
    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "multi", NULL, 0, results, 5);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(
            t,
            results[0].of.i32 == 1 && results[1].of.i32 == 2 && results[2].of.i32 == 3 &&
                results[3].of.i32 == 4 && results[4].of.i32 == 5,
            "%s",
            "unexpected multi-result values");
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_host_trap(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    return WASM_ERR_TRAP;
}

static wasm_error_t wasm_test_host_refuse(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_runtime_set_error(mod->rt, WASM_ERR_TRAP, "host refused");
    return WASM_ERR_TRAP;
}

static void wasm_test_expect_call_error(test_ctx* t, const wasm_runtime_t* rt, const char* what, wasm_error_t err,
                                        wasm_error_t expected, const char* message) {
    CHECK_MSG(t, err == expected, "%s: expected %s, got %s", what, wasm_error_string(expected),
              wasm_error_string(err));
    CHECK_MSG(t, wasm_runtime_last_error(rt) == err, "%s: last error %s does not match the returned code", what,
              wasm_error_string(wasm_runtime_last_error(rt)));
    CHECK_MSG(t, strcmp(wasm_runtime_error_message(rt), message) == 0, "%s: expected message '%s', got '%s'", what,
              message, wasm_runtime_error_message(rt));
}

TEST(test_failed_calls_record_trap_codes_and_messages) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0d, 0x03, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7f,
        0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x19, 0x02, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x66, 0x61, 0x69, 0x6c, 0x00,
        0x00, 0x03, 0x65, 0x6e, 0x76, 0x06, 0x72, 0x65, 0x66, 0x75, 0x73, 0x65, 0x00, 0x00, 0x03, 0x07, 0x06, 0x00,
        0x01, 0x02, 0x00, 0x00, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x0d, 0x03,
        0x01, 0x00, 0x00, 0x07, 0x3f, 0x08, 0x01, 0x74, 0x01, 0x00, 0x07, 0x75, 0x6e, 0x72, 0x65, 0x61, 0x63, 0x68,
        0x00, 0x02, 0x03, 0x6f, 0x6f, 0x62, 0x00, 0x03, 0x03, 0x64, 0x69, 0x76, 0x00, 0x04, 0x04, 0x73, 0x70, 0x69,
        0x6e, 0x00, 0x05, 0x05, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x00, 0x06, 0x0b, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x72,
        0x65, 0x66, 0x75, 0x73, 0x65, 0x00, 0x07, 0x04, 0x66, 0x61, 0x69, 0x6c, 0x00, 0x00, 0x09, 0x07, 0x01, 0x00,
        0x41, 0x00, 0x0b, 0x01, 0x02, 0x0a, 0x29, 0x06, 0x03, 0x00, 0x00, 0x0b, 0x09, 0x00, 0x41, 0xf0, 0xa2, 0x04,
        0x28, 0x02, 0x00, 0x0b, 0x07, 0x00, 0x41, 0x01, 0x20, 0x00, 0x6d, 0x0b, 0x07, 0x00, 0x03, 0x40, 0x0c, 0x00,
        0x0b, 0x0b, 0x04, 0x00, 0x08, 0x00, 0x0b, 0x04, 0x00, 0x10, 0x01, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    wasm_value_t ref;
    int32_t out = 0;

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "fail", "()", wasm_test_host_trap, NULL));
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "refuse", "()", wasm_test_host_refuse, NULL));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    wasm_test_expect_call_error(t, &rt, "unreachable", wasm_call(m, "unreach", NULL, 0, NULL, 0),
                                WASM_ERR_UNREACHABLE, "unreachable executed");
    wasm_test_expect_call_error(t, &rt, "out-of-bounds load", wasm_call(m, "oob", NULL, 0, &result, 1),
                                WASM_ERR_OUT_OF_BOUNDS, "out of bounds memory access");
    arg = wasm_i32(0);
    wasm_test_expect_call_error(t, &rt, "divide by zero", wasm_call(m, "div", &arg, 1, &result, 1),
                                WASM_ERR_DIV_BY_ZERO, "integer divide by zero");
    arg = wasm_i32(1);
    WASM_CHECK_OK(t, wasm_call(m, "div", &arg, 1, &result, 1));
    CHECK(t, wasm_runtime_last_error(&rt) == WASM_OK);
    CHECK(t, strcmp(wasm_runtime_error_message(&rt), "ok") == 0);

    wasm_runtime_set_fuel(&rt, 5);
    wasm_test_expect_call_error(t, &rt, "fuel", wasm_call(m, "spin", NULL, 0, NULL, 0), WASM_ERR_OUT_OF_FUEL,
                                "out of fuel");
    wasm_runtime_cancel(&rt);
    wasm_runtime_disable_fuel(&rt);

    wasm_test_expect_call_error(t, &rt, "uncaught exception", wasm_call(m, "throw", NULL, 0, NULL, 0), WASM_ERR_TRAP,
                                "uncaught exception tag 0");

    WASM_CHECK_OK(t, wasm_table_get(m, 0, 0, &ref));
    wasm_test_expect_call_error(t, &rt, "wasm_call_ref", wasm_call_ref(&rt, ref, NULL, 0, NULL, 0),
                                WASM_ERR_UNREACHABLE, "unreachable executed");
    wasm_test_expect_call_error(t, &rt, "wasm_call_index", wasm_call_index(m, 2, NULL, 0, NULL, 0),
                                WASM_ERR_UNREACHABLE, "unreachable executed");
    wasm_test_expect_call_error(t, &rt, "wasm_call_fmt", wasm_call_fmt(m, "div", "i(i)", 0, &out),
                                WASM_ERR_DIV_BY_ZERO, "integer divide by zero");

    wasm_test_expect_call_error(t, &rt, "host message", wasm_call(m, "call_refuse", NULL, 0, NULL, 0), WASM_ERR_TRAP,
                                "host refused");
    wasm_test_expect_call_error(t, &rt, "imported host function", wasm_call(m, "fail", NULL, 0, NULL, 0),
                                WASM_ERR_TRAP, "trap");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* State shared by the host imports of the resume test module. */
typedef struct wasm_test_resume_host_t {
    wasm_module_t* mod;
    const char* reenter_export; /* export "reenter" calls with its argument */
    uint64_t reenter_fuel;      /* fuel set before the inner call; 0 leaves fuel unchanged */
    uint32_t yields;
    int32_t last_arg;
} wasm_test_resume_host_t;

static wasm_error_t wasm_test_resume_yield(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_test_resume_host_t* host = (wasm_test_resume_host_t*)userdata;

    (void)mod;
    (void)num_args;
    (void)results;
    (void)num_results;
    host->yields++;
    host->last_arg = args[0].of.i32;
    return WASM_YIELD;
}

static wasm_error_t wasm_test_resume_reenter(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_test_resume_host_t* host = (wasm_test_resume_host_t*)userdata;
    wasm_value_t inner = wasm_i64(0);
    wasm_error_t err;

    (void)num_args;
    (void)num_results;
    if (host->reenter_fuel) wasm_runtime_set_fuel(wasm_module_runtime(mod), host->reenter_fuel);
    err = wasm_call(host->mod, host->reenter_export, args, 1, &inner, 1);
    if (err != WASM_OK) return err;
    results[0] = wasm_i32(inner.type == WASM_TYPE_I64 ? (int32_t)inner.of.i64 : inner.of.i32);
    return WASM_OK;
}

static wasm_error_t wasm_test_resume_interrupt(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_runtime_interrupt(wasm_module_runtime(mod));
    return WASM_OK;
}

static wasm_module_t* wasm_test_load_resume_module(test_ctx* t, wasm_runtime_t* rt, wasm_test_resume_host_t* host) {
    /* Function indices: 0 yield, 1 reenter, 2 interrupt (imports), 3 sum, 4 count, 5 rec,
     * 6 catch_after_yield, 7 tail_target, 8 tail, 9 tail_import, 10 outer, 11 gc_keep,
     * 12 self_interrupt, 13 spin_until_flag, 14 fill, 15 grow, 16 catch_loop, 17 leaf,
     * 18 call_after_interrupt, 19 nested_tail_import. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x20, 0x07, 0x5f, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f,
        0x01, 0x7f, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f, 0x01, 0x7e, 0x60, 0x02, 0x7f, 0x7f,
        0x01, 0x7f, 0x60, 0x00, 0x01, 0x7f, 0x02, 0x2e, 0x03, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x05, 0x79, 0x69, 0x65,
        0x6c, 0x64, 0x00, 0x01, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x07, 0x72, 0x65, 0x65, 0x6e, 0x74, 0x65, 0x72, 0x00,
        0x01, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x09, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x00, 0x02,
        0x03, 0x12, 0x11, 0x04, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x06, 0x01, 0x06, 0x01, 0x06,
        0x06, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x0d, 0x03, 0x01, 0x00, 0x03, 0x07, 0xc0, 0x01, 0x10, 0x03, 0x6d,
        0x65, 0x6d, 0x02, 0x00, 0x03, 0x73, 0x75, 0x6d, 0x00, 0x03, 0x05, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x00, 0x04,
        0x03, 0x72, 0x65, 0x63, 0x00, 0x05, 0x11, 0x63, 0x61, 0x74, 0x63, 0x68, 0x5f, 0x61, 0x66, 0x74, 0x65, 0x72,
        0x5f, 0x79, 0x69, 0x65, 0x6c, 0x64, 0x00, 0x06, 0x04, 0x74, 0x61, 0x69, 0x6c, 0x00, 0x08, 0x0b, 0x74, 0x61,
        0x69, 0x6c, 0x5f, 0x69, 0x6d, 0x70, 0x6f, 0x72, 0x74, 0x00, 0x09, 0x05, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x00,
        0x0a, 0x07, 0x67, 0x63, 0x5f, 0x6b, 0x65, 0x65, 0x70, 0x00, 0x0b, 0x0e, 0x73, 0x65, 0x6c, 0x66, 0x5f, 0x69,
        0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x00, 0x0c, 0x0f, 0x73, 0x70, 0x69, 0x6e, 0x5f, 0x75, 0x6e,
        0x74, 0x69, 0x6c, 0x5f, 0x66, 0x6c, 0x61, 0x67, 0x00, 0x0d, 0x04, 0x66, 0x69, 0x6c, 0x6c, 0x00, 0x0e, 0x04,
        0x67, 0x72, 0x6f, 0x77, 0x00, 0x0f, 0x0a, 0x63, 0x61, 0x74, 0x63, 0x68, 0x5f, 0x6c, 0x6f, 0x6f, 0x70, 0x00,
        0x10, 0x14, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x61, 0x66, 0x74, 0x65, 0x72, 0x5f, 0x69, 0x6e, 0x74, 0x65, 0x72,
        0x72, 0x75, 0x70, 0x74, 0x00, 0x12, 0x12, 0x6e, 0x65, 0x73, 0x74, 0x65, 0x64, 0x5f, 0x74, 0x61, 0x69, 0x6c,
        0x5f, 0x69, 0x6d, 0x70, 0x6f, 0x72, 0x74, 0x00, 0x13, 0x0a, 0xd4, 0x02, 0x11, 0x26, 0x02, 0x01, 0x7f, 0x01,
        0x7e, 0x02, 0x40, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00, 0x4f, 0x0d, 0x01, 0x20, 0x02, 0x20, 0x01, 0xad, 0x7c,
        0x21, 0x02, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x0c, 0x00, 0x0b, 0x0b, 0x20, 0x02, 0x0b, 0x17, 0x00,
        0x20, 0x00, 0x45, 0x04, 0x7f, 0x20, 0x01, 0x05, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x20, 0x01, 0x20, 0x00, 0x6a,
        0x12, 0x04, 0x0b, 0x0b, 0x18, 0x00, 0x20, 0x00, 0x45, 0x04, 0x7f, 0x41, 0xe4, 0x00, 0x10, 0x00, 0x05, 0x20,
        0x00, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x10, 0x05, 0x6a, 0x0b, 0x0b, 0x14, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x09, 0x00, 0x20, 0x00,
        0x10, 0x00, 0x41, 0x02, 0x6c, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x12, 0x07, 0x0b, 0x06, 0x00,
        0x20, 0x00, 0x12, 0x00, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x10, 0x01, 0x41, 0x01, 0x6a, 0x0b, 0x27, 0x01, 0x01,
        0x63, 0x00, 0x41, 0x01, 0xfb, 0x00, 0x00, 0x1a, 0x20, 0x00, 0xfb, 0x00, 0x00, 0x21, 0x01, 0x41, 0x05, 0xfb,
        0x00, 0x00, 0x41, 0x00, 0x10, 0x00, 0x1a, 0xfb, 0x02, 0x00, 0x00, 0x20, 0x01, 0xfb, 0x02, 0x00, 0x00, 0x6a,
        0x0b, 0x21, 0x01, 0x01, 0x7f, 0x03, 0x40, 0x20, 0x01, 0x41, 0x03, 0x46, 0x04, 0x40, 0x10, 0x02, 0x0b, 0x20,
        0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x20, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0x0b, 0x18,
        0x01, 0x01, 0x7f, 0x03, 0x40, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x21, 0x00, 0x41, 0x00, 0x28, 0x02, 0x00, 0x45,
        0x0d, 0x00, 0x0b, 0x20, 0x00, 0x0b, 0x13, 0x00, 0x41, 0x10, 0x41, 0x07, 0x20, 0x00, 0xfc, 0x0b, 0x00, 0x41,
        0x0f, 0x20, 0x00, 0x6a, 0x2d, 0x00, 0x00, 0x0b, 0x06, 0x00, 0x41, 0x01, 0x40, 0x00, 0x0b, 0x2c, 0x00, 0x20,
        0x00, 0x03, 0x01, 0x21, 0x00, 0x20, 0x00, 0x45, 0x04, 0x40, 0x41, 0xcd, 0x00, 0x0f, 0x0b, 0x1f, 0x40, 0x01,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x41, 0x02, 0x46, 0x04, 0x40, 0x10, 0x02, 0x0b, 0x20, 0x00, 0x41, 0x01, 0x6b,
        0x08, 0x00, 0x0b, 0x00, 0x0b, 0x0b, 0x04, 0x00, 0x41, 0x05, 0x0b, 0x06, 0x00, 0x10, 0x02, 0x10, 0x11, 0x0b,
        0x09, 0x00, 0x20, 0x00, 0x10, 0x09, 0x41, 0x01, 0x6a, 0x0b,
    };
    wasm_module_t* m;

    memset(host, 0, sizeof(*host));
    host->reenter_export = "sum";
    WASM_CHECK_OK(t, wasm_bind_host_func(rt, "host", "yield", "i(i)", wasm_test_resume_yield, host));
    WASM_CHECK_OK(t, wasm_bind_host_func(rt, "host", "reenter", "i(i)", wasm_test_resume_reenter, host));
    WASM_CHECK_OK(t, wasm_bind_host_func(rt, "host", "interrupt", "()", wasm_test_resume_interrupt, host));
    m = wasm_load(rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt->error_msg);
    host->mod = m;
    return m;
}

TEST(test_resume_fuel_steps_match_unmetered_runs) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t args[2];
    wasm_value_t expected = wasm_i64(0);
    wasm_value_t result = wasm_i64(0);
    uint32_t steps;

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(1000);
    WASM_CHECK_OK(t, wasm_call(m, "sum", args, 1, &expected, 1));
    steps = wasm_test_run_in_fuel_steps(t, m, "sum", args, 1, 5, 0, &result);
    CHECK_MSG(t, steps > 100u, "expected many fuel stops, got %u", (unsigned)steps);
    WASM_CHECK_I64(t, result.of.i64, expected.of.i64);
    WASM_CHECK_I64(t, result.of.i64, 499500);

    args[0] = wasm_i32(500);
    args[1] = wasm_i32(0);
    WASM_CHECK_OK(t, wasm_call(m, "count", args, 2, &expected, 1));
    steps = wasm_test_run_in_fuel_steps(t, m, "count", args, 2, 3, 0, &result);
    CHECK(t, steps > 100u);
    WASM_CHECK_I32(t, result.of.i32, expected.of.i32);
    WASM_CHECK_I32(t, result.of.i32, 125250);

    args[0] = wasm_i32(4096);
    steps = wasm_test_run_in_fuel_steps(t, m, "fill", args, 1, 1, 1, &result);
    CHECK(t, steps > 0u);
    WASM_CHECK_I32(t, result.of.i32, 7);

    wasm_runtime_set_fuel(&rt, 10);
    CHECK(t, wasm_call(m, "grow", NULL, 0, &result, 1) == WASM_ERR_OUT_OF_FUEL);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_OUT_OF_FUEL);
    WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), 65536);
    wasm_runtime_set_fuel(&rt, 5000);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 1);
    WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), 2 * 65536);
    wasm_runtime_disable_fuel(&rt);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_resume_yielding_host_imports) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t host_result;
    wasm_value_t result = wasm_i32(0);
    uint32_t func_idx = 0;
    char backtrace[256];

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    arg = wasm_i32(5);
    CHECK(t, wasm_call(m, "rec", &arg, 1, &result, 1) == WASM_YIELD);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_YIELD);
    WASM_CHECK_I32(t, host.last_arg, 100);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 6);
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, NULL));
    WASM_CHECK_I32(t, (int32_t)func_idx, 5);
    wasm_runtime_backtrace(&rt, backtrace, sizeof(backtrace));
    CHECK_MSG(t, backtrace[0] != '\0', "%s", "expected a backtrace while suspended");

    CHECK(t, wasm_call(m, "sum", &arg, 1, &result, 1) == WASM_ERR_SUSPENDED);
    CHECK(t, wasm_runtime_last_error(&rt) == WASM_ERR_SUSPENDED);
    CHECK_MSG(t, strstr(rt.error_msg, "suspended") != NULL, "%s", rt.error_msg);
    {
        static const uint8_t start_module[] = {
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
            0x01, 0x00, 0x08, 0x01, 0x00, 0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
        };
        CHECK(t, wasm_load(&rt, start_module, sizeof(start_module)) == NULL);
        CHECK(t, wasm_runtime_last_error(&rt) == WASM_ERR_SUSPENDED);
    }
    CHECK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    host_result = wasm_i64(7);
    CHECK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    host_result = wasm_i32(7);
    CHECK(t, wasm_runtime_resume(&rt, &host_result, 1, NULL, 0) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_YIELD);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 5 + 4 + 3 + 2 + 1 + 7);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    CHECK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1) == WASM_ERR_MALFORMED);

    arg = wasm_i32(3);
    CHECK(t, wasm_call(m, "catch_after_yield", &arg, 1, &result, 1) == WASM_YIELD);
    host_result = wasm_i32(42);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);

    arg = wasm_i32(5);
    CHECK(t, wasm_call(m, "tail", &arg, 1, &result, 1) == WASM_YIELD);
    WASM_CHECK_I32(t, host.last_arg, 6);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 1);
    host_result = wasm_i32(10);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 20);

    arg = wasm_i32(7);
    CHECK(t, wasm_call(m, "tail_import", &arg, 1, &result, 1) == WASM_YIELD);
    WASM_CHECK_I32(t, host.last_arg, 7);
    host_result = wasm_i32(9);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 9);
    CHECK(t, wasm_call(m, "nested_tail_import", &arg, 1, &result, 1) == WASM_YIELD);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 2);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 10);

    arg = wasm_i32(3);
    CHECK(t, wasm_call(m, "rec", &arg, 1, &result, 1) == WASM_YIELD);
    wasm_runtime_cancel(&rt);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 0);
    arg = wasm_i32(10);
    WASM_CHECK_OK(t, wasm_call(m, "sum", &arg, 1, &result, 1));
    WASM_CHECK_I64(t, result.of.i64, 45);

    arg = wasm_i32(1);
    CHECK(t, wasm_call_index(m, 0, &arg, 1, &result, 1) == WASM_ERR_TRAP);
    CHECK_MSG(t, strstr(rt.error_msg, "yielded outside") != NULL, "%s", rt.error_msg);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);

    host.reenter_export = "rec";
    CHECK(t, wasm_call(m, "outer", &arg, 1, &result, 1) == WASM_ERR_TRAP);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);

    arg = wasm_i32(2);
    CHECK(t, wasm_call(m, "rec", &arg, 1, &result, 1) == WASM_YIELD);
    wasm_module_free(m);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    wasm_destroy(&rt);
}

TEST(test_resume_keeps_gc_objects_alive_across_collections) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(9);
    wasm_value_t host_result = wasm_i32(0);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    CHECK(t, wasm_call(m, "gc_keep", &arg, 1, &result, 1) == WASM_YIELD);
    wasm_gc_collect(&rt);
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 9 + 5);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_resume_collects_with_references_on_the_operand_stack) {
    /* yield_refs(n, ext) and yield_exn() yield in host.yield with GC, i31, extern, and exn references
     * on the operand stack of two frames, returning n + 137 and 5; fuel_refs(n, ext) builds n boxes in
     * a loop that carries a reference and returns n + 1003. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x28, 0x08, 0x5f, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f,
        0x01, 0x7f, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x02, 0x7f, 0x6f, 0x01, 0x7f, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x01,
        0x69, 0x01, 0x7f, 0x60, 0x01, 0x69, 0x00, 0x60, 0x01, 0x64, 0x00, 0x01, 0x64, 0x00, 0x02, 0x0e, 0x01, 0x04,
        0x68, 0x6f, 0x73, 0x74, 0x05, 0x79, 0x69, 0x65, 0x6c, 0x64, 0x00, 0x01, 0x03, 0x05, 0x04, 0x01, 0x03, 0x04,
        0x03, 0x0d, 0x03, 0x01, 0x00, 0x02, 0x07, 0x26, 0x03, 0x0a, 0x79, 0x69, 0x65, 0x6c, 0x64, 0x5f, 0x72, 0x65,
        0x66, 0x73, 0x00, 0x02, 0x09, 0x79, 0x69, 0x65, 0x6c, 0x64, 0x5f, 0x65, 0x78, 0x6e, 0x00, 0x03, 0x09, 0x66,
        0x75, 0x65, 0x6c, 0x5f, 0x72, 0x65, 0x66, 0x73, 0x00, 0x04, 0x0a, 0xcb, 0x01, 0x04, 0x21, 0x00, 0x41, 0x0a,
        0xfb, 0x00, 0x00, 0x20, 0x00, 0x41, 0x14, 0xfb, 0x00, 0x00, 0x41, 0x01, 0x10, 0x00, 0x1a, 0xfb, 0x02, 0x00,
        0x00, 0x6a, 0x21, 0x00, 0xfb, 0x02, 0x00, 0x00, 0x20, 0x00, 0x6a, 0x0b, 0x29, 0x01, 0x01, 0x7f, 0x20, 0x01,
        0x41, 0xe4, 0x00, 0xfb, 0x00, 0x00, 0x41, 0x07, 0xfb, 0x1c, 0x20, 0x00, 0x10, 0x01, 0x21, 0x02, 0xfb, 0x1d,
        0x20, 0x02, 0x6a, 0x21, 0x02, 0xfb, 0x02, 0x00, 0x00, 0x20, 0x02, 0x6a, 0x21, 0x02, 0x1a, 0x20, 0x02, 0x0b,
        0x21, 0x00, 0x02, 0x69, 0x1f, 0x40, 0x01, 0x03, 0x00, 0x41, 0x05, 0x08, 0x00, 0x0b, 0x00, 0x0b, 0x41, 0x02,
        0x10, 0x00, 0x1a, 0x02, 0x05, 0x1f, 0x06, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x0b, 0x00, 0x0b, 0x0b, 0x5b, 0x02,
        0x01, 0x7f, 0x01, 0x63, 0x00, 0x20, 0x01, 0x41, 0x03, 0xfb, 0x1c, 0x41, 0xe8, 0x07, 0xfb, 0x00, 0x00, 0x41,
        0x00, 0xfb, 0x00, 0x00, 0x03, 0x07, 0x21, 0x03, 0x41, 0x05, 0xfb, 0x00, 0x00, 0x20, 0x03, 0xfb, 0x02, 0x00,
        0x00, 0x41, 0x01, 0x6a, 0xfb, 0x00, 0x00, 0x20, 0x02, 0x41, 0x01, 0x6a, 0x21, 0x02, 0x20, 0x02, 0x20, 0x00,
        0x49, 0x0d, 0x00, 0x21, 0x03, 0x1a, 0x20, 0x03, 0xd4, 0x0b, 0xfb, 0x02, 0x00, 0x00, 0x21, 0x02, 0xfb, 0x02,
        0x00, 0x00, 0x20, 0x02, 0x6a, 0x21, 0x02, 0xfb, 0x1d, 0x20, 0x02, 0x6a, 0x21, 0x02, 0x1a, 0x20, 0x02, 0x0b,
    };
    static const uint64_t fuel_steps[] = { 1, 2, 3, 7, 50 };
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t args[2];
    wasm_value_t host_result = wasm_i32(0);
    wasm_value_t result = wasm_i32(0);
    uint32_t i;

    wasm_init(&rt);
    memset(&host, 0, sizeof(host));
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "host", "yield", "i(i)", wasm_test_resume_yield, &host));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(4);
    args[1] = wasm_externref((uintptr_t)0x1230u);
    CHECK(t, wasm_call(m, "yield_refs", args, 2, &result, 1) == WASM_YIELD);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 2);
    wasm_gc_collect(&rt);
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 4 + 137);

    CHECK(t, wasm_call(m, "yield_exn", NULL, 0, &result, 1) == WASM_YIELD);
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 5);

    for (i = 0; i < sizeof(fuel_steps) / sizeof(fuel_steps[0]); i++) {
        uint32_t steps = 0;
        wasm_error_t err;

        args[0] = wasm_i32(6);
        wasm_runtime_set_fuel(&rt, fuel_steps[i]);
        err = wasm_call(m, "fuel_refs", args, 2, &result, 1);
        while (err == WASM_ERR_OUT_OF_FUEL && steps < 100000u) {
            wasm_gc_collect(&rt);
            wasm_runtime_set_fuel(&rt, fuel_steps[i]);
            err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
            steps++;
        }
        CHECK_MSG(t, err == WASM_OK, "fuel step %u stopped with %s: %s", (unsigned)fuel_steps[i],
                  wasm_error_string(err), rt.error_msg);
        CHECK(t, steps > 0);
        WASM_CHECK_I32(t, result.of.i32, 6 + 1003);
    }
    wasm_runtime_disable_fuel(&rt);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_resume_after_interrupts) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(10);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    CHECK(t, wasm_call(m, "self_interrupt", &arg, 1, &result, 1) == WASM_ERR_INTERRUPTED);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
    wasm_runtime_interrupt(&rt);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 10);

    arg = wasm_i32(5);
    CHECK(t, wasm_call(m, "catch_loop", &arg, 1, &result, 1) == WASM_ERR_INTERRUPTED);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 77);

    CHECK(t, wasm_call(m, "call_after_interrupt", NULL, 0, &result, 1) == WASM_ERR_INTERRUPTED);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 2);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 5);

    wasm_runtime_interrupt(&rt);
    CHECK(t, wasm_call(m, "sum", &arg, 1, &result, 1) == WASM_ERR_INTERRUPTED);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);

#if defined(WASM_TEST_THREADS)
    {
        wasm_test_thread_start_t start;
        wasm_test_thread_t thread;
        uint8_t* data = wasm_memory_data(m, 0);

        memset(data, 0, 4);
        start.fn = wasm_test_interrupt_later;
        start.arg = &rt;
        CHECK(t, wasm_test_thread_spawn(&thread, &start));
        CHECK_MSG(t, wasm_call(m, "spin_until_flag", NULL, 0, &result, 1) == WASM_ERR_INTERRUPTED, "%s",
                  rt.error_msg);
        wasm_test_thread_join(thread);
        CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
        data[0] = 1;
        WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
        CHECK(t, result.of.i32 > 0);
    }
#endif

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_resume_reentered_calls_unwind_on_fuel) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(1000);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    host.reenter_fuel = 5;
    CHECK(t, wasm_call(m, "outer", &arg, 1, &result, 1) == WASM_ERR_OUT_OF_FUEL);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    host.reenter_fuel = 0;
    wasm_runtime_disable_fuel(&rt);
    arg = wasm_i32(10);
    WASM_CHECK_OK(t, wasm_call(m, "outer", &arg, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 46);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case call_cases[] = {
    { "public api: format-call packs ints and floats", test_format_call_api },
    { "public api: bind_host_func registers typed callbacks", test_bind_host_func_api },
    { "public api: bind_host_func callbacks can read module userdata", test_bind_host_func_uses_module_userdata },
    { "public api: module-first host callbacks can read guest memory", test_host_callback_module_reads_memory },
    { "call with 17 params survives dynamic signature storage", test_large_param_call },
    { "call with 5 results survives dynamic result storage", test_multi_result_call },
    { "resume: fuel stops resumed step by step match unmetered runs", test_resume_fuel_steps_match_unmetered_runs },
    { "resume: yielding host imports suspend nested frames until resumed or cancelled", test_resume_yielding_host_imports },
    { "resume: GC objects in suspended frames survive collections", test_resume_keeps_gc_objects_alive_across_collections },
    { "resume: collections while suspended trace references on the operand stack", test_resume_collects_with_references_on_the_operand_stack },
    { "resume: interrupted calls continue where they stopped", test_resume_after_interrupts },
    { "resume: a call re-entered from a host function unwinds when it runs out of fuel", test_resume_reentered_calls_unwind_on_fuel },
    { "public api: failed calls record the trap code and message", test_failed_calls_record_trap_codes_and_messages },
};
const test_suite call_suite = { "call", call_cases, COUNTOF(call_cases) };

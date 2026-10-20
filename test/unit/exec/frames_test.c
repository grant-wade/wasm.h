#include "../support.h"

TEST(test_runtime_config_limits_call_depth_and_backtrace) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    wasm_config_t cfg;
    uint32_t func_idx = UINT32_MAX;
    uint32_t offset = UINT32_MAX;

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
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_config_default(&cfg);
    cfg.max_call_depth = 4;
    cfg.frame_arena_size = 128;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_CALL_STACK_EXHAUSTED,
              "expected WASM_ERR_CALL_STACK_EXHAUSTED, got %d",
              (int)err);
    CHECK_MSG(t, wasm_runtime_call_depth(&rt) == 4,
              "expected backtrace depth 4, got %u",
              (unsigned)wasm_runtime_call_depth(&rt));
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &offset));
    CHECK_MSG(t, func_idx == 0, "expected top backtrace function 0, got %u", (unsigned)func_idx);
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 3, NULL, &func_idx, &offset));
    CHECK_MSG(t, func_idx == 0, "expected deepest captured function 0, got %u", (unsigned)func_idx);
    CHECK_MSG(t, wasm_runtime_call_frame(&rt, 4, NULL, &func_idx, &offset) == WASM_ERR_MALFORMED,
              "%s",
              "expected querying beyond configured backtrace depth to fail");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_public_backtrace_helpers_capture_traps) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t bad_arg;
    wasm_error_t err;
    uint32_t func_idx;
    uint32_t offset;
    char backtrace[256];

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body0 = { 0 };
        wasm_builder_t body1 = { 0 };
        wasm_builder_t body2 = { 0 };

        emit_leb128_u32(&sec, 3);

        emit_leb128_u32(&body0, 0);
        emit(&body0, 0x10);
        emit_leb128_u32(&body0, 1);
        emit(&body0, 0x0B);
        emit_leb128_u32(&sec, body0.len);
        emit_bytes(&sec, body0.buf, body0.len);

        emit_leb128_u32(&body1, 0);
        emit(&body1, 0x10);
        emit_leb128_u32(&body1, 2);
        emit(&body1, 0x0B);
        emit_leb128_u32(&sec, body1.len);
        emit_bytes(&sec, body1.buf, body1.len);

        emit_leb128_u32(&body2, 0);
        emit(&body2, 0x00);
        emit(&body2, 0x0B);
        emit_leb128_u32(&sec, body2.len);
        emit_bytes(&sec, body2.buf, body2.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, wasm_runtime_call_depth(&rt) == 0, "%s", "expected empty initial backtrace");

    err = wasm_call(m, "run", NULL, 0, NULL, 0);
    CHECK_MSG(t, err == WASM_ERR_UNREACHABLE, "%s", "expected unreachable trap");

    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 3);

    err = wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 2);
        WASM_CHECK_I32(t, (int32_t)offset, 0);
    }

    err = wasm_runtime_call_frame(&rt, 1, NULL, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 1);
        WASM_CHECK_I32(t, (int32_t)offset, 0);
    }

    err = wasm_runtime_call_frame(&rt, 2, NULL, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 0);
        WASM_CHECK_I32(t, (int32_t)offset, 0);
    }

    wasm_runtime_backtrace(&rt, backtrace, sizeof(backtrace));
    CHECK_MSG(t, strstr(backtrace, "#0 func[2] at offset 0x0000") != NULL,
              "%s", "expected top frame in formatted backtrace");
    CHECK_MSG(t, strstr(backtrace, "#1 func[1] at offset 0x0000") != NULL,
              "%s", "expected middle frame in formatted backtrace");
    CHECK_MSG(t, strstr(backtrace, "#2 func[0] at offset 0x0000") != NULL,
              "%s", "expected root frame in formatted backtrace");

    err = wasm_runtime_call_frame(&rt, 3, NULL, &func_idx, &offset);
    CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected out-of-range frame query rejection");

    bad_arg = wasm_i32(7);
    err = wasm_call(m, "run", &bad_arg, 1, NULL, 0);
    CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected signature mismatch rejection");
    CHECK_MSG(t, wasm_runtime_call_depth(&rt) == 0, "%s", "expected backtrace reset before a non-executing call failure");

    wasm_runtime_backtrace(&rt, backtrace, sizeof(backtrace));
    CHECK_MSG(t, backtrace[0] == '\0', "%s", "expected empty backtrace string after reset");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_backtraces_name_modules_and_functions) {
    /* (module $demo (func $inner unreachable) (func $outer (export "outer") call $inner)) */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x03, 0x02, 0x00,
        0x00, 0x07, 0x09, 0x01, 0x05, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x00, 0x01, 0x0a, 0x0a, 0x02, 0x03, 0x00, 0x00,
        0x0b, 0x04, 0x00, 0x10, 0x00, 0x0b, 0x00, 0x1d, 0x04, 0x6e, 0x61, 0x6d, 0x65, 0x00, 0x05, 0x04, 0x64, 0x65,
        0x6d, 0x6f, 0x01, 0x0f, 0x02, 0x00, 0x05, 0x69, 0x6e, 0x6e, 0x65, 0x72, 0x01, 0x05, 0x6f, 0x75, 0x74, 0x65,
        0x72,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_module_t* frame_module = NULL;
    uint32_t func_idx = 0;
    char trace[256];

    wasm_init(&rt);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    CHECK(t, wasm_module_name(m) && strcmp(wasm_module_name(m), "demo") == 0);
    CHECK(t, wasm_func_name(m, 1) && strcmp(wasm_func_name(m, 1), "outer") == 0);
    CHECK(t, wasm_func_name(m, 2) == NULL);

    CHECK(t, wasm_call(m, "outer", NULL, 0, NULL, 0) == WASM_ERR_UNREACHABLE);
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, &frame_module, &func_idx, NULL));
    CHECK(t, frame_module == m && func_idx == 0u);
    wasm_runtime_backtrace(&rt, trace, sizeof(trace));
    CHECK_MSG(t, strstr(trace, "#0 demo!inner (func[0]) at offset") != NULL &&
                     strstr(trace, "#1 demo!outer (func[1]) at offset") != NULL,
              "unexpected backtrace:\n%s", trace);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_frames_cases[] = {
    { "runtime config: call depth limit drives captured backtraces", test_runtime_config_limits_call_depth_and_backtrace },
    { "backtrace: frames name their module and function from the name section", test_backtraces_name_modules_and_functions },
    { "public api: backtrace helpers expose trap call stacks", test_public_backtrace_helpers_capture_traps },
};
const test_suite exec_frames_suite = { "exec.frames", exec_frames_cases, COUNTOF(exec_frames_cases) };

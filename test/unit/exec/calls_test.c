#include "../support.h"

TEST(test_tail_call_self_recursion) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[1];
    wasm_value_t result;
    wasm_error_t err;

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
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 9);
        emit(&sec, 'c');
        emit(&sec, 'o');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 'd');
        emit(&sec, 'o');
        emit(&sec, 'w');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x45);
        emit(&body, 0x04);
        emit(&body, 0x7F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 42);
        emit(&body, 0x05);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x6B);
        emit(&body, 0x12);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
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

    args[0] = wasm_i32(2000);
    err = wasm_call(m, "countdown", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 42);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_tail_call_indirect) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

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
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
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
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 77);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x13);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
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

    err = wasm_call(m, "call", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 77);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_host_grow_memory(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    return wasm_memory_grow(mod, 0, 1) == 2 ? WASM_OK : WASM_ERR_TRAP;
}

TEST(test_memory_grown_by_callee_or_host_is_visible_to_caller) {
    /* wasm() and host() store 1 at address 0, grow memory through a Wasm callee or env.grow, store
       42 or 43 in the new page, and return it plus the value at 0; past_end() loads across the end
       of the third page. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7f,
        0x02, 0x0c, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x67, 0x72, 0x6f, 0x77, 0x00, 0x00, 0x03, 0x05, 0x04, 0x00,
        0x01, 0x01, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x1a, 0x03, 0x04, 0x77, 0x61, 0x73, 0x6d, 0x00, 0x02,
        0x04, 0x68, 0x6f, 0x73, 0x74, 0x00, 0x03, 0x08, 0x70, 0x61, 0x73, 0x74, 0x5f, 0x65, 0x6e, 0x64, 0x00, 0x04,
        0x0a, 0x57, 0x04, 0x07, 0x00, 0x41, 0x01, 0x40, 0x00, 0x1a, 0x0b, 0x21, 0x00, 0x41, 0x00, 0x41, 0x01, 0x36,
        0x02, 0x00, 0x10, 0x01, 0x41, 0x80, 0x80, 0x04, 0x41, 0x2a, 0x36, 0x02, 0x00, 0x41, 0x80, 0x80, 0x04, 0x28,
        0x02, 0x00, 0x41, 0x00, 0x28, 0x02, 0x00, 0x6a, 0x0b, 0x21, 0x00, 0x41, 0x00, 0x41, 0x01, 0x36, 0x02, 0x00,
        0x10, 0x00, 0x41, 0x80, 0x80, 0x08, 0x41, 0x2b, 0x36, 0x02, 0x00, 0x41, 0x80, 0x80, 0x08, 0x28, 0x02, 0x00,
        0x41, 0x00, 0x28, 0x02, 0x00, 0x6a, 0x0b, 0x09, 0x00, 0x41, 0xfd, 0xff, 0x0b, 0x28, 0x02, 0x00, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "grow", "()", wasm_test_host_grow_memory, NULL));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_OK(t, wasm_call(m, "wasm", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 43);
    WASM_CHECK_OK(t, wasm_call(m, "host", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 44);
    CHECK(t, wasm_call(m, "past_end", NULL, 0, &result, 1) == WASM_ERR_OUT_OF_BOUNDS);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_calls_collect(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                            wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_gc_collect(wasm_module_runtime(mod));
    return WASM_OK;
}

static wasm_error_t wasm_test_calls_interrupt(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_runtime_interrupt(wasm_module_runtime(mod));
    return WASM_OK;
}

/* Exports deep, ci, ci64, tail_ci, gcdeep, interrupt_at (function 10), and catcher. */
static wasm_module_t* wasm_test_load_calls_module(test_ctx* t, wasm_runtime_t* rt) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x1d, 0x06, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60,
        0x01, 0x7f, 0x01, 0x7f, 0x5f, 0x01, 0x7f, 0x00, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x02, 0x7e,
        0x7f, 0x01, 0x7f, 0x02, 0x21, 0x02, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x07, 0x63, 0x6f, 0x6c, 0x6c, 0x65, 0x63,
        0x74, 0x00, 0x03, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x09, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74,
        0x00, 0x03, 0x03, 0x0d, 0x0c, 0x00, 0x01, 0x01, 0x00, 0x05, 0x00, 0x01, 0x03, 0x01, 0x01, 0x01, 0x01, 0x04,
        0x07, 0x02, 0x70, 0x00, 0x04, 0x70, 0x04, 0x03, 0x0d, 0x03, 0x01, 0x00, 0x04, 0x07, 0x40, 0x07, 0x04, 0x64,
        0x65, 0x65, 0x70, 0x00, 0x04, 0x02, 0x63, 0x69, 0x00, 0x05, 0x04, 0x63, 0x69, 0x36, 0x34, 0x00, 0x06, 0x07,
        0x74, 0x61, 0x69, 0x6c, 0x5f, 0x63, 0x69, 0x00, 0x07, 0x06, 0x67, 0x63, 0x64, 0x65, 0x65, 0x70, 0x00, 0x08,
        0x0c, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x5f, 0x61, 0x74, 0x00, 0x0a, 0x07, 0x63, 0x61,
        0x74, 0x63, 0x68, 0x65, 0x72, 0x00, 0x0b, 0x09, 0x12, 0x02, 0x02, 0x00, 0x41, 0x00, 0x0b, 0x00, 0x02, 0x02,
        0x03, 0x02, 0x01, 0x42, 0x01, 0x0b, 0x00, 0x01, 0x02, 0x0a, 0xd3, 0x02, 0x0c, 0x07, 0x00, 0x20, 0x00, 0x20,
        0x01, 0x6a, 0x0b, 0x07, 0x00, 0x41, 0x00, 0x20, 0x00, 0x6b, 0x0b, 0x45, 0x03, 0x01, 0x7f, 0x01, 0x7e, 0x01,
        0x7c, 0x20, 0x01, 0x20, 0x02, 0xa7, 0x20, 0x03, 0xaa, 0x72, 0x72, 0x04, 0x40, 0x00, 0x0b, 0x20, 0x00, 0x21,
        0x01, 0x20, 0x00, 0xad, 0x21, 0x02, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x21, 0x03, 0x20,
        0x00, 0x45, 0x04, 0x7f, 0x41, 0x00, 0x05, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x10, 0x04, 0x20, 0x01, 0x20, 0x02,
        0xa7, 0x20, 0x03, 0xaa, 0x6a, 0x6a, 0x6a, 0x0b, 0x0b, 0x0b, 0x00, 0x20, 0x01, 0x20, 0x01, 0x20, 0x00, 0x11,
        0x00, 0x00, 0x0b, 0x0b, 0x00, 0x20, 0x01, 0x20, 0x01, 0x20, 0x00, 0x11, 0x00, 0x01, 0x0b, 0x0b, 0x00, 0x20,
        0x01, 0x20, 0x01, 0x20, 0x00, 0x13, 0x00, 0x00, 0x0b, 0x2b, 0x02, 0x01, 0x63, 0x02, 0x01, 0x7e, 0x20, 0x00,
        0xfb, 0x00, 0x02, 0x21, 0x01, 0x20, 0x00, 0x45, 0x04, 0x7f, 0x10, 0x00, 0x10, 0x09, 0x10, 0x00, 0x41, 0x00,
        0x05, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x10, 0x08, 0x0b, 0x20, 0x01, 0xfb, 0x02, 0x02, 0x00, 0x6a, 0x0b, 0x1d,
        0x01, 0x01, 0x7f, 0x03, 0x40, 0x20, 0x00, 0xfb, 0x00, 0x02, 0x1a, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x21, 0x00,
        0x20, 0x00, 0x41, 0xa0, 0x9c, 0x01, 0x49, 0x0d, 0x00, 0x0b, 0x0b, 0x28, 0x01, 0x01, 0x7f, 0x20, 0x00, 0x41,
        0x03, 0x6c, 0x21, 0x01, 0x20, 0x00, 0x41, 0x03, 0x46, 0x04, 0x40, 0x10, 0x01, 0x0b, 0x20, 0x00, 0x45, 0x04,
        0x7f, 0x20, 0x01, 0x05, 0x20, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x10, 0x0a, 0x6a, 0x0b, 0x0b, 0x23, 0x01,
        0x01, 0x7e, 0x20, 0x00, 0x41, 0xe8, 0x07, 0x6a, 0xad, 0x21, 0x01, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00,
        0x00, 0x20, 0x00, 0x10, 0x0c, 0x1a, 0x0b, 0x41, 0x7f, 0x0f, 0x0b, 0x20, 0x01, 0xa7, 0x6a, 0x0b, 0x1f, 0x01,
        0x01, 0x7f, 0x20, 0x00, 0x41, 0x07, 0x6a, 0x21, 0x01, 0x20, 0x00, 0x45, 0x04, 0x40, 0x41, 0x05, 0x08, 0x00,
        0x0b, 0x20, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x10, 0x0d, 0x6a, 0x0b, 0x20, 0x01, 0x01, 0x7f, 0x41, 0x01,
        0x21, 0x01, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x20, 0x01, 0x20, 0x00, 0x10, 0x0c, 0x6a, 0x0f,
        0x0b, 0x00, 0x0b, 0x20, 0x01, 0x6a, 0x08, 0x00, 0x0b,
    };
    wasm_import_t imp;
    wasm_module_t* m;

    memset(&imp, 0, sizeof(imp));
    imp.module = "host";
    imp.name = "collect";
    imp.func = wasm_test_calls_collect;
    WASM_CHECK_OK(t, wasm_register_import(rt, &imp));
    imp.name = "interrupt";
    imp.func = wasm_test_calls_interrupt;
    WASM_CHECK_OK(t, wasm_register_import(rt, &imp));
    m = wasm_load(rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", wasm_runtime_error_message(rt));
    return m;
}

static int32_t wasm_test_call_i32(test_ctx* t, wasm_module_t* m, const char* name, int32_t arg) {
    wasm_value_t value = wasm_i32(arg);
    wasm_value_t result = wasm_i32(-1);

    WASM_CHECK_OK(t, wasm_call(m, name, &value, 1, &result, 1));
    return result.of.i32;
}

TEST(test_calls_reach_the_depth_limit_with_fresh_locals_per_frame) {
    static const size_t arena_sizes[] = { WASM_FRAME_ARENA_SIZE, 64 };
    size_t k;

    for (k = 0; k < sizeof(arena_sizes) / sizeof(arena_sizes[0]); k++) {
        wasm_runtime_t rt;
        wasm_config_t cfg;
        wasm_module_t* m;
        wasm_value_t arg;
        wasm_value_t result = wasm_i32(0);
        int round;

        wasm_config_default(&cfg);
        cfg.max_call_depth = 300;
        cfg.frame_arena_size = arena_sizes[k];
        wasm_init_config(&rt, &cfg);
        m = wasm_test_load_calls_module(t, &rt);
        if (!m) {
            wasm_destroy(&rt);
            continue;
        }
        for (round = 0; round < 3; round++) {
            WASM_CHECK_I32(t, wasm_test_call_i32(t, m, "deep", 299), 299 * 299 + 2 * 299);
            arg = wasm_i32(300);
            CHECK(t, wasm_call(m, "deep", &arg, 1, &result, 1) == WASM_ERR_CALL_STACK_EXHAUSTED);
            WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 300);
            CHECK(t, rt.exec->call_frame_sp == 0 && rt.exec->sp == 0);
            CHECK(t, rt.exec->frame_arena != NULL && rt.exec->frame_arena->prev == NULL && rt.exec->frame_arena->used == 0u);
        }
        arg = wasm_i32(40);
        CHECK(t, wasm_test_run_in_fuel_steps(t, m, "deep", &arg, 1, 1, 0, &result) > 500u);
        WASM_CHECK_I32(t, result.of.i32, 40 * 40 + 2 * 40);
        wasm_module_free(m);
        wasm_destroy(&rt);
    }
}

TEST(test_call_indirect_traps_on_mismatched_null_and_out_of_range_entries) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[2];
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    m = wasm_test_load_calls_module(t, &rt);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    args[1] = wasm_i32(21);
    args[0] = wasm_i32(0);
    WASM_CHECK_OK(t, wasm_call(m, "ci", args, 2, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_OK(t, wasm_call(m, "tail_ci", args, 2, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    args[0] = wasm_i32(1);
    CHECK(t, wasm_call(m, "ci", args, 2, &result, 1) == WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
    CHECK(t, wasm_call(m, "tail_ci", args, 2, &result, 1) == WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
    args[0] = wasm_i32(2);
    CHECK(t, wasm_call(m, "ci", args, 2, &result, 1) == WASM_ERR_UNINITIALIZED_TABLE);
    args[0] = wasm_i32(4);
    CHECK(t, wasm_call(m, "ci", args, 2, &result, 1) == WASM_ERR_UNDEFINED_ELEMENT);
    args[0] = wasm_i32(-1);
    CHECK(t, wasm_call(m, "tail_ci", args, 2, &result, 1) == WASM_ERR_UNDEFINED_ELEMENT);

    args[0] = wasm_i64(1);
    WASM_CHECK_OK(t, wasm_call(m, "ci64", args, 2, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    args[0] = wasm_i64(0);
    CHECK(t, wasm_call(m, "ci64", args, 2, &result, 1) == WASM_ERR_UNINITIALIZED_TABLE);
    args[0] = wasm_i64(3);
    CHECK(t, wasm_call(m, "ci64", args, 2, &result, 1) == WASM_ERR_UNDEFINED_ELEMENT);
    args[0] = wasm_i64(((int64_t)1 << 32) + 1);
    CHECK(t, wasm_call(m, "ci64", args, 2, &result, 1) == WASM_ERR_UNDEFINED_ELEMENT);
    CHECK(t, rt.exec->call_frame_sp == 0 && rt.exec->sp == 0);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_cross_module_call_indirect_checks_types_per_calling_module) {
    static const uint8_t a_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
        0x02, 0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x02, 0x07, 0x05, 0x01, 0x01, 0x74, 0x01, 0x00, 0x09, 0x07,
        0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
    };
    static const uint8_t b_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x17, 0x04, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60,
        0x02, 0x7f, 0x7f, 0x01, 0x7e, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x01, 0x7e, 0x02, 0x09, 0x01,
        0x01, 0x61, 0x01, 0x74, 0x01, 0x70, 0x00, 0x02, 0x03, 0x04, 0x03, 0x02, 0x03, 0x02, 0x07, 0x16, 0x03, 0x02,
        0x6f, 0x6b, 0x00, 0x00, 0x03, 0x62, 0x61, 0x64, 0x00, 0x01, 0x07, 0x74, 0x61, 0x69, 0x6c, 0x5f, 0x6f, 0x6b,
        0x00, 0x02, 0x0a, 0x25, 0x03, 0x0b, 0x00, 0x20, 0x00, 0x20, 0x00, 0x41, 0x00, 0x11, 0x00, 0x00, 0x0b, 0x0b,
        0x00, 0x20, 0x00, 0x20, 0x00, 0x41, 0x00, 0x11, 0x01, 0x00, 0x0b, 0x0b, 0x00, 0x20, 0x00, 0x20, 0x00, 0x41,
        0x00, 0x13, 0x00, 0x00, 0x0b,
    };
    static const uint8_t c_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x17, 0x04, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7e, 0x60,
        0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x01, 0x7e, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x09, 0x01,
        0x01, 0x61, 0x01, 0x74, 0x01, 0x70, 0x00, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x07, 0x0c, 0x02, 0x03, 0x62,
        0x61, 0x64, 0x00, 0x00, 0x02, 0x6f, 0x6b, 0x00, 0x01, 0x0a, 0x19, 0x02, 0x0b, 0x00, 0x20, 0x00, 0x20, 0x00,
        0x41, 0x00, 0x11, 0x00, 0x00, 0x0b, 0x0b, 0x00, 0x20, 0x00, 0x20, 0x00, 0x41, 0x00, 0x11, 0x01, 0x00, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* a;
    wasm_module_t* b;
    wasm_module_t* c;
    wasm_value_t arg = wasm_i32(21);
    wasm_value_t result = wasm_i32(0);
    int round;

    wasm_init(&rt);
    a = wasm_load(&rt, a_bytes, sizeof(a_bytes));
    CHECK_MSG(t, a != NULL, "%s", rt.error_msg);
    if (!a) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "a", a));
    for (round = 0; round < 4; round++) {
        b = wasm_load(&rt, b_bytes, sizeof(b_bytes));
        CHECK_MSG(t, b != NULL, "%s", rt.error_msg);
        if (!b) break;
        WASM_CHECK_OK(t, wasm_call(b, "ok", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 42);
        WASM_CHECK_OK(t, wasm_call(b, "tail_ok", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 42);
        CHECK(t, wasm_call(b, "bad", &arg, 1, &result, 1) == WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
        WASM_CHECK_OK(t, wasm_call(b, "ok", &arg, 1, &result, 1));
        wasm_module_free(b);

        c = wasm_load(&rt, c_bytes, sizeof(c_bytes));
        CHECK_MSG(t, c != NULL, "%s", rt.error_msg);
        if (!c) break;
        CHECK(t, wasm_call(c, "bad", &arg, 1, &result, 1) == WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
        WASM_CHECK_OK(t, wasm_call(c, "ok", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 42);
        CHECK(t, wasm_call(c, "bad", &arg, 1, &result, 1) == WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
        wasm_module_free(c);
    }
    wasm_module_free(a);
    wasm_destroy(&rt);
}

typedef struct wasm_test_make_host_t {
    wasm_module_t* owner;
    int yield;
} wasm_test_make_host_t;

static wasm_error_t wasm_test_make_struct(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_test_make_host_t* host = (wasm_test_make_host_t*)userdata;
    wasm_value_t field = wasm_i32(7);

    (void)mod;
    (void)args;
    (void)num_args;
    (void)num_results;
    if (host->yield) return WASM_YIELD;
    return wasm_struct_new(host->owner, 1, &field, 1, &results[0]);
}

TEST(test_imports_called_indirectly_from_another_module_check_results_against_their_own_type) {
    /* a: types array, struct $s, func returning (ref null $s); imports host.make and exports it in table t.
     * b: types struct $s, array, the same func type; calls t[0] and returns 7 + ref.is_null. */
    static const uint8_t a_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0d, 0x03, 0x5e, 0x7f, 0x00, 0x5f, 0x01, 0x7f, 0x00,
        0x60, 0x00, 0x01, 0x63, 0x01, 0x02, 0x0d, 0x01, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x04, 0x6d, 0x61, 0x6b, 0x65,
        0x00, 0x02, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x07, 0x05, 0x01, 0x01, 0x74, 0x01, 0x00, 0x09, 0x07, 0x01,
        0x00, 0x41, 0x00, 0x0b, 0x01, 0x00,
    };
    static const uint8_t b_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x11, 0x04, 0x5f, 0x01, 0x7f, 0x00, 0x5e, 0x7e, 0x00,
        0x60, 0x00, 0x01, 0x63, 0x00, 0x60, 0x00, 0x01, 0x7f, 0x02, 0x09, 0x01, 0x01, 0x61, 0x01, 0x74, 0x01, 0x70,
        0x00, 0x01, 0x03, 0x02, 0x01, 0x03, 0x07, 0x07, 0x01, 0x03, 0x67, 0x65, 0x74, 0x00, 0x00, 0x0a, 0x0d, 0x01,
        0x0b, 0x00, 0x41, 0x07, 0x41, 0x00, 0x11, 0x02, 0x00, 0xd1, 0x6a, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_test_make_host_t host = { NULL, 0 };
    wasm_import_t imp;
    wasm_module_t* b;
    wasm_value_t made;
    wasm_value_t field = wasm_i32(9);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    memset(&imp, 0, sizeof(imp));
    imp.module = "host";
    imp.name = "make";
    imp.func = wasm_test_make_struct;
    imp.userdata = &host;
    WASM_CHECK_OK(t, wasm_register_import(&rt, &imp));
    host.owner = wasm_load(&rt, a_bytes, sizeof(a_bytes));
    CHECK_MSG(t, host.owner != NULL, "%s", rt.error_msg);
    if (!host.owner) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "a", host.owner));
    b = wasm_load(&rt, b_bytes, sizeof(b_bytes));
    CHECK_MSG(t, b != NULL, "%s", rt.error_msg);
    if (b) {
        WASM_CHECK_OK(t, wasm_call(b, "get", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 7);
        host.yield = 1;
        CHECK(t, wasm_call(b, "get", NULL, 0, &result, 1) == WASM_YIELD);
        WASM_CHECK_OK(t, wasm_struct_new(host.owner, 1, &field, 1, &made));
        WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &made, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 7);
        wasm_module_free(b);
    }
    wasm_module_free(host.owner);
    wasm_destroy(&rt);
}

TEST(test_calls_interrupted_at_callee_entry_resume_there) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(5);
    wasm_value_t result = wasm_i32(0);
    uint32_t func_idx = 0;
    uint32_t top_offset = 0;
    uint32_t caller_offset = 0;

    wasm_init(&rt);
    m = wasm_test_load_calls_module(t, &rt);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    CHECK(t, wasm_call(m, "interrupt_at", &arg, 1, &result, 1) == WASM_ERR_INTERRUPTED);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 4);
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &top_offset));
    WASM_CHECK_I32(t, (int32_t)func_idx, 10);
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 1, NULL, &func_idx, &caller_offset));
    WASM_CHECK_I32(t, (int32_t)func_idx, 10);
    CHECK(t, top_offset < caller_offset);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 45);
    WASM_CHECK_I32(t, wasm_test_call_i32(t, m, "deep", 10), 120);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_calls_keep_references_in_locals_through_collections_and_throws) {
    wasm_runtime_t rt;
    wasm_config_t cfg;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);

    wasm_config_default(&cfg);
    cfg.initial_gc_heap_size = 4096;
    cfg.frame_arena_size = 128;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_calls_module(t, &rt);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_I32(t, wasm_test_call_i32(t, m, "gcdeep", 200), 200 * 201 / 2);
    WASM_CHECK_I32(t, wasm_test_call_i32(t, m, "catcher", 0), 1005);
    WASM_CHECK_I32(t, wasm_test_call_i32(t, m, "catcher", 30), 1065);
    arg = wasm_i32(12);
    CHECK(t, wasm_test_run_in_fuel_steps(t, m, "gcdeep", &arg, 1, 1, 0, &result) > 100u);
    WASM_CHECK_I32(t, result.of.i32, 12 * 13 / 2);
    arg = wasm_i32(6);
    CHECK(t, wasm_test_run_in_fuel_steps(t, m, "catcher", &arg, 1, 1, 0, &result) > 50u);
    WASM_CHECK_I32(t, result.of.i32, 1017);
    CHECK(t, rt.exec->call_frame_sp == 0 && rt.exec->frame_arena != NULL && rt.exec->frame_arena->prev == NULL &&
                 rt.exec->frame_arena->used == 0u);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_calls_cases[] = {
    { "tail call: self-recursive return_call reuses the frame", test_tail_call_self_recursion },
    { "tail call: return_call_indirect dispatches through a table", test_tail_call_indirect },
    { "memory: growth by a callee or host is visible to the caller", test_memory_grown_by_callee_or_host_is_visible_to_caller },
    { "calls: recursion reaches the depth limit with fresh locals in every frame", test_calls_reach_the_depth_limit_with_fresh_locals_per_frame },
    { "calls: call_indirect traps on mismatched, null, and out-of-range entries", test_call_indirect_traps_on_mismatched_null_and_out_of_range_entries },
    { "calls: cross-module call_indirect checks types per calling module", test_cross_module_call_indirect_checks_types_per_calling_module },
    { "calls: imports called indirectly from another module check results against their own type", test_imports_called_indirectly_from_another_module_check_results_against_their_own_type },
    { "calls: an interrupt at a callee's entry suspends and resumes there", test_calls_interrupted_at_callee_entry_resume_there },
    { "calls: references in locals survive collections and throws through deep chains", test_calls_keep_references_in_locals_through_collections_and_throws },
};
const test_suite exec_calls_suite = { "exec.calls", exec_calls_cases, COUNTOF(exec_calls_cases) };

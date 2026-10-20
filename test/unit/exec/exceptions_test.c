#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/exec/interpreter.h"
#endif

TEST(test_exception_try_catch_payload) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[1];
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
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

    emit_single_tag_section(&mod, 1);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 6);
        emit(&sec, 'h');
        emit(&sec, 'a');
        emit(&sec, 'n');
        emit(&sec, 'd');
        emit(&sec, 'l');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x06);
        emit(&body, 0x7F);
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
        emit(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x07);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x6A);
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

    args[0] = wasm_i32(0);
    err = wasm_call(m, "handle", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);

    args[0] = wasm_i32(9);
    err = wasm_call(m, "handle", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 14);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_exception_catch_all) {
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
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'l');
        emit(&sec, 'l');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x19);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 55);
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

    err = wasm_call(m, "call", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 55);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_exception_rethrow) {
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
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 7);
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 'h');
        emit(&sec, 'r');
        emit(&sec, 'o');
        emit(&sec, 'w');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x07);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x09);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x07);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 100);
        emit(&body, 0x6A);
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

    err = wasm_call(m, "rethrow", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 109);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_exception_delegate) {
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
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 8);
        emit(&sec, 'd');
        emit(&sec, 'e');
        emit(&sec, 'l');
        emit(&sec, 'e');
        emit(&sec, 'g');
        emit(&sec, 'a');
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x18);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x07);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x6A);
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

    err = wasm_call(m, "delegate", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 8);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

typedef struct wasm_test_forward_t {
    wasm_module_t* module;
    uint32_t func_index;
} wasm_test_forward_t;

static wasm_error_t wasm_test_forward_call(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata) {
    wasm_test_forward_t* forward = (wasm_test_forward_t*)userdata;

    (void)mod;
    (void)num_results;
    return wasm__interp(forward->module, forward->func_index, (wasm_value_t*)args, num_args, results);
}

/* Exceptions thrown by an imported function reach handlers on every call path, and tail
   calls do not leave their own handlers active. */
TEST(test_exceptions_from_imports_reach_handlers_on_all_call_paths) {
    static const uint8_t thrower_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x02, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f,
        0x01, 0x7f, 0x03, 0x02, 0x01, 0x01, 0x0d, 0x03, 0x01, 0x00, 0x00, 0x07, 0x0d, 0x02, 0x01, 0x65, 0x04, 0x00,
        0x05, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x00, 0x00, 0x0a, 0x08, 0x01, 0x06, 0x00, 0x20, 0x00, 0x08, 0x00, 0x0b,
    };
    static const uint8_t catcher_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x13, 0x04, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x01,
        0x7f, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x00, 0x01, 0x7f, 0x02, 0x12, 0x02, 0x01, 0x74, 0x01, 0x65,
        0x04, 0x00, 0x02, 0x01, 0x74, 0x05, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x00, 0x00, 0x03, 0x09, 0x08, 0x03, 0x03,
        0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x07, 0x4d, 0x05, 0x11, 0x76, 0x69,
        0x61, 0x5f, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x00, 0x01, 0x0c,
        0x76, 0x69, 0x61, 0x5f, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x72, 0x65, 0x66, 0x00, 0x02, 0x0d, 0x74, 0x61, 0x69,
        0x6c, 0x5f, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x00, 0x06, 0x08, 0x74, 0x61, 0x69, 0x6c, 0x5f,
        0x72, 0x65, 0x66, 0x00, 0x07, 0x0b, 0x74, 0x61, 0x69, 0x6c, 0x5f, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x00,
        0x08, 0x09, 0x0b, 0x02, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x0a, 0xb1, 0x01, 0x08,
        0x16, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x41, 0x2a, 0x41, 0x00, 0x11, 0x00, 0x00, 0x1a,
        0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x15, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x41, 0x2b, 0xd2,
        0x00, 0x14, 0x00, 0x1a, 0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x18, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00,
        0x00, 0x20, 0x00, 0x41, 0x00, 0x13, 0x00, 0x00, 0x0b, 0x00, 0x0b, 0x41, 0xe4, 0x00, 0x6a, 0x0b, 0x17, 0x00,
        0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0xd2, 0x00, 0x15, 0x00, 0x0b, 0x00, 0x0b, 0x41,
        0xe4, 0x00, 0x6a, 0x0b, 0x15, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x12, 0x00,
        0x0b, 0x00, 0x0b, 0x41, 0xe4, 0x00, 0x6a, 0x0b, 0x13, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00,
        0x41, 0x05, 0x10, 0x03, 0x1a, 0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x13, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00,
        0x00, 0x00, 0x41, 0x06, 0x10, 0x04, 0x1a, 0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x13, 0x00, 0x02, 0x7f, 0x1f, 0x40,
        0x01, 0x00, 0x00, 0x00, 0x41, 0x07, 0x10, 0x05, 0x1a, 0x0b, 0x41, 0x7f, 0x0b, 0x0b,
    };
    static const struct {
        const char* name;
        int32_t expected;
    } cases[] = {
        { "via_call_indirect", 42 },
        { "via_call_ref", 43 },
        { "tail_indirect", 5 },
        { "tail_ref", 6 },
        { "tail_direct", 7 },
    };
    wasm_runtime_t rt;
    wasm_module_t* thrower;
    wasm_module_t* catcher = NULL;
    wasm_test_forward_t forward;
    wasm_import_t func_import;
    wasm_tag_import_t tag_import;
    uint32_t func_index = 0;
    uint32_t tag_index = 0;
    wasm_export_kind_t kind = WASM_EXPORT_FUNC;
    uint32_t i;

    wasm_init(&rt);
    thrower = wasm_load(&rt, thrower_bytes, sizeof(thrower_bytes));
    CHECK_MSG(t, thrower != NULL, "%s", rt.error_msg);
    if (thrower && wasm_find_export(thrower, "throw", &kind, &func_index) && wasm_find_export(thrower, "e", &kind, &tag_index)) {
        forward.module = thrower;
        forward.func_index = func_index;
        memset(&func_import, 0, sizeof(func_import));
        func_import.module = "t";
        func_import.name = "throw";
        func_import.type = *wasm_func_functype(thrower, func_index);
        func_import.type_module = thrower;
        func_import.type_index = thrower->funcs[func_index].type_idx;
        func_import.has_type_context = 1;
        func_import.func = wasm_test_forward_call;
        func_import.userdata = &forward;
        WASM_CHECK_OK(t, wasm__register_import_internal(&rt, &func_import, 0));

        memset(&tag_import, 0, sizeof(tag_import));
        tag_import.module = "t";
        tag_import.name = "e";
        tag_import.type = *wasm_tag_functype(thrower, tag_index);
        tag_import.type_module = thrower;
        tag_import.type_index = thrower->tags[tag_index].type_idx;
        tag_import.has_type_context = 1;
        tag_import.identity = thrower->tags[tag_index].identity;
        WASM_CHECK_OK(t, wasm_register_tag_import(&rt, &tag_import));

        catcher = wasm_load(&rt, catcher_bytes, sizeof(catcher_bytes));
        CHECK_MSG(t, catcher != NULL, "%s", rt.error_msg);
    }
    for (i = 0; catcher && i < COUNTOF(cases); i++) {
        wasm_value_t result = wasm_i32(0);
        wasm_error_t err = wasm_call(catcher, cases[i].name, NULL, 0, &result, 1);

        CHECK_MSG(t, err == WASM_OK && result.of.i32 == cases[i].expected, "%s: expected %d, got err=%d result=%d (%s)",
                  cases[i].name, (int)cases[i].expected, (int)err, (int)result.of.i32, rt.error_msg);
    }
    wasm_module_free(catcher);
    wasm_module_free(thrower);
    wasm_destroy(&rt);
}

static const test_case exec_exceptions_cases[] = {
    { "exceptions: typed catch receives payload values", test_exception_try_catch_payload },
    { "exceptions: catch_all handles unmatched tags", test_exception_catch_all },
    { "exceptions: rethrow forwards to outer catches", test_exception_rethrow },
    { "exceptions: delegate forwards to outer handlers", test_exception_delegate },
    { "exceptions: imports that throw reach handlers on every call path", test_exceptions_from_imports_reach_handlers_on_all_call_paths },
};
const test_suite exec_exceptions_suite = { "exec.exceptions", exec_exceptions_cases, COUNTOF(exec_exceptions_cases) };

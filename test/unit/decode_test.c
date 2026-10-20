#include "support.h"

TEST(test_loader_reports_section_decode_errors) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 8);
        emit_section(&mod, 9, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected malformed element section to fail load");
    CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", "expected malformed decoder error code");
    CHECK_MSG(t, rt.error_msg[0] != '\0', "%s", "expected loader failure to populate error text");
    CHECK_MSG(t, strstr(rt.error_msg, "section 9 decode failed") != NULL,
              "%s", rt.error_msg[0] ? rt.error_msg : "missing decode failure context");
    wasm_destroy(&rt);
}

TEST(test_exported_ctor_startup) {
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
        emit_leb128_u32(&sec, 0);

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
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 11);
        emit(&sec, 0x0B);
        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 17);
        emit(&sec, '_');
        emit(&sec, '_');
        emit(&sec, 'w');
        emit(&sec, 'a');
        emit(&sec, 's');
        emit(&sec, 'm');
        emit(&sec, '_');
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'l');
        emit(&sec, 'l');
        emit(&sec, '_');
        emit(&sec, 'c');
        emit(&sec, 't');
        emit(&sec, 'o');
        emit(&sec, 'r');
        emit(&sec, 's');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
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
        emit_leb128_i32(&body, 77);
        emit(&body, 0x24);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
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

    err = wasm_call(m, "get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 77);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_feature_gate_exceptions_disabled) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 0);

    wasm_init_features(&rt, WASM_FEATURES_ALL & ~WASM_FEATURE_EXCEPTIONS);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected disabled exceptions feature to reject module load");
    CHECK_MSG(t, strstr(rt.error_msg, "disabled") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_validation_rejects_duplicate_export_names) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

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
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'd');
        emit(&sec, 'u');
        emit(&sec, 'p');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'd');
        emit(&sec, 'u');
        emit(&sec, 'p');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected duplicate exports to fail validation");
    CHECK_MSG(t, strstr(rt.error_msg, "duplicate export") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_validation_rejects_invalid_start_signature) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
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

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 8, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected invalid start signature to fail validation");
    CHECK_MSG(t, strstr(rt.error_msg, "start function") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_validation_rejects_code_body_count_mismatch) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

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
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected code/body count mismatch to fail validation");
    CHECK_MSG(t, strstr(rt.error_msg, "code section count") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_validation_rejects_data_count_mismatch) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 12, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 0xAA };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_bytes(&sec, data_bytes, 1);
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected data count mismatch to fail validation");
    CHECK_MSG(t, strstr(rt.error_msg, "data count section declares") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

static void wasm_test_build_locals_module(wasm_builder_t* mod, uint32_t local_count) {
    wasm_builder_t sec = { 0 };
    wasm_builder_t body = { 0 };

    emit_header(mod);
    emit_leb128_u32(&sec, 1);
    emit(&sec, 0x60);
    emit_leb128_u32(&sec, 0);
    emit_leb128_u32(&sec, 0);
    emit_section(mod, 1, sec.buf, sec.len);

    sec.len = 0;
    emit_leb128_u32(&sec, 1);
    emit_leb128_u32(&sec, 0);
    emit_section(mod, 3, sec.buf, sec.len);

    sec.len = 0;
    emit_leb128_u32(&body, 1);
    emit_leb128_u32(&body, local_count);
    emit(&body, 0x7F);
    emit(&body, 0x0B);
    emit_leb128_u32(&sec, 1);
    emit_leb128_u32(&sec, body.len);
    emit_bytes(&sec, body.buf, body.len);
    emit_section(mod, 10, sec.buf, sec.len);
}

TEST(test_load_limits_function_locals) {
    wasm_builder_t at_limit = { 0 };
    wasm_builder_t over_limit = { 0 };
    wasm_builder_t huge = { 0 };
    wasm_runtime_t rt;
    wasm_config_t config;
    wasm_module_t* m;

    wasm_test_build_locals_module(&at_limit, WASM_MAX_FUNCTION_LOCALS);
    wasm_test_build_locals_module(&over_limit, WASM_MAX_FUNCTION_LOCALS + 1u);
    wasm_test_build_locals_module(&huge, 400000000u);

    wasm_init(&rt);
    m = wasm_load(&rt, at_limit.buf, at_limit.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    wasm_module_free(m);
    m = wasm_load(&rt, over_limit.buf, over_limit.len);
    CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_MALFORMED && strstr(rt.error_msg, "too many locals"),
              "expected the locals limit to reject the module, got: %s", rt.error_msg);
    m = wasm_load(&rt, huge.buf, huge.len);
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "too many locals"),
              "expected a 400M-local function to be rejected, got: %s", rt.error_msg);
    wasm_destroy(&rt);

    wasm_config_default(&config);
    config.max_function_locals = 0;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, over_limit.buf, over_limit.len);
    CHECK_MSG(t, m != NULL, "expected max_function_locals = 0 to lift the limit: %s", rt.error_msg);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_decode_rejects_counts_beyond_section) {
    static const uint8_t type_count[] = { 0x01, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F };
    static const uint8_t rec_group_size[] = { 0x01, 0x07, 0x01, 0x4E, 0x80, 0x80, 0x80, 0x80, 0x04 };
    static const uint8_t param_count[] = { 0x01, 0x08, 0x01, 0x60, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00 };
    static const uint8_t supertype_count[] = { 0x01, 0x09, 0x01, 0x50, 0x81, 0x80, 0x80, 0x80, 0x04, 0x5F, 0x00 };
    static const uint8_t struct_field_count[] = { 0x01, 0x07, 0x01, 0x5F, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F };
    static const uint8_t func_count[] = { 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F };
    static const struct {
        const char* name;
        const uint8_t* section;
        size_t len;
    } cases[] = {
        { "type count", type_count, sizeof(type_count) },
        { "rec group size", rec_group_size, sizeof(rec_group_size) },
        { "param count", param_count, sizeof(param_count) },
        { "supertype count", supertype_count, sizeof(supertype_count) },
        { "struct field count", struct_field_count, sizeof(struct_field_count) },
        { "function count", func_count, sizeof(func_count) },
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        wasm_builder_t mod = { 0 };
        wasm_runtime_t rt;
        wasm_module_t* m;

        emit_header(&mod);
        emit_bytes(&mod, cases[i].section, (uint32_t)cases[i].len);

        wasm_init(&rt);
        m = wasm_load(&rt, mod.buf, mod.len);
        CHECK_MSG(t, m == NULL, "expected oversized %s to be rejected", cases[i].name);
        CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s: %s", cases[i].name, rt.error_msg);
        if (m != NULL) wasm_module_free(m);
        wasm_destroy(&rt);
    }
}

TEST(test_compact_imports_feature_gate) {
    static const uint8_t import_section[] = { 0x02, 0x06, 0x01, 0x01, 'a', 0x00, 0x7F, 0x00 };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);
    emit_bytes(&mod, import_section, sizeof(import_section));

    wasm_init_features(&rt, WASM_FEATURES_STANDARD);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected compact imports to be disabled");
    CHECK_MSG(t, strstr(rt.error_msg, "compact imports") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case decode_cases[] = {
    { "startup: exported __wasm_call_ctors runs on load", test_exported_ctor_startup },
    { "compact imports: rejected when disabled, empty groups accepted when enabled", test_compact_imports_feature_gate },
    { "decode: vector counts beyond the section are malformed", test_decode_rejects_counts_beyond_section },
    { "feature gate: disabled exceptions reject module load", test_feature_gate_exceptions_disabled },
    { "validation: duplicate export names are rejected at load", test_validation_rejects_duplicate_export_names },
    { "validation: invalid start signature is rejected at load", test_validation_rejects_invalid_start_signature },
    { "validation: code body count must match declared functions", test_validation_rejects_code_body_count_mismatch },
    { "validation: data count must match data segment count", test_validation_rejects_data_count_mismatch },
    { "validation: section decoder failures populate loader errors", test_loader_reports_section_decode_errors },
    { "load: function locals are limited by config", test_load_limits_function_locals },
};
const test_suite decode_suite = { "decode", decode_cases, COUNTOF(decode_cases) };

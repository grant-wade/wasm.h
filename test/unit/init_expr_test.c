#include "support.h"

#ifdef WASM_TEST_MODULAR
#include "../../src/values.h"
#endif

TEST(test_global_init_expr_global_get_import) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_global_import_t imp;
    wasm_value_t imported_seed = wasm_i32(33);
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
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 4);
        emit(&sec, 's');
        emit(&sec, 'e');
        emit(&sec, 'e');
        emit(&sec, 'd');
        emit(&sec, 0x03);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit_section(&mod, 2, sec.buf, sec.len);
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
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 8);
        emit(&sec, 's');
        emit(&sec, 'n');
        emit(&sec, 'a');
        emit(&sec, 'p');
        emit(&sec, 's');
        emit(&sec, 'h');
        emit(&sec, 'o');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "seed";
    imp.type = WASM_TYPE_I32;
    imp.is_mutable = 0;
    imp.value = &imported_seed;

    err = wasm_register_global_import(&rt, &imp);
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

    imported_seed.of.i32 = 99;
    err = wasm_call(m, "snapshot", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 33);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_global_init_expr_extended_i32_arithmetic) {
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
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 6);
        emit(&sec, 0x0B);

        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 3);
        emit(&sec, 0x6C);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 4);
        emit(&sec, 0x6A);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 1);
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
        WASM_CHECK_I32(t, result.of.i32, 22);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_global_init_expr_extended_i64_arithmetic) {
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
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x7E);
        emit(&sec, 0x00);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 5);
        emit(&sec, 0x0B);

        emit(&sec, 0x7E);
        emit(&sec, 0x00);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 4);
        emit(&sec, 0x7E);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 3);
        emit(&sec, 0x7C);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 1);
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
        WASM_CHECK_I64(t, result.of.i64, 23);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_init_expr_rejects_local_mutable_global_get) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x7F);
        emit(&sec, 0x01);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 6);
        emit(&sec, 0x0B);

        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected mutable local global.get rejection");
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_data_offset_from_imported_global) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_global_import_t imp;
    wasm_value_t stack_pointer = wasm_i32(8);
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
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 15);
        emit(&sec, '_');
        emit(&sec, '_');
        emit(&sec, 's');
        emit(&sec, 't');
        emit(&sec, 'a');
        emit(&sec, 'c');
        emit(&sec, 'k');
        emit(&sec, '_');
        emit(&sec, 'p');
        emit(&sec, 'o');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 'r');
        emit(&sec, 0x03);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit_section(&mod, 2, sec.buf, sec.len);
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
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 8);
        emit(&body, 0x28);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[4] = { 42, 0, 0, 0 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 4);
        emit_bytes(&sec, data_bytes, 4);
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "__stack_pointer";
    imp.type = WASM_TYPE_I32;
    imp.is_mutable = 0;
    imp.value = &stack_pointer;

    err = wasm_register_global_import(&rt, &imp);
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

    err = wasm_call(m, "get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 42);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_data_offset_extended_const_expr) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_global_import_t imp;
    wasm_value_t stack_pointer = wasm_i32(6);
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
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 15);
        emit(&sec, '_');
        emit(&sec, '_');
        emit(&sec, 's');
        emit(&sec, 't');
        emit(&sec, 'a');
        emit(&sec, 'c');
        emit(&sec, 'k');
        emit(&sec, '_');
        emit(&sec, 'p');
        emit(&sec, 'o');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 'r');
        emit(&sec, 0x03);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit_section(&mod, 2, sec.buf, sec.len);
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
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 12);
        emit(&body, 0x28);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[4] = { 55, 0, 0, 0 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 3);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 2);
        emit(&sec, 0x6C);
        emit(&sec, 0x6A);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 4);
        emit_bytes(&sec, data_bytes, 4);
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "__stack_pointer";
    imp.type = WASM_TYPE_I32;
    imp.is_mutable = 0;
    imp.value = &stack_pointer;

    err = wasm_register_global_import(&rt, &imp);
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

    err = wasm_call(m, "get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 55);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_element_offset_extended_const_expr) {
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
        emit_leb128_u32(&sec, 4);
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
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 1);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 1);
        emit(&sec, 0x6A);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 77);
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

TEST(test_memory64_active_data_segment_offset) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_memory_type(&sec, 1, 0, 1, 0);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 'z', 0 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 4);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        uint8_t* memory = wasm_memory_data(m, 0);

        CHECK_MSG(t, memory != NULL, "%s", "expected memory allocation");
        if (memory) {
            WASM_CHECK_I32(t, memory[4], 'z');
            WASM_CHECK_I32(t, memory[5], 0);
        }
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_reference_types_expr_element_segments) {
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
        emit_leb128_u32(&sec, 4);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 'u');
        emit(&sec, 's');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 3);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x70);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x0B);
        emit(&sec, 0xD0);
        emit(&sec, 0x70);
        emit(&sec, 0x0B);

        emit_leb128_u32(&sec, 7);
        emit(&sec, 0x70);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 4);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 11);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 29);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x25);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD1);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
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

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 30);
    }

    err = wasm_call(m, "reuse", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS,
              "%s", "expected dropped passive expr element segment to behave like an empty segment on table.init");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_reference_types_active_expr_element_segment) {
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
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
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
        emit(&body, 0x11);
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

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 77);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_const_expr_supports_gc_alloc_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_gc_struct_t* struct_init;
    wasm_gc_struct_t* struct_default;
    wasm_gc_array_t* array_init;
    wasm_gc_array_t* array_default;
    wasm_gc_array_t* array_fixed;
    wasm_value_t* struct_init_field;
    wasm_value_t* struct_default_field;
    wasm_value_t* array_init_data;
    wasm_value_t* array_default_data;
    wasm_value_t* array_fixed_data;
    size_t struct_field_offset;
    size_t array_data_offset;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x5E);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 6);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 7);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x01);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 9);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 3);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x06);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x0B);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 4);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x07);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x0B);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 1);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 2);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x08);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x0B);

        emit(&sec, 0x6C);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, -1);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x1C);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, m->num_globals == 6, "expected 6 globals, got %u", (unsigned)m->num_globals);

    struct_init = (wasm_gc_struct_t*)m->globals[0].value.of.gc_obj;
    struct_default = (wasm_gc_struct_t*)m->globals[1].value.of.gc_obj;
    array_init = (wasm_gc_array_t*)m->globals[2].value.of.gc_obj;
    array_default = (wasm_gc_array_t*)m->globals[3].value.of.gc_obj;
    array_fixed = (wasm_gc_array_t*)m->globals[4].value.of.gc_obj;

    CHECK_MSG(t, m->globals[0].type == WASM_TYPE_STRUCTREF, "%s", "expected struct.new global to resolve to structref");
    CHECK_MSG(t, m->globals[1].type == WASM_TYPE_STRUCTREF, "%s", "expected struct.new_default global to resolve to structref");
    CHECK_MSG(t, m->globals[2].type == WASM_TYPE_ARRAYREF, "%s", "expected array.new global to resolve to arrayref");
    CHECK_MSG(t, m->globals[3].type == WASM_TYPE_ARRAYREF, "%s", "expected array.new_default global to resolve to arrayref");
    CHECK_MSG(t, m->globals[4].type == WASM_TYPE_ARRAYREF, "%s", "expected array.new_fixed global to resolve to arrayref");
    CHECK_MSG(t, m->globals[5].type == WASM_TYPE_I31REF, "%s", "expected ref.i31 global to resolve to i31ref");

    CHECK_MSG(t, !wasm__is_null_ref(&m->globals[0].value), "%s", "expected struct.new const expr to allocate an object");
    CHECK_MSG(t, !wasm__is_null_ref(&m->globals[1].value), "%s", "expected struct.new_default const expr to allocate an object");
    CHECK_MSG(t, !wasm__is_null_ref(&m->globals[2].value), "%s", "expected array.new const expr to allocate an object");
    CHECK_MSG(t, !wasm__is_null_ref(&m->globals[3].value), "%s", "expected array.new_default const expr to allocate an object");
    CHECK_MSG(t, !wasm__is_null_ref(&m->globals[4].value), "%s", "expected array.new_fixed const expr to allocate an object");
    CHECK_MSG(t, !wasm__is_null_ref(&m->globals[5].value), "%s", "expected ref.i31 const expr to produce a non-null tagged ref");

    CHECK_MSG(t, struct_init != NULL && struct_init->header.kind == WASM_GC_OBJ_STRUCT,
              "%s", "expected struct.new global to point at a struct object");
    CHECK_MSG(t, struct_default != NULL && struct_default->header.kind == WASM_GC_OBJ_STRUCT,
              "%s", "expected struct.new_default global to point at a struct object");
    CHECK_MSG(t, array_init != NULL && array_init->header.kind == WASM_GC_OBJ_ARRAY,
              "%s", "expected array.new global to point at an array object");
    CHECK_MSG(t, array_default != NULL && array_default->header.kind == WASM_GC_OBJ_ARRAY,
              "%s", "expected array.new_default global to point at an array object");
    CHECK_MSG(t, array_fixed != NULL && array_fixed->header.kind == WASM_GC_OBJ_ARRAY,
              "%s", "expected array.new_fixed global to point at an array object");

    struct_field_offset = m->types[0].of.struct_.field_offsets[0];
    array_data_offset = m->types[1].of.array.data_offset;
    struct_init_field = (wasm_value_t*)((uint8_t*)struct_init + struct_field_offset);
    struct_default_field = (wasm_value_t*)((uint8_t*)struct_default + struct_field_offset);
    array_init_data = (wasm_value_t*)((uint8_t*)array_init + array_data_offset);
    array_default_data = (wasm_value_t*)((uint8_t*)array_default + array_data_offset);
    array_fixed_data = (wasm_value_t*)((uint8_t*)array_fixed + array_data_offset);

    CHECK_MSG(t, struct_init_field->type == WASM_TYPE_I32 && struct_init_field->of.i32 == 7,
              "%s", "expected struct.new const expr to preserve field values");
    CHECK_MSG(t, struct_default_field->type == WASM_TYPE_I32 && struct_default_field->of.i32 == 0,
              "%s", "expected struct.new_default const expr to zero-initialize fields");

    CHECK_MSG(t, array_init->length == 3, "expected array.new length 3, got %u", (unsigned)array_init->length);
    CHECK_MSG(t, array_default->length == 4, "expected array.new_default length 4, got %u", (unsigned)array_default->length);
    CHECK_MSG(t, array_fixed->length == 2, "expected array.new_fixed length 2, got %u", (unsigned)array_fixed->length);
    CHECK_MSG(t, array_init_data[0].of.i32 == 9 && array_init_data[1].of.i32 == 9 && array_init_data[2].of.i32 == 9,
              "%s", "expected array.new const expr to fill with the initializer value");
    CHECK_MSG(t, array_default_data[0].of.i32 == 0 && array_default_data[3].of.i32 == 0,
              "%s", "expected array.new_default const expr to zero-initialize elements");
    CHECK_MSG(t, array_fixed_data[0].of.i32 == 1 && array_fixed_data[1].of.i32 == 2,
              "%s", "expected array.new_fixed const expr to preserve element order");

    CHECK_MSG(t, wasm_test_value_is_i31ref(&m->globals[5].value), "%s", "expected ref.i31 const expr to use the tagged i31 representation");
    WASM_CHECK_I32(t, wasm_test_gc_i31_get_s(m->globals[5].value.of.gc_ref), -1);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (rec (type $a (descriptor $b) (struct (field i32)))
        (type $b (describes $a) (descriptor $c) (struct (field i32)))
        (type $c (describes $b) (struct (field i32))))
   (global $c (ref (exact $c)) (struct.new $c (i32.const 3)))
   (global $b (ref (exact $b)) (struct.new_desc $b (i32.const 2) (global.get $c)))
   (global $a (ref (exact $a)) (struct.new_default_desc $a (global.get $b)))
   (table $t 1 (ref null $a)
     (struct.new_desc $a (i32.const 9) (struct.new_desc $b (i32.const 8) (struct.new $c (i32.const 7)))))
   chain and nested return a + 10 * b + 100 * c for the fields along the descriptor chain of $a and of
   (table.get $t 0). */
static const uint8_t wasm_test_init_desc_chain_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x21, 0x03, 0x4E, 0x03, 0x4D, 0x01, 0x5F,
    0x01, 0x7F, 0x00, 0x4C, 0x00, 0x4D, 0x02, 0x5F, 0x01, 0x7F, 0x00, 0x4C, 0x01, 0x5F, 0x01, 0x7F,
    0x00, 0x60, 0x01, 0x63, 0x00, 0x01, 0x7F, 0x60, 0x00, 0x01, 0x7F, 0x03, 0x04, 0x03, 0x03, 0x04,
    0x04, 0x04, 0x17, 0x01, 0x40, 0x00, 0x63, 0x00, 0x00, 0x01, 0x41, 0x09, 0x41, 0x08, 0x41, 0x07,
    0xFB, 0x00, 0x02, 0xFB, 0x20, 0x01, 0xFB, 0x20, 0x00, 0x0B, 0x06, 0x21, 0x03, 0x64, 0x62, 0x02,
    0x00, 0x41, 0x03, 0xFB, 0x00, 0x02, 0x0B, 0x64, 0x62, 0x01, 0x00, 0x41, 0x02, 0x23, 0x00, 0xFB,
    0x20, 0x01, 0x0B, 0x64, 0x62, 0x00, 0x00, 0x23, 0x01, 0xFB, 0x21, 0x00, 0x0B, 0x07, 0x12, 0x02,
    0x05, 0x63, 0x68, 0x61, 0x69, 0x6E, 0x00, 0x01, 0x06, 0x6E, 0x65, 0x73, 0x74, 0x65, 0x64, 0x00,
    0x02, 0x0A, 0x38, 0x03, 0x26, 0x00, 0x20, 0x00, 0xFB, 0x02, 0x00, 0x00, 0x41, 0x0A, 0x20, 0x00,
    0xFB, 0x22, 0x00, 0xFB, 0x02, 0x01, 0x00, 0x6C, 0x41, 0xE4, 0x00, 0x20, 0x00, 0xFB, 0x22, 0x00,
    0xFB, 0x22, 0x01, 0xFB, 0x02, 0x02, 0x00, 0x6C, 0x6A, 0x6A, 0x0B, 0x06, 0x00, 0x23, 0x02, 0x10,
    0x00, 0x0B, 0x08, 0x00, 0x41, 0x00, 0x25, 0x00, 0x10, 0x00, 0x0B,
};

/* The same $a and $b; (global (ref null $a) (struct.new_desc $a (i32.const 0) (ref.null none))). */
static const uint8_t wasm_test_init_desc_null_sections[] = {
    0x01, 0x0D, 0x01, 0x4E, 0x02, 0x4D, 0x01, 0x5F, 0x01, 0x7F, 0x00, 0x4C, 0x00, 0x5F, 0x00, 0x06,
    0x0C, 0x01, 0x63, 0x00, 0x00, 0x41, 0x00, 0xD0, 0x71, 0xFB, 0x20, 0x00, 0x0B,
};

/* The same $a and $b; (import "env" "vt" (global $vt (ref (exact $b))))
   (global (ref $a) (struct.new_desc $a (i32.const 1) (global.get $vt))). */
static const uint8_t wasm_test_init_desc_import_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0D, 0x01, 0x4E, 0x02, 0x4D, 0x01, 0x5F,
    0x01, 0x7F, 0x00, 0x4C, 0x00, 0x5F, 0x00, 0x02, 0x0D, 0x01, 0x03, 0x65, 0x6E, 0x76, 0x02, 0x76,
    0x74, 0x03, 0x64, 0x62, 0x01, 0x00, 0x06, 0x0C, 0x01, 0x64, 0x00, 0x00, 0x41, 0x01, 0x23, 0x00,
    0xFB, 0x20, 0x00, 0x0B,
};

TEST(test_const_expr_struct_new_desc) {
    wasm_builder_t null_mod = { 0 };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_module_t* compiled;
    wasm_value_t result;
    size_t heap_sizes[2];
    size_t i;

    heap_sizes[0] = (size_t)WASM_GC_HEAP_SIZE;
    heap_sizes[1] = 0;
    for (i = 0; i < COUNTOF(heap_sizes); i++) {
        wasm_config_default(&config);
        config.initial_gc_heap_size = heap_sizes[i];
        wasm_init_config(&rt, &config);
        m = wasm_load(&rt, wasm_test_init_desc_chain_bytes, sizeof(wasm_test_init_desc_chain_bytes));
        CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
        if (m != NULL) {
            WASM_CHECK_OK(t, wasm_call(m, "chain", NULL, 0, &result, 1));
            WASM_CHECK_I32(t, result.of.i32, 320);
            WASM_CHECK_OK(t, wasm_call(m, "nested", NULL, 0, &result, 1));
            WASM_CHECK_I32(t, result.of.i32, 789);
        }
        wasm_module_free(m);
        wasm_destroy(&rt);
    }

    wasm_init(&rt);
    compiled = wasm_compile(&rt, wasm_test_init_desc_chain_bytes, sizeof(wasm_test_init_desc_chain_bytes));
    CHECK_MSG(t, compiled != NULL, "%s", rt.error_msg);
    m = compiled ? wasm_instantiate(&rt, compiled, NULL) : NULL;
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        WASM_CHECK_OK(t, wasm_call(m, "nested", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 789);
    }
    wasm_module_free(m);
    wasm_module_free(compiled);

    m = wasm_test_load_sections(&rt, wasm_test_init_desc_null_sections, sizeof(wasm_test_init_desc_null_sections));
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "null descriptor reference") != NULL, "%s", rt.error_msg);
    wasm_module_free(m);
    emit_header(&null_mod);
    emit_bytes(&null_mod, wasm_test_init_desc_null_sections, sizeof(wasm_test_init_desc_null_sections));
    compiled = wasm_compile(&rt, null_mod.buf, null_mod.len);
    CHECK_MSG(t, compiled != NULL, "%s", rt.error_msg);
    m = compiled ? wasm_instantiate(&rt, compiled, NULL) : NULL;
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "null descriptor reference") != NULL, "%s", rt.error_msg);
    wasm_module_free(m);
    wasm_module_free(compiled);

    compiled = wasm_compile(&rt, wasm_test_init_desc_import_bytes, sizeof(wasm_test_init_desc_import_bytes));
    CHECK_MSG(t, compiled != NULL, "%s", rt.error_msg);
    wasm_module_free(compiled);
    wasm_destroy(&rt);
}

/* (type $x (struct (field i64))) (type $s (struct (field i32))) (table (export "t") 2 (ref null $s)) */
static const uint8_t wasm_test_init_expr_c1_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x5F, 0x01, 0x7E, 0x00, 0x5F,
    0x01, 0x7F, 0x00, 0x04, 0x05, 0x01, 0x63, 0x01, 0x00, 0x02, 0x07, 0x05, 0x01, 0x01, 0x74, 0x01,
    0x00,
};

/* (type $s (struct (field i32))) (import "c1" "t" (table 2 (ref null $s)))
   (elem (table 0) (i32.const 1) (ref null $s) (struct.new $s (i32.const 9))) */
static const uint8_t wasm_test_init_expr_imported_elem_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x5F, 0x01, 0x7F, 0x00, 0x02,
    0x0B, 0x01, 0x02, 0x63, 0x31, 0x01, 0x74, 0x01, 0x63, 0x00, 0x00, 0x02, 0x09, 0x0F, 0x01, 0x06,
    0x00, 0x41, 0x01, 0x0B, 0x63, 0x00, 0x01, 0x41, 0x09, 0xFB, 0x00, 0x00, 0x0B,
};

TEST(test_active_elem_segment_into_imported_table) {
    wasm_runtime_t rt;
    wasm_module_t* c1;
    wasm_module_t* m = NULL;
    wasm_value_t value;

    wasm_init(&rt);
    c1 = wasm_load(&rt, wasm_test_init_expr_c1_bytes, sizeof(wasm_test_init_expr_c1_bytes));
    CHECK_MSG(t, c1 != NULL, "%s", rt.error_msg);
    if (c1) {
        WASM_CHECK_OK(t, wasm_register_module(&rt, "c1", c1));
        m = wasm_load(&rt, wasm_test_init_expr_imported_elem_bytes, sizeof(wasm_test_init_expr_imported_elem_bytes));
        CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    }
    if (m) {
        WASM_CHECK_OK(t, wasm_table_get(c1, 0, 0, &value));
        CHECK(t, value.of.gc_ref == 0);
        WASM_CHECK_OK(t, wasm_table_get(m, 0, 1, &value));
        WASM_CHECK_OK(t, wasm_struct_get_field(c1, value, 0, &value));
        WASM_CHECK_I32(t, value.of.i32, 9);
        wasm_module_free(m);
    }
    if (c1) wasm_module_free(c1);
    wasm_destroy(&rt);
}

static const test_case init_expr_cases[] = {
    { "global init expr: global.get can read imported globals", test_global_init_expr_global_get_import },
    { "global init expr: i32 add/mul runs in initializer", test_global_init_expr_extended_i32_arithmetic },
    { "global init expr: i64 add/mul runs in initializer", test_global_init_expr_extended_i64_arithmetic },
    { "global init expr: local mutable globals stay invalid", test_init_expr_rejects_local_mutable_global_get },
    { "data offset expr: global.get can use imported __stack_pointer", test_data_offset_from_imported_global },
    { "data offset expr: add/mul runs in initializer", test_data_offset_extended_const_expr },
    { "element offset expr: add runs before active table init", test_element_offset_extended_const_expr },
    { "memory64: active data offsets accept i64 init exprs", test_memory64_active_data_segment_offset },
    { "reference types: expression-based passive/declarative elements", test_reference_types_expr_element_segments },
    { "reference types: active expr element segments with implicit funcref load", test_reference_types_active_expr_element_segment },
    { "wasmgc: const expressions support GC allocation ops", test_gc_const_expr_supports_gc_alloc_ops },
    { "custom descriptors: const expressions allocate described structs", test_const_expr_struct_new_desc },
    { "wasmgc: active element segments fill imported tables of concrete reference types",
      test_active_elem_segment_into_imported_table },
};
const test_suite init_expr_suite = { "init_expr", init_expr_cases, COUNTOF(init_expr_cases) };

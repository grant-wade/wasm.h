#include "support.h"

TEST(test_memory) {
    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    /* Type: () -> (i32) */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Memory: 1 page min */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 memory */
        emit(&sec, 0x00);         /* limits: no max */
        emit_leb128_u32(&sec, 1); /* 1 page initial */
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    /* Export */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 's');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code: i32.const 0; i32.const 99; i32.store; i32.const 0; i32.load */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0);

        /* store 99 at address 0 */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0); /* i32.const 0 (addr) */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 99); /* i32.const 99 (value) */
        emit(&body, 0x36);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0); /* i32.store align=2 offset=0 */

        /* load from address 0 */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0); /* i32.const 0 */
        emit(&body, 0x28);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0); /* i32.load align=2 offset=0 */

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "test", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 99);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_multi_memory_indexed_access) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint8_t* memory0;
    uint8_t* memory1;

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
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
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

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 11);
        emit(&body, 0x36);
        emit_memarg(&body, 2, 1, 0, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 31);
        emit(&body, 0x36);
        emit_memarg(&body, 2, 1, 1, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x28);
        emit_memarg(&body, 2, 1, 0, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x28);
        emit_memarg(&body, 2, 1, 1, 0);

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

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 42);
    }

    WASM_CHECK_I32(t, (int32_t)wasm_memory_count(m), 2);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 0), (int32_t)WASM_PAGE_SIZE);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 1), (int32_t)WASM_PAGE_SIZE);
    CHECK_MSG(t, wasm_memory_data(m, 2) == NULL, "%s", "expected only two memories");

    memory0 = wasm_memory_data(m, 0);
    memory1 = wasm_memory_data(m, 1);
    CHECK_MSG(t, memory0 != NULL, "%s", "expected memory 0 allocation");
    CHECK_MSG(t, memory1 != NULL, "%s", "expected memory 1 allocation");
    if (memory0 && memory1) {
        WASM_CHECK_I32(t, memory0[0], 11);
        WASM_CHECK_I32(t, memory1[0], 31);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_public_memory_helpers) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    uint8_t bytes[4] = { 0, 0, 0, 0 };
    static const uint8_t tail_bytes[2] = { 'x', 'y' };
    char buffer[8];

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_memory_write(m, 0, 4, "abc", 4);
    WASM_CHECK_OK(t, err);

    err = wasm_memory_read(m, 0, 4, bytes, sizeof(bytes));
    WASM_CHECK_OK(t, err);
    CHECK_MSG(t, memcmp(bytes, "abc", sizeof(bytes)) == 0,
              "%s", "expected memory_read to copy the written bytes");

    memset(buffer, '?', sizeof(buffer));
    err = wasm_memory_get_string(m, 0, 4, buffer, sizeof(buffer));
    WASM_CHECK_OK(t, err);
    CHECK_MSG(t, strcmp(buffer, "abc") == 0,
              "%s", "expected memory_get_string to read a NUL-terminated string");

    err = wasm_memory_write(m, 0, WASM_PAGE_SIZE - 2u, tail_bytes, sizeof(tail_bytes));
    WASM_CHECK_OK(t, err);

    memset(buffer, '?', sizeof(buffer));
    err = wasm_memory_get_string(m, 0, WASM_PAGE_SIZE - 2u, buffer, sizeof(buffer));
    WASM_CHECK_OK(t, err);
    CHECK_MSG(t, strcmp(buffer, "xy") == 0,
              "%s", "expected memory_get_string to stop at the end of memory and append NUL");

    err = wasm_memory_read(m, 0, WASM_PAGE_SIZE - 1u, bytes, sizeof(bytes));
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected out-of-bounds read rejection");

    err = wasm_memory_write(m, 1, 0, bytes, 1);
    CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected invalid memory index rejection");

    err = wasm_memory_get_string(m, 0, 0, buffer, 0);
    CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected zero-length string buffer rejection");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_multi_memory_bulk_ops_and_growth) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    int64_t old_pages;
    uint8_t* memory0;
    uint8_t* memory1;

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
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 12, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0A);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 6);
        emit(&body, 0x41);
        emit_leb128_i32(&body, '!');
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x3F);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x3F);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x40);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x2D);
        emit_memarg(&body, 0, 1, 0, 0);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x2D);
        emit_memarg(&body, 0, 1, 0, 0);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 6);
        emit(&body, 0x2D);
        emit_memarg(&body, 0, 1, 1, 0);
        emit(&body, 0x6A);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 'a', 'b' };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
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
        WASM_CHECK_I32(t, result.of.i32, 231);
    }

    memory0 = wasm_memory_data(m, 0);
    memory1 = wasm_memory_data(m, 1);
    CHECK_MSG(t, memory0 != NULL, "%s", "expected memory 0 allocation");
    CHECK_MSG(t, memory1 != NULL, "%s", "expected memory 1 allocation");
    if (memory0 && memory1) {
        WASM_CHECK_I32(t, memory0[4], 'a');
        WASM_CHECK_I32(t, memory0[5], 'b');
        WASM_CHECK_I32(t, memory1[0], 'a');
        WASM_CHECK_I32(t, memory1[1], 'b');
        WASM_CHECK_I32(t, memory1[6], '!');
    }
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 1), (int32_t)(2 * WASM_PAGE_SIZE));

    old_pages = wasm_memory_grow(m, 0, 1);
    WASM_CHECK_I64(t, old_pages, 1);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 0), (int32_t)(2 * WASM_PAGE_SIZE));

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_memory64_basic_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    int64_t old_pages;
    uint8_t bytes[8] = { 0 };

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
        emit_memory_type(&sec, 1, 1, 1, 3);
        emit_section(&mod, 5, sec.buf, sec.len);
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

        emit(&body, 0x42);
        emit_leb128_i64(&body, 8);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 42);
        emit(&body, 0x37);
        emit_memarg(&body, 3, 0, 0, 0);

        emit(&body, 0x3F);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x42);
        emit_leb128_i64(&body, 8);
        emit(&body, 0x29);
        emit_memarg(&body, 3, 0, 0, 0);
        emit(&body, 0x7C);

        emit(&body, 0x42);
        emit_leb128_i64(&body, 1);
        emit(&body, 0x40);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x7C);
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
    if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, 44);

    WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), (int64_t)(2u * WASM_PAGE_SIZE));

    err = wasm_memory_read(m, 0, 8, bytes, sizeof(bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, bytes[0], 42);
        WASM_CHECK_I32(t, bytes[1], 0);
    }

    old_pages = wasm_memory_grow(m, 0, 1);
    WASM_CHECK_I64(t, old_pages, 2);
    WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), (int64_t)(3u * WASM_PAGE_SIZE));

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_public_global_helpers) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t value;
    wasm_error_t err;

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

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x7F);
        emit(&sec, 0x01);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 7);
        emit(&sec, 0x0B);

        emit(&sec, 0x7E);
        emit(&sec, 0x00);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 99);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);
        emit_export_func(&sec, "run", 0);
        emit_export_kind(&sec, "counter", 0x03, 0);
        emit_export_kind(&sec, "limit", 0x03, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
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
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_global_get(m, "counter", &value);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, value.type == WASM_TYPE_I32, "%s", "expected counter to be an i32 global");
        WASM_CHECK_I32(t, value.of.i32, 7);
    }

    err = wasm_global_set(m, "counter", wasm_i32(11));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        err = wasm_global_get(m, "counter", &value);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            WASM_CHECK_I32(t, value.of.i32, 11);
            WASM_CHECK_I32(t, m->globals[0].value.of.i32, 11);
        }
    }

    err = wasm_global_set(m, "counter", wasm_i64(11));
    CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected type mismatch for wrong global value type");

    err = wasm_global_set(m, "limit", wasm_i64(100));
    CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected immutable global write rejection");

    err = wasm_global_get(m, "run", &value);
    CHECK_MSG(t, err == WASM_ERR_UNDEFINED_EXPORT, "%s", "expected non-global export lookup to fail");

    err = wasm_global_get(m, "missing", &value);
    CHECK_MSG(t, err == WASM_ERR_UNDEFINED_EXPORT, "%s", "expected missing global export lookup to fail");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_memory_grow_respects_zero_max) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    int64_t old_pages;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    old_pages = wasm_memory_grow(m, 0, 0);
    WASM_CHECK_I64(t, old_pages, 0);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 0), 0);

    old_pages = wasm_memory_grow(m, 0, 1);
    WASM_CHECK_I64(t, old_pages, -1);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 0), 0);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_custom_page_sizes_feature_gate) {
    static const uint8_t memory_section[] = { 0x05, 0x04, 0x01, 0x08, 0x03, 0x00 };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);
    emit_bytes(&mod, memory_section, sizeof(memory_section));

    wasm_init_features(&rt, WASM_FEATURES_STANDARD);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected custom page sizes to be disabled");
    CHECK_MSG(t, strstr(rt.error_msg, "custom page sizes") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        int64_t old_pages;
        WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), 3);
        old_pages = wasm_memory_grow(m, 0, 5);
        WASM_CHECK_I64(t, old_pages, 3);
        WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), 8);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

/* (type $big (struct (field (mut i64)) (field (mut i64)) (field (mut i64)) (field (mut i64))))
   (table (export "t") 1 (ref null $big))
   (func (export "read") (param i32) (result i64) (struct.get $big 3 (table.get 0 (local.get 0)))) */
static const uint8_t wasm_test_table_exporter_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x10, 0x02, 0x5F, 0x04, 0x7E, 0x01, 0x7E,
    0x01, 0x7E, 0x01, 0x7E, 0x01, 0x60, 0x01, 0x7F, 0x01, 0x7E, 0x03, 0x02, 0x01, 0x01, 0x04, 0x05,
    0x01, 0x63, 0x00, 0x00, 0x01, 0x07, 0x0C, 0x02, 0x01, 0x74, 0x01, 0x00, 0x04, 0x72, 0x65, 0x61,
    0x64, 0x00, 0x00, 0x0A, 0x0C, 0x01, 0x0A, 0x00, 0x20, 0x00, 0x25, 0x00, 0xFB, 0x02, 0x00, 0x03,
    0x0B,
};

/* (type $s (struct (field (mut i32)))) (type $big ...as in the exporter...)
   (import "a" "t" (table 1 (ref null $big))) (import "host" "any" (table 1 anyref)) */
static const uint8_t wasm_test_table_importer_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0F, 0x02, 0x5F, 0x01, 0x7F, 0x01, 0x5F,
    0x04, 0x7E, 0x01, 0x7E, 0x01, 0x7E, 0x01, 0x7E, 0x01, 0x02, 0x17, 0x02, 0x01, 0x61, 0x01, 0x74,
    0x01, 0x63, 0x01, 0x00, 0x01, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x03, 0x61, 0x6E, 0x79, 0x01, 0x6E,
    0x00, 0x01,
};

static int64_t wasm_test_table_read(test_ctx* t, wasm_module_t* a, int32_t index) {
    wasm_value_t arg = wasm_i32(index);
    wasm_value_t result = wasm_i64(-1);

    WASM_CHECK_OK(t, wasm_call(a, "read", &arg, 1, &result, 1));
    return result.of.i64;
}

static wasm_value_t* wasm_test_table_struct(test_ctx* t, wasm_module_t* mod, uint32_t type_idx, int64_t first,
                                            uint32_t num_fields) {
    wasm_value_t fields[4];
    wasm_value_t value;
    wasm_error_t err;
    uint32_t i;

    for (i = 0; i < num_fields; i++) fields[i] = num_fields == 1 ? wasm_i32((int32_t)first) : wasm_i64(first + i);
    err = wasm_struct_new(mod, type_idx, fields, num_fields, &value);
    WASM_CHECK_OK(t, err);
    return err == WASM_OK ? wasm_ref_hold(wasm_module_runtime(mod), value) : NULL;
}

static void wasm_test_imported_table_api(test_ctx* t, wasm_module_t* a, wasm_module_t* b) {
    wasm_runtime_t* rt = wasm_module_runtime(a);
    const wasm_reftype_t* ref;
    wasm_value_t* other = wasm_test_table_struct(t, b, 0, 7, 1);
    wasm_value_t* big_b = wasm_test_table_struct(t, b, 1, 1, 4);
    wasm_value_t* big_a = wasm_test_table_struct(t, a, 0, 5, 4);
    wasm_value_t value;

    CHECK(t, wasm_table_type(a, 0) == WASM_TYPE_STRUCTREF && wasm_table_type(b, 0) == WASM_TYPE_STRUCTREF);
    ref = wasm_table_reftype(a, 0);
    CHECK(t, ref && ref->has_type_index && ref->type_index == 0 && ref->nullable);
    ref = wasm_table_reftype(b, 0);
    CHECK(t, ref && ref->has_type_index && ref->type_index == 1 && ref->nullable);
    ref = wasm_table_reftype(b, 1);
    CHECK(t, wasm_table_type(b, 1) == WASM_TYPE_ANYREF && ref && !ref->has_type_index && ref->nullable);
    CHECK(t, wasm_table_type(b, 2) == WASM_TYPE_VOID && wasm_table_reftype(b, 2) == NULL);
    if (!other || !big_a || !big_b) {
        CHECK_MSG(t, 0, "%s", "could not create the test structs");
        wasm_ref_release(rt, other);
        wasm_ref_release(rt, big_a);
        wasm_ref_release(rt, big_b);
        return;
    }

    CHECK(t, wasm_table_set(b, 0, 0, *other) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_table_set(a, 0, 0, *other) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_table_grow(b, 0, 1, *other) == -1);
    CHECK(t, wasm_table_grow(a, 0, 1, *other) == -1);
    CHECK(t, wasm_table_size(a, 0) == 1u && wasm_table_size(b, 0) == 1u);
    WASM_CHECK_OK(t, wasm_table_get(b, 0, 0, &value));
    CHECK(t, value.of.gc_ref == 0);

    WASM_CHECK_OK(t, wasm_table_set(b, 0, 0, *big_b));
    CHECK(t, wasm_test_table_read(t, a, 0) == 4);
    WASM_CHECK_OK(t, wasm_table_set(a, 0, 0, *big_a));
    CHECK(t, wasm_test_table_read(t, a, 0) == 8);
    CHECK(t, wasm_table_grow(b, 0, 1, *big_a) == 1);
    CHECK(t, wasm_table_grow(a, 0, 1, *big_b) == 2);
    CHECK(t, wasm_table_size(b, 0) == 3u);

    WASM_CHECK_OK(t, wasm_table_set(b, 1, 0, *other));
    CHECK(t, wasm_table_set(b, 1, 0, wasm_i32(0)) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_table_grow(b, 1, 1, *big_a) == 1);

    wasm_gc_collect(rt);
    CHECK(t, wasm_test_table_read(t, a, 1) == 8);
    CHECK(t, wasm_test_table_read(t, a, 2) == 4);
    WASM_CHECK_OK(t, wasm_table_get(b, 1, 0, &value));
    WASM_CHECK_OK(t, wasm_struct_get_field(b, value, 0, &value));
    WASM_CHECK_I32(t, value.of.i32, 7);

    wasm_ref_release(rt, other);
    wasm_ref_release(rt, big_a);
    wasm_ref_release(rt, big_b);
}

TEST(test_imported_table_api_uses_importer_types) {
    wasm_runtime_t rt;
    wasm_module_t* a;
    wasm_module_t* b = NULL;
    wasm_table_t* host_table;
    wasm_table_import_t host_import;

    wasm_init(&rt);
    host_table = wasm_table_new(&wasm_test_allocator, WASM_TYPE_ANYREF, 1, UINT32_MAX);
    CHECK(t, host_table != NULL);
    memset(&host_import, 0, sizeof(host_import));
    host_import.module = "host";
    host_import.name = "any";
    host_import.table = host_table;
    WASM_CHECK_OK(t, wasm_register_table_import(&rt, &host_import));
    a = wasm_load(&rt, wasm_test_table_exporter_bytes, sizeof(wasm_test_table_exporter_bytes));
    CHECK_MSG(t, a != NULL, "%s", rt.error_msg);
    if (a) {
        WASM_CHECK_OK(t, wasm_register_module(&rt, "a", a));
        b = wasm_load(&rt, wasm_test_table_importer_bytes, sizeof(wasm_test_table_importer_bytes));
        CHECK_MSG(t, b != NULL, "%s", rt.error_msg);
    }
    if (a && b) wasm_test_imported_table_api(t, a, b);
    if (b) wasm_module_free(b);
    if (a) wasm_module_free(a);
    wasm_table_free(host_table);
    wasm_destroy(&rt);
}

static const test_case memory_cases[] = {
    { "memory: store 99 at offset 0, load it back", test_memory },
    { "public api: safe memory read/write/string helpers", test_public_memory_helpers },
    { "multi-memory: indexed load/store and memory APIs", test_multi_memory_indexed_access },
    { "multi-memory: indexed bulk ops plus size/grow", test_multi_memory_bulk_ops_and_growth },
    { "memory64: i64 load/store plus memory.size/memory.grow", test_memory64_basic_ops },
    { "custom page sizes: rejected when disabled, byte-sized pages when enabled", test_custom_page_sizes_feature_gate },
    { "memory: zero max blocks later growth", test_memory_grow_respects_zero_max },
    { "public api: exported globals can be read and updated safely", test_public_global_helpers },
    { "public api: imported table types, set, and grow use the importer's type indices",
      test_imported_table_api_uses_importer_types },
};
const test_suite memory_suite = { "memory", memory_cases, COUNTOF(memory_cases) };

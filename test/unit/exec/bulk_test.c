#include "../support.h"

TEST(test_bulk_memory_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint8_t* memory;

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

    /* Functions: run, reuse */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Memory: 1 page min */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    /* Exports */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 'u');
        emit(&sec, 's');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 12, sec.buf, sec.len);
    }

    /* Code */
    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t run_body = { 0 };
        wasm_builder_t reuse_body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 0);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 0);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 5);
        emit(&run_body, 0xFC);
        emit_leb128_u32(&run_body, 0x08);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 0);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 5);
        emit(&run_body, 0xFC);
        emit_leb128_u32(&run_body, 0x0A);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 5);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, '!');
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0xFC);
        emit_leb128_u32(&run_body, 0x0B);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0xFC);
        emit_leb128_u32(&run_body, 0x09);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0x2D);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 4);
        emit(&run_body, 0x2D);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);
        emit(&run_body, 0x6A);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 5);
        emit(&run_body, 0x2D);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);
        emit(&run_body, 0x6A);
        emit(&run_body, 0x0B);

        emit_leb128_u32(&sec, run_body.len);
        emit_bytes(&sec, run_body.buf, run_body.len);

        emit_leb128_u32(&reuse_body, 0);
        emit(&reuse_body, 0x41);
        emit_leb128_i32(&reuse_body, 0);
        emit(&reuse_body, 0x41);
        emit_leb128_i32(&reuse_body, 0);
        emit(&reuse_body, 0x41);
        emit_leb128_i32(&reuse_body, 1);
        emit(&reuse_body, 0xFC);
        emit_leb128_u32(&reuse_body, 0x08);
        emit_leb128_u32(&reuse_body, 0);
        emit_leb128_u32(&reuse_body, 0);
        emit(&reuse_body, 0x41);
        emit_leb128_i32(&reuse_body, 0);
        emit(&reuse_body, 0x0B);

        emit_leb128_u32(&sec, reuse_body.len);
        emit_bytes(&sec, reuse_body.buf, reuse_body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    /* Data: one passive segment containing "ABCDE" */
    {
        static const uint8_t data_bytes[] = { 'A', 'B', 'C', 'D', 'E' };
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
        WASM_CHECK_I32(t, result.of.i32, 166);

        memory = wasm_memory_data(m, 0);
        CHECK_MSG(t, memory != NULL, "%s", "expected linear memory allocation");
        if (memory) {
            WASM_CHECK_I32(t, memory[0], 'A');
            WASM_CHECK_I32(t, memory[1], 'A');
            WASM_CHECK_I32(t, memory[4], 'D');
            WASM_CHECK_I32(t, memory[5], '!');
        }
    }

    err = wasm_call(m, "reuse", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS,
              "%s", "expected dropped passive segment to behave like an empty segment on memory.init");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_bulk_memory_table_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

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

    /* Functions: eleven, twenty_nine, run, reuse */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Table: 4 funcref slots */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 4);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    /* Exports */
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

    /* Element segments:
     *   0: active, implicit table 0, offset 0 => [func 0]
     *   1: active, explicit table 0, offset 3 => [func 1]
     *   2: passive => [func 1, func 0]
     *   3: declarative => [func 0]
     */
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 3);
        emit(&sec, 0x0B);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 9, sec.buf, sec.len);
    }

    /* Code */
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
        emit_leb128_i32(&body, 0);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0E);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 2);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 3);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);
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
        emit_leb128_u32(&body, 2);
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
        WASM_CHECK_I32(t, result.of.i32, 80);
    }

    err = wasm_call(m, "reuse", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS,
              "%s", "expected dropped passive element segment to behave like an empty segment on table.init");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_bulk_memory_oob_traps) {
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
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 8);
        emit(&sec, 'b');
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, '_');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 'i');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 8);
        emit(&sec, 'b');
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, '_');
        emit(&sec, 'c');
        emit(&sec, 'o');
        emit(&sec, 'p');
        emit(&sec, 'y');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

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

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 65535);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 65535);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0A);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 'X', 'Y' };
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

    err = wasm_call(m, "bad_init", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected memory.init out-of-bounds trap");

    err = wasm_call(m, "bad_copy", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected memory.copy out-of-bounds trap");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_bulk_memory_table_oob_traps) {
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
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 8);
        emit(&sec, 'b');
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, '_');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 'i');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 8);
        emit(&sec, 'b');
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, '_');
        emit(&sec, 'c');
        emit(&sec, 'o');
        emit(&sec, 'p');
        emit(&sec, 'y');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 3);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 4);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
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
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0E);
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

    err = wasm_call(m, "bad_init", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected table.init out-of-bounds trap");

    err = wasm_call(m, "bad_copy", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected table.copy out-of-bounds trap");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_reference_types_table_ops) {
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
        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x70);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 5);

        emit(&sec, 0x6F);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);

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
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x0B);

        emit_leb128_u32(&sec, 6);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit(&sec, 0x6F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0xD0);
        emit(&sec, 0x6F);
        emit(&sec, 0x0B);

        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 3);

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
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x7F);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xD0);
        emit(&body, 0x70);
        emit(&body, 0xD1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xD2);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x26);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xD2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0F);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xD2);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x11);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xD0);
        emit(&body, 0x6F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0F);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x10);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xD0);
        emit(&body, 0x6F);
        emit(&body, 0x26);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x25);
        emit_leb128_u32(&body, 1);
        emit(&body, 0xD1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
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
        WASM_CHECK_I32(t, result.of.i32, 83);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_bulk_fuel_and_array_new_rooting) {
    /* fill(n) runs memory.fill over n bytes; churn(n) builds n arrays of 64 copies of a new
     * struct holding the loop counter and sums each array's last element's field. A struct kept
     * alive across the loop makes every collection copy the heap and free the old one. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x12, 0x04, 0x5f, 0x01, 0x7f, 0x00, 0x5e, 0x63, 0x00,
        0x00, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x03, 0x03, 0x02, 0x02, 0x03, 0x05, 0x03, 0x01,
        0x00, 0x01, 0x07, 0x10, 0x02, 0x04, 0x66, 0x69, 0x6c, 0x6c, 0x00, 0x00, 0x05, 0x63, 0x68, 0x75, 0x72, 0x6e,
        0x00, 0x01, 0x0a, 0x4a, 0x02, 0x0b, 0x00, 0x41, 0x00, 0x41, 0x00, 0x20, 0x00, 0xfc, 0x0b, 0x00, 0x0b, 0x3c,
        0x02, 0x01, 0x7f, 0x01, 0x63, 0x00, 0x41, 0x00, 0xfb, 0x00, 0x00, 0x21, 0x02, 0x03, 0x40, 0x20, 0x01, 0x20,
        0x00, 0xfb, 0x00, 0x00, 0x41, 0xc0, 0x00, 0xfb, 0x06, 0x01, 0x41, 0x3f, 0xfb, 0x0b, 0x01, 0xfb, 0x02, 0x00,
        0x00, 0x6a, 0x21, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x22, 0x00, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0x20, 0x02,
        0xfb, 0x02, 0x00, 0x00, 0x6a, 0x0b,
    };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);

    wasm_config_default(&config);
    config.initial_gc_heap_size = 8192;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    wasm_runtime_set_fuel(&rt, 100);
    arg = wasm_i32(64 * 50);
    WASM_CHECK_OK(t, wasm_call(m, "fill", &arg, 1, NULL, 0));
    CHECK_MSG(t, wasm_runtime_fuel(&rt) < 50u, "expected 3200 bytes to cost 50 units, %u left",
              (unsigned)wasm_runtime_fuel(&rt));
    wasm_runtime_set_fuel(&rt, 100);
    arg = wasm_i32(65536);
    CHECK(t, wasm_call(m, "fill", &arg, 1, NULL, 0) == WASM_ERR_OUT_OF_FUEL);
    wasm_runtime_cancel(&rt);
    wasm_runtime_disable_fuel(&rt);

    /* A held struct survives the collections below and follows the object as it moves. */
    {
        wasm_value_t field = wasm_i32(99);
        wasm_value_t point;
        wasm_value_t original;
        wasm_value_t* held;

        WASM_CHECK_OK(t, wasm_struct_new(m, 0, &field, 1, &point));
        original = point;
        held = wasm_ref_hold(&rt, point);
        CHECK(t, held != NULL);

        /* Each array needs about 1.5 KB, so the heap is collected every few iterations. */
        arg = wasm_i32(200);
        WASM_CHECK_OK(t, wasm_call(m, "churn", &arg, 1, &result, 1));
        CHECK_MSG(t, result.of.i32 == 200 * 201 / 2, "expected each array to hold its struct, got %d",
                  (int)result.of.i32);

        if (held) {
            CHECK(t, held->of.gc_obj != original.of.gc_obj);
            WASM_CHECK_OK(t, wasm_struct_get_field(m, *held, 0, &field));
            CHECK(t, field.of.i32 == 99);
            wasm_ref_release(&rt, held);
        }
        wasm_ref_release(&rt, NULL);
        CHECK(t, wasm_ref_hold(NULL, point) == NULL);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_bulk_cases[] = {
    { "bulk memory: passive data, copy/fill/drop semantics", test_bulk_memory_ops },
    { "bulk memory: table init/copy/drop and element segment forms", test_bulk_memory_table_ops },
    { "bulk memory: memory.init/copy out-of-bounds traps", test_bulk_memory_oob_traps },
    { "bulk memory: table.init/copy out-of-bounds traps", test_bulk_memory_table_oob_traps },
    { "reference types: ref ops plus typed table get/set/grow/fill", test_reference_types_table_ops },
    { "gc: bulk instructions pay fuel for their size; array.new and held values stay rooted", test_bulk_fuel_and_array_new_rooting },
};
const test_suite exec_bulk_suite = { "exec.bulk", exec_bulk_cases, COUNTOF(exec_bulk_cases) };

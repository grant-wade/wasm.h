#include "../support.h"

TEST(test_gc_runtime_executes_struct_array_i31_and_conversion_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
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

        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 3);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 10);
        emit(&body, 0x41);
        emit_leb128_i32(&body, -1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x00);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x02);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x03);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x04);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 20);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x05);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x08);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0F);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0E);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x10);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x06);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 2);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x07);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 3);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 3);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, -5);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1C);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1D);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1C);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1E);
        emit(&body, 0x6A);

        emit(&body, 0xD0);
        emit(&body, 0x6F);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1A);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1B);
        emit(&body, 0xD1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD3);
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
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 286);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_runtime_executes_cast_and_branch_cast_opcodes) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
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

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x01);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x14);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x16);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD3);
        emit(&body, 0x6A);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x18);
        emit(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x14);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x19);
        emit(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x14);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0xD0);
        emit(&body, 0x6B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x15);
        emit(&body, 0x6B);
        emit(&body, 0x6A);

        emit(&body, 0xD0);
        emit(&body, 0x6B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x17);
        emit(&body, 0x6B);
        emit(&body, 0xD1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD4);
        emit(&body, 0xD1);
        emit(&body, 0x45);
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
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 7);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_runtime_executes_segment_backed_array_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x6B);
        emit(&sec, 0x01);

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

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x6B);
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0xD0);
        emit(&sec, 0x6B);
        emit(&sec, 0x0B);

        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x01);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit_section(&mod, 9, sec.buf, sec.len);
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

        emit_leb128_u32(&body, 3);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 2);

        emit(&body, 0x41);
        emit_leb128_i32(&body, -1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x00);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x04);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x09);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x11);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x12);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0A);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 2);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0F);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 2);
        emit(&body, 0xD1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x13);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 2);
        emit(&body, 0xD1);
        emit(&body, 0x6A);

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 0x80, 0x7F, 0x05 };
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
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 390);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (rec (type $obj (descriptor $vt) (struct (field i32))) (type $vt (describes $obj) (struct (field i32))))
   (global $a (ref (exact $vt)) (struct.new $vt (i32.const 1)))
   (global $b (ref (exact $vt)) (struct.new $vt (i32.const 1)))
   $value(v) is null for 0, else (struct.new_desc $obj (i32.const 5) $a); $desc(d) is null, $a, or $b for d = 0,
   1, 2; $id(r) is -1 for null, else field 0 of r.
   new(d) and new-default(d) read field 0 of the descriptor or object they allocate with $desc(d), get-null is
   ref.get_desc of null, cast(v, d) and cast-null(v, d) are $id of ref.cast_desc_eq (ref $obj) and (ref null $obj)
   of $value(v) and $desc(d), cast-any(x) casts the anyref x to (ref null $obj) with $a, br, br-nn, br-fail, and
   br-fail-nn branch with br_on_cast_desc_eq(_fail) on $value(v) and $desc(d) to (ref null $obj) or (ref $obj),
   returning $id of a cast result and 100 + ref.is_null of anything else. */
static const uint8_t wasm_test_exec_desc_sections[] = {
    0x01, 0x35, 0x08, 0x4E, 0x02, 0x4D, 0x01, 0x5F, 0x01, 0x7F, 0x00, 0x4C, 0x00, 0x5F, 0x01, 0x7F,
    0x00, 0x60, 0x01, 0x7F, 0x01, 0x6E, 0x60, 0x01, 0x7F, 0x01, 0x63, 0x62, 0x01, 0x60, 0x01, 0x63,
    0x00, 0x01, 0x7F, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x60, 0x00, 0x01, 0x7F, 0x60, 0x02, 0x7F, 0x7F,
    0x01, 0x7F, 0x60, 0x01, 0x6E, 0x01, 0x7F, 0x03, 0x0E, 0x0D, 0x02, 0x03, 0x04, 0x05, 0x05, 0x06,
    0x07, 0x07, 0x08, 0x07, 0x07, 0x07, 0x07, 0x06, 0x15, 0x02, 0x64, 0x62, 0x01, 0x00, 0x41, 0x01,
    0xFB, 0x00, 0x01, 0x0B, 0x64, 0x62, 0x01, 0x00, 0x41, 0x01, 0xFB, 0x00, 0x01, 0x0B, 0x07, 0x62,
    0x0A, 0x03, 0x6E, 0x65, 0x77, 0x00, 0x03, 0x0B, 0x6E, 0x65, 0x77, 0x2D, 0x64, 0x65, 0x66, 0x61,
    0x75, 0x6C, 0x74, 0x00, 0x04, 0x08, 0x67, 0x65, 0x74, 0x2D, 0x6E, 0x75, 0x6C, 0x6C, 0x00, 0x05,
    0x04, 0x63, 0x61, 0x73, 0x74, 0x00, 0x06, 0x09, 0x63, 0x61, 0x73, 0x74, 0x2D, 0x6E, 0x75, 0x6C,
    0x6C, 0x00, 0x07, 0x08, 0x63, 0x61, 0x73, 0x74, 0x2D, 0x61, 0x6E, 0x79, 0x00, 0x08, 0x02, 0x62,
    0x72, 0x00, 0x09, 0x05, 0x62, 0x72, 0x2D, 0x6E, 0x6E, 0x00, 0x0A, 0x07, 0x62, 0x72, 0x2D, 0x66,
    0x61, 0x69, 0x6C, 0x00, 0x0B, 0x0A, 0x62, 0x72, 0x2D, 0x66, 0x61, 0x69, 0x6C, 0x2D, 0x6E, 0x6E,
    0x00, 0x0C, 0x0A, 0x8B, 0x02, 0x0D, 0x11, 0x00, 0x20, 0x00, 0x04, 0x6E, 0x41, 0x05, 0x23, 0x00,
    0xFB, 0x20, 0x00, 0x05, 0xD0, 0x71, 0x0B, 0x0B, 0x1B, 0x00, 0x20, 0x00, 0x45, 0x04, 0x63, 0x62,
    0x01, 0xD0, 0x71, 0x05, 0x23, 0x00, 0x23, 0x01, 0x20, 0x00, 0x41, 0x01, 0x46, 0x1C, 0x01, 0x64,
    0x62, 0x01, 0x0B, 0x0B, 0x11, 0x00, 0x20, 0x00, 0xD1, 0x04, 0x7F, 0x41, 0x7F, 0x05, 0x20, 0x00,
    0xFB, 0x02, 0x00, 0x00, 0x0B, 0x0B, 0x12, 0x00, 0x41, 0x05, 0x20, 0x00, 0x10, 0x01, 0xFB, 0x20,
    0x00, 0xFB, 0x22, 0x00, 0xFB, 0x02, 0x01, 0x00, 0x0B, 0x0D, 0x00, 0x20, 0x00, 0x10, 0x01, 0xFB,
    0x21, 0x00, 0xFB, 0x02, 0x00, 0x00, 0x0B, 0x0A, 0x00, 0xD0, 0x71, 0xFB, 0x22, 0x00, 0x1A, 0x41,
    0x00, 0x0B, 0x0F, 0x00, 0x20, 0x00, 0x10, 0x00, 0x20, 0x01, 0x10, 0x01, 0xFB, 0x23, 0x00, 0x10,
    0x02, 0x0B, 0x0F, 0x00, 0x20, 0x00, 0x10, 0x00, 0x20, 0x01, 0x10, 0x01, 0xFB, 0x24, 0x00, 0x10,
    0x02, 0x0B, 0x0B, 0x00, 0x20, 0x00, 0x23, 0x00, 0xFB, 0x24, 0x00, 0x10, 0x02, 0x0B, 0x1C, 0x00,
    0x02, 0x63, 0x00, 0x20, 0x00, 0x10, 0x00, 0x20, 0x01, 0x10, 0x01, 0xFB, 0x25, 0x03, 0x00, 0x6E,
    0x00, 0xD1, 0x41, 0xE4, 0x00, 0x6A, 0x0F, 0x0B, 0x10, 0x02, 0x0B, 0x1C, 0x00, 0x02, 0x64, 0x00,
    0x20, 0x00, 0x10, 0x00, 0x20, 0x01, 0x10, 0x01, 0xFB, 0x25, 0x01, 0x00, 0x6E, 0x00, 0xD1, 0x41,
    0xE4, 0x00, 0x6A, 0x0F, 0x0B, 0x10, 0x02, 0x0B, 0x1B, 0x00, 0x02, 0x6E, 0x20, 0x00, 0x10, 0x00,
    0x20, 0x01, 0x10, 0x01, 0xFB, 0x26, 0x03, 0x00, 0x6E, 0x00, 0x10, 0x02, 0x0F, 0x0B, 0xD1, 0x41,
    0xE4, 0x00, 0x6A, 0x0B, 0x1B, 0x00, 0x02, 0x6E, 0x20, 0x00, 0x10, 0x00, 0x20, 0x01, 0x10, 0x01,
    0xFB, 0x26, 0x01, 0x00, 0x6E, 0x00, 0x10, 0x02, 0x0F, 0x0B, 0xD1, 0x41, 0xE4, 0x00, 0x6A, 0x0B,
};

typedef struct wasm_test_exec_desc_case_t {
    const char* name;
    int32_t args[2];
    uint32_t num_args;
    int32_t expected;
    const char* trap;
} wasm_test_exec_desc_case_t;

static void wasm_test_exec_desc_cases(test_ctx* t, size_t gc_heap_size) {
    static const wasm_test_exec_desc_case_t cases[] = {
        { "new", { 1 }, 1, 1, NULL },
        { "new", { 2 }, 1, 1, NULL },
        { "new", { 0 }, 1, 0, "null descriptor reference" },
        { "new-default", { 1 }, 1, 0, NULL },
        { "new-default", { 0 }, 1, 0, "null descriptor reference" },
        { "get-null", { 0 }, 0, 0, "null reference" },
        { "cast", { 1, 1 }, 2, 5, NULL },
        { "cast", { 1, 2 }, 2, 0, "descriptor cast failure" },
        { "cast", { 0, 1 }, 2, 0, "descriptor cast failure" },
        { "cast", { 1, 0 }, 2, 0, "null descriptor reference" },
        { "cast", { 0, 0 }, 2, 0, "null descriptor reference" },
        { "cast-null", { 1, 1 }, 2, 5, NULL },
        { "cast-null", { 0, 1 }, 2, -1, NULL },
        { "cast-null", { 1, 2 }, 2, 0, "descriptor cast failure" },
        { "cast-null", { 0, 0 }, 2, 0, "null descriptor reference" },
        { "br", { 1, 1 }, 2, 5, NULL },
        { "br", { 1, 2 }, 2, 100, NULL },
        { "br", { 0, 1 }, 2, -1, NULL },
        { "br", { 1, 0 }, 2, 0, "null descriptor reference" },
        { "br-nn", { 1, 1 }, 2, 5, NULL },
        { "br-nn", { 1, 2 }, 2, 100, NULL },
        { "br-nn", { 0, 1 }, 2, 101, NULL },
        { "br-fail", { 1, 1 }, 2, 5, NULL },
        { "br-fail", { 1, 2 }, 2, 100, NULL },
        { "br-fail", { 0, 1 }, 2, -1, NULL },
        { "br-fail", { 0, 0 }, 2, 0, "null descriptor reference" },
        { "br-fail-nn", { 1, 1 }, 2, 5, NULL },
        { "br-fail-nn", { 1, 2 }, 2, 100, NULL },
        { "br-fail-nn", { 0, 1 }, 2, 101, NULL },
    };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[2];
    wasm_value_t result;
    size_t i;

    wasm_config_default(&config);
    config.initial_gc_heap_size = gc_heap_size;
    wasm_init_config(&rt, &config);
    m = wasm_test_load_sections(&rt, wasm_test_exec_desc_sections, sizeof(wasm_test_exec_desc_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(cases); i++) {
        wasm_error_t err;

        args[0] = wasm_i32(cases[i].args[0]);
        args[1] = wasm_i32(cases[i].args[1]);
        result = wasm_i32(-1000);
        err = wasm_call(m, cases[i].name, args, cases[i].num_args, &result, 1);
        if (cases[i].trap) {
            CHECK_MSG(t, err == WASM_ERR_TRAP && strcmp(rt.error_msg, cases[i].trap) == 0, "%s(%d, %d): %s",
                      cases[i].name, cases[i].args[0], cases[i].args[1], rt.error_msg);
        } else {
            CHECK_MSG(t, err == WASM_OK && result.of.i32 == cases[i].expected, "%s(%d, %d): expected %d, got %d (%s)",
                      cases[i].name, cases[i].args[0], cases[i].args[1], cases[i].expected, result.of.i32,
                      rt.error_msg);
        }
    }
    args[0] = wasm_i31ref(3);
    CHECK(t, wasm_call(m, "cast-any", args, 1, &result, 1) == WASM_ERR_TRAP &&
                 strcmp(rt.error_msg, "descriptor cast failure") == 0);
    args[0] = wasm_ref_null(WASM_TYPE_ANYREF);
    WASM_CHECK_OK(t, wasm_call(m, "cast-any", args, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, -1);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_runtime_executes_descriptor_ops) {
    wasm_test_exec_desc_cases(t, (size_t)WASM_GC_HEAP_SIZE);
    wasm_test_exec_desc_cases(t, 0);
}

/* $mixed has mutable fields i8 i64 i16 f32 (ref null $leaf) i8 v128 f64 i32 anyref i16 externref i8
   (ref null $leaf); $high has twelve numeric fields, then (ref null $high) and eqref; $obj has descriptor $vt.
   new-mixed, set-mixed, and get-mixed store and read $mixed fields as wasm_test_layout_expected describes;
   push-high and sum-high build and sum a $high chain; new-objs and sum-objs fill and sum an array of $obj. */
static const uint8_t wasm_test_gc_layout_sections[] = {
    0x01, 0x83, 0x01, 0x0B, 0x5F, 0x01, 0x7F, 0x01, 0x5F, 0x0E, 0x78, 0x01, 0x7E, 0x01, 0x77, 0x01,
    0x7D, 0x01, 0x63, 0x00, 0x01, 0x78, 0x01, 0x7B, 0x01, 0x7C, 0x01, 0x7F, 0x01, 0x6E, 0x01, 0x77,
    0x01, 0x6F, 0x01, 0x78, 0x01, 0x63, 0x00, 0x01, 0x4E, 0x01, 0x5F, 0x0E, 0x7F, 0x01, 0x7E, 0x01,
    0x7D, 0x01, 0x7C, 0x01, 0x78, 0x01, 0x77, 0x01, 0x7B, 0x01, 0x7F, 0x01, 0x7E, 0x01, 0x7F, 0x01,
    0x78, 0x01, 0x7F, 0x01, 0x63, 0x02, 0x01, 0x6D, 0x01, 0x4E, 0x02, 0x4D, 0x04, 0x5F, 0x03, 0x78,
    0x01, 0x63, 0x00, 0x01, 0x77, 0x01, 0x4C, 0x03, 0x5F, 0x02, 0x7F, 0x01, 0x63, 0x00, 0x01, 0x5E,
    0x63, 0x03, 0x01, 0x60, 0x01, 0x7F, 0x01, 0x64, 0x00, 0x60, 0x02, 0x7F, 0x6F, 0x01, 0x64, 0x01,
    0x60, 0x02, 0x64, 0x01, 0x7F, 0x01, 0x7E, 0x60, 0x03, 0x64, 0x01, 0x7F, 0x7F, 0x00, 0x60, 0x01,
    0x7F, 0x00, 0x60, 0x00, 0x01, 0x7E, 0x03, 0x0A, 0x09, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0A,
    0x0B, 0x0A, 0x06, 0x0D, 0x02, 0x63, 0x02, 0x01, 0xD0, 0x71, 0x0B, 0x63, 0x05, 0x01, 0xD0, 0x71,
    0x0B, 0x07, 0x5C, 0x08, 0x09, 0x6E, 0x65, 0x77, 0x2D, 0x6D, 0x69, 0x78, 0x65, 0x64, 0x00, 0x01,
    0x09, 0x67, 0x65, 0x74, 0x2D, 0x6D, 0x69, 0x78, 0x65, 0x64, 0x00, 0x02, 0x09, 0x73, 0x65, 0x74,
    0x2D, 0x6D, 0x69, 0x78, 0x65, 0x64, 0x00, 0x03, 0x09, 0x70, 0x75, 0x73, 0x68, 0x2D, 0x68, 0x69,
    0x67, 0x68, 0x00, 0x04, 0x08, 0x73, 0x75, 0x6D, 0x2D, 0x68, 0x69, 0x67, 0x68, 0x00, 0x05, 0x08,
    0x6E, 0x65, 0x77, 0x2D, 0x6F, 0x62, 0x6A, 0x73, 0x00, 0x06, 0x08, 0x73, 0x75, 0x6D, 0x2D, 0x6F,
    0x62, 0x6A, 0x73, 0x00, 0x07, 0x07, 0x67, 0x61, 0x72, 0x62, 0x61, 0x67, 0x65, 0x00, 0x08, 0x0A,
    0xE6, 0x06, 0x09, 0x07, 0x00, 0x20, 0x00, 0xFB, 0x00, 0x00, 0x0B, 0x55, 0x00, 0x20, 0x00, 0x20,
    0x00, 0xAC, 0x42, 0x81, 0x80, 0x80, 0x80, 0x10, 0x7E, 0x20, 0x00, 0x41, 0x02, 0x6A, 0x20, 0x00,
    0x41, 0x03, 0x6A, 0xB2, 0x20, 0x00, 0x41, 0x04, 0x6A, 0x10, 0x00, 0x20, 0x00, 0x41, 0x05, 0x6A,
    0x20, 0x00, 0x41, 0x06, 0x6A, 0xAC, 0xFD, 0x12, 0x20, 0x00, 0x41, 0x07, 0x6A, 0xB7, 0x20, 0x00,
    0x41, 0x08, 0x6A, 0x20, 0x00, 0x41, 0x09, 0x6A, 0x10, 0x00, 0x20, 0x00, 0x41, 0x0A, 0x6A, 0x20,
    0x01, 0x20, 0x00, 0x41, 0x0C, 0x6A, 0x20, 0x00, 0x41, 0x0D, 0x6A, 0x10, 0x00, 0xFB, 0x00, 0x01,
    0x0B, 0xC8, 0x01, 0x01, 0x01, 0x7B, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40,
    0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40,
    0x02, 0x40, 0x20, 0x01, 0x0E, 0x0D, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x0B, 0x0C, 0x0D, 0x0B, 0x20, 0x00, 0xFB, 0x03, 0x01, 0x00, 0xAC, 0x0F, 0x0B, 0x20, 0x00,
    0xFB, 0x02, 0x01, 0x01, 0x0F, 0x0B, 0x20, 0x00, 0xFB, 0x04, 0x01, 0x02, 0xAD, 0x0F, 0x0B, 0x20,
    0x00, 0xFB, 0x02, 0x01, 0x03, 0xAE, 0x0F, 0x0B, 0x20, 0x00, 0xFB, 0x02, 0x01, 0x04, 0xFB, 0x02,
    0x00, 0x00, 0xAC, 0x0F, 0x0B, 0x20, 0x00, 0xFB, 0x03, 0x01, 0x05, 0xAC, 0x0F, 0x0B, 0x20, 0x00,
    0xFB, 0x02, 0x01, 0x06, 0x22, 0x02, 0xFD, 0x1D, 0x00, 0x20, 0x02, 0xFD, 0x1D, 0x01, 0x7C, 0x0F,
    0x0B, 0x20, 0x00, 0xFB, 0x02, 0x01, 0x07, 0xB0, 0x0F, 0x0B, 0x20, 0x00, 0xFB, 0x02, 0x01, 0x08,
    0xAC, 0x0F, 0x0B, 0x20, 0x00, 0xFB, 0x02, 0x01, 0x09, 0xFB, 0x16, 0x00, 0xFB, 0x02, 0x00, 0x00,
    0xAC, 0x0F, 0x0B, 0x20, 0x00, 0xFB, 0x03, 0x01, 0x0A, 0xAC, 0x0F, 0x0B, 0x20, 0x00, 0xFB, 0x02,
    0x01, 0x0B, 0xD1, 0xAD, 0x0F, 0x0B, 0x20, 0x00, 0xFB, 0x04, 0x01, 0x0C, 0xAD, 0x0F, 0x0B, 0x20,
    0x00, 0xFB, 0x02, 0x01, 0x0D, 0xFB, 0x02, 0x00, 0x00, 0xAC, 0x0B, 0xCE, 0x01, 0x00, 0x02, 0x40,
    0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40,
    0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x20, 0x01, 0x0E, 0x0D, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0B, 0x20, 0x00, 0x20,
    0x02, 0xFB, 0x05, 0x01, 0x00, 0x0F, 0x0B, 0x20, 0x00, 0x20, 0x02, 0xAC, 0x42, 0x81, 0x80, 0x80,
    0x80, 0x10, 0x7E, 0xFB, 0x05, 0x01, 0x01, 0x0F, 0x0B, 0x20, 0x00, 0x20, 0x02, 0xFB, 0x05, 0x01,
    0x02, 0x0F, 0x0B, 0x20, 0x00, 0x20, 0x02, 0xB2, 0xFB, 0x05, 0x01, 0x03, 0x0F, 0x0B, 0x20, 0x00,
    0x20, 0x02, 0x10, 0x00, 0xFB, 0x05, 0x01, 0x04, 0x0F, 0x0B, 0x20, 0x00, 0x20, 0x02, 0xFB, 0x05,
    0x01, 0x05, 0x0F, 0x0B, 0x20, 0x00, 0x20, 0x02, 0xAC, 0xFD, 0x12, 0xFB, 0x05, 0x01, 0x06, 0x0F,
    0x0B, 0x20, 0x00, 0x20, 0x02, 0xB7, 0xFB, 0x05, 0x01, 0x07, 0x0F, 0x0B, 0x20, 0x00, 0x20, 0x02,
    0xFB, 0x05, 0x01, 0x08, 0x0F, 0x0B, 0x20, 0x00, 0x20, 0x02, 0x10, 0x00, 0xFB, 0x05, 0x01, 0x09,
    0x0F, 0x0B, 0x20, 0x00, 0x20, 0x02, 0xFB, 0x05, 0x01, 0x0A, 0x0F, 0x0B, 0x20, 0x00, 0xD0, 0x6F,
    0xFB, 0x05, 0x01, 0x0B, 0x0F, 0x0B, 0x20, 0x00, 0x20, 0x02, 0xFB, 0x05, 0x01, 0x0C, 0x0F, 0x0B,
    0x20, 0x00, 0x20, 0x02, 0x10, 0x00, 0xFB, 0x05, 0x01, 0x0D, 0x0B, 0x43, 0x00, 0x20, 0x00, 0x20,
    0x00, 0xAC, 0x43, 0x00, 0x00, 0x80, 0x3F, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x41, 0x03, 0x41, 0x04, 0xFD, 0x0C, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x07, 0x42, 0x08, 0x41, 0x09, 0x41, 0x0A, 0x20, 0x00,
    0x23, 0x00, 0x20, 0x00, 0x41, 0x03, 0x6C, 0x10, 0x00, 0xFB, 0x00, 0x02, 0x24, 0x00, 0x0B, 0x4D,
    0x02, 0x01, 0x63, 0x02, 0x01, 0x7E, 0x23, 0x00, 0x21, 0x00, 0x02, 0x40, 0x03, 0x40, 0x20, 0x00,
    0xD1, 0x0D, 0x01, 0x20, 0x01, 0x20, 0x00, 0xFB, 0x02, 0x02, 0x00, 0xAC, 0x7C, 0x21, 0x01, 0x20,
    0x01, 0x20, 0x00, 0xFB, 0x02, 0x02, 0x0B, 0xAC, 0x7C, 0x21, 0x01, 0x20, 0x01, 0x20, 0x00, 0xFB,
    0x02, 0x02, 0x0D, 0xFB, 0x16, 0x00, 0xFB, 0x02, 0x00, 0x00, 0xAC, 0x7C, 0x21, 0x01, 0x20, 0x00,
    0xFB, 0x02, 0x02, 0x0C, 0x21, 0x00, 0x0C, 0x00, 0x0B, 0x0B, 0x20, 0x01, 0x0B, 0x48, 0x01, 0x01,
    0x7F, 0x20, 0x00, 0xFB, 0x07, 0x05, 0x24, 0x01, 0x02, 0x40, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00,
    0x4F, 0x0D, 0x01, 0x23, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x41, 0x01, 0x6A, 0x10, 0x00,
    0x20, 0x01, 0x41, 0x02, 0x6A, 0x20, 0x01, 0x41, 0x03, 0x6A, 0x20, 0x01, 0x41, 0x04, 0x6A, 0x10,
    0x00, 0xFB, 0x00, 0x04, 0xFB, 0x20, 0x03, 0xFB, 0x0E, 0x05, 0x20, 0x01, 0x41, 0x01, 0x6A, 0x21,
    0x01, 0x0C, 0x00, 0x0B, 0x0B, 0x0B, 0x76, 0x03, 0x01, 0x7F, 0x01, 0x63, 0x03, 0x01, 0x7E, 0x02,
    0x40, 0x03, 0x40, 0x20, 0x00, 0x23, 0x01, 0xFB, 0x0F, 0x4F, 0x0D, 0x01, 0x23, 0x01, 0x20, 0x00,
    0xFB, 0x0B, 0x05, 0x21, 0x01, 0x20, 0x02, 0x20, 0x01, 0xFB, 0x04, 0x03, 0x00, 0xAD, 0x7C, 0x21,
    0x02, 0x20, 0x02, 0x20, 0x01, 0xFB, 0x02, 0x03, 0x01, 0xFB, 0x02, 0x00, 0x00, 0xAC, 0x7C, 0x21,
    0x02, 0x20, 0x02, 0x20, 0x01, 0xFB, 0x04, 0x03, 0x02, 0xAD, 0x7C, 0x21, 0x02, 0x20, 0x02, 0x20,
    0x01, 0xFB, 0x22, 0x03, 0xFB, 0x02, 0x04, 0x00, 0xAC, 0x7C, 0x21, 0x02, 0x20, 0x02, 0x20, 0x01,
    0xFB, 0x22, 0x03, 0xFB, 0x02, 0x04, 0x01, 0xFB, 0x02, 0x00, 0x00, 0xAC, 0x7C, 0x21, 0x02, 0x20,
    0x00, 0x41, 0x01, 0x6A, 0x21, 0x00, 0x0C, 0x00, 0x0B, 0x0B, 0x20, 0x02, 0x0B, 0x1A, 0x00, 0x03,
    0x40, 0x20, 0x00, 0xFB, 0x00, 0x00, 0x1A, 0x41, 0x04, 0xFB, 0x07, 0x05, 0x1A, 0x20, 0x00, 0x41,
    0x01, 0x6B, 0x22, 0x00, 0x0D, 0x00, 0x0B, 0x0B,
};

/* The i64 get-mixed returns for field i after it stored x, with ext_null for field 11. */
static int64_t wasm_test_layout_expected(uint32_t i, int32_t x, int ext_null) {
    switch (i) {
        case 0:
        case 5:
            return (int64_t)(int8_t)(uint8_t)((uint32_t)x & 0xFFu);
        case 1:
            return (int64_t)x * INT64_C(0x100000001);
        case 2:
            return (int64_t)((uint32_t)x & 0xFFFFu);
        case 6:
            return 2 * (int64_t)x;
        case 10:
            return (int64_t)(int16_t)(uint16_t)((uint32_t)x & 0xFFFFu);
        case 11:
            return ext_null;
        case 12:
            return (int64_t)((uint32_t)x & 0xFFu);
        default:
            return (int64_t)x;
    }
}

enum { WASM_TEST_LAYOUT_OBJECTS = 3, WASM_TEST_LAYOUT_FIELDS = 14 };

static void wasm_test_layout_check(test_ctx* t, wasm_module_t* m, wasm_value_t* const* held,
                                   int32_t stored[WASM_TEST_LAYOUT_OBJECTS][WASM_TEST_LAYOUT_FIELDS],
                                   const int* ext_null) {
    uint32_t k;
    uint32_t i;

    for (k = 0; k < WASM_TEST_LAYOUT_OBJECTS; k++) {
        for (i = 0; i < WASM_TEST_LAYOUT_FIELDS; i++) {
            wasm_value_t args[2];
            wasm_value_t result = wasm_i64(-1);

            args[0] = *held[k];
            args[1] = wasm_i32((int32_t)i);
            WASM_CHECK_OK(t, wasm_call(m, "get-mixed", args, 2, &result, 1));
            CHECK_MSG(t, result.of.i64 == wasm_test_layout_expected(i, stored[k][i], ext_null[k]),
                      "object %u field %u: expected %lld, got %lld", (unsigned)k, (unsigned)i,
                      (long long)wasm_test_layout_expected(i, stored[k][i], ext_null[k]), (long long)result.of.i64);
        }
    }
}

TEST(test_gc_runtime_reads_and_writes_every_field_kind_across_collections) {
    static const int32_t seeds[WASM_TEST_LAYOUT_OBJECTS] = { 200, -5, 70000 };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t* held[WASM_TEST_LAYOUT_OBJECTS];
    int32_t stored[WASM_TEST_LAYOUT_OBJECTS][WASM_TEST_LAYOUT_FIELDS];
    int ext_null[WASM_TEST_LAYOUT_OBJECTS];
    wasm_value_t args[3];
    wasm_value_t result;
    wasm_value_t garbage = wasm_i32(100);
    uint32_t k;
    uint32_t i;
    int all_held = 1;

    wasm_config_default(&config);
    config.initial_gc_heap_size = 4096;
    wasm_init_config(&rt, &config);
    m = wasm_test_load_sections(&rt, wasm_test_gc_layout_sections, sizeof(wasm_test_gc_layout_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    for (k = 0; k < WASM_TEST_LAYOUT_OBJECTS; k++) {
        result = wasm_ref_null(WASM_TYPE_NONE);
        args[0] = wasm_i32(seeds[k]);
        args[1] = k == 0 ? wasm_externref((uintptr_t)0x1230u) : wasm_ref_null(WASM_TYPE_EXTERNREF);
        ext_null[k] = k != 0;
        for (i = 0; i < WASM_TEST_LAYOUT_FIELDS; i++) stored[k][i] = i == 1 ? seeds[k] : seeds[k] + (int32_t)i;
        WASM_CHECK_OK(t, wasm_call(m, "new-mixed", args, 2, &result, 1));
        held[k] = wasm_ref_hold(&rt, result);
        all_held = all_held && held[k] != NULL;
        WASM_CHECK_OK(t, wasm_call(m, "garbage", &garbage, 1, NULL, 0));
    }
    CHECK(t, all_held);
    if (all_held) {
        wasm_gc_collect(&rt);
        wasm_test_layout_check(t, m, held, stored, ext_null);

        for (k = 0; k < WASM_TEST_LAYOUT_OBJECTS; k++) {
            for (i = 0; i < WASM_TEST_LAYOUT_FIELDS; i++) {
                stored[k][i] = 1000 * (int32_t)k + 17 * (int32_t)i - 300;
                args[0] = *held[k];
                args[1] = wasm_i32((int32_t)i);
                args[2] = wasm_i32(stored[k][i]);
                WASM_CHECK_OK(t, wasm_call(m, "set-mixed", args, 3, NULL, 0));
                WASM_CHECK_OK(t, wasm_call(m, "garbage", &garbage, 1, NULL, 0));
            }
            ext_null[k] = 1;
        }
        wasm_gc_collect(&rt);
        wasm_test_layout_check(t, m, held, stored, ext_null);
    }
    for (k = 0; k < WASM_TEST_LAYOUT_OBJECTS; k++) wasm_ref_release(&rt, held[k]);

    for (i = 1; i <= 300; i++) {
        args[0] = wasm_i32((int32_t)i);
        WASM_CHECK_OK(t, wasm_call(m, "push-high", args, 1, NULL, 0));
        if (i % 50u == 0u) WASM_CHECK_OK(t, wasm_call(m, "garbage", &garbage, 1, NULL, 0));
    }
    wasm_gc_collect(&rt);
    result = wasm_i64(0);
    WASM_CHECK_OK(t, wasm_call(m, "sum-high", NULL, 0, &result, 1));
    WASM_CHECK_I64(t, result.of.i64, INT64_C(5) * 300 * 301 / 2);

    args[0] = wasm_i32(200);
    WASM_CHECK_OK(t, wasm_call(m, "new-objs", args, 1, NULL, 0));
    WASM_CHECK_OK(t, wasm_call(m, "garbage", &garbage, 1, NULL, 0));
    wasm_gc_collect(&rt);
    result = wasm_i64(0);
    WASM_CHECK_OK(t, wasm_call(m, "sum-objs", NULL, 0, &result, 1));
    WASM_CHECK_I64(t, result.of.i64, INT64_C(5) * 199 * 200 / 2 + 10 * 200);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_gc_cases[] = {
    { "wasmgc: runtime executes struct, array, i31, and conversion ops", test_gc_runtime_executes_struct_array_i31_and_conversion_ops },
    { "wasmgc: runtime executes casts and branch-on-cast ops", test_gc_runtime_executes_cast_and_branch_cast_opcodes },
    { "wasmgc: runtime executes segment-backed array ops", test_gc_runtime_executes_segment_backed_array_ops },
    { "custom descriptors: runtime allocates, reads, casts, and branches on descriptors", test_gc_runtime_executes_descriptor_ops },
    { "wasmgc: runtime reads and writes fields of every kind, high-index refs, and descriptors across collections",
      test_gc_runtime_reads_and_writes_every_field_kind_across_collections },
};
const test_suite exec_gc_suite = { "exec.gc", exec_gc_cases, COUNTOF(exec_gc_cases) };

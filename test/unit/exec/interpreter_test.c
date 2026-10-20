#include "../support.h"

TEST(test_add) {
    /*
     * Module with one function: (func (param i32 i32) (result i32)
     *   local.get 0
     *   local.get 1
     *   i32.add)
     */
    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    /* Type section: one func type (i32, i32) -> i32 */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 type */
        emit(&sec, 0x60);         /* func */
        emit_leb128_u32(&sec, 2); /* 2 params */
        emit(&sec, 0x7F);         /* i32 */
        emit(&sec, 0x7F);         /* i32 */
        emit_leb128_u32(&sec, 1); /* 1 result */
        emit(&sec, 0x7F);         /* i32 */
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function section: one function using type 0 */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 func */
        emit_leb128_u32(&sec, 0); /* type index 0 */
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export section: export func 0 as "add" */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 export */
        emit_leb128_u32(&sec, 3); /* name len */
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, 'd');
        emit(&sec, 0x00);         /* func kind */
        emit_leb128_u32(&sec, 0); /* func index */
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code section */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 body */

        /* Function body */
        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0); /* 0 local declarations */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1); /* local.get 1 */
        emit(&body, 0x6A);         /* i32.add */
        emit(&body, 0x0B);         /* end */

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[] = { wasm_i32(3), wasm_i32(7) };
    wasm_value_t result;
    wasm_error_t err;
    wasm_test_alloc_stats alloc_before = wasm_test_alloc_snapshot();
    wasm_test_alloc_stats alloc_after;
    size_t alloc_delta;
    size_t free_delta;

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "add", args, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 10);
    }

    args[0] = wasm_f32(1.0f);
    result = wasm_i32(99);
    err = wasm_call(m, "add", args, 2, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected argument type mismatch rejection");
    WASM_CHECK_I32(t, result.of.i32, 99);

    args[0] = wasm_i32(3);
    args[1] = wasm_i64(7);
    err = wasm_call_index(m, 0, args, 2, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected indexed argument type mismatch rejection");

    args[1] = wasm_i32(7);
    err = wasm_call_index(m, 0, args, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 10);

    wasm_module_free(m);
    wasm_destroy(&rt);

    alloc_after = wasm_test_alloc_snapshot();
    alloc_delta = (alloc_after.alloc_calls - alloc_before.alloc_calls) +
                  (alloc_after.resize_calls - alloc_before.resize_calls);
    free_delta = alloc_after.free_calls - alloc_before.free_calls;
    CHECK_MSG(t, alloc_delta > 0, "%s", "expected custom wasm allocators to be exercised");
    CHECK_MSG(t, free_delta > 0, "%s", "expected custom wasm allocator frees during cleanup");
}

TEST(test_factorial) {
    /*
     * (func $fac (param i32) (result i32)
     *   local.get 0
     *   i32.eqz
     *   if (result i32)
     *     i32.const 1
     *   else
     *     local.get 0
     *     local.get 0
     *     i32.const 1
     *     i32.sub
     *     call $fac
     *     i32.mul
     *   end)
     */
    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    /* Type section */
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

    /* Function section */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'f');
        emit(&sec, 'a');
        emit(&sec, 'c');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code section */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0); /* no local decls */

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x45);         /* i32.eqz */
        emit(&body, 0x04);
        emit(&body, 0x7F); /* if (result i32) */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1); /* i32.const 1 */
        emit(&body, 0x05);         /* else */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1); /* i32.const 1 */
        emit(&body, 0x6B);         /* i32.sub */
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0); /* call 0 (self) */
        emit(&body, 0x6C);         /* i32.mul */
        emit(&body, 0x0B);         /* end if */
        emit(&body, 0x0B);         /* end func */

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[] = { wasm_i32(10) };
    wasm_value_t result;
    wasm_error_t err;

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "fac", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 3628800);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_loop) {
    /*
     * (func (result i32)
     *   (local $i i32) (local $sum i32)
     *   local.get $i   ;; i = 0
     *   block
     *     loop
     *       ;; i = i + 1
     *       local.get $i
     *       i32.const 1
     *       i32.add
     *       local.set $i
     *       ;; sum = sum + i
     *       local.get $sum
     *       local.get $i
     *       i32.add
     *       local.set $sum
     *       ;; br_if 1 if i >= 10 (exit block)
     *       local.get $i
     *       i32.const 10
     *       i32.ge_s
     *       br_if 1
     *       ;; else loop
     *       br 0
     *     end
     *   end
     *   local.get $sum)
     */
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

    /* Export */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 's');
        emit(&sec, 'u');
        emit(&sec, 'm');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = { 0 };
        /* 2 locals: i32, i32 */
        emit_leb128_u32(&body, 1); /* 1 local declaration */
        emit_leb128_u32(&body, 2); /* 2 locals */
        emit(&body, 0x7F);         /* i32 */

        /* block (void) */
        emit(&body, 0x02);
        emit(&body, 0x40);
        /* loop (void) */
        emit(&body, 0x03);
        emit(&body, 0x40);

        /* i = i + 1 */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 ($i) */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1); /* i32.const 1 */
        emit(&body, 0x6A);         /* i32.add */
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0); /* local.set 0 ($i) */

        /* sum = sum + i */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1); /* local.get 1 ($sum) */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 ($i) */
        emit(&body, 0x6A);         /* i32.add */
        emit(&body, 0x21);
        emit_leb128_u32(&body, 1); /* local.set 1 ($sum) */

        /* if i >= 10, break */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 ($i) */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 10); /* i32.const 10 */
        emit(&body, 0x4E);          /* i32.ge_s */
        emit(&body, 0x0D);
        emit_leb128_u32(&body, 1); /* br_if 1 (break block) */

        /* continue loop */
        emit(&body, 0x0C);
        emit_leb128_u32(&body, 0); /* br 0 (loop) */

        emit(&body, 0x0B); /* end loop */
        emit(&body, 0x0B); /* end block */

        /* return sum */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1); /* local.get 1 ($sum) */
        emit(&body, 0x0B);         /* end func */

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

    err = wasm_call(m, "sum", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 55);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_loop_type_index_block) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
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
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'l');
        emit(&sec, 'o');
        emit(&sec, 'o');
        emit(&sec, 'p');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x7F);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x03);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x6A);
        emit(&body, 0x22);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x48);
        emit(&body, 0x0D);
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

    err = wasm_call(m, "loop", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 5);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_sign_extension_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t i32_result, i64_result;
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
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

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

        emit_leb128_u32(&sec, 5);
        emit(&sec, 'e');
        emit(&sec, 'x');
        emit(&sec, 't');
        emit(&sec, '3');
        emit(&sec, '2');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 'e');
        emit(&sec, 'x');
        emit(&sec, 't');
        emit(&sec, '6');
        emit(&sec, '4');
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
        emit_leb128_i32(&body, 65408);
        emit(&body, 0xC0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 32769);
        emit(&body, 0xC1);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 128);
        emit(&body, 0xC2);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 32769);
        emit(&body, 0xC3);
        emit(&body, 0x7C);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 4294967295LL);
        emit(&body, 0xC4);
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

    err = wasm_call(m, "ext32", NULL, 0, &i32_result, 1);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_module_free(m);
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "ext64", NULL, 0, &i64_result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, i32_result.of.i32, -32895);
        WASM_CHECK_I64(t, i64_result.of.i64, -32896);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_multivalue_if_block) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[1];
    wasm_value_t results[2];
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
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
        emit_leb128_u32(&sec, 2);
        emit(&sec, 'm');
        emit(&sec, 'v');
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
        emit(&body, 0x04);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 11);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 22);
        emit(&body, 0x05);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 33);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 44);
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

    args[0] = wasm_i32(1);
    err = wasm_call(m, "mv", args, 1, results, 2);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_module_free(m);
        wasm_destroy(&rt);
        return;
    }
    if (!(results[0].of.i32 == 11 && results[1].of.i32 == 22)) {
        CHECK_MSG(t, false, "%s", "unexpected true-branch multivalue results");
        wasm_module_free(m);
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(0);
    err = wasm_call(m, "mv", args, 1, results, 2);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(
            t,
            results[0].of.i32 == 33 && results[1].of.i32 == 44,
            "%s",
            "unexpected false-branch multivalue results");
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_multivalue_branch_block) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t results[2];
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
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
        emit_leb128_u32(&sec, 2);
        emit(&sec, 'b');
        emit(&sec, 'r');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x02);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x0C);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
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

    err = wasm_call(m, "br", NULL, 0, results, 2);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(
            t,
            results[0].of.i32 == 7 && results[1].of.i32 == 9,
            "%s",
            "unexpected multivalue branch results");
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_multivalue_loop_params) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t results[2];
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
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
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'l');
        emit(&sec, 'o');
        emit(&sec, 'o');
        emit(&sec, 'p');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x7F);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x03);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0C);
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

    err = wasm_call(m, "loop", NULL, 0, results, 2);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(
            t,
            results[0].of.i32 == 4 && results[1].of.i32 == 9,
            "%s",
            "unexpected multivalue loop results");
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_f64_arithmetic_propagates_the_first_nan_operand) {
    static const char* const names[] = { "add", "sub", "mul", "div" };
    static const uint64_t cases[][3] = {
        { UINT64_C(0x7FF0000000000001), UINT64_C(0x7FF8000000000002), UINT64_C(0x7FF8000000000001) },
        { UINT64_C(0x7FF8000000000002), UINT64_C(0x7FF0000000000001), UINT64_C(0x7FF8000000000002) },
        { UINT64_C(0x3FF0000000000000), UINT64_C(0xFFF0000000000003), UINT64_C(0xFFF8000000000003) },
        { UINT64_C(0xFFF800000000000A), UINT64_C(0x3FF0000000000000), UINT64_C(0xFFF800000000000A) },
        { UINT64_C(0x7FF800000000000A), UINT64_C(0xFFF800000000000B), UINT64_C(0x7FF800000000000A) },
    };
    wasm_builder_t mod = { 0 };
    wasm_builder_t sec = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t i;
    uint32_t k;

    emit_header(&mod);
    emit(&sec, 1);
    emit(&sec, 0x60);
    emit(&sec, 2);
    emit(&sec, 0x7E);
    emit(&sec, 0x7E);
    emit(&sec, 1);
    emit(&sec, 0x7E);
    emit_section(&mod, 1, sec.buf, sec.len);
    sec.len = 0;
    emit(&sec, 4);
    for (i = 0; i < 4; i++) emit(&sec, 0);
    emit_section(&mod, 3, sec.buf, sec.len);
    sec.len = 0;
    emit(&sec, 4);
    for (i = 0; i < 4; i++) emit_export_func(&sec, names[i], i);
    emit_section(&mod, 7, sec.buf, sec.len);
    sec.len = 0;
    emit(&sec, 4);
    for (i = 0; i < 4; i++) {
        static const uint8_t body[] = { 10, 0, 0x20, 0, 0xBF, 0x20, 1, 0xBF, 0, 0xBD, 0x0B };
        uint8_t code[sizeof(body)];

        memcpy(code, body, sizeof(body));
        code[8] = (uint8_t)(0xA0 + i);
        emit_bytes(&sec, code, sizeof(code));
    }
    emit_section(&mod, 10, sec.buf, sec.len);

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < 4; i++) {
        for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
            wasm_value_t args[2];
            wasm_value_t result = wasm_i64(0);

            args[0] = wasm_i64((int64_t)cases[k][0]);
            args[1] = wasm_i64((int64_t)cases[k][1]);
            WASM_CHECK_OK(t, wasm_call(m, names[i], args, 2, &result, 1));
            CHECK_MSG(t, (uint64_t)result.of.i64 == cases[k][2], "f64.%s of %llx and %llx gave %llx", names[i],
                      (unsigned long long)cases[k][0], (unsigned long long)cases[k][1],
                      (unsigned long long)result.of.i64);
        }
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_div_by_zero) {
    wasm_builder_t mod = { 0 };
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
        emit_leb128_u32(&sec, 1);
        emit(&sec, 'f');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 10);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x6D); /* i32.div_s */
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

    err = wasm_call(m, "f", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_DIV_BY_ZERO, "%s", "expected div by zero trap");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_float_signaling_nan_bits) {
    static const uint8_t code[][16] = {
        { 0x43, 0x10, 0x32, 0xA0, 0xFF, 0xBC },
        { 0x43, 0x10, 0x32, 0xA0, 0xFF, 0x8B, 0xBC },
        { 0x43, 0x10, 0x32, 0xA0, 0x7F, 0x8C, 0xBC },
        { 0x43, 0x10, 0x32, 0xA0, 0xFF, 0xFD, 0x13, 0xFD, 0x1F, 0, 0xBC },
        { 0x43, 0x10, 0x32, 0xA0, 0xFF, 0xFD, 0x13, 0xFD, 0xE0, 1, 0xFD, 0x1F, 0, 0xBC }
    };
    static const uint32_t lengths[] = { 6, 7, 7, 11, 14 };
    static const uint32_t expected[] = { 0xFFA03210u, 0x7FA03210u, 0xFFA03210u, 0xFFA03210u, 0x7FA03210u };
    uint32_t i;
    for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        wasm_builder_t mod = { 0 }, sec = { 0 }, body = { 0 };
        wasm_runtime_t rt;
        wasm_module_t* m;
        wasm_value_t result;
        wasm_error_t err;
        static const uint8_t type[] = { 1, 0x60, 0, 1, 0x7F };
        static const uint8_t funcs[] = { 1, 0 };
        emit_header(&mod);
        emit_section(&mod, 1, type, sizeof(type));
        emit_section(&mod, 3, funcs, sizeof(funcs));
        emit(&sec, 1);
        emit_export_func(&sec, "bits", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
        sec.len = 0;
        emit(&body, 0);
        emit_bytes(&body, code[i], lengths[i]);
        emit(&body, 0x0B);
        emit(&sec, 1);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
        wasm_init(&rt);
        m = wasm_load(&rt, mod.buf, mod.len);
        CHECK_MSG(t, m != NULL, "case %u: %s", i, rt.error_msg);
        if (m) {
            err = wasm_call(m, "bits", NULL, 0, &result, 1);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) {
                CHECK_MSG(t, (uint32_t)result.of.i32 == expected[i],
                          "case %u: signaling NaN bits changed", i);
            }
            wasm_module_free(m);
        }
        wasm_destroy(&rt);
    }
}

TEST(test_rotate_by_zero) {
    static const char* export_names[] = { "rotl32", "rotr32", "rotl64", "rotr64" };
    static const uint8_t type_indices[] = { 0, 0, 1, 1 };
    static const uint8_t ops[] = { 0x77, 0x78, 0x89, 0x8A };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint32_t i;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        for (i = 0; i < 4; i++) emit_leb128_u32(&sec, type_indices[i]);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        for (i = 0; i < 4; i++) emit_export_func(&sec, export_names[i], i);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        for (i = 0; i < 4; i++) {
            wasm_builder_t body = { 0 };
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 0);
            if (i < 2) {
                emit(&body, 0x41);
                emit_leb128_i32(&body, 0);
            } else {
                emit(&body, 0x42);
                emit_leb128_i64(&body, 0);
            }
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

    {
        wasm_value_t args[] = { wasm_i32((int32_t)0x81234567u) };
        err = wasm_call(m, "rotl32", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, (int32_t)0x81234567u);

        err = wasm_call(m, "rotr32", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, (int32_t)0x81234567u);
    }

    {
        wasm_value_t args[] = { wasm_i64((int64_t)0x8123456789ABCDEFull) };
        err = wasm_call(m, "rotl64", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, (int64_t)0x8123456789ABCDEFull);

        err = wasm_call(m, "rotr64", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, (int64_t)0x8123456789ABCDEFull);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_wide_arithmetic_feature_gate) {
    static const uint8_t sections[] = {
        0x01, 0x08, 0x01, 0x60, 0x02, 0x7E, 0x7E, 0x02, 0x7E, 0x7E,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x07, 0x01, 0x03, 'm', 'u', 'l', 0x00, 0x00,
        0x0A, 0x0A, 0x01, 0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0xFC, 0x15, 0x0B,
    };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[2];
    wasm_value_t results[2];
    wasm_error_t err;

    emit_header(&mod);
    emit_bytes(&mod, sections, sizeof(sections));

    wasm_init_features(&rt, WASM_FEATURES_STANDARD);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected wide arithmetic to be disabled");
    CHECK_MSG(t, strstr(rt.error_msg, "wide arithmetic") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        args[0] = wasm_i64(-3);
        args[1] = wasm_i64(5);
        err = wasm_call(m, "mul", args, 2, results, 2);
        CHECK_MSG(t, err == WASM_OK, "%s", rt.error_msg);
        WASM_CHECK_I64(t, results[0].of.i64, -15);
        WASM_CHECK_I64(t, results[1].of.i64, -1);

        args[0] = wasm_i64(INT64_MIN);
        args[1] = wasm_i64(INT64_MIN);
        err = wasm_call(m, "mul", args, 2, results, 2);
        CHECK_MSG(t, err == WASM_OK, "%s", rt.error_msg);
        WASM_CHECK_I64(t, results[0].of.i64, 0);
        WASM_CHECK_I64(t, results[1].of.i64, INT64_C(0x4000000000000000));
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_host_set_payload_only(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    results[0].of.i32 = 7;
    return WASM_OK;
}

static wasm_error_t wasm_test_host_return_i32(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    results[0] = wasm_i32(0x1234);
    return WASM_OK;
}

TEST(test_host_results_must_match_the_import_type) {
    /* get() returns env.get(), declared () -> i32; ref() returns ref.is_null(env.ref()), declared () -> anyref. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x00, 0x01,
        0x6e, 0x02, 0x15, 0x02, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x67, 0x65, 0x74, 0x00, 0x00, 0x03, 0x65, 0x6e, 0x76,
        0x03, 0x72, 0x65, 0x66, 0x00, 0x01, 0x03, 0x03, 0x02, 0x00, 0x00, 0x07, 0x0d, 0x02, 0x03, 0x67, 0x65, 0x74,
        0x00, 0x02, 0x03, 0x72, 0x65, 0x66, 0x00, 0x03, 0x0a, 0x0c, 0x02, 0x04, 0x00, 0x10, 0x00, 0x0b, 0x05, 0x00,
        0x10, 0x01, 0xd1, 0x0b,
    };
    wasm_valtype_t anyref_result = WASM_TYPE_ANYREF;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_import_t imp;
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "get", "(i)", wasm_test_host_set_payload_only, NULL));
    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "ref";
    imp.type.num_results = 1;
    imp.type.results = &anyref_result;
    imp.func = wasm_test_host_return_i32;
    WASM_CHECK_OK(t, wasm_register_import(&rt, &imp));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_OK(t, wasm_call(m, "get", NULL, 0, &result, 1));
    CHECK(t, result.type == WASM_TYPE_I32 && result.of.i32 == 7);
    CHECK(t, wasm_call(m, "ref", NULL, 0, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    CHECK_MSG(t, strstr(rt.error_msg, "host function result 0") != NULL, "%s", rt.error_msg);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* Exports i32_f32_s, i32_f32_u, i32_f64_s, i32_f64_u, i64_f32_s, i64_f32_u, i64_f64_s, and i64_f64_u, which run
 * each saturating truncation on the float whose bits they take. */
static const uint8_t wasm_test_trunc_sat_sections[] = {
    0x01, 0x15, 0x04, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x60, 0x01, 0x7E, 0x01, 0x7F, 0x60, 0x01, 0x7F,
    0x01, 0x7E, 0x60, 0x01, 0x7E, 0x01, 0x7E, 0x03, 0x09, 0x08, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02,
    0x03, 0x03, 0x07, 0x61, 0x08, 0x09, 0x69, 0x33, 0x32, 0x5F, 0x66, 0x33, 0x32, 0x5F, 0x73, 0x00,
    0x00, 0x09, 0x69, 0x33, 0x32, 0x5F, 0x66, 0x33, 0x32, 0x5F, 0x75, 0x00, 0x01, 0x09, 0x69, 0x33,
    0x32, 0x5F, 0x66, 0x36, 0x34, 0x5F, 0x73, 0x00, 0x02, 0x09, 0x69, 0x33, 0x32, 0x5F, 0x66, 0x36,
    0x34, 0x5F, 0x75, 0x00, 0x03, 0x09, 0x69, 0x36, 0x34, 0x5F, 0x66, 0x33, 0x32, 0x5F, 0x73, 0x00,
    0x04, 0x09, 0x69, 0x36, 0x34, 0x5F, 0x66, 0x33, 0x32, 0x5F, 0x75, 0x00, 0x05, 0x09, 0x69, 0x36,
    0x34, 0x5F, 0x66, 0x36, 0x34, 0x5F, 0x73, 0x00, 0x06, 0x09, 0x69, 0x36, 0x34, 0x5F, 0x66, 0x36,
    0x34, 0x5F, 0x75, 0x00, 0x07, 0x0A, 0x41, 0x08, 0x07, 0x00, 0x20, 0x00, 0xBE, 0xFC, 0x00, 0x0B,
    0x07, 0x00, 0x20, 0x00, 0xBE, 0xFC, 0x01, 0x0B, 0x07, 0x00, 0x20, 0x00, 0xBF, 0xFC, 0x02, 0x0B,
    0x07, 0x00, 0x20, 0x00, 0xBF, 0xFC, 0x03, 0x0B, 0x07, 0x00, 0x20, 0x00, 0xBE, 0xFC, 0x04, 0x0B,
    0x07, 0x00, 0x20, 0x00, 0xBE, 0xFC, 0x05, 0x0B, 0x07, 0x00, 0x20, 0x00, 0xBF, 0xFC, 0x06, 0x0B,
    0x07, 0x00, 0x20, 0x00, 0xBF, 0xFC, 0x07, 0x0B,
};

/* Returns the saturating truncation of value to a 32- or 64-bit integer, as bits zero-extended from its width. */
static uint64_t wasm_test_trunc_sat(double value, int is_signed, int is_64) {
    double t;

    if (value != value) return 0;
    t = trunc(value);
    if (is_signed) {
        if (t < (is_64 ? -9223372036854775808.0 : -2147483648.0))
            return is_64 ? UINT64_C(0x8000000000000000) : UINT64_C(0x80000000);
        if (t >= (is_64 ? 9223372036854775808.0 : 2147483648.0))
            return is_64 ? UINT64_C(0x7FFFFFFFFFFFFFFF) : UINT64_C(0x7FFFFFFF);
        return is_64 ? (uint64_t)(int64_t)t : (uint64_t)(uint32_t)(int32_t)t;
    }
    if (t < 0.0) return 0;
    if (t >= (is_64 ? 18446744073709551616.0 : 4294967296.0)) return is_64 ? UINT64_MAX : UINT64_C(0xFFFFFFFF);
    return (uint64_t)t;
}

TEST(test_trunc_sat_saturates_every_edge_value) {
    static const uint32_t f32_bits[] = {
        0x7FC00000u, 0xFFC00001u, 0x7FA00000u, 0x7F800000u, 0xFF800000u, 0x00000000u,
        0x80000000u, 0x00000001u, 0x3F000000u, 0xBF000000u, 0xBF7FFFFFu, 0x3FC00000u,
        0xC2F70000u, 0x4EFFFFFFu, 0x4F000000u, 0xCF000000u, 0xCF000001u, 0x4F7FFFFFu,
        0x4F800000u, 0x5EFFFFFFu, 0x5F000000u, 0xDF000000u, 0xDF000001u, 0x5F7FFFFFu,
        0x5F800000u, 0x7F7FFFFFu, 0xFF7FFFFFu,
    };
    static const uint64_t f64_bits[] = {
        UINT64_C(0x7FF8000000000000), UINT64_C(0xFFF0000000000001), UINT64_C(0x7FF0000000000000),
        UINT64_C(0xFFF0000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
        UINT64_C(0x41DFFFFFFFC00000), UINT64_C(0x41DFFFFFFFE00000), UINT64_C(0x41E0000000000000),
        UINT64_C(0xC1E0000000000000), UINT64_C(0xC1E0000000100000), UINT64_C(0xC1E0000000200000),
        UINT64_C(0x41EFFFFFFFE00000), UINT64_C(0x41EFFFFFFFF00000), UINT64_C(0x41F0000000000000),
        UINT64_C(0x43DFFFFFFFFFFFFF), UINT64_C(0x43E0000000000000), UINT64_C(0xC3E0000000000000),
        UINT64_C(0xC3E0000000000001), UINT64_C(0x43EFFFFFFFFFFFFF), UINT64_C(0x43F0000000000000),
        UINT64_C(0x7E37E43C8800759C), UINT64_C(0xFE37E43C8800759C), UINT64_C(0x0000000000000001),
        UINT64_C(0xBFE0000000000000), UINT64_C(0xBFEFFFFFFFF24190), UINT64_C(0x3FE0000000000000),
        UINT64_C(0xC05EF00000000000),
    };
    static const char* const names[] = { "i32_f32_s", "i32_f32_u", "i32_f64_s", "i32_f64_u",
                                         "i64_f32_s", "i64_f32_u", "i64_f64_s", "i64_f64_u" };
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t op;
    uint32_t i;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, wasm_test_trunc_sat_sections, sizeof(wasm_test_trunc_sat_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (op = 0; op < COUNTOF(names); op++) {
        int is_64 = op >= 4;
        int is_signed = (op & 1u) == 0;
        int from_f64 = (op & 2u) != 0;
        uint32_t count = from_f64 ? (uint32_t)COUNTOF(f64_bits) : (uint32_t)COUNTOF(f32_bits);

        for (i = 0; i < count; i++) {
            wasm_value_t arg;
            wasm_value_t result;
            uint64_t expected;
            uint64_t actual;
            double value;

            if (from_f64) {
                memcpy(&value, &f64_bits[i], sizeof(value));
                arg = wasm_i64((int64_t)f64_bits[i]);
            } else {
                float narrow;

                memcpy(&narrow, &f32_bits[i], sizeof(narrow));
                value = (double)narrow;
                arg = wasm_i32((int32_t)f32_bits[i]);
            }
            expected = wasm_test_trunc_sat(value, is_signed, is_64);
            memset(&result, 0, sizeof(result));
            WASM_CHECK_OK(t, wasm_call(m, names[op], &arg, 1, &result, 1));
            actual = is_64 ? (uint64_t)result.of.i64 : (uint64_t)(uint32_t)result.of.i32;
            CHECK_MSG(t, actual == expected, "%s(%u) returned 0x%llx, not 0x%llx", names[op], (unsigned)i,
                      (unsigned long long)actual, (unsigned long long)expected);
        }
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* Exports inc, twice, and table, which return ref.func of a function in a table, ref.func of the import
 * host.twice, and the table's element, and loop(n), which calls through both references n times. */
static const uint8_t wasm_test_ref_func_sections[] = {
    0x01, 0x0A, 0x02, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x60, 0x00, 0x01, 0x70, 0x02, 0x0E, 0x01, 0x04,
    0x68, 0x6F, 0x73, 0x74, 0x05, 0x74, 0x77, 0x69, 0x63, 0x65, 0x00, 0x00, 0x03, 0x06, 0x05, 0x00,
    0x01, 0x01, 0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x07, 0x1E, 0x04, 0x03, 0x69, 0x6E,
    0x63, 0x00, 0x02, 0x05, 0x74, 0x77, 0x69, 0x63, 0x65, 0x00, 0x03, 0x05, 0x74, 0x61, 0x62, 0x6C,
    0x65, 0x00, 0x04, 0x04, 0x6C, 0x6F, 0x6F, 0x70, 0x00, 0x05, 0x09, 0x0D, 0x02, 0x02, 0x00, 0x41,
    0x00, 0x0B, 0x00, 0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x40, 0x05, 0x07, 0x00, 0x20, 0x00,
    0x41, 0x01, 0x6A, 0x0B, 0x04, 0x00, 0xD2, 0x01, 0x0B, 0x04, 0x00, 0xD2, 0x00, 0x0B, 0x06, 0x00,
    0x41, 0x00, 0x25, 0x00, 0x0B, 0x25, 0x01, 0x02, 0x7F, 0x03, 0x40, 0x20, 0x02, 0xD2, 0x01, 0x14,
    0x00, 0x21, 0x02, 0x20, 0x02, 0xD2, 0x00, 0x14, 0x00, 0x21, 0x02, 0x20, 0x01, 0x41, 0x01, 0x6A,
    0x22, 0x01, 0x20, 0x00, 0x49, 0x0D, 0x00, 0x0B, 0x20, 0x02, 0x0B,
};

static wasm_error_t wasm_test_twice(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                    wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    results[0] = wasm_i32((int32_t)((uint32_t)args[0].of.i32 * 2u));
    return WASM_OK;
}

TEST(test_ref_func_returns_one_reference_per_function) {
    static const char* const names[] = { "inc", "twice", "table" };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_module_t* target = NULL;
    wasm_value_t refs[3];
    wasm_value_t again;
    wasm_value_t arg;
    wasm_value_t result;
    uint32_t func_idx = UINT32_MAX;
    uint32_t i;

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "host", "twice", "i(i)", wasm_test_twice, NULL));
    m = wasm_test_load_sections(&rt, wasm_test_ref_func_sections, sizeof(wasm_test_ref_func_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(names); i++) {
        WASM_CHECK_OK(t, wasm_call(m, names[i], NULL, 0, &refs[i], 1));
        WASM_CHECK_OK(t, wasm_call(m, names[i], NULL, 0, &again, 1));
        CHECK_MSG(t, refs[i].of.funcref != UINT32_MAX && again.of.funcref == refs[i].of.funcref,
                  "%s returned references %u and %u", names[i], (unsigned)refs[i].of.funcref, (unsigned)again.of.funcref);
    }
    CHECK(t, refs[0].of.funcref == refs[2].of.funcref && refs[0].of.funcref != refs[1].of.funcref);
    WASM_CHECK_OK(t, wasm_func_ref_target(&rt, refs[0], &target, &func_idx));
    CHECK(t, target == m && func_idx == 1);
    WASM_CHECK_OK(t, wasm_func_ref_target(&rt, refs[1], &target, &func_idx));
    CHECK(t, target == m && func_idx == 0);
    arg = wasm_i32(5);
    WASM_CHECK_OK(t, wasm_call(m, "loop", &arg, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 62);
    result = wasm_i32(0);
    wasm_test_run_in_fuel_steps(t, m, "loop", &arg, 1, 1, 0, &result);
    WASM_CHECK_I32(t, result.of.i32, 62);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_interpreter_cases[] = {
    { "i32.add(3, 7) == 10", test_add },
    { "factorial(10) == 3628800", test_factorial },
    { "loop: sum(1..10) == 55", test_loop },
    { "loop type index preserves block params on br_if", test_loop_type_index_block },
    { "sign-extension ops: i32/i64 extend*_s", test_sign_extension_ops },
    { "multivalue if: type-index block returns two values", test_multivalue_if_block },
    { "multivalue br: block branch preserves two results", test_multivalue_branch_block },
    { "multivalue loop: br 0 preserves loop params", test_multivalue_loop_params },
    { "float copies/sign: signaling NaN payloads", test_float_signaling_nan_bits },
    { "rotates: zero-count paths avoid shift-width UB", test_rotate_by_zero },
    { "wide arithmetic: rejected when disabled, 128-bit products when enabled", test_wide_arithmetic_feature_gate },
    { "host import: results must match the declared type", test_host_results_must_match_the_import_type },
    { "f64 arithmetic: a NaN result takes the first NaN operand's payload", test_f64_arithmetic_propagates_the_first_nan_operand },
    { "trap on i32.div_s by zero", test_div_by_zero },
    { "saturating truncations: NaNs, infinities, and out-of-range values saturate", test_trunc_sat_saturates_every_edge_value },
    { "ref.func: returns one reference per function, cached or not", test_ref_func_returns_one_reference_per_function },
};
const test_suite exec_interpreter_suite = { "exec.interpreter", exec_interpreter_cases, COUNTOF(exec_interpreter_cases) };

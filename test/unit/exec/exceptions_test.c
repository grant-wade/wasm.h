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

/* (module
     (type $s (struct (field i32)))
     (type $box (struct (field (mut exnref))))
     (type $f (func (result i32)))
     (type $unit (func))
     (type $k (cont $unit))
     (import "env" "gc" (func $gc))
     (import "env" "arm" (func $arm))
     (import "env" "barrier" (func $barrier))
     (import "env" "interrupt" (func $interrupt))
     (import "env" "host-exn" (func $host-exn (result exnref)))
     (tag $e (param i32))
     (tag $sref (param (ref null $s)))
     (tag $yield)
     (tag $all (param i32 i64 f32 f64 v128 funcref externref (ref null $s) i31ref exnref))
     (tag $big (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32
                      (ref null $s) v128 (ref null $s)))
     (global $g (export "g") (mut exnref) (ref.null exn))
     (global $seen (mut i32) (i32.const 0))
     (table $t 2 exnref)
     (elem declare func $seven $cbody $waiter)
     (func $seven (result i32) (i32.const 7))
     (func $exn (param $v i32) (result exnref)
       (block $h (result exnref)
         (try_table (catch_all_ref $h) (throw $e (local.get $v)))
         (unreachable)))
     (func $unwrap (param $x exnref) (result i32)
       (block $h (result i32)
         (try_table (catch $e $h) (throw_ref (local.get $x)))
         (unreachable)))
     (func (export "all-kinds") (param $x externref) (result i32 externref)
       (local $a i32) (local $b i64) (local $c f32) (local $d f64) (local $v v128) (local $fn funcref)
       (local $ext externref) (local $st (ref null $s)) (local $i (ref null i31)) (local $inner exnref)
       (local $code i32)
       (block $done (result i32 i64 f32 f64 v128 funcref externref (ref null $s) i31ref exnref)
         (try_table (catch $all $done)
           (block $second (result i32 i64 f32 f64 v128 funcref externref (ref null $s) i31ref exnref exnref)
             (try_table (catch_ref $all $second)
               (block $first (result i32 i64 f32 f64 v128 funcref externref (ref null $s) i31ref exnref exnref)
                 (try_table (catch_ref $all $first)
                   (call $exn (i32.const 99))
                   (local.set $inner)
                   (call $arm)
                   (throw $all (i32.const 1) (i64.const 0x123456789) (f32.const 2.5) (f64.const -3.25)
                               (v128.const i32x4 1 2 3 4) (ref.func $seven) (local.get $x)
                               (struct.new $s (i32.const 41)) (ref.i31 (i32.const 5)) (local.get $inner)))
                 (unreachable))
               (call $gc)
               (call $arm)
               (throw_ref))
             (unreachable))
           (call $gc)
           (throw_ref))
         (unreachable))
       (local.set $inner) (local.set $i) (local.set $st) (local.set $ext) (local.set $fn) (local.set $v)
       (local.set $d) (local.set $c) (local.set $b) (local.set $a)
       (call $gc)
       (if (i32.eq (local.get $a) (i32.const 1)) (then (local.set $code (i32.or (local.get $code) (i32.const 1)))))
       (if (i64.eq (local.get $b) (i64.const 0x123456789)) (then (local.set $code (i32.or (local.get $code) (i32.const 2)))))
       (if (f32.eq (local.get $c) (f32.const 2.5)) (then (local.set $code (i32.or (local.get $code) (i32.const 4)))))
       (if (f64.eq (local.get $d) (f64.const -3.25)) (then (local.set $code (i32.or (local.get $code) (i32.const 8)))))
       (if (i32x4.all_true (i32x4.eq (local.get $v) (v128.const i32x4 1 2 3 4)))
         (then (local.set $code (i32.or (local.get $code) (i32.const 16)))))
       (if (i32.eq (call_ref $f (ref.cast (ref $f) (local.get $fn))) (i32.const 7))
         (then (local.set $code (i32.or (local.get $code) (i32.const 32)))))
       (if (i32.eq (struct.get $s 0 (local.get $st)) (i32.const 41))
         (then (local.set $code (i32.or (local.get $code) (i32.const 64)))))
       (if (i32.eq (i31.get_s (local.get $i)) (i32.const 5))
         (then (local.set $code (i32.or (local.get $code) (i32.const 128)))))
       (if (i32.eq (call $unwrap (local.get $inner)) (i32.const 99))
         (then (local.set $code (i32.or (local.get $code) (i32.const 256)))))
       (local.get $code)
       (local.get $ext))
   
     (func $sum17 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32
                         (ref null $s) v128 (ref null $s)) (result i32)
       (i32.add (i32.add (i32.add (i32.add (local.get 0) (local.get 1)) (i32.add (local.get 2) (local.get 3)))
                         (i32.add (i32.add (local.get 4) (local.get 5)) (i32.add (local.get 6) (local.get 7))))
                (i32.add (i32.add (i32.add (local.get 8) (local.get 9)) (i32.add (local.get 10) (local.get 11)))
                         (i32.add (i32.add (local.get 12) (local.get 13))
                                  (i32.add (i32.add (local.get 14) (local.get 15))
                                           (i32.add (local.get 16)
                                                    (i32.add (i32.mul (struct.get $s 0 (local.get 17)) (i32.const 1000))
                                                             (i32.add (i32x4.extract_lane 3 (local.get 18))
                                                                      (i32.mul (struct.get $s 0 (local.get 19))
                                                                               (i32.const 100000))))))))))
     (func $throw-big
       (throw $big (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6)
                   (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12)
                   (i32.const 13) (i32.const 14) (i32.const 15) (i32.const 16) (i32.const 17)
                   (struct.new $s (i32.const 3)) (v128.const i32x4 0 0 0 50) (struct.new $s (i32.const 4))))
     (func (export "big-ref") (result i32)
       (drop (call $unwrap (call $exn (i32.const 1))))
       (block $done (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32
                            (ref null $s) v128 (ref null $s))
         (try_table (catch $big $done)
           (block $h (result exnref)
             (try_table (catch_all_ref $h) (call $arm) (call $throw-big))
             (unreachable))
           (call $gc)
           (throw_ref))
         (unreachable))
       (call $sum17))
     (func (export "big-legacy") (result i32)
       try (result i32)
         try
           (call $throw-big)
         catch $big
           (call $gc)
           (drop (call $sum17))
           (call $gc)
           rethrow 0
         end
         unreachable
       catch $big
         (call $gc)
         (call $sum17)
       end)
   
     (func (export "nested") (result i32) (local $acc i32)
       (block $out (result i32)
         (try_table (catch $e $out)
           try
             (throw $e (i32.const 1))
           catch $e
             (local.set $acc)
             try
               (throw $e (i32.const 10))
             catch $e
               (local.set $acc (i32.add (local.get $acc)))
               (call $gc)
               rethrow 1
             end
           end)
         (i32.const -1))
       (i32.add (local.get $acc) (i32.mul (i32.const 100))))
     (func (export "reentrant") (result i32)
       (block $outer (result i32)
         (try_table (catch $e $outer)
           (block $inner (result i32)
             (try_table (catch $e $inner) (throw $e (i32.const 5)))
             (unreachable))
           (throw $e (i32.mul (i32.const 3))))
         (unreachable)))
     (func $catch-inside (result i32)
       try (result i32) (throw $e (i32.const 20)) catch $e end)
     (func (export "reentrant-legacy") (result i32)
       (block $out (result i32)
         (try_table (catch $e $out)
           try
             (throw $e (i32.const 2))
           catch $e
             (drop)
             (drop (call $catch-inside))
             rethrow 0
           end)
         (unreachable)))
     (func (export "delegate") (result i32)
       try (result i32)
         try (result i32)
           (throw $e (i32.const 3))
         delegate 0
       catch $e
         (i32.add (i32.const 30))
       end)
     (func (export "legacy-catch-all") (result i32)
       (block $out (result (ref null $s))
         (try_table (catch $sref $out)
           try
             (throw $sref (struct.new $s (i32.const 8)))
           catch_all
             (call $gc)
             rethrow 0
           end)
         (unreachable))
       (struct.get $s 0))
   
     (func $cbody (throw $sref (struct.new $s (i32.const 77))))
     (func $waiter
       (block $h (result (ref null $s))
         (try_table (catch $sref $h) (suspend $yield))
         (return))
       (global.set $seen (struct.get $s 0)))
     (func (export "cont-throw") (result i32) (local $x exnref) (local $k2 (ref null $k))
       (block $h (result (ref null $s) exnref)
         (try_table (catch_ref $sref $h)
           (call $arm)
           (resume $k (cont.new $k (ref.func $cbody))))
         (unreachable))
       (local.set $x)
       (drop)
       (call $gc)
       (block $on (result (ref $k))
         (resume $k (on $yield $on) (cont.new $k (ref.func $waiter)))
         (unreachable))
       (local.set $k2)
       (call $arm)
       (resume_throw_ref $k (local.get $x) (local.get $k2))
       (global.get $seen))
   
     (func (export "store") (param $n i32) (result i32) (local $i i32) (local $acc i32) (local $b (ref null $box))
       (global.set $g (call $exn (i32.const 6)))
       (local.set $b (struct.new $box (global.get $g)))
       (table.set $t (i32.const 0) (global.get $g))
       (loop $l
         (block $h (result exnref)
           (try_table (catch_all_ref $h)
             (throw_ref (if (result exnref) (i32.and (local.get $i) (i32.const 1))
                          (then (struct.get $box 0 (local.get $b)))
                          (else (table.get $t (i32.const 0))))))
           (unreachable))
         (global.set $g)
         (struct.set $box 0 (local.get $b) (global.get $g))
         (table.set $t (i32.const 0) (global.get $g))
         (local.set $acc (i32.add (local.get $acc) (call $unwrap (global.get $g))))
         (local.set $i (i32.add (local.get $i) (i32.const 1)))
         (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
       (local.get $acc))
     (func (export "get-g") (result exnref) (global.get $g))
     (func (export "make") (param $v i32) (result exnref) (call $exn (local.get $v)))
     (func (export "unwrap") (param $x exnref) (result i32) (call $unwrap (local.get $x)))
     (func (export "rethrow-host") (param $x exnref) (result i32) (throw_ref (local.get $x)))
     (func (export "throw") (param $v i32) (result i32) (throw $e (local.get $v)))
     (func (export "from-host") (result i32) (call $unwrap (call $host-exn)))
     (func (export "barrier") (result i32)
       (block $out (result i32)
         (try_table (catch $e $out)
           try
             (throw $e (i32.const 4))
           catch $e
             (drop)
             (call $barrier)
             rethrow 0
           end)
         (unreachable)))
     (func (export "interrupt-catch") (param $n i32) (result i32) (local $acc i32)
       (i32.const 0)
       (loop $l (param i32)
         (local.set $acc (i32.add (local.get $acc)))
         (if (i32.lt_u (local.get $acc) (local.get $n))
           (then
             (try_table (catch $e $l)
               (call $interrupt)
               (throw $e (i32.const 1))))))
       (local.get $acc))
   
     (func (export "loop-catch") (param $n i32) (result i32) (local $i i32) (local $acc i32)
       (loop $l
         (local.set $acc (i32.add (local.get $acc)
           (block $h (result i32)
             (try_table (catch $e $h) (throw $e (local.get $i)))
             (unreachable))))
         (local.set $i (i32.add (local.get $i) (i32.const 1)))
         (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
       (local.get $acc))
     (func (export "loop-legacy") (param $n i32) (result i32) (local $i i32) (local $acc i32)
       (loop $l
         (local.set $acc (i32.add (local.get $acc)
           try (result i32)
             try (result i32)
               (throw $e (local.get $i))
             catch $e
               (drop)
               rethrow 0
             end
           catch_all
             (i32.const 1)
           end))
         (local.set $i (i32.add (local.get $i) (i32.const 1)))
         (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
       (local.get $acc))
     (func (export "loop-ref") (param $n i32) (result i32) (local $i i32) (local $acc i32)
       (loop $l
         (local.set $acc (i32.add (local.get $acc)
           (call $unwrap
             (block $h (result exnref)
               (try_table (catch_all_ref $h) (throw $e (local.get $i)))
               (unreachable)))))
         (local.set $i (i32.add (local.get $i) (i32.const 1)))
         (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
       (local.get $acc))
     (func (export "loop-cont") (param $n i32) (result i32) (local $i i32)
       (loop $l
         (block $h (result (ref null $s))
           (try_table (catch $sref $h) (resume $k (cont.new $k (ref.func $cbody))))
           (unreachable))
         (drop)
         (local.set $i (i32.add (local.get $i) (i32.const 1)))
         (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
       (local.get $i))) */
static const uint8_t wasm_test_exn_bytes[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0xb1, 0x01, 0x13, 0x5f, 0x01, 0x7f, 0x00,
    0x5f, 0x01, 0x69, 0x01, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x00, 0x00, 0x5d, 0x03, 0x60, 0x00, 0x01,
    0x69, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x63, 0x00, 0x00, 0x60, 0x0a, 0x7f, 0x7e, 0x7d, 0x7c,
    0x7b, 0x70, 0x6f, 0x63, 0x00, 0x6c, 0x69, 0x00, 0x60, 0x14, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x63, 0x00, 0x7b, 0x63, 0x00,
    0x00, 0x60, 0x01, 0x7f, 0x01, 0x69, 0x60, 0x01, 0x69, 0x01, 0x7f, 0x60, 0x01, 0x6f, 0x02, 0x7f,
    0x6f, 0x60, 0x00, 0x0a, 0x7f, 0x7e, 0x7d, 0x7c, 0x7b, 0x70, 0x6f, 0x63, 0x00, 0x6c, 0x69, 0x60,
    0x00, 0x0b, 0x7f, 0x7e, 0x7d, 0x7c, 0x7b, 0x70, 0x6f, 0x63, 0x00, 0x6c, 0x69, 0x69, 0x60, 0x14,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x63, 0x00, 0x7b, 0x63, 0x00, 0x01, 0x7f, 0x60, 0x00, 0x14, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
    0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x63, 0x00, 0x7b, 0x63,
    0x00, 0x60, 0x00, 0x02, 0x63, 0x00, 0x69, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x41, 0x05, 0x03,
    0x65, 0x6e, 0x76, 0x02, 0x67, 0x63, 0x00, 0x03, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x61, 0x72, 0x6d,
    0x00, 0x03, 0x03, 0x65, 0x6e, 0x76, 0x07, 0x62, 0x61, 0x72, 0x72, 0x69, 0x65, 0x72, 0x00, 0x03,
    0x03, 0x65, 0x6e, 0x76, 0x09, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x00, 0x03,
    0x03, 0x65, 0x6e, 0x76, 0x08, 0x68, 0x6f, 0x73, 0x74, 0x2d, 0x65, 0x78, 0x6e, 0x00, 0x05, 0x03,
    0x1f, 0x1e, 0x02, 0x0a, 0x0b, 0x0c, 0x0f, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x02, 0x12, 0x05, 0x0a, 0x0b, 0x0b, 0x12, 0x02, 0x02, 0x12, 0x12, 0x12, 0x12, 0x12,
    0x04, 0x04, 0x01, 0x69, 0x00, 0x02, 0x0d, 0x0b, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x03, 0x00,
    0x08, 0x00, 0x09, 0x06, 0x0b, 0x02, 0x69, 0x01, 0xd0, 0x69, 0x0b, 0x7f, 0x01, 0x41, 0x00, 0x0b,
    0x07, 0x8c, 0x02, 0x17, 0x01, 0x67, 0x03, 0x00, 0x09, 0x61, 0x6c, 0x6c, 0x2d, 0x6b, 0x69, 0x6e,
    0x64, 0x73, 0x00, 0x08, 0x07, 0x62, 0x69, 0x67, 0x2d, 0x72, 0x65, 0x66, 0x00, 0x0b, 0x0a, 0x62,
    0x69, 0x67, 0x2d, 0x6c, 0x65, 0x67, 0x61, 0x63, 0x79, 0x00, 0x0c, 0x06, 0x6e, 0x65, 0x73, 0x74,
    0x65, 0x64, 0x00, 0x0d, 0x09, 0x72, 0x65, 0x65, 0x6e, 0x74, 0x72, 0x61, 0x6e, 0x74, 0x00, 0x0e,
    0x10, 0x72, 0x65, 0x65, 0x6e, 0x74, 0x72, 0x61, 0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x67, 0x61, 0x63,
    0x79, 0x00, 0x10, 0x08, 0x64, 0x65, 0x6c, 0x65, 0x67, 0x61, 0x74, 0x65, 0x00, 0x11, 0x10, 0x6c,
    0x65, 0x67, 0x61, 0x63, 0x79, 0x2d, 0x63, 0x61, 0x74, 0x63, 0x68, 0x2d, 0x61, 0x6c, 0x6c, 0x00,
    0x12, 0x0a, 0x63, 0x6f, 0x6e, 0x74, 0x2d, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x00, 0x15, 0x05, 0x73,
    0x74, 0x6f, 0x72, 0x65, 0x00, 0x16, 0x05, 0x67, 0x65, 0x74, 0x2d, 0x67, 0x00, 0x17, 0x04, 0x6d,
    0x61, 0x6b, 0x65, 0x00, 0x18, 0x06, 0x75, 0x6e, 0x77, 0x72, 0x61, 0x70, 0x00, 0x19, 0x0c, 0x72,
    0x65, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x2d, 0x68, 0x6f, 0x73, 0x74, 0x00, 0x1a, 0x05, 0x74, 0x68,
    0x72, 0x6f, 0x77, 0x00, 0x1b, 0x09, 0x66, 0x72, 0x6f, 0x6d, 0x2d, 0x68, 0x6f, 0x73, 0x74, 0x00,
    0x1c, 0x07, 0x62, 0x61, 0x72, 0x72, 0x69, 0x65, 0x72, 0x00, 0x1d, 0x0f, 0x69, 0x6e, 0x74, 0x65,
    0x72, 0x72, 0x75, 0x70, 0x74, 0x2d, 0x63, 0x61, 0x74, 0x63, 0x68, 0x00, 0x1e, 0x0a, 0x6c, 0x6f,
    0x6f, 0x70, 0x2d, 0x63, 0x61, 0x74, 0x63, 0x68, 0x00, 0x1f, 0x0b, 0x6c, 0x6f, 0x6f, 0x70, 0x2d,
    0x6c, 0x65, 0x67, 0x61, 0x63, 0x79, 0x00, 0x20, 0x08, 0x6c, 0x6f, 0x6f, 0x70, 0x2d, 0x72, 0x65,
    0x66, 0x00, 0x21, 0x09, 0x6c, 0x6f, 0x6f, 0x70, 0x2d, 0x63, 0x6f, 0x6e, 0x74, 0x00, 0x22, 0x09,
    0x07, 0x01, 0x03, 0x00, 0x03, 0x05, 0x13, 0x14, 0x0a, 0xf2, 0x09, 0x1e, 0x04, 0x00, 0x41, 0x07,
    0x0b, 0x10, 0x00, 0x02, 0x69, 0x1f, 0x40, 0x01, 0x03, 0x00, 0x20, 0x00, 0x08, 0x00, 0x0b, 0x00,
    0x0b, 0x0b, 0x10, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x0a, 0x0b,
    0x00, 0x0b, 0x0b, 0xd8, 0x02, 0x0b, 0x01, 0x7f, 0x01, 0x7e, 0x01, 0x7d, 0x01, 0x7c, 0x01, 0x7b,
    0x01, 0x70, 0x01, 0x6f, 0x01, 0x63, 0x00, 0x01, 0x6c, 0x01, 0x69, 0x01, 0x7f, 0x02, 0x0d, 0x1f,
    0x40, 0x01, 0x00, 0x03, 0x00, 0x02, 0x0e, 0x1f, 0x40, 0x01, 0x01, 0x03, 0x00, 0x02, 0x0e, 0x1f,
    0x40, 0x01, 0x01, 0x03, 0x00, 0x41, 0xe3, 0x00, 0x10, 0x06, 0x21, 0x0a, 0x10, 0x01, 0x41, 0x01,
    0x42, 0x89, 0xcf, 0x95, 0x9a, 0x12, 0x43, 0x00, 0x00, 0x20, 0x40, 0x44, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0a, 0xc0, 0xfd, 0x0c, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xd2, 0x05, 0x20, 0x00, 0x41, 0x29, 0xfb, 0x00, 0x00, 0x41,
    0x05, 0xfb, 0x1c, 0x20, 0x0a, 0x08, 0x03, 0x0b, 0x00, 0x0b, 0x10, 0x00, 0x10, 0x01, 0x0a, 0x0b,
    0x00, 0x0b, 0x10, 0x00, 0x0a, 0x0b, 0x00, 0x0b, 0x21, 0x0a, 0x21, 0x09, 0x21, 0x08, 0x21, 0x07,
    0x21, 0x06, 0x21, 0x05, 0x21, 0x04, 0x21, 0x03, 0x21, 0x02, 0x21, 0x01, 0x10, 0x00, 0x20, 0x01,
    0x41, 0x01, 0x46, 0x04, 0x40, 0x20, 0x0b, 0x41, 0x01, 0x72, 0x21, 0x0b, 0x0b, 0x20, 0x02, 0x42,
    0x89, 0xcf, 0x95, 0x9a, 0x12, 0x51, 0x04, 0x40, 0x20, 0x0b, 0x41, 0x02, 0x72, 0x21, 0x0b, 0x0b,
    0x20, 0x03, 0x43, 0x00, 0x00, 0x20, 0x40, 0x5b, 0x04, 0x40, 0x20, 0x0b, 0x41, 0x04, 0x72, 0x21,
    0x0b, 0x0b, 0x20, 0x04, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0xc0, 0x61, 0x04, 0x40,
    0x20, 0x0b, 0x41, 0x08, 0x72, 0x21, 0x0b, 0x0b, 0x20, 0x05, 0xfd, 0x0c, 0x01, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xfd, 0x37, 0xfd, 0xa3,
    0x01, 0x04, 0x40, 0x20, 0x0b, 0x41, 0x10, 0x72, 0x21, 0x0b, 0x0b, 0x20, 0x06, 0xfb, 0x16, 0x02,
    0x14, 0x02, 0x41, 0x07, 0x46, 0x04, 0x40, 0x20, 0x0b, 0x41, 0x20, 0x72, 0x21, 0x0b, 0x0b, 0x20,
    0x08, 0xfb, 0x02, 0x00, 0x00, 0x41, 0x29, 0x46, 0x04, 0x40, 0x20, 0x0b, 0x41, 0xc0, 0x00, 0x72,
    0x21, 0x0b, 0x0b, 0x20, 0x09, 0xfb, 0x1d, 0x41, 0x05, 0x46, 0x04, 0x40, 0x20, 0x0b, 0x41, 0x80,
    0x01, 0x72, 0x21, 0x0b, 0x0b, 0x20, 0x0a, 0x10, 0x07, 0x41, 0xe3, 0x00, 0x46, 0x04, 0x40, 0x20,
    0x0b, 0x41, 0x80, 0x02, 0x72, 0x21, 0x0b, 0x0b, 0x20, 0x0b, 0x20, 0x07, 0x0b, 0x51, 0x00, 0x20,
    0x00, 0x20, 0x01, 0x6a, 0x20, 0x02, 0x20, 0x03, 0x6a, 0x6a, 0x20, 0x04, 0x20, 0x05, 0x6a, 0x20,
    0x06, 0x20, 0x07, 0x6a, 0x6a, 0x6a, 0x20, 0x08, 0x20, 0x09, 0x6a, 0x20, 0x0a, 0x20, 0x0b, 0x6a,
    0x6a, 0x20, 0x0c, 0x20, 0x0d, 0x6a, 0x20, 0x0e, 0x20, 0x0f, 0x6a, 0x20, 0x10, 0x20, 0x11, 0xfb,
    0x02, 0x00, 0x00, 0x41, 0xe8, 0x07, 0x6c, 0x20, 0x12, 0xfd, 0x1b, 0x03, 0x20, 0x13, 0xfb, 0x02,
    0x00, 0x00, 0x41, 0xa0, 0x8d, 0x06, 0x6c, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x6a, 0x0b, 0x42,
    0x00, 0x41, 0x01, 0x41, 0x02, 0x41, 0x03, 0x41, 0x04, 0x41, 0x05, 0x41, 0x06, 0x41, 0x07, 0x41,
    0x08, 0x41, 0x09, 0x41, 0x0a, 0x41, 0x0b, 0x41, 0x0c, 0x41, 0x0d, 0x41, 0x0e, 0x41, 0x0f, 0x41,
    0x10, 0x41, 0x11, 0x41, 0x03, 0xfb, 0x00, 0x00, 0xfd, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x41, 0x04, 0xfb, 0x00, 0x00, 0x08,
    0x04, 0x0b, 0x27, 0x00, 0x41, 0x01, 0x10, 0x06, 0x10, 0x07, 0x1a, 0x02, 0x10, 0x1f, 0x40, 0x01,
    0x00, 0x04, 0x00, 0x02, 0x69, 0x1f, 0x40, 0x01, 0x03, 0x00, 0x10, 0x01, 0x10, 0x0a, 0x0b, 0x00,
    0x0b, 0x10, 0x00, 0x0a, 0x0b, 0x00, 0x0b, 0x10, 0x09, 0x0b, 0x1c, 0x00, 0x06, 0x7f, 0x06, 0x40,
    0x10, 0x0a, 0x07, 0x04, 0x10, 0x00, 0x10, 0x09, 0x1a, 0x10, 0x00, 0x09, 0x00, 0x0b, 0x00, 0x07,
    0x04, 0x10, 0x00, 0x10, 0x09, 0x0b, 0x0b, 0x34, 0x01, 0x01, 0x7f, 0x02, 0x7f, 0x1f, 0x40, 0x01,
    0x00, 0x00, 0x00, 0x06, 0x40, 0x41, 0x01, 0x08, 0x00, 0x07, 0x00, 0x21, 0x00, 0x06, 0x40, 0x41,
    0x0a, 0x08, 0x00, 0x07, 0x00, 0x20, 0x00, 0x6a, 0x21, 0x00, 0x10, 0x00, 0x09, 0x01, 0x0b, 0x0b,
    0x0b, 0x41, 0x7f, 0x0b, 0x20, 0x00, 0x41, 0xe4, 0x00, 0x6c, 0x6a, 0x0b, 0x21, 0x00, 0x02, 0x7f,
    0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x41, 0x05,
    0x08, 0x00, 0x0b, 0x00, 0x0b, 0x41, 0x03, 0x6c, 0x08, 0x00, 0x0b, 0x00, 0x0b, 0x0b, 0x0b, 0x00,
    0x06, 0x7f, 0x41, 0x14, 0x08, 0x00, 0x07, 0x00, 0x0b, 0x0b, 0x1c, 0x00, 0x02, 0x7f, 0x1f, 0x40,
    0x01, 0x00, 0x00, 0x00, 0x06, 0x40, 0x41, 0x02, 0x08, 0x00, 0x07, 0x00, 0x1a, 0x10, 0x0f, 0x1a,
    0x09, 0x00, 0x0b, 0x0b, 0x00, 0x0b, 0x0b, 0x12, 0x00, 0x06, 0x7f, 0x06, 0x7f, 0x41, 0x03, 0x08,
    0x00, 0x18, 0x00, 0x07, 0x00, 0x41, 0x1e, 0x6a, 0x0b, 0x0b, 0x21, 0x00, 0x02, 0x63, 0x00, 0x1f,
    0x40, 0x01, 0x00, 0x01, 0x00, 0x06, 0x40, 0x41, 0x08, 0xfb, 0x00, 0x00, 0x08, 0x01, 0x19, 0x10,
    0x00, 0x09, 0x00, 0x0b, 0x0b, 0x00, 0x0b, 0xfb, 0x02, 0x00, 0x00, 0x0b, 0x0a, 0x00, 0x41, 0xcd,
    0x00, 0xfb, 0x00, 0x00, 0x08, 0x01, 0x0b, 0x16, 0x00, 0x02, 0x63, 0x00, 0x1f, 0x40, 0x01, 0x00,
    0x01, 0x00, 0xe2, 0x02, 0x0b, 0x0f, 0x0b, 0xfb, 0x02, 0x00, 0x00, 0x24, 0x01, 0x0b, 0x3c, 0x02,
    0x01, 0x69, 0x01, 0x63, 0x04, 0x02, 0x11, 0x1f, 0x40, 0x01, 0x01, 0x01, 0x00, 0x10, 0x01, 0xd2,
    0x13, 0xe0, 0x04, 0xe3, 0x04, 0x00, 0x0b, 0x00, 0x0b, 0x21, 0x00, 0x1a, 0x10, 0x00, 0x02, 0x64,
    0x04, 0xd2, 0x14, 0xe0, 0x04, 0xe3, 0x04, 0x01, 0x00, 0x02, 0x00, 0x00, 0x0b, 0x21, 0x01, 0x10,
    0x01, 0x20, 0x00, 0x20, 0x01, 0xe5, 0x04, 0x00, 0x23, 0x01, 0x0b, 0x64, 0x02, 0x02, 0x7f, 0x01,
    0x63, 0x01, 0x41, 0x06, 0x10, 0x06, 0x24, 0x00, 0x23, 0x00, 0xfb, 0x00, 0x01, 0x21, 0x03, 0x41,
    0x00, 0x23, 0x00, 0x26, 0x00, 0x03, 0x40, 0x02, 0x69, 0x1f, 0x40, 0x01, 0x03, 0x00, 0x20, 0x01,
    0x41, 0x01, 0x71, 0x04, 0x69, 0x20, 0x03, 0xfb, 0x02, 0x01, 0x00, 0x05, 0x41, 0x00, 0x25, 0x00,
    0x0b, 0x0a, 0x0b, 0x00, 0x0b, 0x24, 0x00, 0x20, 0x03, 0x23, 0x00, 0xfb, 0x05, 0x01, 0x00, 0x41,
    0x00, 0x23, 0x00, 0x26, 0x00, 0x20, 0x02, 0x23, 0x00, 0x10, 0x07, 0x6a, 0x21, 0x02, 0x20, 0x01,
    0x41, 0x01, 0x6a, 0x21, 0x01, 0x20, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x20, 0x02, 0x0b,
    0x04, 0x00, 0x23, 0x00, 0x0b, 0x06, 0x00, 0x20, 0x00, 0x10, 0x06, 0x0b, 0x06, 0x00, 0x20, 0x00,
    0x10, 0x07, 0x0b, 0x05, 0x00, 0x20, 0x00, 0x0a, 0x0b, 0x06, 0x00, 0x20, 0x00, 0x08, 0x00, 0x0b,
    0x06, 0x00, 0x10, 0x04, 0x10, 0x07, 0x0b, 0x1b, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00,
    0x00, 0x06, 0x40, 0x41, 0x04, 0x08, 0x00, 0x07, 0x00, 0x1a, 0x10, 0x02, 0x09, 0x00, 0x0b, 0x0b,
    0x00, 0x0b, 0x0b, 0x25, 0x01, 0x01, 0x7f, 0x41, 0x00, 0x03, 0x06, 0x20, 0x01, 0x6a, 0x21, 0x01,
    0x20, 0x01, 0x20, 0x00, 0x49, 0x04, 0x40, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x01, 0x10, 0x03, 0x41,
    0x01, 0x08, 0x00, 0x0b, 0x0b, 0x0b, 0x20, 0x01, 0x0b, 0x2b, 0x01, 0x02, 0x7f, 0x03, 0x40, 0x20,
    0x02, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x20, 0x01, 0x08, 0x00, 0x0b, 0x00, 0x0b,
    0x6a, 0x21, 0x02, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x20, 0x01, 0x20, 0x00, 0x49, 0x0d,
    0x00, 0x0b, 0x20, 0x02, 0x0b, 0x2e, 0x01, 0x02, 0x7f, 0x03, 0x40, 0x20, 0x02, 0x06, 0x7f, 0x06,
    0x7f, 0x20, 0x01, 0x08, 0x00, 0x07, 0x00, 0x1a, 0x09, 0x00, 0x0b, 0x19, 0x41, 0x01, 0x0b, 0x6a,
    0x21, 0x02, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x20, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00,
    0x0b, 0x20, 0x02, 0x0b, 0x2c, 0x01, 0x02, 0x7f, 0x03, 0x40, 0x20, 0x02, 0x02, 0x69, 0x1f, 0x40,
    0x01, 0x03, 0x00, 0x20, 0x01, 0x08, 0x00, 0x0b, 0x00, 0x0b, 0x10, 0x07, 0x6a, 0x21, 0x02, 0x20,
    0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x20, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x20, 0x02,
    0x0b, 0x2b, 0x01, 0x01, 0x7f, 0x03, 0x40, 0x02, 0x63, 0x00, 0x1f, 0x40, 0x01, 0x00, 0x01, 0x00,
    0xd2, 0x13, 0xe0, 0x04, 0xe3, 0x04, 0x00, 0x0b, 0x00, 0x0b, 0x1a, 0x20, 0x01, 0x41, 0x01, 0x6a,
    0x21, 0x01, 0x20, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0x0b,
};

static wasm_value_t wasm_test_exn_host_value;

static wasm_error_t wasm_test_exn_collect(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_gc_collect(wasm_module_runtime(mod));
    return WASM_OK;
}

static wasm_error_t wasm_test_exn_force_next_collection(wasm_module_t* mod, const wasm_value_t* args,
                                                        uint32_t num_args, wasm_value_t* results,
                                                        uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_module_runtime(mod)->exns.new_bytes = (size_t)-1 / 2u;
    return WASM_OK;
}

static wasm_error_t wasm_test_exn_barrier(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_value_t arg = wasm_i32(7);
    wasm_value_t result = wasm_i32(0);

    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    if (wasm_call(mod, "throw", &arg, 1, &result, 1) != WASM_ERR_TRAP) return WASM_ERR_MALFORMED;
    arg = wasm_i32(50);
    if (wasm_call(mod, "loop-ref", &arg, 1, &result, 1) != WASM_OK || result.of.i32 != 1225) return WASM_ERR_MALFORMED;
    if (wasm_call(mod, "loop-legacy", &arg, 1, &result, 1) != WASM_OK || result.of.i32 != 50) return WASM_ERR_MALFORMED;
    return WASM_OK;
}

static wasm_error_t wasm_test_exn_interrupt(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                            wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_runtime_interrupt(wasm_module_runtime(mod));
    return WASM_OK;
}

static wasm_error_t wasm_test_exn_host_exn(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    results[0] = wasm_test_exn_host_value;
    return WASM_OK;
}

static void wasm_test_exn_bind(wasm_runtime_t* rt, const char* name, wasm_host_func_t func) {
    wasm_import_t imp;

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = name;
    imp.func = func;
    wasm_register_import(rt, &imp);
}

static wasm_module_t* wasm_test_exn_load(test_ctx* t, wasm_runtime_t* rt) {
    wasm_module_t* m;

    wasm_test_exn_bind(rt, "gc", wasm_test_exn_collect);
    wasm_test_exn_bind(rt, "arm", wasm_test_exn_force_next_collection);
    wasm_test_exn_bind(rt, "barrier", wasm_test_exn_barrier);
    wasm_test_exn_bind(rt, "interrupt", wasm_test_exn_interrupt);
    wasm_test_exn_bind(rt, "host-exn", wasm_test_exn_host_exn);
    m = wasm_load(rt, wasm_test_exn_bytes, sizeof(wasm_test_exn_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt->error_msg);
    return m;
}

static int32_t wasm_test_exn_call_i32(test_ctx* t, wasm_module_t* m, const char* name, const wasm_value_t* arg) {
    wasm_value_t result = wasm_i32(0);
    wasm_error_t err = wasm_call(m, name, arg, arg ? 1u : 0u, &result, 1);

    CHECK_MSG(t, err == WASM_OK, "%s: %s", name, wasm_module_runtime(m)->error_msg);
    return result.of.i32;
}

static void wasm_test_exn_expect(test_ctx* t, wasm_module_t* m, const char* name, int32_t arg, int32_t expected) {
    wasm_value_t value = wasm_i32(arg);
    int32_t actual = wasm_test_exn_call_i32(t, m, name, &value);

    CHECK_MSG(t, actual == expected, "%s(%d): expected %d, got %d", name, (int)arg, (int)expected, (int)actual);
}

TEST(test_exception_payload_kinds_survive_collections) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t ext = wasm_externref((uintptr_t)0x5A5A50u);
    wasm_value_t results[2];
    wasm_error_t err;
    int round;

    wasm_init(&rt);
    m = wasm_test_exn_load(t, &rt);
    for (round = 0; m && round < 3; round++) {
        err = wasm_call(m, "all-kinds", &ext, 1, results, 2);
        CHECK_MSG(t, err == WASM_OK, "%s", rt.error_msg);
        if (err != WASM_OK) break;
        CHECK_MSG(t, results[0].of.i32 == 0x1FF, "payload checks 0x%x of 0x1ff passed", (unsigned)results[0].of.i32);
        CHECK(t, results[1].type == WASM_TYPE_EXTERNREF && results[1].of.externref == ext.of.externref);
        CHECK_MSG(t, rt.exns.new_bytes < (size_t)1 << 20, "%s", "expected the forced collections to run");
        CHECK(t, !rt.pending_exception.is_pending);
    }
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.exns.live == 0u && rt.held_exns.live == 0u, "%u exnrefs and %u caught exceptions left",
              (unsigned)rt.exns.live, (unsigned)rt.held_exns.live);
    if (m) wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_exception_payload_larger_than_any_before) {
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_test_exn_load(t, &rt);
    if (m) {
        uint32_t cap;

        wasm_test_exn_expect(t, m, "loop-catch", 10, 45);
        cap = rt.pending_exception.buffer_cap;
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "big-ref", NULL), 403203);
        CHECK_MSG(t, rt.pending_exception.buffer_cap >= 21u && rt.pending_exception.buffer_cap > cap,
                  "expected the payload buffer to grow from %u slots, has %u", (unsigned)cap,
                  (unsigned)rt.pending_exception.buffer_cap);
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "big-legacy", NULL), 403203);
        wasm_test_exn_expect(t, m, "loop-catch", 10, 45);
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "big-ref", NULL), 403203);
        CHECK(t, rt.held_exns.live == 0u);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_exception_nested_and_reentrant_throws) {
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_test_exn_load(t, &rt);
    if (m) {
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "nested", NULL), 1101);
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "reentrant", NULL), 15);
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "reentrant-legacy", NULL), 2);
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "delegate", NULL), 33);
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "legacy-catch-all", NULL), 8);
        wasm_test_exn_expect(t, m, "loop-legacy", 100, 100);
        CHECK_MSG(t, rt.held_exns.live == 0u && rt.held_exns.count <= 4u,
                  "expected caught exceptions to be released and reused, %u live of %u", (unsigned)rt.held_exns.live,
                  (unsigned)rt.held_exns.count);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_exception_crosses_continuations_and_host_barriers) {
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_test_exn_load(t, &rt);
    if (m) {
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "cont-throw", NULL), 77);
        wasm_test_exn_expect(t, m, "loop-cont", 1000, 1000);
        CHECK(t, rt.num_cont_stacks == 0u && rt.exec == &rt.main_stack);
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "barrier", NULL), 4);
        CHECK(t, !rt.pending_exception.is_pending && rt.held_exns.live == 0u);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_exception_uncaught_reaches_host) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(3);
    wasm_value_t exn;
    wasm_value_t result;
    wasm_error_t err;

    wasm_init(&rt);
    m = wasm_test_exn_load(t, &rt);
    if (m) {
        err = wasm_call(m, "throw", &arg, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_TRAP && strstr(rt.error_msg, "uncaught exception tag") != NULL, "%d: %s",
                  (int)err, rt.error_msg);
        CHECK(t, !rt.pending_exception.is_pending);
        wasm_test_exn_expect(t, m, "loop-catch", 4, 6);
        WASM_CHECK_OK(t, wasm_call(m, "make", &arg, 1, &exn, 1));
        CHECK(t, exn.type == WASM_TYPE_EXNREF && exn.of.externref != 0u);
        err = wasm_call(m, "rethrow-host", &exn, 1, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_TRAP && strstr(rt.error_msg, "uncaught exception tag") != NULL, "%d: %s",
                  (int)err, rt.error_msg);
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "unwrap", &exn), 3);
        exn = wasm_ref_null(WASM_TYPE_EXNREF);
        CHECK(t, wasm_call(m, "rethrow-host", &exn, 1, &result, 1) == WASM_ERR_TRAP);
        CHECK(t, !rt.pending_exception.is_pending);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_exception_exnref_rethrown_and_stored) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t exn;

    wasm_init(&rt);
    m = wasm_test_exn_load(t, &rt);
    if (m) {
        wasm_test_exn_expect(t, m, "store", 5000, 30000);
        CHECK_MSG(t, rt.exns.count <= 2u, "expected rethrown exnrefs to keep their entry, %u used",
                  (unsigned)rt.exns.count);
        wasm_gc_collect(&rt);
        WASM_CHECK_OK(t, wasm_call(m, "get-g", NULL, 0, &exn, 1));
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "unwrap", &exn), 6);
        wasm_test_exn_expect(t, m, "store", 10, 60);
        wasm_gc_collect(&rt);
        CHECK_MSG(t, rt.exns.live == 1u, "expected only the stored exnref to stay, %u live", (unsigned)rt.exns.live);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_exception_host_exnrefs_must_be_live_and_local) {
    wasm_runtime_t rt;
    wasm_runtime_t other;
    wasm_module_t* m;
    wasm_module_t* foreign;
    wasm_value_t arg = wasm_i32(5);
    wasm_value_t exn;
    wasm_value_t stale;
    wasm_value_t forged[6];
    wasm_value_t* held;
    wasm_value_t result;
    uint64_t fake[4] = { 0 };
    uint32_t i;

    wasm_init(&rt);
    m = wasm_test_exn_load(t, &rt);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(m, "make", &arg, 1, &exn, 1));
    held = wasm_ref_hold(&rt, exn);
    CHECK(t, held != NULL);
    for (i = 0; i < COUNTOF(forged); i++) forged[i] = exn;
    forged[0].of.externref += (uintptr_t)1u << WASM__CONT_GEN_SHIFT;
    forged[1].of.externref += (uintptr_t)WASM__EXN_BLOCK_ENTRIES << 1;
    forged[2].of.externref = (exn.of.externref & (((uintptr_t)1u << WASM__CONT_TAG_SHIFT) - 1u)) |
                             (((exn.of.externref >> WASM__CONT_TAG_SHIFT) % WASM__CONT_TAG_MAX + 1u) << WASM__CONT_TAG_SHIFT);
    forged[3].of.externref |= (uintptr_t)1u;
    forged[4].of.externref = (uintptr_t)fake;
    forged[5].of.externref = (uintptr_t)2u;
    for (i = 0; i < COUNTOF(forged); i++) {
        wasm_value_t* held_forged;
        wasm_error_t err = wasm_call(m, "unwrap", &forged[i], 1, &result, 1);

        CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "forged exnref %u: expected a type mismatch, got %d (%s)",
                  (unsigned)i, (int)err, rt.error_msg);
        CHECK(t, wasm_global_set(m, "g", forged[i]) == WASM_ERR_TYPE_MISMATCH);
        CHECK(t, wasm_table_set(m, 0, 0, forged[i]) == WASM_ERR_TYPE_MISMATCH);
        CHECK(t, wasm_table_grow(m, 0, 1, forged[i]) == -1);
        wasm_test_exn_host_value = forged[i];
        err = wasm_call(m, "from-host", NULL, 0, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH && strstr(rt.error_msg, "host function result 0") != NULL,
                  "forged host result %u: %d (%s)", (unsigned)i, (int)err, rt.error_msg);
        held_forged = wasm_ref_hold(&rt, forged[i]);
        CHECK(t, held_forged != NULL);
        wasm_gc_collect(&rt);
        wasm_ref_release(&rt, held_forged);
    }
    if (held) {
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "unwrap", held), 5);
        WASM_CHECK_OK(t, wasm_global_set(m, "g", *held));
        WASM_CHECK_OK(t, wasm_table_set(m, 0, 1, *held));
        wasm_test_exn_host_value = *held;
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "from-host", NULL), 5);
        wasm_ref_release(&rt, held);
    }

    arg = wasm_i32(8);
    WASM_CHECK_OK(t, wasm_call(m, "make", &arg, 1, &stale, 1));
    WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, m, "unwrap", &stale), 8);
    wasm_gc_collect(&rt);
    CHECK(t, wasm_call(m, "unwrap", &stale, 1, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    for (i = 0; i < 4u; i++) {
        WASM_CHECK_OK(t, wasm_call(m, "make", &arg, 1, &exn, 1));
        CHECK(t, exn.of.externref != stale.of.externref);
    }
    CHECK(t, wasm_call(m, "unwrap", &stale, 1, &result, 1) == WASM_ERR_TYPE_MISMATCH);

    WASM_CHECK_OK(t, wasm_test_init_raw(&other, NULL));
    foreign = wasm_test_exn_load(t, &other);
    if (foreign) {
        arg = wasm_i32(9);
        WASM_CHECK_OK(t, wasm_call(foreign, "make", &arg, 1, &exn, 1));
        WASM_CHECK_I32(t, wasm_test_exn_call_i32(t, foreign, "unwrap", &exn), 9);
        CHECK(t, wasm_call(m, "unwrap", &exn, 1, &result, 1) == WASM_ERR_TYPE_MISMATCH);
        CHECK(t, wasm_global_set(m, "g", exn) == WASM_ERR_TYPE_MISMATCH);
        wasm_module_free(foreign);
    }
    wasm__runtime_destroy(&other);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* fuel is the budget of each run; *out_stops receives how many runs ran out of it. */
static int32_t wasm_test_exn_call_fueled(test_ctx* t, wasm_runtime_t* rt, wasm_module_t* m, const char* name,
                                         int32_t arg, uint64_t fuel, uint32_t* out_stops) {
    wasm_value_t value = wasm_i32(arg);
    wasm_value_t result = wasm_i32(0);
    wasm_error_t err;
    uint32_t stops = 0;

    wasm_runtime_set_fuel(rt, fuel);
    err = wasm_call(m, name, &value, 1, &result, 1);
    while (err == WASM_ERR_OUT_OF_FUEL && wasm_runtime_suspended(rt) == WASM_ERR_OUT_OF_FUEL && stops < 1000000u) {
        stops++;
        wasm_gc_collect(rt);
        wasm_runtime_set_fuel(rt, fuel);
        err = wasm_runtime_resume(rt, NULL, 0, &result, 1);
    }
    wasm_runtime_disable_fuel(rt);
    CHECK_MSG(t, err == WASM_OK, "%s: %d (%s)", name, (int)err, rt->error_msg);
    *out_stops = stops;
    return result.of.i32;
}

TEST(test_exception_fuel_and_interrupt_stops_resume) {
    static const struct {
        const char* name;
        int32_t arg;
        int32_t expected;
    } cases[] = {
        { "loop-catch", 40, 780 }, { "loop-legacy", 40, 40 }, { "loop-ref", 40, 780 },
        { "store", 40, 240 },      { "loop-cont", 40, 40 },
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(25);
    wasm_value_t result = wasm_i32(0);
    wasm_error_t err;
    uint32_t interrupts = 0;
    uint32_t i;

    wasm_init(&rt);
    m = wasm_test_exn_load(t, &rt);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(cases); i++) {
        uint64_t fuel;

        for (fuel = 1; fuel <= 13; fuel += 3) {
            uint32_t stops = 0;
            int32_t actual = wasm_test_exn_call_fueled(t, &rt, m, cases[i].name, cases[i].arg, fuel, &stops);

            CHECK_MSG(t, actual == cases[i].expected, "%s with %u fuel per run: expected %d, got %d", cases[i].name,
                      (unsigned)fuel, (int)cases[i].expected, (int)actual);
            CHECK_MSG(t, stops > 0u, "%s: expected fuel stops", cases[i].name);
        }
    }

    err = wasm_call(m, "interrupt-catch", &arg, 1, &result, 1);
    while (err == WASM_ERR_INTERRUPTED && wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED && interrupts < 100u) {
        interrupts++;
        CHECK(t, !rt.pending_exception.is_pending);
        wasm_gc_collect(&rt);
        err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
    }
    WASM_CHECK_OK(t, err);
    WASM_CHECK_I32(t, result.of.i32, 25);
    CHECK_MSG(t, interrupts == 25u, "expected 25 interrupts in catch handlers, got %u", (unsigned)interrupts);
    CHECK(t, rt.held_exns.live == 0u);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_exception_steady_state_allocates_nothing) {
    static const char* const names[] = { "loop-catch", "loop-legacy", "store", "loop-cont" };
#ifdef WASM_GC_STRESS
    const int32_t loops = 2000;
    const int32_t sum = 1999000;
#else
    const int32_t loops = 200000;
    const int32_t sum = -1474936480;
#endif
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t entries;
    size_t live_blocks;
    uint32_t i;

    wasm_init(&rt);
    m = wasm_test_exn_load(t, &rt);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(names); i++) {
        wasm_value_t arg = wasm_i32(100);
        wasm_value_t result;
        size_t alloc_calls;

        WASM_CHECK_OK(t, wasm_call(m, names[i], &arg, 1, &result, 1));
        WASM_CHECK_OK(t, wasm_call(m, names[i], &arg, 1, &result, 1));
        wasm_gc_collect(&rt);
        alloc_calls = wasm_test_alloc_snapshot().alloc_calls;
        arg = wasm_i32(20000);
        WASM_CHECK_OK(t, wasm_call(m, names[i], &arg, 1, &result, 1));
#ifndef WASM_GC_STRESS
        CHECK_MSG(t, wasm_test_alloc_snapshot().alloc_calls == alloc_calls, "%s: %u allocations in steady state",
                  names[i], (unsigned)(wasm_test_alloc_snapshot().alloc_calls - alloc_calls));
#else
        (void)alloc_calls;
#endif
    }

    wasm_test_exn_expect(t, m, "loop-ref", loops, sum);
    entries = rt.exns.count;
    live_blocks = wasm_test_alloc_snapshot().live_blocks;
    for (i = 0; i < 4u; i++) wasm_test_exn_expect(t, m, "loop-ref", loops, sum);
    CHECK_MSG(t, rt.exns.count == entries && entries < (rt.gc_heap_size + 512u * 1024u) / sizeof(wasm__exception_object_t),
              "expected collections to bound the exception table, %u entries after %u", (unsigned)rt.exns.count,
              (unsigned)entries);
    CHECK_MSG(t, wasm_test_alloc_snapshot().live_blocks <= live_blocks + 16u, "live blocks grew from %u to %u",
              (unsigned)live_blocks, (unsigned)wasm_test_alloc_snapshot().live_blocks);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_exceptions_cases[] = {
    { "exceptions: typed catch receives payload values", test_exception_try_catch_payload },
    { "exceptions: catch_all handles unmatched tags", test_exception_catch_all },
    { "exceptions: rethrow forwards to outer catches", test_exception_rethrow },
    { "exceptions: delegate forwards to outer handlers", test_exception_delegate },
    { "exceptions: imports that throw reach handlers on every call path", test_exceptions_from_imports_reach_handlers_on_all_call_paths },
    { "exceptions: payloads of every kind survive collections in flight", test_exception_payload_kinds_survive_collections },
    { "exceptions: payloads larger than any before grow the buffer", test_exception_payload_larger_than_any_before },
    { "exceptions: nested and re-entrant throws", test_exception_nested_and_reentrant_throws },
    { "exceptions: exceptions cross continuations and host barriers", test_exception_crosses_continuations_and_host_barriers },
    { "exceptions: uncaught exceptions reach the host", test_exception_uncaught_reaches_host },
    { "exceptions: exnrefs rethrown many times and stored keep their entry", test_exception_exnref_rethrown_and_stored },
    { "exceptions: host exnrefs must be live exceptions of the runtime", test_exception_host_exnrefs_must_be_live_and_local },
    { "exceptions: fuel and interrupt stops around throw and catch resume", test_exception_fuel_and_interrupt_stops_resume },
    { "exceptions: throw and catch allocate nothing in steady state", test_exception_steady_state_allocates_nothing },
};
const test_suite exec_exceptions_suite = { "exec.exceptions", exec_exceptions_cases, COUNTOF(exec_exceptions_cases) };

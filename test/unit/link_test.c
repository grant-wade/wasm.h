#include "support.h"

TEST(test_host_import) {
    host_print_called = 0;
    host_print_value = 0;

    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    /* Type section: (i32) -> () */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2); /* 2 types */
        /* type 0: (i32) -> () for import */
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);
        /* type 1: () -> () for exported func */
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Import section: env.print */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 import */
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 5);
        emit(&sec, 'p');
        emit(&sec, 'r');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 0x00);         /* func import */
        emit_leb128_u32(&sec, 0); /* type index 0 */
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    /* Function section: 1 func (the exported one) */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1); /* type index 1: () -> () */
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export: "run" = func 1 (func 0 is the import) */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1); /* func index 1 */
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code section */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 42); /* i32.const 42 */
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0); /* call 0 (import) */
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;

    wasm_init(&rt);

    wasm_import_t imp;

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "print";
    imp.func = host_print;
    wasm_register_import(&rt, &imp);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, NULL, 0);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, host_print_called, "%s", "host func not called");
        if (host_print_called) {
            WASM_CHECK_I32(t, host_print_value, 42);
        }
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_imported_mutable_global) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_global_import_t imp;
    wasm_value_t imported_counter = wasm_i32(10);
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
        emit_leb128_u32(&sec, 7);
        emit(&sec, 'c');
        emit(&sec, 'o');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 'r');
        emit(&sec, 0x03);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);
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
        emit(&body, 0x23);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x6A);
        emit(&body, 0x24);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "counter";
    imp.type = WASM_TYPE_I32;
    imp.is_mutable = 1;
    imp.value = &imported_counter;

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

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 15);
        WASM_CHECK_I32(t, imported_counter.of.i32, 15);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module (type $fa (func)) (type $fb (func (result i64)))
     (import "host" "ta" (tag $ta (type $fa))) (import "host" "tb" (tag $tb (type $fb)))
     (type $ft (func (result i32))) (type $ct (cont $ft))
     (func $gen (result i32) (i32.wrap_i64 (suspend $tb))) (elem declare func $gen)
     (func (export "main") (param i32) (result i32)
       (block $h (result (ref $ct))
         (return (resume $ct (on $ta $h) (cont.new $ct (ref.func $gen)))))
       (resume $ct))) */
static const uint8_t wasm_test_cont_tag_alias_sections[] = {
    0x01, 0x13, 0x05, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7E, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x02,
    0x60, 0x01, 0x7F, 0x01, 0x7F, 0x02, 0x17, 0x02, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x02, 0x74, 0x61,
    0x04, 0x00, 0x00, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x02, 0x74, 0x62, 0x04, 0x00, 0x01, 0x03, 0x03,
    0x02, 0x02, 0x04, 0x07, 0x08, 0x01, 0x04, 0x6D, 0x61, 0x69, 0x6E, 0x00, 0x01, 0x09, 0x05, 0x01,
    0x03, 0x00, 0x01, 0x00, 0x0A, 0x1C, 0x02, 0x05, 0x00, 0xE2, 0x01, 0xA7, 0x0B, 0x14, 0x00, 0x02,
    0x64, 0x03, 0xD2, 0x00, 0xE0, 0x03, 0xE3, 0x03, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x0B, 0xE3, 0x03,
    0x00, 0x0B,
};

/* (module (tag (export "e") (param i32))) */
static const uint8_t wasm_test_tag_export_sections[] = {
    0x01, 0x05, 0x01, 0x60, 0x01, 0x7F, 0x00, 0x0D, 0x03, 0x01, 0x00, 0x00, 0x07, 0x05, 0x01, 0x01,
    0x65, 0x04, 0x00,
};

TEST(test_stack_switching_tag_identity_names_one_type) {
    wasm_valtype_t i32_type = WASM_TYPE_I32;
    wasm_valtype_t i64_type = WASM_TYPE_I64;
    wasm_reftype_t typed_ref;
    wasm_valtype_t ref_type = WASM_TYPE_STRUCTREF;
    wasm_runtime_t rt;
    wasm_tag_import_t tag;
    wasm_module_t* lib;
    wasm_module_t* m;
    uint64_t identity;

    wasm_init(&rt);
    memset(&tag, 0, sizeof(tag));
    tag.module = "host";
    tag.name = "ta";
    WASM_CHECK_OK(t, wasm_register_tag_import(&rt, &tag));
    identity = rt.tag_imports[rt.num_tag_imports - 1u].identity;

    tag.name = "tb";
    tag.identity = identity;
    tag.type.num_results = 1;
    tag.type.results = &i64_type;
    CHECK(t, wasm_register_tag_import(&rt, &tag) == WASM_ERR_TYPE_MISMATCH);
    tag.identity = rt.next_tag_identity + 100u;
    CHECK(t, wasm_register_tag_import(&rt, &tag) == WASM_ERR_MALFORMED);
    m = wasm_test_load_sections(&rt, wasm_test_cont_tag_alias_sections, sizeof(wasm_test_cont_tag_alias_sections));
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "host.tb") != NULL, "%s", rt.error_msg);
    wasm_module_free(m);

    tag.name = "ta2";
    tag.identity = identity;
    tag.type.num_results = 0;
    tag.type.results = NULL;
    WASM_CHECK_OK(t, wasm_register_tag_import(&rt, &tag));

    memset(&typed_ref, 0, sizeof(typed_ref));
    typed_ref.type = WASM_TYPE_STRUCTREF;
    typed_ref.has_type_index = 1;
    tag.name = "typed";
    tag.identity = 0;
    tag.type.num_params = 1;
    tag.type.params = &ref_type;
    tag.type.param_reftypes = &typed_ref;
    CHECK(t, wasm_register_tag_import(&rt, &tag) == WASM_ERR_MALFORMED);

    lib = wasm_test_load_sections(&rt, wasm_test_tag_export_sections, sizeof(wasm_test_tag_export_sections));
    CHECK_MSG(t, lib != NULL, "%s", rt.error_msg);
    if (lib != NULL) {
        tag.name = "e";
        tag.identity = lib->tags[0].identity;
        tag.type.params = &i64_type;
        tag.type.param_reftypes = NULL;
        CHECK(t, wasm_register_tag_import(&rt, &tag) == WASM_ERR_TYPE_MISMATCH);
        tag.type.params = &i32_type;
        WASM_CHECK_OK(t, wasm_register_tag_import(&rt, &tag));
        wasm_module_free(lib);
    }
    wasm_destroy(&rt);
}

/* (module (type $pad (func (param f64))) (type $s (struct)) (type $f (func (param (ref null $s))))
     (func (export "f") (type $f))) */
static const uint8_t wasm_test_typed_func_lib_sections[] = {
    0x01, 0x0C, 0x03, 0x60, 0x01, 0x7C, 0x00, 0x5F, 0x00, 0x60, 0x01, 0x63, 0x01, 0x00, 0x03, 0x02,
    0x01, 0x02, 0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00, 0x0A, 0x04, 0x01, 0x02, 0x00, 0x0B,
};

/* (module (type $s (struct)) (import "lib" "f" (func (param (ref null $s))))) */
static const uint8_t wasm_test_typed_func_user_sections[] = {
    0x01, 0x08, 0x02, 0x5F, 0x00, 0x60, 0x01, 0x63, 0x00, 0x00, 0x02, 0x09, 0x01, 0x03, 0x6C, 0x69,
    0x62, 0x01, 0x66, 0x00, 0x01,
};

TEST(test_function_imports_of_a_freed_module_do_not_match) {
    wasm_runtime_t rt;
    wasm_module_t* lib;
    wasm_module_t* m;

    wasm_init(&rt);
    lib = wasm_test_load_sections(&rt, wasm_test_typed_func_lib_sections, sizeof(wasm_test_typed_func_lib_sections));
    CHECK_MSG(t, lib != NULL, "%s", rt.error_msg);
    if (lib == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "lib", lib));
    m = wasm_test_load_sections(&rt, wasm_test_typed_func_user_sections, sizeof(wasm_test_typed_func_user_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    wasm_module_free(m);

    wasm_module_free(lib);
    m = wasm_test_load_sections(&rt, wasm_test_typed_func_user_sections, sizeof(wasm_test_typed_func_user_sections));
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "lib.f") != NULL, "%s", rt.error_msg);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_register_module_links_instances_and_host_objects) {
    /* a exports double(x) = 2x, mutable global g = 1, immutable global k = 5, table t holding double,
     * and memory m whose first byte is 100. b imports all of them; run() returns
     * double(g) + m[0] + table.size(t). c imports only double. e imports host.mem and host.tab and
     * grows them. */
    static const uint8_t a_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x02, 0x05, 0x03, 0x01, 0x00, 0x01, 0x06, 0x0b, 0x02, 0x7f, 0x01,
        0x41, 0x01, 0x0b, 0x7f, 0x00, 0x41, 0x05, 0x0b, 0x07, 0x1a, 0x05, 0x06, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65,
        0x00, 0x00, 0x01, 0x67, 0x03, 0x00, 0x01, 0x6b, 0x03, 0x01, 0x01, 0x74, 0x01, 0x00, 0x01, 0x6d, 0x02, 0x00,
        0x09, 0x07, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x41, 0x02,
        0x6c, 0x0b, 0x0b, 0x07, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x64,
    };
    static const uint8_t b_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x02, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x00,
        0x01, 0x7f, 0x02, 0x22, 0x04, 0x01, 0x61, 0x06, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x01, 0x61,
        0x01, 0x67, 0x03, 0x7f, 0x01, 0x01, 0x61, 0x01, 0x74, 0x01, 0x70, 0x00, 0x02, 0x01, 0x61, 0x01, 0x6d, 0x02,
        0x00, 0x01, 0x03, 0x02, 0x01, 0x01, 0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x01, 0x0a, 0x12, 0x01,
        0x10, 0x00, 0x23, 0x00, 0x10, 0x00, 0x41, 0x00, 0x2d, 0x00, 0x00, 0x6a, 0xfc, 0x10, 0x00, 0x6a, 0x0b,
    };
    static const uint8_t c_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x0c,
        0x01, 0x01, 0x61, 0x06, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x07, 0x0f,
        0x01, 0x0b, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x00, 0x01, 0x0a, 0x08, 0x01,
        0x06, 0x00, 0x20, 0x00, 0x10, 0x00, 0x0b,
    };
    static const uint8_t e_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x1b,
        0x02, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x01, 0x01, 0x02, 0x04, 0x68, 0x6f, 0x73,
        0x74, 0x03, 0x74, 0x61, 0x62, 0x01, 0x70, 0x00, 0x01, 0x03, 0x03, 0x02, 0x00, 0x00, 0x07, 0x17, 0x02, 0x08,
        0x67, 0x72, 0x6f, 0x77, 0x5f, 0x6d, 0x65, 0x6d, 0x00, 0x00, 0x08, 0x67, 0x72, 0x6f, 0x77, 0x5f, 0x74, 0x61,
        0x62, 0x00, 0x01, 0x0a, 0x12, 0x02, 0x06, 0x00, 0x20, 0x00, 0x40, 0x00, 0x0b, 0x09, 0x00, 0xd0, 0x70, 0x20,
        0x00, 0xfc, 0x0f, 0x00, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* a;
    wasm_module_t* b;
    wasm_module_t* c;
    wasm_module_t* e;
    wasm_memory_t* memory;
    wasm_table_t* table;
    wasm_memory_import_t memory_import;
    wasm_table_import_t table_import;
    wasm_value_t value;
    wasm_value_t ref;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    a = wasm_load(&rt, a_bytes, sizeof(a_bytes));
    CHECK_MSG(t, a != NULL, "%s", rt.error_msg);
    if (!a) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "a", a));
    b = wasm_load(&rt, b_bytes, sizeof(b_bytes));
    c = wasm_load(&rt, c_bytes, sizeof(c_bytes));
    CHECK_MSG(t, b != NULL && c != NULL, "%s", rt.error_msg);

    WASM_CHECK_OK(t, wasm_global_set_index(a, 0, wasm_i32(21)));
    CHECK(t, wasm_global_set_index(a, 1, wasm_i32(6)) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_global_set_index(a, 0, wasm_i64(21)) == WASM_ERR_TYPE_MISMATCH);
    WASM_CHECK_OK(t, wasm_global_get_index(a, 1, &value));
    CHECK(t, value.of.i32 == 5);
    CHECK(t, wasm_global_get_index(a, 2, &value) == WASM_ERR_MALFORMED);

    CHECK(t, wasm_table_size(a, 0) == 2u);
    WASM_CHECK_OK(t, wasm_table_get(a, 0, 0, &ref));
    arg = wasm_i32(5);
    WASM_CHECK_OK(t, wasm_call_ref(&rt, ref, &arg, 1, &result, 1));
    CHECK(t, result.of.i32 == 10);
    {
        wasm_module_t* target = NULL;
        uint32_t target_idx = UINT32_MAX;

        WASM_CHECK_OK(t, wasm_func_ref_target(&rt, ref, &target, &target_idx));
        CHECK(t, target == a && wasm_func_functype(a, target_idx) != NULL);
        CHECK(t, wasm_func_ref_target(&rt, wasm_ref_null(WASM_TYPE_FUNCREF), &target, NULL) ==
                     WASM_ERR_TYPE_MISMATCH && target == NULL);
        CHECK(t, wasm_func_ref_target(&rt, wasm_i32(0), NULL, NULL) == WASM_ERR_TYPE_MISMATCH);
    }
    WASM_CHECK_OK(t, wasm_table_get(a, 0, 1, &value));
    CHECK(t, wasm_call_ref(&rt, value, &arg, 1, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_table_grow(a, 0, 1, ref) == 2);
    CHECK(t, wasm_table_grow(a, 0, 1, wasm_i32(0)) == -1);
    WASM_CHECK_OK(t, wasm_table_set(a, 0, 1, ref));
    CHECK(t, wasm_table_set(a, 0, 1, wasm_i32(0)) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_table_get(a, 0, 3, &value) == WASM_ERR_OUT_OF_BOUNDS);

    if (b) {
        WASM_CHECK_OK(t, wasm_call(b, "run", NULL, 0, &result, 1));
        CHECK_MSG(t, result.of.i32 == 42 + 100 + 3, "expected 145, got %d", (int)result.of.i32);
        wasm_module_free(b);
    }
    wasm_module_free(a);
    if (c) {
        arg = wasm_i32(4);
        CHECK(t, wasm_call(c, "call_double", &arg, 1, &result, 1) == WASM_ERR_TRAP);
        CHECK_MSG(t, strstr(rt.error_msg, "freed module") != NULL, "%s", rt.error_msg);
        wasm_module_free(c);
    }

    memory = wasm_memory_new(&wasm_test_allocator, 1, 2, 0);
    table = wasm_table_new(&wasm_test_allocator, WASM_TYPE_FUNCREF, 1, UINT32_MAX);
    CHECK(t, memory != NULL && table != NULL);
    memset(&memory_import, 0, sizeof(memory_import));
    memory_import.module = "host";
    memory_import.name = "mem";
    memory_import.memory = memory;
    memset(&table_import, 0, sizeof(table_import));
    table_import.module = "host";
    table_import.name = "tab";
    table_import.table = table;
    WASM_CHECK_OK(t, wasm_register_memory_import(&rt, &memory_import));
    WASM_CHECK_OK(t, wasm_register_table_import(&rt, &table_import));
    e = wasm_load(&rt, e_bytes, sizeof(e_bytes));
    CHECK_MSG(t, e != NULL, "%s", rt.error_msg);
    if (e) {
        arg = wasm_i32(1);
        WASM_CHECK_OK(t, wasm_call(e, "grow_mem", &arg, 1, &result, 1));
        CHECK(t, result.of.i32 == 1 && memory->pages == 2u);
        CHECK(t, wasm_memory_base(memory) == wasm_memory_data(e, 0));
        CHECK(t, wasm_memory_length(memory) == 2u * WASM_PAGE_SIZE);
        WASM_CHECK_OK(t, wasm_call(e, "grow_mem", &arg, 1, &result, 1));
        CHECK(t, result.of.i32 == -1);
        arg = wasm_i32(3);
        WASM_CHECK_OK(t, wasm_call(e, "grow_tab", &arg, 1, &result, 1));
        CHECK(t, result.of.i32 == 1 && table->size == 4u);
        wasm_module_free(e);
    }
    wasm_memory_free(memory);
    wasm_table_free(table);
    wasm_destroy(&rt);
}

/* (type $super (sub (func))) (type $sub (sub $super (func))) begins each of these modules.
   a: (func (export "f") (type $sub)) (func (export "g") (type $super)) */
static const uint8_t wasm_test_dyn_a_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0C, 0x02, 0x50, 0x00, 0x60, 0x00, 0x00,
    0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x03, 0x03, 0x02, 0x01, 0x00, 0x07, 0x09, 0x02, 0x01, 0x66,
    0x00, 0x00, 0x01, 0x67, 0x00, 0x01, 0x0A, 0x07, 0x02, 0x02, 0x00, 0x0B, 0x02, 0x00, 0x0B,
};

/* b: (import "a" "f" (func (type $super))) (export "f" (func 0)) */
static const uint8_t wasm_test_dyn_b_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0C, 0x02, 0x50, 0x00, 0x60, 0x00, 0x00,
    0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x02, 0x07, 0x01, 0x01, 0x61, 0x01, 0x66, 0x00, 0x00, 0x07,
    0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
};

/* (import "b" "f" (func (exact (type $sub)))) */
static const uint8_t wasm_test_dyn_exact_sub_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0C, 0x02, 0x50, 0x00, 0x60, 0x00, 0x00,
    0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x02, 0x07, 0x01, 0x01, 0x62, 0x01, 0x66, 0x20, 0x01,
};

/* (import "b" "f" (func (exact (type $super)))) */
static const uint8_t wasm_test_dyn_exact_super_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0C, 0x02, 0x50, 0x00, 0x60, 0x00, 0x00,
    0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x02, 0x07, 0x01, 0x01, 0x62, 0x01, 0x66, 0x20, 0x00,
};

/* (import "b" "f" (func $f (type $super))) (import "a" "g" (func $g (type $super)))
   (table 2 funcref) (elem (i32.const 0) func $f $g)
   f-exact and g-exact are (ref.test (ref (exact $sub)) (ref.func $f or $g)), g-super is
   (ref.test (ref (exact $super)) (ref.func $g)), and call-sub(i) is (call_indirect (type $sub) i). */
static const uint8_t wasm_test_dyn_user_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x14, 0x04, 0x50, 0x00, 0x60, 0x00, 0x00,
    0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x60, 0x01, 0x7F, 0x00, 0x02, 0x0D,
    0x02, 0x01, 0x62, 0x01, 0x66, 0x00, 0x00, 0x01, 0x61, 0x01, 0x67, 0x00, 0x00, 0x03, 0x05, 0x04,
    0x02, 0x02, 0x02, 0x03, 0x04, 0x04, 0x01, 0x70, 0x00, 0x02, 0x07, 0x2A, 0x04, 0x07, 0x66, 0x2D,
    0x65, 0x78, 0x61, 0x63, 0x74, 0x00, 0x02, 0x07, 0x67, 0x2D, 0x65, 0x78, 0x61, 0x63, 0x74, 0x00,
    0x03, 0x07, 0x67, 0x2D, 0x73, 0x75, 0x70, 0x65, 0x72, 0x00, 0x04, 0x08, 0x63, 0x61, 0x6C, 0x6C,
    0x2D, 0x73, 0x75, 0x62, 0x00, 0x05, 0x09, 0x08, 0x01, 0x00, 0x41, 0x00, 0x0B, 0x02, 0x00, 0x01,
    0x0A, 0x24, 0x04, 0x08, 0x00, 0xD2, 0x00, 0xFB, 0x14, 0x62, 0x01, 0x0B, 0x08, 0x00, 0xD2, 0x01,
    0xFB, 0x14, 0x62, 0x01, 0x0B, 0x08, 0x00, 0xD2, 0x01, 0xFB, 0x14, 0x62, 0x00, 0x0B, 0x07, 0x00,
    0x20, 0x00, 0x11, 0x01, 0x00, 0x0B,
};

/* (type $f (func (param i32))) (import "env" "print" (func $exact (exact (type $f))))
   (import "env" "print" (func $inexact (type $f)))
   inexact-exact is (ref.test (ref (exact $f)) (ref.func $inexact)). */
static const uint8_t wasm_test_dyn_host_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x60, 0x01, 0x7F, 0x00, 0x60,
    0x00, 0x01, 0x7F, 0x02, 0x19, 0x02, 0x03, 0x65, 0x6E, 0x76, 0x05, 0x70, 0x72, 0x69, 0x6E, 0x74,
    0x20, 0x00, 0x03, 0x65, 0x6E, 0x76, 0x05, 0x70, 0x72, 0x69, 0x6E, 0x74, 0x00, 0x00, 0x03, 0x02,
    0x01, 0x01, 0x07, 0x11, 0x01, 0x0D, 0x69, 0x6E, 0x65, 0x78, 0x61, 0x63, 0x74, 0x2D, 0x65, 0x78,
    0x61, 0x63, 0x74, 0x00, 0x02, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x01, 0x0A, 0x0A, 0x01, 0x08,
    0x00, 0xD2, 0x01, 0xFB, 0x14, 0x62, 0x00, 0x0B,
};

/* (type $t (func (result anyref))) (import "env" "h" (func $h (type $t))) (elem declare func $h)
   (func (export "get") (result funcref) (ref.func $h)) */
static const uint8_t wasm_test_dyn_host_ref_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x60, 0x00, 0x01, 0x6E, 0x60,
    0x00, 0x01, 0x70, 0x02, 0x09, 0x01, 0x03, 0x65, 0x6E, 0x76, 0x01, 0x68, 0x00, 0x00, 0x03, 0x02,
    0x01, 0x01, 0x07, 0x07, 0x01, 0x03, 0x67, 0x65, 0x74, 0x00, 0x01, 0x09, 0x05, 0x01, 0x03, 0x00,
    0x01, 0x00, 0x0A, 0x06, 0x01, 0x04, 0x00, 0xD2, 0x00, 0x0B,
};

/* (type $t (func (result anyref)))
   (func (export "test") (param funcref) (result i32) (ref.test (ref (exact $t)) (local.get 0))) */
static const uint8_t wasm_test_dyn_exact_test_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0A, 0x02, 0x60, 0x00, 0x01, 0x6E, 0x60,
    0x01, 0x70, 0x01, 0x7F, 0x03, 0x02, 0x01, 0x01, 0x07, 0x08, 0x01, 0x04, 0x74, 0x65, 0x73, 0x74,
    0x00, 0x00, 0x0A, 0x0A, 0x01, 0x08, 0x00, 0x20, 0x00, 0xFB, 0x14, 0x62, 0x00, 0x0B,
};

/* (type $r (func (result i32))) (import "b" "f" (func $f (exact (type $sub)))) (elem declare func $f)
   self-exact is (ref.test (ref (exact $sub)) (ref.func $f)). */
static const uint8_t wasm_test_dyn_exact_self_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x10, 0x03, 0x50, 0x00, 0x60, 0x00, 0x00,
    0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x02, 0x07, 0x01, 0x01, 0x62, 0x01,
    0x66, 0x20, 0x01, 0x03, 0x02, 0x01, 0x02, 0x07, 0x0E, 0x01, 0x0A, 0x73, 0x65, 0x6C, 0x66, 0x2D,
    0x65, 0x78, 0x61, 0x63, 0x74, 0x00, 0x01, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x0A,
    0x01, 0x08, 0x00, 0xD2, 0x00, 0xFB, 0x14, 0x62, 0x01, 0x0B,
};

/* (import "b" "f" (func $f (type $super))) (func (export "call") (call $f)) */
static const uint8_t wasm_test_dyn_inexact_call_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0C, 0x02, 0x50, 0x00, 0x60, 0x00, 0x00,
    0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x02, 0x07, 0x01, 0x01, 0x62, 0x01, 0x66, 0x00, 0x00, 0x03,
    0x02, 0x01, 0x00, 0x07, 0x08, 0x01, 0x04, 0x63, 0x61, 0x6C, 0x6C, 0x00, 0x01, 0x0A, 0x06, 0x01,
    0x04, 0x00, 0x10, 0x00, 0x0B,
};

static int32_t wasm_test_dyn_call(test_ctx* t, wasm_module_t* m, const char* name) {
    wasm_value_t result = wasm_i32(-1);

    WASM_CHECK_OK(t, wasm_call(m, name, NULL, 0, &result, 1));
    return result.of.i32;
}

TEST(test_function_references_carry_their_dynamic_types) {
    wasm_valtype_t i32_param = WASM_TYPE_I32;
    wasm_runtime_t rt;
    wasm_module_t* a;
    wasm_module_t* b;
    wasm_module_t* m;
    wasm_import_t imp;
    wasm_value_t arg;
    int typed;

    wasm_init(&rt);
    a = wasm_load(&rt, wasm_test_dyn_a_bytes, sizeof(wasm_test_dyn_a_bytes));
    CHECK_MSG(t, a != NULL, "%s", rt.error_msg);
    if (a) WASM_CHECK_OK(t, wasm_register_module(&rt, "a", a));
    b = wasm_load(&rt, wasm_test_dyn_b_bytes, sizeof(wasm_test_dyn_b_bytes));
    CHECK_MSG(t, b != NULL, "%s", rt.error_msg);
    if (b) WASM_CHECK_OK(t, wasm_register_module(&rt, "b", b));

    m = wasm_load(&rt, wasm_test_dyn_exact_sub_bytes, sizeof(wasm_test_dyn_exact_sub_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    wasm_module_free(m);
    m = wasm_load(&rt, wasm_test_dyn_exact_super_bytes, sizeof(wasm_test_dyn_exact_super_bytes));
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "import type mismatch") != NULL, "%s", rt.error_msg);
    wasm_module_free(m);

    m = wasm_load(&rt, wasm_test_dyn_user_bytes, sizeof(wasm_test_dyn_user_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        WASM_CHECK_I32(t, wasm_test_dyn_call(t, m, "f-exact"), 1);
        WASM_CHECK_I32(t, wasm_test_dyn_call(t, m, "g-exact"), 0);
        WASM_CHECK_I32(t, wasm_test_dyn_call(t, m, "g-super"), 1);
        arg = wasm_i32(0);
        WASM_CHECK_OK(t, wasm_call(m, "call-sub", &arg, 1, NULL, 0));
        arg = wasm_i32(1);
        CHECK(t, wasm_call(m, "call-sub", &arg, 1, NULL, 0) == WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH);
        wasm_module_free(m);
    }

    m = wasm_load(&rt, wasm_test_dyn_exact_self_bytes, sizeof(wasm_test_dyn_exact_self_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) WASM_CHECK_I32(t, wasm_test_dyn_call(t, m, "self-exact"), 1);

    wasm_module_free(a);
    if (m) WASM_CHECK_I32(t, wasm_test_dyn_call(t, m, "self-exact"), 1);
    wasm_module_free(m);
    m = wasm_load(&rt, wasm_test_dyn_exact_sub_bytes, sizeof(wasm_test_dyn_exact_sub_bytes));
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "import type mismatch") != NULL, "%s", rt.error_msg);
    wasm_module_free(m);
    m = wasm_load(&rt, wasm_test_dyn_inexact_call_bytes, sizeof(wasm_test_dyn_inexact_call_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        CHECK(t, wasm_call(m, "call", NULL, 0, NULL, 0) == WASM_ERR_TRAP);
        CHECK_MSG(t, strstr(rt.error_msg, "freed module") != NULL, "%s", rt.error_msg);
        wasm_module_free(m);
    }
    wasm_module_free(b);
    wasm_destroy(&rt);

    for (typed = 0; typed < 2; typed++) {
        wasm_init(&rt);
        memset(&imp, 0, sizeof(imp));
        imp.module = "env";
        imp.name = "print";
        imp.func = host_print;
        if (typed) {
            imp.type.num_params = 1;
            imp.type.params = &i32_param;
        }
        WASM_CHECK_OK(t, wasm_register_import(&rt, &imp));
        m = wasm_load(&rt, wasm_test_dyn_host_bytes, sizeof(wasm_test_dyn_host_bytes));
        CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
        if (m) {
            WASM_CHECK_I32(t, wasm_test_dyn_call(t, m, "inexact-exact"), typed);
            wasm_module_free(m);
        }
        wasm_destroy(&rt);
    }

    for (typed = 0; typed < 2; typed++) {
        wasm_valtype_t result_type = typed ? WASM_TYPE_ANYREF : WASM_TYPE_I31REF;
        wasm_reftype_t result_ref;
        wasm_value_t ref = wasm_ref_null(WASM_TYPE_FUNCREF);
        wasm_value_t result = wasm_i32(-1);

        wasm_init(&rt);
        memset(&imp, 0, sizeof(imp));
        imp.module = "env";
        imp.name = "h";
        imp.func = host_print;
        memset(&result_ref, 0, sizeof(result_ref));
        result_ref.type = result_type;
        result_ref.nullable = 1;
        imp.type.num_results = 1;
        imp.type.results = &result_type;
        imp.type.result_reftypes = &result_ref;
        WASM_CHECK_OK(t, wasm_register_import(&rt, &imp));
        a = wasm_load(&rt, wasm_test_dyn_host_ref_bytes, sizeof(wasm_test_dyn_host_ref_bytes));
        CHECK_MSG(t, typed ? a != NULL : a == NULL && strstr(rt.error_msg, "import type mismatch") != NULL, "%s",
                  rt.error_msg);
        m = wasm_load(&rt, wasm_test_dyn_exact_test_bytes, sizeof(wasm_test_dyn_exact_test_bytes));
        CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
        if (a && m) {
            WASM_CHECK_OK(t, wasm_call(a, "get", NULL, 0, &ref, 1));
            WASM_CHECK_OK(t, wasm_call(m, "test", &ref, 1, &result, 1));
            WASM_CHECK_I32(t, result.of.i32, typed);
        }
        wasm_module_free(m);
        wasm_module_free(a);
        wasm_destroy(&rt);
    }
}

/* lib: (module (type $s (struct (field i32))) (global (export "g") (mut anyref) (ref.null any))
     (table (export "t") 1 anyref)
     (func (export "fill") (param $v i32)
       (global.set 0 (struct.new $s (local.get $v))) (table.set 0 (i32.const 0) (struct.new $s (local.get $v)))))
   user: (module (import "lib" "g" (global (mut anyref))) (import "lib" "t" (table 1 anyref))) */
static const uint8_t wasm_test_link_freed_lib_sections[] = {
    0x01, 0x09, 0x02, 0x5F, 0x01, 0x7F, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x03, 0x02, 0x01, 0x01, 0x04,
    0x04, 0x01, 0x6E, 0x00, 0x01, 0x06, 0x06, 0x01, 0x6E, 0x01, 0xD0, 0x6E, 0x0B, 0x07, 0x10, 0x03,
    0x01, 0x67, 0x03, 0x00, 0x01, 0x74, 0x01, 0x00, 0x04, 0x66, 0x69, 0x6C, 0x6C, 0x00, 0x00, 0x0A,
    0x14, 0x01, 0x12, 0x00, 0x20, 0x00, 0xFB, 0x00, 0x00, 0x24, 0x00, 0x41, 0x00, 0x20, 0x00, 0xFB,
    0x00, 0x00, 0x26, 0x00, 0x0B,
};
static const uint8_t wasm_test_link_freed_lib_user_sections[] = {
    0x02, 0x14, 0x02, 0x03, 0x6C, 0x69, 0x62, 0x01, 0x67, 0x03, 0x6E, 0x01, 0x03, 0x6C, 0x69, 0x62,
    0x01, 0x74, 0x01, 0x6E, 0x00, 0x01,
};

TEST(test_exports_of_a_freed_registered_module_unbind) {
    wasm_runtime_t rt;
    wasm_module_t* lib;
    wasm_module_t* user;
    wasm_value_t arg;

    wasm_init(&rt);
    lib = wasm_test_load_sections(&rt, wasm_test_link_freed_lib_sections, sizeof(wasm_test_link_freed_lib_sections));
    CHECK_MSG(t, lib != NULL, "%s", rt.error_msg);
    if (!lib) {
        wasm_destroy(&rt);
        return;
    }
    arg = wasm_i32(7);
    WASM_CHECK_OK(t, wasm_call(lib, "fill", &arg, 1, NULL, 0));
    WASM_CHECK_OK(t, wasm_register_module(&rt, "lib", lib));
    user = wasm_test_load_sections(&rt, wasm_test_link_freed_lib_user_sections,
                                   sizeof(wasm_test_link_freed_lib_user_sections));
    CHECK_MSG(t, user != NULL, "%s", rt.error_msg);
    wasm_module_free(user);
    wasm_module_free(lib);
    wasm_gc_collect(&rt);
    user = wasm_test_load_sections(&rt, wasm_test_link_freed_lib_user_sections,
                                   sizeof(wasm_test_link_freed_lib_user_sections));
    CHECK_MSG(t, user == NULL, "%s", "expected the freed module's global and table not to bind");
    wasm_module_free(user);
    wasm_destroy(&rt);
}

static const test_case link_cases[] = {
    { "host import: env.print(42)", test_host_import },
    { "global import: mutable env.counter is read and updated by reference", test_imported_mutable_global },
    { "stack switching: a tag identity names tags of one type", test_stack_switching_tag_identity_names_one_type },
    { "imports: function imports of a freed module do not match", test_function_imports_of_a_freed_module_do_not_match },
    { "public api: register_module links instances; table, global, funcref, and host object APIs", test_register_module_links_instances_and_host_objects },
    { "custom descriptors: function references and exact imports use the dynamic type of the function", test_function_references_carry_their_dynamic_types },
    { "imports: a freed registered module's globals and tables no longer bind and are not roots", test_exports_of_a_freed_registered_module_unbind },
};
const test_suite link_suite = { "link", link_cases, COUNTOF(link_cases) };

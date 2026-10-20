#include "support.h"

#ifdef WASM_TEST_MODULAR
#include "../../src/types/canonical.h"
#include "../../src/types/references.h"
#include "../../src/values.h"
#endif

static void emit_custom_section(wasm_builder_t* b,
                                const char* name,
                                const uint8_t* payload,
                                uint32_t payload_len) {
    wasm_builder_t sec = { 0 };
    uint32_t name_len = (uint32_t)strlen(name);

    emit_leb128_u32(&sec, name_len);
    emit_bytes(&sec, (const uint8_t*)name, name_len);
    if (payload_len > 0) emit_bytes(&sec, payload, payload_len);
    emit_section(b, 0, sec.buf, sec.len);
}

TEST(test_introspection_helpers) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_export_kind_t kind = WASM_EXPORT_MEM;
    uint32_t func_idx = 77;
    static const uint8_t meta_payload[] = { 0x01, 0x02, 0x03 };
    static const uint8_t cfg_payload[] = { 'o', 'k' };
    const uint8_t* custom_data;
    size_t custom_len = 0;

    emit_header(&mod);
    emit_custom_section(&mod, "meta", meta_payload, (uint32_t)sizeof(meta_payload));

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
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

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "add", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    emit_custom_section(&mod, "cfg", cfg_payload, (uint32_t)sizeof(cfg_payload));

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
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

    CHECK_MSG(t, wasm_export_count(NULL) == 0, "%s", "NULL module should report zero exports");
    CHECK_MSG(t, wasm_export_count(m) == 1, "%s", "expected one export");
    CHECK_MSG(t, wasm_export_name(m, 0) != NULL && strcmp(wasm_export_name(m, 0), "add") == 0,
              "%s", "expected export 0 to be named add");
    CHECK_MSG(t, wasm_export_name(m, 1) == NULL, "%s", "out-of-range export name should be NULL");
    CHECK_MSG(t, wasm_export_kind(m, 0) == WASM_EXPORT_FUNC, "%s", "expected function export kind");
    CHECK_MSG(t, wasm_export_index(m, 0) == 0, "%s", "expected function index 0");
    CHECK_MSG(t, wasm_export_index(m, 1) == 0, "%s", "out-of-range export index should default to 0");

    CHECK_MSG(t, !wasm_find_export(m, "missing", &kind, &func_idx), "%s", "missing export should not resolve");
    CHECK_MSG(t, kind == WASM_EXPORT_MEM, "%s", "missing export should leave kind untouched");
    CHECK_MSG(t, func_idx == 77, "%s", "missing export should leave index untouched");
    CHECK_MSG(t, wasm_find_export(m, "add", &kind, &func_idx), "%s", "expected add export lookup to succeed");
    CHECK_MSG(t, kind == WASM_EXPORT_FUNC, "%s", "expected add export kind to be function");
    CHECK_MSG(t, func_idx == 0, "%s", "expected add export to reference function 0");

    CHECK_MSG(t, wasm_custom_section_count(NULL) == 0,
              "%s", "NULL module should report zero custom sections");
    CHECK_MSG(t, wasm_custom_section_count(m) == 2,
              "%s", "expected two custom sections");
    CHECK_MSG(t, wasm_custom_section_name(m, 0) != NULL && strcmp(wasm_custom_section_name(m, 0), "meta") == 0,
              "%s", "expected custom section 0 to be named meta");
    CHECK_MSG(t, wasm_custom_section_name(m, 1) != NULL && strcmp(wasm_custom_section_name(m, 1), "cfg") == 0,
              "%s", "expected custom section 1 to be named cfg");
    CHECK_MSG(t, wasm_custom_section_name(m, 2) == NULL,
              "%s", "out-of-range custom section name should be NULL");

    custom_data = wasm_custom_section_data(m, 0, &custom_len);
    CHECK_MSG(t, custom_data != NULL, "%s", "expected custom section payload");
    CHECK_MSG(t, custom_len == sizeof(meta_payload), "%s", "unexpected meta payload length");
    CHECK_MSG(t, memcmp(custom_data, meta_payload, sizeof(meta_payload)) == 0,
              "%s", "unexpected meta payload bytes");

    custom_len = 123;
    CHECK_MSG(t, wasm_custom_section_data(m, 9, &custom_len) == NULL,
              "%s", "out-of-range custom section data should be NULL");
    CHECK_MSG(t, custom_len == 0, "%s", "out-of-range custom section length should reset to zero");

    custom_data = wasm_custom_section_find(m, "cfg", &custom_len);
    CHECK_MSG(t, custom_data != NULL, "%s", "expected cfg custom section lookup to succeed");
    CHECK_MSG(t, custom_len == sizeof(cfg_payload), "%s", "unexpected cfg payload length");
    CHECK_MSG(t, memcmp(custom_data, cfg_payload, sizeof(cfg_payload)) == 0,
              "%s", "unexpected cfg payload bytes");
    custom_len = 55;
    CHECK_MSG(t, wasm_custom_section_find(m, "missing", &custom_len) == NULL,
              "%s", "missing custom section should not resolve");
    CHECK_MSG(t, custom_len == 0, "%s", "missing custom section length should reset to zero");

    CHECK_MSG(t, wasm_func_count(NULL) == 0, "%s", "NULL module should report zero functions");
    CHECK_MSG(t, wasm_func_count(m) == 1, "%s", "expected one function");
    CHECK_MSG(t, wasm_func_param_count(m, 0) == 2, "%s", "expected two params");
    CHECK_MSG(t, wasm_func_param_count(m, 1) == 0, "%s", "out-of-range param count should default to 0");
    CHECK_MSG(t, wasm_func_result_count(m, 0) == 1, "%s", "expected one result");
    CHECK_MSG(t, wasm_func_result_count(m, 1) == 0, "%s", "out-of-range result count should default to 0");
    CHECK_MSG(t, wasm_func_param_type(m, 0, 0) == WASM_TYPE_I32, "%s", "expected first param type i32");
    CHECK_MSG(t, wasm_func_param_type(m, 0, 1) == WASM_TYPE_I32, "%s", "expected second param type i32");
    CHECK_MSG(t, wasm_func_param_type(m, 0, 2) == WASM_TYPE_VOID,
              "%s", "out-of-range param type should default to void");
    CHECK_MSG(t, wasm_func_param_type(NULL, 0, 0) == WASM_TYPE_VOID,
              "%s", "NULL module param type should default to void");
    CHECK_MSG(t, wasm_func_result_type(m, 0, 0) == WASM_TYPE_I32, "%s", "expected result type i32");
    CHECK_MSG(t, wasm_func_result_type(m, 0, 1) == WASM_TYPE_VOID,
              "%s", "out-of-range result type should default to void");
    CHECK_MSG(t, wasm_func_result_type(NULL, 0, 0) == WASM_TYPE_VOID,
              "%s", "NULL module result type should default to void");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static wasm_module_t* host_userdata_seen_module;

static void* host_userdata_seen_value;

static wasm_error_t host_capture_module_userdata(wasm_module_t* mod,
                                                 const wasm_value_t* args, uint32_t num_args,
                                                 wasm_value_t* results, uint32_t num_results,
                                                 void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;

    host_userdata_seen_module = mod;
    host_userdata_seen_value = wasm_module_userdata(mod);
    return WASM_OK;
}

TEST(test_import_introspection_api) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_import_t func_imp;
    wasm_global_import_t global_imp;
    wasm_value_t imported_counter = wasm_i32(7);
    const wasm_import_info_t* info;

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

        emit_leb128_u32(&sec, 2);

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
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

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
        emit(&sec, 0x00);

        emit_section(&mod, 2, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&func_imp, 0, sizeof(func_imp));
    func_imp.module = "env";
    func_imp.name = "print";
    func_imp.func = host_print;
    wasm_register_import(&rt, &func_imp);

    memset(&global_imp, 0, sizeof(global_imp));
    global_imp.module = "env";
    global_imp.name = "counter";
    global_imp.type = WASM_TYPE_I32;
    global_imp.value = &imported_counter;
    wasm_register_global_import(&rt, &global_imp);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_I32(t, (int32_t)wasm_import_count(m), 2);

    info = wasm_import_info(m, 0);
    CHECK_MSG(t, info != NULL, "%s", "expected function import metadata");
    if (info) {
        CHECK_MSG(t, strcmp(info->module, "env") == 0, "%s", "wrong function import module name");
        CHECK_MSG(t, strcmp(info->name, "print") == 0, "%s", "wrong function import name");
        CHECK_MSG(t, info->kind == WASM_IMPORT_FUNC, "%s", "wrong function import kind");
        WASM_CHECK_I32(t, (int32_t)info->index, 0);
        WASM_CHECK_I32(t, (int32_t)info->type_index, 0);
    }

    info = wasm_import_info(m, 1);
    CHECK_MSG(t, info != NULL, "%s", "expected global import metadata");
    if (info) {
        CHECK_MSG(t, strcmp(info->module, "env") == 0, "%s", "wrong global import module name");
        CHECK_MSG(t, strcmp(info->name, "counter") == 0, "%s", "wrong global import name");
        CHECK_MSG(t, info->kind == WASM_IMPORT_GLOBAL, "%s", "wrong global import kind");
        WASM_CHECK_I32(t, (int32_t)info->index, 0);
        CHECK_MSG(t, info->type == WASM_TYPE_I32, "%s", "wrong global import type");
        CHECK_MSG(t, info->is_mutable == 0, "%s", "expected immutable global import");
    }

    CHECK_MSG(t, wasm_import_info(m, 2) == NULL, "%s", "out-of-range import metadata should be null");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_module_userdata_api) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    int module_context = 1234;

    host_userdata_seen_module = NULL;
    host_userdata_seen_value = NULL;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
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
        emit(&sec, 'a');
        emit(&sec, 'p');
        emit(&sec, 't');
        emit(&sec, 'u');
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
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
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    {
        wasm_import_t imp;

        memset(&imp, 0, sizeof(imp));
        imp.module = "env";
        imp.name = "capture";
        imp.func = host_capture_module_userdata;
        wasm_register_import(&rt, &imp);
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, wasm_module_userdata(m) == NULL,
              "%s", "module userdata should default to null");
    wasm_module_set_userdata(m, &module_context);
    CHECK_MSG(t, wasm_module_userdata(m) == &module_context,
              "%s", "module userdata getter should return the stored pointer");

    err = wasm_call(m, "run", NULL, 0, NULL, 0);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, host_userdata_seen_module == m,
                  "%s", "host callback should see the calling module");
        CHECK_MSG(t, host_userdata_seen_value == &module_context,
                  "%s", "host callback should see the module userdata");
    }

    wasm_module_set_userdata(m, NULL);
    CHECK_MSG(t, wasm_module_userdata(m) == NULL,
              "%s", "module userdata should clear back to null");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_module_userdata_api_multiple_modules) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m1;
    wasm_module_t* m2;
    wasm_error_t err;
    int ctx1 = 11;
    int ctx2 = 22;

    host_userdata_seen_module = NULL;
    host_userdata_seen_value = NULL;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
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
        emit(&sec, 'a');
        emit(&sec, 'p');
        emit(&sec, 't');
        emit(&sec, 'u');
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
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
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    {
        wasm_import_t imp;

        memset(&imp, 0, sizeof(imp));
        imp.module = "env";
        imp.name = "capture";
        imp.func = host_capture_module_userdata;
        wasm_register_import(&rt, &imp);
    }

    m1 = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m1 != NULL, "%s", rt.error_msg);
    if (m1 == NULL) {
        wasm_destroy(&rt);
        return;
    }

    m2 = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m2 != NULL, "%s", rt.error_msg);
    if (m2 == NULL) {
        wasm_module_free(m1);
        wasm_destroy(&rt);
        return;
    }

    wasm_module_set_userdata(m1, &ctx1);
    wasm_module_set_userdata(m2, &ctx2);

    err = wasm_call(m1, "run", NULL, 0, NULL, 0);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, host_userdata_seen_module == m1,
                  "%s", "shared host import should receive the first module instance");
        CHECK_MSG(t, host_userdata_seen_value == &ctx1,
                  "%s", "shared host import should read the first module userdata");
    }

    host_userdata_seen_module = NULL;
    host_userdata_seen_value = NULL;

    err = wasm_call(m2, "run", NULL, 0, NULL, 0);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, host_userdata_seen_module == m2,
                  "%s", "shared host import should receive the second module instance");
        CHECK_MSG(t, host_userdata_seen_value == &ctx2,
                  "%s", "shared host import should read the second module userdata");
    }

    wasm_module_free(m2);
    wasm_module_free(m1);
    wasm_destroy(&rt);
}

TEST(test_public_api_tolerates_null_handles) {
    wasm_value_t value = wasm_i32(0);
    wasm_export_kind_t kind;
    uint32_t index = 0;
    uint32_t out_u32 = 0;
    size_t out_len = 0;
    uint8_t byte = 0;
    char text[16];
    wasm_import_t imp;
    wasm_global_import_t global_imp;
    wasm_table_import_t table_imp;
    wasm_memory_import_t memory_imp;
    wasm_tag_import_t tag_imp;

    memset(&imp, 0, sizeof(imp));
    memset(&global_imp, 0, sizeof(global_imp));
    memset(&table_imp, 0, sizeof(table_imp));
    memset(&memory_imp, 0, sizeof(memory_imp));
    memset(&tag_imp, 0, sizeof(tag_imp));

    CHECK(t, wasm_runtime_last_error(NULL) != WASM_OK || 1);
    (void)wasm_runtime_error_message(NULL);
    wasm_runtime_clear_error(NULL);
    wasm_runtime_set_error(NULL, WASM_ERR_TRAP, "x");
    CHECK(t, wasm_runtime_features(NULL) == 0);
    CHECK(t, wasm_runtime_exit_code(NULL) == 0);
    CHECK(t, wasm_register_import(NULL, &imp) != WASM_OK);
    CHECK(t, wasm_register_global_import(NULL, &global_imp) != WASM_OK);
    CHECK(t, wasm_register_table_import(NULL, &table_imp) != WASM_OK);
    CHECK(t, wasm_register_memory_import(NULL, &memory_imp) != WASM_OK);
    CHECK(t, wasm_register_tag_import(NULL, &tag_imp) != WASM_OK);
    wasm_gc_collect(NULL);
    CHECK(t, wasm_load(NULL, &byte, 1) == NULL);
    wasm_module_free(NULL);
    CHECK(t, wasm_call(NULL, "f", NULL, 0, NULL, 0) != WASM_OK);
    CHECK(t, wasm_call_index(NULL, 0, NULL, 0, NULL, 0) != WASM_OK);
    CHECK(t, wasm_call_fmt(NULL, "f", "v()") != WASM_OK);
    CHECK(t, wasm_bind_host_func(NULL, "m", "f", "v()", NULL, NULL) != WASM_OK);
    CHECK(t, wasm_memory_count(NULL) == 0);
    CHECK(t, wasm_memory_read(NULL, 0, 0, &byte, 1) != WASM_OK);
    CHECK(t, wasm_memory_write(NULL, 0, 0, &byte, 1) != WASM_OK);
    CHECK(t, wasm_memory_get_string(NULL, 0, 0, text, sizeof(text)) != WASM_OK);
    CHECK(t, wasm_module_memory(NULL, 0) == NULL);
    CHECK(t, wasm_memory_data(NULL, 0) == NULL);
    CHECK(t, wasm_memory_size(NULL, 0) == 0);
    CHECK(t, wasm_memory_grow(NULL, 0, 1) < 0);
    CHECK(t, wasm_memory_base(NULL) == NULL);
    CHECK(t, wasm_memory_length(NULL) == 0);
    wasm_memory_free(NULL);
    wasm_table_free(NULL);
    CHECK(t, wasm_memory_new(&wasm_test_allocator, 2, 1, 0) == NULL);
    CHECK(t, wasm_memory_new(NULL, 1, 1, 0) == NULL);
    CHECK(t, wasm_shared_memory_new(NULL, 1, 1, 0) == NULL);
    CHECK(t, wasm_table_new(&wasm_test_allocator, WASM_TYPE_I32, 1, 1) == NULL);
    CHECK(t, wasm_table_new(NULL, WASM_TYPE_FUNCREF, 1, 1) == NULL);
    CHECK(t, wasm_table_count(NULL) == 0);
    CHECK(t, wasm_module_table(NULL, 0) == NULL);
    (void)wasm_table_type(NULL, 0);
    CHECK(t, wasm_table_reftype(NULL, 0) == NULL);
    CHECK(t, wasm_global_count(NULL) == 0);
    (void)wasm_global_type(NULL, 0);
    CHECK(t, wasm_global_reftype(NULL, 0) == NULL);
    CHECK(t, wasm_global_is_mutable(NULL, 0) == 0);
    CHECK(t, wasm_module_global(NULL, 0) == NULL);
    CHECK(t, wasm_global_get(NULL, "g", &value) != WASM_OK);
    CHECK(t, wasm_global_set(NULL, "g", value) != WASM_OK);
    CHECK(t, wasm_import_count(NULL) == 0);
    CHECK(t, wasm_import_info(NULL, 0) == NULL);
    CHECK(t, wasm_export_count(NULL) == 0);
    CHECK(t, wasm_export_name(NULL, 0) == NULL);
    CHECK(t, wasm_export_name_bytes(NULL, 0, &out_len) == NULL);
    (void)wasm_export_kind(NULL, 0);
    (void)wasm_export_index(NULL, 0);
    CHECK(t, !wasm_find_export(NULL, "f", &kind, &index));
    CHECK(t, !wasm_find_export_bytes(NULL, &byte, 1, &kind, &index));
    CHECK(t, wasm_custom_section_count(NULL) == 0);
    CHECK(t, wasm_custom_section_name(NULL, 0) == NULL);
    CHECK(t, wasm_custom_section_data(NULL, 0, &out_len) == NULL);
    CHECK(t, wasm_custom_section_find(NULL, "name", &out_len) == NULL);
    CHECK(t, wasm_type_count(NULL) == 0);
    (void)wasm_type_kind(NULL, 0);
    CHECK(t, wasm_type_functype(NULL, 0) == NULL);
    CHECK(t, wasm_struct_type_field_count(NULL, 0) == 0);
    CHECK(t, wasm_struct_type_field(NULL, 0, 0) == NULL);
    CHECK(t, wasm_array_type_field(NULL, 0) == NULL);
    CHECK(t, wasm_type_descriptor(NULL, 0) == UINT32_MAX);
    CHECK(t, wasm_type_describes(NULL, 0) == UINT32_MAX);
    CHECK(t, wasm_func_count(NULL) == 0);
    CHECK(t, wasm_func_functype(NULL, 0) == NULL);
    CHECK(t, wasm_func_param_count(NULL, 0) == 0);
    CHECK(t, wasm_func_result_count(NULL, 0) == 0);
    (void)wasm_func_param_type(NULL, 0, 0);
    (void)wasm_func_result_type(NULL, 0, 0);
    CHECK(t, wasm_tag_count(NULL) == 0);
    CHECK(t, wasm_tag_functype(NULL, 0) == NULL);
    CHECK(t, wasm_struct_new(NULL, 0, NULL, 0, &value) != WASM_OK);
    CHECK(t, wasm_array_new(NULL, 0, NULL, 0, &value) != WASM_OK);
    CHECK(t, wasm_struct_get_field(NULL, value, 0, &value) != WASM_OK);
    CHECK(t, wasm_struct_set_field(NULL, value, 0, value) != WASM_OK);
    CHECK(t, wasm_array_length(NULL, value, &out_u32) != WASM_OK);
    CHECK(t, wasm_array_get_elem(NULL, value, 0, &value) != WASM_OK);
    CHECK(t, wasm_array_set_elem(NULL, value, 0, value) != WASM_OK);
    wasm_runtime_backtrace(NULL, text, sizeof(text));
    CHECK(t, wasm_runtime_call_depth(NULL) == 0);
    CHECK(t, wasm_runtime_call_frame(NULL, 0, NULL, &index, &out_u32) != WASM_OK);
    wasm_runtime_set_fuel(NULL, 1);
    wasm_runtime_disable_fuel(NULL);
    CHECK(t, wasm_runtime_fuel(NULL) == 0);
    CHECK(t, wasm_module_runtime(NULL) == NULL);
    wasm_module_set_userdata(NULL, NULL);
    CHECK(t, wasm_module_userdata(NULL) == NULL);
#if WASM_ENABLE_PLATFORM
    CHECK(t, wasm_bind_wasi_stubs(NULL) != WASM_OK);
    CHECK(t, wasm_bind_emscripten_stubs(NULL) != WASM_OK);
#endif
    CHECK(t, wasm_wasi_set_args(NULL, 0, NULL) != WASM_OK);
    CHECK(t, wasm_wasi_set_env(NULL, 0, NULL) != WASM_OK);
    CHECK(t, wasm_wasi_preopen_dir(NULL, ".", "/") != WASM_OK);
    wasm_runtime_free(NULL);
}

TEST(test_public_fuel_helpers_account_for_finite_calls) {
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

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x02);
        emit(&body, 0x7F);
        emit(&body, 0x03);
        emit(&body, 0x7F);
        emit(&body, 0x02);
        emit(&body, 0x40);
        emit(&body, 0x0B);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 42);
        emit(&body, 0x0B);
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

    WASM_CHECK_I64(t, (int64_t)wasm_runtime_fuel(&rt), 0);

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_I64(t, (int64_t)wasm_runtime_fuel(&rt), 0);

    wasm_runtime_set_fuel(&rt, 1);
    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_I64(t, (int64_t)wasm_runtime_fuel(&rt), 0);
    CHECK(t, wasm_call(m, "run", NULL, 0, &result, 1) == WASM_ERR_OUT_OF_FUEL);
    wasm_runtime_cancel(&rt);

    wasm_runtime_disable_fuel(&rt);
    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_public_fuel_helpers_trap_in_infinite_loop) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    uint32_t func_idx;
    uint32_t offset;
    char backtrace[128];

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

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "spin", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x03);
        emit(&body, 0x40);
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

    wasm_runtime_set_fuel(&rt, 5);
    err = wasm_call(m, "spin", NULL, 0, NULL, 0);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_FUEL, "%s", "expected out of fuel trap");
    WASM_CHECK_I64(t, (int64_t)wasm_runtime_fuel(&rt), 0);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 1);

    err = wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 0);
        WASM_CHECK_I32(t, (int32_t)offset, 2);
    }

    wasm_runtime_backtrace(&rt, backtrace, sizeof(backtrace));
    CHECK_MSG(t, strstr(backtrace, "#0 func[0] at offset 0x0002") != NULL,
              "%s", "expected fuel trap backtrace frame");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_public_type_helpers_and_host_allocators) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t struct_fields[2] = { wasm_i32(9), wasm_i32(255) };
    wasm_value_t array_values[2] = { wasm_i32(10), wasm_i32(20) };
    wasm_value_t struct_ref;
    wasm_value_t* held_struct = NULL;
    wasm_value_t array_ref;
    const wasm_gc_header_t* struct_header;
    const wasm_gc_header_t* array_header;
    wasm_value_t loaded;
    uint32_t array_len = 0;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);

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
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x77);
        emit(&sec, 0x01);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x01);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);
        emit_export_kind(&sec, "table0", 0x01, 0);
        emit_export_kind(&sec, "global0", 0x03, 0);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_I32(t, (int32_t)wasm_type_count(m), 4);
    CHECK_MSG(t, wasm_type_kind(m, 0) == WASM_COMP_STRUCT, "%s", "expected type 0 to be a struct");
    CHECK_MSG(t, wasm_type_kind(m, 1) == WASM_COMP_STRUCT, "%s", "expected type 1 to be a struct");
    CHECK_MSG(t, wasm_type_kind(m, 2) == WASM_COMP_ARRAY, "%s", "expected type 2 to be an array");
    CHECK_MSG(t, wasm_type_kind(m, 3) == WASM_COMP_FUNC, "%s", "expected type 3 to be a function");
    CHECK_MSG(t, wasm_type_kind(m, 99) == WASM_COMP_INVALID, "%s", "expected invalid kind for out-of-range type");
    CHECK_MSG(t, wasm_struct_type_field_count(m, 1) == 2, "%s", "expected subtype field count");
    CHECK_MSG(t, wasm_struct_type_field(m, 1, 1) != NULL, "%s", "expected second struct field");
    CHECK_MSG(t, wasm_struct_type_field(m, 1, 1)->storage.kind == WASM_STORAGE_PACKED,
              "%s", "expected packed second struct field");
    CHECK_MSG(t, wasm_struct_type_field(m, 1, 1)->is_mutable,
              "%s", "expected second struct field to be mutable");
    CHECK_MSG(t, wasm_array_type_field(m, 2) != NULL, "%s", "expected array element field");
    CHECK_MSG(t, wasm_array_type_field(m, 2)->storage.kind == WASM_STORAGE_PACKED,
              "%s", "expected packed array element type");
    CHECK_MSG(t, wasm_type_functype(m, 3) != NULL, "%s", "expected function type accessor");

    WASM_CHECK_I32(t, (int32_t)wasm_table_count(m), 1);
    CHECK_MSG(t, wasm_table_type(m, 0) == WASM_TYPE_STRUCTREF, "%s", "expected structref table");
    CHECK_MSG(t, wasm_table_reftype(m, 0) != NULL && wasm_table_reftype(m, 0)->has_type_index,
              "%s", "expected concrete table reftype metadata");
    CHECK_MSG(t, wasm_table_reftype(m, 0)->type_index == 0,
              "%s", "expected table concrete type index 0");

    WASM_CHECK_I32(t, (int32_t)wasm_global_count(m), 1);
    CHECK_MSG(t, wasm_global_type(m, 0) == WASM_TYPE_STRUCTREF, "%s", "expected structref global");
    CHECK_MSG(t, !wasm_global_is_mutable(m, 0), "%s", "expected immutable global");
    CHECK_MSG(t, wasm_global_reftype(m, 0) != NULL && wasm_global_reftype(m, 0)->has_type_index,
              "%s", "expected concrete global reftype metadata");
    CHECK_MSG(t, wasm_global_reftype(m, 0)->type_index == 1,
              "%s", "expected global concrete type index 1");

    err = wasm_struct_new(m, 1, struct_fields, 2, &struct_ref);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        struct_header = wasm__runtime_get_gc_object(m, &struct_ref);
        CHECK_MSG(t, struct_header != NULL, "%s", "expected struct allocation to produce a GC object");
        if (struct_header != NULL) {
            CHECK_MSG(t, struct_header->kind == WASM_GC_OBJ_STRUCT, "%s", "expected struct object kind");
            CHECK_MSG(t, struct_header->type_index == 1, "%s", "expected struct object type index 1");
            err = wasm__gc_struct_get_field((const wasm_gc_struct_t*)struct_header,
                                            wasm__module_const_structtype(m, 1),
                                            0,
                                            0,
                                            &loaded);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) WASM_CHECK_I32(t, loaded.of.i32, 9);
        }

        err = wasm_struct_get_field(m, struct_ref, 0, &loaded);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            CHECK_MSG(t, loaded.type == WASM_TYPE_I32, "%s", "expected public struct.get to return i32");
            WASM_CHECK_I32(t, loaded.of.i32, 9);
        }

        err = wasm_struct_set_field(m, struct_ref, 1, wasm_i32(44));
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            err = wasm_struct_get_field(m, struct_ref, 1, &loaded);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) WASM_CHECK_I32(t, loaded.of.i32, 44);
        }

        err = wasm_struct_set_field(m, struct_ref, 0, wasm_i32(12));
        CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected immutable struct field rejection");

        err = wasm_struct_set_field(m, struct_ref, 1, wasm_i64(12));
        CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected struct field type mismatch");

        err = wasm_struct_get_field(m, wasm_ref_null(WASM_TYPE_NONE), 0, &loaded);
        CHECK_MSG(t, err == WASM_ERR_TRAP, "%s", "expected null struct ref to trap");
        held_struct = wasm_ref_hold(&rt, struct_ref);
        CHECK(t, held_struct != NULL);
    }

    err = wasm_array_new(m, 2, array_values, 2, &array_ref);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        array_header = wasm__runtime_get_gc_object(m, &array_ref);
        CHECK_MSG(t, array_header != NULL, "%s", "expected array allocation to produce a GC object");
        if (array_header != NULL) {
            CHECK_MSG(t, array_header->kind == WASM_GC_OBJ_ARRAY, "%s", "expected array object kind");
            CHECK_MSG(t, array_header->type_index == 2, "%s", "expected array object type index 2");
            err = wasm__gc_array_get((const wasm_gc_array_t*)array_header,
                                     wasm__module_const_arraytype(m, 2),
                                     1,
                                     0,
                                     &loaded);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) WASM_CHECK_I32(t, loaded.of.i32, 20);
        }

        err = wasm_array_length(m, array_ref, &array_len);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, (int32_t)array_len, 2);

        err = wasm_array_get_elem(m, array_ref, 1, &loaded);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            CHECK_MSG(t, loaded.type == WASM_TYPE_I32, "%s", "expected public array.get to return i32");
            WASM_CHECK_I32(t, loaded.of.i32, 20);
        }

        err = wasm_array_set_elem(m, array_ref, 0, wasm_i32(4660));
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            err = wasm_array_get_elem(m, array_ref, 0, &loaded);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) WASM_CHECK_I32(t, loaded.of.i32, 4660);
        }

        err = wasm_array_set_elem(m, array_ref, 0, wasm_i64(1));
        CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected array element type mismatch");

        err = wasm_array_get_elem(m, array_ref, 9, &loaded);
        CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected array element bounds rejection");

        if (held_struct) {
            err = wasm_array_length(m, *held_struct, &array_len);
            CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected wrong-kind array length rejection");
        }

        err = wasm_struct_get_field(m, array_ref, 0, &loaded);
        CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected wrong-kind struct access rejection");

        err = wasm_array_length(m, wasm_i31ref(7), &array_len);
        CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected i31 array length rejection");
    }

    if (held_struct) wasm_ref_release(&rt, held_struct);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case inspect_cases[] = {
    { "introspection: exports, custom sections, and signatures are queryable", test_introspection_helpers },
    { "public api: import metadata is queryable", test_import_introspection_api },
    { "public api: module userdata is visible in host callbacks", test_module_userdata_api },
    { "public api: shared host imports still see the right module instance", test_module_userdata_api_multiple_modules },
    { "wasmgc: public helpers expose GC types and host allocators", test_gc_public_type_helpers_and_host_allocators },
    { "public api: NULL handles are rejected without crashing", test_public_api_tolerates_null_handles },
    { "public api: fuel helpers meter finite execution", test_public_fuel_helpers_account_for_finite_calls },
    { "public api: fuel helpers stop infinite loops", test_public_fuel_helpers_trap_in_infinite_loop },
};
const test_suite inspect_suite = { "inspect", inspect_cases, COUNTOF(inspect_cases) };

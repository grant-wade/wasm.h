#include "support.h"

TEST(test_compile_inspects_without_imports_and_instantiates_repeatedly) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x03, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60,
        0x00, 0x01, 0x7f, 0x60, 0x00, 0x00, 0x02, 0x2e, 0x04, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x61, 0x64, 0x64, 0x00,
        0x00, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x62,
        0x61, 0x73, 0x65, 0x03, 0x7f, 0x00, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x74, 0x61, 0x62, 0x01, 0x70, 0x00, 0x01,
        0x03, 0x03, 0x02, 0x01, 0x02, 0x06, 0x06, 0x01, 0x7f, 0x01, 0x23, 0x00, 0x0b, 0x07, 0x08, 0x01, 0x04, 0x62,
        0x75, 0x6d, 0x70, 0x00, 0x01, 0x08, 0x01, 0x02, 0x0a, 0x1e, 0x02, 0x0c, 0x00, 0x23, 0x01, 0x41, 0x01, 0x10,
        0x00, 0x24, 0x01, 0x23, 0x01, 0x0b, 0x0f, 0x00, 0x41, 0x00, 0x2d, 0x00, 0x00, 0x41, 0xe8, 0x00, 0x47, 0x04,
        0x40, 0x00, 0x0b, 0x0b, 0x0b, 0x08, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x02, 0x68, 0x69,
    };
    static const uint8_t trapping_start_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7f,
        0x03, 0x03, 0x02, 0x00, 0x01, 0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x01, 0x08, 0x01, 0x00, 0x0a, 0x0a, 0x02,
        0x03, 0x00, 0x00, 0x0b, 0x04, 0x00, 0x41, 0x01, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* compiled;
    wasm_module_t* first = NULL;
    wasm_module_t* second = NULL;
    wasm_module_t* m;
    wasm_memory_t memory;
    wasm_table_t table;
    wasm_value_t table_elems[1];
    wasm_value_t base = wasm_i32(10);
    wasm_memory_import_t memory_import;
    wasm_table_import_t table_import;
    wasm_global_import_t global_import;
    wasm_value_t result = wasm_i32(0);
    wasm_export_kind_t kind = WASM_EXPORT_FUNC;
    uint32_t index = 0;
    uint8_t byte = 0;
    wasm_error_t err;

    wasm_init(&rt);
    compiled = wasm_compile(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, compiled != NULL, "expected compile to need no imports: %s", rt.error_msg);
    if (!compiled) {
        wasm_destroy(&rt);
        return;
    }
    CHECK(t, wasm_import_count(compiled) == 4u);
    CHECK(t, wasm_find_export(compiled, "bump", &kind, &index) && kind == WASM_EXPORT_FUNC);
    CHECK(t, wasm_func_result_count(compiled, index) == 1u);
    err = wasm_call(compiled, "bump", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_MALFORMED && strstr(rt.error_msg, "not instantiated"), "%s", rt.error_msg);
    CHECK(t, wasm_memory_read(compiled, 0, 0, &byte, 1) != WASM_OK);
    CHECK(t, wasm_memory_data(compiled, 0) == NULL);
    CHECK(t, wasm_module_global(compiled, 0) == NULL);

    m = wasm_instantiate(&rt, compiled, NULL);
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "env"), "expected unresolved imports to fail: %s", rt.error_msg);
    for (index = 0; index < wasm_import_count(compiled); index++) {
        CHECK(t, wasm_import_check(&rt, compiled, index) == WASM_ERR_UNKNOWN_IMPORT);
    }
    CHECK(t, wasm_import_check(&rt, compiled, wasm_import_count(compiled)) == WASM_ERR_MALFORMED);

    memset(&memory, 0, sizeof(memory));
    memory.data = (uint8_t*)calloc(1, WASM_PAGE_SIZE);
    memory.pages = 1;
    memory.max_pages = WASM_MEMORY32_MAX_PAGES;
    memset(&table, 0, sizeof(table));
    table_elems[0] = wasm_ref_null(WASM_TYPE_FUNCREF);
    table.reftype = WASM_TYPE_FUNCREF;
    table.reftype_info.type = WASM_TYPE_FUNCREF;
    table.reftype_info.nullable = 1;
    table.elems = table_elems;
    table.size = 1;
    table.max_size = UINT32_MAX;
    memory_import.module = "env";
    memory_import.name = "mem";
    memory_import.memory = &memory;
    table_import.module = "env";
    table_import.name = "tab";
    table_import.table = &table;
    memset(&global_import, 0, sizeof(global_import));
    global_import.module = "env";
    global_import.name = "base";
    global_import.type = WASM_TYPE_I32;
    global_import.value = &base;
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "add", "ii(i)", wasm_test_host_add, NULL));
    WASM_CHECK_OK(t, wasm_register_memory_import(&rt, &memory_import));
    WASM_CHECK_OK(t, wasm_register_table_import(&rt, &table_import));
    WASM_CHECK_OK(t, wasm_register_global_import(&rt, &global_import));
    for (index = 0; index < wasm_import_count(compiled); index++) {
        WASM_CHECK_OK(t, wasm_import_check(&rt, compiled, index));
    }

    first = wasm_instantiate(&rt, compiled, NULL);
    CHECK_MSG(t, first != NULL, "%s", rt.error_msg);
    second = wasm_instantiate(&rt, compiled, NULL);
    CHECK_MSG(t, second != NULL, "%s", rt.error_msg);
    if (first && second) {
        CHECK(t, memory.data[0] == 'h' && memory.data[1] == 'i');
        err = wasm_call(first, "bump", NULL, 0, &result, 1);
        WASM_CHECK_OK(t, err);
        err = wasm_call(first, "bump", NULL, 0, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, 12);
        err = wasm_call(second, "bump", NULL, 0, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, 11);
    }
    wasm_module_free(second);
    wasm_module_free(first);
    wasm_module_free(compiled);
    free(memory.data);

    compiled = wasm_compile(&rt, trapping_start_bytes, sizeof(trapping_start_bytes));
    CHECK_MSG(t, compiled != NULL, "expected compile not to run the start function: %s", rt.error_msg);
    m = compiled ? wasm_instantiate(&rt, compiled, NULL) : NULL;
    CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_UNREACHABLE && strstr(rt.error_msg, "unreachable"),
              "expected the start function to trap: %s", rt.error_msg);
    wasm_module_free(m);
    wasm_module_free(compiled);
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_host_context(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results, void* userdata) {
    const int32_t* context = (const int32_t*)wasm_module_userdata(mod);

    (void)args;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    results[0] = wasm_i32(context ? *context : -1);
    return WASM_OK;
}

TEST(test_instantiate_in_another_runtime_with_userdata) {
    /* (import "env" "ctx" (func (result i32))), a start function storing its result in a global,
       and an export "seen" returning the global. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x00, 0x00,
        0x02, 0x0b, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x63, 0x74, 0x78, 0x00, 0x00, 0x03, 0x03, 0x02, 0x01, 0x00,
        0x06, 0x06, 0x01, 0x7f, 0x01, 0x41, 0x00, 0x0b, 0x07, 0x08, 0x01, 0x04, 0x73, 0x65, 0x65, 0x6e, 0x00, 0x02,
        0x08, 0x01, 0x01, 0x0a, 0x0d, 0x02, 0x06, 0x00, 0x10, 0x00, 0x24, 0x00, 0x0b, 0x04, 0x00, 0x23, 0x00, 0x0b,
    };
    wasm_runtime_t* owner = wasm_runtime_new(wasm_libc_allocator(), NULL);
    wasm_runtime_t* host = wasm_runtime_new(wasm_libc_allocator(), NULL);
    wasm_runtime_t* wrong = wasm_runtime_new(wasm_libc_allocator(), NULL);
    wasm_module_t* compiled = NULL;
    wasm_module_t* first = NULL;
    wasm_module_t* second = NULL;
    wasm_value_t result = wasm_i32(0);
    int32_t first_context = 77;
    int32_t second_context = 5;

    CHECK(t, owner && host && wrong);
    if (!owner || !host || !wrong) goto done;
    compiled = wasm_compile(owner, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, compiled != NULL, "%s", wasm_runtime_error_message(owner));
    if (!compiled) goto done;
    WASM_CHECK_OK(t, wasm_bind_host_func(host, "env", "ctx", "(i)", wasm_test_host_context, NULL));
    WASM_CHECK_OK(t, wasm_bind_host_func(wrong, "env", "ctx", "i(i)", wasm_test_host_context, NULL));

    CHECK(t, wasm_import_check(owner, compiled, 0) == WASM_ERR_UNKNOWN_IMPORT);
    CHECK_MSG(t, strstr(wasm_runtime_error_message(owner), "env.ctx") != NULL, "%s",
              wasm_runtime_error_message(owner));
    CHECK(t, wasm_import_check(wrong, compiled, 0) == WASM_ERR_TYPE_MISMATCH);
    WASM_CHECK_OK(t, wasm_import_check(host, compiled, 0));
    CHECK(t, wasm_instantiate(wrong, compiled, NULL) == NULL);
    CHECK(t, wasm_runtime_last_error(wrong) == WASM_ERR_TYPE_MISMATCH);

    first = wasm_instantiate(host, compiled, &first_context);
    second = wasm_instantiate(host, compiled, &second_context);
    CHECK_MSG(t, first && second, "%s", wasm_runtime_error_message(host));
    if (first && second) {
        CHECK(t, wasm_module_runtime(first) == host);
        CHECK(t, wasm_module_userdata(first) == &first_context);
        WASM_CHECK_OK(t, wasm_call(first, "seen", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 77);
        WASM_CHECK_OK(t, wasm_call(second, "seen", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 5);
    }
    CHECK(t, wasm_instantiate(host, first, NULL) == NULL);
    CHECK(t, wasm_runtime_last_error(host) == WASM_ERR_MALFORMED);
    CHECK(t, wasm_import_check(host, first, 0) == WASM_ERR_MALFORMED);

done:
    wasm_module_free(second);
    wasm_module_free(first);
    wasm_module_free(compiled);
    wasm_runtime_free(wrong);
    wasm_runtime_free(host);
    wasm_runtime_free(owner);
}

TEST(test_load_rejects_misordered_sections) {
    /* Two export sections; the second used to replace the first and leak it. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x03, 0x02, 0x00,
        0x00, 0x07, 0x09, 0x02, 0x01, 0x61, 0x00, 0x00, 0x01, 0x62, 0x00, 0x01, 0x07, 0x09, 0x02, 0x01, 0x61, 0x00,
        0x00, 0x01, 0x62, 0x00, 0x01, 0x0a, 0x07, 0x02, 0x02, 0x00, 0x0b, 0x02, 0x00, 0x0b,
    };
    wasm_builder_t late_types = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_MALFORMED && strstr(rt.error_msg, "section 7 is repeated"),
              "expected a repeated export section to be malformed, got: %s", rt.error_msg);
    wasm_module_free(m);
    m = wasm_compile(&rt, module_bytes, sizeof(module_bytes));
    CHECK(t, m == NULL);
    wasm_module_free(m);

    emit_header(&late_types);
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 0);
        emit_section(&late_types, 3, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 0);
        emit_section(&late_types, 1, sec.buf, sec.len);
    }
    m = wasm_load(&rt, late_types.buf, late_types.len);
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "section out of order: section 1 must come earlier"),
              "expected a type section after the function section to be malformed, got: %s", rt.error_msg);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_bad_magic) {
    uint8_t bad[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x00, 0x00, 0x00 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_load(&rt, bad, sizeof(bad));
    if (m) {
        CHECK_MSG(t, false, "%s", "should have rejected");
        wasm_module_free(m);
    } else {
        CHECK(t, rt.last_error == WASM_ERR_INVALID_MAGIC);
    }
    wasm_destroy(&rt);
}

static const test_case load_cases[] = {
    { "public api: compile inspects without imports and instantiates repeatedly", test_compile_inspects_without_imports_and_instantiates_repeatedly },
    { "public api: instantiate in another runtime with userdata", test_instantiate_in_another_runtime_with_userdata },
    { "load: repeated and out-of-order sections are malformed", test_load_rejects_misordered_sections },
    { "reject bad magic number", test_bad_magic },
};
const test_suite load_suite = { "load", load_cases, COUNTOF(load_cases) };

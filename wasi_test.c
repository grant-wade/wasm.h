#include <string.h>

#define WL_IMPL
#include "wl.h"

#define WASI_IMPL
#include "wasi.h"

static const uint8_t wasi_test_core_header[8] = {
    0x00, 0x61, 0x73, 0x6D,
    0x01, 0x00, 0x00, 0x00,
};

static const uint8_t wasi_test_component_header_old[8] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
};

static const uint8_t wasi_test_component_header_new[8] = {
    0x00, 0x61, 0x73, 0x6D,
    0x01, 0x00, 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_sections[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x00, 0x04, 0x03, 'w', 'i', 't',
    0x02, 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_core_module[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x01, 0x08,
    0x00, 0x61, 0x73, 0x6D,
    0x01, 0x00, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_import_export[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x05,
    0x01,
    0x40, 0x00, 0x01, 0x00,
    0x0A, 0x0D,
    0x01,
    0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g',
    0x01, 0x00,
    0x0B, 0x0A,
    0x01,
    0x00, 0x04, 'p', 'i', 'n', 'g',
    0x01, 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_component_instances[] = {
    0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00,
    0x0a, 0x0d, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x05, 0x0f, 0x01, 0x01, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x04, 0x1e,
    0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00,
    0x0a, 0x0d, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x05, 0x0f, 0x01, 0x00, 0x00, 0x01, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_start[] = {
    0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00,
    0x0a, 0x08, 0x01, 0x00, 0x03, 'r', 'u', 'n', 0x01, 0x00,
    0x09, 0x03, 0x00, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_alias_variants[] = {
    0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00,
    0x0a, 0x0d, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x05, 0x0f, 0x01, 0x01, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x06, 0x0d, 0x01, 0x01, 0x00, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g',
    0x04, 0x0f, 0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x06, 0x05, 0x01, 0x03, 0x02, 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_core_instances[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x02, 0x12,
    0x02,
    0x01,
    0x01,
    0x03, 'l', 'o', 'g',
    0x00, 0x00,
    0x00,
    0x02,
    0x01,
    0x03, 'd', 'e', 'p',
    0x12, 0x01,
};

static const uint8_t wasi_test_component_with_extended_types[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x28,
    0x0B,
    0x40, 0x00, 0x01, 0x00,
    0x41, 0x00,
    0x42, 0x00,
    0x3F, 0x7F, 0x00,
    0x72, 0x01, 0x01, 'x', 0x79,
    0x71, 0x01, 0x02, 'o', 'k', 0x01, 0x73, 0x00,
    0x70, 0x7D,
    0x6B, 0x73,
    0x6A, 0x01, 0x73, 0x01, 0x79,
    0x65, 0x01, 0x73,
    0x66, 0x01, 0x7D,
};

static const uint8_t wasi_test_component_with_versioned_names[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x05,
    0x01,
    0x40, 0x00, 0x01, 0x00,
    0x0A, 0x17,
    0x01,
    0x00, 0x12, 'w', 'a', 's', 'i', ':', 'c', 'l', 'i', '/', 'r', 'u', 'n', '@', '0', '.', '3', '.', '0',
    0x01, 0x00,
    0x0B, 0x18,
    0x01,
    0x00, 0x12, 'w', 'a', 's', 'i', ':', 'c', 'l', 'i', '/', 'r', 'u', 'n', '@', '0', '.', '3', '.', '0',
    0x01, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_canon_builtins[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x07,
    0x02,
    0x3F, 0x7F, 0x00,
    0x65, 0x01, 0x73,
    0x08, 0x11,
    0x08,
    0x02, 0x00,
    0x03, 0x00,
    0x04, 0x00,
    0x1F,
    0x0D,
    0x0E, 0x01,
    0x15, 0x01,
    0x09, 0x01, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_async_lower_option[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x05,
    0x01,
    0x40, 0x00, 0x01, 0x00,
    0x0A, 0x06,
    0x01,
    0x00, 0x01, 'f',
    0x01, 0x00,
    0x08, 0x06,
    0x01,
    0x01, 0x00, 0x00, 0x01, 0x06,
};

WL_TEST(test_wasi_detects_core_modules) {
    WL_CHECK(t, wasi_detect_binary_kind(wasi_test_core_header, sizeof(wasi_test_core_header)) ==
                    WASI_BINARY_KIND_CORE_MODULE);
}

WL_TEST(test_wasi_detects_component_versions) {
    WL_CHECK(t, wasi_detect_binary_kind(wasi_test_component_header_old, sizeof(wasi_test_component_header_old)) ==
                    WASI_BINARY_KIND_COMPONENT);
    WL_CHECK(t, wasi_detect_binary_kind(wasi_test_component_header_new, sizeof(wasi_test_component_header_new)) ==
                    WASI_BINARY_KIND_COMPONENT);
}

WL_TEST(test_wasi_rejects_core_module_loads) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine, wasi_test_core_header, sizeof(wasi_test_core_header));
    WL_CHECK(t, component == NULL);
    WL_CHECK(t, engine.last_error == WASI_ERR_UNSUPPORTED_BINARY);
    WL_CHECK(t, strstr(engine.error_msg, "wasm_load") != NULL);

    wasi_destroy(&engine);
}

WL_TEST(test_wasi_loads_component_section_framing) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[256];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_sections,
                          sizeof(wasi_test_component_with_sections));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_binary_kind(component) == WASI_BINARY_KIND_COMPONENT);
    WL_CHECK(t, wasi_component_status(component) == WASI_COMPONENT_STATUS_PARSED_STRUCTURE);
    WL_CHECK(t, wasi_component_layer(component) == 0x0D);
    WL_CHECK(t, wasi_component_section_count(component) == 2u);
    WL_CHECK(t, wasi_component_section_id(component, 0) == 0u);
    WL_CHECK(t, wasi_component_section_size(component, 0) == 4u);
    WL_CHECK(t, strcmp(wasi_component_section_name(component, 0), "wit") == 0);
    WL_CHECK(t, wasi_component_section_id(component, 1) == 2u);
    WL_CHECK(t, wasi_component_section_name(component, 1) == NULL);
    WL_CHECK(t, strcmp(wasi_component_status_string(wasi_component_status(component)),
                       "component structure parsed") == 0);
    WL_CHECK(t, wasi_component_core_module_count(component) == 0u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "sections=2") != NULL);
    WL_CHECK(t, strstr(dump, "core-modules=0") != NULL);
    WL_CHECK(t, strstr(dump, "custom=wit") != NULL);
    WL_CHECK(t, strstr(dump, "kind=core-instance") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_extended_component_types) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[1024];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_extended_types,
                          sizeof(wasi_test_component_with_extended_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_type_count(component) == 11u);
    WL_CHECK(t, wasi_component_type_kind(component, 0) == WASI_COMPONENT_TYPE_KIND_FUNC);
    WL_CHECK(t, wasi_component_type_kind(component, 1) == WASI_COMPONENT_TYPE_KIND_COMPONENT);
    WL_CHECK(t, wasi_component_type_kind(component, 2) == WASI_COMPONENT_TYPE_KIND_INSTANCE);
    WL_CHECK(t, wasi_component_type_kind(component, 3) == WASI_COMPONENT_TYPE_KIND_RESOURCE);
    WL_CHECK(t, wasi_component_type_kind(component, 4) == WASI_COMPONENT_TYPE_KIND_DEFINED);
    WL_CHECK(t, strcmp(wasi_component_type_kind_string(wasi_component_type_kind(component, 3)), "resource") == 0);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "type[1]: kind=component decls=0") != NULL);
    WL_CHECK(t, strstr(dump, "type[2]: kind=instance decls=0") != NULL);
    WL_CHECK(t, strstr(dump, "type[3]: kind=resource async=0 rep=i32") != NULL);
    WL_CHECK(t, strstr(dump, "type[4]: kind=defined detail=record") != NULL);
    WL_CHECK(t, strstr(dump, "type[9]: kind=defined detail=future") != NULL);
    WL_CHECK(t, strstr(dump, "type[10]: kind=defined detail=stream") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_resolves_versioned_interface_names) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[768];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_versioned_names,
                          sizeof(wasi_test_component_with_versioned_names));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, strcmp(wasi_component_import_name(component, 0), "wasi:cli/run@0.3.0") == 0);
    WL_CHECK(t, strcmp(wasi_component_import_interface_name(component, 0), "wasi:cli/run") == 0);
    WL_CHECK(t, strcmp(wasi_component_import_interface_version(component, 0), "0.3.0") == 0);
    WL_CHECK(t, strcmp(wasi_component_export_name(component, 0), "wasi:cli/run@0.3.0") == 0);
    WL_CHECK(t, strcmp(wasi_component_export_interface_name(component, 0), "wasi:cli/run") == 0);
    WL_CHECK(t, strcmp(wasi_component_export_interface_version(component, 0), "0.3.0") == 0);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "import-interface[0]: name=wasi:cli/run version=0.3.0") != NULL);
    WL_CHECK(t, strstr(dump, "export-interface[0]: name=wasi:cli/run version=0.3.0") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_canon_builtins) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[1024];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_canon_builtins,
                          sizeof(wasi_test_component_with_canon_builtins));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_canon_count(component) == 8u);
    WL_CHECK(t, wasi_component_canon_kind(component, 0) == WASI_COMPONENT_CANON_KIND_RESOURCE_NEW);
    WL_CHECK(t, wasi_component_canon_kind(component, 1) == WASI_COMPONENT_CANON_KIND_RESOURCE_DROP);
    WL_CHECK(t, wasi_component_canon_kind(component, 2) == WASI_COMPONENT_CANON_KIND_RESOURCE_REP);
    WL_CHECK(t, wasi_component_canon_kind(component, 3) == WASI_COMPONENT_CANON_KIND_WAITABLE_SET_NEW);
    WL_CHECK(t, wasi_component_canon_kind(component, 4) == WASI_COMPONENT_CANON_KIND_SUBTASK_DROP);
    WL_CHECK(t, wasi_component_canon_kind(component, 5) == WASI_COMPONENT_CANON_KIND_STREAM_NEW);
    WL_CHECK(t, wasi_component_canon_kind(component, 6) == WASI_COMPONENT_CANON_KIND_FUTURE_NEW);
    WL_CHECK(t, wasi_component_canon_kind(component, 7) == WASI_COMPONENT_CANON_KIND_TASK_RETURN);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "canon[0]: kind=resource.new async=0 type=0") != NULL);
    WL_CHECK(t, strstr(dump, "canon[3]: kind=waitable-set.new async=0") != NULL);
    WL_CHECK(t, strstr(dump, "canon[7]: kind=task.return async=0") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_current_canon_async_option) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[512];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_async_lower_option,
                          sizeof(wasi_test_component_with_async_lower_option));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_canon_count(component) == 1u);
    WL_CHECK(t, wasi_component_canon_kind(component, 0) == WASI_COMPONENT_CANON_KIND_LOWER);
    WL_CHECK(t, wasi_component_canon_async_flag(component, 0) == 1u);
    WL_CHECK(t, wasi_component_canon_option_count(component, 0) == 1u);
    WL_CHECK(t, wasi_component_canon_option_code(component, 0, 0) == 0x06u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "canon[0]: kind=lower async=1 func=0 options=1") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_extracts_embedded_core_modules) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[256];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_core_module,
                          sizeof(wasi_test_component_with_core_module));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_section_count(component) == 1u);
    WL_CHECK(t, wasi_component_core_module_count(component) == 1u);
    WL_CHECK(t, wasi_component_core_module_at(component, 0) != NULL);
    WL_CHECK(t, wasi_component_core_module_error(component, 0) == WASM_OK);
    WL_CHECK(t, wasi_component_core_module_error_message(component, 0) == NULL);
    WL_CHECK(t, wasm_export_count(wasi_component_core_module_at(component, 0)) == 0u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "core-modules=1") != NULL);
    WL_CHECK(t, strstr(dump, "core-module[0]:") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_component_imports_and_exports) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[512];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_import_export,
                          sizeof(wasi_test_component_with_import_export));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_import_count(component) == 1u);
    WL_CHECK(t, strcmp(wasi_component_import_name(component, 0), "host-log") == 0);
    WL_CHECK(t, wasi_component_import_kind(component, 0) == WASI_COMPONENT_EXTERN_KIND_FUNC);
    WL_CHECK(t, wasi_component_import_type_index(component, 0) == 0u);
    WL_CHECK(t, wasi_component_type_count(component) == 1u);
    WL_CHECK(t, wasi_component_type_kind(component, 0) == WASI_COMPONENT_TYPE_KIND_FUNC);
    WL_CHECK(t, strcmp(wasi_component_type_kind_string(wasi_component_type_kind(component, 0)), "func") == 0);
    WL_CHECK(t, wasi_component_func_type_param_count(component, 0) == 0u);
    WL_CHECK(t, wasi_component_func_type_result_count(component, 0) == 0u);

    WL_CHECK(t, wasi_component_export_count(component) == 1u);
    WL_CHECK(t, strcmp(wasi_component_export_name(component, 0), "ping") == 0);
    WL_CHECK(t, wasi_component_export_kind(component, 0) == WASI_COMPONENT_EXTERN_KIND_FUNC);
    WL_CHECK(t, wasi_component_export_index(component, 0) == 1u);
    WL_CHECK(t, !wasi_component_export_has_type(component, 0));
    WL_CHECK(t, wasi_component_export_type_index(component, 0) == 0u);
    WL_CHECK(t, strcmp(wasi_component_extern_kind_string(wasi_component_export_kind(component, 0)), "func") == 0);
    {
        uint32_t resolved_type_index = UINT32_MAX;
        WL_CHECK(t, !wasi_component_export_func_type_index(component, 0, &resolved_type_index));
        WL_CHECK(t, resolved_type_index == 0u);
    }

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "import[0]: name=host-log kind=func type=0") != NULL);
    WL_CHECK(t, strstr(dump, "export[0]: name=ping kind=func index=1") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_component_core_instances) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[512];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_core_instances,
                          sizeof(wasi_test_component_with_core_instances));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_core_instance_count(component) == 2u);
    WL_CHECK(t, wasi_component_core_instance_kind(component, 0) == WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS);
    WL_CHECK(t, strcmp(wasi_component_core_instance_kind_string(wasi_component_core_instance_kind(component, 0)),
                       "from-exports") == 0);
    WL_CHECK(t, wasi_component_core_instance_export_count(component, 0) == 1u);
    WL_CHECK(t, strcmp(wasi_component_core_instance_export_name(component, 0, 0), "log") == 0);
    WL_CHECK(t, wasi_component_core_instance_export_kind(component, 0, 0) == WASM_EXPORT_FUNC);
    WL_CHECK(t, wasi_component_core_instance_export_index(component, 0, 0) == 0u);

    WL_CHECK(t, wasi_component_core_instance_kind(component, 1) == WASI_COMPONENT_CORE_INSTANCE_KIND_INSTANTIATE);
    WL_CHECK(t, wasi_component_core_instance_module_index(component, 1) == 2u);
    WL_CHECK(t, wasi_component_core_instance_arg_count(component, 1) == 1u);
    WL_CHECK(t, strcmp(wasi_component_core_instance_arg_name(component, 1, 0), "dep") == 0);
    WL_CHECK(t, wasi_component_core_instance_arg_kind(component, 1, 0) == 0x12u);
    WL_CHECK(t, strcmp(wasi_component_core_sort_string(wasi_component_core_instance_arg_kind(component, 1, 0)),
                       "instance") == 0);
    WL_CHECK(t, wasi_component_core_instance_arg_index(component, 1, 0) == 1u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "core-instance[0]: kind=from-exports exports=1") != NULL);
    WL_CHECK(t, strstr(dump, "core-instance-export[0,0]: name=log kind=func index=0") != NULL);
    WL_CHECK(t, strstr(dump, "core-instance[1]: kind=instantiate module=2 args=1") != NULL);
    WL_CHECK(t, strstr(dump, "core-instance-arg[1,0]: name=dep kind=instance index=1") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_component_instances) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[768];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_component_instances,
                          sizeof(wasi_test_component_with_component_instances));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_instance_count(component) == 2u);
    WL_CHECK(t, wasi_component_instance_kind(component, 0) == WASI_COMPONENT_INSTANCE_KIND_FROM_EXPORTS);
    WL_CHECK(t, strcmp(wasi_component_instance_kind_string(wasi_component_instance_kind(component, 0)),
                       "from-exports") == 0);
    WL_CHECK(t, wasi_component_instance_export_count(component, 0) == 1u);
    WL_CHECK(t, strcmp(wasi_component_instance_export_name(component, 0, 0), "host-log") == 0);
    WL_CHECK(t, wasi_component_instance_export_kind(component, 0, 0) == WASI_COMPONENT_EXTERN_KIND_FUNC);
    WL_CHECK(t, wasi_component_instance_export_index(component, 0, 0) == 0u);

    WL_CHECK(t, wasi_component_instance_kind(component, 1) == WASI_COMPONENT_INSTANCE_KIND_INSTANTIATE);
    WL_CHECK(t, wasi_component_instance_component_index(component, 1) == 0u);
    WL_CHECK(t, wasi_component_instance_arg_count(component, 1) == 1u);
    WL_CHECK(t, strcmp(wasi_component_instance_arg_name(component, 1, 0), "host-log") == 0);
    WL_CHECK(t, wasi_component_instance_arg_kind(component, 1, 0) == WASI_COMPONENT_EXTERN_KIND_FUNC);
    WL_CHECK(t, wasi_component_instance_arg_index(component, 1, 0) == 0u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "component-instance[0]: kind=from-exports exports=1") != NULL);
    WL_CHECK(t, strstr(dump, "component-instance-export[0,0]: name=host-log kind=func index=0") != NULL);
    WL_CHECK(t, strstr(dump, "component-instance[1]: kind=instantiate component=0 args=1") != NULL);
    WL_CHECK(t, strstr(dump, "component-instance-arg[1,0]: name=host-log kind=func index=0") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_extracts_nested_components) {
    wasi_engine_t engine;
    wasi_component_t* component;
    const wasi_component_t* nested;
    char dump[896];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_component_instances,
                          sizeof(wasi_test_component_with_component_instances));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_nested_component_count(component) == 1u);
    WL_CHECK(t, wasi_component_nested_component_offset(component, 0) == 49u);
    WL_CHECK(t, wasi_component_nested_component_size(component, 0) == 30u);
    nested = wasi_component_nested_component_at(component, 0);
    WL_REQUIRE(t, nested != NULL);
    WL_CHECK(t, wasi_component_binary_kind(nested) == WASI_BINARY_KIND_COMPONENT);
    WL_CHECK(t, wasi_component_section_count(nested) == 2u);
    WL_CHECK(t, wasi_component_import_count(nested) == 1u);
    WL_CHECK(t, strcmp(wasi_component_import_name(nested, 0), "host-log") == 0);
    WL_CHECK(t, wasi_component_instance_count(nested) == 0u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "nested-component[0]: offset=49 size=30 sections=2") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_alias_variants) {
    wasi_engine_t engine;
    wasi_component_t* component;
    const wasi_component_t* nested;
    char dump[896];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_alias_variants,
                          sizeof(wasi_test_component_with_alias_variants));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_alias_kind(component, 0) == WASI_COMPONENT_ALIAS_KIND_INSTANCE_EXPORT);
    WL_CHECK(t, !wasi_component_alias_sort_is_core(component, 0));
    WL_CHECK(t, wasi_component_alias_sort_code(component, 0) == 0x01u);
    WL_CHECK(t, wasi_component_alias_extern_kind(component, 0) == WASI_COMPONENT_EXTERN_KIND_FUNC);
    WL_CHECK(t, wasi_component_alias_instance_index(component, 0) == 0u);
    WL_CHECK(t, strcmp(wasi_component_alias_name(component, 0), "host-log") == 0);

    WL_CHECK(t, wasi_component_nested_component_count(component) == 1u);
    nested = wasi_component_nested_component_at(component, 0);
    WL_REQUIRE(t, nested != NULL);
    WL_CHECK(t, wasi_component_alias_count(nested) == 1u);
    WL_CHECK(t, wasi_component_alias_kind(nested, 0) == WASI_COMPONENT_ALIAS_KIND_OUTER);
    WL_CHECK(t, !wasi_component_alias_sort_is_core(nested, 0));
    WL_CHECK(t, wasi_component_alias_sort_code(nested, 0) == 0x03u);
    WL_CHECK(t, wasi_component_alias_extern_kind(nested, 0) == WASI_COMPONENT_EXTERN_KIND_TYPE);
    WL_CHECK(t, wasi_component_alias_outer_count(nested, 0) == 1u);
    WL_CHECK(t, wasi_component_alias_outer_index(nested, 0) == 0u);
    WL_CHECK(t, wasi_component_alias_name(nested, 0) == NULL);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "alias[0]: kind=instance-export target=func instance=0 name=host-log") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_component_start) {
    wasi_engine_t engine;
    wasi_component_t* component;
    char dump[512];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_start,
                          sizeof(wasi_test_component_with_start));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_has_start(component));
    WL_CHECK(t, wasi_component_start_func_index(component) == 0u);
    WL_CHECK(t, wasi_component_start_arg_count(component) == 0u);
    WL_CHECK(t, wasi_component_start_result_count(component) == 0u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "start: func=0 args=0 results=0") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_rejects_bad_magic) {
    uint8_t bad_header[8];
    wasi_engine_t engine;
    wasi_error_t err;

    memcpy(bad_header, wasi_test_component_header_old, sizeof(bad_header));
    bad_header[0] = 0x01;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_detect_binary_kind(bad_header, sizeof(bad_header)) == WASI_BINARY_KIND_UNKNOWN);
    WL_CHECK(t, wasi_load(&engine, bad_header, sizeof(bad_header)) == NULL);
    WL_CHECK(t, engine.last_error == WASI_ERR_INVALID_MAGIC);

    wasi_destroy(&engine);
}

int main(void) {
    static const wl_test_case cases[] = {
        WL_TEST_CASE(test_wasi_detects_core_modules),
        WL_TEST_CASE(test_wasi_detects_component_versions),
        WL_TEST_CASE(test_wasi_rejects_core_module_loads),
        WL_TEST_CASE(test_wasi_loads_component_section_framing),
        WL_TEST_CASE(test_wasi_extracts_embedded_core_modules),
        WL_TEST_CASE(test_wasi_parses_component_imports_and_exports),
        WL_TEST_CASE(test_wasi_parses_extended_component_types),
        WL_TEST_CASE(test_wasi_resolves_versioned_interface_names),
        WL_TEST_CASE(test_wasi_parses_component_core_instances),
        WL_TEST_CASE(test_wasi_parses_component_instances),
        WL_TEST_CASE(test_wasi_extracts_nested_components),
        WL_TEST_CASE(test_wasi_parses_alias_variants),
        WL_TEST_CASE(test_wasi_parses_component_start),
        WL_TEST_CASE(test_wasi_parses_canon_builtins),
        WL_TEST_CASE(test_wasi_parses_current_canon_async_option),
        WL_TEST_CASE(test_wasi_rejects_bad_magic),
    };

    return wl_test_run("wasi", cases, WL_COUNTOF(cases));
}
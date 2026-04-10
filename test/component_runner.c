#define WASI_IMPL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../wasi.h"

static void component_runner_usage(const char* argv0) {
    fprintf(stderr, "usage: %s <component.wasm> <mode> [args...]\n", argv0);
    fprintf(stderr, "  modes:\n");
    fprintf(stderr, "    detect-component\n");
    fprintf(stderr, "    detect-core\n");
    fprintf(stderr, "    load-ok\n");
    fprintf(stderr, "    load-status <unparsed|parsed-sections|parsed-structure>\n");
    fprintf(stderr, "    type-param-count <type_index> <expected>\n");
    fprintf(stderr, "    type-result-count <type_index> <expected>\n");
    fprintf(stderr, "    alias-count <expected>\n");
    fprintf(stderr, "    alias-kind <index> <core-instance-export|instance-export|outer>\n");
    fprintf(stderr, "    alias-name <index> <expected>\n");
    fprintf(stderr, "    core-instance-count <expected>\n");
    fprintf(stderr, "    core-instance-kind <index> <instantiate|from-exports>\n");
    fprintf(stderr, "    core-instance-export-name <instance_index> <export_index> <expected>\n");
    fprintf(stderr, "    core-instance-module <index> <expected_module_index>\n");
    fprintf(stderr, "    core-instance-arg-name <instance_index> <arg_index> <expected>\n");
    fprintf(stderr, "    component-instance-count <expected>\n");
    fprintf(stderr, "    component-instance-kind <index> <instantiate|from-exports>\n");
    fprintf(stderr, "    component-instance-export-name <instance_index> <export_index> <expected>\n");
    fprintf(stderr, "    component-instance-component <index> <expected_component_index>\n");
    fprintf(stderr, "    component-instance-arg-name <instance_index> <arg_index> <expected>\n");
    fprintf(stderr, "    nested-component-count <expected>\n");
    fprintf(stderr, "    nested-component-section-count <index> <expected>\n");
    fprintf(stderr, "    nested-component-import-name <component_index> <import_index> <expected>\n");
    fprintf(stderr, "    nested-component-alias-kind <component_index> <alias_index> <core-instance-export|instance-export|outer>\n");
    fprintf(stderr, "    nested-component-instance-kind <component_index> <instance_index> <instantiate|from-exports>\n");
    fprintf(stderr, "    start-present <0|1>\n");
    fprintf(stderr, "    start-func <expected_func_index>\n");
    fprintf(stderr, "    start-args <expected_count>\n");
    fprintf(stderr, "    start-results <expected_count>\n");
    fprintf(stderr, "    canon-count <expected>\n");
    fprintf(stderr, "    canon-kind <index> <lift|lower>\n");
    fprintf(stderr, "    canon-option-count <index> <expected>\n");
    fprintf(stderr, "    import-count <expected>\n");
    fprintf(stderr, "    export-count <expected>\n");
    fprintf(stderr, "    import-name <index> <expected>\n");
    fprintf(stderr, "    export-name <index> <expected>\n");
    fprintf(stderr, "    export-type <index> <expected_type_index>\n");
    fprintf(stderr, "    core-module-count <expected>\n");
    fprintf(stderr, "    dump-contains <substring>\n");
}

static unsigned char* component_runner_read_file(const char* path, size_t* out_size) {
    FILE* file;
    long file_size;
    unsigned char* bytes;

    if (out_size) *out_size = 0;
    file = fopen(path, "rb");
    if (!file) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    bytes = (unsigned char*)malloc((size_t)(file_size > 0 ? file_size : 1));
    if (!bytes) {
        fclose(file);
        return NULL;
    }
    if (file_size > 0 && fread(bytes, 1, (size_t)file_size, file) != (size_t)file_size) {
        free(bytes);
        fclose(file);
        return NULL;
    }

    fclose(file);
    if (out_size) *out_size = (size_t)file_size;
    return bytes;
}

static const char* component_runner_status_token(wasi_component_status_t status) {
    switch (status) {
        case WASI_COMPONENT_STATUS_UNPARSED:
            return "unparsed";
        case WASI_COMPONENT_STATUS_PARSED_SECTIONS:
            return "parsed-sections";
        case WASI_COMPONENT_STATUS_PARSED_STRUCTURE:
            return "parsed-structure";
        default:
            return "unknown";
    }
}

int main(int argc, char** argv) {
    const char* path;
    const char* mode;
    unsigned char* bytes = NULL;
    size_t size = 0;
    wasi_engine_t engine;
    wasi_component_t* component = NULL;
    wasi_binary_kind_t kind;
    char dump[1024];
    int exit_code = 1;

    if (argc < 3) {
        component_runner_usage(argv[0]);
        return 2;
    }

    path = argv[1];
    mode = argv[2];
    bytes = component_runner_read_file(path, &size);
    if (!bytes) {
        fprintf(stderr, "failed to read %s\n", path);
        return 1;
    }

    kind = wasi_detect_binary_kind(bytes, size);
    if (strcmp(mode, "detect-component") == 0) {
        exit_code = kind == WASI_BINARY_KIND_COMPONENT ? 0 : 1;
        goto cleanup;
    }
    if (strcmp(mode, "detect-core") == 0) {
        exit_code = kind == WASI_BINARY_KIND_CORE_MODULE ? 0 : 1;
        goto cleanup;
    }

    if (wasi_init(&engine, NULL) != WASI_OK) {
        fprintf(stderr, "wasi_init failed: %s\n", engine.error_msg);
        goto cleanup;
    }

    component = wasi_load(&engine, bytes, size);
    if (!component) {
        fprintf(stderr, "wasi_load failed: %s\n", engine.error_msg);
        wasi_destroy(&engine);
        goto cleanup;
    }

    wasi_dump_component(component, dump, sizeof(dump));
    printf("%s", dump);

    if (strcmp(mode, "load-ok") == 0) {
        exit_code = 0;
    } else if (strcmp(mode, "load-status") == 0) {
        const char* expected;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = argv[3];
        exit_code = strcmp(component_runner_status_token(wasi_component_status(component)), expected) == 0 ? 0 : 1;
    } else if (strcmp(mode, "alias-count") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected alias count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_alias_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "alias-kind") == 0) {
        unsigned long index;
        char* end = NULL;
        const char* actual;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid alias index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_alias_kind_string(wasi_component_alias_kind(component, (uint32_t)index));
        exit_code = strcmp(actual, argv[4]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "alias-name") == 0) {
        unsigned long index;
        char* end = NULL;
        const char* actual;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid alias index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_alias_name(component, (uint32_t)index);
        exit_code = actual && strcmp(actual, argv[4]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "core-instance-count") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected core instance count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_core_instance_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "core-instance-kind") == 0) {
        unsigned long index;
        char* end = NULL;
        const char* actual;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid core instance index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_core_instance_kind_string(wasi_component_core_instance_kind(component, (uint32_t)index));
        exit_code = strcmp(actual, argv[4]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "core-instance-export-name") == 0) {
        unsigned long instance_index;
        unsigned long export_index;
        char* end = NULL;
        const char* actual;
        if (argc != 6) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        instance_index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid core instance index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        export_index = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid core instance export index: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_core_instance_export_name(component,
                                                          (uint32_t)instance_index,
                                                          (uint32_t)export_index);
        exit_code = actual && strcmp(actual, argv[5]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "core-instance-module") == 0) {
        unsigned long index;
        unsigned long expected;
        char* end = NULL;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid core instance index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected module index: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_core_instance_module_index(component, (uint32_t)index) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "core-instance-arg-name") == 0) {
        unsigned long instance_index;
        unsigned long arg_index;
        char* end = NULL;
        const char* actual;
        if (argc != 6) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        instance_index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid core instance index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        arg_index = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid core instance arg index: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_core_instance_arg_name(component,
                                                       (uint32_t)instance_index,
                                                       (uint32_t)arg_index);
        exit_code = actual && strcmp(actual, argv[5]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "component-instance-count") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected component instance count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_instance_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "component-instance-kind") == 0) {
        unsigned long index;
        char* end = NULL;
        const char* actual;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid component instance index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_instance_kind_string(wasi_component_instance_kind(component, (uint32_t)index));
        exit_code = strcmp(actual, argv[4]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "component-instance-export-name") == 0) {
        unsigned long instance_index;
        unsigned long export_index;
        char* end = NULL;
        const char* actual;
        if (argc != 6) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        instance_index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid component instance index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        export_index = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid component instance export index: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_instance_export_name(component,
                                                     (uint32_t)instance_index,
                                                     (uint32_t)export_index);
        exit_code = actual && strcmp(actual, argv[5]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "component-instance-component") == 0) {
        unsigned long index;
        unsigned long expected;
        char* end = NULL;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid component instance index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected component index: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_instance_component_index(component, (uint32_t)index) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "component-instance-arg-name") == 0) {
        unsigned long instance_index;
        unsigned long arg_index;
        char* end = NULL;
        const char* actual;
        if (argc != 6) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        instance_index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid component instance index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        arg_index = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid component instance arg index: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_instance_arg_name(component,
                                                  (uint32_t)instance_index,
                                                  (uint32_t)arg_index);
        exit_code = actual && strcmp(actual, argv[5]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "nested-component-count") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected nested component count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_nested_component_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "nested-component-section-count") == 0) {
        unsigned long index;
        unsigned long expected;
        char* end = NULL;
        const wasi_component_t* nested;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid nested component index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected nested section count: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        nested = wasi_component_nested_component_at(component, (uint32_t)index);
        exit_code = nested && wasi_component_section_count(nested) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "nested-component-import-name") == 0) {
        unsigned long component_index;
        unsigned long import_index;
        char* end = NULL;
        const wasi_component_t* nested;
        const char* actual;
        if (argc != 6) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        component_index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid nested component index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        import_index = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid nested import index: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        nested = wasi_component_nested_component_at(component, (uint32_t)component_index);
        actual = nested ? wasi_component_import_name(nested, (uint32_t)import_index) : NULL;
        exit_code = actual && strcmp(actual, argv[5]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "nested-component-alias-kind") == 0) {
        unsigned long component_index;
        unsigned long alias_index;
        char* end = NULL;
        const wasi_component_t* nested;
        const char* actual;
        if (argc != 6) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        component_index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid nested component index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        alias_index = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid nested alias index: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        nested = wasi_component_nested_component_at(component, (uint32_t)component_index);
        actual = nested ? wasi_component_alias_kind_string(wasi_component_alias_kind(nested, (uint32_t)alias_index)) : NULL;
        exit_code = actual && strcmp(actual, argv[5]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "nested-component-instance-kind") == 0) {
        unsigned long component_index;
        unsigned long instance_index;
        char* end = NULL;
        const wasi_component_t* nested;
        const char* actual;
        if (argc != 6) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        component_index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid nested component index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        instance_index = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid nested instance index: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        nested = wasi_component_nested_component_at(component, (uint32_t)component_index);
        actual = nested ? wasi_component_instance_kind_string(wasi_component_instance_kind(nested, (uint32_t)instance_index)) : NULL;
        exit_code = actual && strcmp(actual, argv[5]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "start-present") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0' || expected > 1u) {
            fprintf(stderr, "invalid expected start presence: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_has_start(component) == (int)expected ? 0 : 1;
    } else if (strcmp(mode, "start-func") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected start function: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_start_func_index(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "start-args") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected start arg count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_start_arg_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "start-results") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected start result count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_start_result_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "canon-count") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected canon count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_canon_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "canon-kind") == 0) {
        unsigned long index;
        char* end = NULL;
        const char* actual;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid canon index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_canon_kind_string(wasi_component_canon_kind(component, (uint32_t)index));
        exit_code = strcmp(actual, argv[4]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "canon-option-count") == 0) {
        unsigned long index;
        unsigned long expected;
        char* end = NULL;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid canon index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected option count: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_canon_option_count(component, (uint32_t)index) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "type-param-count") == 0) {
        unsigned long type_index;
        unsigned long expected;
        char* end = NULL;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        type_index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid type index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected param count: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_func_type_param_count(component, (uint32_t)type_index) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "type-result-count") == 0) {
        unsigned long type_index;
        unsigned long expected;
        char* end = NULL;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        type_index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid type index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected result count: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_func_type_result_count(component, (uint32_t)type_index) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "import-count") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected import count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_import_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "export-count") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected export count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_export_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "import-name") == 0) {
        unsigned long index;
        char* end = NULL;
        const char* actual;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid import index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_import_name(component, (uint32_t)index);
        exit_code = actual && strcmp(actual, argv[4]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "export-name") == 0) {
        unsigned long index;
        char* end = NULL;
        const char* actual;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid export index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        actual = wasi_component_export_name(component, (uint32_t)index);
        exit_code = actual && strcmp(actual, argv[4]) == 0 ? 0 : 1;
    } else if (strcmp(mode, "export-type") == 0) {
        unsigned long index;
        unsigned long expected;
        uint32_t actual_type = 0;
        char* end = NULL;
        if (argc != 5) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        index = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid export index: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[4], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected export type: %s\n", argv[4]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_export_func_type_index(component, (uint32_t)index, &actual_type) && actual_type == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "core-module-count") == 0) {
        unsigned long expected;
        char* end = NULL;
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        expected = strtoul(argv[3], &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid expected core module count: %s\n", argv[3]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = wasi_component_core_module_count(component) == (uint32_t)expected ? 0 : 1;
    } else if (strcmp(mode, "dump-contains") == 0) {
        if (argc != 4) {
            component_runner_usage(argv[0]);
            wasi_free_component(component);
            wasi_destroy(&engine);
            goto cleanup;
        }
        exit_code = strstr(dump, argv[3]) != NULL ? 0 : 1;
    } else {
        component_runner_usage(argv[0]);
        exit_code = 2;
    }

    wasi_free_component(component);
    wasi_destroy(&engine);

cleanup:
    free(bytes);
    return exit_code;
}
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
    fprintf(stderr, "    load-status <unparsed|parsed-sections>\n");
    fprintf(stderr, "    import-count <expected>\n");
    fprintf(stderr, "    export-count <expected>\n");
    fprintf(stderr, "    import-name <index> <expected>\n");
    fprintf(stderr, "    export-name <index> <expected>\n");
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
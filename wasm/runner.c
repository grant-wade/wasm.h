#define WASM_IMPL
#include "../wasm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char* argv0) {
    fprintf(stderr, "usage: %s <module.wasm> <mode> [args...]\n", argv0);
    fprintf(stderr, "  modes:\n");
    fprintf(stderr, "    load-ok\n");
    fprintf(stderr, "    load-fail\n");
    fprintf(stderr, "    call-void <export_name>\n");
    fprintf(stderr, "    call-i32 <export_name> <expected>\n");
    fprintf(stderr, "    call-i64 <export_name> <expected>\n");
    fprintf(stderr, "    call-fail <export_name>\n");
}

static unsigned char* read_file_bytes(const char* path, size_t* out_size) {
    FILE* file;
    long file_size;
    unsigned char* bytes;

    *out_size = 0;
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

    bytes = (unsigned char*)malloc((size_t)file_size);
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
    *out_size = (size_t)file_size;
    return bytes;
}

static void print_exports(const wasm_module_t* mod) {
    uint32_t index;

    printf("exports (%u):\n", (unsigned)mod->num_exports);
    for (index = 0; index < mod->num_exports; index++) {
        const wasm_export_t* exp = &mod->exports[index];
        const char* kind = "unknown";

        switch (exp->kind) {
            case WASM_EXPORT_FUNC:
                kind = "func";
                break;
            case WASM_EXPORT_TABLE:
                kind = "table";
                break;
            case WASM_EXPORT_MEM:
                kind = "memory";
                break;
            case WASM_EXPORT_GLOBAL:
                kind = "global";
                break;
        }

        printf("  %-24s kind=%s index=%u\n", exp->name, kind, (unsigned)exp->index);
    }
}

int main(int argc, char** argv) {
    const char* wasm_path;
    const char* mode;
    const char* export_name = NULL;
    unsigned char* bytes = NULL;
    size_t size = 0;
    wasm_runtime_t runtime;
    wasm_module_t* module = NULL;
    int exit_code = 1;

    if (argc < 3) {
        print_usage(argv[0]);
        return 2;
    }

    wasm_path = argv[1];
    mode = argv[2];

    bytes = read_file_bytes(wasm_path, &size);
    if (!bytes) {
        fprintf(stderr, "failed to read %s\n", wasm_path);
        return 1;
    }

    if (wasm_init(&runtime) != WASM_OK) {
        fprintf(stderr, "failed to initialize runtime\n");
        free(bytes);
        return 1;
    }

    module = wasm_load(&runtime, bytes, size);
    if (!module) {
        if (strcmp(mode, "load-fail") == 0) {
            printf("expected load failure: %s\n",
                   runtime.error_msg[0] ? runtime.error_msg : wasm_error_string(runtime.last_error));
            exit_code = 0;
            goto cleanup;
        }
        fprintf(stderr, "load failed: %s\n", runtime.error_msg[0] ? runtime.error_msg : wasm_error_string(runtime.last_error));
        goto cleanup;
    }

    printf("loaded: %s\n", wasm_path);
    print_exports(module);
    printf("memories: %u\n", (unsigned)wasm_memory_count(module));
    {
        uint32_t memory_index;

        for (memory_index = 0; memory_index < wasm_memory_count(module); memory_index++) {
            printf("memory[%u]: %u bytes\n",
                   (unsigned)memory_index,
                   (unsigned)wasm_memory_size_at(module, memory_index));
        }
    }

    if (strcmp(mode, "load-ok") == 0) {
        exit_code = 0;
        goto cleanup;
    }

    if (strcmp(mode, "load-fail") == 0) {
        fprintf(stderr, "expected load failure but module loaded\n");
        goto cleanup;
    }

    if (strcmp(mode, "call-void") == 0) {
        if (argc != 4) {
            print_usage(argv[0]);
            exit_code = 2;
            goto cleanup;
        }
        export_name = argv[3];
        {
        wasm_error_t err = wasm_call(module, export_name, NULL, 0, NULL, 0);
        if (err != WASM_OK) {
            fprintf(stderr, "call failed: %s\n", runtime.error_msg[0] ? runtime.error_msg : wasm_error_string(err));
            goto cleanup;
        }
        printf("call ok: %s() -> void\n", export_name);
        exit_code = 0;
        goto cleanup;
        }
    }

    if (strcmp(mode, "call-fail") == 0) {
        if (argc != 4) {
            print_usage(argv[0]);
            exit_code = 2;
            goto cleanup;
        }
        export_name = argv[3];
        {
        wasm_value_t result;
        wasm_error_t err = wasm_call(module, export_name, NULL, 0, &result, 1);
        if (err == WASM_OK) {
            fprintf(stderr, "expected call failure but call succeeded with %d\n", result.of.i32);
            goto cleanup;
        }
        printf("expected call failure: %s\n",
               runtime.error_msg[0] ? runtime.error_msg : wasm_error_string(err));
        exit_code = 0;
        goto cleanup;
        }
    }

    if (strcmp(mode, "call-i32") == 0) {
        int expected_i32;

        if (argc != 5) {
            print_usage(argv[0]);
            exit_code = 2;
            goto cleanup;
        }
        export_name = argv[3];
        expected_i32 = atoi(argv[4]);

        {
        wasm_value_t result;
        wasm_error_t err = wasm_call(module, export_name, NULL, 0, &result, 1);
        if (err != WASM_OK) {
            fprintf(stderr, "call failed: %s\n", runtime.error_msg[0] ? runtime.error_msg : wasm_error_string(err));
            goto cleanup;
        }
        printf("call ok: %s() -> %d\n", export_name, result.of.i32);
        if (result.of.i32 != expected_i32) {
            fprintf(stderr, "expected %d but got %d\n", expected_i32, result.of.i32);
            goto cleanup;
        }
        exit_code = 0;
        goto cleanup;
        }
    }

    if (strcmp(mode, "call-i64") == 0) {
        long long expected_i64;
        char* endptr = NULL;

        if (argc != 5) {
            print_usage(argv[0]);
            exit_code = 2;
            goto cleanup;
        }
        export_name = argv[3];
        expected_i64 = strtoll(argv[4], &endptr, 10);
        if (!endptr || *endptr != '\0') {
            fprintf(stderr, "invalid i64 expectation: %s\n", argv[4]);
            exit_code = 2;
            goto cleanup;
        }

        {
        wasm_value_t result;
        wasm_error_t err = wasm_call(module, export_name, NULL, 0, &result, 1);
        if (err != WASM_OK) {
            fprintf(stderr, "call failed: %s\n", runtime.error_msg[0] ? runtime.error_msg : wasm_error_string(err));
            goto cleanup;
        }
        printf("call ok: %s() -> %lld\n", export_name, (long long)result.of.i64);
        if (result.of.i64 != expected_i64) {
            fprintf(stderr, "expected %lld but got %lld\n",
                    expected_i64, (long long)result.of.i64);
            goto cleanup;
        }
        exit_code = 0;
        goto cleanup;
        }
    }

    fprintf(stderr, "unknown mode: %s\n", mode);

cleanup:
    wasm_free_module(module);
    wasm_destroy(&runtime);
    free(bytes);
    return exit_code;
}

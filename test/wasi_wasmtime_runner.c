#define WASI_IMPL

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../wasi.h"

#if defined(_WIN32)
#include <io.h>
#include <process.h>
#include <windows.h>
#define WASI_COMPARE_GETPID _getpid
#define WASI_COMPARE_PCLOSE _pclose
#define WASI_COMPARE_POPEN _popen
#define WASI_COMPARE_UNLINK _unlink
#else
#include <sys/wait.h>
#include <unistd.h>
#define WASI_COMPARE_GETPID getpid
#define WASI_COMPARE_PCLOSE pclose
#define WASI_COMPARE_POPEN popen
#define WASI_COMPARE_UNLINK unlink
#endif

typedef union wasi_compare_scalar_t {
    int boolean;
    int8_t s8;
    uint8_t u8;
    int16_t s16;
    uint16_t u16;
    int32_t s32;
    uint32_t u32;
    int64_t s64;
    uint64_t u64;
    float f32;
    double f64;
    uint32_t char32;
} wasi_compare_scalar_t;

typedef struct wasi_compare_case_t {
    const char* name;
    const char* wit_type;
    const char* core_type;
    const char* invoke_expr;
    wasi_value_kind_t value_kind;
    const char* string_input;
    size_t string_len;
    const char* wasm_tools_encoding;
    wasi_string_encoding_t string_encoding;
    wasi_compare_scalar_t input;
} wasi_compare_case_t;

static void wasi_compare_usage(const char* argv0) {
    fprintf(stderr, "usage: %s <wasmtime> <wasm-tools> <--list|--all|case-name>\n", argv0);
}

static int wasi_compare_quote_arg(const char* input, char* output, size_t output_cap) {
    size_t i;
    size_t out_len = 0u;

    if (!input || !output || output_cap < 3u) return 0;
    output[out_len++] = '"';
    for (i = 0; input[i] != '\0'; i++) {
        if (input[i] == '"' || input[i] == '\\' || input[i] == '$' || input[i] == '`') {
            if (out_len + 3u >= output_cap) return 0;
            output[out_len++] = '\\';
        } else if (out_len + 2u >= output_cap) {
            return 0;
        }
        output[out_len++] = input[i];
    }
    output[out_len++] = '"';
    output[out_len] = '\0';
    return 1;
}

static void wasi_compare_trim_trailing_whitespace(char* text) {
    size_t len;

    if (!text) return;
    len = strlen(text);
    while (len > 0u && isspace((unsigned char)text[len - 1u])) {
        text[len - 1u] = '\0';
        len--;
    }
}

static int wasi_compare_process_succeeded(int status) {
#if defined(_WIN32)
    return status == 0;
#else
    return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

static int wasi_compare_run_command(const char* command, char* output, size_t output_cap) {
    FILE* pipe;
    size_t offset = 0u;
    int status;

    if (!command || !output || output_cap == 0u) return 0;

    pipe = WASI_COMPARE_POPEN(command, "r");
    if (!pipe) return 0;

    output[0] = '\0';
    while (offset + 1u < output_cap && fgets(output + offset, (int)(output_cap - offset), pipe)) {
        offset = strlen(output);
    }

    status = WASI_COMPARE_PCLOSE(pipe);
    wasi_compare_trim_trailing_whitespace(output);
    return wasi_compare_process_succeeded(status);
}

static int wasi_compare_make_temp_base(const char* case_name, char* base, size_t base_cap) {
#if defined(_WIN32)
    char temp_dir[MAX_PATH];
    DWORD temp_len;

    temp_len = GetTempPathA((DWORD)sizeof(temp_dir), temp_dir);
    if (temp_len == 0 || temp_len >= sizeof(temp_dir)) return 0;
    return snprintf(base,
                    base_cap,
                    "%swl_wasi_compare_%lu_%lu_%s",
                    temp_dir,
                    (unsigned long)time(NULL),
                    (unsigned long)WASI_COMPARE_GETPID(),
                    case_name) < (int)base_cap;
#else
    const char* temp_dir = getenv("TMPDIR");

    if (!temp_dir || !temp_dir[0]) temp_dir = "/tmp";
    return snprintf(base,
                    base_cap,
                    "%s/wl_wasi_compare_%lu_%lu_%s",
                    temp_dir,
                    (unsigned long)time(NULL),
                    (unsigned long)WASI_COMPARE_GETPID(),
                    case_name) < (int)base_cap;
#endif
}

static int wasi_compare_write_text_file(const char* path, const char* text) {
    FILE* file;
    size_t len;

    if (!path || !text) return 0;
    len = strlen(text);
    file = fopen(path, "wb");
    if (!file) return 0;
    if (len > 0u && fwrite(text, 1u, len, file) != len) {
        fclose(file);
        return 0;
    }
    if (fclose(file) != 0) return 0;
    return 1;
}

static unsigned char* wasi_compare_read_file_bytes(const char* path, size_t* out_size) {
    FILE* file;
    long file_size;
    unsigned char* bytes;

    if (out_size) *out_size = 0u;
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
    if (file_size > 0 && fread(bytes, 1u, (size_t)file_size, file) != (size_t)file_size) {
        free(bytes);
        fclose(file);
        return NULL;
    }

    fclose(file);
    if (out_size) *out_size = (size_t)file_size;
    return bytes;
}

static void wasi_compare_set_arg(wasi_value_t* arg, const wasi_compare_case_t* test_case) {
    memset(arg, 0, sizeof(*arg));
    arg->kind = test_case->value_kind;

    switch (test_case->value_kind) {
        case WASI_VALUE_KIND_BOOL:
            arg->of.boolean = (uint8_t)test_case->input.boolean;
            break;
        case WASI_VALUE_KIND_S8:
            arg->of.s8 = test_case->input.s8;
            break;
        case WASI_VALUE_KIND_U8:
            arg->of.u8 = test_case->input.u8;
            break;
        case WASI_VALUE_KIND_S16:
            arg->of.s16 = test_case->input.s16;
            break;
        case WASI_VALUE_KIND_U16:
            arg->of.u16 = test_case->input.u16;
            break;
        case WASI_VALUE_KIND_S32:
            arg->of.s32 = test_case->input.s32;
            break;
        case WASI_VALUE_KIND_U32:
            arg->of.u32 = test_case->input.u32;
            break;
        case WASI_VALUE_KIND_S64:
            arg->of.s64 = test_case->input.s64;
            break;
        case WASI_VALUE_KIND_U64:
            arg->of.u64 = test_case->input.u64;
            break;
        case WASI_VALUE_KIND_F32:
            arg->of.f32 = test_case->input.f32;
            break;
        case WASI_VALUE_KIND_F64:
            arg->of.f64 = test_case->input.f64;
            break;
        case WASI_VALUE_KIND_CHAR:
            arg->of.char32 = test_case->input.char32;
            break;
        case WASI_VALUE_KIND_STRING:
            arg->of.string.data = (char*)test_case->string_input;
            arg->of.string.len = test_case->string_len;
            arg->of.string.owned = 0;
            break;
        default:
            break;
    }
}

static int wasi_compare_serialize_value(const wasi_value_t* value, char* output, size_t output_cap) {
    size_t in_len;
    size_t out_len = 0u;
    size_t i;

    switch (value->kind) {
        case WASI_VALUE_KIND_BOOL:
            return snprintf(output, output_cap, "%s", value->of.boolean ? "true" : "false") < (int)output_cap;
        case WASI_VALUE_KIND_S8:
            return snprintf(output, output_cap, "%" PRId8, value->of.s8) < (int)output_cap;
        case WASI_VALUE_KIND_U8:
            return snprintf(output, output_cap, "%" PRIu8, value->of.u8) < (int)output_cap;
        case WASI_VALUE_KIND_S16:
            return snprintf(output, output_cap, "%" PRId16, value->of.s16) < (int)output_cap;
        case WASI_VALUE_KIND_U16:
            return snprintf(output, output_cap, "%" PRIu16, value->of.u16) < (int)output_cap;
        case WASI_VALUE_KIND_S32:
            return snprintf(output, output_cap, "%" PRId32, value->of.s32) < (int)output_cap;
        case WASI_VALUE_KIND_U32:
            return snprintf(output, output_cap, "%" PRIu32, value->of.u32) < (int)output_cap;
        case WASI_VALUE_KIND_S64:
            return snprintf(output, output_cap, "%" PRId64, value->of.s64) < (int)output_cap;
        case WASI_VALUE_KIND_U64:
            return snprintf(output, output_cap, "%" PRIu64, value->of.u64) < (int)output_cap;
        case WASI_VALUE_KIND_F32:
            return snprintf(output, output_cap, "%.9g", (double)value->of.f32) < (int)output_cap;
        case WASI_VALUE_KIND_F64:
            return snprintf(output, output_cap, "%.17g", value->of.f64) < (int)output_cap;
        case WASI_VALUE_KIND_CHAR:
            return value->of.char32 >= 0x20u && value->of.char32 <= 0x7Eu && value->of.char32 != '\'' &&
                           snprintf(output, output_cap, "'%c'", (char)value->of.char32) < (int)output_cap;
        case WASI_VALUE_KIND_STRING:
            if (!output || output_cap < 3u) return 0;
            output[out_len++] = '"';
            in_len = value->of.string.len;
            for (i = 0; i < in_len; i++) {
                unsigned char byte = (unsigned char)value->of.string.data[i];

                if (byte == '"' || byte == '\\') {
                    if (out_len + 2u >= output_cap) return 0;
                    output[out_len++] = '\\';
                    output[out_len++] = (char)byte;
                } else if (byte == '\n' || byte == '\r' || byte == '\t') {
                    char escaped = byte == '\n' ? 'n' : (byte == '\r' ? 'r' : 't');
                    if (out_len + 2u >= output_cap) return 0;
                    output[out_len++] = '\\';
                    output[out_len++] = escaped;
                } else {
                    if (out_len + 1u >= output_cap) return 0;
                    output[out_len++] = (char)byte;
                }
            }
            if (out_len + 2u > output_cap) return 0;
            output[out_len++] = '"';
            output[out_len] = '\0';
            return 1;
        default:
            return 0;
    }
}

static int wasi_compare_build_component(const char* wasm_tools_path,
                                        const wasi_compare_case_t* test_case,
                                        char* component_path,
                                        size_t component_path_cap) {
    char base[1024];
    char wit_path[1064];
    char wat_path[1064];
    char embedded_path[1064];
    char tools_arg[1024];
    char wit_arg[1200];
    char wat_arg[1200];
    char embedded_arg[1200];
    char component_arg[1200];
    char encoding_arg[128];
    char command[8192];
    char output[4096];
    char wit_text[256];
    char wat_text[2048];
    int ok = 0;

    if (!wasi_compare_make_temp_base(test_case->name, base, sizeof(base))) {
        fprintf(stderr, "%s: failed to create temp file base\n", test_case->name);
        return 0;
    }
    if (snprintf(wit_path, sizeof(wit_path), "%s.wit", base) >= (int)sizeof(wit_path) ||
        snprintf(wat_path, sizeof(wat_path), "%s.wat", base) >= (int)sizeof(wat_path) ||
        snprintf(embedded_path, sizeof(embedded_path), "%s.core.wasm", base) >= (int)sizeof(embedded_path) ||
        snprintf(component_path, component_path_cap, "%s.component.wasm", base) >= (int)component_path_cap) {
        fprintf(stderr, "%s: temp path too long\n", test_case->name);
        return 0;
    }

    if (snprintf(wit_text,
                 sizeof(wit_text),
                 "package local:compare;\nworld compare {\n  export echo: func(x: %s) -> %s;\n}\n",
                 test_case->wit_type,
                 test_case->wit_type) >= (int)sizeof(wit_text)) {
        fprintf(stderr, "%s: failed to build WIT text\n", test_case->name);
        return 0;
    }

    if (test_case->value_kind == WASI_VALUE_KIND_STRING) {
        if (snprintf(wat_text,
                     sizeof(wat_text),
                     "(module\n"
                     "  (memory (export \"cm32p2_memory\") 1)\n"
                     "  (global $heap (mut i32) (i32.const 16))\n"
                     "  (func $realloc_impl (param $old_ptr i32) (param $old_size i32) (param $align i32) (param $new_size i32) (result i32)\n"
                     "    (local $ptr i32)\n"
                     "    global.get $heap\n"
                     "    local.get $align\n"
                     "    i32.const 1\n"
                     "    i32.sub\n"
                     "    i32.add\n"
                     "    local.get $align\n"
                     "    i32.const 1\n"
                     "    i32.sub\n"
                     "    i32.const -1\n"
                     "    i32.xor\n"
                     "    i32.and\n"
                     "    local.tee $ptr\n"
                     "    local.get $new_size\n"
                     "    i32.add\n"
                     "    global.set $heap\n"
                     "    local.get $ptr)\n"
                     "  (func (export \"cm32p2||echo\") (param $ptr i32) (param $len i32) (result i32)\n"
                     "    (local $dst i32)\n"
                     "    (local $ret i32)\n"
                     "    i32.const 0\n"
                     "    i32.const 0\n"
                     "    i32.const 1\n"
                     "    local.get $len\n"
                     "    call $realloc_impl\n"
                     "    local.set $dst\n"
                     "    local.get $dst\n"
                     "    local.get $ptr\n"
                     "    local.get $len\n"
                     "    memory.copy\n"
                     "    i32.const 0\n"
                     "    i32.const 0\n"
                     "    i32.const 4\n"
                     "    i32.const 8\n"
                     "    call $realloc_impl\n"
                     "    local.set $ret\n"
                     "    local.get $ret\n"
                     "    local.get $dst\n"
                     "    i32.store\n"
                     "    local.get $ret\n"
                     "    i32.const 4\n"
                     "    i32.add\n"
                     "    local.get $len\n"
                     "    i32.store\n"
                     "    local.get $ret)\n"
                     "  (func (export \"cm32p2||echo_post\") (param i32))\n"
                     "  (func (export \"cm32p2_realloc\") (param i32 i32 i32 i32) (result i32)\n"
                     "    local.get 0\n"
                     "    local.get 1\n"
                     "    local.get 2\n"
                     "    local.get 3\n"
                     "    call $realloc_impl)\n"
                     "  (func (export \"cm32p2_initialize\"))\n"
                     ")\n") >= (int)sizeof(wat_text)) {
            fprintf(stderr, "%s: failed to build string WAT text\n", test_case->name);
            return 0;
        }
    } else if (snprintf(wat_text,
                        sizeof(wat_text),
                        "(module\n"
                        "  (func (export \"cm32p2||echo\") (param %s) (result %s)\n"
                        "    local.get 0)\n"
                        "  (func (export \"cm32p2||echo_post\") (param %s))\n"
                        "  (memory (export \"cm32p2_memory\") 0)\n"
                        "  (func (export \"cm32p2_realloc\") (param i32 i32 i32 i32) (result i32)\n"
                        "    unreachable)\n"
                        "  (func (export \"cm32p2_initialize\"))\n"
                        ")\n",
                        test_case->core_type,
                        test_case->core_type,
                        test_case->core_type) >= (int)sizeof(wat_text)) {
        fprintf(stderr, "%s: failed to build WAT text\n", test_case->name);
        return 0;
    }

    if (!wasi_compare_write_text_file(wit_path, wit_text)) {
        fprintf(stderr, "%s: failed to write %s: %s\n", test_case->name, wit_path, strerror(errno));
        goto cleanup;
    }
    if (!wasi_compare_write_text_file(wat_path, wat_text)) {
        fprintf(stderr, "%s: failed to write %s: %s\n", test_case->name, wat_path, strerror(errno));
        goto cleanup;
    }

    if (!wasi_compare_quote_arg(wasm_tools_path, tools_arg, sizeof(tools_arg)) ||
        !wasi_compare_quote_arg(wit_path, wit_arg, sizeof(wit_arg)) ||
        !wasi_compare_quote_arg(wat_path, wat_arg, sizeof(wat_arg)) ||
        !wasi_compare_quote_arg(embedded_path, embedded_arg, sizeof(embedded_arg)) ||
        !wasi_compare_quote_arg(component_path, component_arg, sizeof(component_arg))) {
        fprintf(stderr, "%s: failed to quote tool arguments\n", test_case->name);
        goto cleanup;
    }

    encoding_arg[0] = '\0';
    if (test_case->wasm_tools_encoding && test_case->wasm_tools_encoding[0]) {
        if (snprintf(encoding_arg, sizeof(encoding_arg), " --encoding %s", test_case->wasm_tools_encoding) >= (int)sizeof(encoding_arg)) {
            fprintf(stderr, "%s: encoding option too long\n", test_case->name);
            goto cleanup;
        }
    }

    if (snprintf(command,
                 sizeof(command),
                 "%s component embed%s --world compare %s %s -o %s 2>&1",
                 tools_arg,
                 encoding_arg,
                 wit_arg,
                 wat_arg,
                 embedded_arg) >= (int)sizeof(command)) {
        fprintf(stderr, "%s: embed command too long\n", test_case->name);
        goto cleanup;
    }
    if (!wasi_compare_run_command(command, output, sizeof(output))) {
        fprintf(stderr, "%s: wasm-tools component embed failed\n", test_case->name);
        if (output[0]) fprintf(stderr, "%s\n", output);
        goto cleanup;
    }

    if (snprintf(command,
                 sizeof(command),
                 "%s component new %s -o %s 2>&1",
                 tools_arg,
                 embedded_arg,
                 component_arg) >= (int)sizeof(command)) {
        fprintf(stderr, "%s: component new command too long\n", test_case->name);
        goto cleanup;
    }
    if (!wasi_compare_run_command(command, output, sizeof(output))) {
        fprintf(stderr, "%s: wasm-tools component new failed\n", test_case->name);
        if (output[0]) fprintf(stderr, "%s\n", output);
        goto cleanup;
    }

    ok = 1;

cleanup:
    WASI_COMPARE_UNLINK(wit_path);
    WASI_COMPARE_UNLINK(wat_path);
    WASI_COMPARE_UNLINK(embedded_path);
    if (!ok) WASI_COMPARE_UNLINK(component_path);
    return ok;
}

static int wasi_compare_run_case(const char* wasmtime_path,
                                 const char* wasm_tools_path,
                                 const wasi_compare_case_t* test_case) {
    char component_path[1064];
    char wasmtime_arg[1024];
    char component_arg[1200];
    char invoke_arg[1024];
    char command[8192];
    char wasmtime_output[4096];
    char runtime_output[128];
    unsigned char* component_bytes = NULL;
    size_t component_size = 0u;
    wasi_engine_t engine;
    wasi_component_t* component = NULL;
    wasm_module_t* core_module = NULL;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_canon_options_t options;
    uint32_t func_type_index = 0u;
    wasi_error_t err;
    int ok = 0;

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(component_path, 0, sizeof(component_path));
    memset(&options, 0, sizeof(options));

    if (!wasi_compare_build_component(wasm_tools_path, test_case, component_path, sizeof(component_path))) {
        return 1;
    }

    component_bytes = wasi_compare_read_file_bytes(component_path, &component_size);
    if (!component_bytes) {
        fprintf(stderr, "%s: failed to read %s: %s\n", test_case->name, component_path, strerror(errno));
        goto cleanup;
    }

    err = wasi_init(&engine, NULL);
    if (err != WASI_OK) {
        fprintf(stderr, "%s: wasi_init failed: %s\n", test_case->name, engine.error_msg);
        goto cleanup;
    }

    component = wasi_load(&engine, component_bytes, component_size);
    if (!component) {
        fprintf(stderr, "%s: wasi_load failed: %s\n", test_case->name, engine.error_msg);
        goto cleanup_engine;
    }

    core_module = wasi_component_core_module_at(component, 0u);
    if (!core_module) {
        fprintf(stderr, "%s: missing primary core module\n", test_case->name);
        goto cleanup_component;
    }
    if (!wasi_component_export_func_type_index(component, 0u, &func_type_index)) {
        fprintf(stderr, "%s: missing exported function type index\n", test_case->name);
        goto cleanup_component;
    }

    wasi_compare_set_arg(&arg, test_case);
    err = wasi_canon_options_default(&options);
    if (err != WASI_OK) {
        fprintf(stderr, "%s: wasi_canon_options_default failed: %s\n", test_case->name, engine.error_msg);
        goto cleanup_component;
    }
    if (test_case->value_kind == WASI_VALUE_KIND_STRING) {
        options.post_return_name = "cm32p2||echo_post";
        options.cabi_realloc_name = "cm32p2_realloc";
        options.string_encoding = test_case->string_encoding;
    }
    err = wasi_canon_call(component,
                          func_type_index,
                          core_module,
                          "cm32p2||echo",
                          &options,
                          &arg,
                          1u,
                          &result,
                          1u);
    if (err != WASI_OK) {
        fprintf(stderr, "%s: wasi_canon_call failed: %s\n", test_case->name, engine.error_msg);
        goto cleanup_component;
    }
    if (!wasi_compare_serialize_value(&result, runtime_output, sizeof(runtime_output))) {
        fprintf(stderr, "%s: failed to serialize runtime result kind 0x%02X\n", test_case->name, (unsigned)result.kind);
        goto cleanup_component;
    }

    if (!wasi_compare_quote_arg(wasmtime_path, wasmtime_arg, sizeof(wasmtime_arg)) ||
        !wasi_compare_quote_arg(component_path, component_arg, sizeof(component_arg)) ||
        !wasi_compare_quote_arg(test_case->invoke_expr, invoke_arg, sizeof(invoke_arg))) {
        fprintf(stderr, "%s: failed to quote wasmtime arguments\n", test_case->name);
        goto cleanup_component;
    }
    if (snprintf(command,
                 sizeof(command),
                 "%s run --invoke %s %s 2>&1",
                 wasmtime_arg,
                 invoke_arg,
                 component_arg) >= (int)sizeof(command)) {
        fprintf(stderr, "%s: wasmtime command too long\n", test_case->name);
        goto cleanup_component;
    }
    if (!wasi_compare_run_command(command, wasmtime_output, sizeof(wasmtime_output))) {
        fprintf(stderr, "%s: wasmtime failed\n", test_case->name);
        if (wasmtime_output[0]) fprintf(stderr, "%s\n", wasmtime_output);
        goto cleanup_component;
    }

    if (strcmp(runtime_output, wasmtime_output) != 0) {
        fprintf(stderr, "%s: result mismatch\n", test_case->name);
        fprintf(stderr, "  wasi.h:    %s\n", runtime_output);
        fprintf(stderr, "  wasmtime:  %s\n", wasmtime_output);
        goto cleanup_component;
    }

    ok = 1;

cleanup_component:
    wasi_value_destroy(&result);
    if (component) wasi_free_component(component);
cleanup_engine:
    wasi_destroy(&engine);
cleanup:
    free(component_bytes);
    if (component_path[0]) WASI_COMPARE_UNLINK(component_path);
    return ok ? 0 : 1;
}

static const wasi_compare_case_t wasi_compare_cases[] = {
    { "bool", "bool", "i32", "echo(true)", WASI_VALUE_KIND_BOOL, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .boolean = 1 } },
    { "s8", "s8", "i32", "echo(-8)", WASI_VALUE_KIND_S8, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .s8 = -8 } },
    { "u8", "u8", "i32", "echo(255)", WASI_VALUE_KIND_U8, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .u8 = 255u } },
    { "s16", "s16", "i32", "echo(-1234)", WASI_VALUE_KIND_S16, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .s16 = -1234 } },
    { "u16", "u16", "i32", "echo(54321)", WASI_VALUE_KIND_U16, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .u16 = 54321u } },
    { "s32", "s32", "i32", "echo(-12345678)", WASI_VALUE_KIND_S32, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .s32 = -12345678 } },
    { "u32", "u32", "i32", "echo(3456789012)", WASI_VALUE_KIND_U32, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .u32 = 3456789012u } },
    { "s64", "s64", "i64", "echo(-1234567890123)", WASI_VALUE_KIND_S64, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .s64 = INT64_C(-1234567890123) } },
    { "u64", "u64", "i64", "echo(12345678901234)", WASI_VALUE_KIND_U64, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .u64 = UINT64_C(12345678901234) } },
    { "f32", "f32", "f32", "echo(1.5)", WASI_VALUE_KIND_F32, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .f32 = 1.5f } },
    { "f64", "f64", "f64", "echo(-2.25)", WASI_VALUE_KIND_F64, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .f64 = -2.25 } },
    { "char", "char", "i32", "echo('A')", WASI_VALUE_KIND_CHAR, NULL, 0u, NULL, WASI_STRING_ENCODING_UTF8, { .char32 = (uint32_t)'A' } },
    { "string-hello", "string", "i32", "echo(\"hello\")", WASI_VALUE_KIND_STRING, "hello", 5u, NULL, WASI_STRING_ENCODING_UTF8, { 0 } },
    { "string-unicode", "string", "i32", "echo(\"🙂\")", WASI_VALUE_KIND_STRING, "🙂", sizeof("🙂") - 1u, NULL, WASI_STRING_ENCODING_UTF8, { 0 } },
    { "string-empty", "string", "i32", "echo(\"\")", WASI_VALUE_KIND_STRING, "", 0u, NULL, WASI_STRING_ENCODING_UTF8, { 0 } },
};

static const wasi_compare_case_t* wasi_compare_find_case(const char* name) {
    size_t i;

    for (i = 0; i < sizeof(wasi_compare_cases) / sizeof(wasi_compare_cases[0]); i++) {
        if (strcmp(wasi_compare_cases[i].name, name) == 0) return &wasi_compare_cases[i];
    }
    return NULL;
}

int main(int argc, char** argv) {
    const char* wasmtime_path;
    const char* wasm_tools_path;
    const char* selector;
    size_t i;
    int exit_code = 0;

    if (argc != 4) {
        wasi_compare_usage(argv[0]);
        return 2;
    }

    wasmtime_path = argv[1];
    wasm_tools_path = argv[2];
    selector = argv[3];

    if (strcmp(selector, "--list") == 0) {
        for (i = 0; i < sizeof(wasi_compare_cases) / sizeof(wasi_compare_cases[0]); i++) {
            puts(wasi_compare_cases[i].name);
        }
        return 0;
    }

    if (strcmp(selector, "--all") == 0) {
        for (i = 0; i < sizeof(wasi_compare_cases) / sizeof(wasi_compare_cases[0]); i++) {
            if (wasi_compare_run_case(wasmtime_path, wasm_tools_path, &wasi_compare_cases[i]) != 0) exit_code = 1;
        }
        return exit_code;
    }

    {
        const wasi_compare_case_t* test_case = wasi_compare_find_case(selector);

        if (!test_case) {
            fprintf(stderr, "unknown case: %s\n", selector);
            wasi_compare_usage(argv[0]);
            return 2;
        }
        return wasi_compare_run_case(wasmtime_path, wasm_tools_path, test_case);
    }
}
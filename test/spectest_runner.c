#define WASM_IMPL
#include "../wasm.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#define SPEC_UNLINK _unlink
#else
#include <sys/wait.h>
#include <unistd.h>
#define SPEC_UNLINK unlink
#endif

typedef enum spec_json_type_t {
    SPEC_JSON_UNDEFINED = 0,
    SPEC_JSON_OBJECT = 1,
    SPEC_JSON_ARRAY = 2,
    SPEC_JSON_STRING = 3,
    SPEC_JSON_PRIMITIVE = 4
} spec_json_type_t;

typedef struct spec_json_token_t {
    spec_json_type_t type;
    int start;
    int end;
    int size;
    int parent;
} spec_json_token_t;

typedef struct spec_json_parser_t {
    unsigned pos;
    unsigned toknext;
    int toksuper;
} spec_json_parser_t;

typedef enum spec_float_expectation_t {
    SPEC_FLOAT_EXACT = 0,
    SPEC_FLOAT_NAN_CANONICAL = 1,
    SPEC_FLOAT_NAN_ARITHMETIC = 2
} spec_float_expectation_t;

typedef enum spec_v128_lane_type_t {
    SPEC_V128_LANE_NONE = 0,
    SPEC_V128_LANE_I8,
    SPEC_V128_LANE_I16,
    SPEC_V128_LANE_I32,
    SPEC_V128_LANE_I64,
    SPEC_V128_LANE_F32,
    SPEC_V128_LANE_F64
} spec_v128_lane_type_t;

typedef struct spec_value_t {
    wasm_value_t value;
    spec_float_expectation_t float_expectation;
    spec_v128_lane_type_t v128_lane_type;
    spec_float_expectation_t v128_lane_expectation[4];
} spec_value_t;

typedef struct spec_forward_func_t {
    wasm_module_t* module;
    uint32_t func_index;
} spec_forward_func_t;

typedef struct spec_named_module_t {
    char* name;
    wasm_module_t* module;
} spec_named_module_t;

typedef struct spec_action_result_t {
    wasm_error_t err;
    char error_text[512];
    wasm_value_t* results;
    uint32_t num_results;
} spec_action_result_t;

typedef struct spec_harness_t {
    wasm_runtime_t runtime;
    wasm_module_t** owned_modules;
    size_t num_owned_modules;
    size_t cap_owned_modules;
    spec_named_module_t* named_modules;
    size_t num_named_modules;
    size_t cap_named_modules;
    spec_forward_func_t* forwarded_funcs;
    size_t num_forwarded_funcs;
    size_t cap_forwarded_funcs;
    wasm_module_t* last_module;
    const char* json_path;
    const char* wasm_tools_path;
    const char* support_wasm;
    char* json_dir;
    const char* source_filename;
    wasm_value_t test_global_i32;
    wasm_value_t test_global_f32;
    wasm_value_t test_global_g;
    wasm_value_t test_global_mut_i32;
    wasm_value_t test_global_mut_i64;
    wasm_value_t empty_global_externref;
} spec_harness_t;

static char* spec_strdup_cstr(const char* text) {
    size_t len;
    char* copy;

    if (!text) return NULL;
    len = strlen(text);
    copy = (char*)malloc(len + 1u);
    if (!copy) return NULL;
    memcpy(copy, text, len + 1u);
    return copy;
}

static void spec_json_parser_init(spec_json_parser_t* parser) {
    parser->pos = 0u;
    parser->toknext = 0u;
    parser->toksuper = -1;
}

static spec_json_token_t* spec_json_alloc_token(spec_json_parser_t* parser,
                                                spec_json_token_t* tokens,
                                                size_t num_tokens) {
    spec_json_token_t* token;

    if (parser->toknext >= num_tokens) return NULL;
    token = &tokens[parser->toknext++];
    token->type = SPEC_JSON_UNDEFINED;
    token->start = -1;
    token->end = -1;
    token->size = 0;
    token->parent = -1;
    return token;
}

static void spec_json_fill_token(spec_json_token_t* token,
                                 spec_json_type_t type,
                                 int start,
                                 int end) {
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

static int spec_json_parse_primitive(spec_json_parser_t* parser,
                                     const char* json,
                                     size_t len,
                                     spec_json_token_t* tokens,
                                     size_t num_tokens) {
    int start = (int)parser->pos;
    spec_json_token_t* token;

    for (; parser->pos < len; parser->pos++) {
        char ch = json[parser->pos];

        if (ch == '\t' || ch == '\r' || ch == '\n' || ch == ' ' || ch == ',' || ch == ']' ||
            ch == '}')
            break;
        if ((unsigned char)ch < 32u) return -2;
    }

    token = spec_json_alloc_token(parser, tokens, num_tokens);
    if (!token) {
        parser->pos = (unsigned)start;
        return -1;
    }
    spec_json_fill_token(token, SPEC_JSON_PRIMITIVE, start, (int)parser->pos);
    token->parent = parser->toksuper;
    if (parser->toksuper >= 0) tokens[parser->toksuper].size++;
    parser->pos--;
    return 0;
}

static int spec_json_parse_string(spec_json_parser_t* parser,
                                  const char* json,
                                  size_t len,
                                  spec_json_token_t* tokens,
                                  size_t num_tokens) {
    int start = (int)parser->pos + 1;
    spec_json_token_t* token;

    for (parser->pos++; parser->pos < len; parser->pos++) {
        char ch = json[parser->pos];

        if (ch == '"') {
            token = spec_json_alloc_token(parser, tokens, num_tokens);
            if (!token) {
                parser->pos = (unsigned)start - 1u;
                return -1;
            }
            spec_json_fill_token(token, SPEC_JSON_STRING, start, (int)parser->pos);
            token->parent = parser->toksuper;
            if (parser->toksuper >= 0) tokens[parser->toksuper].size++;
            return 0;
        }

        if (ch == '\\') {
            parser->pos++;
            if (parser->pos >= len) return -2;
            ch = json[parser->pos];
            if (ch == '"' || ch == '/' || ch == '\\' || ch == 'b' || ch == 'f' || ch == 'n' ||
                ch == 'r' || ch == 't')
                continue;
            if (ch == 'u') {
                int i;

                for (i = 0; i < 4; i++) {
                    parser->pos++;
                    if (parser->pos >= len || !isxdigit((unsigned char)json[parser->pos])) return -2;
                }
                continue;
            }
            return -2;
        }
    }

    return -2;
}

static int spec_json_parse(spec_json_parser_t* parser,
                           const char* json,
                           size_t len,
                           spec_json_token_t* tokens,
                           size_t num_tokens) {
    unsigned i;

    for (; parser->pos < len; parser->pos++) {
        char ch = json[parser->pos];
        spec_json_token_t* token;

        switch (ch) {
            case '{':
            case '[':
                token = spec_json_alloc_token(parser, tokens, num_tokens);
                if (!token) return -1;
                if (parser->toksuper >= 0) tokens[parser->toksuper].size++;
                token->type = (ch == '{') ? SPEC_JSON_OBJECT : SPEC_JSON_ARRAY;
                token->start = (int)parser->pos;
                token->parent = parser->toksuper;
                parser->toksuper = (int)(parser->toknext - 1u);
                break;

            case '}':
            case ']':
                for (i = parser->toknext; i > 0u; i--) {
                    token = &tokens[i - 1u];
                    if (token->start != -1 && token->end == -1) {
                        if ((ch == '}' && token->type != SPEC_JSON_OBJECT) ||
                            (ch == ']' && token->type != SPEC_JSON_ARRAY))
                            return -2;
                        token->end = (int)parser->pos + 1;
                        parser->toksuper = token->parent;
                        break;
                    }
                }
                if (i == 0u) return -2;
                break;

            case '"': {
                int err = spec_json_parse_string(parser, json, len, tokens, num_tokens);
                if (err != 0) return err;
                break;
            }

            case '\t':
            case '\r':
            case '\n':
            case ' ':
            case ':':
            case ',':
                break;

            default: {
                int err = spec_json_parse_primitive(parser, json, len, tokens, num_tokens);
                if (err != 0) return err;
                break;
            }
        }
    }

    for (i = parser->toknext; i > 0u; i--) {
        if (tokens[i - 1u].start != -1 && tokens[i - 1u].end == -1) return -2;
    }

    return (int)parser->toknext;
}

static int spec_token_span_eq(const char* json,
                              const spec_json_token_t* token,
                              const char* text) {
    size_t len;

    if (!json || !token || !text) return 0;
    if (token->start < 0 || token->end < token->start) return 0;
    len = strlen(text);
    return (size_t)(token->end - token->start) == len &&
           strncmp(json + token->start, text, len) == 0;
}

static int spec_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

static char* spec_strdup_token_len(const char* json,
                                   const spec_json_token_t* token,
                                   size_t* out_len) {
    size_t cap;
    size_t decoded_len = 0u;
    char* out;
    int i;

    if (!json || !token || token->start < 0 || token->end < token->start) return NULL;
    cap = (size_t)(token->end - token->start) + 1u;
    out = (char*)malloc(cap);
    if (!out) return NULL;

    for (i = token->start; i < token->end; i++) {
        char ch = json[i];

        if (ch != '\\') {
            out[decoded_len++] = ch;
            continue;
        }

        i++;
        if (i >= token->end) {
            free(out);
            return NULL;
        }

        ch = json[i];
        switch (ch) {
            case '"':
            case '/':
            case '\\':
                out[decoded_len++] = ch;
                break;
            case 'b':
                out[decoded_len++] = '\b';
                break;
            case 'f':
                out[decoded_len++] = '\f';
                break;
            case 'n':
                out[decoded_len++] = '\n';
                break;
            case 'r':
                out[decoded_len++] = '\r';
                break;
            case 't':
                out[decoded_len++] = '\t';
                break;
            case 'u': {
                int h0;
                int h1;
                int h2;
                int h3;
                unsigned value;

                if (i + 4 >= token->end) {
                    free(out);
                    return NULL;
                }
                h0 = spec_hex_value(json[i + 1]);
                h1 = spec_hex_value(json[i + 2]);
                h2 = spec_hex_value(json[i + 3]);
                h3 = spec_hex_value(json[i + 4]);
                if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) {
                    free(out);
                    return NULL;
                }
                value = (unsigned)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
                out[decoded_len++] = value < 128u ? (char)value : '?';
                i += 4;
                break;
            }
            default:
                free(out);
                return NULL;
        }
    }

    out[decoded_len] = '\0';
    if (out_len) *out_len = decoded_len;
    return out;
}

static char* spec_strdup_token(const char* json, const spec_json_token_t* token) {
    return spec_strdup_token_len(json, token, NULL);
}

static int spec_token_next(const spec_json_token_t* tokens, int index) {
    int i;
    int next;

    if (index < 0) return index;
    next = index + 1;
    switch (tokens[index].type) {
        case SPEC_JSON_PRIMITIVE:
        case SPEC_JSON_STRING:
            return next;
        case SPEC_JSON_ARRAY:
            for (i = 0; i < tokens[index].size; i++) next = spec_token_next(tokens, next);
            return next;
        case SPEC_JSON_OBJECT:
            for (i = 0; i < tokens[index].size / 2; i++) {
                next = spec_token_next(tokens, next);
                next = spec_token_next(tokens, next);
            }
            return next;
        default:
            return next;
    }
}

static int spec_object_get(const char* json,
                           const spec_json_token_t* tokens,
                           int object_index,
                           const char* key) {
    int i;
    int cursor;

    if (object_index < 0 || tokens[object_index].type != SPEC_JSON_OBJECT) return -1;
    cursor = object_index + 1;
    for (i = 0; i < tokens[object_index].size / 2; i++) {
        int key_index = cursor;
        int value_index = spec_token_next(tokens, key_index);

        if (tokens[key_index].type == SPEC_JSON_STRING && spec_token_span_eq(json, &tokens[key_index], key))
            return value_index;
        cursor = spec_token_next(tokens, value_index);
    }

    return -1;
}

static int spec_array_at(const spec_json_token_t* tokens, int array_index, int item_index) {
    int i;
    int cursor;

    if (array_index < 0 || tokens[array_index].type != SPEC_JSON_ARRAY) return -1;
    if (item_index < 0 || item_index >= tokens[array_index].size) return -1;
    cursor = array_index + 1;
    for (i = 0; i < item_index; i++) cursor = spec_token_next(tokens, cursor);
    return cursor;
}

static int spec_parse_u64_token(const char* json,
                                const spec_json_token_t* token,
                                uint64_t* out_value) {
    char* text;
    char* endptr;
    unsigned long long value;

    if (!json || !token || !out_value) return 0;
    text = spec_strdup_token(json, token);
    if (!text) return 0;
    errno = 0;
    value = strtoull(text, &endptr, 10);
    if (errno != 0 || !endptr || *endptr != '\0') {
        free(text);
        return 0;
    }
    *out_value = (uint64_t)value;
    free(text);
    return 1;
}

static int spec_parse_wrapping_u64_token(const char* json,
                                         const spec_json_token_t* token,
                                         uint64_t* out_value) {
    char* text;
    char* endptr;

    if (!json || !token || !out_value) return 0;
    text = spec_strdup_token(json, token);
    if (!text) return 0;

    errno = 0;
    if (text[0] == '-') {
        long long value = strtoll(text, &endptr, 10);
        if (errno != 0 || !endptr || *endptr != '\0') {
            free(text);
            return 0;
        }
        *out_value = (uint64_t)value;
    } else {
        unsigned long long value = strtoull(text, &endptr, 10);
        if (errno != 0 || !endptr || *endptr != '\0') {
            free(text);
            return 0;
        }
        *out_value = (uint64_t)value;
    }

    free(text);
    return 1;
}

static int spec_parse_f32_bits_token(const char* json,
                                     const spec_json_token_t* token,
                                     uint32_t* out_bits,
                                     spec_float_expectation_t* out_expectation) {
    char* text;
    uint64_t bits;

    if (!json || !token || !out_bits) return 0;
    if (out_expectation) *out_expectation = SPEC_FLOAT_EXACT;
    text = spec_strdup_token(json, token);
    if (!text) return 0;

    if (strcmp(text, "nan:canonical") == 0) {
        *out_bits = 0x7FC00000u;
        if (out_expectation) *out_expectation = SPEC_FLOAT_NAN_CANONICAL;
        free(text);
        return 1;
    }
    if (strcmp(text, "nan:arithmetic") == 0) {
        *out_bits = 0x7FC00000u;
        if (out_expectation) *out_expectation = SPEC_FLOAT_NAN_ARITHMETIC;
        free(text);
        return 1;
    }

    free(text);
    if (!spec_parse_u64_token(json, token, &bits) || bits > UINT32_MAX) return 0;
    *out_bits = (uint32_t)bits;
    return 1;
}

static int spec_parse_f64_bits_token(const char* json,
                                     const spec_json_token_t* token,
                                     uint64_t* out_bits,
                                     spec_float_expectation_t* out_expectation) {
    char* text;

    if (!json || !token || !out_bits) return 0;
    if (out_expectation) *out_expectation = SPEC_FLOAT_EXACT;
    text = spec_strdup_token(json, token);
    if (!text) return 0;

    if (strcmp(text, "nan:canonical") == 0) {
        *out_bits = UINT64_C(0x7FF8000000000000);
        if (out_expectation) *out_expectation = SPEC_FLOAT_NAN_CANONICAL;
        free(text);
        return 1;
    }
    if (strcmp(text, "nan:arithmetic") == 0) {
        *out_bits = UINT64_C(0x7FF8000000000000);
        if (out_expectation) *out_expectation = SPEC_FLOAT_NAN_ARITHMETIC;
        free(text);
        return 1;
    }

    free(text);
    return spec_parse_u64_token(json, token, out_bits);
}

static int spec_encode_externref_handle(uint64_t bits, uintptr_t* out_value) {
    if (!out_value) return 0;
    if (bits > (uint64_t)UINTPTR_MAX - 1u) return 0;
    *out_value = (uintptr_t)(bits + 1u);
    return 1;
}

static int spec_json_uses_multi_memory(const char* json_path) {
    const char* leaf;

    if (!json_path) return 0;
    leaf = strrchr(json_path, '/');
    leaf = leaf ? leaf + 1 : json_path;
    return strstr(leaf, "memory-multi") != NULL || strstr(leaf, "memory_multi") != NULL;
}

static char* spec_read_file_text(const char* path, size_t* out_size) {
    FILE* file;
    long size;
    char* text;

    if (out_size) *out_size = 0u;
    file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    text = (char*)malloc((size_t)size + 1u);
    if (!text) {
        fclose(file);
        return NULL;
    }
    if (size > 0 && fread(text, 1u, (size_t)size, file) != (size_t)size) {
        free(text);
        fclose(file);
        return NULL;
    }
    fclose(file);
    text[size] = '\0';
    if (out_size) *out_size = (size_t)size;
    return text;
}

static int spec_status_requests_skip(const char* status_path, char* reason, size_t reason_size) {
    char* text;

    if (!status_path) return 0;
    text = spec_read_file_text(status_path, NULL);
    if (!text) return 0;

    if (strncmp(text, "skip\n", 5u) == 0) {
        if (reason && reason_size > 0u) {
            snprintf(reason, reason_size, "%s", text + 5);
        }
        free(text);
        return 1;
    }

    free(text);
    return 0;
}

static unsigned char* spec_read_file_bytes(const char* path, size_t* out_size) {
    FILE* file;
    long size;
    unsigned char* bytes;

    if (out_size) *out_size = 0u;
    file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    bytes = (unsigned char*)malloc((size_t)size > 0u ? (size_t)size : 1u);
    if (!bytes) {
        fclose(file);
        return NULL;
    }
    if (size > 0 && fread(bytes, 1u, (size_t)size, file) != (size_t)size) {
        free(bytes);
        fclose(file);
        return NULL;
    }
    fclose(file);
    if (out_size) *out_size = (size_t)size;
    return bytes;
}

static char* spec_dirname_dup(const char* path) {
    const char* slash;
    size_t len;
    char* dir;

    if (!path) return NULL;
    slash = strrchr(path, '/');
    if (!slash) return spec_strdup_cstr(".");
    len = (size_t)(slash - path);
    if (len == 0u) len = 1u;
    dir = (char*)malloc(len + 1u);
    if (!dir) return NULL;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return dir;
}

static char* spec_path_join(const char* dir, const char* leaf) {
    size_t dir_len;
    size_t leaf_len;
    int need_sep;
    char* path;

    if (!dir || !leaf) return NULL;
    dir_len = strlen(dir);
    leaf_len = strlen(leaf);
    need_sep = dir_len > 0u && dir[dir_len - 1u] != '/';
    path = (char*)malloc(dir_len + (need_sep ? 1u : 0u) + leaf_len + 1u);
    if (!path) return NULL;
    memcpy(path, dir, dir_len);
    if (need_sep) path[dir_len++] = '/';
    memcpy(path + dir_len, leaf, leaf_len);
    path[dir_len + leaf_len] = '\0';
    return path;
}

static const char* spec_runtime_error_text(const wasm_runtime_t* rt, wasm_error_t err) {
    if (rt && rt->error_msg[0] != '\0') return rt->error_msg;
    if (err == WASM_OK) return "runtime returned NULL without setting an error";
    return wasm_error_string(err);
}

static void spec_runtime_clear_error(wasm_runtime_t* rt) {
    if (!rt) return;
    rt->last_error = WASM_OK;
    rt->error_msg[0] = '\0';
}

static void spec_action_result_dispose(spec_action_result_t* result) {
    if (!result) return;
    free(result->results);
    result->results = NULL;
    result->num_results = 0u;
}

static int spec_harness_reserve_modules(spec_harness_t* harness, size_t count) {
    wasm_module_t** modules;
    size_t cap;

    if (harness->num_owned_modules + count <= harness->cap_owned_modules) return 1;
    cap = harness->cap_owned_modules ? harness->cap_owned_modules * 2u : 16u;
    while (cap < harness->num_owned_modules + count) cap *= 2u;
    modules = (wasm_module_t**)realloc(harness->owned_modules, cap * sizeof(*modules));
    if (!modules) return 0;
    harness->owned_modules = modules;
    harness->cap_owned_modules = cap;
    return 1;
}

static int spec_harness_add_owned_module(spec_harness_t* harness, wasm_module_t* module) {
    if (!spec_harness_reserve_modules(harness, 1u)) return 0;
    harness->owned_modules[harness->num_owned_modules++] = module;
    return 1;
}

static int spec_harness_reserve_named_modules(spec_harness_t* harness, size_t count) {
    spec_named_module_t* modules;
    size_t cap;

    if (harness->num_named_modules + count <= harness->cap_named_modules) return 1;
    cap = harness->cap_named_modules ? harness->cap_named_modules * 2u : 16u;
    while (cap < harness->num_named_modules + count) cap *= 2u;
    modules = (spec_named_module_t*)realloc(harness->named_modules, cap * sizeof(*modules));
    if (!modules) return 0;
    harness->named_modules = modules;
    harness->cap_named_modules = cap;
    return 1;
}

static int spec_harness_set_module_name(spec_harness_t* harness,
                                        const char* name,
                                        wasm_module_t* module) {
    size_t i;

    for (i = 0; i < harness->num_named_modules; i++) {
        if (strcmp(harness->named_modules[i].name, name) == 0) {
            harness->named_modules[i].module = module;
            return 1;
        }
    }

    if (!spec_harness_reserve_named_modules(harness, 1u)) return 0;
    harness->named_modules[harness->num_named_modules].name = spec_strdup_cstr(name);
    if (!harness->named_modules[harness->num_named_modules].name) return 0;
    harness->named_modules[harness->num_named_modules].module = module;
    harness->num_named_modules++;
    return 1;
}

static wasm_module_t* spec_harness_find_module(const spec_harness_t* harness, const char* name) {
    size_t i;

    if (!name || !*name) return harness->last_module;
    for (i = 0; i < harness->num_named_modules; i++) {
        if (strcmp(harness->named_modules[i].name, name) == 0) return harness->named_modules[i].module;
    }
    return NULL;
}

static int spec_harness_reserve_forwarders(spec_harness_t* harness, size_t count) {
    spec_forward_func_t* funcs;
    size_t cap;

    if (harness->num_forwarded_funcs + count <= harness->cap_forwarded_funcs) return 1;
    cap = harness->cap_forwarded_funcs ? harness->cap_forwarded_funcs * 2u : 16u;
    while (cap < harness->num_forwarded_funcs + count) cap *= 2u;
    funcs = (spec_forward_func_t*)realloc(harness->forwarded_funcs, cap * sizeof(*funcs));
    if (!funcs) return 0;
    harness->forwarded_funcs = funcs;
    harness->cap_forwarded_funcs = cap;
    return 1;
}

static wasm_error_t spec_forward_import_call(wasm_module_t* mod,
                                             const wasm_value_t* args,
                                             uint32_t num_args,
                                             wasm_value_t* results,
                                             uint32_t num_results,
                                             void* userdata) {
    spec_forward_func_t* forward = (spec_forward_func_t*)userdata;
    (void)mod;
    return wasm_call_index(forward->module, forward->func_index, args, num_args, results, num_results);
}

static wasm_error_t spec_spectest_print_stub(wasm_module_t* mod,
                                             const wasm_value_t* args,
                                             uint32_t num_args,
                                             wasm_value_t* results,
                                             uint32_t num_results,
                                             void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    return WASM_OK;
}

static wasm_error_t spec_bind_print_imports(spec_harness_t* harness) {
    struct binding {
        const char* name;
        const char* fmt;
    } bindings[] = {
        { "print", "(v)" },
        { "print_i32", "i(v)" },
        { "print_i64", "I(v)" },
        { "print_f32", "f(v)" },
        { "print_f64", "F(v)" },
        { "print_i32_f32", "if(v)" },
        { "print_f64_f64", "FF(v)" }
    };
    size_t i;

    for (i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++) {
        wasm_error_t err = wasm_bind_host_func(&harness->runtime,
                                               "spectest",
                                               bindings[i].name,
                                               bindings[i].fmt,
                                               spec_spectest_print_stub,
                                               NULL);
        if (err != WASM_OK) return err;
    }

    return WASM_OK;
}

static wasm_error_t spec_bind_default_test_globals(spec_harness_t* harness) {
    wasm_global_import_t import_desc;
    wasm_error_t err;

    harness->test_global_i32 = wasm_i32(55);
    harness->test_global_f32 = wasm_f32(44.0f);
    harness->test_global_g = wasm_i32(0);
    harness->test_global_mut_i32 = wasm_i32(66);
    harness->test_global_mut_i64 = wasm_i64(66);

    memset(&import_desc, 0, sizeof(import_desc));
    import_desc.module = "test";

    import_desc.name = "global-i32";
    import_desc.type = WASM_TYPE_I32;
    import_desc.value = &harness->test_global_i32;
    err = wasm_register_global_import(&harness->runtime, &import_desc);
    if (err != WASM_OK) return err;

    import_desc.name = "global-f32";
    import_desc.type = WASM_TYPE_F32;
    import_desc.value = &harness->test_global_f32;
    err = wasm_register_global_import(&harness->runtime, &import_desc);
    if (err != WASM_OK) return err;

    import_desc.name = "g";
    import_desc.type = WASM_TYPE_I32;
    import_desc.is_mutable = 1;
    import_desc.value = &harness->test_global_g;
    err = wasm_register_global_import(&harness->runtime, &import_desc);
    if (err != WASM_OK) return err;

    import_desc.name = "global-mut-i32";
    import_desc.type = WASM_TYPE_I32;
    import_desc.value = &harness->test_global_mut_i32;
    err = wasm_register_global_import(&harness->runtime, &import_desc);
    if (err != WASM_OK) return err;

    import_desc.name = "global-mut-i64";
    import_desc.type = WASM_TYPE_I64;
    import_desc.value = &harness->test_global_mut_i64;
    err = wasm_register_global_import(&harness->runtime, &import_desc);
    if (err != WASM_OK) return err;

    return WASM_OK;
}

static wasm_error_t spec_bind_default_empty_imports(spec_harness_t* harness,
                                                   wasm_module_t* support_module) {
    wasm_table_import_t table_import;
    wasm_memory_import_t memory_import;
    wasm_global_import_t global_import;
    wasm_error_t err;
    uint32_t i;
    int bound_table = 0;
    int bound_memory = 0;

    if (!harness || !support_module) return WASM_ERR_MALFORMED;

    harness->empty_global_externref = wasm_externref((uintptr_t)0);
    memset(&global_import, 0, sizeof(global_import));
    global_import.module = "";
    global_import.name = "";
    global_import.type = WASM_TYPE_EXTERNREF;
    global_import.value = &harness->empty_global_externref;
    err = wasm_register_global_import(&harness->runtime, &global_import);
    if (err != WASM_OK) return err;

    for (i = 0; i < wasm_export_count(support_module) && (!bound_table || !bound_memory); i++) {
        wasm_export_kind_t kind = wasm_export_kind(support_module, i);
        uint32_t index = wasm_export_index(support_module, i);

        if (!bound_table && kind == WASM_EXPORT_TABLE) {
            memset(&table_import, 0, sizeof(table_import));
            table_import.module = "";
            table_import.name = "";
            table_import.table = wasm_table_ref_at(support_module, index);
            err = wasm_register_table_import(&harness->runtime, &table_import);
            if (err != WASM_OK) return err;
            bound_table = 1;
            continue;
        }

        if (!bound_memory && kind == WASM_EXPORT_MEM) {
            memset(&memory_import, 0, sizeof(memory_import));
            memory_import.module = "";
            memory_import.name = "";
            memory_import.memory = wasm_memory_ref_at(support_module, index);
            err = wasm_register_memory_import(&harness->runtime, &memory_import);
            if (err != WASM_OK) return err;
            bound_memory = 1;
        }
    }

    return WASM_OK;
}

static int spec_lowercase_copy(char* dst, size_t dst_size, const char* src) {
    size_t i;

    if (!dst || dst_size == 0u) return 0;
    if (!src) {
        dst[0] = '\0';
        return 1;
    }
    for (i = 0; src[i] != '\0' && i + 1u < dst_size; i++) {
        char ch = src[i];

        if (ch == '_' || ch == '-' || ch == '.') ch = ' ';
        dst[i] = (char)tolower((unsigned char)ch);
    }
    dst[i] = '\0';
    return src[i] == '\0';
}

static int spec_message_matches(const char* expected, const char* actual) {
    char expected_lower[256];
    char actual_lower[512];

    if (!expected || !*expected) return 1;
    if (!actual) return 0;
    spec_lowercase_copy(expected_lower, sizeof(expected_lower), expected);
    spec_lowercase_copy(actual_lower, sizeof(actual_lower), actual);

    if (strstr(actual_lower, expected_lower) != NULL) return 1;
    if (strcmp(expected_lower, "unknown import") == 0 && strstr(actual_lower, "unresolved:") != NULL) return 1;
    if (strcmp(expected_lower, "unknown type") == 0 &&
        ((strstr(actual_lower, "type index") != NULL && strstr(actual_lower, "out of range") != NULL) ||
         (strstr(actual_lower, "section 10 decode failed") != NULL && strstr(actual_lower, "malformed module") != NULL) ||
         strstr(actual_lower, "invalid func type index") != NULL))
        return 1;
    if (strcmp(expected_lower, "incompatible import type") == 0 &&
        (strstr(actual_lower, "type mismatch") != NULL || strstr(actual_lower, "import type mismatch") != NULL))
        return 1;
    if (strcmp(expected_lower, "malformed import kind") == 0 &&
        (strstr(actual_lower, "unknown type ") != NULL ||
         (strstr(actual_lower, "section 2 decode failed") != NULL && strstr(actual_lower, "malformed module") != NULL)))
        return 1;
    if (strcmp(expected_lower, "type mismatch") == 0 &&
        (strstr(actual_lower, "requires a funcref table") != NULL ||
         strstr(actual_lower, "wrong stack height") != NULL ||
         strstr(actual_lower, "do not share the same signature") != NULL))
        return 1;
    if (strcmp(expected_lower, "duplicate global") == 0 && strstr(actual_lower, "redefinition of global") != NULL)
        return 1;
    if (strcmp(expected_lower, "duplicate table") == 0 && strstr(actual_lower, "redefinition of table") != NULL)
        return 1;
    if (strcmp(expected_lower, "duplicate memory") == 0 && strstr(actual_lower, "redefinition of memory") != NULL)
        return 1;
    if (strcmp(expected_lower, "duplicate func") == 0 && strstr(actual_lower, "redefinition of function") != NULL)
        return 1;
    if (strcmp(expected_lower, "duplicate local") == 0 &&
        (strstr(actual_lower, "redefinition of parameter") != NULL ||
         strstr(actual_lower, "redefinition of local") != NULL))
        return 1;
    if (strcmp(expected_lower, "unknown global") == 0 &&
        ((strstr(actual_lower, "global index") != NULL && strstr(actual_lower, "out of range") != NULL) ||
         (strstr(actual_lower, "export '") != NULL && strstr(actual_lower, "index") != NULL && strstr(actual_lower, "out of range") != NULL)))
        return 1;
    if (strncmp(expected_lower, "unknown function", 16u) == 0 &&
        ((strstr(actual_lower, "call target") != NULL && strstr(actual_lower, "out of range") != NULL) ||
         (strstr(actual_lower, "start function index") != NULL && strstr(actual_lower, "out of range") != NULL) ||
         (strstr(actual_lower, "export '") != NULL && strstr(actual_lower, "index") != NULL && strstr(actual_lower, "out of range") != NULL)))
        return 1;
    if (strncmp(expected_lower, "unknown label", 13u) == 0 &&
        ((strstr(actual_lower, "branch depth") != NULL && strstr(actual_lower, "out of range") != NULL) ||
         (strstr(actual_lower, "br table depth") != NULL && strstr(actual_lower, "out of range") != NULL) ||
         strstr(actual_lower, "undefined label variable") != NULL))
        return 1;
    if (strncmp(expected_lower, "unknown local", 13u) == 0 &&
        strstr(actual_lower, "local index") != NULL && strstr(actual_lower, "out of range") != NULL)
        return 1;
    if (strcmp(expected_lower, "unexpected end") == 0 &&
        (strstr(actual_lower, "too short") != NULL || strstr(actual_lower, "unexpected eof") != NULL ||
         strstr(actual_lower, "malformed section header") != NULL ||
         strstr(actual_lower, "decode failed") != NULL ||
         strstr(actual_lower, "function body ended before all blocks closed") != NULL ||
         strstr(actual_lower, "expected at least one module field") != NULL))
        return 1;
    if (strcmp(expected_lower, "end opcode expected") == 0 &&
        strstr(actual_lower, "function body ended before all blocks closed") != NULL)
        return 1;
    if (strcmp(expected_lower, "unexpected end of section or function") == 0 &&
        (strstr(actual_lower, "function body ended before all blocks closed") != NULL ||
         (strstr(actual_lower, "decode failed") != NULL && strstr(actual_lower, "malformed module") != NULL)))
        return 1;
    if (strcmp(expected_lower, "length out of bounds") == 0 &&
        (strstr(actual_lower, "overruns module") != NULL ||
         (strstr(actual_lower, "decode failed") != NULL && strstr(actual_lower, "malformed module") != NULL)))
        return 1;
    if ((strncmp(expected_lower, "unknown operator", 16u) == 0 || strcmp(expected_lower, "unexpected token") == 0) &&
        (strstr(actual_lower, "unexpected token") != NULL || strstr(actual_lower, "unexpected type") != NULL ||
         strstr(actual_lower, "invalid literal") != NULL || strstr(actual_lower, "expected ") != NULL ||
         strstr(actual_lower, "u64 constant out of range") != NULL))
        return 1;
    if (strcmp(expected_lower, "mismatching label") == 0 &&
        (strstr(actual_lower, "unexpected label") != NULL || strstr(actual_lower, "undefined label variable") != NULL))
        return 1;
    if (strcmp(expected_lower, "inline function type") == 0 &&
        strstr(actual_lower, "expected ") != NULL && strstr(actual_lower, " got ") != NULL)
        return 1;
    if ((strstr(expected_lower, "constant out of range") != NULL ||
         strstr(expected_lower, "i32 constant out of range") != NULL ||
         strstr(expected_lower, "i64 constant out of range") != NULL) &&
        (strstr(actual_lower, "out of range") != NULL || strstr(actual_lower, "natural number in range") != NULL ||
            strstr(actual_lower, "invalid literal") != NULL || strstr(actual_lower, "invalid int") != NULL ||
            strstr(actual_lower, "memory size must be at most") != NULL ||
            strstr(actual_lower, "table size must be at most") != NULL))
        return 1;
    if (strstr(expected_lower, "i32 constant") != NULL &&
        (strstr(actual_lower, "offset must be less than or equal") != NULL ||
         strstr(actual_lower, "offset out of range") != NULL ||
         strstr(actual_lower, "must be <= 2**32") != NULL ||
         strstr(actual_lower, "exceeds i32 memory address space") != NULL))
        return 1;
    if (strcmp(expected_lower, "alignment must be a power of two") == 0 &&
        strstr(actual_lower, "alignment must be power of two") != NULL)
        return 1;
    if (strcmp(expected_lower, "malformed memop flags") == 0 &&
        (strstr(actual_lower, "alignment") != NULL || strstr(actual_lower, "memop") != NULL ||
         strstr(actual_lower, "feature 'multi memory' is disabled") != NULL))
        return 1;
    if (strcmp(expected_lower, "size minimum must not be greater than maximum") == 0 &&
        strstr(actual_lower, "min") != NULL && strstr(actual_lower, "exceeds max") != NULL)
        return 1;
    if (strcmp(expected_lower, "memory size must be at most 65536 pages (4gib)") == 0 &&
        (strstr(actual_lower, "exceeds max 65536") != NULL ||
         strstr(actual_lower, "exceeds mvp page limit") != NULL))
        return 1;
    if (strcmp(expected_lower, "wrong number of lane literals") == 0 &&
        (strstr(actual_lower, "literal") != NULL || strstr(actual_lower, "unexpected token") != NULL ||
         strstr(actual_lower, "expected a i8") != NULL || strstr(actual_lower, "expected a i16") != NULL ||
         strstr(actual_lower, "expected a ") != NULL ||
         strstr(actual_lower, "expected an instruction") != NULL ||
         strstr(actual_lower, "constant out of range") != NULL))
        return 1;
    if (strcmp(expected_lower, "invalid lane length") == 0 &&
        (strstr(actual_lower, "expected a natural number in range") != NULL ||
         strstr(actual_lower, "unexpected token") != NULL ||
         strstr(actual_lower, "expected a u8") != NULL ||
         strstr(actual_lower, "expected an instruction") != NULL))
        return 1;
    if (strcmp(expected_lower, "offset out of range") == 0 &&
        strstr(actual_lower, "offset must be less than or equal") != NULL)
        return 1;
    if (strcmp(expected_lower, "invalid lane index") == 0 &&
        strstr(actual_lower, "lane") != NULL && strstr(actual_lower, "out of range") != NULL)
        return 1;
    if (strcmp(expected_lower, "malformed lane index") == 0 &&
        ((strstr(actual_lower, "lane index") != NULL && strstr(actual_lower, "out of range") != NULL) ||
         strstr(actual_lower, "invalid literal") != NULL ||
         strstr(actual_lower, "natural number in range") != NULL ||
         strstr(actual_lower, "expected a u8") != NULL ||
         strstr(actual_lower, "invalid u8 number") != NULL ||
         strstr(actual_lower, "constant out of range") != NULL))
        return 1;
    if (strcmp(expected_lower, "alignment must not be larger than natural") == 0 &&
        strstr(actual_lower, "exceeds natural alignment") != NULL)
        return 1;
    if (strcmp(expected_lower, "unknown table") == 0 &&
        ((strstr(actual_lower, "table index") != NULL && strstr(actual_lower, "out of range") != NULL) ||
         (strstr(actual_lower, "export '") != NULL && strstr(actual_lower, "index") != NULL && strstr(actual_lower, "out of range") != NULL)))
        return 1;
    if (strcmp(expected_lower, "unknown memory") == 0 &&
        (strstr(actual_lower, "unknown memory") != NULL ||
         (strstr(actual_lower, "export '") != NULL && strstr(actual_lower, "index") != NULL && strstr(actual_lower, "out of range") != NULL)))
        return 1;
    if (strcmp(expected_lower, "global is immutable") == 0 &&
        strstr(actual_lower, "global") != NULL && strstr(actual_lower, "is immutable") != NULL)
        return 1;
    if (strcmp(expected_lower, "malformed mutability") == 0 &&
        strstr(actual_lower, "malformed module") != NULL)
        return 1;
    if (strstr(expected_lower, "uninitialized element") != NULL &&
        strstr(actual_lower, "uninitialized table element") != NULL)
        return 1;
    if ((strcmp(expected_lower, "out of bounds table access") == 0 ||
         strcmp(expected_lower, "out of bounds memory access") == 0) &&
        strstr(actual_lower, "out of bounds") != NULL)
        return 1;
    if (strcmp(expected_lower, "integer divide by zero") == 0 &&
        strstr(actual_lower, "divide by zero") != NULL)
        return 1;
    if (strcmp(expected_lower, "integer overflow") == 0 &&
        strstr(actual_lower, "invalid conversion to integer") != NULL)
        return 1;
    if (strcmp(expected_lower, "integer representation too long") == 0 &&
        ((strstr(actual_lower, "decode failed") != NULL && strstr(actual_lower, "malformed module") != NULL) ||
         strstr(actual_lower, "malformed section header") != NULL ||
         strstr(actual_lower, "trailing bytes") != NULL ||
         strstr(actual_lower, "overruns module") != NULL ||
         strstr(actual_lower, "unknown section id") != NULL ||
         strstr(actual_lower, "malformed immediate") != NULL ||
         strstr(actual_lower, "integer representation too long") != NULL))
        return 1;
    if (strcmp(expected_lower, "integer too large") == 0 &&
        ((strstr(actual_lower, "decode failed") != NULL && strstr(actual_lower, "malformed module") != NULL) ||
         strstr(actual_lower, "malformed section header") != NULL ||
         strstr(actual_lower, "malformed immediate") != NULL ||
         strstr(actual_lower, "trailing bytes") != NULL ||
         strstr(actual_lower, "integer too large") != NULL))
        return 1;
    if (strcmp(expected_lower, "magic header not detected") == 0 &&
        (strstr(actual_lower, "too short") != NULL || strstr(actual_lower, "bad magic") != NULL))
        return 1;
    if (strcmp(expected_lower, "unknown binary version") == 0 &&
        strstr(actual_lower, "version ") != NULL)
        return 1;
    if (strcmp(expected_lower, "illegal opcode") == 0 &&
        ((strstr(actual_lower, "decode failed") != NULL && strstr(actual_lower, "malformed module") != NULL) ||
         strstr(actual_lower, "trailing bytes") != NULL ||
         strstr(actual_lower, "constant expression required") != NULL))
        return 1;
    if (strcmp(expected_lower, "malformed section id") == 0 &&
        strstr(actual_lower, "unknown section id") != NULL)
        return 1;
    if (strcmp(expected_lower, "malformed reference type") == 0 &&
        strstr(actual_lower, "malformed module") != NULL)
        return 1;
    if (strcmp(expected_lower, "function and code section have inconsistent lengths") == 0 &&
        (strstr(actual_lower, "section 10 decode failed") != NULL ||
         strstr(actual_lower, "code section count") != NULL))
        return 1;
    if (strcmp(expected_lower, "data count section required") == 0 &&
        strstr(actual_lower, "data count section is required") != NULL)
        return 1;
    if (strcmp(expected_lower, "multiple start sections") == 0 &&
        strstr(actual_lower, "section out of order") != NULL)
        return 1;
    if (strcmp(expected_lower, "unexpected content after last section") == 0 &&
        (strstr(actual_lower, "section out of order") != NULL ||
         strstr(actual_lower, "function and code section have inconsistent lengths") != NULL ||
         strstr(actual_lower, "code section count") != NULL ||
         strstr(actual_lower, "data count section declares") != NULL))
        return 1;
    if (strcmp(expected_lower, "section size mismatch") == 0 &&
        (strstr(actual_lower, "function body ended before all blocks closed") != NULL ||
         strstr(actual_lower, "has trailing bytes") != NULL))
        return 1;
    if (strcmp(expected_lower, "data count and data section have inconsistent lengths") == 0 &&
        strstr(actual_lower, "data count section declares") != NULL)
        return 1;
    if (strcmp(expected_lower, "invalid result arity") == 0 &&
        (strstr(actual_lower, "type mismatch: stack underflow") != NULL ||
         strstr(actual_lower, "typed select requires exactly one value type") != NULL))
        return 1;
    if (strcmp(expected_lower, "type mismatch") == 0 &&
        (strstr(actual_lower, "select requires matching numeric or vector operand types") != NULL ||
         strstr(actual_lower, "ref is null requires a reference operand") != NULL))
        return 1;
    if (strcmp(expected_lower, "malformed utf 8 encoding") == 0 &&
        (strstr(actual_lower, "invalid utf 8 encoding") != NULL ||
         (strstr(actual_lower, "decode failed") != NULL && strstr(actual_lower, "malformed module") != NULL)))
        return 1;
    if (strcmp(expected_lower, "type mismatch") == 0 &&
        strstr(actual_lower, "if without else requires param and result types to match") != NULL)
        return 1;
    if (strcmp(expected_lower, "call stack exhausted") == 0 &&
        strstr(actual_lower, "call frame depth exceeded limit") != NULL)
        return 1;
    if (strncmp(expected_lower, "import after ", 13u) == 0 &&
        strstr(actual_lower, "imports must occur before all non import definitions") != NULL)
        return 1;
    if (strcmp(expected_lower, "unreachable") == 0 && strstr(actual_lower, "unreachable") != NULL)
        return 1;
    return 0;
}

static uint32_t spec_f32_bits(float value) {
    union {
        float f;
        uint32_t u;
    } bits;

    bits.f = value;
    return bits.u;
}

static uint64_t spec_f64_bits(double value) {
    union {
        double f;
        uint64_t u;
    } bits;

    bits.f = value;
    return bits.u;
}

static float spec_f32_from_bits(uint32_t bits) {
    union {
        float f;
        uint32_t u;
    } value;

    value.u = bits;
    return value.f;
}

static double spec_f64_from_bits(uint64_t bits) {
    union {
        double f;
        uint64_t u;
    } value;

    value.u = bits;
    return value.f;
}

static int spec_f32_matches(uint32_t actual_bits, spec_float_expectation_t expectation, uint32_t expected_bits) {
    uint32_t magnitude = actual_bits & 0x7FFFFFFFu;

    if (expectation == SPEC_FLOAT_EXACT) return actual_bits == expected_bits;
    if ((magnitude & 0x7F800000u) != 0x7F800000u || (magnitude & 0x007FFFFFu) == 0u) return 0;
    if (expectation == SPEC_FLOAT_NAN_CANONICAL) return magnitude == 0x7FC00000u;
    return (magnitude & 0x00400000u) != 0u;
}

static int spec_f64_matches(uint64_t actual_bits, spec_float_expectation_t expectation, uint64_t expected_bits) {
    uint64_t magnitude = actual_bits & UINT64_C(0x7FFFFFFFFFFFFFFF);

    if (expectation == SPEC_FLOAT_EXACT) return actual_bits == expected_bits;
    if ((magnitude & UINT64_C(0x7FF0000000000000)) != UINT64_C(0x7FF0000000000000) ||
        (magnitude & UINT64_C(0x000FFFFFFFFFFFFF)) == 0u)
        return 0;
    if (expectation == SPEC_FLOAT_NAN_CANONICAL) return magnitude == UINT64_C(0x7FF8000000000000);
    return (magnitude & UINT64_C(0x0008000000000000)) != 0u;
}

static void spec_set_error(char* buffer, size_t buffer_size, const char* fmt, ...) {
    va_list ap;

    if (!buffer || buffer_size == 0u) return;
    va_start(ap, fmt);
    vsnprintf(buffer, buffer_size, fmt, ap);
    va_end(ap);
}

static int spec_parse_v128(const char* json,
                           const spec_json_token_t* tokens,
                           int value_index,
                           const char* lane_type,
                           spec_value_t* out_value,
                           char* error_text,
                           size_t error_size) {
    uint8_t bytes[16];
    int i;
    int count;

    if (!out_value) return 0;

    memset(bytes, 0, sizeof(bytes));
    out_value->v128_lane_type = SPEC_V128_LANE_NONE;
    for (i = 0; i < 4; i++) out_value->v128_lane_expectation[i] = SPEC_FLOAT_EXACT;

    if (value_index < 0 || tokens[value_index].type != SPEC_JSON_ARRAY) {
        spec_set_error(error_text, error_size, "v128 value must be an array");
        return 0;
    }

    count = tokens[value_index].size;
    if (strcmp(lane_type, "i8") == 0) {
        out_value->v128_lane_type = SPEC_V128_LANE_I8;
        if (count != 16) {
            spec_set_error(error_text, error_size, "v128 i8 expects 16 lanes");
            return 0;
        }
        for (i = 0; i < 16; i++) {
            uint64_t lane_bits;
            int lane_index = spec_array_at(tokens, value_index, i);

            if (!spec_parse_wrapping_u64_token(json, &tokens[lane_index], &lane_bits)) {
                spec_set_error(error_text, error_size, "invalid v128 i8 lane");
                return 0;
            }
            bytes[i] = (uint8_t)lane_bits;
        }
    } else if (strcmp(lane_type, "i16") == 0) {
        out_value->v128_lane_type = SPEC_V128_LANE_I16;
        if (count != 8) {
            spec_set_error(error_text, error_size, "v128 i16 expects 8 lanes");
            return 0;
        }
        for (i = 0; i < 8; i++) {
            uint64_t lane_bits;
            uint16_t lane;
            int lane_index = spec_array_at(tokens, value_index, i);

            if (!spec_parse_wrapping_u64_token(json, &tokens[lane_index], &lane_bits)) {
                spec_set_error(error_text, error_size, "invalid v128 i16 lane");
                return 0;
            }
            lane = (uint16_t)lane_bits;
            bytes[i * 2] = (uint8_t)(lane & 0xFFu);
            bytes[i * 2 + 1] = (uint8_t)(lane >> 8);
        }
    } else if (strcmp(lane_type, "i32") == 0) {
        out_value->v128_lane_type = SPEC_V128_LANE_I32;
        if (count != 4) {
            spec_set_error(error_text, error_size, "v128 i32/f32 expects 4 lanes");
            return 0;
        }
        for (i = 0; i < 4; i++) {
            uint64_t lane_bits;
            uint32_t lane;
            int lane_index = spec_array_at(tokens, value_index, i);

            if (!spec_parse_wrapping_u64_token(json, &tokens[lane_index], &lane_bits)) {
                spec_set_error(error_text, error_size, "invalid v128 i32/f32 lane");
                return 0;
            }
            lane = (uint32_t)lane_bits;
            bytes[i * 4] = (uint8_t)(lane & 0xFFu);
            bytes[i * 4 + 1] = (uint8_t)((lane >> 8) & 0xFFu);
            bytes[i * 4 + 2] = (uint8_t)((lane >> 16) & 0xFFu);
            bytes[i * 4 + 3] = (uint8_t)((lane >> 24) & 0xFFu);
        }
    } else if (strcmp(lane_type, "f32") == 0) {
        out_value->v128_lane_type = SPEC_V128_LANE_F32;
        if (count != 4) {
            spec_set_error(error_text, error_size, "v128 i32/f32 expects 4 lanes");
            return 0;
        }
        for (i = 0; i < 4; i++) {
            uint32_t lane_bits;
            spec_float_expectation_t lane_expectation;
            int lane_index = spec_array_at(tokens, value_index, i);

            if (!spec_parse_f32_bits_token(json, &tokens[lane_index], &lane_bits, &lane_expectation)) {
                spec_set_error(error_text, error_size, "invalid v128 i32/f32 lane");
                return 0;
            }
            out_value->v128_lane_expectation[i] = lane_expectation;
            bytes[i * 4] = (uint8_t)(lane_bits & 0xFFu);
            bytes[i * 4 + 1] = (uint8_t)((lane_bits >> 8) & 0xFFu);
            bytes[i * 4 + 2] = (uint8_t)((lane_bits >> 16) & 0xFFu);
            bytes[i * 4 + 3] = (uint8_t)((lane_bits >> 24) & 0xFFu);
        }
    } else if (strcmp(lane_type, "i64") == 0) {
        out_value->v128_lane_type = SPEC_V128_LANE_I64;
        if (count != 2) {
            spec_set_error(error_text, error_size, "v128 i64/f64 expects 2 lanes");
            return 0;
        }
        for (i = 0; i < 2; i++) {
            uint64_t lane;
            int lane_index = spec_array_at(tokens, value_index, i);
            int j;

            if (!spec_parse_wrapping_u64_token(json, &tokens[lane_index], &lane)) {
                spec_set_error(error_text, error_size, "invalid v128 i64/f64 lane");
                return 0;
            }
            for (j = 0; j < 8; j++) bytes[i * 8 + j] = (uint8_t)(lane >> (j * 8));
        }
    } else if (strcmp(lane_type, "f64") == 0) {
        out_value->v128_lane_type = SPEC_V128_LANE_F64;
        if (count != 2) {
            spec_set_error(error_text, error_size, "v128 i64/f64 expects 2 lanes");
            return 0;
        }
        for (i = 0; i < 2; i++) {
            uint64_t lane_bits;
            spec_float_expectation_t lane_expectation;
            int lane_index = spec_array_at(tokens, value_index, i);
            int j;

            if (!spec_parse_f64_bits_token(json, &tokens[lane_index], &lane_bits, &lane_expectation)) {
                spec_set_error(error_text, error_size, "invalid v128 i64/f64 lane");
                return 0;
            }
            out_value->v128_lane_expectation[i] = lane_expectation;
            for (j = 0; j < 8; j++) bytes[i * 8 + j] = (uint8_t)(lane_bits >> (j * 8));
        }
    } else {
        spec_set_error(error_text, error_size, "unsupported v128 lane type '%s'", lane_type);
        return 0;
    }

    out_value->value = wasm_v128(bytes);
    return 1;
}

static int spec_parse_value(const char* json,
                            const spec_json_token_t* tokens,
                            int object_index,
                            spec_value_t* out_value,
                            char* error_text,
                            size_t error_size) {
    char* type_text;
    char* value_text = NULL;
    char* lane_type = NULL;
    int type_index;
    int value_index;
    int lane_type_index;
    int ok = 0;

    if (!out_value) return 0;
    memset(out_value, 0, sizeof(*out_value));
    out_value->float_expectation = SPEC_FLOAT_EXACT;

    type_index = spec_object_get(json, tokens, object_index, "type");
    value_index = spec_object_get(json, tokens, object_index, "value");
    lane_type_index = spec_object_get(json, tokens, object_index, "lane_type");
    if (type_index < 0) {
        spec_set_error(error_text, error_size, "missing value type");
        return 0;
    }

    type_text = spec_strdup_token(json, &tokens[type_index]);
    if (!type_text) {
        spec_set_error(error_text, error_size, "missing value type");
        return 0;
    }

    if (strcmp(type_text, "i32") == 0) {
        uint64_t bits;
        if (!spec_parse_wrapping_u64_token(json, &tokens[value_index], &bits)) {
            spec_set_error(error_text, error_size, "invalid i32 value");
            goto done;
        }
        out_value->value = wasm_i32((int32_t)(uint32_t)bits);
    } else if (strcmp(type_text, "i64") == 0) {
        uint64_t bits;
        if (!spec_parse_wrapping_u64_token(json, &tokens[value_index], &bits)) {
            spec_set_error(error_text, error_size, "invalid i64 value");
            goto done;
        }
        out_value->value = wasm_i64((int64_t)bits);
    } else if (strcmp(type_text, "f32") == 0) {
        uint32_t bits;
        if (!spec_parse_f32_bits_token(json, &tokens[value_index], &bits, &out_value->float_expectation)) {
            spec_set_error(error_text, error_size, "invalid f32 bit pattern");
            goto done;
        }
        out_value->value = wasm_f32(spec_f32_from_bits(bits));
    } else if (strcmp(type_text, "f64") == 0) {
        uint64_t bits;
        if (!spec_parse_f64_bits_token(json, &tokens[value_index], &bits, &out_value->float_expectation)) {
            spec_set_error(error_text, error_size, "invalid f64 bit pattern");
            goto done;
        }
        out_value->value = wasm_f64(spec_f64_from_bits(bits));
    } else if (strcmp(type_text, "externref") == 0) {
        uint64_t bits;
        uintptr_t encoded;
        value_text = spec_strdup_token(json, &tokens[value_index]);
        if (!value_text) {
            spec_set_error(error_text, error_size, "invalid externref value");
            goto done;
        }
        if (strcmp(value_text, "null") == 0) {
            out_value->value = wasm_ref_null(WASM_TYPE_EXTERNREF);
        } else {
            if (!spec_parse_u64_token(json, &tokens[value_index], &bits) ||
                !spec_encode_externref_handle(bits, &encoded)) {
                spec_set_error(error_text, error_size, "invalid externref value");
                goto done;
            }
            out_value->value = wasm_externref(encoded);
        }
    } else if (strcmp(type_text, "funcref") == 0) {
        uint64_t bits;
        value_text = spec_strdup_token(json, &tokens[value_index]);
        if (!value_text) {
            spec_set_error(error_text, error_size, "invalid funcref value");
            goto done;
        }
        if (strcmp(value_text, "null") == 0) {
            out_value->value = wasm_ref_null(WASM_TYPE_FUNCREF);
        } else {
            if (!spec_parse_u64_token(json, &tokens[value_index], &bits) || bits > UINT32_MAX) {
                spec_set_error(error_text, error_size, "invalid funcref value");
                goto done;
            }
            out_value->value = wasm_funcref((uint32_t)bits);
        }
    } else if (strcmp(type_text, "i31ref") == 0) {
        uint64_t bits;
        value_text = spec_strdup_token(json, &tokens[value_index]);
        if (!value_text) {
            spec_set_error(error_text, error_size, "invalid i31ref value");
            goto done;
        }
        if (strcmp(value_text, "null") == 0) {
            out_value->value = wasm_ref_null(WASM_TYPE_I31REF);
        } else {
            if (!spec_parse_u64_token(json, &tokens[value_index], &bits) || bits > UINT32_MAX) {
                spec_set_error(error_text, error_size, "invalid i31ref value");
                goto done;
            }
            out_value->value = wasm_i31ref((int32_t)(uint32_t)bits);
        }
    } else if (strcmp(type_text, "anyref") == 0 || strcmp(type_text, "eqref") == 0 ||
               strcmp(type_text, "structref") == 0 || strcmp(type_text, "arrayref") == 0) {
        wasm_valtype_t ref_type = WASM_TYPE_ANYREF;
        value_text = spec_strdup_token(json, &tokens[value_index]);
        if (!value_text) {
            spec_set_error(error_text, error_size, "invalid reference value");
            goto done;
        }
        if (strcmp(type_text, "eqref") == 0) ref_type = WASM_TYPE_EQREF;
        else if (strcmp(type_text, "structref") == 0) ref_type = WASM_TYPE_STRUCTREF;
        else if (strcmp(type_text, "arrayref") == 0) ref_type = WASM_TYPE_ARRAYREF;
        if (strcmp(value_text, "null") != 0) {
            spec_set_error(error_text, error_size, "only null reference values are supported");
            goto done;
        }
        out_value->value = wasm_ref_null(ref_type);
    } else if (strcmp(type_text, "v128") == 0) {
        lane_type = lane_type_index >= 0 ? spec_strdup_token(json, &tokens[lane_type_index]) : NULL;
        if (!lane_type) {
            spec_set_error(error_text, error_size, "missing v128 lane type");
            goto done;
        }
        if (!spec_parse_v128(json, tokens, value_index, lane_type, out_value, error_text, error_size)) {
            goto done;
        }
    } else {
        spec_set_error(error_text, error_size, "unsupported value type '%s'", type_text);
        goto done;
    }

    ok = 1;

done:
    free(lane_type);
    free(value_text);
    return ok;
}

static void spec_describe_value(const wasm_value_t* value, char* buffer, size_t buffer_size) {
    size_t i;

    if (!buffer || buffer_size == 0u || !value) return;
    switch (value->type) {
        case WASM_TYPE_I32:
            snprintf(buffer, buffer_size, "i32(0x%08" PRIx32 ")", (uint32_t)value->of.i32);
            break;
        case WASM_TYPE_I64:
            snprintf(buffer, buffer_size, "i64(0x%016" PRIx64 ")", (uint64_t)value->of.i64);
            break;
        case WASM_TYPE_F32:
            snprintf(buffer, buffer_size, "f32(0x%08" PRIx32 ")", spec_f32_bits(value->of.f32));
            break;
        case WASM_TYPE_F64:
            snprintf(buffer, buffer_size, "f64(0x%016" PRIx64 ")", spec_f64_bits(value->of.f64));
            break;
        case WASM_TYPE_FUNCREF:
            if (value->of.funcref == UINT32_MAX)
                snprintf(buffer, buffer_size, "funcref(null)");
            else
                snprintf(buffer, buffer_size, "funcref(%" PRIu32 ")", value->of.funcref);
            break;
        case WASM_TYPE_EXTERNREF:
            if (value->of.externref == (uintptr_t)0)
                snprintf(buffer, buffer_size, "externref(null)");
            else
                snprintf(buffer, buffer_size, "externref(0x%" PRIxPTR ")", value->of.externref);
            break;
        case WASM_TYPE_I31REF:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            if (value->of.gc_ref == (uintptr_t)0)
                snprintf(buffer, buffer_size, "ref(null)");
            else
                snprintf(buffer, buffer_size, "ref(0x%" PRIxPTR ")", value->of.gc_ref);
            break;
        case WASM_TYPE_V128: {
            size_t offset = 0u;
            offset += (size_t)snprintf(buffer + offset, buffer_size - offset, "v128(");
            for (i = 0; i < 16u && offset + 2u < buffer_size; i++) {
                offset += (size_t)snprintf(buffer + offset, buffer_size - offset, "%02x", value->of.v128[i]);
            }
            snprintf(buffer + offset, buffer_size - offset, ")");
            break;
        }
        default:
            snprintf(buffer, buffer_size, "type(0x%x)", (unsigned)value->type);
            break;
    }
}

static int spec_values_match(const spec_value_t* expected,
                             const wasm_value_t* actual,
                             char* error_text,
                             size_t error_size) {
    char actual_desc[128];
    char expected_desc[128];

    if (expected->value.type != actual->type) {
        spec_describe_value(&expected->value, expected_desc, sizeof(expected_desc));
        spec_describe_value(actual, actual_desc, sizeof(actual_desc));
        spec_set_error(error_text, error_size, "type mismatch: expected %s but got %s", expected_desc, actual_desc);
        return 0;
    }

    switch (actual->type) {
        case WASM_TYPE_I32:
            if ((uint32_t)expected->value.of.i32 == (uint32_t)actual->of.i32) return 1;
            break;
        case WASM_TYPE_I64:
            if ((uint64_t)expected->value.of.i64 == (uint64_t)actual->of.i64) return 1;
            break;
        case WASM_TYPE_F32:
            if (spec_f32_matches(spec_f32_bits(actual->of.f32),
                                 expected->float_expectation,
                                 spec_f32_bits(expected->value.of.f32)))
                return 1;
            break;
        case WASM_TYPE_F64:
            if (spec_f64_matches(spec_f64_bits(actual->of.f64),
                                 expected->float_expectation,
                                 spec_f64_bits(expected->value.of.f64)))
                return 1;
            break;
        case WASM_TYPE_FUNCREF:
            if (expected->value.of.funcref == actual->of.funcref) return 1;
            break;
        case WASM_TYPE_EXTERNREF:
            if (expected->value.of.externref == actual->of.externref) return 1;
            break;
        case WASM_TYPE_I31REF:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            if (expected->value.of.gc_ref == actual->of.gc_ref) return 1;
            break;
        case WASM_TYPE_V128:
            if (expected->v128_lane_type == SPEC_V128_LANE_F32) {
                uint32_t lane;
                for (lane = 0; lane < 4u; lane++) {
                    if (!spec_f32_matches(wasm__v128_get_u32(actual, lane),
                                          expected->v128_lane_expectation[lane],
                                          wasm__v128_get_u32(&expected->value, lane)))
                        break;
                }
                if (lane == 4u) return 1;
                break;
            }
            if (expected->v128_lane_type == SPEC_V128_LANE_F64) {
                uint32_t lane;
                for (lane = 0; lane < 2u; lane++) {
                    if (!spec_f64_matches(wasm__v128_get_u64(actual, lane),
                                          expected->v128_lane_expectation[lane],
                                          wasm__v128_get_u64(&expected->value, lane)))
                        break;
                }
                if (lane == 2u) return 1;
                break;
            }
            if (memcmp(expected->value.of.v128, actual->of.v128, 16u) == 0) return 1;
            break;
        default:
            break;
    }

    spec_describe_value(&expected->value, expected_desc, sizeof(expected_desc));
    spec_describe_value(actual, actual_desc, sizeof(actual_desc));
    spec_set_error(error_text, error_size, "expected %s but got %s", expected_desc, actual_desc);
    return 0;
}

static int spec_parse_expected_list(const char* json,
                                    const spec_json_token_t* tokens,
                                    int array_index,
                                    spec_value_t** out_values,
                                    uint32_t* out_count,
                                    char* error_text,
                                    size_t error_size) {
    spec_value_t* values;
    int i;
    int count;

    if (out_values) *out_values = NULL;
    if (out_count) *out_count = 0u;
    if (array_index < 0) return 1;
    if (tokens[array_index].type != SPEC_JSON_ARRAY) {
        spec_set_error(error_text, error_size, "expected list is not an array");
        return 0;
    }

    count = tokens[array_index].size;
    if (count == 0) {
        if (out_values) *out_values = NULL;
        if (out_count) *out_count = 0u;
        return 1;
    }

    values = (spec_value_t*)calloc((size_t)count, sizeof(*values));
    if (!values) {
        spec_set_error(error_text, error_size, "out of memory parsing expected values");
        return 0;
    }

    for (i = 0; i < count; i++) {
        int item_index = spec_array_at(tokens, array_index, i);
        if (!spec_parse_value(json, tokens, item_index, &values[i], error_text, error_size)) {
            free(values);
            return 0;
        }
    }

    if (out_values) *out_values = values;
    if (out_count) *out_count = (uint32_t)count;
    return 1;
}

static int spec_compare_results_against_array(const char* json,
                                              const spec_json_token_t* tokens,
                                              int array_index,
                                              const wasm_value_t* actual,
                                              uint32_t actual_count,
                                              char* error_text,
                                              size_t error_size) {
    spec_value_t* expected = NULL;
    uint32_t expected_count = 0u;
    uint32_t i;
    int ok;

    if (!spec_parse_expected_list(json, tokens, array_index, &expected, &expected_count, error_text, error_size))
        return 0;

    if (expected_count != actual_count) {
        free(expected);
        spec_set_error(error_text, error_size, "result count mismatch: expected %u but got %u",
                       (unsigned)expected_count, (unsigned)actual_count);
        return 0;
    }

    ok = 1;
    for (i = 0; i < expected_count; i++) {
        if (!spec_values_match(&expected[i], &actual[i], error_text, error_size)) {
            ok = 0;
            break;
        }
    }

    free(expected);
    return ok;
}

static int spec_compare_expected(const char* json,
                                 const spec_json_token_t* tokens,
                                 int command_index,
                                 const wasm_value_t* actual,
                                 uint32_t actual_count,
                                 char* error_text,
                                 size_t error_size) {
    int either_index = spec_object_get(json, tokens, command_index, "either");
    int expected_index = spec_object_get(json, tokens, command_index, "expected");

    if (either_index >= 0) {
        int i;

        if (tokens[either_index].type != SPEC_JSON_ARRAY) {
            spec_set_error(error_text, error_size, "'either' must be an array");
            return 0;
        }

        for (i = 0; i < tokens[either_index].size; i++) {
            char candidate_error[256];
            int candidate_index = spec_array_at(tokens, either_index, i);

            if (spec_compare_results_against_array(json, tokens, candidate_index, actual, actual_count,
                                                   candidate_error, sizeof(candidate_error)))
                return 1;
        }

        spec_set_error(error_text, error_size, "result did not match any 'either' candidate");
        return 0;
    }

    return spec_compare_results_against_array(json, tokens, expected_index, actual, actual_count, error_text, error_size);
}

static int spec_run_command_capture(const char* command, char** out_text, int* out_exit_code) {
    char temp_path[] = "/tmp/wl-spectest-XXXXXX";
    char* text = NULL;
    char* wrapped;
    int fd;
    int status;

    if (out_text) *out_text = NULL;
    if (out_exit_code) *out_exit_code = -1;

    fd = mkstemp(temp_path);
    if (fd < 0) return 0;
    close(fd);

    wrapped = (char*)malloc(strlen(command) + strlen(temp_path) + 16u);
    if (!wrapped) {
        SPEC_UNLINK(temp_path);
        return 0;
    }

    sprintf(wrapped, "%s > \"%s\" 2>&1", command, temp_path);
    status = system(wrapped);
    free(wrapped);

    text = spec_read_file_text(temp_path, NULL);
    SPEC_UNLINK(temp_path);

    if (out_text) *out_text = text;
    else free(text);

    if (out_exit_code) {
#if defined(_WIN32)
        *out_exit_code = status;
#else
        if (WIFEXITED(status))
            *out_exit_code = WEXITSTATUS(status);
        else
            *out_exit_code = status;
#endif
    }

    return 1;
}

static int spec_compile_wat(spec_harness_t* harness,
                            const char* wat_path,
                            const char* wasm_path,
                            char** compiler_output,
                            int* exit_code) {
    char* command;
    int ok;

    command = (char*)malloc(strlen(harness->wasm_tools_path) + strlen(wat_path) + strlen(wasm_path) + 80u);
    if (!command) {
        return 0;
    }
    sprintf(command, "\"%s\" parse \"%s\" -o \"%s\"", harness->wasm_tools_path, wat_path, wasm_path);
    ok = spec_run_command_capture(command, compiler_output, exit_code);
    free(command);
    return ok;
}

static int spec_validate_wasm(spec_harness_t* harness,
                              const char* wasm_path,
                              char** validator_output,
                              int* exit_code) {
    char* command;
    int ok;

    command = (char*)malloc(strlen(harness->wasm_tools_path) + strlen(wasm_path) + 40u);
    if (!command) {
        return 0;
    }
    sprintf(command, "\"%s\" validate \"%s\"", harness->wasm_tools_path, wasm_path);
    ok = spec_run_command_capture(command, validator_output, exit_code);
    free(command);
    return ok;
}

static int spec_bind_export(spec_harness_t* harness,
                            const char* module_name,
                            wasm_module_t* module,
                            const char* export_name,
                            wasm_export_kind_t kind,
                            uint32_t index,
                            char* error_text,
                            size_t error_size) {
    wasm_error_t err;

    if (kind == WASM_EXPORT_FUNC) {
        const wasm_functype_t* type = wasm_func_functype(module, index);
        wasm_import_t import_desc;
        spec_forward_func_t* forward;

        if (!type) {
            spec_set_error(error_text, error_size, "missing function type for export '%s'", export_name);
            return 0;
        }
        if (!spec_harness_reserve_forwarders(harness, 1u)) {
            spec_set_error(error_text, error_size, "out of memory storing forwarded function");
            return 0;
        }
        forward = &harness->forwarded_funcs[harness->num_forwarded_funcs++];
        forward->module = module;
        forward->func_index = index;
        memset(&import_desc, 0, sizeof(import_desc));
        import_desc.module = module_name;
        import_desc.name = export_name;
        import_desc.type = *type;
        import_desc.func = spec_forward_import_call;
        import_desc.userdata = forward;
        err = wasm__register_import_internal(&harness->runtime, &import_desc, 0);
    } else if (kind == WASM_EXPORT_GLOBAL) {
        const wasm_reftype_t* ref_type = wasm_global_reftype(module, index);
        wasm_global_import_t import_desc;

        memset(&import_desc, 0, sizeof(import_desc));
        import_desc.module = module_name;
        import_desc.name = export_name;
        import_desc.type = wasm_global_type(module, index);
        if (ref_type) import_desc.ref_type = *ref_type;
        import_desc.is_mutable = wasm_global_is_mutable(module, index);
        import_desc.value = wasm_global_value_ref_at(module, index);
        err = wasm_register_global_import(&harness->runtime, &import_desc);
    } else if (kind == WASM_EXPORT_TABLE) {
        wasm_table_import_t import_desc;

        memset(&import_desc, 0, sizeof(import_desc));
        import_desc.module = module_name;
        import_desc.name = export_name;
        import_desc.table = wasm_table_ref_at(module, index);
        err = wasm_register_table_import(&harness->runtime, &import_desc);
    } else if (kind == WASM_EXPORT_MEM) {
        wasm_memory_import_t import_desc;

        memset(&import_desc, 0, sizeof(import_desc));
        import_desc.module = module_name;
        import_desc.name = export_name;
        import_desc.memory = wasm_memory_ref_at(module, index);
        err = wasm_register_memory_import(&harness->runtime, &import_desc);
    } else if (kind == WASM_EXPORT_TAG) {
        const wasm_functype_t* type = wasm_tag_functype(module, index);
        wasm_tag_import_t import_desc;

        if (!type) {
            spec_set_error(error_text, error_size, "missing tag type for export '%s'", export_name);
            return 0;
        }
        memset(&import_desc, 0, sizeof(import_desc));
        import_desc.module = module_name;
        import_desc.name = export_name;
        import_desc.type = *type;
        err = wasm_register_tag_import(&harness->runtime, &import_desc);
    } else {
        spec_set_error(error_text, error_size, "unsupported export kind for '%s'", export_name);
        return 0;
    }

    if (err != WASM_OK) {
        spec_set_error(error_text, error_size, "%s", spec_runtime_error_text(&harness->runtime, err));
        return 0;
    }

    return 1;
}

static int spec_register_module_exports(spec_harness_t* harness,
                                        const char* module_name,
                                        wasm_module_t* module,
                                        char* error_text,
                                        size_t error_size) {
    uint32_t i;

    for (i = 0; i < wasm_export_count(module); i++) {
    const uint8_t* export_name_bytes;
    size_t export_name_len = 0u;
        const char* export_name = wasm_export_name(module, i);
        wasm_export_kind_t kind = wasm_export_kind(module, i);
        uint32_t index = wasm_export_index(module, i);

    export_name_bytes = wasm_export_name_bytes(module, i, &export_name_len);
    if (export_name_bytes && memchr(export_name_bytes, '\0', export_name_len) != NULL) continue;

        if (!spec_bind_export(harness, module_name, module, export_name, kind, index,
                              error_text, error_size))
            return 0;
    }

    return 1;
}

static wasm_module_t* spec_load_module_bytes(spec_harness_t* harness,
                                             const char* path,
                                             const unsigned char* bytes,
                                             size_t size,
                                             const char* name,
                                             int update_last,
                                             char* error_text,
                                             size_t error_size) {
    spec_runtime_clear_error(&harness->runtime);
    wasm_module_t* module = wasm_load(&harness->runtime, bytes, size);

    if (!module) {
        spec_set_error(error_text, error_size, "%s: %s", path,
                       spec_runtime_error_text(&harness->runtime, harness->runtime.last_error));
        return NULL;
    }
    if (!spec_harness_add_owned_module(harness, module)) {
        wasm_free_module(module);
        spec_set_error(error_text, error_size, "out of memory storing loaded module");
        return NULL;
    }
    if (name && *name && !spec_harness_set_module_name(harness, name, module)) {
        spec_set_error(error_text, error_size, "out of memory storing module name");
        return NULL;
    }
    if (update_last) harness->last_module = module;
    return module;
}

static wasm_module_t* spec_load_module_file(spec_harness_t* harness,
                                            const char* path,
                                            const char* name,
                                            int update_last,
                                            char* error_text,
                                            size_t error_size) {
    unsigned char* bytes;
    size_t size;
    wasm_module_t* module;

    bytes = spec_read_file_bytes(path, &size);
    if (!bytes) {
        spec_set_error(error_text, error_size, "failed to read %s", path);
        return NULL;
    }
    module = spec_load_module_bytes(harness, path, bytes, size, name, update_last, error_text, error_size);
    free(bytes);
    return module;
}

static int spec_execute_action(spec_harness_t* harness,
                               const char* json,
                               const spec_json_token_t* tokens,
                               int action_index,
                               spec_action_result_t* out_result) {
    char* action_type;
    int module_index;
    int field_index;
    int args_index;
    char* module_name = NULL;
    char* field_name = NULL;
    size_t field_name_len = 0u;
    wasm_module_t* module;

    memset(out_result, 0, sizeof(*out_result));

    action_type = spec_strdup_token(json, &tokens[spec_object_get(json, tokens, action_index, "type")]);
    if (!action_type) {
        spec_set_error(out_result->error_text, sizeof(out_result->error_text), "missing action type");
        out_result->err = WASM_ERR_MALFORMED;
        return 0;
    }

    module_index = spec_object_get(json, tokens, action_index, "module");
    field_index = spec_object_get(json, tokens, action_index, "field");
    args_index = spec_object_get(json, tokens, action_index, "args");
    if (module_index >= 0) module_name = spec_strdup_token(json, &tokens[module_index]);
    if (field_index >= 0) field_name = spec_strdup_token_len(json, &tokens[field_index], &field_name_len);
    module = spec_harness_find_module(harness, module_name);
    if (!module) {
        spec_set_error(out_result->error_text, sizeof(out_result->error_text),
                       "unknown module reference %s", module_name ? module_name : "<last>");
        out_result->err = WASM_ERR_UNKNOWN_IMPORT;
        free(action_type);
        free(module_name);
        free(field_name);
        return 0;
    }

    if (strcmp(action_type, "invoke") == 0) {
        wasm_export_kind_t kind;
        uint32_t func_index;
        uint32_t num_args = 0u;
        uint32_t num_results = 0u;
        wasm_value_t* args = NULL;
        wasm_value_t* results = NULL;
        int i;

        if (!field_name ||
            !wasm_find_export_bytes(module, (const uint8_t*)field_name, field_name_len,
                                    &kind, &func_index) ||
            kind != WASM_EXPORT_FUNC) {
            spec_set_error(out_result->error_text, sizeof(out_result->error_text),
                           "%s", "function export not found");
            out_result->err = WASM_ERR_UNDEFINED_EXPORT;
            free(action_type);
            free(module_name);
            free(field_name);
            return 0;
        }

        if (args_index >= 0) {
            char parse_error[256];
            num_args = (uint32_t)tokens[args_index].size;
            if (num_args > 0u) args = (wasm_value_t*)calloc(num_args, sizeof(*args));
            if (num_args > 0u && !args) {
                out_result->err = WASM_ERR_OOM;
                spec_set_error(out_result->error_text, sizeof(out_result->error_text), "out of memory allocating call arguments");
                free(action_type);
                free(module_name);
                free(field_name);
                return 0;
            }
            for (i = 0; i < (int)num_args; i++) {
                spec_value_t parsed;
                int arg_value_index = spec_array_at(tokens, args_index, i);

                if (!spec_parse_value(json, tokens, arg_value_index, &parsed, parse_error, sizeof(parse_error))) {
                    out_result->err = WASM_ERR_MALFORMED;
                    spec_set_error(out_result->error_text, sizeof(out_result->error_text), "%s", parse_error);
                    free(args);
                    free(action_type);
                    free(module_name);
                    free(field_name);
                    return 0;
                }
                args[i] = parsed.value;
            }
        }

        num_results = wasm_func_result_count(module, func_index);
        if (num_results > 0u) results = (wasm_value_t*)calloc(num_results, sizeof(*results));
        if (num_results > 0u && !results) {
            out_result->err = WASM_ERR_OOM;
            spec_set_error(out_result->error_text, sizeof(out_result->error_text), "out of memory allocating call results");
            free(args);
            free(action_type);
            free(module_name);
            free(field_name);
            return 0;
        }

        spec_runtime_clear_error(&harness->runtime);
        out_result->err = wasm_call_index(module, func_index, args, num_args, results, num_results);
        if (out_result->err != WASM_OK) {
            spec_set_error(out_result->error_text, sizeof(out_result->error_text), "%s",
                           spec_runtime_error_text(&harness->runtime, out_result->err));
            free(results);
            results = NULL;
            num_results = 0u;
        }
        out_result->results = results;
        out_result->num_results = num_results;
        free(args);
    } else if (strcmp(action_type, "get") == 0) {
        wasm_value_t* results = (wasm_value_t*)calloc(1u, sizeof(*results));
        if (!results) {
            out_result->err = WASM_ERR_OOM;
            spec_set_error(out_result->error_text, sizeof(out_result->error_text), "out of memory allocating global result");
            free(action_type);
            free(module_name);
            free(field_name);
            return 0;
        }
        spec_runtime_clear_error(&harness->runtime);
        out_result->err = wasm_global_get(module, field_name, &results[0]);
        if (out_result->err != WASM_OK) {
            spec_set_error(out_result->error_text, sizeof(out_result->error_text), "%s",
                           spec_runtime_error_text(&harness->runtime, out_result->err));
            free(results);
            results = NULL;
        } else {
            out_result->results = results;
            out_result->num_results = 1u;
        }
    } else {
        spec_set_error(out_result->error_text, sizeof(out_result->error_text), "unsupported action type '%s'", action_type);
        out_result->err = WASM_ERR_MALFORMED;
        free(action_type);
        free(module_name);
        free(field_name);
        return 0;
    }

    free(action_type);
    free(module_name);
    free(field_name);
    return out_result->err == WASM_OK;
}

static int spec_run_negative_module(spec_harness_t* harness,
                                    const char* json,
                                    const spec_json_token_t* tokens,
                                    int command_index,
                                    const char* expected_text,
                                    int expect_text_compile_failure,
                                    char* error_text,
                                    size_t error_size) {
    char* file_name = NULL;
    char* module_type = NULL;
    char* path = NULL;
    int file_index = spec_object_get(json, tokens, command_index, "filename");
    int module_type_index = spec_object_get(json, tokens, command_index, "module_type");

    if (file_index < 0) {
        spec_set_error(error_text, error_size, "missing negative test filename");
        return 0;
    }
    file_name = spec_strdup_token(json, &tokens[file_index]);
    module_type = module_type_index >= 0 ? spec_strdup_token(json, &tokens[module_type_index]) : NULL;
    path = spec_path_join(harness->json_dir, file_name);
    if (!path) {
        spec_set_error(error_text, error_size, "out of memory resolving module path");
        free(file_name);
        free(module_type);
        return 0;
    }

    if (module_type && strcmp(module_type, "text") == 0) {
        char* wasm_path = (char*)malloc(strlen(path) + 6u);
        char* compiler_output = NULL;
        char* validator_output = NULL;
        int compiler_exit = -1;
        int validator_exit = -1;
        wasm_module_t* module = NULL;

        if (!wasm_path) {
            spec_set_error(error_text, error_size, "out of memory preparing temporary wasm path");
            free(path);
            free(file_name);
            free(module_type);
            return 0;
        }
        sprintf(wasm_path, "%s.wasm", path);
        if (!spec_compile_wat(harness, path, wasm_path, &compiler_output, &compiler_exit)) {
            spec_set_error(error_text, error_size, "failed to invoke wasm-tools parse");
            free(wasm_path);
            free(path);
            free(file_name);
            free(module_type);
            return 0;
        }

        if (compiler_exit != 0) {
            int matched = spec_message_matches(expected_text, compiler_output);
            if (!matched) {
                spec_set_error(error_text, error_size,
                               "wasm-tools parse failed with '%s' (expected '%s')",
                               compiler_output ? compiler_output : "<none>", expected_text);
            }
            free(compiler_output);
            free(wasm_path);
            free(path);
            free(file_name);
            free(module_type);
            return matched;
        }

        free(compiler_output);
        if (expect_text_compile_failure) {
            int matched;

            if (!spec_validate_wasm(harness, wasm_path, &validator_output, &validator_exit)) {
                spec_set_error(error_text, error_size, "failed to invoke wasm-tools validate");
                free(wasm_path);
                free(path);
                free(file_name);
                free(module_type);
                return 0;
            }

            if (validator_exit != 0) {
                matched = spec_message_matches(expected_text, validator_output);
                if (matched) {
                    free(validator_output);
                    free(wasm_path);
                    free(path);
                    free(file_name);
                    free(module_type);
                    return 1;
                }
            }

            free(validator_output);
        }

        module = spec_load_module_file(harness, wasm_path, NULL, 0, error_text, error_size);
        if (module) {
            spec_set_error(error_text, error_size,
                           expect_text_compile_failure
                               ? "expected wasm-tools parse or module load failure matching '%s' but load succeeded"
                               : "expected module load failure matching '%s' but load succeeded",
                           expected_text);
            free(wasm_path);
            free(path);
            free(file_name);
            free(module_type);
            return 0;
        }

        if (!spec_message_matches(expected_text, error_text)) {
            char actual_text[512];
            snprintf(actual_text, sizeof(actual_text), "%s", error_text);
            spec_set_error(error_text, error_size,
                           "expected load failure matching '%s' but got '%s'",
                           expected_text, actual_text);
            free(wasm_path);
            free(path);
            free(file_name);
            free(module_type);
            return 0;
        }

        free(wasm_path);
    } else {
        char* validator_output = NULL;
        int validator_exit = -1;
        wasm_module_t* module;

        if (expect_text_compile_failure) {
            int matched;

            if (!spec_validate_wasm(harness, path, &validator_output, &validator_exit)) {
                spec_set_error(error_text, error_size, "failed to invoke wasm-tools validate");
                free(path);
                free(file_name);
                free(module_type);
                return 0;
            }

            if (validator_exit != 0) {
                matched = spec_message_matches(expected_text, validator_output);
                if (matched) {
                    free(validator_output);
                    free(path);
                    free(file_name);
                    free(module_type);
                    return 1;
                }
            }

            free(validator_output);
        }

        module = spec_load_module_file(harness, path, NULL, 0, error_text, error_size);
        if (module) {
            spec_set_error(error_text, error_size,
                           "expected module load failure matching '%s' but load succeeded",
                           expected_text);
            free(path);
            free(file_name);
            free(module_type);
            return 0;
        }
        if (!spec_message_matches(expected_text, error_text)) {
            char actual_text[512];
            snprintf(actual_text, sizeof(actual_text), "%s", error_text);
            spec_set_error(error_text, error_size,
                           "expected load failure matching '%s' but got '%s'",
                           expected_text, actual_text);
            free(path);
            free(file_name);
            free(module_type);
            return 0;
        }
    }

    free(path);
    free(file_name);
    free(module_type);
    return 1;
}

static int spec_run_command(spec_harness_t* harness,
                            const char* json,
                            const spec_json_token_t* tokens,
                            int command_index) {
    char* type_text;
    char* text_field = NULL;
    char command_error[512];
    int line_index;
    uint64_t line = 0u;
    int ok = 0;

    line_index = spec_object_get(json, tokens, command_index, "line");
    if (line_index >= 0) spec_parse_u64_token(json, &tokens[line_index], &line);

    type_text = spec_strdup_token(json, &tokens[spec_object_get(json, tokens, command_index, "type")]);
    if (!type_text) {
        fprintf(stderr, "%s:%" PRIu64 ": unable to read command type\n",
                harness->source_filename ? harness->source_filename : harness->json_path,
                line);
        return 0;
    }

    memset(command_error, 0, sizeof(command_error));

    if (strcmp(type_text, "module") == 0) {
        char* file_name = NULL;
        char* module_name = NULL;
        char* module_type = NULL;
        char* binary_file_name = NULL;
        char* path = NULL;
        char* load_path = NULL;
        char* compiler_output = NULL;
        int file_index = spec_object_get(json, tokens, command_index, "filename");
        int name_index = spec_object_get(json, tokens, command_index, "name");
        int module_type_index = spec_object_get(json, tokens, command_index, "module_type");
        int binary_file_index = spec_object_get(json, tokens, command_index, "binary_filename");
        int compiler_exit = -1;

        if (file_index < 0) {
            spec_set_error(command_error, sizeof(command_error), "missing module filename");
            goto done;
        }

        file_name = spec_strdup_token(json, &tokens[file_index]);
        module_name = name_index >= 0 ? spec_strdup_token(json, &tokens[name_index]) : NULL;
        module_type = module_type_index >= 0 ? spec_strdup_token(json, &tokens[module_type_index]) : NULL;
        binary_file_name = binary_file_index >= 0 ? spec_strdup_token(json, &tokens[binary_file_index]) : NULL;
        path = file_name ? spec_path_join(harness->json_dir, file_name) : NULL;
        if (!file_name || !path) {
            spec_set_error(command_error, sizeof(command_error), "out of memory resolving module path");
            free(file_name);
            free(module_name);
            free(module_type);
            free(binary_file_name);
            free(path);
            goto done;
        }

        load_path = path;
        if (module_type && strcmp(module_type, "text") == 0) {
            if (binary_file_name) {
                load_path = spec_path_join(harness->json_dir, binary_file_name);
                if (!load_path) {
                    spec_set_error(command_error, sizeof(command_error), "out of memory resolving text module binary path");
                    free(file_name);
                    free(module_name);
                    free(module_type);
                    free(binary_file_name);
                    free(path);
                    goto done;
                }
            } else {
                load_path = (char*)malloc(strlen(path) + 6u);
                if (!load_path) {
                    spec_set_error(command_error, sizeof(command_error), "out of memory preparing temporary wasm path");
                    free(file_name);
                    free(module_name);
                    free(module_type);
                    free(binary_file_name);
                    free(path);
                    goto done;
                }
                sprintf(load_path, "%s.wasm", path);
                if (!spec_compile_wat(harness, path, load_path, &compiler_output, &compiler_exit)) {
                    spec_set_error(command_error, sizeof(command_error), "failed to invoke wasm-tools parse");
                    free(file_name);
                    free(module_name);
                    free(module_type);
                    free(binary_file_name);
                    free(path);
                    free(load_path);
                    goto done;
                }
                if (compiler_exit != 0) {
                    spec_set_error(command_error, sizeof(command_error),
                                   "wasm-tools parse failed with '%s'",
                                   compiler_output ? compiler_output : "<none>");
                    free(file_name);
                    free(module_name);
                    free(module_type);
                    free(binary_file_name);
                    free(path);
                    free(load_path);
                    free(compiler_output);
                    goto done;
                }
            }
        }

        ok = spec_load_module_file(harness, load_path, module_name, 1, command_error, sizeof(command_error)) != NULL;
        free(compiler_output);
        free(file_name);
        free(module_name);
        free(module_type);
        free(binary_file_name);
        if (load_path != path) free(load_path);
        free(path);
    } else if (strcmp(type_text, "register") == 0) {
        char* alias = NULL;
        char* module_ref = NULL;
        wasm_module_t* module;
        int as_index = spec_object_get(json, tokens, command_index, "as");
        int name_index = spec_object_get(json, tokens, command_index, "name");

        if (as_index < 0) {
            spec_set_error(command_error, sizeof(command_error), "missing register alias");
            goto done;
        }

        alias = spec_strdup_token(json, &tokens[as_index]);
        module_ref = name_index >= 0 ? spec_strdup_token(json, &tokens[name_index]) : NULL;
        if (!alias) {
            spec_set_error(command_error, sizeof(command_error), "out of memory resolving register alias");
            free(module_ref);
            goto done;
        }

        module = spec_harness_find_module(harness, module_ref);
        if (!module) {
            spec_set_error(command_error, sizeof(command_error),
                           "unknown module reference %s", module_ref ? module_ref : "<last>");
            free(alias);
            free(module_ref);
            goto done;
        }

        ok = spec_register_module_exports(harness, alias, module, command_error, sizeof(command_error));
        free(alias);
        free(module_ref);
    } else if (strcmp(type_text, "action") == 0 || strcmp(type_text, "assert_return") == 0) {
        spec_action_result_t result;
        int action_index = spec_object_get(json, tokens, command_index, "action");

        if (action_index < 0) {
            spec_set_error(command_error, sizeof(command_error), "missing action payload");
            goto done;
        }

        if (!spec_execute_action(harness, json, tokens, action_index, &result)) {
            spec_set_error(command_error, sizeof(command_error), "%s", result.error_text);
            spec_action_result_dispose(&result);
            goto done;
        }

        ok = spec_compare_expected(json, tokens, command_index, result.results, result.num_results,
                                   command_error, sizeof(command_error));
        spec_action_result_dispose(&result);
    } else if (strcmp(type_text, "assert_trap") == 0 || strcmp(type_text, "assert_exhaustion") == 0 ||
               strcmp(type_text, "assert_exception") == 0) {
        spec_action_result_t result;
        int action_index = spec_object_get(json, tokens, command_index, "action");
        int text_index = spec_object_get(json, tokens, command_index, "text");

        if (action_index < 0) {
            spec_set_error(command_error, sizeof(command_error), "missing action payload");
            goto done;
        }
        if (text_index >= 0) text_field = spec_strdup_token(json, &tokens[text_index]);

        if (spec_execute_action(harness, json, tokens, action_index, &result)) {
            spec_set_error(command_error, sizeof(command_error),
                           "expected failure matching '%s' but action succeeded",
                           text_field ? text_field : "<any>");
            spec_action_result_dispose(&result);
            goto done;
        }

        ok = spec_message_matches(text_field, result.error_text);
        if (!ok) {
            spec_set_error(command_error, sizeof(command_error),
                           "expected failure matching '%s' but got '%s'",
                           text_field ? text_field : "<any>", result.error_text);
        }
        spec_action_result_dispose(&result);
    } else if (strcmp(type_text, "assert_invalid") == 0 || strcmp(type_text, "assert_malformed") == 0 ||
               strcmp(type_text, "assert_unlinkable") == 0 || strcmp(type_text, "assert_uninstantiable") == 0) {
        int text_index = spec_object_get(json, tokens, command_index, "text");

        if (text_index < 0) {
            spec_set_error(command_error, sizeof(command_error), "missing failure text");
            goto done;
        }

        text_field = spec_strdup_token(json, &tokens[text_index]);
        if (!text_field) {
            spec_set_error(command_error, sizeof(command_error), "out of memory resolving failure text");
            goto done;
        }

        ok = spec_run_negative_module(harness, json, tokens, command_index, text_field,
                                      strcmp(type_text, "assert_malformed") == 0,
                                      command_error, sizeof(command_error));
    } else {
        spec_set_error(command_error, sizeof(command_error), "unsupported command type '%s'", type_text);
    }

done:
    if (!ok) {
        fprintf(stderr, "%s:%" PRIu64 ": %s: %s\n",
                harness->source_filename ? harness->source_filename : harness->json_path,
                line,
                type_text,
                command_error[0] ? command_error : "failed");
    }

    free(text_field);
    free(type_text);
    return ok;
}

static void spec_harness_destroy(spec_harness_t* harness) {
    size_t i;

    if (!harness) return;

    for (i = 0; i < harness->num_named_modules; i++) free(harness->named_modules[i].name);
    for (i = 0; i < harness->num_owned_modules; i++) wasm_free_module(harness->owned_modules[i]);

    free(harness->owned_modules);
    free(harness->named_modules);
    free(harness->forwarded_funcs);
    free(harness->json_dir);
    free((void*)harness->source_filename);

    wasm_destroy(&harness->runtime);
    memset(harness, 0, sizeof(*harness));
}

static int spec_harness_init(spec_harness_t* harness,
                             const char* json_path,
                             const char* wasm_tools_path,
                             const char* support_wasm,
                             char* error_text,
                             size_t error_size) {
    wasm_error_t err;
    wasm_module_t* support_module;

    memset(harness, 0, sizeof(*harness));
    harness->json_path = json_path;
    harness->wasm_tools_path = wasm_tools_path;
    harness->support_wasm = support_wasm;
    harness->json_dir = spec_dirname_dup(json_path);
    if (!harness->json_dir) {
        spec_set_error(error_text, error_size, "out of memory resolving json directory");
        return 0;
    }

    err = wasm_init(&harness->runtime, NULL);
    if (err != WASM_OK) {
        spec_set_error(error_text, error_size, "failed to initialize runtime: %s", wasm_error_string(err));
        return 0;
    }

    wasm_enable_all_features(&harness->runtime);
    if (!spec_json_uses_multi_memory(json_path))
        wasm_disable_feature(&harness->runtime, WASM_FEATURE_MULTI_MEMORY);
    wasm_disable_feature(&harness->runtime, WASM_FEATURE_EXTENDED_CONST);

    err = wasm_bind_wasi_stubs(&harness->runtime);
    if (err != WASM_OK) {
        spec_set_error(error_text, error_size, "failed to bind WASI stubs: %s",
                       spec_runtime_error_text(&harness->runtime, err));
        return 0;
    }

    err = spec_bind_print_imports(harness);
    if (err != WASM_OK) {
        spec_set_error(error_text, error_size, "failed to bind spectest print imports: %s",
                       spec_runtime_error_text(&harness->runtime, err));
        return 0;
    }

    err = spec_bind_default_test_globals(harness);
    if (err != WASM_OK) {
        spec_set_error(error_text, error_size, "failed to bind default test globals: %s",
                       spec_runtime_error_text(&harness->runtime, err));
        return 0;
    }

    support_module = spec_load_module_file(harness, support_wasm, NULL, 0, error_text, error_size);
    if (!support_module) return 0;

    err = spec_bind_default_empty_imports(harness, support_module);
    if (err != WASM_OK) {
        spec_set_error(error_text, error_size, "failed to bind empty-name placeholder imports: %s",
                       spec_runtime_error_text(&harness->runtime, err));
        return 0;
    }

    if (!spec_register_module_exports(harness, "spectest", support_module, error_text, error_size)) return 0;

    return 1;
}

static int spec_run_file(spec_harness_t* harness) {
    char* json_text = NULL;
    spec_json_token_t* tokens = NULL;
    int token_count;
    int commands_index;
    int i;
    int ok = 0;
    size_t json_size = 0u;
    spec_json_parser_t parser;

    json_text = spec_read_file_text(harness->json_path, &json_size);
    if (!json_text) {
        fprintf(stderr, "failed to read %s\n", harness->json_path);
        goto cleanup;
    }

    tokens = (spec_json_token_t*)calloc(json_size + 1u, sizeof(*tokens));
    if (!tokens) {
        fprintf(stderr, "out of memory allocating json tokens\n");
        goto cleanup;
    }

    spec_json_parser_init(&parser);
    token_count = spec_json_parse(&parser, json_text, json_size, tokens, json_size + 1u);
    if (token_count < 0) {
        fprintf(stderr, "%s: failed to parse spectest JSON\n", harness->json_path);
        goto cleanup;
    }
    if (token_count == 0 || tokens[0].type != SPEC_JSON_OBJECT) {
        fprintf(stderr, "%s: spectest JSON root is not an object\n", harness->json_path);
        goto cleanup;
    }

    if (spec_object_get(json_text, tokens, 0, "source_filename") >= 0) {
        int source_index = spec_object_get(json_text, tokens, 0, "source_filename");
        harness->source_filename = spec_strdup_token(json_text, &tokens[source_index]);
    }

    commands_index = spec_object_get(json_text, tokens, 0, "commands");
    if (commands_index < 0 || tokens[commands_index].type != SPEC_JSON_ARRAY) {
        fprintf(stderr, "%s: missing commands array\n", harness->json_path);
        goto cleanup;
    }

    ok = 1;
    for (i = 0; i < tokens[commands_index].size; i++) {
        int command_index = spec_array_at(tokens, commands_index, i);

        if (!spec_run_command(harness, json_text, tokens, command_index)) {
            ok = 0;
            break;
        }
    }

cleanup:
    free(tokens);
    free(json_text);
    return ok;
}

static void spec_print_usage(const char* argv0) {
    fprintf(stderr, "usage: %s <spec.json> <wasm-tools> <spectest_support.wasm> [conversion.status]\n", argv0);
}

int main(int argc, char** argv) {
    spec_harness_t harness;
    char error_text[512];
    int exit_code = 1;

    if (argc != 4 && argc != 5) {
        spec_print_usage(argv[0]);
        return 2;
    }

    if (argc == 5 && spec_status_requests_skip(argv[4], error_text, sizeof(error_text))) {
        fprintf(stderr, "skipped: %s\n", error_text[0] ? error_text : "spectest JSON conversion failed for this spec file");
        return 125;
    }

    memset(&harness, 0, sizeof(harness));
    memset(error_text, 0, sizeof(error_text));

    if (!spec_harness_init(&harness, argv[1], argv[2], argv[3], error_text, sizeof(error_text))) {
        fprintf(stderr, "%s\n", error_text[0] ? error_text : "failed to initialize spectest harness");
        spec_harness_destroy(&harness);
        return 1;
    }

    if (spec_run_file(&harness)) exit_code = 0;

    spec_harness_destroy(&harness);
    return exit_code;
}
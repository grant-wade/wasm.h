#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WASM_IMPL
#include "wasm.h"

typedef struct wasm_cli_options_t {
    int bind_wasi;
    int bind_emscripten;
    int fuel_set;
    uint64_t fuel;
} wasm_cli_options_t;

static void wasm_cli_print_usage(FILE* stream, const char* argv0) {
    fprintf(stream,
            "usage: %s [--wasi] [--emscripten] [--fuel N] <module.wasm> <command> [args...]\n"
            "\n"
            "commands:\n"
            "  info\n"
            "  exports\n"
            "  funcs\n"
            "  types\n"
            "  globals\n"
            "  memories\n"
            "  custom-sections\n"
            "  call <export_name> [args...]\n"
            "  call-index <func_index> [args...]\n"
            "  global-get <export_name>\n"
            "  global-set <export_name> <value>\n"
            "  memory-read <memory_index> <offset> <length>\n"
            "  memory-string <memory_index> <offset> [max_len]\n"
            "\n"
            "argument syntax:\n"
            "  i32/i64: decimal or 0x-prefixed integers\n"
            "  f32/f64: floating-point literals\n"
            "  funcref: integer function index or 'null'\n"
            "  externref: integer address-like value or 'null'\n"
            "  i31ref: signed integer or 'null'\n"
            "  typed refs: 'null' only\n"
            "  v128: hex:00112233445566778899aabbccddeeff\n",
            argv0);
}

static const char* wasm_cli_error_text(const wasm_runtime_t* rt) {
    if (!rt) return "unknown error";
    if (rt->error_msg[0] != '\0') return rt->error_msg;
    return wasm_error_string(rt->last_error);
}

static void wasm_cli_print_backtrace(const wasm_runtime_t* rt) {
    char buffer[1024];

    if (!rt) return;
    buffer[0] = '\0';
    wasm_dump_backtrace((wasm_runtime_t*)rt, buffer, sizeof(buffer));
    if (buffer[0] != '\0') fprintf(stderr, "backtrace:\n%s\n", buffer);
}

static unsigned char* wasm_cli_read_file(const char* path, size_t* out_len) {
    FILE* file;
    long size;
    unsigned char* bytes;

    if (out_len) *out_len = 0u;
    if (!path || !out_len) return NULL;

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

    if ((size_t)size > 0u && fread(bytes, 1u, (size_t)size, file) != (size_t)size) {
        free(bytes);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *out_len = (size_t)size;
    return bytes;
}

static int wasm_cli_parse_u32(const char* text, uint32_t* out_value) {
    unsigned long long value;
    char* end;

    if (!text || !out_value) return 0;

    errno = 0;
    value = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX) return 0;

    *out_value = (uint32_t)value;
    return 1;
}

static int wasm_cli_parse_size(const char* text, size_t* out_value) {
    unsigned long long value;
    char* end;

    if (!text || !out_value) return 0;

    errno = 0;
    value = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') return 0;
    if (value > (unsigned long long)SIZE_MAX) return 0;

    *out_value = (size_t)value;
    return 1;
}

static int wasm_cli_parse_u64(const char* text, uint64_t* out_value) {
    unsigned long long value;
    char* end;

    if (!text || !out_value) return 0;

    errno = 0;
    value = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') return 0;

    *out_value = (uint64_t)value;
    return 1;
}

static int wasm_cli_parse_i32(const char* text, int32_t* out_value) {
    long long value;
    char* end;

    if (!text || !out_value) return 0;

    errno = 0;
    value = strtoll(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') return 0;
    if (value < INT32_MIN || value > INT32_MAX) return 0;

    *out_value = (int32_t)value;
    return 1;
}

static int wasm_cli_parse_i64(const char* text, int64_t* out_value) {
    long long value;
    char* end;

    if (!text || !out_value) return 0;

    errno = 0;
    value = strtoll(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') return 0;

    *out_value = (int64_t)value;
    return 1;
}

static int wasm_cli_parse_f32(const char* text, float* out_value) {
    double value;
    char* end;

    if (!text || !out_value) return 0;

    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') return 0;

    *out_value = (float)value;
    return 1;
}

static int wasm_cli_parse_f64(const char* text, double* out_value) {
    double value;
    char* end;

    if (!text || !out_value) return 0;

    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0') return 0;

    *out_value = value;
    return 1;
}

static int wasm_cli_parse_ptr(const char* text, uintptr_t* out_value) {
    unsigned long long value;
    char* end;

    if (!text || !out_value) return 0;

    errno = 0;
    value = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0') return 0;
    if (value > (unsigned long long)UINTPTR_MAX) return 0;

    *out_value = (uintptr_t)value;
    return 1;
}

static int wasm_cli_hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

static int wasm_cli_parse_v128(const char* text, uint8_t out_bytes[16]) {
    const char* hex;
    size_t i;

    if (!text || !out_bytes) return 0;

    hex = text;
    if (strncmp(hex, "hex:", 4) == 0) hex += 4;
    if (strlen(hex) != 32u) return 0;

    for (i = 0; i < 16u; i++) {
        int hi = wasm_cli_hex_digit(hex[i * 2u]);
        int lo = wasm_cli_hex_digit(hex[i * 2u + 1u]);

        if (hi < 0 || lo < 0) return 0;
        out_bytes[i] = (uint8_t)((hi << 4) | lo);
    }

    return 1;
}

static int wasm_cli_is_null_ref(const wasm_value_t* value) {
    if (!value) return 1;
    if (wasm__uses_funcref_storage(value->type)) return value->of.funcref == UINT32_MAX;
    if (wasm__uses_externref_storage(value->type)) return value->of.externref == (uintptr_t)0;
    if (wasm__uses_gc_ref_storage(value->type)) return value->of.gc_ref == (uintptr_t)0;
    return 0;
}

static const char* wasm_cli_valtype_name(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_I32:
            return "i32";
        case WASM_TYPE_I64:
            return "i64";
        case WASM_TYPE_F32:
            return "f32";
        case WASM_TYPE_F64:
            return "f64";
        case WASM_TYPE_V128:
            return "v128";
        case WASM_TYPE_FUNCREF:
            return "funcref";
        case WASM_TYPE_EXTERNREF:
            return "externref";
        case WASM_TYPE_ANYREF:
            return "anyref";
        case WASM_TYPE_EQREF:
            return "eqref";
        case WASM_TYPE_I31REF:
            return "i31ref";
        case WASM_TYPE_STRUCTREF:
            return "structref";
        case WASM_TYPE_ARRAYREF:
            return "arrayref";
        case WASM_TYPE_NOFUNC:
            return "nofunc";
        case WASM_TYPE_NOEXTERN:
            return "noextern";
        case WASM_TYPE_NONE:
            return "none";
        case WASM_TYPE_VOID:
            return "void";
        default:
            return "unknown";
    }
}

static const char* wasm_cli_export_kind_name(wasm_export_kind_t kind) {
    switch (kind) {
        case WASM_EXPORT_FUNC:
            return "func";
        case WASM_EXPORT_TABLE:
            return "table";
        case WASM_EXPORT_MEM:
            return "memory";
        case WASM_EXPORT_GLOBAL:
            return "global";
        case WASM_EXPORT_TAG:
            return "tag";
        default:
            return "unknown";
    }
}

static const char* wasm_cli_comptype_kind_name(wasm_comptype_kind_t kind) {
    switch (kind) {
        case WASM_COMP_FUNC:
            return "func";
        case WASM_COMP_STRUCT:
            return "struct";
        case WASM_COMP_ARRAY:
            return "array";
        default:
            return "invalid";
    }
}

static void wasm_cli_print_reftype_suffix(FILE* stream, const wasm_reftype_t* ref_type) {
    if (!stream || !ref_type) return;
    if (!wasm__uses_funcref_storage(ref_type->type) &&
        !wasm__uses_externref_storage(ref_type->type) &&
        !wasm__uses_gc_ref_storage(ref_type->type))
        return;
    if (ref_type->has_type_index)
        fprintf(stream, "<type=%u%s>", (unsigned)ref_type->type_index,
                ref_type->nullable ? ", nullable" : "");
    else if (!ref_type->nullable)
        fprintf(stream, "<non-null>");
}

static void wasm_cli_print_value_type(FILE* stream, wasm_valtype_t type, const wasm_reftype_t* ref_type) {
    fprintf(stream, "%s", wasm_cli_valtype_name(type));
    if (ref_type) wasm_cli_print_reftype_suffix(stream, ref_type);
}

static void wasm_cli_print_storage_type(FILE* stream, const wasm_storagetype_t* storage) {
    if (!stream || !storage) return;

    if (storage->kind == WASM_STORAGE_PACKED) {
        fprintf(stream, "%s", storage->of.packed_type == WASM_PACKED_I8 ? "i8" : "i16");
        return;
    }

    wasm_cli_print_value_type(stream, storage->of.valtype, &storage->ref_type);
}

static void wasm_cli_print_field_type(FILE* stream, const wasm_fieldtype_t* field) {
    if (!field) return;
    wasm_cli_print_storage_type(stream, &field->storage);
    fprintf(stream, "%s", field->is_mutable ? " mut" : " const");
}

static void wasm_cli_print_value(FILE* stream, const wasm_value_t* value) {
    size_t i;

    if (!stream || !value) return;

    switch (value->type) {
        case WASM_TYPE_I32:
            fprintf(stream, "%" PRId32, value->of.i32);
            break;
        case WASM_TYPE_I64:
            fprintf(stream, "%" PRId64, value->of.i64);
            break;
        case WASM_TYPE_F32:
            fprintf(stream, "%.9g", (double)value->of.f32);
            break;
        case WASM_TYPE_F64:
            fprintf(stream, "%.17g", value->of.f64);
            break;
        case WASM_TYPE_V128:
            fputs("0x", stream);
            for (i = 0; i < 16u; i++) fprintf(stream, "%02x", (unsigned)value->of.v128[i]);
            break;
        case WASM_TYPE_FUNCREF:
            if (wasm_cli_is_null_ref(value))
                fputs("null", stream);
            else
                fprintf(stream, "func[%u]", (unsigned)value->of.funcref);
            break;
        case WASM_TYPE_EXTERNREF:
        case WASM_TYPE_NOEXTERN:
            if (wasm_cli_is_null_ref(value))
                fputs("null", stream);
            else
                fprintf(stream, "0x%" PRIxPTR, value->of.externref);
            break;
        case WASM_TYPE_I31REF:
            if (wasm_cli_is_null_ref(value))
                fputs("null", stream);
            else
                fprintf(stream, "i31(0x%" PRIxPTR ")", value->of.gc_ref);
            break;
        case WASM_TYPE_NOFUNC:
            if (wasm_cli_is_null_ref(value))
                fputs("null", stream);
            else
                fprintf(stream, "func[%u]", (unsigned)value->of.funcref);
            break;
        default:
            if (wasm__uses_gc_ref_storage(value->type)) {
                if (wasm_cli_is_null_ref(value))
                    fputs("null", stream);
                else
                    fprintf(stream, "ref(0x%" PRIxPTR ")", value->of.gc_ref);
            } else {
                fputs("<unsupported>", stream);
            }
            break;
    }
}

static int wasm_cli_parse_value(const char* text, wasm_valtype_t type,
                                const wasm_reftype_t* ref_type, wasm_value_t* out_value,
                                char* error_buf, size_t error_buf_size) {
    int32_t i32_value;
    int64_t i64_value;
    float f32_value;
    double f64_value;
    uintptr_t ptr_value;
    uint32_t func_index;
    uint8_t v128_bytes[16];

    (void)ref_type;

    if (!text || !out_value) return 0;

    switch (type) {
        case WASM_TYPE_I32:
            if (!wasm_cli_parse_i32(text, &i32_value)) break;
            *out_value = wasm_i32(i32_value);
            return 1;
        case WASM_TYPE_I64:
            if (!wasm_cli_parse_i64(text, &i64_value)) break;
            *out_value = wasm_i64(i64_value);
            return 1;
        case WASM_TYPE_F32:
            if (!wasm_cli_parse_f32(text, &f32_value)) break;
            *out_value = wasm_f32(f32_value);
            return 1;
        case WASM_TYPE_F64:
            if (!wasm_cli_parse_f64(text, &f64_value)) break;
            *out_value = wasm_f64(f64_value);
            return 1;
        case WASM_TYPE_V128:
            if (!wasm_cli_parse_v128(text, v128_bytes)) break;
            *out_value = wasm_v128(v128_bytes);
            return 1;
        case WASM_TYPE_FUNCREF:
        case WASM_TYPE_NOFUNC:
            if (strcmp(text, "null") == 0) {
                *out_value = wasm_ref_null(type);
                return 1;
            }
            if (!wasm_cli_parse_u32(text, &func_index)) break;
            *out_value = wasm_funcref(func_index);
            out_value->type = type;
            return 1;
        case WASM_TYPE_EXTERNREF:
        case WASM_TYPE_NOEXTERN:
            if (strcmp(text, "null") == 0) {
                *out_value = wasm_ref_null(type);
                return 1;
            }
            if (!wasm_cli_parse_ptr(text, &ptr_value)) break;
            *out_value = wasm_externref(ptr_value);
            out_value->type = type;
            return 1;
        case WASM_TYPE_I31REF:
            if (strcmp(text, "null") == 0) {
                *out_value = wasm_ref_null(type);
                return 1;
            }
            if (!wasm_cli_parse_i32(text, &i32_value)) break;
            *out_value = wasm_i31ref(i32_value);
            return 1;
        default:
            if (wasm__uses_gc_ref_storage(type) && strcmp(text, "null") == 0) {
                *out_value = wasm_ref_null(type);
                return 1;
            }
            break;
    }

    if (error_buf && error_buf_size > 0u) {
        snprintf(error_buf, error_buf_size,
                 "could not parse '%s' as %s", text, wasm_cli_valtype_name(type));
    }
    return 0;
}

static uint32_t wasm_cli_count_exports_of_kind(wasm_module_t* mod, wasm_export_kind_t kind) {
    uint32_t count = 0u;
    uint32_t i;

    for (i = 0; i < wasm_export_count(mod); i++) {
        if (wasm_export_kind(mod, i) == kind) count++;
    }
    return count;
}

static const char* wasm_cli_first_export_name(wasm_module_t* mod, wasm_export_kind_t kind, uint32_t target_index) {
    uint32_t i;

    for (i = 0; i < wasm_export_count(mod); i++) {
        if (wasm_export_kind(mod, i) == kind && wasm_export_index(mod, i) == target_index)
            return wasm_export_name(mod, i);
    }

    return NULL;
}

static void wasm_cli_print_func_signature(FILE* stream, wasm_module_t* mod, uint32_t func_idx) {
    const wasm_functype_t* ft;
    uint32_t i;

    if (!stream || !mod || func_idx >= mod->num_funcs) return;

    ft = wasm_type_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) {
        fputs("<invalid>", stream);
        return;
    }

    fputc('(', stream);
    for (i = 0; i < ft->num_params; i++) {
        if (i != 0u) fputs(", ", stream);
        wasm_cli_print_value_type(stream, ft->params[i], &ft->param_reftypes[i]);
    }
    fputc(')', stream);
    fputs(" -> (", stream);
    for (i = 0; i < ft->num_results; i++) {
        if (i != 0u) fputs(", ", stream);
        wasm_cli_print_value_type(stream, ft->results[i], &ft->result_reftypes[i]);
    }
    fputc(')', stream);
}

static void wasm_cli_print_summary(wasm_module_t* mod, const char* module_path) {
    printf("module: %s\n", module_path);
    printf("types: %u\n", (unsigned)wasm_type_count(mod));
    printf("funcs: %u (%u exports)\n", (unsigned)wasm_func_count(mod),
           (unsigned)wasm_cli_count_exports_of_kind(mod, WASM_EXPORT_FUNC));
    printf("globals: %u (%u exports)\n", (unsigned)wasm_global_count(mod),
           (unsigned)wasm_cli_count_exports_of_kind(mod, WASM_EXPORT_GLOBAL));
    printf("tables: %u (%u exports)\n", (unsigned)wasm_table_count(mod),
           (unsigned)wasm_cli_count_exports_of_kind(mod, WASM_EXPORT_TABLE));
    printf("memories: %u (%u exports)\n", (unsigned)wasm_memory_count(mod),
           (unsigned)wasm_cli_count_exports_of_kind(mod, WASM_EXPORT_MEM));
    printf("custom sections: %u\n", (unsigned)wasm_custom_section_count(mod));
    printf("required features: 0x%08x\n", (unsigned)mod->required_features);
}

static void wasm_cli_print_exports(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < wasm_export_count(mod); i++) {
        wasm_export_kind_t kind = wasm_export_kind(mod, i);
        uint32_t target = wasm_export_index(mod, i);

        printf("[%u] %s kind=%s index=%u",
               (unsigned)i,
               wasm_export_name(mod, i),
               wasm_cli_export_kind_name(kind),
               (unsigned)target);
        if (kind == WASM_EXPORT_FUNC) {
            fputc(' ', stdout);
            wasm_cli_print_func_signature(stdout, mod, target);
        }
        fputc('\n', stdout);
    }
}

static void wasm_cli_print_funcs(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < wasm_func_count(mod); i++) {
        const char* export_name = wasm_cli_first_export_name(mod, WASM_EXPORT_FUNC, i);

        printf("[%u] %s", (unsigned)i, mod->funcs[i].is_import ? "import" : "local");
        if (export_name) printf(" export=%s", export_name);
        fputc(' ', stdout);
        wasm_cli_print_func_signature(stdout, mod, i);
        fputc('\n', stdout);
    }
}

static void wasm_cli_print_types(wasm_module_t* mod) {
    uint32_t type_idx;

    for (type_idx = 0; type_idx < wasm_type_count(mod); type_idx++) {
        wasm_comptype_kind_t kind = wasm_type_kind(mod, type_idx);
        uint32_t field_idx;

        printf("[%u] %s", (unsigned)type_idx, wasm_cli_comptype_kind_name(kind));
        if (kind == WASM_COMP_FUNC) {
            const wasm_functype_t* ft = wasm_type_functype(mod, type_idx);
            uint32_t i;

            fputc(' ', stdout);
            fputc('(', stdout);
            if (ft) {
                for (i = 0; i < ft->num_params; i++) {
                    if (i != 0u) fputs(", ", stdout);
                    wasm_cli_print_value_type(stdout, ft->params[i], &ft->param_reftypes[i]);
                }
            } else {
                fputs("<invalid>", stdout);
            }
            fputs(") -> (", stdout);
            if (ft) {
                for (i = 0; i < ft->num_results; i++) {
                    if (i != 0u) fputs(", ", stdout);
                    wasm_cli_print_value_type(stdout, ft->results[i], &ft->result_reftypes[i]);
                }
            }
            fputc(')', stdout);
            fputc('\n', stdout);
            continue;
        }
        if (kind == WASM_COMP_STRUCT) {
            printf(" fields=%u\n", (unsigned)wasm_struct_type_field_count(mod, type_idx));
            for (field_idx = 0; field_idx < wasm_struct_type_field_count(mod, type_idx); field_idx++) {
                const wasm_fieldtype_t* field = wasm_struct_type_field(mod, type_idx, field_idx);

                printf("  .field[%u] ", (unsigned)field_idx);
                wasm_cli_print_field_type(stdout, field);
                fputc('\n', stdout);
            }
            continue;
        }
        if (kind == WASM_COMP_ARRAY) {
            const wasm_fieldtype_t* field = wasm_array_type_field(mod, type_idx);

            fputs(" element=", stdout);
            wasm_cli_print_field_type(stdout, field);
            fputc('\n', stdout);
            continue;
        }

        fputc('\n', stdout);
    }
}

static wasm_value_t wasm_cli_global_value(const wasm_global_t* global) {
    if (global && global->is_import && global->import_value) return *global->import_value;
    if (global) return global->value;
    return wasm_i32(0);
}

static void wasm_cli_print_globals(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < wasm_global_count(mod); i++) {
        const char* export_name = wasm_cli_first_export_name(mod, WASM_EXPORT_GLOBAL, i);
        wasm_value_t value = wasm_cli_global_value(&mod->globals[i]);

        printf("[%u] %s ", (unsigned)i, wasm_global_is_mutable(mod, i) ? "var" : "const");
        wasm_cli_print_value_type(stdout, wasm_global_type(mod, i), wasm_global_reftype(mod, i));
        if (export_name) printf(" export=%s", export_name);
        fputs(" value=", stdout);
        wasm_cli_print_value(stdout, &value);
        fputc('\n', stdout);
    }
}

static void wasm_cli_print_memories(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < wasm_memory_count(mod); i++) {
        const char* export_name = wasm_cli_first_export_name(mod, WASM_EXPORT_MEM, i);
        uint64_t size_bytes = wasm_memory_size_at(mod, i);

        printf("[%u] pages=%llu bytes=%llu max_pages=%llu%s",
               (unsigned)i,
               (unsigned long long)mod->memories[i].pages,
               (unsigned long long)size_bytes,
               (unsigned long long)mod->memories[i].max_pages,
               mod->memories[i].is_64 ? " type=i64" : " type=i32");
        if (export_name) printf(" export=%s", export_name);
        fputc('\n', stdout);
    }
}

static void wasm_cli_print_custom_sections(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < wasm_custom_section_count(mod); i++) {
        size_t size = 0u;

        (void)wasm_custom_section_data(mod, i, &size);
        printf("[%u] %s size=%zu\n", (unsigned)i, wasm_custom_section_name(mod, i), size);
    }
}

static int wasm_cli_print_results(const wasm_value_t* results, uint32_t num_results) {
    uint32_t i;

    if (num_results == 0u) {
        puts("ok");
        return 0;
    }

    for (i = 0; i < num_results; i++) {
        printf("result[%u] ", (unsigned)i);
        wasm_cli_print_value_type(stdout, results[i].type, NULL);
        fputs(" = ", stdout);
        wasm_cli_print_value(stdout, &results[i]);
        fputc('\n', stdout);
    }

    return 0;
}

static int wasm_cli_call_export(wasm_module_t* mod, const char* export_name,
                                int argc, char** argv) {
    wasm_export_kind_t kind;
    uint32_t func_idx;
    const wasm_functype_t* ft;
    wasm_value_t* args = NULL;
    wasm_value_t* results = NULL;
    wasm_error_t err;
    uint32_t i;
    int exit_code = 1;
    char error_buf[128];

    if (!wasm_find_export(mod, export_name, &kind, &func_idx) || kind != WASM_EXPORT_FUNC) {
        fprintf(stderr, "function export '%s' not found\n", export_name);
        return 1;
    }

    ft = wasm_type_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) {
        fprintf(stderr, "function export '%s' has an invalid type\n", export_name);
        return 1;
    }
    if ((uint32_t)argc != ft->num_params) {
        fprintf(stderr, "function '%s' expects %u args, got %d\n",
                export_name, (unsigned)ft->num_params, argc);
        fprintf(stderr, "signature: ");
        wasm_cli_print_func_signature(stderr, mod, func_idx);
        fputc('\n', stderr);
        return 1;
    }

    if (ft->num_params > 0u) {
        args = (wasm_value_t*)calloc(ft->num_params, sizeof(*args));
        if (!args) {
            fprintf(stderr, "out of memory allocating call args\n");
            return 1;
        }
    }
    if (ft->num_results > 0u) {
        results = (wasm_value_t*)calloc(ft->num_results, sizeof(*results));
        if (!results) {
            fprintf(stderr, "out of memory allocating call results\n");
            free(args);
            return 1;
        }
    }

    for (i = 0; i < ft->num_params; i++) {
        if (!wasm_cli_parse_value(argv[i], ft->params[i], &ft->param_reftypes[i], &args[i], error_buf,
                                  sizeof(error_buf))) {
            fprintf(stderr, "%s\n", error_buf);
            fprintf(stderr, "argument %u type: ", (unsigned)i);
            wasm_cli_print_value_type(stderr, ft->params[i], &ft->param_reftypes[i]);
            fputc('\n', stderr);
            goto cleanup;
        }
    }

    err = wasm_call_index(mod, func_idx, args, ft->num_params, results, ft->num_results);
    if (err != WASM_OK) {
        fprintf(stderr, "call failed: %s\n", wasm_cli_error_text(mod->rt));
        wasm_cli_print_backtrace(mod->rt);
        goto cleanup;
    }

    exit_code = wasm_cli_print_results(results, ft->num_results);

cleanup:
    free(args);
    free(results);
    return exit_code;
}

static int wasm_cli_call_index(wasm_module_t* mod, const char* func_index_text,
                               int argc, char** argv) {
    uint32_t func_idx;
    const char* export_name;

    if (!wasm_cli_parse_u32(func_index_text, &func_idx)) {
        fprintf(stderr, "invalid function index '%s'\n", func_index_text);
        return 1;
    }
    if (func_idx >= wasm_func_count(mod)) {
        fprintf(stderr, "function index %u out of range\n", (unsigned)func_idx);
        return 1;
    }

    export_name = wasm_cli_first_export_name(mod, WASM_EXPORT_FUNC, func_idx);
    if (export_name) return wasm_cli_call_export(mod, export_name, argc, argv);

    {
        const wasm_functype_t* ft = wasm_type_functype(mod, mod->funcs[func_idx].type_idx);
        wasm_value_t* args = NULL;
        wasm_value_t* results = NULL;
        wasm_error_t err;
        uint32_t i;
        char error_buf[128];
        int exit_code = 1;

        if (!ft) {
            fprintf(stderr, "function index %u has an invalid type\n", (unsigned)func_idx);
            return 1;
        }
        if ((uint32_t)argc != ft->num_params) {
            fprintf(stderr, "function %u expects %u args, got %d\n",
                    (unsigned)func_idx, (unsigned)ft->num_params, argc);
            fprintf(stderr, "signature: ");
            wasm_cli_print_func_signature(stderr, mod, func_idx);
            fputc('\n', stderr);
            return 1;
        }

        if (ft->num_params > 0u) {
            args = (wasm_value_t*)calloc(ft->num_params, sizeof(*args));
            if (!args) {
                fprintf(stderr, "out of memory allocating call args\n");
                return 1;
            }
        }
        if (ft->num_results > 0u) {
            results = (wasm_value_t*)calloc(ft->num_results, sizeof(*results));
            if (!results) {
                fprintf(stderr, "out of memory allocating call results\n");
                free(args);
                return 1;
            }
        }

        for (i = 0; i < ft->num_params; i++) {
            if (!wasm_cli_parse_value(argv[i], ft->params[i], &ft->param_reftypes[i], &args[i],
                                      error_buf, sizeof(error_buf))) {
                fprintf(stderr, "%s\n", error_buf);
                goto cleanup;
            }
        }

        err = wasm_call_index(mod, func_idx, args, ft->num_params, results, ft->num_results);
        if (err != WASM_OK) {
            fprintf(stderr, "call failed: %s\n", wasm_cli_error_text(mod->rt));
            wasm_cli_print_backtrace(mod->rt);
            goto cleanup;
        }

        exit_code = wasm_cli_print_results(results, ft->num_results);

    cleanup:
        free(args);
        free(results);
        return exit_code;
    }
}

static int wasm_cli_global_get_cmd(wasm_module_t* mod, const char* export_name) {
    wasm_value_t value;
    wasm_error_t err;

    err = wasm_global_get(mod, export_name, &value);
    if (err != WASM_OK) {
        fprintf(stderr, "global-get failed: %s\n", wasm_cli_error_text(mod->rt));
        return 1;
    }

    wasm_cli_print_value_type(stdout, value.type, NULL);
    fputs(" = ", stdout);
    wasm_cli_print_value(stdout, &value);
    fputc('\n', stdout);
    return 0;
}

static int wasm_cli_global_set_cmd(wasm_module_t* mod, const char* export_name, const char* value_text) {
    wasm_export_kind_t kind;
    uint32_t global_idx;
    wasm_value_t value;
    wasm_error_t err;
    char error_buf[128];

    if (!wasm_find_export(mod, export_name, &kind, &global_idx) || kind != WASM_EXPORT_GLOBAL) {
        fprintf(stderr, "global export '%s' not found\n", export_name);
        return 1;
    }

    if (!wasm_cli_parse_value(value_text, wasm_global_type(mod, global_idx),
                              wasm_global_reftype(mod, global_idx), &value,
                              error_buf, sizeof(error_buf))) {
        fprintf(stderr, "%s\n", error_buf);
        return 1;
    }

    err = wasm_global_set(mod, export_name, value);
    if (err != WASM_OK) {
        fprintf(stderr, "global-set failed: %s\n", wasm_cli_error_text(mod->rt));
        return 1;
    }

    return wasm_cli_global_get_cmd(mod, export_name);
}

static void wasm_cli_hexdump(uint64_t base_offset, const uint8_t* data, size_t len) {
    size_t row;

    for (row = 0; row < len; row += 16u) {
        size_t col;

        printf("%016llx  ", (unsigned long long)(base_offset + (uint64_t)row));
        for (col = 0; col < 16u; col++) {
            if (row + col < len)
                printf("%02x ", (unsigned)data[row + col]);
            else
                fputs("   ", stdout);
        }
        fputs(" |", stdout);
        for (col = 0; col < 16u && row + col < len; col++) {
            unsigned char ch = data[row + col];
            fputc(isprint(ch) ? (int)ch : '.', stdout);
        }
        fputs("|\n", stdout);
    }
}

static int wasm_cli_memory_read_cmd(wasm_module_t* mod, const char* memory_index_text,
                                    const char* offset_text, const char* length_text) {
    uint32_t memory_index;
    uint64_t offset;
    size_t length;
    uint8_t* buffer;
    wasm_error_t err;

    if (!wasm_cli_parse_u32(memory_index_text, &memory_index)) {
        fprintf(stderr, "invalid memory index '%s'\n", memory_index_text);
        return 1;
    }
    if (!wasm_cli_parse_u64(offset_text, &offset)) {
        fprintf(stderr, "invalid offset '%s'\n", offset_text);
        return 1;
    }
    if (!wasm_cli_parse_size(length_text, &length)) {
        fprintf(stderr, "invalid length '%s'\n", length_text);
        return 1;
    }

    buffer = (uint8_t*)malloc(length > 0u ? length : 1u);
    if (!buffer) {
        fprintf(stderr, "out of memory allocating memory-read buffer\n");
        return 1;
    }

    err = wasm_memory_read(mod, memory_index, offset, buffer, length);
    if (err != WASM_OK) {
        fprintf(stderr, "memory-read failed: %s\n", wasm_cli_error_text(mod->rt));
        free(buffer);
        return 1;
    }

    wasm_cli_hexdump(offset, buffer, length);
    free(buffer);
    return 0;
}

static void wasm_cli_print_escaped(const char* text) {
    const unsigned char* p = (const unsigned char*)text;

    fputc('"', stdout);
    while (p && *p != '\0') {
        switch (*p) {
            case '\\':
                fputs("\\\\", stdout);
                break;
            case '"':
                fputs("\\\"", stdout);
                break;
            case '\n':
                fputs("\\n", stdout);
                break;
            case '\r':
                fputs("\\r", stdout);
                break;
            case '\t':
                fputs("\\t", stdout);
                break;
            default:
                if (isprint(*p))
                    fputc(*p, stdout);
                else
                    printf("\\x%02x", (unsigned)*p);
                break;
        }
        p++;
    }
    fputs("\"\n", stdout);
}

static int wasm_cli_memory_string_cmd(wasm_module_t* mod, const char* memory_index_text,
                                      const char* offset_text, const char* max_len_text) {
    uint32_t memory_index;
    uint64_t offset;
    size_t max_len = 256u;
    char* buffer;
    wasm_error_t err;

    if (!wasm_cli_parse_u32(memory_index_text, &memory_index)) {
        fprintf(stderr, "invalid memory index '%s'\n", memory_index_text);
        return 1;
    }
    if (!wasm_cli_parse_u64(offset_text, &offset)) {
        fprintf(stderr, "invalid offset '%s'\n", offset_text);
        return 1;
    }
    if (max_len_text && !wasm_cli_parse_size(max_len_text, &max_len)) {
        fprintf(stderr, "invalid max_len '%s'\n", max_len_text);
        return 1;
    }
    if (max_len == 0u) {
        fprintf(stderr, "max_len must be greater than zero\n");
        return 1;
    }

    buffer = (char*)malloc(max_len);
    if (!buffer) {
        fprintf(stderr, "out of memory allocating string buffer\n");
        return 1;
    }

    err = wasm_memory_get_string(mod, memory_index, offset, buffer, max_len);
    if (err != WASM_OK) {
        fprintf(stderr, "memory-string failed: %s\n", wasm_cli_error_text(mod->rt));
        free(buffer);
        return 1;
    }

    wasm_cli_print_escaped(buffer);
    free(buffer);
    return 0;
}

int main(int argc, char** argv) {
    wasm_cli_options_t options;
    int argi = 1;
    const char* module_path;
    const char* command;
    unsigned char* bytes = NULL;
    size_t bytes_len = 0u;
    wasm_runtime_t runtime;
    wasm_module_t* module = NULL;
    int exit_code = 1;

    options.bind_wasi = 0;
    options.bind_emscripten = 0;
    options.fuel_set = 0;
    options.fuel = 0;

    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "--help") == 0 || strcmp(argv[argi], "-h") == 0) {
            wasm_cli_print_usage(stdout, argv[0]);
            return 0;
        }
        if (strcmp(argv[argi], "--wasi") == 0) {
            options.bind_wasi = 1;
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "--no-wasi") == 0) {
            options.bind_wasi = 0;
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "--emscripten") == 0) {
            options.bind_emscripten = 1;
            argi++;
            continue;
        }
        if (strcmp(argv[argi], "--fuel") == 0) {
            if (argi + 1 >= argc || !wasm_cli_parse_u64(argv[argi + 1], &options.fuel)) {
                fprintf(stderr, "invalid fuel value\n");
                return 2;
            }
            options.fuel_set = 1;
            argi += 2;
            continue;
        }
        break;
    }

    if (argc - argi < 2) {
        wasm_cli_print_usage(stderr, argv[0]);
        return 2;
    }

    module_path = argv[argi++];
    command = argv[argi++];

    bytes = wasm_cli_read_file(module_path, &bytes_len);
    if (!bytes) {
        fprintf(stderr, "failed to read %s: %s\n", module_path, strerror(errno));
        return 1;
    }

    if (wasm_init(&runtime, NULL) != WASM_OK) {
        fprintf(stderr, "failed to initialize runtime\n");
        free(bytes);
        return 1;
    }

    if (options.fuel_set) wasm_set_fuel(&runtime, options.fuel);

    if (options.bind_wasi) {
        wasm_error_t err = wasm_bind_wasi_stubs(&runtime);

        if (err != WASM_OK) {
            fprintf(stderr, "failed to bind WASI stubs: %s\n", wasm_cli_error_text(&runtime));
            goto cleanup;
        }
    }

    if (options.bind_emscripten) {
        wasm_error_t err = wasm_bind_emscripten_stubs(&runtime);

        if (err != WASM_OK) {
            fprintf(stderr, "failed to bind Emscripten stubs: %s\n", wasm_cli_error_text(&runtime));
            goto cleanup;
        }
    }

    module = wasm_load(&runtime, bytes, bytes_len);
    free(bytes);
    bytes = NULL;
    if (!module) {
        fprintf(stderr, "load failed: %s\n", wasm_cli_error_text(&runtime));
        goto cleanup;
    }

    if (strcmp(command, "info") == 0) {
        wasm_cli_print_summary(module, module_path);
        exit_code = 0;
    } else if (strcmp(command, "exports") == 0) {
        wasm_cli_print_exports(module);
        exit_code = 0;
    } else if (strcmp(command, "funcs") == 0) {
        wasm_cli_print_funcs(module);
        exit_code = 0;
    } else if (strcmp(command, "types") == 0) {
        wasm_cli_print_types(module);
        exit_code = 0;
    } else if (strcmp(command, "globals") == 0) {
        wasm_cli_print_globals(module);
        exit_code = 0;
    } else if (strcmp(command, "memories") == 0) {
        wasm_cli_print_memories(module);
        exit_code = 0;
    } else if (strcmp(command, "custom-sections") == 0) {
        wasm_cli_print_custom_sections(module);
        exit_code = 0;
    } else if (strcmp(command, "call") == 0) {
        if (argc - argi < 1) {
            fprintf(stderr, "call requires <export_name>\n");
            wasm_cli_print_usage(stderr, argv[0]);
            exit_code = 2;
        } else {
            exit_code = wasm_cli_call_export(module, argv[argi], argc - argi - 1, &argv[argi + 1]);
        }
    } else if (strcmp(command, "call-index") == 0) {
        if (argc - argi < 1) {
            fprintf(stderr, "call-index requires <func_index>\n");
            wasm_cli_print_usage(stderr, argv[0]);
            exit_code = 2;
        } else {
            exit_code = wasm_cli_call_index(module, argv[argi], argc - argi - 1, &argv[argi + 1]);
        }
    } else if (strcmp(command, "global-get") == 0) {
        if (argc - argi != 1) {
            fprintf(stderr, "global-get requires <export_name>\n");
            exit_code = 2;
        } else {
            exit_code = wasm_cli_global_get_cmd(module, argv[argi]);
        }
    } else if (strcmp(command, "global-set") == 0) {
        if (argc - argi != 2) {
            fprintf(stderr, "global-set requires <export_name> <value>\n");
            exit_code = 2;
        } else {
            exit_code = wasm_cli_global_set_cmd(module, argv[argi], argv[argi + 1]);
        }
    } else if (strcmp(command, "memory-read") == 0) {
        if (argc - argi != 3) {
            fprintf(stderr, "memory-read requires <memory_index> <offset> <length>\n");
            exit_code = 2;
        } else {
            exit_code = wasm_cli_memory_read_cmd(module, argv[argi], argv[argi + 1], argv[argi + 2]);
        }
    } else if (strcmp(command, "memory-string") == 0) {
        if (argc - argi < 2 || argc - argi > 3) {
            fprintf(stderr, "memory-string requires <memory_index> <offset> [max_len]\n");
            exit_code = 2;
        } else {
            exit_code = wasm_cli_memory_string_cmd(module, argv[argi], argv[argi + 1],
                                                   argc - argi == 3 ? argv[argi + 2] : NULL);
        }
    } else {
        fprintf(stderr, "unknown command '%s'\n", command);
        wasm_cli_print_usage(stderr, argv[0]);
        exit_code = 2;
    }

cleanup:
    wasm_free_module(module);
    wasm_destroy(&runtime);
    free(bytes);
    return exit_code;
}
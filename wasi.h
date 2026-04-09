/*
 * wasi.h — Experimental single-header component runtime scaffold for C99
 *
 * USAGE:
 *   #define WASI_IMPL
 *   #include "wasi.h"
 *
 * This first scaffold layer wires wasi.h into the build, owns an embedded
 * wasm_runtime_t, distinguishes core modules from component binaries, and
 * retains raw component bytes for future parser work.
 */

#ifndef WASI_H
#define WASI_H

#if defined(WASI_IMPL) && !defined(WASM_IMPL)
#define WASM_IMPL
#endif

#include "wasm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wasi_component_t wasi_component_t;

typedef enum wasi_error_t {
    WASI_OK = 0,
    WASI_ERR_INVALID_ARGUMENT,
    WASI_ERR_INVALID_MAGIC,
    WASI_ERR_INVALID_VERSION,
    WASI_ERR_MALFORMED,
    WASI_ERR_UNSUPPORTED_BINARY,
    WASI_ERR_NOT_IMPLEMENTED,
    WASI_ERR_OOM,
    WASI_ERR_RUNTIME_INIT,
} wasi_error_t;

typedef enum wasi_binary_kind_t {
    WASI_BINARY_KIND_UNKNOWN = 0,
    WASI_BINARY_KIND_CORE_MODULE,
    WASI_BINARY_KIND_COMPONENT,
} wasi_binary_kind_t;

typedef enum wasi_component_status_t {
    WASI_COMPONENT_STATUS_UNPARSED = 0,
    WASI_COMPONENT_STATUS_PARSED_SECTIONS,
} wasi_component_status_t;

typedef enum wasi_component_extern_kind_t {
    WASI_COMPONENT_EXTERN_KIND_MODULE = 0,
    WASI_COMPONENT_EXTERN_KIND_FUNC = 1,
    WASI_COMPONENT_EXTERN_KIND_VALUE = 2,
    WASI_COMPONENT_EXTERN_KIND_TYPE = 3,
    WASI_COMPONENT_EXTERN_KIND_COMPONENT = 4,
    WASI_COMPONENT_EXTERN_KIND_INSTANCE = 5,
    WASI_COMPONENT_EXTERN_KIND_UNKNOWN = 255,
} wasi_component_extern_kind_t;

typedef enum wasi_component_type_kind_t {
    WASI_COMPONENT_TYPE_KIND_UNKNOWN = 0,
    WASI_COMPONENT_TYPE_KIND_FUNC,
} wasi_component_type_kind_t;

typedef enum wasi_component_alias_kind_t {
    WASI_COMPONENT_ALIAS_KIND_UNKNOWN = 0,
    WASI_COMPONENT_ALIAS_KIND_CORE_INSTANCE_EXPORT,
    WASI_COMPONENT_ALIAS_KIND_INSTANCE_EXPORT,
    WASI_COMPONENT_ALIAS_KIND_OUTER,
} wasi_component_alias_kind_t;

typedef enum wasi_component_core_instance_kind_t {
    WASI_COMPONENT_CORE_INSTANCE_KIND_UNKNOWN = 0,
    WASI_COMPONENT_CORE_INSTANCE_KIND_INSTANTIATE,
    WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS,
} wasi_component_core_instance_kind_t;

typedef enum wasi_component_instance_kind_t {
    WASI_COMPONENT_INSTANCE_KIND_UNKNOWN = 0,
    WASI_COMPONENT_INSTANCE_KIND_INSTANTIATE,
    WASI_COMPONENT_INSTANCE_KIND_FROM_EXPORTS,
} wasi_component_instance_kind_t;

typedef enum wasi_component_canon_kind_t {
    WASI_COMPONENT_CANON_KIND_UNKNOWN = 0,
    WASI_COMPONENT_CANON_KIND_LIFT,
    WASI_COMPONENT_CANON_KIND_LOWER,
} wasi_component_canon_kind_t;

typedef struct wasi_component_named_type_t {
    char* name;
    uint8_t type_code;
} wasi_component_named_type_t;

typedef struct wasi_component_func_type_t {
    wasi_component_named_type_t* params;
    uint32_t num_params;
    uint32_t params_capacity;
    wasi_component_named_type_t* results;
    uint32_t num_results;
    uint32_t results_capacity;
} wasi_component_func_type_t;

typedef struct wasi_component_type_t {
    wasi_component_type_kind_t kind;
    wasi_component_func_type_t func;
} wasi_component_type_t;

typedef struct wasi_component_func_t {
    uint32_t type_index;
} wasi_component_func_t;

typedef struct wasi_component_import_t {
    char* name;
    uint8_t name_kind;
    wasi_component_extern_kind_t kind;
    uint32_t type_index;
} wasi_component_import_t;

typedef struct wasi_component_export_t {
    char* name;
    uint8_t name_kind;
    wasi_component_extern_kind_t kind;
    uint32_t index;
    int has_type;
    uint32_t type_index;
} wasi_component_export_t;

typedef struct wasi_component_alias_t {
    wasi_component_alias_kind_t kind;
    int sort_is_core;
    uint8_t sort_code;
    wasi_component_extern_kind_t extern_kind;
    wasm_export_kind_t core_export_kind;
    uint32_t instance_index;
    uint8_t name_kind;
    uint32_t outer_count;
    uint32_t outer_index;
    char* name;
} wasi_component_alias_t;

typedef struct wasi_component_core_instance_export_t {
    char* name;
    wasm_export_kind_t kind;
    uint32_t index;
} wasi_component_core_instance_export_t;

typedef struct wasi_component_core_instantiation_arg_t {
    char* name;
    uint8_t kind;
    uint32_t index;
} wasi_component_core_instantiation_arg_t;

typedef struct wasi_component_core_instance_t {
    wasi_component_core_instance_kind_t kind;
    uint32_t module_index;
    wasi_component_core_instance_export_t* exports;
    uint32_t num_exports;
    uint32_t exports_capacity;
    wasi_component_core_instantiation_arg_t* args;
    uint32_t num_args;
    uint32_t args_capacity;
} wasi_component_core_instance_t;

typedef struct wasi_component_instance_export_t {
    char* name;
    uint8_t name_kind;
    wasi_component_extern_kind_t kind;
    uint32_t index;
} wasi_component_instance_export_t;

typedef struct wasi_component_instantiation_arg_t {
    char* name;
    wasi_component_extern_kind_t kind;
    uint32_t index;
} wasi_component_instantiation_arg_t;

typedef struct wasi_component_instance_t {
    wasi_component_instance_kind_t kind;
    uint32_t component_index;
    wasi_component_instance_export_t* exports;
    uint32_t num_exports;
    uint32_t exports_capacity;
    wasi_component_instantiation_arg_t* args;
    uint32_t num_args;
    uint32_t args_capacity;
} wasi_component_instance_t;

typedef struct wasi_component_canon_option_t {
    uint8_t code;
    int has_index;
    uint32_t index;
} wasi_component_canon_option_t;

typedef struct wasi_component_canon_t {
    wasi_component_canon_kind_t kind;
    uint8_t async_flag;
    uint32_t func_index;
    uint32_t core_func_index;
    int has_type_index;
    uint32_t type_index;
    uint32_t defined_func_index;
    wasi_component_canon_option_t* options;
    uint32_t num_options;
    uint32_t options_capacity;
} wasi_component_canon_t;

typedef struct wasi_component_section_t {
    uint8_t id;
    size_t section_offset;
    size_t payload_offset;
    size_t payload_size;
    char* name;
} wasi_component_section_t;

typedef struct wasi_component_core_module_t {
    size_t payload_offset;
    size_t payload_size;
    wasm_module_t* module;
    wasm_error_t load_error;
    char load_error_msg[256];
} wasi_component_core_module_t;

typedef struct wasi_component_nested_component_t {
    size_t payload_offset;
    size_t payload_size;
    wasi_component_t* component;
} wasi_component_nested_component_t;

typedef struct wasi_config_t {
    wasm_config_t runtime_config;
    int has_runtime_config;
} wasi_config_t;

typedef struct wasi_engine_t {
    wasm_runtime_t runtime;
    wasi_error_t last_error;
    char error_msg[256];
} wasi_engine_t;

struct wasi_component_t {
    wasi_engine_t* engine;
    uint8_t* bytes;
    size_t size;
    uint8_t version_bytes[4];
    wasi_binary_kind_t binary_kind;
    wasi_component_status_t status;
    wasi_component_section_t* sections;
    uint32_t num_sections;
    uint32_t sections_capacity;
    wasi_component_type_t* types;
    uint32_t num_types;
    uint32_t types_capacity;
    wasi_component_import_t* imports;
    uint32_t num_imports;
    uint32_t imports_capacity;
    wasi_component_export_t* exports;
    uint32_t num_exports;
    uint32_t exports_capacity;
    wasi_component_alias_t* aliases;
    uint32_t num_aliases;
    uint32_t aliases_capacity;
    wasi_component_core_instance_t* core_instances;
    uint32_t num_core_instances;
    uint32_t core_instances_capacity;
    wasi_component_instance_t* instances;
    uint32_t num_instances;
    uint32_t instances_capacity;
    wasi_component_canon_t* canons;
    uint32_t num_canons;
    uint32_t canons_capacity;
    wasi_component_func_t* funcs;
    uint32_t num_funcs;
    uint32_t funcs_capacity;
    wasi_component_core_module_t* core_modules;
    uint32_t num_core_modules;
    uint32_t core_modules_capacity;
    wasi_component_nested_component_t* nested_components;
    uint32_t num_nested_components;
    uint32_t nested_components_capacity;
    int has_start;
    uint32_t start_func_index;
    uint32_t start_arg_count;
    uint32_t start_result_count;
};

typedef struct wasi_instance_t wasi_instance_t;

void wasi_config_default(wasi_config_t* config);
wasi_error_t wasi_init(wasi_engine_t* engine, const wasi_config_t* config);
void wasi_destroy(wasi_engine_t* engine);
wasi_binary_kind_t wasi_detect_binary_kind(const uint8_t* bytes, size_t len);
wasi_component_t* wasi_load(wasi_engine_t* engine, const uint8_t* bytes, size_t len);
void wasi_free_component(wasi_component_t* component);
wasi_binary_kind_t wasi_component_binary_kind(const wasi_component_t* component);
wasi_component_status_t wasi_component_status(const wasi_component_t* component);
uint8_t wasi_component_layer(const wasi_component_t* component);
uint32_t wasi_component_section_count(const wasi_component_t* component);
uint8_t wasi_component_section_id(const wasi_component_t* component, uint32_t index);
size_t wasi_component_section_size(const wasi_component_t* component, uint32_t index);
const char* wasi_component_section_name(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_type_count(const wasi_component_t* component);
wasi_component_type_kind_t wasi_component_type_kind(const wasi_component_t* component, uint32_t type_index);
uint32_t wasi_component_func_type_param_count(const wasi_component_t* component, uint32_t type_index);
const char* wasi_component_func_type_param_name(const wasi_component_t* component, uint32_t type_index, uint32_t param_index);
uint8_t wasi_component_func_type_param_code(const wasi_component_t* component, uint32_t type_index, uint32_t param_index);
uint32_t wasi_component_func_type_result_count(const wasi_component_t* component, uint32_t type_index);
const char* wasi_component_func_type_result_name(const wasi_component_t* component, uint32_t type_index, uint32_t result_index);
uint8_t wasi_component_func_type_result_code(const wasi_component_t* component, uint32_t type_index, uint32_t result_index);
uint32_t wasi_component_import_count(const wasi_component_t* component);
const char* wasi_component_import_name(const wasi_component_t* component, uint32_t index);
wasi_component_extern_kind_t wasi_component_import_kind(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_import_type_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_export_count(const wasi_component_t* component);
const char* wasi_component_export_name(const wasi_component_t* component, uint32_t index);
wasi_component_extern_kind_t wasi_component_export_kind(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_export_index(const wasi_component_t* component, uint32_t index);
int wasi_component_export_has_type(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_export_type_index(const wasi_component_t* component, uint32_t index);
int wasi_component_export_func_type_index(const wasi_component_t* component, uint32_t index, uint32_t* out_type_index);
uint32_t wasi_component_alias_count(const wasi_component_t* component);
wasi_component_alias_kind_t wasi_component_alias_kind(const wasi_component_t* component, uint32_t index);
int wasi_component_alias_sort_is_core(const wasi_component_t* component, uint32_t index);
uint8_t wasi_component_alias_sort_code(const wasi_component_t* component, uint32_t index);
wasi_component_extern_kind_t wasi_component_alias_extern_kind(const wasi_component_t* component, uint32_t index);
wasm_export_kind_t wasi_component_alias_core_export_kind(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_alias_instance_index(const wasi_component_t* component, uint32_t index);
const char* wasi_component_alias_name(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_alias_outer_count(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_alias_outer_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_core_instance_count(const wasi_component_t* component);
wasi_component_core_instance_kind_t wasi_component_core_instance_kind(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_core_instance_module_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_core_instance_export_count(const wasi_component_t* component, uint32_t instance_index);
const char* wasi_component_core_instance_export_name(const wasi_component_t* component, uint32_t instance_index, uint32_t export_index);
wasm_export_kind_t wasi_component_core_instance_export_kind(const wasi_component_t* component, uint32_t instance_index, uint32_t export_index);
uint32_t wasi_component_core_instance_export_index(const wasi_component_t* component, uint32_t instance_index, uint32_t export_index);
uint32_t wasi_component_core_instance_arg_count(const wasi_component_t* component, uint32_t instance_index);
const char* wasi_component_core_instance_arg_name(const wasi_component_t* component, uint32_t instance_index, uint32_t arg_index);
uint8_t wasi_component_core_instance_arg_kind(const wasi_component_t* component, uint32_t instance_index, uint32_t arg_index);
uint32_t wasi_component_core_instance_arg_index(const wasi_component_t* component, uint32_t instance_index, uint32_t arg_index);
uint32_t wasi_component_instance_count(const wasi_component_t* component);
wasi_component_instance_kind_t wasi_component_instance_kind(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_instance_component_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_instance_export_count(const wasi_component_t* component, uint32_t instance_index);
const char* wasi_component_instance_export_name(const wasi_component_t* component, uint32_t instance_index, uint32_t export_index);
wasi_component_extern_kind_t wasi_component_instance_export_kind(const wasi_component_t* component, uint32_t instance_index, uint32_t export_index);
uint32_t wasi_component_instance_export_index(const wasi_component_t* component, uint32_t instance_index, uint32_t export_index);
uint32_t wasi_component_instance_arg_count(const wasi_component_t* component, uint32_t instance_index);
const char* wasi_component_instance_arg_name(const wasi_component_t* component, uint32_t instance_index, uint32_t arg_index);
wasi_component_extern_kind_t wasi_component_instance_arg_kind(const wasi_component_t* component, uint32_t instance_index, uint32_t arg_index);
uint32_t wasi_component_instance_arg_index(const wasi_component_t* component, uint32_t instance_index, uint32_t arg_index);
int wasi_component_has_start(const wasi_component_t* component);
uint32_t wasi_component_start_func_index(const wasi_component_t* component);
uint32_t wasi_component_start_arg_count(const wasi_component_t* component);
uint32_t wasi_component_start_result_count(const wasi_component_t* component);
uint32_t wasi_component_canon_count(const wasi_component_t* component);
wasi_component_canon_kind_t wasi_component_canon_kind(const wasi_component_t* component, uint32_t index);
uint8_t wasi_component_canon_async_flag(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_canon_func_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_canon_core_func_index(const wasi_component_t* component, uint32_t index);
int wasi_component_canon_has_type_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_canon_type_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_canon_defined_func_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_canon_option_count(const wasi_component_t* component, uint32_t index);
uint8_t wasi_component_canon_option_code(const wasi_component_t* component, uint32_t canon_index, uint32_t option_index);
int wasi_component_canon_option_has_index(const wasi_component_t* component, uint32_t canon_index, uint32_t option_index);
uint32_t wasi_component_canon_option_index(const wasi_component_t* component, uint32_t canon_index, uint32_t option_index);
uint32_t wasi_component_core_module_count(const wasi_component_t* component);
wasm_module_t* wasi_component_core_module_at(const wasi_component_t* component, uint32_t index);
wasm_error_t wasi_component_core_module_error(const wasi_component_t* component, uint32_t index);
const char* wasi_component_core_module_error_message(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_nested_component_count(const wasi_component_t* component);
const wasi_component_t* wasi_component_nested_component_at(const wasi_component_t* component, uint32_t index);
size_t wasi_component_nested_component_offset(const wasi_component_t* component, uint32_t index);
size_t wasi_component_nested_component_size(const wasi_component_t* component, uint32_t index);
void wasi_dump_component(const wasi_component_t* component, char* buffer, size_t buffer_size);
const char* wasi_component_extern_kind_string(wasi_component_extern_kind_t kind);
const char* wasi_component_type_kind_string(wasi_component_type_kind_t kind);
const char* wasi_component_primitive_type_string(uint8_t type_code);
const char* wasi_component_alias_kind_string(wasi_component_alias_kind_t kind);
const char* wasi_component_core_instance_kind_string(wasi_component_core_instance_kind_t kind);
const char* wasi_component_instance_kind_string(wasi_component_instance_kind_t kind);
const char* wasi_component_core_sort_string(uint8_t sort);
const char* wasi_component_canon_kind_string(wasi_component_canon_kind_t kind);
const char* wasi_component_core_export_kind_string(wasm_export_kind_t kind);
const char* wasi_error_string(wasi_error_t err);
const char* wasi_component_status_string(wasi_component_status_t status);

#ifdef __cplusplus
}
#endif

#ifdef WASI_IMPL

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define WASI__COMPONENT_PRE_STANDARD_BYTE 0x0Du
#define WASI__COMPONENT_STANDARD_BYTE 0x01u
#define WASI__COMPONENT_VERSION_BYTE_1 0x00u
#define WASI__COMPONENT_VERSION_BYTE_2 0x01u
#define WASI__COMPONENT_VERSION_BYTE_3 0x00u

typedef struct wasi__reader_t {
    const uint8_t* ptr;
    const uint8_t* end;
    int malformed;
} wasi__reader_t;

static wasi_error_t wasi__classify_binary(const uint8_t* bytes,
                                          size_t len,
                                          wasi_binary_kind_t* out_kind,
                                          uint8_t out_version[4]);

static void wasi__clear_error(wasi_engine_t* engine) {
    if (!engine) return;
    engine->last_error = WASI_OK;
    engine->error_msg[0] = '\0';
}

static wasi_error_t wasi__set_error_literal(wasi_engine_t* engine,
                                            wasi_error_t err,
                                            const char* msg) {
    if (engine) {
        engine->last_error = err;
        if (msg && msg[0])
            (void)snprintf(engine->error_msg, sizeof(engine->error_msg), "%s", msg);
        else
            engine->error_msg[0] = '\0';
    }
    return err;
}

static wasi_error_t wasi__set_errorf(wasi_engine_t* engine, wasi_error_t err, const char* fmt, ...) {
    va_list args;

    if (!engine) return err;

    engine->last_error = err;
    if (!fmt || !fmt[0]) {
        engine->error_msg[0] = '\0';
        return err;
    }

    va_start(args, fmt);
    (void)vsnprintf(engine->error_msg, sizeof(engine->error_msg), fmt, args);
    va_end(args);
    return err;
}

static int wasi__has_magic(const uint8_t* bytes, size_t len) {
    return len >= 4 && bytes && bytes[0] == 0x00 && bytes[1] == 0x61 && bytes[2] == 0x73 && bytes[3] == 0x6D;
}

static int wasi__is_core_module_version(const uint8_t version[4]) {
    return version[0] == 0x01 && version[1] == 0x00 && version[2] == 0x00 && version[3] == 0x00;
}

static int wasi__is_component_version(const uint8_t version[4]) {
    if (!version) return 0;
    if (version[1] != WASI__COMPONENT_VERSION_BYTE_1 ||
        version[2] != WASI__COMPONENT_VERSION_BYTE_2 ||
        version[3] != WASI__COMPONENT_VERSION_BYTE_3)
        return 0;
    return version[0] == WASI__COMPONENT_PRE_STANDARD_BYTE ||
           version[0] == WASI__COMPONENT_STANDARD_BYTE;
}

static int wasi__has(const wasi__reader_t* reader, size_t size) {
    return reader && reader->ptr && reader->end && (size_t)(reader->end - reader->ptr) >= size;
}

static uint8_t wasi__read_u8(wasi__reader_t* reader) {
    if (!wasi__has(reader, 1)) {
        if (reader) reader->malformed = 1;
        return 0;
    }
    return *reader->ptr++;
}

static uint32_t wasi__read_leb128_u32(wasi__reader_t* reader) {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint32_t i;

    for (i = 0; i < 5; i++) {
        uint8_t byte;
        if (!wasi__has(reader, 1)) {
            if (reader) reader->malformed = 1;
            return 0;
        }

        byte = *reader->ptr++;
        result |= (uint32_t)(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) return result;
        shift += 7;
    }

    if (reader) reader->malformed = 1;
    return 0;
}

static char* wasi__dup_name_bytes(const uint8_t* bytes, size_t len) {
    char* name;

    name = (char*)WASM_MALLOC(len + 1u);
    if (!name) return NULL;
    if (len) memcpy(name, bytes, len);
    name[len] = '\0';
    return name;
}

static wasi_component_extern_kind_t wasi__extern_kind_from_byte(uint8_t byte) {
    switch (byte) {
        case 0:
            return WASI_COMPONENT_EXTERN_KIND_MODULE;
        case 1:
            return WASI_COMPONENT_EXTERN_KIND_FUNC;
        case 2:
            return WASI_COMPONENT_EXTERN_KIND_VALUE;
        case 3:
            return WASI_COMPONENT_EXTERN_KIND_TYPE;
        case 4:
            return WASI_COMPONENT_EXTERN_KIND_COMPONENT;
        case 5:
            return WASI_COMPONENT_EXTERN_KIND_INSTANCE;
        default:
            return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    }
}

static int wasi__read_component_name(wasi__reader_t* reader,
                                     uint8_t* out_name_kind,
                                     char** out_name) {
    uint8_t name_kind;
    uint32_t name_len;

    if (out_name_kind) *out_name_kind = 0;
    if (out_name) *out_name = NULL;
    if (!reader || !out_name) return 0;

    name_kind = wasi__read_u8(reader);
    name_len = wasi__read_leb128_u32(reader);
    if (reader->malformed || !wasi__has(reader, (size_t)name_len)) {
        reader->malformed = 1;
        return 0;
    }

    *out_name = wasi__dup_name_bytes(reader->ptr, (size_t)name_len);
    if (!*out_name) {
        reader->malformed = 1;
        return 0;
    }
    if (out_name_kind) *out_name_kind = name_kind;
    reader->ptr += name_len;
    return 1;
}

static int wasi__read_component_plain_name(wasi__reader_t* reader, char** out_name) {
    uint32_t name_len;

    if (out_name) *out_name = NULL;
    if (!reader || !out_name) return 0;

    name_len = wasi__read_leb128_u32(reader);
    if (reader->malformed || !wasi__has(reader, (size_t)name_len)) {
        reader->malformed = 1;
        return 0;
    }

    *out_name = wasi__dup_name_bytes(reader->ptr, (size_t)name_len);
    if (!*out_name) {
        reader->malformed = 1;
        return 0;
    }
    reader->ptr += name_len;
    return 1;
}

static int wasi__canon_option_has_immediate(uint8_t option) {
    return option == 0x03 || option == 0x04 || option == 0x05;
}

static wasi_component_extern_kind_t wasi__alias_component_sort_from_byte(uint8_t byte) {
    switch (byte) {
        case 0x01:
            return WASI_COMPONENT_EXTERN_KIND_FUNC;
        case 0x02:
            return WASI_COMPONENT_EXTERN_KIND_VALUE;
        case 0x03:
            return WASI_COMPONENT_EXTERN_KIND_TYPE;
        case 0x04:
            return WASI_COMPONENT_EXTERN_KIND_COMPONENT;
        case 0x05:
            return WASI_COMPONENT_EXTERN_KIND_INSTANCE;
        default:
            return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    }
}

static const char* wasi__alias_sort_string(int sort_is_core, uint8_t sort_code) {
    if (!sort_is_core) return wasi_component_extern_kind_string(wasi__alias_component_sort_from_byte(sort_code));

    switch (sort_code) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
            return wasi_component_core_export_kind_string((wasm_export_kind_t)sort_code);
        case 0x10:
            return "core-type";
        case 0x11:
            return "core-module";
        default:
            return "unknown";
    }
}

static void wasi__component_core_instance_release(wasi_component_core_instance_t* instance) {
    uint32_t i;

    if (!instance) return;

    for (i = 0; i < instance->num_exports; i++) {
        WASM_FREE(instance->exports[i].name);
    }
    WASM_FREE(instance->exports);
    instance->exports = NULL;
    instance->num_exports = 0;
    instance->exports_capacity = 0;

    for (i = 0; i < instance->num_args; i++) {
        WASM_FREE(instance->args[i].name);
    }
    WASM_FREE(instance->args);
    instance->args = NULL;
    instance->num_args = 0;
    instance->args_capacity = 0;
    instance->module_index = UINT32_MAX;
    instance->kind = WASI_COMPONENT_CORE_INSTANCE_KIND_UNKNOWN;
}

static void wasi__component_instance_release(wasi_component_instance_t* instance) {
    uint32_t i;

    if (!instance) return;

    for (i = 0; i < instance->num_exports; i++) {
        WASM_FREE(instance->exports[i].name);
    }
    WASM_FREE(instance->exports);
    instance->exports = NULL;
    instance->num_exports = 0;
    instance->exports_capacity = 0;

    for (i = 0; i < instance->num_args; i++) {
        WASM_FREE(instance->args[i].name);
    }
    WASM_FREE(instance->args);
    instance->args = NULL;
    instance->num_args = 0;
    instance->args_capacity = 0;
    instance->component_index = UINT32_MAX;
    instance->kind = WASI_COMPONENT_INSTANCE_KIND_UNKNOWN;
}

static void wasi__component_release_storage(wasi_component_t* component) {
    uint32_t i;

    if (!component) return;

    for (i = 0; i < component->num_aliases; i++) {
        WASM_FREE(component->aliases[i].name);
    }
    WASM_FREE(component->aliases);
    component->aliases = NULL;
    component->num_aliases = 0;
    component->aliases_capacity = 0;

    for (i = 0; i < component->num_core_instances; i++) {
        wasi__component_core_instance_release(&component->core_instances[i]);
    }
    WASM_FREE(component->core_instances);
    component->core_instances = NULL;
    component->num_core_instances = 0;
    component->core_instances_capacity = 0;

    for (i = 0; i < component->num_instances; i++) {
        wasi__component_instance_release(&component->instances[i]);
    }
    WASM_FREE(component->instances);
    component->instances = NULL;
    component->num_instances = 0;
    component->instances_capacity = 0;

    for (i = 0; i < component->num_canons; i++) {
        WASM_FREE(component->canons[i].options);
    }
    WASM_FREE(component->canons);
    component->canons = NULL;
    component->num_canons = 0;
    component->canons_capacity = 0;

    for (i = 0; i < component->num_types; i++) {
        uint32_t j;
        for (j = 0; j < component->types[i].func.num_params; j++) {
            WASM_FREE(component->types[i].func.params[j].name);
        }
        WASM_FREE(component->types[i].func.params);
        for (j = 0; j < component->types[i].func.num_results; j++) {
            WASM_FREE(component->types[i].func.results[j].name);
        }
        WASM_FREE(component->types[i].func.results);
    }
    WASM_FREE(component->types);
    component->types = NULL;
    component->num_types = 0;
    component->types_capacity = 0;

    WASM_FREE(component->funcs);
    component->funcs = NULL;
    component->num_funcs = 0;
    component->funcs_capacity = 0;

    for (i = 0; i < component->num_imports; i++) {
        WASM_FREE(component->imports[i].name);
    }
    WASM_FREE(component->imports);
    component->imports = NULL;
    component->num_imports = 0;
    component->imports_capacity = 0;

    for (i = 0; i < component->num_exports; i++) {
        WASM_FREE(component->exports[i].name);
    }
    WASM_FREE(component->exports);
    component->exports = NULL;
    component->num_exports = 0;
    component->exports_capacity = 0;

    for (i = 0; i < component->num_core_modules; i++) {
        wasm_free_module(component->core_modules[i].module);
    }
    WASM_FREE(component->core_modules);
    component->core_modules = NULL;
    component->num_core_modules = 0;
    component->core_modules_capacity = 0;

    for (i = 0; i < component->num_nested_components; i++) {
        wasi_free_component(component->nested_components[i].component);
    }
    WASM_FREE(component->nested_components);
    component->nested_components = NULL;
    component->num_nested_components = 0;
    component->nested_components_capacity = 0;

    for (i = 0; i < component->num_sections; i++) {
        WASM_FREE(component->sections[i].name);
    }
    WASM_FREE(component->sections);
    component->sections = NULL;
    component->num_sections = 0;
    component->sections_capacity = 0;

    WASM_FREE(component->bytes);
    component->bytes = NULL;
    component->size = 0;
}

static wasi_error_t wasi__func_type_append_named(wasi_component_named_type_t** items,
                                                 uint32_t* count,
                                                 uint32_t* capacity,
                                                 char* name,
                                                 uint8_t type_code) {
    wasi_component_named_type_t* grown;
    size_t next_capacity;
    wasi_component_named_type_t* entry;

    if (!items || !count || !capacity) {
        WASM_FREE(name);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (*count == *capacity) {
        next_capacity = *capacity ? (size_t)(*capacity) * 2u : 4u;
        grown = (wasi_component_named_type_t*)WASM_REALLOC(*items, next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            return WASI_ERR_OOM;
        }
        *items = grown;
        *capacity = (uint32_t)next_capacity;
    }

    entry = &(*items)[(*count)++];
    memset(entry, 0, sizeof(*entry));
    entry->name = name;
    entry->type_code = type_code;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_type(wasi_component_t* component, const wasi_component_type_t* type) {
    wasi_component_type_t* grown;
    size_t next_capacity;

    if (!component || !type) return WASI_ERR_INVALID_ARGUMENT;

    if (component->num_types == component->types_capacity) {
        next_capacity = component->types_capacity ? (size_t)component->types_capacity * 2u : 4u;
        grown = (wasi_component_type_t*)WASM_REALLOC(component->types, next_capacity * sizeof(*grown));
        if (!grown) return WASI_ERR_OOM;
        component->types = grown;
        component->types_capacity = (uint32_t)next_capacity;
    }

    component->types[component->num_types++] = *type;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_func(wasi_component_t* component, uint32_t type_index) {
    wasi_component_func_t* grown;
    size_t next_capacity;

    if (!component) return WASI_ERR_INVALID_ARGUMENT;

    if (component->num_funcs == component->funcs_capacity) {
        next_capacity = component->funcs_capacity ? (size_t)component->funcs_capacity * 2u : 4u;
        grown = (wasi_component_func_t*)WASM_REALLOC(component->funcs, next_capacity * sizeof(*grown));
        if (!grown) return WASI_ERR_OOM;
        component->funcs = grown;
        component->funcs_capacity = (uint32_t)next_capacity;
    }

    component->funcs[component->num_funcs].type_index = type_index;
    component->num_funcs++;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_core_module(wasi_component_t* component,
                                                       size_t payload_offset,
                                                       size_t payload_size,
                                                       wasm_module_t* module,
                                                       wasm_error_t load_error,
                                                       const char* load_error_msg) {
    wasi_component_core_module_t* grown;
    size_t next_capacity;
    wasi_component_core_module_t* entry;

    if (!component) return WASI_ERR_INVALID_ARGUMENT;

    if (component->num_core_modules == component->core_modules_capacity) {
        next_capacity = component->core_modules_capacity ? (size_t)component->core_modules_capacity * 2u : 4u;
        grown = (wasi_component_core_module_t*)WASM_REALLOC(component->core_modules,
                                                            next_capacity * sizeof(*grown));
        if (!grown) return WASI_ERR_OOM;
        component->core_modules = grown;
        component->core_modules_capacity = (uint32_t)next_capacity;
    }

    entry = &component->core_modules[component->num_core_modules++];
    memset(entry, 0, sizeof(*entry));
    entry->payload_offset = payload_offset;
    entry->payload_size = payload_size;
    entry->module = module;
    entry->load_error = load_error;
    if (load_error_msg && load_error_msg[0])
        (void)snprintf(entry->load_error_msg, sizeof(entry->load_error_msg), "%s", load_error_msg);
    return WASI_OK;
}

static wasi_error_t wasi__component_append_nested_component(wasi_component_t* component,
                                                            size_t payload_offset,
                                                            size_t payload_size,
                                                            wasi_component_t* nested_component) {
    wasi_component_nested_component_t* grown;
    size_t next_capacity;
    wasi_component_nested_component_t* entry;

    if (!component || !nested_component) {
        wasi_free_component(nested_component);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (component->num_nested_components == component->nested_components_capacity) {
        next_capacity = component->nested_components_capacity ? (size_t)component->nested_components_capacity * 2u : 4u;
        grown = (wasi_component_nested_component_t*)WASM_REALLOC(component->nested_components,
                                                                 next_capacity * sizeof(*grown));
        if (!grown) {
            wasi_free_component(nested_component);
            return WASI_ERR_OOM;
        }
        component->nested_components = grown;
        component->nested_components_capacity = (uint32_t)next_capacity;
    }

    entry = &component->nested_components[component->num_nested_components++];
    memset(entry, 0, sizeof(*entry));
    entry->payload_offset = payload_offset;
    entry->payload_size = payload_size;
    entry->component = nested_component;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_import(wasi_component_t* component,
                                                  char* name,
                                                  uint8_t name_kind,
                                                  wasi_component_extern_kind_t kind,
                                                  uint32_t type_index) {
    wasi_component_import_t* grown;
    size_t next_capacity;
    wasi_component_import_t* entry;

    if (!component || !name) {
        WASM_FREE(name);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (component->num_imports == component->imports_capacity) {
        next_capacity = component->imports_capacity ? (size_t)component->imports_capacity * 2u : 4u;
        grown = (wasi_component_import_t*)WASM_REALLOC(component->imports,
                                                       next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            return WASI_ERR_OOM;
        }
        component->imports = grown;
        component->imports_capacity = (uint32_t)next_capacity;
    }

    entry = &component->imports[component->num_imports++];
    memset(entry, 0, sizeof(*entry));
    entry->name = name;
    entry->name_kind = name_kind;
    entry->kind = kind;
    entry->type_index = type_index;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_export(wasi_component_t* component,
                                                  char* name,
                                                  uint8_t name_kind,
                                                  wasi_component_extern_kind_t kind,
                                                  uint32_t index,
                                                  int has_type,
                                                  uint32_t type_index) {
    wasi_component_export_t* grown;
    size_t next_capacity;
    wasi_component_export_t* entry;

    if (!component || !name) {
        WASM_FREE(name);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (component->num_exports == component->exports_capacity) {
        next_capacity = component->exports_capacity ? (size_t)component->exports_capacity * 2u : 4u;
        grown = (wasi_component_export_t*)WASM_REALLOC(component->exports,
                                                       next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            return WASI_ERR_OOM;
        }
        component->exports = grown;
        component->exports_capacity = (uint32_t)next_capacity;
    }

    entry = &component->exports[component->num_exports++];
    memset(entry, 0, sizeof(*entry));
    entry->name = name;
    entry->name_kind = name_kind;
    entry->kind = kind;
    entry->index = index;
    entry->has_type = has_type;
    entry->type_index = type_index;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_alias(wasi_component_t* component,
                                                 wasi_component_alias_kind_t kind,
                                                 int sort_is_core,
                                                 uint8_t sort_code,
                                                 wasi_component_extern_kind_t extern_kind,
                                                 wasm_export_kind_t core_export_kind,
                                                 uint32_t instance_index,
                                                 uint8_t name_kind,
                                                 uint32_t outer_count,
                                                 uint32_t outer_index,
                                                 char* name) {
    wasi_component_alias_t* grown;
    size_t next_capacity;
    wasi_component_alias_t* entry;

    if (!component || (kind != WASI_COMPONENT_ALIAS_KIND_OUTER && !name)) {
        WASM_FREE(name);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (component->num_aliases == component->aliases_capacity) {
        next_capacity = component->aliases_capacity ? (size_t)component->aliases_capacity * 2u : 4u;
        grown = (wasi_component_alias_t*)WASM_REALLOC(component->aliases, next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            return WASI_ERR_OOM;
        }
        component->aliases = grown;
        component->aliases_capacity = (uint32_t)next_capacity;
    }

    entry = &component->aliases[component->num_aliases++];
    memset(entry, 0, sizeof(*entry));
    entry->kind = kind;
    entry->sort_is_core = sort_is_core;
    entry->sort_code = sort_code;
    entry->extern_kind = extern_kind;
    entry->core_export_kind = core_export_kind;
    entry->instance_index = instance_index;
    entry->name_kind = name_kind;
    entry->outer_count = outer_count;
    entry->outer_index = outer_index;
    entry->name = name;
    return WASI_OK;
}

static wasi_error_t wasi__component_core_instance_append_export(wasi_component_core_instance_t* instance,
                                                                char* name,
                                                                wasm_export_kind_t kind,
                                                                uint32_t index) {
    wasi_component_core_instance_export_t* grown;
    size_t next_capacity;
    wasi_component_core_instance_export_t* entry;

    if (!instance || !name) {
        WASM_FREE(name);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (instance->num_exports == instance->exports_capacity) {
        next_capacity = instance->exports_capacity ? (size_t)instance->exports_capacity * 2u : 4u;
        grown = (wasi_component_core_instance_export_t*)WASM_REALLOC(instance->exports,
                                                                     next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            return WASI_ERR_OOM;
        }
        instance->exports = grown;
        instance->exports_capacity = (uint32_t)next_capacity;
    }

    entry = &instance->exports[instance->num_exports++];
    memset(entry, 0, sizeof(*entry));
    entry->name = name;
    entry->kind = kind;
    entry->index = index;
    return WASI_OK;
}

static wasi_error_t wasi__component_core_instance_append_arg(wasi_component_core_instance_t* instance,
                                                             char* name,
                                                             uint8_t kind,
                                                             uint32_t index) {
    wasi_component_core_instantiation_arg_t* grown;
    size_t next_capacity;
    wasi_component_core_instantiation_arg_t* entry;

    if (!instance || !name) {
        WASM_FREE(name);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (instance->num_args == instance->args_capacity) {
        next_capacity = instance->args_capacity ? (size_t)instance->args_capacity * 2u : 4u;
        grown = (wasi_component_core_instantiation_arg_t*)WASM_REALLOC(instance->args,
                                                                       next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            return WASI_ERR_OOM;
        }
        instance->args = grown;
        instance->args_capacity = (uint32_t)next_capacity;
    }

    entry = &instance->args[instance->num_args++];
    memset(entry, 0, sizeof(*entry));
    entry->name = name;
    entry->kind = kind;
    entry->index = index;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_core_instance(wasi_component_t* component,
                                                         const wasi_component_core_instance_t* instance) {
    wasi_component_core_instance_t* grown;
    size_t next_capacity;

    if (!component || !instance) return WASI_ERR_INVALID_ARGUMENT;

    if (component->num_core_instances == component->core_instances_capacity) {
        next_capacity = component->core_instances_capacity ? (size_t)component->core_instances_capacity * 2u : 4u;
        grown = (wasi_component_core_instance_t*)WASM_REALLOC(component->core_instances,
                                                              next_capacity * sizeof(*grown));
        if (!grown) return WASI_ERR_OOM;
        component->core_instances = grown;
        component->core_instances_capacity = (uint32_t)next_capacity;
    }

    component->core_instances[component->num_core_instances++] = *instance;
    return WASI_OK;
}

static wasi_error_t wasi__component_instance_append_export(wasi_component_instance_t* instance,
                                                           char* name,
                                                           uint8_t name_kind,
                                                           wasi_component_extern_kind_t kind,
                                                           uint32_t index) {
    wasi_component_instance_export_t* grown;
    size_t next_capacity;
    wasi_component_instance_export_t* entry;

    if (!instance || !name) {
        WASM_FREE(name);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (instance->num_exports == instance->exports_capacity) {
        next_capacity = instance->exports_capacity ? (size_t)instance->exports_capacity * 2u : 4u;
        grown = (wasi_component_instance_export_t*)WASM_REALLOC(instance->exports,
                                                                next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            return WASI_ERR_OOM;
        }
        instance->exports = grown;
        instance->exports_capacity = (uint32_t)next_capacity;
    }

    entry = &instance->exports[instance->num_exports++];
    memset(entry, 0, sizeof(*entry));
    entry->name = name;
    entry->name_kind = name_kind;
    entry->kind = kind;
    entry->index = index;
    return WASI_OK;
}

static wasi_error_t wasi__component_instance_append_arg(wasi_component_instance_t* instance,
                                                        char* name,
                                                        wasi_component_extern_kind_t kind,
                                                        uint32_t index) {
    wasi_component_instantiation_arg_t* grown;
    size_t next_capacity;
    wasi_component_instantiation_arg_t* entry;

    if (!instance || !name) {
        WASM_FREE(name);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (instance->num_args == instance->args_capacity) {
        next_capacity = instance->args_capacity ? (size_t)instance->args_capacity * 2u : 4u;
        grown = (wasi_component_instantiation_arg_t*)WASM_REALLOC(instance->args,
                                                                  next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            return WASI_ERR_OOM;
        }
        instance->args = grown;
        instance->args_capacity = (uint32_t)next_capacity;
    }

    entry = &instance->args[instance->num_args++];
    memset(entry, 0, sizeof(*entry));
    entry->name = name;
    entry->kind = kind;
    entry->index = index;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_instance(wasi_component_t* component,
                                                    const wasi_component_instance_t* instance) {
    wasi_component_instance_t* grown;
    size_t next_capacity;

    if (!component || !instance) return WASI_ERR_INVALID_ARGUMENT;

    if (component->num_instances == component->instances_capacity) {
        next_capacity = component->instances_capacity ? (size_t)component->instances_capacity * 2u : 4u;
        grown = (wasi_component_instance_t*)WASM_REALLOC(component->instances,
                                                         next_capacity * sizeof(*grown));
        if (!grown) return WASI_ERR_OOM;
        component->instances = grown;
        component->instances_capacity = (uint32_t)next_capacity;
    }

    component->instances[component->num_instances++] = *instance;
    return WASI_OK;
}

static wasi_error_t wasi__canon_append_option(wasi_component_canon_t* canon,
                                              uint8_t code,
                                              int has_index,
                                              uint32_t index) {
    wasi_component_canon_option_t* grown;
    size_t next_capacity;
    wasi_component_canon_option_t* entry;

    if (!canon) return WASI_ERR_INVALID_ARGUMENT;

    if (canon->num_options == canon->options_capacity) {
        next_capacity = canon->options_capacity ? (size_t)canon->options_capacity * 2u : 4u;
        grown = (wasi_component_canon_option_t*)WASM_REALLOC(canon->options, next_capacity * sizeof(*grown));
        if (!grown) return WASI_ERR_OOM;
        canon->options = grown;
        canon->options_capacity = (uint32_t)next_capacity;
    }

    entry = &canon->options[canon->num_options++];
    entry->code = code;
    entry->has_index = has_index;
    entry->index = index;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_canon(wasi_component_t* component,
                                                 const wasi_component_canon_t* canon) {
    wasi_component_canon_t* grown;
    size_t next_capacity;

    if (!component || !canon) return WASI_ERR_INVALID_ARGUMENT;

    if (component->num_canons == component->canons_capacity) {
        next_capacity = component->canons_capacity ? (size_t)component->canons_capacity * 2u : 4u;
        grown = (wasi_component_canon_t*)WASM_REALLOC(component->canons, next_capacity * sizeof(*grown));
        if (!grown) return WASI_ERR_OOM;
        component->canons = grown;
        component->canons_capacity = (uint32_t)next_capacity;
    }

    component->canons[component->num_canons++] = *canon;
    return WASI_OK;
}

static const char* wasi__component_section_kind_name(uint8_t id) {
    switch (id) {
        case 0:
            return "custom";
        case 1:
            return "core-module";
        case 2:
            return "core-instance";
        case 3:
            return "core-type";
        case 4:
            return "component";
        case 5:
            return "instance";
        case 6:
            return "alias";
        case 7:
            return "type";
        case 8:
            return "canon";
        case 9:
            return "start";
        case 10:
            return "import";
        case 11:
            return "export";
        default:
            return "unknown";
    }
}

static wasi_error_t wasi__component_append_section(wasi_component_t* component,
                                                   uint8_t id,
                                                   size_t section_offset,
                                                   size_t payload_offset,
                                                   size_t payload_size,
                                                   char* name) {
    wasi_component_section_t* grown;
    size_t next_capacity;
    wasi_component_section_t* section;

    if (!component) {
        WASM_FREE(name);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (component->num_sections == component->sections_capacity) {
        next_capacity = component->sections_capacity ? (size_t)component->sections_capacity * 2u : 8u;
        grown = (wasi_component_section_t*)WASM_REALLOC(component->sections,
                                                        next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            return WASI_ERR_OOM;
        }
        component->sections = grown;
        component->sections_capacity = (uint32_t)next_capacity;
    }

    section = &component->sections[component->num_sections++];
    memset(section, 0, sizeof(*section));
    section->id = id;
    section->section_offset = section_offset;
    section->payload_offset = payload_offset;
    section->payload_size = payload_size;
    section->name = name;
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_sections(wasi_engine_t* engine, wasi_component_t* component) {
    wasi__reader_t reader;

    if (!engine || !component || !component->bytes || component->size < 8) {
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component missing bytes");
    }

    reader.ptr = component->bytes + 8;
    reader.end = component->bytes + component->size;
    reader.malformed = 0;

    while (reader.ptr < reader.end) {
        const uint8_t* payload_start;
        wasi__reader_t payload_reader;
        uint8_t id;
        uint32_t payload_size_u32;
        size_t section_offset;
        size_t payload_offset;
        size_t payload_size;
        char* name = NULL;
        wasi_error_t err;

        section_offset = (size_t)(reader.ptr - component->bytes);
        id = wasi__read_u8(&reader);
        payload_size_u32 = wasi__read_leb128_u32(&reader);
        payload_size = (size_t)payload_size_u32;
        payload_offset = (size_t)(reader.ptr - component->bytes);
        if (reader.malformed || !wasi__has(&reader, payload_size)) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "component section extends past end of file");
        }

        payload_start = reader.ptr;
        payload_reader.ptr = payload_start;
        payload_reader.end = payload_start + payload_size;
        payload_reader.malformed = 0;

        if (id == 0 && payload_size > 0) {
            uint32_t name_len = wasi__read_leb128_u32(&payload_reader);
            if (payload_reader.malformed || !wasi__has(&payload_reader, (size_t)name_len)) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component custom section name");
            }
            name = wasi__dup_name_bytes(payload_reader.ptr, (size_t)name_len);
            if (!name) return wasi__set_error_literal(engine, WASI_ERR_OOM, "component custom section name alloc failed");
        }

        err = wasi__component_append_section(component,
                                             id,
                                             section_offset,
                                             payload_offset,
                                             payload_size,
                                             name);
        if (err != WASI_OK) {
            return wasi__set_error_literal(engine, err, err == WASI_ERR_OOM ? "component section alloc failed" : wasi_error_string(err));
        }

        reader.ptr = payload_start + payload_size;
    }

    component->status = WASI_COMPONENT_STATUS_PARSED_SECTIONS;
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_imports(wasi_engine_t* engine,
                                                  wasi_component_t* component,
                                                  const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component import section");
    }

    for (i = 0; i < count; i++) {
        char* name = NULL;
        uint8_t name_kind = 0;
        uint8_t kind_byte;
        wasi_component_extern_kind_t kind;
        uint32_t type_index;
        wasi_error_t err;

        if (!wasi__read_component_name(&reader, &name_kind, &name)) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component import name");
        }

        kind_byte = wasi__read_u8(&reader);
        kind = wasi__extern_kind_from_byte(kind_byte);
        type_index = wasi__read_leb128_u32(&reader);
        if (reader.malformed) {
            WASM_FREE(name);
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component import descriptor");
        }

        err = wasi__component_append_import(component, name, name_kind, kind, type_index);
        if (err != WASI_OK) {
            return wasi__set_error_literal(engine,
                                           err,
                                           err == WASI_ERR_OOM ? "component import alloc failed"
                                                               : wasi_error_string(err));
        }
        if (kind == WASI_COMPONENT_EXTERN_KIND_FUNC) {
            err = wasi__component_append_func(component, type_index);
            if (err != WASI_OK) {
                return wasi__set_error_literal(engine,
                                               err,
                                               err == WASI_ERR_OOM ? "component func alloc failed"
                                                                   : wasi_error_string(err));
            }
        }
    }

    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in component import section");
    }
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_types(wasi_engine_t* engine,
                                                wasi_component_t* component,
                                                const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component type section");
    }

    for (i = 0; i < count; i++) {
        wasi_component_type_t type;
        uint8_t opcode;
        wasi_error_t err;
        uint32_t j;

        memset(&type, 0, sizeof(type));
        opcode = wasi__read_u8(&reader);
        if (reader.malformed) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component type entry");
        }

        if (opcode != 0x40) {
            return wasi__set_errorf(engine,
                                    WASI_ERR_NOT_IMPLEMENTED,
                                    "component type opcode 0x%02x not implemented",
                                    (unsigned)opcode);
        }

        type.kind = WASI_COMPONENT_TYPE_KIND_FUNC;
        {
            uint32_t param_count = wasi__read_leb128_u32(&reader);
            if (reader.malformed) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component func param count");
            }
            for (j = 0; j < param_count; j++) {
                char* name = NULL;
                uint8_t type_code;

                if (!wasi__read_component_plain_name(&reader, &name)) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component func param name");
                }
                type_code = wasi__read_u8(&reader);
                if (reader.malformed) {
                    WASM_FREE(name);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component func param type");
                }
                err = wasi__func_type_append_named(&type.func.params,
                                                   &type.func.num_params,
                                                   &type.func.params_capacity,
                                                   name,
                                                   type_code);
                if (err != WASI_OK) {
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "component func param alloc failed"
                                                                       : wasi_error_string(err));
                }
            }
        }

        {
            uint8_t result_tag = wasi__read_u8(&reader);
            if (reader.malformed) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component func result tag");
            }
            if (result_tag == 0x00) {
                uint8_t type_code = wasi__read_u8(&reader);
                if (reader.malformed) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component func result type");
                }
                err = wasi__func_type_append_named(&type.func.results,
                                                   &type.func.num_results,
                                                   &type.func.results_capacity,
                                                   NULL,
                                                   type_code);
                if (err != WASI_OK) {
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "component func result alloc failed"
                                                                       : wasi_error_string(err));
                }
            } else if (result_tag == 0x01) {
                uint32_t result_count = wasi__read_leb128_u32(&reader);
                if (reader.malformed) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component named result count");
                }
                for (j = 0; j < result_count; j++) {
                    char* name = NULL;
                    uint8_t type_code;

                    if (!wasi__read_component_plain_name(&reader, &name)) {
                        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component named result name");
                    }
                    type_code = wasi__read_u8(&reader);
                    if (reader.malformed) {
                        WASM_FREE(name);
                        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component named result type");
                    }
                    err = wasi__func_type_append_named(&type.func.results,
                                                       &type.func.num_results,
                                                       &type.func.results_capacity,
                                                       name,
                                                       type_code);
                    if (err != WASI_OK) {
                        return wasi__set_error_literal(engine,
                                                       err,
                                                       err == WASI_ERR_OOM ? "component named result alloc failed"
                                                                           : wasi_error_string(err));
                    }
                }
            } else {
                return wasi__set_errorf(engine,
                                        WASI_ERR_NOT_IMPLEMENTED,
                                        "component func result tag 0x%02x not implemented",
                                        (unsigned)result_tag);
            }
        }

        err = wasi__component_append_type(component, &type);
        if (err != WASI_OK) {
            return wasi__set_error_literal(engine,
                                           err,
                                           err == WASI_ERR_OOM ? "component type alloc failed"
                                                               : wasi_error_string(err));
        }
    }

    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in component type section");
    }
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_aliases(wasi_engine_t* engine,
                                                  wasi_component_t* component,
                                                  const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component alias section");
    }

    for (i = 0; i < count; i++) {
        uint8_t first = wasi__read_u8(&reader);
        int sort_is_core = 0;
        uint8_t sort_code = first;
        wasi_component_extern_kind_t extern_kind = WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
        wasm_export_kind_t core_export_kind = (wasm_export_kind_t)255;
        uint8_t alias_op;
        if (reader.malformed) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component alias entry");
        }
        if (first == 0x00) {
            sort_is_core = 1;
            sort_code = wasi__read_u8(&reader);
            if (sort_code <= 0x04) core_export_kind = (wasm_export_kind_t)sort_code;
        } else {
            extern_kind = wasi__alias_component_sort_from_byte(first);
        }
        alias_op = wasi__read_u8(&reader);
        if (reader.malformed) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component alias descriptor");
        }

        if ((sort_is_core && alias_op == 0x01) || (!sort_is_core && alias_op == 0x00)) {
            uint32_t instance_index = wasi__read_leb128_u32(&reader);
            char* name = NULL;
            wasi_error_t err;

            if (!wasi__read_component_plain_name(&reader, &name)) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed instance export alias name");
            }
            err = wasi__component_append_alias(component,
                                               sort_is_core ? WASI_COMPONENT_ALIAS_KIND_CORE_INSTANCE_EXPORT
                                                            : WASI_COMPONENT_ALIAS_KIND_INSTANCE_EXPORT,
                                               sort_is_core,
                                               sort_code,
                                               extern_kind,
                                               core_export_kind,
                                               instance_index,
                                               0,
                                               0,
                                               0,
                                               name);
            if (err != WASI_OK) {
                return wasi__set_error_literal(engine,
                                               err,
                                               err == WASI_ERR_OOM ? "component alias alloc failed"
                                                                   : wasi_error_string(err));
            }
        } else if (alias_op == 0x02) {
            uint32_t outer_count = wasi__read_leb128_u32(&reader);
            uint32_t outer_index = wasi__read_leb128_u32(&reader);
            wasi_error_t err;

            if (reader.malformed) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed outer alias descriptor");
            }
            err = wasi__component_append_alias(component,
                                               WASI_COMPONENT_ALIAS_KIND_OUTER,
                                               sort_is_core,
                                               sort_code,
                                               extern_kind,
                                               core_export_kind,
                                               UINT32_MAX,
                                               0,
                                               outer_count,
                                               outer_index,
                                               NULL);
            if (err != WASI_OK) {
                return wasi__set_error_literal(engine,
                                               err,
                                               err == WASI_ERR_OOM ? "component alias alloc failed"
                                                                   : wasi_error_string(err));
            }
        } else {
            return wasi__set_errorf(engine,
                                    WASI_ERR_NOT_IMPLEMENTED,
                                    "component alias opcode 0x%02x not implemented",
                                    (unsigned)alias_op);
        }
    }

    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in component alias section");
    }
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_core_instances(wasi_engine_t* engine,
                                                         wasi_component_t* component,
                                                         const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core instance section");
    }

    for (i = 0; i < count; i++) {
        uint8_t opcode = wasi__read_u8(&reader);
        wasi_component_core_instance_t instance;
        wasi_error_t err;
        memset(&instance, 0, sizeof(instance));
        instance.module_index = UINT32_MAX;
        if (reader.malformed) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core instance opcode");
        }

        if (opcode == 0x00) {
            uint32_t arg_count;
            uint32_t arg_index;
            instance.kind = WASI_COMPONENT_CORE_INSTANCE_KIND_INSTANTIATE;
            instance.module_index = wasi__read_leb128_u32(&reader);
            arg_count = wasi__read_leb128_u32(&reader);
            if (reader.malformed) {
                wasi__component_core_instance_release(&instance);
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core instantiation header");
            }
            for (arg_index = 0; arg_index < arg_count; arg_index++) {
                char* name = NULL;
                uint8_t sort;
                uint32_t index;

                if (!wasi__read_component_plain_name(&reader, &name)) {
                    wasi__component_core_instance_release(&instance);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core instantiation arg name");
                }
                sort = wasi__read_u8(&reader);
                index = wasi__read_leb128_u32(&reader);
                if (reader.malformed) {
                    WASM_FREE(name);
                    wasi__component_core_instance_release(&instance);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core instantiation arg descriptor");
                }
                err = wasi__component_core_instance_append_arg(&instance, name, sort, index);
                if (err != WASI_OK) {
                    wasi__component_core_instance_release(&instance);
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "core instantiation arg alloc failed"
                                                                       : wasi_error_string(err));
                }
            }
        } else if (opcode == 0x01) {
            uint32_t export_count;
            uint32_t export_index;
            instance.kind = WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS;
            export_count = wasi__read_leb128_u32(&reader);
            if (reader.malformed) {
                wasi__component_core_instance_release(&instance);
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core instance export count");
            }
            for (export_index = 0; export_index < export_count; export_index++) {
                char* name = NULL;
                uint8_t kind_byte;
                uint32_t index;

                if (!wasi__read_component_plain_name(&reader, &name)) {
                    wasi__component_core_instance_release(&instance);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core instance export name");
                }
                kind_byte = wasi__read_u8(&reader);
                index = wasi__read_leb128_u32(&reader);
                if (reader.malformed) {
                    WASM_FREE(name);
                    wasi__component_core_instance_release(&instance);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core instance export descriptor");
                }
                err = wasi__component_core_instance_append_export(&instance,
                                                                 name,
                                                                 (wasm_export_kind_t)kind_byte,
                                                                 index);
                if (err != WASI_OK) {
                    wasi__component_core_instance_release(&instance);
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "core instance export alloc failed"
                                                                       : wasi_error_string(err));
                }
            }
        } else {
            return wasi__set_errorf(engine,
                                    WASI_ERR_NOT_IMPLEMENTED,
                                    "core instance opcode 0x%02x not implemented",
                                    (unsigned)opcode);
        }

        err = wasi__component_append_core_instance(component, &instance);
        if (err != WASI_OK) {
            wasi__component_core_instance_release(&instance);
            return wasi__set_error_literal(engine,
                                           err,
                                           err == WASI_ERR_OOM ? "core instance alloc failed"
                                                               : wasi_error_string(err));
        }
    }

    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in core instance section");
    }
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_instances(wasi_engine_t* engine,
                                                    wasi_component_t* component,
                                                    const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component instance section");
    }

    for (i = 0; i < count; i++) {
        uint8_t opcode = wasi__read_u8(&reader);
        wasi_component_instance_t instance;
        wasi_error_t err;

        memset(&instance, 0, sizeof(instance));
        instance.component_index = UINT32_MAX;
        if (reader.malformed) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component instance opcode");
        }

        if (opcode == 0x00) {
            uint32_t arg_count;
            uint32_t arg_index;
            instance.kind = WASI_COMPONENT_INSTANCE_KIND_INSTANTIATE;
            instance.component_index = wasi__read_leb128_u32(&reader);
            arg_count = wasi__read_leb128_u32(&reader);
            if (reader.malformed) {
                wasi__component_instance_release(&instance);
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component instantiation header");
            }
            for (arg_index = 0; arg_index < arg_count; arg_index++) {
                char* name = NULL;
                uint8_t kind_byte;
                uint32_t index;

                if (!wasi__read_component_plain_name(&reader, &name)) {
                    wasi__component_instance_release(&instance);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component instantiation arg name");
                }
                kind_byte = wasi__read_u8(&reader);
                index = wasi__read_leb128_u32(&reader);
                if (reader.malformed) {
                    WASM_FREE(name);
                    wasi__component_instance_release(&instance);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component instantiation arg descriptor");
                }
                err = wasi__component_instance_append_arg(&instance,
                                                          name,
                                                          wasi__extern_kind_from_byte(kind_byte),
                                                          index);
                if (err != WASI_OK) {
                    wasi__component_instance_release(&instance);
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "component instance arg alloc failed"
                                                                       : wasi_error_string(err));
                }
            }
        } else if (opcode == 0x01) {
            uint32_t export_count;
            uint32_t export_index;
            instance.kind = WASI_COMPONENT_INSTANCE_KIND_FROM_EXPORTS;
            export_count = wasi__read_leb128_u32(&reader);
            if (reader.malformed) {
                wasi__component_instance_release(&instance);
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component instance export count");
            }
            for (export_index = 0; export_index < export_count; export_index++) {
                char* name = NULL;
                uint8_t name_kind = 0;
                uint8_t kind_byte;
                uint32_t index;

                if (!wasi__read_component_name(&reader, &name_kind, &name)) {
                    wasi__component_instance_release(&instance);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component instance export name");
                }
                kind_byte = wasi__read_u8(&reader);
                index = wasi__read_leb128_u32(&reader);
                if (reader.malformed) {
                    WASM_FREE(name);
                    wasi__component_instance_release(&instance);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component instance export descriptor");
                }
                err = wasi__component_instance_append_export(&instance,
                                                            name,
                                                            name_kind,
                                                            wasi__extern_kind_from_byte(kind_byte),
                                                            index);
                if (err != WASI_OK) {
                    wasi__component_instance_release(&instance);
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "component instance export alloc failed"
                                                                       : wasi_error_string(err));
                }
            }
        } else {
            return wasi__set_errorf(engine,
                                    WASI_ERR_NOT_IMPLEMENTED,
                                    "component instance opcode 0x%02x not implemented",
                                    (unsigned)opcode);
        }

        err = wasi__component_append_instance(component, &instance);
        if (err != WASI_OK) {
            wasi__component_instance_release(&instance);
            return wasi__set_error_literal(engine,
                                           err,
                                           err == WASI_ERR_OOM ? "component instance alloc failed"
                                                               : wasi_error_string(err));
        }
    }

    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in component instance section");
    }
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_start(wasi_engine_t* engine,
                                                wasi_component_t* component,
                                                const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t func_index;
    uint32_t arg_count;
    uint32_t result_count;

    if (component->has_start) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "duplicate component start section");
    }

    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    func_index = wasi__read_leb128_u32(&reader);
    arg_count = wasi__read_leb128_u32(&reader);
    result_count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component start section");
    }
    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in component start section");
    }

    component->has_start = 1;
    component->start_func_index = func_index;
    component->start_arg_count = arg_count;
    component->start_result_count = result_count;
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_canon(wasi_engine_t* engine,
                                                wasi_component_t* component,
                                                const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canonical function section");
    }

    for (i = 0; i < count; i++) {
        uint8_t opcode = wasi__read_u8(&reader);
        wasi_component_canon_t canon;
        if (reader.malformed) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canonical function opcode");
        }
        memset(&canon, 0, sizeof(canon));
        canon.func_index = UINT32_MAX;
        canon.core_func_index = UINT32_MAX;
        canon.type_index = UINT32_MAX;
        canon.defined_func_index = UINT32_MAX;
        if (opcode == 0x00) {
            uint8_t async_flag = wasi__read_u8(&reader);
            uint32_t core_func_index = wasi__read_leb128_u32(&reader);
            uint32_t option_count = wasi__read_leb128_u32(&reader);
            uint32_t type_index;
            uint32_t opt;
            wasi_error_t err;
            canon.kind = WASI_COMPONENT_CANON_KIND_LIFT;
            canon.async_flag = async_flag;
            canon.core_func_index = core_func_index;
            if (reader.malformed) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canon lift header");
            }
            for (opt = 0; opt < option_count; opt++) {
                uint8_t option = wasi__read_u8(&reader);
                uint32_t immediate = 0;
                if (reader.malformed) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canon lift option");
                }
                switch (option) {
                    case 0x00:
                    case 0x01:
                    case 0x02:
                        break;
                    case 0x03:
                    case 0x04:
                    case 0x05:
                        immediate = wasi__read_leb128_u32(&reader);
                        if (reader.malformed) {
                            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canon lift option immediate");
                        }
                        break;
                    default:
                        return wasi__set_errorf(engine,
                                                WASI_ERR_NOT_IMPLEMENTED,
                                                "canonical option 0x%02x not implemented",
                                                (unsigned)option);
                }
                err = wasi__canon_append_option(&canon,
                                               option,
                                               wasi__canon_option_has_immediate(option),
                                               immediate);
                if (err != WASI_OK) {
                    WASM_FREE(canon.options);
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "canonical option alloc failed"
                                                                       : wasi_error_string(err));
                }
            }
            type_index = wasi__read_leb128_u32(&reader);
            if (reader.malformed) {
                WASM_FREE(canon.options);
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canon lift type index");
            }
            canon.has_type_index = 1;
            canon.type_index = type_index;
            canon.defined_func_index = component->num_funcs;
            err = wasi__component_append_func(component, type_index);
            if (err != WASI_OK) {
                WASM_FREE(canon.options);
                return wasi__set_error_literal(engine,
                                               err,
                                               err == WASI_ERR_OOM ? "component func alloc failed"
                                                                   : wasi_error_string(err));
            }
            err = wasi__component_append_canon(component, &canon);
            if (err != WASI_OK) {
                WASM_FREE(canon.options);
                return wasi__set_error_literal(engine,
                                               err,
                                               err == WASI_ERR_OOM ? "canonical record alloc failed"
                                                                   : wasi_error_string(err));
            }
        } else if (opcode == 0x01) {
            uint8_t async_flag = wasi__read_u8(&reader);
            uint32_t func_index = wasi__read_leb128_u32(&reader);
            uint32_t option_count = wasi__read_leb128_u32(&reader);
            uint32_t opt;
            wasi_error_t err;
            canon.kind = WASI_COMPONENT_CANON_KIND_LOWER;
            canon.async_flag = async_flag;
            canon.func_index = func_index;
            if (reader.malformed) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canon lower header");
            }
            for (opt = 0; opt < option_count; opt++) {
                uint8_t option = wasi__read_u8(&reader);
                uint32_t immediate = 0;
                if (reader.malformed) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canon lower option");
                }
                switch (option) {
                    case 0x00:
                    case 0x01:
                    case 0x02:
                        break;
                    case 0x03:
                    case 0x04:
                    case 0x05:
                        immediate = wasi__read_leb128_u32(&reader);
                        if (reader.malformed) {
                            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canon lower option immediate");
                        }
                        break;
                    default:
                        return wasi__set_errorf(engine,
                                                WASI_ERR_NOT_IMPLEMENTED,
                                                "canonical option 0x%02x not implemented",
                                                (unsigned)option);
                }
                err = wasi__canon_append_option(&canon,
                                               option,
                                               wasi__canon_option_has_immediate(option),
                                               immediate);
                if (err != WASI_OK) {
                    WASM_FREE(canon.options);
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "canonical option alloc failed"
                                                                       : wasi_error_string(err));
                }
            }
            err = wasi__component_append_canon(component, &canon);
            if (err != WASI_OK) {
                WASM_FREE(canon.options);
                return wasi__set_error_literal(engine,
                                               err,
                                               err == WASI_ERR_OOM ? "canonical record alloc failed"
                                                                   : wasi_error_string(err));
            }
        } else {
            return wasi__set_errorf(engine,
                                    WASI_ERR_NOT_IMPLEMENTED,
                                    "canonical opcode 0x%02x not implemented",
                                    (unsigned)opcode);
        }
    }

    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in canonical function section");
    }
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_exports(wasi_engine_t* engine,
                                                  wasi_component_t* component,
                                                  const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component export section");
    }

    for (i = 0; i < count; i++) {
        char* name = NULL;
        uint8_t name_kind = 0;
        uint8_t kind_byte;
        wasi_component_extern_kind_t kind;
        uint32_t index;
        uint8_t type_tag;
        uint32_t type_index = 0;
        int has_type = 0;
        wasi_error_t err;

        if (!wasi__read_component_name(&reader, &name_kind, &name)) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component export name");
        }

        kind_byte = wasi__read_u8(&reader);
        kind = wasi__extern_kind_from_byte(kind_byte);
        index = wasi__read_leb128_u32(&reader);
        type_tag = wasi__read_u8(&reader);
        if (reader.malformed) {
            WASM_FREE(name);
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component export descriptor");
        }
        if (type_tag != 0) {
            has_type = 1;
            type_index = wasi__read_leb128_u32(&reader);
            if (reader.malformed) {
                WASM_FREE(name);
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component export type annotation");
            }
        }

        err = wasi__component_append_export(component, name, name_kind, kind, index, has_type, type_index);
        if (err != WASI_OK) {
            return wasi__set_error_literal(engine,
                                           err,
                                           err == WASI_ERR_OOM ? "component export alloc failed"
                                                               : wasi_error_string(err));
        }
    }

    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in component export section");
    }
    return WASI_OK;
}

static wasi_error_t wasi__parse_component_interfaces(wasi_engine_t* engine, wasi_component_t* component) {
    uint32_t i;

    if (!engine || !component) {
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component missing state");
    }

    for (i = 0; i < component->num_sections; i++) {
        const wasi_component_section_t* section = &component->sections[i];
        wasi_error_t err = WASI_OK;

        if (section->id == 10)
            err = wasi__parse_component_imports(engine, component, section);
        else if (section->id == 2)
            err = wasi__parse_component_core_instances(engine, component, section);
        else if (section->id == 5)
            err = wasi__parse_component_instances(engine, component, section);
        else if (section->id == 9)
            err = wasi__parse_component_start(engine, component, section);
        else if (section->id == 11)
            err = wasi__parse_component_exports(engine, component, section);
        else if (section->id == 7)
            err = wasi__parse_component_types(engine, component, section);
        else if (section->id == 8)
            err = wasi__parse_component_canon(engine, component, section);
        else if (section->id == 6)
            err = wasi__parse_component_aliases(engine, component, section);

        if (err != WASI_OK) return err;
    }

    return WASI_OK;
}

static wasi_error_t wasi__extract_core_modules(wasi_engine_t* engine, wasi_component_t* component) {
    uint32_t i;

    if (!engine || !component) {
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component missing state");
    }

    for (i = 0; i < component->num_sections; i++) {
        const wasi_component_section_t* section = &component->sections[i];
        wasm_module_t* module;
        wasi_binary_kind_t binary_kind;
        uint8_t version[4];
        wasi_error_t err;

        if (section->id != 1) continue;

        err = wasi__classify_binary(component->bytes + section->payload_offset,
                                    section->payload_size,
                                    &binary_kind,
                                    version);
        if (err != WASI_OK || binary_kind != WASI_BINARY_KIND_CORE_MODULE) {
            return wasi__set_errorf(engine,
                                    WASI_ERR_MALFORMED,
                                    "embedded core module %u has invalid binary header",
                                    (unsigned)component->num_core_modules);
        }

        module = wasm_load(&engine->runtime,
                           component->bytes + section->payload_offset,
                           section->payload_size);
        if (!module) {
            const char* load_error_msg = engine->runtime.error_msg[0]
                ? engine->runtime.error_msg
                : wasm_error_string(engine->runtime.last_error);

            if (engine->runtime.last_error != WASM_ERR_UNKNOWN_IMPORT) {
                return wasi__set_errorf(engine,
                                        WASI_ERR_MALFORMED,
                                        "embedded core module %u failed to load: %s",
                                        (unsigned)component->num_core_modules,
                                        load_error_msg);
            }

            err = wasi__component_append_core_module(component,
                                                     section->payload_offset,
                                                     section->payload_size,
                                                     NULL,
                                                     engine->runtime.last_error,
                                                     load_error_msg);
            if (err != WASI_OK) {
                return wasi__set_error_literal(engine,
                                               err,
                                               err == WASI_ERR_OOM ? "embedded core module alloc failed"
                                                                   : wasi_error_string(err));
            }
            engine->runtime.last_error = WASM_OK;
            engine->runtime.error_msg[0] = '\0';
            continue;
        }

        err = wasi__component_append_core_module(component,
                                                 section->payload_offset,
                                                 section->payload_size,
                                                 module,
                                                 WASM_OK,
                                                 NULL);
        if (err != WASI_OK) {
            wasm_free_module(module);
            return wasi__set_error_literal(engine, err, err == WASI_ERR_OOM ? "embedded core module alloc failed" : wasi_error_string(err));
        }
    }

    return WASI_OK;
}

static wasi_error_t wasi__extract_nested_components(wasi_engine_t* engine, wasi_component_t* component) {
    uint32_t i;

    if (!engine || !component) {
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component missing state");
    }

    for (i = 0; i < component->num_sections; i++) {
        const wasi_component_section_t* section = &component->sections[i];
        wasi_component_t* nested_component;
        wasi_binary_kind_t binary_kind;
        uint8_t version[4];
        wasi_error_t err;

        if (section->id != 4) continue;

        err = wasi__classify_binary(component->bytes + section->payload_offset,
                                    section->payload_size,
                                    &binary_kind,
                                    version);
        if (err != WASI_OK || binary_kind != WASI_BINARY_KIND_COMPONENT) {
            return wasi__set_errorf(engine,
                                    WASI_ERR_MALFORMED,
                                    "nested component %u has invalid binary header",
                                    (unsigned)component->num_nested_components);
        }

        nested_component = wasi_load(engine,
                                     component->bytes + section->payload_offset,
                                     section->payload_size);
        if (!nested_component) {
            return wasi__set_errorf(engine,
                                    WASI_ERR_MALFORMED,
                                    "nested component %u failed to load: %s",
                                    (unsigned)component->num_nested_components,
                                    engine->error_msg[0] ? engine->error_msg : wasi_error_string(engine->last_error));
        }

        err = wasi__component_append_nested_component(component,
                                                      section->payload_offset,
                                                      section->payload_size,
                                                      nested_component);
        if (err != WASI_OK) {
            return wasi__set_error_literal(engine,
                                           err,
                                           err == WASI_ERR_OOM ? "nested component alloc failed"
                                                               : wasi_error_string(err));
        }
    }

    return WASI_OK;
}

static void wasi__appendf(char* buffer, size_t buffer_size, size_t* offset, const char* fmt, ...) {
    int written;
    va_list args;

    if (!buffer || buffer_size == 0 || !offset || !fmt) return;
    if (*offset >= buffer_size) return;

    va_start(args, fmt);
    written = vsnprintf(buffer + *offset, buffer_size - *offset, fmt, args);
    va_end(args);
    if (written < 0) {
        *offset = buffer_size;
        return;
    }

    if ((size_t)written >= buffer_size - *offset)
        *offset = buffer_size;
    else
        *offset += (size_t)written;
}

static wasi_error_t wasi__classify_binary(const uint8_t* bytes,
                                          size_t len,
                                          wasi_binary_kind_t* out_kind,
                                          uint8_t out_version[4]) {
    const uint8_t* version;

    if (out_kind) *out_kind = WASI_BINARY_KIND_UNKNOWN;
    if (out_version) memset(out_version, 0, 4);

    if (!bytes) return WASI_ERR_INVALID_ARGUMENT;
    if (len < 8) return WASI_ERR_MALFORMED;
    if (!wasi__has_magic(bytes, len)) return WASI_ERR_INVALID_MAGIC;

    version = bytes + 4;
    if (out_version) memcpy(out_version, version, 4);

    if (wasi__is_core_module_version(version)) {
        if (out_kind) *out_kind = WASI_BINARY_KIND_CORE_MODULE;
        return WASI_OK;
    }
    if (wasi__is_component_version(version)) {
        if (out_kind) *out_kind = WASI_BINARY_KIND_COMPONENT;
        return WASI_OK;
    }

    return WASI_ERR_INVALID_VERSION;
}

void wasi_config_default(wasi_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    wasm_config_default(&config->runtime_config);
    config->has_runtime_config = 0;
}

wasi_error_t wasi_init(wasi_engine_t* engine, const wasi_config_t* config) {
    wasm_error_t wasm_err;
    const wasm_config_t* runtime_config = NULL;

    if (!engine) return WASI_ERR_INVALID_ARGUMENT;

    memset(engine, 0, sizeof(*engine));
    if (config && config->has_runtime_config) runtime_config = &config->runtime_config;

    wasm_err = wasm_init(&engine->runtime, runtime_config);
    if (wasm_err != WASM_OK) {
        return wasi__set_errorf(engine,
                                WASI_ERR_RUNTIME_INIT,
                                "wasm_init failed: %s",
                                engine->runtime.error_msg[0]
                                    ? engine->runtime.error_msg
                                    : wasm_error_string(wasm_err));
    }

    wasi__clear_error(engine);
    return WASI_OK;
}

void wasi_destroy(wasi_engine_t* engine) {
    if (!engine) return;
    wasm_destroy(&engine->runtime);
    engine->last_error = WASI_OK;
    engine->error_msg[0] = '\0';
}

wasi_binary_kind_t wasi_detect_binary_kind(const uint8_t* bytes, size_t len) {
    wasi_binary_kind_t kind = WASI_BINARY_KIND_UNKNOWN;

    if (wasi__classify_binary(bytes, len, &kind, NULL) != WASI_OK) return WASI_BINARY_KIND_UNKNOWN;
    return kind;
}

wasi_component_t* wasi_load(wasi_engine_t* engine, const uint8_t* bytes, size_t len) {
    wasi_component_t* component;
    wasi_binary_kind_t kind = WASI_BINARY_KIND_UNKNOWN;
    uint8_t version[4];
    wasi_error_t err;

    if (!engine) return NULL;
    wasi__clear_error(engine);

    err = wasi__classify_binary(bytes, len, &kind, version);
    if (err != WASI_OK) {
        switch (err) {
            case WASI_ERR_INVALID_ARGUMENT:
                wasi__set_error_literal(engine, err, "bytes must not be NULL");
                break;
            case WASI_ERR_MALFORMED:
                wasi__set_error_literal(engine, err, "component binary too short");
                break;
            case WASI_ERR_INVALID_MAGIC:
                wasi__set_error_literal(engine, err, "bad wasm magic");
                break;
            case WASI_ERR_INVALID_VERSION:
                wasi__set_errorf(engine,
                                 err,
                                 "unsupported wasm binary version %u.%u.%u.%u",
                                 (unsigned)version[0],
                                 (unsigned)version[1],
                                 (unsigned)version[2],
                                 (unsigned)version[3]);
                break;
            default:
                wasi__set_error_literal(engine, err, wasi_error_string(err));
                break;
        }
        return NULL;
    }

    if (kind == WASI_BINARY_KIND_CORE_MODULE) {
        wasi__set_error_literal(engine,
                                WASI_ERR_UNSUPPORTED_BINARY,
                                "wasi_load expects a component binary; use wasm_load for core modules");
        return NULL;
    }

    component = (wasi_component_t*)WASM_CALLOC(1, sizeof(*component));
    if (!component) {
        wasi__set_error_literal(engine, WASI_ERR_OOM, "component alloc failed");
        return NULL;
    }

    component->bytes = (uint8_t*)WASM_MALLOC(len ? len : 1u);
    if (!component->bytes) {
        WASM_FREE(component);
        wasi__set_error_literal(engine, WASI_ERR_OOM, "component bytes alloc failed");
        return NULL;
    }

    memcpy(component->bytes, bytes, len);
    component->engine = engine;
    component->size = len;
    memcpy(component->version_bytes, version, sizeof(component->version_bytes));
    component->binary_kind = kind;
    component->status = WASI_COMPONENT_STATUS_UNPARSED;
    err = wasi__parse_component_sections(engine, component);
    if (err == WASI_OK) err = wasi__parse_component_interfaces(engine, component);
    if (err == WASI_OK) err = wasi__extract_core_modules(engine, component);
    if (err == WASI_OK) err = wasi__extract_nested_components(engine, component);
    if (err != WASI_OK) {
        wasi__component_release_storage(component);
        WASM_FREE(component);
        return NULL;
    }
    return component;
}

void wasi_free_component(wasi_component_t* component) {
    if (!component) return;
    wasi__component_release_storage(component);
    WASM_FREE(component);
}

wasi_binary_kind_t wasi_component_binary_kind(const wasi_component_t* component) {
    return component ? component->binary_kind : WASI_BINARY_KIND_UNKNOWN;
}

wasi_component_status_t wasi_component_status(const wasi_component_t* component) {
    return component ? component->status : WASI_COMPONENT_STATUS_UNPARSED;
}

uint8_t wasi_component_layer(const wasi_component_t* component) {
    if (!component) return 0;
    return component->version_bytes[0];
}

uint32_t wasi_component_section_count(const wasi_component_t* component) {
    return component ? component->num_sections : 0;
}

uint8_t wasi_component_section_id(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_sections) return 0;
    return component->sections[index].id;
}

size_t wasi_component_section_size(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_sections) return 0;
    return component->sections[index].payload_size;
}

const char* wasi_component_section_name(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_sections) return NULL;
    return component->sections[index].name;
}

uint32_t wasi_component_import_count(const wasi_component_t* component) {
    return component ? component->num_imports : 0;
}

uint32_t wasi_component_type_count(const wasi_component_t* component) {
    return component ? component->num_types : 0;
}

wasi_component_type_kind_t wasi_component_type_kind(const wasi_component_t* component, uint32_t type_index) {
    if (!component || type_index >= component->num_types) return WASI_COMPONENT_TYPE_KIND_UNKNOWN;
    return component->types[type_index].kind;
}

uint32_t wasi_component_func_type_param_count(const wasi_component_t* component, uint32_t type_index) {
    if (!component || type_index >= component->num_types) return 0;
    if (component->types[type_index].kind != WASI_COMPONENT_TYPE_KIND_FUNC) return 0;
    return component->types[type_index].func.num_params;
}

const char* wasi_component_func_type_param_name(const wasi_component_t* component, uint32_t type_index, uint32_t param_index) {
    if (!component || type_index >= component->num_types) return NULL;
    if (component->types[type_index].kind != WASI_COMPONENT_TYPE_KIND_FUNC) return NULL;
    if (param_index >= component->types[type_index].func.num_params) return NULL;
    return component->types[type_index].func.params[param_index].name;
}

uint8_t wasi_component_func_type_param_code(const wasi_component_t* component, uint32_t type_index, uint32_t param_index) {
    if (!component || type_index >= component->num_types) return 0;
    if (component->types[type_index].kind != WASI_COMPONENT_TYPE_KIND_FUNC) return 0;
    if (param_index >= component->types[type_index].func.num_params) return 0;
    return component->types[type_index].func.params[param_index].type_code;
}

uint32_t wasi_component_func_type_result_count(const wasi_component_t* component, uint32_t type_index) {
    if (!component || type_index >= component->num_types) return 0;
    if (component->types[type_index].kind != WASI_COMPONENT_TYPE_KIND_FUNC) return 0;
    return component->types[type_index].func.num_results;
}

const char* wasi_component_func_type_result_name(const wasi_component_t* component, uint32_t type_index, uint32_t result_index) {
    if (!component || type_index >= component->num_types) return NULL;
    if (component->types[type_index].kind != WASI_COMPONENT_TYPE_KIND_FUNC) return NULL;
    if (result_index >= component->types[type_index].func.num_results) return NULL;
    return component->types[type_index].func.results[result_index].name;
}

uint8_t wasi_component_func_type_result_code(const wasi_component_t* component, uint32_t type_index, uint32_t result_index) {
    if (!component || type_index >= component->num_types) return 0;
    if (component->types[type_index].kind != WASI_COMPONENT_TYPE_KIND_FUNC) return 0;
    if (result_index >= component->types[type_index].func.num_results) return 0;
    return component->types[type_index].func.results[result_index].type_code;
}

const char* wasi_component_import_name(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_imports) return NULL;
    return component->imports[index].name;
}

wasi_component_extern_kind_t wasi_component_import_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_imports) return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    return component->imports[index].kind;
}

uint32_t wasi_component_import_type_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_imports) return 0;
    return component->imports[index].type_index;
}

uint32_t wasi_component_export_count(const wasi_component_t* component) {
    return component ? component->num_exports : 0;
}

const char* wasi_component_export_name(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_exports) return NULL;
    return component->exports[index].name;
}

wasi_component_extern_kind_t wasi_component_export_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_exports) return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    return component->exports[index].kind;
}

uint32_t wasi_component_export_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_exports) return 0;
    return component->exports[index].index;
}

int wasi_component_export_has_type(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_exports) return 0;
    return component->exports[index].has_type;
}

uint32_t wasi_component_export_type_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_exports) return 0;
    return component->exports[index].type_index;
}

int wasi_component_export_func_type_index(const wasi_component_t* component, uint32_t index, uint32_t* out_type_index) {
    uint32_t func_index;

    if (out_type_index) *out_type_index = 0;
    if (!component || index >= component->num_exports || !out_type_index) return 0;
    if (component->exports[index].kind != WASI_COMPONENT_EXTERN_KIND_FUNC) return 0;
    if (component->exports[index].has_type) {
        *out_type_index = component->exports[index].type_index;
        return 1;
    }
    func_index = component->exports[index].index;
    if (func_index >= component->num_funcs) return 0;
    *out_type_index = component->funcs[func_index].type_index;
    return 1;
}

uint32_t wasi_component_alias_count(const wasi_component_t* component) {
    return component ? component->num_aliases : 0;
}

wasi_component_alias_kind_t wasi_component_alias_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return WASI_COMPONENT_ALIAS_KIND_UNKNOWN;
    return component->aliases[index].kind;
}

int wasi_component_alias_sort_is_core(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return 0;
    return component->aliases[index].sort_is_core;
}

uint8_t wasi_component_alias_sort_code(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return 0xFFu;
    return component->aliases[index].sort_code;
}

wasi_component_extern_kind_t wasi_component_alias_extern_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    return component->aliases[index].extern_kind;
}

wasm_export_kind_t wasi_component_alias_core_export_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return (wasm_export_kind_t)255;
    return component->aliases[index].core_export_kind;
}

uint32_t wasi_component_alias_instance_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return 0;
    return component->aliases[index].instance_index;
}

const char* wasi_component_alias_name(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return NULL;
    return component->aliases[index].name;
}

uint32_t wasi_component_alias_outer_count(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return UINT32_MAX;
    return component->aliases[index].outer_count;
}

uint32_t wasi_component_alias_outer_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return UINT32_MAX;
    return component->aliases[index].outer_index;
}

uint32_t wasi_component_core_instance_count(const wasi_component_t* component) {
    return component ? component->num_core_instances : 0;
}

wasi_component_core_instance_kind_t wasi_component_core_instance_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_instances) return WASI_COMPONENT_CORE_INSTANCE_KIND_UNKNOWN;
    return component->core_instances[index].kind;
}

uint32_t wasi_component_core_instance_module_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_instances) return UINT32_MAX;
    return component->core_instances[index].module_index;
}

uint32_t wasi_component_core_instance_export_count(const wasi_component_t* component, uint32_t instance_index) {
    if (!component || instance_index >= component->num_core_instances) return 0;
    return component->core_instances[instance_index].num_exports;
}

const char* wasi_component_core_instance_export_name(const wasi_component_t* component,
                                                     uint32_t instance_index,
                                                     uint32_t export_index) {
    if (!component || instance_index >= component->num_core_instances) return NULL;
    if (export_index >= component->core_instances[instance_index].num_exports) return NULL;
    return component->core_instances[instance_index].exports[export_index].name;
}

wasm_export_kind_t wasi_component_core_instance_export_kind(const wasi_component_t* component,
                                                            uint32_t instance_index,
                                                            uint32_t export_index) {
    if (!component || instance_index >= component->num_core_instances) return (wasm_export_kind_t)255;
    if (export_index >= component->core_instances[instance_index].num_exports) return (wasm_export_kind_t)255;
    return component->core_instances[instance_index].exports[export_index].kind;
}

uint32_t wasi_component_core_instance_export_index(const wasi_component_t* component,
                                                   uint32_t instance_index,
                                                   uint32_t export_index) {
    if (!component || instance_index >= component->num_core_instances) return UINT32_MAX;
    if (export_index >= component->core_instances[instance_index].num_exports) return UINT32_MAX;
    return component->core_instances[instance_index].exports[export_index].index;
}

uint32_t wasi_component_core_instance_arg_count(const wasi_component_t* component, uint32_t instance_index) {
    if (!component || instance_index >= component->num_core_instances) return 0;
    return component->core_instances[instance_index].num_args;
}

const char* wasi_component_core_instance_arg_name(const wasi_component_t* component,
                                                  uint32_t instance_index,
                                                  uint32_t arg_index) {
    if (!component || instance_index >= component->num_core_instances) return NULL;
    if (arg_index >= component->core_instances[instance_index].num_args) return NULL;
    return component->core_instances[instance_index].args[arg_index].name;
}

uint8_t wasi_component_core_instance_arg_kind(const wasi_component_t* component,
                                              uint32_t instance_index,
                                              uint32_t arg_index) {
    if (!component || instance_index >= component->num_core_instances) return 0xFFu;
    if (arg_index >= component->core_instances[instance_index].num_args) return 0xFFu;
    return component->core_instances[instance_index].args[arg_index].kind;
}

uint32_t wasi_component_core_instance_arg_index(const wasi_component_t* component,
                                                uint32_t instance_index,
                                                uint32_t arg_index) {
    if (!component || instance_index >= component->num_core_instances) return UINT32_MAX;
    if (arg_index >= component->core_instances[instance_index].num_args) return UINT32_MAX;
    return component->core_instances[instance_index].args[arg_index].index;
}

uint32_t wasi_component_instance_count(const wasi_component_t* component) {
    return component ? component->num_instances : 0;
}

wasi_component_instance_kind_t wasi_component_instance_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_instances) return WASI_COMPONENT_INSTANCE_KIND_UNKNOWN;
    return component->instances[index].kind;
}

uint32_t wasi_component_instance_component_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_instances) return UINT32_MAX;
    return component->instances[index].component_index;
}

uint32_t wasi_component_instance_export_count(const wasi_component_t* component, uint32_t instance_index) {
    if (!component || instance_index >= component->num_instances) return 0;
    return component->instances[instance_index].num_exports;
}

const char* wasi_component_instance_export_name(const wasi_component_t* component,
                                                uint32_t instance_index,
                                                uint32_t export_index) {
    if (!component || instance_index >= component->num_instances) return NULL;
    if (export_index >= component->instances[instance_index].num_exports) return NULL;
    return component->instances[instance_index].exports[export_index].name;
}

wasi_component_extern_kind_t wasi_component_instance_export_kind(const wasi_component_t* component,
                                                                 uint32_t instance_index,
                                                                 uint32_t export_index) {
    if (!component || instance_index >= component->num_instances) return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    if (export_index >= component->instances[instance_index].num_exports) return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    return component->instances[instance_index].exports[export_index].kind;
}

uint32_t wasi_component_instance_export_index(const wasi_component_t* component,
                                              uint32_t instance_index,
                                              uint32_t export_index) {
    if (!component || instance_index >= component->num_instances) return UINT32_MAX;
    if (export_index >= component->instances[instance_index].num_exports) return UINT32_MAX;
    return component->instances[instance_index].exports[export_index].index;
}

uint32_t wasi_component_instance_arg_count(const wasi_component_t* component, uint32_t instance_index) {
    if (!component || instance_index >= component->num_instances) return 0;
    return component->instances[instance_index].num_args;
}

const char* wasi_component_instance_arg_name(const wasi_component_t* component,
                                             uint32_t instance_index,
                                             uint32_t arg_index) {
    if (!component || instance_index >= component->num_instances) return NULL;
    if (arg_index >= component->instances[instance_index].num_args) return NULL;
    return component->instances[instance_index].args[arg_index].name;
}

wasi_component_extern_kind_t wasi_component_instance_arg_kind(const wasi_component_t* component,
                                                              uint32_t instance_index,
                                                              uint32_t arg_index) {
    if (!component || instance_index >= component->num_instances) return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    if (arg_index >= component->instances[instance_index].num_args) return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    return component->instances[instance_index].args[arg_index].kind;
}

uint32_t wasi_component_instance_arg_index(const wasi_component_t* component,
                                           uint32_t instance_index,
                                           uint32_t arg_index) {
    if (!component || instance_index >= component->num_instances) return UINT32_MAX;
    if (arg_index >= component->instances[instance_index].num_args) return UINT32_MAX;
    return component->instances[instance_index].args[arg_index].index;
}

int wasi_component_has_start(const wasi_component_t* component) {
    return component ? component->has_start : 0;
}

uint32_t wasi_component_start_func_index(const wasi_component_t* component) {
    if (!component || !component->has_start) return UINT32_MAX;
    return component->start_func_index;
}

uint32_t wasi_component_start_arg_count(const wasi_component_t* component) {
    if (!component || !component->has_start) return UINT32_MAX;
    return component->start_arg_count;
}

uint32_t wasi_component_start_result_count(const wasi_component_t* component) {
    if (!component || !component->has_start) return UINT32_MAX;
    return component->start_result_count;
}

uint32_t wasi_component_canon_count(const wasi_component_t* component) {
    return component ? component->num_canons : 0;
}

wasi_component_canon_kind_t wasi_component_canon_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_canons) return WASI_COMPONENT_CANON_KIND_UNKNOWN;
    return component->canons[index].kind;
}

uint8_t wasi_component_canon_async_flag(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_canons) return 0;
    return component->canons[index].async_flag;
}

uint32_t wasi_component_canon_func_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_canons) return UINT32_MAX;
    return component->canons[index].func_index;
}

uint32_t wasi_component_canon_core_func_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_canons) return UINT32_MAX;
    return component->canons[index].core_func_index;
}

int wasi_component_canon_has_type_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_canons) return 0;
    return component->canons[index].has_type_index;
}

uint32_t wasi_component_canon_type_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_canons) return UINT32_MAX;
    return component->canons[index].type_index;
}

uint32_t wasi_component_canon_defined_func_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_canons) return UINT32_MAX;
    return component->canons[index].defined_func_index;
}

uint32_t wasi_component_canon_option_count(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_canons) return 0;
    return component->canons[index].num_options;
}

uint8_t wasi_component_canon_option_code(const wasi_component_t* component, uint32_t canon_index, uint32_t option_index) {
    if (!component || canon_index >= component->num_canons) return 0;
    if (option_index >= component->canons[canon_index].num_options) return 0;
    return component->canons[canon_index].options[option_index].code;
}

int wasi_component_canon_option_has_index(const wasi_component_t* component, uint32_t canon_index, uint32_t option_index) {
    if (!component || canon_index >= component->num_canons) return 0;
    if (option_index >= component->canons[canon_index].num_options) return 0;
    return component->canons[canon_index].options[option_index].has_index;
}

uint32_t wasi_component_canon_option_index(const wasi_component_t* component, uint32_t canon_index, uint32_t option_index) {
    if (!component || canon_index >= component->num_canons) return 0;
    if (option_index >= component->canons[canon_index].num_options) return 0;
    return component->canons[canon_index].options[option_index].index;
}

uint32_t wasi_component_core_module_count(const wasi_component_t* component) {
    return component ? component->num_core_modules : 0;
}

wasm_module_t* wasi_component_core_module_at(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_modules) return NULL;
    return component->core_modules[index].module;
}

wasm_error_t wasi_component_core_module_error(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_modules) return WASM_OK;
    return component->core_modules[index].load_error;
}

const char* wasi_component_core_module_error_message(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_modules) return NULL;
    return component->core_modules[index].load_error_msg[0] ? component->core_modules[index].load_error_msg : NULL;
}

uint32_t wasi_component_nested_component_count(const wasi_component_t* component) {
    return component ? component->num_nested_components : 0;
}

const wasi_component_t* wasi_component_nested_component_at(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_nested_components) return NULL;
    return component->nested_components[index].component;
}

size_t wasi_component_nested_component_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_nested_components) return 0;
    return component->nested_components[index].payload_offset;
}

size_t wasi_component_nested_component_size(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_nested_components) return 0;
    return component->nested_components[index].payload_size;
}

void wasi_dump_component(const wasi_component_t* component, char* buffer, size_t buffer_size) {
    size_t offset = 0;
    uint32_t i;

    if (!buffer || buffer_size == 0) return;
    buffer[0] = '\0';
    if (!component) {
        wasi__appendf(buffer, buffer_size, &offset, "component=<null>\n");
        return;
    }

    wasi__appendf(buffer,
                  buffer_size,
                  &offset,
                  "kind=%s layer=0x%02x status=%s sections=%u core-modules=%u\n",
                  component->binary_kind == WASI_BINARY_KIND_COMPONENT ? "component"
                  : component->binary_kind == WASI_BINARY_KIND_CORE_MODULE ? "core"
                  : "unknown",
                  (unsigned)wasi_component_layer(component),
                  wasi_component_status_string(component->status),
                  (unsigned)component->num_sections,
                  (unsigned)component->num_core_modules);

    for (i = 0; i < component->num_sections; i++) {
        const wasi_component_section_t* section = &component->sections[i];
        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "section[%u]: id=%u kind=%s offset=%llu size=%llu",
                      (unsigned)i,
                      (unsigned)section->id,
                      wasi__component_section_kind_name(section->id),
                      (unsigned long long)section->section_offset,
                      (unsigned long long)section->payload_size);
        if (section->name && section->name[0])
            wasi__appendf(buffer, buffer_size, &offset, " custom=%s", section->name);
        wasi__appendf(buffer, buffer_size, &offset, "\n");
    }

    for (i = 0; i < component->num_imports; i++) {
        const wasi_component_import_t* import = &component->imports[i];
        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "import[%u]: name=%s kind=%s type=%u\n",
                      (unsigned)i,
                      import->name ? import->name : "",
                      wasi_component_extern_kind_string(import->kind),
                      (unsigned)import->type_index);
    }

    for (i = 0; i < component->num_exports; i++) {
        const wasi_component_export_t* export_ = &component->exports[i];
        uint32_t resolved_type_index = 0;
        int has_resolved_type = wasi_component_export_func_type_index(component, i, &resolved_type_index);
        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "export[%u]: name=%s kind=%s index=%u",
                      (unsigned)i,
                      export_->name ? export_->name : "",
                      wasi_component_extern_kind_string(export_->kind),
                      (unsigned)export_->index);
        if (export_->has_type)
            wasi__appendf(buffer, buffer_size, &offset, " type=%u", (unsigned)export_->type_index);
        else if (has_resolved_type)
            wasi__appendf(buffer, buffer_size, &offset, " resolved-type=%u", (unsigned)resolved_type_index);
        wasi__appendf(buffer, buffer_size, &offset, "\n");
    }

    for (i = 0; i < component->num_aliases; i++) {
        const wasi_component_alias_t* alias = &component->aliases[i];
        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "alias[%u]: kind=%s target=%s",
                      (unsigned)i,
                      wasi_component_alias_kind_string(alias->kind),
                      wasi__alias_sort_string(alias->sort_is_core, alias->sort_code));
        if (alias->kind == WASI_COMPONENT_ALIAS_KIND_CORE_INSTANCE_EXPORT ||
            alias->kind == WASI_COMPONENT_ALIAS_KIND_INSTANCE_EXPORT) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " instance=%u name=%s",
                          (unsigned)alias->instance_index,
                          alias->name ? alias->name : "");
        } else if (alias->kind == WASI_COMPONENT_ALIAS_KIND_OUTER) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " count=%u index=%u",
                          (unsigned)alias->outer_count,
                          (unsigned)alias->outer_index);
        }
        wasi__appendf(buffer, buffer_size, &offset, "\n");
    }

    for (i = 0; i < component->num_core_instances; i++) {
        const wasi_component_core_instance_t* instance = &component->core_instances[i];
        uint32_t j;

        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "core-instance[%u]: kind=%s",
                      (unsigned)i,
                      wasi_component_core_instance_kind_string(instance->kind));
        if (instance->kind == WASI_COMPONENT_CORE_INSTANCE_KIND_INSTANTIATE) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " module=%u args=%u",
                          (unsigned)instance->module_index,
                          (unsigned)instance->num_args);
        } else if (instance->kind == WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " exports=%u",
                          (unsigned)instance->num_exports);
        }
        wasi__appendf(buffer, buffer_size, &offset, "\n");

        for (j = 0; j < instance->num_exports; j++) {
            const wasi_component_core_instance_export_t* export_ = &instance->exports[j];
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          "  core-instance-export[%u,%u]: name=%s kind=%s index=%u\n",
                          (unsigned)i,
                          (unsigned)j,
                          export_->name ? export_->name : "",
                          wasi_component_core_export_kind_string(export_->kind),
                          (unsigned)export_->index);
        }

        for (j = 0; j < instance->num_args; j++) {
            const wasi_component_core_instantiation_arg_t* arg = &instance->args[j];
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          "  core-instance-arg[%u,%u]: name=%s kind=%s index=%u\n",
                          (unsigned)i,
                          (unsigned)j,
                          arg->name ? arg->name : "",
                          wasi_component_core_sort_string(arg->kind),
                          (unsigned)arg->index);
        }
    }

    for (i = 0; i < component->num_instances; i++) {
        const wasi_component_instance_t* instance = &component->instances[i];
        uint32_t j;

        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "component-instance[%u]: kind=%s",
                      (unsigned)i,
                      wasi_component_instance_kind_string(instance->kind));
        if (instance->kind == WASI_COMPONENT_INSTANCE_KIND_INSTANTIATE) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " component=%u args=%u",
                          (unsigned)instance->component_index,
                          (unsigned)instance->num_args);
        } else if (instance->kind == WASI_COMPONENT_INSTANCE_KIND_FROM_EXPORTS) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " exports=%u",
                          (unsigned)instance->num_exports);
        }
        wasi__appendf(buffer, buffer_size, &offset, "\n");

        for (j = 0; j < instance->num_exports; j++) {
            const wasi_component_instance_export_t* export_ = &instance->exports[j];
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          "  component-instance-export[%u,%u]: name=%s kind=%s index=%u\n",
                          (unsigned)i,
                          (unsigned)j,
                          export_->name ? export_->name : "",
                          wasi_component_extern_kind_string(export_->kind),
                          (unsigned)export_->index);
        }

        for (j = 0; j < instance->num_args; j++) {
            const wasi_component_instantiation_arg_t* arg = &instance->args[j];
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          "  component-instance-arg[%u,%u]: name=%s kind=%s index=%u\n",
                          (unsigned)i,
                          (unsigned)j,
                          arg->name ? arg->name : "",
                          wasi_component_extern_kind_string(arg->kind),
                          (unsigned)arg->index);
        }
    }

    if (component->has_start) {
        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "start: func=%u args=%u results=%u\n",
                      (unsigned)component->start_func_index,
                      (unsigned)component->start_arg_count,
                      (unsigned)component->start_result_count);
    }

    for (i = 0; i < component->num_canons; i++) {
        const wasi_component_canon_t* canon = &component->canons[i];
        uint32_t opt;
        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "canon[%u]: kind=%s async=%u",
                      (unsigned)i,
                      wasi_component_canon_kind_string(canon->kind),
                      (unsigned)canon->async_flag);
        if (canon->kind == WASI_COMPONENT_CANON_KIND_LIFT) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " core-func=%u type=%u defined-func=%u",
                          (unsigned)canon->core_func_index,
                          (unsigned)canon->type_index,
                          (unsigned)canon->defined_func_index);
        } else if (canon->kind == WASI_COMPONENT_CANON_KIND_LOWER) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " func=%u",
                          (unsigned)canon->func_index);
        }
        if (canon->num_options)
            wasi__appendf(buffer, buffer_size, &offset, " options=%u", (unsigned)canon->num_options);
        wasi__appendf(buffer, buffer_size, &offset, "\n");
        for (opt = 0; opt < canon->num_options; opt++) {
            const wasi_component_canon_option_t* option = &canon->options[opt];
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          "  canon-option[%u,%u]: code=0x%02x",
                          (unsigned)i,
                          (unsigned)opt,
                          (unsigned)option->code);
            if (option->has_index)
                wasi__appendf(buffer, buffer_size, &offset, " index=%u", (unsigned)option->index);
            wasi__appendf(buffer, buffer_size, &offset, "\n");
        }
    }

    for (i = 0; i < component->num_core_modules; i++) {
        const wasi_component_core_module_t* core_module = &component->core_modules[i];

        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "core-module[%u]: offset=%llu size=%llu status=%s",
                      (unsigned)i,
                      (unsigned long long)core_module->payload_offset,
                      (unsigned long long)core_module->payload_size,
                      core_module->module ? "loaded" : "unresolved");
        if (core_module->load_error_msg[0])
            wasi__appendf(buffer, buffer_size, &offset, " error=%s", core_module->load_error_msg);
        wasi__appendf(buffer, buffer_size, &offset, "\n");
    }

    for (i = 0; i < component->num_nested_components; i++) {
        const wasi_component_nested_component_t* nested = &component->nested_components[i];

        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "nested-component[%u]: offset=%llu size=%llu sections=%u status=%s\n",
                      (unsigned)i,
                      (unsigned long long)nested->payload_offset,
                      (unsigned long long)nested->payload_size,
                      (unsigned)(nested->component ? nested->component->num_sections : 0u),
                      nested->component ? wasi_component_status_string(nested->component->status) : "missing");
    }
}

const char* wasi_component_extern_kind_string(wasi_component_extern_kind_t kind) {
    switch (kind) {
        case WASI_COMPONENT_EXTERN_KIND_MODULE:
            return "module";
        case WASI_COMPONENT_EXTERN_KIND_FUNC:
            return "func";
        case WASI_COMPONENT_EXTERN_KIND_VALUE:
            return "value";
        case WASI_COMPONENT_EXTERN_KIND_TYPE:
            return "type";
        case WASI_COMPONENT_EXTERN_KIND_COMPONENT:
            return "component";
        case WASI_COMPONENT_EXTERN_KIND_INSTANCE:
            return "instance";
        default:
            return "unknown";
    }
}

const char* wasi_component_type_kind_string(wasi_component_type_kind_t kind) {
    switch (kind) {
        case WASI_COMPONENT_TYPE_KIND_FUNC:
            return "func";
        default:
            return "unknown";
    }
}

const char* wasi_component_primitive_type_string(uint8_t type_code) {
    switch (type_code) {
        case 0x7F:
            return "bool";
        case 0x7E:
            return "s8";
        case 0x7D:
            return "u8";
        case 0x7C:
            return "s16";
        case 0x7B:
            return "u16";
        case 0x7A:
            return "s32";
        case 0x79:
            return "u32";
        case 0x78:
            return "s64";
        case 0x77:
            return "u64";
        case 0x76:
            return "f32";
        case 0x75:
            return "f64";
        case 0x74:
            return "char";
        case 0x73:
            return "string";
        default:
            return "unknown";
    }
}

const char* wasi_component_alias_kind_string(wasi_component_alias_kind_t kind) {
    switch (kind) {
        case WASI_COMPONENT_ALIAS_KIND_CORE_INSTANCE_EXPORT:
            return "core-instance-export";
        case WASI_COMPONENT_ALIAS_KIND_INSTANCE_EXPORT:
            return "instance-export";
        case WASI_COMPONENT_ALIAS_KIND_OUTER:
            return "outer";
        default:
            return "unknown";
    }
}

const char* wasi_component_core_instance_kind_string(wasi_component_core_instance_kind_t kind) {
    switch (kind) {
        case WASI_COMPONENT_CORE_INSTANCE_KIND_INSTANTIATE:
            return "instantiate";
        case WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS:
            return "from-exports";
        default:
            return "unknown";
    }
}

const char* wasi_component_instance_kind_string(wasi_component_instance_kind_t kind) {
    switch (kind) {
        case WASI_COMPONENT_INSTANCE_KIND_INSTANTIATE:
            return "instantiate";
        case WASI_COMPONENT_INSTANCE_KIND_FROM_EXPORTS:
            return "from-exports";
        default:
            return "unknown";
    }
}

const char* wasi_component_core_sort_string(uint8_t sort) {
    switch (sort) {
        case 0x00:
            return "func";
        case 0x01:
            return "table";
        case 0x02:
            return "memory";
        case 0x03:
            return "global";
        case 0x10:
            return "type";
        case 0x11:
            return "module";
        case 0x12:
            return "instance";
        case 0x13:
            return "component";
        default:
            return "unknown";
    }
}

const char* wasi_component_canon_kind_string(wasi_component_canon_kind_t kind) {
    switch (kind) {
        case WASI_COMPONENT_CANON_KIND_LIFT:
            return "lift";
        case WASI_COMPONENT_CANON_KIND_LOWER:
            return "lower";
        default:
            return "unknown";
    }
}

const char* wasi_component_core_export_kind_string(wasm_export_kind_t kind) {
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

const char* wasi_error_string(wasi_error_t err) {
    switch (err) {
        case WASI_OK:
            return "ok";
        case WASI_ERR_INVALID_ARGUMENT:
            return "invalid argument";
        case WASI_ERR_INVALID_MAGIC:
            return "invalid magic number";
        case WASI_ERR_INVALID_VERSION:
            return "invalid version";
        case WASI_ERR_MALFORMED:
            return "malformed binary";
        case WASI_ERR_UNSUPPORTED_BINARY:
            return "unsupported binary kind";
        case WASI_ERR_NOT_IMPLEMENTED:
            return "not implemented";
        case WASI_ERR_OOM:
            return "out of memory";
        case WASI_ERR_RUNTIME_INIT:
            return "runtime init failed";
        default:
            return "unknown error";
    }
}

const char* wasi_component_status_string(wasi_component_status_t status) {
    switch (status) {
        case WASI_COMPONENT_STATUS_UNPARSED:
            return "component parser not implemented yet";
        case WASI_COMPONENT_STATUS_PARSED_SECTIONS:
            return "component section framing parsed; semantic parser not implemented";
        default:
            return "unknown component status";
    }
}

#endif

#endif
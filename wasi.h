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

typedef struct wasi_config_t {
    wasm_config_t runtime_config;
    int has_runtime_config;
} wasi_config_t;

typedef struct wasi_engine_t {
    wasm_runtime_t runtime;
    wasi_error_t last_error;
    char error_msg[256];
} wasi_engine_t;

typedef struct wasi_component_t {
    wasi_engine_t* engine;
    uint8_t* bytes;
    size_t size;
    uint8_t version_bytes[4];
    wasi_binary_kind_t binary_kind;
    wasi_component_status_t status;
    wasi_component_section_t* sections;
    uint32_t num_sections;
    uint32_t sections_capacity;
    wasi_component_import_t* imports;
    uint32_t num_imports;
    uint32_t imports_capacity;
    wasi_component_export_t* exports;
    uint32_t num_exports;
    uint32_t exports_capacity;
    wasi_component_core_module_t* core_modules;
    uint32_t num_core_modules;
    uint32_t core_modules_capacity;
} wasi_component_t;

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
uint32_t wasi_component_core_module_count(const wasi_component_t* component);
wasm_module_t* wasi_component_core_module_at(const wasi_component_t* component, uint32_t index);
wasm_error_t wasi_component_core_module_error(const wasi_component_t* component, uint32_t index);
const char* wasi_component_core_module_error_message(const wasi_component_t* component, uint32_t index);
void wasi_dump_component(const wasi_component_t* component, char* buffer, size_t buffer_size);
const char* wasi_component_extern_kind_string(wasi_component_extern_kind_t kind);
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

static void wasi__component_release_storage(wasi_component_t* component) {
    uint32_t i;

    if (!component) return;

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
    }

    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in component import section");
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
        else if (section->id == 11)
            err = wasi__parse_component_exports(engine, component, section);

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
        wasi__appendf(buffer, buffer_size, &offset, "\n");
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
/*
 * wasi.h — Experimental single-header component runtime scaffold for C99
 *
 * USAGE:
 *   #define WASI_IMPL
 *   #include "wasi.h"
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
typedef struct wasi_component_valtype_t wasi_component_valtype_t;
typedef struct wasi_component_type_decl_t wasi_component_type_decl_t;
typedef struct wasi_component_core_type_t wasi_component_core_type_t;
typedef struct wasi_component_core_module_decl_t wasi_component_core_module_decl_t;

typedef enum wasi_error_t {
    WASI_OK = 0,
    WASI_ERR_INVALID_ARGUMENT,
    WASI_ERR_INVALID_MAGIC,
    WASI_ERR_INVALID_VERSION,
    WASI_ERR_MALFORMED,
    WASI_ERR_UNSUPPORTED_BINARY,
    WASI_ERR_NOT_IMPLEMENTED,
    WASI_ERR_TYPE_MISMATCH,
    WASI_ERR_UNDEFINED_EXPORT,
    WASI_ERR_RUNTIME,
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
    WASI_COMPONENT_STATUS_PARSED_STRUCTURE,
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
    WASI_COMPONENT_TYPE_KIND_DEFINED,
    WASI_COMPONENT_TYPE_KIND_FUNC,
    WASI_COMPONENT_TYPE_KIND_COMPONENT,
    WASI_COMPONENT_TYPE_KIND_INSTANCE,
    WASI_COMPONENT_TYPE_KIND_RESOURCE,
} wasi_component_type_kind_t;

typedef enum wasi_component_type_decl_kind_t {
    WASI_COMPONENT_TYPE_DECL_KIND_UNKNOWN = 0,
    WASI_COMPONENT_TYPE_DECL_KIND_CORE_TYPE,
    WASI_COMPONENT_TYPE_DECL_KIND_TYPE,
    WASI_COMPONENT_TYPE_DECL_KIND_ALIAS,
    WASI_COMPONENT_TYPE_DECL_KIND_IMPORT,
    WASI_COMPONENT_TYPE_DECL_KIND_EXPORT,
} wasi_component_type_decl_kind_t;

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

typedef enum wasi_component_core_type_kind_t {
    WASI_COMPONENT_CORE_TYPE_KIND_UNKNOWN = 0,
    WASI_COMPONENT_CORE_TYPE_KIND_MODULE,
    WASI_COMPONENT_CORE_TYPE_KIND_TYPE,
} wasi_component_core_type_kind_t;

typedef enum wasi_component_core_module_decl_kind_t {
    WASI_COMPONENT_CORE_MODULE_DECL_KIND_UNKNOWN = 0,
    WASI_COMPONENT_CORE_MODULE_DECL_KIND_IMPORT,
    WASI_COMPONENT_CORE_MODULE_DECL_KIND_TYPE,
    WASI_COMPONENT_CORE_MODULE_DECL_KIND_ALIAS,
    WASI_COMPONENT_CORE_MODULE_DECL_KIND_EXPORT,
} wasi_component_core_module_decl_kind_t;

typedef enum wasi_component_instance_kind_t {
    WASI_COMPONENT_INSTANCE_KIND_UNKNOWN = 0,
    WASI_COMPONENT_INSTANCE_KIND_INSTANTIATE,
    WASI_COMPONENT_INSTANCE_KIND_FROM_EXPORTS,
} wasi_component_instance_kind_t;

typedef enum wasi_component_canon_kind_t {
    WASI_COMPONENT_CANON_KIND_UNKNOWN = 0,
    WASI_COMPONENT_CANON_KIND_LIFT,
    WASI_COMPONENT_CANON_KIND_LOWER,
    WASI_COMPONENT_CANON_KIND_RESOURCE_NEW,
    WASI_COMPONENT_CANON_KIND_RESOURCE_DROP,
    WASI_COMPONENT_CANON_KIND_RESOURCE_REP,
    WASI_COMPONENT_CANON_KIND_BACKPRESSURE_SET,
    WASI_COMPONENT_CANON_KIND_TASK_RETURN,
    WASI_COMPONENT_CANON_KIND_TASK_CANCEL,
    WASI_COMPONENT_CANON_KIND_CONTEXT_GET,
    WASI_COMPONENT_CANON_KIND_CONTEXT_SET,
    WASI_COMPONENT_CANON_KIND_TASK_YIELD,
    WASI_COMPONENT_CANON_KIND_SUBTASK_CANCEL,
    WASI_COMPONENT_CANON_KIND_SUBTASK_DROP,
    WASI_COMPONENT_CANON_KIND_STREAM_NEW,
    WASI_COMPONENT_CANON_KIND_STREAM_READ,
    WASI_COMPONENT_CANON_KIND_STREAM_WRITE,
    WASI_COMPONENT_CANON_KIND_STREAM_CANCEL_READ,
    WASI_COMPONENT_CANON_KIND_STREAM_CANCEL_WRITE,
    WASI_COMPONENT_CANON_KIND_STREAM_DROP_READABLE,
    WASI_COMPONENT_CANON_KIND_STREAM_DROP_WRITABLE,
    WASI_COMPONENT_CANON_KIND_FUTURE_NEW,
    WASI_COMPONENT_CANON_KIND_FUTURE_READ,
    WASI_COMPONENT_CANON_KIND_FUTURE_WRITE,
    WASI_COMPONENT_CANON_KIND_FUTURE_CANCEL_READ,
    WASI_COMPONENT_CANON_KIND_FUTURE_CANCEL_WRITE,
    WASI_COMPONENT_CANON_KIND_FUTURE_DROP_READABLE,
    WASI_COMPONENT_CANON_KIND_FUTURE_DROP_WRITABLE,
    WASI_COMPONENT_CANON_KIND_ERROR_CONTEXT_NEW,
    WASI_COMPONENT_CANON_KIND_ERROR_CONTEXT_DEBUG_MESSAGE,
    WASI_COMPONENT_CANON_KIND_ERROR_CONTEXT_DROP,
    WASI_COMPONENT_CANON_KIND_WAITABLE_SET_NEW,
    WASI_COMPONENT_CANON_KIND_WAITABLE_SET_WAIT,
    WASI_COMPONENT_CANON_KIND_WAITABLE_SET_POLL,
    WASI_COMPONENT_CANON_KIND_WAITABLE_SET_DROP,
    WASI_COMPONENT_CANON_KIND_WAITABLE_JOIN,
    WASI_COMPONENT_CANON_KIND_BACKPRESSURE_INC,
    WASI_COMPONENT_CANON_KIND_BACKPRESSURE_DEC,
} wasi_component_canon_kind_t;

typedef struct wasi_component_named_type_t {
    char* name;
    uint8_t type_code;
    wasi_component_valtype_t* type;
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
    size_t offset;
    wasi_component_type_kind_t kind;
    uint8_t opcode;
    uint8_t async_flag;
    uint8_t detail_opcode;
    uint32_t item_count;
    int has_primary_index;
    uint32_t primary_index;
    int has_secondary_index;
    uint32_t secondary_index;
    wasi_component_func_type_t func;
    wasi_component_valtype_t* defined_type;
    wasi_component_type_decl_t* decls;
    uint32_t num_decls;
    uint32_t decls_capacity;
} wasi_component_type_t;

typedef struct wasi_component_func_t {
    size_t offset;
    uint32_t type_index;
} wasi_component_func_t;

typedef struct wasi_component_import_t {
    size_t offset;
    char* name;
    char* interface_name;
    char* interface_version;
    uint8_t name_kind;
    wasi_component_extern_kind_t kind;
    uint32_t type_index;
} wasi_component_import_t;

typedef struct wasi_component_export_t {
    size_t offset;
    char* name;
    char* interface_name;
    char* interface_version;
    uint8_t name_kind;
    wasi_component_extern_kind_t kind;
    uint32_t index;
    int has_type;
    uint32_t type_index;
} wasi_component_export_t;

typedef struct wasi_component_alias_t {
    size_t offset;
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
    size_t offset;
    wasi_component_core_instance_kind_t kind;
    uint32_t module_index;
    wasi_component_core_instance_export_t* exports;
    uint32_t num_exports;
    uint32_t exports_capacity;
    wasi_component_core_instantiation_arg_t* args;
    uint32_t num_args;
    uint32_t args_capacity;
} wasi_component_core_instance_t;

typedef struct wasi_component_core_valtype_t {
    wasm_valtype_t type;
    wasm_reftype_t reftype;
} wasi_component_core_valtype_t;

typedef struct wasi_component_core_limits_t {
    uint8_t flags;
    int has_max;
    int is_64;
    int is_shared;
    uint64_t min_size;
    uint64_t max_size;
} wasi_component_core_limits_t;

typedef struct wasi_component_core_table_type_t {
    wasi_component_core_limits_t limits;
    wasm_reftype_t reftype;
} wasi_component_core_table_type_t;

typedef struct wasi_component_core_memory_type_t {
    wasi_component_core_limits_t limits;
} wasi_component_core_memory_type_t;

typedef struct wasi_component_core_global_type_t {
    wasi_component_core_valtype_t value_type;
    int is_mutable;
} wasi_component_core_global_type_t;

typedef struct wasi_component_core_tag_type_t {
    uint8_t attribute;
    uint32_t type_index;
} wasi_component_core_tag_type_t;

typedef struct wasi_component_core_externtype_t {
    wasm_export_kind_t kind;
    union {
        uint32_t func_type_index;
        wasi_component_core_table_type_t table;
        wasi_component_core_memory_type_t memory;
        wasi_component_core_global_type_t global;
        wasi_component_core_tag_type_t tag;
    } of;
} wasi_component_core_externtype_t;

typedef struct wasi_component_core_module_alias_t {
    uint8_t sort_code;
    uint8_t alias_kind;
    uint32_t outer_count;
    uint32_t outer_index;
} wasi_component_core_module_alias_t;

struct wasi_component_core_module_decl_t {
    size_t offset;
    wasi_component_core_module_decl_kind_t kind;
    char* module_name;
    char* name;
    union {
        wasi_component_core_externtype_t externtype;
        wasi_component_core_type_t* type;
        wasi_component_core_module_alias_t alias;
    } data;
};

struct wasi_component_core_type_t {
    size_t offset;
    wasi_component_core_type_kind_t kind;
    uint8_t opcode;
    uint8_t detail_opcode;
    uint32_t item_count;
    int has_primary_index;
    uint32_t primary_index;
    int is_final;
    uint32_t num_supertypes;
    uint32_t* supertypes;
    union {
        wasm_functype_t func;
        wasm_structtype_t struct_;
        wasm_arraytype_t array;
        struct {
            uint32_t type_index;
        } cont;
        struct {
            wasi_component_core_module_decl_t* decls;
            uint32_t num_decls;
            uint32_t decls_capacity;
        } module;
        struct {
            wasi_component_core_type_t** entries;
            uint32_t num_entries;
        } rec_group;
    } data;
};

typedef struct wasi_component_externdesc_t {
    wasi_component_extern_kind_t kind;
    uint8_t raw_kind;
    uint8_t bound_tag;
    int has_type_index;
    uint32_t type_index;
    wasi_component_valtype_t* value_type;
} wasi_component_externdesc_t;

struct wasi_component_type_decl_t {
    size_t offset;
    wasi_component_type_decl_kind_t kind;
    char* name;
    char* interface_name;
    char* interface_version;
    uint8_t name_kind;
    union {
        wasi_component_core_type_t core_type;
        wasi_component_type_t type;
        wasi_component_alias_t alias;
        wasi_component_externdesc_t externdesc;
    } data;
};

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
    size_t offset;
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
    size_t offset;
    wasi_component_canon_kind_t kind;
    uint8_t opcode;
    uint8_t async_flag;
    uint8_t flag;
    uint32_t func_index;
    uint32_t core_func_index;
    int has_type_index;
    uint32_t type_index;
    uint32_t defined_func_index;
    uint32_t item_count;
    int has_secondary_index;
    uint32_t secondary_index;
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
    wasi_component_core_type_t* core_types;
    uint32_t num_core_types;
    uint32_t core_types_capacity;
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
    size_t start_offset;
    uint32_t start_func_index;
    uint32_t start_arg_count;
    uint32_t start_result_count;
};

typedef struct wasi_instance_t wasi_instance_t;

typedef enum wasi_string_encoding_t {
    WASI_STRING_ENCODING_UTF8 = 0,
    WASI_STRING_ENCODING_UTF16,
    WASI_STRING_ENCODING_LATIN1_UTF16,
} wasi_string_encoding_t;

typedef enum wasi_value_kind_t {
    WASI_VALUE_KIND_FIXED_LIST = 0x67,
    WASI_VALUE_KIND_RESULT = 0x6A,
    WASI_VALUE_KIND_OPTION = 0x6B,
    WASI_VALUE_KIND_ENUM = 0x6D,
    WASI_VALUE_KIND_FLAGS = 0x6E,
    WASI_VALUE_KIND_TUPLE = 0x6F,
    WASI_VALUE_KIND_LIST = 0x70,
    WASI_VALUE_KIND_VARIANT = 0x71,
    WASI_VALUE_KIND_RECORD = 0x72,
    WASI_VALUE_KIND_BOOL = 0x7F,
    WASI_VALUE_KIND_S8 = 0x7E,
    WASI_VALUE_KIND_U8 = 0x7D,
    WASI_VALUE_KIND_S16 = 0x7C,
    WASI_VALUE_KIND_U16 = 0x7B,
    WASI_VALUE_KIND_S32 = 0x7A,
    WASI_VALUE_KIND_U32 = 0x79,
    WASI_VALUE_KIND_S64 = 0x78,
    WASI_VALUE_KIND_U64 = 0x77,
    WASI_VALUE_KIND_F32 = 0x76,
    WASI_VALUE_KIND_F64 = 0x75,
    WASI_VALUE_KIND_CHAR = 0x74,
    WASI_VALUE_KIND_STRING = 0x73,
} wasi_value_kind_t;

typedef struct wasi_value_t wasi_value_t;

typedef struct wasi_string_t {
    char* data;
    size_t len;
    int owned;
} wasi_string_t;

typedef struct wasi_value_seq_t {
    wasi_value_t* values;
    size_t len;
    int owned;
} wasi_value_seq_t;

typedef struct wasi_value_flags_t {
    uint32_t* words;
    size_t len;
    int owned;
} wasi_value_flags_t;

typedef struct wasi_value_variant_t {
    uint32_t case_index;
    int has_payload;
    wasi_value_t* payload;
    int payload_owned;
} wasi_value_variant_t;

struct wasi_value_t {
    wasi_value_kind_t kind;
    union {
        uint32_t enum_case;
        uint8_t boolean;
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
        wasi_string_t string;
        wasi_value_seq_t seq;
        wasi_value_flags_t flags;
        wasi_value_variant_t variant;
    } of;
    };

typedef struct wasi_canon_options_t {
    uint32_t memory_index;
    const char* cabi_realloc_name;
    const char* post_return_name;
    wasi_string_encoding_t string_encoding;
} wasi_canon_options_t;

void wasi_config_default(wasi_config_t* config);
wasi_error_t wasi_value_set_string_copy(wasi_value_t* value, const char* bytes, size_t len);
void wasi_value_init(wasi_value_t* value);
void wasi_value_destroy(wasi_value_t* value);
wasi_error_t wasi_canon_call(const wasi_component_t* component,
                             uint32_t func_type_index,
                             wasm_module_t* core_module,
                             const char* core_export_name,
                             const wasi_canon_options_t* options,
                             const wasi_value_t* args,
                             uint32_t num_args,
                             wasi_value_t* results,
                             uint32_t num_results);
wasi_error_t wasi_canon_options_default(wasi_canon_options_t* options);
wasi_instance_t* wasi_instantiate(const wasi_component_t* component);
void wasi_free_instance(wasi_instance_t* instance);
wasi_error_t wasi_call(wasi_instance_t* instance,
                       const char* export_name,
                       const wasi_value_t* args,
                       uint32_t num_args,
                       wasi_value_t* results,
                       uint32_t num_results);
wasi_error_t wasi_init(wasi_engine_t* engine, const wasi_config_t* config);
void wasi_destroy(wasi_engine_t* engine);
wasi_binary_kind_t wasi_detect_binary_kind(const uint8_t* bytes, size_t len);
wasi_component_t* wasi_load(wasi_engine_t* engine, const uint8_t* bytes, size_t len);
void wasi_free_component(wasi_component_t* component);
wasi_binary_kind_t wasi_component_binary_kind(const wasi_component_t* component);
wasi_component_status_t wasi_component_status(const wasi_component_t* component);
uint8_t wasi_component_layer(const wasi_component_t* component);
uint32_t wasi_component_section_count(const wasi_component_t* component);
size_t wasi_component_section_offset(const wasi_component_t* component, uint32_t index);
size_t wasi_component_section_payload_offset(const wasi_component_t* component, uint32_t index);
uint8_t wasi_component_section_id(const wasi_component_t* component, uint32_t index);
size_t wasi_component_section_size(const wasi_component_t* component, uint32_t index);
const char* wasi_component_section_name(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_type_count(const wasi_component_t* component);
size_t wasi_component_type_offset(const wasi_component_t* component, uint32_t type_index);
wasi_component_type_kind_t wasi_component_type_kind(const wasi_component_t* component, uint32_t type_index);
uint32_t wasi_component_type_decl_count(const wasi_component_t* component, uint32_t type_index);
const wasi_component_type_decl_t* wasi_component_type_decl_at(const wasi_component_t* component,
                                                              uint32_t type_index,
                                                              uint32_t decl_index);
uint32_t wasi_component_func_type_param_count(const wasi_component_t* component, uint32_t type_index);
const char* wasi_component_func_type_param_name(const wasi_component_t* component, uint32_t type_index, uint32_t param_index);
uint8_t wasi_component_func_type_param_code(const wasi_component_t* component, uint32_t type_index, uint32_t param_index);
const wasi_component_valtype_t* wasi_component_func_type_param_type(const wasi_component_t* component,
                                                                    uint32_t type_index,
                                                                    uint32_t param_index);
uint32_t wasi_component_func_type_result_count(const wasi_component_t* component, uint32_t type_index);
const char* wasi_component_func_type_result_name(const wasi_component_t* component, uint32_t type_index, uint32_t result_index);
uint8_t wasi_component_func_type_result_code(const wasi_component_t* component, uint32_t type_index, uint32_t result_index);
const wasi_component_valtype_t* wasi_component_func_type_result_type(const wasi_component_t* component,
                                                                     uint32_t type_index,
                                                                     uint32_t result_index);
const wasi_component_valtype_t* wasi_component_type_defined_valtype(const wasi_component_t* component,
                                                                    uint32_t type_index);
uint8_t wasi_component_valtype_opcode(const wasi_component_valtype_t* type);
uint32_t wasi_component_valtype_item_count(const wasi_component_valtype_t* type);
const char* wasi_component_valtype_item_name(const wasi_component_valtype_t* type, uint32_t item_index);
int wasi_component_valtype_item_has_type(const wasi_component_valtype_t* type, uint32_t item_index);
const wasi_component_valtype_t* wasi_component_valtype_item_type(const wasi_component_valtype_t* type,
                                                                 uint32_t item_index);
const wasi_component_valtype_t* wasi_component_valtype_element_type(const wasi_component_valtype_t* type);
const wasi_component_valtype_t* wasi_component_valtype_ok_type(const wasi_component_valtype_t* type);
const wasi_component_valtype_t* wasi_component_valtype_err_type(const wasi_component_valtype_t* type);
uint32_t wasi_component_valtype_type_index(const wasi_component_valtype_t* type);
uint32_t wasi_component_valtype_fixed_length(const wasi_component_valtype_t* type);
uint32_t wasi_component_import_count(const wasi_component_t* component);
size_t wasi_component_import_offset(const wasi_component_t* component, uint32_t index);
const char* wasi_component_import_name(const wasi_component_t* component, uint32_t index);
const char* wasi_component_import_interface_name(const wasi_component_t* component, uint32_t index);
const char* wasi_component_import_interface_version(const wasi_component_t* component, uint32_t index);
wasi_component_extern_kind_t wasi_component_import_kind(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_import_type_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_export_count(const wasi_component_t* component);
size_t wasi_component_export_offset(const wasi_component_t* component, uint32_t index);
const char* wasi_component_export_name(const wasi_component_t* component, uint32_t index);
const char* wasi_component_export_interface_name(const wasi_component_t* component, uint32_t index);
const char* wasi_component_export_interface_version(const wasi_component_t* component, uint32_t index);
wasi_component_extern_kind_t wasi_component_export_kind(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_export_index(const wasi_component_t* component, uint32_t index);
int wasi_component_export_has_type(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_export_type_index(const wasi_component_t* component, uint32_t index);
int wasi_component_export_func_type_index(const wasi_component_t* component, uint32_t index, uint32_t* out_type_index);
uint32_t wasi_component_alias_count(const wasi_component_t* component);
size_t wasi_component_alias_offset(const wasi_component_t* component, uint32_t index);
wasi_component_alias_kind_t wasi_component_alias_kind(const wasi_component_t* component, uint32_t index);
int wasi_component_alias_sort_is_core(const wasi_component_t* component, uint32_t index);
uint8_t wasi_component_alias_sort_code(const wasi_component_t* component, uint32_t index);
wasi_component_extern_kind_t wasi_component_alias_extern_kind(const wasi_component_t* component, uint32_t index);
wasm_export_kind_t wasi_component_alias_core_export_kind(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_alias_instance_index(const wasi_component_t* component, uint32_t index);
const char* wasi_component_alias_name(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_alias_outer_count(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_alias_outer_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_core_type_count(const wasi_component_t* component);
size_t wasi_component_core_type_offset(const wasi_component_t* component, uint32_t index);
const wasi_component_core_type_t* wasi_component_core_type_at(const wasi_component_t* component, uint32_t index);
wasi_component_core_type_kind_t wasi_component_core_type_kind(const wasi_component_t* component, uint32_t index);
uint8_t wasi_component_core_type_opcode(const wasi_component_t* component, uint32_t index);
uint8_t wasi_component_core_type_detail_opcode(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_core_type_item_count(const wasi_component_t* component, uint32_t index);
int wasi_component_core_type_has_primary_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_core_type_primary_index(const wasi_component_t* component, uint32_t index);
uint32_t wasi_component_core_instance_count(const wasi_component_t* component);
size_t wasi_component_core_instance_offset(const wasi_component_t* component, uint32_t index);
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
size_t wasi_component_instance_offset(const wasi_component_t* component, uint32_t index);
int wasi_component_has_start(const wasi_component_t* component);
size_t wasi_component_start_offset(const wasi_component_t* component);
uint32_t wasi_component_start_func_index(const wasi_component_t* component);
uint32_t wasi_component_start_arg_count(const wasi_component_t* component);
uint32_t wasi_component_start_result_count(const wasi_component_t* component);
uint32_t wasi_component_canon_count(const wasi_component_t* component);
size_t wasi_component_canon_offset(const wasi_component_t* component, uint32_t index);
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
size_t wasi_component_core_module_offset(const wasi_component_t* component, uint32_t index);
size_t wasi_component_core_module_size(const wasi_component_t* component, uint32_t index);
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
const char* wasi_component_type_decl_kind_string(wasi_component_type_decl_kind_t kind);
const char* wasi_component_core_type_kind_string(wasi_component_core_type_kind_t kind);
const char* wasi_component_core_module_decl_kind_string(wasi_component_core_module_decl_kind_t kind);
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
    const uint8_t* base;
    const uint8_t* ptr;
    const uint8_t* end;
    int malformed;
} wasi__reader_t;

typedef struct wasi__component_valtype_item_t {
    char* name;
    int has_type;
    wasi_component_valtype_t* type;
} wasi__component_valtype_item_t;

struct wasi_component_valtype_t {
    uint8_t opcode;
    uint32_t item_count;
    union {
        struct {
            wasi__component_valtype_item_t* items;
        } aggregate;
        struct {
            wasi_component_valtype_t* type;
        } unary;
        struct {
            wasi_component_valtype_t* ok_type;
            wasi_component_valtype_t* err_type;
        } result;
        struct {
            wasi_component_valtype_t* element_type;
            uint32_t length;
        } fixed_list;
        struct {
            uint32_t type_index;
        } index;
    } data;
};

static wasi_error_t wasi__classify_binary(const uint8_t* bytes,
                                          size_t len,
                                          wasi_binary_kind_t* out_kind,
                                          uint8_t out_version[4]);
static void wasi__component_valtype_release(wasi_component_valtype_t* type);
static void wasi__component_type_release(wasi_component_type_t* type);
static void wasi__component_type_decl_release(wasi_component_type_decl_t* decl);
static void wasi__component_core_type_release(wasi_component_core_type_t* type);

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

static size_t wasi__reader_offset(const wasi__reader_t* reader) {
    if (!reader || !reader->base || !reader->ptr || reader->ptr < reader->base) return 0u;
    return (size_t)(reader->ptr - reader->base);
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

static char* wasi__dup_string(const char* src) {
    return src ? wasi__dup_name_bytes((const uint8_t*)src, strlen(src)) : NULL;
}

static char* wasi__join_name_version(const char* name, const char* version) {
    size_t name_len;
    size_t version_len;
    char* full_name;

    if (!name || !version || !version[0]) return wasi__dup_string(name);

    name_len = strlen(name);
    version_len = strlen(version);
    full_name = (char*)WASM_MALLOC(name_len + 1u + version_len + 1u);
    if (!full_name) return NULL;

    memcpy(full_name, name, name_len);
    full_name[name_len] = '@';
    memcpy(full_name + name_len + 1u, version, version_len);
    full_name[name_len + 1u + version_len] = '\0';
    return full_name;
}

static void wasi__resolve_interface_name(const char* name,
                                         char** out_interface_name,
                                         char** out_interface_version) {
    const char* at;

    if (out_interface_name) *out_interface_name = NULL;
    if (out_interface_version) *out_interface_version = NULL;
    if (!name) return;

    at = strrchr(name, '@');
    if (!at || at == name || !at[1]) return;

    if (out_interface_name) *out_interface_name = wasi__dup_name_bytes((const uint8_t*)name, (size_t)(at - name));
    if (out_interface_version) *out_interface_version = wasi__dup_string(at + 1);
    if ((out_interface_name && !*out_interface_name) || (out_interface_version && !*out_interface_version)) {
        if (out_interface_name) {
            WASM_FREE(*out_interface_name);
            *out_interface_name = NULL;
        }
        if (out_interface_version) {
            WASM_FREE(*out_interface_version);
            *out_interface_version = NULL;
        }
    }
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
    char* name = NULL;
    char* version = NULL;
    char* full_name = NULL;

    if (out_name_kind) *out_name_kind = 0;
    if (out_name) *out_name = NULL;
    if (!reader || !out_name) return 0;

    name_kind = wasi__read_u8(reader);
    name_len = wasi__read_leb128_u32(reader);
    if (reader->malformed || !wasi__has(reader, (size_t)name_len)) {
        reader->malformed = 1;
        return 0;
    }

    name = wasi__dup_name_bytes(reader->ptr, (size_t)name_len);
    if (!name) {
        reader->malformed = 1;
        return 0;
    }
    reader->ptr += name_len;

    if (name_kind == 0x02) {
        uint32_t version_len = wasi__read_leb128_u32(reader);
        if (reader->malformed || !wasi__has(reader, (size_t)version_len)) {
            WASM_FREE(name);
            reader->malformed = 1;
            return 0;
        }
        version = wasi__dup_name_bytes(reader->ptr, (size_t)version_len);
        if (!version) {
            WASM_FREE(name);
            reader->malformed = 1;
            return 0;
        }
        reader->ptr += version_len;
        full_name = wasi__join_name_version(name, version);
        WASM_FREE(name);
        WASM_FREE(version);
        if (!full_name) {
            reader->malformed = 1;
            return 0;
        }
        name = full_name;
    }

    if (out_name_kind) *out_name_kind = name_kind;
    *out_name = name;
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
    return option == 0x03 || option == 0x04 || option == 0x05 || option == 0x07;
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

    for (i = 0; i < component->num_core_types; i++) {
        wasi__component_core_type_release(&component->core_types[i]);
    }
    WASM_FREE(component->core_types);
    component->core_types = NULL;
    component->num_core_types = 0;
    component->core_types_capacity = 0;

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
        wasi__component_type_release(&component->types[i]);
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
        WASM_FREE(component->imports[i].interface_name);
        WASM_FREE(component->imports[i].interface_version);
    }
    WASM_FREE(component->imports);
    component->imports = NULL;
    component->num_imports = 0;
    component->imports_capacity = 0;

    for (i = 0; i < component->num_exports; i++) {
        WASM_FREE(component->exports[i].name);
        WASM_FREE(component->exports[i].interface_name);
        WASM_FREE(component->exports[i].interface_version);
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

static void wasi__component_type_release(wasi_component_type_t* type) {
    uint32_t i;

    if (!type) return;

    for (i = 0; i < type->func.num_params; i++) {
        WASM_FREE(type->func.params[i].name);
        wasi__component_valtype_release(type->func.params[i].type);
    }
    WASM_FREE(type->func.params);
    type->func.params = NULL;
    type->func.num_params = 0;
    type->func.params_capacity = 0;

    for (i = 0; i < type->func.num_results; i++) {
        WASM_FREE(type->func.results[i].name);
        wasi__component_valtype_release(type->func.results[i].type);
    }
    WASM_FREE(type->func.results);
    type->func.results = NULL;
    type->func.num_results = 0;
    type->func.results_capacity = 0;

    wasi__component_valtype_release(type->defined_type);
    type->defined_type = NULL;

    for (i = 0; i < type->num_decls; i++) {
        wasi__component_type_decl_release(&type->decls[i]);
    }
    WASM_FREE(type->decls);
    type->decls = NULL;
    type->num_decls = 0;
    type->decls_capacity = 0;
}

static void wasi__component_externdesc_release(wasi_component_externdesc_t* desc) {
    if (!desc) return;
    wasi__component_valtype_release(desc->value_type);
    desc->value_type = NULL;
    desc->has_type_index = 0;
    desc->type_index = UINT32_MAX;
}

static void wasi__component_type_decl_release(wasi_component_type_decl_t* decl) {
    if (!decl) return;

    switch (decl->kind) {
        case WASI_COMPONENT_TYPE_DECL_KIND_CORE_TYPE:
            wasi__component_core_type_release(&decl->data.core_type);
            break;
        case WASI_COMPONENT_TYPE_DECL_KIND_TYPE:
            wasi__component_type_release(&decl->data.type);
            break;
        case WASI_COMPONENT_TYPE_DECL_KIND_ALIAS:
            WASM_FREE(decl->data.alias.name);
            decl->data.alias.name = NULL;
            break;
        case WASI_COMPONENT_TYPE_DECL_KIND_IMPORT:
        case WASI_COMPONENT_TYPE_DECL_KIND_EXPORT:
            WASM_FREE(decl->name);
            WASM_FREE(decl->interface_name);
            WASM_FREE(decl->interface_version);
            decl->name = NULL;
            decl->interface_name = NULL;
            decl->interface_version = NULL;
            wasi__component_externdesc_release(&decl->data.externdesc);
            break;
        default:
            break;
    }
}

static void wasi__component_core_externtype_release(wasi_component_core_externtype_t* type) {
    if (!type) return;
    memset(type, 0, sizeof(*type));
}

static void wasi__component_core_module_decl_release(wasi_component_core_module_decl_t* decl) {
    if (!decl) return;

    WASM_FREE(decl->module_name);
    decl->module_name = NULL;
    WASM_FREE(decl->name);
    decl->name = NULL;

    switch (decl->kind) {
        case WASI_COMPONENT_CORE_MODULE_DECL_KIND_TYPE:
            if (decl->data.type) {
                wasi__component_core_type_release(decl->data.type);
                WASM_FREE(decl->data.type);
                decl->data.type = NULL;
            }
            break;
        case WASI_COMPONENT_CORE_MODULE_DECL_KIND_IMPORT:
        case WASI_COMPONENT_CORE_MODULE_DECL_KIND_EXPORT:
            wasi__component_core_externtype_release(&decl->data.externtype);
            break;
        default:
            break;
    }
}

static void wasi__component_core_type_release(wasi_component_core_type_t* type) {
    uint32_t i;

    if (!type) return;

    WASM_FREE(type->supertypes);
    type->supertypes = NULL;
    type->num_supertypes = 0;

    if (type->kind == WASI_COMPONENT_CORE_TYPE_KIND_MODULE) {
        for (i = 0; i < type->data.module.num_decls; i++) {
            wasi__component_core_module_decl_release(&type->data.module.decls[i]);
        }
        WASM_FREE(type->data.module.decls);
        type->data.module.decls = NULL;
        type->data.module.num_decls = 0;
        type->data.module.decls_capacity = 0;
    } else if (type->opcode == 0x4Eu) {
        for (i = 0; i < type->data.rec_group.num_entries; i++) {
            if (type->data.rec_group.entries[i]) {
                wasi__component_core_type_release(type->data.rec_group.entries[i]);
                WASM_FREE(type->data.rec_group.entries[i]);
            }
        }
        WASM_FREE(type->data.rec_group.entries);
        type->data.rec_group.entries = NULL;
        type->data.rec_group.num_entries = 0;
    } else {
        switch (type->detail_opcode) {
            case 0x60u:
                wasm__free_functype(&type->data.func);
                break;
            case 0x5Fu:
                wasm__free_structtype(&type->data.struct_);
                break;
            default:
                break;
        }
    }

    memset(type, 0, sizeof(*type));
}

static void wasi__component_valtype_release(wasi_component_valtype_t* type) {
    uint32_t i;

    if (!type) return;

    switch (type->opcode) {
        case 0x72u:
        case 0x71u:
        case 0x6Fu:
        case 0x6Eu:
        case 0x6Du:
            for (i = 0; i < type->item_count; i++) {
                WASM_FREE(type->data.aggregate.items[i].name);
                wasi__component_valtype_release(type->data.aggregate.items[i].type);
            }
            WASM_FREE(type->data.aggregate.items);
            break;
        case 0x70u:
        case 0x6Bu:
        case 0x66u:
        case 0x65u:
            wasi__component_valtype_release(type->data.unary.type);
            break;
        case 0x6Au:
            wasi__component_valtype_release(type->data.result.ok_type);
            wasi__component_valtype_release(type->data.result.err_type);
            break;
        case 0x67u:
            wasi__component_valtype_release(type->data.fixed_list.element_type);
            break;
        default:
            break;
    }

    WASM_FREE(type);
}

static wasi_error_t wasi__func_type_append_named(wasi_component_named_type_t** items,
                                                 uint32_t* count,
                                                 uint32_t* capacity,
                                                 char* name,
                                                 wasi_component_valtype_t* type) {
    wasi_component_named_type_t* grown;
    size_t next_capacity;
    wasi_component_named_type_t* entry;

    if (!items || !count || !capacity) {
        WASM_FREE(name);
        wasi__component_valtype_release(type);
        return WASI_ERR_INVALID_ARGUMENT;
    }

    if (*count == *capacity) {
        next_capacity = *capacity ? (size_t)(*capacity) * 2u : 4u;
        grown = (wasi_component_named_type_t*)WASM_REALLOC(*items, next_capacity * sizeof(*grown));
        if (!grown) {
            WASM_FREE(name);
            wasi__component_valtype_release(type);
            return WASI_ERR_OOM;
        }
        *items = grown;
        *capacity = (uint32_t)next_capacity;
    }

    entry = &(*items)[(*count)++];
    memset(entry, 0, sizeof(*entry));
    entry->name = name;
    entry->type_code = type ? type->opcode : 0u;
    entry->type = type;
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

static wasi_error_t wasi__component_type_append_decl(wasi_component_type_t* type,
                                                     const wasi_component_type_decl_t* decl) {
    wasi_component_type_decl_t* grown;
    size_t next_capacity;

    if (!type || !decl) return WASI_ERR_INVALID_ARGUMENT;

    if (type->num_decls == type->decls_capacity) {
        next_capacity = type->decls_capacity ? (size_t)type->decls_capacity * 2u : 4u;
        grown = (wasi_component_type_decl_t*)WASM_REALLOC(type->decls, next_capacity * sizeof(*grown));
        if (!grown) return WASI_ERR_OOM;
        type->decls = grown;
        type->decls_capacity = (uint32_t)next_capacity;
    }

    type->decls[type->num_decls++] = *decl;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_core_type(wasi_component_t* component,
                                                     const wasi_component_core_type_t* type) {
    wasi_component_core_type_t* grown;
    size_t next_capacity;

    if (!component || !type) return WASI_ERR_INVALID_ARGUMENT;

    if (component->num_core_types == component->core_types_capacity) {
        next_capacity = component->core_types_capacity ? (size_t)component->core_types_capacity * 2u : 4u;
        grown = (wasi_component_core_type_t*)WASM_REALLOC(component->core_types,
                                                          next_capacity * sizeof(*grown));
        if (!grown) return WASI_ERR_OOM;
        component->core_types = grown;
        component->core_types_capacity = (uint32_t)next_capacity;
    }

    component->core_types[component->num_core_types++] = *type;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_func(wasi_component_t* component,
                                                size_t offset,
                                                uint32_t type_index) {
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

    component->funcs[component->num_funcs].offset = offset;
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
                                                  size_t offset,
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
    entry->offset = offset;
    entry->name = name;
    wasi__resolve_interface_name(name, &entry->interface_name, &entry->interface_version);
    entry->name_kind = name_kind;
    entry->kind = kind;
    entry->type_index = type_index;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_export(wasi_component_t* component,
                                                  size_t offset,
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
    entry->offset = offset;
    entry->name = name;
    wasi__resolve_interface_name(name, &entry->interface_name, &entry->interface_version);
    entry->name_kind = name_kind;
    entry->kind = kind;
    entry->index = index;
    entry->has_type = has_type;
    entry->type_index = type_index;
    return WASI_OK;
}

static wasi_error_t wasi__component_append_alias(wasi_component_t* component,
                                                 size_t offset,
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
    entry->offset = offset;
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

    reader.base = component->bytes;
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
        payload_reader.base = component->bytes;
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

static int wasi__read_component_externdesc(wasi__reader_t* reader, wasi_component_externdesc_t* desc);

static wasi_error_t wasi__parse_component_imports(wasi_engine_t* engine,
                                                  wasi_component_t* component,
                                                  const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.base = component->bytes;
    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component import section");
    }

    for (i = 0; i < count; i++) {
        size_t entry_offset = wasi__reader_offset(&reader);
        char* name = NULL;
        uint8_t name_kind = 0;
        wasi_component_externdesc_t desc;
        wasi_error_t err;

        memset(&desc, 0, sizeof(desc));
        desc.kind = WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
        desc.type_index = UINT32_MAX;

        if (!wasi__read_component_name(&reader, &name_kind, &name)) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component import name");
        }

        if (!wasi__read_component_externdesc(&reader, &desc)) {
            WASM_FREE(name);
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component import descriptor");
        }

        err = wasi__component_append_import(component,
                                            entry_offset,
                                            name,
                                            name_kind,
                                            desc.kind,
                                            desc.has_type_index ? desc.type_index : UINT32_MAX);
        if (err != WASI_OK) {
            wasi__component_externdesc_release(&desc);
            return wasi__set_error_literal(engine,
                                           err,
                                           err == WASI_ERR_OOM ? "component import alloc failed"
                                                               : wasi_error_string(err));
        }
        if (desc.kind == WASI_COMPONENT_EXTERN_KIND_FUNC && desc.has_type_index) {
            err = wasi__component_append_func(component, entry_offset, desc.type_index);
            if (err != WASI_OK) {
                wasi__component_externdesc_release(&desc);
                return wasi__set_error_literal(engine,
                                               err,
                                               err == WASI_ERR_OOM ? "component func alloc failed"
                                                                   : wasi_error_string(err));
            }
        }
        wasi__component_externdesc_release(&desc);
    }

    if (reader.ptr != reader.end) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "trailing bytes in component import section");
    }
    return WASI_OK;
}

static int wasi__read_component_optional_valtype(wasi__reader_t* reader,
                                                 wasi_component_valtype_t** out_type,
                                                 int* out_has_type);

static int wasi__read_component_valtype(wasi__reader_t* reader,
                                        wasi_component_valtype_t** out_type,
                                        uint8_t* out_opcode) {
    const uint8_t* start;
    uint8_t opcode;
    uint32_t count;
    uint32_t i;
    wasi_component_valtype_t* type = NULL;

    if (out_type) *out_type = NULL;
    if (out_opcode) *out_opcode = 0;
    if (!reader || !reader->ptr) return 0;

    start = reader->ptr;
    opcode = wasi__read_u8(reader);
    if (reader->malformed) return 0;

    if (out_type) {
        type = (wasi_component_valtype_t*)WASM_CALLOC(1u, sizeof(*type));
        if (!type) {
            reader->malformed = 1;
            return 0;
        }
        type->opcode = opcode;
    }

    switch (opcode) {
        case 0x64:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x76:
        case 0x77:
        case 0x78:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
            break;
        case 0x72:
            count = wasi__read_leb128_u32(reader);
            if (reader->malformed) goto fail;
            if (type) {
                type->item_count = count;
                if (count) {
                    type->data.aggregate.items = (wasi__component_valtype_item_t*)WASM_CALLOC(count,
                                                                                               sizeof(*type->data.aggregate.items));
                    if (!type->data.aggregate.items) {
                        reader->malformed = 1;
                        goto fail;
                    }
                }
            }
            for (i = 0; i < count; i++) {
                char* name = NULL;
                wasi_component_valtype_t* item_type = NULL;
                if (!wasi__read_component_plain_name(reader, &name)) goto fail;
                if (!wasi__read_component_valtype(reader, type ? &item_type : NULL, NULL)) {
                    WASM_FREE(name);
                    goto fail;
                }
                if (type) {
                    type->data.aggregate.items[i].name = name;
                    type->data.aggregate.items[i].has_type = 1;
                    type->data.aggregate.items[i].type = item_type;
                } else {
                    WASM_FREE(name);
                }
            }
            break;
        case 0x71:
            count = wasi__read_leb128_u32(reader);
            if (reader->malformed) goto fail;
            if (type) {
                type->item_count = count;
                if (count) {
                    type->data.aggregate.items = (wasi__component_valtype_item_t*)WASM_CALLOC(count,
                                                                                               sizeof(*type->data.aggregate.items));
                    if (!type->data.aggregate.items) {
                        reader->malformed = 1;
                        goto fail;
                    }
                }
            }
            for (i = 0; i < count; i++) {
                char* name = NULL;
                wasi_component_valtype_t* item_type = NULL;
                int has_type = 0;
                uint8_t reserved;
                if (!wasi__read_component_plain_name(reader, &name)) goto fail;
                if (!wasi__read_component_optional_valtype(reader,
                                                           type ? &item_type : NULL,
                                                           &has_type)) {
                    WASM_FREE(name);
                    goto fail;
                }
                reserved = wasi__read_u8(reader);
                if (reader->malformed || reserved != 0x00u) {
                    reader->malformed = 1;
                    WASM_FREE(name);
                    wasi__component_valtype_release(item_type);
                    goto fail;
                }
                if (type) {
                    type->data.aggregate.items[i].name = name;
                    type->data.aggregate.items[i].has_type = has_type;
                    type->data.aggregate.items[i].type = item_type;
                } else {
                    WASM_FREE(name);
                }
            }
            break;
        case 0x70:
            if (!wasi__read_component_valtype(reader, type ? &type->data.unary.type : NULL, NULL)) goto fail;
            break;
        case 0x67:
            if (!wasi__read_component_valtype(reader,
                                              type ? &type->data.fixed_list.element_type : NULL,
                                              NULL)) {
                goto fail;
            }
            if (type) type->data.fixed_list.length = wasi__read_leb128_u32(reader);
            else (void)wasi__read_leb128_u32(reader);
            if (reader->malformed) goto fail;
            break;
        case 0x6F:
            count = wasi__read_leb128_u32(reader);
            if (reader->malformed) goto fail;
            if (type) {
                type->item_count = count;
                if (count) {
                    type->data.aggregate.items = (wasi__component_valtype_item_t*)WASM_CALLOC(count,
                                                                                               sizeof(*type->data.aggregate.items));
                    if (!type->data.aggregate.items) {
                        reader->malformed = 1;
                        goto fail;
                    }
                }
            }
            for (i = 0; i < count; i++) {
                wasi_component_valtype_t* item_type = NULL;
                if (!wasi__read_component_valtype(reader, type ? &item_type : NULL, NULL)) goto fail;
                if (type) {
                    type->data.aggregate.items[i].has_type = 1;
                    type->data.aggregate.items[i].type = item_type;
                }
            }
            break;
        case 0x6E:
        case 0x6D:
            count = wasi__read_leb128_u32(reader);
            if (reader->malformed) goto fail;
            if (type) {
                type->item_count = count;
                if (count) {
                    type->data.aggregate.items = (wasi__component_valtype_item_t*)WASM_CALLOC(count,
                                                                                               sizeof(*type->data.aggregate.items));
                    if (!type->data.aggregate.items) {
                        reader->malformed = 1;
                        goto fail;
                    }
                }
            }
            for (i = 0; i < count; i++) {
                char* name = NULL;
                if (!wasi__read_component_plain_name(reader, &name)) goto fail;
                if (type) {
                    type->data.aggregate.items[i].name = name;
                } else {
                    WASM_FREE(name);
                }
            }
            break;
        case 0x6B:
            if (!wasi__read_component_valtype(reader, type ? &type->data.unary.type : NULL, NULL)) goto fail;
            break;
        case 0x6A:
            if (!wasi__read_component_optional_valtype(reader,
                                                       type ? &type->data.result.ok_type : NULL,
                                                       NULL)) {
                goto fail;
            }
            if (!wasi__read_component_optional_valtype(reader,
                                                       type ? &type->data.result.err_type : NULL,
                                                       NULL)) {
                goto fail;
            }
            break;
        case 0x69:
        case 0x68:
            if (type) type->data.index.type_index = wasi__read_leb128_u32(reader);
            else (void)wasi__read_leb128_u32(reader);
            if (reader->malformed) goto fail;
            break;
        case 0x66:
        case 0x65:
            if (!wasi__read_component_optional_valtype(reader, type ? &type->data.unary.type : NULL, NULL)) {
                goto fail;
            }
            break;
        default:
            reader->ptr = start;
            if (type) {
                type->opcode = 0x00u;
                type->data.index.type_index = wasi__read_leb128_u32(reader);
            } else {
                (void)wasi__read_leb128_u32(reader);
            }
            if (reader->malformed) goto fail;
            opcode = 0x00u;
            break;
    }

    if (out_opcode) *out_opcode = opcode;
    if (out_type) *out_type = type;
    return 1;

fail:
    wasi__component_valtype_release(type);
    return 0;
}

static int wasi__read_component_optional_valtype(wasi__reader_t* reader,
                                                 wasi_component_valtype_t** out_type,
                                                 int* out_has_type) {
    uint8_t tag;

    if (out_type) *out_type = NULL;
    if (out_has_type) *out_has_type = 0;
    if (!reader) return 0;

    tag = wasi__read_u8(reader);
    if (reader->malformed) return 0;
    if (tag == 0x00u) return 1;
    if (tag != 0x01u) {
        reader->malformed = 1;
        return 0;
    }

    if (!wasi__read_component_valtype(reader, out_type, NULL)) return 0;
    if (out_has_type) *out_has_type = 1;
    return 1;
}

static int wasi__read_component_resultlist(wasi__reader_t* reader,
                                           wasi_component_named_type_t** items,
                                           uint32_t* count,
                                           uint32_t* capacity,
                                           uint32_t* out_item_count) {
    uint8_t result_tag;
    uint32_t i;

    if (out_item_count) *out_item_count = 0;
    if (!reader || !items || !count || !capacity) return 0;

    result_tag = wasi__read_u8(reader);
    if (reader->malformed) return 0;
    if (result_tag == 0x00u) {
        wasi_component_valtype_t* type = NULL;
        if (!wasi__read_component_valtype(reader, &type, NULL)) return 0;
        if (wasi__func_type_append_named(items, count, capacity, NULL, type) != WASI_OK) return 0;
        if (out_item_count) *out_item_count = 1;
        return 1;
    }
    if (result_tag != 0x01u) {
        reader->malformed = 1;
        return 0;
    }

    i = wasi__read_leb128_u32(reader);
    if (reader->malformed) return 0;
    if (out_item_count) *out_item_count = i;
    while (i--) {
        char* name = NULL;
        wasi_component_valtype_t* type = NULL;
        if (!wasi__read_component_plain_name(reader, &name)) return 0;
        if (!wasi__read_component_valtype(reader, &type, NULL)) {
            WASM_FREE(name);
            return 0;
        }
        if (wasi__func_type_append_named(items, count, capacity, name, type) != WASI_OK) {
            WASM_FREE(name);
            return 0;
        }
    }
    return 1;
}

static void wasi__core_reader_begin(const wasi__reader_t* reader, wasm__reader_t* core_reader) {
    if (!core_reader) return;
    core_reader->ptr = reader ? reader->ptr : NULL;
    core_reader->end = reader ? reader->end : NULL;
    core_reader->malformed = reader ? reader->malformed : 1;
}

static void wasi__core_reader_end(wasi__reader_t* reader, const wasm__reader_t* core_reader) {
    if (!reader || !core_reader) return;
    reader->ptr = core_reader->ptr;
    reader->end = core_reader->end;
    reader->malformed = core_reader->malformed;
}

static uint64_t wasi__read_core_leb128_u64(wasi__reader_t* reader) {
    wasm__reader_t core_reader;
    uint64_t value;

    if (!reader) return 0;
    wasi__core_reader_begin(reader, &core_reader);
    value = wasm__read_leb128_u64(&core_reader);
    wasi__core_reader_end(reader, &core_reader);
    return value;
}

static int wasi__read_core_valtype(wasi__reader_t* reader, wasi_component_core_valtype_t* out_type) {
    wasm__reader_t core_reader;
    wasm_error_t err;
    wasi_component_core_valtype_t parsed;

    if (!reader) return 0;

    memset(&parsed, 0, sizeof(parsed));
    wasi__core_reader_begin(reader, &core_reader);
    err = wasm__read_unresolved_valtype(NULL, &core_reader, &parsed.type, &parsed.reftype);
    wasi__core_reader_end(reader, &core_reader);
    if (err != WASM_OK || reader->malformed) {
        reader->malformed = 1;
        return 0;
    }

    if (out_type) *out_type = parsed;
    return 1;
}

static int wasi__read_core_reftype(wasi__reader_t* reader, wasm_reftype_t* out_type) {
    wasm__reader_t core_reader;
    wasm_error_t err;
    wasm_valtype_t ignored_type = WASM_TYPE_VOID;
    wasm_reftype_t parsed;

    if (!reader) return 0;

    memset(&parsed, 0, sizeof(parsed));
    wasi__core_reader_begin(reader, &core_reader);
    err = wasm__read_reftype(NULL, &core_reader, &ignored_type, &parsed);
    wasi__core_reader_end(reader, &core_reader);
    if (err != WASM_OK || reader->malformed) {
        reader->malformed = 1;
        return 0;
    }

    if (out_type) *out_type = parsed;
    return 1;
}

static int wasi__read_core_storage_type(wasi__reader_t* reader, wasm_storagetype_t* out_type) {
    wasi_component_core_valtype_t value_type;

    if (!reader || !wasi__has(reader, 1)) {
        if (reader) reader->malformed = 1;
        return 0;
    }

    memset(&value_type, 0, sizeof(value_type));
    if (wasm__is_packedtype_byte(*reader->ptr)) {
        uint8_t packed = wasi__read_u8(reader);
        if (out_type) {
            memset(out_type, 0, sizeof(*out_type));
            out_type->kind = WASM_STORAGE_PACKED;
            out_type->of.packed_type = (wasm_packedtype_t)packed;
        }
        return !reader->malformed;
    }

    if (!wasi__read_core_valtype(reader, &value_type)) return 0;
    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = WASM_STORAGE_VALTYPE;
        out_type->of.valtype = value_type.type;
        out_type->ref_type = value_type.reftype;
    }
    return 1;
}

static int wasi__read_core_field_type(wasi__reader_t* reader, wasm_fieldtype_t* out_type) {
    uint8_t mutability;
    wasm_fieldtype_t parsed;

    if (!reader) return 0;

    memset(&parsed, 0, sizeof(parsed));
    if (!wasi__read_core_storage_type(reader, &parsed.storage)) return 0;
    mutability = wasi__read_u8(reader);
    if (reader->malformed || mutability > 1u) {
        reader->malformed = 1;
        return 0;
    }
    parsed.is_mutable = (int)mutability;
    if (out_type) *out_type = parsed;
    return 1;
}

static int wasi__read_core_functype(wasi__reader_t* reader, wasm_functype_t* out_type) {
    wasm_functype_t parsed;
    uint32_t count;
    uint32_t i;

    if (!reader || !out_type) return 0;

    memset(&parsed, 0, sizeof(parsed));

    count = wasi__read_leb128_u32(reader);
    if (reader->malformed) return 0;
    parsed.num_params = count;
    if (count) {
        parsed.params = (wasm_valtype_t*)WASM_CALLOC(count, sizeof(*parsed.params));
        parsed.param_reftypes = (wasm_reftype_t*)WASM_CALLOC(count, sizeof(*parsed.param_reftypes));
        if (!parsed.params || !parsed.param_reftypes) {
            reader->malformed = 1;
            wasm__free_functype(&parsed);
            return 0;
        }
    }
    for (i = 0; i < count; i++) {
        wasi_component_core_valtype_t value_type;
        if (!wasi__read_core_valtype(reader, &value_type)) {
            wasm__free_functype(&parsed);
            return 0;
        }
        parsed.params[i] = value_type.type;
        parsed.param_reftypes[i] = value_type.reftype;
    }

    count = wasi__read_leb128_u32(reader);
    if (reader->malformed) {
        wasm__free_functype(&parsed);
        return 0;
    }
    parsed.num_results = count;
    if (count) {
        parsed.results = (wasm_valtype_t*)WASM_CALLOC(count, sizeof(*parsed.results));
        parsed.result_reftypes = (wasm_reftype_t*)WASM_CALLOC(count, sizeof(*parsed.result_reftypes));
        if (!parsed.results || !parsed.result_reftypes) {
            reader->malformed = 1;
            wasm__free_functype(&parsed);
            return 0;
        }
    }
    for (i = 0; i < count; i++) {
        wasi_component_core_valtype_t value_type;
        if (!wasi__read_core_valtype(reader, &value_type)) {
            wasm__free_functype(&parsed);
            return 0;
        }
        parsed.results[i] = value_type.type;
        parsed.result_reftypes[i] = value_type.reftype;
    }

    *out_type = parsed;
    return 1;
}

static int wasi__component_core_type_append_module_decl(wasi_component_core_type_t* type,
                                                        const wasi_component_core_module_decl_t* decl) {
    wasi_component_core_module_decl_t* grown;
    size_t next_capacity;

    if (!type || !decl) return 0;

    if (type->data.module.num_decls == type->data.module.decls_capacity) {
        next_capacity = type->data.module.decls_capacity ? (size_t)type->data.module.decls_capacity * 2u : 4u;
        grown = (wasi_component_core_module_decl_t*)WASM_REALLOC(type->data.module.decls,
                                                                 next_capacity * sizeof(*grown));
        if (!grown) return 0;
        type->data.module.decls = grown;
        type->data.module.decls_capacity = (uint32_t)next_capacity;
    }

    type->data.module.decls[type->data.module.num_decls++] = *decl;
    return 1;
}

static int wasi__read_core_limits(wasi__reader_t* reader,
                                  uint8_t flags_mask,
                                  wasi_component_core_limits_t* out_limits) {
    uint8_t flags;
    wasi_component_core_limits_t parsed;

    if (!reader || !out_limits) return 0;

    memset(&parsed, 0, sizeof(parsed));
    flags = wasi__read_u8(reader);
    if (reader->malformed || (flags & ~flags_mask) != 0) {
        reader->malformed = 1;
        return 0;
    }
    parsed.flags = flags;
    parsed.has_max = (flags & 0x01u) != 0;
    parsed.is_shared = (flags & 0x02u) != 0;
    parsed.is_64 = (flags & 0x04u) != 0;
    parsed.min_size = wasi__read_core_leb128_u64(reader);
    if (reader->malformed) return 0;
    parsed.max_size = parsed.has_max ? wasi__read_core_leb128_u64(reader) : UINT64_MAX;
    if (reader->malformed) return 0;
    *out_limits = parsed;
    return 1;
}

static int wasi__read_core_table_type(wasi__reader_t* reader, wasi_component_core_table_type_t* out_type) {
    wasi_component_core_table_type_t parsed;

    if (!reader || !out_type) return 0;
    if (!wasi__has(reader, 1) || *reader->ptr == 0x40u) {
        reader->malformed = 1;
        return 0;
    }

    memset(&parsed, 0, sizeof(parsed));
    if (!wasi__read_core_reftype(reader, &parsed.reftype)) return 0;
    if (!wasi__read_core_limits(reader, 0x07u, &parsed.limits)) return 0;
    *out_type = parsed;
    return 1;
}

static int wasi__read_core_memory_type(wasi__reader_t* reader, wasi_component_core_memory_type_t* out_type) {
    if (!reader || !out_type) return 0;
    memset(out_type, 0, sizeof(*out_type));
    return wasi__read_core_limits(reader, 0x0Fu, &out_type->limits);
}

static int wasi__read_core_global_type(wasi__reader_t* reader, wasi_component_core_global_type_t* out_type) {
    uint8_t mutability;
    wasi_component_core_global_type_t parsed;

    if (!reader || !out_type) return 0;

    memset(&parsed, 0, sizeof(parsed));
    if (!wasi__read_core_valtype(reader, &parsed.value_type)) return 0;
    mutability = wasi__read_u8(reader);
    if (reader->malformed || mutability > 1u) {
        reader->malformed = 1;
        return 0;
    }
    parsed.is_mutable = (int)mutability;
    *out_type = parsed;
    return 1;
}

static int wasi__read_core_module_externtype(wasi__reader_t* reader, wasi_component_core_externtype_t* out_type) {
    uint8_t kind;
    wasi_component_core_externtype_t parsed;

    if (!reader || !out_type) return 0;

    memset(&parsed, 0, sizeof(parsed));
    kind = wasi__read_u8(reader);
    if (reader->malformed || kind > 0x04u) {
        reader->malformed = 1;
        return 0;
    }
    parsed.kind = (wasm_export_kind_t)kind;

    switch (kind) {
        case 0x00u:
            parsed.of.func_type_index = wasi__read_leb128_u32(reader);
            break;
        case 0x01u:
            if (!wasi__read_core_table_type(reader, &parsed.of.table)) return 0;
            break;
        case 0x02u:
            if (!wasi__read_core_memory_type(reader, &parsed.of.memory)) return 0;
            break;
        case 0x03u:
            if (!wasi__read_core_global_type(reader, &parsed.of.global)) return 0;
            break;
        case 0x04u:
            parsed.of.tag.attribute = wasi__read_u8(reader);
            parsed.of.tag.type_index = wasi__read_leb128_u32(reader);
            if (reader->malformed || parsed.of.tag.attribute != 0x00u) {
                reader->malformed = 1;
                return 0;
            }
            break;
        default:
            reader->malformed = 1;
            return 0;
    }

    if (reader->malformed) return 0;
    *out_type = parsed;
    return 1;
}

static int wasi__read_core_module_alias(wasi__reader_t* reader, wasi_component_core_module_alias_t* out_alias) {
    wasi_component_core_module_alias_t parsed;

    if (!reader || !out_alias) return 0;

    memset(&parsed, 0, sizeof(parsed));
    parsed.sort_code = wasi__read_u8(reader);
    parsed.alias_kind = wasi__read_u8(reader);
    parsed.outer_count = wasi__read_leb128_u32(reader);
    parsed.outer_index = wasi__read_leb128_u32(reader);
    if (reader->malformed ||
        (parsed.sort_code != 0x00u && parsed.sort_code != 0x01u && parsed.sort_code != 0x02u &&
         parsed.sort_code != 0x03u && parsed.sort_code != 0x04u && parsed.sort_code != 0x10u &&
         parsed.sort_code != 0x11u) ||
        parsed.alias_kind != 0x01u) {
        reader->malformed = 1;
        return 0;
    }

    *out_alias = parsed;
    return 1;
}

static int wasi__read_core_subtype_payload(wasi__reader_t* reader,
                                           uint8_t opcode,
                                           wasi_component_core_type_t* type) {
    size_t offset;
    uint32_t count;
    uint32_t i;

    if (!reader || !type) return 0;

    offset = type ? type->offset : 0u;
    memset(type, 0, sizeof(*type));
    type->offset = offset;
    type->kind = WASI_COMPONENT_CORE_TYPE_KIND_TYPE;
    type->opcode = opcode;
    type->detail_opcode = opcode;
    type->item_count = 1u;
    type->primary_index = UINT32_MAX;
    type->is_final = 1;

    if (opcode == 0x4Eu) {
        type->detail_opcode = 0x4Eu;
        count = wasi__read_leb128_u32(reader);
        if (reader->malformed) return 0;
        type->item_count = count;
        type->data.rec_group.num_entries = count;
        if (count) {
            type->data.rec_group.entries = (wasi_component_core_type_t**)WASM_CALLOC(count,
                                                                                      sizeof(*type->data.rec_group.entries));
            if (!type->data.rec_group.entries) {
                reader->malformed = 1;
                return 0;
            }
        }
        for (i = 0; i < count; i++) {
            size_t nested_offset = wasi__reader_offset(reader);
            uint8_t nested_opcode = wasi__read_u8(reader);
            wasi_component_core_type_t* nested_type;
            if (reader->malformed) return 0;
            nested_type = (wasi_component_core_type_t*)WASM_CALLOC(1u, sizeof(*nested_type));
            if (!nested_type) {
                reader->malformed = 1;
                return 0;
            }
            nested_type->offset = nested_offset;
            if (!wasi__read_core_subtype_payload(reader, nested_opcode, nested_type)) {
                wasi__component_core_type_release(nested_type);
                WASM_FREE(nested_type);
                return 0;
            }
            type->data.rec_group.entries[i] = nested_type;
        }
        if (count) type->detail_opcode = type->data.rec_group.entries[count - 1u]->detail_opcode;
        return 1;
    }

    if (opcode == 0x4Fu || opcode == 0x50u) {
        type->is_final = (int)(opcode == 0x4Fu);
        count = wasi__read_leb128_u32(reader);
        if (reader->malformed) return 0;
        type->num_supertypes = count;
        if (count) {
            type->supertypes = (uint32_t*)WASM_CALLOC(count, sizeof(*type->supertypes));
            if (!type->supertypes) {
                reader->malformed = 1;
                return 0;
            }
        }
        for (i = 0; i < count; i++) {
            type->supertypes[i] = wasi__read_leb128_u32(reader);
            if (reader->malformed) return 0;
        }
        opcode = wasi__read_u8(reader);
        if (reader->malformed) return 0;
        type->detail_opcode = opcode;
    }

    switch (opcode) {
        case 0x60u:
            return wasi__read_core_functype(reader, &type->data.func);
        case 0x5Fu:
            count = wasi__read_leb128_u32(reader);
            if (reader->malformed) return 0;
            type->item_count = count;
            type->data.struct_.num_fields = count;
            if (count) {
                type->data.struct_.fields = (wasm_fieldtype_t*)WASM_CALLOC(count, sizeof(*type->data.struct_.fields));
                if (!type->data.struct_.fields) {
                    reader->malformed = 1;
                    return 0;
                }
            }
            for (i = 0; i < count; i++) {
                if (!wasi__read_core_field_type(reader, &type->data.struct_.fields[i])) return 0;
            }
            return 1;
        case 0x5Eu:
            type->item_count = 1u;
            return wasi__read_core_field_type(reader, &type->data.array.field);
        case 0x5Du:
            type->has_primary_index = 1;
            type->primary_index = wasi__read_leb128_u32(reader);
            type->data.cont.type_index = type->primary_index;
            return !reader->malformed;
        default:
            reader->malformed = 1;
            return 0;
    }
}

static int wasi__read_core_module_type(wasi__reader_t* reader, wasi_component_core_type_t* type) {
    size_t offset;
    uint32_t decl_count;
    uint32_t i;

    if (!reader || !type) return 0;

    offset = type ? type->offset : 0u;
    memset(type, 0, sizeof(*type));
    decl_count = wasi__read_leb128_u32(reader);
    if (reader->malformed) return 0;

    type->offset = offset;
    type->kind = WASI_COMPONENT_CORE_TYPE_KIND_MODULE;
    type->opcode = 0x50u;
    type->detail_opcode = 0x50u;
    type->item_count = decl_count;
    type->primary_index = UINT32_MAX;

    for (i = 0; i < decl_count; i++) {
        size_t decl_offset = wasi__reader_offset(reader);
        uint8_t decl_opcode = wasi__read_u8(reader);
        wasi_component_core_module_decl_t decl;
        if (reader->malformed) return 0;

        memset(&decl, 0, sizeof(decl));
        decl.offset = decl_offset;
        switch (decl_opcode) {
            case 0x00u:
                decl.kind = WASI_COMPONENT_CORE_MODULE_DECL_KIND_IMPORT;
                if (!wasi__read_component_plain_name(reader, &decl.module_name) ||
                    !wasi__read_component_plain_name(reader, &decl.name) ||
                    !wasi__read_core_module_externtype(reader, &decl.data.externtype)) {
                    wasi__component_core_module_decl_release(&decl);
                    return 0;
                }
                break;
            case 0x01u: {
                size_t subtype_offset = wasi__reader_offset(reader);
                uint8_t subtype_opcode = wasi__read_u8(reader);
                if (reader->malformed) return 0;
                decl.kind = WASI_COMPONENT_CORE_MODULE_DECL_KIND_TYPE;
                decl.data.type = (wasi_component_core_type_t*)WASM_CALLOC(1u, sizeof(*decl.data.type));
                if (!decl.data.type) {
                    reader->malformed = 1;
                    return 0;
                }
                decl.data.type->offset = subtype_offset;
                if (!wasi__read_core_subtype_payload(reader, subtype_opcode, decl.data.type)) {
                    wasi__component_core_module_decl_release(&decl);
                    return 0;
                }
                break;
            }
            case 0x02u:
                decl.kind = WASI_COMPONENT_CORE_MODULE_DECL_KIND_ALIAS;
                if (!wasi__read_core_module_alias(reader, &decl.data.alias)) return 0;
                break;
            case 0x03u:
                decl.kind = WASI_COMPONENT_CORE_MODULE_DECL_KIND_EXPORT;
                if (!wasi__read_component_plain_name(reader, &decl.name) ||
                    !wasi__read_core_module_externtype(reader, &decl.data.externtype)) {
                    wasi__component_core_module_decl_release(&decl);
                    return 0;
                }
                break;
            default:
                reader->malformed = 1;
                return 0;
        }

        if (!wasi__component_core_type_append_module_decl(type, &decl)) {
            reader->malformed = 1;
            wasi__component_core_module_decl_release(&decl);
            return 0;
        }
    }

    return 1;
}

static int wasi__is_core_subtype_opcode(uint8_t opcode) {
    return opcode == 0x4Eu || opcode == 0x4Fu || opcode == 0x50u ||
           opcode == 0x5Du || opcode == 0x5Eu || opcode == 0x5Fu || opcode == 0x60u;
}

static int wasi__read_component_core_type_entries(wasi__reader_t* reader,
                                                  wasi_component_core_type_t* entries,
                                                  uint32_t index,
                                                  uint32_t count) {
    size_t entry_offset;
    uint8_t opcode;
    wasi_component_core_type_t type;
    wasi__reader_t branch_reader;

    if (!reader || !entries) return 0;
    if (index == count) return reader->ptr == reader->end;

    entry_offset = wasi__reader_offset(reader);
    opcode = wasi__read_u8(reader);
    if (reader->malformed) return 0;

    if (opcode == 0x50u) {
        memset(&type, 0, sizeof(type));
        type.offset = entry_offset;
        branch_reader = *reader;
        if (wasi__read_core_module_type(&branch_reader, &type)) {
            entries[index] = type;
            if (wasi__read_component_core_type_entries(&branch_reader, entries, index + 1u, count)) {
                *reader = branch_reader;
                return 1;
            }
            wasi__component_core_type_release(&type);
        }

        memset(&type, 0, sizeof(type));
    type.offset = entry_offset;
        branch_reader = *reader;
        if (wasi__read_core_subtype_payload(&branch_reader, opcode, &type)) {
            entries[index] = type;
            if (wasi__read_component_core_type_entries(&branch_reader, entries, index + 1u, count)) {
                *reader = branch_reader;
                return 1;
            }
            wasi__component_core_type_release(&type);
        }

        reader->malformed = 1;
        return 0;
    }

    if (!wasi__is_core_subtype_opcode(opcode)) {
        reader->malformed = 1;
        return 0;
    }

    memset(&type, 0, sizeof(type));
    type.offset = entry_offset;
    if (!wasi__read_core_subtype_payload(reader, opcode, &type)) return 0;
    entries[index] = type;
    return wasi__read_component_core_type_entries(reader, entries, index + 1u, count);
}

static wasi_error_t wasi__read_nested_core_type_declaration(wasi_engine_t* engine,
                                                            wasi__reader_t* reader,
                                                            wasi_component_core_type_t* type) {
    uint8_t opcode;

    if (!engine || !reader) {
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component core type reader missing state");
    }

    if (type) type->offset = wasi__reader_offset(reader);
    opcode = wasi__read_u8(reader);
    if (reader->malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed nested core type declaration");
    }

    if (opcode != 0x50u) {
        return wasi__set_errorf(engine,
                                WASI_ERR_NOT_IMPLEMENTED,
                                "nested core type opcode 0x%02x not implemented",
                                (unsigned)opcode);
    }

    if (!wasi__read_core_module_type(reader, type)) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed nested core module type");
    }

    return WASI_OK;
}

static int wasi__read_component_externdesc(wasi__reader_t* reader, wasi_component_externdesc_t* desc) {
    uint8_t kind;

    if (desc) {
        memset(desc, 0, sizeof(*desc));
        desc->kind = WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
        desc->type_index = UINT32_MAX;
    }
    if (!reader) return 0;
    kind = wasi__read_u8(reader);
    if (reader->malformed) return 0;

    if (desc) {
        desc->raw_kind = kind;
        desc->kind = wasi__extern_kind_from_byte(kind);
    }

    switch (kind) {
        case 0x00:
            if (wasi__read_u8(reader) != 0x11u) {
                reader->malformed = 1;
                return 0;
            }
            if (desc) {
                desc->bound_tag = 0x11u;
                desc->has_type_index = 1;
                desc->type_index = wasi__read_leb128_u32(reader);
            } else {
                (void)wasi__read_leb128_u32(reader);
            }
            return !reader->malformed;
        case 0x01:
        case 0x04:
        case 0x05:
            if (desc) {
                desc->has_type_index = 1;
                desc->type_index = wasi__read_leb128_u32(reader);
            } else {
                (void)wasi__read_leb128_u32(reader);
            }
            return !reader->malformed;
        case 0x02: {
            uint8_t bound = wasi__read_u8(reader);
            if (reader->malformed) return 0;
            if (desc) desc->bound_tag = bound;
            if (bound == 0x00u) {
                if (desc) {
                    desc->has_type_index = 1;
                    desc->type_index = wasi__read_leb128_u32(reader);
                } else {
                    (void)wasi__read_leb128_u32(reader);
                }
                return !reader->malformed;
            }
            if (bound != 0x01u) {
                reader->malformed = 1;
                return 0;
            }
            return wasi__read_component_valtype(reader, desc ? &desc->value_type : NULL, NULL);
        }
        case 0x03: {
            uint8_t bound = wasi__read_u8(reader);
            if (reader->malformed) return 0;
            if (desc) desc->bound_tag = bound;
            if (bound == 0x00u) {
                if (desc) {
                    desc->has_type_index = 1;
                    desc->type_index = wasi__read_leb128_u32(reader);
                } else {
                    (void)wasi__read_leb128_u32(reader);
                }
                return !reader->malformed;
            }
            if (bound != 0x01u) {
                reader->malformed = 1;
                return 0;
            }
            return 1;
        }
        default:
            reader->malformed = 1;
            return 0;
    }
}

static int wasi__read_component_alias_inline(wasi__reader_t* reader, wasi_component_alias_t* alias) {
    size_t offset;
    uint8_t first;
    int sort_is_core;
    uint8_t alias_op;

    if (!reader) return 0;
    offset = wasi__reader_offset(reader);
    if (alias) {
        memset(alias, 0, sizeof(*alias));
        alias->offset = offset;
        alias->extern_kind = WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
        alias->core_export_kind = (wasm_export_kind_t)255;
        alias->instance_index = UINT32_MAX;
    }

    first = wasi__read_u8(reader);
    if (reader->malformed) return 0;
    sort_is_core = first == 0x00u;
    if (alias) alias->sort_is_core = sort_is_core;
    if (sort_is_core) {
        uint8_t sort_code = wasi__read_u8(reader);
        if (alias) {
            alias->sort_code = sort_code;
            if (sort_code <= 0x04u) alias->core_export_kind = (wasm_export_kind_t)sort_code;
        }
    } else if (alias) {
        alias->sort_code = first;
        alias->extern_kind = wasi__alias_component_sort_from_byte(first);
    }
    alias_op = wasi__read_u8(reader);
    if (reader->malformed) return 0;

    if ((sort_is_core && alias_op == 0x01u) || (!sort_is_core && alias_op == 0x00u)) {
        uint32_t instance_index;
        char* name = NULL;
        instance_index = wasi__read_leb128_u32(reader);
        if (reader->malformed) return 0;
        if (!wasi__read_component_plain_name(reader, &name)) return 0;
        if (alias) {
            alias->kind = sort_is_core ? WASI_COMPONENT_ALIAS_KIND_CORE_INSTANCE_EXPORT
                                       : WASI_COMPONENT_ALIAS_KIND_INSTANCE_EXPORT;
            alias->instance_index = instance_index;
            alias->name = name;
        } else {
            WASM_FREE(name);
        }
        return 1;
    }
    if (alias_op == 0x02u) {
        uint32_t outer_count = wasi__read_leb128_u32(reader);
        uint32_t outer_index = wasi__read_leb128_u32(reader);
        if (alias) {
            alias->kind = WASI_COMPONENT_ALIAS_KIND_OUTER;
            alias->outer_count = outer_count;
            alias->outer_index = outer_index;
        }
        return !reader->malformed;
    }

    reader->malformed = 1;
    return 0;
}

static wasi_error_t wasi__read_component_type_entry(wasi_engine_t* engine,
                                                    wasi__reader_t* reader,
                                                    wasi_component_type_t* type) {
    uint8_t opcode;
    uint32_t i;

    if (!engine || !reader || !type) {
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component type reader missing state");
    }

    memset(type, 0, sizeof(*type));
    type->offset = wasi__reader_offset(reader);
    opcode = wasi__read_u8(reader);
    if (reader->malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component type entry");
    }

    type->opcode = opcode;
    type->detail_opcode = opcode;
    switch (opcode) {
        case 0x40:
        case 0x43: {
            uint32_t param_count = wasi__read_leb128_u32(reader);
            type->kind = WASI_COMPONENT_TYPE_KIND_FUNC;
            type->async_flag = (uint8_t)(opcode == 0x43u);
            if (reader->malformed) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component func param count");
            }
            for (i = 0; i < param_count; i++) {
                char* name = NULL;
                uint8_t type_code;
                wasi_error_t err;

                if (!wasi__read_component_plain_name(reader, &name)) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component func param name");
                }
                wasi_component_valtype_t* param_type = NULL;

                if (!wasi__read_component_valtype(reader, &param_type, &type_code)) {
                    WASM_FREE(name);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component func param type");
                }
                err = wasi__func_type_append_named(&type->func.params,
                                                   &type->func.num_params,
                                                   &type->func.params_capacity,
                                                   name,
                                                   param_type);
                if (err != WASI_OK) {
                    WASM_FREE(name);
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "component func param alloc failed"
                                                                       : wasi_error_string(err));
                }
            }
            if (!wasi__read_component_resultlist(reader,
                                                 &type->func.results,
                                                 &type->func.num_results,
                                                 &type->func.results_capacity,
                                                 &type->item_count)) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component func result list");
            }
            return WASI_OK;
        }
        case 0x41:
        case 0x42: {
            uint32_t decl_count = wasi__read_leb128_u32(reader);
            type->kind = opcode == 0x41u ? WASI_COMPONENT_TYPE_KIND_COMPONENT : WASI_COMPONENT_TYPE_KIND_INSTANCE;
            type->item_count = decl_count;
            if (reader->malformed) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component type declaration count");
            }
            for (i = 0; i < decl_count; i++) {
                size_t decl_offset = wasi__reader_offset(reader);
                uint8_t decl_opcode = wasi__read_u8(reader);
                if (reader->malformed) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed nested type declaration");
                }
                switch (decl_opcode) {
                    case 0x00:
                        {
                            wasi_component_type_decl_t decl;
                            wasi_error_t err;
                            memset(&decl, 0, sizeof(decl));
                            decl.offset = decl_offset;
                            decl.kind = WASI_COMPONENT_TYPE_DECL_KIND_CORE_TYPE;
                            err = wasi__read_nested_core_type_declaration(engine,
                                                                          reader,
                                                                          &decl.data.core_type);
                            if (err != WASI_OK) return err;
                            err = wasi__component_type_append_decl(type, &decl);
                            if (err != WASI_OK) {
                                return wasi__set_error_literal(engine,
                                                               err,
                                                               err == WASI_ERR_OOM ? "component type declaration alloc failed"
                                                                                   : wasi_error_string(err));
                            }
                        }
                        break;
                    case 0x01: {
                        wasi_component_type_decl_t decl;
                        wasi_error_t err;
                        memset(&decl, 0, sizeof(decl));
                        decl.offset = decl_offset;
                        decl.kind = WASI_COMPONENT_TYPE_DECL_KIND_TYPE;
                        err = wasi__read_component_type_entry(engine, reader, &decl.data.type);
                        if (err != WASI_OK) return err;
                        err = wasi__component_type_append_decl(type, &decl);
                        if (err != WASI_OK) {
                            wasi__component_type_decl_release(&decl);
                            return wasi__set_error_literal(engine,
                                                           err,
                                                           err == WASI_ERR_OOM ? "component type declaration alloc failed"
                                                                               : wasi_error_string(err));
                        }
                        break;
                    }
                    case 0x02: {
                        wasi_component_type_decl_t decl;
                        wasi_error_t err;
                        memset(&decl, 0, sizeof(decl));
                        decl.offset = decl_offset;
                        decl.kind = WASI_COMPONENT_TYPE_DECL_KIND_ALIAS;
                        if (!wasi__read_component_alias_inline(reader, &decl.data.alias)) {
                            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed nested type alias");
                        }
                        err = wasi__component_type_append_decl(type, &decl);
                        if (err != WASI_OK) {
                            wasi__component_type_decl_release(&decl);
                            return wasi__set_error_literal(engine,
                                                           err,
                                                           err == WASI_ERR_OOM ? "component type declaration alloc failed"
                                                                               : wasi_error_string(err));
                        }
                        break;
                    }
                    case 0x03:
                        if (opcode != 0x41u) {
                            return wasi__set_error_literal(engine,
                                                           WASI_ERR_MALFORMED,
                                                           "instance type import declaration is invalid");
                        }
                        {
                            wasi_component_type_decl_t decl;
                            wasi_error_t err;
                            memset(&decl, 0, sizeof(decl));
                            decl.offset = decl_offset;
                            decl.kind = WASI_COMPONENT_TYPE_DECL_KIND_IMPORT;
                            if (!wasi__read_component_name(reader, &decl.name_kind, &decl.name)) {
                                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component type import name");
                            }
                            wasi__resolve_interface_name(decl.name, &decl.interface_name, &decl.interface_version);
                            if (!wasi__read_component_externdesc(reader, &decl.data.externdesc)) {
                                wasi__component_type_decl_release(&decl);
                                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component type import descriptor");
                            }
                            err = wasi__component_type_append_decl(type, &decl);
                            if (err != WASI_OK) {
                                wasi__component_type_decl_release(&decl);
                                return wasi__set_error_literal(engine,
                                                               err,
                                                               err == WASI_ERR_OOM ? "component type declaration alloc failed"
                                                                                   : wasi_error_string(err));
                            }
                        }
                        break;
                    case 0x04: {
                        wasi_component_type_decl_t decl;
                        wasi_error_t err;
                        memset(&decl, 0, sizeof(decl));
                        decl.offset = decl_offset;
                        decl.kind = WASI_COMPONENT_TYPE_DECL_KIND_EXPORT;
                        if (!wasi__read_component_name(reader, &decl.name_kind, &decl.name)) {
                            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component type export name");
                        }
                        wasi__resolve_interface_name(decl.name, &decl.interface_name, &decl.interface_version);
                        if (!wasi__read_component_externdesc(reader, &decl.data.externdesc)) {
                            wasi__component_type_decl_release(&decl);
                            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component type export descriptor");
                        }
                        err = wasi__component_type_append_decl(type, &decl);
                        if (err != WASI_OK) {
                            wasi__component_type_decl_release(&decl);
                            return wasi__set_error_literal(engine,
                                                           err,
                                                           err == WASI_ERR_OOM ? "component type declaration alloc failed"
                                                                               : wasi_error_string(err));
                        }
                        break;
                    }
                    default:
                        return wasi__set_errorf(engine,
                                                WASI_ERR_NOT_IMPLEMENTED,
                                                "nested type declaration opcode 0x%02x not implemented",
                                                (unsigned)decl_opcode);
                }
            }
            return WASI_OK;
        }
        case 0x3F:
        case 0x3E: {
            uint8_t rep = wasi__read_u8(reader);
            uint8_t tag;

            type->kind = WASI_COMPONENT_TYPE_KIND_RESOURCE;
            type->async_flag = (uint8_t)(opcode == 0x3Eu);
            type->detail_opcode = rep;
            if (reader->malformed || rep != 0x7Fu) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed resource type representation");
            }

            tag = wasi__read_u8(reader);
            if (reader->malformed) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed resource type destructor tag");
            }
            if (tag == 0x01u) {
                type->has_primary_index = 1;
                type->primary_index = wasi__read_leb128_u32(reader);
                if (reader->malformed) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed resource type destructor index");
                }
            } else if (tag != 0x00u) {
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "invalid resource type destructor tag");
            }

            if (opcode == 0x3Eu) {
                uint8_t callback_tag = wasi__read_u8(reader);
                if (reader->malformed) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed async resource callback tag");
                }
                if (callback_tag == 0x01u) {
                    type->has_secondary_index = 1;
                    type->secondary_index = wasi__read_leb128_u32(reader);
                    if (reader->malformed) {
                        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed async resource callback index");
                    }
                } else if (callback_tag != 0x00u) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "invalid async resource callback tag");
                }
            }
            return WASI_OK;
        }
        default:
            type->kind = WASI_COMPONENT_TYPE_KIND_DEFINED;
            reader->ptr--;
            if (!wasi__read_component_valtype(reader, &type->defined_type, &type->detail_opcode)) {
                return wasi__set_errorf(engine,
                                        WASI_ERR_NOT_IMPLEMENTED,
                                        "component type opcode 0x%02x not implemented",
                                        (unsigned)opcode);
            }
            return WASI_OK;
    }
}

static wasi_error_t wasi__parse_component_types(wasi_engine_t* engine,
                                                wasi_component_t* component,
                                                const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.base = component->bytes;
    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component type section");
    }

    for (i = 0; i < count; i++) {
        wasi_component_type_t type;
        wasi_error_t err = wasi__read_component_type_entry(engine, &reader, &type);
        if (err != WASI_OK) {
            wasi__component_type_release(&type);
            return err;
        }
        err = wasi__component_append_type(component, &type);
        if (err != WASI_OK) {
            wasi__component_type_release(&type);
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

static wasi_error_t wasi__parse_component_core_types(wasi_engine_t* engine,
                                                     wasi_component_t* component,
                                                     const wasi_component_section_t* section) {
    wasi__reader_t reader;
    wasi_component_core_type_t* entries;
    uint32_t count;
    uint32_t i;

    reader.base = component->bytes;
    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core type section");
    }

    entries = (wasi_component_core_type_t*)WASM_CALLOC(count ? count : 1u, sizeof(*entries));
    if (!entries) {
        return wasi__set_error_literal(engine, WASI_ERR_OOM, "core type alloc failed");
    }

    if (!wasi__read_component_core_type_entries(&reader, entries, 0, count)) {
        WASM_FREE(entries);
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core type section");
    }

    for (i = 0; i < count; i++) {
        wasi_error_t err = wasi__component_append_core_type(component, &entries[i]);
        if (err != WASI_OK) {
            WASM_FREE(entries);
            return wasi__set_error_literal(engine,
                                           err,
                                           err == WASI_ERR_OOM ? "core type alloc failed"
                                                               : wasi_error_string(err));
        }
    }

    WASM_FREE(entries);
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
        size_t entry_offset = wasi__reader_offset(&reader);
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
                                               entry_offset,
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
                                               entry_offset,
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

    reader.base = component->bytes;
    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed core instance section");
    }

    for (i = 0; i < count; i++) {
        size_t entry_offset = wasi__reader_offset(&reader);
        uint8_t opcode = wasi__read_u8(&reader);
        wasi_component_core_instance_t instance;
        wasi_error_t err;
        memset(&instance, 0, sizeof(instance));
        instance.offset = entry_offset;
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

    reader.base = component->bytes;
    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component instance section");
    }

    for (i = 0; i < count; i++) {
        size_t entry_offset = wasi__reader_offset(&reader);
        uint8_t opcode = wasi__read_u8(&reader);
        wasi_component_instance_t instance;
        wasi_error_t err;

        memset(&instance, 0, sizeof(instance));
        instance.offset = entry_offset;
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

    reader.base = component->bytes;
    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    component->start_offset = wasi__reader_offset(&reader);
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

static int wasi__skip_component_resultlist(wasi__reader_t* reader, uint32_t* out_count) {
    wasi_component_named_type_t* items = NULL;
    uint32_t count = 0;
    uint32_t capacity = 0;
    uint32_t item_count = 0;
    uint32_t i;
    int ok;

    ok = wasi__read_component_resultlist(reader, &items, &count, &capacity, &item_count);
    for (i = 0; i < count; i++) {
        WASM_FREE(items[i].name);
        wasi__component_valtype_release(items[i].type);
    }
    WASM_FREE(items);
    if (out_count) *out_count = item_count;
    return ok;
}

static wasi_error_t wasi__read_canon_options_count(wasi_engine_t* engine,
                                                   wasi__reader_t* reader,
                                                   wasi_component_canon_t* canon,
                                                   uint32_t option_count,
                                                   const char* malformed_msg,
                                                   const char* option_msg) {
    uint32_t opt;

    for (opt = 0; opt < option_count; opt++) {
        uint8_t option = wasi__read_u8(reader);
        uint32_t immediate = 0;
        wasi_error_t err;

        if (reader->malformed) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, malformed_msg);
        }

        switch (option) {
            case 0x00:
            case 0x01:
            case 0x02:
            case 0x06:
                break;
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x07:
                immediate = wasi__read_leb128_u32(reader);
                if (reader->malformed) {
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, malformed_msg);
                }
                break;
            default:
                return wasi__set_errorf(engine,
                                        WASI_ERR_NOT_IMPLEMENTED,
                                        "%s 0x%02x not implemented",
                                        option_msg,
                                        (unsigned)option);
        }

        if (option == 0x06u) canon->async_flag = 1u;
        err = wasi__canon_append_option(canon,
                                        option,
                                        wasi__canon_option_has_immediate(option),
                                        immediate);
        if (err != WASI_OK) {
            return wasi__set_error_literal(engine,
                                           err,
                                           err == WASI_ERR_OOM ? "canonical option alloc failed"
                                                               : wasi_error_string(err));
        }
    }

    return WASI_OK;
}

static wasi_error_t wasi__read_canon_options(wasi_engine_t* engine,
                                             wasi__reader_t* reader,
                                             wasi_component_canon_t* canon,
                                             const char* count_msg,
                                             const char* malformed_msg,
                                             const char* option_msg) {
    uint32_t option_count = wasi__read_leb128_u32(reader);
    if (reader->malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, count_msg);
    }
    return wasi__read_canon_options_count(engine,
                                          reader,
                                          canon,
                                          option_count,
                                          malformed_msg,
                                          option_msg);
}

static wasi_error_t wasi__parse_component_canon(wasi_engine_t* engine,
                                                wasi_component_t* component,
                                                const wasi_component_section_t* section) {
    wasi__reader_t reader;
    uint32_t count;
    uint32_t i;

    reader.base = component->bytes;
    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canonical function section");
    }

    for (i = 0; i < count; i++) {
        size_t entry_offset = wasi__reader_offset(&reader);
        uint8_t opcode = wasi__read_u8(&reader);
        wasi_component_canon_t canon;
        wasi_error_t err = WASI_OK;
        if (reader.malformed) {
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canonical function opcode");
        }
        memset(&canon, 0, sizeof(canon));
        canon.offset = entry_offset;
        canon.opcode = opcode;
        canon.func_index = UINT32_MAX;
        canon.core_func_index = UINT32_MAX;
        canon.type_index = UINT32_MAX;
        canon.defined_func_index = UINT32_MAX;
        if (opcode == 0x00u || opcode == 0x01u) {
            uint8_t marker = wasi__read_u8(&reader);
            if (reader.malformed) {
                return wasi__set_error_literal(engine,
                                               WASI_ERR_MALFORMED,
                                               opcode == 0x00u ? "malformed canon lift header" : "malformed canon lower header");
            }

            canon.kind = opcode == 0x00u ? WASI_COMPONENT_CANON_KIND_LIFT : WASI_COMPONENT_CANON_KIND_LOWER;
            if (marker == 0x00u) {
                if (opcode == 0x00u) canon.core_func_index = wasi__read_leb128_u32(&reader);
                else canon.func_index = wasi__read_leb128_u32(&reader);
                if (reader.malformed) {
                    return wasi__set_error_literal(engine,
                                                   WASI_ERR_MALFORMED,
                                                   opcode == 0x00u ? "malformed canon lift header" : "malformed canon lower header");
                }
                err = wasi__read_canon_options(engine,
                                              &reader,
                                              &canon,
                                              opcode == 0x00u ? "malformed canon lift option count" : "malformed canon lower option count",
                                              opcode == 0x00u ? "malformed canon lift option" : "malformed canon lower option",
                                              "canonical option");
                if (err != WASI_OK) {
                    WASM_FREE(canon.options);
                    return err;
                }
            } else {
                uint32_t option_count;
                canon.async_flag = marker;
                if (opcode == 0x00u) canon.core_func_index = wasi__read_leb128_u32(&reader);
                else canon.func_index = wasi__read_leb128_u32(&reader);
                option_count = wasi__read_leb128_u32(&reader);
                if (reader.malformed) {
                    WASM_FREE(canon.options);
                    return wasi__set_error_literal(engine,
                                                   WASI_ERR_MALFORMED,
                                                   opcode == 0x00u ? "malformed canon lift header" : "malformed canon lower header");
                }
                err = wasi__read_canon_options_count(engine,
                                                    &reader,
                                                    &canon,
                                                    option_count,
                                                    opcode == 0x00u ? "malformed canon lift option" : "malformed canon lower option",
                                                    "canonical option");
                if (err != WASI_OK) {
                    WASM_FREE(canon.options);
                    return err;
                }
            }

            if (opcode == 0x00u) {
                canon.has_type_index = 1;
                canon.type_index = wasi__read_leb128_u32(&reader);
                if (reader.malformed) {
                    WASM_FREE(canon.options);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canon lift type index");
                }
                canon.defined_func_index = component->num_funcs;
                err = wasi__component_append_func(component, entry_offset, canon.type_index);
                if (err != WASI_OK) {
                    WASM_FREE(canon.options);
                    return wasi__set_error_literal(engine,
                                                   err,
                                                   err == WASI_ERR_OOM ? "component func alloc failed"
                                                                       : wasi_error_string(err));
                }
            }
        } else {
            switch (opcode) {
                case 0x02u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_RESOURCE_NEW;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    break;
                case 0x03u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_RESOURCE_DROP;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    break;
                case 0x04u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_RESOURCE_REP;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    break;
                case 0x05u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_TASK_CANCEL;
                    break;
                case 0x06u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_SUBTASK_CANCEL;
                    canon.flag = wasi__read_u8(&reader);
                    break;
                case 0x08u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_BACKPRESSURE_SET;
                    break;
                case 0x09u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_TASK_RETURN;
                    if (!wasi__skip_component_resultlist(&reader, &canon.item_count)) {
                        WASM_FREE(canon.options);
                        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canon task.return result list");
                    }
                    err = wasi__read_canon_options(engine,
                                                  &reader,
                                                  &canon,
                                                  "malformed canon task.return option count",
                                                  "malformed canon task.return option",
                                                  "canonical option");
                    if (err != WASI_OK) {
                        WASM_FREE(canon.options);
                        return err;
                    }
                    break;
                case 0x0Au:
                    canon.kind = WASI_COMPONENT_CANON_KIND_CONTEXT_GET;
                    if (wasi__read_u8(&reader) != 0x7Fu) reader.malformed = 1;
                    canon.secondary_index = wasi__read_leb128_u32(&reader);
                    canon.has_secondary_index = 1;
                    break;
                case 0x0Bu:
                    canon.kind = WASI_COMPONENT_CANON_KIND_CONTEXT_SET;
                    if (wasi__read_u8(&reader) != 0x7Fu) reader.malformed = 1;
                    canon.secondary_index = wasi__read_leb128_u32(&reader);
                    canon.has_secondary_index = 1;
                    break;
                case 0x0Cu:
                    canon.kind = WASI_COMPONENT_CANON_KIND_TASK_YIELD;
                    canon.flag = wasi__read_u8(&reader);
                    break;
                case 0x0Du:
                    canon.kind = WASI_COMPONENT_CANON_KIND_SUBTASK_DROP;
                    break;
                case 0x0Eu:
                    canon.kind = WASI_COMPONENT_CANON_KIND_STREAM_NEW;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    break;
                case 0x0Fu:
                    canon.kind = WASI_COMPONENT_CANON_KIND_STREAM_READ;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    err = wasi__read_canon_options(engine,
                                                  &reader,
                                                  &canon,
                                                  "malformed canon stream.read option count",
                                                  "malformed canon stream.read option",
                                                  "canonical option");
                    if (err != WASI_OK) {
                        WASM_FREE(canon.options);
                        return err;
                    }
                    break;
                case 0x10u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_STREAM_WRITE;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    err = wasi__read_canon_options(engine,
                                                  &reader,
                                                  &canon,
                                                  "malformed canon stream.write option count",
                                                  "malformed canon stream.write option",
                                                  "canonical option");
                    if (err != WASI_OK) {
                        WASM_FREE(canon.options);
                        return err;
                    }
                    break;
                case 0x11u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_STREAM_CANCEL_READ;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    canon.flag = wasi__read_u8(&reader);
                    break;
                case 0x12u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_STREAM_CANCEL_WRITE;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    canon.flag = wasi__read_u8(&reader);
                    break;
                case 0x13u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_STREAM_DROP_READABLE;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    break;
                case 0x14u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_STREAM_DROP_WRITABLE;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    break;
                case 0x15u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_FUTURE_NEW;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    break;
                case 0x16u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_FUTURE_READ;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    err = wasi__read_canon_options(engine,
                                                  &reader,
                                                  &canon,
                                                  "malformed canon future.read option count",
                                                  "malformed canon future.read option",
                                                  "canonical option");
                    if (err != WASI_OK) {
                        WASM_FREE(canon.options);
                        return err;
                    }
                    break;
                case 0x17u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_FUTURE_WRITE;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    err = wasi__read_canon_options(engine,
                                                  &reader,
                                                  &canon,
                                                  "malformed canon future.write option count",
                                                  "malformed canon future.write option",
                                                  "canonical option");
                    if (err != WASI_OK) {
                        WASM_FREE(canon.options);
                        return err;
                    }
                    break;
                case 0x18u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_FUTURE_CANCEL_READ;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    canon.flag = wasi__read_u8(&reader);
                    break;
                case 0x19u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_FUTURE_CANCEL_WRITE;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    canon.flag = wasi__read_u8(&reader);
                    break;
                case 0x1Au:
                    canon.kind = WASI_COMPONENT_CANON_KIND_FUTURE_DROP_READABLE;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    break;
                case 0x1Bu:
                    canon.kind = WASI_COMPONENT_CANON_KIND_FUTURE_DROP_WRITABLE;
                    canon.has_type_index = 1;
                    canon.type_index = wasi__read_leb128_u32(&reader);
                    break;
                case 0x1Cu:
                    canon.kind = WASI_COMPONENT_CANON_KIND_ERROR_CONTEXT_NEW;
                    err = wasi__read_canon_options(engine,
                                                  &reader,
                                                  &canon,
                                                  "malformed canon error-context.new option count",
                                                  "malformed canon error-context.new option",
                                                  "canonical option");
                    if (err != WASI_OK) {
                        WASM_FREE(canon.options);
                        return err;
                    }
                    break;
                case 0x1Du:
                    canon.kind = WASI_COMPONENT_CANON_KIND_ERROR_CONTEXT_DEBUG_MESSAGE;
                    err = wasi__read_canon_options(engine,
                                                  &reader,
                                                  &canon,
                                                  "malformed canon error-context.debug-message option count",
                                                  "malformed canon error-context.debug-message option",
                                                  "canonical option");
                    if (err != WASI_OK) {
                        WASM_FREE(canon.options);
                        return err;
                    }
                    break;
                case 0x1Eu:
                    canon.kind = WASI_COMPONENT_CANON_KIND_ERROR_CONTEXT_DROP;
                    break;
                case 0x1Fu:
                    canon.kind = WASI_COMPONENT_CANON_KIND_WAITABLE_SET_NEW;
                    break;
                case 0x20u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_WAITABLE_SET_WAIT;
                    canon.flag = wasi__read_u8(&reader);
                    canon.secondary_index = wasi__read_leb128_u32(&reader);
                    canon.has_secondary_index = 1;
                    break;
                case 0x21u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_WAITABLE_SET_POLL;
                    canon.flag = wasi__read_u8(&reader);
                    canon.secondary_index = wasi__read_leb128_u32(&reader);
                    canon.has_secondary_index = 1;
                    break;
                case 0x22u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_WAITABLE_SET_DROP;
                    break;
                case 0x23u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_WAITABLE_JOIN;
                    break;
                case 0x24u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_BACKPRESSURE_INC;
                    break;
                case 0x25u:
                    canon.kind = WASI_COMPONENT_CANON_KIND_BACKPRESSURE_DEC;
                    break;
                default:
                    return wasi__set_errorf(engine,
                                            WASI_ERR_NOT_IMPLEMENTED,
                                            "canonical opcode 0x%02x not implemented",
                                            (unsigned)opcode);
            }

            if (reader.malformed) {
                WASM_FREE(canon.options);
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed canonical function payload");
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

    reader.base = component->bytes;
    reader.ptr = component->bytes + section->payload_offset;
    reader.end = reader.ptr + section->payload_size;
    reader.malformed = 0;
    count = wasi__read_leb128_u32(&reader);
    if (reader.malformed) {
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "malformed component export section");
    }

    for (i = 0; i < count; i++) {
        size_t entry_offset = wasi__reader_offset(&reader);
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

        err = wasi__component_append_export(component,
                            entry_offset,
                            name,
                            name_kind,
                            kind,
                            index,
                            has_type,
                            type_index);
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
        else if (section->id == 3)
            err = wasi__parse_component_core_types(engine, component, section);
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

    component->status = WASI_COMPONENT_STATUS_PARSED_STRUCTURE;
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

static const char* wasi__component_type_detail_string(uint8_t opcode) {
    switch (opcode) {
        case 0x00:
            return "type-index";
        case 0x64:
            return "error-context";
        case 0x65:
            return "future";
        case 0x66:
            return "stream";
        case 0x67:
            return "fixed-list";
        case 0x68:
            return "borrow";
        case 0x69:
            return "own";
        case 0x6A:
            return "result";
        case 0x6B:
            return "option";
        case 0x6D:
            return "enum";
        case 0x6E:
            return "flags";
        case 0x6F:
            return "tuple";
        case 0x70:
            return "list";
        case 0x71:
            return "variant";
        case 0x72:
            return "record";
        default:
            return wasi_component_primitive_type_string(opcode);
    }
}

static const char* wasi__component_core_type_detail_string(uint8_t opcode) {
    switch (opcode) {
        case 0x50:
            return "module";
        case 0x60:
            return "func";
        case 0x5F:
            return "struct";
        case 0x5E:
            return "array";
        case 0x5D:
            return "cont";
        default:
            return "unknown";
    }
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

#define WASI__CABI_REALLOC_EXPORT "cabi_realloc"
#define WASI__CANON_STRING_UTF16_TAG 0x80000000u
#define WASI__CANON_STRING_MAX_CODE_UNITS 0x7FFFFFFFu
#define WASI__CANON_MAX_FLAT_PARAMS 16u
#define WASI__CANON_MAX_FLAT_RESULTS 1u

typedef struct wasi__canon_runtime_options_t {
    wasi_canon_options_t api;
    int has_realloc_func_index;
    uint32_t realloc_func_index;
    int has_post_return_func_index;
    uint32_t post_return_func_index;
} wasi__canon_runtime_options_t;

struct wasi_instance_t {
    const wasi_component_t* component;
    wasm_module_t* core_module;
    uint32_t core_module_index;
};

static void wasi__value_clear(wasi_value_t* value) {
    size_t i;

    if (!value) return;

    switch (value->kind) {
        case WASI_VALUE_KIND_STRING:
            if (value->of.string.owned && value->of.string.data) WASM_FREE(value->of.string.data);
            break;
        case WASI_VALUE_KIND_FIXED_LIST:
        case WASI_VALUE_KIND_TUPLE:
        case WASI_VALUE_KIND_LIST:
        case WASI_VALUE_KIND_RECORD:
            if (value->of.seq.owned && value->of.seq.values) {
                for (i = 0; i < value->of.seq.len; i++) wasi__value_clear(&value->of.seq.values[i]);
                WASM_FREE(value->of.seq.values);
            }
            break;
        case WASI_VALUE_KIND_FLAGS:
            if (value->of.flags.owned && value->of.flags.words) WASM_FREE(value->of.flags.words);
            break;
        case WASI_VALUE_KIND_RESULT:
        case WASI_VALUE_KIND_OPTION:
        case WASI_VALUE_KIND_VARIANT:
            if (value->of.variant.payload_owned && value->of.variant.payload) {
                wasi__value_clear(value->of.variant.payload);
                WASM_FREE(value->of.variant.payload);
            }
            break;
        default:
            break;
    }

    memset(value, 0, sizeof(*value));
}

void wasi_value_init(wasi_value_t* value) {
    if (!value) return;
    memset(value, 0, sizeof(*value));
}

void wasi_value_destroy(wasi_value_t* value) {
    wasi__value_clear(value);
}

wasi_error_t wasi_value_set_string_copy(wasi_value_t* value, const char* bytes, size_t len) {
    char* copy;

    if (!value) return WASI_ERR_INVALID_ARGUMENT;
    if (!bytes && len) return WASI_ERR_INVALID_ARGUMENT;
    if (len > ((size_t)-1) - 1u) return WASI_ERR_OOM;

    copy = (char*)WASM_MALLOC(len + 1u);
    if (!copy) return WASI_ERR_OOM;
    if (len) memcpy(copy, bytes, len);
    copy[len] = '\0';

    wasi__value_clear(value);
    value->kind = WASI_VALUE_KIND_STRING;
    value->of.string.data = copy;
    value->of.string.len = len;
    value->of.string.owned = 1;
    return WASI_OK;
}

wasi_error_t wasi_canon_options_default(wasi_canon_options_t* options) {
    if (!options) return WASI_ERR_INVALID_ARGUMENT;
    memset(options, 0, sizeof(*options));
    options->cabi_realloc_name = WASI__CABI_REALLOC_EXPORT;
    options->string_encoding = WASI_STRING_ENCODING_UTF8;
    return WASI_OK;
}

static int wasi__is_valid_unicode_scalar(uint32_t codepoint) {
    return codepoint <= 0x10FFFFu && !(codepoint >= 0xD800u && codepoint <= 0xDFFFu);
}

static int wasi__utf8_decode_one(const uint8_t* bytes, size_t len, size_t* cursor, uint32_t* out_codepoint) {
    size_t index;
    uint8_t b0;
    uint32_t codepoint;

    if (!bytes || !cursor || !out_codepoint || *cursor >= len) return 0;

    index = *cursor;
    b0 = bytes[index++];
    if ((b0 & 0x80u) == 0) {
        *cursor = index;
        *out_codepoint = b0;
        return 1;
    }

    if ((b0 & 0xE0u) == 0xC0u) {
        uint8_t b1;
        if (index >= len) return 0;
        b1 = bytes[index++];
        if ((b1 & 0xC0u) != 0x80u) return 0;
        codepoint = (uint32_t)(b0 & 0x1Fu) << 6;
        codepoint |= (uint32_t)(b1 & 0x3Fu);
        if (codepoint < 0x80u) return 0;
        *cursor = index;
        *out_codepoint = codepoint;
        return 1;
    }

    if ((b0 & 0xF0u) == 0xE0u) {
        uint8_t b1;
        uint8_t b2;
        if (index + 1u >= len) return 0;
        b1 = bytes[index++];
        b2 = bytes[index++];
        if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u) return 0;
        codepoint = (uint32_t)(b0 & 0x0Fu) << 12;
        codepoint |= (uint32_t)(b1 & 0x3Fu) << 6;
        codepoint |= (uint32_t)(b2 & 0x3Fu);
        if (codepoint < 0x800u || !wasi__is_valid_unicode_scalar(codepoint)) return 0;
        *cursor = index;
        *out_codepoint = codepoint;
        return 1;
    }

    if ((b0 & 0xF8u) == 0xF0u) {
        uint8_t b1;
        uint8_t b2;
        uint8_t b3;
        if (index + 2u >= len) return 0;
        b1 = bytes[index++];
        b2 = bytes[index++];
        b3 = bytes[index++];
        if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u || (b3 & 0xC0u) != 0x80u) return 0;
        codepoint = (uint32_t)(b0 & 0x07u) << 18;
        codepoint |= (uint32_t)(b1 & 0x3Fu) << 12;
        codepoint |= (uint32_t)(b2 & 0x3Fu) << 6;
        codepoint |= (uint32_t)(b3 & 0x3Fu);
        if (codepoint < 0x10000u || !wasi__is_valid_unicode_scalar(codepoint)) return 0;
        *cursor = index;
        *out_codepoint = codepoint;
        return 1;
    }

    return 0;
}

static int wasi__utf8_validate(const char* bytes, size_t len) {
    size_t cursor = 0;
    uint32_t codepoint;

    while (cursor < len) {
        if (!wasi__utf8_decode_one((const uint8_t*)bytes, len, &cursor, &codepoint)) return 0;
    }
    return 1;
}

static size_t wasi__utf8_encode_one(uint32_t codepoint, char out[4]) {
    if (codepoint <= 0x7Fu) {
        out[0] = (char)codepoint;
        return 1u;
    }
    if (codepoint <= 0x7FFu) {
        out[0] = (char)(0xC0u | (codepoint >> 6));
        out[1] = (char)(0x80u | (codepoint & 0x3Fu));
        return 2u;
    }
    if (codepoint <= 0xFFFFu) {
        out[0] = (char)(0xE0u | (codepoint >> 12));
        out[1] = (char)(0x80u | ((codepoint >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (codepoint & 0x3Fu));
        return 3u;
    }

    out[0] = (char)(0xF0u | (codepoint >> 18));
    out[1] = (char)(0x80u | ((codepoint >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((codepoint >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (codepoint & 0x3Fu));
    return 4u;
}

static wasi_error_t wasi__utf8_to_utf16le(wasi_engine_t* engine,
                                          const char* utf8,
                                          size_t utf8_len,
                                          uint8_t** out_bytes,
                                          size_t* out_len) {
    uint8_t* bytes;
    size_t capacity;
    size_t cursor = 0;
    size_t written = 0;
    uint32_t codepoint;

    if (out_bytes) *out_bytes = NULL;
    if (out_len) *out_len = 0;
    if (!out_bytes || !out_len) return WASI_ERR_INVALID_ARGUMENT;
    if (!utf8 && utf8_len) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "string bytes must not be NULL");
    if (utf8_len > ((size_t)-1) / 2u) return wasi__set_error_literal(engine, WASI_ERR_OOM, "utf16 conversion overflow");

    capacity = utf8_len ? utf8_len * 2u : 1u;
    bytes = (uint8_t*)WASM_MALLOC(capacity);
    if (!bytes) return wasi__set_error_literal(engine, WASI_ERR_OOM, "utf16 conversion alloc failed");

    while (cursor < utf8_len) {
        if (!wasi__utf8_decode_one((const uint8_t*)utf8, utf8_len, &cursor, &codepoint)) {
            WASM_FREE(bytes);
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "invalid utf8 string");
        }
        if (codepoint <= 0xFFFFu) {
            bytes[written++] = (uint8_t)(codepoint & 0xFFu);
            bytes[written++] = (uint8_t)((codepoint >> 8) & 0xFFu);
        } else {
            uint32_t payload = codepoint - 0x10000u;
            uint16_t high = (uint16_t)(0xD800u + (payload >> 10));
            uint16_t low = (uint16_t)(0xDC00u + (payload & 0x3FFu));
            bytes[written++] = (uint8_t)(high & 0xFFu);
            bytes[written++] = (uint8_t)((high >> 8) & 0xFFu);
            bytes[written++] = (uint8_t)(low & 0xFFu);
            bytes[written++] = (uint8_t)((low >> 8) & 0xFFu);
        }
    }

    *out_bytes = bytes;
    *out_len = written;
    return WASI_OK;
}

static wasi_error_t wasi__utf16le_to_utf8(wasi_engine_t* engine,
                                          const uint8_t* utf16,
                                          size_t code_units,
                                          char** out_utf8,
                                          size_t* out_len) {
    char* utf8;
    size_t capacity;
    size_t index = 0;
    size_t written = 0;

    if (out_utf8) *out_utf8 = NULL;
    if (out_len) *out_len = 0;
    if (!out_utf8 || !out_len) return WASI_ERR_INVALID_ARGUMENT;
    if (!utf16 && code_units) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "utf16 bytes must not be NULL");
    if (code_units > (((size_t)-1) - 1u) / 3u)
        return wasi__set_error_literal(engine, WASI_ERR_OOM, "utf8 conversion overflow");

    capacity = code_units * 3u + 1u;
    utf8 = (char*)WASM_MALLOC(capacity);
    if (!utf8) return wasi__set_error_literal(engine, WASI_ERR_OOM, "utf8 conversion alloc failed");

    while (index < code_units) {
        uint16_t unit = (uint16_t)utf16[index * 2u] | ((uint16_t)utf16[index * 2u + 1u] << 8);
        uint32_t codepoint = unit;
        char encoded[4];
        size_t encoded_len;

        index++;
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
            uint16_t low;
            if (index >= code_units) {
                WASM_FREE(utf8);
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "unterminated utf16 surrogate pair");
            }
            low = (uint16_t)utf16[index * 2u] | ((uint16_t)utf16[index * 2u + 1u] << 8);
            if (low < 0xDC00u || low > 0xDFFFu) {
                WASM_FREE(utf8);
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "invalid utf16 surrogate pair");
            }
            index++;
            codepoint = 0x10000u + (((uint32_t)(unit - 0xD800u)) << 10) + (uint32_t)(low - 0xDC00u);
        } else if (unit >= 0xDC00u && unit <= 0xDFFFu) {
            WASM_FREE(utf8);
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "unexpected utf16 low surrogate");
        }

        encoded_len = wasi__utf8_encode_one(codepoint, encoded);
        memcpy(utf8 + written, encoded, encoded_len);
        written += encoded_len;
    }

    utf8[written] = '\0';
    *out_utf8 = utf8;
    *out_len = written;
    return WASI_OK;
}

static wasi_error_t wasi__latin1_to_utf8(wasi_engine_t* engine,
                                         const uint8_t* latin1,
                                         size_t latin1_len,
                                         char** out_utf8,
                                         size_t* out_len) {
    char* utf8;
    size_t capacity;
    size_t i;
    size_t written = 0;

    if (out_utf8) *out_utf8 = NULL;
    if (out_len) *out_len = 0;
    if (!out_utf8 || !out_len) return WASI_ERR_INVALID_ARGUMENT;
    if (!latin1 && latin1_len)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "latin1 bytes must not be NULL");
    if (latin1_len > (((size_t)-1) - 1u) / 2u)
        return wasi__set_error_literal(engine, WASI_ERR_OOM, "utf8 conversion overflow");

    capacity = latin1_len * 2u + 1u;
    utf8 = (char*)WASM_MALLOC(capacity);
    if (!utf8) return wasi__set_error_literal(engine, WASI_ERR_OOM, "utf8 conversion alloc failed");

    for (i = 0; i < latin1_len; i++) {
        uint8_t byte = latin1[i];
        if (byte < 0x80u) {
            utf8[written++] = (char)byte;
        } else {
            utf8[written++] = (char)(0xC0u | (byte >> 6));
            utf8[written++] = (char)(0x80u | (byte & 0x3Fu));
        }
    }

    utf8[written] = '\0';
    *out_utf8 = utf8;
    *out_len = written;
    return WASI_OK;
}

static wasi_error_t wasi__set_runtime_error_from_module(wasi_engine_t* engine,
                                                        const wasm_module_t* core_module,
                                                        wasm_error_t wasm_err,
                                                        const char* action) {
    const char* detail = wasm_error_string(wasm_err);

    if (core_module && core_module->rt && core_module->rt->error_msg[0]) detail = core_module->rt->error_msg;
    return wasi__set_errorf(engine, WASI_ERR_RUNTIME, "%s: %s", action, detail);
}

static int wasi__component_type_is_func(const wasi_component_t* component, uint32_t type_index) {
    return component && type_index < component->num_types && component->types[type_index].kind == WASI_COMPONENT_TYPE_KIND_FUNC;
}

typedef enum wasi__canon_result_abi_t {
    WASI__CANON_RESULT_ABI_FLAT = 0,
    WASI__CANON_RESULT_ABI_SPILL_PARAM = 1,
    WASI__CANON_RESULT_ABI_SPILL_RETURN = 2,
} wasi__canon_result_abi_t;

typedef struct wasi__canon_func_sig_t {
    int params_spilled;
    int results_spilled;
    int results_retptr_returned;
    wasm_valtype_t* param_types;
    uint32_t num_param_types;
    wasm_valtype_t* result_types;
    uint32_t num_result_types;
    wasm_valtype_t* post_return_types;
    uint32_t num_post_return_types;
} wasi__canon_func_sig_t;

static const wasi_component_valtype_t* wasi__resolve_valtype(const wasi_component_t* component,
                                                             const wasi_component_valtype_t* type) {
    const wasi_component_valtype_t* resolved = type;
    uint32_t depth = 0;

    while (resolved && resolved->opcode == 0x00u && component && depth < 32u) {
        uint32_t type_index = resolved->data.index.type_index;
        if (type_index >= component->num_types) break;
        if (!component->types[type_index].defined_type) break;
        resolved = component->types[type_index].defined_type;
        depth++;
    }

    return resolved;
}

static wasi_value_kind_t wasi__variant_value_kind(uint8_t opcode) {
    switch (opcode) {
        case 0x71u:
            return WASI_VALUE_KIND_VARIANT;
        case 0x6Bu:
            return WASI_VALUE_KIND_OPTION;
        case 0x6Au:
            return WASI_VALUE_KIND_RESULT;
        default:
            return 0;
    }
}

static uint32_t wasi__variant_case_count(const wasi_component_valtype_t* type) {
    if (!type) return 0u;
    switch (type->opcode) {
        case 0x71u:
            return type->item_count;
        case 0x6Bu:
        case 0x6Au:
            return 2u;
        default:
            return 0u;
    }
}

static int wasi__variant_case_has_payload(const wasi_component_valtype_t* type, uint32_t case_index) {
    if (!type) return 0;
    switch (type->opcode) {
        case 0x71u:
            return case_index < type->item_count && type->data.aggregate.items[case_index].has_type;
        case 0x6Bu:
            return case_index == 1u && type->data.unary.type != NULL;
        case 0x6Au:
            if (case_index == 0u) return type->data.result.ok_type != NULL;
            if (case_index == 1u) return type->data.result.err_type != NULL;
            return 0;
        default:
            return 0;
    }
}

static const wasi_component_valtype_t* wasi__variant_case_payload_type(const wasi_component_valtype_t* type,
                                                                       uint32_t case_index) {
    if (!type) return NULL;
    switch (type->opcode) {
        case 0x71u:
            if (case_index >= type->item_count || !type->data.aggregate.items[case_index].has_type) return NULL;
            return type->data.aggregate.items[case_index].type;
        case 0x6Bu:
            return case_index == 1u ? type->data.unary.type : NULL;
        case 0x6Au:
            if (case_index == 0u) return type->data.result.ok_type;
            if (case_index == 1u) return type->data.result.err_type;
            return NULL;
        default:
            return NULL;
    }
}

static uint32_t wasi__canon_flags_word_count(uint32_t flag_count) {
    if (flag_count <= 32u) return 1u;
    return (flag_count + 31u) / 32u;
}

static void wasi__canon_func_sig_release(wasi__canon_func_sig_t* sig) {
    if (!sig) return;
    WASM_FREE(sig->param_types);
    WASM_FREE(sig->result_types);
    WASM_FREE(sig->post_return_types);
    memset(sig, 0, sizeof(*sig));
}

static wasi_error_t wasi__flat_types_for_valtype(wasi_engine_t* engine,
                                                 const wasi_component_t* component,
                                                 const wasi_component_valtype_t* type,
                                                 wasm_valtype_t* out_types,
                                                 uint32_t* out_count);

static wasm_valtype_t wasi__join_variant_flat_type(wasm_valtype_t lhs, wasm_valtype_t rhs) {
    if (lhs == rhs) return lhs;
    if ((lhs == WASM_TYPE_I32 && rhs == WASM_TYPE_F32) ||
        (lhs == WASM_TYPE_F32 && rhs == WASM_TYPE_I32))
        return WASM_TYPE_I32;
    return WASM_TYPE_I64;
}

static wasi_error_t wasi__variant_case_flat_types(wasi_engine_t* engine,
                                                  const wasi_component_t* component,
                                                  const wasi_component_valtype_t* type,
                                                  uint32_t case_index,
                                                  wasm_valtype_t* out_types,
                                                  uint32_t* out_count) {
    if (out_count) *out_count = 0u;
    if (!type || !out_count)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "missing variant case flat type output");
    if (!wasi__variant_case_has_payload(type, case_index)) return WASI_OK;
    return wasi__flat_types_for_valtype(engine,
                                        component,
                                        wasi__variant_case_payload_type(type, case_index),
                                        out_types,
                                        out_count);
}

static wasi_error_t wasi__coerce_variant_flat_value_for_lower(wasi_engine_t* engine,
                                                              wasm_valtype_t have,
                                                              const wasm_value_t* value,
                                                              wasm_valtype_t want,
                                                              wasm_value_t* out_value) {
    uint32_t bits32;
    uint64_t bits64;

    if (!value || !out_value)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "missing variant lower coercion value");

    if (have == want) {
        *out_value = *value;
        return WASI_OK;
    }

    if (want == WASM_TYPE_I32 && have == WASM_TYPE_F32) {
        memcpy(&bits32, &value->of.f32, sizeof(bits32));
        *out_value = wasm_i32((int32_t)bits32);
        return WASI_OK;
    }

    if (want == WASM_TYPE_I64) {
        if (have == WASM_TYPE_I32) {
            *out_value = wasm_i64((int64_t)(uint32_t)value->of.i32);
            return WASI_OK;
        }
        if (have == WASM_TYPE_F32) {
            memcpy(&bits32, &value->of.f32, sizeof(bits32));
            *out_value = wasm_i64((int64_t)(uint32_t)bits32);
            return WASI_OK;
        }
        if (have == WASM_TYPE_F64) {
            memcpy(&bits64, &value->of.f64, sizeof(bits64));
            *out_value = wasm_i64((int64_t)bits64);
            return WASI_OK;
        }
    }

    return wasi__set_errorf(engine,
                            WASI_ERR_NOT_IMPLEMENTED,
                            "canonical abi cannot lower joined variant payload slot from 0x%02x to 0x%02x",
                            (unsigned)have,
                            (unsigned)want);
}

static wasi_error_t wasi__coerce_variant_flat_value_for_lift(wasi_engine_t* engine,
                                                             wasm_valtype_t have,
                                                             const wasm_value_t* value,
                                                             wasm_valtype_t want,
                                                             wasm_value_t* out_value) {
    uint32_t bits32;
    uint64_t bits64;

    if (!value || !out_value)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "missing variant lift coercion value");

    if (have == want) {
        *out_value = *value;
        return WASI_OK;
    }

    if (have == WASM_TYPE_I32 && want == WASM_TYPE_F32) {
        bits32 = (uint32_t)value->of.i32;
        memcpy(&out_value->of.f32, &bits32, sizeof(bits32));
        out_value->type = WASM_TYPE_F32;
        return WASI_OK;
    }

    if (have == WASM_TYPE_I64) {
        if (want == WASM_TYPE_I32) {
            *out_value = wasm_i32((int32_t)(uint32_t)value->of.i64);
            return WASI_OK;
        }
        if (want == WASM_TYPE_F32) {
            bits32 = (uint32_t)value->of.i64;
            memcpy(&out_value->of.f32, &bits32, sizeof(bits32));
            out_value->type = WASM_TYPE_F32;
            return WASI_OK;
        }
        if (want == WASM_TYPE_F64) {
            bits64 = (uint64_t)value->of.i64;
            memcpy(&out_value->of.f64, &bits64, sizeof(bits64));
            out_value->type = WASM_TYPE_F64;
            return WASI_OK;
        }
    }

    return wasi__set_errorf(engine,
                            WASI_ERR_NOT_IMPLEMENTED,
                            "canonical abi cannot lift joined variant payload slot from 0x%02x to 0x%02x",
                            (unsigned)have,
                            (unsigned)want);
}

static wasi_error_t wasi__merge_variant_flat_types(wasi_engine_t* engine,
                                                   const wasi_component_t* component,
                                                   const wasi_component_valtype_t* type,
                                                   wasm_valtype_t** out_types,
                                                   uint32_t* out_count) {
    uint32_t case_count;
    wasm_valtype_t* merged = NULL;
    uint8_t* seen = NULL;
    uint32_t merged_count = 0;
    uint32_t case_index;
    wasi_error_t err = WASI_OK;

    if (out_types) *out_types = NULL;
    if (out_count) *out_count = 0u;
    if (!out_count) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "missing variant flat type count");

    case_count = wasi__variant_case_count(type);
    for (case_index = 0; case_index < case_count; case_index++) {
        const wasi_component_valtype_t* payload_type;
        wasm_valtype_t* case_types = NULL;
        uint32_t case_flat_count = 0;
        uint32_t slot;

        if (!wasi__variant_case_has_payload(type, case_index)) continue;
        payload_type = wasi__variant_case_payload_type(type, case_index);
        err = wasi__flat_types_for_valtype(engine, component, payload_type, NULL, &case_flat_count);
        if (err != WASI_OK) goto fail;

        if (case_flat_count > merged_count) {
            wasm_valtype_t* grown_types = (wasm_valtype_t*)WASM_REALLOC(merged,
                                                                        (size_t)case_flat_count * sizeof(*grown_types));
            uint8_t* grown_seen;
            if (!grown_types) {
                err = wasi__set_error_literal(engine, WASI_ERR_OOM, "variant flat signature alloc failed");
                goto fail;
            }
            merged = grown_types;

            grown_seen = (uint8_t*)WASM_REALLOC(seen, (size_t)case_flat_count * sizeof(*grown_seen));
            if (!grown_seen) {
                err = wasi__set_error_literal(engine, WASI_ERR_OOM, "variant flat signature alloc failed");
                goto fail;
            }
            memset(grown_seen + merged_count, 0, (size_t)(case_flat_count - merged_count));
            seen = grown_seen;
            merged_count = case_flat_count;
        }

        if (case_flat_count) {
            case_types = (wasm_valtype_t*)WASM_MALLOC((size_t)case_flat_count * sizeof(*case_types));
            if (!case_types) {
                err = wasi__set_error_literal(engine, WASI_ERR_OOM, "variant flat payload alloc failed");
                goto fail;
            }
            err = wasi__flat_types_for_valtype(engine, component, payload_type, case_types, &case_flat_count);
            if (err != WASI_OK) {
                WASM_FREE(case_types);
                goto fail;
            }
        }

        for (slot = 0; slot < case_flat_count; slot++) {
            if (!seen[slot]) {
                merged[slot] = case_types[slot];
                seen[slot] = 1u;
            } else {
                merged[slot] = wasi__join_variant_flat_type(merged[slot], case_types[slot]);
            }
        }

        WASM_FREE(case_types);
    }

    if (out_types) {
        *out_types = merged;
    } else {
        WASM_FREE(merged);
    }
    *out_count = merged_count;
    WASM_FREE(seen);
    return WASI_OK;

fail:
    WASM_FREE(merged);
    WASM_FREE(seen);
    return err;
}

static wasm_value_t wasi__zero_wasm_value(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_I64:
            return wasm_i64(0);
        case WASM_TYPE_F32:
            return wasm_f32(0.0f);
        case WASM_TYPE_F64:
            return wasm_f64(0.0);
        case WASM_TYPE_I32:
        default:
            return wasm_i32(0);
    }
}

static wasi_error_t wasi__flat_types_for_valtype(wasi_engine_t* engine,
                                                 const wasi_component_t* component,
                                                 const wasi_component_valtype_t* type,
                                                 wasm_valtype_t* out_types,
                                                 uint32_t* out_count) {
    const wasi_component_valtype_t* resolved;
    uint32_t i;
    uint32_t cursor = 0;

    if (!type || !out_count) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "missing component value type");

    resolved = wasi__resolve_valtype(component, type);
    if (!resolved) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "missing component value type");

    switch (resolved->opcode) {
        case 0x7Fu:
        case 0x7Eu:
        case 0x7Du:
        case 0x7Cu:
        case 0x7Bu:
        case 0x7Au:
        case 0x79u:
        case 0x74u:
        case 0x6Du:
            if (out_types) out_types[0] = WASM_TYPE_I32;
            *out_count = 1u;
            return WASI_OK;
        case 0x78u:
        case 0x77u:
            if (out_types) out_types[0] = WASM_TYPE_I64;
            *out_count = 1u;
            return WASI_OK;
        case 0x76u:
            if (out_types) out_types[0] = WASM_TYPE_F32;
            *out_count = 1u;
            return WASI_OK;
        case 0x75u:
            if (out_types) out_types[0] = WASM_TYPE_F64;
            *out_count = 1u;
            return WASI_OK;
        case 0x73u:
        case 0x70u:
            if (out_types) {
                out_types[0] = WASM_TYPE_I32;
                out_types[1] = WASM_TYPE_I32;
            }
            *out_count = 2u;
            return WASI_OK;
        case 0x67u:
            for (i = 0; i < resolved->data.fixed_list.length; i++) {
                uint32_t added = 0;
                wasi_error_t err = wasi__flat_types_for_valtype(engine,
                                                                component,
                                                                resolved->data.fixed_list.element_type,
                                                                out_types ? out_types + cursor : NULL,
                                                                &added);
                if (err != WASI_OK) return err;
                cursor += added;
            }
            *out_count = cursor;
            return WASI_OK;
        case 0x72u:
        case 0x6Fu:
            for (i = 0; i < resolved->item_count; i++) {
                uint32_t added = 0;
                wasi_error_t err = wasi__flat_types_for_valtype(engine,
                                                                component,
                                                                resolved->data.aggregate.items[i].type,
                                                                out_types ? out_types + cursor : NULL,
                                                                &added);
                if (err != WASI_OK) return err;
                cursor += added;
            }
            *out_count = cursor;
            return WASI_OK;
        case 0x6Eu: {
            uint32_t words = wasi__canon_flags_word_count(resolved->item_count);
            for (i = 0; i < words; i++) {
                if (out_types) out_types[cursor] = WASM_TYPE_I32;
                cursor++;
            }
            *out_count = cursor;
            return WASI_OK;
        }
        case 0x71u:
        case 0x6Bu:
        case 0x6Au: {
            wasm_valtype_t* payload_types = NULL;
            uint32_t payload_count = 0;
            wasi_error_t err = wasi__merge_variant_flat_types(engine,
                                                              component,
                                                              resolved,
                                                              &payload_types,
                                                              &payload_count);
            if (err != WASI_OK) return err;
            if (out_types) {
                out_types[0] = WASM_TYPE_I32;
                if (payload_count) {
                    memcpy(out_types + 1u,
                           payload_types,
                           (size_t)payload_count * sizeof(*payload_types));
                }
            }
            WASM_FREE(payload_types);
            *out_count = 1u + payload_count;
            return WASI_OK;
        }
        default:
            return wasi__set_errorf(engine,
                                    WASI_ERR_NOT_IMPLEMENTED,
                                    "canonical abi does not support %s yet",
                                    wasi__component_type_detail_string(resolved->opcode));
    }
}

static wasi_error_t wasi__build_func_signature(wasi_engine_t* engine,
                                               const wasi_component_t* component,
                                               uint32_t type_index,
                                               wasi__canon_result_abi_t result_abi,
                                               wasi__canon_func_sig_t* out_sig) {
    uint32_t param_count;
    uint32_t result_count;
    uint32_t flat_params = 0;
    uint32_t flat_results = 0;
    uint32_t i;

    if (!out_sig) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "missing function signature output");
    memset(out_sig, 0, sizeof(*out_sig));

    if (!wasi__component_type_is_func(component, type_index))
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component type index is not a function type");

    param_count = wasi_component_func_type_param_count(component, type_index);
    result_count = wasi_component_func_type_result_count(component, type_index);

    for (i = 0; i < param_count; i++) {
        uint32_t added = 0;
        wasi_error_t err = wasi__flat_types_for_valtype(engine,
                                                        component,
                                                        wasi_component_func_type_param_type(component, type_index, i),
                                                        NULL,
                                                        &added);
        if (err != WASI_OK) return err;
        flat_params += added;
    }
    for (i = 0; i < result_count; i++) {
        const wasi_component_valtype_t* result_type = wasi_component_func_type_result_type(component, type_index, i);
        uint32_t added = 0;
        wasi_error_t err = wasi__flat_types_for_valtype(engine, component, result_type, NULL, &added);
        if (err != WASI_OK) return err;
        flat_results += added;
    }

    out_sig->params_spilled = flat_params > WASI__CANON_MAX_FLAT_PARAMS;
    out_sig->results_spilled = flat_results > WASI__CANON_MAX_FLAT_RESULTS &&
                               result_abi != WASI__CANON_RESULT_ABI_FLAT;
    out_sig->results_retptr_returned = out_sig->results_spilled &&
                                       result_abi == WASI__CANON_RESULT_ABI_SPILL_RETURN;

    out_sig->num_param_types = out_sig->params_spilled ? 1u : flat_params;
    if (out_sig->results_spilled && !out_sig->results_retptr_returned) out_sig->num_param_types += 1u;
    out_sig->num_result_types = out_sig->results_spilled ? (out_sig->results_retptr_returned ? 1u : 0u) : flat_results;
    out_sig->num_post_return_types = out_sig->results_spilled ? 1u : flat_results;

    if (out_sig->num_param_types) {
        out_sig->param_types = (wasm_valtype_t*)WASM_MALLOC((size_t)out_sig->num_param_types * sizeof(*out_sig->param_types));
        if (!out_sig->param_types) return wasi__set_error_literal(engine, WASI_ERR_OOM, "flat param signature alloc failed");
    }
    if (out_sig->num_result_types) {
        out_sig->result_types = (wasm_valtype_t*)WASM_MALLOC((size_t)out_sig->num_result_types * sizeof(*out_sig->result_types));
        if (!out_sig->result_types) {
            wasi__canon_func_sig_release(out_sig);
            return wasi__set_error_literal(engine, WASI_ERR_OOM, "flat result signature alloc failed");
        }
    }
    if (out_sig->num_post_return_types) {
        out_sig->post_return_types = (wasm_valtype_t*)WASM_MALLOC((size_t)out_sig->num_post_return_types * sizeof(*out_sig->post_return_types));
        if (!out_sig->post_return_types) {
            wasi__canon_func_sig_release(out_sig);
            return wasi__set_error_literal(engine, WASI_ERR_OOM, "post-return signature alloc failed");
        }
    }

    if (out_sig->params_spilled) {
        out_sig->param_types[0] = WASM_TYPE_I32;
    } else {
        uint32_t cursor = 0;
        for (i = 0; i < param_count; i++) {
            uint32_t added = 0;
            wasi_error_t err = wasi__flat_types_for_valtype(engine,
                                                            component,
                                                            wasi_component_func_type_param_type(component, type_index, i),
                                                            out_sig->param_types + cursor,
                                                            &added);
            if (err != WASI_OK) {
                wasi__canon_func_sig_release(out_sig);
                return err;
            }
            cursor += added;
        }
    }

    if (out_sig->results_spilled) {
        if (out_sig->results_retptr_returned) {
            out_sig->result_types[0] = WASM_TYPE_I32;
        } else {
            out_sig->param_types[out_sig->num_param_types - 1u] = WASM_TYPE_I32;
        }
        out_sig->post_return_types[0] = WASM_TYPE_I32;
    } else {
        uint32_t cursor = 0;
        for (i = 0; i < result_count; i++) {
            uint32_t added = 0;
            wasi_error_t err = wasi__flat_types_for_valtype(engine,
                                                            component,
                                                            wasi_component_func_type_result_type(component, type_index, i),
                                                            out_sig->result_types + cursor,
                                                            &added);
            if (err != WASI_OK) {
                wasi__canon_func_sig_release(out_sig);
                return err;
            }
            cursor += added;
        }
        if (cursor) memcpy(out_sig->post_return_types, out_sig->result_types, (size_t)cursor * sizeof(*out_sig->post_return_types));
    }

    return WASI_OK;
}

static int wasi__func_signature_matches(const wasi__canon_func_sig_t* sig,
                                        wasm_module_t* core_module,
                                        uint32_t func_index) {
    uint32_t i;

    if (!sig || !core_module) return 0;
    if (wasm_func_param_count(core_module, func_index) != sig->num_param_types ||
        wasm_func_result_count(core_module, func_index) != sig->num_result_types)
        return 0;

    for (i = 0; i < sig->num_param_types; i++) {
        if (wasm_func_param_type(core_module, func_index, i) != sig->param_types[i]) return 0;
    }
    for (i = 0; i < sig->num_result_types; i++) {
        if (wasm_func_result_type(core_module, func_index, i) != sig->result_types[i]) return 0;
    }
    return 1;
}

static wasi_error_t wasi__validate_func_signature(wasi_engine_t* engine,
                                                  const wasi_component_t* component,
                                                  uint32_t type_index,
                                                  wasm_module_t* core_module,
                                                  uint32_t func_index,
                                                  const char* func_label,
                                                  wasi__canon_func_sig_t* out_sig) {
    uint32_t i;
    wasi_error_t err;

    if (!engine || !component || !core_module || !func_label || !out_sig)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid canonical call signature inputs");

    err = wasi__build_func_signature(engine, component, type_index, WASI__CANON_RESULT_ABI_FLAT, out_sig);
    if (err != WASI_OK) return err;

    if (func_index >= wasm_func_count(core_module)) {
        wasi__canon_func_sig_release(out_sig);
        return wasi__set_errorf(engine, WASI_ERR_UNDEFINED_EXPORT, "missing core function %s", func_label);
    }

    if (!wasi__func_signature_matches(out_sig, core_module, func_index) &&
        wasi_component_func_type_result_count(component, type_index) > 0u) {
        wasi__canon_func_sig_release(out_sig);
        err = wasi__build_func_signature(engine,
                                         component,
                                         type_index,
                                         WASI__CANON_RESULT_ABI_SPILL_PARAM,
                                         out_sig);
        if (err != WASI_OK) return err;
    }

    if (!wasi__func_signature_matches(out_sig, core_module, func_index) && out_sig->results_spilled) {
        wasi__canon_func_sig_release(out_sig);
        err = wasi__build_func_signature(engine,
                                         component,
                                         type_index,
                                         WASI__CANON_RESULT_ABI_SPILL_RETURN,
                                         out_sig);
        if (err != WASI_OK) return err;
    }

    if (!wasi__func_signature_matches(out_sig, core_module, func_index)) {
        for (i = 0; i < out_sig->num_param_types; i++) {
            if (i >= wasm_func_param_count(core_module, func_index) ||
                wasm_func_param_type(core_module, func_index, i) != out_sig->param_types[i]) {
                wasi__canon_func_sig_release(out_sig);
                return wasi__set_errorf(engine,
                                        WASI_ERR_TYPE_MISMATCH,
                                        "core function %s %s %u type mismatch",
                                        func_label,
                                        i < wasm_func_param_count(core_module, func_index) ? "param" : "flat arity",
                                        (unsigned)i);
            }
        }
        for (i = 0; i < out_sig->num_result_types; i++) {
            if (i >= wasm_func_result_count(core_module, func_index) ||
                wasm_func_result_type(core_module, func_index, i) != out_sig->result_types[i]) {
                wasi__canon_func_sig_release(out_sig);
                return wasi__set_errorf(engine,
                                        WASI_ERR_TYPE_MISMATCH,
                                        "core function %s %s %u type mismatch",
                                        func_label,
                                        i < wasm_func_result_count(core_module, func_index) ? "result" : "flat arity",
                                        (unsigned)i);
            }
        }
        wasi__canon_func_sig_release(out_sig);
        return wasi__set_errorf(engine,
                                WASI_ERR_TYPE_MISMATCH,
                                "core function %s flat arity mismatch",
                                func_label);
    }

    return WASI_OK;
}

static wasi_error_t wasi__effective_canon_options(wasi__canon_runtime_options_t* out,
                                                  const wasi_canon_options_t* options) {
    wasi_error_t err;

    if (!out) return WASI_ERR_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    err = wasi_canon_options_default(&out->api);
    if (err != WASI_OK) return err;
    if (!options) return WASI_OK;

    out->api.memory_index = options->memory_index;
    out->api.post_return_name = options->post_return_name;
    out->api.string_encoding = options->string_encoding;
    if (options->cabi_realloc_name) out->api.cabi_realloc_name = options->cabi_realloc_name;
    return WASI_OK;
}

static wasi_error_t wasi__resolve_lift_runtime_options(wasi_engine_t* engine,
                                                       const wasi_component_canon_t* canon,
                                                       wasi__canon_runtime_options_t* out) {
    uint32_t i;
    wasi_error_t err;

    if (!engine || !canon || !out)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid canon option resolution inputs");

    err = wasi__effective_canon_options(out, NULL);
    if (err != WASI_OK) return err;

    for (i = 0; i < canon->num_options; i++) {
        const wasi_component_canon_option_t* option = &canon->options[i];
        switch (option->code) {
            case 0x00u:
                out->api.string_encoding = WASI_STRING_ENCODING_UTF8;
                break;
            case 0x01u:
                out->api.string_encoding = WASI_STRING_ENCODING_UTF16;
                break;
            case 0x02u:
                out->api.string_encoding = WASI_STRING_ENCODING_LATIN1_UTF16;
                break;
            case 0x03u:
                if (!option->has_index)
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "canonical memory option missing index");
                out->api.memory_index = option->index;
                break;
            case 0x04u:
                if (!option->has_index)
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "canonical realloc option missing index");
                out->has_realloc_func_index = 1;
                out->realloc_func_index = option->index;
                break;
            case 0x05u:
                if (!option->has_index)
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "canonical post-return option missing index");
                out->has_post_return_func_index = 1;
                out->post_return_func_index = option->index;
                break;
            case 0x06u:
                return wasi__set_error_literal(engine, WASI_ERR_NOT_IMPLEMENTED, "async canonical lifts are not implemented");
            case 0x07u:
                return wasi__set_error_literal(engine, WASI_ERR_NOT_IMPLEMENTED, "callback canonical lifts are not implemented");
            default:
                return wasi__set_errorf(engine,
                                        WASI_ERR_NOT_IMPLEMENTED,
                                        "canonical option 0x%02x not implemented",
                                        (unsigned)option->code);
        }
    }

    return WASI_OK;
}

static wasi_error_t wasi__encode_guest_string(wasi_engine_t* engine,
                                              wasi_string_encoding_t encoding,
                                              const wasi_string_t* string,
                                              uint8_t** out_bytes,
                                              uint32_t* out_len_units,
                                              uint32_t* out_size_bytes,
                                              uint32_t* out_align) {
    uint8_t* guest_bytes = NULL;
    size_t guest_size = 0;

    if (out_bytes) *out_bytes = NULL;
    if (out_len_units) *out_len_units = 0;
    if (out_size_bytes) *out_size_bytes = 0;
    if (out_align) *out_align = 1;
    if (!out_bytes || !out_len_units || !out_size_bytes || !out_align || !string)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid string lowering inputs");

    switch (encoding) {
        case WASI_STRING_ENCODING_UTF8:
            if (!wasi__utf8_validate(string->data, string->len))
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "invalid utf8 string");
            if (string->len > UINT32_MAX)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "string too large for canonical abi");
            if (string->len) {
                guest_bytes = (uint8_t*)WASM_MALLOC(string->len);
                if (!guest_bytes) return wasi__set_error_literal(engine, WASI_ERR_OOM, "utf8 string alloc failed");
                memcpy(guest_bytes, string->data, string->len);
            }
            *out_bytes = guest_bytes;
            *out_len_units = (uint32_t)string->len;
            *out_size_bytes = (uint32_t)string->len;
            *out_align = 1u;
            return WASI_OK;
        case WASI_STRING_ENCODING_UTF16: {
            wasi_error_t err = wasi__utf8_to_utf16le(engine, string->data, string->len, &guest_bytes, &guest_size);
            if (err != WASI_OK) return err;
            if ((guest_size / 2u) > UINT32_MAX) {
                WASM_FREE(guest_bytes);
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "utf16 string too large for canonical abi");
            }
            *out_bytes = guest_bytes;
            *out_len_units = (uint32_t)(guest_size / 2u);
            *out_size_bytes = (uint32_t)guest_size;
            *out_align = 2u;
            return WASI_OK;
        }
        case WASI_STRING_ENCODING_LATIN1_UTF16: {
            size_t cursor = 0;
            size_t written = 0;
            uint32_t codepoint;

            if (string->len > WASI__CANON_STRING_MAX_CODE_UNITS)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "string too large for canonical abi");

            guest_bytes = string->len ? (uint8_t*)WASM_MALLOC(string->len) : NULL;
            if (string->len && !guest_bytes)
                return wasi__set_error_literal(engine, WASI_ERR_OOM, "latin1 string alloc failed");

            while (cursor < string->len) {
                if (!wasi__utf8_decode_one((const uint8_t*)string->data, string->len, &cursor, &codepoint)) {
                    WASM_FREE(guest_bytes);
                    return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "invalid utf8 string");
                }
                if (codepoint >= 0x100u) {
                    wasi_error_t err;
                    WASM_FREE(guest_bytes);
                    err = wasi__utf8_to_utf16le(engine, string->data, string->len, &guest_bytes, &guest_size);
                    if (err != WASI_OK) return err;
                    if ((guest_size / 2u) > WASI__CANON_STRING_MAX_CODE_UNITS) {
                        WASM_FREE(guest_bytes);
                        return wasi__set_error_literal(engine,
                                                       WASI_ERR_INVALID_ARGUMENT,
                                                       "utf16 string too large for canonical abi");
                    }
                    *out_bytes = guest_bytes;
                    *out_len_units = ((uint32_t)(guest_size / 2u)) | WASI__CANON_STRING_UTF16_TAG;
                    *out_size_bytes = (uint32_t)guest_size;
                    *out_align = 2u;
                    return WASI_OK;
                }
                guest_bytes[written++] = (uint8_t)codepoint;
            }

            *out_bytes = guest_bytes;
            *out_len_units = (uint32_t)written;
            *out_size_bytes = (uint32_t)written;
            *out_align = 2u;
            return WASI_OK;
        }
        default:
            return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "unknown string encoding");
    }
}

static wasi_error_t wasi__decode_guest_string(wasi_engine_t* engine,
                                              const wasi__canon_runtime_options_t* options,
                                              wasm_module_t* core_module,
                                              uint32_t ptr,
                                              uint32_t len_units,
                                              wasi_value_t* out_value) {
    size_t size_bytes = 0;
    uint8_t* bytes = NULL;
    uint32_t align = 1u;
    wasi_error_t err;

    if (!engine || !options || !core_module || !out_value)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid string lifting inputs");

    switch (options->api.string_encoding) {
        case WASI_STRING_ENCODING_UTF8:
            size_bytes = (size_t)len_units;
            align = 1u;
            break;
        case WASI_STRING_ENCODING_UTF16:
#if SIZE_MAX <= UINT32_MAX
            if (len_units > (uint32_t)(SIZE_MAX / 2u))
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "utf16 guest string too large");
#endif
            size_bytes = (size_t)len_units * 2u;
            align = 2u;
            break;
        case WASI_STRING_ENCODING_LATIN1_UTF16:
            align = 2u;
            if (len_units & WASI__CANON_STRING_UTF16_TAG) {
                uint32_t utf16_units = len_units & ~WASI__CANON_STRING_UTF16_TAG;
#if SIZE_MAX <= UINT32_MAX
                if (utf16_units > (uint32_t)(SIZE_MAX / 2u))
                    return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "utf16 guest string too large");
#endif
                size_bytes = (size_t)utf16_units * 2u;
            } else {
                size_bytes = (size_t)len_units;
            }
            break;
        default:
            return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "unknown string encoding");
    }

    if (size_bytes && (ptr & (align - 1u)) != 0u)
        return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "guest string pointer is misaligned");

    if (!size_bytes) {
        wasi_value_init(out_value);
        out_value->kind = WASI_VALUE_KIND_STRING;
        out_value->of.string.data = NULL;
        out_value->of.string.len = 0;
        out_value->of.string.owned = 0;
        return WASI_OK;
    }

    bytes = (uint8_t*)WASM_MALLOC(size_bytes);
    if (!bytes) return wasi__set_error_literal(engine, WASI_ERR_OOM, "guest string buffer alloc failed");

    if (wasm_memory_read(core_module, options->api.memory_index, ptr, bytes, size_bytes) != WASM_OK) {
        WASM_FREE(bytes);
        return wasi__set_runtime_error_from_module(engine, core_module, core_module->rt->last_error, "string lift memory read failed");
    }

    if (options->api.string_encoding == WASI_STRING_ENCODING_UTF8) {
        if (!wasi__utf8_validate((const char*)bytes, size_bytes)) {
            WASM_FREE(bytes);
            return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "guest string is not valid utf8");
        }
        err = wasi_value_set_string_copy(out_value, (const char*)bytes, size_bytes);
        WASM_FREE(bytes);
        return err == WASI_OK ? WASI_OK : wasi__set_error_literal(engine, err, err == WASI_ERR_OOM ? "string copy alloc failed" : "string copy failed");
    }

    if (options->api.string_encoding == WASI_STRING_ENCODING_UTF16) {
        char* utf8 = NULL;
        size_t utf8_len = 0;
        err = wasi__utf16le_to_utf8(engine, bytes, len_units, &utf8, &utf8_len);
        WASM_FREE(bytes);
        if (err != WASI_OK) return err;
        err = wasi_value_set_string_copy(out_value, utf8, utf8_len);
        WASM_FREE(utf8);
        return err == WASI_OK ? WASI_OK : wasi__set_error_literal(engine, err, err == WASI_ERR_OOM ? "string copy alloc failed" : "string copy failed");
    }

    if (options->api.string_encoding == WASI_STRING_ENCODING_LATIN1_UTF16) {
        char* utf8 = NULL;
        size_t utf8_len = 0;

        if (len_units & WASI__CANON_STRING_UTF16_TAG) {
            uint32_t utf16_units = len_units & ~WASI__CANON_STRING_UTF16_TAG;
            err = wasi__utf16le_to_utf8(engine, bytes, utf16_units, &utf8, &utf8_len);
        } else {
            err = wasi__latin1_to_utf8(engine, bytes, size_bytes, &utf8, &utf8_len);
        }
        WASM_FREE(bytes);
        if (err != WASI_OK) return err;
        err = wasi_value_set_string_copy(out_value, utf8, utf8_len);
        WASM_FREE(utf8);
        return err == WASI_OK ? WASI_OK : wasi__set_error_literal(engine, err, err == WASI_ERR_OOM ? "string copy alloc failed" : "string copy failed");
    }

    WASM_FREE(bytes);
    return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "unknown string encoding");
}

static wasi_error_t wasi__store_value_to_memory(wasi_engine_t* engine,
                                                const wasi_component_t* component,
                                                const wasi_component_valtype_t* type,
                                                const wasi_value_t* value,
                                                wasm_module_t* core_module,
                                                const wasi__canon_runtime_options_t* options,
                                                uint32_t guest_ptr);

static wasi_error_t wasi__load_value_from_memory(wasi_engine_t* engine,
                                                 const wasi_component_t* component,
                                                 const wasi_component_valtype_t* type,
                                                 wasm_module_t* core_module,
                                                 const wasi__canon_runtime_options_t* options,
                                                 uint32_t guest_ptr,
                                                 wasi_value_t* out_value);

static wasi_error_t wasi__canon_align_to(wasi_engine_t* engine, uint32_t value, uint32_t align, uint32_t* out) {
    uint32_t aligned;

    if (!out || align == 0u) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid canonical alignment");
    aligned = (value + (align - 1u)) & ~(align - 1u);
    if (aligned < value) return wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical layout overflow");
    *out = aligned;
    return WASI_OK;
}

static wasi_error_t wasi__guest_add_offset(wasi_engine_t* engine, uint32_t base, uint32_t offset, uint32_t* out) {
    uint64_t sum;

    if (!out) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "missing guest address output");
    sum = (uint64_t)base + (uint64_t)offset;
    if (sum > UINT32_MAX) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "guest address overflow");
    *out = (uint32_t)sum;
    return WASI_OK;
}

static wasi_error_t wasi__memory_write_bytes(wasi_engine_t* engine,
                                             wasm_module_t* core_module,
                                             uint32_t memory_index,
                                             uint32_t guest_ptr,
                                             const void* bytes,
                                             size_t len,
                                             const char* action) {
    wasm_error_t wasm_err = wasm_memory_write(core_module, memory_index, guest_ptr, bytes, len);
    if (wasm_err != WASM_OK) return wasi__set_runtime_error_from_module(engine, core_module, wasm_err, action);
    return WASI_OK;
}

static wasi_error_t wasi__memory_read_bytes(wasi_engine_t* engine,
                                            wasm_module_t* core_module,
                                            uint32_t memory_index,
                                            uint32_t guest_ptr,
                                            void* bytes,
                                            size_t len,
                                            const char* action) {
    wasm_error_t wasm_err = wasm_memory_read(core_module, memory_index, guest_ptr, bytes, len);
    if (wasm_err != WASM_OK) return wasi__set_runtime_error_from_module(engine, core_module, wasm_err, action);
    return WASI_OK;
}

static wasi_error_t wasi__memory_write_u8(wasi_engine_t* engine,
                                          wasm_module_t* core_module,
                                          uint32_t memory_index,
                                          uint32_t guest_ptr,
                                          uint8_t value,
                                          const char* action) {
    return wasi__memory_write_bytes(engine, core_module, memory_index, guest_ptr, &value, 1u, action);
}

static wasi_error_t wasi__memory_write_u16(wasi_engine_t* engine,
                                           wasm_module_t* core_module,
                                           uint32_t memory_index,
                                           uint32_t guest_ptr,
                                           uint16_t value,
                                           const char* action) {
    uint8_t bytes[2];
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    return wasi__memory_write_bytes(engine, core_module, memory_index, guest_ptr, bytes, sizeof(bytes), action);
}

static wasi_error_t wasi__memory_write_u32(wasi_engine_t* engine,
                                           wasm_module_t* core_module,
                                           uint32_t memory_index,
                                           uint32_t guest_ptr,
                                           uint32_t value,
                                           const char* action) {
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFu);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFu);
    return wasi__memory_write_bytes(engine, core_module, memory_index, guest_ptr, bytes, sizeof(bytes), action);
}

static wasi_error_t wasi__memory_write_u64(wasi_engine_t* engine,
                                           wasm_module_t* core_module,
                                           uint32_t memory_index,
                                           uint32_t guest_ptr,
                                           uint64_t value,
                                           const char* action) {
    uint8_t bytes[8];
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFu);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFu);
    bytes[4] = (uint8_t)((value >> 32) & 0xFFu);
    bytes[5] = (uint8_t)((value >> 40) & 0xFFu);
    bytes[6] = (uint8_t)((value >> 48) & 0xFFu);
    bytes[7] = (uint8_t)((value >> 56) & 0xFFu);
    return wasi__memory_write_bytes(engine, core_module, memory_index, guest_ptr, bytes, sizeof(bytes), action);
}

static wasi_error_t wasi__memory_read_u8(wasi_engine_t* engine,
                                         wasm_module_t* core_module,
                                         uint32_t memory_index,
                                         uint32_t guest_ptr,
                                         uint8_t* out,
                                         const char* action) {
    return wasi__memory_read_bytes(engine, core_module, memory_index, guest_ptr, out, 1u, action);
}

static wasi_error_t wasi__memory_read_u16(wasi_engine_t* engine,
                                          wasm_module_t* core_module,
                                          uint32_t memory_index,
                                          uint32_t guest_ptr,
                                          uint16_t* out,
                                          const char* action) {
    uint8_t bytes[2];
    wasi_error_t err = wasi__memory_read_bytes(engine, core_module, memory_index, guest_ptr, bytes, sizeof(bytes), action);
    if (err != WASI_OK) return err;
    *out = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    return WASI_OK;
}

static wasi_error_t wasi__memory_read_u32(wasi_engine_t* engine,
                                          wasm_module_t* core_module,
                                          uint32_t memory_index,
                                          uint32_t guest_ptr,
                                          uint32_t* out,
                                          const char* action) {
    uint8_t bytes[4];
    wasi_error_t err = wasi__memory_read_bytes(engine, core_module, memory_index, guest_ptr, bytes, sizeof(bytes), action);
    if (err != WASI_OK) return err;
    *out = (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
    return WASI_OK;
}

static wasi_error_t wasi__memory_read_u64(wasi_engine_t* engine,
                                          wasm_module_t* core_module,
                                          uint32_t memory_index,
                                          uint32_t guest_ptr,
                                          uint64_t* out,
                                          const char* action) {
    uint8_t bytes[8];
    wasi_error_t err = wasi__memory_read_bytes(engine, core_module, memory_index, guest_ptr, bytes, sizeof(bytes), action);
    if (err != WASI_OK) return err;
    *out = (uint64_t)bytes[0] |
           ((uint64_t)bytes[1] << 8) |
           ((uint64_t)bytes[2] << 16) |
           ((uint64_t)bytes[3] << 24) |
           ((uint64_t)bytes[4] << 32) |
           ((uint64_t)bytes[5] << 40) |
           ((uint64_t)bytes[6] << 48) |
           ((uint64_t)bytes[7] << 56);
    return WASI_OK;
}

static wasi_error_t wasi__canon_alloc_guest(wasi_engine_t* engine,
                                            wasm_module_t* core_module,
                                            const wasi__canon_runtime_options_t* options,
                                            uint32_t align,
                                            uint32_t size,
                                            uint32_t* out_ptr) {
    wasm_value_t realloc_args[4];
    wasm_value_t realloc_result;
    wasm_error_t wasm_err;

    if (out_ptr) *out_ptr = 0u;
    if (!engine || !core_module || !options || !out_ptr)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid guest allocation inputs");
    if (size == 0u) {
        *out_ptr = 0u;
        return WASI_OK;
    }
    if (!options->has_realloc_func_index && (!options->api.cabi_realloc_name || !options->api.cabi_realloc_name[0]))
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "canonical allocation requires cabi_realloc");

    realloc_args[0] = wasm_i32(0);
    realloc_args[1] = wasm_i32(0);
    realloc_args[2] = wasm_i32((int32_t)align);
    realloc_args[3] = wasm_i32((int32_t)size);
    if (options->has_realloc_func_index) {
        wasm_err = wasm_call_index(core_module, options->realloc_func_index, realloc_args, 4u, &realloc_result, 1u);
    } else {
        wasm_err = wasm_call(core_module, options->api.cabi_realloc_name, realloc_args, 4u, &realloc_result, 1u);
    }
    if (wasm_err != WASM_OK) return wasi__set_runtime_error_from_module(engine, core_module, wasm_err, "cabi_realloc failed");
    if ((((uint32_t)realloc_result.of.i32) & (align - 1u)) != 0u)
        return wasi__set_error_literal(engine, WASI_ERR_RUNTIME, "cabi_realloc returned misaligned pointer");
    *out_ptr = (uint32_t)realloc_result.of.i32;
    return WASI_OK;
}

static wasi_error_t wasi__layout_aggregate(wasi_engine_t* engine,
                                           const wasi_component_t* component,
                                           const wasi_component_valtype_t* type,
                                           uint32_t* offsets,
                                           uint32_t* out_size,
                                           uint32_t* out_align);

static wasi_error_t wasi__layout_valtype(wasi_engine_t* engine,
                                         const wasi_component_t* component,
                                         const wasi_component_valtype_t* type,
                                         uint32_t* out_size,
                                         uint32_t* out_align);

static wasi_error_t wasi__layout_variant(wasi_engine_t* engine,
                                         const wasi_component_t* component,
                                         const wasi_component_valtype_t* type,
                                         uint32_t* out_payload_offset,
                                         uint32_t* out_size,
                                         uint32_t* out_align) {
    uint32_t case_count;
    uint32_t payload_offset;
    uint32_t max_payload_size = 0u;
    uint32_t max_align = 4u;
    uint32_t case_index;
    wasi_error_t err;

    if (out_payload_offset) *out_payload_offset = 0u;
    if (!type || !out_size || !out_align)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid variant layout inputs");

    case_count = wasi__variant_case_count(type);
    for (case_index = 0; case_index < case_count; case_index++) {
        const wasi_component_valtype_t* payload_type;
        uint32_t case_size;
        uint32_t case_align;

        if (!wasi__variant_case_has_payload(type, case_index)) continue;
        payload_type = wasi__variant_case_payload_type(type, case_index);
        err = wasi__layout_valtype(engine, component, payload_type, &case_size, &case_align);
        if (err != WASI_OK) return err;
        if (case_align > max_align) max_align = case_align;
        if (case_size > max_payload_size) max_payload_size = case_size;
    }

    err = wasi__canon_align_to(engine, 4u, max_align, &payload_offset);
    if (err != WASI_OK) return err;
    if (max_payload_size > UINT32_MAX - payload_offset)
        return wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical variant layout overflow");

    *out_align = max_align;
    if (out_payload_offset) *out_payload_offset = payload_offset;
    return wasi__canon_align_to(engine, payload_offset + max_payload_size, max_align, out_size);
}

static wasi_error_t wasi__layout_valtype(wasi_engine_t* engine,
                                         const wasi_component_t* component,
                                         const wasi_component_valtype_t* type,
                                         uint32_t* out_size,
                                         uint32_t* out_align) {
    const wasi_component_valtype_t* resolved = wasi__resolve_valtype(component, type);

    if (!resolved || !out_size || !out_align)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid canonical layout inputs");

    switch (resolved->opcode) {
        case 0x7Fu:
        case 0x7Eu:
        case 0x7Du:
            *out_size = 1u;
            *out_align = 1u;
            return WASI_OK;
        case 0x7Cu:
        case 0x7Bu:
            *out_size = 2u;
            *out_align = 2u;
            return WASI_OK;
        case 0x7Au:
        case 0x79u:
        case 0x76u:
        case 0x74u:
        case 0x6Du:
            *out_size = 4u;
            *out_align = 4u;
            return WASI_OK;
        case 0x78u:
        case 0x77u:
        case 0x75u:
            *out_size = 8u;
            *out_align = 8u;
            return WASI_OK;
        case 0x73u:
        case 0x70u:
            *out_size = 8u;
            *out_align = 4u;
            return WASI_OK;
        case 0x72u:
        case 0x6Fu:
            return wasi__layout_aggregate(engine, component, resolved, NULL, out_size, out_align);
        case 0x71u:
        case 0x6Bu:
        case 0x6Au:
            return wasi__layout_variant(engine, component, resolved, NULL, out_size, out_align);
        case 0x6Eu: {
            uint32_t words = wasi__canon_flags_word_count(resolved->item_count);
            if (resolved->item_count <= 8u) {
                *out_size = 1u;
                *out_align = 1u;
            } else if (resolved->item_count <= 16u) {
                *out_size = 2u;
                *out_align = 2u;
            } else {
                *out_size = words * 4u;
                *out_align = 4u;
            }
            return WASI_OK;
        }
        case 0x67u: {
            uint32_t elem_size;
            uint32_t elem_align;
            wasi_error_t err = wasi__layout_valtype(engine, component, resolved->data.fixed_list.element_type, &elem_size, &elem_align);
            if (err != WASI_OK) return err;
            if (resolved->data.fixed_list.length && elem_size > UINT32_MAX / resolved->data.fixed_list.length)
                return wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical fixed-list layout overflow");
            *out_size = elem_size * resolved->data.fixed_list.length;
            *out_align = elem_align;
            return WASI_OK;
        }
        default:
            return wasi__set_errorf(engine,
                                    WASI_ERR_NOT_IMPLEMENTED,
                                    "canonical abi layout does not support %s yet",
                                    wasi__component_type_detail_string(resolved->opcode));
    }
}

static wasi_error_t wasi__layout_aggregate(wasi_engine_t* engine,
                                           const wasi_component_t* component,
                                           const wasi_component_valtype_t* type,
                                           uint32_t* offsets,
                                           uint32_t* out_size,
                                           uint32_t* out_align) {
    uint32_t offset = 0;
    uint32_t max_align = 1u;
    uint32_t i;

    if (!type || !out_size || !out_align)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid aggregate layout inputs");

    for (i = 0; i < type->item_count; i++) {
        uint32_t item_size;
        uint32_t item_align;
        wasi_error_t err = wasi__layout_valtype(engine, component, type->data.aggregate.items[i].type, &item_size, &item_align);
        if (err != WASI_OK) return err;
        err = wasi__canon_align_to(engine, offset, item_align, &offset);
        if (err != WASI_OK) return err;
        if (offsets) offsets[i] = offset;
        if (item_align > max_align) max_align = item_align;
        if (item_size > UINT32_MAX - offset) return wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical aggregate layout overflow");
        offset += item_size;
    }

    *out_align = max_align;
    return wasi__canon_align_to(engine, offset, max_align, out_size);
}

static wasi_error_t wasi__lower_string_pair(wasi_engine_t* engine,
                                            const wasi_string_t* string,
                                            wasm_module_t* core_module,
                                            const wasi__canon_runtime_options_t* options,
                                            uint32_t* out_ptr,
                                            uint32_t* out_len_units) {
    uint8_t* guest_bytes = NULL;
    uint32_t guest_ptr = 0;
    uint32_t guest_size_bytes = 0;
    uint32_t align = 1u;
    wasi_error_t err;

    if (out_ptr) *out_ptr = 0u;
    if (out_len_units) *out_len_units = 0u;
    if (!engine || !string || !core_module || !options || !out_ptr || !out_len_units)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid string lowering inputs");
    if (!string->data && string->len)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "string bytes must not be NULL");

    err = wasi__encode_guest_string(engine,
                                    options->api.string_encoding,
                                    string,
                                    &guest_bytes,
                                    out_len_units,
                                    &guest_size_bytes,
                                    &align);
    if (err != WASI_OK) return err;

    err = wasi__canon_alloc_guest(engine, core_module, options, align, guest_size_bytes, &guest_ptr);
    if (err != WASI_OK) {
        WASM_FREE(guest_bytes);
        return err;
    }
    if (guest_size_bytes) {
        err = wasi__memory_write_bytes(engine,
                                       core_module,
                                       options->api.memory_index,
                                       guest_ptr,
                                       guest_bytes,
                                       guest_size_bytes,
                                       "string lower memory write failed");
    }
    WASM_FREE(guest_bytes);
    if (err != WASI_OK) return err;
    *out_ptr = guest_ptr;
    return WASI_OK;
}

static wasi_error_t wasi__lower_list_pair(wasi_engine_t* engine,
                                          const wasi_component_t* component,
                                          const wasi_component_valtype_t* type,
                                          const wasi_value_t* value,
                                          wasm_module_t* core_module,
                                          const wasi__canon_runtime_options_t* options,
                                          uint32_t* out_ptr,
                                          uint32_t* out_len) {
    const wasi_component_valtype_t* resolved = wasi__resolve_valtype(component, type);
    uint32_t elem_size;
    uint32_t elem_align;
    uint32_t total_size = 0;
    uint32_t base_ptr = 0;
    size_t i;
    wasi_error_t err;

    if (out_ptr) *out_ptr = 0u;
    if (out_len) *out_len = 0u;
    if (!resolved || !value || !out_ptr || !out_len)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid list lowering inputs");
    if (resolved->opcode != 0x70u)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component value type is not a list");
    if (value->kind != WASI_VALUE_KIND_LIST)
        return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected list argument");
    if (value->of.seq.len > UINT32_MAX)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "list too large for canonical abi");

    err = wasi__layout_valtype(engine, component, resolved->data.unary.type, &elem_size, &elem_align);
    if (err != WASI_OK) return err;
    if (value->of.seq.len && elem_size > UINT32_MAX / value->of.seq.len)
        return wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical list layout overflow");
    total_size = (uint32_t)((size_t)elem_size * value->of.seq.len);

    err = wasi__canon_alloc_guest(engine, core_module, options, elem_align, total_size, &base_ptr);
    if (err != WASI_OK) return err;

    for (i = 0; i < value->of.seq.len; i++) {
        uint32_t elem_ptr;
        err = wasi__guest_add_offset(engine, base_ptr, (uint32_t)(i * (size_t)elem_size), &elem_ptr);
        if (err != WASI_OK) return err;
        err = wasi__store_value_to_memory(engine,
                                          component,
                                          resolved->data.unary.type,
                                          &value->of.seq.values[i],
                                          core_module,
                                          options,
                                          elem_ptr);
        if (err != WASI_OK) return err;
    }

    *out_ptr = base_ptr;
    *out_len = (uint32_t)value->of.seq.len;
    return WASI_OK;
}

static wasi_error_t wasi__store_scalar_to_memory(wasi_engine_t* engine,
                                                 const wasi_component_valtype_t* type,
                                                 const wasi_value_t* value,
                                                 wasm_module_t* core_module,
                                                 const wasi__canon_runtime_options_t* options,
                                                 uint32_t guest_ptr) {
    const char* action = "canonical memory write failed";
    uint32_t bits32;
    uint64_t bits64;

    switch (type->opcode) {
        case 0x7Fu:
            if (value->kind != WASI_VALUE_KIND_BOOL) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected bool argument");
            if (value->of.boolean > 1u) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "bool argument must be 0 or 1");
            return wasi__memory_write_u8(engine, core_module, options->api.memory_index, guest_ptr, value->of.boolean, action);
        case 0x7Eu:
            if (value->kind != WASI_VALUE_KIND_S8) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected s8 argument");
            return wasi__memory_write_u8(engine, core_module, options->api.memory_index, guest_ptr, (uint8_t)value->of.s8, action);
        case 0x7Du:
            if (value->kind != WASI_VALUE_KIND_U8) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected u8 argument");
            return wasi__memory_write_u8(engine, core_module, options->api.memory_index, guest_ptr, value->of.u8, action);
        case 0x7Cu:
            if (value->kind != WASI_VALUE_KIND_S16) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected s16 argument");
            return wasi__memory_write_u16(engine, core_module, options->api.memory_index, guest_ptr, (uint16_t)value->of.s16, action);
        case 0x7Bu:
            if (value->kind != WASI_VALUE_KIND_U16) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected u16 argument");
            return wasi__memory_write_u16(engine, core_module, options->api.memory_index, guest_ptr, value->of.u16, action);
        case 0x7Au:
            if (value->kind != WASI_VALUE_KIND_S32) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected s32 argument");
            return wasi__memory_write_u32(engine, core_module, options->api.memory_index, guest_ptr, (uint32_t)value->of.s32, action);
        case 0x79u:
            if (value->kind != WASI_VALUE_KIND_U32) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected u32 argument");
            return wasi__memory_write_u32(engine, core_module, options->api.memory_index, guest_ptr, value->of.u32, action);
        case 0x78u:
            if (value->kind != WASI_VALUE_KIND_S64) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected s64 argument");
            return wasi__memory_write_u64(engine, core_module, options->api.memory_index, guest_ptr, (uint64_t)value->of.s64, action);
        case 0x77u:
            if (value->kind != WASI_VALUE_KIND_U64) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected u64 argument");
            return wasi__memory_write_u64(engine, core_module, options->api.memory_index, guest_ptr, value->of.u64, action);
        case 0x76u:
            if (value->kind != WASI_VALUE_KIND_F32) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected f32 argument");
            memcpy(&bits32, &value->of.f32, sizeof(bits32));
            return wasi__memory_write_u32(engine, core_module, options->api.memory_index, guest_ptr, bits32, action);
        case 0x75u:
            if (value->kind != WASI_VALUE_KIND_F64) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected f64 argument");
            memcpy(&bits64, &value->of.f64, sizeof(bits64));
            return wasi__memory_write_u64(engine, core_module, options->api.memory_index, guest_ptr, bits64, action);
        case 0x74u:
            if (value->kind != WASI_VALUE_KIND_CHAR) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected char argument");
            if (!wasi__is_valid_unicode_scalar(value->of.char32))
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "char argument is not a unicode scalar value");
            return wasi__memory_write_u32(engine, core_module, options->api.memory_index, guest_ptr, value->of.char32, action);
        case 0x6Du:
            if (value->kind != WASI_VALUE_KIND_ENUM) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected enum argument");
            return wasi__memory_write_u32(engine, core_module, options->api.memory_index, guest_ptr, value->of.enum_case, action);
        default:
            return wasi__set_errorf(engine, WASI_ERR_NOT_IMPLEMENTED, "canonical memory store does not support %s yet", wasi__component_type_detail_string(type->opcode));
    }
}

static wasi_error_t wasi__store_value_to_memory(wasi_engine_t* engine,
                                                const wasi_component_t* component,
                                                const wasi_component_valtype_t* type,
                                                const wasi_value_t* value,
                                                wasm_module_t* core_module,
                                                const wasi__canon_runtime_options_t* options,
                                                uint32_t guest_ptr) {
    const wasi_component_valtype_t* resolved = wasi__resolve_valtype(component, type);
    wasi_error_t err;

    if (!resolved || !value || !core_module || !options)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid canonical memory store inputs");

    switch (resolved->opcode) {
        case 0x73u: {
            uint32_t ptr;
            uint32_t len;
            if (value->kind != WASI_VALUE_KIND_STRING)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected string argument");
            err = wasi__lower_string_pair(engine, &value->of.string, core_module, options, &ptr, &len);
            if (err != WASI_OK) return err;
            err = wasi__memory_write_u32(engine, core_module, options->api.memory_index, guest_ptr, ptr, "string pair write failed");
            if (err != WASI_OK) return err;
            return wasi__memory_write_u32(engine, core_module, options->api.memory_index, guest_ptr + 4u, len, "string pair write failed");
        }
        case 0x70u: {
            uint32_t ptr;
            uint32_t len;
            if (value->kind != WASI_VALUE_KIND_LIST)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected list argument");
            err = wasi__lower_list_pair(engine, component, resolved, value, core_module, options, &ptr, &len);
            if (err != WASI_OK) return err;
            err = wasi__memory_write_u32(engine, core_module, options->api.memory_index, guest_ptr, ptr, "list pair write failed");
            if (err != WASI_OK) return err;
            return wasi__memory_write_u32(engine, core_module, options->api.memory_index, guest_ptr + 4u, len, "list pair write failed");
        }
        case 0x67u: {
            uint32_t elem_size;
            uint32_t elem_align;
            size_t i;

            if (value->kind != WASI_VALUE_KIND_FIXED_LIST)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected fixed-list argument");
            if (value->of.seq.len != resolved->data.fixed_list.length)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "fixed-list argument length mismatch");

            err = wasi__layout_valtype(engine,
                                       component,
                                       resolved->data.fixed_list.element_type,
                                       &elem_size,
                                       &elem_align);
            if (err != WASI_OK) return err;
            (void)elem_align;

            for (i = 0; i < value->of.seq.len; i++) {
                uint32_t elem_ptr;
                err = wasi__guest_add_offset(engine, guest_ptr, (uint32_t)(i * (size_t)elem_size), &elem_ptr);
                if (err != WASI_OK) return err;
                err = wasi__store_value_to_memory(engine,
                                                  component,
                                                  resolved->data.fixed_list.element_type,
                                                  &value->of.seq.values[i],
                                                  core_module,
                                                  options,
                                                  elem_ptr);
                if (err != WASI_OK) return err;
            }
            return WASI_OK;
        }
        case 0x72u:
        case 0x6Fu: {
            uint32_t* offsets;
            uint32_t i;
            wasi_value_kind_t expected_kind = resolved->opcode == 0x72u ? WASI_VALUE_KIND_RECORD : WASI_VALUE_KIND_TUPLE;
            if (value->kind != expected_kind)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, resolved->opcode == 0x72u ? "expected record argument" : "expected tuple argument");
            if (value->of.seq.len != resolved->item_count)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "aggregate argument field count mismatch");
            offsets = resolved->item_count ? (uint32_t*)WASM_MALLOC((size_t)resolved->item_count * sizeof(*offsets)) : NULL;
            if (resolved->item_count && !offsets)
                return wasi__set_error_literal(engine, WASI_ERR_OOM, "aggregate layout alloc failed");
            err = wasi__layout_aggregate(engine, component, resolved, offsets, &i, &i);
            if (err != WASI_OK) {
                WASM_FREE(offsets);
                return err;
            }
            for (i = 0; i < resolved->item_count; i++) {
                uint32_t field_ptr;
                err = wasi__guest_add_offset(engine, guest_ptr, offsets[i], &field_ptr);
                if (err != WASI_OK) {
                    WASM_FREE(offsets);
                    return err;
                }
                err = wasi__store_value_to_memory(engine,
                                                  component,
                                                  resolved->data.aggregate.items[i].type,
                                                  &value->of.seq.values[i],
                                                  core_module,
                                                  options,
                                                  field_ptr);
                if (err != WASI_OK) {
                    WASM_FREE(offsets);
                    return err;
                }
            }
            WASM_FREE(offsets);
            return WASI_OK;
        }
        case 0x6Eu: {
            uint32_t words = wasi__canon_flags_word_count(resolved->item_count);
            uint32_t i;
            if (value->kind != WASI_VALUE_KIND_FLAGS)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected flags argument");
            if (value->of.flags.len < words)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "flags argument word count mismatch");
            if (resolved->item_count <= 8u)
                return wasi__memory_write_u8(engine, core_module, options->api.memory_index, guest_ptr, (uint8_t)value->of.flags.words[0], "flags write failed");
            if (resolved->item_count <= 16u)
                return wasi__memory_write_u16(engine, core_module, options->api.memory_index, guest_ptr, (uint16_t)value->of.flags.words[0], "flags write failed");
            for (i = 0; i < words; i++) {
                err = wasi__memory_write_u32(engine,
                                             core_module,
                                             options->api.memory_index,
                                             guest_ptr + i * 4u,
                                             value->of.flags.words[i],
                                             "flags write failed");
                if (err != WASI_OK) return err;
            }
            return WASI_OK;
        }
        case 0x71u:
        case 0x6Bu:
        case 0x6Au: {
            wasi_value_kind_t expected_kind = wasi__variant_value_kind(resolved->opcode);
            uint32_t case_count = wasi__variant_case_count(resolved);
            uint32_t case_index;
            uint32_t payload_offset;
            uint32_t variant_size;
            uint32_t variant_align;
            uint32_t payload_ptr;

            if (value->kind != expected_kind)
                return wasi__set_errorf(engine,
                                        WASI_ERR_TYPE_MISMATCH,
                                        "expected %s argument",
                                        wasi__component_type_detail_string(resolved->opcode));

            case_index = value->of.variant.case_index;
            if (case_index >= case_count)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "variant argument case index out of range");

            err = wasi__memory_write_u32(engine,
                                         core_module,
                                         options->api.memory_index,
                                         guest_ptr,
                                         case_index,
                                         "variant discriminant write failed");
            if (err != WASI_OK) return err;

            if (!wasi__variant_case_has_payload(resolved, case_index)) {
                if (value->of.variant.has_payload || value->of.variant.payload)
                    return wasi__set_error_literal(engine,
                                                   WASI_ERR_INVALID_ARGUMENT,
                                                   "variant argument payload mismatch");
                return WASI_OK;
            }
            if (!value->of.variant.has_payload || !value->of.variant.payload)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "variant argument payload is missing");

            err = wasi__layout_variant(engine,
                                       component,
                                       resolved,
                                       &payload_offset,
                                       &variant_size,
                                       &variant_align);
            if (err != WASI_OK) return err;
            (void)variant_size;
            (void)variant_align;

            err = wasi__guest_add_offset(engine, guest_ptr, payload_offset, &payload_ptr);
            if (err != WASI_OK) return err;
            return wasi__store_value_to_memory(engine,
                                               component,
                                               wasi__variant_case_payload_type(resolved, case_index),
                                               value->of.variant.payload,
                                               core_module,
                                               options,
                                               payload_ptr);
        }
        default:
            return wasi__store_scalar_to_memory(engine, resolved, value, core_module, options, guest_ptr);
    }
}

static wasi_error_t wasi__lower_value(wasi_engine_t* engine,
                                      const wasi_component_t* component,
                                      const wasi_component_valtype_t* type,
                                      const wasi_value_t* value,
                                      wasm_module_t* core_module,
                                      const wasi__canon_runtime_options_t* options,
                                      wasm_value_t* out_values,
                                      uint32_t* out_count) {
    const wasi_component_valtype_t* resolved = wasi__resolve_valtype(component, type);
    uint32_t ptr;
    uint32_t len;
    uint32_t i;
    uint32_t cursor = 0;
    wasi_error_t err;

    if (!resolved || !value || !core_module || !options || !out_values || !out_count)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid canonical lowering inputs");

    switch (resolved->opcode) {
        case 0x7Fu:
            if (value->kind != WASI_VALUE_KIND_BOOL) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected bool argument");
            if (value->of.boolean > 1u) return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "bool argument must be 0 or 1");
            out_values[0] = wasm_i32((int32_t)value->of.boolean);
            *out_count = 1u;
            return WASI_OK;
        case 0x7Eu:
            if (value->kind != WASI_VALUE_KIND_S8) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected s8 argument");
            out_values[0] = wasm_i32((int32_t)value->of.s8);
            *out_count = 1u;
            return WASI_OK;
        case 0x7Du:
            if (value->kind != WASI_VALUE_KIND_U8) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected u8 argument");
            out_values[0] = wasm_i32((int32_t)value->of.u8);
            *out_count = 1u;
            return WASI_OK;
        case 0x7Cu:
            if (value->kind != WASI_VALUE_KIND_S16) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected s16 argument");
            out_values[0] = wasm_i32((int32_t)value->of.s16);
            *out_count = 1u;
            return WASI_OK;
        case 0x7Bu:
            if (value->kind != WASI_VALUE_KIND_U16) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected u16 argument");
            out_values[0] = wasm_i32((int32_t)value->of.u16);
            *out_count = 1u;
            return WASI_OK;
        case 0x7Au:
            if (value->kind != WASI_VALUE_KIND_S32) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected s32 argument");
            out_values[0] = wasm_i32(value->of.s32);
            *out_count = 1u;
            return WASI_OK;
        case 0x79u:
            if (value->kind != WASI_VALUE_KIND_U32) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected u32 argument");
            out_values[0] = wasm_i32((int32_t)value->of.u32);
            *out_count = 1u;
            return WASI_OK;
        case 0x78u:
            if (value->kind != WASI_VALUE_KIND_S64) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected s64 argument");
            out_values[0] = wasm_i64(value->of.s64);
            *out_count = 1u;
            return WASI_OK;
        case 0x77u:
            if (value->kind != WASI_VALUE_KIND_U64) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected u64 argument");
            out_values[0] = wasm_i64((int64_t)value->of.u64);
            *out_count = 1u;
            return WASI_OK;
        case 0x76u:
            if (value->kind != WASI_VALUE_KIND_F32) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected f32 argument");
            out_values[0] = wasm_f32(value->of.f32);
            *out_count = 1u;
            return WASI_OK;
        case 0x75u:
            if (value->kind != WASI_VALUE_KIND_F64) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected f64 argument");
            out_values[0] = wasm_f64(value->of.f64);
            *out_count = 1u;
            return WASI_OK;
        case 0x74u:
            if (value->kind != WASI_VALUE_KIND_CHAR) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected char argument");
            if (!wasi__is_valid_unicode_scalar(value->of.char32))
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "char argument is not a unicode scalar value");
            out_values[0] = wasm_i32((int32_t)value->of.char32);
            *out_count = 1u;
            return WASI_OK;
        case 0x73u:
            if (value->kind != WASI_VALUE_KIND_STRING) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected string argument");
            err = wasi__lower_string_pair(engine, &value->of.string, core_module, options, &ptr, &len);
            if (err != WASI_OK) return err;
            out_values[0] = wasm_i32((int32_t)ptr);
            out_values[1] = wasm_i32((int32_t)len);
            *out_count = 2u;
            return WASI_OK;
        case 0x70u:
            err = wasi__lower_list_pair(engine, component, resolved, value, core_module, options, &ptr, &len);
            if (err != WASI_OK) return err;
            out_values[0] = wasm_i32((int32_t)ptr);
            out_values[1] = wasm_i32((int32_t)len);
            *out_count = 2u;
            return WASI_OK;
        case 0x67u:
            if (value->kind != WASI_VALUE_KIND_FIXED_LIST)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected fixed-list argument");
            if (value->of.seq.len != resolved->data.fixed_list.length)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "fixed-list argument length mismatch");
            for (i = 0; i < resolved->data.fixed_list.length; i++) {
                uint32_t added = 0;
                err = wasi__lower_value(engine,
                                        component,
                                        resolved->data.fixed_list.element_type,
                                        &value->of.seq.values[i],
                                        core_module,
                                        options,
                                        out_values + cursor,
                                        &added);
                if (err != WASI_OK) return err;
                cursor += added;
            }
            *out_count = cursor;
            return WASI_OK;
        case 0x72u:
        case 0x6Fu: {
            wasi_value_kind_t expected_kind = resolved->opcode == 0x72u ? WASI_VALUE_KIND_RECORD : WASI_VALUE_KIND_TUPLE;
            if (value->kind != expected_kind)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, resolved->opcode == 0x72u ? "expected record argument" : "expected tuple argument");
            if (value->of.seq.len != resolved->item_count)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "aggregate argument field count mismatch");
            for (i = 0; i < resolved->item_count; i++) {
                uint32_t added = 0;
                err = wasi__lower_value(engine,
                                        component,
                                        resolved->data.aggregate.items[i].type,
                                        &value->of.seq.values[i],
                                        core_module,
                                        options,
                                        out_values + cursor,
                                        &added);
                if (err != WASI_OK) return err;
                cursor += added;
            }
            *out_count = cursor;
            return WASI_OK;
        }
        case 0x6Eu: {
            uint32_t words = wasi__canon_flags_word_count(resolved->item_count);
            if (value->kind != WASI_VALUE_KIND_FLAGS)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected flags argument");
            if (value->of.flags.len < words)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "flags argument word count mismatch");
            for (i = 0; i < words; i++) out_values[i] = wasm_i32((int32_t)value->of.flags.words[i]);
            *out_count = words;
            return WASI_OK;
        }
        case 0x71u:
        case 0x6Bu:
        case 0x6Au: {
            wasi_value_kind_t expected_kind = wasi__variant_value_kind(resolved->opcode);
            uint32_t case_count = wasi__variant_case_count(resolved);
            uint32_t case_index;
            wasm_valtype_t* case_types = NULL;
            wasm_valtype_t* payload_types = NULL;
            wasm_value_t* case_values = NULL;
            uint32_t case_flat_count = 0;
            uint32_t payload_count = 0;

            if (value->kind != expected_kind)
                return wasi__set_errorf(engine,
                                        WASI_ERR_TYPE_MISMATCH,
                                        "expected %s argument",
                                        wasi__component_type_detail_string(resolved->opcode));

            case_index = value->of.variant.case_index;
            if (case_index >= case_count)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "variant argument case index out of range");

            err = wasi__merge_variant_flat_types(engine,
                                                 component,
                                                 resolved,
                                                 &payload_types,
                                                 &payload_count);
            if (err != WASI_OK) return err;

            out_values[0] = wasm_i32((int32_t)case_index);
            for (i = 0; i < payload_count; i++) out_values[i + 1u] = wasi__zero_wasm_value(payload_types[i]);

            if (!wasi__variant_case_has_payload(resolved, case_index)) {
                if (value->of.variant.has_payload || value->of.variant.payload) {
                    WASM_FREE(payload_types);
                    return wasi__set_error_literal(engine,
                                                   WASI_ERR_INVALID_ARGUMENT,
                                                   "variant argument payload mismatch");
                }
            } else {
                if (!value->of.variant.has_payload || !value->of.variant.payload) {
                    WASM_FREE(payload_types);
                    return wasi__set_error_literal(engine,
                                                   WASI_ERR_INVALID_ARGUMENT,
                                                   "variant argument payload is missing");
                }
                err = wasi__variant_case_flat_types(engine,
                                                   component,
                                                   resolved,
                                                   case_index,
                                                   NULL,
                                                   &case_flat_count);
                if (err != WASI_OK) {
                    WASM_FREE(payload_types);
                    return err;
                }
                if (case_flat_count) {
                    case_types = (wasm_valtype_t*)WASM_MALLOC((size_t)case_flat_count * sizeof(*case_types));
                    case_values = (wasm_value_t*)WASM_CALLOC(case_flat_count, sizeof(*case_values));
                    if (!case_types || !case_values) {
                        WASM_FREE(case_values);
                        WASM_FREE(case_types);
                        WASM_FREE(payload_types);
                        return wasi__set_error_literal(engine, WASI_ERR_OOM, "variant payload coercion alloc failed");
                    }
                    err = wasi__variant_case_flat_types(engine,
                                                       component,
                                                       resolved,
                                                       case_index,
                                                       case_types,
                                                       &case_flat_count);
                    if (err != WASI_OK) {
                        WASM_FREE(case_values);
                        WASM_FREE(case_types);
                        WASM_FREE(payload_types);
                        return err;
                    }
                }
                err = wasi__lower_value(engine,
                                        component,
                                        wasi__variant_case_payload_type(resolved, case_index),
                                        value->of.variant.payload,
                                        core_module,
                                        options,
                                        case_values,
                                        &cursor);
                if (err != WASI_OK) {
                    WASM_FREE(case_values);
                    WASM_FREE(case_types);
                    WASM_FREE(payload_types);
                    return err;
                }
                for (i = 0; i < cursor; i++) {
                    err = wasi__coerce_variant_flat_value_for_lower(engine,
                                                                    case_types[i],
                                                                    &case_values[i],
                                                                    payload_types[i],
                                                                    &out_values[i + 1u]);
                    if (err != WASI_OK) {
                        WASM_FREE(case_values);
                        WASM_FREE(case_types);
                        WASM_FREE(payload_types);
                        return err;
                    }
                }
            }

            WASM_FREE(case_values);
            WASM_FREE(case_types);
            WASM_FREE(payload_types);
            *out_count = 1u + payload_count;
            return WASI_OK;
        }
        case 0x6Du:
            if (value->kind != WASI_VALUE_KIND_ENUM)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "expected enum argument");
            if (value->of.enum_case >= resolved->item_count)
                return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "enum argument case index out of range");
            out_values[0] = wasm_i32((int32_t)value->of.enum_case);
            *out_count = 1u;
            return WASI_OK;
        default:
            return wasi__set_errorf(engine,
                                    WASI_ERR_NOT_IMPLEMENTED,
                                    "canonical abi does not support %s yet",
                                    wasi__component_type_detail_string(resolved->opcode));
    }
}

static wasi_error_t wasi__load_scalar_from_memory(wasi_engine_t* engine,
                                                  const wasi_component_valtype_t* type,
                                                  wasm_module_t* core_module,
                                                  const wasi__canon_runtime_options_t* options,
                                                  uint32_t guest_ptr,
                                                  wasi_value_t* out_value) {
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    wasi_error_t err;

    switch (type->opcode) {
        case 0x7Fu:
            err = wasi__memory_read_u8(engine, core_module, options->api.memory_index, guest_ptr, &u8, "bool load failed");
            if (err != WASI_OK) return err;
            if (u8 > 1u) return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "bool result must be 0 or 1");
            out_value->kind = WASI_VALUE_KIND_BOOL;
            out_value->of.boolean = u8;
            return WASI_OK;
        case 0x7Eu:
            err = wasi__memory_read_u8(engine, core_module, options->api.memory_index, guest_ptr, &u8, "s8 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_S8;
            out_value->of.s8 = (int8_t)u8;
            return WASI_OK;
        case 0x7Du:
            err = wasi__memory_read_u8(engine, core_module, options->api.memory_index, guest_ptr, &u8, "u8 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_U8;
            out_value->of.u8 = u8;
            return WASI_OK;
        case 0x7Cu:
            err = wasi__memory_read_u16(engine, core_module, options->api.memory_index, guest_ptr, &u16, "s16 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_S16;
            out_value->of.s16 = (int16_t)u16;
            return WASI_OK;
        case 0x7Bu:
            err = wasi__memory_read_u16(engine, core_module, options->api.memory_index, guest_ptr, &u16, "u16 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_U16;
            out_value->of.u16 = u16;
            return WASI_OK;
        case 0x7Au:
            err = wasi__memory_read_u32(engine, core_module, options->api.memory_index, guest_ptr, &u32, "s32 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_S32;
            out_value->of.s32 = (int32_t)u32;
            return WASI_OK;
        case 0x79u:
            err = wasi__memory_read_u32(engine, core_module, options->api.memory_index, guest_ptr, &u32, "u32 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_U32;
            out_value->of.u32 = u32;
            return WASI_OK;
        case 0x78u:
            err = wasi__memory_read_u64(engine, core_module, options->api.memory_index, guest_ptr, &u64, "s64 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_S64;
            out_value->of.s64 = (int64_t)u64;
            return WASI_OK;
        case 0x77u:
            err = wasi__memory_read_u64(engine, core_module, options->api.memory_index, guest_ptr, &u64, "u64 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_U64;
            out_value->of.u64 = u64;
            return WASI_OK;
        case 0x76u:
            err = wasi__memory_read_u32(engine, core_module, options->api.memory_index, guest_ptr, &u32, "f32 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_F32;
            memcpy(&out_value->of.f32, &u32, sizeof(u32));
            return WASI_OK;
        case 0x75u:
            err = wasi__memory_read_u64(engine, core_module, options->api.memory_index, guest_ptr, &u64, "f64 load failed");
            if (err != WASI_OK) return err;
            out_value->kind = WASI_VALUE_KIND_F64;
            memcpy(&out_value->of.f64, &u64, sizeof(u64));
            return WASI_OK;
        case 0x74u:
            err = wasi__memory_read_u32(engine, core_module, options->api.memory_index, guest_ptr, &u32, "char load failed");
            if (err != WASI_OK) return err;
            if (!wasi__is_valid_unicode_scalar(u32))
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "char result is not a unicode scalar value");
            out_value->kind = WASI_VALUE_KIND_CHAR;
            out_value->of.char32 = u32;
            return WASI_OK;
        case 0x6Du:
            err = wasi__memory_read_u32(engine, core_module, options->api.memory_index, guest_ptr, &u32, "enum load failed");
            if (err != WASI_OK) return err;
            if (u32 >= type->item_count)
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "enum result case index out of range");
            out_value->kind = WASI_VALUE_KIND_ENUM;
            out_value->of.enum_case = u32;
            return WASI_OK;
        default:
            return wasi__set_errorf(engine, WASI_ERR_NOT_IMPLEMENTED, "canonical memory load does not support %s yet", wasi__component_type_detail_string(type->opcode));
    }
}

static wasi_error_t wasi__load_value_from_memory(wasi_engine_t* engine,
                                                 const wasi_component_t* component,
                                                 const wasi_component_valtype_t* type,
                                                 wasm_module_t* core_module,
                                                 const wasi__canon_runtime_options_t* options,
                                                 uint32_t guest_ptr,
                                                 wasi_value_t* out_value) {
    const wasi_component_valtype_t* resolved = wasi__resolve_valtype(component, type);
    wasi_error_t err;

    if (!resolved || !out_value)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid canonical memory load inputs");

    wasi_value_init(out_value);
    switch (resolved->opcode) {
        case 0x73u: {
            uint32_t ptr;
            uint32_t len;
            err = wasi__memory_read_u32(engine, core_module, options->api.memory_index, guest_ptr, &ptr, "string pair load failed");
            if (err != WASI_OK) return err;
            err = wasi__memory_read_u32(engine, core_module, options->api.memory_index, guest_ptr + 4u, &len, "string pair load failed");
            if (err != WASI_OK) return err;
            return wasi__decode_guest_string(engine, options, core_module, ptr, len, out_value);
        }
        case 0x70u: {
            uint32_t ptr;
            uint32_t len;
            uint32_t elem_size;
            uint32_t elem_align;
            size_t i;
            err = wasi__memory_read_u32(engine, core_module, options->api.memory_index, guest_ptr, &ptr, "list pair load failed");
            if (err != WASI_OK) return err;
            err = wasi__memory_read_u32(engine, core_module, options->api.memory_index, guest_ptr + 4u, &len, "list pair load failed");
            if (err != WASI_OK) return err;
            err = wasi__layout_valtype(engine, component, resolved->data.unary.type, &elem_size, &elem_align);
            if (err != WASI_OK) return err;
            (void)elem_align;
            out_value->kind = WASI_VALUE_KIND_LIST;
            out_value->of.seq.len = len;
            out_value->of.seq.owned = 1;
            if (len) {
                out_value->of.seq.values = (wasi_value_t*)WASM_CALLOC(len, sizeof(*out_value->of.seq.values));
                if (!out_value->of.seq.values) return wasi__set_error_literal(engine, WASI_ERR_OOM, "list result alloc failed");
            }
            for (i = 0; i < len; i++) {
                uint32_t elem_ptr;
                err = wasi__guest_add_offset(engine, ptr, (uint32_t)(i * (size_t)elem_size), &elem_ptr);
                if (err != WASI_OK) return err;
                err = wasi__load_value_from_memory(engine,
                                                   component,
                                                   resolved->data.unary.type,
                                                   core_module,
                                                   options,
                                                   elem_ptr,
                                                   &out_value->of.seq.values[i]);
                if (err != WASI_OK) return err;
            }
            return WASI_OK;
        }
        case 0x67u: {
            uint32_t elem_size;
            uint32_t elem_align;
            size_t i;

            err = wasi__layout_valtype(engine,
                                       component,
                                       resolved->data.fixed_list.element_type,
                                       &elem_size,
                                       &elem_align);
            if (err != WASI_OK) return err;
            (void)elem_align;

            out_value->kind = WASI_VALUE_KIND_FIXED_LIST;
            out_value->of.seq.len = resolved->data.fixed_list.length;
            out_value->of.seq.owned = 1;
            if (resolved->data.fixed_list.length) {
                out_value->of.seq.values = (wasi_value_t*)WASM_CALLOC(resolved->data.fixed_list.length,
                                                                      sizeof(*out_value->of.seq.values));
                if (!out_value->of.seq.values)
                    return wasi__set_error_literal(engine, WASI_ERR_OOM, "fixed-list result alloc failed");
            }

            for (i = 0; i < resolved->data.fixed_list.length; i++) {
                uint32_t elem_ptr;
                err = wasi__guest_add_offset(engine, guest_ptr, (uint32_t)(i * (size_t)elem_size), &elem_ptr);
                if (err != WASI_OK) return err;
                err = wasi__load_value_from_memory(engine,
                                                   component,
                                                   resolved->data.fixed_list.element_type,
                                                   core_module,
                                                   options,
                                                   elem_ptr,
                                                   &out_value->of.seq.values[i]);
                if (err != WASI_OK) return err;
            }
            return WASI_OK;
        }
        case 0x72u:
        case 0x6Fu: {
            uint32_t* offsets;
            uint32_t i;
            uint32_t size_tmp;
            uint32_t align_tmp;
            out_value->kind = resolved->opcode == 0x72u ? WASI_VALUE_KIND_RECORD : WASI_VALUE_KIND_TUPLE;
            out_value->of.seq.len = resolved->item_count;
            out_value->of.seq.owned = 1;
            if (resolved->item_count) {
                out_value->of.seq.values = (wasi_value_t*)WASM_CALLOC(resolved->item_count, sizeof(*out_value->of.seq.values));
                if (!out_value->of.seq.values) return wasi__set_error_literal(engine, WASI_ERR_OOM, "aggregate result alloc failed");
            }
            offsets = resolved->item_count ? (uint32_t*)WASM_MALLOC((size_t)resolved->item_count * sizeof(*offsets)) : NULL;
            if (resolved->item_count && !offsets) return wasi__set_error_literal(engine, WASI_ERR_OOM, "aggregate layout alloc failed");
            err = wasi__layout_aggregate(engine, component, resolved, offsets, &size_tmp, &align_tmp);
            if (err != WASI_OK) {
                WASM_FREE(offsets);
                return err;
            }
            for (i = 0; i < resolved->item_count; i++) {
                uint32_t field_ptr;
                err = wasi__guest_add_offset(engine, guest_ptr, offsets[i], &field_ptr);
                if (err != WASI_OK) {
                    WASM_FREE(offsets);
                    return err;
                }
                err = wasi__load_value_from_memory(engine,
                                                   component,
                                                   resolved->data.aggregate.items[i].type,
                                                   core_module,
                                                   options,
                                                   field_ptr,
                                                   &out_value->of.seq.values[i]);
                if (err != WASI_OK) {
                    WASM_FREE(offsets);
                    return err;
                }
            }
            WASM_FREE(offsets);
            return WASI_OK;
        }
        case 0x6Eu: {
            uint32_t words = wasi__canon_flags_word_count(resolved->item_count);
            uint32_t i;
            out_value->kind = WASI_VALUE_KIND_FLAGS;
            out_value->of.flags.len = words;
            out_value->of.flags.owned = 1;
            out_value->of.flags.words = (uint32_t*)WASM_CALLOC(words, sizeof(*out_value->of.flags.words));
            if (!out_value->of.flags.words) return wasi__set_error_literal(engine, WASI_ERR_OOM, "flags result alloc failed");
            if (resolved->item_count <= 8u) {
                uint8_t raw;
                err = wasi__memory_read_u8(engine, core_module, options->api.memory_index, guest_ptr, &raw, "flags load failed");
                if (err != WASI_OK) return err;
                out_value->of.flags.words[0] = raw;
                return WASI_OK;
            }
            if (resolved->item_count <= 16u) {
                uint16_t raw;
                err = wasi__memory_read_u16(engine, core_module, options->api.memory_index, guest_ptr, &raw, "flags load failed");
                if (err != WASI_OK) return err;
                out_value->of.flags.words[0] = raw;
                return WASI_OK;
            }
            for (i = 0; i < words; i++) {
                err = wasi__memory_read_u32(engine,
                                            core_module,
                                            options->api.memory_index,
                                            guest_ptr + i * 4u,
                                            &out_value->of.flags.words[i],
                                            "flags load failed");
                if (err != WASI_OK) return err;
            }
            return WASI_OK;
        }
        case 0x71u:
        case 0x6Bu:
        case 0x6Au: {
            uint32_t case_index;
            uint32_t case_count = wasi__variant_case_count(resolved);
            uint32_t payload_offset;
            uint32_t variant_size;
            uint32_t variant_align;
            uint32_t payload_ptr;

            err = wasi__memory_read_u32(engine,
                                        core_module,
                                        options->api.memory_index,
                                        guest_ptr,
                                        &case_index,
                                        "variant discriminant load failed");
            if (err != WASI_OK) return err;
            if (case_index >= case_count)
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "variant result case index out of range");

            out_value->kind = wasi__variant_value_kind(resolved->opcode);
            out_value->of.variant.case_index = case_index;
            if (!wasi__variant_case_has_payload(resolved, case_index)) return WASI_OK;

            err = wasi__layout_variant(engine,
                                       component,
                                       resolved,
                                       &payload_offset,
                                       &variant_size,
                                       &variant_align);
            if (err != WASI_OK) return err;
            (void)variant_size;
            (void)variant_align;

            out_value->of.variant.payload = (wasi_value_t*)WASM_CALLOC(1u, sizeof(*out_value->of.variant.payload));
            if (!out_value->of.variant.payload)
                return wasi__set_error_literal(engine, WASI_ERR_OOM, "variant payload alloc failed");

            err = wasi__guest_add_offset(engine, guest_ptr, payload_offset, &payload_ptr);
            if (err != WASI_OK) {
                WASM_FREE(out_value->of.variant.payload);
                out_value->of.variant.payload = NULL;
                return err;
            }
            err = wasi__load_value_from_memory(engine,
                                               component,
                                               wasi__variant_case_payload_type(resolved, case_index),
                                               core_module,
                                               options,
                                               payload_ptr,
                                               out_value->of.variant.payload);
            if (err != WASI_OK) {
                wasi_value_destroy(out_value->of.variant.payload);
                WASM_FREE(out_value->of.variant.payload);
                out_value->of.variant.payload = NULL;
                return err;
            }

            out_value->of.variant.has_payload = 1;
            out_value->of.variant.payload_owned = 1;
            return WASI_OK;
        }
        default:
            return wasi__load_scalar_from_memory(engine, resolved, core_module, options, guest_ptr, out_value);
    }
}

static wasi_error_t wasi__lift_value(wasi_engine_t* engine,
                                     const wasi_component_t* component,
                                     const wasi_component_valtype_t* type,
                                     const wasm_value_t* flat_values,
                                     uint32_t flat_count,
                                     wasm_module_t* core_module,
                                     const wasi__canon_runtime_options_t* options,
                                     wasi_value_t* out_value,
                                     uint32_t* out_consumed) {
    const wasi_component_valtype_t* resolved = wasi__resolve_valtype(component, type);
    uint32_t i;
    uint32_t cursor = 0;
    wasi_error_t err;

    if (!resolved || !flat_values || !core_module || !options || !out_value || !out_consumed)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid canonical lifting inputs");

    *out_consumed = 0;
    wasi_value_init(out_value);

    switch (resolved->opcode) {
        case 0x7Fu: {
            uint32_t raw;
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "bool result requires one i32");
            raw = (uint32_t)flat_values[0].of.i32;
            if (raw > 1u) return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "bool result must be 0 or 1");
            out_value->kind = WASI_VALUE_KIND_BOOL;
            out_value->of.boolean = (uint8_t)raw;
            *out_consumed = 1u;
            return WASI_OK;
        }
        case 0x7Eu:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "s8 result requires one i32");
            if (flat_values[0].of.i32 < INT8_MIN || flat_values[0].of.i32 > INT8_MAX)
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "s8 result out of range");
            out_value->kind = WASI_VALUE_KIND_S8;
            out_value->of.s8 = (int8_t)flat_values[0].of.i32;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x7Du:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "u8 result requires one i32");
            if ((uint32_t)flat_values[0].of.i32 > UINT8_MAX)
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "u8 result out of range");
            out_value->kind = WASI_VALUE_KIND_U8;
            out_value->of.u8 = (uint8_t)flat_values[0].of.i32;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x7Cu:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "s16 result requires one i32");
            if (flat_values[0].of.i32 < INT16_MIN || flat_values[0].of.i32 > INT16_MAX)
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "s16 result out of range");
            out_value->kind = WASI_VALUE_KIND_S16;
            out_value->of.s16 = (int16_t)flat_values[0].of.i32;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x7Bu:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "u16 result requires one i32");
            if ((uint32_t)flat_values[0].of.i32 > UINT16_MAX)
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "u16 result out of range");
            out_value->kind = WASI_VALUE_KIND_U16;
            out_value->of.u16 = (uint16_t)flat_values[0].of.i32;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x7Au:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "s32 result requires one i32");
            out_value->kind = WASI_VALUE_KIND_S32;
            out_value->of.s32 = flat_values[0].of.i32;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x79u:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "u32 result requires one i32");
            out_value->kind = WASI_VALUE_KIND_U32;
            out_value->of.u32 = (uint32_t)flat_values[0].of.i32;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x78u:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I64)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "s64 result requires one i64");
            out_value->kind = WASI_VALUE_KIND_S64;
            out_value->of.s64 = flat_values[0].of.i64;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x77u:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I64)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "u64 result requires one i64");
            out_value->kind = WASI_VALUE_KIND_U64;
            out_value->of.u64 = (uint64_t)flat_values[0].of.i64;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x76u:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_F32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "f32 result requires one f32");
            out_value->kind = WASI_VALUE_KIND_F32;
            out_value->of.f32 = flat_values[0].of.f32;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x75u:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_F64)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "f64 result requires one f64");
            out_value->kind = WASI_VALUE_KIND_F64;
            out_value->of.f64 = flat_values[0].of.f64;
            *out_consumed = 1u;
            return WASI_OK;
        case 0x74u: {
            uint32_t raw;
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "char result requires one i32");
            raw = (uint32_t)flat_values[0].of.i32;
            if (!wasi__is_valid_unicode_scalar(raw))
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "char result is not a unicode scalar value");
            out_value->kind = WASI_VALUE_KIND_CHAR;
            out_value->of.char32 = raw;
            *out_consumed = 1u;
            return WASI_OK;
        }
        case 0x73u:
            if (flat_count < 2u || flat_values[0].type != WASM_TYPE_I32 || flat_values[1].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "string result requires two i32 values");
            *out_consumed = 2u;
            return wasi__decode_guest_string(engine,
                                             options,
                                             core_module,
                                             (uint32_t)flat_values[0].of.i32,
                                             (uint32_t)flat_values[1].of.i32,
                                             out_value);
        case 0x70u:
            if (flat_count < 2u || flat_values[0].type != WASM_TYPE_I32 || flat_values[1].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "list result requires two i32 values");
            *out_consumed = 2u;
            {
                uint32_t ptr = (uint32_t)flat_values[0].of.i32;
                uint32_t len = (uint32_t)flat_values[1].of.i32;
                uint32_t elem_size;
                uint32_t elem_align;
                size_t item_index;
                wasi_error_t err = wasi__layout_valtype(engine, component, resolved->data.unary.type, &elem_size, &elem_align);
                if (err != WASI_OK) return err;
                (void)elem_align;
                out_value->kind = WASI_VALUE_KIND_LIST;
                out_value->of.seq.len = len;
                out_value->of.seq.owned = 1;
                if (len) {
                    out_value->of.seq.values = (wasi_value_t*)WASM_CALLOC(len, sizeof(*out_value->of.seq.values));
                    if (!out_value->of.seq.values)
                        return wasi__set_error_literal(engine, WASI_ERR_OOM, "list result alloc failed");
                }
                for (item_index = 0; item_index < len; item_index++) {
                    uint32_t elem_ptr;
                    err = wasi__guest_add_offset(engine, ptr, (uint32_t)(item_index * (size_t)elem_size), &elem_ptr);
                    if (err != WASI_OK) return err;
                    err = wasi__load_value_from_memory(engine,
                                                       component,
                                                       resolved->data.unary.type,
                                                       core_module,
                                                       options,
                                                       elem_ptr,
                                                       &out_value->of.seq.values[item_index]);
                    if (err != WASI_OK) return err;
                }
                return WASI_OK;
            }
        case 0x67u:
            out_value->kind = WASI_VALUE_KIND_FIXED_LIST;
            out_value->of.seq.len = resolved->data.fixed_list.length;
            out_value->of.seq.owned = 1;
            if (resolved->data.fixed_list.length) {
                out_value->of.seq.values = (wasi_value_t*)WASM_CALLOC(resolved->data.fixed_list.length,
                                                                      sizeof(*out_value->of.seq.values));
                if (!out_value->of.seq.values)
                    return wasi__set_error_literal(engine, WASI_ERR_OOM, "fixed-list result alloc failed");
            }
            for (i = 0; i < resolved->data.fixed_list.length; i++) {
                uint32_t consumed = 0;
                wasi_error_t item_err = wasi__lift_value(engine,
                                                         component,
                                                         resolved->data.fixed_list.element_type,
                                                         flat_values + cursor,
                                                         flat_count - cursor,
                                                         core_module,
                                                         options,
                                                         &out_value->of.seq.values[i],
                                                         &consumed);
                if (item_err != WASI_OK) return item_err;
                cursor += consumed;
            }
            *out_consumed = cursor;
            return WASI_OK;
        case 0x72u:
        case 0x6Fu: {
            wasi_value_kind_t kind = resolved->opcode == 0x72u ? WASI_VALUE_KIND_RECORD : WASI_VALUE_KIND_TUPLE;
            out_value->kind = kind;
            out_value->of.seq.len = resolved->item_count;
            out_value->of.seq.owned = 1;
            if (resolved->item_count) {
                out_value->of.seq.values = (wasi_value_t*)WASM_CALLOC(resolved->item_count, sizeof(*out_value->of.seq.values));
                if (!out_value->of.seq.values) return wasi__set_error_literal(engine, WASI_ERR_OOM, "aggregate result alloc failed");
            }
            for (i = 0; i < resolved->item_count; i++) {
                uint32_t consumed = 0;
                wasi_error_t err = wasi__lift_value(engine,
                                                   component,
                                                   resolved->data.aggregate.items[i].type,
                                                   flat_values + cursor,
                                                   flat_count - cursor,
                                                   core_module,
                                                   options,
                                                   &out_value->of.seq.values[i],
                                                   &consumed);
                if (err != WASI_OK) return err;
                cursor += consumed;
            }
            *out_consumed = cursor;
            return WASI_OK;
        }
        case 0x6Eu: {
            uint32_t words = wasi__canon_flags_word_count(resolved->item_count);
            if (flat_count < words) return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "flags result arity mismatch");
            out_value->kind = WASI_VALUE_KIND_FLAGS;
            out_value->of.flags.len = words;
            out_value->of.flags.owned = 1;
            out_value->of.flags.words = (uint32_t*)WASM_CALLOC(words, sizeof(*out_value->of.flags.words));
            if (!out_value->of.flags.words) return wasi__set_error_literal(engine, WASI_ERR_OOM, "flags result alloc failed");
            for (i = 0; i < words; i++) {
                if (flat_values[i].type != WASM_TYPE_I32)
                    return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "flags result requires i32 words");
                out_value->of.flags.words[i] = (uint32_t)flat_values[i].of.i32;
            }
            *out_consumed = words;
            return WASI_OK;
        }
        case 0x71u:
        case 0x6Bu:
        case 0x6Au: {
            uint32_t case_index;
            uint32_t case_count = wasi__variant_case_count(resolved);
            wasm_valtype_t* case_types = NULL;
            wasm_value_t* case_values = NULL;
            wasm_valtype_t* payload_types = NULL;
            uint32_t case_flat_count = 0;
            uint32_t payload_count = 0;

            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "variant result requires one i32 discriminant");

            case_index = (uint32_t)flat_values[0].of.i32;
            if (case_index >= case_count)
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "variant result case index out of range");

            err = wasi__merge_variant_flat_types(engine,
                                                 component,
                                                 resolved,
                                                 &payload_types,
                                                 &payload_count);
            if (err != WASI_OK) return err;
            if (flat_count < 1u + payload_count) {
                WASM_FREE(payload_types);
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "variant result arity mismatch");
            }

            out_value->kind = wasi__variant_value_kind(resolved->opcode);
            out_value->of.variant.case_index = case_index;
            *out_consumed = 1u + payload_count;

            if (!wasi__variant_case_has_payload(resolved, case_index)) {
                WASM_FREE(payload_types);
                return WASI_OK;
            }

            err = wasi__variant_case_flat_types(engine,
                                               component,
                                               resolved,
                                               case_index,
                                               NULL,
                                               &case_flat_count);
            if (err != WASI_OK) {
                WASM_FREE(payload_types);
                return err;
            }

            if (case_flat_count) {
                case_types = (wasm_valtype_t*)WASM_MALLOC((size_t)case_flat_count * sizeof(*case_types));
                case_values = (wasm_value_t*)WASM_CALLOC(case_flat_count, sizeof(*case_values));
                if (!case_types || !case_values) {
                    WASM_FREE(case_values);
                    WASM_FREE(case_types);
                    WASM_FREE(payload_types);
                    return wasi__set_error_literal(engine, WASI_ERR_OOM, "variant payload coercion alloc failed");
                }
                err = wasi__variant_case_flat_types(engine,
                                                   component,
                                                   resolved,
                                                   case_index,
                                                   case_types,
                                                   &case_flat_count);
                if (err != WASI_OK) {
                    WASM_FREE(case_values);
                    WASM_FREE(case_types);
                    WASM_FREE(payload_types);
                    return err;
                }
                for (i = 0; i < case_flat_count; i++) {
                    err = wasi__coerce_variant_flat_value_for_lift(engine,
                                                                   payload_types[i],
                                                                   &flat_values[i + 1u],
                                                                   case_types[i],
                                                                   &case_values[i]);
                    if (err != WASI_OK) {
                        WASM_FREE(case_values);
                        WASM_FREE(case_types);
                        WASM_FREE(payload_types);
                        return err;
                    }
                }
            }

            out_value->of.variant.payload = (wasi_value_t*)WASM_CALLOC(1u, sizeof(*out_value->of.variant.payload));
            if (!out_value->of.variant.payload) {
                WASM_FREE(case_values);
                WASM_FREE(case_types);
                WASM_FREE(payload_types);
                return wasi__set_error_literal(engine, WASI_ERR_OOM, "variant payload alloc failed");
            }
            err = wasi__lift_value(engine,
                                   component,
                                   wasi__variant_case_payload_type(resolved, case_index),
                                   case_values ? case_values : flat_values + 1u,
                                   case_flat_count,
                                   core_module,
                                   options,
                                   out_value->of.variant.payload,
                                   &cursor);
            WASM_FREE(case_values);
            WASM_FREE(case_types);
            WASM_FREE(payload_types);
            if (err != WASI_OK) {
                wasi_value_destroy(out_value->of.variant.payload);
                WASM_FREE(out_value->of.variant.payload);
                out_value->of.variant.payload = NULL;
                return err;
            }

            out_value->of.variant.has_payload = 1;
            out_value->of.variant.payload_owned = 1;
            return WASI_OK;
        }
        case 0x6Du:
            if (flat_count < 1u || flat_values[0].type != WASM_TYPE_I32)
                return wasi__set_error_literal(engine, WASI_ERR_TYPE_MISMATCH, "enum result requires one i32");
            if ((uint32_t)flat_values[0].of.i32 >= resolved->item_count)
                return wasi__set_error_literal(engine, WASI_ERR_MALFORMED, "enum result case index out of range");
            out_value->kind = WASI_VALUE_KIND_ENUM;
            out_value->of.enum_case = (uint32_t)flat_values[0].of.i32;
            *out_consumed = 1u;
            return WASI_OK;
        default:
            return wasi__set_errorf(engine,
                                    WASI_ERR_NOT_IMPLEMENTED,
                                    "canonical abi does not support %s yet",
                                    wasi__component_type_detail_string(resolved->opcode));
    }
}

static wasi_error_t wasi__layout_func_values(wasi_engine_t* engine,
                                             const wasi_component_t* component,
                                             uint32_t type_index,
                                             int use_results,
                                             uint32_t* offsets,
                                             uint32_t* out_size,
                                             uint32_t* out_align) {
    uint32_t count = use_results ? wasi_component_func_type_result_count(component, type_index)
                                 : wasi_component_func_type_param_count(component, type_index);
    uint32_t offset = 0;
    uint32_t max_align = 1u;
    uint32_t i;

    if (!out_size || !out_align)
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "invalid function layout outputs");

    for (i = 0; i < count; i++) {
        const wasi_component_valtype_t* item_type = use_results
            ? wasi_component_func_type_result_type(component, type_index, i)
            : wasi_component_func_type_param_type(component, type_index, i);
        uint32_t item_size;
        uint32_t item_align;
        wasi_error_t err = wasi__layout_valtype(engine, component, item_type, &item_size, &item_align);
        if (err != WASI_OK) return err;
        err = wasi__canon_align_to(engine, offset, item_align, &offset);
        if (err != WASI_OK) return err;
        if (offsets) offsets[i] = offset;
        if (item_align > max_align) max_align = item_align;
        if (item_size > UINT32_MAX - offset) return wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical function layout overflow");
        offset += item_size;
    }

    *out_align = max_align;
    return wasi__canon_align_to(engine, offset, max_align, out_size);
}

static wasi_error_t wasi__canon_call_func_index(const wasi_component_t* component,
                                                uint32_t func_type_index,
                                                wasm_module_t* core_module,
                                                uint32_t func_index,
                                                const char* func_label,
                                                const wasi__canon_runtime_options_t* options,
                                                const wasi_value_t* args,
                                                uint32_t num_args,
                                                wasi_value_t* results,
                                                uint32_t num_results) {
    wasi_engine_t* engine;
    wasi__canon_func_sig_t sig;
    wasm_value_t* flat_args = NULL;
    wasm_value_t* flat_results = NULL;
    wasi_value_t* lifted_results = NULL;
    uint32_t* spill_offsets = NULL;
    uint32_t spill_size = 0;
    uint32_t spill_align = 1u;
    uint32_t arg_cursor = 0;
    uint32_t result_cursor = 0;
    uint32_t spill_ptr = 0;
    uint32_t i;
    wasi_error_t err;
    wasm_error_t wasm_err;

    memset(&sig, 0, sizeof(sig));
    if (!component || !component->engine || !core_module || !func_label || !options || (!args && num_args) || (!results && num_results))
        return WASI_ERR_INVALID_ARGUMENT;

    engine = component->engine;

    if (!wasi__component_type_is_func(component, func_type_index))
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component type index is not a function type");
    if (num_args != wasi_component_func_type_param_count(component, func_type_index))
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "canonical call argument count mismatch");
    if (num_results != wasi_component_func_type_result_count(component, func_type_index))
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "canonical call result count mismatch");

    err = wasi__validate_func_signature(engine, component, func_type_index, core_module, func_index, func_label, &sig);
    if (err != WASI_OK) return err;

    if (sig.num_param_types) {
        flat_args = (wasm_value_t*)WASM_CALLOC(sig.num_param_types, sizeof(*flat_args));
        if (!flat_args) {
            err = wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical call arg buffer alloc failed");
            goto cleanup;
        }
    }
    if (sig.num_result_types) {
        flat_results = (wasm_value_t*)WASM_CALLOC(sig.num_result_types, sizeof(*flat_results));
        if (!flat_results) {
            err = wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical call result buffer alloc failed");
            goto cleanup;
        }
    }
    if (num_results) {
        lifted_results = (wasi_value_t*)WASM_CALLOC(num_results, sizeof(*lifted_results));
        if (!lifted_results) {
            err = wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical call lifted result alloc failed");
            goto cleanup;
        }
    }

    if (sig.params_spilled) {
        spill_offsets = num_args ? (uint32_t*)WASM_MALLOC((size_t)num_args * sizeof(*spill_offsets)) : NULL;
        if (num_args && !spill_offsets) {
            err = wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical spill layout alloc failed");
            goto cleanup;
        }
        err = wasi__layout_func_values(engine, component, func_type_index, 0, spill_offsets, &spill_size, &spill_align);
        if (err != WASI_OK) goto cleanup;
        err = wasi__canon_alloc_guest(engine, core_module, options, spill_align, spill_size, &spill_ptr);
        if (err != WASI_OK) goto cleanup;
        for (i = 0; i < num_args; i++) {
            uint32_t item_ptr;
            err = wasi__guest_add_offset(engine, spill_ptr, spill_offsets[i], &item_ptr);
            if (err != WASI_OK) goto cleanup;
            err = wasi__store_value_to_memory(engine,
                                              component,
                                              wasi_component_func_type_param_type(component, func_type_index, i),
                                              &args[i],
                                              core_module,
                                              options,
                                              item_ptr);
            if (err != WASI_OK) goto cleanup;
        }
        flat_args[0] = wasm_i32((int32_t)spill_ptr);
        arg_cursor = 1u;
    } else {
        for (i = 0; i < num_args; i++) {
            uint32_t produced = 0;
            err = wasi__lower_value(engine,
                                    component,
                                    wasi_component_func_type_param_type(component, func_type_index, i),
                                    &args[i],
                                    core_module,
                                    options,
                                    flat_args + arg_cursor,
                                    &produced);
            if (err != WASI_OK) goto cleanup;
            arg_cursor += produced;
        }
    }

    if (sig.results_spilled) {
        WASM_FREE(spill_offsets);
        spill_offsets = num_results ? (uint32_t*)WASM_MALLOC((size_t)num_results * sizeof(*spill_offsets)) : NULL;
        if (num_results && !spill_offsets) {
            err = wasi__set_error_literal(engine, WASI_ERR_OOM, "canonical result spill layout alloc failed");
            goto cleanup;
        }
        err = wasi__layout_func_values(engine, component, func_type_index, 1, spill_offsets, &spill_size, &spill_align);
        if (err != WASI_OK) goto cleanup;
        if (!sig.results_retptr_returned) {
            err = wasi__canon_alloc_guest(engine, core_module, options, spill_align, spill_size, &spill_ptr);
            if (err != WASI_OK) goto cleanup;
            flat_args[arg_cursor++] = wasm_i32((int32_t)spill_ptr);
        }
    }

    wasm_err = wasm_call_index(core_module, func_index, flat_args, sig.num_param_types, flat_results, sig.num_result_types);
    if (wasm_err != WASM_OK) {
        err = wasi__set_runtime_error_from_module(engine, core_module, wasm_err, "canonical call failed");
        goto cleanup;
    }

    if (sig.results_spilled) {
        if (sig.results_retptr_returned) spill_ptr = (uint32_t)flat_results[0].of.i32;
        for (i = 0; i < num_results; i++) {
            uint32_t item_ptr;
            err = wasi__guest_add_offset(engine, spill_ptr, spill_offsets[i], &item_ptr);
            if (err != WASI_OK) goto cleanup;
            err = wasi__load_value_from_memory(engine,
                                               component,
                                               wasi_component_func_type_result_type(component, func_type_index, i),
                                               core_module,
                                               options,
                                               item_ptr,
                                               &lifted_results[i]);
            if (err != WASI_OK) goto cleanup;
        }
    } else {
        for (i = 0; i < num_results; i++) {
            uint32_t consumed = 0;
            err = wasi__lift_value(engine,
                                   component,
                                   wasi_component_func_type_result_type(component, func_type_index, i),
                                   flat_results + result_cursor,
                                   sig.num_result_types - result_cursor,
                                   core_module,
                                   options,
                                   &lifted_results[i],
                                   &consumed);
            if (err != WASI_OK) goto cleanup;
            result_cursor += consumed;
        }
    }

    if (options->has_post_return_func_index || (options->api.post_return_name && options->api.post_return_name[0])) {
        uint32_t post_index = 0;
        const char* post_label = options->api.post_return_name ? options->api.post_return_name : "<post-return>";
        const wasm_value_t* post_args = sig.results_spilled
                            ? (sig.results_retptr_returned ? flat_results : flat_args + (sig.num_param_types - 1u))
                            : flat_results;

        if (options->has_post_return_func_index) {
            post_index = options->post_return_func_index;
            post_label = "<post-return>";
            if (post_index >= wasm_func_count(core_module)) {
                err = wasi__set_error_literal(engine, WASI_ERR_UNDEFINED_EXPORT, "missing post-return function");
                goto cleanup;
            }
        } else {
            wasm_export_kind_t post_kind = WASM_EXPORT_FUNC;
            if (!wasm_find_export(core_module, options->api.post_return_name, &post_kind, &post_index) || post_kind != WASM_EXPORT_FUNC) {
                err = wasi__set_errorf(engine,
                                       WASI_ERR_UNDEFINED_EXPORT,
                                       "missing post-return export %s",
                                       options->api.post_return_name);
                goto cleanup;
            }
        }
        if (wasm_func_param_count(core_module, post_index) != sig.num_post_return_types || wasm_func_result_count(core_module, post_index) != 0u) {
            err = wasi__set_errorf(engine,
                                   WASI_ERR_TYPE_MISMATCH,
                                   "post-return function %s signature mismatch",
                                   post_label);
            goto cleanup;
        }
        for (i = 0; i < sig.num_post_return_types; i++) {
            if (wasm_func_param_type(core_module, post_index, i) != sig.post_return_types[i]) {
                err = wasi__set_errorf(engine,
                                       WASI_ERR_TYPE_MISMATCH,
                                       "post-return function %s parameter %u type mismatch",
                                       post_label,
                                       (unsigned)i);
                goto cleanup;
            }
        }
        wasm_err = wasm_call_index(core_module, post_index, (wasm_value_t*)post_args, sig.num_post_return_types, NULL, 0u);
        if (wasm_err != WASM_OK) {
            err = wasi__set_runtime_error_from_module(engine, core_module, wasm_err, "post-return call failed");
            goto cleanup;
        }
    }

    for (i = 0; i < num_results; i++) {
        results[i] = lifted_results[i];
        memset(&lifted_results[i], 0, sizeof(lifted_results[i]));
    }

    err = WASI_OK;

cleanup:
    if (lifted_results) {
        for (i = 0; i < num_results; i++) wasi_value_destroy(&lifted_results[i]);
    }
    WASM_FREE(spill_offsets);
    WASM_FREE(lifted_results);
    WASM_FREE(flat_args);
    WASM_FREE(flat_results);
    wasi__canon_func_sig_release(&sig);
    return err;
}


wasi_error_t wasi_canon_call(const wasi_component_t* component,
                             uint32_t func_type_index,
                             wasm_module_t* core_module,
                             const char* core_export_name,
                             const wasi_canon_options_t* options,
                             const wasi_value_t* args,
                             uint32_t num_args,
                             wasi_value_t* results,
                             uint32_t num_results) {
    wasi_engine_t* engine;
    wasi__canon_runtime_options_t effective_options;
    wasm_export_kind_t kind = WASM_EXPORT_FUNC;
    uint32_t func_index = 0;
    wasi_error_t err;

    if (!component || !component->engine || !core_module || !core_export_name || (!args && num_args) || (!results && num_results))
        return WASI_ERR_INVALID_ARGUMENT;

    engine = component->engine;
    wasi__clear_error(engine);

    if (!wasi__component_type_is_func(component, func_type_index))
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "component type index is not a function type");
    if (num_args != wasi_component_func_type_param_count(component, func_type_index))
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "canonical call argument count mismatch");
    if (num_results != wasi_component_func_type_result_count(component, func_type_index))
        return wasi__set_error_literal(engine, WASI_ERR_INVALID_ARGUMENT, "canonical call result count mismatch");

    err = wasi__effective_canon_options(&effective_options, options);
    if (err != WASI_OK) return wasi__set_error_literal(engine, err, wasi_error_string(err));

    if (!wasm_find_export(core_module, core_export_name, &kind, &func_index) || kind != WASM_EXPORT_FUNC)
        return wasi__set_errorf(engine, WASI_ERR_UNDEFINED_EXPORT, "missing core function export %s", core_export_name);

    return wasi__canon_call_func_index(component,
                                       func_type_index,
                                       core_module,
                                       func_index,
                                       core_export_name,
                                       &effective_options,
                                       args,
                                       num_args,
                                       results,
                                       num_results);
}

static int wasi__find_component_export(const wasi_component_t* component,
                                       const char* export_name,
                                       uint32_t* out_export_index) {
    uint32_t i;

    if (out_export_index) *out_export_index = 0;
    if (!component || !export_name || !out_export_index) return 0;

    for (i = 0; i < component->num_exports; i++) {
        const char* name = component->exports[i].name;
        if (name && strcmp(name, export_name) == 0) {
            *out_export_index = i;
            return 1;
        }
    }
    return 0;
}

static const wasi_component_canon_t* wasi__find_lift_for_defined_func(const wasi_component_t* component,
                                                                      uint32_t defined_func_index) {
    uint32_t i;

    if (!component) return NULL;
    for (i = 0; i < component->num_canons; i++) {
        if (component->canons[i].kind == WASI_COMPONENT_CANON_KIND_LIFT &&
            component->canons[i].defined_func_index == defined_func_index)
            return &component->canons[i];
    }
    return NULL;
}

wasi_instance_t* wasi_instantiate(const wasi_component_t* component) {
    wasi_instance_t* instance;
    wasi_engine_t* engine;

    if (!component || !component->engine) return NULL;
    engine = component->engine;
    wasi__clear_error(engine);

    if (component->num_imports != 0u || component->num_core_instances != 0u || component->num_instances != 0u ||
        component->num_aliases != 0u || component->num_nested_components != 0u || component->has_start) {
        wasi__set_error_literal(engine,
                                WASI_ERR_NOT_IMPLEMENTED,
                                "wasi_instantiate currently supports only single-module components without linking or start functions");
        return NULL;
    }
    if (component->num_core_modules != 1u || !component->core_modules[0].module) {
        wasi__set_error_literal(engine,
                                WASI_ERR_NOT_IMPLEMENTED,
                                "wasi_instantiate currently requires exactly one embedded core module");
        return NULL;
    }

    instance = (wasi_instance_t*)WASM_CALLOC(1, sizeof(*instance));
    if (!instance) {
        wasi__set_error_literal(engine, WASI_ERR_OOM, "instance alloc failed");
        return NULL;
    }

    instance->component = component;
    instance->core_module = component->core_modules[0].module;
    instance->core_module_index = 0u;
    return instance;
}

void wasi_free_instance(wasi_instance_t* instance) {
    WASM_FREE(instance);
}

wasi_error_t wasi_call(wasi_instance_t* instance,
                       const char* export_name,
                       const wasi_value_t* args,
                       uint32_t num_args,
                       wasi_value_t* results,
                       uint32_t num_results) {
    const wasi_component_t* component;
    wasi_engine_t* engine;
    uint32_t export_index = 0;
    uint32_t type_index = 0;
    const wasi_component_export_t* export_;
    const wasi_component_canon_t* canon;
    wasi__canon_runtime_options_t options;
    wasi_error_t err;

    if (!instance || !instance->component || !export_name || (!args && num_args) || (!results && num_results))
        return WASI_ERR_INVALID_ARGUMENT;

    component = instance->component;
    engine = component->engine;
    wasi__clear_error(engine);

    if (!wasi__find_component_export(component, export_name, &export_index))
        return wasi__set_errorf(engine, WASI_ERR_UNDEFINED_EXPORT, "missing component export %s", export_name);

    export_ = &component->exports[export_index];
    if (export_->kind != WASI_COMPONENT_EXTERN_KIND_FUNC)
        return wasi__set_errorf(engine, WASI_ERR_TYPE_MISMATCH, "component export %s is not a function", export_name);
    if (!wasi_component_export_func_type_index(component, export_index, &type_index))
        return wasi__set_errorf(engine, WASI_ERR_TYPE_MISMATCH, "component export %s does not resolve to a function type", export_name);

    canon = wasi__find_lift_for_defined_func(component, export_->index);
    if (!canon)
        return wasi__set_errorf(engine,
                                WASI_ERR_NOT_IMPLEMENTED,
                                "component export %s is not backed by a supported canon lift",
                                export_name);

    err = wasi__resolve_lift_runtime_options(engine, canon, &options);
    if (err != WASI_OK) return err;

    return wasi__canon_call_func_index(component,
                                       type_index,
                                       instance->core_module,
                                       canon->core_func_index,
                                       export_name,
                                       &options,
                                       args,
                                       num_args,
                                       results,
                                       num_results);
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

size_t wasi_component_section_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_sections) return 0;
    return component->sections[index].section_offset;
}

size_t wasi_component_section_payload_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_sections) return 0;
    return component->sections[index].payload_offset;
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

size_t wasi_component_type_offset(const wasi_component_t* component, uint32_t type_index) {
    if (!component || type_index >= component->num_types) return 0;
    return component->types[type_index].offset;
}

wasi_component_type_kind_t wasi_component_type_kind(const wasi_component_t* component, uint32_t type_index) {
    if (!component || type_index >= component->num_types) return WASI_COMPONENT_TYPE_KIND_UNKNOWN;
    return component->types[type_index].kind;
}

uint32_t wasi_component_type_decl_count(const wasi_component_t* component, uint32_t type_index) {
    if (!component || type_index >= component->num_types) return 0;
    return component->types[type_index].num_decls;
}

const wasi_component_type_decl_t* wasi_component_type_decl_at(const wasi_component_t* component,
                                                              uint32_t type_index,
                                                              uint32_t decl_index) {
    if (!component || type_index >= component->num_types) return NULL;
    if (decl_index >= component->types[type_index].num_decls) return NULL;
    return &component->types[type_index].decls[decl_index];
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

const wasi_component_valtype_t* wasi_component_func_type_param_type(const wasi_component_t* component,
                                                                    uint32_t type_index,
                                                                    uint32_t param_index) {
    if (!component || type_index >= component->num_types) return NULL;
    if (component->types[type_index].kind != WASI_COMPONENT_TYPE_KIND_FUNC) return NULL;
    if (param_index >= component->types[type_index].func.num_params) return NULL;
    return component->types[type_index].func.params[param_index].type;
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

const wasi_component_valtype_t* wasi_component_func_type_result_type(const wasi_component_t* component,
                                                                     uint32_t type_index,
                                                                     uint32_t result_index) {
    if (!component || type_index >= component->num_types) return NULL;
    if (component->types[type_index].kind != WASI_COMPONENT_TYPE_KIND_FUNC) return NULL;
    if (result_index >= component->types[type_index].func.num_results) return NULL;
    return component->types[type_index].func.results[result_index].type;
}

const wasi_component_valtype_t* wasi_component_type_defined_valtype(const wasi_component_t* component,
                                                                    uint32_t type_index) {
    if (!component || type_index >= component->num_types) return NULL;
    if (component->types[type_index].kind != WASI_COMPONENT_TYPE_KIND_DEFINED) return NULL;
    return component->types[type_index].defined_type;
}

uint8_t wasi_component_valtype_opcode(const wasi_component_valtype_t* type) {
    return type ? type->opcode : 0u;
}

uint32_t wasi_component_valtype_item_count(const wasi_component_valtype_t* type) {
    if (!type) return 0u;
    switch (type->opcode) {
        case 0x72u:
        case 0x71u:
        case 0x6Fu:
        case 0x6Eu:
        case 0x6Du:
            return type->item_count;
        default:
            return 0u;
    }
}

const char* wasi_component_valtype_item_name(const wasi_component_valtype_t* type, uint32_t item_index) {
    if (!type) return NULL;
    if (item_index >= wasi_component_valtype_item_count(type)) return NULL;
    return type->data.aggregate.items[item_index].name;
}

int wasi_component_valtype_item_has_type(const wasi_component_valtype_t* type, uint32_t item_index) {
    if (!type) return 0;
    if (item_index >= wasi_component_valtype_item_count(type)) return 0;
    return type->data.aggregate.items[item_index].has_type;
}

const wasi_component_valtype_t* wasi_component_valtype_item_type(const wasi_component_valtype_t* type,
                                                                 uint32_t item_index) {
    if (!type) return NULL;
    if (item_index >= wasi_component_valtype_item_count(type)) return NULL;
    return type->data.aggregate.items[item_index].type;
}

const wasi_component_valtype_t* wasi_component_valtype_element_type(const wasi_component_valtype_t* type) {
    if (!type) return NULL;
    switch (type->opcode) {
        case 0x70u:
        case 0x6Bu:
        case 0x66u:
        case 0x65u:
            return type->data.unary.type;
        case 0x67u:
            return type->data.fixed_list.element_type;
        default:
            return NULL;
    }
}

const wasi_component_valtype_t* wasi_component_valtype_ok_type(const wasi_component_valtype_t* type) {
    if (!type || type->opcode != 0x6Au) return NULL;
    return type->data.result.ok_type;
}

const wasi_component_valtype_t* wasi_component_valtype_err_type(const wasi_component_valtype_t* type) {
    if (!type || type->opcode != 0x6Au) return NULL;
    return type->data.result.err_type;
}

uint32_t wasi_component_valtype_type_index(const wasi_component_valtype_t* type) {
    if (!type) return UINT32_MAX;
    switch (type->opcode) {
        case 0x00u:
        case 0x68u:
        case 0x69u:
            return type->data.index.type_index;
        default:
            return UINT32_MAX;
    }
}

uint32_t wasi_component_valtype_fixed_length(const wasi_component_valtype_t* type) {
    if (!type || type->opcode != 0x67u) return UINT32_MAX;
    return type->data.fixed_list.length;
}

static void wasi__append_component_valtype_brief(char* buffer,
                                                 size_t buffer_size,
                                                 size_t* offset,
                                                 const wasi_component_valtype_t* type) {
    if (!type) {
        wasi__appendf(buffer, buffer_size, offset, "none");
        return;
    }

    switch (type->opcode) {
        case 0x00u:
            wasi__appendf(buffer, buffer_size, offset, "type[%u]", (unsigned)type->data.index.type_index);
            break;
        case 0x67u:
            wasi__appendf(buffer, buffer_size, offset, "fixed-list(len=%u, ", (unsigned)type->data.fixed_list.length);
            wasi__append_component_valtype_brief(buffer, buffer_size, offset, type->data.fixed_list.element_type);
            wasi__appendf(buffer, buffer_size, offset, ")");
            break;
        case 0x68u:
        case 0x69u:
            wasi__appendf(buffer,
                          buffer_size,
                          offset,
                          "%s[%u]",
                          wasi__component_type_detail_string(type->opcode),
                          (unsigned)type->data.index.type_index);
            break;
        default:
            wasi__appendf(buffer,
                          buffer_size,
                          offset,
                          "%s",
                          wasi__component_type_detail_string(type->opcode));
            break;
    }
}

static void wasi__append_component_externdesc_brief(char* buffer,
                                                    size_t buffer_size,
                                                    size_t* offset,
                                                    const wasi_component_externdesc_t* desc) {
    if (!desc) {
        wasi__appendf(buffer, buffer_size, offset, "<null>");
        return;
    }

    wasi__appendf(buffer,
                  buffer_size,
                  offset,
                  "%s",
                  wasi_component_extern_kind_string(desc->kind));
    if (desc->has_type_index) {
        wasi__appendf(buffer, buffer_size, offset, " type=%u", (unsigned)desc->type_index);
    }
    if (desc->value_type) {
        wasi__appendf(buffer, buffer_size, offset, " value=");
        wasi__append_component_valtype_brief(buffer, buffer_size, offset, desc->value_type);
    }
    if (desc->kind == WASI_COMPONENT_EXTERN_KIND_VALUE ||
        desc->kind == WASI_COMPONENT_EXTERN_KIND_TYPE ||
        desc->bound_tag) {
        wasi__appendf(buffer, buffer_size, offset, " bound=0x%02x", (unsigned)desc->bound_tag);
    }
}

static void wasi__append_core_externtype_brief(char* buffer,
                                               size_t buffer_size,
                                               size_t* offset,
                                               const wasi_component_core_externtype_t* type) {
    if (!type) {
        wasi__appendf(buffer, buffer_size, offset, "<null>");
        return;
    }

    wasi__appendf(buffer,
                  buffer_size,
                  offset,
                  "%s",
                  wasi_component_core_export_kind_string(type->kind));
    switch (type->kind) {
        case WASM_EXPORT_FUNC:
            wasi__appendf(buffer, buffer_size, offset, " type=%u", (unsigned)type->of.func_type_index);
            break;
        case WASM_EXPORT_TABLE:
            wasi__appendf(buffer,
                          buffer_size,
                          offset,
                          " min=%llu",
                          (unsigned long long)type->of.table.limits.min_size);
            if (type->of.table.limits.has_max) {
                wasi__appendf(buffer,
                              buffer_size,
                              offset,
                              " max=%llu",
                              (unsigned long long)type->of.table.limits.max_size);
            }
            break;
        case WASM_EXPORT_MEM:
            wasi__appendf(buffer,
                          buffer_size,
                          offset,
                          " min=%llu",
                          (unsigned long long)type->of.memory.limits.min_size);
            if (type->of.memory.limits.has_max) {
                wasi__appendf(buffer,
                              buffer_size,
                              offset,
                              " max=%llu",
                              (unsigned long long)type->of.memory.limits.max_size);
            }
            break;
        case WASM_EXPORT_GLOBAL:
            wasi__appendf(buffer,
                          buffer_size,
                          offset,
                          " mutable=%u",
                          (unsigned)type->of.global.is_mutable);
            break;
        case WASM_EXPORT_TAG:
            wasi__appendf(buffer,
                          buffer_size,
                          offset,
                          " type=%u",
                          (unsigned)type->of.tag.type_index);
            break;
        default:
            break;
    }
}

const char* wasi_component_import_name(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_imports) return NULL;
    return component->imports[index].name;
}

const char* wasi_component_import_interface_name(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_imports) return NULL;
    return component->imports[index].interface_name ? component->imports[index].interface_name
                                                    : component->imports[index].name;
}

const char* wasi_component_import_interface_version(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_imports) return NULL;
    return component->imports[index].interface_version;
}

wasi_component_extern_kind_t wasi_component_import_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_imports) return WASI_COMPONENT_EXTERN_KIND_UNKNOWN;
    return component->imports[index].kind;
}

uint32_t wasi_component_import_type_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_imports) return 0;
    return component->imports[index].type_index;
}

size_t wasi_component_import_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_imports) return 0;
    return component->imports[index].offset;
}

uint32_t wasi_component_export_count(const wasi_component_t* component) {
    return component ? component->num_exports : 0;
}

size_t wasi_component_export_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_exports) return 0;
    return component->exports[index].offset;
}

const char* wasi_component_export_name(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_exports) return NULL;
    return component->exports[index].name;
}

const char* wasi_component_export_interface_name(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_exports) return NULL;
    return component->exports[index].interface_name ? component->exports[index].interface_name
                                                    : component->exports[index].name;
}

const char* wasi_component_export_interface_version(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_exports) return NULL;
    return component->exports[index].interface_version;
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
        uint32_t annotated_type_index = component->exports[index].type_index;
        if (annotated_type_index < component->num_types &&
            component->types[annotated_type_index].kind == WASI_COMPONENT_TYPE_KIND_FUNC) {
            *out_type_index = annotated_type_index;
            return 1;
        }
    }
    func_index = component->exports[index].index;
    if (func_index >= component->num_funcs) return 0;
    *out_type_index = component->funcs[func_index].type_index;
    return 1;
}

uint32_t wasi_component_alias_count(const wasi_component_t* component) {
    return component ? component->num_aliases : 0;
}

size_t wasi_component_alias_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_aliases) return 0;
    return component->aliases[index].offset;
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

uint32_t wasi_component_core_type_count(const wasi_component_t* component) {
    return component ? component->num_core_types : 0;
}

size_t wasi_component_core_type_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_types) return 0;
    return component->core_types[index].offset;
}

const wasi_component_core_type_t* wasi_component_core_type_at(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_types) return NULL;
    return &component->core_types[index];
}

wasi_component_core_type_kind_t wasi_component_core_type_kind(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_types) return WASI_COMPONENT_CORE_TYPE_KIND_UNKNOWN;
    return component->core_types[index].kind;
}

uint8_t wasi_component_core_type_opcode(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_types) return 0u;
    return component->core_types[index].opcode;
}

uint8_t wasi_component_core_type_detail_opcode(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_types) return 0u;
    return component->core_types[index].detail_opcode;
}

uint32_t wasi_component_core_type_item_count(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_types) return 0u;
    return component->core_types[index].item_count;
}

int wasi_component_core_type_has_primary_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_types) return 0;
    return component->core_types[index].has_primary_index;
}

uint32_t wasi_component_core_type_primary_index(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_types) return UINT32_MAX;
    return component->core_types[index].primary_index;
}

uint32_t wasi_component_core_instance_count(const wasi_component_t* component) {
    return component ? component->num_core_instances : 0;
}

size_t wasi_component_core_instance_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_instances) return 0;
    return component->core_instances[index].offset;
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

size_t wasi_component_instance_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_instances) return 0;
    return component->instances[index].offset;
}

int wasi_component_has_start(const wasi_component_t* component) {
    return component ? component->has_start : 0;
}

size_t wasi_component_start_offset(const wasi_component_t* component) {
    if (!component || !component->has_start) return 0;
    return component->start_offset;
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

size_t wasi_component_canon_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_canons) return 0;
    return component->canons[index].offset;
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

size_t wasi_component_core_module_offset(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_modules) return 0;
    return component->core_modules[index].payload_offset;
}

size_t wasi_component_core_module_size(const wasi_component_t* component, uint32_t index) {
    if (!component || index >= component->num_core_modules) return 0;
    return component->core_modules[index].payload_size;
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
                  "kind=%s layer=0x%02x status=%s sections=%u core-types=%u core-modules=%u\n",
                  component->binary_kind == WASI_BINARY_KIND_COMPONENT ? "component"
                  : component->binary_kind == WASI_BINARY_KIND_CORE_MODULE ? "core"
                  : "unknown",
                  (unsigned)wasi_component_layer(component),
                  wasi_component_status_string(component->status),
                  (unsigned)component->num_sections,
                  (unsigned)component->num_core_types,
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

    for (i = 0; i < component->num_core_types; i++) {
        const wasi_component_core_type_t* type = &component->core_types[i];
        uint32_t j;
        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "core-type[%u]: kind=%s detail=%s",
                      (unsigned)i,
                      wasi_component_core_type_kind_string(type->kind),
                      wasi__component_core_type_detail_string(type->detail_opcode));
        if (type->kind == WASI_COMPONENT_CORE_TYPE_KIND_MODULE) {
            wasi__appendf(buffer, buffer_size, &offset, " decls=%u", (unsigned)type->item_count);
        } else if (type->opcode == 0x4Eu) {
            wasi__appendf(buffer, buffer_size, &offset, " group=%u", (unsigned)type->item_count);
        } else {
            if (type->num_supertypes) {
                wasi__appendf(buffer, buffer_size, &offset, " supertypes=%u", (unsigned)type->num_supertypes);
            }
            if (type->opcode == 0x4Fu || type->opcode == 0x50u) {
                wasi__appendf(buffer, buffer_size, &offset, " final=%u", (unsigned)type->is_final);
            }
            switch (type->detail_opcode) {
                case 0x60u:
                    wasi__appendf(buffer,
                                  buffer_size,
                                  &offset,
                                  " params=%u results=%u",
                                  (unsigned)type->data.func.num_params,
                                  (unsigned)type->data.func.num_results);
                    break;
                case 0x5Fu:
                    wasi__appendf(buffer,
                                  buffer_size,
                                  &offset,
                                  " fields=%u",
                                  (unsigned)type->data.struct_.num_fields);
                    break;
                case 0x5Eu:
                    wasi__appendf(buffer,
                                  buffer_size,
                                  &offset,
                                  " mutable=%u",
                                  (unsigned)type->data.array.field.is_mutable);
                    break;
                case 0x5Du:
                    wasi__appendf(buffer,
                                  buffer_size,
                                  &offset,
                                  " index=%u",
                                  (unsigned)type->data.cont.type_index);
                    break;
                default:
                    break;
            }
        }
        if (type->has_primary_index)
            wasi__appendf(buffer, buffer_size, &offset, " index=%u", (unsigned)type->primary_index);
        wasi__appendf(buffer, buffer_size, &offset, " offset=%llu", (unsigned long long)type->offset);
        wasi__appendf(buffer, buffer_size, &offset, "\n");

        if (type->kind == WASI_COMPONENT_CORE_TYPE_KIND_MODULE) {
            for (j = 0; j < type->data.module.num_decls; j++) {
                const wasi_component_core_module_decl_t* decl = &type->data.module.decls[j];
                wasi__appendf(buffer,
                              buffer_size,
                              &offset,
                              "  core-module-decl[%u,%u]: kind=%s",
                              (unsigned)i,
                              (unsigned)j,
                              wasi_component_core_module_decl_kind_string(decl->kind));
                switch (decl->kind) {
                    case WASI_COMPONENT_CORE_MODULE_DECL_KIND_IMPORT:
                        wasi__appendf(buffer,
                                      buffer_size,
                                      &offset,
                                      " module=%s name=%s desc=",
                                      decl->module_name ? decl->module_name : "",
                                      decl->name ? decl->name : "");
                        wasi__append_core_externtype_brief(buffer,
                                                           buffer_size,
                                                           &offset,
                                                           &decl->data.externtype);
                        break;
                    case WASI_COMPONENT_CORE_MODULE_DECL_KIND_TYPE:
                        wasi__appendf(buffer,
                                      buffer_size,
                                      &offset,
                                      " detail=%s",
                                      decl->data.type ? wasi__component_core_type_detail_string(decl->data.type->detail_opcode)
                                                      : "unknown");
                        break;
                    case WASI_COMPONENT_CORE_MODULE_DECL_KIND_ALIAS:
                        wasi__appendf(buffer,
                                      buffer_size,
                                      &offset,
                                      " target=%s count=%u index=%u",
                                      wasi_component_core_sort_string(decl->data.alias.sort_code),
                                      (unsigned)decl->data.alias.outer_count,
                                      (unsigned)decl->data.alias.outer_index);
                        break;
                    case WASI_COMPONENT_CORE_MODULE_DECL_KIND_EXPORT:
                        wasi__appendf(buffer,
                                      buffer_size,
                                      &offset,
                                      " name=%s desc=",
                                      decl->name ? decl->name : "");
                        wasi__append_core_externtype_brief(buffer,
                                                           buffer_size,
                                                           &offset,
                                                           &decl->data.externtype);
                        break;
                    default:
                        break;
                }
                wasi__appendf(buffer,
                              buffer_size,
                              &offset,
                              " offset=%llu",
                              (unsigned long long)decl->offset);
                wasi__appendf(buffer, buffer_size, &offset, "\n");
            }
        }
    }

    for (i = 0; i < component->num_types; i++) {
        const wasi_component_type_t* type = &component->types[i];
        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "type[%u]: kind=%s",
                      (unsigned)i,
                      wasi_component_type_kind_string(type->kind));
        if (type->kind == WASI_COMPONENT_TYPE_KIND_FUNC) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " async=%u params=%u results=%u",
                          (unsigned)type->async_flag,
                          (unsigned)type->func.num_params,
                          (unsigned)type->func.num_results);
        } else if (type->kind == WASI_COMPONENT_TYPE_KIND_COMPONENT ||
                   type->kind == WASI_COMPONENT_TYPE_KIND_INSTANCE) {
            wasi__appendf(buffer, buffer_size, &offset, " decls=%u", (unsigned)type->item_count);
        } else if (type->kind == WASI_COMPONENT_TYPE_KIND_RESOURCE) {
            wasi__appendf(buffer, buffer_size, &offset, " async=%u rep=%s",
                          (unsigned)type->async_flag,
                          type->detail_opcode == 0x7Fu ? "i32" : wasi_component_primitive_type_string(type->detail_opcode));
            if (type->has_primary_index)
                wasi__appendf(buffer, buffer_size, &offset, " dtor=%u", (unsigned)type->primary_index);
            if (type->has_secondary_index)
                wasi__appendf(buffer, buffer_size, &offset, " callback=%u", (unsigned)type->secondary_index);
        } else if (type->kind == WASI_COMPONENT_TYPE_KIND_DEFINED) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          " detail=%s",
                          wasi__component_type_detail_string(type->detail_opcode));
            switch (type->detail_opcode) {
                case 0x72u:
                    wasi__appendf(buffer, buffer_size, &offset, " fields=%u", (unsigned)type->defined_type->item_count);
                    break;
                case 0x71u:
                    wasi__appendf(buffer, buffer_size, &offset, " cases=%u", (unsigned)type->defined_type->item_count);
                    break;
                case 0x6Fu:
                    wasi__appendf(buffer, buffer_size, &offset, " items=%u", (unsigned)type->defined_type->item_count);
                    break;
                case 0x6Eu:
                case 0x6Du:
                    wasi__appendf(buffer, buffer_size, &offset, " names=%u", (unsigned)type->defined_type->item_count);
                    break;
                case 0x70u:
                case 0x6Bu:
                case 0x65u:
                case 0x66u:
                    wasi__appendf(buffer, buffer_size, &offset, " item=");
                    wasi__append_component_valtype_brief(buffer,
                                                         buffer_size,
                                                         &offset,
                                                         wasi_component_valtype_element_type(type->defined_type));
                    break;
                case 0x6Au:
                    wasi__appendf(buffer, buffer_size, &offset, " ok=");
                    wasi__append_component_valtype_brief(buffer,
                                                         buffer_size,
                                                         &offset,
                                                         wasi_component_valtype_ok_type(type->defined_type));
                    wasi__appendf(buffer, buffer_size, &offset, " err=");
                    wasi__append_component_valtype_brief(buffer,
                                                         buffer_size,
                                                         &offset,
                                                         wasi_component_valtype_err_type(type->defined_type));
                    break;
                case 0x67u:
                    wasi__appendf(buffer,
                                  buffer_size,
                                  &offset,
                                  " len=%u item=",
                                  (unsigned)wasi_component_valtype_fixed_length(type->defined_type));
                    wasi__append_component_valtype_brief(buffer,
                                                         buffer_size,
                                                         &offset,
                                                         wasi_component_valtype_element_type(type->defined_type));
                    break;
                case 0x00u:
                case 0x68u:
                case 0x69u:
                    wasi__appendf(buffer,
                                  buffer_size,
                                  &offset,
                                  " index=%u",
                                  (unsigned)wasi_component_valtype_type_index(type->defined_type));
                    break;
                default:
                    break;
            }
        }
            wasi__appendf(buffer, buffer_size, &offset, " offset=%llu", (unsigned long long)type->offset);
        wasi__appendf(buffer, buffer_size, &offset, "\n");

        if ((type->kind == WASI_COMPONENT_TYPE_KIND_COMPONENT ||
             type->kind == WASI_COMPONENT_TYPE_KIND_INSTANCE) && type->num_decls) {
            uint32_t decl_index;
            for (decl_index = 0; decl_index < type->num_decls; decl_index++) {
                const wasi_component_type_decl_t* decl = &type->decls[decl_index];
                wasi__appendf(buffer,
                              buffer_size,
                              &offset,
                              "  type-decl[%u,%u]: kind=%s",
                              (unsigned)i,
                              (unsigned)decl_index,
                              wasi_component_type_decl_kind_string(decl->kind));
                switch (decl->kind) {
                    case WASI_COMPONENT_TYPE_DECL_KIND_CORE_TYPE:
                        wasi__appendf(buffer,
                                      buffer_size,
                                      &offset,
                                      " core-kind=%s detail=%s",
                                      wasi_component_core_type_kind_string(decl->data.core_type.kind),
                                      wasi__component_core_type_detail_string(decl->data.core_type.detail_opcode));
                        if (decl->data.core_type.kind == WASI_COMPONENT_CORE_TYPE_KIND_MODULE) {
                            wasi__appendf(buffer,
                                          buffer_size,
                                          &offset,
                                          " decls=%u",
                                          (unsigned)decl->data.core_type.item_count);
                        }
                        break;
                    case WASI_COMPONENT_TYPE_DECL_KIND_TYPE:
                        wasi__appendf(buffer,
                                      buffer_size,
                                      &offset,
                                      " type-kind=%s",
                                      wasi_component_type_kind_string(decl->data.type.kind));
                        break;
                    case WASI_COMPONENT_TYPE_DECL_KIND_ALIAS:
                        wasi__appendf(buffer,
                                      buffer_size,
                                      &offset,
                                      " alias-kind=%s target=%s",
                                      wasi_component_alias_kind_string(decl->data.alias.kind),
                                      wasi__alias_sort_string(decl->data.alias.sort_is_core,
                                                              decl->data.alias.sort_code));
                        if (decl->data.alias.kind == WASI_COMPONENT_ALIAS_KIND_OUTER) {
                            wasi__appendf(buffer,
                                          buffer_size,
                                          &offset,
                                          " count=%u index=%u",
                                          (unsigned)decl->data.alias.outer_count,
                                          (unsigned)decl->data.alias.outer_index);
                        } else {
                            wasi__appendf(buffer,
                                          buffer_size,
                                          &offset,
                                          " instance=%u name=%s",
                                          (unsigned)decl->data.alias.instance_index,
                                          decl->data.alias.name ? decl->data.alias.name : "");
                        }
                        break;
                    case WASI_COMPONENT_TYPE_DECL_KIND_IMPORT:
                    case WASI_COMPONENT_TYPE_DECL_KIND_EXPORT:
                        wasi__appendf(buffer,
                                      buffer_size,
                                      &offset,
                                      " name=%s desc=",
                                      decl->name ? decl->name : "");
                        wasi__append_component_externdesc_brief(buffer,
                                                                buffer_size,
                                                                &offset,
                                                                &decl->data.externdesc);
                        break;
                    default:
                        break;
                }
                wasi__appendf(buffer,
                              buffer_size,
                              &offset,
                              " offset=%llu",
                              (unsigned long long)decl->offset);
                wasi__appendf(buffer, buffer_size, &offset, "\n");
            }
        }
    }

    for (i = 0; i < component->num_imports; i++) {
        const wasi_component_import_t* import = &component->imports[i];
        wasi__appendf(buffer,
                      buffer_size,
                      &offset,
                      "import[%u]: name=%s kind=%s type=%u offset=%llu\n",
                      (unsigned)i,
                      import->name ? import->name : "",
                      wasi_component_extern_kind_string(import->kind),
                      (unsigned)import->type_index,
                      (unsigned long long)import->offset);
        if (import->interface_version) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          "  import-interface[%u]: name=%s version=%s\n",
                          (unsigned)i,
                          import->interface_name ? import->interface_name : "",
                          import->interface_version);
        }
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
        wasi__appendf(buffer, buffer_size, &offset, " offset=%llu\n", (unsigned long long)export_->offset);
        if (export_->interface_version) {
            wasi__appendf(buffer,
                          buffer_size,
                          &offset,
                          "  export-interface[%u]: name=%s version=%s\n",
                          (unsigned)i,
                          export_->interface_name ? export_->interface_name : "",
                          export_->interface_version);
        }
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
        wasi__appendf(buffer, buffer_size, &offset, " offset=%llu\n", (unsigned long long)alias->offset);
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
        wasi__appendf(buffer, buffer_size, &offset, " offset=%llu\n", (unsigned long long)instance->offset);

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
        wasi__appendf(buffer, buffer_size, &offset, " offset=%llu\n", (unsigned long long)instance->offset);

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
                      "start: func=%u args=%u results=%u offset=%llu\n",
                      (unsigned)component->start_func_index,
                      (unsigned)component->start_arg_count,
                      (unsigned)component->start_result_count,
                      (unsigned long long)component->start_offset);
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
        } else {
            if (canon->has_type_index)
                wasi__appendf(buffer, buffer_size, &offset, " type=%u", (unsigned)canon->type_index);
            if (canon->item_count)
                wasi__appendf(buffer, buffer_size, &offset, " items=%u", (unsigned)canon->item_count);
            if (canon->has_secondary_index)
                wasi__appendf(buffer, buffer_size, &offset, " index=%u", (unsigned)canon->secondary_index);
            if (canon->flag)
                wasi__appendf(buffer, buffer_size, &offset, " flag=%u", (unsigned)canon->flag);
        }
        if (canon->num_options)
            wasi__appendf(buffer, buffer_size, &offset, " options=%u", (unsigned)canon->num_options);
        wasi__appendf(buffer, buffer_size, &offset, " offset=%llu\n", (unsigned long long)canon->offset);
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
        case WASI_COMPONENT_TYPE_KIND_DEFINED:
            return "defined";
        case WASI_COMPONENT_TYPE_KIND_FUNC:
            return "func";
        case WASI_COMPONENT_TYPE_KIND_COMPONENT:
            return "component";
        case WASI_COMPONENT_TYPE_KIND_INSTANCE:
            return "instance";
        case WASI_COMPONENT_TYPE_KIND_RESOURCE:
            return "resource";
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

const char* wasi_component_type_decl_kind_string(wasi_component_type_decl_kind_t kind) {
    switch (kind) {
        case WASI_COMPONENT_TYPE_DECL_KIND_CORE_TYPE:
            return "core-type";
        case WASI_COMPONENT_TYPE_DECL_KIND_TYPE:
            return "type";
        case WASI_COMPONENT_TYPE_DECL_KIND_ALIAS:
            return "alias";
        case WASI_COMPONENT_TYPE_DECL_KIND_IMPORT:
            return "import";
        case WASI_COMPONENT_TYPE_DECL_KIND_EXPORT:
            return "export";
        default:
            return "unknown";
    }
}

const char* wasi_component_core_type_kind_string(wasi_component_core_type_kind_t kind) {
    switch (kind) {
        case WASI_COMPONENT_CORE_TYPE_KIND_MODULE:
            return "module";
        case WASI_COMPONENT_CORE_TYPE_KIND_TYPE:
            return "type";
        default:
            return "unknown";
    }
}

const char* wasi_component_core_module_decl_kind_string(wasi_component_core_module_decl_kind_t kind) {
    switch (kind) {
        case WASI_COMPONENT_CORE_MODULE_DECL_KIND_IMPORT:
            return "import";
        case WASI_COMPONENT_CORE_MODULE_DECL_KIND_TYPE:
            return "type";
        case WASI_COMPONENT_CORE_MODULE_DECL_KIND_ALIAS:
            return "alias";
        case WASI_COMPONENT_CORE_MODULE_DECL_KIND_EXPORT:
            return "export";
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
        case WASI_COMPONENT_CANON_KIND_RESOURCE_NEW:
            return "resource.new";
        case WASI_COMPONENT_CANON_KIND_RESOURCE_DROP:
            return "resource.drop";
        case WASI_COMPONENT_CANON_KIND_RESOURCE_REP:
            return "resource.rep";
        case WASI_COMPONENT_CANON_KIND_BACKPRESSURE_SET:
            return "backpressure.set";
        case WASI_COMPONENT_CANON_KIND_TASK_RETURN:
            return "task.return";
        case WASI_COMPONENT_CANON_KIND_TASK_CANCEL:
            return "task.cancel";
        case WASI_COMPONENT_CANON_KIND_CONTEXT_GET:
            return "context.get";
        case WASI_COMPONENT_CANON_KIND_CONTEXT_SET:
            return "context.set";
        case WASI_COMPONENT_CANON_KIND_TASK_YIELD:
            return "task.yield";
        case WASI_COMPONENT_CANON_KIND_SUBTASK_CANCEL:
            return "subtask.cancel";
        case WASI_COMPONENT_CANON_KIND_SUBTASK_DROP:
            return "subtask.drop";
        case WASI_COMPONENT_CANON_KIND_STREAM_NEW:
            return "stream.new";
        case WASI_COMPONENT_CANON_KIND_STREAM_READ:
            return "stream.read";
        case WASI_COMPONENT_CANON_KIND_STREAM_WRITE:
            return "stream.write";
        case WASI_COMPONENT_CANON_KIND_STREAM_CANCEL_READ:
            return "stream.cancel-read";
        case WASI_COMPONENT_CANON_KIND_STREAM_CANCEL_WRITE:
            return "stream.cancel-write";
        case WASI_COMPONENT_CANON_KIND_STREAM_DROP_READABLE:
            return "stream.drop-readable";
        case WASI_COMPONENT_CANON_KIND_STREAM_DROP_WRITABLE:
            return "stream.drop-writable";
        case WASI_COMPONENT_CANON_KIND_FUTURE_NEW:
            return "future.new";
        case WASI_COMPONENT_CANON_KIND_FUTURE_READ:
            return "future.read";
        case WASI_COMPONENT_CANON_KIND_FUTURE_WRITE:
            return "future.write";
        case WASI_COMPONENT_CANON_KIND_FUTURE_CANCEL_READ:
            return "future.cancel-read";
        case WASI_COMPONENT_CANON_KIND_FUTURE_CANCEL_WRITE:
            return "future.cancel-write";
        case WASI_COMPONENT_CANON_KIND_FUTURE_DROP_READABLE:
            return "future.drop-readable";
        case WASI_COMPONENT_CANON_KIND_FUTURE_DROP_WRITABLE:
            return "future.drop-writable";
        case WASI_COMPONENT_CANON_KIND_ERROR_CONTEXT_NEW:
            return "error-context.new";
        case WASI_COMPONENT_CANON_KIND_ERROR_CONTEXT_DEBUG_MESSAGE:
            return "error-context.debug-message";
        case WASI_COMPONENT_CANON_KIND_ERROR_CONTEXT_DROP:
            return "error-context.drop";
        case WASI_COMPONENT_CANON_KIND_WAITABLE_SET_NEW:
            return "waitable-set.new";
        case WASI_COMPONENT_CANON_KIND_WAITABLE_SET_WAIT:
            return "waitable-set.wait";
        case WASI_COMPONENT_CANON_KIND_WAITABLE_SET_POLL:
            return "waitable-set.poll";
        case WASI_COMPONENT_CANON_KIND_WAITABLE_SET_DROP:
            return "waitable-set.drop";
        case WASI_COMPONENT_CANON_KIND_WAITABLE_JOIN:
            return "waitable.join";
        case WASI_COMPONENT_CANON_KIND_BACKPRESSURE_INC:
            return "backpressure.inc";
        case WASI_COMPONENT_CANON_KIND_BACKPRESSURE_DEC:
            return "backpressure.dec";
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
        case WASI_ERR_TYPE_MISMATCH:
            return "type mismatch";
        case WASI_ERR_UNDEFINED_EXPORT:
            return "undefined export";
        case WASI_ERR_RUNTIME:
            return "runtime error";
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
            return "component not parsed";
        case WASI_COMPONENT_STATUS_PARSED_SECTIONS:
            return "component section framing parsed";
        case WASI_COMPONENT_STATUS_PARSED_STRUCTURE:
            return "component structure parsed";
        default:
            return "unknown component status";
    }
}

#endif

#endif
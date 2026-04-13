#include <math.h>
#include <string.h>

#define WL_IMPL
#include "wl.h"

#define WASI_IMPL
#include "wasi.h"

static const uint8_t wasi_test_core_header[8] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x01,
    0x00,
    0x00,
    0x00,
};

static const uint8_t wasi_test_component_header_old[8] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
};

static const uint8_t wasi_test_component_header_new[8] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x01,
    0x00,
    0x01,
    0x00,
};

static const uint8_t wasi_test_component_with_sections[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x00,
    0x04,
    0x03,
    'w',
    'i',
    't',
    0x02,
    0x01,
    0x00,
};

static const uint8_t wasi_test_component_with_core_module[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x01,
    0x08,
    0x00,
    0x61,
    0x73,
    0x6D,
    0x01,
    0x00,
    0x00,
    0x00,
};

static const uint8_t wasi_test_component_with_import_export[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x07,
    0x05,
    0x01,
    0x40,
    0x00,
    0x01,
    0x00,
    0x0A,
    0x0D,
    0x01,
    0x00,
    0x08,
    'h',
    'o',
    's',
    't',
    '-',
    'l',
    'o',
    'g',
    0x01,
    0x00,
    0x0B,
    0x0A,
    0x01,
    0x00,
    0x04,
    'p',
    'i',
    'n',
    'g',
    0x01,
    0x01,
    0x00,
};

static const uint8_t wasi_test_component_with_component_instances[] = {
    0x00,
    0x61,
    0x73,
    0x6d,
    0x0d,
    0x00,
    0x01,
    0x00,
    0x07,
    0x05,
    0x01,
    0x40,
    0x00,
    0x01,
    0x00,
    0x0a,
    0x0d,
    0x01,
    0x00,
    0x08,
    'h',
    'o',
    's',
    't',
    '-',
    'l',
    'o',
    'g',
    0x01,
    0x00,
    0x05,
    0x0f,
    0x01,
    0x01,
    0x01,
    0x00,
    0x08,
    'h',
    'o',
    's',
    't',
    '-',
    'l',
    'o',
    'g',
    0x01,
    0x00,
    0x04,
    0x1e,
    0x00,
    0x61,
    0x73,
    0x6d,
    0x0d,
    0x00,
    0x01,
    0x00,
    0x07,
    0x05,
    0x01,
    0x40,
    0x00,
    0x01,
    0x00,
    0x0a,
    0x0d,
    0x01,
    0x00,
    0x08,
    'h',
    'o',
    's',
    't',
    '-',
    'l',
    'o',
    'g',
    0x01,
    0x00,
    0x05,
    0x0f,
    0x01,
    0x00,
    0x00,
    0x01,
    0x08,
    'h',
    'o',
    's',
    't',
    '-',
    'l',
    'o',
    'g',
    0x01,
    0x00,
};

static const uint8_t wasi_test_component_with_start[] = {
    0x00,
    0x61,
    0x73,
    0x6d,
    0x0d,
    0x00,
    0x01,
    0x00,
    0x07,
    0x05,
    0x01,
    0x40,
    0x00,
    0x01,
    0x00,
    0x0a,
    0x08,
    0x01,
    0x00,
    0x03,
    'r',
    'u',
    'n',
    0x01,
    0x00,
    0x09,
    0x03,
    0x00,
    0x00,
    0x00,
};

static const uint8_t wasi_test_component_with_alias_variants[] = {
    0x00,
    0x61,
    0x73,
    0x6d,
    0x0d,
    0x00,
    0x01,
    0x00,
    0x07,
    0x05,
    0x01,
    0x40,
    0x00,
    0x01,
    0x00,
    0x0a,
    0x0d,
    0x01,
    0x00,
    0x08,
    'h',
    'o',
    's',
    't',
    '-',
    'l',
    'o',
    'g',
    0x01,
    0x00,
    0x05,
    0x0f,
    0x01,
    0x01,
    0x01,
    0x00,
    0x08,
    'h',
    'o',
    's',
    't',
    '-',
    'l',
    'o',
    'g',
    0x01,
    0x00,
    0x06,
    0x0d,
    0x01,
    0x01,
    0x00,
    0x00,
    0x08,
    'h',
    'o',
    's',
    't',
    '-',
    'l',
    'o',
    'g',
    0x04,
    0x0f,
    0x00,
    0x61,
    0x73,
    0x6d,
    0x0d,
    0x00,
    0x01,
    0x00,
    0x06,
    0x05,
    0x01,
    0x03,
    0x02,
    0x01,
    0x00,
};

static const uint8_t wasi_test_component_with_core_instances[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x02,
    0x12,
    0x02,
    0x01,
    0x01,
    0x03,
    'l',
    'o',
    'g',
    0x00,
    0x00,
    0x00,
    0x02,
    0x01,
    0x03,
    'd',
    'e',
    'p',
    0x12,
    0x01,
};

static const uint8_t wasi_test_component_with_extended_types[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x07,
    0x28,
    0x0B,
    0x40,
    0x00,
    0x01,
    0x00,
    0x41,
    0x00,
    0x42,
    0x00,
    0x3F,
    0x7F,
    0x00,
    0x72,
    0x01,
    0x01,
    'x',
    0x79,
    0x71,
    0x01,
    0x02,
    'o',
    'k',
    0x01,
    0x73,
    0x00,
    0x70,
    0x7D,
    0x6B,
    0x73,
    0x6A,
    0x01,
    0x73,
    0x01,
    0x79,
    0x65,
    0x01,
    0x73,
    0x66,
    0x01,
    0x7D,
};

static const uint8_t wasi_test_component_with_versioned_names[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x07,
    0x05,
    0x01,
    0x40,
    0x00,
    0x01,
    0x00,
    0x0A,
    0x17,
    0x01,
    0x00,
    0x12,
    'w',
    'a',
    's',
    'i',
    ':',
    'c',
    'l',
    'i',
    '/',
    'r',
    'u',
    'n',
    '@',
    '0',
    '.',
    '3',
    '.',
    '0',
    0x01,
    0x00,
    0x0B,
    0x18,
    0x01,
    0x00,
    0x12,
    'w',
    'a',
    's',
    'i',
    ':',
    'c',
    'l',
    'i',
    '/',
    'r',
    'u',
    'n',
    '@',
    '0',
    '.',
    '3',
    '.',
    '0',
    0x01,
    0x00,
    0x00,
};

static const uint8_t wasi_test_component_with_canon_builtins[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x07,
    0x07,
    0x02,
    0x3F,
    0x7F,
    0x00,
    0x65,
    0x01,
    0x73,
    0x08,
    0x11,
    0x08,
    0x02,
    0x00,
    0x03,
    0x00,
    0x04,
    0x00,
    0x1F,
    0x0D,
    0x0E,
    0x01,
    0x15,
    0x01,
    0x09,
    0x01,
    0x00,
    0x00,
};

static const uint8_t wasi_test_component_with_async_lower_option[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x07,
    0x05,
    0x01,
    0x40,
    0x00,
    0x01,
    0x00,
    0x0A,
    0x06,
    0x01,
    0x00,
    0x01,
    'f',
    0x01,
    0x00,
    0x08,
    0x06,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x06,
};

static const uint8_t wasi_test_component_with_nested_core_types[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x07,
    0x24,
    0x02,
    0x41,
    0x01,
    0x00,
    0x50,
    0x04,
    0x01,
    0x50,
    0x00,
    0x60,
    0x01,
    0x7F,
    0x01,
    0x7F,
    0x00,
    0x01,
    0x6D,
    0x01,
    0x66,
    0x00,
    0x00,
    0x02,
    0x10,
    0x01,
    0x00,
    0x00,
    0x03,
    0x01,
    0x67,
    0x00,
    0x00,
    0x42,
    0x01,
    0x00,
    0x50,
    0x00,
};

static const uint8_t wasi_test_component_with_type_declarations[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x07,
    0x3C,
    0x02,
    0x41,
    0x04,
    0x01,
    0x40,
    0x00,
    0x01,
    0x00,
    0x02,
    0x03,
    0x02,
    0x01,
    0x00,
    0x03,
    0x00,
    0x03,
    'c',
    'f',
    'g',
    0x02,
    0x01,
    0x73,
    0x04,
    0x02,
    0x0D,
    'p',
    'k',
    'g',
    ':',
    'i',
    'f',
    'a',
    'c',
    'e',
    '/',
    'r',
    'u',
    'n',
    0x05,
    '0',
    '.',
    '3',
    '.',
    '0',
    0x01,
    0x00,
    0x42,
    0x02,
    0x01,
    0x70,
    0x7D,
    0x04,
    0x00,
    0x03,
    'c',
    'f',
    'g',
    0x02,
    0x00,
    0x00,
};

static const uint8_t wasi_test_component_with_top_level_core_types[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x03,
    0x13,
    0x03,
    0x60,
    0x00,
    0x00,
    0x50,
    0x02,
    0x01,
    0x60,
    0x01,
    0x7F,
    0x01,
    0x7F,
    0x03,
    0x01,
    0x67,
    0x00,
    0x00,
    0x5D,
    0x01,
};

static const uint8_t wasi_test_component_with_core_type_payloads[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x03,
    0x10,
    0x02,
    0x4E,
    0x02,
    0x5F,
    0x01,
    0x78,
    0x01,
    0x5E,
    0x7E,
    0x00,
    0x4F,
    0x01,
    0x00,
    0x60,
    0x00,
    0x00,
};

static const uint8_t wasi_test_component_with_scalar_func_types[] = {
    0x00,
    0x61,
    0x73,
    0x6D,
    0x0D,
    0x00,
    0x01,
    0x00,
    0x07,
    0x60,
    0x0E,
    0x40,
    0x01,
    0x01,
    'x',
    0x7F,
    0x00,
    0x7F,
    0x40,
    0x01,
    0x01,
    'x',
    0x77,
    0x00,
    0x77,
    0x40,
    0x01,
    0x01,
    'x',
    0x76,
    0x00,
    0x76,
    0x40,
    0x01,
    0x01,
    'x',
    0x75,
    0x00,
    0x75,
    0x40,
    0x01,
    0x01,
    'x',
    0x74,
    0x00,
    0x74,
    0x40,
    0x01,
    0x01,
    'x',
    0x73,
    0x00,
    0x73,
    0x40,
    0x00,
    0x00,
    0x7F,
    0x40,
    0x01,
    0x01,
    'x',
    0x7E,
    0x00,
    0x7E,
    0x40,
    0x01,
    0x01,
    'x',
    0x7D,
    0x00,
    0x7D,
    0x40,
    0x01,
    0x01,
    'x',
    0x7C,
    0x00,
    0x7C,
    0x40,
    0x01,
    0x01,
    'x',
    0x7B,
    0x00,
    0x7B,
    0x40,
    0x01,
    0x01,
    'x',
    0x7A,
    0x00,
    0x7A,
    0x40,
    0x01,
    0x01,
    'x',
    0x79,
    0x00,
    0x79,
    0x40,
    0x01,
    0x01,
    'x',
    0x78,
    0x00,
    0x78,
};

enum {
    WASI_TEST_SCALAR_TYPE_BOOL = 0u,
    WASI_TEST_SCALAR_TYPE_U64 = 1u,
    WASI_TEST_SCALAR_TYPE_F32 = 2u,
    WASI_TEST_SCALAR_TYPE_F64 = 3u,
    WASI_TEST_SCALAR_TYPE_CHAR = 4u,
    WASI_TEST_SCALAR_TYPE_STRING = 5u,
    WASI_TEST_SCALAR_TYPE_BAD_BOOL_RESULT = 6u,
    WASI_TEST_SCALAR_TYPE_S8 = 7u,
    WASI_TEST_SCALAR_TYPE_U8 = 8u,
    WASI_TEST_SCALAR_TYPE_S16 = 9u,
    WASI_TEST_SCALAR_TYPE_U16 = 10u,
    WASI_TEST_SCALAR_TYPE_S32 = 11u,
    WASI_TEST_SCALAR_TYPE_U32 = 12u,
    WASI_TEST_SCALAR_TYPE_S64 = 13u,
};

enum {
    WASI_TEST_COMPOUND_TYPE_RECORD = 0u,
    WASI_TEST_COMPOUND_TYPE_FLAGS = 1u,
    WASI_TEST_COMPOUND_TYPE_SPILL_SUM = 2u,
    WASI_TEST_COMPOUND_TYPE_OPTION = 3u,
    WASI_TEST_COMPOUND_TYPE_RESULT = 4u,
    WASI_TEST_COMPOUND_TYPE_VARIANT = 5u,
    WASI_TEST_COMPOUND_TYPE_VARIANT_JOIN = 6u,
    WASI_TEST_COMPOUND_TYPE_FIXED_LIST_FLAT = 7u,
    WASI_TEST_COMPOUND_TYPE_FIXED_LIST_SPILL = 8u,
    WASI_TEST_COMPOUND_TYPE_TUPLE = 9u,
};

enum {
    WASI_TEST_ENUM_TYPE_ECHO = 0u,
    WASI_TEST_ENUM_TYPE_BAD_FLAT = 1u,
    WASI_TEST_ENUM_TYPE_BAD_SPILLED = 2u,
};

typedef struct wasi_test_builder_t {
    uint8_t buf[2048];
    uint32_t len;
} wasi_test_builder_t;

static void wasi_test_emit(wasi_test_builder_t* b, uint8_t byte) {
    b->buf[b->len++] = byte;
}

static void wasi_test_emit_bytes(wasi_test_builder_t* b, const uint8_t* data, uint32_t len) {
    memcpy(b->buf + b->len, data, len);
    b->len += len;
}

static void wasi_test_emit_leb128_u32(wasi_test_builder_t* b, uint32_t val) {
    do {
        uint8_t byte = (uint8_t)(val & 0x7Fu);
        val >>= 7;
        if (val) byte |= 0x80u;
        wasi_test_emit(b, byte);
    } while (val);
}

static void wasi_test_emit_section(wasi_test_builder_t* b, uint8_t id, const uint8_t* content, uint32_t len) {
    wasi_test_emit(b, id);
    wasi_test_emit_leb128_u32(b, len);
    wasi_test_emit_bytes(b, content, len);
}

static void wasi_test_emit_header(wasi_test_builder_t* b) {
    static const uint8_t header[] = {
        0x00,
        0x61,
        0x73,
        0x6D,
        0x01,
        0x00,
        0x00,
        0x00,
    };
    wasi_test_emit_bytes(b, header, (uint32_t)sizeof(header));
}

static void wasi_test_emit_export(wasi_test_builder_t* b, const char* name, uint8_t kind, uint32_t index) {
    size_t len = strlen(name);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
    wasi_test_emit(b, kind);
    wasi_test_emit_leb128_u32(b, index);
}

static void wasi_test_emit_functype(wasi_test_builder_t* b,
                                    const uint8_t* params,
                                    uint32_t num_params,
                                    const uint8_t* results,
                                    uint32_t num_results) {
    uint32_t i;

    wasi_test_emit(b, 0x60);
    wasi_test_emit_leb128_u32(b, num_params);
    for (i = 0; i < num_params; i++) wasi_test_emit(b, params[i]);
    wasi_test_emit_leb128_u32(b, num_results);
    for (i = 0; i < num_results; i++) wasi_test_emit(b, results[i]);
}

static void wasi_test_emit_code_body(wasi_test_builder_t* code_sec,
                                     const uint8_t* locals,
                                     uint32_t locals_len,
                                     const uint8_t* ops,
                                     uint32_t ops_len) {
    wasi_test_builder_t body = { 0 };
    wasi_test_emit_bytes(&body, locals, locals_len);
    wasi_test_emit_bytes(&body, ops, ops_len);
    wasi_test_emit_leb128_u32(code_sec, body.len);
    wasi_test_emit_bytes(code_sec, body.buf, body.len);
}

static void wasi_test_build_scalar_core_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_params4[] = { 0x7F, 0x7F, 0x7F, 0x7F };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t i32_params2[] = { 0x7F, 0x7F };
    static const uint8_t i32_results2[] = { 0x7F, 0x7F };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t i64_param[] = { 0x7E };
    static const uint8_t i64_result[] = { 0x7E };
    static const uint8_t f32_param[] = { 0x7D };
    static const uint8_t f32_result[] = { 0x7D };
    static const uint8_t f64_param[] = { 0x7C };
    static const uint8_t f64_result[] = { 0x7C };
    static const uint8_t empty[] = { 0 };
    static const uint8_t cabi_realloc_locals[] = { 0x01, 0x01, 0x7F };
    static const uint8_t cabi_realloc_ops[] = {
        0x20,
        0x00,
        0x24,
        0x03,
        0x20,
        0x01,
        0x24,
        0x04,
        0x20,
        0x02,
        0x24,
        0x05,
        0x20,
        0x03,
        0x24,
        0x06,
        0x23,
        0x00,
        0x21,
        0x04,
        0x23,
        0x00,
        0x20,
        0x03,
        0x6A,
        0x24,
        0x00,
        0x20,
        0x04,
        0x0B,
    };
    static const uint8_t echo_ops[] = { 0x20, 0x00, 0x20, 0x01, 0x0B };
    static const uint8_t id_ops[] = { 0x20, 0x00, 0x0B };
    static const uint8_t post_echo_ops[] = {
        0x20,
        0x00,
        0x24,
        0x01,
        0x20,
        0x01,
        0x24,
        0x02,
        0x0B,
    };
    static const uint8_t const2_ops[] = { 0x41, 0x02, 0x0B };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 8u);
    wasi_test_emit_functype(&sec, i32_params4, 4u, i32_result, 1u);
    wasi_test_emit_functype(&sec, i32_params2, 2u, i32_results2, 2u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_functype(&sec, i64_param, 1u, i64_result, 1u);
    wasi_test_emit_functype(&sec, f32_param, 1u, f32_result, 1u);
    wasi_test_emit_functype(&sec, f64_param, 1u, f64_result, 1u);
    wasi_test_emit_functype(&sec, i32_params2, 2u, NULL, 0u);
    wasi_test_emit_functype(&sec, NULL, 0u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 8u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_leb128_u32(&sec, 5u);
    wasi_test_emit_leb128_u32(&sec, 6u);
    wasi_test_emit_leb128_u32(&sec, 7u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(mod, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 7u);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x20);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit_section(mod, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 15u);
    wasi_test_emit_export(&sec, "memory", 0x02u, 0u);
    wasi_test_emit_export(&sec, "cabi_realloc", 0x00u, 0u);
    wasi_test_emit_export(&sec, "echo", 0x00u, 1u);
    wasi_test_emit_export(&sec, "id32", 0x00u, 2u);
    wasi_test_emit_export(&sec, "id64", 0x00u, 3u);
    wasi_test_emit_export(&sec, "idf32", 0x00u, 4u);
    wasi_test_emit_export(&sec, "idf64", 0x00u, 5u);
    wasi_test_emit_export(&sec, "cabi_post_echo", 0x00u, 6u);
    wasi_test_emit_export(&sec, "const2", 0x00u, 7u);
    wasi_test_emit_export(&sec, "last_post_ptr", 0x03u, 1u);
    wasi_test_emit_export(&sec, "last_post_len", 0x03u, 2u);
    wasi_test_emit_export(&sec, "last_realloc_old_ptr", 0x03u, 3u);
    wasi_test_emit_export(&sec, "last_realloc_old_size", 0x03u, 4u);
    wasi_test_emit_export(&sec, "last_realloc_align", 0x03u, 5u);
    wasi_test_emit_export(&sec, "last_realloc_new_size", 0x03u, 6u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 8u);
    wasi_test_emit_code_body(&sec, cabi_realloc_locals, (uint32_t)sizeof(cabi_realloc_locals), cabi_realloc_ops, (uint32_t)sizeof(cabi_realloc_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, echo_ops, (uint32_t)sizeof(echo_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, id_ops, (uint32_t)sizeof(id_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, id_ops, (uint32_t)sizeof(id_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, id_ops, (uint32_t)sizeof(id_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, id_ops, (uint32_t)sizeof(id_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, post_echo_ops, (uint32_t)sizeof(post_echo_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, const2_ops, (uint32_t)sizeof(const2_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_expect_i32_global(wl_test_ctx* t,
                                        wasm_module_t* core_module,
                                        const char* name,
                                        int32_t expected) {
    wasm_value_t global_value;

    WL_REQUIRE(t, wasm_global_get(core_module, name, &global_value) == WASM_OK);
    WL_CHECK(t, global_value.of.i32 == expected);
}

static void wasi_test_expect_realloc_args(wl_test_ctx* t,
                                          wasm_module_t* core_module,
                                          int32_t old_ptr,
                                          int32_t old_size,
                                          int32_t align,
                                          int32_t new_size) {
    wasi_test_expect_i32_global(t, core_module, "last_realloc_old_ptr", old_ptr);
    wasi_test_expect_i32_global(t, core_module, "last_realloc_old_size", old_size);
    wasi_test_expect_i32_global(t, core_module, "last_realloc_align", align);
    wasi_test_expect_i32_global(t, core_module, "last_realloc_new_size", new_size);
}

typedef struct wasi_test_resource_drop_state_t {
    uint32_t drops;
    void* last_representation;
} wasi_test_resource_drop_state_t;

static void wasi_test_resource_destructor(void* representation, void* userdata) {
    wasi_test_resource_drop_state_t* state = (wasi_test_resource_drop_state_t*)userdata;

    if (!state) return;
    state->drops++;
    state->last_representation = representation;
}

static void wasi_test_emit_component_header(wasi_test_builder_t* b) {
    static const uint8_t header[] = {
        0x00,
        0x61,
        0x73,
        0x6D,
        0x0D,
        0x00,
        0x01,
        0x00,
    };
    wasi_test_emit_bytes(b, header, (uint32_t)sizeof(header));
}

static void wasi_test_emit_component_export(wasi_test_builder_t* b,
                                            const char* name,
                                            uint8_t kind,
                                            uint32_t index) {
    size_t len = strlen(name);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
    wasi_test_emit(b, kind);
    wasi_test_emit_leb128_u32(b, index);
    wasi_test_emit(b, 0x00u);
}

static void wasi_test_emit_component_import_instance(wasi_test_builder_t* b,
                                                     const char* name,
                                                     uint32_t type_index) {
    size_t len = strlen(name);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
    wasi_test_emit(b, 0x05u);
    wasi_test_emit_leb128_u32(b, type_index);
}

static void wasi_test_emit_component_import_module(wasi_test_builder_t* b,
                                                   const char* name,
                                                   uint32_t type_index) {
    size_t len = strlen(name);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit(b, 0x11u);
    wasi_test_emit_leb128_u32(b, type_index);
}

static void wasi_test_emit_component_import_func(wasi_test_builder_t* b,
                                                 const char* name,
                                                 uint32_t type_index) {
    size_t len = strlen(name);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
    wasi_test_emit(b, 0x01u);
    wasi_test_emit_leb128_u32(b, type_index);
}

static void wasi_test_emit_component_import_value(wasi_test_builder_t* b,
                                                  const char* name,
                                                  uint32_t type_index) {
    size_t len = strlen(name);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
    wasi_test_emit(b, 0x02u);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, type_index);
}

static void wasi_test_emit_component_import_component(wasi_test_builder_t* b,
                                                      const char* name,
                                                      uint32_t type_index) {
    size_t len = strlen(name);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
    wasi_test_emit(b, 0x04u);
    wasi_test_emit_leb128_u32(b, type_index);
}

static void wasi_test_emit_component_import_type(wasi_test_builder_t* b,
                                                 const char* name,
                                                 uint8_t bound_tag,
                                                 uint32_t type_index) {
    size_t len = strlen(name);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
    wasi_test_emit(b, 0x03u);
    wasi_test_emit(b, bound_tag);
    if (bound_tag == 0x00u) wasi_test_emit_leb128_u32(b, type_index);
}

static void wasi_test_emit_typed_component_export(wasi_test_builder_t* b,
                                                  const char* name,
                                                  uint8_t kind,
                                                  uint32_t index,
                                                  uint32_t type_index) {
    size_t len = strlen(name);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
    wasi_test_emit(b, kind);
    wasi_test_emit_leb128_u32(b, index);
    wasi_test_emit(b, 0x01u);
    wasi_test_emit_leb128_u32(b, type_index);
}

static void wasi_test_emit_component_plain_name(wasi_test_builder_t* b, const char* name) {
    size_t len = strlen(name);
    wasi_test_emit_leb128_u32(b, (uint32_t)len);
    wasi_test_emit_bytes(b, (const uint8_t*)name, (uint32_t)len);
}

static void wasi_test_emit_core_instance_export_alias(wasi_test_builder_t* b,
                                                      uint8_t sort_code,
                                                      uint32_t instance_index,
                                                      const char* name) {
    wasi_test_emit(b, 0x00u);
    wasi_test_emit(b, sort_code);
    wasi_test_emit(b, 0x01u);
    wasi_test_emit_leb128_u32(b, instance_index);
    wasi_test_emit_component_plain_name(b, name);
}

static void wasi_test_emit_component_instance_export_alias(wasi_test_builder_t* b,
                                                           uint8_t sort_code,
                                                           uint32_t instance_index,
                                                           const char* name) {
    wasi_test_emit(b, sort_code);
    wasi_test_emit(b, 0x00u);
    wasi_test_emit_leb128_u32(b, instance_index);
    wasi_test_emit_component_plain_name(b, name);
}

static void wasi_test_emit_component_outer_alias(wasi_test_builder_t* b,
                                                 uint8_t sort_code,
                                                 uint32_t outer_count,
                                                 uint32_t outer_index) {
    wasi_test_emit(b, sort_code);
    wasi_test_emit(b, 0x02u);
    wasi_test_emit_leb128_u32(b, outer_count);
    wasi_test_emit_leb128_u32(b, outer_index);
}

static void wasi_test_emit_core_outer_alias(wasi_test_builder_t* b,
                                            uint8_t sort_code,
                                            uint32_t outer_count,
                                            uint32_t outer_index) {
    wasi_test_emit(b, 0x00u);
    wasi_test_emit(b, sort_code);
    wasi_test_emit(b, 0x02u);
    wasi_test_emit_leb128_u32(b, outer_count);
    wasi_test_emit_leb128_u32(b, outer_index);
}

static void wasi_test_emit_record_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x72u);
    wasi_test_emit_leb128_u32(b, 3u);
    wasi_test_emit_component_plain_name(b, "a");
    wasi_test_emit(b, 0x79u);
    wasi_test_emit_component_plain_name(b, "b");
    wasi_test_emit(b, 0x73u);
    wasi_test_emit_component_plain_name(b, "c");
    wasi_test_emit(b, 0x70u);
    wasi_test_emit(b, 0x7Du);
}

static void wasi_test_emit_flags_valtype(wasi_test_builder_t* b) {
    uint32_t i;

    wasi_test_emit(b, 0x6Eu);
    wasi_test_emit_leb128_u32(b, 40u);
    for (i = 0; i < 40u; i++) {
        char name[4];
        name[0] = 'f';
        name[1] = (char)('0' + (int)((i / 10u) % 10u));
        name[2] = (char)('0' + (int)(i % 10u));
        name[3] = '\0';
        wasi_test_emit_component_plain_name(b, name);
    }
}

static void wasi_test_emit_fixed_list_u32_valtype(wasi_test_builder_t* b, uint32_t length) {
    wasi_test_emit(b, 0x67u);
    wasi_test_emit(b, 0x79u);
    wasi_test_emit_leb128_u32(b, length);
}

static void wasi_test_emit_tuple_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x6Fu);
    wasi_test_emit_leb128_u32(b, 3u);
    wasi_test_emit(b, 0x79u);
    wasi_test_emit(b, 0x73u);
    wasi_test_emit(b, 0x70u);
    wasi_test_emit(b, 0x7Du);
}

static void wasi_test_emit_option_string_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x6Bu);
    wasi_test_emit(b, 0x73u);
}

static void wasi_test_emit_result_u32_string_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x6Au);
    wasi_test_emit(b, 0x01u);
    wasi_test_emit(b, 0x79u);
    wasi_test_emit(b, 0x01u);
    wasi_test_emit(b, 0x73u);
}

static void wasi_test_emit_variant_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x71u);
    wasi_test_emit_leb128_u32(b, 3u);

    wasi_test_emit_component_plain_name(b, "none");
    wasi_test_emit(b, 0x00u);
    wasi_test_emit(b, 0x00u);

    wasi_test_emit_component_plain_name(b, "num");
    wasi_test_emit(b, 0x01u);
    wasi_test_emit(b, 0x79u);
    wasi_test_emit(b, 0x00u);

    wasi_test_emit_component_plain_name(b, "text");
    wasi_test_emit(b, 0x01u);
    wasi_test_emit(b, 0x73u);
    wasi_test_emit(b, 0x00u);
}

static void wasi_test_emit_variant_join_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x71u);
    wasi_test_emit_leb128_u32(b, 3u);

    wasi_test_emit_component_plain_name(b, "none");
    wasi_test_emit(b, 0x00u);
    wasi_test_emit(b, 0x00u);

    wasi_test_emit_component_plain_name(b, "num");
    wasi_test_emit(b, 0x01u);
    wasi_test_emit(b, 0x79u);
    wasi_test_emit(b, 0x00u);

    wasi_test_emit_component_plain_name(b, "big");
    wasi_test_emit(b, 0x01u);
    wasi_test_emit(b, 0x77u);
    wasi_test_emit(b, 0x00u);
}

static void wasi_test_emit_enum2_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x6Du);
    wasi_test_emit_leb128_u32(b, 2u);
    wasi_test_emit_component_plain_name(b, "a");
    wasi_test_emit_component_plain_name(b, "b");
}

static void wasi_test_emit_enum_record_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x72u);
    wasi_test_emit_leb128_u32(b, 2u);
    wasi_test_emit_component_plain_name(b, "tag");
    wasi_test_emit_enum2_valtype(b);
    wasi_test_emit_component_plain_name(b, "value");
    wasi_test_emit(b, 0x79u);
}

static void wasi_test_emit_resource_type(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x3Fu);
    wasi_test_emit(b, 0x7Fu);
    wasi_test_emit(b, 0x00u);
}

static void wasi_test_emit_resource_type_with_destructor(wasi_test_builder_t* b, uint32_t destructor_func_index) {
    wasi_test_emit(b, 0x3Fu);
    wasi_test_emit(b, 0x7Fu);
    wasi_test_emit(b, 0x01u);
    wasi_test_emit_leb128_u32(b, destructor_func_index);
}

static void wasi_test_emit_borrow_resource_valtype(wasi_test_builder_t* b, uint32_t type_index) {
    wasi_test_emit(b, 0x68u);
    wasi_test_emit_leb128_u32(b, type_index);
}

static void wasi_test_emit_own_resource_valtype(wasi_test_builder_t* b, uint32_t type_index) {
    wasi_test_emit(b, 0x69u);
    wasi_test_emit_leb128_u32(b, type_index);
}

typedef void (*wasi_test_emit_component_valtype_fn)(wasi_test_builder_t* b);

static void wasi_test_emit_top_level_borrow_resource_valtype(wasi_test_builder_t* b) {
    wasi_test_emit_borrow_resource_valtype(b, 0u);
}

static void wasi_test_emit_list_own_resource_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x70u);
    wasi_test_emit_own_resource_valtype(b, 0u);
}

static void wasi_test_emit_list_borrow_resource_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x70u);
    wasi_test_emit_borrow_resource_valtype(b, 0u);
}

static void wasi_test_emit_record_own_resource_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x72u);
    wasi_test_emit_leb128_u32(b, 2u);
    wasi_test_emit_component_plain_name(b, "tag");
    wasi_test_emit(b, 0x79u);
    wasi_test_emit_component_plain_name(b, "handle");
    wasi_test_emit_own_resource_valtype(b, 0u);
}

static void wasi_test_emit_variant_own_resource_valtype(wasi_test_builder_t* b) {
    wasi_test_emit(b, 0x71u);
    wasi_test_emit_leb128_u32(b, 2u);

    wasi_test_emit_component_plain_name(b, "none");
    wasi_test_emit(b, 0x00u);
    wasi_test_emit(b, 0x00u);

    wasi_test_emit_component_plain_name(b, "handle");
    wasi_test_emit(b, 0x01u);
    wasi_test_emit_own_resource_valtype(b, 0u);
    wasi_test_emit(b, 0x00u);
}

static void wasi_test_build_resource_core_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t i32_params2[] = { 0x7F, 0x7F };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t empty[] = { 0 };
    static const uint8_t id_ops[] = { 0x20, 0x00, 0x0B };
    static const uint8_t second_ops[] = { 0x20, 0x01, 0x0B };
    static const uint8_t end_ops[] = { 0x0B };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_functype(&sec, i32_params2, 2u, i32_result, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, NULL, 0u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_export(&sec, "id_handle", 0x00u, 0u);
    wasi_test_emit_export(&sec, "second_handle", 0x00u, 1u);
    wasi_test_emit_export(&sec, "consume_handle", 0x00u, 2u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_code_body(&sec, empty, 1u, id_ops, (uint32_t)sizeof(id_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, second_ops, (uint32_t)sizeof(second_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, end_ops, (uint32_t)sizeof(end_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_resource_destructor_core_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t empty[] = { 0 };
    static const uint8_t end_ops[] = { 0x0B };
    static const uint8_t record_drop_ops[] = {
        0x23, 0x01,
        0x41, 0x01,
        0x6A,
        0x24, 0x01,
        0x20, 0x00,
        0x24, 0x00,
        0x0B,
    };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, NULL, 0u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit_section(mod, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_export(&sec, "consume_handle", 0x00u, 0u);
    wasi_test_emit_export(&sec, "record_drop", 0x00u, 1u);
    wasi_test_emit_export(&sec, "last_drop", 0x03u, 0u);
    wasi_test_emit_export(&sec, "drop_count", 0x03u, 1u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_code_body(&sec, empty, 1u, end_ops, (uint32_t)sizeof(end_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, record_drop_ops, (uint32_t)sizeof(record_drop_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_instance_source_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t empty[] = { 0 };
    static const uint8_t inc_ops[] = { 0x20, 0x00, 0x41, 0x01, 0x6A, 0x0B };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "inc", 0x00u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_code_body(&sec, empty, 1u, inc_ops, (uint32_t)sizeof(inc_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_start_state_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t empty[] = { 0 };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t mark_ops[] = { 0x41, 0x01, 0x24, 0x00, 0x0B };
    static const uint8_t status_ops[] = { 0x23, 0x00, 0x0B };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_functype(&sec, NULL, 0u, NULL, 0u);
    wasi_test_emit_functype(&sec, NULL, 0u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x7Fu);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit_section(mod, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_export(&sec, "mark", 0x00u, 0u);
    wasi_test_emit_export(&sec, "status", 0x00u, 1u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_code_body(&sec, empty, 1u, mark_ops, (uint32_t)sizeof(mark_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, status_ops, (uint32_t)sizeof(status_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_instance_importing_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t empty[] = { 0 };
    static const uint8_t call_dep_ops[] = { 0x20, 0x00, 0x10, 0x00, 0x0B };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"dep", 3u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"inc", 3u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "call_dep", 0x00u, 1u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_code_body(&sec, empty, 1u, call_dep_ops, (uint32_t)sizeof(call_dep_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_instance_resource_importing_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t empty[] = { 0 };
    static const uint8_t call_dep_ops[] = { 0x20, 0x00, 0x10, 0x00, 0x0B };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"dep", 3u);
    wasi_test_emit_leb128_u32(&sec, 9u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"id_handle", 9u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "call_dep", 0x00u, 1u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_code_body(&sec, empty, 1u, call_dep_ops, (uint32_t)sizeof(call_dep_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_instance_incrementing_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t empty[] = { 0 };
    static const uint8_t call_dep_ops[] = { 0x20, 0x00, 0x10, 0x00, 0x41, 0x01, 0x6A, 0x0B };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"dep", 3u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"inc", 3u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "inc", 0x00u, 1u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_code_body(&sec, empty, 1u, call_dep_ops, (uint32_t)sizeof(call_dep_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_identity_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t empty[] = { 0 };
    static const uint8_t id_ops[] = { 0x20, 0x00, 0x0B };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "id", 0x00u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_code_body(&sec, empty, 1u, id_ops, (uint32_t)sizeof(id_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_global_source_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x7Fu);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x41u);
    wasi_test_emit(&sec, 0x07u);
    wasi_test_emit(&sec, 0x0Bu);
    wasi_test_emit_section(mod, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "g", 0x03u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);
}

static void wasi_test_build_core_global_importing_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t empty[] = { 0 };
    static const uint8_t read_global_ops[] = { 0x23u, 0x00u, 0x0Bu };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, NULL, 0u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"dep", 3u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"g", 1u);
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit(&sec, 0x7Fu);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_section(mod, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "read_dep", 0x00u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_code_body(&sec, empty, 1u, read_global_ops, (uint32_t)sizeof(read_global_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_memory_source_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t payload[] = { 42u, 0u, 0u, 0u };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(mod, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "mem", 0x02u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x41u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x0Bu);
    wasi_test_emit_leb128_u32(&sec, (uint32_t)sizeof(payload));
    wasi_test_emit_bytes(&sec, payload, (uint32_t)sizeof(payload));
    wasi_test_emit_section(mod, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_memory_importing_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_result[] = { 0x7Fu };
    static const uint8_t empty[] = { 0 };
    static const uint8_t read_memory_ops[] = { 0x41u, 0x00u, 0x28u, 0x02u, 0x00u, 0x0Bu };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, NULL, 0u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"dep", 3u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"mem", 3u);
    wasi_test_emit(&sec, 0x02u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(mod, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "read_dep", 0x00u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_code_body(&sec, empty, 1u, read_memory_ops, (uint32_t)sizeof(read_memory_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_table_source_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_param[] = { 0x7Fu };
    static const uint8_t i32_result[] = { 0x7Fu };
    static const uint8_t empty[] = { 0 };
    static const uint8_t inc_ops[] = { 0x20u, 0x00u, 0x41u, 0x01u, 0x6Au, 0x0Bu };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x70u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(mod, 4u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_export(&sec, "inc", 0x00u, 0u);
    wasi_test_emit_export(&sec, "tbl", 0x01u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x41u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x0Bu);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 9u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_code_body(&sec, empty, 1u, inc_ops, (uint32_t)sizeof(inc_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_table_importing_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_param[] = { 0x7Fu };
    static const uint8_t i32_result[] = { 0x7Fu };
    static const uint8_t empty[] = { 0 };
    static const uint8_t call_dep_ops[] = { 0x20u, 0x00u, 0x41u, 0x00u, 0x11u, 0x00u, 0x00u, 0x0Bu };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"dep", 3u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"tbl", 3u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit(&sec, 0x70u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(mod, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "call_dep", 0x00u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_code_body(&sec, empty, 1u, call_dep_ops, (uint32_t)sizeof(call_dep_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_core_tag_source_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, NULL, 0u, NULL, 0u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 13u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "tag", 0x04u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);
}

static void wasi_test_build_core_tag_importing_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_functype(&sec, NULL, 0u, NULL, 0u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"dep", 3u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"tag", 3u);
    wasi_test_emit(&sec, 0x04u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(mod, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_export(&sec, "dep_tag", 0x04u, 0u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);
}

static void wasi_test_build_resource_bounce_component(wasi_test_builder_t* component,
                                                      const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_resource_type(&sec);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "r");
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "bounce", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_resource_conflict_component(wasi_test_builder_t* component,
                                                        const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_resource_type(&sec);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_plain_name(&sec, "b");
    wasi_test_emit_borrow_resource_valtype(&sec, 0u);
    wasi_test_emit_component_plain_name(&sec, "o");
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "conflict", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_resource_sink_component(wasi_test_builder_t* component,
                                                    const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_resource_type(&sec);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "r");
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "take", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_resource_builtin_component(wasi_test_builder_t* component,
                                                       const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_resource_type(&sec);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "rep");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_own_resource_valtype(&sec, 0u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "handle");
    wasi_test_emit_borrow_resource_valtype(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "handle");
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit(&sec, 0x02u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x04u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_typed_component_export(&sec, "make", 0x01u, 0u, 1u);
    wasi_test_emit_typed_component_export(&sec, "peek", 0x01u, 1u, 2u);
    wasi_test_emit_typed_component_export(&sec, "drop", 0x01u, 2u, 3u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_resource_destructor_component(wasi_test_builder_t* component,
                                                          const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_resource_type_with_destructor(&sec, 0u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "handle");
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "rep");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);

    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_typed_component_export(&sec, "take", 0x01u, 1u, 1u);
    wasi_test_emit_typed_component_export(&sec, "drop", 0x01u, 2u, 1u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_resource_alias_destructor_component(wasi_test_builder_t* component,
                                                                const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "rep");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit_resource_type_with_destructor(&sec, 1u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"drop", 4u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "drop");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);
}

static void wasi_test_build_core_instance_u32_component(wasi_test_builder_t* component,
                                                        const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_start_state_component(wasi_test_builder_t* component,
                                                  const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_export(&sec, "run", 0x01u, 0u);
    wasi_test_emit_component_export(&sec, "status", 0x01u, 1u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_instance_arg_component(wasi_test_builder_t* component,
                                                        const wasi_test_builder_t* source_module,
                                                        const wasi_test_builder_t* importing_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, importing_module->buf, importing_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x12u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_instance_chain_component(wasi_test_builder_t* component,
                                                          const wasi_test_builder_t* source_module,
                                                          const wasi_test_builder_t* middle_module,
                                                          const wasi_test_builder_t* final_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, middle_module->buf, middle_module->len);
    wasi_test_emit_section(component, 1u, final_module->buf, final_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x12u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x12u);
    wasi_test_emit_leb128_u32(&sec, 1u);

    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_canon_lower_resource_link_component(wasi_test_builder_t* component,
                                                                const wasi_test_builder_t* source_module,
                                                                const wasi_test_builder_t* caller_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, caller_module->buf, caller_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "id_handle");
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x12u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x00u, 0u, "id_handle");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_resource_type(&sec);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "r");
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "run", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_direct_func_canon_lower_resource_link_component(wasi_test_builder_t* component,
                                                                            const wasi_test_builder_t* source_module,
                                                                            const wasi_test_builder_t* caller_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, caller_module->buf, caller_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "id_handle");
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x00u, 0u, "id_handle");
    wasi_test_emit_core_instance_export_alias(&sec, 0x00u, 1u, "id_handle");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_resource_type(&sec);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "r");
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "run", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_alias_lift_component(wasi_test_builder_t* component,
                                                      const wasi_test_builder_t* source_module,
                                                      const wasi_test_builder_t* identity_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, identity_module->buf, identity_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x00u, 0u, "inc");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_component_export_alias_component(wasi_test_builder_t* component,
                                                             const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_bytes(&sec, (const uint8_t*)"echo", 4u);
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 1u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_imported_instance_alias_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_instance(&sec, "wasi:test/ops@0.3.0", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_imported_func_alias_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_func(&sec, "wasi:test/ops/inc@0.3.0", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_imported_component_alias_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_component(&sec, "wasi:test/components@0.3.0", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_imported_module_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_module(&sec, "wasi:test/modules@0.3.0", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_parent_module_arg_component(wasi_test_builder_t* component,
                                                        const wasi_test_builder_t* nested_component,
                                                        const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);
    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "wasi:test/modules@0.3.0");
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_type_only_child_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_type(&sec, "ty", 0x01u, 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);
}

static void wasi_test_build_value_importing_child_component(wasi_test_builder_t* component,
                                                            uint8_t type_opcode) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, type_opcode);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_value(&sec, "seed", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);
}

static void wasi_test_build_parent_value_arg_component(wasi_test_builder_t* component,
                                                       const wasi_test_builder_t* nested_component,
                                                       uint8_t type_opcode) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, type_opcode);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_value(&sec, "seed", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "seed");
    wasi_test_emit(&sec, 0x02u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);
}

static void wasi_test_build_imported_type_value_child_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x73u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_import_type(&sec, "ty", 0x00u, 0u);
    wasi_test_emit_component_import_value(&sec, "seed", 1u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);
}

static void wasi_test_build_imported_type_value_parent_component(wasi_test_builder_t* component,
                                                                 const wasi_test_builder_t* nested_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x73u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_import_type(&sec, "ty", 0x00u, 0u);
    wasi_test_emit_component_import_value(&sec, "seed", 1u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_plain_name(&sec, "ty");
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_component_plain_name(&sec, "seed");
    wasi_test_emit(&sec, 0x02u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);
}

static void wasi_test_build_grandparent_imported_type_value_component(wasi_test_builder_t* component,
                                                                      const wasi_test_builder_t* nested_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x73u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_value(&sec, "seed", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_plain_name(&sec, "ty");
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_component_plain_name(&sec, "seed");
    wasi_test_emit(&sec, 0x02u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);
}

static void wasi_test_build_resource_type_only_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_resource_type(&sec);
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);
}

static void wasi_test_build_imported_resource_value_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_resource_type(&sec);
    wasi_test_emit_own_resource_valtype(&sec, 0u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_value(&sec, "seed", 1u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);
}

static void wasi_test_build_resource_typed_value_component(wasi_test_builder_t* component,
                                                           wasi_test_emit_component_valtype_fn emit_value_type,
                                                           int import_value) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_resource_type(&sec);
    emit_value_type(&sec);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    if (!import_value) return;

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_value(&sec, "seed", 1u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);
}

static void wasi_test_build_type_exporting_child_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_resource_type(&sec);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "ty", 0x03u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_parent_type_arg_component(wasi_test_builder_t* component,
                                                      const wasi_test_builder_t* nested_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_resource_type(&sec);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "ty");
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);
}

static void wasi_test_build_parent_instance_exported_type_arg_component(wasi_test_builder_t* component,
                                                                        const wasi_test_builder_t* exporting_component,
                                                                        const wasi_test_builder_t* importing_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 4u, exporting_component->buf, exporting_component->len);
    wasi_test_emit_section(component, 4u, importing_component->buf, importing_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x03u, 0u, "ty");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "ty");
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);
}

static void wasi_test_build_nested_component_alias_component(wasi_test_builder_t* component,
                                                             const wasi_test_builder_t* nested_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_global_arg_component(wasi_test_builder_t* component,
                                                      const wasi_test_builder_t* source_module,
                                                      const wasi_test_builder_t* importing_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, importing_module->buf, importing_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x03u, 0u, "g");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "read", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_global_from_exports_component(wasi_test_builder_t* component,
                                                               const wasi_test_builder_t* source_module,
                                                               const wasi_test_builder_t* importing_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, importing_module->buf, importing_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x03u, 0u, "g");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "g");
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x12u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "read", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_memory_arg_component(wasi_test_builder_t* component,
                                                      const wasi_test_builder_t* source_module,
                                                      const wasi_test_builder_t* importing_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, importing_module->buf, importing_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x02u, 0u, "mem");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x02u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "read", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_memory_from_exports_component(wasi_test_builder_t* component,
                                                               const wasi_test_builder_t* source_module,
                                                               const wasi_test_builder_t* importing_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, importing_module->buf, importing_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x02u, 0u, "mem");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "mem");
    wasi_test_emit(&sec, 0x02u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x12u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "read", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_table_arg_component(wasi_test_builder_t* component,
                                                     const wasi_test_builder_t* source_module,
                                                     const wasi_test_builder_t* importing_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, importing_module->buf, importing_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x01u, 0u, "tbl");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "call-dep", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_table_from_exports_component(wasi_test_builder_t* component,
                                                              const wasi_test_builder_t* source_module,
                                                              const wasi_test_builder_t* importing_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, importing_module->buf, importing_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x01u, 0u, "tbl");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "tbl");
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x12u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "call-dep", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_core_tag_arg_component(wasi_test_builder_t* component,
                                                   const wasi_test_builder_t* source_module,
                                                   const wasi_test_builder_t* importing_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, importing_module->buf, importing_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x04u, 0u, "tag");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x04u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);
}

static void wasi_test_build_core_tag_from_exports_component(wasi_test_builder_t* component,
                                                            const wasi_test_builder_t* source_module,
                                                            const wasi_test_builder_t* importing_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, source_module->buf, source_module->len);
    wasi_test_emit_section(component, 1u, importing_module->buf, importing_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_instance_export_alias(&sec, 0x04u, 0u, "tag");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "tag");
    wasi_test_emit(&sec, 0x04u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x12u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);
}

static void wasi_test_build_outer_func_alias_child_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_outer_alias(&sec, 0x01u, 1u, 0u);
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_outer_core_module_alias_child_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_core_outer_alias(&sec, 0x11u, 1u, 0u);
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_outer_type_alias_child_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_outer_alias(&sec, 0x03u, 1u, 0u);
    wasi_test_emit_component_outer_alias(&sec, 0x01u, 1u, 0u);
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_typed_component_export(&sec, "echo", 0x01u, 0u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_outer_instance_alias_child_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_outer_alias(&sec, 0x05u, 1u, 0u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_parent_outer_func_alias_component(wasi_test_builder_t* component,
                                                              const wasi_test_builder_t* nested_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_func(&sec, "wasi:test/ops/inc@0.3.0", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_parent_outer_core_module_alias_component(wasi_test_builder_t* component,
                                                                     const wasi_test_builder_t* nested_component,
                                                                     const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);
    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_parent_outer_instance_alias_component(wasi_test_builder_t* component,
                                                                  const wasi_test_builder_t* nested_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_instance(&sec, "wasi:test/ops@0.3.0", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_outer_component_alias_child_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_outer_alias(&sec, 0x04u, 1u, 0u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_parent_outer_component_alias_component(wasi_test_builder_t* component,
                                                                   const wasi_test_builder_t* nested_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_component(&sec, "wasi:test/components@0.3.0", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_outer_type_alias_importing_parent_component(wasi_test_builder_t* component,
                                                                        const wasi_test_builder_t* nested_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_import_type(&sec, "ty", 0x01u, 0u);
    wasi_test_emit_component_import_func(&sec, "wasi:test/ops/inc@0.3.0", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);
}

static void wasi_test_build_grandparent_outer_imported_type_alias_component(wasi_test_builder_t* component,
                                                                             const wasi_test_builder_t* nested_component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_import_func(&sec, "wasi:test/ops/inc@0.3.0", 0u);
    wasi_test_emit_section(component, 10u, sec.buf, sec.len);

    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_plain_name(&sec, "ty");
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_component_plain_name(&sec, "wasi:test/ops/inc@0.3.0");
    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);
}

static void wasi_test_build_nested_component_lower_link_component(wasi_test_builder_t* component,
                                                                  const wasi_test_builder_t* nested_component,
                                                                  const wasi_test_builder_t* caller_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, caller_module->buf, caller_module->len);
    wasi_test_emit_section(component, 4u, nested_component->buf, nested_component->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "inc");
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "dep");
    wasi_test_emit(&sec, 0x12u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 2u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_instance_export_alias(&sec, 0x01u, 0u, "echo");
    wasi_test_emit_section(component, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "n");
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x01u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "run", 0x01u, 1u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_compound_types_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };
    uint32_t i;

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 10u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "r");
    wasi_test_emit_record_valtype(&sec);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_record_valtype(&sec);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "f");
    wasi_test_emit_flags_valtype(&sec);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_flags_valtype(&sec);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 17u);
    for (i = 0; i < 17u; i++) {
        char name[3];
        name[0] = (char)('a' + (int)i);
        name[1] = '\0';
        wasi_test_emit_component_plain_name(&sec, name);
        wasi_test_emit(&sec, 0x79u);
    }
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x79u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "o");
    wasi_test_emit_option_string_valtype(&sec);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_option_string_valtype(&sec);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "r");
    wasi_test_emit_result_u32_string_valtype(&sec);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_result_u32_string_valtype(&sec);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "v");
    wasi_test_emit_variant_valtype(&sec);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_variant_valtype(&sec);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "v");
    wasi_test_emit_variant_join_valtype(&sec);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_variant_join_valtype(&sec);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "v");
    wasi_test_emit_fixed_list_u32_valtype(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_fixed_list_u32_valtype(&sec, 1u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "v");
    wasi_test_emit_fixed_list_u32_valtype(&sec, 17u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_fixed_list_u32_valtype(&sec, 17u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "t");
    wasi_test_emit_tuple_valtype(&sec);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_tuple_valtype(&sec);

    wasi_test_emit_section(component, 7u, sec.buf, sec.len);
}

static void wasi_test_build_compound_core_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_params4[] = { 0x7F, 0x7F, 0x7F, 0x7F };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t join_variant_params[] = { 0x7F, 0x7E, 0x7F };
    static const uint8_t i32_params6[] = { 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F };
    static const uint8_t i32_params3[] = { 0x7F, 0x7F, 0x7F };
    static const uint8_t i32_params2[] = { 0x7F, 0x7F };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t empty[] = { 0x00 };
    static const uint8_t id_ops[] = { 0x20, 0x00, 0x0B };
    static const uint8_t cabi_realloc_locals[] = { 0x01, 0x01, 0x7F };
    static const uint8_t cabi_realloc_ops[] = {
        0x20,
        0x00,
        0x24,
        0x01,
        0x20,
        0x01,
        0x24,
        0x02,
        0x20,
        0x02,
        0x24,
        0x03,
        0x20,
        0x03,
        0x24,
        0x04,
        0x23,
        0x00,
        0x20,
        0x02,
        0x41,
        0x01,
        0x6B,
        0x6A,
        0x20,
        0x02,
        0x6E,
        0x20,
        0x02,
        0x6C,
        0x21,
        0x04,
        0x20,
        0x04,
        0x20,
        0x03,
        0x6A,
        0x24,
        0x00,
        0x20,
        0x04,
        0x0B,
    };
    static const uint8_t record_echo_ops[] = {
        0x20,
        0x05,
        0x20,
        0x00,
        0x36,
        0x02,
        0x00,
        0x20,
        0x05,
        0x20,
        0x01,
        0x36,
        0x02,
        0x04,
        0x20,
        0x05,
        0x20,
        0x02,
        0x36,
        0x02,
        0x08,
        0x20,
        0x05,
        0x20,
        0x03,
        0x36,
        0x02,
        0x0C,
        0x20,
        0x05,
        0x20,
        0x04,
        0x36,
        0x02,
        0x10,
        0x0B,
    };
    static const uint8_t flags_echo_ops[] = {
        0x20,
        0x02,
        0x20,
        0x00,
        0x36,
        0x02,
        0x00,
        0x20,
        0x02,
        0x20,
        0x01,
        0x36,
        0x02,
        0x04,
        0x0B,
    };
    static const uint8_t tagged_echo_ops[] = {
        0x20,
        0x03,
        0x20,
        0x00,
        0x36,
        0x02,
        0x00,
        0x20,
        0x03,
        0x20,
        0x01,
        0x36,
        0x02,
        0x04,
        0x20,
        0x03,
        0x20,
        0x02,
        0x36,
        0x02,
        0x08,
        0x0B,
    };
    static const uint8_t spill_sum_ops[] = {
        0x20,
        0x00,
        0x28,
        0x02,
        0x00,
        0x20,
        0x00,
        0x28,
        0x02,
        0x40,
        0x6A,
        0x0B,
    };
    static const uint8_t join_variant_echo_ops[] = {
        0x20,
        0x02,
        0x20,
        0x00,
        0x36,
        0x02,
        0x00,
        0x20,
        0x02,
        0x20,
        0x01,
        0x37,
        0x03,
        0x08,
        0x0B,
    };
    static const uint8_t fixed_list_spill_echo_ops[] = {
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x00,
        0x36,
        0x02,
        0x00,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x04,
        0x36,
        0x02,
        0x04,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x08,
        0x36,
        0x02,
        0x08,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x0C,
        0x36,
        0x02,
        0x0C,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x10,
        0x36,
        0x02,
        0x10,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x14,
        0x36,
        0x02,
        0x14,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x18,
        0x36,
        0x02,
        0x18,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x1C,
        0x36,
        0x02,
        0x1C,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x20,
        0x36,
        0x02,
        0x20,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x24,
        0x36,
        0x02,
        0x24,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x28,
        0x36,
        0x02,
        0x28,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x2C,
        0x36,
        0x02,
        0x2C,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x30,
        0x36,
        0x02,
        0x30,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x34,
        0x36,
        0x02,
        0x34,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x38,
        0x36,
        0x02,
        0x38,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x3C,
        0x36,
        0x02,
        0x3C,
        0x20,
        0x01,
        0x20,
        0x00,
        0x28,
        0x02,
        0x40,
        0x36,
        0x02,
        0x40,
        0x0B,
    };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 7u);
    wasi_test_emit_functype(&sec, i32_params4, 4u, i32_result, 1u);
    wasi_test_emit_functype(&sec, i32_params6, 6u, NULL, 0u);
    wasi_test_emit_functype(&sec, i32_params3, 3u, NULL, 0u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_functype(&sec, i32_params4, 4u, NULL, 0u);
    wasi_test_emit_functype(&sec, join_variant_params, 3u, NULL, 0u);
    wasi_test_emit_functype(&sec, i32_params2, 2u, NULL, 0u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 10u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_leb128_u32(&sec, 5u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_leb128_u32(&sec, 6u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(mod, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 5u);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x20);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit_section(mod, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 16u);
    wasi_test_emit_export(&sec, "memory", 0x02u, 0u);
    wasi_test_emit_export(&sec, "cabi_realloc", 0x00u, 0u);
    wasi_test_emit_export(&sec, "record_echo", 0x00u, 1u);
    wasi_test_emit_export(&sec, "tuple_echo", 0x00u, 1u);
    wasi_test_emit_export(&sec, "flags_echo", 0x00u, 2u);
    wasi_test_emit_export(&sec, "spill_sum", 0x00u, 3u);
    wasi_test_emit_export(&sec, "option_echo", 0x00u, 4u);
    wasi_test_emit_export(&sec, "result_echo", 0x00u, 5u);
    wasi_test_emit_export(&sec, "variant_echo", 0x00u, 6u);
    wasi_test_emit_export(&sec, "variant_join_echo", 0x00u, 7u);
    wasi_test_emit_export(&sec, "fixed_list_flat_echo", 0x00u, 8u);
    wasi_test_emit_export(&sec, "fixed_list_spill_echo", 0x00u, 9u);
    wasi_test_emit_export(&sec, "last_realloc_old_ptr", 0x03u, 1u);
    wasi_test_emit_export(&sec, "last_realloc_old_size", 0x03u, 2u);
    wasi_test_emit_export(&sec, "last_realloc_align", 0x03u, 3u);
    wasi_test_emit_export(&sec, "last_realloc_new_size", 0x03u, 4u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 10u);
    wasi_test_emit_code_body(&sec, cabi_realloc_locals, (uint32_t)sizeof(cabi_realloc_locals), cabi_realloc_ops, (uint32_t)sizeof(cabi_realloc_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, record_echo_ops, (uint32_t)sizeof(record_echo_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, flags_echo_ops, (uint32_t)sizeof(flags_echo_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, spill_sum_ops, (uint32_t)sizeof(spill_sum_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, tagged_echo_ops, (uint32_t)sizeof(tagged_echo_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, tagged_echo_ops, (uint32_t)sizeof(tagged_echo_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, tagged_echo_ops, (uint32_t)sizeof(tagged_echo_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, join_variant_echo_ops, (uint32_t)sizeof(join_variant_echo_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, id_ops, (uint32_t)sizeof(id_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, fixed_list_spill_echo_ops, (uint32_t)sizeof(fixed_list_spill_echo_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_enum_validation_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 3u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "e");
    wasi_test_emit_enum2_valtype(&sec);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_enum2_valtype(&sec);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_enum2_valtype(&sec);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_enum_record_valtype(&sec);

    wasi_test_emit_section(component, 7u, sec.buf, sec.len);
}

static void wasi_test_build_enum_validation_core_module(wasi_test_builder_t* mod) {
    wasi_test_builder_t sec = { 0 };
    static const uint8_t i32_params4[] = { 0x7F, 0x7F, 0x7F, 0x7F };
    static const uint8_t i32_result[] = { 0x7F };
    static const uint8_t i32_param[] = { 0x7F };
    static const uint8_t empty[] = { 0x00 };
    static const uint8_t cabi_realloc_locals[] = { 0x01, 0x01, 0x7F };
    static const uint8_t cabi_realloc_ops[] = {
        0x23,
        0x00,
        0x21,
        0x04,
        0x23,
        0x00,
        0x20,
        0x03,
        0x6A,
        0x24,
        0x00,
        0x20,
        0x04,
        0x0B,
    };
    static const uint8_t id_ops[] = { 0x20, 0x00, 0x0B };
    static const uint8_t const2_ops[] = { 0x41, 0x02, 0x0B };
    static const uint8_t bad_enum_record_ops[] = {
        0x20,
        0x00,
        0x41,
        0x02,
        0x36,
        0x02,
        0x00,
        0x20,
        0x00,
        0x41,
        0x07,
        0x36,
        0x02,
        0x04,
        0x0B,
    };

    memset(mod, 0, sizeof(*mod));
    wasi_test_emit_header(mod);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_functype(&sec, i32_params4, 4u, i32_result, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, i32_result, 1u);
    wasi_test_emit_functype(&sec, NULL, 0u, i32_result, 1u);
    wasi_test_emit_functype(&sec, i32_param, 1u, NULL, 0u);
    wasi_test_emit_section(mod, 1u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_leb128_u32(&sec, 3u);
    wasi_test_emit_section(mod, 3u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_section(mod, 5u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x7F);
    wasi_test_emit(&sec, 0x01);
    wasi_test_emit(&sec, 0x41);
    wasi_test_emit(&sec, 0x20);
    wasi_test_emit(&sec, 0x0B);
    wasi_test_emit_section(mod, 6u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 5u);
    wasi_test_emit_export(&sec, "memory", 0x02u, 0u);
    wasi_test_emit_export(&sec, "cabi_realloc", 0x00u, 0u);
    wasi_test_emit_export(&sec, "id32", 0x00u, 1u);
    wasi_test_emit_export(&sec, "const2", 0x00u, 2u);
    wasi_test_emit_export(&sec, "bad_enum_record", 0x00u, 3u);
    wasi_test_emit_section(mod, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 4u);
    wasi_test_emit_code_body(&sec, cabi_realloc_locals, (uint32_t)sizeof(cabi_realloc_locals), cabi_realloc_ops, (uint32_t)sizeof(cabi_realloc_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, id_ops, (uint32_t)sizeof(id_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, const2_ops, (uint32_t)sizeof(const2_ops));
    wasi_test_emit_code_body(&sec, empty, 1u, bad_enum_record_ops, (uint32_t)sizeof(bad_enum_record_ops));
    wasi_test_emit_section(mod, 10u, sec.buf, sec.len);
}

static void wasi_test_build_string_lift_component(wasi_test_builder_t* component,
                                                  const wasi_test_builder_t* core_module,
                                                  int string_encoding) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 's');
    wasi_test_emit(&sec, 0x73u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x73u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_leb128_u32(&sec, string_encoding ? 4u : 3u);
    if (string_encoding == 1) wasi_test_emit(&sec, 0x01u);
    if (string_encoding == 2) wasi_test_emit(&sec, 0x02u);
    wasi_test_emit(&sec, 0x03u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x04u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x05u);
    wasi_test_emit_leb128_u32(&sec, 6u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "echo", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_bad_bool_lift_component(wasi_test_builder_t* component,
                                                    const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x7Fu);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 7u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_export(&sec, "const2", 0x01u, 0u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

static void wasi_test_build_fixed_list_type_component(wasi_test_builder_t* component) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_fixed_list_u32_valtype(&sec, 4u);
    wasi_test_emit_section(component, 7u, sec.buf, sec.len);
}

static void wasi_test_build_fixed_list_lift_component(wasi_test_builder_t* component,
                                                      const wasi_test_builder_t* core_module) {
    wasi_test_builder_t sec = { 0 };

    memset(component, 0, sizeof(*component));
    wasi_test_emit_component_header(component);

    wasi_test_emit_section(component, 1u, core_module->buf, core_module->len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "v");
    wasi_test_emit_fixed_list_u32_valtype(&sec, 1u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_fixed_list_u32_valtype(&sec, 1u);

    wasi_test_emit(&sec, 0x40u);
    wasi_test_emit_leb128_u32(&sec, 1u);
    wasi_test_emit_component_plain_name(&sec, "v");
    wasi_test_emit_fixed_list_u32_valtype(&sec, 17u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_fixed_list_u32_valtype(&sec, 17u);

    wasi_test_emit_section(component, 7u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 8u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 0u);

    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit(&sec, 0x00u);
    wasi_test_emit_leb128_u32(&sec, 9u);
    wasi_test_emit_leb128_u32(&sec, 0u);
    wasi_test_emit_leb128_u32(&sec, 1u);

    wasi_test_emit_section(component, 8u, sec.buf, sec.len);

    memset(&sec, 0, sizeof(sec));
    wasi_test_emit_leb128_u32(&sec, 2u);
    wasi_test_emit_component_export(&sec, "flat_echo", 0x01u, 0u);
    wasi_test_emit_component_export(&sec, "spill_echo", 0x01u, 1u);
    wasi_test_emit_section(component, 11u, sec.buf, sec.len);
}

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
    const wasi_component_valtype_t* type;
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
    WL_CHECK(t, strstr(dump, "type[4]: kind=defined detail=record fields=1") != NULL);
    WL_CHECK(t, strstr(dump, "type[8]: kind=defined detail=result ok=string err=u32") != NULL);
    WL_CHECK(t, strstr(dump, "type[9]: kind=defined detail=future item=string") != NULL);
    WL_CHECK(t, strstr(dump, "type[10]: kind=defined detail=stream item=u8") != NULL);

    type = wasi_component_type_defined_valtype(component, 4);
    WL_REQUIRE(t, type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(type) == 0x72u);
    WL_CHECK(t, wasi_component_valtype_item_count(type) == 1u);
    WL_CHECK(t, strcmp(wasi_component_valtype_item_name(type, 0), "x") == 0);
    WL_CHECK(t, wasi_component_valtype_item_has_type(type, 0));
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_item_type(type, 0)) == 0x79u);

    type = wasi_component_type_defined_valtype(component, 5);
    WL_REQUIRE(t, type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(type) == 0x71u);
    WL_CHECK(t, wasi_component_valtype_item_count(type) == 1u);
    WL_CHECK(t, strcmp(wasi_component_valtype_item_name(type, 0), "ok") == 0);
    WL_CHECK(t, wasi_component_valtype_item_has_type(type, 0));
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_item_type(type, 0)) == 0x73u);

    type = wasi_component_type_defined_valtype(component, 6);
    WL_REQUIRE(t, type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(type) == 0x70u);
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_element_type(type)) == 0x7Du);

    type = wasi_component_type_defined_valtype(component, 7);
    WL_REQUIRE(t, type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(type) == 0x6Bu);
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_element_type(type)) == 0x73u);

    type = wasi_component_type_defined_valtype(component, 8);
    WL_REQUIRE(t, type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(type) == 0x6Au);
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_ok_type(type)) == 0x73u);
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_err_type(type)) == 0x79u);

    type = wasi_component_type_defined_valtype(component, 9);
    WL_REQUIRE(t, type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(type) == 0x65u);
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_element_type(type)) == 0x73u);

    type = wasi_component_type_defined_valtype(component, 10);
    WL_REQUIRE(t, type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(type) == 0x66u);
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_element_type(type)) == 0x7Du);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_fixed_list_defined_types) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasi_test_builder_t component_bytes;
    const wasi_component_valtype_t* type;
    char dump[256];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_fixed_list_type_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_type_count(component) == 1u);
    WL_CHECK(t, wasi_component_type_kind(component, 0u) == WASI_COMPONENT_TYPE_KIND_DEFINED);

    type = wasi_component_type_defined_valtype(component, 0u);
    WL_REQUIRE(t, type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(type) == 0x67u);
    WL_CHECK(t, wasi_component_valtype_fixed_length(type) == 4u);
    WL_REQUIRE(t, wasi_component_valtype_element_type(type) != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_element_type(type)) == 0x79u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "fixed-list") != NULL);
    WL_CHECK(t, strstr(dump, "len=4") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_nested_core_types_in_type_space) {
    wasi_engine_t engine;
    wasi_component_t* component;
    const wasi_component_type_decl_t* decl;
    const wasi_component_core_module_decl_t* module_decl;
    char dump[768];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_nested_core_types,
                          sizeof(wasi_test_component_with_nested_core_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_type_count(component) == 2u);
    WL_CHECK(t, wasi_component_type_kind(component, 0) == WASI_COMPONENT_TYPE_KIND_COMPONENT);
    WL_CHECK(t, wasi_component_type_kind(component, 1) == WASI_COMPONENT_TYPE_KIND_INSTANCE);
    WL_CHECK(t, wasi_component_type_decl_count(component, 0) == 1u);
    WL_CHECK(t, wasi_component_type_decl_count(component, 1) == 1u);

    decl = wasi_component_type_decl_at(component, 0, 0);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->kind == WASI_COMPONENT_TYPE_DECL_KIND_CORE_TYPE);
    WL_CHECK(t, decl->data.core_type.kind == WASI_COMPONENT_CORE_TYPE_KIND_MODULE);
    WL_CHECK(t, decl->data.core_type.item_count == 4u);
    WL_CHECK(t, decl->data.core_type.data.module.num_decls == 4u);

    module_decl = &decl->data.core_type.data.module.decls[0];
    WL_CHECK(t, module_decl->kind == WASI_COMPONENT_CORE_MODULE_DECL_KIND_TYPE);
    WL_REQUIRE(t, module_decl->data.type != NULL);
    WL_CHECK(t, module_decl->data.type->detail_opcode == 0x60u);
    WL_CHECK(t, module_decl->data.type->data.func.num_params == 1u);
    WL_CHECK(t, module_decl->data.type->data.func.num_results == 1u);

    module_decl = &decl->data.core_type.data.module.decls[1];
    WL_CHECK(t, module_decl->kind == WASI_COMPONENT_CORE_MODULE_DECL_KIND_IMPORT);
    WL_CHECK(t, strcmp(module_decl->module_name, "m") == 0);
    WL_CHECK(t, strcmp(module_decl->name, "f") == 0);
    WL_CHECK(t, module_decl->data.externtype.kind == WASM_EXPORT_FUNC);
    WL_CHECK(t, module_decl->data.externtype.of.func_type_index == 0u);

    module_decl = &decl->data.core_type.data.module.decls[2];
    WL_CHECK(t, module_decl->kind == WASI_COMPONENT_CORE_MODULE_DECL_KIND_ALIAS);
    WL_CHECK(t, module_decl->data.alias.sort_code == 0x10u);
    WL_CHECK(t, module_decl->data.alias.outer_count == 0u);
    WL_CHECK(t, module_decl->data.alias.outer_index == 0u);

    module_decl = &decl->data.core_type.data.module.decls[3];
    WL_CHECK(t, module_decl->kind == WASI_COMPONENT_CORE_MODULE_DECL_KIND_EXPORT);
    WL_CHECK(t, strcmp(module_decl->name, "g") == 0);
    WL_CHECK(t, module_decl->data.externtype.kind == WASM_EXPORT_FUNC);
    WL_CHECK(t, module_decl->data.externtype.of.func_type_index == 0u);

    decl = wasi_component_type_decl_at(component, 1, 0);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->kind == WASI_COMPONENT_TYPE_DECL_KIND_CORE_TYPE);
    WL_CHECK(t, decl->data.core_type.kind == WASI_COMPONENT_CORE_TYPE_KIND_MODULE);
    WL_CHECK(t, decl->data.core_type.item_count == 0u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "type[0]: kind=component decls=1") != NULL);
    WL_CHECK(t, strstr(dump, "type[1]: kind=instance decls=1") != NULL);
    WL_CHECK(t, strstr(dump, "type-decl[0,0]: kind=core-type core-kind=module detail=module decls=4") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_retains_component_type_declarations) {
    wasi_engine_t engine;
    wasi_component_t* component;
    const wasi_component_type_decl_t* decl;
    char dump[1024];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_type_declarations,
                          sizeof(wasi_test_component_with_type_declarations));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_type_count(component) == 2u);
    WL_CHECK(t, wasi_component_type_decl_count(component, 0) == 4u);
    WL_CHECK(t, wasi_component_type_decl_count(component, 1) == 2u);

    decl = wasi_component_type_decl_at(component, 0, 0);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->kind == WASI_COMPONENT_TYPE_DECL_KIND_TYPE);
    WL_CHECK(t, decl->data.type.kind == WASI_COMPONENT_TYPE_KIND_FUNC);

    decl = wasi_component_type_decl_at(component, 0, 1);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->kind == WASI_COMPONENT_TYPE_DECL_KIND_ALIAS);
    WL_CHECK(t, decl->data.alias.kind == WASI_COMPONENT_ALIAS_KIND_OUTER);
    WL_CHECK(t, !decl->data.alias.sort_is_core);
    WL_CHECK(t, decl->data.alias.sort_code == 0x03u);
    WL_CHECK(t, decl->data.alias.extern_kind == WASI_COMPONENT_EXTERN_KIND_TYPE);
    WL_CHECK(t, decl->data.alias.outer_count == 1u);
    WL_CHECK(t, decl->data.alias.outer_index == 0u);

    decl = wasi_component_type_decl_at(component, 0, 2);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->kind == WASI_COMPONENT_TYPE_DECL_KIND_IMPORT);
    WL_CHECK(t, strcmp(decl->name, "cfg") == 0);
    WL_CHECK(t, decl->data.externdesc.kind == WASI_COMPONENT_EXTERN_KIND_VALUE);
    WL_CHECK(t, decl->data.externdesc.bound_tag == 0x01u);
    WL_REQUIRE(t, decl->data.externdesc.value_type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(decl->data.externdesc.value_type) == 0x73u);

    decl = wasi_component_type_decl_at(component, 0, 3);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->kind == WASI_COMPONENT_TYPE_DECL_KIND_EXPORT);
    WL_CHECK(t, strcmp(decl->name, "pkg:iface/run@0.3.0") == 0);
    WL_CHECK(t, strcmp(decl->interface_name, "pkg:iface/run") == 0);
    WL_CHECK(t, strcmp(decl->interface_version, "0.3.0") == 0);
    WL_CHECK(t, decl->data.externdesc.kind == WASI_COMPONENT_EXTERN_KIND_FUNC);
    WL_CHECK(t, decl->data.externdesc.has_type_index);
    WL_CHECK(t, decl->data.externdesc.type_index == 0u);

    decl = wasi_component_type_decl_at(component, 1, 0);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->kind == WASI_COMPONENT_TYPE_DECL_KIND_TYPE);
    WL_CHECK(t, decl->data.type.kind == WASI_COMPONENT_TYPE_KIND_DEFINED);
    WL_REQUIRE(t, decl->data.type.defined_type != NULL);
    WL_CHECK(t, wasi_component_valtype_opcode(decl->data.type.defined_type) == 0x70u);
    WL_CHECK(t, wasi_component_valtype_opcode(wasi_component_valtype_element_type(decl->data.type.defined_type)) == 0x7Du);

    decl = wasi_component_type_decl_at(component, 1, 1);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->kind == WASI_COMPONENT_TYPE_DECL_KIND_EXPORT);
    WL_CHECK(t, strcmp(decl->name, "cfg") == 0);
    WL_CHECK(t, decl->data.externdesc.kind == WASI_COMPONENT_EXTERN_KIND_VALUE);
    WL_CHECK(t, decl->data.externdesc.bound_tag == 0x00u);
    WL_CHECK(t, decl->data.externdesc.has_type_index);
    WL_CHECK(t, decl->data.externdesc.type_index == 0u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "type-decl[0,2]: kind=import name=cfg desc=value value=string bound=0x01") != NULL);
    WL_CHECK(t, strstr(dump, "type-decl[0,3]: kind=export name=pkg:iface/run@0.3.0 desc=func type=0") != NULL);
    WL_CHECK(t, strstr(dump, "type-decl[1,1]: kind=export name=cfg desc=value type=0 bound=0x00") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_parses_top_level_core_type_sections) {
    wasi_engine_t engine;
    wasi_component_t* component;
    const wasi_component_core_type_t* core_type;
    const wasi_component_core_module_decl_t* module_decl;
    char dump[768];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_top_level_core_types,
                          sizeof(wasi_test_component_with_top_level_core_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_core_type_count(component) == 3u);
    WL_CHECK(t, wasi_component_core_type_kind(component, 0) == WASI_COMPONENT_CORE_TYPE_KIND_TYPE);
    WL_CHECK(t, wasi_component_core_type_detail_opcode(component, 0) == 0x60u);
    WL_CHECK(t, wasi_component_core_type_kind(component, 1) == WASI_COMPONENT_CORE_TYPE_KIND_MODULE);
    WL_CHECK(t, wasi_component_core_type_item_count(component, 1) == 2u);
    WL_CHECK(t, wasi_component_core_type_kind(component, 2) == WASI_COMPONENT_CORE_TYPE_KIND_TYPE);
    WL_CHECK(t, wasi_component_core_type_detail_opcode(component, 2) == 0x5Du);
    WL_CHECK(t, wasi_component_core_type_has_primary_index(component, 2));
    WL_CHECK(t, wasi_component_core_type_primary_index(component, 2) == 1u);

    core_type = wasi_component_core_type_at(component, 0);
    WL_REQUIRE(t, core_type != NULL);
    WL_CHECK(t, core_type->data.func.num_params == 0u);
    WL_CHECK(t, core_type->data.func.num_results == 0u);

    core_type = wasi_component_core_type_at(component, 1);
    WL_REQUIRE(t, core_type != NULL);
    WL_CHECK(t, core_type->data.module.num_decls == 2u);
    module_decl = &core_type->data.module.decls[0];
    WL_CHECK(t, module_decl->kind == WASI_COMPONENT_CORE_MODULE_DECL_KIND_TYPE);
    WL_REQUIRE(t, module_decl->data.type != NULL);
    WL_CHECK(t, module_decl->data.type->detail_opcode == 0x60u);
    WL_CHECK(t, module_decl->data.type->data.func.num_params == 1u);
    WL_CHECK(t, module_decl->data.type->data.func.num_results == 1u);
    module_decl = &core_type->data.module.decls[1];
    WL_CHECK(t, module_decl->kind == WASI_COMPONENT_CORE_MODULE_DECL_KIND_EXPORT);
    WL_CHECK(t, strcmp(module_decl->name, "g") == 0);
    WL_CHECK(t, module_decl->data.externtype.kind == WASM_EXPORT_FUNC);
    WL_CHECK(t, module_decl->data.externtype.of.func_type_index == 0u);

    core_type = wasi_component_core_type_at(component, 2);
    WL_REQUIRE(t, core_type != NULL);
    WL_CHECK(t, core_type->data.cont.type_index == 1u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "core-types=3") != NULL);
    WL_CHECK(t, strstr(dump, "core-type[0]: kind=type detail=func params=0 results=0") != NULL);
    WL_CHECK(t, strstr(dump, "core-type[1]: kind=module detail=module decls=2") != NULL);
    WL_CHECK(t, strstr(dump, "core-module-decl[1,1]: kind=export name=g desc=func type=0") != NULL);
    WL_CHECK(t, strstr(dump, "core-type[2]: kind=type detail=cont index=1") != NULL);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_retains_core_type_payloads) {
    wasi_engine_t engine;
    wasi_component_t* component;
    const wasi_component_core_type_t* core_type;
    char dump[768];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_core_type_payloads,
                          sizeof(wasi_test_component_with_core_type_payloads));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    WL_CHECK(t, wasi_component_core_type_count(component) == 2u);

    core_type = wasi_component_core_type_at(component, 0);
    WL_REQUIRE(t, core_type != NULL);
    WL_CHECK(t, core_type->opcode == 0x4Eu);
    WL_CHECK(t, core_type->item_count == 2u);
    WL_CHECK(t, core_type->data.rec_group.num_entries == 2u);
    WL_REQUIRE(t, core_type->data.rec_group.entries[0] != NULL);
    WL_CHECK(t, core_type->data.rec_group.entries[0]->detail_opcode == 0x5Fu);
    WL_CHECK(t, core_type->data.rec_group.entries[0]->data.struct_.num_fields == 1u);
    WL_CHECK(t, core_type->data.rec_group.entries[0]->data.struct_.fields[0].storage.kind == WASM_STORAGE_PACKED);
    WL_CHECK(t, core_type->data.rec_group.entries[0]->data.struct_.fields[0].storage.of.packed_type == WASM_PACKED_I8);
    WL_CHECK(t, core_type->data.rec_group.entries[0]->data.struct_.fields[0].is_mutable == 1);
    WL_REQUIRE(t, core_type->data.rec_group.entries[1] != NULL);
    WL_CHECK(t, core_type->data.rec_group.entries[1]->detail_opcode == 0x5Eu);
    WL_CHECK(t, core_type->data.rec_group.entries[1]->data.array.field.storage.kind == WASM_STORAGE_VALTYPE);
    WL_CHECK(t, core_type->data.rec_group.entries[1]->data.array.field.storage.of.valtype == WASM_TYPE_I64);
    WL_CHECK(t, core_type->data.rec_group.entries[1]->data.array.field.is_mutable == 0);

    core_type = wasi_component_core_type_at(component, 1);
    WL_REQUIRE(t, core_type != NULL);
    WL_CHECK(t, core_type->opcode == 0x4Fu);
    WL_CHECK(t, core_type->is_final);
    WL_CHECK(t, core_type->num_supertypes == 1u);
    WL_CHECK(t, core_type->supertypes[0] == 0u);
    WL_CHECK(t, core_type->detail_opcode == 0x60u);
    WL_CHECK(t, core_type->data.func.num_params == 0u);
    WL_CHECK(t, core_type->data.func.num_results == 0u);

    wasi_dump_component(component, dump, sizeof(dump));
    WL_CHECK(t, strstr(dump, "core-type[0]: kind=type detail=array group=2") != NULL);
    WL_CHECK(t, strstr(dump, "core-type[1]: kind=type detail=func supertypes=1 final=1 params=0 results=0") != NULL);

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

WL_TEST(test_wasi_tracks_component_source_offsets) {
    wasi_engine_t engine;
    wasi_component_t* component;
    const wasi_component_type_decl_t* decl;
    const wasi_component_core_type_t* core_type;
    const wasi_component_core_module_decl_t* module_decl;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_import_export,
                          sizeof(wasi_test_component_with_import_export));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_section_offset(component, 0) == 8u);
    WL_CHECK(t, wasi_component_section_payload_offset(component, 0) == 10u);
    WL_CHECK(t, wasi_component_section_offset(component, 1) == 15u);
    WL_CHECK(t, wasi_component_section_payload_offset(component, 1) == 17u);
    WL_CHECK(t, wasi_component_section_offset(component, 2) == 30u);
    WL_CHECK(t, wasi_component_section_payload_offset(component, 2) == 32u);
    WL_CHECK(t, wasi_component_type_offset(component, 0) == 11u);
    WL_CHECK(t, wasi_component_import_offset(component, 0) == 18u);
    WL_CHECK(t, wasi_component_export_offset(component, 0) == 33u);
    wasi_free_component(component);

    component = wasi_load(&engine,
                          wasi_test_component_with_type_declarations,
                          sizeof(wasi_test_component_with_type_declarations));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_type_offset(component, 0) == 11u);
    WL_CHECK(t, wasi_component_type_offset(component, 1) == 56u);

    decl = wasi_component_type_decl_at(component, 0, 0);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->offset == 13u);
    WL_CHECK(t, decl->data.type.offset == 14u);

    decl = wasi_component_type_decl_at(component, 0, 1);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->offset == 18u);
    WL_CHECK(t, decl->data.alias.offset == 19u);

    decl = wasi_component_type_decl_at(component, 0, 2);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->offset == 23u);

    decl = wasi_component_type_decl_at(component, 0, 3);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->offset == 32u);

    decl = wasi_component_type_decl_at(component, 1, 0);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->offset == 58u);
    WL_CHECK(t, decl->data.type.offset == 59u);

    decl = wasi_component_type_decl_at(component, 1, 1);
    WL_REQUIRE(t, decl != NULL);
    WL_CHECK(t, decl->offset == 61u);
    wasi_free_component(component);

    component = wasi_load(&engine,
                          wasi_test_component_with_top_level_core_types,
                          sizeof(wasi_test_component_with_top_level_core_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_type_offset(component, 0) == 11u);
    WL_CHECK(t, wasi_component_core_type_offset(component, 1) == 14u);
    WL_CHECK(t, wasi_component_core_type_offset(component, 2) == 27u);
    core_type = wasi_component_core_type_at(component, 1);
    WL_REQUIRE(t, core_type != NULL);
    module_decl = &core_type->data.module.decls[0];
    WL_CHECK(t, module_decl->offset == 16u);
    module_decl = &core_type->data.module.decls[1];
    WL_CHECK(t, module_decl->offset == 22u);
    wasi_free_component(component);

    component = wasi_load(&engine,
                          wasi_test_component_with_core_instances,
                          sizeof(wasi_test_component_with_core_instances));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_offset(component, 0) == 11u);
    WL_CHECK(t, wasi_component_core_instance_offset(component, 1) == 19u);
    wasi_free_component(component);

    component = wasi_load(&engine,
                          wasi_test_component_with_alias_variants,
                          sizeof(wasi_test_component_with_alias_variants));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_alias_offset(component, 0) == 50u);
    wasi_free_component(component);

    component = wasi_load(&engine,
                          wasi_test_component_with_start,
                          sizeof(wasi_test_component_with_start));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_start_offset(component) == 27u);
    wasi_free_component(component);

    component = wasi_load(&engine,
                          wasi_test_component_with_canon_builtins,
                          sizeof(wasi_test_component_with_canon_builtins));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_canon_offset(component, 0) == 20u);
    WL_CHECK(t, wasi_component_canon_offset(component, 7) == 32u);
    wasi_free_component(component);

    component = wasi_load(&engine,
                          wasi_test_component_with_core_module,
                          sizeof(wasi_test_component_with_core_module));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_module_offset(component, 0) == 10u);
    WL_CHECK(t, wasi_component_core_module_size(component, 0) == 8u);
    wasi_free_component(component);

    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_scalars) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t module_bytes;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_scalar_func_types,
                          sizeof(wasi_test_component_with_scalar_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_scalar_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_BOOL;
    args[0].of.boolean = 1u;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_BOOL, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call bool failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_BOOL);
    WL_CHECK(t, results[0].of.boolean == 1u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.boolean = 0u;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_BOOL, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call bool false failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_BOOL);
    WL_CHECK(t, results[0].of.boolean == 0u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_S8;
    args[0].of.s8 = INT8_MIN;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_S8, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s8 min failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S8);
    WL_CHECK(t, results[0].of.s8 == INT8_MIN);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.s8 = INT8_MAX;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_S8, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s8 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S8);
    WL_CHECK(t, results[0].of.s8 == INT8_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U8;
    args[0].of.u8 = 0u;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_U8, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u8 zero failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U8);
    WL_CHECK(t, results[0].of.u8 == 0u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.u8 = UINT8_MAX;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_U8, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u8 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U8);
    WL_CHECK(t, results[0].of.u8 == UINT8_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_S16;
    args[0].of.s16 = INT16_MIN;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_S16, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s16 min failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S16);
    WL_CHECK(t, results[0].of.s16 == INT16_MIN);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.s16 = INT16_MAX;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_S16, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s16 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S16);
    WL_CHECK(t, results[0].of.s16 == INT16_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U16;
    args[0].of.u16 = 0u;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_U16, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u16 zero failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U16);
    WL_CHECK(t, results[0].of.u16 == 0u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.u16 = UINT16_MAX;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_U16, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u16 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U16);
    WL_CHECK(t, results[0].of.u16 == UINT16_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_S32;
    args[0].of.s32 = INT32_MIN;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_S32, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s32 min failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S32);
    WL_CHECK(t, results[0].of.s32 == INT32_MIN);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.s32 = INT32_MAX;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_S32, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s32 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S32);
    WL_CHECK(t, results[0].of.s32 == INT32_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 0u;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_U32, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u32 zero failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 0u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.u32 = UINT32_MAX;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_U32, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u32 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == UINT32_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_S64;
    args[0].of.s64 = INT64_MIN;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_S64, core_module, "id64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s64 min failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S64);
    WL_CHECK(t, results[0].of.s64 == INT64_MIN);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.s64 = INT64_MAX;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_S64, core_module, "id64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s64 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S64);
    WL_CHECK(t, results[0].of.s64 == INT64_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U64;
    args[0].of.u64 = UINT64_MAX;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_U64, core_module, "id64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u64 failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U64);
    WL_CHECK(t, results[0].of.u64 == UINT64_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_F32;
    args[0].of.f32 = 1.25f;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_F32, core_module, "idf32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call f32 failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_F32);
    WL_CHECK(t, results[0].of.f32 == 1.25f);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.f32 = NAN;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_F32, core_module, "idf32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call f32 nan failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_F32);
    WL_CHECK(t, isnan(results[0].of.f32));
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_F64;
    args[0].of.f64 = 2.5;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_F64, core_module, "idf64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call f64 failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_F64);
    WL_CHECK(t, results[0].of.f64 == 2.5);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.f64 = NAN;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_F64, core_module, "idf64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call f64 nan failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_F64);
    WL_CHECK(t, isnan(results[0].of.f64));
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_CHAR;
    args[0].of.char32 = 0x0000u;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call char zero failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_CHAR);
    WL_CHECK(t, results[0].of.char32 == 0x0000u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.char32 = 0xD7FFu;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call char d7ff failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_CHAR);
    WL_CHECK(t, results[0].of.char32 == 0xD7FFu);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.char32 = 0xE000u;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call char e000 failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_CHAR);
    WL_CHECK(t, results[0].of.char32 == 0xE000u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.char32 = 0x10FFFFu;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call char max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_CHAR);
    WL_CHECK(t, results[0].of.char32 == 0x10FFFFu);
    wasi_value_destroy(&results[0]);

    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_utf8_strings) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t module_bytes;
    wasi_canon_options_t options;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasm_value_t global_value;
    char buffer[8];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_scalar_func_types,
                          sizeof(wasi_test_component_with_scalar_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_scalar_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    err = wasi_canon_options_default(&options);
    WL_REQUIRE(t, err == WASI_OK);
    options.post_return_name = "cabi_post_echo";

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_STRING;
    args[0].of.string.data = (char*)"hello";
    args[0].of.string.len = 5u;
    args[0].of.string.owned = 0;

    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_STRING, core_module, "echo", &options, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call string failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 1, 5);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, results[0].of.string.len == 5u);
    WL_CHECK(t, memcmp(results[0].of.string.data, "hello", 5u) == 0);

    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_len", &global_value) == WASM_OK);
    WL_CHECK(t, global_value.of.i32 == 5);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_ptr", &global_value) == WASM_OK);
    WL_REQUIRE(t, wasm_memory_read(core_module, 0u, (uint32_t)global_value.of.i32, buffer, 5u) == WASM_OK);
    WL_CHECK(t, memcmp(buffer, "hello", 5u) == 0);

    wasi_value_destroy(&results[0]);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_utf16_strings) {
    static const char smile[] = "\xF0\x9F\x99\x82";
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t module_bytes;
    wasi_canon_options_t options;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasm_value_t global_value;
    uint8_t utf16_bytes[4];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_scalar_func_types,
                          sizeof(wasi_test_component_with_scalar_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_scalar_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    err = wasi_canon_options_default(&options);
    WL_REQUIRE(t, err == WASI_OK);
    options.string_encoding = WASI_STRING_ENCODING_UTF16;
    options.post_return_name = "cabi_post_echo";

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_STRING;
    args[0].of.string.data = (char*)smile;
    args[0].of.string.len = sizeof(smile) - 1u;
    args[0].of.string.owned = 0;

    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_STRING, core_module, "echo", &options, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call utf16 string failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 2, 4);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, results[0].of.string.len == sizeof(smile) - 1u);
    WL_CHECK(t, memcmp(results[0].of.string.data, smile, sizeof(smile) - 1u) == 0);

    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_len", &global_value) == WASM_OK);
    WL_CHECK(t, global_value.of.i32 == 2);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_ptr", &global_value) == WASM_OK);
    WL_REQUIRE(t, wasm_memory_read(core_module, 0u, (uint32_t)global_value.of.i32, utf16_bytes, sizeof(utf16_bytes)) == WASM_OK);
    WL_CHECK(t, utf16_bytes[0] == 0x3Du);
    WL_CHECK(t, utf16_bytes[1] == 0xD8u);
    WL_CHECK(t, utf16_bytes[2] == 0x42u);
    WL_CHECK(t, utf16_bytes[3] == 0xDEu);

    wasi_value_destroy(&results[0]);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_latin1_utf16_strings) {
    static const char cafe[] = "caf\xC3\xA9";
    static const char smile[] = "\xF0\x9F\x99\x82";
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t module_bytes;
    wasi_canon_options_t options;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasm_value_t global_value;
    uint8_t bytes[4];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_scalar_func_types,
                          sizeof(wasi_test_component_with_scalar_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_scalar_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    err = wasi_canon_options_default(&options);
    WL_REQUIRE(t, err == WASI_OK);
    options.string_encoding = WASI_STRING_ENCODING_LATIN1_UTF16;
    options.post_return_name = "cabi_post_echo";

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_STRING;
    args[0].of.string.data = (char*)cafe;
    args[0].of.string.len = sizeof(cafe) - 1u;
    args[0].of.string.owned = 0;

    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_STRING, core_module, "echo", &options, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call latin1 string failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 2, 4);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, results[0].of.string.len == sizeof(cafe) - 1u);
    WL_CHECK(t, memcmp(results[0].of.string.data, cafe, sizeof(cafe) - 1u) == 0);

    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_len", &global_value) == WASM_OK);
    WL_CHECK(t, (uint32_t)global_value.of.i32 == 4u);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_ptr", &global_value) == WASM_OK);
    WL_REQUIRE(t, wasm_memory_read(core_module, 0u, (uint32_t)global_value.of.i32, bytes, 4u) == WASM_OK);
    WL_CHECK(t, bytes[0] == 'c');
    WL_CHECK(t, bytes[1] == 'a');
    WL_CHECK(t, bytes[2] == 'f');
    WL_CHECK(t, bytes[3] == 0xE9u);

    wasi_value_destroy(&results[0]);
    memset(results, 0, sizeof(results));
    args[0].of.string.data = (char*)smile;
    args[0].of.string.len = sizeof(smile) - 1u;

    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_STRING, core_module, "echo", &options, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call latin1+utf16 fallback failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 2, 4);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, results[0].of.string.len == sizeof(smile) - 1u);
    WL_CHECK(t, memcmp(results[0].of.string.data, smile, sizeof(smile) - 1u) == 0);

    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_len", &global_value) == WASM_OK);
    WL_CHECK(t, (uint32_t)global_value.of.i32 == 0x80000002u);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_ptr", &global_value) == WASM_OK);
    WL_REQUIRE(t, wasm_memory_read(core_module, 0u, (uint32_t)global_value.of.i32, bytes, sizeof(bytes)) == WASM_OK);
    WL_CHECK(t, bytes[0] == 0x3Du);
    WL_CHECK(t, bytes[1] == 0xD8u);
    WL_CHECK(t, bytes[2] == 0x42u);
    WL_CHECK(t, bytes[3] == 0xDEu);

    wasi_value_destroy(&results[0]);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_rejects_invalid_scalar_values) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t module_bytes;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    component = wasi_load(&engine,
                          wasi_test_component_with_scalar_func_types,
                          sizeof(wasi_test_component_with_scalar_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_scalar_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(results, 0, sizeof(results));
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_BAD_BOOL_RESULT, core_module, "const2", NULL, NULL, 0u, results, 1u);
    WL_CHECK(t, err == WASI_ERR_MALFORMED);
    WL_CHECK(t, strstr(engine.error_msg, "bool result") != NULL);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_CHAR;
    args[0].of.char32 = 0xD800u;
    err = wasi_canon_call(component, WASI_TEST_SCALAR_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_CHECK(t, err == WASI_ERR_INVALID_ARGUMENT);
    WL_CHECK(t, strstr(engine.error_msg, "unicode scalar") != NULL);

    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_calls_utf8_canon_lift_exports) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_module_t* core_module;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasm_value_t global_value;
    char buffer[8];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_scalar_core_module(&module_bytes);
    wasi_test_build_string_lift_component(&component_bytes, &module_bytes, 0);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_STRING;
    args[0].of.string.data = (char*)"hello";
    args[0].of.string.len = 5u;
    args[0].of.string.owned = 0;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, results[0].of.string.len == 5u);
    WL_CHECK(t, memcmp(results[0].of.string.data, "hello", 5u) == 0);

    core_module = wasi_component_core_module_at(component, 0u);
    WL_REQUIRE(t, core_module != NULL);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 1, 5);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_len", &global_value) == WASM_OK);
    WL_CHECK(t, global_value.of.i32 == 5);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_ptr", &global_value) == WASM_OK);
    WL_REQUIRE(t, wasm_memory_read(core_module, 0u, (uint32_t)global_value.of.i32, buffer, 5u) == WASM_OK);
    WL_CHECK(t, memcmp(buffer, "hello", 5u) == 0);

    wasi_value_destroy(&results[0]);
    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_calls_utf16_canon_lift_exports) {
    static const char smile[] = "\xF0\x9F\x99\x82";
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_module_t* core_module;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasm_value_t global_value;
    uint8_t utf16_bytes[4];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_scalar_core_module(&module_bytes);
    wasi_test_build_string_lift_component(&component_bytes, &module_bytes, 1);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_STRING;
    args[0].of.string.data = (char*)smile;
    args[0].of.string.len = sizeof(smile) - 1u;
    args[0].of.string.owned = 0;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, results[0].of.string.len == sizeof(smile) - 1u);
    WL_CHECK(t, memcmp(results[0].of.string.data, smile, sizeof(smile) - 1u) == 0);

    core_module = wasi_component_core_module_at(component, 0u);
    WL_REQUIRE(t, core_module != NULL);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 2, 4);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_len", &global_value) == WASM_OK);
    WL_CHECK(t, global_value.of.i32 == 2);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_ptr", &global_value) == WASM_OK);
    WL_REQUIRE(t, wasm_memory_read(core_module, 0u, (uint32_t)global_value.of.i32, utf16_bytes, sizeof(utf16_bytes)) == WASM_OK);
    WL_CHECK(t, utf16_bytes[0] == 0x3Du);
    WL_CHECK(t, utf16_bytes[1] == 0xD8u);
    WL_CHECK(t, utf16_bytes[2] == 0x42u);
    WL_CHECK(t, utf16_bytes[3] == 0xDEu);

    wasi_value_destroy(&results[0]);
    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_calls_latin1_utf16_canon_lift_exports) {
    static const char cafe[] = "caf\xC3\xA9";
    static const char smile[] = "\xF0\x9F\x99\x82";
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_module_t* core_module;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasm_value_t global_value;
    uint8_t bytes[4];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_scalar_core_module(&module_bytes);
    wasi_test_build_string_lift_component(&component_bytes, &module_bytes, 2);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_STRING;
    args[0].of.string.data = (char*)cafe;
    args[0].of.string.len = sizeof(cafe) - 1u;
    args[0].of.string.owned = 0;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call latin1 failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, results[0].of.string.len == sizeof(cafe) - 1u);
    WL_CHECK(t, memcmp(results[0].of.string.data, cafe, sizeof(cafe) - 1u) == 0);

    core_module = wasi_component_core_module_at(component, 0u);
    WL_REQUIRE(t, core_module != NULL);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 2, 4);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_len", &global_value) == WASM_OK);
    WL_CHECK(t, (uint32_t)global_value.of.i32 == 4u);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_ptr", &global_value) == WASM_OK);
    WL_REQUIRE(t, wasm_memory_read(core_module, 0u, (uint32_t)global_value.of.i32, bytes, 4u) == WASM_OK);
    WL_CHECK(t, bytes[0] == 'c');
    WL_CHECK(t, bytes[1] == 'a');
    WL_CHECK(t, bytes[2] == 'f');
    WL_CHECK(t, bytes[3] == 0xE9u);

    wasi_value_destroy(&results[0]);
    memset(results, 0, sizeof(results));
    args[0].of.string.data = (char*)smile;
    args[0].of.string.len = sizeof(smile) - 1u;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call latin1+utf16 fallback failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, results[0].of.string.len == sizeof(smile) - 1u);
    WL_CHECK(t, memcmp(results[0].of.string.data, smile, sizeof(smile) - 1u) == 0);

    wasi_test_expect_realloc_args(t, core_module, 0, 0, 2, 4);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_len", &global_value) == WASM_OK);
    WL_CHECK(t, (uint32_t)global_value.of.i32 == 0x80000002u);
    WL_REQUIRE(t, wasm_global_get(core_module, "last_post_ptr", &global_value) == WASM_OK);
    WL_REQUIRE(t, wasm_memory_read(core_module, 0u, (uint32_t)global_value.of.i32, bytes, sizeof(bytes)) == WASM_OK);
    WL_CHECK(t, bytes[0] == 0x3Du);
    WL_CHECK(t, bytes[1] == 0xD8u);
    WL_CHECK(t, bytes[2] == 0x42u);
    WL_CHECK(t, bytes[3] == 0xDEu);

    wasi_value_destroy(&results[0]);
    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_resources_new_drop_and_reuse_slots) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    uint32_t first_handle = UINT32_MAX;
    uint32_t second_handle = UINT32_MAX;
    int first_rep = 11;
    int second_rep = 22;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_core_module(&module_bytes);
    wasi_test_build_resource_bounce_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);

    err = wasi_instance_bind_resource_type(instance, 0u, resource_type);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_instance_bind_resource_type failed: %s", engine.error_msg);

    err = wasi_resource_new(instance, 0u, &first_rep, &first_handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_new failed: %s", engine.error_msg);
    WL_CHECK(t, first_handle == 0u);

    err = wasi_resource_rep(instance, 0u, first_handle, &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep failed: %s", engine.error_msg);
    WL_CHECK(t, representation == &first_rep);

    err = wasi_resource_drop(instance, 0u, first_handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_drop failed: %s", engine.error_msg);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == &first_rep);

    err = wasi_resource_new(instance, 0u, &second_rep, &second_handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_new second failed: %s", engine.error_msg);
    WL_CHECK(t, second_handle == 0u);

    err = wasi_resource_rep(instance, 0u, second_handle, &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep second failed: %s", engine.error_msg);
    WL_CHECK(t, representation == &second_rep);

    wasi_free_instance(instance);
    WL_CHECK(t, drop_state.drops == 2u);
    WL_CHECK(t, drop_state.last_representation == &second_rep);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_calls_own_resource_canon_lift_exports) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t args[1];
    wasi_value_t results[1];
    uint32_t handle = UINT32_MAX;
    int representation_value = 7;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_core_module(&module_bytes);
    wasi_test_build_resource_bounce_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_resource_new(instance, 0u, &representation_value, &handle) == WASI_OK,
                   "wasi_resource_new failed: %s",
                   engine.error_msg);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_OWN;
    args[0].of.resource.type_index = 0u;
    args[0].of.resource.handle = handle;

    err = wasi_call(instance, "bounce", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_OWN);
    WL_CHECK(t, results[0].of.resource.type_index == 0u);
    WL_CHECK(t, results[0].of.resource.handle == handle);

    err = wasi_resource_rep(instance, 0u, results[0].of.resource.handle, &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep failed: %s", engine.error_msg);
    WL_CHECK(t, representation == &representation_value);
    WL_CHECK(t, drop_state.drops == 0u);

    wasi_value_destroy(&results[0]);
    wasi_free_instance(instance);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == &representation_value);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_resource_own_transfer_rejects_outstanding_borrow) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t args[2];
    wasi_value_t results[1];
    uint32_t handle = UINT32_MAX;
    int representation_value = 99;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_core_module(&module_bytes);
    wasi_test_build_resource_conflict_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_resource_new(instance, 0u, &representation_value, &handle) == WASI_OK,
                   "wasi_resource_new failed: %s",
                   engine.error_msg);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_BORROW;
    args[0].of.resource.type_index = 0u;
    args[0].of.resource.handle = handle;
    args[1].kind = WASI_VALUE_KIND_OWN;
    args[1].of.resource.type_index = 0u;
    args[1].of.resource.handle = handle;

    err = wasi_call(instance, "conflict", args, 2u, results, 1u);
    WL_CHECK(t, err == WASI_ERR_RUNTIME);
    WL_CHECK(t, strstr(engine.error_msg, "outstanding borrow") != NULL);

    err = wasi_resource_rep(instance, 0u, handle, &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep failed after conflict: %s", engine.error_msg);
    WL_CHECK(t, representation == &representation_value);

    err = wasi_resource_drop(instance, 0u, handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_drop failed after conflict: %s", engine.error_msg);
    WL_CHECK(t, drop_state.drops == 1u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_consumes_own_resource_without_returning_it) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t args[1];
    uint32_t handle = UINT32_MAX;
    int representation_value = 55;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_core_module(&module_bytes);
    wasi_test_build_resource_sink_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_resource_new(instance, 0u, &representation_value, &handle) == WASI_OK,
                   "wasi_resource_new failed: %s",
                   engine.error_msg);

    memset(args, 0, sizeof(args));
    args[0].kind = WASI_VALUE_KIND_OWN;
    args[0].of.resource.type_index = 0u;
    args[0].of.resource.handle = handle;

    err = wasi_call(instance, "take", args, 1u, NULL, 0u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call failed: %s", engine.error_msg);
    WL_CHECK(t, drop_state.drops == 0u);

    err = wasi_resource_rep(instance, 0u, handle, &representation);
    WL_CHECK(t, err == WASI_ERR_RUNTIME);
    WL_CHECK(t, strstr(engine.error_msg, "not currently owned by the host") != NULL);

    err = wasi_resource_drop(instance, 0u, handle);
    WL_CHECK(t, err == WASI_ERR_RUNTIME);
    WL_CHECK(t, strstr(engine.error_msg, "not currently owned by the host") != NULL);

    wasi_free_instance(instance);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == &representation_value);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_calls_resource_canon_builtins) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t args[1];
    wasi_value_t results[1];
    uint32_t handle = UINT32_MAX;
    uint32_t representation_value = 1234u;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_core_module(&module_bytes);
    wasi_test_build_resource_builtin_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type failed: %s",
                   engine.error_msg);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = representation_value;

    err = wasi_call(instance, "make", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call make failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_OWN);
    WL_CHECK(t, results[0].of.resource.type_index == 0u);
    handle = results[0].of.resource.handle;

    err = wasi_resource_rep(instance, 0u, handle, &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep failed after make: %s", engine.error_msg);
    WL_CHECK(t, representation == (void*)(uintptr_t)representation_value);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_BORROW;
    args[0].of.resource.type_index = 0u;
    args[0].of.resource.handle = handle;

    err = wasi_call(instance, "peek", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call peek failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == representation_value);

    memset(args, 0, sizeof(args));
    args[0].kind = WASI_VALUE_KIND_OWN;
    args[0].of.resource.type_index = 0u;
    args[0].of.resource.handle = handle;

    err = wasi_call(instance, "drop", args, 1u, NULL, 0u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call drop failed: %s", engine.error_msg);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == (void*)(uintptr_t)representation_value);

    err = wasi_resource_rep(instance, 0u, handle, &representation);
    WL_CHECK(t, err == WASI_ERR_INVALID_ARGUMENT);

    wasi_free_instance(instance);
    WL_CHECK(t, drop_state.drops == 1u);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_runs_component_resource_destructor_on_drop) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_module_t* core_module;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t args[1];
    uint32_t handle = UINT32_MAX;
    uint32_t representation_value = 77u;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_destructor_core_module(&module_bytes);
    wasi_test_build_resource_destructor_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    core_module = instance->core_module;

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_resource_new(instance, 0u, (void*)(uintptr_t)representation_value, &handle) == WASI_OK,
                   "wasi_resource_new failed: %s",
                   engine.error_msg);

    memset(args, 0, sizeof(args));
    args[0].kind = WASI_VALUE_KIND_OWN;
    args[0].of.resource.type_index = 0u;
    args[0].of.resource.handle = handle;

    err = wasi_call(instance, "drop", args, 1u, NULL, 0u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call drop failed: %s", engine.error_msg);

    wasi_test_expect_i32_global(t, core_module, "last_drop", (int32_t)representation_value);
    wasi_test_expect_i32_global(t, core_module, "drop_count", 1);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == (void*)(uintptr_t)representation_value);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_runs_component_resource_destructor_on_teardown) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_module_t* core_module;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t args[1];
    uint32_t handle = UINT32_MAX;
    uint32_t representation_value = 41u;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_destructor_core_module(&module_bytes);
    wasi_test_build_resource_destructor_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    core_module = instance->core_module;

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_resource_new(instance, 0u, (void*)(uintptr_t)representation_value, &handle) == WASI_OK,
                   "wasi_resource_new failed: %s",
                   engine.error_msg);

    memset(args, 0, sizeof(args));
    args[0].kind = WASI_VALUE_KIND_OWN;
    args[0].of.resource.type_index = 0u;
    args[0].of.resource.handle = handle;

    err = wasi_call(instance, "take", args, 1u, NULL, 0u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call take failed: %s", engine.error_msg);
    wasi_test_expect_i32_global(t, core_module, "last_drop", -1);
    wasi_test_expect_i32_global(t, core_module, "drop_count", 0);
    WL_CHECK(t, drop_state.drops == 0u);

    wasi_free_instance(instance);

    wasi_test_expect_i32_global(t, core_module, "last_drop", (int32_t)representation_value);
    wasi_test_expect_i32_global(t, core_module, "drop_count", 1);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == (void*)(uintptr_t)representation_value);

    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_resource_transfer_preserves_origin_destructor) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t dest_module_bytes;
    wasi_test_builder_t dest_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* dest_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* dest_instance;
    wasm_module_t* source_core_module;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    uint32_t source_handle = UINT32_MAX;
    uint32_t dest_handle = UINT32_MAX;
    uint32_t representation_value = 73u;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_destructor_core_module(&source_module_bytes);
    wasi_test_build_resource_destructor_component(&source_component_bytes, &source_module_bytes);
    wasi_test_build_resource_core_module(&dest_module_bytes);
    wasi_test_build_resource_sink_component(&dest_component_bytes, &dest_module_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    dest_component = wasi_load(&engine, dest_component_bytes.buf, dest_component_bytes.len);
    WL_REQUIRE_MSG(t, dest_component != NULL, "wasi_load dest failed: %s", engine.error_msg);

    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);
    dest_instance = wasi_instantiate(dest_component);
    WL_REQUIRE_MSG(t, dest_instance != NULL, "wasi_instantiate dest failed: %s", engine.error_msg);
    source_core_module = source_instance->core_module;

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(source_instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type source failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(dest_instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type dest failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_resource_new(source_instance, 0u, (void*)(uintptr_t)representation_value, &source_handle) == WASI_OK,
                   "wasi_resource_new failed: %s",
                   engine.error_msg);

    err = wasi__transfer_resource_handle(&engine,
                                         source_instance,
                                         0u,
                                         source_handle,
                                         dest_instance,
                                         0u,
                                         &dest_handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi__transfer_resource_handle failed: %s", engine.error_msg);
    WL_CHECK(t, dest_handle == 0u);

    err = wasi_resource_rep(source_instance, 0u, source_handle, &representation);
    WL_CHECK(t, err == WASI_ERR_INVALID_ARGUMENT);

    err = wasi_resource_drop(dest_instance, 0u, dest_handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_drop dest failed: %s", engine.error_msg);
    wasi_test_expect_i32_global(t, source_core_module, "last_drop", (int32_t)representation_value);
    wasi_test_expect_i32_global(t, source_core_module, "drop_count", 1);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == (void*)(uintptr_t)representation_value);

    wasi_free_instance(dest_instance);
    wasi_free_instance(source_instance);
    wasi_free_component(dest_component);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_resource_destructor_resolves_component_alias) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_module_t* core_module;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    uint32_t handle = UINT32_MAX;
    uint32_t representation_value = 88u;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_destructor_core_module(&module_bytes);
    wasi_test_build_resource_alias_destructor_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_instance_count(component) == 1u);
    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_alias_kind(component, 0u) == WASI_COMPONENT_ALIAS_KIND_INSTANCE_EXPORT);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    core_module = instance->core_module;

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(instance, 1u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_resource_new(instance, 1u, (void*)(uintptr_t)representation_value, &handle) == WASI_OK,
                   "wasi_resource_new failed: %s",
                   engine.error_msg);

    err = wasi_resource_drop(instance, 1u, handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_drop failed: %s", engine.error_msg);
    wasi_test_expect_i32_global(t, core_module, "last_drop", (int32_t)representation_value);
    wasi_test_expect_i32_global(t, core_module, "drop_count", 1);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == (void*)(uintptr_t)representation_value);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_allows_trivial_core_instance) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_core_module(&module_bytes);
    wasi_test_build_core_instance_u32_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 1u);
    WL_CHECK(t, wasi_component_core_instance_kind(component, 0u) == WASI_COMPONENT_CORE_INSTANCE_KIND_INSTANTIATE);
    WL_CHECK(t, wasi_component_core_instance_module_index(component, 0u) == 0u);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 0u);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 99u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_core_instance_function_args) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t importing_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_importing_module(&importing_module_bytes);
    wasi_test_build_core_instance_arg_component(&component_bytes,
                                                &source_module_bytes,
                                                &importing_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 2u);
    WL_CHECK(t, wasi_component_core_instance_arg_count(component, 1u) == 1u);
    WL_CHECK(t, strcmp(wasi_component_core_instance_arg_name(component, 1u, 0u), "dep") == 0);
    WL_CHECK(t, wasi_component_core_instance_arg_kind(component, 1u, 0u) == 0x12u);
    WL_CHECK(t, wasi_component_core_instance_arg_index(component, 1u, 0u) == 0u);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_core_instance_global_args) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t importing_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t result;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_global_source_module(&source_module_bytes);
    wasi_test_build_core_global_importing_module(&importing_module_bytes);
    wasi_test_build_core_global_arg_component(&component_bytes,
                                              &source_module_bytes,
                                              &importing_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 2u);
    WL_CHECK(t, wasi_component_core_instance_arg_kind(component, 1u, 0u) == 0x03u);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    memset(&result, 0, sizeof(result));
    err = wasi_call(instance, "read", NULL, 0u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call read failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.u32 == 7u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_core_instance_memory_args) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t importing_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t result;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_memory_source_module(&source_module_bytes);
    wasi_test_build_core_memory_importing_module(&importing_module_bytes);
    wasi_test_build_core_memory_arg_component(&component_bytes,
                                              &source_module_bytes,
                                              &importing_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 2u);
    WL_CHECK(t, wasi_component_core_instance_arg_kind(component, 1u, 0u) == 0x02u);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    memset(&result, 0, sizeof(result));
    err = wasi_call(instance, "read", NULL, 0u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call read failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.u32 == 42u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_core_instance_table_args) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t importing_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_table_source_module(&source_module_bytes);
    wasi_test_build_core_table_importing_module(&importing_module_bytes);
    wasi_test_build_core_table_arg_component(&component_bytes,
                                             &source_module_bytes,
                                             &importing_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 2u);
    WL_CHECK(t, wasi_component_core_instance_arg_kind(component, 1u, 0u) == 0x01u);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 41u;

    err = wasi_call(instance, "call-dep", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call call-dep failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 42u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_core_instance_tag_args) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t importing_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_export_kind_t export_kind = WASM_EXPORT_FUNC;
    uint32_t export_index = UINT32_MAX;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_tag_source_module(&source_module_bytes);
    wasi_test_build_core_tag_importing_module(&importing_module_bytes);
    wasi_test_build_core_tag_arg_component(&component_bytes,
                                           &source_module_bytes,
                                           &importing_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 2u);
    WL_CHECK(t, wasi_component_core_instance_arg_kind(component, 1u, 0u) == 0x04u);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);
    WL_CHECK(t, wasm_tag_count(instance->core_instances[1].module) == 1u);
    WL_REQUIRE(t, wasm_find_export(instance->core_instances[1].module, "dep_tag", &export_kind, &export_index));
    WL_CHECK(t, export_kind == WASM_EXPORT_TAG);
    WL_CHECK(t, export_index == 0u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_chains_dynamic_core_instance_args) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t middle_module_bytes;
    wasi_test_builder_t final_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_incrementing_module(&middle_module_bytes);
    wasi_test_build_core_instance_incrementing_module(&final_module_bytes);
    wasi_test_build_core_instance_chain_component(&component_bytes,
                                                  &source_module_bytes,
                                                  &middle_module_bytes,
                                                  &final_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 3u);
    WL_CHECK(t, wasi_component_core_module_error(component, 1u) == WASM_ERR_UNKNOWN_IMPORT);
    WL_CHECK(t, wasi_component_core_module_error(component, 2u) == WASM_ERR_UNKNOWN_IMPORT);
    WL_CHECK(t, wasi_component_core_instance_arg_index(component, 1u, 0u) == 0u);
    WL_CHECK(t, wasi_component_core_instance_arg_index(component, 2u, 0u) == 1u);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 2u);
    WL_CHECK(t, instance->core_instances[1].owns_module == 1u);
    WL_CHECK(t, instance->core_instances[2].owns_module == 1u);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 102u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_links_core_from_exports_globals) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t importing_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t result;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_global_source_module(&source_module_bytes);
    wasi_test_build_core_global_importing_module(&importing_module_bytes);
    wasi_test_build_core_global_from_exports_component(&component_bytes,
                                                       &source_module_bytes,
                                                       &importing_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 3u);
    WL_CHECK(t, wasi_component_core_instance_kind(component, 1u) == WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS);
    WL_CHECK(t, wasi_component_core_instance_export_kind(component, 1u, 0u) == WASM_EXPORT_GLOBAL);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    memset(&result, 0, sizeof(result));
    err = wasi_call(instance, "read", NULL, 0u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call read failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.u32 == 7u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_links_core_from_exports_memories) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t importing_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t result;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_memory_source_module(&source_module_bytes);
    wasi_test_build_core_memory_importing_module(&importing_module_bytes);
    wasi_test_build_core_memory_from_exports_component(&component_bytes,
                                                       &source_module_bytes,
                                                       &importing_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 3u);
    WL_CHECK(t, wasi_component_core_instance_kind(component, 1u) == WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS);
    WL_CHECK(t, wasi_component_core_instance_export_kind(component, 1u, 0u) == WASM_EXPORT_MEM);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    memset(&result, 0, sizeof(result));
    err = wasi_call(instance, "read", NULL, 0u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call read failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.u32 == 42u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_links_core_from_exports_tables) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t importing_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_table_source_module(&source_module_bytes);
    wasi_test_build_core_table_importing_module(&importing_module_bytes);
    wasi_test_build_core_table_from_exports_component(&component_bytes,
                                                      &source_module_bytes,
                                                      &importing_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 3u);
    WL_CHECK(t, wasi_component_core_instance_kind(component, 1u) == WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS);
    WL_CHECK(t, wasi_component_core_instance_export_kind(component, 1u, 0u) == WASM_EXPORT_TABLE);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 6u;

    err = wasi_call(instance, "call-dep", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call call-dep failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 7u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_links_core_from_exports_tags) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t importing_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_export_kind_t export_kind = WASM_EXPORT_FUNC;
    uint32_t export_index = UINT32_MAX;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_tag_source_module(&source_module_bytes);
    wasi_test_build_core_tag_importing_module(&importing_module_bytes);
    wasi_test_build_core_tag_from_exports_component(&component_bytes,
                                                    &source_module_bytes,
                                                    &importing_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 3u);
    WL_CHECK(t, wasi_component_core_instance_kind(component, 1u) == WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS);
    WL_CHECK(t, wasi_component_core_instance_export_kind(component, 1u, 0u) == WASM_EXPORT_TAG);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, wasm_tag_count(instance->core_instances[2].module) == 1u);
    WL_REQUIRE(t, wasm_find_export(instance->core_instances[2].module, "dep_tag", &export_kind, &export_index));
    WL_CHECK(t, export_kind == WASM_EXPORT_TAG);
    WL_CHECK(t, export_index == 0u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_call_resolves_core_export_alias_lifts) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t identity_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_identity_module(&identity_module_bytes);
    wasi_test_build_core_alias_lift_component(&component_bytes,
                                              &source_module_bytes,
                                              &identity_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_alias_kind(component, 0u) == WASI_COMPONENT_ALIAS_KIND_CORE_INSTANCE_EXPORT);
    WL_CHECK(t, wasi_component_alias_instance_index(component, 0u) == 0u);
    WL_CHECK(t, strcmp(wasi_component_alias_name(component, 0u), "inc") == 0);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_call_resolves_component_export_alias_lifts) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    uint32_t resolved_type_index = UINT32_MAX;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_component_export_alias_component(&component_bytes, &source_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_instance_count(component) == 1u);
    WL_CHECK(t, wasi_component_instance_kind(component, 0u) == WASI_COMPONENT_INSTANCE_KIND_FROM_EXPORTS);
    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_alias_kind(component, 0u) == WASI_COMPONENT_ALIAS_KIND_INSTANCE_EXPORT);
    WL_CHECK(t, !wasi_component_alias_sort_is_core(component, 0u));
    WL_CHECK(t, wasi_component_alias_extern_kind(component, 0u) == WASI_COMPONENT_EXTERN_KIND_FUNC);
    WL_CHECK(t, wasi_component_alias_instance_index(component, 0u) == 0u);
    WL_CHECK(t, strcmp(wasi_component_alias_name(component, 0u), "echo") == 0);
    WL_CHECK(t, wasi_component_export_index(component, 0u) == 1u);
    WL_CHECK(t, wasi_component_export_func_type_index(component, 0u, &resolved_type_index));
    WL_CHECK(t, resolved_type_index == 0u);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 0u);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_component_export_func_type_index_handles_preceding_type_imports) {
    wasi_engine_t engine;
    wasi_component_t component;
    wasi_component_import_t import_;
    wasi_component_type_t types[4];
    wasi_component_func_t func;
    wasi_component_export_t export_;
    uint32_t resolved_type_index = UINT32_MAX;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    memset(&component, 0, sizeof(component));
    memset(&import_, 0, sizeof(import_));
    memset(types, 0, sizeof(types));
    memset(&func, 0, sizeof(func));
    memset(&export_, 0, sizeof(export_));

    component.engine = &engine;
    component.imports = &import_;
    component.num_imports = 1u;
    component.types = types;
    component.num_types = 4u;
    component.funcs = &func;
    component.num_funcs = 1u;
    component.exports = &export_;
    component.num_exports = 1u;

    import_.kind = WASI_COMPONENT_EXTERN_KIND_TYPE;
    import_.name = (char*)"rec";
    import_.offset = 30u;
    import_.type_index = 1u;

    types[0].kind = WASI_COMPONENT_TYPE_KIND_DEFINED;
    types[0].offset = 10u;
    types[1].kind = WASI_COMPONENT_TYPE_KIND_DEFINED;
    types[1].offset = 20u;
    types[2].kind = WASI_COMPONENT_TYPE_KIND_DEFINED;
    types[2].offset = 30u;
    types[3].kind = WASI_COMPONENT_TYPE_KIND_FUNC;
    types[3].offset = 40u;
    types[3].func.num_params = 1u;
    types[3].func.num_results = 1u;

    func.offset = 50u;
    func.type_index = 3u;

    export_.kind = WASI_COMPONENT_EXTERN_KIND_FUNC;
    export_.name = (char*)"echo";
    export_.index = 0u;

    WL_CHECK(t, wasi_component_import_count(&component) == 1u);
    WL_CHECK(t, wasi_component_type_count(&component) == 4u);
    WL_CHECK(t, wasi_component_type_kind(&component, 0u) == WASI_COMPONENT_TYPE_KIND_DEFINED);
    WL_CHECK(t, wasi_component_type_kind(&component, 1u) == WASI_COMPONENT_TYPE_KIND_DEFINED);
    WL_CHECK(t, wasi_component_type_kind(&component, 2u) == WASI_COMPONENT_TYPE_KIND_DEFINED);
    WL_CHECK(t, wasi_component_type_kind(&component, 3u) == WASI_COMPONENT_TYPE_KIND_FUNC);
    WL_REQUIRE(t, wasi_component_export_func_type_index(&component, 0u, &resolved_type_index));
    WL_CHECK(t, resolved_type_index == 3u);
    WL_CHECK(t, wasi_component_func_type_param_count(&component, resolved_type_index) == 1u);
    WL_CHECK(t, wasi_component_func_type_result_count(&component, resolved_type_index) == 1u);

    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_executes_nested_component_instances) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_u32_component(&child_component_bytes, &source_module_bytes);
    wasi_test_build_nested_component_alias_component(&parent_component_bytes, &child_component_bytes);

    component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_nested_component_count(component) == 1u);
    WL_CHECK(t, wasi_component_instance_count(component) == 1u);
    WL_CHECK(t, wasi_component_instance_kind(component, 0u) == WASI_COMPONENT_INSTANCE_KIND_INSTANTIATE);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->num_component_instances == 1u);
    WL_CHECK(t, instance->component_instances[0].ref.kind == WASI__COMPONENT_NAMESPACE_INSTANCE);
    WL_REQUIRE(t, instance->component_instances[0].ref.instance != NULL);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_accepts_component_type_args) {
    wasi_engine_t engine;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_type_only_child_component(&child_component_bytes);
    wasi_test_build_parent_type_arg_component(&parent_component_bytes, &child_component_bytes);

    component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_type_count(component) == 1u);
    WL_CHECK(t, wasi_component_instance_count(component) == 1u);
    WL_CHECK(t, wasi_component_instance_arg_kind(component, 0u, 0u) == WASI_COMPONENT_EXTERN_KIND_TYPE);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->num_component_instances == 1u);
    WL_CHECK(t, instance->component_instances[0].ref.kind == WASI__COMPONENT_NAMESPACE_INSTANCE);
    WL_REQUIRE(t, instance->component_instances[0].ref.instance != NULL);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_instance_exported_type_args) {
    wasi_engine_t engine;
    wasi_test_builder_t exporting_child_component_bytes;
    wasi_test_builder_t importing_child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* component;
    const wasi_component_t* exporting_child_component;
    wasi_instance_t* instance;
    wasi_instance_t* importing_child_instance;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_type_exporting_child_component(&exporting_child_component_bytes);
    wasi_test_build_type_only_child_component(&importing_child_component_bytes);
    wasi_test_build_parent_instance_exported_type_arg_component(&parent_component_bytes,
                                                                &exporting_child_component_bytes,
                                                                &importing_child_component_bytes);

    component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_nested_component_count(component) == 2u);
    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_alias_kind(component, 0u) == WASI_COMPONENT_ALIAS_KIND_INSTANCE_EXPORT);
    WL_CHECK(t, wasi_component_alias_extern_kind(component, 0u) == WASI_COMPONENT_EXTERN_KIND_TYPE);
    WL_CHECK(t, wasi_component_instance_count(component) == 2u);
    WL_CHECK(t, wasi_component_instance_arg_kind(component, 1u, 0u) == WASI_COMPONENT_EXTERN_KIND_TYPE);

    exporting_child_component = wasi_component_nested_component_at(component, 0u);
    WL_REQUIRE(t, exporting_child_component != NULL);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_REQUIRE(t, instance->num_component_instances == 2u);
    importing_child_instance = instance->component_instances[1].ref.instance;
    WL_REQUIRE(t, importing_child_instance != NULL);
    WL_REQUIRE(t, importing_child_instance->num_imports == 1u);
    WL_CHECK(t, importing_child_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_TYPE);
    WL_CHECK(t, importing_child_instance->imports[0].of.type_ref.component == exporting_child_component);
    WL_CHECK(t, importing_child_instance->imports[0].of.type_ref.type_index == 0u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_accepts_component_value_args) {
    wasi_engine_t engine;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_instance_t* child_instance;
    wasi__component_import_runtime_t overrides[1];
    const wasi_value_t* parent_value;
    const wasi_value_t* child_value;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_value_importing_child_component(&child_component_bytes, 0x73u);
    wasi_test_build_parent_value_arg_component(&parent_component_bytes, &child_component_bytes, 0x73u);

    component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_import_count(component) == 1u);
    WL_CHECK(t, wasi_component_import_kind(component, 0u) == WASI_COMPONENT_EXTERN_KIND_VALUE);
    WL_CHECK(t, wasi_component_instance_count(component) == 1u);
    WL_CHECK(t, wasi_component_instance_arg_kind(component, 0u, 0u) == WASI_COMPONENT_EXTERN_KIND_VALUE);

    memset(overrides, 0, sizeof(overrides));
    overrides[0].kind = WASI__COMPONENT_IMPORT_RUNTIME_VALUE;
    overrides[0].of.value.type_ref.component = component;
    overrides[0].of.value.type_ref.type_index = 0u;
    overrides[0].of.value.value = (wasi_value_t*)WASM_CALLOC(1u, sizeof(*overrides[0].of.value.value));
    WL_REQUIRE(t, overrides[0].of.value.value != NULL);

    err = wasi_value_set_string_copy(overrides[0].of.value.value, "linked", strlen("linked"));
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_value_set_string_copy failed: %s", engine.error_msg);

    instance = wasi__instantiate_component_internal(component, overrides, 1u, NULL);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi__instantiate_component_internal failed: %s", engine.error_msg);
    WL_REQUIRE(t, instance->num_imports == 1u);
    WL_CHECK(t, instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_VALUE);
    WL_REQUIRE(t, instance->num_component_instances == 1u);

    child_instance = instance->component_instances[0].ref.instance;
    WL_REQUIRE(t, child_instance != NULL);
    WL_REQUIRE(t, child_instance->num_imports == 1u);
    WL_CHECK(t, child_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_VALUE);

    parent_value = instance->imports[0].of.value.value;
    child_value = child_instance->imports[0].of.value.value;
    WL_REQUIRE(t, parent_value != NULL);
    WL_REQUIRE(t, child_value != NULL);
    WL_CHECK(t, parent_value != overrides[0].of.value.value);
    WL_CHECK(t, child_value != parent_value);
    WL_CHECK(t, parent_value->kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, child_value->kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, strcmp(parent_value->of.string.data, "linked") == 0);
    WL_CHECK(t, strcmp(child_value->of.string.data, "linked") == 0);
    WL_CHECK(t, parent_value->of.string.data != overrides[0].of.value.value->of.string.data);
    WL_CHECK(t, child_value->of.string.data != parent_value->of.string.data);

    wasi_free_instance(instance);
    wasi__component_import_runtime_array_clear(overrides, 1u);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_rejects_mismatched_component_value_args) {
    wasi_engine_t engine;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* component;
    wasi__component_import_runtime_t overrides[1];
    wasi_instance_t* instance;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_value_importing_child_component(&child_component_bytes, 0x79u);
    wasi_test_build_parent_value_arg_component(&parent_component_bytes, &child_component_bytes, 0x73u);

    component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    memset(overrides, 0, sizeof(overrides));
    overrides[0].kind = WASI__COMPONENT_IMPORT_RUNTIME_VALUE;
    overrides[0].of.value.type_ref.component = component;
    overrides[0].of.value.type_ref.type_index = 0u;
    overrides[0].of.value.value = (wasi_value_t*)WASM_CALLOC(1u, sizeof(*overrides[0].of.value.value));
    WL_REQUIRE(t, overrides[0].of.value.value != NULL);

    err = wasi_value_set_string_copy(overrides[0].of.value.value, "linked", strlen("linked"));
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_value_set_string_copy failed: %s", engine.error_msg);

    instance = wasi__instantiate_component_internal(component, overrides, 1u, NULL);
    WL_CHECK(t, instance == NULL);
    WL_CHECK(t, engine.last_error == WASI_ERR_TYPE_MISMATCH);
    WL_CHECK(t, strstr(engine.error_msg, "type does not match") != NULL);

    wasi__component_import_runtime_array_clear(overrides, 1u);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_bind_import_value_resolves_top_level_string_imports) {
    wasi_engine_t engine;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t bound_value;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_value_importing_child_component(&component_bytes, 0x73u);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_value_init(&bound_value);
    err = wasi_value_set_string_copy(&bound_value, "linked", strlen("linked"));
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_value_set_string_copy failed: %s", engine.error_msg);

    err = wasi_bind_import_value(&engine, "seed", component, 0u, &bound_value, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_value failed: %s", engine.error_msg);
    wasi_value_destroy(&bound_value);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_REQUIRE(t, instance->num_imports == 1u);
    WL_CHECK(t, instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_VALUE);
    WL_CHECK(t, instance->imports[0].of.value.type_ref.component == component);
    WL_CHECK(t, instance->imports[0].of.value.type_ref.type_index == 0u);
    WL_CHECK(t, instance->imports[0].of.value.owner_instance == instance);
    WL_REQUIRE(t, instance->imports[0].of.value.value != NULL);
    WL_CHECK(t, instance->imports[0].of.value.value->kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, strcmp(instance->imports[0].of.value.value->of.string.data, "linked") == 0);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_imported_type_backed_value_args) {
    wasi_engine_t engine;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_test_builder_t grandparent_component_bytes;
    wasi_component_t* grandparent_component;
    wasi_instance_t* grandparent_instance;
    wasi_instance_t* parent_instance;
    wasi_instance_t* child_instance;
    wasi_value_t bound_value;
    const wasi_value_t* parent_value;
    const wasi_value_t* child_value;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_imported_type_value_child_component(&child_component_bytes);
    wasi_test_build_imported_type_value_parent_component(&parent_component_bytes, &child_component_bytes);
    wasi_test_build_grandparent_imported_type_value_component(&grandparent_component_bytes, &parent_component_bytes);

    grandparent_component = wasi_load(&engine, grandparent_component_bytes.buf, grandparent_component_bytes.len);
    WL_REQUIRE_MSG(t, grandparent_component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_value_init(&bound_value);
    err = wasi_value_set_string_copy(&bound_value, "linked", strlen("linked"));
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_value_set_string_copy failed: %s", engine.error_msg);

    err = wasi_bind_import_value(&engine, "seed", grandparent_component, 0u, &bound_value, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_value failed: %s", engine.error_msg);
    wasi_value_destroy(&bound_value);

    grandparent_instance = wasi_instantiate(grandparent_component);
    WL_REQUIRE_MSG(t, grandparent_instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_REQUIRE(t, grandparent_instance->num_component_instances == 1u);

    parent_instance = grandparent_instance->component_instances[0].ref.instance;
    WL_REQUIRE(t, parent_instance != NULL);
    WL_REQUIRE(t, parent_instance->num_imports == 2u);
    WL_CHECK(t, parent_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_TYPE);
    WL_CHECK(t, parent_instance->imports[1].kind == WASI__COMPONENT_IMPORT_RUNTIME_VALUE);
    WL_CHECK(t, parent_instance->imports[1].of.value.type_ref.component != NULL);
    WL_CHECK(t, parent_instance->imports[1].of.value.type_ref.type_index != UINT32_MAX);
    WL_CHECK(t, parent_instance->imports[1].of.value.owner_instance == parent_instance);

    parent_value = parent_instance->imports[1].of.value.value;
    WL_REQUIRE(t, parent_value != NULL);
    WL_CHECK(t, parent_value->kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, strcmp(parent_value->of.string.data, "linked") == 0);

    WL_REQUIRE(t, parent_instance->num_component_instances == 1u);
    child_instance = parent_instance->component_instances[0].ref.instance;
    WL_REQUIRE(t, child_instance != NULL);
    WL_REQUIRE(t, child_instance->num_imports == 2u);
    WL_CHECK(t, child_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_TYPE);
    WL_CHECK(t, child_instance->imports[1].kind == WASI__COMPONENT_IMPORT_RUNTIME_VALUE);
    WL_CHECK(t, child_instance->imports[1].of.value.type_ref.component != NULL);
    WL_CHECK(t, child_instance->imports[1].of.value.type_ref.type_index != UINT32_MAX);
    WL_CHECK(t, child_instance->imports[1].of.value.owner_instance == child_instance);

    child_value = child_instance->imports[1].of.value.value;
    WL_REQUIRE(t, child_value != NULL);
    WL_CHECK(t, child_value->kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, strcmp(child_value->of.string.data, "linked") == 0);

    wasi_free_instance(grandparent_instance);
    wasi_free_component(grandparent_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_bind_import_value_transfers_top_level_resource_values) {
    wasi_engine_t engine;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t dest_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* dest_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* dest_instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t bound_value;
    uint32_t handle = UINT32_MAX;
    int representation_value = 17;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_type_only_component(&source_component_bytes);
    wasi_test_build_imported_resource_value_component(&dest_component_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    dest_component = wasi_load(&engine, dest_component_bytes.buf, dest_component_bytes.len);
    WL_REQUIRE_MSG(t, dest_component != NULL, "wasi_load dest failed: %s", engine.error_msg);

    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    err = wasi_instance_bind_resource_type(source_instance, 0u, resource_type);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_instance_bind_resource_type failed: %s", engine.error_msg);
    err = wasi_resource_new(source_instance, 0u, &representation_value, &handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_new failed: %s", engine.error_msg);

    memset(&bound_value, 0, sizeof(bound_value));
    bound_value.kind = WASI_VALUE_KIND_OWN;
    bound_value.of.resource.type_index = 0u;
    bound_value.of.resource.handle = handle;

    err = wasi_bind_import_value(&engine, "seed", source_component, 1u, &bound_value, source_instance);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_value failed: %s", engine.error_msg);

    dest_instance = wasi_instantiate(dest_component);
    WL_REQUIRE_MSG(t, dest_instance != NULL, "wasi_instantiate dest failed: %s", engine.error_msg);
    WL_REQUIRE(t, dest_instance->num_imports == 1u);
    WL_CHECK(t, dest_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_VALUE);
    WL_CHECK(t, dest_instance->imports[0].of.value.owner_instance == dest_instance);
    WL_REQUIRE(t, dest_instance->imports[0].of.value.value != NULL);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->kind == WASI_VALUE_KIND_OWN);
    WL_CHECK(t, dest_instance->num_resource_types == 1u);

    err = wasi_resource_rep(dest_instance,
                            0u,
                            dest_instance->imports[0].of.value.value->of.resource.handle,
                            &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep dest failed: %s", engine.error_msg);
    WL_CHECK(t, representation == &representation_value);

    err = wasi_resource_rep(source_instance, 0u, handle, &representation);
    WL_CHECK(t, err == WASI_ERR_INVALID_ARGUMENT);

    wasi_free_instance(dest_instance);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == &representation_value);

    wasi_free_instance(source_instance);
    WL_CHECK(t, drop_state.drops == 1u);

    wasi_free_component(dest_component);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_bind_import_value_transfers_list_resource_values) {
    wasi_engine_t engine;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t dest_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* dest_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* dest_instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t bound_value;
    wasi_value_t list_items[1];
    uint32_t handle = UINT32_MAX;
    int representation_value = 23;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_typed_value_component(&source_component_bytes,
                                                   wasi_test_emit_list_own_resource_valtype,
                                                   0);
    wasi_test_build_resource_typed_value_component(&dest_component_bytes,
                                                   wasi_test_emit_list_own_resource_valtype,
                                                   1);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    dest_component = wasi_load(&engine, dest_component_bytes.buf, dest_component_bytes.len);
    WL_REQUIRE_MSG(t, dest_component != NULL, "wasi_load dest failed: %s", engine.error_msg);

    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    err = wasi_instance_bind_resource_type(source_instance, 0u, resource_type);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_instance_bind_resource_type failed: %s", engine.error_msg);
    err = wasi_resource_new(source_instance, 0u, &representation_value, &handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_new failed: %s", engine.error_msg);

    memset(&bound_value, 0, sizeof(bound_value));
    memset(list_items, 0, sizeof(list_items));
    list_items[0].kind = WASI_VALUE_KIND_OWN;
    list_items[0].of.resource.type_index = 0u;
    list_items[0].of.resource.handle = handle;
    bound_value.kind = WASI_VALUE_KIND_LIST;
    bound_value.of.seq.values = list_items;
    bound_value.of.seq.len = 1u;
    bound_value.of.seq.owned = 0;

    err = wasi_bind_import_value(&engine, "seed", source_component, 1u, &bound_value, source_instance);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_value failed: %s", engine.error_msg);

    dest_instance = wasi_instantiate(dest_component);
    WL_REQUIRE_MSG(t, dest_instance != NULL, "wasi_instantiate dest failed: %s", engine.error_msg);
    WL_REQUIRE(t, dest_instance->num_imports == 1u);
    WL_REQUIRE(t, dest_instance->imports[0].of.value.value != NULL);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->kind == WASI_VALUE_KIND_LIST);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->of.seq.len == 1u);
    WL_REQUIRE(t, dest_instance->imports[0].of.value.value->of.seq.values != NULL);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->of.seq.values[0].kind == WASI_VALUE_KIND_OWN);

    err = wasi_resource_rep(dest_instance,
                            0u,
                            dest_instance->imports[0].of.value.value->of.seq.values[0].of.resource.handle,
                            &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep dest failed: %s", engine.error_msg);
    WL_CHECK(t, representation == &representation_value);

    err = wasi_resource_rep(source_instance, 0u, handle, &representation);
    WL_CHECK(t, err == WASI_ERR_INVALID_ARGUMENT);

    wasi_free_instance(dest_instance);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == &representation_value);

    wasi_free_instance(source_instance);
    WL_CHECK(t, drop_state.drops == 1u);

    wasi_free_component(dest_component);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_bind_import_value_transfers_record_resource_values) {
    wasi_engine_t engine;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t dest_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* dest_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* dest_instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t bound_value;
    wasi_value_t record_fields[2];
    uint32_t handle = UINT32_MAX;
    int representation_value = 29;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_typed_value_component(&source_component_bytes,
                                                   wasi_test_emit_record_own_resource_valtype,
                                                   0);
    wasi_test_build_resource_typed_value_component(&dest_component_bytes,
                                                   wasi_test_emit_record_own_resource_valtype,
                                                   1);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    dest_component = wasi_load(&engine, dest_component_bytes.buf, dest_component_bytes.len);
    WL_REQUIRE_MSG(t, dest_component != NULL, "wasi_load dest failed: %s", engine.error_msg);

    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    err = wasi_instance_bind_resource_type(source_instance, 0u, resource_type);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_instance_bind_resource_type failed: %s", engine.error_msg);
    err = wasi_resource_new(source_instance, 0u, &representation_value, &handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_new failed: %s", engine.error_msg);

    memset(&bound_value, 0, sizeof(bound_value));
    memset(record_fields, 0, sizeof(record_fields));
    record_fields[0].kind = WASI_VALUE_KIND_U32;
    record_fields[0].of.u32 = 7u;
    record_fields[1].kind = WASI_VALUE_KIND_OWN;
    record_fields[1].of.resource.type_index = 0u;
    record_fields[1].of.resource.handle = handle;
    bound_value.kind = WASI_VALUE_KIND_RECORD;
    bound_value.of.seq.values = record_fields;
    bound_value.of.seq.len = 2u;
    bound_value.of.seq.owned = 0;

    err = wasi_bind_import_value(&engine, "seed", source_component, 1u, &bound_value, source_instance);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_value failed: %s", engine.error_msg);

    dest_instance = wasi_instantiate(dest_component);
    WL_REQUIRE_MSG(t, dest_instance != NULL, "wasi_instantiate dest failed: %s", engine.error_msg);
    WL_REQUIRE(t, dest_instance->num_imports == 1u);
    WL_REQUIRE(t, dest_instance->imports[0].of.value.value != NULL);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->kind == WASI_VALUE_KIND_RECORD);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->of.seq.len == 2u);
    WL_REQUIRE(t, dest_instance->imports[0].of.value.value->of.seq.values != NULL);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->of.seq.values[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->of.seq.values[0].of.u32 == 7u);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->of.seq.values[1].kind == WASI_VALUE_KIND_OWN);

    err = wasi_resource_rep(dest_instance,
                            0u,
                            dest_instance->imports[0].of.value.value->of.seq.values[1].of.resource.handle,
                            &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep dest failed: %s", engine.error_msg);
    WL_CHECK(t, representation == &representation_value);

    err = wasi_resource_rep(source_instance, 0u, handle, &representation);
    WL_CHECK(t, err == WASI_ERR_INVALID_ARGUMENT);

    wasi_free_instance(dest_instance);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == &representation_value);

    wasi_free_instance(source_instance);
    WL_CHECK(t, drop_state.drops == 1u);

    wasi_free_component(dest_component);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_bind_import_value_transfers_variant_resource_values) {
    wasi_engine_t engine;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t dest_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* dest_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* dest_instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t bound_value;
    wasi_value_t payload;
    uint32_t handle = UINT32_MAX;
    int representation_value = 31;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_typed_value_component(&source_component_bytes,
                                                   wasi_test_emit_variant_own_resource_valtype,
                                                   0);
    wasi_test_build_resource_typed_value_component(&dest_component_bytes,
                                                   wasi_test_emit_variant_own_resource_valtype,
                                                   1);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    dest_component = wasi_load(&engine, dest_component_bytes.buf, dest_component_bytes.len);
    WL_REQUIRE_MSG(t, dest_component != NULL, "wasi_load dest failed: %s", engine.error_msg);

    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    err = wasi_instance_bind_resource_type(source_instance, 0u, resource_type);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_instance_bind_resource_type failed: %s", engine.error_msg);
    err = wasi_resource_new(source_instance, 0u, &representation_value, &handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_new failed: %s", engine.error_msg);

    memset(&bound_value, 0, sizeof(bound_value));
    memset(&payload, 0, sizeof(payload));
    payload.kind = WASI_VALUE_KIND_OWN;
    payload.of.resource.type_index = 0u;
    payload.of.resource.handle = handle;
    bound_value.kind = WASI_VALUE_KIND_VARIANT;
    bound_value.of.variant.case_index = 1u;
    bound_value.of.variant.has_payload = 1;
    bound_value.of.variant.payload = &payload;
    bound_value.of.variant.payload_owned = 0;

    err = wasi_bind_import_value(&engine, "seed", source_component, 1u, &bound_value, source_instance);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_value failed: %s", engine.error_msg);

    dest_instance = wasi_instantiate(dest_component);
    WL_REQUIRE_MSG(t, dest_instance != NULL, "wasi_instantiate dest failed: %s", engine.error_msg);
    WL_REQUIRE(t, dest_instance->num_imports == 1u);
    WL_REQUIRE(t, dest_instance->imports[0].of.value.value != NULL);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->kind == WASI_VALUE_KIND_VARIANT);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->of.variant.case_index == 1u);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->of.variant.has_payload);
    WL_REQUIRE(t, dest_instance->imports[0].of.value.value->of.variant.payload != NULL);
    WL_CHECK(t, dest_instance->imports[0].of.value.value->of.variant.payload->kind == WASI_VALUE_KIND_OWN);

    err = wasi_resource_rep(dest_instance,
                            0u,
                            dest_instance->imports[0].of.value.value->of.variant.payload->of.resource.handle,
                            &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep dest failed: %s", engine.error_msg);
    WL_CHECK(t, representation == &representation_value);

    err = wasi_resource_rep(source_instance, 0u, handle, &representation);
    WL_CHECK(t, err == WASI_ERR_INVALID_ARGUMENT);

    wasi_free_instance(dest_instance);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == &representation_value);

    wasi_free_instance(source_instance);
    WL_CHECK(t, drop_state.drops == 1u);

    wasi_free_component(dest_component);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_bind_import_value_rejects_top_level_borrow_resource_values) {
    wasi_engine_t engine;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t dest_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* dest_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* dest_instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t bound_value;
    uint32_t handle = UINT32_MAX;
    int representation_value = 37;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_typed_value_component(&source_component_bytes,
                                                   wasi_test_emit_top_level_borrow_resource_valtype,
                                                   0);
    wasi_test_build_resource_typed_value_component(&dest_component_bytes,
                                                   wasi_test_emit_top_level_borrow_resource_valtype,
                                                   1);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    dest_component = wasi_load(&engine, dest_component_bytes.buf, dest_component_bytes.len);
    WL_REQUIRE_MSG(t, dest_component != NULL, "wasi_load dest failed: %s", engine.error_msg);

    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    err = wasi_instance_bind_resource_type(source_instance, 0u, resource_type);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_instance_bind_resource_type failed: %s", engine.error_msg);
    err = wasi_resource_new(source_instance, 0u, &representation_value, &handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_new failed: %s", engine.error_msg);

    memset(&bound_value, 0, sizeof(bound_value));
    bound_value.kind = WASI_VALUE_KIND_BORROW;
    bound_value.of.resource.type_index = 0u;
    bound_value.of.resource.handle = handle;

    err = wasi_bind_import_value(&engine, "seed", source_component, 1u, &bound_value, source_instance);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_value failed: %s", engine.error_msg);

    dest_instance = wasi_instantiate(dest_component);
    WL_CHECK(t, dest_instance == NULL);
    WL_CHECK(t, engine.last_error == WASI_ERR_NOT_IMPLEMENTED);
    WL_CHECK(t, strstr(engine.error_msg, "uses borrow resources") != NULL);

    err = wasi_resource_rep(source_instance, 0u, handle, &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep source failed: %s", engine.error_msg);
    WL_CHECK(t, representation == &representation_value);

    wasi_free_instance(source_instance);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == &representation_value);

    wasi_free_component(dest_component);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_bind_import_value_rejects_list_borrow_resource_values) {
    wasi_engine_t engine;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t dest_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* dest_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* dest_instance;
    wasi_resource_type_t resource_type;
    wasi_test_resource_drop_state_t drop_state;
    wasi_value_t bound_value;
    wasi_value_t list_items[1];
    uint32_t handle = UINT32_MAX;
    int representation_value = 41;
    void* representation = NULL;
    wasi_error_t err;

    memset(&drop_state, 0, sizeof(drop_state));
    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_typed_value_component(&source_component_bytes,
                                                   wasi_test_emit_list_borrow_resource_valtype,
                                                   0);
    wasi_test_build_resource_typed_value_component(&dest_component_bytes,
                                                   wasi_test_emit_list_borrow_resource_valtype,
                                                   1);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    dest_component = wasi_load(&engine, dest_component_bytes.buf, dest_component_bytes.len);
    WL_REQUIRE_MSG(t, dest_component != NULL, "wasi_load dest failed: %s", engine.error_msg);

    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         wasi_test_resource_destructor,
                                         &drop_state);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    err = wasi_instance_bind_resource_type(source_instance, 0u, resource_type);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_instance_bind_resource_type failed: %s", engine.error_msg);
    err = wasi_resource_new(source_instance, 0u, &representation_value, &handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_new failed: %s", engine.error_msg);

    memset(&bound_value, 0, sizeof(bound_value));
    memset(list_items, 0, sizeof(list_items));
    list_items[0].kind = WASI_VALUE_KIND_BORROW;
    list_items[0].of.resource.type_index = 0u;
    list_items[0].of.resource.handle = handle;
    bound_value.kind = WASI_VALUE_KIND_LIST;
    bound_value.of.seq.values = list_items;
    bound_value.of.seq.len = 1u;
    bound_value.of.seq.owned = 0;

    err = wasi_bind_import_value(&engine, "seed", source_component, 1u, &bound_value, source_instance);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_value failed: %s", engine.error_msg);

    dest_instance = wasi_instantiate(dest_component);
    WL_CHECK(t, dest_instance == NULL);
    WL_CHECK(t, engine.last_error == WASI_ERR_NOT_IMPLEMENTED);
    WL_CHECK(t, strstr(engine.error_msg, "uses borrow resources") != NULL);

    err = wasi_resource_rep(source_instance, 0u, handle, &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep source failed: %s", engine.error_msg);
    WL_CHECK(t, representation == &representation_value);

    wasi_free_instance(source_instance);
    WL_CHECK(t, drop_state.drops == 1u);
    WL_CHECK(t, drop_state.last_representation == &representation_value);

    wasi_free_component(dest_component);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_outer_func_aliases) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* parent_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* parent_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_u32_component(&source_component_bytes, &source_module_bytes);
    wasi_test_build_outer_func_alias_child_component(&child_component_bytes);
    wasi_test_build_parent_outer_func_alias_component(&parent_component_bytes, &child_component_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    err = wasi_bind_import_func(&engine, "wasi:test/ops/inc@0.3.0", source_instance, "echo");
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_func failed: %s", engine.error_msg);

    parent_component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, parent_component != NULL, "wasi_load parent failed: %s", engine.error_msg);
    parent_instance = wasi_instantiate(parent_component);
    WL_REQUIRE_MSG(t, parent_instance != NULL, "wasi_instantiate parent failed: %s", engine.error_msg);
    WL_CHECK(t, parent_instance->num_component_instances == 1u);
    WL_REQUIRE(t, parent_instance->component_instances[0].ref.instance != NULL);
    WL_CHECK(t, parent_instance->component_instances[0].ref.instance->parent_instance == parent_instance);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(parent_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(parent_instance);
    wasi_free_component(parent_component);
    wasi_free_instance(source_instance);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_outer_type_aliases_for_export_types) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* parent_component;
    const wasi_component_t* nested_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* parent_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    uint32_t export_type_index = UINT32_MAX;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_u32_component(&source_component_bytes, &source_module_bytes);
    wasi_test_build_outer_type_alias_child_component(&child_component_bytes);
    wasi_test_build_parent_outer_func_alias_component(&parent_component_bytes, &child_component_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    err = wasi_bind_import_func(&engine, "wasi:test/ops/inc@0.3.0", source_instance, "echo");
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_func failed: %s", engine.error_msg);

    parent_component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, parent_component != NULL, "wasi_load parent failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_nested_component_count(parent_component) == 1u);

    nested_component = wasi_component_nested_component_at(parent_component, 0u);
    WL_REQUIRE(t, nested_component != NULL);
    WL_CHECK(t, wasi_component_type_count(nested_component) == 1u);
    WL_CHECK(t, wasi_component_export_count(nested_component) == 1u);
    WL_CHECK(t, wasi_component_export_has_type(nested_component, 0u));
    WL_CHECK(t, wasi_component_export_type_index(nested_component, 0u) == 0u);
    WL_REQUIRE(t, wasi_component_export_func_type_index(nested_component, 0u, &export_type_index));
    WL_CHECK(t, export_type_index == 0u);
    WL_CHECK(t, wasi_component_type_kind(nested_component, export_type_index) == WASI_COMPONENT_TYPE_KIND_FUNC);
    WL_CHECK(t, wasi_component_func_type_param_count(nested_component, export_type_index) == 1u);
    WL_CHECK(t, wasi_component_func_type_param_code(nested_component, export_type_index, 0u) == 0x79u);
    WL_CHECK(t, wasi_component_func_type_result_count(nested_component, export_type_index) == 1u);
    WL_CHECK(t, wasi_component_func_type_result_code(nested_component, export_type_index, 0u) == 0x79u);

    parent_instance = wasi_instantiate(parent_component);
    WL_REQUIRE_MSG(t, parent_instance != NULL, "wasi_instantiate parent failed: %s", engine.error_msg);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(parent_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(parent_instance);
    wasi_free_component(parent_component);
    wasi_free_instance(source_instance);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_outer_type_aliases_through_imported_type_args) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_test_builder_t grandparent_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* grandparent_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* grandparent_instance;
    wasi_instance_t* parent_instance;
    wasi_instance_t* child_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_u32_component(&source_component_bytes, &source_module_bytes);
    wasi_test_build_outer_type_alias_child_component(&child_component_bytes);
    wasi_test_build_outer_type_alias_importing_parent_component(&parent_component_bytes, &child_component_bytes);
    wasi_test_build_grandparent_outer_imported_type_alias_component(&grandparent_component_bytes,
                                                                    &parent_component_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    err = wasi_bind_import_func(&engine, "wasi:test/ops/inc@0.3.0", source_instance, "echo");
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_func failed: %s", engine.error_msg);

    grandparent_component = wasi_load(&engine, grandparent_component_bytes.buf, grandparent_component_bytes.len);
    WL_REQUIRE_MSG(t, grandparent_component != NULL, "wasi_load grandparent failed: %s", engine.error_msg);

    grandparent_instance = wasi_instantiate(grandparent_component);
    WL_REQUIRE_MSG(t, grandparent_instance != NULL, "wasi_instantiate grandparent failed: %s", engine.error_msg);
    WL_REQUIRE(t, grandparent_instance->num_component_instances == 1u);

    parent_instance = grandparent_instance->component_instances[0].ref.instance;
    WL_REQUIRE(t, parent_instance != NULL);
    WL_REQUIRE(t, parent_instance->num_imports == 2u);
    WL_CHECK(t, parent_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_TYPE);
    WL_CHECK(t, parent_instance->imports[0].of.type_ref.component == grandparent_component);
    WL_CHECK(t, parent_instance->imports[0].of.type_ref.type_index == 0u);
    WL_CHECK(t, parent_instance->imports[1].kind == WASI__COMPONENT_IMPORT_RUNTIME_FUNC);
    WL_REQUIRE(t, parent_instance->num_component_instances == 1u);

    child_instance = parent_instance->component_instances[0].ref.instance;
    WL_REQUIRE(t, child_instance != NULL);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(child_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call child echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(grandparent_instance);
    wasi_free_component(grandparent_component);
    wasi_free_instance(source_instance);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_outer_instance_aliases) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* parent_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* parent_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_u32_component(&source_component_bytes, &source_module_bytes);
    wasi_test_build_outer_instance_alias_child_component(&child_component_bytes);
    wasi_test_build_parent_outer_instance_alias_component(&parent_component_bytes, &child_component_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    err = wasi_bind_import_instance(&engine, "wasi:test/ops@0.3.0", source_instance);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_instance failed: %s", engine.error_msg);

    parent_component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, parent_component != NULL, "wasi_load parent failed: %s", engine.error_msg);
    parent_instance = wasi_instantiate(parent_component);
    WL_REQUIRE_MSG(t, parent_instance != NULL, "wasi_instantiate parent failed: %s", engine.error_msg);
    WL_CHECK(t, parent_instance->num_component_instances == 1u);
    WL_REQUIRE(t, parent_instance->component_instances[0].ref.instance != NULL);
    WL_CHECK(t, parent_instance->component_instances[0].ref.instance->parent_instance == parent_instance);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(parent_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(parent_instance);
    wasi_free_component(parent_component);
    wasi_free_instance(source_instance);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_outer_component_aliases) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* parent_component;
    wasi_instance_t* parent_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_u32_component(&source_component_bytes, &source_module_bytes);
    wasi_test_build_outer_component_alias_child_component(&child_component_bytes);
    wasi_test_build_parent_outer_component_alias_component(&parent_component_bytes, &child_component_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);

    err = wasi_bind_import_component(&engine, "wasi:test/components@0.3.0", source_component);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_component failed: %s", engine.error_msg);

    parent_component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, parent_component != NULL, "wasi_load parent failed: %s", engine.error_msg);
    parent_instance = wasi_instantiate(parent_component);
    WL_REQUIRE_MSG(t, parent_instance != NULL, "wasi_instantiate parent failed: %s", engine.error_msg);
    WL_CHECK(t, parent_instance->num_component_instances == 1u);
    WL_REQUIRE(t, parent_instance->component_instances[0].ref.instance != NULL);
    WL_CHECK(t, parent_instance->component_instances[0].ref.instance->parent_instance == parent_instance);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(parent_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(parent_instance);
    wasi_free_component(parent_component);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_bound_instance_imports) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t importing_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* importing_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* importing_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_u32_component(&source_component_bytes, &source_module_bytes);
    wasi_test_build_imported_instance_alias_component(&importing_component_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    err = wasi_bind_import_instance(&engine, "wasi:test/ops@0.3.0", source_instance);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_instance failed: %s", engine.error_msg);

    importing_component = wasi_load(&engine, importing_component_bytes.buf, importing_component_bytes.len);
    WL_REQUIRE_MSG(t, importing_component != NULL, "wasi_load importing failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_import_count(importing_component) == 1u);
    WL_CHECK(t, wasi_component_import_kind(importing_component, 0u) == WASI_COMPONENT_EXTERN_KIND_INSTANCE);
    WL_CHECK(t, strcmp(wasi_component_import_name(importing_component, 0u), "wasi:test/ops@0.3.0") == 0);

    importing_instance = wasi_instantiate(importing_component);
    WL_REQUIRE_MSG(t, importing_instance != NULL, "wasi_instantiate importing failed: %s", engine.error_msg);
    WL_CHECK(t, importing_instance->num_imports == 1u);
    WL_CHECK(t, importing_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_INSTANCE);
    WL_CHECK(t, importing_instance->imports[0].of.instance_ref.instance == source_instance);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(importing_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(importing_instance);
    wasi_free_component(importing_component);
    wasi_free_instance(source_instance);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_bound_function_imports) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t importing_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* importing_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* importing_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_u32_component(&source_component_bytes, &source_module_bytes);
    wasi_test_build_imported_func_alias_component(&importing_component_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    err = wasi_bind_import_func(&engine, "wasi:test/ops/inc@0.3.0", source_instance, "echo");
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_func failed: %s", engine.error_msg);

    importing_component = wasi_load(&engine, importing_component_bytes.buf, importing_component_bytes.len);
    WL_REQUIRE_MSG(t, importing_component != NULL, "wasi_load importing failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_import_count(importing_component) == 1u);
    WL_CHECK(t, wasi_component_import_kind(importing_component, 0u) == WASI_COMPONENT_EXTERN_KIND_FUNC);
    WL_CHECK(t, strcmp(wasi_component_import_name(importing_component, 0u), "wasi:test/ops/inc@0.3.0") == 0);

    importing_instance = wasi_instantiate(importing_component);
    WL_REQUIRE_MSG(t, importing_instance != NULL, "wasi_instantiate importing failed: %s", engine.error_msg);
    WL_CHECK(t, importing_instance->num_imports == 1u);
    WL_CHECK(t, importing_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_FUNC);
    WL_CHECK(t, importing_instance->imports[0].of.func.instance == source_instance);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(importing_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(importing_instance);
    wasi_free_component(importing_component);
    wasi_free_instance(source_instance);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_bound_component_imports) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_test_builder_t importing_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* importing_component;
    wasi_instance_t* importing_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_u32_component(&source_component_bytes, &source_module_bytes);
    wasi_test_build_imported_component_alias_component(&importing_component_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);

    err = wasi_bind_import_component(&engine, "wasi:test/components@0.3.0", source_component);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_component failed: %s", engine.error_msg);

    importing_component = wasi_load(&engine, importing_component_bytes.buf, importing_component_bytes.len);
    WL_REQUIRE_MSG(t, importing_component != NULL, "wasi_load importing failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_import_count(importing_component) == 1u);
    WL_CHECK(t, wasi_component_import_kind(importing_component, 0u) == WASI_COMPONENT_EXTERN_KIND_COMPONENT);
    WL_CHECK(t, strcmp(wasi_component_import_name(importing_component, 0u), "wasi:test/components@0.3.0") == 0);

    importing_instance = wasi_instantiate(importing_component);
    WL_REQUIRE_MSG(t, importing_instance != NULL, "wasi_instantiate importing failed: %s", engine.error_msg);
    WL_CHECK(t, importing_instance->num_imports == 1u);
    WL_CHECK(t, importing_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_COMPONENT);
    WL_CHECK(t, importing_instance->imports[0].of.component == source_component);
    WL_CHECK(t, importing_instance->num_component_instances == 1u);
    WL_CHECK(t, importing_instance->component_instances[0].ref.kind == WASI__COMPONENT_NAMESPACE_INSTANCE);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(importing_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(importing_instance);
    wasi_free_component(importing_component);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_bound_module_imports) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_module_t* source_module;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_imported_module_component(&component_bytes);

    wasm_runtime_clear_error(engine.runtime);
    source_module = wasm_load(engine.runtime, source_module_bytes.buf, source_module_bytes.len);
    WL_REQUIRE_MSG(t,
                   source_module != NULL,
                   "wasm_load source module failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_import_count(component) == 1u);
    WL_CHECK(t, wasi_component_import_kind(component, 0u) == WASI_COMPONENT_EXTERN_KIND_MODULE);

    err = wasi_bind_import_module(&engine, "wasi:test/modules@0.3.0", source_module);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_module failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->num_imports == 1u);
    WL_CHECK(t, instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_MODULE);
    WL_CHECK(t, instance->imports[0].of.module.module == source_module);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasm_free_module(source_module);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_accepts_component_module_args) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* parent_component;
    wasi_instance_t* parent_instance;
    wasi_instance_t* child_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_imported_module_component(&child_component_bytes);
    wasi_test_build_parent_module_arg_component(&parent_component_bytes,
                                                &child_component_bytes,
                                                &source_module_bytes);

    parent_component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, parent_component != NULL, "wasi_load failed: %s", engine.error_msg);

    parent_instance = wasi_instantiate(parent_component);
    WL_REQUIRE_MSG(t, parent_instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_REQUIRE(t, parent_instance->num_component_instances == 1u);

    child_instance = parent_instance->component_instances[0].ref.instance;
    WL_REQUIRE(t, child_instance != NULL);
    WL_REQUIRE(t, child_instance->num_imports == 1u);
    WL_CHECK(t, child_instance->imports[0].kind == WASI__COMPONENT_IMPORT_RUNTIME_MODULE);
    WL_CHECK(t, child_instance->imports[0].of.module.module == wasi_component_core_module_at(parent_component, 0u));

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(parent_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(parent_instance);
    wasi_free_component(parent_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_resolves_outer_core_module_aliases) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* parent_component;
    wasi_instance_t* parent_instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_outer_core_module_alias_child_component(&child_component_bytes);
    wasi_test_build_parent_outer_core_module_alias_component(&parent_component_bytes,
                                                             &child_component_bytes,
                                                             &source_module_bytes);

    parent_component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, parent_component != NULL, "wasi_load failed: %s", engine.error_msg);

    parent_instance = wasi_instantiate(parent_component);
    WL_REQUIRE_MSG(t, parent_instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, parent_instance->num_component_instances == 1u);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(parent_instance, "echo", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call echo failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(parent_instance);
    wasi_free_component(parent_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_links_canon_lower_from_nested_component_instance) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t caller_module_bytes;
    wasi_test_builder_t child_component_bytes;
    wasi_test_builder_t parent_component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_core_instance_source_module(&source_module_bytes);
    wasi_test_build_core_instance_importing_module(&caller_module_bytes);
    wasi_test_build_core_instance_u32_component(&child_component_bytes, &source_module_bytes);
    wasi_test_build_nested_component_lower_link_component(&parent_component_bytes,
                                                          &child_component_bytes,
                                                          &caller_module_bytes);

    component = wasi_load(&engine, parent_component_bytes.buf, parent_component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_nested_component_count(component) == 1u);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 2u);
    WL_CHECK(t, wasi_component_canon_count(component) == 2u);
    WL_CHECK(t, wasi_component_canon_kind(component, 0u) == WASI_COMPONENT_CANON_KIND_LOWER);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->num_component_instances == 1u);
    WL_CHECK(t, instance->core_module_index == 0u);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 99u;

    err = wasi_call(instance, "run", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call run failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 100u);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_links_canon_lower_resource_roundtrip) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t caller_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_resource_type_t resource_type;
    wasi_value_t args[1];
    wasi_value_t results[1];
    uint32_t handle = UINT32_MAX;
    uint32_t representation_value = 91u;
    void* representation = NULL;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_core_module(&source_module_bytes);
    wasi_test_build_core_instance_resource_importing_module(&caller_module_bytes);
    wasi_test_build_canon_lower_resource_link_component(&component_bytes,
                                                        &source_module_bytes,
                                                        &caller_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 3u);
    WL_CHECK(t, wasi_component_core_instance_kind(component, 1u) == WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS);
    WL_CHECK(t, wasi_component_alias_count(component) == 1u);
    WL_CHECK(t, wasi_component_canon_count(component) == 2u);
    WL_CHECK(t, wasi_component_canon_kind(component, 1u) == WASI_COMPONENT_CANON_KIND_LOWER);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         NULL,
                                         NULL);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_resource_new(instance, 0u, (void*)(uintptr_t)representation_value, &handle) == WASI_OK,
                   "wasi_resource_new failed: %s",
                   engine.error_msg);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_OWN;
    args[0].of.resource.type_index = 0u;
    args[0].of.resource.handle = handle;

    err = wasi_call(instance, "run", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call run failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_OWN);
    WL_CHECK(t, results[0].of.resource.type_index == 0u);

    err = wasi_resource_rep(instance, 0u, results[0].of.resource.handle, &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep failed: %s", engine.error_msg);
    WL_CHECK(t, representation == (void*)(uintptr_t)representation_value);

    err = wasi_resource_drop(instance, 0u, results[0].of.resource.handle);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_drop failed: %s", engine.error_msg);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_links_direct_func_args_to_canon_lower) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t caller_module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_resource_type_t resource_type;
    wasi_value_t args[1];
    wasi_value_t results[1];
    uint32_t handle = UINT32_MAX;
    uint32_t representation_value = 113u;
    void* representation = NULL;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_resource_core_module(&source_module_bytes);
    wasi_test_build_core_instance_resource_importing_module(&caller_module_bytes);
    wasi_test_build_direct_func_canon_lower_resource_link_component(&component_bytes,
                                                                    &source_module_bytes,
                                                                    &caller_module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);
    WL_CHECK(t, wasi_component_core_instance_count(component) == 3u);
    WL_CHECK(t, wasi_component_core_instance_kind(component, 1u) == WASI_COMPONENT_CORE_INSTANCE_KIND_FROM_EXPORTS);
    WL_CHECK(t, wasi_component_core_instance_arg_kind(component, 2u, 0u) == 0x00u);
    WL_CHECK(t, wasi_component_alias_count(component) == 2u);
    WL_CHECK(t, wasi_component_canon_count(component) == 2u);
    WL_CHECK(t, wasi_component_canon_kind(component, 1u) == WASI_COMPONENT_CANON_KIND_LOWER);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);
    WL_CHECK(t, instance->core_module_index == 1u);

    resource_type = wasi_define_resource(&engine,
                                         "wasi:test/resources@0.3.0",
                                         "thing",
                                         NULL,
                                         NULL);
    WL_REQUIRE_MSG(t, resource_type != WASI_RESOURCE_TYPE_INVALID, "wasi_define_resource failed: %s", engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_instance_bind_resource_type(instance, 0u, resource_type) == WASI_OK,
                   "wasi_instance_bind_resource_type failed: %s",
                   engine.error_msg);
    WL_REQUIRE_MSG(t,
                   wasi_resource_new(instance, 0u, (void*)(uintptr_t)representation_value, &handle) == WASI_OK,
                   "wasi_resource_new failed: %s",
                   engine.error_msg);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_OWN;
    args[0].of.resource.type_index = 0u;
    args[0].of.resource.handle = handle;

    err = wasi_call(instance, "run", args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call run failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_OWN);
    WL_CHECK(t, results[0].of.resource.type_index == 0u);

    err = wasi_resource_rep(instance, 0u, results[0].of.resource.handle, &representation);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_resource_rep failed: %s", engine.error_msg);
    WL_CHECK(t, representation == (void*)(uintptr_t)representation_value);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instantiate_executes_zero_arg_component_start) {
    wasi_engine_t engine;
    wasi_test_builder_t source_module_bytes;
    wasi_test_builder_t source_component_bytes;
    wasi_component_t* source_component;
    wasi_component_t* importing_component;
    wasi_instance_t* source_instance;
    wasi_instance_t* importing_instance;
    wasi_value_t result;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_start_state_module(&source_module_bytes);
    wasi_test_build_start_state_component(&source_component_bytes, &source_module_bytes);

    source_component = wasi_load(&engine, source_component_bytes.buf, source_component_bytes.len);
    WL_REQUIRE_MSG(t, source_component != NULL, "wasi_load source failed: %s", engine.error_msg);
    source_instance = wasi_instantiate(source_component);
    WL_REQUIRE_MSG(t, source_instance != NULL, "wasi_instantiate source failed: %s", engine.error_msg);

    err = wasi_bind_import_func(&engine, "run", source_instance, "run");
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_bind_import_func failed: %s", engine.error_msg);

    importing_component = wasi_load(&engine,
                                    wasi_test_component_with_start,
                                    sizeof(wasi_test_component_with_start));
    WL_REQUIRE_MSG(t, importing_component != NULL, "wasi_load importing failed: %s", engine.error_msg);

    importing_instance = wasi_instantiate(importing_component);
    WL_REQUIRE_MSG(t, importing_instance != NULL, "wasi_instantiate importing failed: %s", engine.error_msg);

    memset(&result, 0, sizeof(result));
    err = wasi_call(source_instance, "status", NULL, 0u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call status failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.u32 == 1u);

    wasi_value_destroy(&result);
    wasi_free_instance(importing_instance);
    wasi_free_component(importing_component);
    wasi_free_instance(source_instance);
    wasi_free_component(source_component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_surfaces_lift_validation_failures) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasi_value_t result;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_scalar_core_module(&module_bytes);
    wasi_test_build_bad_bool_lift_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    memset(&result, 0, sizeof(result));
    err = wasi_call(instance, "const2", NULL, 0u, &result, 1u);
    WL_CHECK(t, err == WASI_ERR_MALFORMED);
    WL_CHECK(t, strstr(engine.error_msg, "bool result") != NULL);

    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_instance_calls_fixed_list_canon_lift_exports) {
    wasi_engine_t engine;
    wasi_test_builder_t module_bytes;
    wasi_test_builder_t component_bytes;
    wasi_component_t* component;
    wasi_instance_t* instance;
    wasm_module_t* core_module;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_value_t flat_items[1];
    wasi_value_t spill_items[17];
    uint32_t i;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    wasi_test_build_fixed_list_lift_component(&component_bytes, &module_bytes);

    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    instance = wasi_instantiate(component);
    WL_REQUIRE_MSG(t, instance != NULL, "wasi_instantiate failed: %s", engine.error_msg);

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(flat_items, 0, sizeof(flat_items));
    flat_items[0].kind = WASI_VALUE_KIND_U32;
    flat_items[0].of.u32 = 0x11223344u;
    arg.kind = WASI_VALUE_KIND_FIXED_LIST;
    arg.of.seq.values = flat_items;
    arg.of.seq.len = 1u;
    arg.of.seq.owned = 0;

    err = wasi_call(instance, "flat_echo", &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call flat_echo failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_FIXED_LIST);
    WL_CHECK(t, result.of.seq.len == 1u);
    WL_CHECK(t, result.of.seq.values[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.seq.values[0].of.u32 == 0x11223344u);
    wasi_value_destroy(&result);

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(spill_items, 0, sizeof(spill_items));
    for (i = 0; i < 17u; i++) {
        spill_items[i].kind = WASI_VALUE_KIND_U32;
        spill_items[i].of.u32 = 200u + i;
    }
    arg.kind = WASI_VALUE_KIND_FIXED_LIST;
    arg.of.seq.values = spill_items;
    arg.of.seq.len = 17u;
    arg.of.seq.owned = 0;

    err = wasi_call(instance, "spill_echo", &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_call spill_echo failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_FIXED_LIST);
    WL_CHECK(t, result.of.seq.len == 17u);
    for (i = 0; i < 17u; i++) {
        WL_CHECK(t, result.of.seq.values[i].kind == WASI_VALUE_KIND_U32);
        WL_CHECK(t, result.of.seq.values[i].of.u32 == 200u + i);
    }

    core_module = wasi_component_core_module_at(component, 0u);
    WL_REQUIRE(t, core_module != NULL);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 4, 68);

    wasi_value_destroy(&result);
    wasi_free_instance(instance);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_record_and_list) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t args[1];
    wasi_value_t results[1];
    wasi_value_t record_fields[3];
    wasi_value_t list_items[3];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    memset(record_fields, 0, sizeof(record_fields));
    memset(list_items, 0, sizeof(list_items));

    list_items[0].kind = WASI_VALUE_KIND_U8;
    list_items[0].of.u8 = 1u;
    list_items[1].kind = WASI_VALUE_KIND_U8;
    list_items[1].of.u8 = 2u;
    list_items[2].kind = WASI_VALUE_KIND_U8;
    list_items[2].of.u8 = 3u;

    record_fields[0].kind = WASI_VALUE_KIND_U32;
    record_fields[0].of.u32 = 123u;
    record_fields[1].kind = WASI_VALUE_KIND_STRING;
    record_fields[1].of.string.data = (char*)"hi";
    record_fields[1].of.string.len = 2u;
    record_fields[1].of.string.owned = 0;
    record_fields[2].kind = WASI_VALUE_KIND_LIST;
    record_fields[2].of.seq.values = list_items;
    record_fields[2].of.seq.len = 3u;
    record_fields[2].of.seq.owned = 0;

    args[0].kind = WASI_VALUE_KIND_RECORD;
    args[0].of.seq.values = record_fields;
    args[0].of.seq.len = 3u;
    args[0].of.seq.owned = 0;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_RECORD, core_module, "record_echo", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call record failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 4, 20);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_RECORD);
    WL_CHECK(t, results[0].of.seq.len == 3u);
    WL_CHECK(t, results[0].of.seq.values[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.seq.values[0].of.u32 == 123u);
    WL_CHECK(t, results[0].of.seq.values[1].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, results[0].of.seq.values[1].of.string.len == 2u);
    WL_CHECK(t, memcmp(results[0].of.seq.values[1].of.string.data, "hi", 2u) == 0);
    WL_CHECK(t, results[0].of.seq.values[2].kind == WASI_VALUE_KIND_LIST);
    WL_CHECK(t, results[0].of.seq.values[2].of.seq.len == 3u);
    WL_CHECK(t, results[0].of.seq.values[2].of.seq.values[0].of.u8 == 1u);
    WL_CHECK(t, results[0].of.seq.values[2].of.seq.values[1].of.u8 == 2u);
    WL_CHECK(t, results[0].of.seq.values[2].of.seq.values[2].of.u8 == 3u);

    wasi_value_destroy(&results[0]);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_tuple) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_value_t tuple_fields[3];
    wasi_value_t list_items[3];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(tuple_fields, 0, sizeof(tuple_fields));
    memset(list_items, 0, sizeof(list_items));

    list_items[0].kind = WASI_VALUE_KIND_U8;
    list_items[0].of.u8 = 4u;
    list_items[1].kind = WASI_VALUE_KIND_U8;
    list_items[1].of.u8 = 5u;
    list_items[2].kind = WASI_VALUE_KIND_U8;
    list_items[2].of.u8 = 6u;

    tuple_fields[0].kind = WASI_VALUE_KIND_U32;
    tuple_fields[0].of.u32 = 321u;
    tuple_fields[1].kind = WASI_VALUE_KIND_STRING;
    tuple_fields[1].of.string.data = (char*)"yo";
    tuple_fields[1].of.string.len = 2u;
    tuple_fields[1].of.string.owned = 0;
    tuple_fields[2].kind = WASI_VALUE_KIND_LIST;
    tuple_fields[2].of.seq.values = list_items;
    tuple_fields[2].of.seq.len = 3u;
    tuple_fields[2].of.seq.owned = 0;

    arg.kind = WASI_VALUE_KIND_TUPLE;
    arg.of.seq.values = tuple_fields;
    arg.of.seq.len = 3u;
    arg.of.seq.owned = 0;

    err = wasi_canon_call(component,
                          WASI_TEST_COMPOUND_TYPE_TUPLE,
                          core_module,
                          "tuple_echo",
                          NULL,
                          &arg,
                          1u,
                          &result,
                          1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call tuple failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 4, 20);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_TUPLE);
    WL_CHECK(t, result.of.seq.len == 3u);
    WL_CHECK(t, result.of.seq.values[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.seq.values[0].of.u32 == 321u);
    WL_CHECK(t, result.of.seq.values[1].kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, result.of.seq.values[1].of.string.len == 2u);
    WL_CHECK(t, memcmp(result.of.seq.values[1].of.string.data, "yo", 2u) == 0);
    WL_CHECK(t, result.of.seq.values[2].kind == WASI_VALUE_KIND_LIST);
    WL_CHECK(t, result.of.seq.values[2].of.seq.len == 3u);
    WL_CHECK(t, result.of.seq.values[2].of.seq.values[0].of.u8 == 4u);
    WL_CHECK(t, result.of.seq.values[2].of.seq.values[1].of.u8 == 5u);
    WL_CHECK(t, result.of.seq.values[2].of.seq.values[2].of.u8 == 6u);

    wasi_value_destroy(&result);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_flags) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    uint32_t flag_words[2] = { 0x00000005u, 0x80000000u };
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    arg.kind = WASI_VALUE_KIND_FLAGS;
    arg.of.flags.words = flag_words;
    arg.of.flags.len = 2u;
    arg.of.flags.owned = 0;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_FLAGS, core_module, "flags_echo", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call flags failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 4, 8);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_FLAGS);
    WL_CHECK(t, result.of.flags.len == 2u);
    WL_CHECK(t, result.of.flags.words[0] == flag_words[0]);
    WL_CHECK(t, result.of.flags.words[1] == flag_words[1]);

    wasi_value_destroy(&result);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_spills_large_param_lists) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t args[17];
    wasi_value_t result;
    uint32_t i;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(args, 0, sizeof(args));
    memset(&result, 0, sizeof(result));
    for (i = 0; i < 17u; i++) {
        args[i].kind = WASI_VALUE_KIND_U32;
        args[i].of.u32 = i + 1u;
    }

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_SPILL_SUM, core_module, "spill_sum", NULL, args, 17u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call spill_sum failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 4, 68);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.u32 == 18u);

    wasi_value_destroy(&result);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_option) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_value_t payload;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(&payload, 0, sizeof(payload));

    payload.kind = WASI_VALUE_KIND_STRING;
    payload.of.string.data = (char*)"ok";
    payload.of.string.len = 2u;
    payload.of.string.owned = 0;

    arg.kind = WASI_VALUE_KIND_OPTION;
    arg.of.variant.case_index = 1u;
    arg.of.variant.has_payload = 1;
    arg.of.variant.payload = &payload;
    arg.of.variant.payload_owned = 0;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_OPTION, core_module, "option_echo", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call option_echo failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 4, 12);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_OPTION);
    WL_CHECK(t, result.of.variant.case_index == 1u);
    WL_CHECK(t, result.of.variant.has_payload);
    WL_REQUIRE(t, result.of.variant.payload != NULL);
    WL_CHECK(t, result.of.variant.payload->kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, result.of.variant.payload->of.string.len == 2u);
    WL_CHECK(t, memcmp(result.of.variant.payload->of.string.data, "ok", 2u) == 0);

    wasi_value_destroy(&result);
    memset(&result, 0, sizeof(result));
    arg.of.variant.case_index = 0u;
    arg.of.variant.has_payload = 0;
    arg.of.variant.payload = NULL;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_OPTION, core_module, "option_echo", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call option_echo none failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_OPTION);
    WL_CHECK(t, result.of.variant.case_index == 0u);
    WL_CHECK(t, !result.of.variant.has_payload);
    WL_CHECK(t, result.of.variant.payload == NULL);

    wasi_value_destroy(&result);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_result) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_value_t payload;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(&payload, 0, sizeof(payload));

    payload.kind = WASI_VALUE_KIND_U32;
    payload.of.u32 = 77u;

    arg.kind = WASI_VALUE_KIND_RESULT;
    arg.of.variant.case_index = 0u;
    arg.of.variant.has_payload = 1;
    arg.of.variant.payload = &payload;
    arg.of.variant.payload_owned = 0;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_RESULT, core_module, "result_echo", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call result_echo ok failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 4, 12);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_RESULT);
    WL_CHECK(t, result.of.variant.case_index == 0u);
    WL_CHECK(t, result.of.variant.has_payload);
    WL_REQUIRE(t, result.of.variant.payload != NULL);
    WL_CHECK(t, result.of.variant.payload->kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.variant.payload->of.u32 == 77u);

    wasi_value_destroy(&result);
    memset(&result, 0, sizeof(result));
    memset(&payload, 0, sizeof(payload));
    payload.kind = WASI_VALUE_KIND_STRING;
    payload.of.string.data = (char*)"err";
    payload.of.string.len = 3u;
    payload.of.string.owned = 0;
    arg.of.variant.case_index = 1u;
    arg.of.variant.payload = &payload;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_RESULT, core_module, "result_echo", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call result_echo err failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_RESULT);
    WL_CHECK(t, result.of.variant.case_index == 1u);
    WL_CHECK(t, result.of.variant.has_payload);
    WL_REQUIRE(t, result.of.variant.payload != NULL);
    WL_CHECK(t, result.of.variant.payload->kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, result.of.variant.payload->of.string.len == 3u);
    WL_CHECK(t, memcmp(result.of.variant.payload->of.string.data, "err", 3u) == 0);

    wasi_value_destroy(&result);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_variant) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_value_t payload;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(&payload, 0, sizeof(payload));

    payload.kind = WASI_VALUE_KIND_U32;
    payload.of.u32 = 99u;

    arg.kind = WASI_VALUE_KIND_VARIANT;
    arg.of.variant.case_index = 1u;
    arg.of.variant.has_payload = 1;
    arg.of.variant.payload = &payload;
    arg.of.variant.payload_owned = 0;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_VARIANT, core_module, "variant_echo", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call variant_echo num failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 4, 12);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_VARIANT);
    WL_CHECK(t, result.of.variant.case_index == 1u);
    WL_CHECK(t, result.of.variant.has_payload);
    WL_REQUIRE(t, result.of.variant.payload != NULL);
    WL_CHECK(t, result.of.variant.payload->kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.variant.payload->of.u32 == 99u);

    wasi_value_destroy(&result);
    memset(&result, 0, sizeof(result));
    memset(&payload, 0, sizeof(payload));
    payload.kind = WASI_VALUE_KIND_STRING;
    payload.of.string.data = (char*)"yo";
    payload.of.string.len = 2u;
    payload.of.string.owned = 0;
    arg.of.variant.case_index = 2u;
    arg.of.variant.payload = &payload;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_VARIANT, core_module, "variant_echo", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call variant_echo text failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_VARIANT);
    WL_CHECK(t, result.of.variant.case_index == 2u);
    WL_CHECK(t, result.of.variant.has_payload);
    WL_REQUIRE(t, result.of.variant.payload != NULL);
    WL_CHECK(t, result.of.variant.payload->kind == WASI_VALUE_KIND_STRING);
    WL_CHECK(t, result.of.variant.payload->of.string.len == 2u);
    WL_CHECK(t, memcmp(result.of.variant.payload->of.string.data, "yo", 2u) == 0);

    wasi_value_destroy(&result);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_joined_variant) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_value_t payload;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(&payload, 0, sizeof(payload));

    payload.kind = WASI_VALUE_KIND_U32;
    payload.of.u32 = 99u;

    arg.kind = WASI_VALUE_KIND_VARIANT;
    arg.of.variant.case_index = 1u;
    arg.of.variant.has_payload = 1;
    arg.of.variant.payload = &payload;
    arg.of.variant.payload_owned = 0;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_VARIANT_JOIN, core_module, "variant_join_echo", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call variant_join_echo u32 failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 8, 16);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_VARIANT);
    WL_CHECK(t, result.of.variant.case_index == 1u);
    WL_CHECK(t, result.of.variant.has_payload);
    WL_REQUIRE(t, result.of.variant.payload != NULL);
    WL_CHECK(t, result.of.variant.payload->kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.variant.payload->of.u32 == 99u);

    wasi_value_destroy(&result);
    memset(&result, 0, sizeof(result));
    memset(&payload, 0, sizeof(payload));

    payload.kind = WASI_VALUE_KIND_U64;
    payload.of.u64 = UINT64_C(0x0123456789ABCDEF);

    arg.of.variant.case_index = 2u;
    arg.of.variant.payload = &payload;

    err = wasi_canon_call(component, WASI_TEST_COMPOUND_TYPE_VARIANT_JOIN, core_module, "variant_join_echo", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call variant_join_echo u64 failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 8, 16);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_VARIANT);
    WL_CHECK(t, result.of.variant.case_index == 2u);
    WL_CHECK(t, result.of.variant.has_payload);
    WL_REQUIRE(t, result.of.variant.payload != NULL);
    WL_CHECK(t, result.of.variant.payload->kind == WASI_VALUE_KIND_U64);
    WL_CHECK(t, result.of.variant.payload->of.u64 == UINT64_C(0x0123456789ABCDEF));

    wasi_value_destroy(&result);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_flat_fixed_list) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_value_t items[1];
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(items, 0, sizeof(items));

    items[0].kind = WASI_VALUE_KIND_U32;
    items[0].of.u32 = 0x12345678u;

    arg.kind = WASI_VALUE_KIND_FIXED_LIST;
    arg.of.seq.values = items;
    arg.of.seq.len = 1u;
    arg.of.seq.owned = 0;

    err = wasi_canon_call(component,
                          WASI_TEST_COMPOUND_TYPE_FIXED_LIST_FLAT,
                          core_module,
                          "fixed_list_flat_echo",
                          NULL,
                          &arg,
                          1u,
                          &result,
                          1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call fixed_list_flat_echo failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 0, 0);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_FIXED_LIST);
    WL_CHECK(t, result.of.seq.len == 1u);
    WL_CHECK(t, result.of.seq.values[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, result.of.seq.values[0].of.u32 == 0x12345678u);

    wasi_value_destroy(&result);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_roundtrips_spilled_fixed_list) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_value_t items[17];
    uint32_t i;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(items, 0, sizeof(items));

    for (i = 0; i < 17u; i++) {
        items[i].kind = WASI_VALUE_KIND_U32;
        items[i].of.u32 = 100u + i;
    }

    arg.kind = WASI_VALUE_KIND_FIXED_LIST;
    arg.of.seq.values = items;
    arg.of.seq.len = 17u;
    arg.of.seq.owned = 0;

    err = wasi_canon_call(component,
                          WASI_TEST_COMPOUND_TYPE_FIXED_LIST_SPILL,
                          core_module,
                          "fixed_list_spill_echo",
                          NULL,
                          &arg,
                          1u,
                          &result,
                          1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call fixed_list_spill_echo failed: %s", engine.error_msg);
    wasi_test_expect_realloc_args(t, core_module, 0, 0, 4, 68);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_FIXED_LIST);
    WL_CHECK(t, result.of.seq.len == 17u);
    for (i = 0; i < 17u; i++) {
        WL_CHECK(t, result.of.seq.values[i].kind == WASI_VALUE_KIND_U32);
        WL_CHECK(t, result.of.seq.values[i].of.u32 == 100u + i);
    }

    wasi_value_destroy(&result);
    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_rejects_fixed_list_length_mismatch) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_value_t flat_items[2];
    wasi_value_t spill_items[16];
    uint32_t i;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_compound_types_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_compound_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    memset(flat_items, 0, sizeof(flat_items));
    flat_items[0].kind = WASI_VALUE_KIND_U32;
    flat_items[0].of.u32 = 1u;
    flat_items[1].kind = WASI_VALUE_KIND_U32;
    flat_items[1].of.u32 = 2u;
    arg.kind = WASI_VALUE_KIND_FIXED_LIST;
    arg.of.seq.values = flat_items;
    arg.of.seq.len = 2u;
    arg.of.seq.owned = 0;

    err = wasi_canon_call(component,
                          WASI_TEST_COMPOUND_TYPE_FIXED_LIST_FLAT,
                          core_module,
                          "fixed_list_flat_echo",
                          NULL,
                          &arg,
                          1u,
                          &result,
                          1u);
    WL_CHECK(t, err == WASI_ERR_INVALID_ARGUMENT);
    WL_CHECK(t, strstr(engine.error_msg, "fixed-list argument length mismatch") != NULL);

    memset(&arg, 0, sizeof(arg));
    memset(spill_items, 0, sizeof(spill_items));
    for (i = 0; i < 16u; i++) {
        spill_items[i].kind = WASI_VALUE_KIND_U32;
        spill_items[i].of.u32 = 100u + i;
    }
    arg.kind = WASI_VALUE_KIND_FIXED_LIST;
    arg.of.seq.values = spill_items;
    arg.of.seq.len = 16u;
    arg.of.seq.owned = 0;

    err = wasi_canon_call(component,
                          WASI_TEST_COMPOUND_TYPE_FIXED_LIST_SPILL,
                          core_module,
                          "fixed_list_spill_echo",
                          NULL,
                          &arg,
                          1u,
                          &result,
                          1u);
    WL_CHECK(t, err == WASI_ERR_INVALID_ARGUMENT);
    WL_CHECK(t, strstr(engine.error_msg, "fixed-list argument length mismatch") != NULL);

    wasm_free_module(core_module);
    wasi_free_component(component);
    wasi_destroy(&engine);
}

WL_TEST(test_wasi_canon_call_validates_enum_results) {
    wasi_engine_t engine;
    wasi_component_t* component;
    wasm_module_t* core_module;
    wasi_test_builder_t component_bytes;
    wasi_test_builder_t module_bytes;
    wasi_value_t arg;
    wasi_value_t result;
    wasi_error_t err;

    err = wasi_init(&engine, NULL);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_init failed: %s", engine.error_msg);

    wasi_test_build_enum_validation_component(&component_bytes);
    component = wasi_load(&engine, component_bytes.buf, component_bytes.len);
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_enum_validation_core_module(&module_bytes);
    core_module = wasm_load(engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   wasm_runtime_error_message(engine.runtime));

    memset(&arg, 0, sizeof(arg));
    memset(&result, 0, sizeof(result));
    arg.kind = WASI_VALUE_KIND_ENUM;
    arg.of.enum_case = 1u;

    err = wasi_canon_call(component, WASI_TEST_ENUM_TYPE_ECHO, core_module, "id32", NULL, &arg, 1u, &result, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call enum echo failed: %s", engine.error_msg);
    WL_CHECK(t, result.kind == WASI_VALUE_KIND_ENUM);
    WL_CHECK(t, result.of.enum_case == 1u);

    wasi_value_destroy(&result);
    memset(&result, 0, sizeof(result));

    err = wasi_canon_call(component, WASI_TEST_ENUM_TYPE_BAD_FLAT, core_module, "const2", NULL, NULL, 0u, &result, 1u);
    WL_CHECK(t, err == WASI_ERR_MALFORMED);
    WL_CHECK(t, strstr(engine.error_msg, "enum result case index out of range") != NULL);

    err = wasi_canon_call(component, WASI_TEST_ENUM_TYPE_BAD_SPILLED, core_module, "bad_enum_record", NULL, NULL, 0u, &result, 1u);
    WL_CHECK(t, err == WASI_ERR_MALFORMED);
    WL_CHECK(t, strstr(engine.error_msg, "enum result case index out of range") != NULL);

    wasm_free_module(core_module);
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
        WL_TEST_CASE(test_wasi_parses_fixed_list_defined_types),
        WL_TEST_CASE(test_wasi_parses_nested_core_types_in_type_space),
        WL_TEST_CASE(test_wasi_retains_component_type_declarations),
        WL_TEST_CASE(test_wasi_parses_top_level_core_type_sections),
        WL_TEST_CASE(test_wasi_retains_core_type_payloads),
        WL_TEST_CASE(test_wasi_resolves_versioned_interface_names),
        WL_TEST_CASE(test_wasi_parses_component_core_instances),
        WL_TEST_CASE(test_wasi_parses_component_instances),
        WL_TEST_CASE(test_wasi_extracts_nested_components),
        WL_TEST_CASE(test_wasi_parses_alias_variants),
        WL_TEST_CASE(test_wasi_parses_component_start),
        WL_TEST_CASE(test_wasi_tracks_component_source_offsets),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_scalars),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_utf8_strings),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_utf16_strings),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_latin1_utf16_strings),
        WL_TEST_CASE(test_wasi_canon_call_rejects_invalid_scalar_values),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_record_and_list),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_tuple),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_flags),
        WL_TEST_CASE(test_wasi_canon_call_spills_large_param_lists),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_option),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_result),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_variant),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_joined_variant),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_flat_fixed_list),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_spilled_fixed_list),
        WL_TEST_CASE(test_wasi_canon_call_rejects_fixed_list_length_mismatch),
        WL_TEST_CASE(test_wasi_canon_call_validates_enum_results),
        WL_TEST_CASE(test_wasi_instance_calls_fixed_list_canon_lift_exports),
        WL_TEST_CASE(test_wasi_instance_calls_utf8_canon_lift_exports),
        WL_TEST_CASE(test_wasi_instance_calls_utf16_canon_lift_exports),
        WL_TEST_CASE(test_wasi_instance_calls_latin1_utf16_canon_lift_exports),
        WL_TEST_CASE(test_wasi_resources_new_drop_and_reuse_slots),
        WL_TEST_CASE(test_wasi_instance_calls_own_resource_canon_lift_exports),
        WL_TEST_CASE(test_wasi_resource_own_transfer_rejects_outstanding_borrow),
        WL_TEST_CASE(test_wasi_instance_consumes_own_resource_without_returning_it),
        WL_TEST_CASE(test_wasi_instance_calls_resource_canon_builtins),
        WL_TEST_CASE(test_wasi_instance_runs_component_resource_destructor_on_drop),
        WL_TEST_CASE(test_wasi_instance_runs_component_resource_destructor_on_teardown),
        WL_TEST_CASE(test_wasi_resource_transfer_preserves_origin_destructor),
        WL_TEST_CASE(test_wasi_resource_destructor_resolves_component_alias),
        WL_TEST_CASE(test_wasi_instantiate_allows_trivial_core_instance),
        WL_TEST_CASE(test_wasi_instantiate_resolves_core_instance_function_args),
        WL_TEST_CASE(test_wasi_instantiate_resolves_core_instance_global_args),
        WL_TEST_CASE(test_wasi_instantiate_resolves_core_instance_memory_args),
        WL_TEST_CASE(test_wasi_instantiate_resolves_core_instance_table_args),
        WL_TEST_CASE(test_wasi_instantiate_resolves_core_instance_tag_args),
        WL_TEST_CASE(test_wasi_instantiate_chains_dynamic_core_instance_args),
        WL_TEST_CASE(test_wasi_instantiate_links_core_from_exports_globals),
        WL_TEST_CASE(test_wasi_instantiate_links_core_from_exports_memories),
        WL_TEST_CASE(test_wasi_instantiate_links_core_from_exports_tables),
        WL_TEST_CASE(test_wasi_instantiate_links_core_from_exports_tags),
        WL_TEST_CASE(test_wasi_call_resolves_core_export_alias_lifts),
        WL_TEST_CASE(test_wasi_call_resolves_component_export_alias_lifts),
        WL_TEST_CASE(test_wasi_component_export_func_type_index_handles_preceding_type_imports),
        WL_TEST_CASE(test_wasi_instantiate_executes_nested_component_instances),
        WL_TEST_CASE(test_wasi_instantiate_accepts_component_type_args),
        WL_TEST_CASE(test_wasi_instantiate_resolves_instance_exported_type_args),
        WL_TEST_CASE(test_wasi_instantiate_accepts_component_value_args),
        WL_TEST_CASE(test_wasi_instantiate_rejects_mismatched_component_value_args),
        WL_TEST_CASE(test_wasi_bind_import_value_resolves_top_level_string_imports),
        WL_TEST_CASE(test_wasi_instantiate_resolves_imported_type_backed_value_args),
        WL_TEST_CASE(test_wasi_bind_import_value_transfers_top_level_resource_values),
        WL_TEST_CASE(test_wasi_bind_import_value_transfers_list_resource_values),
        WL_TEST_CASE(test_wasi_bind_import_value_transfers_record_resource_values),
        WL_TEST_CASE(test_wasi_bind_import_value_transfers_variant_resource_values),
        WL_TEST_CASE(test_wasi_bind_import_value_rejects_top_level_borrow_resource_values),
        WL_TEST_CASE(test_wasi_bind_import_value_rejects_list_borrow_resource_values),
        WL_TEST_CASE(test_wasi_instantiate_resolves_outer_func_aliases),
        WL_TEST_CASE(test_wasi_instantiate_resolves_outer_type_aliases_for_export_types),
        WL_TEST_CASE(test_wasi_instantiate_resolves_outer_type_aliases_through_imported_type_args),
        WL_TEST_CASE(test_wasi_instantiate_resolves_outer_instance_aliases),
        WL_TEST_CASE(test_wasi_instantiate_resolves_outer_component_aliases),
        WL_TEST_CASE(test_wasi_instantiate_resolves_bound_instance_imports),
        WL_TEST_CASE(test_wasi_instantiate_resolves_bound_module_imports),
        WL_TEST_CASE(test_wasi_instantiate_resolves_bound_function_imports),
        WL_TEST_CASE(test_wasi_instantiate_resolves_bound_component_imports),
        WL_TEST_CASE(test_wasi_instantiate_accepts_component_module_args),
        WL_TEST_CASE(test_wasi_instantiate_resolves_outer_core_module_aliases),
        WL_TEST_CASE(test_wasi_instantiate_links_canon_lower_from_nested_component_instance),
        WL_TEST_CASE(test_wasi_instantiate_links_canon_lower_resource_roundtrip),
        WL_TEST_CASE(test_wasi_instantiate_links_direct_func_args_to_canon_lower),
        WL_TEST_CASE(test_wasi_instantiate_executes_zero_arg_component_start),
        WL_TEST_CASE(test_wasi_instance_surfaces_lift_validation_failures),
        WL_TEST_CASE(test_wasi_parses_canon_builtins),
        WL_TEST_CASE(test_wasi_parses_current_canon_async_option),
        WL_TEST_CASE(test_wasi_rejects_bad_magic),
    };

    return wl_test_run("wasi", cases, WL_COUNTOF(cases));
}
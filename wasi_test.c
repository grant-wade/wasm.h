#include <math.h>
#include <string.h>

#define WL_IMPL
#include "wl.h"

#define WASI_IMPL
#include "wasi.h"

static const uint8_t wasi_test_core_header[8] = {
    0x00, 0x61, 0x73, 0x6D,
    0x01, 0x00, 0x00, 0x00,
};

static const uint8_t wasi_test_component_header_old[8] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
};

static const uint8_t wasi_test_component_header_new[8] = {
    0x00, 0x61, 0x73, 0x6D,
    0x01, 0x00, 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_sections[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x00, 0x04, 0x03, 'w', 'i', 't',
    0x02, 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_core_module[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x01, 0x08,
    0x00, 0x61, 0x73, 0x6D,
    0x01, 0x00, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_import_export[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x05,
    0x01,
    0x40, 0x00, 0x01, 0x00,
    0x0A, 0x0D,
    0x01,
    0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g',
    0x01, 0x00,
    0x0B, 0x0A,
    0x01,
    0x00, 0x04, 'p', 'i', 'n', 'g',
    0x01, 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_component_instances[] = {
    0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00,
    0x0a, 0x0d, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x05, 0x0f, 0x01, 0x01, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x04, 0x1e,
    0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00,
    0x0a, 0x0d, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x05, 0x0f, 0x01, 0x00, 0x00, 0x01, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_start[] = {
    0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00,
    0x0a, 0x08, 0x01, 0x00, 0x03, 'r', 'u', 'n', 0x01, 0x00,
    0x09, 0x03, 0x00, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_alias_variants[] = {
    0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00,
    0x0a, 0x0d, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x05, 0x0f, 0x01, 0x01, 0x01, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g', 0x01, 0x00,
    0x06, 0x0d, 0x01, 0x01, 0x00, 0x00, 0x08, 'h', 'o', 's', 't', '-', 'l', 'o', 'g',
    0x04, 0x0f, 0x00, 0x61, 0x73, 0x6d, 0x0d, 0x00, 0x01, 0x00,
    0x06, 0x05, 0x01, 0x03, 0x02, 0x01, 0x00,
};

static const uint8_t wasi_test_component_with_core_instances[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x02, 0x12,
    0x02,
    0x01,
    0x01,
    0x03, 'l', 'o', 'g',
    0x00, 0x00,
    0x00,
    0x02,
    0x01,
    0x03, 'd', 'e', 'p',
    0x12, 0x01,
};

static const uint8_t wasi_test_component_with_extended_types[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x28,
    0x0B,
    0x40, 0x00, 0x01, 0x00,
    0x41, 0x00,
    0x42, 0x00,
    0x3F, 0x7F, 0x00,
    0x72, 0x01, 0x01, 'x', 0x79,
    0x71, 0x01, 0x02, 'o', 'k', 0x01, 0x73, 0x00,
    0x70, 0x7D,
    0x6B, 0x73,
    0x6A, 0x01, 0x73, 0x01, 0x79,
    0x65, 0x01, 0x73,
    0x66, 0x01, 0x7D,
};

static const uint8_t wasi_test_component_with_versioned_names[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x05,
    0x01,
    0x40, 0x00, 0x01, 0x00,
    0x0A, 0x17,
    0x01,
    0x00, 0x12, 'w', 'a', 's', 'i', ':', 'c', 'l', 'i', '/', 'r', 'u', 'n', '@', '0', '.', '3', '.', '0',
    0x01, 0x00,
    0x0B, 0x18,
    0x01,
    0x00, 0x12, 'w', 'a', 's', 'i', ':', 'c', 'l', 'i', '/', 'r', 'u', 'n', '@', '0', '.', '3', '.', '0',
    0x01, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_canon_builtins[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x07,
    0x02,
    0x3F, 0x7F, 0x00,
    0x65, 0x01, 0x73,
    0x08, 0x11,
    0x08,
    0x02, 0x00,
    0x03, 0x00,
    0x04, 0x00,
    0x1F,
    0x0D,
    0x0E, 0x01,
    0x15, 0x01,
    0x09, 0x01, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_async_lower_option[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x05,
    0x01,
    0x40, 0x00, 0x01, 0x00,
    0x0A, 0x06,
    0x01,
    0x00, 0x01, 'f',
    0x01, 0x00,
    0x08, 0x06,
    0x01,
    0x01, 0x00, 0x00, 0x01, 0x06,
};

static const uint8_t wasi_test_component_with_nested_core_types[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x24,
    0x02,
    0x41, 0x01, 0x00, 0x50, 0x04,
    0x01, 0x50, 0x00, 0x60, 0x01, 0x7F, 0x01, 0x7F,
    0x00, 0x01, 0x6D, 0x01, 0x66, 0x00, 0x00,
    0x02, 0x10, 0x01, 0x00, 0x00,
    0x03, 0x01, 0x67, 0x00, 0x00,
    0x42, 0x01, 0x00, 0x50, 0x00,
};

static const uint8_t wasi_test_component_with_type_declarations[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x3C,
    0x02,
    0x41, 0x04,
    0x01, 0x40, 0x00, 0x01, 0x00,
    0x02, 0x03, 0x02, 0x01, 0x00,
    0x03, 0x00, 0x03, 'c', 'f', 'g', 0x02, 0x01, 0x73,
    0x04, 0x02, 0x0D, 'p', 'k', 'g', ':', 'i', 'f', 'a', 'c', 'e', '/', 'r', 'u', 'n',
    0x05, '0', '.', '3', '.', '0', 0x01, 0x00,
    0x42, 0x02,
    0x01, 0x70, 0x7D,
    0x04, 0x00, 0x03, 'c', 'f', 'g', 0x02, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_top_level_core_types[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x03, 0x13,
    0x03,
    0x60, 0x00, 0x00,
    0x50, 0x02,
    0x01, 0x60, 0x01, 0x7F, 0x01, 0x7F,
    0x03, 0x01, 0x67, 0x00, 0x00,
    0x5D, 0x01,
};

static const uint8_t wasi_test_component_with_core_type_payloads[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x03, 0x10,
    0x02,
    0x4E, 0x02,
    0x5F, 0x01, 0x78, 0x01,
    0x5E, 0x7E, 0x00,
    0x4F, 0x01, 0x00, 0x60, 0x00, 0x00,
};

static const uint8_t wasi_test_component_with_m2_func_types[] = {
    0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x01, 0x00,
    0x07, 0x60,
    0x0E,
    0x40, 0x01, 0x01, 'x', 0x7F, 0x00, 0x7F,
    0x40, 0x01, 0x01, 'x', 0x77, 0x00, 0x77,
    0x40, 0x01, 0x01, 'x', 0x76, 0x00, 0x76,
    0x40, 0x01, 0x01, 'x', 0x75, 0x00, 0x75,
    0x40, 0x01, 0x01, 'x', 0x74, 0x00, 0x74,
    0x40, 0x01, 0x01, 'x', 0x73, 0x00, 0x73,
    0x40, 0x00, 0x00, 0x7F,
    0x40, 0x01, 0x01, 'x', 0x7E, 0x00, 0x7E,
    0x40, 0x01, 0x01, 'x', 0x7D, 0x00, 0x7D,
    0x40, 0x01, 0x01, 'x', 0x7C, 0x00, 0x7C,
    0x40, 0x01, 0x01, 'x', 0x7B, 0x00, 0x7B,
    0x40, 0x01, 0x01, 'x', 0x7A, 0x00, 0x7A,
    0x40, 0x01, 0x01, 'x', 0x79, 0x00, 0x79,
    0x40, 0x01, 0x01, 'x', 0x78, 0x00, 0x78,
};

enum {
    WASI_TEST_M2_TYPE_BOOL = 0u,
    WASI_TEST_M2_TYPE_U64 = 1u,
    WASI_TEST_M2_TYPE_F32 = 2u,
    WASI_TEST_M2_TYPE_F64 = 3u,
    WASI_TEST_M2_TYPE_CHAR = 4u,
    WASI_TEST_M2_TYPE_STRING = 5u,
    WASI_TEST_M2_TYPE_BAD_BOOL_RESULT = 6u,
    WASI_TEST_M2_TYPE_S8 = 7u,
    WASI_TEST_M2_TYPE_U8 = 8u,
    WASI_TEST_M2_TYPE_S16 = 9u,
    WASI_TEST_M2_TYPE_U16 = 10u,
    WASI_TEST_M2_TYPE_S32 = 11u,
    WASI_TEST_M2_TYPE_U32 = 12u,
    WASI_TEST_M2_TYPE_S64 = 13u,
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
        0x00, 0x61, 0x73, 0x6D,
        0x01, 0x00, 0x00, 0x00,
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

static void wasi_test_build_m2_core_module(wasi_test_builder_t* mod) {
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
        0x20, 0x00,
        0x24, 0x03,
        0x20, 0x01,
        0x24, 0x04,
        0x20, 0x02,
        0x24, 0x05,
        0x20, 0x03,
        0x24, 0x06,
        0x23, 0x00,
        0x21, 0x04,
        0x23, 0x00,
        0x20, 0x03,
        0x6A,
        0x24, 0x00,
        0x20, 0x04,
        0x0B,
    };
    static const uint8_t echo_ops[] = { 0x20, 0x00, 0x20, 0x01, 0x0B };
    static const uint8_t id_ops[] = { 0x20, 0x00, 0x0B };
    static const uint8_t post_echo_ops[] = {
        0x20, 0x00,
        0x24, 0x01,
        0x20, 0x01,
        0x24, 0x02,
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
    wasi_test_emit(&sec, 0x7F); wasi_test_emit(&sec, 0x01); wasi_test_emit(&sec, 0x41); wasi_test_emit(&sec, 0x20); wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F); wasi_test_emit(&sec, 0x01); wasi_test_emit(&sec, 0x41); wasi_test_emit(&sec, 0x00); wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F); wasi_test_emit(&sec, 0x01); wasi_test_emit(&sec, 0x41); wasi_test_emit(&sec, 0x00); wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F); wasi_test_emit(&sec, 0x01); wasi_test_emit(&sec, 0x41); wasi_test_emit(&sec, 0x00); wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F); wasi_test_emit(&sec, 0x01); wasi_test_emit(&sec, 0x41); wasi_test_emit(&sec, 0x00); wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F); wasi_test_emit(&sec, 0x01); wasi_test_emit(&sec, 0x41); wasi_test_emit(&sec, 0x00); wasi_test_emit(&sec, 0x0B);
    wasi_test_emit(&sec, 0x7F); wasi_test_emit(&sec, 0x01); wasi_test_emit(&sec, 0x41); wasi_test_emit(&sec, 0x00); wasi_test_emit(&sec, 0x0B);
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

static void wasi_test_emit_component_header(wasi_test_builder_t* b) {
    static const uint8_t header[] = {
        0x00, 0x61, 0x73, 0x6D,
        0x0D, 0x00, 0x01, 0x00,
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

WL_TEST(test_wasi_canon_call_roundtrips_m2_scalars) {
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
                          wasi_test_component_with_m2_func_types,
                          sizeof(wasi_test_component_with_m2_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_m2_core_module(&module_bytes);
    core_module = wasm_load(&engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   engine.runtime.error_msg[0] ? engine.runtime.error_msg : wasm_error_string(engine.runtime.last_error));

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_BOOL;
    args[0].of.boolean = 1u;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_BOOL, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call bool failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_BOOL);
    WL_CHECK(t, results[0].of.boolean == 1u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.boolean = 0u;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_BOOL, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call bool false failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_BOOL);
    WL_CHECK(t, results[0].of.boolean == 0u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_S8;
    args[0].of.s8 = INT8_MIN;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_S8, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s8 min failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S8);
    WL_CHECK(t, results[0].of.s8 == INT8_MIN);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.s8 = INT8_MAX;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_S8, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s8 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S8);
    WL_CHECK(t, results[0].of.s8 == INT8_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U8;
    args[0].of.u8 = 0u;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_U8, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u8 zero failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U8);
    WL_CHECK(t, results[0].of.u8 == 0u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.u8 = UINT8_MAX;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_U8, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u8 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U8);
    WL_CHECK(t, results[0].of.u8 == UINT8_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_S16;
    args[0].of.s16 = INT16_MIN;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_S16, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s16 min failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S16);
    WL_CHECK(t, results[0].of.s16 == INT16_MIN);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.s16 = INT16_MAX;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_S16, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s16 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S16);
    WL_CHECK(t, results[0].of.s16 == INT16_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U16;
    args[0].of.u16 = 0u;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_U16, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u16 zero failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U16);
    WL_CHECK(t, results[0].of.u16 == 0u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.u16 = UINT16_MAX;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_U16, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u16 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U16);
    WL_CHECK(t, results[0].of.u16 == UINT16_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_S32;
    args[0].of.s32 = INT32_MIN;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_S32, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s32 min failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S32);
    WL_CHECK(t, results[0].of.s32 == INT32_MIN);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.s32 = INT32_MAX;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_S32, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s32 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S32);
    WL_CHECK(t, results[0].of.s32 == INT32_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U32;
    args[0].of.u32 = 0u;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_U32, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u32 zero failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == 0u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.u32 = UINT32_MAX;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_U32, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u32 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U32);
    WL_CHECK(t, results[0].of.u32 == UINT32_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_S64;
    args[0].of.s64 = INT64_MIN;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_S64, core_module, "id64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s64 min failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S64);
    WL_CHECK(t, results[0].of.s64 == INT64_MIN);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.s64 = INT64_MAX;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_S64, core_module, "id64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call s64 max failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_S64);
    WL_CHECK(t, results[0].of.s64 == INT64_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_U64;
    args[0].of.u64 = UINT64_MAX;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_U64, core_module, "id64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call u64 failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_U64);
    WL_CHECK(t, results[0].of.u64 == UINT64_MAX);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_F32;
    args[0].of.f32 = 1.25f;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_F32, core_module, "idf32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call f32 failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_F32);
    WL_CHECK(t, results[0].of.f32 == 1.25f);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.f32 = NAN;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_F32, core_module, "idf32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call f32 nan failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_F32);
    WL_CHECK(t, isnan(results[0].of.f32));
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_F64;
    args[0].of.f64 = 2.5;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_F64, core_module, "idf64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call f64 failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_F64);
    WL_CHECK(t, results[0].of.f64 == 2.5);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.f64 = NAN;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_F64, core_module, "idf64", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call f64 nan failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_F64);
    WL_CHECK(t, isnan(results[0].of.f64));
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_CHAR;
    args[0].of.char32 = 0x0000u;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call char zero failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_CHAR);
    WL_CHECK(t, results[0].of.char32 == 0x0000u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.char32 = 0xD7FFu;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call char d7ff failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_CHAR);
    WL_CHECK(t, results[0].of.char32 == 0xD7FFu);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.char32 = 0xE000u;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
    WL_REQUIRE_MSG(t, err == WASI_OK, "wasi_canon_call char e000 failed: %s", engine.error_msg);
    WL_CHECK(t, results[0].kind == WASI_VALUE_KIND_CHAR);
    WL_CHECK(t, results[0].of.char32 == 0xE000u);
    wasi_value_destroy(&results[0]);

    memset(results, 0, sizeof(results));
    args[0].of.char32 = 0x10FFFFu;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
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
                          wasi_test_component_with_m2_func_types,
                          sizeof(wasi_test_component_with_m2_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_m2_core_module(&module_bytes);
    core_module = wasm_load(&engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   engine.runtime.error_msg[0] ? engine.runtime.error_msg : wasm_error_string(engine.runtime.last_error));

    err = wasi_canon_options_default(&options);
    WL_REQUIRE(t, err == WASI_OK);
    options.post_return_name = "cabi_post_echo";

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_STRING;
    args[0].of.string.data = (char*)"hello";
    args[0].of.string.len = 5u;
    args[0].of.string.owned = 0;

    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_STRING, core_module, "echo", &options, args, 1u, results, 1u);
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
                          wasi_test_component_with_m2_func_types,
                          sizeof(wasi_test_component_with_m2_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_m2_core_module(&module_bytes);
    core_module = wasm_load(&engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   engine.runtime.error_msg[0] ? engine.runtime.error_msg : wasm_error_string(engine.runtime.last_error));

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

    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_STRING, core_module, "echo", &options, args, 1u, results, 1u);
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
                          wasi_test_component_with_m2_func_types,
                          sizeof(wasi_test_component_with_m2_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_m2_core_module(&module_bytes);
    core_module = wasm_load(&engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   engine.runtime.error_msg[0] ? engine.runtime.error_msg : wasm_error_string(engine.runtime.last_error));

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

    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_STRING, core_module, "echo", &options, args, 1u, results, 1u);
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

    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_STRING, core_module, "echo", &options, args, 1u, results, 1u);
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
                          wasi_test_component_with_m2_func_types,
                          sizeof(wasi_test_component_with_m2_func_types));
    WL_REQUIRE_MSG(t, component != NULL, "wasi_load failed: %s", engine.error_msg);

    wasi_test_build_m2_core_module(&module_bytes);
    core_module = wasm_load(&engine.runtime, module_bytes.buf, module_bytes.len);
    WL_REQUIRE_MSG(t, core_module != NULL,
                   "wasm_load failed: %s",
                   engine.runtime.error_msg[0] ? engine.runtime.error_msg : wasm_error_string(engine.runtime.last_error));

    memset(results, 0, sizeof(results));
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_BAD_BOOL_RESULT, core_module, "const2", NULL, NULL, 0u, results, 1u);
    WL_CHECK(t, err == WASI_ERR_MALFORMED);
    WL_CHECK(t, strstr(engine.error_msg, "bool result") != NULL);

    memset(args, 0, sizeof(args));
    memset(results, 0, sizeof(results));
    args[0].kind = WASI_VALUE_KIND_CHAR;
    args[0].of.char32 = 0xD800u;
    err = wasi_canon_call(component, WASI_TEST_M2_TYPE_CHAR, core_module, "id32", NULL, args, 1u, results, 1u);
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

    wasi_test_build_m2_core_module(&module_bytes);
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

    wasi_test_build_m2_core_module(&module_bytes);
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

    wasi_test_build_m2_core_module(&module_bytes);
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

    wasi_test_build_m2_core_module(&module_bytes);
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
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_m2_scalars),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_utf8_strings),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_utf16_strings),
        WL_TEST_CASE(test_wasi_canon_call_roundtrips_latin1_utf16_strings),
        WL_TEST_CASE(test_wasi_canon_call_rejects_invalid_scalar_values),
        WL_TEST_CASE(test_wasi_instance_calls_utf8_canon_lift_exports),
        WL_TEST_CASE(test_wasi_instance_calls_utf16_canon_lift_exports),
        WL_TEST_CASE(test_wasi_instance_calls_latin1_utf16_canon_lift_exports),
        WL_TEST_CASE(test_wasi_instance_surfaces_lift_validation_failures),
        WL_TEST_CASE(test_wasi_parses_canon_builtins),
        WL_TEST_CASE(test_wasi_parses_current_canon_async_option),
        WL_TEST_CASE(test_wasi_rejects_bad_magic),
    };

    return wl_test_run("wasi", cases, WL_COUNTOF(cases));
}
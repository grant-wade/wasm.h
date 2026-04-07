/*
 * test_wasm.c — Test harness for wasm.h single-header runtime
 *
 * Builds hand-crafted Wasm modules as byte arrays and executes them.
 */

#define WL_IMPL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#define WASM_TEST_DUP _dup
#define WASM_TEST_DUP2 _dup2
#define WASM_TEST_CLOSE _close
#define WASM_TEST_FILENO _fileno
#else
#include <unistd.h>
#define WASM_TEST_DUP dup
#define WASM_TEST_DUP2 dup2
#define WASM_TEST_CLOSE close
#define WASM_TEST_FILENO fileno
#endif

#include "wl.h"

typedef struct wasm_test_alloc_header {
    size_t size;
} wasm_test_alloc_header;

typedef struct wasm_test_alloc_stats {
    size_t malloc_calls;
    size_t realloc_calls;
    size_t free_calls;
    size_t calloc_calls;
    size_t live_blocks;
    size_t peak_live_blocks;
} wasm_test_alloc_stats;

static wasm_test_alloc_stats g_wasm_test_alloc_stats;

static void wasm_test_record_live_alloc(void) {
    g_wasm_test_alloc_stats.live_blocks++;
    if (g_wasm_test_alloc_stats.live_blocks > g_wasm_test_alloc_stats.peak_live_blocks)
        g_wasm_test_alloc_stats.peak_live_blocks = g_wasm_test_alloc_stats.live_blocks;
}

static void* wasm_test_alloc_block(size_t size, int zero_fill) {
    wasm_test_alloc_header* header;
    size_t payload_size = size ? size : 1u;

    if (payload_size > ((size_t)-1) - sizeof(*header)) return NULL;

    if (zero_fill)
        header = (wasm_test_alloc_header*)calloc(1, sizeof(*header) + payload_size);
    else
        header = (wasm_test_alloc_header*)malloc(sizeof(*header) + payload_size);
    if (!header) return NULL;

    header->size = payload_size;
    wasm_test_record_live_alloc();
    return (void*)(header + 1);
}

static void* wasm_test_malloc(size_t size) {
    g_wasm_test_alloc_stats.malloc_calls++;
    return wasm_test_alloc_block(size, 0);
}

static void* wasm_test_calloc(size_t count, size_t size) {
    g_wasm_test_alloc_stats.calloc_calls++;
    if (size != 0 && count > ((size_t)-1) / size) return NULL;
    return wasm_test_alloc_block(count * size, 1);
}

static void* wasm_test_realloc(void* ptr, size_t size) {
    wasm_test_alloc_header* header;
    wasm_test_alloc_header* resized;
    size_t payload_size = size ? size : 1u;

    g_wasm_test_alloc_stats.realloc_calls++;
    if (!ptr) return wasm_test_alloc_block(size, 0);
    if (size == 0) {
        free(((wasm_test_alloc_header*)ptr) - 1);
        if (g_wasm_test_alloc_stats.live_blocks > 0) g_wasm_test_alloc_stats.live_blocks--;
        return NULL;
    }
    if (payload_size > ((size_t)-1) - sizeof(*header)) return NULL;

    header = ((wasm_test_alloc_header*)ptr) - 1;
    resized = (wasm_test_alloc_header*)realloc(header, sizeof(*resized) + payload_size);
    if (!resized) return NULL;

    resized->size = payload_size;
    return (void*)(resized + 1);
}

static void wasm_test_free(void* ptr) {
    g_wasm_test_alloc_stats.free_calls++;
    if (!ptr) return;

    free(((wasm_test_alloc_header*)ptr) - 1);
    if (g_wasm_test_alloc_stats.live_blocks > 0) g_wasm_test_alloc_stats.live_blocks--;
}

static wasm_test_alloc_stats wasm_test_alloc_snapshot(void) {
    return g_wasm_test_alloc_stats;
}

#define WASM_MALLOC(sz) wasm_test_malloc(sz)
#define WASM_REALLOC(p, sz) wasm_test_realloc((p), (sz))
#define WASM_FREE(p) wasm_test_free(p)
#define WASM_CALLOC(n, sz) wasm_test_calloc((n), (sz))

#define WASM_IMPL
#include "wasm.h"

static wasm_error_t wasm_test_init_raw(wasm_runtime_t* rt, const wasm_config_t* config) {
    return wasm_init(rt, config);
}

static void wasm_test_init_checked(wl_test_ctx* t, wasm_runtime_t* rt, const char* test_name) {
    wasm_error_t err;

    WL_CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0,
                 "allocator leak before %s: %zu live blocks",
                 test_name,
                 g_wasm_test_alloc_stats.live_blocks);
    err = wasm_test_init_raw(rt, NULL);
    WL_CHECK_MSG(t, err == WASM_OK,
                 "wasm_init failed before %s: %s",
                 test_name,
                 rt->error_msg[0] ? rt->error_msg : wasm_error_string(err));
}

static void wasm_test_init_with_config_checked(wl_test_ctx* t,
                                               wasm_runtime_t* rt,
                                               const wasm_config_t* config,
                                               const char* test_name) {
    wasm_error_t err;

    WL_CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0,
                 "allocator leak before %s: %zu live blocks",
                 test_name,
                 g_wasm_test_alloc_stats.live_blocks);
    err = wasm_test_init_raw(rt, config);
    WL_CHECK_MSG(t, err == WASM_OK,
                 "wasm_init failed before %s: %s",
                 test_name,
                 rt->error_msg[0] ? rt->error_msg : wasm_error_string(err));
}

static void wasm_test_destroy_checked(wl_test_ctx* t, wasm_runtime_t* rt, const char* test_name) {
    wasm_destroy(rt);
    WL_CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0,
                 "allocator leak after %s: %zu live blocks",
                 test_name,
                 g_wasm_test_alloc_stats.live_blocks);
}

static void wasm_test_verify_case_cleanup(wl_test_ctx* t) {
    const char* case_name = (t && t->case_name) ? t->case_name : "<unnamed>";

    WL_CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0,
                 "allocator leak after test %s: %zu live blocks",
                 case_name,
                 g_wasm_test_alloc_stats.live_blocks);
}

#define wasm_init(rt) wasm_test_init_checked(t, (rt), ((t) && (t)->case_name) ? (t)->case_name : __func__)
#define wasm_init_config(rt, config)             \
    wasm_test_init_with_config_checked(t,        \
                                       (rt),     \
                                       (config), \
                                       ((t) && (t)->case_name) ? (t)->case_name : __func__)
#define wasm_destroy(rt) wasm_test_destroy_checked(t, (rt), ((t) && (t)->case_name) ? (t)->case_name : __func__)

#undef WL_TEST
#define WL_TEST(name)                        \
    static void name##_impl(wl_test_ctx* t); \
    static void name(wl_test_ctx* t) {       \
        name##_impl(t);                      \
        wasm_test_verify_case_cleanup(t);    \
    }                                        \
    static void name##_impl(wl_test_ctx* t)

#if defined(_MSC_VER) && _MSC_VER < 1900
#define WASM_TEST_SNPRINTF _snprintf
#else
#define WASM_TEST_SNPRINTF snprintf
#endif

#define WASM_TEST_GC_I31_TAG ((uintptr_t)1u)

static int wasm_test_value_is_i31ref(const wasm_value_t* value) {
    return value && value->type == WASM_TYPE_I31REF && (value->of.gc_ref & WASM_TEST_GC_I31_TAG) != 0;
}

static int32_t wasm_test_gc_i31_get_s(uintptr_t bits) {
    uint32_t raw = (uint32_t)(bits >> 1) & 0x7FFFFFFFu;

    if ((raw & 0x40000000u) != 0) raw |= 0x80000000u;
    return (int32_t)raw;
}

static uint32_t wasm_test_gc_i31_get_u(uintptr_t bits) {
    return (uint32_t)(bits >> 1) & 0x7FFFFFFFu;
}

static uint32_t wasm_test_gc_allocation_count(const wasm_runtime_t* rt) {
    uint32_t count = 0;
    const wasm_gc_header_t* object;

    if (!rt) return 0;
    for (object = rt->gc_allocations; object; object = object->next_alloc) count++;
    return count;
}

static wasm_value_t* wasm_test_gc_struct_field_value(wasm_gc_struct_t* object,
                                                     const wasm_structtype_t* type,
                                                     uint32_t field_index) {
    size_t offset = wasm__gc_struct_field_offset(type, field_index);

    if (!object || !type || offset == (size_t)-1) return NULL;
    return (wasm_value_t*)(((uint8_t*)object) + offset);
}

#define WASM_CHECK_OK(t, err) WL_CHECK_MSG((t), (err) == WASM_OK, "%s", wasm_error_string(err))
#define WASM_CHECK_I32(t, actual, expected) \
    WL_CHECK_MSG((t), (actual) == (expected), "expected %d, got %d", (expected), (actual))
#define WASM_CHECK_I64(t, actual, expected) \
    WL_CHECK_MSG((t), (actual) == (expected), "expected %lld, got %lld", (long long)(expected), (long long)(actual))

/* ── Helper: build minimal wasm module ──────────────────────────── */

typedef struct {
    uint8_t buf[4096];
    uint32_t len;
} wasm_builder_t;

static void emit(wasm_builder_t* b, uint8_t byte) {
    b->buf[b->len++] = byte;
}

static void emit_bytes(wasm_builder_t* b, const uint8_t* data, uint32_t len) {
    memcpy(b->buf + b->len, data, len);
    b->len += len;
}

static void emit_leb128_u32(wasm_builder_t* b, uint32_t val) {
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val) byte |= 0x80;
        emit(b, byte);
    } while (val);
}

static void emit_leb128_i32(wasm_builder_t* b, int32_t val) {
    int more = 1;
    while (more) {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if ((val == 0 && !(byte & 0x40)) || (val == -1 && (byte & 0x40)))
            more = 0;
        else
            byte |= 0x80;
        emit(b, byte);
    }
}

static void emit_leb128_i64(wasm_builder_t* b, int64_t val) {
    int more = 1;
    while (more) {
        uint8_t byte = (uint8_t)(val & 0x7F);
        val >>= 7;
        if ((val == 0 && !(byte & 0x40)) || (val == -1 && (byte & 0x40)))
            more = 0;
        else
            byte |= 0x80;
        emit(b, byte);
    }
}

static void emit_f32(wasm_builder_t* b, float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    emit(b, (uint8_t)(bits & 0xFFu));
    emit(b, (uint8_t)((bits >> 8) & 0xFFu));
    emit(b, (uint8_t)((bits >> 16) & 0xFFu));
    emit(b, (uint8_t)((bits >> 24) & 0xFFu));
}

static void emit_f64(wasm_builder_t* b, double value) {
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    emit(b, (uint8_t)(bits & 0xFFu));
    emit(b, (uint8_t)((bits >> 8) & 0xFFu));
    emit(b, (uint8_t)((bits >> 16) & 0xFFu));
    emit(b, (uint8_t)((bits >> 24) & 0xFFu));
    emit(b, (uint8_t)((bits >> 32) & 0xFFu));
    emit(b, (uint8_t)((bits >> 40) & 0xFFu));
    emit(b, (uint8_t)((bits >> 48) & 0xFFu));
    emit(b, (uint8_t)((bits >> 56) & 0xFFu));
}

static void emit_memarg(wasm_builder_t* b,
                        uint32_t align_log2,
                        int has_memory_index,
                        uint32_t memory_index,
                        uint32_t offset) {
    emit_leb128_u32(b, align_log2 | (has_memory_index ? 0x40u : 0u));
    if (has_memory_index) emit_leb128_u32(b, memory_index);
    emit_leb128_u32(b, offset);
}

/* Emit a section: id + size + content */
static void emit_section(wasm_builder_t* b, uint8_t id, const uint8_t* content, uint32_t len) {
    emit(b, id);
    emit_leb128_u32(b, len);
    emit_bytes(b, content, len);
}

static void emit_custom_section(wasm_builder_t* b,
                                const char* name,
                                const uint8_t* payload,
                                uint32_t payload_len) {
    wasm_builder_t sec = { 0 };
    uint32_t name_len = (uint32_t)strlen(name);

    emit_leb128_u32(&sec, name_len);
    emit_bytes(&sec, (const uint8_t*)name, name_len);
    if (payload_len > 0) emit_bytes(&sec, payload, payload_len);
    emit_section(b, 0, sec.buf, sec.len);
}

static void emit_export_func(wasm_builder_t* b, const char* name, uint32_t index) {
    uint32_t len = (uint32_t)strlen(name);

    emit_leb128_u32(b, len);
    emit_bytes(b, (const uint8_t*)name, len);
    emit(b, 0x00);
    emit_leb128_u32(b, index);
}

static void emit_export_kind(wasm_builder_t* b, const char* name, uint8_t kind, uint32_t index) {
    uint32_t len = (uint32_t)strlen(name);

    emit_leb128_u32(b, len);
    emit_bytes(b, (const uint8_t*)name, len);
    emit(b, kind);
    emit_leb128_u32(b, index);
}

static void emit_import_func(wasm_builder_t* b, const char* module, const char* name, uint32_t type_index) {
    uint32_t module_len = (uint32_t)strlen(module);
    uint32_t name_len = (uint32_t)strlen(name);

    emit_leb128_u32(b, module_len);
    emit_bytes(b, (const uint8_t*)module, module_len);
    emit_leb128_u32(b, name_len);
    emit_bytes(b, (const uint8_t*)name, name_len);
    emit(b, 0x00);
    emit_leb128_u32(b, type_index);
}

static void emit_malformed_uleb128_u32(wasm_builder_t* b) {
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x00);
}

static void emit_malformed_sleb128_i32(wasm_builder_t* b) {
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x10);
}

static void emit_malformed_sleb128_i64(wasm_builder_t* b) {
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x80);
    emit(b, 0x01);
}

static void emit_header(wasm_builder_t* b) {
    /* magic */
    emit(b, 0x00);
    emit(b, 0x61);
    emit(b, 0x73);
    emit(b, 0x6D);
    /* version 1 */
    emit(b, 0x01);
    emit(b, 0x00);
    emit(b, 0x00);
    emit(b, 0x00);
}

static void emit_single_tag_section(wasm_builder_t* b, uint32_t type_index) {
    wasm_builder_t sec = { 0 };

    emit_leb128_u32(&sec, 1);
    emit(&sec, 0x00);
    emit_leb128_u32(&sec, type_index);
    emit_section(b, 13, sec.buf, sec.len);
}

static void emit_simd_op(wasm_builder_t* b, uint32_t subop) {
    emit(b, 0xFD);
    emit_leb128_u32(b, subop);
}

static void emit_v128_const_bytes(wasm_builder_t* b, const uint8_t bytes[16]) {
    emit_simd_op(b, 0x0C);
    emit_bytes(b, bytes, 16);
}

static wasm_value_t wasm_test_v128_from_i8x16(const int8_t lanes[16]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) wasm__v128_set_u8(&value, lane, (uint8_t)lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_i16x8(const int16_t lanes[8]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 8; lane++) wasm__v128_set_i16(&value, lane, lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_i32x4(const int32_t lanes[4]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) wasm__v128_set_i32(&value, lane, lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_i64x2(const int64_t lanes[2]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) wasm__v128_set_i64(&value, lane, lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_f32x4(const float lanes[4]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) wasm__v128_set_f32(&value, lane, lanes[lane]);
    return value;
}

static wasm_value_t wasm_test_v128_from_f64x2(const double lanes[2]) {
    wasm_value_t value = wasm_v128_zero();
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) wasm__v128_set_f64(&value, lane, lanes[lane]);
    return value;
}

static void wasm_test_expect_u8x16(wl_test_ctx* t,
                                   const wasm_value_t* actual,
                                   const uint8_t expected[16],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        WL_CHECK_MSG(t,
                     wasm__v128_get_u8(actual, lane) == expected[lane],
                     "%s lane %u: expected %u, got %u",
                     label,
                     (unsigned)lane,
                     (unsigned)expected[lane],
                     (unsigned)wasm__v128_get_u8(actual, lane));
    }
}

static void wasm_test_expect_i16x8(wl_test_ctx* t,
                                   const wasm_value_t* actual,
                                   const int16_t expected[8],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 8; lane++) {
        WL_CHECK_MSG(t,
                     wasm__v128_get_i16(actual, lane) == expected[lane],
                     "%s lane %u: expected %d, got %d",
                     label,
                     (unsigned)lane,
                     (int)expected[lane],
                     (int)wasm__v128_get_i16(actual, lane));
    }
}

static void wasm_test_expect_i32x4(wl_test_ctx* t,
                                   const wasm_value_t* actual,
                                   const int32_t expected[4],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        WL_CHECK_MSG(t,
                     wasm__v128_get_i32(actual, lane) == expected[lane],
                     "%s lane %u: expected %d, got %d",
                     label,
                     (unsigned)lane,
                     expected[lane],
                     wasm__v128_get_i32(actual, lane));
    }
}

static void wasm_test_expect_i64x2(wl_test_ctx* t,
                                   const wasm_value_t* actual,
                                   const int64_t expected[2],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        WL_CHECK_MSG(t,
                     wasm__v128_get_i64(actual, lane) == expected[lane],
                     "%s lane %u: expected %lld, got %lld",
                     label,
                     (unsigned)lane,
                     (long long)expected[lane],
                     (long long)wasm__v128_get_i64(actual, lane));
    }
}

static void wasm_test_expect_f32x4(wl_test_ctx* t,
                                   const wasm_value_t* actual,
                                   const float expected[4],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 4; lane++) {
        float got = wasm__v128_get_f32(actual, lane);
        WL_CHECK_MSG(t,
                     fabsf(got - expected[lane]) <= 1e-6f,
                     "%s lane %u: expected %.6f, got %.6f",
                     label,
                     (unsigned)lane,
                     expected[lane],
                     got);
    }
}

static void wasm_test_expect_f64x2(wl_test_ctx* t,
                                   const wasm_value_t* actual,
                                   const double expected[2],
                                   const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 2; lane++) {
        double got = wasm__v128_get_f64(actual, lane);
        WL_CHECK_MSG(t,
                     fabs(got - expected[lane]) <= 1e-9,
                     "%s lane %u: expected %.9f, got %.9f",
                     label,
                     (unsigned)lane,
                     expected[lane],
                     got);
    }
}

/* ── Test 1: add(a, b) -> a + b ─────────────────────────────────── */

WL_TEST(test_add) {
    /*
     * Module with one function: (func (param i32 i32) (result i32)
     *   local.get 0
     *   local.get 1
     *   i32.add)
     */
    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    /* Type section: one func type (i32, i32) -> i32 */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 type */
        emit(&sec, 0x60);         /* func */
        emit_leb128_u32(&sec, 2); /* 2 params */
        emit(&sec, 0x7F);         /* i32 */
        emit(&sec, 0x7F);         /* i32 */
        emit_leb128_u32(&sec, 1); /* 1 result */
        emit(&sec, 0x7F);         /* i32 */
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function section: one function using type 0 */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 func */
        emit_leb128_u32(&sec, 0); /* type index 0 */
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export section: export func 0 as "add" */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 export */
        emit_leb128_u32(&sec, 3); /* name len */
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, 'd');
        emit(&sec, 0x00);         /* func kind */
        emit_leb128_u32(&sec, 0); /* func index */
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code section */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 body */

        /* Function body */
        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0); /* 0 local declarations */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1); /* local.get 1 */
        emit(&body, 0x6A);         /* i32.add */
        emit(&body, 0x0B);         /* end */

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[] = { wasm_i32(3), wasm_i32(7) };
    wasm_value_t result;
    wasm_error_t err;
    wasm_test_alloc_stats alloc_before = wasm_test_alloc_snapshot();
    wasm_test_alloc_stats alloc_after;
    size_t alloc_delta;
    size_t free_delta;

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "add", args, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 10);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);

    alloc_after = wasm_test_alloc_snapshot();
    alloc_delta = (alloc_after.malloc_calls - alloc_before.malloc_calls) +
                  (alloc_after.realloc_calls - alloc_before.realloc_calls) +
                  (alloc_after.calloc_calls - alloc_before.calloc_calls);
    free_delta = alloc_after.free_calls - alloc_before.free_calls;
    WL_CHECK_MSG(t, alloc_delta > 0, "%s", "expected custom wasm allocators to be exercised");
    WL_CHECK_MSG(t, free_delta > 0, "%s", "expected custom wasm allocator frees during cleanup");
}

WL_TEST(test_introspection_helpers) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_export_kind_t kind = WASM_EXPORT_MEM;
    uint32_t func_idx = 77;
    static const uint8_t meta_payload[] = { 0x01, 0x02, 0x03 };
    static const uint8_t cfg_payload[] = { 'o', 'k' };
    const uint8_t* custom_data;
    size_t custom_len = 0;

    emit_header(&mod);
    emit_custom_section(&mod, "meta", meta_payload, (uint32_t)sizeof(meta_payload));

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "add", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    emit_custom_section(&mod, "cfg", cfg_payload, (uint32_t)sizeof(cfg_payload));

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, wasm_export_count(NULL) == 0, "%s", "NULL module should report zero exports");
    WL_CHECK_MSG(t, wasm_export_count(m) == 1, "%s", "expected one export");
    WL_CHECK_MSG(t, wasm_export_name(m, 0) != NULL && strcmp(wasm_export_name(m, 0), "add") == 0,
                 "%s", "expected export 0 to be named add");
    WL_CHECK_MSG(t, wasm_export_name(m, 1) == NULL, "%s", "out-of-range export name should be NULL");
    WL_CHECK_MSG(t, wasm_export_kind(m, 0) == WASM_EXPORT_FUNC, "%s", "expected function export kind");
    WL_CHECK_MSG(t, wasm_export_index(m, 0) == 0, "%s", "expected function index 0");
    WL_CHECK_MSG(t, wasm_export_index(m, 1) == 0, "%s", "out-of-range export index should default to 0");

    WL_CHECK_MSG(t, !wasm_find_export(m, "missing", &kind, &func_idx), "%s", "missing export should not resolve");
    WL_CHECK_MSG(t, kind == WASM_EXPORT_MEM, "%s", "missing export should leave kind untouched");
    WL_CHECK_MSG(t, func_idx == 77, "%s", "missing export should leave index untouched");
    WL_CHECK_MSG(t, wasm_find_export(m, "add", &kind, &func_idx), "%s", "expected add export lookup to succeed");
    WL_CHECK_MSG(t, kind == WASM_EXPORT_FUNC, "%s", "expected add export kind to be function");
    WL_CHECK_MSG(t, func_idx == 0, "%s", "expected add export to reference function 0");

    WL_CHECK_MSG(t, wasm_custom_section_count(NULL) == 0,
                 "%s", "NULL module should report zero custom sections");
    WL_CHECK_MSG(t, wasm_custom_section_count(m) == 2,
                 "%s", "expected two custom sections");
    WL_CHECK_MSG(t, wasm_custom_section_name(m, 0) != NULL && strcmp(wasm_custom_section_name(m, 0), "meta") == 0,
                 "%s", "expected custom section 0 to be named meta");
    WL_CHECK_MSG(t, wasm_custom_section_name(m, 1) != NULL && strcmp(wasm_custom_section_name(m, 1), "cfg") == 0,
                 "%s", "expected custom section 1 to be named cfg");
    WL_CHECK_MSG(t, wasm_custom_section_name(m, 2) == NULL,
                 "%s", "out-of-range custom section name should be NULL");

    custom_data = wasm_custom_section_data(m, 0, &custom_len);
    WL_CHECK_MSG(t, custom_data != NULL, "%s", "expected custom section payload");
    WL_CHECK_MSG(t, custom_len == sizeof(meta_payload), "%s", "unexpected meta payload length");
    WL_CHECK_MSG(t, memcmp(custom_data, meta_payload, sizeof(meta_payload)) == 0,
                 "%s", "unexpected meta payload bytes");

    custom_len = 123;
    WL_CHECK_MSG(t, wasm_custom_section_data(m, 9, &custom_len) == NULL,
                 "%s", "out-of-range custom section data should be NULL");
    WL_CHECK_MSG(t, custom_len == 0, "%s", "out-of-range custom section length should reset to zero");

    custom_data = wasm_custom_section_find(m, "cfg", &custom_len);
    WL_CHECK_MSG(t, custom_data != NULL, "%s", "expected cfg custom section lookup to succeed");
    WL_CHECK_MSG(t, custom_len == sizeof(cfg_payload), "%s", "unexpected cfg payload length");
    WL_CHECK_MSG(t, memcmp(custom_data, cfg_payload, sizeof(cfg_payload)) == 0,
                 "%s", "unexpected cfg payload bytes");
    custom_len = 55;
    WL_CHECK_MSG(t, wasm_custom_section_find(m, "missing", &custom_len) == NULL,
                 "%s", "missing custom section should not resolve");
    WL_CHECK_MSG(t, custom_len == 0, "%s", "missing custom section length should reset to zero");

    WL_CHECK_MSG(t, wasm_func_count(NULL) == 0, "%s", "NULL module should report zero functions");
    WL_CHECK_MSG(t, wasm_func_count(m) == 1, "%s", "expected one function");
    WL_CHECK_MSG(t, wasm_func_param_count(m, 0) == 2, "%s", "expected two params");
    WL_CHECK_MSG(t, wasm_func_param_count(m, 1) == 0, "%s", "out-of-range param count should default to 0");
    WL_CHECK_MSG(t, wasm_func_result_count(m, 0) == 1, "%s", "expected one result");
    WL_CHECK_MSG(t, wasm_func_result_count(m, 1) == 0, "%s", "out-of-range result count should default to 0");
    WL_CHECK_MSG(t, wasm_func_param_type(m, 0, 0) == WASM_TYPE_I32, "%s", "expected first param type i32");
    WL_CHECK_MSG(t, wasm_func_param_type(m, 0, 1) == WASM_TYPE_I32, "%s", "expected second param type i32");
    WL_CHECK_MSG(t, wasm_func_param_type(m, 0, 2) == WASM_TYPE_VOID,
                 "%s", "out-of-range param type should default to void");
    WL_CHECK_MSG(t, wasm_func_param_type(NULL, 0, 0) == WASM_TYPE_VOID,
                 "%s", "NULL module param type should default to void");
    WL_CHECK_MSG(t, wasm_func_result_type(m, 0, 0) == WASM_TYPE_I32, "%s", "expected result type i32");
    WL_CHECK_MSG(t, wasm_func_result_type(m, 0, 1) == WASM_TYPE_VOID,
                 "%s", "out-of-range result type should default to void");
    WL_CHECK_MSG(t, wasm_func_result_type(NULL, 0, 0) == WASM_TYPE_VOID,
                 "%s", "NULL module result type should default to void");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Test 2: factorial(n) recursive ─────────────────────────────── */

WL_TEST(test_factorial) {
    /*
     * (func $fac (param i32) (result i32)
     *   local.get 0
     *   i32.eqz
     *   if (result i32)
     *     i32.const 1
     *   else
     *     local.get 0
     *     local.get 0
     *     i32.const 1
     *     i32.sub
     *     call $fac
     *     i32.mul
     *   end)
     */
    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    /* Type section */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function section */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'f');
        emit(&sec, 'a');
        emit(&sec, 'c');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code section */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0); /* no local decls */

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x45);         /* i32.eqz */
        emit(&body, 0x04);
        emit(&body, 0x7F); /* if (result i32) */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1); /* i32.const 1 */
        emit(&body, 0x05);         /* else */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1); /* i32.const 1 */
        emit(&body, 0x6B);         /* i32.sub */
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0); /* call 0 (self) */
        emit(&body, 0x6C);         /* i32.mul */
        emit(&body, 0x0B);         /* end if */
        emit(&body, 0x0B);         /* end func */

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[] = { wasm_i32(10) };
    wasm_value_t result;
    wasm_error_t err;

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "fac", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 3628800);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Test 3: host import (print function) ───────────────────────── */

static int host_print_called = 0;
static int32_t host_print_value = 0;
static int host_fmt_add_called = 0;

typedef struct wasm_test_stream_capture {
    FILE* stream;
    FILE* temp;
    int saved_fd;
    int stream_fd;
} wasm_test_stream_capture;

static int wasm_test_begin_stream_capture(wasm_test_stream_capture* capture, FILE* stream) {
    int temp_fd;

    if (!capture || !stream) return 0;

    memset(capture, 0, sizeof(*capture));
    capture->stream = stream;
    capture->stream_fd = WASM_TEST_FILENO(stream);
    if (capture->stream_fd < 0) return 0;

    fflush(stream);
    capture->saved_fd = WASM_TEST_DUP(capture->stream_fd);
    if (capture->saved_fd < 0) return 0;

    capture->temp = tmpfile();
    if (!capture->temp) {
        WASM_TEST_CLOSE(capture->saved_fd);
        capture->saved_fd = -1;
        return 0;
    }

    temp_fd = WASM_TEST_FILENO(capture->temp);
    if (temp_fd < 0 || WASM_TEST_DUP2(temp_fd, capture->stream_fd) < 0) {
        fclose(capture->temp);
        capture->temp = NULL;
        WASM_TEST_CLOSE(capture->saved_fd);
        capture->saved_fd = -1;
        return 0;
    }

    return 1;
}

static size_t wasm_test_end_stream_capture(wasm_test_stream_capture* capture,
                                           char* buffer,
                                           size_t buffer_size) {
    size_t bytes_read = 0;

    if (!capture || !capture->temp) return 0;

    fflush(capture->stream);
    fflush(capture->temp);
    if (buffer && buffer_size > 0) {
        long end_pos;

        if (fseek(capture->temp, 0, SEEK_END) == 0) {
            end_pos = ftell(capture->temp);
            if (end_pos > 0) {
                size_t max_read = buffer_size - 1u;

                if ((size_t)end_pos < max_read) max_read = (size_t)end_pos;
                if (fseek(capture->temp, 0, SEEK_SET) == 0)
                    bytes_read = fread(buffer, 1u, max_read, capture->temp);
            }
        }
        buffer[bytes_read] = '\0';
    }

    WASM_TEST_DUP2(capture->saved_fd, capture->stream_fd);
    WASM_TEST_CLOSE(capture->saved_fd);
    fclose(capture->temp);
    memset(capture, 0, sizeof(*capture));
    return bytes_read;
}

static wasm_error_t host_print(wasm_module_t* mod,
                               const wasm_value_t* args, uint32_t num_args,
                               wasm_value_t* results, uint32_t num_results,
                               void* userdata) {
    (void)mod;
    (void)results;
    (void)num_results;
    (void)userdata;
    host_print_called = 1;
    if (num_args > 0) host_print_value = args[0].of.i32;
    return WASM_OK;
}

static wasm_error_t host_gc_subtype_identity(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)userdata;

    if (num_results > 0) results[0] = wasm_ref_null(WASM_TYPE_NONE);
    return WASM_OK;
}

static wasm_error_t host_fmt_add(wasm_module_t* mod,
                                 const wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results,
                                 void* userdata) {
    int32_t bias = userdata ? *(const int32_t*)userdata : 0;

    (void)mod;
    host_fmt_add_called = 1;
    if (num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    results[0] = wasm_i32(args[0].of.i32 + args[1].of.i32 + bias);
    return WASM_OK;
}

static wasm_module_t* host_userdata_seen_module;
static void* host_userdata_seen_value;

static wasm_error_t host_capture_module_userdata(wasm_module_t* mod,
                                                 const wasm_value_t* args, uint32_t num_args,
                                                 wasm_value_t* results, uint32_t num_results,
                                                 void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;

    host_userdata_seen_module = mod;
    host_userdata_seen_value = wasm_module_get_userdata(mod);
    return WASM_OK;
}

static wasm_error_t host_module_userdata_add(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    int32_t bias = 0;

    (void)userdata;

    if (!mod || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (wasm_module_get_userdata(mod)) bias = *(const int32_t*)wasm_module_get_userdata(mod);

    results[0] = wasm_i32(args[0].of.i32 + args[1].of.i32 + bias);
    return WASM_OK;
}

static wasm_error_t host_peek_i32_from_memory(wasm_module_t* mod,
                                              const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results,
                                              void* userdata) {
    int32_t value = 0;
    wasm_error_t err;

    (void)userdata;

    if (!mod || !args || !results || num_args != 1 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (args[0].of.i32 < 0) return WASM_ERR_TYPE_MISMATCH;

    err = wasm_memory_read(mod, 0, (uint32_t)args[0].of.i32, &value, sizeof(value));
    if (err != WASM_OK) return err;

    results[0] = wasm_i32(value);
    return WASM_OK;
}

WL_TEST(test_host_import) {
    host_print_called = 0;
    host_print_value = 0;

    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    /* Type section: (i32) -> () */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2); /* 2 types */
        /* type 0: (i32) -> () for import */
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);
        /* type 1: () -> () for exported func */
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Import section: env.print */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 import */
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 5);
        emit(&sec, 'p');
        emit(&sec, 'r');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 0x00);         /* func import */
        emit_leb128_u32(&sec, 0); /* type index 0 */
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    /* Function section: 1 func (the exported one) */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1); /* type index 1: () -> () */
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export: "run" = func 1 (func 0 is the import) */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1); /* func index 1 */
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code section */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 42); /* i32.const 42 */
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0); /* call 0 (import) */
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;

    wasm_init(&rt);

    wasm_import_t imp;

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "print";
    imp.func = host_print;
    wasm_register_import(&rt, &imp);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, NULL, 0);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, host_print_called, "%s", "host func not called");
        if (host_print_called) {
            WASM_CHECK_I32(t, host_print_value, 42);
        }
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_import_introspection_api) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_import_t func_imp;
    wasm_global_import_t global_imp;
    wasm_value_t imported_counter = wasm_i32(7);
    const wasm_import_info_t* info;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 5);
        emit(&sec, 'p');
        emit(&sec, 'r');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 7);
        emit(&sec, 'c');
        emit(&sec, 'o');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 'r');
        emit(&sec, 0x03);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit_section(&mod, 2, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&func_imp, 0, sizeof(func_imp));
    func_imp.module = "env";
    func_imp.name = "print";
    func_imp.func = host_print;
    wasm_register_import(&rt, &func_imp);

    memset(&global_imp, 0, sizeof(global_imp));
    global_imp.module = "env";
    global_imp.name = "counter";
    global_imp.type = WASM_TYPE_I32;
    global_imp.value = &imported_counter;
    wasm_register_global_import(&rt, &global_imp);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_I32(t, (int32_t)wasm_import_count(m), 2);

    info = wasm_import_info(m, 0);
    WL_CHECK_MSG(t, info != NULL, "%s", "expected function import metadata");
    if (info) {
        WL_CHECK_MSG(t, strcmp(info->module, "env") == 0, "%s", "wrong function import module name");
        WL_CHECK_MSG(t, strcmp(info->name, "print") == 0, "%s", "wrong function import name");
        WL_CHECK_MSG(t, info->kind == WASM_IMPORT_FUNC, "%s", "wrong function import kind");
        WASM_CHECK_I32(t, (int32_t)info->index, 0);
        WASM_CHECK_I32(t, (int32_t)info->type_index, 0);
    }

    info = wasm_import_info(m, 1);
    WL_CHECK_MSG(t, info != NULL, "%s", "expected global import metadata");
    if (info) {
        WL_CHECK_MSG(t, strcmp(info->module, "env") == 0, "%s", "wrong global import module name");
        WL_CHECK_MSG(t, strcmp(info->name, "counter") == 0, "%s", "wrong global import name");
        WL_CHECK_MSG(t, info->kind == WASM_IMPORT_GLOBAL, "%s", "wrong global import kind");
        WASM_CHECK_I32(t, (int32_t)info->index, 0);
        WL_CHECK_MSG(t, info->type == WASM_TYPE_I32, "%s", "wrong global import type");
        WL_CHECK_MSG(t, info->is_mutable == 0, "%s", "expected immutable global import");
    }

    WL_CHECK_MSG(t, wasm_import_info(m, 2) == NULL, "%s", "out-of-range import metadata should be null");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_module_userdata_api) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    int module_context = 1234;

    host_userdata_seen_module = NULL;
    host_userdata_seen_value = NULL;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 7);
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'p');
        emit(&sec, 't');
        emit(&sec, 'u');
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    {
        wasm_import_t imp;

        memset(&imp, 0, sizeof(imp));
        imp.module = "env";
        imp.name = "capture";
        imp.func = host_capture_module_userdata;
        wasm_register_import(&rt, &imp);
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, wasm_module_get_userdata(m) == NULL,
                 "%s", "module userdata should default to null");
    wasm_module_set_userdata(m, &module_context);
    WL_CHECK_MSG(t, wasm_module_get_userdata(m) == &module_context,
                 "%s", "module userdata getter should return the stored pointer");

    err = wasm_call(m, "run", NULL, 0, NULL, 0);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, host_userdata_seen_module == m,
                     "%s", "host callback should see the calling module");
        WL_CHECK_MSG(t, host_userdata_seen_value == &module_context,
                     "%s", "host callback should see the module userdata");
    }

    wasm_module_set_userdata(m, NULL);
    WL_CHECK_MSG(t, wasm_module_get_userdata(m) == NULL,
                 "%s", "module userdata should clear back to null");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_module_userdata_api_multiple_modules) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m1;
    wasm_module_t* m2;
    wasm_error_t err;
    int ctx1 = 11;
    int ctx2 = 22;

    host_userdata_seen_module = NULL;
    host_userdata_seen_value = NULL;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 7);
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'p');
        emit(&sec, 't');
        emit(&sec, 'u');
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    {
        wasm_import_t imp;

        memset(&imp, 0, sizeof(imp));
        imp.module = "env";
        imp.name = "capture";
        imp.func = host_capture_module_userdata;
        wasm_register_import(&rt, &imp);
    }

    m1 = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m1 != NULL, "%s", rt.error_msg);
    if (m1 == NULL) {
        wasm_destroy(&rt);
        return;
    }

    m2 = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m2 != NULL, "%s", rt.error_msg);
    if (m2 == NULL) {
        wasm_free_module(m1);
        wasm_destroy(&rt);
        return;
    }

    wasm_module_set_userdata(m1, &ctx1);
    wasm_module_set_userdata(m2, &ctx2);

    err = wasm_call(m1, "run", NULL, 0, NULL, 0);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, host_userdata_seen_module == m1,
                     "%s", "shared host import should receive the first module instance");
        WL_CHECK_MSG(t, host_userdata_seen_value == &ctx1,
                     "%s", "shared host import should read the first module userdata");
    }

    host_userdata_seen_module = NULL;
    host_userdata_seen_value = NULL;

    err = wasm_call(m2, "run", NULL, 0, NULL, 0);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, host_userdata_seen_module == m2,
                     "%s", "shared host import should receive the second module instance");
        WL_CHECK_MSG(t, host_userdata_seen_value == &ctx2,
                     "%s", "shared host import should read the second module userdata");
    }

    wasm_free_module(m2);
    wasm_free_module(m1);
    wasm_destroy(&rt);
}

WL_TEST(test_format_call_api) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    int32_t sum_i32 = 0;
    float sum_f32 = 0.0f;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7D);
        emit(&sec, 0x7D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7D);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);
        emit_export_func(&sec, "add", 0);
        emit_export_func(&sec, "fadd", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        static const uint8_t ops[] = { 0x6A, 0x92 };
        uint32_t i;
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);
        for (i = 0; i < 2; i++) {
            wasm_builder_t body = { 0 };
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 1);
            emit(&body, ops[i]);
            emit(&body, 0x0B);
            emit_leb128_u32(&sec, body.len);
            emit_bytes(&sec, body.buf, body.len);
        }

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call_fmt(m, "add", "ii(i)", (int32_t)3, (int32_t)7, &sum_i32);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, sum_i32, 10);
    }

    err = wasm_call_fmt(m, "fadd", "ff(f)", 1.5, 2.5, &sum_f32);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, sum_f32 == 4.0f, "%s", "expected f32 format-call result 4.0");
    }

    err = wasm_call_fmt(m, "add", "ii(v)", (int32_t)1, (int32_t)2);
    WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected signature mismatch rejection");

    err = wasm_call_fmt(m, "add", "ii(i", (int32_t)1, (int32_t)2, &sum_i32);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected malformed format string rejection");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_bind_host_func_api) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    int32_t bias = 5;
    int32_t result = 0;

    host_fmt_add_called = 0;
    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, 'd');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    err = wasm_bind_host_func(&rt, "env", "bad", "i(vv)", host_fmt_add, &bias);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected malformed format rejection during bind");

    err = wasm_bind_host_func(&rt, "env", "add", "ii(i)", host_fmt_add, &bias);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call_fmt(m, "run", "(i)", &result);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, host_fmt_add_called, "%s", "expected bound host function to run");
        WASM_CHECK_I32(t, result, 18);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_bind_host_func_uses_module_userdata) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    int32_t bias = 6;
    int32_t result = 0;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, 'd');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    err = wasm_bind_host_func(&rt, "env", "add", "ii(i)", host_module_userdata_add, NULL);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call_fmt(m, "run", "(i)", &result);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result, 13);
    }

    wasm_module_set_userdata(m, &bias);
    err = wasm_call_fmt(m, "run", "(i)", &result);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result, 19);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_host_callback_module_reads_memory) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_import_t imp;
    wasm_error_t err;
    int32_t result = 0;
    static const uint8_t data_bytes[4] = { 0x78, 0x56, 0x34, 0x12 };

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'p');
        emit(&sec, 'e');
        emit(&sec, 'e');
        emit(&sec, 'k');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "peek", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 8);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "peek";
    imp.func = host_peek_i32_from_memory;
    err = wasm_register_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call_fmt(m, "peek", "i(i)", (int32_t)8, &result);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result, 0x12345678);
    }

    err = wasm_call_fmt(m, "peek", "i(i)", (int32_t)(WASM_PAGE_SIZE - 2u), &result);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS,
                 "%s", "expected host callback memory read to preserve out-of-bounds errors");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_imported_mutable_global) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_global_import_t imp;
    wasm_value_t imported_counter = wasm_i32(10);
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 7);
        emit(&sec, 'c');
        emit(&sec, 'o');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 'r');
        emit(&sec, 0x03);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x6A);
        emit(&body, 0x24);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "counter";
    imp.type = WASM_TYPE_I32;
    imp.is_mutable = 1;
    imp.value = &imported_counter;

    err = wasm_register_global_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 15);
        WASM_CHECK_I32(t, imported_counter.of.i32, 15);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_global_init_expr_global_get_import) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_global_import_t imp;
    wasm_value_t imported_seed = wasm_i32(33);
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 4);
        emit(&sec, 's');
        emit(&sec, 'e');
        emit(&sec, 'e');
        emit(&sec, 'd');
        emit(&sec, 0x03);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 8);
        emit(&sec, 's');
        emit(&sec, 'n');
        emit(&sec, 'a');
        emit(&sec, 'p');
        emit(&sec, 's');
        emit(&sec, 'h');
        emit(&sec, 'o');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "seed";
    imp.type = WASM_TYPE_I32;
    imp.is_mutable = 0;
    imp.value = &imported_seed;

    err = wasm_register_global_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    imported_seed.of.i32 = 99;
    err = wasm_call(m, "snapshot", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 33);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_global_init_expr_extended_i32_arithmetic) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 6);
        emit(&sec, 0x0B);

        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 3);
        emit(&sec, 0x6C);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 4);
        emit(&sec, 0x6A);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 22);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_global_init_expr_extended_i64_arithmetic) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x7E);
        emit(&sec, 0x00);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 5);
        emit(&sec, 0x0B);

        emit(&sec, 0x7E);
        emit(&sec, 0x00);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 4);
        emit(&sec, 0x7E);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 3);
        emit(&sec, 0x7C);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I64(t, result.of.i64, 23);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_init_expr_rejects_local_mutable_global_get) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x7F);
        emit(&sec, 0x01);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 6);
        emit(&sec, 0x0B);

        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected mutable local global.get rejection");
    if (m != NULL) wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_data_offset_from_imported_global) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_global_import_t imp;
    wasm_value_t stack_pointer = wasm_i32(8);
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 15);
        emit(&sec, '_');
        emit(&sec, '_');
        emit(&sec, 's');
        emit(&sec, 't');
        emit(&sec, 'a');
        emit(&sec, 'c');
        emit(&sec, 'k');
        emit(&sec, '_');
        emit(&sec, 'p');
        emit(&sec, 'o');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 'r');
        emit(&sec, 0x03);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 8);
        emit(&body, 0x28);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[4] = { 42, 0, 0, 0 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 4);
        emit_bytes(&sec, data_bytes, 4);
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "__stack_pointer";
    imp.type = WASM_TYPE_I32;
    imp.is_mutable = 0;
    imp.value = &stack_pointer;

    err = wasm_register_global_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 42);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_data_offset_extended_const_expr) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_global_import_t imp;
    wasm_value_t stack_pointer = wasm_i32(6);
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 15);
        emit(&sec, '_');
        emit(&sec, '_');
        emit(&sec, 's');
        emit(&sec, 't');
        emit(&sec, 'a');
        emit(&sec, 'c');
        emit(&sec, 'k');
        emit(&sec, '_');
        emit(&sec, 'p');
        emit(&sec, 'o');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 'r');
        emit(&sec, 0x03);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 12);
        emit(&body, 0x28);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[4] = { 55, 0, 0, 0 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x23);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 3);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 2);
        emit(&sec, 0x6C);
        emit(&sec, 0x6A);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 4);
        emit_bytes(&sec, data_bytes, 4);
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "__stack_pointer";
    imp.type = WASM_TYPE_I32;
    imp.is_mutable = 0;
    imp.value = &stack_pointer;

    err = wasm_register_global_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 55);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_element_offset_extended_const_expr) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 4);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'l');
        emit(&sec, 'l');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 1);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 1);
        emit(&sec, 0x6A);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 77);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "call", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 77);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Test 4: memory load/store ──────────────────────────────────── */

WL_TEST(test_memory) {
    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    /* Type: () -> (i32) */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Memory: 1 page min */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1); /* 1 memory */
        emit(&sec, 0x00);         /* limits: no max */
        emit_leb128_u32(&sec, 1); /* 1 page initial */
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    /* Export */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 's');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code: i32.const 0; i32.const 99; i32.store; i32.const 0; i32.load */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0);

        /* store 99 at address 0 */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0); /* i32.const 0 (addr) */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 99); /* i32.const 99 (value) */
        emit(&body, 0x36);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0); /* i32.store align=2 offset=0 */

        /* load from address 0 */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0); /* i32.const 0 */
        emit(&body, 0x28);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0); /* i32.load align=2 offset=0 */

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "test", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 99);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_multi_memory_indexed_access) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint8_t* memory0;
    uint8_t* memory1;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 11);
        emit(&body, 0x36);
        emit_memarg(&body, 2, 1, 0, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 31);
        emit(&body, 0x36);
        emit_memarg(&body, 2, 1, 1, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x28);
        emit_memarg(&body, 2, 1, 0, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x28);
        emit_memarg(&body, 2, 1, 1, 0);

        emit(&body, 0x6A);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 42);
    }

    WASM_CHECK_I32(t, (int32_t)wasm_memory_count(m), 2);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m), (int32_t)WASM_PAGE_SIZE);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size_at(m, 1), (int32_t)WASM_PAGE_SIZE);
    WL_CHECK_MSG(t, wasm_memory_data_at(m, 2) == NULL, "%s", "expected only two memories");

    memory0 = wasm_memory_data(m);
    memory1 = wasm_memory_data_at(m, 1);
    WL_CHECK_MSG(t, memory0 != NULL, "%s", "expected memory 0 allocation");
    WL_CHECK_MSG(t, memory1 != NULL, "%s", "expected memory 1 allocation");
    if (memory0 && memory1) {
        WASM_CHECK_I32(t, memory0[0], 11);
        WASM_CHECK_I32(t, memory1[0], 31);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_public_memory_helpers) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    uint8_t bytes[4] = { 0, 0, 0, 0 };
    static const uint8_t tail_bytes[2] = { 'x', 'y' };
    char buffer[8];

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_memory_write(m, 0, 4, "abc", 4);
    WASM_CHECK_OK(t, err);

    err = wasm_memory_read(m, 0, 4, bytes, sizeof(bytes));
    WASM_CHECK_OK(t, err);
    WL_CHECK_MSG(t, memcmp(bytes, "abc", sizeof(bytes)) == 0,
                 "%s", "expected memory_read to copy the written bytes");

    memset(buffer, '?', sizeof(buffer));
    err = wasm_memory_get_string(m, 0, 4, buffer, sizeof(buffer));
    WASM_CHECK_OK(t, err);
    WL_CHECK_MSG(t, strcmp(buffer, "abc") == 0,
                 "%s", "expected memory_get_string to read a NUL-terminated string");

    err = wasm_memory_write(m, 0, WASM_PAGE_SIZE - 2u, tail_bytes, sizeof(tail_bytes));
    WASM_CHECK_OK(t, err);

    memset(buffer, '?', sizeof(buffer));
    err = wasm_memory_get_string(m, 0, WASM_PAGE_SIZE - 2u, buffer, sizeof(buffer));
    WASM_CHECK_OK(t, err);
    WL_CHECK_MSG(t, strcmp(buffer, "xy") == 0,
                 "%s", "expected memory_get_string to stop at the end of memory and append NUL");

    err = wasm_memory_read(m, 0, WASM_PAGE_SIZE - 1u, bytes, sizeof(bytes));
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected out-of-bounds read rejection");

    err = wasm_memory_write(m, 1, 0, bytes, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected invalid memory index rejection");

    err = wasm_memory_get_string(m, 0, 0, buffer, 0);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected zero-length string buffer rejection");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_multi_memory_bulk_ops_and_growth) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    int32_t old_pages;
    uint8_t* memory0;
    uint8_t* memory1;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 'a', 'b' };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 12, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0A);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 6);
        emit(&body, 0x41);
        emit_leb128_i32(&body, '!');
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x3F);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x3F);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x40);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x2D);
        emit_memarg(&body, 0, 1, 0, 0);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x2D);
        emit_memarg(&body, 0, 1, 0, 0);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 6);
        emit(&body, 0x2D);
        emit_memarg(&body, 0, 1, 1, 0);
        emit(&body, 0x6A);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 231);
    }

    memory0 = wasm_memory_data_at(m, 0);
    memory1 = wasm_memory_data_at(m, 1);
    WL_CHECK_MSG(t, memory0 != NULL, "%s", "expected memory 0 allocation");
    WL_CHECK_MSG(t, memory1 != NULL, "%s", "expected memory 1 allocation");
    if (memory0 && memory1) {
        WASM_CHECK_I32(t, memory0[4], 'a');
        WASM_CHECK_I32(t, memory0[5], 'b');
        WASM_CHECK_I32(t, memory1[0], 'a');
        WASM_CHECK_I32(t, memory1[1], 'b');
        WASM_CHECK_I32(t, memory1[6], '!');
    }
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size_at(m, 1), (int32_t)(2 * WASM_PAGE_SIZE));

    old_pages = wasm_memory_grow_at(m, 0, 1);
    WASM_CHECK_I32(t, old_pages, 1);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size_at(m, 0), (int32_t)(2 * WASM_PAGE_SIZE));

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_bulk_memory_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint8_t* memory;

    emit_header(&mod);

    /* Type: () -> (i32) */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Functions: run, reuse */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Memory: 1 page min */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    /* Exports */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 'u');
        emit(&sec, 's');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Data: one passive segment containing "ABCDE" */
    {
        static const uint8_t data_bytes[] = { 'A', 'B', 'C', 'D', 'E' };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 12, sec.buf, sec.len);
    }

    /* Code */
    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t run_body = { 0 };
        wasm_builder_t reuse_body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 0);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 0);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 5);
        emit(&run_body, 0xFC);
        emit_leb128_u32(&run_body, 0x08);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 0);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 5);
        emit(&run_body, 0xFC);
        emit_leb128_u32(&run_body, 0x0A);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 5);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, '!');
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0xFC);
        emit_leb128_u32(&run_body, 0x0B);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0xFC);
        emit_leb128_u32(&run_body, 0x09);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0x2D);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 4);
        emit(&run_body, 0x2D);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);
        emit(&run_body, 0x6A);

        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 5);
        emit(&run_body, 0x2D);
        emit_leb128_u32(&run_body, 0);
        emit_leb128_u32(&run_body, 0);
        emit(&run_body, 0x6A);
        emit(&run_body, 0x0B);

        emit_leb128_u32(&sec, run_body.len);
        emit_bytes(&sec, run_body.buf, run_body.len);

        emit_leb128_u32(&reuse_body, 0);
        emit(&reuse_body, 0x41);
        emit_leb128_i32(&reuse_body, 0);
        emit(&reuse_body, 0x41);
        emit_leb128_i32(&reuse_body, 0);
        emit(&reuse_body, 0x41);
        emit_leb128_i32(&reuse_body, 1);
        emit(&reuse_body, 0xFC);
        emit_leb128_u32(&reuse_body, 0x08);
        emit_leb128_u32(&reuse_body, 0);
        emit_leb128_u32(&reuse_body, 0);
        emit(&reuse_body, 0x41);
        emit_leb128_i32(&reuse_body, 0);
        emit(&reuse_body, 0x0B);

        emit_leb128_u32(&sec, reuse_body.len);
        emit_bytes(&sec, reuse_body.buf, reuse_body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 166);

        memory = wasm_memory_data(m);
        WL_CHECK_MSG(t, memory != NULL, "%s", "expected linear memory allocation");
        if (memory) {
            WASM_CHECK_I32(t, memory[0], 'A');
            WASM_CHECK_I32(t, memory[1], 'A');
            WASM_CHECK_I32(t, memory[4], 'D');
            WASM_CHECK_I32(t, memory[5], '!');
        }
    }

    err = wasm_call(m, "reuse", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS,
                 "%s", "expected dropped passive segment to behave like an empty segment on memory.init");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_bulk_memory_table_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    /* Type: () -> (i32) */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Functions: eleven, twenty_nine, run, reuse */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Table: 4 funcref slots */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 4);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    /* Exports */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 'u');
        emit(&sec, 's');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 3);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Element segments:
     *   0: active, implicit table 0, offset 0 => [func 0]
     *   1: active, explicit table 0, offset 3 => [func 1]
     *   2: passive => [func 1, func 0]
     *   3: declarative => [func 0]
     */
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 3);
        emit(&sec, 0x0B);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 9, sec.buf, sec.len);
    }

    /* Code */
    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 4);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 11);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 29);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0E);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 2);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 3);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x00);
        emit(&body, 0x6A);

        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 80);
    }

    err = wasm_call(m, "reuse", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS,
                 "%s", "expected dropped passive element segment to behave like an empty segment on table.init");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_bulk_memory_oob_traps) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 8);
        emit(&sec, 'b');
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, '_');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 'i');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 8);
        emit(&sec, 'b');
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, '_');
        emit(&sec, 'c');
        emit(&sec, 'o');
        emit(&sec, 'p');
        emit(&sec, 'y');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 'X', 'Y' };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 12, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 65535);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 65535);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0A);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "bad_init", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected memory.init out-of-bounds trap");

    err = wasm_call(m, "bad_copy", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected memory.copy out-of-bounds trap");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_bulk_memory_table_oob_traps) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 8);
        emit(&sec, 'b');
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, '_');
        emit(&sec, 'i');
        emit(&sec, 'n');
        emit(&sec, 'i');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 8);
        emit(&sec, 'b');
        emit(&sec, 'a');
        emit(&sec, 'd');
        emit(&sec, '_');
        emit(&sec, 'c');
        emit(&sec, 'o');
        emit(&sec, 'p');
        emit(&sec, 'y');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 3);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 4);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0E);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "bad_init", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected table.init out-of-bounds trap");

    err = wasm_call(m, "bad_copy", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected table.copy out-of-bounds trap");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_reference_types_table_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x70);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 5);

        emit(&sec, 0x6F);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);

        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit_leb128_u32(&sec, 6);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit(&sec, 0x6F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0xD0);
        emit(&sec, 0x6F);
        emit(&sec, 0x0B);

        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 3);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 11);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 29);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x7F);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xD0);
        emit(&body, 0x70);
        emit(&body, 0xD1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xD2);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x26);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xD2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0F);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xD2);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x11);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xD0);
        emit(&body, 0x6F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0F);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x10);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xD0);
        emit(&body, 0x6F);
        emit(&body, 0x26);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x25);
        emit_leb128_u32(&body, 1);
        emit(&body, 0xD1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x6A);

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 83);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_reference_types_expr_element_segments) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 'u');
        emit(&sec, 's');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 3);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x70);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x0B);
        emit(&sec, 0xD0);
        emit(&sec, 0x70);
        emit(&sec, 0x0B);

        emit_leb128_u32(&sec, 7);
        emit(&sec, 0x70);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 4);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 11);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 29);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x25);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD1);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 30);
    }

    err = wasm_call(m, "reuse", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS,
                 "%s", "expected dropped passive expr element segment to behave like an empty segment on table.init");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_reference_types_active_expr_element_segment) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 77);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 77);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_loader_reports_section_decode_errors) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 8);
        emit_section(&mod, 9, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected malformed element section to fail load");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", "expected malformed decoder error code");
    WL_CHECK_MSG(t, rt.error_msg[0] != '\0', "%s", "expected loader failure to populate error text");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "section 9 decode failed") != NULL,
                 "%s", rt.error_msg[0] ? rt.error_msg : "missing decode failure context");
    wasm_destroy(&rt);
}

/* ── Test 5: loop with br_if (sum 1..10) ────────────────────────── */

WL_TEST(test_loop) {
    /*
     * (func (result i32)
     *   (local $i i32) (local $sum i32)
     *   local.get $i   ;; i = 0
     *   block
     *     loop
     *       ;; i = i + 1
     *       local.get $i
     *       i32.const 1
     *       i32.add
     *       local.set $i
     *       ;; sum = sum + i
     *       local.get $sum
     *       local.get $i
     *       i32.add
     *       local.set $sum
     *       ;; br_if 1 if i >= 10 (exit block)
     *       local.get $i
     *       i32.const 10
     *       i32.ge_s
     *       br_if 1
     *       ;; else loop
     *       br 0
     *     end
     *   end
     *   local.get $sum)
     */
    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    /* Type: () -> (i32) */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 's');
        emit(&sec, 'u');
        emit(&sec, 'm');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code */
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = { 0 };
        /* 2 locals: i32, i32 */
        emit_leb128_u32(&body, 1); /* 1 local declaration */
        emit_leb128_u32(&body, 2); /* 2 locals */
        emit(&body, 0x7F);         /* i32 */

        /* block (void) */
        emit(&body, 0x02);
        emit(&body, 0x40);
        /* loop (void) */
        emit(&body, 0x03);
        emit(&body, 0x40);

        /* i = i + 1 */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 ($i) */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1); /* i32.const 1 */
        emit(&body, 0x6A);         /* i32.add */
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0); /* local.set 0 ($i) */

        /* sum = sum + i */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1); /* local.get 1 ($sum) */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 ($i) */
        emit(&body, 0x6A);         /* i32.add */
        emit(&body, 0x21);
        emit_leb128_u32(&body, 1); /* local.set 1 ($sum) */

        /* if i >= 10, break */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0); /* local.get 0 ($i) */
        emit(&body, 0x41);
        emit_leb128_i32(&body, 10); /* i32.const 10 */
        emit(&body, 0x4E);          /* i32.ge_s */
        emit(&body, 0x0D);
        emit_leb128_u32(&body, 1); /* br_if 1 (break block) */

        /* continue loop */
        emit(&body, 0x0C);
        emit_leb128_u32(&body, 0); /* br 0 (loop) */

        emit(&body, 0x0B); /* end loop */
        emit(&body, 0x0B); /* end block */

        /* return sum */
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1); /* local.get 1 ($sum) */
        emit(&body, 0x0B);         /* end func */

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "sum", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 55);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Test 6: large signatures and type-index blocks ─────────────── */

WL_TEST(test_large_param_call) {
    wasm_builder_t mod = { 0 };
    wasm_value_t args[17];
    wasm_value_t result;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    uint32_t index;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 17);
        for (index = 0; index < 17; index++) emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'l');
        emit(&sec, 'l');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 16);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        for (index = 0; index < 17; index++) {
            emit(&body, 0x20);
            emit_leb128_u32(&body, index);
        }
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    for (index = 0; index < 17; index++) args[index] = wasm_i32((int32_t)index + 1);

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "call", args, 17, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 17);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_multi_result_call) {
    wasm_builder_t mod = { 0 };
    wasm_value_t results[5];
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 5);
        emit(&sec, 'm');
        emit(&sec, 'u');
        emit(&sec, 'l');
        emit(&sec, 't');
        emit(&sec, 'i');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    memset(results, 0, sizeof(results));
    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "multi", NULL, 0, results, 5);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(
            t,
            results[0].of.i32 == 1 && results[1].of.i32 == 2 && results[2].of.i32 == 3 &&
                results[3].of.i32 == 4 && results[4].of.i32 == 5,
            "%s",
            "unexpected multi-result values");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_loop_type_index_block) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'l');
        emit(&sec, 'o');
        emit(&sec, 'o');
        emit(&sec, 'p');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x7F);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x03);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x6A);
        emit(&body, 0x22);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x48);
        emit(&body, 0x0D);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "loop", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 5);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_prefixed_opcode_in_dead_branch) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 's');
        emit(&sec, 'k');
        emit(&sec, 'i');
        emit(&sec, 'p');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x04);
        emit(&body, 0x7F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xFC);
        emit(&body, 0x0B);
        emit(&body, 0x00);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x05);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "skip", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 7);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_sign_extension_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t i32_result, i64_result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 'e');
        emit(&sec, 'x');
        emit(&sec, 't');
        emit(&sec, '3');
        emit(&sec, '2');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 'e');
        emit(&sec, 'x');
        emit(&sec, 't');
        emit(&sec, '6');
        emit(&sec, '4');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 65408);
        emit(&body, 0xC0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 32769);
        emit(&body, 0xC1);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 128);
        emit(&body, 0xC2);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 32769);
        emit(&body, 0xC3);
        emit(&body, 0x7C);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 4294967295LL);
        emit(&body, 0xC4);
        emit(&body, 0x7C);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "ext32", NULL, 0, &i32_result, 1);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_free_module(m);
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "ext64", NULL, 0, &i64_result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, i32_result.of.i32, -32895);
        WASM_CHECK_I64(t, i64_result.of.i64, -32896);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_exported_ctor_startup) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 11);
        emit(&sec, 0x0B);
        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 17);
        emit(&sec, '_');
        emit(&sec, '_');
        emit(&sec, 'w');
        emit(&sec, 'a');
        emit(&sec, 's');
        emit(&sec, 'm');
        emit(&sec, '_');
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'l');
        emit(&sec, 'l');
        emit(&sec, '_');
        emit(&sec, 'c');
        emit(&sec, 't');
        emit(&sec, 'o');
        emit(&sec, 'r');
        emit(&sec, 's');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'g');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 77);
        emit(&body, 0x24);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 77);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_multivalue_if_block) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[1];
    wasm_value_t results[2];
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 'm');
        emit(&sec, 'v');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x04);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 11);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 22);
        emit(&body, 0x05);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 33);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 44);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(1);
    err = wasm_call(m, "mv", args, 1, results, 2);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_free_module(m);
        wasm_destroy(&rt);
        return;
    }
    if (!(results[0].of.i32 == 11 && results[1].of.i32 == 22)) {
        WL_CHECK_MSG(t, false, "%s", "unexpected true-branch multivalue results");
        wasm_free_module(m);
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(0);
    err = wasm_call(m, "mv", args, 1, results, 2);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(
            t,
            results[0].of.i32 == 33 && results[1].of.i32 == 44,
            "%s",
            "unexpected false-branch multivalue results");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_multivalue_branch_block) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t results[2];
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 'b');
        emit(&sec, 'r');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x02);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x0C);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "br", NULL, 0, results, 2);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(
            t,
            results[0].of.i32 == 7 && results[1].of.i32 == 9,
            "%s",
            "unexpected multivalue branch results");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_multivalue_loop_params) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t results[2];
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'l');
        emit(&sec, 'o');
        emit(&sec, 'o');
        emit(&sec, 'p');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x7F);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x03);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0C);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "loop", NULL, 0, results, 2);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(
            t,
            results[0].of.i32 == 4 && results[1].of.i32 == 9,
            "%s",
            "unexpected multivalue loop results");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_trunc_sat_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint32_t index;
    const char* names[8] = { "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7" };
    const int is_i64[8] = { 0, 0, 0, 0, 1, 1, 1, 1 };
    const int32_t expected_i32[8] = {
        INT32_MAX,
        -1,
        INT32_MIN,
        0,
        0,
        0,
        0,
        0
    };
    const int64_t expected_i64[8] = {
        0,
        0,
        0,
        0,
        INT64_MAX,
        0,
        INT64_MIN,
        -1
    };

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        for (index = 0; index < 8; index++) {
            emit_leb128_u32(&sec, 2);
            emit(&sec, names[index][0]);
            emit(&sec, names[index][1]);
            emit(&sec, 0x00);
            emit_leb128_u32(&sec, index);
        }
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 8);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x43);
        emit_f32(&body, 1.0e30f);
        emit(&body, 0xFC);
        emit(&body, 0x00);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x43);
        emit_f32(&body, 1.0e30f);
        emit(&body, 0xFC);
        emit(&body, 0x01);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x44);
        emit_f64(&body, -1.0e300);
        emit(&body, 0xFC);
        emit(&body, 0x02);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x44);
        emit_f64(&body, -123.0);
        emit(&body, 0xFC);
        emit(&body, 0x03);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x43);
        emit_f32(&body, 1.0e30f);
        emit(&body, 0xFC);
        emit(&body, 0x04);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x43);
        emit_f32(&body, -42.0f);
        emit(&body, 0xFC);
        emit(&body, 0x05);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x44);
        emit_f64(&body, -1.0e300);
        emit(&body, 0xFC);
        emit(&body, 0x06);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x44);
        emit_f64(&body, 1.0e300);
        emit(&body, 0xFC);
        emit(&body, 0x07);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    for (index = 0; index < 8; index++) {
        err = wasm_call(m, names[index], NULL, 0, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err != WASM_OK) {
            wasm_free_module(m);
            wasm_destroy(&rt);
            return;
        }
        if (!is_i64[index] && result.of.i32 != expected_i32[index]) {
            WASM_CHECK_I32(t, result.of.i32, expected_i32[index]);
            wasm_free_module(m);
            wasm_destroy(&rt);
            return;
        }
        if (is_i64[index] && result.of.i64 != expected_i64[index]) {
            WASM_CHECK_I64(t, result.of.i64, expected_i64[index]);
            wasm_free_module(m);
            wasm_destroy(&rt);
            return;
        }
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_tail_call_self_recursion) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[1];
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 9);
        emit(&sec, 'c');
        emit(&sec, 'o');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 't');
        emit(&sec, 'd');
        emit(&sec, 'o');
        emit(&sec, 'w');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x45);
        emit(&body, 0x04);
        emit(&body, 0x7F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 42);
        emit(&body, 0x05);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x6B);
        emit(&body, 0x12);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(2000);
    err = wasm_call(m, "countdown", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 42);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_tail_call_indirect) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'l');
        emit(&sec, 'l');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 77);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x13);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "call", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 77);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_exception_try_catch_payload) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[1];
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 6);
        emit(&sec, 'h');
        emit(&sec, 'a');
        emit(&sec, 'n');
        emit(&sec, 'd');
        emit(&sec, 'l');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x45);
        emit(&body, 0x04);
        emit(&body, 0x7F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 42);
        emit(&body, 0x05);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x07);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 5);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(0);
    err = wasm_call(m, "handle", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);

    args[0] = wasm_i32(9);
    err = wasm_call(m, "handle", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 14);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_exception_catch_all) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 'c');
        emit(&sec, 'a');
        emit(&sec, 'l');
        emit(&sec, 'l');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x19);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 55);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "call", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 55);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_exception_rethrow) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 7);
        emit(&sec, 'r');
        emit(&sec, 'e');
        emit(&sec, 't');
        emit(&sec, 'h');
        emit(&sec, 'r');
        emit(&sec, 'o');
        emit(&sec, 'w');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x07);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x09);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x07);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 100);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "rethrow", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 109);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_exception_delegate) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 8);
        emit(&sec, 'd');
        emit(&sec, 'e');
        emit(&sec, 'l');
        emit(&sec, 'e');
        emit(&sec, 'g');
        emit(&sec, 'a');
        emit(&sec, 't');
        emit(&sec, 'e');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x06);
        emit(&body, 0x7F);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x18);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x07);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "delegate", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 8);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_feature_gate_exceptions_disabled) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 0);

    wasm_init(&rt);
    wasm_disable_feature(&rt, WASM_FEATURE_EXCEPTIONS);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected disabled exceptions feature to reject module load");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "disabled") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_feature_gate_tail_call_disabled) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x12);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    wasm_disable_feature(&rt, WASM_FEATURE_TAIL_CALL);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected disabled tail-call feature to reject module load");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "disabled") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_feature_gate_sign_extension_disabled) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 255);
        emit(&body, 0xC0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    wasm_disable_feature(&rt, WASM_FEATURE_SIGN_EXT);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected disabled sign-extension feature to reject module load");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "disabled") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_runtime_config_defaults) {
    wasm_runtime_t rt;
    wasm_config_t cfg;

    wasm_config_default(&cfg);
    WL_CHECK_MSG(t, cfg.max_stack_values == WASM_MAX_STACK,
                 "expected default max_stack_values %u, got %u",
                 (unsigned)WASM_MAX_STACK,
                 (unsigned)cfg.max_stack_values);
    WL_CHECK_MSG(t, cfg.max_call_depth == WASM_MAX_CALL_DEPTH,
                 "expected default max_call_depth %u, got %u",
                 (unsigned)WASM_MAX_CALL_DEPTH,
                 (unsigned)cfg.max_call_depth);
    WL_CHECK_MSG(t, cfg.max_labels == WASM_MAX_LABELS,
                 "expected default max_labels %u, got %u",
                 (unsigned)WASM_MAX_LABELS,
                 (unsigned)cfg.max_labels);
    WL_CHECK_MSG(t, cfg.initial_gc_heap_size == (size_t)WASM_GC_HEAP_SIZE,
                 "expected default GC heap size %zu, got %zu",
                 (size_t)WASM_GC_HEAP_SIZE,
                 cfg.initial_gc_heap_size);
    WL_CHECK_MSG(t, cfg.frame_arena_size == (size_t)WASM_FRAME_ARENA_SIZE,
                 "expected default frame arena size %zu, got %zu",
                 (size_t)WASM_FRAME_ARENA_SIZE,
                 cfg.frame_arena_size);

    wasm_init(&rt);
    WL_CHECK_MSG(t, rt.max_stack == cfg.max_stack_values,
                 "expected runtime max_stack %u, got %u",
                 (unsigned)cfg.max_stack_values,
                 (unsigned)rt.max_stack);
    WL_CHECK_MSG(t, rt.max_call_frames == cfg.max_call_depth,
                 "expected runtime max_call_frames %u, got %u",
                 (unsigned)cfg.max_call_depth,
                 (unsigned)rt.max_call_frames);
    WL_CHECK_MSG(t, rt.max_labels == cfg.max_labels,
                 "expected runtime max_labels %u, got %u",
                 (unsigned)cfg.max_labels,
                 (unsigned)rt.max_labels);
    WL_CHECK_MSG(t, rt.stack != NULL, "%s", "expected value stack allocation");
    WL_CHECK_MSG(t, rt.call_frames != NULL, "%s", "expected call frame allocation");
    WL_CHECK_MSG(t, rt.backtrace_func_indices != NULL, "%s", "expected backtrace function buffer");
    WL_CHECK_MSG(t, rt.backtrace_offsets != NULL, "%s", "expected backtrace offset buffer");
    WL_CHECK_MSG(t, rt.gc_heap_size == cfg.initial_gc_heap_size,
                 "expected GC heap size %zu, got %zu",
                 cfg.initial_gc_heap_size,
                 rt.gc_heap_size);
    WL_CHECK_MSG(t, rt.frame_arena_size == cfg.frame_arena_size,
                 "expected frame arena size %zu, got %zu",
                 cfg.frame_arena_size,
                 rt.frame_arena_size);
    wasm_destroy(&rt);
}

WL_TEST(test_runtime_config_custom_allocations) {
    wasm_runtime_t rt;
    wasm_config_t cfg;

    wasm_config_default(&cfg);
    cfg.max_stack_values = 32;
    cfg.max_call_depth = 4;
    cfg.max_labels = 8;
    cfg.initial_gc_heap_size = 256;
    cfg.frame_arena_size = 64;

    wasm_init_config(&rt, &cfg);
    WL_CHECK_MSG(t, rt.max_stack == 32, "expected max_stack 32, got %u", (unsigned)rt.max_stack);
    WL_CHECK_MSG(t, rt.max_call_frames == 4,
                 "expected max_call_frames 4, got %u",
                 (unsigned)rt.max_call_frames);
    WL_CHECK_MSG(t, rt.max_labels == 8, "expected max_labels 8, got %u", (unsigned)rt.max_labels);
    WL_CHECK_MSG(t, rt.gc_heap_size == 256, "expected gc_heap_size 256, got %zu", rt.gc_heap_size);
    WL_CHECK_MSG(t, rt.frame_arena_size == 64,
                 "expected frame_arena_size 64, got %zu",
                 rt.frame_arena_size);
    WL_CHECK_MSG(t, rt.stack != NULL, "%s", "expected configured value stack allocation");
    WL_CHECK_MSG(t, rt.call_frames != NULL, "%s", "expected configured call frame allocation");
    WL_CHECK_MSG(t, rt.backtrace_func_indices != NULL, "%s", "expected configured backtrace function buffer");
    WL_CHECK_MSG(t, rt.backtrace_offsets != NULL, "%s", "expected configured backtrace offset buffer");
    wasm_destroy(&rt);
}

WL_TEST(test_runtime_config_rejects_zero_limits) {
    wasm_runtime_t rt;
    wasm_config_t cfg;
    wasm_error_t err;

    memset(&rt, 0, sizeof(rt));
    wasm_config_default(&cfg);
    cfg.max_stack_values = 0;
    err = wasm_test_init_raw(&rt, &cfg);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED,
                 "expected WASM_ERR_MALFORMED for zero stack limit, got %d",
                 (int)err);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "invalid runtime config") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);

    memset(&rt, 0, sizeof(rt));
    wasm_config_default(&cfg);
    cfg.max_call_depth = 0;
    err = wasm_test_init_raw(&rt, &cfg);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED,
                 "expected WASM_ERR_MALFORMED for zero call depth, got %d",
                 (int)err);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "invalid runtime config") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);

    memset(&rt, 0, sizeof(rt));
    wasm_config_default(&cfg);
    cfg.max_labels = 0;
    err = wasm_test_init_raw(&rt, &cfg);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED,
                 "expected WASM_ERR_MALFORMED for zero label limit, got %d",
                 (int)err);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "invalid runtime config") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_runtime_config_limits_operand_stack_validation) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_config_t cfg;
    uint32_t i;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        for (i = 0; i < 9; i++) {
            emit(&body, 0x41);
            emit_leb128_i32(&body, (int32_t)i);
        }
        for (i = 0; i < 8; i++) emit(&body, 0x1A);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_config_default(&cfg);
    cfg.max_stack_values = 8;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected load to fail when validator stack exceeds configured limit");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "operand stack overflow") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_runtime_config_limits_label_depth_validation) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_config_t cfg;
    uint32_t i;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        for (i = 0; i < 4; i++) {
            emit(&body, 0x02);
            emit(&body, 0x40);
        }
        for (i = 0; i < 4; i++) emit(&body, 0x0B);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_config_default(&cfg);
    cfg.max_labels = 4;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected load to fail when control depth exceeds configured limit");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "control stack overflow") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_runtime_config_limits_call_depth_and_backtrace) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    wasm_config_t cfg;
    uint32_t func_idx = UINT32_MAX;
    uint32_t offset = UINT32_MAX;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_config_default(&cfg);
    cfg.max_call_depth = 4;
    cfg.frame_arena_size = 128;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_CALL_STACK_EXHAUSTED,
                 "expected WASM_ERR_CALL_STACK_EXHAUSTED, got %d",
                 (int)err);
    WL_CHECK_MSG(t, wasm_get_call_stack_depth(&rt) == 4,
                 "expected backtrace depth 4, got %u",
                 (unsigned)wasm_get_call_stack_depth(&rt));
    WASM_CHECK_OK(t, wasm_get_call_frame_info(&rt, 0, &func_idx, &offset));
    WL_CHECK_MSG(t, func_idx == 0, "expected top backtrace function 0, got %u", (unsigned)func_idx);
    WASM_CHECK_OK(t, wasm_get_call_frame_info(&rt, 3, &func_idx, &offset));
    WL_CHECK_MSG(t, func_idx == 0, "expected deepest captured function 0, got %u", (unsigned)func_idx);
    WL_CHECK_MSG(t, wasm_get_call_frame_info(&rt, 4, &func_idx, &offset) == WASM_ERR_MALFORMED,
                 "%s",
                 "expected querying beyond configured backtrace depth to fail");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_frame_arena_allocator) {
    wasm_runtime_t rt;
    void* first;
    void* second;
    void* third;

    memset(&rt, 0, sizeof(rt));

    WASM_CHECK_OK(t, wasm__arena_init(&rt, 64));
    WL_CHECK_MSG(t, rt.frame_arena != NULL, "%s", "expected arena storage");
    WL_CHECK_MSG(t, rt.frame_arena_size == 64, "expected arena size 64, got %zu", rt.frame_arena_size);
    WL_CHECK_MSG(t, rt.frame_arena_offset == 0, "expected zero offset, got %zu", rt.frame_arena_offset);

    first = wasm__arena_alloc(&rt, 1);
    second = wasm__arena_alloc(&rt, 8);
    WL_CHECK_MSG(t, first != NULL, "%s", "expected first arena allocation to succeed");
    WL_CHECK_MSG(t, second != NULL, "%s", "expected second arena allocation to succeed");
    WL_CHECK_MSG(t, ((uintptr_t)first % sizeof(void*)) == 0, "%s", "expected aligned first allocation");
    WL_CHECK_MSG(t, ((uintptr_t)second % sizeof(void*)) == 0, "%s", "expected aligned second allocation");

    wasm__arena_reset(&rt, 0);
    WL_CHECK_MSG(t, rt.frame_arena_offset == 0, "expected reset offset 0, got %zu", rt.frame_arena_offset);

    third = wasm__arena_alloc(&rt, 64);
    WL_CHECK_MSG(t, third == rt.frame_arena, "%s", "expected reset to rewind to the arena base");
    WL_CHECK_MSG(t, wasm__arena_alloc(&rt, 1) == NULL, "%s", "expected exhaustion to return NULL");

    wasm__arena_destroy(&rt);
    WL_CHECK_MSG(t, rt.frame_arena == NULL, "%s", "expected arena destroy to clear storage");
    WL_CHECK_MSG(t, rt.frame_arena_size == 0, "expected arena size 0, got %zu", rt.frame_arena_size);
    WL_CHECK_MSG(t, rt.frame_arena_offset == 0, "expected arena offset 0, got %zu", rt.frame_arena_offset);
}

WL_TEST(test_runtime_grows_frame_arena_for_execution) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    wasm_config_t cfg;
    size_t frame_bytes = 0;
    size_t expected_size = 0;
    size_t required_size = 0;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'r');
        emit(&sec, 'u');
        emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x7E);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_config_default(&cfg);
    cfg.max_call_depth = 2;
    cfg.frame_arena_size = 64;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, wasm__module_frame_arena_requirement(m, &required_size), "%s", "expected arena sizing for module to succeed");
    WL_CHECK_MSG(t, wasm__frame_storage_size(m->funcs[0].num_locals, m->funcs[0].max_label_depth ? m->funcs[0].max_label_depth : 1u, &frame_bytes),
                 "%s",
                 "expected frame sizing for function to succeed");
    WL_CHECK_MSG(t, wasm__size_mul(frame_bytes, (size_t)cfg.max_call_depth, &expected_size),
                 "%s",
                 "expected frame arena requirement multiplication to succeed");
    if (expected_size < cfg.frame_arena_size) expected_size = cfg.frame_arena_size;
    WL_CHECK_MSG(t, required_size == expected_size,
                 "expected arena requirement %zu, got %zu",
                 expected_size,
                 required_size);
    WL_CHECK_MSG(t, rt.frame_arena_size == cfg.frame_arena_size,
                 "expected load to preserve undersized arena until execution, got %zu",
                 rt.frame_arena_size);

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, result.of.i32, 7);
        WL_CHECK_MSG(t, rt.frame_arena_size >= required_size,
                     "expected runtime arena to grow to at least %zu bytes, got %zu",
                     required_size,
                     rt.frame_arena_size);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_duplicate_export_names) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'd');
        emit(&sec, 'u');
        emit(&sec, 'p');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);

        emit_leb128_u32(&sec, 3);
        emit(&sec, 'd');
        emit(&sec, 'u');
        emit(&sec, 'p');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected duplicate exports to fail validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "duplicate export") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_invalid_start_signature) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 8, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected invalid start signature to fail validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "start function") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_stack_type_errors) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected stack type error to fail validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "type mismatch") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_requires_data_count_for_memory_init) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 1 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_bytes(&sec, data_bytes, 1);
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFC);
        emit_leb128_u32(&body, 0x08);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected missing data count section to fail validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "data count section") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_code_body_count_mismatch) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected code/body count mismatch to fail validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "code section count") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_data_count_mismatch) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 12, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 0xAA };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_bytes(&sec, data_bytes, 1);
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected data count mismatch to fail validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "data count section declares") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_call_indirect_on_externref_table) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6F);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected call_indirect on externref table to fail validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "funcref table") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_invalid_load_alignment) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x28);
        emit_leb128_u32(&body, 3);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected invalid load alignment to fail validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "alignment") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

/* ── Test 6: error handling ─────────────────────────────────────── */

WL_TEST(test_bad_magic) {
    uint8_t bad[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x00, 0x00, 0x00 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_load(&rt, bad, sizeof(bad));
    if (m) {
        WL_CHECK_MSG(t, false, "%s", "should have rejected");
        wasm_free_module(m);
    } else {
        WL_CHECK(t, rt.last_error == WASM_ERR_INVALID_MAGIC);
    }
    wasm_destroy(&rt);
}

WL_TEST(test_div_by_zero) {
    wasm_builder_t mod = { 0 };
    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 'f');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        wasm_builder_t body = { 0 };
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 10);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x6D); /* i32.div_s */
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "f", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_DIV_BY_ZERO, "%s", "expected div by zero trap");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_public_backtrace_helpers_capture_traps) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t bad_arg;
    wasm_error_t err;
    uint32_t func_idx;
    uint32_t offset;
    char backtrace[256];

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body0 = { 0 };
        wasm_builder_t body1 = { 0 };
        wasm_builder_t body2 = { 0 };

        emit_leb128_u32(&sec, 3);

        emit_leb128_u32(&body0, 0);
        emit(&body0, 0x10);
        emit_leb128_u32(&body0, 1);
        emit(&body0, 0x0B);
        emit_leb128_u32(&sec, body0.len);
        emit_bytes(&sec, body0.buf, body0.len);

        emit_leb128_u32(&body1, 0);
        emit(&body1, 0x10);
        emit_leb128_u32(&body1, 2);
        emit(&body1, 0x0B);
        emit_leb128_u32(&sec, body1.len);
        emit_bytes(&sec, body1.buf, body1.len);

        emit_leb128_u32(&body2, 0);
        emit(&body2, 0x00);
        emit(&body2, 0x0B);
        emit_leb128_u32(&sec, body2.len);
        emit_bytes(&sec, body2.buf, body2.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, wasm_get_call_stack_depth(&rt) == 0, "%s", "expected empty initial backtrace");

    err = wasm_call(m, "run", NULL, 0, NULL, 0);
    WL_CHECK_MSG(t, err == WASM_ERR_UNREACHABLE, "%s", "expected unreachable trap");

    WASM_CHECK_I32(t, (int32_t)wasm_get_call_stack_depth(&rt), 3);

    err = wasm_get_call_frame_info(&rt, 0, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 2);
        WASM_CHECK_I32(t, (int32_t)offset, 0);
    }

    err = wasm_get_call_frame_info(&rt, 1, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 1);
        WASM_CHECK_I32(t, (int32_t)offset, 0);
    }

    err = wasm_get_call_frame_info(&rt, 2, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 0);
        WASM_CHECK_I32(t, (int32_t)offset, 0);
    }

    wasm_dump_backtrace(&rt, backtrace, sizeof(backtrace));
    WL_CHECK_MSG(t, strstr(backtrace, "#0 func[2] at offset 0x0000") != NULL,
                 "%s", "expected top frame in formatted backtrace");
    WL_CHECK_MSG(t, strstr(backtrace, "#1 func[1] at offset 0x0000") != NULL,
                 "%s", "expected middle frame in formatted backtrace");
    WL_CHECK_MSG(t, strstr(backtrace, "#2 func[0] at offset 0x0000") != NULL,
                 "%s", "expected root frame in formatted backtrace");

    err = wasm_get_call_frame_info(&rt, 3, &func_idx, &offset);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected out-of-range frame query rejection");

    bad_arg = wasm_i32(7);
    err = wasm_call(m, "run", &bad_arg, 1, NULL, 0);
    WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected signature mismatch rejection");
    WL_CHECK_MSG(t, wasm_get_call_stack_depth(&rt) == 0, "%s", "expected backtrace reset before a non-executing call failure");

    wasm_dump_backtrace(&rt, backtrace, sizeof(backtrace));
    WL_CHECK_MSG(t, backtrace[0] == '\0', "%s", "expected empty backtrace string after reset");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_public_fuel_helpers_account_for_finite_calls) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 42);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_I64(t, (int64_t)wasm_get_fuel(&rt), 0);

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_I64(t, (int64_t)wasm_get_fuel(&rt), 0);

    wasm_set_fuel(&rt, 2);
    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_I64(t, (int64_t)wasm_get_fuel(&rt), 0);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_public_fuel_helpers_trap_in_infinite_loop) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    uint32_t func_idx;
    uint32_t offset;
    char backtrace[128];

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "spin", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x03);
        emit(&body, 0x40);
        emit(&body, 0x0C);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    wasm_set_fuel(&rt, 5);
    err = wasm_call(m, "spin", NULL, 0, NULL, 0);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_FUEL, "%s", "expected out of fuel trap");
    WASM_CHECK_I64(t, (int64_t)wasm_get_fuel(&rt), 0);
    WASM_CHECK_I32(t, (int32_t)wasm_get_call_stack_depth(&rt), 1);

    err = wasm_get_call_frame_info(&rt, 0, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 0);
        WASM_CHECK_I32(t, (int32_t)offset, 2);
    }

    wasm_dump_backtrace(&rt, backtrace, sizeof(backtrace));
    WL_CHECK_MSG(t, strstr(backtrace, "#0 func[0] at offset 0x0002") != NULL,
                 "%s", "expected fuel trap backtrace frame");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_public_wasi_stub_helpers) {
    static const uint8_t wasi_message[] = "hello wasi\n";
    static const uint8_t wasi_iov[] = {
        64u,
        0u,
        0u,
        0u,
        (uint8_t)sizeof(wasi_message) - 1u,
        0u,
        0u,
        0u,
    };
    static const uint8_t wasi_seek_initial[8] = {
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
    };

    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_value_t result;
    wasm_test_stream_capture capture;
    uint8_t nwritten_bytes[4];
    uint8_t seek_offset_bytes[8];
    uint32_t nwritten = 0;
    uint64_t seek_offset = UINT64_MAX;
    char output[64];
    size_t output_len;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 7);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 5);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_write", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_get", 1);
        emit_import_func(&sec, "wasi_snapshot_preview1", "proc_exit", 2);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_close", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_seek", 4);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 6);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 5);
        emit_export_func(&sec, "run", 5);
        emit_export_func(&sec, "env", 6);
        emit_export_func(&sec, "close", 7);
        emit_export_func(&sec, "seek", 8);
        emit_export_func(&sec, "quit", 9);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t run_body = { 0 };
        wasm_builder_t env_body = { 0 };
        wasm_builder_t close_body = { 0 };
        wasm_builder_t seek_body = { 0 };
        wasm_builder_t quit_body = { 0 };

        emit_leb128_u32(&sec, 5);

        emit_leb128_u32(&run_body, 0);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 32);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 48);
        emit(&run_body, 0x10);
        emit_leb128_u32(&run_body, 0);
        emit(&run_body, 0x0B);
        emit_leb128_u32(&sec, run_body.len);
        emit_bytes(&sec, run_body.buf, run_body.len);

        emit_leb128_u32(&env_body, 0);
        emit(&env_body, 0x41);
        emit_leb128_i32(&env_body, 0);
        emit(&env_body, 0x41);
        emit_leb128_i32(&env_body, 0);
        emit(&env_body, 0x10);
        emit_leb128_u32(&env_body, 1);
        emit(&env_body, 0x0B);
        emit_leb128_u32(&sec, env_body.len);
        emit_bytes(&sec, env_body.buf, env_body.len);

        emit_leb128_u32(&close_body, 0);
        emit(&close_body, 0x41);
        emit_leb128_i32(&close_body, 1);
        emit(&close_body, 0x10);
        emit_leb128_u32(&close_body, 3);
        emit(&close_body, 0x0B);
        emit_leb128_u32(&sec, close_body.len);
        emit_bytes(&sec, close_body.buf, close_body.len);

        emit_leb128_u32(&seek_body, 0);
        emit(&seek_body, 0x41);
        emit_leb128_i32(&seek_body, 1);
        emit(&seek_body, 0x42);
        emit_leb128_i64(&seek_body, 0);
        emit(&seek_body, 0x41);
        emit_leb128_i32(&seek_body, 1);
        emit(&seek_body, 0x41);
        emit_leb128_i32(&seek_body, 80);
        emit(&seek_body, 0x10);
        emit_leb128_u32(&seek_body, 4);
        emit(&seek_body, 0x0B);
        emit_leb128_u32(&sec, seek_body.len);
        emit_bytes(&sec, seek_body.buf, seek_body.len);

        emit_leb128_u32(&quit_body, 0);
        emit(&quit_body, 0x41);
        emit_leb128_i32(&quit_body, 5);
        emit(&quit_body, 0x10);
        emit_leb128_u32(&quit_body, 2);
        emit(&quit_body, 0x0B);
        emit_leb128_u32(&sec, quit_body.len);
        emit_bytes(&sec, quit_body.buf, quit_body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 32);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(wasi_iov));
        emit_bytes(&sec, wasi_iov, (uint32_t)sizeof(wasi_iov));

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 64);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(wasi_message) - 1u);
        emit_bytes(&sec, wasi_message, (uint32_t)sizeof(wasi_message) - 1u);

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 80);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(wasi_seek_initial));
        emit_bytes(&sec, wasi_seek_initial, (uint32_t)sizeof(wasi_seek_initial));

        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);
    err = wasm_bind_wasi_stubs(&rt);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, wasm_test_begin_stream_capture(&capture, stdout), "%s", "failed to capture stdout");
    err = wasm_call(m, "run", NULL, 0, &result, 1);
    output_len = wasm_test_end_stream_capture(&capture, output, sizeof(output));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    WL_CHECK_MSG(t, output_len == sizeof(wasi_message) - 1u,
                 "expected %u bytes of wasi output, got %zu",
                 (unsigned)(sizeof(wasi_message) - 1u), output_len);
    WL_CHECK_MSG(t, strcmp(output, (const char*)wasi_message) == 0,
                 "expected captured wasi output '%s', got '%s'",
                 (const char*)wasi_message,
                 output);

    err = wasm_memory_read(m, 0, 48, nwritten_bytes, sizeof(nwritten_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        nwritten = wasm__load_le32(nwritten_bytes);
        WASM_CHECK_I32(t, (int32_t)nwritten, (int32_t)(sizeof(wasi_message) - 1u));
    }

    err = wasm_call(m, "env", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_call(m, "close", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_call(m, "seek", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_memory_read(m, 0, 80, seek_offset_bytes, sizeof(seek_offset_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        seek_offset = wasm__load_le64(seek_offset_bytes);
        WASM_CHECK_I64(t, (int64_t)seek_offset, 0);
    }

    err = wasm_call(m, "quit", NULL, 0, NULL, 0);
    WASM_CHECK_OK(t, err);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_public_wasi_metadata_helpers) {
    static const uint8_t random_sentinel[16] = {
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
        0xAAu,
    };
    static const uint8_t stats_sentinel[24] = {
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
    };
    static const uint8_t size_sentinel[16] = {
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
    };

    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_value_t result;
    uint8_t size_bytes[16];
    uint8_t random_bytes[16];
    uint8_t clock_bytes[8];
    uint8_t fdstat_bytes[24];
    uint64_t clock_value;
    uint64_t rights_base;
    uint64_t rights_inheriting;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 7);
        emit_import_func(&sec, "wasi_snapshot_preview1", "args_sizes_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "args_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_sizes_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "random_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "clock_time_get", 1);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 7);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 7);
        emit_export_func(&sec, "args_sizes", 7);
        emit_export_func(&sec, "args_get", 8);
        emit_export_func(&sec, "env_sizes", 9);
        emit_export_func(&sec, "env_get", 10);
        emit_export_func(&sec, "fdstat", 11);
        emit_export_func(&sec, "random", 12);
        emit_export_func(&sec, "clock", 13);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 7);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 32);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 36);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 96);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 112);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 40);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 44);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 104);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 120);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 3);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 192);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 4);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 128);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 16);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 5);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 160);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 6);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        uint8_t clock_sentinel[8];
        wasm_builder_t sec = { 0 };
        memset(clock_sentinel, 0xFF, sizeof(clock_sentinel));

        emit_leb128_u32(&sec, 4);

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 32);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(size_sentinel));
        emit_bytes(&sec, size_sentinel, (uint32_t)sizeof(size_sentinel));

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 128);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(random_sentinel));
        emit_bytes(&sec, random_sentinel, (uint32_t)sizeof(random_sentinel));

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 160);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(clock_sentinel));
        emit_bytes(&sec, clock_sentinel, (uint32_t)sizeof(clock_sentinel));

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 192);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(stats_sentinel));
        emit_bytes(&sec, stats_sentinel, (uint32_t)sizeof(stats_sentinel));

        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);
    err = wasm_bind_wasi_stubs(&rt);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "args_sizes", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 32, size_bytes, 8);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)wasm__load_le32(size_bytes), 0);
        WASM_CHECK_I32(t, (int32_t)wasm__load_le32(size_bytes + 4), 0);
    }

    err = wasm_call(m, "args_get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_call(m, "env_sizes", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 40, size_bytes + 8, 8);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)wasm__load_le32(size_bytes + 8), 0);
        WASM_CHECK_I32(t, (int32_t)wasm__load_le32(size_bytes + 12), 0);
    }

    err = wasm_call(m, "env_get", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_call(m, "fdstat", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 192, fdstat_bytes, sizeof(fdstat_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        rights_base = wasm__load_le64(fdstat_bytes + 8);
        rights_inheriting = wasm__load_le64(fdstat_bytes + 16);
        WL_CHECK_MSG(t, fdstat_bytes[0] == 2u, "expected character device filetype, got %u", (unsigned)fdstat_bytes[0]);
        WL_CHECK_MSG(t, wasm__load_le16(fdstat_bytes + 2) == 0u, "expected zero fdflags");
        WL_CHECK_MSG(t, rights_base == UINT64_MAX, "expected all base rights, got %llu", (unsigned long long)rights_base);
        WL_CHECK_MSG(t, rights_inheriting == UINT64_MAX,
                     "expected all inheriting rights, got %llu",
                     (unsigned long long)rights_inheriting);
    }

    err = wasm_call(m, "random", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 128, random_bytes, sizeof(random_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, memcmp(random_bytes, random_sentinel, sizeof(random_bytes)) != 0,
                     "%s", "expected random_get to overwrite sentinel bytes");
    }

    err = wasm_call(m, "clock", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    err = wasm_memory_read(m, 0, 160, clock_bytes, sizeof(clock_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        clock_value = wasm__load_le64(clock_bytes);
        WL_CHECK_MSG(t, clock_value != UINT64_MAX,
                     "%s", "expected clock_time_get to overwrite the sentinel timestamp");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_set_immutable_global_rejected) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 7);
        emit(&sec, 0x0B);
        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 'f');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x24);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected immutable global write rejection during load");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "immutable") != NULL, "%s", rt.error_msg);

    wasm_destroy(&rt);
}

WL_TEST(test_public_global_helpers) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t value;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x7F);
        emit(&sec, 0x01);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 7);
        emit(&sec, 0x0B);

        emit(&sec, 0x7E);
        emit(&sec, 0x00);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 99);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);
        emit_export_func(&sec, "run", 0);
        emit_export_kind(&sec, "counter", 0x03, 0);
        emit_export_kind(&sec, "limit", 0x03, 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_global_get(m, "counter", &value);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, value.type == WASM_TYPE_I32, "%s", "expected counter to be an i32 global");
        WASM_CHECK_I32(t, value.of.i32, 7);
    }

    err = wasm_global_set(m, "counter", wasm_i32(11));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        err = wasm_global_get(m, "counter", &value);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            WASM_CHECK_I32(t, value.of.i32, 11);
            WASM_CHECK_I32(t, m->globals[0].value.of.i32, 11);
        }
    }

    err = wasm_global_set(m, "counter", wasm_i64(11));
    WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected type mismatch for wrong global value type");

    err = wasm_global_set(m, "limit", wasm_i64(100));
    WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected immutable global write rejection");

    err = wasm_global_get(m, "run", &value);
    WL_CHECK_MSG(t, err == WASM_ERR_UNDEFINED_EXPORT, "%s", "expected non-global export lookup to fail");

    err = wasm_global_get(m, "missing", &value);
    WL_CHECK_MSG(t, err == WASM_ERR_UNDEFINED_EXPORT, "%s", "expected missing global export lookup to fail");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_float_min_max_semantics) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args_f32[2];
    wasm_value_t args_f64[2];
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7D);
        emit(&sec, 0x7D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7D);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7C);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        emit_export_func(&sec, "f32min", 0);
        emit_export_func(&sec, "f32max", 1);
        emit_export_func(&sec, "f64min", 2);
        emit_export_func(&sec, "f64max", 3);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        static const uint8_t ops[] = { 0x96, 0x97, 0xA4, 0xA5 };
        uint32_t i;
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);
        for (i = 0; i < 4; i++) {
            wasm_builder_t body = { 0 };
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 1);
            emit(&body, ops[i]);
            emit(&body, 0x0B);
            emit_leb128_u32(&sec, body.len);
            emit_bytes(&sec, body.buf, body.len);
        }

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    args_f32[0] = wasm_f32(-0.0f);
    args_f32[1] = wasm_f32(0.0f);
    err = wasm_call(m, "f32min", args_f32, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, result.of.f32 == 0.0f && signbit(result.of.f32), "%s", "expected f32.min(-0,+0) to return -0");
    }

    err = wasm_call(m, "f32max", args_f32, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, result.of.f32 == 0.0f && !signbit(result.of.f32), "%s", "expected f32.max(-0,+0) to return +0");
    }

    args_f32[0] = wasm_f32(NAN);
    args_f32[1] = wasm_f32(1.0f);
    err = wasm_call(m, "f32min", args_f32, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, isnan(result.of.f32), "%s", "expected f32.min(NaN, 1) to return NaN");
    }

    args_f64[0] = wasm_f64(-0.0);
    args_f64[1] = wasm_f64(0.0);
    err = wasm_call(m, "f64min", args_f64, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, result.of.f64 == 0.0 && signbit(result.of.f64), "%s", "expected f64.min(-0,+0) to return -0");
    }

    err = wasm_call(m, "f64max", args_f64, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, result.of.f64 == 0.0 && !signbit(result.of.f64), "%s", "expected f64.max(-0,+0) to return +0");
    }

    args_f64[0] = wasm_f64(1.0);
    args_f64[1] = wasm_f64(NAN);
    err = wasm_call(m, "f64max", args_f64, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, isnan(result.of.f64), "%s", "expected f64.max(1, NaN) to return NaN");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_trapping_float_to_int_conversions) {
    static const char* export_names[] = {
        "i32_trunc_f32_s",
        "i32_trunc_f32_u",
        "i32_trunc_f64_s",
        "i32_trunc_f64_u",
        "i64_trunc_f32_s",
        "i64_trunc_f32_u",
        "i64_trunc_f64_s",
        "i64_trunc_f64_u"
    };
    static const uint8_t type_indices[] = { 0, 0, 1, 1, 2, 2, 3, 3 };
    static const uint8_t ops[] = { 0xA8, 0xA9, 0xAA, 0xAB, 0xAE, 0xAF, 0xB0, 0xB1 };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint32_t i;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        for (i = 0; i < 8; i++) emit_leb128_u32(&sec, type_indices[i]);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        for (i = 0; i < 8; i++) emit_export_func(&sec, export_names[i], i);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 8);
        for (i = 0; i < 8; i++) {
            wasm_builder_t body = { 0 };
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 0);
            emit(&body, ops[i]);
            emit(&body, 0x0B);
            emit_leb128_u32(&sec, body.len);
            emit_bytes(&sec, body.buf, body.len);
        }
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    {
        wasm_value_t args[] = { wasm_f32(123.75f) };
        err = wasm_call(m, "i32_trunc_f32_s", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 123);
    }

    {
        wasm_value_t args[] = { wasm_f64(42.9) };
        err = wasm_call(m, "i64_trunc_f64_u", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, 42);
    }

    {
        wasm_value_t args[] = { wasm_f32(NAN) };
        err = wasm_call(m, "i32_trunc_f32_s", args, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f32(-1.0f) };
        err = wasm_call(m, "i32_trunc_f32_u", args, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f64(2147483648.0) };
        err = wasm_call(m, "i32_trunc_f64_s", args, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f64(4294967296.0) };
        err = wasm_call(m, "i32_trunc_f64_u", args, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f32(NAN) };
        err = wasm_call(m, "i64_trunc_f32_s", args, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f32(-1.0f) };
        err = wasm_call(m, "i64_trunc_f32_u", args, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f64(9223372036854775808.0) };
        err = wasm_call(m, "i64_trunc_f64_s", args, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    {
        wasm_value_t args[] = { wasm_f64(18446744073709551616.0) };
        err = wasm_call(m, "i64_trunc_f64_u", args, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_INVALID_CONVERSION, "%s", wasm_error_string(err));
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_rotate_by_zero) {
    static const char* export_names[] = { "rotl32", "rotr32", "rotl64", "rotr64" };
    static const uint8_t type_indices[] = { 0, 0, 1, 1 };
    static const uint8_t ops[] = { 0x77, 0x78, 0x89, 0x8A };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint32_t i;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        for (i = 0; i < 4; i++) emit_leb128_u32(&sec, type_indices[i]);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        for (i = 0; i < 4; i++) emit_export_func(&sec, export_names[i], i);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 4);
        for (i = 0; i < 4; i++) {
            wasm_builder_t body = { 0 };
            emit_leb128_u32(&body, 0);
            emit(&body, 0x20);
            emit_leb128_u32(&body, 0);
            if (i < 2) {
                emit(&body, 0x41);
                emit_leb128_i32(&body, 0);
            } else {
                emit(&body, 0x42);
                emit_leb128_i64(&body, 0);
            }
            emit(&body, ops[i]);
            emit(&body, 0x0B);
            emit_leb128_u32(&sec, body.len);
            emit_bytes(&sec, body.buf, body.len);
        }
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    {
        wasm_value_t args[] = { wasm_i32((int32_t)0x81234567u) };
        err = wasm_call(m, "rotl32", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, (int32_t)0x81234567u);

        err = wasm_call(m, "rotr32", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, (int32_t)0x81234567u);
    }

    {
        wasm_value_t args[] = { wasm_i64((int64_t)0x8123456789ABCDEFull) };
        err = wasm_call(m, "rotl64", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, (int64_t)0x8123456789ABCDEFull);

        err = wasm_call(m, "rotr64", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, (int64_t)0x8123456789ABCDEFull);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_typed_select_funcref) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[] = { wasm_i32(1) };
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "pick", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x70);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1C);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x70);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "pick", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, result.type == WASM_TYPE_FUNCREF, "%s", "expected funcref result type");
        WL_CHECK_MSG(t, result.of.funcref == 0, "%s", "expected typed select to choose ref.func 0");
    }

    args[0] = wasm_i32(0);
    err = wasm_call(m, "pick", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, result.type == WASM_TYPE_FUNCREF, "%s", "expected funcref result type");
        WL_CHECK_MSG(t, result.of.funcref == UINT32_MAX, "%s", "expected typed select to choose ref.null");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_simd_helper_ops) {
    static const int8_t i8_a[16] = { -8, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7 };
    static const int8_t i8_b[16] = { 7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, -8 };
    static const int16_t i16_a[8] = { -300, -200, -100, -1, 1, 100, 200, 300 };
    static const int16_t i16_b[8] = { 150, -150, 50, -50, 25, -25, 10, -10 };
    static const int32_t i32_a[4] = { -100000, -1000, 1000, 100000 };
    static const int32_t i32_b[4] = { 33333, -2000, 2000, -33333 };
    static const int64_t i64_a[2] = { -5000000000LL, 5000000000LL };
    static const int64_t i64_b[2] = { 3000000000LL, -3000000000LL };
    static const float f32_a[4] = { 1.25f, -2.5f, 3.75f, -4.0f };
    static const float f32_b[4] = { -0.5f, 4.0f, -1.25f, 8.0f };
    static const double f64_a[2] = { 1.5, -8.0 };
    static const double f64_b[2] = { -0.25, 2.0 };
    static const uint32_t i8_cmp_ops[] = { 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C };
    static const uint32_t i16_cmp_ops[] = { 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36 };
    static const uint32_t i32_cmp_ops[] = { 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40 };
    static const uint32_t i64_cmp_ops[] = { 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB };
    static const uint32_t f32_cmp_ops[] = { 0x41, 0x42, 0x43, 0x44, 0x45, 0x46 };
    static const uint32_t f64_cmp_ops[] = { 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C };
    static const uint32_t i8_binary_ops[] = { 0x65, 0x66, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x76, 0x77, 0x78, 0x79, 0x7B };
    static const uint32_t i16_binary_ops[] = { 0x82, 0x85, 0x86, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F };
    static const uint32_t i32_binary_ops[] = { 0xAE, 0xB1, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBC, 0xBD, 0xBE, 0xBF };
    static const uint32_t i64_binary_ops[] = { 0xCE, 0xD1, 0xD5, 0xDC, 0xDD, 0xDE, 0xDF };
    wasm_value_t vi8_a = wasm_test_v128_from_i8x16(i8_a);
    wasm_value_t vi8_b = wasm_test_v128_from_i8x16(i8_b);
    wasm_value_t vi16_a = wasm_test_v128_from_i16x8(i16_a);
    wasm_value_t vi16_b = wasm_test_v128_from_i16x8(i16_b);
    wasm_value_t vi32_a = wasm_test_v128_from_i32x4(i32_a);
    wasm_value_t vi32_b = wasm_test_v128_from_i32x4(i32_b);
    wasm_value_t vi64_a = wasm_test_v128_from_i64x2(i64_a);
    wasm_value_t vi64_b = wasm_test_v128_from_i64x2(i64_b);
    wasm_value_t vf32_a = wasm_test_v128_from_f32x4(f32_a);
    wasm_value_t vf32_b = wasm_test_v128_from_f32x4(f32_b);
    wasm_value_t vf64_a = wasm_test_v128_from_f64x2(f64_a);
    wasm_value_t vf64_b = wasm_test_v128_from_f64x2(f64_b);
    uint32_t index;

    {
        uint8_t bytes[16];
        uint8_t expected[16];
        wasm_value_t actual;
        wasm_value_t input;
        wasm_value_t zero = wasm_v128_zero();

        for (index = 0; index < 16; index++) bytes[index] = (uint8_t)index;
        input = wasm_v128(bytes);
        actual = wasm__simd_bitwise_not(&input);
        for (index = 0; index < 16; index++) expected[index] = (uint8_t)~bytes[index];
        wasm_test_expect_u8x16(t, &actual, expected, "v128.not");

        actual = wasm__simd_bitwise_binary(0x4E, &vi8_a, &vi8_b);
        for (index = 0; index < 16; index++) expected[index] = (uint8_t)i8_a[index] & (uint8_t)i8_b[index];
        wasm_test_expect_u8x16(t, &actual, expected, "v128.and");

        actual = wasm__simd_bitwise_binary(0x50, &vi8_a, &vi8_b);
        for (index = 0; index < 16; index++) expected[index] = (uint8_t)i8_a[index] | (uint8_t)i8_b[index];
        wasm_test_expect_u8x16(t, &actual, expected, "v128.or");

        WL_CHECK_MSG(t, wasm__simd_any_true(&vi8_a), "%s", "expected non-zero vector to be any_true");
        WL_CHECK_MSG(t, !wasm__simd_any_true(&zero), "%s", "expected zero vector to be falsey");
    }

    for (index = 0; index < (uint32_t)(sizeof(i8_cmp_ops) / sizeof(i8_cmp_ops[0])); index++) {
        uint8_t expected[16];
        wasm_value_t actual = wasm__simd_cmp_i8x16(i8_cmp_ops[index], &vi8_a, &vi8_b);
        uint32_t lane;

        for (lane = 0; lane < 16; lane++) {
            int8_t a = i8_a[lane];
            int8_t b = i8_b[lane];
            uint8_t ua = (uint8_t)a;
            uint8_t ub = (uint8_t)b;
            int cond = 0;
            switch (i8_cmp_ops[index]) {
                case 0x23:
                    cond = a == b;
                    break;
                case 0x24:
                    cond = a != b;
                    break;
                case 0x25:
                    cond = a < b;
                    break;
                case 0x26:
                    cond = ua < ub;
                    break;
                case 0x27:
                    cond = a > b;
                    break;
                case 0x28:
                    cond = ua > ub;
                    break;
                case 0x29:
                    cond = a <= b;
                    break;
                case 0x2A:
                    cond = ua <= ub;
                    break;
                case 0x2B:
                    cond = a >= b;
                    break;
                default:
                    cond = ua >= ub;
                    break;
            }
            expected[lane] = cond ? 0xFFu : 0u;
        }
        wasm_test_expect_u8x16(t, &actual, expected, "i8x16 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(i16_cmp_ops) / sizeof(i16_cmp_ops[0])); index++) {
        int16_t expected[8];
        wasm_value_t actual = wasm__simd_cmp_i16x8(i16_cmp_ops[index], &vi16_a, &vi16_b);
        uint32_t lane;

        for (lane = 0; lane < 8; lane++) {
            int cond = 0;
            uint16_t ua = (uint16_t)i16_a[lane];
            uint16_t ub = (uint16_t)i16_b[lane];
            switch (i16_cmp_ops[index]) {
                case 0x2D:
                    cond = i16_a[lane] == i16_b[lane];
                    break;
                case 0x2E:
                    cond = i16_a[lane] != i16_b[lane];
                    break;
                case 0x2F:
                    cond = i16_a[lane] < i16_b[lane];
                    break;
                case 0x30:
                    cond = ua < ub;
                    break;
                case 0x31:
                    cond = i16_a[lane] > i16_b[lane];
                    break;
                case 0x32:
                    cond = ua > ub;
                    break;
                case 0x33:
                    cond = i16_a[lane] <= i16_b[lane];
                    break;
                case 0x34:
                    cond = ua <= ub;
                    break;
                case 0x35:
                    cond = i16_a[lane] >= i16_b[lane];
                    break;
                default:
                    cond = ua >= ub;
                    break;
            }
            expected[lane] = cond ? (int16_t)-1 : 0;
        }
        wasm_test_expect_i16x8(t, &actual, expected, "i16x8 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(i32_cmp_ops) / sizeof(i32_cmp_ops[0])); index++) {
        int32_t expected[4];
        wasm_value_t actual = wasm__simd_cmp_i32x4(i32_cmp_ops[index], &vi32_a, &vi32_b);
        uint32_t lane;

        for (lane = 0; lane < 4; lane++) {
            int cond = 0;
            uint32_t ua = (uint32_t)i32_a[lane];
            uint32_t ub = (uint32_t)i32_b[lane];
            switch (i32_cmp_ops[index]) {
                case 0x37:
                    cond = i32_a[lane] == i32_b[lane];
                    break;
                case 0x38:
                    cond = i32_a[lane] != i32_b[lane];
                    break;
                case 0x39:
                    cond = i32_a[lane] < i32_b[lane];
                    break;
                case 0x3A:
                    cond = ua < ub;
                    break;
                case 0x3B:
                    cond = i32_a[lane] > i32_b[lane];
                    break;
                case 0x3C:
                    cond = ua > ub;
                    break;
                case 0x3D:
                    cond = i32_a[lane] <= i32_b[lane];
                    break;
                case 0x3E:
                    cond = ua <= ub;
                    break;
                case 0x3F:
                    cond = i32_a[lane] >= i32_b[lane];
                    break;
                default:
                    cond = ua >= ub;
                    break;
            }
            expected[lane] = cond ? -1 : 0;
        }
        wasm_test_expect_i32x4(t, &actual, expected, "i32x4 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(i64_cmp_ops) / sizeof(i64_cmp_ops[0])); index++) {
        int64_t expected[2];
        wasm_value_t actual = wasm__simd_cmp_i64x2(i64_cmp_ops[index], &vi64_a, &vi64_b);
        uint32_t lane;

        for (lane = 0; lane < 2; lane++) {
            int cond = 0;
            switch (i64_cmp_ops[index]) {
                case 0xD6:
                    cond = i64_a[lane] == i64_b[lane];
                    break;
                case 0xD7:
                    cond = i64_a[lane] != i64_b[lane];
                    break;
                case 0xD8:
                    cond = i64_a[lane] < i64_b[lane];
                    break;
                case 0xD9:
                    cond = i64_a[lane] > i64_b[lane];
                    break;
                case 0xDA:
                    cond = i64_a[lane] <= i64_b[lane];
                    break;
                default:
                    cond = i64_a[lane] >= i64_b[lane];
                    break;
            }
            expected[lane] = cond ? -1LL : 0LL;
        }
        wasm_test_expect_i64x2(t, &actual, expected, "i64x2 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(i8_binary_ops) / sizeof(i8_binary_ops[0])); index++) {
        wasm_value_t actual = wasm__simd_i8x16_binary(i8_binary_ops[index], &vi16_a, &vi16_b);
        uint8_t expected[16];
        uint32_t lane;

        memset(expected, 0, sizeof(expected));
        if (i8_binary_ops[index] == 0x65 || i8_binary_ops[index] == 0x66) {
            for (lane = 0; lane < 8; lane++) {
                int32_t lo = i16_a[lane];
                int32_t hi = i16_b[lane];
                expected[lane] = (i8_binary_ops[index] == 0x65) ? (uint8_t)wasm__sat_i8(lo) : wasm__sat_u8(lo);
                expected[lane + 8u] = (i8_binary_ops[index] == 0x65) ? (uint8_t)wasm__sat_i8(hi) : wasm__sat_u8(hi);
            }
        } else {
            actual = wasm__simd_i8x16_binary(i8_binary_ops[index], &vi8_a, &vi8_b);
            for (lane = 0; lane < 16; lane++) {
                int8_t a = i8_a[lane];
                int8_t b = i8_b[lane];
                uint8_t ua = (uint8_t)a;
                uint8_t ub = (uint8_t)b;
                switch (i8_binary_ops[index]) {
                    case 0x6E:
                        expected[lane] = (uint8_t)(ua + ub);
                        break;
                    case 0x6F:
                        expected[lane] = (uint8_t)wasm__sat_i8((int32_t)a + (int32_t)b);
                        break;
                    case 0x70:
                        expected[lane] = wasm__sat_u8((int32_t)ua + (int32_t)ub);
                        break;
                    case 0x71:
                        expected[lane] = (uint8_t)(ua - ub);
                        break;
                    case 0x72:
                        expected[lane] = (uint8_t)wasm__sat_i8((int32_t)a - (int32_t)b);
                        break;
                    case 0x73:
                        expected[lane] = wasm__sat_u8((int32_t)ua - (int32_t)ub);
                        break;
                    case 0x76:
                        expected[lane] = (uint8_t)((a < b) ? a : b);
                        break;
                    case 0x77:
                        expected[lane] = (ua < ub) ? ua : ub;
                        break;
                    case 0x78:
                        expected[lane] = (uint8_t)((a > b) ? a : b);
                        break;
                    case 0x79:
                        expected[lane] = (ua > ub) ? ua : ub;
                        break;
                    default:
                        expected[lane] = (uint8_t)(((uint32_t)ua + (uint32_t)ub + 1u) >> 1);
                        break;
                }
            }
        }
        wasm_test_expect_u8x16(t, &actual, expected, "i8x16 binary");
    }

    {
        uint8_t expected[16];
        wasm_value_t swizzle_indices =
            wasm_test_v128_from_i8x16((const int8_t[]){ 15, 14, 13, 12, 11, 10, 9, 8,
                                                        7, 6, 5, 4, 3, 2, 1, 0 });
        wasm_value_t actual = wasm__simd_shuffle(&vi8_a, &vi8_b,
                                                 (const uint8_t[]){ 0, 17, 2, 19, 4, 21, 6, 23,
                                                                    8, 25, 10, 27, 12, 29, 14, 31 });
        for (index = 0; index < 8; index++) {
            expected[index * 2u] = (uint8_t)i8_a[index * 2u];
            expected[index * 2u + 1u] = (uint8_t)i8_b[index * 2u + 1u];
        }
        wasm_test_expect_u8x16(t, &actual, expected, "i8x16.shuffle");

        actual = wasm__simd_swizzle(&vi8_a, &swizzle_indices);
        for (index = 0; index < 16; index++) expected[index] = (uint8_t)i8_a[15u - index];
        wasm_test_expect_u8x16(t, &actual, expected, "i8x16.swizzle");
    }

    {
        int16_t expected_i16[8] = { -15, -11, -7, -3, 1, 5, 9, 13 };
        int32_t expected_i32[4] = { -500, -101, 101, 500 };
        wasm_value_t actual_i16 = wasm__simd_extadd_pairwise(0x7C, &vi8_a);
        wasm_value_t actual_i32 = wasm__simd_extadd_pairwise(0x7E, &vi16_a);
        wasm_test_expect_i16x8(t, &actual_i16, expected_i16, "extadd pairwise i16x8");
        wasm_test_expect_i32x4(t, &actual_i32, expected_i32, "extadd pairwise i32x4");
    }

    for (index = 0; index < (uint32_t)(sizeof(i16_binary_ops) / sizeof(i16_binary_ops[0])); index++) {
        wasm_value_t actual;
        uint32_t lane;
        if (i16_binary_ops[index] == 0x82 || i16_binary_ops[index] == 0x8E || i16_binary_ops[index] == 0x8F ||
            i16_binary_ops[index] == 0x90 || i16_binary_ops[index] == 0x91 || i16_binary_ops[index] == 0x92 ||
            i16_binary_ops[index] == 0x93 || i16_binary_ops[index] == 0x95 || i16_binary_ops[index] == 0x96 ||
            i16_binary_ops[index] == 0x97 || i16_binary_ops[index] == 0x98 || i16_binary_ops[index] == 0x99 ||
            i16_binary_ops[index] == 0x9B) {
            int16_t expected[8];
            actual = wasm__simd_i16x8_binary(i16_binary_ops[index], &vi16_a, &vi16_b);
            for (lane = 0; lane < 8; lane++) {
                int16_t a = i16_a[lane];
                int16_t b = i16_b[lane];
                uint16_t ua = (uint16_t)a;
                uint16_t ub = (uint16_t)b;
                switch (i16_binary_ops[index]) {
                    case 0x82: {
                        int32_t product = (int32_t)a * (int32_t)b;
                        expected[lane] = wasm__sat_i16((product + 0x4000) >> 15);
                        break;
                    }
                    case 0x8E:
                        expected[lane] = (int16_t)(ua + ub);
                        break;
                    case 0x8F:
                        expected[lane] = wasm__sat_i16((int32_t)a + (int32_t)b);
                        break;
                    case 0x90:
                        expected[lane] = (int16_t)wasm__sat_u16((int32_t)ua + (int32_t)ub);
                        break;
                    case 0x91:
                        expected[lane] = (int16_t)(ua - ub);
                        break;
                    case 0x92:
                        expected[lane] = wasm__sat_i16((int32_t)a - (int32_t)b);
                        break;
                    case 0x93:
                        expected[lane] = (int16_t)wasm__sat_u16((int32_t)ua - (int32_t)ub);
                        break;
                    case 0x95:
                        expected[lane] = (int16_t)(ua * ub);
                        break;
                    case 0x96:
                        expected[lane] = (a < b) ? a : b;
                        break;
                    case 0x97:
                        expected[lane] = (int16_t)((ua < ub) ? ua : ub);
                        break;
                    case 0x98:
                        expected[lane] = (a > b) ? a : b;
                        break;
                    case 0x99:
                        expected[lane] = (int16_t)((ua > ub) ? ua : ub);
                        break;
                    default:
                        expected[lane] = (int16_t)(((uint32_t)ua + (uint32_t)ub + 1u) >> 1);
                        break;
                }
            }
            wasm_test_expect_i16x8(t, &actual, expected, "i16x8 binary");
        }
    }

    for (index = 0; index < (uint32_t)(sizeof(i32_binary_ops) / sizeof(i32_binary_ops[0])); index++) {
        int32_t expected[4];
        wasm_value_t actual = ((i32_binary_ops[index] == 0xBA) ||
                               (i32_binary_ops[index] >= 0xBC && i32_binary_ops[index] <= 0xBF))
                                  ? wasm__simd_i32x4_binary(i32_binary_ops[index], &vi16_a, &vi16_b)
                                  : wasm__simd_i32x4_binary(i32_binary_ops[index], &vi32_a, &vi32_b);
        uint32_t lane;

        for (lane = 0; lane < 4; lane++) {
            int32_t a = i32_a[lane];
            int32_t b = i32_b[lane];
            uint32_t ua = (uint32_t)a;
            uint32_t ub = (uint32_t)b;
            switch (i32_binary_ops[index]) {
                case 0xAE:
                    expected[lane] = (int32_t)(ua + ub);
                    break;
                case 0xB1:
                    expected[lane] = (int32_t)(ua - ub);
                    break;
                case 0xB5:
                    expected[lane] = (int32_t)(ua * ub);
                    break;
                case 0xB6:
                    expected[lane] = (a < b) ? a : b;
                    break;
                case 0xB7:
                    expected[lane] = (int32_t)((ua < ub) ? ua : ub);
                    break;
                case 0xB8:
                    expected[lane] = (a > b) ? a : b;
                    break;
                case 0xB9:
                    expected[lane] = (int32_t)((ua > ub) ? ua : ub);
                    break;
                case 0xBA: {
                    uint32_t base = lane * 2u;
                    expected[lane] = (int32_t)i16_a[base] * (int32_t)i16_b[base] +
                                     (int32_t)i16_a[base + 1u] * (int32_t)i16_b[base + 1u];
                    break;
                }
                case 0xBC:
                case 0xBD:
                case 0xBE:
                default: {
                    uint32_t base = (i32_binary_ops[index] == 0xBD || i32_binary_ops[index] == 0xBF) ? 4u : 0u;
                    uint32_t src = base + lane;
                    if (i32_binary_ops[index] == 0xBC || i32_binary_ops[index] == 0xBD)
                        expected[lane] = (int32_t)i16_a[src] * (int32_t)i16_b[src];
                    else
                        expected[lane] = (int32_t)((uint32_t)(uint16_t)i16_a[src] * (uint32_t)(uint16_t)i16_b[src]);
                    break;
                }
            }
        }
        wasm_test_expect_i32x4(t, &actual, expected, "i32x4 binary");
    }

    for (index = 0; index < (uint32_t)(sizeof(i64_binary_ops) / sizeof(i64_binary_ops[0])); index++) {
        int64_t expected[2];
        wasm_value_t actual = (i64_binary_ops[index] >= 0xDC)
                                  ? wasm__simd_i64x2_binary(i64_binary_ops[index], &vi32_a, &vi32_b)
                                  : wasm__simd_i64x2_binary(i64_binary_ops[index], &vi64_a, &vi64_b);
        uint32_t lane;

        for (lane = 0; lane < 2; lane++) {
            uint64_t ua = (uint64_t)i64_a[lane];
            uint64_t ub = (uint64_t)i64_b[lane];
            switch (i64_binary_ops[index]) {
                case 0xCE:
                    expected[lane] = (int64_t)(ua + ub);
                    break;
                case 0xD1:
                    expected[lane] = (int64_t)(ua - ub);
                    break;
                case 0xD5:
                    expected[lane] = (int64_t)(ua * ub);
                    break;
                case 0xDC:
                case 0xDD:
                case 0xDE:
                default: {
                    uint32_t base = (i64_binary_ops[index] == 0xDD || i64_binary_ops[index] == 0xDF) ? 2u : 0u;
                    uint32_t src = base + lane;
                    if (i64_binary_ops[index] == 0xDC || i64_binary_ops[index] == 0xDD)
                        expected[lane] = (int64_t)i32_a[src] * (int64_t)i32_b[src];
                    else
                        expected[lane] = (int64_t)((uint64_t)(uint32_t)i32_a[src] * (uint64_t)(uint32_t)i32_b[src]);
                    break;
                }
            }
        }
        wasm_test_expect_i64x2(t, &actual, expected, "i64x2 binary");
    }

    for (index = 0; index < (uint32_t)(sizeof(f32_cmp_ops) / sizeof(f32_cmp_ops[0])); index++) {
        uint8_t expected[16];
        wasm_value_t actual = wasm__simd_cmp_f32x4(f32_cmp_ops[index], &vf32_a, &vf32_b);
        uint32_t lane;
        for (lane = 0; lane < 4; lane++) {
            int cond = 0;
            switch (f32_cmp_ops[index]) {
                case 0x41:
                    cond = f32_a[lane] == f32_b[lane];
                    break;
                case 0x42:
                    cond = f32_a[lane] != f32_b[lane];
                    break;
                case 0x43:
                    cond = f32_a[lane] < f32_b[lane];
                    break;
                case 0x44:
                    cond = f32_a[lane] > f32_b[lane];
                    break;
                case 0x45:
                    cond = f32_a[lane] <= f32_b[lane];
                    break;
                default:
                    cond = f32_a[lane] >= f32_b[lane];
                    break;
            }
            memset(expected + lane * 4u, cond ? 0xFF : 0x00, 4);
        }
        wasm_test_expect_u8x16(t, &actual, expected, "f32x4 cmp");
    }

    for (index = 0; index < (uint32_t)(sizeof(f64_cmp_ops) / sizeof(f64_cmp_ops[0])); index++) {
        int64_t expected[2];
        wasm_value_t actual = wasm__simd_cmp_f64x2(f64_cmp_ops[index], &vf64_a, &vf64_b);
        uint32_t lane;
        for (lane = 0; lane < 2; lane++) {
            int cond = 0;
            switch (f64_cmp_ops[index]) {
                case 0x47:
                    cond = f64_a[lane] == f64_b[lane];
                    break;
                case 0x48:
                    cond = f64_a[lane] != f64_b[lane];
                    break;
                case 0x49:
                    cond = f64_a[lane] < f64_b[lane];
                    break;
                case 0x4A:
                    cond = f64_a[lane] > f64_b[lane];
                    break;
                case 0x4B:
                    cond = f64_a[lane] <= f64_b[lane];
                    break;
                default:
                    cond = f64_a[lane] >= f64_b[lane];
                    break;
            }
            expected[lane] = cond ? -1LL : 0LL;
        }
        wasm_test_expect_i64x2(t, &actual, expected, "f64x2 cmp");
    }

    {
        float expected_f32[4] = { 0.75f, 1.5f, 2.5f, 4.0f };
        double expected_f64[2] = { 1.75, -10.0 };
        wasm_value_t actual_f32 = wasm__simd_f32x4_binary(0xE4, &vf32_a, &vf32_b);
        wasm_value_t actual_f64 = wasm__simd_f64x2_binary(0xF1, &vf64_a, &vf64_b);
        wasm_test_expect_f32x4(t, &actual_f32, expected_f32, "f32x4.add");
        wasm_test_expect_f64x2(t, &actual_f64, expected_f64, "f64x2.sub");
    }

    {
        wasm_value_t trunc_f32_input =
            wasm_test_v128_from_f32x4((const float[]){ 1.9f, -2.1f, 3.0f, -4.9f });
        wasm_value_t trunc_f64_input = wasm_test_v128_from_f64x2((const double[]){ 5.75, -6.25 });
        wasm_value_t trunc_f32 = wasm__simd_trunc_sat_f32x4(0xF8, &trunc_f32_input);
        wasm_value_t convert_f32 = wasm__simd_convert_i32x4(0xFA, &vi32_b);
        wasm_value_t trunc_f64 = wasm__simd_trunc_sat_f64x2_zero(0xFC, &trunc_f64_input);
        wasm_value_t convert_f64 = wasm__simd_convert_low_i32x4_to_f64x2(0xFE, &vi32_a);
        int32_t expected_trunc_f32[4] = { 1, -2, 3, -4 };
        float expected_convert_f32[4] = { 33333.0f, -2000.0f, 2000.0f, -33333.0f };
        int32_t expected_trunc_f64[4] = { 5, -6, 0, 0 };
        double expected_convert_f64[2] = { -100000.0, -1000.0 };

        wasm_test_expect_i32x4(t, &trunc_f32, expected_trunc_f32, "i32x4.trunc_sat_f32x4_s");
        wasm_test_expect_f32x4(t, &convert_f32, expected_convert_f32, "f32x4.convert_i32x4_s");
        wasm_test_expect_i32x4(t, &trunc_f64, expected_trunc_f64, "i32x4.trunc_sat_f64x2_s_zero");
        wasm_test_expect_f64x2(t, &convert_f64, expected_convert_f64, "f64x2.convert_low_i32x4_s");
    }
}

WL_TEST(test_simd_execution_and_feature_gate) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_value_t result;
    static const uint8_t add_lhs[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 250, 251, 252, 253, 254, 255, 9, 10 };
    static const uint8_t add_rhs[16] = { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
    static const uint8_t memory_bytes[16] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 255, 254, 253, 252, 251, 250 };

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7B);
        emit(&sec, 0x7B);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7B);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x7B);
        emit(&sec, 0x7B);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7B);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);
        emit_export_func(&sec, "addv", 0);
        emit_export_func(&sec, "pickv", 1);
        emit_export_func(&sec, "memlane", 2);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 3);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit_simd_op(&body, 0x6E);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x1B);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit_v128_const_bytes(&body, memory_bytes);
        emit_simd_op(&body, 0x0B);
        emit_memarg(&body, 4, 0, 0, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit_v128_const_bytes(&body, memory_bytes);
        emit_simd_op(&body, 0x58);
        emit_memarg(&body, 0, 0, 0, 0);
        emit(&body, 15);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x2D);
        emit_memarg(&body, 0, 0, 0, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit_simd_op(&body, 0x00);
        emit_memarg(&body, 4, 0, 0, 0);
        emit_simd_op(&body, 0x16);
        emit(&body, 0);
        emit(&body, 0x6A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    {
        uint8_t expected[16];
        wasm_value_t args[] = { wasm_v128(add_lhs), wasm_v128(add_rhs) };
        uint32_t lane;

        err = wasm_call(m, "addv", args, 2, &result, 1);
        WASM_CHECK_OK(t, err);
        for (lane = 0; lane < 16; lane++) expected[lane] = (uint8_t)(add_lhs[lane] + add_rhs[lane]);
        if (err == WASM_OK) wasm_test_expect_u8x16(t, &result, expected, "simd addv");
    }

    {
        wasm_value_t args[] = { wasm_v128(add_lhs), wasm_v128(add_rhs), wasm_i32(1) };
        err = wasm_call(m, "pickv", args, 3, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) wasm_test_expect_u8x16(t, &result, add_lhs, "simd select true");

        args[2] = wasm_i32(0);
        err = wasm_call(m, "pickv", args, 3, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) wasm_test_expect_u8x16(t, &result, add_rhs, "simd select false");
    }

    err = wasm_call(m, "memlane", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, (int32_t)memory_bytes[0] + (int32_t)memory_bytes[15]);

    wasm_free_module(m);
    wasm_destroy(&rt);

    wasm_init(&rt);
    wasm_disable_feature(&rt, WASM_FEATURE_SIMD);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected SIMD-disabled runtime to reject v128 module");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", "expected malformed error for disabled SIMD feature");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "SIMD") != NULL, "%s", "expected SIMD feature error message");
    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_memory_grow_respects_zero_max) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    int32_t old_pages;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x01);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    old_pages = wasm_memory_grow_at(m, 0, 0);
    WASM_CHECK_I32(t, old_pages, 0);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size_at(m, 0), 0);

    old_pages = wasm_memory_grow_at(m, 0, 1);
    WASM_CHECK_I32(t, old_pages, -1);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size_at(m, 0), 0);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_trunc_allows_fractional_lower_edges) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 3);
        emit_export_func(&sec, "i32s", 0);
        emit_export_func(&sec, "i32u", 1);
        emit_export_func(&sec, "i64u", 2);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 3);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xAA);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xAB);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xB1);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    {
        wasm_value_t args[] = { wasm_f64(-2147483648.75) };
        err = wasm_call(m, "i32s", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, INT32_MIN);
    }

    {
        wasm_value_t args[] = { wasm_f64(-0.75) };
        err = wasm_call(m, "i32u", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    }

    {
        wasm_value_t args[] = { wasm_f64(-0.75) };
        err = wasm_call(m, "i64u", args, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, 0);
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_call_indirect_uses_structural_type_equality) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 7);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_type_section_parses_composite_types) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x4E);
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x77);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, m->num_types == 4, "expected 4 types, got %u", (unsigned)m->num_types);
    WL_CHECK_MSG(t, m->num_rec_groups == 3, "expected 3 rec groups, got %u", (unsigned)m->num_rec_groups);

    WL_CHECK_MSG(t, m->types[0].kind == WASM_COMP_STRUCT, "type 0 kind = %u", (unsigned)m->types[0].kind);
    WL_CHECK_MSG(t, m->types[1].kind == WASM_COMP_STRUCT, "type 1 kind = %u", (unsigned)m->types[1].kind);
    WL_CHECK_MSG(t, m->types[2].kind == WASM_COMP_ARRAY, "type 2 kind = %u", (unsigned)m->types[2].kind);
    WL_CHECK_MSG(t, m->types[3].kind == WASM_COMP_FUNC, "type 3 kind = %u", (unsigned)m->types[3].kind);

    WL_CHECK_MSG(t, m->rec_groups[0].first_type == 0 && m->rec_groups[0].num_types == 2,
                 "unexpected rec group 0: first=%u count=%u",
                 (unsigned)m->rec_groups[0].first_type,
                 (unsigned)m->rec_groups[0].num_types);
    WL_CHECK_MSG(t, m->rec_groups[1].first_type == 2 && m->rec_groups[1].num_types == 1,
                 "unexpected rec group 1: first=%u count=%u",
                 (unsigned)m->rec_groups[1].first_type,
                 (unsigned)m->rec_groups[1].num_types);
    WL_CHECK_MSG(t, m->rec_groups[2].first_type == 3 && m->rec_groups[2].num_types == 1,
                 "unexpected rec group 2: first=%u count=%u",
                 (unsigned)m->rec_groups[2].first_type,
                 (unsigned)m->rec_groups[2].num_types);

    WL_CHECK_MSG(t, m->types[1].num_supertypes == 1, "expected subtype to record one supertype");
    WL_CHECK_MSG(t, m->types[1].supertypes != NULL && m->types[1].supertypes[0] == 0,
                 "%s", "expected subtype to point at type 0");
    WL_CHECK_MSG(t, m->types[0].is_final == 0, "%s", "expected explicit root type to stay non-final");
    WL_CHECK_MSG(t, m->types[1].is_final == 0, "%s", "sub type should not be final by default");
    WL_CHECK_MSG(t, m->types[0].of.struct_.num_fields == 1, "%s", "struct field count mismatch");
    WL_CHECK_MSG(t, m->types[1].of.struct_.num_fields == 2, "%s", "sub-struct field count mismatch");
    WL_CHECK_MSG(t, m->types[1].of.struct_.fields[1].storage.kind == WASM_STORAGE_PACKED,
                 "%s", "expected packed storage on subtype field");
    WL_CHECK_MSG(t, m->types[1].of.struct_.fields[1].storage.of.packed_type == WASM_PACKED_I8,
                 "%s", "expected i8 packed field");
    WL_CHECK_MSG(t, m->types[1].of.struct_.fields[1].is_mutable == 1,
                 "%s", "expected mutable subtype field");
    WL_CHECK_MSG(t, m->types[2].of.array.field.storage.kind == WASM_STORAGE_PACKED,
                 "%s", "expected packed array storage");
    WL_CHECK_MSG(t, m->types[2].of.array.field.storage.of.packed_type == WASM_PACKED_I16,
                 "%s", "expected i16 packed array element");

    WL_CHECK_MSG(t, wasm_func_param_count(m, 0) == 0, "%s", "function param count mismatch");
    WL_CHECK_MSG(t, wasm_func_result_count(m, 0) == 1, "%s", "function result count mismatch");

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 7);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_type_section_respects_feature_gate) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    wasm_disable_feature(&rt, WASM_FEATURE_GC);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected GC-disabled runtime to reject GC type section");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "garbage collection") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_heaptype_parsing_tracks_concrete_type_refs) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x4E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit(&sec, 0xD0);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x64);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x1A);

        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x1C);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1A);

        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD1);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, m->types[0].of.struct_.fields[0].storage.of.valtype == WASM_TYPE_STRUCTREF,
                 "%s", "expected self field to resolve to structref");
    WL_CHECK_MSG(t, m->types[0].of.struct_.fields[0].storage.ref_type.has_type_index,
                 "%s", "expected self field to preserve concrete type index");
    WL_CHECK_MSG(t, m->types[0].of.struct_.fields[0].storage.ref_type.type_index == 0,
                 "%s", "expected self field to point at type 0");
    WL_CHECK_MSG(t, m->types[0].of.struct_.fields[0].storage.ref_type.nullable,
                 "%s", "expected self field to stay nullable");

    WL_CHECK_MSG(t, m->types[1].of.func.params[0] == WASM_TYPE_STRUCTREF,
                 "%s", "expected concrete param to resolve to structref");
    WL_CHECK_MSG(t, !m->types[1].of.func.param_reftypes[0].nullable,
                 "%s", "expected concrete param to stay non-null");
    WL_CHECK_MSG(t, m->types[1].of.func.param_reftypes[0].has_type_index && m->types[1].of.func.param_reftypes[0].type_index == 0,
                 "%s", "expected concrete param to keep type index 0");
    WL_CHECK_MSG(t, m->types[1].of.func.results[0] == WASM_TYPE_STRUCTREF,
                 "%s", "expected concrete result to resolve to structref");
    WL_CHECK_MSG(t, m->types[1].of.func.result_reftypes[0].nullable,
                 "%s", "expected concrete result to stay nullable");

    WL_CHECK_MSG(t, m->tables[0].reftype == WASM_TYPE_STRUCTREF,
                 "%s", "expected table reftype to resolve to structref");
    WL_CHECK_MSG(t, m->tables[0].reftype_info.has_type_index && m->tables[0].reftype_info.type_index == 0,
                 "%s", "expected table to preserve concrete heap type index");
    WL_CHECK_MSG(t, m->globals[0].type == WASM_TYPE_STRUCTREF,
                 "%s", "expected global reftype to resolve to structref");
    WL_CHECK_MSG(t, m->globals[0].ref_type.has_type_index && m->globals[0].ref_type.type_index == 0,
                 "%s", "expected global to preserve concrete heap type index");
    WL_CHECK_MSG(t, m->funcs[0].num_locals == 1, "%s", "expected one concrete local");
    WL_CHECK_MSG(t, m->funcs[0].locals[0] == WASM_TYPE_STRUCTREF,
                 "%s", "expected concrete local to resolve to structref");
    WL_CHECK_MSG(t, m->funcs[0].local_reftypes[0].has_type_index && m->funcs[0].local_reftypes[0].type_index == 0,
                 "%s", "expected local to preserve concrete heap type index");
    WL_CHECK_MSG(t, !m->funcs[0].local_reftypes[0].nullable,
                 "%s", "expected concrete local to stay non-null");

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_ref_is_null_on_abstract_anyref) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6E);
        emit(&body, 0xD1);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_typed_select_accepts_eqref_as_anyref) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[] = { wasm_i32(1) };
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "pick", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6D);
        emit(&body, 0xD0);
        emit(&body, 0x6E);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1C);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6E);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "pick", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, result.type == WASM_TYPE_EQREF, "%s", "expected subtype operand to survive typed select");
        WL_CHECK_MSG(t, wasm__is_null_ref(&result), "%s", "expected null eqref result");
    }

    args[0] = wasm_i32(0);
    err = wasm_call(m, "pick", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, result.type == WASM_TYPE_ANYREF, "%s", "expected anyref branch to survive typed select");
        WL_CHECK_MSG(t, wasm__is_null_ref(&result), "%s", "expected null anyref result");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_table_set_accepts_eqref_into_anyref_table) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6D);
        emit(&body, 0x26);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x25);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WL_CHECK_MSG(t, result.type == WASM_TYPE_EQREF, "%s", "expected table.get to return the stored eqref value");
        WL_CHECK_MSG(t, wasm__is_null_ref(&result), "%s", "expected table entry 0 to hold a null eqref");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_ref_eq_accepts_eqref_subtypes) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6B);
        emit(&body, 0xD0);
        emit(&body, 0x6D);
        emit(&body, 0xD3);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_function_import_accepts_subtype_signature) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_import_t imp;
    wasm_valtype_t param_types[1] = { WASM_TYPE_ANYREF };
    wasm_valtype_t result_types[1] = { WASM_TYPE_STRUCTREF };
    wasm_reftype_t param_reftypes[1];
    wasm_reftype_t result_reftypes[1];
    wasm_value_t result;
    wasm_error_t err;

    memset(param_reftypes, 0, sizeof(param_reftypes));
    memset(result_reftypes, 0, sizeof(result_reftypes));

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 2);
        emit(&sec, 'i');
        emit(&sec, 'd');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);

        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD1);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    param_reftypes[0].type = WASM_TYPE_ANYREF;
    param_reftypes[0].nullable = 1;
    result_reftypes[0].type = WASM_TYPE_STRUCTREF;
    result_reftypes[0].has_type_index = 1;
    result_reftypes[0].type_index = 1;
    result_reftypes[0].nullable = 1;
    imp.module = "env";
    imp.name = "id";
    imp.type.num_params = 1;
    imp.type.num_results = 1;
    imp.type.params = param_types;
    imp.type.results = result_types;
    imp.type.param_reftypes = param_reftypes;
    imp.type.result_reftypes = result_reftypes;
    imp.func = host_gc_subtype_identity;

    err = wasm_register_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_global_import_accepts_subtype_binding) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_global_import_t imp;
    wasm_value_t imported = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 1);
        emit(&sec, 'g');
        emit(&sec, 0x03);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);

        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x23);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD1);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "g";
    imp.type = WASM_TYPE_STRUCTREF;
    imp.ref_type.type = WASM_TYPE_STRUCTREF;
    imp.ref_type.has_type_index = 1;
    imp.ref_type.type_index = 1;
    imp.ref_type.nullable = 1;
    imp.value = &imported;

    err = wasm_register_global_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_public_type_helpers_and_host_allocators) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t struct_fields[2] = { wasm_i32(9), wasm_i32(255) };
    wasm_value_t array_values[2] = { wasm_i32(10), wasm_i32(20) };
    wasm_value_t struct_ref;
    wasm_value_t array_ref;
    const wasm_gc_header_t* struct_header;
    const wasm_gc_header_t* array_header;
    wasm_value_t loaded;
    uint32_t array_len = 0;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x77);
        emit(&sec, 0x01);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);

        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x01);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);
        emit_export_kind(&sec, "table0", 0x01, 0);
        emit_export_kind(&sec, "global0", 0x03, 0);

        emit_section(&mod, 7, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_I32(t, (int32_t)wasm_type_count(m), 4);
    WL_CHECK_MSG(t, wasm_type_kind(m, 0) == WASM_COMP_STRUCT, "%s", "expected type 0 to be a struct");
    WL_CHECK_MSG(t, wasm_type_kind(m, 1) == WASM_COMP_STRUCT, "%s", "expected type 1 to be a struct");
    WL_CHECK_MSG(t, wasm_type_kind(m, 2) == WASM_COMP_ARRAY, "%s", "expected type 2 to be an array");
    WL_CHECK_MSG(t, wasm_type_kind(m, 3) == WASM_COMP_FUNC, "%s", "expected type 3 to be a function");
    WL_CHECK_MSG(t, wasm_type_kind(m, 99) == WASM_COMP_INVALID, "%s", "expected invalid kind for out-of-range type");
    WL_CHECK_MSG(t, wasm_struct_type_field_count(m, 1) == 2, "%s", "expected subtype field count");
    WL_CHECK_MSG(t, wasm_struct_type_field(m, 1, 1) != NULL, "%s", "expected second struct field");
    WL_CHECK_MSG(t, wasm_struct_type_field(m, 1, 1)->storage.kind == WASM_STORAGE_PACKED,
                 "%s", "expected packed second struct field");
    WL_CHECK_MSG(t, wasm_struct_type_field(m, 1, 1)->is_mutable,
                 "%s", "expected second struct field to be mutable");
    WL_CHECK_MSG(t, wasm_array_type_field(m, 2) != NULL, "%s", "expected array element field");
    WL_CHECK_MSG(t, wasm_array_type_field(m, 2)->storage.kind == WASM_STORAGE_PACKED,
                 "%s", "expected packed array element type");
    WL_CHECK_MSG(t, wasm_type_functype(m, 3) != NULL, "%s", "expected function type accessor");

    WASM_CHECK_I32(t, (int32_t)wasm_table_count(m), 1);
    WL_CHECK_MSG(t, wasm_table_type(m, 0) == WASM_TYPE_STRUCTREF, "%s", "expected structref table");
    WL_CHECK_MSG(t, wasm_table_reftype(m, 0) != NULL && wasm_table_reftype(m, 0)->has_type_index,
                 "%s", "expected concrete table reftype metadata");
    WL_CHECK_MSG(t, wasm_table_reftype(m, 0)->type_index == 0,
                 "%s", "expected table concrete type index 0");

    WASM_CHECK_I32(t, (int32_t)wasm_global_count(m), 1);
    WL_CHECK_MSG(t, wasm_global_type(m, 0) == WASM_TYPE_STRUCTREF, "%s", "expected structref global");
    WL_CHECK_MSG(t, !wasm_global_is_mutable(m, 0), "%s", "expected immutable global");
    WL_CHECK_MSG(t, wasm_global_reftype(m, 0) != NULL && wasm_global_reftype(m, 0)->has_type_index,
                 "%s", "expected concrete global reftype metadata");
    WL_CHECK_MSG(t, wasm_global_reftype(m, 0)->type_index == 1,
                 "%s", "expected global concrete type index 1");

    err = wasm_struct_new(m, 1, struct_fields, 2, &struct_ref);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        struct_header = wasm__runtime_get_gc_object(m, &struct_ref);
        WL_CHECK_MSG(t, struct_header != NULL, "%s", "expected struct allocation to produce a GC object");
        if (struct_header != NULL) {
            WL_CHECK_MSG(t, struct_header->kind == WASM_GC_OBJ_STRUCT, "%s", "expected struct object kind");
            WL_CHECK_MSG(t, struct_header->type_index == 1, "%s", "expected struct object type index 1");
            err = wasm__gc_struct_get_field((const wasm_gc_struct_t*)struct_header,
                                            wasm__module_const_structtype(m, 1),
                                            0,
                                            0,
                                            &loaded);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) WASM_CHECK_I32(t, loaded.of.i32, 9);
        }

        err = wasm_struct_get_field(m, struct_ref, 0, &loaded);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            WL_CHECK_MSG(t, loaded.type == WASM_TYPE_I32, "%s", "expected public struct.get to return i32");
            WASM_CHECK_I32(t, loaded.of.i32, 9);
        }

        err = wasm_struct_set_field(m, struct_ref, 1, wasm_i32(44));
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            err = wasm_struct_get_field(m, struct_ref, 1, &loaded);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) WASM_CHECK_I32(t, loaded.of.i32, 44);
        }

        err = wasm_struct_set_field(m, struct_ref, 0, wasm_i32(12));
        WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected immutable struct field rejection");

        err = wasm_struct_set_field(m, struct_ref, 1, wasm_i64(12));
        WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected struct field type mismatch");

        err = wasm_struct_get_field(m, wasm_ref_null(WASM_TYPE_NONE), 0, &loaded);
        WL_CHECK_MSG(t, err == WASM_ERR_TRAP, "%s", "expected null struct ref to trap");
    }

    err = wasm_array_new(m, 2, array_values, 2, &array_ref);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        array_header = wasm__runtime_get_gc_object(m, &array_ref);
        WL_CHECK_MSG(t, array_header != NULL, "%s", "expected array allocation to produce a GC object");
        if (array_header != NULL) {
            WL_CHECK_MSG(t, array_header->kind == WASM_GC_OBJ_ARRAY, "%s", "expected array object kind");
            WL_CHECK_MSG(t, array_header->type_index == 2, "%s", "expected array object type index 2");
            err = wasm__gc_array_get((const wasm_gc_array_t*)array_header,
                                     wasm__module_const_arraytype(m, 2),
                                     1,
                                     0,
                                     &loaded);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) WASM_CHECK_I32(t, loaded.of.i32, 20);
        }

        err = wasm_array_length(m, array_ref, &array_len);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) WASM_CHECK_I32(t, (int32_t)array_len, 2);

        err = wasm_array_get_elem(m, array_ref, 1, &loaded);
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            WL_CHECK_MSG(t, loaded.type == WASM_TYPE_I32, "%s", "expected public array.get to return i32");
            WASM_CHECK_I32(t, loaded.of.i32, 20);
        }

        err = wasm_array_set_elem(m, array_ref, 0, wasm_i32(4660));
        WASM_CHECK_OK(t, err);
        if (err == WASM_OK) {
            err = wasm_array_get_elem(m, array_ref, 0, &loaded);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) WASM_CHECK_I32(t, loaded.of.i32, 4660);
        }

        err = wasm_array_set_elem(m, array_ref, 0, wasm_i64(1));
        WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected array element type mismatch");

        err = wasm_array_get_elem(m, array_ref, 9, &loaded);
        WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_BOUNDS, "%s", "expected array element bounds rejection");

        err = wasm_array_length(m, struct_ref, &array_len);
        WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected wrong-kind array length rejection");

        err = wasm_struct_get_field(m, array_ref, 0, &loaded);
        WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected wrong-kind struct access rejection");

        err = wasm_array_length(m, wasm_i31ref(7), &array_len);
        WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected i31 array length rejection");
    }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_runtime_initializes_heap_arena) {
    wasm_runtime_t rt;

    wasm_init(&rt);

    WL_CHECK_MSG(t, rt.gc_heap != NULL, "%s", "expected wasm_init to allocate a GC heap arena");
    WL_CHECK_MSG(t, rt.gc_heap_size == (size_t)WASM_GC_HEAP_SIZE,
                 "expected GC heap size %zu, got %zu",
                 (size_t)WASM_GC_HEAP_SIZE,
                 rt.gc_heap_size);
    WL_CHECK_MSG(t, rt.gc_heap_offset == 0, "%s", "expected empty GC heap to start at offset 0");
    WL_CHECK_MSG(t, rt.gc_allocations == NULL, "%s", "expected no GC objects to be tracked initially");

    wasm_destroy(&rt);

    WL_CHECK_MSG(t, rt.gc_heap == NULL, "%s", "expected wasm_destroy to release the GC heap arena");
    WL_CHECK_MSG(t, rt.gc_heap_size == 0, "%s", "expected wasm_destroy to clear GC heap size");
    WL_CHECK_MSG(t, rt.gc_heap_offset == 0, "%s", "expected wasm_destroy to clear GC heap offset");
    WL_CHECK_MSG(t, rt.gc_allocations == NULL, "%s", "expected wasm_destroy to clear tracked GC objects");
}

WL_TEST(test_gc_i31ref_uses_tagged_non_null_storage) {
    wasm_value_t null_i31 = wasm_ref_null(WASM_TYPE_I31REF);
    wasm_value_t minus_one = wasm_i31ref(-1);
    wasm_value_t positive = wasm_i31ref(123456789);

    WL_CHECK_MSG(t, null_i31.type == WASM_TYPE_I31REF, "%s", "expected null i31ref to preserve its value type");
    WL_CHECK_MSG(t, wasm__is_null_ref(&null_i31), "%s", "expected ref.null i31 to stay null");

    WL_CHECK_MSG(t, !wasm__is_null_ref(&minus_one), "%s", "expected tagged i31ref payloads to be non-null");
    WL_CHECK_MSG(t, wasm_test_value_is_i31ref(&minus_one), "%s", "expected wasm_i31ref to set the i31 tag bit");
    WL_CHECK_MSG(t, minus_one.of.gc_ref == wasm__gc_i31_encode(-1), "%s", "expected wasm_i31ref to use the internal tagged encoding");
    WASM_CHECK_I32(t, wasm_test_gc_i31_get_s(minus_one.of.gc_ref), -1);
    WL_CHECK_MSG(t, wasm_test_gc_i31_get_u(minus_one.of.gc_ref) == 0x7FFFFFFFu,
                 "expected unsigned i31 decode to keep the low 31 bits, got %u",
                 (unsigned)wasm_test_gc_i31_get_u(minus_one.of.gc_ref));
    WASM_CHECK_I32(t, wasm_test_gc_i31_get_s(positive.of.gc_ref), 123456789);
}

WL_TEST(test_gc_allocator_tracks_struct_and_array_objects) {
    wasm_runtime_t rt;
    wasm_fieldtype_t struct_fields[2];
    wasm_structtype_t struct_type;
    wasm_arraytype_t array_type;
    wasm_gc_struct_t* struct_obj;
    wasm_gc_array_t* array_obj;
    size_t struct_size = 0;
    size_t array_size = 0;
    size_t struct_field0_offset;
    size_t struct_field1_offset;
    size_t first_heap_offset;

    memset(struct_fields, 0, sizeof(struct_fields));
    memset(&struct_type, 0, sizeof(struct_type));
    memset(&array_type, 0, sizeof(array_type));

    wasm_init(&rt);

    struct_fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    struct_fields[0].storage.of.valtype = WASM_TYPE_I32;
    struct_fields[1].storage.kind = WASM_STORAGE_PACKED;
    struct_fields[1].storage.of.packed_type = WASM_PACKED_I8;
    struct_fields[1].is_mutable = 1;
    struct_type.num_fields = 2;
    struct_type.fields = struct_fields;

    array_type.field.storage.kind = WASM_STORAGE_PACKED;
    array_type.field.storage.of.packed_type = WASM_PACKED_I16;

    WL_CHECK_MSG(t, wasm__gc_struct_size(&struct_type, &struct_size), "%s", "expected struct object size calculation to succeed");
    WL_CHECK_MSG(t, wasm__gc_array_total_size(&array_type, 4, &array_size), "%s", "expected array object size calculation to succeed");

    struct_field0_offset = wasm__gc_struct_field_offset(&struct_type, 0);
    struct_field1_offset = wasm__gc_struct_field_offset(&struct_type, 1);
    WL_CHECK_MSG(t, struct_field0_offset == wasm__align_up_size(WASM__GC_STRUCT_HEADER_SIZE, (size_t)WASM__ALIGNOF(wasm_value_t)),
                 "unexpected first struct field offset %zu",
                 struct_field0_offset);
    WL_CHECK_MSG(t, struct_field1_offset > struct_field0_offset,
                 "expected later packed fields to follow earlier fields (%zu <= %zu)",
                 struct_field1_offset,
                 struct_field0_offset);

    struct_obj = wasm__gc_alloc_struct_object(&rt, NULL, 7, &struct_type);
    WL_CHECK_MSG(t, struct_obj != NULL, "%s", "expected struct allocation from the GC heap to succeed");
    if (struct_obj == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, struct_obj->header.type_index == 7, "%s", "expected struct object header to store its runtime type index");
    WL_CHECK_MSG(t, struct_obj->header.kind == WASM_GC_OBJ_STRUCT, "%s", "expected struct allocation kind tag");
    WL_CHECK_MSG(t, struct_obj->header.object_size == struct_size,
                 "expected struct object size %zu, got %zu",
                 struct_size,
                 struct_obj->header.object_size);
    WL_CHECK_MSG(t, struct_obj->header.next_alloc == NULL, "%s", "expected first GC allocation to terminate the allocation list");
    WL_CHECK_MSG(t, rt.gc_allocations == &struct_obj->header, "%s", "expected runtime to track the struct allocation");

    first_heap_offset = rt.gc_heap_offset;

    array_obj = wasm__gc_alloc_array_object(&rt, NULL, 11, &array_type, 4);
    WL_CHECK_MSG(t, array_obj != NULL, "%s", "expected array allocation from the GC heap to succeed");
    if (array_obj == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, array_obj->length == 4, "%s", "expected array header to store its logical length");
    WL_CHECK_MSG(t, array_obj->header.type_index == 11, "%s", "expected array object header to store its runtime type index");
    WL_CHECK_MSG(t, array_obj->header.kind == WASM_GC_OBJ_ARRAY, "%s", "expected array allocation kind tag");
    WL_CHECK_MSG(t, array_obj->header.object_size == array_size,
                 "expected array object size %zu, got %zu",
                 array_size,
                 array_obj->header.object_size);
    WL_CHECK_MSG(t, rt.gc_allocations == &array_obj->header, "%s", "expected latest allocation to be the head of the GC allocation list");
    WL_CHECK_MSG(t, array_obj->header.next_alloc == &struct_obj->header,
                 "%s",
                 "expected GC allocation list to chain older objects behind newer ones");
    WL_CHECK_MSG(t, rt.gc_heap_offset > first_heap_offset,
                 "expected second allocation to advance the heap offset (%zu -> %zu)",
                 first_heap_offset,
                 rt.gc_heap_offset);

    wasm_destroy(&rt);
}

WL_TEST(test_gc_const_expr_supports_gc_alloc_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_gc_struct_t* struct_init;
    wasm_gc_struct_t* struct_default;
    wasm_gc_array_t* array_init;
    wasm_gc_array_t* array_default;
    wasm_gc_array_t* array_fixed;
    wasm_value_t* struct_init_field;
    wasm_value_t* struct_default_field;
    wasm_value_t* array_init_data;
    wasm_value_t* array_default_data;
    wasm_value_t* array_fixed_data;
    size_t struct_field_offset;
    size_t array_data_offset;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x5E);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 6);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 7);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x01);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 3);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 9);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x06);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x0B);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 4);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x07);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x0B);

        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 1);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 2);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x08);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x0B);

        emit(&sec, 0x6C);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, -1);
        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x1C);
        emit(&sec, 0x0B);

        emit_section(&mod, 6, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, m->num_globals == 6, "expected 6 globals, got %u", (unsigned)m->num_globals);

    struct_init = (wasm_gc_struct_t*)m->globals[0].value.of.gc_obj;
    struct_default = (wasm_gc_struct_t*)m->globals[1].value.of.gc_obj;
    array_init = (wasm_gc_array_t*)m->globals[2].value.of.gc_obj;
    array_default = (wasm_gc_array_t*)m->globals[3].value.of.gc_obj;
    array_fixed = (wasm_gc_array_t*)m->globals[4].value.of.gc_obj;

    WL_CHECK_MSG(t, m->globals[0].type == WASM_TYPE_STRUCTREF, "%s", "expected struct.new global to resolve to structref");
    WL_CHECK_MSG(t, m->globals[1].type == WASM_TYPE_STRUCTREF, "%s", "expected struct.new_default global to resolve to structref");
    WL_CHECK_MSG(t, m->globals[2].type == WASM_TYPE_ARRAYREF, "%s", "expected array.new global to resolve to arrayref");
    WL_CHECK_MSG(t, m->globals[3].type == WASM_TYPE_ARRAYREF, "%s", "expected array.new_default global to resolve to arrayref");
    WL_CHECK_MSG(t, m->globals[4].type == WASM_TYPE_ARRAYREF, "%s", "expected array.new_fixed global to resolve to arrayref");
    WL_CHECK_MSG(t, m->globals[5].type == WASM_TYPE_I31REF, "%s", "expected ref.i31 global to resolve to i31ref");

    WL_CHECK_MSG(t, !wasm__is_null_ref(&m->globals[0].value), "%s", "expected struct.new const expr to allocate an object");
    WL_CHECK_MSG(t, !wasm__is_null_ref(&m->globals[1].value), "%s", "expected struct.new_default const expr to allocate an object");
    WL_CHECK_MSG(t, !wasm__is_null_ref(&m->globals[2].value), "%s", "expected array.new const expr to allocate an object");
    WL_CHECK_MSG(t, !wasm__is_null_ref(&m->globals[3].value), "%s", "expected array.new_default const expr to allocate an object");
    WL_CHECK_MSG(t, !wasm__is_null_ref(&m->globals[4].value), "%s", "expected array.new_fixed const expr to allocate an object");
    WL_CHECK_MSG(t, !wasm__is_null_ref(&m->globals[5].value), "%s", "expected ref.i31 const expr to produce a non-null tagged ref");

    WL_CHECK_MSG(t, struct_init != NULL && struct_init->header.kind == WASM_GC_OBJ_STRUCT,
                 "%s", "expected struct.new global to point at a struct object");
    WL_CHECK_MSG(t, struct_default != NULL && struct_default->header.kind == WASM_GC_OBJ_STRUCT,
                 "%s", "expected struct.new_default global to point at a struct object");
    WL_CHECK_MSG(t, array_init != NULL && array_init->header.kind == WASM_GC_OBJ_ARRAY,
                 "%s", "expected array.new global to point at an array object");
    WL_CHECK_MSG(t, array_default != NULL && array_default->header.kind == WASM_GC_OBJ_ARRAY,
                 "%s", "expected array.new_default global to point at an array object");
    WL_CHECK_MSG(t, array_fixed != NULL && array_fixed->header.kind == WASM_GC_OBJ_ARRAY,
                 "%s", "expected array.new_fixed global to point at an array object");

    struct_field_offset = wasm__gc_struct_field_offset(&m->types[0].of.struct_, 0);
    array_data_offset = wasm__gc_array_data_offset(&m->types[1].of.array);
    struct_init_field = (wasm_value_t*)((uint8_t*)struct_init + struct_field_offset);
    struct_default_field = (wasm_value_t*)((uint8_t*)struct_default + struct_field_offset);
    array_init_data = (wasm_value_t*)((uint8_t*)array_init + array_data_offset);
    array_default_data = (wasm_value_t*)((uint8_t*)array_default + array_data_offset);
    array_fixed_data = (wasm_value_t*)((uint8_t*)array_fixed + array_data_offset);

    WL_CHECK_MSG(t, struct_init_field->type == WASM_TYPE_I32 && struct_init_field->of.i32 == 7,
                 "%s", "expected struct.new const expr to preserve field values");
    WL_CHECK_MSG(t, struct_default_field->type == WASM_TYPE_I32 && struct_default_field->of.i32 == 0,
                 "%s", "expected struct.new_default const expr to zero-initialize fields");

    WL_CHECK_MSG(t, array_init->length == 3, "expected array.new length 3, got %u", (unsigned)array_init->length);
    WL_CHECK_MSG(t, array_default->length == 4, "expected array.new_default length 4, got %u", (unsigned)array_default->length);
    WL_CHECK_MSG(t, array_fixed->length == 2, "expected array.new_fixed length 2, got %u", (unsigned)array_fixed->length);
    WL_CHECK_MSG(t, array_init_data[0].of.i32 == 9 && array_init_data[1].of.i32 == 9 && array_init_data[2].of.i32 == 9,
                 "%s", "expected array.new const expr to fill with the initializer value");
    WL_CHECK_MSG(t, array_default_data[0].of.i32 == 0 && array_default_data[3].of.i32 == 0,
                 "%s", "expected array.new_default const expr to zero-initialize elements");
    WL_CHECK_MSG(t, array_fixed_data[0].of.i32 == 1 && array_fixed_data[1].of.i32 == 2,
                 "%s", "expected array.new_fixed const expr to preserve element order");

    WL_CHECK_MSG(t, wasm_test_value_is_i31ref(&m->globals[5].value), "%s", "expected ref.i31 const expr to use the tagged i31 representation");
    WASM_CHECK_I32(t, wasm_test_gc_i31_get_s(m->globals[5].value.of.gc_ref), -1);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_accessors_handle_packed_fields_and_array_bounds) {
    wasm_runtime_t rt;
    wasm_module_t mod;
    wasm_comptype_t types[2];
    wasm_fieldtype_t struct_fields[3];
    wasm_gc_struct_t* struct_obj;
    wasm_gc_array_t* array_obj;
    wasm_value_t value;

    memset(&mod, 0, sizeof(mod));
    memset(types, 0, sizeof(types));
    memset(struct_fields, 0, sizeof(struct_fields));

    wasm_init(&rt);

    struct_fields[0].storage.kind = WASM_STORAGE_PACKED;
    struct_fields[0].storage.of.packed_type = WASM_PACKED_I8;
    struct_fields[0].is_mutable = 1;

    struct_fields[1].storage.kind = WASM_STORAGE_PACKED;
    struct_fields[1].storage.of.packed_type = WASM_PACKED_I16;
    struct_fields[1].is_mutable = 1;

    struct_fields[2].storage.kind = WASM_STORAGE_VALTYPE;
    struct_fields[2].storage.of.valtype = WASM_TYPE_I32;
    struct_fields[2].is_mutable = 1;

    types[0].kind = WASM_COMP_STRUCT;
    types[0].of.struct_.num_fields = 3;
    types[0].of.struct_.fields = struct_fields;

    types[1].kind = WASM_COMP_ARRAY;
    types[1].of.array.field.storage.kind = WASM_STORAGE_PACKED;
    types[1].of.array.field.storage.of.packed_type = WASM_PACKED_I8;
    types[1].of.array.field.is_mutable = 1;

    mod.rt = &rt;
    mod.types = types;
    mod.num_types = 2;

    struct_obj = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    array_obj = wasm__gc_alloc_array_object(&rt, &mod, 1, &types[1].of.array, 2);

    WL_CHECK_MSG(t, struct_obj != NULL, "%s", "expected struct allocation for accessor test to succeed");
    WL_CHECK_MSG(t, array_obj != NULL, "%s", "expected array allocation for accessor test to succeed");
    if (!struct_obj || !array_obj) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, wasm__gc_struct_set_field(struct_obj, &types[0].of.struct_, 0, wasm_i32(0xFF)) == WASM_OK,
                 "%s",
                 "expected packed i8 struct store to succeed");
    WL_CHECK_MSG(t, wasm__gc_struct_set_field(struct_obj, &types[0].of.struct_, 1, wasm_i32(0xFF80)) == WASM_OK,
                 "%s",
                 "expected packed i16 struct store to succeed");
    WL_CHECK_MSG(t, wasm__gc_struct_set_field(struct_obj, &types[0].of.struct_, 2, wasm_i32(77)) == WASM_OK,
                 "%s",
                 "expected unpacked struct store to succeed");

    WL_CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 0, 1, &value) == WASM_OK,
                 "%s",
                 "expected packed i8 signed load to succeed");
    WASM_CHECK_I32(t, value.of.i32, -1);
    WL_CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 0, 0, &value) == WASM_OK,
                 "%s",
                 "expected packed i8 unsigned load to succeed");
    WASM_CHECK_I32(t, value.of.i32, 255);

    WL_CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 1, 1, &value) == WASM_OK,
                 "%s",
                 "expected packed i16 signed load to succeed");
    WASM_CHECK_I32(t, value.of.i32, -128);
    WL_CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 1, 0, &value) == WASM_OK,
                 "%s",
                 "expected packed i16 unsigned load to succeed");
    WASM_CHECK_I32(t, value.of.i32, 65408);

    WL_CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 2, 0, &value) == WASM_OK,
                 "%s",
                 "expected unpacked struct load to succeed");
    WL_CHECK_MSG(t, value.type == WASM_TYPE_I32 && value.of.i32 == 77,
                 "%s",
                 "expected unpacked struct accessor to preserve the stored value");

    WL_CHECK_MSG(t, wasm__gc_struct_get_field(NULL, &types[0].of.struct_, 0, 0, &value) == WASM_ERR_TRAP,
                 "%s",
                 "expected null struct access to trap");
    WL_CHECK_MSG(t, wasm__gc_struct_get_field(struct_obj, &types[0].of.struct_, 3, 0, &value) == WASM_ERR_MALFORMED,
                 "%s",
                 "expected invalid struct field access to be rejected");

    WL_CHECK_MSG(t, wasm__gc_array_set(array_obj, &types[1].of.array, 0, wasm_i32(0x7F)) == WASM_OK,
                 "%s",
                 "expected first packed array store to succeed");
    WL_CHECK_MSG(t, wasm__gc_array_set(array_obj, &types[1].of.array, 1, wasm_i32(0x80)) == WASM_OK,
                 "%s",
                 "expected second packed array store to succeed");
    WL_CHECK_MSG(t, wasm__gc_array_len(array_obj) == 2,
                 "expected array accessor length 2, got %u",
                 (unsigned)wasm__gc_array_len(array_obj));

    WL_CHECK_MSG(t, wasm__gc_array_get(array_obj, &types[1].of.array, 0, 1, &value) == WASM_OK,
                 "%s",
                 "expected first packed array load to succeed");
    WASM_CHECK_I32(t, value.of.i32, 127);
    WL_CHECK_MSG(t, wasm__gc_array_get(array_obj, &types[1].of.array, 1, 1, &value) == WASM_OK,
                 "%s",
                 "expected signed packed array load to succeed");
    WASM_CHECK_I32(t, value.of.i32, -128);
    WL_CHECK_MSG(t, wasm__gc_array_get(array_obj, &types[1].of.array, 1, 0, &value) == WASM_OK,
                 "%s",
                 "expected unsigned packed array load to succeed");
    WASM_CHECK_I32(t, value.of.i32, 128);

    WL_CHECK_MSG(t, wasm__gc_array_get(array_obj, &types[1].of.array, 2, 0, &value) == WASM_ERR_OUT_OF_BOUNDS,
                 "%s",
                 "expected out-of-bounds array access to trap with bounds error");
    WL_CHECK_MSG(t, wasm__gc_array_set(NULL, &types[1].of.array, 0, wasm_i32(0)) == WASM_ERR_TRAP,
                 "%s",
                 "expected null array store to trap");

    wasm_destroy(&rt);
}

WL_TEST(test_gc_collect_marks_runtime_roots_and_preserves_object_graphs) {
    wasm_runtime_t rt;
    wasm_module_t mod;
    wasm_comptype_t types[2];
    wasm_fieldtype_t child_fields[1];
    wasm_fieldtype_t parent_fields[1];
    wasm_global_t globals[1];
    wasm_table_t tables[1];
    wasm_value_t table_elems[1];
    wasm_func_t fake_func;
    wasm_value_t frame_locals[1];
    wasm_gc_struct_t* child;
    wasm_gc_struct_t* parent;
    wasm_gc_struct_t* table_root;
    wasm_gc_struct_t* local_root;
    wasm_gc_struct_t* stack_root;
    wasm_gc_struct_t* garbage;
    wasm_gc_struct_t* moved_parent;
    wasm_gc_struct_t* moved_child;
    wasm_value_t* parent_field;
    wasm_value_t* child_field;
    wasm_value_t* table_field;
    wasm_value_t* local_field;
    wasm_value_t* stack_field;

    memset(&mod, 0, sizeof(mod));
    memset(types, 0, sizeof(types));
    memset(child_fields, 0, sizeof(child_fields));
    memset(parent_fields, 0, sizeof(parent_fields));
    memset(globals, 0, sizeof(globals));
    memset(tables, 0, sizeof(tables));
    memset(table_elems, 0, sizeof(table_elems));
    memset(&fake_func, 0, sizeof(fake_func));
    memset(frame_locals, 0, sizeof(frame_locals));

    wasm_init(&rt);

    child_fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    child_fields[0].storage.of.valtype = WASM_TYPE_I32;
    types[0].kind = WASM_COMP_STRUCT;
    types[0].of.struct_.num_fields = 1;
    types[0].of.struct_.fields = child_fields;

    parent_fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    parent_fields[0].storage.of.valtype = WASM_TYPE_STRUCTREF;
    types[1].kind = WASM_COMP_STRUCT;
    types[1].of.struct_.num_fields = 1;
    types[1].of.struct_.fields = parent_fields;

    mod.rt = &rt;
    mod.types = types;
    mod.num_types = 2;
    mod.globals = globals;
    mod.num_globals = 1;
    mod.tables = tables;
    mod.num_tables = 1;
    tables[0].elems = table_elems;
    tables[0].size = 1;
    tables[0].reftype = WASM_TYPE_STRUCTREF;
    wasm__gc_register_module(&rt, &mod);

    fake_func.num_locals = 1;
    rt.call_frame_sp = 1;
    rt.call_frames[0].func = &fake_func;
    rt.call_frames[0].locals = frame_locals;

    child = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    parent = wasm__gc_alloc_struct_object(&rt, &mod, 1, &types[1].of.struct_);
    table_root = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    local_root = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    stack_root = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    garbage = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);

    WL_CHECK_MSG(t, child && parent && table_root && local_root && stack_root && garbage,
                 "%s",
                 "expected all GC test allocations to succeed");
    if (!(child && parent && table_root && local_root && stack_root && garbage)) {
        wasm__gc_unregister_module(&rt, &mod);
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, wasm__gc_struct_store_field(child, &types[0].of.struct_, 0, wasm_i32(123)) == WASM_OK,
                 "%s",
                 "expected child field store to succeed");
    WL_CHECK_MSG(t, wasm__gc_struct_store_field(parent, &types[1].of.struct_, 0, wasm__gc_type_ref_value(&mod, 0, &child->header)) == WASM_OK,
                 "%s",
                 "expected parent field store to succeed");
    WL_CHECK_MSG(t, wasm__gc_struct_store_field(table_root, &types[0].of.struct_, 0, wasm_i32(77)) == WASM_OK,
                 "%s",
                 "expected table-root field store to succeed");
    WL_CHECK_MSG(t, wasm__gc_struct_store_field(local_root, &types[0].of.struct_, 0, wasm_i32(99)) == WASM_OK,
                 "%s",
                 "expected local-root field store to succeed");
    WL_CHECK_MSG(t, wasm__gc_struct_store_field(stack_root, &types[0].of.struct_, 0, wasm_i32(55)) == WASM_OK,
                 "%s",
                 "expected stack-root field store to succeed");
    WL_CHECK_MSG(t, wasm__gc_struct_store_field(garbage, &types[0].of.struct_, 0, wasm_i32(-1)) == WASM_OK,
                 "%s",
                 "expected garbage field store to succeed");

    globals[0].type = WASM_TYPE_STRUCTREF;
    globals[0].value = wasm__gc_type_ref_value(&mod, 1, &parent->header);
    table_elems[0] = wasm__gc_type_ref_value(&mod, 0, &table_root->header);
    frame_locals[0] = wasm__gc_type_ref_value(&mod, 0, &local_root->header);
    rt.stack[rt.sp++] = wasm__gc_type_ref_value(&mod, 0, &stack_root->header);

    wasm_gc_collect(&rt);

    WL_CHECK_MSG(t, wasm_test_gc_allocation_count(&rt) == 5,
                 "expected 5 live GC objects after collection, got %u",
                 (unsigned)wasm_test_gc_allocation_count(&rt));

    moved_parent = (wasm_gc_struct_t*)globals[0].value.of.gc_obj;
    parent_field = wasm_test_gc_struct_field_value(moved_parent, &types[1].of.struct_, 0);
    moved_child = parent_field ? (wasm_gc_struct_t*)parent_field->of.gc_obj : NULL;
    child_field = wasm_test_gc_struct_field_value(moved_child, &types[0].of.struct_, 0);
    table_field = wasm_test_gc_struct_field_value((wasm_gc_struct_t*)table_elems[0].of.gc_obj,
                                                  &types[0].of.struct_,
                                                  0);
    local_field = wasm_test_gc_struct_field_value((wasm_gc_struct_t*)frame_locals[0].of.gc_obj,
                                                  &types[0].of.struct_,
                                                  0);
    stack_field = wasm_test_gc_struct_field_value((wasm_gc_struct_t*)rt.stack[0].of.gc_obj,
                                                  &types[0].of.struct_,
                                                  0);

    WL_CHECK_MSG(t, moved_parent != NULL && moved_parent != parent,
                 "%s",
                 "expected collection to rewrite the rooted parent reference");
    WL_CHECK_MSG(t, moved_child != NULL && moved_child != child,
                 "%s",
                 "expected collection to rewrite nested child references");
    WL_CHECK_MSG(t, child_field != NULL && child_field->of.i32 == 123,
                 "%s",
                 "expected child object contents to survive collection");
    WL_CHECK_MSG(t, table_field != NULL && table_field->of.i32 == 77,
                 "%s",
                 "expected table roots to survive collection");
    WL_CHECK_MSG(t, local_field != NULL && local_field->of.i32 == 99,
                 "%s",
                 "expected call-frame local roots to survive collection");
    WL_CHECK_MSG(t, stack_field != NULL && stack_field->of.i32 == 55,
                 "%s",
                 "expected operand-stack roots to survive collection");

    wasm__gc_unregister_module(&rt, &mod);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_allocator_retries_after_collection_on_heap_exhaustion) {
    wasm_runtime_t rt;
    wasm_module_t mod;
    wasm_comptype_t types[1];
    wasm_fieldtype_t fields[1];
    wasm_gc_struct_t* root;
    wasm_gc_struct_t* garbage;
    wasm_gc_struct_t* fresh;
    size_t object_size = 0;
    size_t heap_size;

    memset(&mod, 0, sizeof(mod));
    memset(types, 0, sizeof(types));
    memset(fields, 0, sizeof(fields));

    wasm_init(&rt);

    fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    fields[0].storage.of.valtype = WASM_TYPE_I32;
    types[0].kind = WASM_COMP_STRUCT;
    types[0].of.struct_.num_fields = 1;
    types[0].of.struct_.fields = fields;

    mod.rt = &rt;
    mod.types = types;
    mod.num_types = 1;
    wasm__gc_register_module(&rt, &mod);

    WL_CHECK_MSG(t, wasm__gc_struct_size(&types[0].of.struct_, &object_size),
                 "%s",
                 "expected struct size calculation to succeed");
    heap_size = object_size * 2u + (object_size / 2u);
    WL_CHECK_MSG(t, wasm__gc_heap_init(&rt, heap_size) == WASM_OK,
                 "%s",
                 "expected GC heap resize for exhaustion test to succeed");

    root = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    garbage = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);
    WL_CHECK_MSG(t, root != NULL && garbage != NULL,
                 "%s",
                 "expected initial allocations in the resized heap to succeed");
    if (!(root && garbage)) {
        wasm__gc_unregister_module(&rt, &mod);
        wasm_destroy(&rt);
        return;
    }

    rt.stack[rt.sp++] = wasm__gc_type_ref_value(&mod, 0, &root->header);
    fresh = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);

    WL_CHECK_MSG(t, fresh != NULL,
                 "%s",
                 "expected GC allocation to retry after collecting unreachable objects");
    WL_CHECK_MSG(t, wasm_test_gc_allocation_count(&rt) == 2,
                 "expected only the rooted object and the fresh allocation to remain, got %u",
                 (unsigned)wasm_test_gc_allocation_count(&rt));
    WL_CHECK_MSG(t, rt.stack[0].of.gc_obj != NULL && rt.stack[0].of.gc_obj != &root->header,
                 "%s",
                 "expected collection to rewrite the rooted reference during retry");

    wasm__gc_unregister_module(&rt, &mod);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_runtime_executes_struct_array_i31_and_conversion_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 3);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 10);
        emit(&body, 0x41);
        emit_leb128_i32(&body, -1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x00);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x02);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x03);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x04);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 20);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x05);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x08);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0F);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0E);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x10);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 4);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x06);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 2);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x07);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 3);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 3);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, -5);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1C);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1D);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1C);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1E);
        emit(&body, 0x6A);

        emit(&body, 0xD0);
        emit(&body, 0x6F);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1A);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x1B);
        emit(&body, 0xD1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD3);
        emit(&body, 0x6A);

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 286);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_runtime_executes_cast_and_branch_cast_opcodes) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x01);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x14);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x16);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD3);
        emit(&body, 0x6A);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x18);
        emit(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x14);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x19);
        emit(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x0B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x14);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0xD0);
        emit(&body, 0x6B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x15);
        emit(&body, 0x6B);
        emit(&body, 0x6A);

        emit(&body, 0xD0);
        emit(&body, 0x6B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x17);
        emit(&body, 0x6B);
        emit(&body, 0xD1);
        emit(&body, 0x6A);

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 6);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_runtime_executes_segment_backed_array_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x6B);
        emit(&sec, 0x01);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 0x80, 0x7F, 0x05 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 12, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x6B);
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0xD0);
        emit(&sec, 0x6B);
        emit(&sec, 0x0B);

        emit(&sec, 0xFB);
        emit_leb128_u32(&sec, 0x01);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);

        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 3);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 2);

        emit(&body, 0x41);
        emit_leb128_i32(&body, -1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x00);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x04);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x09);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0C);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x11);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x12);
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0D);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6A);

        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0A);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x21);
        emit_leb128_u32(&body, 2);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0F);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 2);
        emit(&body, 0xD1);
        emit(&body, 0x6A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x13);
        emit_leb128_u32(&body, 2);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 2);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x0B);
        emit_leb128_u32(&body, 2);
        emit(&body, 0xD1);
        emit(&body, 0x6A);

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 390);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_validation_accepts_struct_and_array_gc_opcodes) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x5E);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x00);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1A);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x06);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x1A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_validation_rejects_struct_get_on_packed_field_without_sign) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x78);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x02);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected unpacked struct.get validation to reject packed fields");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "unpacked field") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_validation_accepts_cast_and_branch_cast_opcodes) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_i32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x14);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x1A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x17);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x1A);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x18);
        emit(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit_leb128_i32(&body, 0);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x1A);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x19);
        emit(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit_leb128_i32(&body, 0);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x1A);

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_validation_rejects_invalid_ref_cast_relation) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6F);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x16);
        emit(&body, 0x6D);
        emit(&body, 0x1A);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected invalid static ref.cast relation to fail validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "type mismatch") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_type_canonicalization_and_function_subtyping) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, m->types[0].canonical_id != 0, "%s", "expected canonical ids to be assigned");
    WL_CHECK_MSG(t, m->types[0].canonical_id == m->types[2].canonical_id,
                 "expected equivalent function types to share a canonical id (%u vs %u)",
                 (unsigned)m->types[0].canonical_id,
                 (unsigned)m->types[2].canonical_id);
    WL_CHECK_MSG(t, wasm__type_equal(m, 0, 2), "%s", "expected structurally identical types to compare equal");
    WL_CHECK_MSG(t, !wasm__type_equal(m, 0, 1), "%s", "expected subtype body to remain distinct from its supertype");
    WL_CHECK_MSG(t, wasm__is_subtype(m, 1, 0), "%s", "expected declared subtype relation to hold");
    WL_CHECK_MSG(t, !wasm__is_subtype(m, 0, 1), "%s", "expected subtype relation to be directional");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_type_validation_accepts_struct_width_subtyping) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);
        emit(&sec, 0x00);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x6C);
        emit(&sec, 0x00);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK_MSG(t, wasm__is_subtype(m, 1, 0), "%s", "expected struct width/depth subtype to validate");
    WL_CHECK_MSG(t, !wasm__type_equal(m, 1, 0), "%s", "expected subtype to stay distinct from supertype");

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_type_validation_rejects_invalid_function_subtype) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected invalid function variance to fail load");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "valid subtype") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_type_validation_rejects_final_supertype) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected subtyping from a final type to fail load");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "final type") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_control_target_scan_accepts_signed_i32_const_leb) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, INT32_MIN);
        emit(&body, 0x1A);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_free_module(m);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_long_u32_leb128) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);
    emit(&mod, 0x01);
    emit_leb128_u32(&mod, 6);
    emit_malformed_uleb128_u32(&mod);

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected malformed long u32 LEB to fail load");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_long_i32_leb128) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "bad_i32", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_malformed_sleb128_i32(&body);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected malformed long i32 LEB to fail load");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_validation_rejects_long_i64_leb128) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7E);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "bad_i64", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x42);
        emit_malformed_sleb128_i64(&body);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected malformed long i64 LEB to fail load");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

/* ── Main ─────────────────────────────────────────────────────────── */

int main(void) {
    static const wl_test_case cases[] = {
        { "i32.add(3, 7) == 10", test_add },
        { "introspection: exports, custom sections, and signatures are queryable", test_introspection_helpers },
        { "factorial(10) == 3628800", test_factorial },
        { "host import: env.print(42)", test_host_import },
        { "public api: import metadata is queryable", test_import_introspection_api },
        { "public api: module userdata is visible in host callbacks", test_module_userdata_api },
        { "public api: shared host imports still see the right module instance", test_module_userdata_api_multiple_modules },
        { "public api: format-call packs ints and floats", test_format_call_api },
        { "public api: bind_host_func registers typed callbacks", test_bind_host_func_api },
        { "public api: bind_host_func callbacks can read module userdata", test_bind_host_func_uses_module_userdata },
        { "global import: mutable env.counter is read and updated by reference", test_imported_mutable_global },
        { "global init expr: global.get can read imported globals", test_global_init_expr_global_get_import },
        { "global init expr: i32 add/mul runs in initializer", test_global_init_expr_extended_i32_arithmetic },
        { "global init expr: i64 add/mul runs in initializer", test_global_init_expr_extended_i64_arithmetic },
        { "global init expr: local mutable globals stay invalid", test_init_expr_rejects_local_mutable_global_get },
        { "data offset expr: global.get can use imported __stack_pointer", test_data_offset_from_imported_global },
        { "data offset expr: add/mul runs in initializer", test_data_offset_extended_const_expr },
        { "element offset expr: add runs before active table init", test_element_offset_extended_const_expr },
        { "memory: store 99 at offset 0, load it back", test_memory },
        { "public api: safe memory read/write/string helpers", test_public_memory_helpers },
        { "public api: module-first host callbacks can read guest memory", test_host_callback_module_reads_memory },
        { "multi-memory: indexed load/store and memory APIs", test_multi_memory_indexed_access },
        { "multi-memory: indexed bulk ops plus size/grow", test_multi_memory_bulk_ops_and_growth },
        { "bulk memory: passive data, copy/fill/drop semantics", test_bulk_memory_ops },
        { "bulk memory: table init/copy/drop and element segment forms", test_bulk_memory_table_ops },
        { "bulk memory: memory.init/copy out-of-bounds traps", test_bulk_memory_oob_traps },
        { "bulk memory: table.init/copy out-of-bounds traps", test_bulk_memory_table_oob_traps },
        { "reference types: ref ops plus typed table get/set/grow/fill", test_reference_types_table_ops },
        { "reference types: expression-based passive/declarative elements", test_reference_types_expr_element_segments },
        { "reference types: active expr element segments with implicit funcref load", test_reference_types_active_expr_element_segment },
        { "loop: sum(1..10) == 55", test_loop },
        { "call with 17 params survives dynamic signature storage", test_large_param_call },
        { "call with 5 results survives dynamic result storage", test_multi_result_call },
        { "loop type index preserves block params on br_if", test_loop_type_index_block },
        { "0xFC immediates are skipped while scanning dead branches", test_prefixed_opcode_in_dead_branch },
        { "sign-extension ops: i32/i64 extend*_s", test_sign_extension_ops },
        { "startup: exported __wasm_call_ctors runs on load", test_exported_ctor_startup },
        { "multivalue if: type-index block returns two values", test_multivalue_if_block },
        { "multivalue br: block branch preserves two results", test_multivalue_branch_block },
        { "multivalue loop: br 0 preserves loop params", test_multivalue_loop_params },
        { "float min/max: NaN propagation and signed zero semantics", test_float_min_max_semantics },
        { "float trunc: trapping conversions reject NaN and overflow", test_trapping_float_to_int_conversions },
        { "float trunc: fractional lower-edge values still convert", test_trunc_allows_fractional_lower_edges },
        { "rotates: zero-count paths avoid shift-width UB", test_rotate_by_zero },
        { "SIMD helpers: arithmetic, compare, shuffle, and conversions", test_simd_helper_ops },
        { "SIMD: execution path, memory ops, select, and feature gate", test_simd_execution_and_feature_gate },
        { "trunc-sat ops: all 0xFC 0x00-0x07 cases", test_trunc_sat_ops },
        { "tail call: self-recursive return_call reuses the frame", test_tail_call_self_recursion },
        { "tail call: return_call_indirect dispatches through a table", test_tail_call_indirect },
        { "indirect call: structurally equal signatures are accepted", test_call_indirect_uses_structural_type_equality },
        { "wasmgc: composite type sections parse rec/sub/struct/array forms", test_gc_type_section_parses_composite_types },
        { "wasmgc: GC type syntax is feature-gated", test_gc_type_section_respects_feature_gate },
        { "wasmgc: concrete heaptype parsing preserves typed refs across sections", test_gc_heaptype_parsing_tracks_concrete_type_refs },
        { "wasmgc: ref.is_null works on abstract anyref nulls", test_gc_ref_is_null_on_abstract_anyref },
        { "wasmgc: typed select accepts eqref where anyref is expected", test_gc_typed_select_accepts_eqref_as_anyref },
        { "wasmgc: anyref tables accept eqref values via subtyping", test_gc_table_set_accepts_eqref_into_anyref_table },
        { "wasmgc: ref.eq accepts eqref subtypes", test_gc_ref_eq_accepts_eqref_subtypes },
        { "wasmgc: function imports accept GC subtype signatures", test_gc_function_import_accepts_subtype_signature },
        { "wasmgc: global imports accept GC subtype bindings", test_gc_global_import_accepts_subtype_binding },
        { "wasmgc: public helpers expose GC types and host allocators", test_gc_public_type_helpers_and_host_allocators },
        { "wasmgc: runtime init allocates a tracked GC heap arena", test_gc_runtime_initializes_heap_arena },
        { "wasmgc: i31ref values use tagged non-null storage", test_gc_i31ref_uses_tagged_non_null_storage },
        { "wasmgc: GC allocator tracks struct and array objects", test_gc_allocator_tracks_struct_and_array_objects },
        { "wasmgc: const expressions support GC allocation ops", test_gc_const_expr_supports_gc_alloc_ops },
        { "wasmgc: GC accessors handle packed fields and array bounds", test_gc_accessors_handle_packed_fields_and_array_bounds },
        { "wasmgc: collector traces roots and rewrites live graphs", test_gc_collect_marks_runtime_roots_and_preserves_object_graphs },
        { "wasmgc: allocator retries after collecting unreachable objects", test_gc_allocator_retries_after_collection_on_heap_exhaustion },
        { "wasmgc: runtime executes struct, array, i31, and conversion ops", test_gc_runtime_executes_struct_array_i31_and_conversion_ops },
        { "wasmgc: runtime executes casts and branch-on-cast ops", test_gc_runtime_executes_cast_and_branch_cast_opcodes },
        { "wasmgc: runtime executes segment-backed array ops", test_gc_runtime_executes_segment_backed_array_ops },
        { "wasmgc: validator accepts struct.new and array.new opcodes", test_gc_validation_accepts_struct_and_array_gc_opcodes },
        { "wasmgc: validator rejects struct.get on packed fields without signedness", test_gc_validation_rejects_struct_get_on_packed_field_without_sign },
        { "wasmgc: validator accepts ref.cast and br_on_cast opcodes", test_gc_validation_accepts_cast_and_branch_cast_opcodes },
        { "wasmgc: validator rejects invalid static ref.cast relations", test_gc_validation_rejects_invalid_ref_cast_relation },
        { "wasmgc: canonical ids and function subtyping are computed at load", test_gc_type_canonicalization_and_function_subtyping },
        { "wasmgc: struct width/depth subtyping validates", test_gc_type_validation_accepts_struct_width_subtyping },
        { "wasmgc: invalid function variance is rejected", test_gc_type_validation_rejects_invalid_function_subtype },
        { "wasmgc: final supertypes cannot be extended", test_gc_type_validation_rejects_final_supertype },
        { "loader: control-target scan accepts signed i32.const encodings", test_control_target_scan_accepts_signed_i32_const_leb },
        { "exceptions: typed catch receives payload values", test_exception_try_catch_payload },
        { "exceptions: catch_all handles unmatched tags", test_exception_catch_all },
        { "exceptions: rethrow forwards to outer catches", test_exception_rethrow },
        { "exceptions: delegate forwards to outer handlers", test_exception_delegate },
        { "reference types: typed select chooses funcref operands", test_typed_select_funcref },
        { "feature gate: disabled exceptions reject module load", test_feature_gate_exceptions_disabled },
        { "feature gate: disabled tail calls reject module load", test_feature_gate_tail_call_disabled },
        { "feature gate: disabled sign-extension rejects module load", test_feature_gate_sign_extension_disabled },
        { "runtime config: defaults initialize dynamic runtime capacities", test_runtime_config_defaults },
        { "runtime config: custom sizes allocate stack, frames, and buffers", test_runtime_config_custom_allocations },
        { "runtime config: zero limits are rejected", test_runtime_config_rejects_zero_limits },
        { "runtime config: validator stack limit is enforced at load", test_runtime_config_limits_operand_stack_validation },
        { "runtime config: validator label limit is enforced at load", test_runtime_config_limits_label_depth_validation },
        { "runtime config: call depth limit drives captured backtraces", test_runtime_config_limits_call_depth_and_backtrace },
        { "runtime: frame arena alloc/reset/destroy are well-formed", test_frame_arena_allocator },
        { "runtime: top-level execution grows the frame arena for validated modules", test_runtime_grows_frame_arena_for_execution },
        { "validation: duplicate export names are rejected at load", test_validation_rejects_duplicate_export_names },
        { "validation: invalid start signature is rejected at load", test_validation_rejects_invalid_start_signature },
        { "validation: stack type errors are rejected at load", test_validation_rejects_stack_type_errors },
        { "validation: memory.init requires a data count section", test_validation_requires_data_count_for_memory_init },
        { "validation: code body count must match declared functions", test_validation_rejects_code_body_count_mismatch },
        { "validation: data count must match data segment count", test_validation_rejects_data_count_mismatch },
        { "validation: call_indirect requires a funcref table", test_validation_rejects_call_indirect_on_externref_table },
        { "validation: invalid load alignment is rejected at load", test_validation_rejects_invalid_load_alignment },
        { "validation: section decoder failures populate loader errors", test_loader_reports_section_decode_errors },
        { "validation: long u32 LEBs are rejected during load", test_validation_rejects_long_u32_leb128 },
        { "validation: long i32 LEBs are rejected during load", test_validation_rejects_long_i32_leb128 },
        { "validation: long i64 LEBs are rejected during load", test_validation_rejects_long_i64_leb128 },
        { "memory: zero max blocks later growth", test_memory_grow_respects_zero_max },
        { "reject bad magic number", test_bad_magic },
        { "trap on i32.div_s by zero", test_div_by_zero },
        { "public api: backtrace helpers expose trap call stacks", test_public_backtrace_helpers_capture_traps },
        { "public api: fuel helpers meter finite execution", test_public_fuel_helpers_account_for_finite_calls },
        { "public api: fuel helpers stop infinite loops", test_public_fuel_helpers_trap_in_infinite_loop },
        { "public api: WASI stubs bind fd_write, environ_get, and proc_exit", test_public_wasi_stub_helpers },
        { "public api: WASI stubs expose args, env, fdstat, random, and clock helpers", test_public_wasi_metadata_helpers },
        { "reject global.set on immutable global", test_set_immutable_global_rejected },
        { "public api: exported globals can be read and updated safely", test_public_global_helpers },
    };

    return wl_test_run("wasm", cases, WL_COUNTOF(cases));
}
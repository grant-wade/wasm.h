/*
 * test_wasm.c — Test harness for wasm.h single-header runtime
 *
 * Builds hand-crafted Wasm modules as byte arrays and executes them.
 * Compile: cc -O2 -o test_wasm test_wasm.c -lm
 */

#define WL_IMPL
#include "wl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void wasm_test_init_checked(wl_test_ctx* t, wasm_runtime_t* rt, const char* test_name) {
    WL_CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0,
                 "allocator leak before %s: %zu live blocks",
                 test_name,
                 g_wasm_test_alloc_stats.live_blocks);
    wasm_init(rt);
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
#define wasm_destroy(rt) wasm_test_destroy_checked(t, (rt), ((t) && (t)->case_name) ? (t)->case_name : __func__)

#undef WL_TEST
#define WL_TEST(name)                              \
    static void name##_impl(wl_test_ctx* t);      \
    static void name(wl_test_ctx* t) {            \
        name##_impl(t);                           \
        wasm_test_verify_case_cleanup(t);         \
    }                                             \
    static void name##_impl(wl_test_ctx* t)

#if defined(_MSC_VER) && _MSC_VER < 1900
#define WASM_TEST_SNPRINTF _snprintf
#else
#define WASM_TEST_SNPRINTF snprintf
#endif

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

static void emit_export_func(wasm_builder_t* b, const char* name, uint32_t index) {
    uint32_t len = (uint32_t)strlen(name);

    emit_leb128_u32(b, len);
    emit_bytes(b, (const uint8_t*)name, len);
    emit(b, 0x00);
    emit_leb128_u32(b, index);
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

static wasm_error_t host_print(wasm_runtime_t* rt,
                               const wasm_value_t* args, uint32_t num_args,
                               wasm_value_t* results, uint32_t num_results,
                               void* userdata) {
    (void)rt;
    (void)results;
    (void)num_results;
    (void)userdata;
    host_print_called = 1;
    if (num_args > 0) host_print_value = args[0].of.i32;
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
    imp.is_mutable = 1;
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
    imp.is_mutable = 1;
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
    imp.is_mutable = 1;
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
    WL_CHECK_MSG(t, err == WASM_ERR_TRAP, "%s", "expected dropped passive segment to trap on memory.init");

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
    WL_CHECK_MSG(t, err == WASM_ERR_TRAP, "%s", "expected dropped passive element segment to trap on table.init");

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
        emit(&sec, 0x70);
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
    WL_CHECK_MSG(t, err == WASM_ERR_TRAP, "%s", "expected dropped passive expr element segment to trap on table.init");

    wasm_free_module(m);
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
    WL_CHECK_MSG(t, strstr(rt.error_msg, "stack underflow") != NULL, "%s", rt.error_msg);
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
        { "factorial(10) == 3628800", test_factorial },
        { "host import: env.print(42)", test_host_import },
        { "global import: mutable env.counter is read and updated by reference", test_imported_mutable_global },
        { "global init expr: global.get can read imported globals", test_global_init_expr_global_get_import },
        { "global init expr: i32 add/mul runs in initializer", test_global_init_expr_extended_i32_arithmetic },
        { "global init expr: i64 add/mul runs in initializer", test_global_init_expr_extended_i64_arithmetic },
        { "global init expr: local mutable globals stay invalid", test_init_expr_rejects_local_mutable_global_get },
        { "data offset expr: global.get can use imported __stack_pointer", test_data_offset_from_imported_global },
        { "data offset expr: add/mul runs in initializer", test_data_offset_extended_const_expr },
        { "element offset expr: add runs before active table init", test_element_offset_extended_const_expr },
        { "memory: store 99 at offset 0, load it back", test_memory },
        { "multi-memory: indexed load/store and memory APIs", test_multi_memory_indexed_access },
        { "multi-memory: indexed bulk ops plus size/grow", test_multi_memory_bulk_ops_and_growth },
        { "bulk memory: passive data, copy/fill/drop semantics", test_bulk_memory_ops },
        { "bulk memory: table init/copy/drop and element segment forms", test_bulk_memory_table_ops },
        { "bulk memory: memory.init/copy out-of-bounds traps", test_bulk_memory_oob_traps },
        { "bulk memory: table.init/copy out-of-bounds traps", test_bulk_memory_table_oob_traps },
        { "reference types: ref ops plus typed table get/set/grow/fill", test_reference_types_table_ops },
        { "reference types: expression-based passive/declarative elements", test_reference_types_expr_element_segments },
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
        { "trunc-sat ops: all 0xFC 0x00-0x07 cases", test_trunc_sat_ops },
        { "tail call: self-recursive return_call reuses the frame", test_tail_call_self_recursion },
        { "tail call: return_call_indirect dispatches through a table", test_tail_call_indirect },
        { "indirect call: structurally equal signatures are accepted", test_call_indirect_uses_structural_type_equality },
        { "exceptions: typed catch receives payload values", test_exception_try_catch_payload },
        { "exceptions: catch_all handles unmatched tags", test_exception_catch_all },
        { "exceptions: rethrow forwards to outer catches", test_exception_rethrow },
        { "exceptions: delegate forwards to outer handlers", test_exception_delegate },
        { "reference types: typed select chooses funcref operands", test_typed_select_funcref },
        { "feature gate: disabled exceptions reject module load", test_feature_gate_exceptions_disabled },
        { "feature gate: disabled tail calls reject module load", test_feature_gate_tail_call_disabled },
        { "feature gate: disabled sign-extension rejects module load", test_feature_gate_sign_extension_disabled },
        { "validation: duplicate export names are rejected at load", test_validation_rejects_duplicate_export_names },
        { "validation: invalid start signature is rejected at load", test_validation_rejects_invalid_start_signature },
        { "validation: stack type errors are rejected at load", test_validation_rejects_stack_type_errors },
        { "validation: memory.init requires a data count section", test_validation_requires_data_count_for_memory_init },
        { "validation: code body count must match declared functions", test_validation_rejects_code_body_count_mismatch },
        { "validation: data count must match data segment count", test_validation_rejects_data_count_mismatch },
        { "validation: call_indirect requires a funcref table", test_validation_rejects_call_indirect_on_externref_table },
        { "validation: invalid load alignment is rejected at load", test_validation_rejects_invalid_load_alignment },
        { "validation: long u32 LEBs are rejected during load", test_validation_rejects_long_u32_leb128 },
        { "validation: long i32 LEBs are rejected during load", test_validation_rejects_long_i32_leb128 },
        { "validation: long i64 LEBs are rejected during load", test_validation_rejects_long_i64_leb128 },
        { "memory: zero max blocks later growth", test_memory_grow_respects_zero_max },
        { "reject bad magic number", test_bad_magic },
        { "trap on i32.div_s by zero", test_div_by_zero },
        { "reject global.set on immutable global", test_set_immutable_global_rejected },
    };

    return wl_test_run("wasm", cases, WL_COUNTOF(cases));
}
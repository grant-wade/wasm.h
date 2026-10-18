/*
 * test_wasm.c — Test harness for wasm.h single-header runtime
 *
 * Builds hand-crafted Wasm modules as byte arrays and executes them.
 */

#define WL_IMPL
#include <stdio.h>
#include <math.h>
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

#if WASM_ENABLE_PLATFORM && (defined(_WIN32) || defined(__unix__) || defined(__unix) || defined(__APPLE__))
#define WASM_TEST_THREADS 1
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef HANDLE wasm_test_thread_t;
#else
#include <pthread.h>
typedef pthread_t wasm_test_thread_t;
#endif

typedef struct wasm_test_thread_start_t {
    void (*fn)(void*);
    void* arg;
} wasm_test_thread_start_t;

#if defined(_WIN32)
static DWORD WINAPI wasm_test_thread_main(LPVOID start) {
    ((wasm_test_thread_start_t*)start)->fn(((wasm_test_thread_start_t*)start)->arg);
    return 0;
}
#else
static void* wasm_test_thread_main(void* start) {
    ((wasm_test_thread_start_t*)start)->fn(((wasm_test_thread_start_t*)start)->arg);
    return NULL;
}
#endif

static int wasm_test_thread_spawn(wasm_test_thread_t* thread, wasm_test_thread_start_t* start) {
#if defined(_WIN32)
    *thread = CreateThread(NULL, 0, wasm_test_thread_main, start, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, wasm_test_thread_main, start) == 0;
#endif
}

static void wasm_test_thread_join(wasm_test_thread_t thread) {
#if defined(_WIN32)
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
}
#endif

#ifndef WASM_TEST_MODULAR
#define WASM_IMPL
#include "wasm.h"
#else
#include "../src/module.h"
#include "../src/runtime.h"
#include "../src/values.h"
#include "../src/portable.h"
#include "../src/types/canonical.h"
#include "../src/types/references.h"
#include "../src/exec/frames.h"
#include "../src/exec/interpreter.h"
#include "../src/exec/state.h"
#include "../src/platform/wasi_path.h"
#include "../src/platform/wasi_state.h"
#include "../src/simd/compare.h"
#include "../src/simd/float.h"
#include "../src/simd/integer_small.h"
#include "../src/simd/integer_wide.h"
#include "../src/simd/lanes.h"
#endif


#include <stddef.h>
#include <stdint.h>

typedef struct wasm_test_alloc_stats {
    size_t alloc_calls;
    size_t resize_calls;
    size_t free_calls;
    size_t live_blocks;
    size_t peak_live_blocks;
    size_t mismatches; /* frees and resizes whose size or alignment differ from the block's */
} wasm_test_alloc_stats;

static wasm_test_alloc_stats g_wasm_test_alloc_stats;

static size_t g_wasm_test_alloc_fail_countdown;

/* Counters change atomically because the threads test runs runtimes on several threads at once. */
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

static size_t wasm_test_counter_cas(size_t* counter, size_t expected, size_t desired) {
#if defined(__GNUC__) || defined(__clang__)
    __atomic_compare_exchange_n(counter, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
#elif defined(_MSC_VER) && defined(_WIN64)
    return (size_t)_InterlockedCompareExchange64((__int64 volatile*)counter, (__int64)desired, (__int64)expected);
#elif defined(_MSC_VER)
    return (size_t)_InterlockedCompareExchange((long volatile*)counter, (long)desired, (long)expected);
#else
    size_t current = *counter;
    if (current == expected) *counter = desired;
    return current;
#endif
}

/* Applies update to the counter's value until no other thread changed it in between; returns the new value. */
static size_t wasm_test_counter_update(size_t* counter, size_t (*update)(size_t)) {
    size_t observed = wasm_test_counter_cas(counter, 0, 0);
    for (;;) {
        size_t desired = update(observed);
        size_t previous = wasm_test_counter_cas(counter, observed, desired);
        if (previous == observed) return desired;
        observed = previous;
    }
}

static size_t wasm_test_increment(size_t value) {
    return value + 1u;
}

static size_t wasm_test_decrement_to_zero(size_t value) {
    return value > 0 ? value - 1u : 0;
}

static void wasm_test_record_live_alloc(void) {
    size_t live = wasm_test_counter_update(&g_wasm_test_alloc_stats.live_blocks, wasm_test_increment);
    size_t peak = wasm_test_counter_cas(&g_wasm_test_alloc_stats.peak_live_blocks, 0, 0);

    while (live > peak) {
        size_t previous = wasm_test_counter_cas(&g_wasm_test_alloc_stats.peak_live_blocks, peak, live);
        if (previous == peak) break;
        peak = previous;
    }
}

/* Bookkeeping stored just before each block the test allocator returns. */
typedef struct wasm_test_block_t {
    void* raw;
    size_t size;
    size_t align;
} wasm_test_block_t;

static int wasm_test_alloc_should_fail(void) {
    if (g_wasm_test_alloc_fail_countdown == 0) return 0;
    return --g_wasm_test_alloc_fail_countdown == 0;
}

static void* wasm_test_alloc_block(size_t size, size_t align) {
    wasm_test_block_t block;
    size_t pad = align < 16u ? 16u : align;
    uint8_t* raw;
    uint8_t* p;

    if (size == 0 || align == 0 || (align & (align - 1u)) != 0) {
        wasm_test_counter_update(&g_wasm_test_alloc_stats.mismatches, wasm_test_increment);
        return NULL;
    }
    if (size > SIZE_MAX - sizeof(block) - pad) return NULL;
    raw = (uint8_t*)malloc(sizeof(block) + pad + size);
    if (!raw) return NULL;
    p = (uint8_t*)(((uintptr_t)raw + sizeof(block) + pad - 1u) & ~(uintptr_t)(pad - 1u));
    block.raw = raw;
    block.size = size;
    block.align = align;
    memcpy(p - sizeof(block), &block, sizeof(block));
    memset(p, 0xA5, size);
    wasm_test_record_live_alloc();
    return p;
}

static void wasm_test_block_take(void* p, size_t size, size_t align, wasm_test_block_t* out) {
    memcpy(out, (uint8_t*)p - sizeof(*out), sizeof(*out));
    if (out->size != size || out->align != align)
        wasm_test_counter_update(&g_wasm_test_alloc_stats.mismatches, wasm_test_increment);
}

static void wasm_test_block_release(const wasm_test_block_t* block) {
    free(block->raw);
    wasm_test_counter_update(&g_wasm_test_alloc_stats.live_blocks, wasm_test_decrement_to_zero);
}

static void* wasm_test_alloc(void* ctx, size_t size, size_t align) {
    (void)ctx;
    wasm_test_counter_update(&g_wasm_test_alloc_stats.alloc_calls, wasm_test_increment);
    if (wasm_test_alloc_should_fail()) return NULL;
    return wasm_test_alloc_block(size, align);
}

static void* wasm_test_resize(void* ctx, void* p, size_t old_size, size_t new_size, size_t align) {
    wasm_test_block_t block;
    void* moved;

    (void)ctx;
    wasm_test_counter_update(&g_wasm_test_alloc_stats.resize_calls, wasm_test_increment);
    if (!p) {
        wasm_test_counter_update(&g_wasm_test_alloc_stats.mismatches, wasm_test_increment);
        return NULL;
    }
    if (wasm_test_alloc_should_fail()) return NULL;
    wasm_test_block_take(p, old_size, align, &block);
    moved = wasm_test_alloc_block(new_size, block.align);
    if (!moved) return NULL;
    memcpy(moved, p, block.size < new_size ? block.size : new_size);
    wasm_test_block_release(&block);
    return moved;
}

static void wasm_test_free(void* ctx, void* p, size_t size, size_t align) {
    wasm_test_block_t block;

    (void)ctx;
    wasm_test_counter_update(&g_wasm_test_alloc_stats.free_calls, wasm_test_increment);
    if (!p) {
        wasm_test_counter_update(&g_wasm_test_alloc_stats.mismatches, wasm_test_increment);
        return;
    }
    wasm_test_block_take(p, size, align, &block);
    wasm_test_block_release(&block);
}

static const wasm_allocator_t wasm_test_allocator = { NULL, wasm_test_alloc, wasm_test_resize, wasm_test_free };

static wasm_test_alloc_stats wasm_test_alloc_snapshot(void) {
    return g_wasm_test_alloc_stats;
}

static wasm_error_t wasm_test_init_raw(wasm_runtime_t* rt, const wasm_config_t* config) {
    return wasm__runtime_init(rt, &wasm_test_allocator, config);
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
    wasm__runtime_destroy(rt);
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
    WL_CHECK_MSG(t, g_wasm_test_alloc_stats.mismatches == 0,
                 "allocator misuse in test %s: %zu frees or resizes with the wrong size or alignment",
                 case_name,
                 g_wasm_test_alloc_stats.mismatches);
    g_wasm_test_alloc_stats.mismatches = 0;
    g_wasm_test_alloc_fail_countdown = 0;
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

#define WASM_CHECK_OK(t, err)                                                              \
    do {                                                                                   \
        wasm_error_t wasm_check_ok_err_ = (err);                                           \
        WL_CHECK_MSG((t), wasm_check_ok_err_ == WASM_OK, "%s", wasm_error_string(wasm_check_ok_err_)); \
    } while (0)
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

static void emit_leb128_u64(wasm_builder_t* b, uint64_t val) {
    do {
        uint8_t byte = (uint8_t)(val & 0x7Fu);
        val >>= 7;
        if (val) byte |= 0x80u;
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
                        uint64_t offset) {
    emit_leb128_u32(b, align_log2 | (has_memory_index ? 0x40u : 0u));
    if (has_memory_index) emit_leb128_u32(b, memory_index);
    emit_leb128_u64(b, offset);
}

static void emit_memory_type(wasm_builder_t* b,
                             int is_64,
                             int has_max,
                             uint64_t initial_pages,
                             uint64_t max_pages) {
    uint8_t flags = (uint8_t)((has_max ? 0x01u : 0u) | (is_64 ? 0x04u : 0u));

    emit(b, flags);
    if (is_64) {
        emit_leb128_u64(b, initial_pages);
        if (has_max) emit_leb128_u64(b, max_pages);
    } else {
        emit_leb128_u32(b, (uint32_t)initial_pages);
        if (has_max) emit_leb128_u32(b, (uint32_t)max_pages);
    }
}

static void emit_table_type(wasm_builder_t* b,
                            wasm_valtype_t reftype,
                            int is_64,
                            int has_max,
                            uint32_t initial_elems,
                            uint32_t max_elems) {
    uint8_t flags = (uint8_t)((has_max ? 0x01u : 0u) | (is_64 ? 0x04u : 0u));

    emit(b, (uint8_t)reftype);
    emit(b, flags);
    emit_leb128_u32(b, initial_elems);
    if (has_max) emit_leb128_u32(b, max_elems);
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

#if WASM_ENABLE_PLATFORM
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
#endif

static void emit_import_table(wasm_builder_t* b,
                              const char* module,
                              const char* name,
                              wasm_valtype_t reftype,
                              int is_64,
                              int has_max,
                              uint32_t initial_elems,
                              uint32_t max_elems) {
    uint32_t module_len = (uint32_t)strlen(module);
    uint32_t name_len = (uint32_t)strlen(name);

    emit_leb128_u32(b, module_len);
    emit_bytes(b, (const uint8_t*)module, module_len);
    emit_leb128_u32(b, name_len);
    emit_bytes(b, (const uint8_t*)name, name_len);
    emit(b, 0x01);
    emit_table_type(b, reftype, is_64, has_max, initial_elems, max_elems);
}

#if WASM_ENABLE_PLATFORM
static void emit_import_memory(wasm_builder_t* b,
                               const char* module,
                               const char* name,
                               int is_64,
                               int has_max,
                               uint64_t initial_pages,
                               uint64_t max_pages) {
    uint32_t module_len = (uint32_t)strlen(module);
    uint32_t name_len = (uint32_t)strlen(name);

    emit_leb128_u32(b, module_len);
    emit_bytes(b, (const uint8_t*)module, module_len);
    emit_leb128_u32(b, name_len);
    emit_bytes(b, (const uint8_t*)name, name_len);
    emit(b, 0x02);
    emit_memory_type(b, is_64, has_max, initial_pages, max_pages);
}
#endif

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

    args[0] = wasm_f32(1.0f);
    result = wasm_i32(99);
    err = wasm_call(m, "add", args, 2, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected argument type mismatch rejection");
    WASM_CHECK_I32(t, result.of.i32, 99);

    args[0] = wasm_i32(3);
    args[1] = wasm_i64(7);
    err = wasm_call_index(m, 0, args, 2, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected indexed argument type mismatch rejection");

    args[1] = wasm_i32(7);
    err = wasm_call_index(m, 0, args, 2, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 10);

    wasm_module_free(m);
    wasm_destroy(&rt);

    alloc_after = wasm_test_alloc_snapshot();
    alloc_delta = (alloc_after.alloc_calls - alloc_before.alloc_calls) +
                  (alloc_after.resize_calls - alloc_before.resize_calls);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* ── Test 3: host import (print function) ───────────────────────── */

static int host_print_called = 0;
static int32_t host_print_value = 0;
static int host_fmt_add_called = 0;

#if WASM_ENABLE_PLATFORM
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
#endif

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
    host_userdata_seen_value = wasm_module_userdata(mod);
    return WASM_OK;
}

static wasm_error_t host_module_userdata_add(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    int32_t bias = 0;

    (void)userdata;

    if (!mod || num_args != 2 || num_results != 1) return WASM_ERR_TYPE_MISMATCH;
    if (wasm_module_userdata(mod)) bias = *(const int32_t*)wasm_module_userdata(mod);

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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    WL_CHECK_MSG(t, wasm_module_userdata(m) == NULL,
                 "%s", "module userdata should default to null");
    wasm_module_set_userdata(m, &module_context);
    WL_CHECK_MSG(t, wasm_module_userdata(m) == &module_context,
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
    WL_CHECK_MSG(t, wasm_module_userdata(m) == NULL,
                 "%s", "module userdata should clear back to null");

    wasm_module_free(m);
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
        wasm_module_free(m1);
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

    wasm_module_free(m2);
    wasm_module_free(m1);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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
    if (m != NULL) wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 0), (int32_t)WASM_PAGE_SIZE);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 1), (int32_t)WASM_PAGE_SIZE);
    WL_CHECK_MSG(t, wasm_memory_data(m, 2) == NULL, "%s", "expected only two memories");

    memory0 = wasm_memory_data(m, 0);
    memory1 = wasm_memory_data(m, 1);
    WL_CHECK_MSG(t, memory0 != NULL, "%s", "expected memory 0 allocation");
    WL_CHECK_MSG(t, memory1 != NULL, "%s", "expected memory 1 allocation");
    if (memory0 && memory1) {
        WASM_CHECK_I32(t, memory0[0], 11);
        WASM_CHECK_I32(t, memory1[0], 31);
    }

    wasm_module_free(m);
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

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_multi_memory_bulk_ops_and_growth) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    int64_t old_pages;
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

    {
        static const uint8_t data_bytes[] = { 'a', 'b' };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
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

    memory0 = wasm_memory_data(m, 0);
    memory1 = wasm_memory_data(m, 1);
    WL_CHECK_MSG(t, memory0 != NULL, "%s", "expected memory 0 allocation");
    WL_CHECK_MSG(t, memory1 != NULL, "%s", "expected memory 1 allocation");
    if (memory0 && memory1) {
        WASM_CHECK_I32(t, memory0[4], 'a');
        WASM_CHECK_I32(t, memory0[5], 'b');
        WASM_CHECK_I32(t, memory1[0], 'a');
        WASM_CHECK_I32(t, memory1[1], 'b');
        WASM_CHECK_I32(t, memory1[6], '!');
    }
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 1), (int32_t)(2 * WASM_PAGE_SIZE));

    old_pages = wasm_memory_grow(m, 0, 1);
    WASM_CHECK_I64(t, old_pages, 1);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 0), (int32_t)(2 * WASM_PAGE_SIZE));

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_memory64_basic_ops) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    int64_t old_pages;
    uint8_t bytes[8] = { 0 };

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
        emit_memory_type(&sec, 1, 1, 1, 3);
        emit_section(&mod, 5, sec.buf, sec.len);
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

        emit(&body, 0x42);
        emit_leb128_i64(&body, 8);
        emit(&body, 0x42);
        emit_leb128_i64(&body, 42);
        emit(&body, 0x37);
        emit_memarg(&body, 3, 0, 0, 0);

        emit(&body, 0x3F);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x42);
        emit_leb128_i64(&body, 8);
        emit(&body, 0x29);
        emit_memarg(&body, 3, 0, 0, 0);
        emit(&body, 0x7C);

        emit(&body, 0x42);
        emit_leb128_i64(&body, 1);
        emit(&body, 0x40);
        emit_leb128_u32(&body, 0);
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

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I64(t, result.of.i64, 44);

    WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), (int64_t)(2u * WASM_PAGE_SIZE));

    err = wasm_memory_read(m, 0, 8, bytes, sizeof(bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, bytes[0], 42);
        WASM_CHECK_I32(t, bytes[1], 0);
    }

    old_pages = wasm_memory_grow(m, 0, 1);
    WASM_CHECK_I64(t, old_pages, 2);
    WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), (int64_t)(3u * WASM_PAGE_SIZE));

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_memory64_active_data_segment_offset) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_memory_type(&sec, 1, 0, 1, 0);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        static const uint8_t data_bytes[] = { 'z', 0 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x42);
        emit_leb128_i64(&sec, 4);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        uint8_t* memory = wasm_memory_data(m, 0);

        WL_CHECK_MSG(t, memory != NULL, "%s", "expected memory allocation");
        if (memory) {
            WASM_CHECK_I32(t, memory[4], 'z');
            WASM_CHECK_I32(t, memory[5], 0);
        }
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

WL_TEST(test_memory64_rejects_i32_address_operands) {
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
        emit_memory_type(&sec, 1, 0, 1, 0);
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
        emit_memarg(&body, 2, 0, 0, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected Memory64 load to reject i32 address operands");
    if (m) wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_table64_import_width_mismatch) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_table_t host_table;
    wasm_table_import_t import_desc;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_import_table(&sec, "host", "table", WASM_TYPE_FUNCREF, 0, 1, 1, 1);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&host_table, 0, sizeof(host_table));
    host_table.reftype = WASM_TYPE_FUNCREF;
    host_table.reftype_info.type = WASM_TYPE_FUNCREF;
    host_table.reftype_info.nullable = 1;
    host_table.size = 1;
    host_table.max_size = 1;
    host_table.is_64 = 1;

    memset(&import_desc, 0, sizeof(import_desc));
    import_desc.module = "host";
    import_desc.name = "table";
    import_desc.table = &host_table;

    err = wasm_register_table_import(&rt, &import_desc);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected table32 import to reject table64 host binding");
    if (m) wasm_module_free(m);
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

        memory = wasm_memory_data(m, 0);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    {
        static const uint8_t data_bytes[] = { 'X', 'Y' };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
        emit(&sec, 0xD2);
        emit_leb128_u32(&sec, 1);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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
        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
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

    wasm_module_free(m);
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
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
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

    wasm_module_free(m);
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
        wasm_module_free(m);
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "ext64", NULL, 0, &i64_result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, i32_result.of.i32, -32895);
        WASM_CHECK_I64(t, i64_result.of.i64, -32896);
    }

    wasm_module_free(m);
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

    wasm_module_free(m);
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
        wasm_module_free(m);
        wasm_destroy(&rt);
        return;
    }
    if (!(results[0].of.i32 == 11 && results[1].of.i32 == 22)) {
        WL_CHECK_MSG(t, false, "%s", "unexpected true-branch multivalue results");
        wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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
            wasm_module_free(m);
            wasm_destroy(&rt);
            return;
        }
        if (!is_i64[index] && result.of.i32 != expected_i32[index]) {
            WASM_CHECK_I32(t, result.of.i32, expected_i32[index]);
            wasm_module_free(m);
            wasm_destroy(&rt);
            return;
        }
        if (is_i64[index] && result.of.i64 != expected_i64[index]) {
            WASM_CHECK_I64(t, result.of.i64, expected_i64[index]);
            wasm_module_free(m);
            wasm_destroy(&rt);
            return;
        }
    }

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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
        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
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

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

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

    wasm_module_free(m);
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

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

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

    wasm_module_free(m);
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

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

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

    wasm_module_free(m);
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

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    emit_single_tag_section(&mod, 1);

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

    wasm_module_free(m);
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
    wasm_runtime_disable_feature(&rt, WASM_FEATURE_EXCEPTIONS);
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
    wasm_runtime_disable_feature(&rt, WASM_FEATURE_TAIL_CALL);
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
    wasm_runtime_disable_feature(&rt, WASM_FEATURE_SIGN_EXT);
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
    WL_CHECK_MSG(t, cfg.lazy_validation == 0,
                 "expected default lazy_validation 0, got %d",
                 cfg.lazy_validation);

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
    WL_CHECK_MSG(t, rt.lazy_validation == 0,
                 "expected runtime lazy_validation 0, got %d",
                 rt.lazy_validation);
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
    cfg.lazy_validation = 1;

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
    WL_CHECK_MSG(t, rt.lazy_validation == 1,
                 "expected lazy_validation 1, got %d",
                 rt.lazy_validation);
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

    WL_CHECK(t, wasm_runtime_new(&wasm_test_allocator, &cfg) == NULL);
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

WL_TEST(test_runtime_operand_stack_overflow_traps_at_call) {
    /* rec(n) keeps one value on the stack per pending call and returns n + 1. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x03, 0x02,
        0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x72, 0x65, 0x63, 0x00, 0x00, 0x0a, 0x17, 0x01, 0x15, 0x00, 0x41, 0x01,
        0x20, 0x00, 0x45, 0x04, 0x7f, 0x41, 0x00, 0x05, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x10, 0x00, 0x0b, 0x6a, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_config_t cfg;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);

    wasm_config_default(&cfg);
    cfg.max_stack_values = 16;
    cfg.max_call_depth = 1000;
    wasm_init_config(&rt, &cfg);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    arg = wasm_i32(10);
    WASM_CHECK_OK(t, wasm_call(m, "rec", &arg, 1, &result, 1));
    WL_CHECK(t, result.of.i32 == 11);
    arg = wasm_i32(100);
    WL_CHECK(t, wasm_call(m, "rec", &arg, 1, &result, 1) == WASM_ERR_STACK_OVERFLOW);
    WL_CHECK(t, rt.sp == 0 && rt.call_frame_sp == 0);
    arg = wasm_i32(5);
    WASM_CHECK_OK(t, wasm_call(m, "rec", &arg, 1, &result, 1));
    WL_CHECK(t, result.of.i32 == 6);

    wasm_module_free(m);
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
    WL_CHECK_MSG(t, wasm_runtime_call_depth(&rt) == 4,
                 "expected backtrace depth 4, got %u",
                 (unsigned)wasm_runtime_call_depth(&rt));
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &offset));
    WL_CHECK_MSG(t, func_idx == 0, "expected top backtrace function 0, got %u", (unsigned)func_idx);
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 3, NULL, &func_idx, &offset));
    WL_CHECK_MSG(t, func_idx == 0, "expected deepest captured function 0, got %u", (unsigned)func_idx);
    WL_CHECK_MSG(t, wasm_runtime_call_frame(&rt, 4, NULL, &func_idx, &offset) == WASM_ERR_MALFORMED,
                 "%s",
                 "expected querying beyond configured backtrace depth to fail");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_frame_arena_allocator) {
    wasm_runtime_t rt;
    wasm__arena_mark_t start;
    wasm__arena_mark_t middle;
    void* first;
    void* second;
    void* third;
    void* big;

    memset(&rt, 0, sizeof(rt));
    rt.allocator = wasm_test_allocator;

    WASM_CHECK_OK(t, wasm__arena_init(&rt, 64));
    WL_CHECK_MSG(t, rt.frame_arena != NULL, "%s", "expected a first arena block");
    WL_CHECK_MSG(t, rt.frame_arena_size == 64, "expected block size 64, got %zu", rt.frame_arena_size);
    start = wasm__arena_mark(&rt);

    first = wasm__arena_alloc(&rt, 1);
    second = wasm__arena_alloc(&rt, 8);
    WL_CHECK_MSG(t, first != NULL && second != NULL, "%s", "expected small allocations to succeed");
    WL_CHECK_MSG(t, ((uintptr_t)second - (uintptr_t)first) % 16u == 0, "%s", "expected aligned allocations");
    middle = wasm__arena_mark(&rt);

    big = wasm__arena_alloc(&rt, 1000);
    WL_CHECK_MSG(t, big != NULL, "%s", "expected an oversized allocation to add a block");
    WL_CHECK(t, rt.frame_arena != start.chunk && rt.frame_arena->prev == start.chunk);
    memset(big, 0xAB, 1000);

    wasm__arena_reset(&rt, middle);
    WL_CHECK(t, rt.frame_arena == start.chunk && rt.frame_arena_spare != NULL);
    third = wasm__arena_alloc(&rt, 16);
    WL_CHECK_MSG(t, third != NULL && third != first && third != second, "%s",
                 "expected allocation after the middle mark");
    WL_CHECK(t, wasm__arena_alloc(&rt, 1000) == big);

    wasm__arena_reset(&rt, start);
    WL_CHECK(t, wasm__arena_alloc(&rt, 1) == first);

    wasm__arena_destroy(&rt);
    WL_CHECK_MSG(t, rt.frame_arena == NULL && rt.frame_arena_spare == NULL, "%s",
                 "expected arena destroy to free every block");
}

WL_TEST(test_runtime_grows_frame_arena_for_execution) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    wasm_config_t cfg;

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
    cfg.frame_arena_size = 16;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    /* The frame outgrows the 16-byte block, so the call adds a block and drops it on return. */
    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 7);
    WL_CHECK(t, rt.frame_arena != NULL && rt.frame_arena->prev == NULL && rt.frame_arena->used == 0u);
    WL_CHECK(t, rt.frame_arena_spare != NULL && rt.frame_arena_spare->size > 16u);

    wasm_module_free(m);
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

WL_TEST(test_lazy_validation_defers_body_errors_until_first_call) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_config_t cfg;
    wasm_value_t result = { 0 };
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
        emit_export_func(&sec, "good", 0);
        emit_export_func(&sec, "bad", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t good = { 0 };
        wasm_builder_t bad = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&good, 0);
        emit(&good, 0x41);
        emit_leb128_i32(&good, 7);
        emit(&good, 0x0B);
        emit_leb128_u32(&sec, good.len);
        emit_bytes(&sec, good.buf, good.len);

        emit_leb128_u32(&bad, 0);
        emit(&bad, 0x41);
        emit_leb128_i32(&bad, 1);
        emit(&bad, 0x6A);
        emit(&bad, 0x0B);
        emit_leb128_u32(&sec, bad.len);
        emit_bytes(&sec, bad.buf, bad.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_config_default(&cfg);
    cfg.lazy_validation = 1;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "good", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_OK, "expected good export to run, got %d (%s)", (int)err, rt.error_msg);
    WL_CHECK_MSG(t, result.of.i32 == 7, "expected good export result 7, got %d", result.of.i32);

    err = wasm_call(m, "bad", NULL, 0, NULL, 0);
    WL_CHECK_MSG(t, err != WASM_OK, "%s", "expected bad export to fail deferred validation");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "type mismatch") != NULL, "%s", rt.error_msg);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_lazy_validation_still_runs_startup_bodies_at_load) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_config_t cfg;

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
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 8, sec.buf, sec.len);
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

    wasm_config_default(&cfg);
    cfg.lazy_validation = 1;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected invalid start body to fail during load under lazy validation");
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

    {
        static const uint8_t data_bytes[] = { 1 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_bytes(&sec, data_bytes, 1);
        emit_section(&mod, 11, sec.buf, sec.len);
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
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
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

static void wasm_test_build_locals_module(wasm_builder_t* mod, uint32_t local_count) {
    wasm_builder_t sec = { 0 };
    wasm_builder_t body = { 0 };

    emit_header(mod);
    emit_leb128_u32(&sec, 1);
    emit(&sec, 0x60);
    emit_leb128_u32(&sec, 0);
    emit_leb128_u32(&sec, 0);
    emit_section(mod, 1, sec.buf, sec.len);

    sec.len = 0;
    emit_leb128_u32(&sec, 1);
    emit_leb128_u32(&sec, 0);
    emit_section(mod, 3, sec.buf, sec.len);

    sec.len = 0;
    emit_leb128_u32(&body, 1);
    emit_leb128_u32(&body, local_count);
    emit(&body, 0x7F);
    emit(&body, 0x0B);
    emit_leb128_u32(&sec, 1);
    emit_leb128_u32(&sec, body.len);
    emit_bytes(&sec, body.buf, body.len);
    emit_section(mod, 10, sec.buf, sec.len);
}

WL_TEST(test_load_limits_function_locals) {
    wasm_builder_t at_limit = { 0 };
    wasm_builder_t over_limit = { 0 };
    wasm_builder_t huge = { 0 };
    wasm_runtime_t rt;
    wasm_config_t config;
    wasm_module_t* m;

    wasm_test_build_locals_module(&at_limit, WASM_MAX_FUNCTION_LOCALS);
    wasm_test_build_locals_module(&over_limit, WASM_MAX_FUNCTION_LOCALS + 1u);
    wasm_test_build_locals_module(&huge, 400000000u);

    wasm_init(&rt);
    m = wasm_load(&rt, at_limit.buf, at_limit.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    wasm_module_free(m);
    m = wasm_load(&rt, over_limit.buf, over_limit.len);
    WL_CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_MALFORMED && strstr(rt.error_msg, "too many locals"),
                 "expected the locals limit to reject the module, got: %s", rt.error_msg);
    m = wasm_load(&rt, huge.buf, huge.len);
    WL_CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "too many locals"),
                 "expected a 400M-local function to be rejected, got: %s", rt.error_msg);
    wasm_destroy(&rt);

    wasm_config_default(&config);
    config.max_function_locals = 0;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, over_limit.buf, over_limit.len);
    WL_CHECK_MSG(t, m != NULL, "expected max_function_locals = 0 to lift the limit: %s", rt.error_msg);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static size_t wasm_test_put_leb128_u32(uint8_t* out, uint32_t value) {
    size_t n = 0;

    do {
        uint8_t byte = (uint8_t)(value & 0x7Fu);

        value >>= 7;
        if (value) byte |= 0x80u;
        out[n++] = byte;
    } while (value);
    return n;
}

WL_TEST(test_canonicalization_dedupes_many_types) {
    enum { DISTINCT = 2048, PARAMS = 11, TYPE_BYTES = PARAMS + 3 };
    static const uint8_t header[] = { 0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00 };
    uint8_t* bytes = (uint8_t*)malloc(64u + (size_t)DISTINCT * 2u * TYPE_BYTES);
    uint8_t count[5];
    size_t count_len = wasm_test_put_leb128_u32(count, DISTINCT * 2u);
    size_t len = 0;
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t i;
    uint32_t d;
    uint32_t mismatches = 0;

    WL_CHECK_MSG(t, bytes != NULL, "%s", "allocation failed");
    if (!bytes) return;
    memcpy(bytes, header, sizeof(header));
    len = sizeof(header);
    bytes[len++] = 1;
    len += wasm_test_put_leb128_u32(bytes + len, (uint32_t)(count_len + (size_t)DISTINCT * 2u * TYPE_BYTES));
    memcpy(bytes + len, count, count_len);
    len += count_len;
    for (i = 0; i < DISTINCT * 2u; i++) {
        uint32_t bits = i % DISTINCT;

        bytes[len++] = 0x60;
        bytes[len++] = PARAMS;
        for (d = 0; d < PARAMS; d++) bytes[len++] = (bits >> d) & 1u ? 0x7E : 0x7F;
        bytes[len++] = 0;
    }

    wasm_init(&rt);
    m = wasm_load(&rt, bytes, len);
    free(bytes);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        for (i = 0; i < DISTINCT; i++) {
            if (m->types[i].canonical_id != m->types[i + DISTINCT].canonical_id) mismatches++;
            if (i > 0 && m->types[i].canonical_id == m->types[i - 1u].canonical_id) mismatches++;
        }
        WL_CHECK_MSG(t, mismatches == 0, "expected identical types to share ids and distinct types not to, %u wrong",
                     (unsigned)mismatches);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_public_api_tolerates_null_handles) {
    wasm_value_t value = wasm_i32(0);
    wasm_export_kind_t kind;
    uint32_t index = 0;
    uint32_t out_u32 = 0;
    size_t out_len = 0;
    uint8_t byte = 0;
    char text[16];
    wasm_import_t imp;
    wasm_global_import_t global_imp;
    wasm_table_import_t table_imp;
    wasm_memory_import_t memory_imp;
    wasm_tag_import_t tag_imp;

    memset(&imp, 0, sizeof(imp));
    memset(&global_imp, 0, sizeof(global_imp));
    memset(&table_imp, 0, sizeof(table_imp));
    memset(&memory_imp, 0, sizeof(memory_imp));
    memset(&tag_imp, 0, sizeof(tag_imp));

    WL_CHECK(t, wasm_runtime_last_error(NULL) != WASM_OK || 1);
    (void)wasm_runtime_error_message(NULL);
    wasm_runtime_clear_error(NULL);
    wasm_runtime_set_error(NULL, WASM_ERR_TRAP, "x");
    wasm_runtime_set_enabled_features(NULL, 0);
    (void)wasm_runtime_enabled_features(NULL);
    WL_CHECK(t, wasm_runtime_exit_code(NULL) == 0);
    WL_CHECK(t, wasm_register_import(NULL, &imp) != WASM_OK);
    WL_CHECK(t, wasm_register_global_import(NULL, &global_imp) != WASM_OK);
    WL_CHECK(t, wasm_register_table_import(NULL, &table_imp) != WASM_OK);
    WL_CHECK(t, wasm_register_memory_import(NULL, &memory_imp) != WASM_OK);
    WL_CHECK(t, wasm_register_tag_import(NULL, &tag_imp) != WASM_OK);
    wasm_runtime_enable_feature(NULL, WASM_FEATURE_SIMD);
    wasm_runtime_disable_feature(NULL, WASM_FEATURE_SIMD);
    wasm_runtime_enable_all_features(NULL);
    wasm_gc_collect(NULL);
    WL_CHECK(t, wasm_load(NULL, &byte, 1) == NULL);
    wasm_module_free(NULL);
    WL_CHECK(t, wasm_call(NULL, "f", NULL, 0, NULL, 0) != WASM_OK);
    WL_CHECK(t, wasm_call_index(NULL, 0, NULL, 0, NULL, 0) != WASM_OK);
    WL_CHECK(t, wasm_call_fmt(NULL, "f", "v()") != WASM_OK);
    WL_CHECK(t, wasm_bind_host_func(NULL, "m", "f", "v()", NULL, NULL) != WASM_OK);
    WL_CHECK(t, wasm_memory_count(NULL) == 0);
    WL_CHECK(t, wasm_memory_read(NULL, 0, 0, &byte, 1) != WASM_OK);
    WL_CHECK(t, wasm_memory_write(NULL, 0, 0, &byte, 1) != WASM_OK);
    WL_CHECK(t, wasm_memory_get_string(NULL, 0, 0, text, sizeof(text)) != WASM_OK);
    WL_CHECK(t, wasm_module_memory(NULL, 0) == NULL);
    WL_CHECK(t, wasm_memory_data(NULL, 0) == NULL);
    WL_CHECK(t, wasm_memory_size(NULL, 0) == 0);
    WL_CHECK(t, wasm_memory_grow(NULL, 0, 1) < 0);
    WL_CHECK(t, wasm_memory_base(NULL) == NULL);
    WL_CHECK(t, wasm_memory_length(NULL) == 0);
    wasm_memory_free(NULL);
    wasm_table_free(NULL);
    WL_CHECK(t, wasm_memory_new(&wasm_test_allocator, 2, 1, 0) == NULL);
    WL_CHECK(t, wasm_memory_new(NULL, 1, 1, 0) == NULL);
    WL_CHECK(t, wasm_shared_memory_new(NULL, 1, 1, 0) == NULL);
    WL_CHECK(t, wasm_table_new(&wasm_test_allocator, WASM_TYPE_I32, 1, 1) == NULL);
    WL_CHECK(t, wasm_table_new(NULL, WASM_TYPE_FUNCREF, 1, 1) == NULL);
    WL_CHECK(t, wasm_table_count(NULL) == 0);
    WL_CHECK(t, wasm_module_table(NULL, 0) == NULL);
    (void)wasm_table_type(NULL, 0);
    WL_CHECK(t, wasm_table_reftype(NULL, 0) == NULL);
    WL_CHECK(t, wasm_global_count(NULL) == 0);
    (void)wasm_global_type(NULL, 0);
    WL_CHECK(t, wasm_global_reftype(NULL, 0) == NULL);
    WL_CHECK(t, wasm_global_is_mutable(NULL, 0) == 0);
    WL_CHECK(t, wasm_module_global(NULL, 0) == NULL);
    WL_CHECK(t, wasm_global_get(NULL, "g", &value) != WASM_OK);
    WL_CHECK(t, wasm_global_set(NULL, "g", value) != WASM_OK);
    WL_CHECK(t, wasm_import_count(NULL) == 0);
    WL_CHECK(t, wasm_import_info(NULL, 0) == NULL);
    WL_CHECK(t, wasm_export_count(NULL) == 0);
    WL_CHECK(t, wasm_export_name(NULL, 0) == NULL);
    WL_CHECK(t, wasm_export_name_bytes(NULL, 0, &out_len) == NULL);
    (void)wasm_export_kind(NULL, 0);
    (void)wasm_export_index(NULL, 0);
    WL_CHECK(t, !wasm_find_export(NULL, "f", &kind, &index));
    WL_CHECK(t, !wasm_find_export_bytes(NULL, &byte, 1, &kind, &index));
    WL_CHECK(t, wasm_custom_section_count(NULL) == 0);
    WL_CHECK(t, wasm_custom_section_name(NULL, 0) == NULL);
    WL_CHECK(t, wasm_custom_section_data(NULL, 0, &out_len) == NULL);
    WL_CHECK(t, wasm_custom_section_find(NULL, "name", &out_len) == NULL);
    WL_CHECK(t, wasm_type_count(NULL) == 0);
    (void)wasm_type_kind(NULL, 0);
    WL_CHECK(t, wasm_type_functype(NULL, 0) == NULL);
    WL_CHECK(t, wasm_struct_type_field_count(NULL, 0) == 0);
    WL_CHECK(t, wasm_struct_type_field(NULL, 0, 0) == NULL);
    WL_CHECK(t, wasm_array_type_field(NULL, 0) == NULL);
    WL_CHECK(t, wasm_func_count(NULL) == 0);
    WL_CHECK(t, wasm_func_functype(NULL, 0) == NULL);
    WL_CHECK(t, wasm_func_param_count(NULL, 0) == 0);
    WL_CHECK(t, wasm_func_result_count(NULL, 0) == 0);
    (void)wasm_func_param_type(NULL, 0, 0);
    (void)wasm_func_result_type(NULL, 0, 0);
    WL_CHECK(t, wasm_tag_count(NULL) == 0);
    WL_CHECK(t, wasm_tag_functype(NULL, 0) == NULL);
    WL_CHECK(t, wasm_struct_new(NULL, 0, NULL, 0, &value) != WASM_OK);
    WL_CHECK(t, wasm_array_new(NULL, 0, NULL, 0, &value) != WASM_OK);
    WL_CHECK(t, wasm_struct_get_field(NULL, value, 0, &value) != WASM_OK);
    WL_CHECK(t, wasm_struct_set_field(NULL, value, 0, value) != WASM_OK);
    WL_CHECK(t, wasm_array_length(NULL, value, &out_u32) != WASM_OK);
    WL_CHECK(t, wasm_array_get_elem(NULL, value, 0, &value) != WASM_OK);
    WL_CHECK(t, wasm_array_set_elem(NULL, value, 0, value) != WASM_OK);
    wasm_runtime_backtrace(NULL, text, sizeof(text));
    WL_CHECK(t, wasm_runtime_call_depth(NULL) == 0);
    WL_CHECK(t, wasm_runtime_call_frame(NULL, 0, NULL, &index, &out_u32) != WASM_OK);
    wasm_runtime_set_fuel(NULL, 1);
    wasm_runtime_disable_fuel(NULL);
    WL_CHECK(t, wasm_runtime_fuel(NULL) == 0);
    WL_CHECK(t, wasm_module_runtime(NULL) == NULL);
    wasm_module_set_userdata(NULL, NULL);
    WL_CHECK(t, wasm_module_userdata(NULL) == NULL);
#if WASM_ENABLE_PLATFORM
    WL_CHECK(t, wasm_bind_wasi_stubs(NULL) != WASM_OK);
    WL_CHECK(t, wasm_bind_emscripten_stubs(NULL) != WASM_OK);
#endif
    WL_CHECK(t, wasm_wasi_set_args(NULL, 0, NULL) != WASM_OK);
    WL_CHECK(t, wasm_wasi_set_env(NULL, 0, NULL) != WASM_OK);
    WL_CHECK(t, wasm_wasi_preopen_dir(NULL, ".", "/") != WASM_OK);
    wasm_runtime_free(NULL);
}

WL_TEST(test_funcrefs_from_freed_modules_trap) {
    static const uint8_t provider_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x00, 0x01,
        0x70, 0x03, 0x03, 0x02, 0x00, 0x01, 0x07, 0x07, 0x01, 0x03, 0x67, 0x65, 0x74, 0x00, 0x01, 0x09, 0x05, 0x01,
        0x03, 0x00, 0x01, 0x00, 0x0a, 0x0b, 0x02, 0x04, 0x00, 0x41, 0x07, 0x0b, 0x04, 0x00, 0xd2, 0x00, 0x0b
    };
    static const uint8_t caller_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x02, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x01, 0x70,
        0x01, 0x7f, 0x03, 0x02, 0x01, 0x01, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x07, 0x08, 0x01, 0x04, 0x63, 0x61,
        0x6c, 0x6c, 0x00, 0x00, 0x0a, 0x0f, 0x01, 0x0d, 0x00, 0x41, 0x00, 0x20, 0x00, 0x26, 0x00, 0x41, 0x00, 0x11,
        0x00, 0x00, 0x0b
    };
    wasm_runtime_t rt;
    wasm_module_t* provider;
    wasm_module_t* caller;
    wasm_value_t ref = wasm_ref_null(WASM_TYPE_FUNCREF);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    provider = wasm_load(&rt, provider_bytes, sizeof(provider_bytes));
    caller = wasm_load(&rt, caller_bytes, sizeof(caller_bytes));
    WL_CHECK_MSG(t, provider != NULL && caller != NULL, "%s", rt.error_msg);
    if (provider && caller) {
        WASM_CHECK_OK(t, wasm_call(provider, "get", NULL, 0, &ref, 1));
        WASM_CHECK_OK(t, wasm_call(caller, "call", &ref, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 7);
        wasm_module_free(provider);
        provider = NULL;
        WL_CHECK_MSG(t, wasm_call(caller, "call", &ref, 1, &result, 1) != WASM_OK, "%s",
                     "expected a funcref into a freed module to trap");
    }
    wasm_module_free(provider);
    wasm_module_free(caller);
    wasm_destroy(&rt);
}

typedef struct wasm_test_forward_t {
    wasm_module_t* module;
    uint32_t func_index;
} wasm_test_forward_t;

static wasm_error_t wasm_test_forward_call(wasm_module_t* mod,
                                           const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results,
                                           void* userdata) {
    wasm_test_forward_t* forward = (wasm_test_forward_t*)userdata;

    (void)mod;
    return wasm__interp(forward->module, forward->func_index, (wasm_value_t*)args, num_args, results, num_results);
}

/* Exceptions thrown by an imported function reach handlers on every call path, and tail
   calls do not leave their own handlers active. */
WL_TEST(test_exceptions_from_imports_reach_handlers_on_all_call_paths) {
    static const uint8_t thrower_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x02, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f,
        0x01, 0x7f, 0x03, 0x02, 0x01, 0x01, 0x0d, 0x03, 0x01, 0x00, 0x00, 0x07, 0x0d, 0x02, 0x01, 0x65, 0x04, 0x00,
        0x05, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x00, 0x00, 0x0a, 0x08, 0x01, 0x06, 0x00, 0x20, 0x00, 0x08, 0x00, 0x0b,
    };
    static const uint8_t catcher_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x13, 0x04, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x01,
        0x7f, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x00, 0x01, 0x7f, 0x02, 0x12, 0x02, 0x01, 0x74, 0x01, 0x65,
        0x04, 0x00, 0x02, 0x01, 0x74, 0x05, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x00, 0x00, 0x03, 0x09, 0x08, 0x03, 0x03,
        0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x07, 0x4d, 0x05, 0x11, 0x76, 0x69,
        0x61, 0x5f, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x00, 0x01, 0x0c,
        0x76, 0x69, 0x61, 0x5f, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x72, 0x65, 0x66, 0x00, 0x02, 0x0d, 0x74, 0x61, 0x69,
        0x6c, 0x5f, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x00, 0x06, 0x08, 0x74, 0x61, 0x69, 0x6c, 0x5f,
        0x72, 0x65, 0x66, 0x00, 0x07, 0x0b, 0x74, 0x61, 0x69, 0x6c, 0x5f, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x00,
        0x08, 0x09, 0x0b, 0x02, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x00, 0x03, 0x00, 0x01, 0x00, 0x0a, 0xb1, 0x01, 0x08,
        0x16, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x41, 0x2a, 0x41, 0x00, 0x11, 0x00, 0x00, 0x1a,
        0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x15, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x41, 0x2b, 0xd2,
        0x00, 0x14, 0x00, 0x1a, 0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x18, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00,
        0x00, 0x20, 0x00, 0x41, 0x00, 0x13, 0x00, 0x00, 0x0b, 0x00, 0x0b, 0x41, 0xe4, 0x00, 0x6a, 0x0b, 0x17, 0x00,
        0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0xd2, 0x00, 0x15, 0x00, 0x0b, 0x00, 0x0b, 0x41,
        0xe4, 0x00, 0x6a, 0x0b, 0x15, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x12, 0x00,
        0x0b, 0x00, 0x0b, 0x41, 0xe4, 0x00, 0x6a, 0x0b, 0x13, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00,
        0x41, 0x05, 0x10, 0x03, 0x1a, 0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x13, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01, 0x00,
        0x00, 0x00, 0x41, 0x06, 0x10, 0x04, 0x1a, 0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x13, 0x00, 0x02, 0x7f, 0x1f, 0x40,
        0x01, 0x00, 0x00, 0x00, 0x41, 0x07, 0x10, 0x05, 0x1a, 0x0b, 0x41, 0x7f, 0x0b, 0x0b,
    };
    static const struct {
        const char* name;
        int32_t expected;
    } cases[] = {
        { "via_call_indirect", 42 },
        { "via_call_ref", 43 },
        { "tail_indirect", 5 },
        { "tail_ref", 6 },
        { "tail_direct", 7 },
    };
    wasm_runtime_t rt;
    wasm_module_t* thrower;
    wasm_module_t* catcher = NULL;
    wasm_test_forward_t forward;
    wasm_import_t func_import;
    wasm_tag_import_t tag_import;
    uint32_t func_index = 0;
    uint32_t tag_index = 0;
    wasm_export_kind_t kind = WASM_EXPORT_FUNC;
    uint32_t i;

    wasm_init(&rt);
    thrower = wasm_load(&rt, thrower_bytes, sizeof(thrower_bytes));
    WL_CHECK_MSG(t, thrower != NULL, "%s", rt.error_msg);
    if (thrower && wasm_find_export(thrower, "throw", &kind, &func_index) && wasm_find_export(thrower, "e", &kind, &tag_index)) {
        forward.module = thrower;
        forward.func_index = func_index;
        memset(&func_import, 0, sizeof(func_import));
        func_import.module = "t";
        func_import.name = "throw";
        func_import.type = *wasm_func_functype(thrower, func_index);
        func_import.type_module = thrower;
        func_import.type_index = thrower->funcs[func_index].type_idx;
        func_import.has_type_context = 1;
        func_import.func = wasm_test_forward_call;
        func_import.userdata = &forward;
        WASM_CHECK_OK(t, wasm__register_import_internal(&rt, &func_import, 0));

        memset(&tag_import, 0, sizeof(tag_import));
        tag_import.module = "t";
        tag_import.name = "e";
        tag_import.type = *wasm_tag_functype(thrower, tag_index);
        tag_import.type_module = thrower;
        tag_import.type_index = thrower->tags[tag_index].type_idx;
        tag_import.has_type_context = 1;
        tag_import.identity = thrower->tags[tag_index].identity;
        WASM_CHECK_OK(t, wasm_register_tag_import(&rt, &tag_import));

        catcher = wasm_load(&rt, catcher_bytes, sizeof(catcher_bytes));
        WL_CHECK_MSG(t, catcher != NULL, "%s", rt.error_msg);
    }
    for (i = 0; catcher && i < WL_COUNTOF(cases); i++) {
        wasm_value_t result = wasm_i32(0);
        wasm_error_t err = wasm_call(catcher, cases[i].name, NULL, 0, &result, 1);

        WL_CHECK_MSG(t, err == WASM_OK && result.of.i32 == cases[i].expected, "%s: expected %d, got err=%d result=%d (%s)",
                     cases[i].name, (int)cases[i].expected, (int)err, (int)result.of.i32, rt.error_msg);
    }
    wasm_module_free(catcher);
    wasm_module_free(thrower);
    wasm_destroy(&rt);
}

WL_TEST(test_runtime_limits_memory_and_tables) {
    static const uint8_t grow_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x03, 0x03,
        0x02, 0x00, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x05, 0x03, 0x01, 0x00, 0x02, 0x07, 0x19, 0x02, 0x08,
        0x67, 0x72, 0x6f, 0x77, 0x5f, 0x6d, 0x65, 0x6d, 0x00, 0x00, 0x0a, 0x67, 0x72, 0x6f, 0x77, 0x5f, 0x74, 0x61,
        0x62, 0x6c, 0x65, 0x00, 0x01, 0x0a, 0x12, 0x02, 0x06, 0x00, 0x20, 0x00, 0x40, 0x00, 0x0b, 0x09, 0x00, 0xd0,
        0x70, 0x20, 0x00, 0xfc, 0x0f, 0x00, 0x0b,
    };
    static const uint8_t big_memory_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x05, 0x03, 0x01, 0x00, 0x05,
    };
    static const uint8_t big_table_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x09,
    };
    wasm_runtime_t rt;
    wasm_config_t config;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);
    wasm_error_t err;

    wasm_config_default(&config);
    WL_CHECK(t, config.max_memory_bytes == 0u);
    WL_CHECK(t, config.max_table_elements == 10000000u);
    config.max_memory_bytes = 4u * WASM_PAGE_SIZE;
    config.max_table_elements = 8u;
    wasm_init_config(&rt, &config);

    m = wasm_load(&rt, big_memory_bytes, sizeof(big_memory_bytes));
    WL_CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_OOM && strstr(rt.error_msg, "limit"),
                 "expected a 5-page memory to exceed a 4-page limit, got: %s", rt.error_msg);
    wasm_module_free(m);
    m = wasm_load(&rt, big_table_bytes, sizeof(big_table_bytes));
    WL_CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_OOM && strstr(rt.error_msg, "limit"),
                 "expected a 9-element table to exceed an 8-element limit, got: %s", rt.error_msg);
    wasm_module_free(m);

    m = wasm_load(&rt, grow_bytes, sizeof(grow_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        arg = wasm_i32(2);
        err = wasm_call(m, "grow_mem", &arg, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, 2);
        arg = wasm_i32(1);
        err = wasm_call(m, "grow_mem", &arg, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, -1);
        WL_CHECK(t, wasm_memory_grow(m, 0, 1) == -1);
        WL_CHECK(t, wasm_memory_size(m, 0) == 4u * WASM_PAGE_SIZE);

        arg = wasm_i32(7);
        err = wasm_call(m, "grow_table", &arg, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, 1);
        arg = wasm_i32(1);
        err = wasm_call(m, "grow_table", &arg, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, -1);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);

    /* Each instance of grow_bytes starts with 2 pages; the runtime allows 5 in total. */
    wasm_config_default(&config);
    WL_CHECK(t, config.max_total_memory_bytes == 0u);
    config.max_total_memory_bytes = 5u * WASM_PAGE_SIZE;
    wasm_init_config(&rt, &config);
    {
        wasm_module_t* first = wasm_load(&rt, grow_bytes, sizeof(grow_bytes));
        wasm_module_t* second = wasm_load(&rt, grow_bytes, sizeof(grow_bytes));

        WL_CHECK_MSG(t, first != NULL && second != NULL, "%s", rt.error_msg);
        m = wasm_load(&rt, grow_bytes, sizeof(grow_bytes));
        WL_CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_OOM && strstr(rt.error_msg, "runtime's limit"),
                     "expected a third instance to exceed the runtime limit, got: %s", rt.error_msg);
        WL_CHECK(t, wasm_memory_grow(first, 0, 1) == 2);
        WL_CHECK(t, wasm_memory_grow(first, 0, 1) == -1);
        wasm_module_free(second);
        WL_CHECK(t, wasm_memory_grow(first, 0, 2) == 3);
        wasm_module_free(first);
    }
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_host_add(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                       wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    results[0] = wasm_i32(args[0].of.i32 + args[1].of.i32);
    return WASM_OK;
}

WL_TEST(test_allocator_contract) {
    const wasm_allocator_t* libc = wasm_libc_allocator();
    wasm_allocator_t missing = wasm_test_allocator;
    wasm_runtime_t* rt;
    size_t align;

    WL_CHECK(t, libc && libc->alloc && libc->resize && libc->free);
    for (align = 1; align <= 4096u; align *= 2u) {
        uint8_t* p = (uint8_t*)libc->alloc(libc->ctx, 3u, align);
        uint8_t* q;

        WL_CHECK_MSG(t, p && ((uintptr_t)p & (align - 1u)) == 0, "alloc misaligned for %zu", align);
        if (!p) continue;
        memcpy(p, "abc", 3u);
        q = (uint8_t*)libc->resize(libc->ctx, p, 3u, 5000u, align);
        WL_CHECK_MSG(t, q && ((uintptr_t)q & (align - 1u)) == 0 && memcmp(q, "abc", 3u) == 0,
                     "resize misaligned or lost data for %zu", align);
        libc->free(libc->ctx, q ? q : p, q ? 5000u : 3u, align);
    }

    WL_CHECK(t, wasm_runtime_new(NULL, NULL) == NULL);
    missing.resize = NULL;
    WL_CHECK(t, wasm_runtime_new(&missing, NULL) == NULL);
    WL_CHECK(t, wasm_memory_new(&missing, 1, 1, 0) == NULL);
    WL_CHECK(t, wasm_table_new(&missing, WASM_TYPE_FUNCREF, 1, 1) == NULL);
    rt = wasm_runtime_new(libc, NULL);
    WL_CHECK(t, rt != NULL && wasm_runtime_allocator(rt)->alloc == libc->alloc);
    wasm_runtime_free(rt);
}

WL_TEST(test_allocation_failures_unwind_without_leaks) {
    /* Imports env.add; run(1) grows memory and a table, allocates GC objects, throws through try_table,
     * calls through the table, and catches with a legacy try, returning 123. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x21, 0x07, 0x5f, 0x01, 0x7f, 0x01, 0x5e, 0x7f, 0x01,
        0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x02, 0x7f, 0x7e, 0x00, 0x60, 0x01,
        0x7f, 0x00, 0x60, 0x00, 0x02, 0x7f, 0x7e, 0x02, 0x0b, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x61, 0x64, 0x64,
        0x00, 0x03, 0x03, 0x05, 0x04, 0x02, 0x02, 0x02, 0x02, 0x04, 0x04, 0x01, 0x70, 0x00, 0x04, 0x05, 0x04, 0x01,
        0x01, 0x01, 0x04, 0x0d, 0x05, 0x02, 0x00, 0x04, 0x00, 0x05, 0x06, 0x06, 0x01, 0x7f, 0x01, 0x41, 0x05, 0x0b,
        0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x04, 0x09, 0x08, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x02, 0x01,
        0x02, 0x0a, 0x7e, 0x04, 0x07, 0x00, 0x20, 0x00, 0x41, 0x02, 0x6c, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x41, 0x03,
        0x6c, 0x0b, 0x0e, 0x00, 0x06, 0x7f, 0x20, 0x00, 0x08, 0x01, 0x07, 0x01, 0x41, 0x01, 0x6a, 0x0b, 0x0b, 0x5d,
        0x02, 0x01, 0x63, 0x00, 0x01, 0x63, 0x01, 0x41, 0x01, 0x40, 0x00, 0x1a, 0xd2, 0x01, 0x41, 0x02, 0xfc, 0x0f,
        0x00, 0x1a, 0x20, 0x00, 0xfb, 0x00, 0x00, 0x21, 0x01, 0x41, 0x07, 0x41, 0x03, 0xfb, 0x06, 0x01, 0x21, 0x02,
        0x02, 0x06, 0x1f, 0x40, 0x01, 0x00, 0x00, 0x00, 0x20, 0x00, 0x41, 0x01, 0x11, 0x02, 0x00, 0x23, 0x00, 0x6a,
        0x42, 0x09, 0x08, 0x00, 0x0b, 0x00, 0x0b, 0x1a, 0x20, 0x01, 0xfb, 0x02, 0x00, 0x00, 0x6a, 0x20, 0x02, 0x41,
        0x02, 0xfb, 0x0b, 0x01, 0x6a, 0x41, 0x10, 0x2d, 0x00, 0x00, 0x6a, 0x20, 0x00, 0x10, 0x03, 0x6a, 0x41, 0x01,
        0x10, 0x00, 0x0b, 0x0b, 0x0b, 0x01, 0x00, 0x41, 0x10, 0x0b, 0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x00, 0x58,
        0x04, 0x6e, 0x61, 0x6d, 0x65, 0x01, 0x1e, 0x04, 0x00, 0x03, 0x61, 0x64, 0x64, 0x01, 0x06, 0x64, 0x6f, 0x75,
        0x62, 0x6c, 0x65, 0x02, 0x06, 0x74, 0x72, 0x69, 0x70, 0x6c, 0x65, 0x03, 0x06, 0x6c, 0x65, 0x67, 0x61, 0x63,
        0x79, 0x02, 0x09, 0x01, 0x04, 0x02, 0x01, 0x01, 0x73, 0x02, 0x01, 0x61, 0x03, 0x06, 0x01, 0x04, 0x01, 0x00,
        0x01, 0x68, 0x04, 0x0e, 0x03, 0x00, 0x02, 0x70, 0x74, 0x01, 0x03, 0x61, 0x72, 0x72, 0x02, 0x02, 0x75, 0x6e,
        0x07, 0x04, 0x01, 0x00, 0x01, 0x67, 0x0b, 0x08, 0x02, 0x00, 0x01, 0x65, 0x01, 0x02, 0x65, 0x31,
    };
    size_t fail_at;
    int completed = 0;

    for (fail_at = 1; fail_at < 100000u && !completed; fail_at++) {
        wasm_runtime_t* rt;
        wasm_module_t* compiled = NULL;
        wasm_module_t* m = NULL;
        wasm_value_t* held = NULL;
        wasm_value_t arg = wasm_i32(1);
        wasm_value_t result = wasm_i32(0);
        wasm_value_t field = wasm_i32(4);
        wasm_value_t object;
        int32_t fmt_result = 0;
        wasm_error_t err = WASM_ERR_OOM;

        g_wasm_test_alloc_fail_countdown = fail_at;
        rt = wasm_runtime_new(&wasm_test_allocator, NULL);
        if (rt && wasm_bind_host_func(rt, "env", "add", "ii(i)", wasm_test_host_add, NULL) == WASM_OK) {
            compiled = wasm_compile(rt, module_bytes, sizeof(module_bytes));
            m = compiled ? wasm_instantiate(compiled) : NULL;
            if (m) err = wasm_call(m, "run", &arg, 1, &result, 1);
            if (err == WASM_OK) err = wasm_call_fmt(m, "run", "i(i)", 1, &fmt_result);
            if (err == WASM_OK) err = wasm_struct_new(m, 0, &field, 1, &object);
            if (err == WASM_OK) {
                held = wasm_ref_hold(rt, object);
                if (!held) err = WASM_ERR_OOM;
            }
            wasm_gc_collect(rt);
        }
        if (g_wasm_test_alloc_fail_countdown != 0) {
            completed = 1;
            WASM_CHECK_OK(t, err);
            WASM_CHECK_I32(t, result.of.i32, 123);
            WASM_CHECK_I32(t, fmt_result, 123);
        }
        g_wasm_test_alloc_fail_countdown = 0;
        if (held) wasm_ref_release(rt, held);
        wasm_module_free(m);
        wasm_module_free(compiled);
        wasm_runtime_free(rt);
        WL_CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0, "%zu blocks leaked when allocation %zu failed",
                     g_wasm_test_alloc_stats.live_blocks, fail_at);
        if (g_wasm_test_alloc_stats.live_blocks != 0) break;
    }
    WL_CHECK_MSG(t, completed, "%s", "expected the run to succeed once no allocation fails");
}

WL_TEST(test_compile_inspects_without_imports_and_instantiates_repeatedly) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x03, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60,
        0x00, 0x01, 0x7f, 0x60, 0x00, 0x00, 0x02, 0x2e, 0x04, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x61, 0x64, 0x64, 0x00,
        0x00, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x62,
        0x61, 0x73, 0x65, 0x03, 0x7f, 0x00, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x74, 0x61, 0x62, 0x01, 0x70, 0x00, 0x01,
        0x03, 0x03, 0x02, 0x01, 0x02, 0x06, 0x06, 0x01, 0x7f, 0x01, 0x23, 0x00, 0x0b, 0x07, 0x08, 0x01, 0x04, 0x62,
        0x75, 0x6d, 0x70, 0x00, 0x01, 0x08, 0x01, 0x02, 0x0a, 0x1e, 0x02, 0x0c, 0x00, 0x23, 0x01, 0x41, 0x01, 0x10,
        0x00, 0x24, 0x01, 0x23, 0x01, 0x0b, 0x0f, 0x00, 0x41, 0x00, 0x2d, 0x00, 0x00, 0x41, 0xe8, 0x00, 0x47, 0x04,
        0x40, 0x00, 0x0b, 0x0b, 0x0b, 0x08, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x02, 0x68, 0x69,
    };
    static const uint8_t trapping_start_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7f,
        0x03, 0x03, 0x02, 0x00, 0x01, 0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x01, 0x08, 0x01, 0x00, 0x0a, 0x0a, 0x02,
        0x03, 0x00, 0x00, 0x0b, 0x04, 0x00, 0x41, 0x01, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* compiled;
    wasm_module_t* first = NULL;
    wasm_module_t* second = NULL;
    wasm_module_t* m;
    wasm_memory_t memory;
    wasm_table_t table;
    wasm_value_t table_elems[1];
    wasm_value_t base = wasm_i32(10);
    wasm_memory_import_t memory_import;
    wasm_table_import_t table_import;
    wasm_global_import_t global_import;
    wasm_value_t result = wasm_i32(0);
    wasm_export_kind_t kind = WASM_EXPORT_FUNC;
    uint32_t index = 0;
    uint8_t byte = 0;
    wasm_error_t err;

    wasm_init(&rt);
    compiled = wasm_compile(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, compiled != NULL, "expected compile to need no imports: %s", rt.error_msg);
    if (!compiled) {
        wasm_destroy(&rt);
        return;
    }
    WL_CHECK(t, wasm_import_count(compiled) == 4u);
    WL_CHECK(t, wasm_find_export(compiled, "bump", &kind, &index) && kind == WASM_EXPORT_FUNC);
    WL_CHECK(t, wasm_func_result_count(compiled, index) == 1u);
    err = wasm_call(compiled, "bump", NULL, 0, &result, 1);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED && strstr(rt.error_msg, "not instantiated"), "%s", rt.error_msg);
    WL_CHECK(t, wasm_memory_read(compiled, 0, 0, &byte, 1) != WASM_OK);
    WL_CHECK(t, wasm_memory_data(compiled, 0) == NULL);
    WL_CHECK(t, wasm_module_global(compiled, 0) == NULL);

    m = wasm_instantiate(compiled);
    WL_CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "env"), "expected unresolved imports to fail: %s", rt.error_msg);

    memset(&memory, 0, sizeof(memory));
    memory.data = (uint8_t*)calloc(1, WASM_PAGE_SIZE);
    memory.pages = 1;
    memory.max_pages = WASM_MEMORY32_MAX_PAGES;
    memset(&table, 0, sizeof(table));
    table_elems[0] = wasm_ref_null(WASM_TYPE_FUNCREF);
    table.reftype = WASM_TYPE_FUNCREF;
    table.reftype_info.type = WASM_TYPE_FUNCREF;
    table.reftype_info.nullable = 1;
    table.elems = table_elems;
    table.size = 1;
    table.max_size = UINT32_MAX;
    memory_import.module = "env";
    memory_import.name = "mem";
    memory_import.memory = &memory;
    table_import.module = "env";
    table_import.name = "tab";
    table_import.table = &table;
    memset(&global_import, 0, sizeof(global_import));
    global_import.module = "env";
    global_import.name = "base";
    global_import.type = WASM_TYPE_I32;
    global_import.value = &base;
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "add", "ii(i)", wasm_test_host_add, NULL));
    WASM_CHECK_OK(t, wasm_register_memory_import(&rt, &memory_import));
    WASM_CHECK_OK(t, wasm_register_table_import(&rt, &table_import));
    WASM_CHECK_OK(t, wasm_register_global_import(&rt, &global_import));

    first = wasm_instantiate(compiled);
    WL_CHECK_MSG(t, first != NULL, "%s", rt.error_msg);
    second = wasm_instantiate(compiled);
    WL_CHECK_MSG(t, second != NULL, "%s", rt.error_msg);
    if (first && second) {
        WL_CHECK(t, memory.data[0] == 'h' && memory.data[1] == 'i');
        err = wasm_call(first, "bump", NULL, 0, &result, 1);
        WASM_CHECK_OK(t, err);
        err = wasm_call(first, "bump", NULL, 0, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, 12);
        err = wasm_call(second, "bump", NULL, 0, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, 11);
    }
    wasm_module_free(second);
    wasm_module_free(first);
    wasm_module_free(compiled);
    free(memory.data);

    compiled = wasm_compile(&rt, trapping_start_bytes, sizeof(trapping_start_bytes));
    WL_CHECK_MSG(t, compiled != NULL, "expected compile not to run the start function: %s", rt.error_msg);
    m = compiled ? wasm_instantiate(compiled) : NULL;
    WL_CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_UNREACHABLE && strstr(rt.error_msg, "unreachable"),
                 "expected the start function to trap: %s", rt.error_msg);
    wasm_module_free(m);
    wasm_module_free(compiled);
    wasm_destroy(&rt);
}

WL_TEST(test_load_rejects_misordered_sections) {
    /* Two export sections; the second used to replace the first and leak it. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x03, 0x02, 0x00,
        0x00, 0x07, 0x09, 0x02, 0x01, 0x61, 0x00, 0x00, 0x01, 0x62, 0x00, 0x01, 0x07, 0x09, 0x02, 0x01, 0x61, 0x00,
        0x00, 0x01, 0x62, 0x00, 0x01, 0x0a, 0x07, 0x02, 0x02, 0x00, 0x0b, 0x02, 0x00, 0x0b,
    };
    wasm_builder_t late_types = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_MALFORMED && strstr(rt.error_msg, "section 7 is repeated"),
                 "expected a repeated export section to be malformed, got: %s", rt.error_msg);
    wasm_module_free(m);
    m = wasm_compile(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK(t, m == NULL);
    wasm_module_free(m);

    emit_header(&late_types);
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 0);
        emit_section(&late_types, 3, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 0);
        emit_section(&late_types, 1, sec.buf, sec.len);
    }
    m = wasm_load(&rt, late_types.buf, late_types.len);
    WL_CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "section out of order: section 1 must come earlier"),
                 "expected a type section after the function section to be malformed, got: %s", rt.error_msg);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_bad_magic) {
    uint8_t bad[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x00, 0x00, 0x00 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_load(&rt, bad, sizeof(bad));
    if (m) {
        WL_CHECK_MSG(t, false, "%s", "should have rejected");
        wasm_module_free(m);
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

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_host_trap(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    return WASM_ERR_TRAP;
}

static wasm_error_t wasm_test_host_refuse(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_runtime_set_error(mod->rt, WASM_ERR_TRAP, "host refused");
    return WASM_ERR_TRAP;
}

static void wasm_test_expect_call_error(wl_test_ctx* t, const wasm_runtime_t* rt, const char* what, wasm_error_t err,
                                        wasm_error_t expected, const char* message) {
    WL_CHECK_MSG(t, err == expected, "%s: expected %s, got %s", what, wasm_error_string(expected),
                 wasm_error_string(err));
    WL_CHECK_MSG(t, wasm_runtime_last_error(rt) == err, "%s: last error %s does not match the returned code", what,
                 wasm_error_string(wasm_runtime_last_error(rt)));
    WL_CHECK_MSG(t, strcmp(wasm_runtime_error_message(rt), message) == 0, "%s: expected message '%s', got '%s'", what,
                 message, wasm_runtime_error_message(rt));
}

WL_TEST(test_failed_calls_record_trap_codes_and_messages) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0d, 0x03, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7f,
        0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x19, 0x02, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x66, 0x61, 0x69, 0x6c, 0x00,
        0x00, 0x03, 0x65, 0x6e, 0x76, 0x06, 0x72, 0x65, 0x66, 0x75, 0x73, 0x65, 0x00, 0x00, 0x03, 0x07, 0x06, 0x00,
        0x01, 0x02, 0x00, 0x00, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x0d, 0x03,
        0x01, 0x00, 0x00, 0x07, 0x3f, 0x08, 0x01, 0x74, 0x01, 0x00, 0x07, 0x75, 0x6e, 0x72, 0x65, 0x61, 0x63, 0x68,
        0x00, 0x02, 0x03, 0x6f, 0x6f, 0x62, 0x00, 0x03, 0x03, 0x64, 0x69, 0x76, 0x00, 0x04, 0x04, 0x73, 0x70, 0x69,
        0x6e, 0x00, 0x05, 0x05, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x00, 0x06, 0x0b, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x72,
        0x65, 0x66, 0x75, 0x73, 0x65, 0x00, 0x07, 0x04, 0x66, 0x61, 0x69, 0x6c, 0x00, 0x00, 0x09, 0x07, 0x01, 0x00,
        0x41, 0x00, 0x0b, 0x01, 0x02, 0x0a, 0x29, 0x06, 0x03, 0x00, 0x00, 0x0b, 0x09, 0x00, 0x41, 0xf0, 0xa2, 0x04,
        0x28, 0x02, 0x00, 0x0b, 0x07, 0x00, 0x41, 0x01, 0x20, 0x00, 0x6d, 0x0b, 0x07, 0x00, 0x03, 0x40, 0x0c, 0x00,
        0x0b, 0x0b, 0x04, 0x00, 0x08, 0x00, 0x0b, 0x04, 0x00, 0x10, 0x01, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    wasm_value_t ref;
    int32_t out = 0;

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "fail", "()", wasm_test_host_trap, NULL));
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "refuse", "()", wasm_test_host_refuse, NULL));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    wasm_test_expect_call_error(t, &rt, "unreachable", wasm_call(m, "unreach", NULL, 0, NULL, 0),
                                WASM_ERR_UNREACHABLE, "unreachable executed");
    wasm_test_expect_call_error(t, &rt, "out-of-bounds load", wasm_call(m, "oob", NULL, 0, &result, 1),
                                WASM_ERR_OUT_OF_BOUNDS, "out of bounds memory access");
    arg = wasm_i32(0);
    wasm_test_expect_call_error(t, &rt, "divide by zero", wasm_call(m, "div", &arg, 1, &result, 1),
                                WASM_ERR_DIV_BY_ZERO, "integer divide by zero");
    arg = wasm_i32(1);
    WASM_CHECK_OK(t, wasm_call(m, "div", &arg, 1, &result, 1));
    WL_CHECK(t, wasm_runtime_last_error(&rt) == WASM_OK);
    WL_CHECK(t, strcmp(wasm_runtime_error_message(&rt), "ok") == 0);

    wasm_runtime_set_fuel(&rt, 5);
    wasm_test_expect_call_error(t, &rt, "fuel", wasm_call(m, "spin", NULL, 0, NULL, 0), WASM_ERR_OUT_OF_FUEL,
                                "out of fuel");
    wasm_runtime_cancel(&rt);
    wasm_runtime_disable_fuel(&rt);

    wasm_test_expect_call_error(t, &rt, "uncaught exception", wasm_call(m, "throw", NULL, 0, NULL, 0), WASM_ERR_TRAP,
                                "uncaught exception tag 0");

    WASM_CHECK_OK(t, wasm_table_get(m, 0, 0, &ref));
    wasm_test_expect_call_error(t, &rt, "wasm_call_ref", wasm_call_ref(&rt, ref, NULL, 0, NULL, 0),
                                WASM_ERR_UNREACHABLE, "unreachable executed");
    wasm_test_expect_call_error(t, &rt, "wasm_call_index", wasm_call_index(m, 2, NULL, 0, NULL, 0),
                                WASM_ERR_UNREACHABLE, "unreachable executed");
    wasm_test_expect_call_error(t, &rt, "wasm_call_fmt", wasm_call_fmt(m, "div", "i(i)", 0, &out),
                                WASM_ERR_DIV_BY_ZERO, "integer divide by zero");

    wasm_test_expect_call_error(t, &rt, "host message", wasm_call(m, "call_refuse", NULL, 0, NULL, 0), WASM_ERR_TRAP,
                                "host refused");
    wasm_test_expect_call_error(t, &rt, "imported host function", wasm_call(m, "fail", NULL, 0, NULL, 0),
                                WASM_ERR_TRAP, "trap");

    wasm_module_free(m);
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

    WL_CHECK_MSG(t, wasm_runtime_call_depth(&rt) == 0, "%s", "expected empty initial backtrace");

    err = wasm_call(m, "run", NULL, 0, NULL, 0);
    WL_CHECK_MSG(t, err == WASM_ERR_UNREACHABLE, "%s", "expected unreachable trap");

    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 3);

    err = wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 2);
        WASM_CHECK_I32(t, (int32_t)offset, 0);
    }

    err = wasm_runtime_call_frame(&rt, 1, NULL, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 1);
        WASM_CHECK_I32(t, (int32_t)offset, 0);
    }

    err = wasm_runtime_call_frame(&rt, 2, NULL, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 0);
        WASM_CHECK_I32(t, (int32_t)offset, 0);
    }

    wasm_runtime_backtrace(&rt, backtrace, sizeof(backtrace));
    WL_CHECK_MSG(t, strstr(backtrace, "#0 func[2] at offset 0x0000") != NULL,
                 "%s", "expected top frame in formatted backtrace");
    WL_CHECK_MSG(t, strstr(backtrace, "#1 func[1] at offset 0x0000") != NULL,
                 "%s", "expected middle frame in formatted backtrace");
    WL_CHECK_MSG(t, strstr(backtrace, "#2 func[0] at offset 0x0000") != NULL,
                 "%s", "expected root frame in formatted backtrace");

    err = wasm_runtime_call_frame(&rt, 3, NULL, &func_idx, &offset);
    WL_CHECK_MSG(t, err == WASM_ERR_MALFORMED, "%s", "expected out-of-range frame query rejection");

    bad_arg = wasm_i32(7);
    err = wasm_call(m, "run", &bad_arg, 1, NULL, 0);
    WL_CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "%s", "expected signature mismatch rejection");
    WL_CHECK_MSG(t, wasm_runtime_call_depth(&rt) == 0, "%s", "expected backtrace reset before a non-executing call failure");

    wasm_runtime_backtrace(&rt, backtrace, sizeof(backtrace));
    WL_CHECK_MSG(t, backtrace[0] == '\0', "%s", "expected empty backtrace string after reset");

    wasm_module_free(m);
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

    WASM_CHECK_I64(t, (int64_t)wasm_runtime_fuel(&rt), 0);

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_I64(t, (int64_t)wasm_runtime_fuel(&rt), 0);

    wasm_runtime_set_fuel(&rt, 2);
    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_I64(t, (int64_t)wasm_runtime_fuel(&rt), 0);
    WL_CHECK(t, wasm_call(m, "run", NULL, 0, &result, 1) == WASM_ERR_OUT_OF_FUEL);
    wasm_runtime_cancel(&rt);

    wasm_runtime_disable_fuel(&rt);
    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 42);

    wasm_module_free(m);
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

    wasm_runtime_set_fuel(&rt, 5);
    err = wasm_call(m, "spin", NULL, 0, NULL, 0);
    WL_CHECK_MSG(t, err == WASM_ERR_OUT_OF_FUEL, "%s", "expected out of fuel trap");
    WASM_CHECK_I64(t, (int64_t)wasm_runtime_fuel(&rt), 0);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 1);

    err = wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &offset);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        WASM_CHECK_I32(t, (int32_t)func_idx, 0);
        WASM_CHECK_I32(t, (int32_t)offset, 2);
    }

    wasm_runtime_backtrace(&rt, backtrace, sizeof(backtrace));
    WL_CHECK_MSG(t, strstr(backtrace, "#0 func[0] at offset 0x0002") != NULL,
                 "%s", "expected fuel trap backtrace frame");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

#if WASM_ENABLE_PLATFORM
static uint32_t wasm_test_wasi_call(wl_test_ctx* t, wasm_module_t* m, const char* name, const wasm_value_t* args,
                                    uint32_t num_args) {
    wasm_value_t result = wasm_i32(-1);

    WASM_CHECK_OK(t, wasm_call(m, name, args, num_args, &result, 1));
    return (uint32_t)result.of.i32;
}

WL_TEST(test_wasi_config_args_env_preopens_and_poll) {
    /* Each export forwards its arguments to the wasi_snapshot_preview1 import of the same name. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0x03, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60,
        0x03, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x04, 0x7f, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x02, 0xf0, 0x01, 0x06,
        0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70, 0x72, 0x65,
        0x76, 0x69, 0x65, 0x77, 0x31, 0x0e, 0x61, 0x72, 0x67, 0x73, 0x5f, 0x73, 0x69, 0x7a, 0x65, 0x73, 0x5f, 0x67,
        0x65, 0x74, 0x00, 0x00, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74,
        0x5f, 0x70, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x08, 0x61, 0x72, 0x67, 0x73, 0x5f, 0x67, 0x65, 0x74,
        0x00, 0x00, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70,
        0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x11, 0x65, 0x6e, 0x76, 0x69, 0x72, 0x6f, 0x6e, 0x5f, 0x73, 0x69,
        0x7a, 0x65, 0x73, 0x5f, 0x67, 0x65, 0x74, 0x00, 0x00, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61,
        0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x0e, 0x66, 0x64, 0x5f,
        0x70, 0x72, 0x65, 0x73, 0x74, 0x61, 0x74, 0x5f, 0x67, 0x65, 0x74, 0x00, 0x00, 0x16, 0x77, 0x61, 0x73, 0x69,
        0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31,
        0x13, 0x66, 0x64, 0x5f, 0x70, 0x72, 0x65, 0x73, 0x74, 0x61, 0x74, 0x5f, 0x64, 0x69, 0x72, 0x5f, 0x6e, 0x61,
        0x6d, 0x65, 0x00, 0x01, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74,
        0x5f, 0x70, 0x72, 0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x0b, 0x70, 0x6f, 0x6c, 0x6c, 0x5f, 0x6f, 0x6e, 0x65,
        0x6f, 0x66, 0x66, 0x00, 0x02, 0x03, 0x07, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x05, 0x03, 0x01, 0x00,
        0x01, 0x07, 0x6f, 0x07, 0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x0e, 0x61, 0x72, 0x67, 0x73,
        0x5f, 0x73, 0x69, 0x7a, 0x65, 0x73, 0x5f, 0x67, 0x65, 0x74, 0x00, 0x06, 0x08, 0x61, 0x72, 0x67, 0x73, 0x5f,
        0x67, 0x65, 0x74, 0x00, 0x07, 0x11, 0x65, 0x6e, 0x76, 0x69, 0x72, 0x6f, 0x6e, 0x5f, 0x73, 0x69, 0x7a, 0x65,
        0x73, 0x5f, 0x67, 0x65, 0x74, 0x00, 0x08, 0x0e, 0x66, 0x64, 0x5f, 0x70, 0x72, 0x65, 0x73, 0x74, 0x61, 0x74,
        0x5f, 0x67, 0x65, 0x74, 0x00, 0x09, 0x13, 0x66, 0x64, 0x5f, 0x70, 0x72, 0x65, 0x73, 0x74, 0x61, 0x74, 0x5f,
        0x64, 0x69, 0x72, 0x5f, 0x6e, 0x61, 0x6d, 0x65, 0x00, 0x0a, 0x0b, 0x70, 0x6f, 0x6c, 0x6c, 0x5f, 0x6f, 0x6e,
        0x65, 0x6f, 0x66, 0x66, 0x00, 0x0b, 0x0a, 0x3d, 0x06, 0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0x10, 0x00, 0x0b,
        0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0x10, 0x01, 0x0b, 0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0x10, 0x02, 0x0b,
        0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0x10, 0x03, 0x0b, 0x0a, 0x00, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0x10,
        0x04, 0x0b, 0x0c, 0x00, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0x20, 0x03, 0x10, 0x05, 0x0b,
    };
    static const char* const argv[] = { "prog", "a b" };
    static const char* const env[] = { "KEY=value" };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[4];
    uint8_t buf[64];
    uint32_t value = 0;

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_wasi_set_args(&rt, 2, argv));
    WASM_CHECK_OK(t, wasm_bind_wasi_stubs(&rt));
    WASM_CHECK_OK(t, wasm_wasi_set_env(&rt, 1, env));
    WASM_CHECK_OK(t, wasm_wasi_preopen_dir(&rt, ".", "/work"));
    WL_CHECK(t, wasm_wasi_preopen_dir(&rt, "wasm_test_missing_dir/nested", "/missing") == WASM_ERR_MALFORMED);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "wasm_test_missing_dir") != NULL, "%s", rt.error_msg);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(0);
    args[1] = wasm_i32(4);
    WL_CHECK(t, wasm_test_wasi_call(t, m, "args_sizes_get", args, 2) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 0, &value, 4));
    WL_CHECK(t, value == 2u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 4, &value, 4));
    WL_CHECK(t, value == 9u);
    args[0] = wasm_i32(16);
    args[1] = wasm_i32(32);
    WL_CHECK(t, wasm_test_wasi_call(t, m, "args_get", args, 2) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 32, buf, 9));
    WL_CHECK(t, memcmp(buf, "prog\0a b\0", 9) == 0);

    args[0] = wasm_i32(0);
    args[1] = wasm_i32(4);
    WL_CHECK(t, wasm_test_wasi_call(t, m, "environ_sizes_get", args, 2) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 0, &value, 4));
    WL_CHECK(t, value == 1u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 4, &value, 4));
    WL_CHECK(t, value == 10u);

    args[0] = wasm_i32(3);
    args[1] = wasm_i32(0);
    WL_CHECK(t, wasm_test_wasi_call(t, m, "fd_prestat_get", args, 2) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 4, &value, 4));
    WL_CHECK(t, value == 5u);
    args[1] = wasm_i32(64);
    args[2] = wasm_i32(5);
    WL_CHECK(t, wasm_test_wasi_call(t, m, "fd_prestat_dir_name", args, 3) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 64, buf, 5));
    WL_CHECK(t, memcmp(buf, "/work", 5) == 0);
    args[0] = wasm_i32(4);
    args[1] = wasm_i32(0);
    WL_CHECK(t, wasm_test_wasi_call(t, m, "fd_prestat_get", args, 2) == 8u);

    /* One relative 1 ms monotonic clock subscription with userdata 42, and one on a closed fd. */
    memset(buf, 0, sizeof(buf));
    buf[0] = 42;
    buf[8] = 0;
    buf[16] = 1;
    buf[24 + 2] = 0x0F;
    buf[24 + 1] = 0x42;
    buf[24] = 0x40;
    WASM_CHECK_OK(t, wasm_memory_write(m, 0, 256, buf, 48));
    args[0] = wasm_i32(256);
    args[1] = wasm_i32(512);
    args[2] = wasm_i32(1);
    args[3] = wasm_i32(600);
    WL_CHECK(t, wasm_test_wasi_call(t, m, "poll_oneoff", args, 4) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 600, &value, 4));
    WL_CHECK(t, value == 1u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 512, buf, 32));
    WL_CHECK(t, buf[0] == 42 && buf[8] == 0 && buf[9] == 0 && buf[10] == 0);

    memset(buf, 0, sizeof(buf));
    buf[0] = 7;
    buf[8] = 1;
    buf[16] = 40;
    WASM_CHECK_OK(t, wasm_memory_write(m, 0, 256 + 48, buf, 48));
    args[2] = wasm_i32(2);
    WL_CHECK(t, wasm_test_wasi_call(t, m, "poll_oneoff", args, 4) == 0u);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 600, &value, 4));
    WL_CHECK_MSG(t, value == 1u, "expected only the fd event, got %u events", (unsigned)value);
    WASM_CHECK_OK(t, wasm_memory_read(m, 0, 512, buf, 32));
    WL_CHECK(t, buf[0] == 7 && buf[8] == 8 && buf[10] == 1);

    args[2] = wasm_i32(0);
    WL_CHECK(t, wasm_test_wasi_call(t, m, "poll_oneoff", args, 4) == 28u);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_wasi_proc_raise_does_not_signal_the_host) {
    /* raise(sig) forwards to wasi_snapshot_preview1.proc_raise. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x25,
        0x01, 0x16, 0x77, 0x61, 0x73, 0x69, 0x5f, 0x73, 0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f, 0x70, 0x72,
        0x65, 0x76, 0x69, 0x65, 0x77, 0x31, 0x0a, 0x70, 0x72, 0x6f, 0x63, 0x5f, 0x72, 0x61, 0x69, 0x73, 0x65, 0x00,
        0x00, 0x03, 0x02, 0x01, 0x00, 0x07, 0x09, 0x01, 0x05, 0x72, 0x61, 0x69, 0x73, 0x65, 0x00, 0x01, 0x0a, 0x08,
        0x01, 0x06, 0x00, 0x20, 0x00, 0x10, 0x00, 0x0b,
    };
    static const int32_t signals[] = { 2, 6, 9, 11, 15 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t i;

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_wasi_stubs(&rt));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        wasm_value_t arg = wasm_i32(signals[i]);
        wasm_value_t result = wasm_i32(0);

        WASM_CHECK_OK(t, wasm_call(m, "raise", &arg, 1, &result, 1));
        WL_CHECK_MSG(t, result.of.i32 == 52, "expected ENOSYS for signal %d, got %d", (int)signals[i],
                     (int)result.of.i32);
    }
    wasm_module_free(m);
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
    wasm_test_stream_capture capture = { 0 };
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
        emit(&quit_body, 0x00);
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
    WASM_CHECK_I32(t, (int32_t)err, (int32_t)WASM_ERR_EXIT);
    WASM_CHECK_I32(t, wasm_runtime_exit_code(&rt), 5);

    wasm_module_free(m);
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

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_public_wasi_preview1_surface_imports) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 17);

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
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 5);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 7);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 7);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 9);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 6);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
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

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 46);
        emit_import_func(&sec, "wasi_snapshot_preview1", "args_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "args_sizes_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_sizes_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "clock_res_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "clock_time_get", 1);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_advise", 2);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_allocate", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_close", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_datasync", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_set_flags", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_set_rights", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_filestat_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_filestat_set_size", 15);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_filestat_set_times", 2);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_pread", 5);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_prestat_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_prestat_dir_name", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_pwrite", 5);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_read", 7);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_readdir", 5);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_renumber", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_seek", 16);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_sync", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_tell", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_write", 7);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_create_directory", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_filestat_get", 8);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_filestat_set_times", 9);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_link", 10);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_open", 11);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_readlink", 12);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_remove_directory", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_rename", 12);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_symlink", 8);
        emit_import_func(&sec, "wasi_snapshot_preview1", "path_unlink_file", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "poll_oneoff", 7);
        emit_import_func(&sec, "wasi_snapshot_preview1", "proc_exit", 14);
        emit_import_func(&sec, "wasi_snapshot_preview1", "proc_raise", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sched_yield", 13);
        emit_import_func(&sec, "wasi_snapshot_preview1", "random_get", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sock_accept", 6);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sock_recv", 12);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sock_send", 8);
        emit_import_func(&sec, "wasi_snapshot_preview1", "sock_shutdown", 0);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    wasm_init(&rt);
    err = wasm_bind_wasi_stubs(&rt);
    WASM_CHECK_OK(t, err);
    WL_CHECK_MSG(t, wasm__wasi_fd_entry(&rt, 3u) == NULL,
                 "%s", "WASI bindings must not implicitly preopen the host working directory");

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static uint32_t wasm_test_wasi_path_open(wasm_module_t* m, uint32_t dir_fd, const char* path,
                                         uint32_t oflags, uint32_t* out_fd) {
    wasm_value_t args[9];
    wasm_value_t result = wasm_i32(-1);
    uint8_t fd_bytes[4] = { 0 };
    size_t len = strlen(path);

    if (wasm_memory_write(m, 0, 256, path, len) != WASM_OK) return UINT32_MAX;
    args[0] = wasm_i32((int32_t)dir_fd);
    args[1] = wasm_i32(1);
    args[2] = wasm_i32(256);
    args[3] = wasm_i32((int32_t)len);
    args[4] = wasm_i32((int32_t)oflags);
    args[5] = wasm_i64(-1);
    args[6] = wasm_i64(-1);
    args[7] = wasm_i32(0);
    args[8] = wasm_i32(128);
    if (wasm__wasi_path_open(m, args, 9, &result, 1, NULL) != WASM_OK) return UINT32_MAX;
    if (out_fd && wasm_memory_read(m, 0, 128, fd_bytes, sizeof(fd_bytes)) == WASM_OK)
        *out_fd = wasm__load_le32(fd_bytes);
    return (uint32_t)result.of.i32;
}

#if WASM__PLATFORM_POSIX
static uint32_t wasm_test_wasi_symlink(wasm_module_t* m, uint32_t dir_fd, const char* target, const char* link) {
    wasm_value_t args[5];
    wasm_value_t result = wasm_i32(-1);
    size_t target_len = strlen(target);
    size_t link_len = strlen(link);

    if (wasm_memory_write(m, 0, 256, target, target_len) != WASM_OK) return UINT32_MAX;
    if (wasm_memory_write(m, 0, 512, link, link_len) != WASM_OK) return UINT32_MAX;
    args[0] = wasm_i32(256);
    args[1] = wasm_i32((int32_t)target_len);
    args[2] = wasm_i32((int32_t)dir_fd);
    args[3] = wasm_i32(512);
    args[4] = wasm_i32((int32_t)link_len);
    if (wasm__wasi_path_symlink(m, args, 5, &result, 1, NULL) != WASM_OK) return UINT32_MAX;
    return (uint32_t)result.of.i32;
}
#endif

WL_TEST(test_wasi_paths_stay_beneath_preopens) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t errno_value;

    emit_header(&mod);
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_memory_type(&sec, 0, 0, 1u, 0u);
        emit_section(&mod, 5, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_kind(&sec, "memory", 0x02, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_wasi_stubs(&rt));
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm__wasi_state_ensure(&rt));

    errno_value = wasm_test_wasi_path_open(m, WASM__WASI_FD_STDIN, "/etc/hosts", 0u, NULL);
    WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_BADF);
    errno_value = wasm_test_wasi_path_open(m, WASM__WASI_FD_STDOUT, "hosts", 0u, NULL);
    WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_BADF);

#if WASM__PLATFORM_POSIX
    {
        char root[64];
        char path[128];
        int root_fd;
        uint32_t root_wasi_fd = 0u;
        uint32_t fd = 0u;
        FILE* f;

        snprintf(root, sizeof(root), "/tmp/wasm_test_wasi_%ld", (long)getpid());
        WL_CHECK_MSG(t, mkdir(root, 0700) == 0, "%s", "mkdir root failed");
        snprintf(path, sizeof(path), "%s/sub", root);
        WL_CHECK_MSG(t, mkdir(path, 0700) == 0, "%s", "mkdir failed");
        snprintf(path, sizeof(path), "%s/file.txt", root);
        f = fopen(path, "w");
        WL_CHECK_MSG(t, f != NULL, "%s", "fopen failed");
        if (f) fclose(f);

        root_fd = open(root, O_RDONLY);
        WL_CHECK_MSG(t, root_fd >= 0, "%s", "open root failed");
        WASM_CHECK_OK(t, wasm__wasi_state_add_fd(&rt, root_fd, 1, WASM__WASI_FILETYPE_DIRECTORY,
                                                 WASM__WASI_RIGHTS_ALL, WASM__WASI_RIGHTS_ALL,
                                                 NULL, "/sandbox", &root_wasi_fd));

        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "file.txt", 0u, &fd);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_SUCCESS);
        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "sub/./../file.txt", 0u, &fd);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_SUCCESS);
        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "/etc/hosts", 0u, NULL);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "../file.txt", 0u, NULL);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "sub/../../file.txt", 0u, NULL);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);

        errno_value = wasm_test_wasi_path_open(m, root_wasi_fd, "sub", 0x2u, &fd);
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_SUCCESS);
        if (errno_value == WASM__WASI_ERRNO_SUCCESS) {
            errno_value = wasm_test_wasi_path_open(m, fd, "../file.txt", 0u, NULL);
            WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        }

        errno_value = wasm_test_wasi_symlink(m, root_wasi_fd, "/etc/hosts", "abs_link");
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        errno_value = wasm_test_wasi_symlink(m, root_wasi_fd, "../../file.txt", "sub/up_link");
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_NOTCAPABLE);
        errno_value = wasm_test_wasi_symlink(m, root_wasi_fd, "../file.txt", "sub/ok_link");
        WASM_CHECK_I32(t, (int32_t)errno_value, (int32_t)WASM__WASI_ERRNO_SUCCESS);

        snprintf(path, sizeof(path), "%s/sub/ok_link", root);
        (void)unlink(path);
        snprintf(path, sizeof(path), "%s/sub", root);
        (void)rmdir(path);
        snprintf(path, sizeof(path), "%s/file.txt", root);
        (void)unlink(path);
        (void)rmdir(root);
    }
#endif

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_public_emscripten_stub_helpers) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_memory_t* memory;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 13);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7C);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
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
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 7);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 6);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
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

        emit_leb128_u32(&sec, 36);
        emit_import_func(&sec, "env", "__syscall_faccessat", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_close", 1);
        emit_import_func(&sec, "env", "emscripten_date_now", 2);
        emit_import_func(&sec, "env", "__syscall_fchmod", 3);
        emit_import_func(&sec, "env", "__syscall_chmod", 3);
        emit_import_func(&sec, "env", "__syscall_fchown32", 4);
        emit_import_func(&sec, "env", "__syscall_fcntl64", 4);
        emit_import_func(&sec, "env", "__syscall_openat", 0);
        emit_import_func(&sec, "env", "__syscall_ioctl", 4);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_write", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_read", 0);
        emit_import_func(&sec, "env", "__syscall_fstat64", 3);
        emit_import_func(&sec, "env", "__syscall_stat64", 3);
        emit_import_func(&sec, "env", "__syscall_newfstatat", 0);
        emit_import_func(&sec, "env", "__syscall_lstat64", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_sync", 1);
        emit_import_func(&sec, "env", "__syscall_ftruncate64", 6);
        emit_import_func(&sec, "env", "__syscall_getcwd", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_sizes_get", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_get", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_seek", 5);
        emit_import_func(&sec, "env", "__syscall_mkdirat", 4);
        emit_import_func(&sec, "env", "emscripten_get_now", 2);
        emit_import_func(&sec, "env", "_tzset_js", 7);
        emit_import_func(&sec, "env", "_localtime_js", 8);
        emit_import_func(&sec, "env", "_munmap_js", 10);
        emit_import_func(&sec, "env", "_mmap_js", 9);
        emit_import_func(&sec, "wasi_snapshot_preview1", "clock_time_get", 11);
        emit_import_func(&sec, "env", "__syscall_readlinkat", 0);
        emit_import_func(&sec, "env", "__syscall_rmdir", 1);
        emit_import_func(&sec, "env", "emscripten_get_heap_max", 12);
        emit_import_func(&sec, "env", "__syscall_unlinkat", 4);
        emit_import_func(&sec, "env", "__syscall_utimensat", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_fdstat_get", 3);
        emit_import_func(&sec, "env", "emscripten_resize_heap", 1);
        emit_import_memory(&sec, "env", "memory", 0, 1, 128u, 32768u);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_export_kind(&sec, "memory", 0x02, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    wasm_init(&rt);
    err = wasm_bind_wasi_stubs(&rt);
    WASM_CHECK_OK(t, err);
    err = wasm_bind_emscripten_stubs(&rt);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    memory = wasm_module_memory(m, 0);
    WL_CHECK_MSG(t, memory != NULL, "%s", "expected imported env.memory binding");
    if (memory) {
        WL_CHECK_MSG(t, memory->pages == 128u, "expected 128 initial pages, got %llu",
                     (unsigned long long)memory->pages);
        WL_CHECK_MSG(t, memory->max_pages == 32768u, "expected max_pages=32768, got %llu",
                     (unsigned long long)memory->max_pages);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}
#endif /* WASM_ENABLE_PLATFORM */

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

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_float_signaling_nan_bits) {
    static const uint8_t code[][16] = {
        { 0x43, 0x10, 0x32, 0xA0, 0xFF, 0xBC },
        { 0x43, 0x10, 0x32, 0xA0, 0xFF, 0x8B, 0xBC },
        { 0x43, 0x10, 0x32, 0xA0, 0x7F, 0x8C, 0xBC },
        { 0x43, 0x10, 0x32, 0xA0, 0xFF, 0xFD, 0x13, 0xFD, 0x1F, 0, 0xBC },
        { 0x43, 0x10, 0x32, 0xA0, 0xFF, 0xFD, 0x13, 0xFD, 0xE0, 1, 0xFD, 0x1F, 0, 0xBC }
    };
    static const uint32_t lengths[] = { 6, 7, 7, 11, 14 };
    static const uint32_t expected[] = { 0xFFA03210u, 0x7FA03210u, 0xFFA03210u, 0xFFA03210u, 0x7FA03210u };
    uint32_t i;
    for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
        wasm_builder_t mod = { 0 }, sec = { 0 }, body = { 0 };
        wasm_runtime_t rt;
        wasm_module_t* m;
        wasm_value_t result;
        wasm_error_t err;
        static const uint8_t type[] = { 1, 0x60, 0, 1, 0x7F };
        static const uint8_t funcs[] = { 1, 0 };
        emit_header(&mod);
        emit_section(&mod, 1, type, sizeof(type));
        emit_section(&mod, 3, funcs, sizeof(funcs));
        emit(&sec, 1);
        emit_export_func(&sec, "bits", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
        sec.len = 0;
        emit(&body, 0);
        emit_bytes(&body, code[i], lengths[i]);
        emit(&body, 0x0B);
        emit(&sec, 1);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
        wasm_init(&rt);
        wasm_runtime_enable_all_features(&rt);
        m = wasm_load(&rt, mod.buf, mod.len);
        WL_CHECK_MSG(t, m != NULL, "case %u: %s", i, rt.error_msg);
        if (m) {
            err = wasm_call(m, "bits", NULL, 0, &result, 1);
            WASM_CHECK_OK(t, err);
            if (err == WASM_OK) {
                WL_CHECK_MSG(t, (uint32_t)result.of.i32 == expected[i],
                             "case %u: signaling NaN bits changed", i);
            }
            wasm_module_free(m);
        }
        wasm_destroy(&rt);
    }
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    /* abs/neg preserve the minimum signed lane modulo its width. The dot
     * products fit individually, but their pairwise sum wraps at 32 bits. */
    {
        wasm_value_t input = wasm_v128_zero();
        wasm_value_t actual;
        uint32_t lane;
        for (lane = 0; lane < 16; lane++) wasm__v128_set_u8(&input, lane, 0x80u);
        actual = wasm__simd_i8x16_unary(0x60, &input);
        WL_CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i8x16.abs minimum");
        actual = wasm__simd_i8x16_unary(0x61, &input);
        WL_CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i8x16.neg minimum");
        for (lane = 0; lane < 8; lane++) wasm__v128_set_u16(&input, lane, 0x8000u);
        actual = wasm__simd_i16x8_unary(0x80, &input);
        WL_CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i16x8.abs minimum");
        actual = wasm__simd_i16x8_unary(0x81, &input);
        WL_CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i16x8.neg minimum");
        actual = wasm__simd_i32x4_binary(0xBA, &input, &input);
        for (lane = 0; lane < 4; lane++) {
            WL_CHECK_MSG(t, wasm__v128_get_u32(&actual, lane) == 0x80000000u, "%s", "i32x4.dot_i16x8_s wraps sum");
            wasm__v128_set_u32(&input, lane, 0x80000000u);
        }
        actual = wasm__simd_i32x4_unary(0xA0, &input);
        WL_CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i32x4.abs minimum");
        actual = wasm__simd_i32x4_unary(0xA1, &input);
        WL_CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i32x4.neg minimum");
        for (lane = 0; lane < 2; lane++) wasm__v128_set_u64(&input, lane, UINT64_C(0x8000000000000000));
        actual = wasm__simd_i64x2_unary(0xC0, &input);
        WL_CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i64x2.abs minimum");
        actual = wasm__simd_i64x2_unary(0xC1, &input);
        WL_CHECK_MSG(t, memcmp(actual.of.v128, input.of.v128, 16) == 0, "%s", "i64x2.neg minimum");
    }

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
                        expected[lane] = (int16_t)((uint32_t)ua * (uint32_t)ub);
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
        emit_leb128_u32(&sec, 3);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
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

    wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_init(&rt);
    wasm_runtime_disable_feature(&rt, WASM_FEATURE_SIMD);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected SIMD-disabled runtime to reject v128 module");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", "expected malformed error for disabled SIMD feature");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "SIMD") != NULL, "%s", "expected SIMD feature error message");
    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_memory_grow_respects_zero_max) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    int64_t old_pages;

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

    old_pages = wasm_memory_grow(m, 0, 0);
    WASM_CHECK_I64(t, old_pages, 0);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 0), 0);

    old_pages = wasm_memory_grow(m, 0, 1);
    WASM_CHECK_I64(t, old_pages, -1);
    WASM_CHECK_I32(t, (int32_t)wasm_memory_size(m, 0), 0);

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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
    wasm_runtime_disable_feature(&rt, WASM_FEATURE_GC);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected GC-disabled runtime to reject GC type section");
    WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "garbage collection") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

WL_TEST(test_decode_rejects_counts_beyond_section) {
    static const uint8_t type_count[] = { 0x01, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F };
    static const uint8_t rec_group_size[] = { 0x01, 0x07, 0x01, 0x4E, 0x80, 0x80, 0x80, 0x80, 0x04 };
    static const uint8_t param_count[] = { 0x01, 0x08, 0x01, 0x60, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00 };
    static const uint8_t supertype_count[] = { 0x01, 0x09, 0x01, 0x50, 0x81, 0x80, 0x80, 0x80, 0x04, 0x5F, 0x00 };
    static const uint8_t struct_field_count[] = { 0x01, 0x07, 0x01, 0x5F, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F };
    static const uint8_t func_count[] = { 0x03, 0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F };
    static const struct {
        const char* name;
        const uint8_t* section;
        size_t len;
    } cases[] = {
        { "type count", type_count, sizeof(type_count) },
        { "rec group size", rec_group_size, sizeof(rec_group_size) },
        { "param count", param_count, sizeof(param_count) },
        { "supertype count", supertype_count, sizeof(supertype_count) },
        { "struct field count", struct_field_count, sizeof(struct_field_count) },
        { "function count", func_count, sizeof(func_count) },
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        wasm_builder_t mod = { 0 };
        wasm_runtime_t rt;
        wasm_module_t* m;

        emit_header(&mod);
        emit_bytes(&mod, cases[i].section, (uint32_t)cases[i].len);

        wasm_init(&rt);
        m = wasm_load(&rt, mod.buf, mod.len);
        WL_CHECK_MSG(t, m == NULL, "expected oversized %s to be rejected", cases[i].name);
        WL_CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s: %s", cases[i].name, rt.error_msg);
        if (m != NULL) wasm_module_free(m);
        wasm_destroy(&rt);
    }
}

WL_TEST(test_gc_rejects_multiple_supertypes) {
    static const uint8_t types[] = {
        0x01, 0x0F, 0x03,
        0x50, 0x00, 0x5F, 0x00,
        0x50, 0x00, 0x5F, 0x00,
        0x50, 0x02, 0x00, 0x01, 0x5F, 0x00,
    };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);
    emit_bytes(&mod, types, sizeof(types));

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected multiple supertypes to be rejected");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "multiple supertypes") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_compact_imports_is_opt_in) {
    static const uint8_t import_section[] = { 0x02, 0x06, 0x01, 0x01, 'a', 0x00, 0x7F, 0x00 };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);
    emit_bytes(&mod, import_section, sizeof(import_section));

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected compact imports to be disabled by default");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "compact imports") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);

    wasm_runtime_enable_feature(&rt, WASM_FEATURE_COMPACT_IMPORTS);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_custom_page_sizes_is_opt_in) {
    static const uint8_t memory_section[] = { 0x05, 0x04, 0x01, 0x08, 0x03, 0x00 };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);
    emit_bytes(&mod, memory_section, sizeof(memory_section));

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected custom page sizes to be disabled by default");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "custom page sizes") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);

    wasm_runtime_enable_feature(&rt, WASM_FEATURE_CUSTOM_PAGE_SIZES);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        int64_t old_pages;
        WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), 3);
        old_pages = wasm_memory_grow(m, 0, 5);
        WASM_CHECK_I64(t, old_pages, 3);
        WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), 8);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

#if defined(WASM_TEST_THREADS)
/* Imports env.mem as (memory 1 1 shared) and exports
   add(n): n times i32.atomic.rmw.add at address 0,
   wait(): memory.atomic.wait32 at address 8 expecting 0 with no timeout,
   notify(): memory.atomic.notify at address 8 for one waiter. */
static void wasm_test_emit_shared_counter_module(wasm_builder_t* mod) {
    static const uint8_t sections[] = {
        0x01, 0x09, 0x02, 0x60, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x01, 0x7F,
        0x02, 0x0D, 0x01, 0x03, 'e', 'n', 'v', 0x03, 'm', 'e', 'm', 0x02, 0x03, 0x01, 0x01,
        0x03, 0x04, 0x03, 0x00, 0x01, 0x01,
        0x07, 0x17, 0x03,
        0x03, 'a', 'd', 'd', 0x00, 0x00,
        0x04, 'w', 'a', 'i', 't', 0x00, 0x01,
        0x06, 'n', 'o', 't', 'i', 'f', 'y', 0x00, 0x02,
        0x0A, 0x39, 0x03,
        0x1F, 0x00, 0x02, 0x40, 0x03, 0x40, 0x20, 0x00, 0x45, 0x0D, 0x01, 0x41, 0x00, 0x41, 0x01,
        0xFE, 0x1E, 0x02, 0x00, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x21, 0x00, 0x0C, 0x00, 0x0B, 0x0B, 0x0B,
        0x0C, 0x00, 0x41, 0x08, 0x41, 0x00, 0x42, 0x7F, 0xFE, 0x01, 0x02, 0x00, 0x0B,
        0x0A, 0x00, 0x41, 0x08, 0x41, 0x01, 0xFE, 0x00, 0x02, 0x00, 0x0B,
    };

    emit_header(mod);
    emit_bytes(mod, sections, sizeof(sections));
}

typedef struct wasm_test_shared_worker_t {
    wasm_runtime_t rt;
    int initialized;
    wasm_module_t* mod;
    const char* export_name;
    int32_t arg;
    wasm_error_t err;
    int32_t result;
} wasm_test_shared_worker_t;

static void wasm_test_shared_worker_run(void* arg) {
    wasm_test_shared_worker_t* worker = (wasm_test_shared_worker_t*)arg;
    wasm_value_t args[1];
    wasm_value_t results[1];

    args[0] = wasm_i32(worker->arg);
    results[0] = wasm_i32(0);
    if (strcmp(worker->export_name, "add") == 0) {
        worker->err = wasm_call(worker->mod, "add", args, 1, NULL, 0);
    } else {
        worker->err = wasm_call(worker->mod, worker->export_name, NULL, 0, results, 1);
        worker->result = results[0].of.i32;
    }
}

/* Several runtimes are live at once, so they skip the one-runtime leak checks; the case-level check remains. */
static wasm_module_t* wasm_test_load_shared_worker(wl_test_ctx* t, wasm_test_shared_worker_t* worker,
                                                  wasm_memory_t* memory, const wasm_builder_t* mod) {
    wasm_memory_import_t imp;

    imp.module = "env";
    imp.name = "mem";
    imp.memory = memory;
    WL_CHECK_MSG(t, wasm_test_init_raw(&worker->rt, NULL) == WASM_OK, "%s", worker->rt.error_msg);
    worker->initialized = 1;
    wasm_runtime_enable_feature(&worker->rt, WASM_FEATURE_THREADS);
    WL_CHECK_MSG(t, wasm_register_memory_import(&worker->rt, &imp) == WASM_OK, "%s", worker->rt.error_msg);
    worker->mod = wasm_load(&worker->rt, mod->buf, mod->len);
    WL_CHECK_MSG(t, worker->mod != NULL, "%s", worker->rt.error_msg);
    return worker->mod;
}

WL_TEST(test_threads_shared_memory_across_runtimes) {
    enum { WORKERS = 4, ITERATIONS = 20000 };
    wasm_builder_t mod = { 0 };
    wasm_test_shared_worker_t workers[WORKERS + 1];
    wasm_test_thread_start_t starts[WORKERS + 1];
    wasm_test_thread_t threads[WORKERS + 1];
    wasm_memory_t* memory = wasm_shared_memory_new(&wasm_test_allocator, 1, 1, 0);
    wasm_test_shared_worker_t* waiter = &workers[WORKERS];
    wasm_value_t results[1];
    uint32_t counter;
    int woken = 0;
    int i;

    WL_CHECK_MSG(t, memory != NULL, "%s", "expected shared memory allocation");
    if (!memory) return;
    memset(workers, 0, sizeof(workers));
    wasm_test_emit_shared_counter_module(&mod);
    for (i = 0; i <= WORKERS; i++) {
        if (!wasm_test_load_shared_worker(t, &workers[i], memory, &mod)) goto cleanup;
        workers[i].export_name = i < WORKERS ? "add" : "wait";
        workers[i].arg = ITERATIONS;
        starts[i].fn = wasm_test_shared_worker_run;
        starts[i].arg = &workers[i];
    }

    WL_CHECK(t, wasm_test_thread_spawn(&threads[WORKERS], &starts[WORKERS]));
    for (i = 0; i < WORKERS; i++) WL_CHECK(t, wasm_test_thread_spawn(&threads[i], &starts[i]));
    for (i = 0; i < WORKERS; i++) wasm_test_thread_join(threads[i]);

    /* The waiter may not have blocked yet, so keep notifying until it is woken. */
    while (!woken) {
        WL_CHECK(t, wasm_call(workers[0].mod, "notify", NULL, 0, results, 1) == WASM_OK);
        woken = results[0].of.i32 == 1;
    }
    wasm_test_thread_join(threads[WORKERS]);

    memcpy(&counter, memory->data, sizeof(counter));
    WASM_CHECK_I32(t, (int32_t)counter, WORKERS * ITERATIONS);
    for (i = 0; i < WORKERS; i++) WL_CHECK_MSG(t, workers[i].err == WASM_OK, "worker %d failed", i);
    WL_CHECK_MSG(t, waiter->err == WASM_OK, "%s", waiter->rt.error_msg);
    WASM_CHECK_I32(t, waiter->result, 0);

cleanup:
    for (i = 0; i <= WORKERS; i++) {
        if (workers[i].mod) wasm_module_free(workers[i].mod);
        if (workers[i].initialized) wasm__runtime_destroy(&workers[i].rt);
    }
    wasm_memory_free(memory);
}
#endif

WL_TEST(test_wide_arithmetic_is_opt_in) {
    static const uint8_t sections[] = {
        0x01, 0x08, 0x01, 0x60, 0x02, 0x7E, 0x7E, 0x02, 0x7E, 0x7E,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x07, 0x01, 0x03, 'm', 'u', 'l', 0x00, 0x00,
        0x0A, 0x0A, 0x01, 0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0xFC, 0x15, 0x0B,
    };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[2];
    wasm_value_t results[2];
    wasm_error_t err;

    emit_header(&mod);
    emit_bytes(&mod, sections, sizeof(sections));

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m == NULL, "%s", "expected wide arithmetic to be disabled by default");
    WL_CHECK_MSG(t, strstr(rt.error_msg, "wide arithmetic") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);

    wasm_runtime_enable_feature(&rt, WASM_FEATURE_WIDE_ARITHMETIC);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        args[0] = wasm_i64(-3);
        args[1] = wasm_i64(5);
        err = wasm_call(m, "mul", args, 2, results, 2);
        WL_CHECK_MSG(t, err == WASM_OK, "%s", rt.error_msg);
        WASM_CHECK_I64(t, results[0].of.i64, -15);
        WASM_CHECK_I64(t, results[1].of.i64, -1);

        args[0] = wasm_i64(INT64_MIN);
        args[1] = wasm_i64(INT64_MIN);
        err = wasm_call(m, "mul", args, 2, results, 2);
        WL_CHECK_MSG(t, err == WASM_OK, "%s", rt.error_msg);
        WASM_CHECK_I64(t, results[0].of.i64, 0);
        WASM_CHECK_I64(t, results[1].of.i64, INT64_C(0x4000000000000000));
        wasm_module_free(m);
    }
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
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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
        emit_leb128_i32(&sec, 9);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 3);
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

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_accessors_handle_packed_fields_and_array_bounds) {
    wasm_runtime_t rt;
    wasm_module_t mod;
    wasm_comptype_t types[2];
    wasm_fieldtype_t struct_fields[3];
    wasm_gc_struct_t* struct_obj;
    wasm_gc_array_t* array_obj;
    wasm_value_t value = { 0 };

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
    fake_func.num_local_slots = 1;
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

WL_TEST(test_gc_collect_marks_deep_object_chains_iteratively) {
    enum { CHAIN_LENGTH = 250000 };
    wasm_runtime_t rt;
    wasm_module_t mod;
    wasm_comptype_t types[1];
    wasm_fieldtype_t fields[1];
    wasm_global_t globals[1];
    wasm_value_t head;
    wasm_gc_header_t* object;
    size_t object_size = 0;
    uint32_t i;
    uint32_t length = 0;

    memset(&mod, 0, sizeof(mod));
    memset(types, 0, sizeof(types));
    memset(fields, 0, sizeof(fields));
    memset(globals, 0, sizeof(globals));

    wasm_init(&rt);

    fields[0].storage.kind = WASM_STORAGE_VALTYPE;
    fields[0].storage.of.valtype = WASM_TYPE_STRUCTREF;
    types[0].kind = WASM_COMP_STRUCT;
    types[0].of.struct_.num_fields = 1;
    types[0].of.struct_.fields = fields;

    mod.rt = &rt;
    mod.types = types;
    mod.num_types = 1;
    mod.globals = globals;
    mod.num_globals = 1;
    globals[0].type = WASM_TYPE_STRUCTREF;
    globals[0].value = wasm_ref_null(WASM_TYPE_STRUCTREF);
    wasm__gc_register_module(&rt, &mod);

    WL_CHECK_MSG(t, wasm__gc_struct_size(&types[0].of.struct_, &object_size), "%s",
                 "expected struct size calculation to succeed");
    WL_CHECK_MSG(t, wasm__gc_heap_init(&rt, object_size * (CHAIN_LENGTH + 16u)) == WASM_OK, "%s",
                 "expected GC heap resize to succeed");

    head = wasm_ref_null(WASM_TYPE_STRUCTREF);
    for (i = 0; i < CHAIN_LENGTH; i++) {
        wasm_gc_struct_t* node = wasm__gc_alloc_struct_object(&rt, &mod, 0, &types[0].of.struct_);

        if (!node || wasm__gc_struct_store_field(node, &types[0].of.struct_, 0, head) != WASM_OK) break;
        head = wasm__gc_type_ref_value(&mod, 0, &node->header);
    }
    WL_CHECK_MSG(t, i == CHAIN_LENGTH, "expected %u chain allocations, got %u", (unsigned)CHAIN_LENGTH,
                 (unsigned)i);
    globals[0].value = head;

    wasm_gc_collect(&rt);

    object = globals[0].value.of.gc_obj;
    while (object) {
        wasm_value_t* next = wasm_test_gc_struct_field_value((wasm_gc_struct_t*)object, &types[0].of.struct_, 0);

        length++;
        object = next && wasm__is_heap_gc_ref_value(next) ? next->of.gc_obj : NULL;
    }
    WL_CHECK_MSG(t, length == CHAIN_LENGTH, "expected the whole chain to survive, walked %u", (unsigned)length);
    WL_CHECK_MSG(t, wasm_test_gc_allocation_count(&rt) == CHAIN_LENGTH, "expected %u live objects, got %u",
                 (unsigned)CHAIN_LENGTH, (unsigned)wasm_test_gc_allocation_count(&rt));

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

static const uint8_t wasm_test_gc_growth_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x1d, 0x06, 0x5f, 0x02, 0x7e, 0x00, 0x7e, 0x00, 0x5e,
        0x63, 0x00, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x01, 0x63, 0x01, 0x60, 0x00, 0x01, 0x7f,
        0x60, 0x00, 0x00, 0x03, 0x05, 0x04, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x01, 0x63, 0x01, 0x01, 0xd0, 0x01,
        0x0b, 0x07, 0x1e, 0x04, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x00, 0x04, 0x66, 0x69, 0x6c, 0x6c, 0x00, 0x01, 0x05,
        0x63, 0x68, 0x65, 0x63, 0x6b, 0x00, 0x02, 0x05, 0x63, 0x6c, 0x65, 0x61, 0x72, 0x00, 0x03, 0x0a, 0xb9, 0x01,
        0x04, 0x31, 0x02, 0x01, 0x63, 0x01, 0x01, 0x7f, 0x20, 0x00, 0xfb, 0x07, 0x01, 0x21, 0x01, 0x03, 0x40, 0x20,
        0x01, 0x20, 0x02, 0x42, 0x01, 0x42, 0x02, 0xfb, 0x00, 0x00, 0xfb, 0x0e, 0x01, 0x20, 0x02, 0x41, 0x01, 0x6a,
        0x21, 0x02, 0x20, 0x02, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0xfb, 0x0f, 0x0b, 0x31, 0x01, 0x01,
        0x7f, 0x20, 0x00, 0xfb, 0x07, 0x01, 0x24, 0x00, 0x03, 0x40, 0x23, 0x00, 0x20, 0x01, 0x20, 0x01, 0xad, 0x20,
        0x01, 0xad, 0x42, 0x03, 0x7e, 0xfb, 0x00, 0x00, 0xfb, 0x0e, 0x01, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01,
        0x20, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x23, 0x00, 0x0b, 0x4c, 0x02, 0x02, 0x7f, 0x01, 0x63, 0x00,
        0x02, 0x40, 0x03, 0x40, 0x20, 0x00, 0x23, 0x00, 0xfb, 0x0f, 0x4f, 0x0d, 0x01, 0x23, 0x00, 0x20, 0x00, 0xfb,
        0x0b, 0x01, 0x21, 0x02, 0x20, 0x02, 0xfb, 0x02, 0x00, 0x00, 0x20, 0x00, 0xad, 0x51, 0x20, 0x02, 0xfb, 0x02,
        0x00, 0x01, 0x20, 0x00, 0xad, 0x42, 0x03, 0x7e, 0x51, 0x71, 0x04, 0x40, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21,
        0x01, 0x0b, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x21, 0x00, 0x0c, 0x00, 0x0b, 0x0b, 0x20, 0x01, 0x0b, 0x06, 0x00,
        0xd0, 0x01, 0x24, 0x00, 0x0b,
};

static wasm_module_t* wasm_test_gc_growth_load(wl_test_ctx* t, wasm_runtime_t* rt, const wasm_config_t* config) {
    wasm_module_t* m;

    wasm_init_config(rt, config);
    m = wasm_load(rt, wasm_test_gc_growth_bytes, sizeof(wasm_test_gc_growth_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt->error_msg);
    return m;
}

static int wasm_test_gc_held_array_intact(wasm_module_t* m, wasm_value_t array, uint32_t expected_len) {
    uint32_t len = 0;
    uint32_t i;

    if (wasm_array_length(m, array, &len) != WASM_OK || len != expected_len) return 0;
    for (i = 0; i < len; i++) {
        wasm_value_t elem;
        wasm_value_t first;
        wasm_value_t second;

        if (wasm_array_get_elem(m, array, i, &elem) != WASM_OK) return 0;
        if (wasm_struct_get_field(m, elem, 0, &first) != WASM_OK ||
            wasm_struct_get_field(m, elem, 1, &second) != WASM_OK)
            return 0;
        if (first.of.i64 != (int64_t)i || second.of.i64 != (int64_t)i * 3) return 0;
    }
    return 1;
}

WL_TEST(test_gc_heap_grows_with_live_data) {
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);
    wasm_value_t* held;

    wasm_config_default(&config);
    m = wasm_test_gc_growth_load(t, &rt, &config);
    if (m) {
        arg = wasm_i32(1000000);
        WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 1000000);
        WL_CHECK_MSG(t, rt.gc_heap_size > (size_t)WASM_GC_HEAP_SIZE, "expected the heap to grow, size %zu",
                     rt.gc_heap_size);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_config_default(&config);
    config.initial_gc_heap_size = 64u * 1024u;
    m = wasm_test_gc_growth_load(t, &rt, &config);
    if (m) {
        size_t heap_size;

        arg = wasm_i32(20000);
        WASM_CHECK_OK(t, wasm_call(m, "fill", &arg, 1, &result, 1));
        held = wasm_ref_hold(&rt, result);
        WL_CHECK(t, held != NULL);
        WASM_CHECK_OK(t, wasm_call(m, "check", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 20000);
        heap_size = rt.gc_heap_size;
        WL_CHECK_MSG(t, heap_size > config.initial_gc_heap_size, "expected the heap to grow, size %zu", heap_size);

        arg = wasm_i32(200000);
        WASM_CHECK_OK(t, wasm_call(m, "fill", &arg, 1, &result, 1));
        arg = wasm_i32(500000);
        WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, &result, 1));
        WL_CHECK_MSG(t, rt.gc_heap_size > heap_size, "expected the heap to grow again, size %zu", rt.gc_heap_size);
        WASM_CHECK_OK(t, wasm_call(m, "check", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 200000);
        if (held) {
            WL_CHECK(t, wasm_test_gc_held_array_intact(m, *held, 20000));
            wasm_ref_release(&rt, held);
        }

        WASM_CHECK_OK(t, wasm_call(m, "clear", NULL, 0, NULL, 0));
        wasm_gc_collect(&rt);
        WL_CHECK_MSG(t, rt.gc_heap_size == config.initial_gc_heap_size,
                     "expected an empty heap to shrink to its initial size, size %zu", rt.gc_heap_size);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_config_default(&config);
    config.initial_gc_heap_size = 0;
    m = wasm_test_gc_growth_load(t, &rt, &config);
    if (m) {
        arg = wasm_i32(1000);
        WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 1000);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_heap_growth_respects_max_gc_heap_bytes) {
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);
    wasm_value_t* held = NULL;
    wasm_error_t err;

    wasm_config_default(&config);
    WL_CHECK(t, config.max_gc_heap_bytes == 0u);
    config.max_gc_heap_bytes = 1024u * 1024u;
    m = wasm_test_gc_growth_load(t, &rt, &config);
    if (m) {
        WL_CHECK_MSG(t, rt.gc_heap_size == 1024u * 1024u, "expected the initial heap to be capped, size %zu",
                     rt.gc_heap_size);
        arg = wasm_i32(2000);
        WASM_CHECK_OK(t, wasm_call(m, "fill", &arg, 1, &result, 1));
        held = wasm_ref_hold(&rt, result);
        WL_CHECK(t, held != NULL);

        arg = wasm_i32(100000);
        err = wasm_call(m, "run", &arg, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_OOM, "expected WASM_ERR_OOM, got %s", wasm_error_string(err));
        WL_CHECK(t, wasm_runtime_last_error(&rt) == WASM_ERR_OOM);
        WL_CHECK_MSG(t, strstr(wasm_runtime_error_message(&rt), "limit") != NULL, "unexpected message '%s'",
                     wasm_runtime_error_message(&rt));
        WL_CHECK(t, rt.gc_heap_size <= 1024u * 1024u);

        WASM_CHECK_OK(t, wasm_call(m, "check", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 2000);
        if (held) WL_CHECK(t, wasm_test_gc_held_array_intact(m, *held, 2000));
        arg = wasm_i32(1000);
        WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 1000);

        arg = wasm_i32(1000000);
        err = wasm_call(m, "run", &arg, 1, &result, 1);
        WL_CHECK_MSG(t, err == WASM_ERR_OOM, "expected an array larger than the limit to fail, got %s",
                     wasm_error_string(err));
        if (held) {
            WL_CHECK(t, wasm_test_gc_held_array_intact(m, *held, 2000));
            wasm_ref_release(&rt, held);
        }
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_host_externrefs_round_trip_through_anyref) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x24, 0x08, 0x5f, 0x01, 0x7e, 0x00, 0x5e, 0x63, 0x00,
        0x01, 0x60, 0x01, 0x6f, 0x00, 0x60, 0x00, 0x00, 0x60, 0x02, 0x6f, 0x7f, 0x01, 0x6f, 0x60, 0x00, 0x01, 0x6f,
        0x60, 0x01, 0x6f, 0x01, 0x7f, 0x60, 0x01, 0x6f, 0x01, 0x6f, 0x03, 0x0b, 0x0a, 0x02, 0x03, 0x04, 0x05, 0x05,
        0x06, 0x07, 0x02, 0x02, 0x02, 0x04, 0x04, 0x01, 0x6e, 0x00, 0x01, 0x06, 0x06, 0x01, 0x6e, 0x01, 0xd0, 0x6e,
        0x0b, 0x07, 0x5f, 0x0a, 0x04, 0x6b, 0x65, 0x65, 0x70, 0x00, 0x00, 0x05, 0x63, 0x6c, 0x65, 0x61, 0x72, 0x00,
        0x01, 0x05, 0x63, 0x68, 0x75, 0x72, 0x6e, 0x00, 0x02, 0x06, 0x67, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x00, 0x03,
        0x05, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x04, 0x05, 0x74, 0x65, 0x73, 0x74, 0x73, 0x00, 0x05, 0x08, 0x63,
        0x61, 0x73, 0x74, 0x5f, 0x61, 0x6e, 0x79, 0x00, 0x06, 0x07, 0x63, 0x61, 0x73, 0x74, 0x5f, 0x65, 0x71, 0x00,
        0x07, 0x08, 0x63, 0x61, 0x73, 0x74, 0x5f, 0x69, 0x33, 0x31, 0x00, 0x08, 0x0b, 0x63, 0x61, 0x73, 0x74, 0x5f,
        0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x00, 0x09, 0x0a, 0xc1, 0x01, 0x0a, 0x10, 0x00, 0x20, 0x00, 0xfb, 0x1a,
        0x24, 0x00, 0x41, 0x00, 0x20, 0x00, 0xfb, 0x1a, 0x26, 0x00, 0x0b, 0x0c, 0x00, 0xd0, 0x6e, 0x24, 0x00, 0x41,
        0x00, 0xd0, 0x6e, 0x26, 0x00, 0x0b, 0x39, 0x02, 0x01, 0x6e, 0x01, 0x63, 0x01, 0x20, 0x00, 0xfb, 0x1a, 0x21,
        0x02, 0x20, 0x01, 0xfb, 0x07, 0x01, 0x21, 0x03, 0x03, 0x40, 0x20, 0x01, 0x41, 0x01, 0x6b, 0x21, 0x01, 0x20,
        0x03, 0x20, 0x01, 0x20, 0x01, 0xad, 0xfb, 0x00, 0x00, 0xfb, 0x0e, 0x01, 0x42, 0x00, 0xfb, 0x00, 0x00, 0x1a,
        0x20, 0x01, 0x0d, 0x00, 0x0b, 0x20, 0x02, 0xfb, 0x1b, 0x0b, 0x06, 0x00, 0x23, 0x00, 0xfb, 0x1b, 0x0b, 0x08,
        0x00, 0x41, 0x00, 0x25, 0x00, 0xfb, 0x1b, 0x0b, 0x2a, 0x01, 0x01, 0x6e, 0x20, 0x00, 0xfb, 0x1a, 0x21, 0x01,
        0x20, 0x01, 0xfb, 0x14, 0x6e, 0x20, 0x01, 0xfb, 0x14, 0x6d, 0x41, 0x01, 0x74, 0x72, 0x20, 0x01, 0xfb, 0x14,
        0x6c, 0x41, 0x02, 0x74, 0x20, 0x01, 0xfb, 0x14, 0x6b, 0x41, 0x03, 0x74, 0x72, 0x72, 0x0b, 0x0b, 0x00, 0x20,
        0x00, 0xfb, 0x1a, 0xfb, 0x16, 0x6e, 0xfb, 0x1b, 0x0b, 0x0a, 0x00, 0x20, 0x00, 0xfb, 0x1a, 0xfb, 0x16, 0x6d,
        0x1a, 0x0b, 0x0a, 0x00, 0x20, 0x00, 0xfb, 0x1a, 0xfb, 0x16, 0x6c, 0x1a, 0x0b, 0x0a, 0x00, 0x20, 0x00, 0xfb,
        0x1a, 0xfb, 0x16, 0x6b, 0x1a, 0x0b,
    };
    static int host_object;
    const uintptr_t handles[] = { (uintptr_t)1u, (uintptr_t)3u, (uintptr_t)0x1234u, (uintptr_t)0x1235u,
                                  (uintptr_t)&host_object, UINTPTR_MAX - 1u, UINTPTR_MAX };
    const char* failing_casts[] = { "cast_eq", "cast_i31", "cast_struct" };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t max_boxes = 0;
    size_t heap_size;
    size_t i;
    size_t j;

    wasm_config_default(&config);
    config.initial_gc_heap_size = 8192;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    for (i = 0; i < sizeof(handles) / sizeof(handles[0]); i++) {
        wasm_value_t args[2];
        wasm_value_t result = wasm_i32(0);

        args[0] = wasm_externref(handles[i]);
        args[1] = wasm_i32(2000 * (int32_t)(i + 1));
        WASM_CHECK_OK(t, wasm_call(m, "keep", args, 1, NULL, 0));
        heap_size = rt.gc_heap_size;
        WASM_CHECK_OK(t, wasm_call(m, "churn", args, 2, &result, 1));
        WL_CHECK_MSG(t, rt.gc_heap_size > heap_size, "expected the heap to grow, size %zu", rt.gc_heap_size);
        WL_CHECK(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == handles[i]);
        wasm_gc_collect(&rt);
        WASM_CHECK_OK(t, wasm_call(m, "global", NULL, 0, &result, 1));
        WL_CHECK(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == handles[i]);
        WASM_CHECK_OK(t, wasm_call(m, "table", NULL, 0, &result, 1));
        WL_CHECK(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == handles[i]);

        WASM_CHECK_OK(t, wasm_call(m, "tests", args, 1, &result, 1));
        WL_CHECK_MSG(t, result.of.i32 == 1, "value %p: expected only ref.test any to succeed, mask %d",
                     (void*)handles[i], (int)result.of.i32);
        WASM_CHECK_OK(t, wasm_call(m, "cast_any", args, 1, &result, 1));
        WL_CHECK(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == handles[i]);
        for (j = 0; j < sizeof(failing_casts) / sizeof(failing_casts[0]); j++)
            WL_CHECK_MSG(t, wasm_call(m, failing_casts[j], args, 1, NULL, 0) == WASM_ERR_TRAP, "%s of %p succeeded",
                         failing_casts[j], (void*)handles[i]);
    }

    for (i = 0; i < 20000; i++) {
        wasm_value_t arg = wasm_externref(handles[i % (sizeof(handles) / sizeof(handles[0]))]);
        wasm_value_t result = wasm_i32(0);

        if (wasm_call(m, "tests", &arg, 1, &result, 1) != WASM_OK || result.of.i32 != 1) break;
        if (rt.externref_boxes.count > max_boxes) max_boxes = rt.externref_boxes.count;
    }
    WL_CHECK_MSG(t, i == 20000, "conversion loop stopped at %u", (unsigned)i);
    WL_CHECK_MSG(t, max_boxes < 4096, "expected unreachable boxes to be freed, %u live", (unsigned)max_boxes);

    WASM_CHECK_OK(t, wasm_call(m, "clear", NULL, 0, NULL, 0));
    wasm_gc_collect(&rt);
    WL_CHECK_MSG(t, rt.externref_boxes.count == 0, "expected every box to be freed, %u live",
                 (unsigned)rt.externref_boxes.count);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

#define WASM_TEST_HEAP_ARENA_SIZE (128u * 1024u)

static int g_wasm_test_heap_arena_used[2];

static uint8_t* wasm_test_heap_arena(int index) {
    static uint8_t raw[2u * WASM_TEST_HEAP_ARENA_SIZE + 16u];
    uintptr_t base = ((uintptr_t)raw + 15u) & ~(uintptr_t)15u;

    return (uint8_t*)base + (size_t)index * WASM_TEST_HEAP_ARENA_SIZE;
}

static void* wasm_test_heap_alloc(void* ctx, size_t size, size_t align) {
    int i;

    if (align != 16u || size < 4096u || size > WASM_TEST_HEAP_ARENA_SIZE) return wasm_test_alloc(ctx, size, align);
    for (i = 0; i < 2; i++) {
        if (!g_wasm_test_heap_arena_used[i]) {
            g_wasm_test_heap_arena_used[i] = 1;
            return wasm_test_heap_arena(i);
        }
    }
    return NULL;
}

static void* wasm_test_heap_resize(void* ctx, void* p, size_t old_size, size_t new_size, size_t align) {
    if (p == wasm_test_heap_arena(0) || p == wasm_test_heap_arena(1)) return NULL;
    return wasm_test_resize(ctx, p, old_size, new_size, align);
}

static void wasm_test_heap_free(void* ctx, void* p, size_t size, size_t align) {
    int i;

    for (i = 0; i < 2; i++) {
        if (p == wasm_test_heap_arena(i)) {
            g_wasm_test_heap_arena_used[i] = 0;
            return;
        }
    }
    wasm_test_free(ctx, p, size, align);
}

WL_TEST(test_gc_host_refs_inside_a_later_heap_stay_host_values) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x27, 0x09, 0x5f, 0x01, 0x7e, 0x01, 0x60, 0x02, 0x6f,
        0x6e, 0x00, 0x60, 0x00, 0x01, 0x6f, 0x60, 0x00, 0x01, 0x6e, 0x60, 0x01, 0x6e, 0x01, 0x7f, 0x60, 0x00, 0x01,
        0x7f, 0x60, 0x01, 0x7e, 0x00, 0x60, 0x00, 0x01, 0x7e, 0x60, 0x01, 0x7f, 0x00, 0x03, 0x09, 0x08, 0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x06, 0x11, 0x03, 0x6e, 0x01, 0xd0, 0x6e, 0x0b, 0x6e, 0x01, 0xd0, 0x6e,
        0x0b, 0x63, 0x00, 0x01, 0xd0, 0x00, 0x0b, 0x07, 0x31, 0x07, 0x04, 0x6b, 0x65, 0x65, 0x70, 0x00, 0x00, 0x03,
        0x65, 0x78, 0x74, 0x00, 0x01, 0x03, 0x61, 0x6e, 0x79, 0x00, 0x02, 0x05, 0x74, 0x65, 0x73, 0x74, 0x73, 0x00,
        0x04, 0x03, 0x70, 0x69, 0x6e, 0x00, 0x05, 0x04, 0x6c, 0x69, 0x76, 0x65, 0x00, 0x06, 0x05, 0x63, 0x68, 0x75,
        0x72, 0x6e, 0x00, 0x07, 0x0a, 0x75, 0x08, 0x0c, 0x00, 0x20, 0x00, 0xfb, 0x1a, 0x24, 0x00, 0x20, 0x01, 0x24,
        0x01, 0x0b, 0x06, 0x00, 0x23, 0x00, 0xfb, 0x1b, 0x0b, 0x04, 0x00, 0x23, 0x01, 0x0b, 0x22, 0x00, 0x20, 0x00,
        0xfb, 0x15, 0x6e, 0x20, 0x00, 0xfb, 0x15, 0x6d, 0x41, 0x01, 0x74, 0x72, 0x20, 0x00, 0xfb, 0x15, 0x6b, 0x41,
        0x02, 0x74, 0x20, 0x00, 0xfb, 0x15, 0x6c, 0x41, 0x03, 0x74, 0x72, 0x72, 0x0b, 0x0e, 0x00, 0x23, 0x00, 0x10,
        0x03, 0x23, 0x01, 0x10, 0x03, 0x41, 0x04, 0x74, 0x72, 0x0b, 0x09, 0x00, 0x20, 0x00, 0xfb, 0x00, 0x00, 0x24,
        0x02, 0x0b, 0x08, 0x00, 0x23, 0x02, 0xfb, 0x02, 0x00, 0x00, 0x0b, 0x15, 0x00, 0x03, 0x40, 0x20, 0x00, 0xad,
        0xfb, 0x00, 0x00, 0x1a, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x22, 0x00, 0x0d, 0x00, 0x0b, 0x0b,
    };
    static const wasm_allocator_t allocator = { NULL, wasm_test_heap_alloc, wasm_test_heap_resize,
                                                wasm_test_heap_free };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m = NULL;
    wasm_value_t args[2];
    wasm_value_t result;
    uintptr_t ext_bits;
    uintptr_t any_bits;
    int round;

    wasm_config_default(&config);
    config.initial_gc_heap_size = 8192;
    config.max_gc_heap_bytes = WASM_TEST_HEAP_ARENA_SIZE;
    WASM_CHECK_OK(t, wasm__runtime_init(&rt, &allocator, &config));
    WL_CHECK(t, rt.gc_heap == wasm_test_heap_arena(0));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        args[0] = wasm_i64(7);
        WASM_CHECK_OK(t, wasm_call(m, "pin", args, 1, NULL, 0));
        ext_bits = (uintptr_t)wasm_test_heap_arena(1);
        any_bits = ext_bits + 64u;
        args[0] = wasm_externref(ext_bits);
        args[1] = wasm_externref(any_bits);
        args[1].type = WASM_TYPE_ANYREF;
        WASM_CHECK_OK(t, wasm_call(m, "keep", args, 2, NULL, 0));
        for (round = 0; round < 6; round++) {
            if (round % 2 == 0) {
                wasm_gc_collect(&rt);
            } else {
                args[0] = wasm_i32(3000);
                WASM_CHECK_OK(t, wasm_call(m, "churn", args, 1, NULL, 0));
            }
            WL_CHECK(t, rt.gc_heap == wasm_test_heap_arena(0) || rt.gc_heap == wasm_test_heap_arena(1));
            WASM_CHECK_OK(t, wasm_call(m, "ext", NULL, 0, &result, 1));
            WL_CHECK_MSG(t, result.type == WASM_TYPE_EXTERNREF && result.of.externref == ext_bits,
                         "round %d: externref came back as %p", round, (void*)result.of.externref);
            WASM_CHECK_OK(t, wasm_call(m, "any", NULL, 0, &result, 1));
            WL_CHECK_MSG(t, result.type == WASM_TYPE_ANYREF && result.of.gc_ref == any_bits,
                         "round %d: anyref came back as %p", round, (void*)result.of.gc_ref);
            WASM_CHECK_OK(t, wasm_call(m, "tests", NULL, 0, &result, 1));
            WL_CHECK_MSG(t, result.of.i32 == 0x11, "round %d: expected only ref.test any to succeed, mask 0x%x",
                         round, (unsigned)result.of.i32);
            WASM_CHECK_OK(t, wasm_call(m, "live", NULL, 0, &result, 1));
            WASM_CHECK_I64(t, result.of.i64, 7);
        }
    }
    wasm_module_free(m);
    wasm__runtime_destroy(&rt);
    WL_CHECK(t, !g_wasm_test_heap_arena_used[0] && !g_wasm_test_heap_arena_used[1]);
}

static const uint8_t wasm_test_init_pair_150000_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0x04, 0x5e, 0x7e, 0x01, 0x5f, 0x02, 0x63, 0x00,
        0x00, 0x63, 0x00, 0x00, 0x60, 0x01, 0x63, 0x00, 0x01, 0x7e, 0x60, 0x00, 0x01, 0x7e, 0x03, 0x04, 0x03, 0x02,
        0x03, 0x03, 0x06, 0x1a, 0x01, 0x63, 0x01, 0x00, 0x42, 0x07, 0x41, 0xf0, 0x93, 0x09, 0xfb, 0x06, 0x00, 0x42,
        0x09, 0x41, 0xf0, 0x93, 0x09, 0xfb, 0x06, 0x00, 0xfb, 0x00, 0x01, 0x0b, 0x07, 0x13, 0x02, 0x06, 0x66, 0x69,
        0x65, 0x6c, 0x64, 0x30, 0x00, 0x01, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x31, 0x00, 0x02, 0x0a, 0x26, 0x03,
        0x0e, 0x00, 0x20, 0x00, 0x20, 0x00, 0xfb, 0x0f, 0x41, 0x01, 0x6b, 0xfb, 0x0b, 0x00, 0x0b, 0x0a, 0x00, 0x23,
        0x00, 0xfb, 0x02, 0x01, 0x00, 0x10, 0x00, 0x0b, 0x0a, 0x00, 0x23, 0x00, 0xfb, 0x02, 0x01, 0x01, 0x10, 0x00,
        0x0b,
};

static const uint8_t wasm_test_init_pair_300000_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0x04, 0x5e, 0x7e, 0x01, 0x5f, 0x02, 0x63, 0x00,
        0x00, 0x63, 0x00, 0x00, 0x60, 0x01, 0x63, 0x00, 0x01, 0x7e, 0x60, 0x00, 0x01, 0x7e, 0x03, 0x04, 0x03, 0x02,
        0x03, 0x03, 0x06, 0x1a, 0x01, 0x63, 0x01, 0x00, 0x42, 0x07, 0x41, 0xe0, 0xa7, 0x12, 0xfb, 0x06, 0x00, 0x42,
        0x09, 0x41, 0xe0, 0xa7, 0x12, 0xfb, 0x06, 0x00, 0xfb, 0x00, 0x01, 0x0b, 0x07, 0x13, 0x02, 0x06, 0x66, 0x69,
        0x65, 0x6c, 0x64, 0x30, 0x00, 0x01, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x31, 0x00, 0x02, 0x0a, 0x26, 0x03,
        0x0e, 0x00, 0x20, 0x00, 0x20, 0x00, 0xfb, 0x0f, 0x41, 0x01, 0x6b, 0xfb, 0x0b, 0x00, 0x0b, 0x0a, 0x00, 0x23,
        0x00, 0xfb, 0x02, 0x01, 0x00, 0x10, 0x00, 0x0b, 0x0a, 0x00, 0x23, 0x00, 0xfb, 0x02, 0x01, 0x01, 0x10, 0x00,
        0x0b,
};

static const uint8_t wasm_test_init_sequence_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x1f, 0x06, 0x5e, 0x7e, 0x01, 0x5f, 0x02, 0x63, 0x00,
        0x00, 0x63, 0x00, 0x00, 0x5e, 0x63, 0x00, 0x00, 0x60, 0x01, 0x63, 0x00, 0x01, 0x7e, 0x60, 0x00, 0x01, 0x7e,
        0x60, 0x01, 0x7f, 0x01, 0x7e, 0x03, 0x0a, 0x09, 0x03, 0x04, 0x04, 0x04, 0x04, 0x05, 0x04, 0x05, 0x05, 0x04,
        0x18, 0x03, 0x40, 0x00, 0x63, 0x00, 0x00, 0x01, 0x42, 0x05, 0x41, 0xc0, 0x00, 0xfb, 0x06, 0x00, 0x0b, 0x63,
        0x00, 0x00, 0x02, 0x63, 0x00, 0x00, 0x02, 0x06, 0x4f, 0x04, 0x63, 0x02, 0x00, 0x42, 0x17, 0x41, 0x80, 0x01,
        0xfb, 0x06, 0x00, 0x41, 0x80, 0x02, 0xfb, 0x06, 0x02, 0x0b, 0x63, 0x00, 0x00, 0x42, 0x03, 0x41, 0x80, 0x04,
        0xfb, 0x06, 0x00, 0x0b, 0x63, 0x01, 0x00, 0x42, 0x07, 0x41, 0x80, 0x08, 0xfb, 0x06, 0x00, 0x42, 0x09, 0x41,
        0x80, 0x10, 0xfb, 0x06, 0x00, 0xfb, 0x00, 0x01, 0x0b, 0x63, 0x02, 0x00, 0x42, 0x13, 0x41, 0x80, 0x20, 0xfb,
        0x06, 0x00, 0x42, 0x15, 0x41, 0x80, 0xc0, 0x00, 0xfb, 0x06, 0x00, 0xfb, 0x08, 0x02, 0x02, 0x0b, 0x07, 0x4c,
        0x08, 0x08, 0x72, 0x65, 0x70, 0x65, 0x61, 0x74, 0x65, 0x64, 0x00, 0x01, 0x06, 0x67, 0x6c, 0x6f, 0x62, 0x61,
        0x6c, 0x00, 0x02, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64, 0x30, 0x00, 0x03, 0x06, 0x66, 0x69, 0x65, 0x6c, 0x64,
        0x31, 0x00, 0x04, 0x05, 0x66, 0x69, 0x78, 0x65, 0x64, 0x00, 0x05, 0x07, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c,
        0x74, 0x00, 0x06, 0x06, 0x61, 0x63, 0x74, 0x69, 0x76, 0x65, 0x00, 0x07, 0x07, 0x70, 0x61, 0x73, 0x73, 0x69,
        0x76, 0x65, 0x00, 0x08, 0x09, 0x35, 0x02, 0x05, 0x63, 0x00, 0x02, 0x42, 0x0b, 0x41, 0x80, 0x80, 0x01, 0xfb,
        0x06, 0x00, 0x0b, 0x42, 0x0d, 0x41, 0x80, 0x80, 0x02, 0xfb, 0x06, 0x00, 0x0b, 0x06, 0x01, 0x41, 0x00, 0x0b,
        0x63, 0x00, 0x02, 0x42, 0x0f, 0x41, 0x80, 0x80, 0x04, 0xfb, 0x06, 0x00, 0x0b, 0x42, 0x11, 0x41, 0x80, 0x80,
        0x08, 0xfb, 0x06, 0x00, 0x0b, 0x0a, 0x6f, 0x09, 0x0e, 0x00, 0x20, 0x00, 0x20, 0x00, 0xfb, 0x0f, 0x41, 0x01,
        0x6b, 0xfb, 0x0b, 0x00, 0x0b, 0x10, 0x00, 0x23, 0x00, 0x23, 0x00, 0xfb, 0x0f, 0x41, 0x01, 0x6b, 0xfb, 0x0b,
        0x02, 0x10, 0x00, 0x0b, 0x06, 0x00, 0x23, 0x01, 0x10, 0x00, 0x0b, 0x0a, 0x00, 0x23, 0x02, 0xfb, 0x02, 0x01,
        0x00, 0x10, 0x00, 0x0b, 0x0a, 0x00, 0x23, 0x02, 0xfb, 0x02, 0x01, 0x01, 0x10, 0x00, 0x0b, 0x0b, 0x00, 0x23,
        0x03, 0x20, 0x00, 0xfb, 0x0b, 0x02, 0x10, 0x00, 0x0b, 0x08, 0x00, 0x41, 0x00, 0x25, 0x00, 0x10, 0x00, 0x0b,
        0x08, 0x00, 0x20, 0x00, 0x25, 0x01, 0x10, 0x00, 0x0b, 0x12, 0x00, 0x41, 0x00, 0x41, 0x00, 0x41, 0x02, 0xfc,
        0x0c, 0x00, 0x02, 0x20, 0x00, 0x25, 0x02, 0x10, 0x00, 0x0b,
};

static int64_t wasm_test_call_i64(wl_test_ctx* t, wasm_module_t* m, const char* name, int32_t arg, int has_arg) {
    wasm_value_t value = wasm_i32(arg);
    wasm_value_t result = wasm_i64(-1);
    wasm_error_t err = wasm_call(m, name, &value, has_arg ? 1u : 0u, &result, 1);

    WL_CHECK_MSG(t, err == WASM_OK, "%s: %s", name, wasm_runtime_error_message(wasm_module_runtime(m)));
    return result.of.i64;
}

WL_TEST(test_gc_init_expressions_keep_their_operands_alive) {
    const uint8_t* pairs[] = { wasm_test_init_pair_150000_bytes, wasm_test_init_pair_300000_bytes };
    const size_t pair_sizes[] = { sizeof(wasm_test_init_pair_150000_bytes), sizeof(wasm_test_init_pair_300000_bytes) };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    size_t i;

    for (i = 0; i < 2; i++) {
        wasm_init(&rt);
        m = wasm_load(&rt, pairs[i], pair_sizes[i]);
        WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
        if (m) {
            WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "field0", 0, 0), 7);
            WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "field1", 0, 0), 9);
        }
        wasm_module_free(m);
        wasm_destroy(&rt);
    }

    wasm_config_default(&config);
    config.initial_gc_heap_size = 0;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, wasm_test_init_sequence_bytes, sizeof(wasm_test_init_sequence_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "default", 0, 0), 5);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "repeated", 0, 0), 23);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "global", 0, 0), 3);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "field0", 0, 0), 7);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "field1", 0, 0), 9);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "fixed", 0, 1), 19);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "fixed", 1, 1), 21);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "passive", 0, 1), 11);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "passive", 1, 1), 13);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "active", 0, 1), 15);
        WASM_CHECK_I64(t, wasm_test_call_i64(t, m, "active", 1, 1), 17);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static int wasm_test_gc_heap_fits(const wasm_runtime_t* rt, size_t size) {
    size_t offset = wasm__align_up_size(rt->gc_heap_offset, (size_t)WASM__ALIGNOF(wasm_value_t));

    return offset <= rt->gc_heap_size && size <= rt->gc_heap_size - offset;
}

static int wasm_test_big_array_intact(wasm_module_t* m, wasm_value_t array, uint32_t expected_len) {
    uint32_t len = 0;
    uint32_t i;

    if (wasm_array_length(m, array, &len) != WASM_OK || len != expected_len) return 0;
    for (i = 0; i < len; i++) {
        wasm_value_t elem;

        if (wasm_array_get_elem(m, array, i, &elem) != WASM_OK || elem.of.i64 != (int64_t)i * 7) return 0;
    }
    return 1;
}

WL_TEST(test_gc_host_constructors_keep_their_inputs_alive) {
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0d, 0x03, 0x5e, 0x7e, 0x01, 0x5f, 0x01, 0x63, 0x00,
        0x00, 0x5e, 0x63, 0x00, 0x00,
    };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t elems[16];
    wasm_value_t null_ref = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t array = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t outer = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t garbage;
    wasm_value_t field;
    size_t struct_size = 0;
    size_t pair_size = 0;
    uint32_t i;

    wasm_config_default(&config);
    config.initial_gc_heap_size = 4096;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    WL_CHECK(t, wasm__gc_struct_size(wasm__module_const_structtype(m, 1), &struct_size));
    WL_CHECK(t, wasm__gc_array_total_size(wasm__module_const_arraytype(m, 2), 1, &pair_size));
    for (i = 0; i < 16; i++) elems[i] = wasm_i64((int64_t)i * 7);

    WASM_CHECK_OK(t, wasm_array_new(m, 0, elems, 16, &array));
    while (wasm_test_gc_heap_fits(&rt, struct_size)) WASM_CHECK_OK(t, wasm_struct_new(m, 1, &null_ref, 1, &garbage));
    WASM_CHECK_OK(t, wasm_struct_new(m, 1, &array, 1, &outer));
    WASM_CHECK_OK(t, wasm_struct_get_field(m, outer, 0, &field));
    WL_CHECK(t, field.of.gc_obj != array.of.gc_obj);
    WL_CHECK(t, wasm_test_big_array_intact(m, field, 16));

    WASM_CHECK_OK(t, wasm_array_new(m, 0, elems, 16, &array));
    while (wasm_test_gc_heap_fits(&rt, pair_size)) WASM_CHECK_OK(t, wasm_array_new(m, 2, &null_ref, 1, &garbage));
    WASM_CHECK_OK(t, wasm_array_new(m, 2, &array, 1, &outer));
    WASM_CHECK_OK(t, wasm_array_get_elem(m, outer, 0, &field));
    WL_CHECK(t, field.of.gc_obj != array.of.gc_obj);
    WL_CHECK(t, wasm_test_big_array_intact(m, field, 16));

    wasm_module_free(m);
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
        emit_leb128_i32(&body, 4);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 2);
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

    wasm_module_free(m);
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

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD4);
        emit(&body, 0xD1);
        emit(&body, 0x45);
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
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 7);

    wasm_module_free(m);
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
        emit_section(&mod, 12, sec.buf, sec.len);
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

    {
        static const uint8_t data_bytes[] = { 0x80, 0x7F, 0x05 };
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, (uint32_t)sizeof(data_bytes));
        emit_bytes(&sec, data_bytes, (uint32_t)sizeof(data_bytes));
        emit_section(&mod, 11, sec.buf, sec.len);
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

    wasm_module_free(m);
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
        emit_leb128_i32(&body, 9);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
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
    if (m != NULL) wasm_module_free(m);
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

        emit(&body, 0xD0);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xD4);
        emit(&body, 0x1A);

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

    wasm_module_free(m);
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

static wasm_error_t wasm_test_host_set_payload_only(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                                    wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    results[0].of.i32 = 7;
    return WASM_OK;
}

static wasm_error_t wasm_test_host_return_i32(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    results[0] = wasm_i32(0x1234);
    return WASM_OK;
}

WL_TEST(test_host_results_must_match_the_import_type) {
    /* get() returns env.get(), declared () -> i32; ref() returns ref.is_null(env.ref()), declared () -> anyref. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x00, 0x01,
        0x6e, 0x02, 0x15, 0x02, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x67, 0x65, 0x74, 0x00, 0x00, 0x03, 0x65, 0x6e, 0x76,
        0x03, 0x72, 0x65, 0x66, 0x00, 0x01, 0x03, 0x03, 0x02, 0x00, 0x00, 0x07, 0x0d, 0x02, 0x03, 0x67, 0x65, 0x74,
        0x00, 0x02, 0x03, 0x72, 0x65, 0x66, 0x00, 0x03, 0x0a, 0x0c, 0x02, 0x04, 0x00, 0x10, 0x00, 0x0b, 0x05, 0x00,
        0x10, 0x01, 0xd1, 0x0b,
    };
    wasm_valtype_t anyref_result = WASM_TYPE_ANYREF;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_import_t imp;
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "get", "(i)", wasm_test_host_set_payload_only, NULL));
    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "ref";
    imp.type.num_results = 1;
    imp.type.results = &anyref_result;
    imp.func = wasm_test_host_return_i32;
    WASM_CHECK_OK(t, wasm_register_import(&rt, &imp));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_OK(t, wasm_call(m, "get", NULL, 0, &result, 1));
    WL_CHECK(t, result.type == WASM_TYPE_I32 && result.of.i32 == 7);
    WL_CHECK(t, wasm_call(m, "ref", NULL, 0, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "host function result 0") != NULL, "%s", rt.error_msg);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_host_grow_memory(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    return wasm_memory_grow(mod, 0, 1) == 2 ? WASM_OK : WASM_ERR_TRAP;
}

WL_TEST(test_memory_grown_by_callee_or_host_is_visible_to_caller) {
    /* wasm() and host() store 1 at address 0, grow memory through a Wasm callee or env.grow, store
       42 or 43 in the new page, and return it plus the value at 0; past_end() loads across the end
       of the third page. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60, 0x00, 0x00, 0x60, 0x00, 0x01, 0x7f,
        0x02, 0x0c, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x67, 0x72, 0x6f, 0x77, 0x00, 0x00, 0x03, 0x05, 0x04, 0x00,
        0x01, 0x01, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x1a, 0x03, 0x04, 0x77, 0x61, 0x73, 0x6d, 0x00, 0x02,
        0x04, 0x68, 0x6f, 0x73, 0x74, 0x00, 0x03, 0x08, 0x70, 0x61, 0x73, 0x74, 0x5f, 0x65, 0x6e, 0x64, 0x00, 0x04,
        0x0a, 0x57, 0x04, 0x07, 0x00, 0x41, 0x01, 0x40, 0x00, 0x1a, 0x0b, 0x21, 0x00, 0x41, 0x00, 0x41, 0x01, 0x36,
        0x02, 0x00, 0x10, 0x01, 0x41, 0x80, 0x80, 0x04, 0x41, 0x2a, 0x36, 0x02, 0x00, 0x41, 0x80, 0x80, 0x04, 0x28,
        0x02, 0x00, 0x41, 0x00, 0x28, 0x02, 0x00, 0x6a, 0x0b, 0x21, 0x00, 0x41, 0x00, 0x41, 0x01, 0x36, 0x02, 0x00,
        0x10, 0x00, 0x41, 0x80, 0x80, 0x08, 0x41, 0x2b, 0x36, 0x02, 0x00, 0x41, 0x80, 0x80, 0x08, 0x28, 0x02, 0x00,
        0x41, 0x00, 0x28, 0x02, 0x00, 0x6a, 0x0b, 0x09, 0x00, 0x41, 0xfd, 0xff, 0x0b, 0x28, 0x02, 0x00, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "env", "grow", "()", wasm_test_host_grow_memory, NULL));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_OK(t, wasm_call(m, "wasm", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 43);
    WASM_CHECK_OK(t, wasm_call(m, "host", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 44);
    WL_CHECK(t, wasm_call(m, "past_end", NULL, 0, &result, 1) == WASM_ERR_OUT_OF_BOUNDS);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_bulk_fuel_and_array_new_rooting) {
    /* fill(n) runs memory.fill over n bytes; churn(n) builds n arrays of 64 copies of a new
     * struct holding the loop counter and sums each array's last element's field. A struct kept
     * alive across the loop makes every collection copy the heap and free the old one. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x12, 0x04, 0x5f, 0x01, 0x7f, 0x00, 0x5e, 0x63, 0x00,
        0x00, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x03, 0x03, 0x02, 0x02, 0x03, 0x05, 0x03, 0x01,
        0x00, 0x01, 0x07, 0x10, 0x02, 0x04, 0x66, 0x69, 0x6c, 0x6c, 0x00, 0x00, 0x05, 0x63, 0x68, 0x75, 0x72, 0x6e,
        0x00, 0x01, 0x0a, 0x4a, 0x02, 0x0b, 0x00, 0x41, 0x00, 0x41, 0x00, 0x20, 0x00, 0xfc, 0x0b, 0x00, 0x0b, 0x3c,
        0x02, 0x01, 0x7f, 0x01, 0x63, 0x00, 0x41, 0x00, 0xfb, 0x00, 0x00, 0x21, 0x02, 0x03, 0x40, 0x20, 0x01, 0x20,
        0x00, 0xfb, 0x00, 0x00, 0x41, 0xc0, 0x00, 0xfb, 0x06, 0x01, 0x41, 0x3f, 0xfb, 0x0b, 0x01, 0xfb, 0x02, 0x00,
        0x00, 0x6a, 0x21, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x22, 0x00, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0x20, 0x02,
        0xfb, 0x02, 0x00, 0x00, 0x6a, 0x0b,
    };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);

    wasm_config_default(&config);
    config.initial_gc_heap_size = 8192;
    wasm_init_config(&rt, &config);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    wasm_runtime_set_fuel(&rt, 100);
    arg = wasm_i32(64 * 50);
    WASM_CHECK_OK(t, wasm_call(m, "fill", &arg, 1, NULL, 0));
    WL_CHECK_MSG(t, wasm_runtime_fuel(&rt) < 50u, "expected 3200 bytes to cost 50 units, %u left",
                 (unsigned)wasm_runtime_fuel(&rt));
    wasm_runtime_set_fuel(&rt, 100);
    arg = wasm_i32(65536);
    WL_CHECK(t, wasm_call(m, "fill", &arg, 1, NULL, 0) == WASM_ERR_OUT_OF_FUEL);
    wasm_runtime_cancel(&rt);
    wasm_runtime_disable_fuel(&rt);

    /* A held struct survives the collections below and follows the object as it moves. */
    {
        wasm_value_t field = wasm_i32(99);
        wasm_value_t point;
        wasm_value_t original;
        wasm_value_t* held;

        WASM_CHECK_OK(t, wasm_struct_new(m, 0, &field, 1, &point));
        original = point;
        held = wasm_ref_hold(&rt, point);
        WL_CHECK(t, held != NULL);

        /* Each array needs about 1.5 KB, so the heap is collected every few iterations. */
        arg = wasm_i32(200);
        WASM_CHECK_OK(t, wasm_call(m, "churn", &arg, 1, &result, 1));
        WL_CHECK_MSG(t, result.of.i32 == 200 * 201 / 2, "expected each array to hold its struct, got %d",
                     (int)result.of.i32);

        if (held) {
            WL_CHECK(t, held->of.gc_obj != original.of.gc_obj);
            WASM_CHECK_OK(t, wasm_struct_get_field(m, *held, 0, &field));
            WL_CHECK(t, field.of.i32 == 99);
            wasm_ref_release(&rt, held);
        }
        wasm_ref_release(&rt, NULL);
        WL_CHECK(t, wasm_ref_hold(NULL, point) == NULL);
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

#if defined(WASM_TEST_THREADS)
static void wasm_test_interrupt_later(void* arg) {
#if defined(_WIN32)
    Sleep(20);
#else
    struct timespec delay = { 0, 20000000L };

    nanosleep(&delay, NULL);
#endif
    wasm_runtime_interrupt((wasm_runtime_t*)arg);
}
#endif

WL_TEST(test_runtime_interrupt_stops_loops_calls_and_waits) {
    /* spin() loops forever, call() returns a callee's 7, and wait() waits forever on shared memory. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x00, 0x00,
        0x03, 0x05, 0x04, 0x00, 0x01, 0x00, 0x00, 0x05, 0x04, 0x01, 0x03, 0x01, 0x01, 0x07, 0x16, 0x03, 0x04, 0x73,
        0x70, 0x69, 0x6e, 0x00, 0x01, 0x04, 0x63, 0x61, 0x6c, 0x6c, 0x00, 0x02, 0x04, 0x77, 0x61, 0x69, 0x74, 0x00,
        0x03, 0x0a, 0x20, 0x04, 0x04, 0x00, 0x41, 0x07, 0x0b, 0x07, 0x00, 0x03, 0x40, 0x0c, 0x00, 0x0b, 0x0b, 0x04,
        0x00, 0x10, 0x00, 0x0b, 0x0c, 0x00, 0x41, 0x00, 0x41, 0x00, 0x42, 0x7f, 0xfe, 0x01, 0x02, 0x00, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    wasm_runtime_enable_feature(&rt, WASM_FEATURE_THREADS);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    wasm_runtime_interrupt(&rt);
    WL_CHECK(t, wasm_call(m, "call", NULL, 0, &result, 1) == WASM_ERR_INTERRUPTED);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "interrupted") != NULL, "%s", rt.error_msg);
    WASM_CHECK_OK(t, wasm_call(m, "call", NULL, 0, &result, 1));
    WL_CHECK(t, result.of.i32 == 7);

#if defined(WASM_TEST_THREADS)
    {
        static const char* const exports[] = { "spin", "wait" };
        uint32_t i;

        for (i = 0; i < 2u; i++) {
            wasm_test_thread_start_t start;
            wasm_test_thread_t thread;

            start.fn = wasm_test_interrupt_later;
            start.arg = &rt;
            WL_CHECK(t, wasm_test_thread_spawn(&thread, &start));
            WL_CHECK_MSG(t, wasm_call(m, exports[i], NULL, 0, &result, i) == WASM_ERR_INTERRUPTED,
                         "expected %s to be interrupted: %s", exports[i], rt.error_msg);
            wasm_test_thread_join(thread);
            wasm_runtime_cancel(&rt);
        }
    }
#endif

    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* State shared by the host imports of the resume test module. */
typedef struct wasm_test_resume_host_t {
    wasm_module_t* mod;
    const char* reenter_export; /* export "reenter" calls with its argument */
    uint64_t reenter_fuel;      /* fuel set before the inner call; 0 leaves fuel unchanged */
    uint32_t yields;
    int32_t last_arg;
} wasm_test_resume_host_t;

static wasm_error_t wasm_test_resume_yield(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_test_resume_host_t* host = (wasm_test_resume_host_t*)userdata;

    (void)mod;
    (void)num_args;
    (void)results;
    (void)num_results;
    host->yields++;
    host->last_arg = args[0].of.i32;
    return WASM_YIELD;
}

static wasm_error_t wasm_test_resume_reenter(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_test_resume_host_t* host = (wasm_test_resume_host_t*)userdata;
    wasm_value_t inner = wasm_i64(0);
    wasm_error_t err;

    (void)num_args;
    (void)num_results;
    if (host->reenter_fuel) wasm_runtime_set_fuel(wasm_module_runtime(mod), host->reenter_fuel);
    err = wasm_call(host->mod, host->reenter_export, args, 1, &inner, 1);
    if (err != WASM_OK) return err;
    results[0] = wasm_i32(inner.type == WASM_TYPE_I64 ? (int32_t)inner.of.i64 : inner.of.i32);
    return WASM_OK;
}

static wasm_error_t wasm_test_resume_interrupt(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_runtime_interrupt(wasm_module_runtime(mod));
    return WASM_OK;
}

static wasm_module_t* wasm_test_load_resume_module(wl_test_ctx* t, wasm_runtime_t* rt, wasm_test_resume_host_t* host) {
    /* Function indices: 0 yield, 1 reenter, 2 interrupt (imports), 3 sum, 4 count, 5 rec,
     * 6 catch_after_yield, 7 tail_target, 8 tail, 9 tail_import, 10 outer, 11 gc_keep,
     * 12 self_interrupt, 13 spin_until_flag, 14 fill, 15 grow, 16 catch_loop, 17 leaf,
     * 18 call_after_interrupt, 19 nested_tail_import. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x20, 0x07, 0x5f, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f,
        0x01, 0x7f, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f, 0x01, 0x7e, 0x60, 0x02, 0x7f, 0x7f,
        0x01, 0x7f, 0x60, 0x00, 0x01, 0x7f, 0x02, 0x2e, 0x03, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x05, 0x79, 0x69, 0x65,
        0x6c, 0x64, 0x00, 0x01, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x07, 0x72, 0x65, 0x65, 0x6e, 0x74, 0x65, 0x72, 0x00,
        0x01, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x09, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x00, 0x02,
        0x03, 0x12, 0x11, 0x04, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x06, 0x01, 0x06, 0x01, 0x06,
        0x06, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x0d, 0x03, 0x01, 0x00, 0x03, 0x07, 0xc0, 0x01, 0x10, 0x03, 0x6d,
        0x65, 0x6d, 0x02, 0x00, 0x03, 0x73, 0x75, 0x6d, 0x00, 0x03, 0x05, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x00, 0x04,
        0x03, 0x72, 0x65, 0x63, 0x00, 0x05, 0x11, 0x63, 0x61, 0x74, 0x63, 0x68, 0x5f, 0x61, 0x66, 0x74, 0x65, 0x72,
        0x5f, 0x79, 0x69, 0x65, 0x6c, 0x64, 0x00, 0x06, 0x04, 0x74, 0x61, 0x69, 0x6c, 0x00, 0x08, 0x0b, 0x74, 0x61,
        0x69, 0x6c, 0x5f, 0x69, 0x6d, 0x70, 0x6f, 0x72, 0x74, 0x00, 0x09, 0x05, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x00,
        0x0a, 0x07, 0x67, 0x63, 0x5f, 0x6b, 0x65, 0x65, 0x70, 0x00, 0x0b, 0x0e, 0x73, 0x65, 0x6c, 0x66, 0x5f, 0x69,
        0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x00, 0x0c, 0x0f, 0x73, 0x70, 0x69, 0x6e, 0x5f, 0x75, 0x6e,
        0x74, 0x69, 0x6c, 0x5f, 0x66, 0x6c, 0x61, 0x67, 0x00, 0x0d, 0x04, 0x66, 0x69, 0x6c, 0x6c, 0x00, 0x0e, 0x04,
        0x67, 0x72, 0x6f, 0x77, 0x00, 0x0f, 0x0a, 0x63, 0x61, 0x74, 0x63, 0x68, 0x5f, 0x6c, 0x6f, 0x6f, 0x70, 0x00,
        0x10, 0x14, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x61, 0x66, 0x74, 0x65, 0x72, 0x5f, 0x69, 0x6e, 0x74, 0x65, 0x72,
        0x72, 0x75, 0x70, 0x74, 0x00, 0x12, 0x12, 0x6e, 0x65, 0x73, 0x74, 0x65, 0x64, 0x5f, 0x74, 0x61, 0x69, 0x6c,
        0x5f, 0x69, 0x6d, 0x70, 0x6f, 0x72, 0x74, 0x00, 0x13, 0x0a, 0xd4, 0x02, 0x11, 0x26, 0x02, 0x01, 0x7f, 0x01,
        0x7e, 0x02, 0x40, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00, 0x4f, 0x0d, 0x01, 0x20, 0x02, 0x20, 0x01, 0xad, 0x7c,
        0x21, 0x02, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x0c, 0x00, 0x0b, 0x0b, 0x20, 0x02, 0x0b, 0x17, 0x00,
        0x20, 0x00, 0x45, 0x04, 0x7f, 0x20, 0x01, 0x05, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x20, 0x01, 0x20, 0x00, 0x6a,
        0x12, 0x04, 0x0b, 0x0b, 0x18, 0x00, 0x20, 0x00, 0x45, 0x04, 0x7f, 0x41, 0xe4, 0x00, 0x10, 0x00, 0x05, 0x20,
        0x00, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x10, 0x05, 0x6a, 0x0b, 0x0b, 0x14, 0x00, 0x02, 0x7f, 0x1f, 0x40, 0x01,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x10, 0x00, 0x08, 0x00, 0x0b, 0x41, 0x7f, 0x0b, 0x0b, 0x09, 0x00, 0x20, 0x00,
        0x10, 0x00, 0x41, 0x02, 0x6c, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x12, 0x07, 0x0b, 0x06, 0x00,
        0x20, 0x00, 0x12, 0x00, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x10, 0x01, 0x41, 0x01, 0x6a, 0x0b, 0x27, 0x01, 0x01,
        0x63, 0x00, 0x41, 0x01, 0xfb, 0x00, 0x00, 0x1a, 0x20, 0x00, 0xfb, 0x00, 0x00, 0x21, 0x01, 0x41, 0x05, 0xfb,
        0x00, 0x00, 0x41, 0x00, 0x10, 0x00, 0x1a, 0xfb, 0x02, 0x00, 0x00, 0x20, 0x01, 0xfb, 0x02, 0x00, 0x00, 0x6a,
        0x0b, 0x21, 0x01, 0x01, 0x7f, 0x03, 0x40, 0x20, 0x01, 0x41, 0x03, 0x46, 0x04, 0x40, 0x10, 0x02, 0x0b, 0x20,
        0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x20, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0x0b, 0x18,
        0x01, 0x01, 0x7f, 0x03, 0x40, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x21, 0x00, 0x41, 0x00, 0x28, 0x02, 0x00, 0x45,
        0x0d, 0x00, 0x0b, 0x20, 0x00, 0x0b, 0x13, 0x00, 0x41, 0x10, 0x41, 0x07, 0x20, 0x00, 0xfc, 0x0b, 0x00, 0x41,
        0x0f, 0x20, 0x00, 0x6a, 0x2d, 0x00, 0x00, 0x0b, 0x06, 0x00, 0x41, 0x01, 0x40, 0x00, 0x0b, 0x2c, 0x00, 0x20,
        0x00, 0x03, 0x01, 0x21, 0x00, 0x20, 0x00, 0x45, 0x04, 0x40, 0x41, 0xcd, 0x00, 0x0f, 0x0b, 0x1f, 0x40, 0x01,
        0x00, 0x00, 0x00, 0x20, 0x00, 0x41, 0x02, 0x46, 0x04, 0x40, 0x10, 0x02, 0x0b, 0x20, 0x00, 0x41, 0x01, 0x6b,
        0x08, 0x00, 0x0b, 0x00, 0x0b, 0x0b, 0x04, 0x00, 0x41, 0x05, 0x0b, 0x06, 0x00, 0x10, 0x02, 0x10, 0x11, 0x0b,
        0x09, 0x00, 0x20, 0x00, 0x10, 0x09, 0x41, 0x01, 0x6a, 0x0b,
    };
    wasm_module_t* m;

    memset(host, 0, sizeof(*host));
    host->reenter_export = "sum";
    WASM_CHECK_OK(t, wasm_bind_host_func(rt, "host", "yield", "i(i)", wasm_test_resume_yield, host));
    WASM_CHECK_OK(t, wasm_bind_host_func(rt, "host", "reenter", "i(i)", wasm_test_resume_reenter, host));
    WASM_CHECK_OK(t, wasm_bind_host_func(rt, "host", "interrupt", "()", wasm_test_resume_interrupt, host));
    m = wasm_load(rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt->error_msg);
    host->mod = m;
    return m;
}

/* Runs export name with fuel units per step, resuming until it finishes; returns the step count. */
static uint32_t wasm_test_run_in_fuel_steps(wl_test_ctx* t, wasm_module_t* m, const char* name,
                                            const wasm_value_t* args, uint32_t num_args,
                                            uint64_t fuel, int grow_fuel, wasm_value_t* result) {
    wasm_runtime_t* rt = wasm_module_runtime(m);
    uint32_t steps = 0;
    wasm_error_t err;

    wasm_runtime_set_fuel(rt, fuel);
    err = wasm_call(m, name, args, num_args, result, 1);
    while (err == WASM_ERR_OUT_OF_FUEL && steps < 1000000u) {
        WL_CHECK(t, wasm_runtime_suspended(rt) == WASM_ERR_OUT_OF_FUEL);
        if (grow_fuel) fuel *= 2u;
        wasm_runtime_set_fuel(rt, fuel);
        err = wasm_runtime_resume(rt, NULL, 0, result, 1);
        steps++;
    }
    WL_CHECK_MSG(t, err == WASM_OK, "%s stopped with %s: %s", name, wasm_error_string(err), rt->error_msg);
    WL_CHECK(t, wasm_runtime_suspended(rt) == WASM_OK);
    wasm_runtime_disable_fuel(rt);
    return steps;
}

WL_TEST(test_resume_fuel_steps_match_unmetered_runs) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t args[2];
    wasm_value_t expected = wasm_i64(0);
    wasm_value_t result = wasm_i64(0);
    uint32_t steps;

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(1000);
    WASM_CHECK_OK(t, wasm_call(m, "sum", args, 1, &expected, 1));
    steps = wasm_test_run_in_fuel_steps(t, m, "sum", args, 1, 5, 0, &result);
    WL_CHECK_MSG(t, steps > 100u, "expected many fuel stops, got %u", (unsigned)steps);
    WASM_CHECK_I64(t, result.of.i64, expected.of.i64);
    WASM_CHECK_I64(t, result.of.i64, 499500);

    args[0] = wasm_i32(500);
    args[1] = wasm_i32(0);
    WASM_CHECK_OK(t, wasm_call(m, "count", args, 2, &expected, 1));
    steps = wasm_test_run_in_fuel_steps(t, m, "count", args, 2, 3, 0, &result);
    WL_CHECK(t, steps > 100u);
    WASM_CHECK_I32(t, result.of.i32, expected.of.i32);
    WASM_CHECK_I32(t, result.of.i32, 125250);

    args[0] = wasm_i32(4096);
    steps = wasm_test_run_in_fuel_steps(t, m, "fill", args, 1, 1, 1, &result);
    WL_CHECK(t, steps > 0u);
    WASM_CHECK_I32(t, result.of.i32, 7);

    wasm_runtime_set_fuel(&rt, 10);
    WL_CHECK(t, wasm_call(m, "grow", NULL, 0, &result, 1) == WASM_ERR_OUT_OF_FUEL);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_OUT_OF_FUEL);
    WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), 65536);
    wasm_runtime_set_fuel(&rt, 5000);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 1);
    WASM_CHECK_I64(t, (int64_t)wasm_memory_size(m, 0), 2 * 65536);
    wasm_runtime_disable_fuel(&rt);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_resume_yielding_host_imports) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t host_result;
    wasm_value_t result = wasm_i32(0);
    uint32_t func_idx = 0;
    char backtrace[256];

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    arg = wasm_i32(5);
    WL_CHECK(t, wasm_call(m, "rec", &arg, 1, &result, 1) == WASM_YIELD);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_YIELD);
    WASM_CHECK_I32(t, host.last_arg, 100);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 6);
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, NULL));
    WASM_CHECK_I32(t, (int32_t)func_idx, 5);
    wasm_runtime_backtrace(&rt, backtrace, sizeof(backtrace));
    WL_CHECK_MSG(t, backtrace[0] != '\0', "%s", "expected a backtrace while suspended");

    WL_CHECK(t, wasm_call(m, "sum", &arg, 1, &result, 1) == WASM_ERR_SUSPENDED);
    WL_CHECK(t, wasm_runtime_last_error(&rt) == WASM_ERR_SUSPENDED);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "suspended") != NULL, "%s", rt.error_msg);
    {
        static const uint8_t start_module[] = {
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x02,
            0x01, 0x00, 0x08, 0x01, 0x00, 0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
        };
        WL_CHECK(t, wasm_load(&rt, start_module, sizeof(start_module)) == NULL);
        WL_CHECK(t, wasm_runtime_last_error(&rt) == WASM_ERR_SUSPENDED);
    }
    WL_CHECK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    host_result = wasm_i64(7);
    WL_CHECK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    host_result = wasm_i32(7);
    WL_CHECK(t, wasm_runtime_resume(&rt, &host_result, 1, NULL, 0) == WASM_ERR_TYPE_MISMATCH);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_YIELD);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 5 + 4 + 3 + 2 + 1 + 7);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    WL_CHECK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1) == WASM_ERR_MALFORMED);

    arg = wasm_i32(3);
    WL_CHECK(t, wasm_call(m, "catch_after_yield", &arg, 1, &result, 1) == WASM_YIELD);
    host_result = wasm_i32(42);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);

    arg = wasm_i32(5);
    WL_CHECK(t, wasm_call(m, "tail", &arg, 1, &result, 1) == WASM_YIELD);
    WASM_CHECK_I32(t, host.last_arg, 6);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 1);
    host_result = wasm_i32(10);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 20);

    arg = wasm_i32(7);
    WL_CHECK(t, wasm_call(m, "tail_import", &arg, 1, &result, 1) == WASM_YIELD);
    WASM_CHECK_I32(t, host.last_arg, 7);
    host_result = wasm_i32(9);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 9);
    WL_CHECK(t, wasm_call(m, "nested_tail_import", &arg, 1, &result, 1) == WASM_YIELD);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 2);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 10);

    arg = wasm_i32(3);
    WL_CHECK(t, wasm_call(m, "rec", &arg, 1, &result, 1) == WASM_YIELD);
    wasm_runtime_cancel(&rt);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 0);
    arg = wasm_i32(10);
    WASM_CHECK_OK(t, wasm_call(m, "sum", &arg, 1, &result, 1));
    WASM_CHECK_I64(t, result.of.i64, 45);

    arg = wasm_i32(1);
    WL_CHECK(t, wasm_call_index(m, 0, &arg, 1, &result, 1) == WASM_ERR_TRAP);
    WL_CHECK_MSG(t, strstr(rt.error_msg, "yielded outside") != NULL, "%s", rt.error_msg);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);

    host.reenter_export = "rec";
    WL_CHECK(t, wasm_call(m, "outer", &arg, 1, &result, 1) == WASM_ERR_TRAP);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);

    arg = wasm_i32(2);
    WL_CHECK(t, wasm_call(m, "rec", &arg, 1, &result, 1) == WASM_YIELD);
    wasm_module_free(m);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    wasm_destroy(&rt);
}

WL_TEST(test_resume_keeps_gc_objects_alive_across_collections) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(9);
    wasm_value_t host_result = wasm_i32(0);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK(t, wasm_call(m, "gc_keep", &arg, 1, &result, 1) == WASM_YIELD);
    wasm_gc_collect(&rt);
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 9 + 5);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_resume_collects_with_references_on_the_operand_stack) {
    /* yield_refs(n, ext) and yield_exn() yield in host.yield with GC, i31, extern, and exn references
     * on the operand stack of two frames, returning n + 137 and 5; fuel_refs(n, ext) builds n boxes in
     * a loop that carries a reference and returns n + 1003. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x28, 0x08, 0x5f, 0x01, 0x7f, 0x00, 0x60, 0x01, 0x7f,
        0x01, 0x7f, 0x60, 0x01, 0x7f, 0x00, 0x60, 0x02, 0x7f, 0x6f, 0x01, 0x7f, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x01,
        0x69, 0x01, 0x7f, 0x60, 0x01, 0x69, 0x00, 0x60, 0x01, 0x64, 0x00, 0x01, 0x64, 0x00, 0x02, 0x0e, 0x01, 0x04,
        0x68, 0x6f, 0x73, 0x74, 0x05, 0x79, 0x69, 0x65, 0x6c, 0x64, 0x00, 0x01, 0x03, 0x05, 0x04, 0x01, 0x03, 0x04,
        0x03, 0x0d, 0x03, 0x01, 0x00, 0x02, 0x07, 0x26, 0x03, 0x0a, 0x79, 0x69, 0x65, 0x6c, 0x64, 0x5f, 0x72, 0x65,
        0x66, 0x73, 0x00, 0x02, 0x09, 0x79, 0x69, 0x65, 0x6c, 0x64, 0x5f, 0x65, 0x78, 0x6e, 0x00, 0x03, 0x09, 0x66,
        0x75, 0x65, 0x6c, 0x5f, 0x72, 0x65, 0x66, 0x73, 0x00, 0x04, 0x0a, 0xcb, 0x01, 0x04, 0x21, 0x00, 0x41, 0x0a,
        0xfb, 0x00, 0x00, 0x20, 0x00, 0x41, 0x14, 0xfb, 0x00, 0x00, 0x41, 0x01, 0x10, 0x00, 0x1a, 0xfb, 0x02, 0x00,
        0x00, 0x6a, 0x21, 0x00, 0xfb, 0x02, 0x00, 0x00, 0x20, 0x00, 0x6a, 0x0b, 0x29, 0x01, 0x01, 0x7f, 0x20, 0x01,
        0x41, 0xe4, 0x00, 0xfb, 0x00, 0x00, 0x41, 0x07, 0xfb, 0x1c, 0x20, 0x00, 0x10, 0x01, 0x21, 0x02, 0xfb, 0x1d,
        0x20, 0x02, 0x6a, 0x21, 0x02, 0xfb, 0x02, 0x00, 0x00, 0x20, 0x02, 0x6a, 0x21, 0x02, 0x1a, 0x20, 0x02, 0x0b,
        0x21, 0x00, 0x02, 0x69, 0x1f, 0x40, 0x01, 0x03, 0x00, 0x41, 0x05, 0x08, 0x00, 0x0b, 0x00, 0x0b, 0x41, 0x02,
        0x10, 0x00, 0x1a, 0x02, 0x05, 0x1f, 0x06, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x0b, 0x00, 0x0b, 0x0b, 0x5b, 0x02,
        0x01, 0x7f, 0x01, 0x63, 0x00, 0x20, 0x01, 0x41, 0x03, 0xfb, 0x1c, 0x41, 0xe8, 0x07, 0xfb, 0x00, 0x00, 0x41,
        0x00, 0xfb, 0x00, 0x00, 0x03, 0x07, 0x21, 0x03, 0x41, 0x05, 0xfb, 0x00, 0x00, 0x20, 0x03, 0xfb, 0x02, 0x00,
        0x00, 0x41, 0x01, 0x6a, 0xfb, 0x00, 0x00, 0x20, 0x02, 0x41, 0x01, 0x6a, 0x21, 0x02, 0x20, 0x02, 0x20, 0x00,
        0x49, 0x0d, 0x00, 0x21, 0x03, 0x1a, 0x20, 0x03, 0xd4, 0x0b, 0xfb, 0x02, 0x00, 0x00, 0x21, 0x02, 0xfb, 0x02,
        0x00, 0x00, 0x20, 0x02, 0x6a, 0x21, 0x02, 0xfb, 0x1d, 0x20, 0x02, 0x6a, 0x21, 0x02, 0x1a, 0x20, 0x02, 0x0b,
    };
    static const uint64_t fuel_steps[] = { 1, 2, 3, 7, 50 };
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t args[2];
    wasm_value_t host_result = wasm_i32(0);
    wasm_value_t result = wasm_i32(0);
    uint32_t i;

    wasm_init(&rt);
    memset(&host, 0, sizeof(host));
    WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "host", "yield", "i(i)", wasm_test_resume_yield, &host));
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    args[0] = wasm_i32(4);
    args[1] = wasm_externref((uintptr_t)0x1230u);
    WL_CHECK(t, wasm_call(m, "yield_refs", args, 2, &result, 1) == WASM_YIELD);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 2);
    wasm_gc_collect(&rt);
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 4 + 137);

    WL_CHECK(t, wasm_call(m, "yield_exn", NULL, 0, &result, 1) == WASM_YIELD);
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 5);

    for (i = 0; i < sizeof(fuel_steps) / sizeof(fuel_steps[0]); i++) {
        uint32_t steps = 0;
        wasm_error_t err;

        args[0] = wasm_i32(6);
        wasm_runtime_set_fuel(&rt, fuel_steps[i]);
        err = wasm_call(m, "fuel_refs", args, 2, &result, 1);
        while (err == WASM_ERR_OUT_OF_FUEL && steps < 100000u) {
            wasm_gc_collect(&rt);
            wasm_runtime_set_fuel(&rt, fuel_steps[i]);
            err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
            steps++;
        }
        WL_CHECK_MSG(t, err == WASM_OK, "fuel step %u stopped with %s: %s", (unsigned)fuel_steps[i],
                     wasm_error_string(err), rt.error_msg);
        WL_CHECK(t, steps > 0);
        WASM_CHECK_I32(t, result.of.i32, 6 + 1003);
    }
    wasm_runtime_disable_fuel(&rt);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_resume_after_interrupts) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(10);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    WL_CHECK(t, wasm_call(m, "self_interrupt", &arg, 1, &result, 1) == WASM_ERR_INTERRUPTED);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
    wasm_runtime_interrupt(&rt);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 10);

    arg = wasm_i32(5);
    WL_CHECK(t, wasm_call(m, "catch_loop", &arg, 1, &result, 1) == WASM_ERR_INTERRUPTED);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 77);

    WL_CHECK(t, wasm_call(m, "call_after_interrupt", NULL, 0, &result, 1) == WASM_ERR_INTERRUPTED);
    WASM_CHECK_I32(t, (int32_t)wasm_runtime_call_depth(&rt), 2);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 5);

    wasm_runtime_interrupt(&rt);
    WL_CHECK(t, wasm_call(m, "sum", &arg, 1, &result, 1) == WASM_ERR_INTERRUPTED);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);

#if defined(WASM_TEST_THREADS)
    {
        wasm_test_thread_start_t start;
        wasm_test_thread_t thread;
        uint8_t* data = wasm_memory_data(m, 0);

        memset(data, 0, 4);
        start.fn = wasm_test_interrupt_later;
        start.arg = &rt;
        WL_CHECK(t, wasm_test_thread_spawn(&thread, &start));
        WL_CHECK_MSG(t, wasm_call(m, "spin_until_flag", NULL, 0, &result, 1) == WASM_ERR_INTERRUPTED, "%s",
                     rt.error_msg);
        wasm_test_thread_join(thread);
        WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
        data[0] = 1;
        WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
        WL_CHECK(t, result.of.i32 > 0);
    }
#endif

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_resume_reentered_calls_unwind_on_fuel) {
    wasm_runtime_t rt;
    wasm_test_resume_host_t host;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(1000);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    m = wasm_test_load_resume_module(t, &rt, &host);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    host.reenter_fuel = 5;
    WL_CHECK(t, wasm_call(m, "outer", &arg, 1, &result, 1) == WASM_ERR_OUT_OF_FUEL);
    WL_CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    host.reenter_fuel = 0;
    wasm_runtime_disable_fuel(&rt);
    arg = wasm_i32(10);
    WASM_CHECK_OK(t, wasm_call(m, "outer", &arg, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 46);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_register_module_links_instances_and_host_objects) {
    /* a exports double(x) = 2x, mutable global g = 1, immutable global k = 5, table t holding double,
     * and memory m whose first byte is 100. b imports all of them; run() returns
     * double(g) + m[0] + table.size(t). c imports only double. e imports host.mem and host.tab and
     * grows them. */
    static const uint8_t a_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x03, 0x02,
        0x01, 0x00, 0x04, 0x04, 0x01, 0x70, 0x00, 0x02, 0x05, 0x03, 0x01, 0x00, 0x01, 0x06, 0x0b, 0x02, 0x7f, 0x01,
        0x41, 0x01, 0x0b, 0x7f, 0x00, 0x41, 0x05, 0x0b, 0x07, 0x1a, 0x05, 0x06, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65,
        0x00, 0x00, 0x01, 0x67, 0x03, 0x00, 0x01, 0x6b, 0x03, 0x01, 0x01, 0x74, 0x01, 0x00, 0x01, 0x6d, 0x02, 0x00,
        0x09, 0x07, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x41, 0x02,
        0x6c, 0x0b, 0x0b, 0x07, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x64,
    };
    static const uint8_t b_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x02, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x00,
        0x01, 0x7f, 0x02, 0x22, 0x04, 0x01, 0x61, 0x06, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x01, 0x61,
        0x01, 0x67, 0x03, 0x7f, 0x01, 0x01, 0x61, 0x01, 0x74, 0x01, 0x70, 0x00, 0x02, 0x01, 0x61, 0x01, 0x6d, 0x02,
        0x00, 0x01, 0x03, 0x02, 0x01, 0x01, 0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x01, 0x0a, 0x12, 0x01,
        0x10, 0x00, 0x23, 0x00, 0x10, 0x00, 0x41, 0x00, 0x2d, 0x00, 0x00, 0x6a, 0xfc, 0x10, 0x00, 0x6a, 0x0b,
    };
    static const uint8_t c_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x0c,
        0x01, 0x01, 0x61, 0x06, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x07, 0x0f,
        0x01, 0x0b, 0x63, 0x61, 0x6c, 0x6c, 0x5f, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x00, 0x01, 0x0a, 0x08, 0x01,
        0x06, 0x00, 0x20, 0x00, 0x10, 0x00, 0x0b,
    };
    static const uint8_t e_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x1b,
        0x02, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x01, 0x01, 0x02, 0x04, 0x68, 0x6f, 0x73,
        0x74, 0x03, 0x74, 0x61, 0x62, 0x01, 0x70, 0x00, 0x01, 0x03, 0x03, 0x02, 0x00, 0x00, 0x07, 0x17, 0x02, 0x08,
        0x67, 0x72, 0x6f, 0x77, 0x5f, 0x6d, 0x65, 0x6d, 0x00, 0x00, 0x08, 0x67, 0x72, 0x6f, 0x77, 0x5f, 0x74, 0x61,
        0x62, 0x00, 0x01, 0x0a, 0x12, 0x02, 0x06, 0x00, 0x20, 0x00, 0x40, 0x00, 0x0b, 0x09, 0x00, 0xd0, 0x70, 0x20,
        0x00, 0xfc, 0x0f, 0x00, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* a;
    wasm_module_t* b;
    wasm_module_t* c;
    wasm_module_t* e;
    wasm_memory_t* memory;
    wasm_table_t* table;
    wasm_memory_import_t memory_import;
    wasm_table_import_t table_import;
    wasm_value_t value;
    wasm_value_t ref;
    wasm_value_t arg;
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    a = wasm_load(&rt, a_bytes, sizeof(a_bytes));
    WL_CHECK_MSG(t, a != NULL, "%s", rt.error_msg);
    if (!a) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "a", a));
    b = wasm_load(&rt, b_bytes, sizeof(b_bytes));
    c = wasm_load(&rt, c_bytes, sizeof(c_bytes));
    WL_CHECK_MSG(t, b != NULL && c != NULL, "%s", rt.error_msg);

    WASM_CHECK_OK(t, wasm_global_set_index(a, 0, wasm_i32(21)));
    WL_CHECK(t, wasm_global_set_index(a, 1, wasm_i32(6)) == WASM_ERR_TYPE_MISMATCH);
    WL_CHECK(t, wasm_global_set_index(a, 0, wasm_i64(21)) == WASM_ERR_TYPE_MISMATCH);
    WASM_CHECK_OK(t, wasm_global_get_index(a, 1, &value));
    WL_CHECK(t, value.of.i32 == 5);
    WL_CHECK(t, wasm_global_get_index(a, 2, &value) == WASM_ERR_MALFORMED);

    WL_CHECK(t, wasm_table_size(a, 0) == 2u);
    WASM_CHECK_OK(t, wasm_table_get(a, 0, 0, &ref));
    arg = wasm_i32(5);
    WASM_CHECK_OK(t, wasm_call_ref(&rt, ref, &arg, 1, &result, 1));
    WL_CHECK(t, result.of.i32 == 10);
    WASM_CHECK_OK(t, wasm_table_get(a, 0, 1, &value));
    WL_CHECK(t, wasm_call_ref(&rt, value, &arg, 1, &result, 1) == WASM_ERR_TYPE_MISMATCH);
    WL_CHECK(t, wasm_table_grow(a, 0, 1, ref) == 2);
    WL_CHECK(t, wasm_table_grow(a, 0, 1, wasm_i32(0)) == -1);
    WASM_CHECK_OK(t, wasm_table_set(a, 0, 1, ref));
    WL_CHECK(t, wasm_table_set(a, 0, 1, wasm_i32(0)) == WASM_ERR_TYPE_MISMATCH);
    WL_CHECK(t, wasm_table_get(a, 0, 3, &value) == WASM_ERR_OUT_OF_BOUNDS);

    if (b) {
        WASM_CHECK_OK(t, wasm_call(b, "run", NULL, 0, &result, 1));
        WL_CHECK_MSG(t, result.of.i32 == 42 + 100 + 3, "expected 145, got %d", (int)result.of.i32);
        wasm_module_free(b);
    }
    wasm_module_free(a);
    if (c) {
        arg = wasm_i32(4);
        WL_CHECK(t, wasm_call(c, "call_double", &arg, 1, &result, 1) == WASM_ERR_TRAP);
        WL_CHECK_MSG(t, strstr(rt.error_msg, "freed module") != NULL, "%s", rt.error_msg);
        wasm_module_free(c);
    }

    memory = wasm_memory_new(&wasm_test_allocator, 1, 2, 0);
    table = wasm_table_new(&wasm_test_allocator, WASM_TYPE_FUNCREF, 1, UINT32_MAX);
    WL_CHECK(t, memory != NULL && table != NULL);
    memset(&memory_import, 0, sizeof(memory_import));
    memory_import.module = "host";
    memory_import.name = "mem";
    memory_import.memory = memory;
    memset(&table_import, 0, sizeof(table_import));
    table_import.module = "host";
    table_import.name = "tab";
    table_import.table = table;
    WASM_CHECK_OK(t, wasm_register_memory_import(&rt, &memory_import));
    WASM_CHECK_OK(t, wasm_register_table_import(&rt, &table_import));
    e = wasm_load(&rt, e_bytes, sizeof(e_bytes));
    WL_CHECK_MSG(t, e != NULL, "%s", rt.error_msg);
    if (e) {
        arg = wasm_i32(1);
        WASM_CHECK_OK(t, wasm_call(e, "grow_mem", &arg, 1, &result, 1));
        WL_CHECK(t, result.of.i32 == 1 && memory->pages == 2u);
        WL_CHECK(t, wasm_memory_base(memory) == wasm_memory_data(e, 0));
        WL_CHECK(t, wasm_memory_length(memory) == 2u * WASM_PAGE_SIZE);
        WASM_CHECK_OK(t, wasm_call(e, "grow_mem", &arg, 1, &result, 1));
        WL_CHECK(t, result.of.i32 == -1);
        arg = wasm_i32(3);
        WASM_CHECK_OK(t, wasm_call(e, "grow_tab", &arg, 1, &result, 1));
        WL_CHECK(t, result.of.i32 == 1 && table->size == 4u);
        wasm_module_free(e);
    }
    wasm_memory_free(memory);
    wasm_table_free(table);
    wasm_destroy(&rt);
}

WL_TEST(test_backtraces_name_modules_and_functions) {
    /* (module $demo (func $inner unreachable) (func $outer (export "outer") call $inner)) */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x03, 0x03, 0x02, 0x00,
        0x00, 0x07, 0x09, 0x01, 0x05, 0x6f, 0x75, 0x74, 0x65, 0x72, 0x00, 0x01, 0x0a, 0x0a, 0x02, 0x03, 0x00, 0x00,
        0x0b, 0x04, 0x00, 0x10, 0x00, 0x0b, 0x00, 0x1d, 0x04, 0x6e, 0x61, 0x6d, 0x65, 0x00, 0x05, 0x04, 0x64, 0x65,
        0x6d, 0x6f, 0x01, 0x0f, 0x02, 0x00, 0x05, 0x69, 0x6e, 0x6e, 0x65, 0x72, 0x01, 0x05, 0x6f, 0x75, 0x74, 0x65,
        0x72,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_module_t* frame_module = NULL;
    uint32_t func_idx = 0;
    char trace[256];

    wasm_init(&rt);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    WL_CHECK(t, wasm_module_name(m) && strcmp(wasm_module_name(m), "demo") == 0);
    WL_CHECK(t, wasm_func_name(m, 1) && strcmp(wasm_func_name(m, 1), "outer") == 0);
    WL_CHECK(t, wasm_func_name(m, 2) == NULL);

    WL_CHECK(t, wasm_call(m, "outer", NULL, 0, NULL, 0) == WASM_ERR_UNREACHABLE);
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, &frame_module, &func_idx, NULL));
    WL_CHECK(t, frame_module == m && func_idx == 0u);
    wasm_runtime_backtrace(&rt, trace, sizeof(trace));
    WL_CHECK_MSG(t, strstr(trace, "#0 demo!inner (func[0]) at offset") != NULL &&
                        strstr(trace, "#1 demo!outer (func[1]) at offset") != NULL,
                 "unexpected backtrace:\n%s", trace);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

WL_TEST(test_gc_collects_unreachable_externref_boxes_and_exnrefs) {
    /* boxes(n) converts n i31refs to externref and keeps one boxed struct alive;
     * exns(n) catches n exceptions with catch_all_ref and drops each exnref. */
    static const uint8_t module_bytes[] = {
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0b, 0x03, 0x5f, 0x00, 0x60, 0x00, 0x00, 0x60, 0x01,
        0x7f, 0x01, 0x7f, 0x03, 0x03, 0x02, 0x02, 0x02, 0x0d, 0x03, 0x01, 0x00, 0x01, 0x07, 0x10, 0x02, 0x05, 0x62,
        0x6f, 0x78, 0x65, 0x73, 0x00, 0x00, 0x04, 0x65, 0x78, 0x6e, 0x73, 0x00, 0x01, 0x0a, 0x49, 0x02, 0x29, 0x01,
        0x01, 0x6f, 0xfb, 0x00, 0x00, 0xfb, 0x1b, 0x21, 0x01, 0x03, 0x40, 0x20, 0x00, 0xfb, 0x1c, 0xfb, 0x1b, 0x1a,
        0xfb, 0x00, 0x00, 0x1a, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x22, 0x00, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0xfb, 0x1a,
        0xfb, 0x14, 0x00, 0x0b, 0x1d, 0x00, 0x03, 0x40, 0x02, 0x69, 0x1f, 0x40, 0x01, 0x03, 0x00, 0x08, 0x00, 0x0b,
        0x00, 0x0b, 0x1a, 0x20, 0x00, 0x41, 0x01, 0x6b, 0x22, 0x00, 0x0d, 0x00, 0x0b, 0x41, 0x01, 0x0b,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(100000);
    wasm_value_t result = wasm_i32(0);

    wasm_init(&rt);
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    WL_CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_OK(t, wasm_call(m, "boxes", &arg, 1, &result, 1));
    WL_CHECK_MSG(t, result.of.i32 == 1, "%s", "expected the rooted box to still hold its struct");
    WL_CHECK_MSG(t, rt.externref_boxes.count < 4096u, "expected unreachable boxes to be freed, %u live",
                 (unsigned)rt.externref_boxes.count);

    WASM_CHECK_OK(t, wasm_call(m, "exns", &arg, 1, &result, 1));
    WL_CHECK_MSG(t, rt.exception_objects.count < 4096u, "expected unreachable exnrefs to be freed, %u live",
                 (unsigned)rt.exception_objects.count);

    wasm_gc_collect(&rt);
    WL_CHECK(t, rt.externref_boxes.count == 0u);
    WL_CHECK(t, rt.exception_objects.count == 0u);

    wasm_module_free(m);
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
        { "memory64: i64 load/store plus memory.size/memory.grow", test_memory64_basic_ops },
        { "memory64: active data offsets accept i64 init exprs", test_memory64_active_data_segment_offset },
        { "memory64: validator rejects i32 memory addresses", test_memory64_rejects_i32_address_operands },
        { "table64: import width mismatch is rejected", test_table64_import_width_mismatch },
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
        { "float copies/sign: signaling NaN payloads", test_float_signaling_nan_bits },
        { "float min/max: NaN propagation and signed zero semantics", test_float_min_max_semantics },
        { "float trunc: trapping conversions reject NaN and overflow", test_trapping_float_to_int_conversions },
        { "float trunc: fractional lower-edge values still convert", test_trunc_allows_fractional_lower_edges },
        { "rotates: zero-count paths avoid shift-width UB", test_rotate_by_zero },
        { "SIMD helpers: arithmetic, compare, shuffle, and conversions", test_simd_helper_ops },
        { "SIMD: execution path, memory ops, select, and feature gate", test_simd_execution_and_feature_gate },
        { "trunc-sat ops: all 0xFC 0x00-0x07 cases", test_trunc_sat_ops },
        { "wide arithmetic: disabled by default, 128-bit products when enabled", test_wide_arithmetic_is_opt_in },
#if defined(WASM_TEST_THREADS)
        { "threads: runtimes on separate threads share atomics and wait/notify", test_threads_shared_memory_across_runtimes },
#endif
        { "custom page sizes: disabled by default, byte-sized pages when enabled", test_custom_page_sizes_is_opt_in },
        { "compact imports: disabled by default, empty groups accepted when enabled", test_compact_imports_is_opt_in },
        { "tail call: self-recursive return_call reuses the frame", test_tail_call_self_recursion },
        { "tail call: return_call_indirect dispatches through a table", test_tail_call_indirect },
        { "indirect call: structurally equal signatures are accepted", test_call_indirect_uses_structural_type_equality },
        { "wasmgc: composite type sections parse rec/sub/struct/array forms", test_gc_type_section_parses_composite_types },
        { "wasmgc: GC type syntax is feature-gated", test_gc_type_section_respects_feature_gate },
        { "decode: vector counts beyond the section are malformed", test_decode_rejects_counts_beyond_section },
        { "wasmgc: subtypes may declare at most one supertype", test_gc_rejects_multiple_supertypes },
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
        { "GC: collection marks deep object chains without recursion", test_gc_collect_marks_deep_object_chains_iteratively },
        { "wasmgc: allocator retries after collecting unreachable objects", test_gc_allocator_retries_after_collection_on_heap_exhaustion },
        { "gc: the heap grows to fit live data and keeps objects intact", test_gc_heap_grows_with_live_data },
        { "gc: heap growth stops at max_gc_heap_bytes with WASM_ERR_OOM", test_gc_heap_growth_respects_max_gc_heap_bytes },
        { "gc: host externrefs round-trip exactly through anyref and are never i31 or eq", test_gc_host_externrefs_round_trip_through_anyref },
        { "gc: host references whose bits fall inside a later GC heap stay host values", test_gc_host_refs_inside_a_later_heap_stay_host_values },
        { "gc: init expressions keep their operands, globals, tables, and segments alive", test_gc_init_expressions_keep_their_operands_alive },
        { "gc: wasm_struct_new and wasm_array_new keep their inputs alive", test_gc_host_constructors_keep_their_inputs_alive },
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
        { "allocator: libc alignment and invalid allocators", test_allocator_contract },
        { "allocator: every failing allocation unwinds without leaks", test_allocation_failures_unwind_without_leaks },
        { "runtime config: zero limits are rejected", test_runtime_config_rejects_zero_limits },
        { "runtime config: validator stack limit is enforced at load", test_runtime_config_limits_operand_stack_validation },
        { "runtime: operand stack overflow traps at the call that needs the space", test_runtime_operand_stack_overflow_traps_at_call },
        { "runtime config: validator label limit is enforced at load", test_runtime_config_limits_label_depth_validation },
        { "runtime config: lazy validation defers body checks until first call", test_lazy_validation_defers_body_errors_until_first_call },
        { "runtime config: lazy validation still validates startup bodies at load", test_lazy_validation_still_runs_startup_bodies_at_load },
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
        { "load: function locals are limited by config", test_load_limits_function_locals },
        { "types: canonicalization dedupes thousands of types", test_canonicalization_dedupes_many_types },
        { "public api: NULL handles are rejected without crashing", test_public_api_tolerates_null_handles },
        { "funcref: references into freed modules trap", test_funcrefs_from_freed_modules_trap },
        { "exceptions: imports that throw reach handlers on every call path", test_exceptions_from_imports_reach_handlers_on_all_call_paths },
        { "runtime config: memory and table sizes are limited", test_runtime_limits_memory_and_tables },
        { "public api: compile inspects without imports and instantiates repeatedly", test_compile_inspects_without_imports_and_instantiates_repeatedly },
        { "host import: results must match the declared type", test_host_results_must_match_the_import_type },
        { "memory: growth by a callee or host is visible to the caller", test_memory_grown_by_callee_or_host_is_visible_to_caller },
        { "interrupt: stops loops, calls, and atomic waits from another thread", test_runtime_interrupt_stops_loops_calls_and_waits },
        { "resume: fuel stops resumed step by step match unmetered runs", test_resume_fuel_steps_match_unmetered_runs },
        { "resume: yielding host imports suspend nested frames until resumed or cancelled", test_resume_yielding_host_imports },
        { "resume: GC objects in suspended frames survive collections", test_resume_keeps_gc_objects_alive_across_collections },
        { "resume: collections while suspended trace references on the operand stack", test_resume_collects_with_references_on_the_operand_stack },
        { "resume: interrupted calls continue where they stopped", test_resume_after_interrupts },
        { "resume: a call re-entered from a host function unwinds when it runs out of fuel", test_resume_reentered_calls_unwind_on_fuel },
        { "public api: register_module links instances; table, global, funcref, and host object APIs", test_register_module_links_instances_and_host_objects },
        { "backtrace: frames name their module and function from the name section", test_backtraces_name_modules_and_functions },
        { "gc: unreachable externref boxes and exnrefs are freed", test_gc_collects_unreachable_externref_boxes_and_exnrefs },
        { "gc: bulk instructions pay fuel for their size; array.new and held values stay rooted", test_bulk_fuel_and_array_new_rooting },
        { "load: repeated and out-of-order sections are malformed", test_load_rejects_misordered_sections },
        { "reject bad magic number", test_bad_magic },
        { "trap on i32.div_s by zero", test_div_by_zero },
        { "public api: failed calls record the trap code and message", test_failed_calls_record_trap_codes_and_messages },
        { "public api: backtrace helpers expose trap call stacks", test_public_backtrace_helpers_capture_traps },
        { "public api: fuel helpers meter finite execution", test_public_fuel_helpers_account_for_finite_calls },
        { "public api: fuel helpers stop infinite loops", test_public_fuel_helpers_trap_in_infinite_loop },
#if WASM_ENABLE_PLATFORM
        { "public api: WASI stubs bind fd_write, environ_get, and proc_exit", test_public_wasi_stub_helpers },
        { "public api: WASI stubs expose args, env, fdstat, random, and clock helpers", test_public_wasi_metadata_helpers },
        { "public api: WASI stubs bind the full preview1 surface", test_public_wasi_preview1_surface_imports },
        { "wasi: proc_raise reports ENOSYS instead of signaling the host", test_wasi_proc_raise_does_not_signal_the_host },
        { "wasi: per-runtime args, environment, preopens, and poll_oneoff", test_wasi_config_args_env_preopens_and_poll },
        { "WASI: path operations need a preopen and stay beneath it", test_wasi_paths_stay_beneath_preopens },
        { "public api: Emscripten stubs bind toolchain env imports and memory", test_public_emscripten_stub_helpers },
#endif
        { "reject global.set on immutable global", test_set_immutable_global_rejected },
        { "public api: exported globals can be read and updated safely", test_public_global_helpers },
    };

    return wl_test_run("wasm", cases, WL_COUNTOF(cases));
}

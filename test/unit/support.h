#ifndef WASM_TEST_SUPPORT_H
#define WASM_TEST_SUPPORT_H

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "test.h"

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

int wasm_test_thread_spawn(wasm_test_thread_t* thread, wasm_test_thread_start_t* start);
void wasm_test_thread_join(wasm_test_thread_t thread);
#endif

#ifndef WASM_TEST_MODULAR
#include "../../wasm.h"
#else
#include "../../src/module.h"
#include "../../src/runtime.h"
#endif

#include "suites.h"

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

extern wasm_test_alloc_stats g_wasm_test_alloc_stats;
extern size_t g_wasm_test_alloc_fail_countdown;
void* wasm_test_alloc(void* ctx, size_t size, size_t align);
void* wasm_test_resize(void* ctx, void* p, size_t old_size, size_t new_size, size_t align);
void wasm_test_free(void* ctx, void* p, size_t size, size_t align);
extern const wasm_allocator_t wasm_test_allocator;
wasm_test_alloc_stats wasm_test_alloc_snapshot(void);
wasm_error_t wasm_test_init_raw(wasm_runtime_t* rt, const wasm_config_t* config);
void wasm_test_init_checked(test_ctx* t, wasm_runtime_t* rt, const char* test_name);

void wasm_test_init_with_config_checked(test_ctx* t,
                                        wasm_runtime_t* rt,
                                        const wasm_config_t* config,
                                        const char* test_name);

void wasm_test_init_features_checked(test_ctx* t,
                                     wasm_runtime_t* rt,
                                     uint64_t features,
                                     const char* test_name);

void wasm_test_destroy_checked(test_ctx* t, wasm_runtime_t* rt, const char* test_name);
void wasm_test_verify_case_cleanup(test_ctx* t);

#define wasm_init(rt) wasm_test_init_checked(t, (rt), ((t) && (t)->case_name) ? (t)->case_name : __func__)

#define wasm_init_config(rt, config)             \
    wasm_test_init_with_config_checked(t,        \
                                       (rt),     \
                                       (config), \
                                       ((t) && (t)->case_name) ? (t)->case_name : __func__)

#define wasm_init_features(rt, features) \
    wasm_test_init_features_checked(t, (rt), (features), ((t) && (t)->case_name) ? (t)->case_name : __func__)

#define wasm_destroy(rt) wasm_test_destroy_checked(t, (rt), ((t) && (t)->case_name) ? (t)->case_name : __func__)

int wasm_test_value_is_i31ref(const wasm_value_t* value);
int32_t wasm_test_gc_i31_get_s(uintptr_t bits);

#define WASM_CHECK_OK(t, err)                                                                    \
    do {                                                                                         \
        wasm_error_t wasm_check_ok_err_ = (err);                                                 \
        CHECK_MSG((t), wasm_check_ok_err_ == WASM_OK, "%s", wasm_error_string(wasm_check_ok_err_)); \
    } while (0)

#define WASM_CHECK_I32(t, actual, expected) \
    CHECK_MSG((t), (actual) == (expected), "expected %d, got %d", (expected), (actual))

#define WASM_CHECK_I64(t, actual, expected) \
    CHECK_MSG((t), (actual) == (expected), "expected %lld, got %lld", (long long)(expected), (long long)(actual))

/* ── Helper: build minimal wasm module ──────────────────────────── */

typedef struct {
    uint8_t buf[4096];
    uint32_t len;
} wasm_builder_t;

void emit(wasm_builder_t* b, uint8_t byte);
void emit_bytes(wasm_builder_t* b, const uint8_t* data, uint32_t len);
void emit_leb128_u32(wasm_builder_t* b, uint32_t val);
void emit_leb128_i32(wasm_builder_t* b, int32_t val);
void emit_leb128_i64(wasm_builder_t* b, int64_t val);

void emit_memarg(wasm_builder_t* b,
                 uint32_t align_log2,
                 int has_memory_index,
                 uint32_t memory_index,
                 uint64_t offset);

void emit_memory_type(wasm_builder_t* b,
                      int is_64,
                      int has_max,
                      uint64_t initial_pages,
                      uint64_t max_pages);

void emit_section(wasm_builder_t* b, uint8_t id, const uint8_t* content, uint32_t len);
void emit_export_func(wasm_builder_t* b, const char* name, uint32_t index);
void emit_export_kind(wasm_builder_t* b, const char* name, uint8_t kind, uint32_t index);

#if WASM_ENABLE_PLATFORM
void emit_import_func(wasm_builder_t* b, const char* module, const char* name, uint32_t type_index);
#endif

void emit_header(wasm_builder_t* b);
void emit_single_tag_section(wasm_builder_t* b, uint32_t type_index);

void wasm_test_expect_u8x16(test_ctx* t,
                            const wasm_value_t* actual,
                            const uint8_t expected[16],
                            const char* label);

extern int host_print_called;
extern int32_t host_print_value;

wasm_error_t host_print(wasm_module_t* mod,
                        const wasm_value_t* args, uint32_t num_args,
                        wasm_value_t* results, uint32_t num_results,
                        void* userdata);

wasm_error_t wasm_test_host_add(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                wasm_value_t* results, uint32_t num_results, void* userdata);

wasm_module_t* wasm_test_load_sections(wasm_runtime_t* rt, const uint8_t* sections, uint32_t len);
wasm_module_t* wasm_test_load_cont(wasm_runtime_t* rt, const uint8_t* sections, uint32_t len);

#if defined(WASM_TEST_THREADS)
void wasm_test_interrupt_later(void* arg);
#endif

uint32_t wasm_test_run_in_fuel_steps(test_ctx* t, wasm_module_t* m, const char* name,
                                     const wasm_value_t* args, uint32_t num_args,
                                     uint64_t fuel, int grow_fuel, wasm_value_t* result);
#endif

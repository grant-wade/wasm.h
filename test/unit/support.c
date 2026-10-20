#include "support.h"

#ifdef WASM_TEST_MODULAR
#include "../../src/simd/lanes.h"
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#if defined(WASM_TEST_THREADS)
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

int wasm_test_thread_spawn(wasm_test_thread_t* thread, wasm_test_thread_start_t* start) {
#if defined(_WIN32)
    *thread = CreateThread(NULL, 0, wasm_test_thread_main, start, 0, NULL);
    return *thread != NULL;
#else
    return pthread_create(thread, NULL, wasm_test_thread_main, start) == 0;
#endif
}

void wasm_test_thread_join(wasm_test_thread_t thread) {
#if defined(_WIN32)
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_join(thread, NULL);
#endif
}
#endif

wasm_test_alloc_stats g_wasm_test_alloc_stats;

size_t g_wasm_test_alloc_fail_countdown;

/* Counters change atomically because the threads test runs runtimes on several threads at once. */
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

void* wasm_test_alloc(void* ctx, size_t size, size_t align) {
    (void)ctx;
    wasm_test_counter_update(&g_wasm_test_alloc_stats.alloc_calls, wasm_test_increment);
    if (wasm_test_alloc_should_fail()) return NULL;
    return wasm_test_alloc_block(size, align);
}

void* wasm_test_resize(void* ctx, void* p, size_t old_size, size_t new_size, size_t align) {
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

void wasm_test_free(void* ctx, void* p, size_t size, size_t align) {
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

const wasm_allocator_t wasm_test_allocator = { NULL, wasm_test_alloc, wasm_test_resize, wasm_test_free };

wasm_test_alloc_stats wasm_test_alloc_snapshot(void) {
    return g_wasm_test_alloc_stats;
}

wasm_error_t wasm_test_init_raw(wasm_runtime_t* rt, const wasm_config_t* config) {
    return wasm__runtime_init(rt, &wasm_test_allocator, config);
}

void wasm_test_init_checked(test_ctx* t, wasm_runtime_t* rt, const char* test_name) {
    wasm_error_t err;

    CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0,
              "allocator leak before %s: %zu live blocks",
              test_name,
              g_wasm_test_alloc_stats.live_blocks);
    err = wasm_test_init_raw(rt, NULL);
    CHECK_MSG(t, err == WASM_OK,
              "wasm_init failed before %s: %s",
              test_name,
              rt->error_msg[0] ? rt->error_msg : wasm_error_string(err));
}

void wasm_test_init_with_config_checked(test_ctx* t,
                                        wasm_runtime_t* rt,
                                        const wasm_config_t* config,
                                        const char* test_name) {
    wasm_error_t err;

    CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0,
              "allocator leak before %s: %zu live blocks",
              test_name,
              g_wasm_test_alloc_stats.live_blocks);
    err = wasm_test_init_raw(rt, config);
    CHECK_MSG(t, err == WASM_OK,
              "wasm_init failed before %s: %s",
              test_name,
              rt->error_msg[0] ? rt->error_msg : wasm_error_string(err));
}

void wasm_test_init_features_checked(test_ctx* t,
                                     wasm_runtime_t* rt,
                                     uint64_t features,
                                     const char* test_name) {
    wasm_config_t config;

    wasm_config_default(&config);
    config.features = features;
    wasm_test_init_with_config_checked(t, rt, &config, test_name);
}

void wasm_test_destroy_checked(test_ctx* t, wasm_runtime_t* rt, const char* test_name) {
    wasm__runtime_destroy(rt);
    CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0,
              "allocator leak after %s: %zu live blocks",
              test_name,
              g_wasm_test_alloc_stats.live_blocks);
}

void wasm_test_verify_case_cleanup(test_ctx* t) {
    const char* case_name = (t && t->case_name) ? t->case_name : "<unnamed>";

    CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0,
              "allocator leak after test %s: %zu live blocks",
              case_name,
              g_wasm_test_alloc_stats.live_blocks);
    CHECK_MSG(t, g_wasm_test_alloc_stats.mismatches == 0,
              "allocator misuse in test %s: %zu frees or resizes with the wrong size or alignment",
              case_name,
              g_wasm_test_alloc_stats.mismatches);
    g_wasm_test_alloc_stats.mismatches = 0;
    g_wasm_test_alloc_fail_countdown = 0;
}

#define WASM_TEST_GC_I31_TAG ((uintptr_t)1u)

int wasm_test_value_is_i31ref(const wasm_value_t* value) {
    return value && value->type == WASM_TYPE_I31REF && (value->of.gc_ref & WASM_TEST_GC_I31_TAG) != 0;
}

int32_t wasm_test_gc_i31_get_s(uintptr_t bits) {
    uint32_t raw = (uint32_t)(bits >> 1) & 0x7FFFFFFFu;

    if ((raw & 0x40000000u) != 0) raw |= 0x80000000u;
    return (int32_t)raw;
}

void emit(wasm_builder_t* b, uint8_t byte) {
    b->buf[b->len++] = byte;
}

void emit_bytes(wasm_builder_t* b, const uint8_t* data, uint32_t len) {
    memcpy(b->buf + b->len, data, len);
    b->len += len;
}

void emit_leb128_u32(wasm_builder_t* b, uint32_t val) {
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

void emit_leb128_i32(wasm_builder_t* b, int32_t val) {
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

void emit_leb128_i64(wasm_builder_t* b, int64_t val) {
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

void emit_memarg(wasm_builder_t* b,
                 uint32_t align_log2,
                 int has_memory_index,
                 uint32_t memory_index,
                 uint64_t offset) {
    emit_leb128_u32(b, align_log2 | (has_memory_index ? 0x40u : 0u));
    if (has_memory_index) emit_leb128_u32(b, memory_index);
    emit_leb128_u64(b, offset);
}

void emit_memory_type(wasm_builder_t* b,
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

/* Emit a section: id + size + content */
void emit_section(wasm_builder_t* b, uint8_t id, const uint8_t* content, uint32_t len) {
    emit(b, id);
    emit_leb128_u32(b, len);
    emit_bytes(b, content, len);
}

void emit_export_func(wasm_builder_t* b, const char* name, uint32_t index) {
    uint32_t len = (uint32_t)strlen(name);

    emit_leb128_u32(b, len);
    emit_bytes(b, (const uint8_t*)name, len);
    emit(b, 0x00);
    emit_leb128_u32(b, index);
}

void emit_export_kind(wasm_builder_t* b, const char* name, uint8_t kind, uint32_t index) {
    uint32_t len = (uint32_t)strlen(name);

    emit_leb128_u32(b, len);
    emit_bytes(b, (const uint8_t*)name, len);
    emit(b, kind);
    emit_leb128_u32(b, index);
}

#if WASM_ENABLE_PLATFORM
void emit_import_func(wasm_builder_t* b, const char* module, const char* name, uint32_t type_index) {
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

void emit_header(wasm_builder_t* b) {
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

void emit_single_tag_section(wasm_builder_t* b, uint32_t type_index) {
    wasm_builder_t sec = { 0 };

    emit_leb128_u32(&sec, 1);
    emit(&sec, 0x00);
    emit_leb128_u32(&sec, type_index);
    emit_section(b, 13, sec.buf, sec.len);
}

void wasm_test_expect_u8x16(test_ctx* t,
                            const wasm_value_t* actual,
                            const uint8_t expected[16],
                            const char* label) {
    uint32_t lane;

    for (lane = 0; lane < 16; lane++) {
        CHECK_MSG(t,
                  actual->of.v128[lane] == expected[lane],
                  "%s lane %u: expected %u, got %u",
                  label,
                  (unsigned)lane,
                  (unsigned)expected[lane],
                  (unsigned)actual->of.v128[lane]);
    }
}

int host_print_called = 0;

int32_t host_print_value = 0;

wasm_error_t host_print(wasm_module_t* mod,
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

wasm_error_t wasm_test_host_add(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    results[0] = wasm_i32(args[0].of.i32 + args[1].of.i32);
    return WASM_OK;
}

wasm_module_t* wasm_test_load_sections(wasm_runtime_t* rt, const uint8_t* sections, uint32_t len) {
    wasm_builder_t mod = { 0 };

    emit_header(&mod);
    emit_bytes(&mod, sections, len);
    return wasm_load(rt, mod.buf, mod.len);
}

wasm_module_t* wasm_test_load_cont(wasm_runtime_t* rt, const uint8_t* sections, uint32_t len) {
    return wasm_test_load_sections(rt, sections, len);
}

#if defined(WASM_TEST_THREADS)
void wasm_test_interrupt_later(void* arg) {
#if defined(_WIN32)
    Sleep(20);
#else
    struct timespec delay = { 0, 20000000L };

    nanosleep(&delay, NULL);
#endif
    wasm_runtime_interrupt((wasm_runtime_t*)arg);
}
#endif

/* Runs export name with fuel units per step, resuming until it finishes; returns the step count. */
uint32_t wasm_test_run_in_fuel_steps(test_ctx* t, wasm_module_t* m, const char* name,
                                     const wasm_value_t* args, uint32_t num_args,
                                     uint64_t fuel, int grow_fuel, wasm_value_t* result) {
    wasm_runtime_t* rt = wasm_module_runtime(m);
    uint32_t steps = 0;
    wasm_error_t err;

    wasm_runtime_set_fuel(rt, fuel);
    err = wasm_call(m, name, args, num_args, result, 1);
    while (err == WASM_ERR_OUT_OF_FUEL && steps < 1000000u) {
        CHECK(t, wasm_runtime_suspended(rt) == WASM_ERR_OUT_OF_FUEL);
        if (grow_fuel) fuel *= 2u;
        wasm_runtime_set_fuel(rt, fuel);
        err = wasm_runtime_resume(rt, NULL, 0, result, 1);
        steps++;
    }
    CHECK_MSG(t, err == WASM_OK, "%s stopped with %s: %s", name, wasm_error_string(err), rt->error_msg);
    CHECK(t, wasm_runtime_suspended(rt) == WASM_OK);
    wasm_runtime_disable_fuel(rt);
    return steps;
}

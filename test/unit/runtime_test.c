#include "support.h"

TEST(test_runtime_config_defaults) {
    wasm_runtime_t rt;
    wasm_config_t cfg;

    wasm_config_default(&cfg);
    CHECK_MSG(t, cfg.max_stack_values == WASM_MAX_STACK,
              "expected default max_stack_values %u, got %u",
              (unsigned)WASM_MAX_STACK,
              (unsigned)cfg.max_stack_values);
    CHECK_MSG(t, cfg.max_call_depth == WASM_MAX_CALL_DEPTH,
              "expected default max_call_depth %u, got %u",
              (unsigned)WASM_MAX_CALL_DEPTH,
              (unsigned)cfg.max_call_depth);
    CHECK_MSG(t, cfg.max_labels == WASM_MAX_LABELS,
              "expected default max_labels %u, got %u",
              (unsigned)WASM_MAX_LABELS,
              (unsigned)cfg.max_labels);
    CHECK_MSG(t, cfg.initial_gc_heap_size == (size_t)WASM_GC_HEAP_SIZE,
              "expected default GC heap size %zu, got %zu",
              (size_t)WASM_GC_HEAP_SIZE,
              cfg.initial_gc_heap_size);
    CHECK_MSG(t, cfg.frame_arena_size == (size_t)WASM_FRAME_ARENA_SIZE,
              "expected default frame arena size %zu, got %zu",
              (size_t)WASM_FRAME_ARENA_SIZE,
              cfg.frame_arena_size);
    CHECK_MSG(t, cfg.lazy_validation == 0,
              "expected default lazy_validation 0, got %d",
              cfg.lazy_validation);

    wasm_init(&rt);
    CHECK_MSG(t, rt.main_stack.max_stack == cfg.max_stack_values,
              "expected runtime max_stack %u, got %u",
              (unsigned)cfg.max_stack_values,
              (unsigned)rt.main_stack.max_stack);
    CHECK_MSG(t, rt.main_stack.max_call_frames == cfg.max_call_depth,
              "expected runtime max_call_frames %u, got %u",
              (unsigned)cfg.max_call_depth,
              (unsigned)rt.main_stack.max_call_frames);
    CHECK_MSG(t, rt.max_labels == cfg.max_labels,
              "expected runtime max_labels %u, got %u",
              (unsigned)cfg.max_labels,
              (unsigned)rt.max_labels);
    CHECK_MSG(t, rt.main_stack.stack != NULL, "%s", "expected value stack allocation");
    CHECK_MSG(t, rt.main_stack.call_frames != NULL, "%s", "expected call frame allocation");
    CHECK_MSG(t, rt.backtrace_func_indices != NULL, "%s", "expected backtrace function buffer");
    CHECK_MSG(t, rt.backtrace_offsets != NULL, "%s", "expected backtrace offset buffer");
    CHECK_MSG(t, rt.gc_heap_size == cfg.initial_gc_heap_size,
              "expected GC heap size %zu, got %zu",
              cfg.initial_gc_heap_size,
              rt.gc_heap_size);
    CHECK_MSG(t, rt.main_stack.frame_arena_size == cfg.frame_arena_size,
              "expected frame arena size %zu, got %zu",
              cfg.frame_arena_size,
              rt.main_stack.frame_arena_size);
    CHECK_MSG(t, rt.lazy_validation == 0,
              "expected runtime lazy_validation 0, got %d",
              rt.lazy_validation);
    wasm_destroy(&rt);
}

TEST(test_runtime_config_custom_allocations) {
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
    CHECK_MSG(t, rt.main_stack.max_stack == 32, "expected max_stack 32, got %u", (unsigned)rt.main_stack.max_stack);
    CHECK_MSG(t, rt.main_stack.max_call_frames == 4,
              "expected max_call_frames 4, got %u",
              (unsigned)rt.main_stack.max_call_frames);
    CHECK_MSG(t, rt.max_labels == 8, "expected max_labels 8, got %u", (unsigned)rt.max_labels);
    CHECK_MSG(t, rt.gc_heap_size == 256, "expected gc_heap_size 256, got %zu", rt.gc_heap_size);
    CHECK_MSG(t, rt.main_stack.frame_arena_size == 64,
              "expected frame_arena_size 64, got %zu",
              rt.main_stack.frame_arena_size);
    CHECK_MSG(t, rt.lazy_validation == 1,
              "expected lazy_validation 1, got %d",
              rt.lazy_validation);
    CHECK_MSG(t, rt.main_stack.stack != NULL, "%s", "expected configured value stack allocation");
    CHECK_MSG(t, rt.main_stack.call_frames != NULL, "%s", "expected configured call frame allocation");
    CHECK_MSG(t, rt.backtrace_func_indices != NULL, "%s", "expected configured backtrace function buffer");
    CHECK_MSG(t, rt.backtrace_offsets != NULL, "%s", "expected configured backtrace offset buffer");
    wasm_destroy(&rt);
}

TEST(test_runtime_config_rejects_zero_limits) {
    wasm_runtime_t rt;
    wasm_config_t cfg;
    wasm_error_t err;

    memset(&rt, 0, sizeof(rt));
    wasm_config_default(&cfg);
    cfg.max_stack_values = 0;
    err = wasm_test_init_raw(&rt, &cfg);
    CHECK_MSG(t, err == WASM_ERR_MALFORMED,
              "expected WASM_ERR_MALFORMED for zero stack limit, got %d",
              (int)err);
    CHECK_MSG(t, strstr(rt.error_msg, "invalid runtime config") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);

    memset(&rt, 0, sizeof(rt));
    wasm_config_default(&cfg);
    cfg.max_call_depth = 0;
    err = wasm_test_init_raw(&rt, &cfg);
    CHECK_MSG(t, err == WASM_ERR_MALFORMED,
              "expected WASM_ERR_MALFORMED for zero call depth, got %d",
              (int)err);
    CHECK_MSG(t, strstr(rt.error_msg, "invalid runtime config") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);

    memset(&rt, 0, sizeof(rt));
    wasm_config_default(&cfg);
    cfg.max_labels = 0;
    err = wasm_test_init_raw(&rt, &cfg);
    CHECK_MSG(t, err == WASM_ERR_MALFORMED,
              "expected WASM_ERR_MALFORMED for zero label limit, got %d",
              (int)err);
    CHECK_MSG(t, strstr(rt.error_msg, "invalid runtime config") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);

    CHECK(t, wasm_runtime_new(&wasm_test_allocator, &cfg) == NULL);

    wasm_config_default(&cfg);
    cfg.features = WASM_FEATURES_ALL | ((uint64_t)1 << 63);
    CHECK(t, wasm_runtime_new(&wasm_test_allocator, &cfg) == NULL);
    cfg.features = WASM_FEATURES_STANDARD;
    {
        wasm_runtime_t* standard = wasm_runtime_new(&wasm_test_allocator, &cfg);
        CHECK(t, standard != NULL && wasm_runtime_features(standard) == WASM_FEATURES_STANDARD);
        wasm_runtime_free(standard);
    }
    {
        wasm_runtime_t* all = wasm_runtime_new(&wasm_test_allocator, NULL);
        CHECK(t, all != NULL && wasm_runtime_features(all) == WASM_FEATURES_ALL);
        wasm_runtime_free(all);
    }
}

TEST(test_runtime_limits_memory_and_tables) {
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
    CHECK(t, config.max_memory_bytes == 0u);
    CHECK(t, config.max_table_elements == 10000000u);
    config.max_memory_bytes = 4u * WASM_PAGE_SIZE;
    config.max_table_elements = 8u;
    wasm_init_config(&rt, &config);

    m = wasm_load(&rt, big_memory_bytes, sizeof(big_memory_bytes));
    CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_OOM && strstr(rt.error_msg, "limit"),
              "expected a 5-page memory to exceed a 4-page limit, got: %s", rt.error_msg);
    wasm_module_free(m);
    m = wasm_load(&rt, big_table_bytes, sizeof(big_table_bytes));
    CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_OOM && strstr(rt.error_msg, "limit"),
              "expected a 9-element table to exceed an 8-element limit, got: %s", rt.error_msg);
    wasm_module_free(m);

    m = wasm_load(&rt, grow_bytes, sizeof(grow_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        arg = wasm_i32(2);
        err = wasm_call(m, "grow_mem", &arg, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, 2);
        arg = wasm_i32(1);
        err = wasm_call(m, "grow_mem", &arg, 1, &result, 1);
        WASM_CHECK_OK(t, err);
        WASM_CHECK_I32(t, result.of.i32, -1);
        CHECK(t, wasm_memory_grow(m, 0, 1) == -1);
        CHECK(t, wasm_memory_size(m, 0) == 4u * WASM_PAGE_SIZE);

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
    CHECK(t, config.max_total_memory_bytes == 0u);
    config.max_total_memory_bytes = 5u * WASM_PAGE_SIZE;
    wasm_init_config(&rt, &config);
    {
        wasm_module_t* first = wasm_load(&rt, grow_bytes, sizeof(grow_bytes));
        wasm_module_t* second = wasm_load(&rt, grow_bytes, sizeof(grow_bytes));

        CHECK_MSG(t, first != NULL && second != NULL, "%s", rt.error_msg);
        m = wasm_load(&rt, grow_bytes, sizeof(grow_bytes));
        CHECK_MSG(t, m == NULL && rt.last_error == WASM_ERR_OOM && strstr(rt.error_msg, "runtime's limit"),
                  "expected a third instance to exceed the runtime limit, got: %s", rt.error_msg);
        CHECK(t, wasm_memory_grow(first, 0, 1) == 2);
        CHECK(t, wasm_memory_grow(first, 0, 1) == -1);
        wasm_module_free(second);
        CHECK(t, wasm_memory_grow(first, 0, 2) == 3);
        wasm_module_free(first);
    }
    wasm_destroy(&rt);
}

TEST(test_allocator_contract) {
    const wasm_allocator_t* libc = wasm_libc_allocator();
    wasm_allocator_t missing = wasm_test_allocator;
    wasm_runtime_t* rt;
    size_t align;

    CHECK(t, libc && libc->alloc && libc->resize && libc->free);
    for (align = 1; align <= 4096u; align *= 2u) {
        uint8_t* p = (uint8_t*)libc->alloc(libc->ctx, 3u, align);
        uint8_t* q;

        CHECK_MSG(t, p && ((uintptr_t)p & (align - 1u)) == 0, "alloc misaligned for %zu", align);
        if (!p) continue;
        memcpy(p, "abc", 3u);
        q = (uint8_t*)libc->resize(libc->ctx, p, 3u, 5000u, align);
        CHECK_MSG(t, q && ((uintptr_t)q & (align - 1u)) == 0 && memcmp(q, "abc", 3u) == 0,
                  "resize misaligned or lost data for %zu", align);
        libc->free(libc->ctx, q ? q : p, q ? 5000u : 3u, align);
    }

    CHECK(t, wasm_runtime_new(NULL, NULL) == NULL);
    missing.resize = NULL;
    CHECK(t, wasm_runtime_new(&missing, NULL) == NULL);
    CHECK(t, wasm_memory_new(&missing, 1, 1, 0) == NULL);
    CHECK(t, wasm_table_new(&missing, WASM_TYPE_FUNCREF, 1, 1) == NULL);
    rt = wasm_runtime_new(libc, NULL);
    CHECK(t, rt != NULL && wasm_runtime_allocator(rt)->alloc == libc->alloc);
    wasm_runtime_free(rt);
}

TEST(test_allocation_failures_unwind_without_leaks) {
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
        wasm_value_t object = wasm_ref_null(WASM_TYPE_STRUCTREF);
        int32_t fmt_result = 0;
        wasm_error_t err = WASM_ERR_OOM;

        g_wasm_test_alloc_fail_countdown = fail_at;
        rt = wasm_runtime_new(&wasm_test_allocator, NULL);
        if (rt && wasm_bind_host_func(rt, "env", "add", "ii(i)", wasm_test_host_add, NULL) == WASM_OK) {
            compiled = wasm_compile(rt, module_bytes, sizeof(module_bytes));
            m = compiled ? wasm_instantiate(rt, compiled, NULL) : NULL;
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
        CHECK_MSG(t, g_wasm_test_alloc_stats.live_blocks == 0, "%zu blocks leaked when allocation %zu failed",
                  g_wasm_test_alloc_stats.live_blocks, fail_at);
        if (g_wasm_test_alloc_stats.live_blocks != 0) break;
    }
    CHECK_MSG(t, completed, "%s", "expected the run to succeed once no allocation fails");
}

TEST(test_runtime_interrupt_stops_loops_calls_and_waits) {
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
    m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    wasm_runtime_interrupt(&rt);
    CHECK(t, wasm_call(m, "call", NULL, 0, &result, 1) == WASM_ERR_INTERRUPTED);
    CHECK_MSG(t, strstr(rt.error_msg, "interrupted") != NULL, "%s", rt.error_msg);
    WASM_CHECK_OK(t, wasm_call(m, "call", NULL, 0, &result, 1));
    CHECK(t, result.of.i32 == 7);

#if defined(WASM_TEST_THREADS)
    {
        static const char* const exports[] = { "spin", "wait" };
        uint32_t i;

        for (i = 0; i < 2u; i++) {
            wasm_test_thread_start_t start;
            wasm_test_thread_t thread;

            start.fn = wasm_test_interrupt_later;
            start.arg = &rt;
            CHECK(t, wasm_test_thread_spawn(&thread, &start));
            CHECK_MSG(t, wasm_call(m, exports[i], NULL, 0, &result, i) == WASM_ERR_INTERRUPTED,
                      "expected %s to be interrupted: %s", exports[i], rt.error_msg);
            wasm_test_thread_join(thread);
            wasm_runtime_cancel(&rt);
        }
    }
#endif

    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module
     (type $f (func)) (type $c (cont $f))
     (func $nop) (elem declare func $nop)
     (func (export "run") (param $n i32)
       (loop $l (resume $c (cont.new $c (ref.func $nop)))
         (br_if $l (local.tee $n (i32.sub (local.get $n) (i32.const 1))))))
     (func (export "drop") (param $n i32)
       (loop $l (drop (cont.new $c (ref.func $nop)))
         (br_if $l (local.tee $n (i32.sub (local.get $n) (i32.const 1))))))) */
static const uint8_t wasm_test_runtime_cont_churn_sections[] = {
    0x01, 0x0A, 0x03, 0x60, 0x00, 0x00, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x03, 0x04, 0x03, 0x00,
    0x02, 0x02, 0x07, 0x0E, 0x02, 0x03, 0x72, 0x75, 0x6E, 0x00, 0x01, 0x04, 0x64, 0x72, 0x6F, 0x70,
    0x00, 0x02, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x2E, 0x03, 0x02, 0x00, 0x0B, 0x15,
    0x00, 0x03, 0x40, 0xD2, 0x00, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x22,
    0x00, 0x0D, 0x00, 0x0B, 0x0B, 0x13, 0x00, 0x03, 0x40, 0xD2, 0x00, 0xE0, 0x01, 0x1A, 0x20, 0x00,
    0x41, 0x01, 0x6B, 0x22, 0x00, 0x0D, 0x00, 0x0B, 0x0B,
};

TEST(test_runtime_collections_count_unconsumed_continuations) {
    uint32_t retirements = (uint32_t)(5000u / ((uint64_t)WASM__CONT_GEN_MAX + 1u));
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, wasm_test_runtime_cont_churn_sections,
                                sizeof(wasm_test_runtime_cont_churn_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    arg = wasm_i32(5000);
    WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, NULL, 0));
    CHECK_MSG(t, rt.handle_gc_threshold == 0 && rt.conts.count <= 1u + retirements, "threshold %u, %u entries",
              (unsigned)rt.handle_gc_threshold, (unsigned)rt.conts.count);
    WASM_CHECK_OK(t, wasm_call(m, "drop", &arg, 1, NULL, 0));
    CHECK_MSG(t, rt.handle_gc_threshold != 0, "%s", "expected unconsumed continuations to trigger a collection");
    CHECK_MSG(t, rt.conts.live < 1024u && rt.conts.count <= 1025u + retirements, "%u live, %u entries",
              (unsigned)rt.conts.live, (unsigned)rt.conts.count);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case runtime_cases[] = {
    { "runtime config: defaults initialize dynamic runtime capacities", test_runtime_config_defaults },
    { "runtime config: custom sizes allocate stack, frames, and buffers", test_runtime_config_custom_allocations },
    { "allocator: libc alignment and invalid allocators", test_allocator_contract },
    { "allocator: every failing allocation unwinds without leaks", test_allocation_failures_unwind_without_leaks },
    { "runtime config: zero limits are rejected", test_runtime_config_rejects_zero_limits },
    { "runtime config: memory and table sizes are limited", test_runtime_limits_memory_and_tables },
    { "interrupt: stops loops, calls, and atomic waits from another thread", test_runtime_interrupt_stops_loops_calls_and_waits },
    { "runtime: consumed continuations never trigger collections, unconsumed ones do", test_runtime_collections_count_unconsumed_continuations },
};
const test_suite runtime_suite = { "runtime", runtime_cases, COUNTOF(runtime_cases) };

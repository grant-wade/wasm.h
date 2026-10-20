#include "../support.h"

TEST(test_runtime_operand_stack_overflow_traps_at_call) {
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
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }

    arg = wasm_i32(10);
    WASM_CHECK_OK(t, wasm_call(m, "rec", &arg, 1, &result, 1));
    CHECK(t, result.of.i32 == 11);
    arg = wasm_i32(100);
    CHECK(t, wasm_call(m, "rec", &arg, 1, &result, 1) == WASM_ERR_STACK_OVERFLOW);
    CHECK(t, rt.exec->sp == 0 && rt.exec->call_frame_sp == 0);
    arg = wasm_i32(5);
    WASM_CHECK_OK(t, wasm_call(m, "rec", &arg, 1, &result, 1));
    CHECK(t, result.of.i32 == 6);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_frame_arena_allocator) {
    wasm_runtime_t rt;
    wasm__exec_stack_t s;
    wasm__arena_mark_t start;
    wasm__arena_mark_t middle;
    void* first;
    void* second;
    void* third;
    void* big;

    memset(&rt, 0, sizeof(rt));
    memset(&s, 0, sizeof(s));
    rt.allocator = wasm_test_allocator;

    WASM_CHECK_OK(t, wasm__arena_init(&rt, &s, 64));
    CHECK_MSG(t, s.frame_arena != NULL, "%s", "expected a first arena block");
    CHECK_MSG(t, s.frame_arena_size == 64, "expected block size 64, got %zu", s.frame_arena_size);
    start = wasm__arena_mark(&s);

    first = wasm__arena_alloc(&rt, &s, 1);
    second = wasm__arena_alloc(&rt, &s, 8);
    CHECK_MSG(t, first != NULL && second != NULL, "%s", "expected small allocations to succeed");
    CHECK_MSG(t, ((uintptr_t)second - (uintptr_t)first) % 16u == 0, "%s", "expected aligned allocations");
    middle = wasm__arena_mark(&s);

    big = wasm__arena_alloc(&rt, &s, 1000);
    CHECK_MSG(t, big != NULL, "%s", "expected an oversized allocation to add a block");
    CHECK(t, s.frame_arena != start.chunk && s.frame_arena->prev == start.chunk);
    memset(big, 0xAB, 1000);

    wasm__arena_reset(&rt, &s, middle);
    CHECK(t, s.frame_arena == start.chunk && rt.frame_arena_spare != NULL);
    third = wasm__arena_alloc(&rt, &s, 16);
    CHECK_MSG(t, third != NULL && third != first && third != second, "%s",
              "expected allocation after the middle mark");
    CHECK(t, wasm__arena_alloc(&rt, &s, 1000) == big);

    wasm__arena_reset(&rt, &s, start);
    CHECK(t, wasm__arena_alloc(&rt, &s, 1) == first);

    wasm__arena_destroy(&rt, &s);
    wasm__arena_free_spare(&rt);
    CHECK_MSG(t, s.frame_arena == NULL && rt.frame_arena_spare == NULL, "%s",
              "expected arena destroy to free every block");
}

TEST(test_runtime_grows_frame_arena_for_execution) {
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
        emit_leb128_u32(&body, 4);
        emit(&body, 0x7E);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_config_default(&cfg);
    cfg.max_call_depth = 3;
    cfg.frame_arena_size = 16;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    /* The frame outgrows the 16-byte block, so the call adds a block and drops it on return. */
    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 7);
    CHECK(t, rt.exec->frame_arena != NULL && rt.exec->frame_arena->prev == NULL && rt.exec->frame_arena->used == 0u);
    CHECK(t, rt.frame_arena_spare != NULL && rt.frame_arena_spare->size > 16u);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_stack_cases[] = {
    { "runtime: operand stack overflow traps at the call that needs the space", test_runtime_operand_stack_overflow_traps_at_call },
    { "runtime: frame arena alloc/reset/destroy are well-formed", test_frame_arena_allocator },
    { "runtime: top-level execution grows the frame arena for validated modules", test_runtime_grows_frame_arena_for_execution },
};
const test_suite exec_stack_suite = { "exec.stack", exec_stack_cases, COUNTOF(exec_stack_cases) };

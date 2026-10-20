#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/exec/translate.h"
#endif

TEST(test_prefixed_opcode_in_dead_branch) {
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
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
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

TEST(test_control_target_scan_accepts_signed_i32_const_leb) {
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
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
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

/* Counts calls to host.tick, which returns arg * 3 + 1; it interrupts the runtime on call
 * interrupt_at and yields on every multiple of yield_every. */
typedef struct wasm_test_tick_t {
    uint32_t calls;
    uint32_t interrupt_at;
    uint32_t yield_every;
    int32_t last;
} wasm_test_tick_t;

static wasm_error_t wasm_test_tick(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                   wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_test_tick_t* tick = (wasm_test_tick_t*)userdata;

    (void)num_args;
    (void)num_results;
    tick->calls++;
    tick->last = (int32_t)((uint32_t)args[0].of.i32 * 3u + 1u);
    results[0] = wasm_i32(tick->last);
    if (tick->calls == tick->interrupt_at) wasm_runtime_interrupt(wasm_module_runtime(mod));
    if (tick->yield_every && tick->calls % tick->yield_every == 0) return WASM_YIELD;
    return WASM_OK;
}

/* Records each fused opcode at an instruction start of m's translated functions in seen. */
static void wasm_test_note_fused_ops(const wasm_module_t* m, uint8_t* seen) {
    uint32_t i;
    uint32_t j;

    for (i = 0; i < m->num_funcs; i++) {
        const wasm_func_t* func = &m->funcs[i];

        for (j = 0; func->ops && j < func->num_op_offsets; j++) {
            uint32_t op = func->ops[func->op_offsets[j * 2u]];

            if (op >= WASM__OP_FUSED && op < WASM__OP_COUNT) seen[op - WASM__OP_FUSED] = 1;
        }
    }
}

/* Byte offsets a call reported at its fuel stops, in order. */
typedef struct wasm_test_fuel_trace_t {
    uint32_t* offsets;
    uint32_t count;
    uint32_t cap;
} wasm_test_fuel_trace_t;

/* Calls name in fuel steps of step units, resuming every fuel stop and answering yields with
 * tick->last, collecting garbage at every stop if collect and appending each fuel stop's offset
 * to trace if not NULL; returns the fuel the call used. */
static uint64_t wasm_test_fused_call_in_steps(test_ctx* t, wasm_module_t* m, const char* name,
                                              const wasm_value_t* args, uint32_t num_args, uint64_t step,
                                              wasm_test_tick_t* tick, int collect, wasm_test_fuel_trace_t* trace,
                                              wasm_value_t* result) {
    wasm_runtime_t* rt = wasm_module_runtime(m);
    uint64_t used = 0;
    uint32_t stops = 0;
    wasm_error_t err;

    wasm_runtime_set_fuel(rt, step);
    err = wasm_call(m, name, args, num_args, result, 1);
    while ((err == WASM_ERR_OUT_OF_FUEL || err == WASM_YIELD) && stops++ < 10000000u) {
        wasm_value_t host_result = wasm_i32(tick ? tick->last : 0);
        uint32_t offset = 0;

        if (collect) wasm_gc_collect(rt);
        if (err == WASM_YIELD) {
            err = wasm_runtime_resume(rt, &host_result, 1, result, 1);
            continue;
        }
        if (trace) {
            WASM_CHECK_OK(t, wasm_runtime_call_frame(rt, 0, NULL, NULL, &offset));
            if (trace->count == trace->cap) {
                trace->cap = trace->cap ? trace->cap * 2u : 1024u;
                trace->offsets = (uint32_t*)realloc(trace->offsets, trace->cap * sizeof(uint32_t));
                if (!trace->offsets) abort();
            }
            trace->offsets[trace->count++] = offset;
        }
        used += step;
        wasm_runtime_set_fuel(rt, step);
        err = wasm_runtime_resume(rt, NULL, 0, result, 1);
    }
    CHECK_MSG(t, err == WASM_OK, "%s in steps of %u stopped with %s: %s", name, (unsigned)step,
              wasm_error_string(err), rt->error_msg);
    used += step - wasm_runtime_fuel(rt);
    wasm_runtime_disable_fuel(rt);
    return used;
}

/* Checks that name returns the same result unmetered, under one budget, and in fuel steps of 1
 * to 5 units, where steps of 1 run every fused op one instruction at a time, and that the metered
 * runs use the same fuel and report the same instruction at each fuel stop. */
static void wasm_test_fused_matches_unfused(test_ctx* t, wasm_module_t* m, const char* name,
                                            const wasm_value_t* args, uint32_t num_args, wasm_test_tick_t* tick,
                                            int collect, wasm_value_t* out) {
    wasm_runtime_t* rt = wasm_module_runtime(m);
    wasm_test_fuel_trace_t unfused = { NULL, 0, 0 };
    wasm_value_t expected;
    wasm_value_t result;
    uint64_t used;
    uint64_t step;

    memset(&expected, 0, sizeof(expected));
    if (tick) tick->calls = 0;
    used = wasm_test_fused_call_in_steps(t, m, name, args, num_args, (uint64_t)1 << 40, tick, collect, NULL,
                                         &expected);
    if (!tick || !tick->yield_every) {
        if (tick) tick->calls = 0;
        memset(&result, 0, sizeof(result));
        WASM_CHECK_OK(t, wasm_call(m, name, args, num_args, &result, 1));
        CHECK_MSG(t, memcmp(&result.of, &expected.of, sizeof(result.of)) == 0, "%s: unmetered result differs",
                  name);
    }
    for (step = 1; step <= 5; step++) {
        wasm_test_fuel_trace_t trace = { NULL, 0, 0 };
        uint64_t step_used;
        uint32_t i;

        if (tick) tick->calls = 0;
        memset(&result, 0, sizeof(result));
        step_used = wasm_test_fused_call_in_steps(t, m, name, args, num_args, step, tick, collect,
                                                  step == 1 ? &unfused : &trace, &result);
        CHECK_MSG(t, memcmp(&result.of, &expected.of, sizeof(result.of)) == 0,
                  "%s: result in fuel steps of %u differs", name, (unsigned)step);
        CHECK_MSG(t, step_used == used, "%s: fuel steps of %u used %llu units, one budget used %llu", name,
                  (unsigned)step, (unsigned long long)step_used, (unsigned long long)used);
        for (i = 0; step > 1 && i < trace.count; i++) {
            uint64_t at = (uint64_t)(i + 1u) * step - 1u;

            if (at >= unfused.count) break;
            if (trace.offsets[i] != unfused.offsets[at]) {
                CHECK_MSG(t, 0, "%s: fuel stop %u in steps of %u reports offset %u, not %u", name, (unsigned)i,
                          (unsigned)step, (unsigned)trace.offsets[i], (unsigned)unfused.offsets[at]);
                break;
            }
        }
        free(trace.offsets);
    }
    free(unfused.offsets);
    CHECK(t, rt->suspended == WASM_OK);
    if (out) *out = expected;
}

static void wasm_test_emit_local_op(wasm_builder_t* b, uint8_t op, uint32_t index) {
    emit(b, op);
    emit_leb128_u32(b, index);
}

static void wasm_test_emit_i32_const(wasm_builder_t* b, int32_t value) {
    emit(b, 0x41);
    emit_leb128_i32(b, value);
}

static void wasm_test_emit_i64_const(wasm_builder_t* b, int64_t value) {
    emit(b, 0x42);
    emit_leb128_i64(b, value);
}

/* Emits local h = (top ^ h * 31), for i64 if wide. */
static void wasm_test_emit_mix(wasm_builder_t* b, uint32_t h, int wide) {
    wasm_test_emit_local_op(b, 0x20, h);
    if (wide)
        wasm_test_emit_i64_const(b, 31);
    else
        wasm_test_emit_i32_const(b, 31);
    emit(b, wide ? 0x7E : 0x6C);
    emit(b, wide ? 0x85 : 0x73);
    wasm_test_emit_local_op(b, 0x21, h);
}

/* Builds into mod a module exporting "f" of type (param type type) (result result) whose body
 * is the local declarations locals followed by code. */
static void wasm_test_emit_fused_module(wasm_builder_t* mod, uint8_t type, uint8_t result, const uint8_t* locals,
                                        uint32_t locals_len, const wasm_builder_t* code) {
    static const uint8_t funcs[] = { 1, 0 };
    wasm_builder_t sec;

    mod->len = 0;
    emit_header(mod);
    sec.len = 0;
    emit(&sec, 1);
    emit(&sec, 0x60);
    emit(&sec, 2);
    emit(&sec, type);
    emit(&sec, type);
    emit(&sec, 1);
    emit(&sec, result);
    emit_section(mod, 1, sec.buf, sec.len);
    emit_section(mod, 3, funcs, sizeof(funcs));
    sec.len = 0;
    emit(&sec, 1);
    emit_export_func(&sec, "f", 0);
    emit_section(mod, 7, sec.buf, sec.len);
    sec.len = 0;
    emit(&sec, 1);
    emit_leb128_u32(&sec, locals_len + code->len + 1u);
    emit_bytes(&sec, locals, locals_len);
    emit_bytes(&sec, code->buf, code->len);
    emit(&sec, 0x0B);
    emit_section(mod, 10, sec.buf, sec.len);
}

/* Loads mod, checks f over every pair of args against unfused execution, and notes its fused ops. */
static void wasm_test_fused_sweep(test_ctx* t, const wasm_builder_t* mod, const wasm_value_t* values,
                                  uint32_t num_values, uint8_t* seen) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t i;
    uint32_t j;

    wasm_init(&rt);
    m = wasm_load(&rt, mod->buf, mod->len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < num_values; i++) {
        for (j = 0; j < num_values; j++) {
            wasm_value_t args[2];

            args[0] = values[i];
            args[1] = values[j];
            wasm_test_fused_matches_unfused(t, m, "f", args, 2, NULL, 0, NULL);
        }
    }
    wasm_test_note_fused_ops(m, seen);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_fused_ops_match_unfused_execution) {
    static const uint8_t i32_binops[] = { 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x6A,
                                          0x6B, 0x6C, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78 };
    static const uint8_t i64_binops[] = { 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7C,
                                          0x7D, 0x7E, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A };
    static const int64_t i64_consts[] = { -1, INT64_MIN, 65 };
    static const uint64_t f64_consts[] = { UINT64_C(0x7FF8000000000001), UINT64_C(0x8000000000000000),
                                           UINT64_C(0x3FF8000000000000), UINT64_C(0x7FF0000000000000),
                                           UINT64_C(0xFFF4000000000002) };
    static const uint8_t i32_locals[] = { 1, 3, 0x7F };
    static const uint8_t i64_locals[] = { 1, 2, 0x7E };
    static const uint8_t f64_locals[] = { 2, 1, 0x7E, 3, 0x7C };
    wasm_value_t values[6];
    wasm_builder_t code;
    wasm_builder_t mod;
    uint8_t seen[WASM__OP_COUNT - WASM__OP_FUSED];
    uint32_t i;
    uint32_t k;

    memset(seen, 0, sizeof(seen));

    code.len = 0;
    for (i = 0; i < sizeof(i32_binops); i++) {
        uint8_t op = i32_binops[i];
        int shift = op >= 0x74;
        int32_t consts[3];

        consts[0] = shift ? 33 : -1;
        consts[1] = shift ? -1 : INT32_MAX;
        consts[2] = shift ? 4 : INT32_MIN;
        for (k = 0; k < 3; k++) {
            wasm_test_emit_local_op(&code, 0x20, 0);
            wasm_test_emit_i32_const(&code, consts[k]);
            emit(&code, op);
            wasm_test_emit_mix(&code, 2, 0);
        }
        wasm_test_emit_local_op(&code, 0x20, 0);
        wasm_test_emit_local_op(&code, 0x20, 1);
        emit(&code, op);
        wasm_test_emit_mix(&code, 2, 0);
        wasm_test_emit_local_op(&code, 0x20, 1);
        wasm_test_emit_local_op(&code, 0x22, 3);
        wasm_test_emit_i32_const(&code, consts[1]);
        emit(&code, op);
        wasm_test_emit_mix(&code, 2, 0);
        wasm_test_emit_local_op(&code, 0x20, 0);
        wasm_test_emit_local_op(&code, 0x22, 3);
        wasm_test_emit_local_op(&code, 0x20, 1);
        emit(&code, op);
        wasm_test_emit_mix(&code, 2, 0);
        wasm_test_emit_local_op(&code, 0x20, 1);
        wasm_test_emit_i32_const(&code, 9);
        emit(&code, op);
        wasm_test_emit_local_op(&code, 0x21, 3);
        wasm_test_emit_local_op(&code, 0x20, 3);
        wasm_test_emit_mix(&code, 2, 0);
        wasm_test_emit_local_op(&code, 0x20, 1);
        wasm_test_emit_local_op(&code, 0x20, 0);
        emit(&code, op);
        wasm_test_emit_local_op(&code, 0x21, 3);
        wasm_test_emit_local_op(&code, 0x20, 3);
        wasm_test_emit_mix(&code, 2, 0);
        wasm_test_emit_i32_const(&code, consts[2]);
        wasm_test_emit_local_op(&code, 0x21, 3);
        wasm_test_emit_local_op(&code, 0x20, 3);
        wasm_test_emit_local_op(&code, 0x20, 0);
        emit(&code, op);
        wasm_test_emit_mix(&code, 2, 0);
        wasm_test_emit_local_op(&code, 0x20, 2);
        wasm_test_emit_local_op(&code, 0x21, 3);
        wasm_test_emit_local_op(&code, 0x20, 3);
        wasm_test_emit_local_op(&code, 0x21, 2);
        wasm_test_emit_local_op(&code, 0x20, 3);
        wasm_test_emit_mix(&code, 2, 0);
        wasm_test_emit_local_op(&code, 0x20, 2);
        wasm_test_emit_local_op(&code, 0x20, 1);
        wasm_test_emit_local_op(&code, 0x20, 0);
        emit(&code, op);
        wasm_test_emit_local_op(&code, 0x22, 3);
        wasm_test_emit_local_op(&code, 0x21, 4);
        wasm_test_emit_i32_const(&code, 5);
        emit(&code, 0x6A);
        wasm_test_emit_local_op(&code, 0x20, 4);
        emit(&code, 0x6A);
        wasm_test_emit_local_op(&code, 0x21, 2);
    }
    wasm_test_emit_local_op(&code, 0x20, 0);
    wasm_test_emit_i32_const(&code, 7);
    emit(&code, 0x6A);
    wasm_test_emit_local_op(&code, 0x22, 3);
    wasm_test_emit_local_op(&code, 0x21, 4);
    wasm_test_emit_local_op(&code, 0x20, 3);
    wasm_test_emit_local_op(&code, 0x20, 4);
    emit(&code, 0x6C);
    wasm_test_emit_local_op(&code, 0x21, 3);
    wasm_test_emit_local_op(&code, 0x20, 1);
    emit(&code, 0x69);
    wasm_test_emit_local_op(&code, 0x21, 4);
    wasm_test_emit_local_op(&code, 0x20, 4);
    wasm_test_emit_i32_const(&code, 9);
    emit(&code, 0x6A);
    wasm_test_emit_local_op(&code, 0x22, 4);
    wasm_test_emit_local_op(&code, 0x20, 3);
    emit(&code, 0x73);
    wasm_test_emit_local_op(&code, 0x20, 4);
    emit(&code, 0x6A);
    wasm_test_emit_mix(&code, 2, 0);
    wasm_test_emit_local_op(&code, 0x20, 2);
    wasm_test_emit_fused_module(&mod, 0x7F, 0x7F, i32_locals, sizeof(i32_locals), &code);
    values[0] = wasm_i32(0);
    values[1] = wasm_i32(-1);
    values[2] = wasm_i32(INT32_MAX);
    values[3] = wasm_i32(INT32_MIN);
    values[4] = wasm_i32(33);
    values[5] = wasm_i32(0x12345678);
    wasm_test_fused_sweep(t, &mod, values, 6, seen);

    code.len = 0;
    for (i = 0; i < sizeof(i64_binops); i++) {
        uint8_t op = i64_binops[i];

        for (k = 0; k < sizeof(i64_consts) / sizeof(i64_consts[0]); k++) {
            wasm_test_emit_local_op(&code, 0x20, 0);
            wasm_test_emit_i64_const(&code, i64_consts[k]);
            emit(&code, op);
            if (op <= 0x5A) emit(&code, 0xAD);
            wasm_test_emit_mix(&code, 2, 1);
            wasm_test_emit_local_op(&code, 0x20, 1);
            wasm_test_emit_local_op(&code, 0x22, 3);
            wasm_test_emit_i64_const(&code, i64_consts[k]);
            emit(&code, op);
            if (op <= 0x5A) emit(&code, 0xAD);
            wasm_test_emit_mix(&code, 2, 1);
        }
    }
    wasm_test_emit_local_op(&code, 0x20, 2);
    wasm_test_emit_fused_module(&mod, 0x7E, 0x7E, i64_locals, sizeof(i64_locals), &code);
    values[0] = wasm_i64(0);
    values[1] = wasm_i64(-1);
    values[2] = wasm_i64(INT64_MIN);
    values[3] = wasm_i64(63);
    values[4] = wasm_i64(INT64_C(0x123456789ABCDEF0));
    values[5] = wasm_i64(INT64_MAX);
    wasm_test_fused_sweep(t, &mod, values, 6, seen);

    code.len = 0;
    for (i = 0; i < 10; i++) {
        uint8_t op = (uint8_t)(0x46 + i);

        for (k = 0; k < 4; k++) {
            emit(&code, 0x02);
            emit(&code, 0x40);
            wasm_test_emit_local_op(&code, 0x20, 0);
            if (k < 2) wasm_test_emit_local_op(&code, 0x22, 3);
            if (k == 0) wasm_test_emit_i32_const(&code, -2);
            if (k == 1 || k == 3) wasm_test_emit_local_op(&code, 0x20, 1);
            if (k == 2) wasm_test_emit_i32_const(&code, 7);
            emit(&code, op);
            emit(&code, 0x0D);
            emit(&code, 0);
            wasm_test_emit_local_op(&code, 0x20, 2);
            wasm_test_emit_i32_const(&code, (int32_t)(1u << (i * 3u + k)));
            emit(&code, 0x6A);
            wasm_test_emit_local_op(&code, 0x21, 2);
            emit(&code, 0x0B);
            wasm_test_emit_local_op(&code, 0x20, 2);
            wasm_test_emit_i32_const(&code, 3);
            emit(&code, 0x6C);
            wasm_test_emit_local_op(&code, 0x21, 2);
        }
        wasm_test_emit_local_op(&code, 0x20, 1);
        wasm_test_emit_i32_const(&code, -9);
        emit(&code, op);
        emit(&code, 0x04);
        emit(&code, 0x7F);
        wasm_test_emit_i32_const(&code, (int32_t)(100 + i));
        emit(&code, 0x05);
        wasm_test_emit_i32_const(&code, (int32_t)(200 + i));
        emit(&code, 0x0B);
        wasm_test_emit_local_op(&code, 0x20, 2);
        emit(&code, 0x6A);
        wasm_test_emit_local_op(&code, 0x21, 2);
    }
    wasm_test_emit_local_op(&code, 0x20, 2);
    wasm_test_emit_fused_module(&mod, 0x7F, 0x7F, i32_locals, sizeof(i32_locals), &code);
    values[0] = wasm_i32(0);
    values[1] = wasm_i32(-1);
    values[2] = wasm_i32(7);
    values[3] = wasm_i32(-2);
    values[4] = wasm_i32(INT32_MIN);
    values[5] = wasm_i32(-9);
    wasm_test_fused_sweep(t, &mod, values, 6, seen);

    code.len = 0;
    wasm_test_emit_local_op(&code, 0x20, 0);
    emit(&code, 0xBF);
    wasm_test_emit_local_op(&code, 0x21, 3);
    wasm_test_emit_local_op(&code, 0x20, 1);
    emit(&code, 0xBF);
    wasm_test_emit_local_op(&code, 0x21, 4);
    for (i = 0; i < 4; i++) {
        uint8_t op = (uint8_t)(0xA0 + i);

        wasm_test_emit_local_op(&code, 0x20, 4);
        wasm_test_emit_local_op(&code, 0x20, 3);
        emit(&code, op);
        emit(&code, 0xBD);
        wasm_test_emit_mix(&code, 2, 1);
        wasm_test_emit_local_op(&code, 0x20, 3);
        wasm_test_emit_local_op(&code, 0x22, 5);
        wasm_test_emit_local_op(&code, 0x20, 4);
        emit(&code, op);
        emit(&code, 0xBD);
        wasm_test_emit_mix(&code, 2, 1);
        for (k = 0; k < sizeof(f64_consts) / sizeof(f64_consts[0]); k++) {
            uint32_t b;

            wasm_test_emit_local_op(&code, 0x20, 3);
            wasm_test_emit_local_op(&code, 0x22, 5);
            emit(&code, 0x44);
            for (b = 0; b < 8; b++) emit(&code, (uint8_t)(f64_consts[k] >> (b * 8u)));
            emit(&code, op);
            emit(&code, 0xBD);
            wasm_test_emit_mix(&code, 2, 1);
        }
    }
    wasm_test_emit_local_op(&code, 0x20, 2);
    wasm_test_emit_fused_module(&mod, 0x7E, 0x7E, f64_locals, sizeof(f64_locals), &code);
    values[0] = wasm_i64(INT64_C(0x7FF4000000000001));
    values[1] = wasm_i64(INT64_C(0x7FF8000000000003));
    values[2] = wasm_i64((int64_t)UINT64_C(0xFFF8000000000000));
    values[3] = wasm_i64(INT64_C(0x3FF8000000000000));
    values[4] = wasm_i64((int64_t)UINT64_C(0x8000000000000000));
    values[5] = wasm_i64(1);
    wasm_test_fused_sweep(t, &mod, values, 6, seen);

    {
        /* moves(a, b), loops(n), fmove(bits), loads(p), load8(p), load64(p), nops(n), and refs(n) run
         * local moves, five loops whose back edges are different fused branches and whose bodies call
         * host.tick, f64 moves, loads from data at 0, nested blocks, and a loop that allocates next to
         * live struct references. */
        static const uint8_t module_bytes[] = {
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x1a, 0x05, 0x5f, 0x01, 0x7f, 0x01, 0x60, 0x01, 0x7f,
            0x01, 0x7f, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x01, 0x7e, 0x01, 0x7e, 0x60, 0x01, 0x7f, 0x01, 0x7e,
            0x02, 0x0d, 0x01, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x04, 0x74, 0x69, 0x63, 0x6b, 0x00, 0x01, 0x03, 0x09, 0x08,
            0x02, 0x01, 0x03, 0x04, 0x01, 0x04, 0x01, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07, 0x40, 0x08, 0x05, 0x6d,
            0x6f, 0x76, 0x65, 0x73, 0x00, 0x01, 0x05, 0x6c, 0x6f, 0x6f, 0x70, 0x73, 0x00, 0x02, 0x05, 0x66, 0x6d, 0x6f,
            0x76, 0x65, 0x00, 0x03, 0x05, 0x6c, 0x6f, 0x61, 0x64, 0x73, 0x00, 0x04, 0x05, 0x6c, 0x6f, 0x61, 0x64, 0x38,
            0x00, 0x05, 0x06, 0x6c, 0x6f, 0x61, 0x64, 0x36, 0x34, 0x00, 0x06, 0x04, 0x6e, 0x6f, 0x70, 0x73, 0x00, 0x07,
            0x04, 0x72, 0x65, 0x66, 0x73, 0x00, 0x08, 0x0a, 0xe0, 0x04, 0x08, 0x75, 0x01, 0x03, 0x7f, 0x20, 0x00, 0x21,
            0x02, 0x20, 0x01, 0x22, 0x03, 0x21, 0x04, 0x20, 0x02, 0x20, 0x03, 0x6a, 0x21, 0x04, 0x20, 0x04, 0x20, 0x00,
            0x6a, 0x22, 0x02, 0x20, 0x03, 0x6a, 0x21, 0x03, 0x20, 0x03, 0x41, 0x7d, 0x6a, 0x21, 0x04, 0x20, 0x04, 0x41,
            0xff, 0xff, 0xff, 0xff, 0x07, 0x6a, 0x22, 0x02, 0x1a, 0x20, 0x02, 0x41, 0x0c, 0x73, 0x21, 0x03, 0x20, 0x02,
            0x20, 0x03, 0x6b, 0x21, 0x04, 0x20, 0x04, 0x21, 0x02, 0x20, 0x02, 0x20, 0x03, 0x22, 0x04, 0x20, 0x02, 0x6a,
            0x6a, 0x21, 0x03, 0x20, 0x00, 0x20, 0x03, 0x20, 0x04, 0x6c, 0x20, 0x02, 0x6c, 0x6a, 0x22, 0x02, 0x1a, 0x20,
            0x00, 0x20, 0x01, 0x22, 0x04, 0x6a, 0x22, 0x02, 0x20, 0x03, 0x20, 0x04, 0x20, 0x02, 0x6c, 0x6a, 0x6a, 0x20,
            0x00, 0x77, 0x0b, 0x8f, 0x01, 0x01, 0x03, 0x7f, 0x03, 0x40, 0x20, 0x03, 0x20, 0x01, 0x10, 0x00, 0x6a, 0x21,
            0x03, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x22, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x41, 0x00, 0x21, 0x01,
            0x03, 0x40, 0x20, 0x03, 0x20, 0x01, 0x10, 0x00, 0x73, 0x21, 0x03, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x22, 0x01,
            0x41, 0x28, 0x47, 0x0d, 0x00, 0x0b, 0x41, 0x00, 0x21, 0x01, 0x03, 0x40, 0x20, 0x03, 0x20, 0x01, 0x10, 0x00,
            0x6a, 0x21, 0x03, 0x20, 0x01, 0x41, 0x03, 0x6a, 0x21, 0x01, 0x20, 0x01, 0x41, 0x32, 0x48, 0x0d, 0x00, 0x0b,
            0x20, 0x00, 0x21, 0x01, 0x03, 0x40, 0x20, 0x03, 0x20, 0x01, 0x10, 0x00, 0x6b, 0x21, 0x03, 0x20, 0x01, 0x41,
            0x7f, 0x6a, 0x21, 0x01, 0x20, 0x01, 0x20, 0x02, 0x4a, 0x0d, 0x00, 0x0b, 0x41, 0x07, 0x21, 0x01, 0x03, 0x40,
            0x20, 0x03, 0x20, 0x01, 0x10, 0x00, 0x6a, 0x21, 0x03, 0x20, 0x01, 0x41, 0x01, 0x6b, 0x22, 0x01, 0x0d, 0x00,
            0x0b, 0x20, 0x03, 0x0b, 0x23, 0x01, 0x03, 0x7c, 0x20, 0x00, 0xbf, 0x21, 0x01, 0x20, 0x01, 0x21, 0x02, 0x20,
            0x02, 0x22, 0x03, 0x21, 0x01, 0x20, 0x01, 0x20, 0x03, 0x21, 0x02, 0x21, 0x03, 0x20, 0x03, 0x20, 0x02, 0x22,
            0x01, 0x1a, 0xbd, 0x0b, 0x53, 0x01, 0x01, 0x7e, 0x20, 0x00, 0x28, 0x02, 0x00, 0xad, 0x21, 0x01, 0x20, 0x00,
            0x2a, 0x02, 0x00, 0xbc, 0xad, 0x20, 0x01, 0x42, 0x07, 0x89, 0x85, 0x21, 0x01, 0x20, 0x00, 0x35, 0x02, 0x00,
            0x20, 0x01, 0x42, 0x07, 0x89, 0x85, 0x21, 0x01, 0x20, 0x00, 0x29, 0x03, 0x08, 0x20, 0x01, 0x42, 0x07, 0x89,
            0x85, 0x21, 0x01, 0x20, 0x00, 0x2b, 0x03, 0x10, 0xbd, 0x20, 0x01, 0x42, 0x07, 0x89, 0x85, 0x21, 0x01, 0x20,
            0x00, 0x2d, 0x00, 0x03, 0xad, 0x20, 0x01, 0x42, 0x07, 0x89, 0x85, 0x21, 0x01, 0x20, 0x01, 0x0b, 0x07, 0x00,
            0x20, 0x00, 0x2d, 0x00, 0x01, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x29, 0x03, 0x08, 0x0b, 0x60, 0x01, 0x02, 0x7f,
            0x03, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x40, 0x20, 0x01, 0x41, 0x07, 0x71, 0x0e,
            0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x04, 0x00, 0x01, 0x0b, 0x20, 0x02, 0x41, 0x01, 0x6a, 0x21, 0x02, 0x0c,
            0x03, 0x0b, 0x20, 0x02, 0x41, 0x0a, 0x6a, 0x21, 0x02, 0x0b, 0x20, 0x02, 0x41, 0xe4, 0x00, 0x6a, 0x21, 0x02,
            0x0b, 0x02, 0x40, 0x02, 0x40, 0x01, 0x0b, 0x0b, 0x0b, 0x20, 0x02, 0x02, 0x40, 0x0b, 0x41, 0x03, 0x73, 0x02,
            0x40, 0x01, 0x0b, 0x21, 0x02, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x22, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b,
            0x20, 0x02, 0x0b, 0x6e, 0x03, 0x01, 0x7f, 0x02, 0x63, 0x00, 0x01, 0x7f, 0x41, 0x01, 0xfb, 0x00, 0x00, 0x21,
            0x02, 0x03, 0x40, 0x20, 0x02, 0x20, 0x01, 0x41, 0x03, 0x71, 0x20, 0x04, 0x20, 0x01, 0x6a, 0x6a, 0xfb, 0x05,
            0x00, 0x00, 0x20, 0x02, 0x21, 0x03, 0x20, 0x03, 0x22, 0x02, 0x21, 0x03, 0x20, 0x03, 0x22, 0x02, 0x20, 0x03,
            0x1a, 0x20, 0x02, 0xfb, 0x02, 0x00, 0x00, 0x20, 0x01, 0x10, 0x00, 0x6a, 0xfb, 0x00, 0x00, 0x21, 0x02, 0xfb,
            0x02, 0x00, 0x00, 0x20, 0x04, 0x6a, 0x21, 0x04, 0x20, 0x04, 0x20, 0x02, 0xfb, 0x02, 0x00, 0x00, 0x73, 0x21,
            0x04, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x22, 0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x20, 0x04, 0x20, 0x03,
            0xfb, 0x02, 0x00, 0x00, 0x6a, 0x0b, 0x0b, 0x1e, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x18, 0x01, 0x80, 0xff, 0x7f,
            0x00, 0x00, 0xc0, 0x7f, 0x01, 0x00, 0xa0, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54,
            0x32, 0x10,
        };
        static const uint64_t nan_bits[] = { UINT64_C(0x7FF0000000000001), UINT64_C(0xFFF4000000000000),
                                             UINT64_C(0x7FF8000000000000) };
        wasm_runtime_t rt;
        wasm_test_tick_t tick;
        wasm_module_t* m;
        wasm_value_t args[2];
        wasm_value_t result;
        wasm_value_t expected;
        uint32_t func_idx = 0;
        uint32_t offset = 0;
        uint32_t interrupts = 0;

        wasm_init(&rt);
        memset(&tick, 0, sizeof(tick));
        WASM_CHECK_OK(t, wasm_bind_host_func(&rt, "host", "tick", "i(i)", wasm_test_tick, &tick));
        m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
        CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
        if (!m) {
            wasm_destroy(&rt);
            return;
        }

        for (i = 0; i < 6; i++) {
            args[0] = wasm_i32(i == 0 ? INT32_MAX : (int32_t)(i * 0x1234567u));
            args[1] = wasm_i32(i == 1 ? -1 : (int32_t)i - 3);
            wasm_test_fused_matches_unfused(t, m, "moves", args, 2, NULL, 0, NULL);
        }

        args[0] = wasm_i32(30);
        wasm_test_fused_matches_unfused(t, m, "loops", args, 1, &tick, 0, &expected);
        WASM_CHECK_I32(t, (int32_t)tick.calls, 124);
        for (k = 1; k < 124; k++) {
            int last_in_loop = k == 30 || k == 70 || k == 87 || k == 117;

            tick.calls = 0;
            tick.interrupt_at = k;
            if (wasm_call(m, "loops", args, 1, &result, 1) != WASM_ERR_INTERRUPTED) continue;
            interrupts++;
            CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
            CHECK_MSG(t, tick.calls == k + (last_in_loop ? 1u : 0u), "interrupt after tick %u stopped after tick %u",
                      (unsigned)k, (unsigned)tick.calls);
            WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
            WASM_CHECK_I32(t, result.of.i32, expected.of.i32);
        }
        tick.interrupt_at = 0;
        WASM_CHECK_I32(t, (int32_t)interrupts, 123);

        for (i = 0; i < sizeof(nan_bits) / sizeof(nan_bits[0]); i++) {
            args[0] = wasm_i64((int64_t)nan_bits[i]);
            wasm_test_fused_matches_unfused(t, m, "fmove", args, 1, NULL, 0, &result);
            CHECK_MSG(t, (uint64_t)result.of.i64 == nan_bits[i], "fmove changed NaN bits %llx to %llx",
                      (unsigned long long)nan_bits[i], (unsigned long long)result.of.i64);
        }

        for (i = 0; i < 4; i++) {
            args[0] = wasm_i32((int32_t)i);
            wasm_test_fused_matches_unfused(t, m, "loads", args, 1, NULL, 0, NULL);
        }
        args[0] = wasm_i32(0);
        WASM_CHECK_OK(t, wasm_call(m, "load64", args, 1, &result, 1));
        CHECK(t, (uint64_t)result.of.i64 == UINT64_C(0xFFFFFFFF7FA00001));
        WASM_CHECK_OK(t, wasm_call(m, "load8", args, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 0x80);
        {
            static const char* const trapping[] = { "load8", "load64", "loads" };
            static const int32_t addresses[] = { 65535, 65530, 65528 };

            for (i = 0; i < 3; i++) {
                uint32_t unfused_offset = 0;
                wasm_error_t err;

                args[0] = wasm_i32(addresses[i]);
                CHECK(t, wasm_call(m, trapping[i], args, 1, &result, 1) == WASM_ERR_OUT_OF_BOUNDS);
                WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &offset));
                wasm_runtime_set_fuel(&rt, 1);
                err = wasm_call(m, trapping[i], args, 1, &result, 1);
                while (err == WASM_ERR_OUT_OF_FUEL) {
                    wasm_runtime_set_fuel(&rt, 1);
                    err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
                }
                wasm_runtime_disable_fuel(&rt);
                CHECK(t, err == WASM_ERR_OUT_OF_BOUNDS);
                WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, NULL, &unfused_offset));
                WASM_CHECK_I32(t, (int32_t)offset, (int32_t)unfused_offset);
                if (i < 2) WASM_CHECK_I32(t, (int32_t)offset, 2);
            }
            CHECK(t, offset > 2);
        }

        args[0] = wasm_i32(21);
        wasm_test_fused_matches_unfused(t, m, "nops", args, 1, NULL, 0, &result);
        WASM_CHECK_I32(t, result.of.i32, 856);

        args[0] = wasm_i32(20);
        wasm_test_fused_matches_unfused(t, m, "refs", args, 1, &tick, 1, &expected);
        tick.yield_every = 3;
        wasm_test_fused_matches_unfused(t, m, "refs", args, 1, &tick, 1, &result);
        tick.yield_every = 0;
        WASM_CHECK_I32(t, result.of.i32, expected.of.i32);

        wasm_test_note_fused_ops(m, seen);
        wasm_module_free(m);
        wasm_destroy(&rt);
    }

    {
        /* lcloads(p) folds loads of every width from p plus constants into an i64; stores(p, v) stores
         * v, its f32 bits, v * 0x123456789, and a signaling f64 NaN from locals at p and returns a
         * hash of what it stored; lc8(p), lc32(p), and lc64(p) load the byte, word, or doubleword
         * ending 2, 7, or 12 bytes past p. */
        static const uint8_t module_bytes[] = {
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0c, 0x02, 0x60, 0x01, 0x7f, 0x01, 0x7e, 0x60, 0x02,
            0x7f, 0x7f, 0x01, 0x7e, 0x03, 0x06, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x03, 0x01, 0x00, 0x01, 0x07,
            0x28, 0x05, 0x07, 0x6c, 0x63, 0x6c, 0x6f, 0x61, 0x64, 0x73, 0x00, 0x00, 0x06, 0x73, 0x74, 0x6f, 0x72, 0x65,
            0x73, 0x00, 0x01, 0x03, 0x6c, 0x63, 0x38, 0x00, 0x02, 0x04, 0x6c, 0x63, 0x33, 0x32, 0x00, 0x03, 0x04, 0x6c,
            0x63, 0x36, 0x34, 0x00, 0x04, 0x0a, 0xe8, 0x01, 0x05, 0x61, 0x01, 0x01, 0x7e, 0x20, 0x00, 0x41, 0x04, 0x6a,
            0x28, 0x02, 0x00, 0xad, 0x21, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x2a, 0x02, 0x02, 0xbc, 0xad, 0x20, 0x01,
            0x42, 0x07, 0x89, 0x85, 0x21, 0x01, 0x20, 0x00, 0x41, 0x02, 0x6a, 0x35, 0x02, 0x01, 0x20, 0x01, 0x42, 0x07,
            0x89, 0x85, 0x21, 0x01, 0x20, 0x00, 0x41, 0x08, 0x6a, 0x29, 0x03, 0x03, 0x20, 0x01, 0x42, 0x07, 0x89, 0x85,
            0x21, 0x01, 0x20, 0x00, 0x41, 0x78, 0x6a, 0x2b, 0x03, 0x10, 0xbd, 0x20, 0x01, 0x42, 0x07, 0x89, 0x85, 0x21,
            0x01, 0x20, 0x00, 0x41, 0x05, 0x6a, 0x2d, 0x00, 0x02, 0xad, 0x20, 0x01, 0x42, 0x07, 0x89, 0x85, 0x0b, 0x61,
            0x03, 0x01, 0x7d, 0x01, 0x7c, 0x01, 0x7e, 0x20, 0x01, 0xbe, 0x21, 0x02, 0x42, 0x81, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0xfa, 0xff, 0x00, 0xbf, 0x21, 0x03, 0x20, 0x01, 0xac, 0x42, 0x89, 0xcf, 0x95, 0x9a, 0x12, 0x7e,
            0x21, 0x04, 0x20, 0x00, 0x20, 0x01, 0x36, 0x02, 0x04, 0x20, 0x00, 0x20, 0x02, 0x38, 0x02, 0x08, 0x20, 0x00,
            0x20, 0x04, 0x3e, 0x02, 0x0c, 0x20, 0x00, 0x20, 0x04, 0x37, 0x03, 0x10, 0x20, 0x00, 0x20, 0x03, 0x39, 0x03,
            0x18, 0x20, 0x00, 0x29, 0x03, 0x04, 0x20, 0x00, 0x29, 0x03, 0x0c, 0x85, 0x20, 0x00, 0x29, 0x03, 0x14, 0x85,
            0x20, 0x00, 0x29, 0x03, 0x18, 0x85, 0x0b, 0x0b, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x2d, 0x00, 0x00, 0xad,
            0x0b, 0x0b, 0x00, 0x20, 0x00, 0x41, 0x02, 0x6a, 0x28, 0x02, 0x01, 0xad, 0x0b, 0x0a, 0x00, 0x20, 0x00, 0x41,
            0x04, 0x6a, 0x29, 0x03, 0x00, 0x0b, 0x0b, 0x26, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x20, 0x01, 0x80, 0xff, 0x7f,
            0x00, 0x00, 0xc0, 0x7f, 0x01, 0x00, 0xa0, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54,
            0x32, 0x10, 0x01, 0x00, 0xf4, 0x7f, 0x00, 0x00, 0xf0, 0xff,
        };
        static const int32_t loads[] = { 8, 9, 12, 100, 65512 };
        static const char* const ends[] = { "lc8", "lc32", "lc64" };
        static const int32_t end_args[] = { 65534, 65529, 65524 };
        static const char* const trapping[] = { "lcloads", "lcloads", "lcloads", "lcloads", "stores", "stores",
                                                "stores", "stores", "lc8", "lc32", "lc64" };
        static const int32_t traps[] = { 65530, 65525, 0, -1, 65530, 65528, 65520, 65512, 65535, 65530, 65525 };
        static const uint8_t f32_bits[] = { 0x01, 0x00, 0xa0, 0x7f };
        static const uint8_t f64_bits[] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf4, 0x7f };
        wasm_runtime_t rt;
        wasm_module_t* m;
        wasm_value_t args[2];
        wasm_value_t result;
        uint8_t* data;

        wasm_init(&rt);
        m = wasm_load(&rt, module_bytes, sizeof(module_bytes));
        CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
        if (!m) {
            wasm_destroy(&rt);
            return;
        }
        for (i = 0; i < sizeof(loads) / sizeof(loads[0]); i++) {
            args[0] = wasm_i32(loads[i]);
            wasm_test_fused_matches_unfused(t, m, "lcloads", args, 1, NULL, 1, NULL);
        }
        for (i = 0; i < 3; i++) {
            args[0] = wasm_i32((int32_t)(100u + i * 40u));
            args[1] = wasm_i32(i == 0 ? 0x7FA00001 : i == 1 ? -1 : 12345);
            wasm_test_fused_matches_unfused(t, m, "stores", args, 2, NULL, 1, NULL);
        }
        for (i = 0; i < 3; i++) {
            args[0] = wasm_i32(end_args[i]);
            wasm_test_fused_matches_unfused(t, m, ends[i], args, 1, NULL, 0, NULL);
        }
        data = wasm_memory_data(m, 0);
        CHECK(t, memcmp(data + 108, f32_bits, sizeof(f32_bits)) == 0);
        CHECK(t, memcmp(data + 124, f64_bits, sizeof(f64_bits)) == 0);
        for (i = 0; i < sizeof(traps) / sizeof(traps[0]); i++) {
            const char* name = trapping[i];
            uint32_t num_args = strcmp(name, "stores") == 0 ? 2u : 1u;
            uint8_t fused_tail[64];
            uint32_t fused_offset = 0;
            uint32_t unfused_offset = 0;
            wasm_error_t err;

            args[0] = wasm_i32(traps[i]);
            args[1] = wasm_i32(-7);
            memset(data + 65472, 0, 64);
            CHECK(t, wasm_call(m, name, args, num_args, &result, 1) == WASM_ERR_OUT_OF_BOUNDS);
            WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, NULL, &fused_offset));
            memcpy(fused_tail, data + 65472, 64);
            memset(data + 65472, 0, 64);
            wasm_runtime_set_fuel(&rt, 1);
            err = wasm_call(m, name, args, num_args, &result, 1);
            while (err == WASM_ERR_OUT_OF_FUEL) {
                wasm_runtime_set_fuel(&rt, 1);
                err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
            }
            wasm_runtime_disable_fuel(&rt);
            CHECK(t, err == WASM_ERR_OUT_OF_BOUNDS);
            WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, NULL, &unfused_offset));
            CHECK_MSG(t, fused_offset == unfused_offset, "%s(%d) trapped at %u, not %u", name, (int)traps[i],
                      (unsigned)fused_offset, (unsigned)unfused_offset);
            CHECK_MSG(t, memcmp(fused_tail, data + 65472, 64) == 0, "%s(%d) stored differently before trapping",
                      name, (int)traps[i]);
        }
        wasm_test_note_fused_ops(m, seen);
        wasm_module_free(m);
        wasm_destroy(&rt);
    }

    for (i = 0; i < WASM__OP_COUNT - WASM__OP_FUSED; i++)
        CHECK_MSG(t, seen[i], "fused op %u never formed", (unsigned)(WASM__OP_FUSED + i));
}

/* Exports gc(n), a loop that allocates structs and arrays of references, bytes, and vectors, moves values
 * through their fields, i31, saturating truncation, and ref.func, and then walks the list it built, and
 * units, a straight run of 27 instructions. */
static const uint8_t wasm_test_gc_loop_sections[] = {
    0x01, 0x27, 0x08, 0x5F, 0x02, 0x7F, 0x01, 0x7F, 0x01, 0x5F, 0x02, 0x7F, 0x00, 0x63, 0x01, 0x00,
    0x5E, 0x63, 0x00, 0x01, 0x5E, 0x78, 0x01, 0x5E, 0x7B, 0x01, 0x5F, 0x02, 0x7E, 0x01, 0x7B, 0x01,
    0x60, 0x01, 0x7F, 0x01, 0x7F, 0x60, 0x00, 0x01, 0x7F, 0x03, 0x04, 0x03, 0x06, 0x06, 0x07, 0x07,
    0x0E, 0x02, 0x02, 0x67, 0x63, 0x00, 0x01, 0x05, 0x75, 0x6E, 0x69, 0x74, 0x73, 0x00, 0x02, 0x09,
    0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0xBA, 0x03, 0x03, 0x07, 0x00, 0x20, 0x00, 0x41, 0x01,
    0x6A, 0x0B, 0xEB, 0x02, 0x07, 0x02, 0x7F, 0x01, 0x63, 0x00, 0x01, 0x63, 0x02, 0x01, 0x63, 0x03,
    0x01, 0x63, 0x01, 0x01, 0x63, 0x04, 0x01, 0x63, 0x05, 0x03, 0x40, 0x20, 0x01, 0x20, 0x02, 0xFB,
    0x00, 0x00, 0x21, 0x03, 0x20, 0x03, 0x20, 0x03, 0xFB, 0x02, 0x00, 0x00, 0x41, 0x03, 0x6A, 0xFB,
    0x05, 0x00, 0x01, 0x20, 0x02, 0x20, 0x03, 0xFB, 0x02, 0x00, 0x01, 0x6A, 0x21, 0x02, 0x20, 0x03,
    0x41, 0x03, 0xFB, 0x06, 0x02, 0x21, 0x04, 0x20, 0x04, 0x41, 0x01, 0xFB, 0x01, 0x00, 0xFB, 0x0E,
    0x02, 0x20, 0x02, 0x20, 0x04, 0x20, 0x01, 0x41, 0x03, 0x70, 0xFB, 0x0B, 0x02, 0xFB, 0x02, 0x00,
    0x00, 0x73, 0x21, 0x02, 0x20, 0x02, 0x20, 0x04, 0xFB, 0x0F, 0x6A, 0x21, 0x02, 0x20, 0x01, 0x20,
    0x02, 0x41, 0xFF, 0x01, 0xFB, 0x08, 0x03, 0x03, 0x21, 0x05, 0x20, 0x02, 0x20, 0x05, 0x41, 0x02,
    0xFB, 0x0C, 0x03, 0x20, 0x05, 0x41, 0x01, 0xFB, 0x0D, 0x03, 0x6A, 0x6A, 0x21, 0x02, 0x20, 0x05,
    0x41, 0x00, 0x20, 0x02, 0xFB, 0x0E, 0x03, 0x20, 0x02, 0x20, 0x05, 0x41, 0x00, 0xFB, 0x0C, 0x03,
    0x6A, 0x21, 0x02, 0x20, 0x02, 0xFD, 0x11, 0x41, 0x02, 0xFB, 0x06, 0x04, 0x21, 0x07, 0x20, 0x07,
    0x41, 0x01, 0x20, 0x07, 0x41, 0x00, 0xFB, 0x0B, 0x04, 0x20, 0x01, 0xFD, 0x1C, 0x03, 0xFB, 0x0E,
    0x04, 0x20, 0x01, 0xAD, 0x20, 0x07, 0x41, 0x01, 0xFB, 0x0B, 0x04, 0xFB, 0x00, 0x05, 0x21, 0x08,
    0x20, 0x08, 0x20, 0x08, 0xFB, 0x02, 0x05, 0x01, 0x20, 0x08, 0xFB, 0x02, 0x05, 0x01, 0xFD, 0xAE,
    0x01, 0xFB, 0x05, 0x05, 0x01, 0x20, 0x02, 0x20, 0x08, 0xFB, 0x02, 0x05, 0x01, 0xFD, 0x1B, 0x03,
    0x6A, 0x21, 0x02, 0x20, 0x02, 0x20, 0x02, 0x41, 0x01, 0x74, 0xFB, 0x1C, 0xFB, 0x1D, 0x6A, 0x21,
    0x02, 0x20, 0x02, 0x20, 0x01, 0xFB, 0x1C, 0xFB, 0x1E, 0x73, 0x21, 0x02, 0x20, 0x02, 0x20, 0x06,
    0xFB, 0x00, 0x01, 0x21, 0x06, 0x20, 0x02, 0xD2, 0x00, 0x14, 0x06, 0x21, 0x02, 0x20, 0x02, 0x20,
    0x02, 0xB7, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x3F, 0xA2, 0xFC, 0x02, 0x6A, 0x21,
    0x02, 0x20, 0x02, 0x20, 0x01, 0xB3, 0xFC, 0x05, 0xA7, 0x73, 0x21, 0x02, 0x20, 0x01, 0x41, 0x01,
    0x6A, 0x22, 0x01, 0x20, 0x00, 0x49, 0x0D, 0x00, 0x0B, 0x02, 0x40, 0x03, 0x40, 0x20, 0x06, 0xD1,
    0x0D, 0x01, 0x20, 0x02, 0x41, 0x05, 0x77, 0x20, 0x06, 0xFB, 0x02, 0x01, 0x00, 0x6A, 0x21, 0x02,
    0x20, 0x06, 0xFB, 0x02, 0x01, 0x01, 0x21, 0x06, 0x0C, 0x00, 0x0B, 0x0B, 0x20, 0x02, 0x0B, 0x43,
    0x02, 0x01, 0x63, 0x00, 0x01, 0x63, 0x03, 0xFB, 0x01, 0x00, 0x21, 0x00, 0x20, 0x00, 0x41, 0x05,
    0xFB, 0x05, 0x00, 0x00, 0x20, 0x00, 0xFB, 0x02, 0x00, 0x00, 0x41, 0x03, 0xFB, 0x07, 0x03, 0x21,
    0x01, 0x20, 0x01, 0x41, 0x01, 0x41, 0xAC, 0x02, 0xFB, 0x0E, 0x03, 0x20, 0x01, 0x41, 0x01, 0xFB,
    0x0D, 0x03, 0x6A, 0x20, 0x01, 0xFB, 0x0F, 0x6A, 0xFB, 0x1C, 0xFB, 0x1D, 0xB2, 0xFC, 0x00, 0xD2,
    0x00, 0x1A, 0x0B,
};

/* Returns whether an instruction of m's translated functions starts with op. */
static int wasm_test_translates_to(const wasm_module_t* m, uint32_t op) {
    uint32_t i;
    uint32_t j;

    for (i = 0; i < m->num_funcs; i++) {
        const wasm_func_t* func = &m->funcs[i];

        for (j = 0; func->ops && j < func->num_op_offsets; j++)
            if (func->ops[func->op_offsets[j * 2u]] == op) return 1;
    }
    return 0;
}

TEST(test_gc_ops_match_unfused_execution) {
    static const uint32_t ops[] = { WASM__OP_STRUCT_GET, WASM__OP_STRUCT_SET, WASM__OP_ARRAY_GET, WASM__OP_ARRAY_SET,
                                    WASM__OP_ARRAY_LEN,  WASM__OP_REF_I31,    WASM__OP_I31_GET_S, WASM__OP_I31_GET_U,
                                    WASM__OP_TRUNC_SAT,  WASM__OP_GC_NEW,     WASM__OP_L_STRUCT_GET };
    static const int32_t counts[] = { 1, 7, 40 };
    static const int32_t expected[] = { 5410, -1095433578, 1139485182 };
    wasm_config_t config;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    uint32_t i;

    wasm_config_default(&config);
    config.initial_gc_heap_size = 2048;
    wasm_init_config(&rt, &config);
    m = wasm_test_load_sections(&rt, wasm_test_gc_loop_sections, sizeof(wasm_test_gc_loop_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(ops); i++)
        CHECK_MSG(t, wasm_test_translates_to(m, ops[i]), "op %u never formed", (unsigned)ops[i]);
    for (i = 0; i < COUNTOF(counts); i++) {
        arg = wasm_i32(counts[i]);
        memset(&result, 0, sizeof(result));
        wasm_test_fused_matches_unfused(t, m, "gc", &arg, 1, NULL, 1, &result);
        WASM_CHECK_I32(t, result.of.i32, expected[i]);
    }
    for (i = 0; i < 2; i++) {
        uint64_t used = wasm_test_fused_call_in_steps(t, m, "units", NULL, 0, i ? (uint64_t)1 << 40 : 1u, NULL, 1,
                                                      NULL, &result);

        WASM_CHECK_I32(t, (int32_t)used, 27);
        WASM_CHECK_I32(t, result.of.i32, 52);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_translate_cases[] = {
    { "0xFC immediates are skipped while scanning dead branches", test_prefixed_opcode_in_dead_branch },
    { "loader: control-target scan accepts signed i32.const encodings", test_control_target_scan_accepts_signed_i32_const_leb },
    { "fused ops: results, traps, fuel stops, interrupts, and collections match unfused execution", test_fused_ops_match_unfused_execution },
    { "GC ops: results, fuel stops, and collections in loops match unfused execution", test_gc_ops_match_unfused_execution },
};
const test_suite exec_translate_suite = { "exec.translate", exec_translate_cases, COUNTOF(exec_translate_cases) };

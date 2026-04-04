/*
 * test_wasm.c — Test harness for wasm.h single-header runtime
 *
 * Builds hand-crafted Wasm modules as byte arrays and executes them.
 * Compile: cc -O2 -o test_wasm test_wasm.c -lm
 */

#define WASM_IMPL
#include "wasm.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
    #define WASM_TEST_SNPRINTF _snprintf
#else
    #define WASM_TEST_SNPRINTF snprintf
#endif

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-50s", name)
#define PASS() do { printf("\033[32mPASS\033[0m\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("\033[31mFAIL: %s\033[0m\n", msg); tests_failed++; } while(0)

static void fail_i32_got(int32_t value) {
    char buf[64];

    WASM_TEST_SNPRINTF(buf, sizeof(buf), "got %d", value);
    FAIL(buf);
}

/* ── Helper: build minimal wasm module ──────────────────────────── */

typedef struct {
    uint8_t  buf[4096];
    uint32_t len;
} wasm_builder_t;

static void emit(wasm_builder_t *b, uint8_t byte) {
    b->buf[b->len++] = byte;
}

static void emit_bytes(wasm_builder_t *b, const uint8_t *data, uint32_t len) {
    memcpy(b->buf + b->len, data, len);
    b->len += len;
}

static void emit_leb128_u32(wasm_builder_t *b, uint32_t val) {
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val) byte |= 0x80;
        emit(b, byte);
    } while (val);
}

static void emit_leb128_i32(wasm_builder_t *b, int32_t val) {
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

/* Emit a section: id + size + content */
static void emit_section(wasm_builder_t *b, uint8_t id, const uint8_t *content, uint32_t len) {
    emit(b, id);
    emit_leb128_u32(b, len);
    emit_bytes(b, content, len);
}

static void emit_header(wasm_builder_t *b) {
    /* magic */
    emit(b, 0x00); emit(b, 0x61); emit(b, 0x73); emit(b, 0x6D);
    /* version 1 */
    emit(b, 0x01); emit(b, 0x00); emit(b, 0x00); emit(b, 0x00);
}

/* ── Test 1: add(a, b) -> a + b ─────────────────────────────────── */

static void test_add(void) {
    TEST("i32.add(3, 7) == 10");

    /*
     * Module with one function: (func (param i32 i32) (result i32)
     *   local.get 0
     *   local.get 1
     *   i32.add)
     */
    wasm_builder_t mod = {0};
    emit_header(&mod);

    /* Type section: one func type (i32, i32) -> i32 */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);     /* 1 type */
        emit(&sec, 0x60);             /* func */
        emit_leb128_u32(&sec, 2);     /* 2 params */
        emit(&sec, 0x7F);             /* i32 */
        emit(&sec, 0x7F);             /* i32 */
        emit_leb128_u32(&sec, 1);     /* 1 result */
        emit(&sec, 0x7F);             /* i32 */
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function section: one function using type 0 */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);     /* 1 func */
        emit_leb128_u32(&sec, 0);     /* type index 0 */
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export section: export func 0 as "add" */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);     /* 1 export */
        emit_leb128_u32(&sec, 3);     /* name len */
        emit(&sec, 'a'); emit(&sec, 'd'); emit(&sec, 'd');
        emit(&sec, 0x00);             /* func kind */
        emit_leb128_u32(&sec, 0);     /* func index */
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code section */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);     /* 1 body */

        /* Function body */
        wasm_builder_t body = {0};
        emit_leb128_u32(&body, 0);    /* 0 local declarations */
        emit(&body, 0x20); emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x20); emit_leb128_u32(&body, 1); /* local.get 1 */
        emit(&body, 0x6A);            /* i32.add */
        emit(&body, 0x0B);            /* end */

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_init(&rt);
    wasm_module_t *m = wasm_load(&rt, mod.buf, mod.len);
    if (!m) { FAIL(rt.error_msg); return; }

    wasm_value_t args[] = { wasm_i32(3), wasm_i32(7) };
    wasm_value_t result;
    wasm_error_t err = wasm_call(m, "add", args, 2, &result, 1);

    if (err != WASM_OK) { FAIL(wasm_error_string(err)); }
    else if (result.of.i32 != 10) { fail_i32_got(result.of.i32); }
    else { PASS(); }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Test 2: factorial(n) recursive ─────────────────────────────── */

static void test_factorial(void) {
    TEST("factorial(10) == 3628800");

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
    wasm_builder_t mod = {0};
    emit_header(&mod);

    /* Type section */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1); emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1); emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function section */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3);
        emit(&sec, 'f'); emit(&sec, 'a'); emit(&sec, 'c');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code section */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = {0};
        emit_leb128_u32(&body, 0);    /* no local decls */

        emit(&body, 0x20); emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x45);            /* i32.eqz */
        emit(&body, 0x04); emit(&body, 0x7F); /* if (result i32) */
        emit(&body, 0x41); emit_leb128_i32(&body, 1); /* i32.const 1 */
        emit(&body, 0x05);            /* else */
        emit(&body, 0x20); emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x20); emit_leb128_u32(&body, 0); /* local.get 0 */
        emit(&body, 0x41); emit_leb128_i32(&body, 1); /* i32.const 1 */
        emit(&body, 0x6B);            /* i32.sub */
        emit(&body, 0x10); emit_leb128_u32(&body, 0); /* call 0 (self) */
        emit(&body, 0x6C);            /* i32.mul */
        emit(&body, 0x0B);            /* end if */
        emit(&body, 0x0B);            /* end func */

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_init(&rt);
    wasm_module_t *m = wasm_load(&rt, mod.buf, mod.len);
    if (!m) { FAIL(rt.error_msg); return; }

    wasm_value_t args[] = { wasm_i32(10) };
    wasm_value_t result;
    wasm_error_t err = wasm_call(m, "fac", args, 1, &result, 1);

    if (err != WASM_OK) { FAIL(wasm_error_string(err)); }
    else if (result.of.i32 != 3628800) { fail_i32_got(result.of.i32); }
    else { PASS(); }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Test 3: host import (print function) ───────────────────────── */

static int host_print_called = 0;
static int32_t host_print_value = 0;

static wasm_error_t host_print(wasm_runtime_t *rt,
                                const wasm_value_t *args, uint32_t num_args,
                                wasm_value_t *results, uint32_t num_results,
                                void *userdata) {
    (void)rt; (void)results; (void)num_results; (void)userdata;
    host_print_called = 1;
    if (num_args > 0) host_print_value = args[0].of.i32;
    return WASM_OK;
}

static void test_host_import(void) {
    TEST("host import: env.print(42)");

    host_print_called = 0;
    host_print_value = 0;

    wasm_builder_t mod = {0};
    emit_header(&mod);

    /* Type section: (i32) -> () */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 2);     /* 2 types */
        /* type 0: (i32) -> () for import */
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1); emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);
        /* type 1: () -> () for exported func */
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Import section: env.print */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);     /* 1 import */
        emit_leb128_u32(&sec, 3); emit(&sec, 'e'); emit(&sec, 'n'); emit(&sec, 'v');
        emit_leb128_u32(&sec, 5); emit(&sec, 'p'); emit(&sec, 'r'); emit(&sec, 'i'); emit(&sec, 'n'); emit(&sec, 't');
        emit(&sec, 0x00);             /* func import */
        emit_leb128_u32(&sec, 0);     /* type index 0 */
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    /* Function section: 1 func (the exported one) */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);     /* type index 1: () -> () */
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export: "run" = func 1 (func 0 is the import) */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3); emit(&sec, 'r'); emit(&sec, 'u'); emit(&sec, 'n');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);     /* func index 1 */
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code section */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = {0};
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41); emit_leb128_i32(&body, 42); /* i32.const 42 */
        emit(&body, 0x10); emit_leb128_u32(&body, 0);  /* call 0 (import) */
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_init(&rt);

    wasm_import_t imp;

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "print";
    imp.func = host_print;
    wasm_register_import(&rt, &imp);

    wasm_module_t *m = wasm_load(&rt, mod.buf, mod.len);
    if (!m) { FAIL(rt.error_msg); return; }

    wasm_error_t err = wasm_call(m, "run", NULL, 0, NULL, 0);

    if (err != WASM_OK) { FAIL(wasm_error_string(err)); }
    else if (!host_print_called) { FAIL("host func not called"); }
    else if (host_print_value != 42) { fail_i32_got(host_print_value); }
    else { PASS(); }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Test 4: memory load/store ──────────────────────────────────── */

static void test_memory(void) {
    TEST("memory: store 99 at offset 0, load it back");

    wasm_builder_t mod = {0};
    emit_header(&mod);

    /* Type: () -> (i32) */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1); emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Memory: 1 page min */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);     /* 1 memory */
        emit(&sec, 0x00);             /* limits: no max */
        emit_leb128_u32(&sec, 1);     /* 1 page initial */
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    /* Export */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 4); emit(&sec, 't'); emit(&sec, 'e'); emit(&sec, 's'); emit(&sec, 't');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code: i32.const 0; i32.const 99; i32.store; i32.const 0; i32.load */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = {0};
        emit_leb128_u32(&body, 0);

        /* store 99 at address 0 */
        emit(&body, 0x41); emit_leb128_i32(&body, 0);   /* i32.const 0 (addr) */
        emit(&body, 0x41); emit_leb128_i32(&body, 99);  /* i32.const 99 (value) */
        emit(&body, 0x36); emit_leb128_u32(&body, 2); emit_leb128_u32(&body, 0); /* i32.store align=2 offset=0 */

        /* load from address 0 */
        emit(&body, 0x41); emit_leb128_i32(&body, 0);   /* i32.const 0 */
        emit(&body, 0x28); emit_leb128_u32(&body, 2); emit_leb128_u32(&body, 0); /* i32.load align=2 offset=0 */

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_init(&rt);
    wasm_module_t *m = wasm_load(&rt, mod.buf, mod.len);
    if (!m) { FAIL(rt.error_msg); return; }

    wasm_value_t result;
    wasm_error_t err = wasm_call(m, "test", NULL, 0, &result, 1);

    if (err != WASM_OK) { FAIL(wasm_error_string(err)); }
    else if (result.of.i32 != 99) { fail_i32_got(result.of.i32); }
    else { PASS(); }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Test 5: loop with br_if (sum 1..10) ────────────────────────── */

static void test_loop(void) {
    TEST("loop: sum(1..10) == 55");

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
    wasm_builder_t mod = {0};
    emit_header(&mod);

    /* Type: () -> (i32) */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1); emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    /* Function */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    /* Export */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 3); emit(&sec, 's'); emit(&sec, 'u'); emit(&sec, 'm');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    /* Code */
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);

        wasm_builder_t body = {0};
        /* 2 locals: i32, i32 */
        emit_leb128_u32(&body, 1);    /* 1 local declaration */
        emit_leb128_u32(&body, 2);    /* 2 locals */
        emit(&body, 0x7F);            /* i32 */

        /* block (void) */
        emit(&body, 0x02); emit(&body, 0x40);
        /* loop (void) */
        emit(&body, 0x03); emit(&body, 0x40);

        /* i = i + 1 */
        emit(&body, 0x20); emit_leb128_u32(&body, 0); /* local.get 0 ($i) */
        emit(&body, 0x41); emit_leb128_i32(&body, 1); /* i32.const 1 */
        emit(&body, 0x6A);                             /* i32.add */
        emit(&body, 0x21); emit_leb128_u32(&body, 0); /* local.set 0 ($i) */

        /* sum = sum + i */
        emit(&body, 0x20); emit_leb128_u32(&body, 1); /* local.get 1 ($sum) */
        emit(&body, 0x20); emit_leb128_u32(&body, 0); /* local.get 0 ($i) */
        emit(&body, 0x6A);                             /* i32.add */
        emit(&body, 0x21); emit_leb128_u32(&body, 1); /* local.set 1 ($sum) */

        /* if i >= 10, break */
        emit(&body, 0x20); emit_leb128_u32(&body, 0);  /* local.get 0 ($i) */
        emit(&body, 0x41); emit_leb128_i32(&body, 10);  /* i32.const 10 */
        emit(&body, 0x4E);                              /* i32.ge_s */
        emit(&body, 0x0D); emit_leb128_u32(&body, 1);  /* br_if 1 (break block) */

        /* continue loop */
        emit(&body, 0x0C); emit_leb128_u32(&body, 0);  /* br 0 (loop) */

        emit(&body, 0x0B); /* end loop */
        emit(&body, 0x0B); /* end block */

        /* return sum */
        emit(&body, 0x20); emit_leb128_u32(&body, 1);  /* local.get 1 ($sum) */
        emit(&body, 0x0B); /* end func */

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_init(&rt);
    wasm_module_t *m = wasm_load(&rt, mod.buf, mod.len);
    if (!m) { FAIL(rt.error_msg); return; }

    wasm_value_t result;
    wasm_error_t err = wasm_call(m, "sum", NULL, 0, &result, 1);

    if (err != WASM_OK) { FAIL(wasm_error_string(err)); }
    else if (result.of.i32 != 55) { fail_i32_got(result.of.i32); }
    else { PASS(); }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Test 6: error handling ─────────────────────────────────────── */

static void test_bad_magic(void) {
    TEST("reject bad magic number");

    uint8_t bad[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x00, 0x00, 0x00 };
    wasm_runtime_t rt;
    wasm_init(&rt);
    wasm_module_t *m = wasm_load(&rt, bad, sizeof(bad));
    if (m) { FAIL("should have rejected"); wasm_free_module(m); }
    else if (rt.last_error != WASM_ERR_INVALID_MAGIC) { FAIL("wrong error code"); }
    else { PASS(); }
    wasm_destroy(&rt);
}

static void test_div_by_zero(void) {
    TEST("trap on i32.div_s by zero");

    wasm_builder_t mod = {0};
    emit_header(&mod);

    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1); emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1); emit(&sec, 'f');
        emit(&sec, 0x00); emit_leb128_u32(&sec, 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }
    {
        wasm_builder_t sec = {0};
        emit_leb128_u32(&sec, 1);
        wasm_builder_t body = {0};
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41); emit_leb128_i32(&body, 10);
        emit(&body, 0x41); emit_leb128_i32(&body, 0);
        emit(&body, 0x6D); /* i32.div_s */
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_runtime_t rt;
    wasm_init(&rt);
    wasm_module_t *m = wasm_load(&rt, mod.buf, mod.len);
    if (!m) { FAIL(rt.error_msg); return; }

    wasm_value_t result;
    wasm_error_t err = wasm_call(m, "f", NULL, 0, &result, 1);
    if (err == WASM_ERR_DIV_BY_ZERO) { PASS(); }
    else { FAIL("expected div by zero trap"); }

    wasm_free_module(m);
    wasm_destroy(&rt);
}

/* ── Main ─────────────────────────────────────────────────────────── */

int main(void) {
    printf("\n\033[1m=== wasm.h test suite ===\033[0m\n\n");

    test_add();
    test_factorial();
    test_host_import();
    test_memory();
    test_loop();
    test_bad_magic();
    test_div_by_zero();

    printf("\n\033[1mResults: %d passed, %d failed\033[0m\n\n", tests_passed, tests_failed);
    return tests_failed ? 1 : 0;
}
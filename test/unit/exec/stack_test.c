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

/* (module
     (type $f (func)) (type $c (cont $f))
     (tag $e)
     (func $f3 (unreachable))
     (func $f2 (try_table (catch $e 0) (call $f3)))
     (func $f1 (call $f2))
     (elem declare func $f1)
     (func (export "run") (resume $c (cont.new $c (ref.func $f1))))) */
static const uint8_t wasm_test_stack_frames_sections[] = {
    0x01, 0x06, 0x02, 0x60, 0x00, 0x00, 0x5D, 0x00, 0x03, 0x05, 0x04, 0x00, 0x00, 0x00, 0x00, 0x0D,
    0x03, 0x01, 0x00, 0x00, 0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6E, 0x00, 0x03, 0x09, 0x05, 0x01,
    0x03, 0x00, 0x01, 0x02, 0x0A, 0x20, 0x04, 0x03, 0x00, 0x00, 0x0B, 0x0B, 0x00, 0x1F, 0x40, 0x01,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x0B, 0x0B, 0x04, 0x00, 0x10, 0x01, 0x0B, 0x09, 0x00, 0xD2, 0x02,
    0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0B,
};

TEST(test_new_stacks_write_each_call_frame_they_push) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t run;

    wasm_config_default(&cfg);
    cfg.cont_call_depth = 4;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_sections(&rt, wasm_test_stack_frames_sections, sizeof(wasm_test_stack_frames_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    for (run = 0; run < 2; run++) {
        uint32_t depth;

        CHECK(t, wasm_call(m, "run", NULL, 0, NULL, 0) == WASM_ERR_UNREACHABLE);
        for (depth = 0; depth < 4; depth++) {
            wasm_module_t* frame_mod = NULL;
            uint32_t func_idx = UINT32_MAX;
            uint32_t offset = 0;

            WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, depth, &frame_mod, &func_idx, &offset));
            CHECK_MSG(t, frame_mod == m && func_idx == depth, "run %u frame %u: function %u", (unsigned)run,
                      (unsigned)depth, (unsigned)func_idx);
        }
        CHECK(t, wasm_runtime_call_frame(&rt, 4, NULL, NULL, NULL) != WASM_OK);
        CHECK(t, rt.num_cont_stacks == 0 && rt.num_pooled_cont_stacks == 1);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module
     (type $r (func (result i32)))
     (type $kr (cont $r))
     (type $i_i (func (param i32) (result i32)))
     (type $ki (cont $i_i))
     (type $tail_t (func (param i32 i32 i32) (result i32)))
     (type $many (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
     (type $kmany (cont $many))
     (type $twelve (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
     (type $k12 (cont $twelve))
     (type $box (struct (field i32)))
     (rec (type $fs (func (param i32 (ref null $cs)) (result i32))) (type $cs (cont $fs)))
     (type $fb (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 (ref null $cs)) (result i32)))
     (type $cb (cont $fb))
     (import "host" "probe" (func $probe (result i32)))
     (import "host" "reenter" (func $reenter (param i32) (result i32)))
     (import "host" "interrupt" (func $interrupt))
     (import "host" "collect" (func $collect))
     (import "host" "sum12" (func $hsum12 (type $twelve)))
     (import "host" "switch12" (func $hswitch12 (type $fb)))
     (tag $yield (param i32))
     (tag $ask (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32))
     (tag $sw (result i32))
     (tag $e12 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32))
     (table $t 2 funcref)
     (elem (table $t) (i32.const 0) func $rec_ind $tail)
     (table $fibers 0 (ref null $kr))
     (elem declare func $rec $rec_ind $rec_ref $tail_entry $sum16 $waiter $catch_deep $catch_legacy
       $reent_body $gcrec $intr $nest_entry $fiber $switcher $hsum12 $hswitch12)
   
     (func $rec (export "rec") (param $n i32) (result i32)
       (if (result i32) (i32.eqz (local.get $n))
         (then (call $probe))
         (else (i32.add (i32.const 1) (call $rec (i32.sub (local.get $n) (i32.const 1)))))))
     (func $rec_ind (param $n i32) (result i32)
       (if (result i32) (i32.eqz (local.get $n))
         (then (call $probe))
         (else (i32.add (i32.const 1)
           (call_indirect $t (type $i_i) (i32.sub (local.get $n) (i32.const 1)) (i32.const 0))))))
     (func $rec_ref (param $n i32) (result i32)
       (if (result i32) (i32.eqz (local.get $n))
         (then (call $probe))
         (else (i32.add (i32.const 1)
           (call_ref $i_i (i32.sub (local.get $n) (i32.const 1)) (ref.func $rec_ref))))))
     (func $tail (type $tail_t) (param $n i32) (param $acc i32) (param $mode i32) (result i32)
       (if (i32.eqz (local.get $n)) (then (return (i32.add (local.get $acc) (call $probe)))))
       (local.set $n (i32.sub (local.get $n) (i32.const 1)))
       (local.set $acc (i32.add (local.get $acc) (i32.const 1)))
       (if (i32.eqz (local.get $mode))
         (then (return_call $tail (local.get $n) (local.get $acc) (local.get $mode))))
       (if (i32.eq (local.get $mode) (i32.const 1))
         (then (return_call_indirect $t (type $tail_t) (local.get $n) (local.get $acc) (local.get $mode) (i32.const 1))))
       (return_call_ref $tail_t (local.get $n) (local.get $acc) (local.get $mode) (ref.func $tail)))
     (func $tail_entry (param $n i32) (result i32)
       (i32.add
         (i32.add (call $tail (local.get $n) (i32.const 0) (i32.const 0))
                  (call $tail (local.get $n) (i32.const 0) (i32.const 1)))
         (call $tail (local.get $n) (i32.const 0) (i32.const 2))))
   
     (func (export "cont-rec") (param $n i32) (result i32)
       (resume $ki (local.get $n) (cont.new $ki (ref.func $rec))))
     (func (export "cont-rec-indirect") (param $n i32) (result i32)
       (resume $ki (local.get $n) (cont.new $ki (ref.func $rec_ind))))
     (func (export "cont-rec-ref") (param $n i32) (result i32)
       (resume $ki (local.get $n) (cont.new $ki (ref.func $rec_ref))))
     (func (export "cont-tail") (param $n i32) (result i32)
       (resume $ki (local.get $n) (cont.new $ki (ref.func $tail_entry))))
   
     (func $sum16 (type $many)
       (i32.add (i32.add (i32.add (i32.add (local.get 0) (local.get 1)) (i32.add (local.get 2) (local.get 3)))
                         (i32.add (i32.add (local.get 4) (local.get 5)) (i32.add (local.get 6) (local.get 7))))
                (i32.add (i32.add (i32.add (local.get 8) (local.get 9)) (i32.add (local.get 10) (local.get 11)))
                         (i32.add (i32.add (local.get 12) (local.get 13)) (i32.add (local.get 14) (local.get 15))))))
     (func $sum12 (type $twelve)
       (i32.add (i32.add (i32.add (i32.add (local.get 0) (local.get 1)) (i32.add (local.get 2) (local.get 3)))
                         (i32.add (i32.add (local.get 4) (local.get 5)) (i32.add (local.get 6) (local.get 7))))
                (i32.add (i32.add (local.get 8) (local.get 9)) (i32.add (local.get 10) (local.get 11)))))
     (func (export "resume-args") (result i32)
       (resume $kmany (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6)
         (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12) (i32.const 13)
         (i32.const 14) (i32.const 15) (i32.const 16) (cont.new $kmany (ref.func $sum16))))
     (func (export "bind-args") (result i32)
       (resume $ki (i32.const 16)
         (cont.bind $kmany $ki (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5)
           (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12)
           (i32.const 13) (i32.const 14) (i32.const 15) (cont.new $kmany (ref.func $sum16)))))
     (func $waiter (result i32)
       (suspend $ask)
       (call $sum16)
       (i32.add (call $rec (i32.const 40))))
     (func (export "bind-suspended") (result i32) (local $k (ref null $kmany))
       (block $h (result (ref $kmany))
         (drop (resume $kr (on $ask $h) (cont.new $kr (ref.func $waiter))))
         (return (i32.const -1)))
       (local.set $k)
       (resume $ki (i32.const 16)
         (cont.bind $kmany $ki (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5)
           (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12)
           (i32.const 13) (i32.const 14) (i32.const 15) (local.get $k))))
     (func (export "resume-host") (result i32)
       (resume $ki (i32.const 12)
         (cont.bind $k12 $ki (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6)
           (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (cont.new $k12 (ref.func $hsum12)))))
     (func $switcher (type $fs) (param $n i32) (param $k (ref null $cs)) (result i32)
       (drop (call $rec (i32.const 30)))
       (switch $cs $sw (i32.add (local.get $n) (i32.const 1)) (local.get $k))
       (drop) (drop) (i32.const -1))
     (func (export "switch-host") (result i32)
       (resume $cs (on $sw switch) (i32.const 11)
         (cont.bind $cb $cs (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6)
           (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (cont.new $cb (ref.func $hswitch12)))
         (cont.new $cs (ref.func $switcher))))
   
     (func $catch_deep (param $n i32) (result i32)
       (block $h (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
         (try_table (catch $e12 $h) (suspend $yield (local.get $n)))
         (return (i32.const -1)))
       (call $sum12)
       (i32.add (call $rec (local.get $n))))
     (func $catch_legacy (param $n i32) (result i32)
       try (result i32)
         (suspend $yield (local.get $n))
         (i32.const -1)
       catch $e12
         (call $sum12)
         (i32.add (call $rec (local.get $n)))
       end)
     (func $throw_into (param $n i32) (param $f (ref $i_i)) (result i32) (local $k (ref null $kr))
       (block $h (result i32 (ref $kr))
         (drop (resume $ki (on $yield $h) (local.get $n) (cont.new $ki (local.get $f))))
         (return (i32.const -1)))
       (local.set $k)
       (drop)
       (resume_throw $kr $e12 (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6)
         (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12) (local.get $k)))
     (func (export "throw-deep") (param $n i32) (result i32) (call $throw_into (local.get $n) (ref.func $catch_deep)))
     (func (export "throw-legacy") (param $n i32) (result i32)
       (call $throw_into (local.get $n) (ref.func $catch_legacy)))
     (func (export "catch-payload") (param $n i32) (result i32)
       (block $h (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
         (try_table (catch $e12 $h)
           (drop (call $rec (local.get $n)))
           (throw $e12 (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6)
             (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12)))
         (return (i32.const -1)))
       (call $sum12))
   
     (func $reent_body (param $n i32) (result i32)
       (i32.add (i32.const 1000) (i32.add (local.get $n) (call $reenter (local.get $n)))))
     (func (export "reenter") (param $n i32) (result i32) (call $reent_body (local.get $n)))
     (func (export "cont-reenter") (param $n i32) (result i32)
       (resume $ki (local.get $n) (cont.new $ki (ref.func $reent_body))))
     (func (export "deep-cont") (param $n i32) (result i32)
       (resume $ki (local.get $n) (cont.new $ki (ref.func $rec))))
   
     (func $gcrec (param $n i32) (result i32) (local $b (ref null $box)) (local $r i32)
       (local.set $b (struct.new $box (local.get $n)))
       (if (result i32) (i32.eqz (local.get $n))
         (then (call $collect) (i32.const 0))
         (else
           (struct.new $box (i32.mul (local.get $n) (i32.const 2)))
           (call $gcrec (i32.sub (local.get $n) (i32.const 1)))
           (local.set $r)
           (struct.get $box 0)
           (local.get $r)
           (i32.add)
           (struct.get $box 0 (local.get $b))
           (i32.add))))
     (func (export "gc-cont") (param $n i32) (result i32)
       (resume $ki (local.get $n) (cont.new $ki (ref.func $gcrec))))
   
     (func $intr (param $n i32) (result i32)
       (if (result i32) (i32.eqz (local.get $n))
         (then (i32.const 0))
         (else (call $interrupt) (i32.add (i32.const 1) (call $intr (i32.sub (local.get $n) (i32.const 1)))))))
     (func (export "intr") (param $n i32) (result i32) (call $intr (local.get $n)))
     (func (export "cont-intr") (param $n i32) (result i32)
       (resume $ki (local.get $n) (cont.new $ki (ref.func $intr))))
   
     (func $nest (param $levels i32) (param $depth i32) (result i32)
       (if (result i32) (local.get $depth)
         (then (i32.add (i32.const 1) (call $nest (local.get $levels) (i32.sub (local.get $depth) (i32.const 1)))))
         (else
           (if (result i32) (local.get $levels)
             (then (resume $ki (i32.sub (local.get $levels) (i32.const 1)) (cont.new $ki (ref.func $nest_entry))))
             (else (call $probe))))))
     (func $nest_entry (param $levels i32) (result i32) (call $nest (local.get $levels) (i32.const 300)))
     (func (export "nest") (param $levels i32) (result i32)
       (resume $ki (local.get $levels) (cont.new $ki (ref.func $nest_entry))))
   
     (func $fiber (param $i i32) (result i32)
       (suspend $yield (local.get $i))
       (i32.mul (local.get $i) (i32.const 2)))
     (func (export "spawn") (param $n i32) (local $i i32) (local $k (ref null $kr))
       (drop (table.grow $fibers (ref.null $kr) (local.get $n)))
       (block $done
         (loop $l
           (br_if $done (i32.ge_u (local.get $i) (local.get $n)))
           (block $h (result i32 (ref $kr))
             (drop (resume $ki (on $yield $h) (local.get $i) (cont.new $ki (ref.func $fiber))))
             (unreachable))
           (local.set $k)
           (drop)
           (table.set $fibers (local.get $i) (local.get $k))
           (local.set $i (i32.add (local.get $i) (i32.const 1)))
           (br $l))))
     (func (export "finish") (result i32) (local $i i32) (local $acc i32)
       (block $done
         (loop $l
           (br_if $done (i32.ge_u (local.get $i) (table.size $fibers)))
           (local.set $acc (i32.add (local.get $acc) (resume $kr (table.get $fibers (local.get $i)))))
           (table.set $fibers (local.get $i) (ref.null $kr))
           (local.set $i (i32.add (local.get $i) (i32.const 1)))
           (br $l)))
       (local.get $acc))) */
static const uint8_t wasm_test_grow_sections[] = {
    0x01, 0xAB, 0x01, 0x15, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D,
    0x02, 0x60, 0x03, 0x7F, 0x7F, 0x7F, 0x01, 0x7F, 0x60, 0x10, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x01, 0x7F, 0x5D, 0x05, 0x60, 0x0C,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x01, 0x7F, 0x5D, 0x07,
    0x5F, 0x01, 0x7F, 0x00, 0x4E, 0x02, 0x60, 0x02, 0x7F, 0x63, 0x0B, 0x01, 0x7F, 0x5D, 0x0A, 0x60,
    0x0D, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x63, 0x0B, 0x01,
    0x7F, 0x5D, 0x0C, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x10, 0x7F, 0x7F, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x60, 0x0C, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x00, 0x60, 0x00, 0x0C, 0x7F,
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x60, 0x02, 0x7F, 0x64, 0x02,
    0x01, 0x7F, 0x60, 0x00, 0x02, 0x7F, 0x64, 0x01, 0x60, 0x02, 0x7F, 0x7F, 0x01, 0x7F, 0x02, 0x5A,
    0x06, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x05, 0x70, 0x72, 0x6F, 0x62, 0x65, 0x00, 0x00, 0x04, 0x68,
    0x6F, 0x73, 0x74, 0x07, 0x72, 0x65, 0x65, 0x6E, 0x74, 0x65, 0x72, 0x00, 0x02, 0x04, 0x68, 0x6F,
    0x73, 0x74, 0x09, 0x69, 0x6E, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x00, 0x0E, 0x04, 0x68,
    0x6F, 0x73, 0x74, 0x07, 0x63, 0x6F, 0x6C, 0x6C, 0x65, 0x63, 0x74, 0x00, 0x0E, 0x04, 0x68, 0x6F,
    0x73, 0x74, 0x05, 0x73, 0x75, 0x6D, 0x31, 0x32, 0x00, 0x07, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x08,
    0x73, 0x77, 0x69, 0x74, 0x63, 0x68, 0x31, 0x32, 0x00, 0x0C, 0x03, 0x28, 0x27, 0x02, 0x02, 0x02,
    0x04, 0x02, 0x02, 0x02, 0x02, 0x02, 0x05, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x02,
    0x02, 0x13, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x15, 0x02,
    0x02, 0x02, 0x0F, 0x00, 0x04, 0x08, 0x02, 0x70, 0x00, 0x02, 0x63, 0x01, 0x00, 0x00, 0x0D, 0x09,
    0x04, 0x00, 0x0F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x11, 0x07, 0x8E, 0x02, 0x16, 0x03, 0x72, 0x65,
    0x63, 0x00, 0x06, 0x08, 0x63, 0x6F, 0x6E, 0x74, 0x2D, 0x72, 0x65, 0x63, 0x00, 0x0B, 0x11, 0x63,
    0x6F, 0x6E, 0x74, 0x2D, 0x72, 0x65, 0x63, 0x2D, 0x69, 0x6E, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74,
    0x00, 0x0C, 0x0C, 0x63, 0x6F, 0x6E, 0x74, 0x2D, 0x72, 0x65, 0x63, 0x2D, 0x72, 0x65, 0x66, 0x00,
    0x0D, 0x09, 0x63, 0x6F, 0x6E, 0x74, 0x2D, 0x74, 0x61, 0x69, 0x6C, 0x00, 0x0E, 0x0B, 0x72, 0x65,
    0x73, 0x75, 0x6D, 0x65, 0x2D, 0x61, 0x72, 0x67, 0x73, 0x00, 0x11, 0x09, 0x62, 0x69, 0x6E, 0x64,
    0x2D, 0x61, 0x72, 0x67, 0x73, 0x00, 0x12, 0x0E, 0x62, 0x69, 0x6E, 0x64, 0x2D, 0x73, 0x75, 0x73,
    0x70, 0x65, 0x6E, 0x64, 0x65, 0x64, 0x00, 0x14, 0x0B, 0x72, 0x65, 0x73, 0x75, 0x6D, 0x65, 0x2D,
    0x68, 0x6F, 0x73, 0x74, 0x00, 0x15, 0x0B, 0x73, 0x77, 0x69, 0x74, 0x63, 0x68, 0x2D, 0x68, 0x6F,
    0x73, 0x74, 0x00, 0x17, 0x0A, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x2D, 0x64, 0x65, 0x65, 0x70, 0x00,
    0x1B, 0x0C, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x2D, 0x6C, 0x65, 0x67, 0x61, 0x63, 0x79, 0x00, 0x1C,
    0x0D, 0x63, 0x61, 0x74, 0x63, 0x68, 0x2D, 0x70, 0x61, 0x79, 0x6C, 0x6F, 0x61, 0x64, 0x00, 0x1D,
    0x07, 0x72, 0x65, 0x65, 0x6E, 0x74, 0x65, 0x72, 0x00, 0x1F, 0x0C, 0x63, 0x6F, 0x6E, 0x74, 0x2D,
    0x72, 0x65, 0x65, 0x6E, 0x74, 0x65, 0x72, 0x00, 0x20, 0x09, 0x64, 0x65, 0x65, 0x70, 0x2D, 0x63,
    0x6F, 0x6E, 0x74, 0x00, 0x21, 0x07, 0x67, 0x63, 0x2D, 0x63, 0x6F, 0x6E, 0x74, 0x00, 0x23, 0x04,
    0x69, 0x6E, 0x74, 0x72, 0x00, 0x25, 0x09, 0x63, 0x6F, 0x6E, 0x74, 0x2D, 0x69, 0x6E, 0x74, 0x72,
    0x00, 0x26, 0x04, 0x6E, 0x65, 0x73, 0x74, 0x00, 0x29, 0x05, 0x73, 0x70, 0x61, 0x77, 0x6E, 0x00,
    0x2B, 0x06, 0x66, 0x69, 0x6E, 0x69, 0x73, 0x68, 0x00, 0x2C, 0x09, 0x1D, 0x02, 0x02, 0x00, 0x41,
    0x00, 0x0B, 0x00, 0x02, 0x07, 0x09, 0x03, 0x00, 0x10, 0x06, 0x07, 0x08, 0x0A, 0x0F, 0x13, 0x18,
    0x19, 0x1E, 0x22, 0x24, 0x28, 0x2A, 0x16, 0x04, 0x05, 0x0A, 0xC5, 0x08, 0x27, 0x15, 0x00, 0x20,
    0x00, 0x45, 0x04, 0x7F, 0x10, 0x00, 0x05, 0x41, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x10, 0x06,
    0x6A, 0x0B, 0x0B, 0x18, 0x00, 0x20, 0x00, 0x45, 0x04, 0x7F, 0x10, 0x00, 0x05, 0x41, 0x01, 0x20,
    0x00, 0x41, 0x01, 0x6B, 0x41, 0x00, 0x11, 0x02, 0x00, 0x6A, 0x0B, 0x0B, 0x17, 0x00, 0x20, 0x00,
    0x45, 0x04, 0x7F, 0x10, 0x00, 0x05, 0x41, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6B, 0xD2, 0x08, 0x14,
    0x02, 0x6A, 0x0B, 0x0B, 0x47, 0x00, 0x20, 0x00, 0x45, 0x04, 0x40, 0x20, 0x01, 0x10, 0x00, 0x6A,
    0x0F, 0x0B, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x21, 0x00, 0x20, 0x01, 0x41, 0x01, 0x6A, 0x21, 0x01,
    0x20, 0x02, 0x45, 0x04, 0x40, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0x12, 0x09, 0x0B, 0x20, 0x02,
    0x41, 0x01, 0x46, 0x04, 0x40, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0x41, 0x01, 0x13, 0x04, 0x00,
    0x0B, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0xD2, 0x09, 0x15, 0x04, 0x0B, 0x1C, 0x00, 0x20, 0x00,
    0x41, 0x00, 0x41, 0x00, 0x10, 0x09, 0x20, 0x00, 0x41, 0x00, 0x41, 0x01, 0x10, 0x09, 0x6A, 0x20,
    0x00, 0x41, 0x00, 0x41, 0x02, 0x10, 0x09, 0x6A, 0x0B, 0x0B, 0x00, 0x20, 0x00, 0xD2, 0x06, 0xE0,
    0x03, 0xE3, 0x03, 0x00, 0x0B, 0x0B, 0x00, 0x20, 0x00, 0xD2, 0x07, 0xE0, 0x03, 0xE3, 0x03, 0x00,
    0x0B, 0x0B, 0x00, 0x20, 0x00, 0xD2, 0x08, 0xE0, 0x03, 0xE3, 0x03, 0x00, 0x0B, 0x0B, 0x00, 0x20,
    0x00, 0xD2, 0x0A, 0xE0, 0x03, 0xE3, 0x03, 0x00, 0x0B, 0x31, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6A,
    0x20, 0x02, 0x20, 0x03, 0x6A, 0x6A, 0x20, 0x04, 0x20, 0x05, 0x6A, 0x20, 0x06, 0x20, 0x07, 0x6A,
    0x6A, 0x6A, 0x20, 0x08, 0x20, 0x09, 0x6A, 0x20, 0x0A, 0x20, 0x0B, 0x6A, 0x6A, 0x20, 0x0C, 0x20,
    0x0D, 0x6A, 0x20, 0x0E, 0x20, 0x0F, 0x6A, 0x6A, 0x6A, 0x6A, 0x0B, 0x25, 0x00, 0x20, 0x00, 0x20,
    0x01, 0x6A, 0x20, 0x02, 0x20, 0x03, 0x6A, 0x6A, 0x20, 0x04, 0x20, 0x05, 0x6A, 0x20, 0x06, 0x20,
    0x07, 0x6A, 0x6A, 0x6A, 0x20, 0x08, 0x20, 0x09, 0x6A, 0x20, 0x0A, 0x20, 0x0B, 0x6A, 0x6A, 0x6A,
    0x0B, 0x29, 0x00, 0x41, 0x01, 0x41, 0x02, 0x41, 0x03, 0x41, 0x04, 0x41, 0x05, 0x41, 0x06, 0x41,
    0x07, 0x41, 0x08, 0x41, 0x09, 0x41, 0x0A, 0x41, 0x0B, 0x41, 0x0C, 0x41, 0x0D, 0x41, 0x0E, 0x41,
    0x0F, 0x41, 0x10, 0xD2, 0x0F, 0xE0, 0x06, 0xE3, 0x06, 0x00, 0x0B, 0x2C, 0x00, 0x41, 0x10, 0x41,
    0x01, 0x41, 0x02, 0x41, 0x03, 0x41, 0x04, 0x41, 0x05, 0x41, 0x06, 0x41, 0x07, 0x41, 0x08, 0x41,
    0x09, 0x41, 0x0A, 0x41, 0x0B, 0x41, 0x0C, 0x41, 0x0D, 0x41, 0x0E, 0x41, 0x0F, 0xD2, 0x0F, 0xE0,
    0x06, 0xE1, 0x06, 0x03, 0xE3, 0x03, 0x00, 0x0B, 0x0B, 0x00, 0xE2, 0x01, 0x10, 0x0F, 0x41, 0x28,
    0x10, 0x06, 0x6A, 0x0B, 0x41, 0x01, 0x01, 0x63, 0x06, 0x02, 0x64, 0x06, 0xD2, 0x13, 0xE0, 0x01,
    0xE3, 0x01, 0x01, 0x00, 0x01, 0x00, 0x1A, 0x41, 0x7F, 0x0F, 0x0B, 0x21, 0x00, 0x41, 0x10, 0x41,
    0x01, 0x41, 0x02, 0x41, 0x03, 0x41, 0x04, 0x41, 0x05, 0x41, 0x06, 0x41, 0x07, 0x41, 0x08, 0x41,
    0x09, 0x41, 0x0A, 0x41, 0x0B, 0x41, 0x0C, 0x41, 0x0D, 0x41, 0x0E, 0x41, 0x0F, 0x20, 0x00, 0xE1,
    0x06, 0x03, 0xE3, 0x03, 0x00, 0x0B, 0x24, 0x00, 0x41, 0x0C, 0x41, 0x01, 0x41, 0x02, 0x41, 0x03,
    0x41, 0x04, 0x41, 0x05, 0x41, 0x06, 0x41, 0x07, 0x41, 0x08, 0x41, 0x09, 0x41, 0x0A, 0x41, 0x0B,
    0xD2, 0x04, 0xE0, 0x08, 0xE1, 0x08, 0x03, 0xE3, 0x03, 0x00, 0x0B, 0x15, 0x00, 0x41, 0x1E, 0x10,
    0x06, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6A, 0x20, 0x01, 0xE6, 0x0B, 0x02, 0x1A, 0x1A, 0x41, 0x7F,
    0x0B, 0x2A, 0x00, 0x41, 0x0B, 0x41, 0x01, 0x41, 0x02, 0x41, 0x03, 0x41, 0x04, 0x41, 0x05, 0x41,
    0x06, 0x41, 0x07, 0x41, 0x08, 0x41, 0x09, 0x41, 0x0A, 0x41, 0x0B, 0xD2, 0x05, 0xE0, 0x0D, 0xE1,
    0x0D, 0x0B, 0xD2, 0x16, 0xE0, 0x0B, 0xE3, 0x0B, 0x01, 0x01, 0x02, 0x0B, 0x1A, 0x00, 0x02, 0x12,
    0x1F, 0x40, 0x01, 0x00, 0x03, 0x00, 0x20, 0x00, 0xE2, 0x00, 0x0B, 0x41, 0x7F, 0x0F, 0x0B, 0x10,
    0x10, 0x20, 0x00, 0x10, 0x06, 0x6A, 0x0B, 0x14, 0x00, 0x06, 0x7F, 0x20, 0x00, 0xE2, 0x00, 0x41,
    0x7F, 0x07, 0x03, 0x10, 0x10, 0x20, 0x00, 0x10, 0x06, 0x6A, 0x0B, 0x0B, 0x39, 0x01, 0x01, 0x63,
    0x01, 0x02, 0x14, 0x20, 0x00, 0x20, 0x01, 0xE0, 0x03, 0xE3, 0x03, 0x01, 0x00, 0x00, 0x00, 0x1A,
    0x41, 0x7F, 0x0F, 0x0B, 0x21, 0x02, 0x1A, 0x41, 0x01, 0x41, 0x02, 0x41, 0x03, 0x41, 0x04, 0x41,
    0x05, 0x41, 0x06, 0x41, 0x07, 0x41, 0x08, 0x41, 0x09, 0x41, 0x0A, 0x41, 0x0B, 0x41, 0x0C, 0x20,
    0x02, 0xE4, 0x01, 0x03, 0x00, 0x0B, 0x08, 0x00, 0x20, 0x00, 0xD2, 0x18, 0x10, 0x1A, 0x0B, 0x08,
    0x00, 0x20, 0x00, 0xD2, 0x19, 0x10, 0x1A, 0x0B, 0x30, 0x00, 0x02, 0x12, 0x1F, 0x40, 0x01, 0x00,
    0x03, 0x00, 0x20, 0x00, 0x10, 0x06, 0x1A, 0x41, 0x01, 0x41, 0x02, 0x41, 0x03, 0x41, 0x04, 0x41,
    0x05, 0x41, 0x06, 0x41, 0x07, 0x41, 0x08, 0x41, 0x09, 0x41, 0x0A, 0x41, 0x0B, 0x41, 0x0C, 0x08,
    0x03, 0x0B, 0x41, 0x7F, 0x0F, 0x0B, 0x10, 0x10, 0x0B, 0x0D, 0x00, 0x41, 0xE8, 0x07, 0x20, 0x00,
    0x20, 0x00, 0x10, 0x01, 0x6A, 0x6A, 0x0B, 0x06, 0x00, 0x20, 0x00, 0x10, 0x1E, 0x0B, 0x0B, 0x00,
    0x20, 0x00, 0xD2, 0x1E, 0xE0, 0x03, 0xE3, 0x03, 0x00, 0x0B, 0x0B, 0x00, 0x20, 0x00, 0xD2, 0x06,
    0xE0, 0x03, 0xE3, 0x03, 0x00, 0x0B, 0x38, 0x02, 0x01, 0x63, 0x09, 0x01, 0x7F, 0x20, 0x00, 0xFB,
    0x00, 0x09, 0x21, 0x01, 0x20, 0x00, 0x45, 0x04, 0x7F, 0x10, 0x03, 0x41, 0x00, 0x05, 0x20, 0x00,
    0x41, 0x02, 0x6C, 0xFB, 0x00, 0x09, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x10, 0x22, 0x21, 0x02, 0xFB,
    0x02, 0x09, 0x00, 0x20, 0x02, 0x6A, 0x20, 0x01, 0xFB, 0x02, 0x09, 0x00, 0x6A, 0x0B, 0x0B, 0x0B,
    0x00, 0x20, 0x00, 0xD2, 0x22, 0xE0, 0x03, 0xE3, 0x03, 0x00, 0x0B, 0x17, 0x00, 0x20, 0x00, 0x45,
    0x04, 0x7F, 0x41, 0x00, 0x05, 0x10, 0x02, 0x41, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x10, 0x24,
    0x6A, 0x0B, 0x0B, 0x06, 0x00, 0x20, 0x00, 0x10, 0x24, 0x0B, 0x0B, 0x00, 0x20, 0x00, 0xD2, 0x24,
    0xE0, 0x03, 0xE3, 0x03, 0x00, 0x0B, 0x28, 0x00, 0x20, 0x01, 0x04, 0x7F, 0x41, 0x01, 0x20, 0x00,
    0x20, 0x01, 0x41, 0x01, 0x6B, 0x10, 0x27, 0x6A, 0x05, 0x20, 0x00, 0x04, 0x7F, 0x20, 0x00, 0x41,
    0x01, 0x6B, 0xD2, 0x28, 0xE0, 0x03, 0xE3, 0x03, 0x00, 0x05, 0x10, 0x00, 0x0B, 0x0B, 0x0B, 0x09,
    0x00, 0x20, 0x00, 0x41, 0xAC, 0x02, 0x10, 0x27, 0x0B, 0x0B, 0x00, 0x20, 0x00, 0xD2, 0x28, 0xE0,
    0x03, 0xE3, 0x03, 0x00, 0x0B, 0x0B, 0x00, 0x20, 0x00, 0xE2, 0x00, 0x20, 0x00, 0x41, 0x02, 0x6C,
    0x0B, 0x3F, 0x02, 0x01, 0x7F, 0x01, 0x63, 0x01, 0xD0, 0x01, 0x20, 0x00, 0xFC, 0x0F, 0x01, 0x1A,
    0x02, 0x40, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00, 0x4F, 0x0D, 0x01, 0x02, 0x14, 0x20, 0x01, 0xD2,
    0x2A, 0xE0, 0x03, 0xE3, 0x03, 0x01, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x0B, 0x21, 0x02, 0x1A, 0x20,
    0x01, 0x20, 0x02, 0x26, 0x01, 0x20, 0x01, 0x41, 0x01, 0x6A, 0x21, 0x01, 0x0C, 0x00, 0x0B, 0x0B,
    0x0B, 0x2F, 0x01, 0x02, 0x7F, 0x02, 0x40, 0x03, 0x40, 0x20, 0x00, 0xFC, 0x10, 0x01, 0x4F, 0x0D,
    0x01, 0x20, 0x01, 0x20, 0x00, 0x25, 0x01, 0xE3, 0x01, 0x00, 0x6A, 0x21, 0x01, 0x20, 0x00, 0xD0,
    0x01, 0x26, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6A, 0x21, 0x00, 0x0C, 0x00, 0x0B, 0x0B, 0x20, 0x01,
    0x0B,
};

#define WASM_TEST_GROW_REC 6u
#define WASM_TEST_GROW_CONT_REC 11u

static wasm_module_t* wasm_test_grow_module;
static int wasm_test_grow_yield;
static uint32_t wasm_test_grow_frames;
static uint32_t wasm_test_grow_cap_frames;
static uint32_t wasm_test_grow_cap_slots;
static uint32_t wasm_test_grow_depth;

static wasm_error_t wasm_test_grow_probe(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_runtime_t* rt = wasm_module_runtime(mod);

    (void)args;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    wasm_test_grow_frames = rt->exec->call_frame_sp;
    wasm_test_grow_cap_frames = rt->exec->cap_call_frames;
    wasm_test_grow_cap_slots = rt->exec->cap_stack;
    wasm_test_grow_depth = wasm_runtime_call_depth(rt);
    if (wasm_test_grow_yield) return WASM_YIELD;
    results[0] = wasm_i32(0);
    return WASM_OK;
}

static wasm_error_t wasm_test_grow_reenter(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_value_t first = wasm_i32(0);
    wasm_value_t second = wasm_i32(0);
    wasm_error_t err;

    (void)mod;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    err = wasm_call(wasm_test_grow_module, "rec", args, 1, &first, 1);
    if (err == WASM_OK) err = wasm_call(wasm_test_grow_module, "deep-cont", args, 1, &second, 1);
    results[0] = wasm_i32(first.of.i32 + second.of.i32);
    return err;
}

static wasm_error_t wasm_test_grow_interrupt(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_runtime_interrupt(wasm_module_runtime(mod));
    return WASM_OK;
}

static wasm_error_t wasm_test_grow_collect(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_gc_collect(wasm_module_runtime(mod));
    return WASM_OK;
}

static wasm_error_t wasm_test_grow_sum12(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results, void* userdata) {
    int32_t sum = 0;
    uint32_t i;

    (void)mod;
    (void)num_results;
    (void)userdata;
    for (i = 0; i < 12u && i < num_args; i++) sum += args[i].of.i32;
    results[0] = wasm_i32(sum);
    return WASM_OK;
}

static void wasm_test_grow_register(wasm_runtime_t* rt, const char* name, wasm_host_func_t func) {
    wasm_import_t imp;

    memset(&imp, 0, sizeof(imp));
    imp.module = "host";
    imp.name = name;
    imp.func = func;
    wasm_register_import(rt, &imp);
}

/* Fills cfg with stacks that start at one slot and one frame and may grow to 65536 slots and 4096 frames. */
static void wasm_test_grow_config(wasm_config_t* cfg) {
    wasm_config_default(cfg);
    cfg->max_stack_values = 65536;
    cfg->max_call_depth = 4096;
    cfg->initial_stack_values = 1;
    cfg->initial_call_depth = 1;
    cfg->cont_stack_values = 65536;
    cfg->cont_call_depth = 4096;
    cfg->cont_initial_stack_values = 1;
    cfg->cont_initial_call_depth = 1;
}

static wasm_module_t* wasm_test_grow_load(test_ctx* t, wasm_runtime_t* rt, const wasm_config_t* cfg) {
    wasm_module_t* m;

    wasm_init_config(rt, cfg);
    wasm_test_grow_register(rt, "probe", wasm_test_grow_probe);
    wasm_test_grow_register(rt, "reenter", wasm_test_grow_reenter);
    wasm_test_grow_register(rt, "interrupt", wasm_test_grow_interrupt);
    wasm_test_grow_register(rt, "collect", wasm_test_grow_collect);
    wasm_test_grow_register(rt, "sum12", wasm_test_grow_sum12);
    wasm_test_grow_register(rt, "switch12", wasm_test_grow_sum12);
    m = wasm_test_load_sections(rt, wasm_test_grow_sections, sizeof(wasm_test_grow_sections));
    CHECK_MSG(t, m != NULL, "%s", rt->error_msg);
    wasm_test_grow_module = m;
    wasm_test_grow_yield = 0;
    return m;
}

/* Calls export name of m with one i32 argument and checks it returns expected. */
static void wasm_test_grow_expect(test_ctx* t, wasm_module_t* m, const char* name, int32_t arg, int32_t expected) {
    wasm_value_t value = wasm_i32(arg);
    wasm_value_t result = wasm_i32(0);
    wasm_error_t err = wasm_call(m, name, &value, 1, &result, 1);

    CHECK_MSG(t, err == WASM_OK, "%s(%d): %s", name, (int)arg, wasm_module_runtime(m)->error_msg);
    CHECK_MSG(t, err != WASM_OK || result.of.i32 == expected, "%s(%d): expected %d, got %d", name, (int)arg,
              (int)expected, (int)result.of.i32);
}

/* Checks that every pooled continuation stack of rt is back at its initial size. */
static void wasm_test_grow_check_pool(test_ctx* t, const wasm_runtime_t* rt) {
    const wasm__exec_stack_t* s;

    for (s = rt->cont_stack_pool; s; s = s->next) {
        CHECK_MSG(t, s->cap_stack <= rt->cont_initial_stack_values && s->cap_call_frames <= rt->cont_initial_call_depth,
                  "pooled stack holds %u slots and %u frames", (unsigned)s->cap_stack, (unsigned)s->cap_call_frames);
    }
}

TEST(test_exec_stack_grows_by_doubling_to_its_limits) {
    wasm_runtime_t rt;
    wasm__exec_stack_t s;

    memset(&rt, 0, sizeof(rt));
    rt.allocator = wasm_test_allocator;
    WASM_CHECK_OK(t, wasm__exec_stack_init(&rt, &s, 2, 10, 1, 3, 64));
#ifndef WASM_STACK_STRESS
    CHECK(t, s.cap_stack == 2 && s.cap_call_frames == 1 && s.max_stack == 10 && s.max_call_frames == 3);
#endif
    WASM_CHECK_OK(t, wasm__exec_stack_reserve(&rt, &s, 3));
#ifndef WASM_STACK_STRESS
    CHECK_MSG(t, s.cap_stack == 4, "expected 4 slots, got %u", (unsigned)s.cap_stack);
#endif
    s.stack[0].u64 = 7;
    s.stack[2].u64 = 9;
    s.sp = 4;
    CHECK(t, wasm__exec_stack_reserve(&rt, &s, 7) == WASM_ERR_STACK_OVERFLOW);
    WASM_CHECK_OK(t, wasm__exec_stack_reserve(&rt, &s, 6));
    CHECK(t, s.cap_stack == 10 && s.stack[0].u64 == 7 && s.stack[2].u64 == 9);
    WASM_CHECK_OK(t, wasm__exec_stack_reserve_frame(&rt, &s));
    s.call_frame_sp = 1;
    WASM_CHECK_OK(t, wasm__exec_stack_reserve_frame(&rt, &s));
#ifndef WASM_STACK_STRESS
    CHECK_MSG(t, s.cap_call_frames == 2, "expected 2 frames, got %u", (unsigned)s.cap_call_frames);
#endif
    s.call_frame_sp = 2;
    WASM_CHECK_OK(t, wasm__exec_stack_reserve_frame(&rt, &s));
    CHECK_MSG(t, s.cap_call_frames == 3, "expected 3 frames, got %u", (unsigned)s.cap_call_frames);
    s.call_frame_sp = 3;
    CHECK(t, wasm__exec_stack_reserve_frame(&rt, &s) == WASM_ERR_CALL_STACK_EXHAUSTED);
    s.call_frame_sp = 0;
    s.sp = 0;
    wasm__exec_stack_trim(&rt, &s, 2, 1);
    CHECK(t, s.cap_stack == 2 && s.cap_call_frames == 1);
    wasm__exec_stack_destroy(&rt, &s);
    wasm__arena_free_spare(&rt);

    WASM_CHECK_OK(t, wasm__exec_stack_init(&rt, &s, 0, 10, 20, 3, 64));
    CHECK(t, wasm_test_alloc_snapshot().mismatches == 0);
#ifndef WASM_STACK_STRESS
    CHECK(t, s.cap_stack == 10 && s.cap_call_frames == 3);
#endif
    wasm__exec_stack_destroy(&rt, &s);
    wasm__arena_free_spare(&rt);
    CHECK(t, wasm__exec_stack_init(&rt, &s, 1, 0, 1, 1, 64) == WASM_ERR_MALFORMED);
}

TEST(test_stack_growth_during_calls) {
    static const char* const names[] = { "rec", "cont-rec", "cont-rec-indirect", "cont-rec-ref" };
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t i;

    wasm_test_grow_config(&cfg);
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(names); i++) {
        wasm_test_grow_expect(t, m, names[i], 1000, 1000);
        CHECK_MSG(t, wasm_test_grow_frames == 1001u && wasm_test_grow_cap_frames >= 1001u,
                  "%s: %u frames in %u", names[i], (unsigned)wasm_test_grow_frames, (unsigned)wasm_test_grow_cap_frames);
        CHECK_MSG(t, wasm_test_grow_cap_slots >= 1000u, "%s: %u slots", names[i], (unsigned)wasm_test_grow_cap_slots);
        CHECK(t, rt.exec == &rt.main_stack && rt.num_cont_stacks == 0);
    }
    CHECK(t, rt.main_stack.cap_call_frames >= 1001u && rt.main_stack.call_frame_sp == 0 && rt.main_stack.sp == 0);
    wasm_test_grow_check_pool(t, &rt);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_growth_tail_calls_keep_one_frame) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_test_grow_config(&cfg);
    cfg.cont_call_depth = 4;
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_grow_expect(t, m, "cont-tail", 20000, 60000);
    CHECK_MSG(t, wasm_test_grow_frames == 2u && wasm_test_grow_cap_frames <= 4u, "%u frames in %u",
              (unsigned)wasm_test_grow_frames, (unsigned)wasm_test_grow_cap_frames);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_growth_continuation_arguments) {
    static const struct {
        const char* name;
        int32_t expected;
    } cases[] = {
        { "resume-args", 136 }, { "bind-args", 136 }, { "bind-suspended", 176 },
        { "resume-host", 78 },  { "switch-host", 78 },
    };
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t i;

    wasm_test_grow_config(&cfg);
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(cases); i++) {
        wasm_value_t result = wasm_i32(0);
        wasm_error_t err = wasm_call(m, cases[i].name, NULL, 0, &result, 1);

        CHECK_MSG(t, err == WASM_OK, "%s: %s", cases[i].name, rt.error_msg);
        CHECK_MSG(t, result.of.i32 == cases[i].expected, "%s: expected %d, got %d", cases[i].name,
                  (int)cases[i].expected, (int)result.of.i32);
        wasm_gc_collect(&rt);
        CHECK_MSG(t, rt.exec == &rt.main_stack && rt.num_cont_stacks == 0, "%s: %u stacks left", cases[i].name,
                  (unsigned)rt.num_cont_stacks);
    }
    wasm_test_grow_check_pool(t, &rt);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_growth_while_handling_exceptions) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_test_grow_config(&cfg);
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_grow_expect(t, m, "throw-deep", 500, 578);
    wasm_test_grow_expect(t, m, "throw-legacy", 500, 578);
    wasm_test_grow_expect(t, m, "catch-payload", 500, 78);
    CHECK(t, rt.exec == &rt.main_stack && rt.num_cont_stacks == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_growth_in_host_callbacks) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_test_grow_config(&cfg);
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_grow_expect(t, m, "reenter", 300, 1900);
    wasm_test_grow_expect(t, m, "cont-reenter", 300, 1900);
    wasm_test_grow_expect(t, m, "cont-reenter", 600, 2800);
    CHECK(t, rt.exec == &rt.main_stack && rt.num_cont_stacks == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* Calls export name of m with arg and checks it fails with err, message text, and a backtrace of depth frames
 * whose innermost are func and whose outermost is outer. */
static void wasm_test_grow_expect_trap(test_ctx* t, wasm_module_t* m, const char* name, int32_t arg,
                                       wasm_error_t err, const char* text, uint32_t depth, uint32_t func,
                                       uint32_t outer) {
    wasm_runtime_t* rt = wasm_module_runtime(m);
    wasm_value_t value = wasm_i32(arg);
    wasm_value_t result;
    wasm_error_t actual = wasm_call(m, name, &value, 1, &result, 1);
    uint32_t func_idx = UINT32_MAX;
    uint32_t i;

    CHECK_MSG(t, actual == err, "%s: expected error %d, got %d", name, (int)err, (int)actual);
    CHECK_MSG(t, strcmp(rt->error_msg, text) == 0, "%s: %s", name, rt->error_msg);
    CHECK_MSG(t, wasm_runtime_call_depth(rt) == depth, "%s: backtrace of %u frames", name,
              (unsigned)wasm_runtime_call_depth(rt));
    for (i = 0; i + 1u < depth; i++) {
        WASM_CHECK_OK(t, wasm_runtime_call_frame(rt, i, NULL, &func_idx, NULL));
        CHECK_MSG(t, func_idx == func, "%s: frame %u runs function %u", name, (unsigned)i, (unsigned)func_idx);
    }
    WASM_CHECK_OK(t, wasm_runtime_call_frame(rt, depth - 1u, NULL, &func_idx, NULL));
    CHECK_MSG(t, func_idx == outer, "%s: outermost frame runs function %u", name, (unsigned)func_idx);
    CHECK(t, rt->exec == &rt->main_stack && rt->exec->call_frame_sp == 0 && rt->exec->sp == 0);
}

TEST(test_stack_growth_stops_at_limits) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;

    wasm_test_grow_config(&cfg);
    cfg.cont_call_depth = 50;
    cfg.max_call_depth = 60;
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (m) {
        wasm_test_grow_expect(t, m, "cont-rec", 49, 49);
        wasm_test_grow_expect_trap(t, m, "cont-rec", 100, WASM_ERR_CALL_STACK_EXHAUSTED,
                                   "call frame depth exceeded limit 50", 51, WASM_TEST_GROW_REC,
                                   WASM_TEST_GROW_CONT_REC);
        wasm_test_grow_expect(t, m, "rec", 59, 59);
        wasm_test_grow_expect_trap(t, m, "rec", 100, WASM_ERR_CALL_STACK_EXHAUSTED,
                                   "call frame depth exceeded limit 60", 60, WASM_TEST_GROW_REC, WASM_TEST_GROW_REC);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);

    wasm_test_grow_config(&cfg);
    cfg.cont_stack_values = 40;
    cfg.max_stack_values = 30;
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (m) {
        wasm_test_grow_expect_trap(t, m, "cont-rec", 100, WASM_ERR_STACK_OVERFLOW, "operand stack limit 40 exceeded",
                                   39, WASM_TEST_GROW_REC, WASM_TEST_GROW_CONT_REC);
        wasm_test_grow_expect_trap(t, m, "rec", 100, WASM_ERR_STACK_OVERFLOW, "operand stack limit 30 exceeded", 28,
                                   WASM_TEST_GROW_REC, WASM_TEST_GROW_REC);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);

    wasm_test_grow_config(&cfg);
    cfg.cont_stack_values = 8;
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (m) {
        CHECK(t, wasm_call(m, "resume-args", NULL, 0, &result, 1) == WASM_ERR_STACK_OVERFLOW);
        CHECK_MSG(t, strcmp(rt.error_msg, "continuation operand stack limit 8 exceeded") == 0, "%s", rt.error_msg);
        CHECK(t, wasm_call(m, "bind-args", NULL, 0, &result, 1) == WASM_ERR_STACK_OVERFLOW);
        CHECK_MSG(t, strcmp(rt.error_msg, "continuation operand stack limit 8 exceeded") == 0, "%s", rt.error_msg);
        wasm_gc_collect(&rt);
        CHECK(t, rt.exec == &rt.main_stack && rt.num_cont_stacks == 0);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_stack_growth_under_collections) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_test_grow_config(&cfg);
    cfg.initial_gc_heap_size = 4096;
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_grow_expect(t, m, "gc-cont", 400, 240600);
    wasm_test_grow_expect(t, m, "gc-cont", 1000, 1501500);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_growth_then_fuel_interrupt_and_yield_stops) {
    static const char* const names[] = { "rec", "cont-rec" };
    static const char* const intr_names[] = { "intr", "cont-intr" };
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    wasm_value_t host_result = wasm_i32(5);
    wasm_error_t err;
    uint32_t i;

    wasm_test_grow_config(&cfg);
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(names); i++) {
        uint32_t steps;

        arg = wasm_i32(300);
        steps = wasm_test_run_in_fuel_steps(t, m, names[i], &arg, 1, 3, 0, &result);
        CHECK_MSG(t, steps > 300u && result.of.i32 == 300, "%s: %u steps, result %d", names[i], (unsigned)steps,
                  (int)result.of.i32);
    }
    for (i = 0; i < COUNTOF(intr_names); i++) {
        uint32_t stops = 0;

        arg = wasm_i32(100);
        err = wasm_call(m, intr_names[i], &arg, 1, &result, 1);
        while (err == WASM_ERR_INTERRUPTED && stops < 1000u) {
            CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
            stops++;
            err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
        }
        CHECK_MSG(t, err == WASM_OK && stops == 100u && result.of.i32 == 100, "%s: %s after %u stops, result %d",
                  intr_names[i], wasm_error_string(err), (unsigned)stops, (int)result.of.i32);
    }
    for (i = 0; i < COUNTOF(names); i++) {
        arg = wasm_i32(300);
        wasm_test_grow_yield = 1;
        err = wasm_call(m, names[i], &arg, 1, &result, 1);
        wasm_test_grow_yield = 0;
        CHECK_MSG(t, err == WASM_YIELD && wasm_runtime_suspended(&rt) == WASM_YIELD, "%s: %s", names[i],
                  wasm_error_string(err));
        err = wasm_runtime_resume(&rt, &host_result, 1, &result, 1);
        CHECK_MSG(t, err == WASM_OK && result.of.i32 == 305, "%s: %s, result %d", names[i], wasm_error_string(err),
                  (int)result.of.i32);
    }
    CHECK(t, rt.exec == &rt.main_stack && rt.num_cont_stacks == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_growth_across_nested_continuations) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_test_grow_config(&cfg);
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_grow_expect(t, m, "nest", 5, 1800);
    CHECK_MSG(t, wasm_test_grow_frames == 302u && wasm_test_grow_depth == 6u * 302u + 1u,
              "innermost stack holds %u frames, %u in all", (unsigned)wasm_test_grow_frames,
              (unsigned)wasm_test_grow_depth);
    CHECK(t, rt.exec == &rt.main_stack && rt.num_cont_stacks == 0);
    wasm_test_grow_check_pool(t, &rt);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_growth_keeps_suspended_fibers_small) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg = wasm_i32(2000);
    wasm_value_t result = wasm_i32(0);
    const wasm__exec_stack_t* s;
    uint32_t live = 0;

    wasm_config_default(&cfg);
    cfg.cont_call_depth = 2000;
    m = wasm_test_grow_load(t, &rt, &cfg);
    if (!m) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_grow_expect(t, m, "cont-rec", 1000, 1000);
    WASM_CHECK_OK(t, wasm_call(m, "spawn", &arg, 1, NULL, 0));
    for (s = rt.cont_stacks; s; s = s->next) {
        live++;
        CHECK_MSG(t, s->cap_stack <= cfg.cont_initial_stack_values && s->cap_call_frames <= cfg.cont_initial_call_depth,
                  "suspended fiber holds %u slots and %u frames", (unsigned)s->cap_stack,
                  (unsigned)s->cap_call_frames);
        if (s->cap_stack > cfg.cont_initial_stack_values) break;
    }
    CHECK_MSG(t, live == 2000u && rt.num_cont_stacks == 2000u, "%u live stacks", (unsigned)live);
    CHECK_MSG(t, rt.cont_stack_bytes <= (size_t)2000u * 4096u, "%zu bytes of suspended stacks", rt.cont_stack_bytes);
    WASM_CHECK_OK(t, wasm_call(m, "finish", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 2000 * 1999);
    CHECK(t, rt.num_cont_stacks == 0 && rt.cont_stack_bytes == 0);
    wasm_test_grow_check_pool(t, &rt);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_stack_cases[] = {
    { "runtime: operand stack overflow traps at the call that needs the space", test_runtime_operand_stack_overflow_traps_at_call },
    { "runtime: frame arena alloc/reset/destroy are well-formed", test_frame_arena_allocator },
    { "runtime: top-level execution grows the frame arena for validated modules", test_runtime_grows_frame_arena_for_execution },
    { "runtime: a new stack writes each call frame it pushes before reading it", test_new_stacks_write_each_call_frame_they_push },
    { "stacks: operand slots and call frames double up to their limits", test_exec_stack_grows_by_doubling_to_its_limits },
    { "stacks: calls, call_indirect, and call_ref grow main and continuation stacks", test_stack_growth_during_calls },
    { "stacks: tail calls in a continuation reuse their frame", test_stack_growth_tail_calls_keep_one_frame },
    { "stacks: resume, cont.bind, and switch arguments grow the stacks they land on", test_stack_growth_continuation_arguments },
    { "stacks: resume_throw and catches with payloads run on grown stacks", test_stack_growth_while_handling_exceptions },
    { "stacks: host callbacks that reenter Wasm grow the stack under a running frame", test_stack_growth_in_host_callbacks },
    { "stacks: growth stops at each limit with its trap and backtrace", test_stack_growth_stops_at_limits },
    { "stacks: collections during deep recursion keep frames intact", test_stack_growth_under_collections },
    { "stacks: fuel, interrupt, and yield stops after growth resume", test_stack_growth_then_fuel_interrupt_and_yield_stops },
    { "stacks: nested continuations each grow their own stack", test_stack_growth_across_nested_continuations },
    { "stacks: suspended fibers keep their initial size", test_stack_growth_keeps_suspended_fibers_small },
};
const test_suite exec_stack_suite = { "exec.stack", exec_stack_cases, COUNTOF(exec_stack_cases) };

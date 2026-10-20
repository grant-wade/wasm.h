#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/exec/state.h"
#endif

/* (module (type $f0 (func)) (type $c0 (cont $f0)) (type $f1 (func (param i32))) (type $c1 (cont $f1))
     (tag $yield (param i32))
     (func $produce (type $f1)
       (loop $l (suspend $yield (local.get 0))
         (local.set 0 (i32.sub (local.get 0) (i32.const 1))) (br_if $l (local.get 0))))
     (elem declare func $produce)
     (func (export "sum") (param $n i32) (result i32) (local $k (ref null $c0)) (local $acc i32)
       (local.set $k (cont.bind $c1 $c0 (local.get $n) (cont.new $c1 (ref.func $produce))))
       (block $done
         (loop $next
           (block $on_yield (result i32 (ref $c0))
             (resume $c0 (on $yield $on_yield) (local.get $k))
             (br $done))
           (local.set $k) (local.set $acc (i32.add (local.get $acc))) (br $next)))
       (local.get $acc))) */
static const uint8_t wasm_test_cont_generator_sections[] = {
    0x01, 0x17, 0x06, 0x60, 0x00, 0x00, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x5D, 0x02, 0x60, 0x01,
    0x7F, 0x01, 0x7F, 0x60, 0x00, 0x02, 0x7F, 0x64, 0x01, 0x03, 0x03, 0x02, 0x02, 0x04, 0x0D, 0x03,
    0x01, 0x00, 0x02, 0x07, 0x07, 0x01, 0x03, 0x73, 0x75, 0x6D, 0x00, 0x01, 0x09, 0x05, 0x01, 0x03,
    0x00, 0x01, 0x00, 0x0A, 0x47, 0x02, 0x14, 0x00, 0x03, 0x40, 0x20, 0x00, 0xE2, 0x00, 0x20, 0x00,
    0x41, 0x01, 0x6B, 0x21, 0x00, 0x20, 0x00, 0x0D, 0x00, 0x0B, 0x0B, 0x30, 0x02, 0x01, 0x63, 0x01,
    0x01, 0x7F, 0x20, 0x00, 0xD2, 0x00, 0xE0, 0x03, 0xE1, 0x03, 0x01, 0x21, 0x01, 0x02, 0x40, 0x03,
    0x40, 0x02, 0x05, 0x20, 0x01, 0xE3, 0x01, 0x01, 0x00, 0x00, 0x00, 0x0C, 0x02, 0x0B, 0x21, 0x01,
    0x20, 0x02, 0x6A, 0x21, 0x02, 0x0C, 0x00, 0x0B, 0x0B, 0x20, 0x02, 0x0B,
};

/* (module (type $f0 (func)) (type $c0 (cont $f0)) (type $f1 (func (param i32))) (type $c1 (cont $f1))
     (func $nop) (func $nop1 (param i32)) (elem declare func $nop $nop1)
     (func (export "resume-twice") (local $k (ref null $c0))
       (local.set $k (cont.new $c0 (ref.func $nop)))
       (resume $c0 (local.get $k)) (resume $c0 (local.get $k)))
     (func (export "bind-twice") (local $k (ref null $c1))
       (local.set $k (cont.new $c1 (ref.func $nop1)))
       (drop (cont.bind $c1 $c0 (i32.const 1) (local.get $k)))
       (drop (cont.bind $c1 $c0 (i32.const 2) (local.get $k))))
     (func (export "resume-after-bind") (local $k (ref null $c1))
       (local.set $k (cont.new $c1 (ref.func $nop1)))
       (drop (cont.bind $c1 $c0 (i32.const 1) (local.get $k)))
       (resume $c1 (i32.const 2) (local.get $k)))
     (func (export "null-resume") (resume $c0 (ref.null $c0)))
     (func (export "null-bind") (drop (cont.bind $c1 $c0 (i32.const 1) (ref.null $c1))))
     (func (export "null-new") (drop (cont.new $c0 (ref.null $f0))))) */
static const uint8_t wasm_test_cont_oneshot_sections[] = {
    0x01, 0x0C, 0x04, 0x60, 0x00, 0x00, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x5D, 0x02, 0x03, 0x09,
    0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x56, 0x06, 0x0C, 0x72, 0x65, 0x73,
    0x75, 0x6D, 0x65, 0x2D, 0x74, 0x77, 0x69, 0x63, 0x65, 0x00, 0x02, 0x0A, 0x62, 0x69, 0x6E, 0x64,
    0x2D, 0x74, 0x77, 0x69, 0x63, 0x65, 0x00, 0x03, 0x11, 0x72, 0x65, 0x73, 0x75, 0x6D, 0x65, 0x2D,
    0x61, 0x66, 0x74, 0x65, 0x72, 0x2D, 0x62, 0x69, 0x6E, 0x64, 0x00, 0x04, 0x0B, 0x6E, 0x75, 0x6C,
    0x6C, 0x2D, 0x72, 0x65, 0x73, 0x75, 0x6D, 0x65, 0x00, 0x05, 0x09, 0x6E, 0x75, 0x6C, 0x6C, 0x2D,
    0x62, 0x69, 0x6E, 0x64, 0x00, 0x06, 0x08, 0x6E, 0x75, 0x6C, 0x6C, 0x2D, 0x6E, 0x65, 0x77, 0x00,
    0x07, 0x09, 0x06, 0x01, 0x03, 0x00, 0x02, 0x00, 0x01, 0x0A, 0x6F, 0x08, 0x02, 0x00, 0x0B, 0x02,
    0x00, 0x0B, 0x15, 0x01, 0x01, 0x63, 0x01, 0xD2, 0x00, 0xE0, 0x01, 0x21, 0x00, 0x20, 0x00, 0xE3,
    0x01, 0x00, 0x20, 0x00, 0xE3, 0x01, 0x00, 0x0B, 0x1B, 0x01, 0x01, 0x63, 0x03, 0xD2, 0x01, 0xE0,
    0x03, 0x21, 0x00, 0x41, 0x01, 0x20, 0x00, 0xE1, 0x03, 0x01, 0x1A, 0x41, 0x02, 0x20, 0x00, 0xE1,
    0x03, 0x01, 0x1A, 0x0B, 0x1A, 0x01, 0x01, 0x63, 0x03, 0xD2, 0x01, 0xE0, 0x03, 0x21, 0x00, 0x41,
    0x01, 0x20, 0x00, 0xE1, 0x03, 0x01, 0x1A, 0x41, 0x02, 0x20, 0x00, 0xE3, 0x03, 0x00, 0x0B, 0x07,
    0x00, 0xD0, 0x01, 0xE3, 0x01, 0x00, 0x0B, 0x0A, 0x00, 0x41, 0x01, 0xD0, 0x03, 0xE1, 0x03, 0x01,
    0x1A, 0x0B, 0x07, 0x00, 0xD0, 0x00, 0xE0, 0x01, 0x1A, 0x0B,
};

/* (module (type $f0 (func)) (type $c0 (cont $f0)) (tag $t) (tag $u)
     (func $suspends (suspend $t)) (elem declare func $suspends)
     (func (export "top") (suspend $t))
     (func (export "inner")
       (block $h (result (ref $c0)) (resume $c0 (on $u $h) (cont.new $c0 (ref.func $suspends))) (return))
       (drop))
     (func (export "handled") (result i32)
       (block $h (result (ref $c0))
         (resume $c0 (on $u $h) (on $t $h) (cont.new $c0 (ref.func $suspends))) (return (i32.const 0)))
       (drop) (i32.const 1))) */
static const uint8_t wasm_test_cont_unhandled_sections[] = {
    0x01, 0x0A, 0x03, 0x60, 0x00, 0x00, 0x5D, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x03, 0x05, 0x04, 0x00,
    0x00, 0x00, 0x02, 0x0D, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x07, 0x19, 0x03, 0x03, 0x74, 0x6F,
    0x70, 0x00, 0x01, 0x05, 0x69, 0x6E, 0x6E, 0x65, 0x72, 0x00, 0x02, 0x07, 0x68, 0x61, 0x6E, 0x64,
    0x6C, 0x65, 0x64, 0x00, 0x03, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x38, 0x04, 0x04,
    0x00, 0xE2, 0x00, 0x0B, 0x04, 0x00, 0xE2, 0x00, 0x0B, 0x12, 0x00, 0x02, 0x64, 0x01, 0xD2, 0x00,
    0xE0, 0x01, 0xE3, 0x01, 0x01, 0x00, 0x01, 0x00, 0x0F, 0x0B, 0x1A, 0x0B, 0x19, 0x00, 0x02, 0x64,
    0x01, 0xD2, 0x00, 0xE0, 0x01, 0xE3, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00,
    0x0F, 0x0B, 0x1A, 0x41, 0x01, 0x0B,
};

/* (module (type $f0 (func)) (type $c0 (cont $f0)) (tag $t)
     (import "host" "call_back" (func $call_back (param i32)))
     (func $nop) (func $suspends (suspend $t))
     (func $via_host (call $call_back (i32.const 0))) (func $via_host_ok (call $call_back (i32.const 1)))
     (elem declare func $nop $suspends $via_host $via_host_ok)
     (func (export "nested") (param i32)
       (if (local.get 0)
         (then
           (resume $c0 (cont.new $c0 (ref.func $nop)))
           (block $h (result (ref $c0)) (resume $c0 (on $t $h) (cont.new $c0 (ref.func $suspends))) (return))
           (drop) (return)))
       (suspend $t))
     (func (export "across") (result i32)
       (block $h (result (ref $c0))
         (resume $c0 (on $t $h) (cont.new $c0 (ref.func $via_host))) (return (i32.const 0)))
       (drop) (i32.const 1))
     (func (export "inside") (result i32)
       (block $h (result (ref $c0))
         (resume $c0 (on $t $h) (cont.new $c0 (ref.func $via_host_ok))) (return (i32.const 0)))
       (drop) (i32.const 1))) */
static const uint8_t wasm_test_cont_barrier_sections[] = {
    0x01, 0x0E, 0x04, 0x60, 0x00, 0x00, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x01, 0x7F,
    0x02, 0x12, 0x01, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x09, 0x63, 0x61, 0x6C, 0x6C, 0x5F, 0x62, 0x61,
    0x63, 0x6B, 0x00, 0x02, 0x03, 0x08, 0x07, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0x03, 0x0D, 0x03,
    0x01, 0x00, 0x00, 0x07, 0x1C, 0x03, 0x06, 0x6E, 0x65, 0x73, 0x74, 0x65, 0x64, 0x00, 0x05, 0x06,
    0x61, 0x63, 0x72, 0x6F, 0x73, 0x73, 0x00, 0x06, 0x06, 0x69, 0x6E, 0x73, 0x69, 0x64, 0x65, 0x00,
    0x07, 0x09, 0x08, 0x01, 0x03, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04, 0x0A, 0x67, 0x07, 0x02, 0x00,
    0x0B, 0x04, 0x00, 0xE2, 0x00, 0x0B, 0x06, 0x00, 0x41, 0x00, 0x10, 0x00, 0x0B, 0x06, 0x00, 0x41,
    0x01, 0x10, 0x00, 0x0B, 0x21, 0x00, 0x20, 0x00, 0x04, 0x40, 0xD2, 0x01, 0xE0, 0x01, 0xE3, 0x01,
    0x00, 0x02, 0x64, 0x01, 0xD2, 0x02, 0xE0, 0x01, 0xE3, 0x01, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x0B,
    0x1A, 0x0F, 0x0B, 0xE2, 0x00, 0x0B, 0x16, 0x00, 0x02, 0x64, 0x01, 0xD2, 0x03, 0xE0, 0x01, 0xE3,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x41, 0x00, 0x0F, 0x0B, 0x1A, 0x41, 0x01, 0x0B, 0x16, 0x00, 0x02,
    0x64, 0x01, 0xD2, 0x04, 0xE0, 0x01, 0xE3, 0x01, 0x01, 0x00, 0x00, 0x00, 0x41, 0x00, 0x0F, 0x0B,
    0x1A, 0x41, 0x01, 0x0B,
};

/* (module (type $f0 (func (result i32))) (type $c0 (cont $f0))
     (type $fi (func (param i32) (result i32))) (type $ci (cont $fi))
     (tag $outer (param i32) (result i32)) (tag $inner)
     (import "host" "stacks" (func $stacks (result i32)))
     (global $captured (export "captured") (mut i32) (i32.const 0))
     (func $leaf (result i32) (i32.add (suspend $outer (i32.const 10)) (suspend $outer (i32.const 20))))
     (func $middle (result i32)
       (block $h (result (ref $c0))
         (return (i32.add (i32.const 100) (resume $c0 (on $inner $h) (cont.new $c0 (ref.func $leaf))))))
       (drop) (i32.const -1))
     (elem declare func $leaf $middle)
     (func (export "run") (result i32) (local $k (ref null $ci)) (local $v i32) (local $count i32)
       (block $h (result i32 (ref $ci))
         (return (resume $c0 (on $outer $h) (cont.new $c0 (ref.func $middle)))))
       (local.set $k) (local.set $v) (global.set $captured (call $stacks))
       (loop $l
         (local.set $count (i32.add (local.get $count) (i32.const 1)))
         (block $h2 (result i32 (ref $ci))
           (return (i32.add (i32.mul (local.get $count) (i32.const 1000))
                            (resume $ci (on $outer $h2) (i32.mul (local.get $v) (i32.const 2)) (local.get $k)))))
         (local.set $k) (local.set $v) (br $l))
       (unreachable))) */
static const uint8_t wasm_test_cont_nested_sections[] = {
    0x01, 0x17, 0x06, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x02,
    0x60, 0x00, 0x00, 0x60, 0x00, 0x02, 0x7F, 0x64, 0x03, 0x02, 0x0F, 0x01, 0x04, 0x68, 0x6F, 0x73,
    0x74, 0x06, 0x73, 0x74, 0x61, 0x63, 0x6B, 0x73, 0x00, 0x00, 0x03, 0x04, 0x03, 0x00, 0x00, 0x00,
    0x0D, 0x05, 0x02, 0x00, 0x02, 0x00, 0x04, 0x06, 0x06, 0x01, 0x7F, 0x01, 0x41, 0x00, 0x0B, 0x07,
    0x12, 0x02, 0x08, 0x63, 0x61, 0x70, 0x74, 0x75, 0x72, 0x65, 0x64, 0x03, 0x00, 0x03, 0x72, 0x75,
    0x6E, 0x00, 0x03, 0x09, 0x06, 0x01, 0x03, 0x00, 0x02, 0x01, 0x02, 0x0A, 0x6D, 0x03, 0x0B, 0x00,
    0x41, 0x0A, 0xE2, 0x00, 0x41, 0x14, 0xE2, 0x00, 0x6A, 0x0B, 0x18, 0x00, 0x02, 0x64, 0x01, 0x41,
    0xE4, 0x00, 0xD2, 0x01, 0xE0, 0x01, 0xE3, 0x01, 0x01, 0x00, 0x01, 0x00, 0x6A, 0x0F, 0x0B, 0x1A,
    0x41, 0x7F, 0x0B, 0x46, 0x02, 0x01, 0x63, 0x03, 0x02, 0x7F, 0x02, 0x05, 0xD2, 0x02, 0xE0, 0x01,
    0xE3, 0x01, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x0B, 0x21, 0x00, 0x21, 0x01, 0x10, 0x00, 0x24, 0x00,
    0x03, 0x40, 0x20, 0x02, 0x41, 0x01, 0x6A, 0x21, 0x02, 0x02, 0x05, 0x20, 0x02, 0x41, 0xE8, 0x07,
    0x6C, 0x20, 0x01, 0x41, 0x02, 0x6C, 0x20, 0x00, 0xE3, 0x03, 0x01, 0x00, 0x00, 0x00, 0x6A, 0x0F,
    0x0B, 0x21, 0x00, 0x21, 0x01, 0x0C, 0x00, 0x0B, 0x00, 0x0B,
};

/* (module (type $box (struct (field i32))) (type $f0 (func (result i32))) (type $c0 (cont $f0))
     (type $fb (func (param (ref $box)) (result i32))) (type $cb (cont $fb)) (tag $yield)
     (import "host" "collect" (func $collect))
     (global $k (mut (ref null $c0)) (ref.null $c0))
     (func $holder (result i32) (local $b (ref null $box))
       (local.set $b (struct.new $box (i32.const 40)))
       (struct.new $box (i32.const 2)) (suspend $yield) (call $collect)
       (struct.get $box 0) (struct.get $box 0 (local.get $b)) (i32.add))
     (func $bound (param $b (ref $box)) (result i32) (call $collect) (struct.get $box 0 (local.get $b)))
     (elem declare func $holder $bound)
     (func (export "start")
       (block $h (result (ref $c0)) (resume $c0 (on $yield $h) (cont.new $c0 (ref.func $holder))) (unreachable))
       (global.set $k))
     (func (export "garbage") (param $n i32)
       (loop $l (drop (struct.new $box (local.get $n)))
         (local.set $n (i32.sub (local.get $n) (i32.const 1))) (br_if $l (local.get $n))))
     (func (export "finish") (result i32) (resume $c0 (global.get $k)))
     (func (export "bound") (result i32) (local $k2 (ref null $c0))
       (local.set $k2 (cont.bind $cb $c0 (struct.new $box (i32.const 7)) (cont.new $cb (ref.func $bound))))
       (call $collect) (resume $c0 (local.get $k2)))) */
static const uint8_t wasm_test_cont_gcroots_sections[] = {
    0x01, 0x1A, 0x07, 0x5F, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x01, 0x60, 0x01, 0x64,
    0x00, 0x01, 0x7F, 0x5D, 0x03, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x02, 0x10, 0x01, 0x04,
    0x68, 0x6F, 0x73, 0x74, 0x07, 0x63, 0x6F, 0x6C, 0x6C, 0x65, 0x63, 0x74, 0x00, 0x05, 0x03, 0x07,
    0x06, 0x01, 0x03, 0x05, 0x06, 0x01, 0x01, 0x0D, 0x03, 0x01, 0x00, 0x05, 0x06, 0x07, 0x01, 0x63,
    0x02, 0x01, 0xD0, 0x02, 0x0B, 0x07, 0x24, 0x04, 0x05, 0x73, 0x74, 0x61, 0x72, 0x74, 0x00, 0x03,
    0x07, 0x67, 0x61, 0x72, 0x62, 0x61, 0x67, 0x65, 0x00, 0x04, 0x06, 0x66, 0x69, 0x6E, 0x69, 0x73,
    0x68, 0x00, 0x05, 0x05, 0x62, 0x6F, 0x75, 0x6E, 0x64, 0x00, 0x06, 0x09, 0x06, 0x01, 0x03, 0x00,
    0x02, 0x01, 0x02, 0x0A, 0x7B, 0x06, 0x20, 0x01, 0x01, 0x63, 0x00, 0x41, 0x28, 0xFB, 0x00, 0x00,
    0x21, 0x00, 0x41, 0x02, 0xFB, 0x00, 0x00, 0xE2, 0x00, 0x10, 0x00, 0xFB, 0x02, 0x00, 0x00, 0x20,
    0x00, 0xFB, 0x02, 0x00, 0x00, 0x6A, 0x0B, 0x0A, 0x00, 0x10, 0x00, 0x20, 0x00, 0xFB, 0x02, 0x00,
    0x00, 0x0B, 0x13, 0x00, 0x02, 0x64, 0x02, 0xD2, 0x01, 0xE0, 0x02, 0xE3, 0x02, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x0B, 0x24, 0x00, 0x0B, 0x16, 0x00, 0x03, 0x40, 0x20, 0x00, 0xFB, 0x00, 0x00, 0x1A,
    0x20, 0x00, 0x41, 0x01, 0x6B, 0x21, 0x00, 0x20, 0x00, 0x0D, 0x00, 0x0B, 0x0B, 0x07, 0x00, 0x23,
    0x00, 0xE3, 0x02, 0x00, 0x0B, 0x1A, 0x01, 0x01, 0x63, 0x02, 0x41, 0x07, 0xFB, 0x00, 0x00, 0xD2,
    0x02, 0xE0, 0x04, 0xE1, 0x04, 0x02, 0x21, 0x00, 0x10, 0x00, 0x20, 0x00, 0xE3, 0x02, 0x00, 0x0B,
};

/* (module (type $f0 (func)) (type $c0 (cont $f0)) (tag $yield) (table $held 8 (ref null $c0))
     (func $suspends (suspend $yield)) (elem declare func $suspends)
     (func $suspended (result (ref $c0))
       (block $h (result (ref $c0)) (resume $c0 (on $yield $h) (cont.new $c0 (ref.func $suspends))) (unreachable)))
     (func (export "churn") (param $n i32)
       (loop $l (drop (call $suspended))
         (local.set $n (i32.sub (local.get $n) (i32.const 1))) (br_if $l (local.get $n))))
     (func (export "hold") (param $i i32) (table.set $held (local.get $i) (call $suspended)))
     (func (export "release") (param $i i32) (table.set $held (local.get $i) (ref.null $c0)))) */
static const uint8_t wasm_test_cont_limits_sections[] = {
    0x01, 0x0F, 0x04, 0x60, 0x00, 0x00, 0x5D, 0x00, 0x60, 0x00, 0x01, 0x64, 0x01, 0x60, 0x01, 0x7F,
    0x00, 0x03, 0x06, 0x05, 0x00, 0x02, 0x03, 0x03, 0x03, 0x04, 0x05, 0x01, 0x63, 0x01, 0x00, 0x08,
    0x0D, 0x03, 0x01, 0x00, 0x00, 0x07, 0x1A, 0x03, 0x05, 0x63, 0x68, 0x75, 0x72, 0x6E, 0x00, 0x02,
    0x04, 0x68, 0x6F, 0x6C, 0x64, 0x00, 0x03, 0x07, 0x72, 0x65, 0x6C, 0x65, 0x61, 0x73, 0x65, 0x00,
    0x04, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x3E, 0x05, 0x04, 0x00, 0xE2, 0x00, 0x0B,
    0x11, 0x00, 0x02, 0x64, 0x01, 0xD2, 0x00, 0xE0, 0x01, 0xE3, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x0B, 0x0B, 0x13, 0x00, 0x03, 0x40, 0x10, 0x01, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x21, 0x00,
    0x20, 0x00, 0x0D, 0x00, 0x0B, 0x0B, 0x08, 0x00, 0x20, 0x00, 0x10, 0x01, 0x26, 0x00, 0x0B, 0x08,
    0x00, 0x20, 0x00, 0xD0, 0x01, 0x26, 0x00, 0x0B,
};

/* (module (type $f0 (func (result i32))) (type $c0 (cont $f0)) (import "host" "yield" (func $yield))
     (func $rec (param i32) (result i32)
       (if (result i32) (local.get 0)
         (then (i32.add (i32.const 1) (call $rec (i32.sub (local.get 0) (i32.const 1)))))
         (else (i32.const 0))))
     (func $deep (result i32) (call $rec (i32.const 100)))
     (func $shallow (result i32) (call $rec (i32.const 5)))
     (func $spin (result i32) (loop $l (br $l)) (unreachable))
     (func $yields (result i32) (call $yield) (i32.const 1))
     (elem declare func $deep $shallow $spin $yields)
     (func (export "deep") (result i32) (resume $c0 (cont.new $c0 (ref.func $deep))))
     (func (export "shallow") (result i32) (resume $c0 (cont.new $c0 (ref.func $shallow))))
     (func (export "spin") (result i32) (resume $c0 (cont.new $c0 (ref.func $spin))))
     (func (export "yields") (result i32) (resume $c0 (cont.new $c0 (ref.func $yields))))
     (func (export "main-deep") (result i32) (call $rec (i32.const 100)))) */
static const uint8_t wasm_test_cont_overflow_sections[] = {
    0x01, 0x0F, 0x04, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7F, 0x01,
    0x7F, 0x02, 0x0E, 0x01, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x05, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x00,
    0x02, 0x03, 0x0B, 0x0A, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x2E,
    0x05, 0x04, 0x64, 0x65, 0x65, 0x70, 0x00, 0x06, 0x07, 0x73, 0x68, 0x61, 0x6C, 0x6C, 0x6F, 0x77,
    0x00, 0x07, 0x04, 0x73, 0x70, 0x69, 0x6E, 0x00, 0x08, 0x06, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x73,
    0x00, 0x09, 0x09, 0x6D, 0x61, 0x69, 0x6E, 0x2D, 0x64, 0x65, 0x65, 0x70, 0x00, 0x0A, 0x09, 0x08,
    0x01, 0x03, 0x00, 0x04, 0x02, 0x03, 0x04, 0x05, 0x0A, 0x65, 0x0A, 0x14, 0x00, 0x20, 0x00, 0x04,
    0x7F, 0x41, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x10, 0x01, 0x6A, 0x05, 0x41, 0x00, 0x0B, 0x0B,
    0x07, 0x00, 0x41, 0xE4, 0x00, 0x10, 0x01, 0x0B, 0x06, 0x00, 0x41, 0x05, 0x10, 0x01, 0x0B, 0x08,
    0x00, 0x03, 0x40, 0x0C, 0x00, 0x0B, 0x00, 0x0B, 0x06, 0x00, 0x10, 0x00, 0x41, 0x01, 0x0B, 0x09,
    0x00, 0xD2, 0x02, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0B, 0x09, 0x00, 0xD2, 0x03, 0xE0, 0x01, 0xE3,
    0x01, 0x00, 0x0B, 0x09, 0x00, 0xD2, 0x04, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0B, 0x09, 0x00, 0xD2,
    0x05, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0B, 0x07, 0x00, 0x41, 0xE4, 0x00, 0x10, 0x01, 0x0B,
};

/* (module (type $f0 (func (result i32))) (type $c0 (cont $f0))
     (func $answer (result i32) (i32.const 42)) (elem declare func $answer)
     (func (export "make") (result (ref $c0)) (cont.new $c0 (ref.func $answer)))) */
static const uint8_t wasm_test_cont_freed_a_sections[] = {
    0x01, 0x0C, 0x03, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x00, 0x01, 0x64, 0x01, 0x03, 0x03,
    0x02, 0x00, 0x02, 0x07, 0x08, 0x01, 0x04, 0x6D, 0x61, 0x6B, 0x65, 0x00, 0x01, 0x09, 0x05, 0x01,
    0x03, 0x00, 0x01, 0x00, 0x0A, 0x0D, 0x02, 0x04, 0x00, 0x41, 0x2A, 0x0B, 0x06, 0x00, 0xD2, 0x00,
    0xE0, 0x01, 0x0B,
};

/* (module (type $f0 (func (result i32))) (type $c0 (cont $f0))
     (import "a" "make" (func $make (result (ref $c0))))
     (global $k (mut (ref null $c0)) (ref.null $c0))
     (func (export "store") (global.set $k (call $make)))
     (func (export "run") (result i32) (resume $c0 (global.get $k)))) */
static const uint8_t wasm_test_cont_freed_b_sections[] = {
    0x01, 0x0F, 0x04, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x00, 0x01, 0x64, 0x01, 0x60, 0x00,
    0x00, 0x02, 0x0A, 0x01, 0x01, 0x61, 0x04, 0x6D, 0x61, 0x6B, 0x65, 0x00, 0x02, 0x03, 0x03, 0x02,
    0x03, 0x00, 0x06, 0x07, 0x01, 0x63, 0x01, 0x01, 0xD0, 0x01, 0x0B, 0x07, 0x0F, 0x02, 0x05, 0x73,
    0x74, 0x6F, 0x72, 0x65, 0x00, 0x01, 0x03, 0x72, 0x75, 0x6E, 0x00, 0x02, 0x0A, 0x10, 0x02, 0x06,
    0x00, 0x10, 0x00, 0x24, 0x00, 0x0B, 0x07, 0x00, 0x23, 0x00, 0xE3, 0x01, 0x00, 0x0B,
};

/* (module (type $f0 (func (result i32))) (type $c0 (cont $f0)) (tag $e (param i32))
     (func $throws (result i32) (throw $e (i32.const 5)))
     (func $catches (result i32)
       (block $h (result i32) (try_table (catch $e $h) (drop (call $throws))) (i32.const -1)))
     (elem declare func $throws $catches)
     (func (export "caught") (result i32) (resume $c0 (cont.new $c0 (ref.func $catches))))
     (func (export "escapes") (result i32)
       (block $h (result i32)
         (try_table (catch $e $h) (drop (resume $c0 (cont.new $c0 (ref.func $throws)))))
         (i32.const -1)))) */
static const uint8_t wasm_test_cont_exceptions_sections[] = {
    0x01, 0x0B, 0x03, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x03, 0x05, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x0D, 0x03, 0x01, 0x00, 0x02, 0x07, 0x14, 0x02, 0x06, 0x63, 0x61, 0x75,
    0x67, 0x68, 0x74, 0x00, 0x02, 0x07, 0x65, 0x73, 0x63, 0x61, 0x70, 0x65, 0x73, 0x00, 0x03, 0x09,
    0x06, 0x01, 0x03, 0x00, 0x02, 0x00, 0x01, 0x0A, 0x3B, 0x04, 0x06, 0x00, 0x41, 0x05, 0x08, 0x00,
    0x0B, 0x11, 0x00, 0x02, 0x7F, 0x1F, 0x40, 0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x1A, 0x0B, 0x41,
    0x7F, 0x0B, 0x0B, 0x09, 0x00, 0xD2, 0x01, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0B, 0x16, 0x00, 0x02,
    0x7F, 0x1F, 0x40, 0x01, 0x00, 0x00, 0x00, 0xD2, 0x00, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x1A, 0x0B,
    0x41, 0x7F, 0x0B, 0x0B,
};

static void wasm_test_register_host(wasm_runtime_t* rt, const char* name, wasm_host_func_t func) {
    wasm_import_t imp;

    memset(&imp, 0, sizeof(imp));
    imp.module = "host";
    imp.name = name;
    imp.func = func;
    wasm_register_import(rt, &imp);
}

/* Calls export func_name of m with no arguments and checks it fails with err and a message containing text. */
static void wasm_test_check_cont_trap(test_ctx* t, wasm_module_t* m, const char* func_name, wasm_error_t err,
                                      const char* text) {
    wasm_value_t results[1];
    wasm_error_t actual = wasm_call(m, func_name, NULL, 0, results, 1);

    CHECK_MSG(t, actual == err, "%s: expected error %d, got %d", func_name, (int)err, (int)actual);
    CHECK_MSG(t, strstr(m->rt->error_msg, text) != NULL, "%s: %s", func_name, m->rt->error_msg);
    CHECK_MSG(t, m->rt->exec == &m->rt->main_stack, "%s: execution left on a continuation stack", func_name);
}

TEST(test_stack_switching_generator) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    uint32_t i;

    wasm_init(&rt);
    m = wasm_test_load_cont(&rt, wasm_test_cont_generator_sections, sizeof(wasm_test_cont_generator_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < 3; i++) {
        arg = wasm_i32(10);
        WASM_CHECK_OK(t, wasm_call(m, "sum", &arg, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 55);
    }
    arg = wasm_i32(1000);
    WASM_CHECK_OK(t, wasm_call(m, "sum", &arg, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 500500);
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected finished generators to release their stacks, %u live",
              (unsigned)rt.num_cont_stacks);
    CHECK_MSG(t, rt.num_pooled_cont_stacks == 1, "expected one pooled stack, got %u",
              (unsigned)rt.num_pooled_cont_stacks);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_one_shot) {
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_test_load_cont(&rt, wasm_test_cont_oneshot_sections, sizeof(wasm_test_cont_oneshot_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_check_cont_trap(t, m, "resume-twice", WASM_ERR_TRAP, "continuation already consumed");
    wasm_test_check_cont_trap(t, m, "bind-twice", WASM_ERR_TRAP, "continuation already consumed");
    wasm_test_check_cont_trap(t, m, "resume-after-bind", WASM_ERR_TRAP, "continuation already consumed");
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected no live continuation stacks, got %u",
              (unsigned)rt.num_cont_stacks);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_null_continuations) {
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_test_load_cont(&rt, wasm_test_cont_oneshot_sections, sizeof(wasm_test_cont_oneshot_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_check_cont_trap(t, m, "null-resume", WASM_ERR_TRAP, "null continuation reference");
    wasm_test_check_cont_trap(t, m, "null-bind", WASM_ERR_TRAP, "null continuation reference");
    wasm_test_check_cont_trap(t, m, "null-new", WASM_ERR_TRAP, "null function reference");
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_unhandled_suspend) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    uint32_t func_idx = UINT32_MAX;
    uint32_t offset = 0;

    wasm_init(&rt);
    m = wasm_test_load_cont(&rt, wasm_test_cont_unhandled_sections, sizeof(wasm_test_cont_unhandled_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_check_cont_trap(t, m, "top", WASM_ERR_TRAP, "unhandled tag");
    wasm_test_check_cont_trap(t, m, "inner", WASM_ERR_TRAP, "unhandled tag");
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected the trap to release the running stacks, %u live",
              (unsigned)rt.num_cont_stacks);
    WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &offset));
    CHECK_MSG(t, func_idx == 0 && m->funcs[0].code[offset] == 0xE2,
              "expected the trap at the suspend in function 0, got function %u offset %u", (unsigned)func_idx,
              (unsigned)offset);
    WASM_CHECK_OK(t, wasm_call(m, "handled", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 1);
    CHECK(t, rt.num_cont_stacks == 1);
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected the dropped continuation to be collected, %u live",
              (unsigned)rt.num_cont_stacks);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_cont_call_back(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)results;
    (void)num_results;
    (void)userdata;
    return wasm_call(mod, "nested", args, num_args, NULL, 0);
}

TEST(test_stack_switching_host_barrier) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;

    wasm_init(&rt);
    wasm_test_register_host(&rt, "call_back", wasm_test_cont_call_back);
    m = wasm_test_load_cont(&rt, wasm_test_cont_barrier_sections, sizeof(wasm_test_cont_barrier_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_check_cont_trap(t, m, "across", WASM_ERR_TRAP, "unhandled tag");
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected the trap to release the running stacks, %u live",
              (unsigned)rt.num_cont_stacks);
    WASM_CHECK_OK(t, wasm_call(m, "inside", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 0);
    CHECK(t, rt.exec == &rt.main_stack);
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected no live continuation stacks, got %u",
              (unsigned)rt.num_cont_stacks);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static uint32_t wasm_test_cont_captured_stacks;

static wasm_error_t wasm_test_cont_stacks(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                          wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)num_results;
    (void)userdata;
    wasm_test_cont_captured_stacks = mod->rt->num_cont_stacks;
    results[0] = wasm_i32((int32_t)mod->rt->num_cont_stacks);
    return WASM_OK;
}

TEST(test_stack_switching_nested_capture) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;

    wasm_init(&rt);
    wasm_test_register_host(&rt, "stacks", wasm_test_cont_stacks);
    m = wasm_test_load_cont(&rt, wasm_test_cont_nested_sections, sizeof(wasm_test_cont_nested_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_cont_captured_stacks = 0;
    WASM_CHECK_OK(t, wasm_call(m, "run", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 2160);
    CHECK_MSG(t, wasm_test_cont_captured_stacks == 2, "expected the suspension to capture 2 stacks, got %u",
              (unsigned)wasm_test_cont_captured_stacks);
    CHECK(t, rt.num_cont_stacks == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static uint32_t wasm_test_cont_collections;

static wasm_error_t wasm_test_cont_collect(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_gc_collect(mod->rt);
    wasm_test_cont_collections++;
    return WASM_OK;
}

TEST(test_stack_switching_gc_roots) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    uint8_t* heap;

    wasm_config_default(&cfg);
    cfg.initial_gc_heap_size = 4096;
    wasm_init_config(&rt, &cfg);
    wasm_test_register_host(&rt, "collect", wasm_test_cont_collect);
    m = wasm_test_load_cont(&rt, wasm_test_cont_gcroots_sections, sizeof(wasm_test_cont_gcroots_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_cont_collections = 0;
    WASM_CHECK_OK(t, wasm_call(m, "start", NULL, 0, NULL, 0));
    arg = wasm_i32(1000);
    WASM_CHECK_OK(t, wasm_call(m, "garbage", &arg, 1, NULL, 0));
    heap = rt.gc_heap;
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.gc_heap != heap, "%s", "expected the collection to move the live objects");
    WASM_CHECK_OK(t, wasm_call(m, "garbage", &arg, 1, NULL, 0));
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_call(m, "finish", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_OK(t, wasm_call(m, "bound", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 7);
    CHECK_MSG(t, wasm_test_cont_collections == 3, "expected 3 collections inside continuations, got %u",
              (unsigned)wasm_test_cont_collections);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_stack_limit) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_error_t err;
    int32_t i;

    wasm_config_default(&cfg);
    cfg.max_cont_stacks = 4;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_cont(&rt, wasm_test_cont_limits_sections, sizeof(wasm_test_cont_limits_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    arg = wasm_i32(100);
    WASM_CHECK_OK(t, wasm_call(m, "churn", &arg, 1, NULL, 0));
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected dropped continuations to release their stacks, %u live",
              (unsigned)rt.num_cont_stacks);
    for (i = 0; i < 4; i++) {
        arg = wasm_i32(i);
        WASM_CHECK_OK(t, wasm_call(m, "hold", &arg, 1, NULL, 0));
    }
    arg = wasm_i32(4);
    err = wasm_call(m, "hold", &arg, 1, NULL, 0);
    CHECK_MSG(t, err == WASM_ERR_OOM, "expected WASM_ERR_OOM, got %d", (int)err);
    CHECK_MSG(t, strstr(rt.error_msg, "continuation stack limit 4") != NULL, "%s", rt.error_msg);
    arg = wasm_i32(0);
    WASM_CHECK_OK(t, wasm_call(m, "release", &arg, 1, NULL, 0));
    arg = wasm_i32(4);
    WASM_CHECK_OK(t, wasm_call(m, "hold", &arg, 1, NULL, 0));
    CHECK(t, rt.num_cont_stacks == 4);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module (type $s (struct (field i32)))
     (type $f2 (func (param (ref null $s) i32) (result i32))) (type $c2 (cont $f2))
     (type $f1 (func (param i32) (result i32))) (type $c1 (cont $f1))
     (type $f0 (func (result i32))) (type $c0 (cont $f0)) (type $arr (array (mut (ref null $c0))))
     (tag $e (param i32)) (tag $y) (global $held (mut (ref null $arr)) (ref.null $arr))
     (func $add (type $f2) (i32.add (struct.get $s 0 (local.get 0)) (local.get 1)))
     (func $yields (result i32) (suspend $y) (i32.const 1)) (elem declare func $add $yields)
     (func (export "make") (param $n i32) (local $i i32)
       (global.set $held (array.new_default $arr (local.get $n)))
       (loop $l
         (array.set $arr (global.get $held) (local.get $i)
           (cont.bind $c1 $c0 (local.get $i)
             (cont.bind $c2 $c1 (struct.new $s (i32.const 1000)) (cont.new $c2 (ref.func $add)))))
         (local.set $i (i32.add (local.get $i) (i32.const 1))) (br_if $l (i32.lt_u (local.get $i) (local.get $n)))))
     (func (export "run") (param $i i32) (result i32) (resume $c0 (array.get $arr (global.get $held) (local.get $i))))
     (func (export "throw") (param $i i32) (result i32)
       (block $h (result i32)
         (try_table (result i32) (catch $e $h)
           (resume_throw $c0 $e (i32.const 7) (array.get $arr (global.get $held) (local.get $i))))))
     (func (export "churn") (param $n i32)
       (loop $l
         (block $h (result (ref $c0)) (drop (resume $c0 (on $y $h) (cont.new $c0 (ref.func $yields)))) (br $l))
         (drop) (br_if $l (local.tee $n (i32.sub (local.get $n) (i32.const 1))))))) */
static const uint8_t wasm_test_cont_lazy_sections[] = {
    0x01, 0x26, 0x0A, 0x5F, 0x01, 0x7F, 0x00, 0x60, 0x02, 0x63, 0x00, 0x7F, 0x01, 0x7F, 0x5D, 0x01,
    0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x03, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x05, 0x5E, 0x63, 0x06,
    0x01, 0x60, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x00, 0x03, 0x07, 0x06, 0x01, 0x05, 0x08, 0x03, 0x03,
    0x08, 0x0D, 0x05, 0x02, 0x00, 0x08, 0x00, 0x09, 0x06, 0x07, 0x01, 0x63, 0x07, 0x01, 0xD0, 0x07,
    0x0B, 0x07, 0x1E, 0x04, 0x04, 0x6D, 0x61, 0x6B, 0x65, 0x00, 0x02, 0x03, 0x72, 0x75, 0x6E, 0x00,
    0x03, 0x05, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x00, 0x04, 0x05, 0x63, 0x68, 0x75, 0x72, 0x6E, 0x00,
    0x05, 0x09, 0x06, 0x01, 0x03, 0x00, 0x02, 0x00, 0x01, 0x0A, 0x92, 0x01, 0x06, 0x0B, 0x00, 0x20,
    0x00, 0xFB, 0x02, 0x00, 0x00, 0x20, 0x01, 0x6A, 0x0B, 0x06, 0x00, 0xE2, 0x01, 0x41, 0x01, 0x0B,
    0x35, 0x01, 0x01, 0x7F, 0x20, 0x00, 0xFB, 0x07, 0x07, 0x24, 0x00, 0x03, 0x40, 0x23, 0x00, 0x20,
    0x01, 0x20, 0x01, 0x41, 0xE8, 0x07, 0xFB, 0x00, 0x00, 0xD2, 0x00, 0xE0, 0x02, 0xE1, 0x02, 0x04,
    0xE1, 0x04, 0x06, 0xFB, 0x0E, 0x07, 0x20, 0x01, 0x41, 0x01, 0x6A, 0x21, 0x01, 0x20, 0x01, 0x20,
    0x00, 0x49, 0x0D, 0x00, 0x0B, 0x0B, 0x0C, 0x00, 0x23, 0x00, 0x20, 0x00, 0xFB, 0x0B, 0x07, 0xE3,
    0x06, 0x00, 0x0B, 0x19, 0x00, 0x02, 0x7F, 0x1F, 0x7F, 0x01, 0x00, 0x00, 0x00, 0x41, 0x07, 0x23,
    0x00, 0x20, 0x00, 0xFB, 0x0B, 0x07, 0xE4, 0x06, 0x00, 0x00, 0x0B, 0x0B, 0x0B, 0x20, 0x00, 0x03,
    0x40, 0x02, 0x64, 0x06, 0xD2, 0x01, 0xE0, 0x06, 0xE3, 0x06, 0x01, 0x00, 0x01, 0x00, 0x1A, 0x0C,
    0x01, 0x0B, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x22, 0x00, 0x0D, 0x00, 0x0B, 0x0B,
};

TEST(test_stack_switching_stacks_are_allocated_on_first_run) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    uint32_t pooled;

    wasm_config_default(&cfg);
    cfg.cont_stack_values = 65536;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_cont(&rt, wasm_test_cont_lazy_sections, sizeof(wasm_test_cont_lazy_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    arg = wasm_i32(1000);
    WASM_CHECK_OK(t, wasm_call(m, "make", &arg, 1, NULL, 0));
    CHECK_MSG(t, rt.num_cont_stacks == 0, "%u continuation stacks", (unsigned)rt.num_cont_stacks);
    wasm_gc_collect(&rt);
    arg = wasm_i32(5);
    WASM_CHECK_OK(t, wasm_call(m, "run", &arg, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 1005);
    CHECK(t, wasm_call(m, "run", &arg, 1, &result, 1) == WASM_ERR_TRAP);
    CHECK_MSG(t, strstr(rt.error_msg, "continuation already consumed") != NULL, "%s", rt.error_msg);
    pooled = rt.num_pooled_cont_stacks;
    arg = wasm_i32(6);
    WASM_CHECK_OK(t, wasm_call(m, "throw", &arg, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 7);
    CHECK(t, rt.num_cont_stacks == 0 && rt.num_pooled_cont_stacks == pooled);
    arg = wasm_i32(100);
    WASM_CHECK_OK(t, wasm_call(m, "churn", &arg, 1, NULL, 0));
    CHECK_MSG(t, rt.num_cont_stacks <= 16, "%u continuation stacks of dropped continuations",
              (unsigned)rt.num_cont_stacks);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_cont_yield(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                         wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    return WASM_YIELD;
}

TEST(test_stack_switching_stack_overflow_and_stops) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    wasm_config_default(&cfg);
    cfg.cont_call_depth = 16;
    wasm_init_config(&rt, &cfg);
    wasm_test_register_host(&rt, "yield", wasm_test_cont_yield);
    m = wasm_test_load_cont(&rt, wasm_test_cont_overflow_sections, sizeof(wasm_test_cont_overflow_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_check_cont_trap(t, m, "deep", WASM_ERR_CALL_STACK_EXHAUSTED, "call frame depth exceeded limit 16");
    WASM_CHECK_OK(t, wasm_call(m, "shallow", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 5);
    WASM_CHECK_OK(t, wasm_call(m, "main-deep", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 100);

    wasm_runtime_set_fuel(&rt, 10000);
    err = wasm_call(m, "spin", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_FUEL, "expected WASM_ERR_OUT_OF_FUEL, got %d", (int)err);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_OUT_OF_FUEL);
    CHECK(t, rt.exec != &rt.main_stack);
    wasm_runtime_cancel(&rt);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    CHECK(t, rt.exec == &rt.main_stack);
    wasm_runtime_disable_fuel(&rt);
    err = wasm_call(m, "yields", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_YIELD, "expected WASM_YIELD, got %d", (int)err);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_YIELD);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 1);
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected stopped continuations to release their stacks, %u live",
              (unsigned)rt.num_cont_stacks);
    err = wasm_call(m, "shallow", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_exceptions) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;

    wasm_init(&rt);
    m = wasm_test_load_cont(&rt, wasm_test_cont_exceptions_sections, sizeof(wasm_test_cont_exceptions_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(m, "caught", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 5);
    WASM_CHECK_OK(t, wasm_call(m, "escapes", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 5);
    CHECK(t, rt.num_cont_stacks == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_freed_module) {
    wasm_runtime_t rt;
    wasm_module_t* a;
    wasm_module_t* b;

    wasm_init(&rt);
    a = wasm_test_load_cont(&rt, wasm_test_cont_freed_a_sections, sizeof(wasm_test_cont_freed_a_sections));
    CHECK_MSG(t, a != NULL, "%s", rt.error_msg);
    if (a == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "a", a));
    b = wasm_test_load_cont(&rt, wasm_test_cont_freed_b_sections, sizeof(wasm_test_cont_freed_b_sections));
    CHECK_MSG(t, b != NULL, "%s", rt.error_msg);
    if (b == NULL) {
        wasm_module_free(a);
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(b, "store", NULL, 0, NULL, 0));
    CHECK(t, rt.num_cont_stacks == 0);
    wasm_module_free(a);
    wasm_test_check_cont_trap(t, b, "run", WASM_ERR_TRAP, "freed module");
    wasm_module_free(b);
    wasm_destroy(&rt);
}

/* (module
     (rec (type $fs (func (param i32 (ref null $cs)) (result i32))) (type $cs (cont $fs)))
     (type $fb (func (param i32 i32 (ref null $cs)) (result i32))) (type $cb (cont $fb))
     (tag $sw (result i32)) (tag $other (result i32))
     (global $trace (export "trace") (mut i32) (i32.const 0))
     (func $co (type $fs) (param $n i32) (param $k (ref null $cs)) (result i32)
       (loop $l
         (global.set $trace (i32.add (i32.mul (global.get $trace) (i32.const 10)) (local.get $n)))
         (if (i32.ge_u (local.get $n) (i32.const 6)) (then (return (local.get $n))))
         (switch $cs $sw (i32.add (local.get $n) (i32.const 1)) (local.get $k))
         (local.set $k) (local.set $n) (br $l))
       (unreachable))
     (func $adder (param $base i32) (param $n i32) (param $k (ref null $cs)) (result i32)
       (i32.add (i32.mul (local.get $base) (i32.const 100)) (local.get $n)))
     (func $switcher (type $fs) (param $n i32) (param $k (ref null $cs)) (result i32)
       (switch $cs $sw (i32.add (local.get $n) (i32.const 1)) (local.get $k))
       (drop) (drop) (i32.const -1))
     (elem declare func $co $adder $switcher)
     (func (export "pingpong") (result i32)
       (resume $cs (on $sw switch) (i32.const 1) (cont.new $cs (ref.func $co)) (cont.new $cs (ref.func $co))))
     (func (export "to-bound") (result i32)
       (resume $cs (on $sw switch) (i32.const 7)
         (cont.bind $cb $cs (i32.const 5) (cont.new $cb (ref.func $adder)))
         (cont.new $cs (ref.func $switcher))))
     (func (export "no-handler") (result i32)
       (resume $cs (on $other switch) (i32.const 1) (cont.new $cs (ref.func $co)) (cont.new $cs (ref.func $switcher))))
     (func (export "top-level") (result i32)
       (switch $cs $sw (i32.const 1) (cont.new $cs (ref.func $co))) (drop) (drop) (i32.const -1))) */
static const uint8_t wasm_test_cont_switch_sections[] = {
    0x01, 0x1A, 0x04, 0x4E, 0x02, 0x60, 0x02, 0x7F, 0x63, 0x01, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x03,
    0x7F, 0x7F, 0x63, 0x01, 0x01, 0x7F, 0x5D, 0x02, 0x60, 0x00, 0x01, 0x7F, 0x03, 0x08, 0x07, 0x00,
    0x02, 0x00, 0x04, 0x04, 0x04, 0x04, 0x0D, 0x05, 0x02, 0x00, 0x04, 0x00, 0x04, 0x06, 0x06, 0x01,
    0x7F, 0x01, 0x41, 0x00, 0x0B, 0x07, 0x38, 0x05, 0x05, 0x74, 0x72, 0x61, 0x63, 0x65, 0x03, 0x00,
    0x08, 0x70, 0x69, 0x6E, 0x67, 0x70, 0x6F, 0x6E, 0x67, 0x00, 0x03, 0x08, 0x74, 0x6F, 0x2D, 0x62,
    0x6F, 0x75, 0x6E, 0x64, 0x00, 0x04, 0x0A, 0x6E, 0x6F, 0x2D, 0x68, 0x61, 0x6E, 0x64, 0x6C, 0x65,
    0x72, 0x00, 0x05, 0x09, 0x74, 0x6F, 0x70, 0x2D, 0x6C, 0x65, 0x76, 0x65, 0x6C, 0x00, 0x06, 0x09,
    0x07, 0x01, 0x03, 0x00, 0x03, 0x00, 0x01, 0x02, 0x0A, 0x95, 0x01, 0x07, 0x2B, 0x00, 0x03, 0x40,
    0x23, 0x00, 0x41, 0x0A, 0x6C, 0x20, 0x00, 0x6A, 0x24, 0x00, 0x20, 0x00, 0x41, 0x06, 0x4F, 0x04,
    0x40, 0x20, 0x00, 0x0F, 0x0B, 0x20, 0x00, 0x41, 0x01, 0x6A, 0x20, 0x01, 0xE6, 0x01, 0x00, 0x21,
    0x01, 0x21, 0x00, 0x0C, 0x00, 0x0B, 0x00, 0x0B, 0x0B, 0x00, 0x20, 0x00, 0x41, 0xE4, 0x00, 0x6C,
    0x20, 0x01, 0x6A, 0x0B, 0x10, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6A, 0x20, 0x01, 0xE6, 0x01, 0x00,
    0x1A, 0x1A, 0x41, 0x7F, 0x0B, 0x11, 0x00, 0x41, 0x01, 0xD2, 0x00, 0xE0, 0x01, 0xD2, 0x00, 0xE0,
    0x01, 0xE3, 0x01, 0x01, 0x01, 0x00, 0x0B, 0x16, 0x00, 0x41, 0x07, 0x41, 0x05, 0xD2, 0x01, 0xE0,
    0x03, 0xE1, 0x03, 0x01, 0xD2, 0x02, 0xE0, 0x01, 0xE3, 0x01, 0x01, 0x01, 0x00, 0x0B, 0x11, 0x00,
    0x41, 0x01, 0xD2, 0x00, 0xE0, 0x01, 0xD2, 0x02, 0xE0, 0x01, 0xE3, 0x01, 0x01, 0x01, 0x01, 0x0B,
    0x0F, 0x00, 0x41, 0x01, 0xD2, 0x00, 0xE0, 0x01, 0xE6, 0x01, 0x00, 0x1A, 0x1A, 0x41, 0x7F, 0x0B,
};

/* (module
     (type $f (func (result i32))) (type $c (cont $f))
     (tag $yield) (tag $e (param i32)) (tag $other)
     (func $catcher (result i32)
       (block $h (result i32)
         (try_table (catch $e $h) (suspend $yield))
         (return (i32.const -1)))
       (i32.add (i32.const 100)))
     (func $noncatcher (result i32)
       (block $h (try_table (catch $other $h) (suspend $yield)))
       (i32.const -1))
     (func $thrower (result i32) (throw $e (i32.const 11)))
     (func $mid (result i32)
       (block $h (try_table (catch $other $h) (return (resume $c (cont.new $c (ref.func $thrower))))))
       (i32.const -1))
     (func $outer (result i32) (i32.add (i32.const 1) (resume $c (cont.new $c (ref.func $mid)))))
     (func $rethrower (result i32)
       try (result i32) (throw $e (i32.const 1)) catch $e drop (throw $e (i32.const 12)) end)
     (func $catch_mid (result i32)
       (block $h (result i32)
         (try_table (catch $e $h) (return (resume $c (cont.new $c (ref.func $outer)))))
         (i32.const -1))
       (i32.add (i32.const 500)))
     (elem declare func $catcher $noncatcher $thrower $mid $outer $rethrower $catch_mid)
     (func $suspended (param $fn (ref $f)) (result (ref $c))
       (block $h (result (ref $c))
         (drop (resume $c (on $yield $h) (cont.new $c (local.get $fn))))
         (unreachable)))
     (func (export "caught-inside") (result i32)
       (resume_throw $c $e (i32.const 5) (call $suspended (ref.func $catcher))))
     (func (export "to-resumer") (result i32)
       (block $h (result i32)
         (try_table (catch $e $h)
           (return (resume_throw $c $e (i32.const 7) (call $suspended (ref.func $noncatcher)))))
         (unreachable))
       (i32.add (i32.const 200)))
     (func (export "unstarted") (result i32)
       (block $h (result i32)
         (try_table (catch $e $h) (return (resume_throw $c $e (i32.const 9) (cont.new $c (ref.func $catcher)))))
         (unreachable))
       (i32.add (i32.const 300)))
     (func (export "throw-ref") (result i32) (local $x exnref)
       (block $h (result exnref)
         (try_table (catch_all_ref $h) (throw $e (i32.const 3)))
         (unreachable))
       (local.set $x)
       (resume_throw_ref $c (local.get $x) (call $suspended (ref.func $catcher))))
     (func (export "null-exn") (result i32)
       (resume_throw_ref $c (ref.null exn) (call $suspended (ref.func $catcher))))
     (func (export "uncaught") (result i32)
       (resume_throw $c $e (i32.const 1) (call $suspended (ref.func $noncatcher))))
     (func (export "consumed") (result i32) (local $k (ref null $c))
       (local.set $k (call $suspended (ref.func $catcher)))
       (drop (resume_throw $c $e (i32.const 1) (local.get $k)))
       (resume_throw $c $e (i32.const 1) (local.get $k)))
     (func (export "three-stacks") (result i32)
       (block $h (result i32)
         (try_table (catch $e $h) (return (resume $c (cont.new $c (ref.func $outer)))))
         (unreachable))
       (i32.add (i32.const 400)))
     (func (export "caught-in-middle") (result i32) (resume $c (cont.new $c (ref.func $catch_mid))))
     (func (export "rethrown") (result i32)
       (block $h (result i32)
         (try_table (catch $e $h) (return (resume $c (cont.new $c (ref.func $rethrower)))))
         (unreachable))
       (i32.add (i32.const 600)))) */
static const uint8_t wasm_test_cont_throw_sections[] = {
    0x01, 0x15, 0x05, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7F, 0x00,
    0x60, 0x01, 0x64, 0x00, 0x01, 0x64, 0x01, 0x03, 0x13, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x07, 0x03, 0x00,
    0x02, 0x00, 0x03, 0x00, 0x02, 0x07, 0x84, 0x01, 0x0A, 0x0D, 0x63, 0x61, 0x75, 0x67, 0x68, 0x74,
    0x2D, 0x69, 0x6E, 0x73, 0x69, 0x64, 0x65, 0x00, 0x08, 0x0A, 0x74, 0x6F, 0x2D, 0x72, 0x65, 0x73,
    0x75, 0x6D, 0x65, 0x72, 0x00, 0x09, 0x09, 0x75, 0x6E, 0x73, 0x74, 0x61, 0x72, 0x74, 0x65, 0x64,
    0x00, 0x0A, 0x09, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x2D, 0x72, 0x65, 0x66, 0x00, 0x0B, 0x08, 0x6E,
    0x75, 0x6C, 0x6C, 0x2D, 0x65, 0x78, 0x6E, 0x00, 0x0C, 0x08, 0x75, 0x6E, 0x63, 0x61, 0x75, 0x67,
    0x68, 0x74, 0x00, 0x0D, 0x08, 0x63, 0x6F, 0x6E, 0x73, 0x75, 0x6D, 0x65, 0x64, 0x00, 0x0E, 0x0C,
    0x74, 0x68, 0x72, 0x65, 0x65, 0x2D, 0x73, 0x74, 0x61, 0x63, 0x6B, 0x73, 0x00, 0x0F, 0x10, 0x63,
    0x61, 0x75, 0x67, 0x68, 0x74, 0x2D, 0x69, 0x6E, 0x2D, 0x6D, 0x69, 0x64, 0x64, 0x6C, 0x65, 0x00,
    0x10, 0x08, 0x72, 0x65, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x6E, 0x00, 0x11, 0x09, 0x0B, 0x01, 0x03,
    0x00, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x0A, 0xEB, 0x02, 0x12, 0x15, 0x00, 0x02,
    0x7F, 0x1F, 0x40, 0x01, 0x00, 0x01, 0x00, 0xE2, 0x00, 0x0B, 0x41, 0x7F, 0x0F, 0x0B, 0x41, 0xE4,
    0x00, 0x6A, 0x0B, 0x10, 0x00, 0x02, 0x40, 0x1F, 0x40, 0x01, 0x00, 0x02, 0x00, 0xE2, 0x00, 0x0B,
    0x0B, 0x41, 0x7F, 0x0B, 0x06, 0x00, 0x41, 0x0B, 0x08, 0x01, 0x0B, 0x16, 0x00, 0x02, 0x40, 0x1F,
    0x40, 0x01, 0x00, 0x02, 0x00, 0xD2, 0x02, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0F, 0x0B, 0x0B, 0x41,
    0x7F, 0x0B, 0x0C, 0x00, 0x41, 0x01, 0xD2, 0x03, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x6A, 0x0B, 0x10,
    0x00, 0x06, 0x7F, 0x41, 0x01, 0x08, 0x01, 0x07, 0x01, 0x1A, 0x41, 0x0C, 0x08, 0x01, 0x0B, 0x0B,
    0x1A, 0x00, 0x02, 0x7F, 0x1F, 0x40, 0x01, 0x00, 0x01, 0x00, 0xD2, 0x04, 0xE0, 0x01, 0xE3, 0x01,
    0x00, 0x0F, 0x0B, 0x41, 0x7F, 0x0B, 0x41, 0xF4, 0x03, 0x6A, 0x0B, 0x12, 0x00, 0x02, 0x64, 0x01,
    0x20, 0x00, 0xE0, 0x01, 0xE3, 0x01, 0x01, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x0B, 0x0B, 0x0C, 0x00,
    0x41, 0x05, 0xD2, 0x00, 0x10, 0x07, 0xE4, 0x01, 0x01, 0x00, 0x0B, 0x1C, 0x00, 0x02, 0x7F, 0x1F,
    0x40, 0x01, 0x00, 0x01, 0x00, 0x41, 0x07, 0xD2, 0x01, 0x10, 0x07, 0xE4, 0x01, 0x01, 0x00, 0x0F,
    0x0B, 0x00, 0x0B, 0x41, 0xC8, 0x01, 0x6A, 0x0B, 0x1C, 0x00, 0x02, 0x7F, 0x1F, 0x40, 0x01, 0x00,
    0x01, 0x00, 0x41, 0x09, 0xD2, 0x00, 0xE0, 0x01, 0xE4, 0x01, 0x01, 0x00, 0x0F, 0x0B, 0x00, 0x0B,
    0x41, 0xAC, 0x02, 0x6A, 0x0B, 0x1D, 0x01, 0x01, 0x69, 0x02, 0x69, 0x1F, 0x40, 0x01, 0x03, 0x00,
    0x41, 0x03, 0x08, 0x01, 0x0B, 0x00, 0x0B, 0x21, 0x00, 0x20, 0x00, 0xD2, 0x00, 0x10, 0x07, 0xE5,
    0x01, 0x00, 0x0B, 0x0B, 0x00, 0xD0, 0x69, 0xD2, 0x00, 0x10, 0x07, 0xE5, 0x01, 0x00, 0x0B, 0x0C,
    0x00, 0x41, 0x01, 0xD2, 0x01, 0x10, 0x07, 0xE4, 0x01, 0x01, 0x00, 0x0B, 0x1C, 0x01, 0x01, 0x63,
    0x01, 0xD2, 0x00, 0x10, 0x07, 0x21, 0x00, 0x41, 0x01, 0x20, 0x00, 0xE4, 0x01, 0x01, 0x00, 0x1A,
    0x41, 0x01, 0x20, 0x00, 0xE4, 0x01, 0x01, 0x00, 0x0B, 0x19, 0x00, 0x02, 0x7F, 0x1F, 0x40, 0x01,
    0x00, 0x01, 0x00, 0xD2, 0x04, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0F, 0x0B, 0x00, 0x0B, 0x41, 0x90,
    0x03, 0x6A, 0x0B, 0x09, 0x00, 0xD2, 0x06, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0B, 0x19, 0x00, 0x02,
    0x7F, 0x1F, 0x40, 0x01, 0x00, 0x01, 0x00, 0xD2, 0x05, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0F, 0x0B,
    0x00, 0x0B, 0x41, 0xD8, 0x04, 0x6A, 0x0B,
};

/* (module
     (type $box (struct (field i32)))
     (type $f (func (result i32))) (type $c (cont $f))
     (type $fi (func (param i32) (result i32))) (type $ci (cont $fi))
     (tag $t (param i32) (result i32)) (tag $e (param i32))
     (import "host" "yield" (func $yield (param i32) (result i32)))
     (import "host" "interrupt" (func $interrupt))
     (import "host" "call_throws" (func $call_throws))
     (global $inner (mut (ref null $f)) (ref.null $f))
     (func $work (param $n i32) (result i32) (local $acc i32)
       (loop $l
         (local.set $acc (i32.add (local.get $acc) (i32.mul (local.get $n) (local.get $n))))
         (local.set $n (i32.sub (local.get $n) (i32.const 1)))
         (br_if $l (local.get $n)))
       (local.get $acc))
     (func $inner_work (result i32) (i32.add (call $work (i32.const 300)) (suspend $t (i32.const 5))))
     (func $inner_yield (result i32) (local $b (ref null $box)) (local $v i32)
       (local.set $b (struct.new $box (i32.const 40)))
       (drop (call $garbage (i32.const 2000)))
       (struct.new $box (i32.const 2))
       (local.set $v (call $yield (i32.const 7)))
       (drop (call $garbage (i32.const 10)))
       (struct.get $box 0)
       (i32.add (struct.get $box 0 (local.get $b)))
       (i32.add (local.get $v))
       (suspend $t))
     (func $inner_interrupt (result i32) (call $interrupt) (call $work (i32.const 50)))
     (func $inner_trap (result i32) (unreachable))
     (func $inner_host_throw (result i32) (call $call_throws) (i32.const -1))
     (func $garbage (param $n i32) (result i32)
       (loop $l (drop (struct.new $box (local.get $n)))
         (local.set $n (i32.sub (local.get $n) (i32.const 1))) (br_if $l (local.get $n)))
       (i32.const 0))
     (func $middle (result i32) (local $k (ref null $ci)) (local $v i32) (local $b (ref null $box))
       (local.set $b (struct.new $box (i32.const 2000)))
       (block $h (result i32 (ref $ci))
         (return (i32.add (i32.const 1000) (resume $c (on $t $h) (cont.new $c (ref.as_non_null (global.get $inner)))))))
       (local.set $k) (local.set $v)
       (i32.add (struct.get $box 0 (local.get $b)) (resume $ci (i32.mul (local.get $v) (i32.const 2)) (local.get $k))))
     (func (export "throws") (throw $e (i32.const 1)))
     (elem declare func $inner_work $inner_yield $inner_interrupt $inner_trap $inner_host_throw $middle)
     (func $run (param $inner (ref $f)) (result i32)
       (global.set $inner (local.get $inner))
       (i32.add (call $work (i32.const 10)) (resume $c (cont.new $c (ref.func $middle)))))
     (func (export "work") (result i32) (call $run (ref.func $inner_work)))
     (func (export "yield") (result i32) (call $run (ref.func $inner_yield)))
     (func (export "interrupt") (result i32) (call $run (ref.func $inner_interrupt)))
     (func (export "trap") (result i32) (call $run (ref.func $inner_trap)))
     (func (export "host-throw") (result i32)
       (block $h
         (try_table (catch_all $h) (return (call $run (ref.func $inner_host_throw)))))
       (i32.const -1))
     (func $inner_tail (result i32) (return_call $yield (i32.const 9)))
     (func $tail_helper (result i32) (return_call $yield (i32.const 9)))
     (func $inner_tail_nested (result i32) (i32.add (i32.const 1) (call $tail_helper)))
     (elem declare func $inner_tail $inner_tail_nested)
     (func (export "tail-yield") (result i32) (call $run (ref.func $inner_tail)))
     (func (export "nested-tail-yield") (result i32) (call $run (ref.func $inner_tail_nested)))) */
static const uint8_t wasm_test_cont_chain_sections[] = {
    0x01, 0x25, 0x09, 0x5F, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x01, 0x60, 0x01, 0x7F,
    0x01, 0x7F, 0x5D, 0x03, 0x60, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x00, 0x60, 0x00, 0x02, 0x7F, 0x64,
    0x04, 0x60, 0x01, 0x64, 0x01, 0x01, 0x7F, 0x02, 0x32, 0x03, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x05,
    0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x03, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x09, 0x69, 0x6E, 0x74,
    0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x00, 0x06, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x0B, 0x63, 0x61,
    0x6C, 0x6C, 0x5F, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x73, 0x00, 0x06, 0x03, 0x15, 0x14, 0x03, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x03, 0x01, 0x06, 0x08, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x0D, 0x05, 0x02, 0x00, 0x03, 0x00, 0x05, 0x06, 0x07, 0x01, 0x63, 0x01, 0x01, 0xD0,
    0x01, 0x0B, 0x07, 0x5A, 0x08, 0x06, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x73, 0x00, 0x0B, 0x04, 0x77,
    0x6F, 0x72, 0x6B, 0x00, 0x0D, 0x05, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x0E, 0x09, 0x69, 0x6E,
    0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x00, 0x0F, 0x04, 0x74, 0x72, 0x61, 0x70, 0x00, 0x10,
    0x0A, 0x68, 0x6F, 0x73, 0x74, 0x2D, 0x74, 0x68, 0x72, 0x6F, 0x77, 0x00, 0x11, 0x0A, 0x74, 0x61,
    0x69, 0x6C, 0x2D, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x15, 0x11, 0x6E, 0x65, 0x73, 0x74, 0x65,
    0x64, 0x2D, 0x74, 0x61, 0x69, 0x6C, 0x2D, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x16, 0x09, 0x0F,
    0x02, 0x03, 0x00, 0x06, 0x04, 0x05, 0x06, 0x07, 0x08, 0x0A, 0x03, 0x00, 0x02, 0x12, 0x14, 0x0A,
    0xB7, 0x02, 0x14, 0x1E, 0x01, 0x01, 0x7F, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00, 0x20, 0x00, 0x6C,
    0x6A, 0x21, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x21, 0x00, 0x20, 0x00, 0x0D, 0x00, 0x0B, 0x20,
    0x01, 0x0B, 0x0C, 0x00, 0x41, 0xAC, 0x02, 0x10, 0x03, 0x41, 0x05, 0xE2, 0x00, 0x6A, 0x0B, 0x34,
    0x02, 0x01, 0x63, 0x00, 0x01, 0x7F, 0x41, 0x28, 0xFB, 0x00, 0x00, 0x21, 0x00, 0x41, 0xD0, 0x0F,
    0x10, 0x09, 0x1A, 0x41, 0x02, 0xFB, 0x00, 0x00, 0x41, 0x07, 0x10, 0x00, 0x21, 0x01, 0x41, 0x0A,
    0x10, 0x09, 0x1A, 0xFB, 0x02, 0x00, 0x00, 0x20, 0x00, 0xFB, 0x02, 0x00, 0x00, 0x6A, 0x20, 0x01,
    0x6A, 0xE2, 0x00, 0x0B, 0x08, 0x00, 0x10, 0x01, 0x41, 0x32, 0x10, 0x03, 0x0B, 0x03, 0x00, 0x00,
    0x0B, 0x06, 0x00, 0x10, 0x02, 0x41, 0x7F, 0x0B, 0x18, 0x00, 0x03, 0x40, 0x20, 0x00, 0xFB, 0x00,
    0x00, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x21, 0x00, 0x20, 0x00, 0x0D, 0x00, 0x0B, 0x41, 0x00,
    0x0B, 0x3A, 0x03, 0x01, 0x63, 0x04, 0x01, 0x7F, 0x01, 0x63, 0x00, 0x41, 0xD0, 0x0F, 0xFB, 0x00,
    0x00, 0x21, 0x02, 0x02, 0x07, 0x41, 0xE8, 0x07, 0x23, 0x00, 0xD4, 0xE0, 0x02, 0xE3, 0x02, 0x01,
    0x00, 0x00, 0x00, 0x6A, 0x0F, 0x0B, 0x21, 0x00, 0x21, 0x01, 0x20, 0x02, 0xFB, 0x02, 0x00, 0x00,
    0x20, 0x01, 0x41, 0x02, 0x6C, 0x20, 0x00, 0xE3, 0x04, 0x00, 0x6A, 0x0B, 0x06, 0x00, 0x41, 0x01,
    0x08, 0x01, 0x0B, 0x12, 0x00, 0x20, 0x00, 0x24, 0x00, 0x41, 0x0A, 0x10, 0x03, 0xD2, 0x0A, 0xE0,
    0x02, 0xE3, 0x02, 0x00, 0x6A, 0x0B, 0x06, 0x00, 0xD2, 0x04, 0x10, 0x0C, 0x0B, 0x06, 0x00, 0xD2,
    0x05, 0x10, 0x0C, 0x0B, 0x06, 0x00, 0xD2, 0x06, 0x10, 0x0C, 0x0B, 0x06, 0x00, 0xD2, 0x07, 0x10,
    0x0C, 0x0B, 0x12, 0x00, 0x02, 0x40, 0x1F, 0x40, 0x01, 0x02, 0x00, 0xD2, 0x08, 0x10, 0x0C, 0x0F,
    0x0B, 0x0B, 0x41, 0x7F, 0x0B, 0x06, 0x00, 0x41, 0x09, 0x12, 0x00, 0x0B, 0x06, 0x00, 0x41, 0x09,
    0x12, 0x00, 0x0B, 0x07, 0x00, 0x41, 0x01, 0x10, 0x13, 0x6A, 0x0B, 0x06, 0x00, 0xD2, 0x12, 0x10,
    0x0C, 0x0B, 0x06, 0x00, 0xD2, 0x14, 0x10, 0x0C, 0x0B,
};

/* (module
     (import "host" "yield" (func $yield (param i32) (result i32)))
     (func (export "spin") (param $n i32) (result i32) (local $acc i32)
       (loop $l
         (local.set $acc (i32.add (local.get $acc) (local.get $n)))
         (local.set $n (i32.sub (local.get $n) (i32.const 1)))
         (br_if $l (local.get $n)))
       (local.get $acc))
     (func (export "yielding") (param i32) (result i32) (i32.add (call $yield (local.get 0)) (i32.const 1)))) */
static const uint8_t wasm_test_cont_link_a_sections[] = {
    0x01, 0x06, 0x01, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x02, 0x0E, 0x01, 0x04, 0x68, 0x6F, 0x73, 0x74,
    0x05, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x00, 0x03, 0x03, 0x02, 0x00, 0x00, 0x07, 0x13, 0x02,
    0x04, 0x73, 0x70, 0x69, 0x6E, 0x00, 0x01, 0x08, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x69, 0x6E, 0x67,
    0x00, 0x02, 0x0A, 0x27, 0x02, 0x1B, 0x01, 0x01, 0x7F, 0x03, 0x40, 0x20, 0x01, 0x20, 0x00, 0x6A,
    0x21, 0x01, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x21, 0x00, 0x20, 0x00, 0x0D, 0x00, 0x0B, 0x20, 0x01,
    0x0B, 0x09, 0x00, 0x20, 0x00, 0x10, 0x00, 0x41, 0x01, 0x6A, 0x0B,
};

/* (module
     (type $f (func (result i32))) (type $c (cont $f))
     (import "a" "spin" (func $spin (param i32) (result i32)))
     (import "a" "yielding" (func $yielding (param i32) (result i32)))
     (func $in_cont (result i32) (call $spin (i32.const 1000)))
     (elem declare func $in_cont)
     (func (export "direct") (result i32) (i32.add (call $spin (i32.const 1000)) (call $yielding (i32.const 3))))
     (func (export "in-cont") (result i32) (resume $c (cont.new $c (ref.func $in_cont))))) */
static const uint8_t wasm_test_cont_link_b_sections[] = {
    0x01, 0x0C, 0x03, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x02, 0x17,
    0x02, 0x01, 0x61, 0x04, 0x73, 0x70, 0x69, 0x6E, 0x00, 0x02, 0x01, 0x61, 0x08, 0x79, 0x69, 0x65,
    0x6C, 0x64, 0x69, 0x6E, 0x67, 0x00, 0x02, 0x03, 0x04, 0x03, 0x00, 0x00, 0x00, 0x07, 0x14, 0x02,
    0x06, 0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x00, 0x03, 0x07, 0x69, 0x6E, 0x2D, 0x63, 0x6F, 0x6E,
    0x74, 0x00, 0x04, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x02, 0x0A, 0x20, 0x03, 0x07, 0x00, 0x41,
    0xE8, 0x07, 0x10, 0x00, 0x0B, 0x0C, 0x00, 0x41, 0xE8, 0x07, 0x10, 0x00, 0x41, 0x03, 0x10, 0x01,
    0x6A, 0x0B, 0x09, 0x00, 0xD2, 0x02, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0B,
};

TEST(test_stack_switching_switch) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_value_t trace;

    wasm_init(&rt);
    m = wasm_test_load_cont(&rt, wasm_test_cont_switch_sections, sizeof(wasm_test_cont_switch_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(m, "pingpong", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 6);
    WASM_CHECK_OK(t, wasm_global_get(m, "trace", &trace));
    WASM_CHECK_I32(t, trace.of.i32, 123456);
    CHECK(t, rt.exec == &rt.main_stack);
    CHECK_MSG(t, rt.num_cont_stacks == 1, "expected the coroutine left switched out to hold one stack, got %u",
              (unsigned)rt.num_cont_stacks);
    wasm_gc_collect(&rt);
    CHECK(t, rt.num_cont_stacks == 0);
    WASM_CHECK_OK(t, wasm_call(m, "to-bound", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 508);
    wasm_test_check_cont_trap(t, m, "no-handler", WASM_ERR_TRAP, "unhandled tag");
    wasm_test_check_cont_trap(t, m, "top-level", WASM_ERR_TRAP, "unhandled tag");
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected no live continuation stacks, got %u",
              (unsigned)rt.num_cont_stacks);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_resume_throw) {
    static const struct {
        const char* name;
        int32_t expected;
    } cases[] = {
        { "caught-inside", 105 }, { "to-resumer", 207 }, { "unstarted", 309 }, { "throw-ref", 103 },
        { "three-stacks", 411 },  { "caught-in-middle", 511 }, { "rethrown", 612 },
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    size_t i;

    wasm_init(&rt);
    m = wasm_test_load_cont(&rt, wasm_test_cont_throw_sections, sizeof(wasm_test_cont_throw_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        wasm_error_t err = wasm_call(m, cases[i].name, NULL, 0, &result, 1);

        CHECK_MSG(t, err == WASM_OK, "%s: %s", cases[i].name, rt.error_msg);
        if (err == WASM_OK)
            CHECK_MSG(t, result.of.i32 == cases[i].expected, "%s: expected %d, got %d", cases[i].name,
                      (int)cases[i].expected, (int)result.of.i32);
        CHECK_MSG(t, rt.num_cont_stacks == 0, "%s: %u continuation stacks left", cases[i].name,
                  (unsigned)rt.num_cont_stacks);
    }
    wasm_test_check_cont_trap(t, m, "null-exn", WASM_ERR_TRAP, "null exception reference");
    wasm_test_check_cont_trap(t, m, "uncaught", WASM_ERR_TRAP, "uncaught exception");
    wasm_test_check_cont_trap(t, m, "consumed", WASM_ERR_TRAP, "continuation already consumed");
    CHECK(t, rt.pending_exception.values == NULL);
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected no live continuation stacks, got %u",
              (unsigned)rt.num_cont_stacks);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static uint32_t wasm_test_cont_chain_yields;

static wasm_error_t wasm_test_cont_chain_yield(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_test_cont_chain_yields++;
    return WASM_YIELD;
}

static wasm_error_t wasm_test_cont_chain_interrupt(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                                   wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    wasm_runtime_interrupt(mod->rt);
    return WASM_OK;
}

static wasm_error_t wasm_test_cont_call_throws(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                               wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    return wasm_call(mod, "throws", NULL, 0, NULL, 0);
}

/* Returns the number of continuation stacks between rt's running stack and its main stack. */
static uint32_t wasm_test_cont_chain_depth(const wasm_runtime_t* rt) {
    const wasm__exec_stack_t* s;
    uint32_t depth = 0;

    for (s = rt->exec; s != &rt->main_stack; s = s->parent) depth++;
    return depth;
}

static wasm_module_t* wasm_test_load_cont_chain(wasm_runtime_t* rt) {
    wasm_test_register_host(rt, "yield", wasm_test_cont_chain_yield);
    wasm_test_register_host(rt, "interrupt", wasm_test_cont_chain_interrupt);
    wasm_test_register_host(rt, "call_throws", wasm_test_cont_call_throws);
    return wasm_test_load_cont(rt, wasm_test_cont_chain_sections, sizeof(wasm_test_cont_chain_sections));
}

TEST(test_stack_switching_fuel_in_chain) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    int32_t expected;
    uint32_t stops = 0;
    uint32_t deep_stops = 0;

    wasm_init(&rt);
    m = wasm_test_load_cont_chain(&rt);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(m, "work", NULL, 0, &result, 1));
    expected = result.of.i32;
    WASM_CHECK_I32(t, expected, 9047445);
    wasm_runtime_set_fuel(&rt, 100);
    err = wasm_call(m, "work", NULL, 0, &result, 1);
    while (err == WASM_ERR_OUT_OF_FUEL && stops < 100000) {
        CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_OUT_OF_FUEL);
        if (wasm_test_cont_chain_depth(&rt) == 2) deep_stops++;
        stops++;
        if (stops % 7 == 0) wasm_gc_collect(&rt);
        wasm_runtime_set_fuel(&rt, 100);
        err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
    }
    WASM_CHECK_OK(t, err);
    WASM_CHECK_I32(t, result.of.i32, expected);
    CHECK_MSG(t, deep_stops > 10, "expected fuel stops two continuations deep, got %u of %u", (unsigned)deep_stops,
              (unsigned)stops);
    CHECK(t, rt.exec == &rt.main_stack);
    CHECK(t, rt.num_cont_stacks == 0);

    wasm_runtime_set_fuel(&rt, 2000);
    err = wasm_call(m, "work", NULL, 0, &result, 1);
    while (err == WASM_ERR_OUT_OF_FUEL && wasm_test_cont_chain_depth(&rt) < 2) {
        wasm_runtime_set_fuel(&rt, 2000);
        err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
    }
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_FUEL, "expected a fuel stop, got %d", (int)err);
    CHECK(t, rt.num_cont_stacks == 2);
    wasm_runtime_cancel(&rt);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    CHECK(t, rt.exec == &rt.main_stack);
    CHECK_MSG(t, rt.num_cont_stacks == 0, "expected cancel to release the chain, %u live",
              (unsigned)rt.num_cont_stacks);
    wasm_runtime_disable_fuel(&rt);
    WASM_CHECK_OK(t, wasm_call(m, "work", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, expected);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_yield_in_chain) {
    static const uint32_t frames[] = { 5, 10, 12, 14 };
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_value_t host_result;
    wasm_error_t err;
    uint8_t* heap;
    uint32_t i;

    wasm_config_default(&cfg);
    cfg.initial_gc_heap_size = 4096;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_cont_chain(&rt);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_cont_chain_yields = 0;
    err = wasm_call(m, "yield", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_YIELD, "expected WASM_YIELD, got %d", (int)err);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_YIELD);
    CHECK(t, wasm_test_cont_chain_depth(&rt) == 2);
    CHECK_MSG(t, wasm_runtime_call_depth(&rt) == 4, "expected 4 frames across the chain, got %u",
              (unsigned)wasm_runtime_call_depth(&rt));
    for (i = 0; i < 4; i++) {
        uint32_t func_idx = UINT32_MAX;

        WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, i, NULL, &func_idx, NULL));
        CHECK_MSG(t, func_idx == frames[i], "frame %u: expected function %u, got %u", (unsigned)i,
                  (unsigned)frames[i], (unsigned)func_idx);
    }
    CHECK(t, wasm_runtime_call_frame(&rt, 4, NULL, NULL, NULL) != WASM_OK);
    heap = rt.gc_heap;
    wasm_gc_collect(&rt);
    CHECK_MSG(t, rt.gc_heap != heap, "%s", "expected the collection to move the live objects");
    CHECK(t, wasm_call(m, "work", NULL, 0, &result, 1) == WASM_ERR_SUSPENDED);
    host_result = wasm_i32(35);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 2539);
    CHECK(t, wasm_test_cont_chain_yields == 1);
    CHECK(t, rt.num_cont_stacks == 0);

    err = wasm_call(m, "yield", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_YIELD, "expected WASM_YIELD, got %d", (int)err);
    CHECK(t, rt.num_cont_stacks == 2);
    wasm_runtime_cancel(&rt);
    CHECK(t, rt.exec == &rt.main_stack);
    CHECK(t, rt.num_cont_stacks == 0);
    CHECK(t, wasm_runtime_call_depth(&rt) == 0);

    err = wasm_call(m, "tail-yield", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_YIELD, "expected WASM_YIELD, got %d", (int)err);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 1420);
    err = wasm_call(m, "nested-tail-yield", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_YIELD, "expected WASM_YIELD, got %d", (int)err);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &host_result, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 1421);
    CHECK(t, rt.num_cont_stacks == 0);

    err = wasm_call(m, "interrupt", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_INTERRUPTED, "expected WASM_ERR_INTERRUPTED, got %d", (int)err);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_ERR_INTERRUPTED);
    CHECK(t, wasm_test_cont_chain_depth(&rt) == 2);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 44310);
    CHECK(t, rt.num_cont_stacks == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_chain_traps) {
    static const uint32_t frames[] = { 7, 10, 12, 16 };
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint32_t i;

    wasm_init(&rt);
    m = wasm_test_load_cont_chain(&rt);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_check_cont_trap(t, m, "trap", WASM_ERR_UNREACHABLE, "unreachable");
    CHECK(t, rt.num_cont_stacks == 0);
    CHECK_MSG(t, wasm_runtime_call_depth(&rt) == 4, "expected a backtrace of 4 frames, got %u",
              (unsigned)wasm_runtime_call_depth(&rt));
    for (i = 0; i < 4; i++) {
        uint32_t func_idx = UINT32_MAX;

        WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, i, NULL, &func_idx, NULL));
        CHECK_MSG(t, func_idx == frames[i], "frame %u: expected function %u, got %u", (unsigned)i,
                  (unsigned)frames[i], (unsigned)func_idx);
    }
    err = wasm_call(m, "host-throw", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_TRAP, "expected an exception not to cross the host call, got %d", (int)err);
    CHECK_MSG(t, strstr(rt.error_msg, "uncaught exception") != NULL, "%s", rt.error_msg);
    CHECK(t, rt.exec == &rt.main_stack);
    CHECK(t, rt.num_cont_stacks == 0);
    CHECK(t, rt.pending_exception.values == NULL);
    wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_config_default(&cfg);
    cfg.max_call_depth = 3;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_cont_chain(&rt);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    wasm_test_check_cont_trap(t, m, "trap", WASM_ERR_UNREACHABLE, "unreachable");
    CHECK_MSG(t, wasm_runtime_call_depth(&rt) == 3, "expected the backtrace to keep 3 frames, got %u",
              (unsigned)wasm_runtime_call_depth(&rt));
    for (i = 0; i < 3; i++) {
        uint32_t func_idx = UINT32_MAX;

        WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, i, NULL, &func_idx, NULL));
        CHECK(t, func_idx == frames[i]);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static wasm_error_t wasm_test_cont_link_yield(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                              wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)results;
    (void)num_results;
    (void)userdata;
    return WASM_YIELD;
}

TEST(test_stack_switching_linked_imports) {
    wasm_runtime_t rt;
    wasm_module_t* a;
    wasm_module_t* b;
    wasm_value_t result;
    wasm_value_t host_result;
    wasm_error_t err;
    uint32_t fuel_stops = 0;
    uint32_t yields = 0;

    wasm_init(&rt);
    wasm_test_register_host(&rt, "yield", wasm_test_cont_link_yield);
    a = wasm_test_load_cont(&rt, wasm_test_cont_link_a_sections, sizeof(wasm_test_cont_link_a_sections));
    CHECK_MSG(t, a != NULL, "%s", rt.error_msg);
    if (a == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "a", a));
    b = wasm_test_load_cont(&rt, wasm_test_cont_link_b_sections, sizeof(wasm_test_cont_link_b_sections));
    CHECK_MSG(t, b != NULL, "%s", rt.error_msg);
    if (b == NULL) {
        wasm_module_free(a);
        wasm_destroy(&rt);
        return;
    }
    wasm_runtime_set_fuel(&rt, 300);
    err = wasm_call(b, "direct", NULL, 0, &result, 1);
    while ((err == WASM_ERR_OUT_OF_FUEL || err == WASM_YIELD) && fuel_stops < 1000) {
        wasm_runtime_set_fuel(&rt, 300);
        if (err == WASM_YIELD) {
            yields++;
            host_result = wasm_i32(10);
            err = wasm_runtime_resume(&rt, &host_result, 1, &result, 1);
        } else {
            fuel_stops++;
            err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
        }
    }
    WASM_CHECK_OK(t, err);
    WASM_CHECK_I32(t, result.of.i32, 500511);
    CHECK_MSG(t, fuel_stops > 5 && yields == 1, "expected fuel stops and one yield, got %u and %u",
              (unsigned)fuel_stops, (unsigned)yields);

    err = wasm_call(b, "in-cont", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_OUT_OF_FUEL, "expected WASM_ERR_OUT_OF_FUEL, got %d", (int)err);
    CHECK(t, rt.num_cont_stacks == 1);
    CHECK(t, rt.exec != &rt.main_stack && rt.exec->call_frames[rt.exec->call_frame_sp - 1u].module == a);
    wasm_module_free(a);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_OK);
    CHECK(t, rt.exec == &rt.main_stack);
    CHECK(t, rt.num_cont_stacks == 0);
    wasm_runtime_disable_fuel(&rt);
    wasm_test_check_cont_trap(t, b, "direct", WASM_ERR_TRAP, "freed module");
    wasm_test_check_cont_trap(t, b, "in-cont", WASM_ERR_TRAP, "freed module");
    CHECK(t, rt.num_cont_stacks == 0);
    wasm_module_free(b);
    wasm_destroy(&rt);
}

/* (module (type $f (func (result i32))) (type $c (cont $f))
     (type $g (func (param i32) (result i32))) (type $cg (cont $g))
     (import "host" "get" (func $get (result (ref null $c))))
     (import "host" "yield" (func $yield (result (ref null $c))))
     (func $forty-two (result i32) (i32.const 42))
     (func $add1 (param i32) (result i32) (i32.add (local.get 0) (i32.const 1)))
     (elem declare func $forty-two $add1)
     (global (export "g") (mut (ref null $c)) (ref.null $c))
     (table (export "t") 1 (ref null $c))
     (func (export "make") (result (ref $c)) (cont.new $c (ref.func $forty-two)))
     (func (export "make-g") (result (ref $cg)) (cont.new $cg (ref.func $add1)))
     (func (export "run") (param (ref null $c)) (result i32) (resume $c (local.get 0)))
     (func (export "is-null") (param contref) (result i32) (ref.is_null (local.get 0)))
     (func (export "run-global") (result i32) (resume $c (global.get 0)))
     (func (export "run-table") (result i32) (resume $c (table.get 0 (i32.const 0))))
     (func (export "run-host") (result i32) (resume $c (call $get)))
     (func (export "run-yield") (result i32) (resume $c (call $yield)))) */
static const uint8_t wasm_test_cont_boundary_sections[] = {
    0x01, 0x28, 0x09, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x02,
    0x60, 0x00, 0x01, 0x63, 0x01, 0x60, 0x00, 0x01, 0x64, 0x01, 0x60, 0x00, 0x01, 0x64, 0x03, 0x60,
    0x01, 0x63, 0x01, 0x01, 0x7F, 0x60, 0x01, 0x68, 0x01, 0x7F, 0x02, 0x19, 0x02, 0x04, 0x68, 0x6F,
    0x73, 0x74, 0x03, 0x67, 0x65, 0x74, 0x00, 0x04, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x05, 0x79, 0x69,
    0x65, 0x6C, 0x64, 0x00, 0x04, 0x03, 0x0B, 0x0A, 0x00, 0x02, 0x05, 0x06, 0x07, 0x08, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x05, 0x01, 0x63, 0x01, 0x00, 0x01, 0x06, 0x07, 0x01, 0x63, 0x01, 0x01, 0xD0,
    0x01, 0x0B, 0x07, 0x59, 0x0A, 0x01, 0x67, 0x03, 0x00, 0x01, 0x74, 0x01, 0x00, 0x04, 0x6D, 0x61,
    0x6B, 0x65, 0x00, 0x04, 0x06, 0x6D, 0x61, 0x6B, 0x65, 0x2D, 0x67, 0x00, 0x05, 0x03, 0x72, 0x75,
    0x6E, 0x00, 0x06, 0x07, 0x69, 0x73, 0x2D, 0x6E, 0x75, 0x6C, 0x6C, 0x00, 0x07, 0x0A, 0x72, 0x75,
    0x6E, 0x2D, 0x67, 0x6C, 0x6F, 0x62, 0x61, 0x6C, 0x00, 0x08, 0x09, 0x72, 0x75, 0x6E, 0x2D, 0x74,
    0x61, 0x62, 0x6C, 0x65, 0x00, 0x09, 0x08, 0x72, 0x75, 0x6E, 0x2D, 0x68, 0x6F, 0x73, 0x74, 0x00,
    0x0A, 0x09, 0x72, 0x75, 0x6E, 0x2D, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x0B, 0x09, 0x06, 0x01,
    0x03, 0x00, 0x02, 0x02, 0x03, 0x0A, 0x4C, 0x0A, 0x04, 0x00, 0x41, 0x2A, 0x0B, 0x07, 0x00, 0x20,
    0x00, 0x41, 0x01, 0x6A, 0x0B, 0x06, 0x00, 0xD2, 0x02, 0xE0, 0x01, 0x0B, 0x06, 0x00, 0xD2, 0x03,
    0xE0, 0x03, 0x0B, 0x07, 0x00, 0x20, 0x00, 0xE3, 0x01, 0x00, 0x0B, 0x05, 0x00, 0x20, 0x00, 0xD1,
    0x0B, 0x07, 0x00, 0x23, 0x00, 0xE3, 0x01, 0x00, 0x0B, 0x09, 0x00, 0x41, 0x00, 0x25, 0x00, 0xE3,
    0x01, 0x00, 0x0B, 0x07, 0x00, 0x10, 0x00, 0xE3, 0x01, 0x00, 0x0B, 0x07, 0x00, 0x10, 0x01, 0xE3,
    0x01, 0x00, 0x0B,
};

static wasm_error_t wasm_test_cont_boundary_get(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                                wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)num_results;
    results[0] = *(const wasm_value_t*)userdata;
    return WASM_OK;
}

static wasm_module_t* wasm_test_load_cont_boundary(wasm_runtime_t* rt, wasm_value_t* host_value) {
    wasm_import_t imp;

    memset(&imp, 0, sizeof(imp));
    imp.module = "host";
    imp.name = "get";
    imp.func = wasm_test_cont_boundary_get;
    imp.userdata = host_value;
    wasm_register_import(rt, &imp);
    wasm_test_register_host(rt, "yield", wasm_test_cont_yield);
    return wasm_test_load_cont(rt, wasm_test_cont_boundary_sections, sizeof(wasm_test_cont_boundary_sections));
}

static void wasm_test_check_cont_arg(test_ctx* t, wasm_module_t* m, const char* func_name, wasm_value_t arg,
                                     wasm_error_t err) {
    wasm_value_t result;
    wasm_error_t actual = wasm_call(m, func_name, &arg, 1, &result, 1);

    CHECK_MSG(t, actual == err, "%s: expected error %d, got %d (%s)", func_name, (int)err, (int)actual,
              m->rt->error_msg);
}

TEST(test_stack_switching_host_boundary) {
    wasm_runtime_t rt;
    wasm_runtime_t other;
    wasm_module_t* m;
    wasm_module_t* m2;
    wasm_module_t* foreign;
    wasm_value_t host_value = wasm_ref_null(WASM_TYPE_NOCONT);
    wasm_value_t other_value = wasm_ref_null(WASM_TYPE_NOCONT);
    wasm_value_t forged = wasm_ref_null(WASM_TYPE_CONTREF);
    wasm_value_t k;
    wasm_value_t k2;
    wasm_value_t result;
    wasm_value_t* held;
    wasm_error_t err;
    uint64_t fake[8] = { 0 };

    forged.of.externref = (uintptr_t)fake;
    wasm_init(&rt);
    m = wasm_test_load_cont_boundary(&rt, &host_value);
    m2 = wasm_test_load_cont_boundary(&rt, &host_value);
    CHECK_MSG(t, m != NULL && m2 != NULL, "%s", rt.error_msg);
    if (m == NULL || m2 == NULL) {
        if (m != NULL) wasm_module_free(m);
        if (m2 != NULL) wasm_module_free(m2);
        wasm_destroy(&rt);
        return;
    }

    WASM_CHECK_OK(t, wasm_call(m, "make", NULL, 0, &k, 1));
    CHECK(t, k.type == WASM_TYPE_CONTREF && k.of.externref != 0);
    WASM_CHECK_OK(t, wasm_call(m, "is-null", &k, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 0);
    WASM_CHECK_OK(t, wasm_call(m, "run", &k, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    err = wasm_call(m, "run", &k, 1, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_TRAP, "expected WASM_ERR_TRAP, got %d", (int)err);
    CHECK_MSG(t, strstr(rt.error_msg, "continuation already consumed") != NULL, "%s", rt.error_msg);

    wasm_test_check_cont_arg(t, m, "run", forged, WASM_ERR_TYPE_MISMATCH);
    wasm_test_check_cont_arg(t, m, "is-null", forged, WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_global_set(m, "g", forged) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_table_set(m, 0, 0, forged) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_table_grow(m, 0, 1, forged) == -1);
    host_value = forged;
    wasm_test_check_cont_trap(t, m, "run-host", WASM_ERR_TYPE_MISMATCH, "host function result 0");
    held = wasm_ref_hold(&rt, forged);
    CHECK(t, held != NULL);
    wasm_gc_collect(&rt);
    wasm_ref_release(&rt, held);

    WASM_CHECK_OK(t, wasm_call(m, "make-g", NULL, 0, &k, 1));
    wasm_test_check_cont_arg(t, m, "run", k, WASM_ERR_TYPE_MISMATCH);
    WASM_CHECK_OK(t, wasm_call(m, "is-null", &k, 1, &result, 1));

    WASM_CHECK_OK(t, wasm_call(m, "make", NULL, 0, &k, 1));
    WASM_CHECK_OK(t, wasm_call(m2, "run", &k, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_OK(t, wasm_call(m, "make", NULL, 0, &k, 1));
    WASM_CHECK_OK(t, wasm_global_set(m, "g", k));
    WASM_CHECK_OK(t, wasm_call(m, "make", NULL, 0, &k, 1));
    WASM_CHECK_OK(t, wasm_table_set(m, 0, 0, k));
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_call(m, "run-global", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    WASM_CHECK_OK(t, wasm_call(m, "run-table", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    wasm_test_check_cont_trap(t, m, "run-global", WASM_ERR_TRAP, "continuation already consumed");
    WASM_CHECK_OK(t, wasm_call(m, "make", NULL, 0, &host_value, 1));
    WASM_CHECK_OK(t, wasm_call(m, "run-host", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);

    WASM_CHECK_OK(t, wasm_call(m, "make", NULL, 0, &k, 1));
    held = wasm_ref_hold(&rt, k);
    CHECK(t, held != NULL);
    WASM_CHECK_OK(t, wasm_call(m, "make", NULL, 0, &k2, 1));
    wasm_gc_collect(&rt);
    wasm_test_check_cont_arg(t, m, "run", k2, WASM_ERR_TYPE_MISMATCH);
    if (held != NULL) {
        WASM_CHECK_OK(t, wasm_call(m, "run", held, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 42);
        wasm_ref_release(&rt, held);
    }

    WASM_CHECK_OK(t, wasm_call(m, "make", NULL, 0, &k, 1));
    err = wasm_call(m, "run-yield", NULL, 0, &result, 1);
    CHECK_MSG(t, err == WASM_YIELD, "expected WASM_YIELD, got %d", (int)err);
    err = wasm_runtime_resume(&rt, &forged, 1, &result, 1);
    CHECK_MSG(t, err == WASM_ERR_TYPE_MISMATCH, "expected WASM_ERR_TYPE_MISMATCH, got %d", (int)err);
    CHECK(t, wasm_runtime_suspended(&rt) == WASM_YIELD);
    WASM_CHECK_OK(t, wasm_runtime_resume(&rt, &k, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);

    WASM_CHECK_OK(t, wasm_test_init_raw(&other, NULL));
    foreign = wasm_test_load_cont_boundary(&other, &other_value);
    CHECK_MSG(t, foreign != NULL, "%s", other.error_msg);
    if (foreign != NULL) {
        WASM_CHECK_OK(t, wasm_call(foreign, "make", NULL, 0, &k, 1));
        wasm_test_check_cont_arg(t, m, "run", k, WASM_ERR_TYPE_MISMATCH);
        CHECK(t, wasm_global_set(m, "g", k) == WASM_ERR_TYPE_MISMATCH);
        WASM_CHECK_OK(t, wasm_call(foreign, "run", &k, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 42);
        wasm_module_free(foreign);
    }
    wasm__runtime_destroy(&other);
    wasm_module_free(m2);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module (type $f (func (param i32) (result i32))) (type $c (cont $f))
     (global $g (mut (ref null $c)) (ref.null $c)) (table 1 (ref null $c))
     (func (export "triple") (param i32) (result i32) (i32.mul (local.get 0) (i32.const 3)))
     (func (export "put") (param (ref null $c)) (table.set 0 (i32.const 0) (local.get 0)))
     (func (export "run") (param (ref null $c)) (result i32) (resume $c (i32.const 5) (local.get 0)))
     (func (export "store-run-from-table") (result i32)
       (global.set $g (table.get 0 (i32.const 0))) (resume $c (i32.const 5) (global.get $g)))) */
static const uint8_t wasm_test_cont_owner_code_sections[] = {
    0x01, 0x17, 0x05, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x01, 0x63, 0x01, 0x00, 0x60,
    0x01, 0x63, 0x01, 0x01, 0x7F, 0x60, 0x00, 0x01, 0x7F, 0x03, 0x05, 0x04, 0x00, 0x02, 0x03, 0x04,
    0x04, 0x05, 0x01, 0x63, 0x01, 0x00, 0x01, 0x06, 0x07, 0x01, 0x63, 0x01, 0x01, 0xD0, 0x01, 0x0B,
    0x07, 0x2D, 0x04, 0x06, 0x74, 0x72, 0x69, 0x70, 0x6C, 0x65, 0x00, 0x00, 0x03, 0x70, 0x75, 0x74,
    0x00, 0x01, 0x03, 0x72, 0x75, 0x6E, 0x00, 0x02, 0x14, 0x73, 0x74, 0x6F, 0x72, 0x65, 0x2D, 0x72,
    0x75, 0x6E, 0x2D, 0x66, 0x72, 0x6F, 0x6D, 0x2D, 0x74, 0x61, 0x62, 0x6C, 0x65, 0x00, 0x03, 0x0A,
    0x2C, 0x04, 0x07, 0x00, 0x20, 0x00, 0x41, 0x03, 0x6C, 0x0B, 0x08, 0x00, 0x41, 0x00, 0x20, 0x00,
    0x26, 0x00, 0x0B, 0x09, 0x00, 0x41, 0x05, 0x20, 0x00, 0xE3, 0x01, 0x00, 0x0B, 0x0F, 0x00, 0x41,
    0x00, 0x25, 0x00, 0x24, 0x00, 0x41, 0x05, 0x23, 0x00, 0xE3, 0x01, 0x00, 0x0B,
};

/* (module (type $x (struct)) (type $f (func (param i32) (result i32))) (type $c (cont $f))
     (import "B" "triple" (func $triple (type $f))) (import "B" "put" (func $put (param (ref null $c))))
     (elem declare func $triple)
     (func (export "make") (result (ref $c))
       (call $put (cont.new $c (ref.func $triple))) (cont.new $c (ref.func $triple)))) */
static const uint8_t wasm_test_cont_owner_type_sections[] = {
    0x01, 0x14, 0x05, 0x5F, 0x00, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x01, 0x60, 0x01, 0x63, 0x02,
    0x00, 0x60, 0x00, 0x01, 0x64, 0x02, 0x02, 0x14, 0x02, 0x01, 0x42, 0x06, 0x74, 0x72, 0x69, 0x70,
    0x6C, 0x65, 0x00, 0x01, 0x01, 0x42, 0x03, 0x70, 0x75, 0x74, 0x00, 0x03, 0x03, 0x02, 0x01, 0x04,
    0x07, 0x08, 0x01, 0x04, 0x6D, 0x61, 0x6B, 0x65, 0x00, 0x02, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01,
    0x00, 0x0A, 0x0E, 0x01, 0x0C, 0x00, 0xD2, 0x00, 0xE0, 0x02, 0x10, 0x01, 0xD2, 0x00, 0xE0, 0x02,
    0x0B,
};

TEST(test_stack_switching_continuations_outlive_their_type_module) {
    wasm_runtime_t rt;
    wasm_module_t* code;
    wasm_module_t* types;
    wasm_value_t k;
    wasm_value_t* held = NULL;
    wasm_value_t result;

    wasm_init(&rt);
    code = wasm_test_load_cont(&rt, wasm_test_cont_owner_code_sections, sizeof(wasm_test_cont_owner_code_sections));
    CHECK_MSG(t, code != NULL, "%s", rt.error_msg);
    if (code == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "B", code));
    types = wasm_test_load_cont(&rt, wasm_test_cont_owner_type_sections, sizeof(wasm_test_cont_owner_type_sections));
    CHECK_MSG(t, types != NULL, "%s", rt.error_msg);
    if (types != NULL) {
        WASM_CHECK_OK(t, wasm_call(types, "make", NULL, 0, &k, 1));
        held = wasm_ref_hold(&rt, k);
        wasm_module_free(types);
        CHECK(t, rt.type_owners != NULL);
        wasm_gc_collect(&rt);
        WASM_CHECK_OK(t, wasm_call(code, "store-run-from-table", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 15);
        CHECK(t, held != NULL);
        if (held != NULL) {
            WASM_CHECK_OK(t, wasm_call(code, "run", held, 1, &result, 1));
            WASM_CHECK_I32(t, result.of.i32, 15);
            wasm_ref_release(&rt, held);
        }
        WASM_CHECK_OK(t, wasm_table_set(code, 0, 0, wasm_ref_null(WASM_TYPE_CONTREF)));
        WASM_CHECK_OK(t, wasm_global_set_index(code, 0, wasm_ref_null(WASM_TYPE_CONTREF)));
        wasm_gc_collect(&rt);
        CHECK_MSG(t, rt.type_owners == NULL, "%s", "expected the freed module's types to be released");
    }
    wasm_module_free(code);
    wasm_destroy(&rt);
}

/* (module (type $s (struct (field i32))) (type $f (func (result i32))) (type $c (cont $f))
     (import "host" "collect" (func $collect (param (ref null $c) (ref null $s) anyref) (result i32)))
     (func $seven (result i32) (i32.const 7)) (elem declare func $seven)
     (func (export "run") (param (ref null $c)) (result i32) (resume $c (local.get 0)))
     (func (export "go") (param externref) (result i32)
       (call $collect (cont.new $c (ref.func $seven)) (struct.new $s (i32.const 35))
         (any.convert_extern (local.get 0))))) */
static const uint8_t wasm_test_cont_host_args_sections[] = {
    0x01, 0x1F, 0x06, 0x5F, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x01, 0x60, 0x03, 0x63,
    0x02, 0x63, 0x00, 0x6E, 0x01, 0x7F, 0x60, 0x01, 0x63, 0x02, 0x01, 0x7F, 0x60, 0x01, 0x6F, 0x01,
    0x7F, 0x02, 0x10, 0x01, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x07, 0x63, 0x6F, 0x6C, 0x6C, 0x65, 0x63,
    0x74, 0x00, 0x03, 0x03, 0x04, 0x03, 0x01, 0x04, 0x05, 0x07, 0x0C, 0x02, 0x03, 0x72, 0x75, 0x6E,
    0x00, 0x02, 0x02, 0x67, 0x6F, 0x00, 0x03, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x01, 0x0A, 0x20,
    0x03, 0x04, 0x00, 0x41, 0x07, 0x0B, 0x07, 0x00, 0x20, 0x00, 0xE3, 0x02, 0x00, 0x0B, 0x11, 0x00,
    0xD2, 0x01, 0xE0, 0x02, 0x41, 0x23, 0xFB, 0x00, 0x00, 0x20, 0x00, 0xFB, 0x1A, 0x10, 0x00, 0x0B,
};

/* Collects, then checks the anyref argument still holds the bits *userdata names and returns the struct
 * argument's field plus the result of resuming the continuation argument. */
static wasm_error_t wasm_test_collect_args(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                           wasm_value_t* results, uint32_t num_results, void* userdata) {
    wasm_value_t field;
    wasm_error_t err;

    (void)num_args;
    (void)num_results;
    wasm_gc_collect(wasm_module_runtime(mod));
    if (args[2].of.gc_ref != *(const uintptr_t*)userdata) return WASM_ERR_TRAP;
    err = wasm_struct_get_field(mod, args[1], 0, &field);
    if (err != WASM_OK) return err;
    err = wasm_call(mod, "run", &args[0], 1, results, 1);
    if (err != WASM_OK) return err;
    results[0].of.i32 += field.of.i32;
    return WASM_OK;
}

TEST(test_stack_switching_host_arguments_are_roots) {
    wasm_runtime_t rt;
    wasm_import_t imp;
    wasm_module_t* m;
    wasm_value_t arg;
    wasm_value_t result;
    uintptr_t host_bits;

    wasm_init(&rt);
    memset(&imp, 0, sizeof(imp));
    imp.module = "host";
    imp.name = "collect";
    imp.func = wasm_test_collect_args;
    imp.userdata = &host_bits;
    WASM_CHECK_OK(t, wasm_register_import(&rt, &imp));
    m = wasm_test_load_cont(&rt, wasm_test_cont_host_args_sections, sizeof(wasm_test_cont_host_args_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    host_bits = (uintptr_t)rt.gc_heap + 64u;
    arg = wasm_externref(host_bits);
    WASM_CHECK_OK(t, wasm_call(m, "go", &arg, 1, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 42);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module (type $f1 (func (param i32) (result i32))) (type $c1 (cont $f1)) (type $f0 (func (result i32)))
     (type $c0 (cont $f0)) (rec (type $fs (func (param i32 (ref null $cs)) (result i32))) (type $cs (cont $fs)))
     (tag $sw (result i32))
     (import "host" "add1" (func $add1 (param i32) (result i32)))
     (import "host" "yield" (func $yield (param i32) (result i32)))
     (import "host" "swt" (func $swt (type $fs))) (import "host" "swy" (func $swy (type $fs)))
     (elem declare func $add1 $yield $swt $swy $switch-add $switch-yield $ytc $ytc2)
     (func (export "start") (result i32) (resume $c1 (i32.const 5) (cont.new $c1 (ref.func $add1))))
     (func (export "start-bound") (result i32)
       (resume $c0 (cont.bind $c1 $c0 (i32.const 7) (cont.new $c1 (ref.func $add1)))))
     (func $use (param i32 (ref null $cs)) (result i32) (i32.const -1))
     (func $switch-add (result i32) (call $use (switch $cs $sw (i32.const 3) (cont.new $cs (ref.func $swt)))))
     (func (export "switch") (result i32) (resume $c0 (on $sw switch) (cont.new $c0 (ref.func $switch-add))))
     (func (export "start-yield") (result i32)
       (i32.add (i32.const 1000) (resume $c1 (i32.const 5) (cont.new $c1 (ref.func $yield)))))
     (func $switch-yield (result i32) (call $use (switch $cs $sw (i32.const 3) (cont.new $cs (ref.func $swy)))))
     (func (export "switch-yield") (result i32)
       (i32.add (i32.const 2000) (resume $c0 (on $sw switch) (cont.new $c0 (ref.func $switch-yield)))))
     (func $ytc (param i32) (result i32) (return_call $yield (local.get 0)))
     (func (export "tail-yield") (result i32)
       (i32.add (i32.const 100) (resume $c1 (i32.const 5) (cont.new $c1 (ref.func $ytc)))))
     (func $ytc2 (param i32) (result i32)
       (i32.add (i32.const 1000) (resume $c1 (local.get 0) (cont.new $c1 (ref.func $ytc)))))
     (func (export "nested-tail-yield") (result i32)
       (i32.add (i32.const 100) (resume $c1 (i32.const 5) (cont.new $c1 (ref.func $ytc2)))))) */
static const uint8_t wasm_test_cont_host_targets_sections[] = {
    0x01, 0x20, 0x06, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x02,
    0x4E, 0x02, 0x60, 0x02, 0x7F, 0x63, 0x05, 0x01, 0x7F, 0x5D, 0x04, 0x60, 0x02, 0x7F, 0x63, 0x05,
    0x01, 0x7F, 0x02, 0x30, 0x04, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x04, 0x61, 0x64, 0x64, 0x31, 0x00,
    0x00, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x05, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x00, 0x04, 0x68,
    0x6F, 0x73, 0x74, 0x03, 0x73, 0x77, 0x74, 0x00, 0x04, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x03, 0x73,
    0x77, 0x79, 0x00, 0x04, 0x03, 0x0D, 0x0C, 0x02, 0x02, 0x06, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00,
    0x02, 0x00, 0x02, 0x0D, 0x03, 0x01, 0x00, 0x02, 0x07, 0x5E, 0x07, 0x05, 0x73, 0x74, 0x61, 0x72,
    0x74, 0x00, 0x04, 0x0B, 0x73, 0x74, 0x61, 0x72, 0x74, 0x2D, 0x62, 0x6F, 0x75, 0x6E, 0x64, 0x00,
    0x05, 0x06, 0x73, 0x77, 0x69, 0x74, 0x63, 0x68, 0x00, 0x08, 0x0B, 0x73, 0x74, 0x61, 0x72, 0x74,
    0x2D, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x09, 0x0C, 0x73, 0x77, 0x69, 0x74, 0x63, 0x68, 0x2D,
    0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x0B, 0x0A, 0x74, 0x61, 0x69, 0x6C, 0x2D, 0x79, 0x69, 0x65,
    0x6C, 0x64, 0x00, 0x0D, 0x11, 0x6E, 0x65, 0x73, 0x74, 0x65, 0x64, 0x2D, 0x74, 0x61, 0x69, 0x6C,
    0x2D, 0x79, 0x69, 0x65, 0x6C, 0x64, 0x00, 0x0F, 0x09, 0x0C, 0x01, 0x03, 0x00, 0x08, 0x00, 0x01,
    0x02, 0x03, 0x07, 0x0A, 0x0C, 0x0E, 0x0A, 0xA0, 0x01, 0x0C, 0x0B, 0x00, 0x41, 0x05, 0xD2, 0x00,
    0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0B, 0x0E, 0x00, 0x41, 0x07, 0xD2, 0x00, 0xE0, 0x01, 0xE1, 0x01,
    0x03, 0xE3, 0x03, 0x00, 0x0B, 0x04, 0x00, 0x41, 0x7F, 0x0B, 0x0D, 0x00, 0x41, 0x03, 0xD2, 0x02,
    0xE0, 0x05, 0xE6, 0x05, 0x00, 0x10, 0x06, 0x0B, 0x0B, 0x00, 0xD2, 0x07, 0xE0, 0x03, 0xE3, 0x03,
    0x01, 0x01, 0x00, 0x0B, 0x0F, 0x00, 0x41, 0xE8, 0x07, 0x41, 0x05, 0xD2, 0x01, 0xE0, 0x01, 0xE3,
    0x01, 0x00, 0x6A, 0x0B, 0x0D, 0x00, 0x41, 0x03, 0xD2, 0x03, 0xE0, 0x05, 0xE6, 0x05, 0x00, 0x10,
    0x06, 0x0B, 0x0F, 0x00, 0x41, 0xD0, 0x0F, 0xD2, 0x0A, 0xE0, 0x03, 0xE3, 0x03, 0x01, 0x01, 0x00,
    0x6A, 0x0B, 0x06, 0x00, 0x20, 0x00, 0x12, 0x01, 0x0B, 0x0F, 0x00, 0x41, 0xE4, 0x00, 0x41, 0x05,
    0xD2, 0x0C, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x6A, 0x0B, 0x0F, 0x00, 0x41, 0xE8, 0x07, 0x20, 0x00,
    0xD2, 0x0C, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x6A, 0x0B, 0x0F, 0x00, 0x41, 0xE4, 0x00, 0x41, 0x05,
    0xD2, 0x0E, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x6A, 0x0B,
};

static wasm_error_t wasm_test_cont_add1(wasm_module_t* mod, const wasm_value_t* args, uint32_t num_args,
                                        wasm_value_t* results, uint32_t num_results, void* userdata) {
    (void)mod;
    (void)num_args;
    (void)num_results;
    results[0] = wasm_i32(args[0].of.i32 + (int32_t)(intptr_t)userdata);
    return WASM_OK;
}

TEST(test_stack_switching_host_function_targets) {
    static const struct {
        const char* name;
        int32_t expected;
    } calls[] = { { "start", 6 }, { "start-bound", 8 }, { "switch", 1003 } };
    static const struct {
        const char* name;
        int32_t expected;
    } yields[] = { { "start-yield", 1041 }, { "switch-yield", 2041 }, { "tail-yield", 141 }, { "nested-tail-yield", 1141 } };
    wasm_runtime_t rt;
    wasm_import_t imp;
    wasm_module_t* m;
    wasm_value_t host_result = wasm_i32(41);
    wasm_value_t result;
    uint32_t i;

    wasm_init(&rt);
    memset(&imp, 0, sizeof(imp));
    imp.module = "host";
    imp.func = wasm_test_cont_add1;
    imp.name = "add1";
    imp.userdata = (void*)(intptr_t)1;
    WASM_CHECK_OK(t, wasm_register_import(&rt, &imp));
    imp.name = "swt";
    imp.userdata = (void*)(intptr_t)1000;
    WASM_CHECK_OK(t, wasm_register_import(&rt, &imp));
    wasm_test_register_host(&rt, "yield", wasm_test_cont_yield);
    wasm_test_register_host(&rt, "swy", wasm_test_cont_yield);
    m = wasm_test_load_cont(&rt, wasm_test_cont_host_targets_sections, sizeof(wasm_test_cont_host_targets_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(calls); i++) {
        wasm_error_t err = wasm_call(m, calls[i].name, NULL, 0, &result, 1);

        CHECK_MSG(t, err == WASM_OK && result.of.i32 == calls[i].expected, "%s: err=%d result=%d (%s)",
                  calls[i].name, (int)err, (int)result.of.i32, rt.error_msg);
    }
    for (i = 0; i < COUNTOF(yields); i++) {
        wasm_error_t err = wasm_call(m, yields[i].name, NULL, 0, &result, 1);

        CHECK_MSG(t, err == WASM_YIELD, "%s: expected a yield, got %d (%s)", yields[i].name, (int)err, rt.error_msg);
        if (err != WASM_YIELD) continue;
        err = wasm_runtime_resume(&rt, &host_result, 1, &result, 1);
        CHECK_MSG(t, err == WASM_OK && result.of.i32 == yields[i].expected, "%s: err=%d result=%d (%s)",
                  yields[i].name, (int)err, (int)result.of.i32, rt.error_msg);
    }
    wasm_gc_collect(&rt);
    CHECK(t, rt.num_cont_stacks == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module
     (rec (type $fk (func (param i32 (ref null $ck)) (result i32))) (type $ck (cont $fk)))
     (type $f0 (func (result i32))) (type $c0 (cont $f0)) (type $f1 (func (param i32) (result i32))) (type $c1 (cont $f1))
     (tag $sw (result i32)) (tag $y (param i32) (result i32)) (tag $e (param i32)) (global $log (mut i32) (i32.const 0))
     (func $gen (param $n i32) (result i32)
       (loop $l (drop (suspend $y (local.get $n)))
         (local.set $n (i32.sub (local.get $n) (i32.const 1))) (br_if $l (local.get $n)))
       (i32.const 0))
     (func $genouter (param $n i32) (result i32) (local $k (ref null $c1)) (local $acc i32)
       (local.set $k (cont.bind $c1 $c1 (cont.new $c1 (ref.func $gen)))) (local.set $n (i32.const 50))
       (loop $l
         (block $h (result i32 (ref $c1))
           (return (i32.add (local.get $acc) (resume $c1 (on $y $h) (local.get $n) (local.get $k)))))
         (local.set $k) (suspend $y) (local.get $acc) (i32.add) (local.set $acc) (local.set $n (i32.const 0)) (br $l))
       (unreachable))
     (func (export "generators") (result i32) (local $k (ref null $c1)) (local $acc i32) (local $v i32)
       (local.set $k (cont.new $c1 (ref.func $genouter)))
       (loop $l
         (block $h (result i32 (ref $c1))
           (return (i32.add (local.get $acc) (resume $c1 (on $y $h) (local.get $v) (local.get $k)))))
         (local.set $k) (local.set $v) (local.set $acc (i32.add (local.get $acc) (local.get $v)))
         (local.set $v (i32.mul (local.get $v) (i32.const 2))) (br $l))
       (unreachable))
     (func $pp (type $fk) (local $n i32) (local $peer (ref null $ck))
       (local.set $n (local.get 0)) (local.set $peer (local.get 1))
       (loop $l
         (global.set $log (i32.add (global.get $log) (local.get $n)))
         (if (i32.ge_u (local.get $n) (i32.const 12)) (then (return_call $fin (local.get $n))))
         (switch $ck $sw (i32.add (local.get $n) (i32.const 1)) (local.get $peer))
         (local.set $peer) (local.set $n) (br $l))
       (unreachable))
     (func $fin (param i32) (result i32) (i32.add (local.get 0) (call $gen-sum)))
     (func $throwing-gen (param i32) (result i32)
       (drop (suspend $y (i32.const 3))) (drop (suspend $y (i32.const 4))) (throw $e (i32.const 5)))
     (func $gen-sum (result i32) (local $k (ref null $c1)) (local $acc i32)
       (local.set $k (cont.new $c1 (ref.func $throwing-gen)))
       (block $c (result i32)
         (try_table (catch $e $c)
           (loop $l
             (block $h (result i32 (ref $c1)) (drop (resume $c1 (on $y $h) (i32.const 0) (local.get $k))) (unreachable))
             (local.set $k) (local.set $acc (i32.add (local.get $acc))) (br $l)))
         (unreachable))
       (i32.add (local.get $acc)))
     (func $start (result i32) (call $pp (switch $ck $sw (i32.const 1) (cont.new $ck (ref.func $pp)))))
     (elem declare func $gen $genouter $pp $start $throwing-gen)
     (func (export "pingpong") (result i32)
       (global.set $log (i32.const 0))
       (i32.add (global.get $log) (resume $c0 (on $sw switch) (cont.new $c0 (ref.func $start)))))) */
static const uint8_t wasm_test_cont_fuel_sweep_sections[] = {
    0x01, 0x23, 0x07, 0x4E, 0x02, 0x60, 0x02, 0x7F, 0x63, 0x01, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x00,
    0x01, 0x7F, 0x5D, 0x02, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x04, 0x60, 0x01, 0x7F, 0x00, 0x60,
    0x00, 0x02, 0x7F, 0x64, 0x05, 0x03, 0x0A, 0x09, 0x04, 0x04, 0x02, 0x00, 0x04, 0x04, 0x02, 0x02,
    0x02, 0x0D, 0x07, 0x03, 0x00, 0x02, 0x00, 0x04, 0x00, 0x06, 0x06, 0x06, 0x01, 0x7F, 0x01, 0x41,
    0x00, 0x0B, 0x07, 0x19, 0x02, 0x0A, 0x67, 0x65, 0x6E, 0x65, 0x72, 0x61, 0x74, 0x6F, 0x72, 0x73,
    0x00, 0x02, 0x08, 0x70, 0x69, 0x6E, 0x67, 0x70, 0x6F, 0x6E, 0x67, 0x00, 0x08, 0x09, 0x09, 0x01,
    0x03, 0x00, 0x05, 0x00, 0x01, 0x03, 0x07, 0x05, 0x0A, 0xB1, 0x02, 0x09, 0x17, 0x00, 0x03, 0x40,
    0x20, 0x00, 0xE2, 0x01, 0x1A, 0x20, 0x00, 0x41, 0x01, 0x6B, 0x21, 0x00, 0x20, 0x00, 0x0D, 0x00,
    0x0B, 0x41, 0x00, 0x0B, 0x38, 0x02, 0x01, 0x63, 0x05, 0x01, 0x7F, 0xD2, 0x00, 0xE0, 0x05, 0xE1,
    0x05, 0x05, 0x21, 0x01, 0x41, 0x32, 0x21, 0x00, 0x03, 0x40, 0x02, 0x07, 0x20, 0x02, 0x20, 0x00,
    0x20, 0x01, 0xE3, 0x05, 0x01, 0x00, 0x01, 0x00, 0x6A, 0x0F, 0x0B, 0x21, 0x01, 0xE2, 0x01, 0x20,
    0x02, 0x6A, 0x21, 0x02, 0x41, 0x00, 0x21, 0x00, 0x0C, 0x00, 0x0B, 0x00, 0x0B, 0x36, 0x02, 0x01,
    0x63, 0x05, 0x02, 0x7F, 0xD2, 0x01, 0xE0, 0x05, 0x21, 0x00, 0x03, 0x40, 0x02, 0x07, 0x20, 0x01,
    0x20, 0x02, 0x20, 0x00, 0xE3, 0x05, 0x01, 0x00, 0x01, 0x00, 0x6A, 0x0F, 0x0B, 0x21, 0x00, 0x21,
    0x02, 0x20, 0x01, 0x20, 0x02, 0x6A, 0x21, 0x01, 0x20, 0x02, 0x41, 0x02, 0x6C, 0x21, 0x02, 0x0C,
    0x00, 0x0B, 0x00, 0x0B, 0x36, 0x02, 0x01, 0x7F, 0x01, 0x63, 0x01, 0x20, 0x00, 0x21, 0x02, 0x20,
    0x01, 0x21, 0x03, 0x03, 0x40, 0x23, 0x00, 0x20, 0x02, 0x6A, 0x24, 0x00, 0x20, 0x02, 0x41, 0x0C,
    0x4F, 0x04, 0x40, 0x20, 0x02, 0x12, 0x04, 0x0B, 0x20, 0x02, 0x41, 0x01, 0x6A, 0x20, 0x03, 0xE6,
    0x01, 0x00, 0x21, 0x03, 0x21, 0x02, 0x0C, 0x00, 0x0B, 0x00, 0x0B, 0x07, 0x00, 0x20, 0x00, 0x10,
    0x06, 0x6A, 0x0B, 0x10, 0x00, 0x41, 0x03, 0xE2, 0x01, 0x1A, 0x41, 0x04, 0xE2, 0x01, 0x1A, 0x41,
    0x05, 0x08, 0x02, 0x0B, 0x36, 0x02, 0x01, 0x63, 0x05, 0x01, 0x7F, 0xD2, 0x05, 0xE0, 0x05, 0x21,
    0x00, 0x02, 0x7F, 0x1F, 0x40, 0x01, 0x00, 0x02, 0x00, 0x03, 0x40, 0x02, 0x07, 0x41, 0x00, 0x20,
    0x00, 0xE3, 0x05, 0x01, 0x00, 0x01, 0x00, 0x1A, 0x00, 0x0B, 0x21, 0x00, 0x20, 0x01, 0x6A, 0x21,
    0x01, 0x0C, 0x00, 0x0B, 0x0B, 0x00, 0x0B, 0x20, 0x01, 0x6A, 0x0B, 0x0D, 0x00, 0x41, 0x01, 0xD2,
    0x03, 0xE0, 0x01, 0xE6, 0x01, 0x00, 0x10, 0x03, 0x0B, 0x12, 0x00, 0x41, 0x00, 0x24, 0x00, 0x23,
    0x00, 0xD2, 0x07, 0xE0, 0x03, 0xE3, 0x03, 0x01, 0x01, 0x00, 0x6A, 0x0B,
};

TEST(test_stack_switching_fuel_stops_anywhere) {
    static const struct {
        const char* name;
        int32_t expected;
    } calls[] = { { "generators", 3825 }, { "pingpong", 24 } };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    uint32_t i;
    uint64_t fuel;

    wasm_init(&rt);
    m = wasm_test_load_cont(&rt, wasm_test_cont_fuel_sweep_sections, sizeof(wasm_test_cont_fuel_sweep_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < COUNTOF(calls); i++) {
        WASM_CHECK_OK(t, wasm_call(m, calls[i].name, NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, calls[i].expected);
        for (fuel = 1; fuel < 60; fuel++) {
            uint32_t stops = 0;
            wasm_error_t err;

            wasm_runtime_set_fuel(&rt, fuel);
            err = wasm_call(m, calls[i].name, NULL, 0, &result, 1);
            while (err == WASM_ERR_OUT_OF_FUEL && stops < 100000u) {
                if (++stops % 3u == 0) wasm_gc_collect(&rt);
                wasm_runtime_set_fuel(&rt, fuel);
                err = wasm_runtime_resume(&rt, NULL, 0, &result, 1);
            }
            CHECK_MSG(t, err == WASM_OK && result.of.i32 == calls[i].expected,
                      "%s with fuel %u: err=%d result=%d after %u stops (%s)", calls[i].name, (unsigned)fuel,
                      (int)err, (int)result.of.i32, (unsigned)stops, rt.error_msg);
            if (err != WASM_OK) break;
        }
        wasm_runtime_disable_fuel(&rt);
    }
    wasm_gc_collect(&rt);
    CHECK(t, rt.num_cont_stacks == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module (type $f (func (result i32))) (type $c (cont $f))
     (func $bad (result i32) (i64.const 1)) (elem declare func $bad)
     (func (export "run") (result i32) (resume $c (cont.new $c (ref.func $bad))))) */
static const uint8_t wasm_test_cont_lazy_invalid_sections[] = {
    0x01, 0x07, 0x02, 0x60, 0x00, 0x01, 0x7F, 0x5D, 0x00, 0x03, 0x03, 0x02, 0x00, 0x00, 0x07, 0x07,
    0x01, 0x03, 0x72, 0x75, 0x6E, 0x00, 0x01, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x10,
    0x02, 0x04, 0x00, 0x42, 0x01, 0x0B, 0x09, 0x00, 0xD2, 0x00, 0xE0, 0x01, 0xE3, 0x01, 0x00, 0x0B,
};

TEST(test_stack_switching_lazy_validation_of_a_continuation) {
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t i;

    wasm_config_default(&cfg);
    cfg.lazy_validation = 1;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_cont(&rt, wasm_test_cont_lazy_invalid_sections, sizeof(wasm_test_cont_lazy_invalid_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    for (i = 0; i < 2; i++) {
        wasm_value_t result;
        wasm_error_t err = wasm_call(m, "run", NULL, 0, &result, 1);

        CHECK_MSG(t, err != WASM_OK && strstr(rt.error_msg, "validation error") != NULL, "err=%d (%s)", (int)err,
                  rt.error_msg);
        CHECK(t, rt.exec == &rt.main_stack);
    }
    wasm_gc_collect(&rt);
    CHECK(t, rt.num_cont_stacks == 0);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case exec_cont_cases[] = {
    { "stack switching: a generator suspends values to its consumer", test_stack_switching_generator },
    { "stack switching: resuming or binding a consumed continuation traps", test_stack_switching_one_shot },
    { "stack switching: null continuations and null functions trap", test_stack_switching_null_continuations },
    { "stack switching: a suspension no resume handles traps", test_stack_switching_unhandled_suspend },
    { "stack switching: suspensions cannot leave a host function call", test_stack_switching_host_barrier },
    { "stack switching: a suspension captures the stacks of nested resumes", test_stack_switching_nested_capture },
    { "stack switching: collections trace and move references on continuation stacks", test_stack_switching_gc_roots },
    { "stack switching: live continuation stacks are limited and unreachable ones collected", test_stack_switching_stack_limit },
    { "stack switching: a continuation gets a stack when it first runs, and dropped stacks are collected by size", test_stack_switching_stacks_are_allocated_on_first_run },
    { "stack switching: overflow traps, and fuel and yields inside a continuation suspend the call", test_stack_switching_stack_overflow_and_stops },
    { "stack switching: exceptions are caught inside a continuation and propagate to the resumer", test_stack_switching_exceptions },
    { "stack switching: freeing a module invalidates continuations that run its code", test_stack_switching_freed_module },
    { "stack switching: switch hands the resumer to another continuation", test_stack_switching_switch },
    { "stack switching: resume_throw raises at the suspension point", test_stack_switching_resume_throw },
    { "stack switching: fuel stops inside a continuation chain resume and cancel", test_stack_switching_fuel_in_chain },
    { "stack switching: yields and interrupts inside a continuation chain resume", test_stack_switching_yield_in_chain },
    { "stack switching: traps and exceptions leaving a continuation chain", test_stack_switching_chain_traps },
    { "stack switching: calls through linked imports suspend and trap once freed", test_stack_switching_linked_imports },
    { "stack switching: contrefs at the host boundary must be live continuations of the runtime", test_stack_switching_host_boundary },
    { "stack switching: continuations keep the types of a freed module", test_stack_switching_continuations_outlive_their_type_module },
    { "stack switching: a host function's reference arguments survive collections during its call", test_stack_switching_host_arguments_are_roots },
    { "stack switching: continuations start in, switch to, and tail-call host functions that may yield", test_stack_switching_host_function_targets },
    { "stack switching: fuel stops at every point of switching, generators, and exceptions resume", test_stack_switching_fuel_stops_anywhere },
    { "stack switching: a continuation's lazily validated function fails each time it starts", test_stack_switching_lazy_validation_of_a_continuation },
};
const test_suite exec_cont_suite = { "exec.cont", exec_cont_cases, COUNTOF(exec_cont_cases) };

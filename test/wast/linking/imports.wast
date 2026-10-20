(module $Q
  (type $pad (func (param f64)))
  (type $pad2 (struct))
  (type $f (func (param i32) (result i32)))
  (type $c (cont $f))
  (type $s (struct (field i32)))
  (type $base (sub (struct)))
  (type $derived (sub $base (struct (field i64))))
  (func $id (type $f) (local.get 0))
  (elem declare func $id)
  (global (export "gf") (mut (ref null $f)) (ref.null $f))
  (global (export "gf-imm") (ref null $f) (ref.func $id))
  (global (export "gc") (mut (ref null $c)) (ref.null $c))
  (global (export "gc-imm") (ref null $c) (ref.null $c))
  (global (export "gs") (mut (ref null $s)) (ref.null $s))
  (global (export "gs-imm") (ref $s) (struct.new $s (i32.const 7)))
  (global (export "gd") (mut (ref null $derived)) (ref.null $derived))
  (global (export "gd-imm") (ref null $derived) (ref.null $derived))
  (table (export "tf") 1 (ref null $f))
  (table (export "tc") 1 (ref null $c))
  (table (export "ts") 1 (ref null $s))
)
(register "Q")

(module
  (type $g (func (param i32) (result i32)))
  (type $k (cont $g))
  (type $t (struct (field i32)))
  (import "Q" "gf" (global (mut (ref null $g))))
  (import "Q" "gf-imm" (global $gf (ref null $g)))
  (import "Q" "gf-imm" (global funcref))
  (import "Q" "gc" (global (mut (ref null $k))))
  (import "Q" "gc-imm" (global (ref null $k)))
  (import "Q" "gc-imm" (global contref))
  (import "Q" "gs" (global (mut (ref null $t))))
  (import "Q" "gs-imm" (global (ref null $t)))
  (import "Q" "gs-imm" (global (ref $t)))
  (import "Q" "gs-imm" (global structref))
  (import "Q" "tf" (table 1 (ref null $g)))
  (import "Q" "tc" (table 1 (ref null $k)))
  (import "Q" "ts" (table 1 (ref null $t)))
  (func (export "call") (result i32) (call_ref $g (i32.const 5) (global.get $gf)))
)
(assert_return (invoke "call") (i32.const 5))

(module
  (type $base (sub (struct)))
  (type $derived (sub $base (struct (field i64))))
  (import "Q" "gd-imm" (global (ref null $base)))
  (import "Q" "gd-imm" (global (ref null $derived)))
  (import "Q" "gd" (global (mut (ref null $derived))))
)

(assert_unlinkable
  (module
    (type $g (func (param i64) (result i32)))
    (import "Q" "gf-imm" (global (ref null $g))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $g (func (param i64) (result i32)))
    (import "Q" "gf" (global (mut (ref null $g)))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $g (func (param i64) (result i32)))
    (type $k (cont $g))
    (import "Q" "gc-imm" (global (ref null $k))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $g (func (param i64) (result i32)))
    (type $k (cont $g))
    (import "Q" "gc" (global (mut (ref null $k)))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $t (struct (field i64)))
    (import "Q" "gs-imm" (global (ref null $t))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $t (struct (field i32)))
    (import "Q" "gs" (global (mut (ref $t)))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $base (sub (struct)))
    (type $derived (sub $base (struct (field i64))))
    (import "Q" "gd" (global (mut (ref null $base)))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $t (struct (field i64)))
    (import "Q" "ts" (table 1 (ref null $t))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $g (func (param i64) (result i32)))
    (import "Q" "tf" (table 1 (ref null $g))))
  "incompatible import type")
(assert_unlinkable
  (module
    (import "Q" "tf" (table 1 funcref)))
  "incompatible import type")

(module $R
  (type $s (struct (field i32)))
  (import "Q" "gs" (global $g (mut (ref null $s))))
  (import "Q" "ts" (table $t 1 (ref null $s)))
  (export "gs" (global $g))
  (export "ts" (table $t))
)
(register "R")
(module
  (type $pad (array i8))
  (type $t (struct (field i32)))
  (import "R" "gs" (global (mut (ref null $t))))
  (import "R" "ts" (table 1 (ref null $t))))
(assert_unlinkable
  (module
    (type $pad (array i8))
    (type $t (struct (field i8)))
    (import "R" "gs" (global (mut (ref null $t)))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $pad (array i8))
    (type $t (struct (field i8)))
    (import "R" "ts" (table 1 (ref null $t))))
  "incompatible import type")

(assert_invalid
  (module
    (import "missing" "g" (global i32))
    (func (result i32) (i64.const 0)))
  "type mismatch")
(assert_invalid
  (module
    (import "Q" "tf" (table 1 funcref))
    (func (result i32) (i64.const 0)))
  "type mismatch")
(assert_invalid
  (module
    (global (ref struct) (ref.null none))
    (func (result i32) (i64.const 0)))
  "type mismatch")

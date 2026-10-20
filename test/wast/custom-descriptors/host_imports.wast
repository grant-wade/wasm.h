(module $A
  (import "spectest" "print_i32" (func $p (param i32)))
  (export "p" (func $p)))
(register "A" $A)
(module $B
  (type $t (func (param i32)))
  (import "A" "p" (func $p (exact (type $t))))
  (elem declare func $p)
  (func (export "t") (result i32) (ref.test (ref (exact $t)) (ref.func $p))))
(assert_return (invoke $B "t") (i32.const 1))
(module $C
  (type $t (func (param i32)))
  (import "spectest" "print_i32" (func $p (exact (type $t))))
  (elem declare func $p)
  (func (export "t") (result i32) (ref.test (ref (exact $t)) (ref.func $p))))
(assert_return (invoke $C "t") (i32.const 1))
(assert_unlinkable
  (module
    (type $t (sub (func (param i32))))
    (import "spectest" "print_i32" (func $p (exact (type $t)))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $t (sub (func (param i32))))
    (import "spectest" "print_i32" (func $p (type $t))))
  "incompatible import type")

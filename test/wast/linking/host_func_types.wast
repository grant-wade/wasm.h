(module
  (type $t (func (param i32)))
  (import "spectest" "print_i32" (func (type $t)))
  (import "spectest" "print_i32" (func (param i32)))
  (import "spectest" "print" (func)))
(assert_unlinkable
  (module
    (type $t (sub (func (param i32))))
    (import "spectest" "print_i32" (func (type $t))))
  "incompatible import type")
(assert_unlinkable
  (module
    (rec (type $t (func (param i32))) (type (struct)))
    (import "spectest" "print_i32" (func (type $t))))
  "incompatible import type")
(assert_unlinkable
  (module
    (type $t (func (param i64)))
    (import "spectest" "print_i32" (func (type $t))))
  "incompatible import type")

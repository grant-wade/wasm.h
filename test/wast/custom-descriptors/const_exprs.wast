(assert_invalid
  (module
    (type $f (func))
    (import "spectest" "print" (func $p (type $f)))
    (global (ref (exact $f)) (ref.func $p)))
  "type mismatch")
(assert_invalid
  (module
    (type $f (func))
    (import "spectest" "print" (func $p (type $f)))
    (table 1 (ref null (exact $f)))
    (elem (table 0) (i32.const 0) (ref (exact $f)) (ref.func $p)))
  "type mismatch")
(assert_invalid
  (module
    (rec (type $A (descriptor $D) (struct)) (type $D (describes $A) (struct)))
    (global $d (ref $D) (struct.new $D))
    (global (ref $A) (struct.new_default_desc $A (global.get $d))))
  "type mismatch")
(assert_invalid
  (module
    (rec (type $A (descriptor $D) (struct)) (type $D (describes $A) (struct)))
    (global $d (ref (exact $D)) (struct.new $D))
    (global $a (ref (exact $A)) (struct.new_default_desc $A (global.get $d)))
    (global (ref $D) (ref.get_desc $A (global.get $a))))
  "constant expression required")
(assert_invalid
  (module
    (type $f (func))
    (import "spectest" "print" (func $p (type $f)))
    (table 1 (ref null (exact $f)) (ref.func $p)))
  "type mismatch")

(module
  (type $f (func))
  (import "spectest" "print" (func $p (exact (type $f))))
  (func $g (type $f))
  (rec (type $A (descriptor $D) (struct (field i32))) (type $D (describes $A) (struct)))
  (global $d (ref (exact $D)) (struct.new $D))
  (global $a (ref (exact $A)) (struct.new_desc $A (i32.const 7) (global.get $d)))
  (global (ref (exact $f)) (ref.func $p))
  (global (ref (exact $f)) (ref.func $g))
  (table 1 (ref null (exact $f)) (ref.func $g))
  (elem (table 0) (i32.const 0) (ref (exact $f)) (ref.func $p))
  (func (export "field") (result i32) (struct.get $A 0 (global.get $a)))
  (func (export "same-desc") (result i32)
    (ref.eq (ref.get_desc $A (global.get $a)) (global.get $d))))
(assert_return (invoke "field") (i32.const 7))
(assert_return (invoke "same-desc") (i32.const 1))

(assert_invalid
  (module
    (rec (type $A (descriptor $D) (struct)) (type $D (describes $A) (struct)))
    (import "M" "d" (global $d (ref $D)))
    (global (ref $A) (struct.new_default_desc $A (global.get $d))))
  "type mismatch")
(assert_trap
  (module
    (rec (type $A (descriptor $D) (struct)) (type $D (describes $A) (struct)))
    (global (ref $A) (struct.new_default_desc $A (ref.null none))))
  "null descriptor reference")
(assert_invalid
  (module
    (rec (type $A (descriptor $D) (struct)) (type $D (describes $A) (struct)))
    (global (ref $A) (struct.new_default_desc $A (ref.null none)))
    (func (result i32) (i64.const 0)))
  "type mismatch")

(module (rec (type $c (cont $f)) (type $f (func))))
(assert_invalid (module (type $c (cont 1)) (type $f (func))) "unknown type")
(assert_invalid (module (type $f (func)) (type $c (cont $f)) (type $d (cont $c))) "non-function type")

(module
  (type $f1 (sub (func (result anyref))))
  (type $f2 (sub $f1 (func (result eqref))))
  (type $c1 (sub (cont $f1)))
  (type $c2 (sub $c1 (cont $f2)))
  (func (param (ref $c2)) (result (ref $c1)) (local.get 0))
  (func (param (ref $c2)) (result contref) (local.get 0))
  (func (param nullcontref) (result (ref null $c2)) (local.get 0))
)
(assert_invalid
  (module
    (type $f1 (sub (func (result anyref))))
    (type $f2 (sub (func (result eqref))))
    (type $c1 (sub (cont $f1)))
    (type $c2 (sub $c1 (cont $f2))))
  "sub type")
(assert_invalid
  (module
    (type $f1 (sub (func (result anyref))))
    (type $f2 (sub $f1 (func (result eqref))))
    (type $c1 (cont $f1))
    (type $c2 (cont $f2))
    (func (param (ref $c2)) (result (ref $c1)) (local.get 0)))
  "type mismatch")
(assert_invalid
  (module
    (type $f (sub (func)))
    (type $c (sub $f (cont $f))))
  "sub type")
(assert_invalid (module (func (param contref) (result anyref) (local.get 0))) "type mismatch")
(assert_invalid (module (func (param contref) (result funcref) (local.get 0))) "type mismatch")
(assert_invalid (module (func (param nullcontref) (result nullfuncref) (local.get 0))) "type mismatch")
(assert_invalid (module (type $f (func)) (func (param (ref $f)) (result contref) (local.get 0))) "type mismatch")
(assert_invalid (module (type $f (func)) (type $c (cont $f)) (func (param (ref $c)) (result funcref) (local.get 0))) "type mismatch")

(module
  (type $f (func))
  (type $c1 (cont $f))
  (type $c2 (cont $f))
  (type $g (func))
  (type $c3 (cont $g))
  (func (param (ref $c1)) (result (ref $c2)) (local.get 0))
  (func (param (ref $c1)) (result (ref $c3)) (local.get 0))
)
(module
  (rec (type $f1 (func (param (ref $c1)))) (type $c1 (cont $f1)))
  (rec (type $f2 (func (param (ref $c2)))) (type $c2 (cont $f2)))
  (func (param (ref $c1)) (result (ref $c2)) (local.get 0))
)
(assert_invalid (module (func (param anyref) (result i32) (ref.test nullcontref (local.get 0)))) "invalid cast")
(module
  (type $f (func))
  (type $c (cont $f))
  (table $t 2 contref)
  (table $t2 2 (ref null $c))
  (global $g (mut contref) (ref.null cont))
  (global $h (mut (ref null $c)) (ref.null $c))
  (elem (table $t2) (i32.const 0) (ref null $c) (ref.null $c))
  (func (export "f") (result i32)
    (local $k (ref null $c))
    (local.set $k (cont.new $c (ref.func $nop)))
    (table.set $t2 (i32.const 1) (local.get $k))
    (table.set $t (i32.const 0) (table.get $t2 (i32.const 1)))
    (global.set $g (table.get $t (i32.const 0)))
    (global.set $h (ref.null $c))
    (drop (table.grow $t (ref.null cont) (i32.const 1)))
    (table.copy $t $t2 (i32.const 0) (i32.const 0) (i32.const 2))
    (resume $c (select (result (ref null $c)) (local.get $k) (ref.null $c) (i32.const 1)))
    (i32.add (ref.is_null (global.get $g)) (ref.is_null (table.get $t (i32.const 1)))))
  (func $nop)
  (elem declare func $nop)
)
(assert_return (invoke "f") (i32.const 0))

(module
  (rec
    (type $c1 (sub (cont $f1)))
    (type $c2 (sub $c1 (cont $f2)))
    (type $f1 (sub (func (result anyref))))
    (type $f2 (sub $f1 (func (result eqref)))))
  (func (param (ref $c2)) (result (ref $c1)) (local.get 0)))
(assert_invalid
  (module
    (rec
      (type $c1 (sub (cont $f1)))
      (type $c2 (sub $c1 (cont $f2)))
      (type $f1 (sub (func (result anyref))))
      (type $f2 (sub (func (result eqref))))))
  "sub type")
(module
  (rec (type $c (cont $f)) (type $f (func (param (ref null $c)))))
  (rec (type $d (cont $g)) (type $g (func (param (ref null $d)))))
  (func (param (ref $c)) (result (ref $d)) (local.get 0)))
(assert_invalid
  (module
    (rec (type $c (cont $f)) (type $f (func (param (ref null $c)))))
    (rec (type $g (func (param (ref null $d)))) (type $d (cont $g)))
    (func (param (ref $c)) (result (ref $d)) (local.get 0)))
  "type mismatch")

(assert_invalid
  (module (type $f (func)) (type $c1 (cont $f)) (type $c2 (sub $c1 (cont $f))))
  "final")
(module (type $f (func)) (type $c1 (sub (cont $f))) (type $c2 (sub final $c1 (cont $f))))

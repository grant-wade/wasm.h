(assert_invalid
  (module
    (type $a (array (ref any)))
    (func (drop (array.new_default $a (i32.const 0)))))
  "field type is not defaultable")

(assert_invalid
  (module
    (type $f (func))
    (func $g (type $f))
    (table 1 (ref null $f))
    (elem (table 0) (i32.const 0) func $g))
  "type mismatch")
(assert_invalid
  (module
    (type $f (func))
    (func $g (type $f))
    (table 1 (ref null $f))
    (elem $e func $g)
    (func (table.init 0 $e (i32.const 0) (i32.const 0) (i32.const 1))))
  "type mismatch")

(module
  (type $f (func))
  (func $g (type $f))
  (table $t 1 (ref func) (ref.func $g))
  (elem (table $t) (i32.const 0) func $g)
  (elem $e func $g)
  (func (export "init") (result i32)
    (table.init $t $e (i32.const 0) (i32.const 0) (i32.const 1))
    (ref.is_null (table.get $t (i32.const 0)))))
(assert_return (invoke "init") (i32.const 0))

(module
  (type $s1 (struct))
  (type $s2 (struct))
  (type $a (sub (struct (field (mut (ref $s1))))))
  (type $b (sub $a (struct (field (mut (ref $s2))))))
  (type $c (sub (array (mut (ref null $s1)))))
  (type $d (sub $c (array (mut (ref null $s2))))))
(assert_invalid
  (module
    (type $s1 (struct))
    (type $s2 (struct (field i32)))
    (type $a (sub (struct (field (mut (ref $s1))))))
    (type $b (sub $a (struct (field (mut (ref $s2)))))))
  "is not a valid subtype")

(assert_invalid
  (module
    (type $super (sub (struct)))
    (type $sub (sub $super (struct)))
    (global $g (ref $super) (struct.new $sub))
    (global (ref $sub) (global.get $g)))
  "type mismatch")
(assert_invalid
  (module
    (type $s (struct (field (ref any))))
    (global (ref $s) (struct.new_default $s)))
  "field type is not defaultable")
(assert_invalid
  (module
    (type $a (array (ref any)))
    (global (ref $a) (array.new_default $a (i32.const 0))))
  "field type is not defaultable")
(assert_invalid
  (module
    (global (ref any) (any.convert_extern (ref.null extern))))
  "type mismatch")

(module
  (type $super (sub (struct (field i32))))
  (type $sub (sub $super (struct (field i32))))
  (type $a (array i8))
  (global $g (ref $super) (struct.new $sub (i32.const 5)))
  (global $h (ref null $super) (global.get $g))
  (global $arr (ref $a) (array.new_fixed $a 2 (i32.const 1) (i32.const 300)))
  (global $e (ref extern) (extern.convert_any (ref.i31 (i32.const 9))))
  (global $i (ref any) (any.convert_extern (global.get $e)))
  (func (export "field") (result i32) (struct.get $super 0 (global.get $h)))
  (func (export "elem") (result i32) (array.get_u $a (global.get $arr) (i32.const 1)))
  (func (export "i31") (result i32) (i31.get_s (ref.cast (ref i31) (global.get $i)))))
(assert_return (invoke "field") (i32.const 5))
(assert_return (invoke "elem") (i32.const 44))
(assert_return (invoke "i31") (i32.const 9))

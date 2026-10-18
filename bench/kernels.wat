(module
  (type $binop (func (param i32 i32) (result i32)))
  (memory (export "memory") 1)
  (table 4 funcref)
  (elem (i32.const 0) func $add $sub $mul $xor)
  (func $add (type $binop) (i32.add (local.get 0) (local.get 1)))
  (func $sub (type $binop) (i32.sub (local.get 0) (local.get 1)))
  (func $mul (type $binop) (i32.mul (local.get 0) (local.get 1)))
  (func $xor (type $binop) (i32.xor (local.get 0) (local.get 1)))

  (func (export "loop") (param $n i32) (result i32) (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (i32.add (local.get $acc) (i32.xor (local.get $i) (i32.const 0x55))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "mem") (param $n i32) (result i32) (local $i i32) (local $acc i32) (local $p i32)
    (loop $l
      (local.set $p (i32.and (i32.shl (local.get $i) (i32.const 2)) (i32.const 0xFFFC)))
      (i32.store (local.get $p) (i32.add (i32.load (local.get $p)) (local.get $i)))
      (local.set $acc (i32.add (local.get $acc) (i32.load8_u (local.get $p))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func $fib (param $n i32) (result i32)
    (if (result i32) (i32.lt_u (local.get $n) (i32.const 2))
      (then (local.get $n))
      (else (i32.add (call $fib (i32.sub (local.get $n) (i32.const 1)))
                     (call $fib (i32.sub (local.get $n) (i32.const 2)))))))
  (func (export "calls") (param $n i32) (result i32) (call $fib (local.get $n)))

  (func (export "br_table") (param $n i32) (result i32) (local $i i32) (local $acc i32)
    (loop $l
      (block $d (block $c (block $b (block $a
        (br_table $a $b $c $d (i32.and (local.get $i) (i32.const 3))))
        (local.set $acc (i32.add (local.get $acc) (i32.const 1))) (br $d))
        (local.set $acc (i32.add (local.get $acc) (i32.const 3))) (br $d))
        (local.set $acc (i32.add (local.get $acc) (i32.const 5))) (br $d))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "call_indirect") (param $n i32) (result i32) (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (call_indirect (type $binop) (local.get $acc) (local.get $i)
                        (i32.and (local.get $i) (i32.const 3))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "float") (param $n i32) (result f64) (local $i i32) (local $x f64)
    (local.set $x (f64.const 1))
    (loop $l
      (local.set $x (f64.add (f64.mul (local.get $x) (f64.const 1.0000001)) (f64.const 0.5)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $x)))

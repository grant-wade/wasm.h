(module
  (type $binop (func (param i32 i32) (result i32)))
  (type $quad (struct (field (mut i32)) (field (mut i32)) (field (mut i32)) (field (mut i32))))
  (type $pair (struct (field i32) (field i32)))
  (type $cons (struct (field i32) (field (ref null $cons))))
  (type $f64s (array (mut f64)))
  (type $bytes (array (mut i8)))
  (type $shape (sub (struct (field i32))))
  (type $circle (sub final $shape (struct (field i32) (field i32))))
  (type $rect (sub final $shape (struct (field i32) (field i32) (field i32))))
  (type $tri (sub final $shape (struct (field i32) (field i64))))
  (type $shapes (array (ref $shape)))
  (type $unit (func))
  (type $unit_k (cont $unit))
  (type $gen (func (param i32)))
  (type $gen_k (cont $gen))
  (import "env" "inc" (func $host_inc (param i32) (result i32)))
  (memory (export "memory") 160)
  (table 4 funcref)
  (elem (i32.const 0) func $add $sub $mul $xor)
  (elem declare func $leaf $producer)
  (tag $exn (param i32))
  (tag $yield (param i32))
  (global $live (mut (ref null $cons)) (ref.null $cons))
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
    (local.get $x))

  (func (export "call") (param $n i32) (result i32) (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (call $add (local.get $acc) (local.get $i)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "call_ref") (param $n i32) (result i32) (local $i i32) (local $acc i32) (local $f (ref null $binop))
    (local.set $f (ref.func $add))
    (loop $l
      (local.set $acc (call_ref $binop (local.get $acc) (local.get $i) (local.get $f)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func $countdown (param $n i32) (param $acc i32) (result i32)
    (if (result i32) (i32.eqz (local.get $n))
      (then (local.get $acc))
      (else (return_call $countdown (i32.sub (local.get $n) (i32.const 1))
                                    (i32.add (local.get $acc) (local.get $n))))))
  (func (export "return_call") (param $n i32) (result i32) (call $countdown (local.get $n) (i32.const 0)))

  (func (export "host_call") (param $n i32) (result i32) (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (call $host_inc (local.get $acc)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "struct_rw") (param $n i32) (result i32) (local $i i32) (local $acc i32) (local $s (ref null $quad))
    (local.set $s (struct.new_default $quad))
    (loop $l
      (struct.set $quad 0 (local.get $s) (local.get $i))
      (local.set $acc (i32.add (local.get $acc) (struct.get $quad 3 (local.get $s))))
      (struct.set $quad 3 (local.get $s) (i32.xor (struct.get $quad 0 (local.get $s)) (i32.const 0x55)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "struct_alloc") (param $n i32) (result i32) (local $i i32) (local $acc i32) (local $p (ref null $pair))
    (loop $l
      (local.set $p (struct.new $pair (local.get $i) (local.get $acc)))
      (local.set $acc (i32.add (struct.get $pair 0 (local.get $p)) (struct.get $pair 1 (local.get $p))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "list") (param $n i32) (result i32) (local $i i32) (local $acc i32) (local $l (ref null $cons))
    (loop $build
      (local.set $l (struct.new $cons (local.get $i) (local.get $l)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $build (i32.lt_u (local.get $i) (local.get $n))))
    (block $done
      (loop $walk
        (br_if $done (ref.is_null (local.get $l)))
        (local.set $acc (i32.add (local.get $acc) (struct.get $cons 0 (local.get $l))))
        (local.set $l (struct.get $cons 1 (local.get $l)))
        (br $walk)))
    (local.get $acc))

  (func (export "mem_f64") (param $n i32) (result f64) (local $i i32) (local $x f64)
    (loop $fill
      (f64.store (i32.shl (local.get $i) (i32.const 3)) (f64.convert_i32_u (local.get $i)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $fill (i32.lt_u (local.get $i) (local.get $n))))
    (local.set $i (i32.const 0))
    (loop $sum
      (local.set $x (f64.add (local.get $x) (f64.load (i32.shl (local.get $i) (i32.const 3)))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $sum (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $x))

  (func (export "array_f64") (param $n i32) (result f64) (local $i i32) (local $x f64) (local $a (ref null $f64s))
    (local.set $a (array.new_default $f64s (local.get $n)))
    (loop $fill
      (array.set $f64s (local.get $a) (local.get $i) (f64.convert_i32_u (local.get $i)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $fill (i32.lt_u (local.get $i) (local.get $n))))
    (local.set $i (i32.const 0))
    (loop $sum
      (local.set $x (f64.add (local.get $x) (array.get $f64s (local.get $a) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $sum (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $x))

  (func (export "array_i8") (param $n i32) (result i32) (local $i i32) (local $acc i32) (local $a (ref null $bytes))
    (local.set $a (array.new_default $bytes (local.get $n)))
    (loop $fill
      (array.set $bytes (local.get $a) (local.get $i) (local.get $i))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $fill (i32.lt_u (local.get $i) (local.get $n))))
    (local.set $i (i32.const 0))
    (loop $sum
      (local.set $acc (i32.add (local.get $acc) (array.get_u $bytes (local.get $a) (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $sum (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "cast") (param $n i32) (result i32) (local $i i32) (local $acc i32) (local $shapes (ref null $shapes))
    (local.set $shapes (array.new_fixed $shapes 3
      (struct.new $circle (i32.const 1) (i32.const 2))
      (struct.new $rect (i32.const 3) (i32.const 4) (i32.const 5))
      (struct.new $tri (i32.const 6) (i64.const 7))))
    (loop $l
      block $next
        block $is_rect (result (ref $rect))
          block $is_circle (result (ref $circle))
            (array.get $shapes (local.get $shapes) (i32.rem_u (local.get $i) (i32.const 3)))
            br_on_cast $is_circle (ref $shape) (ref $circle)
            br_on_cast $is_rect (ref $shape) (ref $rect)
            ref.cast (ref $tri)
            struct.get $tri 1
            i32.wrap_i64
            local.get $acc
            i32.add
            local.set $acc
            br $next
          end
          struct.get $circle 1
          local.get $acc
          i32.add
          local.set $acc
          br $next
        end
        struct.get $rect 2
        local.get $acc
        i32.add
        local.set $acc
      end
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func (export "i31") (param $n i32) (result i32) (local $i i32) (local $acc i32) (local $r (ref null i31))
    (loop $l
      (local.set $r (ref.i31 (local.get $i)))
      (local.set $acc (i32.add (local.get $acc) (i31.get_s (local.get $r))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func $thrower (param $v i32) (throw $exn (local.get $v)))
  (func (export "throw") (param $n i32) (result i32) (local $i i32) (local $acc i32)
    (loop $l
      (local.set $acc (i32.add (local.get $acc)
        (block $caught (result i32)
          (try_table (catch $exn $caught) (call $thrower (local.get $i)))
          (i32.const 0))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $acc))

  (func $producer (param $n i32) (local $i i32)
    (loop $l
      (suspend $yield (local.get $i))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n)))))
  (func (export "generator") (param $n i32) (result i32) (local $acc i32) (local $k (ref null $unit_k))
    block $done
      block $on_yield (result i32 (ref $unit_k))
        local.get $n
        (cont.new $gen_k (ref.func $producer))
        resume $gen_k (on $yield $on_yield)
        br $done
      end
      loop $next (param i32 (ref $unit_k))
        local.set $k
        local.get $acc
        i32.add
        local.set $acc
        block $on_yield (result i32 (ref $unit_k))
          local.get $k
          resume $unit_k (on $yield $on_yield)
          br $done
        end
        br $next
      end
    end
    (local.get $acc))

  (func (export "live_heap") (param $n i32) (result i32) (local $i i32) (local $l (ref null $cons))
    (block $done
      (loop $build
        (br_if $done (i32.ge_u (local.get $i) (local.get $n)))
        (local.set $l (struct.new $cons (local.get $i) (local.get $l)))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $build)))
    (global.set $live (local.get $l))
    (local.get $i))

  (func $leaf)
  (func (export "cont_new") (param $n i32) (result i32) (local $i i32)
    (loop $l
      (resume $unit_k (cont.new $unit_k (ref.func $leaf)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (local.get $i))

  (func (export "simd") (param $n i32) (result f64) (local $i i32) (local $x v128)
    (local.set $x (f32x4.splat (f32.const 1)))
    (loop $l
      (local.set $x (f32x4.add (f32x4.mul (local.get $x) (v128.const f32x4 1.0000001 1.0000001 1.0000001 1.0000001))
                               (v128.const f32x4 0.5 0.5 0.5 0.5)))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (f64.promote_f32 (f32x4.extract_lane 0 (local.get $x))))

  (func (export "simd_mem") (param $n i32) (result i32) (local $i i32) (local $x v128)
    (loop $l
      (local.set $x (i32x4.add (local.get $x)
        (v128.load (i32.and (i32.shl (local.get $i) (i32.const 4)) (i32.const 0xFFFF0)))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
    (i32x4.extract_lane 0 (local.get $x))))

(module
  (memory (export "memory") 1 2)
  (data (i32.const 16) "corpus")
  (func (export "fill") (param i32 i32)
    (memory.fill (local.get 0) (i32.const 170) (local.get 1))
    (memory.copy (i32.const 100) (i32.const 16) (i32.const 6)))
  (func (export "sum") (result i32) (local $i i32) (local $s i32)
    (loop $l
      (local.set $s (i32.add (local.get $s) (i32.load8_u (local.get $i))))
      (local.set $i (i32.add (local.get $i) (i32.const 1)))
      (br_if $l (i32.lt_u (local.get $i) (i32.const 4096))))
    (local.get $s))
  (func (export "oob") (result i32) (i32.load (i32.const 0x20000))))

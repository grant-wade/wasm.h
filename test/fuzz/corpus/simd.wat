(module
  (memory 1)
  (func (export "lanes") (result i32)
    (v128.store (i32.const 0)
      (i32x4.add (v128.const i32x4 1 2 3 4) (i32x4.splat (i32.const 10))))
    (i32x4.extract_lane 3 (v128.load (i32.const 0))))
  (func (export "shuffle") (result i64)
    (i64x2.extract_lane 1
      (i8x16.shuffle 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
        (v128.const i64x2 1 2) (v128.const i64x2 3 4))))
  (func (export "float") (result f32)
    (f32x4.extract_lane 0 (f32x4.sqrt (v128.const f32x4 4 9 16 25)))))

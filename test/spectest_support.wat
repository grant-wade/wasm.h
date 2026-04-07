(module
  (table (export "table") 10 20 funcref)
  (table (export "table64") i64 10 20 funcref)
  (memory (export "memory") 1 2)
  (global (export "global_i32") i32 (i32.const 666))
  (global (export "global_i64") i64 (i64.const 666))
  (global (export "global_f32") f32 (f32.const 666.6))
  (global (export "global_f64") f64 (f64.const 666.6))
)
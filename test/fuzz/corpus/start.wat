(module
  (import "env" "log" (func $log (param i32)))
  (memory 1)
  (global $ready (mut i32) (i32.const 0))
  (func $init (call $log (i32.const 1)) (global.set $ready (i32.const 1)))
  (start $init)
  (func (export "ready") (result i32) (global.get $ready)))

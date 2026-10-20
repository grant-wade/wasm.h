#ifndef WASM_TEST_SUITES_H
#define WASM_TEST_SUITES_H

#include "test.h"

extern const test_suite call_suite;
extern const test_suite decode_suite;
extern const test_suite gc_suite;
extern const test_suite init_expr_suite;
extern const test_suite inspect_suite;
extern const test_suite link_suite;
extern const test_suite load_suite;
extern const test_suite memory_suite;
extern const test_suite module_suite;
extern const test_suite reader_suite;
extern const test_suite runtime_suite;
extern const test_suite values_suite;
extern const test_suite exec_bulk_suite;
extern const test_suite exec_calls_suite;
extern const test_suite exec_cont_suite;
extern const test_suite exec_exceptions_suite;
extern const test_suite exec_frames_suite;
extern const test_suite exec_gc_suite;
extern const test_suite exec_interpreter_suite;
extern const test_suite exec_simd_suite;
extern const test_suite exec_stack_suite;
extern const test_suite exec_translate_suite;
extern const test_suite platform_emscripten_suite;
extern const test_suite platform_threads_suite;
extern const test_suite platform_wasi_bind_suite;
extern const test_suite platform_wasi_io_suite;
extern const test_suite platform_wasi_path_suite;
extern const test_suite platform_wasi_state_suite;
extern const test_suite simd_lanes_suite;
extern const test_suite types_canonical_suite;
extern const test_suite types_cross_module_suite;
extern const test_suite types_decode_suite;
extern const test_suite types_references_suite;
extern const test_suite validate_cont_suite;
extern const test_suite validate_function_suite;
extern const test_suite validate_gc_suite;
extern const test_suite validate_module_suite;
extern const test_suite validate_stack_suite;
#endif

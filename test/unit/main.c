#include "support.h"

static const test_suite* const suites[] = {
    &call_suite,
    &decode_suite,
    &gc_suite,
    &init_expr_suite,
    &inspect_suite,
    &link_suite,
    &load_suite,
    &memory_suite,
    &module_suite,
    &reader_suite,
    &runtime_suite,
    &values_suite,
    &exec_bulk_suite,
    &exec_calls_suite,
    &exec_cont_suite,
    &exec_exceptions_suite,
    &exec_frames_suite,
    &exec_gc_suite,
    &exec_interpreter_suite,
    &exec_simd_suite,
    &exec_stack_suite,
    &exec_translate_suite,
    &platform_emscripten_suite,
    &platform_threads_suite,
    &platform_wasi_bind_suite,
    &platform_wasi_io_suite,
    &platform_wasi_path_suite,
    &platform_wasi_state_suite,
    &simd_lanes_suite,
    &types_canonical_suite,
    &types_cross_module_suite,
    &types_decode_suite,
    &types_references_suite,
    &validate_cont_suite,
    &validate_function_suite,
    &validate_gc_suite,
    &validate_stack_suite,
};

static const test_suite* find_suite(const char* name) {
    size_t i;

    for (i = 0; i < COUNTOF(suites); i++) {
        if (strcmp(suites[i]->name, name) == 0) return suites[i];
    }
    return NULL;
}

/* Runs the suites named by the arguments, or every suite when there are none. */
int main(int argc, char** argv) {
    int failed = 0;
    int i;

    if (argc < 2) {
        size_t k;

        for (k = 0; k < COUNTOF(suites); k++) failed |= test_run(suites[k], wasm_test_verify_case_cleanup);
        return failed;
    }
    for (i = 1; i < argc; i++) {
        const test_suite* suite = find_suite(argv[i]);

        if (!suite) {
            fprintf(stderr, "unknown suite %s\n", argv[i]);
            return 2;
        }
        failed |= test_run(suite, wasm_test_verify_case_cleanup);
    }
    return failed;
}

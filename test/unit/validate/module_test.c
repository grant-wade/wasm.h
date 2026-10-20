#include "../support.h"

/* (type $x (struct (field i64))) (type $s (struct (field i32))) (table (export "t") 2 (ref null $s)) */
static const uint8_t wasm_test_validate_c1_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x5F, 0x01, 0x7E, 0x00, 0x5F,
    0x01, 0x7F, 0x00, 0x04, 0x05, 0x01, 0x63, 0x01, 0x00, 0x02, 0x07, 0x05, 0x01, 0x01, 0x74, 0x01,
    0x00,
};

/* (type $s (struct (field i32))) (import "c1" "t" (table 2 (ref null $s)))
   (elem (table 0) (i32.const 1) (ref null $s) (struct.new $s (i32.const 9))) */
static const uint8_t wasm_test_validate_elem_ok_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x5F, 0x01, 0x7F, 0x00, 0x02,
    0x0B, 0x01, 0x02, 0x63, 0x31, 0x01, 0x74, 0x01, 0x63, 0x00, 0x00, 0x02, 0x09, 0x0F, 0x01, 0x06,
    0x00, 0x41, 0x01, 0x0B, 0x63, 0x00, 0x01, 0x41, 0x09, 0xFB, 0x00, 0x00, 0x0B,
};

/* (type $s (struct (field i32))) (type $x (struct (field i64))) (import "c1" "t" (table 2 (ref null $s)))
   (elem (table 0) (i32.const 0) (ref null $x) (ref.null $x)) */
static const uint8_t wasm_test_validate_elem_bad_bytes[] = {
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x09, 0x02, 0x5F, 0x01, 0x7F, 0x00, 0x5F,
    0x01, 0x7E, 0x00, 0x02, 0x0B, 0x01, 0x02, 0x63, 0x31, 0x01, 0x74, 0x01, 0x63, 0x00, 0x00, 0x02,
    0x09, 0x0C, 0x01, 0x06, 0x00, 0x41, 0x00, 0x0B, 0x63, 0x01, 0x01, 0xD0, 0x01, 0x0B,
};

static void wasm_test_validate_imported_table_elems(test_ctx* t, wasm_runtime_t* rt) {
    wasm_module_t* compiled;
    wasm_module_t* m;

    compiled = wasm_compile(rt, wasm_test_validate_elem_ok_bytes, sizeof(wasm_test_validate_elem_ok_bytes));
    CHECK_MSG(t, compiled != NULL, "%s", rt->error_msg);
    if (compiled) {
        m = wasm_instantiate(rt, compiled, NULL);
        CHECK_MSG(t, m != NULL, "%s", rt->error_msg);
        if (m) wasm_module_free(m);
        wasm_module_free(compiled);
    }
    m = wasm_load(rt, wasm_test_validate_elem_ok_bytes, sizeof(wasm_test_validate_elem_ok_bytes));
    CHECK_MSG(t, m != NULL, "%s", rt->error_msg);
    if (m) wasm_module_free(m);

    compiled = wasm_compile(rt, wasm_test_validate_elem_bad_bytes, sizeof(wasm_test_validate_elem_bad_bytes));
    CHECK_MSG(t, compiled == NULL, "%s", "expected a mismatched element segment to be invalid");
    if (compiled) wasm_module_free(compiled);
    m = wasm_load(rt, wasm_test_validate_elem_bad_bytes, sizeof(wasm_test_validate_elem_bad_bytes));
    CHECK_MSG(t, m == NULL, "%s", "expected a mismatched element segment into an imported table to be invalid");
    CHECK_MSG(t, strstr(rt->error_msg, "element segment 0 type mismatch with table 0") != NULL, "%s", rt->error_msg);
    if (m) wasm_module_free(m);
}

TEST(test_validate_elem_segments_into_imported_tables) {
    wasm_runtime_t rt;
    wasm_module_t* c1;

    wasm_init(&rt);
    c1 = wasm_load(&rt, wasm_test_validate_c1_bytes, sizeof(wasm_test_validate_c1_bytes));
    CHECK_MSG(t, c1 != NULL, "%s", rt.error_msg);
    if (c1) {
        WASM_CHECK_OK(t, wasm_register_module(&rt, "c1", c1));
        wasm_test_validate_imported_table_elems(t, &rt);
        wasm_module_free(c1);
    }
    wasm_destroy(&rt);
}

static const test_case validate_module_cases[] = {
    { "gc: active element segments into imported tables use the importer's type indices",
      test_validate_elem_segments_into_imported_tables },
};
const test_suite validate_module_suite = { "validate.module", validate_module_cases, COUNTOF(validate_module_cases) };

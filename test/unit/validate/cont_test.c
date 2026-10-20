#include "../support.h"

/* (module (type $f (func)) (type $c (cont $f)) (type $fk (func (param i32))) (type $k (cont $fk))
     (tag $yield (result i32)) (func $g) (elem declare func $g)
     (func (export "run") (result i32)
       (block $h (result (ref $k))
         (resume $c (on $yield $h) (cont.new $c (ref.func $g)))
         (return (i32.const 0)))
       drop (i32.const 1))) */
static const uint8_t wasm_test_stack_switching_sections[] = {
    0x01, 0x10, 0x05, 0x60, 0x00, 0x00, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x5D, 0x02, 0x60, 0x00,
    0x01, 0x7F, 0x03, 0x03, 0x02, 0x00, 0x04, 0x0D, 0x03, 0x01, 0x00, 0x04, 0x07, 0x07, 0x01, 0x03,
    0x72, 0x75, 0x6E, 0x00, 0x01, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x1B, 0x02, 0x02,
    0x00, 0x0B, 0x16, 0x00, 0x02, 0x64, 0x03, 0xD2, 0x00, 0xE0, 0x01, 0xE3, 0x01, 0x01, 0x00, 0x00,
    0x00, 0x41, 0x00, 0x0F, 0x0B, 0x1A, 0x41, 0x01, 0x0B,
};

TEST(test_stack_switching_feature_gate) {
    /* (module (tag $t (param i32)) (func (export "run") (suspend $t (i32.const 1)))) */
    static const uint8_t suspend_sections[] = {
        0x01, 0x08, 0x02, 0x60, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x01, 0x0D, 0x03,
        0x01, 0x00, 0x00, 0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6E, 0x00, 0x00, 0x0A, 0x08, 0x01, 0x06,
        0x00, 0x41, 0x01, 0xE2, 0x00, 0x0B,
    };
    /* (module (func (param contref))) */
    static const uint8_t contref_sections[] = {
        0x01, 0x05, 0x01, 0x60, 0x01, 0x68, 0x00, 0x03, 0x02, 0x01, 0x00, 0x0A, 0x04, 0x01, 0x02, 0x00, 0x0B,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint32_t func_idx = 0;
    uint32_t offset = 0;

    wasm_init_features(&rt, WASM_FEATURES_STANDARD);
    m = wasm_test_load_sections(&rt, wasm_test_stack_switching_sections, sizeof(wasm_test_stack_switching_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected continuation types to be disabled");
    CHECK_MSG(t, strstr(rt.error_msg, "stack switching") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    m = wasm_test_load_sections(&rt, suspend_sections, sizeof(suspend_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected suspend to be disabled");
    CHECK_MSG(t, strstr(rt.error_msg, "stack switching") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    m = wasm_test_load_sections(&rt, contref_sections, sizeof(contref_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected contref to be disabled");
    CHECK_MSG(t, strstr(rt.error_msg, "stack switching") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, contref_sections, sizeof(contref_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    m = wasm_test_load_sections(&rt, wasm_test_stack_switching_sections, sizeof(wasm_test_stack_switching_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        CHECK_MSG(t, (m->required_features & WASM_FEATURE_STACK_SWITCHING) != 0, "%s",
                  "expected the module to record that it needs stack switching");
        CHECK(t, wasm_type_kind(m, 1) == WASM_COMP_CONT);
        CHECK(t, wasm_cont_type_func(m, 1) == 0);
        CHECK(t, wasm_cont_type_func(m, 3) == 2);
        CHECK(t, wasm_cont_type_func(m, 0) == UINT32_MAX);
        WASM_CHECK_OK(t, wasm_call(m, "run", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 0);
        wasm_module_free(m);
    }
    m = wasm_test_load_sections(&rt, suspend_sections, sizeof(suspend_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        err = wasm_call(m, "run", NULL, 0, NULL, 0);
        CHECK_MSG(t, err == WASM_ERR_TRAP, "expected WASM_ERR_TRAP, got %d", (int)err);
        CHECK_MSG(t, strstr(rt.error_msg, "unhandled tag") != NULL, "%s", rt.error_msg);
        WASM_CHECK_OK(t, wasm_runtime_call_frame(&rt, 0, NULL, &func_idx, &offset));
        CHECK_MSG(t, func_idx == 0 && m->funcs[0].code[offset] == 0xE2,
                  "expected the trap at the suspend, got function %u offset %u", (unsigned)func_idx,
                  (unsigned)offset);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_stack_switching_malformed_handlers_fail_lazily) {
    uint8_t sections[sizeof(wasm_test_stack_switching_sections)];
    wasm_config_t cfg;
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;
    uint32_t i;

    memcpy(sections, wasm_test_stack_switching_sections, sizeof(sections));
    for (i = 0; i + 3u < sizeof(sections); i++) {
        if (sections[i] == 0xE3 && sections[i + 1u] == 0x01 && sections[i + 2u] == 0x01) {
            sections[i + 3u] = 0x02;
            break;
        }
    }
    CHECK(t, i + 3u < sizeof(sections));

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, sections, sizeof(sections));
    CHECK_MSG(t, m == NULL, "%s", "expected an unknown handler kind to be rejected");
    CHECK_MSG(t, strstr(rt.error_msg, "handler kind") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_config_default(&cfg);
    cfg.lazy_validation = 1;
    wasm_init_config(&rt, &cfg);
    m = wasm_test_load_sections(&rt, sections, sizeof(sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        err = wasm_call(m, "run", NULL, 0, &result, 1);
        CHECK_MSG(t, err == WASM_ERR_MALFORMED, "expected WASM_ERR_MALFORMED, got %d", (int)err);
        CHECK_MSG(t, strstr(rt.error_msg, "handler kind") != NULL, "%s", rt.error_msg);
        wasm_module_free(m);
    }
    m = wasm_test_load_sections(&rt, wasm_test_stack_switching_sections, sizeof(wasm_test_stack_switching_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        WASM_CHECK_OK(t, wasm_call(m, "run", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 0);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_stack_switching_tag_results) {
    /* (module (tag (export "t") (param i64) (result i32))) */
    static const uint8_t tag_sections[] = {
        0x01, 0x06, 0x01, 0x60, 0x01, 0x7E, 0x01, 0x7F, 0x0D, 0x03, 0x01, 0x00, 0x00, 0x07, 0x05, 0x01,
        0x01, 0x74, 0x04, 0x00,
    };
    /* (module (tag $t (result i32)) (func (throw $t))) */
    static const uint8_t throw_sections[] = {
        0x01, 0x08, 0x02, 0x60, 0x00, 0x01, 0x7F, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x01, 0x0D, 0x03,
        0x01, 0x00, 0x00, 0x0A, 0x06, 0x01, 0x04, 0x00, 0x08, 0x00, 0x0B,
    };
    /* (module (tag $t (result i32)) (func (block $l (try_table (catch $t $l))))) */
    static const uint8_t catch_sections[] = {
        0x01, 0x08, 0x02, 0x60, 0x00, 0x01, 0x7F, 0x60, 0x00, 0x00, 0x03, 0x02, 0x01, 0x01, 0x0D, 0x03,
        0x01, 0x00, 0x00, 0x0A, 0x0E, 0x01, 0x0C, 0x00, 0x02, 0x40, 0x1F, 0x40, 0x01, 0x00, 0x00, 0x00,
        0x0B, 0x0B, 0x0B,
    };
    /* (module (import "a" "t" (tag (param i64) (result i32)))) */
    static const uint8_t import_sections[] = {
        0x01, 0x06, 0x01, 0x60, 0x01, 0x7E, 0x01, 0x7F, 0x02, 0x08, 0x01, 0x01, 'a', 0x01, 't', 0x04, 0x00, 0x00,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_module_t* importer;

    wasm_init_features(&rt, WASM_FEATURES_ALL & ~WASM_FEATURE_STACK_SWITCHING);
    m = wasm_test_load_sections(&rt, tag_sections, sizeof(tag_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected tag results to need stack switching");
    if (m != NULL) wasm_module_free(m);
    importer = wasm_test_load_sections(&rt, import_sections, sizeof(import_sections));
    CHECK_MSG(t, importer == NULL, "%s", "expected imported tag results to need stack switching");
    CHECK_MSG(t, strstr(rt.error_msg, "non-empty tag result type") != NULL, "%s", rt.error_msg);
    if (importer != NULL) wasm_module_free(importer);
    wasm_destroy(&rt);

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, throw_sections, sizeof(throw_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected throw to reject a tag with results");
    CHECK_MSG(t, strstr(rt.error_msg, "non-empty tag result type") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    m = wasm_test_load_sections(&rt, catch_sections, sizeof(catch_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected catch to reject a tag with results");
    CHECK_MSG(t, strstr(rt.error_msg, "non-empty tag result type") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);

    m = wasm_test_load_sections(&rt, tag_sections, sizeof(tag_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "a", m));
    importer = wasm_test_load_sections(&rt, import_sections, sizeof(import_sections));
    CHECK_MSG(t, importer != NULL, "%s", rt.error_msg);
    if (importer != NULL) wasm_module_free(importer);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case validate_cont_cases[] = {
    { "stack switching: rejected when disabled, continuations run when enabled", test_stack_switching_feature_gate },
    { "stack switching: malformed resume handlers fail at load or, lazily, at the call", test_stack_switching_malformed_handlers_fail_lazily },
    { "stack switching: tags may declare results but exception uses reject them", test_stack_switching_tag_results },
};
const test_suite validate_cont_suite = { "validate.cont", validate_cont_cases, COUNTOF(validate_cont_cases) };

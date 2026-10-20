#include "../support.h"

TEST(test_gc_validation_accepts_struct_and_array_gc_opcodes) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x5E);
        emit(&sec, 0x7F);
        emit(&sec, 0x01);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x00);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1A);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 9);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 3);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x06);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x1A);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_validation_rejects_struct_get_on_packed_field_without_sign) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x78);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6B);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x02);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected unpacked struct.get validation to reject packed fields");
    CHECK_MSG(t, strstr(rt.error_msg, "unpacked field") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_gc_validation_accepts_cast_and_branch_cast_opcodes) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_i32(&body, 0);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x14);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x1A);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x17);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x1A);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x18);
        emit(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit_leb128_i32(&body, 0);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x1A);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x19);
        emit(&body, 0x01);
        emit_leb128_u32(&body, 0);
        emit_leb128_i32(&body, 0);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x1A);

        emit(&body, 0xD0);
        emit_leb128_i32(&body, 0);
        emit(&body, 0xD4);
        emit(&body, 0x1A);

        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_validation_rejects_invalid_ref_cast_relation) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);

        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6F);

        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xFB);
        emit_leb128_u32(&body, 0x16);
        emit(&body, 0x6D);
        emit(&body, 0x1A);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected invalid static ref.cast relation to fail validation");
    CHECK_MSG(t, strstr(rt.error_msg, "type mismatch") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_custom_descriptor_instructions_validate_and_run) {
    /* (rec (type $a (descriptor $b) (struct)) (type $b (describes $a) (struct)))
       (func (export "new") (result anyref) (struct.new_default_desc $a (struct.new $b)))
       (func (export "get") (param (ref null $a)) (result anyref) (ref.get_desc $a (local.get 0)))
       (func (export "cast") (param anyref) (result anyref)
         (ref.cast_desc_eq (ref null $a) (local.get 0) (struct.new $b)))
       (func (export "br") (param anyref) (result anyref)
         (br_on_cast_desc_eq 0 anyref (ref null $a) (local.get 0) (struct.new $b)))
       (func (param (ref (exact $a))) (result (ref (exact $b))) (ref.get_desc $a (local.get 0))) */
    static const uint8_t sections[] = {
        0x01, 0x23, 0x05, 0x4E, 0x02, 0x4D, 0x01, 0x5F, 0x00, 0x4C, 0x00, 0x5F, 0x00, 0x60, 0x00, 0x01,
        0x6E, 0x60, 0x01, 0x63, 0x00, 0x01, 0x6E, 0x60, 0x01, 0x6E, 0x01, 0x6E, 0x60, 0x01, 0x64, 0x62,
        0x00, 0x01, 0x64, 0x62, 0x01, 0x03, 0x06, 0x05, 0x02, 0x03, 0x04, 0x04, 0x05, 0x07, 0x19, 0x04,
        0x03, 0x6E, 0x65, 0x77, 0x00, 0x00, 0x03, 0x67, 0x65, 0x74, 0x00, 0x01, 0x04, 0x63, 0x61, 0x73,
        0x74, 0x00, 0x02, 0x02, 0x62, 0x72, 0x00, 0x03, 0x0A, 0x33, 0x05, 0x08, 0x00, 0xFB, 0x00, 0x01,
        0xFB, 0x21, 0x00, 0x0B, 0x07, 0x00, 0x20, 0x00, 0xFB, 0x22, 0x00, 0x0B, 0x0A, 0x00, 0x20, 0x00,
        0xFB, 0x00, 0x01, 0xFB, 0x24, 0x00, 0x0B, 0x0D, 0x00, 0x20, 0x00, 0xFB, 0x00, 0x01, 0xFB, 0x25,
        0x03, 0x00, 0x6E, 0x00, 0x0B, 0x07, 0x00, 0x20, 0x00, 0xFB, 0x22, 0x00, 0x0B,
    };
    static const char* const exports[] = { "new", "get", "cast", "br" };
    static const wasm_error_t expected[] = { WASM_OK, WASM_ERR_TRAP, WASM_OK, WASM_OK };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t arg = wasm_ref_null(WASM_TYPE_ANYREF);
    wasm_value_t result;
    size_t i;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, sections, sizeof(sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        for (i = 0; i < COUNTOF(exports); i++) {
            wasm_error_t err = wasm_call(m, exports[i], &arg, i == 0 ? 0u : 1u, &result, 1);

            CHECK_MSG(t, err == expected[i], "%s: expected %d, got %d (%s)", exports[i], (int)expected[i], (int)err,
                      rt.error_msg);
            CHECK_MSG(t, err != WASM_OK || (i == 0) == (result.of.gc_ref != 0), "%s: unexpected result", exports[i]);
            CHECK_MSG(t, err == WASM_OK || strstr(rt.error_msg, "null reference") != NULL, "%s", rt.error_msg);
        }
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_custom_descriptor_casts_keep_their_operand_types) {
    /* (rec (type $a (descriptor $b) (struct)) (type $b (describes $a) (struct)))
       (func (param (ref $a)) (result (ref (exact $b))) (ref.get_desc $a (local.get 0))) */
    static const uint8_t inexact_get_sections[] = {
        0x01, 0x13, 0x02, 0x4E, 0x02, 0x4D, 0x01, 0x5F, 0x00, 0x4C, 0x00, 0x5F, 0x00, 0x60, 0x01, 0x64,
        0x00, 0x01, 0x64, 0x62, 0x01, 0x03, 0x02, 0x01, 0x02, 0x0A, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00,
        0xFB, 0x22, 0x00, 0x0B,
    };
    /* Same types; (func (param (ref null $a) (ref null $b)) (result (ref null none))
         (drop (block (result (ref $a))
           (return (br_on_cast_desc_eq 0 (ref null $a) (ref $a) (local.get 0) (local.get 1)))))
         (ref.null none)) */
    static const uint8_t desc_fallthrough_sections[] = {
        0x01, 0x13, 0x02, 0x4E, 0x02, 0x4D, 0x01, 0x5F, 0x00, 0x4C, 0x00, 0x5F, 0x00, 0x60, 0x02, 0x63,
        0x00, 0x63, 0x01, 0x01, 0x71, 0x03, 0x02, 0x01, 0x02, 0x0A, 0x16, 0x01, 0x14, 0x00, 0x02, 0x64,
        0x00, 0x20, 0x00, 0x20, 0x01, 0xFB, 0x25, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x0B, 0x1A, 0xD0, 0x71,
        0x0B,
    };
    /* (type $s (sub (struct))) (func (param (ref null $s)) (result (ref null none))
         (drop (block (result (ref (exact $s)))
           (return (br_on_cast 0 (ref null $s) (ref (exact $s)) (local.get 0)))))
         (ref.null none)) */
    static const uint8_t exact_fallthrough_sections[] = {
        0x01, 0x0B, 0x02, 0x50, 0x00, 0x5F, 0x00, 0x60, 0x01, 0x63, 0x00, 0x01, 0x71, 0x03, 0x02, 0x01,
        0x01, 0x0A, 0x16, 0x01, 0x14, 0x00, 0x02, 0x64, 0x62, 0x00, 0x20, 0x00, 0xFB, 0x18, 0x01, 0x00,
        0x00, 0x62, 0x00, 0x0F, 0x0B, 0x1A, 0xD0, 0x71, 0x0B,
    };
    static const struct {
        const uint8_t* bytes;
        uint32_t len;
    } cases[] = {
        { inexact_get_sections, sizeof(inexact_get_sections) },
        { desc_fallthrough_sections, sizeof(desc_fallthrough_sections) },
        { exact_fallthrough_sections, sizeof(exact_fallthrough_sections) },
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    size_t i;

    wasm_init(&rt);
    for (i = 0; i < COUNTOF(cases); i++) {
        m = wasm_test_load_sections(&rt, cases[i].bytes, cases[i].len);
        CHECK_MSG(t, m == NULL, "expected case %u to fail validation", (unsigned)i);
        CHECK_MSG(t, strstr(rt.error_msg, "type mismatch") != NULL, "%s", rt.error_msg);
        if (m != NULL) wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_br_on_cast_relaxation_needs_custom_descriptors) {
    /* (func (param eqref) (result anyref) (br_on_cast 0 eqref anyref (local.get 0))) */
    static const uint8_t relaxed_sections[] = {
        0x01, 0x06, 0x01, 0x60, 0x01, 0x6D, 0x01, 0x6E, 0x03, 0x02, 0x01, 0x00, 0x0A, 0x0C, 0x01, 0x0A,
        0x00, 0x20, 0x00, 0xFB, 0x18, 0x03, 0x00, 0x6D, 0x6E, 0x0B,
    };
    /* (func (param anyref) (result anyref) (br_on_cast 0 anyref eqref (local.get 0))) */
    static const uint8_t plain_sections[] = {
        0x01, 0x06, 0x01, 0x60, 0x01, 0x6E, 0x01, 0x6E, 0x03, 0x02, 0x01, 0x00, 0x0A, 0x0C, 0x01, 0x0A,
        0x00, 0x20, 0x00, 0xFB, 0x18, 0x03, 0x00, 0x6E, 0x6D, 0x0B,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init_features(&rt, WASM_FEATURES_ALL & ~WASM_FEATURE_CUSTOM_DESCRIPTORS);
    m = wasm_test_load_sections(&rt, relaxed_sections, sizeof(relaxed_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected a target outside the source type to need custom descriptors");
    CHECK_MSG(t, strstr(rt.error_msg, "type mismatch") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, relaxed_sections, sizeof(relaxed_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        CHECK(t, (m->required_features & WASM_FEATURE_CUSTOM_DESCRIPTORS) != 0);
        wasm_module_free(m);
    }
    m = wasm_test_load_sections(&rt, plain_sections, sizeof(plain_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        CHECK(t, (m->required_features & WASM_FEATURE_CUSTOM_DESCRIPTORS) == 0);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

static const test_case validate_gc_cases[] = {
    { "custom descriptors: descriptor instructions validate and run", test_custom_descriptor_instructions_validate_and_run },
    { "custom descriptors: descriptor and exact casts keep inexact operand types", test_custom_descriptor_casts_keep_their_operand_types },
    { "custom descriptors: br_on_cast to a type outside its source needs the feature", test_br_on_cast_relaxation_needs_custom_descriptors },
    { "wasmgc: validator accepts struct.new and array.new opcodes", test_gc_validation_accepts_struct_and_array_gc_opcodes },
    { "wasmgc: validator rejects struct.get on packed fields without signedness", test_gc_validation_rejects_struct_get_on_packed_field_without_sign },
    { "wasmgc: validator accepts ref.cast and br_on_cast opcodes", test_gc_validation_accepts_cast_and_branch_cast_opcodes },
    { "wasmgc: validator rejects invalid static ref.cast relations", test_gc_validation_rejects_invalid_ref_cast_relation },
};
const test_suite validate_gc_suite = { "validate.gc", validate_gc_cases, COUNTOF(validate_gc_cases) };

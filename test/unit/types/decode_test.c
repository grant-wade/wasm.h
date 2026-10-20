#include "../support.h"

TEST(test_gc_type_section_parses_composite_types) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x4E);
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);
        emit(&sec, 0x78);
        emit(&sec, 0x01);

        emit(&sec, 0x5E);
        emit(&sec, 0x77);
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
        emit_leb128_u32(&sec, 3);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, m->num_types == 4, "expected 4 types, got %u", (unsigned)m->num_types);
    CHECK_MSG(t, m->num_rec_groups == 3, "expected 3 rec groups, got %u", (unsigned)m->num_rec_groups);

    CHECK_MSG(t, m->types[0].kind == WASM_COMP_STRUCT, "type 0 kind = %u", (unsigned)m->types[0].kind);
    CHECK_MSG(t, m->types[1].kind == WASM_COMP_STRUCT, "type 1 kind = %u", (unsigned)m->types[1].kind);
    CHECK_MSG(t, m->types[2].kind == WASM_COMP_ARRAY, "type 2 kind = %u", (unsigned)m->types[2].kind);
    CHECK_MSG(t, m->types[3].kind == WASM_COMP_FUNC, "type 3 kind = %u", (unsigned)m->types[3].kind);

    CHECK_MSG(t, m->rec_groups[0].first_type == 0 && m->rec_groups[0].num_types == 2,
              "unexpected rec group 0: first=%u count=%u",
              (unsigned)m->rec_groups[0].first_type,
              (unsigned)m->rec_groups[0].num_types);
    CHECK_MSG(t, m->rec_groups[1].first_type == 2 && m->rec_groups[1].num_types == 1,
              "unexpected rec group 1: first=%u count=%u",
              (unsigned)m->rec_groups[1].first_type,
              (unsigned)m->rec_groups[1].num_types);
    CHECK_MSG(t, m->rec_groups[2].first_type == 3 && m->rec_groups[2].num_types == 1,
              "unexpected rec group 2: first=%u count=%u",
              (unsigned)m->rec_groups[2].first_type,
              (unsigned)m->rec_groups[2].num_types);

    CHECK_MSG(t, m->types[1].num_supertypes == 1, "expected subtype to record one supertype");
    CHECK_MSG(t, m->types[1].supertypes != NULL && m->types[1].supertypes[0] == 0,
              "%s", "expected subtype to point at type 0");
    CHECK_MSG(t, m->types[0].is_final == 0, "%s", "expected explicit root type to stay non-final");
    CHECK_MSG(t, m->types[1].is_final == 0, "%s", "sub type should not be final by default");
    CHECK_MSG(t, m->types[0].of.struct_.num_fields == 1, "%s", "struct field count mismatch");
    CHECK_MSG(t, m->types[1].of.struct_.num_fields == 2, "%s", "sub-struct field count mismatch");
    CHECK_MSG(t, m->types[1].of.struct_.fields[1].storage.kind == WASM_STORAGE_PACKED,
              "%s", "expected packed storage on subtype field");
    CHECK_MSG(t, m->types[1].of.struct_.fields[1].storage.of.packed_type == WASM_PACKED_I8,
              "%s", "expected i8 packed field");
    CHECK_MSG(t, m->types[1].of.struct_.fields[1].is_mutable == 1,
              "%s", "expected mutable subtype field");
    CHECK_MSG(t, m->types[2].of.array.field.storage.kind == WASM_STORAGE_PACKED,
              "%s", "expected packed array storage");
    CHECK_MSG(t, m->types[2].of.array.field.storage.of.packed_type == WASM_PACKED_I16,
              "%s", "expected i16 packed array element");

    CHECK_MSG(t, wasm_func_param_count(m, 0) == 0, "%s", "function param count mismatch");
    CHECK_MSG(t, wasm_func_result_count(m, 0) == 1, "%s", "function result count mismatch");

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 7);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_type_section_respects_feature_gate) {
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
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init_features(&rt, WASM_FEATURES_ALL & ~WASM_FEATURE_GC);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected GC-disabled runtime to reject GC type section");
    CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    CHECK_MSG(t, strstr(rt.error_msg, "garbage collection") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_gc_rejects_multiple_supertypes) {
    static const uint8_t types[] = {
        0x01, 0x0F, 0x03,
        0x50, 0x00, 0x5F, 0x00,
        0x50, 0x00, 0x5F, 0x00,
        0x50, 0x02, 0x00, 0x01, 0x5F, 0x00,
    };
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);
    emit_bytes(&mod, types, sizeof(types));

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected multiple supertypes to be rejected");
    CHECK_MSG(t, strstr(rt.error_msg, "multiple supertypes") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_stack_switching_cont_type_decoding) {
    /* (rec (type (cont 1))) (rec (type (func))): the continuation names a later rec group. */
    static const uint8_t forward_sections[] = { 0x01, 0x06, 0x02, 0x5D, 0x01, 0x60, 0x00, 0x00 };
    /* A continuation type over the abstract heap type func. */
    static const uint8_t abstract_sections[] = { 0x01, 0x03, 0x01, 0x5D, 0x70 };
    /* (rec (type (cont 1)) (type (func))) */
    static const uint8_t rec_sections[] = { 0x01, 0x08, 0x01, 0x4E, 0x02, 0x5D, 0x01, 0x60, 0x00, 0x00 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, forward_sections, sizeof(forward_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected a forward continuation type reference to be rejected");
    CHECK_MSG(t, strstr(rt.error_msg, "unknown type 1") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    m = wasm_test_load_sections(&rt, abstract_sections, sizeof(abstract_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected a continuation of an abstract heap type to be malformed");
    if (m != NULL) wasm_module_free(m);
    m = wasm_test_load_sections(&rt, rec_sections, sizeof(rec_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        CHECK(t, wasm_cont_type_func(m, 0) == 1);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

TEST(test_custom_descriptor_clauses_and_exact_types_decode) {
    /* (rec (type (sub (descriptor 1) (struct (field i32)))) (type (sub (describes 0) (struct))))
       (type (struct (field (ref null (exact 0))))) */
    static const uint8_t sections[] = {
        0x01, 0x17, 0x02, 0x4E, 0x02, 0x50, 0x00, 0x4D, 0x01, 0x5F, 0x01, 0x7F, 0x00, 0x50, 0x00, 0x4C,
        0x00, 0x5F, 0x00, 0x5F, 0x01, 0x63, 0x62, 0x00, 0x00,
    };
    /* The second type has descriptor before describes. */
    static const uint8_t misordered_sections[] = {
        0x01, 0x11, 0x01, 0x4E, 0x03, 0x4D, 0x01, 0x5F, 0x00, 0x4D, 0x02, 0x4C, 0x00, 0x5F, 0x00, 0x4C,
        0x01, 0x5F, 0x00,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, sections, sizeof(sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        const wasm_reftype_t* field = &m->types[2].of.struct_.fields[0].storage.ref_type;

        CHECK(t, m->types[0].descriptor == 1 && m->types[0].describes == UINT32_MAX);
        CHECK(t, m->types[1].describes == 0 && m->types[1].descriptor == UINT32_MAX);
        CHECK(t, m->types[2].descriptor == UINT32_MAX && m->types[2].describes == UINT32_MAX);
        CHECK(t, wasm_type_descriptor(m, 0) == 1 && wasm_type_describes(m, 0) == UINT32_MAX);
        CHECK(t, wasm_type_describes(m, 1) == 0 && wasm_type_descriptor(m, 1) == UINT32_MAX);
        CHECK(t, wasm_type_descriptor(m, 3) == UINT32_MAX && wasm_type_describes(m, 3) == UINT32_MAX);
        CHECK(t, field->has_type_index && field->type_index == 0 && field->exact && field->nullable);
        CHECK(t, (m->required_features & WASM_FEATURE_CUSTOM_DESCRIPTORS) != 0);
        wasm_module_free(m);
    }
    m = wasm_test_load_sections(&rt, misordered_sections, sizeof(misordered_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected a descriptor clause before a describes clause to be malformed");
    if (m != NULL) wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_custom_descriptors_feature_gate) {
    /* (rec (type (descriptor 1) (struct)) (type (describes 0) (struct))) */
    static const uint8_t clause_sections[] = {
        0x01, 0x0B, 0x01, 0x4E, 0x02, 0x4D, 0x01, 0x5F, 0x00, 0x4C, 0x00, 0x5F, 0x00,
    };
    /* (type (struct)) (type (struct (field (ref (exact 0))))) */
    static const uint8_t exact_sections[] = { 0x01, 0x09, 0x02, 0x5F, 0x00, 0x5F, 0x01, 0x64, 0x62, 0x00, 0x00 };
    /* (type (func)) (import "a" "f" (func (exact (type 0)))) */
    static const uint8_t import_sections[] = {
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x02, 0x07, 0x01, 0x01, 0x61, 0x01, 0x66, 0x20, 0x00,
    };
    static const struct {
        const uint8_t* bytes;
        uint32_t len;
    } cases[] = {
        { clause_sections, sizeof(clause_sections) },
        { exact_sections, sizeof(exact_sections) },
        { import_sections, sizeof(import_sections) },
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    size_t i;

    wasm_init_features(&rt, WASM_FEATURES_ALL & ~WASM_FEATURE_CUSTOM_DESCRIPTORS);
    for (i = 0; i < COUNTOF(cases); i++) {
        m = wasm_test_load_sections(&rt, cases[i].bytes, cases[i].len);
        CHECK_MSG(t, m == NULL, "expected case %u to need custom descriptors", (unsigned)i);
        CHECK_MSG(t, strstr(rt.error_msg, "custom descriptors") != NULL, "%s", rt.error_msg);
        if (m != NULL) wasm_module_free(m);
    }
    wasm_destroy(&rt);

    wasm_init(&rt);
    for (i = 0; i < COUNTOF(cases); i++) {
        wasm_builder_t mod = { 0 };

        emit_header(&mod);
        emit_bytes(&mod, cases[i].bytes, cases[i].len);
        m = wasm_compile(&rt, mod.buf, mod.len);
        CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
        if (m == NULL) continue;
        if (cases[i].bytes == import_sections) {
            const wasm_import_info_t* info = wasm_import_info(m, 0);

            CHECK(t, info != NULL && info->kind == WASM_IMPORT_FUNC && info->exact && info->type_index == 0);
            CHECK(t, !m->funcs[0].is_inexact);
        }
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

static const test_case types_decode_cases[] = {
    { "custom descriptors: descriptor clauses and exact heap types decode onto the type", test_custom_descriptor_clauses_and_exact_types_decode },
    { "custom descriptors: clauses, exact types, and exact imports are feature-gated", test_custom_descriptors_feature_gate },
    { "stack switching: continuation types name an earlier or same-group function type", test_stack_switching_cont_type_decoding },
    { "wasmgc: composite type sections parse rec/sub/struct/array forms", test_gc_type_section_parses_composite_types },
    { "wasmgc: GC type syntax is feature-gated", test_gc_type_section_respects_feature_gate },
    { "wasmgc: subtypes may declare at most one supertype", test_gc_rejects_multiple_supertypes },
};
const test_suite types_decode_suite = { "types.decode", types_decode_cases, COUNTOF(types_decode_cases) };

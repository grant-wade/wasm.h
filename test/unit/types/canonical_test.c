#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/types/canonical.h"
#endif

static size_t wasm_test_put_leb128_u32(uint8_t* out, uint32_t value) {
    size_t n = 0;

    do {
        uint8_t byte = (uint8_t)(value & 0x7Fu);

        value >>= 7;
        if (value) byte |= 0x80u;
        out[n++] = byte;
    } while (value);
    return n;
}

TEST(test_canonicalization_dedupes_many_types) {
    enum { DISTINCT = 2048, PARAMS = 11, TYPE_BYTES = PARAMS + 3 };
    static const uint8_t header[] = { 0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00 };
    uint8_t* bytes = (uint8_t*)malloc(64u + (size_t)DISTINCT * 2u * TYPE_BYTES);
    uint8_t count[5];
    size_t count_len = wasm_test_put_leb128_u32(count, DISTINCT * 2u);
    size_t len = 0;
    wasm_runtime_t rt;
    wasm_module_t* m;
    uint32_t i;
    uint32_t d;
    uint32_t mismatches = 0;

    CHECK_MSG(t, bytes != NULL, "%s", "allocation failed");
    if (!bytes) return;
    memcpy(bytes, header, sizeof(header));
    len = sizeof(header);
    bytes[len++] = 1;
    len += wasm_test_put_leb128_u32(bytes + len, (uint32_t)(count_len + (size_t)DISTINCT * 2u * TYPE_BYTES));
    memcpy(bytes + len, count, count_len);
    len += count_len;
    for (i = 0; i < DISTINCT * 2u; i++) {
        uint32_t bits = i % DISTINCT;

        bytes[len++] = 0x60;
        bytes[len++] = PARAMS;
        for (d = 0; d < PARAMS; d++) bytes[len++] = (bits >> d) & 1u ? 0x7E : 0x7F;
        bytes[len++] = 0;
    }

    wasm_init(&rt);
    m = wasm_load(&rt, bytes, len);
    free(bytes);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m) {
        for (i = 0; i < DISTINCT; i++) {
            if (m->types[i].canonical_id != m->types[i + DISTINCT].canonical_id) mismatches++;
            if (i > 0 && m->types[i].canonical_id == m->types[i - 1u].canonical_id) mismatches++;
        }
        CHECK_MSG(t, mismatches == 0, "expected identical types to share ids and distinct types not to, %u wrong",
                  (unsigned)mismatches);
    }
    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_call_indirect_uses_structural_type_equality) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 2);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 0);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 9, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 2);

        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x0B);
        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);

        body.len = 0;
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x11);
        emit_leb128_u32(&body, 0);
        emit_leb128_u32(&body, 0);
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

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 7);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_type_canonicalization_and_function_subtyping) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, m->types[0].canonical_id != 0, "%s", "expected canonical ids to be assigned");
    CHECK_MSG(t, m->types[0].canonical_id == m->types[2].canonical_id,
              "expected equivalent function types to share a canonical id (%u vs %u)",
              (unsigned)m->types[0].canonical_id,
              (unsigned)m->types[2].canonical_id);
    CHECK_MSG(t, wasm__type_equal(m, 0, 2), "%s", "expected structurally identical types to compare equal");
    CHECK_MSG(t, !wasm__type_equal(m, 0, 1), "%s", "expected subtype body to remain distinct from its supertype");
    CHECK_MSG(t, wasm__is_subtype(m, 1, 0), "%s", "expected declared subtype relation to hold");
    CHECK_MSG(t, !wasm__is_subtype(m, 0, 1), "%s", "expected subtype relation to be directional");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_type_validation_accepts_struct_width_subtyping) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);
        emit(&sec, 0x00);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x6C);
        emit(&sec, 0x00);
        emit(&sec, 0x7F);
        emit(&sec, 0x00);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, wasm__is_subtype(m, 1, 0), "%s", "expected struct width/depth subtype to validate");
    CHECK_MSG(t, !wasm__type_equal(m, 1, 0), "%s", "expected subtype to stay distinct from supertype");

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_type_validation_rejects_invalid_function_subtype) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6D);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected invalid function variance to fail load");
    CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    CHECK_MSG(t, strstr(rt.error_msg, "valid subtype") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_gc_type_validation_rejects_final_supertype) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 2);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    wasm_init(&rt);
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected subtyping from a final type to fail load");
    CHECK_MSG(t, rt.last_error == WASM_ERR_MALFORMED, "%s", rt.error_msg);
    CHECK_MSG(t, strstr(rt.error_msg, "final type") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_canonicalization_distinguishes_descriptor_clauses_and_exactness) {
    /* (rec (type (descriptor 1) (struct)) (type (describes 0) (struct)))
       (rec (type (descriptor 3) (struct)) (type (describes 2) (struct)))
       (rec (type (struct)) (type (struct)))
       (type (struct (field (ref 4)))) (type (struct (field (ref (exact 4)))))
       (type (struct (field (ref (exact 4))))) */
    static const uint8_t sections[] = {
        0x01, 0x2C, 0x06, 0x4E, 0x02, 0x4D, 0x01, 0x5F, 0x00, 0x4C, 0x00, 0x5F, 0x00, 0x4E, 0x02, 0x4D,
        0x03, 0x5F, 0x00, 0x4C, 0x02, 0x5F, 0x00, 0x4E, 0x02, 0x5F, 0x00, 0x5F, 0x00, 0x5F, 0x01, 0x64,
        0x04, 0x00, 0x5F, 0x01, 0x64, 0x62, 0x04, 0x00, 0x5F, 0x01, 0x64, 0x62, 0x04, 0x00,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, sections, sizeof(sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        CHECK(t, m->types[0].canonical_id == m->types[2].canonical_id);
        CHECK(t, m->types[1].canonical_id == m->types[3].canonical_id);
        CHECK(t, m->types[0].canonical_id != m->types[4].canonical_id);
        CHECK(t, m->types[1].canonical_id != m->types[5].canonical_id);
        CHECK(t, m->types[6].canonical_id != m->types[7].canonical_id);
        CHECK(t, m->types[7].canonical_id == m->types[8].canonical_id);
        wasm_module_free(m);
    }
    wasm_destroy(&rt);
}

static const test_case types_canonical_cases[] = {
    { "custom descriptors: descriptor clauses and exactness are part of type identity", test_canonicalization_distinguishes_descriptor_clauses_and_exactness },
    { "indirect call: structurally equal signatures are accepted", test_call_indirect_uses_structural_type_equality },
    { "wasmgc: canonical ids and function subtyping are computed at load", test_gc_type_canonicalization_and_function_subtyping },
    { "wasmgc: struct width/depth subtyping validates", test_gc_type_validation_accepts_struct_width_subtyping },
    { "wasmgc: invalid function variance is rejected", test_gc_type_validation_rejects_invalid_function_subtype },
    { "wasmgc: final supertypes cannot be extended", test_gc_type_validation_rejects_final_supertype },
    { "types: canonicalization dedupes thousands of types", test_canonicalization_dedupes_many_types },
};
const test_suite types_canonical_suite = { "types.canonical", types_canonical_cases, COUNTOF(types_canonical_cases) };

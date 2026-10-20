#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/values.h"
#endif

static void emit_table_type(wasm_builder_t* b,
                            wasm_valtype_t reftype,
                            int is_64,
                            int has_max,
                            uint32_t initial_elems,
                            uint32_t max_elems) {
    uint8_t flags = (uint8_t)((has_max ? 0x01u : 0u) | (is_64 ? 0x04u : 0u));

    emit(b, (uint8_t)reftype);
    emit(b, flags);
    emit_leb128_u32(b, initial_elems);
    if (has_max) emit_leb128_u32(b, max_elems);
}

static void emit_import_table(wasm_builder_t* b,
                              const char* module,
                              const char* name,
                              wasm_valtype_t reftype,
                              int is_64,
                              int has_max,
                              uint32_t initial_elems,
                              uint32_t max_elems) {
    uint32_t module_len = (uint32_t)strlen(module);
    uint32_t name_len = (uint32_t)strlen(name);

    emit_leb128_u32(b, module_len);
    emit_bytes(b, (const uint8_t*)module, module_len);
    emit_leb128_u32(b, name_len);
    emit_bytes(b, (const uint8_t*)name, name_len);
    emit(b, 0x01);
    emit_table_type(b, reftype, is_64, has_max, initial_elems, max_elems);
}

static wasm_error_t host_gc_subtype_identity(wasm_module_t* mod,
                                             const wasm_value_t* args, uint32_t num_args,
                                             wasm_value_t* results, uint32_t num_results,
                                             void* userdata) {
    (void)mod;
    (void)args;
    (void)num_args;
    (void)userdata;

    if (num_results > 0) results[0] = wasm_ref_null(WASM_TYPE_NONE);
    return WASM_OK;
}

TEST(test_table64_import_width_mismatch) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_table_t host_table;
    wasm_table_import_t import_desc;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_import_table(&sec, "host", "table", WASM_TYPE_FUNCREF, 0, 1, 1, 1);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&host_table, 0, sizeof(host_table));
    host_table.reftype = WASM_TYPE_FUNCREF;
    host_table.reftype_info.type = WASM_TYPE_FUNCREF;
    host_table.reftype_info.nullable = 1;
    host_table.size = 1;
    host_table.max_size = 1;
    host_table.is_64 = 1;

    memset(&import_desc, 0, sizeof(import_desc));
    import_desc.module = "host";
    import_desc.name = "table";
    import_desc.table = &host_table;

    err = wasm_register_table_import(&rt, &import_desc);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected table32 import to reject table64 host binding");
    if (m) wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_typed_select_funcref) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[] = { wasm_i32(1) };
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x70);
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
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "pick", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD2);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x70);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1C);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x70);
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

    err = wasm_call(m, "pick", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, result.type == WASM_TYPE_FUNCREF, "%s", "expected funcref result type");
        CHECK_MSG(t, result.of.funcref == 0, "%s", "expected typed select to choose ref.func 0");
    }

    args[0] = wasm_i32(0);
    err = wasm_call(m, "pick", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, result.type == WASM_TYPE_FUNCREF, "%s", "expected funcref result type");
        CHECK_MSG(t, result.of.funcref == UINT32_MAX, "%s", "expected typed select to choose ref.null");
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_heaptype_parsing_tracks_concrete_type_refs) {
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
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x64);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

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
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);
        emit(&sec, 0xD0);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x0B);
        emit_section(&mod, 6, sec.buf, sec.len);
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
        emit_leb128_u32(&body, 1);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x64);
        emit_leb128_u32(&body, 0);

        emit(&body, 0x02);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x0B);
        emit(&body, 0x1A);

        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 1);
        emit(&body, 0x1C);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x63);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1A);

        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD1);
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

    CHECK_MSG(t, m->types[0].of.struct_.fields[0].storage.of.valtype == WASM_TYPE_STRUCTREF,
              "%s", "expected self field to resolve to structref");
    CHECK_MSG(t, m->types[0].of.struct_.fields[0].storage.ref_type.has_type_index,
              "%s", "expected self field to preserve concrete type index");
    CHECK_MSG(t, m->types[0].of.struct_.fields[0].storage.ref_type.type_index == 0,
              "%s", "expected self field to point at type 0");
    CHECK_MSG(t, m->types[0].of.struct_.fields[0].storage.ref_type.nullable,
              "%s", "expected self field to stay nullable");

    CHECK_MSG(t, m->types[1].of.func.params[0] == WASM_TYPE_STRUCTREF,
              "%s", "expected concrete param to resolve to structref");
    CHECK_MSG(t, !m->types[1].of.func.param_reftypes[0].nullable,
              "%s", "expected concrete param to stay non-null");
    CHECK_MSG(t, m->types[1].of.func.param_reftypes[0].has_type_index && m->types[1].of.func.param_reftypes[0].type_index == 0,
              "%s", "expected concrete param to keep type index 0");
    CHECK_MSG(t, m->types[1].of.func.results[0] == WASM_TYPE_STRUCTREF,
              "%s", "expected concrete result to resolve to structref");
    CHECK_MSG(t, m->types[1].of.func.result_reftypes[0].nullable,
              "%s", "expected concrete result to stay nullable");

    CHECK_MSG(t, m->tables[0].reftype == WASM_TYPE_STRUCTREF,
              "%s", "expected table reftype to resolve to structref");
    CHECK_MSG(t, m->tables[0].reftype_info.has_type_index && m->tables[0].reftype_info.type_index == 0,
              "%s", "expected table to preserve concrete heap type index");
    CHECK_MSG(t, m->globals[0].type == WASM_TYPE_STRUCTREF,
              "%s", "expected global reftype to resolve to structref");
    CHECK_MSG(t, m->globals[0].ref_type.has_type_index && m->globals[0].ref_type.type_index == 0,
              "%s", "expected global to preserve concrete heap type index");
    CHECK_MSG(t, m->funcs[0].num_locals == 1, "%s", "expected one concrete local");
    CHECK_MSG(t, m->funcs[0].locals[0] == WASM_TYPE_STRUCTREF,
              "%s", "expected concrete local to resolve to structref");
    CHECK_MSG(t, m->funcs[0].local_reftypes[0].has_type_index && m->funcs[0].local_reftypes[0].type_index == 0,
              "%s", "expected local to preserve concrete heap type index");
    CHECK_MSG(t, !m->funcs[0].local_reftypes[0].nullable,
              "%s", "expected concrete local to stay non-null");

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_ref_is_null_on_abstract_anyref) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
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
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6E);
        emit(&body, 0xD1);
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
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_typed_select_accepts_eqref_as_anyref) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t args[] = { wasm_i32(1) };
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
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
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "pick", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6D);
        emit(&body, 0xD0);
        emit(&body, 0x6E);
        emit(&body, 0x20);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x1C);
        emit_leb128_u32(&body, 1);
        emit(&body, 0x6E);
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

    err = wasm_call(m, "pick", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, result.type == WASM_TYPE_ANYREF, "%s", "expected the declared anyref result type");
        CHECK_MSG(t, wasm__is_null_ref(&result), "%s", "expected null eqref result");
    }

    args[0] = wasm_i32(0);
    err = wasm_call(m, "pick", args, 1, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        CHECK_MSG(t, result.type == WASM_TYPE_ANYREF, "%s", "expected anyref branch to survive typed select");
        CHECK_MSG(t, wasm__is_null_ref(&result), "%s", "expected null anyref result");
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_table_set_accepts_eqref_into_anyref_table) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
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
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x6E);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 4, sec.buf, sec.len);
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
        emit_leb128_i32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6D);
        emit(&body, 0x26);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 0);
        emit(&body, 0x25);
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
    if (err == WASM_OK) {
        CHECK_MSG(t, result.type == WASM_TYPE_ANYREF, "%s", "expected the declared anyref result type");
        CHECK_MSG(t, wasm__is_null_ref(&result), "%s", "expected table entry 0 to hold a null eqref");
    }

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_ref_eq_accepts_eqref_subtypes) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
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
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit(&body, 0x6B);
        emit(&body, 0xD0);
        emit(&body, 0x6D);
        emit(&body, 0xD3);
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
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_module_free(m);
    wasm_destroy(&rt);
}

TEST(test_gc_function_import_matches_host_signature_by_type_equality) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_module_t* types;
    wasm_import_t imp;
    wasm_valtype_t param_types[1] = { WASM_TYPE_ANYREF };
    wasm_valtype_t result_types[1] = { WASM_TYPE_STRUCTREF };
    wasm_reftype_t param_reftypes[1];
    wasm_reftype_t result_reftypes[1];
    wasm_value_t result;
    wasm_error_t err;

    memset(param_reftypes, 0, sizeof(param_reftypes));
    memset(result_reftypes, 0, sizeof(result_reftypes));

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 4);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);

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
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 2);
        emit(&sec, 'i');
        emit(&sec, 'd');
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 2);

        emit_section(&mod, 2, sec.buf, sec.len);
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
        emit_export_func(&sec, "run", 1);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD0);
        emit_leb128_u32(&body, 0);
        emit(&body, 0x10);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD1);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);

    memset(&imp, 0, sizeof(imp));
    param_reftypes[0].type = WASM_TYPE_ANYREF;
    param_reftypes[0].nullable = 1;
    result_reftypes[0].type = WASM_TYPE_STRUCTREF;
    result_reftypes[0].has_type_index = 1;
    result_reftypes[0].type_index = 1;
    result_reftypes[0].nullable = 1;
    imp.module = "env";
    imp.name = "id";
    imp.type.num_params = 1;
    imp.type.num_results = 1;
    imp.type.params = param_types;
    imp.type.results = result_types;
    imp.type.param_reftypes = param_reftypes;
    imp.type.result_reftypes = result_reftypes;
    imp.func = host_gc_subtype_identity;
    CHECK(t, wasm_register_import(&rt, &imp) == WASM_ERR_MALFORMED);

    result_types[0] = WASM_TYPE_NONE;
    memset(result_reftypes, 0, sizeof(result_reftypes));
    result_reftypes[0].type = WASM_TYPE_NONE;
    result_reftypes[0].nullable = 1;
    WASM_CHECK_OK(t, wasm_register_import(&rt, &imp));
    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "env.id") != NULL, "%s", rt.error_msg);
    wasm_module_free(m);

    types = wasm_compile(&rt, mod.buf, mod.len);
    CHECK_MSG(t, types != NULL, "%s", rt.error_msg);
    imp.type_module = types;
    imp.type_index = 2;
    imp.has_type_context = 1;
    err = wasm_register_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK || types == NULL) {
        wasm_module_free(types);
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_module_free(types);
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_module_free(m);
    wasm_module_free(types);
    wasm_destroy(&rt);
}

TEST(test_gc_global_import_accepts_subtype_binding) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_module_t* types;
    wasm_global_import_t imp;
    wasm_value_t imported = wasm_ref_null(WASM_TYPE_NONE);
    wasm_value_t result;
    wasm_error_t err;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x50);
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x5F);
        emit_leb128_u32(&sec, 0);

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
        emit(&sec, 'e');
        emit(&sec, 'n');
        emit(&sec, 'v');
        emit_leb128_u32(&sec, 1);
        emit(&sec, 'g');
        emit(&sec, 0x03);
        emit(&sec, 0x63);
        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x00);

        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 2);
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
        emit(&body, 0x23);
        emit_leb128_u32(&body, 0);
        emit(&body, 0xD1);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_init(&rt);
    types = wasm_compile(&rt, mod.buf, mod.len);
    CHECK_MSG(t, types != NULL, "%s", rt.error_msg);

    memset(&imp, 0, sizeof(imp));
    imp.module = "env";
    imp.name = "g";
    imp.type = WASM_TYPE_STRUCTREF;
    imp.ref_type.type = WASM_TYPE_STRUCTREF;
    imp.ref_type.has_type_index = 1;
    imp.ref_type.type_index = 1;
    imp.ref_type.nullable = 1;
    imp.type_module = types;
    imp.value = &imported;

    err = wasm_register_global_import(&rt, &imp);
    WASM_CHECK_OK(t, err);
    if (err != WASM_OK) {
        wasm_module_free(types);
        wasm_destroy(&rt);
        return;
    }

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_module_free(types);
        wasm_destroy(&rt);
        return;
    }

    err = wasm_call(m, "run", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 1);

    wasm_module_free(m);
    wasm_module_free(types);
    wasm_destroy(&rt);
}

TEST(test_exact_function_imports_link_only_the_exact_type) {
    /* (type $super (sub (func))) (type $sub (sub $super (func))) (func (export "f") (type $sub)) */
    static const uint8_t exporter_sections[] = {
        0x01, 0x0C, 0x02, 0x50, 0x00, 0x60, 0x00, 0x00, 0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x03, 0x02,
        0x01, 0x01, 0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00, 0x0A, 0x04, 0x01, 0x02, 0x00, 0x0B,
    };
    /* The same types and (import "a" "f" (func (exact (type $sub)))). */
    static const uint8_t exact_sub_sections[] = {
        0x01, 0x0C, 0x02, 0x50, 0x00, 0x60, 0x00, 0x00, 0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x02, 0x07,
        0x01, 0x01, 0x61, 0x01, 0x66, 0x20, 0x01,
    };
    /* The same types and (import "a" "f" (func (exact (type $super)))). */
    static const uint8_t exact_super_sections[] = {
        0x01, 0x0C, 0x02, 0x50, 0x00, 0x60, 0x00, 0x00, 0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x02, 0x07,
        0x01, 0x01, 0x61, 0x01, 0x66, 0x20, 0x00,
    };
    /* The same types and (import "a" "f" (func (type $super))). */
    static const uint8_t super_sections[] = {
        0x01, 0x0C, 0x02, 0x50, 0x00, 0x60, 0x00, 0x00, 0x50, 0x01, 0x00, 0x60, 0x00, 0x00, 0x02, 0x07,
        0x01, 0x01, 0x61, 0x01, 0x66, 0x00, 0x00,
    };
    wasm_runtime_t rt;
    wasm_module_t* exporter;
    wasm_module_t* m;

    wasm_init(&rt);
    exporter = wasm_test_load_sections(&rt, exporter_sections, sizeof(exporter_sections));
    CHECK_MSG(t, exporter != NULL, "%s", rt.error_msg);
    if (exporter == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_register_module(&rt, "a", exporter));
    m = wasm_test_load_sections(&rt, exact_sub_sections, sizeof(exact_sub_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    m = wasm_test_load_sections(&rt, super_sections, sizeof(super_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    m = wasm_test_load_sections(&rt, exact_super_sections, sizeof(exact_super_sections));
    CHECK_MSG(t, m == NULL, "%s", "expected an exact import of a supertype to be unlinkable");
    CHECK_MSG(t, strstr(rt.error_msg, "mismatch") != NULL, "%s", rt.error_msg);
    if (m != NULL) wasm_module_free(m);
    wasm_module_free(exporter);
    wasm_destroy(&rt);
}

/* (module
     (type $f (func)) (type $c (cont $f))
     (type $g (func (param i32))) (type $cg (cont $g))
     (func $nop) (elem declare func $nop)
     (func (export "make") (result (ref $c)) (cont.new $c (ref.func $nop)))
     (func (export "run") (param (ref null $c)) (resume $c (local.get 0)))
     (func (export "take-c") (param (ref null $c)) (result i32) (ref.is_null (local.get 0)))
     (func (export "take-cg") (param (ref null $cg)) (result i32) (ref.is_null (local.get 0)))
     (func (export "take-cont") (param contref) (result i32) (ref.is_null (local.get 0)))
     (func (export "take-nocont") (param nullcontref) (result i32) (ref.is_null (local.get 0)))) */
static const uint8_t wasm_test_references_cont_sections[] = {
    0x01, 0x2C, 0x0A, 0x60, 0x00, 0x00, 0x5D, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x5D, 0x02, 0x60, 0x00,
    0x01, 0x64, 0x01, 0x60, 0x01, 0x63, 0x01, 0x00, 0x60, 0x01, 0x63, 0x01, 0x01, 0x7F, 0x60, 0x01,
    0x63, 0x03, 0x01, 0x7F, 0x60, 0x01, 0x68, 0x01, 0x7F, 0x60, 0x01, 0x75, 0x01, 0x7F, 0x03, 0x08,
    0x07, 0x00, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x07, 0x3B, 0x06, 0x04, 0x6D, 0x61, 0x6B, 0x65,
    0x00, 0x01, 0x03, 0x72, 0x75, 0x6E, 0x00, 0x02, 0x06, 0x74, 0x61, 0x6B, 0x65, 0x2D, 0x63, 0x00,
    0x03, 0x07, 0x74, 0x61, 0x6B, 0x65, 0x2D, 0x63, 0x67, 0x00, 0x04, 0x09, 0x74, 0x61, 0x6B, 0x65,
    0x2D, 0x63, 0x6F, 0x6E, 0x74, 0x00, 0x05, 0x0B, 0x74, 0x61, 0x6B, 0x65, 0x2D, 0x6E, 0x6F, 0x63,
    0x6F, 0x6E, 0x74, 0x00, 0x06, 0x09, 0x05, 0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x2B, 0x07, 0x02,
    0x00, 0x0B, 0x06, 0x00, 0xD2, 0x00, 0xE0, 0x01, 0x0B, 0x07, 0x00, 0x20, 0x00, 0xE3, 0x01, 0x00,
    0x0B, 0x05, 0x00, 0x20, 0x00, 0xD1, 0x0B, 0x05, 0x00, 0x20, 0x00, 0xD1, 0x0B, 0x05, 0x00, 0x20,
    0x00, 0xD1, 0x0B, 0x05, 0x00, 0x20, 0x00, 0xD1, 0x0B,
};

static wasm_error_t wasm_test_take_cont(wasm_module_t* m, const char* func_name, wasm_value_t k) {
    wasm_value_t result;

    return wasm_call(m, func_name, &k, 1, &result, 1);
}

TEST(test_contrefs_at_the_host_boundary_match_their_type_until_consumed) {
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_value_t k;
    wasm_value_t forged;
    wasm_value_t* held;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, wasm_test_references_cont_sections, sizeof(wasm_test_references_cont_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(m, "make", NULL, 0, &k, 1));
    held = wasm_ref_hold(&rt, k);
    CHECK(t, held != NULL);
    WASM_CHECK_OK(t, wasm_test_take_cont(m, "take-c", k));
    WASM_CHECK_OK(t, wasm_test_take_cont(m, "take-cont", k));
    CHECK(t, wasm_test_take_cont(m, "take-cg", k) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_test_take_cont(m, "take-nocont", k) == WASM_ERR_TYPE_MISMATCH);
    WASM_CHECK_OK(t, wasm_call(m, "run", &k, 1, NULL, 0));

    WASM_CHECK_OK(t, wasm_test_take_cont(m, "take-c", k));
    WASM_CHECK_OK(t, wasm_test_take_cont(m, "take-cont", k));
    WASM_CHECK_OK(t, wasm_test_take_cont(m, "take-cg", k));
    CHECK(t, wasm_test_take_cont(m, "take-nocont", k) == WASM_ERR_TYPE_MISMATCH);
    CHECK(t, wasm_call(m, "run", &k, 1, NULL, 0) == WASM_ERR_TRAP);
    wasm_gc_collect(&rt);
    WASM_CHECK_OK(t, wasm_test_take_cont(m, "take-cg", k));

    forged = k;
    forged.of.externref += (uintptr_t)1u << WASM__CONT_GEN_SHIFT;
    CHECK(t, wasm_test_take_cont(m, "take-cont", forged) == WASM_ERR_TYPE_MISMATCH);
    if (held != NULL) wasm_ref_release(&rt, held);
    wasm_gc_collect(&rt);
    CHECK(t, wasm_test_take_cont(m, "take-cont", k) == WASM_ERR_TYPE_MISMATCH);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

static const test_case types_references_cases[] = {
    { "custom descriptors: exact function imports link only functions of exactly their type", test_exact_function_imports_link_only_the_exact_type },
    { "table64: import width mismatch is rejected", test_table64_import_width_mismatch },
    { "wasmgc: concrete heaptype parsing preserves typed refs across sections", test_gc_heaptype_parsing_tracks_concrete_type_refs },
    { "wasmgc: ref.is_null works on abstract anyref nulls", test_gc_ref_is_null_on_abstract_anyref },
    { "wasmgc: typed select accepts eqref where anyref is expected", test_gc_typed_select_accepts_eqref_as_anyref },
    { "wasmgc: anyref tables accept eqref values via subtyping", test_gc_table_set_accepts_eqref_into_anyref_table },
    { "wasmgc: ref.eq accepts eqref subtypes", test_gc_ref_eq_accepts_eqref_subtypes },
    { "wasmgc: function imports match host signatures by type equality", test_gc_function_import_matches_host_signature_by_type_equality },
    { "wasmgc: global imports accept GC subtype bindings", test_gc_global_import_accepts_subtype_binding },
    { "reference types: typed select chooses funcref operands", test_typed_select_funcref },
    { "stack switching: host contrefs match their type until consumed, then any continuation type", test_contrefs_at_the_host_boundary_match_their_type_until_consumed },
};
const test_suite types_references_suite = { "types.references", types_references_cases, COUNTOF(types_references_cases) };

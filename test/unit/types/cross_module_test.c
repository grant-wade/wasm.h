#include "../support.h"

TEST(test_stack_switching_contref_values_and_linking) {
    /* (module (type $f (func (param i32))) (type $c (cont $f))
         (global (ref null $c) (ref.null $c)) (table 1 contref)
         (func (export "nulls") (result i32)
           (i32.add (ref.is_null (table.get 0 (i32.const 0))) (ref.is_null (global.get 0))))
         (func (export "take") (param (ref null $c)))) */
    static const uint8_t exporter_sections[] = {
        0x01, 0x10, 0x04, 0x60, 0x01, 0x7F, 0x00, 0x5D, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x60, 0x01, 0x63,
        0x01, 0x00, 0x03, 0x03, 0x02, 0x02, 0x03, 0x04, 0x04, 0x01, 0x68, 0x00, 0x01, 0x06, 0x07, 0x01,
        0x63, 0x01, 0x00, 0xD0, 0x01, 0x0B, 0x07, 0x10, 0x02, 0x05, 0x6E, 0x75, 0x6C, 0x6C, 0x73, 0x00,
        0x00, 0x04, 0x74, 0x61, 0x6B, 0x65, 0x00, 0x01, 0x0A, 0x10, 0x02, 0x0B, 0x00, 0x41, 0x00, 0x25,
        0x00, 0xD1, 0x23, 0x00, 0xD1, 0x6A, 0x0B, 0x02, 0x00, 0x0B,
    };
    /* (module (type (func)) (type $f (func (param i32))) (type $c (cont $f))
         (import "x" "take" (func (param (ref null $c))))) */
    static const uint8_t matching_sections[] = {
        0x01, 0x0F, 0x04, 0x60, 0x00, 0x00, 0x60, 0x01, 0x7F, 0x00, 0x5D, 0x01, 0x60, 0x01, 0x63, 0x02,
        0x00, 0x02, 0x0A, 0x01, 0x01, 0x78, 0x04, 0x74, 0x61, 0x6B, 0x65, 0x00, 0x03,
    };
    /* (module (type $f (func (param i64))) (type $c (cont $f))
         (import "x" "take" (func (param (ref null $c))))) */
    static const uint8_t mismatched_sections[] = {
        0x01, 0x0C, 0x03, 0x60, 0x01, 0x7E, 0x00, 0x5D, 0x00, 0x60, 0x01, 0x63, 0x01, 0x00, 0x02, 0x0A,
        0x01, 0x01, 0x78, 0x04, 0x74, 0x61, 0x6B, 0x65, 0x00, 0x02,
    };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_module_t* importer;
    wasm_value_t result;

    wasm_init(&rt);
    m = wasm_test_load_sections(&rt, exporter_sections, sizeof(exporter_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(m, "nulls", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 2);
    WASM_CHECK_OK(t, wasm_register_module(&rt, "x", m));
    importer = wasm_test_load_sections(&rt, matching_sections, sizeof(matching_sections));
    CHECK_MSG(t, importer != NULL, "%s", rt.error_msg);
    if (importer != NULL) wasm_module_free(importer);
    importer = wasm_test_load_sections(&rt, mismatched_sections, sizeof(mismatched_sections));
    CHECK_MSG(t, importer == NULL, "%s", "expected a different continuation type to fail to link");
    if (importer != NULL) wasm_module_free(importer);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

/* (module (type $f (func (param i32) (result i32))) (type $c (cont $f))
     (func $triple (type $f) (i32.mul (local.get 0) (i32.const 3))) (elem declare func $triple)
     (func (export "make") (result (ref $c)) (cont.new $c (ref.func $triple)))) */
static const uint8_t wasm_test_cont_xmod_maker_sections[] = {
    0x01, 0x0D, 0x03, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x00, 0x60, 0x00, 0x01, 0x64, 0x01, 0x03,
    0x03, 0x02, 0x00, 0x02, 0x07, 0x08, 0x01, 0x04, 0x6D, 0x61, 0x6B, 0x65, 0x00, 0x01, 0x09, 0x05,
    0x01, 0x03, 0x00, 0x01, 0x00, 0x0A, 0x10, 0x02, 0x07, 0x00, 0x20, 0x00, 0x41, 0x03, 0x6C, 0x0B,
    0x06, 0x00, 0xD2, 0x00, 0xE0, 0x01, 0x0B,
};

/* (module (type $s0 (struct (field i64))) (type $f (func (param i32) (result i32))) (type $c (cont $f))
     (type $s (struct (field (mut (ref null $c))))) (type $a (array (mut (ref null $c))))
     (global $g (mut (ref null $c)) (ref.null $c)) (table $t 2 (ref null $c))
     (global $gs (ref $s) (struct.new_default $s)) (global $ga (ref $a) (array.new_default $a (i32.const 1)))
     (func (export "store") (param $k (ref null $c)) (param $n i32)
       (loop $l
         (global.set $g (local.get $k)) (table.set $t (i32.const 0) (local.get $k))
         (table.fill $t (i32.const 1) (local.get $k) (i32.const 1))
         (struct.set $s 0 (global.get $gs) (local.get $k)) (array.set $a (global.get $ga) (i32.const 0) (local.get $k))
         (br_if $l (local.tee $n (i32.sub (local.get $n) (i32.const 1))))))
     (func (export "take") (param (ref null $c)) (global.set $g (local.get 0)))
     (func (export "run") (result i32) (resume $c (i32.const 5) (global.get $g)))) */
static const uint8_t wasm_test_cont_xmod_store_sections[] = {
    0x01, 0x24, 0x08, 0x5F, 0x01, 0x7E, 0x00, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x5D, 0x01, 0x5F, 0x01,
    0x63, 0x02, 0x01, 0x5E, 0x63, 0x02, 0x01, 0x60, 0x02, 0x63, 0x02, 0x7F, 0x00, 0x60, 0x01, 0x63,
    0x02, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x03, 0x04, 0x03, 0x05, 0x06, 0x07, 0x04, 0x05, 0x01, 0x63,
    0x02, 0x00, 0x02, 0x06, 0x17, 0x03, 0x63, 0x02, 0x01, 0xD0, 0x02, 0x0B, 0x64, 0x03, 0x00, 0xFB,
    0x01, 0x03, 0x0B, 0x64, 0x04, 0x00, 0x41, 0x01, 0xFB, 0x07, 0x04, 0x0B, 0x07, 0x16, 0x03, 0x05,
    0x73, 0x74, 0x6F, 0x72, 0x65, 0x00, 0x00, 0x04, 0x74, 0x61, 0x6B, 0x65, 0x00, 0x01, 0x03, 0x72,
    0x75, 0x6E, 0x00, 0x02, 0x0A, 0x45, 0x03, 0x32, 0x00, 0x03, 0x40, 0x20, 0x00, 0x24, 0x00, 0x41,
    0x00, 0x20, 0x00, 0x26, 0x00, 0x41, 0x01, 0x20, 0x00, 0x41, 0x01, 0xFC, 0x11, 0x00, 0x23, 0x01,
    0x20, 0x00, 0xFB, 0x05, 0x03, 0x00, 0x23, 0x02, 0x41, 0x00, 0x20, 0x00, 0xFB, 0x0E, 0x04, 0x20,
    0x01, 0x41, 0x01, 0x6B, 0x22, 0x01, 0x0D, 0x00, 0x0B, 0x0B, 0x06, 0x00, 0x20, 0x00, 0x24, 0x00,
    0x0B, 0x09, 0x00, 0x41, 0x05, 0x23, 0x00, 0xE3, 0x02, 0x00, 0x0B,
};

TEST(test_stack_switching_cross_module_stores_do_not_allocate) {
    wasm_runtime_t rt;
    wasm_module_t* maker;
    wasm_module_t* store;
    wasm_value_t args[2];
    wasm_value_t result;
    size_t alloc_calls;

    wasm_init(&rt);
    maker = wasm_test_load_cont(&rt, wasm_test_cont_xmod_maker_sections, sizeof(wasm_test_cont_xmod_maker_sections));
    store = wasm_test_load_cont(&rt, wasm_test_cont_xmod_store_sections, sizeof(wasm_test_cont_xmod_store_sections));
    CHECK_MSG(t, maker != NULL && store != NULL, "%s", rt.error_msg);
    if (maker == NULL || store == NULL) {
        wasm_module_free(store);
        wasm_module_free(maker);
        wasm_destroy(&rt);
        return;
    }
    WASM_CHECK_OK(t, wasm_call(maker, "make", NULL, 0, &args[0], 1));
    args[1] = wasm_i32(1);
    WASM_CHECK_OK(t, wasm_call(store, "store", args, 2, NULL, 0));
    alloc_calls = wasm_test_alloc_snapshot().alloc_calls;
    args[1] = wasm_i32(100000);
    WASM_CHECK_OK(t, wasm_call(store, "store", args, 2, NULL, 0));
    WASM_CHECK_OK(t, wasm_call(store, "take", args, 1, NULL, 0));
    CHECK_MSG(t, wasm_test_alloc_snapshot().alloc_calls == alloc_calls, "%zu allocations",
              wasm_test_alloc_snapshot().alloc_calls - alloc_calls);
    WASM_CHECK_OK(t, wasm_call(store, "run", NULL, 0, &result, 1));
    WASM_CHECK_I32(t, result.of.i32, 15);
    wasm_module_free(store);
    wasm_module_free(maker);
    wasm_destroy(&rt);
}

/* (module (type $pad (func (param f64))) (type $s (struct (field i32)))
     (global (export "g") (ref null $s) (struct.new $s (i32.const 7))) (table (export "t") 1 (ref null $s))) */
static const uint8_t wasm_test_typed_global_lib_sections[] = {
    0x01, 0x09, 0x02, 0x60, 0x01, 0x7C, 0x00, 0x5F, 0x01, 0x7F, 0x00, 0x04, 0x05, 0x01, 0x63, 0x01,
    0x00, 0x01, 0x06, 0x0A, 0x01, 0x63, 0x01, 0x00, 0x41, 0x07, 0xFB, 0x00, 0x01, 0x0B, 0x07, 0x09,
    0x02, 0x01, 0x67, 0x03, 0x00, 0x01, 0x74, 0x01, 0x00,
};

/* (module (type $t (struct (field i32)))
     (import "host" "g" (global $g (ref null $t))) (import "host" "t" (table 1 (ref null $t)))
     (func (export "get") (result i32) (struct.get $t 0 (global.get $g)))) */
static const uint8_t wasm_test_typed_global_user_sections[] = {
    0x01, 0x09, 0x02, 0x5F, 0x01, 0x7F, 0x00, 0x60, 0x00, 0x01, 0x7F, 0x02, 0x18, 0x02, 0x04, 0x68,
    0x6F, 0x73, 0x74, 0x01, 0x67, 0x03, 0x63, 0x00, 0x00, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x01, 0x74,
    0x01, 0x63, 0x00, 0x00, 0x01, 0x03, 0x02, 0x01, 0x01, 0x07, 0x07, 0x01, 0x03, 0x67, 0x65, 0x74,
    0x00, 0x00, 0x0A, 0x0A, 0x01, 0x08, 0x00, 0x23, 0x00, 0xFB, 0x02, 0x00, 0x00, 0x0B,
};

/* (module (type $t (struct (field i64))) (import "host" "g" (global (ref null $t)))) */
static const uint8_t wasm_test_typed_global_wrong_sections[] = {
    0x01, 0x05, 0x01, 0x5F, 0x01, 0x7E, 0x00, 0x02, 0x0C, 0x01, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x01,
    0x67, 0x03, 0x63, 0x00, 0x00,
};

/* (module (type $t (struct (field i32))) (import "host" "g" (global (ref null $t)))) */
static const uint8_t wasm_test_typed_global_only_sections[] = {
    0x01, 0x05, 0x01, 0x5F, 0x01, 0x7F, 0x00, 0x02, 0x0C, 0x01, 0x04, 0x68, 0x6F, 0x73, 0x74, 0x01,
    0x67, 0x03, 0x63, 0x00, 0x00,
};

TEST(test_typed_global_and_table_imports_use_their_module_types) {
    wasm_runtime_t rt;
    wasm_global_import_t global_imp;
    wasm_table_import_t table_imp;
    wasm_module_t* lib;
    wasm_module_t* m;

    wasm_init(&rt);
    lib = wasm_test_load_sections(&rt, wasm_test_typed_global_lib_sections, sizeof(wasm_test_typed_global_lib_sections));
    CHECK_MSG(t, lib != NULL, "%s", rt.error_msg);
    if (lib == NULL) {
        wasm_destroy(&rt);
        return;
    }

    memset(&global_imp, 0, sizeof(global_imp));
    global_imp.module = "host";
    global_imp.name = "g";
    global_imp.type = wasm_global_type(lib, 0);
    global_imp.ref_type = *wasm_global_reftype(lib, 0);
    global_imp.value = wasm_module_global(lib, 0);
    CHECK(t, wasm_register_global_import(&rt, &global_imp) == WASM_ERR_MALFORMED);
    global_imp.type_module = lib;
    WASM_CHECK_OK(t, wasm_register_global_import(&rt, &global_imp));
    memset(&table_imp, 0, sizeof(table_imp));
    table_imp.module = "host";
    table_imp.name = "t";
    table_imp.table = wasm_module_table(lib, 0);
    WASM_CHECK_OK(t, wasm_register_table_import(&rt, &table_imp));

    m = wasm_test_load_sections(&rt, wasm_test_typed_global_user_sections, sizeof(wasm_test_typed_global_user_sections));
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m != NULL) {
        wasm_value_t result;

        WASM_CHECK_OK(t, wasm_call(m, "get", NULL, 0, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 7);
    }
    wasm_module_free(m);
    m = wasm_test_load_sections(&rt, wasm_test_typed_global_wrong_sections, sizeof(wasm_test_typed_global_wrong_sections));
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "host.g") != NULL, "%s", rt.error_msg);
    wasm_module_free(m);

    wasm_module_free(lib);
    m = wasm_test_load_sections(&rt, wasm_test_typed_global_only_sections, sizeof(wasm_test_typed_global_only_sections));
    CHECK_MSG(t, m == NULL && strstr(rt.error_msg, "host.g") != NULL, "%s", rt.error_msg);
    wasm_module_free(m);
    wasm_destroy(&rt);
}

#define WASM_TEST_CHAIN_TYPES 100u

static void wasm_test_chain_section(uint8_t* out, uint32_t* len, uint8_t id, const uint8_t* body, uint32_t body_len) {
    out[(*len)++] = id;
    out[(*len)++] = (uint8_t)(0x80u | (body_len & 0x7Fu));
    out[(*len)++] = (uint8_t)(body_len >> 7);
    memcpy(out + *len, body, body_len);
    *len += body_len;
}

/* (type $t0 (struct (field i32))) and (type $ti (struct (field (ref null $ti-1)) (field (ref null $ti-1)))) up to
   $t99, then a module exporting "mk" () -> anyref making a $t99 when is_maker, or "cast" (anyref) -> i32 testing
   (ref (exact $t99)) otherwise. */
static uint32_t wasm_test_chain_module(uint8_t* out, int is_maker) {
    static const uint8_t maker_tail[] = { 0x03, 0x02, 0x01, 0x64, 0x07, 0x06, 0x01, 0x02, 0x6D, 0x6B, 0x00, 0x00 };
    static const uint8_t caster_tail[] = { 0x03, 0x02, 0x01, 0x65, 0x07, 0x08, 0x01, 0x04, 0x63, 0x61, 0x73, 0x74,
                                           0x00, 0x00 };
    uint8_t types[1024];
    uint8_t code[16];
    uint32_t types_len = 0;
    uint32_t code_len = 0;
    uint32_t len = 0;
    uint32_t i;

    types[types_len++] = (uint8_t)(WASM_TEST_CHAIN_TYPES + 2u);
    types[types_len++] = 0x5F;
    types[types_len++] = 0x01;
    types[types_len++] = 0x7F;
    types[types_len++] = 0x00;
    for (i = 1; i < WASM_TEST_CHAIN_TYPES; i++) {
        uint32_t field;

        types[types_len++] = 0x5F;
        types[types_len++] = 0x02;
        for (field = 0; field < 2u; field++) {
            types[types_len++] = 0x63;
            if (i - 1u < 64u) {
                types[types_len++] = (uint8_t)(i - 1u);
            } else {
                types[types_len++] = (uint8_t)(0x80u | ((i - 1u) & 0x7Fu));
                types[types_len++] = (uint8_t)((i - 1u) >> 7);
            }
            types[types_len++] = 0x00;
        }
    }
    memcpy(types + types_len, "\x60\x00\x01\x6E\x60\x01\x6E\x01\x7F", 9);
    types_len += 9;
    wasm_test_chain_section(out, &len, 0x01, types, types_len);
    memcpy(out + len, is_maker ? maker_tail : caster_tail, is_maker ? sizeof(maker_tail) : sizeof(caster_tail));
    len += is_maker ? sizeof(maker_tail) : sizeof(caster_tail);
    code[code_len++] = 0x01;
    code[code_len++] = is_maker ? 0x05 : 0x08;
    code[code_len++] = 0x00;
    if (is_maker) {
        memcpy(code + code_len, "\xFB\x01", 2);
        code_len += 2;
    } else {
        memcpy(code + code_len, "\x20\x00\xFB\x14\x62", 5);
        code_len += 5;
    }
    code[code_len++] = (uint8_t)(WASM_TEST_CHAIN_TYPES - 1u);
    code[code_len++] = 0x0B;
    out[len++] = 0x0A;
    out[len++] = (uint8_t)code_len;
    memcpy(out + len, code, code_len);
    return len + code_len;
}

TEST(test_cross_module_exact_casts_compare_canonical_ids) {
    uint8_t maker_bytes[1100];
    uint8_t caster_bytes[1100];
    uint32_t maker_len = wasm_test_chain_module(maker_bytes, 1);
    uint32_t caster_len = wasm_test_chain_module(caster_bytes, 0);
    wasm_runtime_t rt;
    wasm_module_t* maker;
    wasm_module_t* caster;
    wasm_value_t object;
    wasm_value_t result;
    size_t alloc_calls;
    int i;

    wasm_init(&rt);
    maker = wasm_test_load_sections(&rt, maker_bytes, maker_len);
    caster = wasm_test_load_sections(&rt, caster_bytes, caster_len);
    CHECK_MSG(t, maker != NULL && caster != NULL, "%s", rt.error_msg);
    if (maker == NULL || caster == NULL) {
        wasm_module_free(caster);
        wasm_module_free(maker);
        wasm_destroy(&rt);
        return;
    }
    CHECK(t, maker->types[WASM_TEST_CHAIN_TYPES - 1u].canonical_id ==
                 caster->types[WASM_TEST_CHAIN_TYPES - 1u].canonical_id);
    CHECK(t, rt.num_type_groups == WASM_TEST_CHAIN_TYPES + 2u);
    WASM_CHECK_OK(t, wasm_call(maker, "mk", NULL, 0, &object, 1));
    alloc_calls = wasm_test_alloc_snapshot().alloc_calls;
    for (i = 0; i < 100; i++) {
        WASM_CHECK_OK(t, wasm_call(caster, "cast", &object, 1, &result, 1));
        WASM_CHECK_I32(t, result.of.i32, 1);
    }
    CHECK_MSG(t, wasm_test_alloc_snapshot().alloc_calls == alloc_calls, "%zu allocations",
              wasm_test_alloc_snapshot().alloc_calls - alloc_calls);
    wasm_module_free(caster);
    CHECK(t, rt.num_type_groups == WASM_TEST_CHAIN_TYPES + 2u);
    wasm_module_free(maker);
    wasm_gc_collect(&rt);
    CHECK(t, rt.num_type_groups == 0u);
    wasm_destroy(&rt);
}

static const test_case types_cross_module_cases[] = {
    { "stack switching: contref nulls in globals and tables, continuation types match across modules", test_stack_switching_contref_values_and_linking },
    { "imports: typed global and table imports use their module's types", test_typed_global_and_table_imports_use_their_module_types },
    { "stack switching: Wasm and host stores of another module's continuations do not allocate", test_stack_switching_cross_module_stores_do_not_allocate },
    { "types: exact casts across modules compare canonical ids without allocating", test_cross_module_exact_casts_compare_canonical_ids },
};
const test_suite types_cross_module_suite = { "types.cross_module", types_cross_module_cases, COUNTOF(types_cross_module_cases) };

#include "internal.h"

wasm_module_t* wasm_load(wasm_runtime_t* rt, const uint8_t* bytes, size_t len) {
    wasm__reader_t r;
    uint32_t version;
    wasm_module_t* mod;
    wasm_error_t err = WASM_OK;

    if (!rt) return NULL;
    if (!bytes) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s", "null module bytes");
        return NULL;
    }

    r.ptr = bytes;
    r.end = bytes + len;
    r.malformed = 0;
    if (!wasm__has(&r, 8)) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "too short");
        return NULL;
    }
    if (r.ptr[0] != 0x00 || r.ptr[1] != 0x61 || r.ptr[2] != 0x73 || r.ptr[3] != 0x6D) {
        WASM__SET_ERR(rt, WASM_ERR_INVALID_MAGIC, "bad magic");
        return NULL;
    }
    r.ptr += 4;
    version = (uint32_t)r.ptr[0] | ((uint32_t)r.ptr[1] << 8) | ((uint32_t)r.ptr[2] << 16) | ((uint32_t)r.ptr[3] << 24);
    r.ptr += 4;
    if (version != 1) {
        WASM__SET_ERR(rt, WASM_ERR_INVALID_VERSION, "version %u", version);
        return NULL;
    }

    mod = (wasm_module_t*)WASM_CALLOC(1, sizeof(wasm_module_t));
    if (!mod) {
        WASM__SET_ERR(rt, WASM_ERR_OOM, "module alloc");
        return NULL;
    }
    mod->rt = rt;
    mod->start_func = UINT32_MAX;
    if (len > 0) {
        mod->module_bytes = (uint8_t*)WASM_MALLOC(len);
        if (!mod->module_bytes) {
            WASM__SET_ERR(rt, WASM_ERR_OOM, "module bytes alloc");
            WASM_FREE(mod);
            return NULL;
        }
        memcpy(mod->module_bytes, bytes, len);
        mod->module_size = len;
        r.ptr = mod->module_bytes + 8;
        r.end = mod->module_bytes + len;
        r.malformed = 0;
    }
    wasm__gc_register_module(rt, mod);

#define WASM__LOAD_FAIL(stage_literal)                                               \
    do {                                                                             \
        if (rt->last_error == WASM_OK)                                               \
            WASM__SET_ERR(rt, err, "%s: %s", stage_literal, wasm_error_string(err)); \
        if (mod->preserve_on_failure) {                                              \
            mod->failed_next = rt->failed_modules;                                   \
            rt->failed_modules = mod;                                                \
        } else {                                                                     \
            wasm_free_module(mod);                                                   \
        }                                                                            \
        return NULL;                                                                 \
    } while (0)

    while (r.ptr < r.end && err == WASM_OK) {
        uint8_t sid = wasm__read_u8(&r);
        uint32_t ssz = wasm__read_leb128_u32(&r);
        const uint8_t* se;
        wasm__reader_t sr;

        if (r.malformed) {
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "malformed section header");
            err = WASM_ERR_MALFORMED;
            break;
        }

        if (!wasm__has(&r, ssz)) {
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "section %u overruns module", (unsigned)sid);
            err = WASM_ERR_MALFORMED;
            break;
        }
        se = r.ptr + ssz;
        sr.ptr = r.ptr;
        sr.end = se;
        sr.malformed = 0;
        switch (sid) {
            case 0:
                err = wasm__decode_custom_section(mod, &sr);
                break;
            case 1:
                err = wasm__decode_type_section(mod, &sr);
                break;
            case 2:
                err = wasm__decode_import_section(mod, &sr);
                break;
            case 3:
                err = wasm__decode_function_section(mod, &sr);
                break;
            case 4:
                err = wasm__decode_table_section(mod, &sr);
                break;
            case 5:
                err = wasm__decode_memory_section(mod, &sr);
                break;
            case 6:
                err = wasm__decode_global_section(mod, &sr);
                break;
            case 7:
                err = wasm__decode_export_section(mod, &sr);
                break;
            case 8:
                err = wasm__decode_start_section(mod, &sr);
                break;
            case 9:
                err = wasm__decode_element_section(mod, &sr);
                break;
            case 10:
                err = wasm__decode_code_section(mod, &sr);
                break;
            case 11:
                err = wasm__decode_data_section(mod, &sr);
                break;
            case 12:
                err = wasm__decode_data_count_section(mod, &sr);
                break;
            case 13:
                err = wasm__decode_tag_section(mod, &sr);
                break;
            default:
                WASM__SET_ERR(rt, WASM_ERR_INVALID_SECTION, "unknown section id %u", (unsigned)sid);
                err = WASM_ERR_INVALID_SECTION;
                break;
        }
        if (err != WASM_OK && rt->last_error == WASM_OK) {
            WASM__SET_ERR(rt, err, "section %u decode failed: %s",
                          (unsigned)sid, wasm_error_string(err));
        }
        if (err == WASM_OK && (sr.malformed || sr.ptr != sr.end)) {
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "section %u has trailing bytes", (unsigned)sid);
            err = WASM_ERR_MALFORMED;
        }
        r.ptr = se;
    }
    if (err != WASM_OK) {
        wasm_free_module(mod);
        return NULL;
    }

    err = wasm__resolve_module_reftypes(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("resolve_module_reftypes");
    }

    err = wasm__canonicalize_types(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("canonicalize_types");
    }

    err = wasm__validate_types(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("validate_types");
    }

    err = wasm__validate_structural(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("validate_structural");
    }

    err = wasm__validate_code(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("validate_code");
    }

    if (!rt->lazy_validation) {
        err = wasm__build_module_control_targets(mod);
        if (err != WASM_OK) {
            WASM__LOAD_FAIL("build_control_targets");
        }
    }

    err = wasm__check_unresolved_function_imports(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("resolve_function_imports");
    }

    if (mod->num_memories > 0) {
        uint32_t mi;

        for (mi = 0; mi < mod->num_memories; mi++) {
            err = wasm__init_memory_storage(&mod->memories[mi]);
            if (err != WASM_OK) {
                WASM__LOAD_FAIL("init_memory_storage");
            }
        }
    }
    if (mod->num_tables > 0) {
        uint32_t ti;
        for (ti = 0; ti < mod->num_tables; ti++) {
            err = wasm__init_table_storage(&mod->tables[ti]);
            if (err != WASM_OK) {
                WASM__LOAD_FAIL("init_table_storage");
            }
        }
    }

    err = wasm__apply_active_elem_segments(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("apply_active_elem_segments");
    }

    err = wasm__apply_active_data_segments(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("apply_active_data_segments");
    }

    err = wasm__run_startup(mod);
    if (err != WASM_OK) {
        WASM__LOAD_FAIL("run_startup");
    }

#undef WASM__LOAD_FAIL

    return mod;
}

void wasm_free_module(wasm_module_t* mod) {
    wasm_gc_header_t* object;
    uint32_t i;
    if (!mod) return;
    if (mod->rt) {
        for (object = mod->rt->gc_allocations; object; object = object->next_alloc) {
            if (object->module == mod) object->module = NULL;
        }
        wasm__gc_unregister_module(mod->rt, mod);
    }
    for (i = 0; i < mod->num_types; i++) wasm__free_comptype(&mod->types[i]);
    WASM_FREE(mod->types);
    WASM_FREE(mod->rec_groups);
    for (i = 0; i < mod->num_funcs; i++) {
        WASM_FREE(mod->funcs[i].locals);
        WASM_FREE(mod->funcs[i].local_reftypes);
        WASM_FREE(mod->funcs[i].control_targets);
    }
    WASM_FREE(mod->funcs);
    for (i = 0; i < mod->num_imports; i++) wasm__free_import_info(&mod->imports[i]);
    WASM_FREE(mod->imports);
    for (i = 0; i < mod->num_exports; i++) wasm__free_export(&mod->exports[i]);
    WASM_FREE(mod->exports);
    WASM_FREE(mod->globals);
    WASM_FREE(mod->tags);
    for (i = 0; i < mod->num_custom_sections; i++) wasm__free_custom_section(&mod->custom_sections[i]);
    WASM_FREE(mod->custom_sections);
    for (i = 0; i < mod->num_data_segments; i++) wasm__free_data_segment(&mod->data_segments[i]);
    WASM_FREE(mod->data_segments);
    for (i = 0; i < mod->num_elem_segments; i++) wasm__free_elem_segment(&mod->elem_segments[i]);
    WASM_FREE(mod->elem_segments);
    for (i = 0; i < mod->num_tables; i++) wasm__free_table(&mod->tables[i]);
    WASM_FREE(mod->tables);
    for (i = 0; i < mod->num_memories; i++) wasm__free_memory(&mod->memories[i]);
    WASM_FREE(mod->memories);
    WASM_FREE(mod->module_bytes);
    WASM_FREE(mod);
}

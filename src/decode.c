#include "decode.h"
#include "memory_internal.h"
#include "values.h"
#include "reader.h"
#include "module.h"
#include "types/references.h"
#include "runtime.h"
#include "types/decode.h"
#include "platform/emscripten.h"
#include "types/canonical.h"
#include "portable.h"
#include "init_expr.h"
#include "types/cross_module.h"
#include "validate/module.h"
#include "internal/config.h"
#include "internal/alloc.h"
#include <string.h>
#include <stdio.h>

static wasm_error_t wasm__read_elemkind(wasm__reader_t* r) {
    if (wasm__read_u8(r) != 0x00) return WASM_ERR_MALFORMED;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_type_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t entry_count = wasm__read_count(r);
    uint32_t entry_index;

    for (entry_index = 0; entry_index < entry_count; entry_index++) {
        uint8_t opcode = wasm__read_u8(r);
        uint32_t group_index;
        uint32_t first_type;
        uint32_t group_size = 1;
        uint32_t i;
        wasm_error_t err;

        if (opcode == 0x4E) {
            err = wasm__require_feature(mod, WASM_FEATURE_GC);
            if (err != WASM_OK) return err;
            group_size = wasm__read_count(r);
        }

        err = wasm__append_rec_groups(mod, 1, &group_index);
        if (err != WASM_OK) return err;
        err = wasm__append_types(mod, group_size, &first_type);
        if (err != WASM_OK) return err;

        mod->rec_groups[group_index].first_type = first_type;
        mod->rec_groups[group_index].num_types = group_size;

        for (i = 0; i < group_size; i++) {
            wasm_comptype_t* type = &mod->types[first_type + i];
            uint8_t entry_opcode = (opcode == 0x4E) ? wasm__read_u8(r) : opcode;

            err = wasm__decode_type_entry(mod, r, entry_opcode, group_index, type);
            if (err != WASM_OK) return err;
        }
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_custom_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t section_index;
    wasm_custom_section_t* section;
    wasm_error_t err;

    err = wasm__append_custom_sections(mod, 1, &section_index);
    if (err != WASM_OK) return err;

    section = &mod->custom_sections[section_index];
    err = wasm__read_name_owned(&mod->rt->allocator, r, &section->name);
    if (err != WASM_OK) return err;

    section->data = r->ptr;
    section->size = (uint32_t)(r->end - r->ptr);
    r->ptr = r->end;
    return WASM_OK;
}

static wasm_error_t wasm__read_memory_type(wasm_module_t* mod,
                                           wasm__reader_t* r,
                                           wasm_memory_t* memory) {
    uint8_t flags = wasm__read_u8(r);
    int has_max = (flags & 0x01u) != 0;
    int is_shared = (flags & 0x02u) != 0;
    int is_64 = (flags & 0x04u) != 0;
    int has_custom_page_size = (flags & 0x08u) != 0;

    if ((flags & ~0x0Fu) != 0) return WASM_ERR_MALFORMED;
    if (is_shared) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_THREADS);
        if (err != WASM_OK) return err;
    }
    if (is_64) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_MEMORY64);
        if (err != WASM_OK) return err;
    }
    if (has_custom_page_size) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_CUSTOM_PAGE_SIZES);
        if (err != WASM_OK) return err;
    }

    memset(memory, 0, sizeof(*memory));
    memory->allocator = mod->rt->allocator;
    memory->is_64 = is_64;
    memory->is_shared = is_shared;
    memory->pages = wasm__read_leb128_u64(r);
    if (has_max) memory->max_pages = wasm__read_leb128_u64(r);
    if (has_custom_page_size) {
        uint32_t page_size_log2 = wasm__read_leb128_u32(r);
        if (r->malformed) return WASM_ERR_MALFORMED;
        if (page_size_log2 >= 64u) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid custom page size");
            return WASM_ERR_MALFORMED;
        }
        if (page_size_log2 != 0u && page_size_log2 != 16u)
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0, "%s", "invalid custom page size");
        memory->page_size = 1u << page_size_log2;
    }
    if (!has_max) memory->max_pages = wasm__memory_page_limit(memory);

    if (r->malformed) return WASM_ERR_MALFORMED;
    if (is_shared && !has_max)
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0, "%s", "shared memory must have maximum");
    return WASM_OK;
}

static wasm_error_t wasm__read_table_type(wasm_module_t* mod,
                                          wasm__reader_t* r,
                                          wasm_table_t* table) {
    uint8_t leading;
    uint8_t init_flags = 0;
    uint8_t flags;
    int has_max;
    int is_shared;
    int is_64;
    int has_init_expr = 0;
    wasm_error_t err;

    memset(table, 0, sizeof(*table));
    table->allocator = mod->rt->allocator;
    leading = wasm__read_u8(r);
    if (leading == 0x40u) {
        has_init_expr = 1;
        init_flags = wasm__read_u8(r);
        if (init_flags != 0x00u) return WASM_ERR_MALFORMED;
        err = wasm__read_reftype(mod, r, &table->reftype, &table->reftype_info);
        if (err != WASM_OK) return err;
        flags = wasm__read_u8(r);
    } else if (leading == 0x70u) {
        table->reftype = WASM_TYPE_FUNCREF;
        table->reftype_info.type = WASM_TYPE_FUNCREF;
        table->reftype_info.nullable = 1;
        flags = wasm__read_u8(r);
    } else {
        r->ptr--;
        err = wasm__read_reftype(mod, r, &table->reftype, &table->reftype_info);
        if (err != WASM_OK) return err;
        flags = wasm__read_u8(r);
    }

    has_max = (flags & 0x01u) != 0;
    is_shared = (flags & 0x02u) != 0;
    is_64 = (flags & 0x04u) != 0;

    if ((flags & ~0x07u) != 0) return WASM_ERR_MALFORMED;
    if (is_shared) return WASM_ERR_MALFORMED;
    if (is_64) {
        err = wasm__require_feature(mod, WASM_FEATURE_MEMORY64);
        if (err != WASM_OK) return err;
    }

    table->is_64 = is_64;
    if (is_64) {
        uint64_t min_size = wasm__read_leb128_u64(r);

        table->size = (uint32_t)min_size;
        table->max_size = has_max ? wasm__read_leb128_u64(r) : UINT64_MAX;
        if (min_size > UINT32_MAX) return WASM_ERR_OOM;
    } else {
        uint64_t min_size = wasm__read_leb128_u64(r);
        uint64_t max_size = has_max ? wasm__read_leb128_u64(r) : UINT32_MAX;

        if (r->malformed) return WASM_ERR_MALFORMED;
        if (min_size > UINT32_MAX || max_size > UINT32_MAX) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "table size");
            return WASM_ERR_MALFORMED;
        }
        table->size = (uint32_t)min_size;
        table->max_size = max_size;
    }

    table->default_value = wasm__typed_ref_null(table->reftype, &table->reftype_info);
    if (has_init_expr) {
        wasm_value_t init_value;

        err = wasm__eval_init_expr(mod, r, mod->num_globals, &init_value);
        if (err != WASM_OK) return err;
        if (!wasm__runtime_value_matches_type(mod, &init_value, table->reftype, &table->reftype_info))
            return WASM_ERR_TYPE_MISMATCH;
        table->default_value = init_value;
    } else if (!wasm__runtime_value_matches_type(mod,
                                                 &table->default_value,
                                                 table->reftype,
                                                 &table->reftype_info)) {
        return WASM_ERR_TYPE_MISMATCH;
    }

    if (r->malformed) return WASM_ERR_MALFORMED;
    return WASM_OK;
}

static wasm_error_t wasm__decode_import_desc(wasm_module_t* mod,
                                             wasm__reader_t* r,
                                             wasm_import_info_t* info,
                                             uint8_t kind) {
    if (kind == 0x00) {
        uint32_t ti = wasm__read_leb128_u32(r);
        uint32_t fi;
        const wasm_functype_t* import_type = wasm__module_const_functype(mod, ti);
        uint32_t j;

        if (!import_type) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown type %u", (unsigned)ti);
            return WASM_ERR_MALFORMED;
        }
        if (wasm__append_funcs(mod, 1, &fi) != WASM_OK) return WASM_ERR_OOM;
        mod->funcs[fi].type_idx = ti;
        mod->funcs[fi].is_declared_ref = 1;
        mod->funcs[fi].is_import = 1;
        mod->num_func_imports++;
        info->index = fi;
        info->type_index = ti;
        if (mod->is_compiled) return WASM_OK;
        for (j = mod->rt->num_imports; j > 0u; j--) {
            uint32_t binding_index = j - 1u;

            if (strcmp(mod->rt->imports[binding_index].module, info->module) == 0 &&
                strcmp(mod->rt->imports[binding_index].name, info->name) == 0) {
                if (!wasm__functype_is_unspecified(&mod->rt->imports[binding_index].type) &&
                    !((mod->rt->imports[binding_index].has_type_context &&
                       mod->rt->imports[binding_index].type_module &&
                       wasm__functype_is_subtype_across_modules(
                           mod->rt->imports[binding_index].type_module,
                           mod->rt->imports[binding_index].type_index,
                           mod,
                           ti)) ||
                      (!mod->rt->imports[binding_index].has_type_context &&
                       wasm__functype_is_subtype(mod, &mod->rt->imports[binding_index].type, import_type)))) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                                  "function import type mismatch: %.64s.%.64s", info->module, info->name);
                    return WASM_ERR_TYPE_MISMATCH;
                }
                mod->funcs[fi].host_func = mod->rt->imports[binding_index].func;
                mod->funcs[fi].host_userdata = mod->rt->imports[binding_index].userdata;
                break;
            }
        }
        if (!mod->funcs[fi].host_func) {
            if (wasm__has_import_named(mod->rt, info->module, info->name)) {
                WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                              "function import type mismatch: %.64s.%.64s",
                              info->module, info->name);
                return WASM_ERR_TYPE_MISMATCH;
            }
            /* Defer unresolved function imports until after validation so
             * invalid modules still report structural errors first. */
        }
    } else if (kind == 0x01) {
        uint32_t table_index;
        wasm_table_t* table;
        wasm_table_import_t* timp;
        wasm_table_t* bound_table;
        wasm_error_t err;
        if (wasm__append_tables(mod, 1, &table_index) != WASM_OK) return WASM_ERR_OOM;
        if (mod->num_tables > 1) {
            err = wasm__require_multi_table(mod);
            if (err != WASM_OK) return err;
        }
        table = &mod->tables[table_index];
        err = wasm__read_table_type(mod, r, table);
        if (err != WASM_OK) return err;
        timp = mod->is_compiled ? NULL : wasm__find_table_import(mod->rt, info->module, info->name);
        if (!timp) {
            if (!mod->is_compiled && wasm__has_import_named(mod->rt, info->module, info->name)) {
                WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                              "table import type mismatch: %.64s.%.64s",
                              info->module, info->name);
                return WASM_ERR_TYPE_MISMATCH;
            }
            table->is_import = 1;
            table->import_table = NULL;
            info->index = table_index;
            info->type = table->reftype;
            info->ref_type = table->reftype_info;
            return WASM_OK;
        }
        bound_table = wasm__table_actual(timp->table);
        if (!bound_table) return WASM_ERR_MALFORMED;
        if (!wasm__tabletype_matches_import(mod,
                                            bound_table,
                                            table->reftype,
                                            &table->reftype_info,
                                            table->size,
                                            table->max_size,
                                            table->is_64)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "table import type mismatch: %.64s.%.64s",
                          info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        table->is_import = 1;
        table->import_table = bound_table;
        table->size = bound_table->size;
        table->max_size = bound_table->max_size;
        table->is_64 = bound_table->is_64;
        table->elems = bound_table->elems;
        info->index = table_index;
        info->type = table->reftype;
        info->ref_type = table->reftype_info;
    } else if (kind == 0x02) {
        uint32_t memory_index;
        wasm_memory_t* memory;
        wasm_memory_import_t* mimp;
        wasm_memory_t* bound_memory;
        wasm_error_t err;

        if (wasm__append_memories(mod, 1, &memory_index) != WASM_OK) return WASM_ERR_OOM;
        if (mod->num_memories > 1) {
            err = wasm__require_multi_memory(mod);
            if (err != WASM_OK) return err;
        }
        memory = &mod->memories[memory_index];
        err = wasm__read_memory_type(mod, r, memory);
        if (err != WASM_OK) return err;
        err = wasm__validate_memory_limits(mod, memory_index, memory);
        if (err != WASM_OK) return err;
        if (mod->is_compiled) {
            memory->is_import = 1;
            info->index = memory_index;
            return WASM_OK;
        }
        mimp = wasm__find_memory_import(mod->rt, info->module, info->name);
#if WASM_ENABLE_PLATFORM
        if (!mimp && mod->rt && mod->rt->emscripten_stubs_enabled &&
            strcmp(info->module, "env") == 0 && strcmp(info->name, "memory") == 0) {
            err = wasm__ensure_emscripten_memory_import(mod->rt,
                                                        memory->pages,
                                                        memory->max_pages,
                                                        memory->is_64);
            if (err != WASM_OK) return err;
            mimp = wasm__find_memory_import(mod->rt, info->module, info->name);
        }
#endif
        if (!mimp) {
            if (wasm__has_import_named(mod->rt, info->module, info->name)) {
                WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                              "memory import type mismatch: %.64s.%.64s",
                              info->module, info->name);
                return WASM_ERR_TYPE_MISMATCH;
            }
            WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_UNKNOWN_IMPORT;
        }
        bound_memory = wasm__memory_actual(mimp->memory);
        if (!bound_memory) return WASM_ERR_MALFORMED;
        if (!wasm__memorytype_matches_import(bound_memory, memory)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "memory import type mismatch: %.64s.%.64s",
                          info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        memory->is_import = 1;
        memory->import_memory = bound_memory;
        memory->pages = wasm__memory_pages(bound_memory);
        memory->max_pages = bound_memory->max_pages;
        memory->is_64 = bound_memory->is_64;
        memory->page_size = bound_memory->page_size;
        memory->is_shared = bound_memory->is_shared;
        memory->data = bound_memory->data;
        info->index = memory_index;
    } else if (kind == 0x03) {
        uint32_t gi;
        wasm_global_import_t* gimp;
        wasm_valtype_t type;
        wasm_reftype_t ref_type;
        uint8_t is_mutable;
        wasm_error_t err;

        err = wasm__read_valtype(mod, r, &type, &ref_type);
        if (err != WASM_OK) return err;
        is_mutable = wasm__read_u8(r);
        if (is_mutable > 1) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "malformed mutability");
            return WASM_ERR_MALFORMED;
        }
        if (is_mutable) {
            wasm_error_t feature_err = wasm__require_feature(mod, WASM_FEATURE_MUTABLE_GLOBALS);
            if (feature_err != WASM_OK) return feature_err;
        }

        if (mod->is_compiled) {
            wasm_value_t* placeholder = (wasm_value_t*)wasm__malloc(&mod->rt->allocator, sizeof(*placeholder));

            if (!placeholder) return WASM_ERR_OOM;
            if (wasm__append_global_slots(mod, 1, &gi) != WASM_OK) {
                wasm__free(&mod->rt->allocator, placeholder);
                return WASM_ERR_OOM;
            }
            *placeholder = wasm__default_value(type);
            mod->globals[gi].type = type;
            mod->globals[gi].ref_type = ref_type;
            mod->globals[gi].is_mutable = is_mutable;
            mod->globals[gi].is_import = 1;
            mod->globals[gi].import_value = placeholder;
            mod->num_global_imports++;
            info->index = gi;
            info->type = type;
            info->ref_type = ref_type;
            info->is_mutable = (int)is_mutable;
            return WASM_OK;
        }
        gimp = wasm__find_global_import(mod->rt, info->module, info->name);
        if (!gimp) {
            if (wasm__has_import_named(mod->rt, info->module, info->name)) {
                WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                              "global import type mismatch: %.64s.%.64s",
                              info->module, info->name);
                return WASM_ERR_TYPE_MISMATCH;
            }
            WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_UNKNOWN_IMPORT;
        }
        if (!wasm__globaltype_matches_import(mod,
                                             gimp->type,
                                             &gimp->ref_type,
                                             gimp->is_mutable,
                                             type,
                                             &ref_type,
                                             (int)is_mutable)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "global import type mismatch: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        if (!gimp->value) return WASM_ERR_MALFORMED;
        if (!wasm__runtime_value_matches_type(mod, gimp->value, gimp->type, &gimp->ref_type)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "global import value mismatch: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        if (wasm__append_global_slots(mod, 1, &gi) != WASM_OK) return WASM_ERR_OOM;

        mod->globals[gi].type = type;
        mod->globals[gi].ref_type = ref_type;
        mod->globals[gi].is_mutable = is_mutable;
        mod->globals[gi].is_import = 1;
        mod->globals[gi].import_value = gimp->value;
        mod->num_global_imports++;
        info->index = gi;
        info->type = type;
        info->ref_type = ref_type;
        info->is_mutable = (int)is_mutable;
    } else if (kind == 0x04) {
        uint32_t tag_index;
        uint32_t type_index;
        wasm_tag_import_t* timp;
        const wasm_functype_t* import_type;
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_EXCEPTIONS);

        if (err != WASM_OK) return err;
        if (wasm__read_u8(r) != 0x00) return WASM_ERR_MALFORMED;
        type_index = wasm__read_leb128_u32(r);
        import_type = wasm__module_const_functype(mod, type_index);
        if (!import_type) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown type %u", (unsigned)type_index);
            return WASM_ERR_MALFORMED;
        }
        if (import_type->num_results != 0) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "%s", "non-empty tag result type");
            return WASM_ERR_TYPE_MISMATCH;
        }
        if (mod->is_compiled) {
            if (wasm__append_tags(mod, 1, &tag_index) != WASM_OK) return WASM_ERR_OOM;
            mod->tags[tag_index].type_idx = type_index;
            mod->tags[tag_index].identity = wasm__alloc_tag_identity(mod->rt);
            info->index = tag_index;
            info->type_index = type_index;
            return WASM_OK;
        }
        timp = wasm__find_tag_import(mod->rt, info->module, info->name);
        if (!timp) {
            if (wasm__has_import_named(mod->rt, info->module, info->name)) {
                WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                              "tag import type mismatch: %.64s.%.64s",
                              info->module, info->name);
                return WASM_ERR_TYPE_MISMATCH;
            }
            WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s", info->module, info->name);
            return WASM_ERR_UNKNOWN_IMPORT;
        }
        if (!((timp->has_type_context && timp->type_module &&
               wasm__functype_equal_across_modules(timp->type_module,
                                                   timp->type_index,
                                                   mod,
                                                   type_index)) ||
              (!timp->has_type_context && wasm__functype_equal(&timp->type, import_type)))) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "tag import type mismatch: %.64s.%.64s",
                          info->module, info->name);
            return WASM_ERR_TYPE_MISMATCH;
        }
        if (wasm__append_tags(mod, 1, &tag_index) != WASM_OK) return WASM_ERR_OOM;
        mod->tags[tag_index].type_idx = type_index;
        mod->tags[tag_index].identity = timp->identity;
        info->index = tag_index;
        info->type_index = mod->tags[tag_index].type_idx;
    } else {
        return WASM_ERR_MALFORMED;
    }

    return WASM_OK;
}

/* Parses an import description into temporaries so a shared compact-import type can be re-read per item. */
static wasm_error_t wasm__skip_import_desc(wasm_module_t* mod, wasm__reader_t* r, uint8_t kind) {
    switch (kind) {
        case 0x00:
            wasm__read_leb128_u32(r);
            break;
        case 0x01: {
            wasm_table_t table;
            wasm_error_t err = wasm__read_table_type(mod, r, &table);
            if (err != WASM_OK) return err;
            break;
        }
        case 0x02: {
            wasm_memory_t memory;
            wasm_error_t err = wasm__read_memory_type(mod, r, &memory);
            if (err != WASM_OK) return err;
            break;
        }
        case 0x03: {
            wasm_valtype_t type;
            wasm_reftype_t ref_type;
            wasm_error_t err = wasm__read_valtype(mod, r, &type, &ref_type);
            if (err != WASM_OK) return err;
            wasm__read_u8(r);
            break;
        }
        case 0x04:
            wasm__read_u8(r);
            wasm__read_leb128_u32(r);
            break;
        default:
            return WASM_ERR_MALFORMED;
    }
    return r->malformed ? WASM_ERR_MALFORMED : WASM_OK;
}

/* A compact group shares one module name: encoding 0x7F gives each item a name and type,
   0x7E gives one type followed by the item names. */
static wasm_error_t wasm__decode_compact_imports(wasm_module_t* mod,
                                                 wasm__reader_t* r,
                                                 const char* module_name,
                                                 uint8_t encoding) {
    const uint8_t* shared_desc = NULL;
    uint8_t shared_kind = 0;
    uint32_t count;
    uint32_t i;
    wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_COMPACT_IMPORTS);

    if (err != WASM_OK) return err;
    if (encoding == 0x7E) {
        shared_kind = wasm__read_u8(r);
        shared_desc = r->ptr;
        err = wasm__skip_import_desc(mod, r, shared_kind);
        if (err != WASM_OK) return err;
    }

    count = wasm__read_count(r);
    for (i = 0; i < count; i++) {
        uint32_t import_index;
        wasm_import_info_t* info;
        char* field_name = NULL;

        err = wasm__append_import_infos(mod, 1, &import_index);
        if (err != WASM_OK) return err;
        info = &mod->imports[import_index];
        info->type_index = UINT32_MAX;
        info->module = wasm__strdup(&mod->rt->allocator, module_name);
        if (!info->module) return WASM_ERR_OOM;
        err = wasm__read_name_owned(&mod->rt->allocator, r, &field_name);
        info->name = field_name;
        if (err != WASM_OK) return err;

        if (encoding == 0x7F) {
            uint8_t kind = wasm__read_u8(r);
            info->kind = (wasm_import_kind_t)kind;
            err = wasm__decode_import_desc(mod, r, info, kind);
        } else {
            wasm__reader_t desc;
            desc.ptr = shared_desc;
            desc.end = r->end;
            desc.malformed = 0;
            info->kind = (wasm_import_kind_t)shared_kind;
            err = wasm__decode_import_desc(mod, &desc, info, shared_kind);
        }
        if (err != WASM_OK) return err;
    }
    return r->malformed ? WASM_ERR_MALFORMED : WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_import_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r);
    uint32_t i;

    for (i = 0; i < count; i++) {
        char* module_name = NULL;
        char* field_name = NULL;
        uint32_t field_len = 0;
        uint32_t import_index;
        wasm_import_info_t* info;
        wasm_error_t err;
        uint8_t kind;

        err = wasm__read_name_owned(&mod->rt->allocator, r, &module_name);
        if (err != WASM_OK) return err;
        err = wasm__read_name_owned_len(&mod->rt->allocator, r, &field_name, &field_len);
        if (err != WASM_OK) {
            wasm__free(&mod->rt->allocator, module_name);
            return err;
        }
        kind = wasm__read_u8(r);

        if (field_len == 0 && (kind == 0x7F || kind == 0x7E)) {
            wasm__free(&mod->rt->allocator, field_name);
            err = wasm__decode_compact_imports(mod, r, module_name, kind);
            wasm__free(&mod->rt->allocator, module_name);
            if (err != WASM_OK) return err;
            continue;
        }

        err = wasm__append_import_infos(mod, 1, &import_index);
        if (err != WASM_OK) {
            wasm__free(&mod->rt->allocator, module_name);
            wasm__free(&mod->rt->allocator, field_name);
            return err;
        }
        info = &mod->imports[import_index];
        info->type_index = UINT32_MAX;
        info->module = module_name;
        info->name = field_name;
        info->kind = (wasm_import_kind_t)kind;
        err = wasm__decode_import_desc(mod, r, info, kind);
        if (err != WASM_OK) return err;
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_function_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r);

    uint32_t base, i;

    if (wasm__append_funcs(mod, count, &base) != WASM_OK) return WASM_ERR_OOM;
    for (i = 0; i < count; i++) {
        uint32_t fi = base + i;
        mod->funcs[fi].type_idx = wasm__read_leb128_u32(r);
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_table_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r), i, base;

    if (wasm__append_tables(mod, count, &base) != WASM_OK) return WASM_ERR_OOM;
    if (mod->num_tables > 1) {
        wasm_error_t err = wasm__require_multi_table(mod);
        if (err != WASM_OK) return err;
    }
    for (i = 0; i < count; i++) {
        wasm_table_t* table = &mod->tables[base + i];
        wasm_error_t err = wasm__read_table_type(mod, r, table);
        if (err != WASM_OK) return err;
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_memory_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r), i, base;

    if (wasm__append_memories(mod, count, &base) != WASM_OK) return WASM_ERR_OOM;
    if (mod->num_memories > 1) {
        wasm_error_t err = wasm__require_multi_memory(mod);
        if (err != WASM_OK) return err;
    }

    for (i = 0; i < count; i++) {
        wasm_memory_t* memory = &mod->memories[base + i];
        wasm_error_t err = wasm__read_memory_type(mod, r, memory);
        if (err != WASM_OK) return err;
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_global_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r), i, base;
    wasm_error_t err;

    err = wasm__append_global_slots(mod, count, &base);
    if (err != WASM_OK) return err;

    for (i = 0; i < count; i++) {
        uint32_t global_index = base + i;
        wasm_value_t value;

        err = wasm__read_valtype(mod,
                                 r,
                                 &mod->globals[global_index].type,
                                 &mod->globals[global_index].ref_type);
        if (err != WASM_OK) return err;
        mod->globals[global_index].is_mutable = wasm__read_u8(r);
        if (mod->globals[global_index].is_mutable > 1) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "malformed mutability");
            return WASM_ERR_MALFORMED;
        }
        if (mod->globals[global_index].is_mutable) {
            err = wasm__require_feature(mod, WASM_FEATURE_MUTABLE_GLOBALS);
            if (err != WASM_OK) return err;
        }
        mod->init_expr_read_placeholder = 0;
        err = wasm__eval_init_expr(mod, r, global_index, &value);
        if (err != WASM_OK) return err;
        if (!wasm__init_value_matches(mod,
                                      &value,
                                      mod->globals[global_index].type,
                                      &mod->globals[global_index].ref_type))
            return WASM_ERR_TYPE_MISMATCH;
        wasm__global_set_value(&mod->globals[global_index], value);
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_tag_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r), i, base;
    wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_EXCEPTIONS);

    if (err != WASM_OK) return err;
    err = wasm__append_tags(mod, count, &base);
    if (err != WASM_OK) return err;

    for (i = 0; i < count; i++) {
        if (wasm__read_u8(r) != 0x00) return WASM_ERR_MALFORMED;
        mod->tags[base + i].type_idx = wasm__read_leb128_u32(r);
        mod->tags[base + i].identity = wasm__alloc_tag_identity(mod->rt);
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_export_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r), i;
    wasm_error_t err;
    mod->exports = (wasm_export_t*)wasm__calloc(&mod->rt->allocator, count, sizeof(wasm_export_t));
    if (!mod->exports) return WASM_ERR_OOM;
    mod->num_exports = count;
    for (i = 0; i < count; i++) {
        err = wasm__read_name_owned_len(&mod->rt->allocator, r, &mod->exports[i].name, &mod->exports[i].name_len);
        if (err != WASM_OK) return err;
        mod->exports[i].kind = (wasm_export_kind_t)wasm__read_u8(r);
        mod->exports[i].index = wasm__read_leb128_u32(r);
        if (mod->exports[i].kind == WASM_EXPORT_FUNC && mod->exports[i].index < mod->num_funcs)
            wasm__declare_func_ref(mod, mod->exports[i].index);
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_start_section(wasm_module_t* mod, wasm__reader_t* r) {
    mod->start_func = wasm__read_leb128_u32(r);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_element_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r), i, base;
    wasm_error_t err;

    err = wasm__append_elem_segments(mod, count, &base);
    if (err != WASM_OK) return err;

    for (i = 0; i < count; i++) {
        wasm_elem_segment_t* segment = &mod->elem_segments[base + i];
        uint32_t flags = wasm__read_leb128_u32(r);
        int uses_expr = (flags & 0x04) != 0;
        uint32_t j;

        if (flags > 0x07) return WASM_ERR_MALFORMED;

        if (flags & 0x01) {
            err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
        }

        segment->elem_type = WASM_TYPE_FUNCREF;
        segment->elem_ref_type.type = WASM_TYPE_FUNCREF;
        segment->elem_ref_type.nullable = 1;
        segment->uses_expr = uses_expr;
        if (flags & 0x01) {
            if (flags & 0x02)
                segment->is_declarative = 1;
            else
                segment->is_passive = 1;
        }

        if (!(flags & 0x01)) {
            wasm_value_t offset_value;
            wasm_table_t* table = NULL;

            segment->table_index = (flags & 0x02) ? wasm__read_leb128_u32(r) : 0;
            if (segment->table_index < mod->num_tables) table = &mod->tables[segment->table_index];
            err = wasm__eval_init_expr(mod, r, mod->num_globals, &offset_value);
            if (err != WASM_OK) return err;
            if (offset_value.type != wasm__table_index_type(table)) return WASM_ERR_TYPE_MISMATCH;
            segment->offset = wasm__table_index_value(table, &offset_value);
        }

        if (uses_expr) {
            err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
            if (err != WASM_OK) return err;

            /* Flag 4 uses expression elements with an implicit funcref type. */
            if (flags != 0x04) {
                err = wasm__read_reftype(mod, r, &segment->elem_type, &segment->elem_ref_type);
                if (err != WASM_OK) return err;
            }
        } else if ((flags & 0x03) != 0) {
            err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            err = wasm__read_elemkind(r);
            if (err != WASM_OK) return err;
        }

        segment->num_elems = wasm__read_count(r);
        if (segment->num_elems == 0) continue;

        segment->elems = (wasm_value_t*)wasm__malloc_array(&mod->rt->allocator, segment->num_elems, sizeof(wasm_value_t));
        if (!segment->elems) return WASM_ERR_OOM;
        for (j = 0; j < segment->num_elems; j++)
            segment->elems[j] = wasm__typed_ref_null(segment->elem_type, &segment->elem_ref_type);

        for (j = 0; j < segment->num_elems; j++) {
            wasm_value_t value;

            if (uses_expr) {
                mod->init_expr_read_placeholder = 0;
                err = wasm__eval_init_expr(mod, r, mod->num_globals, &value);
                if (err != WASM_OK) return err;
                if (!wasm__init_value_matches(mod, &value, segment->elem_type, &segment->elem_ref_type))
                    return WASM_ERR_TYPE_MISMATCH;
            } else {
                uint32_t func_index = wasm__read_leb128_u32(r);
                if (func_index >= mod->num_funcs) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown function %u",
                                  (unsigned)func_index);
                    return WASM_ERR_MALFORMED;
                }
                wasm__declare_func_ref(mod, func_index);
                err = wasm__make_funcref(mod, func_index, &value);
                if (err != WASM_OK) return err;
            }
            segment->elems[j] = value;
        }
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_code_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r), i;
    mod->num_code_bodies = count;
    for (i = 0; i < count; i++) {
        uint32_t body_size = wasm__read_leb128_u32(r);
        const uint8_t* body_start = r->ptr;
        uint32_t fidx = mod->num_func_imports + i;
        uint32_t ndecls, total_locals = 0, all_locals, local_off, d, j, p;
        const uint8_t* ls;
        wasm_functype_t* ft;
        if (r->malformed || !wasm__has(r, body_size)) return WASM_ERR_MALFORMED;
        if (fidx >= mod->num_funcs) return WASM_ERR_MALFORMED;
        ft = wasm__module_functype(mod, mod->funcs[fidx].type_idx);
        if (!ft) return WASM_ERR_MALFORMED;
        ndecls = wasm__read_count(r);
        ls = r->ptr;
        for (d = 0; d < ndecls; d++) {
            uint32_t local_count;
            wasm_valtype_t local_type;
            wasm_reftype_t local_ref_type;

            local_count = wasm__read_leb128_u32(r);
            if (r->malformed) return WASM_ERR_MALFORMED;
            if (local_count > UINT32_MAX - total_locals) {
                WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "too many locals");
                return WASM_ERR_MALFORMED;
            }
            total_locals += local_count;
            if (wasm__read_valtype(mod, r, &local_type, &local_ref_type) != WASM_OK)
                return WASM_ERR_MALFORMED;
        }
        if (ft->num_params > UINT32_MAX - total_locals) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "too many locals");
            return WASM_ERR_MALFORMED;
        }
        all_locals = ft->num_params + total_locals;
        if (mod->rt->max_function_locals != 0 && all_locals > mod->rt->max_function_locals) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "too many locals: %u exceeds the limit of %u",
                          (unsigned)all_locals, (unsigned)mod->rt->max_function_locals);
            return WASM_ERR_MALFORMED;
        }
        mod->funcs[fidx].num_locals = all_locals;
        mod->funcs[fidx].locals = (wasm_valtype_t*)wasm__malloc_array(&mod->rt->allocator, all_locals, sizeof(wasm_valtype_t));
        if (mod->funcs[fidx].locals)
            mod->funcs[fidx].local_reftypes = (wasm_reftype_t*)wasm__calloc(&mod->rt->allocator, all_locals, sizeof(wasm_reftype_t));
        if (!mod->funcs[fidx].locals || !mod->funcs[fidx].local_reftypes) return WASM_ERR_OOM;
        for (p = 0; p < ft->num_params; p++) {
            mod->funcs[fidx].locals[p] = ft->params[p];
            if (ft->param_reftypes) mod->funcs[fidx].local_reftypes[p] = ft->param_reftypes[p];
        }
        r->ptr = ls;
        local_off = ft->num_params;
        for (d = 0; d < ndecls; d++) {
            uint32_t n = wasm__read_leb128_u32(r);
            wasm_valtype_t t;
            wasm_reftype_t ref_type;
            wasm_error_t local_err = wasm__read_valtype(mod, r, &t, &ref_type);
            if (local_err != WASM_OK) return local_err;
            if (r->malformed || n > all_locals - local_off) {
                WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "too many locals");
                return WASM_ERR_MALFORMED;
            }
            for (j = 0; j < n; j++) {
                mod->funcs[fidx].locals[local_off] = t;
                mod->funcs[fidx].local_reftypes[local_off] = ref_type;
                local_off++;
            }
        }
        if ((uint32_t)(r->ptr - body_start) > body_size) return WASM_ERR_MALFORMED;
        mod->funcs[fidx].code = r->ptr;
        mod->funcs[fidx].code_len = (uint32_t)(body_size - (uint32_t)(r->ptr - body_start));
        r->ptr = body_start + body_size;
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_data_section(wasm_module_t* mod, wasm__reader_t* r) {
    uint32_t count = wasm__read_count(r), i, base;
    wasm_error_t err;

    err = wasm__append_data_segments(mod, count, &base);
    if (err != WASM_OK) return err;

    for (i = 0; i < count; i++) {
        wasm_data_segment_t* segment = &mod->data_segments[base + i];
        uint32_t flags = wasm__read_leb128_u32(r);

        switch (flags) {
            case 0x00: {
                wasm_value_t offset_value;
                wasm_memory_t* memory;

                if (mod->num_memories == 0) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown memory %u", 0u);
                    return WASM_ERR_MALFORMED;
                }
                err = wasm__eval_init_expr(mod, r, mod->num_globals, &offset_value);
                if (err != WASM_OK) return err;
                segment->memory_index = 0;
                memory = &mod->memories[segment->memory_index];
                if (offset_value.type != wasm__memory_index_type(memory)) return WASM_ERR_TYPE_MISMATCH;
                segment->offset = memory->is_64 ? (uint64_t)offset_value.of.i64
                                                : (uint64_t)(uint32_t)offset_value.of.i32;
                break;
            }
            case 0x01:
                err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
                if (err != WASM_OK) return err;
                segment->is_passive = 1;
                break;
            case 0x02: {
                wasm_value_t offset_value;
                wasm_memory_t* memory;

                err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
                if (err != WASM_OK) return err;
                segment->memory_index = wasm__read_leb128_u32(r);
                if (segment->memory_index >= mod->num_memories) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown memory %u",
                                  (unsigned)segment->memory_index);
                    return WASM_ERR_MALFORMED;
                }
                err = wasm__eval_init_expr(mod, r, mod->num_globals, &offset_value);
                if (err != WASM_OK) return err;
                memory = &mod->memories[segment->memory_index];
                if (offset_value.type != wasm__memory_index_type(memory)) return WASM_ERR_TYPE_MISMATCH;
                segment->offset = memory->is_64 ? (uint64_t)offset_value.of.i64
                                                : (uint64_t)(uint32_t)offset_value.of.i32;
                break;
            }
            default:
                return WASM_ERR_MALFORMED;
        }

        segment->size = wasm__read_leb128_u32(r);
        if (!wasm__has(r, segment->size)) return WASM_ERR_MALFORMED;
        if (segment->size > 0) {
            segment->bytes = (uint8_t*)wasm__malloc(&mod->rt->allocator, segment->size);
            if (!segment->bytes) return WASM_ERR_OOM;
            memcpy(segment->bytes, r->ptr, segment->size);
        }
        r->ptr += segment->size;
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_data_count_section(wasm_module_t* mod, wasm__reader_t* r) {
    wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_BULK_MEMORY);
    if (err != WASM_OK) return err;
    mod->declared_data_count = wasm__read_leb128_u32(r);
    mod->has_data_count_section = 1;
    return WASM_OK;
}

static uint32_t wasm__find_exported_func(wasm_module_t* mod, const char* name) {
    uint32_t i;

    for (i = 0; i < mod->num_exports; i++) {
        if (mod->exports[i].kind == WASM_EXPORT_FUNC &&
            wasm__export_name_matches_cstr(&mod->exports[i], name))
            return mod->exports[i].index;
    }

    return UINT32_MAX;
}

WASM_PRIVATE wasm_error_t wasm__check_unresolved_function_imports(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_imports; i++) {
        wasm_import_info_t* info = &mod->imports[i];

        if (info->kind != WASM_IMPORT_FUNC) continue;
        if (info->index >= mod->num_funcs) return WASM_ERR_MALFORMED;
        if (mod->funcs[info->index].host_func) continue;

        WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s",
                      info->module ? info->module : "",
                      info->name ? info->name : "");
        return WASM_ERR_UNKNOWN_IMPORT;
    }

    for (i = 0; i < mod->num_imports; i++) {
        wasm_import_info_t* info = &mod->imports[i];

        if (info->kind != WASM_IMPORT_TABLE) continue;
        if (info->index >= mod->num_tables) return WASM_ERR_MALFORMED;
        if (mod->tables[info->index].import_table) continue;

        WASM__SET_ERR(mod->rt, WASM_ERR_UNKNOWN_IMPORT, "unresolved: %.64s.%.64s",
                      info->module ? info->module : "",
                      info->name ? info->name : "");
        return WASM_ERR_UNKNOWN_IMPORT;
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__run_startup(wasm_module_t* mod) {
    wasm_error_t err;

    if (mod->startup_ran) return WASM_OK;

    mod->startup_ran = 1;

    if (mod->start_func != UINT32_MAX) {
        mod->rt->nonresumable_depth++;
        err = wasm_call_index(mod, mod->start_func, NULL, 0, NULL, 0);
        mod->rt->nonresumable_depth--;
        if (err != WASM_OK) {
            mod->startup_ran = 0;
            return err;
        }
        return WASM_OK;
    }

    {
        uint32_t ctor_idx = wasm__find_exported_func(mod, "__wasm_call_ctors");
        if (ctor_idx != UINT32_MAX) {
            mod->rt->nonresumable_depth++;
            err = wasm_call_index(mod, ctor_idx, NULL, 0, NULL, 0);
            mod->rt->nonresumable_depth--;
            if (err != WASM_OK) {
                mod->startup_ran = 0;
                return err;
            }
        }
    }

    return WASM_OK;
}

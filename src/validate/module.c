#include "module.h"
#include "../module.h"
#include "../memory_internal.h"
#include "../reader.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include "../types/references.h"
#include "../values.h"
#include <stdio.h>
#include <stdarg.h>

WASM_PRIVATE wasm_error_t wasm__set_validation_error(wasm_runtime_t* rt,
                                               uint32_t func_idx,
                                               uint32_t offset,
                                               const char* fmt,
                                               ...) {
    char detail[192];
    va_list args;

    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);

    rt->last_error = WASM_ERR_MALFORMED;
    if (func_idx == UINT32_MAX)
        wasm__snprintf(rt->error_msg, sizeof(rt->error_msg), "validation error: %s", detail);
    else
        wasm__snprintf(rt->error_msg, sizeof(rt->error_msg),
                       "validation error: func[%u] offset 0x%X: %s",
                       (unsigned)func_idx, (unsigned)offset, detail);
    return WASM_ERR_MALFORMED;
}

WASM_PRIVATE wasm_error_t wasm__validate_types(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_rec_groups; i++) {
        wasm_recgroup_t* group = &mod->rec_groups[i];

        if (group->first_type > mod->num_types ||
            group->num_types > mod->num_types - group->first_type) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "rec group %u is out of range",
                                              (unsigned)i);
        }
    }

    for (i = 0; i < mod->num_types; i++) {
        wasm_comptype_t* type = &mod->types[i];
        wasm_recgroup_t* group;
        uint32_t j;

        if (type->rec_group >= mod->num_rec_groups) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "type %u refers to missing rec group %u",
                                              (unsigned)i,
                                              (unsigned)type->rec_group);
        }

        group = &mod->rec_groups[type->rec_group];
        if (i < group->first_type || i >= group->first_type + group->num_types) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "type %u is outside rec group %u bounds",
                                              (unsigned)i,
                                              (unsigned)type->rec_group);
        }

        switch (type->kind) {
            case WASM_COMP_FUNC:
                for (j = 0; j < type->of.func.num_params; j++) {
                    wasm_reftype_t* ref = type->of.func.param_reftypes ? &type->of.func.param_reftypes[j] : NULL;

                    if (!ref || !ref->has_type_index) continue;
                    if (ref->type_index >= mod->num_types ||
                        (ref->type_index >= group->first_type &&
                         ref->type_index >= group->first_type + group->num_types)) {
                        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                          "unknown type %u",
                                                          (unsigned)ref->type_index);
                    }
                }
                for (j = 0; j < type->of.func.num_results; j++) {
                    wasm_reftype_t* ref = type->of.func.result_reftypes ? &type->of.func.result_reftypes[j] : NULL;

                    if (!ref || !ref->has_type_index) continue;
                    if (ref->type_index >= mod->num_types ||
                        (ref->type_index >= group->first_type &&
                         ref->type_index >= group->first_type + group->num_types)) {
                        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                          "unknown type %u",
                                                          (unsigned)ref->type_index);
                    }
                }
                break;
            case WASM_COMP_STRUCT:
                for (j = 0; j < type->of.struct_.num_fields; j++) {
                    wasm_reftype_t* ref = &type->of.struct_.fields[j].storage.ref_type;

                    if (!ref->has_type_index) continue;
                    if (ref->type_index >= mod->num_types ||
                        (ref->type_index >= group->first_type &&
                         ref->type_index >= group->first_type + group->num_types)) {
                        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                          "unknown type %u",
                                                          (unsigned)ref->type_index);
                    }
                }
                break;
            case WASM_COMP_ARRAY: {
                wasm_reftype_t* ref = &type->of.array.field.storage.ref_type;

                if (ref->has_type_index &&
                    (ref->type_index >= mod->num_types ||
                     (ref->type_index >= group->first_type &&
                      ref->type_index >= group->first_type + group->num_types))) {
                    return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                      "unknown type %u",
                                                      (unsigned)ref->type_index);
                }
                break;
            }
            default:
                break;
        }

        for (j = 0; j < type->num_supertypes; j++) {
            uint32_t super_idx = type->supertypes[j];
            wasm_comptype_t* supertype;

            if (super_idx >= mod->num_types) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u supertype %u out of range",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }
            if (super_idx >= i) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u supertype %u must have a lower index",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }

            supertype = &mod->types[super_idx];
            if (supertype->is_final) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u cannot extend final type %u",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }
            if (type->kind != supertype->kind) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u kind does not match supertype %u",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }
            if (!wasm__is_comptype_immediate_subtype(mod, type, supertype)) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "type %u is not a valid subtype of %u",
                                                  (unsigned)i,
                                                  (unsigned)super_idx);
            }
        }
    }

    return WASM_OK;
}

static wasm_error_t wasm__resolve_functype_reftypes(wasm_module_t* mod, wasm_functype_t* ft) {
    uint32_t i;

    for (i = 0; i < ft->num_params; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(
            mod, &ft->params[i], ft->param_reftypes ? &ft->param_reftypes[i] : NULL);
        if (err != WASM_OK) return err;
    }
    for (i = 0; i < ft->num_results; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(
            mod, &ft->results[i], ft->result_reftypes ? &ft->result_reftypes[i] : NULL);
        if (err != WASM_OK) return err;
    }
    return WASM_OK;
}

static wasm_error_t wasm__resolve_storagetype_reftype(wasm_module_t* mod, wasm_storagetype_t* storage) {
    if (storage->kind != WASM_STORAGE_VALTYPE) return WASM_OK;
    return wasm__resolve_valtype_reftype(mod, &storage->of.valtype, &storage->ref_type);
}

WASM_PRIVATE wasm_error_t wasm__resolve_module_reftypes(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_types; i++) {
        wasm_comptype_t* type = &mod->types[i];
        wasm_error_t err;
        uint32_t j;

        switch (type->kind) {
            case WASM_COMP_FUNC:
                err = wasm__resolve_functype_reftypes(mod, &type->of.func);
                if (err != WASM_OK) return err;
                break;
            case WASM_COMP_STRUCT:
                for (j = 0; j < type->of.struct_.num_fields; j++) {
                    err = wasm__resolve_storagetype_reftype(mod, &type->of.struct_.fields[j].storage);
                    if (err != WASM_OK) return err;
                }
                break;
            case WASM_COMP_ARRAY:
                err = wasm__resolve_storagetype_reftype(mod, &type->of.array.field.storage);
                if (err != WASM_OK) return err;
                break;
            default:
                return WASM_ERR_MALFORMED;
        }
    }

    for (i = 0; i < mod->num_tables; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(mod, &mod->tables[i].reftype, &mod->tables[i].reftype_info);
        if (err != WASM_OK) return err;
    }

    for (i = 0; i < mod->num_globals; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(mod, &mod->globals[i].type, &mod->globals[i].ref_type);
        if (err != WASM_OK) return err;
    }

    for (i = 0; i < mod->num_elem_segments; i++) {
        wasm_error_t err = wasm__resolve_valtype_reftype(
            mod, &mod->elem_segments[i].elem_type, &mod->elem_segments[i].elem_ref_type);
        if (err != WASM_OK) return err;
    }

    for (i = 0; i < mod->num_funcs; i++) {
        wasm_func_t* func = &mod->funcs[i];
        uint32_t j;

        for (j = 0; j < func->num_locals; j++) {
            wasm_error_t err = wasm__resolve_valtype_reftype(
                mod, &func->locals[j], func->local_reftypes ? &func->local_reftypes[j] : NULL);
            if (err != WASM_OK) return err;
        }
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validate_structural(wasm_module_t* mod) {
    uint32_t i, j;

    if (mod->num_code_bodies != mod->num_funcs - mod->num_func_imports) {
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                          "code section count %u does not match %u defined functions",
                                          (unsigned)mod->num_code_bodies,
                                          (unsigned)(mod->num_funcs - mod->num_func_imports));
    }

    for (i = 0; i < mod->num_funcs; i++) {
        if (mod->funcs[i].type_idx >= mod->num_types) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "function %u type index %u out of range (have %u types)",
                                              (unsigned)i,
                                              (unsigned)mod->funcs[i].type_idx,
                                              (unsigned)mod->num_types);
        }
    }

    for (i = 0; i < mod->num_tags; i++) {
        wasm_functype_t* tag_type;

        if (mod->tags[i].type_idx >= mod->num_types) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "tag %u type index %u out of range (have %u types)",
                                              (unsigned)i,
                                              (unsigned)mod->tags[i].type_idx,
                                              (unsigned)mod->num_types);
        }

        tag_type = wasm__module_functype(mod, mod->tags[i].type_idx);
        if (!tag_type) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "tag %u type must reference a function type",
                                              (unsigned)i);
        }
        if (tag_type->num_results != 0) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "tag %u type must not declare results",
                                              (unsigned)i);
        }
    }

    for (i = 0; i < mod->num_tables; i++) {
        wasm_table_t* table = &mod->tables[i];

        if (!wasm__is_ref_type(table->reftype)) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "table %u has invalid reftype 0x%X",
                                              (unsigned)i, (unsigned)table->reftype);
        }
        if (table->max_size != UINT64_MAX && (uint64_t)table->size > table->max_size) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "table %u min %llu exceeds max %llu",
                                              (unsigned)i,
                                              (unsigned long long)table->size,
                                              (unsigned long long)table->max_size);
        }
    }

    for (i = 0; i < mod->num_memories; i++) {
        wasm_memory_t* memory = &mod->memories[i];
        wasm_error_t err = wasm__validate_memory_limits(mod, i, memory);

        if (err != WASM_OK) return err;
    }

    for (i = 0; i < mod->num_exports; i++) {
        uint32_t limit = 0;

        switch (mod->exports[i].kind) {
            case WASM_EXPORT_FUNC:
                limit = mod->num_funcs;
                break;
            case WASM_EXPORT_TABLE:
                limit = mod->num_tables;
                break;
            case WASM_EXPORT_MEM:
                limit = mod->num_memories;
                break;
            case WASM_EXPORT_GLOBAL:
                limit = mod->num_globals;
                break;
            case WASM_EXPORT_TAG:
                limit = mod->num_tags;
                break;
            default:
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "export '%s' has invalid kind %u",
                                                  wasm__export_name_cstr(&mod->exports[i]),
                                                  (unsigned)mod->exports[i].kind);
        }

        if (mod->exports[i].index >= limit) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "export '%s' index %u out of range",
                                              wasm__export_name_cstr(&mod->exports[i]),
                                              (unsigned)mod->exports[i].index);
        }

        for (j = i + 1; j < mod->num_exports; j++) {
            if (wasm__export_name_matches_bytes(&mod->exports[i],
                                                (const uint8_t*)mod->exports[j].name,
                                                (size_t)mod->exports[j].name_len)) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "duplicate export name '%s'",
                                                  wasm__export_name_cstr(&mod->exports[i]));
            }
        }
    }

    if (mod->start_func != UINT32_MAX) {
        wasm_functype_t* start_type;

        if (mod->start_func >= mod->num_funcs) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "start function index %u out of range",
                                              (unsigned)mod->start_func);
        }
        start_type = wasm__module_functype(mod, mod->funcs[mod->start_func].type_idx);
        if (!start_type) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "start function must reference a function type");
        }
        if (start_type->num_params != 0 || start_type->num_results != 0) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "start function must have signature [] -> []");
        }
    }

    for (i = 0; i < mod->num_elem_segments; i++) {
        wasm_elem_segment_t* segment = &mod->elem_segments[i];

        if (!wasm__is_ref_type(segment->elem_type)) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "element segment %u has invalid reftype 0x%X",
                                              (unsigned)i,
                                              (unsigned)segment->elem_type);
        }
        if (!segment->is_passive && !segment->is_declarative) {
            wasm_table_t* table;

            if (segment->table_index >= mod->num_tables) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "element segment %u table index %u out of range",
                                                  (unsigned)i,
                                                  (unsigned)segment->table_index);
            }
            table = wasm__table_at(mod, segment->table_index);
            if (!table) return WASM_ERR_MALFORMED;
            if (!wasm__elem_segment_values_match_type(mod,
                                                      segment,
                                                      table->reftype,
                                                      &table->reftype_info)) {
                return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                                  "element segment %u type mismatch with table %u",
                                                  (unsigned)i,
                                                  (unsigned)segment->table_index);
            }
        }
    }

    for (i = 0; i < mod->num_data_segments; i++) {
        wasm_data_segment_t* segment = &mod->data_segments[i];

        if (segment->memory_index >= mod->num_memories && !(segment->is_passive && mod->num_memories == 0)) {
            return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                              "unknown memory %u",
                                              (unsigned)segment->memory_index);
        }
    }

    if (mod->has_data_count_section && mod->declared_data_count != mod->num_data_segments) {
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                          "data count section declares %u segments but module has %u",
                                          (unsigned)mod->declared_data_count,
                                          (unsigned)mod->num_data_segments);
    }

    return WASM_OK;
}

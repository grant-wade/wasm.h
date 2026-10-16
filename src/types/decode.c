#include "../internal.h"

WASM_PRIVATE wasm_error_t wasm__append_types(wasm_module_t* mod, uint32_t count, uint32_t* out_base) {
    uint32_t base;
    size_t new_count;
    wasm_comptype_t* types;

    if (count == 0) {
        if (out_base) *out_base = mod->num_types;
        return WASM_OK;
    }
    if (count > UINT32_MAX - mod->num_types) return WASM_ERR_OOM;

    base = mod->num_types;
    new_count = (size_t)base + (size_t)count;
    types = (wasm_comptype_t*)WASM_REALLOC(mod->types, new_count * sizeof(*types));
    if (!types) return WASM_ERR_OOM;

    mod->types = types;
    memset(mod->types + base, 0, count * sizeof(*mod->types));
    mod->num_types = base + count;
    if (out_base) *out_base = base;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__append_rec_groups(wasm_module_t* mod, uint32_t count, uint32_t* out_base) {
    uint32_t base;
    size_t new_count;
    wasm_recgroup_t* groups;

    if (count == 0) {
        if (out_base) *out_base = mod->num_rec_groups;
        return WASM_OK;
    }
    if (count > UINT32_MAX - mod->num_rec_groups) return WASM_ERR_OOM;

    base = mod->num_rec_groups;
    new_count = (size_t)base + (size_t)count;
    groups = (wasm_recgroup_t*)WASM_REALLOC(mod->rec_groups, new_count * sizeof(*groups));
    if (!groups) return WASM_ERR_OOM;

    mod->rec_groups = groups;
    memset(mod->rec_groups + base, 0, count * sizeof(*mod->rec_groups));
    mod->num_rec_groups = base + count;
    if (out_base) *out_base = base;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_functype(wasm_module_t* mod, wasm__reader_t* r, wasm_functype_t* ft) {
    uint32_t p;

    ft->num_params = wasm__read_leb128_u32(r);
    if (ft->num_params > 0) {
        ft->params = (wasm_valtype_t*)WASM_MALLOC(ft->num_params * sizeof(wasm_valtype_t));
        ft->param_reftypes = (wasm_reftype_t*)WASM_CALLOC(ft->num_params, sizeof(wasm_reftype_t));
        if (!ft->params || !ft->param_reftypes) return WASM_ERR_OOM;
    }
    for (p = 0; p < ft->num_params; p++) {
        wasm_error_t err = wasm__read_unresolved_valtype(mod, r, &ft->params[p], &ft->param_reftypes[p]);
        if (err != WASM_OK) return err;
    }

    ft->num_results = wasm__read_leb128_u32(r);
    if (ft->num_results > 1) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_MULTI_VALUE);
        if (err != WASM_OK) return err;
    }
    if (ft->num_results > 0) {
        ft->results = (wasm_valtype_t*)WASM_MALLOC(ft->num_results * sizeof(wasm_valtype_t));
        ft->result_reftypes = (wasm_reftype_t*)WASM_CALLOC(ft->num_results, sizeof(wasm_reftype_t));
        if (!ft->results || !ft->result_reftypes) return WASM_ERR_OOM;
    }
    for (p = 0; p < ft->num_results; p++) {
        wasm_error_t err = wasm__read_unresolved_valtype(mod, r, &ft->results[p], &ft->result_reftypes[p]);
        if (err != WASM_OK) return err;
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__read_storage_type(wasm_module_t* mod,
                                            wasm__reader_t* r,
                                            wasm_storagetype_t* out_type) {
    uint8_t byte = wasm__read_u8(r);

    wasm__clear_reftype(&out_type->ref_type);

    if (wasm__is_packedtype_byte(byte)) {
        out_type->kind = WASM_STORAGE_PACKED;
        out_type->of.packed_type = (wasm_packedtype_t)byte;
        return WASM_OK;
    }

    r->ptr--;
    out_type->kind = WASM_STORAGE_VALTYPE;
    return wasm__read_unresolved_valtype(mod, r, &out_type->of.valtype, &out_type->ref_type);
}

WASM_PRIVATE wasm_error_t wasm__decode_fieldtype(wasm_module_t* mod,
                                           wasm__reader_t* r,
                                           wasm_fieldtype_t* field) {
    uint8_t mutability;
    wasm_error_t err = wasm__read_storage_type(mod, r, &field->storage);
    if (err != WASM_OK) return err;

    mutability = wasm__read_u8(r);
    if (mutability > 1) return WASM_ERR_MALFORMED;
    field->is_mutable = (int)mutability;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__decode_comptype_body(wasm_module_t* mod,
                                               wasm__reader_t* r,
                                               uint8_t opcode,
                                               wasm_comptype_t* type) {
    switch (opcode) {
        case 0x60:
            type->kind = WASM_COMP_FUNC;
            return wasm__decode_functype(mod, r, &type->of.func);
        case 0x5F: {
            uint32_t i;
            wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
            if (err != WASM_OK) return err;

            type->kind = WASM_COMP_STRUCT;
            type->of.struct_.num_fields = wasm__read_leb128_u32(r);
            if (type->of.struct_.num_fields > 0) {
                type->of.struct_.fields = (wasm_fieldtype_t*)WASM_CALLOC(
                    type->of.struct_.num_fields, sizeof(wasm_fieldtype_t));
                if (!type->of.struct_.fields) return WASM_ERR_OOM;
            }
            for (i = 0; i < type->of.struct_.num_fields; i++) {
                err = wasm__decode_fieldtype(mod, r, &type->of.struct_.fields[i]);
                if (err != WASM_OK) return err;
            }
            return WASM_OK;
        }
        case 0x5E: {
            wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
            if (err != WASM_OK) return err;

            type->kind = WASM_COMP_ARRAY;
            return wasm__decode_fieldtype(mod, r, &type->of.array.field);
        }
        default:
            return WASM_ERR_MALFORMED;
    }
}

WASM_PRIVATE wasm_error_t wasm__decode_type_entry(wasm_module_t* mod,
                                            wasm__reader_t* r,
                                            uint8_t opcode,
                                            uint32_t rec_group_idx,
                                            wasm_comptype_t* type) {
    memset(type, 0, sizeof(*type));
    type->rec_group = rec_group_idx;
    type->is_final = 1;

    if (opcode == 0x50 || opcode == 0x4F) {
        uint32_t i;
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
        if (err != WASM_OK) return err;

        type->is_final = (opcode == 0x4F);
        type->num_supertypes = wasm__read_leb128_u32(r);
        if (type->num_supertypes > 0) {
            type->supertypes = (uint32_t*)WASM_MALLOC(type->num_supertypes * sizeof(uint32_t));
            if (!type->supertypes) return WASM_ERR_OOM;
        }
        for (i = 0; i < type->num_supertypes; i++)
            type->supertypes[i] = wasm__read_leb128_u32(r);
        opcode = wasm__read_u8(r);
    }

    return wasm__decode_comptype_body(mod, r, opcode, type);
}

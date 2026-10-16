#include "../internal.h"

WASM_PRIVATE void wasm__free_functype(wasm_functype_t* ft) {
    WASM_FREE(ft->params);
    WASM_FREE(ft->results);
    WASM_FREE(ft->param_reftypes);
    WASM_FREE(ft->result_reftypes);
    ft->params = NULL;
    ft->results = NULL;
    ft->param_reftypes = NULL;
    ft->result_reftypes = NULL;
    ft->num_params = 0;
    ft->num_results = 0;
}

WASM_PRIVATE int wasm__functype_is_unspecified(const wasm_functype_t* ft) {
    return ft && ft->num_params == UINT32_MAX && ft->num_results == UINT32_MAX;
}

WASM_PRIVATE int wasm__functype_is_zeroed(const wasm_functype_t* ft) {
    return ft && ft->num_params == 0 && ft->num_results == 0 &&
           ft->params == NULL && ft->results == NULL &&
           ft->param_reftypes == NULL && ft->result_reftypes == NULL;
}

WASM_PRIVATE void wasm__functype_mark_unspecified(wasm_functype_t* ft) {
    wasm__free_functype(ft);
    ft->num_params = UINT32_MAX;
    ft->num_results = UINT32_MAX;
}

WASM_PRIVATE void wasm__free_structtype(wasm_structtype_t* st) {
    WASM_FREE(st->fields);
    st->fields = NULL;
    st->num_fields = 0;
}

WASM_PRIVATE void wasm__free_comptype(wasm_comptype_t* type) {
    WASM_FREE(type->supertypes);
    type->supertypes = NULL;
    type->num_supertypes = 0;

    switch (type->kind) {
        case WASM_COMP_FUNC:
            wasm__free_functype(&type->of.func);
            break;
        case WASM_COMP_STRUCT:
            wasm__free_structtype(&type->of.struct_);
            break;
        case WASM_COMP_ARRAY:
            break;
        default:
            break;
    }

    type->kind = WASM_COMP_FUNC;
    type->rec_group = 0;
    type->canonical_id = 0;
    type->is_final = 0;
}

WASM_PRIVATE wasm_error_t wasm__copy_functype(wasm_functype_t* dst, const wasm_functype_t* src) {
    memset(dst, 0, sizeof(*dst));
    if (wasm__functype_is_unspecified(src)) {
        dst->num_params = UINT32_MAX;
        dst->num_results = UINT32_MAX;
        return WASM_OK;
    }
    dst->num_params = src->num_params;
    dst->num_results = src->num_results;

    if (src->num_params > 0) {
        dst->params = (wasm_valtype_t*)WASM_MALLOC(src->num_params * sizeof(wasm_valtype_t));
        dst->param_reftypes = (wasm_reftype_t*)WASM_CALLOC(src->num_params, sizeof(wasm_reftype_t));
        if (!dst->params || !dst->param_reftypes) {
            wasm__free_functype(dst);
            return WASM_ERR_OOM;
        }
        if (src->params)
            memcpy(dst->params, src->params, src->num_params * sizeof(wasm_valtype_t));
        else
            memset(dst->params, 0, src->num_params * sizeof(wasm_valtype_t));
        if (src->param_reftypes)
            memcpy(dst->param_reftypes, src->param_reftypes, src->num_params * sizeof(wasm_reftype_t));
    }

    if (src->num_results > 0) {
        dst->results = (wasm_valtype_t*)WASM_MALLOC(src->num_results * sizeof(wasm_valtype_t));
        dst->result_reftypes = (wasm_reftype_t*)WASM_CALLOC(src->num_results, sizeof(wasm_reftype_t));
        if (!dst->results || !dst->result_reftypes) {
            wasm__free_functype(dst);
            return WASM_ERR_OOM;
        }
        if (src->results)
            memcpy(dst->results, src->results, src->num_results * sizeof(wasm_valtype_t));
        else
            memset(dst->results, 0, src->num_results * sizeof(wasm_valtype_t));
        if (src->result_reftypes)
            memcpy(dst->result_reftypes, src->result_reftypes, src->num_results * sizeof(wasm_reftype_t));
    }

    return WASM_OK;
}

WASM_PRIVATE int wasm__functype_equal(const wasm_functype_t* lhs, const wasm_functype_t* rhs) {
    uint32_t i;

    if (lhs->num_params != rhs->num_params || lhs->num_results != rhs->num_results)
        return 0;
    for (i = 0; i < lhs->num_params; i++) {
        const wasm_reftype_t* lhs_ref = lhs->param_reftypes ? lhs->param_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->param_reftypes ? rhs->param_reftypes + i : NULL;
        if (lhs->params[i] != rhs->params[i]) return 0;
        if (wasm__is_ref_type(lhs->params[i]) || wasm__is_ref_type(rhs->params[i])) {
            if (wasm__has_reftype_info(lhs_ref) || wasm__has_reftype_info(rhs_ref)) {
                if (!wasm__reftype_equal(lhs_ref, rhs_ref)) return 0;
            }
        }
    }
    for (i = 0; i < lhs->num_results; i++) {
        const wasm_reftype_t* lhs_ref = lhs->result_reftypes ? lhs->result_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->result_reftypes ? rhs->result_reftypes + i : NULL;
        if (lhs->results[i] != rhs->results[i]) return 0;
        if (wasm__is_ref_type(lhs->results[i]) || wasm__is_ref_type(rhs->results[i])) {
            if (wasm__has_reftype_info(lhs_ref) || wasm__has_reftype_info(rhs_ref)) {
                if (!wasm__reftype_equal(lhs_ref, rhs_ref)) return 0;
            }
        }
    }
    return 1;
}

WASM_PRIVATE int wasm__storagetype_equal(const wasm_storagetype_t* lhs, const wasm_storagetype_t* rhs) {
    if (lhs->kind != rhs->kind) return 0;
    if (lhs->kind == WASM_STORAGE_PACKED) return lhs->of.packed_type == rhs->of.packed_type;
    if (lhs->of.valtype != rhs->of.valtype) return 0;
    if (!wasm__is_ref_type(lhs->of.valtype) && !wasm__is_ref_type(rhs->of.valtype)) return 1;
    if (wasm__has_reftype_info(&lhs->ref_type) || wasm__has_reftype_info(&rhs->ref_type))
        return wasm__reftype_equal(&lhs->ref_type, &rhs->ref_type);
    return 1;
}

WASM_PRIVATE int wasm__canonical_group_contains_type(const wasm_recgroup_t* group,
                                               uint32_t type_idx) {
    return group && type_idx >= group->first_type && type_idx < group->first_type + group->num_types;
}

WASM_PRIVATE uint32_t wasm__canonical_group_type_offset(const wasm_recgroup_t* group,
                                                  uint32_t type_idx) {
    return type_idx - group->first_type;
}

WASM_PRIVATE int wasm__canonical_reftype_equal_in_groups(const wasm_module_t* mod,
                                                   const wasm_reftype_t* lhs,
                                                   const wasm_reftype_t* rhs,
                                                   const wasm_recgroup_t* lhs_group,
                                                   const wasm_recgroup_t* rhs_group,
                                                   uint8_t* memo) {
    if (!wasm__has_reftype_info(lhs) || !wasm__has_reftype_info(rhs))
        return !wasm__has_reftype_info(lhs) && !wasm__has_reftype_info(rhs);
    if (lhs->type != rhs->type || lhs->nullable != rhs->nullable ||
        lhs->has_type_index != rhs->has_type_index)
        return 0;
    if (!lhs->has_type_index) return 1;
    return wasm__canonical_type_index_equal_in_groups(mod,
                                                      lhs->type_index,
                                                      rhs->type_index,
                                                      lhs_group,
                                                      rhs_group,
                                                      memo);
}

WASM_PRIVATE int wasm__canonical_functype_equal_in_groups(const wasm_module_t* mod,
                                                    const wasm_functype_t* lhs,
                                                    const wasm_functype_t* rhs,
                                                    const wasm_recgroup_t* lhs_group,
                                                    const wasm_recgroup_t* rhs_group,
                                                    uint8_t* memo) {
    uint32_t i;

    if (lhs->num_params != rhs->num_params || lhs->num_results != rhs->num_results)
        return 0;
    for (i = 0; i < lhs->num_params; i++) {
        const wasm_reftype_t* lhs_ref = lhs->param_reftypes ? lhs->param_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->param_reftypes ? rhs->param_reftypes + i : NULL;

        if (lhs->params[i] != rhs->params[i]) return 0;
        if ((wasm__is_ref_type(lhs->params[i]) || wasm__is_ref_type(rhs->params[i])) &&
            (wasm__has_reftype_info(lhs_ref) || wasm__has_reftype_info(rhs_ref)) &&
            !wasm__canonical_reftype_equal_in_groups(mod, lhs_ref, rhs_ref, lhs_group, rhs_group, memo))
            return 0;
    }
    for (i = 0; i < lhs->num_results; i++) {
        const wasm_reftype_t* lhs_ref = lhs->result_reftypes ? lhs->result_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->result_reftypes ? rhs->result_reftypes + i : NULL;

        if (lhs->results[i] != rhs->results[i]) return 0;
        if ((wasm__is_ref_type(lhs->results[i]) || wasm__is_ref_type(rhs->results[i])) &&
            (wasm__has_reftype_info(lhs_ref) || wasm__has_reftype_info(rhs_ref)) &&
            !wasm__canonical_reftype_equal_in_groups(mod, lhs_ref, rhs_ref, lhs_group, rhs_group, memo))
            return 0;
    }
    return 1;
}

WASM_PRIVATE int wasm__canonical_storagetype_equal_in_groups(const wasm_module_t* mod,
                                                       const wasm_storagetype_t* lhs,
                                                       const wasm_storagetype_t* rhs,
                                                       const wasm_recgroup_t* lhs_group,
                                                       const wasm_recgroup_t* rhs_group,
                                                       uint8_t* memo) {
    if (lhs->kind != rhs->kind) return 0;
    if (lhs->kind == WASM_STORAGE_PACKED) return lhs->of.packed_type == rhs->of.packed_type;
    if (lhs->of.valtype != rhs->of.valtype) return 0;
    if (!wasm__is_ref_type(lhs->of.valtype) && !wasm__is_ref_type(rhs->of.valtype)) return 1;
    if (wasm__has_reftype_info(&lhs->ref_type) || wasm__has_reftype_info(&rhs->ref_type))
        return wasm__canonical_reftype_equal_in_groups(mod,
                                                       &lhs->ref_type,
                                                       &rhs->ref_type,
                                                       lhs_group,
                                                       rhs_group,
                                                       memo);
    return 1;
}

WASM_PRIVATE int wasm__canonical_fieldtype_equal_in_groups(const wasm_module_t* mod,
                                                     const wasm_fieldtype_t* lhs,
                                                     const wasm_fieldtype_t* rhs,
                                                     const wasm_recgroup_t* lhs_group,
                                                     const wasm_recgroup_t* rhs_group,
                                                     uint8_t* memo) {
    return lhs->is_mutable == rhs->is_mutable &&
           wasm__canonical_storagetype_equal_in_groups(mod,
                                                       &lhs->storage,
                                                       &rhs->storage,
                                                       lhs_group,
                                                       rhs_group,
                                                       memo);
}

WASM_PRIVATE int wasm__comptype_body_equal_in_groups(const wasm_module_t* mod,
                                               const wasm_comptype_t* lhs,
                                               const wasm_comptype_t* rhs,
                                               const wasm_recgroup_t* lhs_group,
                                               const wasm_recgroup_t* rhs_group,
                                               uint8_t* memo) {
    uint32_t i;

    if (lhs->kind != rhs->kind) return 0;

    switch (lhs->kind) {
        case WASM_COMP_FUNC:
            return wasm__canonical_functype_equal_in_groups(mod,
                                                            &lhs->of.func,
                                                            &rhs->of.func,
                                                            lhs_group,
                                                            rhs_group,
                                                            memo);
        case WASM_COMP_STRUCT:
            if (lhs->of.struct_.num_fields != rhs->of.struct_.num_fields) return 0;
            for (i = 0; i < lhs->of.struct_.num_fields; i++) {
                if (!wasm__canonical_fieldtype_equal_in_groups(mod,
                                                               &lhs->of.struct_.fields[i],
                                                               &rhs->of.struct_.fields[i],
                                                               lhs_group,
                                                               rhs_group,
                                                               memo))
                    return 0;
            }
            return 1;
        case WASM_COMP_ARRAY:
            return wasm__canonical_fieldtype_equal_in_groups(mod,
                                                             &lhs->of.array.field,
                                                             &rhs->of.array.field,
                                                             lhs_group,
                                                             rhs_group,
                                                             memo);
        default:
            return 0;
    }
}

WASM_PRIVATE int wasm__comptype_equal_visit_in_groups(const wasm_module_t* mod,
                                                uint32_t lhs_idx,
                                                uint32_t rhs_idx,
                                                const wasm_recgroup_t* lhs_group,
                                                const wasm_recgroup_t* rhs_group,
                                                uint8_t* memo) {
    const wasm_comptype_t* lhs;
    const wasm_comptype_t* rhs;
    uint32_t i;

    if (!mod || lhs_idx >= mod->num_types || rhs_idx >= mod->num_types) return 0;
    lhs = &mod->types[lhs_idx];
    rhs = &mod->types[rhs_idx];

    if (lhs->is_final != rhs->is_final) return 0;
    if (lhs->num_supertypes != rhs->num_supertypes) return 0;
    for (i = 0; i < lhs->num_supertypes; i++) {
        if (!wasm__canonical_type_index_equal_in_groups(mod,
                                                        lhs->supertypes[i],
                                                        rhs->supertypes[i],
                                                        lhs_group,
                                                        rhs_group,
                                                        memo)) {
            return 0;
        }
    }

    return wasm__comptype_body_equal_in_groups(mod, lhs, rhs, lhs_group, rhs_group, memo);
}

WASM_PRIVATE int wasm__canonical_recgroup_equal(const wasm_module_t* mod,
                                          const wasm_recgroup_t* lhs_group,
                                          const wasm_recgroup_t* rhs_group,
                                          uint8_t* memo) {
    uint32_t i;

    if (!lhs_group || !rhs_group) return 0;
    if (lhs_group->num_types != rhs_group->num_types) return 0;

    for (i = 0; i < lhs_group->num_types; i++) {
        if (!wasm__comptype_equal_visit_in_groups(mod,
                                                  lhs_group->first_type + i,
                                                  rhs_group->first_type + i,
                                                  lhs_group,
                                                  rhs_group,
                                                  memo)) {
            return 0;
        }
    }
    return 1;
}

WASM_PRIVATE int wasm__canonical_type_index_equal_in_groups(const wasm_module_t* mod,
                                                      uint32_t lhs_idx,
                                                      uint32_t rhs_idx,
                                                      const wasm_recgroup_t* lhs_group,
                                                      const wasm_recgroup_t* rhs_group,
                                                      uint8_t* memo) {
    int lhs_internal = wasm__canonical_group_contains_type(lhs_group, lhs_idx);
    int rhs_internal = wasm__canonical_group_contains_type(rhs_group, rhs_idx);

    if (lhs_internal || rhs_internal) {
        if (!(lhs_internal && rhs_internal)) return 0;
        return wasm__canonical_group_type_offset(lhs_group, lhs_idx) ==
               wasm__canonical_group_type_offset(rhs_group, rhs_idx);
    }

    return wasm__canonical_type_equal_visit(mod, lhs_idx, rhs_idx, memo);
}

WASM_PRIVATE int wasm__canonical_type_equal_visit(const wasm_module_t* mod,
                                            uint32_t lhs_idx,
                                            uint32_t rhs_idx,
                                            uint8_t* memo) {
    const wasm_recgroup_t* lhs_group;
    const wasm_recgroup_t* rhs_group;
    size_t idx;
    size_t rev_idx;
    uint8_t state;
    int result;

    if (!mod || lhs_idx >= mod->num_types || rhs_idx >= mod->num_types) return 0;
    if (lhs_idx == rhs_idx) return 1;
    lhs_group = &mod->rec_groups[mod->types[lhs_idx].rec_group];
    rhs_group = &mod->rec_groups[mod->types[rhs_idx].rec_group];
    if (lhs_group->num_types != rhs_group->num_types) return 0;
    if ((lhs_idx - lhs_group->first_type) != (rhs_idx - rhs_group->first_type)) return 0;
    if (mod->types[lhs_idx].canonical_id != 0 && mod->types[rhs_idx].canonical_id != 0)
        return mod->types[lhs_idx].canonical_id == mod->types[rhs_idx].canonical_id;
    if (!memo) return lhs_idx == rhs_idx;

    idx = (size_t)lhs_idx * (size_t)mod->num_types + (size_t)rhs_idx;
    rev_idx = (size_t)rhs_idx * (size_t)mod->num_types + (size_t)lhs_idx;
    state = memo[idx];
    if (state == 1u || state == 2u) return 1;
    if (state == 3u) return 0;

    memo[idx] = 1u;
    memo[rev_idx] = 1u;
    result = wasm__canonical_recgroup_equal(mod, lhs_group, rhs_group, memo);
    memo[idx] = (uint8_t)(result ? 2u : 3u);
    memo[rev_idx] = memo[idx];
    return result;
}

WASM_PRIVATE int wasm__comptype_equal(const wasm_module_t* mod, uint32_t lhs_idx, uint32_t rhs_idx) {
    uint8_t* memo;
    size_t memo_size;
    int result;

    if (!mod) return 0;
    memo_size = mod->num_types ? (size_t)mod->num_types * (size_t)mod->num_types : 1u;
    memo = (uint8_t*)WASM_CALLOC(memo_size, sizeof(uint8_t));
    if (!memo) return 0;
    result = wasm__canonical_type_equal_visit(mod, lhs_idx, rhs_idx, memo);
    WASM_FREE(memo);
    return result;
}

WASM_PRIVATE wasm_error_t wasm__canonicalize_types(wasm_module_t* mod) {
    uint32_t i;
    uint32_t next_id = 1;

    if (!mod) return WASM_OK;

    for (i = 0; i < mod->num_types; i++) mod->types[i].canonical_id = 0;

    for (i = 0; i < mod->num_types; i++) {
        uint32_t j;

        for (j = 0; j < i; j++) {
            if (wasm__comptype_equal(mod, i, j)) {
                mod->types[i].canonical_id = mod->types[j].canonical_id;
                break;
            }
        }

        if (j == i) {
            if (next_id == 0) return WASM_ERR_OOM;
            mod->types[i].canonical_id = next_id++;
        }
    }

    return WASM_OK;
}

WASM_PRIVATE int wasm__type_equal(const wasm_module_t* mod, uint32_t lhs_idx, uint32_t rhs_idx) {
    const wasm_comptype_t* lhs;
    const wasm_comptype_t* rhs;

    if (!mod || lhs_idx >= mod->num_types || rhs_idx >= mod->num_types) return 0;
    if (lhs_idx == rhs_idx) return 1;

    lhs = &mod->types[lhs_idx];
    rhs = &mod->types[rhs_idx];
    if (lhs->canonical_id != 0 && rhs->canonical_id != 0) return lhs->canonical_id == rhs->canonical_id;
    return wasm__comptype_equal(mod, lhs_idx, rhs_idx);
}

WASM_PRIVATE int wasm__is_heap_subtype(const wasm_module_t* mod,
                                 wasm_valtype_t subtype,
                                 wasm_valtype_t supertype) {
    (void)mod;

    if (subtype == supertype) return 1;

    switch (subtype) {
        case WASM_TYPE_NONE:
            return supertype == WASM_TYPE_I31REF || supertype == WASM_TYPE_STRUCTREF ||
                   supertype == WASM_TYPE_ARRAYREF || supertype == WASM_TYPE_EQREF ||
                   supertype == WASM_TYPE_ANYREF;
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            return supertype == WASM_TYPE_EQREF || supertype == WASM_TYPE_ANYREF;
        case WASM_TYPE_EQREF:
            return supertype == WASM_TYPE_ANYREF;
        case WASM_TYPE_NOEXN:
            return supertype == WASM_TYPE_EXNREF;
        case WASM_TYPE_NOFUNC:
            return supertype == WASM_TYPE_FUNCREF;
        case WASM_TYPE_NOEXTERN:
            return supertype == WASM_TYPE_EXTERNREF;
        default:
            return 0;
    }
}

WASM_PRIVATE int wasm__is_reftype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype) {
    if (!wasm__is_ref_type(subtype) || !wasm__is_ref_type(supertype)) return 0;
    return wasm__is_heap_subtype(mod, subtype, supertype);
}

WASM_PRIVATE int wasm__is_valtype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype) {
    if (subtype == supertype) return 1;
    if (wasm__is_ref_type(subtype) && wasm__is_ref_type(supertype))
        return wasm__is_reftype_subtype(mod, subtype, supertype);
    return 0;
}

WASM_PRIVATE int wasm__is_storagetype_subtype(const wasm_module_t* mod,
                                        const wasm_storagetype_t* subtype,
                                        const wasm_storagetype_t* supertype) {
    if (subtype->kind != supertype->kind) return 0;
    if (subtype->kind == WASM_STORAGE_PACKED)
        return subtype->of.packed_type == supertype->of.packed_type;
    if ((wasm__has_reftype_info(&subtype->ref_type) || wasm__has_reftype_info(&supertype->ref_type)) &&
        wasm__is_ref_type(subtype->of.valtype) && wasm__is_ref_type(supertype->of.valtype))
        return wasm__reftype_is_subtype(mod, &subtype->ref_type, &supertype->ref_type);
    return wasm__is_valtype_subtype(mod, subtype->of.valtype, supertype->of.valtype);
}

WASM_PRIVATE int wasm__is_fieldtype_subtype(const wasm_module_t* mod,
                                      const wasm_fieldtype_t* subtype,
                                      const wasm_fieldtype_t* supertype) {
    if (subtype->is_mutable != supertype->is_mutable) return 0;
    if (subtype->is_mutable)
        return wasm__storagetype_equal(&subtype->storage, &supertype->storage);
    return wasm__is_storagetype_subtype(mod, &subtype->storage, &supertype->storage);
}

WASM_PRIVATE int wasm__is_comptype_immediate_subtype(const wasm_module_t* mod,
                                               const wasm_comptype_t* subtype,
                                               const wasm_comptype_t* supertype) {
    uint32_t i;

    if (subtype->kind != supertype->kind) return 0;

    switch (subtype->kind) {
        case WASM_COMP_FUNC:
            if (subtype->of.func.num_params != supertype->of.func.num_params ||
                subtype->of.func.num_results != supertype->of.func.num_results)
                return 0;
            for (i = 0; i < subtype->of.func.num_params; i++) {
                if (!wasm__is_valtype_subtype(mod,
                                              supertype->of.func.params[i],
                                              subtype->of.func.params[i]))
                    return 0;
            }
            for (i = 0; i < subtype->of.func.num_results; i++) {
                if (!wasm__is_valtype_subtype(mod,
                                              subtype->of.func.results[i],
                                              supertype->of.func.results[i]))
                    return 0;
            }
            return 1;
        case WASM_COMP_STRUCT:
            if (subtype->of.struct_.num_fields < supertype->of.struct_.num_fields) return 0;
            for (i = 0; i < supertype->of.struct_.num_fields; i++) {
                if (!wasm__is_fieldtype_subtype(mod,
                                                &subtype->of.struct_.fields[i],
                                                &supertype->of.struct_.fields[i]))
                    return 0;
            }
            return 1;
        case WASM_COMP_ARRAY:
            return wasm__is_fieldtype_subtype(mod,
                                              &subtype->of.array.field,
                                              &supertype->of.array.field);
        default:
            return 0;
    }
}

WASM_PRIVATE_INLINE int wasm__is_subtype(const wasm_module_t* mod,
                                   uint32_t subtype_idx,
                                   uint32_t supertype_idx) {
    const wasm_comptype_t* subtype;
    uint32_t i;

    if (!mod || subtype_idx >= mod->num_types || supertype_idx >= mod->num_types) return 0;
    if (wasm__type_equal(mod, subtype_idx, supertype_idx)) return 1;

    subtype = &mod->types[subtype_idx];
    for (i = 0; i < subtype->num_supertypes; i++) {
        uint32_t parent_idx = subtype->supertypes[i];

        if (parent_idx >= mod->num_types) continue;
        if (wasm__is_subtype(mod, parent_idx, supertype_idx)) return 1;
    }

    return 0;
}

WASM_PRIVATE wasm_functype_t* wasm__type_as_functype(wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_FUNC) return NULL;
    return &type->of.func;
}

WASM_PRIVATE const wasm_functype_t* wasm__type_as_const_functype(const wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_FUNC) return NULL;
    return &type->of.func;
}

WASM_PRIVATE wasm_functype_t* wasm__module_functype(wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_functype(&mod->types[type_idx]);
}

WASM_PRIVATE const wasm_functype_t* wasm__module_const_functype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_const_functype(&mod->types[type_idx]);
}

WASM_PRIVATE const wasm_structtype_t* wasm__type_as_const_structtype(const wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_STRUCT) return NULL;
    return &type->of.struct_;
}

WASM_PRIVATE const wasm_arraytype_t* wasm__type_as_const_arraytype(const wasm_comptype_t* type) {
    if (!type || type->kind != WASM_COMP_ARRAY) return NULL;
    return &type->of.array;
}

WASM_PRIVATE const wasm_structtype_t* wasm__module_const_structtype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_const_structtype(&mod->types[type_idx]);
}

WASM_PRIVATE const wasm_arraytype_t* wasm__module_const_arraytype(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return NULL;
    return wasm__type_as_const_arraytype(&mod->types[type_idx]);
}

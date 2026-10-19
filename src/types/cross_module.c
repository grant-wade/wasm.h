#include "cross_module.h"
#include "../module.h"
#include "../internal/alloc.h"
#include "../runtime.h"
#include "canonical.h"
#include "references.h"
#include "../values.h"
#include <string.h>

static int wasm__cross_type_equal_visit(const wasm_module_t* lhs_mod,
                                        uint32_t lhs_idx,
                                        const wasm_module_t* rhs_mod,
                                        uint32_t rhs_idx,
                                        uint8_t* memo);

static int wasm__cross_group_contains_type(const wasm_recgroup_t* group, uint32_t type_idx) {
    return group && type_idx >= group->first_type && type_idx < group->first_type + group->num_types;
}

static uint32_t wasm__cross_group_type_offset(const wasm_recgroup_t* group, uint32_t type_idx) {
    return type_idx - group->first_type;
}

static int wasm__cross_type_index_equal_in_groups(const wasm_module_t* lhs_mod,
                                                  uint32_t lhs_idx,
                                                  const wasm_recgroup_t* lhs_group,
                                                  const wasm_module_t* rhs_mod,
                                                  uint32_t rhs_idx,
                                                  const wasm_recgroup_t* rhs_group,
                                                  uint8_t* memo) {
    int lhs_internal = wasm__cross_group_contains_type(lhs_group, lhs_idx);
    int rhs_internal = wasm__cross_group_contains_type(rhs_group, rhs_idx);

    if (lhs_internal || rhs_internal) {
        if (!(lhs_internal && rhs_internal)) return 0;
        return wasm__cross_group_type_offset(lhs_group, lhs_idx) ==
               wasm__cross_group_type_offset(rhs_group, rhs_idx);
    }

    return wasm__cross_type_equal_visit(lhs_mod, lhs_idx, rhs_mod, rhs_idx, memo);
}

static int wasm__cross_reftype_equal_in_groups(const wasm_module_t* lhs_mod,
                                               const wasm_reftype_t* lhs,
                                               const wasm_recgroup_t* lhs_group,
                                               const wasm_module_t* rhs_mod,
                                               const wasm_reftype_t* rhs,
                                               const wasm_recgroup_t* rhs_group,
                                               uint8_t* memo) {
    if (!wasm__has_reftype_info(lhs) || !wasm__has_reftype_info(rhs))
        return !wasm__has_reftype_info(lhs) && !wasm__has_reftype_info(rhs);
    if (lhs->type != rhs->type || lhs->nullable != rhs->nullable ||
        lhs->has_type_index != rhs->has_type_index)
        return 0;
    if (!lhs->has_type_index) return 1;
    return wasm__cross_type_index_equal_in_groups(lhs_mod,
                                                  lhs->type_index,
                                                  lhs_group,
                                                  rhs_mod,
                                                  rhs->type_index,
                                                  rhs_group,
                                                  memo);
}

static int wasm__cross_typed_valtype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                     wasm_valtype_t lhs_type,
                                                     const wasm_reftype_t* lhs_ref,
                                                     const wasm_recgroup_t* lhs_group,
                                                     const wasm_module_t* rhs_mod,
                                                     wasm_valtype_t rhs_type,
                                                     const wasm_reftype_t* rhs_ref,
                                                     const wasm_recgroup_t* rhs_group,
                                                     uint8_t* memo) {
    wasm_reftype_t lhs_effective;
    wasm_reftype_t rhs_effective;

    if (!wasm__is_ref_type(lhs_type) && !wasm__is_ref_type(rhs_type)) return lhs_type == rhs_type;
    if (!wasm__is_ref_type(lhs_type) || !wasm__is_ref_type(rhs_type)) return 0;
    if (!wasm__get_effective_reftype(lhs_mod, lhs_type, lhs_ref, &lhs_effective) ||
        !wasm__get_effective_reftype(rhs_mod, rhs_type, rhs_ref, &rhs_effective))
        return 0;
    return wasm__cross_reftype_equal_in_groups(lhs_mod,
                                               &lhs_effective,
                                               lhs_group,
                                               rhs_mod,
                                               &rhs_effective,
                                               rhs_group,
                                               memo);
}

static int wasm__cross_functype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                const wasm_functype_t* lhs,
                                                const wasm_recgroup_t* lhs_group,
                                                const wasm_module_t* rhs_mod,
                                                const wasm_functype_t* rhs,
                                                const wasm_recgroup_t* rhs_group,
                                                uint8_t* memo) {
    uint32_t i;

    if (lhs->num_params != rhs->num_params || lhs->num_results != rhs->num_results)
        return 0;
    for (i = 0; i < lhs->num_params; i++) {
        const wasm_reftype_t* lhs_ref = lhs->param_reftypes ? lhs->param_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->param_reftypes ? rhs->param_reftypes + i : NULL;

        if (!wasm__cross_typed_valtype_equal_in_groups(lhs_mod,
                                                       lhs->params[i],
                                                       lhs_ref,
                                                       lhs_group,
                                                       rhs_mod,
                                                       rhs->params[i],
                                                       rhs_ref,
                                                       rhs_group,
                                                       memo)) {
            return 0;
        }
    }
    for (i = 0; i < lhs->num_results; i++) {
        const wasm_reftype_t* lhs_ref = lhs->result_reftypes ? lhs->result_reftypes + i : NULL;
        const wasm_reftype_t* rhs_ref = rhs->result_reftypes ? rhs->result_reftypes + i : NULL;

        if (!wasm__cross_typed_valtype_equal_in_groups(lhs_mod,
                                                       lhs->results[i],
                                                       lhs_ref,
                                                       lhs_group,
                                                       rhs_mod,
                                                       rhs->results[i],
                                                       rhs_ref,
                                                       rhs_group,
                                                       memo)) {
            return 0;
        }
    }
    return 1;
}

static int wasm__cross_storagetype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                   const wasm_storagetype_t* lhs,
                                                   const wasm_recgroup_t* lhs_group,
                                                   const wasm_module_t* rhs_mod,
                                                   const wasm_storagetype_t* rhs,
                                                   const wasm_recgroup_t* rhs_group,
                                                   uint8_t* memo) {
    if (lhs->kind != rhs->kind) return 0;
    if (lhs->kind == WASM_STORAGE_PACKED) return lhs->of.packed_type == rhs->of.packed_type;
    return wasm__cross_typed_valtype_equal_in_groups(lhs_mod,
                                                     lhs->of.valtype,
                                                     &lhs->ref_type,
                                                     lhs_group,
                                                     rhs_mod,
                                                     rhs->of.valtype,
                                                     &rhs->ref_type,
                                                     rhs_group,
                                                     memo);
}

static int wasm__cross_fieldtype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                 const wasm_fieldtype_t* lhs,
                                                 const wasm_recgroup_t* lhs_group,
                                                 const wasm_module_t* rhs_mod,
                                                 const wasm_fieldtype_t* rhs,
                                                 const wasm_recgroup_t* rhs_group,
                                                 uint8_t* memo) {
    return lhs->is_mutable == rhs->is_mutable &&
           wasm__cross_storagetype_equal_in_groups(lhs_mod, &lhs->storage, lhs_group,
                                                   rhs_mod, &rhs->storage, rhs_group,
                                                   memo);
}

static int wasm__cross_comptype_equal_in_groups(const wasm_module_t* lhs_mod,
                                                uint32_t lhs_idx,
                                                const wasm_recgroup_t* lhs_group,
                                                const wasm_module_t* rhs_mod,
                                                uint32_t rhs_idx,
                                                const wasm_recgroup_t* rhs_group,
                                                uint8_t* memo) {
    const wasm_comptype_t* lhs;
    const wasm_comptype_t* rhs;
    uint32_t i;

    if (!lhs_mod || !rhs_mod || lhs_idx >= lhs_mod->num_types || rhs_idx >= rhs_mod->num_types) return 0;
    lhs = &lhs_mod->types[lhs_idx];
    rhs = &rhs_mod->types[rhs_idx];

    if (lhs->kind != rhs->kind || lhs->is_final != rhs->is_final || lhs->num_supertypes != rhs->num_supertypes)
        return 0;

    for (i = 0; i < lhs->num_supertypes; i++) {
        if (!wasm__cross_type_index_equal_in_groups(lhs_mod,
                                                    lhs->supertypes[i],
                                                    lhs_group,
                                                    rhs_mod,
                                                    rhs->supertypes[i],
                                                    rhs_group,
                                                    memo)) {
            return 0;
        }
    }

    switch (lhs->kind) {
        case WASM_COMP_FUNC:
            return wasm__cross_functype_equal_in_groups(lhs_mod, &lhs->of.func, lhs_group,
                                                        rhs_mod, &rhs->of.func, rhs_group,
                                                        memo);
        case WASM_COMP_STRUCT:
            if (lhs->of.struct_.num_fields != rhs->of.struct_.num_fields) return 0;
            for (i = 0; i < lhs->of.struct_.num_fields; i++) {
                if (!wasm__cross_fieldtype_equal_in_groups(lhs_mod,
                                                           &lhs->of.struct_.fields[i],
                                                           lhs_group,
                                                           rhs_mod,
                                                           &rhs->of.struct_.fields[i],
                                                           rhs_group,
                                                           memo)) {
                    return 0;
                }
            }
            return 1;
        case WASM_COMP_ARRAY:
            return wasm__cross_fieldtype_equal_in_groups(lhs_mod,
                                                         &lhs->of.array.field,
                                                         lhs_group,
                                                         rhs_mod,
                                                         &rhs->of.array.field,
                                                         rhs_group,
                                                         memo);
        default:
            return 0;
    }
}

static int wasm__cross_recgroup_equal(const wasm_module_t* lhs_mod,
                                      const wasm_recgroup_t* lhs_group,
                                      const wasm_module_t* rhs_mod,
                                      const wasm_recgroup_t* rhs_group,
                                      uint8_t* memo) {
    uint32_t i;

    if (!lhs_group || !rhs_group) return 0;
    if (lhs_group->num_types != rhs_group->num_types) return 0;

    for (i = 0; i < lhs_group->num_types; i++) {
        if (!wasm__cross_comptype_equal_in_groups(lhs_mod,
                                                  lhs_group->first_type + i,
                                                  lhs_group,
                                                  rhs_mod,
                                                  rhs_group->first_type + i,
                                                  rhs_group,
                                                  memo)) {
            return 0;
        }
    }
    return 1;
}

static int wasm__cross_type_equal_visit(const wasm_module_t* lhs_mod,
                                        uint32_t lhs_idx,
                                        const wasm_module_t* rhs_mod,
                                        uint32_t rhs_idx,
                                        uint8_t* memo) {
    const wasm_recgroup_t* lhs_group;
    const wasm_recgroup_t* rhs_group;
    size_t idx;
    uint8_t state;
    int result;

    if (!lhs_mod || !rhs_mod || lhs_idx >= lhs_mod->num_types || rhs_idx >= rhs_mod->num_types) return 0;
    if (lhs_mod == rhs_mod) return wasm__type_equal(lhs_mod, lhs_idx, rhs_idx);

    lhs_group = &lhs_mod->rec_groups[lhs_mod->types[lhs_idx].rec_group];
    rhs_group = &rhs_mod->rec_groups[rhs_mod->types[rhs_idx].rec_group];
    if (lhs_group->num_types != rhs_group->num_types) return 0;
    if ((lhs_idx - lhs_group->first_type) != (rhs_idx - rhs_group->first_type)) return 0;

    idx = (size_t)lhs_idx * (size_t)rhs_mod->num_types + (size_t)rhs_idx;
    state = memo[idx];
    if (state == 1u || state == 2u) return 1;
    if (state == 3u) return 0;

    memo[idx] = 1u;
    result = wasm__cross_recgroup_equal(lhs_mod, lhs_group, rhs_mod, rhs_group, memo);
    memo[idx] = (uint8_t)(result ? 2u : 3u);
    return result;
}

static int wasm__cross_type_is_subtype_with_memo(const wasm_module_t* lhs_mod,
                                                 uint32_t lhs_idx,
                                                 const wasm_module_t* rhs_mod,
                                                 uint32_t rhs_idx,
                                                 uint8_t* memo) {
    const wasm_comptype_t* lhs_type;
    uint32_t i;

    if (!lhs_mod || !rhs_mod || lhs_idx >= lhs_mod->num_types || rhs_idx >= rhs_mod->num_types) return 0;
    if (lhs_mod == rhs_mod) return wasm__is_subtype(lhs_mod, lhs_idx, rhs_idx);
    if (wasm__cross_type_equal_visit(lhs_mod, lhs_idx, rhs_mod, rhs_idx, memo)) return 1;

    lhs_type = &lhs_mod->types[lhs_idx];
    for (i = 0; i < lhs_type->num_supertypes; i++) {
        uint32_t parent_idx = lhs_type->supertypes[i];

        if (parent_idx >= lhs_mod->num_types) continue;
        if (wasm__cross_type_is_subtype_with_memo(lhs_mod, parent_idx, rhs_mod, rhs_idx, memo)) return 1;
    }

    return 0;
}

WASM_PRIVATE int wasm__functype_equal_across_modules(const wasm_module_t* lhs_mod,
                                               uint32_t lhs_type_idx,
                                               const wasm_module_t* rhs_mod,
                                               uint32_t rhs_type_idx) {
    size_t memo_size;
    uint8_t* memo;
    int result;

    if (!lhs_mod || !rhs_mod || lhs_type_idx >= lhs_mod->num_types || rhs_type_idx >= rhs_mod->num_types)
        return 0;
    if (lhs_mod == rhs_mod) return wasm__type_equal(lhs_mod, lhs_type_idx, rhs_type_idx);
    if (rhs_mod->num_types != 0u &&
        (size_t)lhs_mod->num_types > ((size_t)-1) / (size_t)rhs_mod->num_types)
        return 0;

    memo_size = (size_t)lhs_mod->num_types * (size_t)rhs_mod->num_types;
    memo = memo_size ? (uint8_t*)wasm__malloc(&lhs_mod->rt->allocator, memo_size) : NULL;
    if (memo_size && !memo) return 0;
    if (memo_size) memset(memo, 0, memo_size);
    result = wasm__cross_type_equal_visit(lhs_mod, lhs_type_idx, rhs_mod, rhs_type_idx, memo);
    wasm__free(&lhs_mod->rt->allocator, memo);
    return result;
}

WASM_PRIVATE int wasm__functype_is_subtype_across_modules(const wasm_module_t* lhs_mod,
                                                    uint32_t lhs_type_idx,
                                                    const wasm_module_t* rhs_mod,
                                                    uint32_t rhs_type_idx) {
    uint8_t* memo;
    size_t memo_size;
    int result;

    if (!lhs_mod || !rhs_mod || lhs_type_idx >= lhs_mod->num_types || rhs_type_idx >= rhs_mod->num_types)
        return 0;
    if (lhs_mod == rhs_mod) return wasm__is_subtype(lhs_mod, lhs_type_idx, rhs_type_idx);

    if (rhs_mod->num_types != 0u &&
        (size_t)lhs_mod->num_types > ((size_t)-1) / (size_t)rhs_mod->num_types)
        return 0;

    memo_size = (size_t)lhs_mod->num_types * (size_t)rhs_mod->num_types;
    memo = memo_size ? (uint8_t*)wasm__malloc(&lhs_mod->rt->allocator, memo_size) : NULL;
    if (memo_size && !memo) return 0;
    if (memo_size) memset(memo, 0, memo_size);

    result = wasm__cross_type_is_subtype_with_memo(lhs_mod, lhs_type_idx, rhs_mod, rhs_type_idx, memo);

    wasm__free(&lhs_mod->rt->allocator, memo);
    return result;
}

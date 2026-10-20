#include "cross_module.h"
#include "../module.h"
#include "../runtime.h"
#include "canonical.h"
#include "references.h"
#include "../values.h"

static uint32_t wasm__cross_canonical_id(const wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return 0;
    return mod->types[type_idx].canonical_id;
}

WASM_PRIVATE int wasm__type_equal_across_modules(const wasm_module_t* lhs_mod,
                                                 uint32_t lhs_type_idx,
                                                 const wasm_module_t* rhs_mod,
                                                 uint32_t rhs_type_idx) {
    uint32_t id;

    if (lhs_mod == rhs_mod) return wasm__type_equal(lhs_mod, lhs_type_idx, rhs_type_idx);
    if (!lhs_mod || !rhs_mod || lhs_mod->rt != rhs_mod->rt) return 0;
    id = wasm__cross_canonical_id(lhs_mod, lhs_type_idx);
    return id != 0 && id == wasm__cross_canonical_id(rhs_mod, rhs_type_idx);
}

WASM_PRIVATE int wasm__type_is_subtype_across_modules(const wasm_module_t* lhs_mod,
                                                      uint32_t lhs_type_idx,
                                                      const wasm_module_t* rhs_mod,
                                                      uint32_t rhs_type_idx) {
    uint32_t id;
    uint32_t depth;

    if (lhs_mod == rhs_mod) return wasm__is_subtype(lhs_mod, lhs_type_idx, rhs_type_idx);
    if (!lhs_mod || !rhs_mod || lhs_mod->rt != rhs_mod->rt) return 0;
    id = wasm__cross_canonical_id(rhs_mod, rhs_type_idx);
    if (id == 0) return 0;
    for (depth = 0; depth < lhs_mod->num_types && lhs_type_idx < lhs_mod->num_types; depth++) {
        const wasm_comptype_t* type = &lhs_mod->types[lhs_type_idx];

        if (type->canonical_id == id) return 1;
        if (type->num_supertypes == 0) return 0;
        lhs_type_idx = type->supertypes[0];
    }
    return 0;
}

WASM_PRIVATE int wasm__valtype_is_subtype_across_modules(const wasm_module_t* lhs_mod,
                                                   wasm_valtype_t lhs_type,
                                                   const wasm_reftype_t* lhs_ref,
                                                   const wasm_module_t* rhs_mod,
                                                   wasm_valtype_t rhs_type,
                                                   const wasm_reftype_t* rhs_ref) {
    wasm_reftype_t subtype;
    wasm_reftype_t supertype;

    if (!lhs_mod) lhs_mod = rhs_mod;
    if (!rhs_mod) rhs_mod = lhs_mod;
    if (lhs_mod == rhs_mod || !wasm__is_ref_type(lhs_type) || !wasm__is_ref_type(rhs_type))
        return wasm__typed_valtype_is_subtype(rhs_mod, lhs_type, lhs_ref, rhs_type, rhs_ref);
    if (!wasm__get_effective_reftype(lhs_mod, lhs_type, lhs_ref, &subtype) ||
        !wasm__get_effective_reftype(rhs_mod, rhs_type, rhs_ref, &supertype))
        return 0;
    if (!subtype.has_type_index) return wasm__reftype_is_subtype(rhs_mod, &subtype, &supertype);
    if (!supertype.has_type_index) return wasm__reftype_is_subtype(lhs_mod, &subtype, &supertype);
    if (subtype.nullable && !supertype.nullable) return 0;
    if (supertype.exact)
        return subtype.exact &&
               wasm__type_equal_across_modules(lhs_mod, subtype.type_index, rhs_mod, supertype.type_index);
    return wasm__type_is_subtype_across_modules(lhs_mod, subtype.type_index, rhs_mod, supertype.type_index);
}

WASM_PRIVATE int wasm__valtype_equal_across_modules(const wasm_module_t* lhs_mod,
                                              wasm_valtype_t lhs_type,
                                              const wasm_reftype_t* lhs_ref,
                                              const wasm_module_t* rhs_mod,
                                              wasm_valtype_t rhs_type,
                                              const wasm_reftype_t* rhs_ref) {
    return wasm__valtype_is_subtype_across_modules(lhs_mod, lhs_type, lhs_ref, rhs_mod, rhs_type, rhs_ref) &&
           wasm__valtype_is_subtype_across_modules(rhs_mod, rhs_type, rhs_ref, lhs_mod, lhs_type, lhs_ref);
}

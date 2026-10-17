#ifndef WASM_SRC_TYPES_CANONICAL_H
#define WASM_SRC_TYPES_CANONICAL_H

#include "../internal/config.h"

WASM_PRIVATE void wasm__free_functype(wasm_functype_t* ft);

WASM_PRIVATE int wasm__functype_is_unspecified(const wasm_functype_t* ft);

WASM_PRIVATE int wasm__functype_is_zeroed(const wasm_functype_t* ft);

WASM_PRIVATE void wasm__functype_mark_unspecified(wasm_functype_t* ft);

WASM_PRIVATE void wasm__free_comptype(wasm_comptype_t* type);

WASM_PRIVATE wasm_error_t wasm__copy_functype(wasm_functype_t* dst, const wasm_functype_t* src);

WASM_PRIVATE int wasm__functype_equal(const wasm_functype_t* lhs, const wasm_functype_t* rhs);

WASM_PRIVATE wasm_error_t wasm__canonicalize_types(wasm_module_t* mod);

WASM_PRIVATE int wasm__type_equal(const wasm_module_t* mod, uint32_t lhs_idx, uint32_t rhs_idx);

WASM_PRIVATE int wasm__is_heap_subtype(const wasm_module_t* mod,
                                 wasm_valtype_t subtype,
                                 wasm_valtype_t supertype);

WASM_PRIVATE int wasm__is_valtype_subtype(const wasm_module_t* mod,
                                    wasm_valtype_t subtype,
                                    wasm_valtype_t supertype);

WASM_PRIVATE int wasm__is_storagetype_subtype(const wasm_module_t* mod,
                                        const wasm_storagetype_t* subtype,
                                        const wasm_storagetype_t* supertype);

WASM_PRIVATE int wasm__is_comptype_immediate_subtype(const wasm_module_t* mod,
                                               const wasm_comptype_t* subtype,
                                               const wasm_comptype_t* supertype);

WASM_PRIVATE wasm_functype_t* wasm__module_functype(wasm_module_t* mod, uint32_t type_idx);

WASM_PRIVATE const wasm_functype_t* wasm__module_const_functype(const wasm_module_t* mod, uint32_t type_idx);

WASM_PRIVATE const wasm_structtype_t* wasm__module_const_structtype(const wasm_module_t* mod, uint32_t type_idx);

WASM_PRIVATE const wasm_arraytype_t* wasm__module_const_arraytype(const wasm_module_t* mod, uint32_t type_idx);
WASM_PRIVATE_INLINE int wasm__is_subtype(const wasm_module_t* mod, uint32_t subtype_idx, uint32_t supertype_idx);

#endif /* WASM_SRC_TYPES_CANONICAL_H */

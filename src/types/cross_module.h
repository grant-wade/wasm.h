#ifndef WASM_SRC_TYPES_CROSS_MODULE_H
#define WASM_SRC_TYPES_CROSS_MODULE_H

#include "../internal/config.h"

WASM_PRIVATE int wasm__type_equal_across_modules(const wasm_module_t* lhs_mod,
                                                 uint32_t lhs_type_idx,
                                                 const wasm_module_t* rhs_mod,
                                                 uint32_t rhs_type_idx);

WASM_PRIVATE int wasm__type_is_subtype_across_modules(const wasm_module_t* lhs_mod,
                                                      uint32_t lhs_type_idx,
                                                      const wasm_module_t* rhs_mod,
                                                      uint32_t rhs_type_idx);

/* lhs_ref has type indices of lhs_mod and rhs_ref of rhs_mod; a module may be NULL when its type has none. */
WASM_PRIVATE int wasm__valtype_is_subtype_across_modules(const wasm_module_t* lhs_mod,
                                                   wasm_valtype_t lhs_type,
                                                   const wasm_reftype_t* lhs_ref,
                                                   const wasm_module_t* rhs_mod,
                                                   wasm_valtype_t rhs_type,
                                                   const wasm_reftype_t* rhs_ref);

WASM_PRIVATE int wasm__valtype_equal_across_modules(const wasm_module_t* lhs_mod,
                                              wasm_valtype_t lhs_type,
                                              const wasm_reftype_t* lhs_ref,
                                              const wasm_module_t* rhs_mod,
                                              wasm_valtype_t rhs_type,
                                              const wasm_reftype_t* rhs_ref);

#endif /* WASM_SRC_TYPES_CROSS_MODULE_H */

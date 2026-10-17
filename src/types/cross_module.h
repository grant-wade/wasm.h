#ifndef WASM_SRC_TYPES_CROSS_MODULE_H
#define WASM_SRC_TYPES_CROSS_MODULE_H

#include "../internal/config.h"

WASM_PRIVATE int wasm__functype_equal_across_modules(const wasm_module_t* lhs_mod,
                                               uint32_t lhs_type_idx,
                                               const wasm_module_t* rhs_mod,
                                               uint32_t rhs_type_idx);

WASM_PRIVATE int wasm__functype_is_subtype_across_modules(const wasm_module_t* lhs_mod,
                                                    uint32_t lhs_type_idx,
                                                    const wasm_module_t* rhs_mod,
                                                    uint32_t rhs_type_idx);

#endif /* WASM_SRC_TYPES_CROSS_MODULE_H */

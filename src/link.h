#ifndef WASM_LINK_H
#define WASM_LINK_H

#include "internal/config.h"

/* Follows *mod's function *func_idx through imports of other modules' registered exports to the
 * Wasm function that defines it. Returns nonzero, with *mod and *func_idx updated, if there is one. */
WASM_PRIVATE int wasm__linked_func_resolve(wasm_module_t** mod, uint32_t* func_idx);

/* Sets *out_mod and *out_type_idx to the type of the function that mod's function func_idx refers to
 * through inexact imports of registered exports. Returns 0 if that type is known only by a supertype. */
WASM_PRIVATE int wasm__func_dynamic_type(const wasm_module_t* mod, uint32_t func_idx,
                                   const wasm_module_t** out_mod, uint32_t* out_type_idx);

/* Sets *out_mod and *out_type_idx to the type of the function binding provides, with *out_mod NULL if its
 * module is freed. Returns 0 if that type is known only by a supertype. */
WASM_PRIVATE int wasm__func_binding_type(const wasm_import_t* binding, const wasm_module_t** out_mod,
                                         uint32_t* out_type_idx);

#endif /* WASM_LINK_H */

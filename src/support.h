#ifndef WASM_SUPPORT_H
#define WASM_SUPPORT_H

#include "internal/config.h"

WASM_PRIVATE_INLINE int wasm__uses_funcref_storage(wasm_valtype_t type);

WASM_PRIVATE_INLINE int wasm__uses_externref_storage(wasm_valtype_t type);

WASM_PRIVATE_INLINE int wasm__uses_exnref_storage(wasm_valtype_t type);

WASM_PRIVATE_INLINE int wasm__uses_contref_storage(wasm_valtype_t type);

WASM_PRIVATE_INLINE int wasm__uses_gc_ref_storage(wasm_valtype_t type);

#endif /* WASM_SUPPORT_H */

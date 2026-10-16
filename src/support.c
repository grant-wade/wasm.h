#include "support.h"
#include "internal/alloc.h"
#include <string.h>

#if defined(WASM__NEEDS_CALLOC_FALLBACK)
WASM_PRIVATE void* wasm__calloc_fallback(size_t count, size_t size) {
    size_t total;
    void* ptr;

    if (size != 0 && count > ((size_t)-1) / size) return NULL;

    total = count * size;
    ptr = WASM_MALLOC(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

#endif
WASM_PRIVATE_INLINE int wasm__uses_funcref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_FUNCREF || type == WASM_TYPE_NOFUNC;
}

WASM_PRIVATE_INLINE int wasm__uses_externref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_EXTERNREF || type == WASM_TYPE_NOEXTERN;
}

WASM_PRIVATE_INLINE int wasm__uses_exnref_storage(wasm_valtype_t type) {
    return type == WASM_TYPE_EXNREF || type == WASM_TYPE_NOEXN;
}

WASM_PRIVATE_INLINE int wasm__uses_gc_ref_storage(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_NONE:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            return 1;
        default:
            return 0;
    }
}

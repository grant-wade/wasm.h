#ifndef WASM_INTERNAL_ALLOC_H
#define WASM_INTERNAL_ALLOC_H

#include "config.h"

#include <stdlib.h>
#ifdef WASM_MALLOC
#define WASM__HAS_CUSTOM_MALLOC 1
#endif

#ifndef WASM_MALLOC
#define WASM_MALLOC(sz) malloc(sz)
#endif
#ifndef WASM_REALLOC
#define WASM_REALLOC(p, sz) realloc(p, sz)
#endif
#ifndef WASM_FREE
#define WASM_FREE(p) free(p)
#endif
#ifndef WASM_CALLOC
#if defined(WASM__HAS_CUSTOM_MALLOC)
#define WASM__NEEDS_CALLOC_FALLBACK 1
#else
#define WASM_CALLOC(n, sz) calloc(n, sz)
#endif
#endif
#ifndef WASM_GC_ALLOC
#define WASM_GC_ALLOC(sz) WASM_MALLOC(sz)
#endif
#ifndef WASM_GC_FREE
#define WASM_GC_FREE(p) WASM_FREE(p)
#endif

#if defined(WASM__NEEDS_CALLOC_FALLBACK)
WASM_PRIVATE void* wasm__calloc_fallback(size_t count, size_t size);
#define WASM_CALLOC(n, sz) wasm__calloc_fallback((n), (sz))
#endif

/* Return NULL when count * size overflows size_t instead of allocating a truncated block. */
WASM_PRIVATE void* wasm__malloc_array(size_t count, size_t size);
WASM_PRIVATE void* wasm__realloc_array(void* ptr, size_t count, size_t size);

#endif /* WASM_INTERNAL_ALLOC_H */

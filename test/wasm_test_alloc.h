#ifndef WASM_TEST_ALLOC_H
#define WASM_TEST_ALLOC_H

#include <stddef.h>

/* Shared by the modular regression harness and its separately built runtime. */
void* wasm_test_malloc(size_t size);
void* wasm_test_realloc(void* ptr, size_t size);
void wasm_test_free(void* ptr);
void* wasm_test_calloc(size_t count, size_t size);

#define WASM_MALLOC(sz) wasm_test_malloc(sz)
#define WASM_REALLOC(p, sz) wasm_test_realloc((p), (sz))
#define WASM_FREE(p) wasm_test_free(p)
#define WASM_CALLOC(n, sz) wasm_test_calloc((n), (sz))

#endif

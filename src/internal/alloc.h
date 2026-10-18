#ifndef WASM_INTERNAL_ALLOC_H
#define WASM_INTERNAL_ALLOC_H

#include "config.h"

/* Alignment of blocks from wasm__malloc, wasm__calloc, wasm__realloc, and the array helpers. */
#define WASM__ALLOC_ALIGN ((size_t)8u)

/* Alignment of linear memory data, the GC heap, and frame arena blocks. */
#define WASM__BLOCK_ALIGN ((size_t)16u)

/* Blocks that record their size before the returned pointer; free them with wasm__free. These
 * return NULL when count * size overflows size_t. p may be NULL. */
WASM_PRIVATE void* wasm__malloc(const wasm_allocator_t* a, size_t size);
WASM_PRIVATE void* wasm__calloc(const wasm_allocator_t* a, size_t count, size_t size);
WASM_PRIVATE void* wasm__realloc(const wasm_allocator_t* a, void* p, size_t size);
WASM_PRIVATE void* wasm__malloc_array(const wasm_allocator_t* a, size_t count, size_t size);
WASM_PRIVATE void* wasm__realloc_array(const wasm_allocator_t* a, void* p, size_t count, size_t size);
WASM_PRIVATE void wasm__free(const wasm_allocator_t* a, void* p);

/* Blocks whose size and alignment the caller passes back to resize and free them. A size of 0
 * is allocated as 1 byte; p may be NULL. */
WASM_PRIVATE void* wasm__alloc_sized(const wasm_allocator_t* a, size_t size, size_t align);
WASM_PRIVATE void* wasm__alloc_zeroed_sized(const wasm_allocator_t* a, size_t size, size_t align);
WASM_PRIVATE void* wasm__resize_sized(const wasm_allocator_t* a, void* p, size_t old_size, size_t new_size,
                                      size_t align);
WASM_PRIVATE void wasm__free_sized(const wasm_allocator_t* a, void* p, size_t size, size_t align);

/* Returns nonzero when a is non-NULL and has every function. */
WASM_PRIVATE int wasm__allocator_valid(const wasm_allocator_t* a);

#endif /* WASM_INTERNAL_ALLOC_H */

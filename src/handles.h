#ifndef WASM_HANDLES_H
#define WASM_HANDLES_H

#include "internal/config.h"

/* Open-addressing set of host-heap objects that reference values point to, so a
 * handle can be checked before it is dereferenced. */
typedef struct wasm__handle_set_t {
    void** slots;      /* NULL marks an empty slot */
    uint32_t capacity; /* zero or a power of two */
    uint32_t count;
} wasm__handle_set_t;

WASM_PRIVATE int wasm__handle_set_contains(const wasm__handle_set_t* set, const void* handle);

WASM_PRIVATE wasm_error_t wasm__handle_set_insert(const wasm_allocator_t* a, wasm__handle_set_t* set, void* handle);

/* Keeps the handles for which keep() returns nonzero and passes the rest to drop() with a. If the
 * smaller table cannot be allocated, every handle is kept. */
WASM_PRIVATE void wasm__handle_set_filter(const wasm_allocator_t* a, wasm__handle_set_t* set, int (*keep)(void*),
                                          void (*drop)(const wasm_allocator_t*, void*));

WASM_PRIVATE void wasm__handle_set_free(const wasm_allocator_t* a, wasm__handle_set_t* set);

#endif /* WASM_HANDLES_H */

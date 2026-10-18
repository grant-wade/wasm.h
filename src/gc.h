#ifndef WASM_GC_H
#define WASM_GC_H

#include "internal/config.h"

/* Collects and resizes the heap to fit `request` more bytes; on failure the heap is unchanged. */
WASM_PRIVATE wasm_error_t wasm__gc_collect(wasm_runtime_t* rt, size_t request);

#endif /* WASM_GC_H */

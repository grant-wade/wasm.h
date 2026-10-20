#ifndef WASM_GC_H
#define WASM_GC_H

#include "internal/config.h"

/* Collects and resizes the heap to fit `request` more bytes; on failure the heap is unchanged. */
WASM_PRIVATE wasm_error_t wasm__gc_collect(wasm_runtime_t* rt, size_t request);

/* mod is being freed; the GC objects and continuations that have its types keep them. */
WASM_PRIVATE void wasm__gc_retire_types(wasm_runtime_t* rt, wasm_module_t* mod);

WASM_PRIVATE void wasm__gc_free_retired_types(wasm_runtime_t* rt);

#endif /* WASM_GC_H */

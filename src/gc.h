#ifndef WASM_GC_H
#define WASM_GC_H

#include "internal/config.h"

/* Collects and resizes the heap to fit `request` more bytes; on failure the heap is unchanged. */
WASM_PRIVATE wasm_error_t wasm__gc_collect(wasm_runtime_t* rt, size_t request);

/* Returns nonzero if a GC object or continuation of rt has a type of mod. */
WASM_PRIVATE int wasm__gc_types_used(const wasm_runtime_t* rt, const wasm_module_t* mod);

/* Keeps mod, being freed, as the owner of its types for the GC objects and continuations that have them. Every
 * other part of mod must already be freed; mod is cleared but for its types and identity. */
WASM_PRIVATE void wasm__gc_retire_types(wasm_runtime_t* rt, wasm_module_t* mod);

WASM_PRIVATE void wasm__gc_free_retired_types(wasm_runtime_t* rt);

#endif /* WASM_GC_H */

#ifndef WASM_PLATFORM_EMSCRIPTEN_H
#define WASM_PLATFORM_EMSCRIPTEN_H

#include "../internal/config.h"

#if WASM_ENABLE_PLATFORM
#define WASM__EMSCRIPTEN_MODULE_NAME "env"
#define WASM__EMSCRIPTEN_MEMORY_NAME "memory"
WASM_PRIVATE void wasm__emscripten_state_destroy(wasm_runtime_t* rt);
WASM_PRIVATE wasm_error_t wasm__ensure_emscripten_memory_import(wasm_runtime_t* rt,
                                                          uint64_t min_pages,
                                                          uint64_t max_pages,
                                                          int is_64);
#endif

#endif /* WASM_PLATFORM_EMSCRIPTEN_H */

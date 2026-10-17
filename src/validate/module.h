#ifndef WASM_SRC_VALIDATE_MODULE_H
#define WASM_SRC_VALIDATE_MODULE_H

#include "../internal/config.h"

WASM_PRIVATE wasm_error_t wasm__set_validation_error(wasm_runtime_t* rt,
                                               uint32_t func_idx,
                                               uint32_t offset,
                                               const char* fmt,
                                               ...);

WASM_PRIVATE wasm_error_t wasm__validate_types(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__resolve_module_reftypes(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__validate_structural(wasm_module_t* mod);

#endif /* WASM_SRC_VALIDATE_MODULE_H */

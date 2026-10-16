#ifndef WASM_SRC_VALIDATE_FUNCTION_H
#define WASM_SRC_VALIDATE_FUNCTION_H

#include "../internal/config.h"

WASM_PRIVATE wasm_error_t wasm__validate_code(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__prepare_function(wasm_module_t* mod, uint32_t func_idx);

#endif /* WASM_SRC_VALIDATE_FUNCTION_H */

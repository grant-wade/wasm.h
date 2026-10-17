#ifndef WASM_SRC_VALIDATE_SIMD_H
#define WASM_SRC_VALIDATE_SIMD_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE wasm_error_t wasm__validator_validate_simd(wasm__validator_t* v, const uint8_t* at);

#endif /* WASM_SRC_VALIDATE_SIMD_H */

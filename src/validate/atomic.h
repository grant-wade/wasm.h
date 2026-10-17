#ifndef WASM_SRC_VALIDATE_ATOMIC_H
#define WASM_SRC_VALIDATE_ATOMIC_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE wasm_error_t wasm__validator_validate_atomic(wasm__validator_t* v, const uint8_t* at);

#endif /* WASM_SRC_VALIDATE_ATOMIC_H */

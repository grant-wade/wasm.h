#ifndef WASM_SRC_VALIDATE_GC_H
#define WASM_SRC_VALIDATE_GC_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE wasm_error_t wasm__validator_validate_gc(wasm__validator_t* v, const uint8_t* at);

#endif /* WASM_SRC_VALIDATE_GC_H */

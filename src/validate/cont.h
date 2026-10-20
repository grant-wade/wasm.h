#ifndef WASM_SRC_VALIDATE_CONT_H
#define WASM_SRC_VALIDATE_CONT_H

#include "../internal/config.h"
#include "state.h"

/* Validates the stack switching instruction op, 0xE0 through 0xE6, whose immediates follow at v->r. */
WASM_PRIVATE wasm_error_t wasm__validator_validate_cont(wasm__validator_t* v, const uint8_t* at, uint8_t op);

#endif /* WASM_SRC_VALIDATE_CONT_H */

#ifndef WASM_SRC_EXEC_EXCEPTIONS_H
#define WASM_SRC_EXEC_EXCEPTIONS_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE wasm_error_t wasm__report_uncaught_exception(wasm_runtime_t* rt);

/* Returns WASM_OK with cf at the handler, or clears its labels and returns WASM__ERR_EXCEPTION. */
WASM_PRIVATE wasm_error_t wasm__handle_exception_in_frame(wasm__call_frame_t* cf);

#endif /* WASM_SRC_EXEC_EXCEPTIONS_H */

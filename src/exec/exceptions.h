#ifndef WASM_SRC_EXEC_EXCEPTIONS_H
#define WASM_SRC_EXEC_EXCEPTIONS_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE wasm_error_t wasm__report_uncaught_exception(wasm_runtime_t* rt);

/* Looks for a handler for rt->pending_exception among the frame's labels. On a match the frame
 * resumes at cf->pc with the stack and labels the handler expects; otherwise the labels are
 * cleared and WASM__ERR_EXCEPTION is returned. */
WASM_PRIVATE wasm_error_t wasm__handle_exception_in_frame(wasm__call_frame_t* cf);

#endif /* WASM_SRC_EXEC_EXCEPTIONS_H */

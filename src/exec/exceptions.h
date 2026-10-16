#ifndef WASM_SRC_EXEC_EXCEPTIONS_H
#define WASM_SRC_EXEC_EXCEPTIONS_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE wasm_error_t wasm__report_uncaught_exception(wasm_runtime_t* rt);

WASM_PRIVATE int wasm__find_rethrow_label(const wasm__label_t* labels,
                                    uint32_t lsp,
                                    uint32_t depth,
                                    uint32_t* out_index);

WASM_PRIVATE wasm_error_t wasm__handle_exception_in_frame(wasm_module_t* mod,
                                                    wasm__label_t* labels,
                                                    uint32_t* label_sp,
                                                    wasm__reader_t* r,
                                                    uint32_t sp_base);

WASM_PRIVATE wasm_error_t wasm__do_branch(wasm_runtime_t* rt,
                                    wasm__label_t* labels,
                                    wasm__label_t* target,
                                    wasm__reader_t* r,
                                    uint32_t* label_sp,
                                    uint32_t depth_l);

WASM_PRIVATE wasm_error_t wasm__leave_label(wasm_runtime_t* rt, const wasm__label_t* label);

#endif /* WASM_SRC_EXEC_EXCEPTIONS_H */

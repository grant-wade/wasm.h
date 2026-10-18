#ifndef WASM_SRC_EXEC_INTERPRETER_H
#define WASM_SRC_EXEC_INTERPRETER_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results);

/* Continues rt's suspended call; host_results must match the yielding import's result types. */
WASM_PRIVATE wasm_error_t wasm__interp_resume(wasm_runtime_t* rt,
                                        const wasm_value_t* host_results, uint32_t num_host_results,
                                        wasm_value_t* results, uint32_t num_results);

WASM_PRIVATE void wasm__interp_cancel(wasm_runtime_t* rt);

/* Cancels rt's suspended call if any of its frames belongs to mod. */
WASM_PRIVATE void wasm__interp_cancel_using(wasm_runtime_t* rt, const wasm_module_t* mod);

#endif /* WASM_SRC_EXEC_INTERPRETER_H */

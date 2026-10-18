#ifndef WASM_SRC_EXEC_INTERPRETER_H
#define WASM_SRC_EXEC_INTERPRETER_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results);

#endif /* WASM_SRC_EXEC_INTERPRETER_H */

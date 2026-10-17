#ifndef WASM_SRC_EXEC_INTERPRETER_H
#define WASM_SRC_EXEC_INTERPRETER_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE void wasm__init_call_frame(wasm__call_frame_t* cf,
                                  wasm_module_t* mod,
                                  wasm_runtime_t* rt,
                                  uint32_t func_idx,
                                  wasm_func_t* func,
                                  wasm_functype_t* ft,
                                  wasm_value_t* locals,
                                  wasm__label_t* labels,
                                  wasm_value_t* results_dst,
                                  uint32_t num_results);

WASM_PRIVATE wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results);

#endif /* WASM_SRC_EXEC_INTERPRETER_H */

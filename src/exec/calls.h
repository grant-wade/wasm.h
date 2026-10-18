#ifndef WASM_SRC_EXEC_CALLS_H
#define WASM_SRC_EXEC_CALLS_H

#include "../internal/config.h"
#include "state.h"

WASM_PRIVATE void wasm__clear_label(wasm__label_t* label);

WASM_PRIVATE void wasm__clear_call_frame(wasm__call_frame_t* cf);

/* args holds the slots of the parameters, or is NULL to start them at their defaults. */
WASM_PRIVATE wasm_error_t wasm__push_call_frame(wasm_module_t* mod,
                                          uint32_t func_idx,
                                          const wasm__slot_t* args,
                                          wasm_value_t* results_dst,
                                          uint32_t num_results);

WASM_PRIVATE wasm_error_t wasm__push_root_call_frame(wasm_module_t* mod,
                                               uint32_t func_idx,
                                               const wasm_value_t* args,
                                               uint32_t num_args,
                                               wasm_value_t* results_dst,
                                               uint32_t num_results);

WASM_PRIVATE wasm_error_t wasm__finish_call_frame(wasm_runtime_t* rt, uint32_t base_frame_sp);

WASM_PRIVATE wasm_error_t wasm__handle_exception_frames(wasm_module_t* mod, uint32_t base_frame_sp);

#endif /* WASM_SRC_EXEC_CALLS_H */

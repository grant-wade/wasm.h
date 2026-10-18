#ifndef WASM_SRC_EXEC_FRAMES_H
#define WASM_SRC_EXEC_FRAMES_H

#include "../internal/config.h"

WASM_PRIVATE wasm_error_t wasm__call_frame_stack_init(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__call_frame_stack_destroy(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__clear_backtrace(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__capture_backtrace(wasm_runtime_t* rt, uint32_t base_frame_sp);

WASM_PRIVATE uint32_t wasm__visible_call_stack_depth(const wasm_runtime_t* rt);

WASM_PRIVATE int wasm__visible_call_frame_info(const wasm_runtime_t* rt,
                                         uint32_t depth,
                                         wasm_module_t** out_module,
                                         uint32_t* out_func_idx,
                                         uint32_t* out_offset);

#endif /* WASM_SRC_EXEC_FRAMES_H */

#ifndef WASM_SRC_EXEC_FRAMES_H
#define WASM_SRC_EXEC_FRAMES_H

#include "../internal/config.h"

WASM_PRIVATE int wasm__size_mul(size_t a, size_t b, size_t* out);

WASM_PRIVATE int wasm__frame_storage_size(uint32_t num_locals,
                                    uint32_t max_labels,
                                    size_t* out);

WASM_PRIVATE int wasm__module_frame_arena_requirement(wasm_module_t* mod, size_t* out);

WASM_PRIVATE wasm_error_t wasm__ensure_module_frame_arena(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__call_frame_stack_init(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__call_frame_stack_destroy(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__clear_backtrace(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__capture_backtrace(wasm_runtime_t* rt, uint32_t base_frame_sp);

WASM_PRIVATE uint32_t wasm__visible_call_stack_depth(const wasm_runtime_t* rt);

WASM_PRIVATE int wasm__visible_call_frame_info(const wasm_runtime_t* rt,
                                         uint32_t depth,
                                         uint32_t* out_func_idx,
                                         uint32_t* out_offset);

#endif /* WASM_SRC_EXEC_FRAMES_H */

#ifndef WASM_SRC_EXEC_FRAMES_H
#define WASM_SRC_EXEC_FRAMES_H

#include "../internal/config.h"

WASM_PRIVATE wasm_error_t wasm__call_frame_stack_init(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__call_frame_stack_destroy(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__clear_backtrace(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__capture_backtrace(wasm_runtime_t* rt, uint32_t base_frame_sp);

WASM_PRIVATE uint32_t wasm__visible_call_stack_depth(const wasm_runtime_t* rt);

#ifndef NDEBUG
/* Returns the slot kinds of frame frame_index's operand stack, covering *out_slots slots from its
 * base, or NULL if none of its slots holds a reference the collector traces. */
WASM_PRIVATE const uint32_t* wasm__frame_stack_map(const wasm_runtime_t* rt, uint32_t frame_index, uint32_t* out_slots);

/* Aborts unless the stack maps and local types of rt's frames agree with the tags of their slots. */
WASM_PRIVATE void wasm__check_stack_maps(const wasm_runtime_t* rt);
#endif

WASM_PRIVATE int wasm__visible_call_frame_info(const wasm_runtime_t* rt,
                                         uint32_t depth,
                                         wasm_module_t** out_module,
                                         uint32_t* out_func_idx,
                                         uint32_t* out_offset);

#endif /* WASM_SRC_EXEC_FRAMES_H */

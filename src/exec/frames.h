#ifndef WASM_SRC_EXEC_FRAMES_H
#define WASM_SRC_EXEC_FRAMES_H

#include "../internal/config.h"
#include "stack.h"

WASM_PRIVATE void wasm__clear_backtrace(wasm_runtime_t* rt);

/* Records the running frames innermost first, from rt->exec through its parents down to the frames above
 * base_frame_sp of base_stack. */
WASM_PRIVATE void wasm__capture_backtrace(wasm_runtime_t* rt, const wasm__exec_stack_t* base_stack,
                                          uint32_t base_frame_sp);

WASM_PRIVATE uint32_t wasm__visible_call_stack_depth(const wasm_runtime_t* rt);

/* Returns the slot kinds of s's frame frame_index's operand stack, covering *out_slots slots from its
 * base, or NULL if none of its slots holds a reference the collector traces. */
WASM_PRIVATE const uint32_t* wasm__frame_stack_map(const wasm__exec_stack_t* s, uint32_t frame_index,
                                                   uint32_t* out_slots);

/* Returns the type of the function s starts when first resumed, whose leading parameters its
 * operand slots hold, or NULL if s has a frame or no such function. */
WASM_PRIVATE const wasm_functype_t* wasm__stack_start_type(const wasm__exec_stack_t* s);

#ifndef NDEBUG
/* Aborts unless each reference slot of the frames on rt's live stacks, by stack map or local type,
 * holds null or a value of its kind the runtime knows. */
WASM_PRIVATE void wasm__check_stack_maps(const wasm_runtime_t* rt);
#endif

WASM_PRIVATE int wasm__visible_call_frame_info(const wasm_runtime_t* rt,
                                         uint32_t depth,
                                         wasm_module_t** out_module,
                                         uint32_t* out_func_idx,
                                         uint32_t* out_offset);

#endif /* WASM_SRC_EXEC_FRAMES_H */

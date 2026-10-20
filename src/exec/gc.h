#ifndef WASM_SRC_EXEC_GC_H
#define WASM_SRC_EXEC_GC_H

#include "../internal/config.h"
#include "../internal/slot.h"

WASM_PRIVATE wasm_error_t wasm__exec_gc(wasm_module_t* mod, wasm__call_frame_t* cf);

/* Runs struct.new, struct.new_default, array.new, array.new_default, or array.new_fixed, by its 0xFB subop,
 * on the operand stack of mod's runtime; count is the element count of array.new_fixed. */
WASM_PRIVATE wasm_error_t wasm__exec_gc_new(wasm_module_t* mod, uint32_t subop, uint32_t type_idx, uint32_t count);

/* Returns whether the reference in slot matches a cast target and type index as WASM__OP_REF_TEST takes them. */
WASM_PRIVATE int wasm__exec_cast_matches(const wasm_module_t* mod, uint32_t target, uint32_t type_idx,
                                         const wasm__slot_t* slot);

/* Returns the descriptor of the struct slot holds, or NULL if it holds no struct with one. */
WASM_PRIVATE wasm_gc_header_t* wasm__gc_slot_descriptor(const wasm_runtime_t* rt, const wasm__slot_t* slot);

#ifndef NDEBUG
/* Aborts unless slot holds null or a reference to an object of type type_idx of mod or a subtype. */
WASM_PRIVATE void wasm__gc_check_operand(wasm_module_t* mod, uint32_t type_idx, const wasm__slot_t* slot);
#endif

#endif /* WASM_SRC_EXEC_GC_H */

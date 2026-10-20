#ifndef WASM_SRC_EXEC_GC_H
#define WASM_SRC_EXEC_GC_H

#include "../internal/config.h"
#include "../internal/slot.h"

WASM_PRIVATE wasm_error_t wasm__exec_gc(wasm_module_t* mod, wasm__call_frame_t* cf);

/* Runs struct.new, struct.new_default, array.new, array.new_default, or array.new_fixed, by its 0xFB subop,
 * on the operand stack of mod's runtime; count is the element count of array.new_fixed. */
WASM_PRIVATE wasm_error_t wasm__exec_gc_new(wasm_module_t* mod, uint32_t subop, uint32_t type_idx, uint32_t count);

/* subop is 0x18 (br_on_cast) or 0x19 (br_on_cast_fail); slot is replaced by the cast result. */
WASM_PRIVATE wasm_error_t wasm__exec_br_on_cast(wasm_module_t* mod,
                                          uint32_t subop,
                                          const uint8_t* flags_at,
                                          const uint8_t* code_end,
                                          wasm__slot_t* slot,
                                          int* out_branch);

#ifndef NDEBUG
/* Aborts unless slot holds null or a reference to an object of type type_idx of mod or a subtype. */
WASM_PRIVATE void wasm__gc_check_operand(wasm_module_t* mod, uint32_t type_idx, const wasm__slot_t* slot);
#endif

#endif /* WASM_SRC_EXEC_GC_H */

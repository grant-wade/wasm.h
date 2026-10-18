#ifndef WASM_SRC_EXEC_GC_H
#define WASM_SRC_EXEC_GC_H

#include "../internal/config.h"

WASM_PRIVATE wasm_error_t wasm__exec_gc(wasm_module_t* mod, wasm__call_frame_t* cf);

/* Runs br_on_cast (subop 0x18) or br_on_cast_fail (0x19), whose flags byte is at flags_at, on the
 * operand value, which it replaces with the cast result; out_branch says whether to branch. */
WASM_PRIVATE wasm_error_t wasm__exec_br_on_cast(wasm_module_t* mod,
                                          uint32_t subop,
                                          const uint8_t* flags_at,
                                          const uint8_t* code_end,
                                          wasm_value_t* value,
                                          int* out_branch);

#endif /* WASM_SRC_EXEC_GC_H */

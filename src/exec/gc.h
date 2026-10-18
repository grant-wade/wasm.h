#ifndef WASM_SRC_EXEC_GC_H
#define WASM_SRC_EXEC_GC_H

#include "../internal/config.h"

WASM_PRIVATE wasm_error_t wasm__exec_gc(wasm_module_t* mod, wasm__call_frame_t* cf);

/* subop is 0x18 (br_on_cast) or 0x19 (br_on_cast_fail); value is replaced by the cast result. */
WASM_PRIVATE wasm_error_t wasm__exec_br_on_cast(wasm_module_t* mod,
                                          uint32_t subop,
                                          const uint8_t* flags_at,
                                          const uint8_t* code_end,
                                          wasm_value_t* value,
                                          int* out_branch);

#endif /* WASM_SRC_EXEC_GC_H */

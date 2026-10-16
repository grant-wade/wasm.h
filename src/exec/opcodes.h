#ifndef WASM_EXEC_OPCODES_H
#define WASM_EXEC_OPCODES_H

#include "../internal/exec.h"

WASM_PRIVATE wasm_error_t wasm__exec_gc(wasm_module_t* mod, wasm__call_frame_t* cf);
WASM_PRIVATE wasm_error_t wasm__exec_bulk(wasm_module_t* mod, wasm__call_frame_t* cf);
WASM_PRIVATE wasm_error_t wasm__exec_simd(wasm_module_t* mod, wasm__call_frame_t* cf);

WASM_PRIVATE wasm_error_t wasm__exec_numeric(wasm_runtime_t* rt, uint8_t op);

#endif /* WASM_EXEC_OPCODES_H */

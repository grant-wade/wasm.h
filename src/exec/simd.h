#ifndef WASM_SRC_EXEC_SIMD_H
#define WASM_SRC_EXEC_SIMD_H

#include "../internal/config.h"

WASM_PRIVATE wasm_error_t wasm__exec_simd(wasm_module_t* mod, wasm__call_frame_t* cf);

#endif /* WASM_SRC_EXEC_SIMD_H */

#ifndef WASM_SRC_EXEC_CONTROL_H
#define WASM_SRC_EXEC_CONTROL_H

#include "../internal/config.h"
#include "../module.h"

typedef struct wasm__try_clause_t {
    uint8_t opcode;
    uint32_t immediate;
    const uint8_t* body_start;
    const uint8_t* next_clause;
} wasm__try_clause_t;

WASM_PRIVATE wasm_error_t wasm__build_control_targets(wasm_func_t* func);

WASM_PRIVATE wasm_error_t wasm__build_module_control_targets(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__read_try_clause(const uint8_t* clause_ptr,
                                          const uint8_t* code_end,
                                          wasm__try_clause_t* out_clause);

#endif /* WASM_SRC_EXEC_CONTROL_H */

#ifndef WASM_INIT_EXPR_H
#define WASM_INIT_EXPR_H

#include "internal/config.h"
#include "reader.h"

WASM_PRIVATE wasm_error_t wasm__apply_active_data_segments(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__apply_active_elem_segments(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__eval_init_expr(wasm_module_t* mod, wasm__reader_t* r,
                                         uint32_t max_global_index,
                                         wasm_value_t* out_value);

#endif /* WASM_INIT_EXPR_H */

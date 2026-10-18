#ifndef WASM_INIT_EXPR_H
#define WASM_INIT_EXPR_H

#include "internal/config.h"
#include "reader.h"

WASM_PRIVATE wasm_error_t wasm__apply_active_data_segments(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__apply_active_elem_segments(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__eval_init_expr(wasm_module_t* mod, wasm__reader_t* r,
                                         uint32_t max_global_index,
                                         wasm_value_t* out_value);

/* Checks an init expression's value against its declared type. A compiled module's imported
   globals hold placeholders known only by type, so a value derived from one is checked by
   value type alone: a placeholder reference may be null where a non-nullable type is declared. */
WASM_PRIVATE int wasm__init_value_matches(wasm_module_t* mod,
                                    const wasm_value_t* value,
                                    wasm_valtype_t type,
                                    const wasm_reftype_t* ref_type);

#endif /* WASM_INIT_EXPR_H */

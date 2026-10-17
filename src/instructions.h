#ifndef WASM_SRC_INSTRUCTIONS_H
#define WASM_SRC_INSTRUCTIONS_H

#include "internal/config.h"
#include "reader.h"

typedef struct wasm__blocktype_t {
    uint32_t param_arity;
    uint32_t result_arity;
    const wasm_valtype_t* params;
    const wasm_valtype_t* results;
    const wasm_reftype_t* param_reftypes;
    const wasm_reftype_t* result_reftypes;
    wasm_valtype_t inline_result;
    wasm_reftype_t inline_result_reftype;
} wasm__blocktype_t;
typedef struct wasm__try_table_clause_t {
    uint8_t kind;
    uint32_t tag_index;
    uint32_t label_depth;
} wasm__try_table_clause_t;
typedef struct wasm__memarg_t {
    uint32_t align_log2;
    uint32_t memory_index;
    uint8_t has_memory_index;
    uint64_t offset;
} wasm__memarg_t;

WASM_PRIVATE void wasm__skip_blocktype(wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__read_try_table_clause(wasm__reader_t* r,
                                                wasm__try_table_clause_t* out_clause);

WASM_PRIVATE wasm_error_t wasm__read_blocktype(wasm_module_t* mod, wasm__reader_t* r, wasm__blocktype_t* blocktype);

WASM_PRIVATE void wasm__skip_immediates(uint8_t op, wasm__reader_t* r);

WASM_PRIVATE uint32_t wasm__simd_memory_align_log2(uint32_t subop);

WASM_PRIVATE uint32_t wasm__simd_lane_limit(uint32_t subop);

WASM_PRIVATE wasm_error_t wasm__read_memarg(wasm__reader_t* r, wasm__memarg_t* out_memarg);

WASM_PRIVATE wasm_error_t wasm__read_typed_select_immediate(wasm_module_t* mod,
                                                      wasm__reader_t* r,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype);

#endif /* WASM_SRC_INSTRUCTIONS_H */

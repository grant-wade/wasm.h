#ifndef WASM_SRC_TYPES_DECODE_H
#define WASM_SRC_TYPES_DECODE_H

#include "../internal/config.h"
#include "../reader.h"

WASM_PRIVATE wasm_error_t wasm__append_types(wasm_module_t* mod, uint32_t count, uint32_t* out_base);

WASM_PRIVATE wasm_error_t wasm__append_rec_groups(wasm_module_t* mod, uint32_t count, uint32_t* out_base);

WASM_PRIVATE wasm_error_t wasm__decode_type_entry(wasm_module_t* mod,
                                            wasm__reader_t* r,
                                            uint8_t opcode,
                                            uint32_t rec_group_idx,
                                            wasm_comptype_t* type);

#endif /* WASM_SRC_TYPES_DECODE_H */

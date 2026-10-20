#ifndef WASM_VALIDATE_STATE_H
#define WASM_VALIDATE_STATE_H

#include "../reader.h"

#define WASM__TYPE_BOT ((wasm_valtype_t)0xFF)
struct wasm__val_frame_t {
    uint8_t opcode;
    uint32_t height;
    const wasm_valtype_t* param_types;
    const wasm_reftype_t* param_reftypes;
    uint32_t num_params;
    const wasm_valtype_t* result_types;
    const wasm_reftype_t* result_reftypes;
    uint32_t num_results;
    wasm_valtype_t inline_result;
    wasm_reftype_t inline_result_reftype;
    int unreachable;
    int seen_else;
    int in_catch;
    int seen_catch_all;
    uint64_t* local_init;
    uint64_t* local_init_entry;
};
typedef struct wasm__validator_t {
    wasm_module_t* mod;
    uint32_t func_idx;
    const uint8_t* body_start;
    wasm__reader_t r;
    wasm_valtype_t* stack;
    wasm_reftype_t* stack_reftypes;
    uint32_t stack_capacity;
    uint32_t sp;
    uint32_t* stack_slots; /* stack_slots[i] is the slot height below stack entry i */
    uint32_t* stack_refs;  /* stack_refs[i] counts the reference entries below stack entry i */
    uint32_t max_slots;
    uint32_t num_maps;       /* entries in rt->validator_maps */
    uint32_t num_map_bits;   /* words in rt->validator_map_bits */
    uint32_t last_map_bits;  /* where the latest map's kinds start */
    uint32_t last_map_slots; /* slots the latest map covers */
    wasm__val_frame_t* frames;
    uint32_t frame_capacity;
    uint32_t fp;
    uint32_t max_fp;
    uint64_t* local_init_bits;
    uint64_t* local_init_entry_bits;
    uint32_t local_init_words;
} wasm__validator_t;

#endif /* WASM_VALIDATE_STATE_H */

#ifndef WASM_SRC_EXEC_TRANSLATE_H
#define WASM_SRC_EXEC_TRANSLATE_H

#include "../internal/config.h"
#include "../module.h"

/* Translated opcodes beyond the core set. A branch descriptor is four words: target, arity,
 * stack height, and WASM__BRANCH_* flags with the handler labels to pop in the upper bits. */
enum {
    WASM__OP_BR = 0x100,          /* target */
    WASM__OP_BR_LOOP,             /* target; polls for interrupts */
    WASM__OP_BR_X,                /* branch descriptor */
    WASM__OP_BR_IF,               /* target */
    WASM__OP_BR_IF_LOOP,          /* target */
    WASM__OP_BR_IF_X,             /* branch descriptor */
    WASM__OP_BR_TABLE,            /* count, then count + 1 branch descriptors */
    WASM__OP_LOCAL_GET_N,         /* slot offset, slot count; a local of vector type */
    WASM__OP_LOCAL_SET_N,         /* slot offset, slot count */
    WASM__OP_LOCAL_TEE_N,         /* slot offset, slot count */
    WASM__OP_DROP_N,              /* slot count */
    WASM__OP_SELECT_N,            /* slot count of each operand */
    WASM__OP_MEMORY_X,            /* Wasm opcode, memory index, offset low and high words */
    WASM__OP_TRY,                 /* clause table index, stack height */
    WASM__OP_TRY_TABLE,           /* stack height, clause count, then six words per clause */
    WASM__OP_TRY_LEAVE,           /* target; ends a try body or catch body that runs into the next clause */
    WASM__OP_TRY_END,             /* ends a try or try_table */
    WASM__OP_BR_ON_CAST,          /* subopcode, bytecode offset of its flags, branch descriptor */
    WASM__OP_COUNT
};

#define WASM__BRANCH_LOOP 0x1u
#define WASM__BRANCH_RETURN 0x2u
#define WASM__BRANCH_POPS_SHIFT 8u

/* WASM__OP_TRY table: count, then kind, tag or delegate skip count, and body index per clause. */
#define WASM__CLAUSE_CATCH 0u
#define WASM__CLAUSE_CATCH_ALL 1u
#define WASM__CLAUSE_DELEGATE 2u

/* What validating a function body tells its translation. */
typedef struct wasm__body_info_t {
    const uint32_t* heights;      /* operand stack slots before the instruction at each byte offset */
    const uint8_t* operand_types; /* type each drop and select operates on, at its byte offset */
    uint32_t max_height;          /* greatest slot height the body reaches */
    const uint32_t* maps;         /* stack maps as in wasm_func_t */
    uint32_t num_maps;
    const uint32_t* map_bits;
    uint32_t num_map_bits;
} wasm__body_info_t;

WASM_PRIVATE wasm_error_t wasm__translate_function(wasm_module_t* mod, uint32_t func_idx, const wasm__body_info_t* info);

WASM_PRIVATE wasm_error_t wasm__copy_translation(const wasm_allocator_t* a, wasm_func_t* dst, const wasm_func_t* src);

WASM_PRIVATE void wasm__free_translation(const wasm_allocator_t* a, wasm_func_t* func);

/* Returns the bytecode offset of the instruction translated to the word at op. */
WASM_PRIVATE uint32_t wasm__translated_offset(const wasm_func_t* func, const uint32_t* op);

#endif /* WASM_SRC_EXEC_TRANSLATE_H */

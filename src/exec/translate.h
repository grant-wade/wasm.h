#ifndef WASM_SRC_EXEC_TRANSLATE_H
#define WASM_SRC_EXEC_TRANSLATE_H

#include "../internal/config.h"
#include "../module.h"

/* Validated bodies run as arrays of 32-bit words: an opcode followed by fixed-width immediates.
 * Core instructions keep their Wasm opcode; the rest use the values below. A branch descriptor is
 * four words: target index, arity, the target label's stack height above the frame's base, and
 * WASM__BRANCH_* flags with the number of handler labels to pop in the upper bits. */
enum {
    WASM__OP_BR = 0x100,          /* target */
    WASM__OP_BR_LOOP,             /* target; polls for interrupts */
    WASM__OP_BR_X,                /* branch descriptor */
    WASM__OP_BR_IF,               /* target */
    WASM__OP_BR_IF_LOOP,          /* target */
    WASM__OP_BR_IF_X,             /* branch descriptor */
    WASM__OP_BR_TABLE,            /* count, then count + 1 branch descriptors */
    WASM__OP_LOCAL_GET_REF,       /* local index; a local of reference type */
    WASM__OP_LOCAL_SET_REF,       /* local index */
    WASM__OP_LOCAL_TEE_REF,       /* local index */
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

/* Legacy try clause kinds in a WASM__OP_TRY table: count, then kind, tag or skip count, and body
 * index per clause. A delegate skips that many handler labels before the search continues. */
#define WASM__CLAUSE_CATCH 0u
#define WASM__CLAUSE_CATCH_ALL 1u
#define WASM__CLAUSE_DELEGATE 2u

/* Translates a function that has just passed validation. heights holds the operand stack height
 * the validator saw before the instruction starting at each byte offset of the body. */
WASM_PRIVATE wasm_error_t wasm__translate_function(wasm_module_t* mod, uint32_t func_idx, const uint32_t* heights);

WASM_PRIVATE wasm_error_t wasm__copy_translation(wasm_func_t* dst, const wasm_func_t* src);

WASM_PRIVATE void wasm__free_translation(wasm_func_t* func);

/* Returns the bytecode offset of the instruction translated to the word at op. */
WASM_PRIVATE uint32_t wasm__translated_offset(const wasm_func_t* func, const uint32_t* op);

#endif /* WASM_SRC_EXEC_TRANSLATE_H */

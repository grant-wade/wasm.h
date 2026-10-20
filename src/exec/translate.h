#ifndef WASM_SRC_EXEC_TRANSLATE_H
#define WASM_SRC_EXEC_TRANSLATE_H

#include "../internal/config.h"
#include "../module.h"

/* Ops that each run a straight-line sequence of instructions left in place after their first
 * opcode word: name, first instruction's opcode, instructions run, and word offset of the last.
 * L is local.get, C i32.const, J i64.const, D f64.const, V v128.const, S local.set, T local.tee,
 * BIN and I64BIN an i32 or i64 binary or comparison that cannot trap, CMP an i32 comparison,
 * F64BIN an f64 add, sub, mul, or div, SIMD_BIN a WASM__OP_SIMD_BINARY instruction, BRIF br_if,
 * LOAD32 a 4-byte zero-extending load, LOAD64 an 8-byte load, STORE32 a 4-byte store, and STORE64
 * an 8-byte store; LC_LOADn runs an i32.add between the constant and the load. ADD, AND, XOR,
 * SHL, SHR_U, ROTL, and MUL are those instructions on i64 after J and on i32 otherwise, and
 * STRUCT_GET is struct.get, struct.get_s, or struct.get_u. */
#define WASM__FUSED_OPS(X)          \
    X(LC_ADD, 0x20, 3, 4)           \
    X(LC_AND, 0x20, 3, 4)           \
    X(LC_XOR, 0x20, 3, 4)           \
    X(LC_SHL, 0x20, 3, 4)           \
    X(LC_SHR_U, 0x20, 3, 4)         \
    X(LC_ROTL, 0x20, 3, 4)          \
    X(LC_BIN, 0x20, 3, 4)           \
    X(LL_ADD, 0x20, 3, 4)           \
    X(LL_BIN, 0x20, 3, 4)           \
    X(C_ADD, 0x41, 2, 2)            \
    X(C_AND, 0x41, 2, 2)            \
    X(C_XOR, 0x41, 2, 2)            \
    X(C_SHL, 0x41, 2, 2)            \
    X(C_SHR_U, 0x41, 2, 2)          \
    X(C_ROTL, 0x41, 2, 2)           \
    X(C_BIN, 0x41, 2, 2)            \
    X(L_ADD, 0x20, 2, 2)            \
    X(L_BIN, 0x20, 2, 2)            \
    X(LC_ADD_S, 0x20, 4, 5)         \
    X(LC_ADD_T, 0x20, 4, 5)         \
    X(LC_ADD_T_S, 0x20, 5, 7)       \
    X(S_LC_ADD_T, 0x21, 5, 7)       \
    X(LC_BIN_S, 0x20, 4, 5)         \
    X(LL_BIN_S, 0x20, 4, 5)         \
    X(ADD_S, 0x6A, 2, 1)            \
    X(ADD_T, 0x6A, 2, 1)            \
    X(S_L, 0x21, 2, 2)              \
    X(T_L, 0x22, 2, 2)              \
    X(T_BRIF_LOOP, 0x22, 2, 2)      \
    X(C_CMP_BRIF, 0x41, 3, 3)       \
    X(C_CMP_BRIF_LOOP, 0x41, 3, 3)  \
    X(L_CMP_BRIF, 0x20, 3, 3)       \
    X(L_CMP_BRIF_LOOP, 0x20, 3, 3)  \
    X(LC_CMP_BRIF, 0x20, 4, 5)      \
    X(LC_CMP_BRIF_LOOP, 0x20, 4, 5) \
    X(LC_CMP_IF, 0x20, 4, 5)        \
    X(LL_CMP_BRIF, 0x20, 4, 5)      \
    X(LL_CMP_BRIF_LOOP, 0x20, 4, 5) \
    X(L_LOAD32, 0x20, 2, 2)         \
    X(L_LOAD64, 0x20, 2, 2)         \
    X(L_LOAD8U, 0x20, 2, 2)         \
    X(LC_LOAD32, 0x20, 4, 5)        \
    X(LC_LOAD64, 0x20, 4, 5)        \
    X(LC_LOAD8U, 0x20, 4, 5)        \
    X(L_STORE32, 0x20, 2, 2)        \
    X(L_STORE64, 0x20, 2, 2)        \
    X(L_F64BIN, 0x20, 2, 2)         \
    X(LL_F64BIN, 0x20, 3, 4)        \
    X(D_F64BIN, 0x44, 2, 3)         \
    X(J_ADD, 0x42, 2, 3)            \
    X(J_I64BIN, 0x42, 2, 3)         \
    X(LJ_AND, 0x20, 3, 5)           \
    X(LJ_MUL, 0x20, 3, 5)           \
    X(LJ_SHR_U, 0x20, 3, 5)         \
    X(LJ_I64BIN, 0x20, 3, 5)        \
    X(L_S, 0x20, 2, 2)              \
    X(L_S_L_S, 0x20, 4, 6)          \
    X(C_S, 0x41, 2, 2)              \
    X(T_S, 0x22, 2, 2)              \
    X(L_T, 0x20, 2, 2)              \
    X(L_STRUCT_GET, 0x20, 2, 2)     \
    X(V_SIMD_BIN, WASM__OP_V128_CONST, 2, 5)

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
    WASM__OP_LOCAL_GET_V128,      /* slot offset */
    WASM__OP_LOCAL_SET_V128,      /* slot offset */
    WASM__OP_LOCAL_TEE_V128,      /* slot offset */
    WASM__OP_DROP_V128,
    WASM__OP_SELECT_V128,
    WASM__OP_MEMORY_X,            /* Wasm opcode, memory index, offset low and high words */
    WASM__OP_TRY,                 /* clause table index, stack height */
    WASM__OP_TRY_TABLE,           /* stack height, clause count, then six words per clause */
    WASM__OP_TRY_LEAVE,           /* target; ends a try body or catch body that runs into the next clause */
    WASM__OP_BR_ON_CAST,          /* cast target, type index, branch descriptor */
    WASM__OP_REF_TEST,            /* cast target, type index */
    WASM__OP_REF_CAST,            /* cast target, type index */
    WASM__OP_REF_CAST_DESC,       /* cast target, type index */
    WASM__OP_BR_ON_CAST_DESC,     /* cast target, type index, branch descriptor */
    WASM__OP_STRUCT_GET,          /* field byte offset, storage code, type index */
    WASM__OP_STRUCT_SET,          /* field byte offset, storage code, type index */
    WASM__OP_ARRAY_GET,           /* data byte offset, element size, storage code, type index */
    WASM__OP_ARRAY_SET,           /* data byte offset, element size, storage code, type index */
    WASM__OP_ARRAY_LEN,
    WASM__OP_REF_I31,
    WASM__OP_I31_GET_S,
    WASM__OP_I31_GET_U,
    WASM__OP_TRUNC_SAT,           /* 0xFC subopcode */
    WASM__OP_GC_NEW,              /* 0xFB subopcode, type index, element count of array.new_fixed */
    WASM__OP_V128_CONST,          /* the 16 bytes of the value */
    WASM__OP_SIMD_UNARY,          /* 0xFD subopcode */
    WASM__OP_SIMD_BINARY,         /* 0xFD subopcode */
    WASM__OP_SIMD_TERNARY,        /* 0xFD subopcode */
    WASM__OP_SIMD_SHIFT,          /* 0xFD subopcode */
    WASM__OP_SIMD_TEST,           /* 0xFD subopcode */
    WASM__OP_SIMD_SPLAT,          /* 0xFD subopcode */
    WASM__OP_SIMD_EXTRACT_LANE,   /* 0xFD subopcode, lane */
    WASM__OP_SIMD_REPLACE_LANE,   /* 0xFD subopcode, lane */
    WASM__OP_I8X16_SHUFFLE,       /* the 16 lane index bytes */
    WASM__OP_V128_LOAD,           /* offset; memory 0, 32-bit */
    WASM__OP_V128_STORE,          /* offset; memory 0, 32-bit */
    WASM__OP_SIMD_MEMORY,         /* 0xFD subopcode, WASM__SIMD_ACCESS word, offset; memory 0, 32-bit */
    WASM__OP_SIMD_MEMORY_X,       /* 0xFD subopcode, WASM__SIMD_ACCESS word, memory index, offset low and high words */
    WASM__OP_TRY_END,             /* ends a try or try_table */
    WASM__OP_END,                 /* ends the function body */
#define WASM__FUSED_ENUM(name, first, units, last) WASM__OP_##name,
    WASM__FUSED_OPS(WASM__FUSED_ENUM)
#undef WASM__FUSED_ENUM
    WASM__OP_COUNT
};
/* Ops from WASM__OP_FREE up to WASM__OP_FUSED cost no fuel. */
#define WASM__OP_FREE WASM__OP_TRY_END
#define WASM__OP_FUSED WASM__OP_LC_ADD

/* What a SIMD memory instruction accesses: its lane, the slots from its address operand to the top of the
 * stack, and the bytes it reads or writes. */
#define WASM__SIMD_ACCESS(lane, depth, width) ((lane) | ((depth) << 8) | ((width) << 16))
#define WASM__SIMD_ACCESS_LANE(word) ((word) & 0xFFu)
#define WASM__SIMD_ACCESS_DEPTH(word) (((word) >> 8) & 0xFFu)
#define WASM__SIMD_ACCESS_WIDTH(word) ((word) >> 16)

#define WASM__BRANCH_LOOP 0x1u
#define WASM__BRANCH_RETURN 0x2u
#define WASM__BRANCH_POPS_SHIFT 8u

/* What decides whether a reference that is not null matches a cast target: a word with its heap type in bits 0-7,
 * one of these in bits 8-11, and WASM__CAST_* flags, then a type index, 0 for an abstract heap type. */
enum {
    WASM__CAST_SUBTYPE,     /* the display of its struct or array type holds the target type */
    WASM__CAST_EXACT_TYPE,  /* its struct or array type is the target type */
    WASM__CAST_STRUCT,      /* it is a struct */
    WASM__CAST_ARRAY,       /* it is an array */
    WASM__CAST_EQ,          /* it is an i31, a struct, or an array */
    WASM__CAST_I31,         /* it is an i31 */
    WASM__CAST_TOP,         /* every reference matches */
    WASM__CAST_BOTTOM,      /* no reference matches */
    WASM__CAST_GENERAL      /* wasm__exec_cast_matches decides */
};
#define WASM__CAST_KIND_SHIFT 8u
#define WASM__CAST_HEAP(word) ((wasm_valtype_t)((word) & 0xFFu))
#define WASM__CAST_KIND(word) (((word) >> WASM__CAST_KIND_SHIFT) & 0xFu)
#define WASM__CAST_NULLABLE 0x1000u
#define WASM__CAST_EXACT 0x2000u
#define WASM__CAST_INDEXED 0x4000u
#define WASM__CAST_ON_FAIL 0x8000u

/* WASM__OP_TRY table: count, then kind, tag or delegate skip count, and body index per clause. */
#define WASM__CLAUSE_CATCH 0u
#define WASM__CLAUSE_CATCH_ALL 1u
#define WASM__CLAUSE_DELEGATE 2u

/* Words per entry of a resume handler table: kind (0 for a label, 1 for switch), tag, the label's
 * continuation type index or UINT32_MAX, and a branch descriptor, all zero for switch. */
#define WASM__HANDLER_WORDS 7u

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

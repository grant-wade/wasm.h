#ifndef WASM_SRC_EXEC_SIMD_H
#define WASM_SRC_EXEC_SIMD_H

#include "../internal/config.h"
#include "../internal/slot.h"

/* Each v128 operand spans WASM__V128_SLOTS slots, and a result overwrites the slots of the first operand.
 * subop is the instruction's 0xFD subopcode, which must belong to the helper's shape. */

WASM_PRIVATE void wasm__exec_simd_unary(uint32_t subop, wasm__slot_t* operand);

/* rhs points at the second operand's 16 bytes, in slots or in a v128.const immediate. */
WASM_PRIVATE void wasm__exec_simd_binary(uint32_t subop, wasm__slot_t* lhs, const void* rhs);

/* operands holds the three v128 operands in order. */
WASM_PRIVATE void wasm__exec_simd_ternary(uint32_t subop, wasm__slot_t* operands);

WASM_PRIVATE void wasm__exec_simd_shift(uint32_t subop, wasm__slot_t* operand, uint32_t amount);

/* Returns the i32 result of an any_true, all_true, or bitmask instruction. */
WASM_PRIVATE uint32_t wasm__exec_simd_test(uint32_t subop, const wasm__slot_t* operand);

/* slot holds the scalar operand; the result spans it and the slot above it. */
WASM_PRIVATE void wasm__exec_simd_splat(uint32_t subop, wasm__slot_t* slot);

/* The scalar result overwrites the first slot of operand. */
WASM_PRIVATE void wasm__exec_simd_extract_lane(uint32_t subop, uint32_t lane, wasm__slot_t* operand);

WASM_PRIVATE void wasm__exec_simd_replace_lane(uint32_t subop, uint32_t lane, wasm__slot_t* vector,
                                               const wasm__slot_t* scalar);

/* mask points at the 16 lane index bytes. */
WASM_PRIVATE void wasm__exec_simd_shuffle(wasm__slot_t* lhs, const wasm__slot_t* rhs, const void* mask);

/* Runs a load, store, lane load, or lane store at p, whose operands end at sp; returns the new sp. */
WASM_PRIVATE wasm__slot_t* wasm__exec_simd_memory(uint32_t subop, uint32_t lane, wasm__slot_t* sp, uint8_t* p);

#endif /* WASM_SRC_EXEC_SIMD_H */

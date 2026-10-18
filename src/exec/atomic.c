#include "atomic.h"
#include "state.h"
#include "../atomics.h"
#include "../instructions.h"
#include "../memory_internal.h"
#include "../module.h"
#include "../platform/threads.h"
#include "../reader.h"
#include "../runtime.h"
#include <stdio.h>

static uint64_t wasm__atomic_operand(wasm_value_t value, wasm_valtype_t type) {
    return type == WASM_TYPE_I32 ? (uint64_t)(uint32_t)value.of.i32 : (uint64_t)value.of.i64;
}

static wasm_value_t wasm__atomic_result(uint64_t value, wasm_valtype_t type) {
    return type == WASM_TYPE_I32 ? wasm_i32((int32_t)(uint32_t)value) : wasm_i64((int64_t)value);
}

/* Applies a 0x1E-0x47 read-modify-write subop; the group of seven selects add, sub, and, or, xor, xchg. */
static uint64_t wasm__atomic_rmw(uint8_t* p, uint32_t size, uint32_t subop, uint64_t operand) {
    uint32_t group = (subop - 0x1Eu) / 7u;
    uint64_t old = wasm__atomic_load(p, size);

    for (;;) {
        uint64_t desired;
        uint64_t observed;

        switch (group) {
            case 0:
                desired = old + operand;
                break;
            case 1:
                desired = old - operand;
                break;
            case 2:
                desired = old & operand;
                break;
            case 3:
                desired = old | operand;
                break;
            case 4:
                desired = old ^ operand;
                break;
            default:
                desired = operand;
                break;
        }
        observed = wasm__atomic_compare_exchange(p, size, old, desired);
        if (observed == old) return old;
        old = observed;
    }
}

WASM_PRIVATE wasm_error_t wasm__exec_atomic(wasm_module_t* mod, wasm__call_frame_t* cf) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err = WASM_OK;
    uint32_t subop = wasm__read_leb128_u32(&cf->r);
    uint32_t size_log2;
    uint32_t size;
    wasm_valtype_t type;
    wasm__memarg_t memarg;
    wasm_value_t operand = wasm_i64(0);
    wasm_value_t replacement = wasm_i64(0);
    wasm_value_t base;
    wasm_memory_t* memory;
    uint64_t addr;
    uint8_t* p;

    if (subop == 0x03) {
        wasm__read_u8(&cf->r);
        wasm__atomic_fence();
        return WASM_OK;
    }
    if (!wasm__atomic_access(subop, &size_log2, &type)) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown prefixed opcode 0xFE 0x%X", (unsigned)subop);
        return WASM_ERR_MALFORMED;
    }
    err = wasm__read_memarg(&cf->r, &memarg);
    if (err != WASM_OK) return err;
    size = 1u << size_log2;

    if (subop == 0x01 || subop == 0x02) {
        replacement = WASM__POP(rt);
        operand = WASM__POP(rt);
    } else if (subop >= 0x48) {
        replacement = WASM__POP(rt);
        operand = WASM__POP(rt);
    } else if (subop == 0x00 || subop >= 0x17) {
        operand = WASM__POP(rt);
    }
    base = WASM__POP(rt);

    memory = wasm__memory_at(mod, memarg.memory_index);
    if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
    if (!wasm__memory_effective_addr(memory, &base, memarg.offset, &addr) ||
        !wasm__memory_access_in_bounds(addr, size, wasm__memory_byte_size(memory)))
        WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
    if ((addr & (size - 1u)) != 0) {
        WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "unaligned atomic");
        WASM__TRAP(WASM_ERR_TRAP);
    }
    p = memory->data + addr;

    if (subop == 0x00) {
        uint32_t count = (uint32_t)operand.of.i32;
        uint32_t woken = memory->is_shared ? wasm__platform_atomic_notify(p, count) : 0u;
        WASM__PUSH(rt, wasm_i32((int32_t)woken));
    } else if (subop == 0x01 || subop == 0x02) {
        int result;
        if (!memory->is_shared) {
            WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "expected shared memory");
            WASM__TRAP(WASM_ERR_TRAP);
        }
        result = wasm__platform_atomic_wait(p, size, wasm__atomic_operand(operand, type), replacement.of.i64,
                                            &rt->interrupt_requested);
        if (result == 3 && wasm__runtime_take_interrupt(rt)) WASM__TRAP(WASM_ERR_INTERRUPTED);
        if (result < 0) {
            WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "atomic wait would block without thread support");
            WASM__TRAP(WASM_ERR_TRAP);
        }
        WASM__PUSH(rt, wasm_i32(result));
    } else if (subop <= 0x16) {
        WASM__PUSH(rt, wasm__atomic_result(wasm__atomic_load(p, size), type));
    } else if (subop <= 0x1D) {
        wasm__atomic_store(p, size, wasm__atomic_operand(operand, type));
    } else if (subop <= 0x47) {
        WASM__PUSH(rt, wasm__atomic_result(wasm__atomic_rmw(p, size, subop, wasm__atomic_operand(operand, type)), type));
    } else {
        uint64_t mask = size == 8u ? UINT64_MAX : ((UINT64_C(1) << (size * 8u)) - 1u);
        uint64_t expected = wasm__atomic_operand(operand, type) & mask;
        WASM__PUSH(rt, wasm__atomic_result(wasm__atomic_compare_exchange(p, size, expected,
                                                                         wasm__atomic_operand(replacement, type)),
                                           type));
    }

cleanup:
    return err;
}

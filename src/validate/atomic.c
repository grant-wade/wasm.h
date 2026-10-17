#include "atomic.h"
#include "../instructions.h"
#include "../memory_internal.h"
#include "../module.h"
#include "../reader.h"
#include "stack.h"

WASM_PRIVATE wasm_error_t wasm__validator_validate_atomic(wasm__validator_t* v, const uint8_t* at) {
    uint32_t subop = wasm__read_leb128_u32(&v->r);
    uint32_t size_log2;
    wasm_valtype_t type;
    wasm_valtype_t index_type;
    wasm__memarg_t memarg;
    wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_THREADS);

    if (err != WASM_OK) return err;
    if (subop == 0x03) {
        if (wasm__read_u8(&v->r) != 0x00) return wasm__validator_error(v, at, "%s", "malformed atomic.fence");
        return WASM_OK;
    }
    if (!wasm__atomic_access(subop, &size_log2, &type))
        return wasm__validator_error(v, at, "unknown prefixed opcode 0xFE 0x%X", (unsigned)subop);

    err = wasm__validator_read_memarg(v, at, size_log2, &memarg);
    if (err != WASM_OK) return err;
    if (memarg.align_log2 != size_log2)
        return wasm__validator_error(v, at, "%s", "atomic alignment must be natural");
    index_type = wasm__memory_index_type(&v->mod->memories[memarg.memory_index]);

    if (subop == 0x00) {
        err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
        if (err != WASM_OK) return err;
        err = wasm__validator_pop_expect(v, at, index_type);
        if (err != WASM_OK) return err;
        return wasm__validator_push(v, at, WASM_TYPE_I32);
    }
    if (subop == 0x01 || subop == 0x02) {
        err = wasm__validator_pop_expect(v, at, WASM_TYPE_I64);
        if (err != WASM_OK) return err;
        err = wasm__validator_pop_expect(v, at, type);
        if (err != WASM_OK) return err;
        err = wasm__validator_pop_expect(v, at, index_type);
        if (err != WASM_OK) return err;
        return wasm__validator_push(v, at, WASM_TYPE_I32);
    }
    if (subop <= 0x16) {
        err = wasm__validator_pop_expect(v, at, index_type);
        if (err != WASM_OK) return err;
        return wasm__validator_push(v, at, type);
    }
    if (subop <= 0x1D) {
        err = wasm__validator_pop_expect(v, at, type);
        if (err != WASM_OK) return err;
        return wasm__validator_pop_expect(v, at, index_type);
    }
    if (subop >= 0x48) {
        err = wasm__validator_pop_expect(v, at, type);
        if (err != WASM_OK) return err;
    }
    err = wasm__validator_pop_expect(v, at, type);
    if (err != WASM_OK) return err;
    err = wasm__validator_pop_expect(v, at, index_type);
    if (err != WASM_OK) return err;
    return wasm__validator_push(v, at, type);
}

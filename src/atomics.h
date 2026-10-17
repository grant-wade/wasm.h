#ifndef WASM_ATOMICS_H
#define WASM_ATOMICS_H

#include "internal/config.h"

/* Sequentially consistent accesses to naturally aligned little-endian values of 1, 2, 4, or 8 bytes.
   Values are passed zero-extended in host order. Compilers without atomic builtins get plain accesses,
   which are only correct while a single thread uses the memory. */
WASM_PRIVATE uint64_t wasm__atomic_load(const uint8_t* p, uint32_t size);

WASM_PRIVATE void wasm__atomic_store(uint8_t* p, uint32_t size, uint64_t value);

/* Stores desired if the current value equals expected; returns the value observed before the call. */
WASM_PRIVATE uint64_t wasm__atomic_compare_exchange(uint8_t* p, uint32_t size, uint64_t expected, uint64_t desired);

WASM_PRIVATE void wasm__atomic_fence(void);

/* Host-order 64-bit counters shared between threads, such as a shared memory's page count. */
WASM_PRIVATE uint64_t wasm__atomic_load_u64(const uint64_t* p);

WASM_PRIVATE void wasm__atomic_store_u64(uint64_t* p, uint64_t value);

#endif /* WASM_ATOMICS_H */

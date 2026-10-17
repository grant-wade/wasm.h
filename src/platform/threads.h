#ifndef WASM_PLATFORM_THREADS_H
#define WASM_PLATFORM_THREADS_H

#include "../internal/config.h"

/* Blocks until a notify at p or until timeout_ns elapses (negative waits forever), comparing the value
   at p with expected first. Returns 0 when woken, 1 when the value differs, 2 on timeout, and -1 when
   the wait would block but thread support is unavailable. */
WASM_PRIVATE int wasm__platform_atomic_wait(uint8_t* p, uint32_t size, uint64_t expected, int64_t timeout_ns);

/* Wakes up to count waiters blocked at p and returns how many were woken. */
WASM_PRIVATE uint32_t wasm__platform_atomic_notify(uint8_t* p, uint32_t count);

/* Shared memory storage is reserved once so it never moves while other threads use it. The reservation
   may be smaller than requested; out_bytes receives its size, which is never below min_bytes. */
WASM_PRIVATE uint8_t* wasm__platform_shared_reserve(uint64_t min_bytes, uint64_t max_bytes, uint64_t* out_bytes);

/* Makes the first bytes of a reservation usable; committed bytes read as zero until written. */
WASM_PRIVATE int wasm__platform_shared_commit(uint8_t* base, uint64_t bytes);

WASM_PRIVATE void wasm__platform_shared_release(uint8_t* base, uint64_t reserved_bytes);

/* Serializes growth of shared memories across threads. */
WASM_PRIVATE void wasm__platform_shared_grow_lock(void);

WASM_PRIVATE void wasm__platform_shared_grow_unlock(void);

#endif /* WASM_PLATFORM_THREADS_H */

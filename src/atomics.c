#include "atomics.h"
#include "portable.h"
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define WASM__ATOMICS_GNU 1
#elif defined(_MSC_VER)
#include <intrin.h>
#define WASM__ATOMICS_MSVC 1
#endif

static uint64_t wasm__atomic_mask(uint32_t size) {
    return size == 8u ? UINT64_MAX : ((UINT64_C(1) << (size * 8u)) - 1u);
}

/* Converts between a little-endian memory value and the host-order bits of the same bytes. */
static uint64_t wasm__atomic_swap_order(uint64_t value, uint32_t size) {
#if WASM__BIG_ENDIAN
    uint64_t swapped = 0;
    uint32_t i;
    for (i = 0; i < size; i++) swapped |= ((value >> (i * 8u)) & 0xFFu) << ((size - 1u - i) * 8u);
    return swapped;
#else
    (void)size;
    return value;
#endif
}

static uint64_t wasm__atomic_cas_raw(uint8_t* p, uint32_t size, uint64_t expected, uint64_t desired) {
#if defined(WASM__ATOMICS_GNU)
    switch (size) {
        case 1: {
            uint8_t e = (uint8_t)expected;
            __atomic_compare_exchange_n((uint8_t*)p, &e, (uint8_t)desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
            return e;
        }
        case 2: {
            uint16_t e = (uint16_t)expected;
            __atomic_compare_exchange_n((uint16_t*)(void*)p, &e, (uint16_t)desired, 0, __ATOMIC_SEQ_CST,
                                        __ATOMIC_SEQ_CST);
            return e;
        }
        case 4: {
            uint32_t e = (uint32_t)expected;
            __atomic_compare_exchange_n((uint32_t*)(void*)p, &e, (uint32_t)desired, 0, __ATOMIC_SEQ_CST,
                                        __ATOMIC_SEQ_CST);
            return e;
        }
        default: {
            uint64_t e = expected;
            __atomic_compare_exchange_n((uint64_t*)(void*)p, &e, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
            return e;
        }
    }
#elif defined(WASM__ATOMICS_MSVC)
    switch (size) {
        case 1:
            return (uint8_t)_InterlockedCompareExchange8((char volatile*)p, (char)desired, (char)expected);
        case 2:
            return (uint16_t)_InterlockedCompareExchange16((short volatile*)(void*)p, (short)desired,
                                                           (short)expected);
        case 4:
            return (uint32_t)_InterlockedCompareExchange((long volatile*)(void*)p, (long)desired, (long)expected);
        default:
            return (uint64_t)_InterlockedCompareExchange64((__int64 volatile*)(void*)p, (__int64)desired,
                                                           (__int64)expected);
    }
#else
    uint64_t current = 0;
    memcpy(&current, p, size);
    if (current == expected) memcpy(p, &desired, size);
    return current;
#endif
}

WASM_PRIVATE uint64_t wasm__atomic_load(const uint8_t* p, uint32_t size) {
#if defined(WASM__ATOMICS_GNU)
    uint64_t raw;
    switch (size) {
        case 1:
            raw = __atomic_load_n(p, __ATOMIC_SEQ_CST);
            break;
        case 2:
            raw = __atomic_load_n((const uint16_t*)(const void*)p, __ATOMIC_SEQ_CST);
            break;
        case 4:
            raw = __atomic_load_n((const uint32_t*)(const void*)p, __ATOMIC_SEQ_CST);
            break;
        default:
            raw = __atomic_load_n((const uint64_t*)(const void*)p, __ATOMIC_SEQ_CST);
            break;
    }
    return wasm__atomic_swap_order(raw, size);
#else
    /* A compare-exchange of 0 with 0 reads atomically and leaves the value unchanged. */
    return wasm__atomic_swap_order(wasm__atomic_cas_raw((uint8_t*)p, size, 0, 0), size);
#endif
}

WASM_PRIVATE void wasm__atomic_store(uint8_t* p, uint32_t size, uint64_t value) {
    uint64_t raw = wasm__atomic_swap_order(value & wasm__atomic_mask(size), size);
#if defined(WASM__ATOMICS_GNU)
    switch (size) {
        case 1:
            __atomic_store_n(p, (uint8_t)raw, __ATOMIC_SEQ_CST);
            break;
        case 2:
            __atomic_store_n((uint16_t*)(void*)p, (uint16_t)raw, __ATOMIC_SEQ_CST);
            break;
        case 4:
            __atomic_store_n((uint32_t*)(void*)p, (uint32_t)raw, __ATOMIC_SEQ_CST);
            break;
        default:
            __atomic_store_n((uint64_t*)(void*)p, raw, __ATOMIC_SEQ_CST);
            break;
    }
#else
    uint64_t observed = wasm__atomic_cas_raw(p, size, 0, 0);
    for (;;) {
        uint64_t previous = wasm__atomic_cas_raw(p, size, observed, raw);
        if (previous == observed) break;
        observed = previous;
    }
#endif
}

WASM_PRIVATE uint64_t wasm__atomic_compare_exchange(uint8_t* p, uint32_t size, uint64_t expected, uint64_t desired) {
    uint64_t mask = wasm__atomic_mask(size);
    uint64_t observed = wasm__atomic_cas_raw(p, size, wasm__atomic_swap_order(expected & mask, size),
                                             wasm__atomic_swap_order(desired & mask, size));
    return wasm__atomic_swap_order(observed, size);
}

WASM_PRIVATE uint64_t wasm__atomic_load_u64(const uint64_t* p) {
#if defined(WASM__ATOMICS_GNU)
    return __atomic_load_n(p, __ATOMIC_SEQ_CST);
#else
    return wasm__atomic_cas_raw((uint8_t*)(uintptr_t)p, 8u, 0, 0);
#endif
}

WASM_PRIVATE void wasm__atomic_store_u64(uint64_t* p, uint64_t value) {
#if defined(WASM__ATOMICS_GNU)
    __atomic_store_n(p, value, __ATOMIC_SEQ_CST);
#else
    uint64_t observed = wasm__atomic_cas_raw((uint8_t*)p, 8u, 0, 0);
    for (;;) {
        uint64_t previous = wasm__atomic_cas_raw((uint8_t*)p, 8u, observed, value);
        if (previous == observed) break;
        observed = previous;
    }
#endif
}

WASM_PRIVATE void wasm__atomic_fence(void) {
#if defined(WASM__ATOMICS_GNU)
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#elif defined(WASM__ATOMICS_MSVC)
    /* Interlocked operations are full barriers. */
    long volatile barrier = 0;
    _InterlockedExchange(&barrier, 0);
#endif
}

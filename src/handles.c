#include "handles.h"
#include "internal/alloc.h"
#include <string.h>

#define WASM__HANDLE_SET_MIN_CAPACITY 16u

static uint32_t wasm__handle_hash(const void* handle) {
    uint64_t h = (uint64_t)(uintptr_t)handle;

    h ^= h >> 33;
    h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33;
    return (uint32_t)h;
}

static void wasm__handle_slots_put(void** slots, uint32_t capacity, void* handle) {
    uint32_t mask = capacity - 1u;
    uint32_t i = wasm__handle_hash(handle) & mask;

    while (slots[i]) i = (i + 1u) & mask;
    slots[i] = handle;
}

WASM_PRIVATE int wasm__handle_set_contains(const wasm__handle_set_t* set, const void* handle) {
    uint32_t mask;
    uint32_t i;

    if (!set || !handle || set->capacity == 0) return 0;
    mask = set->capacity - 1u;
    i = wasm__handle_hash(handle) & mask;
    while (set->slots[i]) {
        if (set->slots[i] == handle) return 1;
        i = (i + 1u) & mask;
    }
    return 0;
}

static wasm_error_t wasm__handle_set_resize(wasm__handle_set_t* set, uint32_t capacity) {
    void** slots = (void**)WASM_CALLOC(capacity, sizeof(void*));
    uint32_t i;

    if (!slots) return WASM_ERR_OOM;
    for (i = 0; i < set->capacity; i++) {
        if (set->slots[i]) wasm__handle_slots_put(slots, capacity, set->slots[i]);
    }
    WASM_FREE(set->slots);
    set->slots = slots;
    set->capacity = capacity;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__handle_set_insert(wasm__handle_set_t* set, void* handle) {
    if (!set || !handle) return WASM_ERR_MALFORMED;
    /* Keep the load factor at or below one half. */
    if ((set->count + 1u) * 2u > set->capacity) {
        uint32_t capacity = set->capacity ? set->capacity * 2u : WASM__HANDLE_SET_MIN_CAPACITY;
        wasm_error_t err;

        if (set->capacity > UINT32_MAX / 4u) return WASM_ERR_OOM;
        err = wasm__handle_set_resize(set, capacity);
        if (err != WASM_OK) return err;
    }
    wasm__handle_slots_put(set->slots, set->capacity, handle);
    set->count++;
    return WASM_OK;
}

WASM_PRIVATE void wasm__handle_set_filter(wasm__handle_set_t* set, int (*keep)(void*),
                                          void (*drop)(void*)) {
    uint32_t survivors = 0;
    uint32_t capacity = WASM__HANDLE_SET_MIN_CAPACITY;
    void** slots;
    uint32_t i;

    if (!set || set->count == 0) return;
    for (i = 0; i < set->capacity; i++) {
        if (set->slots[i] && keep(set->slots[i])) survivors++;
    }
    if (survivors == set->count) return;
    while (capacity < survivors * 2u) capacity *= 2u;

    slots = (void**)WASM_CALLOC(capacity, sizeof(void*));
    if (!slots) return;
    for (i = 0; i < set->capacity; i++) {
        void* handle = set->slots[i];

        if (!handle) continue;
        if (keep(handle))
            wasm__handle_slots_put(slots, capacity, handle);
        else
            drop(handle);
    }
    WASM_FREE(set->slots);
    set->slots = slots;
    set->capacity = capacity;
    set->count = survivors;
}

WASM_PRIVATE void wasm__handle_set_free(wasm__handle_set_t* set) {
    if (!set) return;
    WASM_FREE(set->slots);
    memset(set, 0, sizeof(*set));
}

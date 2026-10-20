#ifndef WASM_INTERNAL_SLOT_H
#define WASM_INTERNAL_SLOT_H

#include "../wasm.h"
#include <string.h>

/* An 8-byte operand stack or local slot; a v128 spans two. */
typedef union wasm__slot_t {
    uint64_t u64; /* numbers zero-extended; references as bits, null 0, a funcref its id + 1 */
    double f64;
} wasm__slot_t;

/* Slots a v128 value occupies. */
#define WASM__V128_SLOTS 2u

WASM_INLINE uint32_t wasm__valtype_slots(wasm_valtype_t type) {
    return type == WASM_TYPE_V128 ? WASM__V128_SLOTS : 1u;
}

WASM_INLINE uint32_t wasm__valtypes_slots(const wasm_valtype_t* types, uint32_t count) {
    uint32_t slots = 0;
    uint32_t i;

    for (i = 0; i < count; i++) slots += wasm__valtype_slots(types[i]);
    return slots;
}

/* What a slot holds for the collector, by the reference hierarchy of its static type. */
#define WASM__SLOT_KIND_NONE 0u
#define WASM__SLOT_KIND_ANY 1u
#define WASM__SLOT_KIND_EXTERN 2u
#define WASM__SLOT_KIND_HANDLE 3u /* an exnref or contref: an object outside the GC heap */
#define WASM__SLOT_KIND_BITS 2u

WASM_INLINE uint32_t wasm__slot_kind(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_NONE:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            return WASM__SLOT_KIND_ANY;
        case WASM_TYPE_EXTERNREF:
        case WASM_TYPE_NOEXTERN:
            return WASM__SLOT_KIND_EXTERN;
        case WASM_TYPE_EXNREF:
        case WASM_TYPE_NOEXN:
        case WASM_TYPE_CONTREF:
        case WASM_TYPE_NOCONT:
            return WASM__SLOT_KIND_HANDLE;
        default:
            return WASM__SLOT_KIND_NONE;
    }
}

/* Returns the kind of slot index in a stack map of WASM__SLOT_KIND_BITS-bit kinds. */
WASM_INLINE uint32_t wasm__stack_map_kind(const uint32_t* bits, uint32_t index) {
    return (bits[index / 16u] >> ((index % 16u) * WASM__SLOT_KIND_BITS)) & 3u;
}

WASM_INLINE int32_t wasm__slot_i32(const wasm__slot_t* s) {
    return (int32_t)(uint32_t)s->u64;
}

WASM_INLINE uint32_t wasm__slot_u32(const wasm__slot_t* s) {
    return (uint32_t)s->u64;
}

WASM_INLINE int64_t wasm__slot_i64(const wasm__slot_t* s) {
    return (int64_t)s->u64;
}

WASM_INLINE uint64_t wasm__slot_u64(const wasm__slot_t* s) {
    return s->u64;
}

WASM_INLINE float wasm__slot_f32(const wasm__slot_t* s) {
    uint32_t bits = (uint32_t)s->u64;
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

WASM_INLINE double wasm__slot_f64(const wasm__slot_t* s) {
    return s->f64;
}

WASM_INLINE uint32_t wasm__slot_f32_bits(const wasm__slot_t* s) {
    return (uint32_t)s->u64;
}

WASM_INLINE uint64_t wasm__slot_f64_bits(const wasm__slot_t* s) {
    return s->u64;
}

/* Reads a memory or table index of an i64 slot if is_64, else of an i32 slot. */
WASM_INLINE uint64_t wasm__slot_index(const wasm__slot_t* s, int is_64) {
    return is_64 ? s->u64 : (uint64_t)(uint32_t)s->u64;
}

/* s spans WASM__V128_SLOTS slots. */
WASM_INLINE void wasm__slot_v128(const wasm__slot_t* s, uint8_t out[16]) {
    memcpy(out, s, 16);
}

/* Returns the function reference id in s, UINT32_MAX for null. */
WASM_INLINE uint32_t wasm__slot_funcref(const wasm__slot_t* s) {
    return (uint32_t)s->u64 - 1u;
}

WASM_INLINE uintptr_t wasm__slot_ref(const wasm__slot_t* s) {
    return (uintptr_t)s->u64;
}

WASM_INLINE void wasm__slot_set_i32(wasm__slot_t* s, int32_t value) {
    s->u64 = (uint32_t)value;
}

WASM_INLINE void wasm__slot_set_u32(wasm__slot_t* s, uint32_t value) {
    s->u64 = value;
}

WASM_INLINE void wasm__slot_set_i64(wasm__slot_t* s, int64_t value) {
    s->u64 = (uint64_t)value;
}

WASM_INLINE void wasm__slot_set_u64(wasm__slot_t* s, uint64_t value) {
    s->u64 = value;
}

WASM_INLINE void wasm__slot_set_f32(wasm__slot_t* s, float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    s->u64 = bits;
}

WASM_INLINE void wasm__slot_set_f64(wasm__slot_t* s, double value) {
    s->f64 = value;
}

WASM_INLINE void wasm__slot_set_f32_bits(wasm__slot_t* s, uint32_t bits) {
    s->u64 = bits;
}

WASM_INLINE void wasm__slot_set_f64_bits(wasm__slot_t* s, uint64_t bits) {
    s->u64 = bits;
}

/* Sets an i64 slot if is_64, else an i32 slot. */
WASM_INLINE void wasm__slot_set_index(wasm__slot_t* s, uint64_t value, int is_64) {
    s->u64 = is_64 ? value : (uint64_t)(uint32_t)value;
}

/* s spans WASM__V128_SLOTS slots. */
WASM_INLINE void wasm__slot_set_v128(wasm__slot_t* s, const uint8_t bytes[16]) {
    memcpy(s, bytes, 16);
}

/* id is a function reference id, UINT32_MAX for null. */
WASM_INLINE void wasm__slot_set_funcref(wasm__slot_t* s, uint32_t id) {
    s->u64 = (uint32_t)(id + 1u);
}

WASM_INLINE void wasm__slot_set_ref(wasm__slot_t* s, uintptr_t bits) {
    s->u64 = (uint64_t)bits;
}

#endif /* WASM_INTERNAL_SLOT_H */

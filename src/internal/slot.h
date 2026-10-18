#ifndef WASM_INTERNAL_SLOT_H
#define WASM_INTERNAL_SLOT_H

#include "../wasm.h"
#include <string.h>

/* One operand stack or local slot. */
typedef wasm_value_t wasm__slot_t;

/* Slots a v128 value occupies. */
#define WASM__V128_SLOTS 1u

WASM_INLINE int32_t wasm__slot_i32(const wasm__slot_t* s) {
    return s->of.i32;
}

WASM_INLINE uint32_t wasm__slot_u32(const wasm__slot_t* s) {
    return (uint32_t)s->of.i32;
}

WASM_INLINE int64_t wasm__slot_i64(const wasm__slot_t* s) {
    return s->of.i64;
}

WASM_INLINE uint64_t wasm__slot_u64(const wasm__slot_t* s) {
    return (uint64_t)s->of.i64;
}

WASM_INLINE float wasm__slot_f32(const wasm__slot_t* s) {
    return s->of.f32;
}

WASM_INLINE double wasm__slot_f64(const wasm__slot_t* s) {
    return s->of.f64;
}

WASM_INLINE uint32_t wasm__slot_f32_bits(const wasm__slot_t* s) {
    uint32_t bits;

    memcpy(&bits, &s->of.f32, sizeof(bits));
    return bits;
}

WASM_INLINE uint64_t wasm__slot_f64_bits(const wasm__slot_t* s) {
    uint64_t bits;

    memcpy(&bits, &s->of.f64, sizeof(bits));
    return bits;
}

/* Reads a memory or table index of an i64 slot if is_64, else of an i32 slot. */
WASM_INLINE uint64_t wasm__slot_index(const wasm__slot_t* s, int is_64) {
    return is_64 ? wasm__slot_u64(s) : (uint64_t)wasm__slot_u32(s);
}

/* s spans WASM__V128_SLOTS slots. */
WASM_INLINE void wasm__slot_v128(const wasm__slot_t* s, uint8_t out[16]) {
    memcpy(out, s->of.v128, 16);
}

WASM_INLINE void wasm__slot_set_i32(wasm__slot_t* s, int32_t value) {
    s->type = WASM_TYPE_I32;
    s->of.i32 = value;
}

WASM_INLINE void wasm__slot_set_u32(wasm__slot_t* s, uint32_t value) {
    s->type = WASM_TYPE_I32;
    s->of.i32 = (int32_t)value;
}

WASM_INLINE void wasm__slot_set_i64(wasm__slot_t* s, int64_t value) {
    s->type = WASM_TYPE_I64;
    s->of.i64 = value;
}

WASM_INLINE void wasm__slot_set_u64(wasm__slot_t* s, uint64_t value) {
    s->type = WASM_TYPE_I64;
    s->of.i64 = (int64_t)value;
}

WASM_INLINE void wasm__slot_set_f32(wasm__slot_t* s, float value) {
    s->type = WASM_TYPE_F32;
    s->of.f32 = value;
}

WASM_INLINE void wasm__slot_set_f64(wasm__slot_t* s, double value) {
    s->type = WASM_TYPE_F64;
    s->of.f64 = value;
}

WASM_INLINE void wasm__slot_set_f32_bits(wasm__slot_t* s, uint32_t bits) {
    s->type = WASM_TYPE_F32;
    memcpy(&s->of.f32, &bits, sizeof(bits));
}

WASM_INLINE void wasm__slot_set_f64_bits(wasm__slot_t* s, uint64_t bits) {
    s->type = WASM_TYPE_F64;
    memcpy(&s->of.f64, &bits, sizeof(bits));
}

/* Sets an i64 slot if is_64, else an i32 slot. */
WASM_INLINE void wasm__slot_set_index(wasm__slot_t* s, uint64_t value, int is_64) {
    if (is_64)
        wasm__slot_set_u64(s, value);
    else
        wasm__slot_set_u32(s, (uint32_t)value);
}

/* s spans WASM__V128_SLOTS slots. */
WASM_INLINE void wasm__slot_set_v128(wasm__slot_t* s, const uint8_t bytes[16]) {
    s->type = WASM_TYPE_V128;
    memcpy(s->of.v128, bytes, 16);
}

#endif /* WASM_INTERNAL_SLOT_H */

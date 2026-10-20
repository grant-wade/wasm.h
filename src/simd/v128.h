#ifndef WASM_SIMD_V128_H
#define WASM_SIMD_V128_H

#include "../internal/config.h"
#include "../portable.h"
#include <string.h>

/* A v128 value: lane i of n-byte lanes is bytes i * n to i * n + n - 1, little-endian. */
typedef struct wasm__v128_t {
    uint8_t bytes[16];
} wasm__v128_t;

WASM_INLINE wasm__v128_t wasm__v128_zero(void) {
    wasm__v128_t value;

    memset(&value, 0, sizeof(value));
    return value;
}

WASM_INLINE uint16_t wasm__v128_bits16(const uint8_t* p) {
    uint16_t bits;

    memcpy(&bits, p, sizeof(bits));
#if WASM__BIG_ENDIAN
    bits = (uint16_t)((bits >> 8) | (bits << 8));
#endif
    return bits;
}

WASM_INLINE uint32_t wasm__v128_bits32(const uint8_t* p) {
    uint32_t bits;

    memcpy(&bits, p, sizeof(bits));
#if WASM__BIG_ENDIAN
    bits = (bits >> 24) | ((bits >> 8) & 0xFF00u) | ((bits & 0xFF00u) << 8) | (bits << 24);
#endif
    return bits;
}

WASM_INLINE uint64_t wasm__v128_bits64(const uint8_t* p) {
#if WASM__BIG_ENDIAN
    return (uint64_t)wasm__v128_bits32(p) | ((uint64_t)wasm__v128_bits32(p + 4) << 32);
#else
    uint64_t bits;

    memcpy(&bits, p, sizeof(bits));
    return bits;
#endif
}

WASM_INLINE void wasm__v128_put16(uint8_t* p, uint16_t bits) {
#if WASM__BIG_ENDIAN
    bits = (uint16_t)((bits >> 8) | (bits << 8));
#endif
    memcpy(p, &bits, sizeof(bits));
}

WASM_INLINE void wasm__v128_put32(uint8_t* p, uint32_t bits) {
#if WASM__BIG_ENDIAN
    bits = (bits >> 24) | ((bits >> 8) & 0xFF00u) | ((bits & 0xFF00u) << 8) | (bits << 24);
#endif
    memcpy(p, &bits, sizeof(bits));
}

WASM_INLINE void wasm__v128_put64(uint8_t* p, uint64_t bits) {
#if WASM__BIG_ENDIAN
    wasm__v128_put32(p, (uint32_t)bits);
    wasm__v128_put32(p + 4, (uint32_t)(bits >> 32));
#else
    memcpy(p, &bits, sizeof(bits));
#endif
}

WASM_INLINE int8_t wasm__v128_get_i8(const wasm__v128_t* value, uint32_t lane) {
    return (int8_t)value->bytes[lane];
}

WASM_INLINE uint8_t wasm__v128_get_u8(const wasm__v128_t* value, uint32_t lane) {
    return value->bytes[lane];
}

WASM_INLINE int16_t wasm__v128_get_i16(const wasm__v128_t* value, uint32_t lane) {
    return (int16_t)wasm__v128_bits16(value->bytes + lane * 2u);
}

WASM_INLINE uint16_t wasm__v128_get_u16(const wasm__v128_t* value, uint32_t lane) {
    return wasm__v128_bits16(value->bytes + lane * 2u);
}

WASM_INLINE int32_t wasm__v128_get_i32(const wasm__v128_t* value, uint32_t lane) {
    return (int32_t)wasm__v128_bits32(value->bytes + lane * 4u);
}

WASM_INLINE uint32_t wasm__v128_get_u32(const wasm__v128_t* value, uint32_t lane) {
    return wasm__v128_bits32(value->bytes + lane * 4u);
}

WASM_INLINE int64_t wasm__v128_get_i64(const wasm__v128_t* value, uint32_t lane) {
    return (int64_t)wasm__v128_bits64(value->bytes + lane * 8u);
}

WASM_INLINE uint64_t wasm__v128_get_u64(const wasm__v128_t* value, uint32_t lane) {
    return wasm__v128_bits64(value->bytes + lane * 8u);
}

WASM_INLINE float wasm__v128_get_f32(const wasm__v128_t* value, uint32_t lane) {
    uint32_t bits = wasm__v128_get_u32(value, lane);
    float result;

    memcpy(&result, &bits, sizeof(result));
    return result;
}

WASM_INLINE double wasm__v128_get_f64(const wasm__v128_t* value, uint32_t lane) {
    uint64_t bits = wasm__v128_get_u64(value, lane);
    double result;

    memcpy(&result, &bits, sizeof(result));
    return result;
}

WASM_INLINE void wasm__v128_set_u8(wasm__v128_t* value, uint32_t lane, uint32_t lane_value) {
    value->bytes[lane] = (uint8_t)lane_value;
}

WASM_INLINE void wasm__v128_set_i16(wasm__v128_t* value, uint32_t lane, int32_t lane_value) {
    wasm__v128_put16(value->bytes + lane * 2u, (uint16_t)lane_value);
}

WASM_INLINE void wasm__v128_set_u16(wasm__v128_t* value, uint32_t lane, uint32_t lane_value) {
    wasm__v128_put16(value->bytes + lane * 2u, (uint16_t)lane_value);
}

WASM_INLINE void wasm__v128_set_i32(wasm__v128_t* value, uint32_t lane, int32_t lane_value) {
    wasm__v128_put32(value->bytes + lane * 4u, (uint32_t)lane_value);
}

WASM_INLINE void wasm__v128_set_u32(wasm__v128_t* value, uint32_t lane, uint32_t lane_value) {
    wasm__v128_put32(value->bytes + lane * 4u, lane_value);
}

WASM_INLINE void wasm__v128_set_i64(wasm__v128_t* value, uint32_t lane, int64_t lane_value) {
    wasm__v128_put64(value->bytes + lane * 8u, (uint64_t)lane_value);
}

WASM_INLINE void wasm__v128_set_u64(wasm__v128_t* value, uint32_t lane, uint64_t lane_value) {
    wasm__v128_put64(value->bytes + lane * 8u, lane_value);
}

WASM_INLINE void wasm__v128_set_f32(wasm__v128_t* value, uint32_t lane, float lane_value) {
    uint32_t bits;

    memcpy(&bits, &lane_value, sizeof(bits));
    wasm__v128_set_u32(value, lane, bits);
}

WASM_INLINE void wasm__v128_set_f64(wasm__v128_t* value, uint32_t lane, double lane_value) {
    uint64_t bits;

    memcpy(&bits, &lane_value, sizeof(bits));
    wasm__v128_set_u64(value, lane, bits);
}

#endif /* WASM_SIMD_V128_H */

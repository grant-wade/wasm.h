#include "simd.h"
#include "../simd/compare.h"
#include "../simd/float.h"
#include "../simd/integer_small.h"
#include "../simd/integer_wide.h"
#include "../simd/lanes.h"
#include "../simd/v128.h"
#include <string.h>

WASM_INLINE const wasm__v128_t* wasm__simd_operand(const wasm__slot_t* slots) {
    return (const wasm__v128_t*)(const void*)slots;
}

WASM_PRIVATE WASM__NOINLINE void wasm__exec_simd_unary(uint32_t subop, wasm__slot_t* operand) {
    const wasm__v128_t* value = wasm__simd_operand(operand);
    wasm__v128_t result;

    switch (subop) {
        case 0x4D:
            result = wasm__simd_bitwise_not(value);
            break;
        case 0x5E:
            result = wasm__simd_demote_f64x2_zero(value);
            break;
        case 0x5F:
            result = wasm__simd_promote_low_f32x4(value);
            break;
        case 0x60:
        case 0x61:
        case 0x62:
            result = wasm__simd_i8x16_unary(subop, value);
            break;
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0xE0:
        case 0xE1:
        case 0xE3:
            result = wasm__simd_f32x4_unary(subop, value);
            break;
        case 0x74:
        case 0x75:
        case 0x7A:
        case 0x94:
        case 0xEC:
        case 0xED:
        case 0xEF:
            result = wasm__simd_f64x2_unary(subop, value);
            break;
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
            result = wasm__simd_extadd_pairwise(subop, value);
            break;
        case 0x80:
        case 0x81:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
            result = wasm__simd_i16x8_unary(subop, value);
            break;
        case 0xA0:
        case 0xA1:
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA:
            result = wasm__simd_i32x4_unary(subop, value);
            break;
        case 0xC0:
        case 0xC1:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA:
            result = wasm__simd_i64x2_unary(subop, value);
            break;
        case 0xF8:
        case 0xF9:
            result = wasm__simd_trunc_sat_f32x4(subop, value);
            break;
        case 0x101:
        case 0x102:
            result = wasm__simd_trunc_sat_f32x4(subop - 0x101u + 0xF8u, value);
            break;
        case 0xFA:
        case 0xFB:
            result = wasm__simd_convert_i32x4(subop, value);
            break;
        case 0xFC:
        case 0xFD:
            result = wasm__simd_trunc_sat_f64x2_zero(subop, value);
            break;
        case 0x103:
        case 0x104:
            result = wasm__simd_trunc_sat_f64x2_zero(subop - 0x103u + 0xFCu, value);
            break;
        default:
            result = wasm__simd_convert_low_i32x4_to_f64x2(subop, value);
            break;
    }
    memcpy(operand, &result, sizeof(result));
}

WASM_PRIVATE WASM__NOINLINE void wasm__exec_simd_binary(uint32_t subop, wasm__slot_t* lhs, const void* rhs) {
    const wasm__v128_t* a = wasm__simd_operand(lhs);
    const wasm__v128_t* b = (const wasm__v128_t*)rhs;
    wasm__v128_t result;

    switch (subop) {
        case 0x0E:
        case 0x100:
            result = wasm__simd_swizzle(a, b);
            break;
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C:
            result = wasm__simd_cmp_i8x16(subop, a, b);
            break;
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
            result = wasm__simd_cmp_i16x8(subop, a, b);
            break;
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
        case 0x40:
            result = wasm__simd_cmp_i32x4(subop, a, b);
            break;
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
            result = wasm__simd_cmp_f32x4(subop, a, b);
            break;
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
            result = wasm__simd_cmp_f64x2(subop, a, b);
            break;
        case 0x4E:
        case 0x4F:
        case 0x50:
        case 0x51:
            result = wasm__simd_bitwise_binary(subop, a, b);
            break;
        case 0x65:
        case 0x66:
        case 0x6E:
        case 0x6F:
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x76:
        case 0x77:
        case 0x78:
        case 0x79:
        case 0x7B:
            result = wasm__simd_i8x16_binary(subop, a, b);
            break;
        case 0x82:
        case 0x85:
        case 0x86:
        case 0x8E:
        case 0x8F:
        case 0x90:
        case 0x91:
        case 0x92:
        case 0x93:
        case 0x95:
        case 0x96:
        case 0x97:
        case 0x98:
        case 0x99:
        case 0x9B:
        case 0x9C:
        case 0x9D:
        case 0x9E:
        case 0x9F:
            result = wasm__simd_i16x8_binary(subop, a, b);
            break;
        case 0x111:
            result = wasm__simd_i16x8_binary(0x82, a, b);
            break;
        case 0xAE:
        case 0xB1:
        case 0xB5:
        case 0xB6:
        case 0xB7:
        case 0xB8:
        case 0xB9:
        case 0xBA:
        case 0xBC:
        case 0xBD:
        case 0xBE:
        case 0xBF:
            result = wasm__simd_i32x4_binary(subop, a, b);
            break;
        case 0xCE:
        case 0xD1:
        case 0xD5:
        case 0xDC:
        case 0xDD:
        case 0xDE:
        case 0xDF:
            result = wasm__simd_i64x2_binary(subop, a, b);
            break;
        case 0xD6:
        case 0xD7:
        case 0xD8:
        case 0xD9:
        case 0xDA:
        case 0xDB:
            result = wasm__simd_cmp_i64x2(subop, a, b);
            break;
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
            result = wasm__simd_f32x4_binary(subop, a, b);
            break;
        case 0x10D:
        case 0x10E:
            result = wasm__simd_f32x4_binary(subop - 0x10Du + 0xE8u, a, b);
            break;
        case 0x10F:
        case 0x110:
            result = wasm__simd_f64x2_binary(subop - 0x10Fu + 0xF4u, a, b);
            break;
        case 0x112:
            result = wasm__simd_relaxed_dot_i8x16_i7x16_s(a, b);
            break;
        default:
            result = wasm__simd_f64x2_binary(subop, a, b);
            break;
    }
    memcpy(lhs, &result, sizeof(result));
}

WASM_PRIVATE WASM__NOINLINE void wasm__exec_simd_ternary(uint32_t subop, wasm__slot_t* operands) {
    const wasm__v128_t* a = wasm__simd_operand(operands);
    const wasm__v128_t* b = wasm__simd_operand(operands + WASM__V128_SLOTS);
    const wasm__v128_t* c = wasm__simd_operand(operands + 2u * WASM__V128_SLOTS);
    wasm__v128_t result;

    switch (subop) {
        case 0x105:
        case 0x106:
        case 0x107:
        case 0x108:
            result = wasm__simd_relaxed_madd(subop, a, b, c);
            break;
        case 0x113:
            result = wasm__simd_relaxed_dot_i8x16_i7x16_add_s(a, b, c);
            break;
        default:
            result = wasm__simd_bitselect(a, b, c);
            break;
    }
    memcpy(operands, &result, sizeof(result));
}

WASM_PRIVATE WASM__NOINLINE void wasm__exec_simd_shift(uint32_t subop, wasm__slot_t* operand, uint32_t amount) {
    const wasm__v128_t* value = wasm__simd_operand(operand);
    wasm__v128_t result;

    if (subop <= 0x6D)
        result = wasm__simd_i8x16_shift(subop, value, amount);
    else if (subop <= 0x8D)
        result = wasm__simd_i16x8_shift(subop, value, amount);
    else if (subop <= 0xAD)
        result = wasm__simd_i32x4_shift(subop, value, amount);
    else
        result = wasm__simd_i64x2_shift(subop, value, amount);
    memcpy(operand, &result, sizeof(result));
}

WASM_PRIVATE WASM__NOINLINE uint32_t wasm__exec_simd_test(uint32_t subop, const wasm__slot_t* operand) {
    const wasm__v128_t* value = wasm__simd_operand(operand);

    switch (subop) {
        case 0x53:
            return (uint32_t)wasm__simd_any_true(value);
        case 0x63:
            return (uint32_t)wasm__simd_all_true_i8x16(value);
        case 0x64:
            return (uint32_t)wasm__simd_bitmask_i8x16(value);
        case 0x83:
            return (uint32_t)wasm__simd_all_true_i16x8(value);
        case 0x84:
            return (uint32_t)wasm__simd_bitmask_i16x8(value);
        case 0xA3:
            return (uint32_t)wasm__simd_all_true_i32x4(value);
        case 0xA4:
            return (uint32_t)wasm__simd_bitmask_i32x4(value);
        case 0xC3:
            return (uint32_t)wasm__simd_all_true_i64x2(value);
        default:
            return (uint32_t)wasm__simd_bitmask_i64x2(value);
    }
}

WASM_PRIVATE WASM__NOINLINE void wasm__exec_simd_splat(uint32_t subop, wasm__slot_t* slot) {
    uint64_t scalar = wasm__slot_u64(slot);
    wasm__v128_t result;
    uint32_t lane;

    switch (subop) {
        case 0x0F:
            memset(result.bytes, (int)(uint8_t)scalar, sizeof(result.bytes));
            break;
        case 0x10:
            for (lane = 0; lane < 8; lane++) wasm__v128_set_u16(&result, lane, (uint32_t)scalar);
            break;
        case 0x11:
        case 0x13:
            for (lane = 0; lane < 4; lane++) wasm__v128_set_u32(&result, lane, (uint32_t)scalar);
            break;
        default:
            for (lane = 0; lane < 2; lane++) wasm__v128_set_u64(&result, lane, scalar);
            break;
    }
    memcpy(slot, &result, sizeof(result));
}

WASM_PRIVATE WASM__NOINLINE void wasm__exec_simd_extract_lane(uint32_t subop, uint32_t lane, wasm__slot_t* operand) {
    const wasm__v128_t* value = wasm__simd_operand(operand);
    uint64_t bits;

    switch (subop) {
        case 0x15:
            bits = (uint32_t)(int32_t)wasm__v128_get_i8(value, lane);
            break;
        case 0x16:
            bits = wasm__v128_get_u8(value, lane);
            break;
        case 0x18:
            bits = (uint32_t)(int32_t)wasm__v128_get_i16(value, lane);
            break;
        case 0x19:
            bits = wasm__v128_get_u16(value, lane);
            break;
        case 0x1B:
        case 0x1F:
            bits = wasm__v128_get_u32(value, lane);
            break;
        default:
            bits = wasm__v128_get_u64(value, lane);
            break;
    }
    wasm__slot_set_u64(operand, bits);
}

WASM_PRIVATE WASM__NOINLINE void wasm__exec_simd_replace_lane(uint32_t subop, uint32_t lane, wasm__slot_t* vector,
                                                              const wasm__slot_t* scalar) {
    wasm__v128_t* value = (wasm__v128_t*)(void*)vector;
    uint64_t bits = wasm__slot_u64(scalar);

    switch (subop) {
        case 0x17:
            wasm__v128_set_u8(value, lane, (uint32_t)bits);
            break;
        case 0x1A:
            wasm__v128_set_u16(value, lane, (uint32_t)bits);
            break;
        case 0x1C:
        case 0x20:
            wasm__v128_set_u32(value, lane, (uint32_t)bits);
            break;
        default:
            wasm__v128_set_u64(value, lane, bits);
            break;
    }
}

WASM_PRIVATE WASM__NOINLINE void wasm__exec_simd_shuffle(wasm__slot_t* lhs, const wasm__slot_t* rhs, const void* mask) {
    wasm__v128_t result = wasm__simd_shuffle(wasm__simd_operand(lhs), wasm__simd_operand(rhs), (const uint8_t*)mask);

    memcpy(lhs, &result, sizeof(result));
}

WASM_PRIVATE WASM__NOINLINE wasm__slot_t* wasm__exec_simd_memory(uint32_t subop, uint32_t lane, wasm__slot_t* sp,
                                                                 uint8_t* p) {
    wasm__v128_t result = wasm__v128_zero();
    uint32_t i;

    switch (subop) {
        case 0x00:
            memcpy(result.bytes, p, sizeof(result.bytes));
            break;
        case 0x01:
            for (i = 0; i < 8; i++) wasm__v128_set_i16(&result, i, (int8_t)p[i]);
            break;
        case 0x02:
            for (i = 0; i < 8; i++) wasm__v128_set_u16(&result, i, p[i]);
            break;
        case 0x03:
            for (i = 0; i < 4; i++) wasm__v128_set_i32(&result, i, (int16_t)wasm__v128_bits16(p + i * 2u));
            break;
        case 0x04:
            for (i = 0; i < 4; i++) wasm__v128_set_u32(&result, i, wasm__v128_bits16(p + i * 2u));
            break;
        case 0x05:
            for (i = 0; i < 2; i++) wasm__v128_set_i64(&result, i, (int32_t)wasm__v128_bits32(p + i * 4u));
            break;
        case 0x06:
            for (i = 0; i < 2; i++) wasm__v128_set_u64(&result, i, wasm__v128_bits32(p + i * 4u));
            break;
        case 0x07:
            memset(result.bytes, *p, sizeof(result.bytes));
            break;
        case 0x08:
            for (i = 0; i < 8; i++) wasm__v128_set_u16(&result, i, wasm__v128_bits16(p));
            break;
        case 0x09:
            for (i = 0; i < 4; i++) wasm__v128_set_u32(&result, i, wasm__v128_bits32(p));
            break;
        case 0x0A:
            for (i = 0; i < 2; i++) wasm__v128_set_u64(&result, i, wasm__v128_bits64(p));
            break;
        case 0x5C:
            wasm__v128_set_u32(&result, 0, wasm__v128_bits32(p));
            break;
        case 0x5D:
            wasm__v128_set_u64(&result, 0, wasm__v128_bits64(p));
            break;
        case 0x0B:
            memcpy(p, sp - WASM__V128_SLOTS, sizeof(result.bytes));
            return sp - WASM__V128_SLOTS - 1u;
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57:
            memcpy(&result, sp - WASM__V128_SLOTS, sizeof(result));
            if (subop == 0x54)
                wasm__v128_set_u8(&result, lane, *p);
            else if (subop == 0x55)
                wasm__v128_set_u16(&result, lane, wasm__v128_bits16(p));
            else if (subop == 0x56)
                wasm__v128_set_u32(&result, lane, wasm__v128_bits32(p));
            else
                wasm__v128_set_u64(&result, lane, wasm__v128_bits64(p));
            memcpy(sp - WASM__V128_SLOTS - 1u, &result, sizeof(result));
            return sp - 1;
        default:
            memcpy(&result, sp - WASM__V128_SLOTS, sizeof(result));
            if (subop == 0x58)
                *p = wasm__v128_get_u8(&result, lane);
            else if (subop == 0x59)
                wasm__v128_put16(p, wasm__v128_get_u16(&result, lane));
            else if (subop == 0x5A)
                wasm__v128_put32(p, wasm__v128_get_u32(&result, lane));
            else
                wasm__v128_put64(p, wasm__v128_get_u64(&result, lane));
            return sp - WASM__V128_SLOTS - 1u;
    }
    memcpy(sp - 1, &result, sizeof(result));
    return sp + 1;
}

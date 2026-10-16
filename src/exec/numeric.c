#include "../internal.h"

WASM_PRIVATE wasm_error_t wasm__exec_numeric(wasm_runtime_t* rt, uint8_t op) {
    wasm_error_t err = WASM_OK;

    switch (op) {
        /* i32 comparison */
        case 0x45: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 == 0 ? 1 : 0));
            break;
        }
        case 0x46: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 == b.of.i32 ? 1 : 0));
            break;
        }
        case 0x47: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 != b.of.i32 ? 1 : 0));
            break;
        }
        case 0x48: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 < b.of.i32 ? 1 : 0));
            break;
        }
        case 0x49: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((uint32_t)a.of.i32 < (uint32_t)b.of.i32 ? 1 : 0));
            break;
        }
        case 0x4A: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 > b.of.i32 ? 1 : 0));
            break;
        }
        case 0x4B: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((uint32_t)a.of.i32 > (uint32_t)b.of.i32 ? 1 : 0));
            break;
        }
        case 0x4C: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 <= b.of.i32 ? 1 : 0));
            break;
        }
        case 0x4D: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((uint32_t)a.of.i32 <= (uint32_t)b.of.i32 ? 1 : 0));
            break;
        }
        case 0x4E: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 >= b.of.i32 ? 1 : 0));
            break;
        }
        case 0x4F: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((uint32_t)a.of.i32 >= (uint32_t)b.of.i32 ? 1 : 0));
            break;
        }

        /* i64 comparison */
        case 0x50: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i64 == 0 ? 1 : 0));
            break;
        }
        case 0x51: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i64 == b.of.i64 ? 1 : 0));
            break;
        }
        case 0x52: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i64 != b.of.i64 ? 1 : 0));
            break;
        }
        case 0x53: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i64 < b.of.i64 ? 1 : 0));
            break;
        }
        case 0x54: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((uint64_t)a.of.i64 < (uint64_t)b.of.i64 ? 1 : 0));
            break;
        }
        case 0x55: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i64 > b.of.i64 ? 1 : 0));
            break;
        }
        case 0x56: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((uint64_t)a.of.i64 > (uint64_t)b.of.i64 ? 1 : 0));
            break;
        }
        case 0x57: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i64 <= b.of.i64 ? 1 : 0));
            break;
        }
        case 0x58: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((uint64_t)a.of.i64 <= (uint64_t)b.of.i64 ? 1 : 0));
            break;
        }
        case 0x59: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i64 >= b.of.i64 ? 1 : 0));
            break;
        }
        case 0x5A: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((uint64_t)a.of.i64 >= (uint64_t)b.of.i64 ? 1 : 0));
            break;
        }

        /* f32/f64 comparison */
        case 0x5B: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f32 == b.of.f32 ? 1 : 0));
            break;
        }
        case 0x5C: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f32 != b.of.f32 ? 1 : 0));
            break;
        }
        case 0x5D: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f32 < b.of.f32 ? 1 : 0));
            break;
        }
        case 0x5E: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f32 > b.of.f32 ? 1 : 0));
            break;
        }
        case 0x5F: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f32 <= b.of.f32 ? 1 : 0));
            break;
        }
        case 0x60: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f32 >= b.of.f32 ? 1 : 0));
            break;
        }
        case 0x61: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f64 == b.of.f64 ? 1 : 0));
            break;
        }
        case 0x62: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f64 != b.of.f64 ? 1 : 0));
            break;
        }
        case 0x63: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f64 < b.of.f64 ? 1 : 0));
            break;
        }
        case 0x64: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f64 > b.of.f64 ? 1 : 0));
            break;
        }
        case 0x65: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f64 <= b.of.f64 ? 1 : 0));
            break;
        }
        case 0x66: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.f64 >= b.of.f64 ? 1 : 0));
            break;
        }

        /* i32 arithmetic */
        case 0x67: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((int32_t)wasm__clz32((uint32_t)a.of.i32)));
            break;
        }
        case 0x68: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((int32_t)wasm__ctz32((uint32_t)a.of.i32)));
            break;
        }
        case 0x69: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((int32_t)wasm__popcnt32((uint32_t)a.of.i32)));
            break;
        }
        case 0x6A: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u32_bits((uint32_t)a.of.i32 + (uint32_t)b.of.i32));
            break;
        }
        case 0x6B: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u32_bits((uint32_t)a.of.i32 - (uint32_t)b.of.i32));
            break;
        }
        case 0x6C: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u32_bits((uint32_t)a.of.i32 * (uint32_t)b.of.i32));
            break;
        }
        case 0x6D: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
            if (a.of.i32 == INT32_MIN && b.of.i32 == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
            WASM__PUSH(rt, wasm_i32(a.of.i32 / b.of.i32));
            break;
        }
        case 0x6E: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
            WASM__PUSH(rt, wasm_i32((int32_t)((uint32_t)a.of.i32 / (uint32_t)b.of.i32)));
            break;
        }
        case 0x6F: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
            WASM__PUSH(rt, wasm_i32((a.of.i32 == INT32_MIN && b.of.i32 == -1) ? 0 : a.of.i32 % b.of.i32));
            break;
        }
        case 0x70: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            if (!b.of.i32) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
            WASM__PUSH(rt, wasm_i32((int32_t)((uint32_t)a.of.i32 % (uint32_t)b.of.i32)));
            break;
        }
        case 0x71: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 & b.of.i32));
            break;
        }
        case 0x72: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 | b.of.i32));
            break;
        }
        case 0x73: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(a.of.i32 ^ b.of.i32));
            break;
        }
        case 0x74: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u32_bits((uint32_t)a.of.i32 << ((uint32_t)b.of.i32 & 31u)));
            break;
        }
        case 0x75: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u32_bits(wasm__shr_s32_bits((uint32_t)a.of.i32,
                                                              (uint32_t)b.of.i32)));
            break;
        }
        case 0x76: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((int32_t)((uint32_t)a.of.i32 >> (b.of.i32 & 31))));
            break;
        }
        case 0x77: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            uint32_t k = (uint32_t)b.of.i32 & 31u;
            WASM__PUSH(rt,
                       wasm__u32_bits(((uint32_t)a.of.i32 << k) |
                                      ((uint32_t)a.of.i32 >> ((32u - k) & 31u))));
            break;
        }
        case 0x78: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            uint32_t k = (uint32_t)b.of.i32 & 31u;
            WASM__PUSH(rt,
                       wasm__u32_bits(((uint32_t)a.of.i32 >> k) |
                                      ((uint32_t)a.of.i32 << ((32u - k) & 31u))));
            break;
        }

        /* i64 arithmetic */
        case 0x79: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i64((int64_t)wasm__clz64((uint64_t)a.of.i64)));
            break;
        }
        case 0x7A: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i64((int64_t)wasm__ctz64((uint64_t)a.of.i64)));
            break;
        }
        case 0x7B: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i64((int64_t)wasm__popcnt64((uint64_t)a.of.i64)));
            break;
        }
        case 0x7C: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u64_bits((uint64_t)a.of.i64 + (uint64_t)b.of.i64));
            break;
        }
        case 0x7D: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u64_bits((uint64_t)a.of.i64 - (uint64_t)b.of.i64));
            break;
        }
        case 0x7E: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u64_bits((uint64_t)a.of.i64 * (uint64_t)b.of.i64));
            break;
        }
        case 0x7F: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
            if (a.of.i64 == INT64_MIN && b.of.i64 == -1) WASM__TRAP(WASM_ERR_INT_OVERFLOW);
            WASM__PUSH(rt, wasm_i64(a.of.i64 / b.of.i64));
            break;
        }
        case 0x80: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
            WASM__PUSH(rt, wasm_i64((int64_t)((uint64_t)a.of.i64 / (uint64_t)b.of.i64)));
            break;
        }
        case 0x81: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
            WASM__PUSH(rt, wasm_i64((a.of.i64 == INT64_MIN && b.of.i64 == -1) ? 0 : a.of.i64 % b.of.i64));
            break;
        }
        case 0x82: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            if (!b.of.i64) WASM__TRAP(WASM_ERR_DIV_BY_ZERO);
            WASM__PUSH(rt, wasm_i64((int64_t)((uint64_t)a.of.i64 % (uint64_t)b.of.i64)));
            break;
        }
        case 0x83: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i64(a.of.i64 & b.of.i64));
            break;
        }
        case 0x84: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i64(a.of.i64 | b.of.i64));
            break;
        }
        case 0x85: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i64(a.of.i64 ^ b.of.i64));
            break;
        }
        case 0x86: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u64_bits((uint64_t)a.of.i64 << ((uint32_t)b.of.i64 & 63u)));
            break;
        }
        case 0x87: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__u64_bits(wasm__shr_s64_bits((uint64_t)a.of.i64,
                                                              (uint32_t)b.of.i64)));
            break;
        }
        case 0x88: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i64((int64_t)((uint64_t)a.of.i64 >> (b.of.i64 & 63))));
            break;
        }
        case 0x89: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            uint32_t k = (uint32_t)b.of.i64 & 63u;
            WASM__PUSH(rt,
                       wasm__u64_bits(((uint64_t)a.of.i64 << k) |
                                      ((uint64_t)a.of.i64 >> ((64u - k) & 63u))));
            break;
        }
        case 0x8A: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            uint32_t k = (uint32_t)b.of.i64 & 63u;
            WASM__PUSH(rt,
                       wasm__u64_bits(((uint64_t)a.of.i64 >> k) |
                                      ((uint64_t)a.of.i64 << ((64u - k) & 63u))));
            break;
        }

        /* f32 arithmetic */
        case 0x8B: {
            wasm_value_t a = WASM__POP(rt);
            uint32_t bits;
            memcpy(&bits, &a.of.f32, sizeof(bits));
            WASM__PUSH(rt, wasm__f32_bits(bits & 0x7FFFFFFFu));
            break;
        }
        case 0x8C: {
            wasm_value_t a = WASM__POP(rt);
            uint32_t bits;
            memcpy(&bits, &a.of.f32, sizeof(bits));
            WASM__PUSH(rt, wasm__f32_bits(bits ^ 0x80000000u));
            break;
        }
        case 0x8D: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(wasm__ceil_f32(a.of.f32)));
            break;
        }
        case 0x8E: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(wasm__floor_f32(a.of.f32)));
            break;
        }
        case 0x8F: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(wasm__trunc_f32(a.of.f32)));
            break;
        }
        case 0x90: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(wasm__nearest_f32(a.of.f32)));
            break;
        }
        case 0x91: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(wasm__sqrt_f32(a.of.f32)));
            break;
        }
        case 0x92: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(a.of.f32 + b.of.f32));
            break;
        }
        case 0x93: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(a.of.f32 - b.of.f32));
            break;
        }
        case 0x94: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(a.of.f32 * b.of.f32));
            break;
        }
        case 0x95: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(a.of.f32 / b.of.f32));
            break;
        }
        case 0x96: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(wasm__f32_min(a.of.f32, b.of.f32)));
            break;
        }
        case 0x97: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32(wasm__f32_max(a.of.f32, b.of.f32)));
            break;
        }
        case 0x98: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            uint32_t ab, bb, rb;
            memcpy(&ab, &a.of.f32, 4);
            memcpy(&bb, &b.of.f32, 4);
            rb = (ab & 0x7FFFFFFFu) | (bb & 0x80000000u);
            WASM__PUSH(rt, wasm__f32_bits(rb));
            break;
        }

        /* f64 arithmetic */
        case 0x99: {
            wasm_value_t a = WASM__POP(rt);
            uint64_t bits;
            memcpy(&bits, &a.of.f64, sizeof(bits));
            WASM__PUSH(rt, wasm__f64_bits(bits & UINT64_C(0x7FFFFFFFFFFFFFFF)));
            break;
        }
        case 0x9A: {
            wasm_value_t a = WASM__POP(rt);
            uint64_t bits;
            memcpy(&bits, &a.of.f64, sizeof(bits));
            WASM__PUSH(rt, wasm__f64_bits(bits ^ UINT64_C(0x8000000000000000)));
            break;
        }
        case 0x9B: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(wasm__ceil_f64(a.of.f64)));
            break;
        }
        case 0x9C: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(wasm__floor_f64(a.of.f64)));
            break;
        }
        case 0x9D: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(wasm__trunc_f64(a.of.f64)));
            break;
        }
        case 0x9E: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(wasm__nearest_f64(a.of.f64)));
            break;
        }
        case 0x9F: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(wasm__sqrt_f64(a.of.f64)));
            break;
        }
        case 0xA0: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(a.of.f64 + b.of.f64));
            break;
        }
        case 0xA1: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(a.of.f64 - b.of.f64));
            break;
        }
        case 0xA2: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(a.of.f64 * b.of.f64));
            break;
        }
        case 0xA3: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(a.of.f64 / b.of.f64));
            break;
        }
        case 0xA4: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(wasm__f64_min(a.of.f64, b.of.f64)));
            break;
        }
        case 0xA5: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64(wasm__f64_max(a.of.f64, b.of.f64)));
            break;
        }
        case 0xA6: {
            wasm_value_t b = WASM__POP(rt), a = WASM__POP(rt);
            uint64_t ab, bb, rb;
            memcpy(&ab, &a.of.f64, 8);
            memcpy(&bb, &b.of.f64, 8);
            rb = (ab & (uint64_t)0x7FFFFFFFFFFFFFFFull) | (bb & (uint64_t)0x8000000000000000ull);
            WASM__PUSH(rt, wasm__f64_bits(rb));
            break;
        }

        /* Conversions */
        case 0xA7: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32((int32_t)a.of.i64));
            break;
        }
        case 0xA8: {
            wasm_value_t a = WASM__POP(rt);
            wasm_value_t value;
            err = wasm__trunc_i32_s((double)a.of.f32, &value);
            if (err != WASM_OK) goto cleanup;
            WASM__PUSH(rt, value);
            break;
        }
        case 0xA9: {
            wasm_value_t a = WASM__POP(rt);
            wasm_value_t value;
            err = wasm__trunc_i32_u((double)a.of.f32, &value);
            if (err != WASM_OK) goto cleanup;
            WASM__PUSH(rt, value);
            break;
        }
        case 0xAA: {
            wasm_value_t a = WASM__POP(rt);
            wasm_value_t value;
            err = wasm__trunc_i32_s(a.of.f64, &value);
            if (err != WASM_OK) goto cleanup;
            WASM__PUSH(rt, value);
            break;
        }
        case 0xAB: {
            wasm_value_t a = WASM__POP(rt);
            wasm_value_t value;
            err = wasm__trunc_i32_u(a.of.f64, &value);
            if (err != WASM_OK) goto cleanup;
            WASM__PUSH(rt, value);
            break;
        }
        case 0xAC: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i64((int64_t)a.of.i32));
            break;
        }
        case 0xAD: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i64((int64_t)(uint32_t)a.of.i32));
            break;
        }
        case 0xAE: {
            wasm_value_t a = WASM__POP(rt);
            wasm_value_t value;
            err = wasm__trunc_i64_s((double)a.of.f32, &value);
            if (err != WASM_OK) goto cleanup;
            WASM__PUSH(rt, value);
            break;
        }
        case 0xAF: {
            wasm_value_t a = WASM__POP(rt);
            wasm_value_t value;
            err = wasm__trunc_i64_u((double)a.of.f32, &value);
            if (err != WASM_OK) goto cleanup;
            WASM__PUSH(rt, value);
            break;
        }
        case 0xB0: {
            wasm_value_t a = WASM__POP(rt);
            wasm_value_t value;
            err = wasm__trunc_i64_s(a.of.f64, &value);
            if (err != WASM_OK) goto cleanup;
            WASM__PUSH(rt, value);
            break;
        }
        case 0xB1: {
            wasm_value_t a = WASM__POP(rt);
            wasm_value_t value;
            err = wasm__trunc_i64_u(a.of.f64, &value);
            if (err != WASM_OK) goto cleanup;
            WASM__PUSH(rt, value);
            break;
        }
        case 0xB2: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32((float)a.of.i32));
            break;
        }
        case 0xB3: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32((float)(uint32_t)a.of.i32));
            break;
        }
        case 0xB4: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32((float)a.of.i64));
            break;
        }
        case 0xB5: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32((float)(uint64_t)a.of.i64));
            break;
        }
        case 0xB6: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f32((float)a.of.f64));
            break;
        }
        case 0xB7: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64((double)a.of.i32));
            break;
        }
        case 0xB8: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64((double)(uint32_t)a.of.i32));
            break;
        }
        case 0xB9: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64((double)a.of.i64));
            break;
        }
        case 0xBA: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64((double)(uint64_t)a.of.i64));
            break;
        }
        case 0xBB: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm_f64((double)a.of.f32));
            break;
        }

        /* Reinterpretations */
        case 0xBC: {
            wasm_value_t a = WASM__POP(rt);
            int32_t v;
            memcpy(&v, &a.of.f32, 4);
            WASM__PUSH(rt, wasm_i32(v));
            break;
        }
        case 0xBD: {
            wasm_value_t a = WASM__POP(rt);
            int64_t v;
            memcpy(&v, &a.of.f64, 8);
            WASM__PUSH(rt, wasm_i64(v));
            break;
        }
        case 0xBE: {
            wasm_value_t a = WASM__POP(rt);
            a.type = WASM_TYPE_F32;
            WASM__PUSH(rt, a);
            break;
        }
        case 0xBF: {
            wasm_value_t a = WASM__POP(rt);
            a.type = WASM_TYPE_F64;
            WASM__PUSH(rt, a);
            break;
        }

        /* Sign extension */
        case 0xC0: {
            wasm_value_t a = WASM__POP(rt);
            uint32_t value = (uint32_t)a.of.i32 & 0xFFu;
            if (value & 0x80u) value |= 0xFFFFFF00u;
            WASM__PUSH(rt, wasm_i32((int32_t)value));
            break;
        }
        case 0xC1: {
            wasm_value_t a = WASM__POP(rt);
            uint32_t value = (uint32_t)a.of.i32 & 0xFFFFu;
            if (value & 0x8000u) value |= 0xFFFF0000u;
            WASM__PUSH(rt, wasm_i32((int32_t)value));
            break;
        }
        case 0xC2: {
            wasm_value_t a = WASM__POP(rt);
            uint64_t value = (uint64_t)a.of.i64 & 0xFFu;
            if (value & 0x80u) value |= 0xFFFFFFFFFFFFFF00ull;
            WASM__PUSH(rt, wasm_i64((int64_t)value));
            break;
        }
        case 0xC3: {
            wasm_value_t a = WASM__POP(rt);
            uint64_t value = (uint64_t)a.of.i64 & 0xFFFFu;
            if (value & 0x8000u) value |= 0xFFFFFFFFFFFF0000ull;
            WASM__PUSH(rt, wasm_i64((int64_t)value));
            break;
        }
        case 0xC4: {
            wasm_value_t a = WASM__POP(rt);
            uint64_t value = (uint64_t)a.of.i64 & 0xFFFFFFFFull;
            if (value & 0x80000000ull) value |= 0xFFFFFFFF00000000ull;
            WASM__PUSH(rt, wasm_i64((int64_t)value));
            break;
        }

        default:
            WASM__TRAP(WASM_ERR_MALFORMED);
    }

cleanup:
    return err;
}

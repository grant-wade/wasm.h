#include "simd.h"
#include "../module.h"
#include "state.h"
#include <stdio.h>
#include "../instructions.h"
#include "../memory_internal.h"
#include "../portable.h"
#include "../reader.h"
#include "../runtime.h"
#include "../simd/compare.h"
#include "../simd/float.h"
#include "../simd/integer_small.h"
#include "../simd/integer_wide.h"
#include "../simd/lanes.h"
#include "../values.h"
#include <string.h>

WASM_PRIVATE wasm_error_t wasm__exec_simd(wasm_module_t* mod, wasm__call_frame_t* cf) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err = WASM_OK;
    uint32_t subop = wasm__read_leb128_u32(&cf->r);

    switch (subop) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x5C:
        case 0x5D: {
            wasm__memarg_t memarg;
            wasm_value_t base;
            wasm_value_t vector = wasm_v128_zero();
            wasm_memory_t* memory;
            uint64_t addr;
            uint64_t msz;
            uint32_t lane;

            err = wasm__read_memarg(&cf->r, &memarg);
            if (err != WASM_OK) goto cleanup;
            base = WASM__POP(rt);
            memory = wasm__memory_at(mod, memarg.memory_index);
            if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
            if (!wasm__memory_effective_addr(memory, &base, memarg.offset, &addr)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            msz = wasm__memory_byte_size(memory);

            switch (subop) {
                case 0x00:
                    if (!wasm__memory_access_in_bounds(addr, 16u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    memcpy(vector.of.v128, memory->data + addr, 16);
                    break;
                case 0x01:
                case 0x02:
                    if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    for (lane = 0; lane < 8; lane++) {
                        if (subop == 0x01)
                            wasm__v128_set_i16(&vector, lane, (int8_t)memory->data[addr + lane]);
                        else
                            wasm__v128_set_u16(&vector, lane, memory->data[addr + lane]);
                    }
                    break;
                case 0x03:
                case 0x04:
                    if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    for (lane = 0; lane < 4; lane++) {
                        uint16_t loaded = wasm__load_le16(memory->data + addr + lane * 2u);
                        if (subop == 0x03)
                            wasm__v128_set_i32(&vector, lane, (int16_t)loaded);
                        else
                            wasm__v128_set_u32(&vector, lane, loaded);
                    }
                    break;
                case 0x05:
                case 0x06:
                    if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    for (lane = 0; lane < 2; lane++) {
                        uint32_t loaded = wasm__load_le32(memory->data + addr + lane * 4u);
                        if (subop == 0x05)
                            wasm__v128_set_i64(&vector, lane, (int32_t)loaded);
                        else
                            wasm__v128_set_u64(&vector, lane, loaded);
                    }
                    break;
                case 0x07:
                    if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    for (lane = 0; lane < 16; lane++) vector.of.v128[lane] = memory->data[addr];
                    break;
                case 0x08: {
                    uint16_t loaded;
                    if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    loaded = wasm__load_le16(memory->data + addr);
                    for (lane = 0; lane < 8; lane++) wasm__v128_set_u16(&vector, lane, loaded);
                    break;
                }
                case 0x09: {
                    uint32_t loaded;
                    if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    loaded = wasm__load_le32(memory->data + addr);
                    for (lane = 0; lane < 4; lane++) wasm__v128_set_u32(&vector, lane, loaded);
                    break;
                }
                case 0x0A: {
                    uint64_t loaded;
                    if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    loaded = wasm__load_le64(memory->data + addr);
                    for (lane = 0; lane < 2; lane++) wasm__v128_set_u64(&vector, lane, loaded);
                    break;
                }
                case 0x5C:
                    if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    wasm__v128_set_u32(&vector, 0, wasm__load_le32(memory->data + addr));
                    break;
                default:
                    if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    wasm__v128_set_u64(&vector, 0, wasm__load_le64(memory->data + addr));
                    break;
            }

            WASM__PUSH(rt, vector);
            break;
        }
        case 0x0B: {
            wasm__memarg_t memarg;
            wasm_value_t vector;
            wasm_value_t base;
            wasm_memory_t* memory;
            uint64_t addr;
            uint64_t msz;

            err = wasm__read_memarg(&cf->r, &memarg);
            if (err != WASM_OK) goto cleanup;
            vector = WASM__POP(rt);
            base = WASM__POP(rt);
            memory = wasm__memory_at(mod, memarg.memory_index);
            if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
            if (!wasm__memory_effective_addr(memory, &base, memarg.offset, &addr)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            msz = wasm__memory_byte_size(memory);
            if (!wasm__memory_access_in_bounds(addr, 16u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            memcpy(memory->data + addr, vector.of.v128, 16);
            break;
        }
        case 0x0C: {
            wasm_value_t vector = wasm_v128_zero();
            if (!wasm__has(&cf->r, 16)) WASM__TRAP(WASM_ERR_MALFORMED);
            memcpy(vector.of.v128, cf->r.ptr, 16);
            cf->r.ptr += 16;
            WASM__PUSH(rt, vector);
            break;
        }
        case 0x0D: {
            wasm_value_t rhs;
            wasm_value_t lhs;
            wasm_value_t vector;
            uint8_t lanes[16];
            uint32_t lane;

            for (lane = 0; lane < 16; lane++) lanes[lane] = wasm__read_u8(&cf->r);
            if (cf->r.malformed) WASM__TRAP(WASM_ERR_MALFORMED);
            rhs = WASM__POP(rt);
            lhs = WASM__POP(rt);
            vector = wasm__simd_shuffle(&lhs, &rhs, lanes);
            WASM__PUSH(rt, vector);
            break;
        }
        case 0x0E: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_swizzle(&lhs, &rhs));
            break;
        }
        case 0x100: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_swizzle(&lhs, &rhs));
            break;
        }
        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14: {
            wasm_value_t scalar = WASM__POP(rt);
            wasm_value_t vector = wasm_v128_zero();
            uint32_t lane;

            switch (subop) {
                case 0x0F:
                    for (lane = 0; lane < 16; lane++) wasm__v128_set_u8(&vector, lane, (uint8_t)scalar.of.i32);
                    break;
                case 0x10:
                    for (lane = 0; lane < 8; lane++) wasm__v128_set_u16(&vector, lane, (uint16_t)scalar.of.i32);
                    break;
                case 0x11:
                    for (lane = 0; lane < 4; lane++) wasm__v128_set_i32(&vector, lane, scalar.of.i32);
                    break;
                case 0x12:
                    for (lane = 0; lane < 2; lane++) wasm__v128_set_i64(&vector, lane, scalar.of.i64);
                    break;
                case 0x13:
                    {
                        uint32_t bits;
                        memcpy(&bits, &scalar.of.f32, sizeof(bits));
                        for (lane = 0; lane < 4; lane++) wasm__v128_set_u32(&vector, lane, bits);
                    }
                    break;
                default:
                    {
                        uint64_t bits;
                        memcpy(&bits, &scalar.of.f64, sizeof(bits));
                        for (lane = 0; lane < 2; lane++) wasm__v128_set_u64(&vector, lane, bits);
                    }
                    break;
            }

            WASM__PUSH(rt, vector);
            break;
        }
        case 0x15:
        case 0x16:
        case 0x18:
        case 0x19:
        case 0x1B:
        case 0x1D:
        case 0x1F:
        case 0x21: {
            uint32_t lane = wasm__read_u8(&cf->r);
            wasm_value_t vector = WASM__POP(rt);
            if (cf->r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);

            switch (subop) {
                case 0x15:
                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_i8(&vector, lane)));
                    break;
                case 0x16:
                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_u8(&vector, lane)));
                    break;
                case 0x18:
                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_i16(&vector, lane)));
                    break;
                case 0x19:
                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_u16(&vector, lane)));
                    break;
                case 0x1B:
                    WASM__PUSH(rt, wasm_i32(wasm__v128_get_i32(&vector, lane)));
                    break;
                case 0x1D:
                    WASM__PUSH(rt, wasm_i64(wasm__v128_get_i64(&vector, lane)));
                    break;
                case 0x1F:
                    WASM__PUSH(rt, wasm__f32_bits(wasm__v128_get_u32(&vector, lane)));
                    break;
                default:
                    WASM__PUSH(rt, wasm__f64_bits(wasm__v128_get_u64(&vector, lane)));
                    break;
            }
            break;
        }
        case 0x17:
        case 0x1A:
        case 0x1C:
        case 0x1E:
        case 0x20:
        case 0x22: {
            uint32_t lane = wasm__read_u8(&cf->r);
            wasm_value_t scalar = WASM__POP(rt);
            wasm_value_t vector = WASM__POP(rt);
            if (cf->r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);

            switch (subop) {
                case 0x17:
                    wasm__v128_set_u8(&vector, lane, (uint8_t)scalar.of.i32);
                    break;
                case 0x1A:
                    wasm__v128_set_u16(&vector, lane, (uint16_t)scalar.of.i32);
                    break;
                case 0x1C:
                    wasm__v128_set_i32(&vector, lane, scalar.of.i32);
                    break;
                case 0x1E:
                    wasm__v128_set_i64(&vector, lane, scalar.of.i64);
                    break;
                case 0x20:
                    {
                        uint32_t bits;
                        memcpy(&bits, &scalar.of.f32, sizeof(bits));
                        wasm__v128_set_u32(&vector, lane, bits);
                    }
                    break;
                default:
                    {
                        uint64_t bits;
                        memcpy(&bits, &scalar.of.f64, sizeof(bits));
                        wasm__v128_set_u64(&vector, lane, bits);
                    }
                    break;
            }

            WASM__PUSH(rt, vector);
            break;
        }
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
        case 0x2C: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_cmp_i8x16(subop, &lhs, &rhs));
            break;
        }
        case 0x2D:
        case 0x2E:
        case 0x2F:
        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_cmp_i16x8(subop, &lhs, &rhs));
            break;
        }
        case 0x37:
        case 0x38:
        case 0x39:
        case 0x3A:
        case 0x3B:
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
        case 0x40: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_cmp_i32x4(subop, &lhs, &rhs));
            break;
        }
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_cmp_f32x4(subop, &lhs, &rhs));
            break;
        }
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_cmp_f64x2(subop, &lhs, &rhs));
            break;
        }
        case 0x4D: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_bitwise_not(&value));
            break;
        }
        case 0x4E:
        case 0x4F:
        case 0x50:
        case 0x51: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_bitwise_binary(subop, &lhs, &rhs));
            break;
        }
        case 0x52: {
            wasm_value_t mask = WASM__POP(rt);
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_bitselect(&lhs, &rhs, &mask));
            break;
        }
        case 0x105:
        case 0x106:
        case 0x107:
        case 0x108: {
            wasm_value_t acc = WASM__POP(rt);
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_relaxed_madd(subop, &lhs, &rhs, &acc));
            break;
        }
        case 0x109:
        case 0x10A:
        case 0x10B:
        case 0x10C: {
            wasm_value_t mask = WASM__POP(rt);
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_bitselect(&lhs, &rhs, &mask));
            break;
        }
        case 0x53: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(wasm__simd_any_true(&value) ? 1 : 0));
            break;
        }
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57: {
            wasm__memarg_t memarg;
            uint32_t lane = 0;
            wasm_value_t vector;
            wasm_value_t base;
            wasm_memory_t* memory;
            uint64_t addr;
            uint64_t msz;

            err = wasm__read_memarg(&cf->r, &memarg);
            if (err != WASM_OK) goto cleanup;
            lane = wasm__read_u8(&cf->r);
            vector = WASM__POP(rt);
            base = WASM__POP(rt);
            if (cf->r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);
            memory = wasm__memory_at(mod, memarg.memory_index);
            if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
            if (!wasm__memory_effective_addr(memory, &base, memarg.offset, &addr)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            msz = wasm__memory_byte_size(memory);

            switch (subop) {
                case 0x54:
                    if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    wasm__v128_set_u8(&vector, lane, memory->data[addr]);
                    break;
                case 0x55:
                    if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    wasm__v128_set_u16(&vector, lane, wasm__load_le16(memory->data + addr));
                    break;
                case 0x56:
                    if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    wasm__v128_set_u32(&vector, lane, wasm__load_le32(memory->data + addr));
                    break;
                default:
                    if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    wasm__v128_set_u64(&vector, lane, wasm__load_le64(memory->data + addr));
                    break;
            }

            WASM__PUSH(rt, vector);
            break;
        }
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B: {
            wasm__memarg_t memarg;
            uint32_t lane = 0;
            wasm_value_t vector;
            wasm_value_t base;
            wasm_memory_t* memory;
            uint64_t addr;
            uint64_t msz;

            err = wasm__read_memarg(&cf->r, &memarg);
            if (err != WASM_OK) goto cleanup;
            lane = wasm__read_u8(&cf->r);
            vector = WASM__POP(rt);
            base = WASM__POP(rt);
            if (cf->r.malformed || lane >= wasm__simd_lane_limit(subop)) WASM__TRAP(WASM_ERR_MALFORMED);
            memory = wasm__memory_at(mod, memarg.memory_index);
            if (!memory) WASM__TRAP(WASM_ERR_MALFORMED);
            if (!wasm__memory_effective_addr(memory, &base, memarg.offset, &addr)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            msz = wasm__memory_byte_size(memory);

            switch (subop) {
                case 0x58:
                    if (!wasm__memory_access_in_bounds(addr, 1u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    memory->data[addr] = wasm__v128_get_u8(&vector, lane);
                    break;
                case 0x59:
                    if (!wasm__memory_access_in_bounds(addr, 2u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    wasm__store_le16(memory->data + addr, wasm__v128_get_u16(&vector, lane));
                    break;
                case 0x5A:
                    if (!wasm__memory_access_in_bounds(addr, 4u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    wasm__store_le32(memory->data + addr, wasm__v128_get_u32(&vector, lane));
                    break;
                default:
                    if (!wasm__memory_access_in_bounds(addr, 8u, msz)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    wasm__store_le64(memory->data + addr, wasm__v128_get_u64(&vector, lane));
                    break;
            }

            break;
        }
        case 0x5E:
        case 0x5F:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFD:
        case 0xFE:
        case 0xFF:
        case 0x101:
        case 0x102:
        case 0x103:
        case 0x104: {
            wasm_value_t value = WASM__POP(rt);
            wasm_value_t vector;

            switch (subop) {
                case 0x5E:
                    vector = wasm__simd_demote_f64x2_zero(&value);
                    break;
                case 0x5F:
                    vector = wasm__simd_promote_low_f32x4(&value);
                    break;
                case 0x7C:
                case 0x7D:
                case 0x7E:
                case 0x7F:
                    vector = wasm__simd_extadd_pairwise(subop, &value);
                    break;
                case 0xF8:
                case 0xF9:
                    vector = wasm__simd_trunc_sat_f32x4(subop, &value);
                    break;
                case 0xFA:
                case 0xFB:
                    vector = wasm__simd_convert_i32x4(subop, &value);
                    break;
                case 0xFC:
                case 0xFD:
                    vector = wasm__simd_trunc_sat_f64x2_zero(subop, &value);
                    break;
                case 0x101:
                    vector = wasm__simd_trunc_sat_f32x4(0xF8, &value);
                    break;
                case 0x102:
                    vector = wasm__simd_trunc_sat_f32x4(0xF9, &value);
                    break;
                case 0x103:
                    vector = wasm__simd_trunc_sat_f64x2_zero(0xFC, &value);
                    break;
                case 0x104:
                    vector = wasm__simd_trunc_sat_f64x2_zero(0xFD, &value);
                    break;
                default:
                    vector = wasm__simd_convert_low_i32x4_to_f64x2(subop, &value);
                    break;
            }

            WASM__PUSH(rt, vector);
            break;
        }
        case 0x60:
        case 0x61:
        case 0x62: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i8x16_unary(subop, &value));
            break;
        }
        case 0x63: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(wasm__simd_all_true_i8x16(&value) ? 1 : 0));
            break;
        }
        case 0x64: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(wasm__simd_bitmask_i8x16(&value)));
            break;
        }
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
        case 0x7B: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i8x16_binary(subop, &lhs, &rhs));
            break;
        }
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0xE0:
        case 0xE1:
        case 0xE3: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_f32x4_unary(subop, &value));
            break;
        }
        case 0x6B:
        case 0x6C:
        case 0x6D: {
            wasm_value_t amount = WASM__POP(rt);
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i8x16_shift(subop, &value, (uint32_t)amount.of.i32));
            break;
        }
        case 0x74:
        case 0x75:
        case 0x7A:
        case 0x94:
        case 0xEC:
        case 0xED:
        case 0xEF: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_f64x2_unary(subop, &value));
            break;
        }
        case 0x80:
        case 0x81:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i16x8_unary(subop, &value));
            break;
        }
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
        case 0x9F: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i16x8_binary(subop, &lhs, &rhs));
            break;
        }
        case 0x83: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(wasm__simd_all_true_i16x8(&value) ? 1 : 0));
            break;
        }
        case 0x84: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(wasm__simd_bitmask_i16x8(&value)));
            break;
        }
        case 0x8B:
        case 0x8C:
        case 0x8D: {
            wasm_value_t amount = WASM__POP(rt);
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i16x8_shift(subop, &value, (uint32_t)amount.of.i32));
            break;
        }
        case 0xA0:
        case 0xA1:
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i32x4_unary(subop, &value));
            break;
        }
        case 0xA3: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(wasm__simd_all_true_i32x4(&value) ? 1 : 0));
            break;
        }
        case 0xA4: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(wasm__simd_bitmask_i32x4(&value)));
            break;
        }
        case 0xAB:
        case 0xAC:
        case 0xAD: {
            wasm_value_t amount = WASM__POP(rt);
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i32x4_shift(subop, &value, (uint32_t)amount.of.i32));
            break;
        }
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
        case 0xBF: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i32x4_binary(subop, &lhs, &rhs));
            break;
        }
        case 0xC0:
        case 0xC1:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i64x2_unary(subop, &value));
            break;
        }
        case 0xC3: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(wasm__simd_all_true_i64x2(&value) ? 1 : 0));
            break;
        }
        case 0xC4: {
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm_i32(wasm__simd_bitmask_i64x2(&value)));
            break;
        }
        case 0xCB:
        case 0xCC:
        case 0xCD: {
            wasm_value_t amount = WASM__POP(rt);
            wasm_value_t value = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i64x2_shift(subop, &value, (uint32_t)amount.of.i32));
            break;
        }
        case 0xCE:
        case 0xD1:
        case 0xD5:
        case 0xD6:
        case 0xD7:
        case 0xD8:
        case 0xD9:
        case 0xDA:
        case 0xDB:
        case 0xDC:
        case 0xDD:
        case 0xDE:
        case 0xDF: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            if (subop >= 0xD6 && subop <= 0xDB)
                WASM__PUSH(rt, wasm__simd_cmp_i64x2(subop, &lhs, &rhs));
            else
                WASM__PUSH(rt, wasm__simd_i64x2_binary(subop, &lhs, &rhs));
            break;
        }
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_f32x4_binary(subop, &lhs, &rhs));
            break;
        }
        case 0xF0:
        case 0xF1:
        case 0xF2:
        case 0xF3:
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
        case 0x10D:
        case 0x10E:
        case 0x10F:
        case 0x110: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            if (subop >= 0x10D) {
                switch (subop) {
                    case 0x10D:
                        WASM__PUSH(rt, wasm__simd_f32x4_binary(0xE8, &lhs, &rhs));
                        break;
                    case 0x10E:
                        WASM__PUSH(rt, wasm__simd_f32x4_binary(0xE9, &lhs, &rhs));
                        break;
                    case 0x10F:
                        WASM__PUSH(rt, wasm__simd_f64x2_binary(0xF4, &lhs, &rhs));
                        break;
                    default:
                        WASM__PUSH(rt, wasm__simd_f64x2_binary(0xF5, &lhs, &rhs));
                        break;
                }
            } else {
                WASM__PUSH(rt, wasm__simd_f64x2_binary(subop, &lhs, &rhs));
            }
            break;
        }
        case 0x111: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_i16x8_binary(0x82, &lhs, &rhs));
            break;
        }
        case 0x112: {
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_relaxed_dot_i8x16_i7x16_s(&lhs, &rhs));
            break;
        }
        case 0x113: {
            wasm_value_t acc = WASM__POP(rt);
            wasm_value_t rhs = WASM__POP(rt);
            wasm_value_t lhs = WASM__POP(rt);
            WASM__PUSH(rt, wasm__simd_relaxed_dot_i8x16_i7x16_add_s(&lhs, &rhs, &acc));
            break;
        }
        default:
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown prefixed opcode 0xFD 0x%X", (unsigned)subop);
            err = WASM_ERR_MALFORMED;
            goto cleanup;
    }

cleanup:
    return err;
}

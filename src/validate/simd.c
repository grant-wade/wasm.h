#include "simd.h"
#include "../module.h"
#include "../instructions.h"
#include "../reader.h"
#include "stack.h"

static wasm_error_t wasm__validator_read_simd_lane(wasm__validator_t* v,
                                                   const uint8_t* at,
                                                   uint32_t subop) {
    uint32_t lane_limit = wasm__simd_lane_limit(subop);
    uint32_t lane = wasm__read_u8(&v->r);

    if (lane >= lane_limit)
        return wasm__validator_error(v, at, "lane %u out of range", (unsigned)lane);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_validate_simd(wasm__validator_t* v, const uint8_t* at) {
    uint32_t subop = wasm__read_leb128_u32(&v->r);
    wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_SIMD);

    if (err != WASM_OK) return err;

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
            uint32_t max_align = wasm__simd_memory_align_log2(subop);
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "SIMD memory load requires a memory");
            err = wasm__validator_read_memarg(v, at, max_align, &memarg);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_memory_index(v, at, memarg.memory_index);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        }
        case 0x0B: {
            wasm__memarg_t memarg;
            uint32_t max_align = wasm__simd_memory_align_log2(subop);
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "SIMD memory store requires a memory");
            err = wasm__validator_read_memarg(v, at, max_align, &memarg);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_memory_index(v, at, memarg.memory_index);
        }
        case 0x0C:
            v->r.ptr += 16;
            if (v->r.ptr > v->r.end) {
                v->r.ptr = v->r.end;
                v->r.malformed = 1;
            }
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x0D: {
            uint32_t lane;
            for (lane = 0; lane < 16; lane++) {
                uint32_t index = wasm__read_u8(&v->r);
                if (index >= 32u)
                    return wasm__validator_error(v, at, "shuffle lane %u out of range", (unsigned)index);
            }
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        }
        case 0x0E:
        case 0x100:
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x0F:
        case 0x10:
        case 0x11:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x12:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I64);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x105:
        case 0x106:
        case 0x107:
        case 0x108:
        case 0x109:
        case 0x10A:
        case 0x10B:
        case 0x10C:
        case 0x113:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x13:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_F32);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x14:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_F64);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x15:
        case 0x16:
        case 0x17:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x17) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        case 0x18:
        case 0x19:
        case 0x1A:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x1A) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        case 0x1B:
        case 0x1C:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x1C) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        case 0x1D:
        case 0x1E:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x1E) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_I64);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I64);
        case 0x1F:
        case 0x20:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x20) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_F32);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_F32);
        case 0x21:
        case 0x22:
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            if (subop == 0x22) {
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_F64);
                if (err != WASM_OK) return err;
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
                if (err != WASM_OK) return err;
                return wasm__validator_push(v, at, WASM_TYPE_V128);
            }
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_F64);
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
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x4D:
            return wasm__validator_unary(v, at, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x4E:
        case 0x4F:
        case 0x50:
        case 0x51:
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x52:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        case 0x53:
        case 0x63:
        case 0x64:
        case 0x83:
        case 0x84:
        case 0xA3:
        case 0xA4:
        case 0xC3:
        case 0xC4:
            return wasm__validator_unary(v, at, WASM_TYPE_V128, WASM_TYPE_I32);
        case 0x54:
        case 0x55:
        case 0x56:
        case 0x57: {
            wasm__memarg_t memarg;
            uint32_t max_align = wasm__simd_memory_align_log2(subop);
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "SIMD lane load requires a memory");
            err = wasm__validator_read_memarg(v, at, max_align, &memarg);
            if (err != WASM_OK) return err;
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_memory_index(v, at, memarg.memory_index);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        }
        case 0x58:
        case 0x59:
        case 0x5A:
        case 0x5B: {
            wasm__memarg_t memarg;
            uint32_t max_align = wasm__simd_memory_align_log2(subop);
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "SIMD lane store requires a memory");
            err = wasm__validator_read_memarg(v, at, max_align, &memarg);
            if (err != WASM_OK) return err;
            err = wasm__validator_read_simd_lane(v, at, subop);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_memory_index(v, at, memarg.memory_index);
        }
        case 0x5E:
        case 0x5F:
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x67:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x74:
        case 0x75:
        case 0x7A:
        case 0x94:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
        case 0x80:
        case 0x81:
        case 0x87:
        case 0x88:
        case 0x89:
        case 0x8A:
        case 0xA0:
        case 0xA1:
        case 0xA7:
        case 0xA8:
        case 0xA9:
        case 0xAA:
        case 0xC0:
        case 0xC1:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA:
        case 0xE0:
        case 0xE1:
        case 0xE3:
        case 0xEC:
        case 0xED:
        case 0xEF:
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
        case 0x104:
            return wasm__validator_unary(v, at, WASM_TYPE_V128, WASM_TYPE_V128);
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
        case 0xDF:
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
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
        case 0x110:
        case 0x111:
        case 0x112:
            return wasm__validator_binary(v, at, WASM_TYPE_V128, WASM_TYPE_V128, WASM_TYPE_V128);
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x8B:
        case 0x8C:
        case 0x8D:
        case 0xAB:
        case 0xAC:
        case 0xAD:
        case 0xCB:
        case 0xCC:
        case 0xCD:
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_V128);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_V128);
        default:
            return wasm__validator_error(v, at, "unknown prefixed opcode 0xFD 0x%X", (unsigned)subop);
    }
}

#include "bulk.h"
#include "../module.h"
#include "slots.h"
#include "state.h"
#include <stdio.h>
#include "../internal/alloc.h"
#include "../memory_internal.h"
#include "../reader.h"
#include "../runtime.h"
#include "../types/references.h"
#include "../values.h"
#include <string.h>

/* Full 128-bit product of two unsigned 64-bit values, built from 32-bit partial products. */
static void wasm__mul_wide_u64(uint64_t a, uint64_t b, uint64_t* lo, uint64_t* hi) {
    uint64_t a_lo = a & 0xFFFFFFFFu, a_hi = a >> 32;
    uint64_t b_lo = b & 0xFFFFFFFFu, b_hi = b >> 32;
    uint64_t ll = a_lo * b_lo, lh = a_lo * b_hi, hl = a_hi * b_lo, hh = a_hi * b_hi;
    uint64_t mid = (ll >> 32) + (lh & 0xFFFFFFFFu) + (hl & 0xFFFFFFFFu);

    *lo = (mid << 32) | (ll & 0xFFFFFFFFu);
    *hi = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
}

WASM_PRIVATE wasm_error_t wasm__exec_bulk(wasm_module_t* mod, wasm__call_frame_t* cf) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err = WASM_OK;
    uint32_t subop = wasm__read_leb128_u32(&cf->r);
    switch (subop) {
        case 0x08: {
            uint32_t data_idx = wasm__read_leb128_u32(&cf->r);
            uint32_t mem_idx = wasm__read_leb128_u32(&cf->r);
            const wasm__slot_t* count = WASM__POP(rt);
            const wasm__slot_t* src = WASM__POP(rt);
            const wasm__slot_t* dst = WASM__POP(rt);
            uint64_t len;
            uint64_t src_offset;
            uint64_t dst_offset;
            wasm_memory_t* memory;
            uint64_t mem_size;
            wasm_data_segment_t* segment;

            if (data_idx >= mod->num_data_segments) {
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown data segment %u", (unsigned)data_idx);
                err = WASM_ERR_MALFORMED;
                goto cleanup;
            }
            memory = wasm__memory_at(mod, mem_idx);
            if (!memory) {
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown memory %u", (unsigned)mem_idx);
                err = WASM_ERR_MALFORMED;
                goto cleanup;
            }
            mem_size = wasm__memory_byte_size(memory);
            len = wasm__slot_u32(count);
            src_offset = wasm__slot_u32(src);
            dst_offset = wasm__slot_index(dst, memory->is_64);

            segment = &mod->data_segments[data_idx];
            if (!wasm__range_in_bounds_u64(src_offset, len, segment->size) ||
                !wasm__range_in_bounds_u64(dst_offset, len, mem_size))
                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            WASM__CHARGE_BYTES(rt, len);
            if (len > 0)
                memcpy(memory->data + dst_offset, segment->bytes + src_offset, (size_t)len);
            break;
        }
        case 0x09: {
            uint32_t data_idx = wasm__read_leb128_u32(&cf->r);

            if (data_idx >= mod->num_data_segments) {
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown data segment %u", (unsigned)data_idx);
                err = WASM_ERR_MALFORMED;
                goto cleanup;
            }

            if (mod->data_segments[data_idx].is_passive) {
                mod->data_segments[data_idx].is_dropped = 1;
                mod->data_segments[data_idx].size = 0;
            }
            break;
        }
        case 0x0A: {
            uint32_t dst_mem_idx = wasm__read_leb128_u32(&cf->r);
            uint32_t src_mem_idx = wasm__read_leb128_u32(&cf->r);
            const wasm__slot_t* count = WASM__POP(rt);
            const wasm__slot_t* src = WASM__POP(rt);
            const wasm__slot_t* dst = WASM__POP(rt);
            uint64_t len;
            uint64_t src_offset;
            uint64_t dst_offset;
            wasm_memory_t* dst_memory = wasm__memory_at(mod, dst_mem_idx);
            wasm_memory_t* src_memory = wasm__memory_at(mod, src_mem_idx);
            uint64_t dst_mem_size;
            uint64_t src_mem_size;

            if (!dst_memory || !src_memory) {
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED,
                              "unknown memory %u,%u",
                              (unsigned)dst_mem_idx, (unsigned)src_mem_idx);
                err = WASM_ERR_MALFORMED;
                goto cleanup;
            }
            dst_mem_size = wasm__memory_byte_size(dst_memory);
            src_mem_size = wasm__memory_byte_size(src_memory);
            len = wasm__slot_index(count, dst_memory->is_64 && src_memory->is_64);
            src_offset = wasm__slot_index(src, src_memory->is_64);
            dst_offset = wasm__slot_index(dst, dst_memory->is_64);
            if (!wasm__range_in_bounds_u64(src_offset, len, src_mem_size) ||
                !wasm__range_in_bounds_u64(dst_offset, len, dst_mem_size))
                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            WASM__CHARGE_BYTES(rt, len);
            if (len > 0)
                memmove(dst_memory->data + dst_offset, src_memory->data + src_offset, (size_t)len);
            break;
        }
        case 0x0B: {
            uint32_t mem_idx = wasm__read_leb128_u32(&cf->r);
            const wasm__slot_t* count = WASM__POP(rt);
            const wasm__slot_t* value = WASM__POP(rt);
            const wasm__slot_t* dst = WASM__POP(rt);
            uint64_t len;
            uint64_t dst_offset;
            wasm_memory_t* memory = wasm__memory_at(mod, mem_idx);
            uint64_t mem_size;

            if (!memory) {
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown memory %u", (unsigned)mem_idx);
                err = WASM_ERR_MALFORMED;
                goto cleanup;
            }
            mem_size = wasm__memory_byte_size(memory);
            len = wasm__slot_index(count, memory->is_64);
            dst_offset = wasm__slot_index(dst, memory->is_64);
            if (!wasm__range_in_bounds_u64(dst_offset, len, mem_size)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            WASM__CHARGE_BYTES(rt, len);
            if (len > 0)
                memset(memory->data + dst_offset, (unsigned char)(wasm__slot_u32(value) & 0xFFu), (size_t)len);
            break;
        }
        case 0x0C: {
            uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);
            uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
            const wasm__slot_t* count = WASM__POP(rt);
            const wasm__slot_t* src = WASM__POP(rt);
            const wasm__slot_t* dst = WASM__POP(rt);
            uint32_t len = wasm__slot_u32(count);
            uint32_t src_offset = wasm__slot_u32(src);
            uint64_t dst_offset;
            wasm_elem_segment_t* segment;
            wasm_table_t* table;
            uint32_t j;

            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
            if (elem_idx >= mod->num_elem_segments) {
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown elem segment %u", (unsigned)elem_idx);
                err = WASM_ERR_MALFORMED;
                goto cleanup;
            }

            table = wasm__table_at(mod, table_idx);
            if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
            dst_offset = wasm__slot_index(dst, table->is_64);
            segment = &mod->elem_segments[elem_idx];
            if (segment->is_declarative) {
                WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "out of bounds table access");
                err = WASM_ERR_TRAP;
                goto cleanup;
            }
            if (!wasm__typed_valtype_is_subtype(mod,
                                                segment->elem_type,
                                                &segment->elem_ref_type,
                                                mod->tables[table_idx].reftype,
                                                &mod->tables[table_idx].reftype_info))
                WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
            if (!wasm__range_in_bounds_u64(src_offset, len, segment->num_elems) ||
                !wasm__range_in_bounds_u64(dst_offset, len, table->size))
                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            WASM__CHARGE_ELEMS(rt, len);
            if (len > 0) {
                if (!table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                for (j = 0; j < len; j++) table->elems[dst_offset + j] = segment->elems[src_offset + j];
            }
            break;
        }
        case 0x0D: {
            uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);

            if (elem_idx >= mod->num_elem_segments) {
                WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown elem segment %u", (unsigned)elem_idx);
                err = WASM_ERR_MALFORMED;
                goto cleanup;
            }

            if (mod->elem_segments[elem_idx].is_passive) {
                mod->elem_segments[elem_idx].is_dropped = 1;
                mod->elem_segments[elem_idx].num_elems = 0;
            }
            break;
        }
        case 0x0E: {
            uint32_t dst_table_idx = wasm__read_leb128_u32(&cf->r);
            uint32_t src_table_idx = wasm__read_leb128_u32(&cf->r);
            const wasm__slot_t* count = WASM__POP(rt);
            const wasm__slot_t* src = WASM__POP(rt);
            const wasm__slot_t* dst = WASM__POP(rt);
            uint64_t len;
            uint64_t src_offset;
            uint64_t dst_offset;
            wasm_table_t* dst_table;
            wasm_table_t* src_table;

            if (dst_table_idx >= mod->num_tables || src_table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
            dst_table = wasm__table_at(mod, dst_table_idx);
            src_table = wasm__table_at(mod, src_table_idx);
            if (!dst_table || !src_table) WASM__TRAP(WASM_ERR_MALFORMED);
            dst_offset = wasm__slot_index(dst, dst_table->is_64);
            src_offset = wasm__slot_index(src, src_table->is_64);
            len = wasm__slot_index(count, wasm__table_copy_size_type(dst_table, src_table) == WASM_TYPE_I64);
            if (!wasm__typed_valtype_is_subtype(mod,
                                                mod->tables[src_table_idx].reftype,
                                                &mod->tables[src_table_idx].reftype_info,
                                                mod->tables[dst_table_idx].reftype,
                                                &mod->tables[dst_table_idx].reftype_info))
                WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
            if (!wasm__range_in_bounds_u64(src_offset, len, src_table->size) ||
                !wasm__range_in_bounds_u64(dst_offset, len, dst_table->size))
                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            WASM__CHARGE_ELEMS(rt, len);
            if (len > 0) {
                if (!dst_table->elems || !src_table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                memmove(dst_table->elems + dst_offset,
                        src_table->elems + src_offset,
                        (size_t)len * sizeof(wasm_value_t));
            }
            break;
        }
        case 0x0F: {
            uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
            const wasm__slot_t* delta_value;
            wasm_value_t init_value;
            wasm_table_t* table;
            uint64_t old_size;
            uint64_t delta;

            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
            table = wasm__table_at(mod, table_idx);
            if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
            delta_value = WASM__POP(rt);
            init_value = wasm__pop_value(rt, table->reftype, &table->reftype_info);
            delta = wasm__slot_index(delta_value, table->is_64);
            if (rt->fuel_enabled && rt->fuel < delta / WASM__FUEL_ELEMS_PER_UNIT &&
                (table->max_size == UINT64_MAX || delta <= table->max_size - table->size)) {
                rt->fuel = 0;
                WASM__TRAP(WASM_ERR_OUT_OF_FUEL);
            }
            old_size = wasm__table_grow(rt, table, delta, init_value);
            if (old_size != UINT64_MAX) WASM__CHARGE_ELEMS(rt, delta);
            wasm__slot_set_index(WASM__PUSH(rt), old_size, table->is_64);
            break;
        }
        case 0x10: {
            uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
            wasm_table_t* table;

            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
            table = wasm__table_at(mod, table_idx);
            if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
            wasm__slot_set_index(WASM__PUSH(rt), table->size, table->is_64);
            break;
        }
        case 0x11: {
            uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
            const wasm__slot_t* count;
            wasm_value_t value;
            const wasm__slot_t* dst;
            wasm_table_t* table;
            uint64_t len;
            uint64_t dst_offset;
            uint32_t j;

            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
            table = wasm__table_at(mod, table_idx);
            if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
            count = WASM__POP(rt);
            value = wasm__pop_value(rt, table->reftype, &table->reftype_info);
            dst = WASM__POP(rt);
            len = wasm__slot_index(count, table->is_64);
            dst_offset = wasm__slot_index(dst, table->is_64);
            if (!wasm__range_in_bounds_u64(dst_offset, len, table->size)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            WASM__CHARGE_ELEMS(rt, len);
            if (len > 0) {
                if (!table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                for (j = 0; j < len; j++) table->elems[dst_offset + j] = value;
            }
            break;
        }
        case 0x13:
        case 0x14: {
            uint64_t rhs_hi = wasm__slot_u64(WASM__POP(rt));
            uint64_t rhs_lo = wasm__slot_u64(WASM__POP(rt));
            uint64_t lhs_hi = wasm__slot_u64(WASM__POP(rt));
            uint64_t lhs_lo = wasm__slot_u64(WASM__POP(rt));
            uint64_t lo, hi;
            if (subop == 0x13) {
                lo = lhs_lo + rhs_lo;
                hi = lhs_hi + rhs_hi + (lo < lhs_lo ? 1u : 0u);
            } else {
                lo = lhs_lo - rhs_lo;
                hi = lhs_hi - rhs_hi - (lhs_lo < rhs_lo ? 1u : 0u);
            }
            wasm__slot_set_u64(WASM__PUSH(rt), lo);
            wasm__slot_set_u64(WASM__PUSH(rt), hi);
            break;
        }
        case 0x15:
        case 0x16: {
            int64_t rhs = wasm__slot_i64(WASM__POP(rt));
            int64_t lhs = wasm__slot_i64(WASM__POP(rt));
            uint64_t lo, hi;
            wasm__mul_wide_u64((uint64_t)lhs, (uint64_t)rhs, &lo, &hi);
            if (subop == 0x15) {
                if (lhs < 0) hi -= (uint64_t)rhs;
                if (rhs < 0) hi -= (uint64_t)lhs;
            }
            wasm__slot_set_u64(WASM__PUSH(rt), lo);
            wasm__slot_set_u64(WASM__PUSH(rt), hi);
            break;
        }
        default:
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown prefixed opcode 0xFC 0x%X", (unsigned)subop);
            err = WASM_ERR_MALFORMED;
            goto cleanup;
    }

cleanup:
    return err;
}

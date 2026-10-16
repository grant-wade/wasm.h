#include "bulk.h"
#include "../module.h"
#include "state.h"
#include <stdio.h>
#include "../internal/alloc.h"
#include "../memory_internal.h"
#include "../reader.h"
#include "../runtime.h"
#include "../types/references.h"
#include "../values.h"
#include <string.h>

WASM_PRIVATE wasm_error_t wasm__exec_bulk(wasm_module_t* mod, wasm__call_frame_t* cf) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err = WASM_OK;
    uint32_t subop = wasm__read_leb128_u32(&cf->r);
    switch (subop) {
        case 0x00: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__trunc_sat_i32_s((double)a.of.f32));
            break;
        }
        case 0x01: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__trunc_sat_i32_u((double)a.of.f32));
            break;
        }
        case 0x02: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__trunc_sat_i32_s(a.of.f64));
            break;
        }
        case 0x03: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__trunc_sat_i32_u(a.of.f64));
            break;
        }
        case 0x04: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__trunc_sat_i64_s((double)a.of.f32));
            break;
        }
        case 0x05: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__trunc_sat_i64_u((double)a.of.f32));
            break;
        }
        case 0x06: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__trunc_sat_i64_s(a.of.f64));
            break;
        }
        case 0x07: {
            wasm_value_t a = WASM__POP(rt);
            WASM__PUSH(rt, wasm__trunc_sat_i64_u(a.of.f64));
            break;
        }
        case 0x08: {
            uint32_t data_idx = wasm__read_leb128_u32(&cf->r);
            uint32_t mem_idx = wasm__read_leb128_u32(&cf->r);
            wasm_value_t count = WASM__POP(rt);
            wasm_value_t src = WASM__POP(rt);
            wasm_value_t dst = WASM__POP(rt);
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
            len = (uint64_t)(uint32_t)count.of.i32;
            src_offset = (uint64_t)(uint32_t)src.of.i32;
            dst_offset = memory->is_64 ? (uint64_t)dst.of.i64 : (uint64_t)(uint32_t)dst.of.i32;

            segment = &mod->data_segments[data_idx];
            if (!wasm__range_in_bounds_u64(src_offset, len, segment->size) ||
                !wasm__range_in_bounds_u64(dst_offset, len, mem_size))
                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
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
            wasm_value_t count = WASM__POP(rt);
            wasm_value_t src = WASM__POP(rt);
            wasm_value_t dst = WASM__POP(rt);
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
            len = (dst_memory->is_64 && src_memory->is_64)
                      ? (uint64_t)count.of.i64
                      : (uint64_t)(uint32_t)count.of.i32;
            src_offset = src_memory->is_64 ? (uint64_t)src.of.i64
                                           : (uint64_t)(uint32_t)src.of.i32;
            dst_offset = dst_memory->is_64 ? (uint64_t)dst.of.i64
                                           : (uint64_t)(uint32_t)dst.of.i32;
            if (!wasm__range_in_bounds_u64(src_offset, len, src_mem_size) ||
                !wasm__range_in_bounds_u64(dst_offset, len, dst_mem_size))
                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            if (len > 0)
                memmove(dst_memory->data + dst_offset, src_memory->data + src_offset, (size_t)len);
            break;
        }
        case 0x0B: {
            uint32_t mem_idx = wasm__read_leb128_u32(&cf->r);
            wasm_value_t count = WASM__POP(rt);
            wasm_value_t value = WASM__POP(rt);
            wasm_value_t dst = WASM__POP(rt);
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
            len = memory->is_64 ? (uint64_t)count.of.i64 : (uint64_t)(uint32_t)count.of.i32;
            dst_offset = memory->is_64 ? (uint64_t)dst.of.i64 : (uint64_t)(uint32_t)dst.of.i32;
            if (!wasm__range_in_bounds_u64(dst_offset, len, mem_size)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            if (len > 0)
                memset(memory->data + dst_offset, (unsigned char)(value.of.i32 & 0xFF), (size_t)len);
            break;
        }
        case 0x0C: {
            uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);
            uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
            wasm_value_t count = WASM__POP(rt);
            wasm_value_t src = WASM__POP(rt);
            wasm_value_t dst = WASM__POP(rt);
            uint32_t len = (uint32_t)count.of.i32;
            uint32_t src_offset = (uint32_t)src.of.i32;
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
            dst_offset = wasm__table_index_value(table, &dst);
            segment = &mod->elem_segments[elem_idx];
            if (segment->is_declarative) {
                WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "out of bounds table access");
                err = WASM_ERR_TRAP;
                goto cleanup;
            }
            if (!wasm__typed_valtype_is_subtype(mod,
                                                segment->elem_type,
                                                &segment->elem_ref_type,
                                                table->reftype,
                                                &table->reftype_info))
                WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
            if (!wasm__range_in_bounds_u64(src_offset, len, segment->num_elems) ||
                !wasm__range_in_bounds_u64(dst_offset, len, table->size))
                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
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
            wasm_value_t count = WASM__POP(rt);
            wasm_value_t src = WASM__POP(rt);
            wasm_value_t dst = WASM__POP(rt);
            uint64_t len;
            uint64_t src_offset;
            uint64_t dst_offset;
            wasm_table_t* dst_table;
            wasm_table_t* src_table;

            if (dst_table_idx >= mod->num_tables || src_table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
            dst_table = wasm__table_at(mod, dst_table_idx);
            src_table = wasm__table_at(mod, src_table_idx);
            if (!dst_table || !src_table) WASM__TRAP(WASM_ERR_MALFORMED);
            dst_offset = wasm__table_index_value(dst_table, &dst);
            src_offset = wasm__table_index_value(src_table, &src);
            len = (wasm__table_copy_size_type(dst_table, src_table) == WASM_TYPE_I64)
                      ? (uint64_t)count.of.i64
                      : (uint64_t)(uint32_t)count.of.i32;
            if (!wasm__typed_valtype_is_subtype(mod,
                                                src_table->reftype,
                                                &src_table->reftype_info,
                                                dst_table->reftype,
                                                &dst_table->reftype_info))
                WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
            if (!wasm__range_in_bounds_u64(src_offset, len, src_table->size) ||
                !wasm__range_in_bounds_u64(dst_offset, len, dst_table->size))
                WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
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
            wasm_value_t delta_value = WASM__POP(rt);
            wasm_value_t init_value = WASM__POP(rt);
            wasm_table_t* table;
            uint64_t old_size;
            uint64_t delta;
            uint64_t new_size;
            wasm_value_t* new_elems;
            uint64_t j;
            size_t alloc_count;

            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
            table = wasm__table_at(mod, table_idx);
            if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
            if (!wasm__runtime_value_matches_type(mod,
                                                  &init_value,
                                                  table->reftype,
                                                  &table->reftype_info))
                WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
            old_size = table->size;
            delta = wasm__table_index_value(table, &delta_value);
            if (delta == 0) {
                WASM__PUSH(rt, wasm__table_index_result(table, old_size));
                break;
            }
            if ((table->max_size != UINT64_MAX && delta > table->max_size - old_size) ||
                delta > UINT32_MAX - old_size) {
                WASM__PUSH(rt, wasm__table_index_result(table, UINT64_MAX));
                break;
            }
            new_size = old_size + delta;
            if (new_size > (uint64_t)(SIZE_MAX / sizeof(wasm_value_t))) {
                WASM__PUSH(rt, wasm__table_index_result(table, UINT64_MAX));
                break;
            }
            alloc_count = (size_t)new_size;
            new_elems = (wasm_value_t*)WASM_REALLOC(table->elems, alloc_count * sizeof(wasm_value_t));
            if (!new_elems) {
                WASM__PUSH(rt, wasm__table_index_result(table, UINT64_MAX));
                break;
            }
            table->elems = new_elems;
            table->size = (uint32_t)new_size;
            for (j = old_size; j < new_size; j++) table->elems[j] = init_value;
            WASM__PUSH(rt, wasm__table_index_result(table, old_size));
            break;
        }
        case 0x10: {
            uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
            wasm_table_t* table;

            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
            table = wasm__table_at(mod, table_idx);
            if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
            WASM__PUSH(rt, wasm__table_index_result(table, table->size));
            break;
        }
        case 0x11: {
            uint32_t table_idx = wasm__read_leb128_u32(&cf->r);
            wasm_value_t count = WASM__POP(rt);
            wasm_value_t value = WASM__POP(rt);
            wasm_value_t dst = WASM__POP(rt);
            wasm_table_t* table;
            uint64_t len;
            uint64_t dst_offset;
            uint32_t j;

            if (table_idx >= mod->num_tables) WASM__TRAP(WASM_ERR_MALFORMED);
            table = wasm__table_at(mod, table_idx);
            if (!table) WASM__TRAP(WASM_ERR_MALFORMED);
            len = wasm__table_index_value(table, &count);
            dst_offset = wasm__table_index_value(table, &dst);
            if (!wasm__runtime_value_matches_type(mod,
                                                  &value,
                                                  table->reftype,
                                                  &table->reftype_info))
                WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
            if (!wasm__range_in_bounds_u64(dst_offset, len, table->size)) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            if (len > 0) {
                if (!table->elems) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                for (j = 0; j < len; j++) table->elems[dst_offset + j] = value;
            }
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

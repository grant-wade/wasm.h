#include "numeric.h"
#include "../module.h"
#include "../memory_internal.h"
#include "../reader.h"
#include "../types/references.h"
#include "stack.h"

WASM_PRIVATE wasm_error_t wasm__validator_validate_numeric(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     uint8_t op) {
    if (op >= 0x45 && op <= 0x4F) {
        if (op == 0x45) return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_I32);
        return wasm__validator_binary(v, at, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32);
    }
    if (op >= 0x50 && op <= 0x5A) {
        if (op == 0x50) return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_I32);
        return wasm__validator_binary(v, at, WASM_TYPE_I64, WASM_TYPE_I64, WASM_TYPE_I32);
    }
    if (op >= 0x5B && op <= 0x60)
        return wasm__validator_binary(v, at, WASM_TYPE_F32, WASM_TYPE_F32, WASM_TYPE_I32);
    if (op >= 0x61 && op <= 0x66)
        return wasm__validator_binary(v, at, WASM_TYPE_F64, WASM_TYPE_F64, WASM_TYPE_I32);
    if (op >= 0x67 && op <= 0x69)
        return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_I32);
    if (op >= 0x6A && op <= 0x78)
        return wasm__validator_binary(v, at, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32);
    if (op >= 0x79 && op <= 0x7B)
        return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_I64);
    if (op >= 0x7C && op <= 0x8A)
        return wasm__validator_binary(v, at, WASM_TYPE_I64, WASM_TYPE_I64, WASM_TYPE_I64);
    if (op >= 0x8B && op <= 0x91)
        return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_F32);
    if (op >= 0x92 && op <= 0x98)
        return wasm__validator_binary(v, at, WASM_TYPE_F32, WASM_TYPE_F32, WASM_TYPE_F32);
    if (op >= 0x99 && op <= 0x9F)
        return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_F64);
    if (op >= 0xA0 && op <= 0xA6)
        return wasm__validator_binary(v, at, WASM_TYPE_F64, WASM_TYPE_F64, WASM_TYPE_F64);

    switch (op) {
        case 0xA7:
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_I32);
        case 0xA8:
        case 0xA9:
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I32);
        case 0xAA:
        case 0xAB:
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I32);
        case 0xAC:
        case 0xAD:
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_I64);
        case 0xAE:
        case 0xAF:
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I64);
        case 0xB0:
        case 0xB1:
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I64);
        case 0xB2:
        case 0xB3:
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_F32);
        case 0xB4:
        case 0xB5:
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_F32);
        case 0xB6:
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_F32);
        case 0xB7:
        case 0xB8:
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_F64);
        case 0xB9:
        case 0xBA:
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_F64);
        case 0xBB:
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_F64);
        case 0xBC:
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I32);
        case 0xBD:
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I64);
        case 0xBE:
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_F32);
        case 0xBF:
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_F64);
        case 0xC0:
        case 0xC1: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_SIGN_EXT);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_I32, WASM_TYPE_I32);
        }
        case 0xC2:
        case 0xC3:
        case 0xC4: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_SIGN_EXT);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_I64, WASM_TYPE_I64);
        }
        default:
            return wasm__validator_error(v, at, "unknown opcode 0x%02X", op);
    }
}

WASM_PRIVATE wasm_error_t wasm__validator_validate_prefixed(wasm__validator_t* v,
                                                      const uint8_t* at) {
    uint32_t subop = wasm__read_leb128_u32(&v->r);

    switch (subop) {
        case 0x00:
        case 0x01: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_NONTRAPPING_FMA);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I32);
        }
        case 0x02:
        case 0x03: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_NONTRAPPING_FMA);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I32);
        }
        case 0x04:
        case 0x05: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_NONTRAPPING_FMA);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_F32, WASM_TYPE_I64);
        }
        case 0x06:
        case 0x07: {
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_NONTRAPPING_FMA);
            if (err != WASM_OK) return err;
            return wasm__validator_unary(v, at, WASM_TYPE_F64, WASM_TYPE_I64);
        }
        case 0x08: {
            uint32_t data_idx = wasm__read_leb128_u32(&v->r);
            uint32_t mem_idx = wasm__read_leb128_u32(&v->r);
            wasm_valtype_t index_type;
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            v->mod->uses_data_count_section = 1;
            if (mem_idx != 0) {
                err = wasm__validator_require_feature(v, at, WASM_FEATURE_MULTI_MEMORY);
                if (err != WASM_OK) return err;
            }
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "unknown memory %u", (unsigned)mem_idx);
            if (mem_idx >= v->mod->num_memories)
                return wasm__validator_error(v, at, "unknown memory %u", (unsigned)mem_idx);
            if (data_idx >= v->mod->num_data_segments)
                return wasm__validator_error(v, at, "unknown data segment %u", (unsigned)data_idx);
            index_type = wasm__memory_index_type(&v->mod->memories[mem_idx]);
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_expect(v, at, index_type);
        }
        case 0x09: {
            uint32_t data_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            v->mod->uses_data_count_section = 1;
            if (data_idx >= v->mod->num_data_segments)
                return wasm__validator_error(v, at, "unknown data segment %u", (unsigned)data_idx);
            return WASM_OK;
        }
        case 0x0A: {
            uint32_t dst_mem_idx = wasm__read_leb128_u32(&v->r);
            uint32_t src_mem_idx = wasm__read_leb128_u32(&v->r);
            wasm_valtype_t dst_index_type;
            wasm_valtype_t src_index_type;
            wasm_valtype_t size_index_type;
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            if (dst_mem_idx != 0 || src_mem_idx != 0) {
                err = wasm__validator_require_feature(v, at, WASM_FEATURE_MULTI_MEMORY);
                if (err != WASM_OK) return err;
            }
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "unknown memory %u", (unsigned)dst_mem_idx);
            if (dst_mem_idx >= v->mod->num_memories || src_mem_idx >= v->mod->num_memories)
                return wasm__validator_error(v, at,
                                             "unknown memory %u",
                                             (unsigned)(dst_mem_idx >= v->mod->num_memories ? dst_mem_idx : src_mem_idx));
            dst_index_type = wasm__memory_index_type(&v->mod->memories[dst_mem_idx]);
            src_index_type = wasm__memory_index_type(&v->mod->memories[src_mem_idx]);
            size_index_type = (dst_index_type == WASM_TYPE_I32 || src_index_type == WASM_TYPE_I32)
                                  ? WASM_TYPE_I32
                                  : WASM_TYPE_I64;
            err = wasm__validator_pop_expect(v, at, size_index_type);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, src_index_type);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_expect(v, at, dst_index_type);
        }
        case 0x0B: {
            uint32_t mem_idx = wasm__read_leb128_u32(&v->r);
            wasm_valtype_t index_type;
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            if (mem_idx != 0) {
                err = wasm__validator_require_feature(v, at, WASM_FEATURE_MULTI_MEMORY);
                if (err != WASM_OK) return err;
            }
            if (v->mod->num_memories == 0)
                return wasm__validator_error(v, at, "unknown memory %u", (unsigned)mem_idx);
            if (mem_idx >= v->mod->num_memories)
                return wasm__validator_error(v, at, "unknown memory %u", (unsigned)mem_idx);
            index_type = wasm__memory_index_type(&v->mod->memories[mem_idx]);
            err = wasm__validator_pop_expect(v, at, index_type);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_expect(v, at, index_type);
        }
        case 0x0C: {
            uint32_t elem_idx = wasm__read_leb128_u32(&v->r);
            uint32_t table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            wasm_elem_segment_t* segment;
            wasm_table_t* table;
            if (err != WASM_OK) return err;
            if (table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "unknown table %u", (unsigned)table_idx);
            if (elem_idx >= v->mod->num_elem_segments)
                return wasm__validator_error(v, at, "unknown elem segment %u", (unsigned)elem_idx);
            segment = &v->mod->elem_segments[elem_idx];
            table = &v->mod->tables[table_idx];
            if (!wasm__elem_segment_values_match_type(v->mod,
                                                      segment,
                                                      table->reftype,
                                                      &table->reftype_info))
                return wasm__validator_error(v, at, "table.init type mismatch");
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_expect(v, at, wasm__table_index_type(table));
        }
        case 0x0D: {
            uint32_t elem_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            if (err != WASM_OK) return err;
            if (elem_idx >= v->mod->num_elem_segments)
                return wasm__validator_error(v, at, "unknown elem segment %u", (unsigned)elem_idx);
            return WASM_OK;
        }
        case 0x0E: {
            uint32_t dst_table_idx = wasm__read_leb128_u32(&v->r);
            uint32_t src_table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_BULK_MEMORY);
            wasm_table_t* dst_table;
            wasm_table_t* src_table;
            wasm_valtype_t size_type;
            if (err != WASM_OK) return err;
            if (dst_table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "unknown table %u", (unsigned)dst_table_idx);
            if (src_table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "unknown table %u", (unsigned)src_table_idx);
            dst_table = &v->mod->tables[dst_table_idx];
            src_table = &v->mod->tables[src_table_idx];
            if (!wasm__validator_type_matches(v,
                                              src_table->reftype,
                                              &src_table->reftype_info,
                                              dst_table->reftype,
                                              &dst_table->reftype_info))
                return wasm__validator_error(v, at, "table.copy type mismatch");
            size_type = wasm__table_copy_size_type(dst_table, src_table);
            err = wasm__validator_pop_expect(v, at, size_type);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect(v, at, wasm__table_index_type(src_table));
            if (err != WASM_OK) return err;
            return wasm__validator_pop_expect(v, at, wasm__table_index_type(dst_table));
        }
        case 0x0F: {
            uint32_t table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_REFERENCE_TYPES);
            wasm_table_t* table;
            if (err != WASM_OK) return err;
            if (table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "unknown table %u", (unsigned)table_idx);
            table = &v->mod->tables[table_idx];
            err = wasm__validator_pop_expect(v, at, wasm__table_index_type(table));
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect_typed(v, at, table->reftype, &table->reftype_info);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, wasm__table_index_type(table));
        }
        case 0x10: {
            uint32_t table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_REFERENCE_TYPES);
            if (err != WASM_OK) return err;
            if (table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "unknown table %u", (unsigned)table_idx);
            return wasm__validator_push(v, at, wasm__table_index_type(&v->mod->tables[table_idx]));
        }
        case 0x11: {
            uint32_t table_idx = wasm__read_leb128_u32(&v->r);
            wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_REFERENCE_TYPES);
            wasm_table_t* table;
            if (err != WASM_OK) return err;
            if (table_idx >= v->mod->num_tables)
                return wasm__validator_error(v, at, "unknown table %u", (unsigned)table_idx);
            table = &v->mod->tables[table_idx];
            err = wasm__validator_pop_expect(v, at, wasm__table_index_type(table));
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect_typed(v, at, table->reftype, &table->reftype_info);
            if (err != WASM_OK) return err;
            return wasm__validator_pop_expect(v, at, wasm__table_index_type(table));
        }
        default:
            return wasm__validator_error(v, at, "unknown prefixed opcode 0xFC 0x%X", (unsigned)subop);
    }
}

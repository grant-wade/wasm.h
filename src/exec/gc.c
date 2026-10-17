#include "gc.h"
#include "../module.h"
#include "state.h"
#include <stdio.h>
#include "exceptions.h"
#include "../internal/gc_types.h"
#include "../memory_internal.h"
#include "../reader.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include "../types/references.h"
#include "../values.h"

WASM_PRIVATE wasm_error_t wasm__exec_gc(wasm_module_t* mod, wasm__call_frame_t* cf) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err = WASM_OK;
    uint32_t i;
    uint32_t subop = wasm__read_leb128_u32(&cf->r);

    switch (subop) {
        case 0x00:
        case 0x01: {
            uint32_t type_idx = wasm__read_leb128_u32(&cf->r);
            const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
            wasm_gc_struct_t* object;

            if (!st) WASM__TRAP(WASM_ERR_MALFORMED);
            object = wasm__gc_alloc_struct_object(rt, mod, type_idx, st);
            if (!object) WASM__TRAP(WASM_ERR_OOM);

            if (subop == 0x00) {
                for (i = st->num_fields; i > 0; i--) {
                    err = wasm__gc_struct_set_field(object, st, i - 1, WASM__POP(rt));
                    if (err != WASM_OK) goto cleanup;
                }
            } else {
                for (i = 0; i < st->num_fields; i++) {
                    err = wasm__gc_struct_set_field(object, st, i,
                                                    wasm__gc_field_default_value(&st->fields[i]));
                    if (err != WASM_OK) goto cleanup;
                }
            }

            WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
            break;
        }
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05: {
            uint32_t type_idx = wasm__read_leb128_u32(&cf->r);
            uint32_t field_idx = wasm__read_leb128_u32(&cf->r);
            const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
            wasm_reftype_t struct_ref;
            wasm_value_t ref_value;
            const wasm_gc_header_t* header;
            wasm_gc_struct_t* object;

            if (!st || field_idx >= st->num_fields) WASM__TRAP(WASM_ERR_MALFORMED);
            err = wasm__runtime_make_typeidx_reftype(mod, type_idx, 1, &struct_ref);
            if (err != WASM_OK) WASM__TRAP(WASM_ERR_MALFORMED);

            if (subop == 0x05) {
                wasm_value_t stored = WASM__POP(rt);

                ref_value = WASM__POP(rt);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, struct_ref.type, &struct_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_STRUCT) WASM__TRAP(WASM_ERR_TRAP);
                object = (wasm_gc_struct_t*)header;
                err = wasm__gc_struct_set_field(object, st, field_idx, stored);
                if (err != WASM_OK) goto cleanup;
                break;
            }

            ref_value = WASM__POP(rt);
            if (!wasm__runtime_value_matches_type(mod, &ref_value, struct_ref.type, &struct_ref))
                WASM__TRAP(WASM_ERR_TRAP);
            header = wasm__runtime_get_gc_object(mod, &ref_value);
            if (!header || header->kind != WASM_GC_OBJ_STRUCT) WASM__TRAP(WASM_ERR_TRAP);
            object = (wasm_gc_struct_t*)header;
            err = wasm__gc_struct_get_field(object,
                                            st,
                                            field_idx,
                                            subop == 0x03,
                                            &ref_value);
            if (err != WASM_OK) goto cleanup;
            WASM__PUSH(rt, ref_value);
            break;
        }
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A: {
            uint32_t type_idx = wasm__read_leb128_u32(&cf->r);
            const wasm_arraytype_t* atype = wasm__module_const_arraytype(mod, type_idx);
            wasm_gc_array_t* object;
            uint32_t count;

            if (!atype) WASM__TRAP(WASM_ERR_MALFORMED);

            if (subop == 0x08) {
                count = wasm__read_leb128_u32(&cf->r);
                object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                if (!object) WASM__TRAP(WASM_ERR_OOM);
                for (i = count; i > 0; i--) {
                    err = wasm__gc_array_set(object, atype, i - 1, WASM__POP(rt));
                    if (err != WASM_OK) goto cleanup;
                }
                WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                break;
            }

            if (subop == 0x06 || subop == 0x07) {
                wasm_value_t length_value;

                if (subop == 0x06) {
                    wasm_value_t init_value;

                    length_value = WASM__POP(rt);
                    init_value = WASM__POP(rt);
                    if (length_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    count = (uint32_t)length_value.of.i32;
                    object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                    if (!object) WASM__TRAP(WASM_ERR_OOM);
                    for (i = 0; i < count; i++) {
                        err = wasm__gc_array_set(object, atype, i, init_value);
                        if (err != WASM_OK) goto cleanup;
                    }
                } else {
                    length_value = WASM__POP(rt);
                    if (length_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                    count = (uint32_t)length_value.of.i32;
                    object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                    if (!object) WASM__TRAP(WASM_ERR_OOM);
                    for (i = 0; i < count; i++) {
                        err = wasm__gc_array_set(object, atype, i,
                                                 wasm__gc_field_default_value(&atype->field));
                        if (err != WASM_OK) goto cleanup;
                    }
                }

                WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                break;
            }

            if (subop == 0x09) {
                uint32_t data_idx = wasm__read_leb128_u32(&cf->r);
                wasm_value_t size_value = WASM__POP(rt);
                wasm_value_t offset_value = WASM__POP(rt);
                wasm_data_segment_t* segment;
                size_t elem_size;
                uint64_t byte_count;
                uint64_t src_offset;

                if (data_idx >= mod->num_data_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                if (size_value.of.i32 < 0 || offset_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__gc_array_data_element_size(atype, &elem_size)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                count = (uint32_t)size_value.of.i32;
                segment = &mod->data_segments[data_idx];
                if (!segment->is_passive) WASM__TRAP(WASM_ERR_TRAP);
                if (count != 0 && elem_size > UINT64_MAX / count) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                byte_count = (uint64_t)elem_size * (uint64_t)count;
                src_offset = (uint64_t)(uint32_t)offset_value.of.i32;
                if (!wasm__range_in_bounds_u64(src_offset, byte_count, segment->size))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                if (!object) WASM__TRAP(WASM_ERR_OOM);
                for (i = 0; i < count; i++) {
                    wasm_value_t elem_value;

                    err = wasm__gc_array_read_data_value(
                        atype,
                        segment->bytes + src_offset + ((uint64_t)i * (uint64_t)elem_size),
                        &elem_value);
                    if (err != WASM_OK) goto cleanup;
                    err = wasm__gc_array_set(object, atype, i, elem_value);
                    if (err != WASM_OK) goto cleanup;
                }

                WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                break;
            }

            {
                uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);
                wasm_value_t size_value = WASM__POP(rt);
                wasm_value_t offset_value = WASM__POP(rt);
                wasm_elem_segment_t* segment;
                uint32_t src_offset;

                if (elem_idx >= mod->num_elem_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                if (size_value.of.i32 < 0 || offset_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                count = (uint32_t)size_value.of.i32;
                src_offset = (uint32_t)offset_value.of.i32;
                segment = &mod->elem_segments[elem_idx];
                if (!segment->is_passive) WASM__TRAP(WASM_ERR_TRAP);
                if (!wasm__range_in_bounds_u64(src_offset, count, segment->num_elems))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                if (!object) WASM__TRAP(WASM_ERR_OOM);
                for (i = 0; i < count; i++) {
                    err = wasm__gc_array_set(object, atype, i, segment->elems[src_offset + i]);
                    if (err != WASM_OK) goto cleanup;
                }

                WASM__PUSH(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                break;
            }
        }
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13: {
            uint32_t type_idx = subop == 0x0F ? UINT32_MAX : wasm__read_leb128_u32(&cf->r);
            const wasm_arraytype_t* atype = subop == 0x0F ? NULL : wasm__module_const_arraytype(mod, type_idx);
            wasm_reftype_t array_ref;
            wasm_value_t ref_value;
            const wasm_gc_header_t* header;
            wasm_gc_array_t* object;

            if (subop != 0x0F && !atype) WASM__TRAP(WASM_ERR_MALFORMED);
            wasm__clear_reftype(&array_ref);
            array_ref.type = WASM_TYPE_ARRAYREF;
            array_ref.nullable = 1;
            if (subop != 0x0F) {
                err = wasm__runtime_make_typeidx_reftype(mod, type_idx, 1, &array_ref);
                if (err != WASM_OK) WASM__TRAP(WASM_ERR_MALFORMED);
            }

            if (subop == 0x0B || subop == 0x0C || subop == 0x0D) {
                wasm_value_t index_value = WASM__POP(rt);

                ref_value = WASM__POP(rt);
                if (index_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                object = (wasm_gc_array_t*)header;
                err = wasm__gc_array_get(object,
                                         atype,
                                         (uint32_t)index_value.of.i32,
                                         subop == 0x0C,
                                         &ref_value);
                if (err != WASM_OK) goto cleanup;
                WASM__PUSH(rt, ref_value);
                break;
            }

            if (subop == 0x0E) {
                wasm_value_t stored = WASM__POP(rt);
                wasm_value_t index_value = WASM__POP(rt);

                ref_value = WASM__POP(rt);
                if (index_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                object = (wasm_gc_array_t*)header;
                err = wasm__gc_array_set(object, atype, (uint32_t)index_value.of.i32, stored);
                if (err != WASM_OK) goto cleanup;
                break;
            }

            if (subop == 0x0F) {
                ref_value = WASM__POP(rt);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                WASM__PUSH(rt, wasm_i32((int32_t)wasm__gc_array_len((const wasm_gc_array_t*)header)));
                break;
            }

            if (subop == 0x10) {
                wasm_value_t count_value = WASM__POP(rt);
                wasm_value_t fill_value = WASM__POP(rt);
                wasm_value_t dst_value = WASM__POP(rt);
                uint32_t count;
                uint32_t dst_index;

                ref_value = WASM__POP(rt);
                if (count_value.of.i32 < 0 || dst_value.of.i32 < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                object = (wasm_gc_array_t*)header;
                count = (uint32_t)count_value.of.i32;
                dst_index = (uint32_t)dst_value.of.i32;
                if (!wasm__range_in_bounds_u64(dst_index, count, object->length))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                for (i = 0; i < count; i++) {
                    err = wasm__gc_array_set(object, atype, dst_index + i, fill_value);
                    if (err != WASM_OK) goto cleanup;
                }
                break;
            }

            if (subop == 0x11) {
                uint32_t src_type_idx = wasm__read_leb128_u32(&cf->r);
                const wasm_arraytype_t* src_type = wasm__module_const_arraytype(mod, src_type_idx);
                wasm_reftype_t src_ref;
                wasm_value_t count_value = WASM__POP(rt);
                wasm_value_t src_offset_value = WASM__POP(rt);
                wasm_value_t src_value = WASM__POP(rt);
                wasm_value_t dst_offset_value = WASM__POP(rt);
                uint32_t count;
                uint32_t src_index;
                uint32_t dst_index;
                const wasm_gc_header_t* src_header;
                const wasm_gc_array_t* src_object;

                ref_value = WASM__POP(rt);
                if (!src_type) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__runtime_make_typeidx_reftype(mod, src_type_idx, 1, &src_ref);
                if (err != WASM_OK) WASM__TRAP(WASM_ERR_MALFORMED);
                if (count_value.of.i32 < 0 || src_offset_value.of.i32 < 0 || dst_offset_value.of.i32 < 0)
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref) ||
                    !wasm__runtime_value_matches_type(mod, &src_value, src_ref.type, &src_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                src_header = wasm__runtime_get_gc_object(mod, &src_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY ||
                    !src_header || src_header->kind != WASM_GC_OBJ_ARRAY)
                    WASM__TRAP(WASM_ERR_TRAP);

                object = (wasm_gc_array_t*)header;
                src_object = (const wasm_gc_array_t*)src_header;
                count = (uint32_t)count_value.of.i32;
                src_index = (uint32_t)src_offset_value.of.i32;
                dst_index = (uint32_t)dst_offset_value.of.i32;
                if (!wasm__range_in_bounds_u64(src_index, count, src_object->length) ||
                    !wasm__range_in_bounds_u64(dst_index, count, object->length))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                if (count > 0 && object == src_object && dst_index > src_index) {
                    for (i = count; i > 0; i--) {
                        wasm_value_t copied;

                        err = wasm__gc_array_get(src_object, src_type, src_index + i - 1, 0, &copied);
                        if (err != WASM_OK) goto cleanup;
                        err = wasm__gc_array_set(object, atype, dst_index + i - 1, copied);
                        if (err != WASM_OK) goto cleanup;
                    }
                } else {
                    for (i = 0; i < count; i++) {
                        wasm_value_t copied;

                        err = wasm__gc_array_get(src_object, src_type, src_index + i, 0, &copied);
                        if (err != WASM_OK) goto cleanup;
                        err = wasm__gc_array_set(object, atype, dst_index + i, copied);
                        if (err != WASM_OK) goto cleanup;
                    }
                }
                break;
            }

            if (subop == 0x12) {
                uint32_t data_idx = wasm__read_leb128_u32(&cf->r);
                wasm_value_t count_value = WASM__POP(rt);
                wasm_value_t src_offset_value = WASM__POP(rt);
                wasm_value_t dst_offset_value = WASM__POP(rt);
                wasm_data_segment_t* segment;
                size_t elem_size;
                uint64_t byte_count;
                uint64_t src_offset;
                uint32_t dst_index;
                uint32_t count;

                ref_value = WASM__POP(rt);
                if (data_idx >= mod->num_data_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                if (count_value.of.i32 < 0 || src_offset_value.of.i32 < 0 || dst_offset_value.of.i32 < 0)
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                object = (wasm_gc_array_t*)header;
                segment = &mod->data_segments[data_idx];
                if (!segment->is_passive) WASM__TRAP(WASM_ERR_TRAP);
                if (!wasm__gc_array_data_element_size(atype, &elem_size)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);

                count = (uint32_t)count_value.of.i32;
                src_offset = (uint64_t)(uint32_t)src_offset_value.of.i32;
                dst_index = (uint32_t)dst_offset_value.of.i32;
                if (count != 0 && elem_size > UINT64_MAX / count) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                byte_count = (uint64_t)elem_size * (uint64_t)count;
                if (!wasm__range_in_bounds_u64(src_offset, byte_count, segment->size) ||
                    !wasm__range_in_bounds_u64(dst_index, count, object->length))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                for (i = 0; i < count; i++) {
                    wasm_value_t elem_value;

                    err = wasm__gc_array_read_data_value(
                        atype,
                        segment->bytes + src_offset + ((uint64_t)i * (uint64_t)elem_size),
                        &elem_value);
                    if (err != WASM_OK) goto cleanup;
                    err = wasm__gc_array_set(object, atype, dst_index + i, elem_value);
                    if (err != WASM_OK) goto cleanup;
                }
                break;
            }

            {
                uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);
                wasm_value_t count_value = WASM__POP(rt);
                wasm_value_t src_offset_value = WASM__POP(rt);
                wasm_value_t dst_offset_value = WASM__POP(rt);
                wasm_elem_segment_t* segment;
                uint32_t count;
                uint32_t src_index;
                uint32_t dst_index;

                ref_value = WASM__POP(rt);
                if (elem_idx >= mod->num_elem_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                if (count_value.of.i32 < 0 || src_offset_value.of.i32 < 0 || dst_offset_value.of.i32 < 0)
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                object = (wasm_gc_array_t*)header;
                segment = &mod->elem_segments[elem_idx];
                if (!segment->is_passive) WASM__TRAP(WASM_ERR_TRAP);
                count = (uint32_t)count_value.of.i32;
                src_index = (uint32_t)src_offset_value.of.i32;
                dst_index = (uint32_t)dst_offset_value.of.i32;
                if (!wasm__range_in_bounds_u64(src_index, count, segment->num_elems) ||
                    !wasm__range_in_bounds_u64(dst_index, count, object->length))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);

                for (i = 0; i < count; i++) {
                    err = wasm__gc_array_set(object, atype, dst_index + i, segment->elems[src_index + i]);
                    if (err != WASM_OK) goto cleanup;
                }
                break;
            }
        }
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17: {
            wasm_reftype_t target_ref;
            wasm_value_t value = WASM__POP(rt);
            int matches;

            err = wasm__runtime_read_heap_reftype(mod,
                                                  &cf->r,
                                                  subop == 0x15 || subop == 0x17,
                                                  &target_ref);
            if (err != WASM_OK) goto cleanup;
            matches = wasm__runtime_value_matches_type(mod, &value, target_ref.type, &target_ref);
            if (subop == 0x14 || subop == 0x15) {
                WASM__PUSH(rt, wasm_i32(matches ? 1 : 0));
                break;
            }
            if (!matches) {
                WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", "cast failure");
                err = WASM_ERR_TRAP;
                goto cleanup;
            }
            WASM__PUSH(rt, wasm__runtime_cast_success_value(&target_ref, value));
            break;
        }
        case 0x18:
        case 0x19: {
            uint8_t flags = wasm__read_u8(&cf->r);
            uint32_t depth = wasm__read_leb128_u32(&cf->r);
            wasm_reftype_t source_ref;
            wasm_reftype_t target_ref;
            wasm_value_t value;
            int matches;

            if ((flags & ~0x03u) != 0 || depth >= cf->lsp) WASM__TRAP(WASM_ERR_MALFORMED);
            err = wasm__runtime_read_heap_reftype(mod, &cf->r, (flags & 0x01u) != 0, &source_ref);
            if (err != WASM_OK) goto cleanup;
            err = wasm__runtime_read_heap_reftype(mod, &cf->r, (flags & 0x02u) != 0, &target_ref);
            if (err != WASM_OK) goto cleanup;

            value = WASM__PEEK(rt);
            matches = wasm__runtime_value_matches_type(mod, &value, target_ref.type, &target_ref);
            if (matches) {
                rt->stack[rt->sp - 1] = wasm__runtime_cast_success_value(&target_ref, value);
                if (subop == 0x18) {
                    err = wasm__do_branch(rt,
                                          cf->labels,
                                          &cf->labels[cf->lsp - 1 - depth],
                                          &cf->r,
                                          &cf->lsp,
                                          depth);
                    if (err != WASM_OK) goto cleanup;
                }
            } else {
                rt->stack[rt->sp - 1] = wasm__runtime_cast_difference_value(&source_ref,
                                                                            &target_ref,
                                                                            value);
                if (subop == 0x19) {
                    err = wasm__do_branch(rt,
                                          cf->labels,
                                          &cf->labels[cf->lsp - 1 - depth],
                                          &cf->r,
                                          &cf->lsp,
                                          depth);
                    if (err != WASM_OK) goto cleanup;
                }
            }
            break;
        }
        case 0x1A:
        case 0x1B: {
            wasm_value_t value = WASM__POP(rt);

            if (subop == 0x1A) {
                WASM__PUSH(rt, wasm__anyref_from_externref_value(rt, value));
            } else {
                wasm_value_t extern_value;

                err = wasm__externref_from_anyref_value(rt, mod, value, &extern_value);
                if (err != WASM_OK) goto cleanup;
                WASM__PUSH(rt, extern_value);
            }
            break;
        }
        case 0x1C: {
            wasm_value_t value = WASM__POP(rt);

            WASM__PUSH(rt, wasm_i31ref(value.of.i32));
            break;
        }
        case 0x1D:
        case 0x1E: {
            wasm_value_t value = WASM__POP(rt);

            if (wasm__is_null_ref(&value)) WASM__TRAP(WASM_ERR_TRAP);
            if ((value.of.gc_ref & WASM__GC_I31_TAG) == 0) WASM__TRAP(WASM_ERR_TRAP);
            WASM__PUSH(rt, subop == 0x1D ? wasm_i32(wasm__gc_i31_get_s(value.of.gc_ref))
                                         : wasm_i32((int32_t)wasm__gc_i31_get_u(value.of.gc_ref)));
            break;
        }
        default:
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown GC opcode 0xFB 0x%X", (unsigned)subop);
            err = WASM_ERR_MALFORMED;
            goto cleanup;
    }

cleanup:
    return err;
}

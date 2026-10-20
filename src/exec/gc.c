#include "gc.h"
#include "../module.h"
#include "state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exceptions.h"
#include "slots.h"
#include "translate.h"
#include "../internal/field.h"
#include "../internal/gc_types.h"
#include "../memory_internal.h"
#include "../portable.h"
#include "../reader.h"
#include "../runtime.h"
#include "../types/canonical.h"
#include "../types/references.h"
#include "../values.h"

#define WASM__PUSH_REF(rt, value)                    \
    do {                                             \
        wasm_value_t wasm__pushed = (value);         \
        wasm__push_value((rt), &wasm__pushed);       \
    } while (0)

static wasm_error_t wasm__exec_gc_trap(wasm_runtime_t* rt, const char* message) {
    WASM__SET_ERR(rt, WASM_ERR_TRAP, "%s", message);
    return WASM_ERR_TRAP;
}

/* Copies count elements of elem_size bytes from little-endian data at src into array elements at dst. */
static void wasm__gc_copy_data_elems(uint8_t* dst, const uint8_t* src, size_t elem_size, uint32_t count) {
#if WASM__BIG_ENDIAN
    size_t i;
    size_t b;

    if (count == 0) return;
    if (elem_size == 1u || elem_size == 16u) {
        memcpy(dst, src, elem_size * (size_t)count);
        return;
    }
    for (i = 0; i < (size_t)count; i++) {
        for (b = 0; b < elem_size; b++) dst[i * elem_size + b] = src[i * elem_size + elem_size - 1u - b];
    }
#else
    if (count > 0) memcpy(dst, src, elem_size * (size_t)count);
#endif
}

/* Stores count values of an element segment at src into array elements at dst. */
static void wasm__gc_copy_elem_values(uint8_t* dst, const wasm_arraytype_t* at, const wasm_value_t* src,
                                      uint32_t count) {
    uint32_t code = wasm__field_code(&at->field);
    uint32_t i;

    for (i = 0; i < count; i++) wasm__field_store_value(dst + (size_t)i * at->elem_size, code, src[i]);
}

WASM_PRIVATE wasm_gc_header_t* wasm__gc_slot_descriptor(const wasm_runtime_t* rt, const wasm__slot_t* slot) {
    uintptr_t bits = wasm__slot_ref(slot);
    wasm_gc_header_t* object = (wasm_gc_header_t*)bits;
    wasm_gc_header_t** descriptor;

    if ((bits & WASM__GC_I31_TAG) != 0 || !wasm__gc_ptr_in_heap(rt, object) || object->kind != WASM_GC_OBJ_STRUCT)
        return NULL;
    descriptor = wasm__gc_descriptor_slot(object);
    return descriptor ? *descriptor : NULL;
}

/* Executes the custom descriptors instruction subop, 0x20 through 0x22, reading its immediates from cf. */
static WASM__NOINLINE wasm_error_t wasm__exec_descriptor_op(wasm_module_t* mod, wasm__call_frame_t* cf,
                                                           uint32_t subop) {
    wasm_runtime_t* rt = mod->rt;
    wasm_gc_header_t* descriptor;
    wasm_error_t err;
    uint32_t i;

    switch (subop) {
        case 0x20:
        case 0x21: {
            uint32_t type_idx = wasm__read_leb128_u32(&cf->r);
            const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
            wasm_gc_struct_t* object;

            if (!st) return WASM_ERR_MALFORMED;
            if (wasm__slot_u64(WASM__PEEK(rt)) == 0) return wasm__exec_gc_trap(rt, "null descriptor reference");
            object = wasm__gc_alloc_struct_object(rt, mod, type_idx, st);
            if (!object) return WASM_ERR_OOM;
            *wasm__gc_descriptor_slot(&object->header) = (wasm_gc_header_t*)wasm__slot_ref(WASM__POP(rt));
            for (i = st->num_fields; subop == 0x20 && i > 0; i--) {
                err = wasm__gc_struct_set_field(object, st, i - 1, wasm__pop_field_value(rt, &st->fields[i - 1]));
                if (err != WASM_OK) return err;
            }
            WASM__PUSH_REF(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
            return WASM_OK;
        }
        default:
            if (wasm__slot_u64(WASM__PEEK(rt)) == 0) return wasm__exec_gc_trap(rt, "null reference");
            descriptor = *wasm__gc_descriptor_slot((wasm_gc_header_t*)wasm__slot_ref(WASM__PEEK(rt)));
            wasm__slot_set_ref(WASM__PEEK(rt), (uintptr_t)descriptor);
            return WASM_OK;
    }
}

WASM_PRIVATE wasm_error_t wasm__exec_gc(wasm_module_t* mod, wasm__call_frame_t* cf) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err = WASM_OK;
    uint32_t subop = wasm__read_leb128_u32(&cf->r);

    switch (subop) {
        case 0x09:
        case 0x0A: {
            uint32_t type_idx = wasm__read_leb128_u32(&cf->r);
            const wasm_arraytype_t* atype = wasm__module_const_arraytype(mod, type_idx);
            wasm_gc_array_t* object;
            uint32_t count;

            if (!atype) WASM__TRAP(WASM_ERR_MALFORMED);

            if (subop == 0x09) {
                uint32_t data_idx = wasm__read_leb128_u32(&cf->r);
                int32_t size_value = wasm__slot_i32(WASM__POP(rt));
                int32_t offset_value = wasm__slot_i32(WASM__POP(rt));
                wasm_data_segment_t* segment;
                size_t elem_size;
                uint64_t byte_count;
                uint64_t src_offset;

                if (data_idx >= mod->num_data_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                if (size_value < 0 || offset_value < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__gc_array_data_element_size(atype, &elem_size)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);
                count = (uint32_t)size_value;
                segment = &mod->data_segments[data_idx];
                if (!segment->is_passive) WASM__TRAP(WASM_ERR_TRAP);
                if (count != 0 && elem_size > UINT64_MAX / count) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                byte_count = (uint64_t)elem_size * (uint64_t)count;
                src_offset = (uint64_t)(uint32_t)offset_value;
                if (!wasm__range_in_bounds_u64(src_offset, byte_count, segment->size))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__CHARGE_ELEMS(rt, count);

                object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                if (!object) WASM__TRAP(WASM_ERR_OOM);
                wasm__gc_copy_data_elems((uint8_t*)object + atype->data_offset, segment->bytes + src_offset, elem_size,
                                         count);

                WASM__PUSH_REF(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                break;
            }

            {
                uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);
                int32_t size_value = wasm__slot_i32(WASM__POP(rt));
                int32_t offset_value = wasm__slot_i32(WASM__POP(rt));
                wasm_elem_segment_t* segment;
                uint32_t src_offset;

                if (elem_idx >= mod->num_elem_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                if (size_value < 0 || offset_value < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                count = (uint32_t)size_value;
                src_offset = (uint32_t)offset_value;
                segment = &mod->elem_segments[elem_idx];
                if (!segment->is_passive) WASM__TRAP(WASM_ERR_TRAP);
                if (!wasm__range_in_bounds_u64(src_offset, count, segment->num_elems))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__CHARGE_ELEMS(rt, count);

                object = wasm__gc_alloc_array_object(rt, mod, type_idx, atype, count);
                if (!object) WASM__TRAP(WASM_ERR_OOM);
                wasm__gc_copy_elem_values((uint8_t*)object + atype->data_offset, atype, segment->elems + src_offset,
                                          count);

                WASM__PUSH_REF(rt, wasm__gc_type_ref_value(mod, type_idx, &object->header));
                break;
            }
        }
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13: {
            uint32_t type_idx = wasm__read_leb128_u32(&cf->r);
            const wasm_arraytype_t* atype = wasm__module_const_arraytype(mod, type_idx);
            wasm_reftype_t array_ref;
            wasm_value_t ref_value;
            const wasm_gc_header_t* header;
            wasm_gc_array_t* object;

            if (!atype) WASM__TRAP(WASM_ERR_MALFORMED);
            err = wasm__runtime_make_typeidx_reftype(mod, type_idx, 1, &array_ref);
            if (err != WASM_OK) WASM__TRAP(WASM_ERR_MALFORMED);

            if (subop == 0x10) {
                uint32_t code = wasm__field_code(&atype->field);
                int32_t count_value = wasm__slot_i32(WASM__POP(rt));
                const wasm__slot_t* fill_value;
                int32_t dst_value;
                uint32_t count;
                uint32_t dst_index;
                uint8_t* dst;

                rt->exec->sp -= wasm__field_slots(code);
                fill_value = &rt->exec->stack[rt->exec->sp];
                dst_value = wasm__slot_i32(WASM__POP(rt));
                ref_value = wasm__pop_value(rt, WASM_TYPE_ANYREF, NULL);
                if (count_value < 0 || dst_value < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                object = (wasm_gc_array_t*)header;
                count = (uint32_t)count_value;
                dst_index = (uint32_t)dst_value;
                if (!wasm__range_in_bounds_u64(dst_index, count, object->length))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__CHARGE_ELEMS(rt, count);
                if (count > 0) {
                    dst = (uint8_t*)object + atype->data_offset + (size_t)dst_index * atype->elem_size;
                    wasm__field_store(dst, code, fill_value);
                    wasm__field_fill(dst, atype->elem_size, count);
                }
                break;
            }

            if (subop == 0x11) {
                uint32_t src_type_idx = wasm__read_leb128_u32(&cf->r);
                const wasm_arraytype_t* src_type = wasm__module_const_arraytype(mod, src_type_idx);
                wasm_reftype_t src_ref;
                int32_t count_value = wasm__slot_i32(WASM__POP(rt));
                int32_t src_offset_value = wasm__slot_i32(WASM__POP(rt));
                wasm_value_t src_value = wasm__pop_value(rt, WASM_TYPE_ANYREF, NULL);
                int32_t dst_offset_value = wasm__slot_i32(WASM__POP(rt));
                uint32_t count;
                uint32_t src_index;
                uint32_t dst_index;
                const wasm_gc_header_t* src_header;
                const wasm_gc_array_t* src_object;

                ref_value = wasm__pop_value(rt, WASM_TYPE_ANYREF, NULL);
                if (!src_type) WASM__TRAP(WASM_ERR_MALFORMED);
                err = wasm__runtime_make_typeidx_reftype(mod, src_type_idx, 1, &src_ref);
                if (err != WASM_OK) WASM__TRAP(WASM_ERR_MALFORMED);
                if (count_value < 0 || src_offset_value < 0 || dst_offset_value < 0)
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
                count = (uint32_t)count_value;
                src_index = (uint32_t)src_offset_value;
                dst_index = (uint32_t)dst_offset_value;
                if (!wasm__range_in_bounds_u64(src_index, count, src_object->length) ||
                    !wasm__range_in_bounds_u64(dst_index, count, object->length))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__CHARGE_ELEMS(rt, count);
                if (count > 0)
                    memmove((uint8_t*)object + atype->data_offset + (size_t)dst_index * atype->elem_size,
                            (const uint8_t*)src_object + src_type->data_offset + (size_t)src_index * src_type->elem_size,
                            (size_t)count * atype->elem_size);
                break;
            }

            if (subop == 0x12) {
                uint32_t data_idx = wasm__read_leb128_u32(&cf->r);
                int32_t count_value = wasm__slot_i32(WASM__POP(rt));
                int32_t src_offset_value = wasm__slot_i32(WASM__POP(rt));
                int32_t dst_offset_value = wasm__slot_i32(WASM__POP(rt));
                wasm_data_segment_t* segment;
                size_t elem_size;
                uint64_t byte_count;
                uint64_t src_offset;
                uint32_t dst_index;
                uint32_t count;

                ref_value = wasm__pop_value(rt, WASM_TYPE_ANYREF, NULL);
                if (data_idx >= mod->num_data_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                if (count_value < 0 || src_offset_value < 0 || dst_offset_value < 0)
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                object = (wasm_gc_array_t*)header;
                segment = &mod->data_segments[data_idx];
                if (!segment->is_passive) WASM__TRAP(WASM_ERR_TRAP);
                if (!wasm__gc_array_data_element_size(atype, &elem_size)) WASM__TRAP(WASM_ERR_TYPE_MISMATCH);

                count = (uint32_t)count_value;
                src_offset = (uint64_t)(uint32_t)src_offset_value;
                dst_index = (uint32_t)dst_offset_value;
                if (count != 0 && elem_size > UINT64_MAX / count) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                byte_count = (uint64_t)elem_size * (uint64_t)count;
                if (!wasm__range_in_bounds_u64(src_offset, byte_count, segment->size) ||
                    !wasm__range_in_bounds_u64(dst_index, count, object->length))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__CHARGE_ELEMS(rt, count);
                wasm__gc_copy_data_elems((uint8_t*)object + atype->data_offset + (size_t)dst_index * elem_size,
                                         segment->bytes + src_offset, elem_size, count);
                break;
            }

            {
                uint32_t elem_idx = wasm__read_leb128_u32(&cf->r);
                int32_t count_value = wasm__slot_i32(WASM__POP(rt));
                int32_t src_offset_value = wasm__slot_i32(WASM__POP(rt));
                int32_t dst_offset_value = wasm__slot_i32(WASM__POP(rt));
                wasm_elem_segment_t* segment;
                uint32_t count;
                uint32_t src_index;
                uint32_t dst_index;

                ref_value = wasm__pop_value(rt, WASM_TYPE_ANYREF, NULL);
                if (elem_idx >= mod->num_elem_segments) WASM__TRAP(WASM_ERR_MALFORMED);
                if (count_value < 0 || src_offset_value < 0 || dst_offset_value < 0)
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                if (!wasm__runtime_value_matches_type(mod, &ref_value, array_ref.type, &array_ref))
                    WASM__TRAP(WASM_ERR_TRAP);
                header = wasm__runtime_get_gc_object(mod, &ref_value);
                if (!header || header->kind != WASM_GC_OBJ_ARRAY) WASM__TRAP(WASM_ERR_TRAP);
                object = (wasm_gc_array_t*)header;
                segment = &mod->elem_segments[elem_idx];
                if (!segment->is_passive) WASM__TRAP(WASM_ERR_TRAP);
                count = (uint32_t)count_value;
                src_index = (uint32_t)src_offset_value;
                dst_index = (uint32_t)dst_offset_value;
                if (!wasm__range_in_bounds_u64(src_index, count, segment->num_elems) ||
                    !wasm__range_in_bounds_u64(dst_index, count, object->length))
                    WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
                WASM__CHARGE_ELEMS(rt, count);
                wasm__gc_copy_elem_values((uint8_t*)object + atype->data_offset + (size_t)dst_index * atype->elem_size,
                                          atype, segment->elems + src_index, count);
                break;
            }
        }
        case 0x1A:
        case 0x1B: {
            wasm_value_t value;
            wasm_value_t converted;

            /* Collect while the operand is still on the stack, where the GC can see it. */
            wasm__gc_collect_handles_if_needed(rt);
            value = wasm__pop_value(rt, subop == 0x1A ? WASM_TYPE_EXTERNREF : WASM_TYPE_ANYREF, NULL);

            if (subop == 0x1A)
                err = wasm__anyref_from_externref_value(rt, value, &converted);
            else
                err = wasm__externref_from_anyref_value(rt, value, &converted);
            if (err != WASM_OK) goto cleanup;
            wasm__push_value(rt, &converted);
            break;
        }
        case 0x20:
        case 0x21:
        case 0x22:
            err = wasm__exec_descriptor_op(mod, cf, subop);
            goto cleanup;
        default:
            WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "unknown GC opcode 0xFB 0x%X", (unsigned)subop);
            err = WASM_ERR_MALFORMED;
            goto cleanup;
    }

cleanup:
    return err;
}

#undef WASM__PUSH_REF

WASM_PRIVATE wasm_error_t wasm__exec_gc_new(wasm_module_t* mod, uint32_t subop, uint32_t type_idx, uint32_t count) {
    wasm_runtime_t* rt = mod->rt;
    wasm__exec_stack_t* exec = rt->exec;
    wasm_error_t err = WASM_OK;
    uint32_t i;

    if (subop <= 0x01) {
        const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
        wasm_gc_struct_t* object;

        if (!st) WASM__TRAP(WASM_ERR_MALFORMED);
        object = wasm__gc_alloc_struct_object(rt, mod, type_idx, st);
        if (!object) WASM__TRAP(WASM_ERR_OOM);
        for (i = st->num_fields; subop == 0x00 && i > 0; i--) {
            uint32_t code = wasm__field_code(&st->fields[i - 1u]);

            exec->sp -= wasm__field_slots(code);
            wasm__field_store((uint8_t*)object + st->field_offsets[i - 1u], code, &exec->stack[exec->sp]);
        }
        wasm__slot_set_ref(&exec->stack[exec->sp++], (uintptr_t)object);
    } else {
        const wasm_arraytype_t* at = wasm__module_const_arraytype(mod, type_idx);
        wasm_gc_array_t* object;
        uint8_t* data;
        uint32_t code;
        uint32_t slots;

        if (!at) WASM__TRAP(WASM_ERR_MALFORMED);
        code = wasm__field_code(&at->field);
        slots = wasm__field_slots(code);
        if (subop != 0x08) {
            if (wasm__slot_i32(WASM__PEEK(rt)) < 0) WASM__TRAP(WASM_ERR_OUT_OF_BOUNDS);
            count = wasm__slot_u32(WASM__PEEK(rt));
        }
        WASM__CHARGE_ELEMS(rt, count);
        object = wasm__gc_alloc_array_object(rt, mod, type_idx, at, count);
        if (!object) WASM__TRAP(WASM_ERR_OOM);
        data = (uint8_t*)object + at->data_offset;
        if (subop == 0x08) {
            for (i = count; i > 0; i--) {
                exec->sp -= slots;
                wasm__field_store(data + (size_t)(i - 1u) * at->elem_size, code, &exec->stack[exec->sp]);
            }
        } else {
            exec->sp--;
            if (subop == 0x06) {
                exec->sp -= slots;
                if (count > 0) {
                    wasm__field_store(data, code, &exec->stack[exec->sp]);
                    wasm__field_fill(data, at->elem_size, count);
                }
            }
        }
        wasm__slot_set_ref(&exec->stack[exec->sp++], (uintptr_t)object);
    }

cleanup:
    return err;
}

#ifndef NDEBUG
WASM_PRIVATE void wasm__gc_check_operand(wasm_module_t* mod, uint32_t type_idx, const wasm__slot_t* slot) {
    wasm_value_t value = wasm__value_from_slot(mod->rt, WASM_TYPE_ANYREF, NULL, slot);
    wasm_reftype_t ref;

    if (wasm__runtime_make_typeidx_reftype(mod, type_idx, 1, &ref) == WASM_OK &&
        wasm__runtime_value_matches_type(mod, &value, ref.type, &ref))
        return;
    fprintf(stderr, "operand %p of a GC instruction does not match type %u\n", (void*)value.of.gc_ref,
            (unsigned)type_idx);
    abort();
}
#endif

WASM_PRIVATE int wasm__exec_cast_matches(const wasm_module_t* mod, uint32_t target, uint32_t type_idx,
                                         const wasm__slot_t* slot) {
    wasm_reftype_t ref;
    wasm_value_t value;

    memset(&ref, 0, sizeof(ref));
    ref.type = WASM__CAST_HEAP(target);
    ref.type_index = type_idx;
    ref.has_type_index = (target & WASM__CAST_INDEXED) != 0;
    ref.nullable = (target & WASM__CAST_NULLABLE) != 0;
    ref.exact = (target & WASM__CAST_EXACT) != 0;
    value = wasm__value_from_slot(mod->rt, wasm__ref_top_type(ref.type), NULL, slot);
    return wasm__runtime_value_matches_type(mod, &value, ref.type, &ref);
}

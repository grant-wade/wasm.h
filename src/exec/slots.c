#include "slots.h"
#include "stack.h"
#include "../runtime.h"
#include "../support.h"
#include "../values.h"
#include "../internal/gc_types.h"
#include <string.h>

WASM_PRIVATE uint32_t wasm__slot_from_value(wasm__slot_t* s, const wasm_value_t* value) {
    switch (value->type) {
        case WASM_TYPE_I32:
            wasm__slot_set_i32(s, value->of.i32);
            return 1;
        case WASM_TYPE_I64:
            wasm__slot_set_i64(s, value->of.i64);
            return 1;
        case WASM_TYPE_F32: {
            uint32_t bits;

            memcpy(&bits, &value->of.f32, sizeof(bits));
            wasm__slot_set_f32_bits(s, bits);
            return 1;
        }
        case WASM_TYPE_F64: {
            uint64_t bits;

            memcpy(&bits, &value->of.f64, sizeof(bits));
            wasm__slot_set_f64_bits(s, bits);
            return 1;
        }
        case WASM_TYPE_V128:
            wasm__slot_set_v128(s, value->of.v128);
            return WASM__V128_SLOTS;
        case WASM_TYPE_FUNCREF:
        case WASM_TYPE_NOFUNC:
            wasm__slot_set_funcref(s, value->of.funcref);
            return 1;
        default:
            wasm__slot_set_ref(s, value->of.gc_ref);
            return 1;
    }
}

static wasm_value_t wasm__ref_from_slot(const wasm_runtime_t* rt,
                                        wasm_valtype_t type,
                                        const wasm_reftype_t* reftype,
                                        const wasm__slot_t* s) {
    wasm_value_t value;

    if (wasm__slot_u64(s) == 0) return wasm__typed_ref_null(type, reftype);
    memset(&value, 0, sizeof(value));
    switch (wasm__ref_top_type(type)) {
        case WASM_TYPE_FUNCREF:
            value.type = WASM_TYPE_FUNCREF;
            value.of.funcref = wasm__slot_funcref(s);
            return value;
        case WASM_TYPE_EXTERNREF:
        case WASM_TYPE_EXNREF:
        case WASM_TYPE_CONTREF:
            value.type = wasm__ref_top_type(type);
            value.of.externref = wasm__slot_ref(s);
            return value;
        default:
            value.type = WASM_TYPE_ANYREF;
            value.of.gc_ref = wasm__slot_ref(s);
            if ((value.of.gc_ref & WASM__GC_I31_TAG) != 0)
                value.type = WASM_TYPE_I31REF;
            else if (wasm__gc_ptr_in_heap(rt, value.of.gc_obj))
                value.type = value.of.gc_obj->kind == WASM_GC_OBJ_ARRAY ? WASM_TYPE_ARRAYREF : WASM_TYPE_STRUCTREF;
            return value;
    }
}

WASM_PRIVATE wasm_value_t wasm__value_from_slot(const wasm_runtime_t* rt,
                                                wasm_valtype_t type,
                                                const wasm_reftype_t* reftype,
                                                const wasm__slot_t* s) {
    wasm_value_t value;

    memset(&value, 0, sizeof(value));
    value.type = type;
    switch (type) {
        case WASM_TYPE_I32:
            value.of.i32 = wasm__slot_i32(s);
            return value;
        case WASM_TYPE_I64:
            value.of.i64 = wasm__slot_i64(s);
            return value;
        case WASM_TYPE_F32: {
            uint32_t bits = wasm__slot_f32_bits(s);

            memcpy(&value.of.f32, &bits, sizeof(bits));
            return value;
        }
        case WASM_TYPE_F64: {
            uint64_t bits = wasm__slot_f64_bits(s);

            memcpy(&value.of.f64, &bits, sizeof(bits));
            return value;
        }
        case WASM_TYPE_V128:
            wasm__slot_v128(s, value.of.v128);
            return value;
        default:
            return wasm__ref_from_slot(rt, type, reftype, s);
    }
}

WASM_PRIVATE uint32_t wasm__slots_from_values(wasm__slot_t* dst, const wasm_value_t* values, uint32_t count) {
    uint32_t slots = 0;
    uint32_t i;

    for (i = 0; i < count; i++) slots += wasm__slot_from_value(dst + slots, &values[i]);
    return slots;
}

WASM_PRIVATE uint32_t wasm__values_slots(const wasm_value_t* values, uint32_t count) {
    uint32_t slots = 0;
    uint32_t i;

    for (i = 0; i < count; i++) slots += wasm__valtype_slots(values[i].type);
    return slots;
}

WASM_PRIVATE uint32_t wasm__values_from_slots(const wasm_runtime_t* rt,
                                              const wasm_valtype_t* types,
                                              const wasm_reftype_t* reftypes,
                                              uint32_t count,
                                              const wasm__slot_t* src,
                                              wasm_value_t* out) {
    uint32_t slots = 0;
    uint32_t i;

    for (i = 0; i < count; i++) {
        out[i] = wasm__value_from_slot(rt, types[i], reftypes ? &reftypes[i] : NULL, src + slots);
        slots += wasm__valtype_slots(types[i]);
    }
    return slots;
}

WASM_PRIVATE wasm_value_t wasm__pop_value(wasm_runtime_t* rt, wasm_valtype_t type, const wasm_reftype_t* reftype) {
    wasm__exec_stack_t* exec = rt->exec;

    exec->sp -= wasm__valtype_slots(type);
    return wasm__value_from_slot(rt, type, reftype, &exec->stack[exec->sp]);
}

WASM_PRIVATE wasm_value_t wasm__pop_field_value(wasm_runtime_t* rt, const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) return wasm__pop_value(rt, WASM_TYPE_I32, NULL);
    return wasm__pop_value(rt, field->storage.of.valtype, &field->storage.ref_type);
}

WASM_PRIVATE void wasm__push_value(wasm_runtime_t* rt, const wasm_value_t* value) {
    wasm__exec_stack_t* exec = rt->exec;

    exec->sp += wasm__slot_from_value(&exec->stack[exec->sp], value);
}

WASM_PRIVATE wasm_valtype_t wasm__ref_top_type(wasm_valtype_t type) {
    if (wasm__uses_funcref_storage(type)) return WASM_TYPE_FUNCREF;
    if (wasm__uses_externref_storage(type)) return WASM_TYPE_EXTERNREF;
    if (wasm__uses_exnref_storage(type)) return WASM_TYPE_EXNREF;
    if (wasm__uses_contref_storage(type)) return WASM_TYPE_CONTREF;
    return WASM_TYPE_ANYREF;
}

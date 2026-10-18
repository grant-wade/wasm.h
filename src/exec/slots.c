#include "slots.h"
#include "../runtime.h"
#include "../support.h"
#include "../values.h"
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
        default:
            *s = *value;
            return 1;
    }
}

WASM_PRIVATE wasm_value_t wasm__value_from_slot(const wasm_runtime_t* rt,
                                                wasm_valtype_t type,
                                                const wasm_reftype_t* reftype,
                                                const wasm__slot_t* s) {
    wasm_value_t value;

    (void)rt;
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
            return wasm__retag_value_for_declared_type(type, reftype, *s);
    }
}

WASM_PRIVATE uint32_t wasm__slots_from_values(wasm__slot_t* dst, const wasm_value_t* values, uint32_t count) {
    uint32_t slots = 0;
    uint32_t i;

    for (i = 0; i < count; i++) slots += wasm__slot_from_value(dst + slots, &values[i]);
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
    rt->sp -= wasm__valtype_slots(type);
    return wasm__value_from_slot(rt, type, reftype, &rt->stack[rt->sp]);
}

WASM_PRIVATE wasm_value_t wasm__pop_field_value(wasm_runtime_t* rt, const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) return wasm__pop_value(rt, WASM_TYPE_I32, NULL);
    return wasm__pop_value(rt, field->storage.of.valtype, &field->storage.ref_type);
}

WASM_PRIVATE void wasm__push_value(wasm_runtime_t* rt, const wasm_value_t* value) {
    rt->sp += wasm__slot_from_value(&rt->stack[rt->sp], value);
}

WASM_PRIVATE void wasm__slot_retag(wasm__slot_t* s, wasm_valtype_t type, const wasm_reftype_t* reftype) {
    *s = wasm__retag_value_for_declared_type(type, reftype, *s);
}

WASM_PRIVATE int wasm__slot_is_null(const wasm__slot_t* s) {
    return wasm__is_null_ref(s);
}

WASM_PRIVATE int wasm__slot_ref_eq(const wasm__slot_t* a, const wasm__slot_t* b) {
    return a->of.gc_ref == b->of.gc_ref;
}

WASM_PRIVATE void wasm__slot_set_null(wasm__slot_t* s, wasm_valtype_t type, const wasm_reftype_t* reftype) {
    *s = wasm__typed_ref_null(type, reftype);
}

WASM_PRIVATE wasm_valtype_t wasm__ref_top_type(wasm_valtype_t type) {
    if (wasm__uses_funcref_storage(type)) return WASM_TYPE_FUNCREF;
    if (wasm__uses_externref_storage(type)) return WASM_TYPE_EXTERNREF;
    if (wasm__uses_exnref_storage(type)) return WASM_TYPE_EXNREF;
    return WASM_TYPE_ANYREF;
}

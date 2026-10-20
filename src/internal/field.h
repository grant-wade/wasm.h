#ifndef WASM_INTERNAL_FIELD_H
#define WASM_INTERNAL_FIELD_H

#include "../wasm.h"
#include "slot.h"
#include <string.h>

/* Storage codes of struct fields and array elements: a kind in the low byte and, for an unpacked
 * field, its declared value type in the next byte. */
#define WASM__FIELD_I8 0x0u       /* packed i8, zero-extended when loaded */
#define WASM__FIELD_I8_S 0x1u     /* packed i8, sign-extended when loaded */
#define WASM__FIELD_I16 0x2u      /* packed i16, zero-extended when loaded */
#define WASM__FIELD_I16_S 0x3u    /* packed i16, sign-extended when loaded */
#define WASM__FIELD_BITS32 0x4u   /* i32 or f32 */
#define WASM__FIELD_BITS64 0x5u   /* i64 or f64 */
#define WASM__FIELD_V128 0x6u
#define WASM__FIELD_FUNCREF 0x7u  /* a function reference id, UINT32_MAX for null */
#define WASM__FIELD_REF 0x8u      /* any other reference's bits, 0 for null */
#define WASM__FIELD_SIGNED 0x1u   /* turns a packed kind into its sign-extending kind */
#define WASM__FIELD_KIND(code) ((code) & 0xFFu)
#define WASM__FIELD_TYPE(code) ((wasm_valtype_t)(((code) >> 8) & 0xFFu))

WASM_INLINE uint32_t wasm__field_code(const wasm_fieldtype_t* field) {
    wasm_valtype_t type = field->storage.of.valtype;
    uint32_t kind;

    if (field->storage.kind == WASM_STORAGE_PACKED)
        return field->storage.of.packed_type == WASM_PACKED_I8 ? WASM__FIELD_I8 : WASM__FIELD_I16;
    switch (type) {
        case WASM_TYPE_I32:
        case WASM_TYPE_F32:
            kind = WASM__FIELD_BITS32;
            break;
        case WASM_TYPE_I64:
        case WASM_TYPE_F64:
            kind = WASM__FIELD_BITS64;
            break;
        case WASM_TYPE_V128:
            kind = WASM__FIELD_V128;
            break;
        case WASM_TYPE_FUNCREF:
        case WASM_TYPE_NOFUNC:
            kind = WASM__FIELD_FUNCREF;
            break;
        default:
            kind = WASM__FIELD_REF;
            break;
    }
    return kind | ((uint32_t)type << 8);
}

WASM_INLINE uint32_t wasm__field_slots(uint32_t code) {
    return WASM__FIELD_KIND(code) == WASM__FIELD_V128 ? WASM__V128_SLOTS : 1u;
}

/* Loads the field at p into the slots at s; returns how many slots it fills. */
WASM_INLINE uint32_t wasm__field_load(const uint8_t* p, uint32_t code, wasm__slot_t* s) {
    const wasm_value_t* v = (const wasm_value_t*)(const void*)p;

    switch (WASM__FIELD_KIND(code)) {
        case WASM__FIELD_I8:
            wasm__slot_set_u32(s, p[0]);
            return 1;
        case WASM__FIELD_I8_S:
            wasm__slot_set_i32(s, (int8_t)p[0]);
            return 1;
        case WASM__FIELD_I16:
            wasm__slot_set_u32(s, (uint32_t)p[0] | ((uint32_t)p[1] << 8));
            return 1;
        case WASM__FIELD_I16_S:
            wasm__slot_set_i32(s, (int16_t)(uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8)));
            return 1;
        case WASM__FIELD_BITS32:
            wasm__slot_set_u32(s, (uint32_t)v->of.i32);
            return 1;
        case WASM__FIELD_BITS64:
            wasm__slot_set_u64(s, (uint64_t)v->of.i64);
            return 1;
        case WASM__FIELD_V128:
            wasm__slot_set_v128(s, v->of.v128);
            return WASM__V128_SLOTS;
        case WASM__FIELD_FUNCREF:
            wasm__slot_set_funcref(s, v->of.funcref);
            return 1;
        default:
            wasm__slot_set_ref(s, v->of.gc_ref);
            return 1;
    }
}

/* Stores the value in the slots at s in the field at p, tagged with its declared type. */
WASM_INLINE void wasm__field_store(uint8_t* p, uint32_t code, const wasm__slot_t* s) {
    wasm_value_t* v = (wasm_value_t*)(void*)p;
    uint64_t bits = wasm__slot_u64(s);

    switch (WASM__FIELD_KIND(code)) {
        case WASM__FIELD_I8:
        case WASM__FIELD_I8_S:
            p[0] = (uint8_t)bits;
            return;
        case WASM__FIELD_I16:
        case WASM__FIELD_I16_S:
            p[0] = (uint8_t)bits;
            p[1] = (uint8_t)(bits >> 8);
            return;
        case WASM__FIELD_V128:
            v->type = WASM__FIELD_TYPE(code);
            wasm__slot_v128(s, v->of.v128);
            return;
        case WASM__FIELD_BITS32:
            v->type = WASM__FIELD_TYPE(code);
            memset(&v->of, 0, sizeof(v->of));
            v->of.i32 = wasm__slot_i32(s);
            return;
        case WASM__FIELD_BITS64:
            v->type = WASM__FIELD_TYPE(code);
            memset(&v->of, 0, sizeof(v->of));
            v->of.i64 = wasm__slot_i64(s);
            return;
        case WASM__FIELD_FUNCREF:
            v->type = WASM__FIELD_TYPE(code);
            memset(&v->of, 0, sizeof(v->of));
            v->of.funcref = wasm__slot_funcref(s);
            return;
        default:
            v->type = WASM__FIELD_TYPE(code);
            memset(&v->of, 0, sizeof(v->of));
            v->of.gc_ref = wasm__slot_ref(s);
            return;
    }
}

/* Returns the field at p as a value; a packed field loads as an i32, sign-extended if sign_extend. */
WASM_INLINE wasm_value_t wasm__field_load_value(const uint8_t* p, uint32_t code, int sign_extend) {
    wasm_value_t value;
    wasm__slot_t s;

    switch (WASM__FIELD_KIND(code)) {
        case WASM__FIELD_I8:
        case WASM__FIELD_I16:
            wasm__field_load(p, sign_extend ? code | WASM__FIELD_SIGNED : code, &s);
            memset(&value, 0, sizeof(value));
            value.type = WASM_TYPE_I32;
            value.of.i32 = wasm__slot_i32(&s);
            return value;
        default:
            memcpy(&value, p, sizeof(value));
            return value;
    }
}

/* Stores value in the field at p, tagged with its declared type; a packed field keeps its low bits. */
WASM_INLINE void wasm__field_store_value(uint8_t* p, uint32_t code, wasm_value_t value) {
    wasm__slot_t s;

    switch (WASM__FIELD_KIND(code)) {
        case WASM__FIELD_I8:
        case WASM__FIELD_I16:
            wasm__slot_set_i32(&s, value.of.i32);
            wasm__field_store(p, code, &s);
            return;
        default:
            value.type = WASM__FIELD_TYPE(code);
            memcpy(p, &value, sizeof(value));
            return;
    }
}

#endif /* WASM_INTERNAL_FIELD_H */

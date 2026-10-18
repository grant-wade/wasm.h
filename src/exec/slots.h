#ifndef WASM_SRC_EXEC_SLOTS_H
#define WASM_SRC_EXEC_SLOTS_H

#include "../internal/config.h"
#include "../internal/slot.h"

/* Stores value in the slots starting at s; returns how many it fills. */
WASM_PRIVATE uint32_t wasm__slot_from_value(wasm__slot_t* s, const wasm_value_t* value);

/* Returns the value of static type type, refined by reftype if not NULL, held in the slots at s. */
WASM_PRIVATE wasm_value_t wasm__value_from_slot(const wasm_runtime_t* rt,
                                                wasm_valtype_t type,
                                                const wasm_reftype_t* reftype,
                                                const wasm__slot_t* s);

/* Stores count values in the slots starting at dst; returns how many slots they fill. */
WASM_PRIVATE uint32_t wasm__slots_from_values(wasm__slot_t* dst, const wasm_value_t* values, uint32_t count);

/* Reads count values of types, refined by reftypes if not NULL, from the slots starting at src;
 * returns how many slots they span. */
WASM_PRIVATE uint32_t wasm__values_from_slots(const wasm_runtime_t* rt,
                                              const wasm_valtype_t* types,
                                              const wasm_reftype_t* reftypes,
                                              uint32_t count,
                                              const wasm__slot_t* src,
                                              wasm_value_t* out);

/* Pops the value of static type type, refined by reftype if not NULL, off rt's operand stack. */
WASM_PRIVATE wasm_value_t wasm__pop_value(wasm_runtime_t* rt, wasm_valtype_t type, const wasm_reftype_t* reftype);

/* Pops a value of the storage type of field off rt's operand stack. */
WASM_PRIVATE wasm_value_t wasm__pop_field_value(wasm_runtime_t* rt, const wasm_fieldtype_t* field);

WASM_PRIVATE void wasm__push_value(wasm_runtime_t* rt, const wasm_value_t* value);

/* Rewrites the reference in s to carry its static type type. */
WASM_PRIVATE void wasm__slot_retag(wasm__slot_t* s, wasm_valtype_t type, const wasm_reftype_t* reftype);

/* s holds a reference. */
WASM_PRIVATE int wasm__slot_is_null(const wasm__slot_t* s);

/* a and b hold eqref values. */
WASM_PRIVATE int wasm__slot_ref_eq(const wasm__slot_t* a, const wasm__slot_t* b);

/* Stores the null reference of type type, refined by reftype if not NULL. */
WASM_PRIVATE void wasm__slot_set_null(wasm__slot_t* s, wasm_valtype_t type, const wasm_reftype_t* reftype);

WASM_PRIVATE wasm_valtype_t wasm__ref_top_type(wasm_valtype_t type);

#endif /* WASM_SRC_EXEC_SLOTS_H */

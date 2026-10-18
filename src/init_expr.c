#include "init_expr.h"
#include "module.h"
#include "runtime.h"
#include "values.h"
#include "reader.h"
#include "memory_internal.h"
#include "types/references.h"
#include "types/canonical.h"
#include "portable.h"
#include "internal/gc_types.h"
#include "internal/config.h"
#include "internal/alloc.h"
#include <string.h>
#include <stdio.h>

typedef struct wasm__init_expr_stack_t {
    wasm_value_t* values;
    uint32_t size;
    uint32_t cap;
} wasm__init_expr_stack_t;

static wasm_error_t wasm__init_expr_stack_push(wasm__init_expr_stack_t* stack, wasm_value_t value) {
    if (stack->size >= stack->cap) {
        uint32_t new_cap = stack->cap ? stack->cap * 2u : 8u;
        wasm_value_t* values = (wasm_value_t*)wasm__realloc_array(stack->values, new_cap, sizeof(wasm_value_t));
        if (!values) return WASM_ERR_OOM;
        stack->values = values;
        stack->cap = new_cap;
    }

    stack->values[stack->size++] = value;
    return WASM_OK;
}

WASM_PRIVATE int wasm__init_value_matches(wasm_module_t* mod,
                                    const wasm_value_t* value,
                                    wasm_valtype_t type,
                                    const wasm_reftype_t* ref_type) {
    if (wasm__runtime_value_matches_type(mod, value, type, ref_type)) return 1;
    if (!mod || !mod->init_expr_read_placeholder) return 0;
    if (!wasm__is_ref_type(type)) return value->type == type;
    return wasm__is_ref_type(value->type);
}

static wasm_error_t wasm__init_expr_stack_pop_expect(wasm_module_t* mod,
                                                     wasm__init_expr_stack_t* stack,
                                                     wasm_valtype_t expected_type,
                                                     const wasm_reftype_t* expected_ref,
                                                     wasm_value_t* out_value) {
    wasm_value_t value;

    if (stack->size == 0) return WASM_ERR_TYPE_MISMATCH;
    value = stack->values[--stack->size];
    if (!wasm__init_value_matches(mod, &value, expected_type, expected_ref)) return WASM_ERR_TYPE_MISMATCH;
    *out_value = value;
    return WASM_OK;
}

static wasm_error_t wasm__init_expr_stack_pop_typed(wasm__init_expr_stack_t* stack,
                                                    wasm_valtype_t expected_type,
                                                    wasm_value_t* out_value) {
    return wasm__init_expr_stack_pop_expect(NULL, stack, expected_type, NULL, out_value);
}

WASM_PRIVATE wasm_error_t wasm__apply_active_data_segments(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_data_segments; i++) {
        wasm_data_segment_t* segment = &mod->data_segments[i];
        wasm_memory_t* memory;
        uint64_t mem_size;

        if (segment->is_passive) continue;
        memory = wasm__memory_at(mod, segment->memory_index);
        if (!memory) return WASM_ERR_MALFORMED;
        mem_size = wasm__memory_byte_size(memory);
        if (!wasm__range_in_bounds_u64(segment->offset, segment->size, mem_size)) return WASM_ERR_OUT_OF_BOUNDS;
        if (segment->size > 0)
            memcpy(memory->data + segment->offset, segment->bytes, segment->size);
        segment->size = 0;
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__apply_active_elem_segments(wasm_module_t* mod) {
    uint32_t i;

    for (i = 0; i < mod->num_elem_segments; i++) {
        wasm_elem_segment_t* segment = &mod->elem_segments[i];
        wasm_table_t* table;
        uint32_t j;

        if (segment->is_passive || segment->is_declarative) continue;
        if (segment->table_index >= mod->num_tables) return WASM_ERR_MALFORMED;
        table = wasm__table_at(mod, segment->table_index);
        if (!table) return WASM_ERR_MALFORMED;
        if (!wasm__elem_segment_values_match_type(mod,
                                                  segment,
                                                  table->reftype,
                                                  &table->reftype_info))
            return WASM_ERR_TYPE_MISMATCH;
        if (!wasm__range_in_bounds_u64(segment->offset, segment->num_elems, table->size))
            return WASM_ERR_OUT_OF_BOUNDS;
        if (segment->num_elems > 0) {
            if (!table->elems) return WASM_ERR_OUT_OF_BOUNDS;
            if (mod->tables[segment->table_index].is_import)
                mod->preserve_on_failure = 1;
            for (j = 0; j < segment->num_elems; j++) table->elems[segment->offset + j] = segment->elems[j];
        }
        segment->num_elems = 0;
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__eval_init_expr(wasm_module_t* mod, wasm__reader_t* r,
                                         uint32_t max_global_index,
                                         wasm_value_t* out_value) {
    wasm__init_expr_stack_t stack;
    wasm_error_t err = WASM_OK;

    memset(&stack, 0, sizeof(stack));

    for (;;) {
        uint8_t op;

        if (!wasm__has(r, 1)) {
            err = WASM_ERR_MALFORMED;
            break;
        }

        op = wasm__read_u8(r);
        if (op == 0x0B) break;

        switch (op) {
            case 0x41:
                err = wasm__init_expr_stack_push(&stack, wasm_i32(wasm__read_leb128_i32(r)));
                break;
            case 0x42:
                err = wasm__init_expr_stack_push(&stack, wasm_i64(wasm__read_leb128_i64(r)));
                break;
            case 0x43: {
                wasm_value_t value = wasm__read_f32(r);
                err = wasm__init_expr_stack_push(&stack, value);
                break;
            }
            case 0x44: {
                wasm_value_t value = wasm__read_f64(r);
                err = wasm__init_expr_stack_push(&stack, value);
                break;
            }
            case 0xFD: {
                uint32_t subop = wasm__read_leb128_u32(r);

                err = wasm__require_feature(mod, WASM_FEATURE_SIMD);
                if (err != WASM_OK) break;
                if (subop != 0x0C) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "constant expression required");
                    err = WASM_ERR_TYPE_MISMATCH;
                    break;
                }
                if (!wasm__has(r, 16)) {
                    err = WASM_ERR_MALFORMED;
                    break;
                }
                {
                    wasm_value_t value = wasm_v128(r->ptr);
                    r->ptr += 16;
                    err = wasm__init_expr_stack_push(&stack, value);
                }
                break;
            }
            case 0x23: {
                uint32_t global_index = wasm__read_leb128_u32(r);
                const wasm_global_t* global;

                if (global_index >= max_global_index || global_index >= mod->num_globals || !mod->globals) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown global %u", (unsigned)global_index);
                    err = WASM_ERR_MALFORMED;
                    break;
                }

                global = &mod->globals[global_index];
                if (!global->is_import &&
                    (!mod->rt || (mod->rt->enabled_features & WASM_FEATURE_EXTENDED_CONST) == 0)) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown global %u", (unsigned)global_index);
                    err = WASM_ERR_MALFORMED;
                    break;
                }
                if (global->is_mutable) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "constant expression required");
                    err = WASM_ERR_TYPE_MISMATCH;
                    break;
                }

                if (global->is_import && mod->is_compiled) mod->init_expr_read_placeholder = 1;
                err = wasm__init_expr_stack_push(&stack, wasm__global_get_value(global));
                break;
            }
            case 0x6A: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i32(wasm__wrap_i32_add(lhs.of.i32, rhs.of.i32)));
                break;
            }
            case 0x6B: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i32(wasm__wrap_i32_sub(lhs.of.i32, rhs.of.i32)));
                break;
            }
            case 0x6C: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i32(wasm__wrap_i32_mul(lhs.of.i32, rhs.of.i32)));
                break;
            }
            case 0x7C: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i64(wasm__wrap_i64_add(lhs.of.i64, rhs.of.i64)));
                break;
            }
            case 0x7D: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i64(wasm__wrap_i64_sub(lhs.of.i64, rhs.of.i64)));
                break;
            }
            case 0x7E: {
                wasm_value_t rhs, lhs;

                err = wasm__require_feature(mod, WASM_FEATURE_EXTENDED_CONST);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &rhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I64, &lhs);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(
                    &stack,
                    wasm_i64(wasm__wrap_i64_mul(lhs.of.i64, rhs.of.i64)));
                break;
            }
            case 0xD0: {
                wasm_valtype_t type;
                wasm_value_t value;
                wasm_reftype_t reftype = { 0 };

                err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) break;
                err = wasm__read_heaptype(mod, r, &reftype);
                if (err != WASM_OK) break;
                reftype.nullable = 1;
                type = reftype.type ? reftype.type : WASM_TYPE_ANYREF;

                value = wasm__typed_ref_null(type, &reftype);
                err = wasm__init_expr_stack_push(&stack, value);
                break;
            }
            case 0xD2: {
                uint32_t func_index = wasm__read_leb128_u32(r);
                wasm_value_t funcref;

                err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
                if (err != WASM_OK) break;
                if (func_index >= mod->num_funcs) {
                    WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown function %u",
                                  (unsigned)func_index);
                    err = WASM_ERR_MALFORMED;
                    break;
                }

                wasm__declare_func_ref(mod, func_index);
                err = wasm__make_funcref(mod, func_index, &funcref);
                if (err != WASM_OK) break;
                err = wasm__init_expr_stack_push(&stack, funcref);
                break;
            }
            case 0xFB: {
                uint32_t subop = wasm__read_leb128_u32(r);

                err = wasm__require_feature(mod, WASM_FEATURE_GC);
                if (err != WASM_OK) break;

                switch (subop) {
                    case 0x00:
                    case 0x01: {
                        uint32_t type_idx = wasm__read_leb128_u32(r);
                        const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
                        wasm_gc_struct_t* object;
                        uint32_t i;

                        if (!st) {
                            err = WASM_ERR_MALFORMED;
                            break;
                        }
                        object = wasm__gc_alloc_struct_object(mod->rt, mod, type_idx, st);
                        if (!object) {
                            err = WASM_ERR_OOM;
                            break;
                        }
                        if (subop == 0x00) {
                            for (i = st->num_fields; i > 0; i--) {
                                wasm_value_t field_value;
                                const wasm_fieldtype_t* field = &st->fields[i - 1];
                                wasm_valtype_t expected_type =
                                    (field->storage.kind == WASM_STORAGE_PACKED) ? WASM_TYPE_I32
                                                                                 : field->storage.of.valtype;

                                err = wasm__init_expr_stack_pop_expect(mod,
                                                                       &stack,
                                                                       expected_type,
                                                                       &field->storage.ref_type,
                                                                       &field_value);
                                if (err != WASM_OK) break;
                                err = wasm__gc_struct_store_field(object, st, i - 1, field_value);
                                if (err != WASM_OK) break;
                            }
                            if (err != WASM_OK) break;
                        } else {
                            for (i = 0; i < st->num_fields; i++) {
                                err = wasm__gc_struct_store_field(object, st, i,
                                                                  wasm__gc_field_default_value(&st->fields[i]));
                                if (err != WASM_OK) break;
                            }
                            if (err != WASM_OK) break;
                        }

                        err = wasm__init_expr_stack_push(&stack,
                                                         wasm__gc_type_ref_value(mod, type_idx, &object->header));
                        break;
                    }
                    case 0x06:
                    case 0x07:
                    case 0x08: {
                        uint32_t type_idx = wasm__read_leb128_u32(r);
                        const wasm_arraytype_t* atype = wasm__module_const_arraytype(mod, type_idx);
                        wasm_gc_array_t* object;
                        uint32_t count = 0;
                        uint32_t i;

                        if (!atype) {
                            err = WASM_ERR_MALFORMED;
                            break;
                        }

                        if (subop == 0x08) {
                            count = wasm__read_leb128_u32(r);
                        } else {
                            wasm_value_t length_value;

                            if (subop == 0x06) {
                                wasm_value_t init_value;
                                wasm_valtype_t expected_type =
                                    (atype->field.storage.kind == WASM_STORAGE_PACKED) ? WASM_TYPE_I32
                                                                                       : atype->field.storage.of.valtype;

                                err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &length_value);
                                if (err != WASM_OK) break;
                                err = wasm__init_expr_stack_pop_expect(mod,
                                                                       &stack,
                                                                       expected_type,
                                                                       &atype->field.storage.ref_type,
                                                                       &init_value);
                                if (err != WASM_OK) break;
                                if (length_value.of.i32 < 0) {
                                    err = WASM_ERR_OUT_OF_BOUNDS;
                                    break;
                                }
                                count = (uint32_t)length_value.of.i32;
                                object = wasm__gc_alloc_array_object(mod->rt, mod, type_idx, atype, count);
                                if (!object) {
                                    err = WASM_ERR_OOM;
                                    break;
                                }
                                for (i = 0; i < count; i++) {
                                    err = wasm__gc_array_store_element(object, atype, i, init_value);
                                    if (err != WASM_OK) break;
                                }
                                if (err != WASM_OK) break;
                                err = wasm__init_expr_stack_push(&stack,
                                                                 wasm__gc_type_ref_value(mod, type_idx, &object->header));
                                break;
                            }

                            err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &length_value);
                            if (err != WASM_OK) break;
                            if (length_value.of.i32 < 0) {
                                err = WASM_ERR_OUT_OF_BOUNDS;
                                break;
                            }
                            count = (uint32_t)length_value.of.i32;
                        }

                        object = wasm__gc_alloc_array_object(mod->rt, mod, type_idx, atype, count);
                        if (!object) {
                            err = WASM_ERR_OOM;
                            break;
                        }

                        if (subop == 0x07) {
                            wasm_value_t default_value = wasm__gc_field_default_value(&atype->field);
                            for (i = 0; i < count; i++) {
                                err = wasm__gc_array_store_element(object, atype, i, default_value);
                                if (err != WASM_OK) break;
                            }
                            if (err != WASM_OK) break;
                        } else {
                            for (i = count; i > 0; i--) {
                                wasm_value_t elem_value;
                                wasm_valtype_t expected_type =
                                    (atype->field.storage.kind == WASM_STORAGE_PACKED) ? WASM_TYPE_I32
                                                                                       : atype->field.storage.of.valtype;

                                err = wasm__init_expr_stack_pop_expect(mod,
                                                                       &stack,
                                                                       expected_type,
                                                                       &atype->field.storage.ref_type,
                                                                       &elem_value);
                                if (err != WASM_OK) break;
                                err = wasm__gc_array_store_element(object, atype, i - 1, elem_value);
                                if (err != WASM_OK) break;
                            }
                            if (err != WASM_OK) break;
                        }

                        err = wasm__init_expr_stack_push(&stack,
                                                         wasm__gc_type_ref_value(mod, type_idx, &object->header));
                        break;
                    }
                    case 0x1A: {
                        wasm_value_t value;

                        err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_EXTERNREF, &value);
                        if (err != WASM_OK) break;
                        value = wasm__anyref_from_externref_value(mod->rt, value);
                        err = wasm__init_expr_stack_push(&stack, value);
                        break;
                    }
                    case 0x1B: {
                        wasm_value_t value;
                        wasm_value_t extern_value;

                        err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_ANYREF, &value);
                        if (err != WASM_OK) break;
                        err = wasm__externref_from_anyref_value(mod->rt, mod, value, &extern_value);
                        if (err != WASM_OK) break;
                        err = wasm__init_expr_stack_push(&stack, extern_value);
                        break;
                    }
                    case 0x1C: {
                        wasm_value_t value;

                        err = wasm__init_expr_stack_pop_typed(&stack, WASM_TYPE_I32, &value);
                        if (err != WASM_OK) break;
                        value.type = WASM_TYPE_I31REF;
                        value.of.gc_ref = wasm__gc_i31_encode(value.of.i32);
                        err = wasm__init_expr_stack_push(&stack, value);
                        break;
                    }
                    default:
                        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "constant expression required");
                        err = WASM_ERR_TYPE_MISMATCH;
                        break;
                }
                break;
            }
            default:
                WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "constant expression required");
                err = WASM_ERR_TYPE_MISMATCH;
                break;
        }

        if (err != WASM_OK) break;
    }

    if (err == WASM_OK) {
        if (stack.size != 1)
            err = WASM_ERR_TYPE_MISMATCH;
        else
            *out_value = stack.values[0];
    }

    WASM_FREE(stack.values);
    return err;
}

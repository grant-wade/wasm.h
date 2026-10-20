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
    const wasm_allocator_t* allocator;
    wasm_value_t* values;
    uint32_t size;
    uint32_t cap;
} wasm__init_expr_stack_t;

/* The static type of a constant expression operand. */
typedef struct wasm__const_type_t {
    wasm_valtype_t type;
    wasm_reftype_t ref;
} wasm__const_type_t;

typedef struct wasm__const_type_stack_t {
    wasm_module_t* mod;
    wasm__const_type_t* items;
    uint32_t size;
    uint32_t cap;
} wasm__const_type_stack_t;

static wasm_error_t wasm__init_expr_stack_push(wasm__init_expr_stack_t* stack, wasm_value_t value) {
    if (stack->size >= stack->cap) {
        uint32_t new_cap = stack->cap ? stack->cap * 2u : 8u;
        wasm_value_t* values = (wasm_value_t*)wasm__realloc_array(stack->allocator, stack->values, new_cap, sizeof(wasm_value_t));
        if (!values) return WASM_ERR_OOM;
        stack->values = values;
        stack->cap = new_cap;
    }

    stack->values[stack->size++] = value;
    return WASM_OK;
}

static wasm_error_t wasm__init_expr_stack_pop(wasm__init_expr_stack_t* stack, wasm_value_t* out_value) {
    if (stack->size == 0) return WASM_ERR_TYPE_MISMATCH;
    *out_value = stack->values[--stack->size];
    return WASM_OK;
}

static wasm_error_t wasm__const_type_mismatch(wasm_module_t* mod) {
    WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "%s", "type mismatch in constant expression");
    return WASM_ERR_TYPE_MISMATCH;
}

static wasm_error_t wasm__const_not_constant(wasm_module_t* mod) {
    WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "%s", "constant expression required");
    return WASM_ERR_TYPE_MISMATCH;
}

/* ref is NULL for a value type without reference type information. */
static wasm_error_t wasm__const_push(wasm__const_type_stack_t* stack, wasm_valtype_t type, const wasm_reftype_t* ref) {
    wasm__const_type_t* item;

    if (stack->size >= stack->cap) {
        uint32_t new_cap = stack->cap ? stack->cap * 2u : 8u;
        wasm__const_type_t* items;

        if (new_cap < stack->cap) return WASM_ERR_OOM;
        items = (wasm__const_type_t*)wasm__realloc_array(&stack->mod->rt->allocator, stack->items, new_cap,
                                                          sizeof(*items));
        if (!items) return WASM_ERR_OOM;
        stack->items = items;
        stack->cap = new_cap;
    }
    item = &stack->items[stack->size++];
    item->type = type;
    if (ref)
        item->ref = *ref;
    else
        wasm__clear_reftype(&item->ref);
    return WASM_OK;
}

static wasm_error_t wasm__const_push_typeidx(wasm__const_type_stack_t* stack, uint32_t type_idx) {
    wasm_reftype_t ref;
    wasm_error_t err = wasm__runtime_make_typeidx_reftype(stack->mod, type_idx, 0, &ref);

    if (err != WASM_OK) return err;
    ref.exact = 1;
    return wasm__const_push(stack, ref.type, &ref);
}

/* ref is NULL for a value type without reference type information. */
static wasm_error_t wasm__const_pop(wasm__const_type_stack_t* stack, wasm_valtype_t type, const wasm_reftype_t* ref,
                                    wasm__const_type_t* out_item) {
    wasm__const_type_t item;

    if (stack->size == 0) return wasm__const_type_mismatch(stack->mod);
    item = stack->items[--stack->size];
    if (!wasm__typed_valtype_is_subtype(stack->mod, item.type, &item.ref, type, ref))
        return wasm__const_type_mismatch(stack->mod);
    if (out_item) *out_item = item;
    return WASM_OK;
}

static wasm_error_t wasm__const_pop_field(wasm__const_type_stack_t* stack, const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) return wasm__const_pop(stack, WASM_TYPE_I32, NULL, NULL);
    return wasm__const_pop(stack, field->storage.of.valtype, &field->storage.ref_type, NULL);
}

static int wasm__const_field_defaultable(const wasm_fieldtype_t* field) {
    return field->storage.kind == WASM_STORAGE_PACKED || !wasm__is_ref_type(field->storage.of.valtype) ||
           !wasm__has_reftype_info(&field->storage.ref_type) || field->storage.ref_type.nullable;
}

static wasm_error_t wasm__const_not_defaultable(wasm_module_t* mod) {
    WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "%s", "field type is not defaultable");
    return WASM_ERR_TYPE_MISMATCH;
}

static wasm_error_t wasm__validate_const_struct_new(wasm__const_type_stack_t* stack, wasm__reader_t* r,
                                                    uint32_t subop) {
    wasm_module_t* mod = stack->mod;
    uint32_t type_idx = wasm__read_leb128_u32(r);
    const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
    int is_default = subop == 0x01 || subop == 0x21;
    int with_desc = subop == 0x20 || subop == 0x21;
    wasm_error_t err;
    uint32_t i;

    if (!st) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown struct type %u", (unsigned)type_idx);
        return WASM_ERR_MALFORMED;
    }
    if (with_desc) {
        wasm_reftype_t descriptor_ref;

        err = wasm__require_feature(mod, WASM_FEATURE_CUSTOM_DESCRIPTORS);
        if (err != WASM_OK) return err;
        if (mod->types[type_idx].descriptor == UINT32_MAX) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "%s",
                          "type without descriptor requires non-descriptor allocation");
            return WASM_ERR_TYPE_MISMATCH;
        }
        err = wasm__runtime_make_typeidx_reftype(mod, mod->types[type_idx].descriptor, 1, &descriptor_ref);
        if (err != WASM_OK) return err;
        descriptor_ref.exact = 1;
        err = wasm__const_pop(stack, descriptor_ref.type, &descriptor_ref, NULL);
        if (err != WASM_OK) return err;
    } else if (mod->types[type_idx].descriptor != UINT32_MAX) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "%s", "type with descriptor requires descriptor allocation");
        return WASM_ERR_TYPE_MISMATCH;
    }
    for (i = st->num_fields; i > 0; i--) {
        if (is_default) {
            if (!wasm__const_field_defaultable(&st->fields[i - 1u])) return wasm__const_not_defaultable(mod);
            continue;
        }
        err = wasm__const_pop_field(stack, &st->fields[i - 1u]);
        if (err != WASM_OK) return err;
    }
    return wasm__const_push_typeidx(stack, type_idx);
}

static wasm_error_t wasm__validate_const_array_new(wasm__const_type_stack_t* stack, wasm__reader_t* r,
                                                   uint32_t subop) {
    wasm_module_t* mod = stack->mod;
    uint32_t type_idx = wasm__read_leb128_u32(r);
    const wasm_arraytype_t* atype = wasm__module_const_arraytype(mod, type_idx);
    wasm_error_t err;

    if (!atype) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown array type %u", (unsigned)type_idx);
        return WASM_ERR_MALFORMED;
    }
    if (subop == 0x08) {
        uint32_t count = wasm__read_leb128_u32(r);
        uint32_t i;

        for (i = 0; i < count; i++) {
            err = wasm__const_pop_field(stack, &atype->field);
            if (err != WASM_OK) return err;
        }
    } else {
        err = wasm__const_pop(stack, WASM_TYPE_I32, NULL, NULL);
        if (err != WASM_OK) return err;
        if (subop == 0x06) {
            err = wasm__const_pop_field(stack, &atype->field);
            if (err != WASM_OK) return err;
        } else if (!wasm__const_field_defaultable(&atype->field)) {
            return wasm__const_not_defaultable(mod);
        }
    }
    return wasm__const_push_typeidx(stack, type_idx);
}

/* Pops an operand of type from and pushes it as type to, keeping its nullability. */
static wasm_error_t wasm__validate_const_convert(wasm__const_type_stack_t* stack, wasm_valtype_t from,
                                                 wasm_valtype_t to) {
    wasm__const_type_t item;
    wasm_reftype_t ref;
    wasm_error_t err;

    wasm__clear_reftype(&ref);
    ref.type = from;
    ref.nullable = 1;
    err = wasm__const_pop(stack, from, &ref, &item);
    if (err != WASM_OK) return err;
    ref.type = to;
    ref.nullable = !wasm__has_reftype_info(&item.ref) || item.ref.nullable;
    return wasm__const_push(stack, to, &ref);
}

static wasm_error_t wasm__validate_const_gc(wasm__const_type_stack_t* stack, wasm__reader_t* r) {
    wasm_module_t* mod = stack->mod;
    uint32_t subop = wasm__read_leb128_u32(r);
    wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);

    if (err != WASM_OK) return err;
    switch (subop) {
        case 0x00:
        case 0x01:
        case 0x20:
        case 0x21:
            return wasm__validate_const_struct_new(stack, r, subop);
        case 0x06:
        case 0x07:
        case 0x08:
            return wasm__validate_const_array_new(stack, r, subop);
        case 0x1A:
            return wasm__validate_const_convert(stack, WASM_TYPE_EXTERNREF, WASM_TYPE_ANYREF);
        case 0x1B:
            return wasm__validate_const_convert(stack, WASM_TYPE_ANYREF, WASM_TYPE_EXTERNREF);
        case 0x1C: {
            wasm_reftype_t ref;

            err = wasm__const_pop(stack, WASM_TYPE_I32, NULL, NULL);
            if (err != WASM_OK) return err;
            wasm__clear_reftype(&ref);
            ref.type = WASM_TYPE_I31REF;
            return wasm__const_push(stack, WASM_TYPE_I31REF, &ref);
        }
        default:
            return wasm__const_not_constant(mod);
    }
}

static wasm_error_t wasm__validate_const_binary(wasm__const_type_stack_t* stack, wasm_valtype_t type) {
    wasm_error_t err = wasm__require_feature(stack->mod, WASM_FEATURE_EXTENDED_CONST);

    if (err != WASM_OK) return err;
    err = wasm__const_pop(stack, type, NULL, NULL);
    if (err != WASM_OK) return err;
    err = wasm__const_pop(stack, type, NULL, NULL);
    if (err != WASM_OK) return err;
    return wasm__const_push(stack, type, NULL);
}

static wasm_error_t wasm__validate_const_instr(wasm__const_type_stack_t* stack, wasm__reader_t* r, uint8_t op,
                                               uint32_t max_global_index) {
    wasm_module_t* mod = stack->mod;
    wasm_error_t err;

    switch (op) {
        case 0x41:
            (void)wasm__read_leb128_i32(r);
            return wasm__const_push(stack, WASM_TYPE_I32, NULL);
        case 0x42:
            (void)wasm__read_leb128_i64(r);
            return wasm__const_push(stack, WASM_TYPE_I64, NULL);
        case 0x43:
            (void)wasm__read_f32(r);
            return wasm__const_push(stack, WASM_TYPE_F32, NULL);
        case 0x44:
            (void)wasm__read_f64(r);
            return wasm__const_push(stack, WASM_TYPE_F64, NULL);
        case 0xFD:
            err = wasm__require_feature(mod, WASM_FEATURE_SIMD);
            if (err != WASM_OK) return err;
            if (wasm__read_leb128_u32(r) != 0x0C) return wasm__const_not_constant(mod);
            if (!wasm__has(r, 16)) return WASM_ERR_MALFORMED;
            r->ptr += 16;
            return wasm__const_push(stack, WASM_TYPE_V128, NULL);
        case 0x23: {
            uint32_t global_index = wasm__read_leb128_u32(r);
            const wasm_global_t* global;

            if (global_index >= max_global_index || global_index >= mod->num_globals || !mod->globals) {
                WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown global %u", (unsigned)global_index);
                return WASM_ERR_MALFORMED;
            }
            global = &mod->globals[global_index];
            if (!global->is_import && (mod->rt->features & WASM_FEATURE_EXTENDED_CONST) == 0) {
                WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown global %u", (unsigned)global_index);
                return WASM_ERR_MALFORMED;
            }
            if (global->is_mutable) return wasm__const_not_constant(mod);
            return wasm__const_push(stack, global->type, &global->ref_type);
        }
        case 0x6A:
        case 0x6B:
        case 0x6C:
            return wasm__validate_const_binary(stack, WASM_TYPE_I32);
        case 0x7C:
        case 0x7D:
        case 0x7E:
            return wasm__validate_const_binary(stack, WASM_TYPE_I64);
        case 0xD0: {
            wasm_reftype_t ref;

            err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
            if (err != WASM_OK) return err;
            wasm__clear_reftype(&ref);
            err = wasm__read_heaptype(mod, r, &ref);
            if (err != WASM_OK) return err;
            ref.nullable = 1;
            if (!ref.type) ref.type = WASM_TYPE_ANYREF;
            return wasm__const_push(stack, ref.type, &ref);
        }
        case 0xD2: {
            uint32_t func_index = wasm__read_leb128_u32(r);
            wasm_reftype_t ref;

            err = wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
            if (err != WASM_OK) return err;
            if (func_index >= mod->num_funcs) {
                WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown function %u", (unsigned)func_index);
                return WASM_ERR_MALFORMED;
            }
            wasm__declare_func_ref(mod, func_index);
            err = wasm__runtime_make_typeidx_reftype(mod, mod->funcs[func_index].type_idx, 0, &ref);
            if (err != WASM_OK) return err;
            ref.exact = !mod->funcs[func_index].is_inexact;
            return wasm__const_push(stack, ref.type, &ref);
        }
        case 0xFB:
            return wasm__validate_const_gc(stack, r);
        default:
            return wasm__const_not_constant(mod);
    }
}

/* ref is NULL for a value type without reference type information. */
static wasm_error_t wasm__validate_init_expr(wasm_module_t* mod, wasm__reader_t* r, uint32_t max_global_index,
                                             wasm_valtype_t type, const wasm_reftype_t* ref) {
    wasm__const_type_stack_t stack;
    wasm_error_t err = WASM_OK;

    memset(&stack, 0, sizeof(stack));
    stack.mod = mod;
    for (;;) {
        uint8_t op;

        if (!wasm__has(r, 1)) {
            err = WASM_ERR_MALFORMED;
            break;
        }
        op = wasm__read_u8(r);
        if (op == 0x0B) break;
        err = wasm__validate_const_instr(&stack, r, op, max_global_index);
        if (err == WASM_OK && r->malformed) err = WASM_ERR_MALFORMED;
        if (err != WASM_OK) break;
    }
    if (err == WASM_OK) {
        if (stack.size != 1)
            err = wasm__const_type_mismatch(mod);
        else
            err = wasm__const_pop(&stack, type, ref, NULL);
    }
    wasm__free(&mod->rt->allocator, stack.items);
    return err;
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
        if (!wasm__elem_segment_matches_type(mod, segment, table->reftype, &table->reftype_info))
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

/* subop is that of struct.new, struct.new_default, struct.new_desc, or struct.new_default_desc. */
static wasm_error_t wasm__init_expr_struct_new(wasm_module_t* mod, wasm__reader_t* r, wasm__init_expr_stack_t* stack,
                                               uint32_t subop) {
    uint32_t type_idx = wasm__read_leb128_u32(r);
    const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
    int is_default = subop == 0x01 || subop == 0x21;
    wasm_gc_struct_t* object;
    wasm_value_t value;
    uint32_t i;
    wasm_error_t err;

    if (!st) return WASM_ERR_MALFORMED;
    if (subop == 0x20 || subop == 0x21) {
        if (stack->size == 0) return WASM_ERR_TYPE_MISMATCH;
        if (wasm__is_null_ref(&stack->values[stack->size - 1u])) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TRAP, "%s", "null descriptor reference");
            return WASM_ERR_TRAP;
        }
    }
    object = wasm__gc_alloc_struct_object(mod->rt, mod, type_idx, st);
    if (!object) return WASM_ERR_OOM;
    if (subop == 0x20 || subop == 0x21) {
        err = wasm__init_expr_stack_pop(stack, &value);
        if (err != WASM_OK) return err;
        *wasm__gc_descriptor_slot(&object->header) = value.of.gc_obj;
    }
    for (i = st->num_fields; i > 0; i--) {
        if (is_default) {
            value = wasm__gc_field_default_value(&st->fields[i - 1u]);
        } else {
            err = wasm__init_expr_stack_pop(stack, &value);
            if (err != WASM_OK) return err;
        }
        err = wasm__gc_struct_store_field(object, st, i - 1u, value);
        if (err != WASM_OK) return err;
    }
    return wasm__init_expr_stack_push(stack, wasm__gc_type_ref_value(mod, type_idx, &object->header));
}

/* subop is that of array.new, array.new_default, or array.new_fixed. */
static wasm_error_t wasm__init_expr_array_new(wasm_module_t* mod, wasm__reader_t* r, wasm__init_expr_stack_t* stack,
                                              uint32_t subop) {
    uint32_t type_idx = wasm__read_leb128_u32(r);
    const wasm_arraytype_t* atype = wasm__module_const_arraytype(mod, type_idx);
    wasm_gc_array_t* object;
    wasm_value_t value;
    uint32_t count;
    uint32_t i;
    wasm_error_t err;

    if (!atype) return WASM_ERR_MALFORMED;
    if (subop == 0x08) {
        count = wasm__read_leb128_u32(r);
    } else {
        err = wasm__init_expr_stack_pop(stack, &value);
        if (err != WASM_OK) return err;
        if (value.of.i32 < 0) return WASM_ERR_OUT_OF_BOUNDS;
        count = (uint32_t)value.of.i32;
    }
    object = wasm__gc_alloc_array_object(mod->rt, mod, type_idx, atype, count);
    if (!object) return WASM_ERR_OOM;
    if (subop == 0x08) {
        for (i = count; i > 0; i--) {
            err = wasm__init_expr_stack_pop(stack, &value);
            if (err != WASM_OK) return err;
            err = wasm__gc_array_store_element(object, atype, i - 1u, value);
            if (err != WASM_OK) return err;
        }
    } else {
        if (subop == 0x06) {
            err = wasm__init_expr_stack_pop(stack, &value);
            if (err != WASM_OK) return err;
        } else {
            value = wasm__gc_field_default_value(&atype->field);
        }
        for (i = 0; i < count; i++) {
            err = wasm__gc_array_store_element(object, atype, i, value);
            if (err != WASM_OK) return err;
        }
    }
    return wasm__init_expr_stack_push(stack, wasm__gc_type_ref_value(mod, type_idx, &object->header));
}

static wasm_error_t wasm__init_expr_binary(wasm__init_expr_stack_t* stack, uint8_t op) {
    wasm_value_t rhs;
    wasm_value_t lhs;
    wasm_error_t err = wasm__init_expr_stack_pop(stack, &rhs);

    if (err == WASM_OK) err = wasm__init_expr_stack_pop(stack, &lhs);
    if (err != WASM_OK) return err;
    switch (op) {
        case 0x6A:
            return wasm__init_expr_stack_push(stack, wasm_i32(wasm__wrap_i32_add(lhs.of.i32, rhs.of.i32)));
        case 0x6B:
            return wasm__init_expr_stack_push(stack, wasm_i32(wasm__wrap_i32_sub(lhs.of.i32, rhs.of.i32)));
        case 0x6C:
            return wasm__init_expr_stack_push(stack, wasm_i32(wasm__wrap_i32_mul(lhs.of.i32, rhs.of.i32)));
        case 0x7C:
            return wasm__init_expr_stack_push(stack, wasm_i64(wasm__wrap_i64_add(lhs.of.i64, rhs.of.i64)));
        case 0x7D:
            return wasm__init_expr_stack_push(stack, wasm_i64(wasm__wrap_i64_sub(lhs.of.i64, rhs.of.i64)));
        default:
            return wasm__init_expr_stack_push(stack, wasm_i64(wasm__wrap_i64_mul(lhs.of.i64, rhs.of.i64)));
    }
}

static wasm_error_t wasm__init_expr_gc(wasm_module_t* mod, wasm__reader_t* r, wasm__init_expr_stack_t* stack) {
    uint32_t subop = wasm__read_leb128_u32(r);
    wasm_value_t value;
    wasm_error_t err;

    switch (subop) {
        case 0x00:
        case 0x01:
        case 0x20:
        case 0x21:
            return wasm__init_expr_struct_new(mod, r, stack, subop);
        case 0x06:
        case 0x07:
        case 0x08:
            return wasm__init_expr_array_new(mod, r, stack, subop);
        case 0x1A:
            err = wasm__init_expr_stack_pop(stack, &value);
            if (err == WASM_OK) err = wasm__anyref_from_externref_value(mod->rt, value, &value);
            if (err != WASM_OK) return err;
            return wasm__init_expr_stack_push(stack, value);
        case 0x1B:
            err = wasm__init_expr_stack_pop(stack, &value);
            if (err == WASM_OK) err = wasm__externref_from_anyref_value(mod->rt, value, &value);
            if (err != WASM_OK) return err;
            return wasm__init_expr_stack_push(stack, value);
        case 0x1C:
            err = wasm__init_expr_stack_pop(stack, &value);
            if (err != WASM_OK) return err;
            value.type = WASM_TYPE_I31REF;
            value.of.gc_ref = wasm__gc_i31_encode(value.of.i32);
            return wasm__init_expr_stack_push(stack, value);
        default:
            return wasm__const_not_constant(mod);
    }
}

static wasm_error_t wasm__init_expr_instr(wasm_module_t* mod, wasm__reader_t* r, wasm__init_expr_stack_t* stack,
                                          uint8_t op) {
    switch (op) {
        case 0x41:
            return wasm__init_expr_stack_push(stack, wasm_i32(wasm__read_leb128_i32(r)));
        case 0x42:
            return wasm__init_expr_stack_push(stack, wasm_i64(wasm__read_leb128_i64(r)));
        case 0x43:
            return wasm__init_expr_stack_push(stack, wasm__read_f32(r));
        case 0x44:
            return wasm__init_expr_stack_push(stack, wasm__read_f64(r));
        case 0xFD: {
            wasm_value_t value;

            (void)wasm__read_leb128_u32(r);
            if (!wasm__has(r, 16)) return WASM_ERR_MALFORMED;
            value = wasm_v128(r->ptr);
            r->ptr += 16;
            return wasm__init_expr_stack_push(stack, value);
        }
        case 0x23:
            return wasm__init_expr_stack_push(stack, wasm__global_get_value(&mod->globals[wasm__read_leb128_u32(r)]));
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x7C:
        case 0x7D:
        case 0x7E:
            return wasm__init_expr_binary(stack, op);
        case 0xD0: {
            wasm_reftype_t ref;
            wasm_error_t err;

            wasm__clear_reftype(&ref);
            err = wasm__read_heaptype(mod, r, &ref);
            if (err != WASM_OK) return err;
            ref.nullable = 1;
            return wasm__init_expr_stack_push(stack, wasm__typed_ref_null(ref.type ? ref.type : WASM_TYPE_ANYREF, &ref));
        }
        case 0xD2: {
            uint32_t func_index = wasm__read_leb128_u32(r);
            wasm_value_t funcref;
            wasm_error_t err;

            err = wasm__make_funcref(mod, func_index, &funcref);
            if (err != WASM_OK) return err;
            return wasm__init_expr_stack_push(stack, funcref);
        }
        case 0xFB:
            return wasm__init_expr_gc(mod, r, stack);
        default:
            return wasm__const_not_constant(mod);
    }
}

WASM_PRIVATE wasm_error_t wasm__eval_init_expr(wasm_module_t* mod, wasm__reader_t* r, uint32_t max_global_index,
                                               wasm_valtype_t type, const wasm_reftype_t* ref,
                                               wasm_value_t* out_value) {
    wasm__init_expr_stack_t stack;
    wasm__gc_root_t root;
    wasm__reader_t start = *r;
    wasm_error_t err = wasm__validate_init_expr(mod, r, max_global_index, type, ref);

    if (err != WASM_OK) return err;
    if (mod->is_compiled) {
        *out_value = wasm__default_value(type);
        return WASM_OK;
    }
    *r = start;
    memset(&stack, 0, sizeof(stack));
    wasm__gc_push_root(mod->rt, &root, &stack.values, &stack.size);
    stack.allocator = &mod->rt->allocator;
    for (;;) {
        uint8_t op;

        if (!wasm__has(r, 1)) {
            err = WASM_ERR_MALFORMED;
            break;
        }
        op = wasm__read_u8(r);
        if (op == 0x0B) break;
        err = wasm__init_expr_instr(mod, r, &stack, op);
        if (err != WASM_OK) break;
    }
    if (err == WASM_OK) {
        if (stack.size != 1)
            err = WASM_ERR_TYPE_MISMATCH;
        else
            *out_value = stack.values[0];
    }
    wasm__gc_pop_root(mod->rt, &root);
    wasm__free(&mod->rt->allocator, stack.values);
    return err;
}

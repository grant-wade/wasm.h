#include "references.h"
#include "../exec/cont.h"
#include "../exec/exceptions.h"
#include "../handles.h"
#include <stdio.h>
#include "../internal/alloc.h"
#include "../internal/gc_types.h"
#include "../link.h"
#include "../memory_internal.h"
#include "../module.h"
#include "../portable.h"
#include "../reader.h"
#include "../runtime.h"
#include "cross_module.h"
#include "../support.h"
#include "canonical.h"
#include "../values.h"
#include <string.h>

static wasm_error_t wasm__require_valtype_feature(wasm_module_t* mod, wasm_valtype_t type) {
    if (!mod) return WASM_OK;
    if (type == WASM_TYPE_FUNCREF || type == WASM_TYPE_EXTERNREF)
        return wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
    if (type == WASM_TYPE_EXNREF || type == WASM_TYPE_NOEXN) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_EXCEPTIONS);
        if (err != WASM_OK) return err;
        return wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
    }
    if (wasm__uses_contref_storage(type)) return wasm__require_feature(mod, WASM_FEATURE_STACK_SWITCHING);
    if (wasm__is_gc_ref_type(type)) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
        if (err != WASM_OK) return err;
        return wasm__require_feature(mod, WASM_FEATURE_REFERENCE_TYPES);
    }
    if (wasm__is_vector_type(type)) return wasm__require_feature(mod, WASM_FEATURE_SIMD);
    return WASM_OK;
}

WASM_PRIVATE void wasm__clear_reftype(wasm_reftype_t* reftype) {
    if (!reftype) return;
    memset(reftype, 0, sizeof(*reftype));
}

static int wasm__is_absheaptype_byte(uint8_t byte) {
    switch (byte) {
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x6E:
        case 0x6F:
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
            return 1;
        default:
            return 0;
    }
}

static int wasm__is_reftype_lead_byte(uint8_t byte) {
    return byte == 0x63 || byte == 0x64 || wasm__is_absheaptype_byte(byte);
}

static wasm_valtype_t wasm__abstract_heap_byte_to_type(uint8_t byte) {
    switch (byte) {
        case 0x75:
            return WASM_TYPE_NOCONT;
        case 0x74:
            return WASM_TYPE_NOEXN;
        case 0x73:
            return WASM_TYPE_NOFUNC;
        case 0x72:
            return WASM_TYPE_NOEXTERN;
        case 0x71:
            return WASM_TYPE_NONE;
        case 0x70:
            return WASM_TYPE_FUNCREF;
        case 0x6F:
            return WASM_TYPE_EXTERNREF;
        case 0x6E:
            return WASM_TYPE_ANYREF;
        case 0x6D:
            return WASM_TYPE_EQREF;
        case 0x6C:
            return WASM_TYPE_I31REF;
        case 0x6B:
            return WASM_TYPE_STRUCTREF;
        case 0x6A:
            return WASM_TYPE_ARRAYREF;
        case 0x69:
            return WASM_TYPE_EXNREF;
        case 0x68:
            return WASM_TYPE_CONTREF;
        default:
            return WASM_TYPE_VOID;
    }
}

WASM_PRIVATE wasm_error_t wasm__resolve_reftype(wasm_module_t* mod, wasm_reftype_t* reftype) {
    if (!reftype || !reftype->has_type_index) return WASM_OK;
    if (!mod || reftype->type_index >= mod->num_types) {
        if (mod && mod->rt) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown type %u", (unsigned)reftype->type_index);
        }
        return WASM_ERR_MALFORMED;
    }

    switch (mod->types[reftype->type_index].kind) {
        case WASM_COMP_FUNC:
            reftype->type = WASM_TYPE_FUNCREF;
            return WASM_OK;
        case WASM_COMP_STRUCT:
            reftype->type = WASM_TYPE_STRUCTREF;
            return WASM_OK;
        case WASM_COMP_ARRAY:
            reftype->type = WASM_TYPE_ARRAYREF;
            return WASM_OK;
        case WASM_COMP_CONT:
            reftype->type = WASM_TYPE_CONTREF;
            return WASM_OK;
        default:
            return WASM_ERR_MALFORMED;
    }
}

WASM_PRIVATE wasm_error_t wasm__resolve_valtype_reftype(wasm_module_t* mod,
                                                  wasm_valtype_t* type,
                                                  wasm_reftype_t* reftype) {
    wasm_error_t err;

    if (!reftype) return WASM_OK;
    err = wasm__resolve_reftype(mod, reftype);
    if (err != WASM_OK) return err;
    if (reftype->nullable || reftype->has_type_index || wasm__is_ref_type(reftype->type)) {
        if (type) *type = reftype->type;
    }
    return WASM_OK;
}

WASM_PRIVATE int wasm__has_reftype_info(const wasm_reftype_t* reftype) {
    return reftype && (reftype->type != 0 || reftype->has_type_index || reftype->nullable);
}

WASM_PRIVATE int wasm__reftype_equal(const wasm_reftype_t* lhs, const wasm_reftype_t* rhs) {
    if (!wasm__has_reftype_info(lhs) || !wasm__has_reftype_info(rhs)) return 0;
    return lhs->type == rhs->type && lhs->type_index == rhs->type_index &&
           lhs->has_type_index == rhs->has_type_index && lhs->nullable == rhs->nullable &&
           (lhs->exact != 0) == (rhs->exact != 0);
}

WASM_PRIVATE int wasm__reftype_is_subtype(const wasm_module_t* mod,
                                    const wasm_reftype_t* subtype,
                                    const wasm_reftype_t* supertype) {
    if (!wasm__has_reftype_info(subtype) || !wasm__has_reftype_info(supertype)) return 0;
    if (subtype->nullable && !supertype->nullable) return 0;

    if (subtype->has_type_index && supertype->has_type_index) {
        if (supertype->exact)
            return subtype->exact && wasm__type_equal(mod, subtype->type_index, supertype->type_index);
        return wasm__is_subtype(mod, subtype->type_index, supertype->type_index);
    }

    if (subtype->has_type_index)
        return wasm__is_heap_subtype(mod, subtype->type, supertype->type);

    if (supertype->has_type_index) {
        switch (subtype->type) {
            case WASM_TYPE_NOFUNC:
                return supertype->type == WASM_TYPE_FUNCREF;
            case WASM_TYPE_NOCONT:
                return supertype->type == WASM_TYPE_CONTREF;
            case WASM_TYPE_NONE:
                return supertype->type == WASM_TYPE_STRUCTREF || supertype->type == WASM_TYPE_ARRAYREF;
            default:
                return 0;
        }
    }

    return wasm__is_heap_subtype(mod, subtype->type, supertype->type);
}

WASM_PRIVATE const wasm_gc_header_t* wasm__runtime_get_gc_object(const wasm_module_t* mod,
                                                           const wasm_value_t* value) {
    const wasm_runtime_t* rt;
    const uint8_t* addr;

    if (!mod || !value || !wasm__uses_gc_ref_storage(value->type)) return NULL;
    if (value->of.gc_ref == (uintptr_t)0 || (value->of.gc_ref & WASM__GC_I31_TAG) != 0) return NULL;

    rt = mod->rt;
    if (!rt || !rt->gc_heap) return NULL;
    addr = (const uint8_t*)value->of.gc_ref;
    if (addr < rt->gc_heap || addr >= rt->gc_heap + rt->gc_heap_size) return NULL;
    return (const wasm_gc_header_t*)value->of.gc_ref;
}

static wasm__externref_box_t* wasm__find_externref_box(const wasm_runtime_t* rt,
                                                       uintptr_t handle) {
    if (!rt || !wasm__handle_set_contains(&rt->externref_boxes, (const void*)handle)) return NULL;
    return (wasm__externref_box_t*)handle;
}

static wasm_error_t wasm__new_externref_box(wasm_runtime_t* rt, wasm_value_t value, wasm__externref_box_t** out_box) {
    wasm__externref_box_t* box;

    if (!rt) return WASM_ERR_MALFORMED;
    box = (wasm__externref_box_t*)wasm__malloc(&rt->allocator, sizeof(*box));
    if (!box) return WASM_ERR_OOM;
    box->value = value;
    box->marked = 0;
    if (wasm__handle_set_insert(&rt->allocator, &rt->externref_boxes, box) != WASM_OK) {
        wasm__free(&rt->allocator, box);
        return WASM_ERR_OOM;
    }
    *out_box = box;
    return WASM_OK;
}

static int wasm__is_unboxed_gc_ref(const wasm_runtime_t* rt, const wasm_value_t* value) {
    if (!wasm__uses_gc_ref_storage(value->type) || value->of.gc_ref == (uintptr_t)0) return 1;
    if ((value->of.gc_ref & WASM__GC_I31_TAG) != 0) return 1;
    return wasm__gc_ptr_in_heap(rt, (const void*)value->of.gc_ref);
}

WASM_PRIVATE wasm_error_t wasm__anyref_from_host(wasm_runtime_t* rt, wasm_value_t* value) {
    wasm__externref_box_t* box;
    wasm_error_t err;

    if (wasm__is_unboxed_gc_ref(rt, value) || wasm__find_externref_box(rt, value->of.gc_ref)) return WASM_OK;
    err = wasm__new_externref_box(rt, wasm_externref(value->of.gc_ref), &box);
    if (err != WASM_OK) {
        WASM__SET_ERR(rt, err, "%s", "out of memory boxing a host anyref");
        return err;
    }
    value->of.gc_ref = (uintptr_t)box;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__values_from_host(wasm_runtime_t* rt, wasm_value_t* values, uint32_t count) {
    uint32_t i;

    for (i = 0; i < count; i++) {
        wasm_error_t err = wasm__anyref_from_host(rt, &values[i]);

        if (err != WASM_OK) return err;
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_value_t wasm__anyref_to_host(const wasm_runtime_t* rt, wasm_value_t value) {
    wasm__externref_box_t* box;

    if (wasm__is_unboxed_gc_ref(rt, &value)) return value;
    box = wasm__find_externref_box(rt, value.of.gc_ref);
    if (box && wasm__uses_externref_storage(box->value.type)) value.of.gc_ref = box->value.of.externref;
    return value;
}

WASM_PRIVATE void wasm__values_to_host(const wasm_runtime_t* rt, const wasm_value_t* values, wasm_value_t* out,
                                       uint32_t count) {
    uint32_t i;

    for (i = 0; i < count; i++) out[i] = wasm__anyref_to_host(rt, values[i]);
}

WASM_PRIVATE wasm_error_t wasm__externref_from_anyref_value(wasm_runtime_t* rt,
                                                      wasm_value_t value,
                                                      wasm_value_t* out_value) {
    wasm__externref_box_t* box;
    wasm_error_t err;

    if (!out_value) return WASM_ERR_MALFORMED;
    if (wasm__is_null_ref(&value)) {
        *out_value = wasm_ref_null(WASM_TYPE_EXTERNREF);
        return WASM_OK;
    }
    box = wasm__uses_gc_ref_storage(value.type) ? wasm__find_externref_box(rt, value.of.gc_ref) : NULL;
    if (box && wasm__uses_externref_storage(box->value.type)) {
        *out_value = box->value;
        return WASM_OK;
    }
    if (wasm__uses_externref_storage(value.type)) {
        *out_value = value;
        return WASM_OK;
    }

    err = wasm__new_externref_box(rt, value, &box);
    if (err != WASM_OK) return err;
    *out_value = wasm_externref((uintptr_t)box);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__anyref_from_externref_value(wasm_runtime_t* rt,
                                                      wasm_value_t value,
                                                      wasm_value_t* out_value) {
    wasm__externref_box_t* box;
    wasm_error_t err;

    if (wasm__is_null_ref(&value)) {
        *out_value = wasm_ref_null(WASM_TYPE_ANYREF);
        return WASM_OK;
    }
    box = wasm__find_externref_box(rt, value.of.externref);
    if (box && !wasm__uses_externref_storage(box->value.type)) {
        *out_value = box->value;
        return WASM_OK;
    }
    value.type = WASM_TYPE_EXTERNREF;
    err = wasm__new_externref_box(rt, value, &box);
    if (err != WASM_OK) return err;
    memset(out_value, 0, sizeof(*out_value));
    out_value->type = WASM_TYPE_ANYREF;
    out_value->of.gc_ref = (uintptr_t)box;
    return WASM_OK;
}

static int wasm__is_bottom_heap_type(wasm_valtype_t type) {
    return type == WASM_TYPE_NONE || type == WASM_TYPE_NOFUNC || type == WASM_TYPE_NOEXTERN || type == WASM_TYPE_NOEXN ||
           type == WASM_TYPE_NOCONT;
}

WASM_PRIVATE int wasm__runtime_value_reftype(const wasm_module_t* mod,
                                       const wasm_value_t* value,
                                       wasm_reftype_t* out_reftype,
                                       const wasm_module_t** out_module) {
    const wasm_gc_header_t* object;
    wasm_module_t* func_mod;
    uint32_t func_idx;

    *out_module = mod;
    if (!value || !out_reftype || !wasm__is_ref_type(value->type)) {
        wasm__clear_reftype(out_reftype);
        return 0;
    }

    wasm__clear_reftype(out_reftype);
    out_reftype->type = value->type;
    out_reftype->nullable = wasm__is_null_ref(value);
    if (out_reftype->nullable) return 1;
    if (wasm__is_bottom_heap_type(value->type)) {
        wasm__clear_reftype(out_reftype);
        return 0;
    }
    if (wasm__uses_externref_storage(value->type)) return 1;
    if (wasm__uses_exnref_storage(value->type)) return mod && mod->rt && wasm__exn_live(mod->rt, value->of.externref);
    if (wasm__uses_contref_storage(value->type)) {
        const wasm__cont_object_t* k = mod && mod->rt ? wasm__cont_live(mod->rt, value->of.externref) : NULL;

        if (!k) return 0;
        if (k->module) {
            out_reftype->has_type_index = 1;
            out_reftype->exact = 1;
            out_reftype->type_index = k->type_index;
            *out_module = k->module;
        }
        return 1;
    }

    if (wasm__uses_funcref_storage(value->type)) {
        if (mod && mod->rt &&
            wasm__resolve_func_ref(mod->rt, value->of.funcref, &func_mod, &func_idx) &&
            func_mod && func_idx < func_mod->num_funcs) {
            const wasm_module_t* type_mod;
            uint32_t type_idx;
            int exact = wasm__func_dynamic_type(func_mod, func_idx, &type_mod, &type_idx);

            if (type_idx < type_mod->num_types) {
                out_reftype->has_type_index = 1;
                out_reftype->exact = exact;
                out_reftype->type_index = type_idx;
                *out_module = type_mod;
            }
        }
        return 1;
    }

    if ((value->of.gc_ref & WASM__GC_I31_TAG) != 0) {
        out_reftype->type = WASM_TYPE_I31REF;
        out_reftype->nullable = 0;
        return 1;
    }

    object = wasm__runtime_get_gc_object(mod, value);
    if (!object) {
        if (value->type == WASM_TYPE_ANYREF)
            out_reftype->type = WASM_TYPE_ANYREF;
        else if (value->type == WASM_TYPE_EQREF)
            out_reftype->type = WASM_TYPE_EQREF;
        return 1;
    }

    out_reftype->nullable = 0;
    switch (object->kind) {
        case WASM_GC_OBJ_STRUCT:
            out_reftype->type = WASM_TYPE_STRUCTREF;
            break;
        case WASM_GC_OBJ_ARRAY:
            out_reftype->type = WASM_TYPE_ARRAYREF;
            break;
        default:
            out_reftype->type = value->type;
            return 1;
    }

    if (object->module && object->type_index < object->module->num_types) {
        out_reftype->has_type_index = 1;
        out_reftype->exact = 1;
        out_reftype->type_index = object->type_index;
        *out_module = object->module;
    }
    return 1;
}

WASM_PRIVATE int wasm__runtime_value_matches_type(const wasm_module_t* mod,
                                            const wasm_value_t* value,
                                            wasm_valtype_t expected_type,
                                            const wasm_reftype_t* expected_ref) {
    wasm_reftype_t actual_ref;
    wasm_reftype_t effective_expected;
    wasm_valtype_t actual_null_type;
    const wasm_module_t* actual_mod;

    if (!value) return 0;
    if (!wasm__is_ref_type(expected_type))
        return wasm__is_valtype_subtype(mod, value->type, expected_type);
    if (expected_ref && wasm__has_reftype_info(expected_ref))
        effective_expected = *expected_ref;
    else {
        wasm__clear_reftype(&effective_expected);
        effective_expected.type = expected_type;
        effective_expected.nullable = 1;
    }
    if (effective_expected.type == WASM_TYPE_VOID) effective_expected.type = expected_type;
    if (wasm__uses_contref_storage(value->type) && mod && mod->rt && wasm__cont_consumed(mod->rt, value->of.externref))
        return effective_expected.type == WASM_TYPE_CONTREF;
    if (!wasm__runtime_value_reftype(mod, value, &actual_ref, &actual_mod)) return 0;

    if (wasm__is_null_ref(value)) {
        if (!effective_expected.nullable) return 0;
        if (wasm__uses_funcref_storage(actual_ref.type))
            actual_null_type = WASM_TYPE_NOFUNC;
        else if (wasm__uses_externref_storage(actual_ref.type))
            actual_null_type = WASM_TYPE_NOEXTERN;
        else if (wasm__uses_exnref_storage(actual_ref.type))
            actual_null_type = WASM_TYPE_NOEXN;
        else if (wasm__uses_contref_storage(actual_ref.type))
            actual_null_type = WASM_TYPE_NOCONT;
        else
            actual_null_type = WASM_TYPE_NONE;
        return wasm__is_valtype_subtype(mod, actual_null_type, expected_type);
    }

    if (!wasm__is_valtype_subtype(mod, actual_ref.type, expected_type)) return 0;
    if (actual_mod != mod && actual_ref.has_type_index && effective_expected.has_type_index) {
        if (effective_expected.exact)
            return actual_ref.exact &&
                   wasm__type_equal_across_modules(actual_mod, actual_ref.type_index, mod, effective_expected.type_index);
        return wasm__type_is_subtype_across_modules(actual_mod, actual_ref.type_index, mod, effective_expected.type_index);
    }
    return wasm__reftype_is_subtype(actual_ref.has_type_index ? actual_mod : mod, &actual_ref, &effective_expected);
}

WASM_PRIVATE int wasm__elem_segment_matches_type(const wasm_module_t* mod,
                                                 const wasm_elem_segment_t* segment,
                                                 wasm_valtype_t expected_type,
                                                 const wasm_reftype_t* expected_ref) {
    return segment && wasm__typed_valtype_is_subtype(mod, segment->elem_type, &segment->elem_ref_type, expected_type,
                                                     expected_ref);
}

WASM_PRIVATE int wasm__get_effective_reftype(const wasm_module_t* mod,
                                       wasm_valtype_t type,
                                       const wasm_reftype_t* ref_type,
                                       wasm_reftype_t* out_reftype) {
    if (!wasm__is_ref_type(type)) {
        wasm__clear_reftype(out_reftype);
        return 0;
    }

    if (ref_type && wasm__has_reftype_info(ref_type)) {
        *out_reftype = *ref_type;
        if (out_reftype->has_type_index && mod != NULL) {
            wasm_reftype_t resolved = *out_reftype;

            if (wasm__resolve_reftype((wasm_module_t*)mod, &resolved) == WASM_OK)
                *out_reftype = resolved;
        }
        if (out_reftype->type == WASM_TYPE_VOID) out_reftype->type = type;
        return 1;
    }

    wasm__clear_reftype(out_reftype);
    out_reftype->type = type;
    out_reftype->nullable = 1;
    return 1;
}

WASM_PRIVATE int wasm__typed_valtype_is_subtype(const wasm_module_t* mod,
                                          wasm_valtype_t subtype,
                                          const wasm_reftype_t* subtype_ref,
                                          wasm_valtype_t supertype,
                                          const wasm_reftype_t* supertype_ref) {
    wasm_reftype_t effective_subtype;
    wasm_reftype_t effective_supertype;

    if (!wasm__is_ref_type(subtype) && !wasm__is_ref_type(supertype))
        return wasm__is_valtype_subtype(mod, subtype, supertype);
    if (!wasm__is_ref_type(subtype) || !wasm__is_ref_type(supertype)) return 0;
    if (!wasm__get_effective_reftype(mod, subtype, subtype_ref, &effective_subtype) ||
        !wasm__get_effective_reftype(mod, supertype, supertype_ref, &effective_supertype))
        return 0;
    return wasm__reftype_is_subtype(mod, &effective_subtype, &effective_supertype);
}

WASM_PRIVATE int wasm__functype_is_subtype(const wasm_module_t* mod,
                                     const wasm_functype_t* subtype,
                                     const wasm_functype_t* supertype) {
    uint32_t i;

    if (!subtype || !supertype) return 0;
    if (subtype->num_params != supertype->num_params || subtype->num_results != supertype->num_results)
        return 0;

    for (i = 0; i < subtype->num_params; i++) {
        const wasm_reftype_t* subtype_ref = subtype->param_reftypes ? &subtype->param_reftypes[i] : NULL;
        const wasm_reftype_t* supertype_ref = supertype->param_reftypes ? &supertype->param_reftypes[i] : NULL;

        if (!wasm__typed_valtype_is_subtype(mod,
                                            supertype->params[i],
                                            supertype_ref,
                                            subtype->params[i],
                                            subtype_ref))
            return 0;
    }

    for (i = 0; i < subtype->num_results; i++) {
        const wasm_reftype_t* subtype_ref = subtype->result_reftypes ? &subtype->result_reftypes[i] : NULL;
        const wasm_reftype_t* supertype_ref = supertype->result_reftypes ? &supertype->result_reftypes[i] : NULL;

        if (!wasm__typed_valtype_is_subtype(mod,
                                            subtype->results[i],
                                            subtype_ref,
                                            supertype->results[i],
                                            supertype_ref))
            return 0;
    }

    return 1;
}

WASM_PRIVATE int wasm__host_functype_equal(const wasm_module_t* mod, const wasm_functype_t* host_type,
                                           uint32_t type_idx) {
    const wasm_comptype_t* type;

    if (!mod || type_idx >= mod->num_types) return 0;
    type = &mod->types[type_idx];
    if (type->kind != WASM_COMP_FUNC || !type->is_final || type->num_supertypes != 0 ||
        type->rec_group >= mod->num_rec_groups || mod->rec_groups[type->rec_group].num_types != 1)
        return 0;
    return wasm__functype_is_subtype(mod, host_type, &type->of.func) &&
           wasm__functype_is_subtype(mod, &type->of.func, host_type);
}

WASM_PRIVATE int wasm__globaltype_matches_import(const wasm_module_t* actual_mod,
                                           wasm_valtype_t actual_type,
                                           const wasm_reftype_t* actual_ref,
                                           int actual_mutable,
                                           const wasm_module_t* mod,
                                           wasm_valtype_t expected_type,
                                           const wasm_reftype_t* expected_ref,
                                           int expected_mutable) {
    if (actual_mutable != expected_mutable) return 0;
    if (expected_mutable)
        return wasm__valtype_equal_across_modules(actual_mod, actual_type, actual_ref, mod, expected_type, expected_ref);
    return wasm__valtype_is_subtype_across_modules(actual_mod, actual_type, actual_ref, mod, expected_type,
                                                   expected_ref);
}

static int wasm__limits_match_import(uint64_t actual_min,
                                     uint64_t actual_max,
                                     uint64_t expected_min,
                                     uint64_t expected_max) {
    if (actual_min < expected_min) return 0;
    if (expected_max == UINT64_MAX) return 1;
    return actual_max <= expected_max;
}

WASM_PRIVATE int wasm__tabletype_matches_import(const wasm_module_t* mod,
                                          const wasm_table_t* actual_table,
                                          wasm_valtype_t expected_type,
                                          const wasm_reftype_t* expected_ref,
                                          uint64_t expected_min,
                                          uint64_t expected_max,
                                          int expected_is_64) {
    actual_table = wasm__table_actual_const(actual_table);
    if (!actual_table) return 0;
    if (actual_table->is_64 != expected_is_64) return 0;
    if (!wasm__valtype_equal_across_modules(actual_table->type_module,
                                            actual_table->reftype,
                                            &actual_table->reftype_info,
                                            mod,
                                            expected_type,
                                            expected_ref)) {
        return 0;
    }
    return wasm__limits_match_import(actual_table->size,
                                     actual_table->max_size,
                                     expected_min,
                                     expected_max);
}

WASM_PRIVATE int wasm__memorytype_matches_import(const wasm_memory_t* actual_memory,
                                           const wasm_memory_t* expected_memory) {
    actual_memory = wasm__memory_actual_const(actual_memory);
    if (!actual_memory) return 0;
    if (actual_memory->is_64 != expected_memory->is_64) return 0;
    if ((actual_memory->is_shared != 0) != (expected_memory->is_shared != 0)) return 0;
    if (wasm__memory_page_size(actual_memory) != wasm__memory_page_size(expected_memory)) return 0;
    return wasm__limits_match_import(wasm__memory_pages(actual_memory),
                                     actual_memory->max_pages,
                                     expected_memory->pages,
                                     expected_memory->max_pages);
}

WASM_PRIVATE wasm_error_t wasm__read_heaptype(wasm_module_t* mod,
                                        wasm__reader_t* r,
                                        wasm_reftype_t* out_reftype) {
    int64_t type_index;
    int nullable = out_reftype ? out_reftype->nullable : 0;

    wasm__clear_reftype(out_reftype);
    if (out_reftype) out_reftype->nullable = nullable;

    if (r->ptr >= r->end) return WASM_ERR_MALFORMED;
    if (wasm__is_absheaptype_byte(*r->ptr)) {
        out_reftype->type = wasm__abstract_heap_byte_to_type(wasm__read_u8(r));
        return wasm__require_valtype_feature(mod, out_reftype->type);
    }

    if (*r->ptr == 0x62) {
        r->ptr++;
        if (mod) {
            wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_CUSTOM_DESCRIPTORS);
            if (err != WASM_OK) return err;
        }
        out_reftype->exact = 1;
        type_index = wasm__read_leb128_u32(r);
        if (r->malformed) return WASM_ERR_MALFORMED;
    } else {
        type_index = wasm__read_leb128_i64(r);
        if (type_index < 0 || type_index > (int64_t)UINT32_MAX) return WASM_ERR_MALFORMED;
    }
    out_reftype->has_type_index = 1;
    out_reftype->type_index = (uint32_t)type_index;
    if (mod && out_reftype->type_index >= mod->num_types) {
        if (mod->rt) {
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "unknown type %u",
                          (unsigned)out_reftype->type_index);
        }
        return WASM_ERR_MALFORMED;
    }

    if (mod) {
        wasm_error_t err = wasm__require_feature(mod, WASM_FEATURE_GC);
        if (err != WASM_OK) return err;
        return wasm__resolve_reftype(mod, out_reftype);
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__runtime_make_typeidx_reftype(wasm_module_t* mod,
                                                       uint32_t type_idx,
                                                       int nullable,
                                                       wasm_reftype_t* out_reftype) {
    wasm_error_t err;

    if (!out_reftype || !mod || type_idx >= mod->num_types) return WASM_ERR_MALFORMED;
    wasm__clear_reftype(out_reftype);
    out_reftype->has_type_index = 1;
    out_reftype->type_index = type_idx;
    out_reftype->nullable = nullable;
    err = wasm__resolve_reftype(mod, out_reftype);
    if (err != WASM_OK) return WASM_ERR_MALFORMED;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__read_reftype(wasm_module_t* mod,
                                       wasm__reader_t* r,
                                       wasm_valtype_t* out_type,
                                       wasm_reftype_t* out_reftype) {
    wasm_reftype_t reftype;
    uint8_t lead;
    wasm_error_t err;

    if (r->ptr >= r->end) return WASM_ERR_MALFORMED;
    lead = *r->ptr;
    wasm__clear_reftype(&reftype);

    if (wasm__is_absheaptype_byte(lead)) {
        reftype.nullable = 1;
        err = wasm__read_heaptype(mod, r, &reftype);
    } else if (lead == 0x63 || lead == 0x64) {
        r->ptr++;
        reftype.nullable = (lead == 0x63);
        err = wasm__read_heaptype(mod, r, &reftype);
    } else {
        return WASM_ERR_MALFORMED;
    }
    if (err != WASM_OK) return err;

    if (out_reftype) *out_reftype = reftype;
    if (out_type) *out_type = reftype.type ? reftype.type : WASM_TYPE_ANYREF;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__read_valtype(wasm_module_t* mod,
                                       wasm__reader_t* r,
                                       wasm_valtype_t* out_type,
                                       wasm_reftype_t* out_reftype) {
    wasm_valtype_t type;

    if (r->ptr >= r->end) return WASM_ERR_MALFORMED;
    if (wasm__is_reftype_lead_byte(*r->ptr)) return wasm__read_reftype(mod, r, out_type, out_reftype);

    type = (wasm_valtype_t)wasm__read_u8(r);
    if (!wasm__is_value_type(type)) return WASM_ERR_MALFORMED;
    if (out_type) *out_type = type;
    if (out_reftype) wasm__clear_reftype(out_reftype);
    return wasm__require_valtype_feature(mod, type);
}

WASM_PRIVATE wasm_error_t wasm__read_unresolved_valtype(wasm_module_t* mod,
                                                  wasm__reader_t* r,
                                                  wasm_valtype_t* out_type,
                                                  wasm_reftype_t* out_reftype) {
    wasm_error_t err;

    if (r->ptr >= r->end) return WASM_ERR_MALFORMED;
    if (!wasm__is_reftype_lead_byte(*r->ptr)) return wasm__read_valtype(mod, r, out_type, out_reftype);

    err = wasm__read_reftype(NULL, r, out_type, out_reftype);
    if (err != WASM_OK) return err;
    if (out_reftype && out_reftype->exact) return wasm__require_feature(mod, WASM_FEATURE_CUSTOM_DESCRIPTORS);
    if (out_reftype && out_reftype->has_type_index) return wasm__require_feature(mod, WASM_FEATURE_GC);
    return wasm__require_valtype_feature(mod, *out_type);
}

WASM_PRIVATE void wasm__skip_heaptype(wasm__reader_t* r) {
    if (r->ptr >= r->end) return;
    if (wasm__is_absheaptype_byte(*r->ptr)) {
        r->ptr++;
        return;
    }
    if (*r->ptr == 0x62) {
        r->ptr++;
        (void)wasm__read_leb128_u32(r);
        return;
    }
    (void)wasm__read_leb128_i64(r);
}

static void wasm__skip_reftype(wasm__reader_t* r) {
    if (r->ptr >= r->end) return;
    if (wasm__is_absheaptype_byte(*r->ptr)) {
        r->ptr++;
        return;
    }
    if (*r->ptr == 0x63 || *r->ptr == 0x64) {
        r->ptr++;
        wasm__skip_heaptype(r);
        return;
    }
    r->ptr = r->end;
}

WASM_PRIVATE void wasm__skip_valtype(wasm__reader_t* r) {
    if (r->ptr >= r->end) return;
    if (wasm__is_reftype_lead_byte(*r->ptr)) {
        wasm__skip_reftype(r);
        return;
    }
    r->ptr++;
}

WASM_PRIVATE int wasm__is_packedtype_byte(uint8_t byte) {
    return byte == 0x77 || byte == 0x78;
}

WASM_PRIVATE int wasm__is_valtype_byte(uint8_t byte) {
    switch (byte) {
        case 0x63:
        case 0x64:
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x6E:
        case 0x6F:
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0x74:
        case 0x75:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
            return 1;
        default:
            return 0;
    }
}

#include "values.h"
#include "simd/native.h"
#include "portable.h"
#include "support.h"
#include "types/references.h"
#include "types/canonical.h"
#include "module.h"
#include "internal/gc_types.h"
#include "internal/config.h"
#include "internal/alloc.h"
#include <string.h>
#include <math.h>

/* Bit transport must not pass signaling NaNs through floating registers. */
WASM_PRIVATE wasm_value_t wasm__f32_bits(uint32_t bits) {
    wasm_value_t result = { 0 };
    result.type = WASM_TYPE_F32;
    memcpy(&result.of.f32, &bits, sizeof(bits));
    return result;
}

WASM_PRIVATE wasm_value_t wasm__f64_bits(uint64_t bits) {
    wasm_value_t result = { 0 };
    result.type = WASM_TYPE_F64;
    memcpy(&result.of.f64, &bits, sizeof(bits));
    return result;
}

WASM_PRIVATE uintptr_t wasm__gc_i31_encode(int32_t value) {
    return ((((uintptr_t)((uint32_t)value & 0x7FFFFFFFu))) << 1) | WASM__GC_I31_TAG;
}

WASM_PRIVATE int32_t wasm__gc_i31_get_s(uintptr_t bits) {
    uint32_t raw = (uint32_t)(bits >> 1) & 0x7FFFFFFFu;

    if ((raw & 0x40000000u) != 0) raw |= 0x80000000u;
    return (int32_t)raw;
}

WASM_PRIVATE uint32_t wasm__gc_i31_get_u(uintptr_t bits) {
    return (uint32_t)(bits >> 1) & 0x7FFFFFFFu;
}

WASM_PRIVATE size_t wasm__align_up_size(size_t value, size_t alignment) {
    size_t remainder;

    if (alignment <= 1) return value;
    remainder = value % alignment;
    if (remainder == 0) return value;
    if (value > (size_t)-1 - (alignment - remainder)) return (size_t)-1;
    return value + (alignment - remainder);
}

static size_t wasm__gc_field_storage_alignment(const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) {
        return field->storage.of.packed_type == WASM_PACKED_I8 ? 1u : 2u;
    }
    return (size_t)WASM__ALIGNOF(wasm_value_t);
}

static size_t wasm__gc_field_storage_size(const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) {
        return field->storage.of.packed_type == WASM_PACKED_I8 ? 1u : 2u;
    }
    return sizeof(wasm_value_t);
}

static int wasm__gc_field_holds_refs(const wasm_fieldtype_t* field) {
    return field->storage.kind != WASM_STORAGE_PACKED && wasm__is_ref_type(field->storage.of.valtype);
}

static uint64_t wasm__align_up_u64(uint64_t value, size_t alignment) {
    uint64_t remainder = value % (uint64_t)alignment;

    return remainder == 0 ? value : value + ((uint64_t)alignment - remainder);
}

WASM_PRIVATE wasm_error_t wasm__gc_struct_layout_init(const wasm_allocator_t* a,
                                                     wasm_structtype_t* type,
                                                     int has_descriptor) {
    size_t value_align = (size_t)WASM__ALIGNOF(wasm_value_t);
    uint64_t offset = WASM__GC_STRUCT_HEADER_SIZE;
    uint32_t num_refs = 0;
    uint32_t i;

    type->field_offsets = NULL;
    type->ref_offsets = NULL;
    type->num_ref_fields = 0;
    type->object_size = 0;
    type->descriptor_offset = 0;
    for (i = 0; i < type->num_fields; i++) num_refs += (uint32_t)wasm__gc_field_holds_refs(&type->fields[i]);
    if (type->num_fields > 0) {
        type->field_offsets =
            (uint32_t*)wasm__malloc_array(a, (size_t)type->num_fields + (size_t)num_refs, sizeof(uint32_t));
        if (!type->field_offsets) return WASM_ERR_OOM;
        type->ref_offsets = type->field_offsets + type->num_fields;
    }

    for (i = 0; i < type->num_fields; i++) {
        const wasm_fieldtype_t* field = &type->fields[i];

        offset = wasm__align_up_u64(offset, wasm__gc_field_storage_alignment(field));
        type->field_offsets[i] = (uint32_t)offset;
        if (wasm__gc_field_holds_refs(field)) type->ref_offsets[type->num_ref_fields++] = (uint32_t)offset;
        offset += wasm__gc_field_storage_size(field);
    }
    offset = wasm__align_up_u64(offset, value_align);
    if (has_descriptor) offset = wasm__align_up_u64(offset + sizeof(wasm_gc_header_t*), value_align);
    if (offset > UINT32_MAX) {
        wasm__gc_struct_layout_free(a, type);
        return WASM_OK;
    }
    type->object_size = (uint32_t)offset;
    if (has_descriptor) type->descriptor_offset = (uint32_t)(offset - sizeof(wasm_gc_header_t*));
    return WASM_OK;
}

WASM_PRIVATE void wasm__gc_struct_layout_free(const wasm_allocator_t* a, wasm_structtype_t* type) {
    wasm__free(a, type->field_offsets);
    type->field_offsets = NULL;
    type->ref_offsets = NULL;
    type->num_ref_fields = 0;
    type->object_size = 0;
    type->descriptor_offset = 0;
}

WASM_PRIVATE void wasm__gc_array_layout_init(wasm_arraytype_t* type) {
    size_t alignment = wasm__gc_field_storage_alignment(&type->field);

    type->data_offset = (uint32_t)wasm__align_up_size(WASM__GC_ARRAY_HEADER_SIZE, alignment);
    type->elem_size = (uint8_t)wasm__gc_field_storage_size(&type->field);
    type->elem_align = (uint8_t)alignment;
    type->elems_hold_refs = (uint8_t)wasm__gc_field_holds_refs(&type->field);
}

WASM_PRIVATE wasm_gc_header_t** wasm__gc_descriptor_slot(wasm_gc_header_t* object) {
    if (!object->has_descriptor) return NULL;
    return (wasm_gc_header_t**)(((uint8_t*)object) + object->object_size - sizeof(wasm_gc_header_t*));
}

WASM_PRIVATE int wasm__gc_array_total_size(const wasm_arraytype_t* type,
                                     uint32_t length,
                                     size_t* out_size) {
    size_t offset = (size_t)type->data_offset;
    size_t elem_size = (size_t)type->elem_size;

    if (length != 0 && elem_size > ((size_t)-1 - offset) / (size_t)length) return 0;
    *out_size = offset + elem_size * (size_t)length;
    return 1;
}

WASM_PRIVATE wasm_value_t wasm__gc_field_default_value(const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) return wasm_i32(0);

    switch (field->storage.of.valtype) {
        case WASM_TYPE_I32:
            return wasm_i32(0);
        case WASM_TYPE_I64:
            return wasm_i64(0);
        case WASM_TYPE_F32:
            return wasm_f32(0.0f);
        case WASM_TYPE_F64:
            return wasm_f64(0.0);
        case WASM_TYPE_V128:
            return wasm_v128_zero();
        default:
            return wasm__typed_ref_null(field->storage.of.valtype, &field->storage.ref_type);
    }
}

static wasm_error_t wasm__gc_load_field_bytes(const wasm_fieldtype_t* field,
                                              const void* src,
                                              int sign_extend,
                                              wasm_value_t* out_value) {
    if (!field || !src || !out_value) return WASM_ERR_MALFORMED;

    if (field->storage.kind == WASM_STORAGE_PACKED) {
        if (field->storage.of.packed_type == WASM_PACKED_I8) {
            uint8_t raw = *(const uint8_t*)src;

            *out_value = wasm_i32(sign_extend ? (int32_t)(int8_t)raw : (int32_t)raw);
        } else {
            uint16_t raw = wasm__load_le16(src);

            *out_value = wasm_i32(sign_extend ? (int32_t)(int16_t)raw : (int32_t)raw);
        }
        return WASM_OK;
    }

    *out_value = *(const wasm_value_t*)src;
    return WASM_OK;
}

static void wasm__gc_write_field_bytes(const wasm_fieldtype_t* field, void* dst, wasm_value_t value) {
    if (field->storage.kind == WASM_STORAGE_PACKED) {
        if (field->storage.of.packed_type == WASM_PACKED_I8) {
            *(uint8_t*)dst = (uint8_t)value.of.i32;
        } else {
            wasm__store_le16(dst, (uint16_t)value.of.i32);
        }
        return;
    }
    value.type = field->storage.of.valtype;
    *(wasm_value_t*)dst = value;
}

static wasm_error_t wasm__gc_store_field_bytes(const wasm_module_t* mod,
                                               const wasm_fieldtype_t* field,
                                               void* dst,
                                               wasm_value_t value) {
    if (field->storage.kind == WASM_STORAGE_PACKED ? value.type != WASM_TYPE_I32
                                                   : !wasm__runtime_value_matches_type(mod,
                                                                                       &value,
                                                                                       field->storage.of.valtype,
                                                                                       &field->storage.ref_type))
        return WASM_ERR_TYPE_MISMATCH;
    wasm__gc_write_field_bytes(field, dst, value);
    return WASM_OK;
}

WASM_PRIVATE_INLINE wasm_error_t wasm__gc_struct_get_field(const wasm_gc_struct_t* object,
                                                   const wasm_structtype_t* type,
                                                   uint32_t field_index,
                                                   int sign_extend,
                                                   wasm_value_t* out_value) {
    if (!object) return WASM_ERR_TRAP;
    if (!type || field_index >= type->num_fields) return WASM_ERR_MALFORMED;
    return wasm__gc_load_field_bytes(&type->fields[field_index],
                                     (const uint8_t*)object + type->field_offsets[field_index],
                                     sign_extend,
                                     out_value);
}

WASM_PRIVATE wasm_error_t wasm__gc_struct_set_field(wasm_gc_struct_t* object,
                                              const wasm_structtype_t* type,
                                              uint32_t field_index,
                                              wasm_value_t value) {
    if (!object) return WASM_ERR_TRAP;
    if (!type || field_index >= type->num_fields) return WASM_ERR_MALFORMED;
    wasm__gc_write_field_bytes(&type->fields[field_index], (uint8_t*)object + type->field_offsets[field_index], value);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__gc_struct_store_field(wasm_gc_struct_t* object,
                                                const wasm_structtype_t* type,
                                                uint32_t field_index,
                                                wasm_value_t value) {
    if (!object) return WASM_ERR_TRAP;
    if (!type || field_index >= type->num_fields) return WASM_ERR_MALFORMED;
    return wasm__gc_store_field_bytes(object->header.module,
                                      &type->fields[field_index],
                                      (uint8_t*)object + type->field_offsets[field_index],
                                      value);
}

static size_t wasm__gc_array_element_offset(const wasm_arraytype_t* type, uint32_t index) {
    return (size_t)type->data_offset + (size_t)index * type->elem_size;
}

WASM_PRIVATE_INLINE wasm_error_t wasm__gc_array_get(const wasm_gc_array_t* object,
                                            const wasm_arraytype_t* type,
                                            uint32_t index,
                                            int sign_extend,
                                            wasm_value_t* out_value) {
    if (!object) return WASM_ERR_TRAP;
    if (!type) return WASM_ERR_MALFORMED;
    if (index >= object->length) return WASM_ERR_OUT_OF_BOUNDS;
    return wasm__gc_load_field_bytes(&type->field,
                                     (const uint8_t*)object + wasm__gc_array_element_offset(type, index),
                                     sign_extend,
                                     out_value);
}

WASM_PRIVATE wasm_error_t wasm__gc_array_set(wasm_gc_array_t* object,
                                       const wasm_arraytype_t* type,
                                       uint32_t index,
                                       wasm_value_t value) {
    if (!object) return WASM_ERR_TRAP;
    if (!type) return WASM_ERR_MALFORMED;
    if (index >= object->length) return WASM_ERR_OUT_OF_BOUNDS;
    wasm__gc_write_field_bytes(&type->field, (uint8_t*)object + wasm__gc_array_element_offset(type, index), value);
    return WASM_OK;
}

WASM_PRIVATE_INLINE uint32_t wasm__gc_array_len(const wasm_gc_array_t* object) {
    return object ? object->length : 0u;
}

WASM_PRIVATE wasm_error_t wasm__gc_array_store_element(wasm_gc_array_t* object,
                                                 const wasm_arraytype_t* type,
                                                 uint32_t index,
                                                 wasm_value_t value) {
    if (!object) return WASM_ERR_TRAP;
    if (!type) return WASM_ERR_MALFORMED;
    if (index >= object->length) return WASM_ERR_OUT_OF_BOUNDS;
    return wasm__gc_store_field_bytes(object->header.module,
                                      &type->field,
                                      (uint8_t*)object + wasm__gc_array_element_offset(type, index),
                                      value);
}

WASM_PRIVATE wasm_value_t wasm__gc_type_ref_value(const wasm_module_t* mod,
                                            uint32_t type_index,
                                            wasm_gc_header_t* object) {
    wasm_value_t result;

    result.type = WASM_TYPE_ANYREF;
    result.of.gc_ref = (uintptr_t)object;
    if (mod && type_index < mod->num_types) {
        switch (mod->types[type_index].kind) {
            case WASM_COMP_STRUCT:
                result.type = WASM_TYPE_STRUCTREF;
                break;
            case WASM_COMP_ARRAY:
                result.type = WASM_TYPE_ARRAYREF;
                break;
            default:
                break;
        }
    }
    return result;
}

WASM_PRIVATE int wasm__gc_array_data_element_size(const wasm_arraytype_t* type, size_t* out_size) {
    if (!type || !out_size) return 0;
    if (type->field.storage.kind == WASM_STORAGE_PACKED) {
        *out_size = type->field.storage.of.packed_type == WASM_PACKED_I8 ? 1u : 2u;
        return 1;
    }

    switch (type->field.storage.of.valtype) {
        case WASM_TYPE_I32:
        case WASM_TYPE_F32:
            *out_size = 4u;
            return 1;
        case WASM_TYPE_I64:
        case WASM_TYPE_F64:
            *out_size = 8u;
            return 1;
        case WASM_TYPE_V128:
            *out_size = 16u;
            return 1;
        default:
            return 0;
    }
}

WASM_PRIVATE wasm_error_t wasm__gc_array_read_data_value(const wasm_arraytype_t* type,
                                                   const uint8_t* src,
                                                   wasm_value_t* out_value) {
    if (!type || !src || !out_value) return WASM_ERR_MALFORMED;
    if (type->field.storage.kind == WASM_STORAGE_PACKED)
        return wasm__gc_load_field_bytes(&type->field, src, 0, out_value);

    switch (type->field.storage.of.valtype) {
        case WASM_TYPE_I32:
            *out_value = wasm_i32((int32_t)wasm__load_le32(src));
            return WASM_OK;
        case WASM_TYPE_I64:
            *out_value = wasm_i64((int64_t)wasm__load_le64(src));
            return WASM_OK;
        case WASM_TYPE_F32: {
            *out_value = wasm__f32_bits(wasm__load_le32(src));
            return WASM_OK;
        }
        case WASM_TYPE_F64: {
            *out_value = wasm__f64_bits(wasm__load_le64(src));
            return WASM_OK;
        }
        case WASM_TYPE_V128:
            *out_value = wasm_v128_zero();
            memcpy(out_value->of.v128, src, 16u);
            return WASM_OK;
        default:
            return WASM_ERR_TYPE_MISMATCH;
    }
}

WASM_PRIVATE int wasm__is_gc_ref_type(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_NOFUNC:
        case WASM_TYPE_NOEXTERN:
        case WASM_TYPE_NONE:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
            return 1;
        default:
            return 0;
    }
}

WASM_PRIVATE int wasm__is_ref_type(wasm_valtype_t type) {
    return type == WASM_TYPE_FUNCREF || type == WASM_TYPE_EXTERNREF ||
           type == WASM_TYPE_EXNREF || type == WASM_TYPE_NOEXN ||
           wasm__uses_contref_storage(type) || wasm__is_gc_ref_type(type);
}

WASM_PRIVATE int wasm__is_vector_type(wasm_valtype_t type) {
    return type == WASM_TYPE_V128;
}

static int wasm__is_numeric_type(wasm_valtype_t type) {
    return type == WASM_TYPE_I32 || type == WASM_TYPE_I64 ||
           type == WASM_TYPE_F32 || type == WASM_TYPE_F64;
}

WASM_PRIVATE int wasm__is_number_or_vector_type(wasm_valtype_t type) {
    return wasm__is_numeric_type(type) || wasm__is_vector_type(type);
}

WASM_PRIVATE int wasm__is_value_type(wasm_valtype_t type) {
    return wasm__is_number_or_vector_type(type) || wasm__is_ref_type(type);
}

WASM_PRIVATE int wasm__is_null_ref(const wasm_value_t* value) {
    if (wasm__uses_funcref_storage(value->type)) return value->of.funcref == UINT32_MAX;
    if (wasm__uses_externref_storage(value->type)) return value->of.externref == (uintptr_t)0;
    if (wasm__uses_exnref_storage(value->type)) return value->of.externref == (uintptr_t)0;
    if (wasm__uses_contref_storage(value->type)) return value->of.externref == (uintptr_t)0;
    if (wasm__uses_gc_ref_storage(value->type)) return value->of.gc_ref == (uintptr_t)0;
    return 0;
}

WASM_PRIVATE int wasm__is_heap_gc_ref_value(const wasm_value_t* value) {
    if (!value || !wasm__uses_gc_ref_storage(value->type)) return 0;
    if (value->of.gc_ref == (uintptr_t)0) return 0;
    return (value->of.gc_ref & WASM__GC_I31_TAG) == 0;
}

WASM_PRIVATE wasm_value_t wasm__typed_ref_null(wasm_valtype_t type,
                                         const wasm_reftype_t* reftype) {
    wasm_valtype_t null_type = type;

    if (reftype && reftype->has_type_index) {
        wasm_valtype_t concrete_type = reftype->type ? reftype->type : type;

        switch (concrete_type) {
            case WASM_TYPE_FUNCREF:
                null_type = WASM_TYPE_NOFUNC;
                break;
            case WASM_TYPE_EXTERNREF:
                null_type = WASM_TYPE_NOEXTERN;
                break;
            case WASM_TYPE_CONTREF:
                null_type = WASM_TYPE_NOCONT;
                break;
            default:
                null_type = WASM_TYPE_NONE;
                break;
        }
    }

    if (null_type == WASM_TYPE_VOID) null_type = WASM_TYPE_ANYREF;
    return wasm_ref_null(null_type);
}

WASM_PRIVATE wasm_value_t wasm__retag_value_for_declared_type(wasm_valtype_t declared_type,
                                                        const wasm_reftype_t* declared_reftype,
                                                        wasm_value_t value) {
    wasm_valtype_t result_type = declared_type == WASM_TYPE_VOID ? WASM_TYPE_ANYREF : declared_type;

    if (!wasm__is_ref_type(result_type) || !wasm__is_ref_type(value.type)) {
        value.type = result_type;
        return value;
    }

    if (wasm__is_valtype_subtype(NULL, value.type, result_type)) return value;

    if (wasm__is_null_ref(&value)) return wasm__typed_ref_null(result_type, declared_reftype);

    value.type = result_type;
    return value;
}

WASM_PRIVATE_INLINE int wasm__is_numeric_valtype(wasm_valtype_t type) {
    return type >= WASM_TYPE_V128 && type <= WASM_TYPE_I32;
}

WASM_PRIVATE wasm_value_t wasm__default_value(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_I32:
            return wasm_i32(0);
        case WASM_TYPE_I64:
            return wasm_i64(0);
        case WASM_TYPE_F32:
            return wasm_f32(0.0f);
        case WASM_TYPE_F64:
            return wasm_f64(0.0);
        case WASM_TYPE_V128:
            return wasm_v128_zero();
        case WASM_TYPE_NOFUNC:
        case WASM_TYPE_NOEXTERN:
        case WASM_TYPE_NOEXN:
        case WASM_TYPE_NOCONT:
        case WASM_TYPE_NONE:
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
        case WASM_TYPE_EXNREF:
        case WASM_TYPE_CONTREF:
        case WASM_TYPE_FUNCREF:
        case WASM_TYPE_EXTERNREF:
            return wasm_ref_null(type);
        default: {
            wasm_value_t result;
            memset(&result, 0, sizeof(result));
            result.type = type;
            return result;
        }
    }
}

WASM_PRIVATE int32_t wasm__trunc_sat_i32_s(double value) {
    if (isnan(value)) return 0;
    if (value <= -2147483648.0) return INT32_MIN;
    if (value >= 2147483648.0) return INT32_MAX;
    return (int32_t)value;
}

WASM_PRIVATE uint32_t wasm__trunc_sat_i32_u(double value) {
    if (isnan(value) || value <= 0.0) return 0;
    if (value >= 4294967296.0) return UINT32_MAX;
    return (uint32_t)value;
}

WASM_PRIVATE int64_t wasm__trunc_sat_i64_s(double value) {
    if (isnan(value)) return 0;
    if (value <= -9223372036854775808.0) return INT64_MIN;
    if (value >= 9223372036854775808.0) return INT64_MAX;
    return (int64_t)value;
}

WASM_PRIVATE uint64_t wasm__trunc_sat_i64_u(double value) {
    if (isnan(value) || value <= 0.0) return 0;
    if (value >= 18446744073709551616.0) return UINT64_MAX;
    return (uint64_t)value;
}

WASM_PRIVATE wasm_error_t wasm__trunc_i32_s(double value, int32_t* out_value) {
    double truncated;

    if (isnan(value))
        return WASM_ERR_INVALID_CONVERSION;
    truncated = trunc(value);
    if (truncated < -2147483648.0 || truncated >= 2147483648.0)
        return WASM_ERR_INVALID_CONVERSION;
    *out_value = (int32_t)truncated;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__trunc_i32_u(double value, uint32_t* out_value) {
    double truncated;

    if (isnan(value))
        return WASM_ERR_INVALID_CONVERSION;
    truncated = trunc(value);
    if (truncated < 0.0 || truncated >= 4294967296.0)
        return WASM_ERR_INVALID_CONVERSION;
    *out_value = (uint32_t)truncated;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__trunc_i64_s(double value, int64_t* out_value) {
    double truncated;

    if (isnan(value))
        return WASM_ERR_INVALID_CONVERSION;
    truncated = trunc(value);
    if (truncated < -9223372036854775808.0 || truncated >= 9223372036854775808.0)
        return WASM_ERR_INVALID_CONVERSION;
    *out_value = (int64_t)truncated;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__trunc_i64_u(double value, uint64_t* out_value) {
    double truncated;

    if (isnan(value))
        return WASM_ERR_INVALID_CONVERSION;
    truncated = trunc(value);
    if (truncated < 0.0 || truncated >= 18446744073709551616.0)
        return WASM_ERR_INVALID_CONVERSION;
    *out_value = (uint64_t)truncated;
    return WASM_OK;
}

static int wasm__f32_is_nan_bits(float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7FFFFFFFu) > 0x7F800000u;
}

static int wasm__f64_is_nan_bits(double value) {
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (bits & UINT64_C(0x7FFFFFFFFFFFFFFF)) > UINT64_C(0x7FF0000000000000);
}

static float wasm__quiet_f32_nan(float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    if ((bits & 0x7FFFFFFFu) > 0x7F800000u) bits |= 0x00400000u;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static double wasm__quiet_f64_nan(double value) {
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    if ((bits & UINT64_C(0x7FFFFFFFFFFFFFFF)) > UINT64_C(0x7FF0000000000000))
        bits |= UINT64_C(0x0008000000000000);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

WASM_PRIVATE float wasm__f32_min(float lhs, float rhs) {
    if (wasm__f32_is_nan_bits(lhs)) return wasm__quiet_f32_nan(lhs);
    if (wasm__f32_is_nan_bits(rhs)) return wasm__quiet_f32_nan(rhs);
    if (lhs == rhs) {
        if (lhs == 0.0f) return (signbit(lhs) || signbit(rhs)) ? -0.0f : 0.0f;
        return lhs;
    }
    return lhs < rhs ? lhs : rhs;
}

WASM_PRIVATE float wasm__f32_max(float lhs, float rhs) {
    if (wasm__f32_is_nan_bits(lhs)) return wasm__quiet_f32_nan(lhs);
    if (wasm__f32_is_nan_bits(rhs)) return wasm__quiet_f32_nan(rhs);
    if (lhs == rhs) {
        if (lhs == 0.0f) return (signbit(lhs) && signbit(rhs)) ? -0.0f : 0.0f;
        return lhs;
    }
    return lhs > rhs ? lhs : rhs;
}

WASM_PRIVATE double wasm__f64_min(double lhs, double rhs) {
    if (wasm__f64_is_nan_bits(lhs)) return wasm__quiet_f64_nan(lhs);
    if (wasm__f64_is_nan_bits(rhs)) return wasm__quiet_f64_nan(rhs);
    if (lhs == rhs) {
        if (lhs == 0.0) return (signbit(lhs) || signbit(rhs)) ? -0.0 : 0.0;
        return lhs;
    }
    return lhs < rhs ? lhs : rhs;
}

WASM_PRIVATE double wasm__f64_max(double lhs, double rhs) {
    if (wasm__f64_is_nan_bits(lhs)) return wasm__quiet_f64_nan(lhs);
    if (wasm__f64_is_nan_bits(rhs)) return wasm__quiet_f64_nan(rhs);
    if (lhs == rhs) {
        if (lhs == 0.0) return (signbit(lhs) && signbit(rhs)) ? -0.0 : 0.0;
        return lhs;
    }
    return lhs > rhs ? lhs : rhs;
}

WASM_PRIVATE float wasm__ceil_f32(float value) {
    return wasm__f32_is_nan_bits(value) ? wasm__quiet_f32_nan(value) : ceilf(value);
}

WASM_PRIVATE float wasm__floor_f32(float value) {
    return wasm__f32_is_nan_bits(value) ? wasm__quiet_f32_nan(value) : floorf(value);
}

WASM_PRIVATE float wasm__trunc_f32(float value) {
    return wasm__f32_is_nan_bits(value) ? wasm__quiet_f32_nan(value) : truncf(value);
}

WASM_PRIVATE float wasm__nearest_f32(float value) {
    return wasm__f32_is_nan_bits(value) ? wasm__quiet_f32_nan(value) : nearbyintf(value);
}

WASM_PRIVATE float wasm__sqrt_f32(float value) {
    if (wasm__f32_is_nan_bits(value)) return wasm__quiet_f32_nan(value);
#if defined(WASM__SIMD_USE_SSE2) && (defined(__i386__) || defined(_M_IX86))
    /* The i386 libm ABI may return an extended-precision x87 square root.
     * Round in the Wasm format before crossing any floating return ABI. */
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(value)));
#else
    return sqrtf(value);
#endif
}

WASM_PRIVATE double wasm__ceil_f64(double value) {
    return wasm__f64_is_nan_bits(value) ? wasm__quiet_f64_nan(value) : ceil(value);
}

WASM_PRIVATE double wasm__floor_f64(double value) {
    return wasm__f64_is_nan_bits(value) ? wasm__quiet_f64_nan(value) : floor(value);
}

WASM_PRIVATE double wasm__trunc_f64(double value) {
    return wasm__f64_is_nan_bits(value) ? wasm__quiet_f64_nan(value) : trunc(value);
}

WASM_PRIVATE double wasm__nearest_f64(double value) {
    return wasm__f64_is_nan_bits(value) ? wasm__quiet_f64_nan(value) : nearbyint(value);
}

WASM_PRIVATE double wasm__sqrt_f64(double value) {
    if (wasm__f64_is_nan_bits(value)) return wasm__quiet_f64_nan(value);
#if defined(WASM__SIMD_USE_SSE2) && (defined(__i386__) || defined(_M_IX86))
    /* The i386 libm ABI may return an extended-precision x87 square root.
     * Round in the Wasm format before crossing any floating return ABI. */
    return _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(value)));
#else
    return sqrt(value);
#endif
}

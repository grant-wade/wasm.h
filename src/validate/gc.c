#include "../internal.h"

WASM_PRIVATE wasm_error_t wasm__validator_validate_gc(wasm__validator_t* v, const uint8_t* at) {
    uint32_t subop = wasm__read_leb128_u32(&v->r);
    wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_GC);

    if (err != WASM_OK) return err;

    switch (subop) {
        case 0x00:
        case 0x01: {
            uint32_t type_idx = wasm__read_leb128_u32(&v->r);
            const wasm_structtype_t* st = wasm__module_const_structtype(v->mod, type_idx);
            wasm_valtype_t result_type;
            wasm_reftype_t result_ref;
            uint32_t i;

            if (!st)
                return wasm__validator_error(v, at, "struct instruction type index %u is not a struct type",
                                             (unsigned)type_idx);
            if (subop == 0x00) {
                for (i = st->num_fields; i > 0; i--) {
                    err = wasm__validator_pop_expect_typed(v, at,
                                                           wasm__field_stack_type(&st->fields[i - 1]),
                                                           wasm__field_stack_reftype(&st->fields[i - 1]));
                    if (err != WASM_OK) return err;
                }
            }
            err = wasm__validator_make_typeidx_reftype(v, at, type_idx, 0, &result_type, &result_ref);
            if (err != WASM_OK) return err;
            return wasm__validator_push_typed(v, at, result_type, &result_ref);
        }
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05: {
            uint32_t type_idx = wasm__read_leb128_u32(&v->r);
            uint32_t field_idx = wasm__read_leb128_u32(&v->r);
            const wasm_structtype_t* st = wasm__module_const_structtype(v->mod, type_idx);
            const wasm_fieldtype_t* field;
            wasm_valtype_t struct_ref_type;
            wasm_reftype_t struct_ref;

            if (!st)
                return wasm__validator_error(v, at, "struct instruction type index %u is not a struct type",
                                             (unsigned)type_idx);
            if (field_idx >= st->num_fields)
                return wasm__validator_error(v, at, "struct field index %u out of range", (unsigned)field_idx);
            err = wasm__validator_make_typeidx_reftype(v, at, type_idx, 1, &struct_ref_type, &struct_ref);
            if (err != WASM_OK) return err;

            field = &st->fields[field_idx];
            if (subop == 0x03 || subop == 0x04) {
                if (field->storage.kind != WASM_STORAGE_PACKED)
                    return wasm__validator_error(v, at, "struct.get_s/u requires a packed field");
            } else if (subop == 0x02 && field->storage.kind == WASM_STORAGE_PACKED) {
                return wasm__validator_error(v, at, "struct.get requires an unpacked field");
            }

            if (subop == 0x05) {
                if (!field->is_mutable)
                    return wasm__validator_error(v, at, "struct.set requires a mutable field");
                err = wasm__validator_pop_expect_typed(v, at,
                                                       wasm__field_stack_type(field),
                                                       wasm__field_stack_reftype(field));
                if (err != WASM_OK) return err;
                return wasm__validator_pop_expect_typed(v, at, struct_ref_type, &struct_ref);
            }

            err = wasm__validator_pop_expect_typed(v, at, struct_ref_type, &struct_ref);
            if (err != WASM_OK) return err;
            return wasm__validator_push_typed(v, at,
                                              wasm__field_stack_type(field),
                                              wasm__field_stack_reftype(field));
        }
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13: {
            uint32_t type_idx = wasm__read_leb128_u32(&v->r);
            const wasm_arraytype_t* atype = wasm__module_const_arraytype(v->mod, type_idx);
            wasm_valtype_t elem_type;
            const wasm_reftype_t* elem_ref_type;
            wasm_valtype_t array_ref_type;
            wasm_reftype_t array_ref;

            if (!atype)
                return wasm__validator_error(v, at, "array instruction type index %u is not an array type",
                                             (unsigned)type_idx);
            err = wasm__validator_make_typeidx_reftype(v, at, type_idx, 1, &array_ref_type, &array_ref);
            if (err != WASM_OK) return err;

            elem_type = wasm__field_stack_type(&atype->field);
            elem_ref_type = wasm__field_stack_reftype(&atype->field);
            switch (subop) {
                case 0x06:
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect_typed(v, at, elem_type, elem_ref_type);
                    if (err != WASM_OK) return err;
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                case 0x07:
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                case 0x08: {
                    uint32_t count = wasm__read_leb128_u32(&v->r);
                    uint32_t i;

                    for (i = count; i > 0; i--) {
                        err = wasm__validator_pop_expect_typed(v, at, elem_type, elem_ref_type);
                        if (err != WASM_OK) return err;
                    }
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x09: {
                    uint32_t data_idx = wasm__read_leb128_u32(&v->r);

                    v->mod->uses_data_count_section = 1;
                    if (data_idx >= v->mod->num_data_segments)
                        return wasm__validator_error(v, at, "data segment %u out of range", (unsigned)data_idx);
                    if (wasm__field_is_reference_type(&atype->field))
                        return wasm__validator_error(v, at, "array.new_data requires a non-reference element type");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x0A: {
                    uint32_t elem_idx = wasm__read_leb128_u32(&v->r);

                    if (elem_idx >= v->mod->num_elem_segments)
                        return wasm__validator_error(v, at, "element segment %u out of range", (unsigned)elem_idx);
                    if (!wasm__field_is_reference_type(&atype->field))
                        return wasm__validator_error(v, at, "array.new_elem requires a reference element type");
                    if (!wasm__validator_type_matches(v,
                                                      v->mod->elem_segments[elem_idx].elem_type,
                                                      &v->mod->elem_segments[elem_idx].elem_ref_type,
                                                      atype->field.storage.of.valtype,
                                                      &atype->field.storage.ref_type)) {
                        return wasm__validator_error(v, at, "array.new_elem type mismatch");
                    }
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    array_ref.nullable = 0;
                    return wasm__validator_push_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x0B:
                case 0x0C:
                case 0x0D:
                    if (subop != 0x0B && atype->field.storage.kind != WASM_STORAGE_PACKED)
                        return wasm__validator_error(v, at, "array.get_s/u requires a packed element type");
                    if (subop == 0x0B && atype->field.storage.kind == WASM_STORAGE_PACKED)
                        return wasm__validator_error(v, at, "array.get requires an unpacked element type");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                    if (err != WASM_OK) return err;
                    return wasm__validator_push_typed(v, at, elem_type, elem_ref_type);
                case 0x0E:
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.set requires a mutable element type");
                    err = wasm__validator_pop_expect_typed(v, at, elem_type, elem_ref_type);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                case 0x10:
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.fill requires a mutable element type");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect_typed(v, at, elem_type, elem_ref_type);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                case 0x11: {
                    uint32_t src_type_idx = wasm__read_leb128_u32(&v->r);
                    const wasm_arraytype_t* src_type = wasm__module_const_arraytype(v->mod, src_type_idx);
                    wasm_valtype_t src_array_ref_type;
                    wasm_reftype_t src_array_ref;

                    if (!src_type)
                        return wasm__validator_error(v, at, "array.copy source type index %u is not an array type",
                                                     (unsigned)src_type_idx);
                    err = wasm__validator_make_typeidx_reftype(v, at, src_type_idx, 1,
                                                               &src_array_ref_type, &src_array_ref);
                    if (err != WASM_OK) return err;
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.copy destination must be mutable");
                    if (!wasm__is_storagetype_subtype(v->mod,
                                                      &src_type->field.storage,
                                                      &atype->field.storage))
                        return wasm__validator_error(v, at, "array.copy type mismatch");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect_typed(v, at, src_array_ref_type, &src_array_ref);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x12: {
                    uint32_t data_idx = wasm__read_leb128_u32(&v->r);

                    v->mod->uses_data_count_section = 1;
                    if (data_idx >= v->mod->num_data_segments)
                        return wasm__validator_error(v, at, "data segment %u out of range", (unsigned)data_idx);
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.init_data requires a mutable element type");
                    if (wasm__field_is_reference_type(&atype->field))
                        return wasm__validator_error(v, at, "array.init_data requires a non-reference element type");
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                }
                case 0x13: {
                    uint32_t elem_idx = wasm__read_leb128_u32(&v->r);

                    if (elem_idx >= v->mod->num_elem_segments)
                        return wasm__validator_error(v, at, "element segment %u out of range", (unsigned)elem_idx);
                    if (!atype->field.is_mutable)
                        return wasm__validator_error(v, at, "array.init_elem requires a mutable element type");
                    if (!wasm__field_is_reference_type(&atype->field))
                        return wasm__validator_error(v, at, "array.init_elem requires a reference element type");
                    if (!wasm__validator_type_matches(v,
                                                      v->mod->elem_segments[elem_idx].elem_type,
                                                      &v->mod->elem_segments[elem_idx].elem_ref_type,
                                                      atype->field.storage.of.valtype,
                                                      &atype->field.storage.ref_type)) {
                        return wasm__validator_error(v, at, "array.init_elem type mismatch");
                    }
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
                    if (err != WASM_OK) return err;
                    return wasm__validator_pop_expect_typed(v, at, array_ref_type, &array_ref);
                }
                default:
                    return wasm__validator_error(v, at, "unsupported array GC opcode 0xFB 0x%X", (unsigned)subop);
            }
        }
        case 0x0F: {
            wasm_reftype_t array_ref;
            wasm__clear_reftype(&array_ref);
            array_ref.type = WASM_TYPE_ARRAYREF;
            array_ref.nullable = 1;
            err = wasm__validator_pop_expect_typed(v, at, WASM_TYPE_ARRAYREF, &array_ref);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        }
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17: {
            wasm_valtype_t target_type;
            wasm_reftype_t target_ref;
            wasm_valtype_t actual_type;
            wasm_reftype_t actual_ref;

            err = wasm__validator_read_heap_reftype(v, at, (subop == 0x15 || subop == 0x17),
                                                    &target_type, &target_ref);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_ref_castable(v, at,
                                                   target_type, &target_ref,
                                                   &actual_type, &actual_ref);
            if (err != WASM_OK) return err;
            if (subop == 0x14 || subop == 0x15)
                return wasm__validator_push(v, at, WASM_TYPE_I32);
            return wasm__validator_push_typed(v, at, target_type, &target_ref);
        }
        case 0x18:
        case 0x19: {
            uint8_t flags = wasm__read_u8(&v->r);
            uint32_t depth = wasm__read_leb128_u32(&v->r);
            const wasm__val_frame_t* target_frame;
            const wasm_valtype_t* branch_types;
            const wasm_reftype_t* branch_reftypes;
            uint32_t branch_count;
            uint32_t branch_arg_count;
            wasm_valtype_t source_type;
            wasm_reftype_t source_ref;
            wasm_valtype_t target_type;
            wasm_reftype_t target_ref;
            wasm_valtype_t diff_type;
            wasm_reftype_t diff_ref;
            wasm_valtype_t actual_type;
            wasm_reftype_t actual_ref;

            if ((flags & ~0x03u) != 0)
                return wasm__validator_error(v, at, "invalid br_on_cast flags 0x%X", (unsigned)flags);
            if (depth >= v->fp)
                return wasm__validator_error(v, at, "branch depth %u out of range", (unsigned)depth);
            err = wasm__validator_read_cast_flag_reftype(v, at, flags, 0, &source_type, &source_ref);
            if (err != WASM_OK) return err;
            err = wasm__validator_read_cast_flag_reftype(v, at, flags, 1, &target_type, &target_ref);
            if (err != WASM_OK) return err;
            if (!wasm__reftype_is_subtype(v->mod, &target_ref, &source_ref))
                return wasm__validator_error(v, at,
                                             "type mismatch: br_on_cast target type must be a subtype of source type");
            err = wasm__validator_pop_ref_subtype(v, at,
                                                  source_type, &source_ref,
                                                  &actual_type, &actual_ref);
            if (err != WASM_OK) return err;

            target_frame = &v->frames[v->fp - 1 - depth];
            wasm__validator_branch_signature(target_frame, &branch_types, &branch_reftypes, &branch_count);
            if (branch_count == 0)
                return wasm__validator_error(v, at, "br_on_cast target must accept the cast operand");
            branch_arg_count = branch_count - 1;
            err = wasm__validator_pop_and_push_types(v, at,
                                                     branch_types,
                                                     branch_reftypes,
                                                     branch_arg_count);
            if (err != WASM_OK) return err;
            if (actual_type == WASM__TYPE_BOT)
                return wasm__validator_push(v, at, WASM__TYPE_BOT);

            wasm__validator_reference_difference(&source_ref, &target_ref, &diff_type, &diff_ref);
            if (subop == 0x18) {
                if (!wasm__validator_type_matches(v,
                                                  target_type, &target_ref,
                                                  branch_types[branch_count - 1],
                                                  branch_reftypes ? &branch_reftypes[branch_count - 1] : NULL)) {
                    return wasm__validator_error(v, at, "br_on_cast target label type mismatch");
                }
                return wasm__validator_push_typed(v, at, diff_type, &diff_ref);
            }

            if (!wasm__validator_type_matches(v,
                                              diff_type, &diff_ref,
                                              branch_types[branch_count - 1],
                                              branch_reftypes ? &branch_reftypes[branch_count - 1] : NULL)) {
                return wasm__validator_error(v, at, "br_on_cast_fail target label type mismatch");
            }
            return wasm__validator_push_typed(v, at, target_type, &target_ref);
        }
        case 0x1A: {
            wasm_valtype_t actual_type;
            wasm_reftype_t actual_ref;
            wasm_reftype_t expected_ref;
            wasm_reftype_t pushed_ref;

            wasm__clear_reftype(&expected_ref);
            expected_ref.type = WASM_TYPE_EXTERNREF;
            expected_ref.nullable = 1;
            err = wasm__validator_pop_ref_subtype(v, at,
                                                  WASM_TYPE_EXTERNREF, &expected_ref,
                                                  &actual_type, &actual_ref);
            if (err != WASM_OK) return err;
            wasm__clear_reftype(&pushed_ref);
            if (actual_type != WASM__TYPE_BOT) {
                wasm__validator_get_effective_reftype(actual_type, &actual_ref, &pushed_ref);
                pushed_ref.type = WASM_TYPE_ANYREF;
            }
            return wasm__validator_push_typed(v, at, WASM_TYPE_ANYREF, &pushed_ref);
        }
        case 0x1B: {
            wasm_valtype_t actual_type;
            wasm_reftype_t actual_ref;
            wasm_reftype_t expected_ref;
            wasm_reftype_t pushed_ref;

            wasm__clear_reftype(&expected_ref);
            expected_ref.type = WASM_TYPE_ANYREF;
            expected_ref.nullable = 1;
            err = wasm__validator_pop_ref_subtype(v, at,
                                                  WASM_TYPE_ANYREF, &expected_ref,
                                                  &actual_type, &actual_ref);
            if (err != WASM_OK) return err;
            wasm__clear_reftype(&pushed_ref);
            if (actual_type != WASM__TYPE_BOT) {
                wasm__validator_get_effective_reftype(actual_type, &actual_ref, &pushed_ref);
                pushed_ref.type = WASM_TYPE_EXTERNREF;
            }
            return wasm__validator_push_typed(v, at, WASM_TYPE_EXTERNREF, &pushed_ref);
        }
        case 0x1C: {
            wasm_reftype_t i31_ref;
            err = wasm__validator_pop_expect(v, at, WASM_TYPE_I32);
            if (err != WASM_OK) return err;
            i31_ref.type = WASM_TYPE_I31REF;
            i31_ref.nullable = 0;
            i31_ref.has_type_index = 0;
            i31_ref.type_index = 0;
            return wasm__validator_push_typed(v, at, WASM_TYPE_I31REF, &i31_ref);
        }
        case 0x1D:
        case 0x1E: {
            wasm_reftype_t i31_ref;
            i31_ref.type = WASM_TYPE_I31REF;
            i31_ref.nullable = 1;
            i31_ref.has_type_index = 0;
            i31_ref.type_index = 0;
            err = wasm__validator_pop_expect_typed(v, at, WASM_TYPE_I31REF, &i31_ref);
            if (err != WASM_OK) return err;
            return wasm__validator_push(v, at, WASM_TYPE_I32);
        }
        default:
            return wasm__validator_error(v, at, "unsupported GC opcode 0xFB 0x%X", (unsigned)subop);
    }
}

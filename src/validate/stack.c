#include "stack.h"
#include "../module.h"
#include "../instructions.h"
#include "../internal/alloc.h"
#include "../memory_internal.h"
#include "../portable.h"
#include "../runtime.h"
#include "../support.h"
#include "../types/canonical.h"
#include "../types/references.h"
#include "module.h"
#include "../values.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static uint32_t wasm__validator_local_init_word_count(uint32_t num_locals) {
    return num_locals ? ((num_locals + 63u) >> 6) : 0u;
}

WASM_PRIVATE int wasm__validator_local_is_undefaulted(const wasm_func_t* func, uint32_t index) {
    const wasm_reftype_t* ref_type;

    if (!func || index >= func->num_locals || !func->local_reftypes) return 0;
    if (!wasm__is_ref_type(func->locals[index])) return 0;
    ref_type = &func->local_reftypes[index];
    return wasm__has_reftype_info(ref_type) && !ref_type->nullable;
}

WASM_PRIVATE void wasm__validator_local_init_set(uint64_t* bits, uint32_t index) {
    bits[index >> 6] |= UINT64_C(1) << (index & 63u);
}

WASM_PRIVATE int wasm__validator_local_init_get(const uint64_t* bits, uint32_t index) {
    return (bits[index >> 6] & (UINT64_C(1) << (index & 63u))) != 0;
}

static void wasm__validator_copy_local_init(uint64_t* dst,
                                            const uint64_t* src,
                                            uint32_t word_count) {
    if (word_count == 0) return;
    memcpy(dst, src, (size_t)word_count * sizeof(*dst));
}

static void wasm__validator_seed_local_init(wasm__validator_t* v,
                                            const wasm_func_t* func,
                                            const wasm_functype_t* ft) {
    uint32_t index;

    if (!v || !func || !ft || v->local_init_words == 0 || !v->frames[0].local_init) return;

    memset(v->frames[0].local_init, 0, (size_t)v->local_init_words * sizeof(*v->frames[0].local_init));
    for (index = 0; index < func->num_locals; index++) {
        if (index < ft->num_params || !wasm__validator_local_is_undefaulted(func, index))
            wasm__validator_local_init_set(v->frames[0].local_init, index);
    }
    wasm__validator_copy_local_init(v->frames[0].local_init_entry,
                                    v->frames[0].local_init,
                                    v->local_init_words);
}

WASM_PRIVATE void wasm__validator_enter_child_frame(wasm__validator_t* v, uint32_t frame_index) {
    const wasm__val_frame_t* parent;
    wasm__val_frame_t* child;

    if (!v || v->local_init_words == 0 || frame_index >= v->frame_capacity || frame_index == 0) return;
    parent = &v->frames[frame_index - 1];
    child = &v->frames[frame_index];
    wasm__validator_copy_local_init(child->local_init, parent->local_init, v->local_init_words);
    wasm__validator_copy_local_init(child->local_init_entry, parent->local_init, v->local_init_words);
}

WASM_PRIVATE void wasm__validator_restore_entry_local_init(wasm__validator_t* v, wasm__val_frame_t* frame) {
    if (!v || !frame || v->local_init_words == 0) return;
    wasm__validator_copy_local_init(frame->local_init, frame->local_init_entry, v->local_init_words);
}

WASM_PRIVATE void wasm__validator_destroy(wasm__validator_t* v) {
    if (!v) return;
    v->stack = NULL;
    v->stack_reftypes = NULL;
    v->stack_slots = NULL;
    v->stack_refs = NULL;
    v->frames = NULL;
    v->stack_capacity = 0;
    v->frame_capacity = 0;
    v->local_init_bits = NULL;
    v->local_init_entry_bits = NULL;
    v->local_init_words = 0;
}

WASM_PRIVATE wasm_error_t wasm__validator_init(wasm__validator_t* v, wasm_module_t* mod, uint32_t func_idx) {
    wasm_runtime_t* rt;
    wasm_func_t* func;
    wasm_functype_t* ft;
    uint32_t stack_capacity;
    uint32_t frame_capacity;
    uint32_t i;

    if (!v || !mod || !mod->rt) return WASM_ERR_MALFORMED;

    memset(v, 0, sizeof(*v));
    v->mod = mod;
    v->func_idx = func_idx;
    func = &mod->funcs[func_idx];
    ft = wasm__module_functype(mod, func->type_idx);
    if (!ft) return WASM_ERR_MALFORMED;
    rt = mod->rt;
    stack_capacity = rt->main_stack.max_stack ? rt->main_stack.max_stack : 1u;
    frame_capacity = rt->max_labels ? rt->max_labels : 1u;

    if (!rt->validator_stack) {
        rt->validator_stack = (wasm_valtype_t*)wasm__malloc_array(&rt->allocator, (size_t)stack_capacity, sizeof(*rt->validator_stack));
        if (!rt->validator_stack) return WASM_ERR_OOM;
    }
    if (!rt->validator_stack_reftypes) {
        rt->validator_stack_reftypes =
            (wasm_reftype_t*)wasm__malloc_array(&rt->allocator, (size_t)stack_capacity, sizeof(*rt->validator_stack_reftypes));
        if (!rt->validator_stack_reftypes) return WASM_ERR_OOM;
    }
    if (!rt->validator_stack_slots) {
        rt->validator_stack_slots =
            (uint32_t*)wasm__malloc_array(&rt->allocator, (size_t)stack_capacity + 1u, sizeof(uint32_t));
        if (!rt->validator_stack_slots) return WASM_ERR_OOM;
    }
    if (!rt->validator_stack_refs) {
        rt->validator_stack_refs =
            (uint32_t*)wasm__malloc_array(&rt->allocator, (size_t)stack_capacity + 1u, sizeof(uint32_t));
        if (!rt->validator_stack_refs) return WASM_ERR_OOM;
    }
    if (!rt->validator_frames) {
        rt->validator_frames = (wasm__val_frame_t*)wasm__calloc(&rt->allocator, frame_capacity, sizeof(*rt->validator_frames));
        if (!rt->validator_frames) return WASM_ERR_OOM;
    } else {
        memset(rt->validator_frames, 0, (size_t)frame_capacity * sizeof(*rt->validator_frames));
    }

    v->stack = rt->validator_stack;
    v->stack_reftypes = rt->validator_stack_reftypes;
    v->stack_slots = rt->validator_stack_slots;
    v->stack_refs = rt->validator_stack_refs;
    v->stack_slots[0] = 0;
    v->stack_refs[0] = 0;
    v->frames = rt->validator_frames;
    v->stack_capacity = stack_capacity;
    v->frame_capacity = frame_capacity;
    if (func->num_locals > ft->num_params) {
        for (i = ft->num_params; i < func->num_locals; i++) {
            if (wasm__validator_local_is_undefaulted(func, i)) {
                v->local_init_words = wasm__validator_local_init_word_count(func->num_locals);
                break;
            }
        }
    }
    if (v->local_init_words != 0) {
        size_t bytes;
        uint64_t total_words;

        total_words = (uint64_t)frame_capacity * (uint64_t)v->local_init_words;
        if (total_words > (uint64_t)(SIZE_MAX / sizeof(uint64_t))) return WASM_ERR_OOM;
        bytes = (size_t)total_words * sizeof(uint64_t);
        if (bytes > rt->validator_local_init_capacity) {
            uint64_t* bits = (uint64_t*)wasm__realloc(&rt->allocator, rt->validator_local_init_bits, bytes);
            uint64_t* entry_bits;

            if (!bits) return WASM_ERR_OOM;
            rt->validator_local_init_bits = bits;
            entry_bits = (uint64_t*)wasm__realloc(&rt->allocator, rt->validator_local_init_entry_bits, bytes);
            if (!entry_bits) return WASM_ERR_OOM;
            rt->validator_local_init_entry_bits = entry_bits;
            rt->validator_local_init_capacity = bytes;
        }
        v->local_init_bits = rt->validator_local_init_bits;
        v->local_init_entry_bits = rt->validator_local_init_entry_bits;
        for (i = 0; i < frame_capacity; i++) {
            v->frames[i].local_init = v->local_init_bits + (size_t)i * v->local_init_words;
            v->frames[i].local_init_entry = v->local_init_entry_bits + (size_t)i * v->local_init_words;
        }
        wasm__validator_seed_local_init(v, func, ft);
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_error(wasm__validator_t* v,
                                          const uint8_t* at,
                                          const char* fmt,
                                          ...) {
    char detail[192];
    va_list args;

    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);

    return wasm__set_validation_error(v->mod->rt,
                                      v->func_idx,
                                      (uint32_t)(at - v->body_start),
                                      "%s",
                                      detail);
}

WASM_PRIVATE wasm_error_t wasm__validator_require_feature(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    uint64_t flag) {
    wasm_error_t err = wasm__require_feature(v->mod, flag);
    if (err != WASM_OK)
        return wasm__validator_error(v, at, "%s", v->mod->rt->error_msg);
    return WASM_OK;
}

WASM_PRIVATE int wasm__validator_get_effective_reftype(wasm_valtype_t type,
                                                 const wasm_reftype_t* ref_type,
                                                 wasm_reftype_t* out) {
    return wasm__get_effective_reftype(NULL, type, ref_type, out);
}

WASM_PRIVATE int wasm__validator_type_matches(const wasm__validator_t* v,
                                        wasm_valtype_t actual,
                                        const wasm_reftype_t* actual_ref,
                                        wasm_valtype_t expected,
                                        const wasm_reftype_t* expected_ref) {
    wasm_reftype_t actual_effective;
    wasm_reftype_t expected_effective;

    if (actual == WASM__TYPE_BOT || expected == WASM__TYPE_BOT) return 1;
    if (!wasm__is_ref_type(actual) && !wasm__is_ref_type(expected))
        return wasm__is_valtype_subtype(v->mod, actual, expected);
    if (!wasm__is_ref_type(actual) || !wasm__is_ref_type(expected)) return 0;
    if (!wasm__validator_get_effective_reftype(actual, actual_ref, &actual_effective) ||
        !wasm__validator_get_effective_reftype(expected, expected_ref, &expected_effective)) {
        return 0;
    }
    return wasm__reftype_is_subtype(v->mod, &actual_effective, &expected_effective);
}

WASM_PRIVATE wasm__val_frame_t* wasm__validator_frame(wasm__validator_t* v) {
    return &v->frames[v->fp - 1];
}

WASM_PRIVATE wasm_error_t wasm__validator_pop_direct_non_null_branch_operand(wasm__validator_t* v,
                                                                       const uint8_t* at,
                                                                       wasm_valtype_t* out_type,
                                                                       wasm_reftype_t* out_source_ref,
                                                                       wasm_reftype_t* out_target_ref) {
    wasm_reftype_t actual_ref;
    wasm_error_t err = wasm__validator_pop_any_typed(v, at, out_type, &actual_ref);

    if (err != WASM_OK) return err;
    if (*out_type == WASM__TYPE_BOT) {
        wasm__clear_reftype(out_source_ref);
        wasm__clear_reftype(out_target_ref);
        return WASM_OK;
    }
    if (!wasm__validator_get_effective_reftype(*out_type, &actual_ref, out_source_ref))
        return wasm__validator_error(v, at, "instruction requires a reference operand");
    out_source_ref->nullable = 1;
    *out_target_ref = *out_source_ref;
    out_target_ref->nullable = 0;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_push(wasm__validator_t* v,
                                         const uint8_t* at,
                                         wasm_valtype_t type) {
    return wasm__validator_push_typed(v, at, type, NULL);
}

WASM_PRIVATE wasm_error_t wasm__validator_push_typed(wasm__validator_t* v,
                                               const uint8_t* at,
                                               wasm_valtype_t type,
                                               const wasm_reftype_t* ref_type) {
    if (v->sp >= v->stack_capacity)
        return wasm__validator_error(v, at, "operand stack overflow");
    v->stack_slots[v->sp + 1] = v->stack_slots[v->sp] + wasm__valtype_slots(type);
    v->stack_refs[v->sp + 1] = v->stack_refs[v->sp] + (wasm__slot_kind(type) != WASM__SLOT_KIND_NONE);
    v->stack[v->sp++] = type;
    if (v->stack_slots[v->sp] > v->max_slots) v->max_slots = v->stack_slots[v->sp];
    if (ref_type && wasm__has_reftype_info(ref_type))
        v->stack_reftypes[v->sp - 1] = *ref_type;
    else
        wasm__clear_reftype(&v->stack_reftypes[v->sp - 1]);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_push_many_typed(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    const wasm_valtype_t* types,
                                                    const wasm_reftype_t* reftypes,
                                                    uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        const wasm_reftype_t* ref_type = reftypes ? &reftypes[i] : NULL;
        wasm_error_t err = wasm__validator_push_typed(v, at, types[i], ref_type);
        if (err != WASM_OK) return err;
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_pop_any(wasm__validator_t* v,
                                            const uint8_t* at,
                                            wasm_valtype_t* out_type) {
    return wasm__validator_pop_any_typed(v, at, out_type, NULL);
}

WASM_PRIVATE wasm_error_t wasm__validator_pop_any_typed(wasm__validator_t* v,
                                                  const uint8_t* at,
                                                  wasm_valtype_t* out_type,
                                                  wasm_reftype_t* out_ref_type) {
    wasm__val_frame_t* frame = wasm__validator_frame(v);

    *out_type = WASM__TYPE_BOT;
    wasm__clear_reftype(out_ref_type);
    if (v->sp == frame->height) {
        if (!frame->unreachable)
            return wasm__validator_error(v, at, "type mismatch: stack underflow");
        *out_type = WASM__TYPE_BOT;
        wasm__clear_reftype(out_ref_type);
        return WASM_OK;
    }

    *out_type = v->stack[--v->sp];
    if (out_ref_type) *out_ref_type = v->stack_reftypes[v->sp];
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_pop_expect(wasm__validator_t* v,
                                               const uint8_t* at,
                                               wasm_valtype_t expected) {
    return wasm__validator_pop_expect_typed(v, at, expected, NULL);
}

WASM_PRIVATE wasm_error_t wasm__validator_pop_expect_typed(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     wasm_valtype_t expected,
                                                     const wasm_reftype_t* expected_ref) {
    wasm_valtype_t actual;
    wasm_reftype_t actual_ref;
    wasm_error_t err = wasm__validator_pop_any_typed(v, at, &actual, &actual_ref);
    if (err != WASM_OK) return err;
    if (!wasm__validator_type_matches(v, actual, &actual_ref, expected, expected_ref)) {
        return wasm__validator_error(v, at,
                                     "type mismatch: expected 0x%X, got 0x%X",
                                     (unsigned)expected,
                                     (unsigned)actual);
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_check_types_typed(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      const wasm_valtype_t* types,
                                                      const wasm_reftype_t* reftypes,
                                                      uint32_t count) {
    wasm__val_frame_t* frame = wasm__validator_frame(v);
    uint32_t sp = v->sp;
    uint32_t i;

    for (i = 0; i < count; i++) {
        wasm_valtype_t actual;
        wasm_reftype_t actual_ref;
        wasm_valtype_t expected = types[count - 1 - i];
        const wasm_reftype_t* expected_ref = reftypes ? &reftypes[count - 1 - i] : NULL;

        if (sp == frame->height) {
            if (!frame->unreachable)
                return wasm__validator_error(v, at, "type mismatch: stack underflow");
            actual = WASM__TYPE_BOT;
            wasm__clear_reftype(&actual_ref);
        } else {
            actual = v->stack[--sp];
            actual_ref = v->stack_reftypes[sp];
        }

        if (!wasm__validator_type_matches(v, actual, &actual_ref, expected, expected_ref)) {
            return wasm__validator_error(v, at,
                                         "type mismatch: expected 0x%X, got 0x%X",
                                         (unsigned)expected,
                                         (unsigned)actual);
        }
    }

    return WASM_OK;
}

WASM_PRIVATE uint32_t wasm__validator_stack_floor_after_checked(const wasm__validator_t* v,
                                                          uint32_t count) {
    const wasm__val_frame_t* frame = &v->frames[v->fp - 1];
    uint32_t available = v->sp - frame->height;

    if (count >= available) return frame->height;
    return v->sp - count;
}

WASM_PRIVATE void wasm__validator_consume_checked(wasm__validator_t* v, uint32_t count) {
    v->sp = wasm__validator_stack_floor_after_checked(v, count);
}

WASM_PRIVATE wasm_error_t wasm__validator_pop_and_push_types(wasm__validator_t* v,
                                                       const uint8_t* at,
                                                       const wasm_valtype_t* types,
                                                       const wasm_reftype_t* reftypes,
                                                       uint32_t count) {
    wasm_error_t err = wasm__validator_check_types_typed(v, at, types, reftypes, count);

    if (err != WASM_OK) return err;
    wasm__validator_consume_checked(v, count);
    return wasm__validator_push_many_typed(v, at, types, reftypes, count);
}

WASM_PRIVATE void wasm__validator_mark_unreachable(wasm__validator_t* v) {
    wasm__val_frame_t* frame = wasm__validator_frame(v);
    v->sp = frame->height;
    frame->unreachable = 1;
}

WASM_PRIVATE int wasm__validator_same_types(const wasm_valtype_t* a,
                                      const wasm_reftype_t* a_refs,
                                      uint32_t a_count,
                                      const wasm_valtype_t* b,
                                      const wasm_reftype_t* b_refs,
                                      uint32_t b_count) {
    wasm_reftype_t a_effective;
    wasm_reftype_t b_effective;
    uint32_t i;
    if (a_count != b_count) return 0;
    for (i = 0; i < a_count; i++) {
        if (a[i] != b[i]) return 0;
        if (wasm__is_ref_type(a[i]) || wasm__is_ref_type(b[i])) {
            wasm__validator_get_effective_reftype(a[i], a_refs ? &a_refs[i] : NULL, &a_effective);
            wasm__validator_get_effective_reftype(b[i], b_refs ? &b_refs[i] : NULL, &b_effective);
            if (!wasm__reftype_equal(&a_effective, &b_effective)) return 0;
        }
    }
    return 1;
}

WASM_PRIVATE int wasm__validator_types_match(const wasm__validator_t* v,
                                       const wasm_valtype_t* actual_types,
                                       const wasm_reftype_t* actual_refs,
                                       uint32_t actual_count,
                                       const wasm_valtype_t* expected_types,
                                       const wasm_reftype_t* expected_refs,
                                       uint32_t expected_count) {
    uint32_t i;

    if (actual_count != expected_count) return 0;
    for (i = 0; i < actual_count; i++) {
        const wasm_reftype_t* actual_ref = actual_refs ? &actual_refs[i] : NULL;
        const wasm_reftype_t* expected_ref = expected_refs ? &expected_refs[i] : NULL;

        if (!wasm__validator_type_matches(v,
                                          actual_types[i],
                                          actual_ref,
                                          expected_types[i],
                                          expected_ref)) {
            return 0;
        }
    }
    return 1;
}

static int wasm__validator_meet_type(const wasm_module_t* mod,
                                     wasm_valtype_t lhs_type,
                                     const wasm_reftype_t* lhs_ref,
                                     wasm_valtype_t rhs_type,
                                     const wasm_reftype_t* rhs_ref,
                                     wasm_valtype_t* out_type,
                                     wasm_reftype_t* out_ref) {
    wasm_reftype_t lhs_effective;
    wasm_reftype_t rhs_effective;
    wasm_reftype_t common_ref;

    if (wasm__typed_valtype_is_subtype(mod, lhs_type, lhs_ref, rhs_type, rhs_ref)) {
        *out_type = lhs_type;
        if (lhs_ref && wasm__has_reftype_info(lhs_ref))
            *out_ref = *lhs_ref;
        else
            wasm__clear_reftype(out_ref);
        return 1;
    }

    if (wasm__typed_valtype_is_subtype(mod, rhs_type, rhs_ref, lhs_type, lhs_ref)) {
        *out_type = rhs_type;
        if (rhs_ref && wasm__has_reftype_info(rhs_ref))
            *out_ref = *rhs_ref;
        else
            wasm__clear_reftype(out_ref);
        return 1;
    }

    if (lhs_type != rhs_type || !wasm__is_ref_type(lhs_type) || !wasm__is_ref_type(rhs_type)) {
        return 0;
    }

    if (!wasm__get_effective_reftype(mod, lhs_type, lhs_ref, &lhs_effective) ||
        !wasm__get_effective_reftype(mod, rhs_type, rhs_ref, &rhs_effective)) {
        return 0;
    }

    wasm__clear_reftype(&common_ref);
    common_ref.nullable = lhs_effective.nullable && rhs_effective.nullable;

    if (wasm__uses_funcref_storage(lhs_effective.type) && wasm__uses_funcref_storage(rhs_effective.type)) {
        common_ref.type = WASM_TYPE_NOFUNC;
    } else if (wasm__uses_externref_storage(lhs_effective.type) &&
               wasm__uses_externref_storage(rhs_effective.type)) {
        common_ref.type = WASM_TYPE_NOEXTERN;
    } else if (wasm__uses_exnref_storage(lhs_effective.type) &&
               wasm__uses_exnref_storage(rhs_effective.type)) {
        common_ref.type = WASM_TYPE_NOEXN;
    } else if (wasm__uses_contref_storage(lhs_effective.type) &&
               wasm__uses_contref_storage(rhs_effective.type)) {
        common_ref.type = WASM_TYPE_NOCONT;
    } else if (wasm__uses_gc_ref_storage(lhs_effective.type) && wasm__uses_gc_ref_storage(rhs_effective.type)) {
        common_ref.type = WASM_TYPE_NONE;
    } else {
        return 0;
    }

    *out_type = common_ref.type;
    *out_ref = common_ref;
    return wasm__typed_valtype_is_subtype(mod, *out_type, out_ref, lhs_type, lhs_ref) &&
           wasm__typed_valtype_is_subtype(mod, *out_type, out_ref, rhs_type, rhs_ref);
}

WASM_PRIVATE int wasm__validator_meet_types(const wasm_module_t* mod,
                                      const wasm_valtype_t* lhs_types,
                                      const wasm_reftype_t* lhs_reftypes,
                                      uint32_t lhs_count,
                                      const wasm_valtype_t* rhs_types,
                                      const wasm_reftype_t* rhs_reftypes,
                                      uint32_t rhs_count,
                                      wasm_valtype_t* out_types,
                                      wasm_reftype_t* out_reftypes) {
    uint32_t i;

    if (lhs_count != rhs_count) return 0;
    for (i = 0; i < lhs_count; i++) {
        const wasm_reftype_t* lhs_ref = lhs_reftypes ? &lhs_reftypes[i] : NULL;
        const wasm_reftype_t* rhs_ref = rhs_reftypes ? &rhs_reftypes[i] : NULL;

        if (!wasm__validator_meet_type(mod,
                                       lhs_types[i], lhs_ref,
                                       rhs_types[i], rhs_ref,
                                       &out_types[i], &out_reftypes[i])) {
            return 0;
        }
    }
    return 1;
}

WASM_PRIVATE void wasm__validator_branch_signature(const wasm__val_frame_t* frame,
                                             const wasm_valtype_t** out_types,
                                             const wasm_reftype_t** out_reftypes,
                                             uint32_t* out_count) {
    if (frame->opcode == 0x03) {
        *out_types = frame->param_types;
        *out_reftypes = frame->param_reftypes;
        *out_count = frame->num_params;
    } else {
        *out_types = frame->result_types;
        *out_reftypes = frame->result_reftypes;
        *out_count = frame->num_results;
    }
}

WASM_PRIVATE wasm_error_t wasm__validator_check_tail_results(wasm__validator_t* v,
                                                       const uint8_t* at,
                                                       const wasm_functype_t* caller_type,
                                                       const wasm_functype_t* callee_type) {
    uint32_t i;

    if (caller_type->num_results != callee_type->num_results) {
        return wasm__validator_error(v, at,
                                     "type mismatch: tail call results must match the enclosing function results");
    }
    for (i = 0; i < caller_type->num_results; i++) {
        const wasm_reftype_t* callee_ref = callee_type->result_reftypes ? &callee_type->result_reftypes[i] : NULL;
        const wasm_reftype_t* caller_ref = caller_type->result_reftypes ? &caller_type->result_reftypes[i] : NULL;

        if (!wasm__typed_valtype_is_subtype(v->mod,
                                            callee_type->results[i], callee_ref,
                                            caller_type->results[i], caller_ref)) {
            return wasm__validator_error(v, at,
                                         "type mismatch: tail call results must match the enclosing function results");
        }
    }
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_check_frame_results(wasm__validator_t* v,
                                                        const uint8_t* at,
                                                        const wasm__val_frame_t* frame,
                                                        const char* wrong_stack_message) {
    uint32_t required_height = frame->height + frame->num_results;

    if (v->sp > required_height)
        return wasm__validator_error(v, at, "%s", wrong_stack_message);
    if (!frame->unreachable && v->sp != required_height)
        return wasm__validator_error(v, at, "%s", wrong_stack_message);
    return wasm__validator_check_types_typed(v, at,
                                             frame->result_types,
                                             frame->result_reftypes,
                                             frame->num_results);
}

WASM_PRIVATE wasm_error_t wasm__validator_finish_try_arm(wasm__validator_t* v,
                                                   const uint8_t* at,
                                                   wasm__val_frame_t* frame) {
    wasm_error_t err;

    err = wasm__validator_check_frame_results(v, at, frame, "try arm leaves wrong stack height");
    if (err != WASM_OK) return err;

    v->sp = frame->height;
    wasm__validator_restore_entry_local_init(v, frame);
    frame->unreachable = 0;
    frame->in_catch = 1;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_find_rethrow_frame(wasm__validator_t* v,
                                                       uint32_t depth,
                                                       wasm__val_frame_t** out_frame) {
    wasm__val_frame_t* frame;

    if (depth >= v->fp) return WASM_ERR_MALFORMED;
    frame = &v->frames[v->fp - 1 - depth];
    if (frame->opcode != 0x06 || !frame->in_catch) return WASM_ERR_MALFORMED;
    *out_frame = frame;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_read_memarg(wasm__validator_t* v,
                                                const uint8_t* at,
                                                uint32_t max_align_log2,
                                                wasm__memarg_t* out_memarg) {
    wasm__memarg_t memarg;
    wasm_error_t err = wasm__read_memarg(&v->r, &memarg);
    const wasm_memory_t* memory;

    if (err != WASM_OK)
        return wasm__validator_error(v, at, "malformed memarg");
    if (memarg.align_log2 > max_align_log2)
        return wasm__validator_error(v, at, "alignment %u exceeds natural alignment %u",
                                     (unsigned)memarg.align_log2,
                                     (unsigned)max_align_log2);
    if (memarg.has_memory_index) {
        err = wasm__validator_require_feature(v, at, WASM_FEATURE_MULTI_MEMORY);
        if (err != WASM_OK) return err;
    }
    if (memarg.memory_index >= v->mod->num_memories)
        return wasm__validator_error(v, at, "unknown memory %u", (unsigned)memarg.memory_index);
    memory = &v->mod->memories[memarg.memory_index];
    if (memory->is_64) {
        err = wasm__validator_require_feature(v, at, WASM_FEATURE_MEMORY64);
        if (err != WASM_OK) return err;
    } else if (memarg.offset > UINT32_MAX) {
        return wasm__validator_error(v, at, "offset %llu exceeds i32 memory address space",
                                     (unsigned long long)memarg.offset);
    }
    if (out_memarg) *out_memarg = memarg;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_pop_memory_index(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     uint32_t memory_index) {
    if (memory_index >= v->mod->num_memories)
        return wasm__validator_error(v, at, "unknown memory %u", (unsigned)memory_index);
    return wasm__validator_pop_expect(v, at,
                                      wasm__memory_index_type(&v->mod->memories[memory_index]));
}

WASM_PRIVATE wasm_error_t wasm__validator_unary(wasm__validator_t* v,
                                          const uint8_t* at,
                                          wasm_valtype_t in_type,
                                          wasm_valtype_t out_type) {
    wasm_error_t err = wasm__validator_pop_expect(v, at, in_type);
    if (err != WASM_OK) return err;
    return wasm__validator_push(v, at, out_type);
}

WASM_PRIVATE wasm_error_t wasm__validator_binary(wasm__validator_t* v,
                                           const uint8_t* at,
                                           wasm_valtype_t lhs_type,
                                           wasm_valtype_t rhs_type,
                                           wasm_valtype_t out_type) {
    wasm_error_t err = wasm__validator_pop_expect(v, at, rhs_type);
    if (err != WASM_OK) return err;
    err = wasm__validator_pop_expect(v, at, lhs_type);
    if (err != WASM_OK) return err;
    return wasm__validator_push(v, at, out_type);
}

WASM_PRIVATE wasm_error_t wasm__validator_read_typed_select(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype) {
    wasm_error_t err = wasm__read_typed_select_immediate(v->mod, &v->r, out_type, out_reftype);

    if (err != WASM_OK && v->mod->rt && v->mod->rt->last_error != WASM_OK)
        return wasm__validator_error(v, at, "%s", v->mod->rt->error_msg);
    if (err != WASM_OK)
        return wasm__validator_error(v, at, "typed select requires exactly one value type");
    return WASM_OK;
}

WASM_PRIVATE wasm_valtype_t wasm__field_stack_type(const wasm_fieldtype_t* field) {
    if (field->storage.kind == WASM_STORAGE_PACKED) return WASM_TYPE_I32;
    return field->storage.of.valtype;
}

WASM_PRIVATE const wasm_reftype_t* wasm__field_stack_reftype(const wasm_fieldtype_t* field) {
    if (field->storage.kind != WASM_STORAGE_VALTYPE) return NULL;
    if (!wasm__is_ref_type(field->storage.of.valtype)) return NULL;
    return &field->storage.ref_type;
}

WASM_PRIVATE int wasm__field_is_reference_type(const wasm_fieldtype_t* field) {
    return field->storage.kind == WASM_STORAGE_VALTYPE &&
           wasm__is_ref_type(field->storage.of.valtype);
}

WASM_PRIVATE wasm_error_t wasm__validator_make_typeidx_reftype(wasm__validator_t* v,
                                                         const uint8_t* at,
                                                         uint32_t type_idx,
                                                         int nullable,
                                                         wasm_valtype_t* out_type,
                                                         wasm_reftype_t* out_reftype) {
    wasm_error_t err;

    if (type_idx >= v->mod->num_types)
        return wasm__validator_error(v, at, "type index %u out of range", (unsigned)type_idx);

    wasm__clear_reftype(out_reftype);
    out_reftype->has_type_index = 1;
    out_reftype->type_index = type_idx;
    out_reftype->nullable = nullable;
    err = wasm__resolve_reftype(v->mod, out_reftype);
    if (err != WASM_OK)
        return wasm__validator_error(v, at, "type index %u does not name a reference type",
                                     (unsigned)type_idx);
    if (out_type) *out_type = out_reftype->type;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_read_heap_reftype(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      int nullable,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype) {
    wasm_error_t err;

    if (out_type) *out_type = WASM__TYPE_BOT;
    wasm__clear_reftype(out_reftype);
    out_reftype->nullable = nullable;
    err = wasm__read_heaptype(v->mod, &v->r, out_reftype);
    if (err != WASM_OK && v->mod->rt->last_error != WASM_OK)
        return wasm__validator_error(v, at, "%s", v->mod->rt->error_msg);
    if (err != WASM_OK) return wasm__validator_error(v, at, "invalid heaptype");
    if (out_type) *out_type = out_reftype->type ? out_reftype->type : WASM_TYPE_ANYREF;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__validator_read_cast_flag_reftype(wasm__validator_t* v,
                                                           const uint8_t* at,
                                                           uint8_t flags,
                                                           uint8_t bit,
                                                           wasm_valtype_t* out_type,
                                                           wasm_reftype_t* out_reftype) {
    return wasm__validator_read_heap_reftype(v, at, (flags & (uint8_t)(1u << bit)) != 0,
                                             out_type, out_reftype);
}

WASM_PRIVATE wasm_error_t wasm__validator_pop_ref_subtype(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    wasm_valtype_t expected_type,
                                                    const wasm_reftype_t* expected_ref,
                                                    wasm_valtype_t* out_type,
                                                    wasm_reftype_t* out_ref) {
    wasm_reftype_t actual_effective;
    wasm_reftype_t expected_effective;
    wasm_error_t err = wasm__validator_pop_any_typed(v, at, out_type, out_ref);

    if (err != WASM_OK) return err;
    if (*out_type == WASM__TYPE_BOT) return WASM_OK;
    if (!wasm__is_ref_type(*out_type))
        return wasm__validator_error(v, at, "instruction requires a reference operand");
    if (!wasm__validator_get_effective_reftype(*out_type, out_ref, &actual_effective) ||
        !wasm__validator_get_effective_reftype(expected_type, expected_ref, &expected_effective) ||
        !wasm__reftype_is_subtype(v->mod, &actual_effective, &expected_effective)) {
        return wasm__validator_error(v, at,
                                     "type mismatch: expected 0x%X, got 0x%X",
                                     (unsigned)expected_type,
                                     (unsigned)*out_type);
    }
    return WASM_OK;
}

static wasm_valtype_t wasm__validator_top_type(wasm_valtype_t type) {
    switch (type) {
        case WASM_TYPE_FUNCREF:
        case WASM_TYPE_NOFUNC:
            return WASM_TYPE_FUNCREF;
        case WASM_TYPE_EXTERNREF:
        case WASM_TYPE_NOEXTERN:
            return WASM_TYPE_EXTERNREF;
        case WASM_TYPE_EXNREF:
        case WASM_TYPE_NOEXN:
            return WASM_TYPE_EXNREF;
        case WASM_TYPE_CONTREF:
        case WASM_TYPE_NOCONT:
            return WASM_TYPE_CONTREF;
        case WASM_TYPE_ANYREF:
        case WASM_TYPE_EQREF:
        case WASM_TYPE_I31REF:
        case WASM_TYPE_STRUCTREF:
        case WASM_TYPE_ARRAYREF:
        case WASM_TYPE_NONE:
            return WASM_TYPE_ANYREF;
        default:
            return WASM_TYPE_VOID;
    }
}

WASM_PRIVATE int wasm__validator_same_hierarchy(const wasm_reftype_t* lhs, const wasm_reftype_t* rhs) {
    wasm_valtype_t top = wasm__validator_top_type(lhs->type);

    return top != WASM_TYPE_VOID && top == wasm__validator_top_type(rhs->type);
}

WASM_PRIVATE wasm_error_t wasm__validator_pop_ref_castable(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     wasm_valtype_t expected_type,
                                                     const wasm_reftype_t* expected_ref,
                                                     wasm_valtype_t* out_type,
                                                     wasm_reftype_t* out_ref) {
    wasm_reftype_t actual_effective;
    wasm_reftype_t expected_effective;
    wasm_error_t err = wasm__validator_pop_any_typed(v, at, out_type, out_ref);

    if (err != WASM_OK) return err;
    if (*out_type == WASM__TYPE_BOT) return WASM_OK;
    if (!wasm__is_ref_type(*out_type))
        return wasm__validator_error(v, at, "instruction requires a reference operand");
    if (!wasm__validator_get_effective_reftype(*out_type, out_ref, &actual_effective) ||
        !wasm__validator_get_effective_reftype(expected_type, expected_ref, &expected_effective)) {
        return WASM_OK;
    }
    if (!wasm__typed_valtype_is_subtype(v->mod, *out_type, out_ref, expected_type, expected_ref) &&
        !wasm__typed_valtype_is_subtype(v->mod, expected_type, expected_ref, *out_type, out_ref) &&
        !((wasm__is_gc_ref_type(actual_effective.type) && wasm__is_gc_ref_type(expected_effective.type)) ||
          (wasm__uses_funcref_storage(actual_effective.type) && wasm__uses_funcref_storage(expected_effective.type)) ||
          (wasm__uses_externref_storage(actual_effective.type) && wasm__uses_externref_storage(expected_effective.type)) ||
          (wasm__uses_exnref_storage(actual_effective.type) && wasm__uses_exnref_storage(expected_effective.type)) ||
          (wasm__uses_contref_storage(actual_effective.type) && wasm__uses_contref_storage(expected_effective.type)))) {
        return wasm__validator_error(v, at,
                                     "type mismatch: expected 0x%X, got 0x%X",
                                     (unsigned)expected_type,
                                     (unsigned)*out_type);
    }
    return WASM_OK;
}

WASM_PRIVATE void wasm__validator_reference_difference(const wasm_reftype_t* source,
                                                 const wasm_reftype_t* target,
                                                 wasm_valtype_t* out_type,
                                                 wasm_reftype_t* out_ref) {
    wasm_reftype_t diff = *source;

    if (target->nullable) diff.nullable = 0;
    if (out_type) *out_type = diff.type;
    if (out_ref) *out_ref = diff;
}

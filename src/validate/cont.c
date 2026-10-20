#include "cont.h"
#include "../internal/slot.h"
#include "../module.h"
#include "../reader.h"
#include "../types/canonical.h"
#include "../types/references.h"
#include "stack.h"

static wasm_error_t wasm__cont_functype(wasm__validator_t* v,
                                        const uint8_t* at,
                                        uint32_t type_idx,
                                        const wasm_functype_t** out_type) {
    const wasm_module_t* mod = v->mod;

    if (type_idx >= mod->num_types) return wasm__validator_error(v, at, "unknown type %u", (unsigned)type_idx);
    if (mod->types[type_idx].kind != WASM_COMP_CONT)
        return wasm__validator_error(v, at, "non-continuation type %u", (unsigned)type_idx);
    *out_type = wasm__module_const_functype(mod, mod->types[type_idx].of.cont.func_type);
    if (!*out_type) return wasm__validator_error(v, at, "non-function type %u", (unsigned)type_idx);
    return WASM_OK;
}

static wasm_error_t wasm__cont_tag_functype(wasm__validator_t* v,
                                            const uint8_t* at,
                                            uint32_t tag_idx,
                                            const wasm_functype_t** out_type) {
    const wasm_module_t* mod = v->mod;

    if (tag_idx >= mod->num_tags) return wasm__validator_error(v, at, "unknown tag %u", (unsigned)tag_idx);
    *out_type = wasm__module_const_functype(mod, mod->tags[tag_idx].type_idx);
    if (!*out_type)
        return wasm__validator_error(v, at, "tag %u does not reference a function type", (unsigned)tag_idx);
    return WASM_OK;
}

/* Returns whether [params] -> [results] is a subtype of type. */
static int wasm__cont_signature_matches(const wasm_module_t* mod,
                                        const wasm_valtype_t* params,
                                        const wasm_reftype_t* param_refs,
                                        uint32_t num_params,
                                        const wasm_valtype_t* results,
                                        const wasm_reftype_t* result_refs,
                                        uint32_t num_results,
                                        const wasm_functype_t* type) {
    uint32_t i;

    if (num_params != type->num_params || num_results != type->num_results) return 0;
    for (i = 0; i < num_params; i++) {
        if (!wasm__typed_valtype_is_subtype(mod, type->params[i],
                                            type->param_reftypes ? &type->param_reftypes[i] : NULL, params[i],
                                            param_refs ? &param_refs[i] : NULL))
            return 0;
    }
    for (i = 0; i < num_results; i++) {
        if (!wasm__typed_valtype_is_subtype(mod, results[i], result_refs ? &result_refs[i] : NULL,
                                            type->results[i],
                                            type->result_reftypes ? &type->result_reftypes[i] : NULL))
            return 0;
    }
    return 1;
}

static wasm_error_t wasm__cont_pop_ref(wasm__validator_t* v, const uint8_t* at, uint32_t type_idx) {
    wasm_valtype_t type;
    wasm_reftype_t ref;
    wasm_error_t err = wasm__validator_make_typeidx_reftype(v, at, type_idx, 1, &type, &ref);

    if (err != WASM_OK) return err;
    return wasm__validator_pop_expect_typed(v, at, type, &ref);
}

static wasm_error_t wasm__cont_push_ref(wasm__validator_t* v, const uint8_t* at, uint32_t type_idx) {
    wasm_valtype_t type;
    wasm_reftype_t ref;
    wasm_error_t err = wasm__validator_make_typeidx_reftype(v, at, type_idx, 0, &type, &ref);

    if (err != WASM_OK) return err;
    return wasm__validator_push_typed(v, at, type, &ref);
}

static wasm_error_t wasm__cont_pop_values(wasm__validator_t* v,
                                          const uint8_t* at,
                                          const wasm_valtype_t* types,
                                          const wasm_reftype_t* refs,
                                          uint32_t count) {
    wasm_error_t err = wasm__validator_check_types_typed(v, at, types, refs, count);

    if (err != WASM_OK) return err;
    wasm__validator_consume_checked(v, count);
    return WASM_OK;
}

/* Validates an (on tag label) clause of a resume whose continuation has function type type. */
static wasm_error_t wasm__cont_check_label_handler(wasm__validator_t* v,
                                                   const uint8_t* at,
                                                   uint32_t tag_idx,
                                                   uint32_t depth,
                                                   const wasm_functype_t* type) {
    const wasm_functype_t* tag_type;
    const wasm_functype_t* handler_type;
    const wasm__val_frame_t* target;
    const wasm_valtype_t* label_types;
    const wasm_reftype_t* label_refs;
    uint32_t label_count;
    wasm_reftype_t last;
    wasm_error_t err = wasm__cont_tag_functype(v, at, tag_idx, &tag_type);

    if (err != WASM_OK) return err;
    if (depth >= v->fp) return wasm__validator_error(v, at, "unknown label %u", (unsigned)depth);
    target = &v->frames[v->fp - 1u - depth];
    wasm__validator_branch_signature(target, &label_types, &label_refs, &label_count);
    if (label_count == 0 || !wasm__validator_get_effective_reftype(label_types[label_count - 1u],
                                                                   label_refs ? &label_refs[label_count - 1u] : NULL,
                                                                   &last) ||
        !last.has_type_index) {
        return wasm__validator_error(v, at,
                                     "type mismatch: instruction requires concrete continuation reference type "
                                     "but label %u has none",
                                     (unsigned)depth);
    }
    err = wasm__cont_functype(v, at, last.type_index, &handler_type);
    if (err != WASM_OK) return err;
    if (!wasm__cont_signature_matches(v->mod, tag_type->results, tag_type->result_reftypes, tag_type->num_results,
                                      type->results, type->result_reftypes, type->num_results, handler_type))
        return wasm__validator_error(v, at, "type mismatch in continuation type");
    if (label_count != tag_type->num_params + 1u ||
        !wasm__validator_types_match(v, tag_type->params, tag_type->param_reftypes, tag_type->num_params,
                                     label_types, label_refs, tag_type->num_params))
        return wasm__validator_error(v, at, "type mismatch: handler tag %u does not match label %u",
                                     (unsigned)tag_idx, (unsigned)depth);
    if (v->stack_slots[target->height] + wasm__valtypes_slots(label_types, label_count) > v->max_slots)
        v->max_slots = v->stack_slots[target->height] + wasm__valtypes_slots(label_types, label_count);
    return WASM_OK;
}

/* Validates the handler table of a resume, resume_throw, or resume_throw_ref whose continuation has type. */
static wasm_error_t wasm__cont_check_handlers(wasm__validator_t* v, const uint8_t* at, const wasm_functype_t* type) {
    uint32_t count = wasm__read_leb128_u32(&v->r);
    uint32_t i;

    for (i = 0; i < count && !v->r.malformed; i++) {
        uint8_t kind = wasm__read_u8(&v->r);
        uint32_t tag_idx = wasm__read_leb128_u32(&v->r);
        wasm_error_t err;

        if (kind == 0x00) {
            uint32_t depth = wasm__read_leb128_u32(&v->r);

            if (v->r.malformed) break;
            err = wasm__cont_check_label_handler(v, at, tag_idx, depth, type);
        } else if (kind == 0x01) {
            const wasm_functype_t* tag_type;

            if (v->r.malformed) break;
            err = wasm__cont_tag_functype(v, at, tag_idx, &tag_type);
            if (err == WASM_OK && tag_type->num_params != 0)
                err = wasm__validator_error(v, at, "type mismatch: switch handler tag %u takes parameters",
                                            (unsigned)tag_idx);
        } else {
            return wasm__validator_error(v, at, "malformed handler kind 0x%02X", (unsigned)kind);
        }
        if (err != WASM_OK) return err;
    }
    if (v->r.malformed) return wasm__validator_error(v, at, "malformed immediate");
    return WASM_OK;
}

static wasm_error_t wasm__cont_validate_bind(wasm__validator_t* v, const uint8_t* at) {
    uint32_t type_idx = wasm__read_leb128_u32(&v->r);
    uint32_t bound_idx = wasm__read_leb128_u32(&v->r);
    const wasm_functype_t* type;
    const wasm_functype_t* bound_type;
    uint32_t bound;
    wasm_error_t err = wasm__cont_functype(v, at, type_idx, &type);

    if (err != WASM_OK) return err;
    err = wasm__cont_functype(v, at, bound_idx, &bound_type);
    if (err != WASM_OK) return err;
    if (type->num_params < bound_type->num_params)
        return wasm__validator_error(v, at, "type mismatch in continuation arguments");
    bound = type->num_params - bound_type->num_params;
    if (!wasm__cont_signature_matches(v->mod, type->params + bound,
                                      type->param_reftypes ? type->param_reftypes + bound : NULL,
                                      bound_type->num_params, type->results, type->result_reftypes,
                                      type->num_results, bound_type))
        return wasm__validator_error(v, at, "type mismatch in continuation types");
    err = wasm__cont_pop_ref(v, at, type_idx);
    if (err != WASM_OK) return err;
    err = wasm__cont_pop_values(v, at, type->params, type->param_reftypes, bound);
    if (err != WASM_OK) return err;
    return wasm__cont_push_ref(v, at, bound_idx);
}

static wasm_error_t wasm__cont_validate_switch(wasm__validator_t* v, const uint8_t* at) {
    uint32_t type_idx = wasm__read_leb128_u32(&v->r);
    uint32_t tag_idx = wasm__read_leb128_u32(&v->r);
    const wasm_functype_t* type;
    const wasm_functype_t* target_type;
    const wasm_functype_t* tag_type;
    wasm_reftype_t last;
    uint32_t args;
    wasm_error_t err = wasm__cont_functype(v, at, type_idx, &type);

    if (err != WASM_OK) return err;
    if (type->num_params == 0 ||
        !wasm__validator_get_effective_reftype(type->params[type->num_params - 1u],
                                               type->param_reftypes ? &type->param_reftypes[type->num_params - 1u]
                                                                    : NULL,
                                               &last) ||
        !last.has_type_index)
        return wasm__validator_error(v, at, "type mismatch: instruction requires continuation reference type");
    err = wasm__cont_functype(v, at, last.type_index, &target_type);
    if (err != WASM_OK) return err;
    err = wasm__cont_tag_functype(v, at, tag_idx, &tag_type);
    if (err != WASM_OK) return err;
    if (tag_type->num_params != 0) return wasm__validator_error(v, at, "type mismatch in switch tag");
    if (!wasm__validator_types_match(v, type->results, type->result_reftypes, type->num_results,
                                     tag_type->results, tag_type->result_reftypes, tag_type->num_results) ||
        !wasm__validator_types_match(v, tag_type->results, tag_type->result_reftypes, tag_type->num_results,
                                     target_type->results, target_type->result_reftypes, target_type->num_results))
        return wasm__validator_error(v, at, "type mismatch in continuation types");
    args = type->num_params - 1u;
    err = wasm__cont_pop_ref(v, at, type_idx);
    if (err != WASM_OK) return err;
    err = wasm__cont_pop_values(v, at, type->params, type->param_reftypes, args);
    if (err != WASM_OK) return err;
    return wasm__validator_push_many_typed(v, at, target_type->params, target_type->param_reftypes,
                                           target_type->num_params);
}

WASM_PRIVATE wasm_error_t wasm__validator_validate_cont(wasm__validator_t* v, const uint8_t* at, uint8_t op) {
    const wasm_functype_t* type;
    const wasm_functype_t* tag_type;
    uint32_t type_idx;
    uint32_t tag_idx;
    wasm_error_t err = wasm__validator_require_feature(v, at, WASM_FEATURE_STACK_SWITCHING);

    if (err != WASM_OK) return err;
    switch (op) {
        case 0xE0: {
            wasm_valtype_t func_type;
            wasm_reftype_t func_ref;

            type_idx = wasm__read_leb128_u32(&v->r);
            err = wasm__cont_functype(v, at, type_idx, &type);
            if (err != WASM_OK) return err;
            err = wasm__validator_make_typeidx_reftype(v, at, v->mod->types[type_idx].of.cont.func_type, 1,
                                                       &func_type, &func_ref);
            if (err != WASM_OK) return err;
            err = wasm__validator_pop_expect_typed(v, at, func_type, &func_ref);
            if (err != WASM_OK) return err;
            return wasm__cont_push_ref(v, at, type_idx);
        }
        case 0xE1:
            return wasm__cont_validate_bind(v, at);
        case 0xE2:
            tag_idx = wasm__read_leb128_u32(&v->r);
            err = wasm__cont_tag_functype(v, at, tag_idx, &tag_type);
            if (err != WASM_OK) return err;
            err = wasm__cont_pop_values(v, at, tag_type->params, tag_type->param_reftypes, tag_type->num_params);
            if (err != WASM_OK) return err;
            return wasm__validator_push_many_typed(v, at, tag_type->results, tag_type->result_reftypes,
                                                   tag_type->num_results);
        case 0xE3:
        case 0xE4:
        case 0xE5:
            type_idx = wasm__read_leb128_u32(&v->r);
            err = wasm__cont_functype(v, at, type_idx, &type);
            if (err != WASM_OK) return err;
            tag_type = NULL;
            if (op == 0xE4) {
                tag_idx = wasm__read_leb128_u32(&v->r);
                err = wasm__cont_tag_functype(v, at, tag_idx, &tag_type);
                if (err != WASM_OK) return err;
                if (tag_type->num_results != 0)
                    return wasm__validator_error(v, at, "invalid exception type: non-empty tag result type");
            }
            err = wasm__cont_check_handlers(v, at, type);
            if (err != WASM_OK) return err;
            err = wasm__cont_pop_ref(v, at, type_idx);
            if (err != WASM_OK) return err;
            if (op == 0xE3)
                err = wasm__cont_pop_values(v, at, type->params, type->param_reftypes, type->num_params);
            else if (op == 0xE4)
                err = wasm__cont_pop_values(v, at, tag_type->params, tag_type->param_reftypes, tag_type->num_params);
            else
                err = wasm__validator_pop_expect(v, at, WASM_TYPE_EXNREF);
            if (err != WASM_OK) return err;
            return wasm__validator_push_many_typed(v, at, type->results, type->result_reftypes, type->num_results);
        case 0xE6:
            return wasm__cont_validate_switch(v, at);
        default:
            return wasm__validator_error(v, at, "unknown opcode 0x%02X", (unsigned)op);
    }
}

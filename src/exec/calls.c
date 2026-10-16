#include "../internal.h"

WASM_PRIVATE void wasm__clear_label(wasm__label_t* label) {
    WASM_FREE(label->caught_values);
    label->caught_values = NULL;
    label->caught_tag = UINT32_MAX;
    label->caught_tag_identity = 0;
    label->caught_count = 0;
    label->active_catch = 0;
    label->delegate_depth = UINT32_MAX;
}

WASM_PRIVATE void wasm__clear_call_frame(wasm__call_frame_t* cf) {
    if (!cf) return;

    while (cf->lsp > 0) {
        wasm__clear_label(&cf->labels[cf->lsp - 1]);
        cf->lsp--;
    }
}

WASM_PRIVATE wasm_error_t wasm__push_call_frame(wasm_module_t* mod,
                                          uint32_t func_idx,
                                          const wasm_value_t* args,
                                          uint32_t num_args,
                                          wasm_value_t* results_dst,
                                          uint32_t num_results) {
    wasm_runtime_t* rt = mod->rt;
    wasm_error_t err;
    wasm_func_t* func;
    wasm_functype_t* ft;
    wasm__call_frame_t* cf;
    wasm_value_t* locals = NULL;
    wasm__label_t* labels;
    uint32_t max_labels;
    size_t arena_saved;
    uint32_t i;

    if (rt->call_frame_sp == 0) {
        err = wasm__ensure_module_frame_arena(mod);
        if (err != WASM_OK) return err;
    }

    if (rt->call_frame_sp >= rt->max_call_frames) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "call frame depth exceeded limit %u",
                      (unsigned)rt->max_call_frames);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }
    if (func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->is_import) return WASM_ERR_MALFORMED;
    err = wasm__prepare_function(mod, func_idx);
    if (err != WASM_OK) return err;

    if (rt->call_frame_sp == 0) {
        err = wasm__ensure_module_frame_arena(mod);
        if (err != WASM_OK) return err;
    }

    ft = wasm__module_functype(mod, func->type_idx);
    if (!ft) return WASM_ERR_MALFORMED;
    max_labels = func->max_label_depth ? func->max_label_depth : 1;
    if (max_labels > rt->max_labels) {
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED,
                      "function label depth %u exceeds limit %u",
                      (unsigned)max_labels,
                      (unsigned)rt->max_labels);
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }
    arena_saved = rt->frame_arena_offset;

    if (func->num_locals > 0) {
        locals = (wasm_value_t*)wasm__arena_alloc(rt, func->num_locals * sizeof(wasm_value_t));
        if (!locals) {
            wasm__arena_reset(rt, arena_saved);
            WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED, "%s", "frame arena exhausted");
            return WASM_ERR_CALL_STACK_EXHAUSTED;
        }
    }

    labels = (wasm__label_t*)wasm__arena_alloc(rt, max_labels * sizeof(wasm__label_t));
    if (!labels) {
        wasm__arena_reset(rt, arena_saved);
        WASM__SET_ERR(rt, WASM_ERR_CALL_STACK_EXHAUSTED, "%s", "frame arena exhausted");
        return WASM_ERR_CALL_STACK_EXHAUSTED;
    }

    cf = &rt->call_frames[rt->call_frame_sp++];
    wasm__init_call_frame(cf, mod, rt, func_idx, func, ft, locals, labels, results_dst, num_results);
    cf->arena_saved = arena_saved;

    for (i = 0; i < func->num_locals; i++) cf->locals[i] = wasm__default_value(func->locals[i]);
    for (i = 0; i < ft->num_params && i < num_args; i++) {
        cf->locals[i] = wasm__retag_value_for_declared_type(
            ft->params[i],
            ft->param_reftypes ? &ft->param_reftypes[i] : NULL,
            args[i]);
    }

    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__finish_call_frame(wasm_runtime_t* rt, uint32_t base_frame_sp) {
    wasm__call_frame_t* cf;
    wasm_value_t result_values_buf[8];
    wasm_value_t* result_values = result_values_buf;
    wasm_value_t* results_dst;
    uint32_t result_count;
    uint32_t i;
    int is_root;

    if (rt->call_frame_sp <= base_frame_sp) return WASM_ERR_MALFORMED;

    cf = &rt->call_frames[rt->call_frame_sp - 1];
    results_dst = cf->results_dst;
    result_count = cf->ft->num_results;
    is_root = (rt->call_frame_sp == base_frame_sp + 1u);

    if (result_count > 8) {
        result_values = (wasm_value_t*)WASM_MALLOC(result_count * sizeof(wasm_value_t));
        if (!result_values) return WASM_ERR_OOM;
    }

    for (i = 0; i < result_count; i++) {
        result_values[i] = wasm__retag_value_for_declared_type(
            cf->ft->results[i],
            cf->ft->result_reftypes ? &cf->ft->result_reftypes[i] : NULL,
            rt->stack[rt->sp - result_count + i]);
    }

    rt->sp = cf->sp_base;
    wasm__arena_reset(rt, cf->arena_saved);
    rt->call_frame_sp--;
    memset(cf, 0, sizeof(*cf));

    if (is_root) {
        for (i = 0; i < result_count; i++) results_dst[i] = result_values[i];
    } else {
        for (i = 0; i < result_count; i++) rt->stack[rt->sp++] = result_values[i];
    }

    if (result_values != result_values_buf) WASM_FREE(result_values);
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__handle_exception_frames(wasm_module_t* mod, uint32_t base_frame_sp) {
    wasm_runtime_t* rt = mod->rt;

    while (rt->call_frame_sp > base_frame_sp) {
        wasm__call_frame_t* cf = &rt->call_frames[rt->call_frame_sp - 1];
        wasm_error_t err;

        err = wasm__handle_exception_in_frame(cf->module, cf->labels, &cf->lsp, &cf->r, cf->sp_base);
        if (err == WASM_OK) return WASM_OK;
        if (err != WASM__ERR_EXCEPTION) return err;

        wasm__clear_call_frame(cf);
        rt->sp = cf->sp_base;
        wasm__arena_reset(rt, cf->arena_saved);
        memset(cf, 0, sizeof(*cf));
        rt->call_frame_sp--;
    }

    return WASM__ERR_EXCEPTION;
}

WASM_PRIVATE void wasm__skip_blocktype(wasm__reader_t* r) {
    uint8_t lead = *r->ptr;

    if (lead == 0x40 || wasm__is_valtype_byte(lead)) {
        if (lead == 0x40)
            r->ptr++;
        else
            wasm__skip_valtype(r);
        return;
    }

    (void)wasm__read_leb128_i32(r);
}

WASM_PRIVATE wasm_error_t wasm__read_try_table_clause(wasm__reader_t* r,
                                                wasm__try_table_clause_t* out_clause) {
    if (!r || !out_clause) return WASM_ERR_MALFORMED;

    memset(out_clause, 0, sizeof(*out_clause));
    out_clause->kind = wasm__read_u8(r);
    switch (out_clause->kind) {
        case 0x00:
        case 0x01:
            out_clause->tag_index = wasm__read_leb128_u32(r);
            out_clause->label_depth = wasm__read_leb128_u32(r);
            break;
        case 0x02:
        case 0x03:
            out_clause->label_depth = wasm__read_leb128_u32(r);
            break;
        default:
            return WASM_ERR_MALFORMED;
    }

    if (r->malformed) return WASM_ERR_MALFORMED;
    return WASM_OK;
}

WASM_PRIVATE void wasm__skip_try_table_immediates(wasm__reader_t* r) {
    uint32_t count;

    wasm__skip_blocktype(r);
    count = wasm__read_leb128_u32(r);
    while (count-- > 0 && r->ptr < r->end) {
        wasm__try_table_clause_t clause;

        if (wasm__read_try_table_clause(r, &clause) != WASM_OK) {
            r->ptr = r->end;
            r->malformed = 1;
            return;
        }
    }
}

WASM_PRIVATE void wasm__skip_prefixed_immediates(wasm__reader_t* r) {
    uint32_t subop = wasm__read_leb128_u32(r);

    switch (subop) {
        case 0x08:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x09:
            wasm__read_leb128_u32(r);
            break;
        case 0x0A:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0B:
            wasm__read_leb128_u32(r);
            break;
        case 0x0C:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0D:
            wasm__read_leb128_u32(r);
            break;
        case 0x0E:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x0F:
        case 0x10:
        case 0x11:
            wasm__read_leb128_u32(r);
            break;
        default:
            break;
    }
}

WASM_PRIVATE void wasm__skip_gc_immediates(wasm__reader_t* r) {
    uint32_t subop = wasm__read_leb128_u32(r);

    switch (subop) {
        case 0x00:
        case 0x01:
        case 0x06:
        case 0x07:
        case 0x0B:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x10:
            wasm__read_leb128_u32(r);
            break;
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x09:
        case 0x0A:
        case 0x11:
        case 0x12:
        case 0x13:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x08:
            wasm__read_leb128_u32(r);
            wasm__read_leb128_u32(r);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            wasm__skip_heaptype(r);
            break;
        case 0x18:
        case 0x19:
            if (r->ptr < r->end) r->ptr++;
            wasm__read_leb128_u32(r);
            wasm__skip_heaptype(r);
            wasm__skip_heaptype(r);
            break;
        default:
            break;
    }
}

WASM_PRIVATE wasm_error_t wasm__read_blocktype(wasm_module_t* mod, wasm__reader_t* r, wasm__blocktype_t* blocktype) {
    uint8_t lead = *r->ptr;
    wasm_error_t err;

    blocktype->param_arity = 0;
    blocktype->result_arity = 0;
    blocktype->params = NULL;
    blocktype->results = NULL;
    blocktype->param_reftypes = NULL;
    blocktype->result_reftypes = NULL;
    blocktype->inline_result = WASM_TYPE_VOID;
    wasm__clear_reftype(&blocktype->inline_result_reftype);

    if (lead == 0x40) {
        r->ptr++;
        return WASM_OK;
    }

    if (wasm__is_valtype_byte(lead)) {
        err = wasm__read_valtype(mod, r, &blocktype->inline_result, &blocktype->inline_result_reftype);
        if (err != WASM_OK) return err;
        blocktype->result_arity = 1;
        blocktype->results = &blocktype->inline_result;
        blocktype->result_reftypes = &blocktype->inline_result_reftype;
        return WASM_OK;
    }

    {
        int32_t type_idx = wasm__read_leb128_i32(r);
        wasm_functype_t* ft;

        if (wasm__require_feature(mod, WASM_FEATURE_MULTI_VALUE) != WASM_OK) return WASM_ERR_MALFORMED;
        if (type_idx < 0 || (uint32_t)type_idx >= mod->num_types) return WASM_ERR_MALFORMED;
        ft = wasm__module_functype(mod, (uint32_t)type_idx);
        if (!ft) return WASM_ERR_MALFORMED;
        blocktype->param_arity = ft->num_params;
        blocktype->result_arity = ft->num_results;
        blocktype->params = ft->params;
        blocktype->results = ft->results;
        blocktype->param_reftypes = ft->param_reftypes;
        blocktype->result_reftypes = ft->result_reftypes;
    }

    return WASM_OK;
}

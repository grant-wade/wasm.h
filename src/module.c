#include "module.h"
#include "values.h"
#include "types/references.h"
#include "runtime.h"
#include "internal/config.h"
#include "internal/alloc.h"
#include <string.h>
#include <stdio.h>

WASM_PRIVATE wasm_error_t wasm__resolve_exported_global(wasm_module_t* mod,
                                                  const char* name,
                                                  wasm_global_t** out_global,
                                                  uint32_t* out_index) {
    wasm_export_kind_t kind;
    uint32_t index;

    if (out_global) *out_global = NULL;
    if (out_index) *out_index = 0;

    if (!mod || !name) return WASM_ERR_MALFORMED;

    if (!wasm_find_export(mod, name, &kind, &index) || kind != WASM_EXPORT_GLOBAL) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_UNDEFINED_EXPORT,
                          "global export '%s' not found", name);
        return WASM_ERR_UNDEFINED_EXPORT;
    }
    if (index >= mod->num_globals) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "global export '%s' index %u out of range",
                          name, (unsigned)index);
        return WASM_ERR_MALFORMED;
    }

    if (out_global) *out_global = &mod->globals[index];
    if (out_index) *out_index = index;
    return WASM_OK;
}

WASM_PRIVATE wasm_value_t wasm__global_get_value(const wasm_global_t* global) {
    wasm_value_t value;

    value = global->is_import ? *global->import_value : global->value;
    return wasm__retag_value_for_declared_type(global->type, &global->ref_type, value);
}

WASM_PRIVATE void wasm__global_set_value(wasm_global_t* global, wasm_value_t value) {
    value = wasm__retag_value_for_declared_type(global->type, &global->ref_type, value);
    if (global->is_import) {
        *global->import_value = value;
    } else {
        global->value = value;
    }
}







WASM_PRIVATE int32_t wasm__wrap_i32_add(int32_t lhs, int32_t rhs) {
    return (int32_t)((uint32_t)lhs + (uint32_t)rhs);
}

WASM_PRIVATE int32_t wasm__wrap_i32_sub(int32_t lhs, int32_t rhs) {
    return (int32_t)((uint32_t)lhs - (uint32_t)rhs);
}

WASM_PRIVATE int32_t wasm__wrap_i32_mul(int32_t lhs, int32_t rhs) {
    return (int32_t)((uint32_t)lhs * (uint32_t)rhs);
}

WASM_PRIVATE int64_t wasm__wrap_i64_add(int64_t lhs, int64_t rhs) {
    return (int64_t)((uint64_t)lhs + (uint64_t)rhs);
}

WASM_PRIVATE int64_t wasm__wrap_i64_sub(int64_t lhs, int64_t rhs) {
    return (int64_t)((uint64_t)lhs - (uint64_t)rhs);
}

WASM_PRIVATE int64_t wasm__wrap_i64_mul(int64_t lhs, int64_t rhs) {
    return (int64_t)((uint64_t)lhs * (uint64_t)rhs);
}

WASM_PRIVATE wasm_error_t wasm__append_funcs(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_funcs;
    wasm_func_t* funcs;
    uint32_t i;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    funcs = (wasm_func_t*)WASM_REALLOC(mod->funcs, (base + count) * sizeof(wasm_func_t));
    if (!funcs) return WASM_ERR_OOM;

    mod->funcs = funcs;
    memset(mod->funcs + base, 0, count * sizeof(wasm_func_t));
    for (i = 0; i < count; i++) mod->funcs[base + i].funcref_id = UINT32_MAX;
    mod->num_funcs = base + count;
    return WASM_OK;
}

WASM_PRIVATE void wasm__free_import_info(wasm_import_info_t* info) {
    if (!info) return;
    WASM_FREE(info->module);
    WASM_FREE(info->name);
    wasm__clear_reftype(&info->ref_type);
    memset(info, 0, sizeof(*info));
}

WASM_PRIVATE wasm_error_t wasm__append_import_infos(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_imports;
    wasm_import_info_t* infos;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    infos = (wasm_import_info_t*)WASM_REALLOC(mod->imports, (base + count) * sizeof(wasm_import_info_t));
    if (!infos) return WASM_ERR_OOM;

    mod->imports = infos;
    memset(mod->imports + base, 0, count * sizeof(wasm_import_info_t));
    mod->num_imports = base + count;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__append_global_slots(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_globals;
    wasm_global_t* globals;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    globals = (wasm_global_t*)WASM_REALLOC(mod->globals, (base + count) * sizeof(wasm_global_t));
    if (!globals) return WASM_ERR_OOM;

    mod->globals = globals;
    memset(mod->globals + base, 0, count * sizeof(wasm_global_t));
    mod->num_globals = base + count;
    return WASM_OK;
}

WASM_PRIVATE void wasm__free_exception_state(wasm__exception_state_t* ex) {
    WASM_FREE(ex->values);
    memset(ex, 0, sizeof(*ex));
}

WASM_PRIVATE wasm_error_t wasm__set_exception_state(wasm__exception_state_t* ex,
                                              uint32_t tag_index,
                                              uint64_t tag_identity,
                                              const wasm_value_t* values,
                                              uint32_t count) {
    wasm_value_t* copy = NULL;

    wasm__free_exception_state(ex);
    if (count > 0) {
        copy = (wasm_value_t*)WASM_MALLOC(count * sizeof(wasm_value_t));
        if (!copy) return WASM_ERR_OOM;
        memcpy(copy, values, count * sizeof(wasm_value_t));
    }

    ex->tag_index = tag_index;
    ex->tag_identity = tag_identity;
    ex->values = copy;
    ex->num_values = count;
    ex->is_pending = 1;
    return WASM_OK;
}

WASM_PRIVATE uint64_t wasm__alloc_tag_identity(wasm_runtime_t* rt) {
    uint64_t identity;

    if (!rt) return 0;
    identity = rt->next_tag_identity;
    if (identity == 0) identity = 1;
    rt->next_tag_identity = identity + 1;
    if (rt->next_tag_identity == 0) rt->next_tag_identity = 1;
    return identity;
}

WASM_PRIVATE wasm_error_t wasm__new_exception_ref(wasm_runtime_t* rt,
                                            const wasm__exception_state_t* ex,
                                            wasm_value_t* out_value) {
    wasm__exception_object_t* object;
    wasm_value_t value;

    if (!rt || !ex || !out_value) return WASM_ERR_MALFORMED;

    object = (wasm__exception_object_t*)WASM_CALLOC(1u, sizeof(*object));
    if (!object) return WASM_ERR_OOM;
    object->tag_index = ex->tag_index;
    object->tag_identity = ex->tag_identity;
    object->num_values = ex->num_values;
    if (ex->num_values > 0) {
        object->values = (wasm_value_t*)WASM_MALLOC(ex->num_values * sizeof(wasm_value_t));
        if (!object->values) {
            WASM_FREE(object);
            return WASM_ERR_OOM;
        }
        memcpy(object->values, ex->values, ex->num_values * sizeof(wasm_value_t));
    }

    object->next = rt->exception_objects;
    rt->exception_objects = object;

    value = wasm_ref_null(WASM_TYPE_EXNREF);
    value.of.externref = (uintptr_t)object;
    *out_value = value;
    return WASM_OK;
}

WASM_PRIVATE wasm__exception_object_t* wasm__exception_object_from_exnref(const wasm_value_t* value) {
    if (!value || value->type != WASM_TYPE_EXNREF || wasm__is_null_ref(value)) return NULL;
    return (wasm__exception_object_t*)value->of.externref;
}

WASM_PRIVATE wasm_error_t wasm__append_tags(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_tags;
    wasm_tag_t* tags;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    tags = (wasm_tag_t*)WASM_REALLOC(mod->tags, (base + count) * sizeof(wasm_tag_t));
    if (!tags) return WASM_ERR_OOM;

    mod->tags = tags;
    memset(mod->tags + base, 0, count * sizeof(wasm_tag_t));
    mod->num_tags = base + count;
    return WASM_OK;
}

WASM_PRIVATE void wasm__free_data_segment(wasm_data_segment_t* segment) {
    WASM_FREE(segment->bytes);
    memset(segment, 0, sizeof(*segment));
}

WASM_PRIVATE wasm_table_t* wasm__table_actual(wasm_table_t* table) {
    while (table && table->is_import && table->import_table && table->import_table != table)
        table = table->import_table;
    return table;
}

WASM_PRIVATE const wasm_table_t* wasm__table_actual_const(const wasm_table_t* table) {
    while (table && table->is_import && table->import_table && table->import_table != table)
        table = table->import_table;
    return table;
}

static wasm_error_t wasm__runtime_register_func_ref(wasm_runtime_t* rt,
                                                    wasm_module_t* mod,
                                                    uint32_t func_idx,
                                                    uint32_t* out_ref_id) {
    uint32_t i;

    if (!rt || !mod || !out_ref_id) return WASM_ERR_MALFORMED;

    for (i = 0; i < rt->num_func_refs; i++) {
        if (rt->func_refs[i].module == mod && rt->func_refs[i].func_idx == func_idx) {
            *out_ref_id = i;
            return WASM_OK;
        }
    }

    if (rt->num_func_refs >= rt->cap_func_refs) {
        uint32_t new_cap = rt->cap_func_refs ? rt->cap_func_refs * 2u : 64u;
        wasm_func_ref_t* refs =
            (wasm_func_ref_t*)WASM_REALLOC(rt->func_refs, new_cap * sizeof(*rt->func_refs));

        if (!refs) return WASM_ERR_OOM;
        rt->func_refs = refs;
        rt->cap_func_refs = new_cap;
    }

    rt->func_refs[rt->num_func_refs].module = mod;
    rt->func_refs[rt->num_func_refs].func_idx = func_idx;
    *out_ref_id = rt->num_func_refs++;
    return WASM_OK;
}

static wasm_error_t wasm__ensure_func_ref_id(wasm_module_t* mod,
                                             uint32_t func_idx,
                                             uint32_t* out_ref_id) {
    wasm_func_t* func;
    wasm_error_t err;

    if (!mod || !mod->rt || !out_ref_id || func_idx >= mod->num_funcs) return WASM_ERR_MALFORMED;

    func = &mod->funcs[func_idx];
    if (func->funcref_id != UINT32_MAX) {
        *out_ref_id = func->funcref_id;
        return WASM_OK;
    }

    err = wasm__runtime_register_func_ref(mod->rt, mod, func_idx, &func->funcref_id);
    if (err != WASM_OK) return err;
    *out_ref_id = func->funcref_id;
    return WASM_OK;
}

WASM_PRIVATE int wasm__resolve_func_ref(const wasm_runtime_t* rt,
                                  uint32_t ref_id,
                                  wasm_module_t** out_module,
                                  uint32_t* out_func_idx) {
    if (!rt || ref_id >= rt->num_func_refs) return 0;
    if (out_module) *out_module = rt->func_refs[ref_id].module;
    if (out_func_idx) *out_func_idx = rt->func_refs[ref_id].func_idx;
    return 1;
}

WASM_PRIVATE void wasm__declare_func_ref(wasm_module_t* mod, uint32_t func_idx) {
    if (!mod || func_idx >= mod->num_funcs) return;
    mod->funcs[func_idx].is_declared_ref = 1;
}

WASM_PRIVATE wasm_error_t wasm__make_funcref(wasm_module_t* mod,
                                       uint32_t func_idx,
                                       wasm_value_t* out_value) {
    uint32_t ref_id;
    wasm_error_t err;

    if (!out_value) return WASM_ERR_MALFORMED;
    err = wasm__ensure_func_ref_id(mod, func_idx, &ref_id);
    if (err != WASM_OK) return err;
    *out_value = wasm_funcref(ref_id);
    return WASM_OK;
}

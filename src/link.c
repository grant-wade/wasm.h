#include "link.h"
#include "runtime.h"
#include "module.h"
#include "exec/frames.h"
#include "exec/interpreter.h"
#include "types/canonical.h"
#include "internal/alloc.h"
#include <string.h>

static wasm_error_t wasm__linked_func_call(wasm_module_t* caller,
                                           const wasm_value_t* args,
                                           uint32_t num_args,
                                           wasm_value_t* results,
                                           uint32_t num_results,
                                           void* userdata) {
    wasm__linked_func_t* linked = (wasm__linked_func_t*)userdata;

    if (!linked->module) {
        WASM__SET_ERR(caller->rt, WASM_ERR_TRAP, "%s", "call to a function of a freed module");
        return WASM_ERR_TRAP;
    }
    wasm__clear_backtrace(linked->module->rt);
    (void)num_results;
    return wasm__interp(linked->module, linked->func_idx, (wasm_value_t*)args, num_args, results);
}

WASM_PRIVATE int wasm__linked_func_resolve(wasm_module_t** mod, uint32_t* func_idx) {
    wasm_module_t* m = *mod;
    uint32_t idx = *func_idx;

    while (m->funcs[idx].is_import) {
        const wasm__linked_func_t* linked = (const wasm__linked_func_t*)m->funcs[idx].host_userdata;

        if (m->funcs[idx].host_func != wasm__linked_func_call || !linked->module) return 0;
        m = linked->module;
        idx = linked->func_idx;
    }
    *mod = m;
    *func_idx = idx;
    return 1;
}

WASM_PRIVATE int wasm__func_dynamic_type(const wasm_module_t* mod, uint32_t func_idx,
                                   const wasm_module_t** out_mod, uint32_t* out_type_idx) {
    const wasm_func_t* func = &mod->funcs[func_idx];

    while (func->is_inexact && !func->is_bound_exactly && func->host_func == wasm__linked_func_call) {
        const wasm__linked_func_t* linked = (const wasm__linked_func_t*)func->host_userdata;

        if (!linked->module) break;
        mod = linked->module;
        func = &mod->funcs[linked->func_idx];
    }
    *out_mod = mod;
    *out_type_idx = func->type_idx;
    return !func->is_inexact || func->is_bound_exactly;
}

WASM_PRIVATE int wasm__func_binding_type(const wasm_import_t* binding, const wasm_module_t** out_mod,
                                         uint32_t* out_type_idx) {
    const wasm__linked_func_t* linked = (const wasm__linked_func_t*)binding->userdata;

    if (binding->func == wasm__linked_func_call && linked->module)
        return wasm__func_dynamic_type(linked->module, linked->func_idx, out_mod, out_type_idx);
    *out_mod = binding->type_module;
    *out_type_idx = binding->type_index;
    return binding->func != wasm__linked_func_call;
}

static wasm_error_t wasm__register_export(wasm_runtime_t* rt, const char* module_name, wasm_module_t* mod,
                                          const char* name, wasm_export_kind_t kind, uint32_t index) {
    switch (kind) {
        case WASM_EXPORT_FUNC: {
            const wasm_functype_t* type;
            wasm__linked_func_t* linked;
            wasm_import_t desc;

            if (index >= mod->num_funcs) return WASM_ERR_MALFORMED;
            type = wasm__module_const_functype(mod, mod->funcs[index].type_idx);
            if (!type) return WASM_ERR_MALFORMED;
            linked = (wasm__linked_func_t*)wasm__calloc(&rt->allocator, 1u, sizeof(*linked));
            if (!linked) return WASM_ERR_OOM;
            linked->module = mod;
            linked->func_idx = index;
            linked->next = rt->linked_funcs;
            rt->linked_funcs = linked;
            memset(&desc, 0, sizeof(desc));
            desc.module = module_name;
            desc.name = name;
            desc.type = *type;
            desc.type_module = mod;
            desc.type_index = mod->funcs[index].type_idx;
            desc.has_type_context = 1;
            desc.func = wasm__linked_func_call;
            desc.userdata = linked;
            return wasm__register_import_internal(rt, &desc, 0);
        }
        case WASM_EXPORT_GLOBAL: {
            const wasm_reftype_t* ref_type = wasm_global_reftype(mod, index);
            wasm_global_import_t desc;

            memset(&desc, 0, sizeof(desc));
            desc.module = module_name;
            desc.name = name;
            desc.type = wasm_global_type(mod, index);
            if (ref_type) desc.ref_type = *ref_type;
            desc.type_module = mod;
            desc.is_mutable = wasm_global_is_mutable(mod, index);
            desc.value = wasm_module_global(mod, index);
            return wasm_register_global_import(rt, &desc);
        }
        case WASM_EXPORT_TABLE: {
            wasm_table_import_t desc;

            memset(&desc, 0, sizeof(desc));
            desc.module = module_name;
            desc.name = name;
            desc.table = wasm_module_table(mod, index);
            return wasm_register_table_import(rt, &desc);
        }
        case WASM_EXPORT_MEM: {
            wasm_memory_import_t desc;

            memset(&desc, 0, sizeof(desc));
            desc.module = module_name;
            desc.name = name;
            desc.memory = wasm_module_memory(mod, index);
            return wasm_register_memory_import(rt, &desc);
        }
        case WASM_EXPORT_TAG: {
            const wasm_functype_t* type = wasm_tag_functype(mod, index);
            wasm_tag_import_t desc;

            if (!type) return WASM_ERR_MALFORMED;
            memset(&desc, 0, sizeof(desc));
            desc.module = module_name;
            desc.name = name;
            desc.type = *type;
            desc.type_module = mod;
            desc.type_index = mod->tags[index].type_idx;
            desc.has_type_context = 1;
            desc.identity = mod->tags[index].identity;
            return wasm_register_tag_import(rt, &desc);
        }
        default:
            return WASM_ERR_MALFORMED;
    }
}

wasm_error_t wasm_register_module(wasm_runtime_t* rt, const char* name, wasm_module_t* mod) {
    uint32_t i;

    if (!rt || !name || !mod) return WASM_ERR_MALFORMED;
    if (mod->rt != rt || mod->is_compiled) {
        WASM__SET_ERR(rt, WASM_ERR_MALFORMED, "%s",
                      "only instances loaded in the same runtime can be registered");
        return WASM_ERR_MALFORMED;
    }
    for (i = 0; i < wasm_export_count(mod); i++) {
        size_t name_len = 0;
        const uint8_t* name_bytes = wasm_export_name_bytes(mod, i, &name_len);
        const char* export_name = wasm_export_name(mod, i);
        wasm_error_t err;

        /* C strings cannot name an export that contains NUL, so no import could match it. */
        if (name_bytes && memchr(name_bytes, '\0', name_len) != NULL) continue;
        err = wasm__register_export(rt, name, mod, export_name, wasm_export_kind(mod, i),
                                    wasm_export_index(mod, i));
        if (err != WASM_OK) {
            if (rt->last_error == WASM_OK)
                WASM__SET_ERR(rt, err, "cannot register export '%s' of module '%s'", export_name, name);
            return err;
        }
    }
    return WASM_OK;
}

#include "runtime.h"
#include "module.h"
#include "exec/frames.h"
#include "exec/interpreter.h"
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
    return wasm__interp(linked->module, linked->func_idx, (wasm_value_t*)args, num_args, results, num_results);
}

static wasm_error_t wasm__register_export(wasm_runtime_t* rt, const char* module_name, wasm_module_t* mod,
                                          const char* name, wasm_export_kind_t kind, uint32_t index) {
    switch (kind) {
        case WASM_EXPORT_FUNC: {
            const wasm_functype_t* type = wasm_func_functype(mod, index);
            wasm__linked_func_t* linked;
            wasm_import_t desc;

            if (!type) return WASM_ERR_MALFORMED;
            linked = (wasm__linked_func_t*)WASM_CALLOC(1u, sizeof(*linked));
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
            desc.is_mutable = wasm_global_is_mutable(mod, index);
            desc.value = wasm_global_value_ref_at(mod, index);
            return wasm_register_global_import(rt, &desc);
        }
        case WASM_EXPORT_TABLE: {
            wasm_table_import_t desc;

            memset(&desc, 0, sizeof(desc));
            desc.module = module_name;
            desc.name = name;
            desc.table = wasm_table_ref_at(mod, index);
            return wasm_register_table_import(rt, &desc);
        }
        case WASM_EXPORT_MEM: {
            wasm_memory_import_t desc;

            memset(&desc, 0, sizeof(desc));
            desc.module = module_name;
            desc.name = name;
            desc.memory = wasm_memory_ref_at(mod, index);
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

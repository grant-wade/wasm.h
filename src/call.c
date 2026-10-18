#include "wasm.h"
#include "runtime.h"
#include "types/canonical.h"
#include "exec/frames.h"
#include "exec/exceptions.h"
#include "exec/interpreter.h"
#include "types/references.h"
#include "module.h"
#include "support.h"
#include "values.h"
#include "internal/alloc.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

wasm_error_t wasm_call(wasm_module_t* mod, const char* func_name,
                       const wasm_value_t* args, uint32_t num_args,
                       wasm_value_t* results, uint32_t num_results) {
    uint32_t idx;
    wasm_export_kind_t kind;

    if (!mod || !mod->rt) return WASM_ERR_MALFORMED;
    if (!func_name || !wasm_find_export(mod, func_name, &kind, &idx) || kind != WASM_EXPORT_FUNC) {
        WASM__SET_ERR(mod->rt, WASM_ERR_UNDEFINED_EXPORT, "export '%s' not found", func_name ? func_name : "");
        return WASM_ERR_UNDEFINED_EXPORT;
    }
    return wasm_call_index(mod, idx, args, num_args, results, num_results);
}

wasm_error_t wasm_call_index(wasm_module_t* mod, uint32_t func_idx,
                             const wasm_value_t* args, uint32_t num_args,
                             wasm_value_t* results, uint32_t num_results) {
    wasm_functype_t* ft;

    if (!mod || !mod->rt) return WASM_ERR_MALFORMED;
    wasm__clear_backtrace(mod->rt);
    if (mod->is_compiled) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "module is compiled but not instantiated");
        return WASM_ERR_MALFORMED;
    }

    if (func_idx >= mod->num_funcs) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "function index %u out of range", (unsigned)func_idx);
        return WASM_ERR_MALFORMED;
    }

    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                      "function %u does not reference a function type",
                      (unsigned)func_idx);
        return WASM_ERR_MALFORMED;
    }
    if (num_args != ft->num_params || num_results < ft->num_results ||
        (ft->num_params > 0 && !args) || (ft->num_results > 0 && !results)) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                      "call signature mismatch: expected %u args/%u results, got %u args/%u results",
                      (unsigned)ft->num_params, (unsigned)ft->num_results,
                      (unsigned)num_args, (unsigned)num_results);
        return WASM_ERR_TYPE_MISMATCH;
    }

    for (uint32_t i = 0; i < num_args; i++) {
        if (!wasm__runtime_value_matches_type(mod, &args[i], ft->params[i],
                                             ft->param_reftypes ? &ft->param_reftypes[i] : NULL)) {
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "argument %u does not match the function parameter type", (unsigned)i);
            return WASM_ERR_TYPE_MISMATCH;
        }
    }

    {
        wasm_error_t err = wasm__interp(mod, func_idx, (wasm_value_t*)args, num_args, results, num_results);
        if (err == WASM__ERR_EXCEPTION) return wasm__report_uncaught_exception(mod->rt);
        return err;
    }
}

wasm_error_t wasm_call_ref(wasm_runtime_t* rt, wasm_value_t funcref,
                           const wasm_value_t* args, uint32_t num_args,
                           wasm_value_t* results, uint32_t num_results) {
    wasm_module_t* mod = NULL;
    uint32_t func_idx = 0;

    if (!rt) return WASM_ERR_MALFORMED;
    if (!wasm__uses_funcref_storage(funcref.type) || wasm__is_null_ref(&funcref)) {
        WASM__SET_ERR(rt, WASM_ERR_TYPE_MISMATCH, "%s", "expected a non-null funcref");
        return WASM_ERR_TYPE_MISMATCH;
    }
    if (!wasm__resolve_func_ref(rt, funcref.of.funcref, &mod, &func_idx) || !mod) {
        WASM__SET_ERR(rt, WASM_ERR_UNDEFINED_ELEMENT, "%s", "funcref does not refer to a live function");
        return WASM_ERR_UNDEFINED_ELEMENT;
    }
    return wasm_call_index(mod, func_idx, args, num_args, results, num_results);
}

static wasm_error_t wasm__resolve_exported_func(wasm_module_t* mod,
                                                const char* name,
                                                uint32_t* out_index,
                                                const wasm_functype_t** out_type) {
    wasm_export_kind_t kind;
    uint32_t index;
    const wasm_functype_t* type;

    if (out_index) *out_index = 0;
    if (out_type) *out_type = NULL;

    if (!mod || !name) return WASM_ERR_MALFORMED;

    if (!wasm_find_export(mod, name, &kind, &index) || kind != WASM_EXPORT_FUNC) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_UNDEFINED_EXPORT,
                          "function export '%s' not found", name);
        return WASM_ERR_UNDEFINED_EXPORT;
    }
    if (index >= mod->num_funcs) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "function export '%s' index %u out of range",
                          name, (unsigned)index);
        return WASM_ERR_MALFORMED;
    }

    type = wasm__module_const_functype(mod, mod->funcs[index].type_idx);
    if (!type) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "function export '%s' has no valid function type", name);
        return WASM_ERR_MALFORMED;
    }

    if (out_index) *out_index = index;
    if (out_type) *out_type = type;
    return WASM_OK;
}



static wasm_error_t wasm__fmt_char_to_type(char ch,
                                           int in_results,
                                           int* out_is_void,
                                           wasm_valtype_t* out_type,
                                           wasm_reftype_t* out_reftype) {
    if (out_is_void) *out_is_void = 0;
    if (out_reftype) wasm__clear_reftype(out_reftype);

    switch (ch) {
        case 'i':
            if (out_type) *out_type = WASM_TYPE_I32;
            return WASM_OK;
        case 'I':
            if (out_type) *out_type = WASM_TYPE_I64;
            return WASM_OK;
        case 'f':
            if (out_type) *out_type = WASM_TYPE_F32;
            return WASM_OK;
        case 'F':
            if (out_type) *out_type = WASM_TYPE_F64;
            return WASM_OK;
        case 'r':
            if (out_type) *out_type = WASM_TYPE_EXTERNREF;
            if (out_reftype) {
                out_reftype->type = WASM_TYPE_EXTERNREF;
                out_reftype->nullable = 1;
            }
            return WASM_OK;
        case 'v':
            if (!in_results) return WASM_ERR_MALFORMED;
            if (out_is_void) *out_is_void = 1;
            return WASM_OK;
        default:
            return WASM_ERR_MALFORMED;
    }
}

static wasm_error_t wasm__parse_fmt_string(const char* fmt,
                                           wasm_functype_t* out_type,
                                           const char** out_error) {
    const char* p;
    uint32_t num_params = 0;
    uint32_t num_results = 0;
    uint32_t param_index = 0;
    uint32_t result_index = 0;
    int in_results = 0;
    int saw_open = 0;
    int saw_close = 0;

    if (out_error) *out_error = "invalid format string";
    if (!fmt || !out_type) return WASM_ERR_MALFORMED;

    memset(out_type, 0, sizeof(*out_type));

    for (p = fmt; *p; p++) {
        int is_void = 0;
        wasm_error_t err;

        if (*p == '(') {
            if (saw_open || saw_close) {
                if (out_error) *out_error = "format string must contain a single result list";
                return WASM_ERR_MALFORMED;
            }
            saw_open = 1;
            in_results = 1;
            continue;
        }
        if (*p == ')') {
            if (!saw_open || saw_close) {
                if (out_error) *out_error = "format string has mismatched parentheses";
                return WASM_ERR_MALFORMED;
            }
            saw_close = 1;
            if (p[1] != '\0') {
                if (out_error) *out_error = "format string has trailing characters";
                return WASM_ERR_MALFORMED;
            }
            break;
        }

        err = wasm__fmt_char_to_type(*p, in_results, &is_void, NULL, NULL);
        if (err != WASM_OK) {
            if (out_error) *out_error = in_results ? "invalid result format specifier" : "invalid parameter format specifier";
            return err;
        }
        if (!in_results)
            num_params++;
        else if (is_void) {
            if (num_results != 0 || p[1] != ')') {
                if (out_error) *out_error = "'v' must be the only result specifier";
                return WASM_ERR_MALFORMED;
            }
        } else {
            num_results++;
        }
    }

    if (!saw_open || !saw_close) {
        if (out_error) *out_error = "format string must use args(results) syntax";
        return WASM_ERR_MALFORMED;
    }

    out_type->num_params = num_params;
    out_type->num_results = num_results;
    if (num_params > 0) {
        out_type->params = (wasm_valtype_t*)wasm__malloc_array(num_params, sizeof(wasm_valtype_t));
        out_type->param_reftypes = (wasm_reftype_t*)WASM_CALLOC(num_params, sizeof(wasm_reftype_t));
        if (!out_type->params || !out_type->param_reftypes) {
            if (out_error) *out_error = "out of memory while parsing parameter types";
            wasm__free_functype(out_type);
            return WASM_ERR_OOM;
        }
    }
    if (num_results > 0) {
        out_type->results = (wasm_valtype_t*)wasm__malloc_array(num_results, sizeof(wasm_valtype_t));
        out_type->result_reftypes = (wasm_reftype_t*)WASM_CALLOC(num_results, sizeof(wasm_reftype_t));
        if (!out_type->results || !out_type->result_reftypes) {
            if (out_error) *out_error = "out of memory while parsing result types";
            wasm__free_functype(out_type);
            return WASM_ERR_OOM;
        }
    }

    in_results = 0;
    for (p = fmt; *p; p++) {
        int is_void = 0;
        wasm_error_t err;
        wasm_valtype_t type;
        wasm_reftype_t ref_type;

        if (*p == '(') {
            in_results = 1;
            continue;
        }
        if (*p == ')') break;

        err = wasm__fmt_char_to_type(*p, in_results, &is_void, &type, &ref_type);
        if (err != WASM_OK) {
            wasm__free_functype(out_type);
            if (out_error) *out_error = "invalid format string";
            return err;
        }
        if (!in_results) {
            out_type->params[param_index] = type;
            out_type->param_reftypes[param_index++] = ref_type;
        } else if (!is_void) {
            out_type->results[result_index] = type;
            out_type->result_reftypes[result_index++] = ref_type;
        }
    }

    return WASM_OK;
}

static wasm_error_t wasm__set_fmt_error(wasm_runtime_t* rt,
                                        wasm_error_t err,
                                        const char* prefix,
                                        const char* detail) {
    if (rt && detail)
        WASM__SET_ERR(rt, err, "%s: %s", prefix, detail);
    else if (rt)
        WASM__SET_ERR(rt, err, "%s", prefix);
    return err;
}

static wasm_error_t wasm__call_fmt_va(wasm_module_t* mod,
                                      const char* func_name,
                                      const char* fmt,
                                      va_list ap) {
    const wasm_functype_t* actual_type;
    wasm_functype_t parsed_type;
    wasm_value_t* args = NULL;
    wasm_value_t* results = NULL;
    void** result_ptrs = NULL;
    const char* parse_error = NULL;
    uint32_t func_index;
    uint32_t i;
    wasm_error_t err;

    memset(&parsed_type, 0, sizeof(parsed_type));
    if (!mod || !func_name || !fmt)
        return wasm__set_fmt_error(mod ? mod->rt : NULL, WASM_ERR_MALFORMED,
                                   "invalid format call arguments", NULL);

    err = wasm__parse_fmt_string(fmt, &parsed_type, &parse_error);
    if (err != WASM_OK)
        return wasm__set_fmt_error(mod->rt, err, "invalid format string", parse_error);

    err = wasm__resolve_exported_func(mod, func_name, &func_index, &actual_type);
    if (err != WASM_OK) goto cleanup;

    if (!wasm__functype_equal(&parsed_type, actual_type)) {
        err = wasm__set_fmt_error(mod->rt, WASM_ERR_TYPE_MISMATCH,
                                  "format signature mismatch",
                                  "format string does not match exported function signature");
        goto cleanup;
    }

    if (parsed_type.num_params > 0) {
        args = (wasm_value_t*)wasm__malloc_array(parsed_type.num_params, sizeof(wasm_value_t));
        if (!args) {
            err = wasm__set_fmt_error(mod->rt, WASM_ERR_OOM, "call_fmt allocation failed", "args");
            goto cleanup;
        }
    }
    if (parsed_type.num_results > 0) {
        results = (wasm_value_t*)wasm__malloc_array(parsed_type.num_results, sizeof(wasm_value_t));
        result_ptrs = (void**)wasm__malloc_array(parsed_type.num_results, sizeof(void*));
        if (!results || !result_ptrs) {
            err = wasm__set_fmt_error(mod->rt, WASM_ERR_OOM, "call_fmt allocation failed", "results");
            goto cleanup;
        }
    }

    for (i = 0; i < parsed_type.num_params; i++) {
        switch (parsed_type.params[i]) {
            case WASM_TYPE_I32:
                args[i] = wasm_i32(va_arg(ap, int32_t));
                break;
            case WASM_TYPE_I64:
                args[i] = wasm_i64(va_arg(ap, int64_t));
                break;
            case WASM_TYPE_F32:
                args[i] = wasm_f32((float)va_arg(ap, double));
                break;
            case WASM_TYPE_F64:
                args[i] = wasm_f64(va_arg(ap, double));
                break;
            case WASM_TYPE_EXTERNREF:
                args[i] = wasm_externref((uintptr_t)va_arg(ap, void*));
                break;
            default:
                err = wasm__set_fmt_error(mod->rt, WASM_ERR_MALFORMED,
                                          "unsupported format argument type", NULL);
                goto cleanup;
        }
    }

    for (i = 0; i < parsed_type.num_results; i++) {
        switch (parsed_type.results[i]) {
            case WASM_TYPE_I32:
                result_ptrs[i] = va_arg(ap, int32_t*);
                break;
            case WASM_TYPE_I64:
                result_ptrs[i] = va_arg(ap, int64_t*);
                break;
            case WASM_TYPE_F32:
                result_ptrs[i] = va_arg(ap, float*);
                break;
            case WASM_TYPE_F64:
                result_ptrs[i] = va_arg(ap, double*);
                break;
            case WASM_TYPE_EXTERNREF:
                result_ptrs[i] = va_arg(ap, void**);
                break;
            default:
                result_ptrs[i] = NULL;
                break;
        }
        if (!result_ptrs[i]) {
            err = wasm__set_fmt_error(mod->rt, WASM_ERR_MALFORMED,
                                      "invalid format result destination", NULL);
            goto cleanup;
        }
    }

    err = wasm_call_index(mod, func_index, args, parsed_type.num_params, results, parsed_type.num_results);
    if (err != WASM_OK) goto cleanup;

    for (i = 0; i < parsed_type.num_results; i++) {
        switch (parsed_type.results[i]) {
            case WASM_TYPE_I32:
                *(int32_t*)result_ptrs[i] = results[i].of.i32;
                break;
            case WASM_TYPE_I64:
                *(int64_t*)result_ptrs[i] = results[i].of.i64;
                break;
            case WASM_TYPE_F32:
                *(float*)result_ptrs[i] = results[i].of.f32;
                break;
            case WASM_TYPE_F64:
                *(double*)result_ptrs[i] = results[i].of.f64;
                break;
            case WASM_TYPE_EXTERNREF:
                *(void**)result_ptrs[i] = (void*)results[i].of.externref;
                break;
            default:
                err = wasm__set_fmt_error(mod->rt, WASM_ERR_MALFORMED,
                                          "unsupported format result type", NULL);
                goto cleanup;
        }
    }

cleanup:
    WASM_FREE(args);
    WASM_FREE(results);
    WASM_FREE(result_ptrs);
    wasm__free_functype(&parsed_type);
    return err;
}

wasm_error_t wasm_call_fmt(wasm_module_t* mod, const char* func_name, const char* fmt, ...) {
    va_list ap;
    wasm_error_t err;

    va_start(ap, fmt);
    err = wasm__call_fmt_va(mod, func_name, fmt, ap);
    va_end(ap);
    return err;
}

wasm_error_t wasm_bind_host_func(wasm_runtime_t* rt, const char* module_name,
                                 const char* func_name, const char* fmt,
                                 wasm_host_func_t callback, void* userdata) {
    wasm_import_t imp;
    wasm_functype_t type;
    const char* parse_error = NULL;
    wasm_error_t err;

    if (!rt || !module_name || !func_name || !fmt || !callback)
        return wasm__set_fmt_error(rt, WASM_ERR_MALFORMED,
                                   "invalid host function binding arguments", NULL);

    memset(&type, 0, sizeof(type));
    err = wasm__parse_fmt_string(fmt, &type, &parse_error);
    if (err != WASM_OK)
        return wasm__set_fmt_error(rt, err, "invalid format string", parse_error);

    memset(&imp, 0, sizeof(imp));
    imp.module = module_name;
    imp.name = func_name;
    imp.type = type;
    imp.func = callback;
    imp.userdata = userdata;

    err = wasm__register_import_internal(rt, &imp, 0);
    wasm__free_functype(&type);
    return err;
}

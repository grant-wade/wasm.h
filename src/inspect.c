#include "wasm.h"
#include "values.h"
#include "runtime.h"
#include "exec/frames.h"
#include "support.h"
#include "types/references.h"
#include "types/canonical.h"
#include "module.h"
#include "reader.h"
#include "internal/alloc.h"
#include "internal/gc_types.h"
#include <stdio.h>

static const char* wasm__gc_kind_name(wasm_gc_object_kind_t kind) {
    switch (kind) {
        case WASM_GC_OBJ_STRUCT:
            return "struct";
        case WASM_GC_OBJ_ARRAY:
            return "array";
        default:
            return "gc";
    }
}

static wasm_error_t wasm__public_resolve_gc_object(const wasm_module_t* mod,
                                                   const wasm_value_t* ref,
                                                   wasm_gc_object_kind_t expected_kind,
                                                   const wasm_gc_header_t** out_object) {
    const wasm_gc_header_t* object;

    if (!mod || !ref || !out_object) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid GC access arguments");
        return WASM_ERR_MALFORMED;
    }

    if (!wasm__uses_gc_ref_storage(ref->type)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "expected %s reference, got value type 0x%X",
                          wasm__gc_kind_name(expected_kind),
                          (unsigned)ref->type);
        return WASM_ERR_TYPE_MISMATCH;
    }

    if (wasm__is_null_ref(ref)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TRAP,
                          "null %s reference", wasm__gc_kind_name(expected_kind));
        return WASM_ERR_TRAP;
    }

    if ((ref->of.gc_ref & WASM__GC_I31_TAG) != 0) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "expected %s heap object reference",
                          wasm__gc_kind_name(expected_kind));
        return WASM_ERR_TYPE_MISMATCH;
    }

    object = wasm__runtime_get_gc_object(mod, ref);
    if (!object) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TRAP,
                          "invalid %s reference", wasm__gc_kind_name(expected_kind));
        return WASM_ERR_TRAP;
    }

    if (object->kind != expected_kind) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "expected %s reference", wasm__gc_kind_name(expected_kind));
        return WASM_ERR_TYPE_MISMATCH;
    }

    if (!object->module || object->type_index >= object->module->num_types) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "invalid %s type metadata", wasm__gc_kind_name(expected_kind));
        return WASM_ERR_MALFORMED;
    }

    *out_object = object;
    return WASM_OK;
}

wasm_error_t wasm_struct_get_field(const wasm_module_t* mod, wasm_value_t struct_ref,
                                   uint32_t field_idx, wasm_value_t* out_val) {
    const wasm_gc_header_t* header;
    const wasm_structtype_t* type;
    wasm_error_t err;

    if (!out_val) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid struct get arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__public_resolve_gc_object(mod, &struct_ref, WASM_GC_OBJ_STRUCT, &header);
    if (err != WASM_OK) return err;

    type = wasm__module_const_structtype(header->module, header->type_index);
    if (!type) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid struct type metadata");
        return WASM_ERR_MALFORMED;
    }

    return wasm__gc_struct_get_field((const wasm_gc_struct_t*)header, type, field_idx, 0, out_val);
}

wasm_error_t wasm_struct_set_field(wasm_module_t* mod, wasm_value_t struct_ref,
                                   uint32_t field_idx, wasm_value_t val) {
    const wasm_gc_header_t* header;
    const wasm_structtype_t* type;
    wasm_error_t err;

    err = wasm__public_resolve_gc_object(mod, &struct_ref, WASM_GC_OBJ_STRUCT, &header);
    if (err != WASM_OK) return err;

    type = wasm__module_const_structtype(header->module, header->type_index);
    if (!type) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid struct type metadata");
        return WASM_ERR_MALFORMED;
    }
    if (field_idx >= type->num_fields) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "struct field %u out of range", (unsigned)field_idx);
        return WASM_ERR_MALFORMED;
    }
    if (!type->fields[field_idx].is_mutable) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "struct field %u is immutable", (unsigned)field_idx);
        return WASM_ERR_TYPE_MISMATCH;
    }

    return wasm__gc_struct_set_field((wasm_gc_struct_t*)header, type, field_idx, val);
}

wasm_error_t wasm_array_length(const wasm_module_t* mod, wasm_value_t array_ref,
                               uint32_t* out_len) {
    const wasm_gc_header_t* header;
    wasm_error_t err;

    if (!out_len) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid array length arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__public_resolve_gc_object(mod, &array_ref, WASM_GC_OBJ_ARRAY, &header);
    if (err != WASM_OK) return err;

    *out_len = ((const wasm_gc_array_t*)header)->length;
    return WASM_OK;
}

wasm_error_t wasm_array_get_elem(const wasm_module_t* mod, wasm_value_t array_ref,
                                 uint32_t index, wasm_value_t* out_val) {
    const wasm_gc_header_t* header;
    const wasm_arraytype_t* type;
    wasm_error_t err;

    if (!out_val) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid array get arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__public_resolve_gc_object(mod, &array_ref, WASM_GC_OBJ_ARRAY, &header);
    if (err != WASM_OK) return err;

    type = wasm__module_const_arraytype(header->module, header->type_index);
    if (!type) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid array type metadata");
        return WASM_ERR_MALFORMED;
    }

    return wasm__gc_array_get((const wasm_gc_array_t*)header, type, index, 0, out_val);
}

wasm_error_t wasm_array_set_elem(wasm_module_t* mod, wasm_value_t array_ref,
                                 uint32_t index, wasm_value_t val) {
    const wasm_gc_header_t* header;
    const wasm_arraytype_t* type;
    wasm_error_t err;

    err = wasm__public_resolve_gc_object(mod, &array_ref, WASM_GC_OBJ_ARRAY, &header);
    if (err != WASM_OK) return err;

    type = wasm__module_const_arraytype(header->module, header->type_index);
    if (!type) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid array type metadata");
        return WASM_ERR_MALFORMED;
    }
    if (!type->field.is_mutable) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "%s", "array element is immutable");
        return WASM_ERR_TYPE_MISMATCH;
    }

    return wasm__gc_array_set((wasm_gc_array_t*)header, type, index, val);
}

/* Reads the module and function names from the "name" custom section. Names that are malformed,
 * repeated, or for missing functions are skipped, and parsing stops at the first malformed part. */
static void wasm__load_names(wasm_module_t* mod) {
    const uint8_t* data;
    size_t len = 0;
    wasm__reader_t r;

    if (mod->names_loaded) return;
    mod->names_loaded = 1;
    data = wasm_custom_section_find(mod, "name", &len);
    if (!data) return;
    r.ptr = data;
    r.end = data + len;
    r.malformed = 0;
    while (r.ptr < r.end && !r.malformed) {
        uint8_t id = wasm__read_u8(&r);
        uint32_t size = wasm__read_leb128_u32(&r);
        wasm__reader_t sub;

        if (r.malformed || !wasm__has(&r, size)) return;
        sub.ptr = r.ptr;
        sub.end = r.ptr + size;
        sub.malformed = 0;
        r.ptr += size;
        if (id == 0u && !mod->module_name) {
            if (wasm__read_name_owned(&mod->rt->allocator, &sub, &mod->module_name) != WASM_OK) return;
        } else if (id == 1u) {
            uint32_t count = wasm__read_count(&sub);
            uint32_t i;

            if (sub.malformed || mod->num_funcs == 0) continue;
            if (!mod->func_names) {
                mod->func_names = (char**)wasm__calloc(&mod->rt->allocator, mod->num_funcs, sizeof(char*));
                if (!mod->func_names) return;
            }
            for (i = 0; i < count; i++) {
                uint32_t func_idx = wasm__read_leb128_u32(&sub);
                char* name = NULL;

                if (sub.malformed || wasm__read_name_owned(&mod->rt->allocator, &sub, &name) != WASM_OK) return;
                if (func_idx < mod->num_funcs && !mod->func_names[func_idx])
                    mod->func_names[func_idx] = name;
                else
                    wasm__free(&mod->rt->allocator, name);
            }
        }
    }
}

const char* wasm_func_name(const wasm_module_t* mod, uint32_t func_idx) {
    if (!mod || func_idx >= mod->num_funcs) return NULL;
    wasm__load_names((wasm_module_t*)mod);
    return mod->func_names ? mod->func_names[func_idx] : NULL;
}

const char* wasm_module_name(const wasm_module_t* mod) {
    if (!mod) return NULL;
    wasm__load_names((wasm_module_t*)mod);
    return mod->module_name;
}

void wasm_runtime_backtrace(const wasm_runtime_t* rt, char* buffer, size_t buffer_size) {
    uint32_t depth;
    size_t used = 0;
    uint32_t i;

    if (!buffer || buffer_size == 0) return;

    buffer[0] = '\0';
    if (!rt) return;

    depth = wasm__visible_call_stack_depth(rt);
    for (i = 0; i < depth; i++) {
        wasm_module_t* mod = NULL;
        uint32_t func_idx;
        uint32_t offset;
        const char* module_name;
        const char* func_name;
        int written;

        if (!wasm__visible_call_frame_info(rt, i, &mod, &func_idx, &offset)) break;
        module_name = wasm_module_name(mod);
        func_name = wasm_func_name(mod, func_idx);
        written = wasm__snprintf(buffer + used, buffer_size - used,
                                 "#%u %s%s%s%sfunc[%u]%s at offset 0x%04X%s",
                                 (unsigned)i,
                                 module_name ? module_name : "",
                                 module_name ? "!" : "",
                                 func_name ? func_name : "",
                                 func_name ? " (" : "",
                                 (unsigned)func_idx,
                                 func_name ? ")" : "",
                                 (unsigned)offset,
                                 (i + 1u < depth) ? "\n" : "");
        if (written < 0) break;
        if ((size_t)written >= buffer_size - used) {
            used = buffer_size - 1u;
            break;
        }
        used += (size_t)written;
    }

    buffer[used] = '\0';
}

uint32_t wasm_runtime_call_depth(const wasm_runtime_t* rt) {
    return wasm__visible_call_stack_depth(rt);
}

wasm_error_t wasm_runtime_call_frame(const wasm_runtime_t* rt, uint32_t depth, wasm_module_t** out_module,
                                      uint32_t* out_func_idx, uint32_t* out_offset) {
    if (!rt || !wasm__visible_call_frame_info(rt, depth, out_module, out_func_idx, out_offset))
        return WASM_ERR_MALFORMED;
    return WASM_OK;
}

void wasm_runtime_set_fuel(wasm_runtime_t* rt, uint64_t fuel) {
    if (!rt) return;

    rt->fuel = fuel;
    rt->fuel_enabled = 1;
}

void wasm_runtime_disable_fuel(wasm_runtime_t* rt) {
    if (!rt) return;

    rt->fuel = 0;
    rt->fuel_enabled = 0;
}

uint64_t wasm_runtime_fuel(const wasm_runtime_t* rt) {
    return rt ? rt->fuel : 0;
}

wasm_runtime_t* wasm_module_runtime(const wasm_module_t* mod) {
    return mod ? mod->rt : NULL;
}

void wasm_module_set_userdata(wasm_module_t* mod, void* userdata) {
    if (!mod) return;
    mod->host_userdata = userdata;
}

void* wasm_module_userdata(const wasm_module_t* mod) {
    return mod ? mod->host_userdata : NULL;
}

wasm_error_t wasm_struct_new(wasm_module_t* mod, uint32_t type_idx,
                             const wasm_value_t* field_values, uint32_t num_fields,
                             wasm_value_t* out_ref) {
    const wasm_structtype_t* st;
    wasm_gc_struct_t* object;
    uint32_t i;

    if (!mod || !out_ref) return WASM_ERR_MALFORMED;

    st = wasm__module_const_structtype(mod, type_idx);
    if (!st) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "type %u is not a struct type", (unsigned)type_idx);
        return WASM_ERR_TYPE_MISMATCH;
    }
    if (num_fields != st->num_fields || (num_fields > 0 && !field_values)) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                      "struct type %u expects %u fields, got %u",
                      (unsigned)type_idx,
                      (unsigned)st->num_fields,
                      (unsigned)num_fields);
        return WASM_ERR_TYPE_MISMATCH;
    }

    object = wasm__gc_alloc_struct_object(mod->rt, mod, type_idx, st);
    if (!object) return WASM_ERR_OOM;

    for (i = 0; i < st->num_fields; i++) {
        wasm_error_t err = wasm__gc_struct_store_field(object, st, i, field_values[i]);
        if (err != WASM_OK) return err;
    }

    *out_ref = wasm__gc_type_ref_value(mod, type_idx, &object->header);
    return WASM_OK;
}

wasm_error_t wasm_array_new(wasm_module_t* mod, uint32_t type_idx,
                            const wasm_value_t* elem_values, uint32_t num_elems,
                            wasm_value_t* out_ref) {
    const wasm_arraytype_t* at;
    wasm_gc_array_t* object;
    uint32_t i;

    if (!mod || !out_ref) return WASM_ERR_MALFORMED;

    at = wasm__module_const_arraytype(mod, type_idx);
    if (!at) {
        WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH, "type %u is not an array type", (unsigned)type_idx);
        return WASM_ERR_TYPE_MISMATCH;
    }
    if (num_elems > 0 && !elem_values) {
        WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                      "array values are required for %u elements",
                      (unsigned)num_elems);
        return WASM_ERR_MALFORMED;
    }

    object = wasm__gc_alloc_array_object(mod->rt, mod, type_idx, at, num_elems);
    if (!object) return WASM_ERR_OOM;

    for (i = 0; i < num_elems; i++) {
        wasm_error_t err = wasm__gc_array_store_element(object, at, i, elem_values[i]);
        if (err != WASM_OK) return err;
    }

    *out_ref = wasm__gc_type_ref_value(mod, type_idx, &object->header);
    return WASM_OK;
}

const char* wasm_error_string(wasm_error_t err) {
    switch (err) {
        case WASM_OK:
            return "ok";
        case WASM_ERR_INVALID_MAGIC:
            return "invalid magic number";
        case WASM_ERR_INVALID_VERSION:
            return "invalid version";
        case WASM_ERR_MALFORMED:
            return "malformed module";
        case WASM_ERR_INVALID_SECTION:
            return "invalid section";
        case WASM_ERR_TYPE_MISMATCH:
            return "type mismatch";
        case WASM_ERR_UNKNOWN_IMPORT:
            return "unknown import";
        case WASM_ERR_UNDEFINED_EXPORT:
            return "undefined export";
        case WASM_ERR_STACK_OVERFLOW:
            return "stack overflow";
        case WASM_ERR_STACK_UNDERFLOW:
            return "stack underflow";
        case WASM_ERR_OUT_OF_BOUNDS:
            return "out of bounds memory access";
        case WASM_ERR_UNREACHABLE:
            return "unreachable executed";
        case WASM_ERR_INDIRECT_CALL_TYPE_MISMATCH:
            return "indirect call type mismatch";
        case WASM_ERR_UNDEFINED_ELEMENT:
            return "undefined element";
        case WASM_ERR_UNINITIALIZED_TABLE:
            return "uninitialized table element";
        case WASM_ERR_DIV_BY_ZERO:
            return "integer divide by zero";
        case WASM_ERR_INT_OVERFLOW:
            return "integer overflow";
        case WASM_ERR_INVALID_CONVERSION:
            return "invalid conversion to integer";
        case WASM_ERR_CALL_STACK_EXHAUSTED:
            return "call stack exhausted";
        case WASM_ERR_OOM:
            return "out of memory";
        case WASM_ERR_OUT_OF_FUEL:
            return "out of fuel";
        case WASM_ERR_TRAP:
            return "trap";
        case WASM_ERR_INTERRUPTED:
            return "interrupted";
        case WASM_ERR_EXIT:
            return "exit";
    }
    return "unknown error";
}

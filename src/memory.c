#include "internal.h"

uint32_t wasm_memory_count(wasm_module_t* mod) {
    return mod ? mod->num_memories : 0;
}

wasm_error_t wasm_memory_read(wasm_module_t* mod, uint32_t memory_index,
                              uint64_t offset, void* dst, size_t len) {
    wasm_memory_t* memory;
    uint64_t mem_size;

    if (!mod || (len > 0 && !dst)) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid memory read arguments");
        return WASM_ERR_MALFORMED;
    }

    memory = wasm__memory_at(mod, memory_index);
    if (!memory) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "unknown memory %u", (unsigned)memory_index);
        return WASM_ERR_MALFORMED;
    }

    mem_size = wasm__memory_byte_size(memory);
    if (!wasm__range_in_bounds_u64(offset, (uint64_t)len, mem_size)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_OUT_OF_BOUNDS,
                          "memory %u read out of bounds: offset=%llu len=%zu size=%llu",
                          (unsigned)memory_index, (unsigned long long)offset, len,
                          (unsigned long long)mem_size);
        return WASM_ERR_OUT_OF_BOUNDS;
    }

    if (len > 0) memcpy(dst, memory->data + offset, len);
    return WASM_OK;
}

wasm_error_t wasm_memory_write(wasm_module_t* mod, uint32_t memory_index,
                               uint64_t offset, const void* src, size_t len) {
    wasm_memory_t* memory;
    uint64_t mem_size;

    if (!mod || (len > 0 && !src)) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid memory write arguments");
        return WASM_ERR_MALFORMED;
    }

    memory = wasm__memory_at(mod, memory_index);
    if (!memory) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "unknown memory %u", (unsigned)memory_index);
        return WASM_ERR_MALFORMED;
    }

    mem_size = wasm__memory_byte_size(memory);
    if (!wasm__range_in_bounds_u64(offset, (uint64_t)len, mem_size)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_OUT_OF_BOUNDS,
                          "memory %u write out of bounds: offset=%llu len=%zu size=%llu",
                          (unsigned)memory_index, (unsigned long long)offset, len,
                          (unsigned long long)mem_size);
        return WASM_ERR_OUT_OF_BOUNDS;
    }

    if (len > 0) memcpy(memory->data + offset, src, len);
    return WASM_OK;
}

wasm_error_t wasm_memory_get_string(wasm_module_t* mod, uint32_t memory_index,
                                    uint64_t offset, char* dst, size_t max_len) {
    wasm_memory_t* memory;
    uint64_t mem_size;
    uint64_t available_u64;
    size_t max_copy;
    size_t actual_len = 0;
    const uint8_t* src;

    if (!mod || !dst || max_len == 0) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid string extraction arguments");
        return WASM_ERR_MALFORMED;
    }

    memory = wasm__memory_at(mod, memory_index);
    if (!memory) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED,
                          "unknown memory %u", (unsigned)memory_index);
        return WASM_ERR_MALFORMED;
    }

    mem_size = wasm__memory_byte_size(memory);
    if (offset > mem_size) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_OUT_OF_BOUNDS,
                          "memory %u string offset out of bounds: offset=%llu size=%llu",
                          (unsigned)memory_index, (unsigned long long)offset,
                          (unsigned long long)mem_size);
        return WASM_ERR_OUT_OF_BOUNDS;
    }

    available_u64 = mem_size - offset;
    max_copy = max_len - 1u;
    if ((uint64_t)max_copy > available_u64) max_copy = (size_t)available_u64;

    src = memory->data ? memory->data + offset : NULL;
    if (src) {
        while (actual_len < max_copy && src[actual_len] != 0) actual_len++;
        if (actual_len > 0) memcpy(dst, src, actual_len);
    }

    dst[actual_len] = '\0';
    return WASM_OK;
}

uint8_t* wasm_memory_data_at(wasm_module_t* mod, uint32_t memory_index) {
    wasm_memory_t* memory = wasm__memory_at(mod, memory_index);
    return memory ? memory->data : NULL;
}

uint64_t wasm_memory_size_at(wasm_module_t* mod, uint32_t memory_index) {
    wasm_memory_t* memory = wasm__memory_at(mod, memory_index);
    return memory ? wasm__memory_byte_size(memory) : 0;
}

int64_t wasm_memory_grow_at(wasm_module_t* mod, uint32_t memory_index, uint64_t delta_pages) {
    wasm_memory_t* memory = wasm__memory_at(mod, memory_index);
    uint64_t old, np;
    size_t old_size;
    size_t new_size;
    uint8_t* nd;

    if (!memory) return -1;

    old = memory->pages;
    if (delta_pages > UINT64_MAX - old) return -1;
    np = old + delta_pages;
    if (np > memory->max_pages) return -1;
    memory->pages = old;
    if (!wasm__memory_byte_size_fits_host(memory, &old_size)) return -1;
    memory->pages = np;
    if (!wasm__memory_byte_size_fits_host(memory, &new_size)) {
        memory->pages = old;
        return -1;
    }
    memory->pages = old;
    nd = (uint8_t*)WASM_REALLOC(memory->data, new_size);
    if (!nd && np > 0) return -1;
    if (new_size > old_size) memset(nd + old_size, 0, new_size - old_size);
    memory->data = nd;
    memory->pages = np;
    return (int64_t)old;
}

wasm_memory_t* wasm_memory_ref_at(wasm_module_t* mod, uint32_t memory_index) {
    return wasm__memory_at(mod, memory_index);
}

uint8_t* wasm_memory_data(wasm_module_t* mod) {
    return wasm_memory_data_at(mod, 0);
}

uint64_t wasm_memory_size(wasm_module_t* mod) {
    return wasm_memory_size_at(mod, 0);
}

int64_t wasm_memory_grow(wasm_module_t* mod, uint64_t delta_pages) {
    return wasm_memory_grow_at(mod, 0, delta_pages);
}

uint32_t wasm_table_count(wasm_module_t* mod) {
    return mod ? mod->num_tables : 0;
}

wasm_table_t* wasm_table_ref_at(wasm_module_t* mod, uint32_t table_idx) {
    return wasm__table_at(mod, table_idx);
}

wasm_valtype_t wasm_table_type(wasm_module_t* mod, uint32_t table_idx) {
    wasm_table_t* table = wasm__table_at(mod, table_idx);
    return table ? table->reftype : WASM_TYPE_VOID;
}

const wasm_reftype_t* wasm_table_reftype(wasm_module_t* mod, uint32_t table_idx) {
    wasm_table_t* table = wasm__table_at(mod, table_idx);
    return table ? &table->reftype_info : NULL;
}

uint32_t wasm_global_count(wasm_module_t* mod) {
    return mod ? mod->num_globals : 0;
}

wasm_valtype_t wasm_global_type(wasm_module_t* mod, uint32_t global_idx) {
    if (!mod || global_idx >= mod->num_globals) return WASM_TYPE_VOID;
    return mod->globals[global_idx].type;
}

const wasm_reftype_t* wasm_global_reftype(wasm_module_t* mod, uint32_t global_idx) {
    if (!mod || global_idx >= mod->num_globals) return NULL;
    return &mod->globals[global_idx].ref_type;
}

int wasm_global_is_mutable(wasm_module_t* mod, uint32_t global_idx) {
    if (!mod || global_idx >= mod->num_globals) return 0;
    return mod->globals[global_idx].is_mutable;
}

wasm_value_t* wasm_global_value_ref_at(wasm_module_t* mod, uint32_t global_idx) {
    wasm_global_t* global;

    if (!mod || global_idx >= mod->num_globals) return NULL;
    global = &mod->globals[global_idx];
    return global->is_import ? global->import_value : &global->value;
}

wasm_error_t wasm_global_get(wasm_module_t* mod, const char* name, wasm_value_t* out_val) {
    wasm_global_t* global;
    wasm_error_t err;

    if (!mod || !name || !out_val) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid global get arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__resolve_exported_global(mod, name, &global, NULL);
    if (err != WASM_OK) return err;

    *out_val = wasm__global_get_value(global);
    return WASM_OK;
}

wasm_error_t wasm_global_set(wasm_module_t* mod, const char* name, wasm_value_t val) {
    wasm_global_t* global;
    wasm_error_t err;

    if (!mod || !name) {
        if (mod && mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_MALFORMED, "%s", "invalid global set arguments");
        return WASM_ERR_MALFORMED;
    }

    err = wasm__resolve_exported_global(mod, name, &global, NULL);
    if (err != WASM_OK) return err;

    if (!global->is_mutable) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "global '%s' is immutable", name);
        return WASM_ERR_TYPE_MISMATCH;
    }
    if (!wasm__runtime_value_matches_type(mod, &val, global->type, &global->ref_type)) {
        if (mod->rt)
            WASM__SET_ERR(mod->rt, WASM_ERR_TYPE_MISMATCH,
                          "global '%s' type mismatch", name);
        return WASM_ERR_TYPE_MISMATCH;
    }

    wasm__global_set_value(global, val);
    return WASM_OK;
}

uint32_t wasm_export_count(wasm_module_t* mod) {
    return mod ? mod->num_exports : 0;
}

uint32_t wasm_import_count(wasm_module_t* mod) {
    return mod ? mod->num_imports : 0;
}

const wasm_import_info_t* wasm_import_info(wasm_module_t* mod, uint32_t index) {
    if (!mod || index >= mod->num_imports) return NULL;
    return &mod->imports[index];
}

const char* wasm_export_name(wasm_module_t* mod, uint32_t index) {
    if (!mod || index >= mod->num_exports) return NULL;
    return wasm__export_name_cstr(&mod->exports[index]);
}

const uint8_t* wasm_export_name_bytes(wasm_module_t* mod, uint32_t index, size_t* out_len) {
    if (!mod || index >= mod->num_exports) {
        if (out_len) *out_len = 0u;
        return NULL;
    }
    if (out_len) *out_len = (size_t)mod->exports[index].name_len;
    return (const uint8_t*)mod->exports[index].name;
}

wasm_export_kind_t wasm_export_kind(wasm_module_t* mod, uint32_t index) {
    if (!mod || index >= mod->num_exports) return (wasm_export_kind_t)0;
    return mod->exports[index].kind;
}

uint32_t wasm_export_index(wasm_module_t* mod, uint32_t export_index) {
    if (!mod || export_index >= mod->num_exports) return 0;
    return mod->exports[export_index].index;
}

int wasm_find_export(wasm_module_t* mod, const char* name,
                     wasm_export_kind_t* out_kind, uint32_t* out_index) {
    if (!name) return 0;
    return wasm_find_export_bytes(mod, (const uint8_t*)name, strlen(name), out_kind, out_index);
}

int wasm_find_export_bytes(wasm_module_t* mod, const uint8_t* name, size_t name_len,
                           wasm_export_kind_t* out_kind, uint32_t* out_index) {
    uint32_t index;

    if (!mod || (!name && name_len != 0u)) return 0;

    for (index = 0; index < mod->num_exports; index++) {
        if (wasm__export_name_matches_bytes(&mod->exports[index], name, name_len)) {
            if (out_kind) *out_kind = mod->exports[index].kind;
            if (out_index) *out_index = mod->exports[index].index;
            return 1;
        }
    }

    return 0;
}

uint32_t wasm_custom_section_count(wasm_module_t* mod) {
    return mod ? mod->num_custom_sections : 0;
}

const char* wasm_custom_section_name(wasm_module_t* mod, uint32_t index) {
    if (!mod || index >= mod->num_custom_sections) return NULL;
    return mod->custom_sections[index].name;
}

const uint8_t* wasm_custom_section_data(wasm_module_t* mod, uint32_t index, size_t* out_len) {
    if (out_len) *out_len = 0;
    if (!mod || index >= mod->num_custom_sections) return NULL;
    if (out_len) *out_len = (size_t)mod->custom_sections[index].size;
    return mod->custom_sections[index].data;
}

const uint8_t* wasm_custom_section_find(wasm_module_t* mod, const char* name, size_t* out_len) {
    uint32_t index;

    if (out_len) *out_len = 0;
    if (!mod || !name) return NULL;

    for (index = 0; index < mod->num_custom_sections; index++) {
        if (strcmp(mod->custom_sections[index].name, name) == 0) {
            if (out_len) *out_len = (size_t)mod->custom_sections[index].size;
            return mod->custom_sections[index].data;
        }
    }

    return NULL;
}

uint32_t wasm_type_count(wasm_module_t* mod) {
    return mod ? mod->num_types : 0;
}

wasm_comptype_kind_t wasm_type_kind(wasm_module_t* mod, uint32_t type_idx) {
    if (!mod || type_idx >= mod->num_types) return WASM_COMP_INVALID;
    return mod->types[type_idx].kind;
}

const wasm_functype_t* wasm_type_functype(wasm_module_t* mod, uint32_t type_idx) {
    return wasm__module_const_functype(mod, type_idx);
}

uint32_t wasm_struct_type_field_count(wasm_module_t* mod, uint32_t type_idx) {
    const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);
    return st ? st->num_fields : 0;
}

const wasm_fieldtype_t* wasm_struct_type_field(wasm_module_t* mod, uint32_t type_idx,
                                               uint32_t field_idx) {
    const wasm_structtype_t* st = wasm__module_const_structtype(mod, type_idx);

    if (!st || field_idx >= st->num_fields) return NULL;
    return &st->fields[field_idx];
}

const wasm_fieldtype_t* wasm_array_type_field(wasm_module_t* mod, uint32_t type_idx) {
    const wasm_arraytype_t* at = wasm__module_const_arraytype(mod, type_idx);

    if (!at) return NULL;
    return &at->field;
}

uint32_t wasm_func_count(wasm_module_t* mod) {
    return mod ? mod->num_funcs : 0;
}

const wasm_functype_t* wasm_func_functype(wasm_module_t* mod, uint32_t func_idx) {
    if (!mod || func_idx >= mod->num_funcs) return NULL;
    return wasm__module_const_functype(mod, mod->funcs[func_idx].type_idx);
}

uint32_t wasm_func_param_count(wasm_module_t* mod, uint32_t func_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return 0;
    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) return 0;
    return ft->num_params;
}

uint32_t wasm_func_result_count(wasm_module_t* mod, uint32_t func_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return 0;
    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) return 0;
    return ft->num_results;
}

wasm_valtype_t wasm_func_param_type(wasm_module_t* mod, uint32_t func_idx,
                                    uint32_t param_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return WASM_TYPE_VOID;
    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) return WASM_TYPE_VOID;
    if (param_idx >= ft->num_params) return WASM_TYPE_VOID;
    return ft->params[param_idx];
}

wasm_valtype_t wasm_func_result_type(wasm_module_t* mod, uint32_t func_idx,
                                     uint32_t result_idx) {
    wasm_functype_t* ft;

    if (!mod || func_idx >= mod->num_funcs) return WASM_TYPE_VOID;
    ft = wasm__module_functype(mod, mod->funcs[func_idx].type_idx);
    if (!ft) return WASM_TYPE_VOID;
    if (result_idx >= ft->num_results) return WASM_TYPE_VOID;
    return ft->results[result_idx];
}

uint32_t wasm_tag_count(wasm_module_t* mod) {
    return mod ? mod->num_tags : 0;
}

const wasm_functype_t* wasm_tag_functype(wasm_module_t* mod, uint32_t tag_idx) {
    if (!mod || tag_idx >= mod->num_tags) return NULL;
    return wasm__module_const_functype(mod, mod->tags[tag_idx].type_idx);
}

#include "memory_internal.h"
#include "types/references.h"
#include "values.h"
#include "validate/module.h"
#include "module.h"
#include "runtime.h"
#include "internal/config.h"
#include "internal/alloc.h"
#include <string.h>

WASM_PRIVATE wasm_memory_t* wasm__memory_actual(wasm_memory_t* memory) {
    while (memory && memory->is_import && memory->import_memory && memory->import_memory != memory)
        memory = memory->import_memory;
    return memory;
}

WASM_PRIVATE const wasm_memory_t* wasm__memory_actual_const(const wasm_memory_t* memory) {
    while (memory && memory->is_import && memory->import_memory && memory->import_memory != memory)
        memory = memory->import_memory;
    return memory;
}

WASM_PRIVATE void wasm__free_table(wasm_table_t* table) {
    if (!table->is_import) WASM_FREE(table->elems);
    memset(table, 0, sizeof(*table));
}

WASM_PRIVATE void wasm__free_memory(wasm_memory_t* memory) {
    if (!memory->is_import) WASM_FREE(memory->data);
    memset(memory, 0, sizeof(*memory));
}

WASM_PRIVATE wasm_error_t wasm__append_tables(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_tables;
    wasm_table_t* tables;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    tables = (wasm_table_t*)WASM_REALLOC(mod->tables, (base + count) * sizeof(wasm_table_t));
    if (!tables) return WASM_ERR_OOM;

    mod->tables = tables;
    memset(mod->tables + base, 0, count * sizeof(wasm_table_t));
    mod->num_tables = base + count;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__append_memories(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_memories;
    wasm_memory_t* memories;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    memories = (wasm_memory_t*)WASM_REALLOC(mod->memories, (base + count) * sizeof(wasm_memory_t));
    if (!memories) return WASM_ERR_OOM;

    mod->memories = memories;
    memset(mod->memories + base, 0, count * sizeof(wasm_memory_t));
    mod->num_memories = base + count;
    return WASM_OK;
}

WASM_PRIVATE wasm_table_t* wasm__table_at(wasm_module_t* mod, uint32_t table_index) {
    if (!mod || table_index >= mod->num_tables) return NULL;
    return wasm__table_actual(&mod->tables[table_index]);
}

WASM_PRIVATE wasm_memory_t* wasm__memory_at(wasm_module_t* mod, uint32_t memory_index) {
    if (!mod || memory_index >= mod->num_memories) return NULL;
    return wasm__memory_actual(&mod->memories[memory_index]);
}

WASM_PRIVATE wasm_valtype_t wasm__memory_index_type(const wasm_memory_t* memory) {
    return (memory && memory->is_64) ? WASM_TYPE_I64 : WASM_TYPE_I32;
}

WASM_PRIVATE wasm_valtype_t wasm__table_index_type(const wasm_table_t* table) {
    return (table && table->is_64) ? WASM_TYPE_I64 : WASM_TYPE_I32;
}

WASM_PRIVATE wasm_valtype_t wasm__table_copy_size_type(const wasm_table_t* dst_table,
                                                 const wasm_table_t* src_table) {
    if ((dst_table && dst_table->is_64) && (src_table && src_table->is_64)) return WASM_TYPE_I64;
    return WASM_TYPE_I32;
}

WASM_PRIVATE uint64_t wasm__table_index_value(const wasm_table_t* table, const wasm_value_t* value) {
    if (table && table->is_64) return (uint64_t)value->of.i64;
    return (uint64_t)(uint32_t)value->of.i32;
}

WASM_PRIVATE wasm_value_t wasm__table_index_result(const wasm_table_t* table, uint64_t value) {
    if (table && table->is_64) return wasm_i64((int64_t)value);
    return wasm_i32((int32_t)value);
}

WASM_PRIVATE uint64_t wasm__memory_page_limit(const wasm_memory_t* memory) {
    return (memory && memory->is_64) ? WASM_MEMORY64_MAX_PAGES : (uint64_t)WASM_MEMORY32_MAX_PAGES;
}

WASM_PRIVATE wasm_error_t wasm__validate_memory_limits(wasm_module_t* mod,
                                                 uint32_t memory_index,
                                                 const wasm_memory_t* memory) {
    if (!mod || !mod->rt || !memory) return WASM_ERR_MALFORMED;

    if (memory->pages > memory->max_pages) {
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                          "memory %u min %llu exceeds max %llu",
                                          (unsigned)memory_index,
                                          (unsigned long long)memory->pages,
                                          (unsigned long long)memory->max_pages);
    }
    if (memory->max_pages > wasm__memory_page_limit(memory)) {
        return wasm__set_validation_error(mod->rt, UINT32_MAX, 0,
                                          "memory %u max %llu exceeds %s page limit",
                                          (unsigned)memory_index,
                                          (unsigned long long)memory->max_pages,
                                          memory->is_64 ? "Memory64" : "MVP");
    }
    return WASM_OK;
}

WASM_PRIVATE uint64_t wasm__memory_byte_size(const wasm_memory_t* memory) {
    return memory ? memory->pages * (uint64_t)WASM_PAGE_SIZE : 0;
}

WASM_PRIVATE int wasm__memory_byte_size_fits_host(const wasm_memory_t* memory, size_t* out_size) {
    uint64_t byte_size = wasm__memory_byte_size(memory);

    if (byte_size > (uint64_t)SIZE_MAX) return 0;
    if (out_size) *out_size = (size_t)byte_size;
    return 1;
}

WASM_PRIVATE int wasm__memory_effective_addr(const wasm_memory_t* memory,
                                       const wasm_value_t* base,
                                       uint64_t offset,
                                       uint64_t* out_addr) {
    uint64_t base_addr;

    if (!memory || !base || !out_addr) return 0;
    base_addr = memory->is_64 ? (uint64_t)base->of.i64 : (uint64_t)(uint32_t)base->of.i32;
    if (offset > UINT64_MAX - base_addr) return 0;
    *out_addr = base_addr + offset;
    return 1;
}

WASM_PRIVATE wasm_error_t wasm__init_memory_storage(wasm_memory_t* memory) {
    size_t byte_size;

    memory = wasm__memory_actual(memory);
    if (!memory) return WASM_ERR_MALFORMED;
    if (memory->pages == 0) return WASM_OK;
    if (memory->data) return WASM_OK;
    if (!wasm__memory_byte_size_fits_host(memory, &byte_size)) return WASM_ERR_OOM;

    memory->data = (uint8_t*)WASM_CALLOC(1u, byte_size);
    if (!memory->data) return WASM_ERR_OOM;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__init_table_storage(wasm_table_t* table) {
    size_t elem_count;
    uint32_t i;

    table = wasm__table_actual(table);
    if (!table) return WASM_ERR_MALFORMED;
    if (table->size == 0) return WASM_OK;
    if (!wasm__is_ref_type(table->reftype)) return WASM_ERR_MALFORMED;
    if (table->elems) return WASM_OK;
    elem_count = (size_t)table->size;
    if ((uint64_t)elem_count * (uint64_t)sizeof(wasm_value_t) > (uint64_t)SIZE_MAX)
        return WASM_ERR_OOM;

    table->elems = (wasm_value_t*)WASM_MALLOC(elem_count * sizeof(wasm_value_t));
    if (!table->elems) return WASM_ERR_OOM;
    for (i = 0; i < table->size; i++) table->elems[i] = table->default_value;
    return WASM_OK;
}

WASM_PRIVATE wasm_error_t wasm__append_data_segments(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_data_segments;
    wasm_data_segment_t* segments;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    segments = (wasm_data_segment_t*)WASM_REALLOC(mod->data_segments, (base + count) * sizeof(wasm_data_segment_t));
    if (!segments) return WASM_ERR_OOM;

    mod->data_segments = segments;
    memset(mod->data_segments + base, 0, count * sizeof(wasm_data_segment_t));
    mod->num_data_segments = base + count;
    return WASM_OK;
}

WASM_PRIVATE void wasm__free_custom_section(wasm_custom_section_t* section) {
    WASM_FREE(section->name);
    memset(section, 0, sizeof(*section));
}

WASM_PRIVATE wasm_error_t wasm__append_custom_sections(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_custom_sections;
    wasm_custom_section_t* sections;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    sections = (wasm_custom_section_t*)WASM_REALLOC(mod->custom_sections,
                                                    (base + count) * sizeof(wasm_custom_section_t));
    if (!sections) return WASM_ERR_OOM;

    mod->custom_sections = sections;
    memset(mod->custom_sections + base, 0, count * sizeof(wasm_custom_section_t));
    mod->num_custom_sections = base + count;
    return WASM_OK;
}

WASM_PRIVATE void wasm__free_elem_segment(wasm_elem_segment_t* segment) {
    WASM_FREE(segment->elems);
    memset(segment, 0, sizeof(*segment));
}

WASM_PRIVATE wasm_error_t wasm__append_elem_segments(wasm_module_t* mod, uint32_t count, uint32_t* base_out) {
    uint32_t base = mod->num_elem_segments;
    wasm_elem_segment_t* segments;

    if (base_out) *base_out = base;
    if (count == 0) return WASM_OK;

    segments = (wasm_elem_segment_t*)WASM_REALLOC(mod->elem_segments, (base + count) * sizeof(wasm_elem_segment_t));
    if (!segments) return WASM_ERR_OOM;

    mod->elem_segments = segments;
    memset(mod->elem_segments + base, 0, count * sizeof(wasm_elem_segment_t));
    mod->num_elem_segments = base + count;
    return WASM_OK;
}

WASM_PRIVATE int wasm__range_in_bounds_u64(uint64_t offset, uint64_t size, uint64_t limit) {
    return offset <= limit && size <= (limit - offset);
}

WASM_PRIVATE int wasm__memory_access_in_bounds(uint64_t addr, uint64_t width, uint64_t mem_size) {
    return wasm__range_in_bounds_u64(addr, width, mem_size);
}

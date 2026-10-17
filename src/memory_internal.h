#ifndef WASM_MEMORY_INTERNAL_H
#define WASM_MEMORY_INTERNAL_H

#include "internal/config.h"

WASM_PRIVATE wasm_memory_t* wasm__memory_actual(wasm_memory_t* memory);

WASM_PRIVATE const wasm_memory_t* wasm__memory_actual_const(const wasm_memory_t* memory);

WASM_PRIVATE void wasm__free_table(wasm_table_t* table);

WASM_PRIVATE void wasm__free_memory(wasm_memory_t* memory);

WASM_PRIVATE wasm_error_t wasm__append_tables(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE wasm_error_t wasm__append_memories(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE wasm_table_t* wasm__table_at(wasm_module_t* mod, uint32_t table_index);

WASM_PRIVATE wasm_memory_t* wasm__memory_at(wasm_module_t* mod, uint32_t memory_index);

WASM_PRIVATE wasm_valtype_t wasm__memory_index_type(const wasm_memory_t* memory);

WASM_PRIVATE wasm_valtype_t wasm__table_index_type(const wasm_table_t* table);

WASM_PRIVATE wasm_valtype_t wasm__table_copy_size_type(const wasm_table_t* dst_table,
                                                 const wasm_table_t* src_table);

WASM_PRIVATE uint64_t wasm__table_index_value(const wasm_table_t* table, const wasm_value_t* value);

WASM_PRIVATE wasm_value_t wasm__table_index_result(const wasm_table_t* table, uint64_t value);

WASM_PRIVATE uint32_t wasm__memory_page_size(const wasm_memory_t* memory);

WASM_PRIVATE uint64_t wasm__memory_page_limit(const wasm_memory_t* memory);

WASM_PRIVATE wasm_error_t wasm__validate_memory_limits(wasm_module_t* mod,
                                                 uint32_t memory_index,
                                                 const wasm_memory_t* memory);

/* Current page count; shared memories read it atomically since other threads may grow them. */
WASM_PRIVATE uint64_t wasm__memory_pages(const wasm_memory_t* memory);

WASM_PRIVATE uint64_t wasm__memory_byte_size(const wasm_memory_t* memory);

WASM_PRIVATE int wasm__memory_byte_size_fits_host(const wasm_memory_t* memory, size_t* out_size);

WASM_PRIVATE int wasm__memory_effective_addr(const wasm_memory_t* memory,
                                       const wasm_value_t* base,
                                       uint64_t offset,
                                       uint64_t* out_addr);

WASM_PRIVATE wasm_error_t wasm__init_memory_storage(wasm_memory_t* memory);

WASM_PRIVATE wasm_error_t wasm__init_table_storage(wasm_table_t* table);

WASM_PRIVATE wasm_error_t wasm__append_data_segments(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE void wasm__free_custom_section(wasm_custom_section_t* section);

WASM_PRIVATE wasm_error_t wasm__append_custom_sections(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE void wasm__free_elem_segment(wasm_elem_segment_t* segment);

WASM_PRIVATE wasm_error_t wasm__append_elem_segments(wasm_module_t* mod, uint32_t count, uint32_t* base_out);

WASM_PRIVATE int wasm__range_in_bounds_u64(uint64_t offset, uint64_t size, uint64_t limit);

WASM_PRIVATE int wasm__memory_access_in_bounds(uint64_t addr, uint64_t width, uint64_t mem_size);


#endif /* WASM_MEMORY_INTERNAL_H */

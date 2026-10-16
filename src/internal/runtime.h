#ifndef WASM_INTERNAL_RUNTIME_H
#define WASM_INTERNAL_RUNTIME_H

#include "values.h"

/* ── Init / Destroy ───────────────────────────────────────────────── */

WASM_PRIVATE void wasm__gc_heap_destroy(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__gc_register_module(wasm_runtime_t* rt, wasm_module_t* mod);

WASM_PRIVATE void wasm__gc_unregister_module(wasm_runtime_t* rt, wasm_module_t* mod);

WASM_PRIVATE int wasm__gc_ptr_in_heap(const wasm_runtime_t* rt, const void* ptr);

WASM_PRIVATE wasm_error_t wasm__gc_heap_init(wasm_runtime_t* rt, size_t size);

WASM_PRIVATE void* wasm__gc_bump_alloc(wasm_runtime_t* rt, size_t size, size_t alignment);

WASM_PRIVATE wasm_gc_header_t* wasm__gc_alloc_object(wasm_runtime_t* rt,
                                               const wasm_module_t* mod,
                                               uint32_t type_index,
                                               wasm_gc_object_kind_t kind,
                                               size_t object_size);

WASM_PRIVATE wasm_gc_struct_t* wasm__gc_alloc_struct_object(wasm_runtime_t* rt,
                                                      const wasm_module_t* mod,
                                                      uint32_t type_index,
                                                      const wasm_structtype_t* type);

WASM_PRIVATE wasm_gc_array_t* wasm__gc_alloc_array_object(wasm_runtime_t* rt,
                                                    const wasm_module_t* mod,
                                                    uint32_t type_index,
                                                    const wasm_arraytype_t* type,
                                                    uint32_t length);

WASM_PRIVATE void wasm__arena_destroy(wasm_runtime_t* rt);

WASM_PRIVATE wasm_error_t wasm__arena_init(wasm_runtime_t* rt, size_t size);

WASM_PRIVATE void* wasm__arena_alloc(wasm_runtime_t* rt, size_t size);

WASM_PRIVATE void wasm__arena_reset(wasm_runtime_t* rt, size_t saved_offset);

void wasm_config_default(wasm_config_t* config);

WASM_PRIVATE wasm_error_t wasm__stack_init(wasm_runtime_t* rt, uint32_t max_stack);

WASM_PRIVATE void wasm__stack_destroy(wasm_runtime_t* rt);

WASM_PRIVATE wasm_error_t wasm__backtrace_init(wasm_runtime_t* rt, uint32_t max_depth);

WASM_PRIVATE void wasm__backtrace_destroy(wasm_runtime_t* rt);

WASM_PRIVATE wasm_error_t wasm__call_frame_stack_init(wasm_runtime_t* rt);
WASM_PRIVATE void wasm__call_frame_stack_destroy(wasm_runtime_t* rt);
WASM_PRIVATE wasm_error_t wasm__ensure_module_frame_arena(wasm_module_t* mod);

wasm_runtime_t* wasm_runtime_new(const wasm_config_t* config);

void wasm_runtime_free(wasm_runtime_t* rt);

wasm_error_t wasm_init(wasm_runtime_t* rt, const wasm_config_t* config);

wasm_error_t wasm_runtime_last_error(const wasm_runtime_t* rt);

const char* wasm_runtime_error_message(const wasm_runtime_t* rt);

void wasm_runtime_clear_error(wasm_runtime_t* rt);

void wasm_runtime_set_error(wasm_runtime_t* rt, wasm_error_t err, const char* message);

void wasm_runtime_set_enabled_features(wasm_runtime_t* rt, uint32_t enabled_features);

uint32_t wasm_runtime_enabled_features(const wasm_runtime_t* rt);

void wasm_destroy(wasm_runtime_t* rt);

WASM_PRIVATE wasm_error_t wasm__register_import_internal(wasm_runtime_t* rt,
                                                   const wasm_import_t* imp,
                                                   int zeroed_type_is_unspecified);

wasm_error_t wasm_register_import(wasm_runtime_t* rt, const wasm_import_t* imp);

wasm_error_t wasm_register_global_import(wasm_runtime_t* rt, const wasm_global_import_t* imp);

wasm_error_t wasm_register_table_import(wasm_runtime_t* rt, const wasm_table_import_t* imp);

wasm_error_t wasm_register_memory_import(wasm_runtime_t* rt, const wasm_memory_import_t* imp);

wasm_error_t wasm_register_tag_import(wasm_runtime_t* rt, const wasm_tag_import_t* imp);

void wasm_enable_feature(wasm_runtime_t* rt, uint32_t flag);

void wasm_disable_feature(wasm_runtime_t* rt, uint32_t flag);

void wasm_enable_all_features(wasm_runtime_t* rt);

/* ── Section decoders ─────────────────────────────────────────────── */

WASM_PRIVATE wasm_error_t wasm__decode_type_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_custom_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__read_memory_type(wasm_module_t* mod,
                                           wasm__reader_t* r,
                                           wasm_memory_t* memory);

WASM_PRIVATE wasm_error_t wasm__read_table_type(wasm_module_t* mod,
                                          wasm__reader_t* r,
                                          wasm_table_t* table);

WASM_PRIVATE wasm_error_t wasm__decode_import_desc(wasm_module_t* mod,
                                             wasm__reader_t* r,
                                             wasm_import_info_t* info,
                                             uint8_t kind);

WASM_PRIVATE wasm_error_t wasm__decode_import_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_function_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_table_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_memory_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_global_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_tag_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_export_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_start_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_element_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_code_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_data_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_data_count_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE uint32_t wasm__find_exported_func(wasm_module_t* mod, const char* name);

WASM_PRIVATE wasm_error_t wasm__check_unresolved_function_imports(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__run_startup(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__set_validation_error(wasm_runtime_t* rt,
                                               uint32_t func_idx,
                                               uint32_t offset,
                                               const char* fmt,
                                               ...);

WASM_PRIVATE wasm_error_t wasm__validate_types(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__resolve_functype_reftypes(wasm_module_t* mod, wasm_functype_t* ft);

WASM_PRIVATE wasm_error_t wasm__resolve_storagetype_reftype(wasm_module_t* mod, wasm_storagetype_t* storage);

WASM_PRIVATE wasm_error_t wasm__resolve_module_reftypes(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__validate_structural(wasm_module_t* mod);

#endif /* WASM_INTERNAL_RUNTIME_H */

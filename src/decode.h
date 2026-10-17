#ifndef WASM_DECODE_H
#define WASM_DECODE_H

#include "internal/config.h"
#include "reader.h"

WASM_PRIVATE wasm_error_t wasm__decode_type_section(wasm_module_t* mod, wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__decode_custom_section(wasm_module_t* mod, wasm__reader_t* r);

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

WASM_PRIVATE wasm_error_t wasm__check_unresolved_function_imports(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__run_startup(wasm_module_t* mod);

#endif /* WASM_DECODE_H */

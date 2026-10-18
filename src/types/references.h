#ifndef WASM_SRC_TYPES_REFERENCES_H
#define WASM_SRC_TYPES_REFERENCES_H

#include "../internal/config.h"
#include "../reader.h"

WASM_PRIVATE void wasm__clear_reftype(wasm_reftype_t* reftype);

WASM_PRIVATE wasm_error_t wasm__resolve_reftype(wasm_module_t* mod, wasm_reftype_t* reftype);

WASM_PRIVATE wasm_error_t wasm__resolve_valtype_reftype(wasm_module_t* mod,
                                                  wasm_valtype_t* type,
                                                  wasm_reftype_t* reftype);

WASM_PRIVATE int wasm__has_reftype_info(const wasm_reftype_t* reftype);

WASM_PRIVATE int wasm__reftype_equal(const wasm_reftype_t* lhs, const wasm_reftype_t* rhs);

WASM_PRIVATE int wasm__reftype_is_subtype(const wasm_module_t* mod,
                                    const wasm_reftype_t* subtype,
                                    const wasm_reftype_t* supertype);

WASM_PRIVATE const wasm_gc_header_t* wasm__runtime_get_gc_object(const wasm_module_t* mod,
                                                           const wasm_value_t* value);

WASM_PRIVATE wasm_error_t wasm__externref_from_anyref_value(wasm_runtime_t* rt,
                                                      wasm_module_t* mod,
                                                      wasm_value_t value,
                                                      wasm_value_t* out_value);

/* A host externref keeps its bits unless they read as an i31, a heap object, or a box; those are boxed. */
WASM_PRIVATE wasm_error_t wasm__anyref_from_externref_value(wasm_runtime_t* rt,
                                                      wasm_value_t value,
                                                      wasm_value_t* out_value);

WASM_PRIVATE int wasm__runtime_value_reftype(const wasm_module_t* mod,
                                       const wasm_value_t* value,
                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE int wasm__runtime_value_matches_type(const wasm_module_t* mod,
                                            const wasm_value_t* value,
                                            wasm_valtype_t expected_type,
                                            const wasm_reftype_t* expected_ref);

WASM_PRIVATE int wasm__elem_segment_values_match_type(const wasm_module_t* mod,
                                                const wasm_elem_segment_t* segment,
                                                wasm_valtype_t expected_type,
                                                const wasm_reftype_t* expected_ref);

WASM_PRIVATE int wasm__get_effective_reftype(const wasm_module_t* mod,
                                       wasm_valtype_t type,
                                       const wasm_reftype_t* ref_type,
                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE int wasm__typed_valtype_is_subtype(const wasm_module_t* mod,
                                          wasm_valtype_t subtype,
                                          const wasm_reftype_t* subtype_ref,
                                          wasm_valtype_t supertype,
                                          const wasm_reftype_t* supertype_ref);

WASM_PRIVATE int wasm__functype_is_subtype(const wasm_module_t* mod,
                                     const wasm_functype_t* subtype,
                                     const wasm_functype_t* supertype);

WASM_PRIVATE int wasm__globaltype_matches_import(const wasm_module_t* mod,
                                           wasm_valtype_t actual_type,
                                           const wasm_reftype_t* actual_ref,
                                           int actual_mutable,
                                           wasm_valtype_t expected_type,
                                           const wasm_reftype_t* expected_ref,
                                           int expected_mutable);

WASM_PRIVATE int wasm__tabletype_matches_import(const wasm_module_t* mod,
                                          const wasm_table_t* actual_table,
                                          wasm_valtype_t expected_type,
                                          const wasm_reftype_t* expected_ref,
                                          uint64_t expected_min,
                                          uint64_t expected_max,
                                          int expected_is_64);

WASM_PRIVATE int wasm__memorytype_matches_import(const wasm_memory_t* actual_memory,
                                           const wasm_memory_t* expected_memory);

WASM_PRIVATE wasm_error_t wasm__read_heaptype(wasm_module_t* mod,
                                        wasm__reader_t* r,
                                        wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__runtime_make_typeidx_reftype(wasm_module_t* mod,
                                                       uint32_t type_idx,
                                                       int nullable,
                                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__runtime_read_heap_reftype(wasm_module_t* mod,
                                                    wasm__reader_t* r,
                                                    int nullable,
                                                    wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_value_t wasm__runtime_cast_success_value(const wasm_reftype_t* target,
                                                     wasm_value_t value);

WASM_PRIVATE wasm_value_t wasm__runtime_cast_difference_value(const wasm_reftype_t* source,
                                                        const wasm_reftype_t* target,
                                                        wasm_value_t value);

WASM_PRIVATE wasm_error_t wasm__read_reftype(wasm_module_t* mod,
                                       wasm__reader_t* r,
                                       wasm_valtype_t* out_type,
                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__read_valtype(wasm_module_t* mod,
                                       wasm__reader_t* r,
                                       wasm_valtype_t* out_type,
                                       wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__read_unresolved_valtype(wasm_module_t* mod,
                                                  wasm__reader_t* r,
                                                  wasm_valtype_t* out_type,
                                                  wasm_reftype_t* out_reftype);

WASM_PRIVATE void wasm__skip_heaptype(wasm__reader_t* r);

WASM_PRIVATE void wasm__skip_valtype(wasm__reader_t* r);

WASM_PRIVATE int wasm__is_packedtype_byte(uint8_t byte);

WASM_PRIVATE int wasm__is_valtype_byte(uint8_t byte);

#endif /* WASM_SRC_TYPES_REFERENCES_H */

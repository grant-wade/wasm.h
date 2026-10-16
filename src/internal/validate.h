#ifndef WASM_INTERNAL_VALIDATE_H
#define WASM_INTERNAL_VALIDATE_H

#include "values.h"

typedef struct wasm__blocktype_t {
    uint32_t param_arity;
    uint32_t result_arity;
    const wasm_valtype_t* params;
    const wasm_valtype_t* results;
    const wasm_reftype_t* param_reftypes;
    const wasm_reftype_t* result_reftypes;
    wasm_valtype_t inline_result;
    wasm_reftype_t inline_result_reftype;
} wasm__blocktype_t;
WASM_PRIVATE wasm_error_t wasm__read_blocktype(wasm_module_t* mod, wasm__reader_t* r, wasm__blocktype_t* blocktype);

#define WASM__TYPE_BOT ((wasm_valtype_t)0xFF)

struct wasm__val_frame_t {
    uint8_t opcode;
    uint32_t height;
    const wasm_valtype_t* param_types;
    const wasm_reftype_t* param_reftypes;
    uint32_t num_params;
    const wasm_valtype_t* result_types;
    const wasm_reftype_t* result_reftypes;
    uint32_t num_results;
    wasm_valtype_t inline_result;
    wasm_reftype_t inline_result_reftype;
    int unreachable;
    int seen_else;
    int in_catch;
    int seen_catch_all;
    int seen_delegate;
    uint64_t* local_init;
    uint64_t* local_init_entry;
};

typedef struct wasm__validator_t {
    wasm_module_t* mod;
    uint32_t func_idx;
    const uint8_t* body_start;
    wasm__reader_t r;
    wasm_valtype_t* stack;
    wasm_reftype_t* stack_reftypes;
    uint32_t stack_capacity;
    uint32_t sp;
    wasm__val_frame_t* frames;
    uint32_t frame_capacity;
    uint32_t fp;
    uint32_t max_fp;
    uint64_t* local_init_bits;
    uint64_t* local_init_entry_bits;
    uint32_t local_init_words;
} wasm__validator_t;

WASM_PRIVATE uint32_t wasm__validator_local_init_word_count(uint32_t num_locals);

WASM_PRIVATE int wasm__validator_local_is_undefaulted(const wasm_func_t* func, uint32_t index);

WASM_PRIVATE void wasm__validator_local_init_set(uint64_t* bits, uint32_t index);

WASM_PRIVATE int wasm__validator_local_init_get(const uint64_t* bits, uint32_t index);

WASM_PRIVATE void wasm__validator_copy_local_init(uint64_t* dst,
                                            const uint64_t* src,
                                            uint32_t word_count);

WASM_PRIVATE void wasm__validator_seed_local_init(wasm__validator_t* v,
                                            const wasm_func_t* func,
                                            const wasm_functype_t* ft);

WASM_PRIVATE void wasm__validator_enter_child_frame(wasm__validator_t* v, uint32_t frame_index);

WASM_PRIVATE void wasm__validator_restore_entry_local_init(wasm__validator_t* v, wasm__val_frame_t* frame);

WASM_PRIVATE void wasm__validator_destroy(wasm__validator_t* v);

WASM_PRIVATE wasm_error_t wasm__validator_init(wasm__validator_t* v, wasm_module_t* mod, uint32_t func_idx);

WASM_PRIVATE wasm_error_t wasm__validator_error(wasm__validator_t* v,
                                          const uint8_t* at,
                                          const char* fmt,
                                          ...);

WASM_PRIVATE wasm_error_t wasm__validator_require_feature(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    uint32_t flag);

WASM_PRIVATE int wasm__validator_get_effective_reftype(wasm_valtype_t type,
                                                 const wasm_reftype_t* ref_type,
                                                 wasm_reftype_t* out);

typedef struct wasm__try_table_clause_t {
    uint8_t kind;
    uint32_t tag_index;
    uint32_t label_depth;
} wasm__try_table_clause_t;

WASM_PRIVATE wasm_error_t wasm__read_try_table_clause(wasm__reader_t* r,
                                                wasm__try_table_clause_t* out_clause);

WASM_PRIVATE int wasm__validator_type_matches(const wasm__validator_t* v,
                                        wasm_valtype_t actual,
                                        const wasm_reftype_t* actual_ref,
                                        wasm_valtype_t expected,
                                        const wasm_reftype_t* expected_ref);

WASM_PRIVATE wasm__val_frame_t* wasm__validator_frame(wasm__validator_t* v);

WASM_PRIVATE wasm_error_t wasm__validator_push_typed(wasm__validator_t* v,
                                               const uint8_t* at,
                                               wasm_valtype_t type,
                                               const wasm_reftype_t* ref_type);
WASM_PRIVATE wasm_error_t wasm__validator_push_many_typed(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    const wasm_valtype_t* types,
                                                    const wasm_reftype_t* reftypes,
                                                    uint32_t count);
WASM_PRIVATE wasm_error_t wasm__validator_pop_any_typed(wasm__validator_t* v,
                                                  const uint8_t* at,
                                                  wasm_valtype_t* out_type,
                                                  wasm_reftype_t* out_ref_type);
WASM_PRIVATE wasm_error_t wasm__validator_pop_expect_typed(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     wasm_valtype_t expected,
                                                     const wasm_reftype_t* expected_ref);
WASM_PRIVATE wasm_error_t wasm__validator_check_types_typed(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      const wasm_valtype_t* types,
                                                      const wasm_reftype_t* reftypes,
                                                      uint32_t count);

WASM_PRIVATE wasm_error_t wasm__validator_pop_direct_non_null_branch_operand(wasm__validator_t* v,
                                                                       const uint8_t* at,
                                                                       wasm_valtype_t* out_type,
                                                                       wasm_reftype_t* out_source_ref,
                                                                       wasm_reftype_t* out_target_ref);

WASM_PRIVATE wasm_error_t wasm__validator_push(wasm__validator_t* v,
                                         const uint8_t* at,
                                         wasm_valtype_t type);

WASM_PRIVATE wasm_error_t wasm__validator_push_typed(wasm__validator_t* v,
                                               const uint8_t* at,
                                               wasm_valtype_t type,
                                               const wasm_reftype_t* ref_type);

WASM_PRIVATE wasm_error_t wasm__validator_push_many_typed(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    const wasm_valtype_t* types,
                                                    const wasm_reftype_t* reftypes,
                                                    uint32_t count);

WASM_PRIVATE wasm_error_t wasm__validator_pop_any(wasm__validator_t* v,
                                            const uint8_t* at,
                                            wasm_valtype_t* out_type);

WASM_PRIVATE wasm_error_t wasm__validator_pop_any_typed(wasm__validator_t* v,
                                                  const uint8_t* at,
                                                  wasm_valtype_t* out_type,
                                                  wasm_reftype_t* out_ref_type);

WASM_PRIVATE wasm_error_t wasm__validator_pop_expect(wasm__validator_t* v,
                                               const uint8_t* at,
                                               wasm_valtype_t expected);

WASM_PRIVATE wasm_error_t wasm__validator_pop_expect_typed(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     wasm_valtype_t expected,
                                                     const wasm_reftype_t* expected_ref);

WASM_PRIVATE wasm_error_t wasm__validator_check_types_typed(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      const wasm_valtype_t* types,
                                                      const wasm_reftype_t* reftypes,
                                                      uint32_t count);

WASM_PRIVATE uint32_t wasm__validator_stack_floor_after_checked(const wasm__validator_t* v,
                                                          uint32_t count);

WASM_PRIVATE void wasm__validator_consume_checked(wasm__validator_t* v, uint32_t count);

WASM_PRIVATE wasm_error_t wasm__validator_pop_and_push_types(wasm__validator_t* v,
                                                       const uint8_t* at,
                                                       const wasm_valtype_t* types,
                                                       const wasm_reftype_t* reftypes,
                                                       uint32_t count);

WASM_PRIVATE void wasm__validator_mark_unreachable(wasm__validator_t* v);

WASM_PRIVATE int wasm__validator_same_types(const wasm_valtype_t* a,
                                      const wasm_reftype_t* a_refs,
                                      uint32_t a_count,
                                      const wasm_valtype_t* b,
                                      const wasm_reftype_t* b_refs,
                                      uint32_t b_count);

WASM_PRIVATE int wasm__validator_types_match(const wasm__validator_t* v,
                                       const wasm_valtype_t* actual_types,
                                       const wasm_reftype_t* actual_refs,
                                       uint32_t actual_count,
                                       const wasm_valtype_t* expected_types,
                                       const wasm_reftype_t* expected_refs,
                                       uint32_t expected_count);

WASM_PRIVATE int wasm__validator_meet_type(const wasm_module_t* mod,
                                     wasm_valtype_t lhs_type,
                                     const wasm_reftype_t* lhs_ref,
                                     wasm_valtype_t rhs_type,
                                     const wasm_reftype_t* rhs_ref,
                                     wasm_valtype_t* out_type,
                                     wasm_reftype_t* out_ref);

WASM_PRIVATE int wasm__validator_meet_types(const wasm_module_t* mod,
                                      const wasm_valtype_t* lhs_types,
                                      const wasm_reftype_t* lhs_reftypes,
                                      uint32_t lhs_count,
                                      const wasm_valtype_t* rhs_types,
                                      const wasm_reftype_t* rhs_reftypes,
                                      uint32_t rhs_count,
                                      wasm_valtype_t* out_types,
                                      wasm_reftype_t* out_reftypes);

WASM_PRIVATE void wasm__validator_branch_signature(const wasm__val_frame_t* frame,
                                             const wasm_valtype_t** out_types,
                                             const wasm_reftype_t** out_reftypes,
                                             uint32_t* out_count);

WASM_PRIVATE wasm_error_t wasm__validator_check_tail_results(wasm__validator_t* v,
                                                       const uint8_t* at,
                                                       const wasm_functype_t* caller_type,
                                                       const wasm_functype_t* callee_type);

WASM_PRIVATE wasm_error_t wasm__validator_check_frame_results(wasm__validator_t* v,
                                                        const uint8_t* at,
                                                        const wasm__val_frame_t* frame,
                                                        const char* wrong_stack_message);

WASM_PRIVATE wasm_error_t wasm__validator_finish_try_arm(wasm__validator_t* v,
                                                   const uint8_t* at,
                                                   wasm__val_frame_t* frame);

WASM_PRIVATE wasm_error_t wasm__validator_find_rethrow_frame(wasm__validator_t* v,
                                                       uint32_t depth,
                                                       wasm__val_frame_t** out_frame);

WASM_PRIVATE wasm_error_t wasm__validator_read_memarg(wasm__validator_t* v,
                                                const uint8_t* at,
                                                uint32_t max_align_log2,
                                                wasm__memarg_t* out_memarg);

WASM_PRIVATE wasm_error_t wasm__validator_pop_memory_index(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     uint32_t memory_index);

WASM_PRIVATE wasm_error_t wasm__validator_unary(wasm__validator_t* v,
                                          const uint8_t* at,
                                          wasm_valtype_t in_type,
                                          wasm_valtype_t out_type);

WASM_PRIVATE wasm_error_t wasm__validator_binary(wasm__validator_t* v,
                                           const uint8_t* at,
                                           wasm_valtype_t lhs_type,
                                           wasm_valtype_t rhs_type,
                                           wasm_valtype_t out_type);

WASM_PRIVATE wasm_error_t wasm__validator_read_typed_select(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_valtype_t wasm__field_stack_type(const wasm_fieldtype_t* field);

WASM_PRIVATE const wasm_reftype_t* wasm__field_stack_reftype(const wasm_fieldtype_t* field);

WASM_PRIVATE int wasm__field_is_reference_type(const wasm_fieldtype_t* field);

WASM_PRIVATE wasm_error_t wasm__validator_make_typeidx_reftype(wasm__validator_t* v,
                                                         const uint8_t* at,
                                                         uint32_t type_idx,
                                                         int nullable,
                                                         wasm_valtype_t* out_type,
                                                         wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__validator_read_heap_reftype(wasm__validator_t* v,
                                                      const uint8_t* at,
                                                      int nullable,
                                                      wasm_valtype_t* out_type,
                                                      wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__validator_read_cast_flag_reftype(wasm__validator_t* v,
                                                           const uint8_t* at,
                                                           uint8_t flags,
                                                           uint8_t bit,
                                                           wasm_valtype_t* out_type,
                                                           wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__validator_pop_ref_subtype(wasm__validator_t* v,
                                                    const uint8_t* at,
                                                    wasm_valtype_t expected_type,
                                                    const wasm_reftype_t* expected_ref,
                                                    wasm_valtype_t* out_type,
                                                    wasm_reftype_t* out_ref);

WASM_PRIVATE wasm_error_t wasm__validator_pop_ref_castable(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     wasm_valtype_t expected_type,
                                                     const wasm_reftype_t* expected_ref,
                                                     wasm_valtype_t* out_type,
                                                     wasm_reftype_t* out_ref);

WASM_PRIVATE void wasm__validator_reference_difference(const wasm_reftype_t* source,
                                                 const wasm_reftype_t* target,
                                                 wasm_valtype_t* out_type,
                                                 wasm_reftype_t* out_ref);

WASM_PRIVATE wasm_error_t wasm__validator_validate_gc(wasm__validator_t* v, const uint8_t* at);

WASM_PRIVATE wasm_error_t wasm__validator_read_simd_lane(wasm__validator_t* v,
                                                   const uint8_t* at,
                                                   uint32_t subop);

WASM_PRIVATE wasm_error_t wasm__validator_validate_simd(wasm__validator_t* v, const uint8_t* at);

WASM_PRIVATE wasm_error_t wasm__validator_validate_numeric(wasm__validator_t* v,
                                                     const uint8_t* at,
                                                     uint8_t op);

WASM_PRIVATE wasm_error_t wasm__validator_validate_prefixed(wasm__validator_t* v,
                                                      const uint8_t* at);

WASM_PRIVATE wasm_error_t wasm__validate_function(wasm_module_t* mod, uint32_t func_idx);

WASM_PRIVATE wasm_error_t wasm__validate_code(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__build_control_targets(wasm_func_t* func);

WASM_PRIVATE wasm_error_t wasm__prepare_function(wasm_module_t* mod, uint32_t func_idx);

WASM_PRIVATE wasm_error_t wasm__build_module_control_targets(wasm_module_t* mod);


#endif /* WASM_INTERNAL_VALIDATE_H */

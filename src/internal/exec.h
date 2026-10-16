#ifndef WASM_INTERNAL_EXEC_H
#define WASM_INTERNAL_EXEC_H

#include "validate.h"

/* ── Module loader ────────────────────────────────────────────────── */

wasm_module_t* wasm_load(wasm_runtime_t* rt, const uint8_t* bytes, size_t len);

void wasm_free_module(wasm_module_t* mod);

/* ── Interpreter types ────────────────────────────────────────────── */

typedef struct wasm__label_t {
    const uint8_t* continuation;
    const uint8_t* handler_ptr;
    uint32_t sp_base;
    uint32_t param_arity;
    uint32_t arity;
    uint32_t caught_tag;
    uint64_t caught_tag_identity;
    uint32_t caught_count;
    wasm_value_t* caught_values;
    uint32_t delegate_depth;
    uint8_t opcode;
    uint8_t active_catch;
} wasm__label_t;

WASM_PRIVATE int wasm__size_mul(size_t a, size_t b, size_t* out);

WASM_PRIVATE int wasm__size_add(size_t a, size_t b, size_t* out);

WASM_PRIVATE int wasm__size_align(size_t value, size_t alignment, size_t* out);

WASM_PRIVATE int wasm__frame_storage_size(uint32_t num_locals,
                                    uint32_t max_labels,
                                    size_t* out);

WASM_PRIVATE int wasm__module_frame_arena_requirement(wasm_module_t* mod, size_t* out);

WASM_PRIVATE wasm_error_t wasm__reserve_frame_arena(wasm_runtime_t* rt, size_t size);

WASM_PRIVATE wasm_error_t wasm__ensure_module_frame_arena(wasm_module_t* mod);

struct wasm__call_frame_t {
    wasm_module_t* module;
    uint32_t func_idx;
    uint32_t sp_base;
    wasm_value_t* locals;
    wasm__label_t* labels;
    uint32_t lsp;
    uint32_t max_labels;
    wasm__reader_t r;
    wasm_func_t* func;
    wasm_functype_t* ft;
    wasm_value_t* results_dst;
    uint32_t num_results;
    size_t arena_saved;
    uint32_t current_offset;
};

WASM_PRIVATE wasm_error_t wasm__call_frame_stack_init(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__call_frame_stack_destroy(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__clear_backtrace(wasm_runtime_t* rt);

WASM_PRIVATE uint32_t wasm__call_frame_offset(const wasm__call_frame_t* cf);

WASM_PRIVATE void wasm__capture_backtrace(wasm_runtime_t* rt, uint32_t base_frame_sp);

WASM_PRIVATE uint32_t wasm__visible_call_stack_depth(const wasm_runtime_t* rt);

WASM_PRIVATE int wasm__visible_call_frame_info(const wasm_runtime_t* rt,
                                         uint32_t depth,
                                         uint32_t* out_func_idx,
                                         uint32_t* out_offset);

WASM_PRIVATE void wasm__gc_mark_object(wasm_gc_header_t* object);

WASM_PRIVATE void wasm__gc_mark_value(const wasm_value_t* value);

WASM_PRIVATE void wasm__gc_mark_values(const wasm_value_t* values, uint32_t count);

WASM_PRIVATE void wasm__gc_mark_struct_fields(wasm_gc_header_t* object,
                                        const wasm_structtype_t* type);

WASM_PRIVATE void wasm__gc_mark_array_elements(wasm_gc_header_t* object,
                                         const wasm_arraytype_t* type);

WASM_PRIVATE void wasm__gc_mark_object(wasm_gc_header_t* object);

WASM_PRIVATE void wasm__gc_mark_runtime_roots(wasm_runtime_t* rt);

WASM_PRIVATE void wasm__gc_forward_value(wasm_runtime_t* rt, wasm_value_t* value);

WASM_PRIVATE void wasm__gc_forward_values(wasm_runtime_t* rt, wasm_value_t* values, uint32_t count);

WASM_PRIVATE void wasm__gc_forward_object_refs(wasm_runtime_t* rt, wasm_gc_header_t* object);

WASM_PRIVATE void wasm__gc_forward_runtime_roots(wasm_runtime_t* rt);

void wasm_gc_collect(wasm_runtime_t* rt);

typedef struct wasm__try_clause_t {
    uint8_t opcode;
    uint32_t immediate;
    const uint8_t* body_start;
    const uint8_t* next_clause;
} wasm__try_clause_t;

WASM_PRIVATE void wasm__clear_label(wasm__label_t* label);

#define WASM__ERR_EXCEPTION ((wasm_error_t) - 1)
#define WASM__ERR_CALL_FRAME ((wasm_error_t) - 2)
#define WASM__ERR_RETURN_FRAME ((wasm_error_t) - 3)
#define WASM__ERR_RESUME_FRAME ((wasm_error_t) - 4)
#define WASM__ERR_TAIL_CALL_FRAME ((wasm_error_t) - 5)

WASM_PRIVATE void wasm__init_call_frame(wasm__call_frame_t* cf,
                                  wasm_module_t* mod,
                                  wasm_runtime_t* rt,
                                  uint32_t func_idx,
                                  wasm_func_t* func,
                                  wasm_functype_t* ft,
                                  wasm_value_t* locals,
                                  wasm__label_t* labels,
                                  wasm_value_t* results_dst,
                                  uint32_t num_results);

WASM_PRIVATE wasm_error_t wasm__handle_exception_in_frame(wasm_module_t* mod,
                                                    wasm__label_t* labels,
                                                    uint32_t* label_sp,
                                                    wasm__reader_t* r,
                                                    uint32_t sp_base);
WASM_PRIVATE wasm_error_t wasm__handle_exception_frames(wasm_module_t* mod, uint32_t base_frame_sp);

WASM_PRIVATE void wasm__clear_call_frame(wasm__call_frame_t* cf);

WASM_PRIVATE wasm_error_t wasm__push_call_frame(wasm_module_t* mod,
                                          uint32_t func_idx,
                                          const wasm_value_t* args,
                                          uint32_t num_args,
                                          wasm_value_t* results_dst,
                                          uint32_t num_results);

WASM_PRIVATE wasm_error_t wasm__finish_call_frame(wasm_runtime_t* rt, uint32_t base_frame_sp);

WASM_PRIVATE wasm_error_t wasm__handle_exception_frames(wasm_module_t* mod, uint32_t base_frame_sp);

WASM_PRIVATE void wasm__skip_blocktype(wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__read_try_table_clause(wasm__reader_t* r,
                                                wasm__try_table_clause_t* out_clause);

WASM_PRIVATE void wasm__skip_try_table_immediates(wasm__reader_t* r);

WASM_PRIVATE void wasm__skip_prefixed_immediates(wasm__reader_t* r);

WASM_PRIVATE void wasm__skip_gc_immediates(wasm__reader_t* r);

WASM_PRIVATE wasm_error_t wasm__read_blocktype(wasm_module_t* mod, wasm__reader_t* r, wasm__blocktype_t* blocktype);

/* ── Block scanning ───────────────────────────────────────────────── */

WASM_PRIVATE void wasm__skip_immediates(uint8_t op, wasm__reader_t* r);

WASM_PRIVATE const uint8_t* wasm__find_try_clause_boundary(const uint8_t* start, const uint8_t* code_end);

typedef struct wasm__control_frame_t {
    uint8_t opcode;
    uint32_t offset;
} wasm__control_frame_t;

WASM_PRIVATE wasm_error_t wasm__build_control_targets(wasm_func_t* func);

WASM_PRIVATE const wasm__control_target_t* wasm__control_target_at(const wasm_func_t* func, const uint8_t* pc);

WASM_PRIVATE wasm_error_t wasm__build_module_control_targets(wasm_module_t* mod);

WASM_PRIVATE wasm_error_t wasm__read_try_clause(const uint8_t* clause_ptr,
                                          const uint8_t* code_end,
                                          wasm__try_clause_t* out_clause);

/* ── Branch helper ────────────────────────────────────────────────── */

WASM_PRIVATE wasm_error_t wasm__copy_exception_to_label(wasm__label_t* label,
                                                  const wasm__exception_state_t* ex);

WASM_PRIVATE wasm_error_t wasm__report_uncaught_exception(wasm_runtime_t* rt);

WASM_PRIVATE int wasm__find_rethrow_label(const wasm__label_t* labels,
                                    uint32_t lsp,
                                    uint32_t depth,
                                    uint32_t* out_index);

WASM_PRIVATE wasm_error_t wasm__do_branch(wasm_runtime_t* rt,
                                    wasm__label_t* labels,
                                    wasm__label_t* target,
                                    wasm__reader_t* r,
                                    uint32_t* label_sp,
                                    uint32_t depth_l);

WASM_PRIVATE wasm_error_t wasm__handle_exception_in_frame(wasm_module_t* mod,
                                                    wasm__label_t* labels,
                                                    uint32_t* label_sp,
                                                    wasm__reader_t* r,
                                                    uint32_t sp_base);

WASM_PRIVATE wasm_error_t wasm__do_branch(wasm_runtime_t* rt,
                                    wasm__label_t* labels,
                                    wasm__label_t* target,
                                    wasm__reader_t* r,
                                    uint32_t* label_sp,
                                    uint32_t depth_l);

WASM_PRIVATE wasm_error_t wasm__leave_label(wasm_runtime_t* rt, const wasm__label_t* label);

/* ── Interpreter ──────────────────────────────────────────────────── */

#define WASM__PUSH(rt, val)                \
    do {                                   \
        if ((rt)->sp >= (rt)->max_stack) { \
            err = WASM_ERR_STACK_OVERFLOW; \
            goto cleanup;                  \
        }                                  \
        (rt)->stack[(rt)->sp++] = (val);   \
    } while (0)
#define WASM__POP(rt) ((rt)->stack[--(rt)->sp])
#define WASM__PEEK(rt) ((rt)->stack[(rt)->sp - 1])
#define WASM__TRAP(code) \
    do {                 \
        err = (code);    \
        goto cleanup;    \
    } while (0)

WASM_PRIVATE wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results);

WASM_PRIVATE wasm_error_t wasm__invoke_host_func(wasm_module_t* mod,
                                           wasm_host_func_t host_func,
                                           const wasm_value_t* args,
                                           uint32_t num_args,
                                           wasm_value_t* results,
                                           uint32_t num_results,
                                           void* userdata);

WASM_PRIVATE void wasm__init_call_frame(wasm__call_frame_t* cf,
                                  wasm_module_t* mod,
                                  wasm_runtime_t* rt,
                                  uint32_t func_idx,
                                  wasm_func_t* func,
                                  wasm_functype_t* ft,
                                  wasm_value_t* locals,
                                  wasm__label_t* labels,
                                  wasm_value_t* results_dst,
                                  uint32_t num_results);

WASM_PRIVATE wasm_error_t wasm__interp_loop(wasm_module_t* mod,
                                      wasm__call_frame_t* cf,
                                      wasm_module_t** next_module,
                                      uint32_t* next_func_idx,
                                      wasm_value_t** next_args,
                                      uint32_t* next_num_args,
                                      int* next_args_owned,
                                      wasm_value_t* tail_args_buf,
                                      uint32_t base_frame_sp);

WASM_PRIVATE wasm_error_t wasm__interp_loop(wasm_module_t* mod,
                                      wasm__call_frame_t* cf,
                                      wasm_module_t** next_module,
                                      uint32_t* next_func_idx,
                                      wasm_value_t** next_args,
                                      uint32_t* next_num_args,
                                      int* next_args_owned,
                                      wasm_value_t* tail_args_buf,
                                      uint32_t base_frame_sp);

WASM_PRIVATE wasm_error_t wasm__interp(wasm_module_t* mod, uint32_t func_idx,
                                 wasm_value_t* args, uint32_t num_args,
                                 wasm_value_t* results, uint32_t num_results);

/* ── Public call API ──────────────────────────────────────────────── */

wasm_error_t wasm_call(wasm_module_t* mod, const char* func_name,
                       const wasm_value_t* args, uint32_t num_args,
                       wasm_value_t* results, uint32_t num_results);

wasm_error_t wasm_call_index(wasm_module_t* mod, uint32_t func_idx,
                             const wasm_value_t* args, uint32_t num_args,
                             wasm_value_t* results, uint32_t num_results);

WASM_PRIVATE wasm_error_t wasm__resolve_exported_func(wasm_module_t* mod,
                                                const char* name,
                                                uint32_t* out_index,
                                                const wasm_functype_t** out_type);

WASM_PRIVATE wasm_error_t wasm__resolve_exported_global(wasm_module_t* mod,
                                                  const char* name,
                                                  wasm_global_t** out_global,
                                                  uint32_t* out_index);

WASM_PRIVATE wasm_error_t wasm__fmt_char_to_type(char ch,
                                           int in_results,
                                           int* out_is_void,
                                           wasm_valtype_t* out_type,
                                           wasm_reftype_t* out_reftype);

WASM_PRIVATE wasm_error_t wasm__parse_fmt_string(const char* fmt,
                                           wasm_functype_t* out_type,
                                           const char** out_error);

WASM_PRIVATE wasm_error_t wasm__set_fmt_error(wasm_runtime_t* rt,
                                        wasm_error_t err,
                                        const char* prefix,
                                        const char* detail);

WASM_PRIVATE wasm_error_t wasm__call_fmt_va(wasm_module_t* mod,
                                      const char* func_name,
                                      const char* fmt,
                                      va_list ap);

wasm_error_t wasm_call_fmt(wasm_module_t* mod, const char* func_name, const char* fmt, ...);

wasm_error_t wasm_bind_host_func(wasm_runtime_t* rt, const char* module_name,
                                 const char* func_name, const char* fmt,
                                 wasm_host_func_t callback, void* userdata);


#endif /* WASM_INTERNAL_EXEC_H */

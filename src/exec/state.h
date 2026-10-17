#ifndef WASM_EXEC_STATE_H
#define WASM_EXEC_STATE_H

#include "../reader.h"

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
#define WASM__ERR_EXCEPTION ((wasm_error_t) - 1)
#define WASM__ERR_CALL_FRAME ((wasm_error_t) - 2)
#define WASM__ERR_RETURN_FRAME ((wasm_error_t) - 3)
#define WASM__ERR_RESUME_FRAME ((wasm_error_t) - 4)
#define WASM__ERR_TAIL_CALL_FRAME ((wasm_error_t) - 5)
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

#endif /* WASM_EXEC_STATE_H */

#ifndef WASM_EXEC_STATE_H
#define WASM_EXEC_STATE_H

#include "../reader.h"
#include "../runtime.h"

/* A try or try_table block that is running: the only blocks that keep a label at run time. */
typedef struct wasm__label_t {
    const uint32_t* handler;                 /* its clause table in the translated body */
    struct wasm__exception_object_t* caught; /* the exception a legacy catch clause of it is handling, or NULL */
    uint32_t sp_base;
    uint8_t opcode;
    uint8_t active_catch;
} wasm__label_t;
struct wasm__call_frame_t {
    wasm_module_t* module;
    uint32_t func_idx;
    uint32_t sp_base;
    wasm__slot_t* locals;
    wasm__label_t* labels;
    uint32_t lsp;
    const uint32_t* pc;    /* where the frame resumes, or NULL once it has branched to its end */
    const uint32_t* op_at; /* the instruction it is running, for backtraces */
    wasm__reader_t r;      /* bytecode for handlers that decode their own immediates, set before each one */
    wasm_func_t* func;
    wasm_value_t* results_dst; /* where a root frame's results go */
    wasm__arena_mark_t arena_saved;
};
#define WASM__ERR_EXCEPTION ((wasm_error_t) - 1)
#define WASM__ERR_CALL_FRAME ((wasm_error_t) - 2)
#define WASM__ERR_RETURN_FRAME ((wasm_error_t) - 3)
#define WASM__ERR_RESUME_FRAME ((wasm_error_t) - 4)
#define WASM__ERR_TAIL_CALL_FRAME ((wasm_error_t) - 5)
#define WASM__PUSH(rt) (&(rt)->exec->stack[(rt)->exec->sp++])
#define WASM__POP(rt) (&(rt)->exec->stack[--(rt)->exec->sp])
#define WASM__PEEK(rt) (&(rt)->exec->stack[(rt)->exec->sp - 1])
#define WASM__TRAP(code) \
    do {                 \
        err = (code);    \
        goto cleanup;    \
    } while (0)

/* Bulk instructions pay for their size on top of the unit every instruction costs: one more
 * unit per WASM__FUEL_BYTES_PER_UNIT bytes or WASM__FUEL_ELEMS_PER_UNIT elements they touch. */
#define WASM__FUEL_BYTES_PER_UNIT 64u
#define WASM__FUEL_ELEMS_PER_UNIT 8u
#define WASM__CHARGE_FUEL(rt, units)                    \
    do {                                                \
        if ((rt)->fuel_enabled) {                       \
            uint64_t wasm__units = (uint64_t)(units);   \
            if ((rt)->fuel < wasm__units) {             \
                (rt)->fuel = 0;                         \
                WASM__TRAP(WASM_ERR_OUT_OF_FUEL);       \
            }                                           \
            (rt)->fuel -= wasm__units;                  \
        }                                               \
    } while (0)
#define WASM__CHARGE_BYTES(rt, bytes) WASM__CHARGE_FUEL(rt, (uint64_t)(bytes) / WASM__FUEL_BYTES_PER_UNIT)
#define WASM__CHARGE_ELEMS(rt, count) WASM__CHARGE_FUEL(rt, (uint64_t)(count) / WASM__FUEL_ELEMS_PER_UNIT)

#endif /* WASM_EXEC_STATE_H */

#ifndef CALC_H
#define CALC_H

#include <stddef.h>
#include <stdint.h>

#if defined(__wasm__)
#define CALC_EXPORT(name) __attribute__((export_name(name)))
#else
#define CALC_EXPORT(name)
#endif

typedef enum calc_status {
    CALC_OK = 0,
    CALC_SYNTAX_ERROR = 1,
    CALC_UNKNOWN_VARIABLE = 2,
    CALC_DIVIDE_BY_ZERO = 3,
} calc_status;

/* The outcome of calc_eval or calc_integrate. */
typedef struct calc_result {
    double value;
    /* Byte offset of the error in the source, when the status is not CALC_OK. */
    uint32_t error_offset;
    /* Steps calc_integrate has finished, updated as it runs. */
    uint32_t steps_done;
} calc_result;

/* Returns NULL when the arena is full. */
CALC_EXPORT("alloc") void* calc_alloc(size_t size);
/* Frees everything calc_alloc returned. */
CALC_EXPORT("reset") void calc_reset(void);

/* `src` uses numbers, + - * /, parentheses, and the host's variables and one-argument functions. */
CALC_EXPORT("eval") calc_status calc_eval(const char* src, size_t len, calc_result* out);

/* Integrates `src` over x from `a` to `b` with the trapezoid rule in `steps` steps. */
CALC_EXPORT("integrate")
calc_status calc_integrate(const char* src, size_t len, double a, double b, uint32_t steps, calc_result* out);

#endif

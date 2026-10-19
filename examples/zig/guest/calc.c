#include "calc.h"

#include <stdbool.h>

#define CALC_IMPORT(name) __attribute__((import_module("host"), import_name(name)))

enum { LOG_DEBUG, LOG_INFO, LOG_WARN };

CALC_IMPORT("log") void host_log(int level, const char* message, size_t len);
CALC_IMPORT("lookup") bool host_lookup(const char* name, size_t len, double* out);
CALC_IMPORT("call") double host_call(const char* name, size_t len, double x);

static _Alignas(8) unsigned char arena[16 * 1024];
static size_t arena_used;

void* calc_alloc(size_t size) {
    size_t start = (arena_used + 7) & ~(size_t)7;
    if (size > sizeof arena - start) return NULL;
    arena_used = start + size;
    return arena + start;
}

void calc_reset(void) {
    arena_used = 0;
}

typedef struct parser {
    const char* src;
    size_t len;
    size_t pos;
    calc_status status;
    size_t error_pos;
    /* The value of x inside calc_integrate, or NULL. */
    const double* x;
} parser;

static double parse_sum(parser* p);

static void fail(parser* p, calc_status status, size_t pos) {
    if (p->status != CALC_OK) return;
    p->status = status;
    p->error_pos = pos;
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || is_digit(c);
}

static char peek(parser* p) {
    while (p->pos < p->len && (p->src[p->pos] == ' ' || p->src[p->pos] == '\t')) p->pos++;
    return p->pos < p->len ? p->src[p->pos] : '\0';
}

static bool accept(parser* p, char c) {
    if (peek(p) != c) return false;
    p->pos++;
    return true;
}

static double parse_number(parser* p) {
    double value = 0;
    while (p->pos < p->len && is_digit(p->src[p->pos])) value = value * 10 + (p->src[p->pos++] - '0');
    if (p->pos < p->len && p->src[p->pos] == '.') {
        double scale = 0.1;
        for (p->pos++; p->pos < p->len && is_digit(p->src[p->pos]); p->pos++, scale /= 10) {
            value += (p->src[p->pos] - '0') * scale;
        }
    }
    return value;
}

static double parse_name(parser* p) {
    size_t start = p->pos;
    while (p->pos < p->len && is_name_char(p->src[p->pos])) p->pos++;
    const char* name = p->src + start;
    size_t len = p->pos - start;

    if (accept(p, '(')) {
        double arg = parse_sum(p);
        if (!accept(p, ')')) fail(p, CALC_SYNTAX_ERROR, p->pos);
        return p->status == CALC_OK ? host_call(name, len, arg) : 0;
    }
    if (p->x && len == 1 && name[0] == 'x') return *p->x;
    double value = 0;
    if (!host_lookup(name, len, &value)) fail(p, CALC_UNKNOWN_VARIABLE, start);
    return value;
}

static double parse_atom(parser* p) {
    char c = peek(p);
    if (accept(p, '-')) return -parse_atom(p);
    if (accept(p, '(')) {
        double value = parse_sum(p);
        if (!accept(p, ')')) fail(p, CALC_SYNTAX_ERROR, p->pos);
        return value;
    }
    if (is_digit(c) || c == '.') return parse_number(p);
    if (is_name_char(c)) return parse_name(p);
    fail(p, CALC_SYNTAX_ERROR, p->pos);
    return 0;
}

static double parse_product(parser* p) {
    double value = parse_atom(p);
    for (;;) {
        if (accept(p, '*')) {
            value *= parse_atom(p);
        } else if (accept(p, '/')) {
            size_t pos = p->pos;
            double divisor = parse_atom(p);
            if (divisor == 0) fail(p, CALC_DIVIDE_BY_ZERO, pos);
            value /= divisor;
        } else {
            return value;
        }
    }
}

static double parse_sum(parser* p) {
    double value = parse_product(p);
    for (;;) {
        if (accept(p, '+')) {
            value += parse_product(p);
        } else if (accept(p, '-')) {
            value -= parse_product(p);
        } else {
            return value;
        }
    }
}

static calc_status evaluate(const char* src, size_t len, const double* x, double* value, uint32_t* error_offset) {
    parser p = { src, len, 0, CALC_OK, 0, x };
    *value = parse_sum(&p);
    if (peek(&p) != '\0') fail(&p, CALC_SYNTAX_ERROR, p.pos);
    *error_offset = (uint32_t)p.error_pos;
    return p.status;
}

calc_status calc_eval(const char* src, size_t len, calc_result* out) {
    out->steps_done = 0;
    return evaluate(src, len, NULL, &out->value, &out->error_offset);
}

static void log_text(int level, const char* text) {
    size_t len = 0;
    while (text[len]) len++;
    host_log(level, text, len);
}

calc_status calc_integrate(const char* src, size_t len, double a, double b, uint32_t steps, calc_result* out) {
    volatile calc_result* progress = out;
    progress->value = 0;
    progress->error_offset = 0;
    progress->steps_done = 0;
    if (steps == 0) {
        log_text(LOG_WARN, "integrate: no steps");
        return CALC_OK;
    }
    log_text(LOG_INFO, "integrate: started");

    double h = (b - a) / steps;
    double sum = 0;
    for (uint32_t i = 0; i <= steps; i++) {
        double x = a + h * i;
        double y = 0;
        uint32_t error_offset = 0;
        calc_status status = evaluate(src, len, &x, &y, &error_offset);
        if (status != CALC_OK) {
            progress->error_offset = error_offset;
            return status;
        }
        sum += (i == 0 || i == steps) ? y / 2 : y;
        progress->value = sum * h;
        progress->steps_done = i;
    }
    log_text(LOG_INFO, "integrate: finished");
    return CALC_OK;
}

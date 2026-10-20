#ifndef WASM_TEST_TEST_H
#define WASM_TEST_TEST_H

#include <stddef.h>

/* The running case and its check counts. */
typedef struct test_ctx {
    const char* suite_name;
    const char* case_name;
    size_t checks;
    size_t failed;
} test_ctx;

typedef struct test_case {
    const char* name;
    void (*fn)(test_ctx* t);
} test_case;

/* The test cases of one *_test.c file. */
typedef struct test_suite {
    const char* name;
    const test_case* cases;
    size_t count;
} test_suite;

/* Counts one check and reports it when passed is 0; fmt may be NULL. Returns passed. */
int test_check(test_ctx* t, int passed, const char* file, int line, const char* expr, const char* fmt, ...);
/* Runs every case of suite, calling after_case after each. Returns 0 if all passed. */
int test_run(const test_suite* suite, void (*after_case)(test_ctx* t));

#define TEST(name) static void name(test_ctx* t)
#define CHECK(t, expr) ((void)test_check((t), !!(expr), __FILE__, __LINE__, #expr, NULL))
#define CHECK_MSG(t, expr, ...) ((void)test_check((t), !!(expr), __FILE__, __LINE__, #expr, __VA_ARGS__))
#define COUNTOF(a) (sizeof(a) / sizeof((a)[0]))

#endif

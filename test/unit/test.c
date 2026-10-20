#include "test.h"

#include <stdarg.h>
#include <stdio.h>

int test_check(test_ctx* t, int passed, const char* file, int line, const char* expr, const char* fmt, ...) {
    t->checks++;
    if (passed) return 1;
    t->failed++;
    fprintf(stderr, "FAIL %s/%s at %s:%d: %s", t->suite_name, t->case_name, file, line, expr);
    if (fmt) {
        va_list args;

        fputs(" - ", stderr);
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
    fputc('\n', stderr);
    return 0;
}

int test_run(const test_suite* suite, void (*after_case)(test_ctx* t)) {
    size_t i;
    size_t failed_cases = 0;
    size_t checks = 0;
    size_t failed_checks = 0;

    printf("suite %s\n", suite->name);
    for (i = 0; i < suite->count; i++) {
        test_ctx t;

        t.suite_name = suite->name;
        t.case_name = suite->cases[i].name;
        t.checks = 0;
        t.failed = 0;
        suite->cases[i].fn(&t);
        after_case(&t);
        printf("  %s %s (%zu checks)\n", t.failed ? "FAIL" : "PASS", t.case_name, t.checks);
        if (t.failed) failed_cases++;
        checks += t.checks;
        failed_checks += t.failed;
    }
    printf("summary: %zu cases, %zu failed, %zu checks, %zu failed\n", suite->count, failed_cases, checks,
           failed_checks);
    return failed_cases != 0;
}

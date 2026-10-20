#if defined(__clang__)
#define FIXTURE_NOINLINE __attribute__((noinline))
#else
#define FIXTURE_NOINLINE
#endif

typedef int (*step_fn)(int remaining, int acc);

static FIXTURE_NOINLINE int bounce_odd(int remaining, int acc);

static FIXTURE_NOINLINE int bounce_even(int remaining, int acc) {
    if (remaining == 0)
        return acc;

    return bounce_odd(remaining - 1, acc + 3);
}

static FIXTURE_NOINLINE int bounce_odd(int remaining, int acc) {
    if (remaining == 0)
        return acc;

    return bounce_even(remaining - 1, acc + 4);
}

static FIXTURE_NOINLINE int run_tail(step_fn fn, int remaining, int acc) {
    return fn(remaining, acc);
}

int fixture_case(void) {
    return run_tail(bounce_even, 8, 1);
}
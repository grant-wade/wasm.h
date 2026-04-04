#if defined(__clang__)
#define WL_NOINLINE __attribute__((noinline))
#else
#define WL_NOINLINE
#endif

typedef int (*wl_step_fn)(int remaining, int acc);

static WL_NOINLINE int bounce_odd(int remaining, int acc);

static WL_NOINLINE int bounce_even(int remaining, int acc) {
    if (remaining == 0)
        return acc;

    return bounce_odd(remaining - 1, acc + 3);
}

static WL_NOINLINE int bounce_odd(int remaining, int acc) {
    if (remaining == 0)
        return acc;

    return bounce_even(remaining - 1, acc + 4);
}

static WL_NOINLINE int run_tail(wl_step_fn fn, int remaining, int acc) {
    return fn(remaining, acc);
}

int wl_case(void) {
    return run_tail(bounce_even, 8, 1);
}
#include "host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if WASM_ENABLE_PLATFORM
WASM_PRIVATE uint32_t wasm__platform_wasi_arg_count(void) {
    return (uint32_t)WASM_WASI_ARG_COUNT();
}

WASM_PRIVATE const char* wasm__platform_wasi_arg_at(uint32_t index) {
    (void)index;
    return WASM_WASI_ARG_AT(index);
}

WASM_PRIVATE uint32_t wasm__platform_wasi_env_count(void) {
    return (uint32_t)WASM_WASI_ENV_COUNT();
}

WASM_PRIVATE const char* wasm__platform_wasi_env_at(uint32_t index) {
    (void)index;
    return WASM_WASI_ENV_AT(index);
}

#endif
#if WASM_ENABLE_PLATFORM
#if WASM__PLATFORM_WINDOWS && defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
static uint64_t wasm__platform_windows_filetime_to_unix_ns(FILETIME ft) {
    ULARGE_INTEGER ticks;

    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;
    if (ticks.QuadPart < 116444736000000000ull) return 0u;
    return (ticks.QuadPart - 116444736000000000ull) * 100ull;
}

static int wasm__platform_windows_qpc_time_ns(uint64_t* out_time) {
    static LARGE_INTEGER freq = { 0 };
    LARGE_INTEGER counter;

    if (!out_time) return 0;
    if (freq.QuadPart == 0) {
        if (QueryPerformanceFrequency(&freq) == 0 || freq.QuadPart <= 0) return 0;
    }
    if (QueryPerformanceCounter(&counter) == 0) return 0;

    *out_time = (uint64_t)((counter.QuadPart / freq.QuadPart) * 1000000000ull +
                           ((counter.QuadPart % freq.QuadPart) * 1000000000ull) / freq.QuadPart);
    return 1;
}

static int wasm__platform_windows_process_time_ns(uint64_t* out_time) {
    FILETIME create_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    ULARGE_INTEGER kernel_ticks;
    ULARGE_INTEGER user_ticks;

    if (!out_time) return 0;
    if (GetProcessTimes(GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time) == 0) return 0;

    kernel_ticks.LowPart = kernel_time.dwLowDateTime;
    kernel_ticks.HighPart = kernel_time.dwHighDateTime;
    user_ticks.LowPart = user_time.dwLowDateTime;
    user_ticks.HighPart = user_time.dwHighDateTime;
    *out_time = (kernel_ticks.QuadPart + user_ticks.QuadPart) * 100ull;
    return 1;
}

static int wasm__platform_windows_thread_time_ns(uint64_t* out_time) {
    FILETIME create_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    ULARGE_INTEGER kernel_ticks;
    ULARGE_INTEGER user_ticks;

    if (!out_time) return 0;
    if (GetThreadTimes(GetCurrentThread(), &create_time, &exit_time, &kernel_time, &user_time) == 0) return 0;

    kernel_ticks.LowPart = kernel_time.dwLowDateTime;
    kernel_ticks.HighPart = kernel_time.dwHighDateTime;
    user_ticks.LowPart = user_time.dwLowDateTime;
    user_ticks.HighPart = user_time.dwHighDateTime;
    *out_time = (kernel_ticks.QuadPart + user_ticks.QuadPart) * 100ull;
    return 1;
}

#endif
#endif
#if WASM_ENABLE_PLATFORM
#if WASM__PLATFORM_POSIX && defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
static int wasm__platform_posix_clock_time_get(uint32_t clock_id, uint64_t* out_time) {
#if defined(CLOCK_REALTIME)
    struct timespec ts;
    int native_clock = -1;

    if (!out_time) return 0;

    switch (clock_id) {
        case 0:
            native_clock = CLOCK_REALTIME;
            break;
#if defined(CLOCK_MONOTONIC)
        case 1:
            native_clock = CLOCK_MONOTONIC;
            break;
#endif
#if defined(CLOCK_PROCESS_CPUTIME_ID)
        case 2:
            native_clock = CLOCK_PROCESS_CPUTIME_ID;
            break;
#endif
#if defined(CLOCK_THREAD_CPUTIME_ID)
        case 3:
            native_clock = CLOCK_THREAD_CPUTIME_ID;
            break;
#endif
        default:
            break;
    }

    if (native_clock >= 0 && clock_gettime((clockid_t)native_clock, &ts) == 0) {
        *out_time = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
        return 1;
    }
#else
    (void)clock_id;
    (void)out_time;
#endif
    return 0;
}

#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#if WASM__PLATFORM_WINDOWS && defined(_MSC_VER)
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    unsigned char* bytes = (unsigned char*)dst;
    size_t i;

    if (len > 0 && !dst) return 0;

    for (i = 0; i < len;) {
        unsigned int value;
        size_t chunk = len - i;

        if (rand_s(&value) != 0) return 0;
        if (chunk > sizeof(value)) chunk = sizeof(value);
        memcpy(bytes + i, &value, chunk);
        i += chunk;
    }
    return 1;
}

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#if WASM__PLATFORM_WINDOWS && defined(_MSC_VER)
#elif WASM__PLATFORM_POSIX
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    unsigned char* bytes = (unsigned char*)dst;
    FILE* random_file;
    size_t read_count;

    if (len > 0 && !dst) return 0;
    if (len == 0) return 1;

    random_file = fopen("/dev/urandom", "rb");
    if (!random_file) return 0;
    read_count = fread(bytes, 1, len, random_file);
    fclose(random_file);
    return read_count == len;
}

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#if WASM__PLATFORM_WINDOWS && defined(_MSC_VER)
#elif WASM__PLATFORM_POSIX
#else
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    (void)dst;
    return len == 0;
}

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_RANDOM_FALLBACK)
#else
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len) {
    return WASM_WASI_FILL_RANDOM(dst, len);
}

#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#if WASM__PLATFORM_WINDOWS
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time) {
    (void)precision;

    if (!out_time) return 0;

    switch (clock_id) {
        case 0: {
            FILETIME ft;

            GetSystemTimeAsFileTime(&ft);
            *out_time = wasm__platform_windows_filetime_to_unix_ns(ft);
            return 1;
        }
        case 1:
            return wasm__platform_windows_qpc_time_ns(out_time);
        case 2:
            return wasm__platform_windows_process_time_ns(out_time);
        case 3:
            return wasm__platform_windows_thread_time_ns(out_time);
        default:
            return 0;
    }
}

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#if WASM__PLATFORM_WINDOWS
#elif WASM__PLATFORM_POSIX
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time) {
    (void)precision;

    if (!out_time) return 0;
    if (wasm__platform_posix_clock_time_get(clock_id, out_time)) return 1;

    switch (clock_id) {
        case 0: {
            time_t now = time(NULL);

            if (now == (time_t)-1) return 0;
            *out_time = (uint64_t)(now > (time_t)0 ? now : (time_t)0) * 1000000000ull;
            return 1;
        }
        case 1:
        case 2:
        case 3: {
            clock_t ticks = clock();

            if (ticks == (clock_t)-1 || CLOCKS_PER_SEC <= 0) return 0;
            *out_time = ((uint64_t)ticks * 1000000000ull) / (uint64_t)CLOCKS_PER_SEC;
            return 1;
        }
        default:
            return 0;
    }
}

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#if WASM__PLATFORM_WINDOWS
#elif WASM__PLATFORM_POSIX
#else
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time) {
    (void)precision;

    if (!out_time) return 0;

    switch (clock_id) {
        case 0: {
            time_t now = time(NULL);

            if (now == (time_t)-1) return 0;
            *out_time = (uint64_t)(now > (time_t)0 ? now : (time_t)0) * 1000000000ull;
            return 1;
        }
        case 1:
        case 2:
        case 3: {
            clock_t ticks = clock();

            if (ticks == (clock_t)-1 || CLOCKS_PER_SEC <= 0) return 0;
            *out_time = ((uint64_t)ticks * 1000000000ull) / (uint64_t)CLOCKS_PER_SEC;
            return 1;
        }
        default:
            return 0;
    }
}

#endif
#endif
#endif
#if WASM_ENABLE_PLATFORM
#if defined(WASM__WASI_NEEDS_CLOCK_FALLBACK)
#else
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id,
                                              uint64_t precision,
                                              uint64_t* out_time) {
    return WASM_WASI_CLOCK_TIME_GET(clock_id, precision, out_time);
}

#endif
#endif

#ifndef WASM_PLATFORM_HOST_H
#define WASM_PLATFORM_HOST_H

#include "../internal/config.h"

#if WASM_ENABLE_PLATFORM
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif
#endif

#if WASM_ENABLE_PLATFORM
/* ── WASI platform hooks ─────────────────────────────────────────── */

#if defined(_WIN32)
#define WASM__PLATFORM_WINDOWS 1
#define WASM__PLATFORM_POSIX 0
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__) || defined(__linux__)
#define WASM__PLATFORM_WINDOWS 0
#define WASM__PLATFORM_POSIX 1
#else
#define WASM__PLATFORM_WINDOWS 0
#define WASM__PLATFORM_POSIX 0
#endif

#ifndef WASM_WASI_ARG_COUNT
#define WASM_WASI_ARG_COUNT() 0u
#endif

#ifndef WASM_WASI_ARG_AT
#define WASM_WASI_ARG_AT(index) ((const char*)0)
#endif

#ifndef WASM_WASI_ENV_COUNT
#define WASM_WASI_ENV_COUNT() 0u
#endif

#ifndef WASM_WASI_ENV_AT
#define WASM_WASI_ENV_AT(index) ((const char*)0)
#endif

#ifndef WASM_WASI_FILL_RANDOM
#define WASM__WASI_NEEDS_RANDOM_FALLBACK 1
#endif

#ifndef WASM_WASI_CLOCK_TIME_GET
#define WASM__WASI_NEEDS_CLOCK_FALLBACK 1
#endif
#endif /* WASM_ENABLE_PLATFORM */

#if WASM_ENABLE_PLATFORM
WASM_PRIVATE uint32_t wasm__platform_wasi_arg_count(void);
WASM_PRIVATE const char* wasm__platform_wasi_arg_at(uint32_t index);
WASM_PRIVATE uint32_t wasm__platform_wasi_env_count(void);
WASM_PRIVATE const char* wasm__platform_wasi_env_at(uint32_t index);
WASM_PRIVATE int wasm__platform_wasi_fill_random(void* dst, size_t len);
WASM_PRIVATE int wasm__platform_wasi_clock_time_get(uint32_t clock_id, uint64_t precision, uint64_t* out_time);
/* Blocks the calling thread for at least ns nanoseconds; returns 0 where sleeping is unsupported. */
WASM_PRIVATE int wasm__platform_sleep_ns(uint64_t ns);
#endif

#endif /* WASM_PLATFORM_HOST_H */

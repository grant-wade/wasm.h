#ifndef WL_H
#define WL_H

/*
Single-header usage:

    #include "wl.h"

Optional platform-backed APIs (time, sleep, filesystem) are available when
WL_ENABLE_PLATFORM is defined to 1 before including wl.h in each translation
unit that uses them.

In exactly one translation unit:

    #define WL_IMPLEMENTATION
    #include "wl.h"
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(__clang__)
#define WL_COMPILER_CLANG 1
#else
#define WL_COMPILER_CLANG 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define WL_COMPILER_GCC 1
#else
#define WL_COMPILER_GCC 0
#endif

#if defined(_MSC_VER)
#define WL_COMPILER_MSVC 1
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#if !defined(__func__)
#define __func__ __FUNCTION__
#endif
#else
#define WL_COMPILER_MSVC 0
#endif

#if defined(__APPLE__)
#define WL_PLATFORM_MACOS 1
#define WL_PLATFORM_LINUX 0
#define WL_PLATFORM_WINDOWS 0
#define WL_PLATFORM_POSIX 1
#elif defined(__linux__)
#define WL_PLATFORM_MACOS 0
#define WL_PLATFORM_LINUX 1
#define WL_PLATFORM_WINDOWS 0
#define WL_PLATFORM_POSIX 1
#elif defined(_WIN32)
#define WL_PLATFORM_MACOS 0
#define WL_PLATFORM_LINUX 0
#define WL_PLATFORM_WINDOWS 1
#define WL_PLATFORM_POSIX 0
#else
#define WL_PLATFORM_MACOS 0
#define WL_PLATFORM_LINUX 0
#define WL_PLATFORM_WINDOWS 0
#define WL_PLATFORM_POSIX 0
#endif

#ifndef WL_ENABLE_PLATFORM
#define WL_ENABLE_PLATFORM 0
#endif

#if WL_COMPILER_MSVC && !defined(__cplusplus)
#define WL_INLINE static __inline
#else
#define WL_INLINE static inline
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;
typedef size_t usize;
typedef ptrdiff_t isize;

/* Small utility macros. Avoid passing expressions with side effects. */
#define WL_MIN(a, b) ((a) < (b) ? (a) : (b))
#define WL_MAX(a, b) ((a) > (b) ? (a) : (b))
#define WL_CLAMP(v, lo, hi) (WL_MIN(WL_MAX((v), (lo)), (hi)))
#define WL_COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))
#define WL_ALIGN_UP(v, a) (((v) + ((a) - 1u)) & ~((a) - 1u))
#define WL_ALIGN_DOWN(v, a) ((v) & ~((a) - 1u))
#define WL_IS_POWER_OF_2(v) ((v) != 0u && (((v) & ((v) - 1u)) == 0u))
#define WL_KB(n) ((u64)(n) * 1024ull)
#define WL_MB(n) ((u64)(n) * 1024ull * 1024ull)
#define WL_GB(n) ((u64)(n) * 1024ull * 1024ull * 1024ull)
#define WL_UNUSED(x) ((void)(x))
#define WL_STRINGIFY_IMPL(x) #x
#define WL_STRINGIFY(x) WL_STRINGIFY_IMPL(x)
#define WL_CONCAT_IMPL(a, b) a##b
#define WL_CONCAT(a, b) WL_CONCAT_IMPL(a, b)

/* C99-compatible alignment helpers used to avoid newer language requirements. */
#define WL_ALIGNOF(T) offsetof(struct { char c; T value; }, value)

typedef union wl__max_align {
    void* ptr;
    void (*fn)(void);
    long long ll;
    long double ld;
} wl__max_align;

#if WL_COMPILER_CLANG || WL_COMPILER_GCC
#define wl_likely(x) __builtin_expect(!!(x), 1)
#define wl_unlikely(x) __builtin_expect(!!(x), 0)
#else
#define wl_likely(x) (x)
#define wl_unlikely(x) (x)
#endif

/* Shared status codes used across the library. */
typedef enum wl_status {
    WL_OK = 0,
    WL_ERR_NOMEM = -1,
    WL_ERR_INVALID = -2,
    WL_ERR_NOT_FOUND = -3,
    WL_ERR_IO = -4,
    WL_ERR_OVERFLOW = -5,
    WL_ERR_TIMEOUT = -6,
    WL_ERR_UNSUPPORTED = -7,
} wl_status;

/* Returns a stable human-readable name for a status code. */
const char* wl_status_str(wl_status status);

/* Allocator interface used by all heap-owning APIs. */
typedef void* (*wl_alloc_fn)(void* ctx, u64 size);
typedef void* (*wl_realloc_fn)(void* ctx, void* ptr, u64 new_size);
typedef void (*wl_free_fn)(void* ctx, void* ptr);

typedef struct wl_alloc_t {
    wl_alloc_fn alloc;
    wl_realloc_fn realloc;
    wl_free_fn free;
} wl_alloc_t;

/* Global malloc-backed allocator used when an API receives NULL. */
extern wl_alloc_t* wl_alloc_default;

/* Resolves NULL to wl_alloc_default and passes embedded allocators through unchanged. */
WL_INLINE wl_alloc_t* wl_as_alloc(void* alloc) {
    return alloc != NULL ? (wl_alloc_t*)alloc : wl_alloc_default;
}

/* Allocates size bytes with the provided allocator or the default allocator when alloc is NULL. */
WL_INLINE void* wl_alloc(void* alloc, u64 size) {
    wl_alloc_t* resolved = wl_as_alloc(alloc);
    return resolved->alloc(resolved, size);
}

/* Reallocates ptr to new_size bytes with the provided allocator or the default allocator when alloc is NULL. */
WL_INLINE void* wl_realloc(void* alloc, void* ptr, u64 new_size) {
    wl_alloc_t* resolved = wl_as_alloc(alloc);
    return resolved->realloc(resolved, ptr, new_size);
}

/* Frees ptr with the provided allocator or the default allocator when alloc is NULL. */
WL_INLINE void wl_free(void* alloc, void* ptr) {
    wl_alloc_t* resolved = wl_as_alloc(alloc);
    resolved->free(resolved, ptr);
}

/* Realloc-like helper that falls back to alloc+copy when the allocator has no realloc hook. */
void* wl_alloc_resize(wl_alloc_t* alloc, void* ptr, usize old_size, usize new_size);

/* Fixed-capacity bump arena. The arena itself can be passed where a wl_alloc_t* is expected. */
typedef struct wl_arena {
    wl_alloc_t base;
    wl_alloc_t* backing;
    u8* buf;
    u64 cap;
    u64 pos;
} wl_arena;

/* Saved arena position used with wl_arena_restore. */
typedef u64 wl_arena_savepoint;

/* Initializes arena with a backing buffer allocated from backing or the default allocator. */
wl_status wl_arena_init(wl_arena* arena, u64 capacity, void* backing);
/* Allocates size bytes from the arena, aligned for the library's widest builtin scalar types. Returns NULL on exhaustion. */
void* wl_arena_alloc(wl_arena* arena, u64 size);
/* Captures the current bump position for later restoration. */
wl_arena_savepoint wl_arena_save(const wl_arena* arena);
/* Rewinds the arena to a previously captured savepoint. */
void wl_arena_restore(wl_arena* arena, wl_arena_savepoint savepoint);
/* Rewinds the arena to the beginning without freeing the backing buffer. */
void wl_arena_reset(wl_arena* arena);
/* Frees the arena backing buffer and clears the arena state. */
void wl_arena_destroy(wl_arena* arena);

/* Logging levels in ascending severity order. */
typedef enum wl_log_level {
    WL_LOG_TRACE = 0,
    WL_LOG_INFO = 1,
    WL_LOG_WARN = 2,
    WL_LOG_ERROR = 3,
    WL_LOG_FATAL = 4,
} wl_log_level;

/* Optional log sink callback. The default sink writes colorized output to stderr when possible. */
typedef void (*wl_log_callback)(wl_log_level level, const char* file, int line, const char* fmt, va_list args);

#ifndef WL_LOG_MIN_LEVEL
#define WL_LOG_MIN_LEVEL WL_LOG_TRACE
#endif

/* Sets the runtime minimum level accepted by wl_log_emit and the log macros. */
void wl_log_set_level(wl_log_level level);
/* Returns the current runtime minimum log level. */
wl_log_level wl_log_get_level(void);
/* Installs a custom log sink. Pass NULL to restore the default stderr sink. */
void wl_log_set_callback(wl_log_callback callback);
/* Emits a log record with an existing va_list. */
void wl_log_vemit(wl_log_level level, const char* file, int line, const char* fmt, va_list args);
/* Emits a log record with source location metadata. */
void wl_log_emit(wl_log_level level, const char* file, int line, const char* fmt, ...);

#if WL_LOG_MIN_LEVEL <= WL_LOG_TRACE
#define wl_log_trace(...) wl_log_emit(WL_LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#else
#define wl_log_trace(...) ((void)0)
#endif

#if WL_LOG_MIN_LEVEL <= WL_LOG_INFO
#define wl_log_info(...) wl_log_emit(WL_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#else
#define wl_log_info(...) ((void)0)
#endif

#if WL_LOG_MIN_LEVEL <= WL_LOG_WARN
#define wl_log_warn(...) wl_log_emit(WL_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#else
#define wl_log_warn(...) ((void)0)
#endif

#if WL_LOG_MIN_LEVEL <= WL_LOG_ERROR
#define wl_log_error(...) wl_log_emit(WL_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#else
#define wl_log_error(...) ((void)0)
#endif

#define wl_log_fatal(...)                                           \
    do {                                                            \
        wl_log_emit(WL_LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__); \
        abort();                                                    \
    } while (0)

/* Optional assertion handler invoked before aborting. */
typedef void (*wl_assert_handler)(const char* expr, const char* file, int line, const char* msg);

/* Installs a custom assertion handler. Pass NULL to restore the default aborting handler. */
void wl_assert_set_handler(wl_assert_handler handler);
/* Reports an assertion failure through the active handler. */
void wl_assert_fail(const char* expr, const char* file, int line, const char* msg);

#ifndef NDEBUG
#define wl_assert(expr) ((expr) ? (void)0 : wl_assert_fail(#expr, __FILE__, __LINE__, NULL))
#define wl_assertm(expr, msg) ((expr) ? (void)0 : wl_assert_fail(#expr, __FILE__, __LINE__, (msg)))
#else
#define wl_assert(expr) ((void)0)
#define wl_assertm(expr, msg) ((void)0)
#endif

#define wl_assert_always(expr) ((expr) ? (void)0 : wl_assert_fail(#expr, __FILE__, __LINE__, NULL))

/* Internal header used by the stretchy-array macros. */
typedef struct wl_arr_hdr {
    usize len;
    usize cap;
    wl_alloc_t* alloc;
} wl_arr_hdr;

/* Low-level stretchy-array helpers backing the wl_arr_* macros. */
void* wl_arr_init_impl(usize elem_size, void* alloc);
wl_status wl_arr_maybe_grow(void** arr_ptr, usize elem_size, usize extra);
void wl_arr_free_impl(void* arr);

#define wl_arr__hdr(arr) ((wl_arr_hdr*)((u8*)(arr) - sizeof(wl_arr_hdr)))
/* Returns the current element count or 0 for a NULL array pointer. */
#define wl_arr_len(arr) ((arr) ? wl_arr__hdr(arr)->len : 0u)
/* Returns the current capacity in elements or 0 for a NULL array pointer. */
#define wl_arr_cap(arr) ((arr) ? wl_arr__hdr(arr)->cap : 0u)
/* Allocates a new stretchy array for T using alloc or the default allocator when alloc is NULL. */
#define wl_arr_init(T, alloc) ((T*)wl_arr_init_impl(sizeof(T), (alloc)))
/* Appends one value, growing the buffer as needed. Returns WL_OK or an allocation error. */
#define wl_arr_push(arr, value) \
    ((wl_arr_maybe_grow((void**)&(arr), sizeof(*(arr)), 1u) == WL_OK) ? ((arr)[wl_arr__hdr(arr)->len++] = (value), WL_OK) : WL_ERR_NOMEM)
/* Removes and returns the last element. The array must be non-NULL and non-empty. */
#define wl_arr_pop(arr) ((arr)[--wl_arr__hdr(arr)->len])
/* Returns the last element without removing it. The array must be non-NULL and non-empty. */
#define wl_arr_last(arr) ((arr)[wl_arr__hdr(arr)->len - 1u])
/* Sets array length to zero without releasing storage. */
#define wl_arr_clear(arr)               \
    do {                                \
        if ((arr) != NULL) {            \
            wl_arr__hdr(arr)->len = 0u; \
        }                               \
    } while (0)
/* Releases the array storage and writes NULL back to the caller variable. */
#define wl_arr_free(arr)       \
    do {                       \
        wl_arr_free_impl(arr); \
        (arr) = NULL;          \
    } while (0)

/* Generates a pointer+length slice type for T. */
#define wl_slice(T) \
    struct {        \
        T* ptr;     \
        usize len;  \
    }

/* Mutable byte slice. */
typedef struct wl_bytes {
    u8* ptr;
    usize len;
} wl_bytes;

/* Read-only byte slice. */
typedef struct wl_bytes_const {
    const u8* ptr;
    usize len;
} wl_bytes_const;

/* Non-owning string view. ptr may point into a larger buffer and need not be NUL-terminated. */
typedef struct wl_str {
    const char* ptr;
    usize len;
} wl_str;

WL_INLINE wl_str wl_str_make(const char* ptr, usize len) {
    wl_str s;
    s.ptr = ptr;
    s.len = len;
    return s;
}

/* Creates a wl_str from a string literal at compile time. */
#define wl_str_lit(s) wl_str_make((s), sizeof(s) - 1u)

/* Creates a wl_str from a C string. NULL produces an empty view. */
wl_str wl_str_from_cstr(const char* cstr);
/* Bytewise equality test for string views. */
bool wl_str_eq(wl_str a, wl_str b);
/* Returns true when s begins with prefix. */
bool wl_str_starts_with(wl_str s, wl_str prefix);
/* Returns true when s ends with suffix. */
bool wl_str_ends_with(wl_str s, wl_str suffix);
/* Searches for needle in haystack and returns the byte offset or -1 when absent. */
isize wl_str_search(wl_str haystack, wl_str needle);

enum {
    WL_STRING_SSO_CAP = 23,
};

/* Owning UTF-8 byte string with small-string optimization for short values. */
typedef struct wl_string {
    wl_alloc_t* alloc;
    usize len;
    usize cap;
    char* data;
    char sso[WL_STRING_SSO_CAP + 1u];
} wl_string;

/* Creates an empty string using alloc or the default allocator when alloc is NULL. */
wl_string wl_string_new(void* alloc);
/* Copies a C string into a wl_string. */
wl_string wl_string_from(const char* cstr, void* alloc);
/* Copies a string view into a wl_string. */
wl_string wl_string_from_str(wl_str view, void* alloc);
/* Releases any heap storage held by the string and resets it to empty. */
void wl_string_destroy(wl_string* s);
/* Returns the string length in bytes. */
usize wl_string_len(const wl_string* s);
/* Returns a NUL-terminated pointer to the current contents. */
const char* wl_string_cstr(const wl_string* s);
/* Returns a non-owning view over the current contents. */
wl_str wl_string_as_str(const wl_string* s);
/* Appends a string view to s. */
wl_status wl_string_append(wl_string* s, wl_str tail);
/* Appends a C string to s. */
wl_status wl_string_append_cstr(wl_string* s, const char* cstr);
/* Appends one byte to s. */
wl_status wl_string_append_char(wl_string* s, char c);
/* Resets the string to empty without releasing heap storage. */
void wl_string_clear(wl_string* s);
/* Bytewise equality over the string contents. */
bool wl_string_eq(const wl_string* a, const wl_string* b);
/* Lexicographic compare over the string contents. */
int wl_string_cmp(const wl_string* a, const wl_string* b);
/* Searches for needle and returns the byte offset or -1 when absent. */
isize wl_string_find(const wl_string* s, wl_str needle);
/* Returns true when s begins with prefix. */
bool wl_string_starts_with(const wl_string* s, wl_str prefix);
/* Returns true when s ends with suffix. */
bool wl_string_ends_with(const wl_string* s, wl_str suffix);

enum {
    WL_UTF8_INVALID = 0xffffffffu,
};

/* Counts Unicode code points in a UTF-8 byte string. */
usize wl_utf8_len(wl_str s);
/* Decodes the code point at *offset and advances offset to the next code point boundary. */
u32 wl_utf8_next(wl_str s, usize* offset);
/* Decodes the code point starting at offset or returns WL_UTF8_INVALID. */
u32 wl_utf8_decode(wl_str s, usize offset);
/* Encodes one code point into buf and returns the number of bytes written, or 0 on invalid input. */
usize wl_utf8_encode(u8* buf, u32 codepoint);
/* Validates that s contains well-formed UTF-8. */
bool wl_utf8_valid(wl_str s);

/* Non-cryptographic hash helpers used by wl_hashmap and available for general use. */
u64 wl_hash_bytes(const void* data, usize len, u64 seed);
u64 wl_hash_str(wl_str s);
u64 wl_hash_u64(u64 value);

/* Customization hooks for wl_hashmap key hashing and equality. */
typedef u64 (*wl_hashmap_hash_fn)(const void* key, usize key_size);
typedef bool (*wl_hashmap_eq_fn)(const void* lhs, const void* rhs, usize key_size);

/* Robin Hood open-addressing hashmap storing opaque fixed-size keys and values. */
typedef struct wl_hashmap {
    u8* slots;
    usize cap;
    usize count;
    usize key_size;
    usize val_size;
    usize stride;
    wl_hashmap_hash_fn hash_fn;
    wl_hashmap_eq_fn eq_fn;
    wl_alloc_t* alloc;
} wl_hashmap;

typedef struct wl_hashmap_iter {
    const wl_hashmap* map;
    usize index;
} wl_hashmap_iter;

/* Initializes a hashmap for fixed-size keys and values. */
wl_status wl_hashmap_init(
    wl_hashmap* map,
    usize key_size,
    usize val_size,
    wl_hashmap_hash_fn hash_fn,
    wl_hashmap_eq_fn eq_fn,
    void* alloc);
/* Inserts or updates one key/value pair. */
wl_status wl_hashmap_put(wl_hashmap* map, const void* key, const void* value);
/* Returns a pointer to the stored value for key, or NULL when absent. */
void* wl_hashmap_get(const wl_hashmap* map, const void* key);
/* Removes key if present and returns true on removal. */
bool wl_hashmap_remove(wl_hashmap* map, const void* key);
/* Removes all entries without releasing the backing storage. */
void wl_hashmap_clear(wl_hashmap* map);
/* Returns the current number of entries. */
usize wl_hashmap_count(const wl_hashmap* map);
/* Releases the backing storage and clears the map state. */
void wl_hashmap_destroy(wl_hashmap* map);
/* Creates an iterator positioned before the first occupied slot. */
wl_hashmap_iter wl_hashmap_iter_new(const wl_hashmap* map);
/* Advances iter and exposes pointers to the stored key and value for the next entry. */
bool wl_hashmap_iter_next(wl_hashmap_iter* iter, void** key_out, void** val_out);

/* Comparator used by wl_sort and wl_bsearch. */
typedef int (*wl_cmp_fn)(const void* a, const void* b, void* ctx);

/* Sorts count elements in-place using cmp and optional context. */
void wl_sort(void* base, usize count, usize elem_size, wl_cmp_fn cmp, void* ctx);
/* Binary search over a sorted array. Returns the matching index or -1. */
isize wl_bsearch(const void* base, usize count, usize elem_size, const void* key, wl_cmp_fn cmp, void* ctx);

/* PCG-style random number generator state. */
typedef struct wl_rng {
    u64 state;
    u64 inc;
} wl_rng;

/* Seeds a deterministic random number generator. */
wl_rng wl_rng_seed(u64 seed);
/* Returns the next 32 random bits. */
u32 wl_rng_u32(wl_rng* rng);
/* Returns the next 64 random bits. */
u64 wl_rng_u64(wl_rng* rng);
/* Returns a double in the half-open interval [0.0, 1.0). */
f64 wl_rng_f64(wl_rng* rng);
/* Returns an integer in the half-open interval [lo, hi). */
u64 wl_rng_range(wl_rng* rng, u64 lo, u64 hi);
/* Fills buf with len pseudo-random bytes. */
void wl_rng_fill(wl_rng* rng, void* buf, usize len);

/* Absolute monotonic or wall-clock time in nanoseconds. */
typedef struct wl_time {
    i64 ns;
} wl_time;

/* Relative duration in nanoseconds. */
typedef struct wl_duration {
    i64 ns;
} wl_duration;

/* Duration constructors. */
WL_INLINE wl_duration wl_ns(i64 ns) {
    wl_duration d = { ns };
    return d;
}

WL_INLINE wl_duration wl_us(i64 us) {
    return wl_ns(us * 1000ll);
}

WL_INLINE wl_duration wl_ms(i64 ms) {
    return wl_ns(ms * 1000000ll);
}

WL_INLINE wl_duration wl_sec(i64 sec) {
    return wl_ns(sec * 1000000000ll);
}

/* Returns a - b in nanoseconds. */
wl_duration wl_time_diff(wl_time a, wl_time b);
/* Returns t + d in nanoseconds. */
wl_time wl_time_add(wl_time t, wl_duration d);
#if WL_ENABLE_PLATFORM
/* Returns a monotonic timestamp suitable for interval measurement. */
wl_time wl_time_now(void);
/* Returns the current wall-clock UTC timestamp. */
wl_time wl_time_wall(void);
/* Sleeps for at least duration, subject to OS scheduler granularity. */
wl_status wl_sleep(wl_duration duration);

/* Reads an entire file into an allocator-owned buffer and appends one trailing NUL byte. */
wl_status wl_fs_read_file(const char* path, void* alloc, u8** buf_out, usize* len_out);
/* Writes len bytes to path, creating or truncating the file. */
wl_status wl_fs_write_file(const char* path, const void* data, usize len);
/* Returns true when path exists. */
bool wl_fs_exists(const char* path);
/* Creates a directory, optionally creating all missing parents. */
wl_status wl_fs_mkdir(const char* path, bool recursive);
/* Removes one file. */
wl_status wl_fs_remove(const char* path);
#endif
/* Returns a view of the file extension including the dot, or an empty view. */
wl_str wl_path_ext(wl_str path);
/* Returns a view of the final path component without its extension. */
wl_str wl_path_stem(wl_str path);
/* Returns a view of the directory portion before the final separator. */
wl_str wl_path_dir(wl_str path);
/* Joins two path segments into buf using '/'. */
wl_status wl_path_join(char* buf, usize buf_size, wl_str a, wl_str b);

/* Minimal built-in test framework used by wl_test.c and available to downstream tests. */
typedef struct wl_test_ctx {
    const char* suite_name;
    const char* case_name;
    usize case_checks;
    usize total_checks;
    usize failed_checks;
    usize cases_run;
    usize cases_failed;
    bool case_failed;
} wl_test_ctx;

/* Signature for one test case function. */
typedef void (*wl_test_fn)(wl_test_ctx* t);

/* Named test case entry consumed by wl_test_run. */
typedef struct wl_test_case {
    const char* name;
    wl_test_fn fn;
} wl_test_case;

/* Initializes a test context for a suite run. */
void wl_test_ctx_init(wl_test_ctx* ctx, const char* suite_name);
/* Records one test expectation and prints a failure message when passed is false. */
bool wl_test_expect(
    wl_test_ctx* ctx,
    bool passed,
    const char* file,
    int line,
    const char* expr,
    const char* fmt,
    ...);
/* Runs count cases and prints a small textual summary. Returns 0 on success. */
int wl_test_run(const char* suite_name, const wl_test_case* cases, usize count);

/* Test declaration and assertion helpers. */
#define WL_TEST(name) static void name(wl_test_ctx* t)
#define WL_TEST_CASE(name) { #name, name }
#define WL_CHECK(t, expr) ((void)wl_test_expect((t), (expr), __FILE__, __LINE__, #expr, NULL))
#define WL_CHECK_MSG(t, expr, ...) ((void)wl_test_expect((t), (expr), __FILE__, __LINE__, #expr, __VA_ARGS__))
#define WL_REQUIRE(t, expr)                                                  \
    do {                                                                     \
        if (!wl_test_expect((t), (expr), __FILE__, __LINE__, #expr, NULL)) { \
            return;                                                          \
        }                                                                    \
    } while (0)
#define WL_REQUIRE_MSG(t, expr, ...)                                                \
    do {                                                                            \
        if (!wl_test_expect((t), (expr), __FILE__, __LINE__, #expr, __VA_ARGS__)) { \
            return;                                                                 \
        }                                                                           \
    } while (0)

#ifdef WL_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#if WL_ENABLE_PLATFORM
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#endif

#if WL_ENABLE_PLATFORM && WL_PLATFORM_POSIX
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#endif

#if WL_ENABLE_PLATFORM && WL_PLATFORM_WINDOWS
#include <direct.h>
#include <io.h>
#include <windows.h>
#endif

#if WL_ENABLE_PLATFORM && !WL_PLATFORM_POSIX && !WL_PLATFORM_WINDOWS
#error "wl.h does not have a platform backend for this target yet"
#endif

#if WL_ENABLE_PLATFORM
#if defined(PATH_MAX)
#define WL__PATH_MAX PATH_MAX
#elif WL_PLATFORM_WINDOWS && defined(MAX_PATH)
#define WL__PATH_MAX MAX_PATH
#else
#define WL__PATH_MAX 4096
#endif
#endif

#if WL_ENABLE_PLATFORM && WL_PLATFORM_WINDOWS
typedef struct _stat64 wl__stat_t;

WL_INLINE int wl__isatty_fd(int fd) {
    return _isatty(fd);
}

WL_INLINE int wl__fileno_file(FILE* file) {
    return _fileno(file);
}

WL_INLINE int wl__stat_path(const char* path, wl__stat_t* st) {
    return _stat64(path, st);
}

WL_INLINE int wl__mkdir_one(const char* path) {
    return _mkdir(path);
}
#elif WL_ENABLE_PLATFORM && WL_PLATFORM_POSIX
typedef struct stat wl__stat_t;

WL_INLINE int wl__isatty_fd(int fd) {
    return isatty(fd);
}

WL_INLINE int wl__fileno_file(FILE* file) {
    return fileno(file);
}

WL_INLINE int wl__stat_path(const char* path, wl__stat_t* st) {
    return stat(path, st);
}

WL_INLINE int wl__mkdir_one(const char* path) {
    return mkdir(path, 0755);
}
#endif

static void* wl__default_alloc(void* ctx, u64 size) {
    WL_UNUSED(ctx);
    if (size == 0u) {
        size = 1u;
    }
    return malloc((size_t)size);
}

static void* wl__default_realloc(void* ctx, void* ptr, u64 new_size) {
    WL_UNUSED(ctx);
    if (new_size == 0u) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, (size_t)new_size);
}

static void wl__default_free(void* ctx, void* ptr) {
    WL_UNUSED(ctx);
    free(ptr);
}

static wl_alloc_t wl__default_allocator = {
    wl__default_alloc,
    wl__default_realloc,
    wl__default_free,
};

wl_alloc_t* wl_alloc_default = &wl__default_allocator;

const char* wl_status_str(wl_status status) {
    switch (status) {
        case WL_OK:
            return "ok";
        case WL_ERR_NOMEM:
            return "out of memory";
        case WL_ERR_INVALID:
            return "invalid argument";
        case WL_ERR_NOT_FOUND:
            return "not found";
        case WL_ERR_IO:
            return "i/o error";
        case WL_ERR_OVERFLOW:
            return "overflow";
        case WL_ERR_TIMEOUT:
            return "timeout";
        case WL_ERR_UNSUPPORTED:
            return "unsupported";
        default:
            return "unknown status";
    }
}

void* wl_alloc_resize(wl_alloc_t* alloc, void* ptr, usize old_size, usize new_size) {
    wl_alloc_t* resolved = wl_as_alloc(alloc);

    if (new_size == 0u) {
        if (ptr != NULL) {
            resolved->free(resolved, ptr);
        }
        return NULL;
    }

    if (ptr == NULL) {
        return resolved->alloc(resolved, (u64)new_size);
    }

    if (resolved->realloc != NULL) {
        void* grown = resolved->realloc(resolved, ptr, (u64)new_size);
        if (grown != NULL) {
            return grown;
        }
    }

    void* copy = resolved->alloc(resolved, (u64)new_size);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, ptr, WL_MIN(old_size, new_size));
    resolved->free(resolved, ptr);
    return copy;
}

static void* wl__arena_alloc_adapter(void* ctx, u64 size) {
    return wl_arena_alloc((wl_arena*)ctx, size);
}

static void* wl__arena_realloc_adapter(void* ctx, void* ptr, u64 new_size) {
    wl_arena* arena = (wl_arena*)ctx;
    if (ptr == NULL) {
        return wl_arena_alloc(arena, new_size);
    }
    WL_UNUSED(ptr);
    WL_UNUSED(new_size);
    return NULL;
}

static void wl__arena_free_adapter(void* ctx, void* ptr) {
    WL_UNUSED(ctx);
    WL_UNUSED(ptr);
}

wl_status wl_arena_init(wl_arena* arena, u64 capacity, void* backing) {
    if (arena == NULL) {
        return WL_ERR_INVALID;
    }

    memset(arena, 0, sizeof(*arena));
    arena->base.alloc = wl__arena_alloc_adapter;
    arena->base.realloc = wl__arena_realloc_adapter;
    arena->base.free = wl__arena_free_adapter;
    arena->backing = wl_as_alloc(backing);

    if (capacity == 0u) {
        return WL_OK;
    }

    arena->buf = arena->backing->alloc(arena->backing, capacity);
    if (arena->buf == NULL) {
        return WL_ERR_NOMEM;
    }

    arena->cap = capacity;
    arena->pos = 0u;
    return WL_OK;
}

void* wl_arena_alloc(wl_arena* arena, u64 size) {
    const u64 align = (u64)WL_ALIGNOF(wl__max_align);
    u64 aligned_pos;

    if (arena == NULL) {
        return NULL;
    }

    if (size == 0u) {
        size = 1u;
    }

    aligned_pos = WL_ALIGN_UP(arena->pos, align);
    if (aligned_pos > arena->cap || size > arena->cap - aligned_pos) {
        return NULL;
    }

    arena->pos = aligned_pos + size;
    return arena->buf + aligned_pos;
}

wl_arena_savepoint wl_arena_save(const wl_arena* arena) {
    return arena != NULL ? arena->pos : 0u;
}

void wl_arena_restore(wl_arena* arena, wl_arena_savepoint savepoint) {
    if (arena == NULL) {
        return;
    }

    arena->pos = WL_MIN(savepoint, arena->cap);
}

void wl_arena_reset(wl_arena* arena) {
    if (arena != NULL) {
        arena->pos = 0u;
    }
}

void wl_arena_destroy(wl_arena* arena) {
    if (arena == NULL) {
        return;
    }

    if (arena->buf != NULL) {
        arena->backing->free(arena->backing, arena->buf);
    }

    memset(arena, 0, sizeof(*arena));
}

static wl_log_level wl__log_level = WL_LOG_TRACE;
static wl_log_callback wl__log_callback = NULL;

static const char* wl__log_level_name(wl_log_level level) {
    switch (level) {
        case WL_LOG_TRACE:
            return "TRACE";
        case WL_LOG_INFO:
            return "INFO";
        case WL_LOG_WARN:
            return "WARN";
        case WL_LOG_ERROR:
            return "ERROR";
        case WL_LOG_FATAL:
            return "FATAL";
        default:
            return "LOG";
    }
}

#if WL_ENABLE_PLATFORM && WL_PLATFORM_POSIX
static const char* wl__log_level_color(wl_log_level level) {
    switch (level) {
        case WL_LOG_TRACE:
            return "\033[36m";
        case WL_LOG_INFO:
            return "\033[32m";
        case WL_LOG_WARN:
            return "\033[33m";
        case WL_LOG_ERROR:
            return "\033[31m";
        case WL_LOG_FATAL:
            return "\033[35m";
        default:
            return "";
    }
}
#endif

void wl_log_set_level(wl_log_level level) {
    wl__log_level = level;
}

wl_log_level wl_log_get_level(void) {
    return wl__log_level;
}

void wl_log_set_callback(wl_log_callback callback) {
    wl__log_callback = callback;
}

void wl_log_vemit(wl_log_level level, const char* file, int line, const char* fmt, va_list args) {
    if (level != WL_LOG_FATAL && level < wl__log_level) {
        return;
    }

    if (wl__log_callback != NULL) {
        wl__log_callback(level, file, line, fmt, args);
        return;
    }

#if WL_ENABLE_PLATFORM && WL_PLATFORM_POSIX
    if (wl__isatty_fd(wl__fileno_file(stderr)) != 0) {
        fprintf(stderr, "%s[%s]\033[0m %s:%d: ", wl__log_level_color(level), wl__log_level_name(level), file, line);
    } else {
        fprintf(stderr, "[%s] %s:%d: ", wl__log_level_name(level), file, line);
    }
#else
    fprintf(stderr, "[%s] %s:%d: ", wl__log_level_name(level), file, line);
#endif
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
}

void wl_log_emit(wl_log_level level, const char* file, int line, const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    wl_log_vemit(level, file, line, fmt, args);
    va_end(args);
}

static void wl__assert_default_handler(const char* expr, const char* file, int line, const char* msg) {
    if (msg != NULL) {
        fprintf(stderr, "assertion failed: %s (%s) at %s:%d\n", expr, msg, file, line);
    } else {
        fprintf(stderr, "assertion failed: %s at %s:%d\n", expr, file, line);
    }
    abort();
}

static wl_assert_handler wl__assert_handler = wl__assert_default_handler;

void wl_assert_set_handler(wl_assert_handler handler) {
    wl__assert_handler = handler != NULL ? handler : wl__assert_default_handler;
}

void wl_assert_fail(const char* expr, const char* file, int line, const char* msg) {
    wl__assert_handler(expr, file, line, msg);
}

void* wl_arr_init_impl(usize elem_size, void* alloc) {
    const usize cap = 8u;
    const usize bytes = sizeof(wl_arr_hdr) + elem_size * cap;
    wl_arr_hdr* hdr;

    if (elem_size == 0u) {
        return NULL;
    }

    hdr = wl_alloc(alloc, bytes);
    if (hdr == NULL) {
        return NULL;
    }

    hdr->len = 0u;
    hdr->cap = cap;
    hdr->alloc = wl_as_alloc(alloc);
    return (u8*)hdr + sizeof(wl_arr_hdr);
}

wl_status wl_arr_maybe_grow(void** arr_ptr, usize elem_size, usize extra) {
    void* arr;
    wl_arr_hdr* hdr;
    usize need;
    usize new_cap;
    usize old_bytes;
    usize new_bytes;
    wl_alloc_t* alloc;

    if (arr_ptr == NULL || elem_size == 0u) {
        return WL_ERR_INVALID;
    }

    arr = *arr_ptr;
    if (arr == NULL) {
        arr = wl_arr_init_impl(elem_size, NULL);
        if (arr == NULL) {
            return WL_ERR_NOMEM;
        }
        *arr_ptr = arr;
    }

    hdr = wl_arr__hdr(arr);
    need = hdr->len + extra;
    if (need <= hdr->cap) {
        return WL_OK;
    }

    new_cap = hdr->cap;
    while (new_cap < need) {
        if (new_cap > (SIZE_MAX / 2u)) {
            return WL_ERR_OVERFLOW;
        }
        new_cap *= 2u;
    }

    old_bytes = sizeof(*hdr) + hdr->cap * elem_size;
    new_bytes = sizeof(*hdr) + new_cap * elem_size;
    alloc = hdr->alloc;
    hdr = wl_alloc_resize(alloc, hdr, old_bytes, new_bytes);
    if (hdr == NULL) {
        return WL_ERR_NOMEM;
    }

    hdr->cap = new_cap;
    *arr_ptr = (u8*)hdr + sizeof(*hdr);
    return WL_OK;
}

void wl_arr_free_impl(void* arr) {
    wl_arr_hdr* hdr;

    if (arr == NULL) {
        return;
    }

    hdr = wl_arr__hdr(arr);
    hdr->alloc->free(hdr->alloc, hdr);
}

wl_str wl_str_from_cstr(const char* cstr) {
    wl_str result;

    if (cstr == NULL) {
        result.ptr = NULL;
        result.len = 0u;
        return result;
    }

    result.ptr = cstr;
    result.len = strlen(cstr);
    return result;
}

bool wl_str_eq(wl_str a, wl_str b) {
    return a.len == b.len && (a.len == 0u || memcmp(a.ptr, b.ptr, a.len) == 0);
}

bool wl_str_starts_with(wl_str s, wl_str prefix) {
    return prefix.len <= s.len && (prefix.len == 0u || memcmp(s.ptr, prefix.ptr, prefix.len) == 0);
}

bool wl_str_ends_with(wl_str s, wl_str suffix) {
    return suffix.len <= s.len && (suffix.len == 0u || memcmp(s.ptr + (s.len - suffix.len), suffix.ptr, suffix.len) == 0);
}

isize wl_str_search(wl_str haystack, wl_str needle) {
    usize skip[256];
    usize i;

    if (needle.len == 0u) {
        return 0;
    }
    if (needle.len > haystack.len) {
        return -1;
    }
    if (needle.len == 1u) {
        const void* found = memchr(haystack.ptr, needle.ptr[0], haystack.len);
        return found != NULL ? (isize)((const char*)found - haystack.ptr) : -1;
    }

    for (i = 0u; i < WL_COUNTOF(skip); ++i) {
        skip[i] = needle.len;
    }
    for (i = 0u; i + 1u < needle.len; ++i) {
        skip[(u8)needle.ptr[i]] = needle.len - i - 1u;
    }

    i = 0u;
    while (i <= haystack.len - needle.len) {
        usize j = needle.len - 1u;
        while (haystack.ptr[i + j] == needle.ptr[j]) {
            if (j == 0u) {
                return (isize)i;
            }
            --j;
        }
        i += skip[(u8)haystack.ptr[i + needle.len - 1u]];
    }

    return -1;
}

static char* wl__string_data(const wl_string* s) {
    return s->data != NULL ? s->data : (char*)s->sso;
}

static wl_status wl__string_reserve(wl_string* s, usize needed) {
    char* cur;
    char* grown;
    usize new_cap;

    if (s == NULL) {
        return WL_ERR_INVALID;
    }
    if (needed <= s->cap) {
        return WL_OK;
    }

    new_cap = s->cap;
    while (new_cap < needed) {
        if (new_cap > (SIZE_MAX / 2u)) {
            return WL_ERR_OVERFLOW;
        }
        new_cap *= 2u;
    }

    cur = wl__string_data(s);
    if (cur == s->sso) {
        grown = wl_alloc(s->alloc, new_cap + 1u);
        if (grown == NULL) {
            return WL_ERR_NOMEM;
        }
        memcpy(grown, s->sso, s->len + 1u);
    } else {
        grown = wl_alloc_resize(s->alloc, cur, s->cap + 1u, new_cap + 1u);
        if (grown == NULL) {
            return WL_ERR_NOMEM;
        }
    }

    s->data = grown;
    s->cap = new_cap;
    return WL_OK;
}

wl_string wl_string_new(void* alloc) {
    wl_string s;
    s.alloc = wl_as_alloc(alloc);
    s.len = 0u;
    s.cap = WL_STRING_SSO_CAP;
    s.data = NULL;
    s.sso[0] = '\0';
    return s;
}

wl_string wl_string_from(const char* cstr, void* alloc) {
    return wl_string_from_str(wl_str_from_cstr(cstr), alloc);
}

wl_string wl_string_from_str(wl_str view, void* alloc) {
    wl_string s = wl_string_new(alloc);
    if (view.len > 0u) {
        wl_status status = wl_string_append(&s, view);
        if (status != WL_OK) {
            wl_string_destroy(&s);
            s = wl_string_new(alloc);
        }
    }
    return s;
}

void wl_string_destroy(wl_string* s) {
    if (s == NULL) {
        return;
    }
    if (s->data != NULL && s->data != s->sso) {
        s->alloc->free(s->alloc, s->data);
    }
    *s = wl_string_new(s->alloc);
}

usize wl_string_len(const wl_string* s) {
    return s != NULL ? s->len : 0u;
}

const char* wl_string_cstr(const wl_string* s) {
    return s != NULL ? wl__string_data(s) : "";
}

wl_str wl_string_as_str(const wl_string* s) {
    wl_str view;
    view.ptr = wl_string_cstr(s);
    view.len = wl_string_len(s);
    return view;
}

wl_status wl_string_append(wl_string* s, wl_str tail) {
    char* dst;
    wl_status status;

    if (s == NULL || (tail.len > 0u && tail.ptr == NULL)) {
        return WL_ERR_INVALID;
    }
    if (tail.len > SIZE_MAX - s->len - 1u) {
        return WL_ERR_OVERFLOW;
    }

    status = wl__string_reserve(s, s->len + tail.len);
    if (status != WL_OK) {
        return status;
    }

    dst = wl__string_data(s);
    memcpy(dst + s->len, tail.ptr, tail.len);
    s->len += tail.len;
    dst[s->len] = '\0';
    return WL_OK;
}

wl_status wl_string_append_cstr(wl_string* s, const char* cstr) {
    return wl_string_append(s, wl_str_from_cstr(cstr));
}

wl_status wl_string_append_char(wl_string* s, char c) {
    return wl_string_append(s, wl_str_make(&c, 1u));
}

void wl_string_clear(wl_string* s) {
    char* dst;
    if (s == NULL) {
        return;
    }
    s->len = 0u;
    dst = wl__string_data(s);
    dst[0] = '\0';
}

bool wl_string_eq(const wl_string* a, const wl_string* b) {
    return wl_str_eq(wl_string_as_str(a), wl_string_as_str(b));
}

int wl_string_cmp(const wl_string* a, const wl_string* b) {
    wl_str lhs = wl_string_as_str(a);
    wl_str rhs = wl_string_as_str(b);
    usize min_len = WL_MIN(lhs.len, rhs.len);
    int cmp = memcmp(lhs.ptr, rhs.ptr, min_len);
    if (cmp != 0) {
        return cmp;
    }
    if (lhs.len < rhs.len) {
        return -1;
    }
    if (lhs.len > rhs.len) {
        return 1;
    }
    return 0;
}

isize wl_string_find(const wl_string* s, wl_str needle) {
    return wl_str_search(wl_string_as_str(s), needle);
}

bool wl_string_starts_with(const wl_string* s, wl_str prefix) {
    return wl_str_starts_with(wl_string_as_str(s), prefix);
}

bool wl_string_ends_with(const wl_string* s, wl_str suffix) {
    return wl_str_ends_with(wl_string_as_str(s), suffix);
}

static u32 wl__utf8_decode_at(wl_str s, usize offset, usize* advance_out, bool strict) {
    const u8* p;
    u8 b0;
    u32 codepoint;

    if (advance_out != NULL) {
        *advance_out = 1u;
    }
    if (offset >= s.len) {
        return WL_UTF8_INVALID;
    }

    p = (const u8*)s.ptr + offset;
    b0 = p[0];
    if (b0 < 0x80u) {
        if (advance_out != NULL) {
            *advance_out = 1u;
        }
        return b0;
    }

    if ((b0 & 0xe0u) == 0xc0u) {
        if (offset + 1u >= s.len) {
            return WL_UTF8_INVALID;
        }
        if ((p[1] & 0xc0u) != 0x80u) {
            return WL_UTF8_INVALID;
        }
        codepoint = ((u32)(b0 & 0x1fu) << 6u) | (u32)(p[1] & 0x3fu);
        if (strict && codepoint < 0x80u) {
            return WL_UTF8_INVALID;
        }
        if (advance_out != NULL) {
            *advance_out = 2u;
        }
        return codepoint;
    }

    if ((b0 & 0xf0u) == 0xe0u) {
        if (offset + 2u >= s.len) {
            return WL_UTF8_INVALID;
        }
        if ((p[1] & 0xc0u) != 0x80u || (p[2] & 0xc0u) != 0x80u) {
            return WL_UTF8_INVALID;
        }
        codepoint = ((u32)(b0 & 0x0fu) << 12u) | ((u32)(p[1] & 0x3fu) << 6u) | (u32)(p[2] & 0x3fu);
        if (strict && (codepoint < 0x800u || (codepoint >= 0xd800u && codepoint <= 0xdfffu))) {
            return WL_UTF8_INVALID;
        }
        if (advance_out != NULL) {
            *advance_out = 3u;
        }
        return codepoint;
    }

    if ((b0 & 0xf8u) == 0xf0u) {
        if (offset + 3u >= s.len) {
            return WL_UTF8_INVALID;
        }
        if ((p[1] & 0xc0u) != 0x80u || (p[2] & 0xc0u) != 0x80u || (p[3] & 0xc0u) != 0x80u) {
            return WL_UTF8_INVALID;
        }
        codepoint = ((u32)(b0 & 0x07u) << 18u) | ((u32)(p[1] & 0x3fu) << 12u) | ((u32)(p[2] & 0x3fu) << 6u) | (u32)(p[3] & 0x3fu);
        if (strict && (codepoint < 0x10000u || codepoint > 0x10ffffu)) {
            return WL_UTF8_INVALID;
        }
        if (advance_out != NULL) {
            *advance_out = 4u;
        }
        return codepoint;
    }

    return WL_UTF8_INVALID;
}

usize wl_utf8_len(wl_str s) {
    usize offset = 0u;
    usize count = 0u;
    while (offset < s.len) {
        (void)wl_utf8_next(s, &offset);
        ++count;
    }
    return count;
}

u32 wl_utf8_next(wl_str s, usize* offset) {
    usize local_off;
    usize advance = 1u;
    u32 cp;

    if (offset == NULL) {
        return WL_UTF8_INVALID;
    }

    local_off = *offset;
    cp = wl__utf8_decode_at(s, local_off, &advance, true);
    *offset = WL_MIN(local_off + advance, s.len);
    return cp;
}

u32 wl_utf8_decode(wl_str s, usize offset) {
    return wl__utf8_decode_at(s, offset, NULL, true);
}

usize wl_utf8_encode(u8* buf, u32 codepoint) {
    if (buf == NULL) {
        return 0u;
    }
    if (codepoint <= 0x7fu) {
        buf[0] = (u8)codepoint;
        return 1u;
    }
    if (codepoint <= 0x7ffu) {
        buf[0] = (u8)(0xc0u | (codepoint >> 6u));
        buf[1] = (u8)(0x80u | (codepoint & 0x3fu));
        return 2u;
    }
    if (codepoint <= 0xffffu && !(codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
        buf[0] = (u8)(0xe0u | (codepoint >> 12u));
        buf[1] = (u8)(0x80u | ((codepoint >> 6u) & 0x3fu));
        buf[2] = (u8)(0x80u | (codepoint & 0x3fu));
        return 3u;
    }
    if (codepoint <= 0x10ffffu) {
        buf[0] = (u8)(0xf0u | (codepoint >> 18u));
        buf[1] = (u8)(0x80u | ((codepoint >> 12u) & 0x3fu));
        buf[2] = (u8)(0x80u | ((codepoint >> 6u) & 0x3fu));
        buf[3] = (u8)(0x80u | (codepoint & 0x3fu));
        return 4u;
    }
    return 0u;
}

bool wl_utf8_valid(wl_str s) {
    usize offset = 0u;
    while (offset < s.len) {
        usize advance = 1u;
        if (wl__utf8_decode_at(s, offset, &advance, true) == WL_UTF8_INVALID) {
            return false;
        }
        offset += advance;
    }
    return true;
}

WL_INLINE u64 wl__rotl64(u64 x, u32 r) {
    return (x << r) | (x >> (64u - r));
}

u64 wl_hash_u64(u64 value) {
    value += 0x9e3779b97f4a7c15ull;
    value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
    value ^= value >> 31u;
    return value;
}

u64 wl_hash_bytes(const void* data, usize len, u64 seed) {
    const u8* ptr = (const u8*)data;
    u64 hash = seed ^ (len * 0x9e3779b97f4a7c15ull);

    while (len >= sizeof(u64)) {
        u64 lane;
        memcpy(&lane, ptr, sizeof(lane));
        hash ^= wl_hash_u64(lane + 0x517cc1b727220a95ull);
        hash = wl__rotl64(hash, 27u) * 0x3c79ac492ba7b653ull + 0x1c69b3f74ac4ae35ull;
        ptr += sizeof(u64);
        len -= sizeof(u64);
    }

    if (len > 0u) {
        u64 tail = 0u;
        memcpy(&tail, ptr, len);
        hash ^= wl_hash_u64(tail);
    }

    hash = wl_hash_u64(hash);
    return hash != 0u ? hash : 1u;
}

u64 wl_hash_str(wl_str s) {
    return wl_hash_bytes(s.ptr, s.len, 0u);
}

static u64 wl__hashmap_default_hash(const void* key, usize key_size) {
    return wl_hash_bytes(key, key_size, 0u);
}

static bool wl__hashmap_default_eq(const void* lhs, const void* rhs, usize key_size) {
    return memcmp(lhs, rhs, key_size) == 0;
}

static u64* wl__hashmap_slot_hash(const wl_hashmap* map, usize index) {
    return (u64*)(map->slots + index * map->stride);
}

static void* wl__hashmap_slot_key(const wl_hashmap* map, usize index) {
    return map->slots + index * map->stride + sizeof(u64);
}

static void* wl__hashmap_slot_val(const wl_hashmap* map, usize index) {
    return map->slots + index * map->stride + sizeof(u64) + map->key_size;
}

static usize wl__hashmap_ideal_index(const wl_hashmap* map, u64 hash) {
    return (usize)(hash & (u64)(map->cap - 1u));
}

static usize wl__hashmap_probe_distance(const wl_hashmap* map, usize index, u64 hash) {
    usize ideal = wl__hashmap_ideal_index(map, hash);
    return (index + map->cap - ideal) & (map->cap - 1u);
}

static void wl__hashmap_write_slot(wl_hashmap* map, usize index, u64 hash, const void* key, const void* value) {
    *wl__hashmap_slot_hash(map, index) = hash;
    memcpy(wl__hashmap_slot_key(map, index), key, map->key_size);
    memcpy(wl__hashmap_slot_val(map, index), value, map->val_size);
}

static void wl__hashmap_insert_at(wl_hashmap* map, usize index, u64 hash, const void* key, const void* value) {
    usize empty = index;

    while (*wl__hashmap_slot_hash(map, empty) != 0u) {
        empty = (empty + 1u) & (map->cap - 1u);
    }

    while (empty != index) {
        usize prev = (empty + map->cap - 1u) & (map->cap - 1u);
        memcpy(map->slots + empty * map->stride, map->slots + prev * map->stride, map->stride);
        empty = prev;
    }

    wl__hashmap_write_slot(map, index, hash, key, value);
    ++map->count;
}

static wl_status wl__hashmap_resize(wl_hashmap* map, usize new_cap);

wl_status wl_hashmap_init(
    wl_hashmap* map,
    usize key_size,
    usize val_size,
    wl_hashmap_hash_fn hash_fn,
    wl_hashmap_eq_fn eq_fn,
    void* alloc) {
    if (map == NULL || key_size == 0u || val_size == 0u) {
        return WL_ERR_INVALID;
    }

    memset(map, 0, sizeof(*map));
    map->key_size = key_size;
    map->val_size = val_size;
    map->stride = sizeof(u64) + key_size + val_size;
    map->hash_fn = hash_fn != NULL ? hash_fn : wl__hashmap_default_hash;
    map->eq_fn = eq_fn != NULL ? eq_fn : wl__hashmap_default_eq;
    map->alloc = wl_as_alloc(alloc);
    return wl__hashmap_resize(map, 16u);
}

static wl_status wl__hashmap_insert_with_hash(wl_hashmap* map, const void* key, const void* value, u64 hash) {
    usize index;
    usize dist = 0u;
    u64 cur_hash;

    cur_hash = hash != 0u ? hash : 1u;
    index = wl__hashmap_ideal_index(map, cur_hash);

    for (;;) {
        u64* slot_hash = wl__hashmap_slot_hash(map, index);
        if (*slot_hash == 0u) {
            wl__hashmap_write_slot(map, index, cur_hash, key, value);
            ++map->count;
            return WL_OK;
        }

        if (*slot_hash == cur_hash && map->eq_fn(wl__hashmap_slot_key(map, index), key, map->key_size)) {
            memcpy(wl__hashmap_slot_val(map, index), value, map->val_size);
            return WL_OK;
        }

        if (wl__hashmap_probe_distance(map, index, *slot_hash) < dist) {
            wl__hashmap_insert_at(map, index, cur_hash, key, value);
            return WL_OK;
        }

        index = (index + 1u) & (map->cap - 1u);
        ++dist;
    }
}

static wl_status wl__hashmap_resize(wl_hashmap* map, usize new_cap) {
    u8* new_slots;
    u8* old_slots;
    usize old_cap;
    usize i;

    if (!WL_IS_POWER_OF_2(new_cap)) {
        return WL_ERR_INVALID;
    }

    new_slots = wl_alloc(map->alloc, new_cap * map->stride);
    if (new_slots == NULL) {
        return WL_ERR_NOMEM;
    }
    memset(new_slots, 0, new_cap * map->stride);

    old_slots = map->slots;
    old_cap = map->cap;
    map->slots = new_slots;
    map->cap = new_cap;
    map->count = 0u;

    for (i = 0u; i < old_cap; ++i) {
        if (old_slots != NULL) {
            u64 hash = *(u64*)(old_slots + i * map->stride);
            if (hash != 0u) {
                void* key = old_slots + i * map->stride + sizeof(u64);
                void* val = old_slots + i * map->stride + sizeof(u64) + map->key_size;
                wl_status status = wl__hashmap_insert_with_hash(map, key, val, hash);
                if (status != WL_OK) {
                    map->alloc->free(map->alloc, new_slots);
                    map->slots = old_slots;
                    map->cap = old_cap;
                    return status;
                }
            }
        }
    }

    if (old_slots != NULL) {
        map->alloc->free(map->alloc, old_slots);
    }

    return WL_OK;
}

wl_status wl_hashmap_put(wl_hashmap* map, const void* key, const void* value) {
    u64 hash;

    if (map == NULL || key == NULL || value == NULL) {
        return WL_ERR_INVALID;
    }

    if ((map->count + 1u) * 4u >= map->cap * 3u) {
        wl_status status = wl__hashmap_resize(map, map->cap * 2u);
        if (status != WL_OK) {
            return status;
        }
    }

    hash = map->hash_fn(key, map->key_size);
    if (hash == 0u) {
        hash = 1u;
    }
    return wl__hashmap_insert_with_hash(map, key, value, hash);
}

void* wl_hashmap_get(const wl_hashmap* map, const void* key) {
    usize index;
    usize dist = 0u;
    u64 hash;

    if (map == NULL || key == NULL || map->cap == 0u) {
        return NULL;
    }

    hash = map->hash_fn(key, map->key_size);
    if (hash == 0u) {
        hash = 1u;
    }
    index = wl__hashmap_ideal_index(map, hash);

    for (;;) {
        u64 slot_hash = *wl__hashmap_slot_hash(map, index);
        if (slot_hash == 0u) {
            return NULL;
        }
        if (wl__hashmap_probe_distance(map, index, slot_hash) < dist) {
            return NULL;
        }
        if (slot_hash == hash && map->eq_fn(wl__hashmap_slot_key(map, index), key, map->key_size)) {
            return wl__hashmap_slot_val(map, index);
        }
        index = (index + 1u) & (map->cap - 1u);
        ++dist;
    }
}

bool wl_hashmap_remove(wl_hashmap* map, const void* key) {
    usize index;
    usize dist = 0u;
    u64 hash;

    if (map == NULL || key == NULL || map->count == 0u) {
        return false;
    }

    hash = map->hash_fn(key, map->key_size);
    if (hash == 0u) {
        hash = 1u;
    }
    index = wl__hashmap_ideal_index(map, hash);

    for (;;) {
        u64* slot_hash = wl__hashmap_slot_hash(map, index);
        if (*slot_hash == 0u) {
            return false;
        }
        if (wl__hashmap_probe_distance(map, index, *slot_hash) < dist) {
            return false;
        }
        if (*slot_hash == hash && map->eq_fn(wl__hashmap_slot_key(map, index), key, map->key_size)) {
            usize hole = index;
            for (;;) {
                usize next = (hole + 1u) & (map->cap - 1u);
                u64 next_hash = *wl__hashmap_slot_hash(map, next);
                if (next_hash == 0u || wl__hashmap_probe_distance(map, next, next_hash) == 0u) {
                    memset(map->slots + hole * map->stride, 0, map->stride);
                    --map->count;
                    return true;
                }
                memcpy(map->slots + hole * map->stride, map->slots + next * map->stride, map->stride);
                hole = next;
            }
        }
        index = (index + 1u) & (map->cap - 1u);
        ++dist;
    }
}

void wl_hashmap_clear(wl_hashmap* map) {
    if (map != NULL && map->slots != NULL) {
        memset(map->slots, 0, map->cap * map->stride);
        map->count = 0u;
    }
}

usize wl_hashmap_count(const wl_hashmap* map) {
    return map != NULL ? map->count : 0u;
}

void wl_hashmap_destroy(wl_hashmap* map) {
    if (map == NULL) {
        return;
    }
    if (map->slots != NULL) {
        map->alloc->free(map->alloc, map->slots);
    }
    memset(map, 0, sizeof(*map));
}

wl_hashmap_iter wl_hashmap_iter_new(const wl_hashmap* map) {
    wl_hashmap_iter iter;
    iter.map = map;
    iter.index = 0u;
    return iter;
}

bool wl_hashmap_iter_next(wl_hashmap_iter* iter, void** key_out, void** val_out) {
    if (iter == NULL || iter->map == NULL) {
        return false;
    }

    while (iter->index < iter->map->cap) {
        usize index = iter->index++;
        if (*wl__hashmap_slot_hash(iter->map, index) != 0u) {
            if (key_out != NULL) {
                *key_out = wl__hashmap_slot_key(iter->map, index);
            }
            if (val_out != NULL) {
                *val_out = wl__hashmap_slot_val(iter->map, index);
            }
            return true;
        }
    }

    return false;
}

static void wl__swap_bytes(u8* a, u8* b, usize size) {
    usize i;
    if (a == b) {
        return;
    }
    for (i = 0u; i < size; ++i) {
        u8 tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

static void wl__insertion_sort(u8* base, usize count, usize size, wl_cmp_fn cmp, void* ctx) {
    usize i;
    for (i = 1u; i < count; ++i) {
        usize j = i;
        while (j > 0u && cmp(base + (j - 1u) * size, base + j * size, ctx) > 0) {
            wl__swap_bytes(base + (j - 1u) * size, base + j * size, size);
            --j;
        }
    }
}

static usize wl__quick_sort_partition(u8* base, usize lo, usize hi, usize size, wl_cmp_fn cmp, void* ctx) {
    usize pivot_index = lo + (hi - lo) / 2u;
    usize store = lo;
    usize i;

    wl__swap_bytes(base + pivot_index * size, base + hi * size, size);
    for (i = lo; i < hi; ++i) {
        if (cmp(base + i * size, base + hi * size, ctx) < 0) {
            wl__swap_bytes(base + store * size, base + i * size, size);
            ++store;
        }
    }
    wl__swap_bytes(base + store * size, base + hi * size, size);
    return store;
}

static void wl__quick_sort(u8* base, isize lo, isize hi, usize size, wl_cmp_fn cmp, void* ctx) {
    while (lo < hi) {
        isize pivot = (isize)wl__quick_sort_partition(base, (usize)lo, (usize)hi, size, cmp, ctx);
        isize left_hi = pivot - 1;
        isize right_lo = pivot + 1;

        if (left_hi - lo < hi - right_lo) {
            if (lo < left_hi) {
                wl__quick_sort(base, lo, left_hi, size, cmp, ctx);
            }
            lo = right_lo;
        } else {
            if (right_lo < hi) {
                wl__quick_sort(base, right_lo, hi, size, cmp, ctx);
            }
            hi = left_hi;
        }
    }
}

void wl_sort(void* base, usize count, usize elem_size, wl_cmp_fn cmp, void* ctx) {
    if (base == NULL || elem_size == 0u || cmp == NULL || count < 2u) {
        return;
    }
    if (count < 32u) {
        wl__insertion_sort((u8*)base, count, elem_size, cmp, ctx);
        return;
    }
    wl__quick_sort((u8*)base, 0, (isize)count - 1, elem_size, cmp, ctx);
}

isize wl_bsearch(const void* base, usize count, usize elem_size, const void* key, wl_cmp_fn cmp, void* ctx) {
    usize lo = 0u;
    usize hi = count;

    if (base == NULL || key == NULL || elem_size == 0u || cmp == NULL) {
        return -1;
    }

    while (lo < hi) {
        usize mid = lo + (hi - lo) / 2u;
        const void* elem = (const u8*)base + mid * elem_size;
        int order = cmp(elem, key, ctx);
        if (order < 0) {
            lo = mid + 1u;
        } else if (order > 0) {
            hi = mid;
        } else {
            return (isize)mid;
        }
    }

    return -1;
}

u32 wl_rng_u32(wl_rng* rng) {
    u64 oldstate;
    u32 xorshifted;
    u32 rot;

    oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ull + (rng->inc | 1ull);
    xorshifted = (u32)(((oldstate >> 18u) ^ oldstate) >> 27u);
    rot = (u32)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-(i32)rot) & 31));
}

wl_rng wl_rng_seed(u64 seed) {
    wl_rng rng;
    rng.state = 0u;
    rng.inc = (seed << 1u) | 1u;
    (void)wl_rng_u32(&rng);
    rng.state += seed ^ 0x9e3779b97f4a7c15ull;
    (void)wl_rng_u32(&rng);
    return rng;
}

u64 wl_rng_u64(wl_rng* rng) {
    return ((u64)wl_rng_u32(rng) << 32u) | wl_rng_u32(rng);
}

f64 wl_rng_f64(wl_rng* rng) {
    return (f64)(wl_rng_u64(rng) >> 11u) * (1.0 / 9007199254740992.0);
}

u64 wl_rng_range(wl_rng* rng, u64 lo, u64 hi) {
    u64 span;
    u64 threshold;
    for (;;) {
        u64 value;
        if (hi <= lo) {
            return lo;
        }
        span = hi - lo;
        threshold = (u64)(-span) % span;
        value = wl_rng_u64(rng);
        if (value >= threshold) {
            return lo + (value % span);
        }
    }
}

void wl_rng_fill(wl_rng* rng, void* buf, usize len) {
    u8* dst = (u8*)buf;
    usize i = 0u;
    while (i < len) {
        u64 value = wl_rng_u64(rng);
        usize chunk = WL_MIN(len - i, sizeof(value));
        memcpy(dst + i, &value, chunk);
        i += chunk;
    }
}

wl_duration wl_time_diff(wl_time a, wl_time b) {
    wl_duration d;
    d.ns = a.ns - b.ns;
    return d;
}

wl_time wl_time_add(wl_time t, wl_duration d) {
    wl_time out;
    out.ns = t.ns + d.ns;
    return out;
}

#if WL_ENABLE_PLATFORM

#if WL_PLATFORM_WINDOWS
static wl_time wl__time_from_filetime(FILETIME ft) {
    ULARGE_INTEGER ticks;
    wl_time t;

    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;
    t.ns = (i64)((ticks.QuadPart - 116444736000000000ull) * 100ull);
    return t;
}

wl_time wl_time_now(void) {
    static LARGE_INTEGER freq = { 0 };
    LARGE_INTEGER counter;
    wl_time t;

    if (freq.QuadPart == 0) {
        if (QueryPerformanceFrequency(&freq) == 0) {
            t.ns = 0ll;
            return t;
        }
    }

    QueryPerformanceCounter(&counter);
    t.ns = (i64)((counter.QuadPart / freq.QuadPart) * 1000000000ll + ((counter.QuadPart % freq.QuadPart) * 1000000000ll) / freq.QuadPart);
    return t;
}

wl_time wl_time_wall(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return wl__time_from_filetime(ft);
}

wl_status wl_sleep(wl_duration duration) {
    if (duration.ns <= 0ll) {
        return WL_OK;
    }

    Sleep((DWORD)((duration.ns + 999999ll) / 1000000ll));
    return WL_OK;
}
#elif WL_PLATFORM_POSIX
static wl_time wl__time_from_timespec(struct timespec ts) {
    wl_time t;
    t.ns = (i64)ts.tv_sec * 1000000000ll + (i64)ts.tv_nsec;
    return t;
}

wl_time wl_time_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return wl__time_from_timespec(ts);
}

wl_time wl_time_wall(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return wl__time_from_timespec(ts);
}

wl_status wl_sleep(wl_duration duration) {
    struct timespec req;
    struct timespec rem;

    if (duration.ns <= 0ll) {
        return WL_OK;
    }

    req.tv_sec = duration.ns / 1000000000ll;
    req.tv_nsec = duration.ns % 1000000000ll;
    while (nanosleep(&req, &rem) != 0) {
        if (errno != EINTR) {
            return WL_ERR_IO;
        }
        req = rem;
    }
    return WL_OK;
}
#endif

wl_status wl_fs_read_file(const char* path, void* alloc, u8** buf_out, usize* len_out) {
    wl__stat_t st;
    FILE* file;
    u8* buf;
    usize file_size;
    usize offset = 0u;

    if (path == NULL || buf_out == NULL) {
        return WL_ERR_INVALID;
    }

    if (wl__stat_path(path, &st) != 0) {
        return errno == ENOENT ? WL_ERR_NOT_FOUND : WL_ERR_IO;
    }
    if (st.st_size < 0) {
        return WL_ERR_IO;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return errno == ENOENT ? WL_ERR_NOT_FOUND : WL_ERR_IO;
    }

    file_size = (usize)st.st_size;
    buf = wl_alloc(alloc, file_size + 1u);
    if (buf == NULL) {
        fclose(file);
        return WL_ERR_NOMEM;
    }

    while (offset < file_size) {
        size_t n = fread(buf + offset, 1u, file_size - offset, file);
        if (n == 0u) {
            if (ferror(file) != 0) {
                wl_free(alloc, buf);
                fclose(file);
                return WL_ERR_IO;
            }
            break;
        }
        offset += n;
    }
    buf[offset] = 0u;

    fclose(file);
    *buf_out = buf;
    if (len_out != NULL) {
        *len_out = offset;
    }
    return WL_OK;
}

wl_status wl_fs_write_file(const char* path, const void* data, usize len) {
    FILE* file;
    usize offset = 0u;

    if (path == NULL || (len > 0u && data == NULL)) {
        return WL_ERR_INVALID;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return WL_ERR_IO;
    }

    while (offset < len) {
        size_t n = fwrite((const u8*)data + offset, 1u, len - offset, file);
        if (n == 0u) {
            fclose(file);
            return WL_ERR_IO;
        }
        offset += n;
    }

    fclose(file);
    return WL_OK;
}

bool wl_fs_exists(const char* path) {
    wl__stat_t st;
    return path != NULL && wl__stat_path(path, &st) == 0;
}

static bool wl__path_is_drive_prefix(const char* path, usize len) {
    return len == 2u && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':';
}

wl_status wl_fs_mkdir(const char* path, bool recursive) {
    char tmp[WL__PATH_MAX];
    usize len;
    usize i;

    if (path == NULL || path[0] == '\0') {
        return WL_ERR_INVALID;
    }

    len = strlen(path);
    if (len >= sizeof(tmp)) {
        return WL_ERR_OVERFLOW;
    }

    memcpy(tmp, path, len + 1u);
    if (!recursive) {
        if (wl__mkdir_one(tmp) == 0 || errno == EEXIST) {
            return WL_OK;
        }
        return WL_ERR_IO;
    }

    for (i = 1u; i <= len; ++i) {
        if (tmp[i] == '/' || tmp[i] == '\\' || tmp[i] == '\0') {
            char saved = tmp[i];
            tmp[i] = '\0';
            if (tmp[0] != '\0' && !wl__path_is_drive_prefix(tmp, i) && wl__mkdir_one(tmp) != 0 && errno != EEXIST) {
                return WL_ERR_IO;
            }
            tmp[i] = saved;
        }
    }

    return WL_OK;
}

wl_status wl_fs_remove(const char* path) {
    if (path == NULL) {
        return WL_ERR_INVALID;
    }
    if (remove(path) != 0) {
        return errno == ENOENT ? WL_ERR_NOT_FOUND : WL_ERR_IO;
    }
    return WL_OK;
}

#endif

static isize wl__path_last_sep(wl_str path) {
    isize i;
    for (i = (isize)path.len - 1; i >= 0; --i) {
        if (path.ptr[i] == '/' || path.ptr[i] == '\\') {
            return i;
        }
    }
    return -1;
}

wl_str wl_path_ext(wl_str path) {
    isize sep = wl__path_last_sep(path);
    isize i;
    for (i = (isize)path.len - 1; i > sep; --i) {
        if (path.ptr[i] == '.') {
            return wl_str_make(path.ptr + i, (usize)((isize)path.len - i));
        }
    }
    return wl_str_make(path.ptr + path.len, 0u);
}

wl_str wl_path_stem(wl_str path) {
    isize sep = wl__path_last_sep(path);
    isize start = sep + 1;
    isize end = (isize)path.len;
    isize i;
    for (i = (isize)path.len - 1; i > sep; --i) {
        if (path.ptr[i] == '.') {
            end = i;
            break;
        }
    }
    return wl_str_make(path.ptr + start, (usize)(end - start));
}

wl_str wl_path_dir(wl_str path) {
    isize sep = wl__path_last_sep(path);
    if (sep < 0) {
        return wl_str_make(path.ptr, 0u);
    }
    return wl_str_make(path.ptr, (usize)sep);
}

wl_status wl_path_join(char* buf, usize buf_size, wl_str a, wl_str b) {
    usize need;
    bool needs_sep;

    if (buf == NULL || buf_size == 0u) {
        return WL_ERR_INVALID;
    }

    needs_sep = a.len > 0u && b.len > 0u && a.ptr[a.len - 1u] != '/' && b.ptr[0] != '/';
    need = a.len + b.len + (needs_sep ? 1u : 0u) + 1u;
    if (need > buf_size) {
        return WL_ERR_OVERFLOW;
    }

    memcpy(buf, a.ptr, a.len);
    if (needs_sep) {
        buf[a.len] = '/';
        memcpy(buf + a.len + 1u, b.ptr, b.len);
        buf[a.len + 1u + b.len] = '\0';
    } else {
        memcpy(buf + a.len, b.ptr, b.len);
        buf[a.len + b.len] = '\0';
    }

    return WL_OK;
}

void wl_test_ctx_init(wl_test_ctx* ctx, const char* suite_name) {
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->suite_name = suite_name != NULL ? suite_name : "wl";
}

bool wl_test_expect(
    wl_test_ctx* ctx,
    bool passed,
    const char* file,
    int line,
    const char* expr,
    const char* fmt,
    ...) {
    if (ctx == NULL) {
        return passed;
    }

    ++ctx->total_checks;
    ++ctx->case_checks;
    if (passed) {
        return true;
    }

    ctx->case_failed = true;
    ++ctx->failed_checks;
    fprintf(
        stderr,
        "FAIL %s/%s at %s:%d: %s",
        ctx->suite_name != NULL ? ctx->suite_name : "wl",
        ctx->case_name != NULL ? ctx->case_name : "<case>",
        file,
        line,
        expr != NULL ? expr : "check failed");
    if (fmt != NULL) {
        va_list args;
        fputs(" - ", stderr);
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
    fputc('\n', stderr);
    return false;
}

int wl_test_run(const char* suite_name, const wl_test_case* cases, usize count) {
    wl_test_ctx ctx;
    usize i;

    if (cases == NULL && count != 0u) {
        fprintf(stderr, "FAIL %s: null test case list\n", suite_name != NULL ? suite_name : "wl");
        return 1;
    }

    wl_test_ctx_init(&ctx, suite_name);
    printf("suite %s\n", ctx.suite_name);
    for (i = 0u; i < count; ++i) {
        ctx.case_name = cases[i].name != NULL ? cases[i].name : "<unnamed>";
        ctx.case_checks = 0u;
        ctx.case_failed = false;
        ++ctx.cases_run;

        if (cases[i].fn == NULL) {
            (void)wl_test_expect(&ctx, false, __FILE__, __LINE__, "cases[i].fn != NULL", "test case function is null");
        } else {
            cases[i].fn(&ctx);
        }

        if (ctx.case_failed) {
            ++ctx.cases_failed;
            printf("  FAIL %s (%zu checks)\n", ctx.case_name, ctx.case_checks);
        } else {
            printf("  PASS %s (%zu checks)\n", ctx.case_name, ctx.case_checks);
        }
    }

    printf(
        "summary: %zu cases, %zu failed, %zu checks, %zu failed\n",
        ctx.cases_run,
        ctx.cases_failed,
        ctx.total_checks,
        ctx.failed_checks);
    return ctx.cases_failed == 0u ? 0 : 1;
}

#endif

#endif

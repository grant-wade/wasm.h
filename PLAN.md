# wl.h — Design & Planning Document

## What This Is

`wl.h` is a single-header C23 utility library for personal use. It exists to eliminate the boilerplate that shows up in every C project: type aliases, dynamic arrays, hash maps, string handling, allocator plumbing, and the handful of OS abstractions (time, file I/O, RNG) that are annoying to write from scratch each time.

The library follows the stb single-header convention:

```c
#include "wl.h"              // declarations only

// In one .c file:
#define WL_IMPLEMENTATION
#include "wl.h"              // pulls in all function bodies
```

Drop one file into a project and go.

## Design Philosophy

**Allocator-everywhere.** Every function that touches the heap takes a `wl_alloc_t*`. This is the single most important decision in the library. It means any subsystem — arena, pool, tracking allocator, mimalloc — can be swapped in without changing call sites. Pass `NULL` to use the default malloc-backed allocator, so there's zero friction if you don't care.

**Non-owning views as the default currency.** Functions accept `wl_str` (pointer + length) rather than `const char*`. This eliminates redundant `strlen` calls, lets you work with substrings without copying, and cleanly separates "looking at data" from "owning data." The owned `wl_string` type exists for when you need to build or mutate strings.

**No hidden allocations.** If a function allocates, it's obvious from the signature (it takes an allocator). If it doesn't allocate, it won't. No global state, no surprise mallocs.

**Errors are values.** Functions return `wl_status` codes. No errno globals, no exceptions, no setjmp. The status enum is small and stable — out of memory, invalid argument, I/O error, not found, overflow, timeout. That covers nearly everything.

**Compile clean or don't ship.** The library is designed to build with `-Wall -Wextra -Werror -fsanitize=address,undefined` in dev. Every section should survive this.

---

## Module-by-Module Design

### 1. Type Aliases & Utility Macros

**Why:** `uint8_t` is 8 characters. `u8` is 2. When you're writing C all day, the short forms are worth it. Same logic for `usize`/`isize` over `size_t`/`ptrdiff_t`.

**What's included:**

- Fixed-width integer aliases: `u8`–`u64`, `i8`–`i64`
- Float aliases: `f32`, `f64`
- Size types: `usize`, `isize`
- `WL_MIN`, `WL_MAX`, `WL_CLAMP`
- `WL_COUNTOF(arr)` — element count for stack arrays
- `WL_ALIGN_UP`, `WL_ALIGN_DOWN`, `WL_IS_POWER_OF_2`
- `WL_KB(n)`, `WL_MB(n)`, `WL_GB(n)` — byte-size literals
- `wl_likely` / `wl_unlikely` — branch hints (compile to `__builtin_expect` on GCC/Clang, no-op elsewhere)
- `WL_UNUSED(x)` — suppress unused-parameter warnings
- `WL_STRINGIFY`, `WL_CONCAT` — token pasting helpers

**Platform & compiler detection** also lives here: `WL_PLATFORM_POSIX`, `WL_PLATFORM_WINDOWS`, `WL_PLATFORM_MACOS`, `WL_PLATFORM_LINUX`, `WL_COMPILER_CLANG`, etc. These are auto-detected from predefined macros and gate platform-specific code in the implementation section.

---

### 2. Status Codes

**Why:** A consistent error vocabulary across the entire library. Instead of each module inventing its own error returns, everything speaks `wl_status`.

```c
typedef enum {
    WL_OK = 0,
    WL_ERR_NOMEM,
    WL_ERR_INVALID,
    WL_ERR_NOT_FOUND,
    WL_ERR_IO,
    WL_ERR_OVERFLOW,
    WL_ERR_TIMEOUT,
    WL_ERR_UNSUPPORTED,
} wl_status;
```

Negative values, zero for success. `wl_status_str()` gives you a human-readable string for logging.

The set is intentionally small. If a module needs more granularity, it can provide its own detail mechanism alongside the status code — but the top-level return type stays uniform.

---

### 3. Allocator Vtable

**Why:** This is the backbone. Every data structure, every buffer, every dynamic allocation flows through `wl_alloc_t`. It's a struct of three function pointers with minimal, obvious signatures:

```c
typedef void *(*wl_alloc_fn)(void *ctx, wl_u64 size);
typedef void *(*wl_realloc_fn)(void *ctx, void *ptr, wl_u64 new_size);
typedef void  (*wl_free_fn)(void *ctx, void *ptr);

typedef struct wl_alloc_t {
    wl_alloc_fn   alloc;
    wl_realloc_fn realloc;
    wl_free_fn    free;
} wl_alloc_t;
```

**Design decisions:**

- **`void* ctx` as the first parameter** rather than a typed self-pointer. The function pointers don't know or care what the allocator *is* — they receive an opaque context. For the default malloc-backed allocator, ctx is just the `wl_alloc_t*` itself (unused). For a custom allocator like an arena, the arena embeds `wl_alloc_t` as its first member and the alloc function casts ctx back to the arena type.
- **No `old_size` on realloc/free.** Keeps signatures simple and mirrors what malloc/realloc/free actually need. Allocators that need size tracking (arenas, pools) already know their block sizes internally. This avoids burdening every call site with bookkeeping it doesn't care about.
- **No alignment parameter.** `_Alignof(max_align_t)` covers the vast majority of allocations. The rare overaligned case can use a specialized allocator or `aligned_alloc` directly — not worth polluting every call site.
- **`wl_u64` for sizes** — explicit 64-bit width, no ambiguity on 32-bit platforms.

**Convenience macros for call sites:**

```c
#define wl_alloc(a, size)           WL_AS_ALLOC(a)->alloc(WL_AS_ALLOC(a), size)
#define wl_realloc(a, ptr, new_sz)  WL_AS_ALLOC(a)->realloc(WL_AS_ALLOC(a), ptr, new_sz)
#define wl_free(a, ptr)             WL_AS_ALLOC(a)->free(WL_AS_ALLOC(a), ptr)
```

Where `WL_AS_ALLOC(a)` null-coalesces to a global default allocator:

```c
extern wl_alloc_t *wl_alloc_default;
#define WL_AS_ALLOC(a) ((a) ? (wl_alloc_t*)(a) : wl_alloc_default)
```

This means any function that takes an allocator can just accept `NULL` to mean "use malloc." No ceremony, no threading a default through every call chain. The macro passes the allocator pointer itself as the ctx argument — custom allocators that embed `wl_alloc_t` as their first member can cast back to recover their state.

**Embedding pattern for custom allocators:**

```c
typedef struct {
    wl_alloc_t base;    // must be first member
    u8*        buf;
    u64        cap;
    u64        pos;
} wl_arena;
```

The arena's alloc function receives `ctx` (which is `&arena.base`), casts to `wl_arena*`, and bumps the pointer. No separate context field needed — the struct layout *is* the context.

---

### 4. Arena Allocator

**Why:** The most useful allocator pattern in C. Bump a pointer forward, free everything at once. Perfect for request handling, frame allocation, temporary parse trees — any situation with a clear lifetime boundary.

**Structure:** The arena embeds `wl_alloc_t` as its first member, so a `wl_arena*` can be passed directly anywhere a `wl_alloc_t*` is expected — the convenience macros handle the cast.

```c
typedef struct {
    wl_alloc_t base;    // embeds allocator vtable (must be first)
    u8*        buf;
    u64        cap;
    u64        pos;
} wl_arena;
```

**API shape:**

- `wl_arena_init(arena, capacity, backing)` — allocates the buffer via `backing` (or default if NULL)
- `wl_arena_alloc(arena, size)` — bump forward, return pointer (NULL on overflow)
- `wl_arena_save(arena)` / `wl_arena_restore(arena, savepoint)` — temp scope pattern: save position, do work, restore to free everything you just allocated
- `wl_arena_reset(arena)` — rewind to zero
- `wl_arena_destroy(arena)` — free the underlying buffer

Because the arena embeds `wl_alloc_t`, you can use it transparently with any function that takes an allocator:

```c
wl_arena arena;
wl_arena_init(&arena, WL_MB(1), NULL);

// pass &arena anywhere an allocator is expected
int *nums = NULL;
wl_arr_push(nums, 42, &arena);
wl_string s = wl_string_from("hello", &arena);
```

**Design note:** The arena does not grow. If you run out of space, `wl_arena_alloc` returns NULL. This is intentional — arenas are for bounded workloads where you know the upper bound. If you need unbounded growth, use the default allocator or chain arenas manually.

---

### 5. Dynamic Array

**Why:** The single most-reached-for data structure that C doesn't provide. This is a stretchy buffer in the style of stb: the array header (length, capacity, allocator pointer) is stored *behind* the user-facing pointer, so `arr[i]` works with normal indexing.

```c
int *nums = wl_arr_init(int, &arena);
wl_arr_push(nums, 42);
wl_arr_push(nums, 99);
wl_arr_push(nums, 7);
printf("%d %d %d\n", nums[0], nums[1], nums[2]);  // 42 99 7
printf("len=%zu\n", wl_arr_len(nums));             // 3
wl_arr_free(nums);
```

**API (all macros for type safety):**

- `wl_arr_init(T, alloc)` — allocate header + initial capacity, store allocator, return `T*`
- `wl_arr_push(arr, val)` — append, grow if needed using stored allocator
- `wl_arr_pop(arr)` — remove and return last element
- `wl_arr_len(arr)` / `wl_arr_cap(arr)` — query length and capacity
- `wl_arr_last(arr)` — peek at last element
- `wl_arr_clear(arr)` — set length to zero (keeps allocation)
- `wl_arr_free(arr)` — free using stored allocator, NULL the pointer

**Growth strategy:** double capacity each time, starting at 8 elements. The allocator is captured at init and stored in the header — every subsequent push, grow, and free uses it. No ambiguity, no mismatch possible.

**Why not a macro-generated typed container?** Stretchy buffers are simpler, work with any type via `sizeof(*(arr))`, and produce less code bloat. The tradeoff is that you lose some type safety on the header access — acceptable for a personal library.

---

### 6. String View (`wl_str`)

**Why:** Half the pain of C string handling comes from null terminators. A `wl_str` is just `{ const char* ptr; usize len; }`. It can point into the middle of another string, it never needs a null terminator, and comparing two views is `memcmp` with a length check.

**Core API:**

- `wl_str_lit("hello")` — compile-time literal to view (uses `sizeof - 1`)
- `wl_str_from_cstr(s)` — wraps a null-terminated C string
- `wl_str_eq(a, b)` — equality
- `wl_str_search(haystack, needle)` — Boyer-Moore-Horspool substring search, returns index or -1
- `wl_str_starts_with`, `wl_str_ends_with`

**Also defined here:** `wl_bytes` (`wl_slice(u8)`) and `wl_bytes_const` for raw byte views. The `wl_slice(T)` macro generates `struct { T* ptr; usize len; }` for any type.

---

### 7. Owned String (SSO)

**Why:** When you need to *build* a string, not just view one. The owned string uses small-string optimization: strings up to 23 bytes (on 64-bit) are stored inline with no heap allocation. Beyond that, it promotes to a heap buffer via the attached allocator.

**API shape:**

- `wl_string_new(alloc)` / `wl_string_from(cstr, alloc)` / `wl_string_from_str(view, alloc)`
- `wl_string_destroy(s)`
- `wl_string_len(s)`, `wl_string_cstr(s)`, `wl_string_as_str(s)` — query / convert to view
- `wl_string_append(s, wl_str)`, `wl_string_append_cstr(s, cstr)`, `wl_string_append_char(s, c)` — building
- `wl_string_clear(s)` — reset to empty (keeps heap allocation if already promoted)
- `wl_string_eq(a, b)`, `wl_string_cmp(a, b)` — comparison
- `wl_string_find(s, needle)`, `wl_string_starts_with`, `wl_string_ends_with` — searching (delegates to `wl_str` operations on the underlying view)

**SSO encoding:** The `sso.len` field stores `SSO_CAP - actual_length`. When `sso.len > SSO_CAP`, the string is on the heap. This means the SSO/heap discrimination is a single comparison, no flag bit needed.

**What's not here:** Split, join, replace, regex. Those can be written as free functions over `wl_str` if needed — keeping the core string type focused on storage and basic access.

---

### 8. UTF-8

**Why:** It's 2026. Strings are UTF-8. The `wl_str` and `wl_string` types already store UTF-8 correctly at the byte level — UTF-8 is just bytes, and all the byte-oriented operations (find, starts_with, append, eq) work on UTF-8 strings without modification because UTF-8 is self-synchronizing. What's missing is the ability to reason about codepoints when you need to.

**Design principle:** Byte length is the default. `wl_string_len` and `wl_str.len` always return bytes — that's what you need for allocation, slicing, and I/O. Codepoint-aware operations are explicitly named with `utf8` in the name so there's never ambiguity about which world you're in.

**API:**

- `wl_utf8_len(wl_str s)` → `usize` — codepoint count. Iterates lead bytes. This is O(n) and intentionally not cached — if you need it frequently, cache it yourself.
- `wl_utf8_next(wl_str s, usize *offset)` → `u32` — decode the codepoint at `*offset`, advance `*offset` past it. Returns the codepoint as a `u32`. This is the core iteration primitive:

```c
wl_str s = wl_str_lit("héllo 🌍");
usize off = 0;
while (off < s.len) {
    u32 cp = wl_utf8_next(s, &off);
    // do something with codepoint
}
```

- `wl_utf8_decode(wl_str s, usize offset)` → `u32` — decode codepoint at offset without advancing. Useful for peeking.
- `wl_utf8_encode(u8 *buf, u32 codepoint)` → `usize` — encode one codepoint into buf (must have room for 4 bytes), returns bytes written (1–4).
- `wl_utf8_valid(wl_str s)` → `bool` — full validation pass. Rejects overlong encodings, surrogates, and values above U+10FFFF.

**What's explicitly not here:** Case conversion, normalization (NFC/NFD), collation, grapheme cluster segmentation, bidirectional text. That's ICU-scale work and doesn't belong in a utility header. This module gives you enough to iterate codepoints, validate input, and encode/decode — the building blocks you need to hand off to a real Unicode library when the time comes.

---

### 9. Hash Function

**Why:** You need a good hash for the hashmap, and it's useful standalone (checksums, hash-based dedup, randomized algorithms). One function, well-tested, is better than scattering ad-hoc hashes across modules.

**Choice: xxHash3-inspired 64-bit.** Fast on modern CPUs, good distribution, handles short and long inputs well. Not cryptographic — that's a different problem.

**API:**

- `wl_hash_bytes(data, len, seed)` → `u64`
- `wl_hash_str(wl_str)` → `u64` (convenience wrapper, seed 0)
- `wl_hash_u64(val)` → `u64` (integer hash for keys that are already integers)

The implementation mixes 8-byte chunks with multiply-rotate, falls back to byte-at-a-time for the tail, and finalizes with a standard avalanche. Self-contained, no external dependency.

---

### 10. Hashmap

**Why:** The #2 most-reached-for missing data structure in C. You either write one per project or reach for a library. This one lives inside `wl.h` so it's always available.

**Design: Robin Hood open addressing.**

- Keeps probe distances low by stealing slots from "rich" entries (those close to their ideal position) and giving them to "poor" entries (those far from ideal)
- Backward-shift deletion instead of tombstones — no degradation over time
- Load factor threshold at 75%, doubles capacity on resize

**Slot layout:** Each slot is a flat byte array: `[u64 cached_hash][key bytes][value bytes]`. Hash 0 means empty. Keys and values are stored as opaque blobs — you pass `key_size` and `val_size` at init, plus function pointers for hashing and equality. Default hash/eq use `wl_hash_bytes` and `memcmp`.

**API:**

- `wl_hashmap_init(map, key_size, val_size, hash_fn, eq_fn, alloc)`
- `wl_hashmap_put(map, key_ptr, val_ptr)` — insert or update
- `wl_hashmap_get(map, key_ptr)` → `void*` to value, or NULL
- `wl_hashmap_remove(map, key_ptr)` → `bool`
- `wl_hashmap_clear(map)`, `wl_hashmap_count(map)`, `wl_hashmap_destroy(map)`
- Iterator: `wl_hashmap_iter_new(map)`, `wl_hashmap_iter_next(iter, &key, &val)`

**Why not a type-safe macro wrapper?** Could add one later as a thin layer. The untyped core keeps the implementation singular and testable. In practice, you cast on access — mildly annoying but not error-prone for a personal library.

---

### 11. Sort

**Why:** `qsort` exists but its comparator takes no context parameter. This means you can't sort by a field offset, a locale, or any external state without resorting to global variables or thread-local hacks. `wl_sort` fixes this.

```c
typedef int (*wl_cmp_fn)(const void* a, const void* b, void* ctx);
void wl_sort(void* base, usize count, usize elem_size, wl_cmp_fn cmp, void* ctx);
```

**Algorithm:** Start with insertion sort for small N (threshold ~32 elements). For larger arrays, introsort (quicksort that falls back to heapsort on bad partitions). The goal is never-worse-than-O(n log n) with good constant factors for typical inputs.

**Also included:**

- `wl_bsearch(base, count, elem_size, key, cmp, ctx)` — binary search with context parameter, returns index or -1
- `wl_str_search(haystack, needle)` — Boyer-Moore-Horspool for substring search (lives in the string view section but conceptually pairs with search)

---

### 12. Logging

**Why:** Every project needs logging. Everyone writes their own. This one is small and does the right things: compile-time level gating so disabled levels generate zero code, runtime level filtering, pluggable callback for custom backends (file, network, structured logging), and a sensible colorized-stderr default.

**Compile-time gating:**

```c
#define WL_LOG_MIN_LEVEL WL_LOG_WARN  // before including wl.h
```

Any call below `WARN` compiles to nothing — the check is in the macro, so the format string and arguments aren't even evaluated.

**Runtime filtering:** `wl_log_set_level(level)` for dynamic control. Messages below this level are discarded even if they pass the compile-time gate.

**Callback hook:** `wl_log_set_callback(fn)` replaces the default stderr output. The callback receives level, file, line, format string, and va_list — enough to integrate with any logging infrastructure.

**Log macros:**

```c
wl_log_trace("tick %d", frame);
wl_log_info("loaded %zu bytes from %s", len, path);
wl_log_error("failed to open %s: %s", path, wl_status_str(err));
wl_log_fatal("unrecoverable: %s", msg);  // calls abort() after logging
```

**Design note:** `wl_log_fatal` always aborts after logging. This is intentional — fatal means fatal. If you want recoverable errors, use a different level.

---

### 13. Assert

**Why:** Standard `assert()` gives you a message and aborts. That's fine until you need to hook into a crash reporter, break into a debugger, or run cleanup. `wl_assert` adds a pluggable handler and a message variant.

**Variants:**

- `wl_assert(expr)` — compiled out under `NDEBUG`
- `wl_assertm(expr, msg)` — same, with a message string
- `wl_assert_always(expr)` — never compiled out, even in release builds

**Handler hook:** `wl_assert_set_handler(fn)` lets you intercept assertion failures. The default handler prints to stderr and calls `abort()`. A custom handler could log to a file, trigger a breakpoint (`__builtin_trap`), or collect a stack trace before dying.

---

### 14. Time

**Why:** Getting the current time in C requires `clock_gettime` on POSIX and `QueryPerformanceCounter` on Windows, with different epoch handling, different resolution, and different APIs. It's not hard but it's tedious, and you do it in almost every project. Abstract it once.

**Types:**

```c
typedef struct { i64 ns; } wl_time;      // absolute point in time (nanoseconds)
typedef struct { i64 ns; } wl_duration;   // relative duration (nanoseconds)
```

Distinct types prevent accidentally mixing absolute and relative times — you can't pass a `wl_time` where a `wl_duration` is expected without an explicit conversion.

**API:**

- `wl_time_now()` — monotonic clock (for measuring intervals)
- `wl_time_wall()` — wall clock UTC (for timestamps)
- `wl_time_diff(a, b)` → `wl_duration`
- `wl_time_add(t, d)` → `wl_time`
- `wl_sleep(duration)`

**Duration constructors:**

```c
wl_ns(500)    // 500 nanoseconds
wl_us(100)    // 100 microseconds
wl_ms(16)     // 16 milliseconds
wl_sec(5)     // 5 seconds
```

**Implementation:** POSIX uses `clock_gettime(CLOCK_MONOTONIC)` and `clock_gettime(CLOCK_REALTIME)`. Windows uses `QueryPerformanceCounter` for monotonic and `GetSystemTimeAsFileTime` for wall clock, with epoch adjustment.

---

### 15. Random

**Why:** `rand()` is terrible — low-quality output, global state, non-reproducible. A PCG32 generator is tiny (two `u64` fields), fast, statistically excellent, and seedable for reproducibility.

**API:**

- `wl_rng_seed(u64)` → `wl_rng` — create a seeded generator
- `wl_rng_u32(rng)` / `wl_rng_u64(rng)` — raw random integers
- `wl_rng_f64(rng)` — uniform double in `[0.0, 1.0)`
- `wl_rng_range(rng, lo, hi)` — uniform integer in `[lo, hi)`
- `wl_rng_fill(rng, buf, len)` — fill a buffer with random bytes

**Not included:** Cryptographic randomness. If you need `/dev/urandom` or `BCryptGenRandom`, that's a different module with different guarantees. PCG32 is for simulations, shuffles, procedural generation, and testing.

---

O/O

**Why:** The "I just want the bytes" problem. Reading a file in C requires `open`, `fstat`, `read` loop, `close`, error handling, and buffer management. That's ~20 lines every time. Same for writing. Wrap it once.

**API:**

- `wl_fs_read_file(path, alloc, &buf, &len)` — reads entire file into allocator-provided buffer (NULL for default), null-terminates for convenience
- `wl_fs_write_file(path, data, len)` — writes a buffer to a file (create/truncate)
- `wl_fs_exists(path)` → `bool`
- `wl_fs_mkdir(path, recursive)` — create directory, optionally creating parents
- `wl_fs_remove(path)` — delete a file

**Path utilities (pure string operations, no I/O):**

- `wl_path_ext(path)` → `wl_str` view of extension (e.g. `.txt`)
- `wl_path_stem(path)` → `wl_str` view of filename without extension
- `wl_path_dir(path)` → `wl_str` view of directory portion
- `wl_path_join(buf, buf_size, a, b)` — join two path segments with `/`

**Scope:** This is deliberately minimal. No directory walking, no mmap, no permissions, no symlink resolution. Those are real features but they're also project-specific — add them when a project needs them, not speculatively.

---

## Section Ordering

Within the header, sections are ordered by dependency — each section only depends on things above it:

1. Config & platform detection
2. Core types
3. Status codes
4. Allocator vtable
5. Arena
6. Logging
7. Assert
8. Dynamic array
9. Slice / string view
10. Owned string
11. UTF-8
12. Hash function
13. Hashmap
14. Sort & search
15. Random
16. Time
17. File I/O

The implementation half (`#ifdef WL_IMPLEMENTATION`) follows the same order. Platform-specific code uses inline `#if WL_PLATFORM_POSIX` / `#elif WL_PLATFORM_WINDOWS` blocks rather than separate files.

---

## Build & Test Expectations

Compile with:

```
-std=c23 -Wall -Wextra -Werror -fsanitize=address,undefined
```

Test with a minimal test harness (or just `assert` + `main`). Each section should have a corresponding test that exercises the happy path, error paths, and edge cases (empty inputs, zero-size allocations, max-capacity hashmap, etc.).

The library should compile clean with both GCC and Clang on Linux and macOS. Windows support (MSVC) is secondary — stub with `#error` until it matters.
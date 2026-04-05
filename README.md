# Wade Lib 'wl'

This repo is the home for a growing set of single-header C libraries.

The center of gravity is `wl.h`: a small foundation library meant to remove the repeated scaffolding that shows up in every C project. Other headers can either build on top of it or live alongside it when they solve a more specific problem. `wasm.h` is an example of the second category: useful when you need it, otherwise irrelevant.

## Quick Start

The libraries follow the usual single-header pattern:

```c
/* Include wl.h wherever it's needed */
#include "wl.h"

/* Implement it in exactly one translation unit. */
#define WL_IMPL
#include "wl.h"
```

Platform specific APIs (time and file ops current) can be enabled with `WL_ENABLE_PLATFORM`:

```c
#define WL_ENABLE_PLATFORM 1
#define WL_IMPL
#include "wl.h"
```

`wasm.h` follows the same pattern:

```c
#define WASM_IMPL
#include "wasm.h"
```

# Included Libraries

## `wl.h`

`wl.h` is the shared utility layer intended to support future projects in this repo.

It currently includes:

- fixed-width type aliases and utility macros
- allocator plumbing and a bump arena
- logging and assertions
- stretchy arrays
- string views and owned strings
- UTF-8 helpers
- hashing and a hashmap
- sorting and binary search
- random number generation
- optional platform-backed APIs for time, sleep, and filesystem access
- a minimal built-in test framework

The portable core targets C99. Platform-backed APIs are enabled explicitly with `WL_ENABLE_PLATFORM=1`.

Allocator API Example:

```c
wl_arena arena;
wl_string msg;
int *values = NULL;

if (wl_arena_init(&arena, WL_KB(4), NULL) != WL_OK) {
	return 1;
}

msg = wl_string_from("values:", &arena);
if (wl_string_append_cstr(&msg, " ready") != WL_OK) {
	wl_arena_destroy(&arena);
	return 1;
}

values = wl_arr_init(int, &arena);
if (values == NULL) {
	wl_arena_destroy(&arena);
	return 1;
}

if (wl_arr_push(values, 10) != WL_OK || wl_arr_push(values, 20) != WL_OK) {
	wl_arena_destroy(&arena);
	return 1;
}

wl_log_info("%s len=%zu last=%d", wl_string_cstr(&msg), wl_arr_len(values), wl_arr_last(values));

wl_arena_destroy(&arena);
```

That example is a little random, but it shows the main thing `wl.h` is good at: using one allocator interface across multiple utilities without having to change how you call them.

## `wasm.h`

`wasm.h` is a standalone single-header WebAssembly runtime and interpreter.

At a high level it gives you:

- loading and validating Wasm binary modules from memory
- calling exported functions by name from C
- registering C callbacks and globals as Wasm imports
- linear memory support, including multiple memories and indexed `memory.grow`
- tables and indirect calls
- trap and error reporting through `wasm_error_t` and `wasm_error_string`

The current scope is the Wasm MVP execution model plus a focused subset of newer proposals. In practice that means the core numeric types, imports/exports, control flow, globals, multi-memory linear memory, tables, bulk memory, reference types, GC, multi-value, sign-extension, non-trapping truncation, mutable globals, extended const expressions, tail calls, exceptions, and SIMD are implemented, while threads remain out of scope.

Internally, the runtime is a straightforward stack-based interpreter. `wasm_runtime_t` holds imports, execution stack state, and error state; `wasm_module_t` owns decoded types, functions, exports, globals, table state, and memory for one loaded module.

Relevant configuration points:

- `WASM_MAX_STACK` defaults to `4096`
- `WASM_MAX_CALL_DEPTH` defaults to `512`
- `WASM_GC_HEAP_SIZE` defaults to `4 * 1024 * 1024`
- `WASM_GC_ALLOC(sz)` / `WASM_GC_FREE(p)` let you override GC arena allocation separately from the general allocator hooks

The minimal usage flow is:

```c
wasm_runtime_t rt;
wasm_module_t *mod;
wasm_value_t args[1];
wasm_value_t result;

wasm_init(&rt);

/* Optional: register host imports before loading. */
/* wasm_register_import(&rt, &imp); */
/* wasm_register_global_import(&rt, &global_imp); */

mod = wasm_load(&rt, bytes, len);
if (mod != NULL) {
	args[0] = wasm_i32(42);
	if (wasm_call(mod, "main", args, 1, &result, 1) == WASM_OK) {
		/* use result */
	}
	wasm_gc_collect(&rt);
	wasm_free_module(mod);
}

wasm_destroy(&rt);
```


For GC-aware tooling you can introspect composite types with `wasm_type_count`, `wasm_type_kind`, `wasm_struct_type_field`, and `wasm_array_type_field`. Host code can also allocate managed values directly with `wasm_struct_new` and `wasm_array_new` when it needs to seed globals, tables, or other runtime state with GC objects.

## Build And Test

This repo includes test programs for both headers.

```sh
make
```

That builds and runs:

- `wl_test` for `wl.h`
- `wasm_test` for `wasm.h`

The current `Makefile` enables `WL_ENABLE_PLATFORM=1` for the repo test build.

There is also an `emcc`-driven harness under [wasm/README.md](wasm/README.md) for compiling fixture C files into real `.wasm` blobs and running them through `wasm.h`.

Useful targets:

- `make wasm-emcc-build` — build the native harness runner and compile the fixture `.wasm` files
- `make wasm-emcc-run` — build and run the `emcc` fixture harness, but do not fail the target if cases fail inside `wasm.h`
- `make wasm-emcc-run-strict` — same harness, but fail if any fixture fails to load, execute, or match its expected result

## Repository Layout

- `wl.h` — foundation library for future projects in this repo
- `wl_test.c` — test suite for `wl.h`
- `wasm.h` — standalone WebAssembly runtime header
- `wasm_test.c` — test suite for `wasm.h`
- `wasm/` — `emcc` fixture harness for testing `wasm.h` against toolchain-produced modules
- `PLAN.md` — design notes and longer-form planning for `wl.h`


## Goals

- keep libraries drop-in simple
- stay close to portable C99 for the shared core
- make platform-dependent features explicit rather than implicit
- keep dependencies near zero
- use `wl.h` as the base layer for future single-header work where it makes sense

## Notes

`wl.h` already has a split between portable code and platform-backed functionality. The portable core builds without POSIX when platform support is disabled, and the platform layer currently has both POSIX and Windows support paths.

This repo is intentionally small and direct. The point is not to build a framework ecosystem; it is to keep a set of sharp, reusable single-header libraries that are pleasant to drop into real projects.
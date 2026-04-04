# wl

This repo is the home for a growing set of single-header C libraries.

The center of gravity is `wl.h`: a small foundation library meant to remove the repeated scaffolding that shows up in every C project. Other headers can either build on top of it or live alongside it when they solve a more specific problem. `wasm.h` is an example of the second category: useful when you need it, otherwise irrelevant.

## Included Libraries

### `wl.h`

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

### `wasm.h`

`wasm.h` is a standalone single-header WebAssembly runtime and interpreter. It is independent from `wl.h` and is meant to be pulled into projects only when it is actually relevant.

At a high level it gives you:

- loading and validating Wasm binary modules from memory
- calling exported functions by name from C
- registering C callbacks as Wasm imports
- linear memory support, including `memory.grow`
- tables and indirect calls
- trap and error reporting through `wasm_error_t` and `wasm_error_string`

The current scope is the Wasm MVP execution model rather than newer proposal work. In practice that means the core numeric types, imports/exports, control flow, globals, linear memory, and tables are implemented, while SIMD, threads, GC, multiple memories, and tail calls are intentionally out of scope.

Internally, the runtime is a straightforward stack-based interpreter. `wasm_runtime_t` holds imports, execution stack state, and error state; `wasm_module_t` owns decoded types, functions, exports, globals, table state, and memory for one loaded module.

Relevant configuration points:

- `WASM_MAX_STACK` defaults to `4096`
- `WASM_MAX_CALL_DEPTH` defaults to `512`

The minimal usage flow is:

```c
wasm_runtime_t rt;
wasm_module_t *mod;
wasm_value_t args[1];
wasm_value_t result;

wasm_init(&rt);

/* Optional: register host imports before loading. */
/* wasm_register_import(&rt, &imp); */

mod = wasm_load(&rt, bytes, len);
if (mod != NULL) {
	args[0] = wasm_i32(42);
	if (wasm_call(mod, "main", args, 1, &result, 1) == WASM_OK) {
		/* use result */
	}
	wasm_free_module(mod);
}

wasm_destroy(&rt);
```

## Single-Header Usage

The libraries follow the usual single-header pattern:

```c
#include "wl.h"

/* In exactly one translation unit. */
#define WL_IMPL
#include "wl.h"
```

If you want the platform-backed APIs from `wl.h`, define `WL_ENABLE_PLATFORM=1` before including it in any translation unit that uses those APIs:

```c
#define WL_ENABLE_PLATFORM 1
#define WL_IMPL
#include "wl.h"
```

For `wasm.h`:

```c
#define WASM_IMPL
#include "wasm.h"
```

## Build And Test

This repo includes test programs for both headers.

```sh
make
```

That builds and runs:

- `wl_test` for `wl.h`
- `wasm_test` for `wasm.h`

The current `Makefile` enables `WL_ENABLE_PLATFORM=1` for the repo test build.

## Repository Layout

- `wl.h` — foundation library for future projects in this repo
- `wl_test.c` — test suite for `wl.h`
- `wasm.h` — standalone WebAssembly runtime header
- `wasm_test.c` — test suite for `wasm.h`
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
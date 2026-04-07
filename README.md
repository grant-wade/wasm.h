# wasm.h

`wasm.h` is a single-header WebAssembly runtime for C.

The runtime loads Wasm binaries from memory, validates them, wires host imports, executes exported functions, and exposes a compact C API for embedding Wasm in native programs.

`wl.h` is used as support code for the local test framework and related development utilities.

## What `wasm.h` provides

At a high level, `wasm.h` gives you:

- loading and validating `.wasm` modules from memory
- calling exported Wasm functions by name from C
- registering host functions and imported globals
- linear memory support, including multiple memories and `memory.grow`
- tables and indirect calls
- trap and error reporting through `wasm_error_t`, `wasm_error_string`, and backtrace helpers
- GC-aware host interop helpers for struct and array references
- optional built-in WASI stubs for common `wasi_snapshot_preview1` imports

The runtime covers the Wasm MVP plus a focused set of newer proposals already implemented in the interpreter. In practice that includes imports/exports, globals, tables, multi-memory, bulk memory, reference types, GC, multi-value, sign-extension, non-trapping float-to-int conversions, mutable globals, extended const expressions, tail calls, exceptions, SIMD (currently lacks platform intrinsics for actual speed), and the current validation/runtime configuration surface in `wasm.h`.


## Quick start

Use `wasm.h` like a normal single-header C library:

```c
#define WASM_IMPL
#include "wasm.h"
```

Then initialize a runtime, load a module, and call an export:

```c
#include <stdio.h>

#define WASM_IMPL
#include "wasm.h"

int main(void) {
	wasm_runtime_t rt;
	wasm_module_t *mod;
	int32_t result;

	if (wasm_init(&rt, NULL) != WASM_OK) {
		return 1;
	}

	/* Register imports here if the module needs them. */
	/* wasm_register_import(&rt, &import); */
	/* wasm_register_global_import(&rt, &global_import); */

	mod = wasm_load(&rt, wasm_bytes, wasm_len);
	if (mod == NULL) {
		wasm_destroy(&rt);
		return 1;
	}

	if (wasm_call_fmt(mod, "main", "i(i)", (int32_t)42, &result) != WASM_OK) {
		wasm_free_module(mod);
		wasm_destroy(&rt);
		return 1;
	}

	/* use result */
    printf("result: %d\n", result);

	wasm_free_module(mod);
	wasm_destroy(&rt);
	return 0;
}
```

The format string uses `args(results)` syntax, so `"i(i)"` means one `i32` argument and one `i32` result.

Useful public APIs beyond the basic load-and-call path include:

- `wasm_enable_feature()` / `wasm_disable_feature()`
- `wasm_call_fmt()` and `wasm_bind_host_func()`
- `wasm_memory_read()` / `wasm_memory_write()`
- `wasm_global_get()` / `wasm_global_set()`
- `wasm_dump_backtrace()` and call-stack inspection helpers
- `wasm_bind_wasi_stubs()`

## Companion tools

### `wasm2api`

`wasm2api` generates a small C wrapper pair from a Wasm module's exported functions:

```sh
cmake --build build --target wasm2api
./build/wasm2api path/to/module.wasm my_module
```

If the module imports host functions or globals, the generated header includes a `<prefix>_imports_t` struct so the host can provide those bindings before initialization.

To embed the module bytes directly into the generated `.c` file:

```sh
./build/wasm2api --embed path/to/module.wasm my_module
```

That mode emits embedded-byte helpers so callers can initialize the wrapper without loading the Wasm file separately at runtime.

### `wasm.c`

`wasm.c` is a small command-line runner and inspection tool built on top of `wasm.h`.

Build it with:

```sh
cmake -S . -B build
cmake --build build --target wasm
```

Then inspect or invoke a module directly:

```sh
./build/wasm path/to/module.wasm info
./build/wasm path/to/module.wasm exports
./build/wasm path/to/module.wasm funcs
./build/wasm path/to/module.wasm call add 2 40
./build/wasm path/to/module.wasm memory-read 0 0 64
```

By default the CLI binds the built-in WASI stub set so common `wasi_snapshot_preview1` imports resolve during inspection and calls. Use `--no-wasi` if you want to load a module without that binding layer.

## Build and test

Configure and build the project with CMake:

```sh
cmake -S . -B build
cmake --build build --target check
```

That builds and runs the main native test targets:

- `wasm_test` for `wasm.h`
- `wl_test` for the local `wl.h` support/test layer

If you want individual targets instead of the aggregate `check` target, the main ones are:

- `wasm`
- `wasm2api`
- `wasm_test`
- `wl_test`
- `basic_add_demo` in `examples/`

There is also an `emcc`-driven harness under `test/` that compiles fixture C files into real `.wasm` binaries and runs them through `wasm.h`.

Useful targets:

- `cmake --build build --target wasm-emcc-build`
- `cmake --build build --target wasm-emcc-run`
- `cmake --build build --target wasm-emcc-run-strict`

Spectest support requires a system `wasm-tools` executable with both the `json-from-wast` and `parse` subcommands available.

## Repository layout

- `wasm.h` - the single-header WebAssembly runtime
- `wasm.c` - CLI runner and inspection tool built on top of `wasm.h`
- `wasm2api.c` - code generator for typed C wrappers around Wasm exports
- `wasm_test.c` - native runtime regression tests
- `examples/` - native demos and generated wrapper examples
- `test/` - `emcc` fixture harness for real Wasm modules
- `wl.h` - local support library used by tests and development utilities
- `wl_test.c` - tests for the local `wl.h` support layer
- `docs/` - planning notes and implementation docs for the runtime and tooling

## Finished proposal status

This table tracks the current finished WebAssembly proposals against the actual `wasm.h` runtime surface. The status here is specifically about this C runtime, not the JS API or text-format tooling.

| Proposal | Status | Notes |
| --- | --- | --- |
| MVP | Implemented | Core load, validate, instantiate, and execute support. |
| Import/Export of Mutable Globals | Implemented | Includes mutable globals and imported global support. |
| Non-trapping float-to-int conversions | Implemented | Supported by the runtime feature gate and execution engine. |
| Sign-extension operators | Implemented | Supported by validation and execution. |
| Multi-value | Implemented | Multi-result validation and execution are supported. |
| JavaScript BigInt to WebAssembly i64 integration | Out of scope | JS-API proposal; not relevant to the native C embedding API. |
| Reference Types | Implemented | Includes `funcref`, `externref`, tables, and indirect-call support. |
| Bulk memory operations | Implemented | Includes bulk memory and table segment operations. |
| Fixed-width SIMD | Implemented | Includes `v128` values plus validated SIMD execution support. |
| Tail call | Implemented | Includes `return_call` and `return_call_indirect`. |
| Extended Constant Expressions | Implemented | Evaluated and validated at load time. |
| Typed Function References | Partial | Related typed-reference machinery exists, but the full finished proposal is not implemented as a complete feature set. |
| Garbage collection | Implemented | Includes GC types, heap objects, and host interop APIs. |
| Multiple memories | Implemented | Includes indexed memory instructions and public multi-memory helpers. |
| Relaxed SIMD | Not implemented | Fixed-width SIMD is implemented; relaxed SIMD is not. |
| Custom Annotation Syntax in the Text Format | Out of scope | `wasm.h` is a binary runtime, not a WAT parser. |
| Branch Hinting | Not implemented | No branch-hint parsing or execution support today. |
| Exception handling | Implemented | Includes tags, `try`, `catch`, `catch_all`, `rethrow`, and `delegate`. |
| JS String Builtins | Out of scope | JS-host proposal; there is no dedicated support in this native runtime. |
| Memory64 | Implemented | Includes i64 memories, width-aware validation/execution, 64-bit memory helper APIs, and focused spectest/unit coverage; full spec-harness coverage is still in progress. |

## Notes on `wl.h`

`wl.h` is included here for internal support code, especially the test framework and related utilities.
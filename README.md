# wasm.h

`wasm.h` is a single-header WebAssembly runtime for C99.

It loads Wasm binaries from memory, validates them, instantiates modules, binds host imports, executes exports, and exposes a compact embedding API with no external runtime dependency. The repository also includes a CLI runner, a wrapper generator, a local spectest harness, and an `emcc`-based fixture suite.

## Current status

The checked-in local test harness is currently green.

On a machine with `wasm-tools` and `emcc` available, the repository's `check` target passes all locally configured tests:

- `wl_test`
- `wasm_test`
- the full spectest harness configured by `test/CMakeLists.txt`
- the `emcc` fixture suite

In the latest local run on this repository state, that was `272/272` passing tests.

## What this runtime provides

At a high level, `wasm.h` includes:

- load-time validation and feature gating
- execution of exported Wasm functions from C
- host function and imported-global binding
- linear memories, including multiple memories and `memory64`
- tables, indirect calls, `call_ref`, and typed reference flows
- bulk memory and element/data segment operations
- exceptions, tail calls, and extended constant expressions
- fixed-width SIMD plus relaxed SIMD execution, with ARM NEON and x86_64 SSE2 fast paths where the platform instructions preserve Wasm semantics exactly
- GC-aware runtime support for structs, arrays, `i31ref`, casts, and heap subtyping
- trap reporting, fuel metering, and backtrace helpers
- optional built-in WASI stubs for common `wasi_snapshot_preview1` imports


## Quick start

Use `wasm.h` like a normal single-header library:

```c
#define WASM_IMPL
#include "wasm.h"
```

Minimal embedding example:

```c
#include <stdio.h>

#define WASM_IMPL
#include "wasm.h"

int main(void) {
	wasm_runtime_t rt;
	wasm_module_t* mod;
	int32_t result = 0;

	if (wasm_init(&rt, NULL) != WASM_OK) return 1;

	mod = wasm_load(&rt, wasm_bytes, wasm_len);
	if (!mod) {
		fprintf(stderr, "load failed: %s\n", rt.error_msg[0] ? rt.error_msg : wasm_error_string(rt.last_error));
		wasm_destroy(&rt);
		return 1;
	}

	if (wasm_call_fmt(mod, "main", "i(i)", (int32_t)42, &result) != WASM_OK) {
		fprintf(stderr, "call failed: %s\n", rt.error_msg[0] ? rt.error_msg : wasm_error_string(rt.last_error));
		wasm_free_module(mod);
		wasm_destroy(&rt);
		return 1;
	}

	printf("result: %d\n", result);

	wasm_free_module(mod);
	wasm_destroy(&rt);
	return 0;
}
```

The format string for `wasm_call_fmt()` uses `args(results)` syntax, so `"i(i)"` means one `i32` argument and one `i32` result.

Useful public APIs beyond the basic load-and-call path include:

- `wasm_enable_feature()`, `wasm_disable_feature()`, `wasm_enable_all_features()`
- `wasm_call()`, `wasm_call_index()`, `wasm_call_fmt()`
- `wasm_bind_host_func()` and the lower-level import registration APIs
- `wasm_memory_read()`, `wasm_memory_write()`, and the indexed memory helpers
- `wasm_global_get()` and `wasm_global_set()`
- `wasm_set_fuel()` and `wasm_get_fuel()`
- `wasm_dump_backtrace()` and call-stack inspection helpers
- `wasm_bind_wasi_stubs()`

## Build and test

Configure and build with CMake:

```sh
cmake -S . -B build
cmake --build build
```

Run the full repository test pass:

```sh
cmake --build build --target check
```

Main native targets:

- `wasm`
- `wasm2api`
- `wasm_test`
- `wl_test`
- `basic_add_demo`

Spectest targets are enabled only when a system `wasm-tools` executable is available with both `json-from-wast` and `parse` subcommands.

Useful spectest targets:

- `cmake --build build --target wasm-spectest-build`
- `cmake --build build --target wasm-spectest-run`
- `cmake --build build --target wasm-spectest-run-strict`

The spectest harness builds per-file JSON command streams under `build/test/spec/` and runs them through `test/spectest_runner.c`.

The repository also includes an `emcc`-driven fixture suite that compiles real C inputs into Wasm and executes them through `wasm.h`.

Useful `emcc` targets:

- `cmake --build build --target wasm-emcc-build`
- `cmake --build build --target wasm-emcc-run`
- `cmake --build build --target wasm-emcc-run-strict`
- `cmake --build build --target wasm-emcc-list`

## CLI runner

`wasm.c` builds a small command-line tool for inspection and execution.

Build it with:

```sh
cmake --build build --target wasm
```

Usage:

```sh
./build/wasm [--no-wasi] [--fuel N] <module.wasm> <command> [args...]
```

Supported commands:

- `info`
- `exports`
- `funcs`
- `types`
- `globals`
- `memories`
- `custom-sections`
- `call <export_name> [args...]`
- `call-index <func_index> [args...]`
- `global-get <export_name>`
- `global-set <export_name> <value>`
- `memory-read <memory_index> <offset> <length>`
- `memory-string <memory_index> <offset> [max_len]`

Examples:

```sh
./build/wasm hello.wasm info
./build/wasm hello.wasm exports
./build/wasm math.wasm call add 2 40
./build/wasm memory.wasm memory-read 0 0 64
```

By default the CLI binds the built-in WASI stub layer. Use `--no-wasi` to disable that.

## `wasm2api`

`wasm2api.c` generates small typed C wrappers around Wasm exports and imports.

Build it with:

```sh
cmake --build build --target wasm2api
```

Generate wrapper files:

```sh
./build/wasm2api path/to/module.wasm my_module
./build/wasm2api --embed path/to/module.wasm my_module
```

What it does today:

- emits `<prefix>.h` and `<prefix>.c`
- caches export indices and provides lifecycle helpers
- emits an imports struct when the module imports host functions or globals
- supports generated wrappers for `i32`, `i64`, `f32`, `f64`, and `externref`
- skips unsupported signatures with diagnostics instead of generating incorrect code

`--embed` includes the module bytes directly in the generated `.c` file so the wrapper can initialize without loading the Wasm from disk at runtime.

## Repository layout

- `wasm.h` — single-header runtime, validator, interpreter, and public C API
- `wasm.c` — CLI runner and inspection tool built on top of `wasm.h`
- `wasm2api.c` — typed wrapper generator for Wasm exports and imports
- `wasm_test.c` — native regression coverage for the runtime
- `wl.h` — local support library used by tests and development utilities
- `wl_test.c` — tests for `wl.h`
- `examples/` — native examples and demo code
- `test/` — spectest harness, `emcc` fixtures, and Wasm fixture runner
- `docs/` — design notes, plans, and historical compliance snapshots

## Proposal support

This table is about the runtime in this repository, not the JS API or text-format tooling.

| Proposal | Status | Notes |
| --- | --- | --- |
| MVP | Implemented | Core validation, instantiation, execution, memory, tables, imports, and exports. |
| Import/Export of Mutable Globals | Implemented | Includes imported globals by reference and mutable global support. |
| Non-trapping float-to-int conversions | Implemented | Covered by validation and execution. |
| Sign-extension operators | Implemented | Supported in validation and execution. |
| Multi-value | Implemented | Includes multi-result validation and runtime support. |
| Reference Types | Implemented | Includes `funcref`, `externref`, tables, and typed reference validation/execution. |
| Bulk Memory Operations | Implemented | Includes passive segments, `memory.init`, `data.drop`, `table.init`, `table.copy`, and related ops. |
| Fixed-width SIMD | Implemented | Includes `v128`, full fixed-width SIMD execution, and platform intrinsics where safe. |
| Relaxed SIMD | Implemented | Covered by the current local spectest harness. |
| Tail Calls | Implemented | Includes `return_call`, `return_call_indirect`, and `return_call_ref`. |
| Extended Constant Expressions | Implemented | Evaluated and validated at load time. |
| Typed Function References | Implemented | Includes `call_ref` and typed function-reference flows exercised by the local testsuite. |
| Garbage Collection | Implemented | Includes recursive types, structs, arrays, `i31ref`, casts, and host GC helpers. |
| Multiple Memories | Implemented | Includes indexed memory ops and public helpers. |
| Exception Handling | Implemented | Includes tags, `try`, `catch`, `catch_all`, `rethrow`, `throw_ref`, and `delegate`. |
| Memory64 | Implemented | Includes 64-bit memories, validation, execution, and test coverage. |
| JavaScript BigInt to WebAssembly i64 integration | Out of scope | JS embedding proposal, not relevant to this native C runtime. |
| JS String Builtins | Out of scope | JS host proposal, not part of this runtime surface. |
| Custom Annotation Syntax in the Text Format | Out of scope | `wasm.h` is a binary runtime, not a WAT parser. |
| Branch Hinting | Not implemented | No branch-hint parsing or execution support is present. |

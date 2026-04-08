# wasm.h

A single-header WebAssembly runtime for C99. No external runtime dependency.

Drop `wasm.h` into a project, define `WASM_IMPL` in one translation unit, and call exported Wasm functions from C. The runtime loads, validates, and interprets standard `.wasm` binaries, with support for most finalized post-MVP proposals.

## Quick start

```c
#define WASM_IMPL
#include "wasm.h"
```

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
		fprintf(stderr, "load: %s\n", rt.error_msg[0] ? rt.error_msg : wasm_error_string(rt.last_error));
		wasm_destroy(&rt);
		return 1;
	}

	if (wasm_call_fmt(mod, "main", "i(i)", (int32_t)42, &result) != WASM_OK) {
		fprintf(stderr, "call: %s\n", rt.error_msg[0] ? rt.error_msg : wasm_error_string(rt.last_error));
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

`wasm_call_fmt` uses `args(results)` syntax: `"i(i)"` means one `i32` argument and one `i32` result.

Format specifiers: `i` i32, `I` i64, `f` f32, `F` f64, `r` externref, `v` void result.

## Embedding API

### Lifecycle

| Function | Purpose |
| --- | --- |
| `wasm_init(rt, config)` | Initialize a runtime. Pass `NULL` for defaults. |
| `wasm_destroy(rt)` | Free all runtime resources. |
| `wasm_load(rt, bytes, len)` | Load, validate, and instantiate a module. |
| `wasm_free_module(mod)` | Free a loaded module. |

### Calling Wasm

| Function | Purpose |
| --- | --- |
| `wasm_call(mod, name, args, nargs, results, nresults)` | Call an export by name with value arrays. |
| `wasm_call_index(mod, idx, args, nargs, results, nresults)` | Call a function by index. |
| `wasm_call_fmt(mod, name, fmt, ...)` | Call an export using a format string and varargs. |
| `wasm_find_export(mod, name, kind, index)` | Look up an export by name. |

### Host imports

| Function | Purpose |
| --- | --- |
| `wasm_bind_host_func(rt, module, name, fmt, cb, userdata)` | Bind a host function import using a format string. |
| `wasm_bind_wasi_stubs(rt)` | Register built-in `wasi_snapshot_preview1` stubs. |

### Memory and globals

| Function | Purpose |
| --- | --- |
| `wasm_memory_read(mod, mem_idx, offset, dst, len)` | Read bytes from linear memory. |
| `wasm_memory_write(mod, mem_idx, offset, src, len)` | Write bytes to linear memory. |
| `wasm_global_get(mod, name, out)` | Read an exported global. |
| `wasm_global_set(mod, name, val)` | Write an exported mutable global. |

### Features and diagnostics

| Function | Purpose |
| --- | --- |
| `wasm_enable_feature(rt, flag)` | Enable a proposal feature flag. |
| `wasm_disable_feature(rt, flag)` | Disable a proposal feature flag. |
| `wasm_enable_all_features(rt)` | Enable all implemented proposals. |
| `wasm_set_fuel(mod, n)` | Set an instruction fuel limit. |
| `wasm_get_fuel(mod)` | Read remaining fuel. |
| `wasm_dump_backtrace(mod)` | Print a call-stack backtrace. |
| `wasm_error_string(err)` | Human-readable name for an error code. |

### Configuration

Pass a `wasm_config_t` to `wasm_init` to override defaults:

```c
wasm_config_t cfg = {
	.max_stack_values  = 65536,
	.max_call_depth    = 1024,
	.max_labels        = 4096,
	.initial_gc_heap_size = 1 << 20,
	.frame_arena_size  = 1 << 16,
	.lazy_validation   = 1,
};
wasm_init(&rt, &cfg);
```

Set `lazy_validation` to defer per-function body validation until first execution. The default is `0`, which preserves eager whole-module validation.

### Custom allocators

Define before including `wasm.h`:

```c
#define WASM_MALLOC(sz)      my_malloc(sz)
#define WASM_REALLOC(p, sz)  my_realloc(p, sz)
#define WASM_FREE(p)         my_free(p)
#define WASM_CALLOC(n, sz)   my_calloc(n, sz)
```

### WASI stubs

`wasm_bind_wasi_stubs(rt)` registers minimal stubs for these `wasi_snapshot_preview1` imports: `fd_write`, `fd_close`, `fd_seek`, `fd_fdstat_get`, `args_get`, `args_sizes_get`, `environ_get`, `environ_sizes_get`, `random_get`, `clock_time_get`, and `proc_exit`.

## Build and test

```sh
cmake -S . -B build
cmake --build build --target check
```

`check` runs the full test pass: unit tests (`wasm_test`, `wl_test`), the spectest harness, and the `emcc` fixture suite.

- `wasm`
- `wasm2api`
- `wasm_test`
- `wl_test`
- `session_math_demo` (when `emcc` is available)

Additional targets:

```sh
cmake --build build --target wasm               # CLI runner
cmake --build build --target wasm2api            # Wrapper generator
cmake --build build --target wasm-spectest-run   # Spectest only
cmake --build build --target wasm-emcc-run       # Emcc fixtures only
```

## CLI

`wasm.c` builds a command-line tool for inspection and execution:

```
wasm [--help] [--no-wasi] [--fuel N] <module.wasm> <command> [args...]
```

| Command | Description |
| --- | --- |
| `info` | Module summary. |
| `exports` | List exports. |
| `funcs` | List functions with signatures. |
| `types` | List type section entries. |
| `globals` | List globals. |
| `memories` | List memories. |
| `custom-sections` | List custom section names and sizes. |
| `call <name> [args...]` | Call an export by name. |
| `call-index <idx> [args...]` | Call a function by index. |
| `global-get <name>` | Read an exported global. |
| `global-set <name> <value>` | Write an exported mutable global. |
| `memory-read <idx> <offset> <len>` | Hex-dump memory. |
| `memory-string <idx> <offset> [max]` | Print a null-terminated string from memory. |

```sh
./build/wasm hello.wasm info
./build/wasm math.wasm call add 2 40
./build/wasm memory.wasm memory-read 0 0 64
./build/wasm --no-wasi bare.wasm call _start
```

WASI stubs are bound by default; `--no-wasi` disables them.

## wasm2api

`wasm2api.c` generates typed C wrappers around a module's exports and imports:

```sh
./build/wasm2api module.wasm my_module          # generates my_module.h + my_module.c
./build/wasm2api --embed module.wasm my_module  # embeds the .wasm bytes in the source
```

Generate wrapper files:

```sh
./build/wasm2api path/to/module.wasm my_module
./build/wasm2api --embed --init-func init_state path/to/module.wasm my_module
./build/wasm2api --singleton --embed path/to/module.wasm my_module
./build/wasm2api --all-exports path/to/module.wasm my_module
./build/wasm2api --exclude-prefix internal_ --exclude-export debug_dump path/to/module.wasm my_module
```

What it does today:

- emits `<prefix>.h` and `<prefix>.c`
- caches export indices and provides lifecycle helpers
- owns a runtime by default so the common case is just `*_init(...)` or `*_init_embedded(NULL)` and then direct wrapper calls
- exposes an `*_init_options_t` struct for advanced callers that want to supply a runtime, runtime config, or import bindings
- can emit a singleton-oriented API with `--singleton`, which hides one generated context instance in the `.c` file and removes the `ctx` parameter from wrapper calls
- if the module exports a memory, also emits `*_get_memory_ptr`, `*_get_memory_size`, `*_read_memory_string`, `*_read_memory`, and `*_write_memory` helpers against that exported memory, preferring an export literally named `memory` when several memories are exported
- emits an imports struct when the module imports host functions or globals
- filters common toolchain-noise exports by default, including Emscripten stack helpers and LLVM-prefixed internals
- supports generated wrappers for `i32`, `i64`, `f32`, `f64`, and `externref`
- skips unsupported signatures with diagnostics instead of generating incorrect code

`--embed` includes the module bytes directly in the generated `.c` file so the wrapper can initialize without loading the Wasm from disk at runtime.

`--singleton` generates a single-instance API for cases where only one module instance should exist in-process. In that mode, the generated export wrappers, `*_free`, `*_get_module`, `*_get_runtime`, and `*_get_last_error*` helpers no longer take a context pointer. `*_init(...)` and `*_init_embedded(...)` return `wasm_error_t` instead of a context pointer and manage one hidden static context inside the generated source.

`--init-func <export>` marks a `void(void)` Wasm export that should be invoked automatically after the wrapper loads the module. This is useful for libraries that need an explicit Wasm-side startup step before other exports are called.

By default, `wasm2api` excludes exports that look like toolchain internals rather than library API surface, such as names beginning with `emscripten_`, `_emscripten_`, `__emscripten_`, `llvm.`, `llvm_`, `__llvm_`, plus `__wasm_call_ctors`.

`--all-exports` disables those built-in exclusions.

You can layer your own rules on top with:

- `--include-export <name>`
- `--include-prefix <prefix>`
- `--exclude-export <name>`
- `--exclude-prefix <prefix>`

User rules override the built-in defaults, so you can re-include a filtered prefix selectively without turning everything back on.

Checked-in example flow:

- `examples/session_math_wasm.c` is a small stateful Wasm library source meant to be compiled with `emcc`
- `cmake --build build --target session_math_wasm` builds the `.wasm` module from that checked-in C source
- `cmake --build build --target session_math_generate` runs `wasm2api --singleton --embed --init-func init_state` against the built module and refreshes `examples/session_math.h` plus `examples/session_math.c`
- `cmake --build build --target session_math_demo` builds and runs a native demo against the generated wrapper API

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

| Proposal | Status |
| --- | --- |
| MVP | Implemented |
| Import/Export of Mutable Globals | Implemented |
| Non-trapping float-to-int conversions | Implemented |
| Sign-extension operators | Implemented |
| Multi-value | Implemented |
| Reference Types | Implemented |
| Typed Function References | Implemented |
| Bulk Memory Operations | Implemented |
| Multiple Memories | Implemented |
| Fixed-width SIMD | Implemented |
| Relaxed SIMD | Implemented |
| Tail Calls | Implemented |
| Extended Constant Expressions | Implemented |
| Garbage Collection | Implemented |
| Exception Handling | Implemented |
| Memory64 | Implemented |
| Branch Hinting | Not implemented |

SIMD execution includes ARM NEON and x86_64 SSE2 fast paths where the platform instructions preserve Wasm semantics exactly.

GC support covers recursive types, structs, arrays, `i31ref`, casts, and heap subtyping.

Exception handling covers both the legacy (`try`/`catch`/`delegate`) and current (`try_table`/`throw_ref`) instruction sets.

JS-only proposals (BigInt-to-i64, JS String Builtins) and text-format proposals (Custom Annotation Syntax) are out of scope for a binary C runtime.

## Repository layout

| Path | Description |
| --- | --- |
| `wasm.h` | Single-header runtime, validator, interpreter, and public API. |
| `wasm.c` | CLI runner. |
| `wasm2api.c` | Typed wrapper generator. |
| `wasm_test.c` | Runtime regression tests. |
| `wl.h` | Local support library (used by tests and utilities). |
| `wl_test.c` | Tests for `wl.h`. |
| `examples/` | Demo code. |
| `test/` | Spectest harness, emcc fixtures, and fixture runner. |
| `docs/` | Design notes and compliance snapshots. |

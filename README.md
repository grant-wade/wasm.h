# wasm.h

A single-header WebAssembly runtime for C99. No external runtime dependency.

The public API follows Semantic Versioning. The current release version is available through `WASM_H_VERSION_MAJOR`, `WASM_H_VERSION_MINOR`, `WASM_H_VERSION_PATCH`, and `WASM_H_VERSION_STRING`.

Drop `wasm.h` into a project, define `WASM_IMPL` in one translation unit, and call exported Wasm functions from C. The runtime loads, validates, and interprets standard `.wasm` binaries, with support for most finalized post-MVP proposals.

Release documentation lives in [`docs/`](docs/README.md), including the [embedding guide](docs/EMBEDDING.md) and [build and test guide](docs/BUILDING.md).

## Quick start

Save this as `quickstart.c` next to `wasm.h`:

```c
#include <stdio.h>

#define WASM_IMPL
#include "wasm.h"

/* (module
 *   (func (export "main") (param i32) (result i32)
 *     local.get 0
 *     i32.const 1
 *     i32.add))
 */
static const uint8_t wasm_bytes[] = {
	0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
	0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,
	0x03, 0x02, 0x01, 0x00,
	0x07, 0x08, 0x01, 0x04, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00,
	0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x0b
};

int main(void) {
	wasm_runtime_t* rt;
	wasm_module_t* mod;
	int32_t result = 0;

	rt = wasm_runtime_new(NULL);
	if (!rt) return 1;
	if (wasm_runtime_last_error(rt) != WASM_OK) {
		fprintf(stderr, "init: %s\n", wasm_runtime_error_message(rt));
		wasm_runtime_free(rt);
		return 1;
	}

	mod = wasm_load(rt, wasm_bytes, sizeof(wasm_bytes));
	if (!mod) {
		fprintf(stderr, "load: %s\n", wasm_runtime_error_message(rt));
		wasm_runtime_free(rt);
		return 1;
	}

	if (wasm_call_fmt(mod, "main", "i(i)", (int32_t)42, &result) != WASM_OK) {
		fprintf(stderr, "call: %s\n", wasm_runtime_error_message(rt));
		wasm_free_module(mod);
		wasm_runtime_free(rt);
		return 1;
	}

	printf("result: %d\n", result);

	wasm_free_module(mod);
	wasm_runtime_free(rt);
	return 0;
}
```

Compile and run it:

```sh
cc -O2 quickstart.c -lm -o quickstart
./quickstart
# result: 43
```

This compiles the portable core only. Platform-backed WASI and Emscripten compatibility helpers are opt-in, as described below.

`wasm_call_fmt` uses `args(results)` syntax: `"i(i)"` means one `i32` argument and one `i32` result.

Format specifiers: `i` i32, `I` i64, `f` f32, `F` f64, `r` externref, `v` void result.

## Embedding API

### Lifecycle

| Function | Purpose |
| --- | --- |
| `wasm_runtime_new(config)` | Allocate and initialize a runtime. |
| `wasm_runtime_free(rt)` | Destroy and free a runtime allocated by `wasm_runtime_new`. |
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
| `wasm_bind_emscripten_stubs(rt)` | Register built-in `env` shims for common Emscripten imports (requires `WASM_ENABLE_PLATFORM`). |
| `wasm_bind_wasi_stubs(rt)` | Register built-in `wasi_snapshot_preview1` stubs (requires `WASM_ENABLE_PLATFORM`). |

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
| `wasm_runtime_error_message(rt)` | Most recent runtime error text. |
| `wasm_set_fuel(rt, n)` | Set an instruction fuel limit. |
| `wasm_get_fuel(rt)` | Read remaining fuel. |
| `wasm_dump_backtrace(rt, buffer, size)` | Format the captured call stack into a caller-provided buffer. |
| `wasm_error_string(err)` | Human-readable name for an error code. |

### Configuration

Pass a `wasm_config_t` to `wasm_runtime_new` (or `wasm_init` when using custom runtime storage) to override defaults:

```c
wasm_config_t cfg;
wasm_runtime_t* rt;

wasm_config_default(&cfg);
cfg.max_stack_values = 65536;
cfg.max_call_depth = 1024;
cfg.max_labels = 4096;
cfg.initial_gc_heap_size = 1 << 20;
cfg.frame_arena_size = 1 << 16;
cfg.lazy_validation = 1;
rt = wasm_runtime_new(&cfg);
```

Set `lazy_validation` to defer per-function body validation until first execution. The default is `0`, which preserves eager whole-module validation.

For interface-only users, `wasm_runtime_t` and `wasm_module_t` should be treated as opaque handles. Prefer `wasm_runtime_new()` and `wasm_runtime_free()` over relying on the concrete runtime layout.

### Custom allocators

Define before including `wasm.h`:

```c
#define WASM_MALLOC(sz)      my_malloc(sz)
#define WASM_REALLOC(p, sz)  my_realloc(p, sz)
#define WASM_FREE(p)         my_free(p)
#define WASM_CALLOC(n, sz)   my_calloc(n, sz)
```

### Platform bridges

The core runtime does not compile OS-specific code by default. To include the built-in WASI and Emscripten bridges, define `WASM_ENABLE_PLATFORM` in the implementation translation unit:

```c
#define WASM_ENABLE_PLATFORM 1
#define WASM_IMPL
#include "wasm.h"
```

Strict C99 builds on POSIX systems may also need `_POSIX_C_SOURCE=200809L` so libc exposes the required platform declarations. The repository's CMake targets configure this automatically.

### WASI stubs

With platform bridges enabled, `wasm_bind_wasi_stubs(rt)` registers the WASI Preview 1 surface implemented by the runtime, including argument/environment metadata, clocks, random bytes, standard I/O, descriptor operations, path operations, and explicit `NOSYS` placeholders.

The binding exposes only stdin, stdout, and stderr. It does **not** implicitly preopen the host working directory, so descriptor and path operations requiring a directory capability return `BADF`. This prevents a guest from acquiring ambient host filesystem access. The helper is still a compatibility layer rather than a complete WASI implementation.

### Emscripten stubs

With platform bridges enabled, `wasm_bind_emscripten_stubs(rt)` registers the `env` imports commonly needed by Emscripten-targeted modules, including `emscripten_date_now`, `emscripten_get_now`, `emscripten_get_heap_max`, `emscripten_resize_heap`, `_tzset_js`, `_localtime_js`, `_mmap_js`, `_munmap_js`, and several `__syscall_*` shims. When enabled, the runtime also lazily provides an imported `env.memory` that matches the module's declared memory limits.

**Security:** these Emscripten shims pass some guest-requested filesystem operations to the host and are intended only for trusted modules. They are never bound automatically.

## Build and test

```sh
cmake -S . -B build
cmake --build build --target check
```

`check` runs the strict native tests and, when their optional tools are available, the spectest harness and `emcc` fixture suite.

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
wasm [--help] [--wasi] [--emscripten] [--fuel N] <module.wasm> <command> [args...]
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
./build/wasm --wasi command.wasm call _start
./build/wasm --emscripten trusted-toolchain-module.wasm info
```

WASI and Emscripten imports are unbound by default. `--wasi` enables the capability-free WASI compatibility bindings described above. Emscripten-compatible `env` shims are opt-in via `--emscripten` or `wasm_bind_emscripten_stubs(rt)` in the embedding API and must only be used with trusted modules.

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
- for non-singleton library output, also emits a `*_api_t` handle so callers can keep an instance pointer together via `*_api_init(...)` or `*_api_from_ctx(...)`
- can emit a singleton-oriented API with `--singleton`, which hides one generated context instance in the `.c` file and removes the `ctx` parameter from wrapper calls
- if the module exports a memory, also emits `*_get_memory_ptr`, `*_get_memory_size`, `*_read_memory_string`, `*_read_memory`, and `*_write_memory` helpers against that exported memory, preferring an export literally named `memory` when several memories are exported
- emits an imports struct when the module imports host functions or globals
- filters common toolchain-noise exports by default, including Emscripten stack helpers and LLVM-prefixed internals
- supports generated wrappers for `i32`, `i64`, `f32`, `f64`, and `externref`
- skips unsupported signatures with diagnostics instead of generating incorrect code

`--embed` includes the module bytes directly in the generated `.c` file so the wrapper can initialize without loading the Wasm from disk at runtime.

`--singleton` generates a single-instance API for cases where only one module instance should exist in-process. In that mode, the generated export wrappers, `*_free`, `*_get_module`, `*_get_runtime`, and `*_get_last_error*` helpers no longer take a context pointer. `*_init(...)` and `*_init_embedded(...)` return `wasm_error_t` instead of a context pointer and manage one hidden static context inside the generated source.

`--no-prefix` removes the generated wrapper prefix from Wasm export wrappers only, so a module export like `sqlite3_open` stays `sqlite3_open(...)` instead of becoming `<prefix>_sqlite3_open(...)`. Generated types, lifecycle helpers, and runtime helpers remain prefixed. If a bare export name would collide with generated helpers or common C runtime symbols, `wasm2api` still disambiguates it, for example `malloc` becomes `export_malloc`.

When you stay in the default non-singleton mode, the generated header now also includes a small object layer:

- `*_api_init(...)` and `*_api_init_embedded(...)` return a `*_api_t` value with a `ctx`, `owns_ctx`, and `vtable`
- `*_api_from_ctx(...)` wraps an existing context without taking ownership
- `*_api_free(...)` frees owned contexts and simply detaches borrowed ones
- the vtable mirrors the generated export wrappers and helper methods, so callers that prefer an object-style surface can dispatch through `api.vtable->method(&api, ...)` while the flat `*_func(ctx, ...)` API remains available

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
| `examples/` | Demo code. |
| `test/wasm_test.c` | Runtime regression tests. |
| `test/wl.h` | Local support library used by native tests. |
| `test/wl_test.c` | Tests for `wl.h`. |
| `test/` | Unit tests, spectest harness, emcc fixtures, and fixture runner. |
| `docs/` | Release-facing embedding and build documentation. |
| `SECURITY.md` | Vulnerability reporting and host-capability security model. |
| `test/fuzz_wasm_load.c` | ASan/UBSan libFuzzer harness for module loading. |

# wasm.h

A single-header WebAssembly runtime for C99. No external runtime dependency.

Drop `wasm.h` into a project, define `WASM_IMPL` in one translation unit, and call exported Wasm functions from C. The runtime loads, validates, and interprets standard `.wasm` binaries, with support for most finalized post-MVP proposals.

The repository also now includes an experimental `wasi.h` scaffold for component-model work. The current surface initializes a dedicated engine, distinguishes core modules from component binaries, parses component structure beyond section framing, extracts embedded core modules and nested components, executes a narrow but real component-instantiation/linking path, and exposes structured component imports, exports, interface versions, core-instance records, component-instance records, start-section state, retained component value-type ASTs, broader component type metadata, aliases, canonical records, and file-relative source offsets for the major retained AST nodes.

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

	mod = wasm_load(rt, wasm_bytes, wasm_len);
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
| `wasm_bind_emscripten_stubs(rt)` | Register built-in `env` shims for common Emscripten imports. |
| `wasm_bind_wasi_stubs(rt)` | Register built-in `wasi_snapshot_preview1` stubs. |

## wasi.h Status

Current `wasi.h` capabilities cover binary introspection plus the completed low-level canonical ABI surface through Milestone 3:

- `wasi_init`, `wasi_destroy`, `wasi_load`, `wasi_free_component`
- core-vs-component detection through `wasi_detect_binary_kind`
- section introspection through `wasi_component_section_*`
- structured component imports/exports through `wasi_component_import_*` and `wasi_component_export_*`
- structured core-instance records through `wasi_component_core_instance_*`
- structured top-level and nested core-type records, including retained module declaration lists and composite subtype payloads, through `wasi_component_core_type_*`
- structured component-instance records through `wasi_component_instance_*`
- structured nested component records through `wasi_component_nested_component_*`
- structured alias records now include instance-export and outer forms through `wasi_component_alias_*`
- component start-section state through `wasi_component_has_start` and `wasi_component_start_func_index`
- parsed component function types, retained value-type ASTs, and nested component/instance type declarations through `wasi_component_type_*`, `wasi_component_type_decl_*`, `wasi_component_func_type_*`, and `wasi_component_valtype_*`
- structured alias and canonical-function records through `wasi_component_alias_*` and `wasi_component_canon_*`
- extracted embedded core modules through `wasi_component_core_module_*`
- file-relative source offsets for sections and major top-level AST records through the various `*_offset` accessors and `wasi_dump_component`
- host-side canonical ABI values through `wasi_value_t` and `wasi_value_set_string_copy`
- low-level scalar/string canonical calls through `wasi_canon_call` with `wasi_canon_options_t`
- narrow component instantiation through `wasi_instantiate`, `wasi_free_instance`, and `wasi_call`
- engine-bound import resolution through `wasi_bind_import_instance`, `wasi_bind_import_module`, `wasi_bind_import_func`, and `wasi_bind_import_component`
- scalar lift/lower for `bool`, integer widths, floats, `char`, and `string`
- compound canonical ABI support for `list`, `record`, `tuple`, `flags`, `variant`, `option`, `result`, and `enum`, including flat param lowering and spill-based result lifting on the current low-level path
- UTF-8, UTF-16, and latin1+utf16 string lowering/lifting through linear memory with `cabi_realloc` and optional post-return dispatch
- parsed canonical lift options for string encoding, memory selection, `realloc`, and post-return on the supported instance path
- host-defined resource registration and per-instance handle tables through `wasi_define_resource`, `wasi_instance_bind_resource_type`, `wasi_resource_new`, `wasi_resource_rep`, and `wasi_resource_drop`
- synchronous `own<T>` and `borrow<T>` lowering on the current `wasi_call` path for supported canon lifts, including outstanding-borrow guards, own-handle round-trips, origin-preserving cross-instance resource transfer machinery, and alias-aware component destructor dispatch
- narrow `canon lower` import bridging on the current instance path, including core-instance `from exports` namespaces that can expose lowered component funcs to later embedded core modules and route resource handles through the same synchronous lift/lower machinery

`wasi.h` still does not implement the full general component linking program. Today it provides parser/introspection, a low-level canonical ABI layer, completed Milestone 4 resource lifecycle support, and two active M5 slices. The current component-instance linking slice can instantiate nested component instances, bind their `module`/`func`/`instance`/`component`/`type` arg maps, resolve top-level component imports against engine-bound names and fully qualified interface versions including host-bound core modules, route exported component functions through live child-component instances rather than only static `instance { export ... }` records, resolve component-exported `type` args through live child-component instances as well as outer aliases on the supported path, execute zero-arg/zero-result component start functions on the current path, resolve non-core outer aliases for `func`, `instance`, and `component`, and resolve outer core-module aliases on the supported path for nested core-instance wiring. The current core-instance linking slice still materializes top-level core-instance `from exports` namespaces backed by direct forwards or synchronous `canon lower` thunks, can derive names for unnamed singleton exports in those namespaces when the destination core module makes the match unique, accepts direct core singleton args backed by the same forward-or-lower targets including unnamed local lowers when the destination core module exposes a unique matching import field, and still performs the sequential core-instance instantiation needed by later embedded core modules. Broader core-type alias coverage and more exotic core-instance forms still remain outside the supported path.

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

For interface-only users, `wasm_runtime_t` and `wasm_module_t` should be treated as opaque handles. Prefer `wasm_runtime_new()` and `wasm_runtime_free()` over relying on the concrete runtime layout.

### Custom allocators

Define before including `wasm.h`:

```c
#define WASM_MALLOC(sz)      my_malloc(sz)
#define WASM_REALLOC(p, sz)  my_realloc(p, sz)
#define WASM_FREE(p)         my_free(p)
#define WASM_CALLOC(n, sz)   my_calloc(n, sz)
```

### WASI stubs

`wasm_bind_wasi_stubs(rt)` registers minimal stubs for these `wasi_snapshot_preview1` imports: `fd_write`, `fd_read`, `fd_close`, `fd_sync`, `fd_seek`, `fd_fdstat_get`, `args_get`, `args_sizes_get`, `environ_get`, `environ_sizes_get`, `random_get`, `clock_time_get`, and `proc_exit`.

### Emscripten stubs

`wasm_bind_emscripten_stubs(rt)` registers the `env` imports commonly needed by Emscripten-targeted modules, including `emscripten_date_now`, `emscripten_get_now`, `emscripten_get_heap_max`, `emscripten_resize_heap`, `_tzset_js`, `_localtime_js`, `_mmap_js`, `_munmap_js`, and the `__syscall_*` shims used by the official sqlite3 Wasm build. When enabled, the runtime also lazily provides an imported `env.memory` that matches the module's declared memory limits.

## Build and test

```sh
cmake -S . -B build
cmake --build build --target check
```

`check` runs the full test pass: unit tests (`wasm_test`, `wasi_test`, `wl_test`), the spectest harness, the optional `wasmtime` comparison harness for representative `wasi.h` canonical ABI cases (`bool`, integer widths, `f32`, `f64`, `char`, UTF-8/UTF-16 string echoes, `list<u8>`/`list<u32>` round-trips, a nested `record { a: u32, b: string, c: list<u8> }` echo, `variant { none, num(u32), text(string) }` payload round-trips, and a joined-payload `variant { none, num(u32), big(u64) }` echo), and the `emcc` fixture suite.

- `wasm`
- `wasm2api`
- `wasi_test`
- `sqlite_wasm_demo`
- `wasm_test`
- `wl_test`
- `session_math_demo` (when `emcc` is available)

Additional targets:

```sh
cmake --build build --target wasm               # CLI runner
cmake --build build --target wasm2api            # Wrapper generator
cmake --build build --target wasi-component-run  # Component smoke harness (when wasm-tools component is available)
cmake --build build --target wasi-wasmtime-run   # Wasmtime reference comparisons (when wasmtime + wasm-tools component support are available)
cmake --build build --target wasm-spectest-run   # Spectest only
cmake --build build --target wasm-emcc-run       # Emcc fixtures only
```

## CLI

`wasm.c` builds a command-line tool for inspection and execution:

```
wasm [--help] [--no-wasi] [--emscripten] [--fuel N] <module.wasm> <command> [args...]
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
./build/wasm --emscripten examples/sqlite3.wasm info
```

WASI stubs are bound by default; `--no-wasi` disables them.
Emscripten-compatible `env` shims are opt-in via `--emscripten` or `wasm_bind_emscripten_stubs(rt)` in the embedding API.

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
./build/wasm2api --singleton --no-prefix examples/sqlite3.wasm sqlite_wasm
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

- `examples/sqlite3.wasm` is a checked-in sqlite module that imports Emscripten-compatible host shims and imported memory
- `cmake --build build --target sqlite_wasm_generate` runs `wasm2api --singleton --no-prefix` against that module and refreshes `examples/sqlite_wasm.h` plus `examples/sqlite_wasm.c`
- `cmake --build build --target sqlite_wasm_demo` builds and runs a native demo that loads the `.wasm` from disk, opens an in-memory database, and prints query results
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

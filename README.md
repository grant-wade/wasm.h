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

	rt = wasm_runtime_new(wasm_libc_allocator(), NULL);
	if (!rt) return 1;

	mod = wasm_load(rt, wasm_bytes, sizeof(wasm_bytes));
	if (!mod) {
		fprintf(stderr, "load: %s\n", wasm_runtime_error_message(rt));
		wasm_runtime_free(rt);
		return 1;
	}

	if (wasm_call_fmt(mod, "main", "i(i)", (int32_t)42, &result) != WASM_OK) {
		fprintf(stderr, "call: %s\n", wasm_runtime_error_message(rt));
		wasm_module_free(mod);
		wasm_runtime_free(rt);
		return 1;
	}

	printf("result: %d\n", result);

	wasm_module_free(mod);
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

## Embedding

See the [embedding guide](docs/EMBEDDING.md) for configuration, host imports, memory access, error handling, and custom allocators. Public declarations are in [`src/wasm.h`](src/wasm.h).

The portable core is enabled by default. To compile WASI and Emscripten bridges:

```c
#define WASM_ENABLE_PLATFORM 1
#define WASM_IMPL
#include "wasm.h"
```

Bind them explicitly before loading a module. `wasm_bind_wasi_stubs(rt)` exposes standard I/O and metadata; the guest reaches host files only through directories granted with `wasm_wasi_preopen_dir`, and `wasm_wasi_set_args` and `wasm_wasi_set_env` supply its arguments and environment. `wasm_bind_emscripten_stubs(rt)` forwards some filesystem operations to the host and must only be used with trusted modules. These are partial compatibility bindings.

## Build and test

The build is [`build.c`](build.c), written against [cbuild](https://github.com/grant-wade/cbuild); it needs only a C compiler.

```sh
cc -o cbuild build.c
./cbuild --test
```

Edit the independently compiled sources in `src/`; the root `wasm.h` is generated. Builds regenerate it automatically, or run `./cbuild -t wasm-header`.

See the [build guide](docs/BUILDING.md) for options, dependencies, modular linking, test selection, and the optional commit hook.

## Zig

The repository is also a Zig 0.17 package. It builds the runtime with Zig's C compiler and provides a `wasm` module with typed calls and host functions:

```zig
const rt = try wasm.Runtime.init(allocator, .{});
defer rt.deinit();
const instance = try rt.load(bytes);
defer instance.deinit();
const add = try instance.func("add", fn (i32, i32) i32);
const sum = try add.call(.{ 2, 40 });
```

See the [Zig guide](docs/ZIG.md) for installation, compiled modules, typed exports, host imports, memory, and threads.

## CLI

`wasm.c` builds a command-line tool for inspection and execution:

```
wasm [--help] [--wasi] [--dir HOST[::GUEST]] [--env NAME=VALUE] [--emscripten] [--fuel N]
     <module.wasm> <command> [args...]
```

| Command | Description |
| --- | --- |
| `run [args...]` | Run a WASI command's `_start`, passing `args` after the module path as its argv. |
| `info` | Module summary. |
| `imports` | List imports with their types. |
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

`info`, `imports`, `exports`, `funcs`, `types`, `memories`, and `custom-sections` compile the module without instantiating it, so they need no imports and run no guest code.

```sh
./build/wasm hello.wasm info
./build/wasm math.wasm call add 2 40
./build/wasm memory.wasm memory-read 0 0 64
./build/wasm --dir ./data::/data --env LANG=C command.wasm run input.txt
./build/wasm --emscripten trusted-toolchain-module.wasm info
```

WASI and Emscripten imports are unbound by default. `--wasi` enables the WASI compatibility bindings described above, and `run`, `--dir`, and `--env` imply it. Each `--dir` grants the guest one host directory, under the guest path after `::` or else under its host path. Emscripten-compatible `env` shims are opt-in via `--emscripten` or `wasm_bind_emscripten_stubs(rt)` in the embedding API and must only be used with trusted modules.

## wasm2api

Generate typed C wrappers for a module's exports and imports:

```sh
./build/wasm2api module.wasm my_module
./build/wasm2api --singleton --embed --init-func init_state module.wasm my_module
```

Output is `<prefix>.h` and `<prefix>.c`. Wrappers support `i32`, `i64`, `f32`, `f64`, and `externref`; unsupported signatures are skipped with diagnostics.

| Option | Effect |
| --- | --- |
| `--embed` | Embed the Wasm bytes in the generated source. |
| `--singleton` | Use one hidden context; omit context arguments from calls. Initialization returns `wasm_error_t`. |
| `--init-func <export>` | Call a `void(void)` export after loading. |
| `--no-prefix` | Remove prefixes from export wrappers only; conflicting names are disambiguated. |
| `--all-exports` | Include toolchain exports filtered out by default. |
| `--include-export`, `--include-prefix` | Include matching names; overrides defaults. |
| `--exclude-export`, `--exclude-prefix` | Exclude matching names. |

Wrappers cache export indices and own a runtime by default. Use `*_init_options_t` to supply a runtime, configuration, allocator for the owned runtime (`wasm_libc_allocator()` by default), or import bindings. An imports struct is emitted for host functions and globals. Memory helpers use the exported memory, preferring an export named `memory`.

Non-singleton output also provides `*_api_t` with `ctx`, `owns_ctx`, and `vtable`. `*_api_init` and `*_api_init_embedded` create owned contexts; `*_api_from_ctx` borrows one. `*_api_free` frees only owned contexts. Call through `api.vtable->method(&api, ...)` or the flat `*_func(ctx, ...)` API.

With `emcc` installed, `session_math_wasm` builds [the example module](examples/session_math_wasm.c), `session_math_generate` refreshes its wrappers, and `session_math_demo` builds the native demo. `./cbuild --test` runs the demo.

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
| Wide Arithmetic | Implemented, opt-in (`WASM_FEATURE_WIDE_ARITHMETIC`) |
| Custom Page Sizes | Implemented, opt-in (`WASM_FEATURE_CUSTOM_PAGE_SIZES`) |
| Compact Import Section | Implemented, opt-in (`WASM_FEATURE_COMPACT_IMPORTS`) |
| Threads | Implemented, opt-in (`WASM_FEATURE_THREADS`); see [Threads](docs/EMBEDDING.md#threads) |
| Extended Name Section | Not applicable: only module and function names are read, for backtraces, and malformed name sections never reject a module |
| Branch Hinting | Not implemented |

Opt-in proposals have not been standardized yet; enable them with `wasm_runtime_enable_feature` or `wasm_runtime_enable_all_features`.

SIMD execution includes ARM NEON and x86_64 SSE2 fast paths where the platform instructions preserve Wasm semantics exactly.

GC support covers recursive types, structs, arrays, `i31ref`, casts, and heap subtyping.

Exception handling covers both the legacy (`try`/`catch`/`delegate`) and current (`try_table`/`throw_ref`) instruction sets.

JS-only proposals (BigInt-to-i64, JS String Builtins) and text-format proposals (Custom Annotation Syntax) are out of scope for a binary C runtime.

## Repository layout

| Path | Description |
| --- | --- |
| `wasm.h` | Generated, standalone single-header distribution. |
| `src/wasm.h` | Public declarations for the modular runtime. |
| `src/` | Runtime source, including validation, interpretation, SIMD, and platform bridges. |
| `src/amalgamation.h` | Assembly order for the generated header. |
| `build.c`, `cbuild.h` | Build and test graph, and the vendored cbuild single header it uses. |
| `build.zig`, `build.zig.zon` | Zig package: runtime library, Zig module, and its tests. |
| `zig/` | Zig bindings (`wasm.zig`), the `wasm2zig` bindings generator, and their tests. |
| `tools/amalgamate.c` | Deterministic header generator and freshness check. |
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

# test/ Wasm emcc Harness

This directory holds a small harness for compiling real C fixtures with `emcc` and running the emitted `.wasm` files through [../wasm.h](../wasm.h).

The intent is not to require green results today. It is a regression and compatibility probe for the interpreter as feature support grows.

## Layout

- `fixtures/*.c` — source files compiled by `emcc`
- `spec/` — git submodule mirror of the official WebAssembly testsuite
- `runner.c` — native host runner that loads a `.wasm` file with `wasm.h`, prints exports, and optionally calls one export
- `CMakeLists.txt` — defines the runner, fixture compilation, and CTest integration

The `spec/` subtree is not wired into CTest yet. The current integration plan for `.wast`-driven spec execution lives in [../docs/WASM_SPEC_TESTSUITE_PLAN.md](../docs/WASM_SPEC_TESTSUITE_PLAN.md).

The runner binds the built-in WASI stubs automatically, so fixtures that call `printf`/`puts` through `wasi_snapshot_preview1.fd_write` can run without extra setup.

## Usage

From the repository root:

```sh
cmake -S . -B build
cmake --build build --target wasm-emcc-build
cmake --build build --target wasm-emcc-run
cmake --build build --target wasm-emcc-run-strict
```

Or directly in this directory:

```sh
cmake -S .. -B ../build
cmake --build ../build --target wasm-emcc-build
cmake --build ../build --target wasm-emcc-run
cmake --build ../build --target wasm-emcc-run-strict
```

`run` always completes and prints a pass/fail summary even if the current interpreter cannot load or execute some modules.

`run-strict` returns a failing exit code if any fixture fails.

If you still use `make`, the local and root Makefiles now forward to these CMake targets instead of maintaining separate build rules.

## Adding A Fixture

1. Add a new `fixtures/<name>.c` file.
2. Add a `wl_add_emcc_case(...)` entry in [test/CMakeLists.txt](test/CMakeLists.txt).
3. Set the fixture runner arguments and any required `emcc` flags in that entry.

Current fixtures assume a zero-argument export named `wl_case`.

For `emcc` constructor-style modules, the harness can also export `__wasm_call_ctors`. The runtime now runs the real start function when present, or an exported `__wasm_call_ctors` once at load time when there is no start section.

Supported runner modes are:

- `load-ok`
- `load-fail`
- `call-void wl_case`
- `call-i32 wl_case <expected>`
- `call-i64 wl_case <expected>`
- `call-stdout wl_case <expected_stdout>`
- `call-fail wl_case`

That makes it easy to keep deliberately unsupported-feature probes in the suite without having to remove them when they fail today.

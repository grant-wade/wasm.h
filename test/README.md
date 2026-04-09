# test/ Wasm emcc Harness

This directory holds a small harness for compiling real C fixtures with `emcc` and running the emitted `.wasm` files through [../wasm.h](../wasm.h).

The intent is not to require green results today. It is a regression and compatibility probe for the interpreter as feature support grows.

## Layout

- `fixtures/*.c` — source files compiled by `emcc`
- `spec/` — git submodule mirror of the official WebAssembly testsuite
- `fixture_runner.c` — native host runner that loads a `.wasm` file with `wasm.h`, prints exports, and optionally calls one export
- `spectest_runner.c` — native host runner that replays `json-from-wast` command streams against `wasm.h`
- `spectest_support.wat` — shared `spectest` globals, table, and memory used by the official testsuite
- `CMakeLists.txt` — defines the runner, fixture compilation, and CTest integration

The `spec/` subtree is wired into CTest through the system `wasm-tools json-from-wast` command and the native `spectest_runner`. Each top-level `.wast` file gets a matching `spectest.<name>` CTest entry, and the generated `.json`, `.wasm`, and `.wat` parts are materialized once up front through a shared fixture so the runtime cases can run in parallel.

The runner binds the built-in WASI stubs automatically, so fixtures that call `printf`/`puts` through `wasi_snapshot_preview1.fd_write` can run without extra setup.

## Usage

From the repository root:

```sh
cmake -S . -B build
cmake --build build --target wasm-emcc-build
cmake --build build --target wasm-emcc-run
cmake --build build --target wasm-emcc-run-strict
cmake --build build --target wasm-spectest-build
cmake --build build --target wasm-spectest-run
cmake --build build --target wasm-spectest-run-strict
```

Or directly in this directory:

```sh
cmake -S .. -B ../build
cmake --build ../build --target wasm-emcc-build
cmake --build ../build --target wasm-emcc-run
cmake --build ../build --target wasm-emcc-run-strict
cmake --build ../build --target wasm-spectest-build
cmake --build ../build --target wasm-spectest-run
cmake --build ../build --target wasm-spectest-run-strict
```

`run` always completes and prints a pass/fail summary even if the current interpreter cannot load or execute some modules.

`run-strict` returns a failing exit code if any fixture fails.

The spectest targets behave the same way, but failures can come from either the `wasm-tools json-from-wast` conversion step or the runtime harness itself. That is intentional: it keeps failures attributed to a specific source `.wast` file instead of collapsing everything into one monolithic job.

The spectest targets require a system `wasm-tools` executable with the `json-from-wast` subcommand available on `PATH` when CMake configures the project.

If `wasm-tools component embed` and `wasm-tools component new` are available, this directory also generates a small dummy component from [component_smoke.wit](component_smoke.wit) and exercises it through the native `component_runner` harness. When `wasm-tools parse` is also available, the harness additionally builds [component_instance_smoke.wat](component_instance_smoke.wat) and [component_start_smoke.wat](component_start_smoke.wat) to exercise parsed component-instance and top-level start-section records. Together those smoke fixtures now verify binary kind detection, section framing, embedded core modules, imports/exports, function types, core instances, component instances, start sections, aliases, and canonical lift/lower records exposed by [../wasi.h](../wasi.h).

If `json-from-wast` cannot compile a particular `.wast` file, the build fixture now records that as a skipped spectest case instead of failing the whole build or stopping later spec files from running.

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

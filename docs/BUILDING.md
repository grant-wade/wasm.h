# Building and Testing

The runtime itself is a single header and does not require CMake. The repository uses CMake to build regression tests, command-line tools, generated-wrapper examples, and compatibility harnesses.

## Requirements

Required for the native build:

- CMake 3.20 or newer
- A C99 compiler

Optional tools enable additional targets:

- `wasm-tools` with `json-from-wast` and `parse` enables the official spectest harness.
- `emcc` enables the Emscripten fixture suite and the generated `session_math` example.

## Configure and validate

```sh
cmake -S . -B build
cmake --build build
cmake --build build --target check
```

Warnings are treated as errors by default. Configure with `-DWL_ENABLE_WERROR=OFF` to disable that policy for local experimentation.

Useful configuration options:

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_TESTING` | `ON` | Register CTest tests and the `check` target. |
| `WL_ENABLE_PLATFORM` | `ON` | Enable platform-backed helpers in the test-only `test/wl.h` support library. |
| `WL_ENABLE_WERROR` | `ON` | Treat compiler warnings as errors. |
| `WL_ENABLE_EMCC_TESTS` | `ON` | Enable Emscripten fixtures when `emcc` is available. |

## Focused targets

```sh
cmake --build build --target wasm_test
cmake --build build --target wl_test
cmake --build build --target wasm
cmake --build build --target wasm2api
cmake --build build --target sqlite_wasm_demo
cmake --build build --target session_math_demo
```

`session_math_demo` is available only when `emcc` was found during configuration.

## Spectest

When the required `wasm-tools` subcommands are available:

```sh
cmake --build build --target wasm-spectest-build
cmake --build build --target wasm-spectest-run
cmake --build build --target wasm-spectest-run-strict
```

The non-strict target prints a summary without making known compatibility failures stop the build. The strict target returns a failing exit status for any failing case. The main `check` target uses the registered CTest suite and is strict.

## Emscripten fixtures

When `emcc` is available:

```sh
cmake --build build --target wasm-emcc-build
cmake --build build --target wasm-emcc-run
cmake --build build --target wasm-emcc-run-strict
```

The fixtures compile checked-in C sources under `test/fixtures/` to Wasm and execute them with the native fixture runner.

## Standalone compilation

For a minimal embedding, compile one C file containing the implementation include:

```sh
cc -std=c99 -O2 application.c -lm -o application
```

The math library is normally required on Unix-like systems. Platform-specific applications may need their usual system libraries in addition.

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

Warnings are treated as errors by default. Configure with `-DWASM_H_ENABLE_WERROR=OFF` to disable that policy for local experimentation.

Useful configuration options:

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_TESTING` | `ON` | Register CTest tests and the `check` target. |
| `WASM_H_ENABLE_PLATFORM` | `ON` | Enable platform-backed runtime compatibility helpers and `test/wl.h` helpers. |
| `WASM_H_ENABLE_WERROR` | `ON` | Treat compiler warnings as errors. |
| `WASM_H_ENABLE_EMCC_TESTS` | `ON` | Enable Emscripten fixtures when `emcc` is available. |
| `WASM_H_BUILD_FUZZER` | `OFF` | Build the Clang/libFuzzer module-loader harness. |

## Focused targets

```sh
cmake --build build --target wasm_test
cmake --build build --target wl_test
cmake --build build --target wasm
cmake --build build --target wasm2api
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

## Sanitizers and fuzzing

A Clang build can compile the module-loader libFuzzer harness with AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
CC=clang cmake -S . -B build-fuzz \
  -DBUILD_TESTING=OFF -DWASM_H_BUILD_FUZZER=ON
cmake --build build-fuzz --target wasm_load_fuzzer
./build-fuzz/wasm_load_fuzzer -runs=10000 -max_len=65536
```

The CI sanitizer job runs the unit and specification suites with ASan, LeakSanitizer, and UBSan.

## Installation

Install the header and CMake package metadata with:

```sh
cmake --install build --prefix /desired/prefix
```

CMake consumers can then use `find_package(wasm_h CONFIG REQUIRED)` and link the header-only declaration target `wasm_h::wasm_h`.

## Standalone compilation

For a minimal embedding, compile one C file containing the implementation include:

```sh
cc -std=c99 -O2 application.c -lm -o application
```

The math library is normally required on Unix-like systems. Platform-specific applications may need their usual system libraries in addition.

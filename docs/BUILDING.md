# Building and Testing

The runtime itself is a single header and does not require CMake. The repository uses CMake to build regression tests, command-line tools, generated-wrapper examples, and compatibility harnesses.

## Editing and generating the runtime

`src/` is the source of truth. Its `.c` files compile independently and share
declarations through `src/internal.h` and the headers in `src/internal/`.
`src/wasm.h` contains the public API. The root `wasm.h` is generated and remains
checked in so consumers can copy just that file.

Generate it without configuring a build or installing any scripting language:

```sh
cmake -P cmake/Amalgamate.cmake
```

Or use the configured build target:

```sh
cmake --build build --target wasm-header
```

`src/amalgamation.h` lists the implementation files in assembly order. The
generator expands quoted local includes once, preserves system includes and
preprocessor conditions, and writes only when the output changes. It adds no
timestamps or absolute paths. New implementation files must be added to both
that assembly and `WASM_RUNTIME_SOURCES` in `CMakeLists.txt`.

Normal builds regenerate before compiling header consumers. To check freshness
without changing anything (as CI does before building):

```sh
cmake -DWASM_AMALGAMATE_CHECK=ON -P cmake/Amalgamate.cmake
# With an existing build directory:
cmake --build build --target wasm-header-check
```

Opt in to the commit-time check for this checkout:

```sh
git config core.hooksPath .githooks
```

The hook validates a temporary snapshot of the Git index, so unstaged changes
cannot hide an outdated staged header. On failure, regenerate and stage the
header along with the intended source changes. The hook does not modify the
index or working files. If you already use a hook manager, invoke
`.githooks/pre-commit` from your existing pre-commit hook instead of changing
`core.hooksPath`.

## Modular library

```sh
cmake -S . -B build
cmake --build build --target wasm_runtime
```

This compiles every implementation file separately into `libwasm_runtime.a`
(or the platform equivalent). With `add_subdirectory`, link `wasm_h::runtime`
to get the public include directory and math-library dependency. Include
`wasm.h` without `WASM_IMPL` in the application. `WASM_H_ENABLE_PLATFORM`
controls the bridges compiled into the library.

For a manual core-only build, compile all `.c` files in `src/` and its
subdirectories with the same C99 compiler options, archive or link the resulting
objects, and add `src/` to the application's include path. Platform bridges and
custom allocator overrides must be configured consistently for every runtime
translation unit. The generated single-header build retains its existing
one-translation-unit configuration interface.

The `check` target runs the same runtime regression suite against both forms,
including custom allocator coverage, plus public-API smoke tests. Available
spectests also run against both forms. Installation continues to distribute the
standalone header and its header-only CMake target.

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

## CI platform coverage

Every push and pull request runs the matrix in
[`ci.yml`](../.github/workflows/ci.yml); it can also be started manually.
The matrix uses distinct standard [GitHub-hosted runner images](https://docs.github.com/en/actions/reference/runners/github-hosted-runners),
with explicit version and architecture labels:

| Platform | Images / target architectures | Compilers |
| --- | --- | --- |
| Linux | Ubuntu 22.04, 24.04, 26.04; x64 and native ARM64 | GCC and Clang |
| Linux 32-bit | Ubuntu 24.04 x64 host executing x86 binaries | GCC and Clang with multilib |
| macOS | 14, 15, 26 on Apple Silicon; 15 and 26 on Intel | Apple Clang |
| Windows | Server 2022 and Server 2025; x64 and executable Win32 targets | MSVC 2022 and 2026, respectively |
| Windows ARM | Windows 11 on native ARM64 | MSVC 2026 |
| Preview | Xcode 27 image on ARM64 | Apple Clang; non-blocking |

The main native matrix tests Debug with platform bridges disabled and Release
with bridges enabled. Ubuntu 24.04 x64 also retains platform-enabled Debug
coverage with both compilers. The additional Linux x86 jobs use Release with
bridges enabled. Every native job builds the modular library, standalone-header
consumers, CLI, and wrapper generator, then runs the regression and specification
suites. Native 64-bit matrix jobs also exercise CMake installation.

The shared `setup-wasm-tools` action installs a pinned release matching the
runner's OS and architecture and verifies both required subcommands, so missing
tool support fails setup instead of silently dropping spec coverage. A 32-bit
runtime uses the 64-bit host's `wasm-tools` executable. CMake enables SSE2
floating-point arithmetic for 32-bit x86 GCC/Clang targets to preserve Wasm's
binary32/binary64 rounding. Standalone-header builds on those targets also need
`-msse2 -mfpmath=sse`; x87 extended-precision arithmetic is not supported.

Separate x64 and ARM64 Linux jobs run ASan/LSan/UBSan and libFuzzer smoke tests.
The sanitizer jobs retain debug symbols and assertions with `-O1`, run two tests
at a time, and fail individual tests that exceed five minutes. Both runtime
build forms still run the full suite; matching spec cases share a CTest resource
lock because their harnesses may write the same compiled text fixture.
Header freshness remains a separate check. The workflow has 53 blocking jobs
and one non-blocking preview job, with independent matrix failures and
cancellation of superseded runs. Runner aliases and larger hardware tiers are
not duplicated when they provide the same OS/architecture coverage. Emscripten
fixtures remain available locally but are not enabled by this native matrix.

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

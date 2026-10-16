# Building and Testing

Requires CMake 3.20+ and a C99 compiler. Copying `wasm.h` into an application does not require CMake.

## Build and test

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
cmake --build build --target check
```

For multi-configuration generators such as Visual Studio, add `--config Release` to build commands.

`check` runs the native tests and available specification tests against both the single header and modular library. Optional tools must be available during configuration:

- `wasm-tools` with `json-from-wast` and `parse`: specification tests.
- `emcc`: Emscripten fixtures and the `session_math` example.

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_TESTING` | `ON` | Enable tests and the `check` target. |
| `WASM_H_ENABLE_PLATFORM` | `ON` | Enable runtime and test platform helpers. |
| `WASM_H_ENABLE_WERROR` | `ON` | Treat compiler warnings as errors. |
| `WASM_H_ENABLE_EMCC_TESTS` | `ON` | Enable Emscripten fixtures when `emcc` is available; requires platform helpers. |
| `WASM_H_BUILD_FUZZER` | `OFF` | Build the Clang/libFuzzer module-loader harness. |

Build individual targets with `cmake --build build --target <target>`:

| Target | Purpose |
| --- | --- |
| `wasm`, `wasm2api` | CLI and wrapper generator. |
| `wasm_runtime` | Modular static library. |
| `wasm_test`, `wasm_modular_test`, `wl_test` | Native regression executables. |
| `session_math_demo` | Wrapper example; requires `emcc`. |
| `wasm-spectest-build`, `wasm-emcc-build` | Prepare specification tests or Emscripten fixtures. |
| `wasm-spectest-run-strict`, `wasm-emcc-run-strict` | Run a suite and fail on test failures. |

The corresponding `*-run` targets report failures but return success. `check` is strict. See [test/README.md](../test/README.md) for fixture development and specification-test skips.

## Runtime sources

Edit `src/`; its `.c` files compile independently. `src/wasm.h` contains the public declarations. Regenerate and commit the root `wasm.h` after source changes:

```sh
cmake -P cmake/Amalgamate.cmake
```

Normal builds regenerate it automatically. Add new implementation files to both `src/amalgamation.h` and `WASM_RUNTIME_SOURCES` in `CMakeLists.txt`.

Check freshness without modifying the header:

```sh
cmake -DWASM_AMALGAMATE_CHECK=ON -P cmake/Amalgamate.cmake
```

The equivalent build targets are `wasm-header` and `wasm-header-check`. To check staged sources and header before commits:

```sh
git config core.hooksPath .githooks
```

If using a hook manager, call `.githooks/pre-commit` from the existing hook instead.

## Modular library

With `add_subdirectory`, link `wasm_h::runtime` and include `wasm.h` without defining `WASM_IMPL`. The target supplies the public include path and math-library dependency.

For a manual build, compile every `.c` file under `src/`, link the objects, and add `src/` to the application's include path. Apply platform and allocator definitions consistently to every runtime translation unit.

## Standalone header and installation

Compile one application translation unit with `WASM_IMPL` defined before including `wasm.h`:

```sh
cc -std=c99 -O2 application.c -lm -o application
```

32-bit x86 GCC/Clang builds require `-msse2 -mfpmath=sse` to preserve Wasm floating-point rounding. CMake supplies these flags automatically.

Install the standalone header and its CMake package:

```sh
cmake --install build --prefix /desired/prefix
```

Consumers use `find_package(wasm_h CONFIG REQUIRED)` and link `wasm_h::wasm_h`; they still define `WASM_IMPL` in one translation unit. The modular library is not installed.

## CI and fuzzing

[ci.yml](../.github/workflows/ci.yml) defines the OS, architecture, and compiler matrix, header freshness check, ASan/LSan/UBSan runs, and libFuzzer smoke tests. Emscripten fixtures run locally.

To fuzz the module loader with Clang and ASan/UBSan:

```sh
CC=clang cmake -S . -B build-fuzz \
  -DBUILD_TESTING=OFF -DWASM_H_BUILD_FUZZER=ON
cmake --build build-fuzz --target wasm_load_fuzzer
./build-fuzz/wasm_load_fuzzer -runs=10000 -max_len=65536
```

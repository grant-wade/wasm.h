# AGENTS.md

## Project Summary

This repo is a C99 WebAssembly runtime developed in `src/` and distributed as the generated single header `wasm.h`. `test/wl.h` is a local support library used by native tests, not the main product.

## Source Of Truth

- Read `README.md` first for the current runtime surface and supported proposals.
- Read `CMakeLists.txt` and `test/CMakeLists.txt` for the actual build and test graph.
- Treat `build/` as generated output only. It may contain stale artifacts from older configurations and is not an authoritative description of the current repo.

## Important Files

- `src/`: independently compiled runtime implementation and shared private headers; edit these sources.
- `src/wasm.h`: public embedding API declarations.
- `src/amalgamation.h`: assembly order for the single-header distribution.
- `wasm.h`: generated distribution; regenerate with `cmake -P cmake/Amalgamate.cmake`.
- `test/wasm_test.c`: primary regression coverage for the runtime.
- `wasm.c`: CLI runner and inspection tool built on top of `wasm.h`.
- `wasm2api.c`: generator for typed C wrappers around Wasm exports and imports.
- `test/wl.h`: local test support/platform layer.
- `test/wl_test.c`: tests for `test/wl.h`.
- `test/fixture_runner.c`: runner for emcc-generated Wasm fixtures.
- `test/spectest_runner.c`: spec JSON harness and diagnostic normalization layer.
- `test/spec/`: WebAssembly testsuite submodule content.
- `docs/`: release-facing embedding and build documentation for the current runtime.

## Build And Test

- Configure: `cmake -S . -B build`
- Main validation pass: `cmake --build build --target check`
- Generate the root header: `cmake -P cmake/Amalgamate.cmake` or build target `wasm-header`.
- Check generation without modifying files: `cmake -DWASM_AMALGAMATE_CHECK=ON -P cmake/Amalgamate.cmake`.
- Modular library: `wasm_runtime`; `check` exercises both modular and single-header builds.
- Focused native targets: `wasm_test`, `wl_test`, `wasm`, `wasm2api`, `session_math_demo`, and `wasm_load_fuzzer` when enabled
- Emscripten fixtures: `wasm-emcc-build`, `wasm-emcc-run`, `wasm-emcc-run-strict`
- Spectest targets exist only when system `wasm-tools` provides both `json-from-wast` and `parse`.

## Agent Notes

- Keep changes C99-compatible. The project builds with warnings-as-errors by default.
- Keep `.c` files independently compilable. Add new units to `WASM_RUNTIME_SOURCES` and `src/amalgamation.h`.
- For generated `wasm.h`, public declarations must work without `WASM_IMPL`, implementation-only internals stay behind `#ifdef WASM_IMPL`. Internal linkage uses `WASM_PRIVATE`/`WASM_PRIVATE_INLINE` to support both build forms.
- `WASM_H_ENABLE_PLATFORM` is a CMake option that defaults ON for this repo build; `test/wl.h` and the runtime platform bridges may be compiled with it off.
- If a change touches validation, init expressions, memarg decoding, or spectest failures, check `test/wasm_test.c` and `test/spectest_runner.c` together.
- `test/spectest_runner.c` intentionally normalizes only semantically equivalent diagnostics. Do not add aliases for cases that should actually change pass/fail behavior.
- The current runtime supports reference types, bulk memory, multi-memory, tail calls, exceptions, SIMD, GC, extended const expressions, and Memory64.
- `wasm2api.c` currently generates wrappers for `i32`, `i64`, `f32`, `f64`, and `externref`; unsupported signatures are skipped with diagnostics.

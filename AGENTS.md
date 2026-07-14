# AGENTS.md

## Project Summary

This repo is a small C99 codebase centered on `wasm.h`, a single-header WebAssembly runtime. `wl.h` is a local support library used by tests and utilities, not the main product.

## Source Of Truth

- Read `README.md` first for the current runtime surface and supported proposals.
- Read `CMakeLists.txt` and `test/CMakeLists.txt` for the actual build and test graph.
- Treat `build/` as generated output only. It may contain stale artifacts from older configurations and is not an authoritative description of the current repo.

## Important Files

- `wasm.h`: main runtime, validator, interpreter, public embedding API.
- `wasm_test.c`: primary regression coverage for the runtime.
- `wasm.c`: CLI runner and inspection tool built on top of `wasm.h`.
- `wasm2api.c`: generator for typed C wrappers around Wasm exports and imports.
- `wl.h`: local support/platform layer.
- `wl_test.c`: tests for `wl.h`.
- `test/fixture_runner.c`: runner for emcc-generated Wasm fixtures.
- `test/spectest_runner.c`: spec JSON harness and diagnostic normalization layer.
- `test/spec/`: WebAssembly testsuite submodule content.
- `docs/`: release-facing embedding and build documentation for the current runtime.

## Build And Test

- Configure: `cmake -S . -B build`
- Main validation pass: `cmake --build build --target check`
- Focused native targets: `wasm_test`, `wl_test`, `wasm`, `wasm2api`, `session_math_demo`
- Emscripten fixtures: `wasm-emcc-build`, `wasm-emcc-run`, `wasm-emcc-run-strict`
- Spectest targets exist only when system `wasm-tools` provides both `json-from-wast` and `parse`.

## Agent Notes

- Keep changes C99-compatible. The project builds with warnings-as-errors by default.
- For `wasm.h`, remember the single-header split: public declarations must work without `WASM_IMPL`, implementation-only internals stay behind `#ifdef WASM_IMPL`.
- `WL_ENABLE_PLATFORM` is a CMake option that defaults ON for this repo build; standalone `wl.h` users may compile with it off.
- If a change touches validation, init expressions, memarg decoding, or spectest failures, check `wasm_test.c` and `test/spectest_runner.c` together.
- `test/spectest_runner.c` intentionally normalizes only semantically equivalent diagnostics. Do not add aliases for cases that should actually change pass/fail behavior.
- The current runtime supports reference types, bulk memory, multi-memory, tail calls, exceptions, SIMD, GC, extended const expressions, and Memory64.
- `wasm2api.c` currently generates wrappers for `i32`, `i64`, `f32`, `f64`, and `externref`; unsupported signatures are skipped with diagnostics.

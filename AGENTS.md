# AGENTS.md

## Project Summary

This repo is a C99 WebAssembly runtime developed in `src/` and distributed as the generated single header `wasm.h`.

## Source Of Truth

- Read `README.md` first for the current runtime surface and supported proposals.
- Read `build.c` for the actual build and test graph. It is a C program using the vendored `cbuild.h`.
- Treat `build/` as generated output only. It may contain stale artifacts from older configurations and is not an authoritative description of the current repo.

## Important Files

- `src/`: independently compiled runtime implementation and shared private headers; edit these sources.
- `src/wasm.h`: public embedding API declarations.
- `src/amalgamation.h`: assembly order for the single-header distribution.
- `wasm.h`: generated distribution; regenerate with `./cbuild -t wasm-header`.
- `build.c`: build and test graph; `cbuild.h` is the vendored single-header build system it uses (generated upstream in the cbuild repository; do not edit it here).
- `tools/amalgamate.c`: generator and freshness check for `wasm.h`.
- `test/unit/`: primary regression coverage for the runtime, one `*_test.c` per `src/` unit; `test.h`/`test.c` are the test harness, `support.h`/`support.c` the shared helpers, `main.c` runs the suites declared in `suites.h`, and `amalgamation.c` builds them as one translation unit against `wasm.h`.
- `wasm.c`: CLI runner and inspection tool built on top of `wasm.h`.
- `wasm2api.c`: generator for typed C wrappers around Wasm exports and imports.
- `build.zig`, `build.zig.zon`: Zig 0.17 package; it compiles every `.c` file under `src/`, so new units need no change there.
- `zig/wasm.zig`: Zig bindings over the C API; `zig/wasm_test.zig` tests them.
- `zig/wasm2zig.zig`: generator for Zig `Exports`, `Api`, and `Imports` declarations from a Wasm module, run at build time by `addBindings` in `build.zig`; `zig/wasm2zig_test.zig` tests its output for `zig/testdata/guest.wasm` (source in `guest.wat`).
- `examples/zig/`: standalone Zig project that compiles `guest/calc.c` to Wasm and embeds it; `./cbuild --test` runs it as `zig_example`.
- `test/fixture_runner.c`: runner for emcc-generated Wasm fixtures.
- `test/spectest_runner.c`: spec JSON harness and diagnostic normalization layer.
- `test/spec/`: WebAssembly testsuite submodule content.
- `test/proposals/`: proposal suites vendored from their repositories (each with a `SOURCE` file), registered like `test/spec/proposals/`.
- `test/wast/<suite>/`: this runtime's own `.wast` suites, registered from `wast_suites` in `build.c` with the features each runs with.
- `docs/`: release-facing embedding and build documentation for the current runtime.

## Build And Test

- Bootstrap once: `cc -o cbuild build.c` (it rebuilds itself when `build.c` or `cbuild.h` changes).
- Main validation pass: `./cbuild --test`. Add `--release` or `--no-platform` for the other CI configurations; pass the same options on every invocation.
- Generate the root header: `./cbuild -t wasm-header`.
- Check generation without modifying files: `cc -o amalgamate tools/amalgamate.c && ./amalgamate --check`.
- Modular library: `wasm_runtime`; `--test` exercises both modular and single-header builds.
- Focused native targets (`./cbuild -t <name>`): `wasm_test`, `wasm`, `wasm2api`, `session_math_demo`, and `wasm_load_fuzzer` with `--fuzzer`.
- Test selection: `--test-label=unit|modular|spectest|proposal|emcc`, `--test-filter=GLOB`, `--test-exclude=GLOB`, `--list-tests`. Unit suites run as `wasm_test.<suite>` and `wasm_modular_test.<suite>`, with `<suite>` the `test/unit/` path of a `*_test.c` file without `_test.c` and with `/` as `.` (e.g. `exec.cont`). Logs land in `build/<config>/tests/`.
- Proposal suites (`test/spec/proposals/`, `test/proposals/`) skip unless listed in `implemented_proposals` in `build.c`; `--spec-proposals` runs them all.
- Spectests exist only when `wasm-tools` on `PATH` provides both `json-from-wast` and `parse`; Emscripten fixtures only when `emcc` is found; the `zig` test only when `zig` 0.17 is found.
- Zig package: `zig build test` (wrapper and modular C tests), `zig build cross` (compile for the targets in `cross_targets`). Public API changes in `src/wasm.h` usually need matching changes in `zig/wasm.zig`.

## Agent Notes

- Keep changes C99-compatible. The project builds with warnings-as-errors by default.
- Keep `.c` files independently compilable. Add new units to `wasm_runtime_sources` in `build.c` and `src/amalgamation.h`.
- Private function declarations belong in a header beside their owning `.c` file, and only functions used by another translation unit belong there. Keep file-local helpers `static`; public API declarations remain in `src/wasm.h`.
- Include a source file's own header first, then the interfaces it uses directly. Headers must be self-contained and their include graph acyclic. Shared layouts belong in focused owner/state headers; do not introduce umbrella internal headers.
- Keep OS dependencies in `src/platform/` and SIMD intrinsics in `src/simd/native.h`. The build compiles each source header independently (`wasm_private_header_check`) and checks missing prototypes in modular runtime builds.
- For generated `wasm.h`, public declarations must work without `WASM_IMPL`, implementation-only internals stay behind `#ifdef WASM_IMPL`. Internal linkage uses `WASM_PRIVATE`/`WASM_PRIVATE_INLINE` to support both build forms.
- Platform helpers default ON for this repo build (`--no-platform` turns them off); the runtime platform bridges may be compiled with them off.
- If a change touches validation, init expressions, memarg decoding, or spectest failures, check `test/unit/` and `test/spectest_runner.c` together.
- `test/spectest_runner.c` intentionally normalizes only semantically equivalent diagnostics. Do not add aliases for cases that should actually change pass/fail behavior.
- The current runtime supports reference types, bulk memory, multi-memory, tail calls, exceptions, SIMD, GC, extended const expressions, Memory64, and the not yet standardized wide arithmetic, custom page sizes, compact imports, threads, stack switching, and custom descriptors. All are enabled by default; `wasm_config_t.features` selects them (`WASM_FEATURES_ALL`, `WASM_FEATURES_STANDARD`). Atomic primitives live in `src/atomics.c`; thread blocking and shared-memory reservation live in `src/platform/threads.c`. Continuation objects, resume, suspend, and switch live in `src/exec/cont.c`; execution stacks, one per continuation plus the main stack, in `src/exec/stack.c`.
- `wasm2api.c` currently generates wrappers for `i32`, `i64`, `f32`, `f64`, and `externref`; unsupported signatures are skipped with diagnostics.

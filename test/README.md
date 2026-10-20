# Tests

See the [build guide](../docs/BUILDING.md) for setup and test commands.

| File | Purpose |
| --- | --- |
| `unit/` | Runtime regressions, one `*_test.c` per `src/` unit, built against the header (`amalgamation.c`) and modular library. |
| `wasm_core_test.c` | Public API smoke test. |
| `spec/` | Official WebAssembly testsuite submodule. |
| `proposals/` | Proposal suites vendored from their repositories; each `SOURCE` names the repository and commit. |
| `wast/` | This runtime's own `.wast` suites, one directory per entry of `wast_suites` in `build.c`. |
| `spectest_runner.c` | Replays `wasm-tools json-from-wast` commands. |
| `spectest_support.wat`, `spectest_support_extra.wat` | Shared specification-test imports. |
| `fixtures/*.c`, `fixture_runner.c` | Emscripten fixtures and native runner. |
| `amalgamate_test.c`, `amalgamate_hook_test.sh` | Header generator and pre-commit hook tests. |

## Specification tests

The build requires `wasm-tools` on `PATH` with both `json-from-wast` and `parse`. Each top-level `.wast` file registers `spectest.<name>` and `spectest.modular.<name>` tests (label `spectest`). `./cbuild --test` converts the files it needs before running them, and reconverts only when a `.wast` changes.

Failed `.wast` conversions are recorded as skipped tests; runtime failures fail `./cbuild --test`.

## Adding an Emscripten fixture

1. Add `fixtures/<name>.c` with a zero-argument export named `fixture_case`.
2. Add an entry to `emcc_cases` in [build.c](../build.c) with runner arguments and any required `emcc` flags. It runs as the `emcc.<name>` test.

Runner modes:

- `load-ok`
- `load-fail`
- `call-void fixture_case`
- `call-i32 fixture_case <expected>`
- `call-i64 fixture_case <expected>`
- `call-stdout fixture_case <expected_stdout>`
- `call-fail fixture_case`

The runner binds WASI stubs for standard I/O. The runtime invokes the start function at load time, or `__wasm_call_ctors` if exported and no start section exists.

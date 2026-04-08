# Repository Memories

Memory Version: 4

Current curated contents of `/memories/repo/` as of 2026-04-07.

This file is the canonical checked-in snapshot of repo memory.

- Increment `Memory Version` whenever repo memory is added, edited, merged, or removed.
- Keep `/memories/repo/` synchronized with this file in the same change.
- Remove stale repo-memory entries that are no longer represented here instead of keeping historical duplicates.

## spec-validator-notes.md

- Const-expression evaluation in `wasm.h` must see all prior immutable globals: use the current global index while decoding the global section, and `mod->num_globals` for data/element offsets after globals are loaded.
- `wasm__read_memory_type()` must decode limits as `u32` unless the memory type itself has the Memory64 flag. Using runtime feature enablement to select `u64` decoding lets overlong MVP LEB encodings slip past `binary-leb128`.
- Memarg parsing needs to preserve whether an explicit memory index immediate was encoded (`has_memory_index`), otherwise invalid MVP memarg flags can be misread as `memory 0` and fail later with unrelated validator errors.
- `test/spectest_runner.c` intentionally normalizes many equivalent diagnostics; add aliases there only for semantically equivalent rejections, not for modules that actually load when spec says invalid.
- Conditional branch validators (`br_if`, `br_on_null`, `br_on_non_null`, `br_on_cast`, `br_on_cast_fail`) need real pop/re-push fallthrough normalization of label arguments. Peeking without retyping preserves overly-specific refs and lets spec-invalid stack-shape cases load.
- `br_on_cast` source-operand validation wants the actual operand to be a subtype of the declared source type, while `ref.test`/`ref.cast` still need the target type to be a subtype of the operand’s static type. Reusing one helper for both directions causes either `expected 0x15, got 0x15` false negatives or valid cast rejections.
- Canonical GC type equality must include the declared supertype chain, not just the immediate struct/array/function body, or sibling types with matching fields collapse and make `br_on_cast` succeed when it should not.
- Typed-reference comparisons in validators and cross-module type matching must use the effective `wasm_reftype_t`, not the raw outer `wasm_valtype_t`. The outer tag can differ (`anyref` vs `funcref`) for the same semantic typed ref, and imports / `call_indirect` / `ref.test` need subtype-based compatibility across modules rather than exact type equality.

## br-table-validator.md

- `br_table` target labels must be merged by a common subtype/result-type match, not exact signature equality. For reference labels, choose the more specific target type when one matches the other; exact-equality checks reject valid `meet-funcref` / `meet-nullref` spectests.

## spectest-harness.md

- Spectest in `test/CMakeLists.txt` depends on a system `wasm-tools` executable with both `json-from-wast` and `parse`; if either subcommand is unavailable, spectest targets are skipped at configure time.
- The build generates per-file `spectest_case_<name>` targets, `build/test/spec/<name>.json` command streams, and `build/test/spectest_support.wasm`; CTest uses one shared `spectest.build` fixture before running `spectest.<name>` cases.
- `test/spectest_runner.c` consumes `wasm-tools json-from-wast` output directly, including positive `module` entries with `module_type: "text"` and optional `binary_filename` fields.
- Text modules in positive and negative cases are compiled and validated through `wasm-tools parse` and `wasm-tools validate` from inside the runner when needed.
- The runner parses wrapped signed integer spellings from JSON and intentionally normalizes only semantically equivalent diagnostics.
- Spectest externref handles should be encoded away from the runtime's low-bit GC/i31 tags before `any.convert_extern`; otherwise host refs like `ref.extern 0` can be misclassified as `i31ref` in `br_on_cast`/`ref_cast` coverage.

## wasm-runtime.md

- `wasm.h` is the single-header runtime; `wasm_load()` performs load-time validation and feature gating via `wasm_runtime_t.enabled_features`, with helper APIs `wasm_enable_feature()`, `wasm_disable_feature()`, and `wasm_enable_all_features()`.
- The implemented proposal surface includes reference types, bulk memory, multi-memory, tail calls, exceptions, SIMD, GC, extended const expressions, and Memory64; unsupported proposals fail cleanly during load or validation.
- Imported globals are supported by reference, participate in init expressions, and mutable non-imported globals are rejected in `global.get` init-expression paths.
- Public host APIs worth remembering are `wasm_call_fmt`, `wasm_bind_host_func`, memory/global helpers, backtrace inspection, runtime configuration (`wasm_config_t` and `wasm_init(rt, NULL)`), GC field/array helpers, and `wasm_bind_wasi_stubs()`.
- Built-in WASI stubs currently cover `fd_write`, args/env sizing and access, `fd_fdstat_get`, `random_get`, `clock_time_get`, `environ_get`, and `proc_exit`; host override hooks are macro-based near the top of `WASM_IMPL`.
- Float min/max intentionally avoid `fmin[f]` and `fmax[f]` and use signed-zero-aware comparisons to preserve Wasm semantics.
- Regression coverage lives primarily in `wasm_test.c`, with additional spectest and emcc coverage under `test/`.

## wasm2api.md

- `wasm2api.c` generates `<prefix>.h` and `<prefix>.c` wrappers from a `.wasm` module by inspecting exports and imports through `wasm.h`.
- Generated export wrappers currently support `i32`, `i64`, `f32`, `f64`, and `externref`; unsupported parameter or result types are skipped with diagnostics.
- Generated code caches export indices, exposes lifecycle and `last_error*` helpers, and maps multi-value returns to output pointers.
- If the module imports host functions or globals, the generated header includes a `<prefix>_imports_t` struct and the init path binds those imports.
- `wasm2api --embed` emits the module bytes into the generated `.c` and adds embedded-byte helpers so callers can initialize without a separate runtime file path.
- Keep internal-only forward declarations in `wasm.h` behind `#ifdef WASM_IMPL`; declarations-only consumers of generated headers are built under `-Werror`.

## wl-platform.md

- `WL_ENABLE_PLATFORM` is the opt-in gate for `wl.h` time, sleep, and filesystem APIs; with the flag off, the portable core stays plain C99 without POSIX or Windows dependencies.
- In the CMake build, `WL_ENABLE_PLATFORM` is a top-level option that defaults ON and is propagated through `wl_project_options`; standalone `wl.h` consumers need to define the macro themselves before including the header.
- `wl.h` contains both POSIX and Windows platform backends under the same flag.
- `wl.h` uses `WL_INLINE` and avoids MSVC-hostile patterns in the portable core.
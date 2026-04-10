# Repository Memories

Memory Version: 15

Current curated contents of `/memories/repo/` as of 2026-04-10.

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

## exception-typing.md

- `try_table` branch-target validation must use subtype-compatible typed-reference matching, not exact signature equality, or `catch` / `catch_ref` rejects valid payload flows like `(ref $t)` into `(ref null $t)`.
- `catch_ref` and `catch_all_ref` produce a definitely non-null exception reference. In `wasm.h`, validate that slot with an explicit non-null `noexn` reftype; passing a bare `exnref` type falls back to the validator's default nullable reference handling and incorrectly rejects `(ref exn)` targets.

## br-table-validator.md

- `br_table` target labels must be merged by a common subtype/result-type match, not exact signature equality. For reference labels, choose the more specific target type when one matches the other; exact-equality checks reject valid `meet-funcref` / `meet-nullref` spectests.

## spectest-harness.md

- Spectest in `test/CMakeLists.txt` depends on a system `wasm-tools` executable with both `json-from-wast` and `parse`; if either subcommand is unavailable, spectest targets are skipped at configure time.
- The build generates per-file `spectest_case_<name>` targets, `build/test/spec/<name>.json` command streams, and `build/test/spectest_support.wasm`; CTest uses one shared `spectest.build` fixture before running `spectest.<name>` cases.
- `test/spectest_runner.c` consumes `wasm-tools json-from-wast` output directly, including positive `module` entries with `module_type: "text"` and optional `binary_filename` fields.
- Text modules in positive and negative cases are compiled and validated through `wasm-tools parse` and `wasm-tools validate` from inside the runner when needed.
- The runner parses wrapped signed integer spellings from JSON and intentionally normalizes only semantically equivalent diagnostics.
- Spectest externref handles should be encoded away from the runtime's low-bit GC/i31 tags before `any.convert_extern`; otherwise host refs like `ref.extern 0` can be misclassified as `i31ref` in `br_on_cast`/`ref_cast` coverage.

## component-type-parser.md

- Nested `core:type` declarations inside component/instance types in `wasi.h` are inline core module types: declaration opcode `0x00` is followed by module-type opcode `0x50`, not a core type index.
- Nested and top-level core module types in `wasi.h` now retain module declaration lists for core `type`, `import`, `alias`, and `export` entries, with regressions in `wasi_test.c` covering both nested type-space and top-level core-type fixtures.
- Top-level `core-type` sections in `wasi.h` need backtracking for entry opcode `0x50`: it can mean either a core module type or a non-final subtype, so parse against the remaining entry count instead of deciding locally.

## component-linker.md

- `wasi.h` now executes `component instance` instantiate records on the current M5 path, including nested component instantiation, arg-map binding for `func` and `instance` imports, and alias resolution through live child instances rather than only static `from exports` records.
- Engine-level `wasi_bind_import_instance()` bindings now satisfy top-level component instance imports by fully qualified interface name/version; unresolved instance imports fail at instantiation time with named diagnostics.
- The current linker still does not resolve outer aliases, imported components, direct component func imports from the engine, or start sections.

## wasm-runtime.md

- `wasm.h` is the single-header runtime; `wasm_load()` performs load-time validation and feature gating via `wasm_runtime_t.enabled_features`, with helper APIs `wasm_enable_feature()`, `wasm_disable_feature()`, and `wasm_enable_all_features()`.
- The implemented proposal surface includes reference types, bulk memory, multi-memory, tail calls, exceptions, SIMD, GC, extended const expressions, and Memory64; unsupported proposals fail cleanly during load or validation.
- Imported globals are supported by reference, participate in init expressions, and mutable non-imported globals are rejected in `global.get` init-expression paths.
- Public host APIs worth remembering are `wasm_call_fmt`, `wasm_bind_host_func`, memory/global helpers, backtrace inspection, runtime configuration (`wasm_config_t` and `wasm_init(rt, NULL)`), GC field/array helpers, `wasm_bind_wasi_stubs()`, and `wasm_bind_emscripten_stubs()`.
- Built-in WASI stubs now cover `fd_write`, `fd_read`, `fd_close`, `fd_sync`, `fd_seek`, `fd_fdstat_get`, args/env sizing and access, `random_get`, `clock_time_get`, `environ_get`, and `proc_exit`; the Emscripten helper binds sqlite-style `env` shims and lazily materializes `env.memory` against the module's declared limits.
- The default validator label budget is `WASM_MAX_LABELS = 4096`, which is high enough for large real-world modules like the official sqlite3 Wasm build without custom runtime config.
- Float min/max intentionally avoid `fmin[f]` and `fmax[f]` and use signed-zero-aware comparisons to preserve Wasm semantics.
- Regression coverage lives primarily in `wasm_test.c`, with additional spectest and emcc coverage under `test/`.
- `wasi.h`'s current M5 slice now accepts top-level core-instance `from exports` records on the narrow instance path and can register their function exports as either direct forwards or synchronous `canon lower` bridges, so later embedded core modules can call back into supported local component funcs and resource builtins through real lower/lift dispatch.

## wasm2api.md

- `wasm2api.c` generates `<prefix>.h` and `<prefix>.c` wrappers from a `.wasm` module by inspecting exports and imports through `wasm.h`.
- Generated wrappers own a runtime by default and expose an `*_init_options_t` path for callers that want to supply their own runtime, config, or imports.
- `--init-func <export>` validates a `void(void)` export and invokes it automatically after load.
- Export collection now applies built-in filters for common Emscripten / LLVM noise by default; `--all-exports` disables those defaults, and user include/exclude exact or prefix rules can override or extend them.
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

## wasi-reference-harness.md

- `test/wasi_wasmtime_runner.c` builds standard scalar/direct reference components at test time with `wasm-tools component embed --world compare` plus `wasm-tools component new`, then compares `wasi_canon_call()` against `wasmtime run --invoke` on the same generated component.
- The harness intentionally targets the low-level canonical ABI path instead of `wasi_instantiate()`, because `wasm-tools component new` emits multi-core-module/start-shim components that exceed the current narrow instance path.
- Standard embedded components require post-return signatures to match the lowered core result type (`i32` for bool/u8/u16/u32/s8/s16/s32, `i64` for s64/u64); hardcoding `i32` makes `wasm-tools component new` reject 64-bit cases.
- CTest label `wasi-wasmtime` is enabled only when both `wasmtime` and `wasm-tools` provide `component embed` and `component new`.

## wasi-standard-type-space.md

- Standard `wasm-tools component new` output can introduce top-level type imports for named WIT types; those imports occupy slots in the component type index space even when `wasi.h` stores only defined types in `component->types`.
- `wasi__parse_component_imports()` must use `wasi__read_component_externdesc()` rather than assuming `kind-byte + type-index`, or standard function import descriptors with bound tags leave trailing bytes.
- `wasi.h` now resolves the current standard named-type compare cases across imported and defined component type slots, with `wasi-wasmtime` coverage for standard `list`, `record`, and `variant` round-trips.
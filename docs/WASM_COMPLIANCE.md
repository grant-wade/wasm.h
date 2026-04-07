# Wasm Spectest Compliance

This document records the current end-to-end status of `wasm.h` against the WebAssembly spec testsuite as exercised by the local harness in `test/`.

The goal here is accuracy, not aspiration. The runtime has broad internal unit coverage and many proposal-focused features under direct test, but the spectest run is the authoritative compatibility snapshot for real `.wast` inputs processed through local WABT tooling and executed by `test/spectest_runner.c`.

## Snapshot

- Date: 2026-04-06
- Build tree: `build/`
- Harness: `spectest.build` plus per-file `spectest.<name>` CTest entries
- Conversion path: `tools/wast2json --enable-all` -> JSON command stream -> `spectest_runner`
- Source artifacts: `build/Testing/Temporary/LastTest.log`, `LastTestsFailed.log`, and `LastTestsDisabled.log`

### Summary

| Scope | Count | Notes |
| --- | ---: | --- |
| Total CTest entries | 272 | Includes unit tests, spectest build fixture, spectest cases, and emcc fixtures |
| Spectest build fixture | 1 pass | `spectest.build` completed successfully |
| Runnable spectest cases | 257 | One case per top-level `test/spec/*.wast` |
| Spectest passes | 45 | Case passed end-to-end |
| Spectest failures | 183 | Case ran and failed |
| Spectest skips | 29 | Local `wast2json` could not translate the file; not counted as passing |

Useful percentages:

- Spectest pass rate by case file: `45 / 257 = 17.5%`
- Spectest non-failing rate by case file: `74 / 257 = 28.8%`
- Overall CTest non-failing rate as reported by CTest: `89 / 272 = 32.7%`

The gap between the spectest pass rate and the overall CTest summary matters. Internal unit coverage and the emcc fixtures are green, but the official spec corpus still exposes major conformance gaps.

## What Is Currently Green

The following spectest case files currently pass end-to-end:

- `address`
- `address0`
- `address1`
- `align0`
- `comments`
- `endianness`
- `exports0`
- `f32_bitwise`
- `f32_cmp`
- `f64_bitwise`
- `f64_cmp`
- `float_exprs`
- `float_exprs0`
- `float_memory`
- `float_memory0`
- `forward`
- `imports4`
- `inline-module`
- `int_exprs`
- `linking2`
- `load0`
- `memory-multi`
- `memory_copy0`
- `memory_copy1`
- `memory_fill0`
- `memory_init0`
- `memory_redundancy`
- `memory_size0`
- `memory_size1`
- `memory_size2`
- `memory_trap`
- `memory_trap0`
- `memory_trap1`
- `simd_memory-multi`
- `simd_select`
- `stack`
- `start0`
- `store0`
- `store1`
- `store2`
- `table-sub`
- `traps`
- `traps0`
- `type`
- `unwind`

In practice, the green set is strongest in a narrow subset of:

- basic core execution and harness plumbing
- numeric compare/bitwise coverage
- parts of the 32-bit memory model
- a few table and linking cases
- a very small SIMD subset

## What The Current Run Says

The failures are not random. They cluster into a few clear buckets.

### 1. Silent Loader Or Instantiation Failure Is Still The Biggest Gap

The last run contains 51 failures with this signature:

`runtime returned NULL without setting an error`

Representative cases:

- `block`, `br`, `br_if`, `if`, `loop`, `return`, `select`, `nop`, `unreachable`
- `call`, `call_indirect`, `call_ref`
- `data`, `data0`, `elem`, `imports`, `imports1`, `imports2`
- `load`, `load2`, `memory_grow`, `memory_size_import`
- `table_copy`, `table_init`, `table_set64`, `table_get64`

This is more than a diagnostics problem. It means the harness is still hitting module-load or instantiation paths that fail without a structured error, which prevents clear attribution and leaves a large part of even the MVP-shaped corpus non-compliant.

### 2. The Validator Still Disagrees With The Spectest Corpus In Many Invalid-Module Cases

The last run contains at least:

- 26 failures surfaced as `stack underflow`
- 4 failures surfaced as `block leaves wrong stack height`

Representative cases:

- `i32`, `labels`, `local_set`, `table_fill`, `table_get`, `table_grow`
- `local_get`, `memory_size`, `memory_size3`, `table_size`
- many SIMD invalid-program files also collapse into these same validator diagnostics

Several of these tests expect a broader `type mismatch` style failure, but the current validator rejects them through a different internal path. Even when the module is correctly rejected, exact spectest matching still fails.

### 3. Memory64 Is Not Spectest-Compliant

The current run shows consistent `i32`/`i64` mismatches and related load failures across the `*64` corpus.

Representative failures:

- `address64`
- `align64`
- `binary_leb128_64`
- `bulk64`
- `call_indirect64`
- `endianness64`
- `float_memory64`
- `load64`
- `memory_copy64`
- `memory_fill64`
- `memory_grow64`
- `memory_init64`
- `memory_redundancy64`
- `memory_trap64`
- `table_copy64`
- `table_fill64`
- `table_grow64`
- `table_size64`

There is also a separate tooling problem: `memory64.wast` itself is currently skipped because local `wast2json` cannot translate it.

The practical status is simple: do not treat Memory64 as compliant based on the current spectest run.

### 4. SIMD Support Is Partial At Best In Spectest Terms

Although the internal unit suite has direct SIMD coverage, the official spectest run remains far from green.

Only these SIMD-tagged spec files currently pass:

- `simd_memory-multi`
- `simd_select`

Everything else in the exercised fixed-width SIMD corpus currently fails, with several distinct patterns:

- unknown prefixed opcodes, for example `0xFD 0x8E`, `0xFD 0x8F`, `0xFD 0x94`, `0xFD 0xD6`
- validator stack-typing failures in invalid-program cases
- runtime behavior mismatches such as `unreachable executed`
- at least one crash in the pasted CTest summary: `simd_load16_lane`

Representative failures:

- `simd_i16x8_arith`
- `simd_i16x8_sat_arith`
- `simd_f64x2_rounding`
- `simd_i64x2_cmp`
- `simd_bit_shift`
- `simd_lane`
- `simd_store8_lane`

The correct conclusion is that SIMD execution exists, but fixed-width SIMD is not yet spectest-complete.

### 5. Relaxed SIMD Is Clearly Unsupported In The Current Runtime Path

Representative failures:

- `i8x16_relaxed_swizzle`
- `i32x4_relaxed_trunc`
- `i16x8_relaxed_q15mulr_s`
- `relaxed_dot_product`
- `relaxed_laneselect`
- `relaxed_madd_nmadd`
- `relaxed_min_max`

These fail with explicit `unknown prefixed opcode` diagnostics in the `0xFD 0x100+` range. This is a clean unsupported-feature bucket, not a close miss.

### 6. GC, Typed-Ref, And Exception Coverage Is Not Yet Green End-To-End

The runtime unit suite exercises significant GC and exception machinery directly, but the official testsuite still exposes two problems:

- many proposal-heavy files do not get through local `wast2json`
- several runnable files still fail in the runtime or validator

Representative runtime-side failures:

- `br_on_null` with `unknown opcode 0xD5`
- `br_on_non_null` with `unknown opcode 0xD6`
- `ref_as_non_null` with `unknown opcode 0xD4`
- `throw` with `unknown opcode 0x1F`
- `throw_ref` with `runtime returned NULL without setting an error`

Representative skipped files due to conversion failure:

- `array`, `array_copy`, `array_fill`, `array_init_data`, `array_init_elem`, `array_new_data`, `array_new_elem`
- `ref_cast`, `ref_eq`, `ref_null`, `ref_test`
- `struct`, `tag`, `try_table`
- `type-canon`, `type-equivalence`, `type-rec`, `type-subtyping`

The current spectest result therefore does not justify a blanket compliance claim for GC, typed references, or exceptions.

### 7. Floating-Point Semantics Still Have Real Spec Bugs

Representative failures:

- `f32`: expected negative zero but got positive zero
- `f64`: expected negative zero but got positive zero
- `float_misc`: one observed value differs by one unit in the last place
- `conversions`: expected `integer overflow` but got `invalid conversion to integer`
- `float_exprs1`: unexpected `unreachable executed`

The pasted CTest summary also reports crashes in:

- `float_literals`
- `func`

This is a correctness and stability issue, not just a string-matching issue.

### 8. A Non-Trivial Slice Of Failures Comes From WABT/Text-Format Expectations

Some tests in the corpus are really checking the behavior and wording of `wat2wasm` or malformed-binary diagnostics, not just runtime execution.

Representative failures:

- `const`
- `int_literals`
- `i64`
- `id`
- `obsolete-keywords`
- `token`
- `simd_address`
- `simd_const`
- `simd_load`
- `simd_load_splat`
- `simd_splat`
- `simd_store`
- `binary`
- `binary0`
- `binary-leb128`
- `custom`
- `utf8-custom-section-id`
- `utf8-import-field`
- `utf8-import-module`
- `utf8-invalid-encoding`

Some of these are genuine malformed-binary compliance gaps in `wasm_load`, while others are simply a mismatch between the expected error text in the official suite and the output produced by the local WABT version.

For compliance reporting, these still count as failures.

## Skipped Cases

The 29 skipped cases are important because they are currently unmeasured. They do not prove runtime support.

Current skipped files:

- `annotations`
- `array`
- `array_copy`
- `array_fill`
- `array_init_data`
- `array_init_elem`
- `array_new_data`
- `array_new_elem`
- `br_on_cast`
- `br_on_cast_fail`
- `extern`
- `global`
- `i31`
- `instance`
- `memory`
- `memory64`
- `ref_cast`
- `ref_eq`
- `ref_null`
- `ref_test`
- `struct`
- `table`
- `table64`
- `tag`
- `try_table`
- `type-canon`
- `type-equivalence`
- `type-rec`
- `type-subtyping`

Observed skip causes from the generated `.status` files include:

- local WABT parser rejection of proposal syntax such as `rec`, array types, `anyref`, `ref.i31`, and text-format annotation forms
- local WABT assertion failure on `global.wast`
- parser rejection of tests that intentionally exercise text-format edge cases such as `memory.wast` and `table.wast`

Until those files are translatable through the local `wast2json` path, compliance for those parts of the suite remains unknown.

## Relationship To Other Test Signals

The broader project test picture is better than the spectest result:

- `wasm_test` passed completely in the same run
- `wl_test` passed completely in the same run
- all 12 emcc fixtures passed completely in the same run

That means the runtime already supports several targeted scenarios that the spectest run still does not validate end-to-end. The likely causes are a mix of:

- missing or incomplete runtime behavior on wider spec coverage
- validator diagnostics that do not match spectest expectations closely enough
- harness error-reporting gaps
- local WABT translation limitations for proposal-heavy `.wast` files

The spectest number is therefore the conservative compatibility metric and should be preferred in user-facing compliance claims.

## Bottom Line

Current status as of 2026-04-06:

- The spectest harness is integrated and stable enough to give a useful baseline.
- Core compliance is still incomplete; many MVP-shaped control-flow, call, import, data, and table cases fail.
- Memory64 is not compliant.
- Fixed-width SIMD is not compliant despite passing internal unit coverage.
- Relaxed SIMD is unsupported in the current runtime path.
- GC, typed-ref, and exception support are not yet demonstrated end-to-end against the official suite.
- Local WABT translation gaps currently hide part of the proposal-heavy corpus behind skips.

For now, the most accurate public statement is:

> `wasm.h` has strong targeted regression coverage and passes internal/unit and emcc fixture suites, but official spectest conformance is still early-stage: 45 passing files, 183 failing files, and 29 skipped files in the current local run.
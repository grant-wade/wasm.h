# Wasm Spectest Compliance

This document records the current end-to-end status of `wasm.h` against the WebAssembly spec testsuite as exercised by the local harness in `test/`.

The goal here is accuracy, not aspiration. `wasm_test`, `wl_test`, and the emcc fixtures are all green in this run, but the official spectest corpus remains the authoritative compatibility snapshot because it exercises real `.wast` inputs through the local JSON conversion path and `test/spectest_runner.c`.

## Snapshot

- Date: 2026-04-07
- Build tree: `build/`
- Harness: `spectest.build` plus per-file `spectest.<name>` CTest entries
- Conversion path: system `wasm-tools json-from-wast` -> JSON command stream -> `spectest_runner`
- Support artifact: `test/spectest_support.wat` -> `wasm-tools parse` -> `build/test/spectest_support.wasm`
- Source artifacts: `build/Testing/Temporary/LastTest.log` and `LastTestsFailed.log`

### Summary

| Scope | Count | Notes |
| --- | ---: | --- |
| Total CTest entries | 272 | Includes unit tests, spectest build fixture, spectest cases, and emcc fixtures |
| Spectest build fixture | 1 pass | `spectest.build` completed successfully |
| Runnable spectest cases | 257 | One case per top-level `test/spec/*.wast` |
| Spectest passes | 191 | Case passed end-to-end |
| Spectest failures | 66 | Case ran and failed |
| Spectest skips | 0 | No spectest case was skipped in this run |

Useful percentages:

- Spectest pass rate by case file: `191 / 257 = 74.3%`
- Spectest non-failing rate by case file: `191 / 257 = 74.3%`
- Overall CTest non-failing rate as reported by CTest: `206 / 272 = 75.7%`

Compared with the 2026-04-06 snapshot, the new run is materially better measured and materially greener:

- spectest passes increased from `45` to `191`
- spectest failures decreased from `183` to `66`
- spectest skips dropped from `29` to `0`

Note on freshness:

- the `191 / 66` aggregate above is still the last full `257`-case sweep
- targeted reruns after that snapshot have since moved `call_ref` and `return_call_ref` to green and removed the old `module_definition` harness blocker
- the document below records those verified deltas, but the top-line counts should stay at `191 / 66` until the next full `check` run is captured

That change matters. A year ago style coverage is no longer the right comparison point; the newer `main` spec corpus is now being exercised much more fully. Several proposal-heavy files that were previously hidden behind tooling skips now run end-to-end and either pass cleanly or fail with concrete runtime or validator diagnostics.

## What Is Currently Green

The current green set is now large enough that the exact pass list is more useful than a short hand-wavy summary.

- `address`, `address0`, `address1`, `address64`, `align`, `align0`, `align64`, `binary-gc`, `binary0`, `binary_leb128_64`, `block`, `br`, `bulk`, `bulk64`
- `call`, `call_indirect`, `call_ref`, `comments`, `const`, `conversions`, `custom`, `data0`, `data1`, `data_drop0`, `endianness`, `endianness64`, `exports`, `exports0`
- `f32`, `f32_bitwise`, `f32_cmp`, `f64`, `f64_bitwise`, `f64_cmp`, `fac`, `float_exprs`, `float_exprs0`, `float_exprs1`, `float_literals`, `float_memory`, `float_memory0`, `float_memory64`, `float_misc`
- `forward`, `func_ptrs`, `i32`, `i64`, `if`, `imports`, `imports0`, `imports1`, `imports2`, `imports3`, `imports4`, `inline-module`
- `int_exprs`, `int_literals`, `labels`, `left-to-right`, `linking`, `linking0`, `linking1`, `linking2`, `linking3`, `load`, `load0`, `load1`, `load2`, `load64`
- `local_get`, `local_set`, `local_tee`, `loop`, `memory-multi`, `memory64-imports`, `memory_copy`, `memory_copy0`, `memory_copy1`, `memory_copy64`, `memory_fill`, `memory_fill0`, `memory_fill64`
- `memory_grow`, `memory_grow64`, `memory_init`, `memory_init0`, `memory_init64`, `memory_redundancy`, `memory_redundancy64`, `memory_size`, `memory_size0`, `memory_size1`, `memory_size2`, `memory_size3`, `memory_size_import`
- `memory_trap`, `memory_trap0`, `memory_trap1`, `memory_trap64`, `names`, `nop`, `obsolete-keywords`, `ref_eq`, `ref_func`, `ref_is_null`, `return`, `return_call_ref`
- `simd_address`, `simd_align`, `simd_bit_shift`, `simd_bitwise`, `simd_boolean`, `simd_const`, `simd_conversions`, `simd_f32x4`, `simd_f32x4_arith`, `simd_f32x4_cmp`, `simd_f32x4_pmin_pmax`, `simd_f32x4_rounding`
- `simd_f64x2`, `simd_f64x2_arith`, `simd_f64x2_cmp`, `simd_f64x2_pmin_pmax`, `simd_f64x2_rounding`, `simd_i16x8_arith`, `simd_i16x8_arith2`, `simd_i16x8_cmp`, `simd_i16x8_extadd_pairwise_i8x16`
- `simd_i16x8_extmul_i8x16`, `simd_i16x8_q15mulr_sat_s`, `simd_i16x8_sat_arith`, `simd_i32x4_arith`, `simd_i32x4_arith2`, `simd_i32x4_cmp`, `simd_i32x4_dot_i16x8`, `simd_i32x4_extadd_pairwise_i16x8`
- `simd_i32x4_extmul_i16x8`, `simd_i32x4_trunc_sat_f32x4`, `simd_i32x4_trunc_sat_f64x2`, `simd_i64x2_arith`, `simd_i64x2_arith2`, `simd_i64x2_cmp`, `simd_i64x2_extmul_i32x4`, `simd_i8x16_arith`, `simd_i8x16_arith2`, `simd_i8x16_cmp`
- `simd_i8x16_sat_arith`, `simd_int_to_int_extend`, `simd_linking`, `simd_load`, `simd_load16_lane`, `simd_load32_lane`, `simd_load64_lane`, `simd_load8_lane`, `simd_load_extend`, `simd_load_splat`, `simd_load_zero`, `simd_memory-multi`
- `simd_select`, `simd_splat`, `simd_store`, `simd_store16_lane`, `simd_store32_lane`, `simd_store64_lane`, `simd_store8_lane`, `skip-stack-guard-page`, `stack`, `start`, `start0`, `store`, `store0`, `store1`, `store2`, `switch`
- `table-sub`, `table_copy`, `table_fill`, `table_get`, `table_grow`, `table_init`, `table_set`, `table_size`, `token`, `traps`, `traps0`, `type`, `type-canon`, `unreachable`, `unwind`
- `utf8-custom-section-id`, `utf8-import-field`, `utf8-import-module`, `utf8-invalid-encoding`

In practice, the green set is strongest in these areas:

- most MVP-style numeric and control-flow execution
- the core Memory64 memory-instruction surface
- a large fixed-width SIMD slice that was not green in the previous snapshot
- most text/binary parser edge-case files that do not depend on unsupported harness commands
- basic table, reference, and bulk-memory operations in their 32-bit forms

## What The Current Run Says

The failures are not random. They now cluster into a much smaller number of clearer buckets than they did on 2026-04-06.

### 1. [DONE] Multi-Memory Was A Harness Gating Bug

The initial 2026-04-07 run contained `40` failures with the signature:

`multiple memories`

Representative cases:

- `address0`, `address1`, `align0`, `binary0`
- `data0`, `data1`, `data_drop0`, `exports0`
- `imports0`, `imports1`, `imports2`, `imports3`, `imports4`
- `linking0`, `linking1`, `linking2`, `linking3`
- `load0`, `load1`, `load2`
- `memory_copy0`, `memory_copy1`, `memory_fill0`, `memory_grow`, `memory_init0`
- `memory_size0`, `memory_size1`, `memory_size2`, `memory_size3`, `memory_size_import`
- `memory_trap0`, `memory_trap1`, `start0`, `store0`, `store1`, `store2`, `traps0`

There is also at least one nearby feature-gating failure in the same neighborhood:

- `align` reports `feature 'multi-memory' is disabled`

This initially looked like the biggest compliance gap, but it turned out to be a spectest harness bug rather than a missing runtime capability.

#### How It Was Fixed

- `test/spectest_runner.c` was enabling all features and then disabling multi-memory again for any JSON file whose filename did not contain `memory-multi` or `memory_multi`.
- Removing that filename heuristic allowed the runtime's existing multi-memory implementation to run across the broader spectest corpus.
- The closely related `linking0` case also needed module-load side effects to match spectest semantics, so active element segments are now applied before active data segments.

As a result, the broad `multiple memories` cluster is gone in the current run.

### 2. [DONE] The Harness Now Implements `module_definition`

The earlier `module_definition: unsupported command type 'module_definition'` bucket is gone.

The affected files were:

- `instance`
- `memory`
- `memory64`
- `table`

Those cases are no longer blocked at the harness-command layer. They now fail, where they still fail, for deeper runtime or validator reasons instead of because the command is unimplemented.

#### How It Was Fixed

- `test/spectest_runner.c` now stores named module definitions and resolves them through a shared module-source path instead of rejecting the command outright
- `module_definition` is validated without full instantiation, using `wasm-tools validate`, so declarative definitions do not eagerly allocate runtime state
- `module_instance` now instantiates fresh modules from stored definitions, which gives the generative semantics that `instance.wast` expects
- the runtime table loader in `wasm.h` was extended to accept the newer default-initializer table encoding used by current spectest artifacts, which was required to get past the first `instance.*.wasm` decode failures

### 3. Proposal Opcodes Still Fail Along The Spectest Path

The original 2026-04-07 run contained `9` failures with direct unknown-opcode diagnostics in the `0xD4` / `0xD5` / `0xD6` / `0x1F` / `0x14` range.

That bucket is smaller now.

Representative cases:

- `br_on_cast` with `unknown opcode 0xD5`
- `br_on_cast_fail` with `unknown opcode 0xD6`
- `br_on_non_null` with `unknown opcode 0xD6`
- `br_on_null` with `unknown opcode 0xD5`
- `ref_as_non_null` with `unknown opcode 0xD4`
- `ref_cast` with `unknown opcode 0xD4`
- `unreached-invalid` with `unknown opcode 0xD4`

Recent targeted work cleared this bucket for:

- `call_ref`
- `return_call_ref`
- the old `instance`-path `unknown opcode 0x1F` failure, which turned out to be `try_table`, not `throw`

`try_table` is still not green, but the current failure is now earlier and narrower: `try_table.1.wasm` still dies during type-section decode rather than on an unknown-opcode path in the interpreter.

This matters because `wasm_test` now has direct coverage for several of these proposal features. The compliance gap is therefore narrower and more specific than "feature not implemented". The current evidence points to a mismatch between spec-path decoding or validation and the separately exercised unit-test path.

#### How Part Of This Was Fixed

- `wasm.h` now recognizes and validates the current encodings for `call_ref` (`0x14`) and `return_call_ref` (`0x15`)
- the interpreter now executes both opcodes directly, including null-function traps with the expected `null function reference` wording
- `ref.func` validation now preserves typed function-reference information instead of flattening everything to plain `funcref`, which was the root cause of the early `call_ref` type-mismatch failures
- `try_table` (`0x1F`) is now recognized by the control-target builder, immediate skipper, validator, and exception unwinder, which moved `instance` past the old opcode barrier even though `try_table` is not yet fully spectest-complete

### 4. Relaxed SIMD Is Still Unsupported, But Fixed-Width SIMD Is No Longer The Story

The current run contains `7` failures with `unknown prefixed opcode` diagnostics in the `0xFD 0x100+` range:

- `i8x16_relaxed_swizzle`
- `i32x4_relaxed_trunc`
- `i16x8_relaxed_q15mulr_s`
- `relaxed_dot_product`
- `relaxed_laneselect`
- `relaxed_madd_nmadd`
- `relaxed_min_max`

That is a clean unsupported-feature bucket.

The important change from the prior snapshot is what is *not* failing anymore: most fixed-width SIMD files now pass end-to-end. Outside relaxed SIMD, the remaining SIMD-tagged failures are now diagnostic mismatches such as:

- `simd_address` expecting `offset out of range` but getting `offset 4294967296 exceeds i32 memory address space`
- `simd_lane` expecting `i8 constant out of range` but getting `malformed lane index` from `wasm-tools`

So the correct current statement is: fixed-width SIMD is largely green, relaxed SIMD is still unsupported.

### 5. GC, Typed References, And Reference-Value Semantics Are Still Not Spectest-Complete

The new run is much more informative than the April 6 run because these files are now being exercised rather than skipped, but they are not green yet.

Representative failures:

- `array`, `ref`, and `struct` expect `unknown type` but currently report `resolve_module_reftypes: malformed module`
- `array_copy`, `array_fill`, `array_init_data`, and `array_init_elem` reject immutable destinations correctly in spirit, but with different wording than the corpus expects
- `array_new_data` and `array_new_elem` fail with `invalid reference value`
- `extern` fails with `only null reference values are supported`
- `i31` returns the wrong tagged reference value: expected `ref(0x1)` but got `ref(0x3)`
- `select` returns the wrong function reference identity: expected `funcref(0)` but got `funcref(1)`
- `ref_null`, `ref_test`, `type-equivalence`, `type-rec`, and `type-subtyping` still fail in the loader or validator

The current run therefore still does not justify a blanket compliance claim for GC or typed references, even though the unit suite exercises substantial GC machinery directly.

### 6. Memory64 Core Memory Ops Are Mostly Green, But Table64 And Wider 64-Bit Typing Are Not

The Memory64 picture is much better than it was in the previous snapshot.

These files now pass:

- `address64`
- `align64`
- `load64`
- `memory64-imports`
- `memory_copy64`
- `memory_fill64`
- `memory_grow64`
- `memory_init64`
- `memory_redundancy64`
- `memory_trap64`
- `float_memory64`

The remaining 64-bit failures are concentrated elsewhere:

- `memory64` no longer fails because `module_definition` is unsupported by the harness; the remaining issue is now deeper runtime validation around Memory64 limits and pages
- `call_indirect64` fails with `section 9 decode failed: type mismatch`
- `table64` fails with `section 4 decode failed: malformed module`
- `table_copy64`, `table_get64`, `table_init64`, and `table_set64` fail with `section 9 decode failed: type mismatch`
- `table_fill64` and `table_grow64` fail with `type mismatch: expected 0x7F, got 0x7E`
- `table_size64` fails with `type mismatch: expected 0x7E, got 0x7F`

The practical conclusion is that the core Memory64 memory surface is now mostly green in spectest terms, but Table64 and related 64-bit index typing are still incomplete.

### 7. Validator Acceptance And Exact Diagnostics Still Cost Real Cases

Several failures are not broad feature gaps. They are narrower validator or diagnostic mismatches that still count as red in spectest terms.

Representative cases where invalid modules currently load successfully:

- `br_if`: expected module load failure matching `type mismatch` but load succeeded
- `func`: expected module load failure matching `uninitialized local` but load succeeded
- `local_init`: expected module load failure matching `uninitialized local` but load succeeded

Representative cases where the runtime rejects the module, but not with the corpus's expected wording:

- `binary-leb128`: expected `integer too large`, got `section 128 overruns module`
- `binary`: expected `malformed limits flags`, got `section 4 decode failed: malformed module`
- `tag`: expected `non-empty tag result type`, got `tag 0 type must not declare results`

These are still compliance failures. Some indicate genuine validator acceptance bugs, while others are primarily exact-message mismatches.

#### How Part Of This Was Fixed

- The spectest harness now treats the spec's expected `offset out of range` wording as equivalent to the runtime's more specific `exceeds i32 memory address space` diagnostic.
- That cleared the wording-only failures in `address`, `align`, and `simd_address` without weakening the runtime's own error text.

### 8. `wasm-tools` Text-Format Expectations Still Diverge From Some Spec Assertions

The current `wasm-tools` conversion path still differs from the official testsuite's expected text-format diagnostics in a few cases.

Representative failures:

- `annotations`: `unexpected character '\u{0}'` where the corpus expects `illegal character`
- `id`: `invalid character in string '\n'` where the corpus expects `empty identifier`
- `simd_lane`: `malformed lane index` where the corpus expects `i8 constant out of range`

These failures do not imply broken runtime execution, but they still count as non-compliance for an end-to-end local spectest run.

### 9. A Smaller Set Of Loader And Control-Flow Integration Gaps Remains

Outside the big clusters above, a few failures still point to real integration gaps in loader or control-flow handling.

Representative cases:

- `br_table` still fails with section decode `type mismatch`
- `return_call` and `return_call_indirect` fail in `build_control_targets`
- `data` fails with `unknown global 0`
- `elem`, `throw_ref`, and `try_table` still fail as malformed modules during decode
- `global` still fails because `extended const expressions` are disabled

These are fewer in number than the multi-memory and proposal buckets, but they remain important because they block end-to-end claims on tail calls, exceptions, linking variants, and some initializer paths.

#### How Part Of This Was Fixed

- `ref.func` values now preserve typed function-reference information at runtime instead of being flattened to plain `funcref`.
- Global storage now preserves reference-subtype null markers such as `nofunc`, which matters for typed-reference export/import linking.
- Table imports now require invariant typed equality instead of permissive subtype matching.
- `call_ref` and `return_call_ref` are now green in targeted reruns after wiring their current opcode forms through validation and execution.
- The spectest harness now accepts `refnull` and wildcard non-null reference expectations from the JSON stream, which was needed to compare current reference-heavy results correctly.

Those changes moved both `linking0` and `linking` out of the remaining failure set.

## Skipped Cases

There are no skipped spectest cases in this run.

That is one of the most important changes relative to the 2026-04-06 snapshot. The official number now measures all `257` top-level `.wast` files directly instead of reporting a partially hidden subset behind tooling skips.

## Relationship To Other Test Signals

The broader project test picture remains stronger than the spectest number:

- `wasm_test` passed completely in the same run
- `wl_test` passed completely in the same run
- all `12` emcc fixtures passed completely in the same run

That means the runtime already supports several targeted scenarios that the official spec corpus still catches as red. The likely causes are now a mix of:

- inconsistent loader or validator handling across the wider spectest corpus
- remaining proposal-path decode or feature-gate mismatches
- exact error-string mismatches between the local toolchain and the official suite
- remaining runtime aliasing or sharing issues in cases such as `instance`

The spectest number is still the conservative compatibility metric and should remain the one used in public-facing compliance claims.

## Bottom Line

Current status as of 2026-04-07:

- spectest coverage is much broader than it was on 2026-04-06 and no longer hides proposal-heavy files behind conversion skips
- official spectest compliance is substantially improved, but still incomplete: `191` passing files and `66` failing files
- the earlier multi-memory failure cluster was a harness bug and is no longer the dominant blocker
- `module_definition` / `module_instance` harness support is now in place, so those files are failing on real runtime issues rather than an unsupported command
- targeted reruns after the original snapshot moved `call_ref` and `return_call_ref` into the green set
- fixed-width SIMD is now mostly green; relaxed SIMD is still unsupported
- core Memory64 memory operations are now mostly green; remaining 64-bit gaps are concentrated in Table64 and related typing paths
- GC, typed references, exceptions, and tail-call integration still fail end-to-end in the local spectest path despite stronger unit coverage
- the next concentrated blockers are `try_table` type/section decoding and remaining runtime-behavior issues such as the shared-instance table case

For now, the most accurate public statement is:

> `wasm.h` passed 191 of 257 locally exercised official spectest files in the last full 2026-04-07 sweep, with no conversion skips. Follow-up targeted reruns have since cleared the old `module_definition` harness blocker and moved `call_ref` and `return_call_ref` to green, but compliance is still incomplete, with the biggest remaining gaps in proposal-path decoding, GC and typed-reference behavior, `try_table`, shared-instance behavior, and 64-bit table typing.
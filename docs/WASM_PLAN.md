## Current State Assessment

Your implementation covers the Wasm 1.0 MVP well: all numeric ops, linear memory with bounds checks, tables (funcref), globals, imports/exports, block/loop/if/br control flow, call/call_indirect. The interpreter is a straightforward recursive `wasm__exec` with a global value stack and per-call locals.

Key structural limitations that will affect almost every milestone:

- **Fixed-size type signatures** — `params[16]`, `results[4]`. Multi-value and bulk memory callbacks will blow past `results[4]`. Needs dynamic allocation.
- **Single table, single memory** — hardcoded `mod->table` / `mod->memory` as singular structs, not arrays.
- **No `0xFC` prefix dispatch** — the interpreter's opcode switch has no handler for the multi-byte opcode prefix, which gates bulk memory, nontrapping conversions, and table ops.
- **Block type decoding assumes single-byte** — `(int8_t)wasm__read_u8(r)` for block types. Multi-value blocks use type indices (encoded as signed LEB128), not single-byte valtypes.
- **`wasm_valtype_t` has no `funcref`/`externref`** — only `i32/i64/f32/f64/void`.
- **Element section decoder is MVP-only** — only handles segment kind 0 (active, table 0, i32.const offset). Newer encodings (passive, declarative, with elemkind/reftype) will fail.
- **Data section decoder is MVP-only** — same issue, no passive segments, no `data.drop` tracking.
- **Global init exprs don't support `global.get`** — needed for imported mutable globals used in init expressions.

---

## Implementation Plan

### [DONE] Milestone 1: Foundation Refactoring

**Goal:** Remove structural ceilings that block everything else.

- Replace `params[16]`/`results[4]` fixed arrays in `wasm_functype_t` with heap-allocated `wasm_valtype_t*` + count. Audit all callers (the `ca[16]`/`cr[4]` locals in call/call_indirect too — those become dynamically sized or at least bumped significantly).
- Add `0xFC` prefix dispatch in the interpreter's main opcode switch — read the second byte as a u32 LEB128, sub-dispatch from there.
- Refactor block type parsing: detect `0x40` (void), `0x7F-0x7C` (single valtype), or otherwise read a signed LEB128 type index into `mod->types[]`. Propagate the resulting param/result counts into `wasm__label_t.arity` (which currently only tracks result arity — multi-value blocks need input arity tracking too).
- Extend `wasm_valtype_t` enum with `WASM_TYPE_FUNCREF = 0x70` and `WASM_TYPE_EXTERNREF = 0x6F`.

### [DONE] Milestone 2: Sign-Extension Operators

**Goal:** Handle `0xC0–0xC4` opcodes.

- `i32.extend8_s` (0xC0), `i32.extend16_s` (0xC1), `i64.extend8_s` (0xC2), `i64.extend16_s` (0xC3), `i64.extend32_s` (0xC4).
- Pure interpreter additions — five trivial sign-extension cases in the opcode switch. No structural changes needed. Smallest possible milestone, good validation checkpoint.

### [DONE] Milestone 3: Non-Trapping Float-to-Int Conversions

**Goal:** Handle `0xFC 0x00–0x07` opcodes.

- Eight saturating truncation ops: `i32.trunc_sat_f32_s/u`, `i32.trunc_sat_f64_s/u`, `i64.trunc_sat_f32_s/u`, `i64.trunc_sat_f64_s/u`.
- Requires the `0xFC` prefix dispatch from M1. Each op clamps NaN→0 and out-of-range values to min/max instead of trapping. Straightforward but the edge cases (NaN, ±inf, values between INT_MAX and INT_MAX+1) need careful handling — the existing trapping conversions at `0xA8–0xB1` don't validate these at all, which is itself a spec conformance bug you'll want to fix in parallel.

### [DONE] Milestone 4: Multi-Value

**Goal:** Blocks, if/else, and functions can consume and produce multiple values.

- This is mostly a consequence of M1's block type refactoring. The interpreter loop's `wasm__label_t` already tracks `arity` and `sp_base` — extend it to also track `param_arity` so that branching to a block correctly preserves the right number of stack values.
- `wasm__do_branch` needs to shuffle N values (not just result arity — loop branches use param arity). The current implementation copies up to 4 values — make this dynamic.
- `0x0B` (end) needs to validate/shuffle multi-value results when closing a block.
- Function return (`0x0F`) already handles `num_results` but the result-popping loop needs auditing for correctness with >4 results.

### [DONE] Milestone 5: Mutable Globals Import/Export

**Goal:** Imported and exported globals, including mutable ones.

- Extend the import section decoder (kind `0x03`) to actually resolve imported globals against `rt->imports` — currently it reads and discards the bytes. Add a `wasm_import_t` variant for globals (or a separate `wasm_global_import_t` registry).
- Imported globals need to be prepended to `mod->globals[]` (same pattern as imported functions with `num_func_imports`), so that global indices are correct.
- Global init expressions need to support `global.get` referencing earlier (imported) globals.
- Export of mutable globals is already structurally fine — the export section records the index, and `wasm_global_t` already has `is_mutable`.
- The `__stack_pointer` global is the canonical use case — WASI/Emscripten modules import it as a mutable i32 global.

### [DONE] Milestone 6: Bulk Memory Operations

**Goal:** `memory.copy`, `memory.fill`, `memory.init`, `data.drop`, `table.copy`, `table.init`, `elem.drop`.

- All under the `0xFC` prefix (opcodes 8–17 in the sub-dispatch).
- `memory.copy` (0xFC 0x0A) and `memory.fill` (0xFC 0x0B) are the critical ones — these replace libc `memcpy`/`memset` in toolchain output. Both take three i32 args from the stack + a memory index byte (0x00 for now).
- `memory.init` (0xFC 0x08) copies from a passive data segment into memory. Requires tracking which data segments are passive vs active, and storing their raw bytes. Your current data section decoder immediately copies active segments and discards the bytes — passive segments need to be retained.
- `data.drop` (0xFC 0x09) marks a passive data segment as dropped (freed). Add a `uint8_t* dropped` bitfield or flag array to the module.
- Mirror for tables: `table.init` (0xFC 0x0C), `elem.drop` (0xFC 0x0D), `table.copy` (0xFC 0x0E). Same pattern — passive element segments need retained/droppable storage.
- Refactor the data section decoder to handle the three segment kinds (0=active, 1=passive, 2=active with explicit memory index). Same for element section — the current decoder only handles kind 0.

### [DONE] Milestone 7: Reference Types

**Goal:** `funcref`/`externref` as first-class stack values, extended table operations.

- `wasm_value_t` needs a ref representation. `funcref` is a nullable function index (`UINT32_MAX` = null). `externref` is an opaque host pointer/handle — pick a representation (index into a host-managed handle table, or a `void*` directly in the value union).
- New opcodes: `ref.null` (0xD0), `ref.is_null` (0xD1), `ref.func` (0xD2).
- Table operations under `0xFC` prefix: `table.get` (0x25), `table.set` (0x26) — wait, these are *not* prefixed, they're primary opcodes 0x25/0x26. Add those to the main switch. Then `table.grow` (0xFC 0x0F), `table.size` (0xFC 0x10), `table.fill` (0xFC 0x11) under the prefix.
- Tables become typed (funcref or externref) and there can be multiple. Refactor `wasm_table_t` to store its reftype, and `mod->table` to `mod->tables[]` + `mod->num_tables`.
- The table section and element section decoders need updates for the new element segment encodings (with explicit reftype, expr-based init, etc.).

### [DONE] Milestone 8: Multi-Memory

**Goal:** Multiple linear memories with indexed access.

- Refactor `mod->memory` to `mod->memories[]` + `mod->num_memories`. All memory instructions already read an alignment + offset — the memory index is the trailing byte on `memory.size`/`memory.grow` (currently hardcoded to read and discard `0x00`). For load/store, multi-memory uses the `0xFC` prefix encoding with an explicit memidx.
- Bulk memory ops (`memory.copy`, `memory.init`) also take memory indices.
- `wasm_memory_data()` / `wasm_memory_size()` / `wasm_memory_grow()` public API should take a memory index parameter (breaking API change) or default to memory 0.

### [DONE] Milestone 9: Extended Const Expressions

**Goal:** `global.get`, `i32.add`, `i32.mul`, `i64.add`, `i64.mul` in init expressions.

- Currently global init and data/element segment offsets only handle `i32.const`, `i64.const`, `f32.const`, `f64.const`. The decoder reads one opcode + one immediate + `end`.
- Refactor init expression evaluation into a mini-interpreter that processes a sequence of const instructions until `0x0B`, supporting the extended set. This is a small contained evaluator — no control flow, no memory access, just arithmetic on a mini-stack.
- `global.get` in init exprs may only reference previously defined immutable globals or imported globals.

### [DONE] Milestone 10: Tail Calls (Optional but Growing)

**Goal:** `return_call` (0x12) and `return_call_indirect` (0x13).

- Semantically: resolve the callee, tear down the current frame, and jump to the target without growing the call stack.
- In your recursive interpreter, true TCO requires restructuring `wasm__exec` to use a loop-based dispatch where `return_call` sets up the new function/args and `continue`s instead of recursing. This is a significant refactor — the current recursive design makes this the hardest milestone architecturally.

### [DONE] Milestone 11: Exception Handling (Optional, Increasing Adoption)

**Goal:** `try`, `catch`, `catch_all`, `throw`, `rethrow`, `delegate`.

- Requires a new section (tag section, section ID 13) and a tag index space.
- The interpreter needs an exception propagation mechanism — when a `throw` is executed, unwind the label/call stack looking for a matching `catch`. This interacts with the call stack in non-trivial ways since exceptions cross function boundaries.
- `try` blocks push a special label type that records catch handlers. `catch` handlers are indexed by tag. `catch_all` is the fallback.
- The value stack needs careful management during unwinding — caught exceptions push their payload values.
- This is a substantial feature but increasingly necessary for C++ code compiled with exceptions enabled and Rust with `panic=unwind`.

---

## Recommended Implementation Order

M1 → M2 → M3 → M4 → M5 → M6 → M7 → M9 → M8 → M10 → M11

Rationale: M1 is prerequisite for almost everything. M2/M3 are quick wins that immediately unblock real-world modules. M4/M5 are needed by virtually all `wasi-sdk` output. M6 is the big one that Emscripten defaults to. M7 enables modern table patterns. M9 before M8 because extended consts are simpler and more commonly encountered than multi-memory. M10/M11 are progressive enhancements.

After M7, you'll be able to run ~95% of C/C++/Rust wasm blobs compiled with default flags. M8+ is for the long tail.

# Validation Plan

## [DONE] Validation & Feature Detection Milestone

### Prerequisites
M1–M7 and M9 are complete. The instruction set, type system, and module structure are stable enough to validate against a near-final surface.

### Design Decision: Eager Validation
All validation runs at load time inside `wasm_load`, before the start function executes. If validation fails, `wasm_load` returns `NULL` with a descriptive error. The rationale is simplicity — embedders get a binary contract: if `wasm_load` succeeds, the module is well-formed and every exported function is safe to call. No deferred surprises.

---

### Phase 1: Feature Detection Infrastructure

Define a `uint32_t` bitmask for proposal tracking:

```
WASM_FEATURE_SIGN_EXT        (1 << 0)
WASM_FEATURE_NONTRAPPING_FMA (1 << 1)
WASM_FEATURE_MULTI_VALUE     (1 << 2)
WASM_FEATURE_MUTABLE_GLOBALS (1 << 3)
WASM_FEATURE_BULK_MEMORY     (1 << 4)
WASM_FEATURE_REFERENCE_TYPES (1 << 5)
WASM_FEATURE_MULTI_MEMORY    (1 << 6)
WASM_FEATURE_EXTENDED_CONST  (1 << 7)
WASM_FEATURE_TAIL_CALL       (1 << 8)
WASM_FEATURE_EXCEPTIONS      (1 << 9)
```

Add `uint32_t enabled_features` to `wasm_runtime_t` and `uint32_t required_features` to `wasm_module_t`. Expose a public API: `wasm_enable_feature(rt, flag)`, `wasm_disable_feature(rt, flag)`, `wasm_enable_all_features(rt)`. Default `enabled_features` to all currently implemented proposals so existing users aren't broken.

Instrument existing decode paths with `mod->required_features |= WASM_FEATURE_X` at detection points: `0xC0–0xC4` for sign-ext, `0xFC` sub-opcodes 0–7 for nontrapping conversions, `0xFC` sub-opcodes 8–17 for bulk memory, multi-value block types during block type parsing, `funcref`/`externref` valtype bytes for reference types, `0x25`/`0x26` for table get/set, memory index >0 on any memory op for multi-memory, `global.get`/`i32.add`/`i32.mul` in init expressions for extended const, `0x12`/`0x13` for tail calls, tag section for exceptions.

After all sections are decoded, check `uint32_t unsupported = mod->required_features & ~rt->enabled_features`. If non-zero, set an error naming the first unsupported proposal and return `NULL`. This gives embedders a clean gate before any validation or execution work happens.

### Phase 2: Structural Validation

Runs after all sections are decoded and the feature gate passes, but before memory/table allocation and data/element population. This is a set of checks over the module's decoded metadata — no bytecode interpretation, just index bounds and well-formedness.

**Type section**: all function type entries use valid valtype bytes. Result counts >1 require multi-value to be enabled.

**Function section**: every `type_idx` is within `mod->num_types`.

**Table section**: min ≤ max for all tables. Element type is `funcref` or `externref`. Multiple tables require multi-memory or reference-types feature (the spec gates this on reference-types).

**Memory section**: min ≤ max for all memories. Max ≤ 65536 (4GB limit). Multiple memories require the multi-memory feature.

**Global section**: init expressions only use permitted opcodes. `global.get` in init exprs only references earlier-index imported immutable globals. Extended const ops (`i32.add`, `i32.mul`, etc.) require the extended-const feature.

**Export section**: all export indices point to valid entries of the declared kind. All export names are unique — this requires a duplicate check, either sort-and-scan or a simple O(n²) loop (export counts are typically small enough that this doesn't matter).

**Start section**: the start function index is within `num_funcs`. Its type signature must be `[] -> []` (no params, no results).

**Import section**: imported function type indices are within `num_types`. Imported table/memory limits are valid. Imported global types use valid valtypes.

**Element section**: all segment table indices are within `num_tables`. All function indices within element segments are within `num_funcs`. Passive/declarative segment kinds are only valid with bulk-memory enabled.

**Data section**: all segment memory indices are within `num_memories`. If the data count section (section 12) is present, its declared count must match the actual number of data segments. If any bulk memory ops (`memory.init`, `data.drop`) appear in code, the data count section must be present.

**Limits validation**: active data segment offsets + sizes fit within their target memory's declared minimum size. Active element segment offsets + sizes fit within their target table's declared minimum size.

Each check returns the first error found with a diagnostic message identifying what's wrong: "type index 7 out of range (have 5 types)", "duplicate export name 'malloc'", etc. If any structural check fails, return `NULL` from `wasm_load` — don't proceed to code validation.

### Phase 3: Code Validation

Runs after structural validation passes, after memory/table allocation, and after data/element segment population. This validates every non-imported function body via single-pass abstract type interpretation. The module structure is known to be well-formed at this point, so all index lookups are safe.

**Data structures**:

A type stack — an array of `wasm_valtype_t` tracking what the operand stack looks like at each point. Size it to `WASM_MAX_STACK`. Track the current height.

A control frame stack — one entry per active block/loop/if/function. Each frame records: the block opcode (`0x02`/`0x03`/`0x04` or a synthetic one for the function body), the type signature (param types + result types resolved from the block type), the operand stack height at block entry, an `unreachable` flag, and for if-blocks whether an `else` has been seen.

**Algorithm** — for each function body:

Initialize the type stack with the function's parameter types. Push an implicit control frame for the function body with the function's result types as its signature.

Walk the bytecode sequentially. For each instruction, pop its expected input types from the type stack (checking they match), then push its output types. If a pop would go below the current control frame's entry height and the frame isn't unreachable, that's a validation error. If the frame *is* unreachable, pops below entry height succeed with any type (polymorphic stack).

On `block`/`loop`/`if`: push a new control frame. For multi-value blocks, the block type references a type index — pop the param types from the enclosing stack and note them as the frame's entry types.

On `else`: verify the operand stack matches the if-block's result types, then reset the stack to the block's param types for the else branch. Mark that the else has been seen.

On `end`: verify the operand stack matches the block's result types at exactly the right height. Pop the control frame. Push the result types onto the enclosing stack. For if-blocks without an else clause, the result types must equal the param types (since the "not taken" path is implicitly identity).

On `br`/`br_if`/`br_table`: verify the label depth is within the control frame stack. Check that the branch target's arity types are on top of the stack. For `br` and the matched case of `br_table`, mark the current frame unreachable. For `br_table`, all targets (including the default) must have identical arity and types. Loop targets branch to the loop's *params*, not results. Block/if targets branch to the block's *results*.

On `return`: check the function's result types are on top of the stack, mark unreachable.

On `unreachable`: mark the current frame unreachable.

On `call`: look up the callee's type, pop params, push results.

On `call_indirect`: check that the table index exists, type index is valid, pop i32 (table element index) + param types, push result types.

On `local.get`/`set`/`tee`: check index in range, check/set types appropriately.

On `global.get`/`set`: check index in range, check type, check mutability for set.

On memory loads/stores: check memory index exists, check alignment ≤ natural alignment (alignment is `2^align_value`, so `align_value` must not exceed `log2(natural_size)` — e.g., `i32.load` allows align 0–2 but not 3+). Pop/push the correct types.

On `select`: both operands must be the same type (numeric types for untyped select, any type for typed select with reference types).

On bulk memory ops: check segment indices and memory/table indices in range.

On reference ops: `ref.func` requires the function index to be "declared" (appears in a non-passive element segment, or is referenced by `ref.func` — the spec tracks this).

**Instruction coverage must exactly mirror `wasm__exec`**: every opcode handled in the interpreter needs a validation rule. Every opcode *not* handled (SIMD, future proposals) must be rejected. The feature bitmask from phase 1 determines which opcodes are valid — if bulk-memory isn't enabled, `memory.copy` is an unknown opcode even if you have the interpreter code for it.

**Unreachable code handling**: after `unreachable`, `br`, `return`, or any unconditional branch, set the control frame's unreachable flag. In unreachable state, the type stack can underflow below the frame's entry height — pops return a "bottom" type that matches anything. When a new block starts or `else`/`end` transitions to reachable code, clear the flag and reset the stack appropriately. Getting this wrong is the most common validator bug — the spec test suite's `unreached-invalid.wast` and `unreached-valid.wast` files are specifically designed to catch these edge cases.

**Error reporting**: on failure, report the function index, the byte offset within the function body, and a human-readable description: "validation error: func[12] offset 0x2A: type mismatch: expected [i32 i32], got [i64 i32]". Store this in `rt->error_msg`. One error, then bail — don't attempt to continue validating the same function or move to the next one.

### Integration Into `wasm_load`

The final load sequence becomes:

1. Parse magic + version
2. Decode all sections (accumulating `required_features` along the way)
3. Feature gate check (`required_features & ~enabled_features`)
4. Structural validation (phase 2)
5. Allocate memory and tables
6. Populate data and element segments (second pass)
7. Code validation — loop over all non-imported functions, call `wasm__validate_function` for each (phase 3)
8. Run start function

Any failure at steps 3, 4, or 7 frees the partially constructed module and returns `NULL`. Step 6 must precede step 7 only if your validator needs to check data/element segment contents (it doesn't strictly need to — segment index bounds are checked in phase 2 — but having them populated means the module is fully constructed if validation passes, ready for step 8 with no further work).



# SIMD Implementation Plan

**1. Feature flag and type plumbing**

Add `WASM_FEATURE_SIMD (1u << 10)` to the feature flags. Add `WASM_TYPE_V128 = 0x7B` to `wasm_valtype_t`. Add `uint8_t v128[16]` to the `wasm_value_t` union. Add `wasm_v128` constructor and update `wasm__is_numeric_type`, `wasm__is_value_type`, `wasm__is_valtype_byte`, `wasm__default_value` to handle v128.

**2. Platform detection at top of `WASM_IMPL`**

Detect SSE2, SSE4.1, NEON via preprocessor. Include the relevant headers. Define `WASM__SIMD_SSE2`, `WASM__SIMD_SSE41`, `WASM__SIMD_NEON` flags. No platform types in public headers — only used inside `WASM_IMPL`.

**3. Validator: add `0xFD` case to `wasm__validate_function`**

In the main opcode switch, add:

```c
case 0xFD: {
    uint32_t subop = wasm__read_leb128_u32(&v.r);
    err = wasm__validator_require_feature(&v, at, WASM_FEATURE_SIMD);
    if (err != WASM_OK) return err;
    switch (subop) {
        /* ~240 cases inline */
    }
    break;
}
```

Each case validates operand types and pushes results. Grouped by category:

- **Memory ops (0x00–0x0A, 0x54–0x5D):** pop i32 address, read memarg, push/pop v128. Lane load/stores also pop v128 + i32 and push v128. Validate alignment doesn't exceed natural.
- **v128.const (0x0C):** skip 16 bytes, push v128
- **i8x16.shuffle (0x0D):** read 16 immediate bytes (each must be < 32), pop two v128, push v128
- **Splat ops (0x0F–0x13):** pop the lane type (i32/i32/i32/i64/f32/f64), push v128
- **Extract lane ops (0x15–0x1C):** read lane immediate, bounds check against lane count, pop v128, push lane type
- **Replace lane ops (0x1D–0x20):** read lane immediate, bounds check, pop lane type + v128, push v128
- **Unary v128→v128 (various):** pop v128, push v128. Covers not, abs, neg, all_true, bitmask, ceil, floor, trunc, nearest, sqrt, popcnt, extend, convert, trunc_sat, demote, promote
- **Binary v128×v128→v128 (various):** pop two v128, push v128. Covers eq, ne, lt, gt, le, ge, add, sub, mul, div, min, max, swizzle, and, or, xor, andnot, bitselect, shl, shr, avgr, dot, extmul, narrow, q15mulr, relaxed ops
- **Test v128→i32 (0x53):** v128.any_true, pop v128, push i32
- **Shift ops:** pop i32 shift amount + v128, push v128

**4. Interpreter: add `0xFD` case to `wasm__exec`**

Same structure — nested switch under `0xFD`. Each case has `#if WASM__SIMD_SSE2` / `#elif WASM__SIMD_NEON` / `#else` scalar fallback. Grouped:

- **v128 memory loads (0x00–0x0A):** memarg parsing, bounds check, load from linear memory into v128. Includes plain v128.load, zero-extending loads (load8x8, load16x4, load32x2), splat loads (load8_splat, load16_splat, load32_splat, load64_splat), zero-padded loads (v128.load32_zero, v128.load64_zero).
- **v128 memory stores (0x0B):** v128.store, memarg, bounds check, store 16 bytes.
- **v128.const (0x0C):** read 16 bytes from bytecode into v128, push.
- **i8x16.shuffle (0x0D):** read 16 lane immediates, pop two v128, build result by indexing into the concatenated 32-byte space. SSE path uses `_mm_shuffle_epi8` (needs SSSE3). NEON uses `vtbl`. Scalar loops over 16 bytes.
- **Lane loads (0x54–0x5D):** v128.loadN_lane / v128.storeN_lane. Pop v128 + i32 addr, load/store specific lane width, push v128.
- **i8x16.swizzle (0x0E):** pop two v128, index into first using second as indices (out of range → 0).
- **Splats (0x0F–0x13):** pop scalar, replicate across lanes. Scalar fallback: store to each lane position. SSE: `_mm_set1_epiN` / `_mm_set1_ps` / `_mm_set1_pd`.
- **Extract lane (0x15–0x1C):** pop v128, read lane immediate, extract and push as scalar. SSE: `_mm_extract_epi8` etc (SSE4.1 for some).
- **Replace lane (0x1D–0x20):** pop scalar + v128, read lane immediate, replace lane, push v128.
- **Comparisons (eq/ne/lt/gt/le/ge per shape):** pop two v128, compare per-lane, result is all-ones or all-zeros per lane, push v128.
- **Arithmetic (add/sub/mul/div/min/max per shape):** pop two v128, operate per-lane, push v128. Integer mul only exists for i16x8, i32x4, i64x2. Division only for f32x4, f64x2.
- **Saturating arithmetic (add_sat/sub_sat):** i8x16 and i16x8 signed/unsigned variants.
- **Shifts (shl/shr_s/shr_u per integer shape):** pop i32 shift amount, pop v128, shift each lane, push v128. Shift amount masked to lane width.
- **Bitwise (and/or/xor/andnot/not/bitselect):** v128×v128→v128 or v128→v128. Bitselect takes three v128.
- **all_true/any_true/bitmask:** v128→i32. Test or extract mask bits.
- **Conversions:** extend, narrow, convert between lane types. Widening ops take a v128 and produce a v128 with wider lanes from the low/high half. Narrowing ops take two v128 and pack into one.
- **Float ops:** abs, neg, sqrt, ceil, floor, trunc, nearest — per-lane for f32x4 and f64x2.
- **Dot product:** i16x8.dot_i8x16_i7x16_s and i32x4.dot_i8x16_i7x16_add_s if doing relaxed SIMD too.
- **Extended multiply:** i16x8.extmul_low/high_i8x16_s/u etc. Widen half the lanes and multiply.
- **avgr:** i8x16 and i16x8 unsigned rounding average.

**5. Scalar fallback patterns**

Every op gets a scalar fallback that operates on `uint8_t[16]` using existing `wasm__load_le16/32/64` and `wasm__store_le16/32/64`. Pattern for a binary i32x4 op:

```c
uint32_t lane;
for (lane = 0; lane < 4; lane++) {
    uint32_t la = wasm__load_le32(a + lane * 4);
    uint32_t lb = wasm__load_le32(b + lane * 4);
    wasm__store_le32(out + lane * 4, la + lb);
}
```

This keeps big-endian correctness for free.

**6. NaN handling for float SIMD ops**

f32x4 and f64x2 min/max need the same NaN propagation semantics as your existing `wasm__f32_min`/`wasm__f64_max`. Apply per-lane in the scalar path. The intrinsic paths need care too — SSE `_mm_min_ps` doesn't match wasm NaN semantics exactly, so those cases need a fixup or fallback to scalar per-lane.

**7. Update `wasm__feature_name`**

Add `WASM_FEATURE_SIMD` → `"SIMD"`.

**8. Update `WASM__IMPLEMENTED_FEATURES`**

Add `WASM_FEATURE_SIMD` to the bitmask.

**9. Update type section decoder**

`0x7B` is already rejected by `wasm__is_value_type`. After the plumbing in step 1 it just works, but gate it behind `wasm__require_feature(mod, WASM_FEATURE_SIMD)` when encountered in type/global/import sections.

**10. Update header comment**

Add SIMD to the features list, note the platform-specific acceleration.

**What's NOT included:**
- Relaxed SIMD (separate proposal, separate feature flag, can layer on later)
- SIMD in extended const expressions (not part of the proposal)

# WebAssembly SIMD Opcodes (0xFD prefix)

Current status: scalar SIMD semantics are implemented inline in `wasm__exec` and covered in `wasm_test.c`. Platform-specific acceleration remains future work.

## Memory Operations

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x00 | v128.load | memarg; i32 addr | v128 | [x] |
| 0x01 | v128.load8x8_s | memarg; i32 addr | v128 | [x] |
| 0x02 | v128.load8x8_u | memarg; i32 addr | v128 | [x] |
| 0x03 | v128.load16x4_s | memarg; i32 addr | v128 | [x] |
| 0x04 | v128.load16x4_u | memarg; i32 addr | v128 | [x] |
| 0x05 | v128.load32x2_s | memarg; i32 addr | v128 | [x] |
| 0x06 | v128.load32x2_u | memarg; i32 addr | v128 | [x] |
| 0x07 | v128.load8_splat | memarg; i32 addr | v128 | [x] |
| 0x08 | v128.load16_splat | memarg; i32 addr | v128 | [x] |
| 0x09 | v128.load32_splat | memarg; i32 addr | v128 | [x] |
| 0x0A | v128.load64_splat | memarg; i32 addr | v128 | [x] |
| 0x0B | v128.store | memarg; i32 addr, v128 value | - | [x] |
| 0x5C | v128.load32_zero | memarg; i32 addr | v128 | [x] |
| 0x5D | v128.load64_zero | memarg; i32 addr | v128 | [x] |
| 0x54 | v128.load8_lane | memarg, lane; i32 addr, v128 | v128 | [x] |
| 0x55 | v128.load16_lane | memarg, lane; i32 addr, v128 | v128 | [x] |
| 0x56 | v128.load32_lane | memarg, lane; i32 addr, v128 | v128 | [x] |
| 0x57 | v128.load64_lane | memarg, lane; i32 addr, v128 | v128 | [x] |
| 0x58 | v128.store8_lane | memarg, lane; i32 addr, v128 | - | [x] |
| 0x59 | v128.store16_lane | memarg, lane; i32 addr, v128 | - | [x] |
| 0x5A | v128.store32_lane | memarg, lane; i32 addr, v128 | - | [x] |
| 0x5B | v128.store64_lane | memarg, lane; i32 addr, v128 | - | [x] |

## Constant and Shuffle

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x0C | v128.const | 16 imm bytes | v128 | [x] |
| 0x0D | i8x16.shuffle | 16 imm lanes; v128, v128 | v128 | [x] |

## Swizzle and Splat

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x0E | i8x16.swizzle | v128, v128 | v128 | [x] |
| 0x0F | i8x16.splat | i32 | v128 | [x] |
| 0x10 | i16x8.splat | i32 | v128 | [x] |
| 0x11 | i32x4.splat | i32 | v128 | [x] |
| 0x12 | i64x2.splat | i64 | v128 | [x] |
| 0x13 | f32x4.splat | f32 | v128 | [x] |
| 0x14 | f64x2.splat | f64 | v128 | [x] |

## Lane Extract

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x15 | i8x16.extract_lane_s | lane; v128 | i32 | [x] |
| 0x16 | i8x16.extract_lane_u | lane; v128 | i32 | [x] |
| 0x17 | i8x16.replace_lane | lane; v128, i32 | v128 | [x] |
| 0x18 | i16x8.extract_lane_s | lane; v128 | i32 | [x] |
| 0x19 | i16x8.extract_lane_u | lane; v128 | i32 | [x] |
| 0x1A | i16x8.replace_lane | lane; v128, i32 | v128 | [x] |
| 0x1B | i32x4.extract_lane | lane; v128 | i32 | [x] |
| 0x1C | i32x4.replace_lane | lane; v128, i32 | v128 | [x] |
| 0x1D | i64x2.extract_lane | lane; v128 | i64 | [x] |
| 0x1E | i64x2.replace_lane | lane; v128, i64 | v128 | [x] |
| 0x1F | f32x4.extract_lane | lane; v128 | f32 | [x] |
| 0x20 | f32x4.replace_lane | lane; v128, f32 | v128 | [x] |
| 0x21 | f64x2.extract_lane | lane; v128 | f64 | [x] |
| 0x22 | f64x2.replace_lane | lane; v128, f64 | v128 | [x] |

## i8x16 Comparisons

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x23 | i8x16.eq | v128, v128 | v128 | [x] |
| 0x24 | i8x16.ne | v128, v128 | v128 | [x] |
| 0x25 | i8x16.lt_s | v128, v128 | v128 | [x] |
| 0x26 | i8x16.lt_u | v128, v128 | v128 | [x] |
| 0x27 | i8x16.gt_s | v128, v128 | v128 | [x] |
| 0x28 | i8x16.gt_u | v128, v128 | v128 | [x] |
| 0x29 | i8x16.le_s | v128, v128 | v128 | [x] |
| 0x2A | i8x16.le_u | v128, v128 | v128 | [x] |
| 0x2B | i8x16.ge_s | v128, v128 | v128 | [x] |
| 0x2C | i8x16.ge_u | v128, v128 | v128 | [x] |

## i16x8 Comparisons

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x2D | i16x8.eq | v128, v128 | v128 | [x] |
| 0x2E | i16x8.ne | v128, v128 | v128 | [x] |
| 0x2F | i16x8.lt_s | v128, v128 | v128 | [x] |
| 0x30 | i16x8.lt_u | v128, v128 | v128 | [x] |
| 0x31 | i16x8.gt_s | v128, v128 | v128 | [x] |
| 0x32 | i16x8.gt_u | v128, v128 | v128 | [x] |
| 0x33 | i16x8.le_s | v128, v128 | v128 | [x] |
| 0x34 | i16x8.le_u | v128, v128 | v128 | [x] |
| 0x35 | i16x8.ge_s | v128, v128 | v128 | [x] |
| 0x36 | i16x8.ge_u | v128, v128 | v128 | [x] |

## i32x4 Comparisons

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x37 | i32x4.eq | v128, v128 | v128 | [x] |
| 0x38 | i32x4.ne | v128, v128 | v128 | [x] |
| 0x39 | i32x4.lt_s | v128, v128 | v128 | [x] |
| 0x3A | i32x4.lt_u | v128, v128 | v128 | [x] |
| 0x3B | i32x4.gt_s | v128, v128 | v128 | [x] |
| 0x3C | i32x4.gt_u | v128, v128 | v128 | [x] |
| 0x3D | i32x4.le_s | v128, v128 | v128 | [x] |
| 0x3E | i32x4.le_u | v128, v128 | v128 | [x] |
| 0x3F | i32x4.ge_s | v128, v128 | v128 | [x] |
| 0x40 | i32x4.ge_u | v128, v128 | v128 | [x] |

## i64x2 Comparisons

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0xD6 | i64x2.eq | v128, v128 | v128 | [x] |
| 0xD7 | i64x2.ne | v128, v128 | v128 | [x] |
| 0xD8 | i64x2.lt_s | v128, v128 | v128 | [x] |
| 0xD9 | i64x2.gt_s | v128, v128 | v128 | [x] |
| 0xDA | i64x2.le_s | v128, v128 | v128 | [x] |
| 0xDB | i64x2.ge_s | v128, v128 | v128 | [x] |

## f32x4 Comparisons

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x41 | f32x4.eq | v128, v128 | v128 | [x] |
| 0x42 | f32x4.ne | v128, v128 | v128 | [x] |
| 0x43 | f32x4.lt | v128, v128 | v128 | [x] |
| 0x44 | f32x4.gt | v128, v128 | v128 | [x] |
| 0x45 | f32x4.le | v128, v128 | v128 | [x] |
| 0x46 | f32x4.ge | v128, v128 | v128 | [x] |

## f64x2 Comparisons

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x47 | f64x2.eq | v128, v128 | v128 | [x] |
| 0x48 | f64x2.ne | v128, v128 | v128 | [x] |
| 0x49 | f64x2.lt | v128, v128 | v128 | [x] |
| 0x4A | f64x2.gt | v128, v128 | v128 | [x] |
| 0x4B | f64x2.le | v128, v128 | v128 | [x] |
| 0x4C | f64x2.ge | v128, v128 | v128 | [x] |

## v128 Bitwise

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x4D | v128.not | v128 | v128 | [x] |
| 0x4E | v128.and | v128, v128 | v128 | [x] |
| 0x4F | v128.andnot | v128, v128 | v128 | [x] |
| 0x50 | v128.or | v128, v128 | v128 | [x] |
| 0x51 | v128.xor | v128, v128 | v128 | [x] |
| 0x52 | v128.bitselect | v128, v128, v128 | v128 | [x] |
| 0x53 | v128.any_true | v128 | i32 | [x] |

## i8x16 Operations

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x60 | i8x16.abs | v128 | v128 | [x] |
| 0x61 | i8x16.neg | v128 | v128 | [x] |
| 0x62 | i8x16.popcnt | v128 | v128 | [x] |
| 0x63 | i8x16.all_true | v128 | i32 | [x] |
| 0x64 | i8x16.bitmask | v128 | i32 | [x] |
| 0x65 | i8x16.narrow_i16x8_s | v128, v128 | v128 | [x] |
| 0x66 | i8x16.narrow_i16x8_u | v128, v128 | v128 | [x] |
| 0x6B | i8x16.shl | v128, i32 | v128 | [x] |
| 0x6C | i8x16.shr_s | v128, i32 | v128 | [x] |
| 0x6D | i8x16.shr_u | v128, i32 | v128 | [x] |
| 0x6E | i8x16.add | v128, v128 | v128 | [x] |
| 0x6F | i8x16.add_sat_s | v128, v128 | v128 | [x] |
| 0x70 | i8x16.add_sat_u | v128, v128 | v128 | [x] |
| 0x71 | i8x16.sub | v128, v128 | v128 | [x] |
| 0x72 | i8x16.sub_sat_s | v128, v128 | v128 | [x] |
| 0x73 | i8x16.sub_sat_u | v128, v128 | v128 | [x] |
| 0x76 | i8x16.min_s | v128, v128 | v128 | [x] |
| 0x77 | i8x16.min_u | v128, v128 | v128 | [x] |
| 0x78 | i8x16.max_s | v128, v128 | v128 | [x] |
| 0x79 | i8x16.max_u | v128, v128 | v128 | [x] |
| 0x7B | i8x16.avgr_u | v128, v128 | v128 | [x] |

## i16x8 Operations

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x80 | i16x8.abs | v128 | v128 | [x] |
| 0x81 | i16x8.neg | v128 | v128 | [x] |
| 0x82 | i16x8.q15mulr_sat_s | v128, v128 | v128 | [x] |
| 0x83 | i16x8.all_true | v128 | i32 | [x] |
| 0x84 | i16x8.bitmask | v128 | i32 | [x] |
| 0x85 | i16x8.narrow_i32x4_s | v128, v128 | v128 | [x] |
| 0x86 | i16x8.narrow_i32x4_u | v128, v128 | v128 | [x] |
| 0x87 | i16x8.extend_low_i8x16_s | v128 | v128 | [x] |
| 0x88 | i16x8.extend_high_i8x16_s | v128 | v128 | [x] |
| 0x89 | i16x8.extend_low_i8x16_u | v128 | v128 | [x] |
| 0x8A | i16x8.extend_high_i8x16_u | v128 | v128 | [x] |
| 0x8B | i16x8.shl | v128, i32 | v128 | [x] |
| 0x8C | i16x8.shr_s | v128, i32 | v128 | [x] |
| 0x8D | i16x8.shr_u | v128, i32 | v128 | [x] |
| 0x8E | i16x8.add | v128, v128 | v128 | [x] |
| 0x8F | i16x8.add_sat_s | v128, v128 | v128 | [x] |
| 0x90 | i16x8.add_sat_u | v128, v128 | v128 | [x] |
| 0x91 | i16x8.sub | v128, v128 | v128 | [x] |
| 0x92 | i16x8.sub_sat_s | v128, v128 | v128 | [x] |
| 0x93 | i16x8.sub_sat_u | v128, v128 | v128 | [x] |
| 0x95 | i16x8.mul | v128, v128 | v128 | [x] |
| 0x96 | i16x8.min_s | v128, v128 | v128 | [x] |
| 0x97 | i16x8.min_u | v128, v128 | v128 | [x] |
| 0x98 | i16x8.max_s | v128, v128 | v128 | [x] |
| 0x99 | i16x8.max_u | v128, v128 | v128 | [x] |
| 0x9B | i16x8.avgr_u | v128, v128 | v128 | [x] |
| 0x9C | i16x8.extmul_low_i8x16_s | v128, v128 | v128 | [x] |
| 0x9D | i16x8.extmul_high_i8x16_s | v128, v128 | v128 | [x] |
| 0x9E | i16x8.extmul_low_i8x16_u | v128, v128 | v128 | [x] |
| 0x9F | i16x8.extmul_high_i8x16_u | v128, v128 | v128 | [x] |

## i32x4 Operations

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0xA0 | i32x4.abs | v128 | v128 | [x] |
| 0xA1 | i32x4.neg | v128 | v128 | [x] |
| 0xA3 | i32x4.all_true | v128 | i32 | [x] |
| 0xA4 | i32x4.bitmask | v128 | i32 | [x] |
| 0xA7 | i32x4.extend_low_i16x8_s | v128 | v128 | [x] |
| 0xA8 | i32x4.extend_high_i16x8_s | v128 | v128 | [x] |
| 0xA9 | i32x4.extend_low_i16x8_u | v128 | v128 | [x] |
| 0xAA | i32x4.extend_high_i16x8_u | v128 | v128 | [x] |
| 0xAB | i32x4.shl | v128, i32 | v128 | [x] |
| 0xAC | i32x4.shr_s | v128, i32 | v128 | [x] |
| 0xAD | i32x4.shr_u | v128, i32 | v128 | [x] |
| 0xAE | i32x4.add | v128, v128 | v128 | [x] |
| 0xB1 | i32x4.sub | v128, v128 | v128 | [x] |
| 0xB5 | i32x4.mul | v128, v128 | v128 | [x] |
| 0xB6 | i32x4.min_s | v128, v128 | v128 | [x] |
| 0xB7 | i32x4.min_u | v128, v128 | v128 | [x] |
| 0xB8 | i32x4.max_s | v128, v128 | v128 | [x] |
| 0xB9 | i32x4.max_u | v128, v128 | v128 | [x] |
| 0xBA | i32x4.dot_i16x8_s | v128, v128 | v128 | [x] |
| 0xBC | i32x4.extmul_low_i16x8_s | v128, v128 | v128 | [x] |
| 0xBD | i32x4.extmul_high_i16x8_s | v128, v128 | v128 | [x] |
| 0xBE | i32x4.extmul_low_i16x8_u | v128, v128 | v128 | [x] |
| 0xBF | i32x4.extmul_high_i16x8_u | v128, v128 | v128 | [x] |

## i64x2 Operations

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0xC0 | i64x2.abs | v128 | v128 | [x] |
| 0xC1 | i64x2.neg | v128 | v128 | [x] |
| 0xC3 | i64x2.all_true | v128 | i32 | [x] |
| 0xC4 | i64x2.bitmask | v128 | i32 | [x] |
| 0xC7 | i64x2.extend_low_i32x4_s | v128 | v128 | [x] |
| 0xC8 | i64x2.extend_high_i32x4_s | v128 | v128 | [x] |
| 0xC9 | i64x2.extend_low_i32x4_u | v128 | v128 | [x] |
| 0xCA | i64x2.extend_high_i32x4_u | v128 | v128 | [x] |
| 0xCB | i64x2.shl | v128, i32 | v128 | [x] |
| 0xCC | i64x2.shr_s | v128, i32 | v128 | [x] |
| 0xCD | i64x2.shr_u | v128, i32 | v128 | [x] |
| 0xCE | i64x2.add | v128, v128 | v128 | [x] |
| 0xD1 | i64x2.sub | v128, v128 | v128 | [x] |
| 0xD5 | i64x2.mul | v128, v128 | v128 | [x] |
| 0xDC | i64x2.extmul_low_i32x4_s | v128, v128 | v128 | [x] |
| 0xDD | i64x2.extmul_high_i32x4_s | v128, v128 | v128 | [x] |
| 0xDE | i64x2.extmul_low_i32x4_u | v128, v128 | v128 | [x] |
| 0xDF | i64x2.extmul_high_i32x4_u | v128, v128 | v128 | [x] |

## f32x4 Operations

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x67 | f32x4.ceil | v128 | v128 | [x] |
| 0x68 | f32x4.floor | v128 | v128 | [x] |
| 0x69 | f32x4.trunc | v128 | v128 | [x] |
| 0x6A | f32x4.nearest | v128 | v128 | [x] |
| 0xE0 | f32x4.abs | v128 | v128 | [x] |
| 0xE1 | f32x4.neg | v128 | v128 | [x] |
| 0xE3 | f32x4.sqrt | v128 | v128 | [x] |
| 0xE4 | f32x4.add | v128, v128 | v128 | [x] |
| 0xE5 | f32x4.sub | v128, v128 | v128 | [x] |
| 0xE6 | f32x4.mul | v128, v128 | v128 | [x] |
| 0xE7 | f32x4.div | v128, v128 | v128 | [x] |
| 0xE8 | f32x4.min | v128, v128 | v128 | [x] |
| 0xE9 | f32x4.max | v128, v128 | v128 | [x] |
| 0xEA | f32x4.pmin | v128, v128 | v128 | [x] |
| 0xEB | f32x4.pmax | v128, v128 | v128 | [x] |

## f64x2 Operations

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x74 | f64x2.ceil | v128 | v128 | [x] |
| 0x75 | f64x2.floor | v128 | v128 | [x] |
| 0x7A | f64x2.trunc | v128 | v128 | [x] |
| 0x94 | f64x2.nearest | v128 | v128 | [x] |
| 0xEC | f64x2.abs | v128 | v128 | [x] |
| 0xED | f64x2.neg | v128 | v128 | [x] |
| 0xEF | f64x2.sqrt | v128 | v128 | [x] |
| 0xF0 | f64x2.add | v128, v128 | v128 | [x] |
| 0xF1 | f64x2.sub | v128, v128 | v128 | [x] |
| 0xF2 | f64x2.mul | v128, v128 | v128 | [x] |
| 0xF3 | f64x2.div | v128, v128 | v128 | [x] |
| 0xF4 | f64x2.min | v128, v128 | v128 | [x] |
| 0xF5 | f64x2.max | v128, v128 | v128 | [x] |
| 0xF6 | f64x2.pmin | v128, v128 | v128 | [x] |
| 0xF7 | f64x2.pmax | v128, v128 | v128 | [x] |

## Conversions

| Opcode | Name | Operands | Result | Done |
|--------|------|----------|--------|------|
| 0x5E | f32x4.demote_f64x2_zero | v128 | v128 | [x] |
| 0x5F | f64x2.promote_low_f32x4 | v128 | v128 | [x] |
| 0x7C | i16x8.extadd_pairwise_i8x16_s | v128 | v128 | [x] |
| 0x7D | i16x8.extadd_pairwise_i8x16_u | v128 | v128 | [x] |
| 0x7E | i32x4.extadd_pairwise_i16x8_s | v128 | v128 | [x] |
| 0x7F | i32x4.extadd_pairwise_i16x8_u | v128 | v128 | [x] |
| 0xF8 | i32x4.trunc_sat_f32x4_s | v128 | v128 | [x] |
| 0xF9 | i32x4.trunc_sat_f32x4_u | v128 | v128 | [x] |
| 0xFA | f32x4.convert_i32x4_s | v128 | v128 | [x] |
| 0xFB | f32x4.convert_i32x4_u | v128 | v128 | [x] |
| 0xFC | i32x4.trunc_sat_f64x2_s_zero | v128 | v128 | [x] |
| 0xFD | i32x4.trunc_sat_f64x2_u_zero | v128 | v128 | [x] |
| 0xFE | f64x2.convert_low_i32x4_s | v128 | v128 | [x] |
| 0xFF | f64x2.convert_low_i32x4_u | v128 | v128 | [x] |

# `wasm__exec` refactor

---

## Phase 1: Data structure foundation

**Goal:** Define the explicit frame stack and arena allocator without changing execution yet.

**New types:**

```c
typedef struct wasm__call_frame_t {
    uint32_t        func_idx;
    uint32_t        sp_base;       // operand stack base for this frame
    wasm_value_t   *locals;        // arena pointer
    wasm__label_t  *labels;        // arena pointer
    uint32_t        lsp;           // label stack pointer
    uint32_t        max_labels;    // precomputed during validation
    wasm__reader_t  r;             // bytecode cursor
    wasm_func_t    *func;          // cached pointer
    wasm_functype_t *ft;           // cached pointer
    wasm_value_t   *results_dst;   // where to write results on return
    uint32_t        num_results;   // how many results caller expects
    size_t          arena_saved;   // arena watermark for dealloc on pop
} wasm__call_frame_t;
```

**Arena allocator on `wasm_runtime_t`:**

```c
struct wasm_runtime_t {
    // ... existing fields ...
    uint8_t  *frame_arena;
    size_t    frame_arena_size;
    size_t    frame_arena_offset;
};
```

Add these functions:

- `wasm__arena_init(rt, size)` — called from `wasm_init`, default size derived from `WASM_MAX_CALL_DEPTH * estimated_frame_size`
- `wasm__arena_alloc(rt, size)` — bump allocator, returns `NULL` on exhaustion
- `wasm__arena_reset(rt, saved_offset)` — rewind to watermark
- `wasm__arena_destroy(rt)` — called from `wasm_destroy`

**Precompute per-function metadata during validation:**

Add to `wasm_func_t`:

```c
uint32_t max_label_depth;  // max nesting depth seen during validation
```

Populate `max_label_depth` inside `wasm__validate_function` — you're already tracking `v.fp`, just record the high-water mark. This lets you arena-allocate exactly the right label array size per frame.

**Testing checkpoint:** Everything compiles, existing `wasm__exec` unchanged. Write a unit test that exercises the arena allocator standalone.

---

## Phase 2: Extract the interpreter loop body

**Goal:** Separate opcode dispatch from call frame management so the rewrite is surgical.

**Refactor `wasm__exec` into two layers:**

1. `wasm__interp_loop(mod, frames, frame_sp)` — the outer frame-aware loop
2. The opcode switch stays inline but all references to locals/labels/reader go through `cf->` instead of bare local variables

**Before touching control flow**, do a mechanical transformation of the current `wasm__exec`:

- Replace `locals[x]` → `cf->locals[x]`
- Replace `labels[x]` → `cf->labels[x]`
- Replace `lsp` → `cf->lsp`
- Replace `r` → `cf->r`
- Replace `sp_base` → `cf->sp_base`
- Replace `func` → `cf->func`
- Replace `ft` → `cf->ft`

This is a large but low-risk find-and-replace. The function signature changes but behavior is identical — still recursive at this point.

**Testing checkpoint:** All existing tests pass. Behavior identical, just accessing state through the frame struct.

---

## Phase 3: Replace recursion with frame stack push/pop

**Goal:** The core change. `call`, `call_indirect`, `return`, and end-of-function all manipulate the frame stack instead of making C-level recursive calls.

**Frame stack location:** Array on `wasm_runtime_t`:

```c
wasm__call_frame_t *call_frames;  // heap-allocated, size WASM_MAX_CALL_DEPTH
uint32_t            call_frame_sp;
```

Allocated in `wasm_init`, freed in `wasm_destroy`.

**New interpreter entry point:**

```c
static wasm_error_t wasm__interp(wasm_module_t *mod,
                                 uint32_t func_idx,
                                 wasm_value_t *args, uint32_t num_args,
                                 wasm_value_t *results, uint32_t num_results)
```

This is non-recursive. The main loop:

```
save arena watermark
push initial frame
loop:
    cf = &frames[frame_sp]
    read opcode
    switch(op):
        ... (most opcodes unchanged) ...
        
        case CALL:        goto handle_call
        case CALL_IND:    goto handle_call_indirect
        case RETURN:      goto handle_return
        case END:         if (last label) goto handle_return
                          else pop label normally
        case RETURN_CALL: goto handle_tail_call
        case RETURN_CALL_IND: goto handle_tail_call_indirect
        case THROW:       goto handle_throw
```

**`handle_call` pseudocode:**

```
1. Read callee index
2. Resolve callee func + functype
3. If host function:
     - pop args from operand stack into temp buffer
     - call host_func directly (this is the ONLY remaining C call)
     - push results onto operand stack
     - continue loop
4. If wasm function:
     - check frame_sp < WASM_MAX_CALL_DEPTH
     - save arena watermark on current frame
     - pop args from operand stack into temp buffer
     - frame_sp++
     - cf = &frames[frame_sp]
     - arena-allocate locals + labels for new frame
     - initialize cf (func, ft, reader, sp_base, copy args into locals)
     - push implicit function-level label
     - continue loop
```

**`handle_return` pseudocode:**

```
1. Copy results from operand stack to cf->results_dst
     (or directly into parent frame's operand stack)
2. Reset arena to cf->arena_saved
3. frame_sp--
4. If frame_sp underflows: we're done, write to top-level results, break
5. cf = &frames[frame_sp]
6. Push returned values onto operand stack
7. continue loop
```

**`handle_tail_call` pseudocode:**

```
1. Pop args from operand stack
2. Reset arena to cf->arena_saved  (free current frame's locals/labels)
3. Overwrite current frame with new callee
4. Re-allocate locals/labels from arena for new function
5. continue loop
```

This is where the tail call design gets cleaner — no `restart` flag, no `goto`, just overwrite the frame in place.

**Host function calls remain synchronous C calls.** They don't push a frame. This is correct because host functions can't be re-entered or suspended — they run to completion.

**Testing checkpoint:** All non-exception tests pass. Call, call_indirect, return, tail calls all work through the frame stack.

---

## Phase 4: Exception handling migration

**Goal:** Port throw/catch/rethrow/delegate to the frame stack model.

This is the trickiest part because exceptions unwind across frame boundaries.

**`handle_throw` pseudocode:**

```
1. Pop tag payload from operand stack
2. Set pending exception on runtime
3. Walk frames from frame_sp downward:
     For each frame:
       Walk labels from cf->lsp downward:
         If label is try block with matching catch:
           - unwind to that frame (frame_sp = target)
           - reset arena for discarded frames
           - reset operand stack
           - enter catch handler
           - clear pending exception
           - continue main loop
         If label is try block with delegate:
           - continue unwinding from delegate target depth
     If no handler found in frame:
       - pop frame, continue to next
4. If no handler found anywhere:
     - return uncaught exception error
```

The key difference from the current recursive model: unwinding doesn't rely on C stack unwinding. You just decrement `frame_sp` and reset arena watermarks. The `wasm__handle_exception` function needs to take the frame stack as context instead of only seeing labels within a single recursive call.

**Rethrow** walks the frame stack's label arrays looking for active catch blocks, same logic as current `wasm__find_rethrow_label` but across frames instead of within one.

**Testing checkpoint:** Exception tests pass. Throw across call boundaries works.

---

## Phase 5: Remove old recursive `wasm__exec`

**Goal:** Delete dead code, clean up.

- Remove the old `wasm__exec` function entirely
- Remove `locals_buf`, `tail_args_buf`, the `restart` loop, `cleanup_frame` label
- `wasm_call_index` calls `wasm__interp` directly
- Remove the `depth` parameter that threaded through recursive calls
- The `WASM_MAX_CALL_DEPTH` check is now just `frame_sp >= WASM_MAX_CALL_DEPTH` at push time

**Testing checkpoint:** Full test suite passes. Valgrind/ASan clean.

---

## Phase 6: Tuning and hardening

- **Arena sizing:** Default to `WASM_MAX_CALL_DEPTH * (max_locals_across_module * sizeof(wasm_value_t) + max_labels_across_module * sizeof(wasm__label_t))`. Or just a flat default like 4MB with a `WASM_FRAME_ARENA_SIZE` define override.
- **Arena exhaustion:** Return `WASM_ERR_CALL_STACK_EXHAUSTED` if bump allocation fails. This is now a clean, deterministic resource limit instead of a C stack overflow / segfault.
- **Operand stack:** Consider moving it to the arena too, or at least heap-allocating it. The fixed `wasm_value_t stack[WASM_MAX_STACK]` on `wasm_runtime_t` is 4096 * 24 bytes = ~96KB inline in the struct, which is fine but worth noting.
- **Fuzz testing:** The explicit frame stack makes it easy to add resource limit assertions — you can check arena usage, frame depth, and operand stack depth all in one place with well-defined bounds.

---

## Risk areas to watch

1. **Host function reentry.** If a host function calls back into `wasm_call` on the same runtime, you now have two `wasm__interp` invocations sharing the same frame stack. Either forbid reentry (simplest), or save/restore `call_frame_sp` around host calls.

2. **The label cleanup dance.** Currently `wasm__clear_label` frees `caught_values`. With arena allocation, you need to decide whether exception payloads live in the arena (and get bulk-freed) or remain individually malloced. Arena is cleaner but means caught_values lifetimes must not outlive the frame.

3. **The `cf` pointer invalidation risk.** If `call_frames` is a flat array, `cf` stays valid across pushes. But if you ever realloc the frame array, all `cf` pointers are dangling. Fix: allocate frame array once at init to `WASM_MAX_CALL_DEPTH` size, never realloc.


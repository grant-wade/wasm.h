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

### Milestone 10: Tail Calls (Optional but Growing)

**Goal:** `return_call` (0x12) and `return_call_indirect` (0x13).

- Semantically: resolve the callee, tear down the current frame, and jump to the target without growing the call stack.
- In your recursive interpreter, true TCO requires restructuring `wasm__exec` to use a loop-based dispatch where `return_call` sets up the new function/args and `continue`s instead of recursing. This is a significant refactor — the current recursive design makes this the hardest milestone architecturally.

### Milestone 11: Exception Handling (Optional, Increasing Adoption)

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


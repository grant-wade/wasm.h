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

### Milestone 8: Multi-Memory

**Goal:** Multiple linear memories with indexed access.

- Refactor `mod->memory` to `mod->memories[]` + `mod->num_memories`. All memory instructions already read an alignment + offset — the memory index is the trailing byte on `memory.size`/`memory.grow` (currently hardcoded to read and discard `0x00`). For load/store, multi-memory uses the `0xFC` prefix encoding with an explicit memidx.
- Bulk memory ops (`memory.copy`, `memory.init`) also take memory indices.
- `wasm_memory_data()` / `wasm_memory_size()` / `wasm_memory_grow()` public API should take a memory index parameter (breaking API change) or default to memory 0.

### Milestone 9: Extended Const Expressions

**Goal:** `global.get`, `i32.add`, `i32.mul`, `i64.add`, `i64.mul` in init expressions.

- Currently global init and data/element segment offsets only handle `i32.const`, `i64.const`, `f32.const`, `f64.const`. The decoder reads one opcode + one immediate + `end`.
- Refactor init expression evaluation into a mini-interpreter that processes a sequence of const instructions until `0x0B`, supporting the extended set. This is a small contained evaluator — no control flow, no memory access, just arithmetic on a mini-stack.
- `global.get` in init exprs may only reference previously defined immutable globals or imported globals.

### Milestone 10: Tail Calls (Optional but Growing)

**Goal:** `return_call` (0x12) and `return_call_indirect` (0x13).

- Semantically: resolve the callee, tear down the current frame, and jump to the target without growing the call stack.
- In your recursive interpreter, true TCO requires restructuring `wasm__exec` to use a loop-based dispatch where `return_call` sets up the new function/args and `continue`s instead of recursing. This is a significant refactor — the current recursive design makes this the hardest milestone architecturally.
- Alternative: just implement them as call + return (correct semantics, just no stack depth guarantee). Pragmatic but modules relying on tail calls for deep recursion will hit `WASM_MAX_CALL_DEPTH`.

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
## WasmGC Implementation Plan

### Phase 1: Type System Refactor

**Goal**: Replace flat `wasm_functype_t` type array with heterogeneous composite types supporting rec groups and subtyping.

**[DONE] 1.1 New type definitions**

- Add `wasm_fieldtype_t` (storagetype + mutability), `wasm_packedtype_t` (i8, i16)
- Add `wasm_comptype_t` (func | struct | array) with subtype info (supertype indices, finality flag)
- Add `wasm_recgroup_t` to track rec group boundaries
- Add `WASM_FEATURE_GC` flag
- Extend `wasm_valtype_t` enum with new abstract heap types: `any` (0x12), `eq` (0x13), `i31` (0x14), `struct` (0x15), `array` (0x16), `none` (0x0F), `noextern` (0x0E), `nofunc` (0x0D)
- Extend `wasm_export_kind_t` if needed (tags already covered)

**[DONE] 1.2 Type section decoder**

- Modify `wasm__decode_type_section` to handle new binary opcodes: `0x4E` (rec), `0x50` (sub), `0x4F` (sub final), `0x5F` (struct), `0x5E` (array)
- Parse rec groups: each `rec` entry defines N types at consecutive indices
- Parse sub/sub final: read supertype vector, then comptype
- Shorthand: bare comptype == `sub final () comptype`
- Parse struct field types including packed storage types (i8=0x78, i16=0x77)
- Store results in `mod->types` as `wasm_comptype_t*`, update `mod->num_types` to count individual types (not rec groups)

**[DONE] 1.3 Replace `wasm_functype_t` usage throughout**

- Anywhere the codebase does `mod->types[idx]` expecting a functype, route through an `expand()` helper that asserts/checks `kind == WASM_COMP_FUNC`
- Update `wasm_func_param_count`, `wasm_func_result_count`, `wasm_func_param_type`, `wasm_func_result_type` to go through `expand()`
- Update call validation, `call_indirect`, import matching, tag type lookups

**[DONE] 1.4 Heap type / reftype parsing**

- Extend `wasm__is_valtype_byte` and reftype readers to handle the new heap type opcodes
- Heap types are encoded as s33 — positive values are type indices, negative values are abstract types
- Update `wasm__read_reftype`, `wasm__read_blocktype`, and anywhere else that reads a valtype from the binary

**[DONE] 1.5 Free/cleanup**

- Update `wasm_free_module` to free struct field arrays, supertype vectors, rec group metadata
- Update `wasm__free_functype` or replace with `wasm__free_comptype`

---

### Phase 2: Type Equivalence and Subtyping

**Goal**: Implement iso-recursive type equivalence and the full subtype relation.

**[DONE] 2.1 Type canonicalization**

- At load time, after all types are parsed, canonicalize rec groups bottom-up
- For each rec group, produce a "tied" representation where internal references use `rec.i` indices instead of absolute type indices
- Hash/intern rec groups so that structurally identical groups map to the same canonical form
- Store canonical ID per type for O(1) equivalence checks at runtime

**[DONE] 2.2 Type equivalence**

- `wasm__type_equal(mod, t1, t2)` — compare canonical IDs
- Used in `call_indirect` type checks (replace current `wasm__functype_equal`), import matching

**[DONE] 2.3 Subtype relation**

- `wasm__is_subtype(mod, t1, t2)` — walk supertype chain from t1, checking equivalence at each step
- `wasm__is_heap_subtype(mod, ht1, ht2)` — handle abstract heap type hierarchy: `none <: i31/struct/array <: eq <: any`, `nofunc <: $functype <: func`, `noextern <: extern`
- `wasm__is_reftype_subtype(mod, rt1, rt2)` — combines heap subtyping with nullability (non-null <: nullable)
- Composite type subtyping (only used during type definition validation): func contravariant/covariant, struct width+depth, array depth, field covariance for immutable / invariance for mutable

**[DONE] 2.4 Validation of type definitions**

- Validate supertype constraints: supertypes must have lower indices than current type, must not be final, comptype must match (struct subtypes struct with width subtyping, etc.)
- Integrate into `wasm__validate_structural` or a new `wasm__validate_types`

---

### Phase 3: Validation Updates

**Goal**: Update the bytecode validator to handle GC instructions and subtype-aware type checking.

**[DONE] 3.1 Subtype-aware operand stack**

- Replace `wasm__validator_type_matches` (currently equality) with subtype checking
- Update `wasm__validator_pop_expect` to accept subtypes
- The `WASM__TYPE_BOT` sentinel becomes `none`/`nofunc`/`noextern` depending on hierarchy

**[DONE] 3.2 Validate new instructions (0xFB prefix)**

- Add `wasm__validator_validate_gc` handler (parallel to existing `wasm__validator_validate_simd`, `wasm__validator_validate_prefixed`)
- Struct ops (0xFB00–0xFB05): check type index expands to struct, validate field index, check mutability for sets, packed signedness
- Array ops (0xFB06–0xFB13): check type index expands to array, validate element type, packed signedness, data/elem segment indices
- i31 ops (0xFB1C–0xFB1E): straightforward i32 ↔ i31ref
- Cast ops (0xFB14–0xFB19): validate `rt <: rt'` constraint, compute type difference for `br_on_cast`/`br_on_cast_fail`
- `ref.eq` (0xD3): both operands eqref, result i32
- `any.convert_extern` (0xFB1A), `extern.convert_any` (0xFB1B): nullability preservation

**[DONE] 3.3 Validate reference type extensions**

- `ref.null` with new heap types
- Blocktype returns with new reference types
- Global/table types with new reference types
- `select` with reference types in the typed variant

**[DONE] 3.4 Constant expression extensions**

- `struct.new`, `struct.new_default`, `array.new`, `array.new_default`, `array.new_fixed`, `ref.i31`, `any.convert_extern`, `extern.convert_any` are all constant instructions
- `global.get` in const exprs can reference preceding immutable globals (not just imports)
- Update `wasm__eval_init_expr` accordingly

---

### Phase 4: GC Heap

**Goal**: Implement a managed heap for GC-allocated structs and arrays.

**[DONE] 4.1 Heap object representation**

- Define `wasm_gc_header_t`: type index (RTT), mark bit, object size/kind
- Struct layout: header + N fields as `wasm_value_t` (or packed for i8/i16)
- Array layout: header + length (u32) + element data (packed or `wasm_value_t` per element)
- i31: no allocation — store as tagged value in `wasm_value_t` (use existing `i32` field with a distinct valtype)

**[DONE] 4.2 GC value in `wasm_value_t`**

- Add `WASM_TYPE_I31` to `wasm_valtype_t` (or reuse the 0x14 encoding)
- Add a GC ref variant to the `wasm_value_t` union — pointer to heap object
- Null GC ref = NULL pointer
- Update `wasm__default_value`, `wasm__is_null_ref`, `wasm__value_matches_type` for new types

**[DONE] 4.3 Heap allocator**

- Bump allocator for GC objects, backed by a configurable-size arena (`WASM_GC_HEAP_SIZE`)
- Track allocation list for mark-sweep traversal
- Provide `WASM_GC_ALLOC(sz)` / `WASM_GC_FREE(p)` override points consistent with existing `WASM_MALLOC` pattern

**[DONE] 4.4 Mark-sweep collector**

- **Mark phase**: walk root set — `rt->stack[0..sp]`, all active call frame locals, `mod->globals`, `mod->tables` — mark reachable GC objects, recursing into struct fields and array elements that hold GC refs
- **Sweep phase**: walk allocation list, free unmarked objects, clear marks on survivors
- Trigger: when bump allocator is exhausted, run collection, then retry
- Expose `wasm_gc_collect(rt)` as public API for manual triggering

** [DONE] 4.5 Struct/array accessors**

- `wasm__gc_struct_get_field(obj, field_idx)` — return `wasm_value_t`, handling packed reads with sign/zero extension
- `wasm__gc_struct_set_field(obj, field_idx, value)` — write, handling packed stores
- `wasm__gc_array_get(obj, idx)` — bounds check + read
- `wasm__gc_array_set(obj, idx, value)` — bounds check + mutability check + write
- `wasm__gc_array_len(obj)` — return length

---

### Phase 5: Interpreter — GC Instructions

**Goal**: Implement all 0xFB-prefixed GC opcodes and related instructions in the interpreter loop.

**[DONE] 5.1 Struct instructions**

- `struct.new $t` (0xFB00): pop N field values, allocate struct, store fields, push ref
- `struct.new_default $t` (0xFB01): allocate struct, zero-init all fields, push ref
- `struct.get $t i` (0xFB02): pop ref, null check, read field, push value
- `struct.get_s $t i` (0xFB03): same with sign extension for packed
- `struct.get_u $t i` (0xFB04): same with zero extension for packed
- `struct.set $t i` (0xFB05): pop value + ref, null check, write field

**[DONE] 5.2 Array instructions**

- `array.new $t` (0xFB06): pop init value + length, allocate, fill, push ref
- `array.new_default $t` (0xFB07): pop length, allocate, zero-init, push ref
- `array.new_fixed $t N` (0xFB08): pop N values, allocate, store, push ref
- `array.new_data $t $d` (0xFB09): pop size + offset, allocate from data segment, push ref
- `array.new_elem $t $e` (0xFB0A): pop size + offset, allocate from elem segment, push ref
- `array.get $t` (0xFB0B): pop index + ref, null/bounds check, read, push
- `array.get_s $t` (0xFB0C): sign-extending variant
- `array.get_u $t` (0xFB0D): zero-extending variant
- `array.set $t` (0xFB0E): pop value + index + ref, null/bounds check, write
- `array.len` (0xFB0F): pop ref, null check, push length
- `array.fill $t` (0xFB10): pop size + value + offset + ref, null/bounds check, fill
- `array.copy $t1 $t2` (0xFB11): pop size + src_offset + src + dst_offset + dst, null/bounds check, memmove-style copy
- `array.init_data $t $d` (0xFB12): pop size + src_offset + dst_offset + ref, copy from data segment
- `array.init_elem $t $e` (0xFB13): pop size + src_offset + dst_offset + ref, copy from elem segment

**[DONE] 5.3 i31 instructions**

- `ref.i31` (0xFB1C): pop i32, truncate to 31 bits, push i31ref
- `i31.get_s` (0xFB1D): pop i31ref, null check, sign-extend to i32, push
- `i31.get_u` (0xFB1E): pop i31ref, null check, zero-extend to i32, push

**5.4 Cast instructions**

- `ref.test (ref ht)` (0xFB14): pop ref, test RTT against target type, push i32 0/1
- `ref.test (ref null ht)` (0xFB15): same but returns 1 for null
- `ref.cast (ref ht)` (0xFB16): pop ref, test, trap on failure, push with narrowed type
- `ref.cast (ref null ht)` (0xFB17): same but passes null through
- `br_on_cast $l rt1 rt2` (0xFB18): read flags byte + label + two heap types, pop ref, if cast succeeds branch with narrowed type, otherwise fall through with type difference
- `br_on_cast_fail $l rt1 rt2` (0xFB19): inverse — branch on failure with original type, fall through with narrowed type

**[DONE] 5.5 Conversion instructions**

- `any.convert_extern` (0xFB1A): pop externref, wrap as anyref, push
- `extern.convert_any` (0xFB1B): pop anyref, wrap as externref, push

**[DONE] 5.6 ref.eq**

- `ref.eq` (0xD3): pop two eqrefs, compare identity (pointer equality for heap objects, value equality for i31), push i32

**[DONE] 5.7 Constant expression support**

- Add GC constant instructions to `wasm__eval_init_expr`: `ref.i31`, `struct.new`, `struct.new_default`, `array.new`, `array.new_default`, `array.new_fixed`, `any.convert_extern`, `extern.convert_any`
- Extend `global.get` in const exprs to reference preceding immutable globals

---

### Phase 6: Integration and Public API

**Goal**: Wire everything together, update public API, test.

**6.1 Import/export updates**

- Table imports/exports with GC reference types
- Global imports/exports with GC reference types
- Validate imported type compatibility using subtyping

**6.2 Table operations with GC types**

- Tables can hold `anyref`, `eqref`, `structref`, `arrayref`, `i31ref` — update table init, `table.get`/`table.set`, elem segment application
- GC collector must trace table entries as roots

**6.3 Public API additions**

- `wasm_gc_collect(wasm_runtime_t* rt)` — manual collection trigger
- Introspection: `wasm_type_kind(mod, type_idx)` → func/struct/array
- Struct/array field count, field types for tooling
- Consider: `wasm_struct_new`, `wasm_array_new` host-side allocation helpers

**6.4 Feature gating**

- All GC functionality gated behind `WASM_FEATURE_GC`
- GC implies `WASM_FEATURE_REFERENCE_TYPES` and typed function references as prerequisites
- Update `WASM__IMPLEMENTED_FEATURES` mask
- Update `wasm__feature_name`

**6.5 Header/documentation updates**

- Update the header comment to list GC support
- Update compatibility notes
- Add GC usage examples to the quick-start section
- Document `WASM_GC_HEAP_SIZE` and custom GC allocator hooks
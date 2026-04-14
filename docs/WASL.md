# wasl — Language Design Document

**Version:** 0.2 (Draft)
**Date:** April 2026
**Author:** Grant

---

## Overview

wasl is a programming language designed around WebAssembly. It is not a language that *targets* WebAssembly as a compilation backend — it is a language whose type system, memory model, and execution semantics are direct expressions of what WebAssembly provides. Where other languages fight WebAssembly's constraints or paper over them with runtime shims, wasl embraces them as the foundation of its design.

The language compiles to standard `.wasm` binaries. These binaries run on any compliant WebAssembly runtime — V8, Wasmtime, wasm-micro-runtime, or any other engine that supports the required proposals. wasl does not depend on a custom VM, a special loader, or a language-specific runtime library.

wasl assumes a runtime that supports the following finalized proposals:

- **GC** (structs, arrays, i31ref, subtyping)
- **Typed function references**
- **Exception handling** (try_table / throw_ref)
- **Fixed-width SIMD** (128-bit)
- **Relaxed SIMD**
- **Tail calls**
- **Multi-value**
- **Multiple memories**
- **Memory64**
- **Bulk memory operations**
- **Reference types**
- **Extended constant expressions**
- **Sign-extension operators**
- **Non-trapping float-to-int conversions**
- **Import/export of mutable globals**

These are all Phase 4 (standardized) or shipping in major engines. wasl does not require any in-progress proposals, though it will adopt new proposals (notably `shared-everything-threads`) as they stabilize.

---

## Motivation

### The problem

Every mainstream language targeting WebAssembly drags its own runtime model through a stack machine that wasn't designed for it:

- **Rust** works well but requires an allocator shim and fights linear memory semantics for anything heap-allocated. The borrow checker is orthogonal to WebAssembly's actual guarantees.
- **C/C++** treats linear memory as flat RAM and ignores the GC proposal, typed function references, and every post-MVP feature that doesn't look like a von Neumann machine.
- **Go, Python, JavaScript** require massive runtime shims — garbage collectors, interpreters, or JITs compiled to WebAssembly running on top of WebAssembly.
- **AssemblyScript** is the closest to "designed for Wasm" but remains TypeScript-flavored and doesn't leverage the full modern proposal surface.

None of these languages were designed with WebAssembly's actual type system in mind. The GC proposal gives WebAssembly real structs, arrays, reference types, and subtyping. Typed function references give it safe indirect calls. Exception handling gives it structured control flow for errors. SIMD gives it data parallelism. These aren't compilation targets to be lowered into — they're language features waiting for syntax.

### The opportunity

wasl is the language that treats WebAssembly's type system as its own. A struct in wasl *is* a WebAssembly GC struct. Pattern matching on a variant *is* a `br_on_cast`. A tail-recursive call *is* a `return_call`. There is no impedance mismatch between the source language and the execution model.

This has concrete consequences:

- **No custom allocator or GC runtime.** Memory management uses WebAssembly's built-in GC. The host runtime handles collection natively.
- **No representation overhead.** A wasl record is a GC struct, not a C struct pretending to be in linear memory, not a JavaScript object serialized into bytes.
- **Predictable codegen.** A wasl programmer who understands WebAssembly can predict what their code compiles to. There are no hidden costs.
- **Full proposal surface.** SIMD, tail calls, exception handling, multiple memories, Memory64 — all have first-class syntax. You don't drop to inline assembly or compiler intrinsics to use them.
- **Concrete exports for free.** Monomorphized generics mean every exported type is a concrete GC struct. Any consumer — JavaScript, Rust, C, another wasl module — sees plain types with no need to understand wasl's generics encoding.
- **No runtime lock-in.** wasl output is standard WebAssembly. Run it on V8 in a browser, Wasmtime on a server, a microcontroller runtime, or any compliant engine. The language doesn't care.

### Why now

WebAssembly GC reached Phase 4 (standardized) and shipped in major engines in late 2023. Exception handling stabilized in early 2024. The post-MVP proposal surface is now rich enough to support a real language without falling back to linear memory for everything. The platform is ready for a language designed around it rather than merely compiled to it.

---

## Design Philosophy

### First principles

1. **WebAssembly is the semantic model, not the compilation target.** Every language feature must have a direct, obvious mapping to WebAssembly constructs. If it can't be expressed cleanly in WebAssembly, it doesn't belong in wasl.

2. **Two memory worlds, both first-class.** The GC heap is the default for structured data. Linear memory is available via `unsafe fn` declarations for raw byte manipulation, SIMD over flat buffers, FFI layouts, and performance-critical data processing. Neither is an afterthought.

3. **Monomorphized generics over GC types.** Parametric polymorphism in the source, concrete GC struct types in the output. No boxing, no uniform representation, no runtime type dispatch. Code size is the tradeoff; predictable performance and clean interop are the payoff.

4. **Expression-oriented, ML-flavored.** Functions are expressions. Match arms are expressions. Blocks evaluate to their last expression. The syntax draws from the ML family — algebraic data types, pattern matching, type inference, `let` bindings — because that family maps cleanly to tagged GC structs with subtyping.

5. **Errors are values and exceptions.** Expected failures return `Result<T, E>`. Unexpected failures throw typed exception tags. Both map directly to WebAssembly constructs (`br_on_cast` for result matching, `try_table`/`throw_ref` for exceptions). The language doesn't force one error model.

6. **Explicit over implicit at boundaries.** Exports are marked `pub`. Imports are declared `extern`. Nothing is automatically visible across module boundaries.

7. **Runtime-agnostic.** wasl output is standard WebAssembly. The language specification does not assume any particular runtime, embedding API, or host environment. A wasl module is a `.wasm` file — the embedder decides how to load and run it.

### What wasl is not

- **Not a systems language.** wasl has an `unsafe fn` escape hatch to linear memory, but the default path is GC-managed. If you need `malloc`/`free` everywhere, write C and compile it to WebAssembly.
- **Not a scripting language.** wasl is statically typed, compiled ahead of time, and has no runtime eval or dynamic dispatch beyond WebAssembly's own `call_indirect`.
- **Not a WebAssembly assembler.** wasl is a high-level language with type inference, generics, and pattern matching. It is informed by WebAssembly's semantics, not a textual encoding of them.
- **Not tied to WASI.** wasl modules communicate with the host through explicit imports. WASI is one possible set of imports, not a requirement. A wasl module can import nothing, import custom host functions, or import full WASI interfaces — it's the embedder's choice.
- **Not tied to the component model.** wasl targets core WebAssembly modules. Component model support (WIT interfaces, canonical ABI, component packaging) is a potential future extension, not a dependency.

---

## Type System

### Primitive types

wasl's primitive types map directly to WebAssembly value types:

| wasl type | WebAssembly type | Notes |
|-----------|-----------------|-------|
| `i32` | `i32` | Signed 32-bit integer |
| `i64` | `i64` | Signed 64-bit integer |
| `u32` | `i32` | Unsigned semantics; compiler inserts wrapping/extending |
| `u64` | `i64` | Unsigned semantics |
| `u8`, `u16` | `i32` | Narrower unsigned types; range-checked at boundaries |
| `s8`, `s16` | `i32` | Narrower signed types; range-checked at boundaries |
| `f32` | `f32` | IEEE 754 single-precision float |
| `f64` | `f64` | IEEE 754 double-precision float |
| `bool` | `i32` | 0 or 1 |
| `char` | `i32` | Unicode scalar value (validated) |

The narrower integer types (`u8`, `u16`, `s8`, `s16`) exist in wasl's type system for clarity and safety. They are represented as `i32` at the WebAssembly level. The compiler inserts range checks, masking, and sign-extension as needed at type boundaries. Within expressions, arithmetic on narrow types follows WebAssembly's `i32` semantics.

### Strings

Strings are GC-managed, UTF-8 encoded, and immutable by default. The runtime representation is a GC struct containing a GC byte array and a length. String operations (slicing, concatenation, iteration) are built-in and bounds-checked.

String interpolation is supported at the language level: `"hello {name}"` desugars to concatenation at compile time.

**String access model:** Direct indexing (`s[n]`) is not supported on strings. Instead, strings expose:

- `.byte_at(n)` — O(1) access to the nth byte. Returns `u8`.
- `.codepoint_at(n)` — O(n) access to the nth Unicode codepoint. Returns `Result<char, Error>`. The name makes the cost visible.
- `.codepoints()` — Returns an iterator over `char` values. This is the primary way to process string contents.
- `.bytes()` — Returns an iterator over `u8` values.
- `.len()` — Byte length (O(1)).
- `.codepoint_len()` — Codepoint count (O(n)). Name signals the cost.

This design prevents accidental O(n²) loops from hidden linear scans. The iterator-based API is the idiomatic path.

### Composite types

**Records** compile to WebAssembly GC structs:

```
type Point = { x: f64, y: f64 }
```

**Algebraic types** (sum types) compile to a GC supertype with one GC subtype per variant. The discriminant is structural via `ref.test` / `br_on_cast`, not a stored tag field:

```
type Shape =
    | Circle { radius: f64 }
    | Rect { width: f64, height: f64 }
    | Triangle { a: Point, b: Point, c: Point }
```

**Enums** (variants with no payload) compile to `i32` discriminants:

```
type Color = | Red | Green | Blue
```

**Tuples** are anonymous GC structs:

```
let pair: (i32, f64) = (42, 3.14)
```

**GC arrays** are WebAssembly GC arrays:

```
type Buffer = [u8]
type Points = [Point]
```

Fixed-length arrays with known size at compile time can be optimized to GC structs or, in `unsafe fn` contexts, linear memory.

### Generics

wasl supports parametric polymorphism with monomorphization. Each concrete instantiation of a generic type produces a distinct GC type hierarchy in the output:

```
type Option<T> =
    | None
    | Some(T)

type Result<T, E> =
    | Ok(T)
    | Err(E)
```

`Option<i32>` and `Option<string>` produce entirely separate GC struct families. There is no shared representation and no boxing. The compiler performs monomorphization as a pass between type checking and codegen.

This is a deliberate interop advantage: every exported type is a concrete GC struct that any consumer can use without understanding wasl's generics. The code size cost is acceptable — runtimes like Go and C# already embed far larger runtime payloads in their WebAssembly output.

### Traits

wasl supports a trait system for bounded polymorphism. Traits define interfaces that types can implement:

```
trait Eq {
    fn eq(self, other: Self) -> bool
}

trait Ord: Eq {
    fn cmp(self, other: Self) -> Ordering
}

trait Show {
    fn show(self) -> string
}
```

Trait implementations are declared separately from the type:

```
impl Eq for Point {
    fn eq(self, other: Point) -> bool =
        self.x == other.x && self.y == other.y
}
```

Generic functions can constrain type parameters with trait bounds:

```
fn sort<T: Ord>(arr: [T]) -> [T] = { ... }
fn print_all<T: Show>(items: [T]) = { ... }
```

Traits are resolved and monomorphized at compile time. There is no vtable, no dynamic dispatch, no runtime cost. A trait bound is a compile-time constraint that ensures the monomorphizer can find a concrete implementation for every instantiation.

Primitive types have built-in trait implementations: all numeric types implement `Eq` and `Ord`, all types implement `Show` (with a default structural representation that can be overridden).

### Function types

Function types are first-class and map to WebAssembly typed function references (`funcref`):

```
let transform: fn(i32) -> i32 = |x| x * 2
```

Closures that capture variables compile to a GC struct (the environment) plus a function that receives the struct as an implicit first parameter. The closure value is a pair of `(funcref, envref)`.

### SIMD types

SIMD types provide lane-typed views over WebAssembly's `v128` type. The exact syntax is TBD — it must be visually distinct from fixed-length array syntax to avoid parser ambiguity and user confusion. Candidate syntaxes under consideration:

```
// Option A: Named types
type f32x4 = simd<f32, 4>
type i16x8 = simd<i16, 8>

// Option B: Built-in named types
f32x4, i16x8, u8x16, f64x2   // first-class type names
```

SIMD operations are namespaced by lane type: `simd.f32x4.mul(a, b)`, `simd.i16x8.add(a, b)`. The compiler enforces lane-type consistency at call sites.

### Linear memory types

Within `unsafe fn` declarations, raw pointer types provide access to linear memory:

```
type ptr32 = i32    // offset into a 32-bit memory
type ptr64 = i64    // offset into a 64-bit memory (Memory64)
```

Packed struct layouts can be declared for structured access to linear memory without GC involvement:

```
#[packed]
type Header = {
    magic: u32,
    version: u16,
    flags: u16,
    size: u64,
}
```

Packed structs have compiler-determined, deterministic layouts with explicit size and alignment. They are read from and written to linear memory as byte sequences — they do not allocate on the GC heap.

---

## Execution Model

### Functions

Functions are expressions. The body is a single expression whose value is the return value:

```
fn add(a: i32, b: i32) -> i32 = a + b

fn classify(n: i32) -> string =
    if n > 0 then "positive"
    else if n < 0 then "negative"
    else "zero"
```

Multi-statement bodies use blocks. The last expression in a block is the block's value:

```
fn process(data: [u8]) -> Result<string, Error> = {
    let validated = validate(data)
    let parsed = parse(validated)
    transform(parsed)
}
```

### Mutability

`let` bindings are immutable by default. `let mut` declares a mutable binding:

```
let x = 42          // immutable
let mut counter = 0 // mutable
counter = counter + 1
```

This follows the ML tradition of immutable-by-default for safety. The `let mut` convention mirrors Rust's syntax, which is widely understood. At the WebAssembly level, all locals are mutable — the immutability constraint is enforced by the compiler.

### Multi-value returns

Functions can return multiple values, mapping directly to WebAssembly's multi-value:

```
fn divmod(a: i32, b: i32) -> (i32, i32) = (a / b, a % b)

let (q, r) = divmod(17, 5)
```

### Tail calls

Explicit tail calls compile to WebAssembly `return_call`:

```
fn gcd(a: i64, b: i64) -> i64 =
    match b {
        0 -> a,
        _ -> tail gcd(b, a % b),
    }
```

The `tail` keyword is required. The compiler verifies the call is in tail position and rejects `tail` on non-tail calls. This is not an optimization hint — it is a semantic guarantee that the call uses constant stack space.

### Pattern matching

Pattern matching is the primary dispatch mechanism. It compiles to `br_on_cast` for GC types and `br_table` for enums:

```
fn describe(s: Shape) -> string =
    match s {
        Circle { radius } if radius > 100.0 -> "big circle",
        Circle { .. }                        -> "small circle",
        Rect { width, height } if width == height -> "square",
        Rect { .. }                          -> "rectangle",
        Triangle { .. }                      -> "triangle",
    }
```

Exhaustiveness checking is enforced at compile time. A non-exhaustive match is a compile error, not a runtime trap.

Guards (`if` clauses) are supported. Guarded arms do not count toward exhaustiveness — a catch-all or complete unguarded coverage is still required.

Nested patterns, literal patterns, and or-patterns are supported:

```
match pair {
    (0, _) | (_, 0) -> "has zero",
    (x, y) if x == y -> "equal",
    (x, y) -> "different",
}
```

### Error handling

wasl provides two complementary error mechanisms:

**Result types** for expected, recoverable errors:

```
fn parse_int(s: string) -> Result<i32, ParseError> = { ... }

match parse_int(input) {
    Ok(n)    -> use(n),
    Err(e)   -> report(e),
}
```

A `?` operator propagates errors:

```
fn load_config(path: string) -> Result<Config, Error> = {
    let text = read_file(path)?
    let parsed = parse_toml(text)?
    validate(parsed)
}
```

**Exception tags** for unexpected, non-local errors. These compile to WebAssembly exception handling (`try_table` / `throw_ref`):

```
exception DivByZero
exception OutOfBounds { index: i32, len: i32 }

fn safe_div(a: i32, b: i32) -> i32 =
    if b == 0 then throw DivByZero
    else a / b

fn main() =
    try {
        let x = safe_div(10, 0)
        print(x)
    } catch {
        DivByZero -> print("division by zero"),
        OutOfBounds { index, len } -> print("oob"),
    }
```

Exception tags are nominal types declared at the module level. They map directly to WebAssembly tag declarations in the tag section.

The two error mechanisms are fully independent. If a function returns `Result<T, E>` and an exception is thrown inside it, the exception propagates — it is not automatically caught and wrapped in `Err`. This keeps the semantics clean: `Result` is for expected failures you handle locally, exceptions are for unexpected failures that escape.

### Unsafe functions and linear memory

The `unsafe` keyword is a function-level annotation, not a block scope. An `unsafe fn` declares a function that operates on linear memory. The function signature is the safety boundary: GC refs go in as parameters, the function does its linear memory work, and GC refs come out as return values.

Memories are declared at the module level and can be 32-bit or 64-bit:

```
memory default: mem32
memory large: mem64
```

Unsafe functions receive GC refs normally and can operate on linear memory:

```
unsafe fn write_header(mem: memory, offset: ptr32, data: Header) = {
    mem.write_packed(offset, data)
}

unsafe fn read_header(mem: memory, offset: ptr32) -> Header = {
    mem.read_packed(offset)
}

unsafe fn process_buffer(mem: memory, ptr: ptr32, len: i32) -> [u8] = {
    let result = Array.new(len)
    // read from linear memory, write to GC array
    result
}
```

The `unsafe` annotation means: this function may read/write linear memory. The caller knows the boundary — calling an `unsafe fn` is an explicit acknowledgment.

#### GC reference pinning

When a GC reference needs to persist beyond a single `unsafe fn` call — for example, registering a callback with a C-compiled module — the `gc.pin` / `gc.unpin` mechanism provides controlled escape:

```
unsafe fn register_callback(cb: fn(i32) -> i32) -> i32 = {
    let handle = gc.pin(cb)
    // handle is an i32 index into a runtime-managed table
    // store it in linear memory, pass to C module, etc.
    handle
}

unsafe fn release_callback(handle: i32) = {
    gc.unpin(handle)
}

unsafe fn invoke_callback(handle: i32, arg: i32) -> i32 = {
    let cb = gc.get(handle)  // retrieve the pinned ref
    cb(arg)
}
```

Under the hood, `gc.pin` performs a `table.set` into a dedicated `funcref`/`externref` table, keeping the GC ref alive. `gc.unpin` clears the table slot. `gc.get` retrieves the ref via `table.get`. The handle is an `i32` table index.

This is not an escape hatch from the type system — it's syntax for the only mechanism WebAssembly provides for holding GC refs outside the GC heap. There is no pointer casting, no raw memory storage of refs. Tables are it, and `gc.pin`/`gc.unpin` is the API.

The programmer takes manual responsibility for the pinned ref's lifetime. An unpinned handle is a dangling reference. This is the correct tradeoff for `unsafe` code.

#### Linear memory allocation

`mem.alloc` and `mem.free` use a built-in bump allocator over linear memory by default. Embedders can supply a custom allocator via host imports:

```
unsafe fn allocate_buffer(mem: memory, size: i32) -> ptr32 = {
    let p = mem.alloc(size)
    mem.store_i32(p, 0, 42)
    p
}
```

#### Safety boundary rules

- GC references cannot be stored in linear memory (WebAssembly's type system enforces this — it's not a compiler rule, it's a VM rule).
- Raw pointers (`ptr32`/`ptr64`) exist only within `unsafe fn` bodies.
- `gc.pin` is the only mechanism for persisting GC refs across `unsafe fn` calls. It compiles to table operations, which is the only thing WebAssembly allows.

### Tables

WebAssembly tables are declared at the module level and accessible for indirect call dispatch:

```
table dispatch: [fn(i32) -> i32; 64]

fn register(idx: i32, f: fn(i32) -> i32) =
    table.set(dispatch, idx, f)

fn call_handler(idx: i32, arg: i32) -> i32 =
    table.get(dispatch, idx)(arg)
```

This compiles to `table.set`, `table.get`, and `call_indirect`. Tables are primarily useful for plugin systems, vtable-style dispatch, and interop with C-compiled modules that use indirect calls.

---

## Module System

### Exports and imports

Exports are marked with `pub`. Non-`pub` declarations are module-internal:

```
pub fn add(a: i32, b: i32) -> i32 = a + b
fn helper() -> i32 = 42   // not exported
```

### Extern declarations

All imports — host functions, other WebAssembly modules, WASI interfaces — are declared with `extern` blocks. The string is the WebAssembly import module name:

```
extern "host" {
    fn print(s: string)
    fn timestamp() -> f64
    fn random_bytes(len: i32) -> [u8]
}

extern "math" {
    fn sqrt(x: f64) -> f64
    fn pow(base: f64, exp: f64) -> f64
}
```

The compiler emits these as WebAssembly imports in the import section. At runtime, the embedder wires the imports to the appropriate module's exports — whether that's a host function, a C-compiled `.wasm` module, another wasl module, or a WASI implementation.

This is the only module system construct. wasl does not have a `use` or `import` statement that references filenames or paths, because a WebAssembly module cannot load other modules — that is always the embedder's responsibility.

The *toolchain* (`wasl run`, a future build system, a manifest file) can maintain the mapping from import module names to `.wasm` files or host bindings. But that mapping is toolchain configuration, not language syntax.

For wasl-to-wasl composition (both modules compiled by `waslc`), the compiler can verify type compatibility between one module's `extern` declarations and another's `pub` exports at build time. For wasl-to-foreign composition, the `extern` declaration is the contract and link-time checking is the enforcement.

### Future: component model integration

wasl targets core WebAssembly modules. The WebAssembly component model (WIT interfaces, canonical ABI, component packaging) is a natural future extension:

- `extern` blocks could reference WIT interfaces instead of raw module names.
- The compiler could emit component-model binaries instead of core modules.
- Cross-language interop would get automatic marshaling through the canonical ABI.

This is an additive extension, not a redesign. The `extern` syntax already separates the logical interface (what functions exist with what types) from the physical binding (which module provides them). Component model support changes the binding mechanism without touching the interface declarations.

---

## Toolchain

### Architecture

```
waslc (compiler)     →  .wasm core modules
wasl  (runner)       →  compile + execute (delegates to any wasm runtime)
```

**`waslc`** is the compiler. It is a native binary written in C. It takes `.wasl` source files and produces standard `.wasm` binaries. It performs lexing, parsing, type checking, type inference, monomorphization, and WebAssembly codegen. It does not require an external toolchain, runtime, or code generation step.

**`wasl`** is a convenience runner that compiles and executes in one step. It delegates execution to whatever WebAssembly runtime is available on the system. The runner is a thin wrapper — the compiler is the core tool.

### Compilation pipeline

```
.wasl source
    │
    ├── Lexer → Token stream
    ├── Parser → AST
    ├── Name resolution → Scoped AST
    ├── Type checking / inference → Typed AST
    ├── Trait resolution → Resolved AST
    ├── Monomorphization → Specialized AST (no generics)
    └── WASM codegen → .wasm binary
```

### Build and run

```sh
# Compile
waslc build app.wasl -o app.wasm

# Run on any wasm runtime
wasmtime app.wasm
node --experimental-wasm-gc -e "..."

# Or compile + run in one step
wasl run app.wasl
```

### Distribution

A wasl project can be distributed as:

1. **Source** (`.wasl` files) — requires `waslc` to build.
2. **WebAssembly** (`.wasm` file) — runs on any WebAssembly runtime that supports the required proposals. Portable, sandboxed, no native dependencies.

Because wasl output is standard WebAssembly, it benefits from the entire existing ecosystem: `wasm-opt` for optimization, `wasm-tools` for inspection and validation, native AOT compilation via Wasmtime's cranelift or V8's TurboFan, and embedding in any host that can load `.wasm` files.

### Embedding

wasl modules are standard `.wasm` files. They can be embedded in any host application using whatever WebAssembly runtime the host prefers:

- **C/C++**: Wasmtime C API, wasm-micro-runtime, or any C-embeddable runtime
- **Rust**: wasmtime, wasmer
- **Go**: wazero, wasmtime-go
- **JavaScript/Browser**: native WebAssembly APIs
- **Python**: wasmtime-py

No wasl-specific runtime or binding library is required. The host loads a `.wasm` file, provides the declared imports, and calls the exported functions. The WebAssembly type system enforces the contract.

---

## Milestones

### Milestone 0: Lexer and Parser

**Goal:** Parse wasl source into an AST.

**Deliverables:**

- Formal EBNF grammar resolving all syntactic ambiguities before parser implementation
- Lexer: tokenize wasl source including keywords (`fn`, `let`, `mut`, `match`, `if`, `then`, `else`, `pub`, `unsafe`, `tail`, `extern`, `type`, `trait`, `impl`, `exception`, `throw`, `try`, `catch`, `memory`, `table`, `simd`), operators, literals (integers, floats, strings with interpolation, chars), identifiers, and delimiters
- Parser: recursive descent, producing a complete AST for type declarations (records, algebraic types, enums, tuples, packed structs), trait declarations and implementations, function declarations (single-expression and block bodies, `unsafe fn`), exception declarations, pattern match expressions, if/then/else, let/let mut bindings, closures / lambda expressions, extern blocks, pub/unsafe/tail modifiers, SIMD and table declarations, memory declarations
- Error reporting: source location tracking, clear error messages with line/column
- Pretty-printer: AST → wasl source round-trip for debugging

**Validation:**

- Parse the full language sketch examples from this document
- Round-trip: parse → pretty-print → parse produces identical AST
- Error recovery: malformed input produces useful diagnostics, not crashes

### Milestone 1: Name Resolution and Scope Analysis

**Goal:** Resolve all identifiers to their declarations, build scope chains, detect errors.

**Deliverables:**

- Module-level scope: type declarations, trait declarations, function declarations, extern imports, exception declarations, memory/table declarations
- Function-level scope: parameters, let bindings, match arm bindings, closure captures
- Shadowing rules: inner let bindings shadow outer, with optional warnings
- Capture analysis for closures: identify which variables a closure references from enclosing scopes
- Forward reference handling: all module-level declarations visible throughout the module regardless of source order
- Error detection: undefined variables, duplicate declarations, unused variables (warning), mutability violations (assignment to immutable binding)

**Validation:**

- Correct resolution of shadowed names across nested scopes
- Closure capture sets are accurate
- Undefined variable errors point to the correct source location
- Assignment to `let` (non-`mut`) binding produces a compile error

### Milestone 2: Type System — Core

**Goal:** Type check all expressions and declarations; infer types for let bindings and closures.

**Deliverables:**

- Bidirectional type checking: propagate expected types downward, synthesize types upward
- Type inference for let bindings: `let x = 42` infers `i32`
- Type inference for closures: `|x| x + 1` infers `fn(i32) -> i32` from context
- Literal typing: integer literals default to `i32`, float literals default to `f64`, with contextual widening to `i64` / `f32` where expected
- Record, algebraic type, and enum type checking
- Tuple type checking
- Function signature verification: argument types, return type, tail call position validity
- `unsafe fn` checking: verify linear memory operations only appear in unsafe functions
- Pattern exhaustiveness checking
- Basic type error messages: expected/got with source location

**Validation:**

- All example programs from this document type-check successfully
- Exhaustiveness checker rejects incomplete matches
- Linear memory operations outside `unsafe fn` produce compile errors
- Type errors produce clear, actionable messages

### Milestone 3: Type System — Traits, Generics, and Monomorphization

**Goal:** Support traits, generic type/function declarations, and monomorphize all generics before codegen.

**Deliverables:**

- Trait declarations: method signatures, supertraits (`trait Ord: Eq`)
- Trait implementations: `impl Trait for Type` with method bodies
- Built-in trait implementations for primitive types (`Eq`, `Ord`, `Show` for numerics)
- Generic type parameters on type declarations: `type Option<T> = | None | Some(T)`
- Generic type parameters on function declarations: `fn map<A, B>(opt: Option<A>, f: fn(A) -> B) -> Option<B>`
- Trait bounds on generic parameters: `fn sort<T: Ord>(arr: [T]) -> [T]`
- Trait resolution: for each monomorphized call site, resolve the concrete trait implementation
- Monomorphization pass: for each concrete use of a generic type or function, generate a specialized version with all type parameters substituted, with resolved trait method calls inlined
- Dead-specialization elimination: don't emit specializations that are never used
- Recursive generic types: `type Tree<T> = | Leaf(T) | Node { left: Tree<T>, right: Tree<T> }` — the monomorphizer handles these without infinite expansion by detecting recursion through the same type parameters
- Error messages that refer to the original generic source, not the monomorphized output
- Missing trait implementation errors: clear message when a type doesn't implement a required trait

**Validation:**

- `Option<i32>` and `Option<string>` produce distinct monomorphized types
- `sort<i32>` resolves `Ord` for `i32` and monomorphizes correctly
- Recursive types like `Tree<i32>` monomorphize correctly
- Unused specializations are absent from the output
- Missing `impl Ord for MyType` when calling `sort<MyType>` produces a clear error

### Milestone 4: WASM Codegen — Fundamentals

**Goal:** Emit valid `.wasm` binaries for a core subset of wasl: functions, arithmetic, control flow, let bindings.

**Deliverables:**

- WASM binary encoder: produce valid `.wasm` files with type section, function section, code section, export section, import section
- Codegen for primitive expressions: integer/float arithmetic, comparisons, boolean logic
- Codegen for let bindings (mutable and immutable): WebAssembly locals
- Codegen for if/then/else: `if` instruction or `br_if`
- Codegen for function calls: `call` instruction
- Codegen for `pub` functions: WebAssembly exports
- Codegen for `extern` imports: WebAssembly imports
- Codegen for multi-value returns: multiple result types on function signature
- Output validation: produced `.wasm` files accepted by at least two independent runtimes and `wasm-tools validate`

**Validation:**

- `fn add(a: i32, b: i32) -> i32 = a + b` compiles and executes correctly
- `fn fib(n: i32) -> i32` compiles and produces correct results
- Imported host functions (via `extern`) are called correctly

**This is the "wasl runs" milestone.**

### Milestone 5: WASM Codegen — GC Types

**Goal:** Emit WebAssembly GC types for wasl records, algebraic types, arrays, and strings.

**Deliverables:**

- Emit GC struct types for records
- Emit GC subtype hierarchies for algebraic types (one supertype, one subtype per variant)
- Emit `struct.new`, `struct.get`, `struct.set` for record construction and field access
- Emit `br_on_cast` / `ref.test` chains for pattern matching on algebraic types
- Emit `br_table` for pattern matching on enums
- Emit GC array types for `[T]` arrays
- Emit `array.new`, `array.get`, `array.set`, `array.len` for array operations
- String representation: GC struct wrapping a GC byte array
- String operations: `.byte_at()`, `.codepoint_at()`, `.codepoints()`, `.bytes()`, `.len()`, `.codepoint_len()`, concatenation, interpolation — all bounds-checked
- Match guard codegen: conditional branches within match arms

**Validation:**

- `type Point = { x: f64, y: f64 }` creates a GC struct, fields are accessible
- `type Shape = | Circle { radius: f64 } | Rect { ... }` pattern matching works
- `Tree<i32>` recursive type with GC refs works (build tree, traverse, produce correct result)
- String iterator and `.codepoint_at()` produce correct results for multi-byte UTF-8

**This is the "wasl has real types" milestone.**

### Milestone 6: WASM Codegen — Closures and Function References

**Goal:** Emit typed function references and closure representations.

**Deliverables:**

- Emit `funcref` types for function reference values
- Closure codegen: generate a GC struct for the capture environment, generate a wrapper function that receives the environment struct, represent the closure value as `(funcref, envref)` pair (or as a GC struct containing both)
- Higher-order function calls: passing and calling function references
- `call_ref` for calling typed function references
- Closure capture of mutable variables via GC ref cells

**Validation:**

- `let f: fn(i32) -> i32 = |x| x + 1` works
- Closures capturing outer variables work
- `fn apply(f: fn(i32) -> i32, x: i32) -> i32 = f(x)` works
- Closures capturing mutable state observe mutations correctly

### Milestone 7: WASM Codegen — Tail Calls and Exception Handling

**Goal:** Emit `return_call` for tail calls and `try_table` / `throw_ref` for exception handling.

**Deliverables:**

- `tail` keyword: emit `return_call` / `return_call_indirect` / `return_call_ref`
- Compiler verification that `tail` is in tail position
- `exception` declarations: emit WebAssembly tag section entries
- `throw` expressions: emit `throw` / `throw_ref`
- `try`/`catch` blocks: emit `try_table` with appropriate catch clauses
- Tag payload extraction in catch arms
- Interaction with pattern matching: catch arms support the same pattern syntax
- Verify that exceptions and `Result` are independent: exceptions thrown inside a `Result`-returning function propagate, they are not auto-wrapped

**Validation:**

- `gcd` with `tail` uses constant stack space (test with deep recursion)
- `throw DivByZero` is caught by the correct handler
- Exception payloads are extracted correctly
- Uncaught exceptions propagate to the host
- Exception inside a `Result`-returning function propagates past the function

### Milestone 8: WASM Codegen — Unsafe, SIMD, and Linear Memory

**Goal:** Emit linear memory access within `unsafe fn` declarations, SIMD instructions, and the `gc.pin`/`gc.unpin` mechanism.

**Deliverables:**

- Memory declarations: emit WebAssembly memory section entries (32-bit and 64-bit)
- `unsafe fn` codegen: track unsafe context, allow linear memory operations within unsafe function bodies
- Load/store operations: emit `i32.load`, `i32.store`, etc. for raw memory access
- Packed struct codegen: compute layouts, emit sequential load/store for `mem.read_packed` / `mem.write_packed`
- `gc.pin` / `gc.unpin` / `gc.get`: emit `table.set` / `table.get` against a dedicated pinning table, manage slot allocation
- SIMD codegen: emit `v128` operations for SIMD expressions, lane-type checking enforced at compile time, map `simd.f32x4.mul` etc. to the corresponding WebAssembly SIMD instructions
- `mem.alloc` / `mem.free`: emit calls to a built-in allocator (a simple bump allocator over linear memory, exported for host override)
- Table declarations: emit WebAssembly table section entries
- `table.get` / `table.set`: emit corresponding instructions

**Validation:**

- `unsafe fn` reads and writes linear memory correctly
- Packed struct round-trip: write a header, read it back, fields match
- `gc.pin` a closure, retrieve it via `gc.get`, invoke it — correct result
- `gc.unpin` clears the table slot
- SIMD dot product produces correct results
- Memory64 addresses work for `mem64` memories
- Table-based indirect dispatch works
- Linear memory operations outside `unsafe fn` are rejected by the compiler

### Milestone 9: Module Linking

**Goal:** Implement multi-module composition through `extern` declarations and toolchain-level linking.

**Deliverables:**

- `extern` declarations emit WebAssembly imports with the appropriate module/name pairs
- `waslc` cross-module checking: when compiling multiple `.wasl` files, verify that one module's `extern` declarations are compatible with another's `pub` exports
- `wasl run` multi-module linking: instantiate multiple modules, wire imports to exports based on toolchain configuration (CLI flags, manifest file, or naming convention)
- Foreign module linking: when linking against a non-wasl `.wasm` module, link-time validation checks the `extern` declaration against the module's actual exports
- Mismatched interface produces a clear link-time error

**Validation:**

- wasl module declares `extern "math" { fn sqrt(x: f64) -> f64 }`, links against a C-compiled math module, calls it correctly
- wasl module A exports `pub fn foo()`, wasl module B declares `extern "A" { fn foo() }`, types checked at compile time
- Mismatched types between `extern` and actual export produce a clear error

### Milestone 10: Standard Library

**Goal:** A minimal standard library providing common operations without requiring host imports.

**Deliverables:**

- `core.string`: string manipulation (split, join, trim, contains, starts_with, replace, codepoint iteration utilities)
- `core.array`: array operations (map, filter, fold, sort, find, zip) — `sort` uses the `Ord` trait
- `core.math`: math functions implementable in pure WebAssembly (abs, min, max, clamp, floor, ceil, sqrt via `f64.sqrt`)
- `core.option` / `core.result`: utility functions (map, and_then, unwrap_or, etc.)
- `core.fmt`: string formatting and interpolation support
- Built-in trait implementations: `Eq`, `Ord`, `Show` for all primitive types

The standard library is written in wasl and compiled to `.wasm`. It is linked via the normal `extern` mechanism. It has no host imports — everything is pure WebAssembly. Embedders who don't need it don't pay for it.

**Validation:**

- All standard library functions have test coverage
- Standard library modules compile and link against user code
- `sort` works for any type implementing `Ord`

### Milestone 11: Polish, Tooling, and Documentation

**Goal:** Production-quality tooling and documentation.

**Deliverables:**

- `waslc` error messages: clear, contextual, with source snippets and suggestions
- `waslc fmt`: source formatter
- `waslc check`: type check without emitting code
- Language reference documentation
- Tutorial: from hello world to a non-trivial program
- Embedding guide: how to use wasl modules from various host languages and runtimes
- Performance baseline: benchmark suite comparing wasl output against hand-written WebAssembly and C-compiled WebAssembly

**Validation:**

- Documentation covers all language features
- Formatter round-trips all valid programs
- Error messages are reviewed for clarity

---

## Summary Timeline

| Milestone | Description | Depends on |
|-----------|-------------|------------|
| M0 | Lexer and parser | — |
| M1 | Name resolution and scope analysis | M0 |
| M2 | Type system — core | M1 |
| M3 | Type system — traits, generics, and monomorphization | M2 |
| M4 | **WASM codegen — fundamentals ("wasl runs")** | M3 |
| M5 | **WASM codegen — GC types ("wasl has real types")** | M4 |
| M6 | WASM codegen — closures and function references | M5 |
| M7 | WASM codegen — tail calls and exception handling | M5 |
| M8 | WASM codegen — unsafe, SIMD, and linear memory | M5 |
| M9 | Module linking | M4 |
| M10 | Standard library | M6, M9 |
| M11 | Polish, tooling, and documentation | all |

M6, M7, and M8 are independent of each other and can be developed in parallel after M5.

---

## Open Questions

1. **SIMD syntax.** The lane-typed SIMD syntax must be visually distinct from fixed-length array syntax. Candidates include `simd<f32, 4>`, built-in named types (`f32x4`), or another syntax. This should be resolved during M0 grammar work.

2. **Concurrency.** WebAssembly has no built-in concurrency model yet. The `shared-everything-threads` proposal is in development. wasl should be ready for it but doesn't need to design around it until the proposal stabilizes. For now, wasl modules are single-threaded.

3. **Package management.** wasl can distribute as `.wasm` files, but a real ecosystem needs dependency resolution. This is a post-1.0 concern. The initial distribution story is manual: copy `.wasm` files and declare interfaces via `extern`.

4. **Debugging.** WebAssembly's DWARF support is improving but still immature. wasl should emit name sections and, eventually, DWARF-compatible debug info so that source-level debugging is possible in tools that support it. This is a post-1.0 concern but the codegen should preserve enough information to support it later.

5. **Component model integration timeline.** The component model and WASI 0.3 are stabilizing in parallel with wasl's development. The language should be ready to adopt component-level packaging when the ecosystem matures, but the core language and toolchain must not depend on it. The `extern` syntax is designed to extend naturally to WIT interfaces when the time comes.
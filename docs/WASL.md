# wasl: Language Design Document

**Version:** 0.1
**Date:** April 2026
**Author:** Grant Wade

---

## High-Level Map

```text
                               +----------------------+
                               |    Host / Embedder   |
                               | browser, Wasmtime,   |
                               | V8, WAMR, custom app |
                               +----------+-----------+
                                          |
                              provides imports / loads module
                                          |
                 +------------------------v------------------------+
                 |          Standard WebAssembly Runtime           |
                 | GC | typed funcref | exceptions | SIMD | tables |
                 | multi-memory | Memory64 | bulk memory | globals |
                 +------------------------+------------------------+
                                          |
                              executes standard output
                                          |
          +-------------------------------+-------------------------------+
          |                                                               |
 +--------v---------+                                           +---------v----------+
 |  Core .wasm      |                                           | Optional Component |
 | default output   |                                           | wrapper + WIT      |
 +--------+---------+                                           +---------+----------+
          ^                                                               ^
          |                           emitted by                          |
          +-------------------------------+-------------------------------+
                                          |
                                +---------v---------+
                                |       waslc       |
                                | parse -> type ->  |
                                | traits -> mono -> |
                                | DCE -> Wasm code  |
                                +----+----+----+----+
                                     |    |    |
                 compile-time only --+    |    +-- optional metadata
                                          |
          +-------------------------------+-------------------------------+
          |                               |                               |
 +--------v---------+           +---------v---------+           +---------v---------+
 | app.wasl         |           | imports resolved  |           | foreign bindings  |
 | user program     |           | at compile time   |           | or host signatures|
 +------------------+           +---------+---------+           +-------------------+
                                          |
           +------------------------------+------------------------------+
           |                              |                              |
 +---------v---------+          +---------v---------+          +---------v---------+
 | wasl:* stdlib     |          | from "foo.wasl"   |          | from "foo.wasm"   |
 | bundled source    |          | source import     |          | concrete exports  |
 +-------------------+          +-------------------+          +-------------------+


    Language model inside the emitted core module:

    - Structured data lives on the GC heap by default.
    - Raw bytes live in explicit linear memories.
    - Functions, tables, globals, and exceptions map directly to Wasm spaces.
    - Generics disappear during monomorphization; exports are concrete.
    - wasl-to-wasl composition happens at compile time, not at runtime.
```

## Overview

wasl is a programming language designed around WebAssembly. It is not a language that *targets* WebAssembly as a compilation backend; it is a language whose type system, memory model, and execution semantics are direct expressions of what WebAssembly provides. Where other languages fight WebAssembly's constraints or paper over them with runtime shims, wasl embraces them as the foundation of its design.

The language compiles to standard `.wasm` binaries. These binaries run on any compliant WebAssembly runtime, including V8, Wasmtime, wasm-micro-runtime, or any other engine that supports the required proposals. wasl does not depend on a custom VM, a special loader, or a language-specific runtime library.

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
- **Go, Python, JavaScript** require massive runtime shims, including garbage collectors, interpreters, or JITs compiled to WebAssembly running on top of WebAssembly.
- **AssemblyScript** is the closest to "designed for Wasm" but remains TypeScript-flavored and doesn't leverage the full modern proposal surface.

None of these languages were designed with WebAssembly's actual type system in mind. The GC proposal gives WebAssembly real structs, arrays, reference types, and subtyping. Typed function references give it safe indirect calls. Exception handling gives it structured control flow for errors. SIMD gives it data parallelism. These aren't compilation targets to be lowered into; they're language features waiting for syntax.

### The opportunity

wasl is the language that treats WebAssembly's type system as its own. A struct in wasl *is* a WebAssembly GC struct. Pattern matching on a variant *is* a `br_on_cast`. A tail-recursive call *is* a `return_call`. There is no impedance mismatch between the source language and the execution model.

This has concrete consequences:

- **No custom allocator or GC runtime.** Memory management uses WebAssembly's built-in GC. The host runtime handles collection natively.
- **No representation overhead.** A wasl record is a GC struct, not a C struct pretending to be in linear memory, not a JavaScript object serialized into bytes.
- **Predictable codegen.** A wasl programmer who understands WebAssembly can predict what their code compiles to. There are no hidden costs.
- **Full proposal surface.** SIMD, tail calls, exception handling, multiple memories, and Memory64 all have first-class syntax. You don't drop to inline assembly or compiler intrinsics to use them.
- **Concrete exports for free.** Monomorphized generics mean every exported type is a concrete GC struct. Any consumer, whether JavaScript, Rust, C, or another wasl module, sees plain types with no need to understand wasl's generics encoding.
- **No runtime lock-in.** wasl output is standard WebAssembly. Run it on V8 in a browser, Wasmtime on a server, a microcontroller runtime, or any compliant engine. The language doesn't care.

### Why now

WebAssembly GC reached Phase 4 (standardized) and shipped in major engines in late 2023. Exception handling stabilized in early 2024. The post-MVP proposal surface is now rich enough to support a real language without falling back to linear memory for everything. The platform is ready for a language designed around it rather than merely compiled to it.

---

## Design Philosophy

### First principles

1. **WebAssembly is the semantic model, not the compilation target.** Every language feature must have a direct, obvious mapping to WebAssembly constructs. If it can't be expressed cleanly in WebAssembly, it doesn't belong in wasl.

2. **Two memory worlds, both first-class.** The GC heap is the default for structured data. Linear memory is available through typed memory declarations for structured byte access, with the GC heap as the default for all structured data. Neither is an afterthought.

3. **Monomorphized generics over GC types.** Parametric polymorphism in the source, concrete GC struct types in the output. No boxing, no uniform representation, no runtime type dispatch. Code size is the tradeoff; predictable performance and clean interop are the payoff.

4. **Expression-oriented, ML-flavored.** Functions are expressions. Match arms are expressions. Blocks evaluate to their last expression. The syntax draws from the ML family, including algebraic data types, pattern matching, type inference, and `let` bindings, because that family maps cleanly to tagged GC structs with subtyping.

5. **Errors are values and exceptions.** Expected failures return `Result<T, E>`. Unexpected failures throw typed exception tags. Both map directly to WebAssembly constructs (`br_on_cast` for result matching, `try_table`/`throw_ref` for exceptions). The language doesn't force one error model.

6. **Explicit over implicit at boundaries.** Exports are marked `pub`. Imports are declared with `import` and accessed through namespace syntax. Nothing is automatically visible across module boundaries.

7. **Runtime-agnostic.** wasl output is standard WebAssembly. The language specification does not assume any particular runtime, embedding API, or host environment. A wasl module is a `.wasm` file, and the embedder decides how to load and run it.

### What wasl is not

- **Not a systems language.** wasl exposes linear memories and allocation strategies directly, but the default path is GC-managed. If you need `malloc`/`free` everywhere, write or import an allocator yourself, or write C and compile it to WebAssembly.
- **Not a scripting language.** wasl is statically typed, compiled ahead of time, and has no runtime eval or dynamic dispatch beyond WebAssembly's own `call_indirect`.
- **Not a WebAssembly assembler.** wasl is a high-level language with type inference, generics, and pattern matching. It is informed by WebAssembly's semantics, not a textual encoding of them.
- **Not tied to WASI.** wasl modules communicate with the host through explicit imports. WASI is one possible set of imports, not a requirement. A wasl module can import nothing, import custom host functions, or import full WASI interfaces; it's the embedder's choice.
- **Not dependent on the Component Model.** wasl supports the Component Model as an optional output mode for ecosystem interop (`waslc build --component --wit`), but does not require it. wasl-to-wasl composition is resolved at compile time through source imports. Direct foreign module interop uses explicit marshaling. The Component Model is available for broader ecosystem interop, not a requirement.

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

- `.byte_at(n)`: O(1) access to the nth byte. Returns `u8`.
- `.codepoint_at(n)`: O(n) access to the nth Unicode codepoint. Returns `Result<char, Error>`. The name makes the cost visible.
- `.codepoints()`: Returns an iterator over `char` values. This is the primary way to process string contents.
- `.bytes()`: Returns an iterator over `u8` values.
- `.len()`: Byte length (O(1)).
- `.codepoint_len()`: Codepoint count (O(n)). Name signals the cost.

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

Fixed-length arrays with known size at compile time can be optimized to GC structs or used as fixed layouts for typed memory declarations.

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

This is a deliberate interop advantage: every exported type is a concrete GC struct that any consumer can use without understanding wasl's generics. The code size cost is acceptable because runtimes like Go and C# already embed far larger runtime payloads in their WebAssembly output.

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

SIMD types are built-in named types that map directly to WebAssembly's fixed-width `v128` lane shapes. wasl does not use parameterized syntax such as `simd<f32, 4>`, because that suggests a degree of variability that standard WebAssembly SIMD does not expose. The syntax should reflect the VM directly.

```
f32x4, f64x2, i8x16, i16x8, i32x4, i64x2
```

These are primitive type names, not user-definable aliases. SIMD operations are namespaced by lane type: `simd.f32x4.mul(a, b)`, `simd.i16x8.add(a, b)`. The compiler enforces lane-type consistency at call sites.

### Typed memories

wasl exposes linear memory through explicit memory declarations. Each declaration emits a separate WebAssembly memory, so isolation is a VM guarantee provided by the multiple memories proposal, not compiler analysis.

Layouts remain compile-time descriptions of byte offsets and alignments:

```
layout RGBA {
    r: u8,   // offset: 0, align: 1
    g: u8,   // offset: 1, align: 1
    b: u8,   // offset: 2, align: 1
    a: u8,   // offset: 3, align: 1
}

layout Header {
    magic: u32,    // offset: 0, align: 4
    version: u16,  // offset: 4, align: 2
    flags: u16,    // offset: 6, align: 2
    size: u64,     // offset: 8, align: 8
}
```

Memories can be fixed, layout-bound, strategy-backed, or raw:

```wasl
memory framebuffer: mem32 as [RGBA; 1920 * 1080]
memory packet_buf:  mem32 as Header
memory arena:       mem32, strategy = bump
memory raw:         mem32
```

Fixed memories bind a memory to a compile-time layout or array shape. The compiler computes the required byte size and page count up front, then emits a dedicated memory sized for that declaration.

Layout-bound memories expose typed access. `framebuffer[i]` knows the `RGBA` stride, and `packet_buf.version` knows the `Header` offset. These operations still lower to ordinary loads and stores; the layout only supplies compile-time structure.

Strategy-backed memories pair a memory with an allocation strategy. `strategy = bump` is the built-in default. It inlines as a watermark global plus `memory.grow` when needed, and exposes `arena.alloc(Layout)` and `arena.reset()`.

The strategy system is open-ended. `bump` is the initial built-in, but the surface is designed so other strategies can be added later without changing the memory model itself.

Raw memories remain available for direct `memory.load` and `memory.store` access with plain `i32` or `i64` indices, depending on the memory kind.

Static bytes for linear memory are declared as data segments, and bulk memory operations work against any declared memory:

```
data HELLO_WORLD = "Hello, World!\n"
```

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

This follows the ML tradition of immutable-by-default for safety. The `let mut` convention mirrors Rust's syntax, which is widely understood. At the WebAssembly level, all locals are mutable; the immutability constraint is enforced by the compiler.

### Multi-value returns

Functions can return multiple values, mapping directly to WebAssembly's multi-value:

```
fn divmod(a: i32, b: i32) -> (i32, i32) = (a / b, a % b)

let (q, r) = divmod(17, 5)
```

### Control Flow

wasl provides standard control flow that lowers predictably into WebAssembly's native structured control flow (`block`, `loop`, `br`, `br_if`) without requiring the developer to think about Wasm's label indices.

**Early Return:**

Functions can exit early using `return`. (This maps directly to WebAssembly's `return` instruction or a branch to the outermost function block).

```wasl
fn check(x: i32) -> i32 = {
    if x < 0 then return -1
    x * 2
}
```

**While and For Loops:**

Loops use familiar `while` and `for` syntax, supporting `break` and `continue`.

```wasl
let mut i = 0
while i < 10 {
    if arr[i] == 0 then break
    i = i + 1
}
```

Under the hood, the compiler lowers a `while` loop into WebAssembly's standard pattern: an outer `block` (to act as the target for `break`) containing a `loop` (to act as the target for `continue`).

**Labeled Breaks:**

To break out of nested loops, wasl supports label annotations rather than raw Wasm block manipulation. This behaves like JavaScript or Rust, but maps perfectly to WebAssembly's multi-level `br` instructions.

```wasl
'outer: for row in grid {
    for cell in row {
        if cell == TARGET then break 'outer
    }
}
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

The `tail` keyword is required. The compiler verifies the call is in tail position and rejects `tail` on non-tail calls. This is not an optimization hint; it is a semantic guarantee that the call uses constant stack space.

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

Guards (`if` clauses) are supported. Guarded arms do not count toward exhaustiveness; a catch-all or complete unguarded coverage is still required.

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

A `?` operator propagates errors. Using `?` in a function that does not return `Result<T, E>` is a compile error.

```
fn load_config(path: string) -> Result<Config, Error> = {
    // assumes: import "wasl:io"
    let text = io.read_file(path)?
    let parsed = parse_toml(text)?
    validate(parsed)
}
```

**Exception tags** for unexpected, non-local errors. These compile to WebAssembly exception handling (`try_table` / `throw_ref`):

```
// assumes: import "wasl:io"
exception DivByZero
exception OutOfBounds { index: i32, len: i32 }

fn safe_div(a: i32, b: i32) -> i32 =
    if b == 0 then throw DivByZero
    else a / b

fn main() =
    try {
        let x = safe_div(10, 0)
        io.print(x)
    } catch {
        DivByZero -> io.print("division by zero"),
        OutOfBounds { index, len } -> io.print("oob"),
    }
```

Exception tags are nominal types declared at the module level. They map directly to WebAssembly tag declarations in the tag section.

The two error mechanisms are fully independent. If a function returns `Result<T, E>` and an exception is thrown inside it, the exception propagates; it is not automatically caught and wrapped in `Err`. This keeps the semantics clean: `Result` is for expected failures you handle locally, exceptions are for unexpected failures that escape.

### The Unreachable Trap

WebAssembly's `unreachable` instruction is exposed as a first-class keyword. It acts as a bottom type (it can coerce to any expected type). It does not throw a catchable exception; it halts the VM with a trap.

```wasl
fn strict_math(x: i32) -> i32 =
    match x {
        0 -> 0,
        1 -> 1,
        _ -> unreachable, // VM trap if hit
    }
```

### Linear memories and GC interaction

Linear memory access is a regular part of wasl. WebAssembly already enforces the hard boundary: memories are isolated, out-of-bounds accesses trap, GC references cannot be stored in linear memory, and byte memories do not turn into GC references by pointer tricks.

#### Raw memory sizing

```wasl
fn request_more_memory(pages: i32) -> i32 = {
    memory.grow(raw, pages)
}

fn current_size() -> i32 = {
    memory.size(raw)
}
```

`memory.grow` returns the previous page count and `-1` on failure, matching WebAssembly exactly.

#### Typed memory access

```wasl
fn write_pixel(i: i32, r: u8, g: u8, b: u8, a: u8) = {
    framebuffer[i].r = r
    framebuffer[i].g = g
    framebuffer[i].b = b
    framebuffer[i].a = a
}

fn read_magic() -> u32 = {
    packet_buf.magic
}

fn check_version() -> bool = {
    packet_buf.version == 2
}
```

Typed memories compile to the same underlying loads and stores as raw memory operations. The difference is that the declaration supplies the compiler with stride, field offset, and bounds structure ahead of time.

#### Strategy-backed memories

```wasl
fn reserve_header() -> i32 = {
    arena.alloc(Header)
}

fn reset_arena() = {
    arena.reset()
}
```

`strategy = bump` lowers to a watermark global plus `memory.grow` when allocation crosses the current capacity.

#### Bulk memory and data segments

```wasl
fn init_buffer(ptr: i32) = {
    memory.init<HELLO_WORLD>(raw, ptr, 0, HELLO_WORLD.len)
}

fn zero_buffer(ptr: i32, len: i32) = {
    memory.fill(raw, ptr, 0, len)
}

fn shift_buffer(src: i32, dest: i32, len: i32) = {
    memory.copy(raw, dest, src, len)
}
```

#### GC reference pinning

When a GC reference needs to persist across linear-memory interactions, for example when registering a callback with a C-compiled module, the `gc.pin` / `gc.unpin` mechanism provides controlled escape:

```
fn register_callback(cb: fn(i32) -> i32) -> i32 = {
    let handle = gc.pin(cb)
    // handle is an i32 index into a runtime-managed table
    // store it in linear memory, pass to C module, etc.
    handle
}

fn release_callback(handle: i32) = {
    gc.unpin(handle)
}

fn invoke_callback(handle: i32, arg: i32) -> i32 = {
    let cb = gc.get(handle)  // retrieve the pinned ref
    cb(arg)
}
```

Under the hood, `gc.pin` performs a `table.set` into a dedicated `funcref`/`externref` table, keeping the GC ref alive. `gc.unpin` clears the table slot. `gc.get` retrieves the ref via `table.get`. The handle is an `i32` table index.

This is not an escape hatch from the type system. It is syntax for the only mechanism WebAssembly provides for holding GC refs outside the GC heap. There is no pointer casting and no raw memory storage of refs. Tables are it, and `gc.pin`/`gc.unpin` is the API.

The programmer takes manual responsibility for the pinned ref's lifetime. An unpinned handle is a dangling reference. That tradeoff is explicit.

#### Safety boundary rules

- GC refs live on the GC heap and in GC locals or tables.
- Linear memories hold raw bytes.
- There is no implicit conversion between GC refs and linear-memory bytes; the VM enforces this structurally.
- `gc.pin` and `gc.unpin` remain the mechanism for holding GC refs across linear-memory interactions via table operations.

### Tables

WebAssembly tables are declared at the module level and accessible for indirect call dispatch:

```
table dispatch: [fn(i32) -> i32; 64]

// Declarative initialization (maps to WebAssembly Elem section)
elem dispatch[0] = [add, sub, mul, div]

// Dynamic initialization (maps to table.set)
fn register(idx: i32, f: fn(i32) -> i32) =
    table.set(dispatch, idx, f)

fn call_handler(idx: i32, arg: i32) -> i32 =
    table.get(dispatch, idx)(arg)
```

The `elem` declaration is evaluated at instantiation time. It maps directly to WebAssembly's Element section, avoiding the overhead of running `table.set` loops in a start function.

This compiles to `table.set`, `table.get`, `call_indirect`, and WebAssembly element segments. Tables are primarily useful for plugin systems, vtable-style dispatch, and interop with C-compiled modules that use indirect calls.

---

## Module System

### Exports and imports

Exports are marked with `pub`. Non-`pub` declarations are module-internal:

```
pub fn add(a: i32, b: i32) -> i32 = a + b
fn helper() -> i32 = 42   // not exported
```

### Globals

WebAssembly globals are a distinct index space, separate from linear memory and function locals. They are declared at the module level using the `global` keyword.

```wasl
global PI: f64 = 3.14159
global mut counter: i32 = 0
pub global mut config_flags: i32 = 0x01
```

Globals can be imported and exported. They map directly to the WebAssembly Global section. They are the idiomatic way to share single scalar values or GC references across a module without incurring linear memory load/store overhead.

### Imports

All external dependencies are declared with `import`. The string after `import` is the WebAssembly import module name. The last segment of the import path becomes the default namespace identifier used in source code.

**Namespace imports** pull in an entire module. Functions are accessed via `namespace.func()` syntax:

```wasl
import "wasl:io"
import "wasl:math"
import "physics" from "physics.wasl"

fn main() = {
    let x = math.sqrt(2.0)
    let vel = physics.integrate(body, dt)
    io.print("result: {x}")
}
```

**Aliased imports** override the default namespace name:

```wasl
import "wasl:io" as console
import "wasl:math" as m
import "network" from "net.wasl" as net

fn main() = {
    console.print(m.sqrt(2.0))
    net.connect("localhost")
}
```

**Selective imports** pull specific names into the local scope without a namespace prefix:

```wasl
import "wasl:math" { sqrt, PI }

fn main() = {
    let x = sqrt(PI)
}
```

In all three forms above, the programmer does not write type signatures. The compiler resolves types from the appropriate source:

- For `wasl:*` imports, the compiler uses built-in interface definitions that ship with `waslc` (see **Standard Library** below).
- For `from` imports pointing to a `.wasl` file, the compiler parses and type-checks the source directly, making generic functions and types available for monomorphization at the importing module's call sites.
- For `from` imports pointing to a `.wasm` file, the compiler reads the export section and performs structural type checking. Only concrete (already-monomorphized) exports are available; cross-module generics require source access.

**Signature imports** are the only form where the programmer provides explicit type declarations. They are used when the compiler has no source of truth — typically for custom host bindings or foreign GC modules. Signature import blocks can contain `fn` declarations, `type` declarations (to name foreign GC types for structural matching), and `memory` declarations (to import a foreign module's linear memory for marshaling):

```wasl
import "my_host" {
    fn custom_callback(x: i32) -> i32
    fn get_timestamp() -> f64
}
```

These signatures are trusted by the compiler. The host satisfies them at instantiation time. When combined with a `from` clause pointing to a `.wasm` file, the compiler structurally verifies the declarations against the module's export section.

**Summary of import forms:**

| Form | Access style | Type source |
|------|-------------|-------------|
| `import "wasl:io"` | `io.print()` | Compiler built-in definitions |
| `import "wasl:io" as console` | `console.print()` | Compiler built-in definitions |
| `import "wasl:io" { print }` | `print()` | Compiler built-in definitions |
| `import "foo" from "foo.wasl"` | `foo.func()` | Source (parsed and type-checked by compiler) |
| `import "foo" from "foo.wasm"` | `foo.func()` | Wasm export section (structural, concrete only) |
| `import "foo" from "foo.wasm" { type T..., fn sig... }` | `foo.func()` | Programmer-provided, structurally verified |
| `import "my_host" { fn sig... }` | `func()` | Programmer-provided (unverified) |

The `from` clause is a compile-time hint only. It tells `waslc` where to find signatures to verify against, but has no effect on the emitted WebAssembly binary — the import section always uses the module name string. The host linker decides what each module name resolves to at instantiation time.

The `from` path is resolved by the toolchain relative to the importing file. Resolution order is: literal relative path, sibling directory, then any configured dependency directories. The compiler does not embed file paths in the output binary.

Namespace and selective imports can be combined with `from`:

```wasl
import "physics" from "physics.wasl"           // physics.integrate()
import "utils" from "utils.wasl" as u          // u.clamp()
import "geo" from "geo.wasl" { distance, area } // distance(), area()
```

This is the only module system construct. wasl does not have a `use` statement that references filenames, because a WebAssembly module cannot load other modules — that is always the embedder's responsibility. The `import` statement bridges developer ergonomics with that constraint by giving the compiler enough information to verify types at build time while leaving runtime linking to the host.

### Source Import Compilation Model

When a `from` clause points to a `.wasl` file, the compiler does not treat it as a binary dependency — it treats it as source. `waslc` parses and type-checks the imported `.wasl` file, making all of its `pub` declarations (including generic functions and types) available to the importing module. Generic functions are monomorphized at the importing module's call sites, and dead-code elimination removes anything unused. The result is a single `.wasm` binary containing all the code the module needs, with no wasl-level imports remaining for source-imported dependencies.

This is the same model used for the standard library: `wasl:*` imports resolve to bundled `.wasl` source, compiled alongside the user's code. The `from "foo.wasl"` form generalizes this to user-defined libraries.

The key consequence: **wasl-to-wasl composition is fully resolved at compile time.** The output is a standard WebAssembly core module with concrete types and concrete functions. No custom linking protocol, no special runtime support, no multi-module wiring. Any compliant WebAssembly host can load and run the result.

For imported `.wasl` files, the compiler caches the typed AST to avoid redundant parsing and type-checking across multiple compilation units that share the same dependency.

### wasl.metadata Custom Section

When `waslc` compiles a module, it optionally injects a custom section named `"wasl.metadata"`. This section is lightweight tooling metadata — it is not load-bearing for linking or runtime behavior. Unrecognized custom sections are ignored by the WebAssembly specification, so wasl `.wasm` files remain standard core modules runnable on any compliant engine.

The section records:

- **Compiler version:** Which version of `waslc` produced the binary.
- **Standard library dependencies:** Which `wasl:*` namespaces and versions the module was compiled against. This allows the `wasl` runner to verify that its host-provided bindings (e.g., `wasl:io`) are compatible with what the module expects.
- **Source-level type names:** Original wasl type names for exported GC structs, useful for debugging, inspection tools, and documentation generation.

The section does **not** encode import/export interfaces for linking. Multi-module composition at runtime is the host's responsibility, using standard WebAssembly import/export matching. wasl does not define its own linking protocol.

### Foreign Module Interop

wasl modules interop with non-wasl WebAssembly modules at two levels, depending on whether the foreign module uses GC types.

**GC-aware foreign modules** (e.g., Kotlin/Wasm, Dart/Wasm) emit GC structs and typed function references. The WebAssembly VM enforces structural type compatibility at the boundary. wasl can call these modules and receive GC references from them, but only structural checking is possible — there is no nominal type safety across the boundary.

Signature imports for foreign GC modules can include `type` declarations to give wasl-level names to the foreign module's structural GC types:

```wasl
import "kotlin_geo" from "geo.wasm" {
    type ForeignPoint = { x: f64, y: f64 }
    fn distance(a: ForeignPoint, b: ForeignPoint) -> f64
}

// ForeignPoint is now usable as a wasl type, structurally matched
// against the GC struct exported by the foreign module
let p = kotlin_geo.distance(a, b)
```

The compiler reads the `.wasm` export section and verifies structural compatibility with the declared types.

**Linear-memory-only foreign modules** (e.g., C, Rust, Go compiled to Wasm) export functions with scalar types (`i32`, `i64`, `f32`, `f64`) and use linear memory for all structured data. Interop requires explicit marshaling between the GC heap and the foreign module's linear memory.

wasl provides two built-in operations for this:

- `memory.copy_from_gc(mem, ptr, gc_array, offset, len)` — copies bytes from a GC byte array into a linear memory. Lowers to `array.get` + store loops.
- `memory.copy_to_gc(mem, ptr, len) -> [u8]` — copies bytes from a linear memory into a new GC byte array. Lowers to load + `array.new`/`array.set` loops.

Example of wrapping a C-compiled zlib module:

```wasl
import "zlib" from "zlib.wasm" {
    memory heap: mem32

    fn _compress(dest: i32, dest_len: i32,
                 src: i32, src_len: i32, level: i32) -> i32
}

fn compress(data: [u8], level: i32) -> Result<[u8], Error> = {
    let src_len = data.len()
    let dest_len = src_len + 128
    let src_ptr = zlib.heap.alloc(src_len)
    let dest_ptr = zlib.heap.alloc(dest_len)

    memory.copy_from_gc(zlib.heap, src_ptr, data, 0, src_len)

    let result = zlib._compress(dest_ptr, dest_len, src_ptr, src_len, level)

    if result != 0 then Err(Error("zlib failed"))
    else {
        let out = memory.copy_to_gc(zlib.heap, dest_ptr, dest_len)
        Ok(out)
    }
}
```

The marshaling cost is explicit and visible. wasl does not hide it behind an abstraction layer.

### Component Model Support

wasl supports the WebAssembly Component Model as an optional output mode for ecosystem interop. It is not required for wasl-to-wasl composition or for direct foreign module interop.

```sh
# Default: core module
waslc build app.wasl -o app.wasm

# Component Model wrapper with canonical ABI
waslc build app.wasl -o app.wasm --component --wit app.wit
```

The `--component` flag wraps the core module in a component envelope with canonical ABI lifting and lowering shims generated from the provided WIT file. The core codegen is identical in both modes.

wasl modules have two interop paths:

1. **Core Wasm interop:** Foreign module, no Component Model. For GC-aware foreign modules, the VM enforces structural type compatibility directly. For linear-memory-only modules, explicit marshaling with `memory.copy_from_gc`/`memory.copy_to_gc`.
2. **Component Model:** Full WIT-based interop with the broader Wasm ecosystem. Canonical ABI handles lifting and lowering automatically.

wasl-to-wasl composition does not appear in this list because it is resolved at compile time through source imports, not at runtime through module linking.

### Standard Library

wasl ships a versioned standard library accessible through the `wasl:` import prefix. Standard library namespaces are a blend of pure wasl source (compiled alongside user code) and host-provided native functions. The split is an implementation detail invisible to the programmer.

```wasl
import "wasl:io"
import "wasl:math"
import "wasl:collections"

fn main() = {
    let items = collections.sort(my_list)
    let mag = math.sqrt(2.0)
    io.print("done: {mag}")
}
```

**Built-in namespaces (initial set):**

- `wasl:math` — math functions and constants implementable in pure WebAssembly (`abs`, `min`, `max`, `clamp`, `floor`, `ceil`, `sqrt` via `f64.sqrt`, `sin`, `cos`, `pow`, `PI`, `E`). Pure wasl, no host imports.
- `wasl:io` — basic I/O (`print`, `read_line`, `read_file`, `write_file`). Host-provided by the runner.
- `wasl:collections` — collection operations (`sort`, `map`, `filter`, `fold`, `find`, `zip`). Pure wasl operating on GC arrays. `sort` uses the `Ord` trait.
- `wasl:option` / `wasl:result` — utility functions for `Option<T>` and `Result<T, E>` (`map`, `and_then`, `unwrap_or`, etc.). Pure wasl.
- `wasl:fmt` — string formatting and interpolation support. Pure wasl.
- `wasl:string` — string manipulation (`split`, `join`, `trim`, `contains`, `starts_with`, `replace`). Pure wasl.

**Compile-time verification:** `waslc` ships with built-in interface definitions for every `wasl:*` namespace at its version. When the compiler encounters a `wasl:` import, it resolves function names and types from these definitions. If a function doesn't exist in the namespace, or is used with the wrong types, it is a compile error. No `from` clause is needed — the `wasl:` prefix is the signal.

**Compilation model:** The standard library ships as `.wasl` source files bundled with `waslc`. When the compiler encounters a `wasl:*` import, it resolves the namespace to the corresponding source file in its stdlib directory, compiles it alongside the user's code, monomorphizes any generic functions at the user's call sites, and performs dead-code elimination. The result is a single `.wasm` binary containing only the stdlib functions actually used. There are no pre-compiled stdlib `.wasm` modules for the pure wasl portions.

Host-provided portions of the standard library (e.g., `wasl:io`) are not compiled from source — the runner provides these as native bindings at instantiation time. The `wasl.metadata` section in the output module records which host bindings are required.

**Versioning:** Standard library namespaces are versioned with the wasl language version. A module compiled with `waslc` v0.2 records that it depends on `wasl:io` v0.2 in its `wasl.metadata` section. The compiler bundles the stdlib source matching its version. Since pure wasl stdlib code is compiled into the user's binary, there is no runtime version mismatch for those portions. For host-provided bindings (`wasl:io`, etc.), the runner must support the version recorded in the module's `wasl.metadata` section:

- Old modules on new runners: always works. The runner supports all previous versions.
- New modules on old runners: the runner detects the version mismatch and reports a clear error ("this module requires wasl:io v0.2 but this runner supports v0.1").

Some namespaces may contain a mix of pure wasl and host functions. For example, a future `wasl:fs` might include `path_join` as pure wasl string manipulation compiled from source alongside the user's code, and `read_dir` as a host binding provided by the runner. The boundary can shift between versions — a function that was host-provided in v0.1 could become pure wasl in v0.2 — without breaking user code.

Because the stdlib is source-distributed, developers can read the implementations, learn idiomatic wasl, and contribute improvements. The stdlib source serves as both a library and a reference codebase for the language.

**Opt-in, not mandatory:** The standard library is not linked unless imported. Modules that import nothing from `wasl:*` have no standard library dependency. Embedders who provide their own host functions can ignore the standard library entirely.

---

## Toolchain

### Architecture

```
waslc (compiler)     →  .wasm core modules
wasl  (runner)       →  compile + execute (delegates to any wasm runtime)
```

**`waslc`** is the compiler. It is a native binary written in C. It takes `.wasl` source files and produces standard `.wasm` binaries. It performs lexing, parsing, type checking, type inference, monomorphization, and WebAssembly codegen. It does not require an external toolchain, runtime, or code generation step.

**`wasl`** is a convenience runner. It delegates execution to an underlying WebAssembly runtime. When running a wasl module, it reads the `"wasl.metadata"` custom section to verify host binding compatibility (e.g., checking that the runner supports the `wasl:io` version the module was compiled against), provides the required host bindings, and kicks off execution.

### Compilation pipeline

```
.wasl source
    │
    ├── Lexer → Token stream
    ├── Parser → AST
    ├── Name resolution → Scoped AST (resolves import namespaces, loads stdlib source,
    │                      parses and type-checks `from "*.wasl"` dependencies)
    ├── Type checking / inference → Typed AST
    ├── Trait resolution → Resolved AST
    ├── Monomorphization → Specialized AST (no generics, stdlib and source imports included)
    ├── Dead-code elimination → Pruned AST
    └── WASM codegen → .wasm binary (with optional wasl.metadata custom section)
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

1. **Source** (`.wasl` files): requires `waslc` to build.
2. **WebAssembly** (`.wasm` file): runs on any WebAssembly runtime that supports the required proposals. Portable, sandboxed, no native dependencies.

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
- Lexer: tokenize wasl source including keywords (`fn`, `let`, `mut`, `match`, `if`, `then`, `else`, `pub`, `tail`, `import`, `from`, `as`, `type`, `trait`, `impl`, `exception`, `throw`, `try`, `catch`, `memory`, `layout`, `data`, `table`, `elem`, `global`, `while`, `for`, `in`, `break`, `continue`, `return`, `unreachable`, `simd`, `strategy`), label identifiers (e.g., `'label`), built-in SIMD type names (`f32x4`, `f64x2`, `i8x16`, `i16x8`, `i32x4`, `i64x2`), operators, literals (integers, floats, strings with interpolation, chars), identifiers, and delimiters
- Parser: recursive descent, producing a complete AST for type declarations (records, algebraic types, enums, tuples, layouts), trait declarations and implementations, function declarations (single-expression and block bodies), exception declarations, pattern match expressions, if/then/else, while loops, for loops, break/continue with optional labels, early returns, let/let mut bindings, closures / lambda expressions, import declarations (namespace, aliased, selective, and signature forms, with optional `from` clause), pub/tail modifiers, SIMD expressions and type references, table declarations, memory declarations with optional layout or strategy bindings, and data declarations
- Error reporting: source location tracking, clear error messages with line/column
- Pretty-printer: AST → wasl source round-trip for debugging

**Validation:**

- Parse the full language sketch examples from this document
- Round-trip: parse → pretty-print → parse produces identical AST
- Error recovery: malformed input produces useful diagnostics, not crashes

### Milestone 1: Name Resolution and Scope Analysis

**Goal:** Resolve all identifiers to their declarations, build scope chains, detect errors.

**Deliverables:**

- Module-level scope: type declarations, trait declarations, function declarations, import declarations, exception declarations, memory/table declarations
- Import namespace resolution: resolve `import "wasl:math"` to namespace `math`, handle aliased imports (`as`), and resolve selective imports to local scope. For `from "*.wasl"` imports, parse and type-check the target source file (caching the typed AST for reuse). Verify that namespace-qualified references (e.g., `math.sqrt`) resolve to valid names in the imported module.
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

### Milestone 2: Type System: Core

**Goal:** Type check all expressions and declarations; infer types for let bindings and closures.

**Deliverables:**

- Bidirectional type checking: propagate expected types downward, synthesize types upward
- Type inference for let bindings: `let x = 42` infers `i32`
- Type inference for closures: `|x| x + 1` infers `fn(i32) -> i32` from context
- Literal typing: integer literals default to `i32`, float literals default to `f64`, with contextual widening to `i64` / `f32` where expected
- Record, algebraic type, and enum type checking
- Tuple type checking
- Function signature verification: argument types, return type, tail call position validity
- Namespace-qualified call resolution: type-check `math.sqrt(x)` by resolving `sqrt` through the `math` namespace's known signatures (from built-in definitions, source imports, or export section)
- `?` operator checking: verify that `?` is only used inside functions returning `Result<T, E>`, and that the error types are compatible
- Memory reference checking: verify that linear-memory operations reference declared memories
- Pattern exhaustiveness checking
- Basic type error messages: expected/got with source location

**Validation:**

- All example programs from this document type-check successfully
- Exhaustiveness checker rejects incomplete matches
- Linear-memory operations that reference undeclared memories produce compile errors
- Type errors produce clear, actionable messages

### Milestone 3: Type System: Traits, Generics, and Monomorphization

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
- Recursive generic types: `type Tree<T> = | Leaf(T) | Node { left: Tree<T>, right: Tree<T> }`; the monomorphizer handles these without infinite expansion by detecting recursion through the same type parameters
- Error messages that refer to the original generic source, not the monomorphized output
- Missing trait implementation errors: clear message when a type doesn't implement a required trait

**Validation:**

- `Option<i32>` and `Option<string>` produce distinct monomorphized types
- `sort<i32>` resolves `Ord` for `i32` and monomorphizes correctly
- Recursive types like `Tree<i32>` monomorphize correctly
- Unused specializations are absent from the output
- Missing `impl Ord for MyType` when calling `sort<MyType>` produces a clear error

### Milestone 4: WASM Codegen: Fundamentals

**Goal:** Emit valid `.wasm` binaries for a core subset of wasl: functions, arithmetic, control flow, let bindings.

**Deliverables:**

- WASM binary encoder: produce valid `.wasm` files with type section, function section, code section, export section, import section
- Codegen for primitive expressions: integer/float arithmetic, comparisons, boolean logic
- Codegen for let bindings (mutable and immutable): WebAssembly locals
- Codegen for module-level `global` and `global mut`: WebAssembly Global section and `global.get`/`global.set` instructions.
- Codegen for if/then/else: `if` instruction or `br_if`
- Codegen for control flow: lower `while`, `for`, `break`, `continue`, and `return` into WebAssembly `block`, `loop`, `br`, and `br_if` instructions. Resolve labeled breaks by calculating the correct relative label depth for WebAssembly branch instructions.
- Codegen for function calls: `call` instruction
- Codegen for `pub` functions: WebAssembly exports
- Codegen for `import` declarations: WebAssembly imports
- Codegen for multi-value returns: multiple result types on function signature
- Codegen for `unreachable`: emit the WebAssembly `unreachable` opcode.
- Output validation: produced `.wasm` files accepted by at least two independent runtimes and `wasm-tools validate`

**Validation:**

- `fn add(a: i32, b: i32) -> i32 = a + b` compiles and executes correctly
- `fn fib(n: i32) -> i32` compiles and produces correct results
- Imported host functions (via `import`) are called correctly

**This is the "wasl runs" milestone.**

### Milestone 5: WASM Codegen: GC Types

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
- String operations: `.byte_at()`, `.codepoint_at()`, `.codepoints()`, `.bytes()`, `.len()`, `.codepoint_len()`, concatenation, and interpolation, all bounds-checked
- Match guard codegen: conditional branches within match arms

**Validation:**

- `type Point = { x: f64, y: f64 }` creates a GC struct, fields are accessible
- `type Shape = | Circle { radius: f64 } | Rect { ... }` pattern matching works
- `Tree<i32>` recursive type with GC refs works (build tree, traverse, produce correct result)
- String iterator and `.codepoint_at()` produce correct results for multi-byte UTF-8

**This is the "wasl has real types" milestone.**

### Milestone 6: WASM Codegen: Closures and Function References

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

### Milestone 7: WASM Codegen: Tail Calls and Exception Handling

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

### Milestone 8: WASM Codegen: SIMD, Linear Memory, and Typed Memories

**Goal:** Emit typed memory declarations, linear-memory access, SIMD instructions, bulk memory operations, data segments, and the `gc.pin`/`gc.unpin` mechanism.

**Deliverables:**

- Memory declarations: emit separate WebAssembly memories for raw, fixed, layout-bound, and strategy-backed declarations (32-bit and 64-bit)
- Fixed typed memories: compute compile-time byte sizes and page counts from array shapes and layouts
- Layout-bound access codegen: lower indexed and field-based memory access into exact load/store sequences with known stride, offset, and alignment
- Strategy-backed memories: emit the built-in `bump` strategy as a watermark global with inline `alloc`, `memory.grow` when needed, and `reset`
- Raw memory sizing and growth: emit `memory.size` and `memory.grow` with WebAssembly-compatible results
- Bulk memory and data segments: emit data segments plus `memory.copy`, `memory.fill`, and `memory.init`
- `gc.pin` / `gc.unpin` / `gc.get`: emit `table.set` / `table.get` against a dedicated pinning table, manage slot allocation
- SIMD codegen: emit `v128` operations for SIMD expressions, lane-type checking enforced at compile time, map `simd.f32x4.mul` etc. to the corresponding WebAssembly SIMD instructions
- Table declarations: emit WebAssembly table section entries
- `elem` declarations: emit WebAssembly element section entries for static table initialization
- `table.get` / `table.set`: emit corresponding instructions

**Validation:**

- Fixed memories compute the correct page counts from their bound layouts
- Layout-bound header and framebuffer access emit the expected loads and stores with no GC allocations
- The built-in `bump` strategy allocates correctly, resets correctly, and grows memory when capacity is exceeded
- `memory.size` and `memory.grow` match WebAssembly behavior, including `-1` on failed growth
- Data segment initialization plus `memory.copy` and `memory.fill` produce the expected bytes
- `gc.pin` a closure, retrieve it via `gc.get`, invoke it, and get the correct result
- `gc.unpin` clears the table slot
- SIMD dot product produces correct results
- Memory64 addresses work for `mem64` memories
- Table-based indirect dispatch works
- Linear-memory operations that reference undeclared memories are rejected by the compiler

### Milestone 9: Foreign Module Interop and Component Model

**Goal:** Implement foreign module interop (non-wasl `.wasm` modules) and optional Component Model output. wasl-to-wasl composition is already handled at compile time through source imports; this milestone covers everything else.

**Deliverables:**

- `import` declarations emit WebAssembly imports with the appropriate module/name pairs for host and foreign module dependencies
- `from` clause resolution for `.wasm` files: `waslc` reads the target's export section and performs structural type checking against programmer-provided signatures
- GC-aware foreign module linking: structural type checking against the export section when linking against non-wasl GC modules (e.g., Kotlin/Wasm, Dart/Wasm)
- Linear-memory foreign module interop: `memory.copy_from_gc` and `memory.copy_to_gc` operations for marshaling data across the GC/linear-memory boundary
- Component Model output mode: `--component --wit` flag wraps the core module in a component envelope with canonical ABI shims
- `wasl.metadata` custom section: emit compiler version, stdlib version dependencies, and source-level type names for tooling
- Mismatched types between `import` signatures and actual foreign exports produce clear compile-time errors

**Validation:**

- wasl module A exports `pub fn foo()`, wasl module B declares `import "A" from "a.wasl"`, types checked at compile time, everything compiled into B's output binary
- wasl module declares `import "my_host" { fn get_timestamp() -> f64 }`, host wires it correctly at instantiation
- wasl module links against a C-compiled zlib `.wasm`, marshals data via `memory.copy_from_gc`, calls `compress`, unmarshals result via `memory.copy_to_gc`
- `waslc build --component --wit` produces a valid component that passes `wasm-tools validate --features component-model`
- Mismatched types between `import` and actual export produce a clear error identifying the mismatch

### Milestone 10: Standard Library

**Goal:** A versioned standard library providing common operations through the `wasl:` import namespace, blending pure wasl modules with runner-provided host bindings.

**Deliverables:**

- Standard library source: `.wasl` source files for all pure wasl namespaces, bundled with `waslc`
- `wasl:math`: `abs`, `min`, `max`, `clamp`, `floor`, `ceil`, `sqrt`, `sin`, `cos`, `pow`, `PI`, `E`
- `wasl:string`: pure wasl module implementing `split`, `join`, `trim`, `contains`, `starts_with`, `replace`, codepoint iteration utilities
- `wasl:collections`: pure wasl module implementing `map`, `filter`, `fold`, `sort`, `find`, `zip` over GC arrays; `sort` uses the `Ord` trait
- `wasl:option` / `wasl:result`: pure wasl utility functions (`map`, `and_then`, `unwrap_or`, etc.)
- `wasl:fmt`: pure wasl string formatting and interpolation support
- `wasl:io`: runner-provided host bindings for `print`, `read_line`, `read_file`, `write_file`
- Built-in trait implementations: `Eq`, `Ord`, `Show` for all primitive types
- Compiler integration: `waslc` resolves `wasl:*` imports to bundled source files, compiles them alongside user code, monomorphizes generics at user call sites, and performs dead-code elimination to emit only the stdlib functions actually used
- Built-in interface definitions: `waslc` ships with type definitions for all `wasl:*` namespaces, used for compile-time verification when the programmer uses namespace or selective import syntax without signatures
- Version embedding: `waslc` records the `wasl:*` version dependencies in the module's `wasl.metadata` custom section
- Runner integration: `wasl run` provides native host bindings for `wasl:io` and any other host-backed namespaces, with version checking against the module's recorded dependencies

**Validation:**

- All standard library functions have test coverage
- `import "wasl:math"` followed by `math.sqrt(2.0)` compiles and executes correctly without the programmer providing any signatures
- Selective imports (`import "wasl:math" { sqrt, PI }`) resolve correctly
- Aliased imports (`import "wasl:math" as m`) resolve correctly
- Generic stdlib functions (`collections.sort<MyRecord>`) monomorphize correctly at the user's call site
- Dead-code elimination: importing `wasl:collections` but only calling `sort` does not include `map`, `filter`, etc. in the output binary
- `wasl:io` host bindings work through the runner
- A module compiled with `waslc` v0.2 that requires `wasl:io` v0.2 host bindings fails with a clear error on a v0.1 runner
- A module that imports no `wasl:*` namespaces has no standard library code or dependency in its output
- Standard library source is readable and serves as idiomatic wasl reference code
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
| M0 | Lexer and parser | none |
| M1 | Name resolution and scope analysis | M0 |
| M2 | Type system: core | M1 |
| M3 | Type system: traits, generics, and monomorphization | M2 |
| M4 | **WASM codegen: fundamentals ("wasl runs")** | M3 |
| M5 | **WASM codegen: GC types ("wasl has real types")** | M4 |
| M6 | WASM codegen: closures and function references | M5 |
| M7 | WASM codegen: tail calls and exception handling | M5 |
| M8 | WASM codegen: SIMD, linear memory, and typed memories | M5 |
| M9 | Foreign module interop and Component Model | M5 |
| M10 | Standard library | M6, M9 |
| M11 | Polish, tooling, and documentation | all |

M6, M7, and M8 are independent of each other and can be developed in parallel after M5. M9 can also be developed in parallel with M6–M8.

---

## Open Questions

1. **Concurrency.** WebAssembly has no built-in concurrency model yet. The `shared-everything-threads` proposal is in development. wasl should be ready for it but doesn't need to design around it until the proposal stabilizes. For now, wasl modules are single-threaded.

2. **Package management.** wasl can distribute as `.wasl` source or `.wasm` binaries, but a real ecosystem needs dependency resolution. The `wasl:` prefix is resolved by the compiler without a `from` clause. For third-party dependencies, a package manager would map package identifiers to local paths before the compiler sees them. A natural extension would be a registry prefix (e.g., `import "pkg:json"`) that the package manager resolves to a local `.wasl` source package (enabling cross-module generics) or a precompiled `.wasm` (concrete exports only). The compiler should remain unaware of package registries; resolution should be pluggable through toolchain configuration. This is a post-1.0 concern. The initial distribution story is manual: copy `.wasl` files and point `from` at them.

3. **Debugging.** WebAssembly's DWARF support is improving but still immature. wasl should emit name sections and, eventually, DWARF-compatible debug info so that source-level debugging is possible in tools that support it. This is a post-1.0 concern but the codegen should preserve enough information to support it later.

4. **Allocation strategies.** The `strategy = bump` mechanism is the initial built-in. Pool allocation and other strategies are natural extensions. The design should keep the strategy system open-ended so new strategies can be added without language changes, either as built-in compiler-known strategies or as a trait/interface that user-defined allocators can implement.

5. **`arena.reset()` and use-after-reset.** The `bump` strategy's `reset()` operation invalidates all previous allocations from that arena. Accessing memory from a prior allocation after a reset is a use-after-free analog in linear memory. The language does not currently prevent this statically. Documenting the hazard explicitly and exploring lightweight static or dynamic guards (e.g., generation counters) is a post-1.0 concern.

6. **Source import compilation caching.** Both the stdlib and user `from "*.wasl"` imports ship as source and are compiled alongside the importing module. For large dependency trees, this adds compile time. A potential optimization is a cached typed AST artifact (e.g., `.wasli` files): `waslc` could cache the typed AST of imported modules after parsing and type-checking, skipping those phases on subsequent compilations and only re-running monomorphization and codegen for the importer's specific instantiations. The cache would be invalidated by source hash changes. This is a toolchain performance optimization, not a language design concern.
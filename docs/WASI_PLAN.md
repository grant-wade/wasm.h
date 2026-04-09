# wasi.h Implementation Plan

Single-header C99 library implementing the WebAssembly Component Model, Canonical ABI, and WASI interfaces. Depends on `wasm.h` for core module execution. Targets WASI 0.3 from the start and tracks the 0.3.x release train through 1.0 stabilization (late 2026 / early 2027).

**Baseline:** `wasm.h` passes the full spectest suite and implements Wasm 3.0 core spec features including GC, exception handling, SIMD, Memory64, multiple memories, tail calls, and typed function references.

**Design philosophy:** Same as `wasm.h` — drop a header into a project and go. No external toolchain dependencies, no code generation step required at build time, no Rust, no cargo-component. A C embedder should be able to load and run a WASI component in under 20 lines of code.

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                      wasi.h                         │
│                                                     │
│  ┌────────────┐  ┌────────────┐  ┌───────────────┐  │
│  │ Component  │  │  Canonical │  │  WASI Worlds  │  │
│  │   Binary   │  │    ABI     │  │   (0.3 → 1.0) │  │
│  │   Parser   │  │ Lift/Lower │  │               │  │
│  └─────┬──────┘  └─────┬──────┘  └──────┬────────┘  │
│        │               │               │            │
│  ┌─────┴───────────────┴───────────────┴──────────┐ │
│  │           Component Runtime                    │ │
│  │  (instantiation, linking, handles, async task  │ │
│  │   scheduler, waitable sets, stream/future)     │ │
│  └─────────────────────┬──────────────────────────┘ │
└────────────────────────┼────────────────────────────┘
                         │ depends on
┌────────────────────────┼────────────────────────────┐
│                    wasm.h                           │
│    (core module parse, validate, interpret)          │
│    (P1 stubs: wasm_bind_wasi_stubs, etc.)           │
└─────────────────────────────────────────────────────┘
```

### Layering principles

- `wasm.h` is unchanged. It remains a standalone core Wasm runtime with its existing P1 WASI stubs for legacy core modules.
- `wasi.h` includes `wasm.h` and adds everything above it: component binary format, canonical ABI, WASI world implementations, and the async task model.
- The component runtime uses `wasm.h` APIs (`wasm_load`, `wasm_call`, `wasm_memory_read/write`, etc.) to drive embedded core modules. It never bypasses the public API.
- Host-side WASI implementations (filesystem, sockets, clocks, etc.) are modular. An embedder binds only what they need. Unbound interfaces produce clean "import not satisfied" errors at instantiation time.

### Public API sketch

```c
#define WASI_IMPL
#include "wasi.h"  // pulls in wasm.h

wasi_engine_t engine;
wasi_init(&engine, NULL);

// Bind WASI worlds — only what you need
wasi_cli_config_t cli = { .argc = argc, .argv = argv, .envp = envp };
wasi_bind_cli(&engine, &cli);

wasi_fs_config_t fs = {
    .preopens = (wasi_preopen_t[]){
        { "/data", "./sandbox", WASI_FS_READONLY },
    },
    .num_preopens = 1,
};
wasi_bind_filesystem(&engine, &fs);

// Load and run
wasi_component_t *comp = wasi_load(&engine, bytes, len);
wasi_instance_t  *inst = wasi_instantiate(comp, NULL);

wasi_value_t result;
wasi_call(inst, "wasi:cli/run@0.3.0", "run", NULL, 0, &result, 1);

wasi_free_instance(inst);
wasi_free_component(comp);
wasi_destroy(&engine);
```

---

## Why 0.3, not 0.2

- WASI 0.3.0 shipped February 2026 with native async in the Canonical ABI. It is the current target for all forward-looking implementations.
- 0.3 is an iterative refinement of 0.2 — same Component Model binary format, same WIT type system, same canonical ABI for sync types. The delta is the addition of `stream<T>`, `future<T>`, async function signatures, and the task/subtask model.
- The 0.3.x release train will carry through to 1.0 (late 2026 / early 2027). Building against 0.2 means building against a version the ecosystem is actively migrating away from.
- The Component Model foundation (binary parser, canonical ABI, instantiation, linking) is identical for 0.2 and 0.3. Targeting 0.3 from the start means building async awareness into the runtime architecture from day one rather than retrofitting it.
- 0.2 compatibility comes for free: the spec explicitly supports polyfilling 0.2 interfaces in terms of 0.3. A 0.2 component's sync `poll`-based I/O can be virtualized on top of the 0.3 async runtime.

---

## [DONE] Milestone 0: Scaffolding & Infrastructure

**Goal:** Project skeleton, build system, test harness, CI.

### Deliverables

- `wasi.h` header with `WASI_IMPL` guard, includes `wasm.h`
- Public type stubs: `wasi_engine_t`, `wasi_component_t`, `wasi_instance_t`, `wasi_error_t`
- Lifecycle API: `wasi_init`, `wasi_destroy`, `wasi_load`, `wasi_free_component`
- CMake integration alongside existing `wasm.h` targets
- Test harness that can load component binaries and report pass/fail
- CI integration: `wasm-tools` for generating test components, Wasmtime for reference output comparison
- Component binary detection: distinguish core module magic (`\0asm` + version 1, layer 0x00) from component layer byte (0x0d)

### Validation

- Builds clean under `-Wall -Wextra -Wpedantic` C99
- Correctly identifies component vs. core module binaries
- Test harness runs, reports "not yet implemented" for component loading


---

## Milestone 1: Component Binary Parser

**Goal:** Parse the component binary format, extract core modules, recover the full component structure.

### Background

A component binary uses the same `\0asm` magic with layer byte 0x0d (vs. 0x01 for core modules). The binary is a sequence of sections that can be interleaved in any order:

- **Core module sections** — embedded complete `.wasm` core modules
- **Core instance sections** — instantiation of core modules with imports
- **Component type sections** — WIT-derived type definitions in the component type index space
- **Import/export sections** — interface-level imports and exports with kebab-name identifiers (e.g., `wasi:filesystem/types@0.3.0`)
- **Canon sections** — canonical function definitions: `canon lift`, `canon lower`, `canon resource.new`, `canon resource.rep`, `canon resource.drop`, plus 0.3 async built-ins (`canon task.return`, `canon stream.new`, `canon stream.read`, `canon stream.write`, `canon future.new`, `canon future.read`, `canon future.write`, `canon waitable-set.new`, `canon waitable-set.wait`, `canon waitable-set.poll`, `canon yield`, `canon subtask.drop`)
- **Instance sections** — component-level instantiation instructions
- **Alias sections** — aliases into child instances and outer scopes
- **Start section** — component start function

### Deliverables

- Section-level parser for the component binary format
- Core module extraction: feed embedded modules to `wasm_load()`, retain `wasm_module_t*` handles
- Component type index space: parse defined types, function types, component types, instance types, resource types
- Import/export parsing with full interface name + version resolution
- Canon section parsing: recognize all `canon` opcodes including the 0.3 async built-ins (can store them as parsed AST without implementing semantics yet)
- Instantiation / alias / start section parsing
- Diagnostic API: `wasi_dump_component(comp)` prints structure summary

### Retained AST Scope

Milestone 1 should not stop at “reader can skip past the bytes”. The parser needs an internal retained AST rich enough to support later milestones without reparsing the original binary from scratch.

Minimum retained structure:

- **Stable index spaces** — preserve the top-level component index spaces as explicit arrays/tables: core types, component types, funcs, core modules, nested components, core instances, component instances, values, aliases, imports, exports, canons, and the start record.
- **Component value/type AST** — retain recursive definitions for `record`, `variant`, `enum`, `flags`, `tuple`, `list`, `option`, `result`, `own`, `borrow`, `stream`, `future`, and `error-context`. Do not collapse these to a single opcode if M2/M3/M11 will need field names, case names, child types, or flag counts.
- **Function type AST** — retain ordered params/results with names and type references for component funcs, plus enough metadata to recover flattening and canonical ABI lowering decisions later.
- **Component / instance type declarations** — retain nested declaration lists, not just a `decl_count`: nested `type`, nested `core:type`, nested `alias`, `import`, and `export` records should all survive parsing as structured nodes.
- **Core type AST** — retain top-level and nested `core:type` entries as structured nodes: module types, rec groups, subtypes, and their inner composite kinds (`func`, `struct`, `array`, `cont`) plus referenced indices where present.
- **Core module type declarations** — inside core module types, retain import, type, alias, and export declarations with their extern-type payloads so M5 can do type-directed linking instead of byte-level reparsing.
- **Extern descriptor AST** — retain parsed component extern descriptors and core extern types, not just resolved kinds, so import/export matching in M5 and wrapper generation in M11 have direct access to the structural type information.
- **Canon AST** — retain every canonical function/builtin node with opcode, kind, async flag, options, referenced func/type/resource indices, callback-related immediates, and task/stream/future result metadata.
- **Instantiation AST** — retain core-instance and component-instance instruction bodies as procedural records in original order, including argument maps, because M5 executes the component’s embedded linking program rather than reconstructing it heuristically.
- **Names and versions** — preserve both the raw import/export name and the split interface name/version pair. Later milestones need both the canonicalized matching key and the original diagnostic spelling.
- **Source offsets** — keep section-relative or file-relative byte offsets for major AST nodes where cheap to do so. This is useful for `wasi_dump_component`, diffing against `wasm-tools dump`, and future diagnostics.

What can remain shallow in Milestone 1:

- The public C API does not need to expose the entire AST immediately.
- Layout/alignment calculations for canonical ABI types can wait until M2/M3.
- Semantic linking, type checking across instances, and canonical lift/lower execution can wait until M5.

Design constraint for the retained AST:

- It should be **index-based and arena-friendly**, so later milestones can cheaply reference nodes by index rather than copying trees.
- It should distinguish **component-level types** from **core-level types** even when their binary encodings share lead bytes such as `0x50`.
- It should preserve enough naming detail that M11 (`wasm2api` component mode) can generate useful C identifiers without going back to the original bytes.

### Validation

- Round-trip: load component binaries from `wasm-tools component new`, verify core modules extract and validate
- Parse components from Rust `wasm32-wasip2` target
- Parse components from `wasm-tools compose`
- Parse 0.3-format components from Wasmtime 37+ test suite (async canon opcodes present)
- Compare parsed structure against `wasm-tools component wit` and `wasm-tools dump` output

---

## Milestone 2: Canonical ABI — Scalar Types & Strings

**Goal:** Lift and lower scalar types and strings across the component boundary.

### Background

The Canonical ABI maps WIT types to core Wasm values and linear memory:

- **Scalars** (`bool`, `u8`–`u64`, `s8`–`s64`, `f32`, `f64`, `char`): direct mapping to i32/i64/f32/f64 with validation (bool ∈ {0,1}, char = valid Unicode scalar value, integer range checks)
- **Strings**: lowered as `(i32 pointer, i32 length)`. UTF-8 encoded by default. The `string-encoding` canon option can specify UTF-16 or latin1+utf16.
- Allocation on the callee side via `cabi_realloc` (a required core export for any component that receives allocated values)
- Post-return cleanup via `cabi_post_*` exports

### Deliverables

- `canon lift` for scalars: validate and widen core values to WIT-level values
- `canon lower` for scalars: narrow WIT values to core Wasm values
- String lifting: read `(ptr, len)` from linear memory, copy UTF-8 bytes out
- String lowering: invoke `cabi_realloc` in callee's instance, write UTF-8 bytes, pass `(ptr, len)`
- String encoding negotiation: support `utf8` (default), `utf16`, `latin1+utf16` canon options
- `cabi_realloc` discovery and dispatch
- `cabi_post_*` post-return cleanup dispatch
- Public `wasi_call` that accepts C values, lowers them, invokes the core function, and lifts results

### Validation

- Round-trip strings through a component export `func(s: string) -> string`
- Boundary values for all scalar types (i32 min/max, u64 max, NaN propagation, Unicode edge cases U+0000, U+D7FF, U+E000, U+10FFFF)
- Verify `cabi_realloc` called with correct `(old_ptr, old_size, align, new_size)` arguments
- Verify post-return functions fire after results are consumed
- UTF-16 string round-trip with a component compiled using `string-encoding=utf16`

---

## Milestone 3: Canonical ABI — Compound Types

**Goal:** Complete canonical ABI for all synchronous WIT value types.

### Deliverables

- **Lists** — `list<T>`: lowered as `(i32 ptr, i32 len)` with per-element layout. Nested lists require recursive realloc. Element alignment and size computed from inner type.
- **Records / Tuples** — Flattened to sequential core params/results up to `MAX_FLAT_PARAMS` (16) and `MAX_FLAT_RESULTS` (1). Beyond thresholds, spilled to linear memory via a return-area pointer (`retptr`).
- **Variants / Enums** — Discriminant as `i32`, payload follows per-case layout with alignment padding. Variant case index validation.
- **Option\<T\>** — Canonical sugar for `variant { none, some(T) }`.
- **Result\<T, E\>** — Canonical sugar for `variant { ok(T), err(E) }`.
- **Flags** — Bit-packed into `ceil(n/32)` `i32` values.
- Flattening / spilling logic: detect when flat limits are exceeded, switch to `retptr`-based memory passing
- Alignment and size calculation engine per canonical ABI spec (recursive over type structure)

### Validation

- Complex round-trip: `func(r: record { a: u32, b: string, c: list<u8> }) -> result<string, error-code>`
- Spill path: function with >16 flat params verified to use memory path
- Single-result spill: function returning multi-field record uses `retptr`
- Flags with >32 bits span multiple i32s
- Fuzz: generate random WIT types via `wasm-tools`, compile Rust components, round-trip values and compare against Wasmtime output

---

## Milestone 4: Resource Types & Handle Tables

**Goal:** Resource lifecycle — handle tables, own/borrow semantics, destructors.

### Background

WIT `resource` types are represented as `i32` handle indices in core Wasm. The component runtime maintains per-instance handle tables. The Canonical ABI defines three operations: `resource.new` (allocate handle), `resource.rep` (get representation), `resource.drop` (destroy). Handles are typed as `own<T>` (ownership transferred) or `borrow<T>` (lent for call duration).

### Deliverables

- Per-instance handle table: `wasi_handle_table_t` mapping i32 indices → host resource pointers + type tag
- `canon resource.new`: allocate handle, associate with host-side representation
- `canon resource.rep`: retrieve representation from handle (only valid within the owning component)
- `canon resource.drop`: invoke destructor, free table slot
- Own vs. borrow tracking:
  - Own handles: consumed on transfer, trap on double-use
  - Borrow handles: valid only for call duration, `num_lends` / `num_borrows` reference counting per the canonical ABI spec
  - Trap on drop of handle with outstanding borrows
- Cross-component handle transfer during calls
- Host-side resource registration API:

```c
wasi_resource_type_t fd_type = wasi_define_resource(
    &engine, "wasi:filesystem/types@0.3.0", "descriptor",
    descriptor_dtor, userdata
);
```

### Validation

- Host creates resource, passes own handle to component, component drops → destructor fires
- Borrow handle: component traps if it tries to drop a borrowed handle
- Cross-component transfer: component A creates resource, passes own to component B, B drops → A's destructor fires
- `num_lends` guard: dropping an own handle while a borrow is outstanding traps the caller
- Handle table growth and slot reuse after drops

---

## Milestone 5: Component Instantiation & Linking

**Goal:** Full component instantiation — import resolution, core module wiring, canonical function dispatch, start execution.

### Background

A component's instantiation section is procedural: it executes a sequence of instructions that instantiate core modules, create component instances, alias exports, define canonical lift/lower wrappers, and export interfaces. This is essentially a small "linking program" embedded in the component binary.

### Deliverables

- **Import resolution:** match component imports against host-provided interfaces and other component exports by fully-qualified name (`wasi:filesystem/types@0.3.0`)
- **Core module instantiation:** for each embedded core module, call `wasm_load()` with resolved imports (host functions wired through canonical lower thunks)
- **Alias resolution:** core export aliases, component export aliases, outer aliases
- **Canonical function wiring:**
  - For each `canon lift`: create a thunk that lifts core Wasm values → WIT values (using M2/M3 logic), makes them available as a component export
  - For each `canon lower`: create a thunk that lowers WIT values → core Wasm values, dispatches to a host import or another component's lifted export
  - For async canon lift/lower: store the async flag and callback option for M8 dispatch
- **Component start execution**
- **Multi-module wiring:** components with multiple core modules sharing memory / table imports
- **Error reporting:** unresolved import diagnostics naming the missing interface + version, type mismatch details

### Validation

- Single-module component: Rust `wasm32-wasip2` trivial component instantiates
- Multi-module component: `wasm-tools compose` output with linked modules
- Missing import: clean error naming `wasi:filesystem/types@0.3.0` as unresolved
- Nested components (component containing sub-components)
- Canon lift/lower wiring verified by calling a simple exported function end-to-end

---

## Milestone 6: Async Runtime Foundation

**Goal:** Implement the 0.3 async task model — tasks, subtasks, waitable sets, `future<T>`, `stream<T>` — at the runtime level, independent of any specific WASI world.

### Why this comes before WASI worlds

WASI 0.3 interfaces are defined in terms of async functions, streams, and futures. The `wasi:io/streams` interface that backs stdout, filesystem I/O, sockets, and HTTP is fundamentally async in 0.3. Implementing WASI worlds without the async runtime would mean implementing 0.2-style sync interfaces and then migrating — exactly the retrofit path we want to avoid. Building the async machinery first means every WASI world implementation is naturally 0.3-native.

### Background

The Component Model async ABI introduces:

- **Tasks:** each call into a component export creates a task. Tasks can suspend (awaiting I/O) and resume. A per-instance task table tracks active tasks.
- **Subtasks:** when a component calls an async import, a subtask is created. States: STARTING → STARTED → RETURNED (or CANCELLED). The caller is notified via events.
- **Waitable sets:** a set of waitables (subtasks, stream/future read/write handles) that a task can block on. `waitable-set.wait` blocks until one fires. `waitable-set.poll` is non-blocking.
- **`future<T>`:** single async value. Readable and writable ends, each an i32 handle. `future.new` creates a pair, `future.write` produces the value, `future.read` consumes it.
- **`stream<T>`:** async sequence of `T` values with backpressure. `stream.new` creates a pair, `stream.write` sends a batch, `stream.read` receives a batch. Zero-length read/write for readiness signaling.
- **Event codes:** `NONE`, `SUBTASK`, `STREAM_READ`, `STREAM_WRITE`, `FUTURE_READ`, `FUTURE_WRITE`, `TASK_CANCELLED`
- **Yield:** cooperative yield point for long-running sync code
- **Callback mode:** alternative to blocking where the component returns a "callback" function pointer and an event code, and the runtime calls back when the event fires

### Deliverables

- **Task table:** per-instance, tracks active tasks with state machines
- **Subtask management:** create/start/return lifecycle, event delivery to parent task
- **Waitable set:** add/remove waitables, `wait` (blocking) and `poll` (non-blocking) dispatch
- **`future<T>` runtime:** handle pair allocation, single-value write/read with ownership transfer, event notification
- **`stream<T>` runtime:** handle pair allocation, batched write/read with backpressure, readiness via 0-length operations, event notification
- **`canon` built-in dispatch:** wire all async canon opcodes parsed in M1 to runtime implementations:
  - `task.return`, `task.wait` (legacy), `task.poll` (legacy), `task.yield`
  - `subtask.drop`
  - `waitable-set.new`, `waitable-set.wait`, `waitable-set.poll`, `waitable-set.drop`
  - `waitable.join`, `waitable.drop`
  - `stream.new`, `stream.read`, `stream.write`, `stream.cancel-read`, `stream.cancel-write`, `stream.close-readable`, `stream.close-writable`
  - `future.new`, `future.read`, `future.write`, `future.cancel-read`, `future.cancel-write`, `future.close-readable`, `future.close-writable`
- **Canon lift/lower with `async` flag:** async-lowered imports return immediately with a subtask handle; async-lifted exports support suspension via callback or blocking
- **Sync-async bridge:** a sync `canon lower` calling an async export blocks the current task until the subtask completes — no function coloring problem at the component boundary
- **Host async integration point:**

```c
// Option A: embedder provides an event loop (non-blocking dispatch)
wasi_set_event_source(&engine, my_poll_callback, userdata);

// Option B: wasi.h uses an internal poll loop (default, blocking)
// Good enough for CLI tools, simple servers
```

### Validation

- Unit test: create a `future<string>`, write from one task, read from another, verify value transfer
- Unit test: create a `stream<u8>`, write batches, read batches, verify backpressure (write blocks when reader is behind)
- Waitable set: add multiple futures, wait, verify correct event delivered
- Sync-async bridge: sync caller → async export that suspends → resumes → returns
- Callback mode: component returns callback pointer, runtime invokes it on event
- Yield: long-running task yields, other tasks execute, original resumes

---

## Milestone 7: wasi:cli — Hello World

**Goal:** Implement enough WASI 0.3 interfaces to run `wasi:cli/command` — a component that prints to stdout, reads args/env, and exits.

### Interfaces (0.3 versions)

- `wasi:cli/environment` — `get-environment`, `get-arguments`, `initial-cwd`
- `wasi:cli/stdin`, `wasi:cli/stdout`, `wasi:cli/stderr` — stream resources
- `wasi:cli/exit` — `exit(result)`
- `wasi:io/streams` — `input-stream` and `output-stream` resources using 0.3 async `stream<u8>` under the hood
- `wasi:io/error` — error resource type
- `wasi:clocks/wall-clock` — `now`, `resolution`
- `wasi:clocks/monotonic-clock` — `now`, `resolution`, `subscribe-duration` (returns a `future`)
- `wasi:random/random` — `get-random-bytes`, `get-random-u64`
- `wasi:random/insecure` — fast PRNG
- `wasi:random/insecure-seed` — deterministic seed for reproducibility

### Deliverables

- Host implementations for all interfaces above
- `output-stream` backed by file descriptors, using the M6 `stream<u8>` runtime for async semantics
- `input-stream` backed by file descriptors
- Clock implementations using platform `clock_gettime` / `QueryPerformanceCounter`
- Random implementations using platform `/dev/urandom` / `BCryptGenRandom`
- Configuration API:

```c
wasi_cli_config_t cli = {
    .argc = argc, .argv = argv, .envp = envp,
    .stdin_fd  = STDIN_FILENO,
    .stdout_fd = STDOUT_FILENO,
    .stderr_fd = STDERR_FILENO,
};
wasi_bind_cli(&engine, &cli);
```

- **0.2 polyfill:** if a loaded component targets `wasi:cli@0.2.0` imports, the runtime adapts by wrapping the 0.3 async stream implementation with the 0.2 pollable-based interface. This is a thin adapter, not a separate implementation.

### Validation

- Rust `wasm32-wasip2`: `fn main() { println!("Hello, world!"); }` produces output *(note: current Rust toolchain still targets P2 components — the 0.2 polyfill makes this work)*
- Rust component reading env vars and args
- Exit codes propagated correctly
- Clock queries return reasonable values
- Random bytes are non-zero and non-deterministic (crypto source), deterministic in seed mode
- P2-targeting component runs through polyfill path

### 🎉 **This is the "Hello World from a component" milestone.**

---

## Milestone 8: wasi:filesystem

**Goal:** File I/O sufficient for real programs.

### Interfaces

- `wasi:filesystem/types@0.3.0` — `descriptor` resource, `stat`, `open-at`, `read-via-stream`, `write-via-stream`, `readdir`, `create-directory-at`, `remove-directory-at`, `unlink-file-at`, `rename-at`, `metadata-hash-at`, `sync`
- `wasi:filesystem/preopens@0.3.0` — `get-directories`

### Deliverables

- `descriptor` resource backed by host file descriptors, registered via M4 resource API
- Capability-based sandbox: all path resolution relative to preopens, `..` traversal clamped, symlink following sandboxed
- `read-via-stream` / `write-via-stream` return `stream<u8>` handles (native 0.3 async)
- Preopen configuration:

```c
wasi_fs_config_t fs = {
    .preopens = (wasi_preopen_t[]){
        { .guest = "/",    .host = "./sandbox", .flags = 0 },
        { .guest = "/tmp", .host = "/tmp/wasi", .flags = 0 },
    },
    .num_preopens = 2,
};
wasi_bind_filesystem(&engine, &fs);
```

- `readdir` returning directory entries as a `stream` of records
- `stat`, `open-at` with open flags (create, exclusive, truncate, directory)
- Read-only mode per preopen

### Validation

- Component creates file, writes, reads back, verifies
- Directory listing matches host
- Sandbox escape (`../../etc/passwd`) blocked
- Symlink traversal respects sandbox
- Read-only enforcement
- Async streaming read of a large file (backpressure exercised)

---

## Milestone 9: wasi:sockets & wasi:http

**Goal:** Network access.

### Interfaces

- `wasi:sockets/tcp@0.3.0` — `tcp-socket` resource with async `connect`, `listen`, `accept`, stream integration
- `wasi:sockets/udp@0.3.0` — `udp-socket` resource
- `wasi:sockets/ip-name-lookup@0.3.0` — `resolve-addresses` (async)
- `wasi:sockets/network@0.3.0` — `network` capability resource
- `wasi:http/types@0.3.0` — request/response types, `fields` (headers) resource
- `wasi:http/outgoing-handler@0.3.0` — `handle` (async)
- `wasi:http/incoming-handler@0.3.0` — async export for server components

### Deliverables

- TCP: create → bind → listen → accept → `stream<u8>` read/write → shutdown (all async-native)
- UDP: bind → send/receive datagrams
- DNS: async `resolve-addresses` delegating to host `getaddrinfo`
- HTTP outgoing: configurable backend — default is a minimal built-in HTTP/1.1 client; embedders can override with a host callback for libcurl, OpenSSL, etc.
- HTTP incoming: server-style async handler export, request body as `stream<u8>`, response body as `stream<u8>`
- Network capability configuration:

```c
wasi_net_config_t net = {
    .allow_tcp = true,
    .allow_udp = false,
    .allow_dns = true,
    .allowed_hosts = (const char*[]){ "*.example.com" },
    .num_allowed_hosts = 1,
};
wasi_bind_network(&engine, &net);
```

- Host event source integration: socket readiness events feed into the M6 async runtime's waitable sets

### Validation

- TCP echo server component
- HTTP outgoing: component fetches a URL, returns body
- DNS resolution
- Network sandbox: disallowed hosts rejected with proper error code
- Concurrent HTTP requests via async (multiple in-flight requests in single instance)

---

## Milestone 10: Polish, Hardening, 0.2 Compat

**Goal:** Production readiness, comprehensive 0.2 polyfill, tooling.

### Deliverables

- **0.2 polyfill completeness:** any component targeting `wasi:*@0.2.x` imports runs correctly through the async-native 0.3 runtime. This covers the 0.2 `pollable`-based I/O pattern, the 11-resource-type `wasi:http@0.2.x` interface, and all 0.2 worlds.
- **Error handling audit:** every WASI call returns proper error codes from the spec's error enums, no silent failures
- **Fuel integration:** component-level fuel limits wired through to `wasm_set_fuel` on inner core modules
- **Diagnostic APIs:**
  - `wasi_component_info(comp)` — human-readable summary
  - `wasi_component_exports(comp)` / `wasi_component_imports(comp)` — iterate interface names
- **Memory audit:** no leaks across instantiate/destroy cycles, Valgrind/ASan clean
- **Thread safety documentation:** which APIs are safe for concurrent use from multiple host threads (answer: separate instances are independent; shared engine requires documented synchronization)
- **Version negotiation:** components targeting 0.2.0, 0.2.1, 0.3.0, or 0.3.x all load correctly; minor version differences handled by implementing the superset

### Validation

- Stress: instantiate/destroy 10,000 components, verify no memory growth
- Valgrind/ASan clean on full test suite
- Real-world Rust P2 components (current toolchain output) all run through polyfill
- Wasmtime output comparison: same component + same input = same result

---

## Milestone 11: wasm2api — Component Mode

**Goal:** Extend `wasm2api` to generate typed C wrappers from component WIT interfaces.

### Background

`wasm2api` currently generates wrappers for core module exports with flat i32/f64 signatures. With `wasi.h` providing the component runtime, `wasm2api` can read the WIT types embedded in a component and generate rich C bindings — real structs for records, tagged unions for variants, `const char*` for strings, proper error returns.

### Deliverables

- Parse WIT type information from component binary (reusing M1 parser)
- Generate C structs for WIT records, enums for WIT enums, tagged unions for WIT variants
- Generate typed wrapper functions for each component export:
  - String params → `const char*`
  - String results → caller-owned `char*` with free function
  - `result<T, E>` → C return code pattern or out-param
  - `list<T>` → pointer + length
  - `resource` → opaque handle typedef
  - `stream<T>` / `future<T>` → async handle types with read/write helpers
- Generated `.c` uses `wasi.h` under the hood — calls `wasi_load`, `wasi_instantiate`, `wasi_call` with proper lift/lower
- `--embed` flag works as before (component bytes in `.c`)
- `--singleton` flag works as before

### Example output

Given a component with:
```wit
interface db {
    resource connection;
    open: func(path: string) -> result<connection, error-code>;
    execute: func(conn: borrow<connection>, sql: string) -> result<list<row>, error-code>;
}
```

Generates:
```c
typedef struct my_db_connection_t my_db_connection_t; // opaque handle
typedef struct my_db_row_t { /* fields */ } my_db_row_t;

my_db_error_t my_db_open(const char *path, my_db_connection_t **out);
my_db_error_t my_db_execute(my_db_connection_t *conn, const char *sql,
                             my_db_row_t **rows, size_t *nrows);
void my_db_connection_free(my_db_connection_t *conn);
void my_db_rows_free(my_db_row_t *rows, size_t nrows);
```

### Validation

- Generate wrappers for a trivial component, compile and link against `wasi.h`, call exports
- Round-trip complex types through generated wrappers
- Verify generated code compiles warning-free under `-Wall -Wextra -Wpedantic`

---

## Milestone 12: 0.3.x Release Train Tracking

**Goal:** Track the 0.3.x point releases as they land, keeping `wasi.h` current through to 1.0.

### Expected 0.3.x additions

Per the WASI roadmap, these are planned as backwards-compatible point releases on a two-month cadence:

- **Cancellation tokens** — integrated with language async idioms
- **Stream optimizations** — canonical ABI built-ins for forwarding/splicing, skipping/writing-zeroes, stream data segments, lulls
- **Caller-supplied buffers** — more zero-copy scenarios
- **HTTP streaming specialization** — `tuple<stream<u8>, future<result<trailers, error>>>` specialization
- **Threading** — `shared-everything-threads` component model built-ins for thread spawning (this is the big one; no timeline yet, but expected in the 0.3.x range before 1.0)

### Approach

- This is not a single milestone but an ongoing tracking effort
- Each 0.3.x release is evaluated for what it adds
- New canon built-ins are implemented as they appear
- Threading will be a significant sub-project when it lands — likely 4–6 weeks on its own depending on the `shared-everything-threads` spec shape


---

## Summary Timeline

| Milestone | Description |
|-----------|-------------|
| M0  | Scaffolding & infrastructure | 
| M1  | Component binary parser | 
| M2  | Canonical ABI — scalars & strings |
| M3  | Canonical ABI — compound types |
| M4  | Resource types & handle tables |
| M5  | Component instantiation & linking |
| M6  | Async runtime foundation | 5–6 | 23–30 |
| **M7**  | **wasi:cli — Hello World** 🎉 |
| M8  | wasi:filesystem |
| M9  | wasi:sockets & wasi:http |
| M10 | Polish, hardening, 0.2 compat |
| M11 | wasm2api — component mode |
| M12 | 0.3.x release train tracking |

---

## Testing Strategy

### Test corpus sources

- **wasm-tools:** generate components from WIT definitions, compose multi-module components, produce 0.3-format components with async interfaces
- **Rust wasm32-wasip2:** real compiler output (current toolchain — exercises 0.2 polyfill path)
- **Rust wasm32-wasip3:** once toolchain support stabilizes, primary test target
- **Wasmtime 37+:** reference output comparison — same component, same inputs, same results
- **Component Model test suite:** track the component-model repo's test suite as it develops

### Continuous validation

- Every milestone adds tests to CI
- Regression: components from prior milestones continue to pass
- ASan/Valgrind on every PR
- Wasmtime comparison testing where feasible

### Fuzz targets

- Component binary parser: random bytes, verify no crashes or UB
- Canonical ABI round-trip: random WIT types → Rust components → lift/lower/verify
- Handle table: random new/drop/borrow sequences
- Async runtime: random task/subtask/stream/future interleaving, verify no deadlocks or leaks

---

## Open Design Questions

1. **Host async integration model.** The default internal poll loop is fine for CLI tools and simple embeddings. For embedders with their own event loop (`epoll`/`kqueue`/`io_uring`, libuv, etc.), we need a clean integration point. Current plan: `wasi_set_event_source()` callback that the runtime invokes when it needs to wait on host I/O. This needs design work during M6 to get right.

2. **HTTP backend.** M9's outgoing HTTP handler needs a client. Options: (a) minimal built-in HTTP/1.1 over raw sockets, (b) host callback so the embedder supplies their own HTTP stack, (c) optional compile-time integration with a library. Leaning toward (b) as default with (a) as a convenience fallback. A single-header library pulling in libcurl or OpenSSL would violate the design philosophy.

3. **Threading (0.3.x).** When `shared-everything-threads` lands, the implementation will need to decide between real OS threads and cooperative green threads. For an interpreter-based runtime, cooperative scheduling might be the pragmatic choice, with an opt-in flag for real threads. This is deferred until the spec stabilizes.

4. **Component Model version byte.** The current pre-standard version is 0x0d. It will change to 0x01 when the spec is finalized. `wasi.h` should accept both during the transition, eventually dropping 0x0d support after 1.0.

5. **P1 coexistence.** `wasm.h`'s existing `wasm_bind_wasi_stubs()` and `wasm_bind_emscripten_stubs()` remain the path for core modules. `wasi.h` does not replace or interfere with them. An embedder can use both in the same process — `wasm.h` for legacy core modules, `wasi.h` for components.
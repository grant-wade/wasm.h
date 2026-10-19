# Using wasm.h from Zig

The repository is a Zig package for Zig 0.17, the only supported version. It compiles the runtime from `src/` with Zig's C compiler and provides `wasm`, a Zig module over the C API.

- [Adding the dependency](#adding-the-dependency)
- [Running a module](#running-a-module)
- [Typed functions](#typed-functions)
- [Host functions](#host-functions)
- [Compiled modules and typed exports](#compiled-modules-and-typed-exports)
- [Generated bindings](#generated-bindings)
- [Errors, fuel, and suspended calls](#errors-fuel-and-suspended-calls)
- [Memory, globals, and tables](#memory-globals-and-tables)
- [Introspection and GC references](#introspection-and-gc-references)
- [Threads](#threads)
- [Example](#example)
- [Testing the package](#testing-the-package)

## Adding the dependency

```sh
zig fetch --save=wasm_h git+https://github.com/grant-wade/wasm.h#<commit-or-tag>
```

```zig
const wasm_h = b.dependency("wasm_h", .{ .target = target, .optimize = optimize });
exe.root_module.addImport("wasm", wasm_h.module("wasm"));
```

The module links the runtime library and libc. Package options, passed through `b.dependency`:

| Option | Default | Purpose |
| --- | --- | --- |
| `platform` | on for Linux, Windows, macOS, and the BSDs; off otherwise | Compile the WASI and Emscripten bindings and blocking `memory.atomic.wait`. |
| `werror` | off | Treat C compiler warnings as errors. |

32-bit x86 targets need SSE2. Zig's default x86 CPU has it, and the build fails with a message for CPUs that do not.

C and C++ code can link the `wasm` artifact (`wasm_h.artifact("wasm")`), which installs `wasm.h` with the modular API declarations. The translated C API is the `wasm_c` module, also reachable as `wasm.c`.

## Running a module

```zig
const std = @import("std");
const wasm = @import("wasm");

pub fn main(init: std.process.Init) !void {
    const rt = try wasm.Runtime.init(init.gpa, .{});
    defer rt.deinit();

    const instance = rt.load(bytes) catch |err| {
        std.debug.print("load failed ({t}): {s}\n", .{ err, rt.errorMessage() });
        return err;
    };
    defer instance.deinit();

    const add = try instance.func("add", fn (i32, i32) i32);
    std.debug.print("{d}\n", .{try add.call(.{ 2, 40 })});
}
```

`Runtime.init` takes the allocator the runtime uses for everything, which must outlive it, and a `Config` whose non-null fields override `wasm_config_default`. `load` compiles and instantiates in one step; it copies its input. Free instances and compiled modules before their runtime.

`Config.features` replaces the enabled proposals and `Config.fuel` sets an initial fuel budget. For untrusted modules, these fields set limits, where 0 is unlimited:

| Field | Limit |
| --- | --- |
| `max_memory_bytes` | Bytes per linear memory, at instantiation and on growth. |
| `max_total_memory_bytes` | Bytes across all the runtime's linear memories. |
| `max_table_elements` | Elements per table, at instantiation and on growth. |
| `max_gc_heap_bytes` | Bytes of GC heap. |
| `max_function_locals` | Locals per function, parameters included. |

```zig
const rt = try wasm.Runtime.init(allocator, .{ .max_memory_bytes = 64 * 1024 * 1024, .max_table_elements = 100_000 });
```

## Typed functions

`Instance.func` looks up an exported function and checks its signature against a Zig function type. A function with several results returns a tuple:

```zig
const divmod = try instance.func("divmod", fn (i32, i32) struct { i32, i32 });
const q, const r = try divmod.call(.{ 17, 5 });
```

Zig types map to Wasm types as follows:

| Zig type | Wasm type |
| --- | --- |
| `i32`, `u32` | `i32` |
| `i64`, `u64` | `i64` |
| `f32`, `f64` | `f32`, `f64` |
| `wasm.V128` (`@Vector(16, u8)`) | `v128` |
| `wasm.FuncRef` | `funcref` |
| `wasm.ExternRef`, or any optional single-item pointer | `externref` |
| `bool`, enums, and packed structs | `i32`, or `i64` when over 32 bits |
| `wasm.Value` | any type, checked when called |

A `bool` is true for any nonzero value. An integer outside an enum's tags or a packed struct's backing integer fails with `error.InvalidConversion`.

`Instance.call` and `Instance.callIndex` take `[]const wasm.Value` arguments and a `[]wasm.Value` for results, for signatures known only at run time. `Value.from` wraps a Zig value and `Value.to` unwraps one.

## Host functions

Bind imports before loading the modules that use them. A bound function is an ordinary Zig function; its C callback and Wasm signature are generated at compile time.

```zig
fn log(caller: *wasm.Instance, value: i32) void {
    _ = caller;
    std.debug.print("guest: {d}\n", .{value});
}

const Counter = struct { total: i64 = 0 };

fn accumulate(counter: *Counter, amount: i64) !i64 {
    if (amount < 0) return error.NegativeAmount;
    counter.total += amount;
    return counter.total;
}

try rt.bindFunc("env", "log", log);
var counter: Counter = .{};
try rt.bindFuncContext("env", "accumulate", &counter, accumulate);
```

- **Parameters:** an optional context pointer, then an optional `*wasm.Instance` or `*wasm.Runtime` for the caller, then Wasm-typed and guest memory parameters.
- **Results:** `void`, a Wasm-typed value, or a tuple of them, optionally in an error union.
- **Errors:** a returned error traps the calling Wasm code, with the error's name as the runtime's error message. Members of `wasm.Error` keep their meaning, such as `error.OutOfBounds`, and `error.Yield` suspends the call (see [suspended calls](#errors-fuel-and-suspended-calls)). Any other error becomes `error.Trap`.

`bindFuncContext` passes its context pointer, which must outlive every module that imports the function, as the first parameter.

### Guest memory parameters

`wasm.GuestSlice(T)` takes an offset and an element count, `wasm.GuestBytes` is `GuestSlice(u8)`, and `wasm.GuestPtr(T)` takes an offset. They read the caller's memory 0 through 32-bit offsets. Ranges are checked before the function runs, and a range out of bounds traps with `error.OutOfBounds`. `T` is an integer, float, packed struct, or an array or extern struct of those, read and written little-endian:

```zig
fn print(text: wasm.GuestBytes) void {
    std.debug.print("{s}", .{text.bytes()});
}

fn stat(path: wasm.GuestBytes, out: wasm.GuestPtr(Stat)) !void {
    try out.store(try statPath(path.bytes()));
}
```

### Namespaces

`bindAll` binds every public function of a namespace struct under one module name. `bindAllContext` also passes a context to the functions whose first parameter has its type. `comptime wasm.checkHost(Imports, Host)` fails to compile unless `Host` has a function with the Wasm signature of each function type declared in `Imports`, ignoring leading pointer parameters:

```zig
const Env = struct {
    pub fn log(state: *State, value: i32) void { ... }
    pub fn now() i64 { ... }
};

comptime wasm.checkHost(struct {
    pub const log = fn (i32) void;
    pub const now = fn () i64;
}, Env);
try rt.bindAllContext("env", Env, &state);
```

### Instance context

`bindInstanceFunc` passes the calling instance's context as the first parameter. `Runtime.instantiateContext` sets it before the start function runs. A call from an instance without a context traps.

```zig
const Tenant = struct { id: i32 };

fn tenantId(tenant: *const Tenant) i32 {
    return tenant.id;
}

try rt.bindInstanceFunc("env", "tenant_id", tenantId);
const instance = try rt.instantiateContext(compiled, &tenant);
```

### Dynamic signatures

`bindFuncDynamic` binds a function whose signature is known only at run time. The function receives the context (unless it is `{}`), the calling instance, the arguments, and a slice for the results:

```zig
fn forward(proxy: *Proxy, caller: *wasm.Instance, args: []const wasm.Value, results: []wasm.Value) !void { ... }

try rt.bindFuncDynamic("env", "forward", &.{ .i32, .i64 }, &.{.f64}, &proxy, forward);
```

### Other imports

- `Runtime.registerModule` makes an instance's exports importable by modules loaded afterwards.
- `bindGlobal` binds a `wasm.Value` that importing modules read and write in place.
- `bindMemory` binds a `wasm.HostMemory`, a `wasm.SharedMemory`, or an instance's `Memory`.
- `bindTable` binds a `wasm.HostTable` or an instance's `Table`.
- `bindTag` binds a new tag whose payload has a function type's parameters, such as `fn (i32, f64) void`.
- `bindWasi` binds the partial WASI layer, and `setWasiArgs`, `setWasiEnv`, and `preopenDir` set the guest's arguments, environment, and accessible directories. `bindEmscripten` binds the partial Emscripten layer, which reaches the host filesystem; bind it only for trusted modules. Both need the `platform` option.

## Compiled modules and typed exports

`Runtime.compile` decodes and validates a module without resolving imports or running code. It returns a `*CompiledModule`, which supports introspection but not calls. `Runtime.instantiate` creates independent instances of it, in any runtime, including runtimes on other threads (see [Threads](#threads)):

```zig
const compiled = try rt.compile(bytes);
defer compiled.deinit();

var missing = rt.unresolvedImports(compiled);
while (missing.next()) |problem| std.debug.print("{s}.{s}: {s}\n", .{ problem.import.module, problem.import.name, problem.message });

const instance = try rt.instantiate(compiled);
defer instance.deinit();
```

`unresolvedImports` iterates the imports the runtime's bindings do not resolve, and `Runtime.checkImport` checks one.

A struct whose fields name exports and give their types describes a module's interface. `CompiledModule.resolve` looks up and checks every field once and returns a `Plan`. `Plan.on` binds it to an instance of that module without further lookups:

```zig
const Api = struct {
    handle: fn (u32, u32) u32,
    memory: wasm.Memory,
    requests: wasm.Global(i64),
    table: ?wasm.Table,
};

const plan = try compiled.resolve(Api);
const api = plan.on(instance);
_ = try api.handle.call(.{ ptr, len });
```

Function types become `TypedFunc` fields; `TypedFunc`, `Global(T)`, `Memory`, and `Table` stay as they are. An optional field is null when the module lacks the export. A missing required export fails with `error.UndefinedExport`, and an export of another kind or type with `error.TypeMismatch`. `Instance.exportsAs(Api)` resolves and binds in one step.

`wasm.Pool(Api)` keeps instances of one compiled module ready. `fill` instantiates ahead of time, `acquire` returns an idle or new instance with its exports bound, and `release` frees it:

```zig
var pool = try wasm.Pool(Api).init(allocator, rt, compiled);
defer pool.deinit();
try pool.fill(8);

const lease = try pool.acquire();
defer pool.release(lease);
_ = try lease.exports.handle.call(.{ ptr, len });
```

## Generated bindings

`addBindings` in the package's `build.zig` runs `wasm2zig` on a Wasm file at build time. The Wasm file may be another build step's output:

```zig
const wasm_h_build = @import("wasm_h");

const guest = wasm_h_build.addBindings(b, wasm_h, b.path("guest.wasm"));
exe.root_module.addImport("guest", guest);
```

The generated module declares:

- `Exports`, a struct of the module's function, global, memory, and table exports for `resolve`.
- `Api(Spec)`, a struct holding `exports: wasm.Bound(Spec)` with a method per export. A function export's method takes its arguments directly and returns its result; another export's method returns its `Global`, `Memory`, or `Table`.
- `Imports`, a namespace per import module holding the type of each function import, for `checkHost`.

```zig
const guest = @import("guest");

comptime wasm.checkHost(guest.Imports.env, Env);
try rt.bindAllContext("env", Env, &state);

const plan = try compiled.resolve(guest.Exports);
const api: guest.Api(guest.Exports) = .{ .exports = plan.on(instance) };
const sum = try api.add(2, 3);
try api.memory().write(ptr, "hello");
```

`Exports` has the Wasm types: `i32` for C pointers, sizes, and enums, and `wasm.Value` for types without a Zig equivalent, such as GC references. `Spec` may instead be a struct with Zig types in their place, such as `u32`, `bool`, or an enum. `comptime wasm.checkExports(guest.Exports, Spec)`, which `Api` also runs, fails to compile unless each field of `Spec` names an export with the same kind and Wasm types. `Spec` may leave out exports, and an optional field may name a missing one, whose method fails with `error.UndefinedExport`:

```zig
const Spec = struct {
    memory: wasm.Memory,
    alloc: fn (u32) u32,
    parse: fn (src: u32, len: u32) Status,
};

const api: guest.Api(Spec) = .{ .exports = try instance.exportsAs(Spec) };
const status = try api.parse(src, len);
```

`api.exports` holds the `TypedFunc`s, for `start` and `resumeWith`. An export named `exports` has no method. Tag exports and non-function imports are left out. `zig build` also installs `wasm2zig`, which takes the module and output paths.

## Errors, fuel, and suspended calls

Errors are the `wasm.Error` set, one per `wasm_error_t` code, and `Runtime.errorMessage` describes the most recent one. `Runtime.frames` iterates the frames of the running or last trapped call. A guest that calls WASI `proc_exit` stops with `error.Exit`, and `Runtime.exitCode` returns its status:

```zig
start.call(.{}) catch |err| switch (err) {
    error.Exit => std.process.exit(@intCast(rt.exitCode() & 0xFF)),
    else => {
        std.debug.print("{t}: {s}\n", .{ err, rt.errorMessage() });
        var frames = rt.frames();
        while (frames.next()) |frame| std.debug.print("  {f}\n", .{frame});
        return err;
    },
};
```

`Runtime.setFuel` limits execution, and `disableFuel` turns metering off. `Runtime.interrupt`, which is safe to call from any thread, stops the running call. A call that stops with `error.OutOfFuel`, `error.Interrupted`, or `error.Yield` stays suspended, following the C rules in [Resuming calls](EMBEDDING.md#resuming-calls). `TypedFunc.start` calls a function and returns an `Outcome`: `.done` with its result, or `.suspended` with the reason. `resumeWith` continues the call with a tuple of the yielding host function's results, empty after a fuel or interrupt stop:

```zig
rt.setFuel(10_000);
var outcome = try run.start(.{});
while (outcome == .suspended) {
    rt.setFuel(10_000);
    outcome = try run.resumeWith(.{});
}
const result = outcome.done;
```

`Runtime.suspended` returns why the last call is suspended, or null. `TypedFunc.resumeCall` and `Runtime.resumeCall` take the host results as `[]const wasm.Value` and fail with the stop's error. `Runtime.cancel` discards the call.

## Memory, globals, and tables

```zig
const memory = instance.memory(0).?;
try memory.write(1024, "hello");
const bytes = memory.data(); // invalidated by growth unless the memory is shared
const message = try memory.slice(ptr, len);
try memory.store(u32, 2048, 7); // little-endian
const header = try memory.load(Header, 4096); // Header is an extern struct
const name = try memory.readString(1024, &buffer);

const count = try instance.global("count", i32);
try count.set(try count.get() + 1);

const table = instance.table(0).?;
const ref = try table.get(wasm.FuncRef, 0);
const callback = try rt.funcRef(ref, fn (i32) i32);
```

`Memory.load` and `store` take the same types as guest memory parameters. `Instance.getGlobal` and `setGlobal` read and write an exported global by name. `Runtime.callRef` calls a funcref with untyped values.

## Introspection and GC references

`CompiledModule.info()` and `Instance.info()` return a `*const ModuleInfo` with the module's exports, imports, types, names, and custom sections:

```zig
const info = instance.info();
var it = info.exports();
while (it.next()) |exp| switch (exp.kind) {
    .func => std.debug.print("{s} {f}\n", .{ exp.name, info.funcType(exp.index).? }),
    else => std.debug.print("{s} ({t})\n", .{ exp.name, exp.kind }),
};
const producers = info.customSection("producers");
```

`imports`, `customSections`, `funcType`, `funcName`, `typeKind`, `tagType`, `structField`, and `arrayField` cover the rest of the C introspection API. `FuncType.paramRefType`, `Import.ref_type`, `globalRefType`, and `tableRefType` describe reference types. `wasm.Value`, `wasm.FuncType`, and `wasm.CallFrame` format with `{f}`.

The GC accessors (`structNew`, `structGet`, `arrayNew`, and the rest) take and return `wasm.Value` references. `structNew` and `arrayNew` take a tuple of values, such as `.{ @as(i32, 3), @as(i64, 4) }`. A reference stays valid only while Wasm code can reach it, or until `release` on the `Held` that `Runtime.hold` returns:

```zig
const held = try rt.hold(point);
defer held.release();
rt.collectGarbage();
const y = try instance.structGet(held.get(), 1);
```

## Threads

A runtime and its modules belong to one thread at a time. To run Wasm on several threads:

1. Give each thread its own runtime and enable `.threads` on each.
2. Create a `wasm.SharedMemory`. Its allocator, and any allocator the runtimes share, must be thread-safe.
3. Bind it with `bindMemory` in every runtime before loading.

Each thread's runtime can instantiate one compiled module at the same time. Free the compiled module, and the runtime that compiled it, after the threads finish.

```zig
const shared = try wasm.SharedMemory.init(std.heap.smp_allocator, .{ .pages = 1, .max_pages = 16 });
defer shared.deinit();

// On each thread:
const rt = try wasm.Runtime.init(std.heap.smp_allocator, .{});
rt.enableFeatures(.{ .threads = true });
try rt.bindMemory("env", "memory", shared);
const instance = try rt.instantiate(compiled);
```

Free the shared memory after every module that imports it. `SharedMemory.data` stays valid while other threads grow it.

## Example

[`examples/zig`](../examples/zig) is a standalone project that depends on this package by path. Its `build.zig` compiles a C calculator library ([`calc.h`](../examples/zig/guest/calc.h)) to `wasm32-freestanding`, generates bindings for it with `addBindings`, and embeds the module in a host program. The host:

- checks its host functions against the generated `Imports` with `checkHost`;
- provides math functions and per-instance variables through `GuestBytes` and `GuestPtr` parameters, enum parameters, and `bindInstanceFunc`;
- declares the exports with `u32` pointers and `Status` enum results, checked against the generated `Exports` by `calc.Api`, and calls them as methods;
- reports syntax errors from a result struct read with `Memory.load`, and a trap with its frames;
- integrates in fuel slices with `start` and `resumeWith`, reading progress from guest memory between slices.

```sh
cd examples/zig
zig build run
```

## Testing the package

From a checkout:

```sh
zig build test              # Zig wrapper and wasm2zig tests under std.testing.allocator, plus the modular C regression tests built by Zig
zig build test -Doptimize=safe -Dplatform=false
zig build cross             # compile for every target in cross_targets in build.zig
```

When `zig` 0.17 is on `PATH`, `./cbuild --test` also runs `zig build test` as the `zig` test, with matching optimization, platform, and warning options, and runs the example as the `zig_example` test.

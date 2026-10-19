# Using wasm.h from Zig

The repository is a Zig package for Zig 0.17. It compiles the runtime from `src/` with Zig's C compiler and provides `wasm`, an idiomatic Zig module over the C API. Other Zig versions are not supported: the build script follows the build-system API of a single release.

## Adding the dependency

```sh
zig fetch --save=wasm_h git+https://github.com/grant-wade/wasm.h#<commit-or-tag>
```

Then import the module in `build.zig`:

```zig
const wasm_h = b.dependency("wasm_h", .{ .target = target, .optimize = optimize });
exe.root_module.addImport("wasm", wasm_h.module("wasm"));
```

The module links the runtime library and libc. Package options, passed through `b.dependency`:

| Option | Default | Purpose |
| --- | --- | --- |
| `platform` | on for Linux, Windows, macOS, and the BSDs; off otherwise | Compile the WASI and Emscripten bindings and blocking `memory.atomic.wait`. |
| `werror` | off | Treat C compiler warnings as errors. |

32-bit x86 targets need SSE2 for Wasm floating-point rounding. Zig's default x86 CPU has it; the build fails with a message for CPUs that do not.

C and C++ code can link the `wasm` artifact instead (`wasm_h.artifact("wasm")`), which installs `wasm.h` with the modular API declarations. The raw translated C API is also exported as the `wasm_c` module, and as `wasm.c` from the Zig module.

## Running a module

```zig
const std = @import("std");
const wasm = @import("wasm");

pub fn main() !void {
    const rt = try wasm.Runtime.init(std.heap.smp_allocator, .{});
    defer rt.deinit();

    const instance = rt.load(bytes) catch |err| {
        std.debug.print("load failed ({s}): {s}\n", .{ @errorName(err), rt.errorMessage() });
        return err;
    };
    defer instance.deinit();

    const add = try instance.func("add", fn (i32, i32) i32);
    std.debug.print("{d}\n", .{try add.call(.{ 2, 40 })});
}
```

`Runtime.init` takes the `std.mem.Allocator` the runtime allocates everything from, which must outlive it, and a `Config` whose fields override `wasm_config_default`; its `features` and `fuel` fields set the enabled proposals and an initial fuel budget. `load` copies its input, so the bytes may be freed afterwards, and returns an `*Instance`. Free every instance and compiled module before its runtime.

For untrusted modules, `max_memory_bytes` and `max_table_elements` cap each memory and table at instantiation and on growth, `max_gc_heap_bytes` caps the GC heap, and `max_function_locals` caps the locals a function may declare; 0 removes a limit:

```zig
const rt = try wasm.Runtime.init(allocator, .{ .max_memory_bytes = 64 * 1024 * 1024, .max_table_elements = 100_000 });
```

## Compiled modules and instances

`Runtime.compile` decodes and validates without resolving imports or running code. It returns a `*CompiledModule`, which supports introspection but not calls; `Runtime.instantiate` creates independent instances of it:

```zig
const compiled = try rt.compile(bytes);
defer compiled.deinit();

var missing = rt.unresolvedImports(compiled);
while (missing.next()) |problem| std.debug.print("{s}.{s}: {s}\n", .{ problem.import.module, problem.import.name, problem.message });

const instance = try rt.instantiate(compiled);
defer instance.deinit();
```

`CompiledModule.info()` and `Instance.info()` return a `*const ModuleInfo` with the module's exports, imports, types, names, and custom sections. `Runtime.checkImport` checks one import against the runtime's bindings, and `unresolvedImports` iterates the imports they do not resolve.

The runtime passed to `instantiate` may differ from the one that compiled the module, and may run on another thread; see [Threads](#threads).

### Typed exports

A struct whose fields name exports and give their types describes a module's interface. `CompiledModule.resolve` looks up and checks every field once and returns a `Plan`; `Plan.on` binds it to any instance of that module without further lookups:

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

Function types become `TypedFunc` fields, and `TypedFunc`, `Global(T)`, `Memory`, and `Table` stay as they are. An optional field is null when the module lacks the export; a missing required export fails with `error.UndefinedExport`, and an export of another kind or type with `error.TypeMismatch`. `Instance.exportsAs(Api)` resolves and binds in one step.

`wasm.Pool(Api)` keeps instances of one compiled module ready: `fill` instantiates ahead of time, `acquire` returns an idle or new instance with its exports bound, and `release` frees it.

```zig
var pool = try wasm.Pool(Api).init(allocator, rt, compiled);
defer pool.deinit();
try pool.fill(8);

const lease = try pool.acquire();
defer pool.release(lease);
_ = try lease.exports.handle.call(.{ ptr, len });
```

Errors are the `wasm.Error` set, one per `wasm_error_t` code; `Runtime.errorMessage` describes the most recent one. A guest that calls WASI `proc_exit` stops with `error.Exit`, and `Runtime.exitCode` returns its status:

```zig
start.call(.{}) catch |err| switch (err) {
    error.Exit => std.process.exit(@intCast(rt.exitCode() & 0xFF)),
    else => return err,
};
```

A call that stops with `error.OutOfFuel`, `error.Interrupted`, or `error.Yield` stays suspended, following the C rules in [Resuming calls](EMBEDDING.md#resuming-calls). `Runtime.suspended` returns why, or null; `TypedFunc.resumeCall` and `Runtime.resumeCall` continue the call with the yielding host function's results, and `Runtime.cancel` discards it:

```zig
rt.setFuel(10_000);
var result = run.call(.{});
while (rt.suspended() == .out_of_fuel) {
    rt.setFuel(10_000);
    result = run.resumeCall(&.{});
}
```

## Typed functions

`Instance.func` looks up an export and checks its signature against a Zig function type. Calls validate argument values against the Wasm parameters, including values passed as `wasm.Value`. Parameters and results map to Wasm types as follows:

| Zig type | Wasm type |
| --- | --- |
| `i32`, `u32` | `i32` |
| `i64`, `u64` | `i64` |
| `f32`, `f64` | `f32`, `f64` |
| `wasm.V128` (`@Vector(16, u8)`) | `v128` |
| `wasm.FuncRef` | `funcref` |
| `wasm.ExternRef`, or any optional single-item pointer | `externref` |
| `wasm.Value` | any type, checked when used |

A function with several results returns a tuple:

```zig
const divmod = try instance.func("divmod", fn (i32, i32) struct { i32, i32 });
const q, const r = try divmod.call(.{ 17, 5 });
```

`Instance.call` and `Instance.callIndex` take `[]const wasm.Value` and `[]wasm.Value` for calls whose types are only known at run time. `Value.from` wraps a Zig value and `Value.to` unwraps one.

## Host functions

Bind imports before loading the modules that use them. Bound functions are ordinary Zig functions; the C callback and the Wasm signature are generated at compile time.

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

- **Parameters:** a function may take a leading `*wasm.Instance`, which is the calling instance, followed by Wasm-typed parameters.
- **Context:** `bindFuncContext` passes its context pointer as the first parameter. The context must outlive every module that imports the function.
- **Instance context:** `bindInstanceFunc` passes the calling instance's context, set by `Runtime.instantiateContext` before the start function runs, as the first parameter. A call from an instance without one traps.
- **Results:** a function may return `void`, a Wasm-typed value, or a tuple of them, optionally as an error union.
- **Errors:** a returned error traps the calling Wasm code, with the error's name as the runtime's error message. Members of `wasm.Error` keep their meaning, such as `error.OutOfBounds`, and `error.Yield` suspends the call until `resumeCall` supplies the results. Any other error becomes `error.Trap`.

```zig
const Tenant = struct { id: i32 };

fn tenantId(tenant: *const Tenant) i32 {
    return tenant.id;
}

try rt.bindInstanceFunc("env", "tenant_id", tenantId);
const instance = try rt.instantiateContext(compiled, &tenant);
```

`bindAll` binds every public function of a namespace struct under one module name, and `bindAllContext` also passes a context to the functions whose first parameter has its type. `comptime wasm.checkHost(Imports, Host)` fails to compile unless `Host` has a function with the Wasm signature of each function type declared in `Imports`, ignoring leading context and `*wasm.Instance` parameters:

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

`Runtime.registerModule` makes an instance's exports importable by modules loaded afterwards. `bindGlobal`, `bindMemory`, and `bindTable` bind the other import kinds. `bindWasi` and `bindEmscripten` bind the partial compatibility layers when the `platform` option is on; `setWasiArgs`, `setWasiEnv`, and `preopenDir` configure the guest's arguments, environment, and accessible directories. Bind Emscripten's layer only for trusted modules, because it reaches the host filesystem.

## Memory, globals, tables, and introspection

```zig
const memory = instance.memory(0).?;
try memory.write(1024, "hello");
const bytes = memory.data(); // invalidated by growth unless the memory is shared
const message = try memory.slice(ptr, len);
try memory.store(u32, 2048, 7); // little-endian
const name = try memory.readString(1024, &buffer);

const count = try instance.global("count", i32);
try count.set(try count.get() + 1);

const table = instance.table(0).?;
const ref = try table.get(wasm.FuncRef, 0);
const callback = try rt.funcRef(ref, fn (i32) i32);

var it = instance.info().exports();
while (it.next()) |exp| std.debug.print("{s} {t}\n", .{ exp.name, exp.kind });
const producers = instance.info().customSection("producers");
```

`getGlobal` and `setGlobal` read and write an exported global by name. `Runtime.callRef` calls a funcref with untyped values. `ModuleInfo`'s `imports`, `customSections`, `funcType`, `funcName`, `typeKind`, and `tagType` cover the rest of the C introspection API.

The GC accessors (`structNew`, `structGet`, `arrayNew`, and the rest) take and return `wasm.Value` references. A reference stays valid only while Wasm code can still reach it.

## Threads

A runtime and its modules belong to one thread at a time. To run Wasm on several threads:

1. Give each thread its own runtime and enable `.threads` on each.
2. Create a `wasm.SharedMemory`. Its allocator, and any allocator the runtimes share, must be thread-safe.
3. Bind it with `bindMemory` in every runtime before loading.

A module compiled once can be instantiated by each thread's runtime, at the same time: `Runtime.instantiate` only reads the compiled module. Free it, and the runtime that compiled it, after the threads finish.

```zig
const shared = try wasm.SharedMemory.init(std.heap.smp_allocator, .{ .pages = 1, .max_pages = 16 });
defer shared.deinit();

// On each thread:
const rt = try wasm.Runtime.init(std.heap.smp_allocator, .{});
rt.enableFeatures(.{ .threads = true });
try rt.bindMemory("env", "memory", shared.raw());
const instance = try rt.instantiate(compiled);
```

Free the shared memory only after every module that imports it. Shared memories never move, so `SharedMemory.data` stays valid while other threads grow it.

## Testing the package

From a checkout:

```sh
zig build test              # Zig wrapper tests under std.testing.allocator, plus the modular C regression tests built by Zig
zig build test -Doptimize=safe -Dplatform=false
zig build cross             # compile for every target in cross_targets in build.zig
```

When `zig` 0.17 is on `PATH`, `./cbuild --test` also runs `zig build test` as the `zig` test, with matching optimization, platform, and warning options.

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
    const rt = try wasm.Runtime.init(.{});
    defer rt.deinit();

    const mod = rt.load(bytes) catch |err| {
        std.debug.print("load failed ({s}): {s}\n", .{ @errorName(err), rt.errorMessage() });
        return err;
    };
    defer mod.deinit();

    const add = try mod.func("add", fn (i32, i32) i32);
    std.debug.print("{d}\n", .{try add.call(.{ 2, 40 })});
}
```

`Runtime.init` takes a `Config` whose fields override `wasm_config_default`. `load` copies its input, so the bytes may be freed afterwards. Free every module before its runtime.

For untrusted modules, `max_memory_bytes` and `max_table_elements` cap each memory and table at instantiation and on growth, and `max_function_locals` caps the locals a function may declare; 0 removes a limit:

```zig
const rt = try wasm.Runtime.init(.{ .max_memory_bytes = 64 * 1024 * 1024, .max_table_elements = 100_000 });
```

`Runtime.compile` decodes and validates without resolving imports or running code; the result supports introspection, and `Module.instantiate` creates independent instances of it.

Errors are the `wasm.Error` set, one per `wasm_error_t` code; `Runtime.errorMessage` describes the most recent one. A guest that calls WASI `proc_exit` stops with `error.Exit`, and `Runtime.exitCode` returns its status:

```zig
start.call(.{}) catch |err| switch (err) {
    error.Exit => std.process.exit(@intCast(rt.exitCode() & 0xFF)),
    else => return err,
};
```

## Typed functions

`Module.func` looks up an export and checks its signature against a Zig function type. Calls validate argument values against the Wasm parameters, including values passed as `wasm.Value`. Parameters and results map to Wasm types as follows:

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
const divmod = try mod.func("divmod", fn (i32, i32) struct { i32, i32 });
const q, const r = try divmod.call(.{ 17, 5 });
```

`Module.call` and `Module.callIndex` take `[]const wasm.Value` and `[]wasm.Value` for calls whose types are only known at run time. `Value.from` wraps a Zig value and `Value.to` unwraps one.

## Host functions

Bind imports before loading the modules that use them. Bound functions are ordinary Zig functions; the C callback and the Wasm signature are generated at compile time.

```zig
fn log(caller: *wasm.Module, value: i32) void {
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

- **Parameters:** a function may take a leading `*wasm.Module`, which is the calling module, followed by Wasm-typed parameters.
- **Context:** `bindFuncContext` passes its context pointer as the first parameter. The context must outlive every module that imports the function.
- **Results:** a function may return `void`, a Wasm-typed value, or a tuple of them, optionally as an error union.
- **Errors:** a returned error traps the calling Wasm code, with the error's name as the runtime's error message. Members of `wasm.Error` keep their meaning, such as `error.OutOfBounds`. Any other error becomes `error.Trap`.

`bindGlobal`, `bindMemory`, and `bindTable` bind the other import kinds. `bindWasi` and `bindEmscripten` bind the partial compatibility layers when the `platform` option is on. Bind Emscripten's layer only for trusted modules, because it reaches the host filesystem.

## Memory, globals, and introspection

```zig
const memory = mod.memory(0).?;
try memory.write(1024, "hello");
const bytes = memory.data(); // invalidated by growth unless the memory is shared
const name = try memory.readString(1024, &buffer);

const count = try mod.getGlobal(i32, "count");
try mod.setGlobal("count", count + 1);

var it = mod.exports();
while (it.next()) |exp| std.debug.print("{s} {t}\n", .{ exp.name, exp.kind });
const producers = mod.customSection("producers");
```

`imports`, `customSections`, `funcType`, `typeKind`, and `tagType` cover the rest of the C introspection API.

The GC accessors (`structNew`, `structGet`, `arrayNew`, and the rest) take and return `wasm.Value` references. A reference stays valid only while Wasm code can still reach it.

## Threads

A runtime and its modules belong to one thread at a time. To run Wasm on several threads:

1. Give each thread its own runtime and enable `.threads` on each.
2. Create a `wasm.SharedMemory`.
3. Bind it with `bindMemory` in every runtime before loading.

```zig
const shared = try wasm.SharedMemory.init(.{ .pages = 1, .max_pages = 16 });
defer shared.deinit();

// On each thread:
const rt = try wasm.Runtime.init(.{});
rt.enableFeatures(.{ .threads = true });
try rt.bindMemory("env", "memory", shared.raw());
```

Free the shared memory only after every module that imports it. Shared memories never move, so `SharedMemory.data` stays valid while other threads grow it.

## Testing the package

From a checkout:

```sh
zig build test              # Zig wrapper tests, plus the modular C regression tests built by Zig
zig build test -Doptimize=safe -Dplatform=false
zig build cross             # compile for every target in cross_targets in build.zig
```

When `zig` 0.17 is on `PATH`, `./cbuild --test` also runs `zig build test` as the `zig` test, with matching optimization, platform, and warning options.

const std = @import("std");
const wasm = @import("wasm");
const build_options = @import("build_options");

const expect = std.testing.expect;
const expectEqual = std.testing.expectEqual;
const expectEqualStrings = std.testing.expectEqualStrings;
const expectError = std.testing.expectError;

test "build.zig.zon version matches the C header" {
    try expectEqualStrings(wasm.c.WASM_H_VERSION_STRING, build_options.package_version);
}

test "typed calls convert parameters and results" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    const add = try mod.func("add", fn (i32, i32) i32);
    try expectEqual(@as(i32, 42), try add.call(.{ 40, 2 }));
    const add_unsigned = try mod.func("add", fn (u32, u32) u32);
    try expectEqual(@as(u32, 0), try add_unsigned.call(.{ std.math.maxInt(u32), 1 }));

    const swap = try mod.func("swap", fn (i64, f64) struct { f64, i64 });
    const swapped = try swap.call(.{ -7, 1.5 });
    try expectEqual(@as(f64, 1.5), swapped[0]);
    try expectEqual(@as(i64, -7), swapped[1]);

    const lanes = try mod.func("lanes", fn (wasm.V128, wasm.V128) wasm.V128);
    const a: wasm.V128 = @bitCast(@Vector(4, u32){ 1, 2, 3, 4 });
    const b: wasm.V128 = @bitCast(@Vector(4, u32){ 10, 20, 30, std.math.maxInt(u32) });
    const sum: @Vector(4, u32) = @bitCast(try lanes.call(.{ a, b }));
    try expectEqual(@Vector(4, u32){ 11, 22, 33, 3 }, sum);

    var token: u64 = 99;
    const same = try mod.func("same", fn (?*u64) ?*u64);
    try expectEqual(@as(?*u64, &token), try same.call(.{&token}));
    try expectEqual(@as(?*u64, null), try same.call(.{null}));

    const add_ref = try mod.func("add_ref", fn () wasm.FuncRef);
    try expectEqual(@as(?u32, add.index), (try add_ref.call(.{})).index());

    const any = try mod.func("add", fn (wasm.Value, i32) wasm.Value);
    const result = try any.call(.{ .from(@as(i32, 5)), 6 });
    try expectEqual(wasm.ValType.i32, result.kind);
    try expectEqual(@as(i32, 11), try result.to(i32));
}

test "signature and lookup failures" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    try expectError(error.TypeMismatch, mod.func("add", fn (i32) i32));
    try expectError(error.TypeMismatch, mod.func("add", fn (i64, i64) i64));
    try expectError(error.TypeMismatch, mod.func("memory", fn () void));
    try expectError(error.UndefinedExport, mod.func("missing", fn () void));
    try expect(rt.errorMessage().len > 0);
}

test "dynamic arguments must match the function parameter types" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    const add = try mod.func("add", fn (wasm.Value, i32) i32);
    try expectError(error.TypeMismatch, add.call(.{ .from(@as(f32, 1.0)), 2 }));
    try expect(rt.errorMessage().len > 0);
    try expectEqual(@as(i32, 42), try add.call(.{ .from(@as(i32, 40)), 2 }));

    var results: [1]wasm.Value = undefined;
    try expectError(error.TypeMismatch, mod.call("add", &.{ .from(@as(i32, 1)), .from(@as(i64, 2)) }, &results));

    const same = try mod.func("same", fn (wasm.Value) wasm.ExternRef);
    try expectEqual(wasm.ExternRef.null, try same.call(.{.null(.nullexternref)}));
    try expectError(error.TypeMismatch, same.call(.{.null(.nullfuncref)}));
    try expectError(error.TypeMismatch, same.call(.{.from(@as(i32, 0))}));
}

test "traps and fuel" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    const div = try mod.func("div", fn (i32, i32) i32);
    try expectEqual(@as(i32, 3), try div.call(.{ 9, 3 }));
    try expectError(error.DivByZero, div.call(.{ 1, 0 }));
    try expectEqualStrings("integer divide by zero", rt.errorMessage());
    const frame = rt.callFrame(0).?;
    try expectEqual(@as(?*wasm.Instance, mod), frame.instance);
    try expectError(error.IntOverflow, div.call(.{ std.math.minInt(i32), -1 }));
    try expectEqualStrings("integer overflow", rt.errorMessage());

    rt.setFuel(10_000);
    const spin = try mod.func("spin", fn () void);
    try expectError(error.OutOfFuel, spin.call(.{}));
    try expectEqualStrings("out of fuel", rt.errorMessage());
    try expectEqual(@as(u64, 0), rt.fuel());
    try expectEqual(wasm.Runtime.Suspension.out_of_fuel, rt.suspended().?);
    try expectError(error.Suspended, div.call(.{ 9, 3 }));
    rt.cancel();
    try expectEqual(@as(?wasm.Runtime.Suspension, null), rt.suspended());
    try expectError(error.OutOfFuel, div.call(.{ 9, 3 }));
    rt.disableFuel();
    try expectEqual(@as(i32, 3), try div.resumeCall(&.{}));
}

fn hostAsk(x: i32) error{Yield}!i32 {
    _ = x;
    return error.Yield;
}

fn countInFuelSteps(rt: *wasm.Runtime, count: anytype, n: i32, steps: *u32) !i32 {
    rt.setFuel(10);
    var result = count.call(.{n});
    while (true) {
        return result catch |err| {
            if (err != error.OutOfFuel) return err;
            steps.* += 1;
            rt.setFuel(10);
            result = count.resumeCall(&.{});
            continue;
        };
    }
}

test "resumable calls" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    try rt.bindFunc("env", "ask", hostAsk);
    const mod = try rt.load(&resume_wasm);
    defer mod.deinit();

    const twice = try mod.func("twice", fn (i32) i32);
    try expectError(error.Yield, twice.call(.{4}));
    try expectEqual(wasm.Runtime.Suspension.yield, rt.suspended().?);
    try expectEqual(@as(u32, 1), rt.callDepth());
    try expectError(error.TypeMismatch, twice.resumeCall(&.{.from(@as(i64, 21))}));
    try expectEqual(@as(i32, 42), try twice.resumeCall(&.{.from(@as(i32, 21))}));
    try expectEqual(@as(?wasm.Runtime.Suspension, null), rt.suspended());

    const count = try mod.func("count", fn (i32) i32);
    var steps: u32 = 0;
    try expectEqual(@as(i32, 1000), try countInFuelSteps(rt, count, 1000, &steps));
    rt.disableFuel();
    try expect(steps > 100);

    try expectError(error.Yield, twice.call(.{1}));
    rt.cancel();
    try expectEqual(@as(i32, 3), try count.call(.{3}));
}

test "wasi configuration" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const args = [_][*:0]const u8{ "prog", "first" };
    const env = [_][*:0]const u8{"GREETING=hi"};

    // Without the platform option every WASI call fails this way.
    rt.setWasiArgs(&args) catch |err| return expectEqual(error.Malformed, err);
    try rt.setWasiEnv(&env);
    try rt.preopenDir(".", "/work");
    try expectError(error.Malformed, rt.preopenDir("missing-wasm-test-dir", "/missing"));
    try rt.bindWasi();
}

test "untyped calls" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    var results: [2]wasm.Value = undefined;
    try mod.call("swap", &.{ .from(@as(i64, 3)), .from(@as(f64, 0.25)) }, &results);
    try expectEqual(@as(f64, 0.25), try results[0].to(f64));
    try expectEqual(@as(i64, 3), try results[1].to(i64));
    try expectError(error.TypeMismatch, results[0].to(i32));
    try expectError(error.TypeMismatch, mod.call("add", &.{.from(@as(i32, 1))}, results[0..1]));
}

test "globals" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    try expectEqual(@as(i32, 7), try mod.getGlobal(i32, "counter"));
    try mod.setGlobal("counter", @as(i32, 40));
    const bump = try mod.func("bump", fn () i32);
    try expectEqual(@as(i32, 41), try bump.call(.{}));
    try expectEqual(@as(f64, 2.5), try mod.getGlobal(f64, "scale"));
    try expectError(error.TypeMismatch, mod.getGlobal(i64, "counter"));
    try expect(std.meta.isError(mod.setGlobal("scale", @as(f64, 1.0))));

    try expectEqual(@as(u32, 2), mod.info().globalCount());
    try expectEqual(wasm.ValType.i32, mod.info().globalType(0));
    try expect(mod.info().globalIsMutable(0));
    try expect(!mod.info().globalIsMutable(1));
}

test "linear memory" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    try expectEqual(@as(u32, 1), mod.info().memoryCount());
    try expectEqual(@as(?wasm.Memory, null), mod.memory(1));
    const memory = mod.memory(0).?;
    try expectEqual(@as(u64, wasm.page_size), memory.size());
    try expectEqualStrings("hello", memory.data()[16..21]);

    var buffer: [32]u8 = undefined;
    try expectEqualStrings("hello", try memory.readString(16, &buffer));
    try expectEqualStrings("hel", try memory.readString(16, buffer[0..4]));

    try memory.write(100, "wasm");
    var read: [4]u8 = undefined;
    try memory.read(100, &read);
    try expectEqualStrings("wasm", &read);
    try expectError(error.OutOfBounds, memory.read(wasm.page_size - 2, &read));

    try expectEqual(@as(?u64, 1), memory.grow(2));
    try expectEqual(@as(u64, 3 * wasm.page_size), memory.size());
    try expectEqualStrings("wasm", memory.data()[100..104]);
    try expectEqual(@as(?u64, null), memory.grow(2));
}

test "introspection" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    var names: [16][]const u8 = undefined;
    var count: usize = 0;
    var it = mod.info().exports();
    while (it.next()) |exp| : (count += 1) names[count] = exp.name;
    try expectEqual(@as(usize, mod.info().exportCount()), count);
    try expectEqualStrings("memory", names[0]);
    try expectEqualStrings("bump", names[count - 1]);

    const add = mod.info().findExport("add").?;
    try expectEqual(wasm.ExternKind.func, add.kind);
    try expectEqual(@as(?wasm.Export, null), mod.info().findExport("nope"));
    const add_type = mod.info().funcType(add.index).?;
    try expectEqualSlices(wasm.ValType, &.{ .i32, .i32 }, add_type.params);
    try expectEqualSlices(wasm.ValType, &.{.i32}, add_type.results);
    try expect(add_type.matches(fn (i32, i32) i32));
    try expect(!add_type.matches(fn (i32, i32) void));
    try expectEqual(wasm.TypeKind.func, mod.info().typeKind(0));

    try expectEqualStrings("abc", mod.info().customSection("meta").?);
    try expectEqual(@as(?[]const u8, null), mod.info().customSection("other"));
    var sections = mod.info().customSections();
    const first = sections.next().?;
    try expectEqualStrings("meta", first.name);
    try expectEqualStrings("abc", first.data);
}

fn expectEqualSlices(comptime T: type, expected: []const T, actual: []const T) !void {
    return std.testing.expectEqualSlices(T, expected, actual);
}

const HostState = struct {
    logged: i32 = 0,
    callers: usize = 0,
};

fn hostLog(state: *HostState, value: i32) void {
    state.logged = value;
}

fn hostAdd3(caller: *wasm.Instance, a: i32, b: i32, c: i32) i32 {
    _ = caller.runtime();
    return a + b + c;
}

fn hostPair() struct { i32, i64 } {
    return .{ -2, 1 << 40 };
}

fn hostFail() error{HostRefused}!void {
    return error.HostRefused;
}

fn hostEcho(state: *HostState, caller: *wasm.Instance, ref: wasm.ExternRef) wasm.ExternRef {
    _ = caller;
    state.callers += 1;
    return ref;
}

test "host functions and globals" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();

    var state: HostState = .{};
    var base: wasm.Value = .from(@as(i32, 100));
    try rt.bindFuncContext("env", "log", &state, hostLog);
    try rt.bindFunc("env", "add3", hostAdd3);
    try rt.bindFunc("env", "pair", hostPair);
    try rt.bindFunc("env", "fail", hostFail);
    try rt.bindFuncContext("env", "echo", &state, hostEcho);
    try rt.bindGlobal("env", "base", &base, true);
    const mod = try rt.load(&host_wasm);
    defer mod.deinit();

    const run = try mod.func("run", fn () i32);
    try expectEqual(@as(i32, 103), try run.call(.{}));
    try expectEqual(@as(i32, 42), state.logged);
    base = .from(@as(i32, 0));
    try expectEqual(@as(i32, 3), try run.call(.{}));

    const pair_sum = try mod.func("pair_sum", fn () i64);
    try expectEqual(@as(i64, (1 << 40) - 2), try pair_sum.call(.{}));

    const fail = try mod.func("fail", fn () void);
    try expectError(error.Trap, fail.call(.{}));
    try expect(std.mem.indexOf(u8, rt.errorMessage(), "HostRefused") != null);

    const echo = try mod.func("echo", fn (wasm.ExternRef) wasm.ExternRef);
    const ref: wasm.ExternRef = .fromPtr(&state);
    try expectEqual(ref, try echo.call(.{ref}));
    try expectEqual(@as(usize, 1), state.callers);
    try expectEqual(@as(?*HostState, &state), ref.toPtr(HostState));

    try expectEqual(@as(u32, 6), mod.info().importCount());
    const log_import = mod.info().importAt(0);
    try expectEqualStrings("env", log_import.module);
    try expectEqualStrings("log", log_import.name);
    try expectEqual(wasm.ExternKind.func, log_import.kind);
    try expectEqual(wasm.ExternKind.global, mod.info().importAt(5).kind);
}

fn hostIgnore() void {}

test "host callbacks accept null buffers for empty arguments and results" {
    const Host = struct {
        fn start(count: *u32) void {
            count.* += 1;
        }
    };
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    var count: u32 = 0;
    try rt.bindFuncContext("m", "f", &count, Host.start);
    const mod = try rt.load(&.{
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x04, 0x01, 0x60, 0x00, 0x00, 0x02, 0x07,
        0x01, 0x01, 0x6d, 0x01, 0x66, 0x00, 0x00, 0x08,
        0x01, 0x00,
    });
    defer mod.deinit();
    try expectEqual(@as(u32, 1), count);

    var unused = wasm.c.wasm_i32(0);
    try expect(wasm.c.wasm_call_index(mod.raw(), 0, &unused, 0, null, 0) == wasm.c.WASM_OK);
    try expect(wasm.c.wasm_call_index(mod.raw(), 0, null, 0, &unused, 0) == wasm.c.WASM_OK);
    try expectEqual(@as(u32, 3), count);
}

test "host signatures must match the import" {
    inline for (.{ .{ "add3", hostPair }, .{ "log", hostIgnore } }) |wrong| {
        const rt = try wasm.Runtime.init(std.testing.allocator, .{});
        defer rt.deinit();
        var state: HostState = .{};
        var base: wasm.Value = .from(@as(i32, 0));
        if (!std.mem.eql(u8, wrong[0], "log")) try rt.bindFuncContext("env", "log", &state, hostLog);
        if (!std.mem.eql(u8, wrong[0], "add3")) try rt.bindFunc("env", "add3", hostAdd3);
        try rt.bindFunc("env", wrong[0], wrong[1]);
        try rt.bindFunc("env", "pair", hostPair);
        try rt.bindFunc("env", "fail", hostFail);
        try rt.bindFuncContext("env", "echo", &state, hostEcho);
        try rt.bindGlobal("env", "base", &base, true);
        try expect(std.meta.isError(rt.load(&host_wasm)));
    }
}

test "missing imports fail to load" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    try expectError(error.UnknownImport, rt.load(&host_wasm));
    try expect(rt.errorMessage().len > 0);
    try expectError(error.InvalidMagic, rt.load("not wasm"));
}

test "compiled modules are inspected without imports and instantiated repeatedly" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();

    const unbound = try rt.compile(&host_wasm);
    defer unbound.deinit();
    try expectError(error.UnknownImport, rt.instantiate(unbound));

    const compiled = try rt.compile(&basic_wasm);
    defer compiled.deinit();
    const first = try rt.instantiate(compiled);
    defer first.deinit();
    const second = try rt.instantiate(compiled);
    defer second.deinit();
    try expectEqual(@as(i32, 3), try (try first.func("add", fn (i32, i32) i32)).call(.{ 1, 2 }));
    try expectEqual(@as(i32, 7), try (try second.func("add", fn (i32, i32) i32)).call(.{ 3, 4 }));
}

test "features" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{ .lazy_validation = false });
    defer rt.deinit();

    try expect(rt.features().simd);
    try expect(!rt.features().threads);
    rt.disableFeatures(.{ .simd = true });
    try expect(!rt.features().simd);
    try expect(std.meta.isError(rt.load(&basic_wasm)));
    rt.enableFeatures(.{ .simd = true, .threads = true });
    try expect(rt.features().simd and rt.features().threads);
    const mod = try rt.load(&basic_wasm);
    mod.deinit();
}

test "shared memory across runtimes" {
    const shared = try wasm.SharedMemory.init(std.testing.allocator, .{ .pages = 1, .max_pages = 4 });
    defer shared.deinit();

    var modules: [2]*wasm.Instance = undefined;
    var runtimes: [2]*wasm.Runtime = undefined;
    for (&runtimes, &modules) |*rt, *mod| {
        rt.* = try wasm.Runtime.init(std.testing.allocator, .{});
        rt.*.enableFeatures(.{ .threads = true });
        try rt.*.bindMemory("env", "memory", shared.raw());
        mod.* = try rt.*.load(&shared_wasm);
    }
    defer for (runtimes, modules) |rt, mod| {
        mod.deinit();
        rt.deinit();
    };

    const store = try modules[0].func("store", fn (u32, i32) void);
    const load = try modules[1].func("load", fn (u32) i32);
    try store.call(.{ 64, 1234 });
    try expectEqual(@as(i32, 1234), try load.call(.{64}));
    try expectEqual(@as(usize, wasm.page_size), shared.data().len);
    try expectEqual(@as(u8, 0xd2), shared.data()[64]);
    try expectEqual(@as(?u64, 1), modules[1].memory(0).?.grow(1));
    try expectEqual(@as(usize, 2 * wasm.page_size), shared.data().len);
}

test "gc structs and arrays" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&gc_wasm);
    defer mod.deinit();

    try expectEqual(wasm.TypeKind.@"struct", mod.info().typeKind(0));
    try expectEqual(wasm.TypeKind.array, mod.info().typeKind(1));
    try expectEqual(@as(?wasm.FuncType, null), mod.info().typeFunc(0));

    const point = try mod.structNew(0, &.{ .from(@as(i32, 3)), .from(@as(i64, 4)) });
    try expectEqual(@as(i64, 4), try (try mod.structGet(point, 1)).to(i64));
    try mod.structSet(point, 0, .from(@as(i32, 30)));
    const point_x = try mod.func("point_x", fn (wasm.Value) i32);
    try expectEqual(@as(i32, 30), try point_x.call(.{point}));
    try expectError(error.Trap, point_x.call(.{.null(.nullref)}));
    try expectError(error.TypeMismatch, point_x.call(.{.from(@as(i32, 0))}));

    const ints = try mod.arrayNew(1, &.{ .from(@as(i32, 1)), .from(@as(i32, 2)) });
    try expectError(error.TypeMismatch, point_x.call(.{ints}));
    try expectEqual(@as(u32, 2), try mod.arrayLen(ints));
    try mod.arraySet(ints, 1, .from(@as(i32, 20)));
    try expectEqual(@as(i32, 20), try (try mod.arrayGet(ints, 1)).to(i32));
    try expectError(error.OutOfBounds, mod.arrayGet(ints, 2));

    // Collection moves objects; only held references stay usable.
    const held = try rt.hold(point);
    defer rt.release(held);
    rt.collectGarbage();
    try expectEqual(@as(i64, 4), try (try mod.structGet(held.*, 1)).to(i64));
}

test "value conversions" {
    try expectEqual(@as(u32, 0xffff_ffff), try wasm.Value.from(@as(i32, -1)).to(u32));
    try expectEqual(wasm.ValType.funcref, wasm.Value.from(wasm.FuncRef.null).kind);
    try expectEqual(wasm.FuncRef.null, try wasm.Value.null(.funcref).to(wasm.FuncRef));
    try expectEqual(wasm.ExternRef.null, try wasm.Value.null(.externref).to(wasm.ExternRef));
    try expectEqual(@as(?wasm.ValType, null), wasm.ValType.of(wasm.Value));
    try expectEqual(wasm.ValType.externref, wasm.ValType.of(?*const HostState).?);
    try expectEqual(@as(wasm.c.wasm_error_t, wasm.c.WASM_ERR_DIV_BY_ZERO), wasm.errorCode(error.DivByZero));
    try expectEqual(@as(wasm.c.wasm_error_t, wasm.c.WASM_ERR_TRAP), wasm.errorCode(error.SomethingElse));
}

// (module
//   (memory (export "memory") 1 4)
//   (data (i32.const 16) "hello\00")
//   (global (export "counter") (mut i32) (i32.const 7))
//   (global (export "scale") f64 (f64.const 2.5))
//   (func $add (export "add") (param i32 i32) (result i32)
//     (i32.add (local.get 0) (local.get 1)))
//   (func (export "swap") (param i64 f64) (result f64 i64)
//     (local.get 1) (local.get 0))
//   (func (export "div") (param i32 i32) (result i32)
//     (i32.div_s (local.get 0) (local.get 1)))
//   (func (export "spin") (loop (br 0)))
//   (func (export "lanes") (param v128 v128) (result v128)
//     (i32x4.add (local.get 0) (local.get 1)))
//   (func (export "same") (param externref) (result externref) (local.get 0))
//   (func (export "add_ref") (result funcref) (ref.func $add))
//   (func (export "bump") (result i32)
//     (global.set 0 (i32.add (global.get 0) (i32.const 1)))
//     (global.get 0))
//   (elem declare func $add)
//   (@custom "meta" "abc"))
const basic_wasm = [_]u8{
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x24, 0x07, 0x60, 0x02, 0x7f, 0x7f, 0x01,
    0x7f, 0x60, 0x02, 0x7e, 0x7c, 0x02, 0x7c, 0x7e, 0x60, 0x00, 0x00, 0x60, 0x02, 0x7b, 0x7b, 0x01,
    0x7b, 0x60, 0x01, 0x6f, 0x01, 0x6f, 0x60, 0x00, 0x01, 0x70, 0x60, 0x00, 0x01, 0x7f, 0x03, 0x09,
    0x08, 0x00, 0x01, 0x00, 0x02, 0x03, 0x04, 0x05, 0x06, 0x05, 0x04, 0x01, 0x01, 0x01, 0x04, 0x06,
    0x12, 0x02, 0x7f, 0x01, 0x41, 0x07, 0x0b, 0x7c, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x40, 0x0b, 0x07, 0x56, 0x0b, 0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00, 0x07,
    0x63, 0x6f, 0x75, 0x6e, 0x74, 0x65, 0x72, 0x03, 0x00, 0x05, 0x73, 0x63, 0x61, 0x6c, 0x65, 0x03,
    0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x04, 0x73, 0x77, 0x61, 0x70, 0x00, 0x01, 0x03, 0x64,
    0x69, 0x76, 0x00, 0x02, 0x04, 0x73, 0x70, 0x69, 0x6e, 0x00, 0x03, 0x05, 0x6c, 0x61, 0x6e, 0x65,
    0x73, 0x00, 0x04, 0x04, 0x73, 0x61, 0x6d, 0x65, 0x00, 0x05, 0x07, 0x61, 0x64, 0x64, 0x5f, 0x72,
    0x65, 0x66, 0x00, 0x06, 0x04, 0x62, 0x75, 0x6d, 0x70, 0x00, 0x07, 0x09, 0x05, 0x01, 0x03, 0x00,
    0x01, 0x00, 0x0a, 0x40, 0x08, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b, 0x06, 0x00, 0x20,
    0x01, 0x20, 0x00, 0x0b, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6d, 0x0b, 0x07, 0x00, 0x03, 0x40,
    0x0c, 0x00, 0x0b, 0x0b, 0x09, 0x00, 0x20, 0x00, 0x20, 0x01, 0xfd, 0xae, 0x01, 0x0b, 0x04, 0x00,
    0x20, 0x00, 0x0b, 0x04, 0x00, 0xd2, 0x00, 0x0b, 0x0b, 0x00, 0x23, 0x00, 0x41, 0x01, 0x6a, 0x24,
    0x00, 0x23, 0x00, 0x0b, 0x0b, 0x0c, 0x01, 0x00, 0x41, 0x10, 0x0b, 0x06, 0x68, 0x65, 0x6c, 0x6c,
    0x6f, 0x00, 0x00, 0x08, 0x04, 0x6d, 0x65, 0x74, 0x61, 0x61, 0x62, 0x63, 0x00, 0x0d, 0x04, 0x6e,
    0x61, 0x6d, 0x65, 0x01, 0x06, 0x01, 0x00, 0x03, 0x61, 0x64, 0x64,
};

// (module
//   (import "env" "log" (func $log (param i32)))
//   (import "env" "add3" (func $add3 (param i32 i32 i32) (result i32)))
//   (import "env" "pair" (func $pair (result i32 i64)))
//   (import "env" "fail" (func $fail))
//   (import "env" "echo" (func $echo (param externref) (result externref)))
//   (import "env" "base" (global $base (mut i32)))
//   (func (export "run") (result i32)
//     (call $log (i32.const 42))
//     (call $add3 (i32.const 1) (i32.const 2) (global.get $base)))
//   (func (export "pair_sum") (result i64)
//     (local i64)
//     (call $pair)
//     (local.set 0)
//     (i64.extend_i32_s)
//     (i64.add (local.get 0)))
//   (func (export "fail") (call $fail))
//   (func (export "echo") (param externref) (result externref) (call $echo (local.get 0))))
const host_wasm = [_]u8{
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x21, 0x07, 0x60, 0x01, 0x7f, 0x00, 0x60,
    0x03, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x60, 0x00, 0x02, 0x7f, 0x7e, 0x60, 0x00, 0x00, 0x60, 0x01,
    0x6f, 0x01, 0x6f, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x00, 0x01, 0x7e, 0x02, 0x43, 0x06, 0x03, 0x65,
    0x6e, 0x76, 0x03, 0x6c, 0x6f, 0x67, 0x00, 0x00, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x61, 0x64, 0x64,
    0x33, 0x00, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x70, 0x61, 0x69, 0x72, 0x00, 0x02, 0x03, 0x65,
    0x6e, 0x76, 0x04, 0x66, 0x61, 0x69, 0x6c, 0x00, 0x03, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x65, 0x63,
    0x68, 0x6f, 0x00, 0x04, 0x03, 0x65, 0x6e, 0x76, 0x04, 0x62, 0x61, 0x73, 0x65, 0x03, 0x7f, 0x01,
    0x03, 0x05, 0x04, 0x05, 0x06, 0x03, 0x04, 0x07, 0x20, 0x04, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x05,
    0x08, 0x70, 0x61, 0x69, 0x72, 0x5f, 0x73, 0x75, 0x6d, 0x00, 0x06, 0x04, 0x66, 0x61, 0x69, 0x6c,
    0x00, 0x07, 0x04, 0x65, 0x63, 0x68, 0x6f, 0x00, 0x08, 0x0a, 0x29, 0x04, 0x0e, 0x00, 0x41, 0x2a,
    0x10, 0x00, 0x41, 0x01, 0x41, 0x02, 0x23, 0x00, 0x10, 0x01, 0x0b, 0x0c, 0x01, 0x01, 0x7e, 0x10,
    0x02, 0x21, 0x00, 0xac, 0x20, 0x00, 0x7c, 0x0b, 0x04, 0x00, 0x10, 0x03, 0x0b, 0x06, 0x00, 0x20,
    0x00, 0x10, 0x04, 0x0b, 0x00, 0x2e, 0x04, 0x6e, 0x61, 0x6d, 0x65, 0x01, 0x1e, 0x05, 0x00, 0x03,
    0x6c, 0x6f, 0x67, 0x01, 0x04, 0x61, 0x64, 0x64, 0x33, 0x02, 0x04, 0x70, 0x61, 0x69, 0x72, 0x03,
    0x04, 0x66, 0x61, 0x69, 0x6c, 0x04, 0x04, 0x65, 0x63, 0x68, 0x6f, 0x07, 0x07, 0x01, 0x00, 0x04,
    0x62, 0x61, 0x73, 0x65,
};

// (module
//   (import "env" "memory" (memory 1 4 shared))
//   (func (export "store") (param i32 i32) (i32.atomic.store (local.get 0) (local.get 1)))
//   (func (export "load") (param i32) (result i32) (i32.atomic.load (local.get 0))))
// (module
//   (import "env" "ask" (func $ask (param i32) (result i32)))
//   (func (export "twice") (param i32) (result i32) (i32.mul (call $ask (local.get 0)) (i32.const 2)))
//   (func (export "count") (param $n i32) (result i32)
//     (local $i i32)
//     (loop $l
//       (local.set $i (i32.add (local.get $i) (i32.const 1)))
//       (br_if $l (i32.lt_u (local.get $i) (local.get $n))))
//     (local.get $i)))
const resume_wasm = [_]u8{
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,
    0x02, 0x0b, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x61, 0x73, 0x6b, 0x00, 0x00, 0x03, 0x03, 0x02,
    0x00, 0x00, 0x07, 0x11, 0x02, 0x05, 0x74, 0x77, 0x69, 0x63, 0x65, 0x00, 0x01, 0x05, 0x63, 0x6f,
    0x75, 0x6e, 0x74, 0x00, 0x02, 0x0a, 0x23, 0x02, 0x09, 0x00, 0x20, 0x00, 0x10, 0x00, 0x41, 0x02,
    0x6c, 0x0b, 0x17, 0x01, 0x01, 0x7f, 0x03, 0x40, 0x20, 0x01, 0x41, 0x01, 0x6a, 0x21, 0x01, 0x20,
    0x01, 0x20, 0x00, 0x49, 0x0d, 0x00, 0x0b, 0x20, 0x01, 0x0b,
};

const shared_wasm = [_]u8{
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0b, 0x02, 0x60, 0x02, 0x7f, 0x7f, 0x00,
    0x60, 0x01, 0x7f, 0x01, 0x7f, 0x02, 0x10, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x06, 0x6d, 0x65, 0x6d,
    0x6f, 0x72, 0x79, 0x02, 0x03, 0x01, 0x04, 0x03, 0x03, 0x02, 0x00, 0x01, 0x07, 0x10, 0x02, 0x05,
    0x73, 0x74, 0x6f, 0x72, 0x65, 0x00, 0x00, 0x04, 0x6c, 0x6f, 0x61, 0x64, 0x00, 0x01, 0x0a, 0x15,
    0x02, 0x0a, 0x00, 0x20, 0x00, 0x20, 0x01, 0xfe, 0x17, 0x02, 0x00, 0x0b, 0x08, 0x00, 0x20, 0x00,
    0xfe, 0x10, 0x02, 0x00, 0x0b,
};

// (module
//   (type $point (struct (field $x (mut i32)) (field $y i64)))
//   (type $ints (array (mut i32)))
//   (func (export "point_x") (param (ref null $point)) (result i32)
//     (struct.get $point $x (local.get 0))))
const gc_wasm = [_]u8{
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x10, 0x03, 0x5f, 0x02, 0x7f, 0x01, 0x7e,
    0x00, 0x5e, 0x7f, 0x01, 0x60, 0x01, 0x63, 0x00, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x02, 0x07, 0x0b,
    0x01, 0x07, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x5f, 0x78, 0x00, 0x00, 0x0a, 0x0a, 0x01, 0x08, 0x00,
    0x20, 0x00, 0xfb, 0x02, 0x00, 0x00, 0x0b, 0x00, 0x20, 0x04, 0x6e, 0x61, 0x6d, 0x65, 0x04, 0x0e,
    0x02, 0x00, 0x05, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x01, 0x04, 0x69, 0x6e, 0x74, 0x73, 0x0a, 0x09,
    0x01, 0x00, 0x02, 0x00, 0x01, 0x78, 0x01, 0x01, 0x79,
};

test "allocation failures return errors without leaking" {
    var fail_index: usize = 0;
    while (true) : (fail_index += 1) {
        var failing = std.testing.FailingAllocator.init(std.testing.allocator, .{ .fail_index = fail_index });
        const rt = wasm.Runtime.init(failing.allocator(), .{}) catch continue;
        defer rt.deinit();
        const mod = rt.load(&basic_wasm) catch continue;
        mod.deinit();
        if (!failing.has_induced_failure) break;
    }
}

const BasicExports = struct {
    add: fn (i32, i32) i32,
    bump: wasm.TypedFunc(fn () i32),
    memory: wasm.Memory,
    counter: wasm.Global(i32),
    scale: wasm.Global(f64),
    absent: ?fn () void,
};

test "plans bind the exports of each instance" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const compiled = try rt.compile(&basic_wasm);
    defer compiled.deinit();

    const plan = try compiled.resolve(BasicExports);
    const first = try rt.instantiate(compiled);
    defer first.deinit();
    const second = try rt.instantiate(compiled);
    defer second.deinit();

    const a = plan.on(first);
    const b = plan.on(second);
    try expectEqual(@as(i32, 5), try a.add.call(.{ 2, 3 }));
    try expectEqual(@as(?wasm.TypedFunc(fn () void), null), a.absent);
    try a.counter.set(100);
    try expectEqual(@as(i32, 101), try a.bump.call(.{}));
    try expectEqual(@as(i32, 8), try b.bump.call(.{}));
    try expectEqual(@as(f64, 2.5), try b.scale.get());
    try expect(a.counter.isMutable());
    try expectError(error.TypeMismatch, b.scale.set(1.0));
    try expectEqualStrings("hello", (try a.memory.slice(16, 5)));

    const bound = try first.exportsAs(struct { counter: wasm.Global(wasm.Value) });
    try expectEqual(@as(i32, 101), try (try bound.counter.get()).to(i32));
    try expectEqual(@as(i32, 101), try (try first.global("counter", i32)).get());
    try expectError(error.TypeMismatch, first.global("counter", i64));

    try expectError(error.UndefinedExport, compiled.resolve(struct { nope: fn () void }));
    try expectEqualStrings("missing export 'nope'", rt.errorMessage());
    try expectError(error.TypeMismatch, compiled.resolve(struct { add: fn (i32) i32 }));
    try expectError(error.TypeMismatch, compiled.resolve(struct { add: wasm.Memory }));
    try expectEqualStrings("export 'add' is not a memory", rt.errorMessage());
    try expectError(error.TypeMismatch, compiled.resolve(struct { scale: wasm.Global(i32) }));
    try expectEqual(@as(?wasm.Error, error.TypeMismatch), rt.lastError());
}

test "pools hand out fresh instances" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const compiled = try rt.compile(&basic_wasm);
    defer compiled.deinit();

    var pool = try wasm.Pool(BasicExports).init(std.testing.allocator, rt, compiled);
    defer pool.deinit();
    try pool.fill(2);
    try expectEqual(@as(usize, 2), pool.idle.items.len);

    var leases: [3]wasm.Pool(BasicExports).Lease = undefined;
    for (&leases) |*lease| lease.* = try pool.acquire();
    try expectEqual(@as(usize, 0), pool.idle.items.len);
    for (leases) |lease| try expectEqual(@as(i32, 8), try lease.exports.bump.call(.{}));
    for (leases) |lease| pool.release(lease);
}

const Tenant = struct { id: i32 };

fn tenantId(tenant: *const Tenant) i32 {
    return tenant.id;
}

test "instances carry their own context from the start function on" {
    const owner = try wasm.Runtime.init(std.testing.allocator, .{});
    defer owner.deinit();
    const compiled = try owner.compile(&ctx_wasm);
    defer compiled.deinit();

    var missing = owner.unresolvedImports(compiled);
    const problem = missing.next().?;
    try expectEqualStrings("env", problem.import.module);
    try expectEqualStrings("ctx", problem.import.name);
    try expectEqual(error.UnknownImport, problem.err);
    try expectEqualStrings("unresolved: env.ctx", problem.message);
    try expectEqual(@as(?wasm.ImportProblem, null), missing.next());

    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    try rt.bindInstanceFunc("env", "ctx", tenantId);
    try rt.checkImport(compiled, 0);
    var resolved = rt.unresolvedImports(compiled);
    try expectEqual(@as(?wasm.ImportProblem, null), resolved.next());

    const first_tenant: Tenant = .{ .id = 77 };
    const second_tenant: Tenant = .{ .id = 5 };
    const first = try rt.instantiateContext(compiled, &first_tenant);
    defer first.deinit();
    const second = try rt.instantiateContext(compiled, &second_tenant);
    defer second.deinit();
    try expectEqual(rt, first.runtime());
    try expectEqual(@as(i32, 77), try (try first.func("seen", fn () i32)).call(.{}));
    try expectEqual(@as(i32, 5), try (try second.func("seen", fn () i32)).call(.{}));

    try expectError(error.Trap, rt.instantiate(compiled));
    try expectEqualStrings("the calling instance has no context", rt.errorMessage());

    const wrong = try wasm.Runtime.init(std.testing.allocator, .{});
    defer wrong.deinit();
    try wrong.bindFunc("env", "ctx", hostIgnore);
    try expectError(error.TypeMismatch, wrong.checkImport(compiled, 0));
}

fn instantiateOnThread(compiled: *const wasm.CompiledModule, id: i32, seen: *i32) !void {
    const rt = try wasm.Runtime.init(std.heap.smp_allocator, .{});
    defer rt.deinit();
    try rt.bindInstanceFunc("env", "ctx", tenantId);
    const tenant: Tenant = .{ .id = id };
    for (0..20) |_| {
        const instance = try rt.instantiateContext(compiled, &tenant);
        defer instance.deinit();
        seen.* = try (try instance.func("seen", fn () i32)).call(.{});
    }
}

test "runtimes on other threads instantiate one compiled module" {
    if (@import("builtin").single_threaded) return error.SkipZigTest;
    const owner = try wasm.Runtime.init(std.heap.smp_allocator, .{});
    defer owner.deinit();
    const compiled = try owner.compile(&ctx_wasm);
    defer compiled.deinit();

    var seen: [4]i32 = @splat(0);
    var threads: [4]std.Thread = undefined;
    for (&threads, &seen, 0..) |*thread, *slot, i| {
        thread.* = try std.Thread.spawn(.{}, instantiateOnThread, .{ compiled, @as(i32, @intCast(i)) + 10, slot });
    }
    for (threads) |thread| thread.join();
    try expectEqual([4]i32{ 10, 11, 12, 13 }, seen);
}

test "tables, funcrefs, and registered modules" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const lib = try rt.load(&table_wasm);
    defer lib.deinit();

    const table = lib.table(0).?;
    try expectEqual(@as(?wasm.Table, null), lib.table(1));
    try expectEqual(wasm.ValType.funcref, table.elemType());
    try expectEqual(@as(u64, 2), table.size());
    const ref = try table.get(wasm.FuncRef, 0);
    try expectEqual(wasm.FuncRef.null, try table.get(wasm.FuncRef, 1));
    try expectError(error.OutOfBounds, table.get(wasm.FuncRef, 2));

    const double = try rt.funcRef(ref, fn (i32) i32);
    try expectEqual(@as(i32, 14), try double.call(.{7}));
    try expectError(error.TypeMismatch, rt.funcRef(ref, fn () void));
    try expectError(error.TypeMismatch, rt.funcRef(.null, fn (i32) i32));
    var results: [1]wasm.Value = undefined;
    try rt.callRef(ref, &.{.from(@as(i32, 4))}, &results);
    try expectEqual(@as(i32, 8), try results[0].to(i32));

    try table.set(1, ref);
    try expectEqual(@as(?u64, 2), table.grow(2, wasm.FuncRef.null));
    try expectEqual(@as(?u64, null), table.grow(1, wasm.FuncRef.null));
    try expectEqual(@as(u64, 4), table.size());
    try expectError(error.TypeMismatch, table.set(0, @as(i32, 1)));

    try rt.registerModule("lib", lib);
    const user = try rt.load(&link_wasm);
    defer user.deinit();
    try expectEqual(@as(i32, 12), try (try user.func("quad", fn (i32) i32)).call(.{3}));
}

const EnvHost = struct {
    pub fn log(state: *HostState, value: i32) void {
        state.logged = value;
    }

    pub fn add3(caller: *wasm.Instance, a: i32, b: i32, c: i32) i32 {
        _ = caller;
        return a + b + c;
    }

    pub fn pair() struct { i32, i64 } {
        return .{ -2, 1 << 40 };
    }

    pub fn fail() error{HostRefused}!void {
        return error.HostRefused;
    }

    pub fn echo(state: *HostState, caller: *wasm.Instance, ref: wasm.ExternRef) wasm.ExternRef {
        _ = caller;
        state.callers += 1;
        return ref;
    }
};

const EnvImports = struct {
    pub const log = fn (i32) void;
    pub const add3 = fn (i32, i32, i32) i32;
    pub const pair = fn () struct { i32, i64 };
    pub const fail = fn () void;
    pub const echo = fn (wasm.ExternRef) wasm.ExternRef;
    pub const base = wasm.Global(i32);
};

test "namespaces bind every host function" {
    comptime wasm.checkHost(EnvImports, EnvHost);
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    var state: HostState = .{};
    var base: wasm.Value = .from(@as(i32, 100));
    try rt.bindAllContext("env", EnvHost, &state);
    try rt.bindGlobal("env", "base", &base, true);
    const mod = try rt.load(&host_wasm);
    defer mod.deinit();
    try expectEqual(@as(i32, 103), try (try mod.func("run", fn () i32)).call(.{}));
    try expectEqual(@as(i32, 42), state.logged);
}

test "typed memory access" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();
    const memory = mod.memory(0).?;

    try memory.store(u32, 200, 0xdead_beef);
    try expectEqual(@as(u8, 0xef), memory.data()[200]);
    try expectEqual(@as(u32, 0xdead_beef), try memory.load(u32, 200));
    try memory.store(f64, 208, -1.5);
    try expectEqual(@as(f64, -1.5), try memory.load(f64, 208));
    try expectError(error.OutOfBounds, memory.load(u64, wasm.page_size - 4));
    try expectError(error.OutOfBounds, memory.slice(wasm.page_size, 1));
    try expectEqual(@as(usize, 0), (try memory.slice(wasm.page_size, 0)).len);
}

test "config sets features and fuel" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{ .features = .{ .simd = false, .multi_value = true }, .fuel = 50 });
    defer rt.deinit();
    try expect(!rt.features().simd);
    try expectEqual(@as(u64, 50), rt.fuel());
    try expectEqual(@as(?wasm.Error, null), rt.lastError());
}

const ctx_wasm = [_]u8{
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60, 0x00, 0x01, 0x7f, 0x60,
    0x00, 0x00, 0x02, 0x0b, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x03, 0x63, 0x74, 0x78, 0x00, 0x00, 0x03,
    0x03, 0x02, 0x01, 0x00, 0x06, 0x06, 0x01, 0x7f, 0x01, 0x41, 0x00, 0x0b, 0x07, 0x08, 0x01, 0x04,
    0x73, 0x65, 0x65, 0x6e, 0x00, 0x02, 0x08, 0x01, 0x01, 0x0a, 0x0d, 0x02, 0x06, 0x00, 0x10, 0x00,
    0x24, 0x00, 0x0b, 0x04, 0x00, 0x23, 0x00, 0x0b,
};

const table_wasm = [_]u8{
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,
    0x03, 0x02, 0x01, 0x00, 0x04, 0x05, 0x01, 0x70, 0x01, 0x02, 0x04, 0x07, 0x12, 0x02, 0x05, 0x74,
    0x61, 0x62, 0x6c, 0x65, 0x01, 0x00, 0x06, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x09,
    0x07, 0x01, 0x00, 0x41, 0x00, 0x0b, 0x01, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x41,
    0x02, 0x6c, 0x0b,
};

const link_wasm = [_]u8{
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f,
    0x02, 0x0e, 0x01, 0x03, 0x6c, 0x69, 0x62, 0x06, 0x64, 0x6f, 0x75, 0x62, 0x6c, 0x65, 0x00, 0x00,
    0x03, 0x02, 0x01, 0x00, 0x07, 0x08, 0x01, 0x04, 0x71, 0x75, 0x61, 0x64, 0x00, 0x01, 0x0a, 0x0a,
    0x01, 0x08, 0x00, 0x20, 0x00, 0x10, 0x00, 0x10, 0x00, 0x0b,
};

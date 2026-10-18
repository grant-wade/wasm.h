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
    const rt = try wasm.Runtime.init(.{});
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
    const rt = try wasm.Runtime.init(.{});
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
    const rt = try wasm.Runtime.init(.{});
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
    const rt = try wasm.Runtime.init(.{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    const div = try mod.func("div", fn (i32, i32) i32);
    try expectEqual(@as(i32, 3), try div.call(.{ 9, 3 }));
    try expectError(error.DivByZero, div.call(.{ 1, 0 }));
    const frame = rt.callFrame(0).?;
    try expectEqual(@as(?*wasm.Module, mod), frame.module);
    try expectError(error.IntOverflow, div.call(.{ std.math.minInt(i32), -1 }));
    try expect(rt.errorMessage().len > 0);

    rt.setFuel(10_000);
    const spin = try mod.func("spin", fn () void);
    try expectError(error.OutOfFuel, spin.call(.{}));
    try expectEqual(@as(u64, 0), rt.fuel());
    try expectError(error.OutOfFuel, div.call(.{ 9, 3 }));
    rt.disableFuel();
    try expectEqual(@as(i32, 3), try div.call(.{ 9, 3 }));
}

test "wasi configuration" {
    const rt = try wasm.Runtime.init(.{});
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
    const rt = try wasm.Runtime.init(.{});
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
    const rt = try wasm.Runtime.init(.{});
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

    try expectEqual(@as(u32, 2), mod.globalCount());
    try expectEqual(wasm.ValType.i32, mod.globalType(0));
    try expect(mod.globalIsMutable(0));
    try expect(!mod.globalIsMutable(1));
}

test "linear memory" {
    const rt = try wasm.Runtime.init(.{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    try expectEqual(@as(u32, 1), mod.memoryCount());
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
    const rt = try wasm.Runtime.init(.{});
    defer rt.deinit();
    const mod = try rt.load(&basic_wasm);
    defer mod.deinit();

    var names: [16][]const u8 = undefined;
    var count: usize = 0;
    var it = mod.exports();
    while (it.next()) |exp| : (count += 1) names[count] = exp.name;
    try expectEqual(@as(usize, mod.exportCount()), count);
    try expectEqualStrings("memory", names[0]);
    try expectEqualStrings("bump", names[count - 1]);

    const add = mod.findExport("add").?;
    try expectEqual(wasm.ExternKind.func, add.kind);
    try expectEqual(@as(?wasm.Export, null), mod.findExport("nope"));
    const add_type = mod.funcType(add.index).?;
    try expectEqualSlices(wasm.ValType, &.{ .i32, .i32 }, add_type.params);
    try expectEqualSlices(wasm.ValType, &.{.i32}, add_type.results);
    try expect(add_type.matches(fn (i32, i32) i32));
    try expect(!add_type.matches(fn (i32, i32) void));
    try expectEqual(wasm.TypeKind.func, mod.typeKind(0));

    try expectEqualStrings("abc", mod.customSection("meta").?);
    try expectEqual(@as(?[]const u8, null), mod.customSection("other"));
    var sections = mod.customSections();
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

fn hostAdd3(caller: *wasm.Module, a: i32, b: i32, c: i32) i32 {
    _ = caller.runtime();
    return a + b + c;
}

fn hostPair() struct { i32, i64 } {
    return .{ -2, 1 << 40 };
}

fn hostFail() error{HostRefused}!void {
    return error.HostRefused;
}

fn hostEcho(state: *HostState, caller: *wasm.Module, ref: wasm.ExternRef) wasm.ExternRef {
    _ = caller;
    state.callers += 1;
    return ref;
}

test "host functions and globals" {
    const rt = try wasm.Runtime.init(.{});
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

    try expectEqual(@as(u32, 6), mod.importCount());
    const log_import = mod.importAt(0);
    try expectEqualStrings("env", log_import.module);
    try expectEqualStrings("log", log_import.name);
    try expectEqual(wasm.ExternKind.func, log_import.kind);
    try expectEqual(wasm.ExternKind.global, mod.importAt(5).kind);
}

fn hostIgnore() void {}

test "host callbacks accept null buffers for empty arguments and results" {
    const Host = struct {
        fn start(count: *u32) void {
            count.* += 1;
        }
    };
    const rt = try wasm.Runtime.init(.{});
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
        const rt = try wasm.Runtime.init(.{});
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
    const rt = try wasm.Runtime.init(.{});
    defer rt.deinit();
    try expectError(error.UnknownImport, rt.load(&host_wasm));
    try expect(rt.errorMessage().len > 0);
    try expectError(error.InvalidMagic, rt.load("not wasm"));
}

test "compiled modules are inspected without imports and instantiated repeatedly" {
    const rt = try wasm.Runtime.init(.{});
    defer rt.deinit();

    const unbound = try rt.compile(&host_wasm);
    defer unbound.deinit();
    try expectError(error.UnknownImport, unbound.instantiate());

    const compiled = try rt.compile(&basic_wasm);
    defer compiled.deinit();
    try expectError(error.Malformed, (try compiled.func("add", fn (i32, i32) i32)).call(.{ 1, 2 }));
    const first = try compiled.instantiate();
    defer first.deinit();
    const second = try compiled.instantiate();
    defer second.deinit();
    try expectEqual(@as(i32, 3), try (try first.func("add", fn (i32, i32) i32)).call(.{ 1, 2 }));
    try expectEqual(@as(i32, 7), try (try second.func("add", fn (i32, i32) i32)).call(.{ 3, 4 }));
}

test "features" {
    const rt = try wasm.Runtime.init(.{ .lazy_validation = false });
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
    const shared = try wasm.SharedMemory.init(.{ .pages = 1, .max_pages = 4 });
    defer shared.deinit();

    var modules: [2]*wasm.Module = undefined;
    var runtimes: [2]*wasm.Runtime = undefined;
    for (&runtimes, &modules) |*rt, *mod| {
        rt.* = try wasm.Runtime.init(.{});
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
    const rt = try wasm.Runtime.init(.{});
    defer rt.deinit();
    const mod = try rt.load(&gc_wasm);
    defer mod.deinit();

    try expectEqual(wasm.TypeKind.@"struct", mod.typeKind(0));
    try expectEqual(wasm.TypeKind.array, mod.typeKind(1));
    try expectEqual(@as(?wasm.FuncType, null), mod.typeFunc(0));

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

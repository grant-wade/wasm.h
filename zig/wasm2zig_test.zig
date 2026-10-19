const std = @import("std");
const wasm = @import("wasm");
const guest = @import("guest");

const expect = std.testing.expect;
const expectEqual = std.testing.expectEqual;
const expectEqualStrings = std.testing.expectEqualStrings;

const State = struct {
    logged: i32 = 0,
    written: [8]u8 = undefined,
    written_len: usize = 0,
};

const Env = struct {
    pub fn log(state: *State, value: i32) void {
        state.logged = value;
    }

    pub fn now() i64 {
        return 1234;
    }
};

const HostIo = struct {
    pub fn write(state: *State, caller: *wasm.Instance, ptr: u32, len: u32) !i32 {
        const bytes = try caller.memory(0).?.slice(ptr, len);
        @memcpy(state.written[0..bytes.len], bytes);
        state.written_len = bytes.len;
        return @intCast(len);
    }
};

const Refined = struct {
    add: fn (u32, u32) u32,
    divmod: fn (i32, i32) struct { i32, wasm.Value },
    run: ?fn () i64,
    counter: wasm.Global(u32),
    missing: ?fn () void,
};

comptime {
    wasm.checkHost(guest.Imports.env, Env);
    wasm.checkHost(guest.Imports.@"host-io", HostIo);
    wasm.checkExports(guest.Exports, guest.Exports);
    wasm.checkExports(guest.Exports, Refined);
}

test "generated types describe the module" {
    try expectEqual(fn (i32, i32) i32, @FieldType(guest.Exports, "add"));
    try expectEqual(fn (i32, i32) struct { i32, i32 }, @FieldType(guest.Exports, "divmod"));
    try expectEqual(fn (wasm.V128, wasm.ExternRef) wasm.ExternRef, @FieldType(guest.Exports, "type"));
    try expectEqual(wasm.Global(i32), @FieldType(guest.Exports, "counter"));
    try expectEqual(wasm.Global(f64), @FieldType(guest.Exports, "ratio"));
    try expectEqual(wasm.Memory, @FieldType(guest.Exports, "memory"));
    try expectEqual(wasm.Table, @FieldType(guest.Exports, "table"));
    try expectEqual(fn (i32, i32) i32, guest.Imports.@"host-io".write);
    try expectEqual(fn () i64, guest.Imports.env.now);
    try expectEqual(@as(usize, 2), @typeInfo(guest.Imports).@"struct".decl_names.len);
}

test "generated exports bind and run" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    var state: State = .{};
    var limit: wasm.Value = .from(@as(i32, 40));
    try rt.bindAllContext("env", Env, &state);
    try rt.bindAllContext("host-io", HostIo, &state);
    try rt.bindGlobal("env", "limit", &limit, false);

    const compiled = try rt.compile(@embedFile("testdata/guest.wasm"));
    defer compiled.deinit();
    const plan = try compiled.resolve(guest.Exports);
    const instance = try rt.instantiate(compiled);
    defer instance.deinit();
    const api = plan.on(instance);

    try expectEqual(@as(i32, 5), try api.add.call(.{ 2, 3 }));
    const q, const r = try api.divmod.call(.{ 17, 5 });
    try expectEqual(@as(i32, 3), q);
    try expectEqual(@as(i32, 2), r);
    try expectEqual(@as(i64, 1234), try api.run.call(.{}));
    try expectEqual(@as(i32, 42), state.logged);
    try expectEqualStrings("hi", state.written[0..state.written_len]);
    try expectEqual(@as(i32, 3), try api.counter.get());
    try expectEqual(@as(f64, 0.5), try api.ratio.get());
    try expectEqual(@as(u64, wasm.page_size), api.memory.size());
    try expectEqual(@as(u64, 1), api.table.size());
    try expectEqual(wasm.ExternRef.fromPtr(&state), try api.type.call(.{ @splat(0), .fromPtr(&state) }));
}

test "generated Api calls exports through methods" {
    const rt = try wasm.Runtime.init(std.testing.allocator, .{});
    defer rt.deinit();
    var state: State = .{};
    var limit: wasm.Value = .from(@as(i32, 40));
    try rt.bindAllContext("env", Env, &state);
    try rt.bindAllContext("host-io", HostIo, &state);
    try rt.bindGlobal("env", "limit", &limit, false);

    const compiled = try rt.compile(@embedFile("testdata/guest.wasm"));
    defer compiled.deinit();
    const instance = try rt.instantiate(compiled);
    defer instance.deinit();

    const api: guest.Api(guest.Exports) = .{ .exports = try instance.exportsAs(guest.Exports) };
    try expectEqual(@as(i32, 5), try api.add(2, 3));
    const q, const r = try api.divmod(17, 5);
    try expectEqual(@as(i32, 3), q);
    try expectEqual(@as(i32, 2), r);
    try expectEqual(@as(i64, 1234), try api.run());
    try expectEqual(@as(i32, 3), try api.counter().get());
    try expectEqual(@as(u64, wasm.page_size), api.memory().size());
    try expectEqual(wasm.ExternRef.fromPtr(&state), try api.type(@splat(0), .fromPtr(&state)));

    const refined: guest.Api(Refined) = .{ .exports = try instance.exportsAs(Refined) };
    try expectEqual(@as(u32, 7), try refined.add(3, 4));
    const q2, const r2 = try refined.divmod(9, 4);
    try expectEqual(@as(i32, 2), q2);
    try expectEqual(@as(i32, 1), try r2.to(i32));
    try expectEqual(@as(i64, 1234), try refined.run());
    try expectEqual(@as(u32, 3), try refined.counter().get());
    try std.testing.expectError(error.UndefinedExport, wasm.callExport(refined.exports, "missing", .{}));
}

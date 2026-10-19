const std = @import("std");
const wasm = @import("wasm");
const calc = @import("calc");

const print = std.debug.print;

const Status = enum(i32) { ok, syntax_error, unknown_variable, divide_by_zero };

const Result = extern struct {
    value: f64,
    error_offset: u32,
    steps_done: u32,
};

const Level = enum(i32) { debug, info, warn };

/// `calc.Exports` with Zig types for the guest's pointers and statuses.
const Calc = struct {
    memory: wasm.Memory,
    alloc: fn (u32) u32,
    reset: fn () void,
    eval: fn (src: u32, len: u32, out: u32) Status,
    integrate: fn (src: u32, len: u32, a: f64, b: f64, steps: u32, out: u32) Status,
};

/// The variables one instance's expressions can read.
const Session = struct {
    name: []const u8,
    variables: []const struct { []const u8, f64 },

    fn get(session: *const Session, name: []const u8) ?f64 {
        for (session.variables) |variable| {
            if (std.mem.eql(u8, variable[0], name)) return variable[1];
        }
        return null;
    }
};

const Host = struct {
    pub fn log(level: Level, message: wasm.GuestBytes) void {
        print("  [guest {t}] {s}\n", .{ level, message.bytes() });
    }

    pub fn lookup(session: *const Session, name: wasm.GuestBytes, out: wasm.GuestPtr(f64)) !bool {
        try out.store(session.get(name.bytes()) orelse return false);
        return true;
    }

    pub fn call(name: wasm.GuestBytes, x: f64) !f64 {
        const function = std.meta.stringToEnum(enum { sin, cos, sqrt, exp, ln, abs }, name.bytes()) orelse
            return error.UnknownFunction;
        return switch (function) {
            .sin => @sin(x),
            .cos => @cos(x),
            .sqrt => @sqrt(x),
            .exp => @exp(x),
            .ln => @log(x),
            .abs => @abs(x),
        };
    }
};

comptime {
    wasm.checkHost(calc.Imports.host, Host);
}

/// One instance of the calculator.
const Calculator = struct {
    api: calc.Api(Calc),

    const Input = struct { src: u32, len: u32, out: u32 };

    /// `source` copied into guest memory, with room for a `Result`. Frees earlier inputs.
    fn input(self: Calculator, source: []const u8) !Input {
        try self.api.reset();
        const len: u32 = @intCast(source.len);
        const src = try self.api.alloc(len);
        const out = try self.api.alloc(@sizeOf(Result));
        if (src == 0 or out == 0) return error.OutOfMemory;
        try self.api.memory().write(src, source);
        return .{ .src = src, .len = len, .out = out };
    }

    fn eval(self: Calculator, source: []const u8) !struct { Status, Result } {
        const in = try self.input(source);
        const status = try self.api.eval(in.src, in.len, in.out);
        return .{ status, try self.api.memory().load(Result, in.out) };
    }
};

pub fn main(init: std.process.Init) !void {
    const rt = try wasm.Runtime.init(init.gpa, .{
        .max_memory_bytes = 1024 * 1024,
        .max_call_depth = 512,
    });
    defer rt.deinit();

    try rt.bindFunc("host", "log", Host.log);
    try rt.bindFunc("host", "call", Host.call);
    try rt.bindInstanceFunc("host", "lookup", Host.lookup);

    const compiled = rt.compile(@embedFile("calc.wasm")) catch |err| {
        print("compile failed ({t}): {s}\n", .{ err, rt.errorMessage() });
        return err;
    };
    defer compiled.deinit();
    printExports(compiled.info());

    var missing = rt.unresolvedImports(compiled);
    while (missing.next()) |problem| {
        print("unresolved import {s}.{s}: {s}\n", .{ problem.import.module, problem.import.name, problem.message });
        return problem.err;
    }
    const plan = try compiled.resolve(Calc);

    const sessions = [_]Session{
        .{ .name = "metric", .variables = &.{ .{ "g", 9.81 }, .{ "t", 3 } } },
        .{ .name = "imperial", .variables = &.{ .{ "g", 32.17 }, .{ "t", 3 } } },
    };
    print("\nfall distance g * t * t / 2 per session:\n", .{});
    for (&sessions) |*session| {
        const instance = try rt.instantiateContext(compiled, session);
        defer instance.deinit();
        const calculator: Calculator = .{ .api = .{ .exports = plan.on(instance) } };
        const status, const result = try calculator.eval("g * t * t / 2");
        print("  {s}: {t} {d:.2}\n", .{ session.name, status, result.value });
    }

    const instance = try rt.instantiateContext(compiled, &sessions[0]);
    defer instance.deinit();
    const calculator: Calculator = .{ .api = .{ .exports = plan.on(instance) } };
    try evalAll(rt, calculator, &.{ "sqrt(2) * sqrt(2)", "2 * (3 + )", "h + 1", "1 / (t - 3)", "frob(2)" });
    try integrate(rt, calculator, "sin(x) * sin(x)", 20_000);
}

fn printExports(info: *const wasm.ModuleInfo) void {
    print("calc.wasm exports:\n", .{});
    var exports = info.exports();
    while (exports.next()) |exp| switch (exp.kind) {
        .func => print("  {s} {f}\n", .{ exp.name, info.funcType(exp.index).? }),
        else => print("  {s} ({t})\n", .{ exp.name, exp.kind }),
    };
}

fn evalAll(rt: *wasm.Runtime, calculator: Calculator, sources: []const []const u8) !void {
    print("\nexpressions:\n", .{});
    for (sources) |source| {
        const status, const result = calculator.eval(source) catch |err| {
            print("  {s}\n    trapped ({t}): {s}\n", .{ source, err, rt.errorMessage() });
            var frames = rt.frames();
            while (frames.next()) |frame| print("      {f}\n", .{frame});
            continue;
        };
        if (status == .ok) {
            print("  {s} = {d:.6}\n", .{ source, result.value });
        } else {
            print("  {[source]s}\n  {[caret]s: >[width]} {[status]t}\n", .{ .source = source, .caret = "^", .width = result.error_offset + 1, .status = status });
        }
    }
}

fn integrate(rt: *wasm.Runtime, calculator: Calculator, source: []const u8, steps: u32) !void {
    const fuel_per_slice = 6_000_000;
    print("\nintegrating {s} over [0, pi] in slices of {d} fuel:\n", .{ source, fuel_per_slice });
    const in = try calculator.input(source);
    rt.setFuel(fuel_per_slice);
    defer rt.disableFuel();
    const func = calculator.api.exports.integrate;
    var outcome = try func.start(.{ in.src, in.len, 0, std.math.pi, steps, in.out });
    while (outcome == .suspended) {
        const progress = try calculator.api.memory().load(Result, in.out);
        print("  {t} at step {d} of {d}, partial sum {d:.6}\n", .{ outcome.suspended, progress.steps_done, steps, progress.value });
        rt.setFuel(fuel_per_slice);
        outcome = try func.resumeWith(.{});
    }
    const result = try calculator.api.memory().load(Result, in.out);
    print("  {t}: {d:.6}\n", .{ outcome.done, result.value });
}

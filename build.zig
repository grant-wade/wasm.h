const std = @import("std");

const cross_targets = [_][]const u8{
    "x86_64-linux-gnu",
    "aarch64-linux-musl",
    "x86-linux-musl",
    "arm-linux-musleabihf",
    "riscv64-linux-musl",
    "s390x-linux-musl",
    "x86_64-windows-gnu",
    "aarch64-windows-gnu",
    "x86-windows-gnu",
    "aarch64-macos",
    "x86_64-macos",
    "x86_64-freebsd",
    "x86_64-netbsd",
    "x86_64-openbsd",
    "wasm32-wasi",
};

const RuntimeOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.Optimize,
    platform: bool,
    werror: bool,
    sources: []const []const u8,
};

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const platform = b.option(bool, "platform", "Compile the WASI, Emscripten, and thread-blocking helpers (default: on for OS targets)") orelse
        defaultPlatform(target.result);
    const werror = b.option(bool, "werror", "Treat C compiler warnings as errors") orelse false;
    const sources = try runtimeSources(b);

    const runtime = addRuntime(b, .{
        .target = target,
        .optimize = optimize,
        .platform = platform,
        .werror = werror,
        .sources = sources,
    }, true);
    b.installArtifact(runtime.library);

    const options = b.addOptions();
    options.addOption([]const u8, "package_version", @import("build.zig.zon").version);
    const wrapper_tests = b.addTest(.{
        .name = "wasm_zig_test",
        .root_module = b.createModule(.{
            .root_source_file = b.path("zig/wasm_test.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{ .name = "wasm", .module = runtime.module },
                .{ .name = "build_options", .module = options.createModule() },
            },
        }),
    });

    const test_step = b.step("test", "Run the Zig wrapper tests and the modular C regression tests");
    test_step.dependOn(&b.addRunArtifact(wrapper_tests).step);
    test_step.dependOn(cTests(b, target, optimize, platform, werror, runtime.library));

    const cross_step = b.step("cross", "Compile the runtime and wrapper tests for every target in cross_targets");
    for (cross_targets) |triple| {
        const cross_target = b.resolveTargetQuery(try std.Target.Query.parse(.{ .arch_os_abi = triple }));
        const cross = addRuntime(b, .{
            .target = cross_target,
            .optimize = optimize,
            .platform = defaultPlatform(cross_target.result),
            .werror = werror,
            .sources = sources,
        }, false);
        const cross_tests = b.addTest(.{
            .name = b.fmt("wasm_zig_test-{s}", .{triple}),
            .root_module = b.createModule(.{
                .root_source_file = b.path("zig/wasm_test.zig"),
                .target = cross_target,
                .optimize = optimize,
                .imports = &.{
                    .{ .name = "wasm", .module = cross.module },
                    .{ .name = "build_options", .module = options.createModule() },
                },
            }),
        });
        cross_step.dependOn(&cross_tests.step);
    }
}

const Runtime = struct {
    library: *std.Build.Step.Compile,
    module: *std.Build.Module,
};

/// `public` exposes the `wasm` and `wasm_c` modules to packages that depend on this one.
fn addRuntime(b: *std.Build, opts: RuntimeOptions, public: bool) Runtime {
    const library = addRuntimeLibrary(b, "wasm", opts);
    library.installHeader(b.path("src/wasm.h"), "wasm.h");

    const translate = b.addTranslateC(.{
        .root_source_file = b.path("src/wasm.h"),
        .target = opts.target,
        .optimize = opts.optimize,
    });
    const c_module = if (public) translate.addModule("wasm_c") else translate.createModule();
    c_module.linkLibrary(library);

    const module_options: std.Build.Module.CreateOptions = .{
        .root_source_file = b.path("zig/wasm.zig"),
        .target = opts.target,
        .optimize = opts.optimize,
        .imports = &.{.{ .name = "wasm_c", .module = c_module }},
    };
    const module = if (public) b.addModule("wasm", module_options) else b.createModule(module_options);
    return .{ .library = library, .module = module };
}

fn addRuntimeLibrary(b: *std.Build, name: []const u8, opts: RuntimeOptions) *std.Build.Step.Compile {
    const module = b.createModule(.{
        .target = opts.target,
        .optimize = opts.optimize,
        .link_libc = true,
    });
    var flags: std.ArrayList([]const u8) = .empty;
    flags.appendSlice(b.allocator, &.{ "-std=c99", "-Wall", "-Wextra", "-Wmissing-prototypes" }) catch @panic("OOM");
    if (opts.werror) flags.append(b.allocator, "-Werror") catch @panic("OOM");
    module.addCSourceFiles(.{ .root = b.path("src"), .files = opts.sources, .flags = flags.items });
    module.addIncludePath(b.path("src"));
    module.addCMacro("_POSIX_C_SOURCE", "200809L");
    // NetBSD declares fchmod and fchown only for X/Open or NetBSD sources.
    if (opts.target.result.os.tag == .netbsd) module.addCMacro("_NETBSD_SOURCE", "1");
    module.addCMacro("WASM_ENABLE_PLATFORM", if (opts.platform) "1" else "0");

    const library = b.addLibrary(.{ .name = name, .root_module = module, .linkage = .static });
    // Wasm float semantics need binary32/binary64 rounding; x87 extended precision breaks them.
    if (opts.target.result.cpu.arch == .x86 and !opts.target.result.cpu.has(.x86, .sse2)) {
        library.step.dependOn(&b.addFail("wasm.h requires SSE2 on 32-bit x86 targets (try -Dcpu=pentium4 or newer)").step);
    }
    return library;
}

fn cTests(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.Optimize,
    platform: bool,
    werror: bool,
    runtime: *std.Build.Step.Compile,
) *std.Build.Step {
    const step = b.step("c-test", "Run the modular C regression tests against the Zig-built runtime");
    const cases = [_]struct { name: []const u8, source: []const u8, runtime: *std.Build.Step.Compile }{
        .{ .name = "wasm_modular_test", .source = "test/wasm_test.c", .runtime = runtime },
        .{ .name = "wasm_modular_core_test", .source = "test/wasm_core_test.c", .runtime = runtime },
    };
    for (cases) |case| {
        const module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true });
        module.addCSourceFile(.{
            .file = b.path(case.source),
            .flags = if (werror) &.{ "-std=c99", "-Wall", "-Wextra", "-Werror" } else &.{ "-std=c99", "-Wall", "-Wextra" },
        });
        module.addIncludePath(b.path("src"));
        module.addCMacro("_POSIX_C_SOURCE", "200809L");
        module.addCMacro("WASM_TEST_MODULAR", "1");
        module.addCMacro("WASM_ENABLE_PLATFORM", if (platform) "1" else "0");
        module.addCMacro("WL_ENABLE_PLATFORM", if (platform) "1" else "0");
        module.linkLibrary(case.runtime);
        const exe = b.addExecutable(.{ .name = case.name, .root_module = module });
        const run = b.addRunArtifact(exe);
        run.setCwd(b.path("."));
        run.expectExitCode(0);
        step.dependOn(&run.step);
    }
    return step;
}

fn runtimeSources(b: *std.Build) ![]const []const u8 {
    const io = b.graph.io;
    var dir = try b.root.openDir(io, "src", .{ .iterate = true });
    defer dir.close(io);
    b.dependOnDirectoryContents(b.path("src"));

    var sources: std.ArrayList([]const u8) = .empty;
    var walker = try dir.walk(b.allocator);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        switch (entry.kind) {
            .directory => b.dependOnDirectoryContents(b.path(b.pathJoin(&.{ "src", entry.path }))),
            .file => if (std.mem.endsWith(u8, entry.basename, ".c")) {
                const path = try b.allocator.dupe(u8, entry.path);
                std.mem.replaceScalar(u8, path, '\\', '/');
                try sources.append(b.allocator, path);
            },
            else => {},
        }
    }
    std.mem.sort([]const u8, sources.items, {}, lessThan);
    return sources.items;
}

fn lessThan(_: void, lhs: []const u8, rhs: []const u8) bool {
    return std.mem.lessThan(u8, lhs, rhs);
}

/// The platform helpers need a hosted OS; Wasm, Emscripten, and freestanding targets build without them.
fn defaultPlatform(target: std.Target) bool {
    return switch (target.os.tag) {
        .linux, .windows, .freebsd, .netbsd, .openbsd, .dragonfly, .illumos => true,
        else => target.os.tag.isDarwin(),
    };
}

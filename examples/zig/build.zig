const std = @import("std");
const wasm_h_build = @import("wasm_h");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const guest = b.addExecutable(.{
        .name = "calc",
        .root_module = b.createModule(.{
            .target = b.resolveTargetQuery(.{ .cpu_arch = .wasm32, .os_tag = .freestanding }),
            .optimize = .ReleaseSmall,
            .strip = false,
        }),
    });
    guest.root_module.addCSourceFile(.{ .file = b.path("guest/calc.c"), .flags = &.{ "-std=c11", "-Wall", "-Wextra", "-Werror" } });
    guest.entry = .disabled;
    guest.stack_size = 64 * 1024;
    b.installArtifact(guest);

    const wasm_h = b.dependency("wasm_h", .{ .target = target, .optimize = optimize });
    const exe = b.addExecutable(.{
        .name = "calc-host",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{ .name = "wasm", .module = wasm_h.module("wasm") },
                .{ .name = "calc", .module = wasm_h_build.addBindings(b, wasm_h, guest.getEmittedBin()) },
            },
        }),
    });
    exe.root_module.addAnonymousImport("calc.wasm", .{ .root_source_file = guest.getEmittedBin() });
    b.installArtifact(exe);

    const run = b.addRunArtifact(exe);
    b.step("run", "Run the calculator host").dependOn(&run.step);
}

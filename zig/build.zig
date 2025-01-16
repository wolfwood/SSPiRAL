const std = @import("std");
//const deps = @import("./deps.zig");

const feature = @import("std").Target.x86.Feature;

const forceAvx2 = false;
const fullAvx512 = true;
const profile = false;
const forceNoBitManip = false;

pub fn build(b: *std.Build) void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    var target = b.standardTargetOptions(.{});

    if (forceAvx2) {
        target.result.cpu.features.removeFeature(@intFromEnum(feature.avx512bw));
        target.result.cpu.features.removeFeature(@intFromEnum(feature.avx512cd));
        target.result.cpu.features.removeFeature(@intFromEnum(feature.avx512dq));
        target.result.cpu.features.removeFeature(@intFromEnum(feature.avx512vl));

        target.result.cpu.features.addFeature(@intFromEnum(feature.avx2));
    } else if (fullAvx512) {
        target.result.cpu.features.removeFeature(@intFromEnum(feature.prefer_256_bit));
    }

    if (forceNoBitManip) {
        target.result.cpu.features.removeFeature(@intFromEnum(feature.bmi));
    }

    // Standard release options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall.
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "zuppa",
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    //if (profile) {
    //    deps.addAllTo(exe);
    //}

    b.installArtifact(exe);

    const run_exe = b.addRunArtifact(exe);

    const run_step = b.step("run", "Run the application");
    run_step.dependOn(&run_exe.step);

    const exe_tests = b.addTest(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&exe_tests.step);
}

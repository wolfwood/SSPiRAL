const std = @import("std");
const deps = @import("./deps.zig");

const feature = @import("std").Target.x86.Feature;

const forceAvx2 = false;
const fullAvx512 = true;
const profile = false;

pub fn build(b: *std.build.Builder) void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    var target = b.standardTargetOptions(.{});

    if (forceAvx2) {
        target.cpu_features_sub.addFeature(@enumToInt(feature.avx512bw));
        target.cpu_features_sub.addFeature(@enumToInt(feature.avx512cd));
        target.cpu_features_sub.addFeature(@enumToInt(feature.avx512dq));
        target.cpu_features_sub.addFeature(@enumToInt(feature.avx512vl));

        target.cpu_features_add.addFeature(@enumToInt(feature.avx2));
    } else if (fullAvx512) {
        target.cpu_features_sub.addFeature(@enumToInt(feature.prefer_256_bit));
    }

    // Standard release options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall.
    const mode = b.standardReleaseOptions();

    const exe = b.addExecutable("zuppa", "src/main.zig");
    exe.setTarget(target);
    exe.setBuildMode(mode);

    if (profile) {
        deps.addAllTo(exe);
    }

    exe.install();

    const run_cmd = exe.run();
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    const exe_tests = b.addTest("src/main.zig");
    exe_tests.setTarget(target);
    exe_tests.setBuildMode(mode);

    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&exe_tests.step);
}

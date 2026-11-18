const std = @import("std");
const Build = std.Build;
const builtin = @import("builtin");

const source_files = [_][]const u8{
    "AABBCollision.cpp",
    "Camera.cpp",
    "Core.cpp",
    "Game.cpp",
    "Input.cpp",
    "Map.cpp",
    "SoundManager.cpp",
    "TextureManager.cpp",
    "command/CommandScheduler.cpp",
    "main.cpp",
    "scenes/GameOverScene.cpp",
    "scenes/GameScene.cpp",
    "scenes/MenuScene.cpp",
    "systems/AnimationSystem.cpp",
    "systems/CallbackSystem.cpp",
    "systems/CollectibleSystem.cpp",
    "systems/EnemySystem.cpp",
    "systems/FlagSystem.cpp",
    "systems/MapSystem.cpp",
    "systems/MenuSystem.cpp",
    "systems/OptionsSystem.cpp",
    "systems/PhysicsSystem.cpp",
    "systems/PlayerSystem.cpp",
    "systems/RenderSystem.cpp",
    "systems/ScoreSystem.cpp",
    "systems/SoundSystem.cpp",
    "systems/WarpSystem.cpp",
};
const resource_dirs = [_][]const u8{ "data", "fonts", "sounds", "sprites" };

pub fn build(b: *Build) void {
    var target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const web = target.result.os.tag == .emscripten;
    const emsdk = b.dependency("emsdk", .{});
    b.step("install-emsdk", "Install Emscripten 7.0.0 in zig-pkg").dependOn(emSdkInstallStep(b, emsdk));
    if (web) {
        var query = target.query;
        query.cpu_features_add.addFeature(@backingInt(std.Target.wasm.Feature.atomics));
        query.cpu_features_add.addFeature(@backingInt(std.Target.wasm.Feature.bulk_memory));
        target = b.resolveTargetQuery(query);
    }
    const sysroot: ?Build.LazyPath = if (web) emsdk.path("upstream/emscripten/cache/sysroot/include") else null;
    const dependency_options = .{ .target = target, .optimize = .fast, .emscripten_include = sysroot };
    const sdl = b.dependency("sdl", dependency_options).artifact("SDL2");
    const image = b.dependency("sdl_image", dependency_options).artifact("SDL2_image");
    const mixer = b.dependency("sdl_mixer", dependency_options).artifact("SDL2_mixer");
    const ttf = b.dependency("sdl_ttf", dependency_options).artifact("SDL2_ttf");
    const mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true, .link_libcpp = true });
    const flags = [_][]const u8{ "-std=c++17", "-Wall", "-Wextra", "-Wno-unused-parameter" };
    mod.addCSourceFiles(.{ .root = b.path("src"), .files = &source_files, .flags = if (web) &(flags ++ .{"-fno-sanitize=undefined"}) else &flags });
    mod.addIncludePath(b.path("include"));
    for ([_]*Build.Step.Compile{ sdl, image, mixer, ttf }) |lib| {
        mod.addIncludePath(lib.getEmittedIncludeTree());
        mod.addIncludePath(lib.getEmittedIncludeTree().path(b, "SDL2"));
        mod.linkLibrary(lib);
    }
    if (web) {
        const lib = b.addLibrary(.{ .name = "super-mario-bros", .root_module = mod });
        for (lib.getCompileDependencies(false)) |dependency| {
            for (dependency.root_module.getGraph().modules) |module| module.addSystemIncludePath(sysroot.?);
        }
        const install = emLinkStep(b, emsdk, lib, optimize);
        b.getInstallStep().dependOn(&install.step);
        b.installFile("web_assets/coi-serviceworker.js", "web/coi-serviceworker.js");
        const run = b.addRunFile(emTool(b, emsdk, "emrun"));
        run.addFileArg(.{ .relative = .{ .base = .install_prefix, .sub_path = "web/index.html" } });
        run.addPassthruArgs();
        run.step.dependOn(b.getInstallStep());
        b.step("run", "Run Super Mario Bros in a browser").dependOn(&run.step);
    } else {
        if (target.result.os.tag == .windows) mod.addWin32ResourceFile(.{ .file = b.path("res/super_mario_bros.rc"), .include_paths = &.{b.path("res")} });
        const exe = b.addExecutable(.{ .name = "super-mario-bros", .root_module = mod });
        b.installArtifact(exe);
        for (resource_dirs) |dir| b.installDirectory(.{ .source_dir = b.path(b.fmt("res/{s}", .{dir})), .install_dir = .bin, .install_subdir = b.fmt("res/{s}", .{dir}) });
        const filename = if (target.result.os.tag == .windows) "super-mario-bros.exe" else "super-mario-bros";
        const run = b.addRunFile(.{ .relative = .{ .base = .install_prefix, .sub_path = b.fmt("bin/{s}", .{filename}) } });
        run.addPassthruArgs();
        run.step.dependOn(b.getInstallStep());
        b.step("run", "Run Super Mario Bros on the desktop").dependOn(&run.step);
    }
}

fn emLinkStep(b: *Build, emsdk: *Build.Dependency, lib: *Build.Step.Compile, optimize: std.builtin.Optimize) *Build.Step.InstallDir {
    const link = b.addRunFile(emTool(b, emsdk, "em++"));
    link.setName("Emscripten link Super Mario Bros");
    if (optimize == .debug) {
        link.addArgs(&.{ "-g", "-Og", "-sSAFE_HEAP=1", "-sASSERTIONS=1", "-sSTACK_OVERFLOW_CHECK=1", "-gsource-map", "--emrun" });
    } else {
        link.addArgs(&.{ "-sASSERTIONS=0", if (optimize == .small) "-Oz" else "-O3", "-flto", "--closure", "1" });
    }
    link.addArgs(&.{ "-sMALLOC=emmalloc", "-pthread", "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency", "-sFILESYSTEM=1", "-sGL_ENABLE_GET_PROC_ADDRESS=1", "-sINITIAL_MEMORY=128MB", "-sSTACK_SIZE=16MB", "-sFULL_ES3=1", "-sASYNCIFY=1", "-sSUPPORT_LONGJMP=emscripten" });
    link.addPrefixedFileArg("--shell-file=", b.path("web_assets/shell_minimal.html"));
    for (resource_dirs) |dir| {
        link.addArg("--preload-file");
        link.addDirectoryArg2(b.path(b.fmt("res/{s}", .{dir})), .{ .suffix = b.fmt("@/res/{s}", .{dir}) });
    }
    link.addArtifactArg(lib);
    for (lib.getCompileDependencies(false)) |dependency| {
        if (dependency != lib and dependency.kind == .lib) link.addArtifactArg(dependency);
    }
    link.addArg("-o");
    const output = link.addOutputFileArg("index.html");
    return b.addInstallDirectory(.{ .source_dir = output.dirname(), .install_dir = .prefix, .install_subdir = "web" });
}

fn emTool(b: *Build, emsdk: *Build.Dependency, tool: []const u8) Build.LazyPath {
    const filename = if (builtin.os.tag == .windows) b.fmt("{s}.exe", .{tool}) else tool;
    return emsdk.path(b.pathJoin(&.{ "upstream", "emscripten", filename }));
}

fn createEmsdkStep(b: *Build, emsdk: *Build.Dependency) *Build.Step.Run {
    if (builtin.os.tag == .windows) {
        return b.addRunFile(emsdk.path("emsdk.bat"));
    } else {
        const run = b.addSystemCommand(&.{"bash"});
        run.addFileArg(emsdk.path("emsdk"));
        return run;
    }
}

fn emSdkInstallStep(b: *Build, emsdk: *Build.Dependency) *Build.Step {
    const install = createEmsdkStep(b, emsdk);
    install.addArgs(&.{ "install", "7.0.0" });
    const activate = createEmsdkStep(b, emsdk);
    activate.addArgs(&.{ "activate", "7.0.0" });
    activate.step.dependOn(&install.step);
    return &activate.step;
}

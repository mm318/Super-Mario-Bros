const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const sysroot = b.option(std.Build.LazyPath, "emscripten_include", "Emscripten sysroot include directory");
    const sdl = b.dependency("sdl", .{ .target = target, .optimize = .fast, .emscripten_include = sysroot }).artifact("SDL2");
    const mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true });
    mod.addIncludePath(sdl.getEmittedIncludeTree().path(b, "SDL2"));
    mod.linkLibrary(sdl);
    if (sysroot) |include| mod.addSystemIncludePath(include);
    const freetype = b.dependency("freetype", .{ .target = target, .optimize = optimize, .emscripten_include = sysroot }).artifact("freetype");
    mod.addIncludePath(freetype.getEmittedIncludeTree());
    mod.linkLibrary(freetype);
    mod.addCMacro("TTF_USE_HARFBUZZ", "0");
    mod.addCSourceFiles(.{ .files = &.{"SDL_ttf.c"}, .flags = if (sysroot != null) &.{ "-std=c99", "-fno-sanitize=undefined" } else &.{"-std=c99"} });
    const lib = b.addLibrary(.{ .name = "SDL2_ttf", .root_module = mod });
    lib.installHeader(b.path("SDL_ttf.h"), "SDL2/SDL_ttf.h");
    b.installArtifact(lib);
}

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
    mod.addIncludePath(b.path("include"));
    mod.addIncludePath(b.path("src"));
    mod.addIncludePath(b.path("src/codecs"));
    const flags = [_][]const u8{ "-std=c99", "-DMUSIC_WAV" };
    mod.addCSourceFiles(.{ .files = &source_files, .flags = if (sysroot != null) &(flags ++ .{"-fno-sanitize=undefined"}) else &flags });
    const lib = b.addLibrary(.{ .name = "SDL2_mixer", .root_module = mod });
    lib.installHeader(b.path("include/SDL_mixer.h"), "SDL2/SDL_mixer.h");
    b.installArtifact(lib);
}

const source_files = [_][]const u8{
    "src/codecs/load_aiff.c",
    "src/codecs/load_voc.c",
    "src/codecs/music_wav.c",
    "src/codecs/mp3utils.c",
    "src/effect_position.c",
    "src/effect_stereoreverse.c",
    "src/effects_internal.c",
    "src/mixer.c",
    "src/music.c",
    "src/utils.c",
};

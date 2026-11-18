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
    const flags = [_][]const u8{ "-std=c99", "-DUSE_STBIMAGE", "-DLOAD_PNG", "-DSDL_IMAGE_USE_COMMON_BACKEND" };
    mod.addCSourceFiles(.{ .files = &source_files, .flags = if (sysroot != null) &(flags ++ .{"-fno-sanitize=undefined"}) else &flags });
    const lib = b.addLibrary(.{ .name = "SDL2_image", .root_module = mod });
    lib.installHeader(b.path("include/SDL_image.h"), "SDL2/SDL_image.h");
    b.installArtifact(lib);
}

const source_files = [_][]const u8{
    "src/IMG.c",
    "src/IMG_avif.c",
    "src/IMG_bmp.c",
    "src/IMG_gif.c",
    "src/IMG_jpg.c",
    "src/IMG_jxl.c",
    "src/IMG_lbm.c",
    "src/IMG_pcx.c",
    "src/IMG_png.c",
    "src/IMG_pnm.c",
    "src/IMG_qoi.c",
    "src/IMG_stb.c",
    "src/IMG_svg.c",
    "src/IMG_tga.c",
    "src/IMG_tif.c",
    "src/IMG_webp.c",
    "src/IMG_xcf.c",
    "src/IMG_xpm.c",
    "src/IMG_xv.c",
};

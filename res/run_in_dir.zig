//! Runs a command from inside a directory and exits with its status.
//!
//! Emscripten names the preloaded data package after the path given to `-o`,
//! and that name is written into the generated JavaScript. Linking from inside
//! the output directory with `-o index.html` keeps build cache paths such as
//! `.zig-cache/o/<hash>/` out of the published files.
const std = @import("std");

pub fn main(init: std.process.Init) !void {
    const arena = init.arena.allocator();
    const args = try init.minimal.args.toSlice(arena);
    if (args.len < 3) {
        std.debug.print("Usage: run-in-dir <directory> <command> [arguments...]\n", .{});
        return error.InvalidArguments;
    }

    const argv = try arena.alloc([]const u8, args.len - 2);
    for (argv, args[2..]) |*out, arg| out.* = arg;

    var child = try std.process.spawn(init.io, .{ .argv = argv, .cwd = .{ .path = args[1] } });
    defer child.kill(init.io);
    const term = try child.wait(init.io);
    switch (term) {
        .exited => |code| std.process.exit(code),
        else => {
            std.log.err("{s} {f}", .{ argv[0], term });
            std.process.exit(1);
        },
    }
}

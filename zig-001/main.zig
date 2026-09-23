const std = @import("std");

pub fn main() !void {
    const allocator = std.heap.page_allocator;
    const stdin = std.io.getStdIn();
    var br = std.io.bufferedReader(stdin.reader());
    const reader = br.reader();

    const stdout = std.io.getStdOut();
    var bw = std.io.bufferedWriter(stdout.writer());
    const writer = bw.writer();

    while (try reader.readUntilDelimiterOrEofAlloc(allocator, '\n', 5_000_000)) |line| {
        defer allocator.free(line);
        const trimmed = std.mem.trim(u8, line, " \r\n\t");
        if (trimmed.len == 0) continue;
        try processLine(allocator, trimmed, writer);
    }
    try bw.flush();
}

fn processLine(allocator: std.mem.Allocator, line: []const u8, writer: anytype) !void {
    var parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
        return;
    };
    defer parsed.deinit();

    const root = parsed.value;
    const obj = switch (root) {
        .object => |o| o,
        else => return,
    };

    const op_ptr = obj.getPtr("op") orelse return;
    const op = switch (op_ptr.*) {
        .string => |s| s,
        else => return,
    };

    const bytes_ptr = obj.getPtr("bytes") orelse return;
    const arr = switch (bytes_ptr.*) {
        .array => |a| a.items,
        else => return,
    };

    // Validate all bytes in 0..255
    for (arr) |item| {
        switch (item) {
            .integer => |n| {
                if (n < 0 or n > 255) {
                    try writeError(writer, "BYTE_RANGE");
                    return;
                }
            },
            else => {
                try writeError(writer, "BYTE_RANGE");
                return;
            },
        }
    }

    if (std.mem.eql(u8, op, "encode")) {
        try encode(arr, writer);
    } else if (std.mem.eql(u8, op, "decode")) {
        try decode(arr, writer);
    }
}

fn encode(arr: []const std.json.Value, writer: anytype) !void {
    try writer.writeAll("{\"bytes\":[");
    var first = true;
    var i: usize = 0;
    while (i < arr.len) {
        const val: u8 = switch (arr[i]) {
            .integer => |n| @intCast(n),
            else => unreachable,
        };
        var count: u8 = 0;
        while (i < arr.len and count < 255) {
            const current: u8 = switch (arr[i]) {
                .integer => |n| @intCast(n),
                else => unreachable,
            };
            if (current != val) break;
            count += 1;
            i += 1;
        }
        if (!first) try writer.writeAll(",");
        first = false;
        try writer.print("{d},{d}", .{count, val});
    }
    try writer.writeAll("]}\n");
}

fn decode(arr: []const std.json.Value, writer: anytype) !void {
    if (arr.len % 2 != 0) {
        try writeError

const std = @import("std");

const Request = struct {
    paths: []const []const u8,
};

const Result = union(enum) {
    path: []const u8,
    error: []const u8,
};

fn hexVal(c: u8) ?u8 {
    if (c >= '0' and c <= '9') return c - '0';
    if (c >= 'a' and c <= 'f') return c - 'a' + 10;
    if (c >= 'A' and c <= 'F') return c - 'A' + 10;
    return null;
}

fn writeJsonString(writer: anytype, s: []const u8) !void {
    try writer.writeByte('"');
    for (s) |c| {
        switch (c) {
            '"' => try writer.writeAll("\\\""),
            '\\' => try writer.writeAll("\\\\"),
            '\n' => try writer.writeAll("\\n"),
            '\r' => try writer.writeAll("\\r"),
            '\t' => try writer.writeAll("\\t"),
            else => {
                if (c < 32) {
                    try writer.print("\\u{x:0>4}", .{c});
                } else {
                    try writer.writeByte(c);
                }
            },
        }
    }
    try writer.writeByte('"');
}

fn processPath(allocator: std.mem.Allocator, raw: []const u8) !Result {
    var decoded = std.ArrayList(u8).init(allocator);
    defer decoded.deinit();

    var i: usize = 0;
    while (i < raw.len) {
        if (raw[i] == '%') {
            if (i + 2 >= raw.len) return Result{ .error = "ENCODING" };
            const hi = hexVal(raw[i+1]) orelse return Result{ .error = "ENCODING" };
            const lo = hexVal(raw[i+2]) orelse return Result{ .error = "ENCODING" };
            const val = (hi << 4) | lo;
            try decoded.append(val);
            i += 3;
        } else {
            try decoded.append(raw[i]);
            i += 1;
        }
    }

    for (decoded.items) |c| {
        if (c < 32 or c > 126 or c == '\\' or c == '?' or c == '#') {
            return Result{ .error = "CHARACTER" };
        }
    }

    var segments = std.ArrayList([]const u8).init(allocator);
    defer segments.deinit();

    var it = std.mem.splitScalar(u8, decoded.items, '/');
    while (it.next()) |seg| {
        if (seg.len == 0) continue;
        if (std.mem.eql(u8, seg, ".")) continue;
        if (std.mem.eql(u8, seg, "..")) {
            if (segments.items.len == 0) {
                return Result{ .error = "ESCAPE" };
            }
            _ = segments.pop();
        } else {
            try segments.append(seg);
        }
    }

    if (segments.items.len == 0) {
        return Result{ .path = "/" };
    }

    var total_len: usize = 1;
    for (segments.items, 0..) |seg, idx| {
        total_len += seg.len;
        if (idx > 0) total_len += 1;
    }

    var path_buf = try allocator.alloc(u8, total_len);
    var pos: usize = 0;
    path_buf[pos] = '/';
    pos += 1;
    for (segments.items, 0..) |seg, idx| {
        if (idx > 0) {
            path_buf[pos] = '/';
            pos += 1;
        }
        @memcpy(path_buf[pos..pos+seg.len], seg);
        pos += seg.len;
    }

    return Result{ .path = path_buf };
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const stdin = std.io.getStdIn();
    const stdout = std.io.getStdOut();
    var buffered_stdout = std.io.bufferedWriter(stdout.writer());
    const writer = buffered_stdout.writer();
    defer buffered_stdout.flush() catch {};

    const input = try stdin.readToEndAlloc(allocator, std.math.maxInt(usize));
    defer allocator.free(input);

    var lines = std.mem.splitScalar(u8, input, '\n');
    while (lines.next()) |line| {
        const trimmed = std.mem.trim(u8, line, " \t\r\n");
        if (trimmed.len == 0) continue;

        var arena = std.heap.ArenaAllocator.init(allocator);
        defer arena.deinit();
        const line_alloc = arena.allocator();

        const parsed = std.json.parseFromSlice(Request, line_alloc, trimmed, .{}) catch {
            continue;
        };

        const req = parsed.value;

        var out = std.ArrayList(u8).init(line_alloc);
        defer out.deinit();

        try out.appendSlice("{\"results\":[");
        for (req.paths, 0..) |raw, idx| {
            if (idx > 0) try out.appendSlice(",");
            const res = try processPath(line_alloc, raw);
            switch (res) {
                .path => |p| {
                    try out.appendSlice("{\"path\":");
                    try writeJsonString(out.writer(), p);
                    try out.appendSlice("}");
                },
                .error => |e| {
                    try out.appendSlice("{\"error\":");
                    try writeJsonString(out.writer(), e);
                    try out.appendSlice("}");
                },
            }
        }
        try out.appendSlice("]}\n");

        try writer.writeAll(out.items);
    }
}

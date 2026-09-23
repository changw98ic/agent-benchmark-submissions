const std = @import("std");

const ErrorCode = enum { encoding, character, escape };

const Result = union(enum) {
    path: []u8,
    err: ErrorCode,
};

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var stdin_buf = std.io.bufferedReader(std.io.getStdIn().reader());
    var stdin_reader = stdin_buf.reader();

    var stdout_buf = std.io.bufferedWriter(std.io.getStdOut().writer());
    const stdout = stdout_buf.writer();
    defer stdout_buf.flush() catch {};

    while (true) {
        const line_opt = stdin_reader.readUntilDelimiterOrEofAlloc(allocator, '\n', 10 * 1024 * 1024) catch |err| {
            if (err == error.StreamTooLong) {
                try stdout.writeAll("INVALID_JSON\n");
                continue;
            }
            return err;
        };
        if (line_opt == null) break;
        const line = line_opt.?;
        defer allocator.free(line);

        var content = line;
        if (content.len > 0 and content[content.len - 1] == '\r') {
            content = content[0 .. content.len - 1];
        }

        try processLine(allocator, content, stdout);
    }
}

fn processLine(allocator: std.mem.Allocator, line: []const u8, out: anytype) !void {
    var parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
        try out.writeAll("INVALID_JSON\n");
        return;
    };
    defer parsed.deinit();

    const root = parsed.value;
    if (root != .object) {
        try out.writeAll("INVALID_JSON\n");
        return;
    }

    const paths_val = root.object.get("paths") orelse {
        try out.writeAll("INVALID_JSON\n");
        return;
    };
    if (paths_val != .array) {
        try out.writeAll("INVALID_JSON\n");
        return;
    }

    const paths = paths_val.array.items;

    var out_buf = std.ArrayList(u8).init(allocator);
    defer out_buf.deinit();

    try out_buf.appendSlice("{\"results\":[");
    for (paths, 0..) |item, idx| {
        if (idx > 0) try out_buf.append(',');

        if (item != .string) {
            try out_buf.appendSlice("{\"error\":\"CHARACTER\"}");
            continue;
        }

        const res = try normalizePath(allocator, item.string);
        switch (res) {
            .path => |p| {
                defer allocator.free(p);
                try out_buf.appendSlice("{\"path\":\"");
                try writeJsonEscaped(out_buf.writer(), p);
                try out_buf.appendSlice("\"}");
            },
            .err => |e| {
                try out_buf.appendSlice("{\"error\":\"");
                try out_buf.appendSlice(errorName(e));
                try out_buf.appendSlice("\"}");
            },
        }
    }
    try out_buf.appendSlice("]}\n");

    try out.writeAll(out_buf.items);
}

fn normalizePath(allocator: std.mem.Allocator, input: []const u8) !Result {
    var decoded = std.ArrayList(u8).init(allocator);
    defer decoded.deinit();
    try decoded.ensureTotalCapacity(input.len);

    var i: usize = 0;
    while (i < input.len) {
        const b = input[i];
        if (b == '%') {
            if (i + 2 >= input.len) return .{ .err = .encoding };
            const hi = hexVal(input[i + 1]) orelse return .{ .err = .encoding };
            const lo = hexVal(input[i + 2]) orelse return .{ .err = .encoding };
            try decoded.append((hi << 4) | lo);
            i += 3;
        } else {
            try decoded.append(b);
            i += 1;
        }
    }

    for (decoded.items) |c| {
        if (c < 32 or c > 126 or c == '\\' or c == '?' or c == '#') {
            return .{ .err = .character };
        }
    }

    var stack = std.ArrayList([]const u8).init(allocator);
    defer stack.deinit();

    var it = std.mem.splitScalar(u8, decoded.items, '/');
    while (it.next()) |seg| {
        if (seg.len == 0) continue;
        if (std.mem.eql(u8, seg, ".")) continue;
        if (std.mem.eql(u8, seg, "..")) {
            if (stack.items.len == 0) return .{ .err = .escape };
            _ = stack.pop();
        } else {
            try stack.append(seg);
        }
    }

    var result = std.ArrayList(u8).init(allocator);
    errdefer result.deinit();

    if (stack.items.len == 0) {
        try result.append('/');
    } else {
        for (stack.items) |seg| {
            try result.append('/');
            try result.appendSlice(seg);
        }
    }

    return .{ .path = try result.toOwnedSlice() };
}

fn hexVal(c: u8) ?u8 {
    return switch (c) {
        '0'...'9' => c - '0',
        'a'...'f' => c - 'a' + 10,
        'A'...'F' => c - 'A' + 10,
        else => null,
    };
}

fn errorName(e: ErrorCode) []const u8 {
    return switch (e) {
        .encoding => "ENCODING",
        .character => "CHARACTER",
        .escape => "ESCAPE",
    };
}

fn writeJsonEscaped(writer: anytype, s: []const u8) !void {
    for (s) |c| {
        switch (c) {
            '"' => try writer.writeAll("\\\""),
            '\\' => try writer.writeAll("\\\\"),
            '\n' => try writer.writeAll("\\n"),
            '\r' => try writer.writeAll("\\r"),
            '\t' => try writer.writeAll("\\t"),
            0x08 => try writer.writeAll("\\b"),
            0x0c => try writer.writeAll("\\f"),
            else => {
                if (c < 0x20) {
                    try writer.print("\\u{x:0>4}", .{c});
                } else {
                    try writer.writeByte(c);
                }
            },
        }
    }
}

const std = @import("std");

const Allocator = std.mem.Allocator;

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const stdin = std.io.getStdIn();
    const stdout = std.io.getStdOut().writer();

    var pending = std.ArrayList(u8).init(allocator);
    defer pending.deinit();

    var buf: [8192]u8 = undefined;
    while (true) {
        const n = stdin.read(&buf) catch |err| {
            return err;
        };
        if (n == 0) {
            if (pending.items.len > 0) {
                try processLine(allocator, stdout, pending.items);
            }
            break;
        }
        var start: usize = 0;
        for (buf[0..n], 0..) |c, i| {
            if (c == '\n') {
                try pending.appendSlice(buf[start..i]);
                try processLine(allocator, stdout, pending.items);
                pending.clearRetainingCapacity();
                start = i + 1;
            }
        }
        if (start < n) {
            try pending.appendSlice(buf[start..n]);
        }
    }
}

fn processLine(allocator: Allocator, writer: anytype, line: []const u8) !void {
    var parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
        try writer.writeAll("INVALID_JSON\n");
        return;
    };
    defer parsed.deinit();

    const root = parsed.value;
    const root_obj = switch (root) {
        .object => |obj| obj,
        else => {
            try writer.writeAll("INVALID_JSON\n");
            return;
        },
    };

    const paths_val = root_obj.get("paths") orelse {
        try writer.writeAll("INVALID_JSON\n");
        return;
    };
    const paths = switch (paths_val) {
        .array => |arr| arr.items,
        else => {
            try writer.writeAll("INVALID_JSON\n");
            return;
        },
    };

    for (paths) |p| {
        switch (p) {
            .string => {},
            else => {
                try writer.writeAll("INVALID_JSON\n");
                return;
            },
        }
    }

    var out = std.ArrayList(u8).init(allocator);
    defer out.deinit();
    const out_writer = out.writer();

    try out_writer.writeAll("{\"results\":[");
    for (paths, 0..) |p, idx| {
        if (idx > 0) try out_writer.writeByte(',');
        const s = switch (p) {
            .string => |str| str,
            else => unreachable,
        };
        try writePathResult(allocator, out_writer, s);
    }
    try out_writer.writeAll("]}\n");
    try writer.writeAll(out.items);
}

fn writePathResult(allocator: Allocator, writer: anytype, input: []const u8) !void {
    var decoded = std.ArrayList(u8).init(allocator);
    defer decoded.deinit();

    var i: usize = 0;
    while (i < input.len) {
        if (input[i] == '%') {
            if (i + 2 >= input.len) {
                return writeError(writer, "ENCODING");
            }
            const h1 = hexVal(input[i + 1]) orelse return writeError(writer, "ENCODING");
            const h2 = hexVal(input[i + 2]) orelse return writeError(writer, "ENCODING");
            try decoded.append((h1 << 4) | h2);
            i += 3;
        } else {
            try decoded.append(input[i]);
            i += 1;
        }
    }

    for (decoded.items) |b| {
        if (b < 32 or b > 126 or b == '\\' or b == '?' or b == '#') {
            return writeError(writer, "CHARACTER");
        }
    }

    var stack = std.ArrayList([]const u8).init(allocator);
    defer stack.deinit();

    var start: usize = 0;
    var j: usize = 0;
    while (j <= decoded.items.len) : (j += 1) {
        if (j == decoded.items.len or decoded.items[j] == '/') {
            const seg = decoded.items[start..j];
            start = j + 1;
            if (seg.len == 0) continue;
            if (std.mem.eql(u8, seg, ".")) continue;
            if (std.mem.eql(u8, seg, "..")) {
                if (stack.items.len == 0) {
                    return writeError(writer, "ESCAPE");
                }
                _ = stack.pop();
                continue;
            }
            try stack.append(seg);
        }
    }

    try writer.writeAll("{\"path\":\"");
    try writer.writeByte('/');
    for (stack.items, 0..) |seg, idx| {
        if (idx > 0) try writer.writeByte('/');
        try writeJsonEscaped(writer, seg);
    }
    try writer.writeAll("\"}");
}

fn writeError(writer: anytype, name: []const u8) !void {
    try writer.writeAll("{\"error\":\"");
    try writer.writeAll(name);
    try writer.writeAll("\"}");
}

fn hexVal(c: u8) ?u8 {
    return switch (c) {
        '0'...'9' => c - '0',
        'a'...'f' => c - 'a' + 10,
        'A'...'F' => c - 'A' + 10,
        else => null,
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
            0...8, 11, 12, 14...31 => {
                const hex = "0123456789abcdef";
                try writer.writeAll("\\u00");
                try writer.writeByte(hex[c >> 4]);
                try writer.writeByte(hex[c & 0xf]);
            },
            else => try writer.writeByte(c),
        }
    }
}

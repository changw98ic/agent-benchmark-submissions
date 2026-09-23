const std = @import("std");

const NormalizeError = error{ Encoding, Character, Escape };

fn hexVal(c: u8) ?u8 {
    return switch (c) {
        '0'...'9' => c - '0',
        'a'...'f' => c - 'a' + 10,
        'A'...'F' => c - 'A' + 10,
        else => null,
    };
}

fn normalize(allocator: std.mem.Allocator, input: []const u8) ![]u8 {
    var decoded = std.ArrayList(u8).init(allocator);
    defer decoded.deinit();

    var i: usize = 0;
    while (i < input.len) {
        if (input[i] == '%') {
            if (i + 2 >= input.len) return error.Encoding;
            const hi = hexVal(input[i + 1]) orelse return error.Encoding;
            const lo = hexVal(input[i + 2]) orelse return error.Encoding;
            try decoded.append((hi << 4) | lo);
            i += 3;
        } else {
            try decoded.append(input[i]);
            i += 1;
        }
    }

    for (decoded.items) |b| {
        if (b < 32 or b > 126 or b == '\\' or b == '?' or b == '#') {
            return error.Character;
        }
    }

    var stack = std.ArrayList([]const u8).init(allocator);
    defer stack.deinit();

    var it = std.mem.splitScalar(u8, decoded.items, '/');
    while (it.next()) |seg| {
        if (seg.len == 0) continue;
        if (std.mem.eql(u8, seg, ".")) continue;
        if (std.mem.eql(u8, seg, "..")) {
            if (stack.items.len == 0) return error.Escape;
            _ = stack.pop();
        } else {
            try stack.append(seg);
        }
    }

    var out = std.ArrayList(u8).init(allocator);
    errdefer out.deinit();

    try out.append('/');
    for (stack.items, 0..) |seg, idx| {
        if (idx != 0) try out.append('/');
        try out.appendSlice(seg);
    }

    return out.toOwnedSlice();
}

fn processLine(allocator: std.mem.Allocator, out: anytype, line: []const u8) !void {
    var parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
        try out.writeAll("INVALID_JSON\n");
        return;
    };
    defer parsed.deinit();

    const root = parsed.value;
    const paths_val = switch (root) {
        .object => |obj| obj.get("paths") orelse {
            try out.writeAll("INVALID_JSON\n");
            return;
        },
        else => {
            try out.writeAll("INVALID_JSON\n");
            return;
        },
    };

    const paths = switch (paths_val) {
        .array => |arr| arr.items,
        else => {
            try out.writeAll("INVALID_JSON\n");
            return;
        },
    };

    // Validate all items are strings before writing output.
    for (paths) |item| {
        switch (item) {
            .string => {},
            else => {
                try out.writeAll("INVALID_JSON\n");
                return;
            },
        }
    }

    try out.writeAll("{\"results\":[");
    for (paths, 0..) |item, idx| {
        if (idx != 0) try out.writeByte(',');
        const path_str = item.string; // safe due to validation
        const result = normalize(allocator, path_str) catch |err| {
            const code = switch (err) {
                error.Encoding => "ENCODING",
                error.Character => "CHARACTER",
                error.Escape => "ESCAPE",
            };
            try out.print("{{\"error\":\"{s}\"}}", .{code});
            continue;
        };
        defer allocator.free(result);
        try out.writeAll("{\"path\":");
        try std.json.stringify(result, .{}, out);
        try out.writeAll("}");
    }
    try out.writeAll("]}\n");
}

pub fn main() !void {
    const allocator = std.heap.page_allocator;

    var stdout_file = std.io.getStdOut();
    var buffered_out = std.io.bufferedWriter(stdout_file.writer());
    const out = buffered_out.writer();

    var stdin_file = std.io.getStdIn();
    var buffered_in = std.io.bufferedReader(stdin_file.reader());
    const in = buffered_in.reader();

    while (true) {
        const maybe_line = in.readUntilDelimiterOrEofAlloc(allocator, '\n', 16 * 1024 * 1024) catch |err| {
            // On read error, break to avoid infinite loop.
            break;
        };
        if (maybe_line == null) break;
        const line = maybe_line.?;
        defer allocator.free(line);
        processLine(allocator, out, line) catch |err| {
            // If processLine fails with OOM or write error, propagate.
            return err;
        };
    }

    try buffered_out.flush();
}

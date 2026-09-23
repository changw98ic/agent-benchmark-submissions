const std = @import("std");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const stdin = std.io.getStdIn().reader();
    var bw = std.io.bufferedWriter(std.io.getStdOut().writer());
    const w = bw.writer();

    while (true) {
        const line_opt = stdin.readUntilDelimiterOrEofAlloc(allocator, '\n', 100_000_000) catch {
            break;
        };
        const line = line_opt orelse break;
        defer allocator.free(line);

        var trimmed = line;
        if (trimmed.len > 0 and trimmed[trimmed.len - 1] == '\r') {
            trimmed = trimmed[0 .. trimmed.len - 1];
        }

        try processLine(allocator, w, trimmed);
        try bw.flush();
    }
}

fn processLine(allocator: std.mem.Allocator, w: anytype, line: []const u8) !void {
    if (line.len == 0) {
        try w.writeAll("INVALID_JSON\n");
        return;
    }
    var parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
        try w.writeAll("INVALID_JSON\n");
        return;
    };
    defer parsed.deinit();

    const root = parsed.value;
    const obj = switch (root) {
        .object => |o| o,
        else => {
            try w.writeAll("INVALID_JSON\n");
            return;
        },
    };

    const op_val = obj.get("op") orelse {
        try w.writeAll("INVALID_JSON\n");
        return;
    };
    const bytes_val = obj.get("bytes") orelse {
        try w.writeAll("INVALID_JSON\n");
        return;
    };

    const op = switch (op_val) {
        .string => |s| s,
        else => {
            try w.writeAll("INVALID_JSON\n");
            return;
        },
    };
    const arr = switch (bytes_val) {
        .array => |a| a.items,
        else => {
            try w.writeAll("INVALID_JSON\n");
            return;
        },
    };

    for (arr) |item| {
        const v = switch (item) {
            .integer => |i| i,
            else => {
                try w.writeAll("BYTE_RANGE\n");
                return;
            },
        };
        if (v < 0 or v > 255) {
            try w.writeAll("BYTE_RANGE\n");
            return;
        }
    }

    if (std.mem.eql(u8, op, "encode")) {
        try encode(allocator, w, arr);
    } else if (std.mem.eql(u8, op, "decode")) {
        try decode(allocator, w, arr);
    } else {
        try w.writeAll("INVALID_JSON\n");
    }
}

fn encode(allocator: std.mem.Allocator, w: anytype, arr: []const std.json.Value) !void {
    const n = arr.len;
    if (n == 0) {
        try w.writeAll("{\"bytes\":[]}\n");
        return;
    }
    const out = try allocator.alloc(u8, n * 2);
    defer allocator.free(out);
    var out_len: usize = 0;
    var i: usize = 0;
    while (i < n) {
        const val_i = switch (arr[i]) {
            .integer => |v| v,
            else => unreachable,
        };
        const val: u8 = @intCast(val_i);
        var j = i + 1;
        while (j < n) {
            const next = switch (arr[j]) {
                .integer => |v| v,
                else => unreachable,
            };
            if (next != val_i) break;
            j += 1;
        }
        var count: usize = j - i;
        while (count > 0) {
            const chunk: usize = if (count > 255) 255 else count;
            out[out_len] = @intCast(chunk);
            out[out_len + 1] = val;
            out_len += 2;
            count -= chunk;
        }
        i = j;
    }
    try writeBytesJson(w, out[0..out_len]);
}

fn decode(allocator: std.mem.Allocator, w: anytype, arr: []const std.json.Value) !void {
    const n = arr.len;
    if (n % 2 != 0) {
        try w.writeAll("TRUNCATED\n");
        return;
    }
    var sum: usize = 0;
    var i: usize = 0;
    while (i < n) : (i += 2) {
        const count_i = switch (arr[i]) {
            .integer => |v| v,
            else => unreachable,
        };
        if (count_i == 0) {
            try w.writeAll("ZERO_COUNT\n");
            return;
        }
        sum += @intCast(count_i);
    }
    if (sum > 65536) {
        try w.writeAll("LIMIT\n");
        return;
    }
    if (sum == 0) {
        try w.writeAll("{\"bytes\":[]}\n");
        return;
    }
    const out = try allocator.alloc(u8, sum);
    defer allocator.free(out);
    var out_len: usize = 0;
    i = 0;
    while (i < n) : (i += 2) {
        const count_i = switch (arr[i]) {
            .integer => |v| v,
            else => unreachable,
        };
        const count: usize = @intCast(count_i);
        const val_i = switch (arr[i + 1]) {
            .integer => |v| v,
            else => unreachable,
        };
        const val: u8 = @intCast(val_i);
        @memset(out[out_len .. out_len + count], val);
        out_len += count;
    }
    try writeBytesJson(w, out);
}

fn writeBytesJson(w: anytype, bytes: []const u8) !void {
    try w.writeAll("{\"bytes\":[");
    for (bytes, 0..) |b, idx| {
        if (idx != 0) try w.writeByte(',');
        try w.print("{d}", .{b});
    }
    try w.writeAll("]}\n");
}

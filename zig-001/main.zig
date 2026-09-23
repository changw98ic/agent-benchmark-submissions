const std = @import("std");

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var br = std.io.bufferedReader(std.io.getStdIn().reader());
    const reader = br.reader();

    var bw = std.io.bufferedWriter(std.io.getStdOut().writer());
    const writer = bw.writer();

    var line_buf = std.ArrayList(u8).init(allocator);
    defer line_buf.deinit();

    while (true) {
        line_buf.clearRetainingCapacity();
        var got_any = false;
        while (true) {
            const byte = reader.readByte() catch |err| switch (err) {
                error.EndOfStream => break,
                else => return err,
            };
            got_any = true;
            if (byte == '\n') break;
            try line_buf.append(byte);
        }
        if (!got_any and line_buf.items.len == 0) break;
        const line = std.mem.trimRight(u8, line_buf.items, "\r");
        try processLine(allocator, writer, line);
        try bw.flush();
    }
}

fn processLine(allocator: std.mem.Allocator, writer: anytype, line: []const u8) !void {
    const parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
        try writer.writeAll("INVALID_JSON\n");
        return;
    };
    defer parsed.deinit();

    const root = parsed.value;
    if (root != .object) {
        try writer.writeAll("INVALID_JSON\n");
        return;
    }
    const op_val = root.object.get("op") orelse {
        try writer.writeAll("INVALID_JSON\n");
        return;
    };
    const bytes_val = root.object.get("bytes") orelse {
        try writer.writeAll("INVALID_JSON\n");
        return;
    };
    if (op_val != .string or bytes_val != .array) {
        try writer.writeAll("INVALID_JSON\n");
        return;
    }
    const op = op_val.string;
    const arr = bytes_val.array;

    const bytes = try allocator.alloc(u8, arr.items.len);
    defer allocator.free(bytes);
    for (arr.items, 0..) |item, i| {
        if (item != .integer) {
            try writer.writeAll("INVALID_JSON\n");
            return;
        }
        const v = item.integer;
        if (v < 0 or v > 255) {
            try writer.writeAll("{\"error\":\"BYTE_RANGE\"}\n");
            return;
        }
        bytes[i] = @intCast(v);
    }

    if (std.mem.eql(u8, op, "encode")) {
        const encoded = try encode(allocator, bytes);
        defer allocator.free(encoded);
        try writeBytes(writer, encoded);
    } else if (std.mem.eql(u8, op, "decode")) {
        if (bytes.len % 2 != 0) {
            try writer.writeAll("{\"error\":\"TRUNCATED\"}\n");
            return;
        }
        var total: usize = 0;
        var i: usize = 0;
        while (i < bytes.len) : (i += 2) {
            const count = bytes[i];
            if (count == 0) {
                try writer.writeAll("{\"error\":\"ZERO_COUNT\"}\n");
                return;
            }
            total += @as(usize, count);
        }
        if (total > 65536) {
            try writer.writeAll("{\"error\":\"LIMIT\"}\n");
            return;
        }
        const result = try allocator.alloc(u8, total);
        defer allocator.free(result);
        var out: usize = 0;
        i = 0;
        while (i < bytes.len) : (i += 2) {
            const count: usize = bytes[i];
            const value = bytes[i + 1];
            var j: usize = 0;
            while (j < count) : (j += 1) {
                result[out] = value;
                out += 1;
            }
        }
        try writeBytes(writer, result);
    } else {
        try writer.writeAll("INVALID_JSON\n");
    }
}

fn encode(allocator: std.mem.Allocator, input: []const u8) ![]u8 {
    if (input.len == 0) {
        return allocator.alloc(u8, 0);
    }
    var out = std.ArrayList(u8).init(allocator);
    errdefer out.deinit();
    var i: usize = 0;
    while (i < input.len) {
        const value = input[i];
        var run: usize = 1;
        while (i + run < input.len and input[i + run] == value and run < 255) : (run += 1) {}
        try out.append(@intCast(run));
        try out.append(value);
        i += run;
    }
    return out.toOwnedSlice();
}

fn writeBytes(writer: anytype, bytes: []const u8) !void {
    try writer.writeAll("{\"bytes\":[");
    for (bytes, 0..) |b, i| {
        if (i != 0) try writer.writeAll(",");
        try writer.print("{d}", .{b});
    }
    try writer.writeAll("]}\n");
}

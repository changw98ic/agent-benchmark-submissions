const std = @import("std");

const DomainError = error{Truncated, ZeroCount, Limit};

fn encode(allocator: std.mem.Allocator, input: []const u8) ![]u8 {
    var out = std.ArrayList(u8).init(allocator);
    errdefer out.deinit();

    var i: usize = 0;
    while (i < input.len) {
        const value = input[i];
        var run: usize = 1;
        while (i + run < input.len and input[i + run] == value) : (run += 1) {}
        var remaining = run;
        while (remaining > 0) {
            const take = @min(remaining, 255);
            try out.append(@intCast(take));
            try out.append(value);
            remaining -= take;
        }
        i += run;
    }
    return out.toOwnedSlice();
}

fn decode(allocator: std.mem.Allocator, input: []const u8) ![]u8 {
    if (input.len % 2 != 0) return error.Truncated;

    var total: usize = 0;
    var i: usize = 0;
    while (i < input.len) : (i += 2) {
        const count = input[i];
        if (count == 0) return error.ZeroCount;
        total += count;
    }
    if (total > 65536) return error.Limit;

    const out = try allocator.alloc(u8, total);
    errdefer allocator.free(out);

    var out_idx: usize = 0;
    i = 0;
    while (i < input.len) : (i += 2) {
        const count = input[i];
        const value = input[i + 1];
        @memset(out[out_idx .. out_idx + count], value);
        out_idx += count;
    }
    return out;
}

fn writeBytesArray(writer: anytype, bytes: []const u8) !void {
    try writer.writeAll("{\"bytes\":[");
    for (bytes, 0..) |b, idx| {
        if (idx > 0) try writer.writeByte(',');
        try writer.print("{d}", .{b});
    }
    try writer.writeAll("]}");
}

fn writeError(writer: anytype, code: []const u8) !void {
    try writer.print("{{\"error\":\"{s}\"}}", .{code});
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const stdin = std.io.getStdIn();
    const input = try stdin.readToEndAlloc(allocator, 100 * 1024 * 1024);
    defer allocator.free(input);

    var bw = std.io.bufferedWriter(std.io.getStdOut().writer());
    const w = bw.writer();

    var lines = std.mem.splitScalar(u8, input, '\n');
    while (lines.next()) |raw_line| {
        const line = std.mem.trimRight(u8, raw_line, "\r");
        if (line.len == 0) continue;

        var response = std.ArrayList(u8).init(allocator);
        defer response.deinit();
        const rw = response.writer();

        const parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
            try writeError(rw, "INVALID");
            try w.writeAll(response.items);
            try w.writeByte('\n');
            continue;
        };
        defer parsed.deinit();

        const root = parsed.value;
        if (root != .object) {
            try writeError(rw, "INVALID");
            try w.writeAll(response.items);
            try w.writeByte('\n');
            continue;
        }

        const op_val = root.object.get("op") orelse {
            try writeError(rw, "INVALID");
            try w.writeAll(response.items);
            try w.writeByte('\n');
            continue;
        };
        if (op_val != .string) {
            try writeError(rw, "INVALID");
            try w.writeAll(response.items);
            try w.writeByte('\n');
            continue;
        }
        const op = op_val.string;

        const bytes_val = root.object.get("bytes") orelse {
            try writeError(rw, "INVALID");
            try w.writeAll(response.items);
            try w.writeByte('\n');
            continue;
        };
        if (bytes_val != .array) {
            try writeError(rw, "INVALID");
            try w.writeAll(response.items);
            try w.writeByte('\n');
            continue;
        }

        const arr = bytes_val.array;
        const input_bytes = try allocator.alloc(u8, arr.items.len);
        defer allocator.free(input_bytes);

        var valid = true;
        for (arr.items, 0..) |item, idx| {
            if (item != .integer) {
                valid = false;
                break;
            }
            const v = item.integer;
            if (v < 0 or v > 255) {
                valid = false;
                break;
            }
            input_bytes[idx] = @intCast(v);
        }

        if (!valid) {
            try writeError(rw, "BYTE_RANGE");
            try w.writeAll(response.items);
            try w.writeByte('\n');
            continue;
        }

        if (std.mem.eql(u8, op, "encode")) {
            const result = try encode(allocator, input_bytes);
            defer allocator.free(result);
            try writeBytesArray(rw, result);
            try w.writeAll(response.items);
            try w.writeByte('\n');
        } else if (std.mem.eql(u8, op, "decode")) {
            const result = decode(allocator, input_bytes) catch |err| {
                const code = switch (err) {
                    error.Truncated => "TRUNCATED",
                    error.ZeroCount => "ZERO_COUNT",
                    error.Limit => "LIMIT",
                    else => "UNKNOWN",
                };
                try writeError(rw, code);
                try w.writeAll(response.items);
                try w.writeByte('\n');
                continue;
            };
            defer allocator.free(result);
            try writeBytesArray(rw, result);
            try w.writeAll(response.items);
            try w.writeByte('\n');
        } else {
            try writeError(rw, "INVALID");
            try w.writeAll(response.items);
            try w.writeByte('\n');
        }
    }

    try bw.flush();
}

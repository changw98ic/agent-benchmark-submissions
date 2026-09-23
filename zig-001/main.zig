const std = @import("std");

fn writeBytesArray(writer: anytype, bytes: []const u8) !void {
    try writer.writeAll("{\"bytes\":[");
    for (bytes, 0..) |b, i| {
        if (i != 0) try writer.writeByte(',');
        try writer.print("{d}", .{b});
    }
    try writer.writeAll("]}\n");
}

fn process(allocator: std.mem.Allocator, line: []const u8, writer: anytype) !void {
    const parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
        try writer.writeAll("INVALID_JSON\n");
        return;
    };
    defer parsed.deinit();

    const root = parsed.value;
    const obj = switch (root) {
        .object => |o| o,
        else => {
            try writer.writeAll("INVALID_JSON\n");
            return;
        },
    };

    const op_val = obj.get("op") orelse {
        try writer.writeAll("INVALID_JSON\n");
        return;
    };
    const op = switch (op_val) {
        .string => |s| s,
        else => {
            try writer.writeAll("INVALID_JSON\n");
            return;
        },
    };

    const bytes_val = obj.get("bytes") orelse {
        try writer.writeAll("INVALID_JSON\n");
        return;
    };
    const arr = switch (bytes_val) {
        .array => |a| a,
        else => {
            try writer.writeAll("INVALID_JSON\n");
            return;
        },
    };

    var bytes = std.ArrayList(u8).init(allocator);
    defer bytes.deinit();
    try bytes.ensureTotalCapacity(arr.items.len);

    for (arr.items) |item| {
        const v = switch (item) {
            .integer => |i| i,
            else => {
                try writer.writeAll("BYTE_RANGE\n");
                return;
            },
        };
        if (v < 0 or v > 255) {
            try writer.writeAll("BYTE_RANGE\n");
            return;
        }
        bytes.appendAssumeCapacity(@intCast(v));
    }

    if (std.mem.eql(u8, op, "encode")) {
        var out = std.ArrayList(u8).init(allocator);
        defer out.deinit();
        try out.ensureTotalCapacity(bytes.items.len * 2);
        var i: usize = 0;
        while (i < bytes.items.len) {
            const val = bytes.items[i];
            var count: usize = 1;
            while (i + count < bytes.items.len and bytes.items[i + count] == val and count < 255) {
                count += 1;
            }
            try out.append(@intCast(count));
            try out.append(val);
            i += count;
        }
        try writeBytesArray(writer, out.items);
    } else if (std.mem.eql(u8, op, "decode")) {
        if (bytes.items.len % 2 != 0) {
            try writer.writeAll("TRUNCATED\n");
            return;
        }
        var sum: u64 = 0;
        var i: usize = 0;
        while (i < bytes.items.len) : (i += 2) {
            const count = bytes.items[i];
            if (count == 0) {
                try writer.writeAll("ZERO_COUNT\n");
                return;
            }
            sum += count;
        }
        if (sum > 65536) {
            try writer.writeAll("LIMIT\n");
            return;
        }
        var out = std.ArrayList(u8).init(allocator);
        defer out.deinit();
        try out.ensureTotalCapacity(@intCast(sum));
        i = 0;
        while (i < bytes.items.len) : (i += 2) {
            const count = bytes.items[i];
            const val = bytes.items[i + 1];
            const old_len = out.items.len;
            const cnt: usize = count;
            try out.resize(old_len + cnt);
            @memset(out.items[old_len..], val);
        }
        try writeBytesArray(writer, out.items);
    } else {
        try writer.writeAll("INVALID_JSON\n");
    }
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const stdin = std.io.getStdIn().reader();
    const stdout = std.io.getStdOut().writer();

    while (true) {
        const maybe_line = try stdin.readUntilDelimiterOrEofAlloc(allocator, '\n', 10 * 1024 * 1024);
        const line = maybe_line orelse break;
        defer allocator.free(line);

        var trimmed = line;
        if (trimmed.len > 0 and trimmed[trimmed.len - 1] == '\r') {
            trimmed = trimmed[0 .. trimmed.len - 1];
        }

        try process(allocator, trimmed, stdout);
    }
}

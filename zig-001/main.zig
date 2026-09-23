const std = @import("std");

pub fn main() !void {
    const allocator = std.heap.page_allocator;
    const stdin = std.io.getStdIn().reader();
    var br = std.io.bufferedReader(stdin);
    const reader = br.reader();
    const stdout = std.io.getStdOut().writer();
    var bw = std.io.bufferedWriter(stdout);
    const out = bw.writer();

    while (true) {
        const line_opt = reader.readUntilDelimiterOrEofAlloc(allocator, '\n', 10_000_000) catch break;
        if (line_opt == null) break;
        var line = line_opt.?;
        defer allocator.free(line);
        if (line.len > 0 and line[line.len - 1] == '\r') {
            line = line[0 .. line.len - 1];
        }

        const parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
            try out.writeAll("INVALID_JSON\n");
            continue;
        };
        defer parsed.deinit();
        const root = parsed.value;
        if (root != .object) {
            try out.writeAll("INVALID_JSON\n");
            continue;
        }
        const op_val = root.object.get("op") orelse {
            try out.writeAll("INVALID_JSON\n");
            continue;
        };
        if (op_val != .string) {
            try out.writeAll("INVALID_JSON\n");
            continue;
        }
        const op = op_val.string;
        const bytes_val = root.object.get("bytes") orelse {
            try out.writeAll("INVALID_JSON\n");
            continue;
        };
        if (bytes_val != .array) {
            try out.writeAll("INVALID_JSON\n");
            continue;
        }
        const items = bytes_val.array.items;

        var bytes: []u8 = &[_]u8{};
        var allocated = false;
        if (items.len > 0) {
            bytes = try allocator.alloc(u8, items.len);
            allocated = true;
        }
        defer if (allocated) allocator.free(bytes);

        var valid = true;
        for (items, 0..) |item, i| {
            if (item != .integer) {
                try out.writeAll("INVALID_JSON\n");
                valid = false;
                break;
            }
            const n = item.integer;
            if (n < 0 or n > 255) {
                try out.writeAll("BYTE_RANGE\n");
                valid = false;
                break;
            }
            bytes[i] = @intCast(n);
        }
        if (!valid) continue;

        if (std.mem.eql(u8, op, "encode")) {
            var out_buf = std.ArrayList(u8).init(allocator);
            defer out_buf.deinit();
            try encode(bytes, &out_buf);
            try out.writeAll(out_buf.items);
            try out.writeAll("\n");
        } else if (std.mem.eql(u8, op, "decode")) {
            if (bytes.len % 2 != 0) {
                try out.writeAll("TRUNCATED\n");
                continue;
            }
            var zero_count = false;
            var total: usize = 0;
            var i: usize = 0;
            while (i < bytes.len) : (i += 2) {
                const count = bytes[i];
                if (count == 0) {
                    zero_count = true;
                    break;
                }
                total += count;
            }
            if (zero_count) {
                try out.writeAll("ZERO_COUNT\n");
                continue;
            }
            if (total > 65536) {
                try out.writeAll("LIMIT\n");
                continue;
            }
            var out_buf = std.ArrayList(u8).init(allocator);
            defer out_buf.deinit();
            try decode(bytes, &out_buf);
            try out.writeAll(out_buf.items);
            try out.writeAll("\n");
        } else {
            try out.writeAll("INVALID_JSON\n");
            continue;
        }
    }
    try bw.flush();
}

fn encode(bytes: []const u8, out: *std.ArrayList(u8)) !void {
    try out.appendSlice("{\"bytes\":[");
    var first = true;
    var i: usize = 0;
    while (i < bytes.len) {
        const val = bytes[i];
        var run_len: usize = 1;
        while (i + run_len < bytes.len and bytes[i + run_len] == val) : (run_len += 1) {}
        var remaining = run_len;
        while (remaining > 0) {
            const chunk = if (remaining > 255) 255 else remaining;
            if (!first) try out.append(',');
            first = false;
            try out.writer().print("{d},{d}", .{chunk, val});
            remaining -= chunk;
        }
        i += run_len;
    }
    try out.appendSlice("]}");
}

fn decode(bytes: []const u8, out: *std.ArrayList(u8)) !void {
    try out.appendSlice("{\"bytes\":[");
    var first = true;
    var i: usize = 0;
    while (i < bytes.len) : (i += 2) {
        const count = bytes[i];
        const val = bytes[i + 1];
        var c: usize = 0;
        while (c < count) : (c += 1) {
            if (!first) try out.append(',');
            first = false;
            try out.writer().print("{d}", .{val});
        }
    }
    try out.appendSlice("]}");
}

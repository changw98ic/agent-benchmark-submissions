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

fn normalize(path: []const u8, decoded_buf: []u8, result_buf: []u8) NormalizeError![]const u8 {
    var dec_len: usize = 0;
    var i: usize = 0;
    while (i < path.len) {
        const c = path[i];
        if (c == '%') {
            if (i + 2 >= path.len) return error.Encoding;
            const h1 = hexVal(path[i + 1]) orelse return error.Encoding;
            const h2 = hexVal(path[i + 2]) orelse return error.Encoding;
            const byte = (h1 << 4) | h2;
            if (dec_len >= decoded_buf.len) return error.Encoding;
            decoded_buf[dec_len] = byte;
            dec_len += 1;
            i += 3;
        } else {
            if (dec_len >= decoded_buf.len) return error.Encoding;
            decoded_buf[dec_len] = c;
            dec_len += 1;
            i += 1;
        }
    }
    const decoded = decoded_buf[0..dec_len];
    for (decoded) |b| {
        if (b < 32 or b > 126) return error.Character;
        if (b == '\\' or b == '?' or b == '#') return error.Character;
    }

    var result_len: usize = 0;
    var stack: [4096]usize = undefined;
    var stack_len: usize = 0;
    var start: usize = 0;
    while (start <= decoded.len) {
        var end = start;
        while (end < decoded.len and decoded[end] != '/') : (end += 1) {}
        const seg = decoded[start..end];
        if (seg.len != 0 and !(seg.len == 1 and seg[0] == '.')) {
            if (seg.len == 2 and seg[0] == '.' and seg[1] == '.') {
                if (stack_len == 0) return error.Escape;
                stack_len -= 1;
                result_len = stack[stack_len];
            } else {
                if (stack_len >= stack.len) return error.Escape;
                stack[stack_len] = result_len;
                stack_len += 1;
                if (result_len == 0) {
                    result_buf[0] = '/';
                    result_len = 1;
                } else {
                    result_buf[result_len] = '/';
                    result_len += 1;
                }
                @memcpy(result_buf[result_len .. result_len + seg.len], seg);
                result_len += seg.len;
            }
        }
        if (end == decoded.len) break;
        start = end + 1;
    }
    if (result_len == 0) {
        result_buf[0] = '/';
        return result_buf[0..1];
    }
    return result_buf[0..result_len];
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const input = try std.io.getStdIn().readToEndAlloc(allocator, 256 * 1024 * 1024);
    defer allocator.free(input);
    if (input.len == 0) return;

    var bw = std.io.bufferedWriter(std.io.getStdOut().writer());
    const out = bw.writer();
    defer bw.flush() catch {};

    const data = if (input[input.len - 1] == '\n') input[0 .. input.len - 1] else input;
    var it = std.mem.splitScalar(u8, data, '\n');
    while (it.next()) |line| {
        const parsed = std.json.parseFromSlice(std.json.Value, allocator, line, .{}) catch {
            try out.writeAll("INVALID_JSON\n");
            continue;
        };
        defer parsed.deinit();

        switch (parsed.value) {
            .object => |obj| {
                const paths_val = obj.get("paths") orelse {
                    try out.writeAll("INVALID_JSON\n");
                    continue;
                };
                switch (paths_val) {
                    .array => |arr| {
                        try out.writeAll("{\"results\":[");
                        for (arr.items, 0..) |item, idx| {
                            if (idx > 0) try out.writeByte(',');
                            switch (item) {
                                .string => |path| {
                                    var dec_buf: [4096]u8 = undefined;
                                    var res_buf: [4098]u8 = undefined;
                                    const norm = normalize(path, dec_buf[0..], res_buf[0..]) catch |err| {
                                        switch (err) {
                                            error.Encoding => try out.writeAll("{\"error\":\"ENCODING\"}"),
                                            error.Character => try out.writeAll("{\"error\":\"CHARACTER\"}"),
                                            error.Escape => try out.writeAll("{\"error\":\"ESCAPE\"}"),
                                        }
                                        continue;
                                    };
                                    try out.writeAll("{\"path\":\"");
                                    for (norm) |c| {
                                        if (c == '"') {
                                            try out.writeAll("\\\"");
                                        } else {
                                            try out.writeByte(c);
                                        }
                                    }
                                    try out.writeAll("\"}");
                                },
                                else => {
                                    try out.writeAll("{\"error\":\"CHARACTER\"}");
                                },
                            }
                        }
                        try out.writeAll("]}\n");
                    },
                    else => {
                        try out.writeAll("INVALID_JSON\n");
                        continue;
                    },
                }
            },
            else => {
                try out.writeAll("INVALID_JSON\n");
                continue;
            },
        }
    }
}

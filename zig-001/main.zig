const std = @import("std");

fn writeError(w: anytype, name: []const u8) !void {
    try w.print("{{\"error\":\"{s}\"}}\n", .{name});
}

fn writeBytes(w: anytype, data: []const u8) !void {
    try w.writeAll("{\"bytes\":[");
    for (data, 0..) |b, i| {
        if (i > 0) try w.writeAll(",");
        try w.print("{d}", .{b});
    }
    try w.writeAll("]}\n");
}

fn processLine(gpa: std.mem.Allocator, line: []const u8, w: anytype) !void {
    const parsed = std.json.parseFromSlice(std.json.Value, gpa, line, .{}) catch {
        // Parse error - what to return? Not specified...
        // Assume input is always valid JSON structure
        return;
    };
    defer parsed.deinit();
    
    const root = parsed.value;
    const op_str = root.object.get("op") orelse return;
    const bytes_val = root.object.get("bytes") orelse return;
    
    const bytes_arr = bytes_val.array;
    
    // Validate byte range
    var i: usize = 0;
    while (i < bytes_arr.items.len) : (i += 1) {
        const item = bytes_arr.items[i];
        switch (item) {
            .integer => |v| {
                if (v < 0 or v > 255) {
                    try writeError(w, "BYTE_RANGE");
                    return;
                }
            },
            else => {
                // Type guaranteed to be integer per spec
                try writeError(w, "BYTE_RANGE");
                return;
            },
        }
    }
    
    // Convert to bytes
    var input_bytes = try gpa.alloc(u8, bytes_arr.items.len);
    defer gpa.free(input_bytes);
    for (bytes_arr.items, 0..) |item, idx| {
        input_bytes[idx] = @intCast(item.integer);
    }
    
    if (std.mem.eql(u8, op_str.string, "encode")) {
        // RLE encode
        var result = std.ArrayList(u8).init(gpa);
        defer result.deinit();
        
        if (input_bytes.len > 0) {
            var run_start: usize = 0;
            while (run_start < input_bytes.len) {
                const val = input_bytes[run_start];
                var run_end = run_start + 1;
                while (run_end < input_bytes.len and input_bytes[run_end] == val) {
                    run_end += 1;
                }
                var remaining = run_end - run_start;
                while (remaining > 0) {
                    const count: u8 = @intCast(@min(remaining, 255));
                    try result.append(count);
                    try result.append(val);
                    remaining -= count;
                }
                run_start = run_end;
            }
        }
        try writeBytes(w, result.items);
    } else {
        // decode
        if (input_bytes.len % 2 != 0) {
            try writeError(w, "TRUNCATED");
            return;
        }
        
        // Scan for zero counts and compute total length
        var total: usize = 0;
        var idx: usize = 0;
        while (idx < input_bytes.len) : (idx += 2) {
            const count = input_bytes[idx];
            if (count == 0) {
                try writeError(w, "ZERO_COUNT");
                return;
            }
            total += count;
        }
        
        if (total > 65536) {
            try writeError(w, "LIMIT");
            return;
        }
        
        // Decode
        var result = try gpa.alloc(u8, total);
        defer gpa.free(result);
        
        var out_idx: usize = 0;
        idx = 0;
        while (idx < input_bytes.len) : (idx += 2) {
            const count = input_bytes[idx];
            const val = input_bytes[idx + 1];
            var j: usize = 0;
            while (j < count) : (j += 1) {
                result[out_idx] = val;
                out_idx += 1;
            }
        }
        
        try writeBytes(w, result);
    }
}

pub fn main() !void {
    var gpa_state = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa_state.deinit();
    const gpa = gpa_state.allocator();
    
    const stdout = std.fs.File.stdout().writer();
    const stdin = std.fs.File.stdin().reader();
    
    var out_buf = std.io.bufferedWriter(stdout);
    const w = out_buf.writer();
    
    var in_buf = std.io.bufferedReader(stdin);
    const r = in_buf.reader();
    
    var line_buf = std.ArrayList(u8).init(gpa);
    defer line_buf.deinit();
    
    while (true) {
        line_buf.clearRetainingCapacity();
        // Read line
        while (true) {
            const byte = r.readByte() catch |err| switch (err) {
                error.EndOfStream => break,
                else => return err,
            };
            if (byte == '\n') break;
            try line_buf.append(byte);
        }
        
        if (line_buf.items.len == 0) {
            // Check if EOF
            // Hmm, how to distinguish empty line from EOF?
            // Actually if we broke due to EOF and line_buf is empty, we're done
        }
        
        // Process line
        if (line_buf.items.len > 0) {
            try processLine(gpa, line_buf.items, w);
            try out_buf.flush();
        }
    }
    
    try out_buf.flush();
}

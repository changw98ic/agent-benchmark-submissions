pub fn main() !void {
       var gpa = std.heap.GeneralPurposeAllocator(.{}){};
       defer _ = gpa.deinit();
       const allocator = gpa.allocator();

       const input = try std.io.getStdIn().readToEndAlloc(allocator, 1024 * 1024 * 1024);
       defer allocator.free(input);

       var out = std.ArrayList(u8).init(allocator);
       defer out.deinit();

       var it = std.mem.splitScalar(u8, input, '\n');
       while (it.next()) |line_raw| {
           const line = std.mem.trim(u8, line_raw, " \t\r\n");
           if (line.len == 0) continue;
           try out.clearRetainingCapacity();
           try processLine(allocator, line, &out);
           try std.io.getStdOut().writeAll(out.items);
       }
   }

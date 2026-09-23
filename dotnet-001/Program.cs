using System;
using System.Collections.Generic;
using System.Text.Json;

class Program
{
    static void Main()
    {
        string? line;
        while ((line = Console.ReadLine()) != null)
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            Console.WriteLine(Process(line));
        }
    }

    static string Process(string line)
    {
        try
        {
            using var doc = JsonDocument.Parse(line);
            var root = doc.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
                return Error("ITEM");

            if (!root.TryGetProperty("items", out var itemsProp) || itemsProp.ValueKind != JsonValueKind.Array)
                return Error("ITEM");

            int itemCount = itemsProp.GetArrayLength();
            if (itemCount > 50)
                return Error("ITEM");

            long subtotal = 0;
            var subtotals = new List<long>(itemCount);
            foreach (var item in itemsProp.EnumerateArray())
            {
                if (item.ValueKind != JsonValueKind.Object)
                    return Error("ITEM");
                if (!item.TryGetProperty("unit_cents", out var unitProp) || !unitProp.TryGetInt64(out long unit))
                    return Error("ITEM");
                if (!item.TryGetProperty("quantity", out var qtyProp) || !qtyProp.TryGetInt64(out long qty))
                    return Error("ITEM");
                if (unit < 0 || unit > 10_000_000) return Error("ITEM");
                if (qty < 1 || qty > 999) return Error("ITEM");
                long lineTotal = unit * qty;
                subtotals.Add(lineTotal);
                subtotal += lineTotal;
            }

            bool hasCoupon = false;
            string kind = "";
            long threshold = 0;
            long value = 0;
            if (root.TryGetProperty("coupon", out var couponProp) && couponProp.ValueKind != JsonValueKind.Null)
            {
                hasCoupon = true;
                if (couponProp.ValueKind != JsonValueKind.Object) return Error("COUPON");

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
            ProcessLine(line);
        }
    }

    static void ProcessLine(string line)
    {
        JsonDocument doc;
        try
        {
            doc = JsonDocument.Parse(line);
        }
        catch (JsonException)
        {
            Console.WriteLine("INVALID_JSON");
            return;
        }
        using (doc)
        {
            JsonElement root = doc.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
            {
                Console.WriteLine("INVALID_JSON");
                return;
            }

            if (!root.TryGetProperty("items", out JsonElement itemsEl) || itemsEl.ValueKind != JsonValueKind.Array)
            {
                Console.WriteLine("ITEM");
                return;
            }

            if (itemsEl.GetArrayLength() > 50)
            {
                Console.WriteLine("ITEM");
                return;
            }

            var subtotals = new List<long>();
            long subtotal = 0;
            foreach (JsonElement item in itemsEl.EnumerateArray())
            {
                if (item.ValueKind != JsonValueKind.Object)
                {
                    Console.WriteLine("ITEM");
                    return;
                }

                if (!item.TryGetProperty("unit_cents", out JsonElement unitEl) ||
                    unitEl.ValueKind != JsonValueKind.Number ||
                    !unitEl.TryGetInt64(out long unit) ||
                    unit < 0 || unit > 10000000)
                {
                    Console.WriteLine("ITEM");
                    return;
                }

                if (!item.TryGetProperty("quantity", out JsonElement qtyEl) ||
                    qtyEl.ValueKind != JsonValueKind.Number ||
                    !qtyEl.TryGetInt64(out long qty) ||
                    qty < 1 || qty > 999)
                {
                    Console.WriteLine("ITEM");
                    return;
                }

                long lineSubtotal = unit * qty;
                subtotals.Add(lineSubtotal);
                subtotal += lineSubtotal;
            }

            bool hasCoupon = false;
            string? kind = null;
            long threshold = 0;
            long value = 0;

            if (root.TryGetProperty("coupon", out JsonElement couponEl) && couponEl.ValueKind != JsonValueKind.Null)
            {
                if (couponEl.ValueKind != JsonValueKind.Object)
                {
                    Console.WriteLine("COUPON");
                    return;
                }

                if (!couponEl.TryGetProperty("kind", out JsonElement kindEl) || kindEl.ValueKind != JsonValueKind.String)
                {
                    Console.WriteLine("COUPON");
                    return;
                }
                kind = kindEl.GetString();
                if (kind != "fixed" && kind != "percent")
                {
                    Console.WriteLine("COUPON");
                    return;
                }

                if (!couponEl.TryGetProperty("threshold", out JsonElement thresholdEl) ||
                    thresholdEl.ValueKind != JsonValueKind.Number ||
                    !thresholdEl.TryGetInt64(out threshold) ||
                    threshold < 0)
                {
                    Console.WriteLine("COUPON");
                    return;
                }

                if (!couponEl.TryGetProperty("value", out JsonElement valueEl) ||
                    valueEl.ValueKind != JsonValueKind.Number ||
                    !valueEl.TryGetInt64(out value))
                {
                    Console.WriteLine("COUPON");
                    return;
                }

                if (kind == "fixed")
                {
                    if (value < 0)
                    {
                        Console.WriteLine("COUPON");
                        return;
                    }
                }
                else
                {
                    if (value < 1 || value > 100)
                    {
                        Console.WriteLine("COUPON");
                        return;
                    }
                }

                hasCoupon = true;
            }

            long discount = 0;
            if (hasCoupon && subtotals.Count > 0 && subtotal >= threshold)
            {
                if (kind == "fixed")
                {
                    discount = value > subtotal ? subtotal : value;
                }
                else
                {
                    discount = (subtotal * value + 50) / 100;
                    if (discount > subtotal) discount = subtotal;
                }
            }

            long total = subtotal - discount;
            if (total < 0) total = 0;

            var response = new Response
            {
                subtotals = subtotals.ToArray(),
                subtotal = subtotal,
                discount = discount,
                total = total
            };
            Console.WriteLine(JsonSerializer.Serialize(response));
        }
    }

    private sealed class Response
    {
        public long[] subtotals { get; set; } = Array.Empty<long>();
        public long subtotal { get; set; }
        public long discount { get; set; }
        public long total { get; set; }
    }
}

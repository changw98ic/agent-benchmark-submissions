using System;
using System.Collections.Generic;
using System.Text.Json;

while (true)
{
    string? line = Console.ReadLine();
    if (line == null) break;
    if (string.IsNullOrWhiteSpace(line))
    {
        Console.WriteLine("INVALID_JSON");
        continue;
    }
    try
    {
        using JsonDocument doc = JsonDocument.Parse(line);
        JsonElement root = doc.RootElement;
        if (root.ValueKind != JsonValueKind.Object)
        {
            Console.WriteLine("ITEM");
            continue;
        }
        if (!root.TryGetProperty("items", out JsonElement itemsEl) || itemsEl.ValueKind != JsonValueKind.Array)
        {
            Console.WriteLine("ITEM");
            continue;
        }
        int count = itemsEl.GetArrayLength();
        if (count > 50)
        {
            Console.WriteLine("ITEM");
            continue;
        }
        var subtotals = new List<long>(count);
        long subtotal = 0;
        bool itemError = false;
        foreach (JsonElement itemEl in itemsEl.EnumerateArray())
        {
            if (itemEl.ValueKind != JsonValueKind.Object)
            {
                itemError = true;
                break;
            }
            if (!itemEl.TryGetProperty("unit_cents", out JsonElement unitEl) || unitEl.ValueKind != JsonValueKind.Number || !unitEl.TryGetInt64(out long unit))
            {
                itemError = true;
                break;
            }
            if (unit < 0 || unit > 10000000)
            {
                itemError = true;
                break;
            }
            if (!itemEl.TryGetProperty("quantity", out JsonElement qtyEl) || qtyEl.ValueKind != JsonValueKind.Number || !qtyEl.TryGetInt64(out long qty))
            {
                itemError = true;
                break;
            }
            if (qty < 1 || qty > 999)
            {
                itemError = true;
                break;
            }
            long lineTotal = unit * qty;
            subtotals.Add(lineTotal);
            subtotal += lineTotal;
        }
        if (itemError)
        {
            Console.WriteLine("ITEM");
            continue;
        }

        bool hasCoupon = root.TryGetProperty("coupon", out JsonElement couponEl) && couponEl.ValueKind != JsonValueKind.Null;
        string kind = "";
        long threshold = 0;
        long value = 0;
        if (hasCoupon)
        {
            if (couponEl.ValueKind != JsonValueKind.Object)
            {
                Console.WriteLine("COUPON");
                continue;
            }
            if (!couponEl.TryGetProperty("kind", out JsonElement kindEl) || kindEl.ValueKind != JsonValueKind.String)
            {
                Console.WriteLine("COUPON");
                continue;
            }
            kind = kindEl.GetString() ?? "";
            if (kind != "fixed" && kind != "percent")
            {
                Console.WriteLine("COUPON");
                continue;
            }
            if (!couponEl.TryGetProperty("threshold", out JsonElement thresholdEl) || thresholdEl.ValueKind != JsonValueKind.Number || !thresholdEl.TryGetInt64(out threshold))
            {
                Console.WriteLine("COUPON");
                continue;
            }
            if (threshold < 0)
            {
                Console.WriteLine("COUPON");
                continue;
            }
            if (!couponEl.TryGetProperty("value", out JsonElement valueEl) || valueEl.ValueKind != JsonValueKind.Number || !valueEl.TryGetInt64(out value))
            {
                Console.WriteLine("COUPON");
                continue;
            }
            if (kind == "fixed")
            {
                if (value < 0)
                {
                    Console.WriteLine("COUPON");
                    continue;
                }
            }
            else
            {
                if (value < 1 || value > 100)
                {
                    Console.WriteLine("COUPON");
                    continue;
                }
            }
        }

        long discount = 0;
        if (hasCoupon && subtotals.Count > 0 && subtotal >= threshold)
        {
            if (kind == "fixed")
            {
                discount = Math.Min(value, subtotal);
            }
            else
            {
                discount = (subtotal * value + 50) / 100;
                if (discount > subtotal) discount = subtotal;
            }
        }
        long total = subtotal - discount;

        var response = new
        {
            subtotals = subtotals,
            subtotal = subtotal,
            discount = discount,
            total = total
        };
        Console.WriteLine(JsonSerializer.Serialize(response));
    }
    catch (JsonException)
    {
        Console.WriteLine("INVALID_JSON");
    }
    catch (Exception)
    {
        Console.WriteLine("INVALID_JSON");
    }
}

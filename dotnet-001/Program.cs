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
        catch (ArgumentException)
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

            if (!root.TryGetProperty("items", out JsonElement itemsElem) || itemsElem.ValueKind != JsonValueKind.Array)
            {
                Console.WriteLine("INVALID_JSON");
                return;
            }

            if (itemsElem.GetArrayLength() > 50)
            {
                Console.WriteLine("ITEM");
                return;
            }

            var subtotals = new List<long>();
            long subtotal = 0;
            foreach (JsonElement itemElem in itemsElem.EnumerateArray())
            {
                if (itemElem.ValueKind != JsonValueKind.Object)
                {
                    Console.WriteLine("ITEM");
                    return;
                }
                if (!itemElem.TryGetProperty("unit_cents", out JsonElement unitElem) || !unitElem.TryGetInt64(out long unit))
                {
                    Console.WriteLine("ITEM");
                    return;
                }
                if (!itemElem.TryGetProperty("quantity", out JsonElement qtyElem) || !qtyElem.TryGetInt64(out long qty))
                {
                    Console.WriteLine("ITEM");
                    return;
                }
                if (unit < 0 || unit > 10000000 || qty < 1 || qty > 999)
                {
                    Console.WriteLine("ITEM");
                    return;
                }
                long lineSubtotal = unit * qty;
                subtotals.Add(lineSubtotal);
                subtotal += lineSubtotal;
            }

            bool hasCoupon = false;
            string kind = "";
            long threshold = 0;
            long value = 0;
            if (root.TryGetProperty("coupon", out JsonElement couponElem) && couponElem.ValueKind != JsonValueKind.Null)
            {
                if (couponElem.ValueKind != JsonValueKind.Object)
                {
                    Console.WriteLine("COUPON");
                    return;
                }
                if (!couponElem.TryGetProperty("kind", out JsonElement kindElem) || kindElem.ValueKind != JsonValueKind.String)
                {
                    Console.WriteLine("COUPON");
                    return;
                }
                kind = kindElem.GetString() ?? "";
                if (kind != "fixed" && kind != "percent")
                {
                    Console.WriteLine("COUPON");
                    return;
                }
                if (!couponElem.TryGetProperty("threshold", out JsonElement thElem) || !thElem.TryGetInt64(out threshold) || threshold < 0)
                {
                    Console.WriteLine("COUPON");
                    return;
                }
                if (!couponElem.TryGetProperty("value", out JsonElement valElem) || !valElem.TryGetInt64(out value))
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
            if (hasCoupon && itemsElem.GetArrayLength() > 0 && subtotal >= threshold)
            {
                if (kind == "fixed")
                {
                    discount = Math.Min(value, subtotal);
                }
                else
                {
                    long product = subtotal * value;
                    discount = (product + 50) / 100;
                    if (discount > subtotal) discount = subtotal;
                }
            }
            long total = subtotal - discount;

            var result = new
            {
                subtotals = subtotals,
                subtotal = subtotal,
                discount = discount,
                total = total
            };
            Console.WriteLine(JsonSerializer.Serialize(result));
        }
    }
}

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

class Program
{
    static void Main()
    {
        using var reader = new StreamReader(Console.OpenStandardInput());
        string? line;
        while ((line = reader.ReadLine()) != null)
        {
            if (string.IsNullOrWhiteSpace(line))
            {
                continue;
            }
            Console.WriteLine(ProcessLine(line));
        }
    }

    static string ProcessLine(string line)
    {
        try
        {
            using JsonDocument doc = JsonDocument.Parse(line);
            JsonElement root = doc.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
            {
                return Error("ITEM");
            }

            if (!root.TryGetProperty("items", out JsonElement itemsElem) || itemsElem.ValueKind != JsonValueKind.Array)
            {
                return Error("ITEM");
            }

            var subtotals = new List<long>();
            long subtotal = 0;

            foreach (JsonElement item in itemsElem.EnumerateArray())
            {
                if (item.ValueKind != JsonValueKind.Object)
                {
                    return Error("ITEM");
                }

                if (!item.TryGetProperty("unit_cents", out JsonElement unitElem) || !unitElem.TryGetInt64(out long unit))
                {
                    return Error("ITEM");
                }

                if (unit < 0 || unit > 10_000_000)
                {
                    return Error("ITEM");
                }

                if (!item.TryGetProperty("quantity", out JsonElement qtyElem) || !qtyElem.TryGetInt64(out long qty))
                {
                    return Error("ITEM");
                }

                if (qty < 1 || qty > 999)
                {
                    return Error("ITEM");
                }

                long lineSubtotal = unit * qty;
                subtotals.Add(lineSubtotal);
                subtotal += lineSubtotal;
            }

            bool hasCoupon = root.TryGetProperty("coupon", out JsonElement couponElem) && couponElem.ValueKind != JsonValueKind.Null;
            string? kind = null;
            long threshold = 0;
            long value = 0;

            if (hasCoupon)
            {
                if (couponElem.ValueKind != JsonValueKind.Object)
                {
                    return Error("COUPON");
                }

                if (!couponElem.TryGetProperty("kind", out JsonElement kindElem) || kindElem.ValueKind != JsonValueKind.String)
                {
                    return Error("COUPON");
                }

                kind = kindElem.GetString();
                if (kind != "fixed" && kind != "percent")
                {
                    return Error("COUPON");
                }

                if (!couponElem.TryGetProperty("threshold", out JsonElement thresholdElem) || !thresholdElem.TryGetInt64(out threshold))
                {
                    return Error("COUPON");
                }

                if (threshold < 0)
                {
                    return Error("COUPON");
                }

                if (!couponElem.TryGetProperty("value", out JsonElement valueElem) || !valueElem.TryGetInt64(out value))
                {
                    return Error("COUPON");
                }

                if (kind == "fixed")
                {
                    if (value < 0)
                    {
                        return Error("COUPON");
                    }
                }
                else
                {
                    if (value < 1 || value > 100)
                    {
                        return Error("COUPON");
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
                }
            }

            long total = subtotal - discount;

            var output = new
            {
                subtotals = subtotals.ToArray(),
                subtotal = subtotal,
                discount = discount,
                total = total
            };

            return JsonSerializer.Serialize(output);
        }
        catch (JsonException)
        {
            return Error("ITEM");
        }
        catch (Exception)
        {
            return Error("ITEM");
        }
    }

    static string Error(string code)
    {
        return $"{{\"error\":\"{code}\"}}";
    }
}

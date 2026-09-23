using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;

class Program
{
    static void Main()
    {
        string line;
        while ((line = Console.ReadLine()) != null)
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            Console.WriteLine(ProcessLine(line));
        }
    }

    static string ProcessLine(string line)
    {
        try
        {
            using var doc = JsonDocument.Parse(line);
            var root = doc.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
                return Error("ITEM");

            if (!root.TryGetProperty("items", out var itemsProp) || itemsProp.ValueKind != JsonValueKind.Array)
                return Error("ITEM");

            var subtotals = new List<long>();
            long subtotal = 0;
            int count = 0;
            foreach (var item in itemsProp.EnumerateArray())
            {
                count++;
                if (count > 50) return Error("ITEM");
                if (item.ValueKind != JsonValueKind.Object) return Error("ITEM");
                if (!item.TryGetProperty("unit_cents", out var unitProp) || unitProp.ValueKind != JsonValueKind.Number)
                    return Error("ITEM");
                if (!unitProp.TryGetInt32(out int unit)) return Error("ITEM");
                if (unit < 0 || unit > 10000000) return Error("ITEM");

                if (!item.TryGetProperty("quantity", out var qtyProp) || qtyProp.ValueKind != JsonValueKind.Number)
                    return Error("ITEM");
                if (!qtyProp.TryGetInt32(out int qty)) return Error("ITEM");
                if (qty < 1 || qty > 999) return Error("ITEM");

                long lineSub = (long)unit * qty;
                subtotals.Add(lineSub);
                subtotal += lineSub;
            }

            bool hasCoupon = false;
            string kind = null;
            long threshold = 0;
            long fixedValue = 0;
            int percentValue = 0;

            if (root.TryGetProperty("coupon", out var couponProp) && couponProp.ValueKind != JsonValueKind.Null)
            {
                if (couponProp.ValueKind != JsonValueKind.Object)
                    return Error("COUPON");

                if (!couponProp.TryGetProperty("kind", out var kindProp) || kindProp.ValueKind != JsonValueKind.String)
                    return Error("COUPON");
                kind = kindProp.GetString();
                if (kind != "fixed" && kind != "percent")
                    return Error("COUPON");

                if (!couponProp.TryGetProperty("threshold", out var thProp) || thProp.ValueKind != JsonValueKind.Number)
                    return Error("COUPON");
                if (!thProp.TryGetInt64(out threshold) || threshold < 0)
                    return Error("COUPON");

                if (!couponProp.TryGetProperty("value", out var valProp) || valProp.ValueKind != JsonValueKind.Number)
                    return Error("COUPON");

                if (kind == "fixed")
                {
                    if (!valProp.TryGetInt64(out fixedValue) || fixedValue < 0)
                        return Error("COUPON");
                }
                else
                {
                    if (!valProp.TryGetInt32(out percentValue) || percentValue < 1 || percentValue > 100)
                        return Error("COUPON");
                }
                hasCoupon = true;
            }

            long discount = 0;
            if (hasCoupon && count > 0 && subtotal >= threshold)
            {
                if (kind == "fixed")
                    discount = Math.Min(fixedValue, subtotal);
                else
                    discount = (subtotal * percentValue + 50) / 100;
            }

            long total = subtotal - discount;

            using var stream = new MemoryStream();
            using (var writer = new Utf8JsonWriter(stream))
            {
                writer.WriteStartObject();
                writer.WritePropertyName("subtotals");
                writer.WriteStartArray();
                foreach (var s in subtotals)
                    writer.WriteNumberValue(s);
                writer.WriteEndArray();
                writer.WriteNumber("subtotal", subtotal);
                writer.WriteNumber("discount", discount);
                writer.WriteNumber("total", total);
                writer.WriteEndObject();
            }
            return Encoding.UTF8.GetString(stream.ToArray());
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
        using var stream = new MemoryStream();
        using (var writer = new Utf8JsonWriter(stream))
        {
            writer.WriteStartObject();
            writer.WriteString("error", code);
            writer.WriteEndObject();
        }
        return Encoding.UTF8.GetString(stream.ToArray());
    }
}

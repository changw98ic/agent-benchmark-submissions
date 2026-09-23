using System;
using System.Collections.Generic;
using System.Text.Json;

internal static class Program
{
    private static void Main()
    {
        string? line;
        while ((line = Console.ReadLine()) != null)
        {
            ProcessLine(line);
        }
    }

    private static void Write(string value)
    {
        Console.WriteLine(value);
        Console.Out.Flush();
    }

    private static void ProcessLine(string line)
    {
        JsonDocument doc;
        try
        {
            doc = JsonDocument.Parse(line);
        }
        catch (JsonException)
        {
            Write("INVALID_JSON");
            return;
        }
        catch (ArgumentException)
        {
            Write("INVALID_JSON");
            return;
        }

        using (doc)
        {
            JsonElement root = doc.RootElement;
            if (root.ValueKind != JsonValueKind.Object)
            {
                Write("INVALID_JSON");
                return;
            }

            if (!root.TryGetProperty("items", out JsonElement itemsElement) ||
                itemsElement.ValueKind != JsonValueKind.Array)
            {
                Write("{\"error\":\"ITEM\"}");
                return;
            }

            if (itemsElement.GetArrayLength() > 50)
            {
                Write("{\"error\":\"ITEM\"}");
                return;
            }

            var subtotals = new List<long>();
            long subtotal = 0;

            foreach (JsonElement item in itemsElement.EnumerateArray())
            {
                if (item.ValueKind != JsonValueKind.Object ||
                    !item.TryGetProperty("unit_cents", out JsonElement unitElement) ||
                    !unitElement.TryGetInt64(out long unitCents) ||
                    !item.TryGetProperty("quantity", out JsonElement quantityElement) ||
                    !quantityElement.TryGetInt64(out long quantity))
                {
                    Write("{\"error\":\"ITEM\"}");
                    return;
                }

                if (unitCents < 0 || unitCents > 10000000 || quantity < 1 || quantity > 999)
                {
                    Write("{\"error\":\"ITEM\"}");
                    return;
                }

                long lineSubtotal = unitCents * quantity;
                subtotals.Add(lineSubtotal);
                subtotal += lineSubtotal;
            }

            bool orderNonEmpty = subtotals.Count > 0;
            long discount = 0;
            if (root.TryGetProperty("coupon", out JsonElement couponElement) &&
                couponElement.ValueKind != JsonValueKind.Null)
            {
                if (couponElement.ValueKind != JsonValueKind.Object ||
                    !couponElement.TryGetProperty("kind", out JsonElement kindElement) ||
                    kindElement.ValueKind != JsonValueKind.String ||
                    !couponElement.TryGetProperty("threshold", out Json

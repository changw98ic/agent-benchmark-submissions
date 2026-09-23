using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;

class Program
{
    static void Main()
    {
        using var reader = new StreamReader(Console.OpenStandardInput());
        using var writer = new StreamWriter(Console.OpenStandardOutput());
        writer.AutoFlush = true;
        string? line;
        while ((line = reader.ReadLine()) != null)
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            writer.WriteLine(ProcessLine(line));
        }
    }

    static string ProcessLine(string line)
    {
        using var doc = JsonDocument.Parse(line);
        var root = doc.RootElement;

        var stock = new Dictionary<string, int>(StringComparer.Ordinal);
        var stockOrder = new List<string>();
        bool stockNegative = false;

        if (root.TryGetProperty("stock", out var stockEl) && stockEl.ValueKind == JsonValueKind.Object)
        {
            foreach (var prop in stockEl.EnumerateObject())
            {
                if (prop.Value.TryGetInt32(out int val))
                {
                    stock[prop.Name] = val;
                    stockOrder.Add(prop.Name);
                    if (val < 0) stockNegative = true;
                }
            }
        }

        var results = new List<string>();
        var available = stockNegative
            ? new Dictionary<string, int>(StringComparer.Ordinal)
            : new Dictionary<string, int>(stock, StringComparer.Ordinal);
        var holds = new Dictionary<string, Dictionary<string, int>>(StringComparer.Ordinal);
        var usedIds = new HashSet<string>(StringComparer.Ordinal);
        var holdOrder = new List<string>();

        if (stockNegative)
        {
            results.Add("STOCK");
            return SerializeResponse(results, stockOrder, available, holds, holdOrder);
        }

        if (root.TryGetProperty("ops", out var opsEl) && opsEl.ValueKind == JsonValueKind.Array)
        {
            foreach (var opEl in opsEl.EnumerateArray())
            {
                if (opEl.ValueKind != JsonValueKind.Object)
                {
                    results.Add("INVALID");
                    continue;
                }

                if (!opEl.TryGetProperty("op", out var opProp) || opProp.ValueKind != JsonValueKind.String)
                {
                    results.Add("INVALID");
                    continue;
                }

                string op = opProp.GetString() ?? "";

                if (op == "reserve")
                {
                    if (!opEl.TryGetProperty("id", out var idProp) || idProp.ValueKind != JsonValueKind.String)
                    {
                        results.Add("INVALID");
                        continue;
                    }
                    string id = idProp.GetString() ?? "";

                    if (usedIds.Contains(id))
                    {
                        results.Add("DUPLICATE_ID");
                        continue;
                    }

                    if (!opEl.TryGetProperty("items", out var itemsEl) || itemsEl.ValueKind != JsonValueKind.Object)
                    {
                        results.Add("INVALID");
                        continue;
                    }

                    var items = new Dictionary<string, int>(StringComparer.Ordinal);
                    bool invalid = false;
                    foreach (var item in itemsEl.EnumerateObject())
                    {
                        if (!item.Value.TryGetInt32(out int qty) || qty <= 0)
                        {
                            invalid = true;
                            break;
                        }
                        if (!stock.ContainsKey(item.Name))
                        {
                            invalid = true;
                            break;
                        }
                        items[item.Name] = qty;
                    }

                    if (invalid || items.Count == 0)
                    {
                        results.Add("INVALID");
                        continue;
                    }

                    bool insufficient = false;
                    foreach (var kvp in items)
                    {
                        if (available[kvp.Key] < kvp.Value)
                        {
                            insufficient = true;
                            break;
                        }
                    }

                    if (insufficient)
                    {
                        results.Add("INSUFFICIENT");
                        continue;
                    }

                    foreach (var kvp in items)
                    {
                        available[kvp.Key] -= kvp.Value;
                    }

                    holds[id] = new Dictionary<string, int>(items, StringComparer.Ordinal);
                    holdOrder.Add(id);
                    usedIds.Add(id);
                    results.Add("OK");
                }
                else if (op == "release")
                {
                    if (!opEl.TryGetProperty("id", out var idProp) || idProp.ValueKind != JsonValueKind.String)
                    {
                        results.Add("INVALID");
                        continue;
                    }
                    string id = idProp.GetString() ?? "";

                    if (!holds.TryGetValue(id, out var items))
                    {
                        results.Add("NOT_FOUND");
                        continue;
                    }

                    foreach (var kvp in items)
                    {
                        available[kvp.Key] += kvp.Value;
                    }
                    holds.Remove(id);
                    holdOrder.Remove(id);
                    results.Add("OK");
                }
                else if (op == "fulfill")
                {
                    if (!opEl.TryGetProperty("id", out var idProp) || idProp.ValueKind != JsonValueKind.String)
                    {
                        results.Add("INVALID");
                        continue;
                    }
                    string id = idProp.GetString() ?? "";

                    if (!holds.ContainsKey(id))
                    {
                        results.Add("NOT_FOUND");
                        continue;
                    }

                    holds.Remove(id);
                    holdOrder.Remove(id);
                    results.Add("OK");
                }
                else
                {
                    results.Add("INVALID");
                }
            }
        }

        return SerializeResponse(results, stockOrder, available, holds, holdOrder);
    }

    static string SerializeResponse(
        List<string> results,
        List<string> stockOrder,
        Dictionary<string, int> available,
        Dictionary<string, Dictionary<string, int>> holds,
        List<string> holdOrder)
    {
        using var ms = new MemoryStream();
        using (var writer = new Utf8JsonWriter(ms))
        {
            writer.WriteStartObject();

            writer.WritePropertyName("results");
            writer.WriteStartArray();
            foreach (var r in results)
            {
                writer.WriteStringValue(r);
            }
            writer.WriteEndArray();

            writer.WritePropertyName("available");
            writer.WriteStartObject();
            foreach (var sku in stockOrder)
            {
                if (available.TryGetValue(sku, out int qty))
                {
                    writer.WriteNumber(sku, qty);
                }
            }
            foreach (var kvp in available)
            {
                if (!stockOrder.Contains(kvp.Key))
                {
                    writer.WriteNumber(kvp.Key, kvp.Value);
                }
            }
            writer.WriteEndObject();

            writer.WritePropertyName("holds");
            writer.WriteStartObject();
            foreach (var id in holdOrder)
            {
                if (holds.TryGetValue(id, out var items))
                {
                    writer.WritePropertyName(id);
                    writer.WriteStartObject();
                    foreach (var item in items)
                    {
                        writer.WriteNumber(item.Key, item.Value);
                    }
                    writer.WriteEndObject();
                }
            }
            writer.WriteEndObject();

            writer.WriteEndObject();
            writer.Flush();
        }
        return Encoding.UTF8.GetString(ms.ToArray());
    }
}

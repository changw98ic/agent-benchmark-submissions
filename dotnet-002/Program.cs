using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Encodings.Web;

public class Program
{
    public static void Main()
    {
        var options = new JsonSerializerOptions { Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping };
        string? line;
        while ((line = Console.ReadLine()) != null)
        {
            try
            {
                if (string.IsNullOrWhiteSpace(line))
                {
                    Console.WriteLine("INVALID_JSON");
                    continue;
                }
                using var doc = JsonDocument.Parse(line);
                var root = doc.RootElement;
                if (root.ValueKind != JsonValueKind.Object)
                {
                    Console.WriteLine("INVALID_JSON");
                    continue;
                }
                if (!root.TryGetProperty("stock", out var stockElem) || stockElem.ValueKind != JsonValueKind.Object)
                {
                    Console.WriteLine("INVALID_JSON");
                    continue;
                }
                if (!root.TryGetProperty("ops", out var opsElem) || opsElem.ValueKind != JsonValueKind.Array)
                {
                    Console.WriteLine("INVALID_JSON");
                    continue;
                }

                var available = new Dictionary<string, long>();
                bool negativeStock = false;
                foreach (var prop in stockElem.EnumerateObject())
                {
                    if (prop.Value.ValueKind != JsonValueKind.Number || !prop.Value.TryGetInt64(out long val))
                    {
                        throw new JsonException();
                    }
                    if (val < 0)
                    {
                        negativeStock = true;
                    }
                    available[prop.Name] = val;
                }

                if (negativeStock)
                {
                    var resp = new Response
                    {
                        results = new List<string> { "STOCK" },
                        available = new Dictionary<string, long>(),
                        holds = new Dictionary<string, Dictionary<string, long>>()
                    };
                    Console.WriteLine(JsonSerializer.Serialize(resp, options));
                    continue;
                }

                var usedIds = new HashSet<string>();
                var activeHolds = new Dictionary<string, Dictionary<string, long>>();
                var results = new List<string>();

                foreach (var opElem in opsElem.EnumerateArray())
                {
                    if (opElem.ValueKind != JsonValueKind.Object)
                    {
                        results.Add("INVALID");
                        continue;
                    }
                    if (!opElem.TryGetProperty("op", out var opNameElem) || opNameElem.ValueKind != JsonValueKind.String)
                    {
                        results.Add("INVALID");
                        continue;
                    }
                    string opName = opNameElem.GetString()!;
                    if (opName == "reserve")
                    {
                        if (!opElem.TryGetProperty("id", out var idElem) || idElem.ValueKind != JsonValueKind.String)
                        {
                            results.Add("INVALID");
                            continue;
                        }
                        string id = idElem.GetString()!;
                        if (usedIds.Contains(id))
                        {
                            results.Add("DUPLICATE_ID");
                            continue;
                        }
                        if (!opElem.TryGetProperty("items", out var itemsElem) || itemsElem.ValueKind != JsonValueKind.Object)
                        {
                            results.Add("INVALID");
                            continue;
                        }
                        var items = new Dictionary<string, long>();
                        bool invalidItems = false;
                        int itemCount = 0;
                        foreach (var itemProp in itemsElem.EnumerateObject())
                        {
                            itemCount++;
                            if (itemProp.Value.ValueKind != JsonValueKind.Number || !itemProp.Value.TryGetInt64(out long qty))
                            {
                                invalidItems = true;
                                break;
                            }
                            if (qty <= 0)
                            {
                                invalidItems = true;
                                break;
                            }
                            if (!available.ContainsKey(itemProp.Name))
                            {
                                invalidItems = true;
                                break;
                            }
                            if (items.TryGetValue(itemProp.Name, out long existing))
                            {
                                try
                                {
                                    checked { items[itemProp.Name] = existing + qty; }
                                }
                                catch (OverflowException)
                                {
                                    invalidItems = true;
                                    break;
                                }
                            }
                            else
                            {
                                items[itemProp.Name] = qty;
                            }
                        }
                        if (invalidItems || itemCount == 0)
                        {
                            results.Add("INVALID");
                            continue;
                        }
                        bool insufficient = false;
                        foreach (var kv in items)
                        {
                            if (available[kv.Key] < kv.Value)
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
                        foreach (var kv in items)
                        {
                            available[kv.Key] -= kv.Value;
                        }
                        usedIds.Add(id);
                        activeHolds[id] = new Dictionary<string, long>(items);
                        results.Add("OK");
                    }
                    else if (opName == "release" || opName == "fulfill")
                    {
                        if (!opElem.TryGetProperty("id", out var idElem) || idElem.ValueKind != JsonValueKind.String)
                        {
                            results.Add("INVALID");
                            continue;
                        }
                        string id = idElem.GetString()!;
                        if (!activeHolds.TryGetValue(id, out var hold))
                        {
                            results.Add("NOT_FOUND");
                            continue;
                        }
                        if (opName == "release")
                        {
                            foreach (var kv in hold)
                            {
                                if (available.TryGetValue(kv.Key, out long cur))
                                {
                                    available[kv.Key] = cur + kv.Value;
                                }
                                else
                                {
                                    available[kv.Key] = kv.Value;
                                }
                            }
                        }
                        activeHolds.Remove(id);
                        results.Add("OK");
                    }
                    else
                    {
                        results.Add("INVALID");
                    }
                }

                var holdsOut = new Dictionary<string, Dictionary<string, long>>();
                foreach (var kv in activeHolds)
                {
                    holdsOut[kv.Key] = new Dictionary<string, long>(kv.Value);
                }

                var response = new Response
                {
                    results = results,
                    available = available,
                    holds = holdsOut
                };
                Console.WriteLine(JsonSerializer.Serialize(response, options));
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
    }

    public class Response
    {
        public List<string> results { get; set; } = new();
        public Dictionary<string, long> available { get; set; } = new();
        public Dictionary<string, Dictionary<string, long>> holds { get; set; } = new();
    }
}

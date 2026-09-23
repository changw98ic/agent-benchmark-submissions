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
            ProcessLine(line);
        }
    }

    static void ProcessLine(string line)
    {
        try
        {
            using var doc = JsonDocument.Parse(line);
            var root = doc.RootElement;
            var fields = root.GetProperty("fields");
            var data = root.GetProperty("data");

            var errors = new List<(string Field, string Code)>();
            var defined = new HashSet<string>(StringComparer.Ordinal);
            var fieldList = new List<(string Name, JsonElement Rule)>();

            foreach (var prop in fields.EnumerateObject())
            {
                defined.Add(prop.Name);
                fieldList.Add((prop.Name, prop.Value));
            }

            fieldList.Sort((a, b) => string.CompareOrdinal(a.Name, b.Name));

            foreach (var (fieldName, rule) in fieldList)
            {
                bool required = rule.TryGetProperty("required", out var reqEl) && reqEl.ValueKind == JsonValueKind.True;
                string type = rule.TryGetProperty("type", out var typeEl) ? typeEl.GetString() ?? "" : "";

                if (!data.TryGetProperty(fieldName, out var valueEl))
                {
                    if (required)
                    {
                        errors.Add((fieldName, "REQUIRED"));
                    }
                    continue;
                }

                if (valueEl.ValueKind == JsonValueKind.Null)
                {
                    errors.Add((fieldName, "TYPE"));
                    continue;
                }

                if (type == "string")
                {
                    if (valueEl.ValueKind != JsonValueKind.String)
                    {
                        errors.Add((fieldName, "TYPE"));
                        continue;
                    }

                    string s = valueEl.GetString()!;
                    int length = CountCodePoints(s);

                    if (rule.TryGetProperty("min", out var minEl) && minEl.ValueKind == JsonValueKind.Number)
                    {
                        int min = GetInt(minEl);
                        if (length < min)
                        {
                            errors.Add((fieldName, "MIN"));
                        }
                    }

                    if (rule.TryGetProperty("max", out var maxEl) && maxEl.ValueKind == JsonValueKind.Number)
                    {
                        int max = GetInt(maxEl);
                        if (length > max)
                        {
                            errors.Add((fieldName, "MAX"));
                        }
                    }

                    if (rule.TryGetProperty("enum", out var enumEl) && enumEl.ValueKind == JsonValueKind.Array)
                    {
                        bool found = false;
                        foreach (var item in enumEl.EnumerateArray())
                        {
                            if (item.ValueKind == JsonValueKind.String && string.Equals(item.GetString(), s, StringComparison.Ordinal))
                            {
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                        {
                            errors.Add((fieldName, "ENUM"));
                        }
                    }
                }
                else if (type == "integer")
                {
                    if (!TryGetInteger(valueEl, out double value))
                    {
                        errors.Add((fieldName, "TYPE"));
                        continue;
                    }

                    if (rule.TryGetProperty("min", out var minEl) && minEl.ValueKind == JsonValueKind.Number)
                    {
                        double min = minEl.GetDouble();
                        if (value < min)
                        {
                            errors.Add((fieldName, "MIN"));
                        }
                    }

                    if (rule.TryGetProperty("max", out var maxEl) && maxEl.ValueKind == JsonValueKind.Number)
                    {
                        double max = maxEl.GetDouble();
                        if (value > max)
                        {
                            errors.Add((fieldName, "MAX"));
                        }
                    }
                }
                else if (type == "boolean")
                {
                    if (valueEl.ValueKind != JsonValueKind.True && valueEl.ValueKind != JsonValueKind.False)
                    {
                        errors.Add((fieldName, "TYPE"));
                        continue;
                    }
                }
            }

            var unknownList = new List<string>();
            foreach (var prop in data.EnumerateObject())
            {
                if (!defined.Contains(prop.Name))
                {
                    unknownList.Add(prop.Name);
                }
            }
            unknownList.Sort(StringComparer.Ordinal);
            foreach (var name in unknownList)
            {
                errors.Add((name, "UNKNOWN"));
            }

            using var stream = new MemoryStream();
            using (var writer = new Utf8JsonWriter(stream))
            {
                writer.WriteStartObject();
                writer.WriteBoolean("valid", errors.Count == 0);
                writer.WriteStartArray("errors");
                foreach (var (field, code) in errors)
                {
                    writer.WriteStartObject();
                    writer.WriteString("field", field);
                    writer.WriteString("code", code);
                    writer.WriteEndObject();
                }
                writer.WriteEndArray();
                writer.WriteEndObject();
            }

            Console.Out.WriteLine(Encoding.UTF8.GetString(stream.ToArray()));
        }
        catch (JsonException)
        {
            Console.Out.WriteLine("INVALID_JSON");
        }
    }

    static int CountCodePoints(string s)
    {
        int count = 0;
        for (int i = 0; i < s.Length; i++)
        {
            if (char.IsHighSurrogate(s[i]) && i + 1 < s.Length && char.IsLowSurrogate(s[i + 1]))
            {
                count++;
                i++;
            }
            else
            {
                count++;
            }
        }
        return count;
    }

    static int GetInt(JsonElement el)
    {
        if (el.TryGetInt32(out int i))
        {
            return i;
        }
        double d = el.GetDouble();
        return (int)d;
    }

    static bool TryGetInteger(JsonElement el, out double value)
    {
        value = 0;
        if (el.ValueKind != JsonValueKind.Number)
        {
            return false;
        }

        if (el.TryGetInt64(out long l))
        {
            value = l;
            return true;
        }

        if (el.TryGetDouble(out double d))
        {
            if (d == Math.Truncate(d) && !double.IsInfinity(d) && !double.IsNaN(d))
            {
                value = d;
                return true;
            }
        }

        return false;
    }
}

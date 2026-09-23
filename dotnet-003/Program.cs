using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json;

class Program
{
    static void Main()
    {
        string? line;
        while ((line = Console.ReadLine()) != null)
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            string output = Process(line);
            Console.WriteLine(output);
        }
    }

    static string Process(string line)
    {
        using var doc = JsonDocument.Parse(line);
        var root = doc.RootElement;
        var fields = root.GetProperty("fields");
        var data = root.GetProperty("data");

        var rules = new Dictionary<string, JsonElement>(StringComparer.Ordinal);
        foreach (var prop in fields.EnumerateObject())
        {
            rules[prop.Name] = prop.Value;
        }

        var errors = new List<(string field, string code)>();

        foreach (var field in rules.Keys.OrderBy(x => x, StringComparer.Ordinal))
        {
            var rule = rules[field];
            bool required = rule.TryGetProperty("required", out var req) && req.GetBoolean();

            if (!data.TryGetProperty(field, out var value))
            {
                if (required)
                {
                    errors.Add((field, "REQUIRED"));
                }
                continue;
            }

            if (value.ValueKind == JsonValueKind.Null)
            {
                errors.Add((field, "TYPE"));
                continue;
            }

            string type = rule.GetProperty("type").GetString()!;
            switch (type)
            {
                case "string":
                    ValidateString(field, rule, value, errors);
                    break;
                case "integer":
                    ValidateInteger(field, rule, value, errors);
                    break;
                case "boolean":
                    ValidateBoolean(field, rule, value, errors);
                    break;
                default:
                    errors.Add((field, "TYPE"));
                    break;
            }
        }

        var unknown = new List<string>();
        foreach (var prop in data.EnumerateObject())
        {
            if (!rules.ContainsKey(prop.Name))
            {
                unknown.Add(prop.Name);
            }
        }
        unknown.Sort(StringComparer.Ordinal);
        foreach (var name in unknown)
        {
            errors.Add((name, "UNKNOWN"));
        }

        var result = new
        {
            valid = errors.Count == 0,
            errors = errors.Select(e => new { field = e.field, code = e.code }).ToList()
        };
        return JsonSerializer.Serialize(result);
    }

    static void ValidateString(string field, JsonElement rule, JsonElement value, List<(string field, string code)> errors)
    {
        if (value.ValueKind != JsonValueKind.String)
        {
            errors.Add((field, "TYPE"));
            return;
        }

        string s = value.GetString()!;
        int count = CountCodePoints(s);

        if (rule.TryGetProperty("min", out var minEl))
        {
            int min = minEl.GetInt32();
            if (count < min)
            {
                errors.Add((field, "MIN"));
            }
        }

        if (rule.TryGetProperty("max", out var maxEl))
        {
            int max = maxEl.GetInt32();
            if (count > max)
            {
                errors.Add((field, "MAX"));
            }
        }

        if (rule.TryGetProperty("enum", out var enumEl))
        {
            bool found = false;
            foreach (var item in enumEl.EnumerateArray())
            {
                if (item.ValueKind == JsonValueKind.String && item.GetString() == s)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                errors.Add((field, "ENUM"));
            }
        }
    }

    static void ValidateInteger(string field, JsonElement rule, JsonElement value, List<(string field, string code)> errors)
    {
        if (value.ValueKind != JsonValueKind.Number)
        {
            errors.Add((field, "TYPE"));
            return;
        }

        if (!TryGetInteger(value, out decimal val))
        {
            errors.Add((field, "TYPE"));
            return;
        }

        if (rule.TryGetProperty("min", out var minEl) && TryGetNumber(minEl, out decimal min))
        {
            if (val < min)
            {
                errors.Add((field, "MIN"));
            }
        }

        if (rule.TryGetProperty("max", out var maxEl) && TryGetNumber(maxEl, out decimal max))
        {
            if (val > max)
            {
                errors.Add((field, "MAX"));
            }
        }
    }

    static void ValidateBoolean(string field, JsonElement rule, JsonElement value, List<(string field, string code)> errors)
    {
        if (value.ValueKind != JsonValueKind.True && value.ValueKind != JsonValueKind.False)
        {
            errors.Add((field, "TYPE"));
        }
    }

    static int CountCodePoints(string s)
    {
        int count = 0;
        for (int i = 0; i < s.Length; i++)
        {
            if (char.IsHighSurrogate(s[i]) && i + 1 < s.Length && char.IsLowSurrogate(s[i + 1]))
            {
                i++;
            }
            count++;
        }
        return count;
    }

    static bool TryGetInteger(JsonElement value, out decimal result)
    {
        result = 0;
        if (value.ValueKind != JsonValueKind.Number)
        {
            return false;
        }

        string raw = value.GetRawText();
        if (!decimal.TryParse(raw, NumberStyles.Float, CultureInfo.InvariantCulture, out decimal d))
        {
            return false;
        }

        if (d != Math.Truncate(d))
        {
            return false;
        }

        result = d;
        return true;
    }

    static bool TryGetNumber(JsonElement value, out decimal result)
    {
        result = 0;
        if (value.ValueKind != JsonValueKind.Number)
        {
            return false;
        }

        string raw = value.GetRawText();
        return decimal.TryParse(raw, NumberStyles.Float, CultureInfo.InvariantCulture, out result);
    }
}

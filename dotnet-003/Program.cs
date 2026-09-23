using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;

class Program
{
    static void Main()
    {
        string? line;
        while ((line = Console.ReadLine()) != null)
        {
            if (line.Length == 0)
            {
                Console.WriteLine("INVALID_JSON");
                continue;
            }
            try
            {
                using var doc = JsonDocument.Parse(line);
                var errors = Validate(doc.RootElement);
                var response = new
                {
                    valid = errors.Count == 0,
                    errors = errors.Select(e => new { field = e.Field, code = e.Code }).ToArray()
                };
                Console.WriteLine(JsonSerializer.Serialize(response));
            }
            catch (JsonException)
            {
                Console.WriteLine("INVALID_JSON");
            }
            catch
            {
                Console.WriteLine("INVALID_JSON");
            }
        }
    }

    static List<FieldError> Validate(JsonElement root)
    {
        var fields = root.GetProperty("fields");
        var data = root.GetProperty("data");

        var errors = new List<FieldError>();

        var definedNames = fields.EnumerateObject()
            .Select(p => p.Name)
            .OrderBy(n => n, StringComparer.Ordinal)
            .ToArray();

        foreach (var name in definedNames)
        {
            var rule = fields.GetProperty(name);
            var type = rule.GetProperty("type").GetString();
            bool required = rule.TryGetProperty("required", out var req) && req.ValueKind == JsonValueKind.True;

            if (!data.TryGetProperty(name, out var value))
            {
                if (required)
                {
                    errors.Add(new FieldError(name, "REQUIRED"));
                }
                continue;
            }

            if (value.ValueKind == JsonValueKind.Null)
            {
                errors.Add(new FieldError(name, "TYPE"));
                continue;
            }

            if (type == "string")
            {
                if (value.ValueKind != JsonValueKind.String)
                {
                    errors.Add(new FieldError(name, "TYPE"));
                    continue;
                }

                var s = value.GetString() ?? string.Empty;
                int length = CountCodePoints(s);

                if (rule.TryGetProperty("min", out var minEl))
                {
                    int min = minEl.GetInt32();
                    if (length < min)
                    {
                        errors.Add(new FieldError(name, "MIN"));
                    }
                }

                if (rule.TryGetProperty("max", out var maxEl))
                {
                    int max = maxEl.GetInt32();
                    if (length > max)
                    {
                        errors.Add(new FieldError(name, "MAX"));
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
                        errors.Add(new FieldError(name, "ENUM"));
                    }
                }
            }
            else if (type == "integer")
            {
                if (value.ValueKind != JsonValueKind.Number || !TryGetInteger(value, out long intValue))
                {
                    errors.Add(new FieldError(name, "TYPE"));
                    continue;
                }

                if (rule.TryGetProperty("min", out var minEl))
                {
                    double min = minEl.GetDouble();
                    if (intValue < min)
                    {
                        errors.Add(new FieldError(name, "MIN"));
                    }
                }

                if (rule.TryGetProperty("max", out var maxEl))
                {
                    double max = maxEl.GetDouble();
                    if (intValue > max)
                    {
                        errors.Add(new FieldError(name, "MAX"));
                    }
                }
            }
            else if (type == "boolean")
            {
                if (value.ValueKind != JsonValueKind.True && value.ValueKind != JsonValueKind.False)
                {
                    errors.Add(new FieldError(name, "TYPE"));
                    continue;
                }
            }
        }

        var unknownNames = data.EnumerateObject()
            .Select(p => p.Name)
            .Where(n => !fields.TryGetProperty(n, out _))
            .OrderBy(n => n, StringComparer.Ordinal);

        foreach (var name in unknownNames)
        {
            errors.Add(new FieldError(name, "UNKNOWN"));
        }

        return errors;
    }

    static int CountCodePoints(string s)
    {
        int count = 0;
        for (int i = 0; i < s.Length; i++)
        {
            count++;
            if (char.IsHighSurrogate(s[i]) && i + 1 < s.Length && char.IsLowSurrogate(s[i + 1]))
            {
                i++;
            }
        }
        return count;
    }

    static bool TryGetInteger(JsonElement value, out long result)
    {
        if (value.TryGetInt64(out result))
        {
            return true;
        }

        if (value.TryGetDecimal(out decimal dec) && dec == decimal.Truncate(dec) &&
            dec >= long.MinValue && dec <= long.MaxValue)
        {
            result = (long)dec;
            return true;
        }

        result = 0;
        return false;
    }

    record FieldError(string Field, string Code);
}

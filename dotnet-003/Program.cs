using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;

public class Program
{
    public static void Main()
    {
        using var reader = new StreamReader(Console.OpenStandardInput());
        using var writer = new StreamWriter(Console.OpenStandardOutput());
        writer.AutoFlush = true;
        string? line;
        while ((line = reader.ReadLine()) != null)
        {
            if (string.IsNullOrWhiteSpace(line)) continue;
            writer.WriteLine(Process(line));
        }
    }

    static string Process(string line)
    {
        using var doc = JsonDocument.Parse(line);
        var root = doc.RootElement;
        var fields = root.GetProperty("fields");
        var data = root.GetProperty("data");

        var errors = new List<Error>();

        var definedNames = new List<string>();
        foreach (var prop in fields.EnumerateObject()) definedNames.Add(prop.Name);
        definedNames.Sort(StringComparer.Ordinal);

        foreach (var name in definedNames)
        {
            var rule = fields.GetProperty(name);
            bool required = rule.TryGetProperty("required", out var reqProp) && reqProp.GetBoolean();
            string type = rule.GetProperty("type").GetString()!;

            if (!data.TryGetProperty(name, out var value))
            {
                if (required) errors.Add(new Error(name, "REQUIRED"));
                continue;
            }

            if (value.ValueKind == JsonValueKind.Null)
            {
                errors.Add(new Error(name, "TYPE"));
                continue;
            }

            switch (type)
            {
                case "string":
                    if (value.ValueKind != JsonValueKind.String)
                    {
                        errors.Add(new Error(name, "TYPE"));
                        continue;
                    }
                    string str = value.GetString()!;
                    int len = CountCodePoints(str);

                    if (rule.TryGetProperty("min", out var minProp) && minProp.ValueKind == JsonValueKind.Number)
                    {
                        int min = minProp.GetInt32();
                        if (len < min) errors.Add(new Error(name, "MIN"));
                    }
                    if (rule.TryGetProperty("max", out var maxProp) && maxProp.ValueKind == JsonValueKind.Number)
                    {
                        int max = maxProp.GetInt32();
                        if (len > max) errors.Add(new Error(name, "MAX"));
                    }
                    if (rule.TryGetProperty("enum", out var enumProp) && enumProp.ValueKind == JsonValueKind.Array)
                    {
                        bool found = false;
                        foreach (var item in enumProp.EnumerateArray())
                        {
                            if (item.ValueKind == JsonValueKind.String && string.Equals(item.GetString(), str, StringComparison.Ordinal))
                            {
                                found = true;
                                break;
                            }
                        }
                        if (!found) errors.Add(new Error(name, "ENUM"));
                    }
                    break;
                case "integer":
                    if (!IsInteger(value, out double intVal))
                    {
                        errors.Add(new Error(name, "TYPE"));
                        continue;
                    }
                    if (rule.TryGetProperty("min", out var minPropInt) && minPropInt.ValueKind == JsonValueKind.Number)
                    {
                        double min = minPropInt.GetDouble();
                        if (intVal < min) errors.Add(new Error(name, "MIN"));
                    }
                    if (rule.TryGetProperty("max", out var maxPropInt) && maxPropInt.ValueKind == JsonValueKind.Number)
                    {
                        double max = maxPropInt.GetDouble();
                        if (intVal > max) errors.Add(new Error(name, "MAX"));
                    }
                    break;
                case "boolean":
                    if (value.ValueKind != JsonValueKind.True && value.ValueKind != JsonValueKind.False)
                    {
                        errors.Add(new Error(name, "TYPE"));
                    }
                    break;
            }
        }

        var unknownNames = new List<string>();
        foreach (var prop in data.EnumerateObject())
        {
            if (!fields.TryGetProperty(prop.Name, out _))
            {
                unknownNames.Add(prop.Name);
            }
        }
        unknownNames.Sort(StringComparer.Ordinal);
        foreach (var name in unknownNames)
        {
            errors.Add(new Error(name, "UNKNOWN"));
        }

        var result = new
        {
            valid = errors.Count == 0,
            errors = errors.Select(e => new { field = e.Field, code = e.Code }).ToArray()
        };

        return JsonSerializer.Serialize(result);
    }

    static int CountCodePoints(string s)
    {
        int count = 0;
        foreach (var _ in s.EnumerateRunes()) count++;
        return count;
    }

    static bool IsInteger(JsonElement value, out double result)
    {
        result = 0;
        if (value.ValueKind != JsonValueKind.Number) return false;
        if (value.TryGetDouble(out double d))
        {
            if (double.IsFinite(d) && d == Math.Truncate(d))
            {
                result = d;
                return true;
            }
        }
        return false;
    }

    record Error(string Field, string Code);
}

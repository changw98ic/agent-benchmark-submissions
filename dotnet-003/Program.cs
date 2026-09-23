using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;

class Program
{
    static void Main()
    {
        string line;
        while ((line = Console.ReadLine()) != null)
        {
            if (line.Length == 0)
            {
                Console.WriteLine("INVALID_JSON");
                continue;
            }
            try
            {
                Console.WriteLine(ProcessLine(line));
            }
            catch
            {
                Console.WriteLine("INVALID_JSON");
            }
        }
    }

    static string ProcessLine(string line)
    {
        using var doc = JsonDocument.Parse(line);
        var root = doc.RootElement;
        if (root.ValueKind != JsonValueKind.Object) throw new JsonException();
        if (!root.TryGetProperty("fields", out var fieldsEl) || fieldsEl.ValueKind != JsonValueKind.Object) throw new JsonException();
        if (!root.TryGetProperty("data", out var dataEl) || dataEl.ValueKind != JsonValueKind.Object) throw new JsonException();

        var rules = new Dictionary<string, Rule>(StringComparer.Ordinal);
        foreach (var prop in fieldsEl.EnumerateObject())
        {
            var name = prop.Name;
            var ruleEl = prop.Value;
            if (ruleEl.ValueKind != JsonValueKind.Object) throw new JsonException();
            var rule = new Rule();
            if (!ruleEl.TryGetProperty("type", out var typeEl) || typeEl.ValueKind != JsonValueKind.String) throw new JsonException();
            rule.Type = typeEl.GetString();
            if (rule.Type != "string" && rule.Type != "integer" && rule.Type != "boolean") throw new JsonException();
            if (ruleEl.TryGetProperty("required", out var reqEl))
            {
                if (reqEl.ValueKind == JsonValueKind.True) rule.Required = true;
                else if (reqEl.ValueKind == JsonValueKind.False) rule.Required = false;
                else throw new JsonException();
            }
            if (ruleEl.TryGetProperty("min", out var minEl))
            {
                if (rule.Type == "boolean") throw new JsonException();
                rule.Min = ParseNumber(minEl);
            }
            if (ruleEl.TryGetProperty("max", out var maxEl))
            {
                if (rule.Type == "boolean") throw new JsonException();
                rule.Max = ParseNumber(maxEl);
            }
            if (ruleEl.TryGetProperty("enum", out var enumEl))
            {
                if (rule.Type != "string") throw new JsonException();
                if (enumEl.ValueKind != JsonValueKind.Array) throw new JsonException();
                rule.Enum = new List<string>();
                foreach (var item in enumEl.EnumerateArray())
                {
                    if (item.ValueKind != JsonValueKind.String) throw new JsonException();
                    rule.Enum.Add(item.GetString());
                }
            }
            rules[name] = rule;
        }

        var errors = new List<Error>();

        var sortedDefined = rules.Keys.OrderBy(k => k, StringComparer.Ordinal).ToList();
        foreach (var name in sortedDefined)
        {
            var rule = rules[name];
            bool has = dataEl.TryGetProperty(name, out var value);
            if (!has)
            {
                if (rule.Required) errors.Add(new Error(name, "REQUIRED"));
                continue;
            }
            if (value.ValueKind == JsonValueKind.Null)
            {
                errors.Add(new Error(name, "TYPE"));
                continue;
            }
            bool typeOk = false;
            switch (rule.Type)
            {
                case "string":
                    typeOk = value.ValueKind == JsonValueKind.String;
                    break;
                case "integer":
                    typeOk = value.ValueKind == JsonValueKind.Number && value.TryGetInt64(out _);
                    break;
                case "boolean":
                    typeOk = value.ValueKind == JsonValueKind.True || value.ValueKind == JsonValueKind.False;
                    break;
            }
            if (!typeOk)
            {
                errors.Add(new Error(name, "TYPE"));
                continue;
            }
            if (rule.Type == "string")
            {
                string s = value.GetString();
                int len = CountUnicodeCodePoints(s);
                if (rule.Min.HasValue && len < rule.Min.Value)
                    errors.Add(new Error(name, "MIN"));
                if (rule.Max.HasValue && len > rule.Max.Value)
                    errors.Add(new Error(name, "MAX"));
                if (rule.Enum != null)
                {
                    bool found = false;
                    foreach (var e in rule.Enum)
                    {
                        if (string.Equals(e, s, StringComparison.Ordinal)) { found = true; break; }
                    }
                    if (!found) errors.Add(new Error(name, "ENUM"));
                }
            }
            else if (rule.Type == "integer")
            {
                if (value.TryGetDecimal(out var num))
                {
                    if (rule.Min.HasValue && num < rule.Min.Value)
                        errors.Add(new Error(name, "MIN"));
                    if (rule.Max.HasValue && num > rule.Max.Value)
                        errors.Add(new Error(name, "MAX"));
                }
            }
        }

        var unknown = new List<string>();
        foreach (var prop in dataEl.EnumerateObject())
        {
            if (!rules.ContainsKey(prop.Name))
            {
                unknown.Add(prop.Name);
            }
        }
        foreach (var name in unknown.OrderBy(k => k, StringComparer.Ordinal))
        {
            errors.Add(new Error(name, "UNKNOWN"));
        }

        return BuildResponse(errors.Count == 0, errors);
    }

    static decimal ParseNumber(JsonElement el)
    {
        if (el.ValueKind != JsonValueKind.Number) throw new JsonException();
        if (el.TryGetDecimal(out var d)) return d;
        return (decimal)el.GetDouble();
    }

    static int CountUnicodeCodePoints(string s)
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

    static string BuildResponse(bool valid, List<Error> errors)
    {
        using var ms = new MemoryStream();
        using (var writer = new Utf8JsonWriter(ms))
        {
            writer.WriteStartObject();
            writer.WriteBoolean("valid", valid);
            writer.WritePropertyName("errors");
            writer.WriteStartArray();
            foreach (var err in errors)
            {
                writer.WriteStartObject();
                writer.WriteString("field", err.Field);
                writer.WriteString("code", err.Code);
                writer.WriteEndObject();
            }
            writer.WriteEndArray();
            writer.WriteEndObject();
        }
        return Encoding.UTF8.GetString(ms.ToArray());
    }

    class Rule
    {
        public string Type = "";
        public bool Required;
        public decimal? Min;
        public decimal? Max;
        public List<string> Enum;
    }

    readonly struct Error
    {
        public readonly string Field;
        public readonly string Code;
        public Error(string field, string code)
        {
            Field = field;
            Code = code;
        }
    }
}

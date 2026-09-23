using System;
using System.Collections.Generic;
using System.Globalization;
using System.Numerics;
using System.Text.Json;

class Program
{
    static void Main()
    {
        string? line;
        while ((line = Console.ReadLine()) != null)
        {
            if (string.IsNullOrWhiteSpace(line))
            {
                Console.WriteLine("INVALID_JSON");
                continue;
            }

            try
            {
                using var doc = JsonDocument.Parse(line);
                var root = doc.RootElement;
                if (root.ValueKind != JsonValueKind.Object)
                {
                    Console.WriteLine("INVALID_JSON");
                    continue;
                }

                var fields = root.GetProperty("fields");
                var data = root.GetProperty("data");

                var errors = new List<ErrorItem>();
                var definedNames = new HashSet<string>(StringComparer.Ordinal);
                var fieldProps = new List<JsonProperty>();

                foreach (var prop in fields.EnumerateObject())
                {
                    definedNames.Add(prop.Name);
                    fieldProps.Add(prop);
                }

                fieldProps.Sort((a, b) => StringComparer.Ordinal.Compare(a.Name, b.Name));

                foreach (var fieldProp in fieldProps)
                {
                    string fieldName = fieldProp.Name;
                    var rule = fieldProp.Value;
                    string type = rule.GetProperty("type").GetString()!;
                    bool required = false;
                    if (rule.TryGetProperty("required", out var requiredProp))
                    {
                        required = requiredProp.GetBoolean();
                    }

                    if (!data.TryGetProperty(fieldName, out var value))
                    {
                        if (required)
                        {
                            errors.Add(new ErrorItem(fieldName, "REQUIRED"));
                        }
                        continue;
                    }

                    if (value.ValueKind == JsonValueKind.Null)
                    {
                        errors.Add(new ErrorItem(fieldName, "TYPE"));
                        continue;
                    }

                    bool typeOk = false;
                    if (type == "string")
                    {
                        typeOk = value.ValueKind == JsonValueKind.String;
                    }
                    else if (type == "integer")
                    {
                        if (value.ValueKind == JsonValueKind.Number)
                        {
                            var num = ParseJsonNumber(value.GetRawText());
                            typeOk = num.TryGetInteger(out _);
                        }
                    }
                    else if (type == "boolean")
                    {
                        typeOk = value.ValueKind == JsonValueKind.True || value.ValueKind == JsonValueKind.False;
                    }

                    if (!typeOk)
                    {
                        errors.Add(new ErrorItem(fieldName, "TYPE"));
                        continue;
                    }

                    if (type == "string")
                    {
                        string s = value.GetString()!;

                        if (rule.TryGetProperty("min", out var minProp))
                        {
                            long min = minProp.GetInt64();
                            if (CountCodePoints(s) < min)
                            {
                                errors.Add(new ErrorItem(fieldName, "MIN"));
                            }
                        }

                        if (rule.TryGetProperty("max", out var maxProp))
                        {
                            long max = maxProp.GetInt64();
                            if (CountCodePoints(s) > max)
                            {
                                errors.Add(new ErrorItem(fieldName, "MAX"));
                            }
                        }

                        if (rule.TryGetProperty("enum", out var enumProp))
                        {
                            bool matched = false;
                            foreach (var item in enumProp.EnumerateArray())
                            {
                                if (item.ValueKind == JsonValueKind.String &&
                                    string.Equals(s, item.GetString(), StringComparison.Ordinal))
                                {
                                    matched = true;
                                    break;
                                }
                            }
                            if (!matched)
                            {
                                errors.Add(new ErrorItem(fieldName, "ENUM"));
                            }
                        }
                    }
                    else if (type == "integer")
                    {
                        var num = ParseJsonNumber(value.GetRawText());
                        num.TryGetInteger(out BigInteger intValue);

                        if (rule.TryGetProperty("min", out var minProp))
                        {
                            var min = ParseJsonNumber(minProp.GetRawText());
                            if (IsLessThan(intValue, min))
                            {
                                errors.Add(new ErrorItem(fieldName, "MIN"));
                            }
                        }

                        if (rule.TryGetProperty("max", out var maxProp))
                        {
                            var max = ParseJsonNumber(maxProp.GetRawText());
                            if (IsGreaterThan(intValue, max))
                            {
                                errors.Add(new ErrorItem(fieldName, "MAX"));
                            }
                        }
                    }
                }

                var unknownNames = new List<string>();
                foreach (var prop in data.EnumerateObject())
                {
                    if (!definedNames.Contains(prop.Name))
                    {
                        unknownNames.Add(prop.Name);
                    }
                }
                unknownNames.Sort(StringComparer.Ordinal);
                foreach (var name in unknownNames)
                {
                    errors.Add(new ErrorItem(name, "UNKNOWN"));
                }

                bool valid = errors.Count == 0;
                var response = new
                {
                    valid = valid,
                    errors = errors.ConvertAll(e => new { field = e.Field, code = e.Code })
                };
                Console.WriteLine(JsonSerializer.Serialize(response));
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

    static JsonNumberParts ParseJsonNumber(string raw)
    {
        int i = 0;
        bool negative = false;
        if (raw[i] == '-')
        {
            negative = true;
            i++;
        }

        int intStart = i;
        while (i < raw.Length && char.IsDigit(raw[i]))
        {
            i++;
        }
        string intPart = raw.Substring(intStart, i - intStart);
        string fracPart = "";

        if (i < raw.Length && raw[i] == '.')
        {
            i++;
            int fracStart = i;
            while (i < raw.Length && char.IsDigit(raw[i]))
            {
                i++;
            }
            fracPart = raw.Substring(fracStart, i - fracStart);
        }

        int exponent = 0;
        if (i < raw.Length && (raw[i] == 'e' || raw[i] == 'E'))
        {
            i++;
            bool expNegative = false;
            if (i < raw.Length && raw[i] == '+')
            {
                i++;
            }
            else if (i < raw.Length && raw[i] == '-')
            {
                expNegative = true;
                i++;
            }

            int expStart = i;
            while (i < raw.Length && char.IsDigit(raw[i]))
            {
                i++;
            }
            string expStr = raw.Substring(expStart, i - expStart);
            if (expStr.Length > 0)
            {
                if (!int.TryParse(expStr, NumberStyles.None, CultureInfo.InvariantCulture, out exponent))
                {
                    throw new FormatException("Exponent too large");
                }
            }
            if (expNegative)
            {
                exponent = -exponent;
            }
        }

        string digits = intPart + fracPart;
        BigInteger mantissa = digits.Length == 0 ? BigInteger.Zero : BigInteger.Parse(digits, CultureInfo.InvariantCulture);
        if (negative)
        {
            mantissa = -mantissa;
        }

        int expShift = exponent - fracPart.Length;
        BigInteger numerator;
        BigInteger denominator;
        if (expShift >= 0)
        {
            numerator = mantissa * BigInteger.Pow(10, expShift);
            denominator = BigInteger.One;
        }
        else
        {
            numerator = mantissa;
            denominator = BigInteger.Pow(10, -expShift);
        }

        return new JsonNumberParts(numerator, denominator);
    }

    static bool IsLessThan(BigInteger value, JsonNumberParts number)
    {
        return value * number.Denominator < number.Numerator;
    }

    static bool IsGreaterThan(BigInteger value, JsonNumberParts number)
    {
        return value * number.Denominator > number.Numerator;
    }
}

class ErrorItem
{
    public string Field { get; }
    public string Code { get; }

    public ErrorItem(string field, string code)
    {
        Field = field;
        Code = code;
    }
}

struct JsonNumberParts
{
    public BigInteger Numerator { get; }
    public BigInteger Denominator { get; }

    public JsonNumberParts(BigInteger numerator, BigInteger denominator)
    {
        Numerator = numerator;
        Denominator = denominator;
    }

    public bool TryGetInteger(out BigInteger value)
    {
        if (Denominator == BigInteger.One)
        {
            value = Numerator;
            return true;
        }

        BigInteger remainder = BigInteger.Remainder(Numerator, Denominator);
        if (remainder.IsZero)
        {
            value = BigInteger.Divide(Numerator, Denominator);
            return true;
        }

        value = BigInteger.Zero;
        return false;
    }
}

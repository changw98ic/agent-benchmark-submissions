using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;

var options = new JsonSerializerOptions
{
    PropertyNameCaseInsensitive = true
};

string? line;
while ((line = Console.ReadLine()) != null)
{
    if (string.IsNullOrWhiteSpace(line)) continue;

    Request? req = null;
    try
    {
        req = JsonSerializer.Deserialize<Request>(line, options);
    }
    catch
    {
        // Malformed JSON; treat as INVALID.
    }

    Response response;
    if (req == null)
    {
        response = new Response { Results = new List<string> { "INVALID" } };
    }
    else
    {
        response = Process(req);
    }

    Console.WriteLine(JsonSerializer.Serialize(response, options));
}

static Response Process(Request req)
{
    var response = new Response();

    if (req.Stock != null && req.Stock.Values.Any(v => v < 0))
    {
        response.Results.Add("STOCK");
        return response;
    }

    var available = new Dictionary<string, int>(req.Stock ?? new Dictionary<string, int>());
    var holds = new Dictionary<string, Dictionary<string, int>>();
    var usedIds = new HashSet<string>();

    if (req.Ops != null)
    {
        foreach (var op in req.Ops)
        {
            string status = ProcessOp(op, available, holds, usedIds);
            response.Results.Add(status);
        }
    }

    response.Available = available;
    response.Holds = holds;
    return response;
}

static string ProcessOp(Operation op, Dictionary<string, int> available, Dictionary<string, Dictionary<string, int>> holds, HashSet<string> usedIds)
{
    if (op == null || string.IsNullOrEmpty(op.Op))
        return "INVALID";

    switch (op.Op)
    {
        case "reserve":
            return Reserve(op, available, holds, usedIds);
        case "release":
            return Release(op, available, holds);
        case "fulfill":
            return Fulfill(op, holds);
        default:
            return "INVALID";
    }
}

static string Reserve(Operation op, Dictionary<string, int> available, Dictionary<string, Dictionary<string, int>> holds, HashSet<string> usedIds)
{
    if (op.Id == null)
        return "INVALID";

    if (usedIds.Contains(op.Id))
        return "DUPLICATE_ID";

    if (op.Items == null || op.Items.Count == 0)
        return "INVALID";

    foreach (var kv in op.Items)
    {
        if (kv.Value <= 0)
            return "INVALID";
        if (!available.ContainsKey(kv.Key))
            return "INVALID";
    }

    foreach (var kv in op.Items)
    {
        if (available[kv.Key] < kv.Value)
            return "INSUFFICIENT";
    }

    foreach (var kv in op.Items)
    {
        available[kv.Key] -= kv.Value;
    }

    holds[op.Id] = new Dictionary<string, int>(op.Items);
    usedIds.Add(op.Id);
    return "OK";
}

static string Release(Operation op, Dictionary<string, int> available, Dictionary<string, Dictionary<string, int>> holds)
{
    if (op.Id == null)
        return "INVALID";

    if (!holds.TryGetValue(op.Id, out var items))
        return "NOT_FOUND";

    foreach (var kv in items)
    {
        if (available.TryGetValue(kv.Key, out int cur))
            available[kv.Key] = cur + kv.Value;
        else
            available[kv.Key] = kv.Value;
    }

    holds.Remove(op.Id);
    return "OK";
}

static string Fulfill(Operation op, Dictionary<string, Dictionary<string, int>> holds)
{
    if (op.Id == null)
        return "INVALID";

    if (!holds.ContainsKey(op.Id))
        return "NOT_FOUND";

    holds.Remove(op.Id);
    return "OK";
}

public class Request
{
    [JsonPropertyName("stock")]
    public Dictionary<string, int>? Stock { get; set; }

    [JsonPropertyName("ops")]
    public List<Operation>? Ops { get; set; }
}

public class Operation
{
    [JsonPropertyName("op")]
    public string? Op { get; set; }

    [JsonPropertyName("id")]
    public string? Id { get; set; }

    [JsonPropertyName("items")]
    public Dictionary<string, int>? Items { get; set; }
}

public class Response
{
    [JsonPropertyName("results")]
    public List<string> Results { get; set; } = new();

    [JsonPropertyName("available")]
    public Dictionary<string, int> Available { get; set; } = new();

    [JsonPropertyName("holds")]
    public Dictionary<string, Dictionary<string, int>> Holds { get; set; } = new();
}

{"files":[{"path":"Program.cs","content":"using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Nodes;

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
                var node = JsonNode.Parse(line);
                if (node is not JsonObject obj)
                {
                    Console.WriteLine("INVALID_JSON");
                    continue;
                }
                Console.WriteLine(Process(obj));
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

    static string Process(JsonObject req)
    {
        if (req["stock"] is not JsonObject stockObj)
            return Output(new[] { "INVALID" }, new JsonObject(), new JsonObject());

        var initial = new Dictionary<string, long>();
        var available = new Dictionary<string, long>();
        foreach (var kv in stockObj)
        {
            if (kv.Value is not JsonValue v || !v.TryGetValue<long>(out long q))
                return Output(new[] { "INVALID" }, new JsonObject(), new JsonObject());
            if (q < 0)
                return Output(new[] { "STOCK" }, new JsonObject(), new JsonObject());
            initial[kv.Key] = q;
            available[kv.Key] = q;
        }

        if (req["ops"] is not JsonArray ops)
            return Output(new[] { "INVALID" }, new JsonObject(), new JsonObject());

        var results = new List<string>();
        var active = new Dictionary<string, Dictionary<string, long>>();
        var used = new HashSet<string>();

        foreach (var opNode in ops)
        {
            if (opNode is not JsonObject op)
            {
                results.Add("INVALID");
                continue;
            }
            string? kind = op["op"]?.GetValue<string>();
            if (kind == "reserve")
            {
                string? id = op["id

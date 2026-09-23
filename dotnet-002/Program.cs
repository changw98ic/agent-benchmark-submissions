- if (!opEl.TryGetProperty("id", out var idProp) || idProp.ValueKind != String) { results.Add("INVALID"); continue; }
  - string id = idProp.GetString();
  - if (usedIds.Contains(id)) { results.Add("DUPLICATE_ID"); continue; }
  - if (!opEl.TryGetProperty("items", out var itemsProp) || itemsProp.ValueKind != Object) { results.Add("INVALID"); continue; }
  - var items = new SortedDictionary<string,int>(StringComparer.Ordinal);
  - bool itemsValid = true;
  - int count = 0;
  - foreach (var item in itemsProp.EnumerateObject()) {
      count++;
      if (item.Value.ValueKind != JsonValueKind.Number || !item.Value.TryGetInt32(out int qty) || qty <= 0) { itemsValid = false; break; }
      string sku = item.Name;
      if (!available.ContainsKey(sku)) { itemsValid = false; break; }
      items[sku] = qty;
    }
  - if (count == 0 || !itemsValid) { results.Add("INVALID"); continue; }

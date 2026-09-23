#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <list>
#include <vector>

using json = nlohmann::json;

class LRUCache {
  size_t cap;
  std::list<std::pair<std::string, json>> lst;
  std::unordered_map<std::string, std::list<std::pair<std::string,json>>::iterator> idx;
public:
  explicit LRUCache(size_t c): cap(c) {}

  json get(const std::string& key) {
    auto it = idx.find(key);
    json r;
    if (it == idx.end()) {
      r["found"] = false;
    } else {
      lst.splice(lst.end(), lst, it->second);
      r["found"] = true;
      r["value"] = it->second->second;
    }
    return r;
  }

  json put(const std::string& key, const json& value) {
    auto it = idx.find(key);
    if (it != idx.end()) {
      it->second->second = value;
      lst.splice(lst.end(), lst, it->second);
    } else {
      lst.emplace_back(key, value);
      idx[key] = std::prev(lst.end());
    }
    std::vector<std::string> evicted;
    while (lst.size() > cap) {
      evicted.push_back(lst.front().first);
      idx.erase(lst.front().first);
      lst.pop_front();
    }
    json r;
    r["evicted"] = evicted;
    return r;
  }

  json resize(long long newcap) {
    if (newcap < 0) { json r; r["error"] = "CAPACITY"; return r; }
    cap = static_cast<size_t>(newcap);
    std::vector<std::string> evicted;
    while (lst.size() > cap) {
      evicted.push_back(lst.front().first);
      idx.erase(lst.front().first);
      lst.pop_front();
    }
    json r;
    r["evicted"] = evicted;
    return r;
  }

  json entries() const {
    json e = json::array();
    for (const auto& p : lst) {
      e.push_back({{"key", p.first}, {"value", p.second}});
    }
    return e;
  }
};

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <stdexcept>
#include <cctype>
#include <iterator>
#include <utility>
#include <cstdio>

struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool boolValue = false;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;
};

JsonValue makeNull() { JsonValue v; v.type = JsonValue::Null; return v; }
JsonValue makeBool(bool b) { JsonValue v; v.type = JsonValue::Bool; v.boolValue = b; return v; }
JsonValue makeNumber(const std::string&

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <utility>
#include <cctype>
#include <cstdio>
#include <stdexcept>

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool boolVal = false;
    std::string strVal;
    std::vector<Json> arrVal;
    std::vector<std::pair<std::string, Json>> objVal;

    static Json makeNull() { Json j; j.type = Type::Null; return j; }
    static Json makeBool(bool b) { Json j; j.type = Type::Bool; j.boolVal = b; return j; }
    static Json makeNumber(const std::string& s) { Json j; j.type = Type::Number; j.strVal = s; return j; }
    static Json makeString(const std::string& s) { Json j; j.type = Type::String; j.strVal = s; return j; }
    static Json makeArray() { Json j; j.type = Type::Array; return j; }
    static Json makeObject() { Json j; j.type = Type::Object; return j; }

    bool isNull() const { return type == Type::Null; }
    bool isBool() const { return type == Type::Bool; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    bool asBool() const { return boolVal; }
    const std::string& asString() const { return strVal; }
    const std::vector<Json>& asArray() const { return arrVal; }
    const std::vector<std::pair<std::string, Json>>& asObject() const { return objVal; }

    const Json* find(const std::string& key) const {
        if (type != Type::Object) return nullptr;
        for (const auto& p : objVal) {
            if (p.first == key) return &p.second;
        }
        return nullptr;
    }

    const Json& operator[](const std::string& key) const {
        const Json* p = find(key);
        if (!p) throw std::runtime_error("Key not found: " + key);
        return *p;
    }

    int asInt() const {
        return static_cast<int>(std::stoll(strVal));
    }
};

class Parser {
public:
    Parser(const std::string& s) : s(s), pos(0) {}
    Json parse() {
        skipWhitespace();
        Json v = parseValue();
        skipWhitespace();
        if (pos != s.size()) throw std::runtime_error("Trailing characters");
        return v;
    }
private:
    const std::string& s;
    size_t pos;

    void skipWhitespace() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) pos++;
    }

    Json parseValue() {
        skipWhitespace();
        if (pos >= s.size()) throw std::runtime_error("Unexpected end");
        char c = s[pos];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't') { expect("true"); return Json::makeBool(true); }
        if (c == 'f') { expect("false"); return Json::makeBool(false); }
        if (c == 'n') { expect("null"); return Json::makeNull(); }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        throw std::runtime_error("Invalid value");
    }

    void expect(const std::string& word) {
        if (s.compare(pos, word.size(), word) != 0) throw std::runtime_error("Expected " + word);
        pos += word.size();
    }

    Json parseString() {
        if (s[pos] != '"') throw std::runtime_error("Expected string");
        pos++;
        std::string out;
        while (pos < s.size() && s[pos] != '"') {
            char c = s[pos];
            if (c == '\\') {
                pos++;
                if (pos >= s.size()) throw std::runtime_error("Bad escape");
                char esc = s[pos];
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (pos + 4 >= s.size()) throw std::runtime_error("Bad unicode escape");
                        std::string hex = s.substr(pos + 1, 4);
                        pos += 4;
                        unsigned int code = std::stoul(hex, nullptr, 16);
                        if (code >= 0xD800 && code <= 0xDBFF) {
                            if (pos + 6 < s.size() && s[pos+1] == '\\' && s[pos+2] == 'u') {
                                std::string hex2 = s.substr(pos + 3, 4);
                                unsigned int code2 = std::stoul(hex2, nullptr, 16);
                                if (code2 >= 0xDC00 && code2 <= 0xDFFF) {
                                    code = 0x10000 + ((code - 0xD800) << 10) + (code2 - 0xDC00);
                                    pos += 6;
                                }
                            }
                        }
                        if (code <= 0x7F) {
                            out += static_cast<char>(code);
                        } else if (code <= 0x7FF) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else if (code <= 0xFFFF) {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xF0 | (code >> 18));
                            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: throw std::runtime_error("Bad escape");
                }
                pos++;
            } else {
                out += c;
                pos++;
            }
        }
        if (pos >= s.size() || s[pos] != '"') throw std::runtime_error("Unterminated string");
        pos++;
        return Json::makeString(out);
    }

    Json parseNumber() {
        size_t start = pos;
        if (s[pos] == '-') pos++;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
        if (pos < s.size() && s[pos] == '.') {
            pos++;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            pos++;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
        }
        std::string num = s.substr(start, pos - start);
        return Json::makeNumber(num);
    }

    Json parseArray() {
        pos++;
        Json j = Json::makeArray();
        skipWhitespace();
        if (pos < s.size() && s[pos] == ']') { pos++; return j; }
        while (true) {
            skipWhitespace();
            j.arrVal.push_back(parseValue());
            skipWhitespace();
            if (pos >= s.size()) throw std::runtime_error("Unterminated array");
            if (s[pos] == ',') { pos++; continue; }
            if (s[pos] == ']') { pos++; break; }
            throw std::runtime_error("Expected , or ]");
        }
        return j;
    }

    Json parseObject() {
        pos++;
        Json j = Json::makeObject();
        skipWhitespace();
        if (pos < s.size() && s[pos] == '}') { pos++; return j; }
        while (true) {
            skipWhitespace();
            if (pos >= s.size() || s[pos] != '"') throw std::runtime_error("Expected string key");
            Json key = parseString();
            skipWhitespace();
            if (pos >= s.size() || s[pos] != ':') throw std::runtime_error("Expected :");
            pos++;
            skipWhitespace();
            Json val = parseValue();
            j.objVal.emplace_back(key.strVal, std::move(val));
            skipWhitespace();
            if (pos >= s.size()) throw std::runtime_error("Unterminated object");
            if (s[pos] == ',') { pos++; continue; }
            if (s[pos] == '}') { pos++; break; }
            throw std::runtime_error("Expected , or }");
        }
        return j;
    }
};

void escapeString(const std::string& str, std::string& out) {
    out += '"';
    for (unsigned char c : str) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

void serialize(const Json& j, std::string& out) {
    switch (j.type) {
        case Json::Type::Null: out += "null"; break;
        case Json::Type::Bool: out += j.boolVal ? "true" : "false"; break;
        case Json::Type::Number: out += j.strVal; break;
        case Json::Type::String: escapeString(j.strVal, out); break;
        case Json::Type::Array: {
            out += '[';
            for (size_t i = 0; i < j.arrVal.size(); ++i) {
                if (i > 0) out += ',';
                serialize(j.arrVal[i], out);
            }
            out += ']';
            break;
        }
        case Json::Type::Object: {
            out += '{';
            for (size_t i = 0; i < j.objVal.size(); ++i) {
                if (i > 0) out += ',';
                escapeString(j.objVal[i].first, out);
                out += ':';
                serialize(j.objVal[i].second, out);
            }
            out += '}';
            break;
        }
    }
}

class LRUCache {
    struct Entry {
        std::string key;
        Json value;
    };
    int capacity;
    std::list<Entry> items; // front = LRU, back = MRU
    std::unordered_map<std::string, std::list<Entry>::iterator> map;

public:
    LRUCache(int cap) : capacity(cap) {}

    bool get(const std::string& key, Json& outValue) {
        auto it = map.find(key);
        if (it == map.end()) return false;
        items.splice(items.end(), items, it->second);
        outValue = it->second->value;
        return true;
    }

    std::vector<std::string> put(const std::string& key, const Json& value) {
        std::vector<std::string> evicted;
        auto it = map.find(key);
        if (it != map.end()) {
            it->second->value = value;
            items.splice(items.end(), items, it->second);
        } else {
            items.push_back({key, value});
            map[key] = std::prev(items.end());
        }
        while (capacity >= 0 && static_cast<int>(items.size()) > capacity) {
            evicted.push_back(items.front().key);
            map.erase(items.front().key);
            items.pop_front();
        }
        return evicted;
    }

    std::vector<std::

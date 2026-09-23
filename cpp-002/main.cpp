#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <stdexcept>
#include <cctype>

struct Json {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    std::string str;
    std::string num;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;

    static Json makeNull() { Json j; j.type = Null; return j; }
    static Json makeBool(bool v) { Json j; j.type = Bool; j.b = v; return j; }
    static Json makeNumber(const std::string& s) { Json j; j.type = Number; j.num = s; return j; }
    static Json makeString(const std::string& s) { Json j; j.type = String; j.str = s; return j; }
    static Json makeArray() { Json j; j.type = Array; return j; }
    static Json makeObject() { Json j; j.type = Object; return j; }
};

// Parser
class Parser {
    const std::string& s;
    size_t i = 0;
public:
    Parser(const std::string& str) : s(str) {}

    void skipWs() {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    }

    Json parse() {
        skipWs();
        Json v = parseValue();
        skipWs();
        if (i != s.size()) throw std::runtime_error("trailing");
        return v;
    }

private:
    int hexVal(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::runtime_error("bad hex");
    }

    Json parseValue() {
        skipWs();
        if (i >= s.size()) throw std::runtime_error("eof");
        char c = s[i];
        if (c == 'n') {
            if (s.compare(i, 4, "null") == 0) { i += 4; return Json::makeNull(); }
            throw std::runtime_error("bad null");
        }
        if (c == 't') {
            if (s.compare(i, 4, "true") == 0) { i += 4; return Json::makeBool(true); }
            throw std::runtime_error("bad true");
        }
        if (c == 'f') {
            if (s.compare(i, 5, "false") == 0) { i += 5; return Json::makeBool(false); }
            throw std::runtime_error("bad false");
        }
        if (c == '"') return parseString();
        if (c == '[') return parseArray();
        if (c == '{') return parseObject();
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        throw std::runtime_error("bad value");
    }

    Json parseString() {
        // assume s[i] == '"'
        i++;
        std::string out;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return Json::makeString(out);
            if (c == '\\') {
                if (i >= s.size()) throw std::runtime_error("bad escape");
                char esc = s[i++];
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
                        if (i + 3 >= s.size()) throw std::runtime_error("bad unicode");
                        unsigned int cp = 0;
                        for (int k = 0; k < 4; ++k) {
                            cp = (cp << 4) | hexVal(s[i++]);
                        }
                        // handle surrogate pair
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (i + 1 < s.size() && s[i] == '\\' && s[i+1] == 'u') {
                                i += 2;
                                if (i + 3 >= s.size()) throw std::runtime_error("bad unicode");
                                unsigned int cp2 = 0;
                                for (int k = 0; k < 4; ++k) {
                                    cp2 = (cp2 << 4) | hexVal(s[i++]);
                                }
                                if (cp2 >= 0xDC00 && cp2 <= 0xDFFF) {
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (cp2 - 0xDC00);
                                } else {
                                    throw std::runtime_error("bad surrogate");
                                }
                            }
                        }
                        // encode UTF-8
                        if (cp <= 0x7F) {
                            out += static_cast<char>(cp);
                        } else if (cp <= 0x7FF) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else if (cp <= 0xFFFF) {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xF0 | (cp >> 18));
                            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: throw std::runtime_error("bad escape");
                }
            } else {
                if (static_cast<unsigned char>(c) < 0x20) throw std::runtime_error("control char");
                out += c;
            }
        }
        throw std::runtime_error("unterminated string");
    }

    Json parseNumber() {
        size_t start = i;
        if (s[i] == '-') i++;
        if (i >= s.size()) throw std::runtime_error("bad number");
        if (s[i] == '0') {
            i++;
        } else if (s[i] >= '1' && s[i] <= '9') {
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
        } else {
            throw std::runtime_error("bad number");
        }
        if (i < s.size() && s[i] == '.') {
            i++;
            if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) throw std::runtime_error("bad number");
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            i++;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) i++;
            if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) throw std::runtime_error("bad number");
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
        }
        return Json::makeNumber(s.substr(start, i - start));
    }

    Json parseArray() {
        i++; // '['
        std::vector<Json> arr;
        skipWs();
        if (i < s.size() && s[i] == ']') {
            i++;
            Json j = Json::makeArray();
            j.arr = std::move(arr);
            return j;
        }
        while (true) {
            arr.push_back(parseValue());
            skipWs();
            if (i >= s.size()) throw std::runtime_error("bad array");
            if (s[i] == ',') {
                i++;
                skipWs();
                continue;
            }
            if (s[i] == ']') {
                i++;
                break;
            }
            throw std::runtime_error("bad array");
        }
        Json j = Json::makeArray();
        j.arr = std::move(arr);
        return j;
    }

    Json parseObject() {
        i++; // '{'
        std::vector<std::pair<std::string, Json>> obj;
        skipWs();
        if (i < s.size() && s[i] == '}') {
            i++;
            Json j = Json::makeObject();
            j.obj = std::move(obj);
            return j;
        }
        while (true) {
            skipWs();
            if (i >= s.size() || s[i] != '"') throw std::runtime_error("bad object key");
            Json key = parseString();
            skipWs();
            if (i >= s.size() || s[i] != ':') throw std::runtime_error("bad object colon");
            i++;
            Json val = parseValue();
            obj.emplace_back(key.str, std::move(val));
            skipWs();
            if (i >= s.size()) throw std::runtime_error("bad object");
            if (s[i] == ',') {
                i++;
                continue;
            }
            if (s[i] == '}') {
                i++;
                break;
            }
            throw std::runtime_error("bad object");
        }
        Json j = Json::makeObject();
        j.obj = std::move(obj);
        return j;
    }
};

// Serialization
void escapeString(const std::string& in, std::string& out) {
    out += '"';
    for (char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[(c >> 4) & 0xF];
                    out += hex[c & 0xF];
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

void serialize(const Json& j, std::string& out) {
    switch (j.type) {
        case Json::Null: out += "null"; break;
        case Json::Bool: out += j.b ? "true" : "false"; break;
        case Json::Number: out += j.num; break;
        case Json::String: escapeString(j.str, out); break;
        case Json::Array: {
            out += '[';
            for (size_t i = 0; i < j.arr.size(); ++i) {
                if (i) out += ',';
                serialize(j.arr[i], out);
            }
            out += ']';
            break;
        }
        case Json::Object: {
            out += '{';
            for (size_t i = 0; i < j.obj.size(); ++i) {
                if (i) out += ',';
                escapeString(j.obj[i].first, out);
                out += ':';
                serialize(j.obj[i].second, out);
            }
            out += '}';
            break;
        }
    }
}

const Json* getField(const Json& obj, const std::string& name) {
    if (obj.type != Json::Object) return nullptr;
    for (const auto& p : obj.obj) {
        if (p.first == name) return &p.second;
    }
    return nullptr;
}

struct Entry {
    std::string key;
    Json value;
};

class LRUCache {
    int capacity;
    std::list<Entry> items; // front = MRU, back = LRU
    std::unordered_map<std::string, std::list<Entry>::iterator> map;
public:
    LRUCache(int cap) : capacity(cap) {}

    Json get(const std::string& key) {
        auto it = map.find(key);
        if (it == map.end()) {
            Json res = Json::makeObject();
            res.obj.emplace_back("found", Json::makeBool(false));
            return res;
        }
        items.splice(items.begin(), items, it->second);
        Json res = Json::makeObject();
        res.obj.emplace_back("found", Json::makeBool(true));
        res.obj.emplace_back("value", it->second->value);
        return res;
    }

    Json put(const std::string& key, const Json& value) {
        std::vector<std::string> evicted;
        auto it = map.find(key);
        if (it != map.end()) {
            it->second->value = value;
            items.splice(items.begin(), items, it->second);
        } else {
            items.push_front({key, value});
            map[key] = items.begin();
            if (capacity == 0) {
                evicted.push_back(key);
                map.erase(key);
                items.pop_front();
            } else if ((int)items.size() > capacity) {
                auto last = items.end();
                --last;
                evicted.push_back(last->key);
                map.erase(last->key);
                items.pop_back();
            }
        }
        Json res = Json::makeObject();
        Json arr = Json::makeArray();
        for (const auto& k : evicted) {
            arr.arr.push_back(Json::makeString(k));
        }
        res.obj.emplace_back("evicted", std::move(arr));
        return res;
    }

    Json resize(int newCap) {
        if (newCap < 0) {
            Json res = Json::makeObject();
            res.obj.emplace_back("error", Json::makeString("CAPACITY"));
            return res;
        }
        capacity = newCap;
        std::vector<std::string> evicted;
        while ((int)items.size() > capacity) {
            auto last = items.end();
            --last;
            evicted.push_back(last->key);
            map.erase(last->key);
            items.pop_back();
        }
        Json res = Json::makeObject();
        Json arr = Json::makeArray();
        for (const auto& k : evicted) {
            arr.arr.push_back(Json::makeString(k));
        }
        res.obj.emplace_back("evicted", std::move(arr));
        return res;
    }

    Json getEntries() const {
        Json arr = Json::makeArray();
        for (auto it = items.rbegin(); it != items.rend(); ++it) {
            Json entry = Json::makeObject();
            entry.obj.emplace_back("key", Json::makeString(it->key));
            entry.obj.emplace_back("value", it->value);
            arr.arr.push_back(std::move(entry));
        }
        return arr;
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "INVALID_JSON\n";
            continue;
        }
        try {
            Parser parser(line);
            Json req = parser.parse();
            if (req.type != Json::Object) throw std::runtime_error("not object");
            const Json* capField = getField(req, "capacity");
            if (!capField || capField->type != Json::Number) throw std::runtime_error("bad capacity");
            int capacity = std::stoi(capField->num);
            if (capacity < 0) {
                Json resp = Json::makeObject();
                resp.obj.emplace_back("error", Json::makeString("CAPACITY"));
                std::string out;
                serialize(resp, out);
                std::cout << out << '\n';
                continue;
            }
            const Json* opsField = getField(req, "ops");
            if (!opsField || opsField->type != Json::Array) throw std::runtime_error("bad ops");
            LRUCache cache(capacity);
            Json results = Json::makeArray();
            for (const auto& op : opsField->arr) {
                if (op.type != Json::Object) throw std::runtime_error("op not object");
                const Json* opNameField = getField(op, "op");
                if (!opNameField || opNameField->type != Json::String) throw std::runtime_error("bad op");
                const std::string& opName = opNameField->str;
                if (opName == "get") {
                    const Json* keyField = getField(op, "key");
                    if (!keyField || keyField->type != Json::String) throw std::runtime_error("bad key");
                    Json res = cache.get(keyField->str);
                    results.arr.push_back(std::move(res));
                } else if (opName == "put") {
                    const Json* keyField = getField(op, "key");
                    if (!keyField || keyField->type != Json::String) throw std::runtime_error("bad key");
                    const Json* valueField = getField(op, "value");
                    if (!valueField) throw std::runtime_error("missing value");
                    Json res = cache.put(keyField->str, *valueField);
                    results.arr.push_back(std::move(res));
                } else if (opName == "resize") {
                    const Json* capFieldOp = getField(op, "capacity");
                    if (!capFieldOp || capFieldOp->type != Json::Number) throw std::runtime_error("bad capacity");
                    int newCap = std::stoi(capFieldOp->num);
                    Json res = cache.resize(newCap);
                    results.arr.push_back(std::move(res));
                } else {
                    throw std::runtime_error("unknown op");
                }
            }
            Json resp = Json::makeObject();
            resp.obj.emplace_back("results", std::move(results));
            resp.obj.emplace_back("entries", cache.getEntries());
            std::string out;
            serialize(resp, out);
            std::cout << out << '\n';
        } catch (...) {
            std::cout << "INVALID_JSON\n";
        }
    }
    return 0;
}

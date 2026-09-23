#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <iterator>

struct Value {
    enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT } type;
    bool b = false;
    std::string s;
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value>> obj;
    Value() : type(NUL) {}
    static Value makeNull() { return Value(); }
    static Value makeBool(bool v) { Value x; x.type = BOOL; x.b = v; return x; }
    static Value makeNumber(std::string v) { Value x; x.type = NUMBER; x.s = std::move(v); return x; }
    static Value makeString(std::string v) { Value x; x.type = STRING; x.s = std::move(v); return x; }
    static Value makeArray(std::vector<Value> v) { Value x; x.type = ARRAY; x.arr = std::move(v); return x; }
    static Value makeObject(std::vector<std::pair<std::string, Value>> v) { Value x; x.type = OBJECT; x.obj = std::move(v); return x; }
};

class JsonParser {
    const std::string& in;
    size_t pos = 0;
public:
    JsonParser(const std::string& input) : in(input) {}
    Value parse() {
        skipws();
        Value v = parseValue();
        skipws();
        if (pos != in.size()) throw std::runtime_error("trailing");
        return v;
    }
private:
    void skipws() {
        while (pos < in.size() && (in[pos] == ' ' || in[pos] == '\t' || in[pos] == '\n' || in[pos] == '\r')) pos++;
    }
    char peek() { if (pos >= in.size()) throw std::runtime_error("eof"); return in[pos]; }
    char get() { if (pos >= in.size()) throw std::runtime_error("eof"); return in[pos++]; }
    void expect(char c) { if (get() != c) throw std::runtime_error("expected"); }
    Value parseValue() {
        skipws();
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't') { parseLiteral("true"); return Value::makeBool(true); }
        if (c == 'f') { parseLiteral("false"); return Value::makeBool(false); }
        if (c == 'n') { parseLiteral("null"); return Value::makeNull(); }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        throw std::runtime_error("invalid value");
    }
    void parseLiteral(const char* lit) {
        for (const char* p = lit; *p; ++p) {
            if (get() != *p) throw std::runtime_error("literal");
        }
    }
    Value parseNumber() {
        size_t start = pos;
        if (peek() == '-') pos++;
        if (pos >= in.size() || in[pos] < '0' || in[pos] > '9') throw std::runtime_error("number");
        while (pos < in.size() && in[pos] >= '0' && in[pos] <= '9') pos++;
        if (pos < in.size() && in[pos] == '.') {
            pos++;
            if (pos >= in.size() || in[pos] < '0' || in[pos] > '9') throw std::runtime_error("number");
            while (pos < in.size() && in[pos] >= '0' && in[pos] <= '9') pos++;
        }
        if (pos < in.size() && (in[pos] == 'e' || in[pos] == 'E')) {
            pos++;
            if (pos < in.size() && (in[pos] == '+' || in[pos] == '-')) pos++;
            if (pos >= in.size() || in[pos] < '0' || in[pos] > '9') throw std::runtime_error("number");
            while (pos < in.size() && in[pos] >= '0' && in[pos] <= '9') pos++;
        }
        return Value::makeNumber(in.substr(start, pos - start));
    }
    Value parseString() {
        expect('"');
        std::string out;
        while (true) {
            if (pos >= in.size()) throw std::runtime_error("string eof");
            char c = in[pos++];
            if (c == '"') break;
            if (c == '\\') {
                if (pos >= in.size()) throw std::runtime_error("escape eof");
                char e = in[pos++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        unsigned int cp = parseHex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (pos + 1 < in.size() && in[pos] == '\\' && in[pos+1] == 'u') {
                                pos += 2;
                                unsigned int low = parseHex4();
                                if (low >= 0xDC00 && low <= 0xDFFF) {
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                } else {
                                    throw std::runtime_error("bad surrogate");
                                }
                            } else {
                                throw std::runtime_error("bad surrogate");
                            }
                        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                            throw std::runtime_error("bad surrogate");
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: throw std::runtime_error("bad escape");
                }
            } else {
                if ((unsigned char)c < 0x20) throw std::runtime_error("control char");
                out += c;
            }
        }
        return Value::makeString(std::move(out));
    }
    unsigned int parseHex4() {
        unsigned int v = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos >= in.size()) throw std::runtime_error("hex eof");
            char c = in[pos++];
            v <<= 4;
            if (c >= '0' && c <= '9') v += c - '0';
            else if (c >= 'a' && c <= 'f') v += c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v += c - 'A' + 10;
            else throw std::runtime_error("hex");
        }
        return v;
    }
    void appendUtf8(std::string& out, unsigned int cp) {
        if (cp <= 0x7F) out += (char)cp;
        else if (cp <= 0x7FF) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    Value parseArray() {
        expect('[');
        std::vector<Value> arr;
        skipws();
        if (peek() == ']') { pos++; return Value::makeArray(std::move(arr)); }
        while (true) {
            arr.push_back(parseValue());
            skipws();
            char c = get();
            if (c == ']') break;
            if (c != ',') throw std::runtime_error("array sep");
            skipws();
        }
        return Value::makeArray(std::move(arr));
    }
    Value parseObject() {
        expect('{');
        std::vector<std::pair<std::string, Value>> obj;
        skipws();
        if (peek() == '}') { pos++; return Value::makeObject(std::move(obj)); }
        while (true) {
            skipws();
            if (peek() != '"') throw std::runtime_error("key");
            std::string key = parseString().s;
            skipws();
            expect(':');
            Value val = parseValue();
            obj.emplace_back(std::move(key), std::move(val));
            skipws();
            char c = get();
            if (c == '}') break;
            if (c != ',') throw std::runtime_error("object sep");
            skipws();
        }
        return Value::makeObject(std::move(obj));
    }
};

void writeString(const std::string& s, std::string& out) {
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

void writeJson(const Value& v, std::string& out) {
    switch (v.type) {
        case Value::NUL: out += "null"; break;
        case Value::BOOL: out += v.b ? "true" : "false"; break;
        case Value::NUMBER: out += v.s; break;
        case Value::STRING: writeString(v.s, out); break;
        case Value::ARRAY: {
            out += '[';
            for (size_t i = 0; i < v.arr.size(); ++i) {
                if (i) out += ',';
                writeJson(v.arr[i], out);
            }
            out += ']';
            break;
        }
        case Value::OBJECT: {
            out += '{';
            for (size_t i = 0; i < v.obj.size(); ++i) {
                if (i) out += ',';
                writeString(v.obj[i].first, out);
                out += ':';
                writeJson(v.obj[i].second, out);
            }
            out += '}';
            break;
        }
    }
}

const Value* getField(const Value& obj, const std::string& key) {
    for (const auto& p : obj.obj) {
        if (p.first == key) return &p.second;
    }
    return nullptr;
}

Value makeError(const std::string& msg) {
    std::vector<std::pair<std::string, Value>> obj;
    obj.emplace_back("error", Value::makeString(msg));
    return Value::makeObject(std::move(obj));
}

struct Cache {
    int64_t cap;
    struct Entry { std::string key; Value val; };
    std::list<Entry> items;
    std::unordered_map<std::string, std::list<Entry>::iterator> map;
    Cache(int64_t c) : cap(c) {}
    Value get(const std::string& key) {
        auto it = map.find(key);
        if (it == map.end()) {
            std::vector<std::pair<std::string, Value>> obj;
            obj.emplace_back("found", Value::makeBool(false));
            return Value::makeObject(std::move(obj));
        }
        items.splice(items.end(), items, it->second);
        it->second = std::prev(items.end());
        std::vector<std::pair<std::string, Value>> obj;
        obj.emplace_back("found", Value::makeBool(true));
        obj.emplace_back("value", it->second->val);
        return Value::makeObject(std::move(obj));
    }
    Value put(const std::string& key, Value val) {
        std::vector<std::string> evicted;
        auto it = map.find(key);
        if (it != map.end()) {
            it->second->val = std::move(val);
            items.splice(items.end(), items, it->second);
            it->second = std::prev(items.end());
            return makeEvicted(evicted);
        }
        if (cap == 0) {
            evicted.push_back(key);
            return makeEvicted(evicted);
        }
        items.push_back({key, std::move(val)});
        auto lit = std::prev(items.end());
        map[key] = lit;
        while ((int64_t)items.size() > cap && !items.empty()) {
            evicted.push_back(items.front().key);
            map.erase(items.front().key);
            items.pop_front();
        }
        return makeEvicted(evicted);
    }
    Value resize(int64_t newCap) {
        if (newCap < 0) {
            return makeError("CAPACITY");
        }
        cap = newCap;
        std::vector<std::string> evicted;
        while ((int64_t)items.size() > cap && !items.empty()) {
            evicted.push_back(items.front().key);
            map.erase(items.front().key);
            items.pop_front();
        }
        return makeEvicted(evicted);
    }
    Value makeEvicted(const std::vector<std::string>& evicted) {
        std::vector<Value> arr;
        for (const auto& k : evicted) arr.push_back(Value::makeString(k));
        std::vector<std::pair<std::string, Value>> obj;
        obj.emplace_back("evicted", Value::makeArray(std::move(arr)));
        return Value::makeObject(std::move(obj));
    }
    Value getEntries() {
        std::vector<Value> arr;
        for (const auto& e : items) {
            std::vector<std::pair<std::string, Value>> obj;
            obj.emplace_back("key", Value::makeString(e.key));
            obj.emplace_back("value", e.val);
            arr.push_back(Value::makeObject(std::move(obj)));
        }
        return Value::makeArray(std::move(arr));
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string line;
    while (std::getline(std::cin, line)) {
        try {
            JsonParser parser(line);
            Value root = parser.parse();
            if (root.type != Value::OBJECT) {
                std::cout << "INVALID_JSON\n";
                continue;
            }
            const Value* capField = getField(root, "capacity");
            const Value* opsField = getField(root, "ops");
            if (!capField || !opsField || capField->type != Value::NUMBER || opsField->type != Value::ARRAY) {
                std::cout << "INVALID_JSON\n";
                continue;
            }
            int64_t initialCap = std::stoll(capField->s);
            if (initialCap < 0) {
                std::string out;
                Value err = makeError("CAPACITY");
                writeJson(err, out);
                std::cout << out << '\n';
                continue;
            }
            Cache cache(initialCap);
            std::vector<Value> results;
            for (const Value& op : opsField->arr) {
                if (op.type != Value::OBJECT) throw std::runtime_error("op not obj");
                const Value* opName = getField(op, "op");
                if (!opName || opName->type != Value::STRING) throw std::runtime_error("op name");
                const std::string& name = opName->s;
                if (name == "get") {
                    const Value* keyField = getField(op, "key");
                    if (!keyField || keyField->type != Value::STRING) throw std::runtime_error("get key");
                    results.push_back(cache.get(keyField->s));
                } else if (name == "put") {
                    const Value* keyField = getField(op, "key");
                    const Value* valField = getField(op, "value");
                    if (!keyField || keyField->type != Value::STRING || !valField) throw std::runtime_error("put fields");
                    results.push_back(cache.put(keyField->s, *valField));
                } else if (name == "resize") {
                    const Value* capField2 = getField(op, "capacity");
                    if (!capField2 || capField2->type != Value::NUMBER) throw std::runtime_error("resize cap");
                    int64_t newCap = std::stoll(capField2->s);
                    results.push_back(cache.resize(newCap));
                } else {
                    throw std::runtime_error("unknown op");
                }
            }
            std::vector<std::pair<std::string, Value>> outObj;
            outObj.emplace_back("results", Value::makeArray(std::move(results)));
            outObj.emplace_back("entries", cache.getEntries());
            Value rootOut = Value::makeObject(std::move(outObj));
            std::string out;
            writeJson(rootOut, out);
            std::cout << out << '\n';
        } catch (...) {
            std::cout << "INVALID_JSON\n";
        }
    }
    return 0;
}

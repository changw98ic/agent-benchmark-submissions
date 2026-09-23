#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

struct Value {
    enum Type { TNull, TBool, TInt, TReal, TStr, TArr, TObj } type = TNull;
    bool b = false;
    long long i = 0;
    double d = 0.0;
    std::string s;
    std::vector<Value> a;
    std::map<std::string, Value> o;
};

static void appendUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80u) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800u) {
        out += static_cast<char>(0xC0u | (cp >> 6));
        out += static_cast<char>(0x80u | (cp & 0x3Fu));
    } else if (cp < 0x10000u) {
        out += static_cast<char>(0xE0u | (cp >> 12));
        out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (cp & 0x3Fu));
    } else {
        out += static_cast<char>(0xF0u | (cp >> 18));
        out += static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
        out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (cp & 0x3Fu));
    }
}

class Parser {
public:
    std::string src;
    size_t pos = 0;
    bool ok = true;

    void skipWs() {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) ++pos;
    }
    char peek() {
        skipWs();
        return pos < src.size() ? src[pos] : '\0';
    }
    bool match(char c) {
        if (peek() == c) { ++pos; return true; }
        return false;
    }
    void expectWord(const char* w) {
        size_t n = std::strlen(w);
        if (src.compare(pos, n, w) == 0) pos += n; else ok = false;
    }
    unsigned parseHex4() {
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            if (pos >= src.size()) { ok = false; return 0; }
            char c = src[pos++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else { ok = false; return 0; }
        }
        return v;
    }
    std::string parseString() {
        std::string out;
        if (!match('"')) { ok = false; return out; }
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') return out;
            if (c == '\\') {
                if (pos >= src.size()) { ok = false; return out; }
                char e = src[pos++];
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
                        unsigned cp = parseHex4();
                        if (!ok) return out;
                        if (cp >= 0xD800u && cp <= 0xDBFFu && pos + 1 < src.size() &&
                            src[pos] == '\\' && src[pos + 1] == 'u') {
                            pos += 2;
                            unsigned low = parseHex4();
                            if (!ok) return out;
                            if (low >= 0xDC00u && low <= 0xDFFFu) {
                                cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
                            }
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: ok = false; return out;
                }
            } else if (static_cast<unsigned char>(c) < 0x20u) {
                ok = false;
                return out;
            } else {
                out += c;
            }
        }
        ok = false;
        return out;
    }
    Value parseNumber() {
        Value v;
        v.type = Value::TInt;
        size_t start = pos;
        bool isReal = false;
        if (pos < src.size() && src[pos] == '-') ++pos;
        while (pos < src.size()) {
            char c = src[pos];
            if (c >= '0' && c <= '9') { ++pos; }
            else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                if (c == '.' || c == 'e' || c == 'E') isReal = true;
                ++pos;
            } else break;
        }
        std::string tok = src.substr(start, pos - start);
        if (tok.empty()) { ok = false; return v; }
        if (isReal) {
            v.type = Value::TReal;
            v.d = std::strtod(tok.c_str(), nullptr);
            v.i = static_cast<long long>(v.d);
        } else {
            v.i = std::strtoll(tok.c_str(), nullptr, 10);
            v.d = static_cast<double>(v.i);
        }
        return v;
    }
    Value parseArray() {
        Value v;
        v.type = Value::TArr;
        if (!match('[')) { ok = false; return v; }
        if (match(']')) return v;
        while (true) {
            Value item = parseValue();
            if (!ok) return v;
            v.a.push_back(std::move(item));
            if (match(',')) continue;
            if (match(']')) break;
            ok = false;
            return v;
        }
        return v;
    }
    Value parseObject() {
        Value v;
        v.type = Value::TObj;
        if (!match('{')) { ok = false; return v; }
        if (match('}')) return v;
        while (true) {
            if (peek() != '"') { ok = false; return v; }
            std::string key = parseString();
            if (!ok) return v;
            if (!match(':')) { ok = false; return v; }
            Value item = parseValue();
            if (!ok) return v;
            v.o[key] = std::move(item);
            if (match(',')) continue;
            if (match('}')) break;
            ok = false;
            return v;
        }
        return v;
    }
    Value parseValue() {
        Value v;
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') { v.type = Value::TStr; v.s = parseString(); return v; }
        if (c == 't') { expectWord("true"); v.type = Value::TBool; v.b = true; return v; }
        if (c == 'f') { expectWord("false"); v.type = Value::TBool; v.b = false; return v; }
        if (c == 'n') { expectWord("null"); v.type = Value::TNull; return v; }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        ok = false;
        return v;
    }
};

static bool asInt(const Value& v, long long& out) {
    if (v.type == Value::TInt) { out = v.i; return true; }
    if (v.type == Value::TReal) { out = static_cast<long long>(v.d); return true; }
    return false;
}

static void emitError(const char* code) {
    std::cout << "{\n  \"error\": \"" << code << "\"\n}\n";
}

static void emitResult(const std::vector<std::pair<long long, long long> >& iv, long long length) {
    std::cout << "{\n  \"intervals\": ";
    if (iv.empty()) {
        std::cout << "[]";
    } else {
        std::cout << "[\n";
        for (size_t k = 0; k < iv.size(); ++k) {
            std::cout << "    [\n      " << iv[k].first << ",\n      " << iv[k].second << "\n    ]";
            if (k + 1 < iv.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]";
    }
    std::cout << ",\n  \"length\": " << length << "\n}\n";
}

static void handle(const std::string& line) {
    Parser p;
    p.src = line;
    Value root = p.parseValue();
    if (!p.ok || root.type != Value::TObj) { emitError("INVALID_REQUEST"); return; }
    auto itIv = root.o.find("intervals");
    if (itIv == root.o.end() || itIv->second.type != Value::TArr) { emitError("INVALID_REQUEST"); return; }
    std::vector<std::pair<long long, long long> > raw;
    for (size_t k = 0; k < itIv->second.a.size(); ++k) {
        const Value& e = itIv->second.a[k];
        if (e.type != Value::TArr || e.a.size() < 2) { emitError("INVALID_REQUEST"); return; }
        long long s = 0, t = 0;
        if (!asInt(e.a[0], s) || !asInt(e.a[1], t)) { emitError("INVALID_REQUEST"); return; }
        if (s > t) { emitError("INVALID_INTERVAL"); return; }
        if (s < t) raw.push_back(std::make_pair(s, t));
    }
    auto itClip = root.o.find("clip");
    if (itClip != root.o.end() && itClip->second.type == Value::TArr && itClip->second.a.size() >= 2) {
        long long lo = 0, hi = 0;
        if (!asInt(itClip->second.a[0], lo) || !asInt(itClip->second.a[1], hi)) { emitError("INVALID_CLIP"); return; }
        if (lo > hi) { emitError("INVALID_CLIP"); return; }
        std::vector<std::pair<long long, long long> > kept;
        for (size_t k = 0; k < raw.size(); ++k) {
            long long ns = std::max(raw[k].first, lo);
            long long ne = std::min(raw[k].second, hi);
            if (ns < ne) kept.push_back(std::make_pair(ns, ne));
        }
        raw.swap(kept);
    }
    std::sort(raw.begin(), raw.end());
    std::vector<std::pair<long long, long long> > merged;
    for (size_t k = 0; k < raw.size(); ++k) {
        if (merged.empty() || merged.back().second < raw[k].first) {
            merged.push_back(raw[k]);
        } else if (raw[k].second > merged.back().second) {
            merged.back().second = raw[k].second;
        }
    }
    long long length = 0;
    for (size_t k = 0; k < merged.size(); ++k) length += merged[k].second - merged[k].first;
    emitResult(merged, length);
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string line;
    while (std::getline(std::cin, line)) {
        bool blank = true;
        for (size_t k = 0; k < line.size(); ++k) {
            if (!std::isspace(static_cast<unsigned char>(line[k]))) { blank = false; break; }
        }
        if (blank) continue;
        handle(line);
        std::cout.flush();
    }
    return 0;
}

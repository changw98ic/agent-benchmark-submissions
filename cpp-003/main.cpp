#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <limits>

struct Json {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type;
    bool boolVal = false;
    long long numVal = 0;
    std::string strVal;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;
    Json() : type(NUL) {}
};

class Parser {
    const std::string& s;
    size_t pos;
public:
    Parser(const std::string& str) : s(str), pos(0) {}
    Json parse() {
        skipWs();
        Json v = parseValue();
        skipWs();
        if (pos != s.size()) throw std::runtime_error("trailing");
        return v;
    }
private:
    void skipWs() {
        while (pos < s.size() && (s[pos]==' '||s[pos]=='\t'||s[pos]=='\n'||s[pos]=='\r')) ++pos;
    }
    char peek() {
        if (pos >= s.size()) throw std::runtime_error("eof");
        return s[pos];
    }
    char get() {
        if (pos >= s.size()) throw std::runtime_error("eof");
        return s[pos++];
    }
    void expect(char c) {
        if (get() != c) throw std::runtime_error("expected");
    }
    Json parseValue() {
        skipWs();
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        throw std::runtime_error("invalid value");
    }
    Json parseObject() {
        Json j; j.type = Json::OBJ;
        expect('{');
        skipWs();
        if (peek() == '}') { get(); return j; }
        while (true) {
            skipWs();
            if (peek() != '"') throw std::runtime_error("expected key");
            std::string key = parseString().strVal;
            skipWs();
            expect(':');
            skipWs();
            Json val = parseValue();
            j.obj.emplace_back(std::move(key), std::move(val));
            skipWs();
            char c = get();
            if (c == '}') break;
            if (c != ',') throw std::runtime_error("expected , or }");
        }
        return j;
    }
    Json parseArray() {
        Json j; j.type = Json::ARR;
        expect('[');
        skipWs();
        if (peek() == ']') { get(); return j; }
        while (true) {
            skipWs();
            Json val = parseValue();
            j.arr.push_back(std::move(val));
            skipWs();
            char c = get();
            if (c == ']') break;
            if (c != ',') throw std::runtime_error("expected , or ]");
        }
        return j;
    }
    Json parseString() {
        Json j; j.type = Json::STR;
        expect('"');
        while (true) {
            char c = get();
            if (c == '"') break;
            if (c == '\\') {
                char e = get();
                switch (e) {
                    case '"': j.strVal += '"'; break;
                    case '\\': j.strVal += '\\'; break;
                    case '/': j.strVal += '/'; break;
                    case 'b': j.strVal += '\b'; break;
                    case 'f': j.strVal += '\f'; break;
                    case 'n': j.strVal += '\n'; break;
                    case 'r': j.strVal += '\r'; break;
                    case 't': j.strVal += '\t'; break;
                    case 'u': {
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = get();
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= (h - '0');
                            else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
                            else throw std::runtime_error("bad unicode");
                        }
                        if (code < 0x80) j.strVal += static_cast<char>(code);
                        else if (code < 0x800) {
                            j.strVal += static_cast<char>(0xC0 | (code >> 6));
                            j.strVal += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            j.strVal += static_cast<char>(0xE0 | (code >> 12));
                            j.strVal += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            j.strVal += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: throw std::runtime_error("bad escape");
                }
            } else {
                j.strVal += c;
            }
        }
        return j;
    }
    Json parseNumber() {
        Json j; j.type = Json::NUM;
        size_t start = pos;
        if (peek() == '-') get();
        if (peek() == '0') {
            get();
            if (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
                throw std::runtime_error("leading zero");
            }
        } else if (peek() >= '1' && peek() <= '9') {
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        } else {
            throw std::runtime_error("bad number");
        }
        if (pos < s.size() && (s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E')) {
            throw std::runtime_error("non-integer");
        }
        std::string numStr = s.substr(start, pos - start);
        try {
            j.numVal = std::stoll(numStr);
        } catch (...) {
            throw std::runtime_error("number out of range");
        }
        return j;
    }
    Json parseBool() {
        Json j; j.type = Json::BOOL;
        if (s.compare(pos, 4, "true") == 0) { pos += 4; j.boolVal = true; }
        else if (s.compare(pos, 5, "false") == 0) { pos += 5; j.boolVal = false; }
        else throw std::runtime_error("bad bool");
        return j;
    }
    Json parseNull() {
        Json j; j.type = Json::NUL;
        if (s.compare(pos, 4, "null") == 0) { pos += 4; }
        else throw std::runtime_error("bad null");
        return j;
    }
};

void processLine(const std::string& line) {
    bool allWs = true;
    for (char c : line) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            allWs = false;
            break;
        }
    }
    if (allWs) {
        std::cout << "INVALID_JSON\n";
        return;
    }
    try {
        Parser p(line);
        Json root = p.parse();
        if (root.type != Json::OBJ) throw std::runtime_error("root not object");
        const Json* intervalsJson = nullptr;
        const Json* clipJson = nullptr;
        for (const auto& [key, val] : root.obj) {
            if (key == "intervals") intervalsJson = &val;
            else if (key == "clip") clipJson = &val;
        }
        if (!intervalsJson || intervalsJson->type != Json::ARR) throw std::runtime_error("missing intervals");
        struct Interval { long long start, end; };
        std::vector<Interval> intervals;
        for (const auto& item : intervalsJson->arr) {
            if (item.type != Json::ARR || item.arr.size() != 2) throw std::runtime_error("bad interval");
            const Json& a = item.arr[0];
            const Json& b = item.arr[1];
            if (a.type != Json::NUM || b.type != Json::NUM) throw std::runtime_error("bad interval");
            long long start = a.numVal, end = b.numVal;
            if (start > end) {
                std::cout << "INVALID_INTERVAL\n";
                return;
            }
            if (start == end) continue;
            intervals.push_back({start, end});
        }
        long long lo = std::numeric_limits<long long>::min();
        long long hi = std::numeric_limits<long long>::max();
        bool hasClip = false;
        if (clipJson) {
            if (clipJson->type != Json::ARR || clipJson->arr.size() != 2) throw std::runtime_error("bad clip");
            const Json& a = clipJson->arr[0];
            const Json& b = clipJson->arr[1];
            if (a.type != Json::NUM || b.type != Json::NUM) throw std::runtime_error("bad clip");
            lo = a.numVal; hi = b.numVal;
            if (lo > hi) {
                std::cout << "INVALID_CLIP\n";
                return;
            }
            hasClip = true;
        }
        if (hasClip) {
            std::vector<Interval> clipped;
            for (const auto& iv : intervals) {
                long long s = std::max(iv.start, lo);
                long long e = std::min(iv.end, hi);
                if (s < e) clipped.push_back({s, e});
            }
            intervals = std::move(clipped);
        }
        std::sort(intervals.begin(), intervals.end(), [](const Interval& a, const Interval& b) {
            if (a.start != b.start) return a.start < b.start;
            return a.end < b.end;
        });
        std::vector<Interval> merged;
        for (const auto& iv : intervals) {
            if (merged.empty() || iv.start > merged.back().end) {
                merged.push_back(iv);
            } else {
                if (iv.end > merged.back().end) merged.back().end = iv.end;
            }
        }
        long long length = 0;
        for (const auto& iv : merged) {
            length += iv.end - iv.start;
        }
        std::cout << "{\"intervals\":[";
        for (size_t i = 0; i < merged.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << "[" << merged[i].start << "," << merged[i].end << "]";
        }
        std::cout << "],\"length\":" << length << "}\n";
    } catch (...) {
        std::cout << "INVALID_JSON\n";
    }
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string line;
    while (std::getline(std::cin, line)) {
        processLine(line);
    }
    return 0;
}

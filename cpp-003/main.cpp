#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cctype>
#include <cstdint>

using namespace std;

enum class JType { Null, Bool, Int, Double, String, Array, Object };

struct Json {
    JType type = JType::Null;
    bool b = false;
    long long i = 0;
    double d = 0;
    string s;
    vector<Json> arr;
    vector<pair<string, Json>> obj;
};

class Parser {
    const string& in;
    size_t pos = 0;
public:
    Parser(const string& s) : in(s) {}
    bool parse(Json& out) {
        skip();
        if (!parseValue(out)) return false;
        skip();
        return pos == in.size();
    }
private:
    void skip() {
        while (pos < in.size() && (in[pos]==' ' || in[pos]=='\t' || in[pos]=='\n' || in[pos]=='\r')) pos++;
    }
    bool parseValue(Json& v) {
        skip();
        if (pos >= in.size()) return false;
        char c = in[pos];
        if (c == '{') return parseObject(v);
        if (c == '[') return parseArray(v);
        if (c == '"') { v.type = JType::String; return parseString(v.s); }
        if (c == 't') {
            if (in.compare(pos, 4, "true") == 0) { pos += 4; v.type = JType::Bool; v.b = true; return true; }
            return false;
        }
        if (c == 'f') {
            if (in.compare(pos, 5, "false") == 0) { pos += 5; v.type = JType::Bool; v.b = false; return true; }
            return false;
        }
        if (c == 'n') {
            if (in.compare(pos, 4, "null") == 0) { pos += 4; v.type = JType::Null; return true; }
            return false;
        }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(v);
        return false;
    }
    bool parseObject(Json& v) {
        v.type = JType::Object;
        pos++; // {
        skip();
        if (pos < in.size() && in[pos] == '}') { pos++; return true; }
        while (true) {
            skip();
            if (pos >= in.size() || in[pos] != '"') return false;
            string key;
            if (!parseString(key)) return false;
            skip();
            if (pos >= in.size() || in[pos] != ':') return false;
            pos++;
            Json val;
            if (!parseValue(val)) return false;
            v.obj.emplace_back(std::move(key), std::move(val));
            skip();
            if (pos >= in.size()) return false;
            if (in[pos] == ',') { pos++; continue; }
            if (in[pos] == '}') { pos++; return true; }
            return false;
        }
    }
    bool parseArray(Json& v) {
        v.type = JType::Array;
        pos++; // [
        skip();
        if (pos < in.size() && in[pos] == ']') { pos++; return true; }
        while (true) {
            Json val;
            if (!parseValue(val)) return false;
            v.arr.emplace_back(std::move(val));
            skip();
            if (pos >= in.size()) return false;
            if (in[pos] == ',') { pos++; continue; }
            if (in[pos] == ']') { pos++; return true; }
            return false;
        }
    }
    bool parseString(string& out) {
        if (pos >= in.size() || in[pos] != '"') return false;
        pos++;
        out.clear();
        while (pos < in.size()) {
            char c = in[pos++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos >= in.size()) return false;
                char e = in[pos++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (pos + 4 > in.size()) return false;
                        unsigned int code = 0;
                        for (int i = 0; i < 4; i++) {
                            char h = in[pos++];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code += h - '0';
                            else if (h >= 'a' && h <= 'f') code += h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') code += h - 'A' + 10;
                            else return false;
                        }
                        if (code <= 0x7F) out.push_back((char)code);
                        else if (code <= 0x7FF) {
                            out.push_back((char)(0xC0 | (code >> 6)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        } else {
                            out.push_back((char)(0xE0 | (code >> 12)));
                            out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (code & 0x3F)));
                        }
                        break;
                    }
                    default: return false;
                }
            } else if ((unsigned char)c < 0x20) {
                return false;
            } else {
                out.push_back(c);
            }
        }
        return false;
    }
    bool parseNumber(Json& v) {
        size_t start = pos;
        if (pos < in.size() && in[pos] == '-') pos++;
        if (pos >= in.size()) return false;
        if (in[pos] == '0') {
            pos++;
            if (pos < in.size() && isdigit((unsigned char)in[pos])) return false;
        } else if (in[pos] >= '1' && in[pos] <= '9') {
            while (pos < in.size() && isdigit((unsigned char)in[pos])) pos++;
        } else {
            return false;
        }
        bool isInt = true;
        if (pos < in.size() && in[pos] == '.') {
            isInt = false;
            pos++;
            if (pos >= in.size() || !isdigit((unsigned char)in[pos])) return false;
            while (pos < in.size() && isdigit((unsigned char)in[pos])) pos++;
        }
        if (pos < in.size() && (in[pos] == 'e' || in[pos] == 'E')) {
            isInt = false;
            pos++;
            if (pos < in.size() && (in[pos] == '+' || in[pos] == '-')) pos++;
            if (pos >= in.size() || !isdigit((unsigned char)in[pos])) return false;
            while (pos < in.size() && isdigit((unsigned char)in[pos])) pos++;
        }
        string num = in.substr(start, pos - start);
        if (isInt) {
            v.type = JType::Int;
            try {
                v.i = std::stoll(num);
            } catch (...) {
                return false;
            }
        } else {
            v.type = JType::Double;
            try {
                v.d = std::stod(num);
            } catch (...) {
                return false;
            }
        }
        return true;
    }
};

static const Json* findField(const Json& obj, const string& key) {
    if (obj.type != JType::Object) return nullptr;
    for (const auto& p : obj.obj) {
        if (p.first == key) return &p.second;
    }
    return nullptr;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string line;
    while (getline(cin, line)) {
        Json root;
        Parser parser(line);
        if (!parser.parse(root) || root.type != JType::Object) {
            cout << "INVALID_JSON\n";
            continue;
        }
        const Json* intervalsField = findField(root, "intervals");
        if (!intervalsField || intervalsField->type != JType::Array) {
            cout << "INVALID_JSON\n";
            continue;
        }
        vector<pair<long long, long long>> intervals;
        bool invalidInterval = false;
        bool invalidStructure = false;
        for (const auto& item : intervalsField->arr) {
            if (item.type != JType::Array || item.arr.size() != 2) {
                invalidStructure = true;
                break;
            }
            if (item.arr[0].type != JType::Int || item.arr[1].type != JType::Int) {
                invalidStructure = true;
                break;
            }
            long long s = item.arr[0].i;
            long long e = item.arr[1].i;
            if (s > e) {
                invalidInterval = true;
                break;
            }
            if (s == e) continue;
            intervals.push_back({s, e});
        }
        if (invalidStructure) {
            cout << "INVALID_JSON\n";
            continue;
        }
        if (invalidInterval) {
            cout << "INVALID_INTERVAL\n";
            continue;
        }
        const Json* clipField = findField(root, "clip");
        if (clipField) {
            if (clipField->type != JType::Array || clipField->arr.size() != 2 ||
                clipField->arr[0].type != JType::Int || clipField->arr[1].type != JType::Int) {
                cout << "INVALID_JSON\n";
                continue;
            }
            long long lo = clipField->arr[0].i;
            long long hi = clipField->arr[1].i;
            if (lo > hi) {
                cout << "INVALID_CLIP\n";
                continue;
            }
            vector<pair<long long, long long>> clipped;
            for (auto [s, e] : intervals) {
                long long ns = max(s, lo);
                long long ne = min(e, hi);
                if (ns < ne) clipped.push_back({ns, ne});
            }
            intervals = std::move(clipped);
        }
        sort(intervals.begin(), intervals.end(), [](const auto& a, const auto& b) {
            if (a.first != b.first) return a.first < b.first;
            return a.second < b.second;
        });
        vector<pair<long long, long long>> merged;
        for (auto [s, e] : intervals) {
            if (merged.empty() || s > merged.back().second) {
                merged.push_back({s, e});
            } else {
                if (e > merged.back().second) merged.back().second = e;
            }
        }
        long long length = 0;
        for (auto [s, e] : merged) {
            length += (e - s);
        }
        cout << "{\"intervals\":[";
        for (size_t i = 0; i < merged.size(); ++i) {
            if (i) cout << ",";
            cout << "[" << merged[i].first << "," << merged[i].second << "]";
        }
        cout << "],\"length\":" << length << "}\n";
    }
    return 0;
}

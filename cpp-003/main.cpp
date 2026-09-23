#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstring>

using namespace std;

struct Interval {
    long long start, end;
};

class Parser {
    const string& s;
    size_t pos = 0;
public:
    Parser(const string& str) : s(str) {}
    bool parseDocument(vector<Interval>& intervals, bool& hasClip, long long& lo, long long& hi) {
        skipWs();
        if (!parseObject(intervals, hasClip, lo, hi)) return false;
        skipWs();
        return pos == s.size();
    }
private:
    void skipWs() {
        while (pos < s.size() && isspace((unsigned char)s[pos])) pos++;
    }
    bool parseObject(vector<Interval>& intervals, bool& hasClip, long long& lo, long long& hi) {
        if (pos >= s.size() || s[pos] != '{') return false;
        pos++;
        skipWs();
        if (pos < s.size() && s[pos] == '}') { pos++; return true; }
        while (true) {
            skipWs();
            string key;
            if (!parseString(key)) return false;
            skipWs();
            if (pos >= s.size() || s[pos] != ':') return false;
            pos++;
            skipWs();
            if (key == "intervals") {
                if (!parseIntervals(intervals)) return false;
            } else if (key == "clip") {
                hasClip = true;
                if (!parseClip(lo, hi)) return false;
            } else {
                if (!skipValue()) return false;
            }
            skipWs();
            if (pos >= s.size()) return false;
            if (s[pos] == ',') {
                pos++;
                skipWs();
                continue;
            } else if (s[pos] == '}') {
                pos++;
                break;
            } else {
                return false;
            }
        }
        return true;
    }
    bool parseString(string& out) {
        if (pos >= s.size() || s[pos] != '"') return false;
        pos++;
        out.clear();
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos >= s.size()) return false;
                char esc = s[pos++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (pos + 4 > s.size()) return false;
                        unsigned int cp = 0;
                        for (int i = 0; i < 4; i++) {
                            char h = s[pos++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp += h - '0';
                            else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
                            else return false;
                        }
                        if (cp <= 0x7F) out.push_back((char)cp);
                        else if (cp <= 0x7FF) {
                            out.push_back((char)(0xC0 | (cp >> 6)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        } else {
                            out.push_back((char)(0xE0 | (cp >> 12)));
                            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back((char)(0x80 | (cp & 0x3F)));
                        }
                        break;
                    }
                    default: return false;
                }
            } else {
                if ((unsigned char)c < 0x20) return false;
                out.push_back(c);
            }
        }
        return false;
    }
    bool parseIntervals(vector<Interval>& intervals) {
        if (pos >= s.size() || s[pos] != '[') return false;
        pos++;
        skipWs();
        if (pos < s.size() && s[pos] == ']') { pos++; return true; }
        while (true) {
            skipWs();
            if (pos >= s.size() || s[pos] != '[') return false;
            pos++;
            skipWs();
            long long st, en;
            if (!parseNumber(st)) return false;
            skipWs();
            if (pos >= s.size() || s[pos] != ',') return false;
            pos++;
            skipWs();
            if (!parseNumber(en)) return false;
            skipWs();
            if (pos >= s.size() || s[pos] != ']') return false;
            pos++;
            intervals.push_back({st, en});
            skipWs();
            if (pos >= s.size()) return false;
            if (s[pos] == ',') {
                pos++;
                skipWs();
                continue;
            } else if (s[pos] == ']') {
                pos++;
                break;
            } else {
                return false;
            }
        }
        return true;
    }
    bool parseClip(long long& lo, long long& hi) {
        if (pos >= s.size() || s[pos] != '[') return false;
        pos++;
        skipWs();
        if (!parseNumber(lo)) return false;
        skipWs();
        if (pos >= s.size() || s[pos] != ',') return false;
        pos++;
        skipWs();
        if (!parseNumber(hi)) return false;
        skipWs();
        if (pos >= s.size() || s[pos] != ']') return false;
        pos++;
        return true;
    }
    bool parseNumber(long long& out) {
        if (pos >= s.size()) return false;
        bool neg = false;
        if (s[pos] == '-') {
            neg = true;
            pos++;
            if (pos >= s.size()) return false;
        }
        if (pos >= s.size()) return false;
        if (s[pos] == '0') {
            pos++;
            if (pos < s.size() && isdigit((unsigned char)s[pos])) return false;
        } else if (s[pos] >= '1' && s[pos] <= '9') {
            // consume in while
        } else {
            return false;
        }
        long long val = 0;
        while (pos < s.size() && isdigit((unsigned char)s[pos])) {
            int d = s[pos] - '0';
            if (val > (LLONG_MAX - d) / 10) return false;
            val = val * 10 + d;
            pos++;
        }
        out = neg ? -val : val;
        return true;
    }
    bool skipValue() {
        skipWs();
        if (pos >= s.size()) return false;
        char c = s[pos];
        if (c == '"') {
            string dummy;
            return parseString(dummy);
        } else if (c == '{') {
            return skipObject();
        } else if (c == '[') {
            return skipArray();
        } else if (c == 't') {
            return consumeLiteral("true");
        } else if (c == 'f') {
            return consumeLiteral("false");
        } else if (c == 'n') {
            return consumeLiteral("null");
        } else if (c == '-' || isdigit((unsigned char)c)) {
            long long dummy;
            return parseNumber(dummy);
        } else {
            return false;
        }
    }
    bool skipObject() {
        if (pos >= s.size() || s[pos] != '{') return false;
        pos++;
        skipWs();
        if (pos < s.size() && s[pos] == '}') { pos++; return true; }
        while (true) {
            skipWs();
            string key;
            if (!parseString(key)) return false;
            skipWs();
            if (pos >= s.size() || s[pos] != ':') return false;
            pos++;
            skipWs();
            if (!skipValue()) return false;
            skipWs();
            if (pos >= s.size()) return false;
            if (s[pos] == ',') {
                pos++;
                skipWs();
                continue;
            } else if (s[pos] == '}') {
                pos++;
                break;
            } else {
                return false;
            }
        }
        return true;
    }
    bool skipArray() {
        if (pos >= s.size() || s[pos] != '[') return false;
        pos++;
        skipWs();
        if (pos < s.size() && s[pos] == ']') { pos++; return true; }
        while (true) {
            skipWs();
            if (!skipValue()) return false;
            skipWs();
            if (pos >= s.size()) return false;
            if (s[pos] == ',') {
                pos++;
                skipWs();
                continue;
            } else if (s[pos] == ']') {
                pos++;
                break;
            } else {
                return false;
            }
        }
        return true;
    }
    bool consumeLiteral(const char* lit) {
        size_t len = strlen(lit);
        if (pos + len > s.size()) return false;
        if (s.compare(pos, len, lit) != 0) return false;
        pos += len;
        return true;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string line;
    while (getline(cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        vector<Interval> intervals;
        bool hasClip = false;
        long long lo = 0, hi = 0;
        Parser parser(line);
        if (!parser.parseDocument(intervals, hasClip, lo, hi)) {
            cout << "INVALID_JSON\n";
            continue;
        }
        bool invalidInterval = false;
        for (const auto& iv : intervals) {
            if (iv.start > iv.end) {
                invalidInterval = true;
                break;
            }
        }
        if (invalidInterval) {
            cout << "INVALID_INTERVAL\n";
            continue;
        }
        if (hasClip && lo > hi) {
            cout << "INVALID_CLIP\n";
            continue;
        }
        vector<Interval> valid;
        for (const auto& iv : intervals) {
            if (iv.start == iv.end) continue;
            long long s = iv.start;
            long long e = iv.end;
            if (hasClip) {
                s = max(s, lo);
                e = min(e, hi);
            }
            if (s < e) {
                valid.push_back({s, e});
            }
        }
        sort(valid.begin(), valid.end(), [](const Interval& a, const Interval& b) {
            if (a.start != b.start) return a.start < b.start;
            return a.end < b.end;
        });
        vector<Interval> merged;
        for (const auto& iv : valid) {
            if (merged.empty() || iv.start > merged.back().end) {
                merged.push_back(iv);
            } else {
                if (iv.end > merged.back().end) {
                    merged.back().end = iv.end;
                }
            }
        }
        long long length = 0;
        for (const auto& iv : merged) {
            length += iv.end - iv.start;
        }
        cout << "{\"intervals\":[";
        for (size_t i = 0; i < merged.size(); ++i) {
            if (i) cout << ',';
            cout << '[' << merged[i].start << ',' << merged[i].end << ']';
        }
        cout << "],\"length\":" << length << "}\n";
    }
    return 0;
}

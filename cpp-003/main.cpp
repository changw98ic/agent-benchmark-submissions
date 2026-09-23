#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>

using namespace std;

struct Request {
    vector<pair<int,int>> intervals;
    bool has_clip = false;
    int clip_lo = 0, clip_hi = 0;
};

class Parser {
    const string& s;
    size_t pos = 0;
public:
    Parser(const string& str) : s(str) {}

    void skip_ws() {
        while (pos < s.size() && isspace((unsigned char)s[pos])) pos++;
    }

    char peek() {
        skip_ws();
        return pos < s.size() ? s[pos] : '\0';
    }

    char get() {
        skip_ws();
        return pos < s.size() ? s[pos++] : '\0';
    }

    void expect(char c) {
        skip_ws();
        if (pos >= s.size() || s[pos] != c) throw runtime_error("parse error");
        pos++;
    }

    string parse_string() {
        skip_ws();
        expect('"');
        string out;
        while (pos < s.size() && s[pos] != '"') {
            char c = s[pos++];
            if (c == '\\') {
                if (pos >= s.size()) break;
                char esc = s[pos++];
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
                        for (int i = 0; i < 4 && pos < s.size(); ++i) pos++;
                        break;
                    }
                    default: out += esc;
                }
            } else {
                out += c;
            }
        }
        expect('"');
        return out;
    }

    long long parse_int() {
        skip_ws();
        bool neg = false;
        if (pos < s.size() && s[pos] == '-') {
            neg = true;
            pos++;
        }
        long long val = 0;
        while (pos < s.size() && isdigit((unsigned char)s[pos])) {
            val = val * 10 + (s[pos] - '0');
            pos++;
        }
        return neg ? -val : val;
    }

    void parse_intervals(vector<pair<int,int>>& intervals) {
        expect('[');
        skip_ws();
        if (peek() == ']') {
            get();
            return;
        }
        while (true) {
            expect('[');
            int start = (int)parse_int();
            expect(',');
            int end = (int)parse_int();
            expect(']');
            intervals.emplace_back(start, end);
            skip_ws();
            if (peek() == ',') {
                get();
                skip_ws();
                continue;
            } else if (peek() == ']') {
                get();
                break;
            } else {
                throw runtime_error("parse error");
            }
        }
    }

    void parse_clip(int& lo, int& hi) {
        expect('[');
        lo = (int)parse_int();
        expect(',');
        hi = (int)parse_int();
        expect(']');
    }

    void skip_value() {
        skip_ws();
        if (pos >= s.size()) return;
        char c = s[pos];
        if (c == '"') {
            parse_string();
        } else if (c == '{') {
            int depth = 0;
            do {
                if (s[pos] == '{') depth++;
                else if (s[pos] == '}') depth--;
                pos++;
            } while (depth > 0 && pos < s.size());
        } else if (c == '[') {
            int depth = 0;
            do {
                if (s[pos] == '[') depth++;
                else if (s[pos] == ']') depth--;
                pos++;
            } while (depth > 0 && pos < s.size());
        } else if (c == 't') {
            pos += 4;
        } else if (c == 'f') {
            pos += 5;
        } else if (c == 'n') {
            pos += 4;
        } else {
            while (pos < s.size() && (isdigit((unsigned char)s[pos]) || s[pos] == '-' || s[pos] == '+' || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E')) pos++;
        }
    }
};

Request parse_request(const string& line) {
    Parser p(line);
    Request req;
    p.expect('{');
    p.skip_ws();
    if (p.peek() == '}') {
        p.get();
        return req;
    }
    while (true) {
        string key = p.parse_string();
        p.expect(':');
        if (key == "intervals") {
            p.parse_intervals(req.intervals);
        } else if (key == "clip") {
            int lo, hi;
            p.parse_clip(lo, hi);
            req.has_clip = true;
            req.clip_lo = lo;
            req.clip_hi = hi;
        } else {
            p.skip_value();
        }
        p.skip_ws();
        if (p.peek() == ',') {
            p.get();
            p.skip_ws();
            continue;
        } else if (p.peek() == '}') {
            p.get();
            break;
        } else {
            throw runtime_error("parse error");
        }
    }
    return req;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string line;
    while (getline(cin, line)) {
        bool empty = true;
        for (char c : line) {
            if (!isspace((unsigned char)c)) { empty = false; break; }
        }
        if (empty) continue;

        try {
            Request req = parse_request(line);

            bool invalid_interval = false;
            for (auto& [s, e] : req.intervals) {
                if (s > e) { invalid_interval = true; break; }
            }
            if (invalid_interval) {
                cout << "{\"error\":\"INVALID_INTERVAL\"}\n";
                continue;
            }

            if (req.has_clip && req.clip_lo > req.clip_hi) {
                cout << "{\"error\":\"INVALID_CLIP\"}\n";
                continue;
            }

            vector<pair<int,int>> intervals;
            for (auto [s, e] : req.intervals) {
                if (s == e) continue;
                if (req.has_clip) {
                    int ns = max(s, req.clip_lo);
                    int ne = min(e, req.clip_hi);
                    if (ns < ne) intervals.emplace_back(ns, ne);
                } else {
                    intervals.emplace_back(s, e);
                }
            }

            sort(intervals.begin(), intervals.end());

            vector<pair<int,int>> merged;
            for (auto [s, e] : intervals) {
                if (merged.empty() || s > merged.back().second) {
                    merged.emplace_back(s, e);
                } else {
                    if (e > merged.back().second) merged.back().second = e;
                }
            }

            cout << "{\"intervals\":[";
            for (size_t i = 0; i < merged.size(); ++i) {
                if (i) cout << ',';
                cout << '[' << merged[i].first << ',' << merged[i].second << ']';
            }
            cout << "],\"length\":";
            long long length = 0;
            for (auto [s, e] : merged) length += (long long)(e - s);
            cout << length << "}\n";
        } catch (...) {
            cout << "{\"error\":\"INVALID_INPUT\"}\n";
        }
    }
    return 0;
}

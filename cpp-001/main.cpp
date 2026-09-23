{"files":[{"path":"main.cpp","content":"#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdint>
#include <cstdio>

using namespace std;

static void skipWs(const string& s, size_t& i) {
    while (i < s.size() && isspace((unsigned char)s[i])) ++i;
}

static bool parseString(const string& s, size_t& i, string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    ++i;
    out.clear();
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') return true;
        if (c == '\\\\') {
            if (i >= s.size()) return false;
            char esc = s[i++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\\\': out.push_back('\\\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\\b'); break;
                case 'f': out.push_back('\\f'); break;
                case 'n': out.push_back('\
'); break;
                case 'r': out.push_back('\\r'); break;
                case 't': out.push_back('\	'); break;
                case 'u': {
                    if (i + 4 > s.size()) return false;
                    unsigned code = 0;
                    for (int k = 0; k < 4; ++k) {
                        char h = s[i++];
                        code <<= 4;
                        if (h >= '0' && h <= '9') code |= (h - '0');
                        else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
                        else return false;
                    }
                    if (code <= 0x7F) {
                        out.push_back((char)code);
                    } else if (code <= 0x7FF) {
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
        } else {
            if ((unsigned char)c < 0x20) return false;
            out.push_back(c);
        }
    }
    return false;
}

static bool skipValue(const string& s, size_t& i);

static bool skipObject(const string& s, size_t& i) {
    ++i;
    skipWs(s, i);
    if (i < s.size() && s[i] == '}') { ++i; return true; }
    while (true) {
        skipWs(s, i);
        string key;
        if (!parseString(s, i, key)) return false;
        skipWs(s, i);
        if (i >= s.size() || s[i] != ':') return false;
        ++i;
        if (!skipValue(s, i)) return false;
        skipWs(s, i);
        if (i >= s.size()) return false;
        if (s[i] == ',') { ++i; continue; }
        if (s[i] == '}') { ++i; return true; }
        return false;
    }
}

static bool skipArray(const string& s, size_t& i) {
    ++i;
    skipWs(s, i);
    if (i < s.size() && s[i] == ']') { ++i; return true; }
    while (true) {
        if (!skipValue(s, i)) return false;
        skipWs(s, i);
        if (i >= s.size()) return false;
        if (s[i] == ',') { ++i; continue; }
        if (s[i] == ']') { ++i; return true; }
        return false;
    }
}

static bool skipNumber(const string& s, size_t& i) {
    if (i >= s.size()) return false;
    if (s[i] == '-') {
        ++i;
        if (i >= s.size()) return false;
    }
    if (s[i] == '0') {
        ++i;
    } else if (s[i] >= '1' && s[i] <= '9') {
        while (i < s.size() && isdigit((unsigned char)s[i])) ++i;
    } else {
        return false;
    }
    if (i < s.size() && s[i] == '.') {
        ++i;
        if (i >= s.size() || !isdigit((unsigned char)s[i])) return false;
        while (i < s.size() && isdigit((unsigned char)s[i])) ++i;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
        if (i >= s.size() || !isdigit((unsigned char)s[i])) return false;
        while (i < s.size() && isdigit((unsigned char)s[i])) ++i;
    }
    return true;
}

static bool skipValue(const string& s, size_t& i) {
    skipWs(s, i);
    if (i >= s.size()) return false;
    char c = s[i];
    if (c == '"') {
        string dummy;
        return parseString(s, i, dummy);
    }
    if (c == '{') return skipObject(s, i);
    if (c == '[') return skipArray(s, i);
    if (c == 't') {
        if (s.compare(i, 4, "true") == 0) { i += 4; return true; }
        return false;
    }
    if (c == 'f') {
        if (s.compare(i, 5, "false") == 0) { i += 5; return true; }
        return false;
    }
    if (c == 'n') {
        if (s.compare(i, 4, "null") == 0) { i += 4; return true; }
        return false;
    }
    if (c == '-' || (c >= '0' && c <=

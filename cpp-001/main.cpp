#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cctype>
#include <cstdio>

using namespace std;

static string toLowerHex(const uint8_t* data, size_t len) {
    if (len == 0) return "";
    static const char* hex = "0123456789abcdef";
    string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(hex[data[i] >> 4]);
        out.push_back(hex[data[i] & 0x0F]);
    }
    return out;
}

static string escapeJsonString(const string& s) {
    string out;
    out.reserve(s.size() + 2);
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (c >= 32 && c <= 126) {
            out.push_back(c);
        } else {
            char buf[7];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        }
    }
    return out;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string line;
    while (getline(cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t hexPos = line.find("\"hex\"");
        if (hexPos == string::npos) {
            cout << "{\"error\":\"INVALID_HEX\"}\n";
            continue;
        }
        size_t colon = line.find(':', hexPos + 5);
        if (colon == string::npos) {
            cout << "{\"error\":\"INVALID_HEX\"}\n";
            continue;
        }
        size_t q1 = line.find('"', colon + 1);
        if (q1 == string::npos) {
            cout << "{\"error\":\"INVALID_HEX\"}\n";
            continue;
        }
        size_t q2 = line.find('"', q1 + 1);
        if (q2 == string::npos) {
            cout << "{\"error\":\"INVALID_HEX\"}\n";
            continue;
        }
        string hex = line.substr(q1 + 1, q2 - q1 - 1);
        bool valid = (hex.size() % 2 == 0);
        for (char c : hex) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            cout << "{\"error\":\"INVALID_HEX\"}\n";
            continue;
        }
        vector<uint8_t> data;
        data.reserve(hex.size() / 2);
        auto hexVal = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
            return 0;
        };
        for (size_t i = 0; i < hex.size(); i += 2) {
            data.push_back(static_cast<uint8_t>((hexVal(hex[i]) << 4) | hexVal(hex[i+1])));
        }

        string out = "{\"records\":[";
        size_t i = 0;
        bool first = true;
        bool error = false;
        string errOut;
        while (i < data.size()) {
            size_t start = i;
            if (data.size() - i < 3) {
                error = true;
                errOut = "{\"error\":\"TRUNCATED\",\"offset\":" + to_string(start) + "}";
                break;
            }
            uint8_t type = data[i];
            uint16_t len = (static_cast<uint16_t>(data[i+1]) << 8) | static_cast<uint16_t>(data[i+2]);
            i += 3;
            if (data.size() - i < len) {
                error = true;
                errOut = "{\"error\":\"TRUNCATED\",\"offset\":" + to_string(start) + "}";
                break;
            }
            if (type == 1) {
                if (len < 1 || len > 32) {
                    error = true;
                    errOut = "{\"error\":\"INVALID_VALUE\",\"offset\":" + to_string(start) + "}";
                    break;
                }
                bool validStr = true;
                for (size_t j = 0; j < len; ++j) {
                    uint8_t b = data[i + j];
                    if (b < 32 || b > 126) {
                        validStr = false;
                        break;
                    }
                }
                if (!validStr) {
                    error = true;
                    errOut = "{\"error\":\"INVALID_VALUE\",\"offset\":" + to_string(start) + "}";
                    break;
                }
                if (!first) out += ",";
                first = false;
                string s(reinterpret_cast<const char*>(&data[i]), len);
                out += "{\"type\":1,\"value\":\"" + escapeJsonString(s) + "\"}";
            } else if (type == 2) {
                if (len != 4) {
                    error = true;
                    errOut = "{\"error\":\"INVALID_VALUE\",\"offset\":" + to_string(start) + "}";
                    break;
                }
                uint32_t u = (static_cast<uint32_t>(data[i]) << 24) |
                             (static_cast<uint32_t>(data[i+1]) << 16) |
                             (static_cast<uint32_t>(data[i+2]) << 8) |
                             static_cast<uint32_t>(data[i+3]);
                int64_t v = u;
                if (v > 2147483647LL) v -= 4294967296LL;
                int32_t val = static_cast<int32_t>(v);
                if (!first) out += ",";
                first = false;
                out += "{\"type\":2,\"value\":" + to_string(val) + "}";
            } else {
                if (!first) out += ",";
                first = false;
                string payloadHex;
                if (len > 0) {
                    payloadHex = toLowerHex(&data[i], len);
                }
                out += "{\"type\":" + to_string(type) + ",\"value\":\"" + payloadHex + "\"}";
            }
            i += len;
        }
        if (error) {
            cout << errOut << '\n';
        } else {
            out += "]}";
            cout << out << '\n';
        }
    }
    return 0;
}

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>

static int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool decodeHex(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = hexDigit(hex[i]);
        int lo = hexDigit(hex[i+1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
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
    return out;
}

static std::string errorJson(const std::string& err, size_t offset) {
    return "{\"error\":\"" + err + "\",\"offset\":" + std::to_string(offset) + "}";
}

static std::string processHex(const std::string& hex) {
    std::vector<uint8_t> data;
    if (!decodeHex(hex, data)) {
        return "{\"error\":\"INVALID_HEX\"}";
    }
    struct Rec {
        int type;
        bool isString;
        std::string str;
        int64_t num;
    };
    std::vector<Rec> records;
    size_t offset = 0;
    while (offset < data.size()) {
        size_t recStart = offset;
        if (offset + 3 > data.size()) {
            return errorJson("TRUNCATED", recStart);
        }
        uint8_t type = data[offset];
        uint16_t len = (static_cast<uint16_t>(data[offset+1]) << 8) | data[offset+2];
        offset += 3;
        if (offset + len > data.size()) {
            return errorJson("TRUNCATED", recStart);
        }
        size_t payloadStart = offset;
        offset += len;
        if (type == 1) {
            if (len < 1 || len > 32) {
                return errorJson("INVALID_VALUE", recStart);
            }
            std::string s;
            s.reserve(len);
            for (size_t i = 0; i < len; ++i) {
                uint8_t c = data[payloadStart + i];
                if (c < 32 || c > 126) {
                    return errorJson("INVALID_VALUE", recStart);
                }
                s.push_back(static_cast<char>(c));
            }
            records.push_back({1, true, s, 0});
        } else if (type == 2) {
            if (len != 4) {
                return errorJson("INVALID_VALUE", recStart);
            }
            uint32_t u = (static_cast<uint32_t>(data[payloadStart]) << 24) |
                         (static_cast<uint32_t>(data[payloadStart+1]) << 16) |
                         (static_cast<uint32_t>(data[payloadStart+2]) << 8) |
                         static_cast<uint32_t>(data[payloadStart+3]);
            int64_t val = static_cast<int64_t>(u);
            if (val >= 2147483648LL) val -= 4294967296LL;
            records.push_back({2, false, "", val});
        } else {
            std::string h;
            h.reserve(len * 2);
            const char* hexchars = "0123456789abcdef";
            for (size_t i = 0; i < len; ++i) {
                uint8_t b = data[payloadStart + i];
                h.push_back(hexchars[b >> 4]);
                h.push_back(hexchars[b & 0x0F]);
            }
            records.push_back({type, true, h, 0});
        }
    }
    std::string out = "{\"records\":[";
    for (size_t i = 0; i < records.size(); ++i) {
        if (i) out += ',';
        const Rec& r = records[i];
        out += "{\"type\":";
        out += std::to_string(r.type);
        out += ",\"value\":";
        if (r.isString) {
            out += '"';
            out += escapeJson(r.str);
            out += '"';
        } else {
            out += std::to_string(r.num);
        }
        out += '}';
    }
    out += "]}";
    return out;
}

static std::string extractHex(const std::string& line) {
    size_t i = 0;
    size_t n = line.size();
    auto skipWS = [&]() {
        while (i < n && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')) ++i;
    };
    skipWS();
    if (i >= n || line[i] != '{') return "";
    ++i;
    skipWS();
    if (i >= n || line[i] != '"') return "";
    ++i;
    std::string key;
    while (i < n && line[i] != '"') {
        key += line[i++];
    }
    if (i >= n) return "";
    ++i; // skip closing quote of key
    skipWS();
    if (i >= n || line[i] != ':') return "";
    ++i;
    skipWS();
    if (i >= n || line[i] != '"') return "";
    ++i;
    std::string hex;
    while (i < n && line[i] != '"') {
        char c = line[i++];
        if (c == '\\') {
            if (i >= n) break;
            char e = line[i++];
            switch (e) {
                case '"': hex += '"'; break;
                case '\\': hex += '\\'; break;
                case '/': hex += '/'; break;
                case 'b': hex += '\b'; break;
                case 'f': hex += '\f'; break;
                case 'n': hex += '\n'; break;
                case 'r': hex += '\r'; break;
                case 't': hex += '\t'; break;
                case 'u': {
                    if (i + 4 <= n) {
                        unsigned code = 0;
                        for (int k = 0; k < 4; ++k) {
                            code = code * 16 + static_cast<unsigned>(hexDigit(line[i++]));
                        }
                        // Encode as UTF-8 (hex string should be ASCII, so this is just defensive)
                        if (code < 0x80) {
                            hex += static_cast<char>(code);
                        } else if (code < 0x800) {
                            hex += static_cast<char>(0xC0 | (code >> 6));
                            hex += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            hex += static_cast<char>(0xE0 | (code >> 12));
                            hex += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            hex += static_cast<char>(0x80 | (code & 0x3F));
                        }
                    }
                    break;
                }
                default: hex += e; break;
            }
        } else {
            hex += c;
        }
    }
    return hex;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::string hex = extractHex(line);
        std::string out = processHex(hex);
        std::cout << out << '\n';
    }
    return 0;
}

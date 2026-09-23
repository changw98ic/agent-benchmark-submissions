#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cctype>

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool parse_json_hex(const std::string& line, std::string& hex) {
    size_t i = 0;
    auto skip_ws = [&]() {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    };
    skip_ws();
    if (i >= line.size() || line[i] != '{') return false;
    ++i;
    skip_ws();
    if (i >= line.size() || line[i] != '"') return false;
    ++i;
    std::string key;
    while (i < line.size() && line[i] != '"') {
        if (line[i] == '\\') return false;
        key += line[i++];
    }
    if (i >= line.size() || line[i] != '"') return false;
    ++i;
    if (key != "hex") return false;
    skip_ws();
    if (i >= line.size() || line[i] != ':') return false;
    ++i;
    skip_ws();
    if (i >= line.size() || line[i] != '"') return false;
    ++i;
    hex.clear();
    while (i < line.size() && line[i] != '"') {
        if (line[i] == '\\') return false;
        hex += line[i++];
    }
    if (i >= line.size() || line[i] != '"') return false;
    ++i;
    skip_ws();
    if (i >= line.size() || line[i] != '}') return false;
    ++i;
    skip_ws();
    if (i != line.size()) return false;
    return true;
}

static bool hex_decode(const std::string& s, std::vector<uint8_t>& out) {
    if (s.size() % 2 != 0) return false;
    if (s.size() > 131072) return false; // 64KiB max
    out.clear();
    out.reserve(s.size() / 2);
    for (size_t i = 0; i < s.size(); i += 2) {
        int hi = hexval(s[i]);
        int lo = hexval(s[i+1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

static std::string parse_records(const std::vector<uint8_t>& data) {
    std::string out = "{\"records\":[";
    bool first = true;
    size_t pos = 0;
    while (pos < data.size()) {
        size_t start = pos;
        if (data.size() - pos < 3) {
            return "{\"error\":\"TRUNCATED\",\"offset\":" + std::to_string(start) + "}";
        }
        uint8_t type = data[pos];
        uint16_t len = (static_cast<uint16_t>(data[pos+1]) << 8) | data[pos+2];
        pos += 3;
        if (data.size() - pos < len) {
            return "{\"error\":\"TRUNCATED\",\"offset\":" + std::to_string(start) + "}";
        }
        if (type == 1) {
            if (len < 1 || len > 32) {
                return "{\"error\":\"INVALID_VALUE\",\"offset\":" + std::to_string(start) + "}";
            }
            bool ok = true;
            for (uint16_t i = 0; i < len; ++i) {
                uint8_t c = data[pos + i];
                if (c < 32 || c > 126) { ok = false; break; }
            }
            if (!ok) {
                return "{\"error\":\"INVALID_VALUE\",\"offset\":" + std::to_string(start) + "}";
            }
            if (!first) out += ",";
            first = false;
            out += "{\"type\":1,\"value\":\"";
            for (uint16_t i = 0; i < len; ++i) {
                char c = static_cast<char>(data[pos + i]);
                if (c == '"' || c == '\\') out += '\\';
                out += c;
            }
            out += "\"}";
        } else if (type == 2) {
            if (len != 4) {
                return "{\"error\":\"INVALID_VALUE\",\"offset\":" + std::to_string(start) + "}";
            }
            uint32_t u = (static_cast<uint32_t>(data[pos]) << 24) |
                         (static_cast<uint32_t>(data[pos+1]) << 16) |
                         (static_cast<uint32_t>(data[pos+2]) << 8) |
                         static_cast<uint32_t>(data[pos+3]);
            int32_t val = static_cast<int32_t>(u);
            if (!first) out += ",";
            first = false;
            out += "{\"type\":2,\"value\":" + std::to_string(val) + "}";
        } else {
            const char* digits = "0123456789abcdef";
            std::string hex;
            for (uint16_t i = 0; i < len; ++i) {
                uint8_t b = data[pos + i];
                hex += digits[b >> 4];
                hex += digits[b & 0xF];
            }
            if (!first) out += ",";
            first = false;
            out += "{\"type\":" + std::to_string(type) + ",\"value\":\"" + hex + "\"}";
        }
        pos += len;
    }
    out += "]}";
    return out;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string hex;
        if (!parse_json_hex(line, hex)) {
            std::cout << "INVALID_JSON\n";
            continue;
        }
        std::vector<uint8_t> data;
        if (!hex_decode(hex, data)) {
            std::cout << "INVALID_HEX\n";
            continue;
        }
        std::cout << parse_records(data) << '\n';
    }
    return 0;
}

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace {

bool extractHex(const std::string& line, std::string& hex) {
    const std::string key = "\"hex\"";
    size_t pos = line.find(key);
    if (pos == std::string::npos) return false;
    pos += key.size();
    pos = line.find(':', pos);
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < line.size()) {
        char c = line[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++pos;
        } else {
            break;
        }
    }
    if (pos >= line.size() || line[pos] != '"') return false;
    ++pos;
    size_t start = pos;
    size_t end = line.find('"', start);
    if (end == std::string::npos) return false;
    hex = line.substr(start, end - start);
    return true;
}

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool decodeHex(const std::string& hex, std::vector<unsigned char>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = hexVal(hex[i]);
        int lo = hexVal(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<unsigned char>((hi << 4) | lo));
    }
    return true;
}

std::string processHex(const std::string& hex) {
    std::vector<unsigned char> data;
    if (!decodeHex(hex, data)) {
        return "{\"error\":\"INVALID_HEX\"}";
    }

    std::string records;
    bool first = true;
    size_t cursor = 0;

    while (cursor < data.size()) {
        size_t recordStart = cursor;

        if (data.size() - cursor < 3) {
            return "{\"error\":\"TRUNCATED\",\"offset\":" + std::to_string(recordStart) + "}";
        }

        unsigned char type = data[cursor];
        unsigned int length = (static_cast<unsigned int>(data[cursor + 1]) << 8) |
                              static_cast<unsigned int>(data[cursor + 2]);
        cursor += 3;

        if (data.size() - cursor < length) {
            return "{\"error\":\"TRUNCATED\",\"offset\":" + std::to_string(recordStart) + "}";
        }

        std::string valueJson;
        if (type == 1) {
            if (length < 1 || length > 32) {
                return "{\"error\":\"INVALID_VALUE\",\"offset\":" + std::to_string(recordStart) + "}";
            }
            bool valid = true;
            for (unsigned int i = 0; i < length; ++i) {
                unsigned char c = data[cursor + i];
                if (c < 32 || c > 126) {
                    valid = false;
                    break;
                }
            }
            if (!valid) {
                return "{\"error\":\"INVALID_VALUE\",\"offset\":" + std::to_string(recordStart) + "}";
            }
            std::string s;
            s.reserve(length + 2);
            s.push_back('"');
            for (unsigned int i = 0; i < length; ++i) {
                unsigned char c = data[cursor + i];
                if (c == '"' || c == '\\') {
                    s.push_back('\\');
                }
                s.push_back(static_cast<char>(c));
            }
            s.push_back('"');
            valueJson = std::move(s);
        } else if (type == 2) {
            if (length != 4) {
                return "{\"error\":\"INVALID_VALUE\",\"offset\":" + std::to_string(recordStart) + "}";
            }
            uint32_t v = (static_cast<uint32_t>(data[cursor]) << 24) |
                         (static_cast<uint32_t>(data[cursor + 1]) << 16) |
                         (static_cast<uint32_t>(data[cursor + 2]) << 8) |
                         static_cast<uint32_t>(data[cursor + 3]);
            int64_t sval;
            if (v <= 0x7FFFFFFFu) {
                sval = static_cast<int64_t>(v);
            } else {
                sval = static_cast<int64_t>(v) - 0x100000000LL;
            }
            valueJson = std::to_string(sval);
        } else {
            std::string h;
            h.reserve(2 * length + 2);
            h.push_back('"');
            const char* hexDigits = "0123456789abcdef";
            for (unsigned int i = 0; i < length; ++i) {
                unsigned char c = data[cursor + i];
                h.push_back(hexDigits[c >> 4]);
                h.push_back(hexDigits[c & 0x0F]);
            }
            h.push_back('"');
            valueJson = std::move(h);
        }

        if (!first) {
            records.push_back(',');
        }
        records += "{\"type\":" + std::to_string(static_cast<unsigned int>(type)) +
                   ",\"value\":" + valueJson + "}";
        first = false;

        cursor += length;
    }

    return "{\"records\":[" + records + "]}";
}

} // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        std::string hex;
        if (!extractHex(line, hex)) {
            std::cout << "{\"error\":\"INVALID_HEX\"}\n";
            continue;
        }

        std::cout << processHex(hex) << '\n';
    }

    return 0;
}

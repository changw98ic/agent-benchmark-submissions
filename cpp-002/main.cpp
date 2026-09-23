struct Json {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type;
    std::string raw;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;
};

class Parser {
    const std::string& s;
    size_t pos = 0;
public:
    Parser(const std::string& str) : s(str) {}
    Json parse() {
        skip_ws();
        return parse_value();
    }
private:
    void skip_ws() {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) pos++;
    }
    Json parse_value() {
        skip_ws();
        size_t start = pos;
        Json v;
        if (pos >= s.size()) throw std::runtime_error("unexpected end");
        if (s[pos] == 'n') {
            expect("null");
            v.type = Json::NUL;
        } else if (s[pos] == 't') {
            expect("true");
            v.type = Json::BOOL;
        } else if (s[pos] == 'f') {
            expect("false");
            v.type = Json::BOOL;
        } else if (s[pos] == '"') {
            v.type = Json::STR;
            v.str = parse_string();
        } else if (s[pos] == '[') {
            v.type = Json::ARR;
            parse_array(v);
        } else if (s[pos] == '{') {
            v.type = Json::OBJ;
            parse_object(v);
        } else if (s[pos] == '-' || std::isdigit(static_cast<unsigned char>(s[pos]))) {
            v.type = Json::NUM;
            parse_number();
        } else {
            throw std::runtime_error("invalid value");
        }
        v.raw = s.substr(start, pos - start);
        return v;
    }
    void expect(const char* lit) {
        while (*lit) {
            if (pos >= s.size() || s[pos] != *lit) throw std::runtime_error("invalid literal");
            pos++;
            lit++;
        }
    }
    std::string parse_string() {
        if (s[pos] != '"') throw std::runtime_error("expected string");
        pos++; // skip "
        std::string result;
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') break;
            if (c == '\\') {
                if (pos >= s.size()) throw std::runtime_error("bad escape");
                char esc = s[pos++];
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        if (pos + 3 >= s.size()) throw std::runtime_error("bad unicode");
                        unsigned code = 0;
                        for (int i = 0; i < 4; i++) {
                            char h = s[pos++];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= h - '0';
                            else if (h >= 'a' && h <= 'f') code |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') code |= h - 'A' + 10;
                            else throw std::runtime_error("bad hex");
                        }
                        // encode UTF-8
                        if (code <= 0x7F) {
                            result += static_cast<char>(code);
                        } else if (code <= 0x7FF) {
                            result += static_cast<char>(0xC0 | (code >> 6));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (code >> 12));
                            result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: result += esc;
                }
            } else {
                result += c;
            }
        }
        return result;
    }
    void parse_number() {
        while (pos < s.size() && (std::isdigit(static_cast<unsigned char>(s[pos])) || s[pos] == '-' || s[pos] == '+' || s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E')) {
            pos++;
        }
    }
    void parse_array(Json& v) {
        pos++; // skip [
        skip_ws();
        if (pos < s.size() && s[pos] == ']') {
            pos++;
            return;
        }
        while (true) {
            v.arr.push_back(parse_value());
            skip_ws();
            if (pos >= s.size()) throw std::runtime_error("bad array");
            if (s[pos] == ',') {
                pos++;
                continue;
            }
            if (s[pos] == ']') {
                pos++;
                break;
            }
            throw std::runtime_error("expected , or ]");
        }
    }
    void parse_object(Json& v) {
        pos++; // skip {
        skip_ws();
        if (pos < s.size() && s[pos] == '}') {
            pos++;
            return;
        }
        while (true) {
            skip_ws();
            if (pos >= s.size() || s[pos] != '"') throw std::runtime_error("expected key");
            std::string key = parse_string();
            skip_ws();
            if (pos >= s.size() || s[pos] != ':') throw std::runtime_error("expected :");
            pos++;
            skip_ws();
            Json val = parse_value();
            v.obj.emplace_back(std::move(key), std::move(val));
            skip_ws();
            if (pos >= s.size()) throw std::runtime_error("bad object");
            if (s[pos] == ',') {
                pos++;
                continue;
            }
            if (s[pos] == '}') {
                pos++;
                break;
            }
            throw std::runtime_error("expected , or }");
        }
    }
};

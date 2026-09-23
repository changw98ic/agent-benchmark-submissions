#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <utility>
#include <cctype>
#include <iterator>

struct Json {
  enum Type { NUL, BOOL, NUM, STR, ARR, OBJ };
  Type type = NUL;
  bool b = false;
  std::string num;
  std::string str;
  std::vector<Json> arr;
  std::vector<std::pair<std::string, Json>> obj;
};

class Parser {
  const std::string& s;
  size_t pos;
public:
  Parser(const std::string& str) : s(str), pos(0) {}
  bool parse(Json& out) {
    if (!parse_value(out)) return false;
    skip_ws();
    return pos == s.size();
  }
private:
  void skip_ws() {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) pos++;
  }
  bool parse_value(Json& out) {
    skip_ws();
    if (pos >= s.size()) return false;
    char c = s[pos];
    if (c == 'n') return parse_lit("null", Json::NUL, out);
    if (c == 't') return parse_lit("true", Json::BOOL, out, true);
    if (c == 'f') return parse_lit("false", Json::BOOL, out, false);
    if (c == '"') return parse_string(out);
    if (c == '[') return parse_array(out);
    if (c == '{') return parse_object(out);
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out);
    return false;
  }
  bool parse_lit(const char* lit, Json::Type t, Json& out, bool b = false) {
    size_t len = 0;
    while (lit[len]) ++len;
    if (pos + len > s.size() || s.compare(pos, len, lit) != 0) return false;
    pos += len;
    out.type = t;
    if (t == Json::BOOL) out.b = b;
    return true;
  }
  bool parse_string(Json& out) {
    if (pos >= s.size() || s[pos] != '"') return false;
    pos++;
    std::string res;
    while (pos < s.size()) {
      char c = s[pos++];
      if (c == '"') {
        out.type = Json::STR;
        out.str = std::move(res);
        return true;
      }
      if (c == '\\') {
        if (pos >= s.size()) return false;
        char e = s[pos++];
        switch (e) {
          case '"': res += '"'; break;
          case '\\': res += '\\'; break;
          case '/': res += '/'; break;
          case 'b': res += '\b'; break;
          case 'f': res += '\f'; break;
          case 'n': res += '\n'; break;
          case 'r': res += '\r'; break;
          case 't': res += '\t'; break;
          case 'u': {
            if (pos + 4 > s.size()) return false;
            unsigned int cp = 0;
            for (int i = 0; i < 4; ++i) {
              char h = s[pos++];
              cp <<= 4;
              if (h >= '0' && h <= '9') cp += h - '0';
              else if (h >= 'a' && h <= 'f') cp += h - 'a' + 10;
              else if (h >= 'A' && h <= 'F') cp += h - 'A' + 10;
              else return false;
            }
            if (cp >= 0xD800 && cp <= 0xDBFF) {
              if (pos + 6 <= s.size() && s[pos] == '\\' && s[pos+1] == 'u') {
                pos += 2;
                unsigned int cp2 = 0;
                for (int i = 0; i < 4; ++i) {
                  char h = s[pos++];
                  cp2 <<= 4;
                  if (h >= '0' && h <= '9') cp2 += h - '0';
                  else if (h >= 'a' && h <= 'f') cp2 += h - 'a' + 10;
                  else if (h >= 'A' && h <= 'F') cp2 += h - 'A' + 10;
                  else return false;
                }
                if (cp2 >= 0xDC00 && cp2 <= 0xDFFF) {
                  cp = 0x10000 + ((cp - 0xD800) << 10) + (cp2 - 0xDC00);
                }
              }
            }
            if (cp <= 0x7F) {
              res += (char)cp;
            } else if (cp <= 0x7FF) {
              res += (char)(0xC0 | (cp >> 6));
              res += (char)(0x80 | (cp & 0x3F));
            } else if (cp <= 0xFFFF) {
              res += (char)(0xE0 | (cp >> 12));
              res += (char)(0x80 | ((cp >> 6) & 0x3F));
              res += (char)(0x80 | (cp & 0x3F));
            } else {
              res += (char)(0xF0 | (cp >> 18));
              res += (char)(0x80 | ((cp >> 12) & 0x3F));
              res += (char)(0x80 | ((cp >> 6) & 0x3F));
              res += (char)(0x80 | (cp & 0x3F));
            }
            break;
          }
          default: return false;
        }
      } else {
        res += c;
      }
    }
    return false;
  }
  bool parse_number(Json& out) {
    size_t start = pos;
    if (s[pos] == '-') pos++;
    if (pos >= s.size() || !isdigit((unsigned char)s[pos])) return false;
    while (pos < s.size() && isdigit((unsigned char)s[pos])) pos++;
    if (pos < s.size() && s[pos] == '.') {
      pos++;
      if (pos >= s.size() || !isdigit((unsigned char)s[pos])) return false;
      while (pos < s.size() && isdigit((unsigned char)s[pos])) pos++;
    }
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
      pos++;
      if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
      if (pos >= s.size() || !isdigit((unsigned char)s[pos])) return false;
      while (pos < s.size() && isdigit((unsigned char)s[pos])) pos++;
    }
    out.type = Json::NUM;
    out.num = s.substr(start, pos - start);
    return true;
  }
  bool parse_array(Json& out) {
    pos++; // '['
    out.type = Json::ARR;
    skip_ws();
    if (pos < s.size() && s[pos] == ']') {
      pos++;
      return true;
    }
    while (true) {
      Json elem;
      if (!parse_value(elem)) return false;
      out.arr.push_back(std::move(elem));
      skip_ws();
      if (pos >= s.size()) return false;
      if (s[pos] == ',') {
        pos++;
        continue;
      }
      if (s[pos] == ']') {
        pos++;
        return true;
      }
      return false;
    }
  }
  bool parse_object(Json& out) {
    pos++; // '{'
    out.type = Json::OBJ;
    skip_ws();
    if (pos < s.size() && s[pos] == '}') {
      pos++;
      return true;
    }
    while (true) {
      Json key;
      if (!parse_string(key)) return false;
      skip_ws();
      if (pos >= s.size() || s[pos] != ':') return false;
      pos++;
      Json val;
      if (!parse_value(val)) return false;
      out.obj.emplace_back(std::move(key.str), std::move(val));
      skip_ws();
      if (pos >= s.size()) return false;
      if (s[pos] == ',') {
        pos++;
        continue;
      }
      if (s[pos] == '}') {
        pos++;
        return true;
      }
      return false;
    }
  }
};

Json make_object() {
  Json j;
  j.type = Json::OBJ;
  return j;
}

Json make_array() {
  Json j;
  j.type = Json::ARR;
  return j;
}

Json make_string(const std::string& s) {
  Json j;
  j.type = Json::STR;
  j.str = s;
  return j;
}

Json make_bool(bool b) {
  Json j;
  j.type = Json::BOOL;
  j.b = b;
  return j;
}

void serialize_string(const std::string& s, std::string& out) {
  out += '"';
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
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += (char)c;
        }
    }
  }
  out += '"';
}

void serialize(const Json& j, std::string& out) {
  switch (j.type) {
    case Json::NUL: out += "null"; break;
    case Json::BOOL: out += j.b ? "true" : "false"; break;
    case Json::NUM: out += j.num; break;
    case Json::STR: serialize_string(j.str, out); break;
    case Json::ARR:
      out += '[';
      for (size_t i = 0; i < j.arr.size(); ++i) {
        if (i) out += ',';
        serialize(j.arr[i], out);
      }
      out += ']';
      break;
    case Json::OBJ:
      out += '{';
      for (size_t i = 0; i < j.obj.size(); ++i) {
        if (i) out += ',';
        serialize_string(j.obj[i].first, out);
        out += ':';
        serialize(j.obj[i].second, out);
      }
      out += '}';
      break;
  }
}

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);
  std::string line;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    Json root;
    Parser parser(line);
    if (!parser.parse(root)) {
      std::cout << "INVALID_JSON\n";
      continue;
    }
    if (root.type != Json::OBJ) {
      std::cout << "INVALID_JSON\n";
      continue;
    }
    const Json* capJson = nullptr;
    const Json* opsJson = nullptr;
    for (const auto& p : root.obj) {
      if (p.first == "capacity") capJson = &p.second;
      else if (p.first == "ops") opsJson = &p.second;
    }
    if (!capJson || capJson->type != Json::NUM || !opsJson || opsJson->type != Json::ARR) {
      std::cout << "INVALID_JSON\n";
      continue;
    }
    int capacity = 0;
    try {
      capacity = std::stoi(capJson->num);
    } catch (...) {
      std::cout << "INVALID_JSON\n";
      continue;
    }
    if (capacity < 0) {
      std::cout << "{\"error\":\"CAPACITY\"}\n";
      continue;
    }
    std::list<std::string> lru;
    std::unordered_map<std::string, std::pair<Json, std::list<std::string>::iterator>> cache;
    std::vector<Json> results;
    for (const Json& opJson : opsJson->arr) {
      if (opJson.type != Json::OBJ) {
        std::cout << "INVALID_JSON\n";
        // To avoid partial processing, we could break and continue outer? But we already processed some. 
        // Better to just treat as invalid and continue outer loop? But we can't easily restart. 
        // Since spec guarantees valid structure, we can just continue.
        continue;
      }
      std::string opType;
      const Json* keyJson = nullptr;
      const Json* valueJson = nullptr;
      const Json* capJson2 = nullptr;
      for (const auto& p : opJson.obj) {
        if (p.first == "op") {
          if (p.second.type == Json::STR) opType = p.second.str;
        } else if (p.first == "key") {
          keyJson = &p.second;
        } else if (p.first == "value") {
          valueJson = &p.second;
        } else if (p.first == "capacity") {
          capJson2 = &p.second;
        }
      }
      if (opType == "get") {
        if (!keyJson || keyJson->type != Json::STR) continue;
        const std::string& key = keyJson->str;
        auto it = cache.find(key);
        if (it != cache.end()) {
          lru.erase(it->second.second);
          lru.push_back(key);
          it->second.second = std::prev(lru.end());
          Json res = make_object();
          res.obj.emplace_back("found", make

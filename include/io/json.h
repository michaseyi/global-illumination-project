// minimal self-contained json parser (header-only).
//
// this is deliberately small and dependency-free: it parses the subset of json
// needed for our scene files (objects, arrays, strings, numbers, bool, null)
// into a Value tree that the scene loader walks. it is not a general-purpose
// json library — no streaming, no duplicate-key handling beyond last-wins — but
// it is easy to read and enough for hand-authored scene descriptions.

#pragma once

#include <cctype>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<Value> arr;
    std::map<std::string, Value> obj;

    bool isNull()   const { return type == Type::Null; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray()  const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    bool has(const std::string& key) const {
        return type == Type::Object && obj.find(key) != obj.end();
    }

    // object lookup; returns a static null Value when absent so callers can
    // chain safely and check the result's type.
    const Value& operator[](const std::string& key) const {
        static const Value kNull;
        if (type != Type::Object) return kNull;
        auto it = obj.find(key);
        return (it == obj.end()) ? kNull : it->second;
    }

    size_t size() const { return type == Type::Array ? arr.size() : 0; }
    const Value& operator[](size_t i) const {
        static const Value kNull;
        return (type == Type::Array && i < arr.size()) ? arr[i] : kNull;
    }

    double asNumber(double fallback = 0.0) const {
        return type == Type::Number ? numVal : fallback;
    }
    bool asBool(bool fallback = false) const {
        return type == Type::Bool ? boolVal : fallback;
    }
    std::string asString(const std::string& fallback = "") const {
        return type == Type::String ? strVal : fallback;
    }
};

namespace detail {

struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string err;

    explicit Parser(const std::string& src) : s(src) {}

    void skipWs() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }
            // tolerate // line comments and /* */ blocks for editability.
            if (c == '/' && i + 1 < s.size()) {
                if (s[i + 1] == '/') {
                    i += 2;
                    while (i < s.size() && s[i] != '\n') ++i;
                    continue;
                }
                if (s[i + 1] == '*') {
                    i += 2;
                    while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) ++i;
                    i += 2;
                    continue;
                }
            }
            break;
        }
    }

    bool fail(const std::string& m) {
        if (err.empty()) err = m + " at offset " + std::to_string(i);
        return false;
    }

    bool parseValue(Value& out) {
        skipWs();
        if (i >= s.size()) return fail("unexpected end");
        char c = s[i];
        switch (c) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': return parseString(out);
            case 't': case 'f': return parseBool(out);
            case 'n': return parseNull(out);
            default:  return parseNumber(out);
        }
    }

    bool parseObject(Value& out) {
        out.type = Value::Type::Object;
        ++i;  // {
        skipWs();
        if (i < s.size() && s[i] == '}') { ++i; return true; }
        while (true) {
            skipWs();
            if (i >= s.size() || s[i] != '"') return fail("expected key string");
            Value key;
            if (!parseString(key)) return false;
            skipWs();
            if (i >= s.size() || s[i] != ':') return fail("expected ':'");
            ++i;
            Value val;
            if (!parseValue(val)) return false;
            out.obj[key.strVal] = std::move(val);
            skipWs();
            if (i >= s.size()) return fail("unterminated object");
            if (s[i] == ',') { ++i; continue; }
            if (s[i] == '}') { ++i; return true; }
            return fail("expected ',' or '}'");
        }
    }

    bool parseArray(Value& out) {
        out.type = Value::Type::Array;
        ++i;  // [
        skipWs();
        if (i < s.size() && s[i] == ']') { ++i; return true; }
        while (true) {
            Value val;
            if (!parseValue(val)) return false;
            out.arr.push_back(std::move(val));
            skipWs();
            if (i >= s.size()) return fail("unterminated array");
            if (s[i] == ',') { ++i; continue; }
            if (s[i] == ']') { ++i; return true; }
            return fail("expected ',' or ']'");
        }
    }

    bool parseString(Value& out) {
        out.type = Value::Type::String;
        ++i;  // opening quote
        std::string r;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') { out.strVal = std::move(r); return true; }
            if (c == '\\') {
                if (i >= s.size()) break;
                char e = s[i++];
                switch (e) {
                    case '"':  r += '"';  break;
                    case '\\': r += '\\'; break;
                    case '/':  r += '/';  break;
                    case 'n':  r += '\n'; break;
                    case 't':  r += '\t'; break;
                    case 'r':  r += '\r'; break;
                    case 'b':  r += '\b'; break;
                    case 'f':  r += '\f'; break;
                    case 'u': {
                        // decode \uXXXX to utf-8 (BMP only; enough for paths).
                        if (i + 4 > s.size()) return fail("bad \\u escape");
                        unsigned cp = std::strtoul(s.substr(i, 4).c_str(), nullptr, 16);
                        i += 4;
                        if (cp < 0x80) r += char(cp);
                        else if (cp < 0x800) {
                            r += char(0xC0 | (cp >> 6));
                            r += char(0x80 | (cp & 0x3F));
                        } else {
                            r += char(0xE0 | (cp >> 12));
                            r += char(0x80 | ((cp >> 6) & 0x3F));
                            r += char(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: return fail("bad string escape");
                }
            } else {
                r += c;
            }
        }
        return fail("unterminated string");
    }

    bool parseBool(Value& out) {
        if (s.compare(i, 4, "true") == 0) { out.type = Value::Type::Bool; out.boolVal = true;  i += 4; return true; }
        if (s.compare(i, 5, "false") == 0) { out.type = Value::Type::Bool; out.boolVal = false; i += 5; return true; }
        return fail("invalid literal");
    }

    bool parseNull(Value& out) {
        if (s.compare(i, 4, "null") == 0) { out.type = Value::Type::Null; i += 4; return true; }
        return fail("invalid literal");
    }

    bool parseNumber(Value& out) {
        size_t start = i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
        bool any = false;
        while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.' ||
                                s[i] == 'e' || s[i] == 'E' ||
                                s[i] == '+' || s[i] == '-')) {
            ++i; any = true;
        }
        if (!any) return fail("invalid number");
        out.type = Value::Type::Number;
        out.numVal = std::strtod(s.substr(start, i - start).c_str(), nullptr);
        return true;
    }
};

}  // namespace detail

// parse `text`; on failure returns a Null value and fills `err`.
inline Value parse(const std::string& text, std::string& err) {
    detail::Parser p(text);
    Value root;
    if (!p.parseValue(root)) { err = p.err; return Value(); }
    p.skipWs();
    if (p.i != text.size()) { err = "trailing characters at offset " + std::to_string(p.i); return Value(); }
    return root;
}

}  // namespace json

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  i18n-catalog-json.cpp

  Qore i18n native catalog JSON loader

  Copyright (C) 2026 Qore Technologies, s.r.o.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#include "i18n-module.h"

#include "qore/QoreFile.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <string>
#include <unordered_set>

namespace {

static constexpr int64 I18N_DEFAULT_MAX_CATALOG_FILE_LEN = 16 * 1024 * 1024;
static constexpr int64 I18N_MAX_CATALOG_FILE_LEN = 256 * 1024 * 1024;

static bool i18n_json_is_ws(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static int i18n_json_hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool i18n_json_append_utf8(std::string& out, uint32_t codepoint, ExceptionSink* xsink, size_t pos) {
    if (codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
        xsink->raiseException("I18N-CATALOG-JSON-ERROR",
            "invalid Unicode code point in JSON string at byte offset %zu", pos);
        return false;
    }

    if (codepoint <= 0x7f) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
    return true;
}

static bool i18n_json_validate_utf8(const std::string& value, ExceptionSink* xsink, const char* context) {
    for (size_t i = 0; i < value.size();) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "validating i18n catalog JSON UTF-8")) {
            return false;
        }

        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c <= 0x7f) {
            ++i;
            continue;
        }

        uint32_t codepoint = 0;
        size_t need = 0;
        if (c >= 0xc2 && c <= 0xdf) {
            codepoint = c & 0x1f;
            need = 1;
        } else if (c >= 0xe0 && c <= 0xef) {
            codepoint = c & 0x0f;
            need = 2;
        } else if (c >= 0xf0 && c <= 0xf4) {
            codepoint = c & 0x07;
            need = 3;
        } else {
            xsink->raiseException("I18N-CATALOG-JSON-ERROR",
                "%s contains invalid UTF-8 at byte offset %zu", context, i);
            return false;
        }

        if (i + need >= value.size()) {
            xsink->raiseException("I18N-CATALOG-JSON-ERROR",
                "%s contains truncated UTF-8 at byte offset %zu", context, i);
            return false;
        }

        for (size_t n = 1; n <= need; ++n) {
            unsigned char cc = static_cast<unsigned char>(value[i + n]);
            if ((cc & 0xc0) != 0x80) {
                xsink->raiseException("I18N-CATALOG-JSON-ERROR",
                    "%s contains invalid UTF-8 continuation byte at byte offset %zu", context, i + n);
                return false;
            }
            codepoint = (codepoint << 6) | (cc & 0x3f);
        }

        if ((need == 2 && codepoint < 0x800) || (need == 3 && codepoint < 0x10000)
            || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
            xsink->raiseException("I18N-CATALOG-JSON-ERROR",
                "%s contains invalid UTF-8 sequence at byte offset %zu", context, i);
            return false;
        }
        i += need + 1;
    }
    return true;
}

class I18nJsonParser {
public:
    DLLLOCAL I18nJsonParser(const char* data, size_t len, ExceptionSink* xsink) : data(data), len(len), xsink(xsink) {
    }

    DLLLOCAL QoreValue parse() {
        skipWs();
        ValueHolder value(parseValue(), xsink);
        if (*xsink) {
            return QoreValue();
        }
        skipWs();
        if (pos != len) {
            raise("unexpected trailing data");
            return QoreValue();
        }
        return value.release();
    }

private:
    const char* data = nullptr;
    size_t len = 0;
    size_t pos = 0;
    ExceptionSink* xsink = nullptr;

    DLLLOCAL void raise(const char* msg) {
        xsink->raiseException("I18N-CATALOG-JSON-ERROR", "%s at byte offset %zu", msg, pos);
    }

    DLLLOCAL void skipWs() {
        while (pos < len && i18n_json_is_ws(data[pos])) {
            ++pos;
        }
    }

    DLLLOCAL bool consume(char c) {
        if (pos < len && data[pos] == c) {
            ++pos;
            return true;
        }
        return false;
    }

    DLLLOCAL bool expect(char c, const char* msg) {
        if (consume(c)) {
            return true;
        }
        raise(msg);
        return false;
    }

    DLLLOCAL bool matchLiteral(const char* literal) {
        size_t literal_len = strlen(literal);
        if (pos + literal_len > len || memcmp(data + pos, literal, literal_len)) {
            return false;
        }
        pos += literal_len;
        return true;
    }

    DLLLOCAL bool parseHex4(uint32_t& out) {
        if (pos + 4 > len) {
            raise("truncated Unicode escape");
            return false;
        }

        uint32_t value = 0;
        for (size_t i = 0; i < 4; ++i) {
            int hv = i18n_json_hex_value(data[pos + i]);
            if (hv < 0) {
                raise("invalid Unicode escape");
                return false;
            }
            value = (value << 4) | static_cast<uint32_t>(hv);
        }
        pos += 4;
        out = value;
        return true;
    }

    DLLLOCAL QoreStringNode* parseString() {
        if (!expect('"', "expected JSON string")) {
            return nullptr;
        }

        std::string out;
        while (pos < len) {
            if (pos && !(pos % 100) && qore_check_cancel(xsink, "parsing i18n catalog JSON string")) {
                return nullptr;
            }

            unsigned char c = static_cast<unsigned char>(data[pos++]);
            if (c == '"') {
                if (!i18n_json_validate_utf8(out, xsink, "JSON string")) {
                    return nullptr;
                }
                return new QoreStringNode(out, QCS_UTF8);
            }
            if (c < 0x20) {
                raise("unescaped control character in JSON string");
                return nullptr;
            }
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (pos == len) {
                raise("truncated JSON string escape");
                return nullptr;
            }

            char esc = data[pos++];
            switch (esc) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(esc);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    uint32_t codepoint = 0;
                    if (!parseHex4(codepoint)) {
                        return nullptr;
                    }

                    if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                        if (pos + 2 > len || data[pos] != '\\' || data[pos + 1] != 'u') {
                            raise("high surrogate must be followed by a low surrogate");
                            return nullptr;
                        }
                        pos += 2;
                        uint32_t low = 0;
                        if (!parseHex4(low)) {
                            return nullptr;
                        }
                        if (low < 0xdc00 || low > 0xdfff) {
                            raise("high surrogate must be followed by a low surrogate");
                            return nullptr;
                        }
                        codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                        raise("low surrogate without preceding high surrogate");
                        return nullptr;
                    }

                    if (!i18n_json_append_utf8(out, codepoint, xsink, pos)) {
                        return nullptr;
                    }
                    break;
                }
                default:
                    raise("unsupported JSON string escape");
                    return nullptr;
            }
        }

        raise("unterminated JSON string");
        return nullptr;
    }

    DLLLOCAL QoreValue parseObject() {
        if (!expect('{', "expected JSON object")) {
            return QoreValue();
        }

        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
        std::unordered_set<std::string> keys;
        skipWs();
        if (consume('}')) {
            return h.release();
        }

        while (pos < len) {
            if (keys.size() && !(keys.size() % 100) && qore_check_cancel(xsink, "parsing i18n catalog JSON object")) {
                return QoreValue();
            }

            SimpleRefHolder<QoreStringNode> key(parseString());
            if (*xsink) {
                return QoreValue();
            }
            std::string key_text = key->c_str();
            if (!keys.insert(key_text).second) {
                xsink->raiseException("I18N-CATALOG-JSON-ERROR",
                    "duplicate object key '%s' at byte offset %zu", key_text.c_str(), pos);
                return QoreValue();
            }

            skipWs();
            if (!expect(':', "expected ':' after JSON object key")) {
                return QoreValue();
            }
            skipWs();

            ValueHolder value(parseValue(), xsink);
            if (*xsink) {
                return QoreValue();
            }
            h->setKeyValue(key_text.c_str(), value.release(), xsink);
            if (*xsink) {
                return QoreValue();
            }

            skipWs();
            if (consume('}')) {
                return h.release();
            }
            if (!expect(',', "expected ',' or '}' in JSON object")) {
                return QoreValue();
            }
            skipWs();
        }

        raise("unterminated JSON object");
        return QoreValue();
    }

    DLLLOCAL QoreValue parseArray() {
        if (!expect('[', "expected JSON array")) {
            return QoreValue();
        }

        ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
        skipWs();
        if (consume(']')) {
            return list.release();
        }

        while (pos < len) {
            if (list->size() && !(list->size() % 100) && qore_check_cancel(xsink, "parsing i18n catalog JSON array")) {
                return QoreValue();
            }

            ValueHolder value(parseValue(), xsink);
            if (*xsink) {
                return QoreValue();
            }
            list->push(value.release(), xsink);
            if (*xsink) {
                return QoreValue();
            }

            skipWs();
            if (consume(']')) {
                return list.release();
            }
            if (!expect(',', "expected ',' or ']' in JSON array")) {
                return QoreValue();
            }
            skipWs();
        }

        raise("unterminated JSON array");
        return QoreValue();
    }

    DLLLOCAL QoreValue parseNumber() {
        size_t start = pos;
        consume('-');

        if (pos == len) {
            raise("truncated JSON number");
            return QoreValue();
        }

        if (data[pos] == '0') {
            ++pos;
            if (pos < len && data[pos] >= '0' && data[pos] <= '9') {
                raise("JSON numbers cannot have leading zeroes");
                return QoreValue();
            }
        } else if (data[pos] >= '1' && data[pos] <= '9') {
            while (pos < len && data[pos] >= '0' && data[pos] <= '9') {
                ++pos;
            }
        } else {
            raise("expected JSON number");
            return QoreValue();
        }

        bool integral = true;
        if (pos < len && data[pos] == '.') {
            integral = false;
            ++pos;
            if (pos == len || data[pos] < '0' || data[pos] > '9') {
                raise("JSON number fraction must contain at least one digit");
                return QoreValue();
            }
            while (pos < len && data[pos] >= '0' && data[pos] <= '9') {
                ++pos;
            }
        }

        if (pos < len && (data[pos] == 'e' || data[pos] == 'E')) {
            integral = false;
            ++pos;
            if (pos < len && (data[pos] == '+' || data[pos] == '-')) {
                ++pos;
            }
            if (pos == len || data[pos] < '0' || data[pos] > '9') {
                raise("JSON number exponent must contain at least one digit");
                return QoreValue();
            }
            while (pos < len && data[pos] >= '0' && data[pos] <= '9') {
                ++pos;
            }
        }

        if (pos - start > 128) {
            raise("JSON number is too long");
            return QoreValue();
        }

        std::string text(data + start, pos - start);
        char* end = nullptr;
        errno = 0;
        if (integral) {
            long long value = std::strtoll(text.c_str(), &end, 10);
            if (errno == ERANGE || !end || *end) {
                xsink->raiseException("I18N-CATALOG-JSON-ERROR",
                    "integer JSON number out of range at byte offset %zu", start);
                return QoreValue();
            }
            return static_cast<int64>(value);
        }

        double value = std::strtod(text.c_str(), &end);
        if (errno == ERANGE || !end || *end || !std::isfinite(value)) {
            xsink->raiseException("I18N-CATALOG-JSON-ERROR",
                "floating-point JSON number out of range at byte offset %zu", start);
            return QoreValue();
        }
        return value;
    }

    DLLLOCAL QoreValue parseValue() {
        if (pos == len) {
            raise("expected JSON value");
            return QoreValue();
        }

        switch (data[pos]) {
            case '{':
                return parseObject();
            case '[':
                return parseArray();
            case '"':
                return parseString();
            case 't':
                if (matchLiteral("true")) {
                    return true;
                }
                break;
            case 'f':
                if (matchLiteral("false")) {
                    return false;
                }
                break;
            case 'n':
                if (matchLiteral("null")) {
                    return QoreValue();
                }
                break;
            default:
                if (data[pos] == '-' || (data[pos] >= '0' && data[pos] <= '9')) {
                    return parseNumber();
                }
                break;
        }

        raise("expected JSON value");
        return QoreValue();
    }
};

static int64 i18n_catalog_checked_max_file_len(int64 max_file_len, ExceptionSink* xsink) {
    if (max_file_len < 1 || max_file_len > I18N_MAX_CATALOG_FILE_LEN) {
        xsink->raiseException("I18N-CATALOG-ERROR",
            "max_file_len must be between 1 and %lld bytes", I18N_MAX_CATALOG_FILE_LEN);
        return 0;
    }
    return max_file_len;
}

} // namespace

QoreHashNode* i18n_parse_native_catalog_json(const QoreStringNode* json, ExceptionSink* xsink) {
    TempEncodingHelper tmp(json, QCS_UTF8, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (!tmp) {
        xsink->raiseException("I18N-CATALOG-JSON-ERROR", "JSON catalog data could not be converted to UTF-8");
        return nullptr;
    }

    I18nJsonParser parser(tmp->c_str(), tmp->size(), xsink);
    ValueHolder root(parser.parse(), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (root->getType() != NT_HASH) {
        xsink->raiseException("I18N-CATALOG-JSON-ERROR", "native catalog JSON root must be an object");
        return nullptr;
    }
    return root.releaseAs<QoreHashNode>();
}

QoreHashNode* i18n_load_native_catalog_json(const QoreStringNode* path, int64 max_file_len, ExceptionSink* xsink) {
    max_file_len = max_file_len < 0 ? I18N_DEFAULT_MAX_CATALOG_FILE_LEN
        : i18n_catalog_checked_max_file_len(max_file_len, xsink);
    if (*xsink) {
        return nullptr;
    }

    TempEncodingHelper npath(path, QCS_DEFAULT, xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreFile qf;
    if (qf.open2(xsink, npath->c_str(), O_RDONLY, 0777, QCS_UTF8)) {
        return nullptr;
    }

    ReferenceHolder<QoreStringNode> data(qf.read(max_file_len + 1, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (!data) {
        data = new QoreStringNode(QCS_UTF8);
    }
    if (data->size() > static_cast<size_t>(max_file_len)) {
        xsink->raiseException("I18N-CATALOG-ERROR",
            "native catalog file '%s' exceeds the maximum allowed size of %lld bytes", npath->c_str(), max_file_len);
        return nullptr;
    }

    return i18n_parse_native_catalog_json(*data, xsink);
}

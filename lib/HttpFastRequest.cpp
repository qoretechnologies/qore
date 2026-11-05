/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HttpFastRequest.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include "qore/intern/HttpFastRequest.h"

#include <cstring>
#include <string_view>

// a named namespace: the library is also built as one compilation unit, where the helpers of other files would collide
namespace http_fast_request_detail {

// the characters of a request target that the parser accepts: unreserved and sub-delimiters, the path and query
// separators, and nothing that needs encoding; in particular no percent sign, so that the path means the same to every
// parser
inline bool fastreq_target_char(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return true;
    }
    switch (c) {
        case '-': case '.': case '_': case '~': case '/': case '?': case '&': case '=': case '+': case ',':
        case ';': case ':': case '@': case '!': case '$': case '\'': case '(': case ')': case '*':
            return true;
        default:
            return false;
    }
}

// a character of a header name (a token, RFC 9110 section 5.6.2)
inline bool fastreq_token_char(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return true;
    }
    switch (c) {
        case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+': case '-': case '.':
        case '^': case '_': case '`': case '|': case '~':
            return true;
        default:
            return false;
    }
}

// a character of a header value: visible ASCII, space and horizontal tab (no control character, no NUL, no byte over
// 0x7e)
inline bool fastreq_value_char(unsigned char c) {
    return (c >= 0x20 && c <= 0x7e) || c == '\t';
}

inline bool fastreq_ows(char c) {
    return c == ' ' || c == '\t';
}

inline char fastreq_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// compares a name with a lower case literal, ignoring case
inline bool fastreq_iequals(std::string_view a, std::string_view lower_literal) {
    if (a.size() != lower_literal.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (fastreq_lower(a[i]) != lower_literal[i]) {
            return false;
        }
    }
    return true;
}

// the headers the parser tracks, as bits of the "seen" mask
enum : unsigned {
    SeenContentLength = 1,
    SeenConnection = 2,
    SeenHost = 4,
    SeenTransferEncoding = 8,
    SeenExpect = 16,
    SeenUpgrade = 32,
    SeenHttp2Settings = 64,
};

} // namespace http_fast_request_detail

bool HttpFastRequestParser::parse(const char* data, size_t size, HttpFastRequestView& view) {
    using namespace http_fast_request_detail;
    // "GET / HTTP/1.1" and the empty line
    if (size < 18 || size > MaxHeaderSize) {
        return false;
    }
    const char* const end = data + size;
    if (memcmp(end - 4, "\r\n\r\n", 4)) {
        return false;
    }

    // request line: exactly GET <target> HTTP/1.1
    if (memcmp(data, "GET /", 5)) {
        return false;
    }
    const char* target = data + 4;
    const char* t = target;
    while (t < end && fastreq_target_char(static_cast<unsigned char>(*t))) {
        ++t;
    }
    if (static_cast<size_t>(t - target) > MaxTargetSize) {
        return false;
    }
    static const char tail[] = " HTTP/1.1\r\n";
    constexpr size_t tail_size = sizeof(tail) - 1;
    if (static_cast<size_t>(end - t) < tail_size || memcmp(t, tail, tail_size)) {
        return false;
    }

    HttpFastRequestView v;
    v.method = std::string_view(data, 3);
    v.path = std::string_view(target, static_cast<size_t>(t - target));
    v.http_version = std::string_view("1.1");

    unsigned seen = 0;
    size_t count = 0;
    const char* line = t + tail_size;
    while (true) {
        if (end - line < 2) {
            return false;
        }
        if (line[0] == '\r') {
            // the empty line must be the last bytes of the header, and the request must have a Host header
            if (line[1] != '\n' || line + 2 != end || !(seen & SeenHost)) {
                return false;
            }
            view = v;
            return true;
        }
        if (++count > MaxHeaders) {
            return false;
        }

        // the name: a token (a line that starts with whitespace is an obsolete fold: rejected as it is not a token),
        // then the colon with no whitespace before it
        const char* n = line;
        while (n < end && fastreq_token_char(static_cast<unsigned char>(*n))) {
            ++n;
        }
        if (n == line || n >= end || *n != ':') {
            return false;
        }
        const std::string_view name(line, static_cast<size_t>(n - line));

        // the value: without the whitespace around it, of visible characters
        const char* val = n + 1;
        while (val < end && fastreq_ows(*val)) {
            ++val;
        }
        const char* e = val;
        while (e < end && fastreq_value_char(static_cast<unsigned char>(*e))) {
            ++e;
        }
        // the line must end here with CRLF: anything else (a CR or LF alone, a control character, a byte over 0x7e, a
        // NUL) is rejected
        if (end - e < 2 || e[0] != '\r' || e[1] != '\n') {
            return false;
        }
        const char* val_end = e;
        while (val_end > val && fastreq_ows(val_end[-1])) {
            --val_end;
        }
        const std::string_view value(val, static_cast<size_t>(val_end - val));

        // the headers that matter; a repeated one is rejected, as the normal parser would combine them
        unsigned bit = 0;
        switch (name.size()) {
            case 4:
                if (fastreq_iequals(name, "host")) {
                    bit = SeenHost;
                }
                break;
            case 6:
                if (fastreq_iequals(name, "expect")) {
                    bit = SeenExpect;
                }
                break;
            case 7:
                if (fastreq_iequals(name, "upgrade")) {
                    bit = SeenUpgrade;
                }
                break;
            case 10:
                if (fastreq_iequals(name, "connection")) {
                    bit = SeenConnection;
                }
                break;
            case 14:
                if (fastreq_iequals(name, "content-length")) {
                    bit = SeenContentLength;
                } else if (fastreq_iequals(name, "http2-settings")) {
                    bit = SeenHttp2Settings;
                }
                break;
            case 17:
                if (fastreq_iequals(name, "transfer-encoding")) {
                    bit = SeenTransferEncoding;
                }
                break;
            default:
                break;
        }
        if (bit) {
            if (seen & bit) {
                return false;
            }
            seen |= bit;
            switch (bit) {
                case SeenHost:
                    if (value.empty()) {
                        return false;
                    }
                    break;
                case SeenConnection:
                    if (value.empty()) {
                        return false;
                    }
                    v.connection = value;
                    break;
                case SeenContentLength: {
                    // digits only, so that the value means the same to every parser
                    if (value.empty() || value.size() > 10) {
                        return false;
                    }
                    for (char c : value) {
                        if (c < '0' || c > '9') {
                            return false;
                        }
                    }
                    v.content_length = value;
                    break;
                }
                case SeenTransferEncoding:
                    v.has_transfer_encoding = true;
                    break;
                case SeenExpect:
                    v.has_expect = true;
                    break;
                case SeenUpgrade:
                case SeenHttp2Settings:
                    v.has_upgrade = true;
                    break;
                default:
                    break;
            }
        }
        line = e + 2;
    }
}

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttpHeaderPairs.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREHTTPHEADERPAIRS_H
#define _QORE_INTERN_QOREHTTPHEADERPAIRS_H

#include <qore/common.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <strings.h>
#include <utility>
#include <vector>

class ExceptionSink;
class QoreHashNode;

//! HTTP header fields as name/value pairs in order
/** A name can occur more than once: a header with several values, such as \c Set-Cookie, is one field per value
    and must never be combined into one field.  HTTP/2 and HTTP/3 header blocks are built from this type.
*/
typedef std::vector<std::pair<std::string, std::string>> qore_http_header_pairs_t;

//! Appends the header fields of a %Qore header hash
/** A list value gives one field per element; string, integer, float, number, and boolean values are formatted as
    for HTTP/1.x messages, and values of other types are ignored, also as for HTTP/1.x messages.

    @param headers the header hash; may be nullptr
    @param out the fields are appended here
*/
DLLLOCAL void qore_get_http_header_pairs(const QoreHashNode* headers, qore_http_header_pairs_t& out);

//! Builds an HTTP/2 request header block and extracts the required method and path; returns -1 on error.
DLLLOCAL int qore_get_http2_request_headers(const QoreHashNode* headers, std::string& method, std::string& path,
        qore_http_header_pairs_t& out, ExceptionSink* xsink);

//! Parses a \c Content-Length field value: decimal digits with optional surrounding whitespace (RFC 9110 section 8.6)
/** @return the length, or -1 if the value is not a valid length (a value that does not fit in an int64)
*/
DLLLOCAL inline int64 qore_http_parse_content_length(const char* value, size_t vlen) {
    size_t i = 0;
    while (i < vlen && (value[i] == ' ' || value[i] == '\t')) {
        ++i;
    }
    while (vlen > i && (value[vlen - 1] == ' ' || value[vlen - 1] == '\t')) {
        --vlen;
    }
    if (i == vlen) {
        return -1;
    }
    int64 v = 0;
    for (; i < vlen; ++i) {
        if (value[i] < '0' || value[i] > '9') {
            return -1;
        }
        int64 d = value[i] - '0';
        if (v > (INT64_MAX - d) / 10) {
            return -1;
        }
        v = v * 10 + d;
    }
    return v;
}

//! The body rules of an HTTP/2 or HTTP/3 server response, which the client's HTTP layer enforces (qore#7212)
/** A client treats a response that breaks them as malformed: with HTTP/3, the client's nghttp3 closes the whole
    connection with every other stream on it (a malformed message cannot be confined to its stream with nghttp3), so
    a server must never send one: with these rules, it refuses or corrects the response of a handler instead.

    - more than one \c Content-Length field, or one that is not a length: refused;
    - a 204 response with a \c Content-Length other than 0: refused;
    - a response to HEAD, or with status 204 or 304, has no content: a body given for it is not sent;
    - otherwise the content sent must have the declared length: a complete body of another length is refused, and a
      streamed body that turns out longer or shorter ends the stream with a reset (a stream error) instead of a
      normal end.

    Add the response's header fields with add(), then call check().
*/
class QoreHttpResponseBodyRule {
public:
    //! Creates the object for a response with the given status to a request with the given method
    DLLLOCAL QoreHttpResponseBodyRule(int status, const char* method)
            : status(status), no_body((method && !strcmp(method, "HEAD")) || status == 204 || status == 304) {
    }

    //! Adds a header field; the name is compared case-insensitively
    DLLLOCAL void add(const char* name, size_t nlen, const char* value, size_t vlen) {
        if (nlen != 14 || strncasecmp(name, "content-length", 14)) {
            return;
        }
        ++count;
        int64 v = qore_http_parse_content_length(value, vlen);
        if (v < 0) {
            invalid.assign(value, vlen);
            return;
        }
        content_length = v;
    }

    //! Returns an empty string if the response is valid, otherwise why it is not
    /** @param body_len the length of the complete body, or -1 for a body that is streamed
    */
    DLLLOCAL std::string check(int64 body_len) const {
        if (count > 1) {
            return "the response has " + std::to_string(count) + " content-length fields";
        }
        if (!invalid.empty() || (count && content_length < 0)) {
            return "the content-length of the response ('" + invalid + "') is not a valid length";
        }
        if (status == 204 && count && content_length) {
            return "a 204 response has a content-length of " + std::to_string(content_length) + ", not 0";
        }
        if (!no_body && count && body_len >= 0 && content_length != body_len) {
            return "the content-length of the response (" + std::to_string(content_length)
                + ") does not match the length of its body (" + std::to_string(body_len) + " bytes)";
        }
        return std::string();
    }

    //! Returns true if no content may be sent with the response (a response to HEAD, or with status 204 or 304)
    DLLLOCAL bool noBody() const {
        return no_body;
    }

    //! Returns the length that a streamed body must have: 0 without content, the declared length, or -1
    DLLLOCAL int64 streamLength() const {
        return no_body ? 0 : (count == 1 ? content_length : -1);
    }

private:
    int status;
    bool no_body;
    int count = 0;
    int64 content_length = -1;
    std::string invalid;
};

//! Returns the value of the first field with the given name, compared case-insensitively, or nullptr
DLLLOCAL inline const std::string* qore_find_http_header(const qore_http_header_pairs_t& headers,
        const char* name) {
    for (const auto& h : headers) {
        if (!strcasecmp(h.first.c_str(), name)) {
            return &h.second;
        }
    }
    return nullptr;
}

//! A media-type parameter found in a \c Content-Type value
struct qore_http_media_type_param {
    //! the parameter value, with any quoted-string unquoted
    std::string value;
    //! the offset of the \c ';' that begins the parameter
    size_t begin = 0;
    //! the offset of the \c ';' that ends the parameter, or the length of the field value for the last parameter
    size_t end = 0;
};

//! Finds a media-type parameter in a \c Content-Type value (RFC 9110 sections 5.6.6 and 8.3.1)
/** Parameters follow the type and subtype, each after a \c ';'; parameter names are compared case-insensitively
    and only whole names match, so \c "xcharset" is not \c "charset".  A quoted-string value is unquoted, with
    quoted-pairs (\c "\\x") decoded, and a \c ';' inside it does not end the parameter.

    Whitespace around the \c '=' is accepted although RFC 9110 does not allow it, and a token value enclosed in
    single quotes is unwrapped, since some senders quote that way and a single quote is never part of a charset
    name.

    @param content_type the \c Content-Type field value; may be nullptr
    @param name the parameter name to find
    @param param the parameter found; set only if the return value is true

    @return true if the parameter was found; the first one is returned if the name occurs more than once
*/
DLLLOCAL bool qore_find_http_media_type_param(const char* content_type, const char* name,
        qore_http_media_type_param& param);

//! Returns the media type of a \c Content-Type value: the type and subtype, lower case, without parameters
DLLLOCAL inline std::string qore_http_media_type(const char* content_type) {
    if (!content_type) {
        return std::string();
    }
    std::string rv = content_type;
    size_t semicolon = rv.find(';');
    if (semicolon != std::string::npos) {
        rv.resize(semicolon);
    }
    while (!rv.empty() && isspace(static_cast<unsigned char>(rv.back()))) {
        rv.pop_back();
    }
    while (!rv.empty() && isspace(static_cast<unsigned char>(rv.front()))) {
        rv.erase(0, 1);
    }
    for (char& c : rv) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return rv;
}

#endif

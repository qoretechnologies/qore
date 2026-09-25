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

#include <cstring>
#include <string>
#include <strings.h>
#include <utility>
#include <vector>

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

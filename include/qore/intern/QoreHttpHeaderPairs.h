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

//! Returns true if a media type carries text, so a message body with it is delivered as a string
/** The media type decides how a message body is delivered: a text type becomes a string in the character
    encoding the message declares, and any other type stays binary, because its octets are not text and
    converting them would corrupt them.

    An empty media type is not text.  A message that declares no type has no media type to judge, which is a
    different question from a type that is not text, and each caller answers it for itself.

    @param media_type a media type from qore_http_media_type()
*/
DLLLOCAL inline bool qore_http_media_type_is_text(const std::string& media_type) {
    return (media_type.size() >= 5 && !media_type.compare(0, 5, "text/"))
        || media_type == "application/json"
        || media_type == "application/xml"
        || media_type == "application/javascript"
        || media_type == "application/x-www-form-urlencoded"
        || media_type == "application/x-yaml"
        || media_type == "application/yaml"
        || (media_type.size() > 5 && !media_type.compare(media_type.size() - 5, 5, "+json"))
        || (media_type.size() > 4 && !media_type.compare(media_type.size() - 4, 4, "+xml"));
}

//! Returns true if a media type is always UTF-8, whatever charset parameter it carries
/** JSON is UTF-8 by RFC 8259 section 8.1, and YAML by the YAML specification
*/
DLLLOCAL inline bool qore_http_media_type_is_utf8(const std::string& media_type) {
    return media_type == "application/json"
        || (media_type.size() > 5 && !media_type.compare(media_type.size() - 5, 5, "+json"))
        || media_type == "application/x-yaml" || media_type == "text/yaml"
        || media_type == "text/x-yaml" || media_type == "application/yaml";
}

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttpBodyCharset.h

    Character encoding of HTTP message bodies

    Qore Programming Language

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREHTTPBODYCHARSET_H
#define _QORE_INTERN_QOREHTTPBODYCHARSET_H

#include <cstddef>
#include <string>

class QoreEncoding;
class BinaryNode;
class QoreStringNode;
class SimpleValueQoreNode;

//! The kind of an HTTP message body, which decides how its character encoding is determined
/** See design/http-body-charset.md for the rules and the specifications they follow.
*/
enum class QoreHttpBodyKind {
    //! Not text: the body is delivered as binary data
    Binary,
    //! JSON (\c application/json, \c +json): always UTF-8 (RFC 8259)
    Json,
    //! YAML (\c application/yaml, \c +yaml): a BOM, else UTF-8 (RFC 9512)
    Yaml,
    //! XML (\c application/xml, \c text/xml, \c +xml): a BOM, the charset parameter, the XML declaration, else
    //! UTF-8 (RFC 7303)
    Xml,
    //! HTML (\c text/html): a BOM, the charset parameter, a \c meta element, else the assumed encoding (WHATWG HTML
    //! encoding sniffing)
    Html,
    //! JavaScript: a BOM, the charset parameter, else UTF-8 (RFC 9239)
    Javascript,
    //! CSS (\c text/css): a BOM, the charset parameter, an \c \@charset rule, else UTF-8 (CSS Syntax)
    Css,
    //! Server-sent events (\c text/event-stream): always UTF-8 (WHATWG HTML)
    EventStream,
    //! Form data (\c application/x-www-form-urlencoded): a BOM, the charset parameter, else UTF-8 (WHATWG URL)
    Form,
    //! Any other text: a BOM, the charset parameter, else the assumed encoding (ISO-8859-1 by default, RFC 2616)
    Text,
};

//! The character encoding determined for an HTTP message body
struct QoreHttpBodyCharset {
    //! The encoding of the body, or nullptr if the body is not text
    const QoreEncoding* enc = nullptr;
    //! The size of the byte order mark at the start of the body, which is not part of the text
    size_t bom_len = 0;
};

//! Returns the kind of an HTTP message body from its \c Content-Type field value
/** A type that is not recognized as text is text only if it has a \c charset parameter and is not binary by
    definition (\c application/octet-stream, \c image/, \c audio/, \c video/, \c font/); without a
    \c Content-Type, the kind depends on the data (see qore_get_http_body_charset()), and this function returns
    QoreHttpBodyKind::Binary.

    @param content_type the \c Content-Type field value; may be nullptr
*/
DLLLOCAL QoreHttpBodyKind qore_http_body_kind(const char* content_type);

//! Returns the character encoding of an HTTP message body
/** A message without a \c Content-Type is text if its data is text according to the WHATWG MIME Sniffing rules
    (a byte order mark or no binary data bytes in the first 1445 bytes), as RFC 9110 section 8.3 allows.

    @param content_type the \c Content-Type field value; may be nullptr
    @param data the start of the body; may be nullptr if \a len is 0
    @param len the number of bytes available at \a data; at most the first 1445 bytes are examined
    @param assumed the encoding assumed for text whose encoding is not determined otherwise; nullptr for
    ISO-8859-1

    @return the encoding and the size of any byte order mark; the encoding is nullptr if the body is not text
*/
DLLLOCAL QoreHttpBodyCharset qore_get_http_body_charset(const char* content_type, const void* data, size_t len,
        const QoreEncoding* assumed);

//! Returns the text of an HTTP message body in the given character encoding, without any byte order mark
/** The bytes are tagged with the encoding; they are not converted.

    @param charset the character encoding returned by qore_get_http_body_charset(), which must be text
    @param data the body
    @param len the size of the body
*/
DLLLOCAL QoreStringNode* qore_http_body_string(const QoreHttpBodyCharset& charset, const void* data, size_t len);

//! Decodes an HTTP message body: returns a string for text, or a reference to the binary body otherwise
/** @param content_type the \c Content-Type field value; may be nullptr
    @param body the body
    @param assumed the encoding assumed for text whose encoding is not determined otherwise; nullptr for
    ISO-8859-1

    @return a new string for text, or a new reference to \a body
*/
DLLLOCAL SimpleValueQoreNode* qore_decode_http_body(const char* content_type, const BinaryNode* body,
        const QoreEncoding* assumed);

//! Returns the default encoding for a body of the given kind before its data is known, from the charset parameter
/** Used where only the header is available: the result is the charset parameter if it applies to the kind, else
    the default of the kind (UTF-8 or \a assumed); nullptr if the body is not text.

    @param content_type the \c Content-Type field value; may be nullptr
    @param assumed the encoding assumed for text whose encoding is not determined otherwise; nullptr for
    ISO-8859-1
*/
DLLLOCAL const QoreEncoding* qore_get_http_header_charset(const char* content_type, const QoreEncoding* assumed);

#endif

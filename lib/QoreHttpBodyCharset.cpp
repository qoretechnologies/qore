/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttpBodyCharset.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreHttpBodyCharset.h"
#include "qore/intern/qore_string_private.h"
#include "qore/intern/QoreHttpHeaderPairs.h"
#include "qore/intern/qore_encoding_private.h"

#include <cctype>
#include <iconv.h>
#include <cstring>
#include <string>

// the number of bytes examined when sniffing an encoding declaration in the body (WHATWG HTML "prescan")
static constexpr size_t HTTP_BODY_SNIFF_LEN = 1024;

// the number of bytes examined to tell text from binary data (WHATWG MIME Sniffing "resource header")
static constexpr size_t HTTP_BODY_RESOURCE_HEADER_LEN = 1445;

// returns true if the data is text according to the WHATWG MIME Sniffing rules for distinguishing whether a resource
// is text or binary (section 7.2): a byte order mark, or no binary data bytes in the resource header
static bool is_text_data(const unsigned char* p, size_t len) {
    if (len >= 2 && ((p[0] == 0xfe && p[1] == 0xff) || (p[0] == 0xff && p[1] == 0xfe))) {
        return true;
    }
    if (len >= 3 && p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf) {
        return true;
    }
    if (len > HTTP_BODY_RESOURCE_HEADER_LEN) {
        len = HTTP_BODY_RESOURCE_HEADER_LEN;
    }
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = p[i];
        if (c <= 0x08 || c == 0x0b || (c >= 0x0e && c <= 0x1a) || (c >= 0x1c && c <= 0x1f)) {
            return false;
        }
    }
    return true;
}

static bool ends_with(const std::string& str, const char* suffix) {
    size_t len = strlen(suffix);
    return str.size() > len && !str.compare(str.size() - len, len, suffix);
}

static bool starts_with(const std::string& str, const char* prefix) {
    return !str.compare(0, strlen(prefix), prefix);
}

// returns true if the media type is that of a body of length-prefixed message envelopes: a Connect stream
// ("application/connect+<codec>", also "application/connect+datagram+<codec>") or gRPC ("application/grpc",
// "application/grpc+<codec>") and gRPC-Web ("application/grpc-web", "application/grpc-web+<codec>"); the suffix names
// the codec of the messages, not the format of the body, whose length prefixes and compressed messages are binary
// data, so that a "+json", "+yaml" or "+xml" suffix does not make it text
static bool is_envelope_media_type(const std::string& media_type) {
    return starts_with(media_type, "application/connect+")
        || media_type == "application/grpc" || starts_with(media_type, "application/grpc+")
        || media_type == "application/grpc-web" || starts_with(media_type, "application/grpc-web+");
}

static QoreHttpBodyKind http_media_type_kind(const std::string& media_type) {
    if (is_envelope_media_type(media_type)) {
        return QoreHttpBodyKind::Binary;
    }
    if (media_type == "application/json" || media_type == "text/json" || ends_with(media_type, "+json")) {
        return QoreHttpBodyKind::Json;
    }
    if (media_type == "application/yaml" || media_type == "application/x-yaml" || media_type == "text/yaml"
            || media_type == "text/x-yaml" || ends_with(media_type, "+yaml")) {
        return QoreHttpBodyKind::Yaml;
    }
    if (media_type == "application/xml" || media_type == "text/xml" || ends_with(media_type, "+xml")) {
        return QoreHttpBodyKind::Xml;
    }
    if (media_type == "text/html") {
        return QoreHttpBodyKind::Html;
    }
    if (media_type == "text/javascript" || media_type == "application/javascript"
            || media_type == "application/x-javascript" || media_type == "application/ecmascript"
            || media_type == "text/ecmascript") {
        return QoreHttpBodyKind::Javascript;
    }
    if (media_type == "text/css") {
        return QoreHttpBodyKind::Css;
    }
    if (media_type == "text/event-stream") {
        return QoreHttpBodyKind::EventStream;
    }
    if (media_type == "application/x-www-form-urlencoded") {
        return QoreHttpBodyKind::Form;
    }
    if (media_type.size() > 5 && !media_type.compare(0, 5, "text/")) {
        return QoreHttpBodyKind::Text;
    }
    return QoreHttpBodyKind::Binary;
}

// returns true if the media type is binary by definition, so that a charset parameter does not make it text
static bool is_binary_media_type(const std::string& media_type) {
    return media_type == "application/octet-stream"
        || is_envelope_media_type(media_type)
        || !media_type.compare(0, 6, "image/")
        || !media_type.compare(0, 6, "audio/")
        || !media_type.compare(0, 6, "video/")
        || !media_type.compare(0, 5, "font/");
}

QoreHttpBodyKind qore_http_body_kind(const char* content_type) {
    if (!content_type) {
        return QoreHttpBodyKind::Binary;
    }
    std::string media_type = qore_http_media_type(content_type);
    QoreHttpBodyKind kind = http_media_type_kind(media_type);
    if (kind != QoreHttpBodyKind::Binary || is_binary_media_type(media_type)) {
        return kind;
    }
    // a type that is not known to be text is text if the sender declares its character encoding
    qore_http_media_type_param charset;
    if (qore_find_http_media_type_param(content_type, "charset", charset) && !charset.value.empty()) {
        return QoreHttpBodyKind::Text;
    }
    return QoreHttpBodyKind::Binary;
}

// returns the encoding given by a byte order mark at the start of the data, and sets its size
static const QoreEncoding* get_bom_encoding(const unsigned char* p, size_t len, bool utf32, size_t& bom_len) {
    if (utf32 && len >= 4) {
        if (p[0] == 0 && p[1] == 0 && p[2] == 0xfe && p[3] == 0xff) {
            bom_len = 4;
            return QEM.findCreate("UTF-32BE");
        }
        if (p[0] == 0xff && p[1] == 0xfe && p[2] == 0 && p[3] == 0) {
            bom_len = 4;
            return QEM.findCreate("UTF-32LE");
        }
    }
    if (len >= 3 && p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf) {
        bom_len = 3;
        return QCS_UTF8;
    }
    if (len >= 2) {
        if (p[0] == 0xfe && p[1] == 0xff) {
            bom_len = 2;
            return QCS_UTF16BE;
        }
        if (p[0] == 0xff && p[1] == 0xfe) {
            bom_len = 2;
            return QCS_UTF16LE;
        }
    }
    return nullptr;
}

// returns the encoding for a charset name from a message (a Content-Type value), or nullptr if the name is empty or
// selects no encoding: the name is untrusted data, which may select only encodings that Qore knows or a standard
// character set, so that it never makes the registry of encodings grow without limit (qore#6460)
static const QoreEncoding* get_named_encoding(const std::string& name) {
    return qore_find_untrusted_encoding(name.data(), name.size());
}

// returns true if iconv converts text to the encoding, by the name that Qore converts with
static bool qore_iconv_converts_to(const QoreEncoding* enc) {
    iconv_t cd = iconv_open(qore_encoding_private::get(*enc)->getIconvTargetCode(), "UTF-8");
    if (cd == (iconv_t)-1) {
        return false;
    }
    iconv_close(cd);
    return true;
}

// returns the encoding for a name declared in the body (an XML declaration, a meta element, or an @charset rule), or
// nullptr if the name is empty or not a supported encoding, in which case the declaration is ignored (WHATWG Encoding
// "get an encoding"): the name is untrusted data, looked up as in get_named_encoding() (qore#6460), and the encoding
// must be one that iconv converts to
static const QoreEncoding* get_supported_encoding(const std::string& name) {
    const QoreEncoding* enc = qore_find_untrusted_encoding(name.data(), name.size());
    return enc && qore_iconv_converts_to(enc) ? enc : nullptr;
}

// returns the value of the charset parameter of a Content-Type value, if any
static std::string get_charset_param(const char* content_type) {
    qore_http_media_type_param charset;
    if (content_type && qore_find_http_media_type_param(content_type, "charset", charset)) {
        return charset.value;
    }
    return std::string();
}

// returns the lower case version of the given data, for case-insensitive searches
static std::string to_lower(const unsigned char* p, size_t len) {
    std::string rv(reinterpret_cast<const char*>(p), len);
    for (char& c : rv) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return rv;
}

// an attribute of a markup tag or declaration; the name is in lower case
struct markup_attribute {
    std::string name;
    std::string value;
};

// returns the next attribute in the markup at \a pos, which excludes the closing '>' or '?>', following the WHATWG
// "get an attribute" algorithm; returns false when there are no more attributes
static bool get_next_attribute(const std::string& text, size_t& pos, markup_attribute& attr) {
    auto is_space = [](char c) -> bool {
        return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
    };
    while (pos < text.size() && (is_space(text[pos]) || text[pos] == '/')) {
        ++pos;
    }
    if (pos >= text.size()) {
        return false;
    }
    attr.name.clear();
    attr.value.clear();
    // the name: up to '=', whitespace or '/'
    while (pos < text.size() && text[pos] != '=' && !is_space(text[pos]) && text[pos] != '/') {
        attr.name += static_cast<char>(tolower(static_cast<unsigned char>(text[pos])));
        ++pos;
    }
    while (pos < text.size() && is_space(text[pos])) {
        ++pos;
    }
    if (pos >= text.size() || text[pos] != '=') {
        // an attribute without a value
        return true;
    }
    ++pos;
    while (pos < text.size() && is_space(text[pos])) {
        ++pos;
    }
    if (pos >= text.size()) {
        return true;
    }
    char quote = text[pos];
    if (quote == '"' || quote == '\'') {
        size_t close = text.find(quote, pos + 1);
        if (close == std::string::npos) {
            attr.value = text.substr(pos + 1);
            pos = text.size();
        } else {
            attr.value = text.substr(pos + 1, close - pos - 1);
            pos = close + 1;
        }
        return true;
    }
    size_t value_end = pos;
    while (value_end < text.size() && !is_space(text[value_end])) {
        ++value_end;
    }
    attr.value = text.substr(pos, value_end - pos);
    pos = value_end;
    return true;
}

// returns the encoding of an XML declaration at the start of the body (XML 1.0 section 4.3.3)
static std::string get_xml_declaration_encoding(const unsigned char* p, size_t len) {
    if (len < 6 || memcmp(p, "<?xml", 5) || !isspace(p[5])) {
        return std::string();
    }
    std::string decl(reinterpret_cast<const char*>(p), len);
    size_t end = decl.find("?>");
    if (end == std::string::npos) {
        return std::string();
    }
    decl.resize(end);
    size_t pos = 5;
    markup_attribute attr;
    while (get_next_attribute(decl, pos, attr)) {
        if (attr.name == "encoding") {
            return attr.value;
        }
    }
    return std::string();
}

// the WHATWG rule for an encoding declared inside a document: a UTF-16 declaration means UTF-8, as the document
// could not have been read to find it otherwise
static const QoreEncoding* get_declared_encoding(const std::string& name) {
    const QoreEncoding* enc = get_supported_encoding(name);
    if (enc && (enc == QCS_UTF16 || enc == QCS_UTF16BE || enc == QCS_UTF16LE)) {
        return QCS_UTF8;
    }
    return enc;
}

// returns the encoding in the value of the content attribute of a meta element (the WHATWG "algorithm for
// extracting a character encoding from a meta element")
static std::string get_meta_content_charset(const std::string& content) {
    std::string text = to_lower(reinterpret_cast<const unsigned char*>(content.data()), content.size());
    size_t pos = 0;
    while ((pos = text.find("charset", pos)) != std::string::npos) {
        pos += 7;
        size_t i = pos;
        while (i < text.size() && isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        if (i >= text.size() || text[i] != '=') {
            // not a charset declaration: look for the next one from here
            continue;
        }
        ++i;
        while (i < text.size() && isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        if (i >= text.size()) {
            return std::string();
        }
        char quote = text[i];
        if (quote == '"' || quote == '\'') {
            size_t close = text.find(quote, i + 1);
            return close == std::string::npos ? std::string() : content.substr(i + 1, close - i - 1);
        }
        size_t end = i;
        while (end < text.size() && !isspace(static_cast<unsigned char>(text[end])) && text[end] != ';') {
            ++end;
        }
        return content.substr(i, end - i);
    }
    return std::string();
}

// returns the encoding given by a meta element in the start of an HTML document, or nullptr if there is none (a
// simplified WHATWG "prescan a byte stream to determine its encoding": comments are skipped, other tags are skipped
// to their end, and a declaration of an unsupported encoding is ignored)
static const QoreEncoding* get_html_meta_encoding(const unsigned char* p, size_t len) {
    std::string text(reinterpret_cast<const char*>(p), len);
    std::string lower = to_lower(p, len);
    size_t pos = 0;
    while ((pos = lower.find('<', pos)) != std::string::npos) {
        if (!lower.compare(pos, 4, "<!--")) {
            size_t end = lower.find("-->", pos + 4);
            if (end == std::string::npos) {
                break;
            }
            pos = end + 3;
            continue;
        }
        size_t end = lower.find('>', pos);
        if (end == std::string::npos) {
            break;
        }
        if (!lower.compare(pos, 5, "<meta") && pos + 5 < end
                && (isspace(static_cast<unsigned char>(lower[pos + 5])) || lower[pos + 5] == '/')) {
            std::string tag = text.substr(pos + 5, end - pos - 5);
            size_t apos = 0;
            markup_attribute attr;
            bool http_equiv_content_type = false;
            std::string charset, content;
            while (get_next_attribute(tag, apos, attr)) {
                if (attr.name == "charset") {
                    if (charset.empty()) {
                        charset = attr.value;
                    }
                } else if (attr.name == "content") {
                    if (content.empty()) {
                        content = attr.value;
                    }
                } else if (attr.name == "http-equiv") {
                    std::string value = to_lower(reinterpret_cast<const unsigned char*>(attr.value.data()),
                        attr.value.size());
                    if (value == "content-type") {
                        http_equiv_content_type = true;
                    }
                }
            }
            if (charset.empty() && http_equiv_content_type && !content.empty()) {
                charset = get_meta_content_charset(content);
            }
            const QoreEncoding* enc = get_declared_encoding(charset);
            if (enc) {
                return enc;
            }
        }
        pos = end + 1;
    }
    return nullptr;
}

// returns the encoding of a @charset rule at the start of a style sheet (CSS Syntax section 3.2)
static std::string get_css_charset_rule(const unsigned char* p, size_t len) {
    static constexpr const char prefix[] = "@charset \"";
    static constexpr size_t prefix_len = sizeof(prefix) - 1;
    if (len <= prefix_len || memcmp(p, prefix, prefix_len)) {
        return std::string();
    }
    const unsigned char* start = p + prefix_len;
    const unsigned char* end = static_cast<const unsigned char*>(memchr(start, '"', len - prefix_len));
    if (!end || static_cast<size_t>(end - p) + 1 >= len || end[1] != ';') {
        return std::string();
    }
    return std::string(reinterpret_cast<const char*>(start), end - start);
}

QoreHttpBodyCharset qore_get_http_body_charset(const char* content_type, const void* data, size_t len,
        const QoreEncoding* assumed) {
    QoreHttpBodyCharset rv;
    const unsigned char* p = static_cast<const unsigned char*>(data);
    if (!p) {
        len = 0;
    }
    QoreHttpBodyKind kind;
    if (!content_type || !*content_type) {
        // a message without a type is examined to tell text from binary data (RFC 9110 section 8.3)
        kind = is_text_data(p, len) ? QoreHttpBodyKind::Text : QoreHttpBodyKind::Binary;
    } else {
        kind = qore_http_body_kind(content_type);
    }
    if (kind == QoreHttpBodyKind::Binary) {
        return rv;
    }
    if (!assumed) {
        assumed = QCS_ISO_8859_1;
    }

    // JSON and server-sent events are always UTF-8; a UTF-8 BOM is ignored
    if (kind == QoreHttpBodyKind::Json || kind == QoreHttpBodyKind::EventStream) {
        rv.enc = QCS_UTF8;
        if (len >= 3 && p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf) {
            rv.bom_len = 3;
        }
        return rv;
    }

    // a byte order mark overrides any other declaration (WHATWG Encoding "decode", RFC 7303 section 3.3)
    rv.enc = get_bom_encoding(p, len, kind == QoreHttpBodyKind::Yaml, rv.bom_len);
    if (rv.enc) {
        return rv;
    }

    // YAML has no charset parameter (RFC 9512)
    if (kind == QoreHttpBodyKind::Yaml) {
        rv.enc = QCS_UTF8;
        return rv;
    }

    rv.enc = get_named_encoding(get_charset_param(content_type));
    if (rv.enc) {
        return rv;
    }

    size_t sniff_len = len > HTTP_BODY_SNIFF_LEN ? HTTP_BODY_SNIFF_LEN : len;
    switch (kind) {
        case QoreHttpBodyKind::Xml:
            rv.enc = get_supported_encoding(get_xml_declaration_encoding(p, sniff_len));
            if (!rv.enc) {
                rv.enc = QCS_UTF8;
            }
            break;

        case QoreHttpBodyKind::Html:
            rv.enc = get_html_meta_encoding(p, sniff_len);
            if (!rv.enc) {
                rv.enc = assumed;
            }
            break;

        case QoreHttpBodyKind::Css:
            rv.enc = get_declared_encoding(get_css_charset_rule(p, sniff_len));
            if (!rv.enc) {
                rv.enc = QCS_UTF8;
            }
            break;

        case QoreHttpBodyKind::Javascript:
        case QoreHttpBodyKind::Form:
            rv.enc = QCS_UTF8;
            break;

        default:
            rv.enc = assumed;
            break;
    }
    return rv;
}

QoreStringNode* qore_http_body_string(const QoreHttpBodyCharset& charset, const void* data, size_t len) {
    assert(charset.enc);
    assert(charset.bom_len <= len);
    const char* p = static_cast<const char*>(data);
    return new QoreStringNode(p + charset.bom_len, len - charset.bom_len, charset.enc);
}

SimpleValueQoreNode* qore_decode_http_body(const char* content_type, const BinaryNode* body,
        const QoreEncoding* assumed) {
    assert(body);
    QoreHttpBodyCharset charset = qore_get_http_body_charset(content_type, body->getPtr(), body->size(), assumed);
    if (!charset.enc) {
        body->ref();
        return const_cast<BinaryNode*>(body);
    }
    return qore_http_body_string(charset, body->getPtr(), body->size());
}

QoreStringNode* qore_http_response_body_string(const char* content_type, const QoreHttpBodyCharset& charset,
        const void* data, size_t len, ExceptionSink* xsink) {
    SimpleRefHolder<QoreStringNode> str(qore_http_body_string(charset, data, len));
    if (qore_string_encoding_valid(**str)) {
        return str.release();
    }
    switch (qore_http_body_kind(content_type)) {
        // a body without a Content-Type that is text is any other text
        case QoreHttpBodyKind::Binary:
        case QoreHttpBodyKind::Html:
        case QoreHttpBodyKind::Text:
            str->setEncoding(QCS_ISO_8859_1);
            return str.release();
        default:
            break;
    }
    xsink->raiseException("INVALID-ENCODING", "the HTTP response body of %zu bytes is not valid in its encoding "
        "\"%s\"", len, charset.enc->getCode());
    return nullptr;
}

SimpleValueQoreNode* qore_decode_http_response_body(const char* content_type, const BinaryNode* body,
        const QoreEncoding* assumed, ExceptionSink* xsink) {
    assert(body);
    QoreHttpBodyCharset charset = qore_get_http_body_charset(content_type, body->getPtr(), body->size(), assumed);
    if (!charset.enc) {
        body->ref();
        return const_cast<BinaryNode*>(body);
    }
    return qore_http_response_body_string(content_type, charset, body->getPtr(), body->size(), xsink);
}

const QoreEncoding* qore_get_http_header_charset(const char* content_type, const QoreEncoding* assumed) {
    if (!content_type || !*content_type) {
        // the data of a message without a type cannot be examined before it is read; it is assumed to be text
        return assumed ? assumed : QCS_ISO_8859_1;
    }
    QoreHttpBodyKind kind = qore_http_body_kind(content_type);
    switch (kind) {
        case QoreHttpBodyKind::Binary:
            return nullptr;
        case QoreHttpBodyKind::Json:
        case QoreHttpBodyKind::EventStream:
        case QoreHttpBodyKind::Yaml:
            return QCS_UTF8;
        default:
            break;
    }
    const QoreEncoding* enc = get_named_encoding(get_charset_param(content_type));
    if (enc) {
        return enc;
    }
    switch (kind) {
        case QoreHttpBodyKind::Xml:
        case QoreHttpBodyKind::Css:
        case QoreHttpBodyKind::Javascript:
        case QoreHttpBodyKind::Form:
            return QCS_UTF8;
        default:
            return assumed ? assumed : QCS_ISO_8859_1;
    }
}

QoreHttpContentTypeInfo qore_get_http_content_type_info(const char* content_type, const QoreEncoding* assumed) {
    assert(content_type);
    assert(assumed);
    QoreHttpContentTypeInfo rv;
    // the encoding of the body follows the rules shared by all HTTP clients and servers, as far as they can be
    // applied before the body is read; see qore_get_http_header_charset()
    const QoreEncoding* hdr_enc = qore_get_http_header_charset(content_type, assumed);
    qore_http_media_type_param charset;
    if (qore_find_http_media_type_param(content_type, "charset", charset) && !charset.value.empty()) {
        // a type that is binary by definition: the charset is used if it selects an encoding (qore#6460)
        if (!hdr_enc) {
            hdr_enc = get_named_encoding(charset.value);
        }
        rv.enc = hdr_enc ? hdr_enc : assumed;
        rv.charset = charset.value;
        // the content type without the charset parameter
        QoreString ct(content_type, charset.begin, QCS_DEFAULT);
        ct.trim_trailing();
        ct.concat(content_type + charset.end);
        ct.trim();
        if (!ct.empty()) {
            rv.body_content_type.assign(ct.c_str(), ct.size());
            rv.has_body_content_type = true;
        }
    } else {
        // a type that is not text is read in the assumed encoding, as before
        rv.enc = hdr_enc ? hdr_enc : assumed;
        rv.charset = rv.enc->getCode();
        rv.body_content_type = content_type;
        rv.has_body_content_type = true;
    }
    return rv;
}

// skips optional whitespace (RFC 9110 section 5.6.3)
static const char* skip_http_ows(const char* p) {
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    return p;
}

// returns true if c is a token character (tchar, RFC 9110 section 5.6.2)
static bool is_http_tchar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || (c && strchr("!#$%&'*+-.^_`|~", c));
}

// returns the end of the token at p (p if there is none)
static const char* skip_http_token(const char* p) {
    while (is_http_tchar(*p)) {
        ++p;
    }
    return p;
}

// returns the weight of a qvalue (RFC 9110 section 12.4.2: "0" [ "." 0*3DIGIT ] / "1" [ "." 0*3("0") ]) in
// thousandths, or -1 if [p, end) is not a qvalue
static int parse_http_qvalue(const char* p, const char* end) {
    if (p == end || (*p != '0' && *p != '1')) {
        return -1;
    }
    int rv = (*p++ - '0') * 1000;
    if (p == end) {
        return rv;
    }
    if (*p++ != '.') {
        return -1;
    }
    int scale = 100;
    if (end - p > 3) {
        return -1;
    }
    for (; p < end; ++p, scale /= 10) {
        if (!isdigit(static_cast<unsigned char>(*p))) {
            return -1;
        }
        rv += (*p - '0') * scale;
    }
    return rv > 1000 ? -1 : rv;
}

// returns the encoding that the response can be sent in for a charset of Accept-Charset, or nullptr: an encoding that
// an untrusted name may select (one that Qore knows, by its name or an alias, case-insensitive, or the encoding of a
// label of a standard character set; see qore_find_untrusted_encoding()) and that iconv converts to, by the name that
// Qore converts with; the server registers the encoding that it chooses, so any other name is never chosen (iconv
// accepts an unbounded number of spellings of one encoding)
static const QoreEncoding* get_supported_http_charset(const std::string& name) {
    return get_supported_encoding(name);
}

std::string qore_get_http_accept_charset(const char* t) {
    assert(t);
    // RFC 9110 section 12.5.2: Accept-Charset = #( ( token / "*" ) weight ), weight = OWS ";" OWS "q=" qvalue
    // (section 12.4.2), and the list syntax of section 5.6.1: elements separated by commas with optional whitespace,
    // empty elements ignored
    // the acceptable charset with the highest weight that is supported, and its weight; an equal weight keeps the
    // earlier one
    std::string best;
    int best_q = 0;
    // the weights of "*" and UTF-8, if given (the first occurrence), else -1
    int star_q = -1;
    int utf8_q = -1;
    const char* p = t;
    while (*p) {
        p = skip_http_ows(p);
        // the charset: a token or "*"; "*" is a token character
        const char* name = p;
        p = skip_http_token(p);
        std::string cs(name, p - name);
        // the parameters: only the weight ("q", case-insensitive; 1 without it) is defined; any other parameter is
        // ignored, with a token or quoted-string value (section 5.6.6)
        int q = 1000;
        bool valid = true;
        // the parameters are parsed to the end after an error too, so that a comma in a quoted string never ends the
        // element
        while (true) {
            p = skip_http_ows(p);
            if (*p != ';') {
                break;
            }
            p = skip_http_ows(p + 1);
            const char* pname = p;
            p = skip_http_token(p);
            bool is_q = p - pname == 1 && (*pname == 'q' || *pname == 'Q');
            if (p == pname || *p != '=') {
                // a parameter without a name or a value (a trailing ";" is allowed)
                if (p != pname || (*p != ',' && *p && *p != ' ' && *p != '\t')) {
                    valid = false;
                    break;
                }
                continue;
            }
            ++p;
            const char* val = p;
            if (*p == '"') {
                // a quoted-string: up to the closing quote, with backslash escapes
                for (++p; *p && *p != '"'; ++p) {
                    if (*p == '\\' && p[1]) {
                        ++p;
                    }
                }
                if (*p != '"') {
                    valid = false;
                    break;
                }
                ++p;
                if (is_q) {
                    valid = false;
                }
                continue;
            }
            p = skip_http_token(p);
            if (is_q) {
                int pq = parse_http_qvalue(val, p);
                if (pq < 0) {
                    valid = false;
                } else {
                    q = pq;
                }
            } else if (p == val) {
                valid = false;
            }
        }
        // anything else before the next comma outside a quoted string makes the element invalid
        if (*p && *p != ',') {
            valid = false;
            bool quoted = false;
            for (; *p && (quoted || *p != ','); ++p) {
                if (*p == '"') {
                    quoted = !quoted;
                } else if (quoted && *p == '\\' && p[1]) {
                    ++p;
                }
            }
        }
        if (*p == ',') {
            ++p;
        }
        if (cs.empty() || !valid) {
            continue;
        }
        if (cs == "*") {
            if (star_q < 0) {
                star_q = q;
            }
            continue;
        }
        if (!strcasecmp(cs.c_str(), "utf-8") || !strcasecmp(cs.c_str(), "utf8")) {
            if (utf8_q < 0) {
                utf8_q = q;
            }
            continue;
        }
        // q=0 means not acceptable
        if (q > best_q) {
            const QoreEncoding* enc = get_supported_http_charset(cs);
            if (enc) {
                // a name or an alias of an encoding that Qore knows is given as it is, a label of a character set
                // (see qore_find_untrusted_encoding()) as the name of its encoding, which the server converts with;
                // a label that the program registered as a name of another encoding is a label (qore#6570)
                best = QoreEncodingManager::find(cs.c_str()) == enc ? cs : std::string(enc->getCode());
                best_q = q;
            }
        }
    }
    // UTF-8, given or matched by "*" when not given, is chosen over another charset with the same weight
    int u = utf8_q >= 0 ? utf8_q : star_q;
    if (u > 0 && u >= best_q) {
        return "utf8";
    }
    return best;
}

/* -*- indent-tabs-mode: nil -*- */
/*
    QoreURL.cpp

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

#include "qore/Qore.h"
#include "qore/QoreURL.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreUriReference.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <regex>
#include <vector>

static std::regex check_port("[0-9]{1,5}(/|$)", std::regex::extended);

struct qore_url_private {
public:
    QoreStringNode* protocol, *path, *username, *password, *host;
    int port;

    DLLLOCAL qore_url_private() {
        zero();
    }

    DLLLOCAL ~qore_url_private() {
        reset();
    }

    DLLLOCAL void zero() {
        protocol = path = username = password = host = nullptr;
        port = 0;
    }

    DLLLOCAL void reset() {
        if (protocol) {
            protocol->deref();
        }
        if (path) {
            path->deref();
        }
        if (username) {
            username->deref();
        }
        if (password) {
            password->deref();
        }
        if (host) {
            host->deref();
        }
    }

    DLLLOCAL int parse(const char* url, int options = 0, ExceptionSink* xsink = nullptr) {
        reset();
        zero();
        parse_intern(url, options, xsink);
        if (xsink && !*xsink && !isValid()) {
            xsink->raiseException("PARSE-URL-ERROR", "URL '%s' cannot be parsed", url);
        }
        return isValid() ? 0 : -1;
    }

    DLLLOCAL bool isValid() const {
        return (host && host->strlen()) || (path && path->strlen());
    }

    // destructive
    DLLLOCAL QoreHashNode* getHash() {
        QoreHashNode* h = new QoreHashNode(hashdeclUrlInfo, nullptr);
        qore_hash_private* ph = qore_hash_private::get(*h);
        if (protocol) {
            ph->setKeyValueIntern("protocol", protocol);
            protocol = nullptr;
        }
        if (path) {
            ph->setKeyValueIntern("path", path);
            path = nullptr;
        }
        if (username) {
            ph->setKeyValueIntern("username", username);
            username = nullptr;
        }
        if (password) {
            ph->setKeyValueIntern("password", password);
            password = nullptr;
        }
        if (host) {
            ph->setKeyValueIntern("host", host);
            host = nullptr;
        }
        if (port) {
            ph->setKeyValueIntern("port", port);
        }

        return h;
    }

private:
    DLLLOCAL void invalidate() {
        if (host) {
            host->deref();
            host = nullptr;
        }
        if (path) {
            path->deref();
            path = nullptr;
        }
    }

    DLLLOCAL void parse_intern(const char* buf, int options, ExceptionSink* xsink) {
        if (!buf || !buf[0]) {
            return;
        }
        bool keep_brackets = options & QURL_KEEP_BRACKETS;

        printd(5, "QoreURL::parse_intern(%s)\n", buf);

        // buf is continuously shrinked depending on the part of the string
        // that remains to be processed
        std::string sbuf(buf);

        QoreString* protocol_lwr = nullptr;
        QoreString tmp;

        // look for the scheme, move 'pos' after the scheme (protocol) specification
        size_t protocol_separator = sbuf.find("://");
        if (protocol_separator != std::string::npos) {
            protocol = new QoreStringNode(sbuf.c_str(), protocol_separator);
            if (!(options & QURL_MAINTAIN_CASE)) {
                // convert to lower case
                protocol->tolwr();
                protocol_lwr = protocol;
            } else {
                tmp.set(*protocol);
                tmp.tolwr();
                protocol_lwr = &tmp;
            }
            //printd(5, "QoreURL::parse_intern protocol: %s\n", protocol->c_str());
            sbuf = sbuf.substr(protocol_separator + 3);

            // check for special cases with Windows paths
            if (*protocol_lwr == "file") {
                // Windows paths should also be parsed like: file:///c:/dir...
                // https://blogs.msdn.microsoft.com/ie/2006/12/06/file-uris-in-windows/
                // https://en.wikipedia.org/wiki/File_URI_scheme#Windows
                if (sbuf.size() >= 3 &&
                    ((sbuf[0] == '/' || sbuf[0] == '\\') && (isalpha(sbuf[1]) && sbuf[2] == ':' && !isdigit(sbuf[3])))
                    && sbuf.find('@') == std::string::npos) {
                    path = new QoreStringNode(sbuf.c_str() + 1);
                    if (options & QURL_DECODE_ANY) {
                        decodeStrings(options, xsink);
                    }
                    return;
                }
            }
        }

        // if there is no scheme or the scheme is file://, see if the rest of the URL is a Windows UNC path
        if ((!protocol_lwr || *protocol_lwr == "file")
            && (sbuf.size() >= 2
            && ((isalpha(sbuf[0]) && sbuf[1] == ':' && !isdigit(sbuf[2]))
                || (sbuf[0] == '\\' && sbuf[1] == '\\')
                || (sbuf[0] == '/' && sbuf[1] == '/'))
            && sbuf.find('@') == std::string::npos)) {
            path = new QoreStringNode(sbuf.c_str());
            if (options & QURL_DECODE_ANY) {
                decodeStrings(options, xsink);
            }
            return;
        }

        // find end of hostname; look for username and password separator characters first
        size_t path_start = sbuf.find(':');
        if (path_start != std::string::npos && path_start) {
            // ignore if this is the port separator
            if (!std::regex_search(sbuf.c_str() + path_start + 1, check_port)) {
                path_start = sbuf.find('@', path_start + 1);
            } else {
                // ignore if there is no subsequent '@' sign
                size_t at_sign = sbuf.find('@', path_start + 1);
                if (at_sign == std::string::npos) {
                    path_start = std::string::npos;
                }
            }
        }

        if (path_start == std::string::npos) {
            path_start = 0;
        }

        path_start = sbuf.find('/', path_start);
        if (path_start == std::string::npos) {
            path_start = sbuf.find('?');
        }
        if (path_start != std::string::npos) {
            // issue #3457: make sure there are no ':' and '@' signs after this mark
            size_t char_pos = sbuf.find(':', path_start + 1);
            if (char_pos != std::string::npos) {
                size_t char_pos2 = sbuf.find('@', path_start + 1);
                if (char_pos2 != std::string::npos) {
                    if (char_pos2 > char_pos) {
                        char_pos = char_pos2;
                    }
                    path_start = sbuf.find('/', char_pos + 1);
                    if (path_start == std::string::npos) {
                        path_start = sbuf.find('?', char_pos + 1);
                    }
                }
            }

            if (path_start != std::string::npos) {
                // get pathname if not at EOS
                path = new QoreStringNode(sbuf.c_str() + path_start);
                //printd(5, "QoreURL::parse_intern path: '%s'\n", path->c_str());
                // get copy of hostname string for localized searching and invasive parsing
                sbuf = sbuf.substr(0, path_start);
                //printd(5, "QoreURL::sbuf: '%s' size: %d\n", sbuf.c_str(), sbuf.size());
            }
        }

        // see if there's a username
        // note that sbuf here has already had the path removed so we can safely do a reverse search for the '@' sign
        size_t username_end = sbuf.rfind('@');
        if (username_end != std::string::npos) {
            // see if there's a password
            size_t pw_start = sbuf.find(':');
            if (pw_start < username_end && pw_start != std::string::npos) {
                printd(5, "QoreURL::parse_intern password: '%s'\n", sbuf.c_str() + pw_start + 1);
                password = new QoreStringNode(sbuf.c_str() + pw_start + 1, username_end - (pw_start + 1));
                // set username
                username = new QoreStringNode(sbuf.c_str(), pw_start);
            } else {
                username = new QoreStringNode(sbuf.c_str(), username_end);
            }
            sbuf = sbuf.substr(username_end + 1);
        }
        // else no username, keep processing sbuf

        // see if the "hostname" is enclosed in square brackets, denoting an ipv6 address
        if (!sbuf.empty() && sbuf[0] == '[') {
            size_t right_bracket = sbuf.find(']');
            if (right_bracket != std::string::npos) {
                host = new QoreStringNode(sbuf.c_str() + (keep_brackets ? 0 : 1),
                    right_bracket - (keep_brackets ? -1 : 1));
                sbuf = sbuf.substr(right_bracket + 1);
            }
        }

        bool has_port = false;
        // see if there's a port
        size_t port_start = sbuf.rfind(':');
        if (port_start != std::string::npos) {
            // see if it's IPv6 localhost (::)
            if (port_start != 1 || sbuf[0] != ':') {
                // find the end of port data
                if (port_start + 1 == sbuf.size()) {
                    if (xsink) {
                        xsink->raiseException("PARSE-URL-ERROR", "URL '%s' has an invalid empty port specification",
                            buf);
                    }
                    invalidate();
                    return;
                }
                std::string port_str;
                for (size_t i = port_start + 1; i < sbuf.size(); ++i) {
                    if (!isdigit(sbuf[i])) {
                        if (xsink)
                            xsink->raiseException("PARSE-URL-ERROR", "URL '%s' has an invalid non-numeric character "
                                "in the port specification (char: '%c' ue: %lld sbuf: '%s')", buf, sbuf[i],
                                username_end, sbuf.c_str());
                        invalidate();
                        return;
                    }
                    port_str += sbuf[i];
                }

                // convert string to a real port
                try {
                    port = std::stoi(port_str);
                } catch (const std::out_of_range& e) {
                    if (xsink) {
                        doInvalidPortException(xsink, buf);
                    }
                    invalidate();
                    return;
                }

                if (port < 0 || port > UINT16_MAX) {
                    if (xsink) {
                        doInvalidPortException(xsink, buf);
                    }
                    invalidate();
                    return;
                }

                sbuf = sbuf.substr(0, port_start);
                has_port = true;
                printd(5, "QoreURL::parse_intern port: %d\n", port);
            }
        }

        // there is no hostname if there is no port specification and
        // no protocol, username, or password -- just a relative path
        if (!host && !sbuf.empty()) {
            // see if the hostname is in the form "socket=xxxx" in which case we interpret as a UNIX domain socket
            if (!strncasecmp(sbuf.c_str(), "socket=", 7)) {
                host = new QoreStringNode;
                host->concatDecodeUrl(sbuf.c_str() + 7);
            } else if (!has_port && !protocol && !username && !password && path) {
                path->replace(0, 0, sbuf.c_str());
            } else {
                // set hostname
                //printd(5, "QoreURL::parse_intern host: %s\n", sbuf.c_str());
                host = new QoreStringNode(sbuf.c_str());
            }
        }

        // perform percent decoding, if required
        if (options & QURL_DECODE_ANY) {
            decodeStrings(options, xsink);
        }
    }

    DLLLOCAL void decodeStrings(int options, ExceptionSink* xsink) {
        if (username && !username->empty()) {
            SimpleRefHolder<QoreStringNode> holder(username);
            username = decodeString(username, xsink);
        }
        if (password && !password->empty()) {
            SimpleRefHolder<QoreStringNode> holder(password);
            password = decodeString(password, xsink);
        }
        if (host && !host->empty()) {
            SimpleRefHolder<QoreStringNode> holder(host);
            host = decodeString(host, xsink);
        }
        if ((options & QURL_DECODE_PATH) && path && !path->empty()) {
            SimpleRefHolder<QoreStringNode> holder(path);
            path = decodeString(path, xsink);
        }
    }

    static QoreStringNode* decodeString(QoreStringNode* str, ExceptionSink* xsink) {
        assert(xsink);
        QoreStringNodeHolder decoded_str(new QoreStringNode(QCS_UTF8));
        decoded_str->concatDecodeUrl(*str, xsink);
        return *xsink ? nullptr : decoded_str.release();
    }

    static void doInvalidPortException(ExceptionSink* xsink, const char* buf) {
        xsink->raiseException("PARSE-URL-ERROR", "URL '%s' has an invalid port value; it must be between 0 and 65535",
            buf);
    }
};

QoreURL::QoreURL() : priv(new qore_url_private) {
}

QoreURL::QoreURL(const char* url) : priv(new qore_url_private) {
    parse(url);
}

QoreURL::QoreURL(const QoreString* url) : priv(new qore_url_private) {
    parse(url->c_str());
}

QoreURL::QoreURL(const char* url, bool keep_brackets) : priv(new qore_url_private) {
    parse(url, keep_brackets ? QURL_KEEP_BRACKETS : 0);
}

QoreURL::QoreURL(const QoreString* url, bool keep_brackets) : priv(new qore_url_private) {
    parse(url->c_str(), keep_brackets ? QURL_KEEP_BRACKETS : 0);
}

QoreURL::QoreURL(const QoreString* url, bool keep_brackets, ExceptionSink* xsink) : priv(new qore_url_private) {
    parse(xsink, url, keep_brackets ? QURL_KEEP_BRACKETS : 0);
}

QoreURL::QoreURL(const char* url, int options) : priv(new qore_url_private) {
    parse(url, options);
}

QoreURL::QoreURL(const QoreString& url, int options) : priv(new qore_url_private) {
    parse(url, options);
}

QoreURL::QoreURL(const std::string& url, int options) : priv(new qore_url_private) {
    parse(url, options);
}

QoreURL::QoreURL(ExceptionSink* xsink, const char* url, int options) : priv(new qore_url_private) {
    parse(xsink, url, options);
}

QoreURL::QoreURL(ExceptionSink* xsink, const QoreString& url, int options) : priv(new qore_url_private) {
    parse(xsink, url, options);
}

QoreURL::QoreURL(ExceptionSink* xsink, const std::string& url, int options) : priv(new qore_url_private) {
    parse(xsink, url, options);
}

QoreURL::~QoreURL() {
    delete priv;
}

int QoreURL::parse(const char* url) {
    return priv->parse(url);
}

int QoreURL::parse(const QoreString* url) {
    return priv->parse(url->c_str());
}

int QoreURL::parse(const char* url, bool keep_brackets) {
    return priv->parse(url, keep_brackets);
}

int QoreURL::parse(const QoreString* url, bool keep_brackets) {
    return priv->parse(url->c_str(), keep_brackets);
}

int QoreURL::parse(const QoreString* url, bool keep_brackets, ExceptionSink* xsink) {
    TempEncodingHelper tmp(url, QCS_UTF8, xsink);
    if (*xsink) {
        return -1;
    }
    return priv->parse(tmp->c_str(), keep_brackets, xsink);
}

int QoreURL::parse(const char* url, int options) {
    return priv->parse(url, options);
}

int QoreURL::parse(const QoreString& url, int options) {
    return priv->parse(url.c_str(), options);
}

int QoreURL::parse(const std::string& url, int options) {
    return priv->parse(url.c_str(), options);
}

int QoreURL::parse(ExceptionSink* xsink, const char* url, int options) {
    return priv->parse(url, options, xsink);
}

int QoreURL::parse(ExceptionSink* xsink, const QoreString& url, int options) {
    TempEncodingHelper tmp(url, QCS_UTF8, xsink);
    if (*xsink) {
        return -1;
    }
    return priv->parse(tmp->c_str(), options, xsink);
}

int QoreURL::parse(ExceptionSink* xsink, const std::string& url, int options) {
    TempEncodingHelper tmp(url, QCS_UTF8, xsink);
    if (*xsink) {
        return -1;
    }
    return priv->parse(tmp->c_str(), options, xsink);
}

bool QoreURL::isValid() const {
    return (priv->host && priv->host->strlen()) || (priv->path && priv->path->strlen());
}

const QoreString* QoreURL::getProtocol() const {
    return priv->protocol;
}

const QoreString* QoreURL::getUserName() const {
    return priv->username;
}

const QoreString* QoreURL::getPassword() const {
    return priv->password;
}

const QoreString* QoreURL::getPath() const {
    return priv->path;
}

const QoreString* QoreURL::getHost() const {
    return priv->host;
}

int QoreURL::getPort() const {
    return priv->port;
}

// destructive
QoreHashNode* QoreURL::getHash() {
    return priv->getHash();
}

char* QoreURL::take_path() {
    return priv->path ? priv->path->giveBuffer() : nullptr;
}

char* QoreURL::take_username() {
   return priv->username ? priv->username->giveBuffer() : nullptr;
}

char* QoreURL::take_password() {
   return priv->password ? priv->password->giveBuffer() : nullptr;
}

char* QoreURL::take_host() {
   return priv->host ? priv->host->giveBuffer() : nullptr;
}

// checks for cancellation every 100 iterations of a loop over a URI reference, if an exception sink is given
class UriCancelCheck {
public:
    DLLLOCAL UriCancelCheck(ExceptionSink* xsink) : xsink(xsink) {
    }

    //! returns true if the operation was cancelled, in which case an exception has been raised
    DLLLOCAL bool operator()() {
        return xsink && !(++count % 100) && qore_check_cancel(xsink, "URI reference processing");
    }

private:
    ExceptionSink* xsink;
    unsigned count = 0;
};

bool QoreUriReference::isValidAuthority(const std::string& authority) {
    const char* p = authority.data();
    const char* end = p + authority.size();
    for (; p < end; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c == '%') {
            if (end - p < 3 || !isxdigit(static_cast<unsigned char>(p[1]))
                    || !isxdigit(static_cast<unsigned char>(p[2]))) {
                return false;
            }
            p += 2;
            continue;
        }
        if (c <= 0x20 || c == 0x7f || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`'
                || c == '{' || c == '|' || c == '}') {
            return false;
        }
    }
    return true;
}

bool QoreUriReference::isValidScheme(const char* str, size_t len) {
    // RFC 3986 section 3.1: scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    if (!len || !isalpha(static_cast<unsigned char>(str[0]))) {
        return false;
    }
    for (size_t i = 1; i < len; ++i) {
        const unsigned char c = static_cast<unsigned char>(str[i]);
        if (!isalnum(c) && c != '+' && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

void QoreUriReference::parse(const char* str, size_t len, ExceptionSink* xsink) {
    *this = QoreUriReference();
    const char* p = str;
    const char* end = str + len;
    UriCancelCheck cancelled(xsink);

    // RFC 3986 appendix B: ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?
    const char* q = p;
    while (q < end && *q != ':' && *q != '/' && *q != '?' && *q != '#') {
        if (cancelled()) {
            return;
        }
        ++q;
    }
    if (q < end && *q == ':' && isValidScheme(p, q - p)) {
        scheme.assign(p, q - p);
        has_scheme = true;
        p = q + 1;
    }

    if (end - p >= 2 && p[0] == '/' && p[1] == '/') {
        p += 2;
        q = p;
        while (q < end && *q != '/' && *q != '?' && *q != '#') {
            if (cancelled()) {
                return;
            }
            ++q;
        }
        authority.assign(p, q - p);
        has_authority = true;
        p = q;
    }

    q = p;
    while (q < end && *q != '?' && *q != '#') {
        if (cancelled()) {
            return;
        }
        ++q;
    }
    path.assign(p, q - p);
    p = q;

    if (p < end && *p == '?') {
        ++p;
        q = p;
        while (q < end && *q != '#') {
            if (cancelled()) {
                return;
            }
            ++q;
        }
        query.assign(p, q - p);
        has_query = true;
        p = q;
    }

    if (p < end && *p == '#') {
        ++p;
        fragment.assign(p, end - p);
        has_fragment = true;
    }
}

std::string QoreUriReference::compose(bool include_fragment, bool encode, ExceptionSink* xsink) const {
    // RFC 3986 section 5.3
    std::string rv;
    if (has_scheme) {
        rv += scheme;
        rv += ':';
    }
    if (has_authority) {
        rv += "//";
        rv += authority;
    } else if (path.size() >= 2 && path[0] == '/' && path[1] == '/') {
        // without an authority, a path cannot start with "//" (RFC 3986 section 3.3), as it would be taken for one;
        // the dot segment keeps the path equivalent
        rv += "/.";
    }
    if (encode) {
        appendEncoded(rv, path, xsink);
    } else {
        rv += path;
    }
    if (has_query) {
        rv += '?';
        if (encode) {
            appendEncoded(rv, query, xsink);
        } else {
            rv += query;
        }
    }
    if (include_fragment && has_fragment) {
        rv += '#';
        if (encode) {
            appendEncoded(rv, fragment, xsink);
        } else {
            rv += fragment;
        }
    }
    return rv;
}

void QoreUriReference::appendEncoded(std::string& out, const std::string& in, ExceptionSink* xsink) {
    UriCancelCheck cancelled(xsink);
    for (unsigned char c : in) {
        if (cancelled()) {
            return;
        }
        if (c <= 0x20 || c >= 0x7f || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`'
                || c == '{' || c == '|' || c == '}') {
            char buf[4];
            snprintf(buf, sizeof buf, "%%%02X", c);
            out += buf;
        } else {
            out += static_cast<char>(c);
        }
    }
}

bool QoreUriReference::hasColonInFirstRelativeSegment() const {
    if (has_scheme || has_authority || path.empty() || path[0] == '/') {
        return false;
    }
    size_t end = path.find('/');
    return path.find(':') < end;
}

std::string QoreUriReference::removeRelativeDotSegments(const std::string& path, ExceptionSink* xsink) {
    UriCancelCheck cancelled(xsink);
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        if (cancelled()) {
            return std::string();
        }
        size_t end = path.find('/', start);
        bool last = end == std::string::npos;
        std::string seg = path.substr(start, last ? std::string::npos : end - start);
        if (seg == "." || seg == "..") {
            if (seg == "..") {
                if (!out.empty() && out.back() != "..") {
                    out.pop_back();
                } else {
                    out.push_back("..");
                }
            }
            // a final dot segment names a directory
            if (last) {
                out.push_back(std::string());
            }
        } else {
            out.push_back(std::move(seg));
        }
        if (last) {
            break;
        }
        start = end + 1;
    }
    std::string rv;
    for (size_t i = 0; i < out.size(); ++i) {
        if (cancelled()) {
            return std::string();
        }
        if (i) {
            rv += '/';
        }
        rv += out[i];
    }
    if (rv.empty() && !path.empty()) {
        // the path named the current directory; an empty path would be a same-document reference
        return "./";
    }
    // a colon in the first segment would make the path look like a scheme (RFC 3986 section 4.2)
    size_t first_end = rv.find('/');
    if (rv.find(':') < first_end) {
        rv.insert(0, "./");
    }
    return rv;
}

// removes the last segment and its preceding "/", if any, from the output buffer
static void uri_remove_last_segment(std::string& out) {
    size_t pos = out.rfind('/');
    if (pos == std::string::npos) {
        out.clear();
    } else {
        out.resize(pos);
    }
}

std::string QoreUriReference::removeDotSegments(const std::string& path, ExceptionSink* xsink) {
    // RFC 3986 section 5.2.4; the letters below refer to the steps in the RFC
    UriCancelCheck cancelled(xsink);
    std::string in(path);
    std::string out;
    size_t i = 0;
    while (i < in.size()) {
        if (cancelled()) {
            return std::string();
        }
        size_t rem = in.size() - i;
        // A: remove a "../" or "./" prefix
        if (!in.compare(i, 3, "../")) {
            i += 3;
            continue;
        }
        if (!in.compare(i, 2, "./")) {
            i += 2;
            continue;
        }
        // B: replace a "/./" prefix or a complete "/." segment with "/"
        if (!in.compare(i, 3, "/./")) {
            i += 2;
            continue;
        }
        if (rem == 2 && !in.compare(i, 2, "/.")) {
            in.replace(i, 2, "/");
            continue;
        }
        // C: replace a "/../" prefix or a complete "/.." segment with "/" and remove the last output segment
        if (!in.compare(i, 4, "/../")) {
            i += 3;
            uri_remove_last_segment(out);
            continue;
        }
        if (rem == 3 && !in.compare(i, 3, "/..")) {
            in.replace(i, 3, "/");
            uri_remove_last_segment(out);
            continue;
        }
        // D: remove a remaining "." or ".."
        if ((rem == 1 && in[i] == '.') || (rem == 2 && !in.compare(i, 2, ".."))) {
            break;
        }
        // E: move the first path segment, including any initial "/", to the output
        size_t start = i;
        if (in[i] == '/') {
            ++i;
        }
        size_t next = in.find('/', i);
        if (next == std::string::npos) {
            next = in.size();
        }
        out.append(in, start, next - start);
        i = next;
    }
    return out;
}

// RFC 3986 section 5.2.3
static std::string uri_merge_paths(const QoreUriReference& base, const std::string& ref_path) {
    if (base.has_authority && base.path.empty()) {
        return "/" + ref_path;
    }
    size_t pos = base.path.rfind('/');
    if (pos == std::string::npos) {
        return ref_path;
    }
    return base.path.substr(0, pos + 1) + ref_path;
}

QoreUriReference QoreUriReference::resolve(const QoreUriReference& ref, ExceptionSink* xsink) const {
    // RFC 3986 section 5.2.2, strict parser
    QoreUriReference t;
    // true if dot segments have to be removed from t.path
    bool remove_dots = true;
    if (ref.has_scheme) {
        t.scheme = ref.scheme;
        t.has_scheme = true;
        t.authority = ref.authority;
        t.has_authority = ref.has_authority;
        t.path = ref.path;
        t.query = ref.query;
        t.has_query = ref.has_query;
    } else {
        if (ref.has_authority) {
            t.authority = ref.authority;
            t.has_authority = true;
            t.path = ref.path;
            t.query = ref.query;
            t.has_query = ref.has_query;
        } else {
            if (ref.path.empty()) {
                t.path = path;
                remove_dots = false;
                if (ref.has_query) {
                    t.query = ref.query;
                    t.has_query = true;
                } else {
                    t.query = query;
                    t.has_query = has_query;
                }
            } else {
                t.path = ref.path[0] == '/' ? ref.path : uri_merge_paths(*this, ref.path);
                t.query = ref.query;
                t.has_query = ref.has_query;
            }
            t.authority = authority;
            t.has_authority = has_authority;
        }
        t.scheme = scheme;
        t.has_scheme = has_scheme;
    }
    if (remove_dots) {
        // a relative target (only possible with a relative base) keeps the ".." segments it cannot remove
        t.path = (!t.has_scheme && !t.has_authority && (t.path.empty() || t.path[0] != '/'))
            ? removeRelativeDotSegments(t.path, xsink)
            : removeDotSegments(t.path, xsink);
    }
    t.fragment = ref.fragment;
    t.has_fragment = ref.has_fragment;
    return t;
}

std::string QoreUriReference::getRequestTarget() const {
    std::string rv = path.empty() ? std::string("/") : path;
    if (has_query) {
        rv += '?';
        rv += query;
    }
    return rv;
}

bool QoreUriReference::isScheme(const char* str) const {
    return has_scheme && !strcasecmp(scheme.c_str(), str);
}

// checks a URI reference for the octets that cannot appear in one
static int uri_check_strict(const char* what, const QoreString& value, ExceptionSink* xsink) {
    const char* p = value.c_str();
    size_t len = value.size();
    UriCancelCheck cancelled(xsink);
    for (size_t i = 0; i < len; ++i) {
        if (cancelled()) {
            return -1;
        }
        const unsigned char c = static_cast<unsigned char>(p[i]);
        if (c <= 0x20 || c == 0x7f) {
            xsink->raiseException("RESOLVE-URL-ERROR", "%s has an invalid character (code %d) at byte offset %lld; "
                "a URI reference cannot contain control or space characters", what, (int)c, (long long)i);
            return -1;
        }
        if (c == '%' && (i + 2 >= len || !isxdigit(static_cast<unsigned char>(p[i + 1]))
                || !isxdigit(static_cast<unsigned char>(p[i + 2])))) {
            xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has a malformed percent-encoded octet at byte "
                "offset %lld", what, p, (long long)i);
            return -1;
        }
    }
    return 0;
}

// RFC 3986 section 2.3: unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"
// the ctype functions are not used here: they are locale-dependent for octets >= 0x80, which are decided by the
// IRI policy instead, not by the C library's idea of a letter
static inline bool uri_is_alpha(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline bool uri_is_digit(unsigned char c) {
    return c >= '0' && c <= '9';
}

static inline bool uri_is_hexdig(unsigned char c) {
    return uri_is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static inline bool uri_is_unreserved(unsigned char c) {
    return uri_is_alpha(c) || uri_is_digit(c) || c == '-' || c == '.' || c == '_' || c == '~';
}

// RFC 3986 section 2.2: sub-delims
static inline bool uri_is_sub_delim(unsigned char c) {
    switch (c) {
        case '!':
        case '$':
        case '&':
        case '\'':
        case '(':
        case ')':
        case '*':
        case '+':
        case ',':
        case ';':
        case '=':
            return true;
        default:
            return false;
    }
}

//! Validates one component: unreserved / pct-encoded / sub-delims / the ASCII octets in @p extra
/** @param extra the delimiters the component's production allows on top of the common set, as a NUL-terminated
    ASCII string; a segment allows ":@", a query or fragment ":@/?", userinfo ":", and a reg-name none
*/
static int uri_check_chars(const char* what, const char* part, const char* full, const std::string& value,
        const char* extra, bool ascii_only, ExceptionSink* xsink) {
    const char* p = value.data();
    const size_t len = value.size();
    UriCancelCheck cancelled(xsink);
    for (size_t i = 0; i < len; ++i) {
        if (cancelled()) {
            return -1;
        }
        const unsigned char c = static_cast<unsigned char>(p[i]);
        if (c == '%') {
            // uri_check_strict() has already proven every escape in the whole reference well formed; this keeps
            // validate() usable on its own
            if (i + 2 >= len || !uri_is_hexdig(static_cast<unsigned char>(p[i + 1]))
                    || !uri_is_hexdig(static_cast<unsigned char>(p[i + 2]))) {
                xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has a malformed percent-encoded octet in its "
                    "%s", what, full, part);
                return -1;
            }
            i += 2;
            continue;
        }
        if (c >= 0x80) {
            if (!ascii_only) {
                // accepted as an IRI character (RFC 3987 section 2.2)
                continue;
            }
            xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has a non-ASCII octet (code %d) in its %s; a URI "
                "can only contain ASCII, and RESOLVE_URL_ASCII was given", what, full, (int)c, part);
            return -1;
        }
        if (uri_is_unreserved(c) || uri_is_sub_delim(c) || (c && strchr(extra, c))) {
            continue;
        }
        xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has an invalid character '%c' (code %d) in its %s; "
            "RFC 3986 does not allow it there", what, full, (char)c, (int)c, part);
        return -1;
    }
    return 0;
}

//! Returns true if @p s matches the RFC 3986 \c IPv4address production
/** Leading zeros are not allowed, so \c "010.0.0.1" is not an IPv4 address; as a host it is still a valid
    \c reg-name, and this is only reached for the \c ls32 tail of an IPv6 address
*/
static bool uri_is_ipv4(const char* p, size_t len) {
    unsigned octets = 0;
    size_t i = 0;
    while (i < len) {
        const size_t start = i;
        while (i < len && uri_is_digit(static_cast<unsigned char>(p[i]))) {
            ++i;
        }
        const size_t dlen = i - start;
        // dec-octet = DIGIT / %x31-39 DIGIT / "1" 2DIGIT / "2" %x30-34 DIGIT / "25" %x30-35
        if (!dlen || dlen > 3 || (dlen > 1 && p[start] == '0')) {
            return false;
        }
        unsigned v = 0;
        for (size_t j = 0; j < dlen; ++j) {
            v = v * 10 + static_cast<unsigned>(p[start + j] - '0');
        }
        if (v > 255 || ++octets > 4) {
            return false;
        }
        if (i == len) {
            break;
        }
        if (p[i] != '.' || ++i == len) {
            return false;
        }
    }
    return octets == 4;
}

//! Counts the 16-bit groups in one side of an IPv6 address, or -1 if a group is malformed
/** @param allow_ipv4_tail true for the side that ends the address, where the last group can be an \c IPv4address
    (the \c ls32 production) and then counts as two groups
*/
static int uri_ipv6_groups(const std::string& s, bool allow_ipv4_tail) {
    if (s.empty()) {
        return 0;
    }
    int groups = 0;
    size_t pos = 0;
    while (true) {
        const size_t next = s.find(':', pos);
        const bool last = next == std::string::npos;
        const std::string g = s.substr(pos, last ? std::string::npos : next - pos);
        if (last && allow_ipv4_tail && g.find('.') != std::string::npos) {
            return uri_is_ipv4(g.data(), g.size()) ? groups + 2 : -1;
        }
        // h16 = 1*4HEXDIG
        if (g.empty() || g.size() > 4) {
            return -1;
        }
        for (const char c : g) {
            if (!uri_is_hexdig(static_cast<unsigned char>(c))) {
                return -1;
            }
        }
        ++groups;
        if (last) {
            return groups;
        }
        pos = next + 1;
    }
}

//! Returns true if @p s matches the RFC 3986 \c IPv6address production
/** The grammar bounds a valid address at 45 octets ("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"), so a longer
    string is rejected without scanning it and every loop below is bounded
*/
static bool uri_is_ipv6(const std::string& s) {
    if (s.size() > 45) {
        return false;
    }
    const size_t dbl = s.find("::");
    if (dbl == std::string::npos) {
        return uri_ipv6_groups(s, true) == 8;
    }
    // at most one group of zeros may be elided
    if (s.find("::", dbl + 2) != std::string::npos) {
        return false;
    }
    const int left = uri_ipv6_groups(s.substr(0, dbl), false);
    const int right = uri_ipv6_groups(s.substr(dbl + 2), true);
    return left >= 0 && right >= 0 && left + right <= 7;
}

//! Returns true if @p s matches the RFC 3986 \c IPvFuture production
/** Unlike IPv6address this is unbounded, so the scan checks for cancellation; when it is cancelled the exception
    is in @p xsink and the caller must not raise its own
*/
static bool uri_is_ipvfuture(const std::string& s, ExceptionSink* xsink) {
    // IPvFuture = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
    if (s.size() < 4 || (s[0] != 'v' && s[0] != 'V')) {
        return false;
    }
    // both scans below are over the unbounded bracket content, so both check for cancellation
    UriCancelCheck cancelled(xsink);
    size_t i = 1;
    while (i < s.size() && uri_is_hexdig(static_cast<unsigned char>(s[i]))) {
        if (cancelled()) {
            return false;
        }
        ++i;
    }
    if (i == 1 || i >= s.size() || s[i] != '.' || ++i >= s.size()) {
        return false;
    }
    for (; i < s.size(); ++i) {
        if (cancelled()) {
            return false;
        }
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (!uri_is_unreserved(c) && !uri_is_sub_delim(c) && c != ':') {
            return false;
        }
    }
    return true;
}

//! Validates an authority: [ userinfo "@" ] host [ ":" port ]
static int uri_check_authority(const char* what, const char* full, const std::string& authority, bool ascii_only,
        ExceptionSink* xsink) {
    std::string rest = authority;
    // userinfo cannot itself contain "@", so the last one is the delimiter either way: splitting earlier would
    // only move the extra "@" into the host, where reg-name rejects it
    const size_t at = rest.rfind('@');
    if (at != std::string::npos) {
        if (uri_check_chars(what, "userinfo", full, rest.substr(0, at), ":", ascii_only, xsink)) {
            return -1;
        }
        rest.erase(0, at + 1);
    }

    // find where the host ends: an IP literal contains colons, so the port delimiter is the colon after its "]"
    size_t host_end;
    if (!rest.empty() && rest[0] == '[') {
        const size_t close = rest.find(']');
        if (close == std::string::npos) {
            xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has an unterminated IP literal in its authority; "
                "RFC 3986 requires \"[\" ( IPv6address / IPvFuture ) \"]\"", what, full);
            return -1;
        }
        host_end = close + 1;
        if (host_end < rest.size() && rest[host_end] != ':') {
            xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has characters after the IP literal in its "
                "authority; only a \":\" port can follow \"]\"", what, full);
            return -1;
        }
    } else {
        host_end = rest.find(':');
        if (host_end == std::string::npos) {
            host_end = rest.size();
        }
    }

    if (host_end < rest.size()) {
        // port = *DIGIT
        const std::string port = rest.substr(host_end + 1);
        UriCancelCheck cancelled(xsink);
        for (size_t i = 0; i < port.size(); ++i) {
            if (cancelled()) {
                return -1;
            }
            if (!uri_is_digit(static_cast<unsigned char>(port[i]))) {
                xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has an invalid port '%s' in its authority; "
                    "RFC 3986 allows only digits", what, full, port.c_str());
                return -1;
            }
        }
    }

    const std::string host = rest.substr(0, host_end);
    if (!host.empty() && host[0] == '[') {
        // host_end was set from the "]", so the literal is delimited here
        const std::string lit = host.substr(1, host.size() - 2);
        if (!uri_is_ipv6(lit) && !uri_is_ipvfuture(lit, xsink)) {
            if (*xsink) {
                return -1;
            }
            xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has an invalid IP literal '%s' in its authority; "
                "RFC 3986 requires an IPv6address or an IPvFuture", what, full, host.c_str());
            return -1;
        }
        return 0;
    }
    // host = IPv4address / reg-name; a reg-name accepts every IPv4address, so only the character set is checked
    return uri_check_chars(what, "host", full, host, "", ascii_only, xsink);
}

//! Validates a path in the context of its reference
static int uri_check_path(const char* what, const char* full, const QoreUriReference& ref, bool ascii_only,
        ExceptionSink* xsink) {
    if (ref.has_authority) {
        // path-abempty = *( "/" segment )
        if (!ref.path.empty() && ref.path[0] != '/') {
            xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has an authority and a path that does not begin "
                "with \"/\"; RFC 3986 requires path-abempty there", what, full);
            return -1;
        }
    } else if (ref.path.size() >= 2 && ref.path[0] == '/' && ref.path[1] == '/') {
        // without an authority a path cannot begin with "//" (RFC 3986 section 3.3): it would be read back as one
        xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has no authority and a path that begins with \"//\"; "
            "it would be taken for an authority", what, full);
        return -1;
    }
    // every segment is *pchar and "/" separates them; the extra rule for the first segment of a relative-path
    // reference (segment-nz-nc, which has no colon) is reported by hasColonInFirstRelativeSegment()
    return uri_check_chars(what, "path", full, ref.path, ":@/", ascii_only, xsink);
}

int QoreUriReference::validate(const char* what, const char* full, bool ascii_only, ExceptionSink* xsink) const {
    if (has_scheme && !isValidScheme(scheme.c_str(), scheme.size())) {
        xsink->raiseException("RESOLVE-URL-ERROR", "%s '%s' has an invalid scheme '%s'; RFC 3986 requires "
            "ALPHA *( ALPHA / DIGIT / \"+\" / \"-\" / \".\" )", what, full, scheme.c_str());
        return -1;
    }
    if (has_authority && uri_check_authority(what, full, authority, ascii_only, xsink)) {
        return -1;
    }
    if (uri_check_path(what, full, *this, ascii_only, xsink)) {
        return -1;
    }
    // query = fragment = *( pchar / "/" / "?" )
    if (has_query && uri_check_chars(what, "query", full, query, ":@/?", ascii_only, xsink)) {
        return -1;
    }
    if (has_fragment && uri_check_chars(what, "fragment", full, fragment, ":@/?", ascii_only, xsink)) {
        return -1;
    }
    return 0;
}

QoreStringNode* qore_resolve_url(const QoreString& base, const QoreString& reference, int options,
        ExceptionSink* xsink) {
    TempEncodingHelper b(base, QCS_UTF8, xsink);
    if (*xsink) {
        return nullptr;
    }
    TempEncodingHelper r(reference, QCS_UTF8, xsink);
    if (*xsink) {
        return nullptr;
    }
    // QRU_ASCII refines the strict check rather than acting on its own, so it implies it
    const bool ascii_only = options & QRU_ASCII;
    const bool strict = (options & QRU_STRICT) || ascii_only;
    if (strict && (uri_check_strict("base URI", **b, xsink) || uri_check_strict("URI reference", **r, xsink))) {
        return nullptr;
    }

    QoreUriReference base_ref;
    base_ref.parse(b->c_str(), b->size(), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (!base_ref.has_scheme && !(options & QRU_RELATIVE_BASE)) {
        xsink->raiseException("RESOLVE-URL-ERROR", "base URI '%s' has no scheme; a base URI must be absolute",
            b->c_str());
        return nullptr;
    }
    QoreUriReference ref;
    ref.parse(r->c_str(), r->size(), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (strict) {
        if (base_ref.hasColonInFirstRelativeSegment()) {
            xsink->raiseException("RESOLVE-URL-ERROR", "base URI '%s' is a relative-path reference whose first "
                "segment contains a colon", b->c_str());
            return nullptr;
        }
        if (ref.hasColonInFirstRelativeSegment()) {
            xsink->raiseException("RESOLVE-URL-ERROR", "URI reference '%s' is a relative-path reference whose "
                "first segment contains a colon; prefix it with \"./\"", r->c_str());
            return nullptr;
        }
        // the octet scan above only rejects what can appear nowhere; this is the component grammar itself
        if (base_ref.validate("base URI", b->c_str(), ascii_only, xsink)
                || ref.validate("URI reference", r->c_str(), ascii_only, xsink)) {
            return nullptr;
        }
    }

    QoreUriReference resolved = base_ref.resolve(ref, xsink);
    if (*xsink) {
        return nullptr;
    }
    std::string target = resolved.compose(!(options & QRU_NO_FRAGMENT), options & QRU_ENCODE, xsink);
    if (*xsink) {
        return nullptr;
    }
    return new QoreStringNode(target.data(), target.size(), QCS_UTF8);
}

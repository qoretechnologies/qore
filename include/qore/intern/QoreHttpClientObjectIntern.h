/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttpClientObjectIntern.h

    Qore Programming Language

    Copyright (C) 2006 - 2024 Qore Technologies, s.r.o.

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

#ifndef QORE_HTTP_CLIENT_OBJECT_INTERN_H_
#define QORE_HTTP_CLIENT_OBJECT_INTERN_H_

#include <map>
#include <set>

// ssl-enabled protocols are stored as negative numbers, non-ssl as positive
#define make_protocol(a, b) ((a) * ((b) ? -1 : 1))
#define get_port(a) ((a) * (((a) < 0) ? -1 : 1))
#define get_ssl(a) (((a) < 0) ? true : false)

// protocol map class to recognize user-defined protocols (mostly useful for derived classes)
typedef std::map<std::string, int> prot_map_t;
typedef std::map<std::string, bool, ltstrcase> method_map_t;
typedef std::set<std::string, ltstrcase> strcase_set_t;
typedef std::map<std::string, std::string> header_map_t;

struct con_info {
    int port;
    std::string host,
        path,
        unix_urlencoded_path,
        username,
        password;
    bool ssl = false,
        is_unix = false;

    DLLLOCAL con_info(const con_info& old) : port(old.port), host(old.host), path(old.path),
            unix_urlencoded_path(old.unix_urlencoded_path), username(old.username), password(old.password),
            ssl(old.ssl), is_unix(old.is_unix) {
    }

    DLLLOCAL con_info(int n_port = 0) : port(n_port) {
    }

    DLLLOCAL con_info& operator=(const con_info& other) {
        port = other.port;
        host = other.host;
        path = other.path;
        unix_urlencoded_path = other.unix_urlencoded_path;
        username = other.username;
        password = other.password;
        ssl = other.ssl;
        is_unix = other.is_unix;

        return (*this);
    }

    DLLLOCAL bool has_url() const {
        return !host.empty();
    }

    DLLLOCAL int set_url(QoreURL& url, bool& port_set, ExceptionSink* xsink) {
        port = 0;
        if (url.getPort()) {
            port = url.getPort();
            port_set = true;
        }

        host = url.getHost() ? url.getHost()->c_str() : "";

        // check if hostname is really a local port number (for a URL string like: "8080")
        if (!url.getPort() && !host.empty()) {
            char *aux;
            int val = strtol(host.c_str(), &aux, 10);
            if (aux == (host.c_str() + host.size())) {
                host = HTTPCLIENT_DEFAULT_HOST;
                port = val;
                port_set = true;
            }
        }

        const QoreString* tmp = url.getPath();
        path = tmp ? tmp->c_str() : "";
        tmp = url.getUserName();
        username = tmp ? tmp->c_str() : "";
        tmp = url.getPassword();
        password = tmp ? tmp->c_str() : "";

        if (username.empty() && !password.empty()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "invalid authorization credentials: password set without "
                "username");
            return -1;
        }

        if (!username.empty() && password.empty()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "invalid authorization credentials: username set without "
                "password");
            return -1;
        }

        if (!port && !host.empty() && host[0] == '/') {
            is_unix = true;
            // issue #3474: set URL-encoded path for UNIX domain sockets
            QoreString tmp_host(host);
            QoreString tmp_path;
            tmp_path.concatEncodeUrl(xsink, tmp_host, true);
            if (*xsink) {
                return -1;
            }
            unix_urlencoded_path = tmp_path.c_str();
        }

        return 0;
    }

    DLLLOCAL QoreStringNode* get_url(int opts = URL_NORMAL) const {
        QoreStringNode *pstr = new QoreStringNode("http");
        if (ssl) {
            pstr->concat("s://");
        } else {
            pstr->concat("://");
        }
        if (opts & (UC_USERNAME | UC_PASSWORD)) {
            bool has_username_or_password = false;
            if (!username.empty() && (opts & UC_USERNAME)) {
                pstr->concat(username);
                has_username_or_password = true;
            }
            if (!password.empty() && (opts & UC_PASSWORD)) {
                pstr->concat(':');
                if (opts & UC_MASK_PASSWORD) {
                    pstr->concat("<masked>");
                } else {
                    pstr->concat(password);
                }
                if (!has_username_or_password) {
                    has_username_or_password = true;
                }
            }
            if (has_username_or_password) {
                pstr->concat('@');
            }
        }

        if (!port) {
            // concat and encode "host" when using a UNIX domain socket
            pstr->concat("socket=");
            for (unsigned i = 0; i < host.size(); ++i) {
                char c = host[i];
                switch (c) {
                    case ' ': pstr->concat("%20"); break;
                    case '/': pstr->concat("%2f"); break;
                    default: pstr->concat(c); break;
                }
            }
        } else {
            pstr->concat(host.c_str());
        }
        if (port && ((!ssl && port != 80) || (ssl && port != 443))) {
            pstr->sprintf(":%d", port);
        }
        if (!path.empty() && (opts & UC_PATH)) {
            if (path[0] != '/') {
                pstr->concat('/');
            }
            pstr->concat(path.c_str());
        }
        return pstr;
    }

    DLLLOCAL void setUserPassword(const char* user, const char* pass) {
        assert(user && pass);
        username = user;
        password = pass;
    }

    DLLLOCAL void clearUserPassword() {
        username.clear();
        password.clear();
    }

    DLLLOCAL void clear() {
        port = 0;
        username.clear();
        password.clear();
        host.clear();
        path.clear();
        unix_urlencoded_path.clear();
        ssl = false;
        is_unix = false;
    }
};

DLLLOCAL extern method_map_t method_map;
DLLLOCAL extern strcase_set_t header_ignore;

DLLLOCAL void do_redirect_event(Queue *event_queue, int64 id, const QoreStringNode* loc, const QoreStringNode* msg);
DLLLOCAL void do_event(Queue* event_queue, int64 id, int event);
DLLLOCAL void check_headers(const char* str, int len, bool& multipart, QoreHashNode& ans, const QoreEncoding* enc,
    ExceptionSink *xsink);

#endif

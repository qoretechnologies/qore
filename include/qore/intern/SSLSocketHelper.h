/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SSLSocketHelper.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_SSLSOCKETHELPER_H

#define _QORE_SSLSOCKETHELPER_H

#include <openssl/ssl.h>

#ifdef NEED_SSL_CTX_NEW_CONST
#define SSL_METHOD_CONST const
#else
#define SSL_METHOD_CONST
#endif

struct qore_socket_private;

typedef enum {
    READ,
    WRITE,
    PEEK,
} SslAction;

static inline const char* get_action_method(SslAction action) {
    switch (action) {
        case READ: return "SSL_read";
        case WRITE: return "SSL_write";
        case PEEK: return "SSL_peek";
    }
    assert(false);
    return "<unknown>";
}

class SSLSocketHelper {
public:
    DLLLOCAL SSLSocketHelper(qore_socket_private& qs) : qs(qs) {
    }

    // we do not need atomic dereferences here, all operations must be already locked
    DLLLOCAL bool deref() {
        if (!--refs) {
            delete this;
            return true;
        }
        return false;
    }

    // we do not need atomic dereferences here, all operations must be already locked
    DLLLOCAL void ref() {
        ++refs;
    }

    // do blocking or non-blocking SSL I/O and handle SSL_ERROR_WANT_READ and SSL_ERROR_WANT_WRITE properly
    DLLLOCAL int doSSLRW(ExceptionSink* xsink, const char* mname, void* buf, int num, int timeout_ms,
            SslAction action, bool do_timeout = true);

    // do nonblocking I/O for polling
    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL int doNonBlockingIo(ExceptionSink* xsink, const char* mname, void* buf, size_t size, SslAction action,
            size_t& real_io);

    DLLLOCAL int setClient(ExceptionSink* xsink, const char* mname, const char* sni_target_host, int sd, X509* cert,
            EVP_PKEY* pk, const char* ca_cert_pem = nullptr);
    DLLLOCAL int setServer(ExceptionSink* xsink, const char* mname, int sd, X509* cert, EVP_PKEY* pk,
            const char* ca_cert_pem = nullptr);
    // returns 0 for success
    DLLLOCAL int connect(const char* mname, int timeout_ms, ExceptionSink* xsink);
    // returns 0 for success
    DLLLOCAL int accept(const char* mname, int timeout_ms, ExceptionSink* xsink);
    // returns 0 for success
    DLLLOCAL int shutdown();
    // returns 0 for success
    DLLLOCAL int shutdown(ExceptionSink* xsink);
    // read with optional timeout in milliseconds
    DLLLOCAL int read(ExceptionSink* xsink, const char* mname, char* buf, int size, int timeout_ms,
            bool suppress_exception = false);
    // returns 0 for success
    DLLLOCAL int write(const char* mname, const void* buf, int size, int timeout_ms, ExceptionSink* xsink);

    //! Starts an SSL negotiation for a client in nonblocking mode
    DLLLOCAL int startConnect(ExceptionSink* xsink);

    //! Starts an SSL negotiation for a server in nonblocking mode
    DLLLOCAL int startAccept(ExceptionSink* xsink);

    DLLLOCAL const char* getCipherName() const;
    DLLLOCAL const char* getCipherVersion() const;
    DLLLOCAL X509* getPeerCertificate() const;
    DLLLOCAL long verifyPeerCertificate() const;

    DLLLOCAL void setVerifyMode(int mode, bool accept_all_certs, const std::string& target);

    DLLLOCAL bool captureRemoteCert() const;
    DLLLOCAL void clearRemoteCertContext() const;

private:
    qore_socket_private& qs;
    SSL_METHOD_CONST SSL_METHOD* meth = nullptr;
    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    unsigned refs = 1;

    DLLLOCAL int setIntern(ExceptionSink* xsink, const char* meth, int sd, X509* cert, EVP_PKEY* pk,
            const char* ca_cert_pem = nullptr);

    // non-blocking I/O helper
    DLLLOCAL int doSSLUpgradeNonBlockingIO(int rc, const char* mname, int timeout_ms, const char* ssl_func,
            ExceptionSink* xsink);

    DLLLOCAL ~SSLSocketHelper();

    // must be called with refs > 1
    DLLLOCAL bool sslError(ExceptionSink* xsink, const char* meth, const char* msg, bool always_error = true);

    // must be called with refs > 1
    DLLLOCAL int sysCallError(ExceptionSink* xsink, int rc, const char* mname, const char* ssl_func);

    DLLLOCAL void handleErrorIntern(ExceptionSink* xsink, int e, const char* mname, const char* func,
            bool always_error);
};

class SSLSocketReferenceHelper {
public:
    DLLLOCAL SSLSocketReferenceHelper(SSLSocketHelper* s, bool set_thread_context = false);

    DLLLOCAL ~SSLSocketReferenceHelper();

protected:
    SSLSocketHelper* s;
    bool context_saved = false;
};

#endif

/* -*- indent-tabs-mode: nil -*- */
/*
    QoreSocket.cpp

    Socket Class for IPv4, IPv6 and UNIX domain sockets with SSL support

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

// FIXME: change int to size_t where applicable! (ex: int rc = recv())

#include <qore/Qore.h>
#include <qore/QoreSocket.h>
#include <qore/QoreSocketObject.h>
#include <qore/QoreSSLCertificate.h>

#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/QoreClassIntern.h"

// maximum number of non-blocking network operations before returning
constexpr unsigned max_nonblock_ops = 10;

void se_in_op(const char* cname, const char* meth, ExceptionSink* xsink) {
    assert(xsink);
    xsink->raiseException("SOCKET-IN-CALLBACK", "calls to %s::%s() cannot be made from a callback on an operation on "
        "the same socket", cname, meth);
}

void se_in_op_thread(const char* cname, const char* meth, ExceptionSink* xsink) {
    assert(xsink);
    xsink->raiseException("SOCKET-IN-CALLBACK", "calls to %s::%s() cannot be made from another thread while a "
        "callback operation is in progress on the same socket", cname, meth);
}

void se_not_open(const char* cname, const char* meth, ExceptionSink* xsink, const char* extra) {
    assert(xsink);
    QoreStringNode* desc = new QoreStringNodeMaker("socket must be opened before %s::%s() call", cname, meth);
    if (extra) {
        desc->sprintf(" (%s)", extra);
    }
    xsink->raiseException("SOCKET-NOT-OPEN", desc);
}

void se_timeout(const char* cname, const char* meth, int timeout_ms, ExceptionSink* xsink, const char* extra) {
    assert(xsink);
    QoreStringNodeHolder desc(new QoreStringNodeMaker("timed out after %d millisecond%s in %s::%s() call", timeout_ms,
        timeout_ms == 1 ? "" : "s", cname, meth));
    if (extra) {
        desc->sprintf(" (%s)", extra);
    }
    xsink->raiseException("SOCKET-TIMEOUT", desc.release());
}

void se_closed(const char* cname, const char* mname, ExceptionSink* xsink) {
    assert(xsink);
    xsink->raiseException("SOCKET-CLOSED", "error in %s::%s(): remote end closed the connection", cname, mname);
}

void se_ssl_already_established(const char* cname, const char* mname, ExceptionSink* xsink) {
    assert(xsink);
    xsink->raiseException("SOCKET-SSL-STATE-ERROR", "error in %s::%s(): SSL already established", cname, mname);
}

#ifdef _Q_WINDOWS
int sock_get_raw_error() {
    return WSAGetLastError();
}

int windows_set_errno() {
    int rc = WSAGetLastError();

    switch (rc) {
        case 0:
            errno = 0;
            break;

        case WSANOTINITIALISED:
        case WSAEINVAL:
        case WSAENOTSOCK:
        case WSAEADDRNOTAVAIL:
        case WSAEAFNOSUPPORT:
        case WSAEOPNOTSUPP:
            errno = EINVAL;
            break;

        case WSAEADDRINUSE:
            errno = EIO;
            break;

        case WSAENETDOWN:
            errno = ENODEV;
            break;

        case WSAEFAULT:
            errno = EFAULT;
            break;

        case WSAENOBUFS:
            errno = ENOMEM;
            break;

        case WSAETIMEDOUT:
            errno = ETIMEDOUT;
            break;

        case WSAECONNREFUSED:
            errno = ECONNREFUSED;
            break;

        case WSAEBADF:
            errno = EBADF;
            break;

        case WSAECONNRESET:
        case WSAECONNABORTED:
            errno = ECONNRESET;
            break;

        case WSAEWOULDBLOCK:
            errno = EAGAIN;
            break;

#ifdef DEBUG
        case WSAEALREADY:
        case WSAEINTR:
        case WSAEINPROGRESS:
            // should never get these here
            printd(0, "sock_get_error() got unexpected error code %d; about to assert()\n", rc);
            assert(false);
            errno = EFAULT;
            break;
#endif

        default:
            printd(0, "sock_get_error() unknown code %d; about to assert()\n", rc);
            assert(false);
            errno = EFAULT;
            break;
    }

    return errno;
}

int sock_get_error() {
    return windows_set_errno();
}

int check_windows_rc(int rc) {
    if (rc != SOCKET_ERROR) {
        return 0;
    }

    windows_set_errno();
    return -1;
}

void qore_socket_error_intern(int rc, ExceptionSink* xsink, const char* err, const char* cdesc, const char* mname,
        const char* host, const char* svc, const struct sockaddr *addr) {
    windows_set_errno();
    assert(xsink);

    QoreStringNode* desc = new QoreStringNode;
    if (mname)
        desc->sprintf("error while executing Socket::%s(): ", mname);

    desc->concat(cdesc);

    if (addr) {
        assert(!host);
        assert(!svc);

        concat_target(*desc, addr);
    } else {
        if (host && host[0]) {
            desc->sprintf(" (target: %s", host);
            if (svc)
                desc->sprintf(":%s", svc);
            desc->concat(")");
        }
    }

    if (!errno) {
        xsink->raiseException(err, desc);
        return;
    }

    desc->concat(": ");
    char* buf;
    // get windows error message
    if (!FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, rc, LANG_USER_DEFAULT, (LPTSTR)&buf, 0, 0)) {
        assert(!buf);
        desc->sprintf("Windows FormatMessage() failed on error code %d", rc);
    } else
        assert(buf);

    desc->concat(buf);
    free(buf);

    xsink->raiseException(err, desc);
}

void qore_socket_error(ExceptionSink* xsink, const char* err, const char* cdesc, const char* mname, const char* host,
        const char* svc, const struct sockaddr *addr) {
    qore_socket_error_intern(WSAGetLastError(), xsink, err, cdesc, mname, host, svc, addr);
}
#else
int sock_get_raw_error() {
    return errno;
}

int sock_get_error() {
    return errno;
}

void qore_socket_error_intern(int rc, ExceptionSink* xsink, const char* err, const char* cdesc, const char* mname,
        const char* host, const char* svc, const struct sockaddr *addr) {
    assert(rc);
    assert(xsink);

    QoreStringNode* desc = new QoreStringNode;
    if (mname) {
        desc->sprintf("error while executing Socket::%s(): ", mname);
    }

    desc->concat(cdesc);

    if (addr) {
        assert(!host);
        assert(!svc);

        concat_target(*desc, addr);
    } else {
        if (host) {
            desc->sprintf(" (target: %s", host);
            if (svc)
                desc->sprintf(":%s", svc);
            desc->concat(")");
        }
    }

    xsink->raiseErrnoException(err, rc, desc);
}

void qore_socket_error(ExceptionSink* xsink, const char* err, const char* cdesc, const char* mname, const char* host,
        const char* svc, const struct sockaddr *addr) {
    qore_socket_error_intern(errno, xsink, err, cdesc, mname, host, svc, addr);
}
#endif

int do_read_error(qore_offset_t rc, const char* method_name, int timeout_ms, ExceptionSink* xsink) {
    if (rc > 0) {
        return 0;
    }
    if (!*xsink) {
        QoreSocket::doException(rc, method_name, timeout_ms, xsink);
    }
    return -1;
}

void concat_target(QoreString& str, const struct sockaddr *addr, const char* type) {
    QoreString host;
    q_addr_to_string2(addr, host);
    if (!host.empty()) {
        str.sprintf(" (%s: %s:%d)", type, host.c_str(), q_get_port_from_addr(addr));
    }
}

qore_socket_op_helper::qore_socket_op_helper(qore_socket_private* sock) : s(sock) {
    assert(s->in_op == -1);
    s->in_op = q_gettid();;
}

qore_socket_op_helper::~qore_socket_op_helper() {
    s->in_op = -1;
}

SSLSocketHelperHelper::SSLSocketHelperHelper(qore_socket_private* sock, bool set_thread_context) : s(sock) {
    assert(!s->ssl);
    ssl = s->ssl = new SSLSocketHelper(*sock);

    //printd(5, "SSLSocketHelperHelper::SSLSocketHelperHelper() priv: %p STC: %d CR: %d\n", s, set_thread_context, s->ssl_capture_remote_cert);

    if (set_thread_context && !qore_socket_private::current_socket && s->ssl_capture_remote_cert) {
        qore_socket_private::current_socket = s;
        context_saved = true;
    }
}

SSLSocketHelperHelper::~SSLSocketHelperHelper() {
    if (context_saved) {
        qore_socket_private::current_socket = nullptr;
        //printd(5, "SSLSocketHelperHelper::~SSLSocketHelperHelper() priv: %p RESET\n", s);
    }
}

void SSLSocketHelperHelper::error() {
    ssl->deref();
    if (s->ssl) {
        s->ssl = nullptr;
    }
}

SSLSocketHelper::~SSLSocketHelper() {
    if (ssl) {
        SSL_free(ssl);
    }
    if (ctx) {
        SSL_CTX_free(ctx);
    }
}

int SSLSocketHelper::setIntern(ExceptionSink* xsink, const char* mname, int sd, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    SSLSocketReferenceHelper ssrh(this);

    assert(!ssl);
    assert(!ctx);
    ctx = SSL_CTX_new(meth);
    if (!ctx) {
        sslError(xsink, mname, "SSL_CTX_new");
        assert(*xsink);
        return -1;
    }
    if (cert) {
        if (!SSL_CTX_use_certificate(ctx, cert->getData())) {
            sslError(xsink, mname, "SSL_CTX_use_certificate");
            assert(*xsink);
            return -1;
        }
        if (!cert->priv->chain.empty()) {
            for (auto& i : cert->priv->chain) {
                if (!SSL_CTX_add_extra_chain_cert(ctx, X509_dup(i))) {
                    sslError(xsink, mname, "SSL_CTX_add_extra_chain_cert");
                    assert(*xsink);
                    return -1;
                }
            }
        }
    }
    if (pkey) {
        if (!SSL_CTX_use_PrivateKey(ctx, pkey->getData())) {
            sslError(xsink, mname, "SSL_CTX_use_PrivateKey");
            assert(*xsink);
            return -1;
        }
    }

    ssl = SSL_new(ctx);
    if (!ssl) {
        sslError(xsink, mname, "SSL_new");
        assert(*xsink);
        return -1;
    }

    SSL_set_ex_data(ssl, qore_ssl_data_index, &qs);

    // turn on SSL_MODE_ENABLE_PARTIAL_WRITE
    SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);

    // turn on SSL_MODE_AUTO_RETRY for blocking I/O
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

    // set the socket file descriptor
    SSL_set_fd(ssl, sd);

#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
    // treat EOF as a normal shutdown
    SSL_set_options(ssl, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif

    // set verification mode
    if (qs.ssl_verify_mode != SSL_VERIFY_NONE) {
        setVerifyMode(qs.ssl_verify_mode, qs.ssl_accept_all_certs, qs.client_target);
    }

#if defined(HAVE_SSL_SET_MAX_PROTO_VERSION) && defined(TLS1_3_VERSION)
    if (qore_library_options & QLO_MINIMUM_TLS_13) {
        if (!SSL_set_min_proto_version(ssl, TLS1_3_VERSION)) {
            sslError(xsink, mname, "SSL_set_min_proto_version");
            assert(*xsink);
            return -1;
        }
    } else if (qore_library_options & QLO_DISABLE_TLS_13) {
        if (!SSL_set_max_proto_version(ssl, TLS1_2_VERSION)) {
            sslError(xsink, mname, "SSL_set_max_proto_version");
            assert(*xsink);
            return -1;
        }
    }
#endif

    return 0;
}

int SSLSocketHelper::setClient(ExceptionSink* xsink, const char* mname, const char* sni_target_host, int sd,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) {
#ifdef HAVE_TLS_SERVER_METHOD
    meth = TLS_client_method();
#else
    meth = SSLv23_client_method();
#endif
    int rc = setIntern(xsink, mname, sd, cert, pkey);
    if (!rc && sni_target_host) {
        // issue #3053 set TLS server name for servers that require SNI
        assert(ssl);
        ERR_clear_error();
        if (!SSL_set_tlsext_host_name(ssl, sni_target_host)) {
            sslError(xsink, mname, "SSL_set_tlsext_host_name");
            assert(*xsink);
            return -1;
        }
    }
    return rc;
}

int SSLSocketHelper::setServer(ExceptionSink* xsink, const char* mname, int sd, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
#ifdef HAVE_TLS_SERVER_METHOD
    meth = TLS_server_method();
#else
    meth = SSLv23_server_method();
#endif
    return setIntern(xsink, mname, sd, cert, pkey);
}

// returns 0 for success
int SSLSocketHelper::connect(const char* mname, int timeout_ms, ExceptionSink* xsink) {
    SSLSocketReferenceHelper ssrh(this, true);

    int rc;

    if (timeout_ms >= 0) {
        if (qs.set_non_blocking(true, xsink))
            return qs.close_and_exit();

        while (true) {
            ERR_clear_error();
            rc = SSL_connect(ssl);

            if (rc == -1 && !(rc = doSSLUpgradeNonBlockingIO(rc, mname, timeout_ms, "SSL_connect", xsink))) {
                if (!qs.isOpen())
                    break;
                continue;
            }

            break;
        }

        if (qs.isOpen() && qs.set_non_blocking(false, xsink))
            return qs.close_and_exit();
    } else {
        ERR_clear_error();
        rc = SSL_connect(ssl);
    }

    if (rc <= 0) {
        if (!*xsink)
            sslError(xsink, mname, "SSL_connect", true);
        return -1;
    }

    return 0;
}

// returns 0 for success
int SSLSocketHelper::accept(const char* mname, int timeout_ms, ExceptionSink* xsink) {
    SSLSocketReferenceHelper ssrh(this, true);

    int rc;

    if (timeout_ms >= 0) {
        if (qs.set_non_blocking(true, xsink))
            return qs.close_and_exit();

        while (true) {
            ERR_clear_error();
            rc = SSL_accept(ssl);

            if (rc == -1 && !(rc = doSSLUpgradeNonBlockingIO(rc, mname, timeout_ms, "SSL_accept", xsink))) {
                if (!qs.isOpen())
                    break;
                continue;
            }

            break;
        }

        if (qs.isOpen() && qs.set_non_blocking(false, xsink))
            return qs.close_and_exit();
    } else {
        ERR_clear_error();
        rc = SSL_accept(ssl);
    }

    if (rc <= 0) {
        //printd(5, "SSLSocketHelper::accept() rc: %d\n", rc);
        if (!*xsink)
            sslError(xsink, mname, "SSL_accept", true);
        assert(*xsink);
        return -1;
    }

    return 0;
}

// returns 0 = success, 1 = need SOCK_POLLIN, 2 = need SOCK_POLLOUT, < 0 = error
int SSLSocketHelper::startConnect(ExceptionSink* xsink) {
    SSLSocketReferenceHelper ssrh(this, true);

    OptionalNonBlockingHelper nbh(qs, true, xsink);
    if (*xsink) {
        return QSE_SSL_ERR;
    }

    ERR_clear_error();
    int rc = SSL_connect(ssl);

    if (rc != 1) {
        int err = SSL_get_error(ssl, rc);
        //printd(5, "SSLSocketHelper::startConnect() rc: %d err: %d\n", rc, err);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                return SOCK_POLLIN;
            case SSL_ERROR_WANT_WRITE:
                return SOCK_POLLOUT;
            case SSL_ERROR_SYSCALL: {
                return sysCallError(xsink, rc, "startConnect", "SSL_connect");
            }
        }

        if (sslError(xsink, "startConnect", "SSL_connect", true)) {
            return QSE_SSL_ERR;
        }
    }

    return 0;
}

// returns 0 for success, 1 = need SOCK_POLLIN, 2 = need SOCK_POLLOUT, < 0 = error
int SSLSocketHelper::startAccept(ExceptionSink* xsink) {
    SSLSocketReferenceHelper ssrh(this, true);

    OptionalNonBlockingHelper nbh(qs, true, xsink);
    if (*xsink) {
        return QSE_SSL_ERR;
    }

    ERR_clear_error();
    int rc = SSL_accept(ssl);

    if (rc != 1) {
        int err = SSL_get_error(ssl, rc);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                return SOCK_POLLIN;
            case SSL_ERROR_WANT_WRITE:
                return SOCK_POLLOUT;
            case SSL_ERROR_SYSCALL: {
                return sysCallError(xsink, rc, "startAccept", "SSL_accept");
            }
        }
        if (sslError(xsink, "startAccept", "SSL_accept", true)) {
            return QSE_SSL_ERR;
        }
    }

    return 0;
}

int SSLSocketHelper::sysCallError(ExceptionSink* xsink, int rc, const char* mname, const char* ssl_func) {
     if (!sslError(xsink, mname, ssl_func)) {
        if (!rc) {
            xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported an " \
                "EOF condition that violates the SSL protocol while calling %s()", mname, ssl_func);
        } else if (rc == -1) {
            xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the " \
                "openssl library reported an I/O error while calling %s()", mname, ssl_func);

#ifdef ECONNRESET
            // close the socket if connection reset received
            // do not access "this" after the connection is closed since the SSLSocketHelper has been deleted
            if (qs.isOpen() && sock_get_error() == ECONNRESET)
                qs.close();
#endif
        } else {
            xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported " \
                "error code %d in %s() but the error queue is empty", mname, rc, ssl_func);
        }
    }

    assert(*xsink);
    return QSE_SSL_ERR;
}

// returns 0 for success
int SSLSocketHelper::shutdown() {
   if (SSL_shutdown(ssl) < 0)
      return -1;
   return 0;
}

// returns 0 for success
int SSLSocketHelper::shutdown(ExceptionSink* xsink) {
    ERR_clear_error();
    if (SSL_shutdown(ssl) < 0) {
        SSLSocketReferenceHelper ssrh(this);
        sslError(xsink, "shutdownSSL", "SSL_shutdown");
        return -1;
    }
    return 0;
}

// returns 0 for success
int SSLSocketHelper::write(const char* mname, const void* buf, int size, int timeout_ms, ExceptionSink* xsink) {
    return doSSLRW(xsink, mname, (void*)buf, size, timeout_ms, WRITE);
}

const char* SSLSocketHelper::getCipherName() const {
    return SSL_get_cipher_name(ssl);
}

const char* SSLSocketHelper::getCipherVersion() const {
    return SSL_get_cipher_version(ssl);
}

X509* SSLSocketHelper::getPeerCertificate() const {
    return SSL_get_peer_certificate(ssl);
}

long SSLSocketHelper::verifyPeerCertificate() const {
    X509* cert = SSL_get_peer_certificate(ssl);

    if (!cert) {
        return -1;
    }

    long rc = SSL_get_verify_result(ssl);
    X509_free(cert);
    return rc;
}

int my_socket_priv::checkOpen(ExceptionSink* xsink) {
    // must be called with the lock held
    assert(m.trylock());

    if (checkValid(xsink)) {
        return -1;
    }

    if (!socket->priv->isOpen()) {
        xsink->raiseException("SOCKET-NOT-OPEN", "the underlying socket object is not open, so the operation "
            "cannot continue");
        return -1;
    }

    return 0;
}

int my_socket_priv::checkOpenAndNotSsl(ExceptionSink* xsink) {
    if (checkOpen(xsink)) {
        return -1;
    }

    if (socket->priv->ssl) {
        xsink->raiseException("SOCKET-SSL-CONNECTED", "a TLS/SSL connection has already been established");
        return -1;
    }

    return 0;
}

thread_local qore_socket_private* qore_socket_private::current_socket;

static int q_ssl_verify_accept_all(int preverify_ok, X509_STORE_CTX* x509_ctx) {
    //printd(5, "q_ssl_verify_accept_all() preverify_ok: %d x509_ctx: %p\n", preverify_ok, x509_ctx);
    // issue #3512: get remote certificate if applicable
    qore_socket_private::captureRemoteCert(x509_ctx);
    // accept all certificates
    return 1;
}

static int q_ssl_verify_accept_default(int preverify_ok, X509_STORE_CTX* x509_ctx) {
    printd(5, "q_ssl_verify_accept_default() preverify_ok: %d x509_ctx: %p\n", preverify_ok, x509_ctx);

    // issue #3512: get remote certificate if applicable
    qore_socket_private::captureRemoteCert(x509_ctx);

    // issue #3818: get verbose info for SSL error
    if (!preverify_ok) {
        SSL* ssl = reinterpret_cast<SSL*>(X509_STORE_CTX_get_ex_data(x509_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
        qore_socket_private* qs = reinterpret_cast<qore_socket_private*>(SSL_get_ex_data(ssl, qore_ssl_data_index));

        X509* err_cert = X509_STORE_CTX_get_current_cert(x509_ctx);
        int err = X509_STORE_CTX_get_error(x509_ctx);
        int depth = X509_STORE_CTX_get_error_depth(x509_ctx);

        char buf[256];
        X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);

        SimpleRefHolder<QoreStringNode> ssl_err(new QoreStringNodeMaker("verify error %d depth %d: %s: %s", err,
            depth, X509_verify_cert_error_string(err), buf));

        // At this point, err contains the last verification error
        if (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) {
            X509_NAME_oneline(X509_get_issuer_name(err_cert), buf, 256);
            ssl_err->sprintf(", issuer: %s", buf);
        }

        qs->setSslErrorString(ssl_err.release());
    }

    return preverify_ok;
}

void SSLSocketHelper::setVerifyMode(int mode, bool accept_all_certs, const std::string& target) {
    printd(5, "SSLSocketHelper::setVerifyMode() mode: %d accept_all_certs: %d target: %s\n", mode,
        (int)accept_all_certs, target.c_str());
    if (!accept_all_certs) {
        // issue #3818: load default CAs
        SSL_CTX_set_default_verify_paths(ctx);

#if defined(HAVE_SSL_SET_HOSTFLAGS) && defined(HAVE_SSL_SET1_HOST)
        // issue #3808: enable hostname validation with certificate validation, otherwise all valid certificates are
        // accepted, even if the hostname does not match; see:
        // https://gist.github.com/theopolis/aeaa8e4808f6b09328dd6996a2ed6c34
        SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        if (!SSL_set1_host(ssl, target.c_str())) {
            // FIXME: openssl docs do not specify what can cause the SSL_set1_host() call to fail
            printd(0, "DEBUG: SSL_set1_host() %s failed\n", target.c_str());
        }
#endif
    }

    SSL_set_verify(ssl, mode, accept_all_certs ? q_ssl_verify_accept_all : q_ssl_verify_accept_default);
}

bool SSLSocketHelper::captureRemoteCert() const {
    if (!qore_socket_private::current_socket && qs.ssl_capture_remote_cert) {
        qore_socket_private::current_socket = &qs;
        //printd(5, "SSLSocketHelper::captureRemoteCert() priv: %p current_sock: %p\n", &qs, &qs);
        return true;
    }
    //printd(5, "SSLSocketHelper::captureRemoteCert() priv: %p FALSE\n", &qs);
    return false;
}

void SSLSocketHelper::clearRemoteCertContext() const {
    assert(qore_socket_private::current_socket == &qs);
    qore_socket_private::current_socket = nullptr;
    //printd(5, "SSLSocketHelper::clearRemoteCertContext()\n");
}

SSLSocketReferenceHelper::SSLSocketReferenceHelper(SSLSocketHelper* s, bool set_thread_context) : s(s) {
    s->ref();
    if (set_thread_context && s->captureRemoteCert()) {
        context_saved = true;
    }
}

SSLSocketReferenceHelper::~SSLSocketReferenceHelper() {
    if (context_saved) {
        s->clearRemoteCertContext();
    }
    s->deref();
}

SocketSource::SocketSource() : priv(new qore_socketsource_private) {
}

SocketSource::~SocketSource() {
   delete priv;
}

QoreStringNode* SocketSource::takeAddress() {
   QoreStringNode* addr = priv->address;
   priv->address = 0;
   return addr;
}

QoreStringNode* SocketSource::takeHostName() {
   QoreStringNode* host = priv->hostname;
   priv->hostname = 0;
   return host;
}

const char* SocketSource::getAddress() const {
   return priv->address ? priv->address->c_str() : 0;
}

const char* SocketSource::getHostName() const {
   return priv->hostname ? priv->hostname->c_str() : 0;
}

void SocketSource::setAll(QoreObject *o, ExceptionSink* xsink) {
   return priv->setAll(o, xsink);
}

SocketConnectInetPollState::SocketConnectInetPollState(ExceptionSink* xsink, qore_socket_private* sock, const char* host,
        const char* service, int family, int type, int protocol)
        : sock(sock), host(host), service(service) {
    assert(xsink);

    family = q_get_af(family);
    type = q_get_sock_type(type);

    // close socket if already open
    sock->close();

    sock->do_resolve_event(host, service);

    if (ai.getInfo(xsink, host, service, family, 0, type, protocol)) {
        assert(*xsink);
        return;
    }

    p = ai.getAddrInfo();

    // emit all "resolved" events
    if (sock->event_queue) {
        for (struct addrinfo* p0 = p; p0; p0 = p0->ai_next) {
            sock->do_resolved_event(p0->ai_addr);
        }
    }

    prt = q_get_port_from_addr(p->ai_addr);

    nextIntern(xsink);
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 1 = error (exception raised)
*/
int SocketConnectInetPollState::continuePoll(ExceptionSink* xsink) {
    // set non-blocking
    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    while (true) {
        if (state == SCIPS_CONNECT) {
            int rc = doConnect(xsink);
            //printd(5, "SocketConnectInetPollState::continuePoll() doConnect() returned %d (ex: %d)\n", rc,
            //    (int)*xsink);
            if (*xsink) {
                sock->close_and_reset();
                return -1;
            }
            if (rc) {
                // try next address
                if (next(xsink)) {
                    return -1;
                }
                continue;
            }

            // connect successful; do an immediate check for a connection
            state = SCIPS_CHECK_CONNECT;
        }

        if (state == SCIPS_CHECK_CONNECT) {
            int rc = checkConnection(xsink);
            //printd(5, "SocketConnectInetPollState::continuePoll() checkConnection() returned %d (ex: %d)\n", rc,
            //    (int)*xsink);
            if (*xsink) {
                sock->close_and_reset();
                return -1;
            }

            if (rc == 1) {
                return SOCK_POLLOUT;
            }

            if (rc < 0) {
                state = SCIPS_CONNECT;
                // try next address
                if (next(xsink)) {
                    return -1;
                }
                continue;
            }

            break;
        }
    }

    return 0;
}

int SocketConnectInetPollState::doConnect(ExceptionSink* xsink) {
    while (true) {
        if (!::connect(sock->sock, p->ai_addr, p->ai_addrlen)) {
            return 0;
        }

#ifdef _Q_WINDOWS
        if (sock_get_error() != EAGAIN) {
            qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, 0, 0, p->ai_addr);
            return -1;
        }
#else
        // try again if we were interrupted by a signal
        if (errno == EINTR) {
            continue;
        }

        if (errno != EINPROGRESS && errno != EAGAIN) {
            return -1;
        }
#endif
        break;
    }
    return 0;
}

// returns 0 = connected, 1 = try again, -1 = error
int SocketConnectInetPollState::checkConnection(ExceptionSink* xsink) {
    assert(!*xsink);
    assert(sock->sock);

#ifdef _Q_WINDOWS
    bool aborted = false;
    int rc = sock->select_intern(xsink, 0, false, true, aborted);

    //printd(5, "SocketConnectInetPollState::doPoll() timeout_ms: %d rc: %d aborted: %d\n",
    //    timeout_ms, rc, aborted);

    // windows select() returns an error in the error socket set instead of an WSAECONNREFUSED error like
    // UNIX, so we simulate it here
    if (rc != QORE_SOCKET_ERROR && aborted) {
        qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, 0, 0, p->ai_addr);
        return -1;
    }
#else
    int rc = sock->asyncIoWait(0, false, true, "Socket", "connect", xsink);
#endif
    if (*xsink) {
        return -1;
    }

    if (rc == QORE_SOCKET_ERROR && sock_get_error() != EINTR) {
        return -1;
    }

    // socket selected for write
    socklen_t lon = sizeof(int);
    int val;
    if (getsockopt(sock->sock, SOL_SOCKET, SO_ERROR, (GETSOCKOPT_ARG_4)(&val), &lon) == QORE_SOCKET_ERROR) {
        return -1;
    }

    if (val) {
        errno = val;
        return -1;
    }

    // try a zero-byte send
    rc = send(sock->sock, nullptr, 0, 0);
    if (rc) {
        // NOTE: an ENOTCONN error can be returned on Darwin / macOS even though poll() reports the connection is ready
        // for writing
        if (errno == EINPROGRESS || errno == EAGAIN || errno == ENOTCONN) {
            return 1;
        }
        return -1;
    }

    // connected successfully within the timeout period
    sock->sfamily = p->ai_family;
    sock->stype = p->ai_socktype;
    sock->sprot = p->ai_protocol;
    sock->port = prt;
    sock->confirmConnected(host.c_str());
    return 0;
}

//! Try to go to next address
int SocketConnectInetPollState::next(ExceptionSink* xsink) {
    p = p->ai_next;
    if (!p) {
        qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", nullptr, host.c_str(), service.c_str());
        if (sock->sock != QORE_INVALID_SOCKET) {
            sock->close_and_reset();
        }
        return -1;
    }

    {
        QoreString addr;
        q_addr_to_string2(p->ai_addr, addr);
        //printd(5, "SocketConnectInetPollState::next() trying address: %p family: %s addr: %s\n", p,
        //    q_af_to_str(p->ai_family), addr.c_str());
    }
    return nextIntern(xsink);
}

//! Setup socket with next address
int SocketConnectInetPollState::nextIntern(ExceptionSink* xsink) {
    assert(p);
    sock->do_connect_event(p->ai_family, p->ai_addr, host.c_str(), service.c_str(), prt);
    if ((sock->sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == QORE_INVALID_SOCKET) {
        xsink->raiseErrnoException("SOCKET-CONNECT-ERROR", errno, "cannot establish a connection to %s:%s",
            host.c_str(), service.c_str());
        return -1;
    }
    return 0;
}

#ifndef _Q_WINDOWS
SocketConnectUnixPollState::SocketConnectUnixPollState(ExceptionSink* xsink, qore_socket_private* sock,
        const char* name, int sock_type, int protocol)
        : sock(sock), name(name) {
    assert(xsink);

    // close socket if already open
    sock->close();

    addr.sun_family = AF_UNIX;
    // copy path and terminate if necessary
    strncpy(addr.sun_path, name, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    if ((sock->sock = socket(AF_UNIX, sock_type, protocol)) == QORE_SOCKET_ERROR) {
        xsink->raiseErrnoException("SOCKET-CONNECT-ERROR", errno, "error connecting to UNIX socket: '%s'", name);
        return;
    }

    sock->do_connect_event(AF_UNIX, (sockaddr*)&addr, name);
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 1 = error (exception raised)
*/
int SocketConnectUnixPollState::continuePoll(ExceptionSink* xsink) {
    // set non-blocking
    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    while (true) {
        if (state == SCIPS_CONNECT) {
            int rc = doConnect(xsink);
            //printd(5, "SocketConnectUnixPollState::continuePoll() doConnect() returned %d (ex: %d)\n", rc,
            //    (int)*xsink);
            if (*xsink) {
                sock->close_and_reset();
                return -1;
            }
            if (rc) {
                sock->close_and_reset();
                qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, name.c_str());
                return -1;
            }

            // connect successful; do an immediate check for a connection
            state = SCIPS_CHECK_CONNECT;
        }

        if (state == SCIPS_CHECK_CONNECT) {
            int rc = checkConnection(xsink);
            //printd(5, "SocketConnectUnixPollState::continuePoll() checkConnection() returned %d (ex: %d)\n", rc,
            //    (int)*xsink);
            if (*xsink) {
                sock->close_and_reset();
                return -1;
            }

            if (rc == 1) {
                return SOCK_POLLOUT;
            }

            if (rc < 0) {
                sock->close_and_reset();
                qore_socket_error(xsink, "SOCKET-CONNECT-ERROR", "error in connect()", 0, name.c_str());
                return -1;
            }
        }

        break;
    }
    return 0;
}

int SocketConnectUnixPollState::doConnect(ExceptionSink* xsink) {
    while (true) {
        if (!::connect(sock->sock, (const sockaddr*)&addr, sizeof(struct sockaddr_un))) {
            return 0;
        }

        // try again if we were interrupted by a signal
        if (errno == EINTR) {
            continue;
        }

        if (errno != EINPROGRESS && errno != EAGAIN) {
            return -1;
        }

        break;
    }
    return 0;
}

// returns 0 = connected, 1 = try again, -1 = error
int SocketConnectUnixPollState::checkConnection(ExceptionSink* xsink) {
    assert(!*xsink);
    assert(sock->sock);

    int rc = sock->asyncIoWait(0, false, true, "Socket", "connect", xsink);
    if (*xsink) {
        return -1;
    }

    if (rc == QORE_SOCKET_ERROR && sock_get_error() != EINTR) {
        return -1;
    }

    // socket selected for write
    socklen_t lon = sizeof(int);
    int val;
    if (getsockopt(sock->sock, SOL_SOCKET, SO_ERROR, (GETSOCKOPT_ARG_4)(&val), &lon) == QORE_SOCKET_ERROR) {
        return -1;
    }

    if (val) {
        errno = val;
        return -1;
    }

    // try a zero-byte send
    rc = send(sock->sock, nullptr, 0, 0);
    if (rc) {
        // NOTE: an ENOTCONN error can be returned on Darwin / macOS even though poll() reports the connection is ready
        // for writing
        if (errno == EINPROGRESS || errno == EAGAIN || errno == ENOTCONN) {
            return 1;
        }
        return -1;
    }

    // connected successfully within the timeout period
    sock->socketname = addr.sun_path;
    sock->sfamily = AF_UNIX;
    sock->confirmConnected(nullptr);
    return 0;
}
#endif

SocketConnectSslPollState::SocketConnectSslPollState(ExceptionSink* xsink, qore_socket_private* sock,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) : sock(sock) {
    assert(sock->sock);
    assert(!sock->ssl);
    SSLSocketHelperHelper sshh(sock, true);

    sock->do_start_ssl_event();
    // issue #3053: send target hostname to support SNI
    const char* sni_target_host = sock->client_target.empty() ? "" : sock->client_target.c_str();
    if (sock->ssl->setClient(xsink, "connectSsl", sni_target_host, sock->sock, cert, pkey)) {
        sshh.error();
    }
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 1 = error (exception raised)
*/
int SocketConnectSslPollState::continuePoll(ExceptionSink* xsink) {
    return sock->ssl->startConnect(xsink);
}

SocketSendPollState::SocketSendPollState(ExceptionSink* xsink, qore_socket_private* sock, const char* data,
        size_t size) : sock(sock), data(data), size(size) {
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SocketSendPollState::continuePoll(ExceptionSink* xsink) {
    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    // do not allow more than max_nonblock_ops loops at a time
    unsigned loop = 0;

    while (true) {
        ssize_t rc;
        if (sock->ssl) {
            size_t real_io = 0;
            rc = sock->ssl->doNonBlockingIo(xsink, "send", const_cast<char*>(data + sent), size - sent,
                SslAction::WRITE, real_io);
            if (*xsink) {
                assert(!real_io);
                return -1;
            }
            if (!rc) {
                sent += real_io;
                if (sent == size) {
                    break;
                }
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLOUT;
                }
                // do another send
                continue;
            }
            return rc;
        } else {
            rc = ::send(sock->sock, data + sent, size - sent, 0);
            //printd(5, "SocketSendPollState::continuePoll() left: %ld rc: %ld errno: %d)\n", size - sent, rc,
            //    errno);
            if (*xsink) {
                return -1;
            }
            if (rc >= 0) {
                sent += rc;
                if (sent == size) {
                    break;
                }
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLOUT;
                }
                // do another send
                continue;
            }
            sock_get_error();
            if (errno == EINTR) {
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLOUT;
                }
                continue;
            }
            if (errno == EAGAIN
#ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return SOCK_POLLOUT;
            }
            xsink->raiseErrnoException("SOCKET-SEND-ERROR", errno, "error while executing Socket::send()");
            return -1;
        }
    }
    return 0;
}

SocketRecvPacketPollState::SocketRecvPacketPollState(ExceptionSink* xsink, qore_socket_private* sock) : sock(sock),
        bin(new BinaryNode) {
    // first take any data in the socket buffer
    if (sock->buflen) {
        if (bin->writeTo(0, sock->rbuf + sock->bufoffset, sock->buflen)) {
            xsink->outOfMemory();
            return;
        }
        sock->buflen = 0;
        sock->bufoffset = 0;
        //printd(5, "SocketRecvPacketPollState::SocketRecvPacketPollState() wrote %d bytes of memory from buffer to "
        //    "bin\n", (int)bin->size());
    }
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SocketRecvPacketPollState::continuePoll(ExceptionSink* xsink) {
    if (io) {
        return 0;
    }

    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    size_t realsize = bin->size();

    while (true) {
        // ensure space in the binary object to write the socket data
        if (bin->preallocate(realsize + DEFAULT_SOCKET_BUFSIZE)) {
            xsink->outOfMemory();
            return -1;
        }

        ssize_t rc;
        if (sock->ssl) {
            size_t real_io = 0;
            rc = sock->ssl->doNonBlockingIo(xsink, "read",
                reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + realsize)),
                DEFAULT_SOCKET_BUFSIZE, SslAction::READ, real_io);
            if (*xsink) {
                assert(!real_io);
                bin->setSize(realsize);
                // if we have received data, we must return it
                if (realsize) {
                    xsink->clear();
                    io = true;
                    return 0;
                }
                return -1;
            }
            if (!rc) {
                if (real_io) {
                    realsize += real_io;
                }
                bin->setSize(realsize);
                // unconditionally done if the socket is closed
                if (!sock->isOpen()) {
                    io = true;
                    return 0;
                }
                // done if we have performed max_nonblock_ops loops
                if (++loop >= max_nonblock_ops) {
                    if (realsize) {
                        io = true;
                        return 0;
                    }
                    return SOCK_POLLIN;
                }
                // otherwise continue reading
                continue;
            } else {
                assert(!real_io);
            }
            bin->setSize(realsize);
            if (realsize) {
                io = true;
                return 0;
            }
            return rc;
        } else {
            //printd(5, "SocketRecvPacketPollState::continuePoll() calling recv\n");
            rc = ::recv(sock->sock,
#ifdef _Q_WINDOWS
                const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + received),
#else
                reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + realsize)),
#endif
                DEFAULT_SOCKET_BUFSIZE, 0);
            //printd(5, "SocketRecvPacketPollState::continuePoll() rc: %d errno: %d bin: %d)\n", rc, errno,
            //    (int)realsize);
            if (rc >= 0) {
                if (rc) {
                    realsize += rc;
                }
                bin->setSize(realsize);
                // done if we have received no data ((rc == 0) => EOF)
                if (!rc) {
                    sock->close();
                    io = true;
                    return 0;
                }
                // done if we have done max_nonblock_ops loops
                if (++loop >= max_nonblock_ops) {
                    //printd(5, "SocketRecvPacketPollState::continuePoll() DONE bin: %d)\n", (int)bin->size());
                    io = true;
                    return 0;
                }
                // do another recv
                continue;
            }
            bin->setSize(realsize);
            sock_get_error();
            if (errno == EINTR) {
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    if (realsize) {
                        //printd(5, "SocketRecvPacketPollState::continuePoll() DONE (I/O) bin: %d)\n",
                        //    (int)bin->size());
                        io = true;
                        return 0;
                    }
                    //printd(5, "SocketRecvPacketPollState::continuePoll() waiting\n");
                    return SOCK_POLLIN;
                }
                continue;
            }
            if (realsize) {
                io = true;
                return 0;
            }
            if (errno == EAGAIN
#ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return SOCK_POLLIN;
            }
            xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
            return -1;
        }
    }
    return 0;
}

SocketRecvPollState::SocketRecvPollState(ExceptionSink* xsink, qore_socket_private* sock, size_t size) : sock(sock),
        bin(new BinaryNode), size(size) {
    if (bin->preallocate(size)) {
        xsink->outOfMemory();
        return;
    }
    // first take any data in the socket buffer
    if (sock->buflen) {
        if (sock->buflen <= size) {
            // cannot fail - memory preallocated above
            bin->writeTo(0, sock->rbuf + sock->bufoffset, sock->buflen);
            received = sock->buflen;
            sock->buflen = 0;
            sock->bufoffset = 0;
        } else {
            // cannot fail - memory preallocated above
            bin->writeTo(0, sock->rbuf + sock->bufoffset, size);
            received = size;
            sock->buflen -= size;
            sock->bufoffset += size;
        }
        //printd(5, "SocketRecvPollState::SocketRecvPollState(size: %zu) wrote %d bytes of memory from buffer to "
        //    "bin (remaining %d bytes in buffer)\n", size, (int)received, sock->buflen);
    }
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SocketRecvPollState::continuePoll(ExceptionSink* xsink) {
    if (received == size) {
        return 0;
    }

    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    while (true) {
        ssize_t rc;
        if (sock->ssl) {
            size_t real_io = 0;
            rc = sock->ssl->doNonBlockingIo(xsink, "read",
                reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + received)),
                size - received, SslAction::READ, real_io);
            if (*xsink) {
                return -1;
            }
            if (!rc) {
                received += real_io;
                if (received == size) {
                    bin->setSize(size);
                    break;
                }
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                // do another read
                continue;
            }
            return rc;
        } else {
            rc = ::recv(sock->sock,
#ifdef _Q_WINDOWS
                const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + received),
#else
                reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + received)),
#endif
                size - received, 0);
            //printd(5, "SocketRecvPollState::continuePoll() left: %ld rc: %d errno: %d)\n", size - received, rc,
            //    errno);
            if (*xsink) {
                return -1;
            }
            if (rc >= 0) {
                received += rc;
                if (received == size) {
                    bin->setSize(size);
                    break;
                }
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                // do another recv
                continue;
            }
            sock_get_error();
            if (errno == EINTR) {
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                continue;
            }
            if (errno == EAGAIN
#ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return SOCK_POLLIN;
            }
            xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
            return -1;
        }
    }
    return 0;
}

SocketRecvUntilBytesPollState::SocketRecvUntilBytesPollState(ExceptionSink* xsink, qore_socket_private* sock,
        const char* bytes, size_t size) : sock(sock), bin(new QoreStringNode), bytes(bytes), size(size) {
}

/** returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SocketRecvUntilBytesPollState::continuePoll(ExceptionSink* xsink) {
    if (matched == size) {
        return 0;
    }

    while (true) {
        char c;
        if (sock->readByteFromBuffer(c)) {
            int rc = doRecv(xsink);
            if (!rc) {
                if (!sock->buflen) {
                    xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                        "chunk");
                    return -1;
                }
                continue;
            }
            if (*xsink) {
                //printd(5, "HttpClientRecvChunkedPollState::readSizeIntern() doRecv() return -1\n");
                return -1;
            }
            return rc;
        }

        bin->concat(c);
        if (c == bytes[matched]) {
            ++matched;
            if (matched == size) {
                break;
            }
        } else if (matched) {
            matched = 0;
        }
    }

    return 0;
}

int SocketRecvUntilBytesPollState::doRecv(ExceptionSink* xsink) {
    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    while (true) {
        ssize_t rc;
        if (sock->ssl) {
            size_t real_io = 0;
            rc = sock->ssl->doNonBlockingIo(xsink, "read", sock->rbuf, DEFAULT_SOCKET_BUFSIZE, SslAction::READ,
                real_io);
            if (*xsink) {
                return -1;
            }
            if (!rc) {
                assert(!sock->bufoffset);
                sock->buflen = real_io;
            }
            assert(!rc || rc == 1 || rc == 2 || rc == 3 || rc == -1);
            return rc;
        } else {
            rc = ::recv(sock->sock, sock->rbuf, DEFAULT_SOCKET_BUFSIZE, 0);
            if (!rc) {
                return 0;
            }
            if (rc > 0) {
                assert(!sock->bufoffset);
                sock->buflen = rc;
                break;
            }
            sock_get_error();
            if (errno == EINTR) {
                // do not allow more than max_nonblock_ops loops at a time
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                continue;
            }
            if (errno == EAGAIN
#ifdef EWOULDBLOCK
                || errno == EWOULDBLOCK
#endif
            ) {
                return SOCK_POLLIN;
            }
            xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
            return -1;
        }
    }
    return 0;
}

int qore_socket_private::send(int fd, qore_offset_t size, int timeout_ms, ExceptionSink* xsink) {
    assert(xsink);

    if (!size)
        return 0;
    if (sock == QORE_INVALID_SOCKET) {
        printd(5, "QoreSocket::send() ERROR: sock: %d size: " QSD "\n", sock, size);
        se_not_open("Socket", "send", xsink);
        return -1;
    }

    char* buf = (char*)malloc(sizeof(char) * DEFAULT_SOCKET_BUFSIZE);
    ON_BLOCK_EXIT(free, buf);

    qore_offset_t rc = 0;
    size_t bs = 0;
    while (true) {
        // calculate bytes needed
        size_t bn;
        if (size < 0) {
            bn = DEFAULT_SOCKET_BUFSIZE;
        } else {
            bn = size - bs;
            if (bn > DEFAULT_SOCKET_BUFSIZE)
                bn = DEFAULT_SOCKET_BUFSIZE;
        }
        while (true) {
            rc = ::read(fd, buf, bn);
            if (rc >= 0) {
                break;
            }
            if (errno != EINTR) {
                xsink->raiseErrnoException("FILE-READ-ERROR", errno, "error reading file after " QSD " bytes read in "
                    "Socket::send()", bs);
                break;
            }
        }
        // issue #3038: handle EOF
        if (!rc) {
            if (size < 0) {
                break;
            } else {
                xsink->raiseErrnoException("FILE-READ-ERROR", errno,
                    "premature EOF reading file; " QSD " bytes requested; " QSD " bytes read in Socket::send()",
                    size, bs);
            }
        }
        if (rc < 0) {
            //printd(5, "QoreSocket::send() read error: %s\n", strerror(errno));
            break;
        }

        // send buffer
        int src = send(xsink, "Socket", "send", buf, rc, timeout_ms);
        if (src < 0) {
            printd(5, "QoreSocket::send() send error: %s\n", strerror(errno));
            break;
        }
        bs += rc;
        if (size > 0 && bs >= (size_t)size) {
            rc = 0;
            break;
        }
    }
    return rc;
}

int qore_socket_private::recv(int fd, qore_offset_t size, int timeout_ms, ExceptionSink* xsink) {
    assert(xsink);
    if (!size)
        return 0;
    if (sock == QORE_INVALID_SOCKET) {
        printd(5, "QoreSocket::send() ERROR: sock: %d size: " QSD "\n", sock, size);
        se_not_open("Socket", "recv", xsink);
        return -1;
    }

    char* buf;
    qore_offset_t br = 0;
    qore_offset_t rc;
    while (true) {
        // calculate bytes needed
        int bn;
        if (size == -1) {
            bn = DEFAULT_SOCKET_BUFSIZE;
        } else {
            bn = size - br;
            if (bn > DEFAULT_SOCKET_BUFSIZE)
                bn = DEFAULT_SOCKET_BUFSIZE;
        }

        rc = brecv(xsink, "recv", buf, bn, 0, timeout_ms);
        if (rc <= 0) {
            break;
        }
        br += rc;

        do_data_event(QORE_EVENT_SOCKET_DATA_READ, QORE_SOURCE_SOCKET, buf, rc);

        // write buffer to file descriptor
        char* tbuf = buf;
        while (true) {
            int op_rc = ::write(fd, tbuf, rc);
            if (op_rc > 0) {
                // handle short write
                if (op_rc < rc) {
                    tbuf += op_rc;
                    rc -= op_rc;
                    continue;
                }
                break;
            }
            // write(2) should not return 0, but in case it does, it's treated as an error
            if (errno != EINTR) {
                xsink->raiseErrnoException("FILE-READ-ERROR", errno, "error writing file after " QSD
                    " bytes read in Socket::send()", br);
                break;
            }
        }

        if (size > 0 && br >= size) {
            rc = 0;
            break;
        }
    }
    return (int)rc;
}

void qore_socket_private::captureRemoteCert(X509_STORE_CTX* x509_ctx) {
    assert(x509_ctx);
    //printd(5, "qore_socket_private::captureRemoteCert() x509_ctx: %p current_sock: %p\n", x509_ctx, current_socket);
    if (!current_socket) {
        return;
    }

    X509* x509 = X509_STORE_CTX_get_current_cert(x509_ctx);
    assert(x509);
    // issue #3665: deref any current client cert before assigning
    if (current_socket->remote_cert) {
        current_socket->remote_cert->deref(nullptr);
    }
    current_socket->remote_cert = new QoreObject(QC_SSLCERTIFICATE, getProgram(),
        new QoreSSLCertificate(X509_dup(x509)));
}

QoreListNode* qore_socket_private::poll(const QoreListNode* poll_list, int timeout_ms, ExceptionSink* xsink) {
#ifndef HAVE_POLL
    xsink->raiseException("MISSING-FEATURE-ERROR", "no support for async I/O polling on this platform");
    return nullptr;
#else
    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclSocketPollInfo->getTypeInfo(false)), xsink);

    if (poll_list->empty()) {
        return rv.release();
    }

    PrivateDataListHolder<QoreSocketObject> pdlh(xsink);

    std::vector<pollfd> fds;
    fds.reserve(poll_list->size());

    ConstListIterator li(poll_list);
    while (li.next()) {
        const QoreValue v = li.getValue();
        assert(QoreTypeInfo::getUniqueReturnHashDecl(v.getFullTypeInfo())->equal(hashdeclSocketPollInfo));
        const QoreHashNode* h = v.get<const QoreHashNode>();
        assert(h);
        bool found;
        int64 events = h->getKeyAsBigInt("events", found);

        // get the socket
        QoreObject* obj;
        {
            QoreValue v = h->getKeyValue("socket");
            if (v.getType() != NT_OBJECT) {
                assert(v.isNothing());
                xsink->raiseException("SOCKET-POLL-ERROR", "element %zu/%zu (starting from 1) is missing "
                    "the 'socket' value", li.index() + 1, poll_list->size());
                return nullptr;
            }
            obj = v.get<QoreObject>();
        }

        int fd;
        // first see if the object inherits AbstractPollableIoObjectBase
        TryPrivateDataRefHolder<AbstractPollableIoObjectBase> io(obj, CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink);
        if (*xsink) {
            return nullptr;
        }
        // if so, get the descriptor; this is faster than executing a %Qore method
        if (io) {
            fd = io->getPollableDescriptor();
        } else {
            fd = obj->evalMethod("getPollableDescriptor", nullptr, xsink).getAsBigInt();
            if (*xsink) {
                return nullptr;
            }
        }
        if (fd < 0) {
            xsink->raiseException("SOCKET-NOT-OPEN", "element %zu/%zu (starting from 1) references a " \
                "pollable object that is not open", li.index() + 1, poll_list->size());
            return nullptr;
        }
        //printd(5, "qore_socket_private::poll() %s fd: %d\n", obj->getClassName(), fd);

        short arg = 0;
        if (events & SOCK_POLLIN) {
            arg |= POLLIN;
        }
        if (events & SOCK_POLLOUT) {
            arg |= POLLOUT;
        }

        if (!arg) {
            xsink->raiseException("SOCKET-POLL-ERROR", "element %zu/%zu (starting from 1) has an invalid " \
                "'events' value; neither SOCK_POLLIN nor SOCK_POLLOUT is set", li.index() + 1, poll_list->size());
            return nullptr;
        }

        fds[li.index()] = {fd, arg, 0};
    }

    int rc;
    while (true) {
        rc = ::poll(&fds[0], poll_list->size(), timeout_ms);
        if (rc == -1 && errno == EINTR) {
            continue;
        }
        if (rc < 0) {
            qore_socket_error(xsink, "SOCKET-POLL-ERROR", "poll(2) returned an error");
            break;
        }

        // scan results for errors
        for (unsigned i = 0; i < poll_list->size(); ++i) {
            int events = 0;
            if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                events = SOCK_POLLERR;
            } else {
                if (fds[i].revents & POLLIN) {
                    events |= SOCK_POLLIN;
                }
                if (fds[i].revents & POLLOUT) {
                    events |= SOCK_POLLOUT;
                }
            }
            if (events) {
                const QoreHashNode* orig = poll_list->retrieveEntry(i).get<const QoreHashNode>();

                ReferenceHolder<QoreHashNode> entry(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
                assert(!*xsink);
                entry->setKeyValue("events", events, xsink);
                entry->setKeyValue("socket", orig->getKeyValue("socket").refSelf(), xsink);
                rv->push(entry.release(), xsink);
                assert(!*xsink);
            }
        }
        if (*xsink || rv->size()) {
            break;
        }
    }

    return rv.release();
#endif
}

void QoreSocket::doException(int rc, const char* meth, int timeout_ms, ExceptionSink* xsink) {
    assert(xsink);
    switch (rc) {
        case 0:
            se_closed("Socket", meth, xsink);
            break;
        case QSE_RECV_ERR: // recv() error
            xsink->raiseException("SOCKET-RECV-ERROR", q_strerror(errno));
            break;
        case QSE_NOT_OPEN:
            se_not_open("Socket", meth, xsink);
            break;
        case QSE_TIMEOUT:
            se_timeout("Socket", meth, timeout_ms, xsink);
            break;
        case QSE_SSL_ERR:
            xsink->raiseException("SOCKET-SSL-ERROR", "SSL error in Socket::%s() call", meth);
            break;
        case QSE_IN_OP:
            se_in_op("Socket", meth, xsink);
            break;
        case QSE_IN_OP_THREAD:
            se_in_op_thread("Socket", meth, xsink);
            break;
        default:
            xsink->raiseException("SOCKET-ERROR", "unknown internal error code %d in Socket::%s() call", rc, meth);
            break;
    }
}

#ifndef HAVE_SSL_READ_EX
DLLLOCAL int SSL_read_ex(SSL* ssl, void* buf, size_t num, size_t* readbytes) {
    (*readbytes) = 0;
    int rc = SSL_read(ssl, buf, num);
    if (rc > 0) {
        (*readbytes) = rc;
        rc = 1;
    } else {
        rc = 0;
    }
    return rc;
}

DLLLOCAL int SSL_peek_ex(SSL* ssl, void* buf, size_t num, size_t* readbytes) {
    (*readbytes) = 0;
    int rc = SSL_peek(ssl, buf, num);
    if (rc > 0) {
        (*readbytes) = rc;
        rc = 1;
    } else {
        rc = 0;
    }
    return rc;
}

DLLLOCAL int SSL_write_ex(SSL* ssl, const void* buf, size_t num, size_t* written) {
    (*written) = 0;
    int rc = SSL_write(ssl, buf, num);
    if (rc > 0) {
        (*written) = rc;
        rc = 1;
    } else {
        rc = 0;
    }
    return rc;
}
#endif

/**
    we assume the socket has already been put in a nonblock state

    returns:
    - SOCK_POLLIN = wait for read and call this again
    - SOCK_POLLOUT = wait for write and call this again
    - 0 = done
    - < 0 = error (exception raised)
*/
int SSLSocketHelper::doNonBlockingIo(ExceptionSink* xsink, const char* mname, void* buf, size_t size,
        SslAction action, size_t& real_io) {
    //printd(5, "SSLSocketHelper::doNonBlockingIo() %s size: %ld action: %s\n", mname, size,
    //    get_action_method(action));
    assert(xsink);
    assert(size);
    assert(!real_io);
    SSLSocketReferenceHelper ssrh(this);

    int rc;
    while (true) {
        ERR_clear_error();
        switch (action) {
            case READ:
                rc = SSL_read_ex(ssl, buf, size, &real_io);
                break;
            case WRITE:
                rc = SSL_write_ex(ssl, buf, size, &real_io);
                break;
            case PEEK:
                rc = SSL_peek_ex(ssl, buf, size, &real_io);
                break;
        }

        //printd(5, "SSLSocketHelper::doNonBlockingIo() %s size: %ld action: %s rc: %d real_io: %ld\n", mname, size,
        //    get_action_method(action), rc, real_io);

        if (rc > 0) {
            rc = 0;
            break;
        }

        int err = SSL_get_error(ssl, rc);

        //printd(5, "SSLSocketHelper::doNonBlockingIo() %s size: %ld action: %s err: %d\n", mname, size,
        //    get_action_method(action), err);

        if (err == SSL_ERROR_WANT_READ) {
            rc = SOCK_POLLIN;
            break;
        } else if (err == SSL_ERROR_WANT_WRITE) {
            rc = SOCK_POLLOUT;
            break;
        } else if (err == SSL_ERROR_ZERO_RETURN) {
            // here we allow the remote side to disconnect and return 0 the first time just like regular recv()
            if (action != WRITE) {
                rc = 0;
            } else {
                if (!sslError(xsink, mname, "SSL_write"))
                    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the socket was closed by the "
                        "remote host while calling SSL_write()", mname);
                rc = QSE_SSL_ERR;
            }
            // close the local socket unconditionally
            qs.close();

            break;
        } else if (err == SSL_ERROR_SYSCALL) {
            if (!sslError(xsink, mname, get_action_method(action), action == WRITE)) {
                if (!rc) {
                    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported " \
                        "an EOF condition that violates the SSL protocol while calling %s()", mname,
                        get_action_method(action));
                } else if (rc == -1) {
                    xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the " \
                        "openssl library reported an I/O error while calling %s()", mname, get_action_method(action));
                } else {
                    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported " \
                        "error code %d in %s() but the error queue is empty", mname, rc, get_action_method(action));
                }
            }
            // close the local socket unconditionally
            qs.close();
            assert(*xsink);
            rc = QSE_SSL_ERR;
            break;
        } else if (err == SSL_ERROR_SSL) {
            qs.close();
            xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the " \
                "openssl library reported a fatal I/O error while calling %s()", mname, get_action_method(action));
            rc = QSE_SSL_ERR;
            break;
        } else {
            //printd(5, "SSLSocketHelper::doNonBlockingIo(buf: %p size: %ld) rc: %d err: %d\n", buf, size, rc, err);
            // always throw an exception if an error occurs while writing
            if (!sslError(xsink, mname, get_action_method(action), action == WRITE)) {
                rc = 0;
            }
            break;
        }
    }

    //printd(5, "SSLSocketHelper::doNonBlockingIo(buf: %p size: %d action: %d) rc: %d\n", buf, size, action, rc);
    return rc;
}

int SSLSocketHelper::doSSLRW(ExceptionSink* xsink, const char* mname, void* buf, int size, int timeout_ms,
        SslAction action, bool do_timeout) {
    //printd(5, "SSLSocketHelper::doSSLRW() %s size: %d timeout_ms: %d read: %d do_timeout: %d\n", mname, size,
    //    timeout_ms, read, do_timeout);
    assert(xsink);
    assert(size);
    SSLSocketReferenceHelper ssrh(this);

    if (timeout_ms < 0) {
        while (true) {
            int rc;
            ERR_clear_error();
            switch (action) {
                case READ:
                    rc = SSL_read(ssl, buf, size);
                    break;
                case WRITE:
                    rc = SSL_write(ssl, buf, size);
                    break;
                case PEEK:
                    rc = SSL_peek(ssl, buf, size);
                    break;
            }
            if (rc <= 0) {
                // we set SSL_MODE_AUTO_RETRY so there should never be any need to retry
                // issue 1729: only return 0 when reading, indicating that the remote closed the connection
                if (!sslError(xsink, mname, get_action_method(action), action == WRITE ? true : false)) {
                    rc = 0;
                }
            }
            return rc;
        }
    }

    // set non blocking
    OptionalNonBlockingHelper nbh(qs, true, xsink);
    if (*xsink) {
        return -1;
    }

    int rc;
    while (true) {
        ERR_clear_error();
        switch (action) {
            case READ:
                rc = SSL_read(ssl, buf, size);
                break;
            case WRITE:
                rc = SSL_write(ssl, buf, size);
                break;
            case PEEK:
                rc = SSL_peek(ssl, buf, size);
                break;
            default:
                assert(false);
        }

        if (rc > 0) {
            break;
        }

        int err = SSL_get_error(ssl, rc);

        if (err == SSL_ERROR_WANT_READ) {
            if (!timeout_ms) {
                rc = QSE_TIMEOUT;
                break;
            }
            if (!qs.isSocketDataAvailable(timeout_ms, mname, xsink)) {
                if (*xsink) {
                    return -1;
                }
                if (do_timeout && timeout_ms) {
                    se_timeout("Socket", mname, timeout_ms, xsink);
                }
                rc = QSE_TIMEOUT;
                break;
            }
        } else if (err == SSL_ERROR_WANT_WRITE) {
            if (!timeout_ms) {
                rc = QSE_TIMEOUT;
                break;
            }
            if (!qs.isWriteFinished(timeout_ms, mname, xsink)) {
                if (*xsink) {
                    return -1;
                }
                if (do_timeout && timeout_ms) {
                    se_timeout("Socket", mname, timeout_ms, xsink);
                }
                rc = QSE_TIMEOUT;
                break;
            }
        } else if (err == SSL_ERROR_ZERO_RETURN) {
            // here we allow the remote side to disconnect and return 0 the first time just like regular recv()
            if (action != WRITE) {
                rc = 0;
            } else {
                if (!sslError(xsink, mname, "SSL_write")) {
                    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the socket was closed by the "
                        "remote host while calling SSL_write()", mname);
                }
                rc = QSE_SSL_ERR;
            }
            // close the socket unconditionally
            qs.close();
            break;
        } else if (err == SSL_ERROR_SYSCALL) {
            if (!sslError(xsink, mname, get_action_method(action), action == WRITE)) {
                if (rc == -1) {
                    xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the " \
                        "openssl library reported an I/O error while calling %s()", mname, get_action_method(action));
                } else if (rc) {
                    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported " \
                        "error code %d in %s() but the error queue is empty", mname, rc, get_action_method(action));
                }
            }
            // close the socket unconditionally
            qs.close();
            rc = !*xsink ? 0 : QSE_SSL_ERR;
            break;
        } else if (err == SSL_ERROR_SSL) {
            // close the socket unconditionally
            qs.close();
            xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the " \
                "openssl library reported a fatal I/O error while calling %s()", mname, get_action_method(action));
            rc = QSE_SSL_ERR;
        } else {
            //printd(5, "SSLSocketHelper::doSSLRW(buf: %p, size: %d, to: %d) rc: %d err: %d\n", buf, size, timeout_ms,
            //    rc, err);
            // always throw an exception if an error occurs while writing
            if (!sslError(xsink, mname, get_action_method(action), action == WRITE)) {
                rc = 0;
            }
            break;
        }
    }

    //printd(5, "SSLSocketHelper::doSSLRW(buf: %p, size: %d, to: %d, read: %d) rc: %d\n", buf, size, timeout_ms,
    //    (int)read, rc);
    return rc;
}

// if we close the connection due to a socket error, then the SSLSocketHelper object is deleted, therefore have to
// ensure that we do not access "this" after the connection is closed
int SSLSocketHelper::doSSLUpgradeNonBlockingIO(int rc, const char* mname, int timeout_ms, const char* ssl_func,
        ExceptionSink* xsink) {
    assert(xsink);
    SSLSocketReferenceHelper ssrh(this, true);

    int err = SSL_get_error(ssl, rc);

    if (err == SSL_ERROR_WANT_READ) {
        if (qs.isSocketDataAvailable(timeout_ms, mname, xsink)) {
            return 0;
        }

        if (*xsink) {
            return -1;
        }
        se_timeout("Socket", mname, timeout_ms, xsink);
        return QSE_TIMEOUT;
    }

    if (err == SSL_ERROR_WANT_WRITE) {
        if (qs.isWriteFinished(timeout_ms, mname, xsink)) {
            return 0;
        }

        if (*xsink) {
            return -1;
        }
        se_timeout("Socket", mname, timeout_ms, xsink);
        return QSE_TIMEOUT;
    }

    if (err == SSL_ERROR_SYSCALL) {
        return sysCallError(xsink, rc, mname, ssl_func);
    }

    //printd(5, "SSLSocketHelper::doSSLNonBlockingIO(buf: %p, size: %d, to: %d) rc: %d err: %d\n", buf, size,
    //    timeout_ms, rc, err);
    // always throw an exception if an error occurs while writing
    if (!sslError(xsink, mname, ssl_func, true)) {
        return 0;
    }

    return !*xsink ? 0 : QSE_SSL_ERR;
}

DLLLOCAL OptionalNonBlockingHelper::OptionalNonBlockingHelper(qore_socket_private& s, bool set, ExceptionSink* xs)
        : sock(s), xsink(xs), set(set) {
    if (set) {
        //printd(5, "OptionalNonBlockingHelper::OptionalNonBlockingHelper() this: %p\n", this);
        sock.set_non_blocking(true, xsink);
    }
}

DLLLOCAL OptionalNonBlockingHelper::~OptionalNonBlockingHelper() {
    if (set && sock.isOpen()) {
        //printd(5, "OptionalNonBlockingHelper::~OptionalNonBlockingHelper() this: %p\n", this);
        sock.set_non_blocking(false, xsink);
    }
}

int SSLSocketHelper::read(ExceptionSink* xsink, const char* mname, char* buf, int size, int timeout_ms,
        bool suppress_exception) {
    int rc = doSSLRW(xsink, mname, buf, size, timeout_ms, READ, true);
    if (suppress_exception && *xsink) {
        xsink->clear();
    }
    return rc;
}

// returns true if an error was raised or the connection was closed, false if not
bool SSLSocketHelper::sslError(ExceptionSink* xsink, const char* mname, const char* func, bool always_error) {
    assert(refs > 1);
    assert(xsink);

    long e = ERR_get_error();
    do {
        //printd(5, "SSLSocketHelper::sslError() '%s' func: '%s' always_error: %d e: %ld\n", mname, func, always_error, e);
        handleErrorIntern(xsink, e ? e : SSL_ERROR_ZERO_RETURN, mname, func, always_error);
    } while ((e = ERR_get_error()));

    return *xsink || !qs.isOpen();
}

void SSLSocketHelper::handleErrorIntern(ExceptionSink* xsink, int e, const char* mname, const char* func,
        bool always_error) {
    if (e == SSL_ERROR_ZERO_RETURN) {
        // the remote end has closed the connection, so we close this end as well
        qs.close();
        if (always_error) {
            xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the %s() call could not be " \
                "completed because the TLS/SSL connection was terminated (err: %d)", mname, func, e);
        }
    } else {
        char buf[121];
        ERR_error_string(e, buf);
        SimpleRefHolder<QoreStringNode> errstr(new QoreStringNodeMaker("error in Socket::%s(): %s(): %s", mname,
            func, buf));
        // issue #3818: consume any ssl_err_str remaining
        if (qs.ssl_err_str) {
            errstr->concat(": ");
            errstr->concat(qs.ssl_err_str);
            qs.ssl_err_str->deref();
            qs.ssl_err_str = nullptr;
        }
        xsink->raiseException("SOCKET-SSL-ERROR", errstr.release());
#ifdef ECONNRESET
        // close the socket if connection reset received
        if (e == SSL_ERROR_SYSCALL && sock_get_error() == ECONNRESET) {
            //printd(5, "SSLSocketHelper::handleErrorIntern() Socket::%s() (%s) socket closed by remote end\n", mname, func);
            qs.close();
        }
#endif
    }
}

PrivateQoreSocketTimeoutHelper::PrivateQoreSocketTimeoutHelper(qore_socket_private* s, const char* o)
        : PrivateQoreSocketTimeoutBase(s->tl_warning_us ? s : 0), op(o) {
}

PrivateQoreSocketTimeoutHelper::~PrivateQoreSocketTimeoutHelper() {
    if (!sock)
        return;

    int64 dt = q_clock_getmicros() - start;
    if (dt >= sock->tl_warning_us)
        sock->doTimeoutWarning(op, dt);
}

PrivateQoreSocketThroughputHelper::PrivateQoreSocketThroughputHelper(qore_socket_private* s, bool snd)
        : PrivateQoreSocketTimeoutBase(s), send(snd) {
}

PrivateQoreSocketThroughputHelper::~PrivateQoreSocketThroughputHelper() {
}

void PrivateQoreSocketThroughputHelper::finalize(int64 bytes) {
    //printd(5, "PrivateQoreSocketThroughputHelper::finalize() bytes: " QLLD " us: " QLLD " (min: " QLLD ") bs: %.6f "
    //    "threshold: %.6f\n", bytes, (q_clock_getmicros() - start), sock->tp_us_min, ((double)bytes /
    //    ((double)(q_clock_getmicros() - start) / (double)1000000.0)), sock->tp_warning_bs);

    if (bytes < DEFAULT_SOCKET_MIN_THRESHOLD_BYTES) {
        return;
    }

    if (send) {
        sock->tp_bytes_sent += bytes;
    } else {
        sock->tp_bytes_recv += bytes;
    }

    if (!sock->tp_warning_bs) {
        return;
    }

    int64 dt = q_clock_getmicros() - start;

    // ignore if less than event time threshold
    if (dt < sock->tp_us_min) {
        return;
    }

    double bs = (double)bytes / ((double)dt / (double)1000000.0);

    //printd(5, "PrivateQoreSocketThroughputHelper::finalize() bytes: " QLLD " us: " QLLD " bs: %.6f threshold: "
    //    %.6f\n", bytes, dt, bs, sock->tp_warning_bs);

    if (bs <= (double)sock->tp_warning_bs) {
        sock->doThroughputWarning(send, bytes, dt, bs);
    }
}

QoreSocket::QoreSocket() : priv(new qore_socket_private) {
}

QoreSocket::QoreSocket(int n_sock, int n_sfamily, int n_stype, int n_prot, const QoreEncoding* n_enc)
        : priv(new qore_socket_private(n_sock, n_sfamily, n_stype, n_prot, n_enc)) {
}

QoreSocket::~QoreSocket() {
    delete priv;
}

int QoreSocket::setNoDelay(int nodelay) {
    return setsockopt(priv->sock, IPPROTO_TCP, TCP_NODELAY, (SETSOCKOPT_ARG_4)&nodelay, sizeof(int));
}

int QoreSocket::getNoDelay() const {
    int rc;
    socklen_t optlen = sizeof(int);
    int sorc = getsockopt(priv->sock, IPPROTO_TCP, TCP_NODELAY, (GETSOCKOPT_ARG_4)&rc, &optlen);
    //printd(5, "Socket::getNoDelay() sorc: %d rc: %d optlen: %d\n", sorc, rc, optlen);
    if (sorc)
        return sorc;
    return rc;
}

int QoreSocket::close() {
    return priv->close();
}

int QoreSocket::shutdown() {
    int rc;
    if (priv->sock != QORE_INVALID_SOCKET) {
        rc = ::shutdown(priv->sock, SHUTDOWN_ARG);
    } else {
        rc = 0;
    }

    return rc;
}

int QoreSocket::shutdownSSL(ExceptionSink* xsink) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        return 0;
    }
    if (!priv->ssl) {
        return 0;
    }
    return priv->ssl->shutdown(xsink);
}

int QoreSocket::getSocket() const {
    return priv->sock;
}

const QoreEncoding* QoreSocket::getEncoding() const {
    return priv->enc;
}

void QoreSocket::setEncoding(const QoreEncoding* id) {
    priv->enc = id;
}

bool QoreSocket::isOpen() const {
    return (bool)(priv->sock != QORE_INVALID_SOCKET);
}

const char* QoreSocket::getSSLCipherName() const {
    if (!priv->ssl) {
        return nullptr;
    }
    return priv->ssl->getCipherName();
}

const char* QoreSocket::getSSLCipherVersion() const {
    if (!priv->ssl) {
        return nullptr;
    }
    return priv->ssl->getCipherVersion();
}

bool QoreSocket::isSecure() const {
    return (bool)priv->ssl;
}

long QoreSocket::verifyPeerCertificate() const {
    if (!priv->ssl) {
        return -1;
    }
    return priv->ssl->verifyPeerCertificate();
}

// hardcoded to SOCK_STREAM (tcp only)
int QoreSocket::connectINET(const char* host, int prt, int timeout_ms, ExceptionSink* xsink) {
    QoreString service;
    service.sprintf("%d", prt);

    return priv->connectINET(host, service.c_str(), timeout_ms, xsink);
}

int QoreSocket::connectINET(const char* host, int prt, ExceptionSink* xsink) {
    QoreString service;
    service.sprintf("%d", prt);

    return priv->connectINET(host, service.c_str(), -1, xsink);
}

int QoreSocket::connectINET2(const char* name, const char* service, int family, int socktype, int protocol,
        int timeout_ms, ExceptionSink* xsink) {
    return priv->connectINET(name, service, timeout_ms, xsink, family, socktype, protocol);
}

int QoreSocket::connectUNIX(const char* p, ExceptionSink* xsink) {
    return priv->connectUNIX(p, SOCK_STREAM, 0, xsink);
}

int QoreSocket::connectUNIX(const char* p, int sock_type, int protocol, ExceptionSink* xsink) {
   return priv->connectUNIX(p, sock_type, protocol, xsink);
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// starts a connection to a remote socket
// for AF_INET sockets:
// * QoreSocket::startConnect("hostname:<port_number>");
// for AF_UNIX sockets:
// * QoreSocket::startConnect("filename");
AbstractPollState* QoreSocket::startConnect(ExceptionSink* xsink, const char* name) {
    const char* p;

    if ((p = strrchr(name, ':'))) {
        QoreString host(name, p - name);
        QoreString service(p + 1);
        // if the address is an ipv6 address like: [<addr>], then connect as ipv6
        if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
            host.terminate(host.strlen() - 1);
            //printd(5, "QoreSocket::connect(%s, %s) [ipv6]\n", host.c_str() + 1, service.c_str());
            return new SocketConnectInetPollState(xsink, priv, host.c_str() + 1, service.c_str(), AF_INET6);
        }
        return new SocketConnectInetPollState(xsink, priv, host.c_str(), service.c_str());
    }

    // otherwise assume it's a file name for a UNIX domain socket
#ifndef _Q_WINDOWS
    return new SocketConnectUnixPollState(xsink, priv, name);
#else
    missing_function_error("Socket::startConnect(<UNIX socket file>)", "UNIX_FILEMGT", xsink);
    return nullptr;
#endif
}

AbstractPollState* QoreSocket::startSslConnect(ExceptionSink* xsink, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startSslConnect", xsink);
        return nullptr;
    }
    if (priv->ssl) {
        se_ssl_already_established("Socket", "startSslConnect", xsink);
        return nullptr;
    }

    return new SocketConnectSslPollState(xsink, priv, cert, pkey);
}

AbstractPollState* QoreSocket::startSend(ExceptionSink* xsink, const char* data, size_t size) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startSend", xsink);
        return nullptr;
    }

    return new SocketSendPollState(xsink, priv, data, size);
}

AbstractPollState* QoreSocket::startRecv(ExceptionSink* xsink, size_t size) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startRecv", xsink);
        return nullptr;
    }

    return new SocketRecvPollState(xsink, priv, size);
}

AbstractPollState* QoreSocket::startRecvUntilBytes(ExceptionSink* xsink, const char* pattern, size_t size) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startRecvUntilBytes", xsink);
        return nullptr;
    }

    return new SocketRecvUntilBytesPollState(xsink, priv, pattern, size);
}

AbstractPollState* QoreSocket::startRecvPacket(ExceptionSink* xsink) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startRecvPacket", xsink);
        return nullptr;
    }

    return new SocketRecvPacketPollState(xsink, priv);
}

#if 0
AbstractPollState* QoreSocket::startAccept(ExceptionSink* xsink) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startAccept", xsink);
        return nullptr;
    }
    return new SocketAcceptPollState(xsink, priv);
}

AbstractPollState* QoreSocket::startSslAccept(ExceptionSink* xsink, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startSslAccept", xsink);
        return nullptr;
    }
    if (priv->ssl) {
        se_ssl_already_established("Socket", "startSslAccept", xsink);
        return nullptr;
    }
    return new SocketAcceptSslPollState(xsink, priv, cert, pkey);
}
#endif

// currently hardcoded to SOCK_STREAM (tcp-only)
// opens and connects to a remote socket
// for AF_INET sockets:
// * QoreSocket::connect("hostname:<port_number>");
// for AF_UNIX sockets:
// * QoreSocket::connect("filename");
int QoreSocket::connect(const char* name, int timeout_ms, ExceptionSink* xsink) {
    const char* p;
    int rc;

    if ((p = strrchr(name, ':'))) {
        QoreString host(name, p - name);
        QoreString service(p + 1);
        // if the address is an ipv6 address like: [<addr>], then connect as ipv6
        if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
            host.terminate(host.strlen() - 1);
            //printd(5, "QoreSocket::connect(%s, %s) [ipv6]\n", host.c_str() + 1, service.c_str());
            rc = priv->connectINET(host.c_str() + 1, service.c_str(), timeout_ms, xsink, AF_INET6);
        } else {
            rc = priv->connectINET(host.c_str(), service.c_str(), timeout_ms, xsink);
        }
    } else {
        // else assume it's a file name for a UNIX domain socket
        rc = priv->connectUNIX(name, SOCK_STREAM, 0, xsink);
    }

    return rc;
}

int QoreSocket::connect(const char* name, ExceptionSink* xsink) {
   return connect(name, -1, xsink);
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// opens and connects to a remote socket and negotiates an SSL connection
// for AF_INET sockets:
// * QoreSocket::connectSSL("hostname:<port_number>");
// for AF_UNIX sockets:
// * QoreSocket::connectSSL("filename");
int QoreSocket::connectSSL(ExceptionSink* xsink, const char* name, int timeout_ms, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    const char* p;
    int rc;

    if ((p = strchr(name, ':'))) {
        QoreString host(name, p - name);
        QoreString service(p + 1);
        // if the address is an ipv6 address like: [<addr>], then connect as ipv6
        if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
            host.terminate(host.strlen() - 1);
            //printd(5, "QoreSocket::connect(%s, %s) [ipv6]\n", host.c_str() + 1, service.c_str());
            rc = connectINET2SSL(xsink, host.c_str() + 1, service.c_str(), AF_INET6, SOCK_STREAM, 0, timeout_ms,
                cert, pkey);
        } else {
            rc = connectINET2SSL(xsink, host.c_str(), service.c_str(), AF_UNSPEC, SOCK_STREAM, 0, timeout_ms, cert,
                pkey);
        }
    } else {
        // else assume it's a file name for a UNIX domain socket
        rc = connectUNIXSSL(xsink, name, SOCK_STREAM, 0, cert, pkey);
    }

    return rc;
}

int QoreSocket::connectSSL(ExceptionSink* xsink, const char* name, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
   return connectSSL(xsink, name, -1, cert, pkey);
}

int QoreSocket::connectINETSSL(ExceptionSink* xsink, const char* host, int prt, int timeout_ms,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) {
    QoreString service;
    service.sprintf("%d", prt);

    int rc = priv->connectINET(host, service.c_str(), timeout_ms, xsink);
    if (rc) {
        return rc;
    }
    return priv->upgradeClientToSSLIntern(xsink, "connectINETSSL", host, timeout_ms, cert, pkey);
}

int QoreSocket::connectINETSSL(ExceptionSink* xsink, const char* host, int prt, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
   return connectINETSSL(xsink, host, prt, -1, cert, pkey);
}

int QoreSocket::connectINET2SSL(ExceptionSink* xsink, const char* name, const char* service, int family,
        int sock_type, int protocol, int timeout_ms, QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) {
    int rc = connectINET2(name, service, family, sock_type, protocol, timeout_ms, xsink);
    if (rc) {
        return rc;
    }
    return priv->upgradeClientToSSLIntern(xsink, "connectINET2SSL", name, timeout_ms, cert, pkey);
}

int QoreSocket::connectUNIXSSL(ExceptionSink* xsink, const char* p, int sock_type, int protocol,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) {
    int rc = connectUNIX(p, sock_type, protocol, xsink);
    if (rc) {
        return rc;
    }
    return priv->upgradeClientToSSLIntern(xsink, "connectUNIXSSL", nullptr, -1, cert, pkey);
}

int QoreSocket::sendi1(char i) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        return -1;
    }

    ExceptionSink xsink;
    int rc = priv->send(&xsink, "Socket", "sendi1", &i, 1);

    if (rc < 0) {
        xsink.clear();
        return -1;
    }

    return 0;
}

int QoreSocket::sendi2(short i) {
    if (priv->sock == QORE_INVALID_SOCKET)
        return -1;

    // convert to network byte order
    i = htons(i);
    ExceptionSink xsink;
    int rc = priv->send(&xsink, "Socket", "sendi2", (char*)&i, 2);
    if (rc) {
        xsink.clear();
    }
    return rc;
}

int QoreSocket::sendi4(int i) {
    if (priv->sock == QORE_INVALID_SOCKET)
        return -1;

    // convert to network byte order
    i = htonl(i);
    ExceptionSink xsink;
    int rc = priv->send(&xsink, "Socket", "sendi4", (char*)&i, 4);
    if (rc) {
        xsink.clear();
    }
    return rc;
}

int QoreSocket::sendi8(int64 i) {
    if (priv->sock == QORE_INVALID_SOCKET)
        return -1;

    // convert to network byte order
    i = i8MSB(i);
    ExceptionSink xsink;
    int rc = priv->send(&xsink, "Socket", "sendi8", (char*)&i, 8);
    if (rc) {
        xsink.clear();
    }
    return rc;
}

int QoreSocket::sendi2LSB(short i) {
    if (priv->sock == QORE_INVALID_SOCKET)
        return -1;

    // convert to LSB byte order
    i = i2LSB(i);
    ExceptionSink xsink;
    int rc = priv->send(&xsink, "Socket", "sendi2LSB", (char*)&i, 2);
    if (rc) {
        xsink.clear();
    }
    return rc;
}

int QoreSocket::sendi4LSB(int i) {
    if (priv->sock == QORE_INVALID_SOCKET)
        return -1;

    // convert to LSB byte order
    i = i4LSB(i);
    ExceptionSink xsink;
    int rc = priv->send(&xsink, "Socket", "sendi4LSB", (char*)&i, 4);
    if (rc) {
        xsink.clear();
    }
    return rc;
}

int QoreSocket::sendi8LSB(int64 i) {
    if (priv->sock == QORE_INVALID_SOCKET)
        return -1;

    // convert to LSB byte order
    i = i8LSB(i);
    ExceptionSink xsink;
    int rc = priv->send(&xsink, "Socket", "sendi8LSB", (char*)&i, 8);
    if (rc) {
        xsink.clear();
    }
    return rc;
}

int QoreSocket::sendi1(char i, int timeout_ms, ExceptionSink* xsink) {
    return priv->send(xsink, "Socket", "sendi1", &i, 1, timeout_ms);
}

int QoreSocket::sendi2(short i, int timeout_ms, ExceptionSink* xsink) {
    // convert to network byte order
    i = htons(i);
    return priv->send(xsink, "Socket", "sendi2", (char*)&i, 2, timeout_ms);
}

int QoreSocket::sendi4(int i, int timeout_ms, ExceptionSink* xsink) {
    // convert to network byte order
    i = htonl(i);
    return priv->send(xsink, "Socket", "sendi4", (char*)&i, 4, timeout_ms);
}

int QoreSocket::sendi8(int64 i, int timeout_ms, ExceptionSink* xsink) {
    // convert to network byte order
    i = i8MSB(i);
    return priv->send(xsink, "Socket", "sendi8", (char*)&i, 8, timeout_ms);
}

int QoreSocket::sendi2LSB(short i, int timeout_ms, ExceptionSink* xsink) {
    // convert to LSB byte order
    i = i2LSB(i);
    return priv->send(xsink, "Socket", "sendi2LSB", (char*)&i, 2, timeout_ms);
}

int QoreSocket::sendi4LSB(int i, int timeout_ms, ExceptionSink* xsink) {
    // convert to LSB byte order
    i = i4LSB(i);
    return priv->send(xsink, "Socket", "sendi4LSB", (char*)&i, 4, timeout_ms);
}

int QoreSocket::sendi8LSB(int64 i, int timeout_ms, ExceptionSink* xsink) {
    // convert to LSB byte order
    i = i8LSB(i);
    return priv->send(xsink, "Socket", "sendi8LSB", (char*)&i, 8, timeout_ms);
}

// receive integer values and convert from network byte order
int QoreSocket::recvi1(int timeout, char* val) {
    ExceptionSink xsink;
    int rc = priv->recvix("recvi1", 1, val, timeout, &xsink);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

// DLLLOCAL int recvix(const char* meth, int len, void* targ, int timeout_ms, ExceptionSink* xsink) {

int QoreSocket::recvi2(int timeout, short *val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvi2", 2, val, timeout, &xsink);
   *val = ntohs(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi4(int timeout, int* val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvi4", 4, val, timeout, &xsink);
   *val = ntohl(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi8(int timeout, int64 *val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvi8", 8, val, timeout, &xsink);
   *val = MSBi8(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi2LSB(int timeout, short *val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvi2LSB", 2, val, timeout, &xsink);
   *val = LSBi2(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi4LSB(int timeout, int* val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvi4LSB", 4, val, timeout, &xsink);
   *val = LSBi4(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvi8LSB(int timeout, int64 *val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvi8LSB", 8, val, timeout, &xsink);
   *val = LSBi8(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu1(int timeout, unsigned char* val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvu1", 1, val, timeout, &xsink);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu2(int timeout, unsigned short *val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvu2", 2, val, timeout, &xsink);
   *val = ntohs(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu4(int timeout, unsigned int* val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvu4", 4, val, timeout, &xsink);
   *val = ntohl(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu2LSB(int timeout, unsigned short *val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvu2LSB", 2, val, timeout, &xsink);
   *val = LSBi2(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int QoreSocket::recvu4LSB(int timeout, unsigned int* val) {
   ExceptionSink xsink;
   int rc = priv->recvix("recvu4LSB", 4, val, timeout, &xsink);
   *val = LSBi4(*val);
   // ignore exception; we just use a return code
   if (xsink)
      xsink.clear();
   return rc;
}

int64 QoreSocket::recvi1(int timeout, char* val, ExceptionSink* xsink) {
   return priv->recvix("recvi1", 1, val, timeout, xsink);
}

int64 QoreSocket::recvi2(int timeout, short *val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvi2", 2, val, timeout, xsink);
   *val = ntohs(*val);
   return rc;
}

int64 QoreSocket::recvi4(int timeout, int* val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvi4", 4, val, timeout, xsink);
   *val = ntohl(*val);
   return rc;
}

int64 QoreSocket::recvi8(int timeout, int64 *val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvi8", 8, val, timeout, xsink);
   *val = MSBi8(*val);
   return rc;
}

int64 QoreSocket::recvi2LSB(int timeout, short *val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvi2LSB", 2, val, timeout, xsink);
   *val = LSBi2(*val);
   return rc;
}

int64 QoreSocket::recvi4LSB(int timeout, int* val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvi4LSB", 4, val, timeout, xsink);
   *val = LSBi4(*val);
   return rc;
}

int64 QoreSocket::recvi8LSB(int timeout, int64 *val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvi8LSB", 8, val, timeout, xsink);
   *val = LSBi8(*val);
   return rc;
}

int64 QoreSocket::recvu1(int timeout, unsigned char* val, ExceptionSink* xsink) {
   return priv->recvix("recvu1", 1, val, timeout, xsink);
}

int64 QoreSocket::recvu2(int timeout, unsigned short *val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvu2", 2, val, timeout, xsink);
   *val = ntohs(*val);
   return rc;
}

int64 QoreSocket::recvu4(int timeout, unsigned int* val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvu4", 4, val, timeout, xsink);
   *val = ntohl(*val);
   return rc;
}

int64 QoreSocket::recvu2LSB(int timeout, unsigned short *val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvu2LSB", 2, val, timeout, xsink);
   *val = LSBi2(*val);
   return rc;
}

int64 QoreSocket::recvu4LSB(int timeout, unsigned int* val, ExceptionSink* xsink) {
   int rc = priv->recvix("recvu4LSB", 4, val, timeout, xsink);
   *val = LSBi4(*val);
   return rc;
}

int QoreSocket::send(int fd, qore_offset_t size) {
    if (priv->sock == QORE_INVALID_SOCKET || !size) {
        printd(5, "QoreSocket::send() ERROR: sock: %d size: " QSD "\n", priv->sock, size);
        return -1;
    }

    char* buf = (char*)malloc(sizeof(char) * DEFAULT_SOCKET_BUFSIZE);
    ON_BLOCK_EXIT(free, buf);

    ExceptionSink xsink;

    qore_offset_t rc = 0;
    size_t bs = 0;
    while (true) {
        // calculate bytes needed
        size_t bn;
        if (size < 0) {
            bn = DEFAULT_SOCKET_BUFSIZE;
        } else {
            bn = size - bs;
            if (bn > DEFAULT_SOCKET_BUFSIZE)
                bn = DEFAULT_SOCKET_BUFSIZE;
        }
        rc = read(fd, buf, bn);
        if (!rc)
            break;
        if (rc < 0) {
            printd(5, "QoreSocket::send() read error: %s\n", strerror(errno));
            break;
        }

        // send buffer
        int src = priv->send(&xsink, "Socket", "send", buf, rc);
        if (src < 0) {
            printd(5, "QoreSocket::send() send error: %s\n", strerror(errno));
            break;
        }
        bs += rc;
        if (size > 0 && bs >= (size_t)size) {
            rc = 0;
            break;
        }
    }

    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();

    return rc;
}

int QoreSocket::send(int fd, qore_offset_t size, int timeout_ms, ExceptionSink* xsink) {
    return priv->send(fd, size, timeout_ms, xsink);
}

BinaryNode* QoreSocket::recvBinary(qore_offset_t bufsize, int timeout, int* rc) {
    assert(rc);
    ExceptionSink xsink;
    qore_offset_t nrc;
    BinaryNode* b = priv->recvBinary(&xsink, bufsize, timeout, nrc);
    *rc = (int)nrc;
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return b;
}

BinaryNode* QoreSocket::recvBinary(int timeout, int* rc) {
    assert(rc);
    ExceptionSink xsink;
    qore_offset_t nrc;
    BinaryNode* b = priv->recvBinaryAll(&xsink, timeout, nrc);
    *rc = (int)nrc;
    // ignore exception; we just use a return code
    if (xsink) {
        xsink.clear();
    }
    return b;
}

BinaryNode* QoreSocket::recvBinary(qore_offset_t bufsize, int timeout, ExceptionSink* xsink) {
    assert(xsink);
    qore_offset_t rc;
    BinaryNodeHolder b(priv->recvBinary(xsink, bufsize, timeout, rc));
    return *xsink ? 0 : b.release();
}

BinaryNode* QoreSocket::recvBinary(int timeout, ExceptionSink* xsink) {
    assert(xsink);
    qore_offset_t rc;
    BinaryNodeHolder b(priv->recvBinaryAll(xsink, timeout, rc));
    return *xsink ? 0 : b.release();
}

QoreStringNode* QoreSocket::recv(qore_offset_t bufsize, int timeout, int* rc) {
    assert(rc);
    qore_offset_t nrc;
    ExceptionSink xsink;
    QoreStringNode* str = priv->recv(&xsink, bufsize, timeout, nrc);
    // ignore exceptions; we use only a return code
    if (xsink)
        xsink.clear();
    *rc = (int)nrc;
    return str;
}

QoreStringNode* QoreSocket::recv(int timeout, int* rc) {
    assert(rc);
    qore_offset_t nrc;
    ExceptionSink xsink;
    QoreStringNode* str = priv->recvAll(&xsink, timeout, nrc);
    // ignore exceptions; we use only a return code
    if (xsink)
        xsink.clear();
    *rc = (int)nrc;
    return str;
}

QoreStringNode* QoreSocket::recv(qore_offset_t bufsize, int timeout, ExceptionSink* xsink) {
    assert(xsink);
    qore_offset_t rc;
    QoreStringNodeHolder str(priv->recv(xsink, bufsize, timeout, rc));
    return *xsink ? 0 : str.release();
}

QoreStringNode* QoreSocket::recv(int timeout, ExceptionSink* xsink) {
    assert(xsink);
    qore_offset_t rc;
    QoreStringNodeHolder str(priv->recvAll(xsink, timeout, rc));
    return *xsink ? 0 : str.release();
}

// receive data and write to file descriptor
int QoreSocket::recv(int fd, qore_offset_t size, int timeout_ms, ExceptionSink* xsink) {
    return priv->recv(fd, size, timeout_ms, xsink);
}

// receive data and write to file descriptor
int QoreSocket::recv(int fd, qore_offset_t size, int timeout) {
    if (priv->sock == QORE_INVALID_SOCKET || !size)
        return -1;

    ExceptionSink xsink;

    char* buf;
    qore_offset_t br = 0;
    qore_offset_t rc;
    while (true) {
        // calculate bytes needed
        int bn;
        if (size == -1) {
            bn = DEFAULT_SOCKET_BUFSIZE;
        } else {
            bn = size - br;
            if (bn > DEFAULT_SOCKET_BUFSIZE)
                bn = DEFAULT_SOCKET_BUFSIZE;
        }

        rc = priv->brecv(&xsink, "recv", buf, bn, 0, timeout);
        if (rc <= 0)
            break;
        br += rc;

        // write buffer to file descriptor
        rc = write(fd, buf, rc);
        if (rc <= 0)
            break;

        if (size > 0 && br >= size) {
            rc = 0;
            break;
        }
    }

    // ignore exceptions; we use only a return code
    if (xsink)
        xsink.clear();

    return (int)rc;
}

// returns 0 for success
int QoreSocket::sendHTTPMessage(const char* method, const char* path, const char* http_version,
        const QoreHashNode* headers, const void *data, size_t size, int source) {
    return priv->sendHttpMessage(0, 0, "Socket", "sendHTTPMessage", method, path, http_version, headers, nullptr,
        data, size, nullptr, nullptr, 0, nullptr, source);
}

// returns 0 for success
int QoreSocket::sendHTTPMessage(QoreHashNode* info, const char* method, const char* path, const char* http_version,
        const QoreHashNode* headers, const void *data, size_t size, int source) {
    return priv->sendHttpMessage(0, info, "Socket", "sendHTTPMessage", method, path, http_version, headers, nullptr,
        data, size, nullptr, nullptr, 0, nullptr, source);
}

int QoreSocket::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const void *data, size_t size, int source) {
    return priv->sendHttpMessage(xsink, info, "Socket", "sendHTTPMessage", method, path, http_version, headers,
        nullptr, data, size, nullptr, nullptr, 0, nullptr, source);
}

int QoreSocket::sendHTTPMessage(ExceptionSink* xsink, QoreHashNode* info, const char* method, const char* path,
        const char* http_version, const QoreHashNode* headers, const void *data, size_t size, int source,
        int timeout_ms) {
    return priv->sendHttpMessage(xsink, info, "Socket", "sendHTTPMessage", method, path, http_version, headers,
        nullptr, data, size, nullptr, nullptr, 0, nullptr, source, timeout_ms);
}

int QoreSocket::sendHTTPMessageWithCallback(ExceptionSink* xsink, QoreHashNode *info, const char* method,
        const char *path, const char *http_version, const QoreHashNode *headers,
        const ResolvedCallReferenceNode& send_callback, int source, int timeout_ms) {
    return priv->sendHttpMessage(xsink, info, "Socket", "sendHTTPMessageWithCallback", method, path, http_version,
        headers, nullptr, nullptr, 0, &send_callback, nullptr, 0, nullptr, source, timeout_ms);
}

// returns 0 for success
int QoreSocket::sendHTTPResponse(int code, const char* desc, const char* http_version, const QoreHashNode* headers,
    const void *data, size_t size, int source) {
    return priv->sendHttpResponse(nullptr, nullptr, "Socket", "sendHTTPResponse", code, desc, http_version, headers,
        nullptr, data, size, nullptr, nullptr, 0, nullptr, source);
}

int QoreSocket::sendHTTPResponse(ExceptionSink* xsink, int code, const char* desc, const char* http_version,
    const QoreHashNode* headers, const void *data, size_t size, int source) {
    return priv->sendHttpResponse(xsink, nullptr, "Socket", "sendHTTPResponse", code, desc, http_version, headers,
        nullptr, data, size, nullptr, nullptr, 0, nullptr, source);
}

int QoreSocket::sendHTTPResponse(ExceptionSink* xsink, int code, const char* desc, const char* http_version,
    const QoreHashNode* headers, const void *data, size_t size, int source, int timeout_ms) {
    return priv->sendHttpResponse(xsink, nullptr, "Socket", "sendHTTPResponse", code, desc, http_version, headers,
        nullptr, data, size, nullptr, nullptr, 0, nullptr, source, timeout_ms);
}

int QoreSocket::sendHTTPResponse(ExceptionSink* xsink, QoreHashNode* info, int code, const char* desc,
    const char* http_version, const QoreHashNode* headers, const void *data, size_t size, int source,
    int timeout_ms) {
    return priv->sendHttpResponse(xsink, info, "Socket", "sendHTTPResponse", code, desc, http_version, headers,
        nullptr, data, size, nullptr, nullptr, 0, nullptr, source, timeout_ms);
}

AbstractQoreNode* QoreSocket::readHTTPHeader(int timeout, int* rc, int source) {
    assert(rc);
    ExceptionSink xsink;
    qore_offset_t nrc;
    AbstractQoreNode* n = priv->readHTTPHeader(&xsink, 0, timeout, nrc, source);
    *rc = (int)nrc;
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return n;
}

// rc is:
//    0 for remote end shutdown
//   -1 for socket error
//   -2 for socket not open
//   -3 for timeout
AbstractQoreNode* QoreSocket::readHTTPHeader(QoreHashNode* info, int timeout, int* rc, int source) {
    assert(rc);
    ExceptionSink xsink;
    qore_offset_t nrc;
    AbstractQoreNode* n = priv->readHTTPHeader(&xsink, info, timeout, nrc, source);
    *rc = (int)nrc;
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return n;
}

QoreHashNode* QoreSocket::readHTTPHeader(ExceptionSink* xsink, QoreHashNode* info, int timeout, int source) {
    assert(xsink);
    qore_offset_t rc;
    // qore_socket_private::readHTTPHeader() always returns a QoreHashNode* (or 0) if an ExceptionSink argument is passed
    return static_cast<QoreHashNode*>(priv->readHTTPHeader(xsink, info, timeout, rc, source));
}

QoreStringNode* QoreSocket::readHTTPHeaderString(ExceptionSink* xsink, int timeout, int source) {
    assert(xsink);
    return priv->readHTTPHeaderString(xsink, timeout, source);
}

// receive a binary message in HTTP chunked format
QoreHashNode* QoreSocket::readHTTPChunkedBodyBinary(int timeout, ExceptionSink* xsink, int source) {
    return priv->readHttpChunkedBodyBinary(timeout, xsink, "Socket", source);
}

// receive a message in HTTP chunked format
QoreHashNode* QoreSocket::readHTTPChunkedBody(int timeout, ExceptionSink* xsink, int source) {
    return priv->readHttpChunkedBody(timeout, xsink, "Socket", source);
}

bool QoreSocket::isDataAvailable(int timeout) const {
    ExceptionSink xsink;
    int rc = priv->isDataAvailable(timeout, "isDataAvailable", &xsink);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

bool QoreSocket::isWriteFinished(int timeout) const {
    ExceptionSink xsink;
    int rc = priv->isWriteFinished(timeout, "isWriteFinished", &xsink);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

bool QoreSocket::isDataAvailable(ExceptionSink* xsink, int timeout) const {
    return priv->isDataAvailable(timeout, "isDataAvailable", xsink);
}

bool QoreSocket::isWriteFinished(ExceptionSink* xsink, int timeout) const {
    return priv->isWriteFinished(timeout, "isWriteFinished", xsink);
}

int QoreSocket::asyncIoWait(int timeout_ms, bool read, bool write) const {
    ExceptionSink xsink;
    int rc = priv->asyncIoWait(timeout_ms, read, write, &xsink);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

int QoreSocket::upgradeClientToSSL(ExceptionSink* xsink, int timeout_ms, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "upgradeClientToSSL", xsink);
        return -1;
    }
    if (priv->ssl) {
        return 0;
    }
    return priv->upgradeClientToSSLIntern(xsink, "upgradeClientToSSL", nullptr, timeout_ms, cert, pkey);
}

int QoreSocket::upgradeServerToSSL(ExceptionSink* xsink, int timeout_ms, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "upgradeServerToSSL", xsink);
        return -1;
    }
    if (priv->ssl) {
        return 0;
    }
    return priv->upgradeServerToSSLIntern(xsink, "upgradeServerToSSL", timeout_ms, cert, pkey);
}

/* currently hardcoded to SOCK_STREAM (tcp-only)
   if there is no port specifier, opens UNIX domain socket (if necessary)
   and binds to a local UNIX socket file
   for UNIX domain sockets: AF_UNIX
   - bind("filename");
   for ipv4 (unless an ipv6 address is detected in the host part): AF_INET
   - bind("interface:port");
   for ipv6 sockets: AF_INET6
   - bind("[interface]:port");
*/
int QoreSocket::bind(const char* name, bool reuseaddr) {
    ExceptionSink xsink;
    //printd(5, "QoreSocket::bind(%s)\n", name);
    // see if there is a port specifier
    const char* p = strrchr(name, ':');
    int rc;
    if (p) {
        QoreString host(name, p - name);
        QoreString service(p + 1);

        // if the address is an ipv6 address like: [<addr>], then bind as ipv6
        if (host.strlen() > 2 && host[0] == '[' && host[host.strlen() - 1] == ']') {
            host.terminate(host.strlen() - 1);
            rc = priv->bindINET(&xsink, host.c_str() + 1, service.c_str(), reuseaddr, AF_INET6, SOCK_STREAM);
        }

        // assume an ipv6 address if there is a ':' character in the hostname, otherwise bind ipv4
        rc = priv->bindINET(&xsink, host.c_str(), service.c_str(), reuseaddr, strchr(host.c_str(), ':')
            ? AF_INET6
            : AF_INET, SOCK_STREAM);
    } else
        rc = priv->bindUNIX(&xsink, name, SOCK_STREAM);

    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

int QoreSocket::bindUNIX(const char* name, int socktype, int protocol, ExceptionSink* xsink) {
   return priv->bindUNIX(xsink, name, socktype, protocol);
}

int QoreSocket::bindINET(const char* name, const char* service, bool reuseaddr, int family, int socktype,
        int protocol, ExceptionSink* xsink) {
   return priv->bindINET(xsink, name, service, reuseaddr, family, socktype, protocol);
}

// currently hardcoded to SOCK_STREAM (tcp-only)
// opens INET socket and binds to a tcp port on all interfaces
// closes socket if already open, because the socket will be
// bound to all interfaces
// * bind(port);
int QoreSocket::bind(int prt, bool reuseaddr) {
    ExceptionSink xsink;
    priv->close();
    QoreString service;
    service.sprintf("%d", prt);
    int rc = priv->bindINET(&xsink, 0, service.c_str(), reuseaddr);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

// to bind to an INET tcp port on a specific interface
int QoreSocket::bind(const char* iface, int prt, bool reuseaddr) {
    ExceptionSink xsink;
    printd(5, "QoreSocket::bind(%s, %d)\n", iface, prt);
    QoreString service;
    service.sprintf("%d", prt);
    int rc = priv->bindINET(&xsink, iface, service.c_str(), reuseaddr);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

// to bind an INET socket to a particular address
int QoreSocket::bind(const struct sockaddr *addr, int size) {
    // close if it's already been opened as an INET socket or with different parameters
    if (priv->sock != QORE_INVALID_SOCKET && (priv->sfamily != AF_INET || priv->stype != SOCK_STREAM
        || priv->sprot != 0))
        close();

    // try to open socket if necessary
    if (priv->sock == QORE_INVALID_SOCKET && priv->openINET())
        return -1;

    if ((::bind(priv->sock, addr, size)) == QORE_SOCKET_ERROR) {
#ifdef _Q_WINDOWS
        // set errno from windows error
        sock_get_error();
#endif
        return -1;
    }

    // set port number to unknown
    priv->port = -1;
    //printd(5, "QoreSocket::bind(interface, port) returning 0 (success)\n");
    return 0;
}

int QoreSocket::bind(int family, const struct sockaddr *addr, int size, int sock_type, int protocol) {
    family = q_get_af(family);
    sock_type = q_get_sock_type(sock_type);

    // close if it's already been opened as an INET socket or with different parameters
    if (priv->sock != QORE_INVALID_SOCKET && (priv->sfamily != family || priv->stype != sock_type
        || priv->sprot != protocol))
        close();

    // try to open socket if necessary
    if (priv->sock == QORE_INVALID_SOCKET && priv->openINET(family, sock_type, protocol))
        return -1;

    if ((::bind(priv->sock, addr, size)) == -1) {
#ifdef _Q_WINDOWS
        // set errno from windows error
        sock_get_error();
#endif
        return -1;
    }

    // set port number
    int prt = q_get_port_from_addr(addr);
    priv->port = prt ? prt : -1;
    //printd(5, "QoreSocket::bind(interface, port) returning 0 (success)\n");
    return 0;
}

// find out what port we're connected to
int QoreSocket::getPort() {
    return priv->getPort();
}

// QoreSocket::accept()
// returns a new socket
QoreSocket* QoreSocket::accept(SocketSource* source, ExceptionSink* xsink) {
    int rc = priv->accept_internal(xsink, source, -1);
    if (rc < 0)
        return 0;

    QoreSocket* s = new QoreSocket(rc, priv->sfamily, priv->stype, priv->sprot, priv->enc);
    if (!priv->socketname.empty())
        s->priv->socketname = priv->socketname;

    // set SSL params on new socket in case SSL negotiation will be made in the background
    s->priv->setSslVerifyMode(priv->ssl_verify_mode);
    s->priv->acceptAllCertificates(priv->ssl_accept_all_certs);
    if (priv->ssl_capture_remote_cert) {
        s->priv->ssl_capture_remote_cert = true;
    }

    return s;
}

// QoreSocket::acceptSSL()
// accepts a new connection, negotiates an SSL connection, and returns the new socket
QoreSocket* QoreSocket::acceptSSL(ExceptionSink* xsink, SocketSource* source, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    QoreSocket* s = accept(source, xsink);
    if (!s)
        return nullptr;

    s->priv->setSslVerifyMode(priv->ssl_verify_mode);
    s->priv->acceptAllCertificates(priv->ssl_accept_all_certs);
    if (priv->ssl_capture_remote_cert) {
        s->priv->ssl_capture_remote_cert = true;
    }
    if (s->priv->upgradeServerToSSLIntern(xsink, "acceptSSL", -1, cert, pkey)) {
        assert(*xsink);
        delete s;
        return nullptr;
    }

    return s;
}

// accept a connection and replace the socket with the new connection
int QoreSocket::acceptAndReplace(SocketSource* source) {
    QORE_TRACE("QoreSocket::acceptAndReplace()");
    ExceptionSink xsink;
    int rc = priv->accept_internal(&xsink, source);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    if (rc < 0)
        return -1;

    priv->close_internal();
    priv->sock = rc;
    return 0;
}

QoreSocket* QoreSocket::accept(int timeout_ms, ExceptionSink* xsink) {
    int rc = priv->accept_internal(xsink, 0, timeout_ms);
    if (rc < 0)
        return nullptr;
    QoreSocket* s = new QoreSocket(rc, priv->sfamily, priv->stype, priv->sprot, priv->enc);
    if (!priv->socketname.empty())
        s->priv->socketname = priv->socketname;

    // set SSL params on new socket in case SSL negotiation will be made in the background
    s->priv->setSslVerifyMode(priv->ssl_verify_mode);
    s->priv->acceptAllCertificates(priv->ssl_accept_all_certs);
    if (priv->ssl_capture_remote_cert) {
        s->priv->ssl_capture_remote_cert = true;
    }

    return s;
}

QoreSocket* QoreSocket::acceptSSL(ExceptionSink* xsink, int timeout_ms, QoreSSLCertificate* cert,
        QoreSSLPrivateKey* pkey) {
    std::unique_ptr<QoreSocket> s(accept(timeout_ms, xsink));
    if (!s.get())
        return nullptr;

    s->priv->setSslVerifyMode(priv->ssl_verify_mode);
    s->priv->acceptAllCertificates(priv->ssl_accept_all_certs);
    if (priv->ssl_capture_remote_cert) {
        s->priv->ssl_capture_remote_cert = true;
    }
    if (s->priv->upgradeServerToSSLIntern(xsink, "acceptSSL", timeout_ms, cert, pkey)) {
        assert(*xsink);
        return nullptr;
    }

    return s.release();
}

int QoreSocket::acceptAndReplace(int timeout_ms, ExceptionSink* xsink) {
    int rc = priv->accept_internal(xsink, 0, timeout_ms);
    if (rc < 0)
        return -1;

    priv->close_internal();
    priv->sock = rc;
    return 0;
}

int QoreSocket::listen(int backlog) {
    return priv->listen(backlog);
}

int QoreSocket::listen() {
    return priv->listen();
}

int QoreSocket::send(const char* buf, size_t size) {
    ExceptionSink xsink;
    int rc = priv->send(&xsink, "Socket", "send", buf, size);
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

int QoreSocket::send(const char* buf, size_t size, ExceptionSink* xsink) {
    return priv->send(xsink, "Socket", "send", buf, size);
}

int QoreSocket::send(const char* buf, size_t size, int timeout_ms, ExceptionSink* xsink) {
    return priv->send(xsink, "Socket", "send", buf, size, timeout_ms);
}

// converts to socket encoding if necessary
int QoreSocket::send(const QoreString* msg, ExceptionSink* xsink) {
    TempEncodingHelper tstr(msg, priv->enc, xsink);
    if (!tstr)
        return -1;

    int rc = priv->send(xsink, "Socket", "send", (const char*)tstr->c_str(), tstr->strlen(), -1, -1);
    if (!rc) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, tstr->c_str(), tstr->size());
    }
    return rc;
}

// converts to socket encoding if necessary
int QoreSocket::send(const QoreString* msg, int timeout_ms, ExceptionSink* xsink) {
    TempEncodingHelper tstr(msg, priv->enc, xsink);
    if (!tstr)
        return -1;

    int rc = priv->send(xsink, "Socket", "send", (const char*)tstr->c_str(), tstr->strlen(), timeout_ms, -1);
    if (!rc) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, tstr->c_str(), tstr->size());
    }
    return rc;
}

// converts to socket encoding if necessary
int QoreSocket::send(const QoreStringNode& msg, int timeout_ms, ExceptionSink* xsink) {
    QoreStringNodeValueHelper tstr(&msg, priv->enc, xsink);
    if (*xsink)
        return -1;

    int rc = priv->send(xsink, "Socket", "send", (const char*)tstr->c_str(), tstr->strlen(), timeout_ms, -1);
    if (!rc) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, **tstr);
    }
    return rc;
}

int QoreSocket::send(const BinaryNode* b) {
    ExceptionSink xsink;
    int rc = priv->send(&xsink, "Socket", "send", (char*)b->getPtr(), b->size());
    if (!rc) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, *b);
    }
    // ignore exception; we just use a return code
    if (xsink)
        xsink.clear();
    return rc;
}

int QoreSocket::send(const BinaryNode* b, ExceptionSink* xsink) {
    int rc = priv->send(xsink, "Socket", "send", (char*)b->getPtr(), b->size());
    if (!rc) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, *b);
    }
    return rc;
}

int QoreSocket::send(const BinaryNode* b, int timeout_ms, ExceptionSink* xsink) {
    int rc = priv->send(xsink, "Socket", "send", (char*)b->getPtr(), b->size(), timeout_ms);
    if (!rc) {
        priv->do_data_event(QORE_EVENT_SOCKET_DATA_SENT, QORE_SOURCE_SOCKET, *b);
    }
    return rc;
}

int QoreSocket::setSendTimeout(int ms) {
    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    return setsockopt(priv->sock, SOL_SOCKET, SO_SNDTIMEO, (SETSOCKOPT_ARG_4)&tv, sizeof(struct timeval));
}

int QoreSocket::setRecvTimeout(int ms) {
    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    return setsockopt(priv->sock, SOL_SOCKET, SO_RCVTIMEO, (SETSOCKOPT_ARG_4)&tv, sizeof(struct timeval));
}

int QoreSocket::getSendTimeout() const {
    return priv->getSendTimeout();
}

int QoreSocket::getRecvTimeout() const {
    return priv->getRecvTimeout();
}

void QoreSocket::setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    priv->setEventQueue(xsink, q, arg, with_data);
}

Queue* QoreSocket::getQueue() {
    return priv->event_queue;
}

void QoreSocket::cleanup(ExceptionSink* xsink) {
    priv->cleanup(xsink);
}

int64 QoreSocket::getObjectIDForEvents() const {
    return priv->getObjectIDForEvents();
}

QoreHashNode* QoreSocket::getPeerInfo(ExceptionSink* xsink) const {
    return priv->getPeerInfo(xsink);
}

QoreHashNode* QoreSocket::getSocketInfo(ExceptionSink* xsink) const {
    return priv->getSocketInfo(xsink);
}

QoreHashNode* QoreSocket::getPeerInfo(ExceptionSink* xsink, bool host_lookup) const {
    return priv->getPeerInfo(xsink, host_lookup);
}

QoreHashNode* QoreSocket::getSocketInfo(ExceptionSink* xsink, bool host_lookup) const {
    return priv->getSocketInfo(xsink, host_lookup);
}

void QoreSocket::setAccept(QoreObject *o) {
    priv->setAccept(o);
}

void QoreSocket::clearWarningQueue(ExceptionSink* xsink) {
    priv->clearWarningQueue(xsink);
}

void QoreSocket::setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq, QoreValue arg, int64 min_ms) {
    priv->setWarningQueue(xsink, warning_ms, warning_bs, wq, arg, min_ms);
}

QoreHashNode* QoreSocket::getUsageInfo() const {
    return priv->getUsageInfo();
}

void QoreSocket::clearStats() {
    priv->clearStats();
}

bool QoreSocket::pendingHttpChunkedBody() const {
    return priv->pendingHttpChunkedBody();
}

void QoreSocket::setSslVerifyMode(int mode) {
    priv->setSslVerifyMode(mode);
}

int QoreSocket::getSslVerifyMode() const {
    return priv->ssl_verify_mode;
}

void QoreSocket::acceptAllCertificates(bool accept_all) {
    priv->acceptAllCertificates(accept_all);
}

bool QoreSocket::getAcceptAllCertificates() const {
    return priv->ssl_accept_all_certs;
}

bool QoreSocket::captureRemoteCertificates(bool set) {
    bool rv = priv->ssl_capture_remote_cert;
    if (rv != set) {
        priv->ssl_capture_remote_cert = set;
    }
    //printd(5, "QoreSocket::captureRemoteCertificates() priv: %p set: %d rv: %d\n", priv, set, rv);
    return rv;
}

QoreObject* QoreSocket::getRemoteCertificate() const {
    if (priv->remote_cert) {
        priv->remote_cert->ref();
        return priv->remote_cert;
    }
    return nullptr;
}

int64 QoreSocket::getConnectionId() const {
    return priv->connection_id;
}

QoreSocketTimeoutHelper::QoreSocketTimeoutHelper(QoreSocket& s, const char* op)
        : priv(new PrivateQoreSocketTimeoutHelper(qore_socket_private::get(s), op)) {
}

QoreSocketTimeoutHelper::~QoreSocketTimeoutHelper() {
    delete priv;
}

QoreSocketThroughputHelper::QoreSocketThroughputHelper(QoreSocket& s, bool snd)
        : priv(new PrivateQoreSocketThroughputHelper(qore_socket_private::get(s), snd)) {
}

QoreSocketThroughputHelper::~QoreSocketThroughputHelper() {
    delete priv;
}

void QoreSocketThroughputHelper::finalize(int64 bytes) {
    priv->finalize(bytes);
}

SocketConnectPollOperation::SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* target,
        QoreSocketObject* sock) : sock(sock) {
    sgoal = ssl ? SPG_CONNECT_SSL : SPG_CONNECT;

    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer valid
    if (sock->priv->checkValid(xsink)) {
        return;
    }

    if (preVerify(xsink)) {
        return;
    }
    if (!sock->priv->setNonBlock(xsink)) {
        set_non_block = true;
        poll_state.reset(sock->priv->socket->startConnect(xsink, target));
        if (!*xsink) {
            if (poll_state) {
                state = SPS_CONNECTING;
            } else {
                if (sgoal == SPG_CONNECT) {
                    sock->priv->clearNonBlock();
                    set_non_block = false;
                    connected();
                } else {
                    assert(sgoal == SPG_CONNECT_SSL);
                    startSslConnect(xsink);
                }
            }
        }
        if (*xsink) {
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
    }
}

QoreHashNode* SocketConnectPollOperation::continuePoll(ExceptionSink* xsink) {
    QoreHashNode* rv = nullptr;

    AutoLocker al(sock->priv->m);

    if (state == SPS_CONNECTED) {
        // throw an exception and exit if the object is no longer valid
        if (sock->priv->checkValid(xsink)) {
            return nullptr;
        }
    } else {
        // throw an exception and exit if the object is no longer open or valid
        if (sock->priv->checkOpen(xsink)) {
            return nullptr;
        }
    }

    switch (state) {
        case SPS_CONNECTING: {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }

            // if we are just connecting, we are done
            if (sgoal == SPG_CONNECT) {
                // SPS_CONNECTED set below
                break;
            }

            assert(sgoal == SPG_CONNECT_SSL);

            if (startSslConnect(xsink)) {
                break;
            }
        }
        // fall down to next case

        case SPS_CONNECTING_SSL: {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }

            // SPS_CONNECTED set below
            break;
        }

        case SPS_CONNECTED: {
            break;
        }

        default:
            assert(false);
    }

    if (!rv) {
        if (*xsink) {
            state = SPS_NONE;
        } else {
            connected();
        }
        sock->priv->clearNonBlock();
    } else {
        assert(!*xsink);
    }
    return rv;
}

void SocketConnectPollOperation::connected() {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    state = SPS_CONNECTED;
}

int SocketConnectPollOperation::startSslConnect(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());

    state = SPS_CONNECTING_SSL;

    poll_state.reset(sock->priv->socket->startSslConnect(xsink, sock->priv->cert, sock->priv->pk));
    if (*xsink) {
        poll_state.reset();
        state = SPS_NONE;
        return -1;
    }
    return 0;
}

int SocketConnectPollOperation::checkContinuePoll(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    assert(poll_state.get());

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketConnectPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
    //    (int)*xsink);
    if (*xsink) {
        assert(rc < 0);
        state = SPS_NONE;
        return -1;
    }
    if (!rc) {
        // release the AbstractPollState value
        poll_state.reset();
    }
    return rc;
}

SocketUpgradeClientSslPollOperation::SocketUpgradeClientSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : sock(sock) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open and valid or if a TLS/SSL connection has already
    // been established
    if (sock->priv->checkOpenAndNotSsl(xsink)) {
        return;
    }

    if (!sock->priv->setNonBlock(xsink)) {
        set_non_block = true;

        poll_state.reset(sock->priv->socket->startSslConnect(xsink, sock->priv->cert, sock->priv->pk));
        if (*xsink) {
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
    }
}

QoreHashNode* SocketUpgradeClientSslPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    if (done) {
        return nullptr;
    }

    // throw an exception and exit if the object is no longer open and valid
    if (sock->priv->checkOpen(xsink)) {
        return nullptr;
    }

    assert(poll_state);

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketUpgradeClientSslPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(),
    //    rc, (int)*xsink);
    if (*xsink) {
        assert(rc < 0);
        return nullptr;
    }
    if (!rc) {
        // release the AbstractPollState value
        poll_state.reset();
        sock->priv->clearNonBlock();
        set_non_block = false;
        done = true;
        return nullptr;
    }

    return getSocketPollInfoHash(xsink, rc);
}

SocketSendPollOperation::SocketSendPollOperation(ExceptionSink* xsink, QoreStringNode* data, QoreSocketObject* sock)
        : data(data), sock(sock), buf(data->c_str()), size(data->size()) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    assert(data->getEncoding() == sock->getEncoding());
    if (!sock->priv->setNonBlock(xsink)) {
        poll_state.reset(sock->priv->socket->startSend(xsink, buf, size));
        if (!poll_state) {
            sock->priv->clearNonBlock();
        } else {
            set_non_block = true;
        }
    }
}

SocketSendPollOperation::SocketSendPollOperation(ExceptionSink* xsink, BinaryNode* data, QoreSocketObject* sock)
        : data(data), sock(sock), buf(reinterpret_cast<const char*>(data->getPtr())),
        size(data->size()) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    if (!sock->priv->setNonBlock(xsink)) {
        poll_state.reset(sock->priv->socket->startSend(xsink, buf, size));
        if (!poll_state) {
            sock->priv->clearNonBlock();
        } else {
            set_non_block = true;
        }
    }
}

QoreHashNode* SocketSendPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return nullptr;
    }

    if (!poll_state) {
        return nullptr;
    }

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketConnectPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
    //    (int)*xsink);
    if (*xsink || !rc) {
        // release the AbstractPollState value
        poll_state.reset();
        sock->priv->clearNonBlock();
        set_non_block = false;
        if (!*xsink) {
            sent = true;
        }
        return nullptr;
    }
    return getSocketPollInfoHash(xsink, rc);
}

QoreHashNode* SocketRecvPollOperationBase::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return nullptr;
    }

    if (!poll_state) {
        return nullptr;
    }

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketRecvPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
    //    (int)*xsink);
    if (!rc) {
        // get output data
        SimpleRefHolder<BinaryNode> d(poll_state->takeOutput().get<BinaryNode>());
        if (to_string) {
            size_t len = d->size();
            data = new QoreStringNode(reinterpret_cast<char*>(d->giveBuffer()), len, len + 1,
                sock->getEncoding());
        } else {
            data = d.release();
        }
        received = true;
    }
    if (*xsink || !rc) {
        // release the AbstractPollState value
        poll_state.reset();
        sock->priv->clearNonBlock();
        set_non_block = false;
        return nullptr;
    }
    return getSocketPollInfoHash(xsink, rc);
}

int SocketRecvPollOperationBase::initIntern(ExceptionSink* xsink) {
    assert(sock->priv->m.trylock());

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return -1;
    }

    if (sock->priv->setNonBlock(xsink)) {
        return -1;
    }

    set_non_block = true;
    return 0;
}

SocketRecvDataPollOperation::SocketRecvDataPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, bool to_string)
        : SocketRecvPollOperationBase(sock, to_string) {
    AutoLocker al(sock->priv->m);

    if (initIntern(xsink)) {
        return;
    }

    poll_state.reset(sock->priv->socket->startRecvPacket(xsink));
    if (*xsink) {
        sock->priv->clearNonBlock();
        set_non_block = false;
    }
}

SocketRecvPollOperation::SocketRecvPollOperation(ExceptionSink* xsink, ssize_t size, QoreSocketObject* sock, bool to_string)
        : SocketRecvPollOperationBase(sock, to_string), size(size) {
    AutoLocker al(sock->priv->m);

    if (initIntern(xsink)) {
        return;
    }

    poll_state.reset(sock->priv->socket->startRecv(xsink, size));
    if (*xsink) {
        sock->priv->clearNonBlock();
        set_non_block = false;
    }
}

SocketRecvUntilBytesPollOperation::SocketRecvUntilBytesPollOperation(ExceptionSink* xsink, const QoreStringNode* pattern,
        QoreSocketObject* sock, bool to_string) : SocketRecvPollOperationBase(sock, to_string),
        pattern(pattern->stringRefSelf()) {
    AutoLocker al(sock->priv->m);

    if (initIntern(xsink)) {
        return;
    }

    poll_state.reset(sock->priv->socket->startRecvUntilBytes(xsink, pattern->c_str(), pattern->size()));
    if (*xsink) {
        sock->priv->clearNonBlock();
        set_non_block = false;
    }
}

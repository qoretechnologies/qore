/* -*- indent-tabs-mode: nil -*- */
/*
    QoreSocket.cpp

    Socket Class for IPv4, IPv6 and UNIX domain sockets with SSL support

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

// FIXME: change int to size_t where applicable! (ex: int rc = recv())

#include <cctype>
#include <map>
#include <qore/Qore.h>
#include <qore/QoreSocket.h>
#include <qore/QoreSocketObject.h>
#include <qore/QoreSSLCertificate.h>
#include <qore/Transform.h>

#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/qore_string_private.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/CompressionTransforms.h"

// maximum number of non-blocking network operations before returning
constexpr unsigned max_nonblock_ops = 32;

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
            if (svc && strcmp(svc, "-1")) {
                desc->sprintf(":%s", svc);
            }
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
            if (svc && strcmp(svc, "-1")) {
                desc->sprintf(":%s", svc);
            }
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
        int port = q_get_port_from_addr(addr);
        str.sprintf(" (%s: %s", type, host.c_str());
        if (port != -1) {
            str.sprintf(":%d", port);
        }
        str.concat(')');
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

int SSLSocketHelper::setAlpnProtocols(const std::vector<std::string>& protocols) {
    if (!ssl) {
        return -1;
    }

    // Build wire format: length-prefixed strings
    alpn_wire_format.clear();
    for (const auto& proto : protocols) {
        if (proto.empty() || proto.size() > 255) {
            continue;  // Skip invalid protocol names
        }
        alpn_wire_format.push_back(static_cast<unsigned char>(proto.size()));
        alpn_wire_format.insert(alpn_wire_format.end(), proto.begin(), proto.end());
    }

    if (alpn_wire_format.empty()) {
        return -1;
    }

    // Set ALPN protocols for client
    ERR_clear_error();
    if (SSL_set_alpn_protos(ssl, alpn_wire_format.data(), alpn_wire_format.size()) != 0) {
        return -1;
    }
    return 0;
}

void SSLSocketHelper::setServerAlpnProtocols(const std::vector<std::string>& protocols) {
    server_alpn_protocols = protocols;

    if (ctx && !protocols.empty()) {
        // Set the ALPN selection callback for server
        SSL_CTX_set_alpn_select_cb(ctx, alpnSelectCallback, this);
    }
}

int SSLSocketHelper::alpnSelectCallback(SSL* ssl, const unsigned char** out, unsigned char* outlen,
        const unsigned char* in, unsigned int inlen, void* arg) {
    SSLSocketHelper* helper = static_cast<SSLSocketHelper*>(arg);
    if (!helper || helper->server_alpn_protocols.empty()) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    // Parse client protocols and select first match
    const unsigned char* p = in;
    const unsigned char* end = in + inlen;

    while (p < end) {
        unsigned char proto_len = *p++;
        if (p + proto_len > end) {
            break;
        }

        std::string client_proto(reinterpret_cast<const char*>(p), proto_len);
        for (const auto& supported : helper->server_alpn_protocols) {
            if (client_proto == supported) {
                *out = p;
                *outlen = proto_len;
                return SSL_TLSEXT_ERR_OK;
            }
        }
        p += proto_len;
    }

    return SSL_TLSEXT_ERR_NOACK;
}

std::string SSLSocketHelper::getAlpnProtocol() const {
    if (!ssl) {
        return "";
    }

    const unsigned char* proto = nullptr;
    unsigned int len = 0;
    SSL_get0_alpn_selected(ssl, &proto, &len);
    if (proto && len > 0) {
        return std::string(reinterpret_cast<const char*>(proto), len);
    }
    return "";
}

bool SSLSocketHelper::isHttp2() const {
    return getAlpnProtocol() == "h2";
}

int SSLSocketHelper::pending() const {
    return ssl ? SSL_pending(ssl) : 0;
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
            case SSL_ERROR_ZERO_RETURN:
                // remote closed the connection
                break;
            default:
                printd(0, "SSLSocketHelper::startConnect() SSL_get_error() reports error %d\n", err);
                break;
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
            default:
                printd(3, "SSLSocketHelper::startAccept() err: %d\n", err);
                break;
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

    // Check sandbox network security restrictions
    QoreSandboxManagerHelper smh;
    if (smh) {
        int proto = (p->ai_socktype == SOCK_STREAM) ? QSEC_NET_TCP :
                    (p->ai_socktype == SOCK_DGRAM) ? QSEC_NET_UDP : QSEC_NET_ALL;
        if (!smh->checkNetworkAccess(p->ai_addr, p->ai_addrlen, proto, xsink)) {
            return -1;
        }
    }

    sock->do_connect_event(p->ai_family, p->ai_addr, host.c_str(), service.c_str(), prt);
    // make sure and close the socket if it is already open
    sock->close_internal();
    assert(sock->sock == QORE_INVALID_SOCKET);
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
    sock->close_internal();
    assert(sock->sock == QORE_INVALID_SOCKET);

    addr.sun_family = AF_UNIX;
    // copy path and terminate if necessary
    strncpy(addr.sun_path, name, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';

    // Check sandbox network security restrictions for UNIX sockets
    QoreSandboxManagerHelper smh;
    if (smh) {
        if (!smh->checkNetworkAccess((const struct sockaddr*)&addr, sizeof(struct sockaddr_un),
                QSEC_NET_UNIX, xsink)) {
            return;
        }
    }

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
        return;
    }

    // Set ALPN protocols if configured (for HTTP/2 support)
    // This must match the synchronous path in upgradeClientToSSLIntern()
    if (!sock->alpn_protocols.empty()) {
        sock->ssl->setAlpnProtocols(sock->alpn_protocols);
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
            if (real_io) {
                sent += real_io;
                if (sent == size) {
                    break;
                }
                if (rc) {
                    return rc;
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

SocketAcceptPollState::SocketAcceptPollState(ExceptionSink* xsink, qore_socket_private* sock) : sock(sock) {
}

/** returns:
- SOCK_POLLIN = wait for read and call this again
- SOCK_POLLOUT = wait for write and call this again
- 0 = done
- < 1 = error (exception raised)
*/
int SocketAcceptPollState::continuePoll(ExceptionSink* xsink) {
    // try an accept with no timeout
    int rc = sock->accept_internal(xsink, nullptr, 0);
    if (*xsink) {
        return -1;
    }
    if (rc < 0) {
        return SOCK_POLLIN;
    }
    descriptor = rc;
    return 0;
}

SocketAcceptSslPollState::SocketAcceptSslPollState(ExceptionSink* xsink, qore_socket_private* sock,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pkey) : sock(sock) {
    assert(sock->sock);
    assert(!sock->ssl);
    SSLSocketHelperHelper sshh(sock, true);

    sock->do_start_ssl_event();
    int rc;
    if ((rc = sock->ssl->setServer(xsink, "acceptSSL", sock->sock, cert, pkey))) {
        sshh.error();
        assert(*xsink);
        return;
    }

    // Set ALPN protocols if configured (for HTTP/2 support)
    if (!sock->alpn_protocols.empty()) {
        sock->ssl->setServerAlpnProtocols(sock->alpn_protocols);
    }
}

int SocketAcceptSslPollState::continuePoll(ExceptionSink* xsink) {
    return sock->ssl->startAccept(xsink);
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
            if (!rc && real_io) {
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
                const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()) + realsize),
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
            if (!rc && real_io) {
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
            if (rc == SOCK_POLLIN && !real_io) {
                if (++loop >= max_nonblock_ops) {
                    return SOCK_POLLIN;
                }
                struct pollfd pfd;
                pfd.fd = sock->sock;
                pfd.events = POLLIN | POLLERR | POLLHUP;
                pfd.revents = 0;
                int prc = ::poll(&pfd, 1, 0);
                if (prc <= 0 || !(pfd.revents & (POLLIN | POLLERR | POLLHUP))) {
                    return SOCK_POLLIN;
                }
                if (pfd.revents & (POLLERR | POLLHUP)) {
                    return 0;
                }
                continue;
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

SocketRecvFromPollState::SocketRecvFromPollState(ExceptionSink* xsink, qore_socket_private* sock, size_t max_size)
        : sock(sock), max_size(max_size) {
    if (sock->sock == QORE_INVALID_SOCKET) {
        xsink->raiseException("SOCKET-NOT-OPEN", "socket is not open");
        return;
    }
    if (sock->stype != SOCK_DGRAM) {
        xsink->raiseException("SOCKET-RECVFROM-ERROR", "recvfrom() is only valid for UDP (SOCK_DGRAM) sockets");
        return;
    }
    bin = new BinaryNode();
    if (bin->preallocate(max_size)) {
        xsink->outOfMemory();
        return;
    }
    memset(&src_addr, 0, sizeof(src_addr));
}

int SocketRecvFromPollState::continuePoll(ExceptionSink* xsink) {
    if (io) {
        return 0;
    }

    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    while (true) {
        src_addr_len = sizeof(src_addr);
        ssize_t rc = ::recvfrom(sock->sock,
#ifdef _Q_WINDOWS
            const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr())),
#else
            reinterpret_cast<void*>(const_cast<char*>(reinterpret_cast<const char*>(bin->getPtr()))),
#endif
            max_size, 0, (struct sockaddr*)&src_addr, &src_addr_len);

        if (rc >= 0) {
            bin->setSize(rc);
            received = rc;
            io = true;
            return 0;
        }

        sock_get_error();
        if (errno == EINTR) {
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
        xsink->raiseErrnoException("SOCKET-RECVFROM-ERROR", errno, "error while executing recvfrom()");
        return -1;
    }
    return 0;
}

QoreValue SocketRecvFromPollState::takeOutput() {
    if (!io) {
        return QoreValue();
    }

    // Build result hash with data and source address info
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);

    // Set the binary data
    h->setKeyValue("data", bin.release(), nullptr);

    // Extract source address info
    if (src_addr.ss_family == AF_INET || src_addr.ss_family == AF_INET6) {
        char ifname[INET6_ADDRSTRLEN];
        if (inet_ntop(src_addr.ss_family, qore_get_in_addr((struct sockaddr*)&src_addr),
                ifname, sizeof(ifname))) {
            h->setKeyValue("address", new QoreStringNode(ifname), nullptr);
        }

        int port;
        if (src_addr.ss_family == AF_INET) {
            struct sockaddr_in* s = (struct sockaddr_in*)&src_addr;
            port = ntohs(s->sin_port);
        } else {
            struct sockaddr_in6* s = (struct sockaddr_in6*)&src_addr;
            port = ntohs(s->sin6_port);
        }
        h->setKeyValue("port", port, nullptr);
    }

    h->setKeyValue("family", (int64)src_addr.ss_family, nullptr);
    h->setKeyValue("familystr", new QoreStringNode(QoreAddrInfo::getFamilyName(src_addr.ss_family)), nullptr);

    return h.release();
}

SocketSendToPollState::SocketSendToPollState(ExceptionSink* xsink, qore_socket_private* sock, const char* data,
        size_t size, const struct sockaddr* dest_addr, socklen_t dest_addr_len)
        : sock(sock), data(data), size(size), dest_addr_len(dest_addr_len) {
    if (sock->sock == QORE_INVALID_SOCKET) {
        xsink->raiseException("SOCKET-NOT-OPEN", "socket is not open");
        return;
    }
    if (sock->stype != SOCK_DGRAM) {
        xsink->raiseException("SOCKET-SENDTO-ERROR", "sendto() is only valid for UDP (SOCK_DGRAM) sockets");
        return;
    }
    memcpy(&this->dest_addr, dest_addr, dest_addr_len);
}

int SocketSendToPollState::continuePoll(ExceptionSink* xsink) {
    OptionalNonBlockingHelper nbh(*sock, true, xsink);
    if (*xsink) {
        return -1;
    }

    unsigned loop = 0;

    while (true) {
        ssize_t rc = ::sendto(sock->sock, data + sent, size - sent, 0,
            (const struct sockaddr*)&dest_addr, dest_addr_len);

        if (rc >= 0) {
            sent += rc;
            if (sent == size) {
                return 0;
            }
            // For UDP, partial sends are unusual but handle them
            if (++loop >= max_nonblock_ops) {
                return SOCK_POLLOUT;
            }
            continue;
        }

        sock_get_error();
        if (errno == EINTR) {
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
        xsink->raiseErrnoException("SOCKET-SENDTO-ERROR", errno, "error while executing sendto()");
        return -1;
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
#if !defined(HAVE_POLL) && !defined(DARWIN)
    xsink->raiseException("MISSING-FEATURE-ERROR", "no support for async I/O polling on this platform");
    return nullptr;
#else
    ReferenceHolder<QoreListNode> rv(new QoreListNode(hashdeclSocketPollInfo->getTypeInfo(false)), xsink);

    if (poll_list->empty()) {
        return rv.release();
    }

    PrivateDataListHolder<QoreSocketObject> pdlh(xsink);

    // Structure to track fd -> poll_list index mapping and requested events
    struct FdInfo {
        int fd;
        int64 events;  // SOCK_POLLIN, SOCK_POLLOUT
        size_t index;  // index in poll_list
    };
    std::vector<FdInfo> fd_info;

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

        if (!(events & (SOCK_POLLIN | SOCK_POLLOUT))) {
            xsink->raiseException("SOCKET-POLL-ERROR", "element %zu/%zu (starting from 1) has an invalid " \
                "'events' value; neither SOCK_POLLIN nor SOCK_POLLOUT is set", li.index() + 1, poll_list->size());
            return nullptr;
        }

        fd_info.push_back({fd, events, li.index()});
    }

#ifdef DARWIN
    // Use kqueue on macOS - it properly handles listener sockets unlike poll()
    int kq = kqueue();
    if (kq == -1) {
        qore_socket_error(xsink, "SOCKET-POLL-ERROR", "kqueue() failed");
        return nullptr;
    }

    // RAII cleanup for kqueue fd
    struct KqueueGuard {
        int kq;
        KqueueGuard(int k) : kq(k) {}
        ~KqueueGuard() { if (kq != -1) ::close(kq); }
    } guard(kq);

    // Build kevent change list - need up to 2 events per fd (read + write)
    std::vector<struct kevent> changes;
    changes.reserve(fd_info.size() * 2);

    for (const auto& info : fd_info) {
        if (info.events & SOCK_POLLIN) {
            struct kevent ev;
            EV_SET(&ev, info.fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0,
                reinterpret_cast<void*>(info.index));
            changes.push_back(ev);
        }
        if (info.events & SOCK_POLLOUT) {
            struct kevent ev;
            EV_SET(&ev, info.fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0,
                reinterpret_cast<void*>(info.index));
            changes.push_back(ev);
        }
    }

    // Convert timeout to timespec (like nginx: NULL for infinite timeout)
    struct timespec ts;
    struct timespec* pts = nullptr;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        pts = &ts;
    }

    // Allocate space for returned events
    std::vector<struct kevent> events(changes.size());

    int rc;
    // First do a zero-timeout kevent to register events and check for already-ready fds
    // This handles the case where events occurred before we registered the filters
    struct timespec ts_zero = {0, 0};

    while (true) {
        rc = kevent(kq, changes.data(), changes.size(), events.data(), events.size(), &ts_zero);
        if (rc >= 0 || errno != EINTR) {
            break;
        }
    }

    if (rc < 0) {
        qore_socket_error(xsink, "SOCKET-POLL-ERROR", "kevent() returned an error");
    } else if (rc == 0 && (timeout_ms < 0 || timeout_ms > 0)) {
        // No events ready yet, wait with actual timeout (events already registered)
        while (true) {
            rc = kevent(kq, nullptr, 0, events.data(), events.size(), pts);
            if (rc >= 0 || errno != EINTR) {
                break;
            }
        }
        if (rc < 0) {
            qore_socket_error(xsink, "SOCKET-POLL-ERROR", "kevent() returned an error");
        }
    }

    // Track which poll_list indices have events and what events they have
    // Use std::map to preserve order (sorted by index) for consistent output order
    std::map<size_t, int> result_events;

    // Process events if we have any
    if (rc > 0) {
        for (int i = 0; i < rc; ++i) {
            size_t idx = reinterpret_cast<size_t>(events[i].udata);
            if (events[i].flags & EV_ERROR) {
                result_events[idx] = SOCK_POLLERR;
                continue;
            }

            int evt = result_events[idx];
            if (events[i].filter == EVFILT_READ) {
                if (events[i].flags & EV_EOF) {
                    // EOF on read - connection closed by peer
                    evt |= SOCK_POLLERR;
                } else {
                    evt |= SOCK_POLLIN;
                }
            } else if (events[i].filter == EVFILT_WRITE) {
                if (events[i].flags & EV_EOF) {
                    // EOF on write - error condition
                    evt |= SOCK_POLLERR;
                } else {
                    evt |= SOCK_POLLOUT;
                }
            }
            result_events[idx] = evt;
        }
    }

    // Check for fds that were closed during the kqueue wait.
    // On macOS, closing a monitored fd removes its kqueue registration
    // without delivering an event, so we need to detect this case.
    // NOTE: This means callers may receive SOCK_POLLERR results even when
    // kevent() itself returned 0 (timeout).  All current callers handle this:
    // - AsyncSocketIoController treats any socket in ready_list as "ready"
    //   and calls continuePoll(), which handles error states
    // - Http2ClientConnection ignores the poll() return value entirely
    // - Test code checks for result presence, not event types
    if (!*xsink) {
        for (const auto& info : fd_info) {
            if (result_events.count(info.index)) {
                continue;
            }
            if (fcntl(info.fd, F_GETFD) == -1 && errno == EBADF) {
                result_events[info.index] = SOCK_POLLERR;
            }
        }
    }

    // Build result list
    if (!*xsink) {
        for (const auto& [idx, evt] : result_events) {
            if (evt) {
                const QoreHashNode* orig = poll_list->retrieveEntry(idx).get<const QoreHashNode>();
                ReferenceHolder<QoreHashNode> entry(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
                assert(!*xsink);
                entry->setKeyValue("events", evt, xsink);
                entry->setKeyValue("socket", orig->getKeyValue("socket").refSelf(), xsink);
                rv->push(entry.release(), xsink);
                assert(!*xsink);
            }
        }
    }

#else  // HAVE_POLL
    std::vector<pollfd> fds;
    for (const auto& info : fd_info) {
        pollfd pfd;
        pfd.fd = info.fd;
        pfd.events = 0;
        if (info.events & SOCK_POLLIN) {
            pfd.events |= POLLIN;
        }
        if (info.events & SOCK_POLLOUT) {
            pfd.events |= POLLOUT;
        }
        pfd.revents = 0;
        fds.push_back(pfd);
    }

    int rc;
    while (true) {
        rc = ::poll(&fds[0], fds.size(), timeout_ms);
        // poll() returns 0 when there is a timeout
        if (!rc) {
            break;
        }
        // continue if interrupted
        if (rc == -1 && errno == EINTR) {
            continue;
        }
        // throw an exception if there was an error
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
#endif  // DARWIN

    return rv.release();
#endif  // !HAVE_POLL && !DARWIN
}

static void write_sse_key_value(ExceptionSink* xsink, ReferenceHolder<QoreHashNode>& rv, const char* f, QoreValue v,
        SimpleRefHolder<QoreStringNode>& value) {
    assert(value && !value->empty());
    if (!v) {
        rv->setKeyValue(f, value.release(), xsink);
    } else {
        v.get<QoreStringNode>()->concat('\n');
        v.get<QoreStringNode>()->concat(*value, xsink);
        value->clear();
    }
}

static void write_sse_key_value(ExceptionSink* xsink, ReferenceHolder<QoreHashNode>& rv, const char* f,
        SimpleRefHolder<QoreStringNode>& value) {
    write_sse_key_value(xsink, rv, f, rv->getKeyValue(f), value);
}

static void write_sse_key(ExceptionSink* xsink, ReferenceHolder<QoreHashNode>& rv, const char* f,
        SimpleRefHolder<QoreStringNode>& value) {
    QoreValue v = rv->getKeyValue(f);
    if (!value || value->empty()) {
        // if the field does not exist, set it to an empty string
        if (!v) {
            rv->setKeyValue(f, new QoreStringNode(QCS_UTF8), xsink);
        }
    } else {
        write_sse_key_value(xsink, rv, f, v, value);
    }
}

static bool sse_is_digits(const char* str) {
    if (!str[0]) {
        return false;
    }
    for (const char* p = str; *p; ++p) {
        if (!isdigit(*p)) {
            return false;
        }
    }
    return true;
}

QoreHashNode* qore_socket_private::readServerSentEvent(ExceptionSink* xsink, Transform* transform, int timeout_ms) {
    assert(xsink);

    if (sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "readServerSentEvent", xsink);
        return nullptr;
    }
    if (in_op >= 0) {
        if (in_op == q_gettid()) {
            se_in_op("Socket", "readServerSentEvent", xsink);
            return nullptr;
        }
        se_in_op_thread("Socket", "readServerSentEvent", xsink);
        return nullptr;
    }

    qore_socket_op_helper oh(this);

    // event stream data is always UTF-8
    QoreString str(QCS_UTF8);

    int eol_count = 0;

    size_t tbufsize = transform ? transform->outputBufferSize() : 0;
    char* tbuf = tbufsize ? (char*)malloc(tbufsize * sizeof(char)) : nullptr;
    ON_BLOCK_EXIT(free, tbuf);
    size_t tlen = 0;
    size_t tpos = 0;
    // input buffer for transforms
    QoreString cibuf;

    while (true) {
        char* cbuf;
        ssize_t rc;
        rc = brecv(xsink, "readServerSentEvent", cbuf, transform ? DEFAULT_SOCKET_BUFSIZE : 1, 0, timeout_ms, false);
        if (rc <= 0) {
            if (!*xsink) {
                assert(!rc);
                se_closed("Socket", "readServerSentEvent", xsink);
            }
            return 0;
        }

        char c;
        // decompress data
        if (transform) {
            cibuf.concat(cbuf, rc);
            if (tpos < tlen) {
                c = tbuf[tpos++];
            } else {
                tpos = 0;
                std::pair<int64, int64> i = transform->apply(cibuf.c_str(), cibuf.size(), tbuf, tbufsize, xsink);
printd(0, "qore_socket_private::readServerSentEvent() cib: %p (%lld) (%s) tbuf: %p (%lld) read: %lld written: %lld\n", cibuf.c_str(), cibuf.size(), cibuf.c_str(), tbuf, tbufsize, i.first, i.second);
                if (*xsink) {
                    return nullptr;
                }
                if (i.first) {
                    cibuf.removeBytes(i.first);
                }
                if (!i.second) {
                    continue;
                }
                tlen = i.second;
                c = tbuf[tpos++];
            }
        } else {
            c = cbuf[0];
        }

        if (sse_got_cr) {
            sse_got_cr = false;
            if (c == '\n') {
                continue;
            }
        }

        if (c == '\r') {
            str.concat('\n');
            sse_got_cr = true;
            if (++eol_count == 2) {
                break;
            }
        } else if (c == '\n') {
            str.concat('\n');
            if (++eol_count == 2) {
                break;
            }
        } else {
            if (eol_count) {
                eol_count = 0;
            }
            str.concat(c);
        }
    }
    //printd(5, "readServerSentEvent: raw SSE message (" QSD " bytes): %s", str.strlen(), str.c_str());
    return parseServerSentEvent(xsink, str);
}

QoreHashNode* qore_socket_private::parseServerSentEvent(ExceptionSink* xsink, const QoreString& buf) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(hashdeclSseMessageInfo, xsink), xsink);
    assert(!*xsink);
    SimpleRefHolder<QoreStringNode> field;
    SimpleRefHolder<QoreStringNode> value;
    int do_value = 0;
    for (size_t i = 0, e = buf.size() - 1; i < e; ++i) {
        char c = buf[i];
        if (c == '\n') {
            if (field && !field->empty()) {
                if ((**field == "event") || (**field == "data")) {
                    write_sse_key(xsink, rv, field->c_str(), value);
                } else if (**field == "id") {
                    if (value && !value->empty()) {
                        write_sse_key_value(xsink, rv, "id", value);
                    }
                } else if (**field == "retry") {
                    if (value && !value->empty() && sse_is_digits(value->c_str()) && !rv->getKeyValue("retry")) {
                        rv->setKeyValue("retry", strtoll(value->c_str(), 0, 10), xsink);
                    }
                }
                // ignore field data
                field->clear();
            }
            if (value && !value->empty()) {
                write_sse_key_value(xsink, rv, "comment", value);
            }
            if (do_value) {
                do_value = 0;
            }
            continue;
        }
        if (!do_value && (c == ':')) {
            do_value = 1;
            continue;
        }
        if (do_value) {
            if (!value) {
                value = new QoreStringNode(QCS_UTF8);
            }
            // skip the first space after the colon
            if (do_value == 1) {
                do_value = 2;
                if (c == ' ') {
                    continue;
                }
            }
            value->concat(c);
            continue;
        }
        if (!field) {
            field = new QoreStringNode(QCS_UTF8);
        }
        field->concat(c);
    }

    //QoreNodeAsStringHelper str(*rv, FMT_NORMAL, xsink);
    //printd(0, "qore_socket_private::parseServerSentEvent() %s\n", str->c_str());

    return rv.release();
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

        if (rc > 0) {
#ifdef DEBUG
            // Test-only hook to simulate SSL partial writes that require repeated polling.
            static int force_count = 0;
            static int force_max = 0;
            const char* env = getenv("QORE_TEST_SSL_PARTIAL_WRITE");
            if (!env || !*env || *env == '0') {
                force_count = 0;
                force_max = 0;
            } else if (action == WRITE && real_io) {
                if (!force_max) {
                    const char* lim = getenv("QORE_TEST_SSL_PARTIAL_WRITE_LIMIT");
                    force_max = lim ? atoi(lim) : 4;
                    if (force_max < 1) {
                        force_max = 1;
                    }
                }
                if (force_count < force_max) {
                    ++force_count;
                    rc = SOCK_POLLOUT;
                    break;
                }
            }
#endif
            // SSL accepted the data, but check if it still has pending encrypted data to write
            // This happens when SSL_write encrypts data but the socket would block
            if (action == WRITE && SSL_want_write(ssl)) {
                rc = SOCK_POLLOUT;
                break;
            }
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
            // For HTTP/2, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qs.close();
            }
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
            // For HTTP/2, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qs.close();
            }
            // in case there is no exception when reading, the remote end closed the connection
            rc = *xsink ? QSE_SSL_ERR : 0;
            break;
        } else if (err == SSL_ERROR_SSL) {
            // For HTTP/2, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qs.close();
            }
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
            // For HTTP/2, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qs.close();
            }
            break;
        } else if (err == SSL_ERROR_SYSCALL) {
            if (!sslError(xsink, mname, get_action_method(action), action == WRITE)) {
                if (rc == -1) {
                    xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the "
                        "openssl library reported an I/O error while calling %s()", mname, get_action_method(action));
                } else if (rc) {
                    xsink->raiseException("SOCKET-SSL-ERROR", "error in Socket::%s(): the openssl library reported "
                        "error code %d in %s() but the error queue is empty", mname, rc, get_action_method(action));
                }
            }
            // close the socket unconditionally
            // For HTTP/2, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qs.close();
            }
            rc = !*xsink ? 0 : QSE_SSL_ERR;
            break;
        } else if (err == SSL_ERROR_SSL) {
            // close the socket unconditionally
            // For HTTP/2, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qs.close();
            }
            xsink->raiseErrnoException("SOCKET-SSL-ERROR", sock_get_error(), "error in Socket::%s(): the "
                "openssl library reported a fatal I/O error while calling %s()", mname, get_action_method(action));
            rc = QSE_SSL_ERR;
            break;
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
        // the remote end has closed the connection
        // NOTE: For HTTP/2 connections, don't auto-close on SSL_ERROR_ZERO_RETURN.
        // This could be a timeout or the end of a read operation, not necessarily a connection close.
        // Let the HTTP/2 layer handle connection lifecycle.
        if (!qs.h2_session) {
            qs.close();
        }
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
            qore_string_private::get(*errstr)->concat(qs.ssl_err_str);
            qs.ssl_err_str->deref();
            qs.ssl_err_str = nullptr;
        }
        xsink->raiseException("SOCKET-SSL-ERROR", errstr.release());
#ifdef ECONNRESET
        // close the socket if connection reset received
        if (e == SSL_ERROR_SYSCALL && sock_get_error() == ECONNRESET) {
            //printd(5, "SSLSocketHelper::handleErrorIntern() Socket::%s() (%s) socket closed by remote end\n", mname, func);
            // For HTTP/2 connections, let the HTTP/2 layer handle connection lifecycle
            if (!qs.h2_session) {
                qs.close();
            }
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

int QoreSocket::setAlpnProtocols(const QoreListNode* protocols, ExceptionSink* xsink) {
    if (!protocols || !protocols->size()) {
        xsink->raiseException("SOCKET-ALPN-ERROR", "protocol list is empty");
        return -1;
    }

    std::vector<std::string> proto_list;
    ConstListIterator li(protocols);
    while (li.next()) {
        QoreStringValueHelper str(li.getValue());
        if (!str->empty()) {
            proto_list.push_back(str->c_str());
        }
    }

    if (proto_list.empty()) {
        xsink->raiseException("SOCKET-ALPN-ERROR", "no valid protocols in list");
        return -1;
    }

    // Store protocols for use during SSL upgrade
    priv->alpn_protocols = proto_list;
    return 0;
}

void QoreSocket::clearAlpnProtocols() {
    priv->alpn_protocols.clear();
}

QoreStringNode* QoreSocket::getAlpnProtocol() const {
    if (!priv->ssl) {
        return nullptr;
    }
    std::string proto = priv->ssl->getAlpnProtocol();
    if (proto.empty()) {
        return nullptr;
    }
    return new QoreStringNode(proto.c_str());
}

bool QoreSocket::isHttp2() const {
    if (!priv->ssl) {
        return false;
    }
    return priv->ssl->isHttp2();
}

int32_t QoreSocket::submitHttp2PushPromise(int32_t stream_id, const char* path,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    // Convert Qore headers hash to std::map
    std::map<std::string, std::string> h2_headers;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            const char* key = hi.getKey();
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                h2_headers[key] = val.get<const QoreStringNode>()->c_str();
            }
        }
    }

    return priv->h2_session->submitPushPromise(stream_id, path, h2_headers, xsink);
}

int QoreSocket::submitHttp2Response(int32_t stream_id, int status_code,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    // Convert Qore headers hash to std::map
    std::map<std::string, std::string> h2_headers;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            const char* key = hi.getKey();
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                h2_headers[key] = val.get<const QoreStringNode>()->c_str();
            }
        }
    }

    return priv->h2_session->submitResponse(stream_id, status_code, h2_headers, body, body_len, xsink);
}

int QoreSocket::submitHttp2ConnectResponse(int32_t stream_id, int status_code,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    // Convert Qore headers hash to std::map
    std::map<std::string, std::string> h2_headers;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            const char* key = hi.getKey();
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                h2_headers[key] = val.get<const QoreStringNode>()->c_str();
            }
        }
    }

    return priv->h2_session->submitConnectResponse(stream_id, status_code, h2_headers, xsink);
}

int32_t QoreSocket::submitHttp2Request(const QoreHashNode* headers, const void* body,
        size_t body_len, ExceptionSink* xsink, bool streaming) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    if (!headers) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 request headers are required");
        return -1;
    }

    // Convert Qore headers hash to std::map and extract :method, :path
    std::map<std::string, std::string> h2_headers;
    std::string method;
    std::string path;

    ConstHashIterator hi(headers);
    while (hi.next()) {
        const char* key = hi.getKey();
        QoreValue val = hi.get();
        if (val.getType() != NT_STRING) {
            continue;
        }
        const char* sval = val.get<const QoreStringNode>()->c_str();
        std::string skey(key);

        if (skey == ":method") {
            if (sval && *sval) {
                method = sval;
            }
        } else if (skey == ":path") {
            if (sval && *sval) {
                path = sval;
            }
        }
        h2_headers[skey] = sval ? sval : "";
    }

    if (method.empty()) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 request ':method' header is missing or empty");
        return -1;
    }

    if (path.empty()) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 request ':path' header is missing or empty");
        return -1;
    }

    return priv->h2_session->submitRequest(method.c_str(), path.c_str(), h2_headers, body, body_len, xsink, streaming);
}

void QoreSocket::cancelHttp2Stream(int32_t stream_id, ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return;
    }
    priv->h2_session->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
}

void QoreSocket::setHttp2ConnectProtocolEnabled(bool enable) {
    priv->h2_enable_connect_protocol = enable;
}

int QoreSocket::sendHttp2StreamData(int32_t stream_id, const BinaryNode* data,
        bool end_stream, ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    const void* ptr = data ? data->getPtr() : nullptr;
    size_t len = data ? data->size() : 0;

    int rv = priv->h2_session->sendStreamData(stream_id, ptr, len, end_stream, xsink);
    if (rv < 0) {
        return -1;
    }
    if (rv > 0) {
        xsink->raiseException("HTTP2-FLOW-CONTROL",
            "stream %d buffer full: data dropped", stream_id);
        return -1;
    }
    // Data queued; nghttp2_session_resume_data() already called by sendStreamData().
    // I/O thread flushes via continuePoll() -> sendPendingData().
    // WebSocket caller wakes I/O thread via wsc.getAsyncCtrl().wake().
    return 0;
}

int QoreSocket::submitHttp2StreamingResponseHeaders(int32_t stream_id, int status_code,
        const QoreHashNode* headers, ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    // Convert QoreHashNode to map
    std::map<std::string, std::string> header_map;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                header_map[hi.getKey()] = val.get<const QoreStringNode>()->c_str();
            }
        }
    }

    return priv->h2_session->submitResponseStreaming(stream_id, status_code, header_map, xsink);
}

int QoreSocket::sendHttp2Trailers(int32_t stream_id, const QoreHashNode* trailers,
        ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return -1;
    }

    // Convert QoreHashNode to map
    std::map<std::string, std::string> trailer_map;
    if (trailers) {
        ConstHashIterator hi(trailers);
        while (hi.next()) {
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                trailer_map[hi.getKey()] = val.get<const QoreStringNode>()->c_str();
            }
        }
    }

    return priv->h2_session->submitTrailers(stream_id, trailer_map, xsink);
}

BinaryNode* QoreSocket::readHttp2StreamData(int32_t stream_id, size_t max_bytes, ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return nullptr;
    }

    return priv->h2_session->takeStreamData(stream_id, max_bytes, xsink);
}

void QoreSocket::setHttp2ActiveStream(int32_t stream_id, ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return;
    }
    priv->setH2ActiveStreamId(stream_id);
}

int32_t QoreSocket::getHttp2ActiveStream() const {
    return priv->getH2ActiveStreamId();
}

// RAII guard for saving/restoring the HTTP/2 active stream ID
namespace {
struct Http2ActiveStreamGuard {
    qore_socket_private& priv;
    int32_t old_id;
    Http2ActiveStreamGuard(qore_socket_private& p, int32_t new_id)
        : priv(p), old_id(p.getH2ActiveStreamId()) {
        p.setH2ActiveStreamId(new_id);
    }
    ~Http2ActiveStreamGuard() { priv.setH2ActiveStreamId(old_id); }
    Http2ActiveStreamGuard(const Http2ActiveStreamGuard&) = delete;
    Http2ActiveStreamGuard& operator=(const Http2ActiveStreamGuard&) = delete;
};
} // anonymous namespace

// Thread safety: this method must only be called from a single thread per socket.
// The server connection model enforces this — each connection has one handler thread
// that drives the HTTP/2 session exclusively.  Concurrent calls on the same socket
// would cause nghttp2 reentrancy issues (receiveData is not reentrant).
BinaryNode* QoreSocket::readHttp2StreamDataBlock(int32_t stream_id, int timeout_ms,
        ExceptionSink* xsink) {
    if (!priv->h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session active");
        return nullptr;
    }

    // RAII guard saves/restores active stream ID on all exit paths
    Http2ActiveStreamGuard id_guard(*priv, stream_id);

    // Use a deadline to avoid timeout reset when control frames arrive
    int64 deadline_ms = timeout_ms >= 0
        ? q_clock_getmillis() + timeout_ms
        : -1;

    while (true) {
        // 1. Check stream buffer for available data
        BinaryNode* data = priv->h2_session->takeStreamData(stream_id, 0, xsink);
        if (data) {
            return data;
        }
        if (*xsink) {
            return nullptr;
        }

        // 2. Check body_complete (END_STREAM received)
        if (priv->h2_session->isStreamComplete(stream_id)) {
            return nullptr;
        }

        // 3. Calculate remaining timeout from deadline
        int remaining_ms;
        if (deadline_ms >= 0) {
            remaining_ms = (int)(deadline_ms - q_clock_getmillis());
            if (remaining_ms < 0) {
                remaining_ms = 0;
            }
        } else {
            remaining_ms = -1;
        }

        // 4. Wait for raw socket data with timeout
        bool has_data = priv->h2_session->hasSocketBufferedData();
        if (!has_data) {
            has_data = priv->isSocketDataAvailable(remaining_ms, "readHttp2StreamDataBlock", xsink);
            if (*xsink) {
                return nullptr;
            }
            if (!has_data) {
                // Timeout
                return nullptr;
            }
        }

        // 5. Process HTTP/2 frames
        priv->h2_receiving_frames = true;
        int rv = priv->h2_session->receiveData(0, xsink);
        priv->h2_receiving_frames = false;
        if (*xsink || rv == 1) {
            return nullptr;
        }

        // 6. Flush pending protocol frames (WINDOW_UPDATE, SETTINGS_ACK)
        priv->h2_session->sendPendingDataBlocking(100, xsink);
        if (*xsink) {
            return nullptr;
        }
    }
}

bool QoreSocket::isHttp2StreamComplete(int32_t stream_id) const {
    if (!priv->h2_session) {
        return true;
    }
    return priv->h2_session->isStreamComplete(stream_id);
}

int QoreSocket::flushHttp2(int timeout_ms, ExceptionSink* xsink) {
    if (!priv->h2_session) {
        return 0;
    }
    return priv->h2_session->sendPendingDataBlocking(timeout_ms, xsink);
}

void QoreSocket::cleanupHttp2Stream(int32_t stream_id) {
    if (priv->h2_session) {
        priv->h2_session->cleanupStream(stream_id);
    }
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

AbstractPollState* QoreSocket::startRecvFrom(ExceptionSink* xsink, size_t max_size) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startRecvFrom", xsink);
        return nullptr;
    }

    return new SocketRecvFromPollState(xsink, priv, max_size);
}

AbstractPollState* QoreSocket::startSendTo(ExceptionSink* xsink, const char* data, size_t size,
        const struct sockaddr* dest_addr, socklen_t dest_addr_len) {
    if (priv->sock == QORE_INVALID_SOCKET) {
        se_not_open("Socket", "startSendTo", xsink);
        return nullptr;
    }

    return new SocketSendToPollState(xsink, priv, data, size, dest_addr, dest_addr_len);
}

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

QoreHashNode* QoreSocket::readHttpChunk(int timeout, ExceptionSink* xsink) {
    return priv->readHttpChunkedBodyBinary(timeout, xsink, "Socket", QORE_SOURCE_SOCKET, nullptr, nullptr, nullptr,
        nullptr, true);
}

QoreHashNode* QoreSocket::parseServerSentEvent(ExceptionSink* xsink, const QoreString& buf) {
    return qore_socket_private::parseServerSentEvent(xsink, buf);
}

QoreHashNode* QoreSocket::readServerSentEvent(ExceptionSink* xsink, const QoreStringNode* content_encoding,
        int timeout_ms) {
    if (content_encoding && (*content_encoding != "identity")) {
        SimpleRefHolder<Transform> t(CompressionTransforms::getDecompressor(content_encoding, xsink));
        if (*xsink) {
            return nullptr;
        }
        return priv->readServerSentEvent(xsink, *t, timeout_ms);
    }
    return priv->readServerSentEvent(xsink, nullptr, timeout_ms);
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
    // Copy ALPN protocols to the new socket for HTTP/2 support
    if (!priv->alpn_protocols.empty()) {
        s->priv->alpn_protocols = priv->alpn_protocols;
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
    assert(priv->sock == QORE_INVALID_SOCKET);
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
    // Copy ALPN protocols to the new socket for HTTP/2 support
    if (!priv->alpn_protocols.empty()) {
        s->priv->alpn_protocols = priv->alpn_protocols;
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
    // Copy ALPN protocols to the new socket for HTTP/2 support
    if (!priv->alpn_protocols.empty()) {
        s->priv->alpn_protocols = priv->alpn_protocols;
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
    assert(priv->sock == QORE_INVALID_SOCKET);
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
        QoreSocketObject* sock) : SocketPollSocketOperationBase(sock) {
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

        case SPS_NONE: {
            // aborted
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

SocketAcceptPollOperation::SocketAcceptPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketAcceptPollSocketOperationBase(sock), accepted_socket_obj(xsink) {
    sgoal = sock->priv->cert && sock->priv->pk ? SPS_ACCEPTING_SSL : SPS_ACCEPTING;

    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer valid
    if (sock->priv->checkValid(xsink)) {
        return;
    }

    if (preVerify(xsink)) {
        return;
    }
    if (!sock->priv->setNonBlockAccept(xsink)) {
        set_non_block_accept = true;
        poll_state.reset(sock->priv->socket->startAccept(xsink));
        if (!*xsink) {
            assert(poll_state);
            state = SPS_ACCEPTING;
        }
        if (*xsink) {
            sock->priv->clearNonBlockAccept();
            set_non_block_accept = false;
        }
    }
}

QoreHashNode* SocketAcceptPollOperation::continuePoll(ExceptionSink* xsink) {
    QoreHashNode* rv = nullptr;

    AutoLocker al(sock->priv->m);

    if (state == SPS_ACCEPTED) {
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
        case SPS_ACCEPTING: {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }

            // if we are just accepting, we are done
            if (sgoal == SPG_ACCEPT) {
                // SPS_ACCEPTED set below
                break;
            }

            assert(sgoal == SPG_ACCEPT_SSL);

            if (startSslAccept(xsink)) {
                break;
            }
        }
        // fall down to next case

        case SPS_ACCEPTING_SSL: {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }

            // SPS_ACCEPTED set below
            break;
        }

        case SPS_ACCEPTED: {
            break;
        }

        case SPS_NONE: {
            // aborted
            break;
        }

        default:
            assert(false);
    }

    if (!rv) {
        if (*xsink) {
            state = SPS_NONE;
        } else {
            accepted();
        }
        sock->priv->clearNonBlockAccept();
        set_non_block_accept = false;
    } else {
        assert(!*xsink);
    }
    return rv;
}

void SocketAcceptPollOperation::accepted() {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    state = SPS_ACCEPTED;
}

int SocketAcceptPollOperation::startSslAccept(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());

    state = SPS_ACCEPTING_SSL;

    assert(accepted_socket);
    // we have the original socket locked, but do the SSL accept operation on the new socket without any locks,
    // however, since no one else can access it yet, this is safe
    poll_state.reset(accepted_socket->priv->socket->startSslAccept(xsink, accepted_socket->priv->cert,
            accepted_socket->priv->pk));
    if (*xsink) {
        poll_state.reset();
        state = SPS_NONE;
        return -1;
    }
    return 0;
}

int SocketAcceptPollOperation::checkContinuePoll(ExceptionSink* xsink) {
    // socket lock must be held here
    assert(sock->priv->m.trylock());
    assert(poll_state.get());

    // see if we are able to continue
    int rc = poll_state->continuePoll(xsink);
    //printd(5, "SocketAcceptPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(), rc,
    //    (int)*xsink);
    if (*xsink) {
        assert(rc < 0);
        state = SPS_NONE;
        return -1;
    }
    if (!rc) {
        if (state == SPS_ACCEPTING) {
            // save socket info for getOutput() value
            assert(dynamic_cast<SocketAcceptPollState*>(poll_state.get()));
            int descriptor = reinterpret_cast<SocketAcceptPollState*>(poll_state.get())->getDescriptor();
            accepted_socket = new QoreSocketObject(*sock, descriptor);
        }
        // release the AbstractPollState value
        poll_state.reset();
    }
    return rc;
}

QoreValue SocketAcceptPollOperation::getOutput() const {
    AutoLocker al(sock->priv->m);
    // If we have a cached QoreObject wrapper (from SSL handshake polling), return that
    if (accepted_socket_obj) {
        // Release the cached object and clear accepted_socket to avoid double-free
        accepted_socket.discard();
        return accepted_socket_obj.release();
    }
    if (accepted_socket) {
        return new QoreObject(QC_SOCKET, getProgram(), accepted_socket.release());
    }
    return QoreValue();
}

SocketUpgradeClientSslPollOperation::SocketUpgradeClientSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
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

SocketUpgradeServerSslPollOperation::SocketUpgradeServerSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open and valid or if a TLS/SSL connection has already
    // been established
    if (sock->priv->checkOpenAndNotSsl(xsink)) {
        return;
    }

    if (!sock->priv->setNonBlock(xsink)) {
        set_non_block = true;

        poll_state.reset(sock->priv->socket->startSslAccept(xsink, sock->priv->cert, sock->priv->pk));
        if (*xsink) {
            poll_state.reset();
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
    }
}

QoreHashNode* SocketUpgradeServerSslPollOperation::continuePoll(ExceptionSink* xsink) {
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
    //printd(5, "SocketUpgradeServerSslPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(),
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
        : SocketPollSocketOperationBase(sock), data(data), buf(data->c_str()), size(data->size()) {
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
        : SocketPollSocketOperationBase(sock), data(data), buf(reinterpret_cast<const char*>(data->getPtr())),
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

bool SocketSendPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketSendPollState*>(poll_state.get()));
        return reinterpret_cast<SocketSendPollState*>(poll_state.get())->getBytesSent() ? true : false;
    }
    return true;
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
        bool ok = true;
        if (to_string) {
            size_t len = d->size();
            char* buf = reinterpret_cast<char*>(d->giveBuffer());
            char* nbuf = reinterpret_cast<char*>(q_realloc(buf, len + 1));
            if (!nbuf) {
                xsink->outOfMemory();
                ok = false;
            } else {
                nbuf[len] = '\0';
                data = new QoreStringNode(nbuf, len, len + 1, sock->getEncoding());
            }
        } else {
            data = d.release();
        }
        if (ok) {
            received = true;
        }
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

bool SocketRecvDataPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketRecvPacketPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvPacketPollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    return true;
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

bool SocketRecvPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketRecvPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvPollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    return true;
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

bool SocketRecvUntilBytesPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketRecvUntilBytesPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvUntilBytesPollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    return true;
}

SocketReadHttpHeaderPollOperation::SocketReadHttpHeaderPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketRecvPollOperationBase(sock, true), out(xsink) {
    AutoLocker al(sock->priv->m);
    if (initIntern(xsink)) {
        return;
    }

    poll_state.reset(sock->priv->socket->startRecvUntilBytes(xsink, "\r\n\r\n", 4));
    if (*xsink) {
        sock->priv->clearNonBlock();
        set_non_block = false;
    }
}

QoreHashNode* SocketReadHttpHeaderPollOperation::continuePoll(ExceptionSink* xsink) {
    QoreHashNode* rv = SocketRecvPollOperationBase::continuePoll(xsink);
    if (rv || !data) {
        return rv;
    }

    ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
    assert(data->getType() == NT_STRING);
    SimpleRefHolder<QoreStringNode> hdrstr(reinterpret_cast<QoreStringNode*>(data.release()));
    ReferenceHolder<QoreHashNode> hdr(sock->priv->socket->priv->processHttpHeaderString(xsink, hdrstr, *info,
        QORE_SOURCE_SOCKET), xsink);
    if (*xsink) {
        return nullptr;
    }
    // Store result in member variable for getOutput() - don't return it from continuePoll()
    out = new QoreHashNode(autoTypeInfo);
    out->setKeyValue("hdr", hdr.release(), xsink);
    out->setKeyValue("info", info.release(), xsink);
    // Return nullptr to indicate operation is complete; result is available via getOutput()
    return nullptr;
}

QoreValue SocketReadHttpHeaderPollOperation::getOutput() const {
    AutoLocker al(sock->priv->m);
    return out.release();
}

bool SocketReadHttpHeaderPollOperation::abortNeedsClose() const {
    if (poll_state) {
        assert(dynamic_cast<SocketRecvUntilBytesPollState*>(poll_state.get()));
        return reinterpret_cast<SocketRecvUntilBytesPollState*>(poll_state.get())->getBytesReceived() ? true : false;
    }
    return true;
}

// SocketSendAndReadHeaderPollOperation implementation

SocketSendAndReadHeaderPollOperation::SocketSendAndReadHeaderPollOperation(ExceptionSink* xsink,
        BinaryNode* response_data, QoreSocketObject* sock, int64 idle_timeout_ms)
        : SocketPollSocketOperationBase(sock), send_data(response_data),
          buf(reinterpret_cast<const char*>(response_data->getPtr())),
          size(response_data->size()),
          idle_timeout_ms(idle_timeout_ms), header_output(xsink) {
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

const char* SocketSendAndReadHeaderPollOperation::getStateImpl() const {
    switch (phase) {
        case Phase::Sending: return "sending";
        case Phase::Idle: return "idle";
        case Phase::ReadingHeader: return "reading-header";
        case Phase::Complete: return "header-complete";
        case Phase::Timeout: return "timeout";
        case Phase::Error: return "error";
        default: return "unknown";
    }
}

QoreHashNode* SocketSendAndReadHeaderPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);
    if (sock->priv->checkOpen(xsink)) {
        phase = Phase::Error;
        return nullptr;
    }

    switch (phase) {
        case Phase::Sending: {
            if (!poll_state) {
                phase = Phase::Error;
                return nullptr;
            }
            // Drive the send poll state
            int rc = poll_state->continuePoll(xsink);
            if (*xsink) {
                phase = Phase::Error;
                poll_state.reset();
                return nullptr;
            }
            if (rc) {
                // Need more I/O for sending
                return getSocketPollInfoHash(xsink, rc);
            }
            // Send complete — free send data and poll state, transition to idle
            poll_state.reset();
            send_data.discard();
            phase = Phase::Idle;
            // Return POLLIN to wait for incoming data
            return getSocketPollInfoHash(xsink, SOCK_POLLIN);
        }

        case Phase::Idle: {
            // Check idle timeout
            int us;
            int64 now_s = q_epoch_us(us);
            int64 now_ms = now_s * 1000 + us / 1000;
            if (idle_timeout_ms > 0 && now_ms > idle_timeout_ms) {
                phase = Phase::Timeout;
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }
            // Data is available (we were woken by POLLIN) — transition to header reading
            poll_state.reset(sock->priv->socket->startRecvUntilBytes(xsink, "\r\n\r\n", 4));
            if (*xsink || !poll_state) {
                phase = Phase::Error;
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }
            phase = Phase::ReadingHeader;
        }
        // fall through to drive header read immediately

        case Phase::ReadingHeader: {
            if (!poll_state) {
                phase = Phase::Error;
                return nullptr;
            }
            int rc = poll_state->continuePoll(xsink);
            if (*xsink) {
                phase = Phase::Error;
                poll_state.reset();
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }
            if (rc) {
                // Need more I/O for header reading
                return getSocketPollInfoHash(xsink, rc);
            }
            // Header read complete — parse it
            SimpleRefHolder<BinaryNode> raw(poll_state->takeOutput().get<BinaryNode>());
            poll_state.reset();

            // Convert binary to string for HTTP header parsing
            size_t len = raw->size();
            char* buf = reinterpret_cast<char*>(raw->giveBuffer());
            char* nbuf = reinterpret_cast<char*>(q_realloc(buf, len + 1));
            if (!nbuf) {
                free(buf);
                xsink->outOfMemory();
                phase = Phase::Error;
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }
            nbuf[len] = '\0';
            SimpleRefHolder<QoreStringNode> hdrstr(new QoreStringNode(nbuf, len, len + 1, sock->getEncoding()));

            // Parse HTTP headers
            ReferenceHolder<QoreHashNode> info(new QoreHashNode(autoTypeInfo), xsink);
            ReferenceHolder<QoreHashNode> hdr(sock->priv->socket->priv->processHttpHeaderString(xsink, hdrstr,
                *info, QORE_SOURCE_SOCKET), xsink);
            if (*xsink) {
                phase = Phase::Error;
                sock->priv->clearNonBlock();
                set_non_block = false;
                return nullptr;
            }

            // Store result for getOutput()
            header_output = new QoreHashNode(autoTypeInfo);
            header_output->setKeyValue("hdr", hdr.release(), xsink);
            header_output->setKeyValue("info", info.release(), xsink);

            // Clear non-block mode now that we're done
            sock->priv->clearNonBlock();
            set_non_block = false;

            phase = Phase::Complete;
            return nullptr;
        }

        default:
            return nullptr;
    }
}

QoreValue SocketSendAndReadHeaderPollOperation::getOutput() const {
    AutoLocker al(sock->priv->m);
    return header_output.release();
}

#include "qore/intern/Http2Session.h"

SocketHttp2ServerPollOperation::SocketHttp2ServerPollOperation(ExceptionSink* xsink, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
    AutoLocker al(sock->priv->m);

    // Check if socket is open and valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    // Set non-blocking mode
    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Check if there's already an HTTP/2 session stored in the socket (from a previous request)
    bool reused_session = false;
    if (sock->priv->socket->priv->h2_session) {
        // Reuse existing session (socket owns it)
        h2_session = sock->priv->socket->priv->h2_session;
        reused_session = true;
    } else {
        // Initialize new HTTP/2 session and store it on the socket
        if (initSession(xsink)) {
            sock->priv->clearNonBlock();
            set_non_block = false;
            return;
        }
    }

    // Set initial state based on whether we reused a session
    if (reused_session) {
        // Session already established - go directly to reading frames
        h2_state = H2S_READING;
    } else {
        // New session - need to exchange connection preface
        h2_state = H2S_SEND_PREFACE;
    }
}

SocketHttp2ServerPollOperation::~SocketHttp2ServerPollOperation() {
}

int SocketHttp2ServerPollOperation::initSession(ExceptionSink* xsink) {
    // Create server-side HTTP/2 session using the underlying QoreSocket's priv
    h2_session = Http2Session::createServer(sock->priv->socket->priv, xsink);
    if (!h2_session) {
        return -1;
    }
    // Apply socket-level HTTP/2 settings before the connection preface is sent
    h2_session->setEnableConnectProtocol(sock->priv->socket->priv->h2_enable_connect_protocol);
    // Store session on socket for shared access
    sock->priv->socket->priv->h2_session = h2_session;
    return 0;
}

QoreHashNode* SocketHttp2ServerPollOperation::checkHeadersOnlyDispatch(bool& handled,
        ExceptionSink* xsink) {
    handled = false;
    if (!headers_only) {
        return nullptr;
    }
    // Flush pending protocol responses (SETTINGS_ACK, WINDOW_UPDATE) so the
    // client can proceed with sending HEADERS/DATA
    int srv = h2_session->sendPendingData(0, xsink);
    if (*xsink) {
        return nullptr;
    }
    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
        handled = true;
        return getSocketPollInfoHash(xsink, srv);
    }
    // Atomically find a headers-ready stream, copy it, and mark dispatched
    cached_stream = h2_session->takeHeadersReadyStreamCopy();
    if (!cached_stream) {
        // No headers-ready stream yet
        return nullptr;
    }
    handled = true;
    h2_state = H2S_HEADERS_READY;
    if (set_non_block) {
        set_non_block = false;
        sock->priv->clearNonBlock();
    }
    return nullptr;
}

QoreHashNode* SocketHttp2ServerPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    while (true) {
        switch (h2_state) {
            case H2S_SEND_PREFACE: {
                // Send server connection preface (SETTINGS frame)
                int rv = h2_session->sendConnectionPreface(xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == -1) {
                    // Would block - need to poll for write
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }
                h2_state = H2S_RECV_PREFACE;
                // Fall through to receive preface
            }

            case H2S_RECV_PREFACE:
            case H2S_READING: {
                // Headers-only mode: check for streams with HEADERS complete but not yet dispatched
                {
                    bool handled = false;
                    QoreHashNode* rv = checkHeadersOnlyDispatch(handled, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (handled) {
                        return rv;
                    }
                }

                // If another thread already completed a stream, return it immediately
                if (h2_session->hasCompletedStreams()) {
                    // Flush any pending output (RST_STREAM, SETTINGS_ACK, etc.) before returning
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
                        // Socket buffer full; poll and retry before completing the operation
                        return getSocketPollInfoHash(xsink, srv);
                    }
                    // Dequeue the completed stream now so getOutput() is idempotent
                    cached_stream = h2_session->takeCompletedStream();
                    h2_state = H2S_REQUEST_READY;
                    if (set_non_block) {
                        set_non_block = false;
                        sock->priv->clearNonBlock();
                    }
                    return nullptr;
                }
                // Try to receive data
                int rv = h2_session->receiveData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == -1) {
                    // Would block on recv - also try to send pending response data
                    // (responses may have been queued by handler threads via submitResponse)
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
                        return getSocketPollInfoHash(xsink, SOCK_POLLIN | srv);
                    }
                    // Always read; add write if there are pending sends
                    int events = SOCK_POLLIN;
                    if (h2_session->hasPendingData() || h2_session->wantWrite()) {
                        events |= SOCK_POLLOUT;
                    }
                    return getSocketPollInfoHash(xsink, events);
                }
                if (rv == 1) {
                    // Connection closed by peer - check if we have a completed request first
                    if (!h2_session->hasCompletedStreams()) {
                        peer_closed = true;
                        // Do NOT set H2S_REQUEST_READY: there are no completed streams to
                        // return, so goalReached() should return false.  The caller will see
                        // continuePoll() -> nullptr with goalReached() == false and handle
                        // the connection closure.
                        if (set_non_block) {
                            set_non_block = false;
                            sock->priv->clearNonBlock();
                        }
                        return nullptr;
                    }
                    // Fall through to handle the completed request
                }

                // Headers-only mode: check for headers-ready streams after receiving data
                {
                    bool handled = false;
                    QoreHashNode* hrv = checkHeadersOnlyDispatch(handled, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (handled) {
                        return hrv;
                    }
                }

                // Check if we have a completed request
                if (h2_session->hasCompletedStreams()) {
                    // Flush any pending output (RST_STREAM, SETTINGS_ACK, etc.) before returning
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
                        // Socket buffer full; poll and retry before completing the operation
                        return getSocketPollInfoHash(xsink, srv);
                    }
                    // Dequeue the completed stream now so getOutput() is idempotent
                    cached_stream = h2_session->takeCompletedStream();
                    h2_state = H2S_REQUEST_READY;
                    // Goal reached - clear non-block flag so subsequent operations can proceed
                    if (set_non_block) {
                        set_non_block = false;
                        sock->priv->clearNonBlock();
                    }
                    return nullptr;
                }

                // Need to send any pending data (like SETTINGS ACK)
                rv = h2_session->sendPendingData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                // sendPendingData returns:
                //   0: success
                //   SOCK_POLLIN: need to poll for read (TLS renegotiation)
                //   SOCK_POLLOUT: need to poll for write
                //   -1: error (exception set)
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    return getSocketPollInfoHash(xsink, rv);
                }

                // If we were receiving preface, move to reading state
                if (h2_state == H2S_RECV_PREFACE) {
                    h2_state = H2S_READING;
                }

                // Always include POLLIN to continue reading new requests.
                // Add POLLOUT when there are pending response sends.
                {
                    int events = SOCK_POLLIN;
                    if (h2_session->hasPendingData() || h2_session->wantWrite()) {
                        events |= SOCK_POLLOUT;
                    }
                    return getSocketPollInfoHash(xsink, events);
                }
            }

            case H2S_REQUEST_READY:
                // If there are more completed streams, dequeue the next one
                if (h2_session->hasCompletedStreams()) {
                    cached_stream = h2_session->takeCompletedStream();
                    if (set_non_block) {
                        set_non_block = false;
                        sock->priv->clearNonBlock();
                    }
                    return nullptr;
                }
                // All completed streams consumed - transition back to reading
                // so the same operation can be reused for multiplexing
                cached_stream.reset();
                h2_state = H2S_READING;
                if (!set_non_block) {
                    if (sock->priv->setNonBlock(xsink)) {
                        return nullptr;
                    }
                    set_non_block = true;
                }
                continue;

            case H2S_HEADERS_READY:
                // Headers-only dispatch complete - transition back to reading
                // so the operation can be reused for the next request
                cached_stream.reset();
                h2_state = H2S_READING;
                if (!set_non_block) {
                    if (sock->priv->setNonBlock(xsink)) {
                        return nullptr;
                    }
                    set_non_block = true;
                }
                continue;

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected state: %d", h2_state);
                return nullptr;
        }
    }
}

QoreValue SocketHttp2ServerPollOperation::getOutput() const {
    if (h2_state != H2S_REQUEST_READY && h2_state != H2S_HEADERS_READY) {
        return QoreValue();
    }
    if (peer_closed) {
        return QoreValue();
    }
    // cached_stream is dequeued in continuePoll() when transitioning to H2S_REQUEST_READY
    if (!cached_stream) {
        return QoreValue();
    }
    // Skip streams reset at protocol level (e.g., CONNECT without ENABLE_CONNECT_PROTOCOL)
    // The RST_STREAM was already sent by nghttp2 via sendPendingData()
    if (cached_stream->reset) {
        return QoreValue();
    }

    const_cast<SocketHttp2ServerPollOperation*>(this)->stream_id = cached_stream->stream_id;

    // Build output hash from cached stream info (idempotent - safe to call multiple times)
    QoreHashNode* result = new QoreHashNode(autoTypeInfo);
    result->setKeyValue("method", new QoreStringNode(cached_stream->method), nullptr);
    result->setKeyValue("path", new QoreStringNode(cached_stream->path), nullptr);
    result->setKeyValue("stream_id", cached_stream->stream_id, nullptr);

    // Build headers hash (lowercase names, handle duplicate headers per RFC 7540)
    QoreHashNode* headers = h2HeadersToQoreHash(cached_stream->headers, true);
    result->setKeyValue("headers", headers, nullptr);

    // Body as binary
    if (!cached_stream->body.empty()) {
        BinaryNode* body = new BinaryNode();
        body->append(cached_stream->body.data(), cached_stream->body.size());
        result->setKeyValue("body", body, nullptr);
    }

    // Add pseudo-headers if present
    if (!cached_stream->authority.empty()) {
        result->setKeyValue("authority", new QoreStringNode(cached_stream->authority), nullptr);
    }
    if (!cached_stream->scheme.empty()) {
        result->setKeyValue("scheme", new QoreStringNode(cached_stream->scheme), nullptr);
    }
    // RFC 8441: Add :protocol pseudo-header for extended CONNECT
    if (!cached_stream->connect_protocol.empty()) {
        headers->setKeyValue(":protocol", new QoreStringNode(cached_stream->connect_protocol), nullptr);
    }

    // Include trailers from client if present (e.g., gRPC client-streaming)
    if (!cached_stream->trailers.empty()) {
        QoreHashNode* trailers_hash = h2HeadersToQoreHash(cached_stream->trailers, true);
        result->setKeyValue("trailers", trailers_hash, nullptr);
    }

    // Indicate headers-only dispatch (stream still in map for incremental reading)
    if (h2_state == H2S_HEADERS_READY) {
        result->setKeyValue("hdr", true, nullptr);
    }

    return result;
}

Http2SessionPtr SocketHttp2ServerPollOperation::takeSession() {
    return nullptr;
}

SocketHttp2SendResponsePollOperation::SocketHttp2SendResponsePollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock, const Http2SessionPtr& h2_session_param, int32_t stream_id, int status_code,
        const QoreHashNode* headers, const BinaryNode* body, bool is_connect)
        : SocketPollSocketOperationBase(sock), h2_session(h2_session_param), stream_id(stream_id) {

    AutoLocker al(sock->priv->m);

    // Get HTTP/2 session from socket (stored by previous read operation)
    Http2Session* session = sock->priv->socket->priv->h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session available; startPollSendHttp2Response() must be "
            "called after startPollReadHttp2Request() completes on an active HTTP/2 connection");
        return;
    }

    // Check if socket is open and valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    // Set non-blocking mode
    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Build headers as vector of pairs to support duplicate header names (e.g., set-cookie)
    std::vector<std::pair<std::string, std::string>> hdr_pairs;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            const char* key = hi.getKey();
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                hdr_pairs.emplace_back(key, val.get<const QoreStringNode>()->c_str());
            } else if (val.getType() == NT_LIST) {
                // Emit separate header entries for each list value
                const QoreListNode* l = val.get<const QoreListNode>();
                for (size_t i = 0; i < l->size(); ++i) {
                    QoreValue lv = l->retrieveEntry(i);
                    if (lv.getType() == NT_STRING) {
                        hdr_pairs.emplace_back(key, lv.get<const QoreStringNode>()->c_str());
                    }
                }
            }
        }
    }

    int rv;
    if (is_connect) {
        // RFC 8441: CONNECT response (WebSocket over HTTP/2) - no body, no END_STREAM
        // submitConnectResponse still uses map (CONNECT doesn't need duplicate headers)
        std::map<std::string, std::string> hdr_map;
        for (const auto& p : hdr_pairs) {
            hdr_map[p.first] = p.second;
        }
        if (getenv("QORE_HTTP2_DEBUG")) {
            fprintf(stderr, "HTTP2 DEBUG: send CONNECT response stream=%d status=%d\n",
                stream_id, status_code);
            fflush(stderr);
        }
        rv = session->submitConnectResponse(stream_id, status_code, hdr_map, xsink);
    } else {
        // Regular HTTP/2 response with body and END_STREAM
        const void* body_ptr = body ? body->getPtr() : nullptr;
        size_t body_len = body ? body->size() : 0;
        printd(5, "SocketHttp2SendResponsePollOperation() submitting response stream_id=%d status=%d body_len=%zu\n",
            stream_id, status_code, body_len);
        rv = session->submitResponse(stream_id, status_code, hdr_pairs, body_ptr, body_len, xsink);
    }
    if (rv != 0 || *xsink) {
        sock->priv->clearNonBlock();
        set_non_block = false;
        return;
    }

    printd(5, "SocketHttp2SendResponsePollOperation() submitResponse succeeded, want_write=%d\n",
        nghttp2_session_want_write(session->getSession()));
    h2_state = H2S_SENDING;
}

SocketHttp2SendResponsePollOperation::~SocketHttp2SendResponsePollOperation() {
}

QoreHashNode* SocketHttp2SendResponsePollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    // Get session from socket
    Http2Session* session = sock->priv->socket->priv->h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 session no longer available");
        return nullptr;
    }

    while (true) {
        switch (h2_state) {
            case H2S_SENDING: {
                // Send pending data using proper async I/O (non-blocking)
                int rv = session->sendPendingData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                // sendPendingData returns:
                //   0: success
                //   SOCK_POLLIN: need to poll for read (TLS renegotiation)
                //   SOCK_POLLOUT: need to poll for write
                //   -1: error (exception set)
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    // Need to poll for the direction SSL indicated
                    return getSocketPollInfoHash(xsink, rv);
                }

                // Check if there's still data in our buffer that wasn't sent yet
                if (session->hasPendingData()) {
                    // More data in buffer - poll for POLLOUT and retry
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }

                // Check if nghttp2 has more data to produce
                if (session->wantWrite()) {
                    continue;
                }

                // Data has been written to the socket buffer - transition to FLUSHING
                // to poll for POLLOUT one more time and verify all data is sent
                h2_state = H2S_FLUSHING;
                return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
            }

            case H2S_FLUSHING: {
                // After POLLOUT, try to send any remaining buffered data (non-blocking)
                // Try to send any remaining data (non-blocking)
                int rv = session->sendPendingData(0, xsink);

                if (*xsink) {
                    return nullptr;
                }

                // sendPendingData returns:
                //   0: success
                //   SOCK_POLLIN: need to poll for read (TLS renegotiation)
                //   SOCK_POLLOUT: need to poll for write
                //   -1: error (exception set)
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    // SSL/socket would block - need to poll for the indicated direction
                    return getSocketPollInfoHash(xsink, rv);
                }

                // Check if there's still data in our send buffer that wasn't sent yet
                if (session->hasPendingData()) {
                    // More data in buffer - poll for POLLOUT and retry
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }

                if (session->wantWrite()) {
                    // nghttp2 has more data to produce (flow control, etc.)
                    h2_state = H2S_SENDING;
                    continue;
                }

                // All done - both nghttp2 and our buffer are empty
                h2_state = H2S_SENT;
                // Goal reached - clear non-block flag so subsequent operations can proceed
                if (set_non_block) {
                    set_non_block = false;
                    sock->priv->clearNonBlock();
                }
                return nullptr;
            }

            case H2S_SENT:
                // Goal reached - clear non-block flag so subsequent operations can proceed
                if (set_non_block) {
                    set_non_block = false;
                    sock->priv->clearNonBlock();
                }
                return nullptr;

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected state: %d", h2_state);
                return nullptr;
        }
    }
}

Http2SessionPtr SocketHttp2SendResponsePollOperation::takeSession() {
    // Session is now managed by socket, not this operation
    return nullptr;
}

SocketHttp2SendStreamingResponsePollOperation::SocketHttp2SendStreamingResponsePollOperation(
        ExceptionSink* xsink, QoreSocketObject* sock, int32_t stream_id, int status_code,
        const QoreHashNode* headers, InputStream* input_stream, int64 chunk_size)
        : SocketPollSocketOperationBase(sock), stream_id(stream_id),
          input_stream(input_stream), chunk_size(chunk_size > 0 ? chunk_size : 16384),
          is_pollable(input_stream->supportsNonBlockingIo()) {

    AutoLocker al(sock->priv->m);

    // Get HTTP/2 session from socket
    Http2Session* session = sock->priv->socket->priv->h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session available; "
            "startPollSendHttp2StreamingResponse() must be called after "
            "startPollReadHttp2Request() completes on an active HTTP/2 connection");
        return;
    }

    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Build headers map
    std::map<std::string, std::string> hdr_map;
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            const char* key = hi.getKey();
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                hdr_map[key] = val.get<const QoreStringNode>()->c_str();
            }
        }
    }

    // Submit streaming response (headers only, deferred data provider)
    int rv = session->submitResponseStreaming(stream_id, status_code, hdr_map, xsink);
    if (rv != 0 || *xsink) {
        sock->priv->clearNonBlock();
        set_non_block = false;
        return;
    }

    // Cache the pollable file descriptor for non-blocking reads
    if (is_pollable) {
        stream_fd = input_stream->getPollableDescriptor();
        if (stream_fd < 0) {
            is_pollable = false;
        }
    }

    // Thread affinity ownership chain for the InputStream:
    //   1. Handler thread creates the InputStream (owns thread affinity)
    //   2. QPP method startPollSendHttp2StreamingResponse() constructs this operation,
    //      then calls body->unassignThread() to release affinity from the handler thread
    //   3. On first continuePoll() call (from the I/O worker thread), need_reassign triggers
    //      input_stream->reassignThread() to claim affinity on the worker thread
    //   4. All subsequent reads happen on that worker thread until completion

    printd(5, "SocketHttp2SendStreamingResponsePollOperation() headers submitted stream_id=%d\n", stream_id);
}

SocketHttp2SendStreamingResponsePollOperation::~SocketHttp2SendStreamingResponsePollOperation() {
}

QoreHashNode* SocketHttp2SendStreamingResponsePollOperation::continuePoll(ExceptionSink* xsink) {
    // Reassign the input stream to the current (worker) thread on first call
    if (need_reassign) {
        need_reassign = false;
        if (input_stream) {
            input_stream->reassignThread(xsink);
            if (*xsink) return nullptr;
        }
    }

    AutoLocker al(sock->priv->m);

    Http2Session* session = sock->priv->socket->priv->h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 session no longer available");
        return nullptr;
    }

    while (true) {
        switch (ss_state) {
            case SS_READ_CHUNK: {
                if (eof) {
                    // Send END_STREAM
                    int rv = session->sendStreamData(stream_id, nullptr, 0, true, xsink);
                    if (*xsink) return nullptr;
                    if (rv != 0) {
                        xsink->raiseException("HTTP2-ERROR", "failed to send END_STREAM");
                        return nullptr;
                    }
                    ss_state = SS_FLUSH;
                    continue;
                }

                if (is_pollable) {
                    // Non-blocking read for pollable streams.
                    // Use inline poll(0) to check if data is available without blocking,
                    // then readNonBlock() to get the data. This distinguishes:
                    // - poll ready + readNonBlock returns 0 → true EOF
                    // - poll not ready → would block, yield to event loop for socket I/O
                    assert(stream_fd >= 0);
                    struct pollfd pfd;
                    pfd.fd = stream_fd;
                    pfd.events = POLLIN;
                    pfd.revents = 0;
                    int poll_rv = poll(&pfd, 1, 0);
                    if (poll_rv < 0) {
                        xsink->raiseException("HTTP2-ERROR", "poll() on stream fd failed: %s",
                            strerror(errno));
                        // Send RST_STREAM to notify client of the error
                        ExceptionSink rst_xsink;
                        session->submitRstStream(stream_id, NGHTTP2_INTERNAL_ERROR, &rst_xsink);
                        if (!rst_xsink) {
                            session->sendPendingDataBlocking(100, &rst_xsink);
                        }
                        return nullptr;
                    }
                    if (poll_rv == 0) {
                        // Stream not ready — yield to event loop for socket I/O
                        // and retry reading on next continuePoll() call
                        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    }

                    // Stream FD is readable — do non-blocking read
                    SimpleRefHolder<BinaryNode> chunk(new BinaryNode);
                    chunk->preallocate(chunk_size);
                    int64 count = input_stream->readNonBlock(
                        const_cast<void*>(chunk->getPtr()), chunk_size, xsink);
                    if (*xsink) {
                        // Send RST_STREAM to notify client of the error
                        ExceptionSink rst_xsink;
                        session->submitRstStream(stream_id, NGHTTP2_INTERNAL_ERROR, &rst_xsink);
                        if (!rst_xsink) {
                            session->sendPendingDataBlocking(100, &rst_xsink);
                        }
                        return nullptr;
                    }
                    if (count == 0) {
                        // poll said readable but read returned 0 → EOF
                        eof = true;
                        continue;
                    }
                    chunk->setSize(count);
                    current_chunk = chunk.release();
                } else {
                    // Non-pollable (memory) streams - read() never blocks
                    current_chunk = input_stream->readHelper(chunk_size, xsink);
                    if (*xsink) {
                        // Send RST_STREAM to notify client of the error
                        ExceptionSink rst_xsink;
                        session->submitRstStream(stream_id, NGHTTP2_INTERNAL_ERROR, &rst_xsink);
                        if (!rst_xsink) {
                            session->sendPendingDataBlocking(100, &rst_xsink);
                        }
                        return nullptr;
                    }
                    if (!current_chunk) {
                        eof = true;
                        continue;
                    }
                }

                printd(5, "SocketHttp2SendStreamingResponsePollOperation::continuePoll() "
                    "read chunk size=%zu\n", current_chunk->size());
                ss_state = SS_SEND_CHUNK;
                continue;
            }

            case SS_SEND_CHUNK: {
                // Send the chunk as HTTP/2 DATA frames (not end_stream)
                int rv = session->sendStreamData(stream_id, current_chunk->getPtr(),
                    current_chunk->size(), false, xsink);
                if (*xsink) return nullptr;
                if (rv != 0) {
                    xsink->raiseException("HTTP2-ERROR", "failed to send stream data");
                    return nullptr;
                }
                current_chunk = nullptr;
                ss_state = SS_FLUSH;
                continue;
            }

            case SS_FLUSH: {
                // Flush pending data to socket
                int rv = session->sendPendingData(0, xsink);
                if (*xsink) return nullptr;

                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    return getSocketPollInfoHash(xsink, rv);
                }

                if (session->hasPendingData()) {
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }

                if (session->wantWrite()) {
                    continue;
                }

                // Check if the flow control window is exhausted.  If so, we
                // need to receive a WINDOW_UPDATE from the client before we can
                // send more data or declare completion.
                //
                // getStreamRemoteWindowSize() returns -1 when the stream no
                // longer exists (already closed after END_STREAM was sent and
                // acknowledged).  A negative value does NOT mean the window is
                // exhausted — it means the stream is gone, so we must NOT
                // enter SS_RECV_WINDOW (which would loop forever waiting for a
                // WINDOW_UPDATE that will never arrive).
                {
                    int32_t stream_window = session->getStreamRemoteWindowSize(stream_id);
                    int32_t conn_window = session->getStreamRemoteWindowSize(0);
                    if ((stream_window >= 0 && stream_window == 0) || conn_window == 0) {
                        ss_state = SS_RECV_WINDOW;
                        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                    }
                }

                if (eof) {
                    // All done
                    ss_state = SS_DONE;
                    if (set_non_block) {
                        set_non_block = false;
                        sock->priv->clearNonBlock();
                    }
                    return nullptr;
                }

                // More data to read
                ss_state = SS_READ_CHUNK;
                continue;
            }

            case SS_RECV_WINDOW: {
                // Read incoming data from client (WINDOW_UPDATE, PING, etc.)
                // to update flow control windows so we can continue sending.
                int rv = session->receiveData(0, xsink);
                if (*xsink) return nullptr;
                if (rv == -1) {
                    // Would block - poll for read
                    return getSocketPollInfoHash(xsink, SOCK_POLLIN);
                }
                if (rv == 1) {
                    // Peer closed connection during streaming response
                    if (set_non_block) {
                        set_non_block = false;
                        sock->priv->clearNonBlock();
                    }
                    ss_state = SS_DONE;
                    return nullptr;
                }
                // Data received - WINDOW_UPDATE may have been processed.
                // Try to flush buffered data now that the window may be open.
                ss_state = SS_FLUSH;
                continue;
            }

            case SS_DONE:
                if (set_non_block) {
                    set_non_block = false;
                    sock->priv->clearNonBlock();
                }
                return nullptr;

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected streaming state: %d", ss_state);
                return nullptr;
        }
    }
}

// HTTP/2 Flush Poll Operation implementation

SocketHttp2FlushPollOperation::SocketHttp2FlushPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock) : SocketPollSocketOperationBase(sock) {
    AutoLocker al(sock->priv->m);

    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;
}

QoreHashNode* SocketHttp2FlushPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    Http2Session* session = sock->priv->socket->priv->h2_session.get();
    if (!session) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 session no longer available");
        return nullptr;
    }

    while (true) {
        switch (h2f_state) {
            case H2F_FLUSHING: {
                int rv = session->sendPendingData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == SOCK_POLLIN || rv == SOCK_POLLOUT) {
                    return getSocketPollInfoHash(xsink, rv);
                }

                if (session->hasPendingData()) {
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }

                if (session->wantWrite()) {
                    continue;
                }

                // All pending data flushed
                h2f_state = H2F_DONE;
                if (set_non_block) {
                    set_non_block = false;
                    sock->priv->clearNonBlock();
                }
                return nullptr;
            }

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected flush state: %d", h2f_state);
                return nullptr;
        }
    }
}

// HTTP/2 Client Multiplex Poll Operation implementation

SocketHttp2ClientMultiplexPollOperation::SocketHttp2ClientMultiplexPollOperation(ExceptionSink* xsink,
        QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock) {
    AutoLocker al(sock->priv->m);

    // Check if socket is open and valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    // Set non-blocking mode
    if (sock->priv->setNonBlock(xsink)) {
        return;
    }
    set_non_block = true;

    // Check if there's already an HTTP/2 session stored in the socket (from a previous request)
    bool reused_session = false;
    if (sock->priv->socket->priv->h2_session) {
        // Reuse existing session (socket owns it)
        h2_session = sock->priv->socket->priv->h2_session;
        reused_session = true;
    } else {
        // Initialize new HTTP/2 client session and store it on the socket
        if (initSession(xsink)) {
            sock->priv->clearNonBlock();
            set_non_block = false;
            return;
        }
    }

    // Set up stream completion callback to queue responses
    // Capture callback_guard by value (shared_ptr copy) to ensure it outlives this object.
    // The mutex in the guard ensures no TOCTOU race between checking the destroyed flag
    // and using 'this' - both happen while holding the mutex.
    auto guard = callback_guard;
    h2_session->setStreamCompleteCallback(
        [this, guard](int32_t stream_id, Http2StreamInfo* stream, ExceptionSink* xsink) {
            // Lock mutex and check if poll operation is being destroyed
            // The lock ensures we don't race with the destructor
            std::lock_guard<std::mutex> lg(guard->mutex);
            if (guard->destroyed) {
                printd(5, "onStreamComplete: poll operation destroyed, ignoring callback\n");
                return;
            }
            this->onStreamComplete(stream_id, stream, xsink);
        });

    // Set initial state based on whether we reused a session
    if (reused_session) {
        // Session already established - go directly to reading frames
        h2_state = H2C_READING;
    } else {
        // New session - need to exchange connection preface
        h2_state = H2C_SEND_PREFACE;
    }
}

SocketHttp2ClientMultiplexPollOperation::~SocketHttp2ClientMultiplexPollOperation() {
    printd(5, "~SocketHttp2ClientMultiplexPollOperation() starting\n");

    // Set destroyed flag while holding the mutex.
    // This ensures any in-flight callback completes before we proceed, and any
    // callback that starts after this will see destroyed=true and return early.
    // The mutex eliminates the TOCTOU race between flag check and 'this' usage.
    {
        std::lock_guard<std::mutex> lg(callback_guard->mutex);
        callback_guard->destroyed = true;
    }

    // Clear stream completion callback - session is guaranteed valid via shared_ptr
    if (h2_session) {
        printd(5, "~SocketHttp2ClientMultiplexPollOperation() clearing callback\n");
        h2_session->clearStreamCompleteCallback();
        printd(5, "~SocketHttp2ClientMultiplexPollOperation() callback cleared\n");
    }

    // Deref any queued responses
    printd(5, "~SocketHttp2ClientMultiplexPollOperation() getting lock for responses\n");
    if (getenv("QORE_HTTP2_DEBUG")) {
        fprintf(stderr, "HTTP2 DEBUG: ~SocketHttp2ClientMultiplexPollOperation() getting lock for responses\n");
        fflush(stderr);
    }
    AutoLocker al(response_lock);
    printd(5, "~SocketHttp2ClientMultiplexPollOperation() derefing %zu responses\n", completed_responses.size());
    if (getenv("QORE_HTTP2_DEBUG")) {
        fprintf(stderr, "HTTP2 DEBUG: ~SocketHttp2ClientMultiplexPollOperation() derefing %zu responses\n",
            completed_responses.size());
        fflush(stderr);
    }
    for (QoreHashNode* h : completed_responses) {
        h->deref(nullptr);
    }
    completed_responses.clear();
    printd(5, "~SocketHttp2ClientMultiplexPollOperation() done\n");
}

int SocketHttp2ClientMultiplexPollOperation::initSession(ExceptionSink* xsink) {
    // Determine scheme from socket state (secure = https, otherwise http)
    const char* scheme = sock->priv->socket->priv->ssl ? "https" : "http";

    // Create client-side HTTP/2 session using the underlying QoreSocket's priv
    h2_session = Http2Session::createClient(sock->priv->socket->priv, xsink, scheme);
    if (!h2_session) {
        return -1;
    }
    // Store session on socket for shared access
    sock->priv->socket->priv->h2_session = h2_session;
    return 0;
}

void SocketHttp2ClientMultiplexPollOperation::onStreamComplete(int32_t stream_id, Http2StreamInfo* stream,
        ExceptionSink* xsink) {
    // Build response hash from stream info
    ReferenceHolder<QoreHashNode> response(new QoreHashNode(autoTypeInfo), xsink);

    response->setKeyValue("stream_id", stream_id, xsink);
    response->setKeyValue("status_code", stream->status_code, xsink);

    // Convert headers map to Qore hash (handle duplicate headers per RFC 7540)
    if (!stream->headers.empty()) {
        response->setKeyValue("headers", h2HeadersToQoreHash(stream->headers), xsink);
    }

    // Convert body vector to Qore binary or string based on content-type
    if (!stream->body.empty()) {
        // Check if content-type indicates text-based content
        bool is_text = false;
        auto ct_it = stream->headers.find("content-type");
        if (ct_it != stream->headers.end() && !ct_it->second.empty()) {
            // Extract media type (before any parameters like charset)
            std::string ct = ct_it->second.back();
            size_t semicolon = ct.find(';');
            if (semicolon != std::string::npos) {
                ct = ct.substr(0, semicolon);
            }
            // Trim whitespace
            while (!ct.empty() && isspace(static_cast<unsigned char>(ct.back()))) {
                ct.pop_back();
            }
            while (!ct.empty() && isspace(static_cast<unsigned char>(ct.front()))) {
                ct.erase(0, 1);
            }
            // Convert to lowercase for comparison
            std::transform(ct.begin(), ct.end(), ct.begin(),
                [](unsigned char c) { return std::tolower(c); });

            // Check for text types
            is_text = (ct.size() >= 5 && ct.compare(0, 5, "text/") == 0) ||  // text/*
                      ct == "application/json" ||
                      ct == "application/xml" ||
                      ct == "application/javascript" ||
                      ct == "application/x-www-form-urlencoded" ||
                      (ct.size() > 5 && ct.compare(ct.size() - 5, 5, "+json") == 0) ||  // *+json
                      (ct.size() > 4 && ct.compare(ct.size() - 4, 4, "+xml") == 0);    // *+xml
        }

        if (is_text) {
            response->setKeyValue("body", new QoreStringNode(
                reinterpret_cast<const char*>(stream->body.data()),
                stream->body.size()), xsink);
        } else {
            SimpleRefHolder<BinaryNode> body(new BinaryNode);
            body->append(stream->body.data(), stream->body.size());
            response->setKeyValue("body", body.release(), xsink);
        }
    }

    // Include trailers if present
    if (!stream->trailers.empty()) {
        response->setKeyValue("trailers", h2HeadersToQoreHash(stream->trailers, true), xsink);
    }

    // Mark that the stream has ended (END_STREAM was received)
    response->setKeyValue("end_stream", true, xsink);

    // Queue the response
    {
        AutoLocker al(response_lock);
        completed_responses.push_back(response.release());
    }
}

void SocketHttp2ClientMultiplexPollOperation::cancelStream(int32_t stream_id, ExceptionSink* xsink) {
    if (!h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session available");
        return;
    }
    h2_session->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
}

int32_t SocketHttp2ClientMultiplexPollOperation::submitRequest(const char* method, const char* path,
        const std::map<std::string, std::string>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink) {
    if (!h2_session) {
        xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session available");
        return -1;
    }
    if (h2_state == H2C_CLOSED) {
        xsink->raiseException("HTTP2-ERROR", "connection is closed");
        return -1;
    }
    return h2_session->submitRequest(method, path, headers, body, body_len, xsink);
}

QoreHashNode* SocketHttp2ClientMultiplexPollOperation::continuePoll(ExceptionSink* xsink) {
    AutoLocker al(sock->priv->m);

    while (true) {
        switch (h2_state) {
            case H2C_SEND_PREFACE: {
                // Send client connection preface (SETTINGS frame)
                int rv = h2_session->sendConnectionPreface(xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == -1) {
                    // Would block - need to poll for write
                    return getSocketPollInfoHash(xsink, SOCK_POLLOUT);
                }
                h2_state = H2C_RECV_PREFACE;
                // Fall through to receive preface
            }

            case H2C_RECV_PREFACE:
            case H2C_READING: {
                // Check if there are completed streams - dispatch via callback
                if (h2_session->hasCompletedStreams()) {
                    // Flush any pending output before dispatching
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLIN || srv == SOCK_POLLOUT) {
                        // Socket buffer full; poll and retry
                        return getSocketPollInfoHash(xsink, srv);
                    }
                    // Callbacks are invoked by the session via StreamCompleteCallback
                    // Continue reading for more responses
                }

                // First try to send any pending data (requests submitted from other threads)
                if (h2_session->wantWrite()) {
                    int srv = h2_session->sendPendingData(0, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == SOCK_POLLOUT) {
                        // Need to poll for write
                        return getSocketPollInfoHash(xsink, SOCK_POLLIN | SOCK_POLLOUT);
                    }
                }

                // Receive and process HTTP/2 frames
                int rv = h2_session->receiveData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (rv == 1) {
                    // Connection closed by peer
                    peer_closed = true;
                    h2_state = H2C_CLOSED;
                    if (set_non_block) {
                        set_non_block = false;
                        sock->priv->clearNonBlock();
                    }
                    return nullptr;
                }
                if (rv == -1) {
                    // Would block - poll for read (and write if we have pending data)
                    int events = SOCK_POLLIN;
                    if (h2_session->wantWrite() || h2_session->hasPendingData()) {
                        events |= SOCK_POLLOUT;
                    }
                    return getSocketPollInfoHash(xsink, events);
                }

                // Data received - check for completed streams
                if (h2_session->hasCompletedStreams()) {
                    // Responses are dispatched via callback mechanism
                    // Continue the loop to process more frames
                    continue;
                }

                // Check for intermediate body data on streaming streams.
                // Batch limit prevents monopolizing the poll loop when many streams
                // have data; remaining data will be picked up on the next poll cycle.
                {
                    int32_t streaming_id = 0;
                    int batch = 0;
                    static const int MAX_STREAMING_BATCH = 16;
                    while (batch < MAX_STREAMING_BATCH
                            && h2_session->hasStreamingData(streaming_id)) {
                        // Take body data from the streaming stream
                        BinaryNode* body = h2_session->takeStreamData(streaming_id, 0, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        if (!body) {
                            break;
                        }
                        // Create a partial response with just the body
                        ReferenceHolder<QoreHashNode> partial(new QoreHashNode(autoTypeInfo), xsink);
                        partial->setKeyValue("stream_id", streaming_id, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        partial->setKeyValue("body", body, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        // No end_stream flag - this is intermediate data
                        {
                            AutoLocker rl(response_lock);
                            completed_responses.push_back(partial.release());
                        }
                        ++batch;
                    }
                }

                // No completed streams yet - check if we're past the preface stage
                if (h2_state == H2C_RECV_PREFACE) {
                    h2_state = H2C_READING;
                }

                // Check for GOAWAY
                if (h2_session->isGoawayReceived()) {
                    h2_state = H2C_CLOSED;
                    if (set_non_block) {
                        set_non_block = false;
                        sock->priv->clearNonBlock();
                    }
                    return nullptr;
                }

                // Poll for more data (and write if we have pending data)
                int events = SOCK_POLLIN;
                if (h2_session->wantWrite() || h2_session->hasPendingData()) {
                    events |= SOCK_POLLOUT;
                }
                return getSocketPollInfoHash(xsink, events);
            }

            case H2C_CLOSED:
                if (set_non_block) {
                    set_non_block = false;
                    sock->priv->clearNonBlock();
                }
                return nullptr;

            default:
                xsink->raiseException("HTTP2-ERROR", "unexpected client multiplex state: %d", h2_state);
                return nullptr;
        }
    }
}

SocketRecvFromPollOperation::SocketRecvFromPollOperation(ExceptionSink* xsink, size_t max_size,
        QoreSocketObject* sock) : SocketPollSocketOperationBase(sock), max_size(max_size), output(xsink) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    if (!sock->priv->setNonBlock(xsink)) {
        poll_state.reset(sock->priv->socket->startRecvFrom(xsink, max_size));
        if (!poll_state) {
            sock->priv->clearNonBlock();
        } else {
            set_non_block = true;
        }
    }
}

QoreHashNode* SocketRecvFromPollOperation::continuePoll(ExceptionSink* xsink) {
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
    if (*xsink || !rc) {
        if (!*xsink) {
            // get output (hash with data + address info)
            output = poll_state->takeOutput().get<QoreHashNode>();
            received = true;
        }
        // release the AbstractPollState value
        poll_state.reset();
        sock->priv->clearNonBlock();
        set_non_block = false;
        return nullptr;
    }
    return getSocketPollInfoHash(xsink, rc);
}

SocketSendToPollOperation::SocketSendToPollOperation(ExceptionSink* xsink, const char* host, int port, int family,
        BinaryNode* data, QoreSocketObject* sock)
        : SocketPollSocketOperationBase(sock), data_holder(data) {
    AutoLocker al(sock->priv->m);

    // throw an exception and exit if the object is no longer open or valid
    if (sock->priv->checkOpen(xsink)) {
        return;
    }

    // Resolve the destination address
    QoreAddrInfo ai;
    QoreString service_str;
    service_str.sprintf("%d", port);
    if (ai.getInfo(xsink, host, service_str.c_str(), family, 0, SOCK_DGRAM, 0)) {
        return;
    }

    struct addrinfo* aip = ai.getAddrInfo();
    if (!aip) {
        xsink->raiseException("SOCKET-SENDTO-ERROR", "could not resolve destination address '%s:%d'", host, port);
        return;
    }

    // Copy resolved address
    memcpy(&dest_addr, aip->ai_addr, aip->ai_addrlen);
    dest_addr_len = aip->ai_addrlen;
    resolved = true;

    if (!sock->priv->setNonBlock(xsink)) {
        poll_state.reset(sock->priv->socket->startSendTo(xsink,
            reinterpret_cast<const char*>(data_holder->getPtr()), data_holder->size(),
            (const struct sockaddr*)&dest_addr, dest_addr_len));
        if (!poll_state) {
            sock->priv->clearNonBlock();
        } else {
            set_non_block = true;
        }
    }
}

QoreHashNode* SocketSendToPollOperation::continuePoll(ExceptionSink* xsink) {
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

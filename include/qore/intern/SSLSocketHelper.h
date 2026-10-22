/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SSLSocketHelper.h

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

#ifndef _QORE_SSLSOCKETHELPER_H

#define _QORE_SSLSOCKETHELPER_H

#include <openssl/ssl.h>

#include <atomic>
#include <string>
#include <vector>

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

//! Classifies the OpenSSL call that reported a failure
/** Only calls that read from or write to the socket can fail because of the peer or the transport.
    An empty OpenSSL error queue means something different for each kind: for a transport call it
    says the connection went away, while for a setup call it says only that the TLS library gave no
    reason — the socket is not involved and must not be blamed or closed.
*/
enum class SslCall : unsigned char {
    //! a local call: context or handle creation and configuration; never touches the socket
    Setup,
    //! a call that reads from or writes to the socket
    Transport,
};

class QoreStringNode;

//! Returns why a certificate failed verification, for the error raised for the refused TLS handshake
/** Shared by TLS connections over TCP and QUIC, so that a refused certificate is described in the same way whatever
    the transport (issue #3818)

    @param x509_ctx the store context of the verify callback call that reports the failure

    @return a new string of the form <tt>verify error <code> depth <depth>: <reason>: <subject></tt>, with the issuer
    added when the issuer certificate cannot be found; the caller owns the reference
*/
DLLLOCAL QoreStringNode* q_ssl_get_verify_error_desc(X509_STORE_CTX* x509_ctx);

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
    friend class SSLSocketReferenceHelper;
public:
    DLLLOCAL SSLSocketHelper(qore_socket_private& qs) : qs(qs) {
    }

    //! Runs the one-time initialization of the TLS code; called when the library is initialized
    /** Creates the process-wide BIO method of layered connections, so that a child process created with fork()
        cannot inherit its initialization in progress in another thread
    */
    DLLLOCAL static void initLibrary();

    //! Releases a reference; the last one deletes the object
    /** The references are released by different threads: a thread closing the socket releases the socket's own
        reference while a thread in an I/O call holds another one, so the count is atomic.
    */
    DLLLOCAL bool deref() {
        if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
            return true;
        }
        return false;
    }

    //! Takes a reference
    /** The caller must hold a reference already, or hold qore_socket_private::close_m while the socket holds its
        own; see qore_socket_private::getSsl()
    */
    DLLLOCAL void ref() {
        refs.fetch_add(1, std::memory_order_relaxed);
    }

    //! Returns true if no reference other than the socket's own is held
    /** Called with qore_socket_private::close_m held while the socket holds its reference: the references of other
        threads are only taken with that lock held, so no SSL call on another thread can be in progress or start
        until the lock is released.
    */
    DLLLOCAL bool onlySocketReference() const {
        return refs.load(std::memory_order_acquire) == 1;
    }

    // do nonblocking I/O for polling
    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL int doNonBlockingIo(ExceptionSink* xsink, const char* mname, void* buf, size_t size, SslAction action,
            size_t& real_io);

    DLLLOCAL int setClient(ExceptionSink* xsink, const char* mname, const char* sni_target_host, int sd,
            QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pkey = nullptr);
    DLLLOCAL int setServer(ExceptionSink* xsink, const char* mname, int sd, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

    //! Sets up a client TLS connection whose records are carried by another TLS connection
    /** Used for a TLS connection to a server through a tunnel of an HTTPS proxy: the outer connection is the
        connection to the proxy.  All data of this connection is read and written through the outer connection, so
        waiting for the socket to become readable or writable works as for a TLS connection on the socket itself.

        @param xsink exception sink
        @param mname the method name for exceptions
        @param sni_target_host the host name to send with SNI, if any
        @param outer the TLS connection that carries this connection; this object takes over one reference to it,
        which is released when this object is destroyed
        @param cert the client certificate, if any
        @param pkey the private key of the client certificate, if any

        @return 0 for success, -1 for error (exception raised); the reference to \a outer is taken over in any case

        @since %Qore 3.0
    */
    DLLLOCAL int setLayeredClient(ExceptionSink* xsink, const char* mname, const char* sni_target_host,
            SSLSocketHelper* outer, QoreSSLCertificate* cert = nullptr, QoreSSLPrivateKey* pkey = nullptr);

    //! Returns true if this connection is carried by another TLS connection
    /** @since %Qore 3.0
    */
    DLLLOCAL bool isLayered() const {
        return outer != nullptr;
    }
    //! Sends the close_notify alert once, without blocking and without waiting for the peer's
    /** For a connection whose write side is about to be closed (RFC 8446 section 6.1).  Nothing is sent if the alert
        (or a fatal alert) was sent already, or if the handshake did not complete.  The descriptor is made
        non-blocking for the attempt and restored afterwards: an alert that cannot be sent at once because the send
        buffer is full is not sent.  A connection carried by another TLS connection sends its alert first, then the
        carrying connection sends its own.

        @param fd the descriptor of the connection

        @since %Qore 3.0
    */
    DLLLOCAL void sendCloseNotify(int fd);
    // returns 0 for success
    DLLLOCAL int shutdown(ExceptionSink* xsink);

    //! Starts an SSL negotiation for a client in nonblocking mode
    DLLLOCAL int startConnect(ExceptionSink* xsink);

    //! Starts an SSL negotiation for a server in nonblocking mode
    DLLLOCAL int startAccept(ExceptionSink* xsink);

    //! Starts an SSL shutdown in nonblocking mode
    DLLLOCAL int startShutdown(ExceptionSink* xsink);

    DLLLOCAL const char* getCipherName() const;
    DLLLOCAL const char* getCipherVersion() const;
    DLLLOCAL X509* getPeerCertificate() const;
    DLLLOCAL long verifyPeerCertificate() const;

    //! Sets the verification mode; returns -1 if the configured CA locations cannot be loaded
    DLLLOCAL int setVerifyMode(int mode, bool accept_all_certs, const std::string& target);

    DLLLOCAL bool captureRemoteCert() const;
    DLLLOCAL void clearRemoteCertContext() const;

    //! Sets ALPN protocols for client-side negotiation (must be called before connect)
    /** @param protocols list of protocol strings in preference order (e.g., {"h2", "http/1.1"})
        @return 0 on success, -1 on error
        @since Qore 3.0
    */
    DLLLOCAL int setAlpnProtocols(const std::vector<std::string>& protocols);

    //! Sets supported ALPN protocols for server-side negotiation (must be called before accept)
    /** @param protocols list of protocol strings the server supports
        @since Qore 3.0
    */
    DLLLOCAL void setServerAlpnProtocols(const std::vector<std::string>& protocols);

    //! Returns the negotiated ALPN protocol after connection
    /** @return the selected protocol string, or empty string if ALPN was not negotiated
        @since Qore 3.0
    */
    DLLLOCAL std::string getAlpnProtocol() const;

    //! Returns true if HTTP/2 ("h2") was negotiated via ALPN
    /** @since Qore 3.0
    */
    DLLLOCAL bool isHttp2() const;

    //! Returns the number of bytes already decrypted and buffered in the SSL layer
    /** This is a lightweight check (no I/O) that reads an internal counter.
        Must be called while the caller holds appropriate locking to prevent
        concurrent SSL operations (SSL objects are not thread-safe).

        @return number of readable bytes buffered in the SSL object, 0 if none

        @since Qore 3.0
    */
    DLLLOCAL int pending() const;

private:
    qore_socket_private& qs;
    SSL_METHOD_CONST SSL_METHOD* meth = nullptr;
    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    //! the references to this object: the socket's own and those of the calls using it; see qore_socket_private::getSsl()
    std::atomic<unsigned> refs{1};
#ifdef DEBUG
    //! Debug-only: the number of SSLSocketReferenceHelper objects for this object; see sslError()
    std::atomic<int> dbg_call_refs{0};
#endif
    //! the TLS connection that carries this connection, if any; see setLayeredClient()
    SSLSocketHelper* outer = nullptr;

    //! ALPN protocols for client-side negotiation (wire format)
    std::vector<unsigned char> alpn_wire_format;
    //! ALPN protocols for server-side selection
    std::vector<std::string> server_alpn_protocols;

    DLLLOCAL int setIntern(ExceptionSink* xsink, const char* meth, int sd, QoreSSLCertificate* cert = nullptr,
            QoreSSLPrivateKey* pkey = nullptr);

#ifdef HAVE_BIO_METH_NEW
    //! Returns the BIO method that reads and writes through the outer connection of a layered connection
    DLLLOCAL static BIO_METHOD* getLayeredBioMethod();
    DLLLOCAL static int layeredBioWrite(BIO* bio, const char* data, size_t len, size_t* written);
    DLLLOCAL static int layeredBioRead(BIO* bio, char* data, size_t len, size_t* read);
    DLLLOCAL static long layeredBioCtrl(BIO* bio, int cmd, long num, void* ptr);
    DLLLOCAL static int layeredBioCreate(BIO* bio);
    //! Sets the retry flags of a layered BIO after an I/O call on the outer connection did not complete
    /** @return true if the call can be retried
    */
    DLLLOCAL static bool setLayeredBioRetry(BIO* bio, SSL* outer_ssl, int rc);
#endif

    //! Advances the TLS state generation of the socket when a handshake starts or completes
    /** A handshake can change the cipher of the connection; see qore_socket_private::tls_state_gen.
    */
    DLLLOCAL static void infoCallback(const SSL* ssl, int where, int ret);

    //! Static callback for server-side ALPN protocol selection
    static int alpnSelectCallback(SSL* ssl, const unsigned char** out, unsigned char* outlen,
        const unsigned char* in, unsigned int inlen, void* arg);

    DLLLOCAL ~SSLSocketHelper();

    //! Reports the errors of a failed SSL call
    /** Must be called with an SSLSocketReferenceHelper for this object: it can close the socket, which releases the
        socket's reference, and a thread closing the socket can have released it already.
    */
    DLLLOCAL bool sslError(ExceptionSink* xsink, const char* meth, const char* msg, bool always_error = true,
            SslCall call = SslCall::Transport);

    //! Reports a failed SSL call that returned SSL_ERROR_SYSCALL; must be called as sslError()
    DLLLOCAL int sysCallError(ExceptionSink* xsink, int rc, const char* mname, const char* ssl_func);

    //! Reports one OpenSSL error-queue entry, or the failure of a call that queued nothing
    /** @param e a packed @c ERR_get_error() code, or 0 when the OpenSSL error queue was empty; what
        that means depends on \a call
        @param sockerr the socket errno captured before the OpenSSL calls that can clobber it
        @param call whether the failing call touches the socket; with an empty error queue a
        @ref SslCall::Transport "Transport" call means the connection went away, while a
        @ref SslCall::Setup "Setup" call means only that the TLS library gave no reason
    */
    DLLLOCAL void handleErrorIntern(ExceptionSink* xsink, unsigned long e, int sockerr, const char* mname,
            const char* func, bool always_error, SslCall call);
};

//! Holds a reference to an SSLSocketHelper for the duration of a call of one of its methods
/** The caller must already hold a reference; see SSLSocketHelper::ref()
*/
class SSLSocketReferenceHelper {
public:
    DLLLOCAL SSLSocketReferenceHelper(SSLSocketHelper* s, bool set_thread_context = false);

    DLLLOCAL ~SSLSocketReferenceHelper();

protected:
    SSLSocketHelper* s;
    bool context_saved = false;
};

#endif

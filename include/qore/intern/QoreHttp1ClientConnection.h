/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttp1ClientConnection.h

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

#ifndef _QORE_INTERN_QOREHTTP1CLIENTCONNECTION_H

#define _QORE_INTERN_QOREHTTP1CLIENTCONNECTION_H

#include <qore/HttpClientConnection.h>

#include <string>

class QoreSocketObject;
class QoreSSLCertificate;
class QoreSSLPrivateKey;
class Http1ClientPollOperationPriv;

//! SSL/TLS configuration for Http1ClientConnection construction
/** Passed to the constructor so SSL settings are applied to the socket
    BEFORE the poll op is submitted to the I/O controller — eliminates the
    race where the I/O thread starts the SSL handshake before configureSsl()
    can run.

    @since %Qore 2.3
*/
struct Http1SslConfig {
    int verify_mode = 0;        //!< SSL_VERIFY_NONE or SSL_VERIFY_PEER etc.
    bool accept_all = false;    //!< accept self-signed certificates
    QoreSSLCertificate* cert = nullptr;  //!< client cert for mutual TLS (NOT ref'd by this struct)
    QoreSSLPrivateKey* key = nullptr;    //!< client key for mutual TLS (NOT ref'd by this struct)
};

//! HTTP/1.1 C++ client connection
/** Wraps a @ref Http1ClientPollOperationPriv and the socket it operates on,
    submits the poll op to the global AsyncIoController on construction, and
    exposes @ref submitRequest for sync-over-async request dispatch.

    The connection starts in CONNECTING state; callers must call
    @ref waitForReadyOrError before @ref submitRequest.

    @since %Qore 2.3
*/
class Http1ClientConnection : public HttpClientConnectionBase {
public:
    //! Creates a new HTTP/1.1 client connection (direct, no proxy)
    /** Creates a socket, a @ref SocketConnectPollOperation, and a
        @ref Http1ClientPollOperationPriv wrapping the two, submits the
        poll op to the global AsyncIoController, and returns.  The
        connection is in CONNECTING state until @ref waitForReadyOrError
        returns.

        @param target_host target hostname (used for the Host header)
        @param target_port target TCP port
        @param ssl_required if @c true, SSL/TLS is required (HTTPS)
        @param xsink exception sink — set on construction failure
        @param mgr optional owning manager — if non-null, registered via
            @ref setManager before the poll op is submitted to the I/O
            controller, eliminating the race where the I/O thread fires
            @ref onClosedHook before the caller can register a manager
        @param ssl_config SSL configuration applied before submission
    */
    DLLLOCAL Http1ClientConnection(const char* target_host, int target_port,
        bool ssl_required, ExceptionSink* xsink,
        HttpClientConnectionManagerBase* mgr = nullptr,
        const Http1SslConfig& ssl_config = Http1SslConfig{});

    //! Creates a new HTTP/1.1 client connection via an HTTP proxy
    /** TCP connects to the proxy.  For HTTPS targets (@a ssl_required is
        true), a CONNECT tunnel is established through the proxy before
        upgrading to SSL.  For plain HTTP targets, requests use absolute
        URIs through the proxy.

        @param target_host target hostname (used for the Host header and
            the CONNECT target)
        @param target_port target TCP port
        @param ssl_required if @c true, SSL/TLS is required (HTTPS) — the
            proxy tunnel will be established via CONNECT before SSL upgrade
        @param proxy_host proxy hostname to connect to
        @param proxy_port proxy TCP port to connect to
        @param xsink exception sink — set on construction failure
        @param mgr optional owning manager
        @param ssl_config SSL configuration applied before submission

        @since %Qore 2.3
    */
    DLLLOCAL Http1ClientConnection(const char* target_host, int target_port,
        bool ssl_required, const char* proxy_host, int proxy_port,
        ExceptionSink* xsink, HttpClientConnectionManagerBase* mgr = nullptr,
        const Http1SslConfig& ssl_config = Http1SslConfig{});

    DLLLOCAL virtual ~Http1ClientConnection();

    //! Sets the controller-submission owner string before submission to
    //! the AsyncIoController.
    /** Must be called BEFORE construction submits the poll op (i.e., the
        manager constructs an "owner-pending" connection, calls @ref setOwner,
        then triggers submission).  In the simple unconstructed-by-manager
        case, the constructor uses a default per-instance owner string.

        For now (Phase P2), the constructor submits immediately and there is
        no opportunity to call setOwner before submission.  Phase P3 will
        introduce a two-phase construction model where the manager creates
        the connection in an unsubmitted state, calls setOwner, then submits.
        Until then this setter just stores the value for the next submission.

        @param owner the owner string to use in the SocketPollOperationInfo
            hash; must outlive this connection (or be copied internally —
            this implementation copies)

        @since %Qore 2.3
    */
    DLLLOCAL void setOwner(const char* owner) {
        if (owner) {
            owner_str = owner;
        }
    }

    //! Configures SSL settings on this connection's socket
    /** Must be called before the connection reaches the SSL handshake state.
        Safe to call immediately after construction — TCP connect is async
        and the SSL handshake doesn't start until TCP connect completes.

        @param verify_mode SSL verification mode (SSL_VERIFY_NONE or SSL_VERIFY_PEER)
        @param accept_all if true, accept self-signed certificates
        @param cert client certificate for mutual TLS (will be ref'd; nullptr = none)
        @param key client private key for mutual TLS (will be ref'd; nullptr = none)

        @since %Qore 2.3
    */
    DLLLOCAL void configureSsl(int verify_mode, bool accept_all,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* key);

    // --- HttpClientConnectionBase overrides ---

    HttpClientProtocol getProtocol() const override {
        return HttpClientProtocol::H1;
    }

    DLLEXPORT int getActiveStreamCount() const override;

    DLLEXPORT QoreHashNode* submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) override;

    DLLEXPORT int64_t submitRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink) override;

    //! Submits a request with streaming send (chunked TE); body pushed via pushSendData()
    /** @param method HTTP method
        @param path request path
        @param headers request headers (user-provided)
        @param streaming_recv if true, response is delivered via channel (streaming receive)
        @param channel_out if streaming_recv, receives the response channel
        @param xsink exception sink
        @return result hash with stream_id and future (if not streaming_recv)
    */
    DLLEXPORT QoreHashNode* submitRequestStreamingSend(const char* method, const char* path,
        const QoreHashNode* headers, bool streaming_recv,
        QoreChannel*& channel_out, ExceptionSink* xsink);

    //! Pushes body data for a streaming send request
    /** @param data body chunk; nullptr signals end-of-body
        @param xsink exception sink
    */
    DLLEXPORT void pushSendData(const QoreStringNode* data, ExceptionSink* xsink);

    //! Sets HTTP trailers for a streaming send request
    /** Must be called before pushSendData(nullptr).
        @param trailers hash of trailer key-value pairs
        @param xsink exception sink
    */
    DLLEXPORT void setTrailers(const QoreHashNode* trailers, ExceptionSink* xsink);

    //! Submits a request with a caller-provided action (for poll-based APIs)
    /** Like submitRequest() but uses the caller's action instead of creating
        a PromiseAction internally.  Used by poll-based wrappers that need
        a PromiseNotifierAction to signal an EventNotifier on completion.

        @param method HTTP method
        @param path request path
        @param headers request headers
        @param body request body (may be nullptr)
        @param body_len body length
        @param action the action to use (ownership transferred to poll op)
        @param xsink exception sink
        @return stream_id on success, -1 on error

        @since %Qore 2.3
    */
    DLLEXPORT int64_t submitRequestWithAction(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        AbstractAsyncAction* action, ExceptionSink* xsink);

    DLLEXPORT void closeConnection(ExceptionSink* xsink) override;

protected:
    DLLLOCAL QoreHashNode* getReferencedErrorInfo() override;

private:
    //! The socket QoreObject (ref'd).  Owns a QoreSocketObject priv.
    QoreObject* sock_obj = nullptr;

    //! The socket priv (raw pointer — ownership via sock_obj)
    QoreSocketObject* sock_priv = nullptr;

    //! The poll op QoreObject (ref'd).  Owns a Http1ClientPollOperationPriv.
    QoreObject* poll_op_obj = nullptr;

    //! The poll op priv (raw pointer — ownership via poll_op_obj).
    Http1ClientPollOperationPriv* poll_op_priv = nullptr;

    //! True once the poll op has been submitted to the controller
    bool submitted_to_controller = false;

    //! Owner string used for the controller submit info hash.
    /** Defaults to a per-instance string built from @c this in
        @ref buildAndSubmit.  Phase P3+ managers can override via
        @ref setOwner before construction-time submission to use their own
        owner string and benefit from @c cancelByOwner cleanup.

        @note @c manager_ and @c onclose_lock live on
        @ref HttpClientConnectionBase (the base class) so all H1/H2/H3
        connection wrappers automatically participate in the close-hook
        protocol without per-protocol overrides.
    */
    std::string owner_str;

    //! Proxy hostname (empty = direct connection, no proxy)
    std::string proxy_host;

    //! Proxy TCP port (only used when @ref proxy_host is non-empty)
    int proxy_port = 0;

    //! SSL certificate verification mode for connections created by this object
    int ssl_verify_mode = 0;  // SSL_VERIFY_NONE

    //! Accept all SSL certificates including self-signed
    bool accept_all_certs = false;

    //! Client certificate for mutual TLS (ref'd; nullptr = no cert)
    QoreSSLCertificate* client_cert = nullptr;

    //! Client private key for mutual TLS (ref'd; nullptr = no key)
    QoreSSLPrivateKey* client_key = nullptr;

    //! Builds the C++ pieces and submits to the controller.  Called from
    //! the constructor.
    DLLLOCAL int buildAndSubmit(ExceptionSink* xsink);
};

#endif // _QORE_INTERN_QOREHTTP1CLIENTCONNECTION_H

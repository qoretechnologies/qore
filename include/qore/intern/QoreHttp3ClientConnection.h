/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttp3ClientConnection.h

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

#ifndef _QORE_INTERN_QOREHTTP3CLIENTCONNECTION_H

#define _QORE_INTERN_QOREHTTP3CLIENTCONNECTION_H

#include <qore/HttpClientConnection.h>

#include <string>

class QoreSocketObject;
class Http3ClientPollOperationPriv;
class QoreSSLCertificate;
class QoreSSLPrivateKey;

//! HTTP/3 (QUIC) C++ client connection.
/** Wraps a @ref Http3ClientPollOperationPriv over a UDP socket with a
    @ref SocketQuicClientPollOperation inner op, submits to the global
    AsyncIoController on construction, and exposes @ref submitRequest for
    sync-over-async multiplexed request dispatch.

    Phase P5 of the HttpClientConnectionManager C++ port.  Uses ngtcp2 for
    QUIC transport and nghttp3 for HTTP/3 framing — TLS 1.3 is integrated
    into the QUIC handshake (no separate SSL upgrade phase).

    @par UDP vs TCP
    Unlike H1/H2 (TCP), this class creates a UDP socket bound to an
    ephemeral port via @c QoreSocketObject::bindINET.  The QUIC session
    drives the handshake and retransmission logic; the AsyncIoController
    polls the UDP fd for readability and calls continuePoll on the H3
    poll op when data arrives.

    @since %Qore 2.3
*/
class Http3ClientConnection : public HttpClientConnectionBase {
public:
    //! Creates a new HTTP/3 (QUIC) client connection.
    /** @param target_host target hostname
        @param target_port target UDP port
        @param max_concurrent_streams advisory stream cap (0 = unlimited)
        @param xsink exception sink
        @param mgr optional owning manager — registered before I/O submission
        @param ssl_verify_mode OpenSSL verify mode bitmask; default
            SSL_VERIFY_NONE — applied to the fresh H3 UDP socket before the
            QUIC handshake so `QuicSession::createClient` picks it up.
        @param ssl_accept_all_certs pass through to the socket (matches H1/H2)
        @param client_cert optional mTLS client certificate (ref'd by ctor)
        @param client_key optional mTLS client private key (ref'd by ctor)
    */
    DLLLOCAL Http3ClientConnection(const char* target_host, int target_port,
        int max_concurrent_streams, ExceptionSink* xsink,
        HttpClientConnectionManagerBase* mgr = nullptr,
        int ssl_verify_mode = 0,
        bool ssl_accept_all_certs = false,
        QoreSSLCertificate* client_cert = nullptr,
        QoreSSLPrivateKey* client_key = nullptr);

    DLLLOCAL virtual ~Http3ClientConnection();

    DLLLOCAL void setOwner(const char* owner) {
        if (owner) {
            owner_str = owner;
        }
    }

    // --- HttpClientConnectionBase overrides ---

    HttpClientProtocol getProtocol() const override {
        return HttpClientProtocol::H3;
    }

    int getMaxConcurrentStreams() const override {
        return max_concurrent_streams_;
    }

    DLLEXPORT int getActiveStreamCount() const override;

    DLLEXPORT QoreHashNode* submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) override;

    //! Submits a request with a caller-provided completion action.
    /** @since %Qore 2.3
    */
    DLLEXPORT int64_t submitRequestWithAction(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        AbstractAsyncAction* action, ExceptionSink* xsink) override;

    //! Submits a streaming request: response delivered incrementally via Channel.
    /** @since %Qore 2.3
    */
    DLLEXPORT int64_t submitRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink) override;

    //! Submits a request with streaming send (H3 DATA frames pushed incrementally)
    /** @since %Qore 2.3
    */
    DLLEXPORT QoreHashNode* submitRequestStreamingSend(const char* method, const char* path,
        const QoreHashNode* headers, bool streaming_recv,
        QoreChannel*& channel_out, ExceptionSink* xsink) override;

    //! Push body data for a streaming send request (H3 DATA frame via QUIC)
    /** @since %Qore 2.3
    */
    DLLEXPORT void pushSendData(const void* data, size_t len, ExceptionSink* xsink) override;

    //! Set HTTP trailers for a streaming send request (H3 TRAILERS)
    /** @since %Qore 2.3
    */
    DLLEXPORT void setTrailers(const QoreHashNode* trailers, ExceptionSink* xsink) override;

    DLLEXPORT void closeConnection(ExceptionSink* xsink) override;

protected:
    DLLLOCAL QoreHashNode* getReferencedErrorInfo() override;

private:
    //! @name QoreObject refs — standalone C++ lifecycle, NOT DGC-visible
    /** @c sock_obj and @c poll_op_obj hold strong refs on the socket /
        poll-op QoreObjects.  They are raw, not stored in any qclass's
        @c internal_members slot — because @c Http3ClientConnection is
        itself a pure C++ class (not wrapped in a QoreObject) and so is
        not a participant in DGC cycle collection.

        Lifetime is managed by the @ref Http3ClientConnection destructor,
        which derefs both when the connection is released.  See
        @c design/dgc.md — Pattern A (internal_members) does not apply
        here because there is no enclosing QoreObject whose data/cdmap
        the scanner could walk.  If a future refactor wraps this class
        in a @c qclass, both refs should move into @c internal_members
        at that point.

        @{
    */
    QoreObject* sock_obj = nullptr;
    QoreSocketObject* sock_priv = nullptr;
    QoreObject* poll_op_obj = nullptr;
    Http3ClientPollOperationPriv* poll_op_priv = nullptr;
    //! @}
    bool submitted_to_controller = false;
    int max_concurrent_streams_ = 0;
    std::string owner_str;

    //! SSL configuration applied to the fresh H3 UDP socket in buildAndSubmit
    /** Propagated from HttpClientConnectionManagerBase::ssl_config so the
        QUIC handshake (which reads the socket's ssl_verify_mode /
        accept-all-certs / cert / pk) honors the user's HTTPClient settings.
        H3 creates a brand-new UDP socket rather than reusing the HTTPClient
        msock; without this plumbing the handshake always ran with
        SSL_VERIFY_NONE.
    */
    int ssl_verify_mode_ = 0;
    bool ssl_accept_all_certs_ = false;
    QoreSSLCertificate* client_cert_ = nullptr;
    QoreSSLPrivateKey* client_key_ = nullptr;

    //! Stream ID for the active streaming send request (-1 = none)
    int64_t streaming_send_stream_id = -1;

    DLLLOCAL int buildAndSubmit(ExceptionSink* xsink);
};

#endif // _QORE_INTERN_QOREHTTP3CLIENTCONNECTION_H

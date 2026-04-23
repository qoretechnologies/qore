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

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

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

    //! Handshake-phase hook invoked by an inner Http3ClientPollOperationPriv
    //! when its QUIC+TLS handshake reaches the READING state.
    /** Happy-eyeballs coordination: the first attempt to succeed wins.  The
        winning attempt is committed as the primary (members @c sock_priv,
        @c poll_op_priv, etc. are populated from its tuple), losing attempts
        are aborted, and the base class transitions to @c READY.
        Subsequent calls (from already-canceled attempts) are no-ops.
        @param inner the poll op priv whose handshake just completed
        @since %Qore 2.3
    */
    DLLLOCAL void onInnerHandshakeReady(Http3ClientPollOperationPriv* inner);

    //! Handshake-phase hook invoked by an inner Http3ClientPollOperationPriv
    //! when its QUIC+TLS handshake fails.
    /** Happy-eyeballs coordination: a single attempt failing does not
        close the connection if other attempts remain.  Only when all
        attempts have failed does the base class transition to @c CLOSED
        with the most recent error details.
        @param inner the poll op priv whose handshake failed
        @param err short error code (e.g. "QUIC-HANDSHAKE-TIMEOUT")
        @param desc human-readable description
        @since %Qore 2.3
    */
    DLLLOCAL void onInnerHandshakeFailed(Http3ClientPollOperationPriv* inner,
        const char* err, const char* desc);

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

        @par Concurrency contract

        These members are populated at two distinct points:
          1. Initial publication in @ref buildAndSubmit on the app thread,
             BEFORE the constructor returns.  At this time the primary
             (first) attempt's tuple is published.
          2. Reassignment in @ref onInnerHandshakeReady on the I/O thread
             when a different attempt wins the happy-eyeballs race.  The
             reassignment is performed under @c attempts_mu_.

        Readers (submitRequest, pushSendData, etc.) gate their access
        through the base-class @c isReady() state transition: none of
        them dereference these pointers unless @c isReady() returns true.
        @ref onConnectionReady is called AFTER the member reassignment,
        and @c onConnectionReady publishes the state change with release
        ordering.  Readers see @c isReady()==true with acquire ordering,
        producing the happens-before edge that makes the pointer write
        visible without per-member atomics.

        The @ref closeConnection path nulls these members under
        @c attempts_mu_ (racing branch) or after disarming the poll op
        (committed branch).  A Qore program calling closeConnection (or
        `delete`) concurrently with another method on the same connection
        violates the Qore object-lifetime contract — the outer QoreObject
        ref held by the calling thread must outlive the method call.

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

    //! One concurrent QUIC handshake attempt during happy-eyeballs racing
    /** Each attempt owns a dedicated UDP socket + poll op.  Raw QoreObject
        pointers are held for deref in @c clearAttempt (on abort or when the
        attempt loses the race).  All fields are populated synchronously at
        @c buildAttempt time; after @c onInnerHandshakeReady commits the
        winner, @c attempts_ is cleared and the winner's tuple is hoisted
        into the corresponding @c sock_obj / @c sock_priv / @c poll_op_obj /
        @c poll_op_priv members on the enclosing connection.
    */
    struct Attempt {
        QoreObject* sock_obj = nullptr;
        QoreSocketObject* sock_priv = nullptr;
        QoreObject* poll_op_obj = nullptr;
        Http3ClientPollOperationPriv* poll_op_priv = nullptr;
        int family = 0;              // AF_INET / AF_INET6 for diagnostics
        bool submitted = false;      // true once passed to AsyncIoController
        bool finished = false;       // true once handshake outcome known

        //! Default destructor is a no-op on the raw pointers: ownership
        //! is transferred either to the enclosing connection's members
        //! (winner) or released via @ref Http3ClientConnection::clearAttempt
        //! (loser / teardown).  In debug builds, assert that both
        //! QoreObject pointers were zeroed before destruction to catch
        //! future ownership-discipline regressions.
        ~Attempt() {
            assert(sock_obj == nullptr);
            assert(poll_op_obj == nullptr);
        }

        // Non-copyable — the raw pointers have a single owner at any
        // time (the Attempt itself during racing, the connection members
        // after winner commit).
        Attempt() = default;
        Attempt(const Attempt&) = delete;
        Attempt& operator=(const Attempt&) = delete;
    };

    //! Abort + deref + zero an attempt (assumes @c attempts_mu_ held).
    DLLLOCAL void clearAttempt(Attempt& a, ExceptionSink* xsink);

    //! Build one QUIC handshake attempt for a specific address family and
    //! optional stagger delay.  Creates a fresh UDP socket, applies SSL
    //! settings, wraps in a SocketQuicClientPollOperation and
    //! Http3ClientPollOperationPriv, and appends to @c attempts_.  The
    //! attempt is NOT yet submitted to the AsyncIoController — callers
    //! drive submission after the full list is built so lookups via
    //! @c poll_op_priv within submission callbacks see a consistent view.
    //! @param family AF_INET or AF_INET6 target family
    //! @param not_before_ns_abs absolute ngtcp2 timestamp (0 = immediate)
    //! @param xsink exception sink
    //! @return index into @c attempts_ on success, -1 on error
    DLLLOCAL int buildAttempt(int family, int64_t not_before_ns_abs,
        ExceptionSink* xsink);

    //! Submit one already-built attempt to the AsyncIoController.
    //! @return 0 on success, -1 on error (sets @c submitted on success).
    DLLLOCAL int submitAttempt(Attempt& a, ExceptionSink* xsink);

    //! @name Happy-eyeballs racing state
    /** During handshake, up to N attempts (one per interleaved v6/v4
        candidate) race concurrently.  The first to report
        @c onInnerHandshakeReady wins; the others are aborted.
        @{
    */
    std::vector<std::unique_ptr<Attempt>> attempts_;
    std::mutex attempts_mu_;
    //! Index of the winning attempt in @c attempts_; -1 while racing.
    /** Written under @c attempts_mu_; read under the same or during
        committed-state method calls where the connection is already
        @c READY and @c attempts_ has been drained.
    */
    int winner_idx_ = -1;
    //! Error details from the most recently failed attempt (for diagnostics
    //! when all attempts fail).  Protected by @c attempts_mu_.
    std::string last_err_, last_desc_;
    //! @}

    DLLLOCAL int buildAndSubmit(ExceptionSink* xsink);
};

#endif // _QORE_INTERN_QOREHTTP3CLIENTCONNECTION_H

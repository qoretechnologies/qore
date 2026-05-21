/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HttpClientConnectionManager.h

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

#ifndef _QORE_HTTPCLIENTCONNECTIONMANAGER_H

#define _QORE_HTTPCLIENTCONNECTIONMANAGER_H

#include <qore/AbstractPrivateData.h>
#include <qore/HttpClientConnection.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vector>

class ExceptionSink;
class QoreHashNode;
class QoreSSLCertificate;
class QoreSSLPrivateKey;

//! C++ HTTP client connection manager — Phase P3 of the porting plan.
/** Implements the connection pool, per-key creation serialization, proxy
    parsing, and lifecycle management for C++-side HTTP client connections.
    H1-only for Phase P3; H2 and H3 protocol branches stub out and raise
    @c PROTOCOL-NOT-IMPLEMENTED until Phases P4/P5.

    See @c design/http-client-manager-cpp-port.md for the full design,
    including the basics-vs-features split (retry, OAuth2, cookies,
    Alt-Svc, and protocol cache stay in the Qore layer; pool/proxy/
    lifecycle live here).

    @par Lifetime contract
    The destructor calls @ref closeAll, which:
    1. Drains the pool into a local vector under @ref pool_lock
    2. Releases @ref pool_lock
    3. For each drained connection: calls @c setManager(nullptr) on the
       connection (per the per-connection @c onclose_lock), then closes
       and derefs it.

    Step 3 MUST happen before the manager frees its own state (the base
    class destructor running) — otherwise the async I/O thread could fire
    @c onClosedHook on a half-destroyed manager.

    @par Lock ordering
    See section 7.2 of the design doc:
    @code
    Http1ClientConnection::onclose_lock  (per connection)
        ↓
    pool_lock                            (this class — shared_mutex)
        ↓
    AsyncIoControllerPriv::m             (controller)
    @endcode

    @par Subclassing
    The class is designed to be subclassed by the Qore-level
    @c HttpClientIo::HttpClientConnectionManager in Phase P6.  Subclasses
    layer retry, Alt-Svc, cookie jar, and protocol cache on top of the
    pool/lifecycle primitives provided here.  Member visibility is
    @c protected to support that.

    @since %Qore 2.3
*/
class HttpClientConnectionManagerBase : public AbstractPrivateData {
public:
    //! Configuration options for the manager.
    struct Options {
        //! Protocol selection: P3 only honors H1; H2/H3 raise on acquireConnection
        HttpClientProtocol protocol = HttpClientProtocol::H1;

        //! Per-host connection cap; 0 = unlimited.
        int max_connections_per_host = 0;

        //! Per-connection concurrent stream cap; 0 = unlimited (advisory for H1)
        int max_streams_per_connection = 0;

        //! Connection establishment timeout in milliseconds.
        int connect_timeout_ms = 30000;

        //! Per-request timeout in milliseconds (used by @ref request).
        int request_timeout_ms = 60000;

        //! Idle timeout in milliseconds — passed through to the connection's
        //! proactive idle-close path.  Default 60000 matches nginx's
        //! upstream @c keepalive_timeout default.
        int idle_timeout_ms = 60000;

        //! Born-at TTL for pooled connections in milliseconds.
        /** Curl analog: @c CURLOPT_MAXAGE_CONN.  Bounds the wall-clock age
            of a pooled connection.  When set, a connection whose age (now −
            @ref HttpClientConnectionBase::getCreatedUs) reaches this value
            is evicted at the next pool checkout — closed and dropped from
            the pool via the close-after-lock-drop machinery in
            @ref evictDeadLocked.

            Skipped for connections with active streams (an in-flight
            request is not a candidate for max-age eviction; the next
            checkout after the stream completes will catch it).  Pure
            stored-state check — no syscall, no I/O thread coupling.

            Default 0 = disabled.
        */
        int max_age_ms = 0;

        //! TCP_USER_TIMEOUT in milliseconds — kernel-level safety net for
        //! TCP-layer death (peer reboot, NIC failure, network partition,
        //! NAT rebind), applied via setsockopt on TCP sockets created by
        //! this manager.  Bounds how long unacknowledged TCP data may sit
        //! before the kernel reports the connection dead with ETIMEDOUT,
        //! instead of hanging in the kernel's full TCP retransmit window
        //! (~15 minutes default on Linux).
        //!
        //! NOT a half-open HTTP detector — if the peer's TCP stack ack's
        //! bytes but the HTTP server stops generating responses, this
        //! never fires.  @ref request_timeout_ms is the application-level
        //! backstop for that case.
        //!
        //! Default 30000 = 30s.  0 disables (kernel default).  No effect
        //! on platforms without TCP_USER_TIMEOUT (BSD/macOS/Windows) or
        //! on UDP/UNIX sockets.
        int tcp_user_timeout_ms = 30000;

        //! Optional proxy URL (e.g., "http://proxy.example.com:8080");
        //! empty string means no proxy.  Parsed in the constructor.
        std::string proxy_url;

        //! SSL certificate verification mode (SSL_VERIFY_NONE or SSL_VERIFY_PEER).
        //! Default is SSL_VERIFY_NONE to match the legacy HTTPClient behavior.
        int ssl_verify_mode = 0;  // SSL_VERIFY_NONE

        //! If true, accept all SSL certificates including self-signed.
        bool accept_all_certs = false;

        //! Client certificate for mutual TLS (ref'd; nullptr = no client cert).
        //! The manager holds a reference while it's alive and passes it to new
        //! connections.
        QoreSSLCertificate* client_cert = nullptr;

        //! Client private key for mutual TLS (ref'd; nullptr = no key).
        QoreSSLPrivateKey* client_key = nullptr;
    };

    //! Creates a new manager with the given options.
    /** @param opts configuration options
        @param xsink set on construction failure (e.g., invalid proxy URL,
            invalid options)
    */
    DLLEXPORT HttpClientConnectionManagerBase(const Options& opts, ExceptionSink* xsink);

    DLLEXPORT virtual ~HttpClientConnectionManagerBase();

    //! Acquires a connection for the given URL.
    /** May reuse a pooled connection or create a new one.  Reservation
        of a stream slot is atomic (no TOCTOU race with concurrent
        acquires).  The returned pointer is borrowed — the pool retains
        ownership.  Callers MUST call @ref releaseConnection when done
        with the stream slot.

        @param scheme URL scheme ("http" or "https"); used to determine
            SSL requirement
        @param host target hostname (used for the Host header and pool key)
        @param port target TCP port (used for the pool key)
        @param xsink exception sink

        @return a borrowed connection pointer (do NOT @c deref); @c nullptr
            on error (@a xsink set)

        @throw HTTPCLIENT-CAPACITY-ERROR if all connections to @a host:@a port
            are at the per-connection stream limit
        @throw HTTPCLIENT-CONNECT-ERROR if a new connection cannot be
            established
        @throw HTTPCLIENT-SHUTDOWN if the manager is shutting down
        @throw PROTOCOL-NOT-IMPLEMENTED if @c opts.protocol is H2 or H3
            (P3 limitation)
    */
    DLLEXPORT virtual HttpClientConnectionBase* acquireConnection(
        const char* scheme, const char* host, int port,
        ExceptionSink* xsink);

    //! Acquires a connection without blocking for it to become READY.
    /** Like @ref acquireConnection but, on a pool miss, returns the newly
        created connection while it is still in CONNECTING state.  The
        caller must wait asynchronously for the connection to become
        READY (typically via
        @ref AbstractHttpPollConnectionPriv::registerReadyNotifier) before
        submitting a request.

        On a pool hit, behaves identically to @ref acquireConnection — the
        returned connection is already READY.

        @param scheme URL scheme ("http" or "https")
        @param host target hostname
        @param port target TCP port
        @param xsink exception sink

        @return a borrowed connection pointer (do NOT @c deref); the
            connection may be in CONNECTING or READY state.  @c nullptr
            on error (@a xsink set).

        @since %Qore 2.3
    */
    DLLEXPORT virtual HttpClientConnectionBase* acquireConnectionAsync(
        const char* scheme, const char* host, int port,
        ExceptionSink* xsink);

    //! Releases a stream slot back to the pool.
    /** Decrements the connection's pending stream count.  Does NOT close
        the connection — the connection stays in the pool for reuse.

        @param conn the connection previously returned by @ref acquireConnection
    */
    DLLEXPORT virtual void releaseConnection(HttpClientConnectionBase* conn);

    //! Force-close a connection and remove it from the pool.
    /** Used when a connection is known to be unusable (e.g., the caller
        observed a fatal error).  Closes the underlying poll op and the
        controller submission, then removes from the pool and derefs.

        @param conn the connection to close and evict
        @param xsink exception sink
    */
    DLLEXPORT virtual void closeAndEvict(HttpClientConnectionBase* conn,
        ExceptionSink* xsink);

    //! Closes all pooled connections and clears the pool.
    /** Called by the destructor; can be called explicitly to drain the
        manager without destroying it.  After this call, @ref acquireConnection
        creates fresh connections (the pool is empty, not poisoned).

        Implements the lifetime contract from section 7.3 of the design
        doc: drains the pool into a local vector under the pool lock,
        releases the lock, then walks each connection, nulls its manager
        back-pointer (via @c setManager(nullptr)), closes it, and derefs.
    */
    DLLEXPORT virtual void closeAll(ExceptionSink* xsink);

    //! Returns the options used to create this manager.
    DLLLOCAL const Options& getOptions() const {
        return opts_;
    }

    //! Returns true if any pooled connection uses the given protocol.
    /** Used by @c isHttp2Active() to detect whether a NEGOTIATE manager
        has established at least one H2 connection.
        @param proto the protocol to check for
        @return true if at least one pooled connection reports @a proto
        @since %Qore 2.3
    */
    DLLEXPORT bool hasProtocolInPool(HttpClientProtocol proto) const;

    //! Returns true if a connection of @a proto was ever added to the pool.
    /** Sticky across pool eviction.  `hasProtocolInPool()` reports only
        live pool contents, so a short-lived H2 response where the server
        closes the connection after sending the body (e.g. the
        @c Http2.qtest poll test's server does @c client.close() right
        after @c h2op.startSendResponse) would briefly see H2 in the pool
        and then — after the close arrives on the I/O thread and
        @c onConnectionClosed evicts the connection — not see it.  This
        method returns true for the entire lifetime of the manager if an
        H2 (or H3) connection was ever pooled, matching what
        @c isHttp2Active() / @c isHttp3Active() semantically want: "did
        this HTTPClient ever speak HTTP/2 over this manager?"
        @param proto the protocol to check for
        @return true if @a proto has been observed at least once
        @since %Qore 2.3
    */
    DLLEXPORT bool hasEverObservedProtocol(HttpClientProtocol proto) const;

    //! Returns the total number of pooled connections across all keys.
    DLLEXPORT int getPoolSize() const;

    //! Returns the number of pooled connections across all keys whose
    //! @c isClosed() returns false.
    /** Unlike @ref getPoolSize, this excludes connections that have already
        been closed (typically by the remote peer) but have not yet been
        evicted by the asynchronous @c onConnectionClosed hook or the next
        @c acquireConnection-time @c evictDeadLocked sweep.

        @since %Qore 2.3
    */
    DLLEXPORT int getOpenPoolSize() const;

    //! Returns the number of connections currently pooled for @a host : @a port.
    DLLEXPORT int getConnectionCount(const char* host, int port) const;

    //! Convenience: acquires a connection, submits a request, awaits the
    //! Future synchronously, releases the connection, and returns the
    //! response hash.
    /** This is the API that Phase P10 uses to convert
        @c QoreHttpClientObject::send_internal to delegate to the C++
        manager.  For now, it is exercised by Phase P3's unit tests.

        @param method HTTP method
        @param scheme URL scheme ("http" or "https")
        @param host target hostname
        @param port target port
        @param path request path
        @param headers optional request headers (may be nullptr)
        @param body optional request body
        @param body_len request body length in bytes
        @param timeout_ms per-request timeout in milliseconds; 0 or negative
            uses the default from @c Options::request_timeout_ms
        @param xsink exception sink

        @return the response hash (caller owns), or @c nullptr on error

        @throw HTTPCLIENT-CONNECT-ERROR / HTTPCLIENT-REQUEST-ERROR /
            FUTURE-TIMEOUT — see @ref acquireConnection /
            @ref HttpClientConnectionBase::submitRequest
    */
    DLLEXPORT virtual QoreHashNode* request(const char* method,
        const char* scheme, const char* host, int port, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        int timeout_ms, ExceptionSink* xsink);

    //! Submits a streaming request and returns a Channel for reading
    /** Like @ref request but uses Channel-based incremental delivery
        for the response.  The caller reads headers, body chunks, and
        end_stream sentinels from the returned channel.

        @param method HTTP method
        @param scheme URL scheme
        @param host target hostname
        @param port target port
        @param path request path
        @param headers optional request headers
        @param body optional complete request body
        @param body_len body length in bytes
        @param xsink exception sink

        @return hash with "stream_id" and "channel" (QoreChannel*, ref'd);
            nullptr on error.  Caller must deref channel when done.

        @since %Qore 2.3
    */
    //! @param channel_out receives a ref'd QoreChannel* for reading
    //!     streaming response data.  Caller must deref when done.
    //! @return stream ID on success, -1 on error
    DLLEXPORT int64_t requestStreaming(const char* method,
        const char* scheme, const char* host, int port, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink);

    // --- Hook from connection close (called by Http1ClientConnection::onClosedHook) ---

    //! Called by a connection's @ref AbstractHttpPollConnectionPriv::onClosedHook
    //! when its state first transitions to CLOSED.
    /** Removes the connection from the pool.  Does NOT @c deref the
        connection — that is the caller's responsibility (or the
        connection's destructor when its last ref drops).

        Called from any thread (typically the async I/O thread when an
        idle timeout fires or the peer closes).  Bounded latency: takes
        @ref pool_lock briefly, removes the connection from its pool key,
        signals @ref create_cond_, returns.

        @param conn the connection that has been closed
    */
    DLLEXPORT virtual void onConnectionClosed(HttpClientConnectionBase* conn);

protected:
    //! Parsed proxy info; @c nullptr if no proxy configured.
    struct ProxyInfo {
        std::string host;
        int port;
        bool ssl;
    };

    Options opts_;
    std::unique_ptr<ProxyInfo> proxy_info_;

    //! Pool: pool-key → list of connections.
    /** Key format is @c "host:port" for direct connections, or
        @c "proxy_host:proxy_port|target_host:target_port" for proxied
        connections (so the same target reached through different proxies
        gets distinct pool entries).

        Protected by @ref pool_lock_.
    */
    std::unordered_map<std::string, std::vector<HttpClientConnectionBase*>> pool_;

    //! RWLock-equivalent for the pool.  Reads (acquireConnection scan
    //! path) take a shared lock; writes (createConnection success path,
    //! eviction, closeAll) take a unique lock.
    mutable std::shared_mutex pool_lock_;

    //! Set when @ref closeAll has been called; subsequent
    //! @ref acquireConnection raises @c HTTPCLIENT-SHUTDOWN.
    bool shutdown_ = false;

    //! Per-key creation serialization (mirrors @c create_mutex /
    //! @c create_cond / @c creating in the existing Qore manager).
    /** Without this, N threads racing to create a connection on a cold
        pool key would each create a connection — N handshakes when 1
        suffices.  The first thread sets @ref creating_, others wait on
        @ref create_cond_.  Wait is bounded by @c connect_timeout_ms;
        on timeout, the waiter takes over creation.
    */
    std::mutex create_lock_;
    std::condition_variable create_cond_;
    std::unordered_set<std::string> creating_;

    //! Connections removed from the pool by onConnectionClosed but not yet
    //! deref'd.  The deref is deferred because onConnectionClosed runs on
    //! the I/O thread; a synchronous deref that triggers destruction would
    //! call closeConnection → controller cancel from inside continuePoll,
    //! causing a use-after-free / deadlock.  Drained by processDeferredDeref().
    //! Protected by pool_lock_ (write).
    std::vector<HttpClientConnectionBase*> deferred_deref_;

    //! Sticky bitmask of protocols ever added to the pool.
    /** Bit N corresponds to the underlying_type value N of
        @c HttpClientProtocol.  Set when a connection is added to the
        pool; never cleared.  Used by @ref hasEverObservedProtocol().
        Atomic so the isHttp2Active() / isHttp3Active() read paths can
        check it without taking @c pool_lock_. */
    std::atomic<unsigned> observed_protocols_{0};

    //! Computes the pool key for the given target (with proxy info baked in).
    DLLLOCAL std::string poolKey(const char* host, int port) const;

    //! Drains deferred connection derefs from onConnectionClosed.
    /** Must be called from an app thread, not the I/O thread.
    */
    DLLLOCAL void processDeferredDeref(ExceptionSink* xsink);

    //! Creates a new connection (must be called outside @ref pool_lock_
    //! since the connection constructor blocks on the controller submit).
    /** @param key the pool key for the connection
        @param host target host
        @param port target port
        @param ssl_required True for HTTPS
        @param wait_for_ready if @c true (default), block until the new
            connection transitions out of CONNECTING (the historical
            behavior).  If @c false, return as soon as the connection is
            constructed and submitted to the I/O controller — the caller
            must wait asynchronously for the READY transition.
        @param xsink exception sink

        @return a new connection (caller owns one ref); @c nullptr on error

        @since %Qore 2.3 @c wait_for_ready parameter
    */
    DLLLOCAL virtual HttpClientConnectionBase* createConnection(
        const std::string& key, const char* host, int port,
        bool ssl_required, ExceptionSink* xsink, bool wait_for_ready = true);

private:
    HttpClientConnectionManagerBase(const HttpClientConnectionManagerBase&) = delete;
    HttpClientConnectionManagerBase& operator=(const HttpClientConnectionManagerBase&) = delete;

    //! Internal: shared implementation for @ref acquireConnection and
    //! @ref acquireConnectionAsync.  When @a wait_for_ready is @c false,
    //! newly-created connections are returned in CONNECTING state.
    DLLLOCAL HttpClientConnectionBase* acquireConnectionImpl(
        const char* scheme, const char* host, int port,
        bool wait_for_ready, ExceptionSink* xsink);

    //! Internal: scans the pool for a reusable connection (caller must
    //! hold a shared or unique lock).  Returns @c nullptr if no live
    //! connection has capacity.  Reservation is atomic via
    //! @c tryReserveStream — see open question 7.6 in the design doc.
    DLLLOCAL HttpClientConnectionBase* findReusableLocked(const std::string& key);

    //! Internal: removes closed and (optionally) max-age-expired connections
    //! from @c pool_[key] (caller must hold the unique lock).
    /** Closed connections are simply deref'd (their I/O thread already
        closed them).  Max-age-expired connections are not yet closed —
        they are pushed into @a out_to_close instead, and the caller MUST
        invoke @ref closeAndDerefAfterLockDrop on each entry after
        releasing @c pool_lock_ to actually trigger the close (else they
        linger on the I/O thread's ref).

        @param key the pool key
        @param out_to_close optional out-vector receiving max-age-expired
            connections that need close-after-lock-drop; pass @c nullptr
            to skip max-age handling
    */
    DLLLOCAL void evictDeadLocked(const std::string& key,
        std::vector<HttpClientConnectionBase*>* out_to_close = nullptr);

    //! Closes and derefs a connection that was identified for eviction
    //! while a pool-lock was held.  Caller must NOT hold @c pool_lock_.
    /** Mirrors the close+deref pattern used by the shutdown branch of
        @ref acquireConnectionImpl (line ~360).
    */
    DLLLOCAL void closeAndDerefAfterLockDrop(HttpClientConnectionBase* conn);
};

#endif // _QORE_HTTPCLIENTCONNECTIONMANAGER_H

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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ExceptionSink;
class QoreHashNode;

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

        //! Optional proxy URL (e.g., "http://proxy.example.com:8080");
        //! empty string means no proxy.  Parsed in the constructor.
        std::string proxy_url;
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

    //! Returns the total number of pooled connections across all keys.
    DLLEXPORT int getPoolSize() const;

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

    //! Computes the pool key for the given target (with proxy info baked in).
    DLLLOCAL std::string poolKey(const char* host, int port) const;

    //! Creates a new connection (must be called outside @ref pool_lock_
    //! since the connection constructor blocks on the controller submit).
    /** @param key the pool key for the connection
        @param host target host
        @param port target port
        @param ssl_required True for HTTPS
        @param xsink exception sink

        @return a new connection (caller owns one ref); @c nullptr on error
    */
    DLLLOCAL virtual HttpClientConnectionBase* createConnection(
        const std::string& key, const char* host, int port,
        bool ssl_required, ExceptionSink* xsink);

private:
    HttpClientConnectionManagerBase(const HttpClientConnectionManagerBase&) = delete;
    HttpClientConnectionManagerBase& operator=(const HttpClientConnectionManagerBase&) = delete;

    //! Internal: scans the pool for a reusable connection (caller must
    //! hold a shared or unique lock).  Returns @c nullptr if no live
    //! connection has capacity.  Reservation is atomic via
    //! @c tryReserveStream — see open question 7.6 in the design doc.
    DLLLOCAL HttpClientConnectionBase* findReusableLocked(const std::string& key);

    //! Internal: removes closed connections from @c pool_[key] (caller
    //! must hold the unique lock).
    DLLLOCAL void evictDeadLocked(const std::string& key);
};

#endif // _QORE_HTTPCLIENTCONNECTIONMANAGER_H

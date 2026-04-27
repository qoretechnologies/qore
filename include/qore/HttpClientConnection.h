/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HttpClientConnection.h

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

#ifndef _QORE_HTTPCLIENTCONNECTION_H

#define _QORE_HTTPCLIENTCONNECTION_H

#include <qore/AbstractHttpPollConnection.h>
#include <qore/QoreThreadLock.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

// Forward declarations
class QoreHashNode;
class QoreObject;
class ExceptionSink;
class QoreChannel;
class HttpClientConnectionManagerBase;
class AbstractAsyncAction;

//! HTTP client protocol version
/** @since %Qore 2.3
*/
enum class HttpClientProtocol {
    H1,         //!< HTTP/1.1 over TCP or TCP+TLS
    H2,         //!< HTTP/2 over TCP (h2c direct) or TCP+TLS (ALPN "h2")
    H3,         //!< HTTP/3 over QUIC
    NEGOTIATE   //!< TCP+TLS with ALPN offer {"h2", "http/1.1"};
                //!< manager decides H1 vs H2 per connection
};

//! Abstract base for C++ HTTP client connections
/** This class wraps a protocol-specific poll operation (H1, H2, or H3) and
    a socket, submits the poll op to the global AsyncIoController, and exposes
    a small C++ API for synchronous submit-and-await semantics on top.

    This is the first phase (Phase P2) of porting the basics of
    @c HttpClientConnectionManager from Qore to C++.  Later phases add the
    pool, proxy handling, and protocol cache around this class and migrate
    @c QoreHttpClientObject (sync HTTPClient) to delegate here.

    The class inherits from @ref AbstractHttpPollConnectionPriv, which
    provides the connection state machine (CONNECTING → READY → CLOSED)
    used by the protocol-specific poll operation to signal readiness.

    @par Ownership
    The connection is ref-counted (via @ref AbstractPrivateData).  Callers
    typically hold a single strong ref and call @ref closeConnection before
    dropping the ref.  Dropping the last ref without calling
    @ref closeConnection triggers an implicit close in the destructor.

    @since %Qore 2.3
*/
class HttpClientConnectionBase : public AbstractHttpPollConnectionPriv {
public:
    //! Returns the protocol (H1, H2, or H3).
    /** Default returns H1.  Subclasses override.
    */
    DLLEXPORT virtual HttpClientProtocol getProtocol() const {
        return HttpClientProtocol::H1;
    }

    //! Returns the number of active streams on this connection.
    /** Default returns 0.  Subclasses override.
    */
    DLLEXPORT virtual int getActiveStreamCount() const {
        return 0;
    }

    //! Hook for the connection manager to push the configured idle timeout
    //! down to the protocol's poll-op proactive-close machinery.
    /** Default no-op.  H1 / H2 subclasses override to call
        @c setIdleTimeout on their poll-op priv.  H3 (QUIC) skips —
        ngtcp2's own idle / keepalive timers cover liveness.

        @param timeout_us idle timeout in microseconds; <=0 disables
    */
    DLLEXPORT virtual void setIdleTimeoutHook(int64_t timeout_us) {
        (void)timeout_us;
    }

    //! Returns the maximum concurrent streams this connection can host.
    /** Default 1 (HTTP/1.1 semantics).  H2 connections override to return
        their negotiated MAX_CONCURRENT_STREAMS setting (or the cap from
        the manager's options).  H3 connections similarly.

        @return the per-connection concurrent stream cap, or 0 for unlimited
    */
    DLLEXPORT virtual int getMaxConcurrentStreams() const {
        return 1;
    }

    //! Atomically checks capacity and reserves a stream slot.
    /** Returns @c true if a slot was reserved, @c false if the connection
        is at capacity.  Callers MUST call @ref releaseStreamReservation
        when the stream is submitted (the reservation converts to an
        active slot inside the poll op) or abandoned.

        Compares @ref getActiveStreamCount + @ref pending_stream_count
        against @ref getMaxConcurrentStreams under @ref reserve_lock —
        no TOCTOU race with concurrent reservers.

        @return @c true on success, @c false at capacity

        @since %Qore 2.3
    */
    DLLEXPORT bool tryReserveStream();

    //! Releases a previously-reserved stream slot.
    /** Decrements @ref pending_stream_count.  No-op if no slot is
        currently reserved (e.g., double-release).

        @since %Qore 2.3
    */
    DLLEXPORT void releaseStreamReservation();

    //! Returns the current pending (reserved-but-not-yet-submitted) count.
    DLLEXPORT int getPendingStreamCount() const;

    //! Registers (or clears) the owning manager back-pointer for close
    //! notifications.
    /** When the connection's state transitions to CLOSED for the first
        time, @ref AbstractHttpPollConnectionPriv::onClosedHook fires; if
        a manager has been registered via this setter, the hook invokes
        @c manager->onConnectionClosed(this) so the manager can evict the
        connection from its pool promptly.

        Lock ordering: this method takes @c onclose_lock briefly to set
        the back-pointer.  See section 7.2 of
        @c design/http-client-manager-cpp-port.md for the full ordering
        rules.

        @param mgr the manager to notify on close, or @c nullptr to clear

        @note Callers MUST clear the back-pointer (via
        @c setManager(nullptr)) before destroying the manager — otherwise
        the I/O thread could fire @ref AbstractHttpPollConnectionPriv::onClosedHook
        on a manager that is mid-destruction (UAF).

        @since %Qore 2.3
    */
    DLLEXPORT void setManager(HttpClientConnectionManagerBase* mgr);

    //! @ref AbstractHttpPollConnectionPriv hook — invoked exactly once on
    //! the first close transition (from any thread).  Forwards to the
    //! registered manager (if any).
    /** Subclasses generally do NOT need to override this — the base
        class implementation handles dispatch via the @ref setManager
        back-pointer.  Override only when additional close-time work is
        needed (e.g., a protocol-specific notification beyond the
        manager dispatch).

        @since %Qore 2.3
    */
    DLLEXPORT void onClosedHook() override;

    //! Submit a request; returns a hash with @c stream_id (int) and
    //! @c future (Future) keys.
    /** The returned Future resolves with the response hash when the I/O
        thread completes the request, or rejects with a protocol error
        if the request fails.  The protocol-specific @c PromiseAction holds
        its own ref on the underlying @c QorePromise so the Promise stays
        alive until the action runs — callers therefore only need to keep
        the Future ref.

        @param method HTTP method (e.g. "GET", "POST")
        @param path request path (e.g. "/api/users")
        @param headers optional request headers (may be nullptr)
        @param body optional request body (may be nullptr if @a body_len is 0)
        @param body_len request body length in bytes
        @param xsink exception sink

        @return a newly-allocated hash with the keys described above
            (caller owns); @c nullptr on error (@a xsink is set)

        @throw HTTPCLIENT-STATE-ERROR if the connection is not READY
            (call @ref waitForReadyOrError first) or has been closed
        @throw HTTP1-ERROR (or protocol equivalent) if the request body
            is of an unsupported type
    */
    DLLEXPORT virtual QoreHashNode* submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink);

    //! Submits a streaming request with Channel-based response delivery
    /** Like @ref submitRequest but uses a @ref QoreChannel for incremental
        response delivery (headers, body chunks, end_stream sentinel).

        @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers optional request headers
        @param body optional complete request body
        @param body_len body length in bytes
        @param channel_out receives a ref'd QoreChannel* on success
        @param xsink exception sink
        @return stream ID on success; -1 on error

        @since %Qore 2.3
    */
    DLLEXPORT virtual int64_t submitRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink);

    //! Submits a request with a caller-provided async completion action.
    /** Like @ref submitRequest but uses the caller's
        @ref AbstractAsyncAction instead of creating a PromiseAction
        internally.  Used by the conn_mgr poll operation to install a
        @c PromiseNotifierAction that signals an @c EventNotifier when
        the response is ready.

        Default implementation raises @c HTTPCLIENT-NOT-IMPLEMENTED;
        H1/H2 subclasses override to dispatch through their
        protocol-specific poll operation.  H3 is not yet supported.

        @param method HTTP method
        @param path request path
        @param headers optional request headers
        @param body optional request body (may be nullptr)
        @param body_len body length
        @param action the action to use — ownership transferred on
            success, dereffed on failure
        @param xsink exception sink

        @return stream id on success, -1 on failure

        @since %Qore 2.3
    */
    DLLEXPORT virtual int64_t submitRequestWithAction(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        AbstractAsyncAction* action, ExceptionSink* xsink);

    //! Submits a request with streaming send (body pushed incrementally via pushSendData)
    /** The initial request headers are sent without a body. The caller then pushes
        body chunks via @ref pushSendData and signals end-of-body with pushSendData(nullptr, 0).
        If @a streaming_recv is true, the response is delivered via @a channel_out;
        otherwise a Future hash is returned in the result.

        @param method HTTP method
        @param path request path
        @param headers optional request headers
        @param streaming_recv if true, response delivered incrementally via Channel
        @param channel_out receives a ref'd QoreChannel* when streaming_recv is true
        @param xsink exception sink
        @return result hash on success, nullptr on error

        @since %Qore 2.3
    */
    DLLEXPORT virtual QoreHashNode* submitRequestStreamingSend(const char* method, const char* path,
        const QoreHashNode* headers, bool streaming_recv,
        QoreChannel*& channel_out, ExceptionSink* xsink);

    //! Push body data for a streaming send request
    /** @param data body chunk pointer (nullptr signals end-of-body)
        @param len data length (0 when data is nullptr)
        @param xsink exception sink

        @since %Qore 2.3
    */
    DLLEXPORT virtual void pushSendData(const void* data, size_t len, ExceptionSink* xsink);

    //! Set HTTP trailers for a streaming send request
    /** Must be called before the final pushSendData(nullptr, 0).

        @param trailers hash of trailer key-value pairs
        @param xsink exception sink

        @since %Qore 2.3
    */
    DLLEXPORT virtual void setTrailers(const QoreHashNode* trailers, ExceptionSink* xsink);

    //! Close the connection and release controller resources
    /** After this call, @ref isClosed returns true and no further requests
        can be submitted.  Any in-flight request is rejected with
        @c HTTP1-ABORT (or protocol equivalent).

        Default: calls @ref setClosed.  Subclasses override to also abort
        the poll op and cancel the controller submission.
    */
    DLLEXPORT virtual void closeConnection(ExceptionSink* xsink);

    //! Blocks until the connection transitions out of CONNECTING
    /** @param timeout_ms wait budget in milliseconds (0 or negative = wait forever)
        @param xsink exception sink — set with a connection error if the
            connection transitioned to CLOSED with an error during the wait

        @return @c true if the connection is READY; @c false if the wait
            timed out (no exception raised) or the connection is CLOSED
            (exception raised via @a xsink)
    */
    DLLEXPORT bool waitForReadyOrError(int64_t timeout_ms, ExceptionSink* xsink);

    //! Target hostname used for Host header and connect
    DLLEXPORT const char* getTargetHost() const {
        return target_host.c_str();
    }

    //! Target port
    DLLEXPORT int getTargetPort() const {
        return target_port;
    }

    //! True if the connection requires SSL/TLS
    DLLEXPORT bool isSslRequired() const {
        return ssl_required;
    }

    //! Default constructor for Qore subclass construction.
    /** Qore subclasses (HttpClientIo::HttpClientConnection) create the
        C++ priv via this default constructor; protocol-specific state
        lives in the Qore layer.
    */
    DLLEXPORT HttpClientConnectionBase() = default;

    //! Sets the pool key for O(1) eviction by the connection manager
    /** Called by the manager when inserting the connection into the pool.
        @param key the pool key string
        @since %Qore 2.3
    */
    DLLEXPORT void setPoolKey(const std::string& key);

    //! Returns the pool key (empty if not in a pool)
    /** @since %Qore 2.3
    */
    DLLEXPORT const std::string& getPoolKey() const;

    //! Returns the referenced protocol-specific error hash (if any)
    /** The returned hash has @c err (string) and @c desc (string) keys;
        caller owns the returned ref (or nullptr if no error info is
        available).  Called internally by @ref waitForReadyOrError, and
        externally by async poll wrappers when the connection transitions
        to CLOSED.

        @since %Qore 2.3
    */
    DLLEXPORT virtual QoreHashNode* getReferencedErrorInfo() {
        return nullptr;
    }

    //! Lifetime barrier: concurrent-safe guard against use-after-free.
    /** Qore's connection APIs are reachable from any thread; a well-formed
        Qore program can `delete rc` from one thread while another is still
        executing `rc.get()`.  Libqore's contract is "Qore code cannot crash
        libqore", so the C++ connection MUST keep itself alive for the
        duration of any in-flight public method call regardless of when
        the destructor is invoked.

        @par Design

        A single atomic @c uint32_t packs two pieces of state:
          - Bit 31 (@ref INVALIDATED_BIT): set exactly once, by
            @ref markInvalidated, when the connection is being destroyed
            or @ref closeConnection is called.  Sticky — once set, never
            clears.
          - Bits 0..30 (@ref IN_FLIGHT_MASK): count of @ref MethodGuard
            instances currently in the @em acquired state.

        @par Methods

        Every public method that dereferences protocol-specific state
        begins with @code{.cpp}
            MethodGuard g(this);
            if (!g.acquired()) {
                xsink->raiseException("HTTPCLIENT-STATE-ERROR",
                    "connection has been closed");
                return ...;
            }
        @endcode The guard's constructor CAS-increments the in-flight
        count unless the invalidated bit is already set, in which case it
        reports @c acquired()==false and the method returns a clean Qore
        exception.  The guard's destructor decrements the count and, if
        it observes the decrement dropping the count to zero while the
        invalidated bit is set, wakes a closer waiting in
        @ref drainInFlight.

        @par Close path

        @ref closeConnection (and the destructor, which calls
        @ref closeConnection) must invoke @ref markInvalidated then
        @ref drainInFlight before dereferencing protocol-specific members.
        After @ref drainInFlight returns, no in-flight call exists and no
        new one can start — the subsequent teardown is race-free.

        @par Concurrency properties
          - Uncontended cost per method call: one acq-rel CAS + one
            acq-rel fetch_sub on a single atomic.  On x86_64 both are
            hardware-LOCK ops, ~10-15 ns total.
          - Parallel method calls from multiple threads do not serialize
            against each other — the guard is a reader-writer pattern
            with close as the single writer.
          - Destructor waits for the specific in-flight calls that were
            already running; it never preempts a live method body.

        @since %Qore 2.3
    */
    static constexpr uint32_t INVALIDATED_BIT = 0x80000000u;
    static constexpr uint32_t IN_FLIGHT_MASK  = 0x7FFFFFFFu;

    //! RAII entry barrier for public methods.  See @c lifetime_state_.
    /** Acquire on method entry.  Check @ref acquired() before dereferencing
        any protocol-specific member; if @c false, the connection is in the
        process of being closed / destroyed and the method must return an
        error without touching further state.  Release is automatic at
        scope exit.

        Designed to be trivially cheap on the uncontended path.  Supports
        recursive acquisition on the same thread (via the count — not a
        mutex): nested calls each add one to the count and each release
        subtracts one.

        @since %Qore 2.3
    */
    class DLLEXPORT MethodGuard {
    public:
        DLLEXPORT explicit MethodGuard(HttpClientConnectionBase* c);
        DLLEXPORT ~MethodGuard();

        //! Non-copyable, non-movable — guards are stack-local.
        MethodGuard(const MethodGuard&) = delete;
        MethodGuard& operator=(const MethodGuard&) = delete;

        //! @return @c true if the guard incremented the in-flight count;
        //!     @c false if the connection was already invalidated.
        bool acquired() const { return acquired_; }

    private:
        HttpClientConnectionBase* conn_;
        bool acquired_;
    };

    //! Atomically sets the invalidated bit and returns the prior state.
    /** Idempotent — subsequent calls are no-ops w.r.t. the bit but still
        return the current state.  Called from @ref closeConnection (and
        the destructor) before @ref drainInFlight.

        @return the prior @c lifetime_state_ value; the caller can mask
            with @ref IN_FLIGHT_MASK to observe how many calls were
            in flight at the moment of invalidation.
        @since %Qore 2.3
    */
    DLLEXPORT uint32_t markInvalidated();

    //! Blocks until every in-flight @ref MethodGuard has released.
    /** Must be called AFTER @ref markInvalidated (otherwise new guards
        would keep acquiring while the caller waits).  Returns promptly
        when no call was in flight at invalidation time.

        @since %Qore 2.3
    */
    DLLEXPORT void drainInFlight();

    //! Returns the connection creation timestamp (epoch microseconds).
    /** Set once in the protected constructor; never updated.  Used by
        @ref HttpClientConnectionManagerBase to enforce
        @ref Options::max_age_ms at pool checkout.  Pure stored-state
        read — no syscall, no I/O thread coupling.
    */
    DLLEXPORT int64_t getCreatedUs() const {
        return created_us_;
    }

protected:
    DLLLOCAL HttpClientConnectionBase(std::string target_host, int target_port,
            bool ssl_required)
        : target_host(std::move(target_host)),
          target_port(target_port),
          ssl_required(ssl_required) {
        int us = 0;
        created_us_ = q_epoch_us(us) * 1000000LL + us;
    }

    //! Raises the stored protocol error for a closed connection, if available
    DLLEXPORT void raiseClosedSubmitError(const char* fallback_desc,
        ExceptionSink* xsink);

    std::string target_host;
    int target_port;
    bool ssl_required;

    //! Connection creation timestamp (epoch microseconds), born-at TTL.
    int64_t created_us_ = 0;

    //! Lock protecting @ref pending_stream_count.
    /** This is a leaf lock — it does not nest with other connection
        locks.  See section 7.2 of @c design/http-client-manager-cpp-port.md
        for the full lock-ordering rules.
    */
    mutable QoreThreadLock reserve_lock;

    //! Reserved-but-not-yet-submitted stream slots.  Protected by
    //! @ref reserve_lock.
    int pending_stream_count = 0;

    //! Lock protecting @ref manager_.
    /** This is the OUTERMOST lock in the close-hook lock-ordering chain:
        @c onclose_lock → manager.pool_lock → controller.lock.  Held only
        briefly to read or write @ref manager_; @ref onClosedHook releases
        it BEFORE invoking the manager method to avoid lock inversion
        with app-thread paths that take @c pool_lock first.
    */
    mutable QoreThreadLock onclose_lock;

    //! Owning manager back-pointer (raw, no ref).  Protected by
    //! @ref onclose_lock.
    /** Set/cleared by @ref setManager.  Read by @ref onClosedHook to
        dispatch the eviction call.  The manager destructor MUST call
        @c setManager(nullptr) on every connection it owns BEFORE
        freeing manager state — see the lifetime contract in section
        7.3 of @c design/http-client-manager-cpp-port.md.
    */
    HttpClientConnectionManagerBase* manager_ = nullptr;

    //! Pool key for O(1) eviction.  Set by setPoolKey(), read by the
    //! manager's onConnectionClosed / closeAndEvict.
    std::string pool_key_;

    //! Lifetime barrier state — see @ref MethodGuard.
    /** Packed: top bit @ref INVALIDATED_BIT + low 31 bits in-flight count.
        Declared @c mutable so @c const methods can also guard (we don't
        add one today, but future @c isFoo() reads could participate).
    */
    mutable std::atomic<uint32_t> lifetime_state_{0};

    //! Mutex + condvar coordinating @ref drainInFlight with the final
    //! @ref MethodGuard release.  @c close_cv_ is notified from
    //! @ref MethodGuard's destructor when it observes the count dropping
    //! to zero while @ref INVALIDATED_BIT is set, waking a closer blocked
    //! in @ref drainInFlight.
    mutable std::mutex close_mu_;
    mutable std::condition_variable close_cv_;
};

#endif // _QORE_HTTPCLIENTCONNECTION_H

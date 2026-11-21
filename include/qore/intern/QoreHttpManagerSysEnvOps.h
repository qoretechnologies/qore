/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttpManagerSysEnvOps.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QOREHTTPMANAGERSYSENVOPS_H
#define _QORE_INTERN_QOREHTTPMANAGERSYSENVOPS_H

#include <qore/HttpClientConnectionManager.h>
#include <qore/SystemEnvironment.h>

#include <utility>

/** The non-virtual DLLLOCAL member functions of HttpClientConnectionManagerBase and SystemEnvironment were declared in the public headers although no module can call them (they are
    not exported); they are members of the classes below, in the implementation of the library.  Each class is a friend
    of the class it serves; call an operation through a temporary object:

    @code
    qore_system_environment_ops::value_exists_intern(name);
    @endcode

    A function that stays out of line is a static fooImpl(Cls* b, ...) with an inline wrapper, so that the temporary is
    optimized away and the call costs what the member function cost (a temporary whose address a call takes makes GCC
    add a stack-protector canary to the caller).
*/

//! The library-internal operations of HttpClientConnectionManagerBase (qore#4380)
class qore_http_manager_ops {
public:
    //! the object
    HttpClientConnectionManagerBase* b;

    DLLLOCAL explicit qore_http_manager_ops(HttpClientConnectionManagerBase* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_http_manager_ops(H& holder) : qore_http_manager_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_http_manager_ops(const HttpClientConnectionManagerBase* n_b) : b(const_cast<HttpClientConnectionManagerBase*>(n_b)) {
    }

    DLLLOCAL explicit qore_http_manager_ops(HttpClientConnectionManagerBase& r) : b(&r) {
    }

    DLLLOCAL explicit qore_http_manager_ops(const HttpClientConnectionManagerBase& r) : b(const_cast<HttpClientConnectionManagerBase*>(&r)) {
    }

    //! Internal: calls @ref acquireConnectionImpl, which returns a referenced connection, and returns the connection
    //! only when no exception was raised; otherwise gives back its stream slot and reference
    DLLLOCAL static HttpClientConnectionBase* acquireConnectionCheckedImpl(HttpClientConnectionManagerBase* b, const char* scheme, const char* host, int port, bool wait_for_ready, bool streaming_send, ExceptionSink* xsink);
    DLLLOCAL HttpClientConnectionBase* acquireConnectionChecked(const char* scheme, const char* host, int port, bool wait_for_ready, bool streaming_send, ExceptionSink* xsink) {
        return acquireConnectionCheckedImpl(b, scheme, host, port, wait_for_ready, streaming_send, xsink);
    }

    //! Internal: shared implementation for the acquire methods; returns a connection with a reference of the
    //! caller, taken under the pool lock.  When @a wait_for_ready is @c false,
    //! newly-created connections are returned in CONNECTING state.  When
    //! @a streaming_send is true, the reservation also claims the
    //! per-connection incremental-body sender slot.
    DLLLOCAL static HttpClientConnectionBase* acquireConnectionImplImpl(HttpClientConnectionManagerBase* b, const char* scheme, const char* host, int port, bool wait_for_ready, bool streaming_send, ExceptionSink* xsink);
    DLLLOCAL HttpClientConnectionBase* acquireConnectionImpl(const char* scheme, const char* host, int port, bool wait_for_ready, bool streaming_send, ExceptionSink* xsink) {
        return acquireConnectionImplImpl(b, scheme, host, port, wait_for_ready, streaming_send, xsink);
    }

    //! Internal: scans the pool for a reusable connection (caller must
    //! hold a shared or unique lock).  Returns @c nullptr if no live
    //! connection has capacity.  Reservation is atomic via
    //! @c tryReserveStream — see open question 7.6 in the design doc.
    /** A connection is reused only by code whose network sandbox allows it, as checked by
        HttpClientConnectionBase::checkSandboxUse().

        @param key the pool key
        @param streaming_send true to claim the incremental-body sender slot as well
        @param scan the network sandbox of the calling code, which receives the decisions of the scan
        @param check_age true to skip the connections that have reached @ref Options::max_age_ms (the lock-free scan
        of a checkout); false after evictDeadLocked() has removed them under the same lock, so that a connection that
        it kept is not then left out of both reuse and eviction (#6150)

        @return a connection with a stream reserved, or nullptr
    */
    //! Replaces the pooled transitional connection of an ALPN negotiation with its concrete connection
    /** The last step of @ref finalizeAsyncConnection: the concrete connection takes the transitional one's place
        in the pool, unless the transitional one has left the pool already or the concrete one's close notification
        has been sent (HttpClientConnectionBase::setPoolKey() refuses its key), in which case the concrete
        connection is returned to the caller without being pooled, and a transitional connection still in the pool
        is removed from it.

        @param neg the transitional connection; the caller keeps its reference
        @param concrete the concrete connection; the reference of the caller is taken
        @param reserve_stream true to reserve a stream on the concrete connection for the caller
        @param xsink exception sink

        @return the concrete connection, with a reference for the caller

        @since %Qore 3.0
    */
    DLLLOCAL static HttpClientConnectionBase* swapNegotiatedConnectionImpl(HttpClientConnectionManagerBase* b, HttpClientConnectionBase* neg,
        HttpClientConnectionBase* concrete, bool reserve_stream, ExceptionSink* xsink);
    DLLLOCAL HttpClientConnectionBase* swapNegotiatedConnection(HttpClientConnectionBase* neg,
        HttpClientConnectionBase* concrete, bool reserve_stream, ExceptionSink* xsink) {
        return swapNegotiatedConnectionImpl(b, neg, concrete, reserve_stream, xsink);
    }

    DLLLOCAL static HttpClientConnectionBase* findReusableLockedImpl(HttpClientConnectionManagerBase* b, const std::string& key, bool streaming_send, HttpClientConnectionManagerBase::SandboxScan& scan, bool check_age);
    DLLLOCAL HttpClientConnectionBase* findReusableLocked(const std::string& key, bool streaming_send, HttpClientConnectionManagerBase::SandboxScan& scan, bool check_age = true) {
        return findReusableLockedImpl(b, key, streaming_send, scan, check_age);
    }

    //! Makes one attempt of @ref request
    /** @param retry if not nullptr, set to true if the request failed because the peer closed a connection that
        had served a request, in which case the request can be repeated on a new connection; nullptr if the
        request must not be repeated
    */
    DLLLOCAL static QoreHashNode* requestOnceImpl(HttpClientConnectionManagerBase* b, const char* method, const char* scheme, const char* host, int port, const char* path, const QoreHashNode* headers, const void* body, size_t body_len, int timeout_ms, ExceptionSink* xsink, HttpClientEventSink* event_sink, bool* retry);
    DLLLOCAL QoreHashNode* requestOnce(const char* method, const char* scheme, const char* host, int port, const char* path, const QoreHashNode* headers, const void* body, size_t body_len, int timeout_ms, ExceptionSink* xsink, HttpClientEventSink* event_sink, bool* retry) {
        return requestOnceImpl(b, method, scheme, host, port, path, headers, body, body_len, timeout_ms, xsink, event_sink, retry);
    }

    //! Makes one attempt of @ref requestStreaming
    /** @param reused if not nullptr, set to true if the request was submitted on a connection that had been used
        before
        @param retry if not nullptr, set to true if the request could not be submitted because the connection that
        it was given, which had served a request, is closed, in which case the request can be repeated on a new
        connection; nullptr if the request must not be repeated
        @param conn_out if not nullptr, receives the connection that the request was submitted on, with a reference
    */
    DLLLOCAL static int64_t requestStreamingOnceImpl(HttpClientConnectionManagerBase* b, const char* method, const char* scheme, const char* host, int port, const char* path, const QoreHashNode* headers, const void* body, size_t body_len, QoreChannel*& channel_out, ExceptionSink* xsink, HttpClientEventSink* event_sink, bool* reused, bool* retry, HttpClientConnectionBase** conn_out);
    DLLLOCAL int64_t requestStreamingOnce(const char* method, const char* scheme, const char* host, int port, const char* path, const QoreHashNode* headers, const void* body, size_t body_len, QoreChannel*& channel_out, ExceptionSink* xsink, HttpClientEventSink* event_sink, bool* reused, bool* retry, HttpClientConnectionBase** conn_out) {
        return requestStreamingOnceImpl(b, method, scheme, host, port, path, headers, body, body_len, channel_out, xsink, event_sink, reused, retry, conn_out);
    }

    //! Returns true if a cancellation of the calling thread is pending, without raising it
    /** A request that a pending cancellation ended is not repeated; the next cancellation point raises the
        cancellation

        @since %Qore 3.0
    */
    DLLLOCAL static bool isCancelPending();

    //! Returns true if the exception means that the peer closed the connection before a response was received
    /** @since %Qore 3.0
    */
    DLLLOCAL static bool isConnectionClosedError(ExceptionSink& xsink);

    //! Returns true if the HTTP method is idempotent (RFC 9110 section 9.2.2), so a request with it can be repeated
    //! automatically when the connection fails before a response is received
    /** @since %Qore 3.0
    */
    DLLLOCAL static bool isIdempotentMethod(const char* method);

    //! Replaces the error of a request with the calling thread's pending cancellation, if any
    /** A cancellation of the thread that waits for a request can end the request before the wait sees the
        cancellation, whatever ends it: the code that the cancellation ends closes the transport, and the request
        fails with a closed connection, or with a send or receive error when the peer resets the connection that it
        sees closed (issue #5793); the caller must see the cancellation, not the error that it caused, and the
        request is not repeated.  An error that the request raised for another reason (before any I/O, for example)
        is replaced as well, as the next cancellation point would raise the cancellation in any case

        @return true if the error was replaced

        @since %Qore 3.0
    */
    DLLLOCAL static bool replaceErrorWithPendingCancel(ExceptionSink& xsink);

    //! Computes the pool key for the given target (with proxy info baked in).
    /** The key includes the scheme: a connection with TLS must never be used for a target without TLS or the
        reverse, as when a redirect changes the scheme but not the host and port.
    */
    DLLLOCAL static std::string poolKeyImpl(HttpClientConnectionManagerBase* b, const char* host, int port, bool ssl);
    DLLLOCAL std::string poolKey(const char* host, int port, bool ssl) {
        return poolKeyImpl(b, host, port, ssl);
    }

    //! Parsed proxy info; @c nullptr if no proxy configured.
    //! Pushes the per-connection options to a connection
    /** Called for a new connection and again for the concrete connection that replaces a negotiating one, as a
        negotiating connection does not keep them.
    */
    DLLLOCAL static void applyConnectionOptionsImpl(HttpClientConnectionManagerBase* b, HttpClientConnectionBase* conn);
    DLLLOCAL void applyConnectionOptions(HttpClientConnectionBase* conn) {
        applyConnectionOptionsImpl(b, conn);
    }

    //! Closes and derefs a connection that was identified for eviction
    //! while a pool-lock was held.  Caller must NOT hold @c pool_lock_.
    /** Mirrors the close+deref pattern used by the shutdown branch of
        @ref acquireConnectionImpl (line ~360).
    */
    DLLLOCAL static void closeAndDerefAfterLockDropImpl(HttpClientConnectionManagerBase* b, HttpClientConnectionBase* conn);
    DLLLOCAL void closeAndDerefAfterLockDrop(HttpClientConnectionBase* conn) {
        closeAndDerefAfterLockDropImpl(b, conn);
    }

    //! Releases a reference to a connection that the manager does not keep in its pool
    /** Clears the connection's manager back-pointer before the reference is released: the reference may not be
        the last one, as the async I/O thread holds the connection while it is closing it, and the close
        notification of a connection that the manager has released can come after the manager has been destroyed.

        @param conn the connection; may be nullptr
        @param xsink for exceptions raised by the release
    */
    DLLLOCAL static void detachAndDerefImpl(HttpClientConnectionManagerBase* b, HttpClientConnectionBase* conn, ExceptionSink* xsink);
    DLLLOCAL void detachAndDeref(HttpClientConnectionBase* conn, ExceptionSink* xsink) {
        detachAndDerefImpl(b, conn, xsink);
    }

    //! Internal: removes closed and (optionally) max-age-expired connections
    //! from @c pool_[key] (caller must hold the unique lock).
    /** Pool references for closed connections are transferred to
        @a out_to_deref so destruction cannot run under @c pool_lock_.
        Max-age-expired connections are not yet closed and are transferred
        to @a out_to_close.  After releasing @c pool_lock_, the caller MUST
        deref every closed entry and invoke @ref closeAndDerefAfterLockDrop
        for every max-age entry.

        @param key the pool key
        @param out_to_close out-vector receiving max-age-expired connections
            that need close-after-lock-drop
        @param out_to_deref out-vector receiving already-closed connections
            whose pool references must be released after the lock drops
    */
    DLLLOCAL static void evictDeadLockedImpl(HttpClientConnectionManagerBase* b, const std::string& key, std::vector<HttpClientConnectionBase*>* out_to_close, std::vector<HttpClientConnectionBase*>* out_to_deref);
    DLLLOCAL void evictDeadLocked(const std::string& key, std::vector<HttpClientConnectionBase*>* out_to_close, std::vector<HttpClientConnectionBase*>* out_to_deref) {
        evictDeadLockedImpl(b, key, out_to_close, out_to_deref);
    }

    //! Drains closed-connection notifications from onConnectionClosed.
    /** Must be called from an app thread, not the I/O thread.
    */
    DLLLOCAL static void processClosedConnectionsImpl(HttpClientConnectionManagerBase* b, ExceptionSink* xsink);
    DLLLOCAL void processClosedConnections(ExceptionSink* xsink) {
        processClosedConnectionsImpl(b, xsink);
    }

};

//! The library-internal operations of SystemEnvironment (qore#4380)
class qore_system_environment_ops {
public:
    //! the object
    SystemEnvironment* b;

    DLLLOCAL explicit qore_system_environment_ops(SystemEnvironment* n_b) : b(n_b) {
    }

    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_system_environment_ops(H& holder) : qore_system_environment_ops(holder.operator->()) {
    }

    DLLLOCAL explicit qore_system_environment_ops(const SystemEnvironment* n_b) : b(const_cast<SystemEnvironment*>(n_b)) {
    }

    DLLLOCAL explicit qore_system_environment_ops(SystemEnvironment& r) : b(&r) {
    }

    DLLLOCAL explicit qore_system_environment_ops(const SystemEnvironment& r) : b(const_cast<SystemEnvironment*>(&r)) {
    }

    //! returns the value of the environment variable as a new QoreString object, 0 if not present, caller owns the pointer returned
    /** the caller must hold the environment lock
        @param name the name of the environment variable
        @return a QoreString pointer (or 0 if the variable does not exist), caller owns the pointer returned
     */
    DLLLOCAL static QoreString* get_intern(const char* name);

    //! returns the value of the environment variable as a new QoreStringNode object, 0 if not present, caller owns the reference count of the pointer returned
    /** the caller must hold the environment lock
        @param name the name of the environment variable
        @return a QoreStringNode pointer (or 0 if the variable does not exist), caller owns the reference count of the pointer returned
     */
    DLLLOCAL static QoreStringNode* get_as_string_node_intern(const char* name);

    //! returns true if the environment variable exists and has a value, false if not
    /** the caller must hold the environment lock
     */
    DLLLOCAL static bool value_exists_intern(const char* name);

    //! appends the value of the given environment variable to a QoreString, returns 0 for OK, -1 for not found
    /** the caller must hold the environment lock
        @param name the name of the environment variable
        @param str a reference to a QoreString object where the value will be concatenated if the environment variable exists
        @return 0 for OK (environment variable found and value concatenated to string), or -1 for not found
     */
    DLLLOCAL static int get_intern(const char* name, QoreString& str);

    //! sets the given environment variable to the value passed, respecting the overwrite flag
    /** the caller must hold the environment lock exclusively
        @param name the name of the environment variable to set
        @param value the value of the environment variable
        @param overwrite the overwrite flag
     */
    DLLLOCAL static int set_intern(const char* name, const char* value, bool overwrite = 1);

    //! unsets the given environment variable
    /** the caller must hold the environment lock exclusively
        @param name the name of the environment variable to unset
        @return 0 for OK, non-0 for error
     */
    DLLLOCAL static int unset_intern(const char* name);

};

#endif // _QORE_INTERN_QOREHTTPMANAGERSYSENVOPS_H

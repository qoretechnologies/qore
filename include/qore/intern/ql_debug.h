/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ql_debug.h

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

#ifndef QORE_LIB_DEBUG_H

#define QORE_LIB_DEBUG_H

#include "qore/QoreThreadLock.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

DLLLOCAL void init_debug_functions(QoreNamespace& qns);

//! Holds the lock that protects the error or another value recorded by a poll operation, for a read of it
/** In debug builds, a read that finds the lock taken calls the "error-read" hold point
    (QoreDbgHttpHoldPoint::ErrorRead), or "value-read" (QoreDbgHttpHoldPoint::ValueRead) for another value, before
    it waits for the lock: a test that holds the thread recording it in its critical section ("error-record" or
    "value-record") is then released only by a read that waits for it (issue #6428); the hold points can be armed
    for one operation, which is identified by its Qore object
*/
class QoreRecordedValueReadLocker {
public:
    //! takes the lock
    /** @param lck the lock
        @param target the Qore object of the operation, which identifies it for the hold points
        @param value false for a read of the error, true for a read of another value
    */
    DLLLOCAL QoreRecordedValueReadLocker(QoreThreadLock& lck, const void* target, bool value = false);

    DLLLOCAL ~QoreRecordedValueReadLocker() {
        lck.unlock();
    }

private:
    QoreThreadLock& lck;
};

class AbstractSmartLock;

#ifdef DEBUG
//! a QCF_NO_DOMAIN_THROW violation aborts the process (the default)
#define QORE_FLAG_VIOLATION_ABORT   0
//! a QCF_NO_DOMAIN_THROW violation is only counted
/** Tests select this so that they can drive a deliberately-lying variant and assert that the check
    fired, instead of having the process abort out from under them.
 */
#define QORE_FLAG_VIOLATION_RECORD  1

//! selects how a detected code flag violation is reported
/** The mode and the counter are both process-global, not per-thread: a test that switches to
    QORE_FLAG_VIOLATION_RECORD suppresses the abort for every thread for as long as it is set, and a
    violation on any thread moves the same counter.  That is a deliberate trade for testability -
    the check runs in ~CodeEvaluationHelper(), which has no natural place to hang per-thread state -
    so a caller that reads the counter as a delta should not have other threads running code under
    test at the same time.
 */
DLLLOCAL void qore_set_flag_violation_mode(int mode);

//! returns the number of code flag violations detected since process start
DLLLOCAL int64 qore_get_flag_violations();

//! called by ConstantEntry::setRuntimeValue() at the point where a thread copying the entry can run
//! concurrently with the store; set by dbg_hold_constant_store()
typedef void (*qore_dbg_constant_store_hook_t)(const char* name);
DLLLOCAL extern std::atomic<qore_dbg_constant_store_hook_t> qore_dbg_constant_store_hook;

//! called by qore_object_private::customDeref() when a dereference of an object that has a collection decision to
//! make has taken its count, before it decides; called with no lock held; set by dbg_hold_deref_decide()
typedef void (*qore_dbg_deref_decide_hook_t)(const char* class_name);
DLLLOCAL extern std::atomic<qore_dbg_deref_decide_hook_t> qore_dbg_deref_decide_hook;

//! called by qore_object_private::customDeref() when a dereference has marked the object as being destroyed and
//! released the object's lock, before the dereference completes and deletes the object; called with no lock held;
//! set by dbg_hold_deref_marked()
typedef void (*qore_dbg_deref_marked_hook_t)(const char* class_name);
DLLLOCAL extern std::atomic<qore_dbg_deref_marked_hook_t> qore_dbg_deref_marked_hook;

//! called by qore_object_private::customDeref() when the dereference that marked the object as being destroyed has
//! completed (RObject::derefDone()), before the deletion starts; called with no lock held; set by
//! dbg_hold_deref_delete()
typedef void (*qore_dbg_deref_delete_hook_t)(const char* class_name);
DLLLOCAL extern std::atomic<qore_dbg_deref_delete_hook_t> qore_dbg_deref_delete_hook;

//! called by qore_object_private::customDeref() when a dereference has released the last reference to the object
//! and has no collection to decide, before it takes the object's lock to delete it or hand its deletion off; called
//! with no lock held; set by dbg_hold_deref_last()
typedef void (*qore_dbg_deref_last_hook_t)(const char* class_name);
DLLLOCAL extern std::atomic<qore_dbg_deref_last_hook_t> qore_dbg_deref_last_hook;

//! called by QoreListNode::derefImpl() and QoreHashNode::derefImpl() when the free of a list or hash that has weak
//! holders starts, before its entries are freed; called with no lock held; \a type_name is "list" or "hash"; set by
//! dbg_hold_container_free() (qore#7198)
typedef void (*qore_dbg_container_free_hook_t)(const char* type_name);
DLLLOCAL extern std::atomic<qore_dbg_container_free_hook_t> qore_dbg_container_free_hook;

//! called by qore_object_private::customDeref() when a dereference has decided to collect the object as part of a
//! recursive set, before it takes the object's lock to start deleting it; set by dbg_hold_deref_collect()
typedef void (*qore_dbg_deref_collect_hook_t)(const char* class_name);
DLLLOCAL extern std::atomic<qore_dbg_deref_collect_hook_t> qore_dbg_deref_collect_hook;

//! called by RObject::derefDone() when a thread starts waiting for other threads' dereferences of the object to
//! finish; called with the object's rlck held, so it must not block; set by dbg_deref_wait_notify()
typedef void (*qore_dbg_deref_wait_hook_t)(const char* name);
DLLLOCAL extern std::atomic<qore_dbg_deref_wait_hook_t> qore_dbg_deref_wait_hook;

//! called right before a read or write of a descriptor waits for it: by qore_fd_wait_ready() after a read or write
//! returned EAGAIN (a File, or standard output or standard error written by print(), printf() or an unhandled
//! exception), and by the waits of a File for data with a timeout other than 0; called in the waiting thread, which
//! can hold the File's lock, so it must not block; set by dbg_fd_wait_notify()
typedef void (*qore_dbg_fd_wait_hook_t)(int fd);
DLLLOCAL extern std::atomic<qore_dbg_fd_wait_hook_t> qore_dbg_fd_wait_hook;

//! called by a read of a File that waited for its descriptor and found it readable, right before the read, with the
//! File's lock held; set by dbg_hold_file_read_ready()
typedef void (*qore_dbg_file_read_ready_hook_t)(int fd);
DLLLOCAL extern std::atomic<qore_dbg_file_read_ready_hook_t> qore_dbg_file_read_ready_hook;

//! called by ClosureVarValue::deref() when a dereference of a closure-bound variable that has a collection decision
//! to make has taken its count, before it decides; called with no lock held; set by dbg_hold_closure_var_deref()
//! called by an LValueHelper whose lvalue was reached through a reference and holds an integer, before it releases
//! the lvalue's locks and leaves the Programs of the references
/** see dbg_hold_reference_lvalue(): a test holds the thread there, after it assigned the value through the reference
*/
typedef void (*qore_dbg_reference_lvalue_hook_t)(int64 value);
DLLLOCAL extern std::atomic<qore_dbg_reference_lvalue_hook_t> qore_dbg_reference_lvalue_hook;

//! called by qore_removed_object_needs_scan() after it has compared the sets of a removed object and of the object
//! that it was removed from, before it reads the set; set by dbg_hold_removed_object_scan() (#6792)
typedef void (*qore_dbg_removed_object_scan_hook_t)();
DLLLOCAL extern std::atomic<qore_dbg_removed_object_scan_hook_t> qore_dbg_removed_object_scan_hook;

typedef void (*qore_dbg_closure_var_decide_hook_t)(const char* name);
DLLLOCAL extern std::atomic<qore_dbg_closure_var_decide_hook_t> qore_dbg_closure_var_decide_hook;

//! called by ClosureVarValue::deref() when a dereference collects a closure-bound variable with its recursive set;
//! set by dbg_count_closure_var_collections()
typedef void (*qore_dbg_closure_var_collect_hook_t)(const char* name);
DLLLOCAL extern std::atomic<qore_dbg_closure_var_collect_hook_t> qore_dbg_closure_var_collect_hook;

//! returns true if the staged response data of the given HTTP/3 stream is held back from the transport, as when its
//! client grants no flow-control credit; set by dbg_quic_hold_stream_data()
DLLLOCAL bool qore_dbg_quic_stream_data_held(int64_t session_id, int64_t stream_id);

//! releases the stream held with dbg_quic_hold_stream_data(..., True) if it is a stream of the session
/** Called by Socket::flushQuicPendingData() on the I/O thread of the session's socket before it writes, so that the
    flush, and not the server's poll operation, writes the data that was held (qore#7414)
*/
DLLLOCAL void qore_dbg_quic_flush_releases_held(int64_t session_id);

//! set by dbg_http2_fail_next_submit(): the next request submitted on an HTTP/2 client session fails as when the
//! session has no stream ID left (NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE; qore#7443)
DLLLOCAL extern std::atomic<bool> qore_dbg_h2_fail_next_submit;

//! the stateless resets that the QUIC listeners of the process sent and that its QUIC clients received (qore#6931);
//! see dbg_quic_stateless_reset_counts()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_quic_stateless_resets_sent;
DLLLOCAL extern std::atomic<int64_t> qore_dbg_quic_stateless_resets_received;

//! the Retry packets that the QUIC servers of the process sent; see dbg_quic_server_retry_counts()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_quic_server_retries_sent;

//! the connections that the QUIC servers of the process dropped because a client Initial did not start the
//! ClientHello although the client had returned a valid Retry token; see dbg_quic_server_retry_counts()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_quic_server_validated_drops;

//! the number of threads waiting in dbg_quic_wait_idle(); a QUIC session wakes them only while it is not 0
DLLLOCAL extern std::atomic<int> qore_dbg_quic_idle_waiters;

//! wakes the threads waiting in dbg_quic_wait_idle(); called by a QUIC session when it has read packets, which can
//! acknowledge the data that it has in flight (issue #7354)
/** Takes only the lock of the waiters, so that a session can call it with its own lock held
*/
DLLLOCAL void qore_dbg_quic_notify_idle_waiters();

class QuicSession;

//! records a QUIC session, so that dbg_quic_wait_idle() can find it by its ID (issue #7354), and
//! dbg_quic_wait_all_idle() can wait for it (qore#7452)
/** Only a weak reference is kept; the record is removed by qore_dbg_quic_unregister_session() when the session is
    destroyed.  A session that cannot be recorded works all the same.
*/
DLLLOCAL void qore_dbg_quic_register_session(const std::shared_ptr<QuicSession>& session) noexcept;

//! removes the record of a QUIC session made by qore_dbg_quic_register_session(), if any
DLLLOCAL void qore_dbg_quic_unregister_session(int64_t session_id) noexcept;

//! counts a client datagram dropped by the QORE_QUIC_TEST_DROP_CLIENT_DATAGRAMS test hook and wakes the threads waiting
//! in dbg_quic_wait_dropped_client_datagrams() (qore#7452)
DLLLOCAL void qore_dbg_quic_note_dropped_client_datagram();

//! Returns True if dbg_io_uring_defer_completions() leaves the io_uring completions that a poll of an I/O thread
//! reports to the drain at the end of the thread's iteration (qore#7435)
DLLLOCAL bool qore_dbg_io_uring_completions_deferred();

//! Returns True if dbg_http2_hold_stream_data() holds the buffered data of this HTTP/2 stream back
DLLLOCAL bool qore_dbg_h2_stream_data_held(int32_t stream_id);

//! Returns True if dbg_http2_hold_stream_reset() holds the RST_STREAM of this HTTP/2 stream back (qore#7532)
DLLLOCAL bool qore_dbg_h2_stream_reset_held(int32_t stream_id);

//! Ends the hold of dbg_http2_hold_stream_reset() on this HTTP/2 stream; returns True if it held the stream
/** Called when the application reads the stream's progress count (Socket::getHttp2StreamTakenBytes()), after it is
    read; the caller then wakes the socket's I/O thread, which sends the RST_STREAM
*/
DLLLOCAL bool qore_dbg_h2_stream_reset_release(int32_t stream_id);

//! Returns the number of objects in the async I/O controller's registry of submitted poll operations without a C++
//! poll operation (those implemented in Qore)
DLLLOCAL size_t qore_dbg_registered_qore_ops();

//! Returns True if dbg_http3_client_hide_error() makes the HTTP/3 client poll operations report no error
DLLLOCAL bool qore_dbg_h3_client_error_hidden();

//! Counts a call of Socket::isHttp2() made off the I/O thread that queried the TLS state with a poll operation, as no
//! negotiation result was recorded (qore#7223); read with dbg_is_http2_poll_count()
DLLLOCAL void qore_dbg_note_is_http2_poll();

//! Returns the bytes of progress that dbg_quic_simulate_stream_progress() adds to the acknowledged byte count of a
//! stream for this read of the count (0 when no progress is simulated for the stream)
DLLLOCAL int64_t qore_dbg_quic_simulated_stream_progress(int64_t stream_id);

//! Returns the progress that dbg_http_ws_simulate_send_progress() simulates for the sends of the HTTP/1.1 WebSocket
//! server connections at the given time of the monotonic clock in milliseconds, or -1 when none is simulated
DLLLOCAL int64_t qore_dbg_ws_simulated_send_progress(int64_t now_ms);
//! the accounting of the stream credit that the HTTP/2 and HTTP/3 clients defer until the application consumes the
//! data (issue #5820), for dbg_http_stream_credit_stats(): after every stream has ended, deferred == credited +
//! settled (a byte is credited when it is consumed, or settled with its stream when the stream or its session ends
//! before it is consumed); excess and orphaned count reports that credit nothing
struct QoreDbgStreamCreditStats {
    //! bytes whose stream credit was deferred when they were received
    std::atomic<int64_t> deferred{0};
    //! bytes whose deferred stream credit was granted when they were consumed
    std::atomic<int64_t> credited{0};
    //! bytes whose deferred stream credit was settled when their stream or session ended before they were consumed
    std::atomic<int64_t> settled{0};
    //! bytes reported as consumed beyond the stream's deferred credit (credited when received, or reported twice)
    std::atomic<int64_t> excess{0};
    //! bytes reported as consumed after their session was gone
    std::atomic<int64_t> orphaned{0};
};
DLLLOCAL extern QoreDbgStreamCreditStats qore_dbg_stream_credit_stats;
//! the bytes held by the extended-CONNECT stream buffers of all QUIC sessions (QuicSession::connect_stream_data_),
//! for dbg_quic_connect_buffered_bytes() (issue #5821)
DLLLOCAL extern std::atomic<int64_t> qore_dbg_quic_connect_buffered;

//! the WINDOW_UPDATE frames for the connection (stream 0) that HTTP/2 sessions sent: [0] client sessions, [1] server
//! sessions; see dbg_http2_connection_window_updates()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_h2_connection_window_updates[2];
//! true to keep the RFC 9113 default connection window (65535) in new HTTP/2 sessions, for tests that detect lost
//! connection credit by watching the window shrink; see dbg_http2_default_connection_window()
DLLLOCAL extern std::atomic<bool> qore_dbg_h2_default_connection_window;
//! Use equally small connection and request-stream windows for new QUIC clients in receive-credit tests.
DLLLOCAL extern std::atomic<bool> qore_dbg_quic_small_client_stream_window;

//! adds received bytes whose stream credit was deferred, and wakes dbg_http_stream_credit_wait_deferred()
DLLLOCAL void qore_dbg_stream_credit_deferred(int64_t bytes);

class AbstractHttpPollConnectionPriv;
//! called by the HTTP/1 client poll operation when it is about to mark its connection closed (holding a reference
//! to it, but no lock of the operation), because it failed (\a failed true) or because a response ended the
//! connection (\a failed false); returns true if the hook took over marking it closed (holding its own reference);
//! set by dbg_defer_http1_connection_close()
typedef bool (*qore_dbg_http1_defer_close_hook_t)(AbstractHttpPollConnectionPriv* conn, bool failed);
DLLLOCAL extern std::atomic<qore_dbg_http1_defer_close_hook_t> qore_dbg_http1_defer_close_hook;

//! The points at which the HTTP/1 client poll operation calls @ref qore_dbg_http1_send_hook
enum class QoreDbgHttp1SendEvent : unsigned char {
    //! the operation decided to read the response after a failed send of the request (I/O thread, no lock held)
    RecoveryStart,
    //! the failed send was given up and the response is about to be read (I/O thread, no lock held)
    RecoveryRead,
    //! the operation published its first error (any thread, no lock held); \a err is the error code
    ErrorPublished,
    //! the request body send side is about to be driven after the receive side, with data to send (I/O thread, no
    //! lock held)
    BeforeSendSide,
    //! a request is about to be sent on a connection: the idle check for data from the server follows (I/O thread)
    BeforeIdleCheck,
    //! a request on a reused connection was sent: the check for the server's close follows (I/O thread)
    AfterRequestSent,
};

//! called by the HTTP/1 client poll operation at the points of QoreDbgHttp1SendEvent; \a fd is the descriptor of the
//! connection, \a err the error code for QoreDbgHttp1SendEvent::ErrorPublished; set by dbg_http1_send_failure_arm()
typedef void (*qore_dbg_http1_send_hook_t)(QoreDbgHttp1SendEvent event, int fd, const char* err);
DLLLOCAL extern std::atomic<qore_dbg_http1_send_hook_t> qore_dbg_http1_send_hook;

//! one-shot: returns true if the HTTP/1 client response header read operation about to be created at \a site must fail
//! (Http1ClientPollOperationPriv::dbgFailHeaderReadOp() then closes the socket); armed by
//! dbg_fail_http1_header_read_op(), which names the site or none for any
DLLLOCAL bool qore_dbg_http1_header_read_op_must_fail(const char* site);

//! one-shot: returns true if the allocation of the text of an HTTP header received at \a site must fail; armed by
//! dbg_fail_http_header_text_alloc(), which names the site (\c "read-header", \c "send-and-read-header" or
//! \c "send-stream-and-read-header")
DLLLOCAL bool qore_dbg_http_header_text_alloc_must_fail(const char* site);

//! returns true if the buffer of a binary value must not grow to \a size bytes; set by dbg_fail_binary_reserve_over()
DLLLOCAL bool qore_dbg_binary_reserve_must_fail(size_t size);

//! notes that thread \a tid starts waiting on a lock, for dbg_lock_wait_wait(); called by VLock::waitOn()
DLLLOCAL void qore_dbg_note_lock_wait(const AbstractSmartLock* asl, int tid);

//! the number of TLS close_notify alerts sent by SSLSocketHelper::sendCloseNotify() (SSL_shutdown() calls); read by
//! dbg_ssl_close_notify_count()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_ssl_close_notify_count;

//! the number of sets of references of synchronous operations that their callers took (AsyncIoSyncRelease) instead of
//! a callback worker; read by dbg_async_io_sync_release_counts()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_sync_release_taken;

//! the number of sets of references of synchronous operations that their callers refused, as their waits had ended,
//! and that went to a callback worker; read by dbg_async_io_sync_release_counts()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_sync_release_refused;

//! holds the thread that converts the end of a synchronous socket call's operation (canceled, or ended with an error)
//! into an error, before it checks the thread's pending cancellation, when dbg_hold_socket_cancel_conversion() has armed
//! the hold; see qore_socket_raise_pending_cancel()
DLLLOCAL void qore_dbg_socket_cancel_conversion();

//! called by SocketSendPollOperation::continuePoll() with the socket's lock held, before it writes; \a fd is the
//! socket's descriptor; set by dbg_http1_send_failure_arm()
typedef void (*qore_dbg_socket_send_hook_t)(int fd);
DLLLOCAL extern std::atomic<qore_dbg_socket_send_hook_t> qore_dbg_socket_send_hook;

//! called by SocketRecvUntilBytesPollState::doRecv() after SSL_read() reported SSL_ERROR_WANT_READ, before the
//! descriptor is polled; \a fd is the socket's descriptor; set by dbg_hold_tls_read_until_hangup() (#6315)
typedef void (*qore_dbg_tls_read_want_hook_t)(int fd);
DLLLOCAL extern std::atomic<qore_dbg_tls_read_want_hook_t> qore_dbg_tls_read_want_hook;

//! called by AbstractHttpPollConnectionPriv::setClosed() on the first close of a connection, after the close hook and
//! before it releases the reference that it holds while running the hook; returns true if the hook released a
//! reference of its own, in which case qore_dbg_conn_close_released() is called once setClosed() has released its
//! reference; set by dbg_release_http1_connection_on_close()
typedef bool (*qore_dbg_conn_close_release_hook_t)(AbstractHttpPollConnectionPriv* conn);
DLLLOCAL extern std::atomic<qore_dbg_conn_close_release_hook_t> qore_dbg_conn_close_release_hook;

//! called by AbstractHttpPollConnectionPriv::setClosed() on the first close of a connection: decrements the Counter
//! armed by dbg_http_submit_after_close()
DLLLOCAL void qore_dbg_http_note_closed();
//! called by HTTPClient::startPollSendRecv() right before it submits its request on a ready connection: waits until
//! the Counter armed by dbg_http_submit_after_close() reaches zero (a connection was marked closed), then disarms it
DLLLOCAL void qore_dbg_http_wait_submit_after_close();

//! the points at which an HTTP client can be held by dbg_http_hold_at()
enum class QoreDbgHttpHoldPoint : unsigned char {
    //! HTTPClient::startPollSendRecv() has checked out its connection and is about to submit the request
    Submit,
    //! an HTTP/1 or HTTP/2 client connection's closeConnection() has invalidated it and is about to close it
    CloseAfterInvalidate,
    //! HTTPClient::startPollSendRecv() submitted the request on a ready connection, the submit raised an exception, and
    //! the error is about to be classified (whether it is reported through the poll operation)
    SubmitFailed,
    //! HTTPClient::startPollSendRecv() found an ALPN-negotiating connection ready and is about to finalize it (hand it
    //! over to a concrete connection); while this point is armed, the request waits for its connection to be ready
    //! first, so that it reaches the point
    Finalize,
    //! NegotiatingHttpClientConnection::takeOver() checked the connection and holds its method guard, before it hands
    //! the socket over to the concrete connection
    Takeover,
    //! HttpClientConnectionBase::drainInFlight() has to wait for a call in flight (armed with a release Counter of 0,
    //! the point only reports that the close waits)
    DrainWait,
    //! HttpClientConnectionManagerBase::swapNegotiatedConnection() was given the concrete connection that took over a
    //! negotiating connection, before it reads the negotiating connection
    Swap,
    //! HttpClientConnectionManagerBase::acquireConnectionImpl() found a connection in its pool and released the pool
    //! lock, before it returns the connection
    Acquired,
    //! HttpClientConnectionBase::tryReservePendingStream() was called to reserve a stream of a Qore-level connection,
    //! before it takes the reservation lock
    ReservePending,
    //! an HTTP/1 or HTTP/3 client, HTTP/3 server, WebSocket server or client, or HTTP client ping poll operation
    //! records its first error: the thread holds the lock that protects the error and has not stored the error yet
    ErrorRecord,
    //! a read of the error of one of the poll operations of @ref ErrorRecord (getErrorInfo()) found the lock that
    //! protects it taken, and is about to wait for it (see QoreRecordedValueReadLocker)
    ErrorRead,
    //! an HTTP client ping poll operation stores its output, or a WebSocket server poll operation its close reason:
    //! the thread holds the lock that protects the value and has not stored it yet
    ValueRecord,
    //! a read of a value of @ref ValueRecord found the lock that protects it taken, and is about to wait for it (see
    //! QoreRecordedValueReadLocker)
    ValueRead,
    //! a thread that uses state that another thread can release in a teardown (the poll operation of an HTTP/3 client
    //! connection, read for its error; the inner operation of an HTTP client ping poll operation) has taken it, and is
    //! about to use it
    TeardownRead,
    //! an HTTP/1.1 or HTTP/3 client poll operation dispatched the end of a request's response and released the
    //! reference to the request's action that the dispatch held: on the I/O thread, or for an HTTP/3 response that
    //! arrived before its request was registered, on the thread that registered it (armed with a release Counter of
    //! 0, the point only reports it)
    StreamEndDispatched,
    //! an I/O thread of the asynchronous I/O controller is about to continue a C++ poll operation on the thread
    //! (call its continuePoll()); the point is armed for the operation's object
    ContinuePoll,
    //! an HTTP/3 client poll operation submitted a request and is about to register it (the submitting thread); the
    //! operation wakes its I/O thread first, so that the request is sent and its response can arrive before the
    //! request is registered
    Http3Register,
    //! an HTTP/3 client poll operation received the response of a request that is not registered yet and kept it for
    //! the registration (the I/O thread; armed with a release Counter of 0, the point only reports it)
    Http3ResponseBuffered,
    //! the address family race of an HTTP/3 client connection found the connection attempt that wins, before the
    //! operation's socket takes it over (the I/O thread)
    Http3RaceWinner,
    //! an HTTP/2 or HTTP/3 client connection's streamed send found the stream's send buffer full and is about to wait
    //! for it to drain (the sending thread; qore#7465)
    SendDrainWait,
    //! the number of hold points
    Count,
};

//! holds the calling thread at the given point if dbg_http_hold_at() armed it (one-shot)
/** @param point the hold point
    @param target the object that reaches the point (the Qore object of a poll operation), if any: a point armed for
    an object holds only the thread that reaches it for that object
*/
DLLLOCAL void qore_dbg_http_hold(QoreDbgHttpHoldPoint point, const void* target = nullptr);

//! true while dbg_http_hold_at() has armed QoreDbgHttpHoldPoint::ContinuePoll and no thread has reached it yet
/** Read before the hold point, which is on a path that runs for each step of a poll operation, so that the path takes
    no lock while the point is not armed
*/
DLLLOCAL extern std::atomic<bool> qore_dbg_continue_poll_hold_armed;

//! holds the creation of a child Program after its options were checked and before the Program is created, if
//! dbg_hold_child_program_create() armed it (one-shot)
DLLLOCAL void qore_dbg_child_program_create_hold();

//! returns true if dbg_http_hold_at() armed the given point and no thread has reached it yet
DLLLOCAL bool qore_dbg_http_hold_armed(QoreDbgHttpHoldPoint point);

//! records that an HTTP client connection's closeConnection() runs, for dbg_http_concurrent_closes(); destroying it
//! records the end of the close
class QoreDbgHttpCloseScope {
public:
    DLLLOCAL QoreDbgHttpCloseScope(const void* conn);
    DLLLOCAL ~QoreDbgHttpCloseScope();

private:
    const void* conn;
};

//! called by AbstractHttpPollConnectionPriv::setClosed() on the first close of a connection, holding a reference to
//! it and no lock: once after the connection has been marked closed and its waiters woken, before the close hook
//! (\a after false), and once after the close hook (\a after true); set by unit tests
typedef void (*qore_dbg_conn_close_notify_hook_t)(AbstractHttpPollConnectionPriv* conn, bool after);
DLLLOCAL extern std::atomic<qore_dbg_conn_close_notify_hook_t> qore_dbg_conn_close_notify_hook;

//! called by AbstractHttpPollConnectionPriv::setClosed() after it released its reference when
//! qore_dbg_conn_close_release_hook released one before
DLLLOCAL void qore_dbg_conn_close_released();

//! the number of times the next c-ares lookups report their query as lost; set by dbg_cares_lose_query()
DLLLOCAL extern std::atomic<int> qore_dbg_cares_lost_query_count;

//! Sets the DNS servers that the c-ares channels made from now on ask, as for ares_set_servers_ports_csv()
/** @param servers the servers, such as \c "127.0.0.1:5353"; empty for the system's servers

    @return the previous setting
*/
DLLLOCAL std::string qore_dbg_cares_set_servers(const std::string& servers);

//! The query options of the c-ares channels that ask the DNS servers of qore_dbg_cares_set_servers()
struct QoreDbgCaresQueryOptions {
    //! the timeout of the first try of a query; c-ares (1.32 and later) caps it with its maximum timeout, which the
    //! channels are given as the greater of this timeout and c-ares's default maximum (5 s), and it takes a timeout
    //! from the latency of a server once it has a few answers from it (five times the average latency, at least
    //! 250 ms; qore#7119)
    int timeout_ms = 1000;
    //! the tries of a query
    int tries = 3;
    //! set if the channels look up names with the search domains in @ref domains, not those of the host
    bool domains_set = false;
    //! the search domains, if @ref domains_set
    std::vector<std::string> domains;
    //! the \c ndots option, or -1 for that of the host
    int ndots = -1;
    //! false to make the channels keep no answer in their query cache
    bool query_cache = true;
};

//! Sets the query options of the c-ares channels made from now on that ask the servers of
//! qore_dbg_cares_set_servers(); returns the previous options
DLLLOCAL QoreDbgCaresQueryOptions qore_dbg_cares_set_query_options(const QoreDbgCaresQueryOptions& opts);

//! A synchronous transport for the c-ares channels that ask the DNS servers of qore_dbg_cares_set_servers()
/** While set, each UDP socket that such a channel makes is one end of a Unix-domain datagram socketpair, and the other
    end is given to @ref peer, which owns it from then on: a query is at the other end when c-ares's send returns, and
    an answer written there can be read by c-ares at once, on every platform (UDP over the loopback interface is
    delivered by an input thread on macOS, and a delivery delayed beyond the timeout of a query made a lookup of a
    test time out; qore#7119).  c-ares sees the address that it connected the socket to as the peer of the socket.
*/
struct QoreDbgCaresLocalTransport {
    //! called with the other end of each socketpair; it must not throw
    void (*peer)(int fd, void* arg) = nullptr;
    //! the argument of @ref peer
    void* arg = nullptr;
};

//! Sets the transport of the c-ares channels made from now on that ask the servers of qore_dbg_cares_set_servers()
/** @param transport the transport; one with no @ref QoreDbgCaresLocalTransport::peer "peer" for UDP sockets

    @return the previous setting
*/
DLLLOCAL QoreDbgCaresLocalTransport qore_dbg_cares_set_local_transport(const QoreDbgCaresLocalTransport& transport);

//! Sets the longest wait of a thread that reaches an end point of the process while the process-end callbacks run on
//! another thread (qore#7191)
DLLLOCAL void qore_dbg_process_end_set_wait_ms(int64 ms);

//! Waits until the process-end callbacks have started to run, at most \a timeout_ms; returns true if they have
DLLLOCAL bool qore_dbg_process_end_wait_started(int64 timeout_ms);

//! Waits until a thread waits for the process-end callbacks that run, at most \a timeout_ms; returns true if one does
DLLLOCAL bool qore_dbg_process_end_wait_waiter(int64 timeout_ms);

//! A registry of process-end callbacks for the unit tests, with its own process ID and wait bound; see
//! qore_register_process_end_callback()
DLLLOCAL void* qore_dbg_process_end_cbs_new(int64 wait_ms);
DLLLOCAL void qore_dbg_process_end_cbs_delete(void* cbs);
DLLLOCAL int64 qore_dbg_process_end_cbs_add(void* cbs, qore_process_end_callback_t cb, void* arg, pid_t pid);
DLLLOCAL int qore_dbg_process_end_cbs_remove(void* cbs, int64 id);
DLLLOCAL bool qore_dbg_process_end_cbs_run(void* cbs, qore_process_end_e reason, pid_t pid);
DLLLOCAL bool qore_dbg_process_end_cbs_wait_started(void* cbs, int64 timeout_ms);
DLLLOCAL bool qore_dbg_process_end_cbs_wait_waiter(void* cbs, int64 timeout_ms);

//! Sets the hosts file that the name lookups started from now on read, as with the c-ares ARES_OPT_HOSTS_FILE option
/** @param path the hosts file; empty for the system's hosts file

    @return the previous setting
*/
DLLLOCAL std::string qore_dbg_cares_set_hosts_file(const std::string& path);

//! returns true if the current thread holds the environment lock (not the environment change lock); see
//! SystemEnvironment.cpp
DLLLOCAL bool qore_dbg_env_lock_held_by_current_thread();

//! called by the fork handler of the environment lock before it waits for the lock; see dbg_fork_lock_cycle_arm()
DLLLOCAL void qore_dbg_fork_env_prepare();

//! called while a c-ares channel is created, with the hold that the creation takes; returns true if the creation
//! must fail without creating the channel; see dbg_fork_lock_cycle_arm()
DLLLOCAL bool qore_dbg_fork_cares_channel_setup();

//! returns true if the current thread is the writer of dbg_env_read_race_arm()
DLLLOCAL bool qore_dbg_env_change_probe_writer();

//! called when the writer of dbg_env_read_race_arm() is about to wait for the environment change lock
DLLLOCAL void qore_dbg_env_change_waits();

//! called when the writer of dbg_env_read_race_arm() has changed the environment and released the locks
DLLLOCAL void qore_dbg_env_change_done();

//! called by a c-ares lookup right before c-ares starts it and reads HOSTALIASES; see dbg_env_read_race_arm()
DLLLOCAL void qore_dbg_cares_lookup_env_read();

//! called by qore_queue_private::push() with the queue's lock held, after a value pushed with the queue's object has
//! been counted for the collector and before the lock is released, with the queue's object; set by the C++ unit tests
typedef void (*qore_dbg_queue_push_locked_hook_t)(const QoreObject* self);
DLLLOCAL extern std::atomic<qore_dbg_queue_push_locked_hook_t> qore_dbg_queue_push_locked_hook;
#endif

inline QoreRecordedValueReadLocker::QoreRecordedValueReadLocker(QoreThreadLock& lck, const void* target, bool value)
        : lck(lck) {
#ifdef DEBUG
    if (lck.trylock()) {
        qore_dbg_http_hold(value ? QoreDbgHttpHoldPoint::ValueRead : QoreDbgHttpHoldPoint::ErrorRead, target);
        lck.lock();
    }
#else
    (void)target;
    (void)value;
    lck.lock();
#endif
}

#endif

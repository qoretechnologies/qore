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

#include <atomic>
#include <string>
#include <vector>

DLLLOCAL void init_debug_functions(QoreNamespace& qns);

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

typedef void (*qore_dbg_closure_var_decide_hook_t)(const char* name);
DLLLOCAL extern std::atomic<qore_dbg_closure_var_decide_hook_t> qore_dbg_closure_var_decide_hook;

//! called by ClosureVarValue::deref() when a dereference collects a closure-bound variable with its recursive set;
//! set by dbg_count_closure_var_collections()
typedef void (*qore_dbg_closure_var_collect_hook_t)(const char* name);
DLLLOCAL extern std::atomic<qore_dbg_closure_var_collect_hook_t> qore_dbg_closure_var_collect_hook;

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

//! the number of TLS close_notify alerts sent by SSLSocketHelper::sendCloseNotify() (SSL_shutdown() calls); read by
//! dbg_ssl_close_notify_count()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_ssl_close_notify_count;

//! the number of sets of references of synchronous operations that their callers took (AsyncIoSyncRelease) instead of
//! a callback worker; read by dbg_async_io_sync_release_counts()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_sync_release_taken;

//! the number of sets of references of synchronous operations that their callers refused, as their waits had ended,
//! and that went to a callback worker; read by dbg_async_io_sync_release_counts()
DLLLOCAL extern std::atomic<int64_t> qore_dbg_sync_release_refused;

//! holds the thread that converts the end of a synchronous socket call's operation (canceled, or ended by a close of
//! its socket) into an error, before it checks the thread's pending cancellation, when dbg_hold_socket_cancel_conversion()
//! has armed the hold; see qore_socket_raise_pending_cancel()
DLLLOCAL void qore_dbg_socket_cancel_conversion();

//! called by SocketSendPollOperation::continuePoll() with the socket's lock held, before it writes; \a fd is the
//! socket's descriptor; set by dbg_http1_send_failure_arm()
typedef void (*qore_dbg_socket_send_hook_t)(int fd);
DLLLOCAL extern std::atomic<qore_dbg_socket_send_hook_t> qore_dbg_socket_send_hook;

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
};

//! holds the calling thread at the given point if dbg_http_hold_at() armed it (one-shot)
DLLLOCAL void qore_dbg_http_hold(QoreDbgHttpHoldPoint point);

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
    //! the timeout of the first try of a query
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

#endif

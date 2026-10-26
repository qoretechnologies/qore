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

//! called by AbstractHttpPollConnectionPriv::setClosed() on the first close of a connection, after the close hook and
//! before it releases the reference that it holds while running the hook; returns true if the hook released a
//! reference of its own, in which case qore_dbg_conn_close_released() is called once setClosed() has released its
//! reference; set by dbg_release_http1_connection_on_close()
typedef bool (*qore_dbg_conn_close_release_hook_t)(AbstractHttpPollConnectionPriv* conn);
DLLLOCAL extern std::atomic<qore_dbg_conn_close_release_hook_t> qore_dbg_conn_close_release_hook;

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

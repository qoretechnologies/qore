/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SqlMutationContext.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Datasource/pool-scoped structured mutation observer support; see
    design/datasource-mutation-observer.md

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

#ifndef _QORE_SQLMUTATIONCONTEXT_H

#define _QORE_SQLMUTATIONCONTEXT_H

#include <qore/QoreReferenceCounter.h>
#include <qore/QoreThreadLock.h>

#include <atomic>
#include <map>
#include <string>
#include <vector>

/** @defgroup sql_mutation_event_codes SQL mutation observer event codes

    Values for the \c "event" key of \c SqlMutationEventInfo.  Event codes are stable; new codes may
    be added in the future, so observers must ignore unknown codes.
*/
///@{
//! a transaction has been started on the datasource
#define SQL_MUTATION_EVENT_TX_BEGIN         1
//! an operation is about to be executed; this is an admission point
#define SQL_MUTATION_EVENT_PRE_EXEC         2
//! an operation has been executed
#define SQL_MUTATION_EVENT_POST_EXEC        3
//! a transaction outcome is known
#define SQL_MUTATION_EVENT_OUTCOME          4
//! a bounded stream is about to start; this is an admission point
#define SQL_MUTATION_EVENT_STREAM_BEGIN     5
//! a bounded stream has consumed more bytes; this is an admission point
#define SQL_MUTATION_EVENT_STREAM_PROGRESS  6
//! a bounded stream has finished
#define SQL_MUTATION_EVENT_STREAM_END       7
///@}

/** @defgroup sql_stmt_class_codes SQL statement class codes

    Values for the \c "stmt_class" key of \c SqlMutationEventInfo.  The statement class is derived
    structurally from the core API used; no SQL text is ever inspected.
*/
///@{
//! a read operation: select, selectRow, selectRows, the typed and columnar variants, or describe
#define SQL_STMT_CLASS_READ         1
//! a mutating operation issued with Datasource::exec() or Datasource::execRaw()
#define SQL_STMT_CLASS_EXEC         2
//! a mutating operation issued with a prepared SQLStatement
#define SQL_STMT_CLASS_STMT_EXEC    3
//! a transaction start
#define SQL_STMT_CLASS_BEGIN        4
//! a commit, including an autocommit
#define SQL_STMT_CLASS_COMMIT       5
//! a rollback
#define SQL_STMT_CLASS_ROLLBACK     6
//! a bounded stream operation
#define SQL_STMT_CLASS_STREAM       7
///@}

/** @defgroup sql_mutation_outcome_codes SQL mutation observer outcome codes

    Values for the \c "outcome" key of \c SqlMutationEventInfo.  All outcomes are derived from
    control flow; no error text is ever matched.
*/
///@{
//! the operation completed and the transaction remains open
#define SQL_MUTATION_OUTCOME_OK                 1
//! the driver raised an exception and the connection is intact
#define SQL_MUTATION_OUTCOME_ERROR              2
//! the transaction was committed and the commit is known to have succeeded
#define SQL_MUTATION_OUTCOME_COMMIT             3
//! a commit was attempted but its result is unknown; this is never reported as a rollback
#define SQL_MUTATION_OUTCOME_COMMIT_AMBIGUOUS   4
//! the transaction was rolled back
#define SQL_MUTATION_OUTCOME_ROLLBACK           5
//! a rollback was attempted and failed; no commit was attempted
#define SQL_MUTATION_OUTCOME_ROLLBACK_ERROR     6
//! the connection was lost or aborted with no commit in flight
#define SQL_MUTATION_OUTCOME_LOST_CONNECTION    7
//! the operation was rejected at an admission point and was not executed
#define SQL_MUTATION_OUTCOME_NOT_EXECUTED       8
///@}

/** @defgroup sql_mutation_effect_codes SQL mutation declared effect codes

    Values for the \c "effect" key of \c SqlMutationInfo.  This is a closed set; the core assigns no
    policy meaning to any of these values.
*/
///@{
//! the operation cannot increase storage
#define SQL_MUTATION_EFFECT_NONE            0
//! the operation may increase storage, bounded by \c "max_growth_bytes" if declared
#define SQL_MUTATION_EFFECT_MAY_GROW        1
//! the operation only reclaims storage
#define SQL_MUTATION_EFFECT_RECLAIM_ONLY    2
///@}

/** @defgroup sql_mutation_mask_codes SQL mutation observer event mask codes

    Values for the event mask passed when registering an observer.
*/
///@{
//! transaction start and transaction outcome events
#define SQL_MUTATION_MASK_TX        (1 << 0)
//! pre- and post-execution events for mutating statements
#define SQL_MUTATION_MASK_EXEC      (1 << 1)
//! pre- and post-execution events for read operations
#define SQL_MUTATION_MASK_READ      (1 << 2)
//! bounded stream events
#define SQL_MUTATION_MASK_STREAM    (1 << 3)
//! the default mask: transactions, mutating statements, and streams, but not reads
#define SQL_MUTATION_MASK_DEFAULT   (SQL_MUTATION_MASK_TX | SQL_MUTATION_MASK_EXEC \
                                        | SQL_MUTATION_MASK_STREAM)
//! all events
#define SQL_MUTATION_MASK_ALL       (SQL_MUTATION_MASK_TX | SQL_MUTATION_MASK_EXEC \
                                        | SQL_MUTATION_MASK_READ | SQL_MUTATION_MASK_STREAM)
///@}

//! the default error code raised when an observer rejects an operation
#define SQL_MUTATION_REJECTED_ERR "SQL-MUTATION-REJECTED"
//! the error code raised for an invalid mutation declaration
#define SQL_MUTATION_DECLARATION_ERR "SQL-MUTATION-DECLARATION-ERROR"

#ifdef DEBUG
/** @defgroup sql_mutation_debug_fault_codes SQL mutation debug fault boundary codes

    Debug builds only.  Values selecting the boundary at which an armed connection abort is
    triggered; see dbg_ds_arm_connection_abort() in lib/ql_debug.cpp.
*/
///@{
//! no fault is armed
#define SQL_MUTATION_FAULT_NONE             0
//! abort the connection after the statement has been executed and before any commit
/** the operation is reported with \c LOST_CONNECTION and \c replay_safe \c True
*/
#define SQL_MUTATION_FAULT_AFTER_EXEC       1
//! abort the connection while a commit is in flight
/** the transaction is reported with \c COMMIT_AMBIGUOUS, since the core cannot know whether the
    server applied the commit
*/
#define SQL_MUTATION_FAULT_ON_COMMIT        2
///@}

//! the error code raised by an armed debug connection abort
#define SQL_MUTATION_DEBUG_ABORT_ERR "DBI:DBG:CONNECTION-ABORTED"
#endif

class Datasource;
class ResolvedCallReferenceNode;

//! a single mutation observer event; all fields are supplied by the core
struct SqlMutationEvent {
    //! @ref sql_mutation_event_codes
    int event = 0;
    //! @ref sql_stmt_class_codes
    int stmt_class = 0;
    //! @ref sql_mutation_outcome_codes; 0 = not an outcome event
    int outcome = 0;
    //! -1 = not applicable, 0 = false, 1 = true
    int replay_safe = -1;
    //! -1 = not applicable
    int64 declared_bytes = -1;
    //! -1 = not applicable
    int64 consumed_bytes = -1;
    //! transaction identity assigned by the core
    const char* tx_id = nullptr;
    //! sequence of this event within the transaction
    int64 tx_seq = 0;
    //! transaction state at the boundary
    bool in_transaction = false;
    //! autocommit state of the datasource
    bool autocommit = false;
    //! if non-null and holding an exception, the driver exception reported to the observer
    /** the exception is not consumed and is not modified
    */
    ExceptionSink* driver_xsink = nullptr;

    DLLLOCAL SqlMutationEvent(int event, int stmt_class) : event(event), stmt_class(stmt_class) {
    }

    //! returns the event mask bit that gates this event
    DLLLOCAL int64 getMaskBit() const;

    //! returns true if this event is an admission point
    DLLLOCAL bool isAdmission() const {
        return event == SQL_MUTATION_EVENT_PRE_EXEC
            || event == SQL_MUTATION_EVENT_STREAM_BEGIN
            || event == SQL_MUTATION_EVENT_STREAM_PROGRESS;
    }
};

//! datasource- or pool-scoped mutation observer and declaration state
/** One context is shared by a DatasourcePool and all of its pooled Datasource objects, which is
    what allows a per-thread declaration to survive connection allocation and release within a
    transaction, and what keeps two pools in the same process independent.

    The context is created lazily; qore_ds_private::mutation_ctx is nullptr until an observer or a
    declaration is registered, so unmonitored datasources cost one null pointer test per operation.

    All public methods are thread-safe.
*/
class SqlMutationContext {
public:
    DLLLOCAL SqlMutationContext() = default;

    DLLLOCAL ~SqlMutationContext();

    //! returns a new reference to this object
    DLLLOCAL SqlMutationContext* refSelf() const {
        refs.ROreference();
        return const_cast<SqlMutationContext*>(this);
    }

    //! dereferences the object and deletes it when the last reference is removed
    DLLLOCAL void deref(ExceptionSink* xsink);

    //! returns true if an observer is currently registered
    /** this is the fast-path check; it performs a single relaxed atomic load
    */
    DLLLOCAL bool hasObserver() const {
        return observer_active.load(std::memory_order_relaxed);
    }

    //! returns true if an observer is registered and the given mask bit is selected
    DLLLOCAL bool observes(int64 mask_bit) const {
        return observer_active.load(std::memory_order_relaxed)
            && (event_mask.load(std::memory_order_relaxed) & mask_bit);
    }

    //! registers an observer; takes ownership of the callback reference and the argument
    DLLLOCAL void setObserver(ResolvedCallReferenceNode* cb, int64 mask, QoreValue arg, ExceptionSink* xsink);

    //! removes any registered observer
    DLLLOCAL void clearObserver(ExceptionSink* xsink);

    //! validates and pushes a declaration for the current thread
    /** @return 0 for OK, -1 if the declaration is invalid, in which case an exception has been
        raised in \a xsink and nothing has been pushed
    */
    DLLLOCAL int pushDeclaration(const QoreHashNode* info, ExceptionSink* xsink);

    //! pops the innermost declaration for the current thread
    /** @return 0 for OK, -1 if there was no declaration to pop, in which case an exception has been
        raised in \a xsink
    */
    DLLLOCAL int popDeclaration(ExceptionSink* xsink);

    //! returns a new reference to the innermost declaration for the current thread or nullptr
    DLLLOCAL QoreHashNode* getDeclaration() const;

    //! returns a new unique transaction identity for this context
    DLLLOCAL void getNewTransactionId(std::string& tx_id);

#ifdef DEBUG
    //! arms a one-shot connection abort for the operation with the given \c "op_id"
    /** Debug builds only; exposed to %Qore as dbg_ds_arm_connection_abort() so that a test can
        produce a structural connection loss at an exact operation boundary on a real driver,
        deterministically and with no sleep, poll, retry, or randomness.

        The arming lives on the context rather than on the connection because a DatasourcePool
        shares one context with every connection it pools: a test arms the pool without knowing
        which connection its thread will be allocated.

        @param op_id the exact declared operation identity to abort on
        @param when the boundary; see @ref sql_mutation_debug_fault_codes
    */
    DLLLOCAL void dbgArmFault(const char* op_id, int when);

    //! returns true if the arming matches \a op_id and \a when, consuming it
    /** The arming is one-shot, so a test can never see a second abort it did not ask for, and it is
        consumed only by the exact boundary it names: an operation passes several boundaries, so
        matching on the operation alone would let an earlier boundary swallow an arming meant for a
        later one.

        @param op_id the declared operation identity at the boundary being passed
        @param when the boundary being passed; see @ref sql_mutation_debug_fault_codes
    */
    DLLLOCAL bool dbgTakeArmedFault(const char* op_id, int when);

    //! returns true if any fault is armed
    DLLLOCAL bool dbgFaultArmed() const {
        return dbg_fault_armed.load(std::memory_order_relaxed);
    }
#endif

    //! delivers an event to the observer
    /** The observer is called with a clean ExceptionSink; anything it raises is assimilated into
        \a xsink afterwards, so that a driver exception already present in \a xsink is always
        reported before an observer exception, and so that an observer never sees a partially
        unwound exception state.

        @return 0 to continue; -1 if an admission event was rejected or raised an exception, in
        which case the caller must not execute the operation.  Notification events always return 0:
        an observer can never change an outcome that has already happened.
    */
    DLLLOCAL int dispatch(const SqlMutationEvent& event, const Datasource& ds, ExceptionSink* xsink);

private:
    //! per-thread declaration stack and re-entrancy depth
    struct ThreadState {
        typedef std::vector<QoreHashNode*> decl_stack_t;

        decl_stack_t decls;
        //! non-zero while an observer callback is running in this thread
        int depth = 0;

        DLLLOCAL bool empty() const {
            return decls.empty() && !depth;
        }
    };

    typedef std::map<int, ThreadState> tmap_t;

    mutable QoreThreadLock m;
    mutable QoreReferenceCounter refs;

    ResolvedCallReferenceNode* observer = nullptr;
    QoreValue observer_arg;

    //! fast-path flags; only ever written with the lock held
    std::atomic<bool> observer_active{false};
    std::atomic<bool> decl_active{false};
    std::atomic<int64> event_mask{0};

    //! transaction identity counter for this context
    std::atomic<int64> tx_counter{0};

    //! process-unique identity for this context, assigned lazily; 0 = not yet assigned
    mutable std::atomic<int64> ctx_id{0};

#ifdef DEBUG
    //! the operation identity the armed fault applies to; empty = no fault armed
    std::string dbg_fault_op_id;
    //! the armed boundary; see @ref sql_mutation_debug_fault_codes
    int dbg_fault_when = SQL_MUTATION_FAULT_NONE;
    //! fast-path flag; only ever written with the lock held
    std::atomic<bool> dbg_fault_armed{false};
#endif

    tmap_t tmap;

    //! removes the thread state entry if it is empty; called with the lock held
    DLLLOCAL void pruneThreadIntern(tmap_t::iterator i) {
        if (i->second.empty()) {
            tmap.erase(i);
            if (tmap.empty()) {
                decl_active.store(false, std::memory_order_relaxed);
            }
        }
    }

    //! validates a declaration hash; returns 0 for OK, -1 with an exception raised
    DLLLOCAL static int checkDeclaration(const QoreHashNode* info, ExceptionSink* xsink);

    //! decrements the observer re-entrancy depth for the given thread
    DLLLOCAL void endDispatch(int tid);

    //! returns the process-unique identity of this context, assigning it on first use
    DLLLOCAL int64 getContextId() const;

    SqlMutationContext(const SqlMutationContext&) = delete;
    SqlMutationContext& operator=(const SqlMutationContext&) = delete;
};

#endif // _QORE_SQLMUTATIONCONTEXT_H

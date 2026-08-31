/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_ds_private.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    The Datasource class provides the low-level interface to Qore DBI drivers.

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

#ifndef _QORE_DS_PRIVATE_H

#define _QORE_DS_PRIVATE_H

#include "qore/intern/qore_dbi_private.h"
#include "qore/intern/QoreSQLStatement.h"
#include "qore/intern/DatasourceStatementHelper.h"
#include "qore/intern/SqlMutationContext.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

typedef std::set<QoreSQLStatement*> stmt_set_t;
typedef std::vector<std::pair<std::string, const QoreTypeInfo*>> qore_dbi_typed_result_members_t;

struct qore_ds_private {
    // mutex
    mutable QoreThreadLock m;

    Datasource* ds;

    bool in_transaction = false;
    bool active_transaction = false;
    bool isopen = false;
    bool autocommit = false;
    bool connection_aborted = false;
    bool keep_lock = false;
    //! true while a driver commit call is in progress; used to derive commit ambiguity structurally
    bool commit_in_progress = false;
    //! set when the driver reports a lost or aborted connection; flushed at the next core boundary
    bool mutation_conn_lost = false;
    //! true while the driver owns an active native bulk-load protocol session
    bool bulk_load_active = false;

    mutable DBIDriver* dsl;
    const QoreEncoding* qorecharset = QCS_DEFAULT;
    void* private_data = nullptr;               // driver private data per connection

    // for pending connection values
    std::string p_username,
        p_password,
        p_dbname,
        p_db_encoding, // database-specific name for the encoding for the connection
        p_hostname;
    int p_port = 0;       // pending port number (0 = default port)

    // actual connection values set by init() before the datasource is opened
    std::string username,
        password,
        db_encoding,   // database-specific name for the encoding for the connection
        dbname,
        hostname;
    int port = 0; // port number (0 = default port)

    // options per connection
    QoreHashNode* opt;
    // DBI event queue
    Queue* event_queue = nullptr;
    // DBI Event queue argument
    QoreValue event_arg{};

    // mutation observer context, shared with the owning pool if any; nullptr = unmonitored
    /** Atomic because an observer can be registered on a DatasourcePool or a ManagedDatasource
        while other threads are executing statements on connections that already exist: the
        registration runs under the pool or datasource lock, but statement execution does not hold
        that lock, so readers are not serialized against the store.  See
        design/datasource-mutation-observer.md.

        The pointer is only ever replaced while it is null (lazy creation, and inheritance by a
        connection that has not yet been handed out), so a reader that observes a non-null value
        always observes a live, fully-constructed, already-referenced context.
    */
    std::atomic<SqlMutationContext*> mutation_ctx{nullptr};
    // transaction identity assigned when the first observer event of a transaction is delivered
    std::string tx_id;
    // event sequence within the current transaction
    int64 tx_seq = 0;
    // declared size in bytes of the bounded stream in progress; -1 = no stream in progress
    int64 stream_declared_bytes = -1;
    //! set when a bounded stream admission point rejected the stream in progress
    /** it distinguishes a stream that the consumer stopped from one that the driver failed, so that
        the terminal stream event can report the exact outcome; see
        design/datasource-mutation-observer.md
    */
    bool stream_rejected = false;

    // interface for the parent class
    DatasourceStatementHelper* dsh;
    std::map<std::string, const TypedHashDecl*> typed_result_hashdecl_cache;

    DLLLOCAL qore_ds_private(Datasource* n_ds, DBIDriver* ndsl, DatasourceStatementHelper* dsh) : ds(n_ds),
            dsl(ndsl), opt(new QoreHashNode(autoTypeInfo)), dsh(dsh) {
    }

    DLLLOCAL qore_ds_private(const qore_ds_private& old, Datasource* n_ds, DatasourceStatementHelper* dsh) :
            ds(n_ds), autocommit(old.autocommit), dsl(old.dsl),
            p_username(old.p_username), p_password(old.p_password),
            p_dbname(old.p_dbname), p_db_encoding(old.p_db_encoding),
            p_hostname(old.p_hostname), p_port(old.p_port),
            //opt(old.opt->copy()) {
            opt(old.getCurrentOptionHash(true)),
            event_queue(old.event_queue ? old.event_queue->queueRefSelf() : nullptr),
            event_arg(old.event_arg.refSelf()),
            mutation_ctx(old.getMutationCtx() ? old.getMutationCtx()->refSelf() : nullptr),
            dsh(dsh) {
    }

    DLLLOCAL ~qore_ds_private() {
        assert(!private_data);
        assert(stmt_set.empty());
        clearTypedResultHashDeclCache();
        ExceptionSink xsink;
        if (opt) {
            opt->deref(&xsink);
        }
        event_arg.discard(&xsink);
        if (event_queue) {
            event_queue->deref(&xsink);
        }
        if (SqlMutationContext* ctx = getMutationCtx()) {
            ctx->deref(&xsink);
        }
    }

    //! returns the mutation observer context or nullptr; this is the fast-path accessor
    DLLLOCAL SqlMutationContext* getMutationCtx() const {
        return mutation_ctx.load(std::memory_order_acquire);
    }

    DLLLOCAL void setPendingConnectionValues(const qore_ds_private* other) {
        p_username    = other->p_username;
        p_password    = other->p_password;
        p_dbname      = other->p_dbname;
        p_hostname    = other->p_hostname;
        p_db_encoding = other->p_db_encoding;
        autocommit    = other->autocommit;
        p_port        = other->p_port;
    }

    DLLLOCAL void setConnectionValues() {
        dbname      = p_dbname;
        username    = p_username;
        password    = p_password;
        hostname    = p_hostname;
        db_encoding = p_db_encoding;
        port        = p_port;
    }

    DLLLOCAL void statementExecuted(int rc);

    DLLLOCAL void clearTypedResultHashDeclCache();

    DLLLOCAL const TypedHashDecl* getTypedResultHashDecl(const char* driver_name, const std::string& signature,
            const qore_dbi_typed_result_members_t& members, ExceptionSink* xsink);

    DLLLOCAL void copyOptions(const Datasource* ods);

    DLLLOCAL void setOption(const char* name, QoreValue v, ExceptionSink* xsink) {
        opt->setKeyValue(name, v.refSelf(), xsink);
    }

    DLLLOCAL QoreHashNode* getOptionHash() const {
        return private_data ? qore_dbi_private::get(*dsl)->getOptionHash(ds) : opt->hashRefSelf();
    }

    DLLLOCAL QoreHashNode* getCurrentOptionHash(bool ensure_hash = false) const;

    DLLLOCAL QoreHashNode* getConfigHash() const;

    DLLLOCAL QoreStringNode* getConfigString() const;

    DLLLOCAL void setEventQueue(Queue* q, QoreValue arg, ExceptionSink* xsink) {
        if (event_queue)
            event_queue->deref(xsink);
        event_arg.discard(xsink);
        event_queue = q;
        event_arg = arg;
    }

    DLLLOCAL QoreHashNode* getEventQueueHash(Queue*& q, int event_code) const {
        q = event_queue;
        if (!q)
            return nullptr;
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);
        if (!username.empty())
            h->setKeyValue("user", new QoreStringNode(username), 0);
        if (!dbname.empty())
            h->setKeyValue("db", new QoreStringNode(dbname), 0);
        h->setKeyValue("eventtype", event_code, 0);
        if (event_arg)
            h->setKeyValue("arg", event_arg.refSelf(), 0);
        return h;
    }

    //! returns true if an observer is registered and wants events for the given mask bit
    /** this is the fast path for all mutation observer hooks; when no context has been created it
        is a single predictable null pointer test
    */
    DLLLOCAL bool observes(int64 mask_bit) const {
        SqlMutationContext* ctx = getMutationCtx();
        return ctx && ctx->observes(mask_bit);
    }

    //! creates the mutation context if it does not yet exist; callers must serialize access
    DLLLOCAL SqlMutationContext* getOrCreateMutationContext() {
        SqlMutationContext* ctx = getMutationCtx();
        if (!ctx) {
            ctx = new SqlMutationContext;
            mutation_ctx.store(ctx, std::memory_order_release);
        }
        return ctx;
    }

    //! replaces the mutation context; takes ownership of the reference passed
    DLLLOCAL void setMutationContext(SqlMutationContext* ctx, ExceptionSink* xsink) {
        SqlMutationContext* old = getMutationCtx();
        if (old == ctx) {
            if (ctx) {
                ctx->deref(xsink);
            }
            return;
        }
        mutation_ctx.store(ctx, std::memory_order_release);
        if (old) {
            old->deref(xsink);
        }
    }

    //! delivers an observer event; fills in the datasource-derived fields first
    /** @return 0 to continue, -1 if an admission event was rejected

        @note the transaction identity is assigned lazily here, so that a transaction that produced
        no observable event never consumes an identity and never reports a terminal outcome
    */
    DLLLOCAL int dispatchMutationEvent(SqlMutationEvent& ev, ExceptionSink* xsink) {
        SqlMutationContext* ctx = getMutationCtx();
        assert(ctx);
        if (!ctx->observes(ev.getMaskBit())) {
            return 0;
        }
        if (tx_id.empty()) {
            ctx->getNewTransactionId(tx_id);
            tx_seq = 0;
        }
        ev.tx_id = tx_id.c_str();
        ev.tx_seq = ++tx_seq;
        ev.in_transaction = in_transaction;
        ev.autocommit = autocommit;
        int rc = ctx->dispatch(ev, *ds, xsink);
        if (rc && ev.isAdmission()) {
            dispatchMutationNotExecuted(ev, xsink);
        }
        return rc;
    }

    //! emits the terminal event for an operation that an admission point rejected
    /** The event closes the operation's lifetime for a consumer that tracks reservations by
        \c "op_id": without it a rejected operation would leave an unpaired admission event, and a
        rejection outside a transaction would produce no terminal event at all.

        It cannot be emitted from SqlMutationContext::dispatch() itself, because the re-entrancy
        depth that suppresses observer-issued operations is still held there; it is emitted here,
        after that depth has been released.

        The event is a notification: the operation has already been refused, so the return value is
        ignored.  The rejection exception is passed for reporting only and is not consumed.

        @param ev the rejected admission event
    */
    DLLLOCAL void dispatchMutationNotExecuted(const SqlMutationEvent& ev, ExceptionSink* xsink) {
        assert(ev.isAdmission());
        // a rejected stream in progress is closed by its producer's terminal event, which reports
        // the rejection through the "stream_rejected" flag; emitting here would duplicate it
        if (ev.event == SQL_MUTATION_EVENT_STREAM_PROGRESS) {
            stream_rejected = true;
            return;
        }
        if (ev.event == SQL_MUTATION_EVENT_STREAM_BEGIN) {
            // the producer never started the stream, so it emits no terminal event of its own
            stream_declared_bytes = -1;
            SqlMutationEvent term(SQL_MUTATION_EVENT_STREAM_END, SQL_STMT_CLASS_STREAM);
            term.declared_bytes = ev.declared_bytes;
            term.consumed_bytes = 0;
            term.outcome = SQL_MUTATION_OUTCOME_NOT_EXECUTED;
            // the operation was refused before execution, so no commit can have been attempted
            term.replay_safe = 1;
            term.driver_xsink = xsink;
            dispatchMutationEvent(term, xsink);
            return;
        }
        assert(ev.event == SQL_MUTATION_EVENT_PRE_EXEC);
        SqlMutationEvent term(SQL_MUTATION_EVENT_POST_EXEC, ev.stmt_class);
        term.outcome = SQL_MUTATION_OUTCOME_NOT_EXECUTED;
        term.replay_safe = 1;
        term.driver_xsink = xsink;
        dispatchMutationEvent(term, xsink);
    }

#ifdef DEBUG
    //! triggers an armed debug connection abort when the current operation matches it
    /** Debug builds only.  The abort goes through the ordinary Datasource::connectionAborted()
        path, so the connection is really closed and the server really discards the transaction;
        nothing about the resulting outcome is synthesized.  The exception is required: the
        connection_aborted paths assert that one is active.

        @param when the boundary being passed; see @ref sql_mutation_debug_fault_codes

        @return true if the connection was aborted, in which case an exception has been raised
    */
    DLLLOCAL bool dbgCheckArmedFault(int when, ExceptionSink* xsink) {
        SqlMutationContext* ctx = getMutationCtx();
        if (!ctx || !ctx->dbgFaultArmed()) {
            return false;
        }
        ReferenceHolder<QoreHashNode> decl(ctx->getDeclaration(), xsink);
        if (!decl) {
            return false;
        }
        QoreStringNodeValueHelper op_id(decl->getKeyValue("op_id"));
        if (!ctx->dbgTakeArmedFault(op_id->c_str(), when)) {
            return false;
        }
        xsink->raiseException(SQL_MUTATION_DEBUG_ABORT_ERR, "debug connection abort armed for op_id %s "
            "triggered at boundary %d", op_id->c_str(), when);
        connectionAborted(xsink);
        return true;
    }
#endif

    //! emits a transaction start event
    DLLLOCAL void dispatchMutationTxBegin(ExceptionSink* xsink) {
        if (!observes(SQL_MUTATION_MASK_TX)) {
            return;
        }
        SqlMutationEvent ev(SQL_MUTATION_EVENT_TX_BEGIN, SQL_STMT_CLASS_BEGIN);
        dispatchMutationEvent(ev, xsink);
    }

    //! emits a terminal transaction outcome event and ends the transaction identity
    DLLLOCAL void dispatchMutationOutcome(int stmt_class, int outcome, int replay_safe,
            ExceptionSink* driver_xsink, ExceptionSink* xsink) {
        mutation_conn_lost = false;
        if (observes(SQL_MUTATION_MASK_TX)) {
            SqlMutationEvent ev(SQL_MUTATION_EVENT_OUTCOME, stmt_class);
            ev.outcome = outcome;
            ev.replay_safe = replay_safe;
            ev.driver_xsink = driver_xsink;
            dispatchMutationEvent(ev, xsink);
        }
        // the next operation starts a new transaction identity
        tx_id.clear();
        tx_seq = 0;
    }

    //! reports a connection loss recorded by the driver at the next core boundary
    /** The event is not emitted from Datasource::connectionAborted()/connectionLost() because
        those are called by the driver from inside a driver call; emitting there would run observer
        code with driver state in flux and would report the transaction outcome before the result of
        the statement that triggered it.
    */
    DLLLOCAL void flushMutationConnectionLost(ExceptionSink* xsink) {
        if (!mutation_conn_lost) {
            return;
        }
        // no commit was in flight, so no commit can have been applied: the operation can be replayed
        dispatchMutationOutcome(SQL_STMT_CLASS_ROLLBACK, SQL_MUTATION_OUTCOME_LOST_CONNECTION, 1, xsink, xsink);
    }

    DLLLOCAL void addStatement(QoreSQLStatement* stmt) {
        AutoLocker al(m);
        assert(stmt_set.find(stmt) == stmt_set.end());
        stmt_set.insert(stmt);
    }

    DLLLOCAL void removeStatement(QoreSQLStatement* stmt) {
        AutoLocker al(m);
        stmt_set_t::iterator i = stmt_set.find(stmt);
        if (i != stmt_set.end())
            stmt_set.erase(i);
    }

    DLLLOCAL void connectionAborted(ExceptionSink* xsink) {
        assert(isopen);
        // close all statements and clear private data, leave datasource allocated
        transactionDone(false, true, xsink);
        // mark connection aborted
        connection_aborted = true;
        if (getMutationCtx()) {
            mutation_conn_lost = true;
        }
        // close the datasource
        close();
    }

    DLLLOCAL void connectionLost(ExceptionSink* xsink) {
#ifdef DEBUG
        // issue #4117: get backtrace if connectionLost() called while the connection is closed
        if (!isopen) {
            qore_machine_backtrace();
        }
#endif
        assert(isopen);
        // close statements but do not clear datasource or statements in the datasource
        transactionDone(false, false, xsink);
        if (getMutationCtx() && in_transaction) {
            mutation_conn_lost = true;
        }
    }

    DLLLOCAL void connectionRecovered(ExceptionSink* xsink) {
        assert(isopen);
        // close all statements, clear private data, leave datasource allocation
        transactionDone(false, true, xsink);
    }

    // @param clear if true then clears the statements' datasource ptrs and the stmt_set, if false, does not
    DLLLOCAL void transactionDone(bool clear, bool close, ExceptionSink* xsink) {
        AutoLocker al(m);
        for (stmt_set_t::iterator i = stmt_set.begin(), e = stmt_set.end(); i != e; ++i) {
            //printd(5, "qore_ds_private::transactionDone() this: %p stmt: %p clear: %d close: %d\n", this, *i, clear, close);
            (*i)->transactionDone(clear, close, xsink);
        }
        if (clear)
            stmt_set.clear();
    }

    DLLLOCAL int commitIntern(ExceptionSink* xsink) {
        //printd(5, "qore_ds_private::commitIntern() this: %p in_transaction: %d active_transaction: %d\n", this, in_transaction, active_transaction);
        assert(isopen);
        in_transaction = false;
        active_transaction = false;
        if (!getMutationCtx()) {
            return qore_dbi_private::get(*dsl)->commit(ds, xsink);
        }
        // the flag is what makes commit ambiguity structural: any failure or connection loss while
        // it is set means the core cannot know whether the server applied the commit
        commit_in_progress = true;
#ifdef DEBUG
        // an abort armed for the commit boundary is triggered with the flag set, so that it is
        // classified as ambiguous exactly as a real connection loss during a commit would be
        int rc = dbgCheckArmedFault(SQL_MUTATION_FAULT_ON_COMMIT, xsink)
            ? -1
            : qore_dbi_private::get(*dsl)->commit(ds, xsink);
        // This second debug-only boundary runs only after the driver has
        // demonstrably committed.  Closing the connection while
        // commit_in_progress is still set gives recovery tests a
        // deterministic durable COMMIT_AMBIGUOUS branch.
        if (!rc && !*xsink
                && dbgCheckArmedFault(SQL_MUTATION_FAULT_AFTER_COMMIT, xsink)) {
            rc = -1;
        }
#else
        int rc = qore_dbi_private::get(*dsl)->commit(ds, xsink);
#endif
        commit_in_progress = false;
        // a commit that did not demonstrably succeed is always ambiguous and is never reported as a
        // rollback: the server may have applied it
        const bool known = !rc && !*xsink && !connection_aborted;
        dispatchMutationOutcome(SQL_STMT_CLASS_COMMIT,
            known ? SQL_MUTATION_OUTCOME_COMMIT : SQL_MUTATION_OUTCOME_COMMIT_AMBIGUOUS, 0, xsink, xsink);
        return rc;
    }

    DLLLOCAL int rollbackIntern(ExceptionSink* xsink) {
        //printd(5, "qore_ds_private::rollbackIntern() this: %p in_transaction: %d active_transaction: %d\n", this, in_transaction, active_transaction);
        assert(isopen);
        in_transaction = false;
        active_transaction = false;
        if (!getMutationCtx()) {
            return qore_dbi_private::get(*dsl)->rollback(ds, xsink);
        }
        int rc = qore_dbi_private::get(*dsl)->rollback(ds, xsink);
        const bool known = !rc && !*xsink && !connection_aborted;
        // no commit was attempted either way, so the operation can always be replayed
        dispatchMutationOutcome(SQL_STMT_CLASS_ROLLBACK,
            known ? SQL_MUTATION_OUTCOME_ROLLBACK : SQL_MUTATION_OUTCOME_ROLLBACK_ERROR, 1, xsink, xsink);
        return rc;
    }

    DLLLOCAL int commit(ExceptionSink* xsink) {
        int rc = commitIntern(xsink);
        transactionDone(true, true, xsink);
        return rc;
    }

    DLLLOCAL int rollback(ExceptionSink* xsink) {
        int rc = rollbackIntern(xsink);
        transactionDone(true, true, xsink);
        return rc;
    }

    DLLLOCAL int close() {
        if (isopen) {
            //printd(5, "qore_ds_private::close() this: %p in_transaction: %d active_transaction: %d\n", this, in_transaction, active_transaction);
            if (bulk_load_active && !connection_aborted) {
                // A native protocol session cannot survive a close/reopen cycle.  Give the driver a
                // final abort callback so it can close its wire-level state and stream boundary.
                ExceptionSink xsink;
                bulk_load_active = false;
                qore_dbi_private::get(*dsl)->bulkLoadEnd(ds, false, &xsink);
                xsink.clear();
            }
            bulk_load_active = false;
            qore_dbi_private::get(*dsl)->close(ds);
            isopen = false;
            in_transaction = false;
            active_transaction = false;
            commit_in_progress = false;
            // a pending connection-loss event must keep the transaction identity it belongs to;
            // it is cleared when the outcome is delivered at the next core boundary
            if (!mutation_conn_lost) {
                tx_id.clear();
                tx_seq = 0;
            }
            clearTypedResultHashDeclCache();
            return 0;
        }
        return -1;
    }

    DLLLOCAL void setStatementKeepLock(QoreSQLStatement* stmt) {
        assert(!keep_lock);
        keep_lock = true;
        if (!in_transaction)
            in_transaction = true;
        if (!active_transaction)
            active_transaction = true;

        addStatement(stmt);
    }

    DLLLOCAL bool keepLock() {
        bool rv = keep_lock;
        if (keep_lock)
            keep_lock = false;
        return rv;
    }

    DLLLOCAL static qore_ds_private* get(Datasource& ds) {
        return ds.priv;
    }

private:
    // set of active SQLStatements on this datasource
    stmt_set_t stmt_set;
};

#endif

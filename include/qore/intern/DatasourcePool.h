/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    DatasourcePool.h

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

#ifndef _QORUS_DATASOURCE_POOL_H

#define _QORUS_DATASOURCE_POOL_H

#include <qore/Datasource.h>
#include <qore/QoreThreadLock.h>
#include <qore/QoreCondition.h>
#include <qore/QoreString.h>
#include <qore/AbstractThreadResource.h>

#include "qore/intern/DatasourceStatementHelper.h"
#include "qore/intern/QoreSQLStatement.h"
#include "qore/intern/SqlMutationContext.h"
#include "qore/intern/DbiForkRegistry.h"
#include "qore/intern/QoreForkHandlers.h"

#include <map>
#include <deque>
#include <string>

typedef std::map<int, int> thread_use_t;   // for marking a datasource in use
typedef std::deque<int> free_list_t;       // for the free list

// class holding datasource configuration params
class DatasourceConfig {
protected:
    DBIDriver* driver;

    std::string user,
        pass,
        db,
        encoding,
        host;

    int port;

    // options
    QoreHashNode* opts;
    // event queue
    Queue* q;
    // Queue argument
    QoreValue arg{};
    // mutation observer context shared by every connection in the pool; nullptr = unmonitored
    SqlMutationContext* mctx = nullptr;

public:
    DLLLOCAL DatasourceConfig(DBIDriver* n_driver, const char* n_user, const char* n_pass, const char* n_db,
                                const char* n_encoding, const char* n_host, int n_port,
                                const QoreHashNode* n_opts, Queue* n_q, QoreValue n_arg) :
        driver(n_driver), user(n_user ? n_user : ""), pass(n_pass ? n_pass : ""), db(n_db ? n_db : ""),
        encoding(n_encoding ? n_encoding : ""), host(n_host ? n_host : ""), port(n_port),
        opts(n_opts ? n_opts->hashRefSelf() : 0), q(n_q), arg(n_arg) {
    }

    DLLLOCAL DatasourceConfig(const DatasourceConfig& old) :
        driver(old.driver), user(old.user), pass(old.pass), db(old.db), encoding(old.encoding), host(old.host),
        port(old.port), opts(old.opts ? old.opts->hashRefSelf() : nullptr),
        q(old.q ? old.q->queueRefSelf() : nullptr), arg(old.arg.refSelf()),
        mctx(old.mctx ? old.mctx->refSelf() : nullptr) {
    }

    DLLLOCAL ~DatasourceConfig() {
        assert(!q);
        assert(!arg);
        assert(!opts);
        assert(!mctx);
    }

    //! Releases the configuration; may be called more than once
    /** An explicit delete of a pool releases it in the destructor, and the last reference to the pool's private
        data - which a thread still inside a pool method can hold - releases it again, so every member is cleared
        once released.
    */
    DLLLOCAL void del(ExceptionSink* xsink) {
        if (q) {
            q->deref(xsink);
            q = nullptr;
        }
        arg.discard(xsink);
        arg = QoreValue();
        if (opts) {
            opts->deref(xsink);
            opts = nullptr;
        }
        if (mctx) {
            mctx->deref(xsink);
            mctx = nullptr;
        }
    }

    // the first connection (opened in the DatasourcePool constructor) is passed with an xsink obj
    // because invalid options can cause an exception to be thrown
    DLLLOCAL Datasource* get(DatasourceStatementHelper* dsh, ExceptionSink* xsink) const;

    DLLLOCAL void setQueue(Queue* n_q, QoreValue n_arg, ExceptionSink* xsink) {
        if (q)
            q->deref(xsink);
        q = n_q;
        arg.discard(xsink);
        arg = n_arg;
    }

    //! returns the mutation context, creating it if necessary; the config keeps the reference
    /** New connections created from this config inherit the context, which is what makes an
        observer registered on the pool apply to connections opened later, and what keeps a
        per-thread declaration valid across connection allocation and release
    */
    DLLLOCAL SqlMutationContext* getOrCreateMutationContext() {
        if (!mctx) {
            mctx = new SqlMutationContext;
        }
        return mctx;
    }

    DLLLOCAL SqlMutationContext* getMutationContext() const {
        return mctx;
    }
};

class DatasourcePool : public AbstractThreadResource, public QoreCondition, public QoreThreadLock, public DatasourceStatementHelper,
        public QoreDbiForkObject {
    friend class DatasourcePoolActionHelper;
protected:
    Datasource** pool;
    //! the TID of the thread holding each connection, or 0 if the connection is free
    /** Kept beside tmap and free_list in an array of integers, so that it is consistent in a child process created
        with fork() while a thread of the parent was changing them; see forkCheckUnlocked()
    */
    int* tid_list;
    thread_use_t tmap;        // map from tids to pool index
    free_list_t free_list;

    unsigned min,
        max,
        cmax,                      // current max
        wait_count,
        wait_max,
        tl_warning_ms;

    int64 tl_timeout_ms,
        stats_reqs,
        stats_hits
        ;

    ResolvedCallReferenceNode* warning_callback;
    QoreValue callback_arg{};

    DatasourceConfig config;

    bool valid;

    //! the fork generation of the process that the pool's connections belong to; see forkCheckUnlocked()
    unsigned fork_generation = qore_fork_get_generation();
    //! the TID of the thread whose next action that uses a connection raises \c DATASOURCE-FORK-ERROR, or 0
    /** In a child process created with fork(): the forking thread held a connection in a transaction in the parent,
        which an action without a connection gave up (see QoreDbiForkCheck::Defer)
    */
    int fork_lost_tx_tid = 0;

    //! gives up the connections of the parent process in a child created with fork(); the pool lock must be held
    /** The connections belong to the process that opened them (see qore_ds_private::abandon()), and the threads that
        held connections in the parent do not exist in this process, unless one of them is the forking thread: every
        connection is given up without calling the driver and made free again, and the allocation containers, which a
        thread of the parent can have been changing when the process was copied, are replaced without being
        traversed.  Connections are opened again when they are needed.

        A transaction that the current thread held a connection for in the parent cannot be continued: the action
        that would continue it raises \c DATASOURCE-FORK-ERROR.

        @param mode Raise for an action that uses a connection, Defer for one that does not (the error is raised by
        the next action of the thread that does; \a xsink can be nullptr), Discard when the pool is closed or
        destroyed
        @param remove_resource if true, the thread resource that the forking thread registered for its connection in
        the parent is removed (false when it is being run as the thread resource)

        @return 0 if nothing was given up, 1 if the connections of the parent were given up, -1 if an exception was
        raised (they were given up as well)
    */
    DLLLOCAL int forkCheckUnlocked(ExceptionSink* xsink, QoreDbiForkCheck mode, bool remove_resource = true);

    //! gives up the connections of the parent process in a child created with fork(); takes the pool lock
    DLLLOCAL int forkCheck(ExceptionSink* xsink, QoreDbiForkCheck mode, bool remove_resource = true) {
        AutoLocker al(static_cast<QoreThreadLock*>(this));
        return forkCheckUnlocked(xsink, mode, remove_resource);
    }

#ifdef DEBUG
    QoreThreadLocalStorage<QoreString> thread_local_storage;
    void addSQL(const char* cmd, const QoreString* sql);
    void resetSQL();
#endif

    //! returns the pool-wide mutation context, creating and fanning it out if necessary
    /** must be called with the pool lock held
    */
    DLLLOCAL SqlMutationContext* getOrCreateMutationContext();

    //! holds the lock of a pool while it is copied; see the copy constructor
    class CopyLock {
    public:
        DLLLOCAL explicit CopyLock(const DatasourcePool& p)
                : lck(static_cast<QoreThreadLock*>(const_cast<DatasourcePool*>(&p))) {
            lck->lock();
        }

        DLLLOCAL ~CopyLock() {
            unlock();
        }

        CopyLock(const CopyLock&) = delete;
        CopyLock& operator=(const CopyLock&) = delete;

        //! releases the lock early
        DLLLOCAL void unlock() {
            if (lck) {
                lck->unlock();
                lck = nullptr;
            }
        }

    private:
        QoreThreadLock* lck;
    };

    //! copies the pool with the lock of the pool that is copied held, which the caller acquires with \a lock
    DLLLOCAL DatasourcePool(const DatasourcePool& old, CopyLock&& lock, ExceptionSink* xsink);

    DLLLOCAL Datasource* getAllocatedDS();
    //! returns the connection allocated to the current thread, or nullptr if it holds none
    DLLLOCAL Datasource* findAllocatedDS();
    //! closes all connections of the pool; called when the last reference to the pool is released
    DLLLOCAL void closeConnections(ExceptionSink* xsink);
    DLLLOCAL Datasource* getDSIntern(bool& new_ds, int64& wait_total, ExceptionSink* xsink);
    DLLLOCAL Datasource* getDS(bool& new_ds, ExceptionSink* xsink);
    DLLLOCAL void freeDS(ExceptionSink* xsink);
    // share the code for exec() and execRaw()
    DLLLOCAL QoreValue exec_internal(bool doBind, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL int checkWait(int64 warn_total, ExceptionSink* xsink);
    DLLLOCAL void init(ExceptionSink* xsink);

public:
#ifdef DEBUG
    QoreString* getAndResetSQL();
#endif

    // min must be 1 or more, max must be greater than min
    DLLLOCAL DatasourcePool(ExceptionSink* xsink, DBIDriver* ndsl, const char* user, const char* pass, const char* db, const char* charset, const char* hostname, unsigned mn, unsigned mx, int port = 0, const QoreHashNode* opts = 0, Queue* q = nullptr, QoreValue a = QoreValue());
    DLLLOCAL DatasourcePool(const DatasourcePool& old, ExceptionSink* xsink);

    DLLLOCAL virtual ~DatasourcePool();

    //! initializes the pool lock and its condition again in a child process created with fork(); see
    //! QoreDbiForkObject
    DLLLOCAL virtual void forkChild() noexcept {
        // the waiters are threads of the parent; the connections they held are given up when the pool is next used
        // (see forkCheckUnlocked())
        QoreCondition::reinitAfterFork();
        QoreThreadLock::reinitAfterFork();
        wait_count = 0;
    }

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        // if the object is obliterated (due to a constructor error in a child class or a serialization error), make sure
        // it's destroyed properly
        if (ROdereference()) {
            // the connections are closed here, where an error closing one can be raised
            closeConnections(xsink);
            config.del(xsink);
            delete this;
        }
    }

    DLLLOCAL void destructor(ExceptionSink* xsink);
    DLLLOCAL virtual void cleanup(ExceptionSink* xsink);
    DLLLOCAL QoreValue select(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* selectRow(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreValue selectRows(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreValue selectTyped(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreColumnarResult* selectColumnar(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreValue selectRowsTyped(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL int beginTransaction(ExceptionSink* xsink);
    DLLLOCAL QoreValue exec(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreValue execRaw(const QoreString* sql, ExceptionSink* xsink);
    DLLLOCAL int bulkLoadBegin(const QoreString* table, const QoreListNode* columns, const QoreHashNode* options,
        ExceptionSink* xsink);
    DLLLOCAL int bulkLoadRows(const QoreHashNode* rows, ExceptionSink* xsink);
    DLLLOCAL int bulkLoadEnd(bool success, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* describe(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL int commit(ExceptionSink* xsink);
    DLLLOCAL int rollback(ExceptionSink* xsink);
    DLLLOCAL QoreStringNode* toString();
    DLLLOCAL unsigned getMin() const;
    DLLLOCAL unsigned getMax() const;
    DLLLOCAL QoreStringNode* getPendingUsername() const;
    DLLLOCAL QoreStringNode* getPendingPassword() const;
    DLLLOCAL QoreStringNode* getPendingDBName() const;
    DLLLOCAL QoreStringNode* getPendingDBEncoding() const;
    DLLLOCAL QoreStringNode* getPendingHostName() const;
    DLLLOCAL int getPendingPort() const;
    DLLLOCAL const QoreEncoding* getQoreEncoding() const;
    DLLLOCAL const DBIDriver* getDriver() const {
        return pool[0]->getDriver();
    }
    DLLLOCAL const char* getDriverName() const {
        return pool[0]->getDriverName();
    }

    DLLLOCAL QoreListNode* getCapabilityList() const;
    DLLLOCAL int getCapabilities() const;

    DLLLOCAL QoreValue getServerVersion(ExceptionSink* xsink);
    DLLLOCAL QoreValue getClientVersion(ExceptionSink* xsink) {
        return pool[0]->getClientVersion(xsink);
    }

    DLLLOCAL QoreStringNode* getDriverRealName(ExceptionSink* xsink);

    DLLLOCAL bool inTransaction();

    DLLLOCAL QoreHashNode* getOptionHash() const {
        return pool[0]->getOptionHash();
    }

    DLLLOCAL QoreValue getOption(const char* opt, ExceptionSink* xsink) {
        return pool[0]->getOption(opt, xsink);
    }

    // mutation observer API; the context is owned by the pool config and shared by every connection
    DLLLOCAL void setMutationObserver(ResolvedCallReferenceNode* observer, int64 event_mask, QoreValue arg,
            ExceptionSink* xsink);
    DLLLOCAL void clearMutationObserver(ExceptionSink* xsink);
    DLLLOCAL bool hasMutationObserver() const;
    DLLLOCAL int pushMutationDeclaration(const QoreHashNode* info, ExceptionSink* xsink);
    DLLLOCAL int popMutationDeclaration(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* getMutationDeclaration() const;
    DLLLOCAL int reportMutationStreamBegin(int64 declared_bytes, ExceptionSink* xsink);
    DLLLOCAL int reportMutationStreamProgress(int64 consumed_bytes, ExceptionSink* xsink);
    DLLLOCAL int reportMutationStreamEnd(int64 consumed_bytes, bool ok, ExceptionSink* xsink);

#ifdef DEBUG
    //! arms a one-shot debug connection abort on the pool-wide context; see lib/ql_debug.cpp
    DLLLOCAL void dbgArmConnectionAbort(const char* op_id, int when);
#endif

    // functions supporting DatasourceStatementHelper
    DLLLOCAL DatasourceStatementHelper* helperRefSelfImpl() {
        ref();
        return this;
    }

    // implementing DatasourceStatementHelper virtual functions
    DLLLOCAL virtual void helperDestructorImpl(QoreSQLStatement* s, ExceptionSink* xsink) {
        deref(xsink);
    }

    DLLLOCAL virtual Datasource* helperStartActionImpl(ExceptionSink* xsink, bool& new_transaction) {
        return getDS(new_transaction, xsink);
    }

    DLLLOCAL virtual Datasource* helperStartActionNoConnectImpl(ExceptionSink* xsink, bool& new_transaction,
            bool& connected) {
        // only a connection that the thread already holds is used
        new_transaction = false;
        Datasource* ds = findAllocatedDS();
        if (!ds) {
            connected = false;
        }
        return ds;
    }

    DLLLOCAL virtual bool helperHeldByCurrentThreadImpl(Datasource* ds) {
        return findAllocatedDS() == ds;
    }

    DLLLOCAL virtual void helperEndActionNoConnectImpl(bool new_transaction) {
        // no connection was allocated for the action
        assert(!new_transaction);
    }

    DLLLOCAL virtual Datasource* helperEndActionImpl(char cmd, bool new_transaction, ExceptionSink* xsink) {
        //printd(5, "DatasourcePool::helperEndAction() cmd: %d '%s', nt: %d\n", cmd, DAH_TEXT(cmd), new_transaction);
        if (cmd == DAH_RELEASE) {
            freeDS(xsink);
            return 0;
        }

        return getAllocatedDS();
    }

    DLLLOCAL bool currentThreadInTransaction() const {
        SafeLocker sl((QoreThreadLock*)this);
        // in a child process created with fork(), a connection that the thread held in the parent is not held here
        if (fork_generation != qore_fork_get_generation()) {
            return false;
        }
        return tmap.find(q_gettid()) != tmap.end();
    }

    DLLLOCAL QoreHashNode* getConfigHash(ExceptionSink* xsink);
    DLLLOCAL QoreStringNode* getConfigString(ExceptionSink* xsink);

    DLLLOCAL void clearWarningCallback(ExceptionSink* xsink);
    DLLLOCAL void setWarningCallback(int64 warning_ms, ResolvedCallReferenceNode* cb, QoreValue arg, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* getUsageInfo() const;

    DLLLOCAL void setErrorTimeout(unsigned t_ms) {
        // a thread waiting for a connection reads the timeout with the pool lock held
        AutoLocker al(static_cast<QoreThreadLock*>(this));
        tl_timeout_ms = t_ms;
    }

    DLLLOCAL unsigned getErrorTimeout() const {
        AutoLocker al(static_cast<QoreThreadLock*>(const_cast<DatasourcePool*>(this)));
        return tl_timeout_ms;
    }

    DLLLOCAL void setEventQueue(Queue* q, QoreValue arg, ExceptionSink* xsink);
};

class DatasourcePoolActionHelper {
protected:
    DatasourcePool& dsp;
    ExceptionSink* xsink;
    Datasource* ds;
    bool new_ds;
    char cmd;

public:
    DLLLOCAL DatasourcePoolActionHelper(DatasourcePool& n_dsp, ExceptionSink* n_xsink, char n_cmd = DAH_NOCHANGE) : dsp(n_dsp), xsink(n_xsink), new_ds(false), cmd(n_cmd) {
        ds = dsp.getDS(new_ds, xsink);
    }

    /* release the connection if:
            1) the connection was aborted (exception already raised)
            2) the connection was acquired for this call, and
                the command was NOCHANGE, meaning, leave the connection in the same state it was before the call
    */
    DLLLOCAL ~DatasourcePoolActionHelper();

    // issue #3509: allow connections to be released if a beginTransaction() cmd fails on a new datasource
    DLLLOCAL void releaseNew() {
        assert(cmd == DAH_ACQUIRE);
        if (new_ds) {
            cmd = DAH_RELEASE;
        }
    }

#if 0
    DLLLOCAL void addSQL(const QoreString* sql) {
        if (ds && !((cmd == DAH_RELEASE) || (new_ds && (cmd == DAH_NOCHANGE)) || ds->wasConnectionAborted()))
            dsp.addSQL(cmd == DAH_NOCHANGE ? "select" : "exec", sql);
    }
#endif

    DLLLOCAL operator bool() const { return ds; }

    DLLLOCAL Datasource* operator*() const { return ds; }
    DLLLOCAL Datasource* operator->() const { return ds; }
};

#endif

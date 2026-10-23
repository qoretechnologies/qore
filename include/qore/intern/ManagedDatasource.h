/* -*- mode: c++; indent-tabs-mode: nil-*- */
/*
    ManagedDatasource.h

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

/*
 FIXME: when raising an timeout exception there is a race condition
 getting the TID of the thread holding the lock, because the lock
 could have been released after the ::enter() call fails... but it's
 only cosmetic (for the exception text)
 */

#ifndef _QORE_MANAGEDDATASOURCE_H
#define _QORE_MANAGEDDATASOURCE_H

#include "qore/intern/DatasourceStatementHelper.h"
#include "qore/intern/QoreSQLStatement.h"
#include "qore/intern/DbiForkRegistry.h"
#include "qore/intern/QoreForkHandlers.h"

#include <atomic>
#include <set>

// default timeout set to 120 seconds
#define DEFAULT_TL_TIMEOUT 120000

class ManagedDatasource : public AbstractThreadResource, public Datasource, public DatasourceStatementHelper,
        public QoreDbiForkObject {
    friend class DatasourceActionHelper;
    friend class DatasourceLockHelper;

protected:
    // connection/transaction lock
    mutable QoreThreadLock ds_lock;

    // TID of the thread holding the connection/transaction lock, or -1; see lockTid()
    std::atomic<int> tid{-1};
    //! the fork generation of the process in which the transaction lock was acquired; see lockTidInProcess()
    std::atomic<unsigned> lock_generation{0};
    // the number of threads waiting on the transaction lock, and the transaction lock timeout in milliseconds; both
    // are accessed only with ds_lock held
    int waiting = 0,
        tl_timeout_ms;

    QoreCondition cond;             // condition when transaction lock is freed

    //! returns the TID of the thread holding the transaction lock, or -1
    /** The TID is written only with ds_lock held, but it is read without it: by currentThreadInTransaction(),
        transactionTid() and after a statement action, so it is atomic.  Relaxed ordering is enough: a thread always
        reads its own last write, so whether the current thread holds the lock is exact without ds_lock, and a TID
        written by another thread is only reported as a snapshot that can change at any time, as documented.
    */
    DLLLOCAL int lockTid() const {
        return tid.load(std::memory_order_relaxed);
    }

    //! sets the TID of the thread holding the transaction lock, or -1; ds_lock must be held
    /** The generation of a lock is written before its TID is released, so that a thread that reads the TID without
        ds_lock reads its generation as well (see lockTidInProcess())
    */
    DLLLOCAL void setLockTid(int t) {
        if (t != -1) {
            lock_generation.store(qore_fork_get_generation(), std::memory_order_relaxed);
        }
        tid.store(t, std::memory_order_release);
    }

    //! returns the TID of the thread holding the transaction lock in this process, or -1
    /** In a child process created with fork(), a lock that a thread of the parent held is held by no thread of this
        process (see forkCheckUnlocked())
    */
    DLLLOCAL int lockTidInProcess() const {
        int t = tid.load(std::memory_order_acquire);
        return t != -1 && lock_generation.load(std::memory_order_relaxed) == qore_fork_get_generation() ? t : -1;
    }

    //! gives up the connection and the transaction lock that a parent process held, in a child created with fork()
    /** The connection belongs to the process that opened it (see qore_ds_private::abandon()), and the thread that held
        the transaction lock in the parent does not exist in this process, unless it is the forking thread.  The
        connection is given up without calling the driver, and the lock is released, so that the next action opens a
        new connection.  A transaction that the connection was in, or that another thread of the parent held the lock
        for, cannot be continued: the action that would continue it raises \c DATASOURCE-FORK-ERROR.  ds_lock must be
        held.

        @param mode Raise for an action that uses the connection, Defer for one that does not (the error is raised by
        the next action that does), Discard when the datasource is closed or destroyed
        @param remove_resource if true, the thread resource that the forking thread registered for the transaction
        lock in the parent is removed (false when it is being run as the thread resource)

        @return 0 if the action can continue, 1 if it can continue and an inherited connection was given up (only
        with Defer: the datasource is closed now), -1 if an exception was raised
    */
    DLLLOCAL int forkCheckUnlocked(ExceptionSink* xsink, QoreDbiForkCheck mode, bool remove_resource = true);

    //! closes the driver data of statements destroyed in other threads that the connection holds
    /** called by the thread holding the transaction lock before it releases it, with ds_lock held; there is no
        caller to raise an error closing them to, so an error is reported as a warning event
    */
    DLLLOCAL void closeOrphans();

    //! acquires the transaction lock for an action that needs no connection
    /** @param fork_gave_up set to true if a connection of the parent of a child process created with fork() was
        given up (see forkCheckUnlocked()): the datasource is closed now

        @return 0 for OK, -1 if an exception was raised
    */
    DLLLOCAL int acquireLock(ExceptionSink* xsink, bool& new_lock, bool& fork_gave_up);
    //! acquires the transaction lock for an action and opens the datasource if necessary
    /** @param open if false, a closed datasource is not opened: 1 is returned with the transaction lock held, as for
        an open datasource, and the caller releases it if \a new_transaction is true

        @return 0 for OK, -1 if an exception was raised, 1 if the datasource is closed and \a open is false
    */
    DLLLOCAL int startDBAction(ExceptionSink* xsink, bool& new_transaction, bool open = true);
    // returns true if we have the transaction lock, false if not
    DLLLOCAL bool endDBActionIntern(char cmd = DAH_NOCHANGE, bool new_transaction = false);
    // returns true if we have the transaction lock, false if not
    DLLLOCAL bool endDBAction(char cmd = DAH_NOCHANGE, bool new_transaction = false);
    DLLLOCAL int closeUnlocked(ExceptionSink* xsink);
    // returns 0 for OK, -1 for timeout, -2 for interrupt
    DLLLOCAL int grabLockIntern(ExceptionSink* xsink = nullptr);
    DLLLOCAL void grabLockUnconditionalIntern();
    // returns 0 for OK, -1 for error
    DLLLOCAL int grabLock(ExceptionSink* xsink);
    DLLLOCAL void releaseLock();
    DLLLOCAL void releaseLockIntern();
    DLLLOCAL void forceReleaseLockIntern();
    DLLLOCAL void finish_transaction();

protected:
    DLLLOCAL virtual ~ManagedDatasource() {
        forkUnregister();
    }

public:
    DLLLOCAL ManagedDatasource(DBIDriver *ndsl) : Datasource(ndsl, this), tl_timeout_ms(DEFAULT_TL_TIMEOUT) {
        forkRegister();
    }

    DLLLOCAL ManagedDatasource(const ManagedDatasource& old) : Datasource(old, this),
            tl_timeout_ms(old.getTransactionLockTimeout()) {
        forkRegister();
    }

    //! initializes the lock and its condition again in a child process created with fork(); see QoreDbiForkObject
    DLLLOCAL void forkChild() noexcept override {
        // the waiters are threads of the parent; the TID of a lock that one of them held is forgotten when the
        // datasource is next used (see forkCheckUnlocked())
        cond.reinitAfterFork();
        ds_lock.reinitAfterFork();
        waiting = 0;
    }

    DLLLOCAL virtual void cleanup(ExceptionSink* xsink);
    DLLLOCAL virtual void destructor(ExceptionSink* xsink);
    DLLLOCAL virtual void deref(ExceptionSink* xsink);
    DLLLOCAL virtual void deref();

    DLLLOCAL QoreValue select(const QoreString *query_str, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* selectRow(const QoreString *query_str, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreValue selectRows(const QoreString *query_str, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreValue selectTyped(const QoreString *query_str, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreColumnarResult* selectColumnar(const QoreString *query_str, const QoreListNode* args,
        ExceptionSink* xsink);
    DLLLOCAL QoreValue selectRowsTyped(const QoreString *query_str, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreValue exec(const QoreString *query_str, const QoreListNode* args, ExceptionSink* xsink);
    DLLLOCAL QoreValue execRaw(const QoreString *query_str, ExceptionSink* xsink);
    DLLLOCAL int bulkLoadBegin(const QoreString* table, const QoreListNode* columns, const QoreHashNode* options,
        ExceptionSink* xsink);
    DLLLOCAL int bulkLoadRows(const QoreHashNode* rows, ExceptionSink* xsink);
    DLLLOCAL int bulkLoadEnd(bool success, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* describe(const QoreString *query_str, const QoreListNode* args, ExceptionSink* xsink);

    DLLLOCAL int commit(ExceptionSink* xsink);
    DLLLOCAL int rollback(ExceptionSink* xsink);
    DLLLOCAL int open(ExceptionSink* xsink);

    using Datasource::close;
    DLLLOCAL int close(ExceptionSink* xsink);
    DLLLOCAL int reset(ExceptionSink* xsink);
    DLLLOCAL void setPendingUsername(const char* u);
    DLLLOCAL void setPendingPassword(const char* p);
    DLLLOCAL void setPendingDBName(const char* d);
    DLLLOCAL void setPendingDBEncoding(const char* c);
    DLLLOCAL void setPendingHostName(const char* h);
    DLLLOCAL void setPendingPort(int port);
    DLLLOCAL QoreStringNode* getPendingUsername() const;
    DLLLOCAL QoreStringNode* getPendingPassword() const;
    DLLLOCAL QoreStringNode* getPendingDBName() const;
    DLLLOCAL QoreStringNode* getPendingDBEncoding() const;
    DLLLOCAL QoreStringNode* getPendingHostName() const;
    DLLLOCAL int getPendingPort() const;
    DLLLOCAL void setTransactionLockTimeout(int t_ms);
    DLLLOCAL int getTransactionLockTimeout() const;
    // returns true if a new transaction was started
    DLLLOCAL bool beginTransaction(ExceptionSink* xsink);

    using Datasource::setAutoCommit;
    DLLLOCAL void setAutoCommit(bool ac, ExceptionSink* xsink);

    using Datasource::copy;
    DLLLOCAL ManagedDatasource* copy();
    DLLLOCAL QoreValue getServerVersion(ExceptionSink* xsink);
    DLLLOCAL QoreValue getClientVersion(ExceptionSink* xsink) const;
    DLLLOCAL QoreStringNode* getDriverRealName(ExceptionSink* xisnk);

    DLLLOCAL QoreHashNode* getConfigHash(ExceptionSink* xsink);
    DLLLOCAL QoreStringNode* getConfigString(ExceptionSink* xsink);

    DLLLOCAL void setEventQueue(Queue* q, QoreValue arg, ExceptionSink* xsink);

    // mutation observer API; these two methods create the mutation context lazily, which the base
    // class does not serialize, so they are serialized here with the datasource lock; the context
    // itself is thread-safe, so no other mutation observer method needs to be overridden
    DLLLOCAL void setMutationObserver(ResolvedCallReferenceNode* observer, int64 event_mask, QoreValue arg,
            ExceptionSink* xsink);
    DLLLOCAL int pushMutationDeclaration(const QoreHashNode* info, ExceptionSink* xsink);

    DLLLOCAL int transactionTid() const {
        return lockTidInProcess();
    }

    DLLLOCAL bool currentThreadInTransaction() const {
        return lockTidInProcess() == q_gettid();
    }

    //! returns true if a transaction is in progress; in a child process created with fork(), a transaction of the
    //! parent is not
    /** Any thread can call it: a transaction is in progress while its thread holds the transaction lock, which in a
        child is a lock of the parent until the child gives it up (see lockTidInProcess()); both are atomic
    */
    DLLLOCAL bool inTransactionInProcess() const {
        return isInTransaction() && lockTidInProcess() != -1;
    }

    DLLLOCAL QoreHashNode* getOptionHash(ExceptionSink* xsink);
    // sets an option in the constructor without locking
    DLLLOCAL int setOptionInit(const char* opt, const QoreValue val, ExceptionSink* xsink);
    DLLLOCAL int setOption(const char* opt, const QoreValue val, ExceptionSink* xsink);
    DLLLOCAL QoreValue getOption(const char* opt, ExceptionSink* xsink);

    // functions supporting DatasourceStatementHelper
    DLLLOCAL virtual DatasourceStatementHelper* helperRefSelfImpl() {
        ref();
        return this;
    }

    // implementing DatasourceStatementHelper virtual functions
    DLLLOCAL virtual void helperDestructorImpl(QoreSQLStatement* s, ExceptionSink* xsink) {
        deref(xsink);
    }

    DLLLOCAL virtual Datasource* helperStartActionImpl(ExceptionSink* xsink, bool& new_transaction) {
        if (!startDBAction(xsink, new_transaction))
            return this;

        // only return "this" when there was an exception in startDBAction if we already had the lock
        return lockTid() == q_gettid() ? this : 0;
    }

    DLLLOCAL virtual Datasource* helperStartActionNoConnectImpl(ExceptionSink* xsink, bool& new_transaction,
            bool& connected) {
        int rc = startDBAction(xsink, new_transaction, false);
        if (!rc) {
            return this;
        }
        if (rc > 0) {
            // the datasource is closed; its transaction lock is held until helperEndActionNoConnectImpl()
            connected = false;
            return this;
        }
        // only return "this" when there was an exception in startDBAction if we already had the lock
        return lockTid() == q_gettid() ? this : nullptr;
    }

    DLLLOCAL virtual bool helperHeldByCurrentThreadImpl(Datasource* ds) {
        assert(ds == this);
        // exact without ds_lock: a thread always reads its own last write
        return lockTidInProcess() == q_gettid();
    }

    DLLLOCAL virtual void helperEndActionNoConnectImpl(bool new_transaction) {
        // the lock is released only if the action acquired it
        if (new_transaction) {
            releaseLock();
        }
    }

    DLLLOCAL virtual Datasource* helperEndActionImpl(char cmd, bool new_transaction, ExceptionSink* xsink) {
        // execute a commit if auto-commit is enabled and the resource is being released
        // and the connection was not aborted
        if (cmd == DAH_RELEASE) {
            autoCommit(xsink);
        }
        return endDBAction(cmd, new_transaction) ? this : 0;
    }
};

class DatasourceActionHelper {
protected:
    ManagedDatasource& ds;
    bool new_transaction = false;
    //! true if a DAH_NOCONN action gave up a connection of the parent of a child process created with fork()
    bool fork_gave_up = false;
    bool ok;
    char cmd;

public:
    DLLLOCAL DatasourceActionHelper(ManagedDatasource& n_ds, ExceptionSink* xsink, char n_cmd = DAH_NOCHANGE) :
        ds(n_ds), ok(n_cmd == DAH_NOCONN ? !ds.acquireLock(xsink, new_transaction, fork_gave_up) : !ds.startDBAction(xsink, new_transaction)), cmd(n_cmd) {
    }

    DLLLOCAL ~DatasourceActionHelper();

    DLLLOCAL bool newTransaction() const { return new_transaction; }

    //! returns true if the action gave up a connection of the parent of a child process created with fork(); the
    //! datasource is closed now (only for DAH_NOCONN)
    DLLLOCAL bool forkGaveUpConnection() const { return fork_gave_up; }

    DLLLOCAL operator bool() const { return ok; }
};

class DatasourceLockHelper {
protected:
    ManagedDatasource& ds;
    bool valid, had_lock;

public:
    //! takes the transaction lock for a change of the datasource
    /** @param fork_mode how a connection or lock of the parent of a child process created with fork() is given up
        first (see ManagedDatasource::forkCheckUnlocked())
    */
    DLLLOCAL DatasourceLockHelper(ManagedDatasource& n_ds, ExceptionSink* xsink, QoreDbiForkCheck fork_mode)
            : ds(n_ds) {
        ds.ds_lock.lock();
        if (ds.forkCheckUnlocked(xsink, fork_mode) < 0) {
            valid = false;
            ds.ds_lock.unlock();
            return;
        }
        had_lock = ds.lockTid() == q_gettid();
        valid = !ds.grabLock(xsink);
        if (!valid)
            ds.ds_lock.unlock();
    }

    DLLLOCAL ~DatasourceLockHelper() {
        if (valid) {
            if (!had_lock)
                ds.releaseLockIntern();
            ds.ds_lock.unlock();
        }
    }

    DLLLOCAL operator bool() const { return valid; }
};

#endif // _QORE_SQL_OBJECTS_DATASOURCE_H

/*
    ManagedDatasource.cpp

    Qore Programming Language

    Copyright 2003 - 2026 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>
#include "qore/intern/ManagedDatasource.h"
#include "qore/intern/qore_ds_private.h"

#include <cstdlib>
#include <cstring>

DatasourceActionHelper::~DatasourceActionHelper() {
    if (ok) {
        if (cmd == DAH_NOCONN) {
            // Metadata/option calls may borrow an existing transaction's lock. Only release a lock
            // acquired by this helper; otherwise another thread can enter the owner's transaction.
            if (new_transaction) {
                ds.releaseLock();
            }
        } else {
            bool keep_lock = qore_ds_private::get(ds)->keepLock();

            // FIXME: check connection aborted handling if exec could have been executed after connection reset
            if (ds.wasConnectionAborted()) {
                cmd = DAH_RELEASE;
            } else if (new_transaction) {
                if (cmd == DAH_NOCHANGE && ds.isInTransaction() && keep_lock) {
                    cmd = DAH_ACQUIRE;
                } else if ((cmd == DAH_NOCHANGE) || !ds.isInTransaction()) {
                    cmd = DAH_RELEASE;
                }
            }

            ds.endDBAction(cmd, new_transaction);
        }
    }
}

int ManagedDatasource::forkCheckUnlocked(ExceptionSink* xsink, QoreDbiForkCheck mode, bool remove_resource) {
    qore_ds_private* priv = qore_ds_private::get(*this);
    const int owner = lockTid();
    // a lock acquired in an ancestor process: the thread that holds it does not exist here, unless it forked
    if (owner != -1 && lockTidInProcess() == -1) {
        if (owner == q_gettid()) {
            if (remove_resource) {
                // the resource that the forking thread registered in the parent would end the parent's transaction
                remove_thread_resource(this);
            }
        } else if (mode != QoreDbiForkCheck::Discard) {
            // the transaction of the other thread cannot be continued here
            priv->setForkLostTransaction(QoreDbiForkReasonLock);
        }
        // there are no waiters in this process; see forkChild()
        setLockTid(-1);
    }
    const bool inherited = priv->isInherited();
    if (mode == QoreDbiForkCheck::Discard) {
        // the transaction of the parent ends with the connection, without an error
        if (inherited) {
            priv->abandon();
        }
        priv->clearForkLostTransaction();
        return 0;
    }
    // the driver is never called with the connection, also not by an action that uses no connection (it can read
    // options from it or close the statements of other threads with it when it releases the lock)
    if (inherited) {
        priv->abandonInherited();
    }
    if (mode == QoreDbiForkCheck::Raise) {
        return priv->raiseForkLostTransaction(xsink);
    }
    return inherited ? 1 : 0;
}

void ManagedDatasource::cleanup(ExceptionSink *xsink) {
    // this thread has the transaction lock
    AutoLocker al(&ds_lock);

    // in a child process created with fork(), a connection opened by the parent is given up: its transaction
    // belongs to the parent, so nothing is rolled back (the thread resource is being run, so it is not removed), and
    // the lock acquired in the parent is released with it
    forkCheckUnlocked(xsink, QoreDbiForkCheck::Discard, false);
    if (lockTid() != q_gettid()) {
        return;
    }

    // the thread resource is registered whenever this thread acquires the connection, which a
    // prepared statement does when it is bound, with or without a transaction in progress; with no
    // transaction there is nothing to roll back and nothing to report, only the lock to release
    if (isInTransaction()) {
        xsink->raiseException("DATASOURCE-TRANSACTION-EXCEPTION", "%s:%s@%s: TID %d terminated while in a " \
            "transaction; transaction will be automatically rolled back and the lock released", getDriverName(),
            getUsernameStr().c_str(), getDBNameStr().c_str(), q_gettid());
        Datasource::rollback(xsink);
        setTransactionStatus(false);
    }
    releaseLockIntern();
}

void ManagedDatasource::destructor(ExceptionSink* xsink) {
    AutoLocker al(&ds_lock);
    // in a child process created with fork(), a connection or transaction lock of the parent is given up
    forkCheckUnlocked(xsink, QoreDbiForkCheck::Discard);
    const int lock_tid = lockTid();
    if (lock_tid == q_gettid() || lock_tid == -1)
        // closeUnlocked will throw an exception if a transaction is in progress (and release the transaction lock if held)
        closeUnlocked(xsink);
    else
        // the connection is held by another thread, so it cannot be closed here, and that thread's
        // resource - which is registered in its thread resource list and not reachable from this one
        // - is left holding the last reference to this datasource.  Its thread-exit purge closes the
        // connection and frees the datasource; see ThreadResourceList::purge()
        xsink->raiseException("DATASOURCE-ERROR", "%s:%s@%s: TID %d deleted Datasource while TID %d is holding the " \
            "transaction lock", getDriverName(), getUsernameStr().c_str(), getDBNameStr().c_str(), q_gettid(),
            lock_tid);
}

void ManagedDatasource::deref(ExceptionSink *xsink) {
    if (ROdereference()) {
        close(xsink);
        delete this;
    }
}

// this function is only called by ThreadResourceList::remove(), from remove_thread_resource(),
// where the caller holds a reference of its own, meaning that the reference count cannot reach 0,
// meaning that the close method will never be run here.  The thread-exit purge, which can hold the
// last reference, uses deref(ExceptionSink*) instead so that the connection is still closed
void ManagedDatasource::deref() {
#ifdef DEBUG
   assert(!ROdereference());
#else
   ROdereference();
#endif
}

int ManagedDatasource::grabLockIntern(ExceptionSink* xsink) {
    int ctid = q_gettid();

    if (lockTid() == ctid) {
        return 0;
    }

    // the deadline for a positive timeout; a timeout of 0 or less means no timeout, as documented (and as for the
    // DatasourcePool error timeout): the wait ends when the lock is released or, with an exception sink, when the
    // thread is cancelled
    int64 deadline_us = tl_timeout_ms > 0 ? q_get_monotonic_us() + static_cast<int64>(tl_timeout_ms) * 1000 : 0;

    while (lockTid() != -1) {
        // a lock that a thread of the parent of this process held is free here; see forkCheckUnlocked()
        if (lockTidInProcess() == -1) {
            setLockTid(-1);
            break;
        }
        // Check for cancellation or program interrupt
        if (xsink && qore_check_cancel(xsink, "datasource lock acquisition")) {
            return -2;  // -2 indicates interrupt
        }

        int64 wait_ms = -1;
        if (tl_timeout_ms > 0) {
            int64 remaining_us = deadline_us - q_get_monotonic_us();
            if (remaining_us <= 0) {
                printd(5, "ManagedDatasource::grabLockIntern() this=%p timed out after %dms waiting for tid %d to "
                    "release lock\n", this, tl_timeout_ms, lockTid());
                return -1;
            }
            wait_ms = (remaining_us + 999) / 1000;
        }

        ++waiting;
        if (xsink) {
            // a cancellation request or program interrupt wakes the wait at once; the check at the top of the
            // loop raises it
            cond.waitWithInterrupt(&ds_lock, wait_ms);
        } else {
            // without an exception sink this is not a cancellation point
            cond.wait2(&ds_lock, wait_ms);
        }
        --waiting;
    }

    setLockTid(ctid);

    return 0;
}

int ManagedDatasource::grabLock(ExceptionSink *xsink) {
    int rc = grabLockIntern(xsink);
    if (rc < 0) {
        endDBActionIntern();
        if (rc == -1) {
            // Timeout
            const char *un = getUsername();
            const char *db = getDBName();
            xsink->raiseException("TRANSACTION-LOCK-TIMEOUT", "%s:%s@%s: TID %d timed out on datasource '%s@%s' after waiting %d millisecond%s on transaction lock held by TID %d",
                                    getDriverName(), getUsernameStr().c_str(), getDBNameStr().c_str(),
                                    q_gettid(), un ? un : "<n/a>", db ? db : "<n/a>", tl_timeout_ms,
                                    tl_timeout_ms == 1 ? "" : "s", lockTid());
        }
        // rc == -2 means interrupt, exception already raised by checkIOInterrupt
        return -1;
    }
    return 0;
}

void ManagedDatasource::releaseLockIntern() {
   assert(lockTid() == q_gettid());
   // the thread gives up the connection: the driver data of statements destroyed in other threads while it held it
   // is closed first, in this thread
   closeOrphans();
   setLockTid(-1);
   if (waiting)
      cond.signal();
}

void ManagedDatasource::releaseLock() {
   AutoLocker al(ds_lock);
   releaseLockIntern();
}

void ManagedDatasource::forceReleaseLockIntern() {
   setLockTid(-1);
   if (waiting)
      cond.signal();
}

ManagedDatasource* ManagedDatasource::copy() {
   return new ManagedDatasource(*this);
}

void ManagedDatasource::closeOrphans() {
    if (!isOpen()) {
        // a connection closes them when it is closed
        return;
    }
    ExceptionSink close_xsink;
    qore_ds_private::get(*this)->closeOrphans(&close_xsink);
    qore_dbi_private::reportCloseWarnings(this, close_xsink);
}

int ManagedDatasource::acquireLock(ExceptionSink* xsink, bool& new_lock, bool& fork_gave_up) {
    AutoLocker al(&ds_lock);
    // a connection or lock of the parent of a child process created with fork() is given up first; the actions that
    // take the lock this way need no connection (options and configuration), so a transaction of the parent is
    // raised by the next action that uses a connection
    int rc = forkCheckUnlocked(xsink, QoreDbiForkCheck::Defer);
    if (rc < 0) {
        return -1;
    }
    fork_gave_up = rc > 0;
    // Observe ownership under the same mutex that protects acquisition and release.
    new_lock = lockTid() != q_gettid();
    return grabLock(xsink);
}

int ManagedDatasource::startDBAction(ExceptionSink *xsink, bool &new_transaction, bool open) {
    AutoLocker al(&ds_lock);

    // a connection or lock of the parent of a child process created with fork() is given up first: the action uses a
    // new connection, unless it would continue a transaction of the parent; for an action that makes no connection
    // (the close of a statement), a transaction of the parent is raised by the next action that uses a connection
    if (forkCheckUnlocked(xsink, open ? QoreDbiForkCheck::Raise : QoreDbiForkCheck::Defer) < 0) {
        new_transaction = false;
        return -1;
    }

    // save previous trans lock status
    new_transaction = (lockTid() != q_gettid());

    // first grab the transaction lock
    if (grabLock(xsink))
        return -1;

    if (!isOpen()) {
        if (!open) {
            // the closed datasource is not opened; the transaction lock stays held for the action, as it is for an
            // open datasource, and is released by the caller
            return 1;
        }
        // open the datasource
        if (Datasource::open(xsink) || *xsink) {
            // release transaction lock if necessary
            if (new_transaction) {
                releaseLockIntern();
            }
            return -1;
        }
    }

    //printd(5, "ManagedDatasource::startDBAction() this=%p need_lock=%d new_trans=%p had_lock=%d\n", this, need_transaction_lock, new_transaction, had_lock);
    return 0;
}

bool ManagedDatasource::endDBActionIntern(char cmd, bool new_transaction) {
    if (cmd == DAH_ACQUIRE) {
        // save thread resource if we just started a transaction
        if (new_transaction) {
            //printd(5, "ManagedDatasource::endDBActionIntern() this: %p priv: %p set_thread_resource()\n", this, qore_ds_private::get(*this));
            set_thread_resource(this);
        }
    } else if (cmd) {
        assert(cmd == DAH_RELEASE);

        // transaction is complete, remove the transaction thread resource
        //printd(5, "ManagedDatasource::endDBActionIntern() this: %p priv: %p remove_thread_resource()\n", this, qore_ds_private::get(*this));
        remove_thread_resource(this);

        releaseLockIntern();
    }

    return lockTid() == q_gettid();
}

bool ManagedDatasource::endDBAction(char cmd, bool new_transaction) {
   AutoLocker al(&ds_lock);
   return endDBActionIntern(cmd, new_transaction);
}

void ManagedDatasource::setTransactionLockTimeout(int t_ms) {
    // a thread waiting for the transaction lock reads the timeout with ds_lock held
    AutoLocker al(&ds_lock);
    tl_timeout_ms = t_ms;
}

int ManagedDatasource::getTransactionLockTimeout() const {
    AutoLocker al(&ds_lock);
    return tl_timeout_ms;
}

void ManagedDatasource::setAutoCommit(bool ac, ExceptionSink *xsink) {
   DatasourceLockHelper dslh(*this, xsink, QoreDbiForkCheck::Defer);
   if (!dslh)
      return;

   Datasource::setAutoCommit(ac);
}

QoreHashNode* ManagedDatasource::getConfigHash(ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink, DAH_NOCONN);
    if (!dbah) {
        return nullptr;
    }

    return Datasource::getConfigHash();
}

QoreStringNode* ManagedDatasource::getConfigString(ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink, DAH_NOCONN);
    if (!dbah) {
        return nullptr;
    }

    return Datasource::getConfigString();
}

QoreValue ManagedDatasource::select(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink);
    if (!dbah)
        return QoreValue();

    return Datasource::select(query_str, args, xsink);
}

QoreValue ManagedDatasource::selectTyped(const QoreString* query_str, const QoreListNode* args, ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink);
    if (!dbah)
        return QoreValue();

    return Datasource::selectTyped(query_str, args, xsink);
}

QoreColumnarResult* ManagedDatasource::selectColumnar(const QoreString* query_str, const QoreListNode* args,
        ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink);
    if (!dbah)
        return nullptr;

    return Datasource::selectColumnar(query_str, args, xsink);
}

QoreHashNode* ManagedDatasource::selectRow(const QoreString *sql, const QoreListNode *args, ExceptionSink *xsink) {
    DatasourceActionHelper dbah(*this, xsink);
    if (!dbah)
        return nullptr;

    return Datasource::selectRow(sql, args, xsink);
}

QoreValue ManagedDatasource::selectRows(const QoreString *query_str, const QoreListNode *args, ExceptionSink *xsink) {
    DatasourceActionHelper dbah(*this, xsink);
    if (!dbah)
        return QoreValue();

    return Datasource::selectRows(query_str, args, xsink);
}

QoreValue ManagedDatasource::selectRowsTyped(const QoreString *query_str, const QoreListNode *args,
        ExceptionSink *xsink) {
    DatasourceActionHelper dbah(*this, xsink);
    if (!dbah)
        return QoreValue();

    return Datasource::selectRowsTyped(query_str, args, xsink);
}

QoreValue ManagedDatasource::exec(const QoreString *query_str, const QoreListNode *args, ExceptionSink *xsink) {
    DatasourceActionHelper dbah(*this, xsink, getAutoCommit() ? DAH_NOCHANGE : DAH_ACQUIRE);
    if (!dbah)
        return QoreValue();

    //printd(5, "ManagedDatasource::exec() st=%d tid=%d\n", start_transaction, tid);

    return Datasource::exec(query_str, args, xsink);
}

QoreValue ManagedDatasource::execRaw(const QoreString *query_str, ExceptionSink *xsink) {
   DatasourceActionHelper dbah(*this, xsink, getAutoCommit() ? DAH_NOCHANGE : DAH_ACQUIRE);
   if (!dbah)
      return QoreValue();

   return Datasource::execRaw(query_str, xsink);
}

int ManagedDatasource::bulkLoadBegin(const QoreString* table, const QoreListNode* columns,
        const QoreHashNode* options, ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink, DAH_ACQUIRE);
    if (!dbah) {
        return -1;
    }
    return Datasource::bulkLoadBegin(table, columns, options, xsink);
}

int ManagedDatasource::bulkLoadRows(const QoreHashNode* rows, ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink, DAH_ACQUIRE);
    if (!dbah) {
        return -1;
    }
    return Datasource::bulkLoadRows(rows, xsink);
}

int ManagedDatasource::bulkLoadEnd(bool success, ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink, getAutoCommit() ? DAH_RELEASE : DAH_ACQUIRE);
    if (!dbah) {
        return -1;
    }
    return Datasource::bulkLoadEnd(success, xsink);
}

QoreHashNode* ManagedDatasource::describe(const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
   DatasourceActionHelper dbah(*this, xsink);
   if (!dbah)
      return nullptr;

   return Datasource::describe(sql, args, xsink);
}

bool ManagedDatasource::beginTransaction(ExceptionSink *xsink) {
   DatasourceActionHelper dbah(*this, xsink, getAutoCommit() ? DAH_NOCHANGE : DAH_ACQUIRE);
   if (!dbah)
      return false;

   Datasource::beginTransaction(xsink);
   //printd(5, "ManagedDatasource::beginTransaction() this=%p isInTransaction()=%d\n", this, isInTransaction());

   return dbah.newTransaction();
}

int ManagedDatasource::commit(ExceptionSink *xsink) {
   DatasourceActionHelper dbah(*this, xsink, getAutoCommit() ? DAH_NOCHANGE : DAH_RELEASE);
   if (!dbah)
      return -1;

   return Datasource::commit(xsink);
}

int ManagedDatasource::rollback(ExceptionSink *xsink) {
    DatasourceActionHelper dbah(*this, xsink, getAutoCommit() ? DAH_NOCHANGE : DAH_RELEASE);
    if (!dbah)
        return -1;

    return Datasource::rollback(xsink);
}

int ManagedDatasource::open(ExceptionSink *xsink) {
   DatasourceLockHelper dslh(*this, xsink, QoreDbiForkCheck::Raise);
   if (!dslh)
      return -1;

   return Datasource::open(xsink);
}

// returns 0 for OK, -1 for exception
int ManagedDatasource::closeUnlocked(ExceptionSink *xsink) {
    int rc = 0;

    // in a child process created with fork(), a connection opened by the parent is given up: nothing is rolled back
    // or sent on it
    forkCheckUnlocked(xsink, QoreDbiForkCheck::Discard);

    // the lock is taken to wait for the actions of other threads to finish; a lock that this call takes is released
    // when the connection is closed, so that the datasource is free for every thread afterwards, as a datasource
    // that was never opened is
    const bool new_lock = lockTid() != q_gettid();
    if (grabLock(xsink)) {
        return -1;
    }

    //printd(5, "ManagedDatasource::closeUnlocked() this: %p priv: %p %s:%s@%s open: %d trans: %d\n", this, qore_ds_private::get(*this), getDriverName(), getUsernameStr().c_str(), getDBNameStr().c_str(), isOpen(), isInTransaction());

    // the thread resource is registered whenever this thread acquires the connection, which a
    // prepared statement does when it is bound, with or without a transaction in progress.  It
    // tracks the connection this thread holds and not the state of the datasource, so it is removed
    // on every close, including the close of a datasource that is already closed.  Two things go
    // wrong when one is left registered: Datasource::close() below frees the driver's connection
    // data, so the resource would run cleanup() - and the driver's rollback - against a closed
    // connection; and the resource outlives the datasource object, which leaves the thread-exit
    // purge holding the last reference to a datasource nobody can reach any more.
    const bool held = !remove_thread_resource(this);

    if (isOpen()) {
        const bool in_transaction = isInTransaction();

        if (in_transaction) {
            if (!wasConnectionAborted()) {
                // FIXME: check for statement
                xsink->raiseException("DATASOURCE-TRANSACTION-EXCEPTION", "%s:%s@%s: Datasource closed while in a transaction; transaction will be automatically rolled back and the lock released", getDriverName(), getUsernameStr().c_str(), getDBNameStr().c_str());
                Datasource::rollback(xsink);
            }
            setTransactionStatus(false);
            rc = -1;
        }

        if (in_transaction || held) {
            // force-exit the transaction lock; the connection is about to be closed, so leaving the
            // lock held would keep every other thread from ever reopening the datasource
            forceReleaseLockIntern();
        }

        // an error closing the connection or its statements is raised; the connection is closed in every case
        if (Datasource::close(xsink)) {
            rc = -1;
        }
    } else if (held) {
        // the connection this thread was holding is already gone, so the lock has to go with it:
        // leaving it held by a thread that may be about to exit would keep every other thread from
        // reopening the datasource
        forceReleaseLockIntern();
    }

    // otherwise the lock taken above was kept by this thread with no transaction and no thread resource to release
    // it: currentThreadInTransaction() returned True after close(), other threads timed out on the lock, and a
    // destructor run in another thread raised DATASOURCE-ERROR
    if (new_lock && lockTid() == q_gettid()) {
        releaseLockIntern();
    }

    return rc;
}

int ManagedDatasource::close(ExceptionSink *xsink) {
   AutoLocker al(&ds_lock);
   return closeUnlocked(xsink);
}

// closes and re-opens to reset a database connection
int ManagedDatasource::reset(ExceptionSink *xsink) {
    AutoLocker al(&ds_lock);
    if (closeUnlocked(xsink))
        return -1;
    // open the connection
    return Datasource::open(xsink);
}

void ManagedDatasource::setPendingUsername(const char *u) {
   AutoLocker al(&ds_lock);
   Datasource::setPendingUsername(u);
}

void ManagedDatasource::setPendingPassword(const char *p) {
   AutoLocker al(&ds_lock);
   Datasource::setPendingPassword(p);
}

void ManagedDatasource::setPendingDBName(const char *d) {
   AutoLocker al(&ds_lock);
   Datasource::setPendingDBName(d);
}

void ManagedDatasource::setPendingDBEncoding(const char *c) {
   AutoLocker al(&ds_lock);
   Datasource::setPendingDBEncoding(c);
}

void ManagedDatasource::setPendingHostName(const char *h) {
   AutoLocker al(&ds_lock);
   Datasource::setPendingHostName(h);
}

void ManagedDatasource::setPendingPort(int port) {
   AutoLocker al(&ds_lock);
   Datasource::setPendingPort(port);
}

QoreStringNode *ManagedDatasource::getPendingUsername() const {
   AutoLocker al(&ds_lock);
   return Datasource::getPendingUsername();
}

QoreStringNode *ManagedDatasource::getPendingPassword() const {
   AutoLocker al(&ds_lock);
   return Datasource::getPendingPassword();
}

QoreStringNode *ManagedDatasource::getPendingDBName() const {
   AutoLocker al(&ds_lock);
   return Datasource::getPendingDBName();
}

QoreStringNode *ManagedDatasource::getPendingDBEncoding() const {
   AutoLocker al(&ds_lock);
   return Datasource::getPendingDBEncoding();
}

QoreStringNode *ManagedDatasource::getPendingHostName() const {
   AutoLocker al(&ds_lock);
   return Datasource::getPendingHostName();
}

int ManagedDatasource::getPendingPort() const {
   AutoLocker al(&ds_lock);
   return Datasource::getPendingPort();
}

QoreValue ManagedDatasource::getServerVersion(ExceptionSink *xsink) {
    DatasourceActionHelper dbah(*this, xsink);
    if (!dbah) {
        return QoreValue();
    }

    return Datasource::getServerVersion(xsink);
}

QoreValue ManagedDatasource::getClientVersion(ExceptionSink *xsink) const {
   return Datasource::getClientVersion(xsink);
}

QoreStringNode* ManagedDatasource::getDriverRealName(ExceptionSink *xsink) {
    DatasourceActionHelper dbah(*this, xsink);
    if (!dbah) {
        return nullptr;
    }

    return Datasource::getDriverRealName(xsink);
}

QoreHashNode* ManagedDatasource::getOptionHash(ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink, DAH_NOCONN);
    if (!dbah)
        return nullptr;
    return Datasource::getOptionHash();
}

int ManagedDatasource::setOptionInit(const char* opt, const QoreValue val, ExceptionSink* xsink) {
    return Datasource::setOption(opt, val, xsink);
}

int ManagedDatasource::setOption(const char* opt, const QoreValue val, ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink, DAH_NOCONN);
    if (!dbah)
        return -1;
    return Datasource::setOption(opt, val, xsink);
}

QoreValue ManagedDatasource::getOption(const char* opt, ExceptionSink* xsink) {
    DatasourceActionHelper dbah(*this, xsink, DAH_NOCONN);
    if (!dbah)
        return QoreValue();
    // the value is the connection's: a connection of the parent of a child process created with fork() that was
    // given up is replaced, unless it was in a transaction
    if (dbah.forkGaveUpConnection()
        && (qore_ds_private::get(*this)->raiseForkLostTransaction(xsink) || Datasource::open(xsink))) {
        return QoreValue();
    }
    return Datasource::getOption(opt, xsink);
}

void ManagedDatasource::setEventQueue(Queue* q, QoreValue arg, ExceptionSink* xsink) {
    AutoLocker al(&ds_lock);
    Datasource::setEventQueue(q, arg, xsink);
}

void ManagedDatasource::setMutationObserver(ResolvedCallReferenceNode* observer, int64 event_mask, QoreValue arg,
        ExceptionSink* xsink) {
    AutoLocker al(&ds_lock);
    Datasource::setMutationObserver(observer, event_mask, arg, xsink);
}

int ManagedDatasource::pushMutationDeclaration(const QoreHashNode* info, ExceptionSink* xsink) {
    AutoLocker al(&ds_lock);
    return Datasource::pushMutationDeclaration(info, xsink);
}

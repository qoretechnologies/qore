/*
    ManagedDatasource.cpp

    Qore Programming Language

    Copyright 2003 - 2024 Qore Technologies, s.r.o.

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
            ds.releaseLock();
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

void ManagedDatasource::cleanup(ExceptionSink *xsink) {
    // this thread has the transaction lock
    AutoLocker al(&ds_lock);
    assert(isInTransaction());

    xsink->raiseException("DATASOURCE-TRANSACTION-EXCEPTION", "%s:%s@%s: TID %d terminated while in a transaction; " \
        "transaction will be automatically rolled back and the lock released", getDriverName(),
        getUsernameStr().c_str(), getDBNameStr().c_str(), q_gettid());
    Datasource::rollback(xsink);
    setTransactionStatus(false);
    releaseLockIntern();
}

void ManagedDatasource::destructor(ExceptionSink* xsink) {
    AutoLocker al(&ds_lock);
    if (tid == q_gettid() || tid == -1)
        // closeUnlocked will throw an exception if a transaction is in progress (and release the transaction lock if held)
        closeUnlocked(xsink);
    else
        xsink->raiseException("DATASOURCE-ERROR", "%s:%s@%s: TID %d deleted Datasource while TID %d is holding the " \
            "transaction lock", getDriverName(), getUsernameStr().c_str(), getDBNameStr().c_str(), q_gettid(), tid);
}

void ManagedDatasource::deref(ExceptionSink *xsink) {
    if (ROdereference()) {
        close(xsink);
        delete this;
    }
}

// this function is only called by remove_thread_resource()
// during a call, meaning that the reference count cannot reach 0,
// meaning that the close method will never be run here
void ManagedDatasource::deref() {
#ifdef DEBUG
   assert(!ROdereference());
#else
   ROdereference();
#endif
}

int ManagedDatasource::grabLockIntern(ExceptionSink* xsink) {
    int ctid = q_gettid();

    if (tid == ctid)
        return 0;

    // Check for sandbox interrupt support
    QoreSandboxManager* sm = runtime_get_sandbox_manager();
    const int poll_interval = QORE_IO_POLL_INTERVAL_MS;
    int64 remaining_timeout = tl_timeout_ms;

    while (tid != -1) {
        // Check for interrupt
        if (sm && xsink && sm->checkIOInterrupt(xsink, "datasource lock acquisition")) {
            return -2;  // -2 indicates interrupt
        }

        ++waiting;
        if (tl_timeout_ms > 0 || (tl_timeout_ms < 0 && sm)) {
            // Positive timeout or infinite timeout with sandbox manager: use polling
            int effective_timeout;
            if (sm) {
                // With sandbox manager, poll at intervals for interrupt checking
                effective_timeout = (tl_timeout_ms < 0 || remaining_timeout > poll_interval)
                    ? poll_interval : remaining_timeout;
            } else {
                // No sandbox manager: use full timeout
                effective_timeout = tl_timeout_ms;
            }

            int rc = cond.wait(&ds_lock, effective_timeout);
            --waiting;
            if (!rc)
                continue;

            // Timeout occurred
            if (sm && tl_timeout_ms > 0) {
                remaining_timeout -= effective_timeout;
                if (remaining_timeout <= 0) {
                    printd(5, "ManagedDatasource::grabLockIntern() this=%p timed out after %dms waiting for tid %d to release lock\n", this, tl_timeout_ms, tid);
                    return -1;
                }
            } else if (sm && tl_timeout_ms < 0) {
                // Infinite timeout with sandbox manager - continue polling
                continue;
            } else {
                printd(5, "ManagedDatasource::grabLockIntern() this=%p timed out after %dms waiting for tid %d to release lock\n", this, tl_timeout_ms, tid);
                return -1;
            }
        } else if (tl_timeout_ms == 0) {
            // Zero timeout means try once without waiting - fail immediately if lock not available
            printd(5, "ManagedDatasource::grabLockIntern() this=%p lock not available (tl_timeout_ms=0), tid %d holds lock\n", this, tid);
            --waiting;
            return -1;
        } else {
            // Infinite timeout without sandbox manager
            cond.wait(&ds_lock);
            --waiting;
        }
    }

    tid = ctid;

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
                                    tl_timeout_ms == 1 ? "" : "s", tid);
        }
        // rc == -2 means interrupt, exception already raised by checkIOInterrupt
        return -1;
    }
    return 0;
}

void ManagedDatasource::releaseLockIntern() {
   assert(tid == q_gettid());
   tid = -1;
   if (waiting)
      cond.signal();
}

void ManagedDatasource::releaseLock() {
   AutoLocker al(ds_lock);
   releaseLockIntern();
}

void ManagedDatasource::forceReleaseLockIntern() {
   tid = -1;
   if (waiting)
      cond.signal();
}

ManagedDatasource* ManagedDatasource::copy() {
   return new ManagedDatasource(*this);
}

int ManagedDatasource::acquireLock(ExceptionSink *xsink) {
   AutoLocker al(&ds_lock);
   return grabLock(xsink);
}

int ManagedDatasource::startDBAction(ExceptionSink *xsink, bool &new_transaction) {
    AutoLocker al(&ds_lock);

    // save previous trans lock status
    new_transaction = (tid != q_gettid());

    // first grab the transaction lock
    if (grabLock(xsink))
        return -1;

    // open the datasource if necessary
    if (!isOpen() && (Datasource::open(xsink) || *xsink)) {
        // release transaction lock if necessary
        if (new_transaction)
            releaseLockIntern();
        return -1;
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

    return tid == q_gettid();
}

bool ManagedDatasource::endDBAction(char cmd, bool new_transaction) {
   AutoLocker al(&ds_lock);
   return endDBActionIntern(cmd, new_transaction);
}

void ManagedDatasource::setTransactionLockTimeout(int t_ms) {
   tl_timeout_ms = t_ms;
}

int ManagedDatasource::getTransactionLockTimeout() const {
   return tl_timeout_ms;
}

void ManagedDatasource::setAutoCommit(bool ac, ExceptionSink *xsink) {
   DatasourceLockHelper dslh(*this, xsink);
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
   DatasourceLockHelper dslh(*this, xsink);
   if (!dslh)
      return -1;

   return Datasource::open(xsink);
}

// returns 0 for OK, -1 for exception
int ManagedDatasource::closeUnlocked(ExceptionSink *xsink) {
    int rc = 0;

    if (grabLock(xsink))
        return -1;

    //printd(5, "ManagedDatasource::closeUnlocked() this: %p priv: %p %s:%s@%s open: %d trans: %d\n", this, qore_ds_private::get(*this), getDriverName(), getUsernameStr().c_str(), getDBNameStr().c_str(), isOpen(), isInTransaction());

    if (isOpen()) {
        if (isInTransaction()) {
            if (!wasConnectionAborted()) {
                // FIXME: check for statement
                xsink->raiseException("DATASOURCE-TRANSACTION-EXCEPTION", "%s:%s@%s: Datasource closed while in a transaction; transaction will be automatically rolled back and the lock released", getDriverName(), getUsernameStr().c_str(), getDBNameStr().c_str());
                Datasource::rollback(xsink);
            }
            remove_thread_resource(this);
            setTransactionStatus(false);
            // force-exit the transaction lock
            forceReleaseLockIntern();
            rc = -1;
        }

        Datasource::close();
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
    return Datasource::getOption(opt, xsink);
}

void ManagedDatasource::setEventQueue(Queue* q, QoreValue arg, ExceptionSink* xsink) {
    AutoLocker al(&ds_lock);
    Datasource::setEventQueue(q, arg, xsink);
}

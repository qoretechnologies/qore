/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSQLStatement.h

    Qore Programming Language

    Copyright (C) 2006 - 2024 Qore Technologies, s.r.o.

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
#include "qore/intern/QC_SQLStatement.h"
#include "qore/intern/DatasourceStatementHelper.h"
#include "qore/intern/sql_statement_private.h"
#include "qore/intern/qore_ds_private.h"
#include "qore/intern/qore_dbi_private.h"
#include "qore/intern/DatasourcePool.h"

const char* QoreSQLStatement::stmt_statuses[] = { "idle", "prepared", "executed", "defined" };

class DBActionHelper {
public:
    QoreSQLStatement& stmt;
    ExceptionSink* xsink;
    bool valid = false;

    // new transaction flag
    bool nt = false;

    // close the statement in the destructor
    bool close = false;

    // the action command
    char cmd;

    DLLLOCAL DBActionHelper(QoreSQLStatement& n_stmt, ExceptionSink* n_xsink, char n_cmd = DAH_NOCHANGE) : stmt(n_stmt), xsink(n_xsink), cmd(n_cmd) {
        Datasource* newds = stmt.dsh->helperStartAction(xsink, nt);
        // issue #2334
        if (newds && stmt.priv->ds && newds != stmt.priv->ds) {
            // can only happen with a DatasourcePool, otherwise the action above will block
            assert(dynamic_cast<DatasourcePool*>(stmt.dsh));
            // raise an exception if we are trying to execute something in another thread
            xsink->raiseException("SQLSTATEMENT-ERROR", "cannot execute an action in another thread with an allocated connection from a DatasourcePool");
            // release the connection back to the pool
            stmt.dsh->helperEndAction(DAH_RELEASE, nt, xsink);
            valid = false;
            return;
        }
        assert(!newds || !stmt.priv->ds || (newds == stmt.priv->ds));
        assert(newds || *xsink);
        if (newds && !stmt.priv->ds)
            qore_ds_private::get(*newds)->addStatement(&n_stmt);
        stmt.priv->ds = newds;

        //printd(5, "DBActionHelper::DBActionHelper() ds: %p new: %p cmd: %s nt: %d xs: %d stmt: %p\n", stmt.priv->ds, newds, DAH_TEXT(cmd), nt, (bool)*xsink, &stmt);
        valid = (bool)stmt.priv->ds;
    }

    DLLLOCAL ~DBActionHelper() {
        // if no datasource is currently assigned
        if (!valid || !stmt.priv->ds)
            return;

        if (close) {
            assert(cmd == DAH_ACQUIRE);
            assert(stmt.priv->ds->isOpen());
            stmt.closeIntern(xsink);
        }

        /* release the Datasource if:
            1) the transaction was aborted
            or
            2) the Datasource was acquired for this call, and
                the command was NOCHANGE, meaning, leave the Datasource in the same state it was before the call
        */
        if (stmt.priv->ds->wasConnectionAborted() || !stmt.priv->ds->isOpen() || (nt && (cmd == DAH_NOCHANGE)))
            cmd = DAH_RELEASE;

        //printd(5, "DBActionHelper::~DBActionHelper() ds: %p cmd: %s nt: %d xsink: %d stmt: %p status: %d data: %p\n", stmt.priv->ds, DAH_TEXT(cmd), nt, xsink->isEvent(), &stmt, stmt.status, stmt.priv->data);

        // remove statement from Datasource if the connection is no longer allocated
        // must be executed in the Datasource allocation lock
        if (cmd == DAH_RELEASE) {
            qore_ds_private* dspriv = qore_ds_private::get(*stmt.priv->ds);
            //printd(5, "DBActionHelper::~DBActionHelper() old: %p ds: %p removing stmt %p\n", oldds, stmt.priv->ds, &stmt);
            dspriv->removeStatement(&stmt);
        }

        // call end action with the command
        stmt.priv->ds = stmt.dsh->helperEndAction(cmd, nt, xsink);

        // we have to remove the datasource from the statement immediately if the Datasource is closed or committed
        assert((cmd == DAH_RELEASE) || stmt.priv->ds);
    }

    DLLLOCAL void markClose() {
        assert(!close);
        close = true;
    }

    DLLLOCAL operator bool() const {
        return valid;
    }

    DLLLOCAL bool inTransaction() const {
        return valid && !nt;
    }
};

QoreSQLStatement::QoreSQLStatement(Datasource* ds, void* data, DatasourceStatementHelper* dsh, unsigned char status) : SQLStatement(ds, data), dsh(dsh->helperRefSelf()), status(status) {
   qore_ds_private::get(*ds)->setStatementKeepLock(this);
}

QoreSQLStatement::~QoreSQLStatement() {
   assert(!priv->data);
}

int QoreSQLStatement::checkStatus(ExceptionSink* xsink, DBActionHelper& dba, int stat, const char* action) {
    //printd(5, "QoreSQLStatement::checkStatus() this: %p stat: %d status: %d action: '%s' ssize: %d\n", this, stat,
    //    status, action, ssize);

    if (stat != status) {
        if (stat == STMT_IDLE)
            return closeIntern(xsink);

        // issue #2773: execute "exec define" if possible
        if (stat == STMT_DEFINED && status < STMT_EXECED
            && qore_dbi_private::get(*priv->ds->getDriver())->hasExecDefine() && !strcmp(action, "describe")) {
            if (status == STMT_IDLE && str.strlen()) {
                if (prepareIntern(xsink))
                    return -1;
            }

            if (execDescribeIntern(dba, xsink))
                return -1;

            // make sure the statement is closed to IDLE after defining
            // as the execution above is only suitable for a define/describe
            dba.markClose();

            return defineIntern(xsink);
        }

        if (stat > STMT_IDLE && status == STMT_IDLE && str.strlen()) {
            if (prepareIntern(xsink))
                return -1;

            if (stat == status)
                return 0;
        }

        if (stat == STMT_PREPARED && status == STMT_EXECED)
            return 0;

        if (stat == STMT_PREPARED && status == STMT_DEFINED) {
            if (closeIntern(xsink))
                return -1;
            return prepareIntern(xsink);
        }

        if ((stat == STMT_EXECED || stat == STMT_DEFINED) && status == STMT_PREPARED) {
            if (execIntern(dba, xsink))
                return -1;

            if (stat == status)
                return 0;
        }

        if (stat == STMT_DEFINED && status == STMT_EXECED)
            return defineIntern(xsink);

        xsink->raiseException("SQLSTATEMENT-ERROR", "SQLStatement::%s() called expecting status '%s', but statement "
            "has status '%s'", action, stmt_statuses[stat], stmt_statuses[status]);
        return -1;
    }

    return 0;
}

void QoreSQLStatement::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        //char cmd = DAH_NOCHANGE;
        //printd(5, "QoreSQLStatement::deref() deleting this: %p cmd: %s\n", this, DAH_TEXT(cmd));
        closeIntern(xsink);

        if (priv->ds)
            qore_ds_private::get(*priv->ds)->removeStatement(this);

        dsh->helperDestructor(this, xsink);

        if (prepare_args)
            prepare_args->deref(xsink);

        delete this;
    }
}

void QoreSQLStatement::transactionDone(bool clear, bool close, ExceptionSink* xsink) {
    //printd(5, "QoreSQLStatement::transactionDone() this: %p data: %p ds: %p clear: %d\n", this, priv->data, priv->ds, clear);
    if (priv->data) {
        assert(priv->ds);
        // if "close" is set, then we delete the statement's local data
        if (close) {
            qore_dbi_private::get(*priv->ds->getDriver())->stmt_close(this, xsink);
            assert(!priv->data);
            status = STMT_IDLE;
        }
        else // otherwise, any handles are freed but the local data stays in place
            qore_dbi_private::get(*priv->ds->getDriver())->stmt_free(this, xsink);
    }
    // if "clear" is set, then we clear the datasource
    if (clear && priv->ds)
        priv->ds = nullptr;
}

int QoreSQLStatement::closeIntern(ExceptionSink* xsink) {
    if (!priv->data)
        return 0;

    assert(priv->ds);
    int rc = qore_dbi_private::get(*priv->ds->getDriver())->stmt_close(this, xsink);
    assert(!priv->data);
    status = STMT_IDLE;

    return rc;
}

int QoreSQLStatement::prepareArgs(bool n_raw, const QoreString& n_str, const QoreListNode* args, ExceptionSink* xsink) {
    raw = n_raw;
    str = n_str;

    if (prepare_args) {
        prepare_args->deref(xsink);
        if (*xsink) {
            prepare_args = nullptr;
            return -1;
        }
    }

    prepare_args = args ? args->listRefSelf() : nullptr;
    return 0;
}

int QoreSQLStatement::prepareIntern(ExceptionSink* xsink) {
    //assert(!stmtds);
    int rc = qore_dbi_private::get(*priv->ds->getDriver())->stmt_prepare(this, str, prepare_args, xsink);
    if (!rc) {
        status = STMT_PREPARED;
    }
    else
        closeIntern(xsink);
    return rc;
}

int QoreSQLStatement::prepare(const QoreString& n_str, const QoreListNode* args, ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink);
    if (!dba)
        return -1;

    if (checkStatus(xsink, dba, STMT_IDLE, "prepare"))
        return -1;

    if (prepareArgs(false, n_str, args, xsink))
        return -1;

    return 0;
}

int QoreSQLStatement::prepareRaw(const QoreString& n_str, ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink);
    if (!dba)
        return -1;

    if (checkStatus(xsink, dba, STMT_IDLE, "prepareRaw"))
        return -1;

    if (prepareArgs(true, n_str, nullptr, xsink))
        return -1;

    return 0;
}

int QoreSQLStatement::bind(const QoreListNode& l, ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return -1;

    if (checkStatus(xsink, dba, STMT_PREPARED, "bind"))
        return -1;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_bind(this, l, xsink);
}

int QoreSQLStatement::bindPlaceholders(const QoreListNode& l, ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return -1;

    if (checkStatus(xsink, dba, STMT_PREPARED, "bindPlaceholders"))
        return -1;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_bind_placeholders(this, l, xsink);
}

int QoreSQLStatement::bindValues(const QoreListNode& l, ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return -1;

    if (checkStatus(xsink, dba, STMT_PREPARED, "bindValues"))
        return -1;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_bind_values(this, l, xsink);
}

int QoreSQLStatement::exec(const QoreListNode* args, ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba) {
        return -1;
    }

    // statements from output buffers have no SQL
    if (str.empty()) {
        xsink->raiseException("SQLSTATEMENT-ERROR", "the current statement has no SQL to execute");
        return -1;
    }

    if (checkStatus(xsink, dba, STMT_PREPARED, "exec")) {
        return -1;
    }

    if (args && args->size() && qore_dbi_private::get(*priv->ds->getDriver())->stmt_bind(this, *args, xsink)) {
        return -1;
    }

    return execIntern(dba, xsink);
}

//! mutation observer admission point for a prepared statement execution
/** @return 0 to continue; -1 if the observer rejected the operation, in which case the statement
    must not be executed

    @note prepared statements carry exactly the same declaration as any other operation, because the
    declaration is read from the shared per-datasource mutation context and never from the statement
*/
static int stmt_mutation_pre(qore_ds_private* dsp, int stmt_class, ExceptionSink* xsink) {
    if (dsp->observes(stmt_class == SQL_STMT_CLASS_READ ? SQL_MUTATION_MASK_READ : SQL_MUTATION_MASK_EXEC)) {
        SqlMutationEvent ev(SQL_MUTATION_EVENT_PRE_EXEC, stmt_class);
        if (dsp->dispatchMutationEvent(ev, xsink)) {
            return -1;
        }
    }
    if (dsp->getMutationCtx() && !dsp->in_transaction) {
        dsp->dispatchMutationTxBegin(xsink);
    }
    return 0;
}

//! mutation observer result boundary for a prepared statement execution
static void stmt_mutation_post(qore_ds_private* dsp, int stmt_class, int rc, ExceptionSink* xsink) {
    if (!dsp->getMutationCtx()) {
        return;
    }
    if (dsp->observes(stmt_class == SQL_STMT_CLASS_READ ? SQL_MUTATION_MASK_READ : SQL_MUTATION_MASK_EXEC)) {
        SqlMutationEvent ev(SQL_MUTATION_EVENT_POST_EXEC, stmt_class);
        if (dsp->connection_aborted) {
            ev.outcome = SQL_MUTATION_OUTCOME_LOST_CONNECTION;
            // no commit was in flight, so the operation can be replayed
            ev.replay_safe = 1;
        } else {
            ev.outcome = (rc || *xsink) ? SQL_MUTATION_OUTCOME_ERROR : SQL_MUTATION_OUTCOME_OK;
        }
        ev.driver_xsink = xsink;
        dsp->dispatchMutationEvent(ev, xsink);
    }
    dsp->flushMutationConnectionLost(xsink);
}

int QoreSQLStatement::execIntern(DBActionHelper& dba, ExceptionSink* xsink) {
    qore_ds_private* dsp = priv->ds->priv;

    if (stmt_mutation_pre(dsp, SQL_STMT_CLASS_STMT_EXEC, xsink)) {
        return -1;
    }

    int rc = qore_dbi_private::get(*priv->ds->getDriver())->stmt_exec(this, xsink);
    if (!rc)
        status = STMT_EXECED;

    //printd(5, "QoreSQLStatement::execIntern() this: %p ds: %p: %s@%s: %s\n", this, priv->ds, priv->ds->getUsername(), priv->ds->getDBName(), str.getBuffer());

    stmt_mutation_post(dsp, SQL_STMT_CLASS_STMT_EXEC, rc, xsink);

    priv->ds->priv->statementExecuted(rc);
    return rc;
}

int QoreSQLStatement::execDescribeIntern(DBActionHelper& dba, ExceptionSink* xsink) {
    qore_ds_private* dsp = priv->ds->priv;

    if (stmt_mutation_pre(dsp, SQL_STMT_CLASS_READ, xsink)) {
        return -1;
    }

    int rc = qore_dbi_private::get(*priv->ds->getDriver())->stmt_exec_describe(this, xsink);
    if (!rc)
        status = STMT_EXECED;

    //printd(5, "QoreSQLStatement::execIntern() this: %p ds: %p: %s@%s: %s\n", this, priv->ds, priv->ds->getUsername(), priv->ds->getDBName(), str.getBuffer());

    stmt_mutation_post(dsp, SQL_STMT_CLASS_READ, rc, xsink);

    priv->ds->priv->statementExecuted(rc);
    return rc;
}

int QoreSQLStatement::affectedRows(ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return -1;

    if (checkStatus(xsink, dba, STMT_EXECED, "affectedRows"))
        return -1;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_affected_rows(this, xsink);
}

QoreHashNode* QoreSQLStatement::getOutput(ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return nullptr;

    if (checkStatus(xsink, dba, STMT_EXECED, "getOutput"))
        return nullptr;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_get_output(this, xsink);
}

QoreHashNode* QoreSQLStatement::getOutputRows(ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return nullptr;

    if (checkStatus(xsink, dba, STMT_EXECED, "getOutputRows"))
        return nullptr;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_get_output_rows(this, xsink);
}

bool QoreSQLStatement::next(ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return (validp = false);

    if (checkStatus(xsink, dba, STMT_DEFINED, "next"))
        return (validp = false);

    return (validp = qore_dbi_private::get(*priv->ds->getDriver())->stmt_next(this, xsink));
}

bool QoreSQLStatement::valid() {
    return validp;
}

int QoreSQLStatement::define(ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return false;

    if (checkStatus(xsink, dba, STMT_EXECED, "define"))
        return false;

    return defineIntern(xsink);
}

int QoreSQLStatement::defineIntern(ExceptionSink* xsink) {
    int rc = qore_dbi_private::get(*priv->ds->getDriver())->stmt_define(this, xsink);
    if (!rc)
        status = STMT_DEFINED;
    return rc;
}

QoreHashNode* QoreSQLStatement::fetchRow(ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return nullptr;

    if (checkStatus(xsink, dba, STMT_DEFINED, "fetchRow"))
        return nullptr;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_fetch_row(this, xsink);
}

QoreListNode* QoreSQLStatement::fetchRows(int rows, ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return nullptr;

    if (checkStatus(xsink, dba, STMT_DEFINED, "fetchRows"))
        return nullptr;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_fetch_rows(this, rows, xsink);
}

QoreHashNode* QoreSQLStatement::fetchColumns(int rows, ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return nullptr;

    if (checkStatus(xsink, dba, STMT_DEFINED, "fetchColumns"))
        return nullptr;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_fetch_columns(this, rows, xsink);
}

QoreColumnarResult* QoreSQLStatement::fetchColumnar(int rows, ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return nullptr;

    if (checkStatus(xsink, dba, STMT_DEFINED, "fetchColumnar"))
        return nullptr;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_fetch_columnar(this, rows, xsink);
}

QoreHashNode* QoreSQLStatement::describe(ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
    if (!dba)
        return nullptr;

    if (checkStatus(xsink, dba, STMT_DEFINED, "describe"))
        return nullptr;

    return qore_dbi_private::get(*priv->ds->getDriver())->stmt_describe(this, xsink);
}

bool QoreSQLStatement::active() const {
   return status != STMT_IDLE;
}

bool QoreSQLStatement::currentThreadInTransaction(ExceptionSink* xsink) {
    DBActionHelper dba(*this, xsink, DAH_NOCHANGE);
    if (!dba)
       return false;

    return dba.inTransaction();
}

int QoreSQLStatement::close(ExceptionSink* xsink) {
   DBActionHelper dba(*this, xsink, DAH_NOCHANGE);
   if (!dba)
      return -1;

   return closeIntern(xsink);
}

int QoreSQLStatement::commit(ExceptionSink* xsink) {
   DBActionHelper dba(*this, xsink, DAH_RELEASE);
   if (!dba)
      return -1;

   int rc = closeIntern(xsink);
   rc = qore_ds_private::get(*priv->ds)->commitIntern(xsink);
   //printd(5, "QoreSQLStatement::commit() this: %p ds: %p rc: %d\n", this, priv->ds, rc);
   return rc;
}

int QoreSQLStatement::rollback(ExceptionSink* xsink) {
   DBActionHelper dba(*this, xsink, DAH_RELEASE);
   if (!dba)
      return -1;

   int rc = closeIntern(xsink);
   rc = qore_ds_private::get(*priv->ds)->rollbackIntern(xsink);
   //printd(5, "QoreSQLStatement::rollback() this: %p ds: %p rc: %d\n", this, priv->ds, rc);
   return rc;
}

int QoreSQLStatement::beginTransaction(ExceptionSink* xsink) {
   DBActionHelper dba(*this, xsink, DAH_ACQUIRE);
   if (!dba)
      return -1;

   return priv->ds->beginTransaction(xsink);
}

QoreStringNode* QoreSQLStatement::getSQL(ExceptionSink* xsink) {
   // we have to acquire the datasource in order to use the thread lock to access the SQL string
   DBActionHelper dba(*this, xsink, DAH_NOCHANGE);
   if (!dba)
      return nullptr;

   return str.empty() ? nullptr : new QoreStringNode(str);
}

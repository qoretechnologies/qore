/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_ds_private.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include <set>

typedef std::set<QoreSQLStatement*> stmt_set_t;

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

    // interface for the parent class
    DatasourceStatementHelper* dsh;

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
            dsh(dsh) {
    }

    DLLLOCAL ~qore_ds_private() {
        assert(!private_data);
        assert(stmt_set.empty());
        ExceptionSink xsink;
        if (opt) {
            opt->deref(&xsink);
        }
        event_arg.discard(&xsink);
        if (event_queue) {
            event_queue->deref(&xsink);
        }
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
        return qore_dbi_private::get(*dsl)->commit(ds, xsink);
    }

    DLLLOCAL int rollbackIntern(ExceptionSink* xsink) {
        //printd(5, "qore_ds_private::rollbackIntern() this: %p in_transaction: %d active_transaction: %d\n", this, in_transaction, active_transaction);
        assert(isopen);
        in_transaction = false;
        active_transaction = false;
        return qore_dbi_private::get(*dsl)->rollback(ds, xsink);
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
            qore_dbi_private::get(*dsl)->close(ds);
            isopen = false;
            in_transaction = false;
            active_transaction = false;
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

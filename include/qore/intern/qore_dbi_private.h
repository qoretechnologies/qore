/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_dbi_private.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORE_DBI_PRIVATE_H
#define _QORE_QORE_DBI_PRIVATE_H

#include <map>

// internal DBI definitions
typedef std::map<int, void*> dbi_method_list_t;
struct DbiOptInfo {
    const char* desc = nullptr;
    const QoreTypeInfo *typeInfo = nullptr;

    DLLLOCAL DbiOptInfo() {
    }

    DLLLOCAL DbiOptInfo(const char* d, const QoreTypeInfo* t) : desc(d), typeInfo(t) {
    }
};
typedef std::map<const char*, DbiOptInfo, ltcstrcase> dbi_opt_map_t;

struct dbi_driver_stmt {
    q_dbi_stmt_prepare_t prepare = nullptr;
    q_dbi_stmt_prepare_raw_t prepare_raw = nullptr;
    q_dbi_stmt_bind_t bind = nullptr,
        bind_placeholders = nullptr,
        bind_values = nullptr;
    q_dbi_stmt_exec_t exec = nullptr,
        exec_describe = nullptr;
    q_dbi_stmt_fetch_row_t fetch_row = nullptr;
    q_dbi_stmt_fetch_rows_t fetch_rows = nullptr;
    q_dbi_stmt_fetch_columns_t fetch_columns = nullptr;
    q_dbi_stmt_fetch_row_t describe = nullptr;
    q_dbi_stmt_next_t next = nullptr;
    q_dbi_stmt_define_t define = nullptr;
    q_dbi_stmt_close_t close = nullptr,
        free = nullptr;
    q_dbi_stmt_affected_rows_t affected_rows = nullptr;
    q_dbi_stmt_get_output_t get_output = nullptr;
    q_dbi_stmt_get_output_rows_t get_output_rows = nullptr;

    DLLLOCAL dbi_driver_stmt() {
    }
};

struct dbi_driver_opt {
    q_dbi_option_set_t set = nullptr;
    q_dbi_option_get_t get = nullptr;

    DLLLOCAL dbi_driver_opt() {
    }
};

struct DBIDriverFunctions {
    q_dbi_open_t open = nullptr;
    q_dbi_close_t close = nullptr;
    q_dbi_select_t select = nullptr;
    q_dbi_select_rows_t selectRows = nullptr;
    q_dbi_select_row_t selectRow = nullptr;
    q_dbi_exec_t execSQL = nullptr;
    q_dbi_execraw_t execRawSQL = nullptr;
    q_dbi_describe_t describe = nullptr;
    q_dbi_commit_t commit = nullptr;
    q_dbi_rollback_t rollback = nullptr;
    q_dbi_begin_transaction_t begin_transaction = nullptr; // for DBI drivers that require explicit transaction starts
    q_dbi_get_server_version_t get_server_version = nullptr;
    q_dbi_get_client_version_t get_client_version = nullptr;
    q_dbi_get_driver_real_name_t get_driver_real_name = nullptr;

    dbi_driver_stmt stmt;
    dbi_driver_opt opt;

    DLLLOCAL DBIDriverFunctions() {
    }
};

// helper class that will edit argument lists and convert number values to floats if the driver does not support the "number" type (QoreNumberNode)
class DbiArgHelper {
protected:
    const QoreListNode* orig;
    QoreListNode* nl;
    ExceptionSink* xsink;

public:
    DLLLOCAL DbiArgHelper(const QoreListNode* ol, bool numeric, ExceptionSink* xs);

    DLLLOCAL ~DbiArgHelper() {
        if (nl)
            nl->deref(xsink);
    }

    DLLLOCAL const QoreListNode* get() const {
        return nl ? nl : orig;
    }

    DLLLOCAL const QoreListNode* operator*() const {
        return nl ? nl : orig;
    }
};

struct OptInputHelper {
    ExceptionSink* xsink;
    QoreValue val{};
    bool tmp;

    DLLLOCAL OptInputHelper(ExceptionSink* xs, const qore_dbi_private& driver, const char* opt, bool set = false, const QoreValue v = QoreValue());

    DLLLOCAL ~OptInputHelper() {
        if (tmp)
            val.discard(xsink);
    }

    DLLLOCAL operator bool() const {
        assert(xsink);
        return !*xsink;
    }
};

struct qore_dbi_private {
    const char* name;
    int caps;
    DBIDriverFunctions f;
    dbi_opt_map_t omap;

    DLLLOCAL qore_dbi_private(const char* nme, const qore_dbi_mlist_private& methods, int cps);

    DLLLOCAL bool hasStatementAPI() const {
        return caps & DBI_CAP_HAS_STATEMENT;
    }

    DLLLOCAL int init(Datasource* ds, ExceptionSink* xsink) const {
        assert(xsink);
        int rc = f.open(ds, xsink);
        assert((!rc && !*xsink) || (rc && *xsink));
        // set option if init was successful
        if (!rc && f.opt.set) {
            ConstHashIterator hi(ds->getConnectOptions());
            while (hi.next()) {
                f.opt.set(ds, hi.getKey(), hi.get(), xsink);
            }
        }
        return rc;
    }

    DLLLOCAL int close(Datasource* ds) const {
        return f.close(ds);
    }

    DLLLOCAL QoreValue select(Datasource* ds, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) const {
        DbiArgHelper dargs(args, (caps & DBI_CAP_HAS_NUMBER_SUPPORT), xsink);
        return f.select(ds, sql, *dargs, xsink);
    }

    DLLLOCAL QoreValue selectRows(Datasource* ds, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) const {
        DbiArgHelper dargs(args, (caps & DBI_CAP_HAS_NUMBER_SUPPORT), xsink);
        return f.selectRows(ds, sql, *dargs, xsink);
    }

    DLLLOCAL QoreHashNode* selectRow(Datasource* ds, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) const {
        DbiArgHelper dargs(args, (caps & DBI_CAP_HAS_NUMBER_SUPPORT), xsink);

        if (f.selectRow) {
            return f.selectRow(ds, sql, *dargs, xsink);
        }

        ValueHolder res(f.selectRows(ds, sql, *dargs, xsink), xsink);
        if (!res) {
            return nullptr;
        }

        if (res->getType() != NT_HASH) {
            assert(res->getType() == NT_LIST);
            assert(res->getInternalNode()->reference_count() == 1);
            QoreListNode* l = res->get<QoreListNode>();
            assert(l->size() <= 1);
            QoreValue n = l->shift();
            assert(n.isNothing() || n.getType() == NT_HASH);
            return n.get<QoreHashNode>();
        }

        return res.release().get<QoreHashNode>();
    }

    DLLLOCAL QoreValue execSQL(Datasource* ds, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) const {
        DbiArgHelper dargs(args, (caps & DBI_CAP_HAS_NUMBER_SUPPORT), xsink);
        return f.execSQL(ds, sql, *dargs, xsink);
    }

    DLLLOCAL QoreValue execRawSQL(Datasource* ds, const QoreString* sql, ExceptionSink* xsink) const {
        if (!f.execRawSQL) {
            xsink->raiseException("DBI-EXEC-RAW-SQL-ERROR", "this driver does not implement the Datasource::execRawSQL() method");
            return QoreValue();
        }
        return f.execRawSQL(ds, sql, xsink);
    }

    DLLLOCAL QoreHashNode* describe(Datasource* ds, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
        if (!f.describe) {
            xsink->raiseException("DBI-DESCRIBE-ERROR", "this driver does not implement the Datasource::describe() method");
            return nullptr;
        }
        DbiArgHelper dargs(args, (caps & DBI_CAP_HAS_NUMBER_SUPPORT), xsink);
        return f.describe(ds, sql, *dargs, xsink);
    }

    DLLLOCAL int commit(Datasource* ds, ExceptionSink* xsink) const {
        return f.commit(ds, xsink);
    }

    DLLLOCAL int rollback(Datasource* ds, ExceptionSink* xsink) const {
        return f.rollback(ds, xsink);
    }

    DLLLOCAL int beginTransaction(Datasource* ds, ExceptionSink* xsink) const {
        if (f.begin_transaction)
            return f.begin_transaction(ds, xsink);
        return 0; // 0 = OK
    }

    DLLLOCAL int autoCommit(Datasource* ds, ExceptionSink* xsink) const {
        // if the driver does not require explicit "begin" statements to
        // start a transaction, then we have to explicitly call "commit" here
        if (!f.begin_transaction)
            return f.commit(ds, xsink);

        return 0; // 0 = OK
    }

    DLLLOCAL QoreValue getServerVersion(Datasource* ds, ExceptionSink* xsink) const {
        if (f.get_server_version)
            return f.get_server_version(ds, xsink);
        return QoreValue();
    }

    DLLLOCAL QoreValue getClientVersion(const Datasource* ds, ExceptionSink* xsink) const {
        if (f.get_client_version)
            return f.get_client_version(ds, xsink);
        return QoreValue();
    }

    DLLLOCAL QoreStringNode* getDriverRealName(Datasource* ds, ExceptionSink* xsink) const {
        if (f.get_driver_real_name) {
            return f.get_driver_real_name(ds, xsink);
        }
        return new QoreStringNode(ds->getDriverName());
    }

    DLLLOCAL int getCaps() const {
        return caps;
    }

    DLLLOCAL QoreListNode* getCapList() const;

    DLLLOCAL bool hasExecDefine() const {
        return (bool)f.stmt.exec_describe;
    }

    DLLLOCAL int stmt_prepare(SQLStatement* stmt, const QoreString& str, const QoreListNode* args, ExceptionSink* xsink) const {
        DbiArgHelper dargs(args, (caps & DBI_CAP_HAS_NUMBER_SUPPORT), xsink);
        return f.stmt.prepare(stmt, str, *dargs, xsink);
    }

    DLLLOCAL int stmt_prepare_raw(SQLStatement* stmt, const QoreString& str, ExceptionSink* xsink) const {
        return f.stmt.prepare_raw(stmt, str, xsink);
    }

    DLLLOCAL int stmt_bind(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) const {
        return f.stmt.bind(stmt, l, xsink);
    }

    DLLLOCAL int stmt_bind_placeholders(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) const {
        if (!f.stmt.bind_placeholders) {
            xsink->raiseException("SQLSTATEMENT-BIND-PLACEHOLDERS-ERROR", "the '%s' driver does not require placeholder buffer specifications so the SQLStatement::bindPlaceholders() method is not supported", name);
            return -1;
        }

        return f.stmt.bind_placeholders(stmt, l, xsink);
    }

    DLLLOCAL int stmt_bind_values(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) const {
        return f.stmt.bind_values(stmt, l, xsink);
    }

    DLLLOCAL int stmt_define(SQLStatement* stmt, ExceptionSink* xsink) const {
        return f.stmt.define(stmt, xsink);
    }

    DLLLOCAL int stmt_exec_describe(SQLStatement* stmt, ExceptionSink* xsink) const {
        assert(f.stmt.exec_describe);
        return f.stmt.exec_describe(stmt, xsink);
    }

    DLLLOCAL int stmt_exec(SQLStatement* stmt, ExceptionSink* xsink) const {
        return f.stmt.exec(stmt, xsink);
    }

    DLLLOCAL int stmt_affected_rows(SQLStatement* stmt, ExceptionSink* xsink) const {
        return f.stmt.affected_rows(stmt, xsink);
    }

    DLLLOCAL QoreHashNode* stmt_get_output(SQLStatement* stmt, ExceptionSink* xsink) const {
        return f.stmt.get_output(stmt, xsink);
    }

    DLLLOCAL QoreHashNode* stmt_get_output_rows(SQLStatement* stmt, ExceptionSink* xsink) const {
        return f.stmt.get_output_rows(stmt, xsink);
    }

    DLLLOCAL QoreHashNode* stmt_fetch_row(SQLStatement* stmt, ExceptionSink* xsink) const {
        return f.stmt.fetch_row(stmt, xsink);
    }

    DLLLOCAL QoreListNode* stmt_fetch_rows(SQLStatement* stmt, int rows, ExceptionSink* xsink) const {
        return f.stmt.fetch_rows(stmt, rows, xsink);
    }

    DLLLOCAL QoreHashNode* stmt_fetch_columns(SQLStatement* stmt, int rows, ExceptionSink* xsink) const {
        return f.stmt.fetch_columns(stmt, rows, xsink);
    }

    DLLLOCAL QoreHashNode* stmt_describe(SQLStatement* stmt, ExceptionSink* xsink) const {
        if (!f.stmt.describe) {
            xsink->raiseException("DBI-DESCRIBE-ERROR", "this driver does not implement the SQLStatement::describe() method");
            return nullptr;
        }
        return f.stmt.describe(stmt, xsink);
    }

    DLLLOCAL bool stmt_next(SQLStatement* stmt, ExceptionSink* xsink) const {
        return f.stmt.next(stmt, xsink);
    }

    DLLLOCAL int stmt_close(SQLStatement* stmt, ExceptionSink* xsink) const {
        return f.stmt.close(stmt, xsink);
    }

    DLLLOCAL int stmt_free(SQLStatement* stmt, ExceptionSink* xsink) const {
        return f.stmt.free ? f.stmt.free(stmt, xsink) : 0;
    }

    DLLLOCAL int opt_set(Datasource* ds, const char* opt, const QoreValue val, ExceptionSink* xsink) {
        OptInputHelper oh(xsink, *this, opt, true, val);
        if (!oh)
            return -1;

        return f.opt.set(ds, opt, oh.val, xsink);
    }

    DLLLOCAL QoreValue opt_get(const Datasource* ds, const char* opt, ExceptionSink* xsink) {
        OptInputHelper oh(xsink, *this, opt);
        if (!oh)
            return QoreValue();

        return f.opt.get(ds, opt);
    }

    DLLLOCAL QoreHashNode* getOptionHash(const Datasource* ds) const {
        QoreHashNode* rv = new QoreHashNode(autoTypeInfo);

        for (dbi_opt_map_t::const_iterator i = omap.begin(), e = omap.end(); i != e; ++i) {
            QoreHashNode* h = new QoreHashNode(autoTypeInfo);
            h->setKeyValue("desc", new QoreStringNode(i->second.desc), 0);
            h->setKeyValue("type", new QoreStringNode(QoreTypeInfo::getName(i->second.typeInfo)), 0);
            h->setKeyValue("value", f.opt.get(ds, i->first), 0);

            rv->setKeyValue(i->first, h, 0);
        }
        return rv;
    }

    DLLLOCAL QoreHashNode* getOptionHash() const {
        QoreHashNode* rv = new QoreHashNode(autoTypeInfo);

        for (dbi_opt_map_t::const_iterator i = omap.begin(), e = omap.end(); i != e; ++i) {
            QoreHashNode* h = new QoreHashNode(autoTypeInfo);
            h->setKeyValue("desc", new QoreStringNode(i->second.desc), 0);
            h->setKeyValue("type", new QoreStringNode(QoreTypeInfo::getName(i->second.typeInfo)), 0);

            rv->setKeyValue(i->first, h, 0);
        }
        return rv;
    }

    DLLLOCAL static qore_dbi_private* get(const DBIDriver& d) {
        return d.priv;
    }
};

#endif

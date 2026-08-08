/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    dbi_test-module.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    A mock DBI driver used only by the %Qore test suite.  It connects to nothing and can be made to
    fail on command, which is what makes the datasource mutation observer outcome boundaries --
    driver exception, restartable failover, and commit ambiguity -- testable deterministically with
    no external database.

    This module is deliberately not installed; see modules/dbi_test/CMakeLists.txt.

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
#include <qore/DBI.h>
#include <qore/SQLStatement.h>

#include <cstring>
#include <string>

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.0"
#endif

//! driver option selecting a fault to inject; see dbitest_fault_t
#define DBITEST_OPT_FAULT "fault"
//! driver option giving the number of rows reported as affected by exec operations
#define DBITEST_OPT_ROWS "rows"
//! driver option giving the chunk size used by the simulated bounded stream
#define DBITEST_OPT_STREAM_CHUNK "stream-chunk"

static DBIDriver* DBID_DBITEST = nullptr;

static void dbitest_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void dbitest_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void dbitest_module_delete();

//! per-connection state
class DbiTestConn {
public:
    //! the currently armed fault; sticky until changed with the "fault" option
    std::string fault;
    //! rows reported as affected by exec operations
    int64 rows = 1;
    //! chunk size for the simulated bounded stream
    int64 stream_chunk = 4096;

    DLLLOCAL bool faultIs(const char* f) const {
        return fault == f;
    }
};

//! per-statement state
class DbiTestStmt {
public:
    QoreString sql;
    //! index of the next row to return from next()
    unsigned row = 0;
};

static DbiTestConn* get_conn(Datasource* ds) {
    return reinterpret_cast<DbiTestConn*>(ds->getPrivateData());
}

//! returns the canned result set as a hash of column lists
static QoreHashNode* dbitest_columns() {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);
    QoreListNode* ids = new QoreListNode(autoTypeInfo);
    ids->push(1LL, nullptr);
    ids->push(2LL, nullptr);
    QoreListNode* vals = new QoreListNode(autoTypeInfo);
    vals->push(new QoreStringNode("a"), nullptr);
    vals->push(new QoreStringNode("b"), nullptr);
    h->setKeyValue("id", ids, nullptr);
    h->setKeyValue("val", vals, nullptr);
    return h.release();
}

//! returns row \a i of the canned result set
static QoreHashNode* dbitest_row(unsigned i) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);
    h->setKeyValue("id", (int64)(i + 1), nullptr);
    h->setKeyValue("val", new QoreStringNode(i ? "b" : "a"), nullptr);
    return h.release();
}

static QoreListNode* dbitest_rows() {
    ReferenceHolder<QoreListNode> l(new QoreListNode(autoTypeInfo), nullptr);
    l->push(dbitest_row(0), nullptr);
    l->push(dbitest_row(1), nullptr);
    return l.release();
}

static QoreHashNode* dbitest_describe() {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), nullptr);
    for (unsigned i = 0; i < 2; ++i) {
        ReferenceHolder<QoreHashNode> col(new QoreHashNode(autoTypeInfo), nullptr);
        col->setKeyValue("name", new QoreStringNode(i ? "val" : "id"), nullptr);
        col->setKeyValue("native_type", new QoreStringNode(i ? "text" : "integer"), nullptr);
        col->setKeyValue("internal_id", (int64)i, nullptr);
        col->setKeyValue("maxsize", (int64)(i ? 64 : 8), nullptr);
        h->setKeyValue(i ? "val" : "id", col.release(), nullptr);
    }
    return h.release();
}

//! drives a simulated bounded stream through the datasource mutation observer stream API
/** This is the C++ API a real driver uses while streaming a \c COPY payload: the core never sees
    the payload, only the declared and consumed byte counts.

    @return 0 for OK, -1 if the observer stopped the stream
*/
static int dbitest_run_copy(Datasource* ds, int64 total_bytes, int64 chunk, ExceptionSink* xsink) {
    if (chunk <= 0) {
        chunk = 4096;
    }
    DbiTestConn* conn = get_conn(ds);
    if (ds->reportMutationStreamBegin(total_bytes, xsink)) {
        // the consumer refused the stream before it started; a real driver sends nothing and
        // reports no end boundary of its own
        return -1;
    }
    int64 consumed = 0;
    while (consumed < total_bytes) {
        consumed += chunk;
        if (consumed > total_bytes) {
            consumed = total_bytes;
        }
        if (conn->faultIs("stream") && consumed >= total_bytes / 2) {
            // the server failed while the payload was being sent; this is a driver failure and not
            // a consumer rejection, and the two must remain distinguishable at the end boundary
            xsink->raiseException("DBI-TEST-STREAM-ERROR", "injected stream failure");
            ExceptionSink end_xsink;
            ds->reportMutationStreamEnd(consumed, false, &end_xsink);
            end_xsink.clear();
            return -1;
        }
        if (ds->reportMutationStreamProgress(consumed, xsink)) {
            // the caller aborted the stream; report the end so that the consumer sees the boundary
            ExceptionSink end_xsink;
            ds->reportMutationStreamEnd(consumed, false, &end_xsink);
            end_xsink.clear();
            return -1;
        }
    }
    ds->reportMutationStreamEnd(consumed, true, xsink);
    return 0;
}

/* ------------------------------------------------------------------------- connection methods */

static int dbitest_open(Datasource* ds, ExceptionSink* xsink) {
    assert(!ds->getPrivateData());
    ds->setPrivateData(new DbiTestConn);
    return 0;
}

static int dbitest_close(Datasource* ds) {
    DbiTestConn* conn = get_conn(ds);
    ds->setPrivateData(nullptr);
    delete conn;
    return 0;
}

static QoreValue dbitest_select(Datasource* ds, const QoreString* str, const QoreListNode* args,
        ExceptionSink* xsink) {
    DbiTestConn* conn = get_conn(ds);
    if (conn->faultIs("select")) {
        xsink->raiseException("DBI-TEST-SELECT-ERROR", "injected select failure");
        return QoreValue();
    }
    return dbitest_columns();
}

static QoreValue dbitest_select_rows(Datasource* ds, const QoreString* str, const QoreListNode* args,
        ExceptionSink* xsink) {
    DbiTestConn* conn = get_conn(ds);
    if (conn->faultIs("select")) {
        xsink->raiseException("DBI-TEST-SELECT-ERROR", "injected select failure");
        return QoreValue();
    }
    return dbitest_rows();
}

static QoreHashNode* dbitest_select_row(Datasource* ds, const QoreString* str, const QoreListNode* args,
        ExceptionSink* xsink) {
    DbiTestConn* conn = get_conn(ds);
    if (conn->faultIs("select")) {
        xsink->raiseException("DBI-TEST-SELECT-ERROR", "injected select failure");
        return nullptr;
    }
    return dbitest_row(0);
}

static QoreHashNode* dbitest_describe_meth(Datasource* ds, const QoreString* str, const QoreListNode* args,
        ExceptionSink* xsink) {
    DbiTestConn* conn = get_conn(ds);
    if (conn->faultIs("select")) {
        xsink->raiseException("DBI-TEST-SELECT-ERROR", "injected select failure");
        return nullptr;
    }
    return dbitest_describe();
}

//! shared implementation for exec and execRaw
static QoreValue dbitest_exec_intern(Datasource* ds, const QoreString* str, ExceptionSink* xsink) {
    DbiTestConn* conn = get_conn(ds);

    // a real driver inspects its own command text; the ban on classification applies to the core,
    // not to drivers
    if (str && !strncmp(str->c_str(), "dbitest-copy ", 13)) {
        int64 total = strtoll(str->c_str() + 13, nullptr, 10);
        const int64 chunk = conn->stream_chunk;
        if (dbitest_run_copy(ds, total, chunk, xsink)) {
            return QoreValue();
        }
        return total;
    }

    if (conn->faultIs("abort-on-exec")) {
        xsink->raiseException("DBI-TEST-CONNECTION-ERROR", "injected connection loss while executing a statement");
        ds->connectionAborted(xsink);
        return QoreValue();
    }
    if (conn->faultIs("exec")) {
        xsink->raiseException("DBI-TEST-EXEC-ERROR", "injected exec failure");
        return QoreValue();
    }
    return conn->rows;
}

static QoreValue dbitest_exec(Datasource* ds, const QoreString* str, const QoreListNode* args,
        ExceptionSink* xsink) {
    return dbitest_exec_intern(ds, str, xsink);
}

static QoreValue dbitest_execraw(Datasource* ds, const QoreString* str, ExceptionSink* xsink) {
    return dbitest_exec_intern(ds, str, xsink);
}

static int dbitest_commit(Datasource* ds, ExceptionSink* xsink) {
    DbiTestConn* conn = get_conn(ds);
    if (conn->faultIs("abort-on-commit")) {
        xsink->raiseException("DBI-TEST-CONNECTION-ERROR", "injected connection loss while committing");
        ds->connectionAborted(xsink);
        return -1;
    }
    if (conn->faultIs("commit")) {
        xsink->raiseException("DBI-TEST-COMMIT-ERROR", "injected commit failure");
        return -1;
    }
    return 0;
}

static int dbitest_rollback(Datasource* ds, ExceptionSink* xsink) {
    DbiTestConn* conn = get_conn(ds);
    if (conn->faultIs("rollback")) {
        xsink->raiseException("DBI-TEST-ROLLBACK-ERROR", "injected rollback failure");
        return -1;
    }
    return 0;
}

static QoreValue dbitest_get_server_version(Datasource* ds, ExceptionSink* xsink) {
    return new QoreStringNode("dbitest-server-1.0");
}

static QoreValue dbitest_get_client_version(const Datasource* ds, ExceptionSink* xsink) {
    return new QoreStringNode("dbitest-client-1.0");
}

static QoreStringNode* dbitest_get_driver_real_name(Datasource* ds, ExceptionSink* xsink) {
    return new QoreStringNode("DBI Test Driver");
}

static int dbitest_opt_set(Datasource* ds, const char* opt, const QoreValue val, ExceptionSink* xsink) {
    DbiTestConn* conn = get_conn(ds);
    if (!conn) {
        // options set while the connection is closed are stored by the core and replayed on open()
        return 0;
    }
    if (!strcmp(opt, DBITEST_OPT_FAULT)) {
        QoreStringNodeValueHelper str(val);
        conn->fault = str->c_str();
        return 0;
    }
    if (!strcmp(opt, DBITEST_OPT_ROWS)) {
        conn->rows = val.getAsBigInt();
        return 0;
    }
    if (!strcmp(opt, DBITEST_OPT_STREAM_CHUNK)) {
        conn->stream_chunk = val.getAsBigInt();
        return 0;
    }
    return 0;
}

static QoreValue dbitest_opt_get(const Datasource* ds, const char* opt) {
    DbiTestConn* conn = reinterpret_cast<DbiTestConn*>(const_cast<Datasource*>(ds)->getPrivateData());
    if (!conn) {
        return QoreValue();
    }
    if (!strcmp(opt, DBITEST_OPT_FAULT)) {
        return new QoreStringNode(conn->fault);
    }
    if (!strcmp(opt, DBITEST_OPT_ROWS)) {
        return conn->rows;
    }
    if (!strcmp(opt, DBITEST_OPT_STREAM_CHUNK)) {
        return conn->stream_chunk;
    }
    return QoreValue();
}

/* -------------------------------------------------------------------------- statement methods */

static DbiTestStmt* get_stmt(SQLStatement* stmt) {
    return reinterpret_cast<DbiTestStmt*>(stmt->getPrivateData());
}

static int dbitest_stmt_prepare(SQLStatement* stmt, const QoreString& str, const QoreListNode* args,
        ExceptionSink* xsink) {
    DbiTestStmt* s = get_stmt(stmt);
    if (!s) {
        s = new DbiTestStmt;
        stmt->setPrivateData(s);
    }
    s->sql = str;
    s->row = 0;
    return 0;
}

static int dbitest_stmt_prepare_raw(SQLStatement* stmt, const QoreString& str, ExceptionSink* xsink) {
    return dbitest_stmt_prepare(stmt, str, nullptr, xsink);
}

static int dbitest_stmt_bind(SQLStatement* stmt, const QoreListNode& l, ExceptionSink* xsink) {
    return 0;
}

static int dbitest_stmt_exec(SQLStatement* stmt, ExceptionSink* xsink) {
    DbiTestStmt* s = get_stmt(stmt);
    if (s) {
        s->row = 0;
    }
    Datasource* ds = stmt->getDatasource();
    DbiTestConn* conn = ds ? get_conn(ds) : nullptr;
    if (conn) {
        if (conn->faultIs("abort-on-exec")) {
            xsink->raiseException("DBI-TEST-CONNECTION-ERROR",
                "injected connection loss while executing a statement");
            ds->connectionAborted(xsink);
            return -1;
        }
        if (conn->faultIs("stmt-exec") || conn->faultIs("exec")) {
            xsink->raiseException("DBI-TEST-STMT-EXEC-ERROR", "injected statement exec failure");
            return -1;
        }
    }
    return 0;
}

static int dbitest_stmt_exec_describe(SQLStatement* stmt, ExceptionSink* xsink) {
    return dbitest_stmt_exec(stmt, xsink);
}

static int dbitest_stmt_affected_rows(SQLStatement* stmt, ExceptionSink* xsink) {
    Datasource* ds = stmt->getDatasource();
    DbiTestConn* conn = ds ? get_conn(ds) : nullptr;
    return conn ? (int)conn->rows : 0;
}

static QoreHashNode* dbitest_stmt_get_output(SQLStatement* stmt, ExceptionSink* xsink) {
    return dbitest_columns();
}

static QoreHashNode* dbitest_stmt_get_output_rows(SQLStatement* stmt, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
    h->setKeyValue("rows", dbitest_rows(), xsink);
    return h.release();
}

static int dbitest_stmt_define(SQLStatement* stmt, ExceptionSink* xsink) {
    return 0;
}

static QoreHashNode* dbitest_stmt_fetch_row(SQLStatement* stmt, ExceptionSink* xsink) {
    DbiTestStmt* s = get_stmt(stmt);
    return (s && s->row) ? dbitest_row(s->row - 1) : dbitest_row(0);
}

static QoreListNode* dbitest_stmt_fetch_rows(SQLStatement* stmt, int rows, ExceptionSink* xsink) {
    return dbitest_rows();
}

static QoreHashNode* dbitest_stmt_fetch_columns(SQLStatement* stmt, int rows, ExceptionSink* xsink) {
    return dbitest_columns();
}

static QoreHashNode* dbitest_stmt_describe(SQLStatement* stmt, ExceptionSink* xsink) {
    return dbitest_describe();
}

static bool dbitest_stmt_next(SQLStatement* stmt, ExceptionSink* xsink) {
    DbiTestStmt* s = get_stmt(stmt);
    if (!s || s->row >= 2) {
        return false;
    }
    ++s->row;
    return true;
}

static int dbitest_stmt_close(SQLStatement* stmt, ExceptionSink* xsink) {
    DbiTestStmt* s = get_stmt(stmt);
    stmt->setPrivateData(nullptr);
    delete s;
    return 0;
}

/* ------------------------------------------------------------------------------ module glue */

static void dbitest_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    qore_dbi_method_list methods;
    methods.add(QDBI_METHOD_OPEN, dbitest_open);
    methods.add(QDBI_METHOD_CLOSE, dbitest_close);
    methods.add(QDBI_METHOD_SELECT, dbitest_select);
    methods.add(QDBI_METHOD_SELECT_ROWS, dbitest_select_rows);
    methods.add(QDBI_METHOD_SELECT_ROW, dbitest_select_row);
    methods.add(QDBI_METHOD_DESCRIBE, dbitest_describe_meth);
    methods.add(QDBI_METHOD_EXEC, dbitest_exec);
    methods.add(QDBI_METHOD_EXECRAW, dbitest_execraw);
    methods.add(QDBI_METHOD_COMMIT, dbitest_commit);
    methods.add(QDBI_METHOD_ROLLBACK, dbitest_rollback);
    // NOTE: no begin_transaction method is registered on purpose, so that the core issues an
    // explicit commit for autocommit operations; that is what makes autocommit commit ambiguity
    // reachable in tests
    methods.add(QDBI_METHOD_GET_SERVER_VERSION, dbitest_get_server_version);
    methods.add(QDBI_METHOD_GET_CLIENT_VERSION, dbitest_get_client_version);
    methods.add(QDBI_METHOD_GET_DRIVER_REAL_NAME, dbitest_get_driver_real_name);

    methods.add(QDBI_METHOD_STMT_PREPARE, dbitest_stmt_prepare);
    methods.add(QDBI_METHOD_STMT_PREPARE_RAW, dbitest_stmt_prepare_raw);
    methods.add(QDBI_METHOD_STMT_BIND, dbitest_stmt_bind);
    methods.add(QDBI_METHOD_STMT_BIND_PLACEHOLDERS, dbitest_stmt_bind);
    methods.add(QDBI_METHOD_STMT_BIND_VALUES, dbitest_stmt_bind);
    methods.add(QDBI_METHOD_STMT_EXEC, dbitest_stmt_exec);
    methods.add(QDBI_METHOD_STMT_EXEC_DESCRIBE, dbitest_stmt_exec_describe);
    methods.add(QDBI_METHOD_STMT_AFFECTED_ROWS, dbitest_stmt_affected_rows);
    methods.add(QDBI_METHOD_STMT_GET_OUTPUT, dbitest_stmt_get_output);
    methods.add(QDBI_METHOD_STMT_GET_OUTPUT_ROWS, dbitest_stmt_get_output_rows);
    methods.add(QDBI_METHOD_STMT_DEFINE, dbitest_stmt_define);
    methods.add(QDBI_METHOD_STMT_FETCH_ROW, dbitest_stmt_fetch_row);
    methods.add(QDBI_METHOD_STMT_FETCH_ROWS, dbitest_stmt_fetch_rows);
    methods.add(QDBI_METHOD_STMT_FETCH_COLUMNS, dbitest_stmt_fetch_columns);
    methods.add(QDBI_METHOD_STMT_DESCRIBE, dbitest_stmt_describe);
    methods.add(QDBI_METHOD_STMT_NEXT, dbitest_stmt_next);
    methods.add(QDBI_METHOD_STMT_CLOSE, dbitest_stmt_close);

    methods.add(QDBI_METHOD_OPT_SET, dbitest_opt_set);
    methods.add(QDBI_METHOD_OPT_GET, dbitest_opt_get);

    methods.registerOption(DBITEST_OPT_FAULT, "the fault to inject: \"\", \"select\", \"exec\", \"commit\", "
        "\"rollback\", \"abort-on-exec\", \"abort-on-commit\", \"stmt-exec\", or \"stream\"", stringTypeInfo);
    methods.registerOption(DBITEST_OPT_ROWS, "the number of rows reported as affected by exec operations",
        bigIntTypeInfo);
    methods.registerOption(DBITEST_OPT_STREAM_CHUNK, "the chunk size in bytes used by the simulated bounded stream",
        bigIntTypeInfo);

    DBID_DBITEST = DBI.registerDriver("dbitest", methods, DBI_CAP_TRANSACTION_MANAGEMENT | DBI_CAP_BIND_BY_VALUE
        | DBI_CAP_HAS_EXECRAW | DBI_CAP_HAS_SELECT_ROW | DBI_CAP_HAS_DESCRIBE);
}

static void dbitest_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
}

static void dbitest_module_delete() {
}

extern "C" DLLEXPORT void dbitest_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "dbitest";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "mock DBI driver for the Qore test suite";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "http://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = dbitest_module_init;
    mod_info.ns_init = dbitest_module_ns_init;
    mod_info.del = dbitest_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

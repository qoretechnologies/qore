/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file dbitargetprobe-module.cpp a test binary module with DBI drivers that report the destinations of their
    connections to the sandbox

    Test scaffolding for the sandbox check of DBI connections (qore#5747): before a driver opens a connection that code
    governed by a sandbox restricting network or filesystem access chose, the Qore library asks the driver for the
    destinations of the connection (QDBI_METHOD_GET_CONNECT_TARGETS), checks them against the sandbox, and gives the
    driver the checked destinations (Datasource::getCheckedConnectTargets()).  This module is built into the build
    tree and never installed.

    The driver \c "dbitargetprobe" connects to nothing.  It reports the destinations of a connection from the host name
    of the datasource:
    - a host name starting with \c "/" or \c "@": the UNIX socket of that path
    - \c "bad": an invalid destination (a hash without a host or a socket)
    - \c "none": no destination
    - \c "raise": the driver refuses the connection itself (\c DBITARGETPROBE-REFUSED)
    - any other value: a comma-separated list of hosts, each with the port of the datasource, or 5432

    <tt>select("checked")</tt> returns the checked destinations that the driver was given when the connection was
    opened (\c NOTHING if it was given none), and <tt>select("current")</tt> those that the datasource has now.

    The driver \c "dbinotargetprobe" is \c "dbitargetprobe" without the destination method: it cannot report the
    destinations of its connections.

    The driver \c "dbilocalprobe" declares DBI_CAP_LOCAL_ONLY and opens no destination: when a connection is opened, it
    checks the database name, if it is an absolute path, with qore_dbi_check_connect_path() for reading.
    <tt>select("restricted")</tt> returns qore_dbi_connect_restricted() for the connection.

    The functions of the namespace \c DbiTargetProbe report how often the drivers were called:
    - <tt>opens()</tt>: the number of connections that the drivers opened
    - <tt>targetCalls()</tt>: the number of calls of the destination method of \c "dbitargetprobe"
    - <tt>reset()</tt>: sets both to 0

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
*/
#include <qore/Qore.h>
#include <qore/QoreSandboxManager.h>

#include <atomic>
#include <cstring>
#include <string>

namespace {
//! the namespace of the module, copied into each Program that loads it
QoreNamespace ProbeNs("DbiTargetProbe");

//! the number of connections that the drivers opened
std::atomic<int64> probe_opens{0};

//! the number of calls of the destination method
std::atomic<int64> probe_target_calls{0};

//! the connection of a driver: the checked destinations that it was given when it was opened
struct ProbeConnection {
    QoreListNode* checked = nullptr;

    ~ProbeConnection() {
        if (checked) {
            checked->deref(nullptr);
        }
    }
};

int probe_open(Datasource* ds, ExceptionSink* xsink) {
    ProbeConnection* conn = new ProbeConnection;
#ifdef QDBI_METHOD_GET_CONNECT_TARGETS
    if (const QoreListNode* checked = ds->getCheckedConnectTargets()) {
        conn->checked = checked->listRefSelf();
    }
#endif
    ds->setPrivateData(conn);
    ds->setQoreEncoding(QCS_DEFAULT);
    ++probe_opens;
    return 0;
}

int probe_close(Datasource* ds) {
    delete ds->getPrivateData<ProbeConnection>();
    ds->setPrivateData(nullptr);
    return 0;
}

#ifdef QDBI_METHOD_GET_CONNECT_TARGETS
//! returns the destinations of a connection from the host name of the datasource; see the file comment
QoreListNode* probe_get_connect_targets(Datasource* ds, ExceptionSink* xsink) {
    ++probe_target_calls;
    std::string host = ds->getHostName() ? ds->getHostName() : "";
    if (host == "raise") {
        xsink->raiseException("DBITARGETPROBE-REFUSED", "the driver refuses the connection");
        return nullptr;
    }
    ReferenceHolder<QoreListNode> rv(new QoreListNode(autoTypeInfo), xsink);
    if (host == "none") {
        return rv.release();
    }
    if (host == "bad") {
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
        h->setKeyValue("port", 5432, xsink);
        rv->push(h.release(), xsink);
        return rv.release();
    }
    if (!host.empty() && (host[0] == '/' || host[0] == '@')) {
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
        h->setKeyValue("socket", new QoreStringNode(host), xsink);
        rv->push(h.release(), xsink);
        return rv.release();
    }
    int port = ds->getPort() ? ds->getPort() : 5432;
    size_t start = 0;
    while (true) {
        size_t end = host.find(',', start);
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
        h->setKeyValue("host", new QoreStringNode(host.substr(start, end == std::string::npos ? std::string::npos
            : end - start)), xsink);
        h->setKeyValue("port", port, xsink);
        rv->push(h.release(), xsink);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return rv.release();
}
#endif

int probe_local_open(Datasource* ds, ExceptionSink* xsink) {
#ifdef QORE_HAS_DBI_CHECK_CONNECT_PATH
    const char* db = ds->getDBName();
    if (db && db[0] == '/' && qore_dbi_check_connect_path(ds, db, QSEC_READ, xsink)) {
        return -1;
    }
#endif
    return probe_open(ds, xsink);
}

QoreValue probe_select(Datasource* ds, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
    ProbeConnection* conn = ds->getPrivateData<ProbeConnection>();
    assert(conn);
    TempEncodingHelper str(sql, QCS_DEFAULT, xsink);
    if (*xsink) {
        return QoreValue();
    }
    if (!strcmp(str->c_str(), "checked")) {
        return conn->checked ? conn->checked->listRefSelf() : QoreValue();
    }
#ifdef QDBI_METHOD_GET_CONNECT_TARGETS
    if (!strcmp(str->c_str(), "current")) {
        const QoreListNode* checked = ds->getCheckedConnectTargets();
        return checked ? checked->listRefSelf() : QoreValue();
    }
#endif
#ifdef QORE_HAS_DBI_CHECK_CONNECT_PATH
    if (!strcmp(str->c_str(), "restricted")) {
        return qore_dbi_connect_restricted(ds);
    }
#endif
    return QoreValue();
}

QoreValue probe_exec(Datasource* ds, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
    return 0;
}

QoreValue probe_execraw(Datasource* ds, const QoreString* sql, ExceptionSink* xsink) {
    return 0;
}

int probe_commit(Datasource* ds, ExceptionSink* xsink) {
    return 0;
}

QoreValue f_opens(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    return (int64)probe_opens;
}

QoreValue f_targetCalls(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    return (int64)probe_target_calls;
}

QoreValue f_reset(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    probe_opens = 0;
    probe_target_calls = 0;
    return QoreValue();
}

//! registers a driver with the methods that both drivers have
void probe_register(const char* name, q_dbi_open_t open, bool targets, int caps) {
    qore_dbi_method_list methods;
    methods.add(QDBI_METHOD_OPEN, open);
    methods.add(QDBI_METHOD_CLOSE, probe_close);
    methods.add(QDBI_METHOD_SELECT, probe_select);
    methods.add(QDBI_METHOD_SELECT_ROWS, probe_select);
    methods.add(QDBI_METHOD_EXEC, probe_exec);
    methods.add(QDBI_METHOD_EXECRAW, probe_execraw);
    methods.add(QDBI_METHOD_COMMIT, probe_commit);
    methods.add(QDBI_METHOD_ROLLBACK, probe_commit);
#ifdef QDBI_METHOD_GET_CONNECT_TARGETS
    if (targets) {
        methods.add(QDBI_METHOD_GET_CONNECT_TARGETS, probe_get_connect_targets);
    }
#endif
    DBI.registerDriver(name, methods, caps);
}

void dbitargetprobe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    ProbeNs.addBuiltinVariant(nullptr, "opens", f_opens, QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo);
    ProbeNs.addBuiltinVariant(nullptr, "targetCalls", f_targetCalls, QCF_NO_FLAGS, QDOM_DEFAULT, bigIntTypeInfo);
    ProbeNs.addBuiltinVariant(nullptr, "reset", f_reset, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);
    probe_register("dbitargetprobe", probe_open, true, DBI_CAP_TRANSACTION_MANAGEMENT);
    probe_register("dbilocalprobe", probe_local_open, false, DBI_CAP_TRANSACTION_MANAGEMENT
#ifdef DBI_CAP_LOCAL_ONLY
        | DBI_CAP_LOCAL_ONLY
#endif
    );
    probe_register("dbinotargetprobe", probe_open, false, DBI_CAP_TRANSACTION_MANAGEMENT);
}

void dbitargetprobe_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(ProbeNs.copy());
}

void dbitargetprobe_module_delete() {
}
}

extern "C" DLLEXPORT void dbitargetprobe_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "dbitargetprobe";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module with DBI drivers that report the destinations of their connections";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = dbitargetprobe_module_init;
    mod_info.ns_init = dbitargetprobe_module_ns_init;
    mod_info.del = dbitargetprobe_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

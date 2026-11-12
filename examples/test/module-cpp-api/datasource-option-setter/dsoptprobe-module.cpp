/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file dsoptprobe-module.cpp a test binary module with a DBI driver that reports the setters of its options

    Test scaffolding for the setters of datasource options (qore#5741): a driver applies the options of a datasource
    when a connection is opened, and judges an option by the code that set it (Datasource::getOptionRestrictions(),
    Datasource::getOptionPolicyManagerRef()), not by the code that opens the connection.  This module is built into
    the build tree and never installed.

    The module registers the DBI driver \c "dsoptprobe", which connects to nothing, with the string options
    \c "classpath" and \c "other".  When a connection is opened, and when an option is set on an open connection, the
    driver records for each option what it observes about the code that set it.  The observation is returned by
    <tt>select("<option>")</tt> (\c NOTHING if the option was not applied to the connection) as a hash:
    - \c recorded: true if the setter of the option is known
    - \c sandboxed: QoreModuleCommandRestrictions::isSandboxed() of the setter
    - \c restricted: QoreModuleCommandRestrictions::isRestricted() of the setter
    - \c no_filesystem: true if the setter has \c PO_NO_FILESYSTEM
    - \c policy: true if a sandbox policy governed the setter (Datasource::getOptionPolicyManagerRef())
    - \c readable: true if the policy of the setter allows reading the path given as the value of the option (true
      if no policy governed the setter)
    - \c tid: the TID of the thread that applied the option

    A value of \c "refuse" is refused by the driver on an open connection (\c DSOPTPROBE-REFUSED), so that a test can
    check that a refused value keeps the previous setter.

    The functions of the namespace \c DsOptProbe stop a thread in the driver, so that a test can change a datasource
    at a chosen point of an operation of another thread without waiting for time to pass:
    - <tt>blockGet(string opt)</tt>: the next call of the driver that reads the value of \a opt on an open connection
      (as a copy of the datasource does) reads it and then stops until release() is called
    - <tt>blockSet(string opt)</tt>: the next call of the driver that sets \a opt on an open connection sets it (the
      connection has the new value) and then stops until release() is called, before it returns
    - <tt>waitBlocked()</tt>: returns once a thread is stopped by blockGet() or blockSet()
    - <tt>release()</tt>: lets the stopped thread continue
    - <tt>unblock()</tt>: cancels a stop set by blockGet() or blockSet() that no thread reached

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

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

namespace {
//! the options of the driver
const char* const ProbeOptions[] = {"classpath", "other"};

//! the namespace of the module, copied into each Program that loads it
QoreNamespace ProbeNs("DsOptProbe");

//! stops a thread that reads or sets an option in the driver, for a test that uses the datasource meanwhile
/** The points are named "get:<option>" and "set:<option>"
*/
class GetBlocker {
public:
    //! the longest that a thread waits for the other one: only a broken test waits that long
    static constexpr std::chrono::seconds MaxWait{120};

    //! makes the next pass of the point \a opt stop
    void block(const std::string& opt) {
        std::lock_guard<std::mutex> l(m);
        block_opt = opt;
        blocked = false;
        released = false;
    }

    //! called at the point \a opt: stops if it is the point to stop at, until release() is called
    void check(const std::string& opt) {
        std::unique_lock<std::mutex> l(m);
        if (block_opt.empty() || block_opt != opt) {
            return;
        }
        block_opt.clear();
        blocked = true;
        cond.notify_all();
        cond.wait_for(l, MaxWait, [this] { return released; });
        blocked = false;
        released = false;
    }

    //! returns 0 once a thread is stopped, -1 if none stops in time
    int waitBlocked(ExceptionSink* xsink) {
        std::unique_lock<std::mutex> l(m);
        if (!cond.wait_for(l, MaxWait, [this] { return blocked; })) {
            xsink->raiseException("DSOPTPROBE-HOOK-ERROR", "no thread read the option to stop at");
            return -1;
        }
        return 0;
    }

    //! cancels a stop that no thread reached, and releases a thread that reaches it meanwhile
    void unblock() {
        std::lock_guard<std::mutex> l(m);
        block_opt.clear();
        released = true;
        cond.notify_all();
    }

    //! lets the stopped thread continue
    void release() {
        std::lock_guard<std::mutex> l(m);
        released = true;
        cond.notify_all();
    }

private:
    std::mutex m;
    std::condition_variable cond;
    //! the option whose next read stops, or empty
    std::string block_opt;
    //! true while a thread is stopped
    bool blocked = false;
    //! true once release() was called for the stopped thread
    bool released = false;
};

GetBlocker get_blocker;

//! the connection data of the driver: the values of the options and what the driver observed about their setters
/** Used only by the thread that holds the connection
*/
class ProbeConnection {
public:
    ProbeConnection() : values(new QoreHashNode(autoTypeInfo)), observed(new QoreHashNode(autoTypeInfo)) {
    }

    ~ProbeConnection() {
        ExceptionSink xsink;
        values->deref(&xsink);
        observed->deref(&xsink);
    }

    //! records the value of an option and what the driver observes about the code that set it
    void apply(const Datasource& ds, const char* opt, const QoreValue val, ExceptionSink* xsink) {
        ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), xsink);
        QoreModuleCommandRestrictions setter;
        bool recorded = ds.getOptionRestrictions(opt, setter);
        h->setKeyValue("recorded", recorded, xsink);
        h->setKeyValue("sandboxed", setter.isSandboxed(), xsink);
        h->setKeyValue("restricted", setter.isRestricted(), xsink);
        h->setKeyValue("no_filesystem", setter.hasAny(QoreParseOptions(PO_NO_FILESYSTEM)), xsink);
        ReferenceHolder<QoreSandboxManager> policy(ds.getOptionPolicyManagerRef(opt), xsink);
        // a setter that is not known has no policy
        assert(recorded || !policy);
        h->setKeyValue("policy", static_cast<bool>(policy), xsink);
        bool readable = true;
        if (policy && val.getType() == NT_STRING) {
            ExceptionSink policy_xsink;
            readable = policy->checkFilesystemAccess(val.get<const QoreStringNode>()->c_str(), QSEC_READ,
                &policy_xsink);
            // the refusal is the observation, not an error of the driver
            policy_xsink.clear();
        }
        h->setKeyValue("readable", readable, xsink);
        h->setKeyValue("tid", q_gettid(), xsink);
        observed->setKeyValue(opt, h.release(), xsink);
        values->setKeyValue(opt, val.refSelf(), xsink);
    }

    //! returns the observation of an option, or NOTHING
    QoreValue getObservation(const char* opt) const {
        return observed->getKeyValue(opt).refSelf();
    }

    //! returns the value of an option, or NOTHING
    QoreValue getValue(const char* opt) const {
        return values->getKeyValue(opt).refSelf();
    }

    ProbeConnection(const ProbeConnection&) = delete;
    ProbeConnection& operator=(const ProbeConnection&) = delete;

private:
    QoreHashNode* values;
    QoreHashNode* observed;
};

int probe_open(Datasource* ds, ExceptionSink* xsink) {
    std::unique_ptr<ProbeConnection> conn(new ProbeConnection);
    // the options are applied when the connection is opened, as a driver that connects to a server applies them
    ConstHashIterator hi(ds->getConnectOptions());
    while (hi.next()) {
        conn->apply(*ds, hi.getKey(), hi.get(), xsink);
        if (*xsink) {
            return -1;
        }
    }
    ds->setPrivateData(conn.release());
    return 0;
}

int probe_close(Datasource* ds) {
    delete ds->getPrivateData<ProbeConnection>();
    ds->setPrivateData(nullptr);
    return 0;
}

int probe_opt_set(Datasource* ds, const char* opt, const QoreValue val, ExceptionSink* xsink) {
    ProbeConnection* conn = ds->getPrivateData<ProbeConnection>();
    assert(conn);
    if (val.getType() == NT_STRING && !strcmp(val.get<const QoreStringNode>()->c_str(), "refuse")) {
        xsink->raiseException("DSOPTPROBE-REFUSED", "the dsoptprobe driver refuses the value of option '%s'", opt);
        return -1;
    }
    conn->apply(*ds, opt, val, xsink);
    if (*xsink) {
        return -1;
    }
    // a test can stop the thread here, after the connection has the value (see GetBlocker)
    get_blocker.check(std::string("set:") + opt);
    return 0;
}

QoreValue probe_opt_get(const Datasource* ds, const char* opt) {
    ProbeConnection* conn = ds->getPrivateData<ProbeConnection>();
    QoreValue rv = conn ? conn->getValue(opt) : QoreValue();
    // a test can stop the thread here, after the value was read (see GetBlocker)
    get_blocker.check(std::string("get:") + opt);
    return rv;
}

QoreValue f_blockGet(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    HARD_QORE_VALUE_PARAM(opt, const QoreStringNode, args, 0);
    get_blocker.block(std::string("get:") + opt->c_str());
    return QoreValue();
}

QoreValue f_blockSet(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    HARD_QORE_VALUE_PARAM(opt, const QoreStringNode, args, 0);
    get_blocker.block(std::string("set:") + opt->c_str());
    return QoreValue();
}

QoreValue f_waitBlocked(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    get_blocker.waitBlocked(xsink);
    return QoreValue();
}

QoreValue f_unblock(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    get_blocker.unblock();
    return QoreValue();
}

QoreValue f_release(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    get_blocker.release();
    return QoreValue();
}

QoreValue probe_select(Datasource* ds, const QoreString* sql, const QoreListNode* args, ExceptionSink* xsink) {
    ProbeConnection* conn = ds->getPrivateData<ProbeConnection>();
    assert(conn);
    TempEncodingHelper str(sql, QCS_DEFAULT, xsink);
    if (*xsink) {
        return QoreValue();
    }
    return conn->getObservation(str->c_str());
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

int probe_rollback(Datasource* ds, ExceptionSink* xsink) {
    return 0;
}

int probe_begin_transaction(Datasource* ds, ExceptionSink* xsink) {
    return 0;
}

void dsoptprobe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    qore_dbi_method_list methods;
    methods.add(QDBI_METHOD_OPEN, probe_open);
    methods.add(QDBI_METHOD_CLOSE, probe_close);
    methods.add(QDBI_METHOD_SELECT, probe_select);
    methods.add(QDBI_METHOD_SELECT_ROWS, probe_select);
    methods.add(QDBI_METHOD_EXEC, probe_exec);
    methods.add(QDBI_METHOD_EXECRAW, probe_execraw);
    methods.add(QDBI_METHOD_COMMIT, probe_commit);
    methods.add(QDBI_METHOD_ROLLBACK, probe_rollback);
    methods.add(QDBI_METHOD_BEGIN_TRANSACTION, probe_begin_transaction);
    methods.add(QDBI_METHOD_OPT_SET, probe_opt_set);
    methods.add(QDBI_METHOD_OPT_GET, probe_opt_get);
    for (const char* opt : ProbeOptions) {
        methods.registerOption(opt, "an option whose setter the driver reports", stringTypeInfo);
    }

    // the functions that stop a thread in the driver (see GetBlocker)
    ProbeNs.addBuiltinVariant(nullptr, "blockGet", f_blockGet, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1,
        stringTypeInfo, QORE_PARAM_NO_ARG, "opt");
    ProbeNs.addBuiltinVariant(nullptr, "blockSet", f_blockSet, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo, 1,
        stringTypeInfo, QORE_PARAM_NO_ARG, "opt");
    ProbeNs.addBuiltinVariant(nullptr, "waitBlocked", f_waitBlocked, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);
    ProbeNs.addBuiltinVariant(nullptr, "release", f_release, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);
    ProbeNs.addBuiltinVariant(nullptr, "unblock", f_unblock, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);
    DBI.registerDriver("dsoptprobe", methods, DBI_CAP_TRANSACTION_MANAGEMENT);
}

void dsoptprobe_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(ProbeNs.copy());
}

void dsoptprobe_module_delete() {
}
}

extern "C" DLLEXPORT void dsoptprobe_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "dsoptprobe";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module with a DBI driver that reports the setters of its options";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = dsoptprobe_module_init;
    mod_info.ns_init = dsoptprobe_module_ns_init;
    mod_info.del = dsoptprobe_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

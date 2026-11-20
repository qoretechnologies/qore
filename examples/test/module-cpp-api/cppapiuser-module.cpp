/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file cppapiuser-module.cpp a test binary module consuming another module's C++ API

    Test scaffolding for the module C++ API mechanism (@ref module_cpp_api): this module is built
    into the build tree but is never installed.  It publishes no C++ API of its own -- which is
    also what makes it the "module exports no C++ API" negative case -- and instead consumes the
    \c cppapitest module's @ref QoreCppApiTestApi from C++, exposing the result to Qore-language
    code so that the mechanism can be tested by examples/test/module-cpp-api/module-cpp-api.qtest.

    Neither module links against the other: cppapiuser includes only QoreCppApiTestApi.h and
    resolves the struct at run time.

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
*/

#include "cppapiuser-module.h"
#include <qore/QoreSandboxManager.h>

#include <atomic>
#include <cassert>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>

static void cppapiuser_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void cppapiuser_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void cppapiuser_module_delete();
static void cppapiuser_parse_cmd(const QoreString& cmd, ExceptionSink* xsink);

extern "C" DLLEXPORT void cppapiuser_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "cppapiuser";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module consuming another module's C++ API; publishes no C++ API itself";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = cppapiuser_module_init;
    mod_info.ns_init = cppapiuser_module_ns_init;
    mod_info.del = cppapiuser_module_delete;
    mod_info.parse_cmd = cppapiuser_parse_cmd;
    // the command handler only records the commands that it receives (and raises an error for "fail"): it does
    // nothing that the issuing code could not do itself, so it honors the restrictions of every issuer, also of the
    // sandboxed Programs of aot-module-cmd-replay.qtest (see QoreModuleInfo::cmd_caller_aware)
    mod_info.cmd_caller_aware = true;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

QoreNamespace CppApiTestNs("Qore::CppApiTest");

// the resolved producer API; resolution takes the module manager's lock, so it is done once and
// cached for the life of the process -- binary modules are only unloaded at shutdown
static std::atomic<const QoreCppApiTestApi*> cached_api{nullptr};

const QoreCppApiTestApi* cppapiuser_get_api(ExceptionSink* xsink) {
    const QoreCppApiTestApi* api = cached_api.load(std::memory_order_acquire);
    if (!api) {
        api = qore_cppapitest_api(xsink);
        if (!api) {
            assert(*xsink);
            return nullptr;
        }
        cached_api.store(api, std::memory_order_release);
    }
    return api;
}

// the only class name the namespace class handler below creates on demand
static const char* CPPAPI_HANDLED_CLASS = "CppApiHandledClass";

// a static method, so that the class needs no constructor and no private data to be callable
static QoreValue cppapi_handled_marker(const QoreMethod& method, const void* ptr,
        const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    return new QoreStringNode("handled");
}

//! creates a class on demand when it cannot be found in the namespace by normal means
/** Registered on the root namespace so that the test covers the shallowest entry in the root
    namespace's depth list, which is the position a broken depth-list iterator drops.
*/
static QoreClass* cppapiuser_class_handler(QoreNamespace* ns, const char* cname) {
    if (strcmp(cname, CPPAPI_HANDLED_CLASS)) {
        return nullptr;
    }
    // the handler is registered on the root namespace, whose path is empty; a builtin class needs
    // a non-empty namespace path, so the class is created in the module's own namespace
    QoreNamespace* target = ns->findCreateNamespacePath("Qore::CppApiTest");
    std::string path = target->getPath();
    assert(!path.empty());
    // QoreClass has a protected destructor, so it cannot be held in a scope guard; the namespace
    // takes ownership unconditionally, which is the same handover the initXClass() helpers use
    QoreClass* qc = new QoreClass(cname, path.c_str());
    qc->addStaticMethod(nullptr, "marker", cppapi_handled_marker, Public, QCF_NO_FLAGS, QDOM_DEFAULT,
        stringTypeInfo);
    target->addSystemClass(qc);
    return qc;
}

static void member_read_probe_constructor(const QoreMethod& method, const void*, QoreObject* self,
        const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    self->setPrivate(method.getClass()->getID(), new AbstractPrivateData);
}

static void member_read_probe_destructor(QoreObject*, AbstractPrivateData* data, RuntimeConfig&, ExceptionSink* xsink) {
    data->deref(xsink);
}

// A read-only proxy verifies inherited builtin memberGate independently of write-gate registration.
static QoreValue member_read_probe(const QoreMethod&, const void*, QoreObject*, AbstractPrivateData*,
        const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    return QoreValue(55ll);
}

static void cppapiuser_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    init_cppapiuser_functions(CppApiTestNs);
    CppApiTestNs.addSystemClass(initProgramContextProbeClass(CppApiTestNs));
    CppApiTestNs.addSystemClass(initSerializationIndexProbeClass(CppApiTestNs));
    CppApiTestNs.addSystemClass(initExceptionResultProbeClass(CppApiTestNs));
    CppApiTestNs.addSystemClass(initPollableProbeClass(CppApiTestNs));
    QoreClass* write_probe = initMemberWriteProbeClass(CppApiTestNs);
    write_probe->setMemberWriteGate(member_write_probe);
    write_probe->addMember("declared", Public, bigIntTypeInfo, QoreValue(7ll));
    write_probe->addMember("secret", Private, bigIntTypeInfo, QoreValue(9ll));
    CppApiTestNs.addSystemClass(write_probe);
    QoreClass* accessor_probe = initMemberAccessorProbeClass(CppApiTestNs);
    init_member_accessor_probe_members(*accessor_probe);
    CppApiTestNs.addSystemClass(accessor_probe);
    // members added to the committed class during the module's initialization, as module-jni does (#6272, #6422)
    add_member_accessor_probe_init_late_members(*accessor_probe);
    QoreClass* inherited_probe = new QoreClass("MemberWriteDerivedProbe", "Qore::CppApiTest");
    inherited_probe->addBuiltinBaseClass(write_probe);
    CppApiTestNs.addSystemClass(inherited_probe);
    QoreClass* read_probe = new QoreClass("MemberReadProbe", "Qore::CppApiTest");
    read_probe->setPublicMemberFlag();
    read_probe->addConstructor(nullptr, member_read_probe_constructor);
    read_probe->setDestructor(member_read_probe_destructor);
    read_probe->addMethod(nullptr, "memberGate", member_read_probe, Public, QCF_CONSTANT, QDOM_DEFAULT,
        autoTypeInfo, {stringTypeInfo}, {}, {"member"});
    CppApiTestNs.addSystemClass(read_probe);
}

static void cppapiuser_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(CppApiTestNs.copy());
    // the namespace must already be attached to the root when the handler is registered, since
    // setClassHandler() records the namespace in the root namespace's depth list
    rns->setClassHandler(cppapiuser_class_handler);
}

static void cppapiuser_module_delete() {
}

// the class of the objects that the commands "pending-error" and "pending-const" create in the Program that the
// command runs in, as module-python creates the objects of the Python classes of the Program (qore#6752): a builtin
// class of that Program (its source Program), with a C++ destructor and a C++ method
static const char* PENDING_PROBE_NS = "CppApiPendingProbe";
static const char* PENDING_PROBE_CLASS = "PendingProbe";

// the number of times the C++ destructor of the class has run, in any Program
static std::atomic<int64> pending_probe_destroyed{0};

static void pending_probe_destructor(QoreObject*, AbstractPrivateData* data, RuntimeConfig&, ExceptionSink* xsink) {
    ++pending_probe_destroyed;
    data->deref(xsink);
}

static void pending_probe_constructor(const QoreMethod& method, const void*, QoreObject* self,
        const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    self->setPrivate(method.getClass()->getID(), new AbstractPrivateData);
}

static QoreValue pending_probe_value(const QoreMethod&, const void*, QoreObject*, AbstractPrivateData*,
        const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    return QoreValue(1ll);
}

// returns the class in the current Program, created there on the first call
static QoreClass* pending_probe_get_class() {
    QoreProgram* pgm = getProgram();
    assert(pgm);
    QoreNamespace* ns = pgm->getRootNS()->findCreateNamespacePathAll(PENDING_PROBE_NS);
    QoreClass* qc = ns->findLocalClass(PENDING_PROBE_CLASS);
    if (qc) {
        return qc;
    }
    std::string path = ns->getPath();
    // the Program is the source Program of the class, as for the classes that module-python creates
    qc = new QoreBuiltinClass(pgm, PENDING_PROBE_CLASS, path.c_str());
    qc->addConstructor(nullptr, pending_probe_constructor);
    qc->setDestructor(pending_probe_destructor);
    qc->addMethod(nullptr, "value", pending_probe_value, Public, QCF_CONSTANT, QDOM_DEFAULT, bigIntTypeInfo);
    ns->addSystemClass(qc);
    return qc;
}

// returns a new object of the class in the current Program
static QoreObject* pending_probe_new_object(ExceptionSink* xsink) {
    QoreClass* qc = pending_probe_get_class();
    // the object and its private data are released if an allocation fails
    ReferenceHolder<QoreObject> obj(new QoreObject(qc, getProgram()), xsink);
    ReferenceHolder<AbstractPrivateData> data(new AbstractPrivateData, xsink);
    obj->setPrivate(qc->getID(), data.release());
    return obj.release();
}

int64 cppapiuser_pending_probe_destroyed() {
    return pending_probe_destroyed.load();
}

// handles the commands "pending-error" (raises CPPAPIUSER-PENDING-ERROR with an object of the class as its argument)
// and "pending-const <name>" (adds a constant with an object of the class to the class's namespace); returns false
// for any other command
static bool cppapiuser_pending_probe_cmd(const QoreString& cmd, ExceptionSink* xsink) {
    if (cmd == "pending-error") {
        xsink->raiseExceptionArg("CPPAPIUSER-PENDING-ERROR", QoreValue(pending_probe_new_object(xsink)),
            "the command \"pending-error\" was issued");
        return true;
    }
    static const char* const_prefix = "pending-const ";
    if (!strncmp(cmd.c_str(), const_prefix, strlen(const_prefix))) {
        const char* name = cmd.c_str() + strlen(const_prefix);
        QoreClass* qc = pending_probe_get_class();
        QoreNamespace* ns = getProgram()->getRootNS()->findCreateNamespacePathAll(PENDING_PROBE_NS);
        // the object is held until the namespace takes it
        ReferenceHolder<QoreObject> obj(pending_probe_new_object(xsink), xsink);
        ns->addConstant(name, obj.release(), qc->getTypeInfo());
        return true;
    }
    return false;
}

// the module commands received (%module-cmd(cppapiuser) and Program::issueModuleCmd()), for the tests of how they
// reach the module (issue #5905)
static std::mutex module_cmd_lock;
static std::vector<std::string> module_cmds;

//! the number of objects of the classes that the "owned-constant" command creates whose C++ destructor has run
static std::atomic<int64> owned_probe_destroyed{0};

//! the destructor of the classes that the "owned-constant" command creates: counts the objects destroyed
static void owned_probe_destructor(QoreObject*, AbstractPrivateData* data, RuntimeConfig&, ExceptionSink* xsink) {
    ++owned_probe_destroyed;
    data->deref(xsink);
}

//! as owned_probe_destructor(), and raises an exception
static void owned_probe_raising_destructor(QoreObject* self, AbstractPrivateData* data, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    owned_probe_destructor(self, data, rc, xsink);
    xsink->raiseException("OWNED-PROBE-DESTRUCTOR", "the destructor of %s raised an exception", self->getClassName());
}

//! "owned-constant <ns>" or "owned-constant-raising <ns>": creates the class <ns>::OwnedProbe with a C++ destructor in
//! the Program the command applies to, and the constant <ns>::Obj that holds an object of it, at run time, as
//! module-python's "import" command creates the classes and constants of a Python module (#6791)
//! OwnedProbe::name(): returns the name of the class
static QoreValue owned_probe_name(const QoreMethod&, const void*, QoreObject* self, AbstractPrivateData*,
        const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    return new QoreStringNode(self->getClassName());
}

static void cppapiuser_owned_constant(const char* ns_name, bool raising, ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();
    // the namespace is added to the Program's root namespace when it is complete, as module-python adds its own
    QoreNamespace* ns = new QoreNamespace(ns_name);
    QoreClass* qc = new QoreClass("OwnedProbe", ns_name);
    qc->setDestructor(raising ? owned_probe_raising_destructor : owned_probe_destructor);
    qc->addMethod(nullptr, "name", owned_probe_name, Public, QCF_CONSTANT, QDOM_DEFAULT, stringTypeInfo);
    ns->addSystemClass(qc);
    ns->addConstant("Obj", new QoreObject(qc, pgm, new AbstractPrivateData));
    pgm->getRootNS()->addNamespace(ns);
}

int64 cppapiuser_get_owned_probe_destroyed() {
    return owned_probe_destroyed.load();
}

// records the command, with " [sandboxed]" appended when the Program that the command runs in has a sandbox manager
// (the sandbox that a module such as python gives what the command creates, see QoreSandboxManagerHelper(pgm)); the
// command "fail" raises an exception instead, and the commands of cppapiuser_pending_probe_cmd() are not recorded
static void cppapiuser_parse_cmd(const QoreString& cmd, ExceptionSink* xsink) {
    assert(xsink);
    if (cmd == "fail") {
        xsink->raiseException("CPPAPIUSER-COMMAND-ERROR", "the command \"fail\" was issued");
        return;
    }
    if (!strncmp(cmd.c_str(), "owned-constant ", 15)) {
        cppapiuser_owned_constant(cmd.c_str() + 15, false, xsink);
        return;
    }
    if (!strncmp(cmd.c_str(), "owned-constant-raising ", 23)) {
        cppapiuser_owned_constant(cmd.c_str() + 23, true, xsink);
        return;
    }
    // no C++ exception may leave the module's C callback
    try {
        if (cppapiuser_pending_probe_cmd(cmd, xsink)) {
            return;
        }
        std::string rec(cmd.c_str());
        {
            QoreSandboxManagerHelper sm(getProgram());
            if (sm) {
                rec += " [sandboxed]";
            }
        }
        std::lock_guard<std::mutex> guard(module_cmd_lock);
        module_cmds.push_back(std::move(rec));
    } catch (const std::bad_alloc&) {
        xsink->outOfMemory();
    } catch (const std::exception& e) {
        xsink->raiseException("CPPAPIUSER-COMMAND-ERROR", "the command failed: %s", e.what());
    }
}

QoreListNode* cppapiuser_get_module_cmds() {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(stringTypeInfo), nullptr);
    std::lock_guard<std::mutex> guard(module_cmd_lock);
    for (const std::string& cmd : module_cmds) {
        rv->push(new QoreStringNode(cmd), nullptr);
    }
    return rv.release();
}

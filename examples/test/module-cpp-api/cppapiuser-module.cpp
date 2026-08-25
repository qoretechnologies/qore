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

#include <cassert>
#include <cstring>

static void cppapiuser_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void cppapiuser_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void cppapiuser_module_delete();

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

static void cppapiuser_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    init_cppapiuser_functions(CppApiTestNs);
    CppApiTestNs.addSystemClass(initProgramContextProbeClass(CppApiTestNs));
}

static void cppapiuser_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(CppApiTestNs.copy());
    // the namespace must already be attached to the root when the handler is registered, since
    // setClassHandler() records the namespace in the root namespace's depth list
    rns->setClassHandler(cppapiuser_class_handler);
}

static void cppapiuser_module_delete() {
}

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

static void cppapiuser_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    init_cppapiuser_functions(CppApiTestNs);
    CppApiTestNs.addSystemClass(initProgramContextProbeClass(CppApiTestNs));
}

static void cppapiuser_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(CppApiTestNs.copy());
}

static void cppapiuser_module_delete() {
}

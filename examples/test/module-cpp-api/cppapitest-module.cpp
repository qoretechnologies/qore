/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file cppapitest-module.cpp a test binary module publishing a C++ API

    Test scaffolding for the module C++ API mechanism (@ref module_cpp_api): this module is built
    into the build tree but is never installed.  It publishes @ref QoreCppApiTestApi through the
    conventional \c cppapitest_qore_cpp_api entry point, and answers a reserved major version with
    a struct whose header does not describe it, so that the version validation in
    q_get_module_cpp_api() can be tested against a deliberately broken producer as well as a
    working one.

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

#include "QoreCppApiTestApi.h"

#include <atomic>

static void cppapitest_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void cppapitest_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void cppapitest_module_delete();

extern "C" DLLEXPORT void cppapitest_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "cppapitest";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module publishing a C++ API through the module C++ API mechanism";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = cppapitest_module_init;
    mod_info.ns_init = cppapitest_module_ns_init;
    mod_info.del = cppapitest_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

static void cppapitest_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
}

static void cppapitest_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
}

static void cppapitest_module_delete() {
}

//! the number of counters currently allocated; a consumer leaking a handle shows up here
static std::atomic<int64> counter_count{0};

//! an opaque handle: the consumer only ever sees a QoreCppApiTestCounter*
class QoreCppApiTestCounter {
public:
    DLLLOCAL QoreCppApiTestCounter(int64 init) : value(init) {
        ++counter_count;
    }

    DLLLOCAL void ref() {
        ++refs;
    }

    DLLLOCAL void deref() {
        if (!--refs) {
            delete this;
        }
    }

    DLLLOCAL int64 add(int64 v) {
        return value += v;
    }

private:
    DLLLOCAL ~QoreCppApiTestCounter() {
        --counter_count;
    }

    std::atomic<int> refs{1};
    std::atomic<int64> value;
};

static int64 cppapitest_add(int64 a, int64 b) {
    return a + b;
}

static QoreStringNode* cppapitest_reverse(const QoreString& str, ExceptionSink* xsink) {
    TempEncodingHelper utf8(str, QCS_UTF8, xsink);
    if (*xsink) {
        return nullptr;
    }
    size_t len = utf8->length();
    SimpleRefHolder<QoreStringNode> rv(new QoreStringNode(QCS_UTF8));
    for (size_t i = len; i; --i) {
        std::unique_ptr<QoreString> c(utf8->substr(i - 1, 1, xsink));
        if (*xsink) {
            return nullptr;
        }
        rv->concat(c.get(), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return rv.release();
}

static QoreCppApiTestCounter* cppapitest_counter_create(int64 init) {
    return new QoreCppApiTestCounter(init);
}

static void cppapitest_counter_ref(QoreCppApiTestCounter* c) {
    c->ref();
}

static void cppapitest_counter_deref(QoreCppApiTestCounter* c) {
    c->deref();
}

static int64 cppapitest_counter_add(QoreCppApiTestCounter* c, int64 v) {
    return c->add(v);
}

static int64 cppapitest_counter_live_count() {
    return counter_count;
}

//! the API this module actually implements
static const QoreCppApiTestApi cppapitest_cpp_api = {
    {QORE_CPPAPITEST_CPP_API_MAJOR, QORE_CPPAPITEST_CPP_API_MINOR},
    cppapitest_add,
    cppapitest_reverse,
    cppapitest_counter_create,
    cppapitest_counter_ref,
    cppapitest_counter_deref,
    cppapitest_counter_add,
    cppapitest_counter_live_count,
};

//! the same struct with a header that does not describe the version it is served for
static const QoreCppApiTestApi cppapitest_bad_header_api = {
    {QORE_CPPAPITEST_BAD_HEADER_STRUCT_MAJOR, QORE_CPPAPITEST_BAD_HEADER_STRUCT_MINOR},
    cppapitest_add,
    cppapitest_reverse,
    cppapitest_counter_create,
    cppapitest_counter_ref,
    cppapitest_counter_deref,
    cppapitest_counter_add,
    cppapitest_counter_live_count,
};

extern "C" DLLEXPORT const void* cppapitest_qore_cpp_api(unsigned major, unsigned minor) {
    // the well-behaved path: serve any minor up to the one implemented
    if (major == QORE_CPPAPITEST_CPP_API_MAJOR) {
        return minor > QORE_CPPAPITEST_CPP_API_MINOR ? nullptr : &cppapitest_cpp_api;
    }
    // the deliberately broken path: accept the request but answer with a struct whose header
    // describes a different version.  A producer must never do this; q_get_module_cpp_api()
    // catches it anyway, which is what this branch exists to prove.  The same struct is served
    // for its own declared major so that the minor check can be exercised as well.
    if (major == QORE_CPPAPITEST_BAD_HEADER_MAJOR || major == QORE_CPPAPITEST_BAD_HEADER_STRUCT_MAJOR) {
        return &cppapitest_bad_header_api;
    }
    // producer-side refusal of an unsupported major version
    return nullptr;
}

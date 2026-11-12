/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file aotv1init-module.cpp test binary modules initialized with the legacy AOT module entry point

    Test scaffolding for the release of the dependency records of a binary module whose initialization fails
    (#5953): this file is built twice, as the modules \c aotv1fail and \c aotv1ok, which are built into the build
    tree but never installed.  Each initializes itself as an AOT module compiled by an older qcc does, with the
    legacy entry point qore_aot_module_init(), which parses the module's source in a user module definition context:
    the initialization closure of the source loads the user module \c AotV1Dep and the native binary module \c json,
    and then fails (\c aotv1fail) or succeeds (\c aotv1ok).  The module is a binary module, so the user module is not
    recorded as its dependency (such a record was never released, as delUser() does not delete binary modules); the
    binary module is, so that it is deleted after the module, and the record is released if the initialization
    fails.

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

#include <qore/Qore.h>

#include <cstring>
#include <string>

#if !defined(AOTV1_ID) || !defined(AOTV1_NAME) || !defined(AOTV1_FAIL)
#error AOTV1_ID, AOTV1_NAME and AOTV1_FAIL must be defined
#endif

#define AOTV1_CAT2(a, b) a ## b
#define AOTV1_CAT(a, b) AOTV1_CAT2(a, b)
#define AOTV1_DESC_FUNC AOTV1_CAT(AOTV1_ID, _qore_module_desc)

// the legacy C ABI of AOT modules exported by libqore (see include/qore/intern/QoreAOT.h)
struct QoreAOTFunc;
extern "C" QoreStringNode* qore_aot_module_init(const char* source, int source_len, const char* label,
        int64_t parse_options, const char* mod_name, const QoreAOTFunc* functions, int num_functions);
extern "C" void qore_aot_module_ns_init_v2(QoreNamespace* root_ns, QoreNamespace* qore_ns, ExceptionSink* xsink);
extern "C" void qore_aot_module_delete();

namespace {
//! the source of the module, as an AOT module embeds it
std::string module_source() {
    std::string src = "%modern\nmodule " AOTV1_NAME " {\n"
        "    version = \"1.0\";\n"
        "    desc = \"test module initialized with the legacy AOT module entry point\";\n"
        "    author = \"Qore Technologies, s.r.o.\";\n"
        "    license = \"MIT\";\n"
        "    init = sub () {\n"
        "        load_module(\"AotV1Dep\");\n"
        "        load_module(\"json\");\n";
#if AOTV1_FAIL
    src += "        throw \"INIT-ERROR\", \"init fails\";\n";
#endif
    src += "    };\n}\n";
    return src;
}
}

static void aotv1_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    std::string src = module_source();
    QoreStringNode* err = qore_aot_module_init(src.c_str(), static_cast<int>(src.size()), AOTV1_NAME ".qm", 0,
        AOTV1_NAME, nullptr, 0);
    if (err) {
        xsink.raiseException("INIT-ERROR", err);
    }
}

static void aotv1_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qore_aot_module_ns_init_v2(rns, qns, &xsink);
}

static void aotv1_module_delete() {
    qore_aot_module_delete();
}

extern "C" DLLEXPORT void AOTV1_DESC_FUNC(QoreModuleInfo& mod_info) {
    mod_info.name = AOTV1_NAME;
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module initialized with the legacy AOT module entry point";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = aotv1_module_init;
    mod_info.ns_init = aotv1_module_ns_init;
    mod_info.del = aotv1_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

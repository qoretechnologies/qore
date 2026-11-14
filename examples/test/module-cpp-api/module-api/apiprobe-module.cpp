/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file apiprobe-module.cpp

    a test module that declares a given module API version; built twice (see ../CMakeLists.txt):
    - apicur: the module API of the headers it is built with (QORE_MODULE_API_MAJOR / QORE_MODULE_API_MINOR)
    - apiold: the preceding module API minor; Qore must refuse to load it

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

#if !defined(APIPROBE_ID) || !defined(APIPROBE_NAME) || !defined(APIPROBE_MAJOR) || !defined(APIPROBE_MINOR)
#error APIPROBE_ID, APIPROBE_NAME, APIPROBE_MAJOR and APIPROBE_MINOR must be defined
#endif

#define APIPROBE_CAT2(a, b) a ## b
#define APIPROBE_CAT(a, b) APIPROBE_CAT2(a, b)
#define APIPROBE_DESC_FUNC APIPROBE_CAT(APIPROBE_ID, _qore_module_desc)

static void apiprobe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
}

static void apiprobe_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
}

static void apiprobe_module_delete() {
}

extern "C" DLLEXPORT void APIPROBE_DESC_FUNC(QoreModuleInfo& mod_info) {
    mod_info.name = APIPROBE_NAME;
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module declaring a module API version";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = APIPROBE_MAJOR;
    mod_info.api_minor = APIPROBE_MINOR;
    mod_info.init = apiprobe_module_init;
    mod_info.ns_init = apiprobe_module_ns_init;
    mod_info.del = apiprobe_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

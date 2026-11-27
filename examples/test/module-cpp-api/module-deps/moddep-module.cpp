/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file moddep-module.cpp test binary module for the dependencies that a binary module declares (qore#7796)

    One source built twice, as the modules \c moddepbase and \c moddepuser (see the CMakeLists.txt of the directory
    above); neither is installed.  \c moddepuser declares \c moddepbase as a dependency in its module description
    (QoreModuleInfo::dependencies), which get_module_hash() reports as its \c required-modules; \c moddepbase
    declares none.  Each module adds a namespace with a constant naming the module.

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

#define MODDEP_DESC_SYMBOL2(id) id ## _qore_module_desc
#define MODDEP_DESC_SYMBOL(id) MODDEP_DESC_SYMBOL2(id)

namespace {
//! the namespace of the module
QoreNamespace* module_ns = nullptr;
}

static void moddep_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    module_ns = new QoreNamespace(MODDEP_NS);
    module_ns->addConstant("Name", new QoreStringNode(MODDEP_NAME));
}

static void moddep_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(module_ns->copy());
}

static void moddep_module_delete() {
    delete module_ns;
    module_ns = nullptr;
}

extern "C" DLLEXPORT void MODDEP_DESC_SYMBOL(MODDEP_ID)(QoreModuleInfo& mod_info) {
    mod_info.name = MODDEP_NAME;
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module for the dependencies that a binary module declares";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = moddep_module_init;
    mod_info.ns_init = moddep_module_ns_init;
    mod_info.del = moddep_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
#ifdef MODDEP_DEPENDENCY
    mod_info.dependencies.push_back(MODDEP_DEPENDENCY);
#endif
}

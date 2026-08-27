/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file staticpgm-module.cpp a test binary module that owns a Program at static scope

    Test scaffolding for process teardown: this module is built into the build tree but is never
    installed.  It does nothing except hold a QoreProgram in an object with static storage
    duration, which is what the jni module does with its global Java context Program.  Such a
    Program is destroyed from a static destructor while exit() runs, and its teardown calls back
    into libqore singletons - so this module is the minimal reproducer for a libqore singleton
    that does not outlive the Programs it serves.  See static-program-exit.qtest.

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

#include <memory>

static void staticpgm_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink);
static void staticpgm_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink);
static void staticpgm_module_delete();

extern "C" DLLEXPORT void staticpgm_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "staticpgm";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module owning a Program with static storage duration";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = staticpgm_module_init;
    mod_info.ns_init = staticpgm_module_ns_init;
    mod_info.del = staticpgm_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

//! the ExceptionSink the Program helper below reports into
/** Declared before the helper on purpose: QoreProgramHelper keeps a reference to it and
    ~QoreProgramHelper() writes into it, and objects with static storage duration are destroyed in
    reverse order of construction, so the sink has to be constructed first to outlive the helper.
*/
static ExceptionSink module_xsink;

//! the Program this module keeps alive for the life of the process
/** The unique_ptr has static storage duration, so ~QoreProgramHelper() runs from a static
    destructor during exit() - after the module manager has already been torn down.  This is
    deliberately the same shape the jni module uses for its global Java context Program.
*/
static std::unique_ptr<QoreProgramHelper> qph;

static void staticpgm_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    qph.reset(new QoreProgramHelper(PO_NEW_STYLE, module_xsink));
    (*qph)->setScriptPath("staticpgm module global Program context");
}

static void staticpgm_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
}

static void staticpgm_module_delete() {
    // the Program is deliberately not released here: the point of this module is that the
    // Program is torn down from a static destructor while exit() runs
}

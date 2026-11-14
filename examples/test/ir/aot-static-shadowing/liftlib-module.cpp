/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    liftlib-module.cpp

    Copyright (C) 2026 Qore Technologies, s.r.o.

    A library module for AOTStaticModuleShadowing.qtest: installed in the module path of the host, so it is library
    code; its initialization calls the host's hook (aot_static_shadowing_lift_hook(), found in the host's executable),
    which registers or loads a module that is not library code (LiftMod) while the library module is
    initialized.

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

#include <dlfcn.h>

namespace {
//! the signature of the host's hook
typedef void (*lift_hook_t)(ExceptionSink* xsink);

void liftlib_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    lift_hook_t hook = reinterpret_cast<lift_hook_t>(dlsym(RTLD_DEFAULT, "aot_static_shadowing_lift_hook"));
    if (!hook) {
        xsink.raiseException("LIFTLIB-ERROR", "the host has no aot_static_shadowing_lift_hook()");
        return;
    }
    hook(&xsink);
}

void liftlib_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
}

void liftlib_module_delete() {
}
}

extern "C" DLLEXPORT void LiftLib_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "LiftLib";
    mod_info.version = "1.0";
    mod_info.desc = "library module whose initialization registers a module that is not library code";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = liftlib_module_init;
    mod_info.ns_init = liftlib_module_ns_init;
    mod_info.del = liftlib_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

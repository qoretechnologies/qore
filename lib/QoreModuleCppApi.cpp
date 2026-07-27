/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreModuleCppApi.cpp

    versioned C++ API publication and resolution between binary modules

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include "qore/Qore.h"
#include "qore/QoreModuleCppApi.h"
#include "qore/intern/ModuleInfo.h"

//! returns the C++ API entry point symbol name for the given feature
static void get_cpp_api_symbol(QoreString& sym, const char* feature) {
    sym.sprintf("%s%s", feature, QORE_MODULE_CPP_API_SUFFIX);
    // the symbol must be a valid C identifier; module feature names may contain hyphens
    sym.replaceAll("-", "_");
}

const void* q_get_module_cpp_api(const char* feature, unsigned major, unsigned minor, ExceptionSink* xsink) {
    assert(xsink);

    if (!feature || !*feature) {
        xsink->raiseException("MODULE-CPP-API-ERROR", "missing or empty module feature name in a call to "
            "q_get_module_cpp_api()");
        return nullptr;
    }

    // load the module if it is not already loaded; no QoreProgram is passed, so the module's
    // namespace is not imported anywhere -- a C++ API consumer needs the module's code, not its
    // Qore-language types
    QoreAbstractModule* mi = QMM.findModule(feature);
    if (!mi) {
        if (ModuleManager::runTimeLoadModule(xsink, feature)) {
            assert(*xsink);
            return nullptr;
        }
        mi = QMM.findModule(feature);
        if (!mi) {
            // cannot happen: runTimeLoadModule() returned success
            xsink->raiseException("MODULE-CPP-API-ERROR", "module '%s' reported a successful load but is not "
                "registered with the module manager", feature);
            return nullptr;
        }
    }

    if (!mi->isBuiltin()) {
        xsink->raiseException("MODULE-CPP-API-ERROR", "module '%s' (%s) is a Qore-language module and therefore "
            "cannot export a C++ API", feature, mi->getFileName());
        return nullptr;
    }

    QoreString sym;
    get_cpp_api_symbol(sym, feature);

    // a statically-registered AOT module has no dlopen() handle; binary modules are opened
    // RTLD_GLOBAL, so the process-global scope resolves those
    void* dlptr = const_cast<void*>(static_cast<QoreBuiltinModule*>(mi)->getPtr());
    qore_module_cpp_api_t entry = reinterpret_cast<qore_module_cpp_api_t>(dlsym(dlptr ? dlptr : RTLD_DEFAULT,
        sym.c_str()));
    if (!entry) {
        xsink->raiseException("MODULE-CPP-API-ERROR", "module '%s' (%s) exports no C++ API: entry point '%s' is "
            "not present", feature, mi->getFileName(), sym.c_str());
        return nullptr;
    }

    const void* api = entry(major, minor);
    if (!api) {
        xsink->raiseException("MODULE-CPP-API-VERSION-ERROR", "module '%s' (version %s) cannot serve C++ API "
            "version %u.%u requested by the caller", feature, mi->getVersion(), major, minor);
        return nullptr;
    }

    // every module C++ API struct begins with a QoreModuleCppApiHeader, so the version the module
    // publishes can be validated here even when the module's entry point does not check it
    const QoreModuleCppApiHeader* hdr = reinterpret_cast<const QoreModuleCppApiHeader*>(api);
    if (hdr->major != major) {
        xsink->raiseException("MODULE-CPP-API-VERSION-ERROR", "module '%s' (version %s) publishes C++ API major "
            "version %u; the caller was compiled against major version %u, which is incompatible", feature,
            mi->getVersion(), hdr->major, major);
        return nullptr;
    }
    if (hdr->minor < minor) {
        xsink->raiseException("MODULE-CPP-API-VERSION-ERROR", "module '%s' (version %s) publishes C++ API version "
            "%u.%u; the caller requires version %u.%u or higher; upgrade the '%s' module", feature,
            mi->getVersion(), hdr->major, hdr->minor, major, minor, feature);
        return nullptr;
    }

    return api;
}

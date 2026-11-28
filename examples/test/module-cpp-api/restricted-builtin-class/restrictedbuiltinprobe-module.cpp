/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file restrictedbuiltinprobe-module.cpp a test binary module that creates builtin classes for the calling Program

    Test scaffolding for the object provenance predicates of SandboxManager for builtin classes (qore#7677): a module
    can create a builtin class for a Program with QoreBuiltinClass(QoreProgram*, ...), which keeps that Program as its
    source Program, as language bridge modules do.  This module is built into the build tree and never installed.

    <tt>RestrictedBuiltinProbe::make()</tt> creates such a class for the calling Program and returns an object of it
    that has no Program of its own (a system object), so that only the source Program of its class tells whose class
    it is.  Its methods \c touch() (in the \c UNCONTROLLED_API domain), \c fsTouch() (\c FILESYSTEM) and
    \c mixedTouch() (both) return @ref True: the uncontrolled API of an object of the class that a Program gives to
    code restricted from it is a capability of that Program, while its other domains are checked against that code
    (qore#7543).  The class <tt>RestrictedBuiltinProbe::StaticProbe</tt>, which no Program owns, has the same
    \c touch() method.

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

namespace {
//! the namespace of the module, copied into each Program that loads it
QoreNamespace ProbeNs("RestrictedBuiltinProbe");

//! the private data of the objects of the class, which its methods need
class ProbeData : public AbstractPrivateData {
};

//! the method of the class, in the UNCONTROLLED_API domain as the methods of the classes of language bridges are
QoreValue ProbeClass_touch(const QoreMethod& meth, const void* ptr, QoreObject* self, AbstractPrivateData* pd,
        const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    return true;
}

//! a method of the class in the FILESYSTEM domain, which it does not use
QoreValue ProbeClass_fsTouch(const QoreMethod& meth, const void* ptr, QoreObject* self, AbstractPrivateData* pd,
        const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    return true;
}

//! a method of the class in both the UNCONTROLLED_API and the FILESYSTEM domains
QoreValue ProbeClass_mixedTouch(const QoreMethod& meth, const void* ptr, QoreObject* self, AbstractPrivateData* pd,
        const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    return true;
}

//! the class of the module that no Program owns (a builtin class of a binary module)
QoreClass* StaticProbeClass = nullptr;

//! the constructor of the class that no Program owns
void StaticProbe_constructor(const QoreMethod& meth, const void* ptr, QoreObject* self, const QoreListNode* args,
        RuntimeConfig& rc, ExceptionSink* xsink) {
    self->setPrivate(StaticProbeClass->getID(), new ProbeData);
}

//! returns an object of a builtin class created for the calling Program
QoreValue f_make(const void* ptr, const QoreListNode* args, RuntimeConfig& rc, ExceptionSink* xsink) {
    // the class keeps the calling Program as its source Program; the object holds the class
    QoreClassHolder cls(new QoreBuiltinClass(getProgram(), "ProbeClass", "RestrictedBuiltinProbe::ProbeClass"));
    QoreClass* c = cls;
    c->addMethod(nullptr, "touch", (q_external_method_t)ProbeClass_touch, Public, QCF_NO_FLAGS,
        QDOM_UNCONTROLLED_API, boolTypeInfo);
    c->addMethod(nullptr, "fsTouch", (q_external_method_t)ProbeClass_fsTouch, Public, QCF_NO_FLAGS, QDOM_FILESYSTEM,
        boolTypeInfo);
    c->addMethod(nullptr, "mixedTouch", (q_external_method_t)ProbeClass_mixedTouch, Public, QCF_NO_FLAGS,
        QDOM_UNCONTROLLED_API | QDOM_FILESYSTEM, boolTypeInfo);
    c->initializeBuiltin();
    return new QoreObject(cls, nullptr, new ProbeData);
}

void restrictedbuiltinprobe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    ProbeNs.addBuiltinVariant(nullptr, "make", f_make, QCF_NO_FLAGS, QDOM_DEFAULT, objectTypeInfo);
    // a builtin class of the module, which no Program owns, with a method in the UNCONTROLLED_API domain
    StaticProbeClass = new QoreClass("StaticProbe", "RestrictedBuiltinProbe::StaticProbe");
    StaticProbeClass->addConstructor(nullptr, (q_external_constructor_t)StaticProbe_constructor);
    StaticProbeClass->addMethod(nullptr, "touch", (q_external_method_t)ProbeClass_touch, Public, QCF_NO_FLAGS,
        QDOM_UNCONTROLLED_API, boolTypeInfo);
    ProbeNs.addSystemClass(StaticProbeClass);
}

void restrictedbuiltinprobe_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(ProbeNs.copy());
}

void restrictedbuiltinprobe_module_delete() {
}
}

extern "C" DLLEXPORT void restrictedbuiltinprobe_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "restrictedbuiltinprobe";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module that creates builtin classes for the calling Program";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = restrictedbuiltinprobe_module_init;
    mod_info.ns_init = restrictedbuiltinprobe_module_ns_init;
    mod_info.del = restrictedbuiltinprobe_module_delete;
    mod_info.license = QL_MIT;
    mod_info.license_str = "MIT";
}

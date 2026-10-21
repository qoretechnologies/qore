/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file AbstractReflectionObject.cpp reflection base class definition */
/*
    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include "AbstractReflectionObject.h"
#include "QC_Class.h"

int get_access(ClassAccess access) {
    switch (access) {
        case Public: return MC_PUBLIC;
        case Private: return MC_PRIVATE;
        case Internal: return MC_PRIVATEINTERNAL;
        case Inaccessible: return 0;
        default: break;
    }
    assert(false);
    return 0;
}

AbstractReflectionObject::AbstractReflectionObject(QoreProgram* pgm) : pgm(pgm) {
    if (pgm) {
        pgm->depRef();
    }
}

AbstractReflectionObject::~AbstractReflectionObject() {
    if (pgm) {
        pgm->depDeref();
    }
}


int reflection_check_program_access(const QoreReflectionCaller& caller, const QoreProgram* pgm, const char* what,
        ExceptionSink* xsink) {
    return caller.checkProgramAccess(pgm, what, xsink);
}

int reflection_check_class_call(const QoreReflectionCaller& caller, const QoreClass* cls, const QoreProgram* target,
        bool create, ExceptionSink* xsink) {
    bool caller_refused = false;
    if (caller.allowsDomain(cls->getDomain(), target, &caller_refused)) {
        return 0;
    }
    // the message of create_object() when the calling code's restrictions refuse the class
    xsink->raiseException(create ? "CREATE-OBJECT-ERROR" : "INVALID-FUNCTION-ACCESS", "%s sandboxing restrictions "
        "do not allow access to the '%s' class", caller_refused ? "current Program" : "the class's Program's",
        cls->getName());
    return -1;
}

int reflection_check_class_value_access(const QoreReflectionCaller& caller, const QoreClass* cls,
        const QoreProgram* target, ExceptionSink* xsink) {
    bool caller_refused = false;
    if (caller.allowsDomain(cls->getDomain(), target, &caller_refused)) {
        return 0;
    }
    xsink->raiseException("REFLECTION-ACCESS-ERROR", "%s sandboxing restrictions do not allow access to the members "
        "and constants of the '%s' class", caller_refused ? "current Program" : "the class's Program's",
        cls->getName());
    return -1;
}

int reflection_check_variant_call(const QoreReflectionCaller& caller, const QoreExternalVariant* variant,
        const char* cname, const char* fname, const QoreProgram* target, ExceptionSink* xsink) {
    bool caller_refused = false;
    if (caller.allowsDomain(variant->getDomain(), target, &caller_refused)) {
        return 0;
    }
    // the message of a call resolved at run time when the calling code's restrictions refuse the variant
    xsink->raiseException("INVALID-FUNCTION-ACCESS", "%s do not allow access to %s '%s%s%s(%s)'",
        caller_refused ? "parse options" : "the parse options of the Program it was found in",
        cname ? "method" : "function", cname ? cname : "", cname ? "::" : "", fname, variant->getSignatureText());
    return -1;
}

const QoreNamespace* reflection_program_namespace(QoreProgram* pgm, const QoreNamespace* ns, ExceptionSink* xsink) {
    if (!ns) {
        xsink->raiseException("UNKNOWN-NAMESPACE", "the symbol has no namespace");
        return nullptr;
    }
    const QoreNamespace* rv = qore_program_find_namespace(*pgm, *ns);
    if (!rv) {
        xsink->raiseException("UNKNOWN-NAMESPACE", "namespace '%s' is not visible in the Program of the reflection "
            "object", ns->getPath(true).c_str());
    }
    return rv;
}

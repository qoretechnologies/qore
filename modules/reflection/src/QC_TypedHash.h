/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_TypedHash.h TypedHash class definition */
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

#ifndef _QORE_INTERN_QC_TYPEDHASH_H

#define _QORE_INTERN_QC_TYPEDHASH_H

#include "AbstractReflectionObject.h"
#include "qore/TypedHashDecl.h"

class QoreReflectionTypedHash : public AbstractReflectionObject {
public:
    const TypedHashDecl* th = nullptr;
    const QoreNamespace* ns = nullptr;

    DLLLOCAL QoreReflectionTypedHash(ExceptionSink* xsink, const char* path, QoreProgram* pgm = getProgram());

    DLLLOCAL QoreReflectionTypedHash(QoreProgram* pgm, const TypedHashDecl* th, const QoreNamespace* ns) : AbstractReflectionObject(pgm), th(th), ns(ns) {
        th->ref();
    }

    DLLLOCAL QoreReflectionTypedHash(QoreProgram* pgm, const TypedHashDecl* th) : AbstractReflectionObject(pgm), th(th), ns(th->getNamespace()) {
        th->ref();
    }

    //! releases the reference to the hashdecl: the handle keeps it, as its values do (qore#5781)
    DLLLOCAL ~QoreReflectionTypedHash() {
        if (th) {
            th->deref();
        }
    }
};

DLLEXPORT extern qore_classid_t CID_TYPEDHASH;
DLLLOCAL extern QoreClass* QC_TYPEDHASH;

DLLLOCAL void preinitTypedHashClass();
DLLLOCAL QoreClass* initTypedHashClass(QoreNamespace& ns);

#endif
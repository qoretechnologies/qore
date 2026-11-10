/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_AbstractReflectionFunction.h AbstractReflectionFunction class definition */
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

#ifndef _QORE_INTERN_QC_ABSTRACTREFLECTIONFUNCTION_H

#define _QORE_INTERN_QC_ABSTRACTREFLECTIONFUNCTION_H

#include "AbstractReflectionObject.h"

class QoreReflectionFunction : public AbstractReflectionObject {
public:
    const QoreExternalFunction* f = nullptr;
    //! the namespace that the function was found in, or nullptr if not known
    /** A call through the handle checks it for the export boundary of a user module (see
        QoreExternalFunction::evalFunction()), as a call by an importer of the module would.
    */
    const QoreNamespace* ns = nullptr;

    DLLLOCAL QoreReflectionFunction(QoreProgram* pgm) : AbstractReflectionObject(pgm) {
    }

    DLLLOCAL QoreReflectionFunction(QoreProgram* pgm, const QoreExternalFunction* f, const QoreNamespace* ns = nullptr)
            : AbstractReflectionObject(pgm), f(f), ns(ns) {
    }

    DLLLOCAL QoreObject* getFunctionObject(ExceptionSink* xsink) const;
};

//! finds a variant; for a function (\a mtype MT_None), \a caller limits the search to the variants it can see
DLLLOCAL QoreObject* find_function_variant(QoreProgram* pgm, const QoreExternalFunction* func, const QoreListNode* args, size_t offset, method_type_e mtype, ExceptionSink* xsink, const QoreReflectionCaller* caller = nullptr, const QoreNamespace* ns = nullptr);

//! \a ns: the namespace of a function (\a mtype MT_None), recorded in the handle
DLLLOCAL QoreObject* get_variant_object(QoreProgram* pgm, const QoreExternalFunction* f, method_type_e mtype, const QoreExternalVariant* v, ExceptionSink* xsink, const QoreNamespace* ns = nullptr);

//! appends the variants; for a function (\a mtype MT_None), only those that \a caller can see
DLLLOCAL void append_variant_objects(QoreListNode& l, QoreProgram* pgm, const QoreExternalFunction* f, method_type_e mtype, ExceptionSink* xsink, const QoreReflectionCaller* caller = nullptr, const QoreNamespace* ns = nullptr);

DLLEXPORT extern qore_classid_t CID_ABSTRACTREFLECTIONFUNCTION;
DLLLOCAL extern QoreClass* QC_ABSTRACTREFLECTIONFUNCTION;

DLLLOCAL void preinitAbstractReflectionFunctionClass();
DLLLOCAL QoreClass* initAbstractReflectionFunctionClass(QoreNamespace& ns);

#endif
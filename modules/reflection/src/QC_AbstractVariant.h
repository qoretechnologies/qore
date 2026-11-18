/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_AbstractVariant.h AbstractVariant class definition */
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

#ifndef _QORE_INTERN_QC_ABSTRACTVARIANT_H

#define _QORE_INTERN_QC_ABSTRACTVARIANT_H

#include "AbstractReflectionObject.h"
#include "QC_AbstractReflectionFunction.h"

// forward references
class AbstractQoreFunctionVariant;

class QoreReflectionVariant : public QoreReflectionFunction {
public:
    const QoreExternalVariant* variant;

    //! \a ns: the namespace of the function, if known (see QoreReflectionFunction::ns)
    DLLLOCAL QoreReflectionVariant(QoreProgram* pgm, const QoreExternalFunction* func, const QoreExternalVariant* variant,
            const QoreNamespace* ns = nullptr) : QoreReflectionFunction(pgm, func, ns), variant(variant) {
    }
};

DLLEXPORT extern qore_classid_t CID_ABSTRACTVARIANT;
DLLLOCAL extern QoreClass* QC_ABSTRACTVARIANT;

DLLLOCAL void preinitAbstractVariantClass();
DLLLOCAL QoreClass* initAbstractVariantClass(QoreNamespace& ns);

DLLLOCAL QoreListNode* get_code_flag_list(int64 c);

#endif

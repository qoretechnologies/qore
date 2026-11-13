/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_Constant.h Constant class definition */
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

#ifndef _QORE_INTERN_QC_CONSTANT_H

#define _QORE_INTERN_QC_CONSTANT_H

#include "QC_AbstractConstant.h"

class QoreReflectionNamespaceConstant : public QoreReflectionConstant {
public:
    const QoreNamespace* ns;

    DLLLOCAL QoreReflectionNamespaceConstant(const char* name, ExceptionSink* xsink);

    DLLLOCAL QoreReflectionNamespaceConstant(QoreProgram* pgm, const char* name, ExceptionSink* xsink);

    DLLLOCAL QoreReflectionNamespaceConstant(QoreProgram* pgm, const QoreExternalConstant* ce, const QoreNamespace* ns) :
        QoreReflectionConstant(pgm, ce), ns(ns) {
    }

    DLLLOCAL virtual const QoreNamespace* getConstantNamespace() const override {
        return ns;
    }

    DLLLOCAL virtual const QoreClass* getClass() const override {
        return nullptr;
    }
};

DLLEXPORT extern qore_classid_t CID_CONSTANT;
DLLLOCAL extern QoreClass* QC_CONSTANT;

DLLLOCAL void preinitConstantClass();
DLLLOCAL QoreClass* initConstantClass(QoreNamespace& ns);

#endif

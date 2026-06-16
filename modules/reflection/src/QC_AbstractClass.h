/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_AbstractClass.h AbstractClass class definition */
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

#ifndef _QORE_INTERN_QC_ABSTRACTCLASS_H

#define _QORE_INTERN_QC_ABSTRACTCLASS_H

#include "AbstractReflectionObject.h"
#include "QC_AbstractMethod.h"

class QoreReflectionClass : public AbstractReflectionObject {
public:
    const QoreClass* cls;

    DLLLOCAL QoreReflectionClass(ExceptionSink* xsink, const char* name, QoreProgram* pgm = getProgram());

    DLLLOCAL QoreReflectionClass(QoreProgram* pgm, const QoreClass* cls);
};

#define FV_NORMAL (1 << 0)
#define FV_STATIC (1 << 1)
#define FV_CONSTRUCTOR (1 << 2)
#define FV_DESTRUCTOR (1 << 3)
#define FV_COPY (1 << 4)
#define FV_PSEUDO (1 << 5)
#define FV_ALL (FV_NORMAL | FV_STATIC | FV_CONSTRUCTOR | FV_DESTRUCTOR | FV_COPY | FV_PSEUDO)

QoreObject* find_variant(const QoreReflectionClass* c, const char* name, unsigned which, const QoreListNode* args, size_t offset, ExceptionSink* xsink);
QoreObject* find_variant(const QoreReflectionClass* c, const QoreStringNode* name, unsigned which, const QoreListNode* args, size_t offset, ExceptionSink* xsink);

DLLLOCAL QoreObject* get_class_object(QoreProgram* pgm, const QoreClass* cls);
DLLLOCAL QoreObject* get_method_object(ReferenceHolder<QoreReflectionMethod>& m, ExceptionSink* xsink);

DLLEXPORT extern qore_classid_t CID_ABSTRACTCLASS;
DLLLOCAL extern QoreClass* QC_ABSTRACTCLASS;

DLLLOCAL void preinitAbstractClassClass();
DLLLOCAL QoreClass* initAbstractClassClass(QoreNamespace& ns);

DLLLOCAL TypedHashDecl* init_hashdecl_ClassAccessInfo(QoreNamespace& ns);
DLLLOCAL TypedHashDecl* init_hashdecl_MethodAccessInfo(QoreNamespace& ns);

#endif

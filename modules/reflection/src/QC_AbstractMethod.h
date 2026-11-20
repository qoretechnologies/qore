/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_AbstractMethod.h AbstractMethod class definition */
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

#ifndef _QORE_INTERN_QC_ABSTRACTMETHOD_H

#define _QORE_INTERN_QC_ABSTRACTMETHOD_H

#include "AbstractReflectionObject.h"
#include "QC_AbstractReflectionFunction.h"

class QoreReflectionMethod : public QoreReflectionFunction {
public:
    const QoreMethod* m = nullptr;
    method_type_e mtype = MT_None;

    DLLLOCAL QoreReflectionMethod(const char* cls_path, const char* name, ExceptionSink* xsink, method_type_e type = MT_None);

    DLLLOCAL QoreReflectionMethod(const QoreClass* cls, const char* name, ExceptionSink* xsink, method_type_e type = MT_None);

    DLLLOCAL QoreReflectionMethod(QoreProgram* pgm, const QoreMethod* m);

    DLLLOCAL const char* getType() const {
        switch (mtype) {
            case MT_Normal: return "normal";
            case MT_Static: return "static";
            case MT_Constructor: return "constructor";
            case MT_Destructor: return "destructor";
            case MT_Copy: return "copy";
            case MT_Pseudo: return "pseudo";
            default: break;
        }
        assert(false);
        return nullptr;
    }

    DLLLOCAL static method_type_e getMethodType(const QoreMethod* m);

protected:
    //! constructor for child classes
    DLLLOCAL QoreReflectionMethod() : QoreReflectionFunction(getProgram()) {
    }

    //! also sets the method type if set successfully
    DLLLOCAL void setMethod(const QoreClass* cls, const char* name, ExceptionSink* xsink, method_type_e type);

    //! sets the type or throws an exception if there is no method
    DLLLOCAL void setType(const QoreClass* cls, const char* name, method_type_e type, ExceptionSink* xsink);

    //! set the method type
    DLLLOCAL void setType();
};

class QoreReflectionHierarchyMethod : public QoreReflectionMethod {
public:
    DLLLOCAL QoreReflectionHierarchyMethod(const QoreClass* cls, const char* name, ExceptionSink* xsink, method_type_e type = MT_None);

    DLLLOCAL ClassAccess getClassAccess() const {
        return class_access;
    }

protected:
    //! access modifier for the class declaring the method
    ClassAccess class_access = Public;

    //! also sets the method type if set successfully
    DLLLOCAL void setMethod(const QoreClass* cls, const char* name, ExceptionSink* xsink, method_type_e type);
};

DLLEXPORT extern qore_classid_t CID_ABSTRACTMETHOD;
DLLLOCAL extern QoreClass* QC_ABSTRACTMETHOD;

DLLLOCAL void preinitAbstractMethodClass();
DLLLOCAL QoreClass* initAbstractMethodClass(QoreNamespace& ns);

DLLLOCAL int check_call(const QoreReflectionCaller& caller, QoreObject* obj, const QoreMethod& m,
        ExceptionSink* xsink);

#endif

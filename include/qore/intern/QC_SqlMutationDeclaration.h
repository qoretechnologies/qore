/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SqlMutationDeclaration.h

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

#ifndef _QORE_QC_SQLMUTATIONDECLARATION_H

#define _QORE_QC_SQLMUTATIONDECLARATION_H

#ifdef _QORE_LIB_INTERN

//! private data for the Qore::SQL::SqlMutationDeclaration class
/** Holds a strong reference to the datasource or pool object the declaration was pushed on, so that
    the declaration is removed deterministically when the object goes out of scope.

    The declaration is pushed and popped through the object's own Qore-level API rather than through
    a C++ downcast, so that user subclasses of AbstractDatasource work with this class as well.
*/
class QoreSqlMutationDeclaration : public AbstractPrivateData {
public:
    DLLLOCAL QoreSqlMutationDeclaration(QoreObject* ds) : ds(ds) {
        ds->ref();
    }

    //! removes the declaration from the datasource and releases the reference to it
    DLLLOCAL void doDestructor(ExceptionSink* xsink) {
        if (!ds) {
            return;
        }
        QoreObject* o = ds;
        ds = nullptr;
        o->evalMethod("popMutationDeclaration", nullptr, xsink).discard(xsink);
        o->deref(xsink);
    }

protected:
    DLLLOCAL virtual ~QoreSqlMutationDeclaration() {
        assert(!ds);
    }

private:
    QoreObject* ds;
};

DLLEXPORT extern qore_classid_t CID_SQLMUTATIONDECLARATION;
DLLLOCAL extern QoreClass* QC_SQLMUTATIONDECLARATION;
DLLLOCAL QoreClass* initSqlMutationDeclarationClass(QoreNamespace& ns);

#endif // _QORE_LIB_INTERN
#endif // _QORE_QC_SQLMUTATIONDECLARATION_H

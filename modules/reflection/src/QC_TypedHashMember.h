/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file QC_TypedHashMember.h TypedHashMember class definition */
/*
    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software wiathout restriction, including without limitation
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

#ifndef _QORE_INTERN_QC_TYPEDHASHMEMBER_H

#define _QORE_INTERN_QC_TYPEDHASHMEMBER_H

#include "QC_AbstractMember.h"

#include <string>

// forward references
class HashDeclMemberInfo;

class QoreReflectionHashDeclMember : public QoreReflectionMember {
public:
    const TypedHashDecl* th;
    std::string name;

    DLLLOCAL QoreReflectionHashDeclMember(QoreProgram* pgm, const TypedHashDecl* th, const char* name, const QoreExternalMemberBase* mem) :
        QoreReflectionMember(pgm, name, mem), th(th), name(name) {
        // the member is the hashdecl's: the handle keeps the hashdecl (qore#5781)
        th->ref();
    }

    DLLLOCAL ~QoreReflectionHashDeclMember() {
        th->deref();
    }
};

DLLEXPORT extern qore_classid_t CID_TYPEDHASHMEMBER;
DLLLOCAL extern QoreClass* QC_TYPEDHASHMEMBER;

DLLLOCAL void preinitTypedHashMemberClass();
DLLLOCAL QoreClass* initTypedHashMemberClass(QoreNamespace& ns);

#endif

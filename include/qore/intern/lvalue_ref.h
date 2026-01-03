/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    lvalue_ref.h

    POSIX thread library for Qore

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

#ifndef _QORE_INTERN_LVALUE_REF_H
#define _QORE_INTERN_LVALUE_REF_H

class RSetHelper;

class lvalue_ref {
public:
    QoreValue vexp{};
    const QoreTypeInfo* typeInfo;
    QoreObject* self;
    QoreProgram* pgm;
    const void* lvalue_id;
    const qore_class_private* cls;

    DLLLOCAL lvalue_ref(QoreValue lvexp, const QoreTypeInfo* typeInfo, QoreObject* self, const void* lvid, const qore_class_private* cls);

    DLLLOCAL lvalue_ref(const lvalue_ref& old);

    DLLLOCAL ~lvalue_ref() {
        //printd(5, "lvalue_ref::~lvalue_ref() this: %p vexp: %p self: %p pgm: %p\n", this, vexp, self, pgm);
        if (self)
            self->tDeref();
        vexp.discard(nullptr);
    }

    DLLLOCAL void del(ExceptionSink* xsink) {
        //printd(5, "lvalue_ref::del() this: %p vexp: %p self: %p pgm: %p\n", this, vexp, self, pgm);
        if (vexp) {
            vexp.discard(nullptr);
            vexp = QoreValue();
        }
    }

    DLLLOCAL const QoreTypeInfo* getLValueTypeInfo() const {
        return QoreTypeInfo::getUniqueReturnComplexReference(typeInfo);
    }

    // returns true if a lock error has occurred and the transaction should be aborted or restarted; the rsection lock is held when this function is called
    DLLLOCAL bool scanReference(RSetHelper& rsh);

    // returns true if the object needs to be scanned for recursive references (ie could contain an object or closure or a container containing one of those)
    DLLLOCAL bool needsScan();

    DLLLOCAL static lvalue_ref* get(const ReferenceNode* r) {
        return r->priv;
    }

    DLLLOCAL static bool scanNode(RSetHelper& rsh, QoreValue vexp);
};

#endif

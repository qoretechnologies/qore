/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHashIterator.h

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

#ifndef _QORE_QOREOBJECTITERATOR_H

#define _QORE_QOREOBJECTITERATOR_H

#include "qore/intern/QoreHashIterator.h"
#include "qore/intern/QoreObjectIntern.h"

extern QoreClass* QC_OBJECTITERATOR;
extern QoreClass* QC_OBJECTKEYITERATOR;
extern QoreClass* QC_OBJECTPAIRITERATOR;
extern QoreClass* QC_OBJECTREVERSEITERATOR;

// the c++ object
class QoreObjectIterator : public QoreHashIterator {
public:
    DLLLOCAL QoreObjectIterator(const QoreObject* o) : QoreHashIterator(qore_object_private::get(*o)->getRuntimeMemberHash(0)) {
    }

    DLLLOCAL QoreObjectIterator() {
    }

    DLLLOCAL QoreObjectIterator(const QoreObjectIterator& old) : QoreHashIterator(old) {
    }

    DLLLOCAL virtual const char* getName() const { return "ObjectIterator"; }
};

// internal reverse iterator class implementation only for the getName() function - the iterators remain
// forwards and are used in the reverse sense by the Qore language class implementation below
class QoreObjectReverseIterator : public QoreObjectIterator {
public:
    DLLLOCAL QoreObjectReverseIterator(const QoreObject* o) : QoreObjectIterator(o) {
    }

    DLLLOCAL QoreObjectReverseIterator() {
    }

    DLLLOCAL QoreObjectReverseIterator(const QoreObjectReverseIterator& old) : QoreObjectIterator(old) {
    }

    DLLLOCAL virtual const char* getName() const {
        return "ObjectReverseIterator";
    }

    // Reverse fast path: nativeNext routes to prev() (the priv's backwards
    // walker).  Without this override the inherited QoreHashIterator fast
    // path would call next() and iterate forward.
    DLLLOCAL bool nativeNext(ExceptionSink* xsink) override {
        if (check(xsink)) {
            return false;
        }
        return prev();
    }
};

#endif // _QORE_QOREOBJECTITERATOR_H

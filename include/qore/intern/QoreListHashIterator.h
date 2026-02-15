/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreListHashIterator.h

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

#ifndef _QORE_QORELISTHASHITERATOR_H

#define _QORE_QORELISTHASHITERATOR_H

#include "qore/intern/QoreListIterator.h"

#include <cassert>

extern QoreClass* QC_LISTHASHITERATOR;

// the c++ object
class QoreListHashIterator : public QoreListIterator {
public:
    DLLLOCAL QoreListHashIterator(const QoreListNode* n_l) : QoreListIterator(n_l) {
        myHashTypeInfo = n_l->getValueTypeInfo();
        if (!QoreTypeInfo::hasType(myHashTypeInfo)) {
            myHashTypeInfo = autoHashTypeInfo;
        }
    }

    DLLLOCAL QoreListHashIterator(const QoreListHashIterator& old) : QoreListIterator(old), myHashTypeInfo(old.myHashTypeInfo) {
    }

    DLLLOCAL QoreValue getReferencedKeyValue(const char* key, ExceptionSink* xsink) const {
        if (checkPtr(xsink)) {
            return QoreValue();
        }

        const QoreHashNode* h = checkHash(xsink);
        if (!h) {
            return QoreValue();
        }
        return getReferencedKeyValueIntern(h, key, xsink);
    }

    DLLLOCAL QoreHashNode* getRow(ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return 0;

        const QoreHashNode* h = checkHash(xsink);
        return h ? h->hashRefSelf() : nullptr;
    }

    DLLLOCAL QoreHashNode* getSlice(const QoreListNode* args, ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return 0;

        const QoreHashNode* h = checkHash(xsink);
        if (!h)
            return 0;

        ReferenceHolder<QoreHashNode> rv(new QoreHashNode(myHashTypeInfo), xsink);

        ConstListIterator li(args);
        while (li.next()) {
            QoreStringValueHelper str(li.getValue(), QCS_UTF8, xsink);
            if (*xsink)
                return nullptr;
            const char* key = str->getBuffer();
            QoreValue n = getReferencedKeyValueIntern(h, key, xsink);
            if (*xsink)
                return nullptr;
            rv->setKeyValue(key, n, xsink);
            // cannot have an exception here
            assert(!*xsink);
        }

        return rv.release();
    }

    DLLLOCAL virtual const char* getName() const {
        return "ListHashIterator";
    }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const {
        return myHashTypeInfo;
    }

protected:
    const QoreTypeInfo* myHashTypeInfo;

    DLLLOCAL virtual ~QoreListHashIterator() {
    }

    DLLLOCAL const QoreHashNode* checkHash(ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return 0;
        QoreValue n = getValue();
        if (n.getType() != NT_HASH) {
            xsink->raiseException("ITERATOR-ERROR", "The %s object is not a list of hashes, element " QSD " (starting with 0) is type '%s' instead (expected 'hash')", getName(), index(), n.getTypeName());
            return 0;
        }
        return n.get<const QoreHashNode>();
    }

    DLLLOCAL QoreValue getReferencedKeyValueIntern(const QoreHashNode* h, const char* key, ExceptionSink* xsink) const {
        bool exists = false;
        QoreValue n = h->getKeyValueExistence(key, exists);
        if (!exists) {
            xsink->raiseException("LISTHASHITERATOR-ERROR", "key '%s' does not exist in the hash at element " QSD " (starting with 0)", key, index());
            return 0;
        }
        return n.refSelf();
    }
};

#endif // _QORE_QORELISTHASHITERATOR_H

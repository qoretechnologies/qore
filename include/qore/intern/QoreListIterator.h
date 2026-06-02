/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreListIterator.h

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

#ifndef _QORE_QORELISTITERATOR_H

#define _QORE_QORELISTITERATOR_H

extern QoreClass* QC_LISTITERATOR;

DLLLOCAL QoreObject* qore_new_list_iterator_object(QoreProgram* pgm, const QoreListNode* l);

// the c++ object
class QoreListIterator : public QoreIteratorBase, public ConstListIterator {
private:
    DLLLOCAL static const QoreListNode* getListRef(const QoreListNode* l) {
        return l ? l->listRefSelf() : new QoreListNode(autoTypeInfo);
    }

public:
    DLLLOCAL QoreListIterator(const QoreListNode* l, const QoreTypeInfo* element_type = nullptr)
            : ConstListIterator(getListRef(l)) {
        myElementTypeInfo = element_type ? element_type : (l ? l->getValueTypeInfo() : autoTypeInfo);
        if (!QoreTypeInfo::hasType(myElementTypeInfo)) {
            myElementTypeInfo = autoTypeInfo;
        }
    }

    DLLLOCAL QoreListIterator(const QoreListIterator& old) : ConstListIterator(old.l->listRefSelf(), old.pos), myElementTypeInfo(old.myElementTypeInfo) {
    }

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            const_cast<QoreListNode*>(l)->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL QoreValue getReferencedValue(ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return QoreValue();
        return ConstListIterator::getReferencedValue();
    }

    DLLLOCAL virtual const char* getName() const {
        return "ListIterator";
    }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const {
        return myElementTypeInfo;
    }

    // Native fast-path: ConstListIterator::next() + getReferencedValue.
    QORE_NATIVE_FAST_PATH_REFVAL()

protected:
    const QoreTypeInfo* myElementTypeInfo;

    DLLLOCAL virtual ~QoreListIterator() {
    }

    DLLLOCAL int checkPtr(ExceptionSink* xsink) const {
        if (pos == -1) {
            xsink->raiseException("ITERATOR-ERROR", "the %s is not pointing at a valid element; make sure %s::next() returns True before calling this method", getName(), getName());
            return -1;
        }
        return 0;
    }
};

#endif // _QORE_QORELISTITERATOR_H

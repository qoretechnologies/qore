/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHashListIterator.h

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

#ifndef _QORE_QOREHASHLISTITERATOR_H

#define _QORE_QOREHASHLISTITERATOR_H

extern QoreClass* QC_HASHLISTITERATOR;

// the c++ object
class QoreHashListIterator : public QoreIteratorBase {
protected:
    QoreHashNode* h;
    qore_offset_t i, limit;
    bool is_list;

    DLLLOCAL virtual ~QoreHashListIterator() {
    }

    DLLLOCAL int checkPtr(ExceptionSink* xsink) const {
        if (i < 0) {
            xsink->raiseException("ITERATOR-ERROR", "the %s is not pointing at a valid element; make sure %s::next() returns True before calling this method", getName(), getName());
            return -1;
        }
        return 0;
    }

    DLLLOCAL QoreValue getReferencedValueIntern(const QoreValue n, const char* key, ExceptionSink* xsink) const {
        qore_type_t t = n.getType();
        if (t == NT_NOTHING)
            return QoreValue();

        if (!is_list || t != NT_LIST)
            return n.refSelf();

        const QoreListNode* l = n.get<const QoreListNode>();
        if (l->size() != (size_t)limit) {
            xsink->raiseException("HASHLISTITERATOR-ERROR", "hash key '%s' is assigned to a list of size " QSD "; expecting a list of size " QSD, key, l->size(), limit);
            return QoreValue();
        }

        return l->getReferencedEntry(i);
    }

    DLLLOCAL QoreValue getReferencedKeyValueIntern(const char* key, ExceptionSink* xsink) const {
        return getReferencedValueIntern(h->getKeyValue(key), key, xsink);
    }

public:
    DLLLOCAL QoreHashListIterator(const QoreHashNode* n_h) : h(n_h->hashRefSelf()), i(-1), limit(0), is_list(false) {
        if (h->empty())
            return;
        // see if the hash has any lists assigned to it
        ConstHashIterator hi(h);
        while (hi.next()) {
            if (hi.get().getType() == NT_LIST) {
                is_list = true;
                limit = (qore_offset_t)hi.get().get<const QoreListNode>()->size();
                break;
            }
        }
        if (!is_list)
            limit = 1;
    }

    DLLLOCAL QoreHashListIterator() : h(0), i(-1), limit(0), is_list(false) {
    }

    DLLLOCAL QoreHashListIterator(const QoreHashListIterator& old) : h(old.h ? old.h->hashRefSelf() : 0), i(old.i), limit(old.limit), is_list(old.is_list) {
    }

    DLLLOCAL void reset() {
        if (i != -1)
            i = -1;
    }

    using AbstractPrivateData::deref;
    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (h)
                h->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL bool next() {
        if (++i == limit) {
            i = -1;
            return false; // finished
        }
        return true;
    }

    DLLLOCAL bool prev() {
        if (!limit)
            return false; // empty
        if (i == -1) {
            i = limit - 1;
            return true;
        }
        --i;
        return i >= 0;
    }

    DLLLOCAL bool empty() const {
        return !limit;
    }

    // returns true if the iterator is pointing to a valid element
    DLLLOCAL bool valid() const {
        return i != -1;
    }

    // returns the current iterator position in the list or -1 if not pointing at a valid element
    DLLLOCAL size_t index() const {
        return i;
    }

    // returns the number of elements in the list
    DLLLOCAL size_t max() const {
        return limit;
    }

    // returns true when the iterator is pointing to the first element in the list
    DLLLOCAL bool first() const {
        return i == 0;
    }

        // returns true when the iterator is pointing to the last element in the list
    DLLLOCAL bool last() const {
        return i == (limit - 1);
    }

    DLLLOCAL bool set(qore_offset_t pos) {
        if (pos < 0 || pos >= limit) {
            i = -1;
            return false;
        }
        i = pos;
        return true;
    }

    DLLLOCAL QoreValue getReferencedKeyValue(const char* key, ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return QoreValue();

        return getReferencedKeyValueIntern(key, xsink);
    }

    DLLLOCAL QoreHashNode* getRow(ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return nullptr;

        if (!is_list)
            return h->hashRefSelf();

        ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);

        ConstHashIterator hi(h);
        while (hi.next()) {
            QoreValue n = hi.get();
            n = getReferencedValueIntern(n, hi.getKey(), xsink);
            if (*xsink)
                return nullptr;
            rv->setKeyValue(hi.getKey(), n, xsink);
            // cannot have an exception here
            //assert(!*xsink);
        }

        return rv.release();
    }

    DLLLOCAL QoreHashNode* getSlice(const QoreListNode* args, ExceptionSink* xsink) const {
        if (checkPtr(xsink))
            return nullptr;

        ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);

        ConstListIterator li(args);
        while (li.next()) {
            QoreStringValueHelper str(li.getValue(), QCS_UTF8, xsink);
            if (*xsink)
                return nullptr;
            const char* key = str->c_str();
            QoreValue n = getReferencedKeyValueIntern(key, xsink);
            if (*xsink)
                return nullptr;
            rv->setKeyValue(key, n, xsink);
            // cannot have an exception here
            assert(!*xsink);
        }

        return rv.release();
    }

    DLLLOCAL virtual const char* getName() const {
        return "HashListIterator";
    }

    DLLLOCAL virtual const QoreTypeInfo* getElementType() const {
        return h->getValueTypeInfo();
    }
};

#endif // _QORE_QOREHASHLISTITERATOR_H

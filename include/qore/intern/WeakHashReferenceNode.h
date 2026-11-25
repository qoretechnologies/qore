/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WeakHashReferenceNode.h

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

#ifndef _QORE_INTERN_WEAKHASHREFERENCENODE_H

#define _QORE_INTERN_WEAKHASHREFERENCENODE_H

class WeakHashReferenceNode : public AbstractQoreNode {
public:
    DLLLOCAL WeakHashReferenceNode(QoreHashNode* h) : AbstractQoreNode(NT_WEAKREF_HASH, false, true), h(h) {
        h->weakRef();
    }

    DLLLOCAL WeakHashReferenceNode(const WeakHashReferenceNode& old) : AbstractQoreNode(old), h(old.h) {
        h->weakRef();
    }

    //! Returns a new reference to the hash, or a new empty hash that cannot be changed if its last reference was released
    /** A weak reference does not keep the entries of its hash: once the last reference to the hash has been
        released, its entries are being freed or have been freed, and the hash must never be referenced again; the
        hash returned then has its type, reads as empty, and changing it raises \c HASH-ERROR.

        Every value read through a weak reference that the reader can keep or whose entries it reads is read with this
        call.
    */
    DLLLOCAL QoreHashNode* lock() const;

    //! Returns the hash without a reference, for reads of its type or its address only
    /** Its entries must not be read through the pointer returned, as they can be freed while it is read.
    */
    DLLLOCAL QoreHashNode* borrow() const {
        return h;
    }

protected:
    QoreHashNode* h;

    static QoreString nstr;

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        h->weakDeref();
        return true;
    }

    DLLLOCAL virtual ~WeakHashReferenceNode() = default;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        assert(needs_deref);
        return lock();
    }

    DLLLOCAL virtual bool getAsBoolImpl() const;

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual AbstractQoreNode* realCopy() const {
        return new WeakHashReferenceNode(*this);
    }

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const;

    DLLLOCAL virtual const char* getTypeName() const {
        return "weak hash reference";
    }
};

#endif

/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WeakListReferenceNode.h

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

#ifndef _QORE_INTERN_WEAKLISTREFERENCENODE_H

#define _QORE_INTERN_WEAKLISTREFERENCENODE_H

class WeakListReferenceNode : public AbstractQoreNode {
public:
    DLLLOCAL WeakListReferenceNode(QoreListNode* l) : AbstractQoreNode(NT_WEAKREF_LIST, false, true), l(l) {
        l->weakRef();
    }

    DLLLOCAL WeakListReferenceNode(const WeakListReferenceNode& old) : AbstractQoreNode(old), l(old.l) {
        l->weakRef();
    }

    DLLLOCAL QoreListNode* operator*() const {
       return l;
    }

    DLLLOCAL QoreListNode* operator->() const {
       return l;
    }

    DLLLOCAL QoreListNode* get() const {
        return l;
    }

protected:
    QoreListNode* l;

    static QoreString nstr;

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        l->weakDeref();
        return true;
    }

    DLLLOCAL virtual ~WeakListReferenceNode() = default;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        assert(needs_deref);
        needs_deref = false;
        return l;
    }

    DLLLOCAL virtual bool getAsBoolImpl() const {
        return l->getAsBoolImpl();
    }

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink* xsink) const {
        return l->getAsString(str, foff, xsink);
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        return l->getAsString(del, foff, xsink);
    }

    DLLLOCAL virtual AbstractQoreNode* realCopy() const {
        return new WeakListReferenceNode(*this);
    }

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return l->is_equal_soft(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return l->is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return "weak list reference";
    }
};

#endif

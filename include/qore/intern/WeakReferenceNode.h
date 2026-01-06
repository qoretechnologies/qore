/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WeakReferenceNode.h

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

#ifndef _QORE_INTERN_WEAKREFERENCENODE_H

#define _QORE_INTERN_WEAKREFERENCENODE_H

class WeakReferenceNode : public AbstractQoreNode {
public:
    DLLLOCAL WeakReferenceNode(QoreObject* obj) : AbstractQoreNode(NT_WEAKREF, false, true), obj(obj) {
        obj->tRef();
    }

    DLLLOCAL WeakReferenceNode(const WeakReferenceNode& old) : AbstractQoreNode(old), obj(old.obj) {
        obj->tRef();
    }

    DLLLOCAL QoreObject* operator*() const {
       return obj;
    }

    DLLLOCAL QoreObject* operator->() const {
       return obj;
    }

    DLLLOCAL QoreObject* get() const {
        return obj;
    }

protected:
    QoreObject* obj;

    static QoreString nstr;

    DLLLOCAL virtual bool derefImpl(ExceptionSink* xsink) {
        obj->tDeref();
        return true;
    }

    DLLLOCAL virtual ~WeakReferenceNode() = default;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        assert(needs_deref);
        needs_deref = false;
        return obj;
    }

    DLLLOCAL virtual bool getAsBoolImpl() const {
        return obj->getAsBoolImpl();
    }

    DLLLOCAL virtual int getAsString(QoreString &str, int foff, ExceptionSink* xsink) const {
        return obj->getAsString(str, foff, xsink);
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        return obj->getAsString(del, foff, xsink);
    }

    DLLLOCAL virtual AbstractQoreNode* realCopy() const {
        return new WeakReferenceNode(*this);
    }

    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return obj->is_equal_soft(v, xsink);
    }

    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        return obj->is_equal_hard(v, xsink);
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return "weak reference";
    }
};

#endif

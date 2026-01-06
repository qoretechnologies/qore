/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    FunctionalOperatorInterface.h

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

#ifndef _QORE_INTERN_FUNCTIONALOPERATORINTERFACE_H

#define _QORE_INTERN_FUNCTIONALOPERATORINTERFACE_H

#include "qore/intern/AbstractIteratorHelper.h"
#include "qore/intern/FunctionalOperator.h"

class FunctionalOperatorInterface {
public:
    DLLLOCAL virtual ~FunctionalOperatorInterface() {
    }

    // returns the next value, if done iterating, then done is returned as true and the return value is NOTHING
    /* @return if true then done iterating, in which case the return value must be NOTHING
    */
    DLLLOCAL bool getNext(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) = 0;

    DLLLOCAL virtual const QoreTypeInfo* getValueType() const {
        return getValueTypeImpl();
    }

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const = 0;

    DLLLOCAL static FunctionalOperatorInterface* getFunctionalIterator(
            FunctionalOperator::FunctionalValueType& value_type, QoreValue exp, bool fwd, const char* who,
            ExceptionSink* xsink);
};

class QoreFunctionalListOperator : public FunctionalOperatorInterface, public ConstListIterator {
protected:
    bool fwd;
    ExceptionSink* xsink;

public:
    DLLLOCAL QoreFunctionalListOperator(bool f, QoreListNode* l, ExceptionSink* xs) : ConstListIterator(l), fwd(f),
            xsink(xs) {
    }

    DLLLOCAL virtual ~QoreFunctionalListOperator() {
        const_cast<QoreListNode*>(getList())->deref(xsink);
    }

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return l->getValueTypeInfo();
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);
};

class QoreFunctionalSingleValueOperator : public FunctionalOperatorInterface {
protected:
    QoreValue v{};
    bool done = false;
    ExceptionSink* xsink;

public:
    DLLLOCAL QoreFunctionalSingleValueOperator(QoreValue n, ExceptionSink* xs) : v(n), xsink(xs) {
    }

    DLLLOCAL virtual ~QoreFunctionalSingleValueOperator() {
        v.discard(xsink);
    }

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return v.getTypeInfo();
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);
};

class QoreFunctionalIteratorOperator : public FunctionalOperatorInterface {
protected:
    bool temp;
    AbstractIteratorHelper h;
    size_t index = 0;
    ExceptionSink* xsink;

public:
    DLLLOCAL QoreFunctionalIteratorOperator(bool t, AbstractIteratorHelper n_h, ExceptionSink* xs) : temp(t), h(n_h),
            xsink(xs) {
    }

    DLLLOCAL ~QoreFunctionalIteratorOperator() {
        if (temp)
            h.obj->deref(xsink);
    }

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return autoTypeInfo;
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);
};

#endif

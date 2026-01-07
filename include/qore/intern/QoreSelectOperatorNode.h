/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSelectOperatorNode.h

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

#ifndef _QORE_QORESELECTOPERATORNODE_H

#define _QORE_QORESELECTOPERATORNODE_H

#include "qore/intern/AbstractIteratorHelper.h"
#include "qore/intern/FunctionalOperator.h"
#include "qore/intern/FunctionalOperatorInterface.h"

class QoreSelectOperatorNode : public QoreBinaryOperatorNode<>, public FunctionalOperator {
    friend class QoreFunctionalSelectListOperator;
    friend class QoreFunctionalSelectSingleValueOperator;
    friend class QoreFunctionalSelectIteratorOperator;
    friend class QoreFunctionalSelectOperator;

public:
    DLLLOCAL QoreSelectOperatorNode(const QoreProgramLocation* loc, QoreValue l, QoreValue r)
            : QoreBinaryOperatorNode<>(loc, l, r), returnTypeInfo(nullptr), iterator_func(nullptr) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return select_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const {
        QoreSelectOperatorNode* rv = copyBackgroundExplicit<QoreSelectOperatorNode>(xsink);
        rv->iterator_func = dynamic_cast<FunctionalOperator*>(rv->left.getInternalNode());
        return rv;
    }

    DLLLOCAL virtual FunctionalOperatorInterface* getFunctionalIteratorImpl(FunctionalValueType& value_type,
            ExceptionSink* xsink) const;

protected:
    const QoreTypeInfo* returnTypeInfo;
    FunctionalOperator* iterator_func;

    DLLLOCAL static QoreString select_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue evalFunc(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual ~QoreSelectOperatorNode() {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }

    DLLLOCAL QoreValue selectIterator(AbstractIteratorHelper& h, ExceptionSink* xsink) const;
};

class QoreFunctionalSelectListOperator : public FunctionalOperatorInterface, public ConstListIterator {
public:
    DLLLOCAL QoreFunctionalSelectListOperator(const QoreSelectOperatorNode* s, QoreListNode* l, ExceptionSink* xs)
            : ConstListIterator(l), select(s), xsink(xs) {
    }

    DLLLOCAL virtual ~QoreFunctionalSelectListOperator() {
        const_cast<QoreListNode*>(getList())->deref(xsink);
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return l->getValueTypeInfo();
    }

protected:
    const QoreSelectOperatorNode* select;
    ExceptionSink* xsink;
};

class QoreFunctionalSelectSingleValueOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalSelectSingleValueOperator(const QoreSelectOperatorNode* s, QoreValue n, ExceptionSink* xs)
            : select(s), v(n), xsink(xs) {
    }

    DLLLOCAL virtual ~QoreFunctionalSelectSingleValueOperator() {
        v.discard(xsink);
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return v.getTypeInfo();
    }

protected:
    const QoreSelectOperatorNode* select;
    QoreValue v{};
    bool done = false;
    ExceptionSink* xsink;
};

class QoreFunctionalSelectIteratorOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalSelectIteratorOperator(const QoreSelectOperatorNode* s, bool t, AbstractIteratorHelper n_h,
            ExceptionSink* xs) : select(s), temp(t), h(n_h), xsink(xs) {
    }

    DLLLOCAL ~QoreFunctionalSelectIteratorOperator() {
        if (temp)
            h.obj->deref(xsink);
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return autoTypeInfo;
    }

protected:
    const QoreSelectOperatorNode* select;
    bool temp;
    AbstractIteratorHelper h;
    size_t index = 0;
    ExceptionSink* xsink;
};

class QoreFunctionalSelectOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalSelectOperator(const QoreSelectOperatorNode* s, FunctionalOperatorInterface* n_f)
            : select(s), f(n_f) {
    }

    DLLLOCAL ~QoreFunctionalSelectOperator() {
        delete f;
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return f->getValueType();
    }

protected:
    const QoreSelectOperatorNode* select;
    FunctionalOperatorInterface* f;
    size_t index = 0;
};

#endif

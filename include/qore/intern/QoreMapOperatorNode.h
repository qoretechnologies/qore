/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreMapOperatorNode.h

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

#ifndef _QORE_QOREMAPOPERATORNODE_H

#define _QORE_QOREMAPOPERATORNODE_H

#include "qore/intern/AbstractIteratorHelper.h"
#include "qore/intern/FunctionalOperator.h"
#include "qore/intern/FunctionalOperatorInterface.h"

class QoreMapOperatorNode : public QoreBinaryOperatorNode<>, public FunctionalOperator {
    friend class QoreFunctionalMapListOperator;
    friend class QoreFunctionalMapSingleValueOperator;
    friend class QoreFunctionalMapIteratorOperator;
    friend class QoreFunctionalMapOperator;

public:
    DLLLOCAL QoreMapOperatorNode(const QoreProgramLocation* loc, QoreValue l, QoreValue r)
            : QoreBinaryOperatorNode<>(loc, l, r), returnTypeInfo(nullptr), iterator_func(nullptr) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return map_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const {
        QoreMapOperatorNode* rv = copyBackgroundExplicit<QoreMapOperatorNode>(xsink);
        rv->iterator_func = dynamic_cast<FunctionalOperator*>(rv->right.getInternalNode());
        return rv;
    }

    DLLLOCAL static const QoreTypeInfo* setReturnTypeInfo(const QoreTypeInfo*& returnTypeInfo,
            const QoreTypeInfo* expTypeInfo, const QoreTypeInfo* iteratorTypeInfo);

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return true;
    }

    // Public accessor for IR lowering to get the result list type
    DLLLOCAL const QoreTypeInfo* getMapReturnType() const {
        return returnTypeInfo;
    }

    // Public accessor for IR lowering to get the mapped expression element type directly
    DLLLOCAL const QoreTypeInfo* getMapExpType() const {
        return expTypeInfo;
    }

protected:
    const QoreTypeInfo* returnTypeInfo;
    const QoreTypeInfo* expTypeInfo = nullptr;
    FunctionalOperator* iterator_func;

    DLLLOCAL static QoreString map_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL QoreValue evalFunc(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual ~QoreMapOperatorNode() {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }

    DLLLOCAL QoreValue mapIterator(AbstractIteratorHelper& h, ExceptionSink* xsink) const;

    DLLLOCAL virtual FunctionalOperatorInterface* getFunctionalIteratorImpl(FunctionalValueType& value_type,
            ExceptionSink* xsink) const;
};

class QoreFunctionalMapListOperator : public FunctionalOperatorInterface, public ConstListIterator {
public:
    DLLLOCAL QoreFunctionalMapListOperator(const QoreMapOperatorNode* m, QoreListNode* l, ExceptionSink* xs)
            : ConstListIterator(l), map(m), xsink(xs) {
    }

    DLLLOCAL virtual ~QoreFunctionalMapListOperator() {
        const_cast<QoreListNode*>(getList())->deref(xsink);
    }

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        // issue #2651: return the iterator expression type as the functional value type
        return map->expTypeInfo;
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

protected:
    const QoreMapOperatorNode* map;
    ExceptionSink* xsink;
};

class QoreFunctionalMapSingleValueOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalMapSingleValueOperator(const QoreMapOperatorNode* m, QoreValue n, ExceptionSink* xs)
            : map(m), v(n), xsink(xs) {
    }

    DLLLOCAL virtual ~QoreFunctionalMapSingleValueOperator() {
        v.discard(xsink);
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return v.getTypeInfo();
    }

protected:
    const QoreMapOperatorNode* map;
    QoreValue v{};
    bool done = false;
    ExceptionSink* xsink;
};

class QoreFunctionalMapIteratorOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalMapIteratorOperator(const QoreMapOperatorNode* m, bool t, AbstractIteratorHelper n_h,
            ExceptionSink* xs) : map(m), temp(t), h(n_h), xsink(xs) {
    }

    DLLLOCAL ~QoreFunctionalMapIteratorOperator() {
        if (temp)
            h.obj->deref(xsink);
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return autoTypeInfo;
    }

protected:
    const QoreMapOperatorNode* map;
    bool temp;
    AbstractIteratorHelper h;
    size_t index = 0;
    ExceptionSink* xsink;
};

class QoreFunctionalMapOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalMapOperator(const QoreMapOperatorNode* m, FunctionalOperatorInterface* n_f) : map(m), f(n_f) {
    }

    DLLLOCAL ~QoreFunctionalMapOperator() {
        delete f;
    }

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return map->expTypeInfo;
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

protected:
    const QoreMapOperatorNode* map;
    FunctionalOperatorInterface* f;
    size_t index = 0;
};

#endif

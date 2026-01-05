/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreMapSelectOperatorNode.h

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

#ifndef _QORE_QOREMAPSELECTOPERATORNODE_H

#define _QORE_QOREMAPSELECTOPERATORNODE_H

#include "qore/intern/AbstractIteratorHelper.h"
#include "qore/intern/FunctionalOperator.h"
#include "qore/intern/FunctionalOperatorInterface.h"

class QoreMapSelectOperatorNode : public QoreNOperatorNodeBase<3>, public FunctionalOperator {
    friend class QoreFunctionalMapSelectListOperator;
    friend class QoreFunctionalMapSelectSingleValueOperator;
    friend class QoreFunctionalMapSelectIteratorOperator;
    friend class QoreFunctionalMapSelectOperator;

public:
    DLLLOCAL QoreMapSelectOperatorNode(const QoreProgramLocation* loc, QoreValue e0, QoreValue e1, QoreValue e2)
            : QoreNOperatorNodeBase<3>(loc, e0, e1, e2),
            returnTypeInfo(nullptr), iterator_func(nullptr) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return map_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        ValueHolder n_e0(copy_value_and_resolve_lvar_refs(e[0], xsink), xsink);
        if (*xsink)
            return nullptr;
        ValueHolder n_e1(copy_value_and_resolve_lvar_refs(e[1], xsink), xsink);
        if (*xsink)
            return nullptr;
        ValueHolder n_e2(copy_value_and_resolve_lvar_refs(e[2], xsink), xsink);
        if (*xsink)
            return nullptr;
        QoreMapSelectOperatorNode* rv = new QoreMapSelectOperatorNode(loc, n_e0.release(), n_e1.release(),
            n_e2.release());
        rv->iterator_func = dynamic_cast<FunctionalOperator*>(rv->e[1].getInternalNode());
        return rv;
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return true;
    }

    DLLLOCAL virtual FunctionalOperatorInterface* getFunctionalIteratorImpl(RuntimeConfig& rc,
            FunctionalValueType& value_type, ExceptionSink* xsink) const override;

protected:
    const QoreTypeInfo* returnTypeInfo;
    const QoreTypeInfo* expTypeInfo = nullptr;
    FunctionalOperator* iterator_func;

    DLLLOCAL static QoreString map_str;

    DLLLOCAL virtual QoreValue evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual ~QoreMapSelectOperatorNode() {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }

    DLLLOCAL QoreValue mapSelectIterator(AbstractIteratorHelper& h, ExceptionSink* xsink) const;
};

class QoreFunctionalMapSelectListOperator : public FunctionalOperatorInterface, public ConstListIterator {
protected:
    const QoreMapSelectOperatorNode* map;
    ExceptionSink* xsink;

public:
    DLLLOCAL QoreFunctionalMapSelectListOperator(const QoreMapSelectOperatorNode* m, QoreListNode* l, ExceptionSink* xs) : ConstListIterator(l), map(m), xsink(xs) {
    }

    DLLLOCAL virtual ~QoreFunctionalMapSelectListOperator() {
        const_cast<QoreListNode*>(getList())->deref(xsink);
    }

    DLLLOCAL virtual const QoreTypeInfo* getValueType() const {
        return getList()->getValueTypeInfo();
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return l->getValueTypeInfo();
    }
};

class QoreFunctionalMapSelectSingleValueOperator : public FunctionalOperatorInterface {
protected:
    const QoreMapSelectOperatorNode* map;
    QoreValue v{};
    bool done;
    ExceptionSink* xsink;

public:
    DLLLOCAL QoreFunctionalMapSelectSingleValueOperator(const QoreMapSelectOperatorNode* m, QoreValue n, ExceptionSink* xs) : map(m), v(n), done(false), xsink(xs) {
    }

    DLLLOCAL virtual ~QoreFunctionalMapSelectSingleValueOperator() {
        v.discard(xsink);
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return v.getTypeInfo();
    }
};

class QoreFunctionalMapSelectIteratorOperator : public FunctionalOperatorInterface {
protected:
    const QoreMapSelectOperatorNode* map;
    bool temp;
    AbstractIteratorHelper h;
    size_t index = 0;
    ExceptionSink* xsink;

public:
    DLLLOCAL QoreFunctionalMapSelectIteratorOperator(const QoreMapSelectOperatorNode* m, bool t, AbstractIteratorHelper n_h, ExceptionSink* xs) : map(m), temp(t), h(n_h), xsink(xs) {
    }

    DLLLOCAL ~QoreFunctionalMapSelectIteratorOperator() {
        if (temp)
            h.obj->deref(xsink);
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return autoTypeInfo;
    }
};

class QoreFunctionalMapSelectOperator : public FunctionalOperatorInterface {
protected:
    const QoreMapSelectOperatorNode* map;
    FunctionalOperatorInterface* f;
    size_t index;

public:
    DLLLOCAL QoreFunctionalMapSelectOperator(const QoreMapSelectOperatorNode* m, FunctionalOperatorInterface* n_f) : map(m), f(n_f), index(0) {
    }

    DLLLOCAL ~QoreFunctionalMapSelectOperator() {
        delete f;
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return map->expTypeInfo;
    }
};

#endif

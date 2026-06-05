/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreStreamingOperatorNode.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_QORESTREAMINGOPERATORNODE_H
#define _QORE_QORESTREAMINGOPERATORNODE_H

#include "qore/intern/FunctionalOperator.h"
#include "qore/intern/FunctionalOperatorInterface.h"

class QoreFunctionalStreamingOperator;

class QoreStreamingOperatorNode : public QoreBinaryOperatorNode<>, public FunctionalOperator {
    friend class QoreFunctionalStreamingOperator;

public:
    enum Kind {
        First,
        Any,
        All,
        Count,
        Take,
        Drop,
        TakeWhile,
        TakeUntil,
    };

    DLLLOCAL QoreStreamingOperatorNode(const QoreProgramLocation* loc, Kind kind, QoreValue predicate,
            QoreValue source) : QoreBinaryOperatorNode<>(loc, predicate, source), kind(kind) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual const char* getTypeName() const {
        return getOperatorName();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        ValueHolder n_left(copy_value_and_resolve_lvar_refs(left, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        ValueHolder n_right(copy_value_and_resolve_lvar_refs(right, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        QoreStreamingOperatorNode* rv = new QoreStreamingOperatorNode(loc, kind, n_left.release(), n_right.release());
        rv->iterator_func = dynamic_cast<FunctionalOperator*>(rv->right.getInternalNode());
        return rv;
    }

    DLLLOCAL Kind getKind() const {
        return kind;
    }

    DLLLOCAL const QoreValue& getPredicate() const {
        return left;
    }

    DLLLOCAL const QoreValue& getSource() const {
        return right;
    }

    DLLLOCAL bool hasPredicate() const {
        return static_cast<bool>(left);
    }

    DLLLOCAL virtual FunctionalOperatorInterface* getFunctionalIteratorImpl(FunctionalValueType& value_type,
            ExceptionSink* xsink) const;

protected:
    Kind kind;
    const QoreTypeInfo* elementTypeInfo = nullptr;
    const QoreTypeInfo* returnTypeInfo = nullptr;
    FunctionalOperator* iterator_func = nullptr;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }

    DLLLOCAL const char* getOperatorName() const;
    DLLLOCAL bool isTerminal() const;
    DLLLOCAL bool isPredicateOperator() const;
};

class QoreFunctionalStreamingOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalStreamingOperator(const QoreStreamingOperatorNode* n_op, FunctionalOperatorInterface* n_f,
            int64 n_limit = 0) : op(n_op), f(n_f), limit(n_limit) {
    }

    DLLLOCAL ~QoreFunctionalStreamingOperator() {
        delete f;
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return f->getValueType();
    }

private:
    const QoreStreamingOperatorNode* op;
    FunctionalOperatorInterface* f;
    int64 limit = 0;
    int64 emitted = 0;
    int64 skipped = 0;
    size_t index = 0;
    bool initialized = false;
    bool done = false;

    DLLLOCAL bool skipDropPrefix(ExceptionSink* xsink);
    DLLLOCAL bool predicateMatches(ValueOptionalRefHolder& iv, ExceptionSink* xsink);
};

#endif

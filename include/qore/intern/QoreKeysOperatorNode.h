/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreKeysOperatorNode.h

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

#ifndef _QORE_QOREKEYSOPERATORNODE_H

#define _QORE_QOREKEYSOPERATORNODE_H

#include "qore/intern/FunctionalOperator.h"
#include "qore/intern/FunctionalOperatorInterface.h"

class QoreKeysOperatorNode : public QoreSingleExpressionOperatorNode<QoreOperatorNode>, public FunctionalOperator {
protected:
    const QoreTypeInfo* returnTypeInfo;

    DLLLOCAL static QoreString keys_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }

    DLLLOCAL virtual FunctionalOperatorInterface* getFunctionalIteratorImpl(FunctionalValueType& value_type,
            ExceptionSink* xsink) const;

public:
    DLLLOCAL QoreKeysOperatorNode(const QoreProgramLocation* loc, QoreValue exp)
            : QoreSingleExpressionOperatorNode<QoreOperatorNode>(loc, exp), returnTypeInfo(nullptr) {
    }

    DLLLOCAL virtual ~QoreKeysOperatorNode() {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return keys_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        return copyBackgroundExplicit<QoreKeysOperatorNode>(xsink);
    }
};

class QoreFunctionalKeysOperator : public FunctionalOperatorInterface, public ConstHashIterator {
public:
    DLLLOCAL QoreFunctionalKeysOperator(QoreHashNode* h, ExceptionSink* xs) : ConstHashIterator(h), xsink(xs) {
    }

    DLLLOCAL virtual ~QoreFunctionalKeysOperator() {
        QoreHashNode* h = const_cast<QoreHashNode*>(getHash());
        if (h)
            h->deref(xsink);
    }

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return stringTypeInfo;
    }

protected:
    ExceptionSink* xsink;
};

#endif

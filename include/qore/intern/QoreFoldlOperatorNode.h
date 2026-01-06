/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreFoldlOperatorNode.h

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

#ifndef _QORE_QOREFOLDLOPERATORNODE_H

#define _QORE_QOREFOLDLOPERATORNODE_H

#include "qore/intern/AbstractIteratorHelper.h"
#include "qore/intern/FunctionalOperator.h"
#include "qore/intern/FunctionalOperatorInterface.h"

class QoreFoldlOperatorNode : public QoreBinaryOperatorNode<> {
protected:
    const QoreTypeInfo* returnTypeInfo;
    FunctionalOperator* iterator_func;

    DLLLOCAL static QoreString foldl_str;

    DLLLOCAL QoreValue doFold(bool fwd, bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual ~QoreFoldlOperatorNode() {
    }

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }

    DLLLOCAL QoreValue foldIterator(AbstractIteratorHelper& h, ExceptionSink* xsink) const;

    DLLLOCAL FunctionalOperatorInterface* getFunctionalIterator(FunctionalOperator::FunctionalValueType& value_type, bool fwd, ExceptionSink* xsink) const;

public:
    DLLLOCAL QoreFoldlOperatorNode(const QoreProgramLocation* loc, QoreValue l, QoreValue r) : QoreBinaryOperatorNode<>(loc, l, r), returnTypeInfo(nullptr), iterator_func(nullptr) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return foldl_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const {
        QoreFoldlOperatorNode* rv = copyBackgroundExplicit<QoreFoldlOperatorNode>(xsink);
        // use lazy evaluation if the iterator expression supports it
        rv->iterator_func = dynamic_cast<FunctionalOperator*>(rv->right.getInternalNode());
        return rv;
    }
};

class QoreFoldrOperatorNode : public QoreFoldlOperatorNode {
protected:
    DLLLOCAL static QoreString foldr_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

public:
    DLLLOCAL QoreFoldrOperatorNode(const QoreProgramLocation* loc, QoreValue l, QoreValue r) : QoreFoldlOperatorNode(loc, l, r) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return foldr_str.c_str();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const {
        QoreFoldrOperatorNode* rv = copyBackgroundExplicit<QoreFoldrOperatorNode>(xsink);
        // use lazy evaluation if the iterator expression supports it
        rv->iterator_func = dynamic_cast<FunctionalOperator*>(rv->right.getInternalNode());
        return rv;
    }
};

#endif

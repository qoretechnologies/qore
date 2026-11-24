/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePlusEqualsOperatorNode.h

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

#ifndef _QORE_QOREPLUSEQUALSOPERATORNODE_H
#define _QORE_QOREPLUSEQUALSOPERATORNODE_H

class QorePlusEqualsOperatorNode : public QoreBinaryLValueOperatorNode {
OP_COMMON
protected:
    //! how the lvalue path is resolved, decided when the operation first runs (qore#7391)
    QoreStorePathMode store_mode;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    template <bool Lexical>
    DLLLOCAL QoreValue evalImplInternal(bool& needs_deref, ExceptionSink* xsink) const;

    //! adds the operand \a right_holder to the lvalue that \a v holds; \a converted is the value that the last round of
    //! a retry converted: the operand that an assignment of the first round took (qore#7391)
    /** @return 0 if the operation was made (\a result holds the value of the expression then), -1 if not
    */
    template <bool Lexical>
    DLLLOCAL int plusEqualsOn(LValueHelper& v, QoreValue converted, ValueHolder& right_holder, QoreValue& result,
            ExceptionSink* xsink) const;

    DLLLOCAL QoreValue evalLexical(bool& needs_deref, ExceptionSink* xsink) const;

public:
    DLLLOCAL QorePlusEqualsOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreBinaryLValueOperatorNode(loc, left, right) {
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const {
        return copyBackgroundExplicit<QorePlusEqualsOperatorNode>(xsink);
    }
};

class LexicalQorePlusEqualsOperatorNode : public QorePlusEqualsOperatorNode {
public:
    using QorePlusEqualsOperatorNode::QorePlusEqualsOperatorNode;

    QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        auto* copy = copyBackgroundExplicit<LexicalQorePlusEqualsOperatorNode>(xsink);
        if (copy) {
            copy->ti = ti;
        }
        return copy;
    }

protected:
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalLexical(needs_deref, xsink);
    }
};

#endif

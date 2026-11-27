/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QorePushOperatorNode.h

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

#ifndef _QORE_QOREPUSHOPERATORNODE_H

#define _QORE_QOREPUSHOPERATORNODE_H

#include "qore/intern/qore_string_private.h"
class QorePushOperatorNode : public QoreUnshiftOperatorNode {
protected:
    template <bool Lexical>
    DLLLOCAL QoreValue evalImplInternal(bool& needs_deref, ExceptionSink* xsink) const;

    //! pushes the operand \a res to the list of the lvalue that \a val holds; \a converted is the value that the last
    //! round of a retry converted, which the operation does not use (qore#7391)
    /** @return 0 if the operation was made (\a rv holds the value of the expression then), -1 if not
    */
    template <bool Lexical>
    DLLLOCAL int pushOn(LValueHelper& val, QoreValue converted, QoreMutationOperandHolder& res, QoreValue& rv,
            ExceptionSink* xsink) const;

    DLLLOCAL QoreValue evalLexical(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL static QoreString push_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

public:
    DLLLOCAL QorePushOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreUnshiftOperatorNode(loc, left, right) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = false;
        return &push_str;
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        qore_string_private::get(str)->concatAsciiName(push_str);
        return 0;
    }

    // returns the type name as a c string
    DLLLOCAL virtual const char* getTypeName() const {
        return push_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        return copyBackgroundExplicit<QorePushOperatorNode>(xsink);
    }
};

class LexicalQorePushOperatorNode : public QorePushOperatorNode {
public:
    using QorePushOperatorNode::QorePushOperatorNode;

    QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        auto* copy = copyBackgroundExplicit<LexicalQorePushOperatorNode>(xsink);
        if (copy) {
            copy->returnTypeInfo = returnTypeInfo;
        }
        return copy;
    }

protected:
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalLexical(needs_deref, xsink);
    }
};

#endif

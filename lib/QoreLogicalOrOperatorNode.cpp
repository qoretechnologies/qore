/*
    QoreLogicalOrOperatorNode.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreStatementTemps.h"

namespace {
class StrictBooleanLogicalOrOperatorNode : public QoreLogicalOrOperatorNode {
public:
    using QoreLogicalOrOperatorNode::QoreLogicalOrOperatorNode;
    QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        return copyBackgroundExplicit<StrictBooleanLogicalOrOperatorNode>(xsink);
    }
private:
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalCommon<true>(needs_deref, xsink);
    }
};
}

QoreLogicalOrOperatorNode* QoreLogicalOrOperatorNode::create(LexicalSemanticPolicy policy, const QoreProgramLocation* loc,
        QoreValue left, QoreValue right) {
    QoreLogicalOrOperatorNode* rv = policy.strictBoolean() ? new StrictBooleanLogicalOrOperatorNode(loc, left, right)
        : new QoreLogicalOrOperatorNode(loc, left, right);
    ParseNode::bindSemanticPolicy(QoreValue(rv), policy);
    return rv;
}

QoreValue QoreLogicalOrOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return evalCommon<false>(needs_deref, xsink);
}


QoreString QoreLogicalOrOperatorNode::logical_or_str("logical or (&&) operator expression");

template <bool StrictBoolean>
QORE_INTERN_ALWAYS_INLINE QoreValue QoreLogicalOrOperatorNode::evalCommon(bool& needs_deref, ExceptionSink* xsink) const {
    QoreOperandEvalHolder lh(left, xsink);
    if (*xsink)
        return false;
    // if left side is true, then do not evaluate right side (logical short circuiting)
    if (LexicalSemanticPolicy::getAsBoolFor<StrictBoolean>(*lh)) {
        return true;
    }

    QoreOperandEvalHolder rh(right, xsink);
    if (*xsink)
        return false;
    return LexicalSemanticPolicy::getAsBoolFor<StrictBoolean>(*rh);
}

/*
    QoreLogicalNotOperatorNode.cpp

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
#include "qore/intern/qore_string_private.h"
#include "qore/intern/QoreStatementTemps.h"

namespace {
class StrictBooleanLogicalNotOperatorNode : public QoreLogicalNotOperatorNode {
public:
    using QoreLogicalNotOperatorNode::QoreLogicalNotOperatorNode;
    QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        return copyBackgroundExplicit<StrictBooleanLogicalNotOperatorNode>(xsink);
    }
private:
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalCommon<true>(needs_deref, xsink);
    }
};
}

QoreLogicalNotOperatorNode* QoreLogicalNotOperatorNode::create(LexicalSemanticPolicy policy, const QoreProgramLocation* loc,
        QoreValue exp) {
    QoreLogicalNotOperatorNode* rv = policy.strictBoolean() ? new StrictBooleanLogicalNotOperatorNode(loc, exp)
        : new QoreLogicalNotOperatorNode(loc, exp);
    ParseNode::bindSemanticPolicy(QoreValue(rv), policy);
    return rv;
}

QoreValue QoreLogicalNotOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return evalCommon<false>(needs_deref, xsink);
}


QoreString QoreLogicalNotOperatorNode::LogicalNot_str("logical not operator expression");

static void set_unary_analysis(QoreParseContext& parse_context,
        const QoreParseAnalysis& operand) {
    parse_context.analysis.clear();
    parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
    parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
    parse_context.analysis.known_type = parse_context.typeInfo;
    if (operand.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
}

// if del is true, then the returned QoreString* should be deleted, if false, then it must not be
QoreString* QoreLogicalNotOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &LogicalNot_str;
}

int QoreLogicalNotOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    qore_string_private::get(str)->concat(&LogicalNot_str);
    return 0;
}

template <bool StrictBoolean>
QORE_INTERN_ALWAYS_INLINE QoreValue QoreLogicalNotOperatorNode::evalCommon(bool& needs_deref, ExceptionSink* xsink) const {
    assert(exp);
    QoreOperandEvalHolder v(exp, xsink);
    return !LexicalSemanticPolicy::getAsBoolFor<StrictBoolean>(*v);
}

int QoreLogicalNotOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    QoreParseAnalysis operand_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(exp, parse_context);
        operand_analysis = parse_context.analysis;
    }

    parse_context.typeInfo = boolTypeInfo;

    // evaluate immediately if possible
    if (!err && exp.isValue()) {
        SimpleRefHolder<QoreLogicalNotOperatorNode> th(this);
        val = !semantic_policy.getAsBool(exp);
    }

    set_unary_analysis(parse_context, operand_analysis);
    return err;
}

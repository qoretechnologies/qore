/*
    QoreLogicalAndOperatorNode.cpp

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
#include "qore/intern/StatementBlock.h"
#include "qore/intern/QoreStatementTemps.h"

namespace {
class StrictBooleanLogicalAndOperatorNode : public QoreLogicalAndOperatorNode {
public:
    using QoreLogicalAndOperatorNode::QoreLogicalAndOperatorNode;
    QoreOperatorNode* copyBackground(ExceptionSink* xsink) const override {
        return copyBackgroundExplicit<StrictBooleanLogicalAndOperatorNode>(xsink);
    }
private:
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalCommon<true>(needs_deref, xsink);
    }
};
}

QoreLogicalAndOperatorNode* QoreLogicalAndOperatorNode::create(LexicalSemanticPolicy policy, const QoreProgramLocation* loc,
        QoreValue left, QoreValue right) {
    QoreLogicalAndOperatorNode* rv = policy.strictBoolean() ? new StrictBooleanLogicalAndOperatorNode(loc, left, right)
        : new QoreLogicalAndOperatorNode(loc, left, right);
    ParseNode::bindSemanticPolicy(QoreValue(rv), policy);
    return rv;
}

QoreValue QoreLogicalAndOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return evalCommon<false>(needs_deref, xsink);
}


QoreString QoreLogicalAndOperatorNode::logical_and_str("logical and (&&) operator expression");

static void set_binary_analysis_and(QoreParseContext& parse_context,
        const QoreParseAnalysis& left,
        const QoreParseAnalysis& right) {
    parse_context.analysis.clear();
    parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
    parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
    parse_context.analysis.known_type = parse_context.typeInfo;
    if (left.hasFlag(QoreParseAnalysis::DefinitelyAssigned)
            && right.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
}

template <bool StrictBoolean>
QORE_INTERN_ALWAYS_INLINE QoreValue QoreLogicalAndOperatorNode::evalCommon(bool& needs_deref, ExceptionSink* xsink) const {
    // if left side is 0, then do not evaluate right side (logical short circuiting)
    QoreOperandEvalHolder lh(left, xsink);
    if (*xsink)
        return QoreValue();
    if (!LexicalSemanticPolicy::getAsBoolFor<StrictBoolean>(*lh)) {
        return false;
    }

    QoreOperandEvalHolder rh(right, xsink);
    if (*xsink)
        return QoreValue();
    return LexicalSemanticPolicy::getAsBoolFor<StrictBoolean>(*rh);
}

int QoreLogicalAndOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    parse_context.typeInfo = nullptr;
    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(left, parse_context);
        left_analysis = parse_context.analysis;
    }

    AssignedStateHelper ash;
    ash.saveState();

    parse_context.typeInfo = nullptr;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        if (parse_init_value(right, parse_context) && !err) {
            err = -1;
        }
        right_analysis = parse_context.analysis;
    }
    ash.recordBranchAndRestore();
    ash.recordSavedAsImplicitBranch();
    ash.mergeAndApply();

    // see if both arguments are constants, then eval immediately and substitute this node with the result
    if (!err && left.isValue() && right.isValue()) {
        SimpleRefHolder<QoreLogicalAndOperatorNode> del(this);
        ParseExceptionSink xsink;
        ValueEvalOptimizedRefHolder v(this, *xsink);
        assert(!**xsink);
        QoreValue result = v.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing()) {
            val = result;
        } else {
            // constants not resolved - skip parse-time folding, let runtime handle it
            del.release();
        }
    }

    parse_context.typeInfo = boolTypeInfo;
    set_binary_analysis_and(parse_context, left_analysis, right_analysis);
    return err;
}

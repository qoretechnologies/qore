/*
    QoreMinusEqualsOperatorNode.cpp

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
#include "qore/intern/ParseNode.h"
#include "qore/intern/QoreIRInterpreter.h"

QoreString QoreMinusEqualsOperatorNode::op_str("-= operator expression");

namespace {
// how the -= operator treats an lvalue type at parse time
enum class minus_equals_lvalue_e {
    // the lvalue keeps its type
    KEEP,
    // the lvalue is assigned an integer value, which its type must accept
    INT,
    // the type is not known well enough; the operation is decided at runtime
    GENERIC,
};

// returns how the -= operator treats the given lvalue type and its dereferenced type
minus_equals_lvalue_e get_minus_equals_lvalue_kind(const QoreTypeInfo* ti, const QoreTypeInfo* check_ti) {
    if (QoreTypeInfo::isType(check_ti, NT_HASH)
        || QoreTypeInfo::isType(check_ti, NT_OBJECT)
        || QoreTypeInfo::isType(check_ti, NT_FLOAT)
        || QoreTypeInfo::isType(check_ti, NT_NUMBER)
        || QoreTypeInfo::isType(check_ti, NT_DATE)) {
        return minus_equals_lvalue_e::KEEP;
    }
    // if the lhs type is not one of the above types, there are 2 possibilities: the lvalue has no value, in which
    // case it takes the value of the right side, or if it's anything else it's converted to an integer, so we just
    // check if it can be assigned an integer value
    if (QoreTypeInfo::returnsSingle(ti) && !QoreTypeInfo::equal(ti, timeoutTypeInfo)) {
        return minus_equals_lvalue_e::INT;
    }
    return minus_equals_lvalue_e::GENERIC;
}
}

int QoreMinusEqualsOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    int err;
    {
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_FOR_ASSIGNMENT);
        parse_context.typeInfo = nullptr;
        err = parse_init_value(left, parse_context);
        ti = parse_context.typeInfo;
    }
    if (!err) {
        err = checkLValue(left, parse_context.pflag);
    }

    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }
    //const QoreTypeInfo* rightTypeInfo = parse_context.typeInfo;

    // Dereference reference types before type checking so that
    // reference<hash<...>> etc. are recognized correctly
    const QoreTypeInfo* check_ti = ti;
    if (QoreTypeInfo::isReference(ti)) {
        const QoreTypeInfo* deref_ti = QoreTypeInfo::getReferenceTarget(ti);
        if (deref_ti) {
            check_ti = deref_ti;
        }
    }

    // the parse-time checks are made on the plain type T of an or-nothing lvalue (*T): an operation that T refuses
    // can never succeed on *T; the error names the declared type, and the result type and the runtime operation
    // follow the declared type
    const QoreTypeInfo* plain_ti = get_or_nothing_base_type(ti);
    const QoreTypeInfo* plain_check_ti = get_or_nothing_base_type(check_ti);
    minus_equals_lvalue_e kind = get_minus_equals_lvalue_kind(ti, check_ti);
    minus_equals_lvalue_e check_kind = (plain_ti == ti && plain_check_ti == check_ti)
        ? kind
        : get_minus_equals_lvalue_kind(plain_ti, plain_check_ti);

    if (check_kind == minus_equals_lvalue_e::INT && check_lvalue_int(loc, ti, "-=") && !err) {
        err = -1;
    }

    if (kind == minus_equals_lvalue_e::INT) {
        parse_context.typeInfo = ti = bigIntTypeInfo;
        // the analysis left by the right operand describes that operand, not the result
        qore_set_result_parse_analysis(parse_context, ti);
        val = makeSpecialization<QoreIntMinusEqualsOperatorNode>();
        if (val.hasNode()) {
            auto* parse_node = dynamic_cast<ParseNode*>(val.getInternalNode());
            if (parse_node) {
                parse_node->setParseAnalysis(parse_context.analysis);
            }
        }
        return err;
    }
    if (kind == minus_equals_lvalue_e::GENERIC) {
        ti = nullptr;
    }
    parse_context.typeInfo = ti;
    // the analysis left by the right operand describes that operand, not the result
    qore_set_result_parse_analysis(parse_context, ti);
    return err;
}

QoreValue QoreMinusEqualsOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    // the operand is a statement of its own where the other execution modes evaluate it so
    QoreMutationOperandHolder new_right(left, right, false, xsink);
    if (*xsink) {
        return QoreValue();
    }

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper v(left, xsink);
    if (!v) {
        return QoreValue();
    }

    // the implementation is shared with the other execution modes
    ValueHolder rv(doMinusEqualsOnLValue(v, *new_right, xsink), xsink);
    if (*xsink || !ref_rv) {
        return QoreValue();
    }
    return rv.release();
}

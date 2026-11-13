/*
    QorePlusEqualsOperatorNode.cpp

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
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/ParseNode.h"
#include "qore/intern/QoreIRInterpreter.h"

QoreString QorePlusEqualsOperatorNode::op_str("+= operator expression");

namespace {
// how the += operator treats an lvalue type that does not hold a list at parse time
enum class plus_equals_lvalue_e {
    // the lvalue keeps its type
    KEEP,
    // the lvalue is assigned an integer value, which its type must accept
    INT,
    // the type is not known well enough; the operation is decided at runtime
    GENERIC,
};

// returns how the += operator treats the given lvalue type and its dereferenced type
plus_equals_lvalue_e get_plus_equals_lvalue_kind(const QoreTypeInfo* ti, const QoreTypeInfo* check_ti) {
    if (QoreTypeInfo::isType(check_ti, NT_LIST)
        || QoreTypeInfo::isType(check_ti, NT_HASH)
        || QoreTypeInfo::isType(check_ti, NT_OBJECT)
        || QoreTypeInfo::isType(check_ti, NT_STRING)
        || QoreTypeInfo::isType(check_ti, NT_FLOAT)
        || QoreTypeInfo::isType(check_ti, NT_NUMBER)
        || QoreTypeInfo::isType(check_ti, NT_DATE)
        || QoreTypeInfo::isType(check_ti, NT_BINARY)) {
        return plus_equals_lvalue_e::KEEP;
    }
    // if the lhs type is not one of the above types, there are 2 possibilities: the lvalue has no value, in which
    // case it takes the value of the right side, or if it's anything else it's converted to an integer, so we just
    // check if it can be assigned an integer value
    if (QoreTypeInfo::returnsSingle(ti) && !QoreTypeInfo::equal(ti, timeoutTypeInfo)) {
        return plus_equals_lvalue_e::INT;
    }
    return plus_equals_lvalue_e::GENERIC;
}
}

int QorePlusEqualsOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    int err = 0;
    {
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_FOR_ASSIGNMENT);
        parse_context.typeInfo = nullptr;
        err = parse_init_value(left, parse_context);
        ti = parse_context.typeInfo;
        fh0.preserveFlags(PF_NARROWED_TYPE);
    }
    if (!err) {
        err = checkLValue(left, parse_context.pflag);
    }

    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }
    const QoreTypeInfo* rightTypeInfo = parse_context.typeInfo;

    // Dereference reference types before type checking so that
    // reference<list<string>> is recognized as a list type
    const QoreTypeInfo* check_ti = ti;
    if (QoreTypeInfo::isReference(ti)) {
        const QoreTypeInfo* deref_ti = QoreTypeInfo::getReferenceTarget(ti);
        if (deref_ti) {
            check_ti = deref_ti;
        }
    }

    // issue #5765 use getReturnComplexListOrNothing to handle or-nothing types (e.g., *list<int>)
    // so that list append operations on nested structures preserve type checking
    // Check if lvalue type returns a list (including *list types)
    const QoreTypeInfo* eti = QoreTypeInfo::getReturnComplexListOrNothing(check_ti);
    if (eti) {
        if (!QoreTypeInfo::parseReturns(rightTypeInfo, NT_LIST)) {
            if (!QoreTypeInfo::parseAccepts(eti, rightTypeInfo)) {
                QoreStringNode* edesc = new QoreStringNodeMaker(
                    "cannot append a value with type '%s' to a list with element type '%s'",
                    QoreTypeInfo::getName(rightTypeInfo), QoreTypeInfo::getName(eti));
                // Inspect the target itself; the right-hand expression can independently contain
                // a narrowed variable and set PF_NARROWED_TYPE.
                bool narrowed = qore_is_narrowed_lvalue(left);
                if (narrowed) {
                    edesc->concat("; the list's element type was inferred from an earlier assignment; "
                        "to use mixed types, include values of all needed types in that assignment, "
                        "or use list<auto!> to disable type narrowing for the variable; "
                        "note: %broken-narrowed-types will suppress this error but move it to runtime");
                }
                QoreDiagnosticMetadata metadata = qore_make_lvalue_type_diagnostic(left, eti, rightTypeInfo,
                    narrowed, "INCOMPATIBLE-LIST-ELEMENT-TYPE", "+=");
                qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", edesc,
                    metadata);
                if (!err) {
                    err = -1;
                }
            }
        }
    } else {
        // the parse-time checks are made on the plain type T of an or-nothing lvalue (*T): an operation that T
        // refuses can never succeed on *T; the error names the declared type, and the result type and the runtime
        // operation follow the declared type
        const QoreTypeInfo* plain_ti = get_or_nothing_base_type(ti);
        const QoreTypeInfo* plain_check_ti = get_or_nothing_base_type(check_ti);
        plus_equals_lvalue_e kind = get_plus_equals_lvalue_kind(ti, check_ti);
        plus_equals_lvalue_e check_kind = (plain_ti == ti && plain_check_ti == check_ti)
            ? kind
            : get_plus_equals_lvalue_kind(plain_ti, plain_check_ti);

        if (check_kind == plus_equals_lvalue_e::INT && check_lvalue_int(loc, ti, "+=") && !err) {
            err = -1;
        }
        // the warning is only given for the plain type: on an or-nothing lvalue, an operand such as NOTHING is
        // valid and expected
        if (kind == plus_equals_lvalue_e::KEEP && QoreTypeInfo::isType(ti, NT_STRING)
            && !QoreTypeInfo::canConvertToScalar(rightTypeInfo)) {
            QoreStringMaker desc("cannot mix string and %s types with the += operator",
                QoreTypeInfo::getName(rightTypeInfo));
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION,
                "INVALID-OPERATION", desc.c_str());
        }

        if (kind == plus_equals_lvalue_e::INT) {
            parse_context.typeInfo = ti = bigIntTypeInfo;
            // the analysis left by the right operand describes that operand, not the result
            qore_set_result_parse_analysis(parse_context, ti);
            val = makeSpecialization<QoreIntPlusEqualsOperatorNode>();
            if (val.hasNode()) {
                auto* parse_node = dynamic_cast<ParseNode*>(val.getInternalNode());
                if (parse_node) {
                    parse_node->setParseAnalysis(parse_context.analysis);
                }
            }
            return err;
        }
        if (kind == plus_equals_lvalue_e::GENERIC) {
            ti = nullptr;
        }
    }
    parse_context.typeInfo = ti;
    // the analysis left by the right operand describes that operand, not the result
    qore_set_result_parse_analysis(parse_context, ti);
    if (getSemanticPolicy().needsFilter()) {
        const QoreTypeInfo* result_type = ti;
        auto* replacement = static_cast<LexicalQorePlusEqualsOperatorNode*>(
            makeSpecialization<LexicalQorePlusEqualsOperatorNode>());
        replacement->ti = result_type;
        replacement->setParseAnalysis(parse_context.analysis);
        val = replacement;
    }
    return err;
}

QoreValue QorePlusEqualsOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return evalImplInternal<false>(needs_deref, xsink);
}

QoreValue QorePlusEqualsOperatorNode::evalLexical(bool& needs_deref, ExceptionSink* xsink) const {
    return evalImplInternal<true>(needs_deref, xsink);
}

template <bool Lexical>
QORE_INTERN_ALWAYS_INLINE QoreValue QorePlusEqualsOperatorNode::evalImplInternal(bool& needs_deref,
        ExceptionSink* xsink) const {
    // the operand is a statement of its own where the other execution modes evaluate it so
    QoreMutationOperandHolder new_right(left, right, false, xsink);
    if (*xsink) {
        return QoreValue();
    }

    // we have to ensure that the value is referenced before the assignment in case the lvalue
    // is the same value, so it can be copied in the LValueHelper constructor
    new_right.ensureReferencedValue();
    // the reference is handed over to the lvalue when += stores the value; the holder is declared before the
    // LValueHelper so that a reference that it does not take is released after the lvalue's locks
    ValueHolder right_holder(new_right.takeReferencedValue(), xsink);

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper v(left, xsink);
    if (!v) {
        return QoreValue();
    }

    // the implementation is shared with the other execution modes
    ValueHolder rv(xsink);
    if constexpr (Lexical) {
        rv = doPlusEqualsOnLValue(v, right_holder, xsink, getSemanticPolicy());
    } else {
        rv = doPlusEqualsOnLValue(v, right_holder, xsink);
    }
    if (*xsink || !ref_rv) {
        return QoreValue();
    }
    return rv.release();
}

/*
    QoreExtractOperatorNode.cpp

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
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_list_private.h"

#include <optional>

namespace {
class LexicalExtractOperatorNode : public QoreExtractOperatorNode {
public:
    using QoreExtractOperatorNode::QoreExtractOperatorNode;
    QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const override {
        return evalCommon<true>(needs_deref, xsink);
    }
};
}

QoreExtractOperatorNode* QoreExtractOperatorNode::create(LexicalSemanticPolicy policy, const QoreProgramLocation* loc,
        QoreValue lvalue, QoreValue offset, QoreValue length, QoreValue replacement) {
    auto* node = policy.needsFilter()
        ? static_cast<QoreExtractOperatorNode*>(new LexicalExtractOperatorNode(loc, lvalue, offset, length, replacement))
        : new QoreExtractOperatorNode(loc, lvalue, offset, length, replacement);
    ParseNode::bindSemanticPolicy(QoreValue(node), policy);
    return node;
}

QoreString QoreExtractOperatorNode::extract_str("extract operator expression");

// if del is true, then the returned QoreString * should be extractd, if false, then it must not be
QoreString *QoreExtractOperatorNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
   del = false;
   return &extract_str;
}

int QoreExtractOperatorNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
    qore_string_private::get(str)->concatAsciiName(extract_str);
    return 0;
}

int QoreExtractOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    // check lvalue expression
    assert(!parse_context.typeInfo);
    int err;
    {
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.setFlags(PF_FOR_ASSIGNMENT);
        err = parse_init_value(lvalue_exp, parse_context);
    }
    const QoreTypeInfo* expTypeInfo = parse_context.typeInfo;
    if (!err && checkLValue(lvalue_exp, parse_context.pflag)) {
        err = -1;
    }

    if (QoreTypeInfo::hasType(expTypeInfo)) {
        if (!QoreTypeInfo::parseAcceptsReturns(expTypeInfo, NT_LIST)
                && !QoreTypeInfo::parseAcceptsReturns(expTypeInfo, NT_BINARY)
                && !QoreTypeInfo::parseAcceptsReturns(expTypeInfo, NT_STRING)) {
            QoreStringNode *desc = new QoreStringNode("the lvalue expression (1st position) with the 'extract' " \
                "operator is ");
            QoreTypeInfo::getThisType(expTypeInfo, *desc);
            desc->sprintf(", therefore this operation is invalid and would throw an exception at run-time; the " \
                "'extract' operator only operates on lists, strings, and binary objects");
            qore_program_private::makeParseException(parse_context.pgm, *loc, "PARSE-TYPE-ERROR", desc);
            if (!err) {
                err = -1;
            }
        }
    }

    // check offset expression
    parse_context.typeInfo = nullptr;
    if (parse_init_value(offset_exp, parse_context) && !err) {
        err = -1;
    }
    if (!QoreTypeInfo::canConvertToScalar(parse_context.typeInfo)) {
        parse_context.typeInfo->doNonNumericWarning(loc, "the offset expression (2nd position) with the 'extract' " \
            "operator is ");
    }

    // check length expression, if any
    if (length_exp) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(length_exp, parse_context) && !err) {
            err = -1;
        }
        if (!QoreTypeInfo::canConvertToScalar(parse_context.typeInfo)) {
            parse_context.typeInfo->doNonNumericWarning(loc, "the length expression (3nd position) with the " \
                "'extract' operator is ");
        }
    }

    // check new value expression, if any
    if (new_exp) {
        parse_context.typeInfo = nullptr;
        if (parse_init_value(new_exp, parse_context) && !err) {
            err = -1;
        }
    }

    parse_context.typeInfo = returnTypeInfo = expTypeInfo;
    // the analysis left by the last operand initialized above describes that operand, not the result
    qore_set_result_parse_analysis(parse_context, returnTypeInfo);
    return err;
}

QoreValue QoreExtractOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return evalCommon<false>(needs_deref, xsink);
}

template <bool Lexical>
QORE_INTERN_ALWAYS_INLINE int QoreExtractOperatorNode::extractOn(LValueHelper& val, QoreValue converted,
        ValueEvalOptimizedRefHolder& eoffset, ValueEvalOptimizedRefHolder& elength, ValueEvalOptimizedRefHolder& exp,
        QoreValue& result, ExceptionSink* xsink) const {
    if (!converted.isNothing()) [[unlikely]] {
        val.saveTemp(converted);
    }

    // if value is not a list or string, throw exception
    qore_type_t vt = val.getType();

    if (vt == NT_NOTHING) {
        // see if the lvalue has a default type
        const QoreTypeInfo *typeInfo = val.getTypeInfo();
        if (typeInfo == softListTypeInfo || typeInfo == listTypeInfo || typeInfo == stringTypeInfo
            || typeInfo == softStringTypeInfo) {
            if (val.assign(QoreTypeInfo::getDefaultQoreValue(typeInfo)))
                return -1;
            vt = val.getType();
        }
    }

    if (vt != NT_LIST && vt != NT_WEAKREF_LIST && vt != NT_STRING && vt != NT_BINARY) {
        xsink->raiseException(*loc, "EXTRACT-ERROR", QoreValue(), "first (lvalue) argument to the extract "
            "operator is not a list, string, or binary object");
        return -1;
    }

    // the list changed: the lvalue's own list, or that of its weak reference, which the lvalue keeps (qore#7297)
    QoreListNode* vl = nullptr;
    if (vt == NT_LIST || vt == NT_WEAKREF_LIST) {
        vl = val.getListForChange();
        if (!vl) {
            return -1;
        }
        // through a weak reference: a value removed or inserted that needs a scan starts the scan of the lvalue's
        // object (see LValueHelper::changedScannedValue())
        val.changedScannedValue(QoreValue(vl));
        if (new_exp) {
            val.changedScannedValue(*exp);
        }
        vt = NT_LIST;
    } else if (val.ensureUnique("<extract operator>")) {
        // a string enum lvalue refuses the plain string that the operation leaves (qore#6195)
        return -1;
    }

    // a list freed under a weak reference cannot be changed, as by the other changes (qore#7199)
    if (vt == NT_LIST && qore_list_private::get(*val.getValue().get<QoreListNode>())->checkValid(xsink)) {
        return -1;
    }

    size_t offset = (size_t)eoffset->getAsBigInt();

#ifdef DEBUG
    if (vt == NT_LIST) {
        printd(5, "op_extract() val: %p (size: " QLLD ") offset: " QLLD "\n", vl, (int64)(vl->size()),
            (int64)offset);
    }
    else if (vt == NT_STRING) {
        QoreStringNode *vs = val.getValue().get<QoreStringNode>();
        printd(5, "op_extract() val: %p (strlen: " QLLD ") offset: " QLLD "\n", vs, (int64)(vs->strlen()),
            (int64)offset);
    }
#endif

    ReferenceHolder<AbstractQoreNode> rv(xsink);

    if (vt == NT_LIST) {
        if (!length_exp && !new_exp)
            rv = vl->extract(offset);
        else {
            size_t length = (size_t)elength->getAsBigInt();
            if (!new_exp) {
                rv = vl->extract(offset, length);
            } else {
                // an element type that can bind a reference: the values are checked with a probe before the list
                // is changed (qore#7391)
                std::optional<QoreReferenceBindProbe> probe;
                if (qore_list_private::get(*vl)->elementMayBindReference()) [[unlikely]] {
                    LexicalSemanticPolicy policy;
                    if constexpr (Lexical) {
                        policy = getSemanticPolicy();
                    }
                    val.enableBindRetry();
                    if (qore_container_store_probe(val, *exp, true, Lexical ? &policy : nullptr, probe)) {
                        return -1;
                    }
                }
                if constexpr (Lexical) {
                    rv = qore_list_private::replace(*vl, offset, length, *exp, true,
                        xsink, getSemanticPolicy());
                } else {
                    rv = qore_list_private::replace(*vl, offset, length, *exp, true, xsink);
                }
                if (probe) [[unlikely]] {
                    probe.reset();
                }
            }
        }
        // the elements taken out of a list reached through a weak reference (qore#7373)
        if (rv) {
            val.removedValueThroughWeak(QoreValue(*rv));
        }
    }
    else if (vt == NT_STRING) {
        // note: safe; val.ensureUnique() above makes a node of an inline short string
        QoreStringNode *vs = val.getValue().get<QoreStringNode>();
        if (!length_exp && !new_exp)
            rv = vs->extract(offset, xsink);
        else {
            size_t length = (size_t)elength->getAsBigInt();
            if (!new_exp)
                rv = vs->extract(offset, length, xsink);
            else
                rv = vs->extract(offset, length, *exp, xsink);
        }
    }
    else { // must be a binary
        BinaryNode* b = val.getValue().get<BinaryNode>();
        BinaryNode* bout = new BinaryNode;
        rv = bout;
        if (!length_exp && !new_exp)
            b->splice(offset, b->size(), bout);
        else {
            size_t length = (size_t)elength->getAsBigInt();
            if (!new_exp)
                b->splice(offset, length, bout);
            else {
                qore_type_t t = exp->getType();
                if (t == NT_BINARY) {
                    const BinaryNode* b1 = exp->get<const BinaryNode>();
                    b->splice(offset, length, b1->getPtr(), b1->size(), bout);
                }
                else {
                    QoreStringNodeValueHelper sv(*exp);
                    if (!sv->strlen())
                        b->splice(offset, length, bout);
                    else
                        b->splice(offset, length, sv->getBuffer(), sv->size(), bout);
                }
            }
        }
    }

    if (*xsink)
        return -1;

    // return value only if used and no exception occured
    if (ref_rv)
        result = rv.release();
    return 0;
}

template <bool Lexical>
QoreValue QoreExtractOperatorNode::evalCommon(bool& needs_deref, ExceptionSink* xsink) const {
    printd(5, "QoreExtractOperatorNode::splice() lvalue_exp: %s, offset_exp: %s, length_exp: %s, new_exp: %s, " \
        "isEvent: %d\n", lvalue_exp.getTypeName(), offset_exp.getTypeName(), length_exp.getTypeName(),
        new_exp.getTypeName(), xsink->isEvent());

    // evaluate arguments
    ValueEvalOptimizedRefHolder eoffset(offset_exp, xsink);
    if (*xsink)
        return QoreValue();

    ValueEvalOptimizedRefHolder elength(length_exp, xsink);
    if (*xsink)
        return QoreValue();

    ValueEvalOptimizedRefHolder exp(new_exp, xsink);
    if (*xsink)
        return QoreValue();

    QoreValue result;
    // the operation on the lvalue that the helper holds, inline on the normal path (extractOn()); made again in a retry
    // of the store, with the operands evaluated once: a probe that records references makes the store again before
    // the list is changed (qore#7391)
    struct Op {
        const QoreExtractOperatorNode* node;
        ValueEvalOptimizedRefHolder& eoffset;
        ValueEvalOptimizedRefHolder& elength;
        ValueEvalOptimizedRefHolder& exp;
        QoreValue& result;
        ExceptionSink* xsink;
        QORE_INTERN_ALWAYS_INLINE int operator()(LValueHelper& h, QoreValue converted) {
            return node->extractOn<Lexical>(h, converted, eoffset, elength, exp, result, xsink);
        }
    } op{this, eoffset, elength, exp, result, xsink};

    // the first run of the operation and an lvalue path whose keys are evaluated first are handled out of line
    // (qore#7391)
    if (store_mode.get() != QoreStorePathMode::Known) [[unlikely]] {
        qore_ast_store_slow(store_mode, lvalue_exp, xsink, false, op);
    } else {
        qore_ast_store<false>(lvalue_exp, xsink, false, nullptr, op);
    }
    return result;
}

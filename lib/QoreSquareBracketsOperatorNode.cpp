/*
    QoreSquareBracketsOperatorNode.cpp

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
#include "qore/intern/qore_program_private.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/qore_string_private.h"

QoreString QoreSquareBracketsOperatorNode::op_str("[] operator expression");

int QoreSquareBracketsOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!typeInfo);

    QoreParseAnalysis left_analysis;
    QoreParseAnalysis right_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(left, parse_context);
        left_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* lti = parse_context.typeInfo;
    {
        QoreParseContextFlagHelper fh0(parse_context);
        fh0.unsetFlags(PF_FOR_ASSIGNMENT);

        parse_context.typeInfo = nullptr;
        QoreParseContextAnalysisHelper ah(parse_context);
        if (parse_init_value(right, parse_context) && !err) {
            err = -1;
        }
        right_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* rti = parse_context.typeInfo;

    bool rti_is_list = QoreTypeInfo::isType(rti, NT_LIST);
    bool rti_can_be_list = rti_is_list ? true : QoreTypeInfo::parseReturns(rti, NT_LIST);

    if (QoreTypeInfo::hasType(lti)) {
        // if we are trying to convert to a list
        if (parse_context.pflag & PF_FOR_ASSIGNMENT) {
            // only throw a parse exception if parse exceptions are enabled
            if (!QoreTypeInfo::parseAcceptsReturns(lti, NT_LIST)) {
                if (getProgram()->getParseExceptionSink()) {
                    QoreStringNode* edesc = new QoreStringNode("cannot convert lvalue defined as ");
                    QoreTypeInfo::getThisType(lti, *edesc);
                    edesc->sprintf(" to a list using the '[]' operator in an assignment expression");
                    qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", edesc);
                }
                if (!err) {
                    err = -1;
                }
            }
        } else {
            if (QoreTypeInfo::isType(lti, NT_STRING)) {
                typeInfo = rti_is_list ? stringTypeInfo : stringOrNothingTypeInfo;
            } else if (QoreTypeInfo::isType(lti, NT_BINARY)) {
                if (rti_is_list) {
                    typeInfo = binaryTypeInfo;
                } else if (!rti_can_be_list) {
                    typeInfo = bigIntOrNothingTypeInfo;
                }
                // if the rhs may or may not be a list, then we can't determine the return type;
                // it could be int, binary, or NOTHING
            } else if (!QoreTypeInfo::parseAccepts(listTypeInfo, lti)
                     && !QoreTypeInfo::parseAccepts(stringTypeInfo, lti)
                     && !QoreTypeInfo::parseAccepts(binaryTypeInfo, lti)) {
                QoreStringNode* edesc = new QoreStringNode("left-hand side of the expression with the '[]' " \
                    "operator is ");
                QoreTypeInfo::getThisType(lti, *edesc);
                edesc->concat(" and so this expression will always return NOTHING; the '[]' operator only returns " \
                    "a value within the legal bounds of lists, strings, and binary objects");
                qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION,
                    "INVALID-OPERATION", edesc);
                typeInfo = nothingTypeInfo;
            }
        }
        if (!typeInfo && QoreTypeInfo::parseAccepts(listTypeInfo, lti)) {
            // issue #5765 use getReturnComplexListOrNothing to handle or-nothing types (e.g., *list<int>)
            // so that nested element access like l[0][1] preserves type information for parse-time checking
            const QoreTypeInfo* ti = QoreTypeInfo::getReturnComplexListOrNothing(lti);
            if (rti_can_be_list) {
                if (rti_is_list) {
                    if (ti) {
                        // with a slice, the return type is list<*type>
                        typeInfo = qore_get_complex_list_type(get_or_nothing_type_check(ti));
                    } else {
                        // otherwise we always get a list when making a slice of a list, but the element type is
                        // unknown
                        // if the lhs is not a list, then we get NOTHING
                        typeInfo = QoreTypeInfo::isType(lti, NT_LIST) ? listTypeInfo : listOrNothingTypeInfo;
                    }
                }
                // if we can be a list but also can be something else (ex NOTHING),
                // then we cannot predict the return type at parse time
            } else {
                if (ti) {
                    // issue #2115 when dereferencing a list, we could get also NOTHING when the requested element is
                    // not present
                    typeInfo = get_or_nothing_type_check(ti);
                }
            }
        }
    }

    // if the rhs cannot be a list, see if the rhs is a type that can be converted to an integer, if not raise an
    // invalid operation warning
    if (!rti_can_be_list) {
        if (!QoreTypeInfo::canConvertToScalar(rti)) {
            QoreStringNode* edesc = new QoreStringNode("the offset operand expression with the '[]' operator is ");
            QoreTypeInfo::getThisType(rti, *edesc);
            edesc->concat(" and so will always evaluate to zero");
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                edesc);
        } else if (right.isConstant() && right.getType() != NT_INT) {
            QoreStringNode* edesc = new QoreStringNode("the offset operand expression with the '[]' operator is a "
                "constant of ");
            QoreTypeInfo::getThisType(rti, *edesc);
            edesc->sprintf(" and will be automatically converted to an integer when evaluated; use a constant "
                "integer operand instead");
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                edesc);
        }
    }

    if (rti_is_list) {
        if (parse_context.pflag & PF_FOR_ASSIGNMENT) {
            parse_error(*loc, "a slice cannot be used on the left-hand side of an assignment expression");
            if (!err) {
                err = -1;
            }
        }

        // check element types in list
        switch (right.getType()) {
            case NT_PARSE_LIST:
                if (parseCheckValueTypes(right.get<const QoreParseListNode>()) && !err) {
                    err = -1;
                }
                break;
            case NT_LIST:
                if (parseCheckValueTypes(right.get<const QoreListNode>()) && !err) {
                    err = -1;
                }
                break;
            default:
                break;
                // no action
        }
    }

    // see if both arguments are constants, and the right side cannot be a list, then eval immediately and substitute
    // this node with the result
    // if the right side can be a list then we need to leave it unevaluated so we can support lazy evaluation with
    // functional operators
    if (!rti_can_be_list && right.isValue() && left.isValue()) {
        SimpleRefHolder<QoreSquareBracketsOperatorNode> del(this);
        ParseExceptionSink xsink;
        ValueEvalOptimizedRefHolder rv(this, *xsink);
        QoreValue result = rv.takeReferencedValue();
        // only use parse-time folding if we got a valid result
        // (constants may not be fully resolved at parse time, resulting in NOTHING)
        if (!result.isNothing() || **xsink) {
            val = result;
            parse_context.typeInfo = typeInfo = val.getFullTypeInfo();
            parse_context.analysis.clear();
            parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
            parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
            if (!val.isNothing()) {
                parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
            }
            parse_context.analysis.known_type = parse_context.typeInfo;
            return **xsink ? -1 : 0;
        }
        // constants not resolved - skip parse-time folding, let runtime handle it
        del.release();
    }

    parse_context.typeInfo = typeInfo;
    parse_context.analysis.clear();
    if (parse_context.typeInfo) {
        parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
        parse_context.analysis.known_type = parse_context.typeInfo;
        if (QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_NOTHING) == QTI_NOT_EQUAL) {
            parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
        }
    }
    if (left_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)
        && right_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

int QoreSquareBracketsOperatorNode::parseCheckValueTypes(const QoreParseListNode* pln) {
    const type_vec_t& vtypes = pln->getValueTypes();
    const QoreParseListNode::nvec_t& vl = pln->getValues();
    int err = 0;
    for (unsigned i = 0; i < vtypes.size(); ++i) {
        if (QoreTypeInfo::canConvertToScalar(vtypes[i])) {
            continue;
        }
        // see if the value is a range
        if (vl[i].getType() == NT_OPERATOR && dynamic_cast<const QoreRangeOperatorNode*>(vl[i].getInternalNode())) {
            if (!rhs_list_range) {
                rhs_list_range = true;
            }
            continue;
        }
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot make a slice with offset %d/%d of type '%s'; need a type " \
            "convertible to an integer or a range", i, (int)vtypes.size(), QoreTypeInfo::getName(vtypes[i]));
        if (!err) {
            err = -1;
        }
    }
    return err;
}

int QoreSquareBracketsOperatorNode::parseCheckValueTypes(const QoreListNode* ln) {
    ConstListIterator i(ln);
    int err = 0;
    while (i.next()) {
        QoreValue v = i.getValue();
        const QoreTypeInfo* vti = v.getTypeInfo();
        if (QoreTypeInfo::canConvertToScalar(vti)) {
            // check if we have a cosntant value that's not an int
            if (v.isConstant() && v.getType() != NT_INT) {
                QoreStringNode* edesc = new QoreStringNodeMaker("the offset operand expression with the '[]' "
                    "operator in list element %d (starting with 1) is a constant of ", i.index() + 1);
                QoreTypeInfo::getThisType(vti, *edesc);
                edesc->sprintf(" and will be automatically converted to an integer when evaluated; use a constant "
                    "integer operand instead");
                qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                    edesc);
            }
            continue;
        }
        parseException(*loc, "PARSE-TYPE-ERROR", "cannot make a slice with offset %lu/%lu of type '%s'; need a " \
            "type convertible to an integer or a range", i.index(), i.max(), QoreTypeInfo::getName(vti));
        if (!err) {
            err = -1;
        }
    }
    return err;
}

QoreValue QoreSquareBracketsOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lh(left, xsink);
    if (*xsink)
        return QoreValue();

    // do not evalute RHS if it's a list with ranges
    if (rhs_list_range)
        return doSquareBracketsListRange(*lh, right.get<const QoreParseListNode>(), xsink);

    ValueEvalOptimizedRefHolder rh(right, xsink);
    if (*xsink)
        return QoreValue();

    return doSquareBrackets(*lh, *rh, true, xsink);
}

QoreValue QoreSquareBracketsOperatorNode::doSquareBracketsListRange(const QoreValue l, const QoreParseListNode* pln,
        ExceptionSink* xsink) {
    switch (l.getType()) {
        case NT_LIST: {
            // calculate the runtime element type if possible
            const QoreTypeInfo* vtype = nullptr;
            // try to find a common value type, if any
            bool vcommon = false;
            ReferenceHolder<QoreListNode> ret(new QoreListNode(autoTypeInfo), xsink);
            const QoreParseListNode::nvec_t& vl = pln->getValues();
            for (unsigned i = 0; i < vl.size(); ++i) {
                ValueEvalOptimizedRefHolder rh(vl[i], xsink);
                if (*xsink)
                    return QoreValue();
                bool is_range = (vl[i].getType() == NT_OPERATOR
                    && dynamic_cast<const QoreRangeOperatorNode*>(vl[i].getInternalNode()));
                ValueHolder entry(doSquareBrackets(l, *rh, is_range, xsink), xsink);
                if (*xsink)
                    return QoreValue();
                if (is_range) {
                    // concatenate each element of the range to the list and manage the common element type
                    assert(entry->getType() == NT_LIST);
                    ConstListIterator li(entry->get<const QoreListNode>());
                    while (li.next()) {
                        QoreValue n = li.getValue();
                        if (!i) {
                            vtype = n.getTypeInfo();
                            vcommon = true;
                        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, n.getTypeInfo()))
                            vcommon = false;

                        ret->push(n.refSelf(), xsink);
                    }
                } else {
                    if (!i) {
                        vtype = entry->getTypeInfo();
                        vcommon = true;
                    } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, entry->getTypeInfo()))
                        vcommon = false;
                    ret->push(entry.release(), xsink);
                }
            }

            // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
            if (vtype && vtype != anyTypeInfo) {
                qore_list_private::get(**ret)->complexTypeInfo = qore_get_complex_list_type(vtype);
            }

            return ret.release();
        }
        case NT_STRING: {
            SimpleRefHolder<QoreStringNode> ret(new QoreStringNode);
            for (auto& i : pln->getValues()) {
                ValueEvalOptimizedRefHolder rh(i, xsink);
                if (*xsink)
                    return QoreValue();
                bool is_range = (i.getType() == NT_OPERATOR
                    && dynamic_cast<const QoreRangeOperatorNode*>(i.getInternalNode()));
                if (doString(ret, l, *rh, is_range, xsink))
                    return QoreValue();
            }
            return ret.release();
        }
        case NT_BINARY: {
            SimpleRefHolder<BinaryNode> bin(new BinaryNode);
            for (auto& i : pln->getValues()) {
                ValueEvalOptimizedRefHolder rh(i, xsink);
                if (*xsink)
                    return QoreValue();
                bool is_range = (i.getType() == NT_OPERATOR
                    && dynamic_cast<const QoreRangeOperatorNode*>(i.getInternalNode()));
                if (doBinary(bin, l, *rh, is_range, xsink))
                    return QoreValue();
            }
            return bin.release();
        }
        default:
            break;
    }
    return QoreValue();
}

int QoreSquareBracketsOperatorNode::doString(SimpleRefHolder<QoreStringNode>& ret, const QoreValue l,
        const QoreValue r, bool list_ok, ExceptionSink* xsink) {
    ValueHolder entry(doSquareBrackets(l, r, list_ok, xsink), xsink);
    if (*xsink)
        return -1;
    if (!entry->isNothing())
        qore_string_private::get(*ret)->concat(entry->get<QoreStringNode>());
    return 0;
}

int QoreSquareBracketsOperatorNode::doBinary(SimpleRefHolder<BinaryNode>& bin, const QoreValue l, const QoreValue r,
        bool list_ok, ExceptionSink* xsink) {
    ValueHolder entry(doSquareBrackets(l, r, list_ok, xsink), xsink);
    if (*xsink)
        return -1;
    switch (entry->getType()) {
        case NT_INT: {
            unsigned char c = (unsigned char)entry->getAsBigInt();
            bin->append(&c, 1);
            break;
        }
        case NT_BINARY: {
            bin->append(entry->get<BinaryNode>());
            break;
        }
        default:
            assert(entry->getType() == NT_NOTHING);
            break;
    }
    return 0;
}

QoreValue QoreSquareBracketsOperatorNode::doSquareBrackets(const QoreValue l, const QoreValue r, bool list_ok,
        ExceptionSink* xsink) {
    qore_type_t left_type = l.getType();
    qore_type_t right_type = r.getType();

    if (right_type == NT_LIST && list_ok) {
        ConstListIterator it(r.get<const QoreListNode>());
        switch (left_type) {
            case NT_LIST: {
                // calculate the runtime element type if possible
                const QoreTypeInfo* vtype = nullptr;
                // try to find a common value type, if any
                bool vcommon = false;
                ReferenceHolder<QoreListNode> ret(new QoreListNode(autoTypeInfo), xsink);
                while (it.next()) {
                    ValueHolder entry(doSquareBrackets(l, it.getValue(), false, xsink), xsink);
                    if (*xsink)
                        return QoreValue();

                    assert(entry->getType() != NT_LIST);

                    if (!it.index()) {
                        vtype = entry->getTypeInfo();
                        vcommon = true;
                    } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, entry->getTypeInfo()))
                        vcommon = false;

                    ret->push(entry.release(), xsink);

                    printd(5, "QoreSquareBracketsOperatorNode::doSquareBrackets() i: %d: vc: %d vtype: '%s' " \
                        "et: '%s'\n", it.index(), (int)vcommon, QoreTypeInfo::getName(vtype),
                        QoreTypeInfo::getName(entry->getTypeInfo()));
                }

                ValueHolder rv(ret.release(), xsink);

                // issue #2791: when performing type folding, do not set to type "any" but rather use "auto"
                if (!vtype || vtype == anyTypeInfo) {
                    vtype = autoTypeInfo;
                }

                const QoreTypeInfo* ti = qore_get_complex_list_type(vtype);
                qore_list_private::get(*rv->get<QoreListNode>())->complexTypeInfo = ti;
                if (QoreTypeInfo::hasType(vtype)) {
                    QoreTypeInfo::acceptAssignment(ti, "<type folding>", *rv, xsink);
                }

                //printd(5, "QoreSquareBracketsOperatorNode::doSquareBrackets() vc: %d vtype: '%s' ti: '%s'\n",
                //  (int)vcommon, QoreTypeInfo::getName(vtype), QoreTypeInfo::getName(ti));

                return rv.release();
            }
            case NT_STRING: {
                SimpleRefHolder<QoreStringNode> ret(new QoreStringNode);
                while (it.next()) {
                    if (doString(ret, l, it.getValue(), false, xsink))
                        return QoreValue();
                }
                return ret.release();
            }
            case NT_BINARY: {
                SimpleRefHolder<BinaryNode> bin(new BinaryNode);
                while (it.next()) {
                    if (doBinary(bin, l, it.getValue(), false, xsink))
                        return QoreValue();
                }
                return bin.release();
            }
            default:
                return QoreValue();
        }
    }

    // rhs not a list, handled as an integer offset
    int64 offset = r.getAsBigInt();
    switch (left_type) {
        case NT_LIST: {
            return l.get<const QoreListNode>()->getReferencedEntry(offset);
        }

        case NT_STRING:
            return l.get<const QoreStringNode>()->substr(offset, 1, xsink);

        case NT_BINARY: {
            const BinaryNode* b = l.get<const BinaryNode>();
            if (offset < 0 || (size_t)offset >= b->size())
                return QoreValue();
            return (int64)(((unsigned char*)b->getPtr())[offset]);
        }
    }

    return QoreValue();
}

FunctionalOperatorInterface* QoreSquareBracketsOperatorNode::getFunctionalIteratorImpl(
        FunctionalValueType& value_type, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder lhs(left, xsink);
    if (*xsink)
        return nullptr;

    ValueEvalOptimizedRefHolder rhs(xsink);

    // do not evaluate the RHS if the RHS is a list with ranges
    if (rhs_list_range) {
        if (lhs->getType() == NT_LIST) {
            value_type = list;
            return new QoreFunctionalSquareBracketsComplexOperator(lhs, right.get<const QoreParseListNode>(), xsink);
        }
    } else {
        if (rhs.eval(right))
            return nullptr;

        // we only support functional iteration when the lhd is a list and the rhs is a list
        if (lhs->getType() == NT_LIST && rhs->getType() == NT_LIST) {
            ValueEvalOptimizedRefHolder rhs(right, xsink);
            if (*xsink)
                return nullptr;

            value_type = list;
            return new QoreFunctionalSquareBracketsOperator(lhs, rhs, xsink);
        }
    }

    ValueHolder res(xsink);

    if (rhs_list_range)
        res = doSquareBracketsListRange(*lhs, right.get<const QoreParseListNode>(), xsink);
    else
        res = doSquareBrackets(*lhs, *rhs, true, xsink);

    if (*xsink)
        return nullptr;
    if (res->isNothing()) {
        value_type = nothing;
        return nullptr;
    } else if (res->getType() == NT_LIST) {
        value_type = list;
        return new QoreFunctionalListOperator(true, res.release().get<QoreListNode>(), xsink);
    }
    value_type = single;
    return new QoreFunctionalSingleValueOperator(res.release(), xsink);
}

bool QoreFunctionalSquareBracketsOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if ((size_t)++offset == rightList->size())  // do a step in the top-level list
        return true;

    val.setValue(QoreSquareBracketsOperatorNode::doSquareBrackets(*leftValue, rightList->retrieveEntry(offset), false,
        xsink), true);

    return false;
}

const QoreTypeInfo* QoreFunctionalSquareBracketsOperator::getValueTypeImpl() const {
    switch (leftValue->getType()) {
        case NT_LIST: return leftValue->get<const QoreListNode>()->getValueTypeInfo();
        case NT_STRING: return stringTypeInfo;
        case NT_BINARY: return binaryTypeInfo;
    }
    return nothingTypeInfo;
}

bool QoreFunctionalSquareBracketsComplexOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (!rangeIter) {                               // the current position is not inside a subrange
        if ((size_t)++offset == rightParseList->size())  // do a step in the top-level list
            return true;

        // if the element on the new current top-level position (offset) is a range then get its iterator
        const QoreRangeOperatorNode* range;
        QoreValue n = rightParseList->get(offset);
        if (n.getType() == NT_OPERATOR && (range = dynamic_cast<const QoreRangeOperatorNode*>(n.getInternalNode()))) {
            FunctionalOperator::FunctionalValueType value_type;
            rangeIter = std::unique_ptr<QoreFunctionalRangeOperator>((QoreFunctionalRangeOperator*)(
                range->getFunctionalIterator(value_type, xsink)
            ));
        }

        if (*xsink)
            return false;
    }

    if (rangeIter) {                                // the current position is inside a subrange
        ValueOptionalRefHolder rangeVal(xsink);
        if (rangeIter->getNext(rangeVal, xsink)) {  // use the inner range iterator to get next ...
            rangeIter.reset();                      // ... if there's no next inner range element then reset the inner
                                                    // iterator ...
            return getNextImpl(val, xsink);         // ... and get the next top-level element
        } else  // set the value using the index from the inner subrange
            val.setValue(QoreSquareBracketsOperatorNode::doSquareBrackets(*leftValue, *rangeVal, false, xsink), true);
    } else {
        ValueEvalOptimizedRefHolder rh(rightParseList->get(offset), xsink);
        if (*xsink)
            return false;

        // set the value using the top-level index
        val.setValue(QoreSquareBracketsOperatorNode::doSquareBrackets(*leftValue, *rh, false, xsink), true);
    }

    return false;
}

const QoreTypeInfo* QoreFunctionalSquareBracketsComplexOperator::getValueTypeImpl() const {
    switch (leftValue->getType()) {
        case NT_LIST: return leftValue->get<const QoreListNode>()->getValueTypeInfo();
        case NT_STRING: return stringTypeInfo;
        case NT_BINARY: return binaryTypeInfo;
    }
    return nothingTypeInfo;
}

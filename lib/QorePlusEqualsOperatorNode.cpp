/*
    QorePlusEqualsOperatorNode.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreObjectIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/ParseNode.h"

QoreString QorePlusEqualsOperatorNode::op_str("+= operator expression");

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
    }
    if (!err) {
        err = checkLValue(left, parse_context.pflag);
    }

    parse_context.typeInfo = nullptr;
    if (parse_init_value(right, parse_context) && !err) {
        err = -1;
    }
    const QoreTypeInfo* rightTypeInfo = parse_context.typeInfo;

    // issue #5765 use getReturnComplexListOrNothing to handle or-nothing types (e.g., *list<int>)
    // so that list append operations on nested structures preserve type checking
    // Check if lvalue type returns a list (including *list types)
    const QoreTypeInfo* eti = QoreTypeInfo::getReturnComplexListOrNothing(ti);
    if (eti) {
        if (!QoreTypeInfo::parseReturns(rightTypeInfo, NT_LIST)) {
            if (!QoreTypeInfo::parseAccepts(eti, rightTypeInfo)) {
                QoreStringNode* edesc = new QoreStringNodeMaker(
                    "cannot append a value with type '%s' to a list with element type '%s'",
                    QoreTypeInfo::getName(rightTypeInfo), QoreTypeInfo::getName(eti));
                // Add context about type narrowing
                edesc->concat("; the list's element type was inferred from the initial value; "
                    "to use mixed types, include values of all needed types in the initial assignment, "
                    "or use list<auto!> to disable type narrowing for the variable; "
                    "note: %broken-narrowed-types will suppress this error but move it to runtime");
                qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", edesc);
                if (!err) {
                    err = -1;
                }
            }
        }
    } else if (!QoreTypeInfo::isType(ti, NT_LIST)
        && !QoreTypeInfo::isType(ti, NT_HASH)
        && !QoreTypeInfo::isType(ti, NT_OBJECT)
        && !QoreTypeInfo::isType(ti, NT_STRING)
        && !QoreTypeInfo::isType(ti, NT_FLOAT)
        && !QoreTypeInfo::isType(ti, NT_NUMBER)
        && !QoreTypeInfo::isType(ti, NT_DATE)
        && !QoreTypeInfo::isType(ti, NT_BINARY)) {
        // if the lhs type is not one of the above types,
        // there are 2 possibilities: the lvalue has no value, in which
        // case it takes the value of the right side, or if it's anything else it's
        // converted to an integer, so we just check if it can be assigned an
        // integer value below, this is enough
        if (QoreTypeInfo::returnsSingle(ti) && !QoreTypeInfo::equal(ti, timeoutTypeInfo)) {
            if (check_lvalue_int(loc, ti, "+=") && !err) {
                err = -1;
            }
            parse_context.typeInfo = ti = bigIntTypeInfo;
            val = makeSpecialization<QoreIntPlusEqualsOperatorNode>();
            if (val.hasNode()) {
                auto* parse_node = dynamic_cast<ParseNode*>(val.getInternalNode());
                if (parse_node) {
                    parse_node->setParseAnalysis(parse_context.analysis);
                }
            }
            return err;
        } else {
            ti = nullptr;
        }
    } else if (QoreTypeInfo::isType(ti, NT_STRING) && !QoreTypeInfo::canConvertToScalar(rightTypeInfo)) {
        QoreStringMaker desc("cannot mix string and %s types with the += operator",
            QoreTypeInfo::getName(rightTypeInfo));
        qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION,
            "INVALID-OPERATION", desc.c_str());
    }
    parse_context.typeInfo = ti;
    return err;
}

QoreValue QorePlusEqualsOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder new_right(right, xsink);
    if (*xsink)
        return QoreValue();

    // we have to ensure that the value is referenced before the assignment in case the lvalue
    // is the same value, so it can be copied in the LValueHelper constructor
    new_right.ensureReferencedValue();

    // issue #4055: values requiring dereferencing must be dereferenced outside the lock
    SafeDerefHelper sdh(xsink);

    // get ptr to current value (lvalue is locked for the scope of the LValueHelper object)
    LValueHelper v(left, xsink);
    if (!v)
        return QoreValue();

    // dereferences happen in each section so that the
    // already referenced value can be passed to list->push()
    // if necessary
    // do list plus-equals if left-hand side is a list
    qore_type_t vtype = v.getType();

    if (vtype == NT_NOTHING) {
        // see if the lvalue has a default type
        const QoreTypeInfo *typeInfo = v.getTypeInfo();
        if (QoreTypeInfo::hasDefaultValue(typeInfo)) {
            if (v.assign(QoreTypeInfo::getDefaultQoreValue(typeInfo), "<lvalue for += operator>"))
                return QoreValue();
            vtype = v.getType();
        } else if (QoreTypeInfo::isListType(typeInfo)) {
            // issue #3586: automatically promote lvalue to correctly-typed empty list for "*list..." types
            if (v.assign(new QoreListNode(QoreTypeInfo::getReturnComplexListOrNothing(typeInfo)),
                "<lvalue for += operator>")) {
                return QoreValue();
            }
            vtype = v.getType();
        } else if (new_right->isNothing()) {
            return QoreValue();
        } else if (QoreTypeInfo::isHashType(typeInfo)) {
            // issue #4133: automatically promote lvalue to correctly-typed empty hash for "*hash..." types
            if (v.assign(new QoreHashNode(QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo)),
                "<lvalue for += operator>")) {
                return QoreValue();
            }
            vtype = v.getType();
        } else {
            // assign rhs to lhs (take reference for plusequals)
            if (v.assign(new_right.takeReferencedValue(), "<lvalue for += operator>"))
                return QoreValue();

            // v has been assigned to a value by this point
            // reference return value
            return ref_rv ? v.getReferencedValue() : QoreValue();
        }
    }

    if (vtype == NT_LIST) {
        v.ensureUnique(); // no exception possible here
        QoreListNode* l = v.getValue().get<QoreListNode>();
        if (new_right->getType() == NT_LIST)
            l->merge(reinterpret_cast<const QoreListNode*>(new_right->getInternalNode()), xsink);
        else
            l->push(new_right.takeReferencedValue(), xsink);
    } else if (vtype == NT_HASH) {
        // do hash plus-equals if left side is a hash
        if (new_right->getType() == NT_HASH) {
            v.ensureUnique();
            qore_hash_private::get(*v.getValue().get<QoreHashNode>())
                ->merge(*qore_hash_private::get(*new_right->get<const QoreHashNode>()), sdh, xsink);
        } else if (new_right->getType() == NT_OBJECT) {
            v.ensureUnique();
            qore_object_private::get(*new_right->get<QoreObject>())->mergeDataToHash(v.getValue().get<QoreHashNode>(),
                sdh, xsink);
        }
    } else if (vtype == NT_OBJECT) {
        // do hash/object plus-equals if left side is an object
        QoreObject* o = v.getValue().get<QoreObject>();
        qore_object_private::get(*o)->plusEquals(new_right->getInternalNode(), v.getAutoVLock(), sdh, xsink);
    } else if (vtype == NT_STRING) {
        // do string plus-equals if left-hand side is a string
        if (!new_right->isNullOrNothing()) {
            QoreStringValueHelper str(*new_right);

            v.ensureUnique();
            QoreStringNode* vs = v.getValue().get<QoreStringNode>();
            vs->concat(*str, xsink);
        }
    } else if (vtype == NT_NUMBER) {
        v.plusEqualsNumber(*new_right, "<+= operator>");
    } else if (vtype == NT_FLOAT) {
        v.plusEqualsFloat(new_right->getAsFloat());
    } else if (vtype == NT_DATE) {
        if (!new_right->isNullOrNothing()) {
            // gets a relative date/time value from the value
            DateTime date(*new_right);
            v.assign(v.getValue().get<DateTimeNode>()->add(date), "<lvalue for += operator>");
        }
    } else if (vtype == NT_BINARY) {
        if (!new_right->isNullOrNothing()) {
            v.ensureUnique();
            BinaryNode *b = v.getValue().get<BinaryNode>();
            if (new_right->getType() == NT_BINARY) {
                const BinaryNode *arg = new_right->get<const BinaryNode>();
                b->append(arg);
            } else {
                QoreStringNodeValueHelper str(*new_right);
                if (str->strlen()) {
                    b->append(str->getBuffer(), str->strlen());
                }
            }
        }
    } else {
        // issue #3157: if the lvalue is a timeout, then convert any date/time value as if it were a timeout too
        if (new_right->getType() == NT_DATE && QoreTypeInfo::equal(v.getTypeInfo(), timeoutTypeInfo)) {
            int64 ms = new_right->get<const DateTimeNode>()->getRelativeMilliseconds();
            // do minus-equals with milliseconds
            return v.plusEqualsBigInt(ms);
        }

        // do integer plus-equals
        v.plusEqualsBigInt(new_right->getAsBigInt());
    }
    if (*xsink) {
        return QoreValue();
    }

    // v has been assigned to a value by this point
    // reference return value
    return ref_rv ? v.getReferencedValue() : QoreValue();
}

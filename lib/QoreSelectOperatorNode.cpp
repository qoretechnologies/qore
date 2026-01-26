/*
    QoreSelectOperatorNode.cpp

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
#include "qore/intern/FunctionalOperator.h"
#include "qore/intern/FunctionalOperatorInterface.h"
#include "qore/intern/qore_list_private.h"

#include <memory>

QoreString QoreSelectOperatorNode::select_str("select operator expression");

// if del is true, then the returned QoreString * should be selectd, if false, then it must not be
QoreString* QoreSelectOperatorNode::getAsString(bool& del, int foff, ExceptionSink *xsink) const {
    del = false;
    return &select_str;
}

int QoreSelectOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    qore_string_private::get(str)->concat(&select_str);
    return 0;
}

int QoreSelectOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    // check iterator expression
    int err = parse_init_value(left, parse_context);
    const QoreTypeInfo* iteratorTypeInfo = parse_context.typeInfo;

    // get element type for the iterator (works for both list<T> and iterator classes)
    const QoreTypeInfo* elementTypeInfo =
        QoreTypeInfo::getImplicitArgTypeForIterator(left, iteratorTypeInfo);

    parse_context.typeInfo = nullptr;
    {
        // set implicit argv arg type
        ParseImplicitArgTypeHelper pia(elementTypeInfo);

        if (parse_init_value(right, parse_context) && !err) {
            err = -1;
        }
    }

    // use lazy evaluation if the iterator expression supports it
    iterator_func = dynamic_cast<FunctionalOperator*>(left.getInternalNode());

    // if iterator is a list or an iterator, then the return type is a list, otherwise it's the return type of the
    // iterated expression
    if (QoreTypeInfo::hasType(iteratorTypeInfo)) {
        if (QoreTypeInfo::isType(iteratorTypeInfo, NT_NOTHING)) {
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                "the iterator expression with the select operator (the first expression) has no value (NOTHING) " \
                "and therefore this expression will also return no value; update the expression to return a value " \
                "or use '%%disable-warning invalid-operation' in your code to avoid seeing this warning in the " \
                "future");
            parse_context.typeInfo = nothingTypeInfo;
        } else if (QoreTypeInfo::isType(iteratorTypeInfo, NT_LIST)) {
            parse_context.typeInfo = listTypeInfo;
        } else {
            const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(iteratorTypeInfo);
            if (qc && qore_class_private::parseCheckCompatibleClass(qc, QC_ABSTRACTITERATOR)) {
                parse_context.typeInfo = listTypeInfo;
            } else if ((QoreTypeInfo::parseReturns(iteratorTypeInfo, NT_LIST) == QTI_NOT_EQUAL)
                && (QoreTypeInfo::parseReturns(iteratorTypeInfo, QC_ABSTRACTITERATOR) == QTI_NOT_EQUAL)) {
                parse_context.typeInfo = iteratorTypeInfo;
            } else {
                parse_context.typeInfo = nullptr;
            }
        }
    } else {
        parse_context.typeInfo = nullptr;
    }

    // If we have element type info, create a properly typed list
    if (parse_context.typeInfo == listTypeInfo && elementTypeInfo) {
        parse_context.typeInfo = qore_get_complex_list_type(elementTypeInfo);
    }

    return err;
}

QoreValue QoreSelectOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    FunctionalValueType value_type;
    std::unique_ptr<FunctionalOperatorInterface> f(getFunctionalIterator(value_type, xsink));
    if (*xsink || value_type == nothing)
        return QoreValue();

    ReferenceHolder<QoreListNode> rv(ref_rv && (value_type != single) ? new QoreListNode(f->getValueType()) : nullptr,
        xsink);

    // calculate the runtime element type if possible
    const QoreTypeInfo* vtype = nullptr;
    bool vcommon = false;

    while (true) {
        ValueOptionalRefHolder iv(xsink);
        if (f->getNext(iv, xsink)) {
            break;
        }

        if (*xsink) {
            return QoreValue();
        }

        if (value_type == single) {
            return ref_rv ? iv.takeReferencedValue() : QoreValue();
        }

        if (ref_rv) {
            QoreValue val = iv.takeReferencedValue();
            if (rv->empty()) {
                vtype = val.getTypeInfo();
                vcommon = true;
            } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, val.getTypeInfo())) {
                vcommon = false;
            }

            rv->push(val, xsink);
        }
    }

    if (rv && vcommon && QoreTypeInfo::hasType(vtype)) {
        qore_list_private::get(**rv)->complexTypeInfo = qore_get_complex_list_type(vtype);
    }

    return rv.release();
}

FunctionalOperatorInterface* QoreSelectOperatorNode::getFunctionalIteratorImpl(FunctionalValueType& value_type,
        ExceptionSink* xsink) const {
    if (iterator_func) {
        std::unique_ptr<FunctionalOperatorInterface> f(iterator_func->getFunctionalIterator(value_type, xsink));
        if (*xsink || value_type == nothing)
            return 0;
        return new QoreFunctionalSelectOperator(this, f.release());
    }

    ValueEvalOptimizedRefHolder marg(left, xsink);
    if (*xsink)
        return 0;

    qore_type_t t = marg->getType();
    if (t != NT_LIST) {
        if (t == NT_OBJECT) {
            AbstractIteratorHelper h(xsink, "select operator",
                const_cast<QoreObject*>(marg->get<const QoreObject>()));
            if (*xsink)
                return 0;
            if (h) {
                bool temp = marg.isTemp();
                marg.clearTemp();
                value_type = list;
                return new QoreFunctionalSelectIteratorOperator(this, temp, h, xsink);
            }
        }
        if (t == NT_NOTHING) {
            value_type = nothing;
            return 0;
        }

        value_type = single;
        return new QoreFunctionalSelectSingleValueOperator(this, marg.getReferencedValue(), xsink);
    }

    value_type = list;
    return new QoreFunctionalSelectListOperator(this, marg.takeReferencedNode<QoreListNode>(), xsink);
}

bool QoreFunctionalSelectListOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    while (true) {
        if (!next())
            return true;

        // set offset in thread-local data for "$#"
        ImplicitElementHelper eh(index());
        SingleArgvContextHelper argv_helper(getReferencedValue(), xsink);

        // check if value can be selected
        ValueEvalOptimizedRefHolder result(select->right, xsink);
        if (*xsink)
            return false;
        if (!result->getAsBool())
            continue;

        val.setValue(getReferencedValue(), true);
        break;
    }
    return false;
}

bool QoreFunctionalSelectSingleValueOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (done)
        return true;

    done = true;

    // setup the implicit argument
    SingleArgvContextHelper argv_helper(v.refSelf(), xsink);

    // check if value can be selected
    ValueEvalOptimizedRefHolder result(select->right, xsink);
    if (*xsink)
        return false;
    if (!result->getAsBool())
        return true;

    val.setValue(v, true);
    v.clear();
    return false;
}

bool QoreFunctionalSelectIteratorOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    while (true) {
        bool b = h.next(xsink);
        if (!b)
            return true;
        if (*xsink)
            return false;

        // set offset in thread-local data for "$#"
        ImplicitElementHelper eh(index++);

        // get the current value
        ValueHolder iv(h.getValue(xsink), xsink);
        if (*xsink)
            return false;
        // setup the implicit argument
        SingleArgvContextHelper argv_helper(iv->refSelf(), xsink);
        // check if value can be selected
        ValueEvalOptimizedRefHolder result(select->right, xsink);
        if (*xsink)
            return false;
        if (!result->getAsBool())
            continue;

        val.setValue(iv.release(), true);
        break;
    }
    return false;
}

bool QoreFunctionalSelectOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    while (true) {
        ValueOptionalRefHolder iv(xsink);
        if (f->getNext(iv, xsink))
            return true;
        if (*xsink)
            return false;

        // set offset in thread-local data for "$#"
        ImplicitElementHelper eh(index++);

        // setup the implicit argument
        SingleArgvContextHelper argv_helper(iv->refSelf(), xsink);
        // check if value can be selected
        ValueEvalOptimizedRefHolder result(select->right, xsink);
        if (*xsink)
            return false;
        if (!result->getAsBool())
            continue;

        iv.ensureReferencedValue();
        val.takeValueFrom(iv);
        break;
    }
    return false;
}

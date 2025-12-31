/*
    QoreFoldlOperatorNode.cpp

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

#include "qore/intern/qore_program_private.h"

#include <memory>

QoreString QoreFoldlOperatorNode::foldl_str("foldl operator expression");
QoreString QoreFoldrOperatorNode::foldr_str("foldr operator expression");

// if del is true, then the returned QoreString * should be foldld, if false, then it must not be
QoreString *QoreFoldlOperatorNode::getAsString(bool &del, int foff, ExceptionSink *xsink) const {
    del = false;
    return &foldl_str;
}

int QoreFoldlOperatorNode::getAsString(QoreString &str, int foff, ExceptionSink *xsink) const {
    str.concat(&foldl_str);
    return 0;
}

int QoreFoldlOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    // turn off "return value ignored" flags
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    assert(!parse_context.typeInfo);
    int err = parse_init_value(right, parse_context);
    const QoreTypeInfo* iteratorTypeInfo = parse_context.typeInfo;

    // check iterated expression
    const QoreTypeInfo* expTypeInfo;
    {
        // set implicit argv arg type
        // FIXME: only works if the result of the fold operation results in the exact same type as the argument type
        const QoreTypeInfo* implicitArgType =
            QoreTypeInfo::getImplicitArgTypeForIterator(right, iteratorTypeInfo);

        ParseImplicitArgTypeHelper pia(implicitArgType);

        parse_context.typeInfo = nullptr;
        if (parse_init_value(left, parse_context) && !err) {
            err = -1;
        }
        expTypeInfo = parse_context.typeInfo;
    }

    // use lazy evaluation if the iterator expression supports it
    iterator_func = dynamic_cast<FunctionalOperator*>(right.getInternalNode());

    // if "right" (iterator exp) is a list or an iterator, then the return type is expTypeInfo, otherwise it's the
    // return type of the iterated expression
    if (QoreTypeInfo::hasType(iteratorTypeInfo)) {
        if (QoreTypeInfo::isType(iteratorTypeInfo, NT_NOTHING)) {
            // FIXME: this should be an error with %strict-types
            qore_program_private::makeParseWarning(getProgram(), *loc, QP_WARN_INVALID_OPERATION, "INVALID-OPERATION",
                "the iterator expression with the foldl/r operator (the second expression) has no value (NOTHING) " \
                "and therefore this expression will also return no value; update the expression to return a value " \
                "or use '%%disable-warning invalid-operation' in your code to avoid seeing this warning in the " \
                "future");
            parse_context.typeInfo = nothingTypeInfo;
        } else if (QoreTypeInfo::isType(iteratorTypeInfo, NT_LIST)) {
            parse_context.typeInfo = expTypeInfo;
        } else {
            const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(iteratorTypeInfo);
            if (qc && qore_class_private::parseCheckCompatibleClass(qc, QC_ABSTRACTITERATOR)) {
                parse_context.typeInfo = expTypeInfo;
            } else if ((QoreTypeInfo::parseReturns(iteratorTypeInfo, NT_LIST) == QTI_NOT_EQUAL)
                && (QoreTypeInfo::parseReturns(iteratorTypeInfo, QC_ABSTRACTITERATOR) == QTI_NOT_EQUAL)) {
                parse_context.typeInfo = iteratorTypeInfo;
            }
        }
    }

    return err;
}

QoreValue QoreFoldlOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
   return doFold(true, needs_deref, xsink);
}

QoreValue QoreFoldlOperatorNode::doFold(bool fwd, bool& needs_deref, ExceptionSink* xsink) const {
    FunctionalOperator::FunctionalValueType value_type;
    std::unique_ptr<FunctionalOperatorInterface> f(getFunctionalIterator(value_type, fwd, xsink));
    if (*xsink || value_type == FunctionalOperator::nothing)
        return QoreValue();

    // get first value
    ValueOptionalRefHolder iv(xsink);
    if (f->getNext(iv, xsink))
        return QoreValue();
    if (*xsink)
        return QoreValue();

    ValueHolder result(iv.takeReferencedValue(), xsink);

    while (true) {
        // get next argument value
        if (f->getNext(iv, xsink))
            break;
        if (*xsink)
            return QoreValue();

        // create argument list for fold expression
        QoreListNode* args = new QoreListNode(autoTypeInfo);
        args->push(result.release(), xsink);
        args->push(iv.takeReferencedValue(), xsink);
        ArgvContextHelper argv_helper(args, xsink);
        result = left.eval(xsink);
        if (*xsink) {
            return QoreValue();
        }
    }

    return result.release();
}

FunctionalOperatorInterface* QoreFoldlOperatorNode::getFunctionalIterator(
        FunctionalOperator::FunctionalValueType& value_type, bool fwd, ExceptionSink* xsink) const {
    // we can only use the iterator_func with foldl
    if (iterator_func && fwd)
        return iterator_func->getFunctionalIterator(value_type, xsink);

    return FunctionalOperatorInterface::getFunctionalIterator(value_type, right, fwd,
        fwd ? "foldl operator" : "foldr operator", xsink);
}

// if del is true, then the returned QoreString * should be derefed, if false, then it must not be
QoreString* QoreFoldrOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &foldr_str;
}

int QoreFoldrOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.concat(&foldr_str);
    return 0;
}

QoreValue QoreFoldrOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    return doFold(false, needs_deref, xsink);
}

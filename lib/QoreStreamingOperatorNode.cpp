/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreStreamingOperatorNode.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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
#include "qore/intern/QoreIterateOperatorNode.h"

const char* QoreStreamingOperatorNode::getOperatorName() const {
    switch (kind) {
        case First:
            return "first operator expression";
        case Any:
            return "any operator expression";
        case All:
            return "all operator expression";
        case Count:
            return "count operator expression";
        case Take:
            return "take operator expression";
        case Drop:
            return "drop operator expression";
        case TakeWhile:
            return "takewhile operator expression";
        case TakeUntil:
            return "takeuntil operator expression";
    }
    return "streaming operator expression";
}

bool QoreStreamingOperatorNode::isTerminal() const {
    return kind == First || kind == Any || kind == All || kind == Count;
}

bool QoreStreamingOperatorNode::isPredicateOperator() const {
    return kind == First || kind == Any || kind == All || kind == Count || kind == TakeWhile || kind == TakeUntil;
}

QoreString* QoreStreamingOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    return new QoreString(getOperatorName());
}

int QoreStreamingOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.concat(getOperatorName());
    return 0;
}

int QoreStreamingOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);

    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    QoreParseAnalysis source_analysis;
    QoreParseAnalysis predicate_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(right, parse_context);
        source_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* sourceTypeInfo = parse_context.typeInfo;
    elementTypeInfo = QoreIterateOperatorNode::getElementTypeInfo(right, sourceTypeInfo);

    if (left && isPredicateOperator()) {
        ParseImplicitArgTypeHelper pia(elementTypeInfo);
        parse_context.typeInfo = nullptr;
        {
            QoreParseContextAnalysisHelper ah(parse_context);
            if (parse_init_value(left, parse_context) && !err) {
                err = -1;
            }
            predicate_analysis = parse_context.analysis;
        }
    } else if (left) {
        parse_context.typeInfo = nullptr;
        {
            QoreParseContextAnalysisHelper ah(parse_context);
            if (parse_init_value(left, parse_context) && !err) {
                err = -1;
            }
            predicate_analysis = parse_context.analysis;
        }
    }

    iterator_func = dynamic_cast<FunctionalOperator*>(right.getInternalNode());

    switch (kind) {
        case First:
            returnTypeInfo = elementTypeInfo ? qore_get_or_nothing_type(elementTypeInfo) : autoTypeInfo;
            break;
        case Any:
        case All:
            returnTypeInfo = boolTypeInfo;
            break;
        case Count:
            returnTypeInfo = bigIntTypeInfo;
            break;
        case Take:
        case Drop:
        case TakeWhile:
        case TakeUntil:
            returnTypeInfo = elementTypeInfo ? qore_get_complex_list_type(elementTypeInfo) : autoTypeInfo;
            break;
    }

    parse_context.typeInfo = returnTypeInfo;
    parse_context.analysis.clear();
    parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
    parse_context.analysis.known_type = returnTypeInfo;
    if (kind == Any || kind == All || kind == Count) {
        parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
    }
    if (source_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)
        && (!left || predicate_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned))) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

QoreValue QoreStreamingOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    if (!isTerminal()) {
        FunctionalOperator::FunctionalValueType value_type;
        std::unique_ptr<FunctionalOperatorInterface> f(getFunctionalIterator(value_type, xsink));
        if (*xsink || value_type == FunctionalOperator::nothing) {
            return QoreValue();
        }

        ReferenceHolder<QoreListNode> rv(new QoreListNode(f->getValueType()), xsink);
        size_t index = 0;
        while (true) {
            if (index && !(index % 100) && qore_check_cancel(xsink, getOperatorName())) {
                return QoreValue();
            }

            ValueOptionalRefHolder iv(xsink);
            if (f->getNext(iv, xsink)) {
                break;
            }
            if (*xsink) {
                return QoreValue();
            }

            rv->push(iv.takeReferencedValue(), xsink);
            if (*xsink) {
                return QoreValue();
            }
            ++index;
        }

        return rv.release();
    }

    FunctionalOperator::FunctionalValueType value_type;
    std::unique_ptr<FunctionalOperatorInterface> f(iterator_func
        ? iterator_func->getFunctionalIterator(value_type, xsink)
        : QoreIterateOperatorNode::getFunctionalIterator(value_type, right, elementTypeInfo, getOperatorName(),
            xsink));
    if (*xsink) {
        return QoreValue();
    }

    if (value_type == FunctionalOperator::nothing) {
        switch (kind) {
            case First:
                return QoreValue();
            case Any:
                return false;
            case All:
                return true;
            case Count:
                return (int64)0;
        }
    }

    size_t index = 0;
    int64 count = 0;
    while (true) {
        if (index && !(index % 100) && qore_check_cancel(xsink, getOperatorName())) {
            return QoreValue();
        }

        ValueOptionalRefHolder iv(xsink);
        if (f->getNext(iv, xsink)) {
            break;
        }
        if (*xsink) {
            return QoreValue();
        }

        bool matches = true;
        if (left) {
            ImplicitElementHelper eh(index);
            SingleArgvContextHelper argv_helper(iv->refSelf(), xsink);
            ValueEvalOptimizedRefHolder predicate(left, xsink);
            if (*xsink) {
                return QoreValue();
            }
            matches = predicate->getAsBool();
        }

        switch (kind) {
            case First:
                if (matches) {
                    return iv.takeReferencedValue();
                }
                break;
            case Any:
                if (!left || matches) {
                    return true;
                }
                break;
            case All:
                if (!matches) {
                    return false;
                }
                break;
            case Count:
                if (!left || matches) {
                    ++count;
                }
                break;
        }
        ++index;
    }

    switch (kind) {
        case First:
            return QoreValue();
        case Any:
            return false;
        case All:
            return true;
        case Count:
            return count;
    }
    return QoreValue();
}

FunctionalOperatorInterface* QoreStreamingOperatorNode::getFunctionalIteratorImpl(FunctionalValueType& value_type,
        ExceptionSink* xsink) const {
    if (isTerminal()) {
        value_type = nothing;
        return nullptr;
    }

    std::unique_ptr<FunctionalOperatorInterface> f(iterator_func
        ? iterator_func->getFunctionalIterator(value_type, xsink)
        : QoreIterateOperatorNode::getFunctionalIterator(value_type, right, elementTypeInfo, getOperatorName(),
            xsink));
    if (*xsink || value_type == nothing) {
        return nullptr;
    }

    int64 limit = 0;
    if (kind == Take || kind == Drop) {
        ValueEvalOptimizedRefHolder limit_holder(left, xsink);
        if (*xsink) {
            return nullptr;
        }
        limit = limit_holder->getAsBigInt();
        if (limit < 0) {
            xsink->raiseException("STREAMING-OPERATOR-ERROR", "%s requires a non-negative count, got " QLLD,
                getOperatorName(), limit);
            return nullptr;
        }
    }

    return new QoreFunctionalStreamingOperator(this, f.release(), limit);
}

bool QoreFunctionalStreamingOperator::skipDropPrefix(ExceptionSink* xsink) {
    if (initialized) {
        return false;
    }
    initialized = true;

    while (skipped < limit) {
        if (skipped && !(skipped % 100) && qore_check_cancel(xsink, op->getOperatorName())) {
            return true;
        }

        ValueOptionalRefHolder iv(xsink);
        if (f->getNext(iv, xsink)) {
            done = true;
            return true;
        }
        if (*xsink) {
            return true;
        }
        ++skipped;
        ++index;
    }
    return false;
}

bool QoreFunctionalStreamingOperator::predicateMatches(ValueOptionalRefHolder& iv, ExceptionSink* xsink) {
    ImplicitElementHelper eh(index);
    SingleArgvContextHelper argv_helper(iv->refSelf(), xsink);
    ValueEvalOptimizedRefHolder result(op->left, xsink);
    if (*xsink) {
        return false;
    }
    return result->getAsBool();
}

bool QoreFunctionalStreamingOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (done) {
        return true;
    }

    if (op->kind == QoreStreamingOperatorNode::Drop && skipDropPrefix(xsink)) {
        return *xsink ? false : true;
    }

    if (op->kind == QoreStreamingOperatorNode::Take && emitted >= limit) {
        done = true;
        return true;
    }

    ValueOptionalRefHolder iv(xsink);
    if (f->getNext(iv, xsink)) {
        done = true;
        return true;
    }
    if (*xsink) {
        return false;
    }

    bool include = true;
    if (op->kind == QoreStreamingOperatorNode::TakeWhile || op->kind == QoreStreamingOperatorNode::TakeUntil) {
        include = predicateMatches(iv, xsink);
        if (*xsink) {
            return false;
        }
        if ((op->kind == QoreStreamingOperatorNode::TakeWhile && !include)
            || (op->kind == QoreStreamingOperatorNode::TakeUntil && include)) {
            done = true;
            return true;
        }
    }

    ++index;
    ++emitted;
    iv.ensureReferencedValue();
    val.takeValueFrom(iv);
    return false;
}

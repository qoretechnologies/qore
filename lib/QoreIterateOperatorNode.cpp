/*
    QoreIterateOperatorNode.cpp

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

#include "qore/intern/qore_string_private.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreListIterator.h"
#include "qore/intern/SingleValueIterator.h"
#include "qore/intern/QC_StringCharIterator.h"

QoreString QoreIterateOperatorNode::iterate_str("iterate operator expression");

static const QoreTypeInfo* get_abstract_iterator_type(const QoreTypeInfo* element_type) {
    if (!QC_ABSTRACTITERATOR) {
        return autoTypeInfo;
    }

    type_vec_t type_args;
    type_args.push_back(element_type && QoreTypeInfo::hasType(element_type) ? element_type : autoTypeInfo);
    return QC_ABSTRACTITERATOR->getTypeInfo(type_args);
}

static bool is_abstract_iterator_object(const QoreObject* obj) {
    if (!obj || !QC_ABSTRACTITERATOR) {
        return false;
    }

    bool priv;
    return obj->getClass()->getClass(*QC_ABSTRACTITERATOR, priv);
}

static QoreObject* make_range_iterator(int64 stop, ExceptionSink* xsink) {
    ReferenceHolder<RangeIterator> r(new RangeIterator(0, stop, 1, QoreValue(), xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    return new QoreObject(QC_RANGEITERATOR, nullptr, r.release());
}

static QoreObject* make_binary_byte_iterator(const BinaryNode* b, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> bytes(new QoreListNode(bigIntTypeInfo), xsink);
    const unsigned char* ptr = static_cast<const unsigned char*>(b->getPtr());
    size_t size = b->size();
    for (size_t i = 0; i < size; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "iterate operator expression")) {
            return nullptr;
        }
        bytes->push(static_cast<int64>(ptr[i]), xsink);
        if (*xsink) {
            return nullptr;
        }
    }
    return qore_new_list_iterator_object(nullptr, *bytes);
}

static QoreObject* make_string_char_iterator(const QoreStringNode* str) {
    return new QoreObject(QC_STRINGITERATOR, nullptr, new StringIterator(str));
}

QoreString* QoreIterateOperatorNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = false;
    return &iterate_str;
}

int QoreIterateOperatorNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    qore_string_private::get(str)->concat(&iterate_str);
    return 0;
}

const QoreTypeInfo* QoreIterateOperatorNode::getElementTypeInfo(const QoreValue& source,
        const QoreTypeInfo* sourceTypeInfo) {
    const QoreTypeInfo* element_type = QoreTypeInfo::getUniqueReturnComplexList(sourceTypeInfo);
    if (element_type) {
        return element_type;
    }

    element_type = QoreTypeInfo::getUniqueReturnComplexHash(sourceTypeInfo);
    if (!element_type) {
        element_type = QoreTypeInfo::getReturnComplexHashOrNothing(sourceTypeInfo);
    }
    if (element_type || (sourceTypeInfo && QoreTypeInfo::parseReturns(sourceTypeInfo, NT_HASH) == QTI_IDENT)) {
        return QoreTypeInfo::getHashPairType(element_type ? element_type : autoTypeInfo);
    }

    element_type = QoreTypeInfo::getUniqueReturnComplexBuffer(sourceTypeInfo);
    if (element_type) {
        return element_type;
    }
    if (sourceTypeInfo && QoreTypeInfo::parseReturns(sourceTypeInfo, NT_BUFFER) == QTI_IDENT) {
        return autoTypeInfo;
    }

    if (sourceTypeInfo && (QoreTypeInfo::parseReturns(sourceTypeInfo, NT_BINARY) == QTI_IDENT
            || QoreTypeInfo::parseReturns(sourceTypeInfo, NT_INT) == QTI_IDENT)) {
        return bigIntTypeInfo;
    }

    if (sourceTypeInfo && QoreTypeInfo::parseReturns(sourceTypeInfo, NT_STRING) == QTI_IDENT) {
        return charTypeInfo;
    }

    element_type = QoreTypeInfo::getAbstractIteratorElementType(sourceTypeInfo);
    if (element_type) {
        return element_type;
    }

    element_type = QoreTypeInfo::getImplicitArgTypeForIterator(source, sourceTypeInfo);
    if (element_type) {
        return element_type;
    }

    return sourceTypeInfo && QoreTypeInfo::hasType(sourceTypeInfo) ? sourceTypeInfo : autoTypeInfo;
}

int QoreIterateOperatorNode::parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
    assert(!parse_context.typeInfo);

    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);

    QoreParseAnalysis source_analysis;
    int err = 0;
    {
        QoreParseContextAnalysisHelper ah(parse_context);
        err = parse_init_value(exp, parse_context);
        source_analysis = parse_context.analysis;
    }
    const QoreTypeInfo* sourceTypeInfo = parse_context.typeInfo;

    elementTypeInfo = getElementTypeInfo(exp, sourceTypeInfo);
    returnTypeInfo = get_abstract_iterator_type(elementTypeInfo);

    parse_context.typeInfo = returnTypeInfo;
    parse_context.analysis.clear();
    parse_context.analysis.setFlag(QoreParseAnalysis::KnownTypeInfo);
    parse_context.analysis.setFlag(QoreParseAnalysis::NeverNothing);
    parse_context.analysis.known_type = returnTypeInfo;
    if (source_analysis.hasFlag(QoreParseAnalysis::DefinitelyAssigned)) {
        parse_context.analysis.setFlag(QoreParseAnalysis::DefinitelyAssigned);
    }
    return err;
}

QoreValue QoreIterateOperatorNode::evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
    ValueEvalOptimizedRefHolder source(exp, xsink);
    if (*xsink) {
        return QoreValue();
    }

    return evalIteratorValue(*source, elementTypeInfo, xsink);
}

QoreValue QoreIterateOperatorNode::evalIteratorValue(QoreValue source, const QoreTypeInfo* elementTypeInfo,
        ExceptionSink* xsink) {
    switch (source.getType()) {
        case NT_LIST:
            return qore_new_list_iterator_object(nullptr, source.get<const QoreListNode>());
        case NT_HASH:
            return qore_new_hash_pair_iterator_object(nullptr, source.get<const QoreHashNode>());
        case NT_BINARY:
            return make_binary_byte_iterator(source.get<const BinaryNode>(), xsink);
        case NT_BUFFER: {
            ReferenceHolder<QoreListNode> l(source.get<const QoreBufferNode>()->toList(xsink), xsink);
            if (*xsink) {
                return QoreValue();
            }
            return qore_new_list_iterator_object(nullptr, *l);
        }
        case NT_INT:
            return make_range_iterator(source.getAsBigInt(), xsink);
        case NT_STRING: {
            QoreStringNodeValueHelper str(source);
            return make_string_char_iterator(*str);
        }
        case NT_OBJECT:
            if (is_abstract_iterator_object(source.get<const QoreObject>())) {
                return source.refSelf();
            }
            break;
        case NT_NOTHING:
            return qore_new_single_value_iterator_object(nullptr, QoreValue(), elementTypeInfo);
        default:
            break;
    }

    return qore_new_single_value_iterator_object(nullptr, source, elementTypeInfo);
}

FunctionalOperatorInterface* QoreIterateOperatorNode::getFunctionalIteratorImpl(FunctionalValueType& value_type,
        ExceptionSink* xsink) const {
    return getFunctionalIterator(value_type, exp, elementTypeInfo, getTypeName(), xsink);
}

FunctionalOperatorInterface* QoreIterateOperatorNode::getFunctionalIterator(
        FunctionalOperator::FunctionalValueType& value_type, QoreValue source, const QoreTypeInfo* elementTypeInfo,
        const char* who, ExceptionSink* xsink) {
    ValueEvalOptimizedRefHolder marg(source, xsink);
    if (*xsink) {
        return nullptr;
    }

    switch (marg->getType()) {
        case NT_LIST:
            value_type = FunctionalOperator::list;
            return new QoreFunctionalListOperator(true, marg.takeReferencedNode<QoreListNode>(), xsink);
        case NT_HASH:
            value_type = FunctionalOperator::list;
            return new QoreFunctionalHashPairOperator(marg->get<const QoreHashNode>(),
                QoreTypeInfo::getUniqueReturnComplexHash(marg->getFullTypeInfo()), xsink);
        case NT_BINARY:
            value_type = FunctionalOperator::list;
            return new QoreFunctionalBinaryByteOperator(marg.takeReferencedNode<BinaryNode>(), xsink);
        case NT_BUFFER:
            value_type = FunctionalOperator::list;
            return new QoreFunctionalBufferOperator(true, marg.takeReferencedNode<QoreBufferNode>(), xsink);
        case NT_INT:
            value_type = FunctionalOperator::list;
            return new QoreFunctionalIterateRangeOperator(marg->getAsBigInt(), xsink);
        case NT_STRING:
            value_type = FunctionalOperator::list;
            {
                QoreStringNodeValueHelper str(*marg);
                return new QoreFunctionalStringCharOperator(str.getReferencedValue(), xsink);
            }
        case NT_OBJECT: {
            AbstractIteratorHelper h(xsink, who, const_cast<QoreObject*>(marg->get<const QoreObject>()));
            if (*xsink) {
                return nullptr;
            }
            if (h) {
                bool temp = marg.isTemp();
                if (temp) {
                    marg.clearTemp();
                } else {
                    const_cast<QoreObject*>(marg->get<const QoreObject>())->ref();
                    temp = true;
                }
                value_type = FunctionalOperator::list;
                return new QoreFunctionalIteratorOperator(temp, h, xsink);
            }
            break;
        }
        case NT_NOTHING:
            value_type = FunctionalOperator::nothing;
            return nullptr;
        default:
            break;
    }

    value_type = FunctionalOperator::single;
    return new QoreFunctionalSingleValueOperator(marg.takeReferencedValue(), xsink);
}

QoreFunctionalHashPairOperator::QoreFunctionalHashPairOperator(const QoreHashNode* h, const QoreTypeInfo* value_type,
        ExceptionSink* xs) : iterator(new QoreHashIterator(h, value_type)), xsink(xs) {
    iterator->setView(QoreHashIterator::VIEW_PAIR);
}

QoreFunctionalHashPairOperator::~QoreFunctionalHashPairOperator() {
    iterator->deref(xsink);
}

bool QoreFunctionalHashPairOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (index && !(index % 100) && qore_check_cancel(xsink, "iterate operator expression")) {
        return false;
    }
    if (!iterator->next()) {
        return true;
    }
    ++index;
    val.setValue(iterator->getReferencedValuePair(xsink), true);
    return false;
}

QoreFunctionalBinaryByteOperator::QoreFunctionalBinaryByteOperator(BinaryNode* n_b, ExceptionSink* xs)
        : b(n_b), ptr(static_cast<const unsigned char*>(n_b->getPtr())), size(n_b->size()), xsink(xs) {
}

QoreFunctionalBinaryByteOperator::~QoreFunctionalBinaryByteOperator() {
    b->deref(xsink);
}

bool QoreFunctionalBinaryByteOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (pos == size) {
        return true;
    }
    if (pos && !(pos % 100) && qore_check_cancel(xsink, "iterate operator expression")) {
        return false;
    }
    val.setValue(static_cast<int64>(ptr[pos++]));
    return false;
}

QoreFunctionalIterateRangeOperator::QoreFunctionalIterateRangeOperator(int64 stop, ExceptionSink* xs)
        : iterator(new RangeIterator(0, stop, 1, QoreValue(), xs)), xsink(xs) {
}

QoreFunctionalIterateRangeOperator::~QoreFunctionalIterateRangeOperator() {
    iterator->deref(xsink);
}

bool QoreFunctionalIterateRangeOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (index && !(index % 100) && qore_check_cancel(xsink, "iterate operator expression")) {
        return false;
    }
    if (!iterator->next()) {
        return true;
    }
    ++index;
    val.setValue(iterator->getValue(xsink), true);
    return false;
}

QoreFunctionalStringCharOperator::QoreFunctionalStringCharOperator(QoreStringNode* n_str, ExceptionSink* xs)
        : str(n_str),
          str_buf(n_str ? n_str->c_str() : nullptr),
          str_size(n_str ? n_str->size() : 0),
          ascii_compat(n_str && n_str->getEncoding()->isAsciiCompat()),
          xsink(xs) {
}

QoreFunctionalStringCharOperator::~QoreFunctionalStringCharOperator() {
    if (str) {
        str->deref(xsink);
    }
}

bool QoreFunctionalStringCharOperator::getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink) {
    if (!str || cursor >= str_size) {
        return true;
    }
    if (index && !(index % 100) && qore_check_cancel(xsink, "iterate operator expression")) {
        return false;
    }

    if (ascii_compat) {
        unsigned char b = static_cast<unsigned char>(str_buf[cursor]);
        if (b < 0x80) {
            ++cursor;
            ++index;
            val.setValue(QoreValue::makeChar(b));
            return false;
        }
    }

    unsigned clen = 0;
    unsigned cp = str->getUnicodePointFromBytePos(cursor, clen, xsink);
    if (*xsink) {
        return true;
    }
    if (clen == 0) {
        clen = 1;
    }
    cursor += clen;
    ++index;
    val.setValue(QoreValue::makeChar(cp));
    return false;
}

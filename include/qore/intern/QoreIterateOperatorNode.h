/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIterateOperatorNode.h

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

#ifndef _QORE_QOREITERATEOPERATORNODE_H
#define _QORE_QOREITERATEOPERATORNODE_H

#include "qore/intern/FunctionalOperator.h"
#include "qore/intern/FunctionalOperatorInterface.h"
#include "qore/intern/QoreHashIterator.h"
#include "qore/intern/RangeIterator.h"

class QoreIterateOperatorNode : public QoreSingleExpressionOperatorNode<QoreOperatorNode>, public FunctionalOperator {
public:
    DLLLOCAL QoreIterateOperatorNode(const QoreProgramLocation* loc, QoreValue exp)
            : QoreSingleExpressionOperatorNode<QoreOperatorNode>(loc, exp) {
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const;
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const;

    DLLLOCAL virtual const char* getTypeName() const {
        return iterate_str.getBuffer();
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        return copyBackgroundExplicit<QoreIterateOperatorNode>(xsink);
    }

    DLLLOCAL static const QoreTypeInfo* getElementTypeInfo(const QoreValue& source,
            const QoreTypeInfo* sourceTypeInfo);

    DLLLOCAL static FunctionalOperatorInterface* getFunctionalIterator(
            FunctionalOperator::FunctionalValueType& value_type, QoreValue source, const QoreTypeInfo* elementTypeInfo,
            const char* who, ExceptionSink* xsink);

    DLLLOCAL static QoreValue evalIteratorValue(QoreValue source, const QoreTypeInfo* elementTypeInfo,
            ExceptionSink* xsink);

protected:
    const QoreTypeInfo* elementTypeInfo = nullptr;
    const QoreTypeInfo* returnTypeInfo = nullptr;

    DLLLOCAL static QoreString iterate_str;

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;
    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return returnTypeInfo;
    }

    DLLLOCAL virtual FunctionalOperatorInterface* getFunctionalIteratorImpl(FunctionalValueType& value_type,
            ExceptionSink* xsink) const;
};

class QoreFunctionalHashPairOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalHashPairOperator(const QoreHashNode* h, const QoreTypeInfo* value_type, ExceptionSink* xs);
    DLLLOCAL ~QoreFunctionalHashPairOperator();

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return QoreTypeInfo::getHashPairType(iterator->getValueTypeInfo());
    }

private:
    QoreHashIterator* iterator;
    size_t index = 0;
    ExceptionSink* xsink;
};

class QoreFunctionalBinaryByteOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalBinaryByteOperator(BinaryNode* b, ExceptionSink* xs);
    DLLLOCAL ~QoreFunctionalBinaryByteOperator();

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return bigIntTypeInfo;
    }

private:
    BinaryNode* b;
    const unsigned char* ptr;
    size_t size;
    size_t pos = 0;
    ExceptionSink* xsink;
};

class QoreFunctionalIterateRangeOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalIterateRangeOperator(int64 stop, ExceptionSink* xs);
    DLLLOCAL ~QoreFunctionalIterateRangeOperator();

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return bigIntTypeInfo;
    }

private:
    RangeIterator* iterator;
    size_t index = 0;
    ExceptionSink* xsink;
};

class QoreFunctionalStringCharOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalStringCharOperator(QoreStringNode* str, ExceptionSink* xs);
    DLLLOCAL ~QoreFunctionalStringCharOperator();

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const {
        return charTypeInfo;
    }

private:
    QoreStringNode* str;
    const char* str_buf = nullptr;
    size_t str_size = 0;
    bool ascii_compat = false;
    size_t cursor = 0;
    size_t index = 0;
    ExceptionSink* xsink;
};

#endif

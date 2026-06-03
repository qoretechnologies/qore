/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreSquareBracketsOperatorNode.h

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

#ifndef _QORE_QORESQUAREBRACKETSOPERATORNODE_H
#define _QORE_QORESQUAREBRACKETSOPERATORNODE_H

class QoreSquareBracketsOperatorNode : public QoreBinaryOperatorNode<>, public FunctionalOperator {
OP_COMMON
public:
    DLLLOCAL QoreSquareBracketsOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreBinaryOperatorNode<>(loc, left, right) {
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const;

    DLLLOCAL static bool normalizeIndex(int64& offset, int64 size, bool negative_offsets);

    DLLLOCAL static QoreValue doSquareBracketsListRange(const QoreValue l, const QoreParseListNode* pln,
            bool string_index_char, bool negative_offsets, ExceptionSink* xsink);

    DLLLOCAL static QoreValue doSquareBrackets(const QoreValue l, const QoreValue r, bool list_ok,
            bool string_index_char, bool negative_offsets, ExceptionSink* xsink);

    //! Returns true if the RHS is a list with a range (e.g., list[1..3])
    DLLLOCAL bool hasRhsListRange() const {
        return rhs_list_range;
    }

    DLLLOCAL bool hasStringIndexChar() const {
        return string_index_char;
    }

    DLLLOCAL bool hasNegativeOffsets() const {
        return negative_offsets;
    }

protected:
    const QoreTypeInfo* typeInfo = nullptr;
    // is the RHS a list with a range?
    bool rhs_list_range = false;
    bool string_index_char = true;
    bool negative_offsets = false;

    DLLLOCAL int parseCheckValueTypes(const QoreParseListNode* pln);
    DLLLOCAL int parseCheckValueTypes(const QoreListNode* ln);

    DLLLOCAL QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context);

    DLLLOCAL virtual FunctionalOperatorInterface* getFunctionalIteratorImpl(FunctionalValueType& value_type,
            ExceptionSink* xsink) const;

    DLLLOCAL static int doString(SimpleRefHolder<QoreStringNode>& ret, const QoreValue l, const QoreValue r,
            bool list_ok, bool string_index_char, bool negative_offsets, ExceptionSink* xsink);
    DLLLOCAL static int doBinary(SimpleRefHolder<BinaryNode>& ret, const QoreValue l, const QoreValue r, bool list_ok,
            bool negative_offsets, ExceptionSink* xsink);
};

class QoreFunctionalSquareBracketsOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalSquareBracketsOperator(ValueEvalRefHolder& lhs, ValueEvalRefHolder& rhs,
            bool string_index_char, bool negative_offsets, ExceptionSink* xsink)
            : leftValue(*lhs, lhs.isTemp(), xsink),
              rightList(rhs->get<const QoreListNode>()),
              string_index_char(string_index_char),
              negative_offsets(negative_offsets) {
        lhs.clearTemp();
        rhs.clearTemp();
    }

    DLLLOCAL virtual ~QoreFunctionalSquareBracketsOperator() {}

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const;

protected:
    ValueOptionalRefHolder leftValue;
    const QoreListNode* rightList;
    qore_offset_t offset = -1;
    bool string_index_char = true;
    bool negative_offsets = false;
};

class QoreFunctionalSquareBracketsComplexOperator : public FunctionalOperatorInterface {
public:
    DLLLOCAL QoreFunctionalSquareBracketsComplexOperator(ValueEvalRefHolder& lhs, const QoreParseListNode* rpl,
            bool string_index_char, bool negative_offsets, ExceptionSink* xsink)
            : leftValue(*lhs, lhs.isTemp(), xsink),
            rightParseList(rpl),
            string_index_char(string_index_char),
            negative_offsets(negative_offsets) {
        lhs.clearTemp();
    }

    DLLLOCAL virtual ~QoreFunctionalSquareBracketsComplexOperator() {}

    DLLLOCAL virtual bool getNextImpl(ValueOptionalRefHolder& val, ExceptionSink* xsink);

    DLLLOCAL virtual const QoreTypeInfo* getValueTypeImpl() const;

protected:
    ValueOptionalRefHolder leftValue;
    const QoreParseListNode* rightParseList;
    qore_offset_t offset = -1;
    std::unique_ptr<class QoreFunctionalRangeOperator> rangeIter;
    bool string_index_char = true;
    bool negative_offsets = false;
};

#endif

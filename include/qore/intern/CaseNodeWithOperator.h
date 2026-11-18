/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    CaseNodeWithOperator.h

    Qore Programming Language

    Copyright (C) 2006 - 2026 Qore Technologies, s.r.o.

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

#ifndef QORE_CASENODEWITHOPERATOR_H
#define QORE_CASENODEWITHOPERATOR_H

#include "qore/intern/SwitchStatement.h"
#include "qore/intern/QoreOperatorNode.h"

//! the operator of a switch case with an operator
enum class QoreSwitchCaseOperator : uint8_t {
    GreaterThanOrEquals,  //!< case >= x:
    LessThanOrEquals,     //!< case <= x:
    Equals,               //!< case == x:
    LessThan,             //!< case < x:
    GreaterThan,          //!< case > x:
};

//! The value of the switch statement in the comparison of a case with an operator
/** A case with an operator matches when <tt>value OP x</tt> is true as the condition of an \c if statement
    (qore#6252); its comparison is that operator expression, parse-initialized with this node as its left operand,
    which has the type of the switch expression.  The AST interpreter evaluates the node in the scope of a
    QoreSwitchCaseValueScope, which gives the value of the switch statement being matched; the IR lowering lowers the
    node to the IR value of the switch expression.
*/
class QoreSwitchValueOperatorNode : public QoreOperatorNode {
public:
    DLLLOCAL QoreSwitchValueOperatorNode(const QoreProgramLocation* loc, const QoreTypeInfo* typeInfo)
            : QoreOperatorNode(loc), typeInfo(typeInfo) {
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        str.concat(op_str.c_str());
        return 0;
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        del = false;
        return &op_str;
    }

    DLLLOCAL virtual const char* getTypeName() const {
        return op_str.c_str();
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return typeInfo;
    }

    DLLLOCAL virtual bool hasEffect() const {
        return false;
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return false;
    }

    DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink* xsink) const {
        return new QoreSwitchValueOperatorNode(loc, typeInfo);
    }

protected:
    //! the type of the switch expression; nullptr if unknown
    const QoreTypeInfo* typeInfo;

    DLLLOCAL static QoreString op_str;

    //! returns the value of the switch statement being matched, without a reference of its own
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        parse_context.typeInfo = typeInfo;
        return 0;
    }
};

//! Gives the value of the switch statement being matched to its QoreSwitchValueOperatorNode while it is in scope
/** The scopes nest: a comparison can run code (a plugin operation) that runs another switch statement
*/
class QoreSwitchCaseValueScope {
public:
    DLLLOCAL QoreSwitchCaseValueScope(const QoreValue& value);
    DLLLOCAL ~QoreSwitchCaseValueScope();

    QoreSwitchCaseValueScope(const QoreSwitchCaseValueScope&) = delete;
    QoreSwitchCaseValueScope& operator=(const QoreSwitchCaseValueScope&) = delete;

private:
    const QoreValue* old;
};

// Class supporting:
// switch ($a) {
// case > 1: ..
// case < 0.0: ...
// case >= 1.0: ...
// case <= -1: ...
// case == 2: ...
//
class CaseNodeWithOperator : public CaseNode {
private:
    DLLLOCAL virtual bool isCaseNodeImpl() const;

public:
    DLLLOCAL CaseNodeWithOperator(const QoreProgramLocation* loc, QoreValue v, StatementBlock* c,
            QoreSwitchCaseOperator op, LexicalSemanticPolicy policy)
            : CaseNode(loc, v, c), op(op), policy(policy) {
    }

    DLLLOCAL virtual ~CaseNodeWithOperator();

    //! matches when <tt>lhs_value OP val</tt> is true as the condition of an \c if statement
    DLLLOCAL virtual bool matches(QoreValue lhs_value, ExceptionSink* xsink) const;

    //! creates and parse-initializes the comparison of the case (qore#6252)
    /** called after the case value has been parse-initialized

        @param switch_type_info the type of the switch expression
        @param parse_context the parse context of the switch statement

        @return 0 for OK, -1 if a parse exception was raised
    */
    DLLLOCAL int parseInitComparison(const QoreTypeInfo* switch_type_info, QoreParseContext& parse_context);

    DLLLOCAL QoreSwitchCaseOperator getOperator() const {
        return op;
    }

    //! returns the comparison of the case: the operator expression with a QoreSwitchValueOperatorNode as its left
    //! operand; NOTHING if it was not parse-initialized
    DLLLOCAL const QoreValue& getComparison() const {
        return cmp;
    }

private:
    //! compares the switch value with the right operand of cmp_op as the operator compares the values of its operands
    DLLLOCAL QORE_INTERN_ALWAYS_INLINE QoreValue compareValues(const QoreValue& lhs_value,
            const QoreValue& rhs_value, ExceptionSink* xsink) const;

    //! returns the truth of a comparison result that is not a bool, as the condition of an if statement, and
    //! releases it
    DLLLOCAL QORE_INTERN_NOINLINE bool matchesResult(QoreValue& result, ExceptionSink* xsink) const;

    //! matches with a case value that is evaluated at run time, or with a comparison that parse initialization made
    //! another expression
    DLLLOCAL QORE_INTERN_NOINLINE bool matchesSlow(const QoreValue& lhs_value, ExceptionSink* xsink) const;

    QoreSwitchCaseOperator op;
    //! the semantic policy of the case operator
    LexicalSemanticPolicy policy;
    //! the comparison: value OP val
    QoreValue cmp;
    //! the comparison operator with the switch value as its left operand, whose evalValues() compares the switch
    //! value with its right operand; nullptr if parse initialization gave another expression, which is evaluated
    const QoreBinaryOperatorNode<>* cmp_op = nullptr;
    //! true if the right operand of cmp_op needs evaluation (a constant whose value an AOT build gives at run time)
    bool cmp_right_needs_eval = false;
    //! the scalar comparison of cmp_op (ex: QoreLogicalLessThanOperatorNode::doLessThan()) when it has no
    //! specialization for the types of its operands and its right operand is an inline constant: the comparison of
    //! cmp_op for a switch value that is not a node, which cannot be a buffer or a plugin value; nullptr otherwise
    bool (*cmp_scalar)(const QoreValue& l, const QoreValue& r, ExceptionSink* xsink) = nullptr;
};

#endif

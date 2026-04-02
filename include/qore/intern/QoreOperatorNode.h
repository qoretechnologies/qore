/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreOperatorNode.h

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

#ifndef _QORE_QOREOPERATORNODE_H
#define _QORE_QOREOPERATORNODE_H

#include <cstdarg>
#include "qore/intern/qore_string_private.h"

class RuntimeConfig;

DLLLOCAL QoreValue copy_value_and_resolve_lvar_refs(const QoreValue& n, ExceptionSink* xsink);
DLLLOCAL QoreValue copy_value_and_resolve_lvar_refs(RuntimeConfig& rc, const QoreValue& n, ExceptionSink* xsink);

// type of logical operator function
typedef bool (*op_log_func_t)(const QoreValue& l, const QoreValue& r, ExceptionSink* xsink);

class QoreOperatorNode : public ParseNode {
private:
   bool in_parentheses;
protected:
   bool ref_rv;

   DLLLOCAL virtual ~QoreOperatorNode() {}

   DLLLOCAL virtual void ignoreReturnValueImpl() {}

public:
   DLLLOCAL QoreOperatorNode(const QoreProgramLocation* loc, bool n_ref_rv = true) : ParseNode(loc, NT_OPERATOR),
        in_parentheses(false), ref_rv(n_ref_rv) {
   }

   // returns the type name as a c string
   DLLLOCAL virtual const char *getTypeName() const = 0;

   //! Returns the expression's result type info, or nullptr if unknown
   /** Overridden by operator subclasses that know their result type at parse time.
       Used by getExprTypeInfo() in IR lowering for type-aware code generation.
   */
   DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const { return nullptr; }

   DLLLOCAL void ignoreReturnValue() {
      ref_rv = false;
      ignoreReturnValueImpl();
   }

   DLLLOCAL bool getInParentheses() const {
       return in_parentheses;
   }

   DLLLOCAL void setInParentheses() {
       in_parentheses = true;
   }

   DLLLOCAL virtual bool hasEffect() const = 0;
   DLLLOCAL virtual bool hasEffectAsRoot() const = 0;

   DLLLOCAL virtual QoreOperatorNode* copyBackground(ExceptionSink *xsink) const = 0;
};

class LValueOperatorNode : public QoreOperatorNode {
public:
    DLLLOCAL LValueOperatorNode(const QoreProgramLocation* loc) : QoreOperatorNode(loc) {
    }

    DLLLOCAL virtual bool hasEffect() const {
        return true;
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return true;
    }

    DLLLOCAL int checkLValue(QoreValue exp, int pflag, bool assignment = true) {
        if (exp) {
            if (check_lvalue(exp, assignment)) {
                parse_error(*loc, "expecting lvalue for %s, got '%s' instead", getTypeName(), exp.getTypeName());
                return -1;
            } else if ((pflag & PF_BACKGROUND) && exp.getType() == NT_VARREF
                && exp.get<const VarRefNode>()->getType() == VT_LOCAL) {
                parse_error(*loc, "illegal local variable modification with the background operator in %s",
                    getTypeName());
                return -1;
            }
        }
        return 0;
    }
};

template <class T = QoreOperatorNode>
class QoreSingleExpressionOperatorNode : public T {
protected:
    QoreValue exp{};

    DLLLOCAL ~QoreSingleExpressionOperatorNode() {
        exp.discard(nullptr);
    }

public:
    DLLLOCAL QoreSingleExpressionOperatorNode(const QoreProgramLocation* loc, QoreValue exp) : T(loc), exp(exp) {
    }

    DLLLOCAL QoreValue getExp() const {
        return exp;
    }

    DLLLOCAL QoreValue takeExp() {
        QoreValue rv = exp;
        exp = QoreValue();
        return rv;
    }

    template <class O>
    DLLLOCAL QoreSingleExpressionOperatorNode* makeSpecialization() {
        QoreValue e = exp;
        exp = QoreValue();
        SimpleRefHolder<QoreSingleExpressionOperatorNode> del(this);
        O* rv = new O(this->loc, e);
        if (!this->ref_rv)
            rv->ignoreReturnValue();
        return rv;
    }

    DLLLOCAL virtual bool hasEffect() const {
        return exp.hasEffect();
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return dynamic_cast<const LValueOperatorNode*>(this);
    }

    template <class O>
    DLLLOCAL O* copyBackgroundExplicit(ExceptionSink* xsink) const {
        ValueHolder n_exp(copy_value_and_resolve_lvar_refs(exp, xsink), xsink);
        if (*xsink)
            return nullptr;
        return new O(this->loc, n_exp.release());
    }
};

template <class T = QoreOperatorNode>
class QoreSingleValueExpressionOperatorNode : public T {
protected:
    QoreValue exp{};

    DLLLOCAL ~QoreSingleValueExpressionOperatorNode() {
        exp.discard(nullptr);
    }

public:
    DLLLOCAL QoreSingleValueExpressionOperatorNode(const QoreProgramLocation* loc, QoreValue exp) : T(loc), exp(exp) {
    }

    DLLLOCAL QoreValue getExp() const {
        return exp;
    }

    DLLLOCAL QoreValue takeExp() {
        QoreValue rv = exp;
        exp.clear();
        return rv;
    }

    template <class O>
    DLLLOCAL QoreSingleValueExpressionOperatorNode* makeSpecialization() {
        QoreValue e = exp;
        exp.clear();
        SimpleRefHolder<QoreSingleValueExpressionOperatorNode> del(this);
        O* rv = new O(this->loc, e);
        if (!this->ref_rv)
            rv->ignoreReturnValue();
        return rv;
    }

    DLLLOCAL virtual bool hasEffect() const {
        return exp.hasEffect();
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return dynamic_cast<const LValueOperatorNode*>(this);
    }

    template <class O>
    DLLLOCAL O* copyBackgroundExplicit(ExceptionSink* xsink) const {
        ValueHolder n_exp(copy_value_and_resolve_lvar_refs(exp, xsink), xsink);
        if (*xsink)
            return nullptr;
        return new O(this->loc, n_exp.release());
    }
};

template <class T = QoreOperatorNode>
class QoreBinaryOperatorNode : public T {
protected:
    QoreValue left, right;

public:
    DLLLOCAL QoreBinaryOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right) : T(loc), left(left), right(right) {
    }

    DLLLOCAL ~QoreBinaryOperatorNode() {
        left.discard(nullptr);
        right.discard(nullptr);
    }

    template<typename U>
    DLLLOCAL QoreBinaryOperatorNode* makeSpecialization() {
        QoreValue l = left, r = right;
        left = right = QoreValue();
        SimpleRefHolder<QoreBinaryOperatorNode> del(this);
        U* rv = new U(this->loc, l, r);
        if (!this->ref_rv)
            rv->ignoreReturnValue();
        return rv;
    }

    DLLLOCAL QoreValue swapLeft(QoreValue n_left) {
        QoreValue old_l = left;
        left = n_left;
        return old_l;
    }

    DLLLOCAL QoreValue swapRight(QoreValue n_right) {
        QoreValue old_r = right;
        right = n_right;
        return old_r;
    }

    DLLLOCAL QoreValue getLeft() const {
        return left;
    }

    DLLLOCAL QoreValue getRight() const {
        return right;
    }

    DLLLOCAL virtual bool hasEffect() const {
        return left.hasEffect() || right.hasEffect();
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return dynamic_cast<const LValueOperatorNode*>(this);
    }

    template <class O>
    DLLLOCAL O* copyBackgroundExplicit(ExceptionSink* xsink) const {
        ValueHolder n_left(copy_value_and_resolve_lvar_refs(left, xsink), xsink);
        if (*xsink)
            return nullptr;
        ValueHolder n_right(copy_value_and_resolve_lvar_refs(right, xsink), xsink);
        if (*xsink)
            return nullptr;
        return new O(this->loc, n_left.release(), n_right.release());
    }
};

class QoreBoolBinaryOperatorNode : public QoreBinaryOperatorNode<> {
public:
    DLLLOCAL QoreBoolBinaryOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreBinaryOperatorNode<>(loc, left, right) {
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return boolTypeInfo;
    }
};

class QoreIntBinaryOperatorNode : public QoreBinaryOperatorNode<> {
public:
    DLLLOCAL QoreIntBinaryOperatorNode(const QoreProgramLocation* loc, QoreValue left, QoreValue right)
            : QoreBinaryOperatorNode<>(loc, left, right) {
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return bigIntTypeInfo;
    }
};

#define OP_COMMON protected:\
    DLLLOCAL static QoreString op_str;\
public:\
    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {del = false;return &op_str;}\
    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {qore_string_private::get(str)->concat(&op_str);return 0;}\
    DLLLOCAL virtual const char* getTypeName() const {return op_str.getBuffer();}

template <unsigned int N, class T = QoreOperatorNode>
class QoreNOperatorNodeBase : public T {
protected:
    QoreValue e[N];

    DLLLOCAL virtual ~QoreNOperatorNodeBase() {
        for (unsigned i = 0; i < N; ++i) {
            e[i].discard(nullptr);
        }
    }

public:
    DLLLOCAL QoreNOperatorNodeBase(const QoreProgramLocation* loc, QoreValue a0, ...) : T(loc) {
        e[0] = a0;
        va_list ap;
        va_start(ap, a0);
        for (unsigned int i = 1; i < N; ++i)
            e[i] = va_arg(ap, QoreSimpleValue);
        va_end(ap);
    }

    DLLLOCAL QoreValue get(unsigned i) const {
        assert(i < N);
        return e[i];
    }

    DLLLOCAL virtual bool hasEffect() const {
        for (unsigned int i = 0; i < N; ++i)
            if (e[i].hasEffect())
                return true;
        return false;
    }

    DLLLOCAL virtual bool hasEffectAsRoot() const {
        return dynamic_cast<const LValueOperatorNode*>(this);
    }
};

// include operator headers
#include "qore/intern/QoreDeleteOperatorNode.h"
#include "qore/intern/QoreRemoveOperatorNode.h"
#include "qore/intern/QoreSpliceOperatorNode.h"
#include "qore/intern/QoreExtractOperatorNode.h"
#include "qore/intern/QoreCastOperatorNode.h"
#include "qore/intern/QoreKeysOperatorNode.h"
#include "qore/intern/QoreUnaryMinusOperatorNode.h"
#include "qore/intern/QoreUnaryPlusOperatorNode.h"
#include "qore/intern/QoreLogicalNotOperatorNode.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QoreLogicalEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteNotEqualsOperatorNode.h"
#include "qore/intern/QoreModuloOperatorNode.h"
#include "qore/intern/QoreBinaryAndOperatorNode.h"
#include "qore/intern/QoreBinaryOrOperatorNode.h"
#include "qore/intern/QoreBinaryXorOperatorNode.h"
#include "qore/intern/QoreBinaryNotOperatorNode.h"
#include "qore/intern/QoreShiftLeftOperatorNode.h"
#include "qore/intern/QoreShiftRightOperatorNode.h"
#include "qore/intern/QoreExistsOperatorNode.h"
#include "qore/intern/QoreElementsOperatorNode.h"
#include "qore/intern/QoreInstanceOfOperatorNode.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QoreRegexMatchOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QoreRegexSubstOperatorNode.h"
#include "qore/intern/QoreTransliterationOperatorNode.h"
#include "qore/intern/QoreBinaryLValueOperatorNode.h"
#include "qore/intern/QoreAssignmentOperatorNode.h"
#include "qore/intern/QoreListAssignmentOperatorNode.h"
#include "qore/intern/QorePlusEqualsOperatorNode.h"
#include "qore/intern/QoreIntPlusEqualsOperatorNode.h"
#include "qore/intern/QoreMinusEqualsOperatorNode.h"
#include "qore/intern/QoreIntMinusEqualsOperatorNode.h"
#include "qore/intern/QoreOrEqualsOperatorNode.h"
#include "qore/intern/QoreAndEqualsOperatorNode.h"
#include "qore/intern/QoreModuloEqualsOperatorNode.h"
#include "qore/intern/QoreMultiplyEqualsOperatorNode.h"
#include "qore/intern/QoreDivideEqualsOperatorNode.h"
#include "qore/intern/QoreXorEqualsOperatorNode.h"
#include "qore/intern/QoreShiftLeftEqualsOperatorNode.h"
#include "qore/intern/QoreShiftRightEqualsOperatorNode.h"
#include "qore/intern/QorePostIncrementOperatorNode.h"
#include "qore/intern/QoreIntPostIncrementOperatorNode.h"
#include "qore/intern/QorePostDecrementOperatorNode.h"
#include "qore/intern/QoreIntPostDecrementOperatorNode.h"
#include "qore/intern/QorePreIncrementOperatorNode.h"
#include "qore/intern/QoreIntPreIncrementOperatorNode.h"
#include "qore/intern/QorePreDecrementOperatorNode.h"
#include "qore/intern/QoreIntPreDecrementOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreDivisionOperatorNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/QoreMapOperatorNode.h"
#include "qore/intern/QoreMapSelectOperatorNode.h"
#include "qore/intern/QoreHashMapOperatorNode.h"
#include "qore/intern/QoreHashMapSelectOperatorNode.h"
#include "qore/intern/QoreFoldlOperatorNode.h"
#include "qore/intern/QoreSelectOperatorNode.h"
#include "qore/intern/QoreNullCoalescingOperatorNode.h"
#include "qore/intern/QoreValueCoalescingOperatorNode.h"
#include "qore/intern/QoreChompOperatorNode.h"
#include "qore/intern/QoreTrimOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/QoreShiftOperatorNode.h"
#include "qore/intern/QoreUnshiftOperatorNode.h"
#include "qore/intern/QorePopOperatorNode.h"
#include "qore/intern/QorePushOperatorNode.h"
#include "qore/intern/QoreLogicalAndOperatorNode.h"
#include "qore/intern/QoreLogicalOrOperatorNode.h"
#include "qore/intern/QoreLogicalComparisonOperatorNode.h"
#include "qore/intern/QorePlusOperatorNode.h"
#include "qore/intern/QoreMinusOperatorNode.h"
#include "qore/intern/QoreMultiplicationOperatorNode.h"
#include "qore/intern/QoreBackgroundOperatorNode.h"
#include "qore/intern/QoreRangeOperatorNode.h"
#include "qore/intern/QoreSquareBracketsRangeOperatorNode.h"

#endif

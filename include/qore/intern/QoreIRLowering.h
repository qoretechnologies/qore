/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRLowering.h

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

#ifndef _QORE_INTERN_QOREIRLOWERING_H
#define _QORE_INTERN_QOREIRLOWERING_H

#include <string>

#include <qore/intern/QoreIRBuilder.h>
#include <qore/intern/QoreParseAnalysis.h>

class QoreParseContext;
class QoreParseListNode;
class QoreListNode;
class QoreValue;
class VarRefNode;

class QoreIRLowering {
public:
    explicit QoreIRLowering(QoreIRBuilder& builder, QoreParseContext* parse_context = nullptr);

    QoreIRValue lowerExpression(const QoreValue& expr, std::string& error);
    void setParseContext(QoreParseContext* parse_context);

private:
    QoreIRValue lowerConstant(const QoreValue& expr, std::string& error);
    QoreIRValue lowerVarRef(const QoreValue& expr, std::string& error);
    QoreIRValue lowerAssignment(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPlusEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMinusEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMultiplyEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerDivideEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerModuloEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerAndEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerOrEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerXorEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPreIncrement(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPostIncrement(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPreDecrement(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPostDecrement(const QoreValue& expr, std::string& error);
    QoreIRValue lowerPlus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMinus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalNotEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalAbsoluteEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalAbsoluteNotEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalLessThan(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalLessThanOrEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalGreaterThan(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalGreaterThanOrEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalComparison(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalAnd(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalOr(const QoreValue& expr, std::string& error);
    QoreIRValue lowerLogicalNot(const QoreValue& expr, std::string& error);
    QoreIRValue lowerNullCoalescing(const QoreValue& expr, std::string& error);
    QoreIRValue lowerValueCoalescing(const QoreValue& expr, std::string& error);
    QoreIRValue lowerQuestionMark(const QoreValue& expr, std::string& error);
    QoreIRValue lowerUnaryPlus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerUnaryMinus(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMultiplication(const QoreValue& expr, std::string& error);
    QoreIRValue lowerDivision(const QoreValue& expr, std::string& error);
    QoreIRValue lowerModulo(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryAnd(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryOr(const QoreValue& expr, std::string& error);
    QoreIRValue lowerBinaryXor(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftLeft(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftRight(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftLeftEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShiftRightEquals(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRange(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSquareBracketsRange(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSquareBrackets(const QoreValue& expr, std::string& error);
    QoreIRValue lowerHashObjectDereference(const QoreValue& expr, std::string& error);
    QoreIRValue lowerShift(const QoreValue& expr, std::string& error);
    QoreIRValue lowerUnshift(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSplice(const QoreValue& expr, std::string& error);
    QoreIRValue lowerExtract(const QoreValue& expr, std::string& error);
    QoreIRValue lowerRemove(const QoreValue& expr, std::string& error);
    QoreIRValue lowerKeys(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFoldl(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFoldr(const QoreValue& expr, std::string& error);
    QoreIRValue lowerMap(const QoreValue& expr, std::string& error);
    QoreIRValue lowerCast(const QoreValue& expr, std::string& error);
    QoreIRValue lowerFunctionCall(const QoreValue& expr, std::string& error);
    QoreIRValue lowerCallReference(const QoreValue& expr, std::string& error);
    QoreIRValue lowerSelfCall(const QoreValue& expr, std::string& error);
    QoreIRValue lowerStaticCall(const QoreValue& expr, std::string& error);
    bool getAnalysis(const QoreValue& expr, QoreParseAnalysis& analysis);
    bool isNeverNothingInt(const QoreParseAnalysis& analysis) const;
    bool isNeverNothingFloat(const QoreParseAnalysis& analysis) const;
    bool ensureBuilderContext(std::string& error) const;
    QoreIRBasicBlock* createBlock(const std::string& prefix);
    QoreIRValue loadVarRef(const VarRefNode* var, std::string& error, const char* context);
    bool storeVarRef(const VarRefNode* var, QoreIRValue value, std::string& error, const char* context);
    bool lowerCallArgs(const QoreParseListNode* parse_args, const QoreListNode* args,
        std::vector<QoreIRValue>& lowered, std::string& error);

    QoreIRBuilder& builder;
    QoreParseContext* parse_context = nullptr;
    uint32_t block_counter = 0;
};

#endif

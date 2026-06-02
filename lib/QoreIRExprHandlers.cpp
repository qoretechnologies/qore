/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRExprHandlers.cpp

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

    Note that the Qore library is dual-licensed under LGPL and MIT licenses; see
    LICENSE.LGPL and LICENSE.MIT in the source code directory for details.
*/

#include "qore/intern/QoreIRLowering.h"
#include "qore/intern/QoreIRExprRegistry.h"

// Handler for lowerConstant
static QoreIRValue handler_lowerConstant(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerConstant(ctx.expr, ctx.error);
}

// Handler for lowerParseList
static QoreIRValue handler_lowerParseList(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerParseList(ctx.expr, ctx.error);
}

// Handler for lowerParseHash
static QoreIRValue handler_lowerParseHash(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerParseHash(ctx.expr, ctx.error);
}

// Handler for lowerVarRef
static QoreIRValue handler_lowerVarRef(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerVarRef(ctx.expr, ctx.error);
}

// Handler for lowerAssignment
static QoreIRValue handler_lowerAssignment(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerAssignment(ctx.expr, ctx.error);
}

// Handler for lowerPlusEquals
static QoreIRValue handler_lowerPlusEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerPlusEquals(ctx.expr, ctx.error);
}

// Handler for lowerMinusEquals
static QoreIRValue handler_lowerMinusEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerMinusEquals(ctx.expr, ctx.error);
}

// Handler for lowerDivideEquals
static QoreIRValue handler_lowerDivideEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerDivideEquals(ctx.expr, ctx.error);
}

// Handler for lowerMultiplyEquals
static QoreIRValue handler_lowerMultiplyEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerMultiplyEquals(ctx.expr, ctx.error);
}

// Handler for lowerModuloEquals
static QoreIRValue handler_lowerModuloEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerModuloEquals(ctx.expr, ctx.error);
}

// Handler for lowerAndEquals
static QoreIRValue handler_lowerAndEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerAndEquals(ctx.expr, ctx.error);
}

// Handler for lowerOrEquals
static QoreIRValue handler_lowerOrEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerOrEquals(ctx.expr, ctx.error);
}

// Handler for lowerXorEquals
static QoreIRValue handler_lowerXorEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerXorEquals(ctx.expr, ctx.error);
}

// Handler for lowerPreDecrement
static QoreIRValue handler_lowerPreDecrement(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerPreDecrement(ctx.expr, ctx.error);
}

// Handler for lowerPostDecrement
static QoreIRValue handler_lowerPostDecrement(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerPostDecrement(ctx.expr, ctx.error);
}

// Handler for lowerPreIncrement
static QoreIRValue handler_lowerPreIncrement(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerPreIncrement(ctx.expr, ctx.error);
}

// Handler for lowerPostIncrement
static QoreIRValue handler_lowerPostIncrement(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerPostIncrement(ctx.expr, ctx.error);
}

// Handler for lowerPlus
static QoreIRValue handler_lowerPlus(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerPlus(ctx.expr, ctx.error);
}

// Handler for lowerMinus
static QoreIRValue handler_lowerMinus(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerMinus(ctx.expr, ctx.error);
}

// Handler for lowerLogicalLessThan
static QoreIRValue handler_lowerLogicalLessThan(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalLessThan(ctx.expr, ctx.error);
}

// Handler for lowerLogicalNotEquals
static QoreIRValue handler_lowerLogicalNotEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalNotEquals(ctx.expr, ctx.error);
}

// Handler for lowerLogicalEquals
static QoreIRValue handler_lowerLogicalEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalEquals(ctx.expr, ctx.error);
}

// Handler for lowerLogicalAbsoluteNotEquals
static QoreIRValue handler_lowerLogicalAbsoluteNotEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalAbsoluteNotEquals(ctx.expr, ctx.error);
}

// Handler for lowerLogicalAbsoluteEquals
static QoreIRValue handler_lowerLogicalAbsoluteEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalAbsoluteEquals(ctx.expr, ctx.error);
}

// Handler for lowerLogicalLessThanOrEquals
static QoreIRValue handler_lowerLogicalLessThanOrEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalLessThanOrEquals(ctx.expr, ctx.error);
}

// Handler for lowerLogicalGreaterThan
static QoreIRValue handler_lowerLogicalGreaterThan(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalGreaterThan(ctx.expr, ctx.error);
}

// Handler for lowerLogicalGreaterThanOrEquals
static QoreIRValue handler_lowerLogicalGreaterThanOrEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalGreaterThanOrEquals(ctx.expr, ctx.error);
}

// Handler for lowerLogicalComparison
static QoreIRValue handler_lowerLogicalComparison(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalComparison(ctx.expr, ctx.error);
}

// Handler for lowerUnaryPlus
static QoreIRValue handler_lowerUnaryPlus(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerUnaryPlus(ctx.expr, ctx.error);
}

// Handler for lowerUnaryMinus
static QoreIRValue handler_lowerUnaryMinus(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerUnaryMinus(ctx.expr, ctx.error);
}

// Handler for lowerBinaryNot
static QoreIRValue handler_lowerBinaryNot(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerBinaryNot(ctx.expr, ctx.error);
}

// Handler for lowerMultiplication
static QoreIRValue handler_lowerMultiplication(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerMultiplication(ctx.expr, ctx.error);
}

// Handler for lowerDivision
static QoreIRValue handler_lowerDivision(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerDivision(ctx.expr, ctx.error);
}

// Handler for lowerModulo
static QoreIRValue handler_lowerModulo(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerModulo(ctx.expr, ctx.error);
}

// Handler for lowerBinaryAnd
static QoreIRValue handler_lowerBinaryAnd(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerBinaryAnd(ctx.expr, ctx.error);
}

// Handler for lowerBinaryOr
static QoreIRValue handler_lowerBinaryOr(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerBinaryOr(ctx.expr, ctx.error);
}

// Handler for lowerBinaryXor
static QoreIRValue handler_lowerBinaryXor(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerBinaryXor(ctx.expr, ctx.error);
}

// Handler for lowerShiftLeft
static QoreIRValue handler_lowerShiftLeft(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerShiftLeft(ctx.expr, ctx.error);
}

// Handler for lowerShiftRight
static QoreIRValue handler_lowerShiftRight(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerShiftRight(ctx.expr, ctx.error);
}

// Handler for lowerShiftLeftEquals
static QoreIRValue handler_lowerShiftLeftEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerShiftLeftEquals(ctx.expr, ctx.error);
}

// Handler for lowerShiftRightEquals
static QoreIRValue handler_lowerShiftRightEquals(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerShiftRightEquals(ctx.expr, ctx.error);
}

// Handler for lowerRange
static QoreIRValue handler_lowerRange(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerRange(ctx.expr, ctx.error);
}

// Handler for lowerSquareBracketsRange
static QoreIRValue handler_lowerSquareBracketsRange(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerSquareBracketsRange(ctx.expr, ctx.error);
}

// Handler for lowerSquareBrackets
static QoreIRValue handler_lowerSquareBrackets(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerSquareBrackets(ctx.expr, ctx.error);
}

// Handler for lowerHashObjectDereference
static QoreIRValue handler_lowerHashObjectDereference(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerHashObjectDereference(ctx.expr, ctx.error);
}

// Handler for lowerPop
static QoreIRValue handler_lowerPop(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerPop(ctx.expr, ctx.error);
}

// Handler for lowerShift
static QoreIRValue handler_lowerShift(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerShift(ctx.expr, ctx.error);
}

// Handler for lowerPush
static QoreIRValue handler_lowerPush(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerPush(ctx.expr, ctx.error);
}

// Handler for lowerUnshift
static QoreIRValue handler_lowerUnshift(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerUnshift(ctx.expr, ctx.error);
}

// Handler for lowerSplice
static QoreIRValue handler_lowerSplice(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerSplice(ctx.expr, ctx.error);
}

// Handler for lowerExtract
static QoreIRValue handler_lowerExtract(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerExtract(ctx.expr, ctx.error);
}

// Handler for lowerRemove
static QoreIRValue handler_lowerRemove(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerRemove(ctx.expr, ctx.error);
}

// Handler for lowerDelete
static QoreIRValue handler_lowerDelete(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerDelete(ctx.expr, ctx.error);
}

// Handler for lowerKeys
static QoreIRValue handler_lowerKeys(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerKeys(ctx.expr, ctx.error);
}

// Handler for lowerRegexExtract
static QoreIRValue handler_lowerRegexExtract(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerRegexExtract(ctx.expr, ctx.error);
}

// Handler for lowerRegexNMatch
static QoreIRValue handler_lowerRegexNMatch(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerRegexNMatch(ctx.expr, ctx.error);
}

// Handler for lowerRegexMatch
static QoreIRValue handler_lowerRegexMatch(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerRegexMatch(ctx.expr, ctx.error);
}

// Handler for lowerRegexSubst
static QoreIRValue handler_lowerRegexSubst(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerRegexSubst(ctx.expr, ctx.error);
}

// Handler for lowerInstanceOf
static QoreIRValue handler_lowerInstanceOf(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerInstanceOf(ctx.expr, ctx.error);
}

// Handler for lowerTrim
static QoreIRValue handler_lowerTrim(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerTrim(ctx.expr, ctx.error);
}

// Handler for lowerChomp
static QoreIRValue handler_lowerChomp(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerChomp(ctx.expr, ctx.error);
}

// Handler for lowerTransliteration
static QoreIRValue handler_lowerTransliteration(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerTransliteration(ctx.expr, ctx.error);
}

// Handler for lowerBackground
static QoreIRValue handler_lowerBackground(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerBackground(ctx.expr, ctx.error);
}

// Handler for lowerListAssignment
static QoreIRValue handler_lowerListAssignment(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerListAssignment(ctx.expr, ctx.error);
}

// Handler for lowerExists
static QoreIRValue handler_lowerExists(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerExists(ctx.expr, ctx.error);
}

// Handler for lowerElements
static QoreIRValue handler_lowerElements(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerElements(ctx.expr, ctx.error);
}

// Handler for lowerDotEval
static QoreIRValue handler_lowerDotEval(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerDotEval(ctx.expr, ctx.error);
}

// Handler for lowerFoldr
static QoreIRValue handler_lowerFoldr(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerFoldr(ctx.expr, ctx.error);
}

// Handler for lowerFoldl
static QoreIRValue handler_lowerFoldl(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerFoldl(ctx.expr, ctx.error);
}

// Handler for lowerMap
static QoreIRValue handler_lowerMap(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerMap(ctx.expr, ctx.error);
}

// Handler for lowerSelect
static QoreIRValue handler_lowerSelect(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerSelect(ctx.expr, ctx.error);
}

// Handler for lowerMapSelect
static QoreIRValue handler_lowerMapSelect(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerMapSelect(ctx.expr, ctx.error);
}

// Handler for lowerHashMap
static QoreIRValue handler_lowerHashMap(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerHashMap(ctx.expr, ctx.error);
}

// Handler for lowerHashMapSelect
static QoreIRValue handler_lowerHashMapSelect(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerHashMapSelect(ctx.expr, ctx.error);
}

// Handler for lowerLogicalAnd
static QoreIRValue handler_lowerLogicalAnd(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalAnd(ctx.expr, ctx.error);
}

// Handler for lowerLogicalOr
static QoreIRValue handler_lowerLogicalOr(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalOr(ctx.expr, ctx.error);
}

// Handler for lowerLogicalNot
static QoreIRValue handler_lowerLogicalNot(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerLogicalNot(ctx.expr, ctx.error);
}

// Handler for lowerNullCoalescing
static QoreIRValue handler_lowerNullCoalescing(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerNullCoalescing(ctx.expr, ctx.error);
}

// Handler for lowerValueCoalescing
static QoreIRValue handler_lowerValueCoalescing(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerValueCoalescing(ctx.expr, ctx.error);
}

// Handler for lowerQuestionMark
static QoreIRValue handler_lowerQuestionMark(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerQuestionMark(ctx.expr, ctx.error);
}

// Handler for lowerCast
static QoreIRValue handler_lowerCast(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerCast(ctx.expr, ctx.error);
}

// Handler for lowerBuiltinTypeConversion
static QoreIRValue handler_lowerBuiltinTypeConversion(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerBuiltinTypeConversion(ctx.expr, ctx.error);
}

// Handler for lowerFunctionCall
static QoreIRValue handler_lowerFunctionCall(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerFunctionCall(ctx.expr, ctx.error);
}

// Handler for lowerCallReference
static QoreIRValue handler_lowerCallReference(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerCallReference(ctx.expr, ctx.error);
}

// Handler for lowerSelfCall
static QoreIRValue handler_lowerSelfCall(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerSelfCall(ctx.expr, ctx.error);
}

// Handler for lowerStaticCall
static QoreIRValue handler_lowerStaticCall(QoreIRExprCtx& ctx) {
    return ctx.lowering.lowerStaticCall(ctx.expr, ctx.error);
}

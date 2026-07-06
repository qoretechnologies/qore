/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRExprRegistry.cpp

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

#include <qore/DateTimeNode.h>
#include <qore/QoreHashNode.h>
#include <qore/QoreListNode.h>
#include <qore/intern/CallReferenceCallNode.h>
#include <qore/intern/FunctionCallNode.h>
#include <qore/intern/QoreAndEqualsOperatorNode.h>
#include <qore/intern/QoreAssignmentOperatorNode.h>
#include <qore/intern/QoreBackgroundOperatorNode.h>
#include <qore/intern/QoreBinaryAndOperatorNode.h>
#include <qore/intern/QoreBinaryNotOperatorNode.h>
#include <qore/intern/QoreBinaryOrOperatorNode.h>
#include <qore/intern/QoreBinaryXorOperatorNode.h>
#include <qore/intern/QoreCastOperatorNode.h>
#include <qore/intern/QoreChompOperatorNode.h>
#include <qore/intern/QoreDeleteOperatorNode.h>
#include <qore/intern/QoreDivideEqualsOperatorNode.h>
#include <qore/intern/QoreDivisionOperatorNode.h>
#include <qore/intern/QoreDotEvalOperatorNode.h>
#include <qore/intern/QoreElementsOperatorNode.h>
#include <qore/intern/QoreExistsOperatorNode.h>
#include <qore/intern/QoreExtractOperatorNode.h>
#include <qore/intern/QoreFoldlOperatorNode.h>
#include <qore/intern/QoreHashMapOperatorNode.h>
#include <qore/intern/QoreHashMapSelectOperatorNode.h>
#include <qore/intern/QoreHashObjectDereferenceOperatorNode.h>
#include <qore/intern/QoreInstanceOfOperatorNode.h>
#include <qore/intern/QoreIterateOperatorNode.h>
#include <qore/intern/QoreIntPostDecrementOperatorNode.h>
#include <qore/intern/QoreIntPostIncrementOperatorNode.h>
#include <qore/intern/QoreIntPreDecrementOperatorNode.h>
#include <qore/intern/QoreIntPreIncrementOperatorNode.h>
#include <qore/intern/QoreKeysOperatorNode.h>
#include <qore/intern/QoreListAssignmentOperatorNode.h>
#include <qore/intern/QoreLogicalAbsoluteEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalAbsoluteNotEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalAndOperatorNode.h>
#include <qore/intern/QoreLogicalComparisonOperatorNode.h>
#include <qore/intern/QoreLogicalEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalGreaterThanOperatorNode.h>
#include <qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalLessThanOperatorNode.h>
#include <qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalNotEqualsOperatorNode.h>
#include <qore/intern/QoreLogicalNotOperatorNode.h>
#include <qore/intern/QoreLogicalOrOperatorNode.h>
#include <qore/intern/QoreMapOperatorNode.h>
#include <qore/intern/QoreMapSelectOperatorNode.h>
#include <qore/intern/QoreMinusEqualsOperatorNode.h>
#include <qore/intern/QoreMinusOperatorNode.h>
#include <qore/intern/QoreModuloEqualsOperatorNode.h>
#include <qore/intern/QoreModuloOperatorNode.h>
#include <qore/intern/QoreMultiplicationOperatorNode.h>
#include <qore/intern/QoreMultiplyEqualsOperatorNode.h>
#include <qore/intern/QoreNullCoalescingOperatorNode.h>
#include <qore/intern/QoreOrEqualsOperatorNode.h>
#include <qore/intern/QoreParseHashNode.h>
#include <qore/intern/QoreParseListNode.h>
#include <qore/intern/QorePlusEqualsOperatorNode.h>
#include <qore/intern/QorePlusOperatorNode.h>
#include <qore/intern/QorePopOperatorNode.h>
#include <qore/intern/QorePostDecrementOperatorNode.h>
#include <qore/intern/QorePostIncrementOperatorNode.h>
#include <qore/intern/QorePreDecrementOperatorNode.h>
#include <qore/intern/QorePreIncrementOperatorNode.h>
#include <qore/intern/QorePushOperatorNode.h>
#include <qore/intern/QoreQuestionMarkOperatorNode.h>
#include <qore/intern/QoreRangeOperatorNode.h>
#include <qore/intern/QoreRegexExtractOperatorNode.h>
#include <qore/intern/QoreRegexMatchOperatorNode.h>
#include <qore/intern/QoreRegexNMatchOperatorNode.h>
#include <qore/intern/QoreRegexSubstOperatorNode.h>
#include <qore/intern/QoreRemoveOperatorNode.h>
#include <qore/intern/QoreSelectOperatorNode.h>
#include <qore/intern/QoreShiftLeftEqualsOperatorNode.h>
#include <qore/intern/QoreShiftLeftOperatorNode.h>
#include <qore/intern/QoreShiftOperatorNode.h>
#include <qore/intern/QoreShiftRightEqualsOperatorNode.h>
#include <qore/intern/QoreShiftRightOperatorNode.h>
#include <qore/intern/QoreSpliceOperatorNode.h>
#include <qore/intern/QoreSquareBracketsOperatorNode.h>
#include <qore/intern/QoreSquareBracketsRangeOperatorNode.h>
#include <qore/intern/QoreStreamingOperatorNode.h>
#include <qore/intern/QoreTransliterationOperatorNode.h>
#include <qore/intern/QoreTrimOperatorNode.h>
#include <qore/intern/QoreUnaryMinusOperatorNode.h>
#include <qore/intern/QoreUnaryPlusOperatorNode.h>
#include <qore/intern/QoreUnshiftOperatorNode.h>
#include <qore/intern/QoreValueCoalescingOperatorNode.h>
#include <qore/intern/QoreXorEqualsOperatorNode.h>
#include <qore/intern/VarRefNode.h>

#include <cstring>
#include <string>

template <typename T>
static bool claimNode(const QoreValue& expr) {
    return expr.hasNode() && dynamic_cast<const T*>(expr.getInternalNode());
}

static bool claimConstant(const QoreValue& expr) {
    if (expr.isEnum() || expr.isNothing() || expr.isBool() || expr.isChar() || expr.isInt()
            || expr.isFloat() || expr.isNull()) {
        return true;
    }
    qore_type_t type = expr.getType();
    if ((type == NT_INT || type == NT_FLOAT) && expr.hasNode()) {
        return true;
    }
    return (type == NT_STRING || type == NT_DATE || type == NT_LIST || type == NT_HASH)
        && expr.isValue();
}

static bool claimCast(const QoreValue& expr) {
    const AbstractQoreNode* node = expr.hasNode() ? expr.getInternalNode() : nullptr;
    return dynamic_cast<const QoreCastOperatorNode*>(node)
        || dynamic_cast<const QoreParseCastOperatorNode*>(node);
}

static bool claimBuiltinTypeConversion(const QoreValue& expr) {
    const auto* call = expr.hasNode()
        ? dynamic_cast<const FunctionCallNode*>(expr.getInternalNode()) : nullptr;
    if (!call) {
        return false;
    }
    const char* func_name = call->getName();
    if (!func_name || (strcmp(func_name, "string") && strcmp(func_name, "int") && strcmp(func_name, "float")
            && strcmp(func_name, "boolean"))) {
        return false;
    }
    const QoreListNode* args = call->getArgs();
    const QoreParseListNode* parse_args = call->getParseArgs();
    size_t nargs = args ? args->size() : (parse_args ? parse_args->size() : 0);
    return nargs == 1;
}

static bool claimFoldl(const QoreValue& expr) {
    const AbstractQoreNode* node = expr.hasNode() ? expr.getInternalNode() : nullptr;
    return dynamic_cast<const QoreFoldlOperatorNode*>(node)
        && !dynamic_cast<const QoreFoldrOperatorNode*>(node);
}

static bool claimLogicalAnd(const QoreValue& expr) {
    const AbstractQoreNode* node = expr.hasNode() ? expr.getInternalNode() : nullptr;
    return dynamic_cast<const QoreLogicalAndOperatorNode*>(node)
        && !dynamic_cast<const QoreLogicalOrOperatorNode*>(node);
}

static bool claimPreIncrement(const QoreValue& expr) {
    const AbstractQoreNode* node = expr.hasNode() ? expr.getInternalNode() : nullptr;
    return dynamic_cast<const QorePreIncrementOperatorNode*>(node)
        || dynamic_cast<const QoreIntPreIncrementOperatorNode*>(node);
}

static bool claimPostIncrement(const QoreValue& expr) {
    const AbstractQoreNode* node = expr.hasNode() ? expr.getInternalNode() : nullptr;
    return dynamic_cast<const QorePostIncrementOperatorNode*>(node)
        || (dynamic_cast<const QoreIntPostIncrementOperatorNode*>(node)
            && !dynamic_cast<const QoreIntPostDecrementOperatorNode*>(node));
}

static bool claimPreDecrement(const QoreValue& expr) {
    const AbstractQoreNode* node = expr.hasNode() ? expr.getInternalNode() : nullptr;
    return dynamic_cast<const QorePreDecrementOperatorNode*>(node)
        || dynamic_cast<const QoreIntPreDecrementOperatorNode*>(node);
}

static bool claimPostDecrement(const QoreValue& expr) {
    const AbstractQoreNode* node = expr.hasNode() ? expr.getInternalNode() : nullptr;
    return dynamic_cast<const QorePostDecrementOperatorNode*>(node)
        || dynamic_cast<const QoreIntPostDecrementOperatorNode*>(node);
}

#define CLAIM_NODE_FN(name, type) \
static bool claim##name(const QoreValue& expr) { \
    return claimNode<type>(expr); \
}

CLAIM_NODE_FN(ParseList, QoreParseListNode)
CLAIM_NODE_FN(ParseHash, QoreParseHashNode)
CLAIM_NODE_FN(VarRef, VarRefNode)
CLAIM_NODE_FN(Assignment, QoreAssignmentOperatorNode)
CLAIM_NODE_FN(PlusEquals, QorePlusEqualsOperatorNode)
CLAIM_NODE_FN(MinusEquals, QoreMinusEqualsOperatorNode)
CLAIM_NODE_FN(DivideEquals, QoreDivideEqualsOperatorNode)
CLAIM_NODE_FN(MultiplyEquals, QoreMultiplyEqualsOperatorNode)
CLAIM_NODE_FN(ModuloEquals, QoreModuloEqualsOperatorNode)
CLAIM_NODE_FN(AndEquals, QoreAndEqualsOperatorNode)
CLAIM_NODE_FN(OrEquals, QoreOrEqualsOperatorNode)
CLAIM_NODE_FN(XorEquals, QoreXorEqualsOperatorNode)
CLAIM_NODE_FN(Plus, QorePlusOperatorNode)
CLAIM_NODE_FN(Minus, QoreMinusOperatorNode)
CLAIM_NODE_FN(LogicalLessThan, QoreLogicalLessThanOperatorNode)
CLAIM_NODE_FN(LogicalNotEquals, QoreLogicalNotEqualsOperatorNode)
CLAIM_NODE_FN(LogicalEquals, QoreLogicalEqualsOperatorNode)
CLAIM_NODE_FN(LogicalAbsoluteNotEquals, QoreLogicalAbsoluteNotEqualsOperatorNode)
CLAIM_NODE_FN(LogicalAbsoluteEquals, QoreLogicalAbsoluteEqualsOperatorNode)
CLAIM_NODE_FN(LogicalLessThanOrEquals, QoreLogicalLessThanOrEqualsOperatorNode)
CLAIM_NODE_FN(LogicalGreaterThan, QoreLogicalGreaterThanOperatorNode)
CLAIM_NODE_FN(LogicalGreaterThanOrEquals, QoreLogicalGreaterThanOrEqualsOperatorNode)
CLAIM_NODE_FN(LogicalComparison, QoreLogicalComparisonOperatorNode)
CLAIM_NODE_FN(UnaryPlus, QoreUnaryPlusOperatorNode)
CLAIM_NODE_FN(UnaryMinus, QoreUnaryMinusOperatorNode)
CLAIM_NODE_FN(BinaryNot, QoreBinaryNotOperatorNode)
CLAIM_NODE_FN(Multiplication, QoreMultiplicationOperatorNode)
CLAIM_NODE_FN(Division, QoreDivisionOperatorNode)
CLAIM_NODE_FN(Modulo, QoreModuloOperatorNode)
CLAIM_NODE_FN(BinaryAnd, QoreBinaryAndOperatorNode)
CLAIM_NODE_FN(BinaryOr, QoreBinaryOrOperatorNode)
CLAIM_NODE_FN(BinaryXor, QoreBinaryXorOperatorNode)
CLAIM_NODE_FN(ShiftLeft, QoreShiftLeftOperatorNode)
CLAIM_NODE_FN(ShiftRight, QoreShiftRightOperatorNode)
CLAIM_NODE_FN(ShiftLeftEquals, QoreShiftLeftEqualsOperatorNode)
CLAIM_NODE_FN(ShiftRightEquals, QoreShiftRightEqualsOperatorNode)
CLAIM_NODE_FN(Range, QoreRangeOperatorNode)
CLAIM_NODE_FN(SquareBracketsRange, QoreSquareBracketsRangeOperatorNode)
CLAIM_NODE_FN(SquareBrackets, QoreSquareBracketsOperatorNode)
CLAIM_NODE_FN(HashObjectDereference, QoreHashObjectDereferenceOperatorNode)
CLAIM_NODE_FN(Pop, QorePopOperatorNode)
CLAIM_NODE_FN(Shift, QoreShiftOperatorNode)
CLAIM_NODE_FN(Push, QorePushOperatorNode)
CLAIM_NODE_FN(Unshift, QoreUnshiftOperatorNode)
CLAIM_NODE_FN(Splice, QoreSpliceOperatorNode)
CLAIM_NODE_FN(Extract, QoreExtractOperatorNode)
CLAIM_NODE_FN(Remove, QoreRemoveOperatorNode)
CLAIM_NODE_FN(Delete, QoreDeleteOperatorNode)
CLAIM_NODE_FN(Keys, QoreKeysOperatorNode)
CLAIM_NODE_FN(RegexExtract, QoreRegexExtractOperatorNode)
CLAIM_NODE_FN(RegexNMatch, QoreRegexNMatchOperatorNode)
CLAIM_NODE_FN(RegexMatch, QoreRegexMatchOperatorNode)
CLAIM_NODE_FN(RegexSubst, QoreRegexSubstOperatorNode)
CLAIM_NODE_FN(InstanceOf, QoreInstanceOfOperatorNode)
CLAIM_NODE_FN(Trim, QoreTrimOperatorNode)
CLAIM_NODE_FN(Chomp, QoreChompOperatorNode)
CLAIM_NODE_FN(Transliteration, QoreTransliterationOperatorNode)
CLAIM_NODE_FN(Background, QoreBackgroundOperatorNode)
CLAIM_NODE_FN(ListAssignment, QoreListAssignmentOperatorNode)
CLAIM_NODE_FN(Exists, QoreExistsOperatorNode)
CLAIM_NODE_FN(Elements, QoreElementsOperatorNode)
CLAIM_NODE_FN(DotEval, QoreDotEvalOperatorNode)
CLAIM_NODE_FN(Foldr, QoreFoldrOperatorNode)
CLAIM_NODE_FN(Map, QoreMapOperatorNode)
CLAIM_NODE_FN(Select, QoreSelectOperatorNode)
CLAIM_NODE_FN(MapSelect, QoreMapSelectOperatorNode)
CLAIM_NODE_FN(HashMap, QoreHashMapOperatorNode)
CLAIM_NODE_FN(HashMapSelect, QoreHashMapSelectOperatorNode)
CLAIM_NODE_FN(Iterate, QoreIterateOperatorNode)
CLAIM_NODE_FN(Streaming, QoreStreamingOperatorNode)
CLAIM_NODE_FN(LogicalOr, QoreLogicalOrOperatorNode)
CLAIM_NODE_FN(LogicalNot, QoreLogicalNotOperatorNode)
CLAIM_NODE_FN(NullCoalescing, QoreNullCoalescingOperatorNode)
CLAIM_NODE_FN(ValueCoalescing, QoreValueCoalescingOperatorNode)
CLAIM_NODE_FN(QuestionMark, QoreQuestionMarkOperatorNode)
CLAIM_NODE_FN(FunctionCall, FunctionCallNode)
CLAIM_NODE_FN(CallReference, CallReferenceCallNode)
CLAIM_NODE_FN(SelfCall, SelfFunctionCallNode)
CLAIM_NODE_FN(StaticCall, StaticMethodCallNode)

#undef CLAIM_NODE_FN

// Include all handler functions
#include "QoreIRExprHandlers.cpp"

#define QORE_IR_EXPR_ENTRY(name, claim_fn, description) \
    { #name, claim_fn, handler_##name, description }

// Registry table: expression handlers in dispatch order
const QoreIRExprHandlerInfo QORE_IR_EXPR_REGISTRY[] = {
    // Foundation handlers (4)
    QORE_IR_EXPR_ENTRY(lowerConstant, claimConstant, "Constants (string, number, etc.)"),
    QORE_IR_EXPR_ENTRY(lowerParseList, claimParseList, "List literals [...]"),
    QORE_IR_EXPR_ENTRY(lowerParseHash, claimParseHash, "Hash literals {...}"),
    QORE_IR_EXPR_ENTRY(lowerVarRef, claimVarRef, "Variable references"),

    // Assignment operators (13)
    QORE_IR_EXPR_ENTRY(lowerAssignment, claimAssignment, "Assignment (=)"),
    QORE_IR_EXPR_ENTRY(lowerPlusEquals, claimPlusEquals, "Addition assignment (+=)"),
    QORE_IR_EXPR_ENTRY(lowerMinusEquals, claimMinusEquals, "Subtraction assignment (-=)"),
    QORE_IR_EXPR_ENTRY(lowerDivideEquals, claimDivideEquals, "Division assignment (/=)"),
    QORE_IR_EXPR_ENTRY(lowerMultiplyEquals, claimMultiplyEquals, "Multiplication assignment (*=)"),
    QORE_IR_EXPR_ENTRY(lowerModuloEquals, claimModuloEquals, "Modulo assignment (%=)"),
    QORE_IR_EXPR_ENTRY(lowerAndEquals, claimAndEquals, "Binary AND assignment (&=)"),
    QORE_IR_EXPR_ENTRY(lowerOrEquals, claimOrEquals, "Binary OR assignment (|=)"),
    QORE_IR_EXPR_ENTRY(lowerXorEquals, claimXorEquals, "Binary XOR assignment (^=)"),
    QORE_IR_EXPR_ENTRY(lowerPreDecrement, claimPreDecrement, "Pre-decrement (--var)"),
    QORE_IR_EXPR_ENTRY(lowerPostDecrement, claimPostDecrement, "Post-decrement (var--)"),
    QORE_IR_EXPR_ENTRY(lowerPreIncrement, claimPreIncrement, "Pre-increment (++var)"),
    QORE_IR_EXPR_ENTRY(lowerPostIncrement, claimPostIncrement, "Post-increment (var++)"),

    // Binary operators (17)
    QORE_IR_EXPR_ENTRY(lowerPlus, claimPlus, "Addition (+)"),
    QORE_IR_EXPR_ENTRY(lowerMinus, claimMinus, "Subtraction (-)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalLessThan, claimLogicalLessThan, "Less than (<)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalNotEquals, claimLogicalNotEquals, "Not equal (!=)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalEquals, claimLogicalEquals, "Equal (==)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalAbsoluteNotEquals, claimLogicalAbsoluteNotEquals, "Strict not equal (!==)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalAbsoluteEquals, claimLogicalAbsoluteEquals, "Strict equal (===)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalLessThanOrEquals, claimLogicalLessThanOrEquals, "Less than or equal (<=)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalGreaterThan, claimLogicalGreaterThan, "Greater than (>)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalGreaterThanOrEquals, claimLogicalGreaterThanOrEquals,
        "Greater than or equal (>=)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalComparison, claimLogicalComparison, "Comparison (=~)"),
    QORE_IR_EXPR_ENTRY(lowerUnaryPlus, claimUnaryPlus, "Unary plus (+x)"),
    QORE_IR_EXPR_ENTRY(lowerUnaryMinus, claimUnaryMinus, "Unary minus (-x)"),
    QORE_IR_EXPR_ENTRY(lowerBinaryNot, claimBinaryNot, "Binary not (~x)"),
    QORE_IR_EXPR_ENTRY(lowerMultiplication, claimMultiplication, "Multiplication (*)"),
    QORE_IR_EXPR_ENTRY(lowerDivision, claimDivision, "Division (/)"),
    QORE_IR_EXPR_ENTRY(lowerModulo, claimModulo, "Modulo (%)"),

    // Bitwise operators (6)
    QORE_IR_EXPR_ENTRY(lowerBinaryAnd, claimBinaryAnd, "Binary AND (&)"),
    QORE_IR_EXPR_ENTRY(lowerBinaryOr, claimBinaryOr, "Binary OR (|)"),
    QORE_IR_EXPR_ENTRY(lowerBinaryXor, claimBinaryXor, "Binary XOR (^)"),
    QORE_IR_EXPR_ENTRY(lowerShiftLeft, claimShiftLeft, "Left shift (<<)"),
    QORE_IR_EXPR_ENTRY(lowerShiftRight, claimShiftRight, "Right shift (>>)"),
    QORE_IR_EXPR_ENTRY(lowerShiftLeftEquals, claimShiftLeftEquals, "Left shift assign (<<=)"),

    // More assignment/operators (1)
    QORE_IR_EXPR_ENTRY(lowerShiftRightEquals, claimShiftRightEquals, "Right shift assign (>>=)"),

    // Container/iteration operators (18)
    QORE_IR_EXPR_ENTRY(lowerRange, claimRange, "Range operator (..) or comma list in subscript"),
    QORE_IR_EXPR_ENTRY(lowerSquareBracketsRange, claimSquareBracketsRange, "Range subscript [..]"),
    QORE_IR_EXPR_ENTRY(lowerSquareBrackets, claimSquareBrackets, "Subscript operator []"),
    QORE_IR_EXPR_ENTRY(lowerHashObjectDereference, claimHashObjectDereference,
        "Hash/object member (.) and subscript"),
    QORE_IR_EXPR_ENTRY(lowerPop, claimPop, "Pop from list/hash"),
    QORE_IR_EXPR_ENTRY(lowerShift, claimShift, "Shift from list"),
    QORE_IR_EXPR_ENTRY(lowerPush, claimPush, "Push to list"),
    QORE_IR_EXPR_ENTRY(lowerUnshift, claimUnshift, "Unshift to list"),
    QORE_IR_EXPR_ENTRY(lowerSplice, claimSplice, "Splice list elements"),
    QORE_IR_EXPR_ENTRY(lowerExtract, claimExtract, "Extract hash/list elements"),
    QORE_IR_EXPR_ENTRY(lowerRemove, claimRemove, "Remove list elements"),
    QORE_IR_EXPR_ENTRY(lowerDelete, claimDelete, "Delete hash/object members"),
    QORE_IR_EXPR_ENTRY(lowerKeys, claimKeys, "Get hash keys"),
    QORE_IR_EXPR_ENTRY(lowerRegexExtract, claimRegexExtract, "Regex extract operator"),
    QORE_IR_EXPR_ENTRY(lowerRegexNMatch, claimRegexNMatch, "Regex not-match"),
    QORE_IR_EXPR_ENTRY(lowerRegexMatch, claimRegexMatch, "Regex match"),
    QORE_IR_EXPR_ENTRY(lowerRegexSubst, claimRegexSubst, "Regex substitution"),
    QORE_IR_EXPR_ENTRY(lowerInstanceOf, claimInstanceOf, "Instance check"),

    // String operators (3)
    QORE_IR_EXPR_ENTRY(lowerTrim, claimTrim, "String trim"),
    QORE_IR_EXPR_ENTRY(lowerChomp, claimChomp, "String chomp"),
    QORE_IR_EXPR_ENTRY(lowerTransliteration, claimTransliteration, "String transliteration"),

    // Background/list operations (2)
    QORE_IR_EXPR_ENTRY(lowerBackground, claimBackground, "Background operator"),
    QORE_IR_EXPR_ENTRY(lowerListAssignment, claimListAssignment, "List assignment"),

    // Test operators (2)
    QORE_IR_EXPR_ENTRY(lowerExists, claimExists, "Exists operator"),
    QORE_IR_EXPR_ENTRY(lowerElements, claimElements, "Elements operator"),

    // Complex operators (7)
    QORE_IR_EXPR_ENTRY(lowerDotEval, claimDotEval, "Dot evaluation"),
    QORE_IR_EXPR_ENTRY(lowerFoldr, claimFoldr, "Foldr operator"),
    QORE_IR_EXPR_ENTRY(lowerFoldl, claimFoldl, "Foldl operator"),
    QORE_IR_EXPR_ENTRY(lowerMap, claimMap, "Map operator"),
    QORE_IR_EXPR_ENTRY(lowerSelect, claimSelect, "Select operator"),
    QORE_IR_EXPR_ENTRY(lowerMapSelect, claimMapSelect, "Map-select operator"),
    QORE_IR_EXPR_ENTRY(lowerHashMap, claimHashMap, "Hash map operator"),

    // More complex operators (3)
    QORE_IR_EXPR_ENTRY(lowerHashMapSelect, claimHashMapSelect, "Hash map-select operator"),
    QORE_IR_EXPR_ENTRY(lowerIterate, claimIterate, "Iterate operator"),
    QORE_IR_EXPR_ENTRY(lowerStreaming, claimStreaming, "Streaming operator"),

    // Logical operators (3)
    QORE_IR_EXPR_ENTRY(lowerLogicalAnd, claimLogicalAnd, "Logical AND (&&)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalOr, claimLogicalOr, "Logical OR (||)"),
    QORE_IR_EXPR_ENTRY(lowerLogicalNot, claimLogicalNot, "Logical NOT (!)"),

    // Coalescing operators (2)
    QORE_IR_EXPR_ENTRY(lowerNullCoalescing, claimNullCoalescing, "Null coalescing operator"),
    QORE_IR_EXPR_ENTRY(lowerValueCoalescing, claimValueCoalescing, "Value coalescing (?:)"),

    // Ternary/conversion (3)
    QORE_IR_EXPR_ENTRY(lowerQuestionMark, claimQuestionMark, "Ternary operator (?:)"),
    QORE_IR_EXPR_ENTRY(lowerCast, claimCast, "Type cast"),
    QORE_IR_EXPR_ENTRY(lowerBuiltinTypeConversion, claimBuiltinTypeConversion, "Builtin type conversion"),

    // Function/method calls (4)
    QORE_IR_EXPR_ENTRY(lowerFunctionCall, claimFunctionCall, "Function call"),
    QORE_IR_EXPR_ENTRY(lowerCallReference, claimCallReference, "Call reference"),
    QORE_IR_EXPR_ENTRY(lowerSelfCall, claimSelfCall, "Self method call"),
    QORE_IR_EXPR_ENTRY(lowerStaticCall, claimStaticCall, "Static method call"),
};

#undef QORE_IR_EXPR_ENTRY

const size_t QORE_IR_EXPR_REGISTRY_SIZE = sizeof(QORE_IR_EXPR_REGISTRY) / sizeof(QORE_IR_EXPR_REGISTRY[0]);

bool qore_ir_validate_expr_registry(std::string& error) {
    for (size_t i = 0; i < QORE_IR_EXPR_REGISTRY_SIZE; ++i) {
        const auto& info = QORE_IR_EXPR_REGISTRY[i];
        if (!info.name || !*info.name) {
            error = "IR expression registry entry " + std::to_string(i) + " has no name";
            return false;
        }
        if (!info.claim) {
            error = "IR expression registry entry '" + std::string(info.name) + "' has no claim predicate";
            return false;
        }
        if (!info.handler) {
            error = "IR expression registry entry '" + std::string(info.name) + "' has no handler";
            return false;
        }
        if (!info.description || !*info.description) {
            error = "IR expression registry entry '" + std::string(info.name) + "' has no description";
            return false;
        }
    }
    return true;
}

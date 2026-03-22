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

// Include all handler functions
#include "QoreIRExprHandlers.cpp"

// Registry table: 80 expression handlers in dispatch order
const QoreIRExprHandlerInfo QORE_IR_EXPR_REGISTRY[] = {
    // Foundation handlers (4)
    { "lowerConstant", handler_lowerConstant, "Constants (string, number, etc.)" },
    { "lowerParseList", handler_lowerParseList, "List literals [...]" },
    { "lowerParseHash", handler_lowerParseHash, "Hash literals {...}" },
    { "lowerVarRef", handler_lowerVarRef, "Variable references" },

    // Assignment operators (13)
    { "lowerAssignment", handler_lowerAssignment, "Assignment (=)" },
    { "lowerPlusEquals", handler_lowerPlusEquals, "Addition assignment (+=)" },
    { "lowerMinusEquals", handler_lowerMinusEquals, "Subtraction assignment (-=)" },
    { "lowerDivideEquals", handler_lowerDivideEquals, "Division assignment (/=)" },
    { "lowerMultiplyEquals", handler_lowerMultiplyEquals, "Multiplication assignment (*=)" },
    { "lowerModuloEquals", handler_lowerModuloEquals, "Modulo assignment (%=)" },
    { "lowerAndEquals", handler_lowerAndEquals, "Binary AND assignment (&=)" },
    { "lowerOrEquals", handler_lowerOrEquals, "Binary OR assignment (|=)" },
    { "lowerXorEquals", handler_lowerXorEquals, "Binary XOR assignment (^=)" },
    { "lowerPreDecrement", handler_lowerPreDecrement, "Pre-decrement (--var)" },
    { "lowerPostDecrement", handler_lowerPostDecrement, "Post-decrement (var--)" },
    { "lowerPreIncrement", handler_lowerPreIncrement, "Pre-increment (++var)" },
    { "lowerPostIncrement", handler_lowerPostIncrement, "Post-increment (var++)" },

    // Binary operators (17)
    { "lowerPlus", handler_lowerPlus, "Addition (+)" },
    { "lowerMinus", handler_lowerMinus, "Subtraction (-)" },
    { "lowerLogicalLessThan", handler_lowerLogicalLessThan, "Less than (<)" },
    { "lowerLogicalNotEquals", handler_lowerLogicalNotEquals, "Not equal (!=)" },
    { "lowerLogicalEquals", handler_lowerLogicalEquals, "Equal (==)" },
    { "lowerLogicalAbsoluteNotEquals", handler_lowerLogicalAbsoluteNotEquals, "Strict not equal (!==)" },
    { "lowerLogicalAbsoluteEquals", handler_lowerLogicalAbsoluteEquals, "Strict equal (===)" },
    { "lowerLogicalLessThanOrEquals", handler_lowerLogicalLessThanOrEquals, "Less than or equal (<=)" },
    { "lowerLogicalGreaterThan", handler_lowerLogicalGreaterThan, "Greater than (>)" },
    { "lowerLogicalGreaterThanOrEquals", handler_lowerLogicalGreaterThanOrEquals, "Greater than or equal (>=)" },
    { "lowerLogicalComparison", handler_lowerLogicalComparison, "Comparison (=~)" },
    { "lowerUnaryPlus", handler_lowerUnaryPlus, "Unary plus (+x)" },
    { "lowerUnaryMinus", handler_lowerUnaryMinus, "Unary minus (-x)" },
    { "lowerBinaryNot", handler_lowerBinaryNot, "Binary not (~x)" },
    { "lowerMultiplication", handler_lowerMultiplication, "Multiplication (*)" },
    { "lowerDivision", handler_lowerDivision, "Division (/)" },
    { "lowerModulo", handler_lowerModulo, "Modulo (%)" },

    // Bitwise operators (6)
    { "lowerBinaryAnd", handler_lowerBinaryAnd, "Binary AND (&)" },
    { "lowerBinaryOr", handler_lowerBinaryOr, "Binary OR (|)" },
    { "lowerBinaryXor", handler_lowerBinaryXor, "Binary XOR (^)" },
    { "lowerShiftLeft", handler_lowerShiftLeft, "Left shift (<<)" },
    { "lowerShiftRight", handler_lowerShiftRight, "Right shift (>>)" },
    { "lowerShiftLeftEquals", handler_lowerShiftLeftEquals, "Left shift assign (<<=)" },

    // More assignment/operators (1)
    { "lowerShiftRightEquals", handler_lowerShiftRightEquals, "Right shift assign (>>=)" },

    // Container/iteration operators (18)
    { "lowerRange", handler_lowerRange, "Range operator (..) or comma list in subscript" },
    { "lowerSquareBracketsRange", handler_lowerSquareBracketsRange, "Range subscript [..]" },
    { "lowerSquareBrackets", handler_lowerSquareBrackets, "Subscript operator []" },
    { "lowerHashObjectDereference", handler_lowerHashObjectDereference, "Hash/object member (.) and subscript" },
    { "lowerPop", handler_lowerPop, "Pop from list/hash" },
    { "lowerShift", handler_lowerShift, "Shift from list" },
    { "lowerPush", handler_lowerPush, "Push to list" },
    { "lowerUnshift", handler_lowerUnshift, "Unshift to list" },
    { "lowerSplice", handler_lowerSplice, "Splice list elements" },
    { "lowerExtract", handler_lowerExtract, "Extract hash/list elements" },
    { "lowerRemove", handler_lowerRemove, "Remove list elements" },
    { "lowerDelete", handler_lowerDelete, "Delete hash/object members" },
    { "lowerKeys", handler_lowerKeys, "Get hash keys" },
    { "lowerRegexExtract", handler_lowerRegexExtract, "Regex extract operator" },
    { "lowerRegexNMatch", handler_lowerRegexNMatch, "Regex not-match" },
    { "lowerRegexMatch", handler_lowerRegexMatch, "Regex match" },
    { "lowerRegexSubst", handler_lowerRegexSubst, "Regex substitution" },
    { "lowerInstanceOf", handler_lowerInstanceOf, "Instance check" },

    // String operators (3)
    { "lowerTrim", handler_lowerTrim, "String trim" },
    { "lowerChomp", handler_lowerChomp, "String chomp" },
    { "lowerTransliteration", handler_lowerTransliteration, "String transliteration" },

    // Background/list operations (2)
    { "lowerBackground", handler_lowerBackground, "Background operator" },
    { "lowerListAssignment", handler_lowerListAssignment, "List assignment" },

    // Test operators (2)
    { "lowerExists", handler_lowerExists, "Exists operator" },
    { "lowerElements", handler_lowerElements, "Elements operator" },

    // Complex operators (7)
    { "lowerDotEval", handler_lowerDotEval, "Dot evaluation" },
    { "lowerFoldr", handler_lowerFoldr, "Foldr operator" },
    { "lowerFoldl", handler_lowerFoldl, "Foldl operator" },
    { "lowerMap", handler_lowerMap, "Map operator" },
    { "lowerSelect", handler_lowerSelect, "Select operator" },
    { "lowerMapSelect", handler_lowerMapSelect, "Map-select operator" },
    { "lowerHashMap", handler_lowerHashMap, "Hash map operator" },

    // More complex operators (1)
    { "lowerHashMapSelect", handler_lowerHashMapSelect, "Hash map-select operator" },

    // Logical operators (3)
    { "lowerLogicalAnd", handler_lowerLogicalAnd, "Logical AND (&&)" },
    { "lowerLogicalOr", handler_lowerLogicalOr, "Logical OR (||)" },
    { "lowerLogicalNot", handler_lowerLogicalNot, "Logical NOT (!)" },

    // Coalescing operators (2)
    { "lowerNullCoalescing", handler_lowerNullCoalescing, "Null coalescing (??)" },
    { "lowerValueCoalescing", handler_lowerValueCoalescing, "Value coalescing (?:)" },

    // Ternary/conversion (3)
    { "lowerQuestionMark", handler_lowerQuestionMark, "Ternary operator (?:)" },
    { "lowerCast", handler_lowerCast, "Type cast" },
    { "lowerBuiltinTypeConversion", handler_lowerBuiltinTypeConversion, "Builtin type conversion" },

    // Function/method calls (4)
    { "lowerFunctionCall", handler_lowerFunctionCall, "Function call" },
    { "lowerCallReference", handler_lowerCallReference, "Call reference" },
    { "lowerSelfCall", handler_lowerSelfCall, "Self method call" },
    { "lowerStaticCall", handler_lowerStaticCall, "Static method call" },
};

const size_t QORE_IR_EXPR_REGISTRY_SIZE = sizeof(QORE_IR_EXPR_REGISTRY) / sizeof(QORE_IR_EXPR_REGISTRY[0]);

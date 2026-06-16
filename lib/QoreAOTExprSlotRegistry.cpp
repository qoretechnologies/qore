/* -*- mode: c++ -*- */
/*
  QoreAOTExprSlotRegistry.cpp

  Expression slot metadata registry for AOT binary serialization

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

  Note that the Qore library is dual-licensed under a choice of two
  licenses.
*/

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/QoreAOTExprSlotRegistry.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/QorePlusOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/QoreSquareBracketsRangeOperatorNode.h"
#include "qore/intern/QoreExistsOperatorNode.h"
#include "qore/intern/QoreImplicitArgumentNode.h"
#include "qore/intern/QoreMinusOperatorNode.h"
#include "qore/intern/QoreKeysOperatorNode.h"
#include "qore/intern/QoreMultiplicationOperatorNode.h"
#include "qore/intern/QoreDivisionOperatorNode.h"
#include "qore/intern/QoreModuloOperatorNode.h"
#include "qore/intern/QoreBinaryAndOperatorNode.h"
#include "qore/intern/QoreBinaryOrOperatorNode.h"
#include "qore/intern/QoreBinaryXorOperatorNode.h"
#include "qore/intern/QoreShiftLeftOperatorNode.h"
#include "qore/intern/QoreShiftRightOperatorNode.h"
#include "qore/intern/QoreImplicitElementNode.h"
#include "qore/intern/QoreInstanceOfOperatorNode.h"
#include "qore/intern/QoreRegexMatchOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QoreRegex.h"
#include "qore/intern/QorePreIncrementOperatorNode.h"
#include "qore/intern/QorePreDecrementOperatorNode.h"
#include "qore/intern/QorePostIncrementOperatorNode.h"
#include "qore/intern/QorePostDecrementOperatorNode.h"
#include "qore/intern/QoreIntPostIncrementOperatorNode.h"
#include "qore/intern/QoreIntPostDecrementOperatorNode.h"
#include "qore/intern/QoreLogicalAndOperatorNode.h"
#include "qore/intern/QoreLogicalOrOperatorNode.h"
#include "qore/intern/QoreLogicalEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalAbsoluteNotEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOperatorNode.h"
#include "qore/intern/QoreLogicalLessThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalGreaterThanOrEqualsOperatorNode.h"
#include "qore/intern/QoreLogicalNotOperatorNode.h"
#include "qore/intern/QoreNullCoalescingOperatorNode.h"
#include "qore/intern/QoreValueCoalescingOperatorNode.h"
#include "qore/intern/QoreQuestionMarkOperatorNode.h"
#include "qore/intern/QoreFoldlOperatorNode.h"
#include "qore/intern/QoreIterateOperatorNode.h"
#include "qore/intern/QoreMapOperatorNode.h"
#include "qore/intern/QoreMapSelectOperatorNode.h"
#include "qore/intern/QoreHashMapOperatorNode.h"
#include "qore/intern/QoreHashMapSelectOperatorNode.h"
#include "qore/intern/QoreSelectOperatorNode.h"
#include "qore/intern/QoreStreamingOperatorNode.h"
#include "qore/intern/QoreRangeOperatorNode.h"
#include "qore/intern/QoreAssignmentOperatorNode.h"
#include "qore/intern/QoreElementsOperatorNode.h"
#include "qore/intern/QoreDeleteOperatorNode.h"
#include "qore/intern/QoreRemoveOperatorNode.h"
#include "qore/intern/QoreBackgroundOperatorNode.h"
#include "qore/intern/QoreTrimOperatorNode.h"
#include "qore/intern/QoreChompOperatorNode.h"
#include "qore/intern/QorePopOperatorNode.h"
#include "qore/intern/QoreShiftOperatorNode.h"
#include "qore/intern/QorePushOperatorNode.h"
#include "qore/intern/QoreUnshiftOperatorNode.h"
#include "qore/intern/QoreUnaryMinusOperatorNode.h"
#include "qore/intern/ContextrefNode.h"
#include "qore/intern/ContextRowNode.h"
#include "qore/intern/ComplexContextrefNode.h"
#include "qore/intern/CallReferenceCallNode.h"

class UserClosureVariant;
class UserSignature;
class LVarSet;
QoreIRFunction* lowerClosureForSerialization(const UserClosureVariant* variant);
bool qoreAOTWriteClosureCaptures(QoreAOTBinaryWriter& writer, const LVarSet* vlist,
    const QoreIRFunction* closure_ir, const std::vector<AOTLocalSlotId>& parent_locals);
void qoreAOTPruneClosureIRBodyLocals(QoreIRFunction* closure_ir, const UserSignature* sig,
    const LVarSet* vlist);

// ============================================================================
// Expression Slot Metadata Handlers (Phase 3.3)
// ============================================================================
// Handler implementations extracted from serializeSlotMaps() switch statement

// Include all handler implementations (Phase 3.3 extracted handlers)
#include "QoreAOTExprSlotHandlers.cpp"

// ============================================================================
// Registry Lookup Function
// ============================================================================

const QoreAOTExprSlotKindInfo* getAOTExprSlotKindInfo(uint8_t kind_byte) {
    if (kind_byte >= 256) {
        return nullptr;
    }
    return &AOT_EXPR_SLOT_KIND_REGISTRY[kind_byte];
}

// ============================================================================
// Complete Registry Table (256 entries)
// ============================================================================

const QoreAOTExprSlotKindInfo AOT_EXPR_SLOT_KIND_REGISTRY[256] = {
    // Index 0: Reserved/Unused
    {nullptr, 0, false, nullptr, nullptr},

    // 1: FUNC_CALL
    {"FUNC_CALL", 1, true, write_slot_FUNC_CALL, "Regular function call"},

    // 2: SELF_METHOD_CALL
    {"SELF_METHOD_CALL", 2, true, write_slot_SELF_METHOD_CALL, "Self method call"},

    // 3: STATIC_METHOD_CALL
    {"STATIC_METHOD_CALL", 3, true, write_slot_STATIC_METHOD_CALL, "Static method call"},

    // 4: NEW_OBJECT
    {"NEW_OBJECT", 4, true, write_slot_NEW_OBJECT, "New object constructor"},

    // 5: RUNTIME_CONST_REF
    {"RUNTIME_CONST_REF", 5, true, write_slot_RUNTIME_CONST_REF, "Runtime constant reference"},

    // 6: SELF_VARREF
    {"SELF_VARREF", 6, true, write_slot_SELF_VARREF, "Self variable reference"},

    // 7: LOCAL_VARREF
    {"LOCAL_VARREF", 7, true, write_slot_LOCAL_VARREF, "Local variable reference"},

    // 8: GLOBAL_VARREF
    {"GLOBAL_VARREF", 8, true, write_slot_GLOBAL_VARREF, "Global variable reference"},

    // 9: CONST_NUMBER
    {"CONST_NUMBER", 9, true, write_slot_CONST_NUMBER, "Number constant"},

    // 10: CONST_BINARY
    {"CONST_BINARY", 10, true, write_slot_CONST_BINARY, "Binary constant"},

    // 11: CLOSURE_CREATE
    {"CLOSURE_CREATE", 11, true, write_slot_CLOSURE_CREATE, "Closure/lambda"},

    // 12: CALL_REF
    {"CALL_REF", 12, true, write_slot_CALL_REF, "Call reference"},

    // 13: OBJ_METHOD_REF
    {"OBJ_METHOD_REF", 13, true, write_slot_OBJ_METHOD_REF, "Object method reference"},

    // 14: STATIC_VARREF
    {"STATIC_VARREF", 14, true, write_slot_STATIC_VARREF, "Static class variable"},

    // 15: SCOPED_NEW_OBJECT
    {"SCOPED_NEW_OBJECT", 15, true, write_slot_SCOPED_NEW_OBJECT, "Scoped new object"},

    // 16: HASHDECL_NEW
    {"HASHDECL_NEW", 16, true, write_slot_HASHDECL_NEW, "Hashdecl construction"},

    // 17: COMPLEX_HASH_NEW
    {"COMPLEX_HASH_NEW", 17, true, write_slot_COMPLEX_HASH_NEW, "Complex hash construction"},

    // 18: COMPLEX_LIST_NEW
    {"COMPLEX_LIST_NEW", 18, true, write_slot_COMPLEX_LIST_NEW, "Complex list construction"},

    // 19: CONST_ENUM
    {"CONST_ENUM", 19, true, write_slot_CONST_ENUM, "Enum constant"},

    // 20: CONST_STRING
    {"CONST_STRING", 20, true, write_slot_CONST_STRING, "String constant"},

    // 21: HASH_LITERAL
    {"HASH_LITERAL", 21, true, write_slot_HASH_LITERAL, "Hash literal"},

    // 22: HASH_DEREF
    {"HASH_DEREF", 22, true, write_slot_HASH_DEREF, "Hash/object dereference"},

    // 23: PARSE_REF
    {"PARSE_REF", 23, true, write_slot_PARSE_REF, "Parse reference"},

    // 24: CAST_HASHDECL
    {"CAST_HASHDECL", 24, true, write_slot_CAST, "Hashdecl cast"},

    // 25: CAST_COMPLEX_HASH
    {"CAST_COMPLEX_HASH", 25, true, write_slot_CAST, "Complex hash cast"},

    // 26: CAST_COMPLEX_LIST
    {"CAST_COMPLEX_LIST", 26, true, write_slot_CAST, "Complex list cast"},

    // 27: CAST_CLASS
    {"CAST_CLASS", 27, true, write_slot_CAST, "Class cast"},

    // 28: CAST_ENUM
    {"CAST_ENUM", 28, true, write_slot_CAST, "Enum cast"},

    // 29: CONST_INT
    {"CONST_INT", 29, true, write_slot_CONST_INT, "Integer constant"},

    // 30: CONST_FLOAT
    {"CONST_FLOAT", 30, true, write_slot_CONST_FLOAT, "Float constant"},

    // 31: CONST_BOOL
    {"CONST_BOOL", 31, true, write_slot_CONST_BOOL, "Boolean constant"},

    // 32: CONST_NOTHING
    {"CONST_NOTHING", 32, true, write_slot_CONST_NOTHING, "Nothing constant"},

    // 33: LIST_LITERAL
    {"LIST_LITERAL", 33, true, write_slot_LIST_LITERAL, "List literal"},

    // 34: CONST_NULL
    {"CONST_NULL", 34, true, write_slot_CONST_NULL, "NULL constant"},

    // 35: DOT_EVAL_TARGET
    {"DOT_EVAL_TARGET", 35, true, write_slot_DOT_EVAL_TARGET, "Dot-eval method target"},

    // 36: FUNC_CALL_REF
    {"FUNC_CALL_REF", 36, true, write_slot_FUNC_CALL_REF, "Function call reference"},

    // 37: BOUND_METHOD_REF
    {"BOUND_METHOD_REF", 37, true, write_slot_BOUND_METHOD_REF, "Bound method reference"},

    // 38: STATIC_METHOD_REF
    {"STATIC_METHOD_REF", 38, true, write_slot_STATIC_METHOD_REF, "Static method reference"},

    // 39: SELF_METHOD_REF
    {"SELF_METHOD_REF", 39, true, write_slot_SELF_METHOD_REF, "Self method reference"},

    // 40: OBJ_METHOD_REF_EXPR
    {"OBJ_METHOD_REF_EXPR", 40, true, write_slot_OBJ_METHOD_REF_EXPR, "Object method reference with expression"},

    // 41: CONST_VALUE
    {"CONST_VALUE", 41, true, write_slot_CONST_VALUE, "Serialized constant value"},

    // 42: PLUS
    {"PLUS", 42, true, write_slot_PLUS, "Plus operator"},

    // 43: SQUARE_BRACKET
    {"SQUARE_BRACKET", 43, true, write_slot_SQUARE_BRACKET, "Square-bracket operator"},

    // 44: PARSE_HASH
    {"PARSE_HASH", 44, true, write_slot_PARSE_HASH, "Parse hash literal"},

    // 45: EXISTS
    {"EXISTS", 45, true, write_slot_EXISTS, "Exists operator"},

    // 46: IMPLICIT_ARG
    {"IMPLICIT_ARG", 46, true, write_slot_IMPLICIT_ARG, "Implicit argument reference"},

    // 47: MINUS
    {"MINUS", 47, true, write_slot_MINUS, "Minus operator"},

    // 48: KEYS
    {"KEYS", 48, true, write_slot_KEYS, "Keys operator"},

    // 49: MULTIPLY
    {"MULTIPLY", 49, true, write_slot_MULTIPLY, "Multiplication operator"},

    // 50: DIVIDE
    {"DIVIDE", 50, true, write_slot_DIVIDE, "Division operator"},

    // 51: MODULO
    {"MODULO", 51, true, write_slot_MODULO, "Modulo operator"},

    // 52: IMPLICIT_ELEM
    {"IMPLICIT_ELEM", 52, true, write_slot_IMPLICIT_ELEM, "Implicit element reference"},

    // 53: INSTANCEOF
    {"INSTANCEOF", 53, true, write_slot_INSTANCEOF, "Instanceof operator"},

    // 54: REGEX_MATCH
    {"REGEX_MATCH", 54, true, write_slot_REGEX_MATCH, "Regex match operator"},

    // 55: REGEX_NMATCH
    {"REGEX_NMATCH", 55, true, write_slot_REGEX_NMATCH, "Regex negative match operator"},

    // 56: REGEX_EXTRACT
    {"REGEX_EXTRACT", 56, true, write_slot_REGEX_EXTRACT, "Regex extract operator"},

    // 57: PRE_INC
    {"PRE_INC", 57, true, write_slot_PRE_INC, "Pre-increment operator"},

    // 58: PRE_DEC
    {"PRE_DEC", 58, true, write_slot_PRE_DEC, "Pre-decrement operator"},

    // 59: POST_INC
    {"POST_INC", 59, true, write_slot_POST_INC, "Post-increment operator"},

    // 60: POST_DEC
    {"POST_DEC", 60, true, write_slot_POST_DEC, "Post-decrement operator"},

    // 61: LOG_EQ
    {"LOG_EQ", 61, true, write_slot_LOG_EQ, "Logical equality operator"},

    // 62: LOG_NE
    {"LOG_NE", 62, true, write_slot_LOG_NE, "Logical not-equals operator"},

    // 63: LOG_NOT
    {"LOG_NOT", 63, true, write_slot_LOG_NOT, "Logical not operator"},

    // 64: TRIM
    {"TRIM", 64, true, write_slot_TRIM, "Trim operator"},

    // 65: CHOMP
    {"CHOMP", 65, true, write_slot_CHOMP, "Chomp operator"},

    // 66: POP
    {"POP", 66, true, write_slot_POP, "Pop operator"},

    // 67: SHIFT
    {"SHIFT", 67, true, write_slot_SHIFT, "Shift operator"},

    // 68: PUSH
    {"PUSH", 68, true, write_slot_PUSH, "Push operator"},

    // 69: UNSHIFT
    {"UNSHIFT", 69, true, write_slot_UNSHIFT, "Unshift operator"},

    // 70: ELEMENTS
    {"ELEMENTS", 70, true, write_slot_ELEMENTS, "Elements operator"},

    // 71: DELETE
    {"DELETE", 71, true, write_slot_DELETE, "Delete operator"},

    // 72: REMOVE
    {"REMOVE", 72, true, write_slot_REMOVE, "Remove operator"},

    // 73: BACKGROUND
    {"BACKGROUND", 73, true, write_slot_BACKGROUND, "Background operator"},

    // 74: CONTEXT_REF
    {"CONTEXT_REF", 74, true, write_slot_CONTEXT_REF, "Context member reference"},

    // 75: CONTEXT_ROW
    {"CONTEXT_ROW", 75, true, write_slot_CONTEXT_ROW, "Context row reference"},

    // 76: COMPLEX_CONTEXT_REF
    {"COMPLEX_CONTEXT_REF", 76, true, write_slot_COMPLEX_CONTEXT_REF, "Named context member reference"},

    // 77: NULL_COAL
    {"NULL_COAL", 77, true, write_slot_NULL_COAL, "Null coalescing operator"},

    // 78: VALUE_COAL
    {"VALUE_COAL", 78, true, write_slot_VALUE_COAL, "Value coalescing operator"},

    // 79: QUESTION
    {"QUESTION", 79, true, write_slot_QUESTION, "Ternary operator"},

    // 80: FOLDL
    {"FOLDL", 80, true, write_slot_FOLDL, "Fold-left operator"},

    // 81: FOLDR
    {"FOLDR", 81, true, write_slot_FOLDR, "Fold-right operator"},

    // 82: MAP
    {"MAP", 82, true, write_slot_MAP, "Map operator"},

    // 83: MAP_SELECT
    {"MAP_SELECT", 83, true, write_slot_MAP_SELECT, "Map-select operator"},

    // 84: HASH_MAP
    {"HASH_MAP", 84, true, write_slot_HASH_MAP, "Hash map operator"},

    // 85: HASH_MAP_SELECT
    {"HASH_MAP_SELECT", 85, true, write_slot_HASH_MAP_SELECT, "Hash map-select operator"},

    // 86: SELECT
    {"SELECT", 86, true, write_slot_SELECT, "Select operator"},

    // 87: LOG_LT
    {"LOG_LT", 87, true, write_slot_LOG_LT, "Logical less-than operator"},

    // 88: LOG_GT
    {"LOG_GT", 88, true, write_slot_LOG_GT, "Logical greater-than operator"},

    // 89: LOG_LE
    {"LOG_LE", 89, true, write_slot_LOG_LE, "Logical less-than-or-equals operator"},

    // 90: LOG_GE
    {"LOG_GE", 90, true, write_slot_LOG_GE, "Logical greater-than-or-equals operator"},

    // 91: LOG_AND
    {"LOG_AND", 91, true, write_slot_LOG_AND, "Logical AND operator"},

    // 92: LOG_OR
    {"LOG_OR", 92, true, write_slot_LOG_OR, "Logical OR operator"},

    // 93: CALLREF_CALL
    {"CALLREF_CALL", 93, true, write_slot_CALLREF_CALL, "Call reference call"},

    // 94: RANGE
    {"RANGE", 94, true, write_slot_RANGE, "Range operator"},

    // 95: ASSIGN
    {"ASSIGN", 95, true, write_slot_ASSIGN, "Assignment operator"},

    // 96: CAST_SCALAR
    {"CAST_SCALAR", 96, true, write_slot_CAST, "Scalar cast"},

    // 97: BIT_AND
    {"BIT_AND", 97, true, write_slot_BIT_AND, "Bitwise AND operator"},

    // 98: BIT_OR
    {"BIT_OR", 98, true, write_slot_BIT_OR, "Bitwise OR operator"},

    // 99: BIT_XOR
    {"BIT_XOR", 99, true, write_slot_BIT_XOR, "Bitwise XOR operator"},

    // 100: SHIFT_LEFT
    {"SHIFT_LEFT", 100, true, write_slot_SHIFT_LEFT, "Left shift operator"},

    // 101: SHIFT_RIGHT
    {"SHIFT_RIGHT", 101, true, write_slot_SHIFT_RIGHT, "Right shift operator"},

    // 102: SQUARE_BRACKET_RANGE
    {"SQUARE_BRACKET_RANGE", 102, true, write_slot_SQUARE_BRACKET_RANGE, "Range subscript operator"},

    // 103: DOT_EVAL_EXPR
    {"DOT_EVAL_EXPR", 103, true, write_slot_DOT_EVAL_EXPR, "Full dot-eval expression slot payload"},
    // 104: UNARY_MINUS
    {"UNARY_MINUS", 104, true, write_slot_UNARY_MINUS, "Unary minus operator"},
    // 105: LOG_AEQ
    {"LOG_AEQ", 105, true, write_slot_LOG_AEQ, "Logical absolute equality operator"},

    // 106: LOG_ANE
    {"LOG_ANE", 106, true, write_slot_LOG_ANE, "Logical absolute not-equals operator"},

    {"COMPLEX_BUFFER_NEW", 107, true, write_slot_COMPLEX_BUFFER_NEW, "Complex buffer construction"},
    {"ITERATE", 108, true, write_slot_ITERATE, "Iterate operator"},
    {"STREAMING", 109, true, write_slot_STREAMING, "Streaming operator"},
    {nullptr, 110, false, nullptr, nullptr},
    {nullptr, 111, false, nullptr, nullptr},
    {nullptr, 112, false, nullptr, nullptr},
    {nullptr, 113, false, nullptr, nullptr},
    {nullptr, 114, false, nullptr, nullptr},
    {nullptr, 115, false, nullptr, nullptr},
    {nullptr, 116, false, nullptr, nullptr},
    {nullptr, 117, false, nullptr, nullptr},
    {nullptr, 118, false, nullptr, nullptr},
    {nullptr, 119, false, nullptr, nullptr},
    {nullptr, 120, false, nullptr, nullptr},
    {nullptr, 121, false, nullptr, nullptr},
    {nullptr, 122, false, nullptr, nullptr},
    {nullptr, 123, false, nullptr, nullptr},
    {nullptr, 124, false, nullptr, nullptr},
    {nullptr, 125, false, nullptr, nullptr},
    {nullptr, 126, false, nullptr, nullptr},
    {nullptr, 127, false, nullptr, nullptr},
    {nullptr, 128, false, nullptr, nullptr},
    {nullptr, 129, false, nullptr, nullptr},
    {nullptr, 130, false, nullptr, nullptr},
    {nullptr, 131, false, nullptr, nullptr},
    {nullptr, 132, false, nullptr, nullptr},
    {nullptr, 133, false, nullptr, nullptr},
    {nullptr, 134, false, nullptr, nullptr},
    {nullptr, 135, false, nullptr, nullptr},
    {nullptr, 136, false, nullptr, nullptr},
    {nullptr, 137, false, nullptr, nullptr},
    {nullptr, 138, false, nullptr, nullptr},
    {nullptr, 139, false, nullptr, nullptr},
    {nullptr, 140, false, nullptr, nullptr},
    {nullptr, 141, false, nullptr, nullptr},
    {nullptr, 142, false, nullptr, nullptr},
    {nullptr, 143, false, nullptr, nullptr},
    {nullptr, 144, false, nullptr, nullptr},
    {nullptr, 145, false, nullptr, nullptr},
    {nullptr, 146, false, nullptr, nullptr},
    {nullptr, 147, false, nullptr, nullptr},
    {nullptr, 148, false, nullptr, nullptr},
    {nullptr, 149, false, nullptr, nullptr},
    {nullptr, 150, false, nullptr, nullptr},
    {nullptr, 151, false, nullptr, nullptr},
    {nullptr, 152, false, nullptr, nullptr},
    {nullptr, 153, false, nullptr, nullptr},
    {nullptr, 154, false, nullptr, nullptr},
    {nullptr, 155, false, nullptr, nullptr},
    {nullptr, 156, false, nullptr, nullptr},
    {nullptr, 157, false, nullptr, nullptr},
    {nullptr, 158, false, nullptr, nullptr},
    {nullptr, 159, false, nullptr, nullptr},
    {nullptr, 160, false, nullptr, nullptr},
    {nullptr, 161, false, nullptr, nullptr},
    {nullptr, 162, false, nullptr, nullptr},
    {nullptr, 163, false, nullptr, nullptr},
    {nullptr, 164, false, nullptr, nullptr},
    {nullptr, 165, false, nullptr, nullptr},
    {nullptr, 166, false, nullptr, nullptr},
    {nullptr, 167, false, nullptr, nullptr},
    {nullptr, 168, false, nullptr, nullptr},
    {nullptr, 169, false, nullptr, nullptr},
    {nullptr, 170, false, nullptr, nullptr},
    {nullptr, 171, false, nullptr, nullptr},
    {nullptr, 172, false, nullptr, nullptr},
    {nullptr, 173, false, nullptr, nullptr},
    {nullptr, 174, false, nullptr, nullptr},
    {nullptr, 175, false, nullptr, nullptr},
    {nullptr, 176, false, nullptr, nullptr},
    {nullptr, 177, false, nullptr, nullptr},
    {nullptr, 178, false, nullptr, nullptr},
    {nullptr, 179, false, nullptr, nullptr},
    {nullptr, 180, false, nullptr, nullptr},
    {nullptr, 181, false, nullptr, nullptr},
    {nullptr, 182, false, nullptr, nullptr},
    {nullptr, 183, false, nullptr, nullptr},
    {nullptr, 184, false, nullptr, nullptr},
    {nullptr, 185, false, nullptr, nullptr},
    {nullptr, 186, false, nullptr, nullptr},
    {nullptr, 187, false, nullptr, nullptr},
    {nullptr, 188, false, nullptr, nullptr},
    {nullptr, 189, false, nullptr, nullptr},
    {nullptr, 190, false, nullptr, nullptr},
    {nullptr, 191, false, nullptr, nullptr},
    {nullptr, 192, false, nullptr, nullptr},
    {nullptr, 193, false, nullptr, nullptr},
    {nullptr, 194, false, nullptr, nullptr},
    {nullptr, 195, false, nullptr, nullptr},
    {nullptr, 196, false, nullptr, nullptr},
    {nullptr, 197, false, nullptr, nullptr},
    {nullptr, 198, false, nullptr, nullptr},
    {nullptr, 199, false, nullptr, nullptr},
    {nullptr, 200, false, nullptr, nullptr},
    {nullptr, 201, false, nullptr, nullptr},
    {nullptr, 202, false, nullptr, nullptr},
    {nullptr, 203, false, nullptr, nullptr},
    {nullptr, 204, false, nullptr, nullptr},
    {nullptr, 205, false, nullptr, nullptr},
    {nullptr, 206, false, nullptr, nullptr},
    {nullptr, 207, false, nullptr, nullptr},
    {nullptr, 208, false, nullptr, nullptr},
    {nullptr, 209, false, nullptr, nullptr},
    {nullptr, 210, false, nullptr, nullptr},
    {nullptr, 211, false, nullptr, nullptr},
    {nullptr, 212, false, nullptr, nullptr},
    {nullptr, 213, false, nullptr, nullptr},
    {nullptr, 214, false, nullptr, nullptr},
    {nullptr, 215, false, nullptr, nullptr},
    {nullptr, 216, false, nullptr, nullptr},
    {nullptr, 217, false, nullptr, nullptr},
    {nullptr, 218, false, nullptr, nullptr},
    {nullptr, 219, false, nullptr, nullptr},
    {nullptr, 220, false, nullptr, nullptr},
    {nullptr, 221, false, nullptr, nullptr},
    {nullptr, 222, false, nullptr, nullptr},
    {nullptr, 223, false, nullptr, nullptr},
    {nullptr, 224, false, nullptr, nullptr},
    {nullptr, 225, false, nullptr, nullptr},
    {nullptr, 226, false, nullptr, nullptr},
    {nullptr, 227, false, nullptr, nullptr},
    {nullptr, 228, false, nullptr, nullptr},
    {nullptr, 229, false, nullptr, nullptr},
    {nullptr, 230, false, nullptr, nullptr},
    {nullptr, 231, false, nullptr, nullptr},
    {nullptr, 232, false, nullptr, nullptr},
    {nullptr, 233, false, nullptr, nullptr},
    {nullptr, 234, false, nullptr, nullptr},
    {nullptr, 235, false, nullptr, nullptr},
    {nullptr, 236, false, nullptr, nullptr},
    {nullptr, 237, false, nullptr, nullptr},
    {nullptr, 238, false, nullptr, nullptr},
    {nullptr, 239, false, nullptr, nullptr},
    {nullptr, 240, false, nullptr, nullptr},
    {nullptr, 241, false, nullptr, nullptr},
    {nullptr, 242, false, nullptr, nullptr},
    {nullptr, 243, false, nullptr, nullptr},
    {nullptr, 244, false, nullptr, nullptr},
    {nullptr, 245, false, nullptr, nullptr},
    {nullptr, 246, false, nullptr, nullptr},
    {nullptr, 247, false, nullptr, nullptr},
    {nullptr, 248, false, nullptr, nullptr},
    {nullptr, 249, false, nullptr, nullptr},
    {nullptr, 250, false, nullptr, nullptr},
    {nullptr, 251, false, nullptr, nullptr},
    {nullptr, 252, false, nullptr, nullptr},
    {nullptr, 253, false, nullptr, nullptr},

    // 254: EXPR_TREE
    {"EXPR_TREE", 254, true, write_slot_EXPR_TREE, "Recursive expression tree"},

    // 255: GENERIC_EVAL
    {"GENERIC_EVAL", 255, true, write_slot_GENERIC_EVAL, "Unsupported expression (AOT compile error)"}
};

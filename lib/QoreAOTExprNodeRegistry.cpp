/* -*- mode: c++ -*- */
/*
  QoreAOTExprNodeRegistry.cpp

  Registry table for expression tree node deserialization handlers

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

// Include all handler implementations
#include "qore/intern/QoreJITIncludes.h"
#include "QoreAOTExprNodeHandlers.cpp"

// 256-entry registry table (one entry per possible uint8_t value)
// Entries for defined kinds point to their handlers; undefined kinds have null entries
const QoreAOTExprNodeKindInfo AOT_EXPR_NODE_KIND_REGISTRY[256] = {
    // [0-9] Leaf constants
    {"EN_NOTHING",  0,   true, read_node_EN_NOTHING,  "Nothing value"},
    {"EN_NULL",     1,   true, read_node_EN_NULL,     "Null value"},
    {"EN_INT",      2,   true, read_node_EN_INT,      "Integer constant"},
    {"EN_FLOAT",    3,   true, read_node_EN_FLOAT,    "Float constant"},
    {"EN_STRING",   4,   true, read_node_EN_STRING,   "String constant"},
    {"EN_BOOL",     5,   true, read_node_EN_BOOL,     "Boolean constant"},
    {"EN_NUMBER",   6,   true, read_node_EN_NUMBER,   "Number constant"},
    {"EN_BINARY",   7,   true, read_node_EN_BINARY,   "Binary constant"},
    {"EN_DATE",     8,   true, read_node_EN_DATE,     "Date/time constant"},
    {"EN_ENUM",     9,   true, read_node_EN_ENUM,     "Enum constant"},

    // [10-14] Variable references
    {"EN_LOCAL_VAR", 10, true, read_node_EN_LOCAL_VAR, "Local variable reference"},
    {"EN_GLOBAL_VAR", 11, true, read_node_EN_GLOBAL_VAR, "Global variable reference"},
    {"EN_SELF_REF", 12, true, read_node_EN_SELF_REF, "Self member reference"},
    {"EN_STATIC_VAR", 13, true, read_node_EN_STATIC_VAR, "Static class variable"},
    {"EN_CONST_REF", 14, true, read_node_EN_CONST_REF, "Constant reference"},

    // [15-19] Undefined
    {nullptr, 15, false, nullptr, nullptr},
    {nullptr, 16, false, nullptr, nullptr},
    {nullptr, 17, false, nullptr, nullptr},
    {nullptr, 18, false, nullptr, nullptr},
    {nullptr, 19, false, nullptr, nullptr},

    // [20-26] Call nodes
    {"EN_FUNC_CALL", 20, true, read_node_EN_FUNC_CALL, "Function call"},
    {"EN_SELF_CALL", 21, true, read_node_EN_SELF_CALL, "Self method call"},
    {"EN_STATIC_CALL", 22, true, read_node_EN_STATIC_CALL, "Static method call"},
    {"EN_DOT_EVAL", 23, true, read_node_EN_DOT_EVAL, "Dot evaluation"},
    {"EN_NEW", 24, true, read_node_EN_NEW, "Object construction"},
    {"EN_CALLREF_CALL", 25, true, read_node_EN_CALLREF_CALL, "Call reference"},
    {"EN_SCOPED_NEW", 26, true, read_node_EN_SCOPED_NEW, "Scoped object construction"},

    // [27-29] Undefined
    {nullptr, 27, false, nullptr, nullptr},
    {nullptr, 28, false, nullptr, nullptr},
    {nullptr, 29, false, nullptr, nullptr},

    // [30-31] Access operators
    {"EN_HASH_DEREF", 30, true, read_node_EN_HASH_DEREF, "Hash dereference"},
    {"EN_SQUARE_BRKT", 31, true, read_node_EN_SQUARE_BRKT, "Square bracket access"},

    // [32-39] Undefined
    {nullptr, 32, false, nullptr, nullptr},
    {nullptr, 33, false, nullptr, nullptr},
    {nullptr, 34, false, nullptr, nullptr},
    {nullptr, 35, false, nullptr, nullptr},
    {nullptr, 36, false, nullptr, nullptr},
    {nullptr, 37, false, nullptr, nullptr},
    {nullptr, 38, false, nullptr, nullptr},
    {nullptr, 39, false, nullptr, nullptr},

    // [40-55] Unary operators (note: 46 is undefined)
    {"EN_KEYS", 40, true, read_node_EN_KEYS, "Extract keys operator"},
    {"EN_ELEMENTS", 41, true, read_node_EN_ELEMENTS, "Extract elements operator"},
    {"EN_EXISTS", 42, true, read_node_EN_EXISTS, "Existence check operator"},
    {"EN_DELETE", 43, true, read_node_EN_DELETE, "Delete operator"},
    {"EN_REMOVE", 44, true, read_node_EN_REMOVE, "Remove operator"},
    {"EN_BACKGROUND", 45, true, read_node_EN_BACKGROUND, "Background operator"},
    {nullptr, 46, false, nullptr, nullptr},  // EN_RESERVED_46
    {"EN_TRIM", 47, true, read_node_EN_TRIM, "Trim operator"},
    {"EN_CHOMP", 48, true, read_node_EN_CHOMP, "Chomp operator"},
    {"EN_POP", 49, true, read_node_EN_POP, "Pop operator"},
    {"EN_INSTANCEOF", 50, true, read_node_EN_INSTANCEOF, "Instance check operator"},
    {"EN_UNARY_MINUS", 51, true, read_node_EN_UNARY_MINUS, "Unary minus operator"},
    {"EN_UNARY_PLUS", 52, true, read_node_EN_UNARY_PLUS, "Unary plus operator"},
    {"EN_LOG_NOT", 53, true, read_node_EN_LOG_NOT, "Logical not operator"},
    {"EN_BIT_NOT", 54, true, read_node_EN_BIT_NOT, "Bitwise not operator"},
    {"EN_SHIFT", 55, true, read_node_EN_SHIFT, "Shift operator"},

    // [56-59] Undefined
    {nullptr, 56, false, nullptr, nullptr},
    {nullptr, 57, false, nullptr, nullptr},
    {nullptr, 58, false, nullptr, nullptr},
    {nullptr, 59, false, nullptr, nullptr},

    // [60-87] Binary operators & assignments (grouped in original switch)
    {"EN_PUSH", 60, true, read_node_EN_PUSH, "Push operator"},
    {"EN_UNSHIFT", 61, true, read_node_EN_UNSHIFT, "Unshift operator"},
    {"EN_LIST_ASSIGN", 62, true, read_node_EN_LIST_ASSIGN, "List assignment operator"},
    {"EN_PLUS", 63, true, read_node_EN_PLUS, "Addition operator"},
    {"EN_MINUS", 64, true, read_node_EN_MINUS, "Subtraction operator"},
    {"EN_MULTIPLY", 65, true, read_node_EN_MULTIPLY, "Multiplication operator"},
    {"EN_DIVIDE", 66, true, read_node_EN_DIVIDE, "Division operator"},
    {"EN_MODULO", 67, true, read_node_EN_MODULO, "Modulo operator"},
    {"EN_SHIFT_LEFT", 68, true, read_node_EN_SHIFT_LEFT, "Left shift operator"},
    {"EN_SHIFT_RIGHT", 69, true, read_node_EN_SHIFT_RIGHT, "Right shift operator"},
    {"EN_BIT_AND", 70, true, read_node_EN_BIT_AND, "Bitwise AND operator"},
    {"EN_BIT_OR", 71, true, read_node_EN_BIT_OR, "Bitwise OR operator"},
    {"EN_BIT_XOR", 72, true, read_node_EN_BIT_XOR, "Bitwise XOR operator"},
    {"EN_LOG_CMP", 73, true, read_node_EN_LOG_CMP, "Logical comparison operator"},
    {"EN_LOG_AND", 74, true, read_node_EN_LOG_AND, "Logical AND operator"},
    {"EN_LOG_OR", 75, true, read_node_EN_LOG_OR, "Logical OR operator"},
    {"EN_LOG_EQ", 76, true, read_node_EN_LOG_EQ, "Equality operator"},
    {"EN_LOG_NE", 77, true, read_node_EN_LOG_NE, "Not equal operator"},
    {"EN_LOG_AEQ", 78, true, read_node_EN_LOG_AEQ, "Absolute equality operator"},
    {"EN_LOG_ANE", 79, true, read_node_EN_LOG_ANE, "Absolute inequality operator"},
    {"EN_LOG_LT", 80, true, read_node_EN_LOG_LT, "Less than operator"},
    {"EN_LOG_GT", 81, true, read_node_EN_LOG_GT, "Greater than operator"},
    {"EN_LOG_LE", 82, true, read_node_EN_LOG_LE, "Less than or equal operator"},
    {"EN_LOG_GE", 83, true, read_node_EN_LOG_GE, "Greater than or equal operator"},
    {"EN_NULL_COAL", 84, true, read_node_EN_NULL_COAL, "Null coalescing operator"},
    {"EN_VAL_COAL", 85, true, read_node_EN_VAL_COAL, "Value coalescing operator"},
    {"EN_QUESTION", 86, true, read_node_EN_QUESTION, "Ternary operator"},
    {"EN_RANGE", 87, true, read_node_EN_RANGE, "Range operator"},

    // [88-89] Undefined
    {nullptr, 88, false, nullptr, nullptr},
    {nullptr, 89, false, nullptr, nullptr},

    // [90-94] Regex operators
    {"EN_REGEX_MATCH", 90, true, read_node_EN_REGEX_MATCH, "Regex match operator"},
    {"EN_REGEX_NMATCH", 91, true, read_node_EN_REGEX_NMATCH, "Regex not-match operator"},
    {"EN_REGEX_EXTRACT", 92, true, read_node_EN_REGEX_EXTRACT, "Regex extract operator"},
    {"EN_REGEX_SUBST", 93, true, read_node_EN_REGEX_SUBST, "Regex substitution operator"},
    {"EN_TRANSLIT", 94, true, read_node_EN_TRANSLIT, "Transliteration operator"},

    // [95-99] Undefined
    {nullptr, 95, false, nullptr, nullptr},
    {nullptr, 96, false, nullptr, nullptr},
    {nullptr, 97, false, nullptr, nullptr},
    {nullptr, 98, false, nullptr, nullptr},
    {nullptr, 99, false, nullptr, nullptr},

    // [100-105] Method references & closure
    {"EN_OBJ_METH_REF", 100, true, read_node_EN_OBJ_METH_REF, "Object method reference"},
    {"EN_SELF_METH_REF", 101, true, read_node_EN_SELF_METH_REF, "Self method reference"},
    {"EN_CLOSURE", 102, true, read_node_EN_CLOSURE, "Closure node"},
    {"EN_FUNC_REF", 103, true, read_node_EN_FUNC_REF, "Function reference"},
    {"EN_STATIC_METH_REF", 104, true, read_node_EN_STATIC_METH_REF, "Static method reference"},
    {"EN_BOUND_METH_REF", 105, true, read_node_EN_BOUND_METH_REF, "Bound method reference"},

    // [106-109] Undefined
    {nullptr, 106, false, nullptr, nullptr},
    {nullptr, 107, false, nullptr, nullptr},
    {nullptr, 108, false, nullptr, nullptr},
    {nullptr, 109, false, nullptr, nullptr},

    // [110-120] Assignment operators (part of grouped case in original switch)
    {"EN_ASSIGN", 110, true, read_node_EN_ASSIGN, "Assignment operator"},
    {"EN_PLUS_EQ", 111, true, read_node_EN_PLUS_EQ, "Plus-equals operator"},
    {"EN_MINUS_EQ", 112, true, read_node_EN_MINUS_EQ, "Minus-equals operator"},
    {"EN_MULTIPLY_EQ", 113, true, read_node_EN_MULTIPLY_EQ, "Multiply-equals operator"},
    {"EN_DIVIDE_EQ", 114, true, read_node_EN_DIVIDE_EQ, "Divide-equals operator"},
    {"EN_MODULO_EQ", 115, true, read_node_EN_MODULO_EQ, "Modulo-equals operator"},
    {"EN_AND_EQ", 116, true, read_node_EN_AND_EQ, "AND-equals operator"},
    {"EN_OR_EQ", 117, true, read_node_EN_OR_EQ, "OR-equals operator"},
    {"EN_XOR_EQ", 118, true, read_node_EN_XOR_EQ, "XOR-equals operator"},
    {"EN_SHL_EQ", 119, true, read_node_EN_SHL_EQ, "Shift-left-equals operator"},
    {"EN_SHR_EQ", 120, true, read_node_EN_SHR_EQ, "Shift-right-equals operator"},

    // [121-124] Pre/post increment/decrement
    {"EN_PRE_INC", 121, true, read_node_EN_PRE_INC, "Pre-increment operator"},
    {"EN_PRE_DEC", 122, true, read_node_EN_PRE_DEC, "Pre-decrement operator"},
    {"EN_POST_INC", 123, true, read_node_EN_POST_INC, "Post-increment operator"},
    {"EN_POST_DEC", 124, true, read_node_EN_POST_DEC, "Post-decrement operator"},

    // [125-129] Undefined
    {nullptr, 125, false, nullptr, nullptr},
    {nullptr, 126, false, nullptr, nullptr},
    {nullptr, 127, false, nullptr, nullptr},
    {nullptr, 128, false, nullptr, nullptr},
    {nullptr, 129, false, nullptr, nullptr},

    // [130-132] Multi-child operators
    {"EN_EXTRACT", 130, true, read_node_EN_EXTRACT, "Extract operator"},
    {"EN_SPLICE", 131, true, read_node_EN_SPLICE, "Splice operator"},
    {"EN_PARSE_LIST", 132, true, read_node_EN_PARSE_LIST, "Parse list node"},

    // [133-139] Undefined
    {nullptr, 133, false, nullptr, nullptr},
    {nullptr, 134, false, nullptr, nullptr},
    {nullptr, 135, false, nullptr, nullptr},
    {nullptr, 136, false, nullptr, nullptr},
    {nullptr, 137, false, nullptr, nullptr},
    {nullptr, 138, false, nullptr, nullptr},
    {nullptr, 139, false, nullptr, nullptr},

    // [140] Cast operator
    {"EN_CAST", 140, true, read_node_EN_CAST, "Cast operator"},

    // [141-149] Undefined
    {nullptr, 141, false, nullptr, nullptr},
    {nullptr, 142, false, nullptr, nullptr},
    {nullptr, 143, false, nullptr, nullptr},
    {nullptr, 144, false, nullptr, nullptr},
    {nullptr, 145, false, nullptr, nullptr},
    {nullptr, 146, false, nullptr, nullptr},
    {nullptr, 147, false, nullptr, nullptr},
    {nullptr, 148, false, nullptr, nullptr},
    {nullptr, 149, false, nullptr, nullptr},

    // [150-165] Collection and special operators
    {"EN_LIST", 150, true, read_node_EN_LIST, "List literal"},
    {"EN_HASH", 151, true, read_node_EN_HASH, "Hash literal"},
    {"EN_IMPLICIT_ARG", 152, true, read_node_EN_IMPLICIT_ARG, "Implicit argument reference"},
    {"EN_IMPLICIT_ELEM", 153, true, read_node_EN_IMPLICIT_ELEM, "Implicit element reference"},
    {"EN_REF_TO_LVALUE", 154, true, read_node_EN_REF_TO_LVALUE, "Reference to lvalue"},
    {"EN_SQ_BRKT_RANGE", 155, true, read_node_EN_SQ_BRKT_RANGE, "Square bracket range"},
    {"EN_PARSE_HASH", 156, true, read_node_EN_PARSE_HASH, "Parse hash node"},

    // [157-159] Undefined
    {nullptr, 157, false, nullptr, nullptr},
    {nullptr, 158, false, nullptr, nullptr},
    {nullptr, 159, false, nullptr, nullptr},

    // [160-165] Map and fold operators
    {"EN_MAP", 160, true, read_node_EN_MAP, "Map operator"},
    {"EN_MAP_SELECT", 161, true, read_node_EN_MAP_SELECT, "Map-select operator"},
    {"EN_HASH_MAP", 162, true, read_node_EN_HASH_MAP, "Hash map operator"},
    {"EN_HASH_MAP_SELECT", 163, true, read_node_EN_HASH_MAP_SELECT, "Hash map-select operator"},
    {"EN_FOLDL", 164, true, read_node_EN_FOLDL, "Fold-left operator"},
    {"EN_FOLDR", 165, true, read_node_EN_FOLDR, "Fold-right operator"},

    // [166-255] Undefined (all remaining entries)
    {nullptr, 166, false, nullptr, nullptr}, {nullptr, 167, false, nullptr, nullptr},
    {nullptr, 168, false, nullptr, nullptr}, {nullptr, 169, false, nullptr, nullptr},
    {nullptr, 170, false, nullptr, nullptr}, {nullptr, 171, false, nullptr, nullptr},
    {nullptr, 172, false, nullptr, nullptr}, {nullptr, 173, false, nullptr, nullptr},
    {nullptr, 174, false, nullptr, nullptr}, {nullptr, 175, false, nullptr, nullptr},
    {nullptr, 176, false, nullptr, nullptr}, {nullptr, 177, false, nullptr, nullptr},
    {nullptr, 178, false, nullptr, nullptr}, {nullptr, 179, false, nullptr, nullptr},
    {nullptr, 180, false, nullptr, nullptr}, {nullptr, 181, false, nullptr, nullptr},
    {nullptr, 182, false, nullptr, nullptr}, {nullptr, 183, false, nullptr, nullptr},
    {nullptr, 184, false, nullptr, nullptr}, {nullptr, 185, false, nullptr, nullptr},
    {nullptr, 186, false, nullptr, nullptr}, {nullptr, 187, false, nullptr, nullptr},
    {nullptr, 188, false, nullptr, nullptr}, {nullptr, 189, false, nullptr, nullptr},
    {nullptr, 190, false, nullptr, nullptr}, {nullptr, 191, false, nullptr, nullptr},
    {nullptr, 192, false, nullptr, nullptr}, {nullptr, 193, false, nullptr, nullptr},
    {nullptr, 194, false, nullptr, nullptr}, {nullptr, 195, false, nullptr, nullptr},
    {nullptr, 196, false, nullptr, nullptr}, {nullptr, 197, false, nullptr, nullptr},
    {nullptr, 198, false, nullptr, nullptr}, {nullptr, 199, false, nullptr, nullptr},
    {nullptr, 200, false, nullptr, nullptr}, {nullptr, 201, false, nullptr, nullptr},
    {nullptr, 202, false, nullptr, nullptr}, {nullptr, 203, false, nullptr, nullptr},
    {nullptr, 204, false, nullptr, nullptr}, {nullptr, 205, false, nullptr, nullptr},
    {nullptr, 206, false, nullptr, nullptr}, {nullptr, 207, false, nullptr, nullptr},
    {nullptr, 208, false, nullptr, nullptr}, {nullptr, 209, false, nullptr, nullptr},
    {nullptr, 210, false, nullptr, nullptr}, {nullptr, 211, false, nullptr, nullptr},
    {nullptr, 212, false, nullptr, nullptr}, {nullptr, 213, false, nullptr, nullptr},
    {nullptr, 214, false, nullptr, nullptr}, {nullptr, 215, false, nullptr, nullptr},
    {nullptr, 216, false, nullptr, nullptr}, {nullptr, 217, false, nullptr, nullptr},
    {nullptr, 218, false, nullptr, nullptr}, {nullptr, 219, false, nullptr, nullptr},
    {nullptr, 220, false, nullptr, nullptr}, {nullptr, 221, false, nullptr, nullptr},
    {nullptr, 222, false, nullptr, nullptr}, {nullptr, 223, false, nullptr, nullptr},
    {nullptr, 224, false, nullptr, nullptr}, {nullptr, 225, false, nullptr, nullptr},
    {nullptr, 226, false, nullptr, nullptr}, {nullptr, 227, false, nullptr, nullptr},
    {nullptr, 228, false, nullptr, nullptr}, {nullptr, 229, false, nullptr, nullptr},
    {nullptr, 230, false, nullptr, nullptr}, {nullptr, 231, false, nullptr, nullptr},
    {nullptr, 232, false, nullptr, nullptr}, {nullptr, 233, false, nullptr, nullptr},
    {nullptr, 234, false, nullptr, nullptr}, {nullptr, 235, false, nullptr, nullptr},
    {nullptr, 236, false, nullptr, nullptr}, {nullptr, 237, false, nullptr, nullptr},
    {nullptr, 238, false, nullptr, nullptr}, {nullptr, 239, false, nullptr, nullptr},
    {nullptr, 240, false, nullptr, nullptr}, {nullptr, 241, false, nullptr, nullptr},
    {nullptr, 242, false, nullptr, nullptr}, {nullptr, 243, false, nullptr, nullptr},
    {nullptr, 244, false, nullptr, nullptr}, {nullptr, 245, false, nullptr, nullptr},
    {nullptr, 246, false, nullptr, nullptr}, {nullptr, 247, false, nullptr, nullptr},
    {nullptr, 248, false, nullptr, nullptr}, {nullptr, 249, false, nullptr, nullptr},
    {nullptr, 250, false, nullptr, nullptr}, {nullptr, 251, false, nullptr, nullptr},
    {nullptr, 252, false, nullptr, nullptr}, {nullptr, 253, false, nullptr, nullptr},
    {nullptr, 254, false, nullptr, nullptr}, {nullptr, 255, false, nullptr, nullptr},
};

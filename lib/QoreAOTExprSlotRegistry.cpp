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

class UserClosureVariant;
QoreIRFunction* lowerClosureForSerialization(const UserClosureVariant* variant);

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

    // 21: HASH_LITERAL (unsupported in slot metadata)
    {nullptr, 21, false, nullptr, "Hash literal"},

    // 22: HASH_DEREF (unsupported in slot metadata)
    {nullptr, 22, false, nullptr, "Hash/object dereference"},

    // 23: PARSE_REF (unsupported in slot metadata)
    {nullptr, 23, false, nullptr, "Parse reference"},

    // 24: CAST_HASHDECL (unsupported in slot metadata)
    {nullptr, 24, false, nullptr, "Hashdecl cast"},

    // 25: CAST_COMPLEX_HASH (unsupported in slot metadata)
    {nullptr, 25, false, nullptr, "Complex hash cast"},

    // 26: CAST_COMPLEX_LIST (unsupported in slot metadata)
    {nullptr, 26, false, nullptr, "Complex list cast"},

    // 27: CAST_CLASS (unsupported in slot metadata)
    {nullptr, 27, false, nullptr, "Class cast"},

    // 28: CAST_ENUM (unsupported in slot metadata)
    {nullptr, 28, false, nullptr, "Enum cast"},

    // 29: CONST_INT
    {"CONST_INT", 29, true, write_slot_CONST_INT, "Integer constant"},

    // 30: CONST_FLOAT
    {"CONST_FLOAT", 30, true, write_slot_CONST_FLOAT, "Float constant"},

    // 31: CONST_BOOL
    {"CONST_BOOL", 31, true, write_slot_CONST_BOOL, "Boolean constant"},

    // 32: CONST_NOTHING
    {"CONST_NOTHING", 32, true, write_slot_CONST_NOTHING, "Nothing constant"},

    // 33: LIST_LITERAL (unsupported in slot metadata)
    {nullptr, 33, false, nullptr, "List literal"},

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

    // 41-253: Unused
    {nullptr, 41, false, nullptr, nullptr},
    {nullptr, 42, false, nullptr, nullptr},
    {nullptr, 43, false, nullptr, nullptr},
    {nullptr, 44, false, nullptr, nullptr},
    {nullptr, 45, false, nullptr, nullptr},
    {nullptr, 46, false, nullptr, nullptr},
    {nullptr, 47, false, nullptr, nullptr},
    {nullptr, 48, false, nullptr, nullptr},
    {nullptr, 49, false, nullptr, nullptr},
    {nullptr, 50, false, nullptr, nullptr},
    {nullptr, 51, false, nullptr, nullptr},
    {nullptr, 52, false, nullptr, nullptr},
    {nullptr, 53, false, nullptr, nullptr},
    {nullptr, 54, false, nullptr, nullptr},
    {nullptr, 55, false, nullptr, nullptr},
    {nullptr, 56, false, nullptr, nullptr},
    {nullptr, 57, false, nullptr, nullptr},
    {nullptr, 58, false, nullptr, nullptr},
    {nullptr, 59, false, nullptr, nullptr},
    {nullptr, 60, false, nullptr, nullptr},
    {nullptr, 61, false, nullptr, nullptr},
    {nullptr, 62, false, nullptr, nullptr},
    {nullptr, 63, false, nullptr, nullptr},
    {nullptr, 64, false, nullptr, nullptr},
    {nullptr, 65, false, nullptr, nullptr},
    {nullptr, 66, false, nullptr, nullptr},
    {nullptr, 67, false, nullptr, nullptr},
    {nullptr, 68, false, nullptr, nullptr},
    {nullptr, 69, false, nullptr, nullptr},
    {nullptr, 70, false, nullptr, nullptr},
    {nullptr, 71, false, nullptr, nullptr},
    {nullptr, 72, false, nullptr, nullptr},
    {nullptr, 73, false, nullptr, nullptr},
    {nullptr, 74, false, nullptr, nullptr},
    {nullptr, 75, false, nullptr, nullptr},
    {nullptr, 76, false, nullptr, nullptr},
    {nullptr, 77, false, nullptr, nullptr},
    {nullptr, 78, false, nullptr, nullptr},
    {nullptr, 79, false, nullptr, nullptr},
    {nullptr, 80, false, nullptr, nullptr},
    {nullptr, 81, false, nullptr, nullptr},
    {nullptr, 82, false, nullptr, nullptr},
    {nullptr, 83, false, nullptr, nullptr},
    {nullptr, 84, false, nullptr, nullptr},
    {nullptr, 85, false, nullptr, nullptr},
    {nullptr, 86, false, nullptr, nullptr},
    {nullptr, 87, false, nullptr, nullptr},
    {nullptr, 88, false, nullptr, nullptr},
    {nullptr, 89, false, nullptr, nullptr},
    {nullptr, 90, false, nullptr, nullptr},
    {nullptr, 91, false, nullptr, nullptr},
    {nullptr, 92, false, nullptr, nullptr},
    {nullptr, 93, false, nullptr, nullptr},
    {nullptr, 94, false, nullptr, nullptr},
    {nullptr, 95, false, nullptr, nullptr},
    {nullptr, 96, false, nullptr, nullptr},
    {nullptr, 97, false, nullptr, nullptr},
    {nullptr, 98, false, nullptr, nullptr},
    {nullptr, 99, false, nullptr, nullptr},
    {nullptr, 100, false, nullptr, nullptr},
    {nullptr, 101, false, nullptr, nullptr},
    {nullptr, 102, false, nullptr, nullptr},
    {nullptr, 103, false, nullptr, nullptr},
    {nullptr, 104, false, nullptr, nullptr},
    {nullptr, 105, false, nullptr, nullptr},
    {nullptr, 106, false, nullptr, nullptr},
    {nullptr, 107, false, nullptr, nullptr},
    {nullptr, 108, false, nullptr, nullptr},
    {nullptr, 109, false, nullptr, nullptr},
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
    {"GENERIC_EVAL", 255, true, write_slot_GENERIC_EVAL, "Unsupported expression (needs source fallback)"}
};

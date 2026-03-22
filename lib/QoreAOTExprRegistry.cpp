/* -*- mode: c++ -*- */
/*
  QoreAOTExprRegistry.cpp

  Qore expression kind registry for AOT binary serialization

  Copyright (C) 2025 Qore Technologies, s.r.o.

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

#include "qore/intern/QoreAOTExprRegistry.h"

// ============================================================================
// Expression Kind Handler Stubs
// ============================================================================
// Each handler pair (write, read) will be extracted from classifyAndWriteExpr
// and classifyAndReadExpr during Phase 3.2

// Placeholder handlers (return true for write, nullptr for read) — to be filled in Phase 3.2

static bool write_func_call(AOTExprWriteCtx& ctx) {
    // TODO: Extract from classifyAndWriteExpr
    return true;
}

static QoreValue read_func_call(AOTExprReadCtx& ctx) {
    // TODO: Extract from classifyAndReadExpr
    return QoreValue();
}

static bool write_self_method_call(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_self_method_call(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_static_method_call(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_static_method_call(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_new_object(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_new_object(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_runtime_const_ref(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_runtime_const_ref(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_self_varref(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_self_varref(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_local_varref(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_local_varref(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_global_varref(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_global_varref(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_const_number(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_const_number(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_const_binary(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_const_binary(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_closure_create(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_closure_create(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_call_ref(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_call_ref(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_obj_method_ref(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_obj_method_ref(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_static_varref(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_static_varref(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_scoped_new_object(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_scoped_new_object(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_hashdecl_new(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_hashdecl_new(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_complex_hash_new(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_complex_hash_new(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_complex_list_new(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_complex_list_new(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_const_enum(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_const_enum(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_const_string(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_const_string(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_hash_literal(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_hash_literal(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_hash_deref(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_hash_deref(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_parse_ref(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_parse_ref(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_cast_hashdecl(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_cast_hashdecl(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_cast_complex_hash(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_cast_complex_hash(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_cast_complex_list(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_cast_complex_list(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_cast_class(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_cast_class(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_cast_enum(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_cast_enum(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_const_int(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_const_int(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_const_float(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_const_float(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_const_bool(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_const_bool(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_const_nothing(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_const_nothing(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_list_literal(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_list_literal(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_expr_tree(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_expr_tree(AOTExprReadCtx& ctx) {
    return QoreValue();
}

static bool write_generic_eval(AOTExprWriteCtx& ctx) {
    return true;
}

static QoreValue read_generic_eval(AOTExprReadCtx& ctx) {
    return QoreValue();
}

// ============================================================================
// Registry Lookup Function
// ============================================================================

const QoreAOTExprKindInfo* getAOTExprKindInfo(uint8_t kind_byte) {
    if (kind_byte >= 256) {
        return nullptr;
    }
    return &AOT_EXPR_KIND_REGISTRY[kind_byte];
}

// ============================================================================
// Complete Registry Table (256 entries)
// ============================================================================

const QoreAOTExprKindInfo AOT_EXPR_KIND_REGISTRY[256] = {
    // Index 0: Reserved/Unused
    {nullptr, 0, false, nullptr, nullptr, nullptr},

    // 1: FUNC_CALL
    {"FUNC_CALL", 1, true, write_func_call, read_func_call, "Regular function call"},

    // 2: SELF_METHOD_CALL
    {"SELF_METHOD_CALL", 2, true, write_self_method_call, read_self_method_call, "Self method call"},

    // 3: STATIC_METHOD_CALL
    {"STATIC_METHOD_CALL", 3, true, write_static_method_call, read_static_method_call, "Static method call"},

    // 4: NEW_OBJECT
    {"NEW_OBJECT", 4, true, write_new_object, read_new_object, "New object constructor"},

    // 5: RUNTIME_CONST_REF
    {"RUNTIME_CONST_REF", 5, true, write_runtime_const_ref, read_runtime_const_ref, "Runtime constant reference"},

    // 6: SELF_VARREF
    {"SELF_VARREF", 6, true, write_self_varref, read_self_varref, "Self variable reference"},

    // 7: LOCAL_VARREF
    {"LOCAL_VARREF", 7, true, write_local_varref, read_local_varref, "Local variable reference"},

    // 8: GLOBAL_VARREF
    {"GLOBAL_VARREF", 8, true, write_global_varref, read_global_varref, "Global variable reference"},

    // 9: CONST_NUMBER
    {"CONST_NUMBER", 9, true, write_const_number, read_const_number, "Number constant"},

    // 10: CONST_BINARY
    {"CONST_BINARY", 10, true, write_const_binary, read_const_binary, "Binary constant"},

    // 11: CLOSURE_CREATE
    {"CLOSURE_CREATE", 11, true, write_closure_create, read_closure_create, "Closure/lambda creation"},

    // 12: CALL_REF
    {"CALL_REF", 12, true, write_call_ref, read_call_ref, "Call reference call"},

    // 13: OBJ_METHOD_REF
    {"OBJ_METHOD_REF", 13, true, write_obj_method_ref, read_obj_method_ref, "Object method reference"},

    // 14: STATIC_VARREF
    {"STATIC_VARREF", 14, true, write_static_varref, read_static_varref, "Static class variable"},

    // 15: SCOPED_NEW_OBJECT
    {"SCOPED_NEW_OBJECT", 15, true, write_scoped_new_object, read_scoped_new_object, "Scoped new object"},

    // 16: HASHDECL_NEW
    {"HASHDECL_NEW", 16, true, write_hashdecl_new, read_hashdecl_new, "Hashdecl construction"},

    // 17: COMPLEX_HASH_NEW
    {"COMPLEX_HASH_NEW", 17, true, write_complex_hash_new, read_complex_hash_new, "Complex hash construction"},

    // 18: COMPLEX_LIST_NEW
    {"COMPLEX_LIST_NEW", 18, true, write_complex_list_new, read_complex_list_new, "Complex list construction"},

    // 19: CONST_ENUM
    {"CONST_ENUM", 19, true, write_const_enum, read_const_enum, "Enum constant"},

    // 20: CONST_STRING
    {"CONST_STRING", 20, true, write_const_string, read_const_string, "String constant"},

    // 21: HASH_LITERAL
    {"HASH_LITERAL", 21, true, write_hash_literal, read_hash_literal, "Hash literal"},

    // 22: HASH_DEREF
    {"HASH_DEREF", 22, true, write_hash_deref, read_hash_deref, "Hash/object dereference"},

    // 23: PARSE_REF
    {"PARSE_REF", 23, true, write_parse_ref, read_parse_ref, "Parse reference"},

    // 24: CAST_HASHDECL
    {"CAST_HASHDECL", 24, true, write_cast_hashdecl, read_cast_hashdecl, "Hashdecl cast"},

    // 25: CAST_COMPLEX_HASH
    {"CAST_COMPLEX_HASH", 25, true, write_cast_complex_hash, read_cast_complex_hash, "Complex hash cast"},

    // 26: CAST_COMPLEX_LIST
    {"CAST_COMPLEX_LIST", 26, true, write_cast_complex_list, read_cast_complex_list, "Complex list cast"},

    // 27: CAST_CLASS
    {"CAST_CLASS", 27, true, write_cast_class, read_cast_class, "Class cast"},

    // 28: CAST_ENUM
    {"CAST_ENUM", 28, true, write_cast_enum, read_cast_enum, "Enum cast"},

    // 29: CONST_INT
    {"CONST_INT", 29, true, write_const_int, read_const_int, "Integer constant"},

    // 30: CONST_FLOAT
    {"CONST_FLOAT", 30, true, write_const_float, read_const_float, "Float constant"},

    // 31: CONST_BOOL
    {"CONST_BOOL", 31, true, write_const_bool, read_const_bool, "Boolean constant"},

    // 32: CONST_NOTHING
    {"CONST_NOTHING", 32, true, write_const_nothing, read_const_nothing, "Nothing constant"},

    // 33: LIST_LITERAL
    {"LIST_LITERAL", 33, true, write_list_literal, read_list_literal, "List literal"},

    // 34-253: Unused/Reserved
    {nullptr, 34, false, nullptr, nullptr, nullptr},
    {nullptr, 35, false, nullptr, nullptr, nullptr},
    {nullptr, 36, false, nullptr, nullptr, nullptr},
    {nullptr, 37, false, nullptr, nullptr, nullptr},
    {nullptr, 38, false, nullptr, nullptr, nullptr},
    {nullptr, 39, false, nullptr, nullptr, nullptr},
    {nullptr, 40, false, nullptr, nullptr, nullptr},
    {nullptr, 41, false, nullptr, nullptr, nullptr},
    {nullptr, 42, false, nullptr, nullptr, nullptr},
    {nullptr, 43, false, nullptr, nullptr, nullptr},
    {nullptr, 44, false, nullptr, nullptr, nullptr},
    {nullptr, 45, false, nullptr, nullptr, nullptr},
    {nullptr, 46, false, nullptr, nullptr, nullptr},
    {nullptr, 47, false, nullptr, nullptr, nullptr},
    {nullptr, 48, false, nullptr, nullptr, nullptr},
    {nullptr, 49, false, nullptr, nullptr, nullptr},
    {nullptr, 50, false, nullptr, nullptr, nullptr},
    {nullptr, 51, false, nullptr, nullptr, nullptr},
    {nullptr, 52, false, nullptr, nullptr, nullptr},
    {nullptr, 53, false, nullptr, nullptr, nullptr},
    {nullptr, 54, false, nullptr, nullptr, nullptr},
    {nullptr, 55, false, nullptr, nullptr, nullptr},
    {nullptr, 56, false, nullptr, nullptr, nullptr},
    {nullptr, 57, false, nullptr, nullptr, nullptr},
    {nullptr, 58, false, nullptr, nullptr, nullptr},
    {nullptr, 59, false, nullptr, nullptr, nullptr},
    {nullptr, 60, false, nullptr, nullptr, nullptr},
    {nullptr, 61, false, nullptr, nullptr, nullptr},
    {nullptr, 62, false, nullptr, nullptr, nullptr},
    {nullptr, 63, false, nullptr, nullptr, nullptr},
    {nullptr, 64, false, nullptr, nullptr, nullptr},
    {nullptr, 65, false, nullptr, nullptr, nullptr},
    {nullptr, 66, false, nullptr, nullptr, nullptr},
    {nullptr, 67, false, nullptr, nullptr, nullptr},
    {nullptr, 68, false, nullptr, nullptr, nullptr},
    {nullptr, 69, false, nullptr, nullptr, nullptr},
    {nullptr, 70, false, nullptr, nullptr, nullptr},
    {nullptr, 71, false, nullptr, nullptr, nullptr},
    {nullptr, 72, false, nullptr, nullptr, nullptr},
    {nullptr, 73, false, nullptr, nullptr, nullptr},
    {nullptr, 74, false, nullptr, nullptr, nullptr},
    {nullptr, 75, false, nullptr, nullptr, nullptr},
    {nullptr, 76, false, nullptr, nullptr, nullptr},
    {nullptr, 77, false, nullptr, nullptr, nullptr},
    {nullptr, 78, false, nullptr, nullptr, nullptr},
    {nullptr, 79, false, nullptr, nullptr, nullptr},
    {nullptr, 80, false, nullptr, nullptr, nullptr},
    {nullptr, 81, false, nullptr, nullptr, nullptr},
    {nullptr, 82, false, nullptr, nullptr, nullptr},
    {nullptr, 83, false, nullptr, nullptr, nullptr},
    {nullptr, 84, false, nullptr, nullptr, nullptr},
    {nullptr, 85, false, nullptr, nullptr, nullptr},
    {nullptr, 86, false, nullptr, nullptr, nullptr},
    {nullptr, 87, false, nullptr, nullptr, nullptr},
    {nullptr, 88, false, nullptr, nullptr, nullptr},
    {nullptr, 89, false, nullptr, nullptr, nullptr},
    {nullptr, 90, false, nullptr, nullptr, nullptr},
    {nullptr, 91, false, nullptr, nullptr, nullptr},
    {nullptr, 92, false, nullptr, nullptr, nullptr},
    {nullptr, 93, false, nullptr, nullptr, nullptr},
    {nullptr, 94, false, nullptr, nullptr, nullptr},
    {nullptr, 95, false, nullptr, nullptr, nullptr},
    {nullptr, 96, false, nullptr, nullptr, nullptr},
    {nullptr, 97, false, nullptr, nullptr, nullptr},
    {nullptr, 98, false, nullptr, nullptr, nullptr},
    {nullptr, 99, false, nullptr, nullptr, nullptr},
    {nullptr, 100, false, nullptr, nullptr, nullptr},
    {nullptr, 101, false, nullptr, nullptr, nullptr},
    {nullptr, 102, false, nullptr, nullptr, nullptr},
    {nullptr, 103, false, nullptr, nullptr, nullptr},
    {nullptr, 104, false, nullptr, nullptr, nullptr},
    {nullptr, 105, false, nullptr, nullptr, nullptr},
    {nullptr, 106, false, nullptr, nullptr, nullptr},
    {nullptr, 107, false, nullptr, nullptr, nullptr},
    {nullptr, 108, false, nullptr, nullptr, nullptr},
    {nullptr, 109, false, nullptr, nullptr, nullptr},
    {nullptr, 110, false, nullptr, nullptr, nullptr},
    {nullptr, 111, false, nullptr, nullptr, nullptr},
    {nullptr, 112, false, nullptr, nullptr, nullptr},
    {nullptr, 113, false, nullptr, nullptr, nullptr},
    {nullptr, 114, false, nullptr, nullptr, nullptr},
    {nullptr, 115, false, nullptr, nullptr, nullptr},
    {nullptr, 116, false, nullptr, nullptr, nullptr},
    {nullptr, 117, false, nullptr, nullptr, nullptr},
    {nullptr, 118, false, nullptr, nullptr, nullptr},
    {nullptr, 119, false, nullptr, nullptr, nullptr},
    {nullptr, 120, false, nullptr, nullptr, nullptr},
    {nullptr, 121, false, nullptr, nullptr, nullptr},
    {nullptr, 122, false, nullptr, nullptr, nullptr},
    {nullptr, 123, false, nullptr, nullptr, nullptr},
    {nullptr, 124, false, nullptr, nullptr, nullptr},
    {nullptr, 125, false, nullptr, nullptr, nullptr},
    {nullptr, 126, false, nullptr, nullptr, nullptr},
    {nullptr, 127, false, nullptr, nullptr, nullptr},
    {nullptr, 128, false, nullptr, nullptr, nullptr},
    {nullptr, 129, false, nullptr, nullptr, nullptr},
    {nullptr, 130, false, nullptr, nullptr, nullptr},
    {nullptr, 131, false, nullptr, nullptr, nullptr},
    {nullptr, 132, false, nullptr, nullptr, nullptr},
    {nullptr, 133, false, nullptr, nullptr, nullptr},
    {nullptr, 134, false, nullptr, nullptr, nullptr},
    {nullptr, 135, false, nullptr, nullptr, nullptr},
    {nullptr, 136, false, nullptr, nullptr, nullptr},
    {nullptr, 137, false, nullptr, nullptr, nullptr},
    {nullptr, 138, false, nullptr, nullptr, nullptr},
    {nullptr, 139, false, nullptr, nullptr, nullptr},
    {nullptr, 140, false, nullptr, nullptr, nullptr},
    {nullptr, 141, false, nullptr, nullptr, nullptr},
    {nullptr, 142, false, nullptr, nullptr, nullptr},
    {nullptr, 143, false, nullptr, nullptr, nullptr},
    {nullptr, 144, false, nullptr, nullptr, nullptr},
    {nullptr, 145, false, nullptr, nullptr, nullptr},
    {nullptr, 146, false, nullptr, nullptr, nullptr},
    {nullptr, 147, false, nullptr, nullptr, nullptr},
    {nullptr, 148, false, nullptr, nullptr, nullptr},
    {nullptr, 149, false, nullptr, nullptr, nullptr},
    {nullptr, 150, false, nullptr, nullptr, nullptr},
    {nullptr, 151, false, nullptr, nullptr, nullptr},
    {nullptr, 152, false, nullptr, nullptr, nullptr},
    {nullptr, 153, false, nullptr, nullptr, nullptr},
    {nullptr, 154, false, nullptr, nullptr, nullptr},
    {nullptr, 155, false, nullptr, nullptr, nullptr},
    {nullptr, 156, false, nullptr, nullptr, nullptr},
    {nullptr, 157, false, nullptr, nullptr, nullptr},
    {nullptr, 158, false, nullptr, nullptr, nullptr},
    {nullptr, 159, false, nullptr, nullptr, nullptr},
    {nullptr, 160, false, nullptr, nullptr, nullptr},
    {nullptr, 161, false, nullptr, nullptr, nullptr},
    {nullptr, 162, false, nullptr, nullptr, nullptr},
    {nullptr, 163, false, nullptr, nullptr, nullptr},
    {nullptr, 164, false, nullptr, nullptr, nullptr},
    {nullptr, 165, false, nullptr, nullptr, nullptr},
    {nullptr, 166, false, nullptr, nullptr, nullptr},
    {nullptr, 167, false, nullptr, nullptr, nullptr},
    {nullptr, 168, false, nullptr, nullptr, nullptr},
    {nullptr, 169, false, nullptr, nullptr, nullptr},
    {nullptr, 170, false, nullptr, nullptr, nullptr},
    {nullptr, 171, false, nullptr, nullptr, nullptr},
    {nullptr, 172, false, nullptr, nullptr, nullptr},
    {nullptr, 173, false, nullptr, nullptr, nullptr},
    {nullptr, 174, false, nullptr, nullptr, nullptr},
    {nullptr, 175, false, nullptr, nullptr, nullptr},
    {nullptr, 176, false, nullptr, nullptr, nullptr},
    {nullptr, 177, false, nullptr, nullptr, nullptr},
    {nullptr, 178, false, nullptr, nullptr, nullptr},
    {nullptr, 179, false, nullptr, nullptr, nullptr},
    {nullptr, 180, false, nullptr, nullptr, nullptr},
    {nullptr, 181, false, nullptr, nullptr, nullptr},
    {nullptr, 182, false, nullptr, nullptr, nullptr},
    {nullptr, 183, false, nullptr, nullptr, nullptr},
    {nullptr, 184, false, nullptr, nullptr, nullptr},
    {nullptr, 185, false, nullptr, nullptr, nullptr},
    {nullptr, 186, false, nullptr, nullptr, nullptr},
    {nullptr, 187, false, nullptr, nullptr, nullptr},
    {nullptr, 188, false, nullptr, nullptr, nullptr},
    {nullptr, 189, false, nullptr, nullptr, nullptr},
    {nullptr, 190, false, nullptr, nullptr, nullptr},
    {nullptr, 191, false, nullptr, nullptr, nullptr},
    {nullptr, 192, false, nullptr, nullptr, nullptr},
    {nullptr, 193, false, nullptr, nullptr, nullptr},
    {nullptr, 194, false, nullptr, nullptr, nullptr},
    {nullptr, 195, false, nullptr, nullptr, nullptr},
    {nullptr, 196, false, nullptr, nullptr, nullptr},
    {nullptr, 197, false, nullptr, nullptr, nullptr},
    {nullptr, 198, false, nullptr, nullptr, nullptr},
    {nullptr, 199, false, nullptr, nullptr, nullptr},
    {nullptr, 200, false, nullptr, nullptr, nullptr},
    {nullptr, 201, false, nullptr, nullptr, nullptr},
    {nullptr, 202, false, nullptr, nullptr, nullptr},
    {nullptr, 203, false, nullptr, nullptr, nullptr},
    {nullptr, 204, false, nullptr, nullptr, nullptr},
    {nullptr, 205, false, nullptr, nullptr, nullptr},
    {nullptr, 206, false, nullptr, nullptr, nullptr},
    {nullptr, 207, false, nullptr, nullptr, nullptr},
    {nullptr, 208, false, nullptr, nullptr, nullptr},
    {nullptr, 209, false, nullptr, nullptr, nullptr},
    {nullptr, 210, false, nullptr, nullptr, nullptr},
    {nullptr, 211, false, nullptr, nullptr, nullptr},
    {nullptr, 212, false, nullptr, nullptr, nullptr},
    {nullptr, 213, false, nullptr, nullptr, nullptr},
    {nullptr, 214, false, nullptr, nullptr, nullptr},
    {nullptr, 215, false, nullptr, nullptr, nullptr},
    {nullptr, 216, false, nullptr, nullptr, nullptr},
    {nullptr, 217, false, nullptr, nullptr, nullptr},
    {nullptr, 218, false, nullptr, nullptr, nullptr},
    {nullptr, 219, false, nullptr, nullptr, nullptr},
    {nullptr, 220, false, nullptr, nullptr, nullptr},
    {nullptr, 221, false, nullptr, nullptr, nullptr},
    {nullptr, 222, false, nullptr, nullptr, nullptr},
    {nullptr, 223, false, nullptr, nullptr, nullptr},
    {nullptr, 224, false, nullptr, nullptr, nullptr},
    {nullptr, 225, false, nullptr, nullptr, nullptr},
    {nullptr, 226, false, nullptr, nullptr, nullptr},
    {nullptr, 227, false, nullptr, nullptr, nullptr},
    {nullptr, 228, false, nullptr, nullptr, nullptr},
    {nullptr, 229, false, nullptr, nullptr, nullptr},
    {nullptr, 230, false, nullptr, nullptr, nullptr},
    {nullptr, 231, false, nullptr, nullptr, nullptr},
    {nullptr, 232, false, nullptr, nullptr, nullptr},
    {nullptr, 233, false, nullptr, nullptr, nullptr},
    {nullptr, 234, false, nullptr, nullptr, nullptr},
    {nullptr, 235, false, nullptr, nullptr, nullptr},
    {nullptr, 236, false, nullptr, nullptr, nullptr},
    {nullptr, 237, false, nullptr, nullptr, nullptr},
    {nullptr, 238, false, nullptr, nullptr, nullptr},
    {nullptr, 239, false, nullptr, nullptr, nullptr},
    {nullptr, 240, false, nullptr, nullptr, nullptr},
    {nullptr, 241, false, nullptr, nullptr, nullptr},
    {nullptr, 242, false, nullptr, nullptr, nullptr},
    {nullptr, 243, false, nullptr, nullptr, nullptr},
    {nullptr, 244, false, nullptr, nullptr, nullptr},
    {nullptr, 245, false, nullptr, nullptr, nullptr},
    {nullptr, 246, false, nullptr, nullptr, nullptr},
    {nullptr, 247, false, nullptr, nullptr, nullptr},
    {nullptr, 248, false, nullptr, nullptr, nullptr},
    {nullptr, 249, false, nullptr, nullptr, nullptr},
    {nullptr, 250, false, nullptr, nullptr, nullptr},
    {nullptr, 251, false, nullptr, nullptr, nullptr},
    {nullptr, 252, false, nullptr, nullptr, nullptr},
    {nullptr, 253, false, nullptr, nullptr, nullptr},

    // 254: EXPR_TREE
    {"EXPR_TREE", 254, true, write_expr_tree, read_expr_tree, "Recursive expression tree"},

    // 255: GENERIC_EVAL
    {"GENERIC_EVAL", 255, true, write_generic_eval, read_generic_eval, "Unsupported expression — fallback to source"},
};

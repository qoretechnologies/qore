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

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/QoreAOTExprRegistry.h"
#include "qore/intern/QoreAOTExprSlotRegistry.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/NewComplexTypeNode.h"
#include "qore/intern/ConstantList.h"

#include <cstring>

// ============================================================================
// Expression Kind Handlers (Phase 3.2 - Extracted from serialization code)
// ============================================================================
// Handler pairs extracted from classifyAndWriteExpr() and readOneExpr()

// Forward declarations for recursive handlers
extern bool classifyAndWriteExpr(QoreAOTBinaryWriter& writer, const QoreValue& expr,
        const std::vector<AOTLocalSlotId>& parent_locals,
        const std::vector<AOTGlobalSlotId>& parent_globals,
        const AOTConstantReverseMap* const_reverse_map);

extern QoreValue readOneExpr(
        const QoreAOTBinaryReader& rdr, const uint8_t*& p, const uint8_t* e,
        std::string& err, QoreProgram* pgm,
        LocalVar** locals, int num_locals,
        Var** globals, int num_globals,
        QoreProgram* local_owner_pgm);

// Include all handler implementations (Phase 3.2 extracted handlers)
#include "QoreAOTExprHandlers.cpp"

// ============================================================================
// Registry Lookup Function
// ============================================================================

const QoreAOTExprKindInfo* getAOTExprKindInfo(uint8_t kind_byte) {
    return &AOT_EXPR_KIND_REGISTRY[kind_byte];
}

bool qore_aot_validate_expr_registries(std::string& error) {
    for (unsigned i = 0; i < 256; ++i) {
        const auto& expr = AOT_EXPR_KIND_REGISTRY[i];
        const auto& slot = AOT_EXPR_SLOT_KIND_REGISTRY[i];

        if (expr.kind_value != i) {
            error = "inline expression registry index " + std::to_string(i)
                + " has kind value " + std::to_string(expr.kind_value);
            return false;
        }
        if (slot.kind_value != i) {
            error = "slot expression registry index " + std::to_string(i)
                + " has kind value " + std::to_string(slot.kind_value);
            return false;
        }

        if (expr.is_supported) {
            if (!expr.name || !*expr.name) {
                error = "inline expression registry index " + std::to_string(i)
                    + " is supported without a name";
                return false;
            }
            if (!expr.write_fn || !expr.read_fn) {
                error = "inline expression registry entry '" + std::string(expr.name)
                    + "' is supported without read/write handlers";
                return false;
            }
        }

        if (slot.is_supported) {
            if (!slot.name || !*slot.name) {
                error = "slot expression registry index " + std::to_string(i)
                    + " is supported without a name";
                return false;
            }
            if (!slot.write_fn) {
                error = "slot expression registry entry '" + std::string(slot.name)
                    + "' is supported without a write handler";
                return false;
            }
        }

        if (expr.is_supported != slot.is_supported) {
            const char* name = expr.name ? expr.name : (slot.name ? slot.name : "<unnamed>");
            error = "AOT expression registry support mismatch for kind "
                + std::to_string(i) + " (" + name + "): inline="
                + (expr.is_supported ? "supported" : "unsupported") + ", slot="
                + (slot.is_supported ? "supported" : "unsupported");
            return false;
        }

        if (expr.is_supported && std::strcmp(expr.name, slot.name)) {
            error = "AOT expression registry name mismatch for kind " + std::to_string(i)
                + ": inline='" + expr.name + "', slot='" + slot.name + "'";
            return false;
        }
    }

    return true;
}

// ============================================================================
// Complete Registry Table (256 entries)
// ============================================================================

const QoreAOTExprKindInfo AOT_EXPR_KIND_REGISTRY[256] = {
    // Index 0: Reserved/Unused
    {nullptr, 0, false, nullptr, nullptr, nullptr},

    // 1: FUNC_CALL
    {"FUNC_CALL", 1, true, write_expr_func_call, read_expr_func_call, "Regular function call"},

    // 2: SELF_METHOD_CALL
    {"SELF_METHOD_CALL", 2, true, write_expr_self_method_call, read_expr_self_method_call, "Self method call"},

    // 3: STATIC_METHOD_CALL
    {"STATIC_METHOD_CALL", 3, true, write_expr_static_method_call, read_expr_static_method_call, "Static method call"},

    // 4: NEW_OBJECT
    {"NEW_OBJECT", 4, true, write_expr_new_object, read_expr_new_object, "New object constructor"},

    // 5: RUNTIME_CONST_REF
    {"RUNTIME_CONST_REF", 5, true, write_expr_runtime_const_ref, read_expr_runtime_const_ref, "Runtime constant reference"},

    // 6: SELF_VARREF
    {"SELF_VARREF", 6, true, write_expr_self_varref, read_expr_self_varref, "Self variable reference"},

    // 7: LOCAL_VARREF
    {"LOCAL_VARREF", 7, true, write_expr_local_varref, read_expr_local_varref, "Local variable reference"},

    // 8: GLOBAL_VARREF
    {"GLOBAL_VARREF", 8, true, write_expr_global_varref, read_expr_global_varref, "Global variable reference"},

    // 9: CONST_NUMBER
    {"CONST_NUMBER", 9, true, write_expr_const_number, read_expr_const_number, "Number constant"},

    // 10: CONST_BINARY
    {"CONST_BINARY", 10, true, write_expr_const_binary, read_expr_const_binary, "Binary constant"},

    // 11: CLOSURE_CREATE
    {"CLOSURE_CREATE", 11, true, write_expr_closure_create, read_expr_closure_create, "Closure/lambda creation"},

    // 12: CALL_REF
    {"CALL_REF", 12, true, write_expr_call_ref, read_expr_call_ref, "Call reference call"},

    // 13: OBJ_METHOD_REF
    {"OBJ_METHOD_REF", 13, true, write_expr_obj_method_ref, read_expr_obj_method_ref, "Object method reference"},

    // 14: STATIC_VARREF
    {"STATIC_VARREF", 14, true, write_expr_static_varref, read_expr_static_varref, "Static class variable"},

    // 15: SCOPED_NEW_OBJECT
    {"SCOPED_NEW_OBJECT", 15, true, write_expr_scoped_new_object, read_expr_scoped_new_object, "Scoped new object"},

    // 16: HASHDECL_NEW
    {"HASHDECL_NEW", 16, true, write_expr_hashdecl_new, read_expr_hashdecl_new, "Hashdecl construction"},

    // 17: COMPLEX_HASH_NEW
    {"COMPLEX_HASH_NEW", 17, true, write_expr_complex_hash_new, read_expr_complex_hash_new, "Complex hash construction"},

    // 18: COMPLEX_LIST_NEW
    {"COMPLEX_LIST_NEW", 18, true, write_expr_complex_list_new, read_expr_complex_list_new, "Complex list construction"},

    // 19: CONST_ENUM
    {"CONST_ENUM", 19, true, write_expr_const_enum, read_expr_const_enum, "Enum constant"},

    // 20: CONST_STRING
    {"CONST_STRING", 20, true, write_expr_const_string, read_expr_const_string, "String constant"},

    // 21: HASH_LITERAL
    {"HASH_LITERAL", 21, true, write_expr_hash_literal, read_expr_hash_literal, "Hash literal"},

    // 22: HASH_DEREF
    {"HASH_DEREF", 22, true, write_expr_hash_deref, read_expr_hash_deref, "Hash/object dereference"},

    // 23: PARSE_REF
    {"PARSE_REF", 23, true, write_expr_parse_ref, read_expr_parse_ref, "Parse reference"},

    // 24: CAST_HASHDECL
    {"CAST_HASHDECL", 24, true, write_expr_cast_hashdecl, read_expr_cast_hashdecl, "Hashdecl cast"},

    // 25: CAST_COMPLEX_HASH
    {"CAST_COMPLEX_HASH", 25, true, write_expr_cast_complex_hash, read_expr_cast_complex_hash, "Complex hash cast"},

    // 26: CAST_COMPLEX_LIST
    {"CAST_COMPLEX_LIST", 26, true, write_expr_cast_complex_list, read_expr_cast_complex_list, "Complex list cast"},

    // 27: CAST_CLASS
    {"CAST_CLASS", 27, true, write_expr_cast_class, read_expr_cast_class, "Class cast"},

    // 28: CAST_ENUM
    {"CAST_ENUM", 28, true, write_expr_cast_enum, read_expr_cast_enum, "Enum cast"},

    // 29: CONST_INT
    {"CONST_INT", 29, true, write_expr_const_int, read_expr_const_int, "Integer constant"},

    // 30: CONST_FLOAT
    {"CONST_FLOAT", 30, true, write_expr_const_float, read_expr_const_float, "Float constant"},

    // 31: CONST_BOOL
    {"CONST_BOOL", 31, true, write_expr_const_bool, read_expr_const_bool, "Boolean constant"},

    // 32: CONST_NOTHING
    {"CONST_NOTHING", 32, true, write_expr_const_nothing, read_expr_const_nothing, "Nothing constant"},

    // 33: LIST_LITERAL
    {"LIST_LITERAL", 33, true, write_expr_list_literal, read_expr_list_literal, "List literal"},

    // 34: CONST_NULL
    {"CONST_NULL", 34, true, write_expr_const_null, read_expr_const_null, "NULL constant"},

    // 35: DOT_EVAL_TARGET (classifyAndWriteExpr writes this for inline dot-eval expressions)
    {"DOT_EVAL_TARGET", 35, true, write_expr_dot_eval_target, read_expr_dot_eval_target, "Dot-eval method target (inline)"},
    {"FUNC_CALL_REF", 36, true, write_expr_func_call_ref, read_expr_func_call_ref, "Function call reference"},
    {"BOUND_METHOD_REF", 37, true, write_expr_bound_method_ref, read_expr_bound_method_ref, "Bound method reference"},
    {"STATIC_METHOD_REF", 38, true, write_expr_static_method_ref, read_expr_static_method_ref, "Static method reference"},
    {"SELF_METHOD_REF", 39, true, write_expr_self_method_ref, read_expr_self_method_ref, "Self method reference"},
    {"OBJ_METHOD_REF_EXPR", 40, true, write_expr_obj_method_ref_expr, read_expr_obj_method_ref_expr, "Object method reference"},
    {"CONST_VALUE", 41, true, write_expr_const_value, read_expr_const_value, "Serialized constant value"},
    {"PLUS", 42, true, write_expr_plus, read_expr_plus, "Plus operator"},
    {"SQUARE_BRACKET", 43, true, write_expr_square_bracket, read_expr_square_bracket, "Square-bracket operator"},
    {"PARSE_HASH", 44, true, write_expr_parse_hash, read_expr_parse_hash, "Parse hash literal"},
    {"EXISTS", 45, true, write_expr_exists, read_expr_exists, "Exists operator"},
    {"IMPLICIT_ARG", 46, true, write_expr_implicit_arg, read_expr_implicit_arg, "Implicit argument reference"},
    {"MINUS", 47, true, write_expr_minus, read_expr_minus, "Minus operator"},
    {"KEYS", 48, true, write_expr_keys, read_expr_keys, "Keys operator"},
    {"MULTIPLY", 49, true, write_expr_multiply, read_expr_multiply, "Multiplication operator"},
    {"DIVIDE", 50, true, write_expr_divide, read_expr_divide, "Division operator"},
    {"MODULO", 51, true, write_expr_modulo, read_expr_modulo, "Modulo operator"},
    {"IMPLICIT_ELEM", 52, true, write_expr_implicit_elem, read_expr_implicit_elem, "Implicit element reference"},
    {"INSTANCEOF", 53, true, write_expr_instanceof, read_expr_instanceof, "Instanceof operator"},
    {"REGEX_MATCH", 54, true, write_expr_regex_match, read_expr_regex_match, "Regex match operator"},
    {"REGEX_NMATCH", 55, true, write_expr_regex_nmatch, read_expr_regex_nmatch, "Regex negative match operator"},
    {"REGEX_EXTRACT", 56, true, write_expr_regex_extract, read_expr_regex_extract, "Regex extract operator"},
    {"PRE_INC", 57, true, write_expr_pre_inc, read_expr_pre_inc, "Pre-increment operator"},
    {"PRE_DEC", 58, true, write_expr_pre_dec, read_expr_pre_dec, "Pre-decrement operator"},
    {"POST_INC", 59, true, write_expr_post_inc, read_expr_post_inc, "Post-increment operator"},
    {"POST_DEC", 60, true, write_expr_post_dec, read_expr_post_dec, "Post-decrement operator"},
    {"LOG_EQ", 61, true, write_expr_log_eq, read_expr_log_eq, "Logical equality operator"},
    {"LOG_NE", 62, true, write_expr_log_ne, read_expr_log_ne, "Logical not-equals operator"},
    {"LOG_NOT", 63, true, write_expr_log_not, read_expr_log_not, "Logical not operator"},
    {"TRIM", 64, true, write_expr_trim, read_expr_trim, "Trim operator"},
    {"CHOMP", 65, true, write_expr_chomp, read_expr_chomp, "Chomp operator"},
    {"POP", 66, true, write_expr_pop, read_expr_pop, "Pop operator"},
    {"SHIFT", 67, true, write_expr_shift, read_expr_shift, "Shift operator"},
    {"PUSH", 68, true, write_expr_push, read_expr_push, "Push operator"},
    {"UNSHIFT", 69, true, write_expr_unshift, read_expr_unshift, "Unshift operator"},
    {"ELEMENTS", 70, true, write_expr_elements, read_expr_elements, "Elements operator"},
    {"DELETE", 71, true, write_expr_delete, read_expr_delete, "Delete operator"},
    {"REMOVE", 72, true, write_expr_remove, read_expr_remove, "Remove operator"},
    {"BACKGROUND", 73, true, write_expr_background, read_expr_background, "Background operator"},
    {"CONTEXT_REF", 74, true, write_expr_context_ref, read_expr_context_ref, "Context member reference"},
    {"CONTEXT_ROW", 75, true, write_expr_context_row, read_expr_context_row, "Context row reference"},
    {"COMPLEX_CONTEXT_REF", 76, true, write_expr_complex_context_ref, read_expr_complex_context_ref,
        "Named context member reference"},
    {"NULL_COAL", 77, true, write_expr_null_coal, read_expr_null_coal, "Null coalescing operator"},
    {"VALUE_COAL", 78, true, write_expr_value_coal, read_expr_value_coal, "Value coalescing operator"},
    {"QUESTION", 79, true, write_expr_question, read_expr_question, "Ternary operator"},
    {"FOLDL", 80, true, write_expr_foldl, read_expr_foldl, "Fold-left operator"},
    {"FOLDR", 81, true, write_expr_foldr, read_expr_foldr, "Fold-right operator"},
    {"MAP", 82, true, write_expr_map, read_expr_map, "Map operator"},
    {"MAP_SELECT", 83, true, write_expr_map_select, read_expr_map_select, "Map-select operator"},
    {"HASH_MAP", 84, true, write_expr_hash_map, read_expr_hash_map, "Hash map operator"},
    {"HASH_MAP_SELECT", 85, true, write_expr_hash_map_select, read_expr_hash_map_select,
        "Hash map-select operator"},
    {"SELECT", 86, true, write_expr_select, read_expr_select, "Select operator"},
    {"LOG_LT", 87, true, write_expr_log_lt, read_expr_log_lt, "Logical less-than operator"},
    {"LOG_GT", 88, true, write_expr_log_gt, read_expr_log_gt, "Logical greater-than operator"},
    {"LOG_LE", 89, true, write_expr_log_le, read_expr_log_le, "Logical less-than-or-equals operator"},
    {"LOG_GE", 90, true, write_expr_log_ge, read_expr_log_ge, "Logical greater-than-or-equals operator"},
    {"LOG_AND", 91, true, write_expr_log_and, read_expr_log_and, "Logical AND operator"},
    {"LOG_OR", 92, true, write_expr_log_or, read_expr_log_or, "Logical OR operator"},
    {"CALLREF_CALL", 93, true, write_expr_callref_call, read_expr_callref_call, "Call reference call"},
    {"RANGE", 94, true, write_expr_range, read_expr_range, "Range operator"},
    {"ASSIGN", 95, true, write_expr_assign, read_expr_assign, "Assignment operator"},
    {"CAST_SCALAR", 96, true, write_expr_cast_scalar, read_expr_cast_scalar, "Scalar cast"},
    {"BIT_AND", 97, true, write_expr_bit_and, read_expr_bit_and, "Bitwise AND operator"},
    {"BIT_OR", 98, true, write_expr_bit_or, read_expr_bit_or, "Bitwise OR operator"},
    {"BIT_XOR", 99, true, write_expr_bit_xor, read_expr_bit_xor, "Bitwise XOR operator"},
    {"SHIFT_LEFT", 100, true, write_expr_shift_left, read_expr_shift_left, "Left shift operator"},
    {"SHIFT_RIGHT", 101, true, write_expr_shift_right, read_expr_shift_right, "Right shift operator"},
    {"SQUARE_BRACKET_RANGE", 102, true, write_expr_square_bracket_range, read_expr_square_bracket_range,
        "Range subscript operator"},
    {"DOT_EVAL_EXPR", 103, true, write_expr_dot_eval_expr, read_expr_dot_eval_expr,
        "Full dot-eval expression slot payload"},
    {"UNARY_MINUS", 104, true, write_expr_unary_minus, read_expr_unary_minus, "Unary minus operator"},
    {"LOG_AEQ", 105, true, write_expr_log_aeq, read_expr_log_aeq, "Logical absolute equality operator"},
    {"LOG_ANE", 106, true, write_expr_log_ane, read_expr_log_ane, "Logical absolute not-equals operator"},
    {"COMPLEX_BUFFER_NEW", 107, true, write_expr_complex_buffer_new, read_expr_complex_buffer_new,
        "Complex buffer construction"},
    {"ITERATE", 108, true, write_expr_iterate, read_expr_iterate, "Iterate operator"},
    {"STREAMING", 109, true, write_expr_streaming, read_expr_streaming, "Streaming operator"},
    {"DEFERRED_STATIC_METHOD_REF", 110, true, write_expr_deferred_static_method_ref,
        read_expr_deferred_static_method_ref, "Deferred static method reference"},
    {"DEFERRED_FUNCTION_REF", 111, true, write_expr_deferred_function_ref,
        read_expr_deferred_function_ref, "Deferred function call reference"},
    {"PLUS_EQ", 112, true, write_expr_plus_eq, read_expr_plus_eq, "Plus-equals operator"},
    {"MINUS_EQ", 113, true, write_expr_minus_eq, read_expr_minus_eq, "Minus-equals operator"},
    {"MULTIPLY_EQ", 114, true, write_expr_multiply_eq, read_expr_multiply_eq, "Multiply-equals operator"},
    {"DIVIDE_EQ", 115, true, write_expr_divide_eq, read_expr_divide_eq, "Divide-equals operator"},
    {"MODULO_EQ", 116, true, write_expr_modulo_eq, read_expr_modulo_eq, "Modulo-equals operator"},
    {"AND_EQ", 117, true, write_expr_and_eq, read_expr_and_eq, "Bitwise-and-equals operator"},
    {"OR_EQ", 118, true, write_expr_or_eq, read_expr_or_eq, "Bitwise-or-equals operator"},
    {"XOR_EQ", 119, true, write_expr_xor_eq, read_expr_xor_eq, "Bitwise-xor-equals operator"},
    {"SHL_EQ", 120, true, write_expr_shl_eq, read_expr_shl_eq, "Shift-left-equals operator"},
    {"SHR_EQ", 121, true, write_expr_shr_eq, read_expr_shr_eq, "Shift-right-equals operator"},
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
    {"EXPR_TREE", 254, true, write_expr_expr_tree, read_expr_expr_tree, "Recursive expression tree"},

    // 255: GENERIC_EVAL
    {"GENERIC_EVAL", 255, true, write_expr_generic_eval, read_expr_generic_eval, "Unsupported expression marker"},
};

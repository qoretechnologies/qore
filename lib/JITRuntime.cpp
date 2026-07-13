/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    JITRuntime.cpp

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

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/JITRuntime.h"
#include "qore/intern/QoreJITException.h"
#include "qore/intern/QoreRegexSubst.h"
#include "qore/intern/QoreRegexSubstOperatorNode.h"
#include "qore/intern/QoreTransliteration.h"
#include "qore/intern/QoreTransliterationOperatorNode.h"
#include "qore/intern/QoreIterateOperatorNode.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/qore_number_private.h"

// Macro for JIT runtime functions: check xsink and throw C++ exception
// if a Qore exception was raised. Used at return points of qore_rt_*
// functions that take ExceptionSink*. This enables LLVM's invoke/landingpad
// to handle exceptions via stack unwinding instead of manual flag checking.
#define QORE_RT_CHECK_THROW(xsink) do { \
    if ((xsink) && *(xsink)) { \
        throw QoreJITException(); \
    } \
} while(0)

#include <cstring>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <typeinfo>

#include <qore/ExceptionSink.h>
#include <qore/QoreValue.h>
#include <qore/QoreStringNode.h>
#include <qore/QoreHashNode.h>
#include <qore/QoreListNode.h>
#include <qore/DateTimeNode.h>
#include <qore/intern/QoreIRInterpreter.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/QoreAOT.h>
#include <qore/intern/QoreAOTBinary.h>
#include <qore/intern/QorePluginRegistry.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/Variable.h>
#include <qore/intern/AbstractStatement.h>
#include <qore/intern/QoreLibIntern.h>
#include <qore/intern/qore_thread_intern.h>
#include <qore/intern/QoreTypeInfo.h>
#include <qore/intern/QoreTypeSpec.h>
#include <qore/intern/QoreTimeZoneManager.h>
#include <qore/intern/OnBlockExitStatement.h>
#include <qore/intern/QoreException.h>
#include <qore/intern/StatementBlock.h>
#include <qore/intern/FunctionCallNode.h>
#include <qore/intern/SelfVarrefNode.h>
#include <qore/intern/QoreHashObjectDereferenceOperatorNode.h>
#include <qore/intern/QoreSquareBracketsOperatorNode.h>
#include <qore/intern/QoreSquareBracketsRangeOperatorNode.h>
#include <qore/intern/CallReferenceCallNode.h>
#include <qore/intern/FunctionalOperatorInterface.h>
#include <qore/intern/QoreClassIntern.h>
#include <qore/intern/CaseNodeRegex.h>
#include <qore/intern/SwitchStatement.h>
#include <qore/intern/ConstantList.h>
#include <qore/intern/QoreClosureParseNode.h>
#include <qore/intern/QoreClosureNode.h>
#include <qore/intern/NewComplexTypeNode.h>
#include <qore/intern/typed_hash_decl_private.h>
#include <qore/intern/qore_list_private.h>
#include <qore/intern/qore_string_private.h>
#include <qore/intern/QoreHashNodeIntern.h>
#include <qore/intern/ParseReferenceNode.h>
#include <qore/intern/VarRefNode.h>
#include <qore/intern/CallReferenceNode.h>
#include <qore/intern/QoreCastOperatorNode.h>
#include <qore/intern/QoreAOTBinary.h>
#include <qore/intern/Context.h>  // Context class (for native `context` IR lowering helpers)
#include <qore/intern/QoreObjectIntern.h>  // qore_object_private::takeMembers for HashKeySlice over object
#include <qore/intern/BackquoteNode.h>

static const QoreJITRuntimeSymbolInfo qore_jit_runtime_symbols[] = {
    { "qore_rt_check_stack", reinterpret_cast<void*>(&qore_rt_check_stack) },
    { "qore_rt_discard_on_block_exit", reinterpret_cast<void*>(&qore_rt_discard_on_block_exit) },
    { "qore_rt_add_any", reinterpret_cast<void*>(&qore_rt_add_any) },
    { "qore_rt_sub_any", reinterpret_cast<void*>(&qore_rt_sub_any) },
    { "qore_rt_mul_any", reinterpret_cast<void*>(&qore_rt_mul_any) },
    { "qore_rt_add_assign_any", reinterpret_cast<void*>(&qore_rt_add_assign_any) },
    { "qore_rt_sub_assign_any", reinterpret_cast<void*>(&qore_rt_sub_assign_any) },
    { "qore_rt_mul_assign_any", reinterpret_cast<void*>(&qore_rt_mul_assign_any) },
    { "qore_rt_add_assign_any_throwing", reinterpret_cast<void*>(&qore_rt_add_assign_any_throwing) },
    { "qore_rt_sub_assign_any_throwing", reinterpret_cast<void*>(&qore_rt_sub_assign_any_throwing) },
    { "qore_rt_mul_assign_any_throwing", reinterpret_cast<void*>(&qore_rt_mul_assign_any_throwing) },
    { "qore_rt_div_any", reinterpret_cast<void*>(&qore_rt_div_any) },
    { "qore_rt_mod_any", reinterpret_cast<void*>(&qore_rt_mod_any) },
    { "qore_rt_div_int", reinterpret_cast<void*>(&qore_rt_div_int) },
    { "qore_rt_mod_int", reinterpret_cast<void*>(&qore_rt_mod_int) },
    { "qore_rt_div_float", reinterpret_cast<void*>(&qore_rt_div_float) },
    { "qore_rt_to_int", reinterpret_cast<void*>(&qore_rt_to_int) },
    { "qore_rt_to_float", reinterpret_cast<void*>(&qore_rt_to_float) },
    { "qore_rt_to_bool", reinterpret_cast<void*>(&qore_rt_to_bool) },
    { "qore_rt_is_null_or_nothing", reinterpret_cast<void*>(&qore_rt_is_null_or_nothing) },
    { "qore_rt_incref", reinterpret_cast<void*>(&qore_rt_incref) },
    { "qore_rt_decref", reinterpret_cast<void*>(&qore_rt_decref) },
    { "qore_rt_decref_nothrow", reinterpret_cast<void*>(&qore_rt_decref_nothrow) },
    { "qore_rt_throw", reinterpret_cast<void*>(&qore_rt_throw) },
    { "qore_rt_throw_value", reinterpret_cast<void*>(&qore_rt_throw_value) },
    { "qore_rt_has_exception", reinterpret_cast<void*>(&qore_rt_has_exception) },
    { "qore_rt_invoke_expr", reinterpret_cast<void*>(&qore_rt_invoke_expr) },
    { "qore_rt_make_string", reinterpret_cast<void*>(&qore_rt_make_string) },
    { "qore_rt_make_string_len", reinterpret_cast<void*>(&qore_rt_make_string_len) },
    { "qore_rt_backquote", reinterpret_cast<void*>(&qore_rt_backquote) },
    { "qore_rt_backquote_throwing", reinterpret_cast<void*>(&qore_rt_backquote_throwing) },
    { "qore_rt_find", reinterpret_cast<void*>(&qore_rt_find) },
    { "qore_rt_find_mode", reinterpret_cast<void*>(&qore_rt_find_mode) },
    { "qore_rt_find_throwing", reinterpret_cast<void*>(&qore_rt_find_throwing) },
    { "qore_rt_find_mode_throwing", reinterpret_cast<void*>(&qore_rt_find_mode_throwing) },
    { "qore_rt_background_dot_eval_name_call_aot",
        reinterpret_cast<void*>(&qore_rt_background_dot_eval_name_call_aot) },
    { "qore_rt_background_dot_eval_name_call_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_background_dot_eval_name_call_aot_throwing) },
    { "qore_rt_background_static_method_name_call_aot",
        reinterpret_cast<void*>(&qore_rt_background_static_method_name_call_aot) },
    { "qore_rt_background_static_method_name_call_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_background_static_method_name_call_aot_throwing) },
    { "qore_rt_background_call_ref_value_aot",
        reinterpret_cast<void*>(&qore_rt_background_call_ref_value_aot) },
    { "qore_rt_background_call_ref_value_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_background_call_ref_value_aot_throwing) },
    { "qore_rt_catch_exception", reinterpret_cast<void*>(&qore_rt_catch_exception) },
    { "qore_rt_catch_end", reinterpret_cast<void*>(&qore_rt_catch_end) },
    { "qore_rt_rethrow", reinterpret_cast<void*>(&qore_rt_rethrow) },
    { "qore_rt_deopt", reinterpret_cast<void*>(&qore_rt_deopt) },
    { "qore_rt_guard_not_nothing", reinterpret_cast<void*>(&qore_rt_guard_not_nothing) },
    { "qore_rt_guard_int", reinterpret_cast<void*>(&qore_rt_guard_int) },
    { "qore_rt_guard_float", reinterpret_cast<void*>(&qore_rt_guard_float) },
    { "qore_rt_instantiate_local", reinterpret_cast<void*>(&qore_rt_instantiate_local) },
    { "qore_rt_assign_local", reinterpret_cast<void*>(&qore_rt_assign_local) },
    { "qore_rt_assign_local_no_coerce", reinterpret_cast<void*>(&qore_rt_assign_local_no_coerce) },
    { "qore_rt_assign_local_throwing", reinterpret_cast<void*>(&qore_rt_assign_local_throwing) },
    { "qore_rt_assign_local_no_coerce_throwing",
        reinterpret_cast<void*>(&qore_rt_assign_local_no_coerce_throwing) },
    { "qore_rt_make_weak_value", reinterpret_cast<void*>(&qore_rt_make_weak_value) },
    { "qore_rt_load_local", reinterpret_cast<void*>(&qore_rt_load_local) },
    { "qore_rt_uninstantiate_local", reinterpret_cast<void*>(&qore_rt_uninstantiate_local) },
    { "qore_rt_binary_op", reinterpret_cast<void*>(&qore_rt_binary_op) },
    { "qore_rt_list_index_dynamic", reinterpret_cast<void*>(&qore_rt_list_index_dynamic) },
    { "qore_rt_unary_op", reinterpret_cast<void*>(&qore_rt_unary_op) },
    { "qore_rt_plugin_unary", reinterpret_cast<void*>(&qore_rt_plugin_unary) },
    { "qore_rt_plugin_binary", reinterpret_cast<void*>(&qore_rt_plugin_binary) },
    { "qore_rt_plugin_call", reinterpret_cast<void*>(&qore_rt_plugin_call) },
    { "qore_rt_plugin_call_args", reinterpret_cast<void*>(&qore_rt_plugin_call_args) },
    { "qore_rt_plugin_subscript", reinterpret_cast<void*>(&qore_rt_plugin_subscript) },
    { "qore_rt_plugin_construct", reinterpret_cast<void*>(&qore_rt_plugin_construct) },
    { "qore_rt_plugin_construct_args", reinterpret_cast<void*>(&qore_rt_plugin_construct_args) },
    { "qore_rt_plugin_dense_buffer_unary", reinterpret_cast<void*>(&qore_rt_plugin_dense_buffer_unary) },
    { "qore_rt_plugin_dense_buffer_binary", reinterpret_cast<void*>(&qore_rt_plugin_dense_buffer_binary) },
    { "qore_rt_guard_plugin_type_profiled", reinterpret_cast<void*>(&qore_rt_guard_plugin_type_profiled) },
    { "qore_rt_expr_op", reinterpret_cast<void*>(&qore_rt_expr_op) },
    { "qore_rt_comparison_op", reinterpret_cast<void*>(&qore_rt_comparison_op) },
    { "qore_rt_ternary_op", reinterpret_cast<void*>(&qore_rt_ternary_op) },
    { "qore_rt_load_global", reinterpret_cast<void*>(&qore_rt_load_global) },
    { "qore_rt_store_global", reinterpret_cast<void*>(&qore_rt_store_global) },
    { "qore_rt_load_closure", reinterpret_cast<void*>(&qore_rt_load_closure) },
    { "qore_rt_store_closure", reinterpret_cast<void*>(&qore_rt_store_closure) },
    { "qore_rt_add_assign_local_int", reinterpret_cast<void*>(&qore_rt_add_assign_local_int) },
    { "qore_rt_increment_closure_int", reinterpret_cast<void*>(&qore_rt_increment_closure_int) },
    { "qore_rt_load_thread_local", reinterpret_cast<void*>(&qore_rt_load_thread_local) },
    { "qore_rt_store_thread_local", reinterpret_cast<void*>(&qore_rt_store_thread_local) },
    { "qore_rt_lvalue_load", reinterpret_cast<void*>(&qore_rt_lvalue_load) },
    { "qore_rt_lvalue_store", reinterpret_cast<void*>(&qore_rt_lvalue_store) },
    { "qore_rt_lvalue_unary", reinterpret_cast<void*>(&qore_rt_lvalue_unary) },
    { "qore_rt_lvalue_binary", reinterpret_cast<void*>(&qore_rt_lvalue_binary) },
    { "qore_rt_lvalue_ternary", reinterpret_cast<void*>(&qore_rt_lvalue_ternary) },
    { "qore_rt_make_list", reinterpret_cast<void*>(&qore_rt_make_list) },
    { "qore_rt_make_list_by_type_path", reinterpret_cast<void*>(&qore_rt_make_list_by_type_path) },
    { "qore_rt_make_hash", reinterpret_cast<void*>(&qore_rt_make_hash) },
    { "qore_rt_make_hash_by_type_path", reinterpret_cast<void*>(&qore_rt_make_hash_by_type_path) },
    { "qore_rt_hash_reserve", reinterpret_cast<void*>(&qore_rt_hash_reserve) },
    { "qore_rt_make_hash_const_keys", reinterpret_cast<void*>(&qore_rt_make_hash_const_keys) },
    { "qore_rt_make_hash_const_keys_by_type_path",
        reinterpret_cast<void*>(&qore_rt_make_hash_const_keys_by_type_path) },
    { "qore_rt_fixed_hash_remap2_aot",
        reinterpret_cast<void*>(&qore_rt_fixed_hash_remap2_aot) },
    { "qore_rt_fixed_hash_remap2_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_fixed_hash_remap2_aot_throwing) },
    { "qore_rt_exec_statement", reinterpret_cast<void*>(&qore_rt_exec_statement) },
    { "qore_rt_thread_exit", reinterpret_cast<void*>(&qore_rt_thread_exit) },
    { "qore_rt_context_init", reinterpret_cast<void*>(&qore_rt_context_init) },
    { "qore_rt_context_init_throwing", reinterpret_cast<void*>(&qore_rt_context_init_throwing) },
    { "qore_rt_context_ref_at", reinterpret_cast<void*>(&qore_rt_context_ref_at) },
    { "qore_rt_context_ref_at_throwing", reinterpret_cast<void*>(&qore_rt_context_ref_at_throwing) },
    { "qore_rt_context_row", reinterpret_cast<void*>(&qore_rt_context_row) },
    { "qore_rt_context_row_throwing", reinterpret_cast<void*>(&qore_rt_context_row_throwing) },
    { "qore_rt_iterate_value", reinterpret_cast<void*>(&qore_rt_iterate_value) },
    { "qore_rt_iterate_value_throwing", reinterpret_cast<void*>(&qore_rt_iterate_value_throwing) },
    { "qore_rt_iterator_create_iterate", reinterpret_cast<void*>(&qore_rt_iterator_create_iterate) },
    { "qore_rt_iterator_create_iterate_throwing",
        reinterpret_cast<void*>(&qore_rt_iterator_create_iterate_throwing) },
    { "qore_rt_context_max_pos", reinterpret_cast<void*>(&qore_rt_context_max_pos) },
    { "qore_rt_context_set_pos", reinterpret_cast<void*>(&qore_rt_context_set_pos) },
    { "qore_rt_context_destroy", reinterpret_cast<void*>(&qore_rt_context_destroy) },
    { "qore_rt_guard_type", reinterpret_cast<void*>(&qore_rt_guard_type) },
    { "qore_rt_make_date", reinterpret_cast<void*>(&qore_rt_make_date) },
    { "qore_rt_make_date_ex", reinterpret_cast<void*>(&qore_rt_make_date_ex) },
    { "qore_rt_make_enum", reinterpret_cast<void*>(&qore_rt_make_enum) },
    { "qore_rt_hash_key_access", reinterpret_cast<void*>(&qore_rt_hash_key_access) },
    { "qore_rt_hash_key_access_for_call", reinterpret_cast<void*>(&qore_rt_hash_key_access_for_call) },
    { "qore_rt_hash_key_access_prehashed", reinterpret_cast<void*>(&qore_rt_hash_key_access_prehashed) },
    { "qore_rt_hash_key_access_for_call_prehashed",
        reinterpret_cast<void*>(&qore_rt_hash_key_access_for_call_prehashed) },
    { "qore_rt_hash_key_truthy", reinterpret_cast<void*>(&qore_rt_hash_key_truthy) },
    { "qore_rt_hash_key_truthy_prehashed",
        reinterpret_cast<void*>(&qore_rt_hash_key_truthy_prehashed) },
    { "qore_rt_hash_key_truthy_guarded",
        reinterpret_cast<void*>(&qore_rt_hash_key_truthy_guarded) },
    { "qore_rt_hash_key_truthy_guarded_prehashed",
        reinterpret_cast<void*>(&qore_rt_hash_key_truthy_guarded_prehashed) },
    { "qore_rt_hash_key_access_int_prehashed",
        reinterpret_cast<void*>(&qore_rt_hash_key_access_int_prehashed) },
    { "qore_rt_select_hash_key_positive_int",
        reinterpret_cast<void*>(&qore_rt_select_hash_key_positive_int) },
    { "qore_rt_select_hash_key_positive_int_prehashed",
        reinterpret_cast<void*>(&qore_rt_select_hash_key_positive_int_prehashed) },
    { "qore_rt_list_index_access", reinterpret_cast<void*>(&qore_rt_list_index_access) },
    { "qore_rt_list_index_access_compat", reinterpret_cast<void*>(&qore_rt_list_index_access_compat) },
    { "qore_rt_string_concat", reinterpret_cast<void*>(&qore_rt_string_concat) },
    { "qore_rt_load_static_var", reinterpret_cast<void*>(&qore_rt_load_static_var) },
    { "qore_rt_load_static_var_for_call", reinterpret_cast<void*>(&qore_rt_load_static_var_for_call) },
    { "qore_rt_load_static_var_throwing", reinterpret_cast<void*>(&qore_rt_load_static_var_throwing) },
    { "qore_rt_load_static_var_for_call_throwing",
        reinterpret_cast<void*>(&qore_rt_load_static_var_for_call_throwing) },
    { "qore_rt_pseudo_list_bool_guarded", reinterpret_cast<void*>(&qore_rt_pseudo_list_bool_guarded) },
    { "qore_rt_pseudo_list_bool_guarded_aot", reinterpret_cast<void*>(&qore_rt_pseudo_list_bool_guarded_aot) },
    { "qore_rt_list_size", reinterpret_cast<void*>(&qore_rt_list_size) },
    { "qore_rt_list_get_int", reinterpret_cast<void*>(&qore_rt_list_get_int) },
    { "qore_rt_list_get_float", reinterpret_cast<void*>(&qore_rt_list_get_float) },
    { "qore_rt_list_get_int_unchecked", reinterpret_cast<void*>(&qore_rt_list_get_int_unchecked) },
    { "qore_rt_list_get_float_unchecked", reinterpret_cast<void*>(&qore_rt_list_get_float_unchecked) },
    { "qore_rt_list_get_data_unchecked", reinterpret_cast<void*>(&qore_rt_list_get_data_unchecked) },
    { "qore_rt_raise_typed_foreach_nothing",
        reinterpret_cast<void*>(&qore_rt_raise_typed_foreach_nothing) },
    { "qore_rt_load_local_aot", reinterpret_cast<void*>(&qore_rt_load_local_aot) },
    { "qore_rt_cleanup_run_allocas", reinterpret_cast<void*>(&qore_rt_cleanup_run_allocas) },
    { "qore_rt_reload_local_if_stale", reinterpret_cast<void*>(&qore_rt_reload_local_if_stale) },
    { "qore_rt_reload_local_if_stale_aot", reinterpret_cast<void*>(&qore_rt_reload_local_if_stale_aot) },
    { "qore_rt_assign_local_aot", reinterpret_cast<void*>(&qore_rt_assign_local_aot) },
    { "qore_rt_assign_local_no_coerce_aot", reinterpret_cast<void*>(&qore_rt_assign_local_no_coerce_aot) },
    { "qore_rt_assign_local_aot_throwing", reinterpret_cast<void*>(&qore_rt_assign_local_aot_throwing) },
    { "qore_rt_assign_local_no_coerce_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_assign_local_no_coerce_aot_throwing) },
    { "qore_rt_instantiate_local_aot", reinterpret_cast<void*>(&qore_rt_instantiate_local_aot) },
    { "qore_rt_uninstantiate_local_aot", reinterpret_cast<void*>(&qore_rt_uninstantiate_local_aot) },
    { "qore_rt_pop_closure_var_aot", reinterpret_cast<void*>(&qore_rt_pop_closure_var_aot) },
    { "qore_rt_load_global_aot", reinterpret_cast<void*>(&qore_rt_load_global_aot) },
    { "qore_rt_store_global_aot", reinterpret_cast<void*>(&qore_rt_store_global_aot) },
    { "qore_rt_load_thread_local_aot", reinterpret_cast<void*>(&qore_rt_load_thread_local_aot) },
    { "qore_rt_store_thread_local_aot", reinterpret_cast<void*>(&qore_rt_store_thread_local_aot) },
    { "qore_rt_load_closure_aot", reinterpret_cast<void*>(&qore_rt_load_closure_aot) },
    { "qore_rt_store_closure_aot", reinterpret_cast<void*>(&qore_rt_store_closure_aot) },
    { "qore_rt_add_assign_local_int_aot", reinterpret_cast<void*>(&qore_rt_add_assign_local_int_aot) },
    { "qore_rt_increment_closure_int_aot", reinterpret_cast<void*>(&qore_rt_increment_closure_int_aot) },
    { "qore_rt_invoke_expr_aot", reinterpret_cast<void*>(&qore_rt_invoke_expr_aot) },
    { "qore_rt_load_constant_aot", reinterpret_cast<void*>(&qore_rt_load_constant_aot) },
    { "qore_rt_list_assignment_value", reinterpret_cast<void*>(&qore_rt_list_assignment_value) },
    { "qore_rt_list_assignment_value_throwing", reinterpret_cast<void*>(&qore_rt_list_assignment_value_throwing) },
    { "qore_rt_load_static_var_by_path", reinterpret_cast<void*>(&qore_rt_load_static_var_by_path) },
    { "qore_rt_load_static_var_by_path_for_call",
        reinterpret_cast<void*>(&qore_rt_load_static_var_by_path_for_call) },
    { "qore_rt_load_static_var_by_path_throwing",
        reinterpret_cast<void*>(&qore_rt_load_static_var_by_path_throwing) },
    { "qore_rt_load_static_var_by_path_for_call_throwing",
        reinterpret_cast<void*>(&qore_rt_load_static_var_by_path_for_call_throwing) },
    { "qore_rt_load_static_var_by_path_aot",
        reinterpret_cast<void*>(&qore_rt_load_static_var_by_path_aot) },
    { "qore_rt_load_static_var_by_path_for_call_aot",
        reinterpret_cast<void*>(&qore_rt_load_static_var_by_path_for_call_aot) },
    { "qore_rt_load_static_var_by_path_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_load_static_var_by_path_aot_throwing) },
    { "qore_rt_load_static_var_by_path_for_call_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_load_static_var_by_path_for_call_aot_throwing) },
    { "qore_rt_create_closure_aot", reinterpret_cast<void*>(&qore_rt_create_closure_aot) },
    { "qore_rt_create_static_method_call_ref_aot",
        reinterpret_cast<void*>(&qore_rt_create_static_method_call_ref_aot) },
    { "qore_rt_create_static_method_call_ref_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_create_static_method_call_ref_aot_throwing) },
    { "qore_rt_create_function_call_ref_aot", reinterpret_cast<void*>(&qore_rt_create_function_call_ref_aot) },
    { "qore_rt_create_function_call_ref_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_create_function_call_ref_aot_throwing) },
    { "qore_rt_create_self_method_ref_aot", reinterpret_cast<void*>(&qore_rt_create_self_method_ref_aot) },
    { "qore_rt_create_self_method_ref_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_create_self_method_ref_aot_throwing) },
    { "qore_rt_create_object_method_ref_aot", reinterpret_cast<void*>(&qore_rt_create_object_method_ref_aot) },
    { "qore_rt_create_object_method_ref_aot_throwing",
        reinterpret_cast<void*>(&qore_rt_create_object_method_ref_aot_throwing) },
    { "qore_rt_create_parse_ref_aot", reinterpret_cast<void*>(&qore_rt_create_parse_ref_aot) },
    { "qore_rt_create_parse_ref_aot_resolved_hash_key",
        reinterpret_cast<void*>(&qore_rt_create_parse_ref_aot_resolved_hash_key) },
    { "qore_rt_create_local_ref_aot", reinterpret_cast<void*>(&qore_rt_create_local_ref_aot) },
    { "qore_rt_create_member_hash_ref_aot", reinterpret_cast<void*>(&qore_rt_create_member_hash_ref_aot) },
    { "qore_rt_new_hash_decl_aot", reinterpret_cast<void*>(&qore_rt_new_hash_decl_aot) },
    { "qore_rt_new_hash_decl_from_hash_by_path_cached",
        reinterpret_cast<void*>(&qore_rt_new_hash_decl_from_hash_by_path_cached) },
    { "qore_rt_new_complex_hash_aot", reinterpret_cast<void*>(&qore_rt_new_complex_hash_aot) },
    { "qore_rt_new_complex_list_aot", reinterpret_cast<void*>(&qore_rt_new_complex_list_aot) },
    { "qore_rt_new_complex_buffer_aot", reinterpret_cast<void*>(&qore_rt_new_complex_buffer_aot) },
    { "qore_rt_lvalue_load_aot", reinterpret_cast<void*>(&qore_rt_lvalue_load_aot) },
    { "qore_rt_lvalue_store_aot", reinterpret_cast<void*>(&qore_rt_lvalue_store_aot) },
    { "qore_rt_lvalue_unary_aot", reinterpret_cast<void*>(&qore_rt_lvalue_unary_aot) },
    { "qore_rt_lvalue_binary_aot", reinterpret_cast<void*>(&qore_rt_lvalue_binary_aot) },
    { "qore_rt_lvalue_ternary_aot", reinterpret_cast<void*>(&qore_rt_lvalue_ternary_aot) },
    { "qore_rt_lv_path_assign", reinterpret_cast<void*>(&qore_rt_lv_path_assign) },
    { "qore_rt_lv_path_compound", reinterpret_cast<void*>(&qore_rt_lv_path_compound) },
    { "qore_rt_lv_path_unary", reinterpret_cast<void*>(&qore_rt_lv_path_unary) },
    { "qore_rt_lv_path_binary_mut", reinterpret_cast<void*>(&qore_rt_lv_path_binary_mut) },
    { "qore_rt_lv_path_ternary", reinterpret_cast<void*>(&qore_rt_lv_path_ternary) },
    { "qore_rt_lv_path_assign_aot", reinterpret_cast<void*>(&qore_rt_lv_path_assign_aot) },
    { "qore_rt_lv_path_compound_aot", reinterpret_cast<void*>(&qore_rt_lv_path_compound_aot) },
    { "qore_rt_lv_path_unary_aot", reinterpret_cast<void*>(&qore_rt_lv_path_unary_aot) },
    { "qore_rt_lv_path_binary_mut_aot", reinterpret_cast<void*>(&qore_rt_lv_path_binary_mut_aot) },
    { "qore_rt_lv_path_ternary_aot", reinterpret_cast<void*>(&qore_rt_lv_path_ternary_aot) },
    { "qore_rt_push_on_block_exit_aot", reinterpret_cast<void*>(&qore_rt_push_on_block_exit_aot) },
    { "qore_rt_call_fast_with_target", reinterpret_cast<void*>(&qore_rt_call_fast_with_target) },
    { "qore_rt_call_with_args", reinterpret_cast<void*>(&qore_rt_call_with_args) },
    { "qore_rt_call_method_direct_consume_args",
        reinterpret_cast<void*>(&qore_rt_call_method_direct_consume_args) },
    { "qore_rt_call_method_fast_consume_args",
        reinterpret_cast<void*>(&qore_rt_call_method_fast_consume_args) },
    { "qore_rt_call_with_args_aot", reinterpret_cast<void*>(&qore_rt_call_with_args_aot) },
    { "qore_rt_call_with_args_aot_consume_args",
        reinterpret_cast<void*>(&qore_rt_call_with_args_aot_consume_args) },
    { "qore_rt_get_aot_call_target_context",
        reinterpret_cast<void*>(&qore_rt_get_aot_call_target_context) },
    { "qore_rt_try_get_aot_call_target_context",
        reinterpret_cast<void*>(&qore_rt_try_get_aot_call_target_context) },
    { "qore_rt_object_is_valid", reinterpret_cast<void*>(&qore_rt_object_is_valid) },
    { "qore_rt_load_object_getter_aot",
        reinterpret_cast<void*>(&qore_rt_load_object_getter_aot) },
    { "qore_rt_call_direct_aot_consume_args", reinterpret_cast<void*>(&qore_rt_call_direct_aot_consume_args) },
    { "qore_rt_call_static_method_direct_aot_consume_args",
        reinterpret_cast<void*>(&qore_rt_call_static_method_direct_aot_consume_args) },
    { "qore_rt_call_method_direct_aot_consume_args",
        reinterpret_cast<void*>(&qore_rt_call_method_direct_aot_consume_args) },
    { "qore_rt_call_method_fast_aot_consume_args",
        reinterpret_cast<void*>(&qore_rt_call_method_fast_aot_consume_args) },
    { "qore_rt_new_object_nb_consume_args",
        reinterpret_cast<void*>(&qore_rt_new_object_nb_consume_args) },
    { "qore_rt_new_object_nb_aot_consume_args",
        reinterpret_cast<void*>(&qore_rt_new_object_nb_aot_consume_args) },
    { "qore_rt_dot_eval_with_base", reinterpret_cast<void*>(&qore_rt_dot_eval_with_base) },
    { "qore_rt_dot_eval_with_base_aot", reinterpret_cast<void*>(&qore_rt_dot_eval_with_base_aot) },
    { "qore_rt_dot_eval_method_direct_consume_args",
        reinterpret_cast<void*>(&qore_rt_dot_eval_method_direct_consume_args) },
    { "qore_rt_dot_eval_method_direct_with_inst",
        reinterpret_cast<void*>(&qore_rt_dot_eval_method_direct_with_inst) },
    { "qore_rt_dot_eval_method_direct_with_inst_consume_args",
        reinterpret_cast<void*>(&qore_rt_dot_eval_method_direct_with_inst_consume_args) },
    { "qore_rt_dot_eval_pseudo_method_direct_consume_args",
        reinterpret_cast<void*>(&qore_rt_dot_eval_pseudo_method_direct_consume_args) },
    { "qore_rt_dot_eval_pseudo_method_direct_with_inst",
        reinterpret_cast<void*>(&qore_rt_dot_eval_pseudo_method_direct_with_inst) },
    { "qore_rt_dot_eval_pseudo_method_direct_with_inst_consume_args",
        reinterpret_cast<void*>(&qore_rt_dot_eval_pseudo_method_direct_with_inst_consume_args) },
    { "qore_rt_dot_eval_method_by_name_consume_args",
        reinterpret_cast<void*>(&qore_rt_dot_eval_method_by_name_consume_args) },
    { "qore_rt_dot_eval_method_by_name_with_inst",
        reinterpret_cast<void*>(&qore_rt_dot_eval_method_by_name_with_inst) },
    { "qore_rt_dot_eval_method_by_name_with_inst_consume_args",
        reinterpret_cast<void*>(&qore_rt_dot_eval_method_by_name_with_inst_consume_args) },
    { "qore_rt_dot_eval_method_direct_aot_consume_args",
        reinterpret_cast<void*>(&qore_rt_dot_eval_method_direct_aot_consume_args) },
    { "qore_rt_dot_eval_pseudo_method_direct_aot_consume_args",
        reinterpret_cast<void*>(&qore_rt_dot_eval_pseudo_method_direct_aot_consume_args) },
    { "qore_rt_regex_op_with_operand", reinterpret_cast<void*>(&qore_rt_regex_op_with_operand) },
    { "qore_rt_regex_op_with_operand_aot", reinterpret_cast<void*>(&qore_rt_regex_op_with_operand_aot) },
    { "qore_rt_switch_case_match", reinterpret_cast<void*>(&qore_rt_switch_case_match) },
};

const QoreJITRuntimeSymbolInfo* qore_jit_get_runtime_symbols(size_t& count) {
    count = sizeof(qore_jit_runtime_symbols) / sizeof(qore_jit_runtime_symbols[0]);
    return qore_jit_runtime_symbols;
}

bool qore_jit_validate_runtime_symbols(std::string& error) {
    std::set<std::string> names;
    size_t count = 0;
    const QoreJITRuntimeSymbolInfo* symbols = qore_jit_get_runtime_symbols(count);
    for (size_t i = 0; i < count; ++i) {
        const QoreJITRuntimeSymbolInfo& symbol = symbols[i];
        if (!symbol.name || !*symbol.name) {
            error = "JIT runtime helper registry contains an unnamed symbol at index " + std::to_string(i);
            return false;
        }
        if (!symbol.address) {
            error = "JIT runtime helper '" + std::string(symbol.name) + "' has a null address";
            return false;
        }
        auto [it, inserted] = names.insert(symbol.name);
        if (!inserted) {
            error = "duplicate JIT runtime helper symbol '" + std::string(symbol.name) + "'";
            return false;
        }
    }
    return true;
}

// --- Runtime location tracking for LLVM-generated code ---
// Returns pointer to the thread-local runtime_loc variable for per-line location updates.
// Called once at function entry; the returned pointer is stored and reused for each line change.
extern "C" DLLEXPORT const QoreProgramLocation** qore_rt_get_loc_ptr() {
    RuntimeLocationCache cache = get_runtime_location_cache();
    return cache.loc_ptr;
}

// Returns pointer to the thread-local runtime_statement variable.
extern "C" DLLEXPORT const AbstractStatement** qore_rt_get_stmt_ptr() {
    RuntimeLocationCache cache = get_runtime_location_cache();
    return cache.stmt_ptr;
}

// Returns pointer to the thread-local runtime_loc_sp variable. JIT-compiled code stores
// its current stack frame address here per line so the AOT lazy-throw resolver can tell
// a JIT frame (eager-tracked) from an AOT frame.
extern "C" DLLEXPORT uintptr_t* qore_rt_get_loc_frame_ptr() {
    RuntimeLocationCache cache = get_runtime_location_cache();
    return cache.sp_ptr;
}

// AOT mode: set runtime location from context location table.
// Per-line update: loads location pointer from ctx->locs[loc_index] and stores to TLS.
extern "C" DLLEXPORT void qore_rt_set_runtime_loc_aot(QoreAOTContext* ctx, int32_t loc_index) {
    if (ctx && ctx->locs && loc_index >= 0 && loc_index < ctx->num_locs && ctx->locs[loc_index]) {
        RuntimeLocationCache cache = get_runtime_location_cache();
        *cache.stmt_ptr = nullptr;
        *cache.loc_ptr = ctx->locs[loc_index];
    }
}

// --- Exported check_stack wrapper for LLVM-generated code ---
extern "C" DLLEXPORT int qore_rt_check_stack(ExceptionSink* xsink) {
#ifdef QORE_MANAGE_STACK
    return check_stack(xsink);
#else
    return 0;
#endif
}

// --- Forward declarations for Phase 5 fast-call builtins ---
extern "C" DLLEXPORT uint64_t qore_fast_strlen(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_now_us(ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_now_ms(ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_now(ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_time(ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_length(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_tolower(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_toupper(uint64_t arg_bits, ExceptionSink* xsink);
// Phase 5.2c: Pseudo-method fast-calls (read-only, non-mutating)
extern "C" DLLEXPORT uint64_t qore_fast_any_size(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_hash_keys(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_hash_values(uint64_t arg_bits, ExceptionSink* xsink);
// Phase 5.3: Additional fast-path optimizations
extern "C" DLLEXPORT uint64_t qore_fast_trim(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_abs(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_first(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_last(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_hash_exists(uint64_t hash_bits, uint64_t key_bits, ExceptionSink* xsink);
static size_t qore_short_string_utf8_length(QoreValue v);

// Fast string comparison helper matching QoreString::compare() semantics
// Returns: negative if l < r, 0 if equal, positive if l > r
// Empty strings sort at end (both "" vs "x" and "x" vs "" return 1)
static inline int fast_string_compare(const QoreStringNode* ls, const QoreStringNode* rs) {
    size_t llen = ls->size();
    size_t rlen = rs->size();

    // Handle empty strings - empty sorts at end (returns 1 if either is empty but not both)
    if (!llen) {
        return rlen ? 1 : 0;
    }
    if (!rlen) {
        return 1;  // right is empty, left is not -> left > right (empty at end)
    }

    // Compare bytes
    const char* lbuf = ls->c_str();
    const char* rbuf = rs->c_str();
    size_t minlen = llen < rlen ? llen : rlen;
    int rc = memcmp(lbuf, rbuf, minlen);
    if (rc != 0) {
        return rc < 0 ? -1 : 1;  // normalize like QoreString::compare
    }

    // Same prefix, compare lengths
    if (llen < rlen) {
        return -1;
    }
    if (llen > rlen) {
        return 1;
    }
    return 0;
}

// Helper: bit-cast between uint64_t and QoreValue.
// QoreValue is NaN-boxed and has the same size as uint64_t.
static_assert(sizeof(QoreValue) == sizeof(uint64_t), "QoreValue must be 64 bits for JIT ABI");

// Macro for always-inline attribute (portable across GCC, Clang, MSVC)
#ifdef _MSC_VER
    #define QORE_ALWAYS_INLINE __forceinline
#else
    #define QORE_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

// toBits/fromBits are defined in QoreJITIncludes.h (shared with QoreIRInterpreter.cpp)

// --- Reference counting helpers ---

// Increments reference count if value is a pointer (node), returns value unchanged
extern "C" DLLEXPORT uint64_t qore_rt_ref(uint64_t val) {
    QoreValue v = fromBits(val);
    if (v.hasNode()) {
        return toBits(v.refSelf());
    }
    return val;
}

// --- Arithmetic helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_add_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::AddAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_sub_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::SubAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_mul_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::MulAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_add_assign_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::AddAssignAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_sub_assign_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::SubAssignAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_mul_assign_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::MulAssignAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_div_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::DivAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_mod_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::ModAny, lv, rv, xsink);
    return toBits(result);
}

// Timeout arithmetic: `int + date` or `date + int` where the int is interpreted
// as milliseconds (Qore's `timeout` type is int-based milliseconds).  Delegates
// to evalBinary for the full typed logic (int→relative-date conversion + date add).
extern "C" DLLEXPORT uint64_t qore_rt_add_timeout(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::AddTimeout, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_sub_timeout(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::SubTimeout, lv, rv, xsink);
    return toBits(result);
}

// --- Number arithmetic (QoreNumberNode operations) ---

extern "C" DLLEXPORT uint64_t qore_rt_number_add(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreNumberNode* ln = lv.getType() == NT_NUMBER ? lv.get<const QoreNumberNode>() : nullptr;
    const QoreNumberNode* rn = rv.getType() == NT_NUMBER ? rv.get<const QoreNumberNode>() : nullptr;
    if (!ln || !rn) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(ln->doPlus(*rn)));
}

extern "C" DLLEXPORT uint64_t qore_rt_number_sub(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreNumberNode* ln = lv.getType() == NT_NUMBER ? lv.get<const QoreNumberNode>() : nullptr;
    const QoreNumberNode* rn = rv.getType() == NT_NUMBER ? rv.get<const QoreNumberNode>() : nullptr;
    if (!ln || !rn) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(ln->doMinus(*rn)));
}

extern "C" DLLEXPORT uint64_t qore_rt_number_mul(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreNumberNode* ln = lv.getType() == NT_NUMBER ? lv.get<const QoreNumberNode>() : nullptr;
    const QoreNumberNode* rn = rv.getType() == NT_NUMBER ? rv.get<const QoreNumberNode>() : nullptr;
    if (!ln || !rn) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(ln->doMultiply(*rn)));
}

extern "C" DLLEXPORT uint64_t qore_rt_number_div(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreNumberNode* ln = lv.getType() == NT_NUMBER ? lv.get<const QoreNumberNode>() : nullptr;
    const QoreNumberNode* rn = rv.getType() == NT_NUMBER ? rv.get<const QoreNumberNode>() : nullptr;
    if (!ln || !rn) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(ln->doDivideBy(*rn, xsink)));
}

// --- Integer/float division with zero check ---

extern "C" DLLEXPORT int64_t qore_rt_div_int(int64_t left, int64_t right, ExceptionSink* xsink) {
    if (!right) {
        if (xsink) {
            xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in integer expression");
        }
        return 0;
    }
    return left / right;
}

extern "C" DLLEXPORT int64_t qore_rt_mod_int(int64_t left, int64_t right, ExceptionSink* xsink) {
    if (!right) {
        if (xsink) {
            xsink->raiseException("DIVISION-BY-ZERO", "modula operand cannot be zero");
        }
        return 0;
    }
    return left % right;
}

extern "C" DLLEXPORT double qore_rt_div_float(double left, double right, ExceptionSink* xsink) {
    if (right == 0.0) {
        if (xsink) {
            xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in floating-point expression");
        }
        return 0.0;
    }
    return left / right;
}

// --- Conversion helpers ---

extern "C" DLLEXPORT int64_t qore_rt_to_int(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.getAsBigInt();
}

extern "C" DLLEXPORT double qore_rt_to_float(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.getAsFloat();
}

extern "C" DLLEXPORT int64_t qore_rt_to_bool(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.getAsBool() ? 1 : 0;
}

extern "C" DLLEXPORT int64_t qore_rt_is_null_or_nothing(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isNullOrNothing() ? 1 : 0;
}

// --- Refcount helpers ---

extern "C" DLLEXPORT void qore_rt_incref(uint64_t val) {
    QoreValue v = fromBits(val);
    if (v.hasNode()) {
        v.getInternalNode()->ref();
    }
}

extern "C" DLLEXPORT void qore_rt_decref(uint64_t val, ExceptionSink* xsink) {
    QoreValue v = fromBits(val);
    v.discard(xsink);
}

extern "C" DLLEXPORT void qore_rt_decref_nothrow(uint64_t val) {
    QoreValue v = fromBits(val);
    v.discard(nullptr);
}

static int clearConsumedArgCleanups(uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink) {
    if (!arg_cleanups) {
        return 0;
    }
    // Argument cleanups must run even if the callee already raised an
    // exception.  Preserve the original exception and suppress any cleanup
    // exception in that case; otherwise report cleanup failures normally.
    bool had_exception = xsink && *xsink;
    ExceptionSink cleanup_sink;
    ExceptionSink* cleanup_xsink = had_exception ? &cleanup_sink : xsink;
    for (int i = nargs - 1; i >= 0; --i) {
        uint64_t* slot = arg_cleanups[i];
        if (!slot) {
            continue;
        }
        uint64_t val = *slot;
        if (!val) {
            continue;
        }
        *slot = 0;
        qore_rt_decref(val, cleanup_xsink);
        if (cleanup_xsink && *cleanup_xsink) {
            if (had_exception) {
                cleanup_sink.clear();
                continue;
            }
            return -1;
        }
    }
    return 0;
}

extern "C" DLLEXPORT void qore_rt_clear_arg_cleanups(uint64_t** arg_cleanups,
        int32_t count, ExceptionSink* xsink) {
    clearConsumedArgCleanups(arg_cleanups, count, xsink);
}

// --- Cleanup stack for JIT/AOT compiled functions ---
// Replaces per-alloca cleanup tracking with a single runtime-managed array.
// This reduces the error_return block from O(N) instructions to O(1), eliminating
// LLVM optimization pathology on large functions.

//! Track a value for cleanup at scope exit.  Replaces the old per-cleanup-alloca
//! pattern with a single dynamically-grown array.
//! @param stack pointer to the stack pointer (alloca in LLVM IR)
//! @param count pointer to the count (alloca in LLVM IR)
//! @param val the NaN-boxed value to track
extern "C" DLLEXPORT void qore_rt_cleanup_push(uint64_t** stack, int32_t* count, uint64_t val) {
    QoreValue v = fromBits(val);
    if (!v.hasNode()) {
        return;  // Simple types (int, float, bool, NOTHING) don't need cleanup
    }
    int32_t n = *count;
    // Grow array if needed (initial allocation or doubling)
    if (n == 0) {
        *stack = (uint64_t*)malloc(16 * sizeof(uint64_t));
        if (!*stack) {
            return;
        }
    } else if ((n & (n - 1)) == 0 && n >= 16) {
        // Power of 2 — double the allocation
        uint64_t* new_stack = (uint64_t*)realloc(*stack, n * 2 * sizeof(uint64_t));
        if (!new_stack) {
            return;
        }
        *stack = new_stack;
    }
    (*stack)[n] = val;
    *count = n + 1;
}

//! Run all cleanup actions from an array of alloca pointers.
/** Each element in the array is a pointer to an i64 alloca. The function loads
    the value from each alloca and decrefs it. Used by emitInvokeCleanup() for
    large functions (50+ cleanup allocas) to avoid O(N) error_return blocks.
*/
extern "C" DLLEXPORT void qore_rt_cleanup_run_allocas(uint64_t** alloca_ptrs, int32_t count, ExceptionSink* xsink) {
    // Cleanup slots are registered in construction order.  Destruct in LIFO
    // order so dependent temporaries (for example iterators) are released
    // before the values they reference.
    for (int32_t i = count - 1; i >= 0; --i) {
        uint64_t* slot = alloca_ptrs[i];
        QoreValue v = fromBits(*slot);
        *slot = 0;
        v.discard(xsink);
    }
}

//! Run all cleanup actions (decref all tracked values) and free the array.
extern "C" DLLEXPORT void qore_rt_cleanup_run(uint64_t* stack, int32_t count, ExceptionSink* xsink) {
    if (!stack || count <= 0) {
        return;
    }
    // Process in reverse order (LIFO — matches scope-based cleanup)
    for (int32_t i = count - 1; i >= 0; --i) {
        QoreValue v = fromBits(stack[i]);
        v.discard(xsink);
    }
    free(stack);
}

// --- Exception helpers ---

//! Check xsink and throw C++ exception for LLVM stack unwinding
/** Called by JIT/AOT-compiled code after each qore_rt_* call that can
    raise a Qore exception. If xsink has an exception, throws QoreJITException
    which LLVM's invoke/landingpad mechanism catches for proper cleanup.
    This replaces the manual per-instruction xsink flag checking pattern.
*/
extern "C" DLLEXPORT void qore_rt_check_throw(ExceptionSink* xsink) {
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT void qore_rt_throw(ExceptionSink* xsink, const char* err, const char* desc) {
    if (xsink) {
        xsink->raiseException(err, desc);
    }
}

extern "C" DLLEXPORT void qore_rt_throw_value(ExceptionSink* xsink, uint64_t val) {
    if (!xsink) {
        return;
    }
    QoreValue arg = fromBits(val);
    if (arg.getType() == NT_LIST) {
        xsink->raiseException(arg.get<const QoreListNode>());
    } else {
        QoreValue owned_arg = arg.hasNode() ? arg.refSelf() : arg;
        xsink->raiseExceptionArg("THROW-ERROR", owned_arg, "throw");
    }
}

extern "C" DLLEXPORT __attribute__((pure)) int64_t qore_rt_has_exception(ExceptionSink* xsink) {
    return (xsink && *xsink) ? 1 : 0;
}

extern "C" DLLEXPORT int64_t qore_rt_check_cancel(ExceptionSink* xsink, const char* operation) {
    return qore_check_cancel(xsink, operation) ? 1 : 0;
}

// --- JIT deopt flag ---
// Thread-local flag set by JIT guard failure to request deopt to AST.
// evalTiered() checks this after JIT returns and re-executes via AST if set.
static thread_local bool tl_jit_deopt_requested = false;

// Empty string used as fallback call name when no cached IR function is available.
static const std::string jit_empty_call_name;

static LocalVar* findIRSelfLocal(const QoreIRFunction* ir) {
    if (!ir) {
        return nullptr;
    }
    LocalVar* named_self = nullptr;
    for (const auto& [lv, slot_id] : ir->local_var_slots) {
        (void)slot_id;
        if (!lv || !lv->getName()) {
            continue;
        }
        if (lv->isSelf()) {
            return const_cast<LocalVar*>(lv);
        }
        if (!named_self && !strcmp(lv->getName(), "self")) {
            named_self = const_cast<LocalVar*>(lv);
        }
    }
    return named_self;
}

extern "C" DLLEXPORT void qore_rt_request_jit_deopt(void* deopt_counter_ptr) {
    tl_jit_deopt_requested = true;
    if (deopt_counter_ptr) {
        auto* counter = static_cast<std::atomic<uint32_t>*>(deopt_counter_ptr);
        counter->fetch_add(1, std::memory_order_relaxed);
        printd(2, "qore_rt_request_jit_deopt: guard failure, deopt_count now %u\n",
            counter->load(std::memory_order_relaxed));
    }
}

DLLLOCAL bool qore_jit_deopt_requested() {
    bool val = tl_jit_deopt_requested;
    tl_jit_deopt_requested = false;
    return val;
}

// Debug-step hook (issue #5352): fires a per-statement debugger step event from
// JIT-compiled native code so single-stepping / breakpoints / LineCoverage work for
// a function already running as native code when a debugger attaches mid-execution.
// Mirrors the IR interpreter's per-statement dbgStep at PushTempMark boundaries
// (QoreIRInterpreter.cpp).  `file`/`abs_line` identify the statement (resolved at
// runtime via the current program's statement index); `statements_block` is the
// function's top-level StatementBlock passed as the onStep `blockStatement` context.
//
// Flow-control return codes from dbgStep (RC_RETURN/RC_BREAK/RC_CONTINUE) are
// intentionally NOT honored here: a fixed native frame cannot redirect control flow
// mid-execution.  Such commands take effect at the next function call boundary, where
// UserVariantBase::evalTiered() deopts to the AST/IR interpreter (hasDebuggerAttached
// downgrade).  A debugger-raised exception IS honored: the caller emits an exception
// check immediately after this call.
extern "C" DLLEXPORT void qore_rt_dbg_step(const char* file, int64_t abs_line,
        void* statements_block, ExceptionSink* xsink) {
    ThreadLocalProgramData* tlpd = get_thread_local_program_data();
    if (!tlpd || !tlpd->runtimeCheck()) {
        return;
    }
    QoreProgram* pgm = getProgram();
    if (!pgm) {
        return;
    }
    AbstractStatement* stmt = qore_program_private::get(*pgm)->getStatementFromIndex(file,
            static_cast<int>(abs_line));
    if (!stmt) {
        return;
    }
    tlpd->dbgStep(static_cast<const StatementBlock*>(statements_block), stmt, xsink);
}

// Synthetic block-entry debug event from JIT native code (issue #5352): mirrors the IR
// interpreter's DebugBlock handler firing dbgSyntheticBlockStep at block entry.  Resolves
// the entered block from the marker's line; falls back to the function's top-level block.
extern "C" DLLEXPORT void qore_rt_dbg_synthetic_block_step(const char* file, int64_t abs_line,
        void* statements_block, ExceptionSink* xsink) {
    ThreadLocalProgramData* tlpd = get_thread_local_program_data();
    if (!tlpd || !tlpd->runtimeCheck()) {
        return;
    }
    const StatementBlock* dbg_block = nullptr;
    QoreProgram* pgm = getProgram();
    if (pgm) {
        AbstractStatement* stmt = qore_program_private::get(*pgm)->getStatementFromIndex(file,
                static_cast<int>(abs_line));
        dbg_block = dynamic_cast<const StatementBlock*>(stmt);
    }
    if (!dbg_block) {
        dbg_block = static_cast<const StatementBlock*>(statements_block);
    }
    if (!dbg_block) {
        return;
    }
    tlpd->dbgSyntheticBlockStep(dbg_block, xsink);
}

// --- Invoke helpers ---

static uint64_t qore_rt_raise_ir_ast_fallback(uint64_t expr_bits, ExceptionSink* xsink,
        const char* helper, int opcode = -1, const char* reason = nullptr) {
    if (xsink && !*xsink) {
        QoreValue expr = fromBits(expr_bits);
        const AbstractQoreNode* node = expr.getInternalNode();
        const ParseNode* parse_node = dynamic_cast<const ParseNode*>(node);
        const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
        const char* opcode_name = opcode >= 0 ? getOpcodeName(opcode) : "<none>";
        const char* node_type = node ? typeid(*node).name() : "<null>";
        if (loc) {
            xsink->raiseException("IR-AST-FALLBACK-ERROR",
                "%s: executable AST expression fallback is disabled: opcode=%s(%d) "
                "expr_type=%s node_type=%s source=%s:%d: %s; add native IR/JIT lowering instead",
                helper ? helper : "<unknown helper>", opcode_name, opcode,
                expr.getTypeName(), node_type, loc->getFileValue(), loc->start_line,
                reason ? reason : "generic expression evaluation is forbidden");
        } else {
            xsink->raiseException("IR-AST-FALLBACK-ERROR",
                "%s: executable AST expression fallback is disabled: opcode=%s(%d) "
                "expr_type=%s node_type=%s: %s; add native IR/JIT lowering instead",
                helper ? helper : "<unknown helper>", opcode_name, opcode,
                expr.getTypeName(), node_type,
                reason ? reason : "generic expression evaluation is forbidden");
        }
    }
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_invoke_expr(uint64_t expr_bits, ExceptionSink* xsink) {
    return qore_rt_raise_ir_ast_fallback(expr_bits, xsink, "qore_rt_invoke_expr");
}

extern "C" DLLEXPORT uint64_t qore_rt_make_string(const char* str) {
    QoreStringNode* s = new QoreStringNode(str);
    QoreValue v(s);
    return toBits(v);
}

extern "C" DLLEXPORT uint64_t qore_rt_make_string_len(const char* str, uint64_t len) {
    QoreStringNode* s = new QoreStringNode(str, static_cast<size_t>(len));
    QoreValue v(s);
    return toBits(v);
}

extern "C" DLLEXPORT uint64_t qore_rt_backquote(const char* cmd, ExceptionSink* xsink) {
    int rc = 0;
    QoreStringNode* s = backquoteEval(cmd ? cmd : "", rc, xsink);
    return toBits(s ? QoreValue(s) : QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_find(uint64_t exp_bits, uint64_t find_exp_bits,
        uint64_t where_bits, ExceptionSink* xsink) {
    return qore_rt_find_mode(exp_bits, find_exp_bits, where_bits, 0, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_find_mode(uint64_t exp_bits, uint64_t find_exp_bits,
        uint64_t where_bits, int32_t mode, ExceptionSink* xsink) {
    QoreValue exp = fromBits(exp_bits);
    QoreValue find_exp = fromBits(find_exp_bits);
    QoreValue where = fromBits(where_bits);

    if (mode < 0 || mode > 3) {
        xsink->raiseException("IR-EXEC-ERROR", "invalid find mode %d", mode);
        return toBits(QoreValue());
    }

    ValueHolder rv(xsink);
    ReferenceHolder<Context> context(new Context(nullptr, xsink, find_exp), xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }

    QoreListNode* lrv = nullptr;
    size_t match_count = 0;
    for (context->pos = 0; context->pos < context->max_pos && !xsink->isEvent(); ++context->pos) {
        if ((context->pos & 0x3f) == 0 && qore_check_cancel(xsink, "find expression")) {
            return toBits(QoreValue());
        }
        bool b = context->check_condition(where, xsink);
        if (xsink && *xsink) {
            return toBits(QoreValue());
        }
        if (!b) {
            continue;
        }

        ++match_count;
        if (mode == 3 && match_count > 1) {
            xsink->raiseException("MULTIPLE-MATCHES-ERROR",
                "find one matched more than one row; at least %d rows matched", (int)match_count);
            return toBits(QoreValue());
        }

        ValueEvalOptimizedRefHolder result(exp, xsink);
        if (xsink && *xsink) {
            return toBits(QoreValue());
        }
        if (mode == 1) {
            return toBits(result.takeReferencedValue());
        }
        if (mode == 2 || mode == 3) {
            rv = result.takeReferencedValue();
            continue;
        }

        if (!rv->isNothing()) {
            if (!lrv) {
                lrv = new QoreListNode(autoTypeInfo);
                lrv->push(rv.release(), xsink);
                lrv->push(result.takeReferencedValue(), xsink);
                rv = lrv;
            } else {
                lrv->push(result.takeReferencedValue(), xsink);
            }
        } else {
            rv = result.takeReferencedValue();
        }
    }

    return toBits(rv.release());
}

// Thread-local stack for catch exception context
// Tracks the raw QoreException* and the saved previous td->catchException
// Push on CatchException, pop on CatchCleanup/Rethrow
struct CatchEntry {
    QoreException* caught;  // the caught exception
    QoreException* saved;   // the previous td->catchException value
};
static thread_local std::vector<CatchEntry> catch_stack;

extern "C" DLLEXPORT uint64_t qore_rt_catch_exception(ExceptionSink* xsink) {
    if (!xsink || !*xsink) {
        catch_stack.push_back({nullptr, nullptr});
        return toBits(QoreValue());
    }
    QoreException* caught = xsink->catchException();
    QoreException* saved = catch_swap_exception(caught);
    catch_stack.push_back({caught, saved});
    QoreHashNode* info = caught->makeExceptionObject();
    return toBits(QoreValue(info));
}

extern "C" DLLEXPORT void qore_rt_catch_end(ExceptionSink* xsink) {
    if (catch_stack.empty()) {
        return;
    }
    auto entry = catch_stack.back();
    catch_stack.pop_back();
    if (entry.caught) {
        catch_swap_exception(entry.saved);
        entry.caught->del(xsink);
    }
}

extern "C" DLLEXPORT void qore_rt_rethrow(ExceptionSink* xsink) {
    QoreException* ex = catch_get_exception();
    if (ex) {
        qore_es_private::get(*xsink)->rethrow(ex);
    }
    // Clean up catch scope after rethrow
    qore_rt_catch_end(xsink);
}

extern "C" DLLEXPORT void qore_rt_rethrow_with_args(uint64_t args_bits, ExceptionSink* xsink) {
    QoreException* ex = catch_get_exception();
    if (ex) {
        QoreValue args = fromBits(args_bits);
        // The args may contain unevaluated AST nodes (e.g., $1.err + "-NEW"
        // wrapped in a QoreListNode by RethrowStatement::parseInitImpl).
        // Evaluate like the AST path does via ValueEvalOptimizedRefHolder.
        if (args.needsEval()) {
            ValueEvalOptimizedRefHolder v(args, xsink);
            if (!*xsink && v->getType() == NT_LIST) {
                ex = ex->replaceTop(*v->get<const QoreListNode>(), *xsink);
            }
        } else if (args.getType() == NT_LIST) {
            ex = ex->replaceTop(*args.get<const QoreListNode>(), *xsink);
        }
        qore_es_private::get(*xsink)->rethrow(ex);
    }
    // Clean up catch scope after rethrow
    qore_rt_catch_end(xsink);
}

// --- Deopt helpers ---

extern "C" DLLEXPORT void qore_rt_deopt(void* deopt_counter_ptr) {
    // Atomically increment the deopt counter for the variant.
    // The evalTiered path checks this counter and triggers JIT recompilation
    // with updated type profiles when it exceeds a threshold.
    if (deopt_counter_ptr) {
        auto* counter = static_cast<std::atomic<uint32_t>*>(deopt_counter_ptr);
        counter->fetch_add(1, std::memory_order_relaxed);
        printd(2, "qore_rt_deopt: guard failure, deopt_count now %u\n",
            counter->load(std::memory_order_relaxed));
    }
}

// --- Guard helpers ---

extern "C" DLLEXPORT int64_t qore_rt_guard_not_nothing(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isNothing() ? 0 : 1;
}

extern "C" DLLEXPORT int64_t qore_rt_guard_int(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isInt() ? 1 : 0;
}

extern "C" DLLEXPORT int64_t qore_rt_guard_float(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isFloat() ? 1 : 0;
}

// --- Boxing helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_box_big_int(int64_t val) {
    QoreValue v(val);
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

// --- Local variable helpers ---

extern "C" DLLEXPORT void qore_rt_instantiate_local(LocalVar* var) {
    if (var) {
        // Don't create a duplicate CVV if the closure variable is already on the
        // cvstack WITHIN THE CURRENT FRAME (e.g., when emitLocalInstantiation
        // and a later InstantiateLocal opcode both target the same entry-block
        // closure-use local — the double-call must be idempotent).
        //
        // Using the frame-aware lookup is critical for recursive calls:
        // plain thread_try_find_closure_var walks all frames and would find
        // the OUTER frame's CVV for the same LocalVar name, causing the
        // inner frame to skip pushing its own CVV, then pop the outer's CVV
        // on inner exit — leaving outer closures with dangling captures and
        // a SIGSEGV on cleanup at outer return. Recursive calls share the
        // same LocalVar* pointer (and therefore the same name pointer)
        // across frames, so the frame boundary is the only way to
        // distinguish "my own frame's CVV" from an outer frame's.
        if (var->closureUse()
                && thread_has_runtime_closure_env()
                && thread_try_get_runtime_closure_var(var)) {
            return;
        }
        if (var->closureUse()
                && thread_try_find_closure_var_in_current_frame(var->getName())) {
            return;
        }
        var->instantiate(QoreParseOptions());
    }
}

static inline bool qore_rt_is_weak_reference_type(qore_type_t type) {
    return type == NT_WEAKREF || type == NT_WEAKREF_HASH || type == NT_WEAKREF_LIST;
}

static bool qore_rt_normalize_weak_reference_for_assignment(QoreValue& val, ValueHolder& holder,
        ExceptionSink* xsink) {
    if (!qore_rt_is_weak_reference_type(val.getType())) {
        return false;
    }
    holder = val.eval(xsink);
    if (xsink && *xsink) {
        return true;
    }
    val = *holder;
    return true;
}

static bool qore_rt_evaluate_owned_weak_reference_result(QoreValue& val, ExceptionSink* xsink) {
    if (!qore_rt_is_weak_reference_type(val.getType())) {
        return false;
    }
    ValueHolder old(val, xsink);
    val = old->eval(xsink);
    return true;
}

static void qore_rt_assign_local_impl(LocalVar* var, uint64_t value, ExceptionSink* xsink,
        bool check_types, bool normalize_weak_refs) {
    if (!var || *xsink) {
        return;
    }
    QoreValue val = fromBits(value);
    ValueHolder weak_eval_holder(xsink);
    if (normalize_weak_refs) {
        qore_rt_normalize_weak_reference_for_assignment(val, weak_eval_holder, xsink);
        if (xsink && *xsink) {
            return;
        }
    }
    LValueHelper helper(xsink);
    if (var->getLValue(helper, false, true)) {
        return;
    }
    // refSelf before assign — assign takes ownership of the reference
    QoreValue stored = val.hasNode() ? val.refSelf() : val;
    helper.assign(stored, "<lvalue>", check_types);
}

static void qore_rt_assign_closure_impl(ClosureVarValue* cvv, uint64_t value,
        ExceptionSink* xsink, bool normalize_weak_refs) {
    if (!cvv || *xsink) {
        return;
    }
    QoreValue val = fromBits(value);
    ValueHolder weak_eval_holder(xsink);
    if (normalize_weak_refs) {
        qore_rt_normalize_weak_reference_for_assignment(val, weak_eval_holder, xsink);
        if (xsink && *xsink) {
            return;
        }
    }
    LValueHelper helper(xsink);
    if (cvv->getLValue(helper, false, true)) {
        return;
    }
    QoreValue stored = val.hasNode() ? val.refSelf() : val;
    helper.assign(stored, "<lvalue>", true);
}

extern "C" DLLEXPORT void qore_rt_assign_local(LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_assign_local_impl(var, value, xsink, true, false);
}

extern "C" DLLEXPORT void qore_rt_assign_local_eval_weak(LocalVar* var, uint64_t value,
        ExceptionSink* xsink) {
    qore_rt_assign_local_impl(var, value, xsink, true, true);
}

static void qore_rt_apply_no_narrow_container_type(const QoreTypeInfo* ti, QoreValue& val,
        ExceptionSink* xsink) {
    // Keep StoreLocal coercion aligned with LValueHelper::assign(): hash<auto!>
    // and list<auto!> accept narrowed containers but must store them as auto
    // containers so later heterogeneous key/element writes remain valid.
    if (ti == anyTypeInfo || ti == autoNoNarrowTypeInfo) {
        if (val.getType() == NT_HASH) {
            map_get_plain_hash(val, xsink);
        } else if (val.getType() == NT_LIST) {
            map_get_plain_list(val, xsink);
        }
    } else if (ti == autoNoNarrowHashTypeInfo || ti == autoNoNarrowHashOrNothingTypeInfo) {
        if (val.getType() != NT_HASH) {
            return;
        }
        QoreHashNode* h = val.get<QoreHashNode>();
        qore_hash_private* hp = qore_hash_private::get(*h);
        if (!hp->getHashDecl() && hp->complexTypeInfo == autoHashTypeInfo) {
            return;
        }
        if (!h->is_unique()) {
            QoreHashNode* copy = h->copy();
            qore_hash_private* cp = qore_hash_private::get(*copy);
            if (cp->getHashDecl()) {
                cp->setHashDecl(nullptr);
            }
            cp->complexTypeInfo = autoHashTypeInfo;
            AbstractQoreNode* old = val.assign(copy);
            discard(old, xsink);
        } else {
            if (hp->getHashDecl()) {
                hp->setHashDecl(nullptr);
            }
            hp->complexTypeInfo = autoHashTypeInfo;
        }
    } else if (ti == autoNoNarrowListTypeInfo || ti == autoNoNarrowListOrNothingTypeInfo) {
        if (val.getType() != NT_LIST) {
            return;
        }
        QoreListNode* l = val.get<QoreListNode>();
        qore_list_private* lp = qore_list_private::get(*l);
        if (lp->complexTypeInfo == autoListTypeInfo) {
            return;
        }
        if (!l->is_unique()) {
            QoreListNode* copy = l->copy();
            qore_list_private::get(*copy)->complexTypeInfo = autoListTypeInfo;
            AbstractQoreNode* old = val.assign(copy);
            discard(old, xsink);
        } else {
            lp->complexTypeInfo = autoListTypeInfo;
        }
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_coerce_value(const QoreTypeInfo* ti, uint64_t value,
        uint64_t* cleanup_ptr, ExceptionSink* xsink) {
    ti = qore_substitute_type_params_if_needed(ti);
    QoreValue val = fromBits(value);
    ValueHolder weak_eval_holder(xsink);
    qore_rt_normalize_weak_reference_for_assignment(val, weak_eval_holder, xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    // Take our own reference before acceptAssignment so its internal
    // `discard(p)` (in the copy branch of acceptInputComplexList/Hash)
    // consumes OUR +1, not the caller's.  Otherwise, when the caller
    // passes a value loaded from an alloca-cached pre-instantiated local
    // (no owned +1), the discard underflows the runtime-stack variable's
    // refcount, causing a use-after-free at function exit.  The refSelf
    // is balanced by the `*cleanup_ptr = result` below — the caller's
    // cleanup alloca drops our +1 at function exit.
    //
    // Reproducer: MewsRestClient.qtest → RestClient.ping → pingImpl →
    // RestClientIo::restDoRequestIntern → HttpClientConnectionManager::request →
    // CookieJar::processResponseHeaders, specifically `cookie_strings =
    // set_cookie` where `set_cookie` is `auto` bound to `headers."set-cookie"`
    // (a list aliased into the shared response hash) and `cookie_strings` is
    // `list<string>`.  The inner-scope StoreLocal coerce-copy path freed the
    // shared list prematurely, tripping `AbstractQoreNode::deref` on dangling
    // bits when the outer response hash later dereffed.
    if (val.hasNode()) {
        val.refSelf();
    }
    QoreTypeInfo::acceptAssignment(ti, "<lvalue>", val, xsink);
    if (!xsink || !*xsink) {
        qore_rt_apply_no_narrow_container_type(ti, val, xsink);
    }
    uint64_t result = toBits(val);
    if (cleanup_ptr) {
        // Always track the output for cleanup at function exit.  In the
        // copy case `val` now points to the freshly-allocated coerced
        // container (refcount 1 from copy).  In the no-op case `val` is
        // the original input with an extra +1 from the refSelf above.
        // Either way there is exactly one +1 that the caller's cleanup
        // alloca must drop.
        *cleanup_ptr = result;
    }
    return result;
}

// Strip complex type info from hash/list values in place.
// Used when storing to plain "hash" or "list" typed variables:
// the IR/JIT creates hashes with narrowed types (e.g., hash<string, int>)
// but plain hash/list variables must not retain these narrowed types.
// Unlike map_get_plain_hash (which copies and frees the original), this
// modifies the value in place when it's unique (refcount 1), avoiding
// ownership transfer issues in the LLVM cleanup alloca tracking.
extern "C" DLLEXPORT void qore_rt_strip_complex_type(uint64_t value) {
    QoreValue val = fromBits(value);
    if (val.getType() == NT_HASH) {
        QoreHashNode* h = val.get<QoreHashNode>();
        if (h && !h->getHashDecl()) {
            qore_hash_private::get(*h)->complexTypeInfo = nullptr;
        }
    } else if (val.getType() == NT_LIST) {
        QoreListNode* l = val.get<QoreListNode>();
        if (l) {
            qore_list_private::get(*l)->complexTypeInfo = nullptr;
        }
    }
}

extern "C" DLLEXPORT void qore_rt_assign_local_no_coerce(LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_assign_local_impl(var, value, xsink, false, false);
}

extern "C" DLLEXPORT void qore_rt_assign_local_no_coerce_eval_weak(LocalVar* var, uint64_t value,
        ExceptionSink* xsink) {
    qore_rt_assign_local_impl(var, value, xsink, false, true);
}

extern "C" DLLEXPORT void qore_rt_sync_local(LocalVar* var, uint64_t value) {
    if (!var) {
        return;
    }
    if (var->isSelf()) {
        return;
    }
    if (var->closureUse()) {
        return;
    }

    ExceptionSink xsink;
    QoreValue val = fromBits(value);

    // This helper publishes the native local cache before running deferred
    // handlers.  It is not a Qore-level assignment, so reference locals must be
    // written to their raw stack slot; LocalVar::getLValue() would follow the
    // reference and write through to the referenced lvalue.
    LocalVarValue* lvv = thread_try_find_lvar(var);
    if (lvv && lvv->isRef()) {
        if (val.getType() == NT_REFERENCE) {
            QoreValue stored = val.refSelf();
            lvv->syncValue(stored, &xsink);
            if (xsink) {
                xsink.clear();
            }
        }
        return;
    }

    LValueHelper helper(&xsink);
    if (!var->getLValue(helper, false, true)) {
        QoreValue stored = val.hasNode() ? val.refSelf() : val;
        // This is a cache publication before running deferred handlers.  The
        // value has already passed normal StoreLocal/type checks in compiled code.
        helper.assign(stored, "<lvalue>", false);
    }
    if (xsink) {
        xsink.clear();
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_make_weak_value(uint64_t value, ExceptionSink* xsink) {
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    QoreValue val = fromBits(value);
    if (!val.hasNode()) {
        return value;
    }
    switch (val.getType()) {
        case NT_OBJECT:
            return toBits(QoreValue(new WeakReferenceNode(val.get<QoreObject>())));
        case NT_HASH:
            return toBits(QoreValue(new WeakHashReferenceNode(val.get<QoreHashNode>())));
        case NT_LIST:
            return toBits(QoreValue(new WeakListReferenceNode(val.get<QoreListNode>())));
        default:
            return toBits(val.refSelf());
    }
}

static QoreValue qore_rt_deref_loaded_var_value(QoreValue result, bool owned, ExceptionSink* xsink) {
    if (result.getType() != NT_REFERENCE) {
        if (!owned && result.hasNode()) {
            return result.refSelf();
        }
        return result;
    }

    ValueHolder ref_holder(owned ? result : result.refSelf(), xsink);
    bool needs_deref = true;
    QoreValue deref = result.getInternalNode()->eval(needs_deref, xsink);
    if (xsink && *xsink) {
        return QoreValue();
    }
    if (!needs_deref && deref.hasNode()) {
        deref = deref.refSelf();
    }
    return deref;
}

extern "C" DLLEXPORT uint64_t qore_rt_load_local(LocalVar* var, ExceptionSink* xsink) {
    if (!var) {
        return toBits(QoreValue());
    }
    bool needs_deref = true;
    QoreValue result = var->eval(needs_deref, xsink);
    QoreValue rv = qore_rt_deref_loaded_var_value(result, needs_deref, xsink);
    return toBits(rv);
}

extern "C" DLLEXPORT void qore_rt_reload_local_if_stale(LocalVar* var, uint64_t* cache,
        uint64_t* tracker, uint64_t* deferred, uint64_t* valid_epoch, uint64_t epoch,
        ExceptionSink* xsink) {
    assert(cache && tracker && deferred && valid_epoch);
    if (*valid_epoch == epoch) {
        return;
    }

    uint64_t old_tracker = *tracker;
    uint64_t old_deferred = *deferred;
    uint64_t reloaded = qore_rt_load_local(var, xsink);

    *cache = reloaded;
    *tracker = reloaded;
    *deferred = old_tracker;
    qore_rt_decref(old_deferred, xsink);
    *valid_epoch = epoch;
}

extern "C" DLLEXPORT uint64_t qore_rt_deref_if_reference(uint64_t val, ExceptionSink* xsink) {
    QoreValue v = fromBits(val);
    if (v.hasNode() && v.getType() == NT_REFERENCE) {
        // Dereference the reference like VarRefNode::evalImpl() does.
        // The reference node stays in the alloca (not deref'd here);
        // only the target value is returned with an extra reference.
        bool needs_deref = true;
        QoreValue result = v.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        return toBits(result);
    }
    return val;
}

extern "C" DLLEXPORT void qore_rt_clear_local(LocalVar* var, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    if (var->closureUse()) {
        // Closure-captured locals live on the cvstack.  Only clear the value
        // if no closures still hold references to this ClosureVarValue.
        // When references > 1, closures may still need to read the value
        // (e.g., closures submitted to a thread pool that haven't executed
        // yet).  When references == 1, only the cvstack entry remains, so
        // it's safe to trigger timely destruction at block scope exit.
        // NOTE: Use thread_try_find_closure_var() to avoid asserting when the
        // variable is not on the cvstack (e.g., in closure contexts on background threads).
        ClosureVarValue* cvv = thread_try_find_closure_var(var->getName());
        if (cvv && cvv->references.load(std::memory_order_acquire) == 1) {
            cvv->clearValue(xsink);
        }
    } else {
        // Find the local on the thread-local variable stack by name pointer
        LocalVarValue* lvar = thread_try_find_lvar(var);
        if (lvar) {
            // del() calls val.removeValue(true).discard(xsink) — no LValueHelper
            // assert, safe even when xsink already has an exception from a prior
            // destructor in the same scope
            lvar->del(xsink);
        }
    }
}

extern "C" DLLEXPORT void qore_rt_uninstantiate_local(LocalVar* var, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    // For closure-use variables: trigger deterministic destruction
    // by clearing the CVV value when it's the last reference,
    // matching the qore_rt_clear_local() pattern
    if (var->closureUse()) {
        ClosureVarValue* cvv = thread_try_find_closure_var(var->getName());
        if (cvv && cvv->references.load(std::memory_order_acquire) == 1) {
            cvv->clearValue(xsink);
        }
    }
    var->uninstantiate(xsink);
}

// --- Generic opcode dispatch helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_binary_op(int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink) {
    ValueEvalOptimizedRefHolder lv(fromBits(left), xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }

    ValueEvalOptimizedRefHolder rv(fromBits(right), xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }

    QoreValue result = QoreIRInterpreter::evalBinary(static_cast<QoreIROpcode>(opcode), *lv, *rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_list_index_dynamic(uint64_t left, uint64_t right,
        int32_t string_index_char, ExceptionSink* xsink) {
    ValueEvalOptimizedRefHolder lv(fromBits(left), xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }

    ValueEvalOptimizedRefHolder rv(fromBits(right), xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }

    bool negative_offsets = runtime_check_parse_option(PO_NEGATIVE_OFFSETS);
    QoreValue result = QoreSquareBracketsOperatorNode::doSquareBrackets(*lv, *rv, true, string_index_char != 0,
        negative_offsets, xsink);
    return toBits(result);
}

static QoreValue qore_rt_build_selector_range_values(int64 start, int64 stop, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> rv(new QoreListNode(bigIntTypeInfo), xsink);
    size_t cancel_i = 0;
    int64 step = start <= stop ? 1 : -1;
    for (int64 i = start;; i += step) {
        rv->push(QoreValue(i), xsink);
        if (*xsink || i == stop) {
            break;
        }
        if ((++cancel_i & 0x3fff) == 0 && qore_check_cancel(xsink, "range selector slice")) {
            return QoreValue();
        }
    }
    return *xsink ? QoreValue() : rv.release();
}

static QoreValue qore_rt_build_selector_range(const QoreValue& seq, QoreValue start, QoreValue stop,
        bool negative_offsets, ExceptionSink* xsink) {
    if (negative_offsets
            && (start.isNothing() || stop.isNothing() || start.getAsBigInt() < 0 || stop.getAsBigInt() < 0)) {
        int64 effective_start;
        int64 effective_stop;
        int64 seq_size;
        bool has_range = QoreSquareBracketsRangeOperatorNode::getEffectiveRange(seq, effective_start,
            effective_stop, seq_size, start, stop,
            static_cast<bool>(runtime_get_parse_options() & PO_BROKEN_LIST_RANGE), negative_offsets, xsink);
        if (*xsink) {
            return QoreValue();
        }
        if (!has_range) {
            return new QoreListNode(bigIntTypeInfo);
        }
        return qore_rt_build_selector_range_values(effective_start, effective_stop, xsink);
    }

    if (start.isNothing()) {
        xsink->raiseException("RANGE-ERROR", "the start expression of the range operator (..) evaluated to NOTHING");
        return QoreValue();
    }
    if (stop.isNothing()) {
        xsink->raiseException("RANGE-ERROR", "the end expression of the range operator (..) evaluated to NOTHING");
        return QoreValue();
    }

    int64 s = start.getAsBigInt();
    int64 e = stop.getAsBigInt();
    return qore_rt_build_selector_range_values(s, e, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_list_index_selectors(uint64_t left_bits, const uint8_t* kinds,
        int32_t count, const uint64_t* selector_bits, int32_t string_index_char, ExceptionSink* xsink) {
    QoreValue left = fromBits(left_bits);
    qore_type_t left_type = left.getType();
    int selector_idx = 0;
    bool negative_offsets = runtime_check_parse_option(PO_NEGATIVE_OFFSETS);

    if (left_type == NT_LIST) {
        const QoreTypeInfo* vtype = nullptr;
        bool vcommon = false;
        bool have_value = false;
        ReferenceHolder<QoreListNode> ret(new QoreListNode(autoTypeInfo), xsink);
        for (int32_t i = 0; i < count; ++i) {
            bool is_range = kinds[i] != 0;
            ValueHolder selector(xsink);
            if (is_range) {
                QoreValue start = fromBits(selector_bits[selector_idx++]);
                QoreValue stop = fromBits(selector_bits[selector_idx++]);
                selector = qore_rt_build_selector_range(left, start, stop, negative_offsets, xsink);
            } else {
                selector = fromBits(selector_bits[selector_idx++]).refSelf();
            }
            if (*xsink) {
                return toBits(QoreValue());
            }

            ValueHolder entry(QoreSquareBracketsOperatorNode::doSquareBrackets(left, *selector, is_range,
                string_index_char != 0, negative_offsets, xsink), xsink);
            if (*xsink) {
                return toBits(QoreValue());
            }
            if (is_range) {
                assert(entry->getType() == NT_LIST);
                ConstListIterator li(entry->get<const QoreListNode>());
                while (li.next()) {
                    QoreValue n = li.getValue();
                    if (!have_value) {
                        vtype = n.getTypeInfo();
                        vcommon = true;
                        have_value = true;
                    } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, n.getTypeInfo())) {
                        vcommon = false;
                    }
                    ret->push(n.refSelf(), xsink);
                    if (*xsink) {
                        return toBits(QoreValue());
                    }
                }
            } else {
                if (!have_value) {
                    vtype = entry->getTypeInfo();
                    vcommon = true;
                    have_value = true;
                } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, entry->getTypeInfo())) {
                    vcommon = false;
                }
                ret->push(entry.release(), xsink);
                if (*xsink) {
                    return toBits(QoreValue());
                }
            }
        }

        if (!vtype || vtype == anyTypeInfo) {
            vtype = autoTypeInfo;
        }
        const QoreTypeInfo* ti = qore_get_complex_list_type(vtype);
        qore_list_private::get(**ret)->complexTypeInfo = ti;
        QoreValue rv(ret.release());
        if (QoreTypeInfo::hasType(vtype)) {
            QoreTypeInfo::acceptAssignment(ti, "<type folding>", rv, xsink);
            if (*xsink) {
                rv.discard(xsink);
                return toBits(QoreValue());
            }
        }
        return toBits(rv);
    }

    if (left_type == NT_STRING) {
        SimpleRefHolder<QoreStringNode> ret(new QoreStringNode);
        for (int32_t i = 0; i < count; ++i) {
            bool is_range = kinds[i] != 0;
            ValueHolder selector(xsink);
            if (is_range) {
                QoreValue start = fromBits(selector_bits[selector_idx++]);
                QoreValue stop = fromBits(selector_bits[selector_idx++]);
                selector = qore_rt_build_selector_range(left, start, stop, negative_offsets, xsink);
            } else {
                selector = fromBits(selector_bits[selector_idx++]).refSelf();
            }
            if (*xsink) {
                return toBits(QoreValue());
            }
            ValueHolder entry(QoreSquareBracketsOperatorNode::doSquareBrackets(left, *selector, is_range,
                string_index_char != 0, negative_offsets, xsink), xsink);
            if (*xsink) {
                return toBits(QoreValue());
            }
            if (!entry->isNothing()) {
                QoreStringValueHelper str(*entry);
                qore_string_private::get(*ret)->concat(*str);
            }
        }
        return toBits(QoreValue(ret.release()));
    }

    if (left_type == NT_BINARY) {
        SimpleRefHolder<BinaryNode> ret(new BinaryNode);
        for (int32_t i = 0; i < count; ++i) {
            bool is_range = kinds[i] != 0;
            ValueHolder selector(xsink);
            if (is_range) {
                QoreValue start = fromBits(selector_bits[selector_idx++]);
                QoreValue stop = fromBits(selector_bits[selector_idx++]);
                selector = qore_rt_build_selector_range(left, start, stop, negative_offsets, xsink);
            } else {
                selector = fromBits(selector_bits[selector_idx++]).refSelf();
            }
            if (*xsink) {
                return toBits(QoreValue());
            }
            ValueHolder entry(QoreSquareBracketsOperatorNode::doSquareBrackets(left, *selector, is_range,
                string_index_char != 0, negative_offsets, xsink), xsink);
            if (*xsink) {
                return toBits(QoreValue());
            }
            switch (entry->getType()) {
                case NT_INT: {
                    unsigned char c = static_cast<unsigned char>(entry->getAsBigInt());
                    ret->append(&c, 1);
                    break;
                }
                case NT_BINARY:
                    ret->append(entry->get<BinaryNode>());
                    break;
                default:
                    assert(entry->getType() == NT_NOTHING);
                    break;
            }
        }
        return toBits(QoreValue(ret.release()));
    }

    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_unary_op(int opcode, uint64_t operand, ExceptionSink* xsink) {
    QoreValue val = fromBits(operand);
    QoreValue result = QoreIRInterpreter::evalUnary(static_cast<QoreIROpcode>(opcode), val, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_expr_op(int opcode, uint64_t expr_bits, ExceptionSink* xsink) {
    return qore_rt_raise_ir_ast_fallback(expr_bits, xsink, "qore_rt_expr_op", opcode);
}

extern "C" DLLEXPORT uint64_t qore_rt_comparison_op(int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalComparison(static_cast<QoreIROpcode>(opcode), lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_ternary_op(int opcode, uint64_t a, uint64_t b, uint64_t c, ExceptionSink* xsink) {
    QoreValue va = fromBits(a);
    QoreValue vb = fromBits(b);
    QoreValue vc = fromBits(c);
    QoreValue result = QoreIRInterpreter::evalTernary(static_cast<QoreIROpcode>(opcode), va, vb, vc, xsink);
    return toBits(result);
}

// --- Variable access helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_load_global(Var* var, ExceptionSink* xsink) {
    if (!var) {
        return toBits(QoreValue());
    }
    // Var::eval() returns an already-referenced value
    QoreValue result = var->eval();
    return toBits(qore_rt_deref_loaded_var_value(result, true, xsink));
}

static void qore_rt_store_global_impl(Var* var, uint64_t value, ExceptionSink* xsink,
        bool normalize_weak_refs) {
    if (!var) {
        return;
    }
    QoreValue val = fromBits(value);
    ValueHolder weak_eval_holder(xsink);
    if (normalize_weak_refs) {
        qore_rt_normalize_weak_reference_for_assignment(val, weak_eval_holder, xsink);
        if (xsink && *xsink) {
            return;
        }
    }
    QoreValue stored = val.hasNode() ? val.refSelf() : val;
    LValueHelper helper(xsink);
    if (!var->getLValue(helper, false)) {
        helper.assign(stored);
    }
}

extern "C" DLLEXPORT void qore_rt_store_global(Var* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_store_global_impl(var, value, xsink, false);
}

extern "C" DLLEXPORT void qore_rt_store_global_eval_weak(Var* var, uint64_t value,
        ExceptionSink* xsink) {
    qore_rt_store_global_impl(var, value, xsink, true);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_closure(ClosureVarValue* var, ExceptionSink* xsink) {
    if (!var) {
        return toBits(QoreValue());
    }
    bool needs_deref = true;
    QoreValue result = var->eval(needs_deref, xsink);
    return toBits(qore_rt_deref_loaded_var_value(result, needs_deref, xsink));
}

static void qore_rt_store_closure_impl(ClosureVarValue* var, uint64_t value, ExceptionSink* xsink,
        bool normalize_weak_refs) {
    if (!var) {
        return;
    }
    QoreValue val = fromBits(value);
    ValueHolder weak_eval_holder(xsink);
    if (normalize_weak_refs) {
        qore_rt_normalize_weak_reference_for_assignment(val, weak_eval_holder, xsink);
        if (xsink && *xsink) {
            return;
        }
    }
    QoreValue stored = val.hasNode() ? val.refSelf() : val;
    LValueHelper helper(xsink);
    if (!var->getLValue(helper, false)) {
        helper.assign(stored);
    }
}

extern "C" DLLEXPORT void qore_rt_store_closure(ClosureVarValue* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_store_closure_impl(var, value, xsink, false);
}

extern "C" DLLEXPORT void qore_rt_store_closure_eval_weak(ClosureVarValue* var, uint64_t value,
        ExceptionSink* xsink) {
    qore_rt_store_closure_impl(var, value, xsink, true);
}

static int64_t qore_rt_apply_int_delta(LValueHelper& helper, int64_t delta) {
    if (delta == 1) {
        return helper.preIncrementBigInt("<integer increment>");
    }
    if (delta == -1) {
        return helper.preDecrementBigInt("<integer decrement>");
    }
    return helper.plusEqualsBigInt(delta, "<integer add assignment>");
}

extern "C" DLLEXPORT int64_t qore_rt_add_assign_local_int(LocalVar* var,
        int64_t delta, ExceptionSink* xsink) {
    if (!var || (xsink && *xsink)) {
        return 0;
    }
    LValueHelper helper(xsink);
    if (var->getLValue(helper, false, false)) {
        return 0;
    }
    return qore_rt_apply_int_delta(helper, delta);
}

static int64_t qore_rt_increment_closure_int_impl(ClosureVarValue* cvv,
        int64_t delta, ExceptionSink* xsink) {
    if (!cvv || (xsink && *xsink)) {
        return 0;
    }
    LValueHelper helper(xsink);
    if (cvv->getLValue(helper, false)) {
        return 0;
    }
    return qore_rt_apply_int_delta(helper, delta);
}

extern "C" DLLEXPORT int64_t qore_rt_increment_closure_int(LocalVar* var,
        int64_t delta, ExceptionSink* xsink) {
    ClosureVarValue* cvv = var ? thread_resolve_runtime_closure_var(var) : nullptr;
    if (cvv) {
        return qore_rt_increment_closure_int_impl(cvv, delta, xsink);
    }
    return qore_rt_add_assign_local_int(var, delta, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_thread_local(Var* var, ExceptionSink* xsink) {
    // Thread-local variables use the same Var class; eval() resolves per-thread
    if (!var) {
        return toBits(QoreValue());
    }
    QoreValue result = var->eval();
    return toBits(qore_rt_deref_loaded_var_value(result, true, xsink));
}

extern "C" DLLEXPORT void qore_rt_store_thread_local(Var* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_store_global(var, value, xsink);
}

extern "C" DLLEXPORT void qore_rt_store_thread_local_eval_weak(Var* var, uint64_t value,
        ExceptionSink* xsink) {
    qore_rt_store_global_eval_weak(var, value, xsink);
}

// --- Self member access helper ---

static int qore_rt_check_closure_self_valid(QoreObject* obj, ExceptionSink* xsink) {
    return (obj && qore_closure_self_context(obj))
        ? qore_object_private::get(*obj)->checkClosureSelfValid(xsink)
        : 0;
}

extern "C" DLLEXPORT uint64_t qore_rt_load_self_member(const char* member_name, ExceptionSink* xsink) {
    QoreObject* obj = runtime_get_stack_object();
    assert(obj);
    if (qore_rt_check_closure_self_valid(obj, xsink)) {
        return toBits(QoreValue());
    }
    // issue 3523: evaluate in case the value is a reference
    ValueHolder val(obj->getReferencedMemberNoMethod(member_name, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(val->needsEval() ? val->eval(xsink) : val.release());
}

// Variant that does NOT evaluate needsEval() values (e.g., WeakReferenceNode).
// Returns the raw member value — safe when the result is used as a DotEval
// base (all method dispatch helpers handle NT_WEAKREF).  Avoids creating a
// temporary strong reference to the weak-ref target, which in the LLVM
// codegen would live until function exit and prevent timely object destruction
// (causing shutdown hangs for long-running functions like PipelineQueue::run()).
extern "C" DLLEXPORT uint64_t qore_rt_load_self_member_for_call(const char* member_name, ExceptionSink* xsink) {
    QoreObject* obj = runtime_get_stack_object();
    assert(obj);
    if (qore_rt_check_closure_self_valid(obj, xsink)) {
        return toBits(QoreValue());
    }
    ValueHolder val(obj->getReferencedMemberNoMethod(member_name, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(val.release());
}

// --- Static class variable access helper ---

static uint64_t qore_rt_load_static_var_impl(QoreVarInfo* vi, const char* var_name, ExceptionSink* xsink,
        bool preserve_weak_result) {
    // issue 3523: evaluate in case the value is a reference
    ValueHolder val(vi->getReferencedValue(var_name, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(!preserve_weak_result && val->needsEval() ? val->eval(xsink) : val.release());
}

static QoreValue qore_rt_eval_runtime_var(Var* var, ExceptionSink* xsink) {
    QoreValue v = var->eval();
    AbstractQoreNode* n = v.getInternalNode();
    if (n && n->getType() == NT_REFERENCE) {
        ReferenceNode* r = reinterpret_cast<ReferenceNode*>(n);
        bool needs_deref = true;
        QoreValue nv = r->eval(needs_deref, xsink);
        discard(v.getInternalNode(), xsink);
        return nv;
    }
    return v;
}

extern "C" DLLEXPORT uint64_t qore_rt_load_static_var(QoreVarInfo* vi, const char* var_name,
        ExceptionSink* xsink) {
    return qore_rt_load_static_var_impl(vi, var_name, xsink, false);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_static_var_for_call(QoreVarInfo* vi, const char* var_name,
        ExceptionSink* xsink) {
    return qore_rt_load_static_var_impl(vi, var_name, xsink, true);
}

static uint64_t qore_rt_load_static_var_by_path_impl(QoreProgram* pgm, const char* class_path,
        const char* var_name, ExceptionSink* xsink, bool preserve_weak_result) {
    if (!class_path || !var_name || !*var_name) {
        xsink->raiseException("STATIC-VAR-ERROR", "cannot resolve static variable '%s::%s'",
            class_path ? class_path : "<null>", var_name ? var_name : "<null>");
        return toBits(QoreValue());
    }

    if (!pgm) {
        xsink->raiseException("STATIC-VAR-ERROR", "cannot resolve static variable '%s::%s': no program context",
            class_path, var_name);
        return toBits(QoreValue());
    }

    const char* resolved_class_path = (class_path[0] == ':' && class_path[1] == ':')
        ? class_path + 2 : class_path;
    qore_program_private* pp = qore_program_private::get(*pgm);

    std::string full_path;
    if (*resolved_class_path) {
        full_path = resolved_class_path;
        full_path += "::";
        full_path += var_name;
    } else {
        full_path = var_name;
    }

    const qore_ns_private* found_ns = nullptr;
    if (Var* var = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, full_path.c_str(), found_ns)) {
        return toBits(qore_rt_eval_runtime_var(var, xsink));
    }
    if (*xsink) {
        return toBits(QoreValue());
    }

    found_ns = nullptr;
    if (const ConstantEntry* ce = qore_root_ns_private::runtimeFindNamespaceConstant(*pp->RootNS,
            full_path.c_str(), found_ns)) {
        return toBits(ce->getReferencedValue());
    }

    if (!*resolved_class_path) {
        xsink->raiseException("STATIC-VAR-ERROR", "cannot resolve variable or constant '%s'", var_name);
        return toBits(QoreValue());
    }

    found_ns = nullptr;
    if (const QoreEnumDecl* ed = qore_root_ns_private::runtimeFindEnum(*pp->RootNS, resolved_class_path,
            found_ns)) {
        if (const QoreEnumMember* member = ed->findMember(var_name)) {
            return toBits(QoreValue::makeEnum(member));
        }
    }

    found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, resolved_class_path, found_ns);
    if (!qc) {
        xsink->raiseException("STATIC-VAR-ERROR", "cannot resolve class '%s' for static variable '%s'",
            class_path, var_name);
        return toBits(QoreValue());
    }

    const QoreClass* owner_qc = qc;
    const QoreExternalStaticMember* member = qc->findLocalStaticMember(var_name);
    if (!member) {
        QoreClassHierarchyIterator hi(*qc);
        unsigned hierarchy_count = 0;
        while (hi.next()) {
            if (++hierarchy_count % 100 == 0
                    && qore_check_cancel(xsink, "static variable hierarchy lookup")) {
                return toBits(QoreValue());
            }
            const QoreClass& parent_qc = hi.get();
            member = parent_qc.findLocalStaticMember(var_name);
            if (member) {
                owner_qc = &parent_qc;
                break;
            }
        }
    }
    if (!member) {
        const QoreExternalConstant* c = qc->findConstant(var_name);
        if (!c) {
            QoreClassHierarchyIterator hi(*qc);
            unsigned hierarchy_count = 0;
            while (hi.next()) {
                if (++hierarchy_count % 100 == 0
                        && qore_check_cancel(xsink, "static constant hierarchy lookup")) {
                    return toBits(QoreValue());
                }
                c = hi.get().findConstant(var_name);
                if (c) {
                    break;
                }
            }
        }
        if (c) {
            return toBits(c->getReferencedValue());
        }
        xsink->raiseException("STATIC-VAR-ERROR", "cannot resolve static variable '%s::%s'",
            class_path, var_name);
        return toBits(QoreValue());
    }

    QoreVarInfo* vi = const_cast<QoreVarInfo*>(
        reinterpret_cast<const QoreVarInfo*>(member));
    (void)owner_qc;
    return qore_rt_load_static_var_impl(vi, var_name, xsink, preserve_weak_result);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_static_var_by_path(const char* class_path,
        const char* var_name, ExceptionSink* xsink) {
    return qore_rt_load_static_var_by_path_impl(getProgram(), class_path, var_name, xsink, false);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_static_var_by_path_for_call(const char* class_path,
        const char* var_name, ExceptionSink* xsink) {
    return qore_rt_load_static_var_by_path_impl(getProgram(), class_path, var_name, xsink, true);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_static_var_by_path_aot(QoreAOTContext* ctx, const char* class_path,
        const char* var_name, ExceptionSink* xsink) {
    return qore_rt_load_static_var_by_path_impl(ctx && ctx->pgm ? ctx->pgm : getProgram(),
        class_path, var_name, xsink, false);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_static_var_by_path_for_call_aot(QoreAOTContext* ctx,
        const char* class_path, const char* var_name, ExceptionSink* xsink) {
    return qore_rt_load_static_var_by_path_impl(ctx && ctx->pgm ? ctx->pgm : getProgram(),
        class_path, var_name, xsink, true);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_object(const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        const QoreListNode* args, ExceptionSink* xsink) {
    RuntimeConfig& rc = rc_get_current_ref();
    return toBits(qore_class_private::execConstructor(*qc, rc, variant, args, xsink));
}

// --- Constant loading helper ---

extern "C" DLLEXPORT uint64_t qore_rt_load_constant(const RuntimeConstantRefNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<RuntimeConstantRefNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_constant_value(uint64_t val_bits) {
    QoreValue val = fromBits(val_bits);
    return toBits(val.refSelf());
}

extern "C" DLLEXPORT uint64_t qore_rt_load_constant_aot(QoreAOTContext* ctx, int32_t idx,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    if (expr.hasNode()) {
        if (auto* node = dynamic_cast<const RuntimeConstantRefNode*>(expr.getInternalNode())) {
            return qore_rt_load_constant(node, xsink);
        }
    }
    if (expr.needsEval()) {
        return toBits(expr.eval(xsink));
    }
    return qore_rt_load_constant_value(ctx->exprs[idx]);
}

// --- Closure creation helper ---

extern "C" DLLEXPORT uint64_t qore_rt_create_closure(const QoreClosureParseNode* cn, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<QoreClosureParseNode*>(cn)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_create_closure_aot(QoreAOTContext* ctx, int32_t idx,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    auto* cn = dynamic_cast<const QoreClosureParseNode*>(expr.getInternalNode());
    if (!cn) {
        xsink->raiseException("AOT-ERROR", "invalid expression for closure creation AOT call");
        return toBits(QoreValue());
    }
    return qore_rt_create_closure(cn, xsink);
}

// --- Cast helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_cast_with_inner(uint64_t cast_expr_bits, uint64_t inner_bits,
        ExceptionSink* xsink) {
    QoreValue cast_expr = fromBits(cast_expr_bits);
    QoreValue inner = fromBits(inner_bits);

    if (!cast_expr.hasNode()) {
        if (xsink) {
            xsink->raiseException("IR-CAST-ERROR", "missing cast expression node");
        }
        return toBits(QoreValue());
    }

    auto* cast_node = dynamic_cast<const QoreCastOperatorNode*>(cast_expr.getInternalNode());
    if (!cast_node) {
        if (xsink) {
            xsink->raiseException("IR-CAST-ERROR", "cast expression is not a resolved cast operator");
        }
        return toBits(QoreValue());
    }

    QoreValue result = cast_node->castValue(inner, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_cast_with_inner_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t inner_bits, ExceptionSink* xsink) {
    return qore_rt_cast_with_inner(ctx->exprs[slot], inner_bits, xsink);
}

// Runtime casts evaluate their input expression before type checking.  The
// type-path cast helper receives a precomputed value, so mirror that evaluation
// step for weak references here.
static QoreValue qore_rt_cast_normalize_weak_ref(const QoreValue& val) {
    switch (val.getType()) {
        case NT_WEAKREF: {
            QoreObject* obj = val.get<const WeakReferenceNode>()->get();
            return obj && obj->isValid() ? QoreValue(obj) : QoreValue();
        }
        case NT_WEAKREF_HASH: {
            QoreHashNode* h = val.get<const WeakHashReferenceNode>()->get();
            return h ? QoreValue(h) : QoreValue();
        }
        case NT_WEAKREF_LIST: {
            QoreListNode* l = val.get<const WeakListReferenceNode>()->get();
            return l ? QoreValue(l) : QoreValue();
        }
        default:
            return val;
    }
}

// Cast by type path: resolves the cast type at runtime from a string path,
// then performs the cast on the pre-evaluated inner value.
// Eliminates the need for EXPR_TREE serialization of cast operator nodes.
static uint64_t qore_rt_cast_by_type_path_in_program(uint64_t inner_bits,
        const char* type_path, int64_t or_nothing, ExceptionSink* xsink, QoreProgram* pgm) {
    QoreValue inner = qore_rt_cast_normalize_weak_ref(fromBits(inner_bits));

    if (!type_path || !*type_path) {
        xsink->raiseException("IR-CAST-ERROR", "missing cast type path");
        return toBits(QoreValue());
    }

    if (!pgm) {
        xsink->raiseException("IR-CAST-ERROR", "no program context for cast type resolution");
        return toBits(QoreValue());
    }

    // Resolve the type path.  IR emission emits the FULL nullable path
    // (e.g. `*list<string>`) for or-nothing casts and sets or_nothing=1
    // alongside, but QoreAOTTypeResolver's `resolveBuiltin` /
    // `resolveComplexType` only know the non-nullable form — feeding
    // them a leading `*` gives "cannot resolve type path".  Strip the
    // prefix before resolution; the `or_nothing` flag already carries
    // the semantic.  Matches the QoreTypeInfo convention where
    // `list<string>` and `*list<string>` share the base type and differ
    // only in the or-nothing flag.
    const char* resolve_path = type_path;
    if (or_nothing && resolve_path[0] == '*') {
        ++resolve_path;
    }
    std::string error;
    QoreAOTTypeResolver resolver(pgm);
    const QoreTypeInfo* ti = resolver.resolve(resolve_path, error);

    if (ti && QoreScalarCastOperatorNode::isSupportedCastType(ti)) {
        QoreValue result = QoreScalarCastOperatorNode::castValueToType(ti, or_nothing != 0, inner, xsink);
        return toBits(result);
    }

    // Try class cast first
    const QoreClass* qc = ti ? QoreTypeInfo::getUniqueReturnClass(ti) : nullptr;
    if (qc || (ti && QoreTypeInfo::isType(ti, NT_OBJECT))) {
        // Class cast: check if value is an object of the right class
        if (inner.isNothing() && or_nothing) {
            return toBits(QoreValue());
        }
        if (inner.getType() != NT_OBJECT) {
            xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to %s'%s'",
                inner.getTypeName(), qc ? "class " : "", qc ? qc->getName() : "object");
            return toBits(QoreValue());
        }
        if (qc) {
            const QoreObject* obj = inner.get<const QoreObject>();
            const QoreClass* oc = obj->getClass();
            bool priv;
            const QoreClass* tc = oc->getClass(*qc, priv);
            if (!tc) {
                xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from class '%s' to class '%s'",
                    obj->getClassName(), qc->getName());
                return toBits(QoreValue());
            }
            if (priv && !qore_class_private::runtimeCheckPrivateClassAccess(*tc)) {
                xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from class '%s' to "
                    "privately-accessible class '%s' in this context", obj->getClassName(), qc->getName());
                return toBits(QoreValue());
            }
        }
        return inner.hasNode() ? toBits(inner.refSelf()) : toBits(inner);
    }

    // Try hashdecl cast
    const TypedHashDecl* hd = ti ? QoreTypeInfo::getUniqueReturnHashDecl(ti) : nullptr;
    if (!hd) {
        // Try direct hashdecl lookup by path
        const QoreNamespace* pns = nullptr;
        hd = pgm->findHashDecl(resolve_path, pns);
    }
    if (hd) {
        if (inner.isNothing() && or_nothing) {
            return toBits(QoreValue());
        }
        if (inner.getType() != NT_HASH) {
            xsink->raiseException("RUNTIME-CAST-ERROR", "cannot cast from type '%s' to hashdecl '%s'",
                inner.getTypeName(), hd->getName());
            return toBits(QoreValue());
        }
        // Perform the same checked/coercing hashdecl cast as
        // QoreHashDeclCastOperatorNode::castValue(). The or_nothing flag only
        // controls whether NOTHING is accepted; it must not disable member
        // type checks for ordinary hashdecl casts.
        QoreValue result = typed_hash_decl_private::get(*hd)->newHash(inner.get<const QoreHashNode>(),
            true, xsink);
        return toBits(result);
    }

    // Try complex hash cast, e.g. cast<hash<string, hash<X>>>(value).
    // For these, `ti` is a complex hash type (value-typed hash); route through
    // newComplexHashFromHash to clear nested hashdecl bindings and set the
    // correct complexTypeInfo on the result.
    if (ti && QoreTypeInfo::getUniqueReturnComplexHash(ti)) {
        if (inner.isNothing() && or_nothing) {
            return toBits(QoreValue());
        }
        if (inner.getType() != NT_HASH) {
            xsink->raiseException("RUNTIME-CAST-ERROR",
                "cannot cast from type '%s' to '%s'",
                inner.getTypeName(), type_path);
            return toBits(QoreValue());
        }
        QoreHashNode* init_hash = inner.get<QoreHashNode>()->hashRefSelf();
        QoreValue result = qore_hash_private::newComplexHashFromHash(ti, init_hash, xsink);
        return toBits(result);
    }

    // Try plain hash cast, e.g. cast<hash>(value).  The parser lowers this to
    // QoreHashDeclCastOperatorNode with a null hashdecl, so mirror that node's
    // castValue() behavior instead of treating bare "hash" as unresolved.
    if (!strcmp(resolve_path, "hash")) {
        if (inner.isNothing() && or_nothing) {
            return toBits(QoreValue());
        }
        if (inner.getType() != NT_HASH) {
            xsink->raiseException("RUNTIME-CAST-ERROR",
                "cannot cast from type '%s' to 'hash'", inner.getTypeName());
            return toBits(QoreValue());
        }
        const QoreHashNode* h = inner.get<const QoreHashNode>();
        if (!h->getHashDecl() && !h->getValueTypeInfo()) {
            return inner.hasNode() ? toBits(inner.refSelf()) : toBits(inner);
        }
        return toBits(qore_hash_private::getPlainHash(inner.get<QoreHashNode>()->hashRefSelf()));
    }

    // Try complex list cast, e.g. cast<list<X>>(value).
    if (ti && QoreTypeInfo::getUniqueReturnComplexList(ti)) {
        if (inner.isNothing() && or_nothing) {
            return toBits(QoreValue());
        }
        if (inner.getType() != NT_LIST) {
            xsink->raiseException("RUNTIME-CAST-ERROR",
                "cannot cast from type '%s' to '%s'",
                inner.getTypeName(), type_path);
            return toBits(QoreValue());
        }
        // List cast: return the value with the desired element type.
        // newComplexListFromValue takes ownership of inner's ref.
        QoreValue result = qore_list_private::newComplexListFromValue(ti,
            inner.refSelf(), xsink);
        return toBits(result);
    }

    // Try plain list cast, e.g. cast<list>(value).  For plain list casts the
    // cast node carries a null complex type, so the AOT emitter passes "list".
    if (!strcmp(resolve_path, "list")) {
        if (inner.isNothing() && or_nothing) {
            return toBits(QoreValue());
        }
        if (inner.getType() != NT_LIST) {
            xsink->raiseException("RUNTIME-CAST-ERROR",
                "cannot cast from type '%s' to 'list'", inner.getFullTypeName());
            return toBits(QoreValue());
        }
        if (inner.getFullTypeInfo() == listTypeInfo) {
            return inner.hasNode() ? toBits(inner.refSelf()) : toBits(inner);
        }
        return toBits(qore_list_private::getPlainList(inner.get<QoreListNode>()->listRefSelf()));
    }

    // Try enum cast, e.g. cast<HttpClientConnectionState>(int_value).
    // Mirrors QoreEnumCastOperatorNode::castValue semantics.
    // Use getReturnEnum so or-nothing enum casts (*enum<Foo>) also match —
    // the or_nothing flag handles the NOTHING input branch.
    const QoreEnumDecl* ed = ti ? QoreTypeInfo::getReturnEnum(ti) : nullptr;
    if (ed) {
        if (inner.isNothing() && or_nothing) {
            return toBits(QoreValue());
        }
        // Re-casting an already-typed enum value
        if (inner.isEnum()) {
            const QoreEnumMember* src_member = inner.getEnumMember();
            if (src_member->getEnumDecl() == ed) {
                return inner.hasNode() ? toBits(inner.refSelf()) : toBits(inner);
            }
            // Different enum: check the base value against target
            QoreValue base_val = src_member->getValue();
            if (!ed->isValidValue(base_val)) {
                xsink->raiseException("RUNTIME-CAST-ERROR",
                    "cannot cast value from enum '%s' to enum '%s'; value is not a valid "
                    "enum member", src_member->getEnumDecl()->getName(), ed->getName());
                return toBits(QoreValue());
            }
            const QoreEnumMember* member = ed->findMemberByValue(base_val);
            assert(member);
            return toBits(QoreValue::makeEnum(member));
        }
        // Check base type
        qore_type_t base_type = QoreTypeInfo::getBaseType(ed->getBaseTypeInfo());
        if (inner.getType() != base_type) {
            xsink->raiseException("RUNTIME-CAST-ERROR",
                "cannot cast from type '%s' to enum '%s'; expected %s value",
                inner.getTypeName(), ed->getName(),
                QoreTypeInfo::getName(ed->getBaseTypeInfo()));
            return toBits(QoreValue());
        }
        if (!ed->isValidValue(inner)) {
            QoreStringMaker desc("cannot cast value ");
            if (base_type == NT_STRING) {
                QoreStringValueHelper str(inner);
                desc.sprintf("'%s'", str->c_str());
            } else if (base_type == NT_INT) {
                desc.sprintf("%lld", inner.getAsBigInt());
            } else {
                desc.sprintf("of type '%s'", inner.getTypeName());
            }
            desc.sprintf(" to enum '%s'; value is not a valid enum member", ed->getName());
            xsink->raiseException("RUNTIME-CAST-ERROR", desc.c_str());
            return toBits(QoreValue());
        }
        const QoreEnumMember* member = ed->findMemberByValue(inner);
        assert(member);
        return toBits(QoreValue::makeEnum(member));
    }

    // Fallback: unsupported cast type
    xsink->raiseException("IR-CAST-ERROR", "cannot resolve cast type '%s'", type_path);
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_cast_by_type_path(uint64_t inner_bits,
        const char* type_path, int64_t or_nothing, ExceptionSink* xsink) {
    return qore_rt_cast_by_type_path_in_program(inner_bits, type_path, or_nothing, xsink, getProgram());
}

extern "C" DLLEXPORT uint64_t qore_rt_cast_by_type_path_aot(QoreAOTContext* ctx, uint64_t inner_bits,
        const char* type_path, int64_t or_nothing, ExceptionSink* xsink) {
    QoreProgram* pgm = ctx && ctx->pgm ? ctx->pgm : getProgram();
    return qore_rt_cast_by_type_path_in_program(inner_bits, type_path, or_nothing, xsink, pgm);
}

static bool qore_rt_apply_complex_hash_value_type(const QoreHashNode* h, const char* key,
        QoreValue& result, ExceptionSink* xsink) {
    if (!h || h->getHashDecl() || result.isNothing()) {
        return true;
    }
    const QoreTypeInfo* vti = h->getValueTypeInfo();
    if (!QoreTypeInfo::hasType(vti) || vti == autoTypeInfo || vti == anyTypeInfo
            || QoreTypeInfo::runtimeAcceptsValue(vti, result) != QTI_NOT_EQUAL) {
        return true;
    }

    ValueHolder holder(result, xsink);
    QoreTypeInfo::acceptInputKey(vti, key, *holder, xsink);
    if (xsink && *xsink) {
        return false;
    }
    result = holder.release();
    return true;
}

// --- Call reference creation helper ---

extern "C" DLLEXPORT uint64_t qore_rt_create_call_ref(uint64_t expr_bits, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return toBits(QoreValue());
    }
    bool needs_deref = true;
    QoreValue ref_expr = expr.refSelf();
    QoreValue result = ref_expr.getInternalNode()->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    ref_expr.discard(xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_create_static_method_call_ref_aot(const char* class_path,
        const char* method_name, ExceptionSink* xsink) {
    if (!class_path || !*class_path || !method_name || !*method_name) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot resolve method call reference '%s::%s()'",
            class_path ? class_path : "<null>", method_name ? method_name : "<null>");
        return toBits(QoreValue());
    }

    QoreProgram* pgm = getProgram();
    if (!pgm) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve method call reference '%s::%s()': no program context",
            class_path, method_name);
        return toBits(QoreValue());
    }

    const QoreClass* qc = qore_aot_resolve_class_ref(pgm, class_path, false);
    if (!qc) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve class '%s' for method call reference '%s()'",
            class_path, method_name);
        return toBits(QoreValue());
    }

    ClassAccess access = Public;
    const QoreMethod* method = qc->findStaticMethod(method_name, access);
    if (!method) {
        method = qc->findMethod(method_name, access);
    }
    if (!method) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve method call reference '%s::%s()'",
            class_path, method_name);
        return toBits(QoreValue());
    }
    if (access > Public && !qore_class_private::runtimeCheckPrivateClassAccess(*qc)) {
        xsink->raiseException("ILLEGAL-CALL-REFERENCE",
            "cannot create a call reference to %s %s::%s() from outside the class",
            privpub(access), class_path, method_name);
        return toBits(QoreValue());
    }

    RuntimeConfig& rc = rc_get_current_ref();
    QoreProgram* call_pgm = rc.getProgram() ? rc.getProgram() : pgm;
    const qore_class_private* cls = rc.getClass() ? rc.getClass() : runtime_get_class();
    if (method->isStatic()) {
        return toBits(QoreValue(new StaticMethodCallReferenceNode(&loc_builtin, method, call_pgm, cls)));
    }

    QoreObject* obj = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    if (!obj) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot create instance method call reference '%s::%s()': no current object",
            class_path, method_name);
        return toBits(QoreValue());
    }
    return toBits(QoreValue(new RunTimeResolvedMethodReferenceNode(&loc_builtin, obj, method, cls)));
}

extern "C" DLLEXPORT uint64_t qore_rt_create_local_method_call_ref_aot(const char* class_path,
        const char* method_name, ExceptionSink* xsink) {
    if (!class_path || !*class_path || !method_name || !*method_name) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot resolve method call reference '%s::%s()'",
            class_path ? class_path : "<null>", method_name ? method_name : "<null>");
        return toBits(QoreValue());
    }

    QoreProgram* pgm = getProgram();
    if (!pgm) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve method call reference '%s::%s()': no program context",
            class_path, method_name);
        return toBits(QoreValue());
    }

    const char* resolved_class_path = (class_path[0] == ':' && class_path[1] == ':')
        ? class_path + 2 : class_path;
    qore_program_private* pp = qore_program_private::get(*pgm);
    const qore_ns_private* found_ns = nullptr;
    const QoreClass* qc = qore_root_ns_private::runtimeFindClass(
        *pp->RootNS, resolved_class_path, found_ns);
    if (!qc) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve class '%s' for method call reference '%s()'",
            class_path, method_name);
        return toBits(QoreValue());
    }

    const QoreMethod* method = qc->findMethod(method_name);
    if (!method) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve method call reference '%s::%s()'",
            class_path, method_name);
        return toBits(QoreValue());
    }

    RuntimeConfig& rc = rc_get_current_ref();
    QoreObject* obj = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    if (!obj) {
        xsink->raiseException("OBJECT-ERROR",
            "cannot evaluate method call reference '\\%s()' when not in an object context; if using "
            "'background', evaluate the method reference before the 'background' statement",
            method_name);
        return toBits(QoreValue());
    }

    const qore_class_private* cls = rc.getClass() ? rc.getClass() : runtime_get_class();
    return toBits(QoreValue(new RunTimeResolvedMethodReferenceNode(&loc_builtin, obj, method, cls)));
}

extern "C" DLLEXPORT uint64_t qore_rt_create_function_call_ref_aot(const char* function_name,
        ExceptionSink* xsink) {
    if (!function_name || !*function_name) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot resolve function call reference '%s()'",
            function_name ? function_name : "<null>");
        return toBits(QoreValue());
    }

    QoreProgram* pgm = getProgram();
    if (!pgm) {
        xsink->raiseException("CALL-REFERENCE-ERROR",
            "cannot resolve function call reference '%s()': no program context", function_name);
        return toBits(QoreValue());
    }

    qore_program_private* pp = qore_program_private::get(*pgm);
    const FunctionEntry* fe = qore_root_ns_private::runtimeFindFunctionEntry(*pp->RootNS, function_name);
    QoreFunction* func = fe ? fe->getFunction(true) : nullptr;
    if (!func) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot resolve function call reference '%s()'",
            function_name);
        return toBits(QoreValue());
    }

    RuntimeConfig& rc = rc_get_current_ref();
    QoreProgram* call_pgm = rc.getProgram() ? rc.getProgram() : pgm;
    return toBits(QoreValue(new FunctionCallReferenceNode(&loc_builtin, func, call_pgm)));
}

// --- Method reference creation helper (delegates to call ref - identical behavior) ---

extern "C" DLLEXPORT uint64_t qore_rt_create_method_ref(uint64_t expr_bits, ExceptionSink* xsink) {
    return qore_rt_create_call_ref(expr_bits, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_create_self_method_ref_aot(const char* method_name, ExceptionSink* xsink) {
    RuntimeConfig& rc = rc_get_current_ref();
    QoreObject* obj = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    if (!obj) {
        xsink->raiseException("OBJECT-ERROR", "cannot evaluate method reference '\\%s()' when not in an object "
            "context; if using 'background', evaluate the method reference before the 'background' statement",
            method_name);
        return toBits(QoreValue());
    }
    return toBits(QoreValue(new RunTimeObjectMethodReferenceNode(&loc_builtin, obj, method_name)));
}

extern "C" DLLEXPORT uint64_t qore_rt_create_object_method_ref_aot(uint64_t object_bits,
        const char* method_name, ExceptionSink* xsink) {
    QoreValue obj_val = fromBits(object_bits);
    QoreObject* obj = obj_val.getType() == NT_OBJECT ? obj_val.get<QoreObject>() : nullptr;
    if (!obj) {
        xsink->raiseException("OBJECT-METHOD-REFERENCE-ERROR",
            "expression does not evaluate to an object; got type '%s' instead", obj_val.getTypeName());
        return toBits(QoreValue());
    }

    const QoreClass* obj_class = obj->getClass();
    ClassAccess access = Public;
    const QoreMethod* method = obj_class->findMethod(method_name, access);
    if (!method) {
        method = obj_class->findStaticMethod(method_name, access);
        if (!method) {
            xsink->raiseException("OBJECT-METHOD-REFERENCE-ERROR",
                "cannot resolve reference to %s::%s(): unknown method", obj->getClassName(), method_name);
            return toBits(QoreValue());
        }
    }

    if (access > Public && !qore_class_private::runtimeCheckPrivateClassAccess(*obj_class)) {
        if (method->isPrivate()) {
            xsink->raiseException("ILLEGAL-CALL-REFERENCE",
                "cannot create a call reference to %s %s::%s() from outside the class",
                privpub(access), obj->getClassName(), method_name);
        } else {
            xsink->raiseException("ILLEGAL-CALL-REFERENCE",
                "cannot create a call reference to %s::%s() because the parent class that implements the method "
                "(%s::%s()) is privately inherited", obj->getClassName(), method_name,
                method->getClass()->getName(), method_name);
        }
        return toBits(QoreValue());
    }

    if (obj->getClass() == method->getClass()) {
        return toBits(QoreValue(new RunTimeResolvedMethodReferenceNode(&loc_builtin, obj, method)));
    }
    return toBits(QoreValue(new RunTimeObjectMethodReferenceNode(&loc_builtin, obj, method_name)));
}

// --- Parse reference creation helper ---

extern "C" DLLEXPORT uint64_t qore_rt_create_parse_ref(const ParseReferenceNode* node, ExceptionSink* xsink) {
    RuntimeConfig& rc = rc_get_current_ref();
    return toBits(QoreValue(const_cast<ParseReferenceNode*>(node)->evalToRef(rc, xsink)));
}

extern "C" DLLEXPORT uint64_t qore_rt_create_parse_ref_resolved_hash_key(const ParseReferenceNode* node,
        uint64_t key_bits, ExceptionSink* xsink) {
    QoreValue key = fromBits(key_bits);
    RuntimeConfig& rc = rc_get_current_ref();
    return toBits(QoreValue(node
        ? const_cast<ParseReferenceNode*>(node)->evalToRefWithResolvedHashKey(rc, key, xsink)
        : nullptr));
}

extern "C" DLLEXPORT uint64_t qore_rt_create_parse_ref_aot(QoreAOTContext* ctx, int32_t idx,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    auto* node = dynamic_cast<const ParseReferenceNode*>(expr.getInternalNode());
    if (!node) {
        xsink->raiseException("AOT-ERROR", "invalid expression for parse reference AOT call");
        return toBits(QoreValue());
    }
    return qore_rt_create_parse_ref(node, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_create_parse_ref_aot_resolved_hash_key(QoreAOTContext* ctx, int32_t idx,
        uint64_t key_bits, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    auto* node = dynamic_cast<const ParseReferenceNode*>(expr.getInternalNode());
    if (!node) {
        xsink->raiseException("AOT-ERROR", "invalid expression for parse reference AOT call");
        return toBits(QoreValue());
    }
    return qore_rt_create_parse_ref_resolved_hash_key(node, key_bits, xsink);
}

static const QoreTypeInfo* get_local_parse_ref_type(LocalVar* lv) {
    const QoreTypeInfo* ti = lv ? lv->parseGetTypeInfoForInitialAssignment() : nullptr;
    if (!ti) {
        return referenceTypeInfo;
    }
    const QoreTypeInfo* ref_target = QoreTypeInfo::getReferenceTarget(ti);
    return qore_get_complex_hard_reference_type(ref_target ? ref_target : ti);
}

static const QoreTypeInfo* resolve_parse_ref_target_type(const char* ref_type_path, ExceptionSink* xsink) {
    if (!ref_type_path || !*ref_type_path) {
        return nullptr;
    }
    QoreProgram* pgm = getProgram();
    if (!pgm) {
        xsink->raiseException("AOT-REF-ERROR",
            "cannot resolve reference type '%s' without a current Program", ref_type_path);
        return nullptr;
    }
    std::string error;
    QoreAOTTypeResolver resolver(pgm);
    const QoreTypeInfo* ref_ti = resolver.resolve(ref_type_path, error);
    if (!ref_ti) {
        xsink->raiseException("AOT-REF-ERROR",
            "cannot resolve reference type '%s': %s", ref_type_path, error.c_str());
        return nullptr;
    }
    if (!QoreTypeInfo::isReference(ref_ti)) {
        xsink->raiseException("AOT-REF-ERROR",
            "resolved parse-reference type '%s' is not a reference type", ref_type_path);
        return nullptr;
    }
    return QoreTypeInfo::getUniqueReturnComplexReference(ref_ti);
}

//! AOT: create a reference to a local variable from slot index
/** Eliminates EXPR_TREE for \var expressions by resolving the LocalVar*
    from the AOT context's local slot, constructing a temporary VarRefNode
    and ParseReferenceNode, and evaluating to produce a ReferenceNode.
*/
extern "C" DLLEXPORT uint64_t qore_rt_create_local_ref_aot(QoreAOTContext* ctx, int32_t local_slot,
        ExceptionSink* xsink) {
    if (local_slot < 0 || local_slot >= ctx->num_locals || !ctx->locals[local_slot]) {
        xsink->raiseException("AOT-REF-ERROR",
            "cannot resolve local slot %d for reference creation", local_slot);
        return 0;
    }
    LocalVar* lv = ctx->locals[local_slot];
    const QoreTypeInfo* ref_ti = get_local_parse_ref_type(lv);
    // Build a temporary VarRefNode + ParseReferenceNode and evaluate.
    // Mirror the source parser's setThreadSafe() call (ReferenceNode.cpp:281):
    // switch the VarRefNode to VT_LOCAL_TS and mark the LocalVar closure_use so
    // doPartialEval resolves through the closure-var stack instead of taking the
    // VT_LOCAL fallthrough path.  Without this, evalToRef returns a ReferenceNode
    // whose vexp is the VT_LOCAL VarRefNode itself — and at lvalue-resolution
    // time inside the callee, VarRefNode::getLValue walks the closure-var stack
    // and finds the callee's own CVV (which holds this very reference), producing
    // a self-referential cycle detected by thread_ref_set() as
    // CIRCULAR-REFERENCE-ERROR.
    VarRefNode* vrn = new VarRefNode(&loc_builtin, lv->getName(), lv, false);
    vrn->setThreadSafe();
    SimpleRefHolder<ParseReferenceNode> prn(new ParseReferenceNode(&loc_builtin, QoreValue(vrn), ref_ti));
    ReferenceNode* ref = prn->evalToRef(xsink);
    return ref ? toBits(QoreValue(ref)) : 0;
}

//! AOT: create a reference to a member hash element (\member{key})
/** Eliminates EXPR_TREE for \self.member{key} expressions by building the
    partial lvalue expression at runtime from pre-evaluated key value and
    member name string embedded as a constant.
*/
extern "C" DLLEXPORT uint64_t qore_rt_create_member_hash_ref_aot(
        const char* member_name, uint64_t key_bits, const char* ref_type_path, ExceptionSink* xsink) {
    assert(member_name);

    // Get current self/class context
    QoreObject* self = nullptr;
    const qore_class_private* qc = nullptr;
    runtime_get_object_and_class(self, qc);
    if (!self) {
        xsink->raiseException("AOT-REF-ERROR",
            "no current object for member hash reference '\\%s{...}'", member_name);
        return 0;
    }

    const QoreTypeInfo* ref_target_ti = resolve_parse_ref_target_type(ref_type_path, xsink);
    if (*xsink) {
        return 0;
    }

    // Build the partial lvalue expression: SelfVarrefNode{key}
    // SelfVarrefNode references the member variable on the current object
    SelfVarrefNode* svn = new SelfVarrefNode(&loc_builtin, strdup(member_name));
    QoreValue key = fromBits(key_bits);
    QoreValue key_copy = key.hasNode() ? key.refSelf() : key;
    QoreValue lv(new QoreHashObjectDereferenceOperatorNode(&loc_builtin, QoreValue(svn), key_copy));

    // Create ReferenceNode with the partial lvalue
    ReferenceNode* ref = new ReferenceNode(lv, ref_target_ti, self, self, qc);
    return toBits(QoreValue(ref));
}

// --- Typed container construction helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_new_hash_decl(const NewHashDeclNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<NewHashDeclNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_hash_decl_aot(QoreAOTContext* ctx, int32_t idx,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    auto* node = dynamic_cast<const NewHashDeclNode*>(expr.getInternalNode());
    if (!node) {
        xsink->raiseException("AOT-ERROR", "invalid expression for hashdecl construction AOT call");
        return toBits(QoreValue());
    }
    return qore_rt_new_hash_decl(node, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_hash(const NewComplexHashNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<NewComplexHashNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_hash_aot(QoreAOTContext* ctx, int32_t idx,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    auto* node = dynamic_cast<const NewComplexHashNode*>(expr.getInternalNode());
    if (!node) {
        xsink->raiseException("AOT-ERROR", "invalid expression for complex hash construction AOT call");
        return toBits(QoreValue());
    }
    return qore_rt_new_complex_hash(node, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_list(const NewComplexListNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<NewComplexListNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_list_aot(QoreAOTContext* ctx, int32_t idx,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    auto* node = dynamic_cast<const NewComplexListNode*>(expr.getInternalNode());
    if (!node) {
        xsink->raiseException("AOT-ERROR", "invalid expression for complex list construction AOT call");
        return toBits(QoreValue());
    }
    return qore_rt_new_complex_list(node, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_buffer(const NewComplexBufferNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<NewComplexBufferNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_buffer_aot(QoreAOTContext* ctx, int32_t idx,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    auto* node = dynamic_cast<const NewComplexBufferNode*>(expr.getInternalNode());
    if (!node) {
        xsink->raiseException("AOT-ERROR", "invalid expression for complex buffer construction AOT call");
        return toBits(QoreValue());
    }
    return qore_rt_new_complex_buffer(node, xsink);
}

// --- VarRefNewObjectNode construction helper (non-object types) ---

extern "C" DLLEXPORT uint64_t qore_rt_vrn_construct(const VarRefNewObjectNode* vrn, ExceptionSink* xsink) {
    return toBits(vrn->constructValue(xsink));
}

// --- Hashdecl construction from pre-lowered hash ---

extern "C" DLLEXPORT uint64_t qore_rt_new_hash_decl_from_hash(const TypedHashDecl* hd,
        uint64_t hash_bits, int32_t runtime_check, ExceptionSink* xsink) {
    if (!hd) {
        xsink->raiseException("HASHDECL-ERROR", "cannot construct hashdecl from hash: missing hashdecl target");
        return toBits(QoreValue());
    }
    QoreValue hash_val = fromBits(hash_bits);
    const QoreHashNode* init = nullptr;
    if (hash_val.getType() != NT_NOTHING) {
        if (hash_val.getType() != NT_HASH) {
            xsink->raiseException("HASHDECL-INIT-ERROR",
                "hashdecl '%s' hash initializer value must be a hash; got type '%s' instead",
                hd ? hd->getName() : "<unknown>", hash_val.getTypeName());
            return toBits(QoreValue());
        }
        init = hash_val.get<const QoreHashNode>();
    }
    QoreHashNode* result = typed_hash_decl_private::get(*hd)->newHash(init,
        runtime_check != 0, xsink);
    return toBits(result ? QoreValue(result) : QoreValue());
}

// AOT variant: resolves hashdecl by namespace path at runtime
extern "C" DLLEXPORT uint64_t qore_rt_new_hash_decl_from_hash_by_path(const char* hd_path,
        uint64_t hash_bits, int32_t runtime_check, ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();
    if (!pgm) {
        if (xsink) {
            xsink->raiseException("HASHDECL-ERROR", "cannot resolve hashdecl '%s': no program context",
                hd_path ? hd_path : "<null>");
        }
        return toBits(QoreValue());
    }
    const TypedHashDecl* hd = qore_aot_resolve_hashdecl_path(pgm, hd_path);
    if (!hd) {
        std::string error;
        QoreAOTTypeResolver resolver(pgm);
        const QoreTypeInfo* ti = resolver.resolve(hd_path, error);
        ti = qore_substitute_type_params_if_needed(ti);
        hd = QoreTypeInfo::getUniqueReturnHashDecl(ti);
    }
    if (!hd) {
        if (xsink) {
            xsink->raiseException("HASHDECL-ERROR", "cannot resolve hashdecl '%s'",
                hd_path ? hd_path : "<null>");
        }
        return toBits(QoreValue());
    }
    return qore_rt_new_hash_decl_from_hash(hd, hash_bits, runtime_check, xsink);
}

static const TypedHashDecl* qore_rt_resolve_hashdecl_path_cached(QoreAOTContext* ctx, const char* hd_path,
        ExceptionSink* xsink) {
    if (!ctx || !hd_path || !*hd_path) {
        return nullptr;
    }

    const QoreTypeInfo* receiver_type_info = qore_get_current_receiver_type_info();
    QoreAOTHashDeclPathCacheKey key{hd_path, receiver_type_info};

    {
        std::lock_guard<std::mutex> lock(ctx->hashdecl_path_cache_mutex);
        auto i = ctx->hashdecl_path_cache.find(key);
        if (i != ctx->hashdecl_path_cache.end()) {
            return i->second;
        }
    }

    QoreProgram* pgm = ctx->pgm ? ctx->pgm : getProgram();
    if (!pgm) {
        if (xsink) {
            xsink->raiseException("HASHDECL-ERROR", "cannot resolve hashdecl '%s': no program context", hd_path);
        }
        return nullptr;
    }

    const TypedHashDecl* hd = qore_aot_resolve_hashdecl_path(pgm, hd_path);
    if (!hd) {
        std::string error;
        QoreAOTTypeResolver resolver(pgm);
        const QoreTypeInfo* ti = resolver.resolve(hd_path, error);
        if (ti) {
            ti = qore_substitute_type_params_if_needed(ti, receiver_type_info);
            hd = QoreTypeInfo::getUniqueReturnHashDecl(ti);
        }
    }
    if (!hd) {
        if (xsink) {
            xsink->raiseException("HASHDECL-ERROR", "cannot resolve hashdecl '%s'", hd_path);
        }
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(ctx->hashdecl_path_cache_mutex);
        ctx->hashdecl_path_cache.emplace(std::move(key), hd);
    }
    return hd;
}

extern "C" DLLEXPORT uint64_t qore_rt_new_hash_decl_from_hash_by_path_cached(QoreAOTContext* ctx,
        const char* hd_path, uint64_t hash_bits, int32_t runtime_check, ExceptionSink* xsink) {
    const TypedHashDecl* hd = qore_rt_resolve_hashdecl_path_cached(ctx, hd_path, xsink);
    if (!hd || (xsink && *xsink)) {
        return toBits(QoreValue());
    }
    return qore_rt_new_hash_decl_from_hash(hd, hash_bits, runtime_check, xsink);
}

// --- Hash building helper ---

extern "C" DLLEXPORT void qore_rt_hash_set_key_value(uint64_t hash_bits, uint64_t key_bits,
        uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue hash_val = fromBits(hash_bits);
    QoreValue key_val = fromBits(key_bits);
    QoreValue value_val = fromBits(value_bits);
    QoreStringValueHelper key_str(key_val);
    QoreHashNode* hash = hash_val.get<QoreHashNode>();
    if (value_val.hasNode()) {
        value_val.refSelf();
    }
    hash->setKeyValue(key_str->c_str(), value_val, xsink);
    // Do NOT discard key_val — the caller (JIT/IR) manages the key's lifetime.
    // Discarding here causes a double-free since the key is also cleaned up by
    // the JIT function's exit cleanup or the IR value map cleanup mechanism.
}

extern "C" DLLEXPORT void qore_rt_hash_reserve(uint64_t hash_bits, int64_t capacity) {
    if (capacity <= 0) {
        return;
    }
    QoreValue hash_val = fromBits(hash_bits);
    if (hash_val.getType() == NT_HASH) {
        qore_hash_private::get(*hash_val.get<QoreHashNode>())->hm.reserve(
            static_cast<size_t>(capacity));
    }
}

// --- Reverse iterator creation helper ---

extern "C" DLLEXPORT void* qore_rt_iterator_create_reverse(uint64_t iterable_bits, ExceptionSink* xsink) {
    QoreValue iterable = fromBits(iterable_bits);
    FunctionalOperator::FunctionalValueType value_type;
    FunctionalOperatorInterface* iter = FunctionalOperatorInterface::getFunctionalIterator(
        value_type, iterable, false, "foldr operator", xsink);
    if (*xsink || value_type == FunctionalOperator::nothing) {
        delete iter;
        return nullptr;
    }
    return iter;
}

// --- Implicit argument helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_load_implicit_arg(int offset, ExceptionSink* xsink) {
    const QoreListNode* argv = thread_get_implicit_args();
    if (!argv) {
        return toBits(QoreValue());
    }
    QoreValue result = argv->retrieveEntry(offset);
    if (result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_implicit_argv(ExceptionSink* xsink) {
    const QoreListNode* argv = thread_get_implicit_args();
    if (!argv) {
        return toBits(QoreValue());
    }
    // Return a reference to the argv list
    QoreValue result = const_cast<QoreListNode*>(argv);
    result.refSelf();
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_implicit_element(ExceptionSink* xsink) {
    return toBits(QoreValue(get_implicit_element()));
}

extern "C" DLLEXPORT uint64_t qore_rt_push_implicit_arg(uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue value = fromBits(value_bits);
    // Get current implicit args (if any), save for restoration
    const QoreListNode* old_argv = thread_get_implicit_args();
    QoreValue old_context = old_argv ? const_cast<QoreListNode*>(old_argv)->refSelf() : QoreValue();

    // Create new single-element list with the value
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> new_argv(new QoreListNode(autoTypeInfo), xsink);
    qore_list_private::get(**new_argv)->pushIntern(value.refSelf());

    thread_set_implicit_args(new_argv.release());

    return toBits(old_context);
}

extern "C" DLLEXPORT uint64_t qore_rt_set_implicit_argv(uint64_t argv_bits, ExceptionSink* xsink) {
    QoreValue argv_val = fromBits(argv_bits);
    // Get current implicit args (if any), save for restoration
    const QoreListNode* old_argv = thread_get_implicit_args();
    QoreValue old_context = old_argv ? const_cast<QoreListNode*>(old_argv)->refSelf() : QoreValue();

    // Set new implicit args (argv_val should be a list or nothing)
    QoreListNode* new_argv = argv_val.get<QoreListNode>();
    if (new_argv) {
        new_argv->ref();
    }
    thread_set_implicit_args(new_argv);

    return toBits(old_context);
}

extern "C" DLLEXPORT void qore_rt_pop_implicit_arg(uint64_t old_context_bits, ExceptionSink* xsink) {
    QoreValue old_context = fromBits(old_context_bits);

    // Get current implicit args and deref
    const QoreListNode* current = thread_get_implicit_args();
    if (current) {
        const_cast<QoreListNode*>(current)->deref(xsink);
    }

    // Restore old context — the old_context was refSelf'd by
    // qore_rt_set_implicit_argv / qore_rt_push_implicit_arg to keep it alive
    // while it was saved. Now that it's restored to current_implicit_arg,
    // release the extra ref. The pointer remains valid because the caller
    // (ArgvContextHelper or argvid on the lvstack) still holds its own ref.
    QoreListNode* old_argv = old_context.get<QoreListNode>();
    thread_set_implicit_args(old_argv);
    if (old_argv) {
        old_argv->deref(xsink);
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_push_implicit_element(int64_t index, ExceptionSink* xsink) {
    // save_implicit_element sets the new value and returns the old value
    int old_element = save_implicit_element(static_cast<int>(index));
    return static_cast<uint64_t>(old_element);
}

extern "C" DLLEXPORT void qore_rt_pop_implicit_element(uint64_t old_element) {
    save_implicit_element(static_cast<int>(old_element));
}

extern "C" DLLEXPORT uint64_t qore_rt_create_empty_list(ExceptionSink* xsink) {
    QoreListNode* list = new QoreListNode(autoTypeInfo);
    return toBits(QoreValue(list));
}

static const QoreTypeInfo* qore_rt_resolve_element_type_path(const char* type_path, const char* op,
        ExceptionSink* xsink) {
    if (!type_path || !*type_path) {
        return autoTypeInfo;
    }

    QoreProgram* pgm = getProgram();
    if (!pgm) {
        if (xsink) {
            xsink->raiseException("AOT-TYPE-ERROR",
                "%s cannot resolve list element type '%s' without a current Program", op, type_path);
        }
        return nullptr;
    }

    std::string error;
    QoreAOTTypeResolver resolver(pgm);
    const QoreTypeInfo* ti = resolver.resolve(type_path, error);
    if (!ti && xsink) {
        xsink->raiseException("AOT-TYPE-ERROR",
            "%s cannot resolve list element type '%s': %s", op, type_path, error.c_str());
    }
    return qore_substitute_type_params_if_needed(ti);
}

static const QoreTypeInfo* qore_rt_resolve_full_type_path(const char* type_path, const char* op,
        ExceptionSink* xsink) {
    if (!type_path || !*type_path) {
        return nullptr;
    }

    QoreProgram* pgm = getProgram();
    if (!pgm) {
        if (xsink) {
            xsink->raiseException("AOT-TYPE-ERROR",
                "%s cannot resolve container type '%s' without a current Program", op, type_path);
        }
        return nullptr;
    }

    std::string error;
    QoreAOTTypeResolver resolver(pgm);
    const QoreTypeInfo* ti = resolver.resolve(type_path, error);
    if (!ti && xsink) {
        xsink->raiseException("AOT-TYPE-ERROR",
            "%s cannot resolve container type '%s': %s", op, type_path, error.c_str());
    }
    return qore_substitute_type_params_if_needed(ti);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_hash_from_hash(const QoreTypeInfo* typeInfo,
        uint64_t hash_bits, ExceptionSink* xsink) {
    typeInfo = qore_substitute_type_params_if_needed(typeInfo);
    if (!typeInfo) {
        if (xsink) {
            xsink->raiseException("HASH-INIT-ERROR",
                "typed hash initializer is missing target type metadata");
        }
        return toBits(QoreValue());
    }

    QoreValue hash_val = fromBits(hash_bits);
    if (hash_val.getType() != NT_HASH) {
        if (xsink) {
            xsink->raiseException("HASH-INIT-ERROR",
                "typed hash initializer value must be a hash; got type '%s' instead",
                hash_val.getTypeName());
        }
        return toBits(QoreValue());
    }

    QoreHashNode* init_hash = hash_val.get<QoreHashNode>()->hashRefSelf();
    QoreHashNode* result = qore_hash_private::newComplexHashFromHash(typeInfo, init_hash, xsink);
    return toBits(result ? QoreValue(result) : QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_hash_from_hash_by_type_path(const char* type_path,
        uint64_t hash_bits, ExceptionSink* xsink) {
    const QoreTypeInfo* typeInfo = qore_rt_resolve_full_type_path(type_path, "NewComplexHash", xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return qore_rt_new_complex_hash_from_hash(typeInfo, hash_bits, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_list_from_value(const QoreTypeInfo* typeInfo,
        uint64_t value_bits, ExceptionSink* xsink) {
    typeInfo = qore_substitute_type_params_if_needed(typeInfo);
    if (!typeInfo) {
        if (xsink) {
            xsink->raiseException("LIST-INIT-ERROR",
                "typed list initializer is missing target type metadata");
        }
        return toBits(QoreValue());
    }

    QoreValue value = fromBits(value_bits);
    QoreValue init = value.hasNode() ? value.refSelf() : value;
    QoreListNode* result = qore_list_private::newComplexListFromValue(typeInfo, init, xsink);
    return toBits(result ? QoreValue(result) : QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_list_from_value_by_type_path(const char* type_path,
        uint64_t value_bits, ExceptionSink* xsink) {
    const QoreTypeInfo* typeInfo = qore_rt_resolve_full_type_path(type_path, "NewComplexList", xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return qore_rt_new_complex_list_from_value(typeInfo, value_bits, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_buffer_from_value(const QoreTypeInfo* typeInfo,
        uint64_t value_bits, ExceptionSink* xsink) {
    return qore_rt_new_complex_buffer_from_value_kind(typeInfo, value_bits,
        static_cast<int32_t>(QoreComplexBufferInitKind::Constructor), xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_buffer_from_value_kind(const QoreTypeInfo* typeInfo,
        uint64_t value_bits, int32_t init_kind, ExceptionSink* xsink) {
    typeInfo = qore_substitute_type_params_if_needed(typeInfo);
    if (!typeInfo) {
        if (xsink) {
            xsink->raiseException("BUFFER-INIT-ERROR",
                "typed buffer initializer is missing target type metadata");
        }
        return toBits(QoreValue());
    }

    QoreValue value = fromBits(value_bits);
    QoreValue init = value.hasNode() ? value.refSelf() : value;
    QoreBufferNode* result = qore_new_complex_buffer_from_value(typeInfo, init, xsink,
        static_cast<QoreComplexBufferInitKind>(init_kind));
    return toBits(result ? QoreValue(result) : QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_buffer_from_value_by_type_path(const char* type_path,
        uint64_t value_bits, ExceptionSink* xsink) {
    const QoreTypeInfo* typeInfo = qore_rt_resolve_full_type_path(type_path, "NewComplexBuffer", xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return qore_rt_new_complex_buffer_from_value(typeInfo, value_bits, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_buffer_from_value_kind_by_type_path(const char* type_path,
        uint64_t value_bits, int32_t init_kind, ExceptionSink* xsink) {
    const QoreTypeInfo* typeInfo = qore_rt_resolve_full_type_path(type_path, "NewComplexBuffer", xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return qore_rt_new_complex_buffer_from_value_kind(typeInfo, value_bits, init_kind, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_create_empty_list_typed(const QoreTypeInfo* element_type,
        ExceptionSink* xsink) {
    element_type = qore_substitute_type_params_if_needed(element_type);
    QoreListNode* list = new QoreListNode(element_type ? element_type : autoTypeInfo);
    return toBits(QoreValue(list));
}

extern "C" DLLEXPORT uint64_t qore_rt_create_empty_list_by_type_path(const char* element_type_path,
        ExceptionSink* xsink) {
    const QoreTypeInfo* element_type = qore_rt_resolve_element_type_path(element_type_path,
        "CreateEmptyList", xsink);
    if (!element_type) {
        return toBits(QoreValue());
    }
    return qore_rt_create_empty_list_typed(element_type, xsink);
}

extern "C" DLLEXPORT void qore_rt_list_append(uint64_t list_bits, uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue list_val = fromBits(list_bits);
    QoreValue value = fromBits(value_bits);

    QoreListNode* list = list_val.get<QoreListNode>();
    if (list) {
        QoreValue ref_val = value.refSelf();
        qore_list_private* priv = qore_list_private::get(*list);
        // Track element type to maintain correct list<T> type info at runtime,
        // matching AST mode's vtype/vcommon tracking in map/select operators.
        priv->setListTypeFromNewElementType(ref_val.getFullTypeInfo());
        priv->pushIntern(ref_val);
    }
}

static uint64_t qore_rt_list_push_impl(QoreValue list_val, QoreValue push_val,
        const QoreTypeInfo* element_type, ExceptionSink* xsink) {
    if (list_val.getType() == NT_LIST) {
        QoreListNode* l = list_val.get<QoreListNode>();
        l->push(push_val.refSelf(), xsink);
        // Return same list with a new reference for the caller to own
        l->ref();
        return toBits(list_val);
    }

    if (list_val.isNothing()) {
        // Auto-vivify empty list (already has refcount 1 from new)
        element_type = qore_substitute_type_params_if_needed(element_type);
        QoreListNode* l = new QoreListNode(element_type ? element_type : autoTypeInfo);
        l->push(push_val.refSelf(), xsink);
        QoreValue result(l);
        return toBits(result);
    }

    // Not a list - raise error
    xsink->raiseException("PUSH-ERROR",
        "the lvalue argument to push is type \"%s\"; expecting \"list\"",
        list_val.getTypeName());
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_list_push_typed(uint64_t list_bits, uint64_t val_bits,
        const QoreTypeInfo* element_type, ExceptionSink* xsink) {
    return qore_rt_list_push_impl(fromBits(list_bits), fromBits(val_bits), element_type, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_list_push(uint64_t list_bits, uint64_t val_bits, ExceptionSink* xsink) {
    return qore_rt_list_push_typed(list_bits, val_bits, autoTypeInfo, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_list_push_in_place(uint64_t list_bits,
        uint64_t val_bits, ExceptionSink* xsink) {
    QoreValue list_val = fromBits(list_bits);
    if (list_val.getType() != NT_LIST) {
        xsink->raiseException("PUSH-ERROR",
            "the lvalue argument to push is type \"%s\"; expecting \"list\"",
            list_val.getTypeName());
        return toBits(QoreValue());
    }
    list_val.get<QoreListNode>()->push(fromBits(val_bits).refSelf(), xsink);
    return xsink && *xsink ? toBits(QoreValue()) : list_bits;
}

extern "C" DLLEXPORT uint64_t qore_rt_list_push_by_type_path(uint64_t list_bits, uint64_t val_bits,
        const char* element_type_path, ExceptionSink* xsink) {
    QoreValue list_val = fromBits(list_bits);
    QoreValue push_val = fromBits(val_bits);
    if (!list_val.isNothing()) {
        return qore_rt_list_push_impl(list_val, push_val, autoTypeInfo, xsink);
    }

    const QoreTypeInfo* element_type = qore_rt_resolve_element_type_path(element_type_path,
        "ListPush", xsink);
    if (!element_type) {
        return toBits(QoreValue());
    }

    return qore_rt_list_push_impl(list_val, push_val, element_type, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_switch_regex_match(uint64_t regex_case_ptr, uint64_t switch_val_bits, ExceptionSink* xsink) {
    const CaseNodeRegex* regex_case = reinterpret_cast<const CaseNodeRegex*>(regex_case_ptr);
    QoreValue switch_val = fromBits(switch_val_bits);

    if (!regex_case) {
        return toBits(QoreValue(false));
    }

    bool match = regex_case->matches(switch_val, xsink);
    return toBits(QoreValue(match));
}

// --- LValue operation helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_load(uint64_t lvalue_bits, ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue result = QoreIRInterpreter::evalLValueLoad(lvalue, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_store(uint64_t lvalue_bits, uint64_t value_bits, ExceptionSink* xsink) {
    if (*xsink) {
        // Defensive guard: if an exception was thrown by a prior instruction and not caught,
        // discard the value and return NOTHING to avoid assertion in LValueHelper
        QoreValue value = fromBits(value_bits);
        value.discard(xsink);
        return toBits(QoreValue());
    }
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue value = fromBits(value_bits);
    // Hold an extra reference on the RHS value so that LValueHelper::ensureUnique()
    // sees the correct refcount for COW. Without this, self-assignment (e.g., h.b = h)
    // creates a circular reference because the hash appears unique at refcount 1.
    ValueHolder val_holder(value.refSelf(), xsink);
    QoreValue result = QoreIRInterpreter::evalLValueStore(lvalue, value, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_store_weak(uint64_t lvalue_bits, uint64_t value_bits,
        ExceptionSink* xsink) {
    if (*xsink) {
        QoreValue value = fromBits(value_bits);
        value.discard(xsink);
        return toBits(QoreValue());
    }
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue value = fromBits(value_bits);
    ValueHolder val_holder(value.refSelf(), xsink);
    QoreValue result = QoreIRInterpreter::evalLValueStore(lvalue, value, xsink, true);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_unary(int opcode, uint64_t lvalue_bits, ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue result = QoreIRInterpreter::evalLValueUnary(static_cast<QoreIROpcode>(opcode), lvalue, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_binary(int opcode, uint64_t lvalue_bits, uint64_t value_bits,
        ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue value = fromBits(value_bits);
    QoreValue result = QoreIRInterpreter::evalLValueBinary(static_cast<QoreIROpcode>(opcode), lvalue, value, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_ternary(int opcode, uint64_t lvalue_bits, uint64_t first_bits,
        uint64_t second_bits, uint64_t third_bits, ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue first = fromBits(first_bits);
    QoreValue second = fromBits(second_bits);
    QoreValue third = fromBits(third_bits);
    QoreValue result = QoreIRInterpreter::evalLValueTernary(static_cast<QoreIROpcode>(opcode), lvalue, first,
        second, third, xsink);
    return toBits(result);
}

// --- Container construction helpers ---

static const QoreTypeInfo* qore_rt_get_declared_list_value_type(const QoreTypeInfo* typeInfo) {
    typeInfo = qore_substitute_type_params_if_needed(typeInfo);
    const QoreTypeInfo* vtype = QoreTypeInfo::getUniqueReturnComplexList(typeInfo);
    if (!vtype) {
        vtype = QoreTypeInfo::getReturnComplexListOrNothing(typeInfo);
    }
    return vtype && vtype != anyTypeInfo ? vtype : nullptr;
}

static const QoreTypeInfo* qore_rt_get_declared_hash_value_type(const QoreTypeInfo* typeInfo) {
    typeInfo = qore_substitute_type_params_if_needed(typeInfo);
    const QoreTypeInfo* vtype = QoreTypeInfo::getUniqueReturnComplexHash(typeInfo);
    if (!vtype) {
        vtype = QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
    }
    return vtype && vtype != anyTypeInfo ? vtype : nullptr;
}

static bool qore_rt_has_declared_container_value_type(const QoreTypeInfo* vtype) {
    return vtype && vtype != autoTypeInfo && vtype != anyTypeInfo;
}

extern "C" DLLEXPORT uint64_t qore_rt_make_list(uint64_t* vals, int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    const QoreTypeInfo* declared_vtype = qore_rt_get_declared_list_value_type(typeInfo);
    bool declared_type = qore_rt_has_declared_container_value_type(declared_vtype);
    ReferenceHolder<QoreListNode> list(
        new QoreListNode(declared_type ? declared_vtype : autoTypeInfo), xsink);
    qore_list_private* priv = qore_list_private::get(**list);
    priv->reserve(count);
    // Track common value type for proper list typing (e.g., list<string> vs list<auto>)
    const QoreTypeInfo* vtype = nullptr;
    bool vcommon = false;
    for (int i = 0; i < count; i++) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "list literal construction")) {
            return toBits(QoreValue());
        }
        QoreValue v = fromBits(vals[i]);
        if (v.hasNode()) {
            v.refSelf();
        }
        const QoreTypeInfo* vt = v.getTypeInfo();
        if (!vtype) {
            vtype = vt;
            vcommon = true;
        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
            vcommon = false;
        }
        if (list->push(v, xsink)) {
            return toBits(QoreValue());
        }
    }
    if (declared_type) {
        priv->complexTypeInfo = qore_get_complex_list_type(declared_vtype);
    } else {
        if (!vtype || vtype == anyTypeInfo || !vcommon) {
            vtype = autoTypeInfo;
        }
        priv->complexTypeInfo = qore_get_complex_list_type(vtype);
    }
    return toBits(QoreValue(list.release()));
}

extern "C" DLLEXPORT uint64_t qore_rt_make_list_by_type_path(uint64_t* vals, int count,
        const char* type_path, ExceptionSink* xsink) {
    const QoreTypeInfo* typeInfo = qore_rt_resolve_full_type_path(type_path, "MakeList", xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return qore_rt_make_list(vals, count, typeInfo, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_make_hash(uint64_t* kv_pairs, int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    const QoreTypeInfo* declared_vtype = qore_rt_get_declared_hash_value_type(typeInfo);
    bool declared_type = qore_rt_has_declared_container_value_type(declared_vtype);
    ReferenceHolder<QoreHashNode> hash(
        new QoreHashNode(declared_type ? declared_vtype : autoTypeInfo), xsink);
    // count is the number of key-value pairs; kv_pairs has 2*count elements
    // Track common value type for proper hash typing (e.g., hash<string, string> vs hash<string, auto>)
    const QoreTypeInfo* vtype = nullptr;
    bool vcommon = false;
    for (int i = 0; i < count; i++) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "hash literal construction")) {
            return toBits(QoreValue());
        }
        QoreValue key = fromBits(kv_pairs[i * 2]);
        QoreValue val = fromBits(kv_pairs[i * 2 + 1]);
        QoreStringValueHelper key_str(key);
        if (val.hasNode()) {
            val.refSelf();
        }
        const QoreTypeInfo* vt = val.getFullTypeInfo();
        if (!i) {
            vtype = vt;
            vcommon = true;
        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
            vcommon = false;
        }
        hash->setKeyValue(key_str->c_str(), val, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
    }
    if (declared_type) {
        qore_hash_private::get(*hash)->complexTypeInfo = qore_get_complex_hash_type(declared_vtype);
    } else {
        if (!vtype || vtype == anyTypeInfo || !vcommon) {
            vtype = autoTypeInfo;
        }
        qore_hash_private::get(*hash)->complexTypeInfo = qore_get_complex_hash_type(vtype);
    }
    return toBits(QoreValue(hash.release()));
}

extern "C" DLLEXPORT uint64_t qore_rt_make_hash_by_type_path(uint64_t* kv_pairs, int count,
        const char* type_path, ExceptionSink* xsink) {
    const QoreTypeInfo* typeInfo = qore_rt_resolve_full_type_path(type_path, "MakeHash", xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return qore_rt_make_hash(kv_pairs, count, typeInfo, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_to_string(uint64_t val_bits) {
    QoreValue val = fromBits(val_bits);
    QoreStringNode* str;
    switch (val.getType()) {
        case NT_STRING: {
            QoreStringNodeValueHelper val_str(val);
            str = val_str.getReferencedValue();
            break;
        }
        case NT_INT:
            str = new QoreStringNodeMaker(QLLD, val.getAsBigInt());
            break;
        case NT_FLOAT:
            str = q_fix_decimal(new QoreStringNodeMaker("%.9g", val.getAsFloat()), 0);
            break;
        case NT_BOOLEAN:
            str = new QoreStringNodeMaker(QLLD, val.getAsBigInt());
            break;
        case NT_NOTHING:
        case NT_NULL:
            str = new QoreStringNode();
            break;
        default: {
            QoreStringValueHelper sv(val);
            str = new QoreStringNode(*sv);
            break;
        }
    }
    return toBits(QoreValue(str));
}

extern "C" DLLEXPORT uint64_t qore_rt_sprintf(uint64_t val_bits, ExceptionSink* xsink) {
    QoreValue val = fromBits(val_bits);
    QoreStringNode* str;
    if (val.getType() == NT_LIST) {
        str = q_sprintf(val.get<const QoreListNode>(), 0, 0, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
    } else {
        // Single value: convert to string
        QoreStringValueHelper sv(val);
        str = new QoreStringNode(*sv);
    }
    return toBits(QoreValue(str));
}

extern "C" DLLEXPORT uint64_t qore_rt_make_hash_const_keys(const char** keys, uint64_t* vals,
        int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    const QoreTypeInfo* declared_vtype = qore_rt_get_declared_hash_value_type(typeInfo);
    bool declared_type = qore_rt_has_declared_container_value_type(declared_vtype);
    ReferenceHolder<QoreHashNode> hash(
        new QoreHashNode(declared_type ? declared_vtype : autoTypeInfo), xsink);
    qore_hash_private* hp = qore_hash_private::get(*hash);
    hp->hm.reserve(count);
    const QoreTypeInfo* vtype = nullptr;
    bool vcommon = false;
    for (int i = 0; i < count; i++) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "hash literal construction")) {
            return toBits(QoreValue());
        }
        QoreValue val = fromBits(vals[i]);
        if (val.hasNode()) {
            val.refSelf();
        }
        const QoreTypeInfo* vt = val.getFullTypeInfo();
        if (!i) {
            vtype = vt;
            vcommon = true;
        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
            vcommon = false;
        }
        hash->setKeyValue(keys[i], val, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
    }
    if (declared_type) {
        hp->complexTypeInfo = qore_get_complex_hash_type(declared_vtype);
    } else {
        if (!vtype || vtype == anyTypeInfo || !vcommon) {
            vtype = autoTypeInfo;
        }
        hp->complexTypeInfo = qore_get_complex_hash_type(vtype);
    }
    return toBits(QoreValue(hash.release()));
}

extern "C" DLLEXPORT uint64_t qore_rt_make_hash_const_keys_by_type_path(const char** keys, uint64_t* vals,
        int count, const char* type_path, ExceptionSink* xsink) {
    const QoreTypeInfo* typeInfo = qore_rt_resolve_full_type_path(type_path, "MakeHashConstKeys", xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return qore_rt_make_hash_const_keys(keys, vals, count, typeInfo, xsink);
}

// --- Statement execution helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_exec_statement(int opcode, const AbstractStatement* stmt, ExceptionSink* xsink) {
    if (!stmt) {
        return toBits(QoreValue());
    }
    if (xsink && !*xsink) {
        const QoreProgramLocation* loc = stmt->loc;
        if (loc) {
            xsink->raiseException("IR-AST-FALLBACK-ERROR",
                "qore_rt_exec_statement: executable AST statement fallback is disabled: "
                "opcode=%s(%d) source=%s:%d; add native IR/JIT lowering instead",
                getOpcodeName(opcode), opcode, loc->getFileValue(), loc->start_line);
        } else {
            xsink->raiseException("IR-AST-FALLBACK-ERROR",
                "qore_rt_exec_statement: executable AST statement fallback is disabled: "
                "opcode=%s(%d); add native IR/JIT lowering instead",
                getOpcodeName(opcode), opcode);
        }
    }
    return toBits(QoreValue());
}

extern "C" DLLEXPORT void qore_rt_thread_exit(ExceptionSink* xsink) {
    if (xsink) {
        xsink->raiseThreadExit();
    }
}

// --- Guard type helper ---

extern "C" DLLEXPORT int64_t qore_rt_guard_type(uint64_t val, const QoreTypeInfo* type_info) {
    QoreValue v = fromBits(val);
    return QoreTypeInfo::runtimeAcceptsValue(type_info, v) != QTI_NOT_EQUAL ? 1 : 0;
}

// --- InstanceOf helper ---

extern "C" DLLEXPORT uint64_t qore_rt_instanceof(uint64_t val_bits, const QoreTypeInfo* ti) {
    QoreValue val = fromBits(val_bits);
    qore_type_t t = val.getType();
    bool result;
    switch (t) {
        case NT_WEAKREF:
            result = QoreTypeInfo::runtimeAcceptsValue(ti,
                **val.get<const WeakReferenceNode>()) != QTI_NOT_EQUAL;
            break;
        case NT_WEAKREF_HASH:
            result = QoreTypeInfo::runtimeAcceptsValue(ti,
                **val.get<const WeakHashReferenceNode>()) != QTI_NOT_EQUAL;
            break;
        case NT_WEAKREF_LIST:
            result = QoreTypeInfo::runtimeAcceptsValue(ti,
                **val.get<const WeakListReferenceNode>()) != QTI_NOT_EQUAL;
            break;
        default:
            result = QoreTypeInfo::runtimeAcceptsValue(ti, val) != QTI_NOT_EQUAL;
            break;
    }
    return toBits(QoreValue(result));
}

// AOT mode: instanceof check with type path string instead of QoreTypeInfo pointer.
// Resolves the type path to QoreTypeInfo* using the program's type resolver, then
// delegates to qore_rt_instanceof.
extern "C" DLLEXPORT uint64_t qore_rt_instanceof_by_type_path(uint64_t val_bits,
        const char* type_path, ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();
    if (!pgm || !type_path || !*type_path) {
        return toBits(QoreValue(false));
    }
    std::string error;
    QoreAOTTypeResolver resolver(pgm);
    const QoreTypeInfo* ti = resolver.resolve(type_path, error);
    if (!ti) {
        return toBits(QoreValue(false));
    }
    return qore_rt_instanceof(val_bits, ti);
}

// --- Date construction helper ---

extern "C" DLLEXPORT uint64_t qore_rt_make_date(int64_t date_microseconds, int64_t is_relative) {
    DateTimeNode* dt;
    if (is_relative) {
        dt = new DateTimeNode(true);
        dt->setRelativeDateSeconds(date_microseconds / 1000000,
            static_cast<int>(date_microseconds % 1000000));
    } else {
        // date_microseconds is a UTC epoch from getEpochMicrosecondsUTC(); use makeAbsolute()
        // which stores the epoch directly without local-to-UTC conversion (unlike DateTimeNode(s, ms)
        // which goes through setLocalDate → setLocalIntern → subtracts timezone offset)
        int64_t epoch_seconds = date_microseconds / 1000000;
        int us = static_cast<int>(date_microseconds % 1000000);
        dt = DateTimeNode::makeAbsolute(currentTZ(), epoch_seconds, us);
    }
    return toBits(QoreValue(dt));
}

static const AbstractQoreZoneInfo* qore_rt_resolve_date_zone(const char* zone_name) {
    if (!zone_name || !*zone_name || !strcmp(zone_name, "UTC")) {
        return nullptr;
    }

    ExceptionSink xsink;
    const AbstractQoreZoneInfo* zone = (*zone_name == '+' || *zone_name == '-')
        ? QTZM.findCreateOffsetZone(zone_name, &xsink)
        : QTZM.findLoadRegion(zone_name, &xsink);
    if (xsink) {
        xsink.clear();
        return nullptr;
    }
    return zone;
}

extern "C" DLLEXPORT uint64_t qore_rt_make_date_ex(int64_t date_microseconds, int64_t is_relative,
        const char* zone_name, int64_t rel_years, int64_t rel_months, int64_t rel_days, int64_t rel_hours,
        int64_t rel_minutes, int64_t rel_seconds, int64_t rel_us) {
    DateTimeNode* dt;
    if (is_relative) {
        dt = DateTimeNode::makeRelativeUnnormalized(
            static_cast<int>(rel_years),
            static_cast<int>(rel_months),
            static_cast<int>(rel_days),
            static_cast<int>(rel_hours),
            static_cast<int>(rel_minutes),
            static_cast<int>(rel_seconds),
            static_cast<int>(rel_us));
    } else {
        int64_t epoch_seconds = date_microseconds / 1000000;
        int us = static_cast<int>(date_microseconds % 1000000);
        if (us < 0) {
            --epoch_seconds;
            us += 1000000;
        }
        dt = DateTimeNode::makeAbsolute(qore_rt_resolve_date_zone(zone_name), epoch_seconds, us);
    }
    return toBits(QoreValue(dt));
}

// --- Enum construction helper ---

extern "C" DLLEXPORT uint64_t qore_rt_make_enum(int64_t member_ptr) {
    const QoreEnumMember* member = reinterpret_cast<const QoreEnumMember*>(member_ptr);
    return toBits(QoreValue::makeEnum(member));
}

// --- Phase 2B Step 5: Container construction throwing wrappers ---

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_make_list_throwing(
        uint64_t* vals, int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    uint64_t result = qore_rt_make_list(vals, count, typeInfo, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_make_list_by_type_path_throwing(
        uint64_t* vals, int count, const char* type_path, ExceptionSink* xsink) {
    uint64_t result = qore_rt_make_list_by_type_path(vals, count, type_path, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_make_hash_throwing(
        uint64_t* kv_pairs, int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    uint64_t result = qore_rt_make_hash(kv_pairs, count, typeInfo, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_make_hash_by_type_path_throwing(
        uint64_t* kv_pairs, int count, const char* type_path, ExceptionSink* xsink) {
    uint64_t result = qore_rt_make_hash_by_type_path(kv_pairs, count, type_path, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_make_hash_const_keys_throwing(
        const char** keys, uint64_t* vals, int count,
        const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    uint64_t result = qore_rt_make_hash_const_keys(keys, vals, count, typeInfo, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_make_hash_const_keys_by_type_path_throwing(
        const char** keys, uint64_t* vals, int count, const char* type_path, ExceptionSink* xsink) {
    uint64_t result = qore_rt_make_hash_const_keys_by_type_path(keys, vals, count, type_path, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_sprintf_throwing(
        uint64_t val_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_sprintf(val_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_backquote_throwing(
        const char* cmd, ExceptionSink* xsink) {
    uint64_t result = qore_rt_backquote(cmd, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_find_throwing(
        uint64_t exp_bits, uint64_t find_exp_bits, uint64_t where_bits,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_find(exp_bits, find_exp_bits, where_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_find_mode_throwing(
        uint64_t exp_bits, uint64_t find_exp_bits, uint64_t where_bits, int32_t mode,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_find_mode(exp_bits, find_exp_bits, where_bits, mode, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

// --- Specialized access helpers (Phase 5b optimizations) ---

static size_t qore_rt_select_precomputed_hash(uint64_t hash64, uint32_t hash32) {
#if TARGET_BITS == 64
    return static_cast<size_t>(hash64);
#else
    return static_cast<size_t>(hash32);
#endif
}

static QoreValue qore_rt_get_hash_key_value(const QoreHashNode* h, const char* key,
        size_t key_hash, bool prehashed, ExceptionSink* xsink) {
    return prehashed
        ? qore_hash_private::get(*h)->getKeyValuePrehashed(key, key_hash, xsink)
        : h->getKeyValue(key, xsink);
}

static uint64_t qore_rt_hash_key_access_hash_impl(uint64_t hash_val, const char* key,
        size_t key_hash, bool prehashed, ExceptionSink* xsink) {
    const QoreHashNode* h = fromBits(hash_val).get<const QoreHashNode>();
    QoreValue result = qore_rt_get_hash_key_value(h, key, key_hash, prehashed, xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    result.refSelf();
    if (!qore_rt_apply_complex_hash_value_type(h, key, result, xsink)) {
        result.discard(xsink);
        return toBits(QoreValue());
    }
    qore_rt_evaluate_owned_weak_reference_result(result, xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(result);
}

static uint64_t qore_rt_hash_key_access_impl(uint64_t hash_val, const char* key,
        size_t key_hash, bool prehashed, ExceptionSink* xsink, bool preserve_weak_result) {
    QoreValue raw_v = fromBits(hash_val);
    ValueEvalOptimizedRefHolder vh(raw_v, xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    QoreValue v = *vh;
    // Unwrap weak references: member access on a weak reference target is transparent,
    // mirroring the IR interpreter's QoreIROpcode::HashKeyAccess handling.
    if (v.getType() == NT_WEAKREF) {
        QoreObject* o = v.get<const WeakReferenceNode>()->get();
        if (!o || !o->isValid()) {
            return toBits(QoreValue());
        }
        QoreValue rv = o->evalMember(key, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
        if (!preserve_weak_result) {
            qore_rt_evaluate_owned_weak_reference_result(rv, xsink);
            if (*xsink) {
                return toBits(QoreValue());
            }
        }
        return toBits(rv);
    }
    if (v.getType() == NT_WEAKREF_HASH) {
        const QoreHashNode* h = v.get<const WeakHashReferenceNode>()->get();
        if (!h) {
            return toBits(QoreValue());
        }
        QoreValue result = qore_rt_get_hash_key_value(h, key, key_hash, prehashed, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
        result.refSelf();
        if (!qore_rt_apply_complex_hash_value_type(h, key, result, xsink)) {
            result.discard(xsink);
            return toBits(QoreValue());
        }
        if (!preserve_weak_result) {
            qore_rt_evaluate_owned_weak_reference_result(result, xsink);
            if (*xsink) {
                return toBits(QoreValue());
            }
        }
        return toBits(result);
    }
    if (v.getType() == NT_HASH) {
        const QoreHashNode* h = v.get<const QoreHashNode>();
        QoreValue result = qore_rt_get_hash_key_value(h, key, key_hash, prehashed, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
        result.refSelf();
        if (!qore_rt_apply_complex_hash_value_type(h, key, result, xsink)) {
            result.discard(xsink);
            return toBits(QoreValue());
        }
        if (!preserve_weak_result) {
            qore_rt_evaluate_owned_weak_reference_result(result, xsink);
            if (*xsink) {
                return toBits(QoreValue());
            }
        }
        return toBits(result);
    }
    if (v.getType() == NT_OBJECT) {
        QoreObject* o = const_cast<QoreObject*>(v.get<const QoreObject>());
        QoreValue rv = o->evalMember(key, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
        if (!preserve_weak_result) {
            qore_rt_evaluate_owned_weak_reference_result(rv, xsink);
            if (*xsink) {
                return toBits(QoreValue());
            }
        }
        return toBits(rv);
    }
    // Not a hash or object (or NOTHING/NULL): return NOTHING
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access(uint64_t hash_val, const char* key, ExceptionSink* xsink) {
    return qore_rt_hash_key_access_impl(hash_val, key, 0, false, xsink, false);
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_for_call(uint64_t hash_val, const char* key,
        ExceptionSink* xsink) {
    return qore_rt_hash_key_access_impl(hash_val, key, 0, false, xsink, true);
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_prehashed(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    return qore_rt_hash_key_access_impl(hash_val, key,
        qore_rt_select_precomputed_hash(hash64, hash32), true, xsink, false);
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_for_call_prehashed(uint64_t hash_val,
        const char* key, uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    return qore_rt_hash_key_access_impl(hash_val, key,
        qore_rt_select_precomputed_hash(hash64, hash32), true, xsink, true);
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_hash(uint64_t hash_val, const char* key,
        ExceptionSink* xsink) {
    return qore_rt_hash_key_access_hash_impl(hash_val, key, 0, false, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_hash_prehashed(uint64_t hash_val,
        const char* key, uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    return qore_rt_hash_key_access_hash_impl(hash_val, key,
        qore_rt_select_precomputed_hash(hash64, hash32), true, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_hash_guarded(uint64_t hash_val,
        const char* key, ExceptionSink* xsink) {
    QoreValue value = fromBits(hash_val);
    return value.getType() == NT_HASH
        ? qore_rt_hash_key_access_hash_impl(hash_val, key, 0, false, xsink)
        : toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_hash_guarded_prehashed(uint64_t hash_val,
        const char* key, uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    QoreValue value = fromBits(hash_val);
    return value.getType() == NT_HASH
        ? qore_rt_hash_key_access_hash_impl(hash_val, key,
            qore_rt_select_precomputed_hash(hash64, hash32), true, xsink)
        : toBits(QoreValue());
}

static int64_t qore_rt_hash_key_truthy_impl(uint64_t hash_val, const char* key,
        size_t key_hash, bool prehashed, bool guarded, ExceptionSink* xsink) {
    QoreValue hash_value = fromBits(hash_val);
    if (guarded && hash_value.getType() != NT_HASH) {
        return 0;
    }
    const QoreHashNode* hash = hash_value.get<const QoreHashNode>();
    QoreValue result = qore_rt_get_hash_key_value(hash, key, key_hash, prehashed, xsink);
    if (*xsink) {
        return 0;
    }

    const QoreTypeInfo* value_type = hash->getValueTypeInfo();
    bool type_accepted = hash->getHashDecl() || result.isNothing()
        || !QoreTypeInfo::hasType(value_type) || value_type == autoTypeInfo
        || value_type == anyTypeInfo
        || QoreTypeInfo::runtimeAcceptsValue(value_type, result) != QTI_NOT_EQUAL;
    if (!result.hasNode() && type_accepted) {
        return result.getAsBool() ? 1 : 0;
    }

    result.refSelf();
    if (!qore_rt_apply_complex_hash_value_type(hash, key, result, xsink)) {
        result.discard(xsink);
        return 0;
    }
    qore_rt_evaluate_owned_weak_reference_result(result, xsink);
    if (*xsink) {
        return 0;
    }
    bool truthy = result.getAsBool();
    result.discard(xsink);
    return truthy ? 1 : 0;
}

extern "C" DLLEXPORT int64_t qore_rt_hash_key_truthy(uint64_t hash_val,
        const char* key, ExceptionSink* xsink) {
    return qore_rt_hash_key_truthy_impl(hash_val, key, 0, false, false, xsink);
}

extern "C" DLLEXPORT int64_t qore_rt_hash_key_truthy_prehashed(uint64_t hash_val,
        const char* key, uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    return qore_rt_hash_key_truthy_impl(hash_val, key,
        qore_rt_select_precomputed_hash(hash64, hash32), true, false, xsink);
}

extern "C" DLLEXPORT int64_t qore_rt_hash_key_truthy_guarded(uint64_t hash_val,
        const char* key, ExceptionSink* xsink) {
    return qore_rt_hash_key_truthy_impl(hash_val, key, 0, false, true, xsink);
}

extern "C" DLLEXPORT int64_t qore_rt_hash_key_truthy_guarded_prehashed(
        uint64_t hash_val, const char* key, uint64_t hash64, uint32_t hash32,
        ExceptionSink* xsink) {
    return qore_rt_hash_key_truthy_impl(hash_val, key,
        qore_rt_select_precomputed_hash(hash64, hash32), true, true, xsink);
}

extern "C" DLLEXPORT __attribute__((noinline)) int64_t qore_rt_hash_key_truthy_throwing(
        uint64_t hash_val, const char* key, ExceptionSink* xsink) {
    int64_t result = qore_rt_hash_key_truthy(hash_val, key, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) int64_t
qore_rt_hash_key_truthy_prehashed_throwing(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    int64_t result = qore_rt_hash_key_truthy_prehashed(
        hash_val, key, hash64, hash32, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) int64_t qore_rt_hash_key_truthy_guarded_throwing(
        uint64_t hash_val, const char* key, ExceptionSink* xsink) {
    int64_t result = qore_rt_hash_key_truthy_guarded(hash_val, key, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) int64_t
qore_rt_hash_key_truthy_guarded_prehashed_throwing(uint64_t hash_val,
        const char* key, uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    int64_t result = qore_rt_hash_key_truthy_guarded_prehashed(
        hash_val, key, hash64, hash32, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT uint64_t qore_rt_fixed_hash_remap2_aot(QoreAOTContext* ctx,
        int32_t slot, uint64_t value_bits, const char* input_key1,
        uint64_t input_hash1_64, uint32_t input_hash1_32, const char* output_key1,
        const char* input_key2, uint64_t input_hash2_64, uint32_t input_hash2_32,
        const char* output_key2, const char* type_path, ExceptionSink* xsink) {
    QoreValue value = fromBits(value_bits);
    if (value.getType() != NT_HASH) {
        uint64_t args[1] = {value_bits};
        return qore_rt_call_direct_aot(ctx, slot, args, 1, xsink);
    }

    uint64_t values[2];
    values[0] = qore_rt_hash_key_access_hash_impl(value_bits, input_key1,
        qore_rt_select_precomputed_hash(input_hash1_64, input_hash1_32), true, xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    values[1] = qore_rt_hash_key_access_hash_impl(value_bits, input_key2,
        qore_rt_select_precomputed_hash(input_hash2_64, input_hash2_32), true, xsink);
    if (*xsink) {
        fromBits(values[0]).discard(xsink);
        return toBits(QoreValue());
    }

    const char* output_keys[2] = {output_key1, output_key2};
    uint64_t result = type_path
        ? qore_rt_make_hash_const_keys_by_type_path(output_keys, values, 2, type_path, xsink)
        : qore_rt_make_hash_const_keys(output_keys, values, 2, nullptr, xsink);
    fromBits(values[0]).discard(xsink);
    fromBits(values[1]).discard(xsink);
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_fixed_hash_remap2_aot_throwing(QoreAOTContext* ctx, int32_t slot,
        uint64_t value, const char* input_key1, uint64_t input_hash1_64,
        uint32_t input_hash1_32, const char* output_key1, const char* input_key2,
        uint64_t input_hash2_64, uint32_t input_hash2_32, const char* output_key2,
        const char* type_path, ExceptionSink* xsink) {
    uint64_t result = qore_rt_fixed_hash_remap2_aot(ctx, slot, value, input_key1,
        input_hash1_64, input_hash1_32, output_key1, input_key2, input_hash2_64,
        input_hash2_32, output_key2, type_path, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_int(uint64_t hash_val, const char* key) {
    ExceptionSink xsink;
    QoreValue raw_v = fromBits(hash_val);
    ValueEvalOptimizedRefHolder vh(raw_v, &xsink);
    if (xsink) {
        return toBits(QoreValue());
    }
    QoreValue v = *vh;
    if (v.getType() == NT_HASH) {
        const QoreHashNode* h = v.get<const QoreHashNode>();
        bool exists = false;
        QoreValue val = h->getKeyValueExistence(key, exists);
        if (exists) {
            return toBits(val.getAsBigInt());
        }
    }
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_int_prehashed(uint64_t hash_val,
        const char* key, uint64_t hash64, uint32_t hash32) {
    ExceptionSink xsink;
    QoreValue raw_v = fromBits(hash_val);
    ValueEvalOptimizedRefHolder vh(raw_v, &xsink);
    if (xsink) {
        return toBits(QoreValue());
    }
    QoreValue v = *vh;
    if (v.getType() == NT_HASH) {
        const QoreHashNode* h = v.get<const QoreHashNode>();
        bool exists = false;
        QoreValue val = qore_hash_private::get(*h)->getKeyValueExistencePrehashedIntern(key,
            qore_rt_select_precomputed_hash(hash64, hash32), exists);
        if (exists) {
            return toBits(val.getAsBigInt());
        }
    }
    return toBits(QoreValue());
}

static QoreHashNode* qore_rt_make_implicit_hash_for_lvalue(LocalVar* var, ExceptionSink* xsink) {
    const QoreTypeInfo* typeInfo = var ? var->getTypeInfoForLValue() : nullptr;
    typeInfo = qore_substitute_type_params_if_needed(typeInfo);
    if (!QoreTypeInfo::parseAcceptsReturns(typeInfo, NT_HASH)) {
        xsink->raiseException("RUNTIME-TYPE-ERROR", "cannot convert lvalue declared as %s to a hash",
            QoreTypeInfo::getName(typeInfo));
        return nullptr;
    }

    if (!typeInfo || typeInfo == anyTypeInfo || typeInfo == hashTypeInfo || typeInfo == hashOrNothingTypeInfo) {
        return new QoreHashNode;
    }

    const QoreTypeInfo* sti = typeInfo == autoTypeInfo
        ? autoTypeInfo
        : QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
    if (sti) {
        return new QoreHashNode(sti);
    }

    const TypedHashDecl* thd = QoreTypeInfo::getUniqueReturnHashDecl(typeInfo);
    if (thd) {
        QoreStringNode* desc = new QoreStringNodeMaker("Cannot implicitly create typed hash '%s' "
            "with an assignment; to address this error, declare the typed hash before the assignment",
            thd->getName());
        xsink->raiseException("HASHDECL-IMPLICIT-CONSTRUCTION-ERROR", desc);
        return nullptr;
    }

    return new QoreHashNode(QoreTypeInfo::getElementType(QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo)));
}

static void qore_rt_assign_object_member_lvalue(QoreObject* obj, const char* key, const QoreValue& val,
        ExceptionSink* xsink) {
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !qore_class_private::runtimeCheckPrivateClassAccess(*obj->getClass(), class_ctx)) {
        class_ctx = nullptr;
    }

    LValueHelper helper(xsink);
    if (qore_object_private::getLValue(*obj, key, helper, class_ctx, false, xsink)) {
        return;
    }

    QoreValue stored = val.hasNode() ? val.refSelf() : val;
    helper.assign(stored);
}

// JIT path: write hash{key} = value with copy-on-write support.
// var: container LocalVar* (used to update the local when COW triggers).
extern "C" DLLEXPORT uint64_t qore_rt_hash_key_store_cow(
        LocalVar* var, uint64_t hash_bits, const char* key,
        uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue hv = fromBits(hash_bits);
    QoreValue val = fromBits(value_bits);
    ValueHolder val_holder(val.refSelf(), xsink);
    if (hv.getType() == NT_HASH) {
        QoreHashNode* h = hv.get<QoreHashNode>();
        // Keep RHS referenced before COW, matching QoreAssignmentOperatorNode.
        // This makes `h.b = h` copy the outer hash before storing the original.
        if (h->reference_count() > 1) {
            QoreHashNode* new_h = h->copy();
            qore_rt_assign_local(var, toBits(QoreValue(new_h)), xsink);
            if (*xsink) {
                new_h->deref(nullptr);
                return toBits(QoreValue());
            }
            // Release copy()'s original ref; variable holds sole ref
            new_h->deref(nullptr);
            h = new_h;
        }
        h->setKeyValue(key, val.refSelf(), xsink);
    } else if (hv.isNothing()) {
        // Auto-vivify according to the declared lvalue type, matching
        // LValueHelper::doHashLValue() including hashdecl error behavior.
        QoreHashNode* new_h = qore_rt_make_implicit_hash_for_lvalue(var, xsink);
        if (!new_h) {
            return toBits(QoreValue());
        }
        new_h->setKeyValue(key, val.refSelf(), xsink);
        if (!*xsink) {
            qore_rt_assign_local(var, toBits(QoreValue(new_h)), xsink);
        }
        if (*xsink) {
            new_h->deref(xsink);
            return toBits(QoreValue());
        }
        new_h->deref(nullptr);
    } else if (hv.getType() == NT_OBJECT) {
        qore_rt_assign_object_member_lvalue(const_cast<QoreObject*>(hv.get<const QoreObject>()), key, val, xsink);
    }
    return value_bits;
}

// AOT path: same semantics but container is identified by its slot index in QoreAOTContext.
extern "C" DLLEXPORT uint64_t qore_rt_hash_key_store_cow_aot(
        QoreAOTContext* ctx, uint32_t local_slot,
        uint64_t hash_bits, const char* key,
        uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue hv = fromBits(hash_bits);
    QoreValue val = fromBits(value_bits);
    ValueHolder val_holder(val.refSelf(), xsink);
    if (hv.getType() == NT_HASH) {
        QoreHashNode* h = hv.get<QoreHashNode>();
        // Keep RHS referenced before COW, matching QoreAssignmentOperatorNode.
        // This makes `h.b = h` copy the outer hash before storing the original.
        if (h->reference_count() > 1) {
            QoreHashNode* new_h = h->copy();
            qore_rt_assign_local_aot(ctx, local_slot, toBits(QoreValue(new_h)), xsink);
            if (*xsink) {
                new_h->deref(nullptr);
                return toBits(QoreValue());
            }
            // Release copy()'s original ref; variable holds sole ref
            new_h->deref(nullptr);
            h = new_h;
        }
        h->setKeyValue(key, val.refSelf(), xsink);
    } else if (hv.isNothing()) {
        // Auto-vivify according to the declared lvalue type, matching
        // LValueHelper::doHashLValue() including hashdecl error behavior.
        LocalVar* var = ctx && local_slot < static_cast<uint32_t>(ctx->num_locals)
            ? ctx->locals[local_slot] : nullptr;
        QoreHashNode* new_h = qore_rt_make_implicit_hash_for_lvalue(var, xsink);
        if (!new_h) {
            return toBits(QoreValue());
        }
        new_h->setKeyValue(key, val.refSelf(), xsink);
        if (!*xsink) {
            qore_rt_assign_local_aot(ctx, local_slot, toBits(QoreValue(new_h)), xsink);
        }
        if (*xsink) {
            new_h->deref(xsink);
            return toBits(QoreValue());
        }
        new_h->deref(nullptr);
    } else if (hv.getType() == NT_OBJECT) {
        qore_rt_assign_object_member_lvalue(const_cast<QoreObject*>(hv.get<const QoreObject>()), key, val, xsink);
    }
    return value_bits;
}

// JIT path: hash[dynamic_key] = value with COW (key is NaN-boxed QoreValue, converted to string)
extern "C" DLLEXPORT uint64_t qore_rt_hash_key_store_dynamic_cow(
        LocalVar* var, uint64_t hash_bits, uint64_t key_bits,
        uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue key_val = fromBits(key_bits);
    QoreStringValueHelper key_str(key_val);
    return qore_rt_hash_key_store_cow(var, hash_bits, key_str->c_str(), value_bits, xsink);
}

// AOT path: hash[dynamic_key] = value with COW (key is NaN-boxed QoreValue, converted to string)
extern "C" DLLEXPORT uint64_t qore_rt_hash_key_store_dynamic_cow_aot(
        QoreAOTContext* ctx, uint32_t local_slot,
        uint64_t hash_bits, uint64_t key_bits,
        uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue key_val = fromBits(key_bits);
    QoreStringValueHelper key_str(key_val);
    return qore_rt_hash_key_store_cow_aot(ctx, local_slot, hash_bits, key_str->c_str(), value_bits, xsink);
}

// JIT path: list[index] = value with COW
// var: container LocalVar* (used to update the local when COW triggers).
extern "C" DLLEXPORT uint64_t qore_rt_list_index_store_cow(
        LocalVar* var, uint64_t list_bits, int64_t index,
        uint64_t val_bits, ExceptionSink* xsink) {
    QoreValue lv = fromBits(list_bits);
    QoreValue val = fromBits(val_bits);
    ValueHolder val_holder(val.refSelf(), xsink);
    bool negative_offsets = runtime_check_parse_option(PO_NEGATIVE_OFFSETS);

    if (lv.getType() == NT_LIST) {
        QoreListNode* l = lv.get<QoreListNode>();
        // ListIndexStore loads the container without taking an extra reference.
        // Any ref beyond the lvalue's own runtime slot is real sharing and must
        // trigger COW; LLVM-side reload/cache refs are cleared before this helper.
        if (l->reference_count() > 1) {
            QoreListNode* new_l = l->copy();
            qore_rt_assign_local(var, toBits(QoreValue(new_l)), xsink);
            if (*xsink) {
                new_l->deref(nullptr);
                return toBits(QoreValue());
            }
            // Release copy()'s original ref; variable holds sole ref (refcount==1)
            new_l->deref(nullptr);
            l = new_l;
        }

        if (index < 0) {
            if (negative_offsets) {
                index += static_cast<int64_t>(l->size());
            }
            if (index < 0) {
                xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must evaluate "
                    "to a non-negative integer)", index);
                return toBits(QoreValue());
            }
        }

        // Apply element type coercion if the list has a typed value type
        // (e.g. list<softint> converts "50" → 50 before storing)
        QoreValue entry = val.hasNode() ? val.refSelf() : val;
        const QoreTypeInfo* vti = qore_list_private::get(*l)->getValueTypeInfo();
        if (QoreTypeInfo::hasType(vti)
                && !QoreTypeInfo::superSetOf(vti, entry.getTypeInfo())) {
            QoreTypeInfo::acceptAssignment(vti,
                "<list element assignment>", entry, xsink);
        }
        if (!*xsink) {
            l->setEntry(index, entry, xsink);
        } else {
            entry.discard(xsink);
        }
    } else if (lv.isNothing()) {
        if (index < 0) {
            xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must evaluate to a "
                "non-negative integer)", index);
            return toBits(QoreValue());
        }
        // Auto-vivify: create new list from NOTHING, set element, assign to variable
        // Use variable's declared type to derive proper element type
        // (e.g. softlist<bool> → list<bool>, not list<auto>)
        const QoreTypeInfo* varTI = var->getTypeInfo();
        const QoreTypeInfo* elemTI = QoreTypeInfo::getReturnComplexListOrNothing(varTI);
        QoreListNode* new_l = new QoreListNode(elemTI ? elemTI : autoTypeInfo);
        QoreValue entry = val.hasNode() ? val.refSelf() : val;
        // Apply element type coercion if needed
        const QoreTypeInfo* vti = qore_list_private::get(*new_l)->getValueTypeInfo();
        if (QoreTypeInfo::hasType(vti)
                && !QoreTypeInfo::superSetOf(vti, entry.getTypeInfo())) {
            QoreTypeInfo::acceptAssignment(vti,
                "<list element assignment>", entry, xsink);
        }
        if (!*xsink) {
            new_l->setEntry(index, entry, xsink);
        } else {
            entry.discard(xsink);
        }
        if (!*xsink) {
            qore_rt_assign_local(var, toBits(QoreValue(new_l)), xsink);
        }
        if (*xsink) {
            new_l->deref(xsink);
            return toBits(QoreValue());
        }
        new_l->deref(nullptr);
    }
    return val_bits;
}

// AOT path: list[index] = value with COW
// ctx: QoreAOTContext, local_slot identifies the list's slot
extern "C" DLLEXPORT uint64_t qore_rt_list_index_store_cow_aot(
        QoreAOTContext* ctx, uint32_t local_slot,
        uint64_t list_bits, int64_t index,
        uint64_t val_bits, ExceptionSink* xsink) {
    QoreValue lv = fromBits(list_bits);
    QoreValue val = fromBits(val_bits);
    ValueHolder val_holder(val.refSelf(), xsink);
    bool negative_offsets = runtime_check_parse_option(PO_NEGATIVE_OFFSETS);
    if (lv.getType() == NT_LIST) {
        QoreListNode* l = lv.get<QoreListNode>();
        // ListIndexStore loads the container without taking an extra reference.
        // Any ref beyond the lvalue's own runtime slot is real sharing and must
        // trigger COW; LLVM-side reload/cache refs are cleared before this helper.
        if (l->reference_count() > 1) {
            QoreListNode* new_l = l->copy();
            qore_rt_assign_local_aot(ctx, local_slot, toBits(QoreValue(new_l)), xsink);
            if (*xsink) {
                new_l->deref(nullptr);
                return toBits(QoreValue());
            }
            // Release copy()'s original ref; variable holds sole ref (refcount==1)
            new_l->deref(nullptr);
            l = new_l;
        }
        if (index < 0) {
            if (negative_offsets) {
                index += static_cast<int64_t>(l->size());
            }
            if (index < 0) {
                xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must evaluate "
                    "to a non-negative integer)", index);
                return toBits(QoreValue());
            }
        }
        // Apply element type coercion if the list has a typed value type
        QoreValue entry = val.hasNode() ? val.refSelf() : val;
        const QoreTypeInfo* vti = qore_list_private::get(*l)->getValueTypeInfo();
        if (QoreTypeInfo::hasType(vti)
                && !QoreTypeInfo::superSetOf(vti, entry.getTypeInfo())) {
            QoreTypeInfo::acceptAssignment(vti,
                "<list element assignment>", entry, xsink);
        }
        if (!*xsink) {
            l->setEntry(index, entry, xsink);
        } else {
            entry.discard(xsink);
        }
    } else if (lv.isNothing()) {
        if (index < 0) {
            xsink->raiseException("NEGATIVE-LIST-INDEX", "list index " QLLD " is invalid (index must evaluate to a "
                "non-negative integer)", index);
            return toBits(QoreValue());
        }
        // Auto-vivify: create new list from NOTHING, set element, assign to variable
        // Use variable's declared type to derive proper element type
        assert(local_slot < (uint32_t)ctx->num_locals);
        LocalVar* var = ctx->locals[local_slot];
        const QoreTypeInfo* varTI = var->getTypeInfo();
        const QoreTypeInfo* elemTI = QoreTypeInfo::getReturnComplexListOrNothing(varTI);
        QoreListNode* new_l = new QoreListNode(elemTI ? elemTI : autoTypeInfo);
        QoreValue entry = val.hasNode() ? val.refSelf() : val;
        // Apply element type coercion if needed
        const QoreTypeInfo* vti = qore_list_private::get(*new_l)->getValueTypeInfo();
        if (QoreTypeInfo::hasType(vti)
                && !QoreTypeInfo::superSetOf(vti, entry.getTypeInfo())) {
            QoreTypeInfo::acceptAssignment(vti,
                "<list element assignment>", entry, xsink);
        }
        if (!*xsink) {
            new_l->setEntry(index, entry, xsink);
        } else {
            entry.discard(xsink);
        }
        if (!*xsink) {
            qore_rt_assign_local_aot(ctx, local_slot, toBits(QoreValue(new_l)), xsink);
        }
        if (*xsink) {
            new_l->deref(xsink);
            return toBits(QoreValue());
        }
        new_l->deref(nullptr);
    }
    return val_bits;
}

extern "C" DLLEXPORT uint64_t qore_rt_list_index_access_compat(uint64_t list_val, int64_t index,
        int32_t string_index_char, ExceptionSink* xsink) {
    QoreValue raw_v = fromBits(list_val);
    ValueEvalOptimizedRefHolder vh(raw_v, xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    QoreValue v = *vh;
    bool negative_offsets = runtime_check_parse_option(PO_NEGATIVE_OFFSETS);
    int64 qindex = static_cast<int64>(index);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (QoreSquareBracketsOperatorNode::normalizeIndex(qindex, static_cast<int64>(l->size()),
                negative_offsets)) {
            return toBits(l->getReferencedEntry(static_cast<size_t>(qindex)));
        }
    } else if (v.getType() == NT_STRING) {
        QoreStringNodeValueHelper s(v);
        if (string_index_char) {
            return toBits(QoreValue::makeCharFromStringAt(**s, static_cast<qore_offset_t>(qindex), xsink));
        }
        return toBits(s->substr(static_cast<qore_offset_t>(qindex), 1, xsink));
    } else if (v.getType() == NT_BINARY) {
        const BinaryNode* b = v.get<const BinaryNode>();
        if (QoreSquareBracketsOperatorNode::normalizeIndex(qindex, static_cast<int64>(b->size()),
                negative_offsets)) {
            return toBits(QoreValue(static_cast<int64>(
                static_cast<const unsigned char*>(b->getPtr())[qindex])));
        }
    } else if (v.getType() == NT_BUFFER) {
        const QoreBufferNode* b = v.get<const QoreBufferNode>();
        if (QoreSquareBracketsOperatorNode::normalizeIndex(qindex, static_cast<int64>(b->size()),
                negative_offsets)) {
            return toBits(b->getReferencedEntry(static_cast<size_t>(qindex), xsink));
        }
    }
    // Unsupported container type or index out of bounds: return NOTHING.
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_list_index_access(uint64_t list_val, int64_t index, ExceptionSink* xsink) {
    return qore_rt_list_index_access_compat(list_val, index, 1, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_list_assignment_value(uint64_t value_bits, int64_t index,
        ExceptionSink* xsink) {
    (void)xsink;
    QoreValue value = fromBits(value_bits);
    if (value.getType() == NT_LIST) {
        const QoreListNode* l = value.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return toBits(l->getReferencedEntry(static_cast<size_t>(index)));
        }
        return toBits(QoreValue());
    }
    return index == 0 ? toBits(value.refSelf()) : toBits(QoreValue());
}

// --- Phase 2B Step 5: Hash/List operations throwing wrappers ---

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_hash_set_key_value_throwing(
        uint64_t hash_bits, uint64_t key_bits, uint64_t value_bits, ExceptionSink* xsink) {
    qore_rt_hash_set_key_value(hash_bits, key_bits, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_hash_key_access_throwing(
        uint64_t hash_val, const char* key, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_access(hash_val, key, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_hash_key_access_for_call_throwing(
        uint64_t hash_val, const char* key, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_access_for_call(hash_val, key, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_hash_key_access_prehashed_throwing(
        uint64_t hash_val, const char* key, uint64_t hash64, uint32_t hash32,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_access_prehashed(hash_val, key, hash64, hash32, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_hash_key_access_for_call_prehashed_throwing(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_access_for_call_prehashed(
        hash_val, key, hash64, hash32, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_hash_key_access_hash_throwing(
        uint64_t hash_val, const char* key, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_access_hash(hash_val, key, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_hash_key_access_hash_prehashed_throwing(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_access_hash_prehashed(hash_val, key, hash64, hash32, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_hash_key_access_hash_guarded_throwing(
        uint64_t hash_val, const char* key, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_access_hash_guarded(hash_val, key, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_hash_key_access_hash_guarded_prehashed_throwing(uint64_t hash_val, const char* key,
        uint64_t hash64, uint32_t hash32, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_access_hash_guarded_prehashed(
        hash_val, key, hash64, hash32, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_hash_key_store_cow_throwing(
        LocalVar* var, uint64_t hash_bits, const char* key,
        uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_store_cow(var, hash_bits, key, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_hash_key_store_cow_aot_throwing(
        QoreAOTContext* ctx, uint32_t local_slot, uint64_t hash_bits, const char* key,
        uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_store_cow_aot(ctx, local_slot, hash_bits, key,
            value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_hash_key_store_dynamic_cow_throwing(
        LocalVar* var, uint64_t hash_bits, uint64_t key_bits,
        uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_store_dynamic_cow(var, hash_bits, key_bits,
            value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_hash_key_store_dynamic_cow_aot_throwing(
        QoreAOTContext* ctx, uint32_t local_slot, uint64_t hash_bits, uint64_t key_bits,
        uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_hash_key_store_dynamic_cow_aot(ctx, local_slot, hash_bits,
            key_bits, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_list_push_throwing(
        uint64_t list_bits, uint64_t val_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_list_push(list_bits, val_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_list_push_typed_throwing(
        uint64_t list_bits, uint64_t val_bits, const QoreTypeInfo* element_type, ExceptionSink* xsink) {
    uint64_t result = qore_rt_list_push_typed(list_bits, val_bits, element_type, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_list_push_by_type_path_throwing(
        uint64_t list_bits, uint64_t val_bits, const char* element_type_path, ExceptionSink* xsink) {
    uint64_t result = qore_rt_list_push_by_type_path(list_bits, val_bits, element_type_path, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_list_push_in_place_throwing(
        uint64_t list_bits, uint64_t val_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_list_push_in_place(list_bits, val_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_list_index_access_throwing(
        uint64_t list_val, int64_t index, ExceptionSink* xsink) {
    uint64_t result = qore_rt_list_index_access(list_val, index, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_list_assignment_value_throwing(
        uint64_t value, int64_t index, ExceptionSink* xsink) {
    uint64_t result = qore_rt_list_assignment_value(value, index, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_list_index_store_cow_throwing(
        LocalVar* var, uint64_t list_bits, int64_t index,
        uint64_t val_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_list_index_store_cow(var, list_bits, index, val_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_list_index_store_cow_aot_throwing(
        QoreAOTContext* ctx, uint32_t local_slot, uint64_t list_bits, int64_t index,
        uint64_t val_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_list_index_store_cow_aot(ctx, local_slot, list_bits,
            index, val_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

// Runtime type check: returns 1 if value is NT_LIST or NT_OBJECT, 0 otherwise
// Used by select to determine if the result should be returned as a list or unwrapped
extern "C" DLLEXPORT int64_t qore_rt_is_collection_type(uint64_t val) {
    QoreValue v = fromBits(val);
    qore_type_t t = v.getType();
    return (t == NT_LIST || t == NT_OBJECT) ? 1 : 0;
}

// Optimized list iteration helpers for foldl/map/select
extern "C" DLLEXPORT int64_t qore_rt_list_size(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        return static_cast<int64_t>(v.get<const QoreListNode>()->size());
    }
    return 0;
}

extern "C" DLLEXPORT int64_t qore_rt_list_get_int(uint64_t list_val, int64_t index) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return l->retrieveEntry(index).getAsBigInt();
        }
    }
    return 0;
}

extern "C" DLLEXPORT double qore_rt_list_get_float(uint64_t list_val, int64_t index) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return l->retrieveEntry(index).getAsFloat();
        }
    }
    return 0.0;
}

extern "C" DLLEXPORT int64_t qore_rt_list_get_int_unchecked(uint64_t list_val, int64_t index) {
    const QoreListNode* list = fromBits(list_val).get<const QoreListNode>();
    return qore_list_private::get(*list)->entry[static_cast<size_t>(index)].getAsBigInt();
}

extern "C" DLLEXPORT double qore_rt_list_get_float_unchecked(uint64_t list_val, int64_t index) {
    const QoreListNode* list = fromBits(list_val).get<const QoreListNode>();
    return qore_list_private::get(*list)->entry[static_cast<size_t>(index)].getAsFloat();
}

extern "C" DLLEXPORT const uint64_t* qore_rt_list_get_data_unchecked(uint64_t list_val) {
    const QoreListNode* list = fromBits(list_val).get<const QoreListNode>();
    static_assert(sizeof(QoreValue) == sizeof(uint64_t));
    return reinterpret_cast<const uint64_t*>(qore_list_private::get(*list)->entry);
}

extern "C" DLLEXPORT void qore_rt_raise_typed_foreach_nothing(int32_t is_float, ExceptionSink* xsink) {
    if (xsink) {
        xsink->raiseException("RUNTIME-TYPE-ERROR",
            "<foreach lvalue assignment> expects type '%s', but got no value instead",
            is_float ? "float" : "int");
    }
}

extern "C" DLLEXPORT uint64_t* qore_rt_list_get_mutable_data_unchecked(uint64_t list_val) {
    QoreListNode* list = fromBits(list_val).get<QoreListNode>();
    static_assert(sizeof(QoreValue) == sizeof(uint64_t));
    return reinterpret_cast<uint64_t*>(qore_list_private::get(*list)->entry);
}

extern "C" DLLEXPORT uint64_t qore_rt_list_get_value(uint64_t list_val, int64_t index, ExceptionSink* xsink) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return toBits(l->getReferencedEntry(static_cast<size_t>(index)));
        }
    }
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_list_get_value_noref(uint64_t list_val, int64_t index, ExceptionSink* xsink) {
    // Read-only element access — returns borrowed reference (no refSelf)
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return toBits(l->retrieveEntry(static_cast<size_t>(index)));
        }
    }
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_create_sized_list(int64_t capacity, ExceptionSink* xsink) {
    return qore_rt_create_sized_list_typed(capacity, autoTypeInfo, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_create_sized_list_typed(int64_t capacity,
        const QoreTypeInfo* element_type, ExceptionSink* xsink) {
    element_type = qore_substitute_type_params_if_needed(element_type);
    QoreListNode* list = new QoreListNode(element_type ? element_type : autoTypeInfo);
    if (capacity > 0) {
        qore_list_private::get(*list)->reserve(static_cast<size_t>(capacity));
    }
    return toBits(QoreValue(list));
}

extern "C" DLLEXPORT uint64_t qore_rt_create_sized_list_by_type_path(int64_t capacity,
        const char* element_type_path, ExceptionSink* xsink) {
    const QoreTypeInfo* element_type = qore_rt_resolve_element_type_path(element_type_path,
        "CreateSizedList", xsink);
    if (!element_type) {
        return toBits(QoreValue());
    }
    return qore_rt_create_sized_list_typed(capacity, element_type, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_create_fixed_list_by_type_path(int64_t size,
        const char* element_type_path, ExceptionSink* xsink) {
    const QoreTypeInfo* element_type = qore_rt_resolve_element_type_path(element_type_path,
        "CreateSizedList", xsink);
    if (!element_type) {
        return toBits(QoreValue());
    }
    ReferenceHolder<QoreListNode> list(new QoreListNode(element_type), xsink);
    if (size > 0) {
        qore_list_private* priv = qore_list_private::get(**list);
        priv->reserve(static_cast<size_t>(size));
        for (int64_t i = 0; i < size; ++i) {
            if (i && !(i % 100) && qore_check_cancel(xsink, "fixed-size list construction")) {
                return toBits(QoreValue());
            }
            priv->entry[static_cast<size_t>(i)] = QoreValue();
        }
        priv->length = static_cast<size_t>(size);
    }
    return toBits(QoreValue(list.release()));
}

extern "C" DLLEXPORT void qore_rt_list_set_int(uint64_t list_bits, int64_t index, int64_t value) {
    QoreValue v = fromBits(list_bits);
    if (v.getType() == NT_LIST) {
        QoreListNode* l = v.get<QoreListNode>();
        qore_list_private* priv = qore_list_private::get(*l);
        priv->getEntryReference(static_cast<size_t>(index)) = QoreValue(value);
        if (static_cast<size_t>(index) >= priv->length) {
            priv->length = static_cast<size_t>(index) + 1;
        }
    }
}

extern "C" DLLEXPORT void qore_rt_list_set_float(uint64_t list_bits, int64_t index, double value) {
    QoreValue v = fromBits(list_bits);
    if (v.getType() == NT_LIST) {
        QoreListNode* l = v.get<QoreListNode>();
        qore_list_private* priv = qore_list_private::get(*l);
        priv->getEntryReference(static_cast<size_t>(index)) = QoreValue(value);
        if (static_cast<size_t>(index) >= priv->length) {
            priv->length = static_cast<size_t>(index) + 1;
        }
    }
}

extern "C" DLLEXPORT void qore_rt_list_set_value(uint64_t list_bits, int64_t index, uint64_t value_bits) {
    QoreValue v = fromBits(list_bits);
    if (v.getType() == NT_LIST) {
        QoreListNode* l = v.get<QoreListNode>();
        QoreValue val = fromBits(value_bits);
        qore_list_private* priv = qore_list_private::get(*l);
        // Track element type for correct list<T> type info at runtime
        priv->setListTypeFromNewElementType(val.getFullTypeInfo());
        priv->getEntryReference(static_cast<size_t>(index)) = val;
        if (static_cast<size_t>(index) >= priv->length) {
            priv->length = static_cast<size_t>(index) + 1;
        }
        if (needs_scan(val)) {
            priv->incScanCount(1);
        }
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_refself(uint64_t bits) {
    QoreValue v = fromBits(bits);
    return toBits(v.refSelf());
}

extern "C" DLLEXPORT uint64_t qore_rt_get_object_class(uint64_t obj_bits) {
    QoreValue v = fromBits(obj_bits);
    if (v.getType() == NT_OBJECT) {
        const QoreObject* obj = v.get<const QoreObject>();
        return reinterpret_cast<uint64_t>(obj->getClass());
    }
    return 0;
}

// Forward declaration — implementation below after instantiateFastCallParams and execJITWithDeopt
static uint64_t execClosureDirect(const QoreClosureBase* cb, const UserVariantBase* uvb,
        int nargs, const uint64_t* args, ExceptionSink* xsink,
        uint64_t** arg_cleanups = nullptr);

static bool tryExecClosureNativeLeaf(const AbstractQoreNode* node,
        const UserVariantBase* uvb, uint64_t* args, int nargs,
        uint64_t** arg_cleanups, uint64_t& result_bits) {
    static const bool disabled =
        std::getenv("QORE_DISABLE_IR_NATIVE_CLOSURE_LEAF_INLINE") != nullptr;
    if (disabled || !uvb || arg_cleanups
            || !uvb->isStaticallyFastCallEligible()) {
        return false;
    }
    const QoreIRFunction* callee_ir = uvb->getCachedIR();
    const UserSignature* sig = uvb->getUserSignature();
    if (!callee_ir || !sig) {
        return false;
    }

    struct NativeClosureLeafCache {
        NativeClosureLeafCache()
            : descriptor(nullptr, nullptr, nullptr, QoreValue()) {
        }

        const UserVariantBase* uvb = nullptr;
        int nargs = -1;
        QoreIRCallDirectInstruction descriptor;
    };
    static thread_local NativeClosureLeafCache cache;
    if (cache.uvb != uvb || cache.nargs != nargs
            || cache.descriptor.cached_callee_ir != callee_ir) {
        cache.uvb = uvb;
        cache.nargs = nargs;
        cache.descriptor.cached_callee_ir = callee_ir;
        cache.descriptor.cached_uvb = uvb;
        cache.descriptor.cached_return_type = sig->getReturnTypeInfo();
        cache.descriptor.native_leaf_state.store(0, std::memory_order_release);
    }

    QoreValue result;
    if (!qore_ir_try_execute_native_leaf(&cache.descriptor, args, nargs, result,
            node ? static_cast<const QoreClosureBase*>(node) : nullptr)) {
        return false;
    }
    result_bits = toBits(result);
    return true;
}

// Fast-path helper for closure calls with no arguments — avoids QoreListNode allocation
extern "C" DLLEXPORT uint64_t qore_rt_call_closure_0(uint64_t ref_bits, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    QoreValue ref_val = fromBits(ref_bits);
    if (!ref_val.hasNode()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot call a NOTHING value as a closure/call reference");
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = ref_val.getInternalNode();
    qore_type_t ntype = node->getType();

    // Fast path for closures: type check + static_cast instead of dynamic_cast
    if (ntype == NT_RUNTIME_CLOSURE) {
        const QoreClosureBase* cb = static_cast<const QoreClosureBase*>(node);
        // Check if the closure variant supports direct dispatch (has cached IR or JIT)
        UserClosureFunction* uf = static_cast<UserClosureFunction*>(cb->getFunction());
        assert(uf);
        const AbstractQoreFunctionVariant* variant = uf->first();
        const UserVariantBase* uvb = variant->getUserVariantBase();
        if (uvb && (uvb->hasCachedFunction() || uvb->getCachedIR())) {
            uint64_t leaf_result;
            if (!uvb->hasCachedAOT()
                    && tryExecClosureNativeLeaf(node, uvb, nullptr, 0, nullptr, leaf_result)) {
                return leaf_result;
            }
            return execClosureDirect(cb, uvb, 0, nullptr, xsink);
        }
        // Fall through to execValue for closures without cached IR/JIT
        QoreValue result = const_cast<QoreClosureBase*>(cb)->execValue(nullptr, xsink);
        return toBits(result);
    }

    // For function references (NT_FUNCREF) and other types: use type check + static_cast
    if (ntype == NT_FUNCREF) {
        ResolvedCallReferenceNode* callref = static_cast<ResolvedCallReferenceNode*>(
            const_cast<AbstractQoreNode*>(node));
        QoreValue result = callref->execValue(nullptr, xsink);
        return toBits(result);
    }

    xsink->raiseException("CALL-REFERENCE-ERROR", "value is not a call reference or closure");
    return toBits(QoreValue());
}

// Fast-path helper for closure calls with one argument — optimized list allocation
extern "C" DLLEXPORT uint64_t qore_rt_call_closure_1(uint64_t ref_bits, uint64_t arg0_bits, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    QoreValue ref_val = fromBits(ref_bits);
    if (!ref_val.hasNode()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot call a NOTHING value as a closure/call reference");
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = ref_val.getInternalNode();
    qore_type_t ntype = node->getType();

    // Fast path for closures: bypass QoreListNode + dynamic_cast
    if (ntype == NT_RUNTIME_CLOSURE) {
        const QoreClosureBase* cb = static_cast<const QoreClosureBase*>(node);
        UserClosureFunction* uf = static_cast<UserClosureFunction*>(cb->getFunction());
        assert(uf);
        const AbstractQoreFunctionVariant* variant = uf->first();
        const UserVariantBase* uvb = variant->getUserVariantBase();
        if (uvb && (uvb->hasCachedFunction() || uvb->getCachedIR())) {
            uint64_t leaf_result;
            if (!uvb->hasCachedAOT()
                    && tryExecClosureNativeLeaf(node, uvb, &arg0_bits, 1, nullptr, leaf_result)) {
                return leaf_result;
            }
            return execClosureDirect(cb, uvb, 1, &arg0_bits, xsink);
        }
        // Fall through to execValue for closures without cached IR/JIT
    }

    // Slow path: build QoreListNode and call execValue
    ResolvedCallReferenceNode* callref;
    if (ntype == NT_RUNTIME_CLOSURE || ntype == NT_FUNCREF) {
        callref = static_cast<ResolvedCallReferenceNode*>(const_cast<AbstractQoreNode*>(node));
    } else {
        xsink->raiseException("CALL-REFERENCE-ERROR", "value is not a call reference or closure");
        return toBits(QoreValue());
    }

    // Build single-element list with pre-allocated capacity
    ReferenceHolder<QoreListNode> arg_list(new QoreListNode(autoTypeInfo), xsink);
    QoreValue arg0 = fromBits(arg0_bits);
    if (arg0.hasNode()) {
        arg0.refSelf();
    }
    qore_list_private::get(**arg_list)->pushIntern(arg0);

    // Call directly with the single-element list (execValue borrows it)
    QoreValue result = callref->execValue(*arg_list, xsink);
    return toBits(result);
}

static uint64_t qore_rt_call_closure_fast_impl(uint64_t ref_bits, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    QoreValue ref_val = fromBits(ref_bits);
    if (!ref_val.hasNode()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot call a NOTHING value as a closure/call reference");
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = ref_val.getInternalNode();
    qore_type_t ntype = node->getType();

    // Fast path for closures: bypass QoreListNode + dynamic_cast
    if (ntype == NT_RUNTIME_CLOSURE) {
        const QoreClosureBase* cb = static_cast<const QoreClosureBase*>(node);
        UserClosureFunction* uf = static_cast<UserClosureFunction*>(cb->getFunction());
        assert(uf);
        const AbstractQoreFunctionVariant* variant = uf->first();
        const UserVariantBase* uvb = variant->getUserVariantBase();
        if (uvb && (uvb->hasCachedFunction() || uvb->getCachedIR())) {
            uint64_t leaf_result;
            if (!uvb->hasCachedAOT()
                    && tryExecClosureNativeLeaf(node, uvb, args, nargs, arg_cleanups,
                    leaf_result)) {
                return leaf_result;
            }
            return execClosureDirect(cb, uvb, nargs, args, xsink, arg_cleanups);
        }
        // Fall through to execValue for closures without cached IR/JIT
    }

    // Slow path: build QoreListNode and call execValue
    ResolvedCallReferenceNode* callref;
    if (ntype == NT_RUNTIME_CLOSURE || ntype == NT_FUNCREF) {
        callref = static_cast<ResolvedCallReferenceNode*>(const_cast<AbstractQoreNode*>(node));
    } else {
        xsink->raiseException("CALL-REFERENCE-ERROR", "value is not a call reference or closure");
        return toBits(QoreValue());
    }

    // Build QoreListNode from NaN-boxed args
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        return toBits(QoreValue());
    }

    QoreValue result = callref->execValue(*arg_list, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_closure_fast(uint64_t ref_bits, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    return qore_rt_call_closure_fast_impl(ref_bits, args, nullptr, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_closure_fast_consume_args(uint64_t ref_bits,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    return qore_rt_call_closure_fast_impl(ref_bits, args, arg_cleanups, nargs, xsink);
}

// Optimized map operations - native loops that return lists
extern "C" DLLEXPORT uint64_t qore_rt_map_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    result->push(iv->getAsBigInt() * scale, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        int64_t val = v.getAsBigInt();
        return toBits(QoreValue(val * scale));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsBigInt() * scale, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_scale_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    result->push(iv->getAsFloat() * scale, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        double val = v.getAsFloat();
        return toBits(QoreValue(val * scale));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsFloat() * scale, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_offset_int(uint64_t list_val, int64_t offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    result->push(iv->getAsBigInt() + offset, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        int64_t val = v.getAsBigInt();
        return toBits(QoreValue(val + offset));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsBigInt() + offset, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_offset_float(uint64_t list_val, double offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    result->push(iv->getAsFloat() + offset, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        double val = v.getAsFloat();
        return toBits(QoreValue(val + offset));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsFloat() + offset, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_square_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    int64_t val = iv->getAsBigInt();
                    result->push(val * val, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        int64_t val = v.getAsBigInt();
        return toBits(QoreValue(val * val));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        result->push(val * val, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_square_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    double val = iv->getAsFloat();
                    result->push(val * val, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        double val = v.getAsFloat();
        return toBits(QoreValue(val * val));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        result->push(val * val, nullptr);
    }
    return toBits(result.release());
}

// Fully specialized hash-key map operations (single runtime call per entire map)
extern "C" DLLEXPORT uint64_t qore_rt_map_hash_key_value(uint64_t list_val, const char* key) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            QoreValue val = elem.get<const QoreHashNode>()->getKeyValue(key);
            result->push(val.refSelf(), nullptr);
        } else {
            result->push(QoreValue(), nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_hash_key_int(uint64_t list_val, const char* key) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            result->push(elem.get<const QoreHashNode>()->getKeyValue(key).getAsBigInt(), nullptr);
        } else {
            result->push(0ll, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_hash_key_offset_int(uint64_t list_val, const char* key, int64_t offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            result->push(elem.get<const QoreHashNode>()->getKeyValue(key).getAsBigInt() + offset, nullptr);
        } else {
            result->push(offset, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_hash_key_scale_int(uint64_t list_val, const char* key, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            result->push(elem.get<const QoreHashNode>()->getKeyValue(key).getAsBigInt() * scale, nullptr);
        } else {
            result->push(0ll, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_map_two_keys(uint64_t list_val, const char* key1, const char* key2) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            const QoreHashNode* h = elem.get<const QoreHashNode>();
            QoreValue k = h->getKeyValue(key1);
            QoreValue val = h->getKeyValue(key2);
            QoreString key_str;
            if (k.getType() == NT_STRING) {
                QoreStringValueHelper sh(k);
                key_str.set(sh->c_str());
            } else {
                QoreStringValueHelper sh(k);
                key_str.set(sh->c_str());
            }
            result->setKeyValue(key_str.c_str(), val.refSelf(), nullptr);
        }
    }
    return toBits(result.release());
}

// Optimized select operations - native loops that filter lists
extern "C" DLLEXPORT uint64_t qore_rt_select_positive_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val > 0) {
            result->push(val, nullptr);
        }
    }
    return toBits(result.release());
}

static uint64_t qore_rt_select_hash_key_positive_int_impl(uint64_t list_val,
        const char* key, size_t key_hash, bool prehashed, ExceptionSink* xsink) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    const QoreTypeInfo* list_type = qore_list_private::get(*l)->complexTypeInfo;
    const QoreTypeInfo* element_type = QoreTypeInfo::getUniqueReturnComplexList(list_type);
    ReferenceHolder<QoreListNode> result(
        new QoreListNode(element_type ? element_type : autoTypeInfo), nullptr);
    size_t sz = l->size();
    for (size_t i = 0; i < sz; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink,
                "select hash-key positive-int loop")) {
            return toBits(QoreValue());
        }
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() != NT_HASH) {
            continue;
        }
        const QoreHashNode* hash = elem.get<const QoreHashNode>();
        QoreValue value = prehashed
            ? qore_hash_private::get(*hash)->getKeyValuePrehashed(key, key_hash, nullptr)
            : hash->getKeyValue(key);
        if (value.getAsBigInt() > 0) {
            result->push(elem.refSelf(), nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_select_hash_key_positive_int(uint64_t list_val,
        const char* key, ExceptionSink* xsink) {
    return qore_rt_select_hash_key_positive_int_impl(list_val, key, 0, false, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_select_hash_key_positive_int_prehashed(
        uint64_t list_val, const char* key, uint64_t hash64, uint32_t hash32,
        ExceptionSink* xsink) {
    return qore_rt_select_hash_key_positive_int_impl(list_val, key,
        qore_rt_select_precomputed_hash(hash64, hash32), true, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_select_positive_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val > 0.0) {
            result->push(val, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_select_nonzero_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val != 0) {
            result->push(val, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_select_nonzero_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val != 0.0) {
            result->push(val, nullptr);
        }
    }
    return toBits(result.release());
}

// Fused map+select operations - filter positive then transform in single pass
extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_scale_positive_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val > 0) {
            result->push(val * scale, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_scale_positive_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val > 0.0) {
            result->push(val * scale, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_offset_positive_int(uint64_t list_val, int64_t offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val > 0) {
            result->push(val + offset, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_offset_positive_float(uint64_t list_val, double offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val > 0.0) {
            result->push(val + offset, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_square_positive_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val > 0) {
            result->push(val * val, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_square_positive_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val > 0.0) {
            result->push(val * val, nullptr);
        }
    }
    return toBits(result.release());
}

// Fused map+foldl operations - map and reduce in single pass, no intermediate list
// Pattern: foldl $1 + $2, (map $1 * c, list) -> sum(list[i] * c)
extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_sum_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    int64_t result = 0;
    for (size_t i = 0; i < sz; ++i) {
        result += l->retrieveEntry(i).getAsBigInt() * scale;
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_sum_scale_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    double result = 0.0;
    for (size_t i = 0; i < sz; ++i) {
        result += l->retrieveEntry(i).getAsFloat() * scale;
    }
    return toBits(result);
}

// Pattern: foldl $1 + $2, (map $1 * $1, list) -> sum(list[i]^2)
extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_sum_square_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    int64_t result = 0;
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        result += val * val;
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_sum_square_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    double result = 0.0;
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        result += val * val;
    }
    return toBits(result);
}

// Pattern: foldl $1 * $2, (map $1 * c, list) -> prod(list[i] * c)
extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_prod_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    int64_t result = 1;
    for (size_t i = 0; i < sz; ++i) {
        result *= l->retrieveEntry(i).getAsBigInt() * scale;
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_prod_scale_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    double result = 1.0;
    for (size_t i = 0; i < sz; ++i) {
        result *= l->retrieveEntry(i).getAsFloat() * scale;
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_string_concat(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    if (lv.getType() == NT_STRING && rv.getType() == NT_STRING) {
        QoreStringNodeValueHelper ls(lv);
        QoreStringNodeValueHelper rs(rv);
        QoreStringNode* result = new QoreStringNode(**ls);
        result->concat(*rs, xsink);
        if (xsink && *xsink) {
            result->deref();
            return toBits(QoreValue());
        }
        return toBits(QoreValue(result));
    }
    // Not both strings: fall back to generic add
    return qore_rt_add_any(left, right, xsink);
}

// Typed string concatenation - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_add_typed(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    bool l_is_string = lv.getType() == NT_STRING;
    bool r_is_string = rv.getType() == NT_STRING;
    // Caller guarantees both are strings, but handle NOTHING gracefully.
    if (!l_is_string && !r_is_string) {
        return toBits(QoreValue());  // Both NOTHING
    }
    if (!l_is_string) {
        QoreStringNodeValueHelper rs(rv);
        return toBits(QoreValue(rs.getReferencedValue()));  // Copy right
    }
    if (!r_is_string) {
        QoreStringNodeValueHelper ls(lv);
        return toBits(QoreValue(ls.getReferencedValue()));  // Copy left
    }
    // Both are strings - concatenate
    QoreStringNodeValueHelper ls(lv);
    QoreStringNodeValueHelper rs(rv);
    QoreStringNode* result = new QoreStringNode(**ls);
    result->concat(*rs, xsink);
    if (xsink && *xsink) {
        result->deref();
        return toBits(QoreValue());
    }
    return toBits(QoreValue(result));
}

// Multi-string concatenation - concatenates N strings in a single pass
// This is more efficient than chaining AddString operations for a + b + c + d patterns
extern "C" DLLEXPORT uint64_t qore_rt_string_concat_multi(uint64_t* args, int nargs, ExceptionSink* xsink) {
    if (nargs == 0) {
        return toBits(QoreValue::makeStringValue(""));
    }

    // First pass: calculate total length and find first non-NOTHING string for encoding
    size_t total_len = 0;
    const QoreEncoding* enc = QCS_DEFAULT;
    for (int i = 0; i < nargs; ++i) {
        QoreValue v = fromBits(args[i]);
        if (v.getType() == NT_STRING) {
            QoreStringValueHelper s(v);
            total_len += s->size();
            if (i == 0) {
                enc = s->getEncoding();
            }
        }
    }

    // Second pass: build the result string
    QoreStringNode* result = new QoreStringNode(enc);
    result->reserve(total_len);

    for (int i = 0; i < nargs; ++i) {
        QoreValue v = fromBits(args[i]);
        if (v.getType() == NT_STRING) {
            QoreStringValueHelper s(v);
            result->concat(*s, xsink);
            if (xsink && *xsink) {
                result->deref();
                return toBits(QoreValue());
            }
        }
        // NOTHING values are skipped (treated as empty string)
    }

    return toBits(QoreValue(result));
}

// Typed string equality - both operands are known to be strings at compile time
// Uses equalSoft() for encoding-aware comparison (e.g. UTF-8 vs ISO-8859-1)
extern "C" DLLEXPORT uint64_t qore_rt_string_eq_typed(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    if (lv.getType() != NT_STRING || rv.getType() != NT_STRING) {
        return toBits(QoreValue(false));
    }
    QoreStringNodeValueHelper ls(lv);
    QoreStringNodeValueHelper rs(rv);
    bool result = ls->equalSoft(**rs, xsink);
    return toBits(QoreValue(result));
}

// Typed string inequality - both operands are known to be strings at compile time
// Uses equalSoft() for encoding-aware comparison (e.g. UTF-8 vs ISO-8859-1)
extern "C" DLLEXPORT uint64_t qore_rt_string_ne_typed(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    if (lv.getType() != NT_STRING || rv.getType() != NT_STRING) {
        return toBits(QoreValue(true));
    }
    QoreStringNodeValueHelper ls(lv);
    QoreStringNodeValueHelper rs(rv);
    bool result = !ls->equalSoft(**rs, xsink);
    return toBits(QoreValue(result));
}

// Typed string less than - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_lt_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    if (lv.getType() != NT_STRING || rv.getType() != NT_STRING) {
        return toBits(QoreValue(false));
    }
    QoreStringNodeValueHelper ls(lv);
    QoreStringNodeValueHelper rs(rv);
    // If either is null/NOTHING, result is false (consistent with Qore semantics)
    bool result = fast_string_compare(*ls, *rs) < 0;
    return toBits(QoreValue(result));
}

// Typed string less than or equal - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_le_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    if (lv.getType() != NT_STRING || rv.getType() != NT_STRING) {
        return toBits(QoreValue(false));
    }
    QoreStringNodeValueHelper ls(lv);
    QoreStringNodeValueHelper rs(rv);
    bool result = fast_string_compare(*ls, *rs) <= 0;
    return toBits(QoreValue(result));
}

// Typed string greater than - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_gt_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    if (lv.getType() != NT_STRING || rv.getType() != NT_STRING) {
        return toBits(QoreValue(false));
    }
    QoreStringNodeValueHelper ls(lv);
    QoreStringNodeValueHelper rs(rv);
    bool result = fast_string_compare(*ls, *rs) > 0;
    return toBits(QoreValue(result));
}

// Typed string greater than or equal - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_ge_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    if (lv.getType() != NT_STRING || rv.getType() != NT_STRING) {
        return toBits(QoreValue(false));
    }
    QoreStringNodeValueHelper ls(lv);
    QoreStringNodeValueHelper rs(rv);
    bool result = fast_string_compare(*ls, *rs) >= 0;
    return toBits(QoreValue(result));
}

// Typed string comparison (spaceship) - both operands are known to be strings at compile time
// Returns -1, 0, or 1 as an integer
extern "C" DLLEXPORT uint64_t qore_rt_string_cmp_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    // fast_string_compare already returns normalized -1/0/1
    int64_t result = 0;
    if (lv.getType() == NT_STRING && rv.getType() == NT_STRING) {
        QoreStringNodeValueHelper ls(lv);
        QoreStringNodeValueHelper rs(rv);
        result = fast_string_compare(*ls, *rs);
    }
    return toBits(QoreValue(result));
}

// String switch lookup - returns the case index or -1 for default
// case_strings is an array of C strings (null-terminated), num_cases is the count
extern "C" DLLEXPORT int32_t qore_rt_switch_string_lookup(uint64_t switch_val_bits, const char** case_strings,
        int32_t num_cases) {
    QoreValue switch_val = fromBits(switch_val_bits);
    if (switch_val.getType() != NT_STRING) {
        return -1;  // Not a string, go to default
    }
    QoreStringNodeValueHelper str(switch_val);
    for (int32_t i = 0; i < num_cases; ++i) {
        if (str->equal(case_strings[i])) {
            return i;
        }
    }
    return -1;  // No match, go to default
}

// --- DotEval with pre-evaluated base helper ---

#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QorePseudoMethods.h"

static bool qore_rt_dot_eval_preserve_raw_base(const QoreValue& base) {
    switch (base.getType()) {
        case NT_WEAKREF:
        case NT_WEAKREF_HASH:
        case NT_WEAKREF_LIST:
            return true;
        default:
            return false;
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_with_base(uint64_t expr_bits, uint64_t base_bits, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return toBits(QoreValue());
    }
    auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(expr.getInternalNode());
    if (!dot_eval) {
        // Fallback: shouldn't happen but be safe
        return qore_rt_invoke_expr(expr_bits, xsink);
    }
    QoreValue raw_base = fromBits(base_bits);
    ValueEvalOptimizedRefHolder base_holder(xsink);
    QoreValue base;
    if (qore_rt_dot_eval_preserve_raw_base(raw_base)) {
        base = raw_base;
    } else {
        base_holder.eval(raw_base);
        base = *base_holder;
    }
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    QoreValue result = dot_eval->evalWithBase(base, xsink);
    return toBits(result);
}

// --- Call with pre-evaluated args helper ---

static uint64_t qore_rt_call_with_args_impl(uint64_t expr_bits, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return toBits(QoreValue());
    }

    // Phase 5: Fast-call detection for builtins
    // Skip QoreListNode allocation for builtins with fast-call variants
    if (auto* call = dynamic_cast<const FunctionCallNode*>(expr.getInternalNode())) {
        const char* fname = call->getName();
        if (fname) {
            // Zero-argument fast-call functions (highest priority - no arg unpacking needed)
            if (nargs == 0) {
                if (!strcmp(fname, "now_us")) {
                    return qore_fast_now_us(xsink);
                } else if (!strcmp(fname, "now_ms")) {
                    return qore_fast_now_ms(xsink);
                } else if (!strcmp(fname, "now")) {
                    return qore_fast_now(xsink);
                } else if (!strcmp(fname, "time")) {
                    return qore_fast_time(xsink);
                }
            }
            // Single-argument fast-call functions
            else if (nargs == 1) {
                if (!strcmp(fname, "strlen")) {
                    return qore_fast_strlen(args[0], xsink);
                } else if (!strcmp(fname, "length")) {
                    return qore_fast_length(args[0], xsink);
                } else if (!strcmp(fname, "tolower") || !strcmp(fname, "lwr")) {
                    return qore_fast_tolower(args[0], xsink);
                } else if (!strcmp(fname, "toupper") || !strcmp(fname, "upr")) {
                    return qore_fast_toupper(args[0], xsink);
                } else if (!strcmp(fname, "trim")) {
                    return qore_fast_trim(args[0], xsink);
                } else if (!strcmp(fname, "abs")) {
                    return qore_fast_abs(args[0], xsink);
                } else if (!strcmp(fname, "first")) {
                    return qore_fast_first(args[0], xsink);
                } else if (!strcmp(fname, "last")) {
                    return qore_fast_last(args[0], xsink);
                }
            }
            // Two-argument fast-call functions
            else if (nargs == 2) {
                if (!strcmp(fname, "exists")) {
                    return qore_fast_hash_exists(args[0], args[1], xsink);
                }
            }
        }
    }

    // Phase 5.2c: Pseudo-method fast-call detection
    // Handle <type>::method() pseudo-method calls without QoreListNode allocation
    if (auto* method_call = dynamic_cast<const MethodCallNode*>(expr.getInternalNode())) {
        if (method_call->isPseudo()) {
            const char* mname = method_call->getName();
            if (mname && nargs == 1) {
                // Phase 5.2c: Pseudo-methods on built-in types (read-only, non-mutating)
                if (!strcmp(mname, "length") || !strcmp(mname, "size")) {
                    return qore_fast_any_size(args[0], xsink);
                } else if (!strcmp(mname, "keys")) {
                    return qore_fast_hash_keys(args[0], xsink);
                } else if (!strcmp(mname, "values")) {
                    return qore_fast_hash_values(args[0], xsink);
                }
            }
        }
    }

    // Build QoreListNode from the NaN-boxed args array
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> arg_list(new QoreListNode(autoTypeInfo), xsink);
    {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Determine call type and create a copy with the pre-built arg list
    bool used_operands = false;
    QoreValue result;

    if (auto* call = dynamic_cast<const FunctionCallNode*>(expr.getInternalNode())) {
        QoreValue call_expr(new FunctionCallNode(*call, arg_list.release()));
        ValueHolder call_holder(call_expr, nullptr);
        bool needs_deref = true;
        result = call_expr.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        used_operands = true;
    } else if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(expr.getInternalNode())) {
        QoreValue call_expr(new SelfFunctionCallNode(*call, arg_list.release()));
        ValueHolder call_holder(call_expr, nullptr);
        bool needs_deref = true;
        result = call_expr.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        used_operands = true;
    } else if (auto* call = dynamic_cast<const StaticMethodCallNode*>(expr.getInternalNode())) {
        QoreValue call_expr(new StaticMethodCallNode(*call, arg_list.release()));
        ValueHolder call_holder(call_expr, nullptr);
        bool needs_deref = true;
        result = call_expr.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        used_operands = true;
    } else if (auto* call = dynamic_cast<const CallReferenceCallNode*>(expr.getInternalNode())) {
        const ParseNode* parse_node = dynamic_cast<const ParseNode*>(expr.getInternalNode());
        const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
        QoreValue exp = call->getExp();
        if (exp.hasNode()) {
            exp = exp.refSelf();
        }
        QoreValue call_expr(new CallReferenceCallNode(loc, exp, arg_list.release()));
        ValueHolder call_holder(call_expr, nullptr);
        bool needs_deref = true;
        result = call_expr.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        used_operands = true;
    }

    if (!used_operands) {
        // Fall back to full AST re-evaluation for unrecognized call types
        return qore_rt_invoke_expr(expr_bits, xsink);
    }

    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_with_args(uint64_t expr_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    return qore_rt_call_with_args_impl(expr_bits, args, nullptr, nargs, xsink);
}

// --- Phase 5: Fast-call builtin variants (skip QoreListNode allocation) ---

extern "C" DLLEXPORT uint64_t qore_fast_strlen(uint64_t arg_bits, ExceptionSink* xsink) {
    QoreValue arg = fromBits(arg_bits);

    if (arg.isShortString()) {
        return toBits(static_cast<int64_t>(arg.shortStringLen()));
    }

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(0);
    }

    // Get string value (softstring auto-converts to string)
    const QoreStringNode* str = nullptr;
    if (auto* s = dynamic_cast<const QoreStringNode*>(arg.getInternalNode())) {
        str = s;
    } else {
        // For non-string types, try to convert via string conversion
        // This matches the behavior of the QPP strlen(softstring) function
        QoreString temp;
        int err = 0;
        arg.getInternalNode()->getAsString(temp, -1, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
        // For non-string-like values, return 0 (matches NOOP variant behavior)
        return toBits(0);
    }

    // Return string length as a 64-bit integer
    int64_t len = static_cast<int64_t>(str->strlen());
    return toBits(len);
}

// --- Phase 5.2a: Zero-argument fast-call functions (highest impact) ---

extern "C" DLLEXPORT uint64_t qore_fast_now_us(ExceptionSink* xsink) {
    // Returns current date/time with microsecond precision
    // Equivalent to: date now_us() { return DateTimeNode::makeNow(); }
    DateTimeNode* dt = DateTimeNode::makeNow();
    return toBits(dt);
}

extern "C" DLLEXPORT uint64_t qore_fast_now_ms(ExceptionSink* xsink) {
    // Returns current date/time with millisecond precision (same as now_us in practice)
    DateTimeNode* dt = DateTimeNode::makeNow();
    return toBits(dt);
}

extern "C" DLLEXPORT uint64_t qore_fast_now(ExceptionSink* xsink) {
    // Returns current date/time with second precision (same as now_us in practice)
    DateTimeNode* dt = DateTimeNode::makeNow();
    return toBits(dt);
}

extern "C" DLLEXPORT uint64_t qore_fast_time(ExceptionSink* xsink) {
    // Returns Unix timestamp as integer (seconds since epoch)
    // Equivalent to: int time() { return (int64)now_us()->getEpoch(); }
    DateTimeNode* dt = DateTimeNode::makeNow();
    int64_t timestamp = dt->getEpochSeconds();
    dt->deref(xsink);
    return toBits(timestamp);
}

// --- Phase 5.2b: Single-argument string functions ---

extern "C" DLLEXPORT uint64_t qore_fast_length(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns length of string or binary data
    // Equivalent to: int length(softstring str) { return str->length(); }
    QoreValue arg = fromBits(arg_bits);

    if (arg.isShortString()) {
        return toBits(static_cast<int64_t>(qore_short_string_utf8_length(arg)));
    }

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(0);
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle string
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        return toBits(static_cast<int64_t>(str->length()));
    }

    // Handle binary
    if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
        return toBits(static_cast<int64_t>(bin->size()));
    }

    // For other types, try string conversion length
    QoreString temp;
    int err = 0;
    node->getAsString(temp, -1, xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(static_cast<int64_t>(temp.length()));
}

extern "C" DLLEXPORT uint64_t qore_fast_tolower(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns lowercase version of string
    // Equivalent to: softstring tolower(softstring str) { return str->tolower(); }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle string directly
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        QoreStringNode* result = str->copy();
        result->tolwr();
        return toBits(result);
    }

    // For non-string types, convert to string first
    QoreStringNode* temp = new QoreStringNode;
    int err = 0;
    node->getAsString(*temp, -1, xsink);
    if (*xsink) {
        temp->deref(xsink);
        return toBits(QoreValue());
    }
    temp->tolwr();
    return toBits(temp);
}

extern "C" DLLEXPORT uint64_t qore_fast_toupper(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns uppercase version of string
    // Equivalent to: softstring toupper(softstring str) { return str->toupper(); }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle string directly
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        QoreStringNode* result = str->copy();
        result->toupr();
        return toBits(result);
    }

    // For non-string types, convert to string first
    QoreStringNode* temp = new QoreStringNode;
    int err = 0;
    node->getAsString(*temp, -1, xsink);
    if (*xsink) {
        temp->deref(xsink);
        return toBits(QoreValue());
    }
    temp->toupr();
    return toBits(temp);
}

// --- Phase 5.2c: Pseudo-method fast-calls ---

extern "C" DLLEXPORT uint64_t qore_fast_any_size(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns size of any collection (hash, list) or length of string
    // Equivalent to: int size(any val) { ... }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(0);
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle hash
    if (auto* hash = dynamic_cast<const QoreHashNode*>(node)) {
        return toBits(static_cast<int64_t>(hash->size()));
    }

    // Handle list
    if (auto* list = dynamic_cast<const QoreListNode*>(node)) {
        return toBits(static_cast<int64_t>(list->size()));
    }

    // Handle string
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        return toBits(static_cast<int64_t>(str->length()));
    }

    // Handle binary
    if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
        return toBits(static_cast<int64_t>(bin->size()));
    }

    // Other types have no size
    return toBits(0);
}

extern "C" DLLEXPORT uint64_t qore_fast_hash_keys(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns list of hash keys
    // Equivalent to: list<string> keys(hash<auto, auto> val) { ... }
    QoreValue arg = fromBits(arg_bits);

    ReferenceHolder<QoreListNode> keys(new QoreListNode(stringTypeInfo), xsink);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(keys.release());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle hash
    if (auto* hash = dynamic_cast<const QoreHashNode*>(node)) {
        // Iterate over all keys and add to result list
        ConstHashIterator hi(hash);
        while (hi.next()) {
            QoreStringNode* key = new QoreStringNode(hi.getKey());
            qore_list_private::get(*keys)->push(key, xsink);
        }
    }

    return toBits(keys.release());
}

extern "C" DLLEXPORT uint64_t qore_fast_hash_values(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns list of hash values
    // Equivalent to: list<auto> values(hash<auto, auto> val) { ... }
    QoreValue arg = fromBits(arg_bits);

    ReferenceHolder<QoreListNode> vals(new QoreListNode(autoTypeInfo), xsink);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(vals.release());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle hash
    if (auto* hash = dynamic_cast<const QoreHashNode*>(node)) {
        // Iterate over all values and add to result list
        ConstHashIterator hi(hash);
        while (hi.next()) {
            QoreValue v = hi.get();
            if (v.hasNode()) {
                v.refSelf();
            }
            qore_list_private::get(*vals)->push(v, xsink);
        }
    }

    return toBits(vals.release());
}

// --- Phase 5.3: Additional fast-path optimizations ---

extern "C" DLLEXPORT uint64_t qore_fast_trim(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns trimmed version of string (whitespace removed from both ends)
    // Equivalent to: softstring trim(softstring str) { return str->trim(); }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle string directly
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        QoreStringNode* result = str->copy();
        result->trim();
        return toBits(result);
    }

    // For non-string types, convert to string first
    QoreStringNode* temp = new QoreStringNode;
    int err = 0;
    node->getAsString(*temp, -1, xsink);
    if (*xsink) {
        temp->deref(xsink);
        return toBits(QoreValue());
    }
    temp->trim();
    return toBits(temp);
}

extern "C" DLLEXPORT uint64_t qore_fast_abs(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns absolute value of int or float
    // Equivalent to: number abs(number n) { return n < 0 ? -n : n; }
    QoreValue arg = fromBits(arg_bits);

    // Handle integers
    if (arg.getType() == QV_Int) {
        int64_t val = arg.getAsBigInt();
        return toBits(val < 0 ? -val : val);
    }

    // Handle floats
    if (arg.getType() == QV_Float) {
        double val = arg.getAsFloat();
        return toBits(val < 0.0 ? -val : val);
    }

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle number nodes
    if (auto* num = dynamic_cast<const QoreNumberNode*>(node)) {
        return toBits(qore_number_private::doUnary(*num, mpfr_abs));
    }

    // Fallback for other numeric types: try conversion to int then float
    // This matches the behavior of the abs() builtin
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_fast_first(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns first element of list
    // Equivalent to: any first(list<any> l) { return l.size() ? l[0] : nothing; }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle list
    if (auto* list = dynamic_cast<const QoreListNode*>(node)) {
        if (list->size() > 0) {
            QoreValue result = list->retrieveEntry(0);
            if (result.hasNode()) {
                result.refSelf();
            }
            return toBits(result);
        }
    }

    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_fast_last(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns last element of list
    // Equivalent to: any last(list<any> l) { return l.size() ? l[l.size()-1] : nothing; }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle list
    if (auto* list = dynamic_cast<const QoreListNode*>(node)) {
        size_t size = list->size();
        if (size > 0) {
            QoreValue result = list->retrieveEntry(size - 1);
            if (result.hasNode()) {
                result.refSelf();
            }
            return toBits(result);
        }
    }

    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_fast_hash_exists(uint64_t hash_bits, uint64_t key_bits, ExceptionSink* xsink) {
    // Returns true if hash key exists
    // Equivalent to: bool exists(hash<auto, auto> h, string key) { return h.exists(key); }
    QoreValue hash_val = fromBits(hash_bits);
    QoreValue key_val = fromBits(key_bits);

    // Handle null/nothing hash
    if (!hash_val.hasNode()) {
        return toBits(false);
    }

    const AbstractQoreNode* hash_node = hash_val.getInternalNode();

    // Handle hash
    if (auto* hash = dynamic_cast<const QoreHashNode*>(hash_node)) {
        // Convert key to string if needed
        QoreString key_str;
        if (key_val.hasNode()) {
            key_val.getInternalNode()->getAsString(key_str, -1, xsink);
        } else {
            key_str.concat(key_val.getAsBigInt());
        }
        if (*xsink) {
            return toBits(false);
        }
        return toBits(hash->existsKey(key_str.c_str()));
    }

    return toBits(false);
}

// --- Direct function call (resolved at parse time, skips AST round-trip) ---

static uint64_t qore_rt_call_function_direct_impl(const QoreFunction* func,
        const AbstractQoreFunctionVariant* variant, QoreProgram* pgm,
        uint64_t* args, uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink) {
    // A direct call must always carry a resolved function: a variant belongs to a function,
    // so a null func reaching here means the func pointer was lost upstream (a codegen/lowering
    // bug).  assert() catches it in debug builds; the runtime guard turns it into a clean error
    // instead of dereferencing null and crashing in release builds.
    assert(func && "direct function call must have a resolved function");
    if (!func) {
        clearConsumedArgCleanups(arg_cleanups, nargs, xsink);
        if (xsink) {
            xsink->raiseException("IR-EXECUTION-ERROR",
                "internal error: direct function call has a null function pointer "
                "(the resolved function was lost before the call)");
        }
        return toBits(QoreValue());
    }
    // Build QoreListNode from the NaN-boxed args array
    // Use pushIntern() to bypass checkVal/stripVal which strips complex types
    // (e.g., hash<string, bool> -> hash<auto>) from arguments in untyped lists,
    // breaking function overload resolution
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Get runtime config
    RuntimeConfig& rc = rc_get_current_ref();

    // Determine the program context
    QoreProgram* call_pgm = pgm ? pgm : rc.getProgram();
    if (!call_pgm) {
        call_pgm = getProgram();
    }

    // Call the function directly — skips dynamic_cast chain and AST node copy
    QoreValue result = func->evalFunctionTmpArgs(variant, *arg_list, call_pgm, rc, xsink);

    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_function_direct(const QoreFunction* func,
        const AbstractQoreFunctionVariant* variant, QoreProgram* pgm,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    return qore_rt_call_function_direct_impl(func, variant, pgm, args, nullptr,
            nargs, xsink);
}

//! Call a function with dynamic variant resolution (no pre-resolved variant)
/** Used when the variant could not be determined at AOT compile/load time
    (e.g., overloaded builtins like int() where the FunctionCallNode was
    reconstructed without args). Builds the arg list and uses evalDynamic()
    to resolve the correct variant at runtime.
*/
static uint64_t qore_rt_call_function_dynamic_impl(const QoreFunction* func,
        QoreProgram* pgm, uint64_t* args, uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink) {
    assert(func);

    // Build QoreListNode from the NaN-boxed args array
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Get runtime config
    RuntimeConfig& rc = rc_get_current_ref();

    // Resolve the variant dynamically while preserving temporary argument
    // ownership, so setupCall() can transfer references into parameters.
    QoreValue result = func->evalDynamicTmpArgs(*arg_list, pgm, rc, xsink);

    return toBits(result);
}

static uint64_t qore_rt_call_function_dynamic(const QoreFunction* func,
        QoreProgram* pgm, uint64_t* args, int nargs, ExceptionSink* xsink) {
    return qore_rt_call_function_dynamic_impl(func, pgm, args, nullptr, nargs,
            xsink);
}

// Stack location for JIT/AOT-executed frames.
class QoreJITStackLocation : public QoreStackLocation, public QoreProgramStackLocationHelper,
        public QoreAOTStackFrameMarker {
public:
    DLLLOCAL QoreJITStackLocation(const std::string& call_name, const QoreProgramLocation* loc,
            const StatementBlock* statements, QoreProgram* pgm, bool is_aot = false)
        : QoreProgramStackLocationHelper(this, saved_stmt, saved_pgm),
          call_name(call_name), loc(loc), statements(statements), pgm(pgm), is_aot(is_aot) {
        if (!this->pgm) {
            this->pgm = saved_pgm;
        }
    }

    DLLLOCAL const QoreProgramLocation& getLocation() const override {
        return loc ? *loc : loc_builtin;
    }

    DLLLOCAL bool isAOTFrame() const override {
        return is_aot;
    }

    DLLLOCAL const std::string& getCallName() const override {
        return call_name;
    }

    DLLLOCAL qore_call_t getCallType() const override {
        return CT_USER;
    }

    DLLLOCAL QoreProgram* getProgram() const override {
        return pgm;
    }

    DLLLOCAL const AbstractStatement* getStatement() const override {
        return statements;
    }

private:
    std::string call_name;
    const QoreProgramLocation* loc;
    const StatementBlock* statements;
    QoreProgram* pgm;
    bool is_aot = false;  //!< true when this frame executes an AOT-compiled function natively
    // saved_stmt and saved_pgm receive old thread-local values from
    // QoreProgramStackLocationHelper constructor via output references.
    // IMPORTANT: no default member initializers — they would overwrite the values
    // written by the base class constructor (same pattern as QoreInternalCallStackLocationHelperBase).
    const AbstractStatement* saved_stmt;
    QoreProgram* saved_pgm;
};

/** Handle body local instantiation before JIT execution and deopt/cleanup after.

    Before the JIT call: instantiates body locals unless all are IR-only.
    Pushes a stack location entry for the JIT frame.
    Calls the provided function to execute JIT code.
    After the call: checks for JIT guard failure, falls back to AST if needed,
    and uninstantiates body locals.

    @param uvb the user variant body (for statement block, program, and body locals)
    @param call_name the function/method name for the stack trace
    @param exec_fn callable that executes the JIT code and returns raw NaN-boxed result bits
    @param val reference to receive the final result value
    @param xsink exception sink
    @param caller_pgm the program context of the CALLER at the time of the call
           (before any ProgramThreadCountContextHelper pgm switch). Used as the
           stack frame's pgm so runtime_get_parse_options_stack() walks back to
           the caller's program, matching CEH semantics. If nullptr, falls back
           to uvb->pgm.
*/
static const QoreTypeInfo* qore_rt_get_effective_return_type(const UserSignature* sig);
static const QoreTypeInfo* qore_rt_get_effective_return_type(const UserSignature* sig,
        const QoreTypeInfo* receiver_type_info);

template <typename ExecFn>
static void execJITWithDeopt(const UserVariantBase* uvb, const std::string& call_name,
        ExecFn&& exec_fn, QoreValue& val, ExceptionSink* xsink,
        QoreProgram* caller_pgm = nullptr, const QoreTypeInfo* receiver_type_info = nullptr,
        QoreProgram* exec_pgm = nullptr) {
    QoreProgram* runtime_pgm = exec_pgm ? exec_pgm : uvb->pgm;
    // Get AST-visible body locals: for AOT use all_body_locals (separate optimization),
    // for IR use filtered ast_visible_body_locals (excludes IR-only locals that
    // are never accessed by AST callbacks).
    bool has_aot = uvb->hasCachedAOT();
    bool has_ir = uvb->getCachedIR() != nullptr;
    // AOT lowering syncs body locals that need ownership through the runtime
    // local stack; proven native scalars can remain alloca-only, but AOT frame
    // metadata does not encode a per-local instantiation subset. The stack slots
    // therefore still exist for every non-closure body local. The skip
    // optimization only applies to in-process JIT/IR functions.
    //
    // AST-only variants (no AOT and no IR cache) own their own LocalVarList via
    // the StatementBlock and instantiate it themselves on entry — pre-instantiating
    // here both has nothing to read from (getASTVisibleBodyLocals() requires
    // cached_ir to be non-null) and would shadow the statement block's own
    // instantiation.  Skip body_local pre-instantiation in that case.
    bool skip_body_locals = (!has_aot && !has_ir)
        || (!has_aot && uvb->areAllBodyLocalsIROnly());
    static const std::vector<LocalVar*> empty_body_locals;
    const std::vector<LocalVar*>& body_locals = has_aot
        ? uvb->getBodyLocals()  // AOT: use all_body_locals via getBodyLocals()
        : (has_ir ? uvb->getASTVisibleBodyLocals()  // IR: use filtered list
                  : empty_body_locals);            // AST-only: nothing to pre-inst here
    QoreParseOptions po = uvb->getParseOptions(runtime_pgm->getParseOptions());
    if (!skip_body_locals) {
        for (LocalVar* lv : body_locals) {
            // Closure-use vars must not be pre-instantiated here: doing so creates
            // empty CVVs in the current frame and shadows captured closure variables.
            if (lv->closureUse()) {
                continue;
            }
            lv->instantiate(po);
        }
    }

    // Push stack location for this JIT execution frame so it appears in
    // get_all_thread_call_stacks() and exception call stacks.
    // Use caller_pgm if supplied so that runtime_get_parse_options_stack() (used by
    // Program::getCallerCapabilityMask) walks back to the caller's program — matching
    // CEH semantics where the CEH frame captures current_pgm BEFORE any pgm switch.
    // Fast helpers capture caller_pgm before their ProgramThreadCountContextHelper
    // call; if not provided, we fall back to uvb->pgm (pre-existing behavior).
    const QoreProgramLocation* parse_loc = uvb->getUserSignature()->getParseLocation();
    const QoreProgramLocation* call_loc = get_runtime_location();
    // Mark the frame as native-AOT (has_aot) so the exception machinery can repair its
    // call-site location via the lazy PC->loc registry when the eager value is stale.
    QoreJITStackLocation jit_stack_loc(call_name, call_loc ? call_loc : parse_loc, uvb->getStatementBlock(),
        caller_pgm ? caller_pgm : runtime_pgm, has_aot);

    struct ReceiverTypeInfoGuard {
        const QoreTypeInfo* old = nullptr;
        bool restore = false;

        ReceiverTypeInfoGuard(const QoreTypeInfo* ti) {
            if (ti) {
                old = runtime_set_receiver_type_info(ti);
                restore = true;
            }
        }

        ~ReceiverTypeInfoGuard() {
            if (restore) {
                runtime_set_receiver_type_info(old);
            }
        }
    } receiver_guard(receiver_type_info);

    // Swap in the callee variant's parse options and set runtime_loc to the
    // function's parse location.  This is critical for correctness: when the
    // caller's program has different parse options than the callee's (e.g. the
    // caller is a restricted sandbox but the callee is a module method), the
    // callee must run with its OWN parse options so that runtime domain checks
    // on builtin calls (see Function.cpp runtimeFindVariant parse-options test)
    // use the callee module's rights rather than the caller's.
    // Mirrors Function.cpp UserVariantBase::evalTiered, which handles the same
    // swap for the evalTiered path.
    const AbstractStatement* old_stmt = nullptr;
    const QoreProgramLocation* old_loc = nullptr;
    QoreParseOptions old_po;
    bool swapped_runtime_ctx = false;
    if (parse_loc) {
        swap_runtime_statement_location(xsink, nullptr, parse_loc, po, old_stmt, old_loc, old_po);
        swapped_runtime_ctx = true;
    }

    // Save/restore the innermost-non-AOT-frame marker across this native (JIT) call so an
    // AOT caller that resumes after it sees its own outer marker, not this JIT frame's
    // stale inner one. See design/aot-lazy-loc-innermost-frame.md.
    struct SpGuard {
        uintptr_t saved_sp;
        ~SpGuard() { set_runtime_loc_sp(saved_sp); }
    } sp_guard{get_runtime_loc_sp()};

    bool fn_invalidated = false;
    uint64_t result_bits = 0;
    try {
        result_bits = exec_fn(xsink, fn_invalidated);
    } catch (const QoreJITException&) {
        // Native helpers throw this only after populating xsink.  Catch at the
        // fast-call boundary so the normal cleanup path below still runs for
        // self, args, body locals, runtime location, and gate state.
    }

    // Check for JIT guard failure or recompilation invalidation requesting deopt to AST
    if (!*xsink && (qore_jit_deopt_requested() || fn_invalidated)) {
        // Ensure body locals are on thread stack for AST execution
        if (skip_body_locals) {
            for (LocalVar* lv : body_locals) {
                lv->instantiate(po);
            }
        }
        StatementBlock* stmts = uvb->getStatementBlock();
        if (stmts) {
            // Set TLS returnTypeInfo to the callee's declared return type so that
            // ReturnStatement::execImpl() performs the correct type check.
            // Fast-call paths (qore_rt_call_fast et al.) bypass CodeEvaluationHelper,
            // so TLS returnTypeInfo may still hold the *caller*'s return type here.
            const QoreTypeInfo* old_rti = saveReturnTypeInfo(
                qore_rt_get_effective_return_type(uvb->getUserSignature(), receiver_type_info));
            val = stmts->exec(xsink);
            saveReturnTypeInfo(old_rti);
        }
        if (skip_body_locals) {
            for (int i = (int)body_locals.size() - 1; i >= 0; --i) {
                body_locals[i]->uninstantiate(xsink);
            }
        }
    } else {
        QoreValue result;
        std::memcpy(&result, &result_bits, sizeof(result));
        val = result;
    }

    // Restore thread-local runtime parse options/location saved above.
    if (swapped_runtime_ctx) {
        const AbstractStatement* dummy_stmt;
        const QoreProgramLocation* dummy_loc;
        QoreParseOptions dummy_po;
        swap_runtime_statement_location(xsink, old_stmt, old_loc, old_po, dummy_stmt, dummy_loc, dummy_po);
    }

    if (!skip_body_locals) {
        for (int i = (int)body_locals.size() - 1; i >= 0; --i) {
            if (body_locals[i]->closureUse()) {
                // Closure-use locals are managed by the LLVM code
                // (emitLocalInstantiation/emitLocalUninstantiation).  On normal
                // return the CVV is already popped.  On exception paths the LLVM
                // epilogue may be skipped, so safely pop any remaining CVV here.
                // Use the frame-aware lookup — a plain thread_try_find_closure_var()
                // walks the entire cvstack (skipping frame boundaries) and can
                // match a same-named CVV pushed by an outer frame. If we then call
                // uninstantiate() under that false positive, it pops the top of
                // the current frame — which may be a frame boundary — and trips
                // the `curr->var[curr->pos].cvv` assertion in
                // ThreadClosureVariableStack.h:152. The in-current-frame variant
                // stops at the boundary, so it only reports presence when the CVV
                // is actually on THIS call's frame.
                if (thread_try_find_closure_var_in_current_frame(body_locals[i]->getName())) {
                    body_locals[i]->uninstantiate(xsink);
                }
                continue;
            }
            body_locals[i]->uninstantiate(xsink);
        }
    }
}

static QoreProgram* qore_rt_method_execution_program(const QoreMethod* method, const UserVariantBase* uvb) {
    QoreProgram* pgm = nullptr;
    if (method) {
        const QoreClass* qc = method->getClass();
        if (qc) {
            pgm = qc->getSourceProgram();
            if (!pgm) {
                pgm = qc->getProgram();
            }
        }
    }
    return pgm ? pgm : uvb->pgm;
}

// --- Fast call parameter instantiation helper ---

static bool qore_rt_preserve_unevaluated_arg(QoreValue val) {
    switch (val.getType()) {
        case NT_REFERENCE:
        case NT_WEAKREF:
        case NT_WEAKREF_HASH:
        case NT_WEAKREF_LIST:
            return true;
        default:
            return false;
    }
}

//! Instantiate parameter locals from NaN-boxed args with default argument evaluation
/** \return 0 on success, -1 on error (already-instantiated params are cleaned up on error,
    but caller must handle selfid cleanup and return value)
*/
static int instantiateFastCallParams(const UserSignature* sig, unsigned num_params, int nargs,
        const uint64_t* args, ExceptionSink* xsink, const QoreTypeInfo* receiver_type_info = nullptr) {
    const arg_vec_t& defaultArgList = sig->getDefaultArgList();
    for (unsigned i = 0; i < num_params; ++i) {
        if (i < (unsigned)nargs) {
            QoreValue raw = fromBits(args[i]);
            QoreValue val;
            if (!raw.needsEval() || qore_rt_preserve_unevaluated_arg(raw)) {
                val = raw.refSelf();
            } else {
                ValueEvalOptimizedRefHolder eval_arg(raw, xsink);
                if (*xsink) {
                    // Uninstantiate already-instantiated params in reverse
                    for (int j = (int)i - 1; j >= 0; --j) {
                        sig->lv[j]->uninstantiate(xsink);
                    }
                    return -1;
                }
                val = eval_arg.getReferencedValue();
            }

            // Match CodeEvaluationHelper::prepareDefaultArgs() for actual args:
            // typed params must always run through acceptInputParam(), not just
            // when mayRequireFilter() predicts a mapping filter.  Reference
            // targets such as reference<softint> can coerce the referenced value
            // even though the argument itself is already an NT_REFERENCE.
            const QoreTypeInfo* paramTypeInfo = sig->getParamTypeInfo(i);
            if (receiver_type_info) {
                paramTypeInfo = qore_substitute_type_params_if_needed(paramTypeInfo, receiver_type_info);
            }
            if (paramTypeInfo && (QoreTypeInfo::hasType(paramTypeInfo)
                    || QoreTypeInfo::mayRequireFilter(paramTypeInfo, val))) {
                QoreTypeInfo::acceptInputParam(paramTypeInfo, i, sig->getName(i), val, xsink);
                if (*xsink) {
                    val.discard(xsink);
                    // Uninstantiate already-instantiated params in reverse
                    for (int j = (int)i - 1; j >= 0; --j) {
                        sig->lv[j]->uninstantiate(xsink);
                    }
                    return -1;
                }
            }

            sig->lv[i]->instantiate(val);
        } else if (i < defaultArgList.size() && defaultArgList[i]) {
            // Evaluate default argument expression
            QoreValue val = defaultArgList[i].eval(xsink);
            if (*xsink) {
                // Uninstantiate already-instantiated params in reverse
                for (int j = (int)i - 1; j >= 0; --j) {
                    sig->lv[j]->uninstantiate(xsink);
                }
                return -1;
            }

            // Apply type filter like the standard path in lib/Function.cpp:404-410
            const QoreTypeInfo* paramTypeInfo = sig->getParamTypeInfo(i);
            if (receiver_type_info) {
                paramTypeInfo = qore_substitute_type_params_if_needed(paramTypeInfo, receiver_type_info);
            }
            if (QoreTypeInfo::mayRequireFilter(paramTypeInfo, val)) {
                QoreTypeInfo::acceptInputParam(paramTypeInfo, i, sig->getName(i), val, xsink);
                if (*xsink) {
                    // Uninstantiate already-instantiated params in reverse
                    for (int j = (int)i - 1; j >= 0; --j) {
                        sig->lv[j]->uninstantiate(xsink);
                    }
                    return -1;
                }
            }

            sig->lv[i]->instantiate(val);
        } else {
            sig->lv[i]->instantiate(QoreValue());
        }
    }
    return 0;
}

static bool qore_rt_user_fast_call_eligible(const AbstractQoreFunctionVariant* variant) {
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    return uvb && uvb->isStaticallyFastCallEligible();
}

static bool qore_rt_method_fast_call_eligible(const AbstractQoreFunctionVariant* variant) {
    const auto* mvb = dynamic_cast<const MethodVariantBase*>(variant);
    return mvb
        ? mvb->isStaticallyFastMethodCallEligible()
        : qore_rt_user_fast_call_eligible(variant);
}

static const QoreTypeInfo* qore_rt_get_effective_return_type(const UserSignature* sig,
        const QoreTypeInfo* receiver_type_info) {
    const QoreTypeInfo* rt = sig->getReturnTypeInfo();
    return receiver_type_info ? qore_substitute_type_params_if_needed(rt, receiver_type_info) : rt;
}

static const QoreTypeInfo* qore_rt_get_effective_return_type(const UserSignature* sig) {
    return qore_substitute_type_params_if_needed(sig->getReturnTypeInfo());
}

// --- Fast function call (bypasses QoreListNode + CodeEvaluationHelper dispatch chain) ---

extern "C" DLLEXPORT uint64_t qore_rt_call_fast(const QoreFunction* func,
        const AbstractQoreFunctionVariant* variant, QoreProgram* pgm,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    assert(variant);

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        // Builtin variant — fall back to slow path for proper type coercion
        // (builtins can have soft types like softstring that require CodeEvaluationHelper)
        return qore_rt_call_function_direct(func, variant, pgm, args, nargs, xsink);
    }

    if (!uvb->isStaticallyFastCallEligible()) {
        return qore_rt_call_function_direct(func, variant, pgm, args, nargs, xsink);
    }

    // If the callee has neither JIT nor IR, fall back to the slow path.
    // This can happen in tiered compilation when the callee hasn't been promoted yet.
    if (!uvb->hasCachedFunction() && !uvb->getCachedIR()) {
        return qore_rt_call_function_direct(func, variant, pgm, args, nargs, xsink);
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Capture caller's program before ptcch switch, for QoreJITStackLocation.
    // See qore_rt_call_static_method_direct for the rationale.
    QoreProgram* caller_pgm = getProgram();
    QoreProgram* exec_pgm = pgm ? pgm : uvb->pgm;

    // Set up program thread context (only if program differs from caller's program)
    std::optional<ProgramThreadCountContextHelper> ptcch;
    if (exec_pgm != caller_pgm) {
        ptcch.emplace(xsink, exec_pgm, true);
        if (*xsink) {
            return toBits(QoreValue());
        }
    }
    // This is a normal function call, not a closure invocation.  Do not let a
    // caller closure's captured LocalVar* map shadow the callee's own closure-use
    // locals when the callee is entered through the fast runtime path.
    ThreadSafeLocalVarRuntimeEnvironmentHelper closure_env_clear(nullptr);

    // Push frame boundary so that get_local_vars()/set_local_var_value() can correctly
    // determine call-stack depth for debugger introspection (same as CodeEvaluationHelper
    // via UserVariantExecHelper::ThreadFrameBoundaryHelper in the AST path).
    ThreadFrameBoundaryHelper tfbh(true);

    // Check if callee IR supports direct param passing (bypass TLS entirely)
    const QoreIRFunction* ir = uvb->getCachedIR();
    bool use_direct_params = ir && ir->isDirectParamsRuntimeSafe()
        && !uvb->hasCachedFunction() && nargs >= (int)num_params;

    if (!use_direct_params) {
        // Standard path: push params to TLS
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            return toBits(QoreValue());
        }
    }
    // else: direct_params path — params pre-populated in IR slot cache

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable (if the function has an argv parameter)
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Use cached IR name when available (zero allocation); fall back to building the name
    std::string call_name_buf;
    if (!ir) {
        const char* class_name = func->className();
        if (class_name) {
            call_name_buf = class_name;
            call_name_buf += "::";
        }
        call_name_buf += func->getName();
    }
    const std::string& call_name = ir ? ir->getDisplayName() : call_name_buf;

    // Tiered promotion: this fast-call path bypasses evalTiered(), so account for
    // the execution here and promote the callee to native once it is hot (no-op
    // when already native / compilation failed).
    uvb->recordFastCallExecution();

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        if (use_direct_params) {
            // Direct params path: pass args straight to IR slot cache, no TLS
            IRDirectParams dp{args, nargs};
            execJITWithDeopt(uvb, call_name, [ir, uvb, exec_pgm, &dp](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), exec_pgm, false, &dp);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink, caller_pgm, nullptr, exec_pgm);
        } else if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink, caller_pgm, nullptr, exec_pgm);
        } else {
            // IR fast path (standard TLS): execute IR directly without QoreListNode.
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            execJITWithDeopt(uvb, call_name, [callee_ir, uvb, exec_pgm](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), exec_pgm);
                if (!ok && !*xs) {
                    inv = true;  // Request deopt to AST
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink, caller_pgm, nullptr, exec_pgm);
        }
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    if (!use_direct_params) {
        // Standard path: uninstantiate params from TLS
        for (int i = static_cast<int>(num_params) - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = qore_rt_get_effective_return_type(sig);
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            // Missing return statement: check type and set location to method definition
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

// --- Fast closure dispatch (bypasses QoreListNode + dynamic_cast for closures with cached IR/JIT) ---

//! Execute a closure directly from NaN-boxed args, bypassing QoreListNode construction
/** This is the fast path for closure calls in JIT/AOT/IR modes. It:
    1. Pushes captured variables onto cvstack (CVecInstantiator)
    2. Sets the closure runtime environment
    3. Instantiates params directly from NaN-boxed args
    4. Calls evalTiered/JIT/IR directly
    5. Cleans up in reverse order

    @param cb the closure base (QoreClosureNode or QoreObjectClosureNode)
    @param uvb the user variant base (already resolved, has cached IR or JIT)
    @param nargs number of NaN-boxed arguments
    @param args pointer to NaN-boxed argument array (may be nullptr if nargs == 0)
    @param xsink exception sink
    @return NaN-boxed result value
*/
static uint64_t execClosureDirect(const QoreClosureBase* cb, const UserVariantBase* uvb,
        int nargs, const uint64_t* args, ExceptionSink* xsink, uint64_t** arg_cleanups) {
    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Closures may be invoked on threads with no program context (e.g., AsyncIoController's
    // ioThread, ThreadPool workers). Ensure tlpd is set before CVecInstantiator runs —
    // otherwise thread_instantiate_closure_var() null-derefs td->tlpd at lib/thread.cpp:1218.
    // The conditional install (tlpd==null only) preserves DataProvider's child-program
    // module-init flow, which depends on lvstack continuity across closure invocations.
    ClosureTlpdEnsureHelper tlpd_helper(xsink, uvb->pgm);

    // Capture caller's program before ptcch switches to uvb->pgm.
    QoreProgram* caller_pgm = getProgram();

    // Program context
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }

    // Push captured vars onto the cvstack selected by the closure's program context.
    CVecInstantiator cvi(cb ? cb->getCvec() : nullptr, xsink);

    // Set closure runtime environment so closure-captured vars are findable.
    ThreadSafeLocalVarRuntimeEnvironmentHelper ch(cb);

    // Frame boundary for debugger/stack introspection
    ThreadFrameBoundaryHelper tfbh(true);

    // Handle self for object closures (closures defined inside methods)
    QoreObject* self = cb ? const_cast<QoreObject*>(cb->getObject()) : nullptr;
    std::optional<QoreClosureSelfContextHelper> csch;
    if (self) {
        csch.emplace(self);
    }

    // For object closures, establish the captured self in both TLS stores so that
    // implicit method calls (SelfFunctionCallNode) and LoadSelfMember resolve
    // against the closure's captured self, not the caller's context.
    // ObjectSubstitutionHelper updates BOTH td->current_obj AND tl_runtime_config.obj.
    // RuntimeConfigObjectHelper alone is insufficient because rc_get_current_ref()
    // reads td->current_obj to repopulate tl_runtime_config.obj on every call.
    std::optional<ObjectSubstitutionHelper> osh;
    if (self) {
        osh.emplace(self, cb->getClassCtx());
    }

    // Check if callee IR supports direct param passing (bypass TLS entirely)
    const QoreIRFunction* ir = uvb->getCachedIR();
    bool use_direct_params = ir && ir->isDirectParamsRuntimeSafe()
        && !uvb->hasCachedFunction() && nargs >= (int)num_params;

    LocalVar* selfid = sig->selfid ? sig->selfid : findIRSelfLocal(ir);
    bool selfid_instantiated = self && selfid;
    if (selfid_instantiated) {
        selfid->instantiateSelf(self);
    }

    if (!use_direct_params) {
        // Standard path: push params to TLS
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            if (selfid_instantiated) {
                selfid->uninstantiateSelf();
            }
            return toBits(QoreValue());
        }
    }
    // else: direct_params path — params are pre-populated in the IR slot cache.
    // selfid still lives in TLS because IR can load `self` explicitly.

    // Build argv for excess arguments (varargs)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }
    if (!use_direct_params && clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        if (sig->argvid) {
            sig->argvid->uninstantiate(xsink);
        }
        if (!use_direct_params) {
            for (int i = static_cast<int>(num_params) - 1; i >= 0; --i) {
                sig->lv[i]->uninstantiate(xsink);
            }
        }
        if (selfid_instantiated) {
            selfid->uninstantiateSelf();
        }
        return toBits(QoreValue());
    }

    // Get call name from cached IR if available, otherwise use static name
    static const std::string closure_name("<anonymous closure>");
    const std::string& call_name = ir ? ir->getDisplayName() : closure_name;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        if (use_direct_params) {
            // Direct params path: pass args straight to IR slot cache, no TLS
            IRDirectParams dp(args, nargs, arg_cleanups);
            execJITWithDeopt(uvb, call_name, [ir, uvb, &dp](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm, false, &dp);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink, caller_pgm);
        } else if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink, caller_pgm);
        } else {
            // IR fast path (standard TLS)
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            execJITWithDeopt(uvb, call_name, [callee_ir, uvb](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm);
                if (!ok && !*xs) {
                    inv = true;  // Request deopt to AST
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink, caller_pgm);
        }
    }

    // Cleanup in reverse order
    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }
    if (!use_direct_params) {
        // Standard path: uninstantiate params from TLS
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
    }
    if (selfid_instantiated) {
        selfid->uninstantiateSelf();
    }

    // Apply return type coercion
    if (!*xsink) {
        const QoreTypeInfo* rt = qore_rt_get_effective_return_type(sig);
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

static uint64_t qore_rt_call_immediate_closure_impl(const QoreClosureParseNode* cn,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    if (!cn) {
        xsink->raiseException("JIT-ERROR",
            "missing closure expression for immediate closure call");
        return toBits(QoreValue());
    }

    const LVarSet* vlist = cn->getVList();
    UserClosureFunction* uf = cn->getFunction();
    const AbstractQoreFunctionVariant* variant = uf ? uf->first() : nullptr;
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    if (!cn->isInMethod() && (!vlist || vlist->empty()) && uvb
            && uvb->isStaticallyFastCallEligible()
            && (uvb->hasCachedFunction() || uvb->getCachedIR())) {
        uint64_t leaf_result;
        if (!uvb->hasCachedAOT()
                && tryExecClosureNativeLeaf(nullptr, uvb, args, nargs,
                    arg_cleanups, leaf_result)) {
            return leaf_result;
        }
        return execClosureDirect(nullptr, uvb, nargs, args, xsink, arg_cleanups);
    }

    uint64_t closure_bits = qore_rt_create_closure(cn, xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    uint64_t result = qore_rt_call_closure_fast_impl(closure_bits, args,
        arg_cleanups, nargs, xsink);
    fromBits(closure_bits).discard(xsink);
    return result;
}

extern "C" DLLEXPORT uint64_t qore_rt_call_immediate_closure(
        const QoreClosureParseNode* cn, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    return qore_rt_call_immediate_closure_impl(cn, args, nullptr, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_immediate_closure_consume_args(
        const QoreClosureParseNode* cn, uint64_t* args, uint64_t** arg_cleanups,
        int nargs, ExceptionSink* xsink) {
    return qore_rt_call_immediate_closure_impl(cn, args, arg_cleanups, nargs, xsink);
}

static const QoreClosureParseNode* qore_rt_get_closure_expr_aot(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    const QoreClosureParseNode* cn =
        dynamic_cast<const QoreClosureParseNode*>(expr.getInternalNode());
    if (!cn) {
        xsink->raiseException("AOT-ERROR",
            "invalid expression for immediate closure AOT call");
    }
    return cn;
}

extern "C" DLLEXPORT uint64_t qore_rt_call_immediate_closure_aot(
        QoreAOTContext* ctx, int32_t idx, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    const QoreClosureParseNode* cn = qore_rt_get_closure_expr_aot(ctx, idx, xsink);
    return cn ? qore_rt_call_immediate_closure_impl(cn, args, nullptr, nargs, xsink)
              : toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_call_immediate_closure_aot_consume_args(
        QoreAOTContext* ctx, int32_t idx, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    const QoreClosureParseNode* cn = qore_rt_get_closure_expr_aot(ctx, idx, xsink);
    return cn ? qore_rt_call_immediate_closure_impl(cn, args, arg_cleanups,
                    nargs, xsink)
              : toBits(QoreValue());
}

// --- Fast function call with explicit target (multi-function module compilation) ---

extern "C" DLLEXPORT uint64_t qore_rt_call_fast_with_target(uint64_t (*target_fn)(ExceptionSink*),
        const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(variant);
    assert(target_fn);

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        // Should not happen for batch-compiled callees, but handle gracefully
        xsink->raiseException("JIT-ERROR", "non-user variant in fast call with target");
        return toBits(QoreValue());
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Capture caller's program before ptcch switch.
    QoreProgram* caller_pgm = getProgram();
    QoreProgram* exec_pgm = uvb->pgm;

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, exec_pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }

    // Instantiate parameter locals directly from NaN-boxed args
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable (if the function has an argv parameter)
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Get call name from cached IR function (always available for JIT-compiled functions)
    const QoreIRFunction* ir = uvb->getCachedIR();
    const std::string& call_name = ir ? ir->getDisplayName() : jit_empty_call_name;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        execJITWithDeopt(uvb, call_name, [target_fn](ExceptionSink* xs, bool& /*inv*/) {
            return target_fn(xs);
        }, val, xsink, caller_pgm, nullptr, exec_pgm);
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    // Uninstantiate parameter locals in reverse order
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = qore_rt_get_effective_return_type(sig);
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_self_recursive(const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    // Self-recursive call helper: sets up params, argv, body locals, and deopt handling.
    // Falls back to evalTiered slow path if JIT function was invalidated by recompilation.
    assert(variant);

    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        xsink->raiseException("JIT-ERROR", "non-user variant in self-recursive call");
        return toBits(QoreValue());
    }

    // Fall back to slow path if JIT function was invalidated by recompilation
    if (!uvb->hasCachedFunction()) {
        // Build QoreListNode from NaN-boxed args and call through evalTiered
        ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
        if (nargs > 0) {
            qore_list_private* priv = qore_list_private::get(**arg_list);
            priv->reserve(nargs);
            for (int i = 0; i < nargs; ++i) {
                QoreValue val = fromBits(args[i]);
                if (val.hasNode()) {
                    val.refSelf();
                }
                priv->pushIntern(val);
            }
        }
        const QoreIRFunction* ir = uvb->getCachedIR();
        const std::string& call_name = ir ? ir->getDisplayName() : jit_empty_call_name;
        QoreValue result = uvb->callTieredPublic(call_name.c_str(), arg_list, nullptr, xsink);
        return toBits(result);
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Capture caller's program before ptcch (for self-recursive calls the caller
    // and callee programs are the same, but keep the pattern consistent).
    QoreProgram* caller_pgm = getProgram();

    // Set up program thread context (program is already correct from parent call)
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }

    // Instantiate parameter locals directly from NaN-boxed args
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Build and instantiate argv for excess arguments (varargs)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Use cached IR name when available (zero allocation)
    const QoreIRFunction* ir = uvb->getCachedIR();
    const std::string& call_name = ir ? ir->getDisplayName() : jit_empty_call_name;

    // Call through execJITWithDeopt which handles body locals, stack location, and deopt
    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
            return uvb->execCachedFunction(xs, inv);
        }, val, xsink, caller_pgm);
    }

    // Uninstantiate argv + params in reverse order (LIFO)
    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // Apply return type coercion to match ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = qore_rt_get_effective_return_type(sig);
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

// --- Direct method call for devirtualized calls (final classes) ---

static uint64_t qore_rt_call_method_direct_impl(const QoreMethod* method,
        uint64_t* args, uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink) {
    if (!method) {
        xsink->raiseException("JIT-ERROR", "null method pointer in direct call");
        return toBits(QoreValue());
    }

    // Get the current self object from the runtime stack
    QoreObject* self = runtime_get_stack_object();
    if (!self) {
        xsink->raiseException("JIT-ERROR", "no self object in direct method call");
        return toBits(QoreValue());
    }

    // Build QoreListNode from the NaN-boxed args array
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Get runtime config
    RuntimeConfig& rc = rc_get_current_ref();

    if (qore_method_private::get(*method)->isAbstract()) {
        const qore_class_private* runtime_cls = rc.getClass() ? rc.getClass() : runtime_get_class();
        return toBits(qore_class_private::get(*self->getClass())->evalMethod(
            self, method->getName(), *arg_list, runtime_cls, rc, xsink));
    }

    // Call the method using evalTmpArgs to preserve reference nodes in the arg list.
    // The eval() path goes through CodeEvaluationHelper with const args which calls
    // evalList() and dereferences ReferenceNode values.
    ClassOnlySubstitutionHelper cosh(qore_class_private::get(*method->getClass()));
    QoreValue result = qore_method_private::evalTmpArgs(*method, xsink, rc, self, *arg_list);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_method_direct(const QoreMethod* method, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    return qore_rt_call_method_direct_impl(method, args, nullptr, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_method_direct_consume_args(const QoreMethod* method,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    return qore_rt_call_method_direct_impl(method, args, arg_cleanups, nargs, xsink);
}

static QoreValue qore_rt_eval_self_method_by_name(QoreObject* self, const char* name,
        QoreListNode* arg_list, const qore_class_private* class_ctx, RuntimeConfig& rc,
        ExceptionSink* xsink) {
    return qore_class_private::get(*self->getClass())->evalMethod(self, name, arg_list, class_ctx, rc, xsink);
}

static uint64_t qore_rt_call_self_method_dispatch_impl(const QoreAOTCallTarget& target,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    const QoreMethod* method = target.method;
    if (!method) {
        xsink->raiseException("JIT-ERROR", "null method pointer in self method dispatch");
        return toBits(QoreValue());
    }

    RuntimeConfig& rc = rc_get_current_ref();
    QoreObject* self = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    if (!self) {
        xsink->raiseException("JIT-ERROR", "no self object in self method dispatch");
        return toBits(QoreValue());
    }

    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        return toBits(QoreValue());
    }

    const qore_class_private* runtime_cls = rc.getClass() ? rc.getClass() : runtime_get_class();
    const qore_class_private* call_ctx = target.class_ctx ? target.class_ctx : runtime_cls;
    const char* method_name = target.method_name ? target.method_name : method->getName();

    if (target.self_is_copy) {
        if (nargs != 0) {
            xsink->raiseException("JIT-ERROR", "copy() self call expects no arguments, got %d", nargs);
            return toBits(QoreValue());
        }
        return toBits(self->getClass()->execCopy(self, xsink));
    }

    if (target.self_ns_single) {
        // Match SelfFunctionCallNode::evalImpl(): unqualified self calls are virtual,
        // except for inherited multi-variant methods where a derived class may hide
        // only some variants. In that case try a matching override first, then fall
        // back to the parse-time method pointer to preserve inherited overload access.
        if (!target.self_is_abstract && self->getClass() != method->getClass()
                && qore_method_private::get(*method)->getFunction()->numVariants() > 1) {
            const qore_class_private* obj_priv = qore_class_private::get(*self->getClass());
            const QoreMethod* derived = obj_priv->getMethodForEval(method_name, self->getProgram(),
                call_ctx, xsink);
            if (*xsink) {
                return toBits(QoreValue());
            }
            if (derived && derived != method) {
                if (target.variant) {
                    const AbstractFunctionSignature* sig = target.variant->getSignature();
                    if (sig) {
                        unsigned np = sig->numParams();
                        const QoreFunction* dfunc = qore_method_private::get(*derived)->getFunction();
                        QoreFunctionIterator it(*dfunc);
                        while (it.next()) {
                            const AbstractQoreFunctionVariant* dv = it.getVariant();
                            const AbstractFunctionSignature* dsig = dv->getSignature();
                            if (!dsig || dsig->numParams() != np) {
                                continue;
                            }
                            bool match = true;
                            for (unsigned i = 0; i < np; ++i) {
                                if (!QoreTypeInfo::isInputIdentical(sig->getParamTypeInfo(i),
                                        dsig->getParamTypeInfo(i))) {
                                    match = false;
                                    break;
                                }
                            }
                            if (match) {
                                return toBits(qore_method_private::evalTmpArgs(*derived, xsink, rc,
                                    self, *arg_list, call_ctx, dv));
                            }
                        }
                    }
                } else {
                    return toBits(qore_rt_eval_self_method_by_name(self, method_name, *arg_list,
                        call_ctx, rc, xsink));
                }
            }
            return toBits(qore_method_private::evalTmpArgs(*method, xsink, rc, self, *arg_list, nullptr,
                target.variant));
        }

        if (target.qc && method && (self->getClass() == target.qc || self->getClass() == method->getClass())) {
            return toBits(qore_method_private::evalTmpArgs(*method, xsink, rc, self, *arg_list, call_ctx,
                target.variant));
        }
        return toBits(qore_rt_eval_self_method_by_name(self, method_name, *arg_list, call_ctx, rc, xsink));
    }

    if (target.self_is_abstract) {
        return toBits(qore_rt_eval_self_method_by_name(self, method_name, *arg_list, call_ctx, rc, xsink));
    }

    // Explicit base/namespace-qualified self calls are deliberately non-virtual.
    return toBits(qore_method_private::evalTmpArgs(*method, xsink, rc, self, *arg_list, nullptr, target.variant));
}

// --- Fast method call (bypasses QoreListNode + dispatch chain for devirtualized calls) ---

static uint64_t qore_rt_call_method_fast_impl(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args, uint64_t** arg_cleanups,
        int nargs, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    assert(method);
    assert(variant);

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        // Builtin method — fall back to slow path for proper type coercion
        // (builtins can have soft types like softstring that require CodeEvaluationHelper)
        return qore_rt_call_method_direct_impl(method, args, arg_cleanups, nargs, xsink);
    }

    if (!qore_rt_method_fast_call_eligible(variant)) {
        return qore_rt_call_method_direct_impl(method, args, arg_cleanups, nargs, xsink);
    }

    // If the callee has neither JIT nor IR, fall back to the slow path.
    // This can happen in tiered compilation when the callee hasn't been promoted yet.
    if (!uvb->hasCachedFunction() && !uvb->getCachedIR()) {
        return qore_rt_call_method_direct_impl(method, args, arg_cleanups, nargs, xsink);
    }

    // Get the current self object from the runtime stack
    QoreObject* self = runtime_get_stack_object();
    if (!self) {
        xsink->raiseException("JIT-ERROR", "no self object in fast method call");
        return toBits(QoreValue());
    }
    const QoreTypeInfo* receiver_type_info = qore_get_object_receiver_type_info(self);
    if (receiver_type_info && QoreTypeInfo::getParameterizedClassType(receiver_type_info)) {
        return qore_rt_call_method_direct_impl(method, args, arg_cleanups, nargs, xsink);
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Capture caller's program before ptcch switch.
    QoreProgram* caller_pgm = getProgram();
    QoreProgram* exec_pgm = qore_rt_method_execution_program(method, uvb);

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, exec_pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }
    // Push frame boundary so that get_local_vars()/set_local_var_value() can correctly
    // determine call-stack depth for debugger introspection.
    ThreadFrameBoundaryHelper tfbh(true);

    // Push self object onto the method call stack (for runtime_get_stack_object()).
    // Use the variant's class as the runtime class context (not method->getClass()):
    // for a method injected/synthesized into a derived class to satisfy an abstract
    // sibling slot, method->getClass() is the derived class, but the variant's body
    // resolves private:internal members against the class where the variant is
    // defined.  This mirrors UserMethodVariant::evalMethod()'s use of getClassPriv().
    const qore_class_private* method_ctx = variant
        ? METHV_const(variant)->getClassPriv() : qore_class_private::get(*method->getClass());
    ObjectSubstitutionHelper osh(self, method_ctx);

    // Check if callee IR supports direct param passing (bypass TLS entirely)
    const QoreIRFunction* ir = uvb->getCachedIR();
    bool use_direct_params = ir && ir->isDirectParamsRuntimeSafe()
        && !uvb->hasCachedFunction() && nargs >= (int)num_params;

    LocalVar* selfid = sig->selfid ? sig->selfid : findIRSelfLocal(ir);
    bool selfid_instantiated = selfid;
    if (selfid_instantiated) {
        selfid->instantiateSelf(self);
    }

    if (!use_direct_params) {
        // Standard path: push params to TLS
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            if (selfid_instantiated) {
                selfid->uninstantiateSelf();
            }
            return toBits(QoreValue());
        }
    }
    // else: direct_params path — params are pre-populated in the IR slot cache.
    // selfid still lives in TLS because IR can load `self` explicitly.

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable (if the function has an argv parameter)
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Use cached IR name when available (zero allocation); fall back to building the name
    std::string call_name_buf;
    if (!ir) {
        const char* cls = method->getClass() ? method->getClass()->getName() : nullptr;
        if (cls) {
            call_name_buf = cls;
            call_name_buf += "::";
        }
        call_name_buf += method->getName();
    }
    const std::string& call_name = ir ? ir->getDisplayName() : call_name_buf;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        if (use_direct_params) {
            // Direct params path: pass args straight to IR slot cache, no TLS
            IRDirectParams dp{args, nargs};
            execJITWithDeopt(uvb, call_name, [ir, uvb, exec_pgm, &dp](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), exec_pgm, false, &dp);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink, caller_pgm, receiver_type_info, exec_pgm);
        } else if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink, caller_pgm, receiver_type_info, exec_pgm);
        } else {
            // IR fast path (standard TLS): execute IR directly without QoreListNode.
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            execJITWithDeopt(uvb, call_name, [callee_ir, uvb, exec_pgm](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), exec_pgm);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink, caller_pgm, receiver_type_info, exec_pgm);
        }
    }

    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        if (sig->argvid) {
            sig->argvid->uninstantiate(xsink);
        }
        if (!use_direct_params) {
            for (int i = static_cast<int>(num_params) - 1; i >= 0; --i) {
                sig->lv[i]->uninstantiate(xsink);
            }
        }
        if (selfid_instantiated) {
            selfid->uninstantiateSelf();
        }
        return toBits(QoreValue());
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    if (!use_direct_params) {
        // Standard path: uninstantiate params from TLS
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
    }
    if (selfid_instantiated) {
        selfid->uninstantiateSelf();
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = qore_rt_get_effective_return_type(sig, receiver_type_info);
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            // Missing return statement: set location to method definition
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_method_fast(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink) {
    return qore_rt_call_method_fast_impl(method, variant, args, nullptr, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_method_fast_consume_args(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args, uint64_t** arg_cleanups,
        int nargs, ExceptionSink* xsink) {
    return qore_rt_call_method_fast_impl(method, variant, args, arg_cleanups, nargs, xsink);
}

// --- Fast call reference/closure call ---

extern "C" DLLEXPORT uint64_t qore_rt_call_ref_fast(uint64_t ref_bits, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    QoreValue ref_val = fromBits(ref_bits);
    if (!ref_val.hasNode()) {
        xsink->raiseException("JIT-ERROR", "call reference value is not a node");
        return toBits(QoreValue());
    }

    // The call reference is a ResolvedCallReferenceNode (closure, function ref, method ref, etc.)
    auto* ref_node = dynamic_cast<ResolvedCallReferenceNode*>(ref_val.getInternalNode());
    if (!ref_node) {
        xsink->raiseException("JIT-ERROR", "call reference is not a ResolvedCallReferenceNode");
        return toBits(QoreValue());
    }

    // Build QoreListNode from the NaN-boxed args array
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }

    // Call execValue() directly — avoids the dynamic_cast chain and AST node copy
    // that qore_rt_call_with_args() performs
    QoreValue result = ref_node->execValue(*arg_list, xsink);
    return toBits(result);
}

// --- On-block-exit handler support for JIT ---

struct JITOnBlockExitHandler {
    obe_type_e type;
    StatementBlock* code;
    const QoreIRFunction* handler_ir = nullptr;  //!< IR handler for IR interpreter execution
    uint64_t (*compiled_fn)(ExceptionSink*) = nullptr;  //!< native compiled handler function
    const QoreIRFunction* handler_func = nullptr;  //!< IR function for pre-instantiation of handler locals
};

static thread_local std::vector<JITOnBlockExitHandler> jit_obe_handlers;

// Thread-local pointer to the innermost IR interpreter's slot cache
// Set/restored by QoreIRInterpreter::execute() to allow qore_rt_exec_on_block_exit_impl
// to pass parent_slot_cache when executing handler IR functions (Phase 2, Fix 2a)
static thread_local std::vector<QoreValue>* current_ir_slot_cache = nullptr;
static thread_local std::vector<bool>* current_ir_slot_cache_dirty = nullptr;
static thread_local bool current_native_ir_slot_cache_active = false;

struct NativeIRSlotCache {
    std::vector<QoreValue> slots;
    std::vector<bool> dirty;
    std::vector<LocalVar*> slot_to_lvar;
    std::vector<QoreValue>* prev_slot_cache = nullptr;
    std::vector<bool>* prev_slot_cache_dirty = nullptr;
    bool prev_native_slot_cache_active = false;

    explicit NativeIRSlotCache(int32_t count)
            : slots(count > 0 ? static_cast<size_t>(count) : 0),
              dirty(count > 0 ? static_cast<size_t>(count) : 0, false),
              slot_to_lvar(count > 0 ? static_cast<size_t>(count) : 0, nullptr) {
    }

    ~NativeIRSlotCache() {
        ExceptionSink xsink;
        for (QoreValue& v : slots) {
            v.discard(&xsink);
            v = QoreValue();
        }
        if (xsink) {
            xsink.clear();
        }
    }
};

// Accessor exported for IR interpreter (returns void** for C ABI compatibility)
extern "C" DLLEXPORT void** qore_rt_get_ir_slot_cache_ptr() {
    return reinterpret_cast<void**>(&current_ir_slot_cache);
}

extern "C" DLLEXPORT void** qore_rt_get_ir_slot_cache_dirty_ptr() {
    return reinterpret_cast<void**>(&current_ir_slot_cache_dirty);
}

extern "C" DLLEXPORT void* qore_rt_begin_native_ir_slot_cache(int32_t count) {
    std::unique_ptr<NativeIRSlotCache> guard(new NativeIRSlotCache(count));
    guard->prev_slot_cache = current_ir_slot_cache;
    guard->prev_slot_cache_dirty = current_ir_slot_cache_dirty;
    guard->prev_native_slot_cache_active = current_native_ir_slot_cache_active;
    current_ir_slot_cache = &guard->slots;
    current_ir_slot_cache_dirty = &guard->dirty;
    current_native_ir_slot_cache_active = true;
    return guard.release();
}

extern "C" DLLEXPORT void qore_rt_set_native_ir_slot_cache_value(void* guard_ptr, int32_t ir_slot,
        LocalVar* var, uint64_t value) {
    NativeIRSlotCache* guard = static_cast<NativeIRSlotCache*>(guard_ptr);
    if (!guard || ir_slot < 0 || static_cast<size_t>(ir_slot) >= guard->slots.size()) {
        return;
    }

    QoreValue& slot = guard->slots[ir_slot];
    ExceptionSink xsink;
    slot.discard(&xsink);
    if (xsink) {
        xsink.clear();
    }

    QoreValue qv = fromBits(value);
    slot = qv.hasNode() ? qv.refSelf() : qv;
    guard->slot_to_lvar[ir_slot] = var;
}

extern "C" DLLEXPORT void qore_rt_set_native_ir_slot_cache_value_aot(QoreAOTContext* ctx, void* guard,
        int32_t ir_slot, int32_t local_slot, uint64_t value) {
    assert(ctx && local_slot >= 0 && local_slot < ctx->num_locals);
    qore_rt_set_native_ir_slot_cache_value(guard, ir_slot, ctx->locals[local_slot], value);
}

extern "C" DLLEXPORT void qore_rt_end_native_ir_slot_cache(void* guard_ptr, ExceptionSink* xsink) {
    NativeIRSlotCache* guard = static_cast<NativeIRSlotCache*>(guard_ptr);
    if (!guard) {
        return;
    }

    current_ir_slot_cache = guard->prev_slot_cache;
    current_ir_slot_cache_dirty = guard->prev_slot_cache_dirty;
    current_native_ir_slot_cache_active = guard->prev_native_slot_cache_active;

    // Handler IR writes parent-slot changes back to the slot cache.  Publish the
    // final cache values to TLS so existing post-handler reload logic updates the
    // compiled parent's LLVM allocas and AST fallback observes the same values.
    for (size_t i = 0; i < guard->slots.size(); ++i) {
        if (i >= guard->dirty.size() || !guard->dirty[i]) {
            continue;
        }
        LocalVar* var = guard->slot_to_lvar[i];
        if (var) {
            qore_rt_sync_local(var, toBits(guard->slots[i]));
        }
    }

    delete guard;
}

extern "C" DLLEXPORT void qore_rt_push_on_block_exit(int type, StatementBlock* code) {
    jit_obe_handlers.push_back({static_cast<obe_type_e>(type), code, nullptr, nullptr, nullptr});
}

extern "C" DLLEXPORT void qore_rt_push_on_block_exit_ir(int type, StatementBlock* code,
        const QoreIRFunction* handler_ir) {
    jit_obe_handlers.push_back({static_cast<obe_type_e>(type), code, handler_ir, nullptr, nullptr});
}

extern "C" DLLEXPORT void qore_rt_push_compiled_handler(int type, StatementBlock* code,
        uint64_t (*compiled_fn)(ExceptionSink*), const QoreIRFunction* handler_func) {
    jit_obe_handlers.push_back({static_cast<obe_type_e>(type), code, nullptr, compiled_fn, handler_func});
}

extern "C" DLLEXPORT int64_t qore_rt_get_on_block_exit_count() {
    return static_cast<int64_t>(jit_obe_handlers.size());
}

extern "C" DLLEXPORT void qore_rt_exec_on_block_exit_impl(int64_t saved_count, ExceptionSink* xsink, bool inline_lowered) {
    size_t start = static_cast<size_t>(saved_count);
    if (jit_obe_handlers.size() <= start) {
        return;
    }

    ExceptionSink obe_xsink;
    bool error = xsink && xsink->isException();

    // This helper is called by LLVM/AOT-compiled parents.  If such a parent is
    // itself running under an outer IR interpreter frame, current_ir_slot_cache
    // belongs to that outer caller, not to the compiled parent whose deferred
    // handlers are being executed.  Compiled parents publish current locals to
    // the runtime local stack before this call; force handler IR to use that TLS
    // fallback instead of copying unrelated outer-frame slots.
    std::vector<QoreValue>* saved_ir_slot_cache = nullptr;
    std::vector<bool>* saved_ir_slot_cache_dirty = nullptr;
    bool saved_native_slot_cache_active = false;
    bool clear_stale_outer_slot_cache = !current_native_ir_slot_cache_active;
    if (clear_stale_outer_slot_cache) {
        saved_ir_slot_cache = current_ir_slot_cache;
        saved_ir_slot_cache_dirty = current_ir_slot_cache_dirty;
        saved_native_slot_cache_active = current_native_ir_slot_cache_active;
        current_ir_slot_cache = nullptr;
        current_ir_slot_cache_dirty = nullptr;
        current_native_ir_slot_cache_active = false;
    }
    struct NativeOBESlotCacheGuard {
        std::vector<QoreValue>*& slot_cache;
        std::vector<QoreValue>* saved;
        std::vector<bool>*& slot_cache_dirty;
        std::vector<bool>* saved_dirty;
        bool& native_slot_cache_active;
        bool saved_native_slot_cache_active;
        bool active;

        ~NativeOBESlotCacheGuard() {
            if (active) {
                slot_cache = saved;
                slot_cache_dirty = saved_dirty;
                native_slot_cache_active = saved_native_slot_cache_active;
            }
        }
    } native_obe_slot_cache_guard{current_ir_slot_cache, saved_ir_slot_cache,
        current_ir_slot_cache_dirty, saved_ir_slot_cache_dirty,
        current_native_ir_slot_cache_active, saved_native_slot_cache_active,
        clear_stale_outer_slot_cache};

    // Skip handler execution if handlers were already inlined; just clean up the vector
    if (!inline_lowered) {
        // Execute in reverse order (LIFO) — matching the AST's
        // StatementBlock::execIntern() semantics.
        for (int i = static_cast<int>(jit_obe_handlers.size()) - 1; i >= static_cast<int>(start); --i) {
            obe_type_e type = jit_obe_handlers[i].type;
            if (type == OBE_Unconditional || (!error && type == OBE_Success) || (error && type == OBE_Error)) {
                if (jit_obe_handlers[i].code || jit_obe_handlers[i].handler_ir
                        || jit_obe_handlers[i].compiled_fn) {
                    // Instantiate exception for on_error blocks as an implicit arg
                    std::unique_ptr<SingleArgvContextHelper> argv_helper;
                    std::unique_ptr<CatchExceptionHelper> ex_helper;
                    if (type == OBE_Error && xsink) {
                        QoreException* except = xsink->getException();
                        if (except) {
                            ex_helper.reset(new CatchExceptionHelper(except));
                            argv_helper.reset(new SingleArgvContextHelper(except->makeExceptionObject(), xsink));
                        }
                    }
                    if (jit_obe_handlers[i].compiled_fn) {
                        // Execute natively compiled handler
                        const QoreIRFunction* hf = jit_obe_handlers[i].handler_func;
                        if (hf) {
                            const QoreParseOptions& po = runtime_get_parse_options();
                            for (LocalVar* lv : hf->all_body_locals) {
                                lv->instantiate(po);
                            }
                        }
                        QoreValue rv(jit_obe_handlers[i].compiled_fn(&obe_xsink));
                        rv.discard(nullptr);
                        if (hf) {
                            for (int j = (int)hf->all_body_locals.size() - 1; j >= 0; --j) {
                                hf->all_body_locals[j]->uninstantiate(&obe_xsink);
                            }
                        }
                    } else if (jit_obe_handlers[i].handler_ir) {
                        // Execute compiled handler via IR interpreter
                        // Phase 2, Fix 2c: Pass parent slot cache for handler access to parent scope
                        QoreValue rv;
                        QoreIRInterpreter::execute(*jit_obe_handlers[i].handler_ir, rv, &obe_xsink,
                            nullptr, nullptr, nullptr,
                            &jit_obe_handlers[i].handler_ir->pre_instantiated_cache,
                            nullptr, nullptr, nullptr, false, nullptr,
                            current_ir_slot_cache);
                        rv.discard(nullptr);
                    } else {
                        StatementBlock* code = jit_obe_handlers[i].code;
                        const QoreProgramLocation* loc = code ? code->loc : nullptr;
                        if (loc) {
                            obe_xsink.raiseException("IR-AST-FALLBACK-ERROR",
                                "on-block-exit AST statement fallback is disabled: source=%s:%d; "
                                "add native handler IR lowering instead",
                                loc->getFileValue(), loc->start_line);
                        } else {
                            obe_xsink.raiseException("IR-AST-FALLBACK-ERROR",
                                "on-block-exit AST statement fallback is disabled; "
                                "add native handler IR lowering instead");
                        }
                    }
                    if (type == OBE_Error) {
                        if (qore_es_private::get(obe_xsink)->rethrown) {
                            if (xsink) {
                                xsink->clear();
                            }
                        }
                    }
                    if (obe_xsink) {
                        if (xsink) {
                            xsink->assimilate(obe_xsink);
                        } else {
                            obe_xsink.clear();
                        }
                        if (!error) {
                            error = true;
                        }
                    }
                }
            }
        }
    }

    // Remove handlers for this function scope
    jit_obe_handlers.resize(start);
}

// Backward-compatible wrapper that calls the implementation with inline_lowered=false
extern "C" DLLEXPORT void qore_rt_exec_on_block_exit(int64_t saved_count, ExceptionSink* xsink) {
    qore_rt_exec_on_block_exit_impl(saved_count, xsink, false);
}

// Discard (without firing) the on_block_exit handlers registered since
// saved_count, truncating the thread-local handler stack back to that mark.
// Used by the JIT deopt path: when a guard fails mid-function, the function
// re-executes via the AST interpreter, which re-registers and fires its own
// handlers — so the handlers the native code already pushed must be dropped
// (not fired), otherwise they leak onto the thread-local stack and fire later
// at an unrelated call (issue: guard-deopt on_block_exit double-fire).
extern "C" DLLEXPORT void qore_rt_discard_on_block_exit(int64_t saved_count) {
    qore_rt_exec_on_block_exit_impl(saved_count, nullptr, /*inline_lowered=*/true);
}

// --- AOT context-based helpers (Phase 7b) ---

extern "C" DLLEXPORT void qore_rt_push_on_block_exit_aot(QoreAOTContext* ctx, int32_t idx, int type,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_stmts);
    // Check if handler IR is available (strip-source mode or optimized path)
    if (idx < static_cast<int32_t>(ctx->handler_irs.size()) && ctx->handler_irs[idx]) {
        // Use handler IR for IR interpreter execution
        qore_rt_push_on_block_exit_ir(type, nullptr, ctx->handler_irs[idx].get());
    } else {
        const AbstractStatement* stmt = ctx->stmts[idx];
        const QoreProgramLocation* loc = stmt ? stmt->loc : nullptr;
        if (xsink && !*xsink) {
            if (loc) {
                xsink->raiseException("IR-AST-FALLBACK-ERROR",
                    "AOT on-block-exit handler has no compiled handler IR: source=%s:%d; "
                    "AST statement fallback is disabled",
                    loc->getFileValue(), loc->start_line);
            } else {
                xsink->raiseException("IR-AST-FALLBACK-ERROR",
                    "AOT on-block-exit handler has no compiled handler IR; "
                    "AST statement fallback is disabled");
            }
        }
    }
}

extern "C" DLLEXPORT CaseNodeRegex* qore_rt_get_regex_case_aot(QoreAOTContext* ctx, int32_t slot) {
    assert(ctx && slot >= 0 && slot < ctx->num_regex_cases);
    return ctx->regex_cases[slot];
}

extern "C" DLLEXPORT uint64_t qore_rt_load_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    return qore_rt_load_local(ctx->locals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_reload_local_if_stale_aot(QoreAOTContext* ctx, int32_t idx,
        uint64_t* cache, uint64_t* tracker, uint64_t* deferred, uint64_t* valid_epoch,
        uint64_t epoch, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_reload_local_if_stale(ctx->locals[idx], cache, tracker, deferred,
            valid_epoch, epoch, xsink);
}

extern "C" DLLEXPORT void qore_rt_assign_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_assign_local(ctx->locals[idx], val, xsink);
}

extern "C" DLLEXPORT void qore_rt_assign_local_eval_weak_aot(QoreAOTContext* ctx, int32_t idx,
        uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_assign_local_eval_weak(ctx->locals[idx], val, xsink);
}

extern "C" DLLEXPORT void qore_rt_assign_local_no_coerce_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_assign_local_no_coerce(ctx->locals[idx], val, xsink);
}

extern "C" DLLEXPORT void qore_rt_assign_local_no_coerce_eval_weak_aot(QoreAOTContext* ctx,
        int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_assign_local_no_coerce_eval_weak(ctx->locals[idx], val, xsink);
}

extern "C" DLLEXPORT void qore_rt_sync_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_sync_local(ctx->locals[idx], val);
}

extern "C" DLLEXPORT uint64_t qore_rt_coerce_value_aot(QoreAOTContext* ctx, int32_t local_idx,
        uint64_t value, uint64_t* cleanup_ptr, ExceptionSink* xsink) {
    assert(ctx && local_idx >= 0 && local_idx < ctx->num_locals);
    const QoreTypeInfo* ti = ctx->locals[local_idx]->getTypeInfoForLValue();
    return qore_rt_coerce_value(ti, value, cleanup_ptr, xsink);
}

// --- Phase 2B Step 5: Cast/coerce throwing wrappers ---

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_coerce_value_throwing(
        const QoreTypeInfo* ti, uint64_t value, uint64_t* cleanup_ptr, ExceptionSink* xsink) {
    uint64_t result = qore_rt_coerce_value(ti, value, cleanup_ptr, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_coerce_value_aot_throwing(
        QoreAOTContext* ctx, int32_t local_idx, uint64_t value,
        uint64_t* cleanup_ptr, ExceptionSink* xsink) {
    uint64_t result = qore_rt_coerce_value_aot(ctx, local_idx, value, cleanup_ptr, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_cast_with_inner_throwing(
        uint64_t cast_expr_bits, uint64_t inner_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_cast_with_inner(cast_expr_bits, inner_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_cast_with_inner_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t inner_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_cast_with_inner_aot(ctx, slot, inner_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_cast_by_type_path_throwing(
        uint64_t inner_bits, const char* type_path, int64_t or_nothing, ExceptionSink* xsink) {
    uint64_t result = qore_rt_cast_by_type_path(inner_bits, type_path, or_nothing, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_cast_by_type_path_aot_throwing(
        QoreAOTContext* ctx, uint64_t inner_bits, const char* type_path, int64_t or_nothing,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_cast_by_type_path_aot(ctx, inner_bits, type_path, or_nothing, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT void qore_rt_instantiate_local_aot(QoreAOTContext* ctx, int32_t idx) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_instantiate_local(ctx->locals[idx]);
}

extern "C" DLLEXPORT void qore_rt_clear_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_clear_local(ctx->locals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_uninstantiate_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    // Use clear (del) instead of uninstantiate (pop) because AOT body locals
    // are pre-instantiated by execJITWithDeopt() at function entry and will be
    // uninstantiated at function exit.  The LLVM code's UninstantiateLocal
    // corresponds to scope exit (like the end of a foreach loop body) and
    // should only clear the value, not pop the variable from the stack.
    qore_rt_clear_local(ctx->locals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_pop_closure_var_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    // For closure-use vars that are NOT pre-instantiated by evalTiered (AOT mode):
    // pop from the cvstack.  The variable may have been lazily instantiated by
    // StoreLocal or by LocalVar::getLValue/eval; if it was never accessed, it
    // may not be on the cvstack at all — check before popping.
    //
    // Frame-aware: only pop if the CVV is in the current frame. Otherwise the
    // pop would touch an outer frame's CVV (dangling for the outer's closure
    // captures) and crash on the outer's return.
    LocalVar* var = ctx->locals[idx];
    if (thread_try_find_closure_var_in_current_frame(var->getName())) {
        qore_rt_uninstantiate_local(var, xsink);
    }
}

static Var* qore_rt_resolve_global_slot_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_globals);
    auto trace_slot = [ctx, idx]() -> bool {
        const char* trace = getenv("QORE_AOT_TRACE_GLOBAL_SLOT");
        if (!trace) {
            return false;
        }
        if (!*trace) {
            return true;
        }
        const char* name = (static_cast<size_t>(idx) < ctx->global_names.size())
            ? ctx->global_names[idx].c_str() : "";
        return strstr(name, trace) != nullptr;
    };
    auto slot_name = [ctx, idx]() -> const char* {
        return (static_cast<size_t>(idx) < ctx->global_names.size())
            ? ctx->global_names[idx].c_str() : "";
    };
    Var* var = ctx->globals[idx];
    if (var) {
        if (trace_slot()) {
            fprintf(stderr, "[qore-aot-global] slot=%d name=%s cached var=%p\n",
                idx, slot_name(), static_cast<void*>(var));
        }
        return var;
    }
    if (static_cast<size_t>(idx) >= ctx->global_names.size() || ctx->global_names[idx].empty()) {
        if (trace_slot()) {
            fprintf(stderr, "[qore-aot-global] slot=%d has no global name\n", idx);
        }
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(ctx->global_resolution_mutex);
    var = ctx->globals[idx];
    if (var) {
        return var;
    }

    qore_program_private* pp = ctx->pgm ? qore_program_private::get(*ctx->pgm) : nullptr;
    if (pp && pp->RootNS) {
        const qore_ns_private* vns = nullptr;
        var = qore_root_ns_private::runtimeFindGlobalVar(*pp->RootNS, ctx->global_names[idx].c_str(), vns);
        if (var) {
            ctx->globals[idx] = var;
            if (trace_slot()) {
                fprintf(stderr, "[qore-aot-global] slot=%d name=%s resolved var=%p\n",
                    idx, slot_name(), static_cast<void*>(var));
            }
            return var;
        }
    }

    if (trace_slot()) {
        fprintf(stderr, "[qore-aot-global] slot=%d name=%s unresolved\n", idx, slot_name());
    }

    bool required = static_cast<size_t>(idx) < ctx->global_required_imports.size()
        && ctx->global_required_imports[idx];
    if (required && xsink) {
        xsink->raiseException("AOT-GLOBAL-IMPORT-ERROR",
            "required global import '%s' is not available in the linked/loaded Program",
            ctx->global_names[idx].c_str());
    }
    return nullptr;
}

extern "C" DLLEXPORT uint64_t qore_rt_load_global_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    Var* var = qore_rt_resolve_global_slot_aot(ctx, idx, xsink);
    uint64_t rv = qore_rt_load_global(var, xsink);
    const char* trace = getenv("QORE_AOT_TRACE_GLOBAL_SLOT");
    if (trace) {
        const char* name = (ctx && static_cast<size_t>(idx) < ctx->global_names.size())
            ? ctx->global_names[idx].c_str() : "";
        if (!*trace || strstr(name, trace)) {
            QoreValue v = fromBits(rv);
            fprintf(stderr, "[qore-aot-global] load slot=%d name=%s var=%p value=%s%s\n",
                idx, name, static_cast<void*>(var), v.getTypeName(), v.isNothing() ? " (NOTHING)" : "");
        }
    }
    return rv;
}

extern "C" DLLEXPORT void qore_rt_store_global_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_global(qore_rt_resolve_global_slot_aot(ctx, idx, xsink), val, xsink);
}

extern "C" DLLEXPORT void qore_rt_store_global_eval_weak_aot(QoreAOTContext* ctx, int32_t idx,
        uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_global_eval_weak(qore_rt_resolve_global_slot_aot(ctx, idx, xsink), val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_thread_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    return qore_rt_load_thread_local(qore_rt_resolve_global_slot_aot(ctx, idx, xsink), xsink);
}

extern "C" DLLEXPORT void qore_rt_store_thread_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_thread_local(qore_rt_resolve_global_slot_aot(ctx, idx, xsink), val, xsink);
}

extern "C" DLLEXPORT void qore_rt_store_thread_local_eval_weak_aot(QoreAOTContext* ctx,
        int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_thread_local_eval_weak(qore_rt_resolve_global_slot_aot(ctx, idx, xsink), val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_closure_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    static const bool disable_direct_load =
        std::getenv("QORE_DISABLE_AOT_DIRECT_CLOSURE_LOAD") != nullptr;
    if (!disable_direct_load) {
        if (ClosureVarValue* cvv = thread_resolve_runtime_closure_var(ctx->locals[idx])) {
            bool needs_deref = true;
            QoreValue result = cvv->eval(needs_deref, xsink);
            return toBits(qore_rt_deref_loaded_var_value(result, needs_deref, xsink));
        }
    }
    return qore_rt_load_local(ctx->locals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_store_closure_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    static const bool disable_direct_store =
        std::getenv("QORE_DISABLE_AOT_DIRECT_CLOSURE_STORE") != nullptr;
    if (!disable_direct_store) {
        if (ClosureVarValue* cvv = thread_resolve_runtime_closure_var(ctx->locals[idx])) {
            qore_rt_assign_closure_impl(cvv, val, xsink, false);
            return;
        }
    }
    qore_rt_assign_local(ctx->locals[idx], val, xsink);
}

extern "C" DLLEXPORT int64_t qore_rt_add_assign_local_int_aot(
        QoreAOTContext* ctx, int32_t idx, int64_t delta, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    return qore_rt_add_assign_local_int(ctx->locals[idx], delta, xsink);
}

extern "C" DLLEXPORT int64_t qore_rt_increment_closure_int_aot(
        QoreAOTContext* ctx, int32_t idx, int64_t delta, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    return qore_rt_increment_closure_int(ctx->locals[idx], delta, xsink);
}

extern "C" DLLEXPORT void qore_rt_store_closure_eval_weak_aot(QoreAOTContext* ctx, int32_t idx,
        uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    static const bool disable_direct_store =
        std::getenv("QORE_DISABLE_AOT_DIRECT_CLOSURE_STORE") != nullptr;
    if (!disable_direct_store) {
        if (ClosureVarValue* cvv = thread_resolve_runtime_closure_var(ctx->locals[idx])) {
            qore_rt_assign_closure_impl(cvv, val, xsink, true);
            return;
        }
    }
    qore_rt_assign_local_eval_weak(ctx->locals[idx], val, xsink);
}

// --- Phase 2B Step 5: Local access category throwing wrappers ---

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_assign_local_throwing(
        LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_assign_local(var, value, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_assign_local_eval_weak_throwing(
        LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_assign_local_eval_weak(var, value, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_assign_local_no_coerce_throwing(
        LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_assign_local_no_coerce(var, value, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_assign_local_no_coerce_eval_weak_throwing(
        LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_assign_local_no_coerce_eval_weak(var, value, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_local_throwing(
        LocalVar* var, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_local(var, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_global_throwing(
        Var* var, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_global(var, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_global_throwing(
        Var* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_store_global(var, value, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_global_eval_weak_throwing(
        Var* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_store_global_eval_weak(var, value, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_thread_local_throwing(
        Var* var, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_thread_local(var, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_thread_local_throwing(
        Var* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_store_thread_local(var, value, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_thread_local_eval_weak_throwing(
        Var* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_store_thread_local_eval_weak(var, value, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_static_var_throwing(
        QoreVarInfo* vi, const char* var_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_static_var(vi, var_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_static_var_for_call_throwing(
        QoreVarInfo* vi, const char* var_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_static_var_for_call(vi, var_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_static_var_by_path_throwing(
        const char* class_path, const char* var_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_static_var_by_path(class_path, var_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_static_var_by_path_for_call_throwing(
        const char* class_path, const char* var_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_static_var_by_path_for_call(class_path, var_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_static_var_by_path_aot_throwing(
        QoreAOTContext* ctx, const char* class_path, const char* var_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_static_var_by_path_aot(ctx, class_path, var_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_static_var_by_path_for_call_aot_throwing(
        QoreAOTContext* ctx, const char* class_path, const char* var_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_static_var_by_path_for_call_aot(ctx, class_path, var_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_constant_throwing(
        const RuntimeConstantRefNode* node, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_constant(node, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_constant_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_constant_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_assign_local_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_assign_local_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_assign_local_eval_weak_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_assign_local_eval_weak_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_assign_local_no_coerce_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_assign_local_no_coerce_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_assign_local_no_coerce_eval_weak_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_assign_local_no_coerce_eval_weak_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_global_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_global_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_global_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_global_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_global_eval_weak_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_global_eval_weak_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_thread_local_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_thread_local_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_thread_local_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_thread_local_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_thread_local_eval_weak_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_thread_local_eval_weak_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_load_closure_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_load_closure_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_closure_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_closure_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_store_closure_eval_weak_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    qore_rt_store_closure_eval_weak_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

static const QoreProgramLocation* qore_rt_aot_expr_loc(QoreAOTContext* ctx, int32_t idx) {
    if (!ctx || idx < 0 || idx >= ctx->num_exprs) {
        return nullptr;
    }
    QoreValue expr = fromBits(ctx->exprs[idx]);
    if (!expr.hasNode()) {
        return nullptr;
    }
    const ParseNode* pn = dynamic_cast<const ParseNode*>(expr.getInternalNode());
    return pn ? pn->loc : nullptr;
}

static const char* qore_rt_aot_expr_type(QoreAOTContext* ctx, int32_t idx) {
    if (!ctx) {
        return "<null AOT context>";
    }
    if (idx < 0 || idx >= ctx->num_exprs) {
        return "<invalid expression slot>";
    }
    QoreValue expr = fromBits(ctx->exprs[idx]);
    return expr.getTypeName();
}

static uint64_t qore_rt_raise_aot_ast_fallback(QoreAOTContext* ctx, int32_t idx,
        ExceptionSink* xsink, const char* helper, const char* reason) {
    if (xsink && !*xsink) {
        const QoreProgramLocation* loc = qore_rt_aot_expr_loc(ctx, idx);
        const char* type_name = qore_rt_aot_expr_type(ctx, idx);
        if (loc) {
            xsink->raiseException("AOT-AST-FALLBACK-ERROR",
                "%s: executable AST expression fallback is disabled for expression slot %d "
                "(type '%s') at %s:%d: %s; add native IR lowering or AOT slot support",
                helper ? helper : "<unknown helper>", idx, type_name,
                loc->getFileValue(), loc->start_line, reason ? reason : "unsupported AOT helper path");
        } else {
            xsink->raiseException("AOT-AST-FALLBACK-ERROR",
                "%s: executable AST expression fallback is disabled for expression slot %d "
                "(type '%s'): %s; add native IR lowering or AOT slot support",
                helper ? helper : "<unknown helper>", idx, type_name,
                reason ? reason : "unsupported AOT helper path");
        }
    }
    return toBits(QoreValue());
}

static bool qore_rt_aot_env_enabled(const char* name) {
    const char* value = getenv(name);
    return value && *value && strcmp(value, "0") && strcmp(value, "false")
        && strcmp(value, "FALSE") && strcmp(value, "no") && strcmp(value, "NO");
}

static uint64_t qore_rt_call_static_method_direct_impl(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const QoreTypeInfo* receiver_type_info);

static bool qore_rt_aot_disable_prelinked_calls() {
    static const bool enabled = qore_rt_aot_env_enabled("QORE_AOT_DISABLE_PRELINKED_CALLS");
    return enabled;
}

static bool qore_rt_aot_trace_link_relocs() {
    static const bool enabled = qore_rt_aot_env_enabled("QORE_AOT_TRACE_LINK_RELOCS");
    return enabled;
}

static bool qore_rt_aot_strict_prelink() {
    static const bool enabled = qore_rt_aot_env_enabled("QORE_AOT_STRICT_PRELINK");
    return enabled;
}

static void qore_rt_trace_aot_prelink(QoreAOTContext* ctx, int32_t slot,
        const char* helper, const char* target_kind, const char* action,
        const char* reason = nullptr) {
    if (!qore_rt_aot_trace_link_relocs()) {
        return;
    }
    fprintf(stderr, "[qore-aot-prelink] helper=%s slot=%d kind=%s action=%s type=%s",
        helper ? helper : "<unknown>", slot, target_kind ? target_kind : "unknown",
        action ? action : "unknown", qore_rt_aot_expr_type(ctx, slot));
    const QoreProgramLocation* loc = qore_rt_aot_expr_loc(ctx, slot);
    if (loc) {
        fprintf(stderr, " loc=%s:%d", loc->getFileValue(), loc->start_line);
    }
    if (reason && *reason) {
        fprintf(stderr, " reason=%s", reason);
    }
    fputc('\n', stderr);
}

static uint64_t qore_rt_raise_aot_prelink_error(QoreAOTContext* ctx, int32_t slot,
        ExceptionSink* xsink, const char* helper, const char* target_kind,
        const char* reason) {
    if (xsink && !*xsink) {
        const QoreProgramLocation* loc = qore_rt_aot_expr_loc(ctx, slot);
        const char* type_name = qore_rt_aot_expr_type(ctx, slot);
        if (loc) {
            xsink->raiseException("AOT-PRELINK-ERROR",
                "%s: missing prelinked %s target for expression slot %d "
                "(type '%s') at %s:%d: %s",
                helper ? helper : "<unknown helper>", target_kind ? target_kind : "call",
                slot, type_name, loc->getFileValue(), loc->start_line,
                reason ? reason : "unresolved prelinked call target");
        } else {
            xsink->raiseException("AOT-PRELINK-ERROR",
                "%s: missing prelinked %s target for expression slot %d "
                "(type '%s'): %s",
                helper ? helper : "<unknown helper>", target_kind ? target_kind : "call",
                slot, type_name, reason ? reason : "unresolved prelinked call target");
        }
    }
    return toBits(QoreValue());
}

static uint64_t qore_rt_call_slot_dynamic_fallback(QoreAOTContext* ctx, int32_t slot,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const char* helper, const char* target_kind, const char* reason) {
    qore_rt_trace_aot_prelink(ctx, slot, helper, target_kind, "fallback", reason);
    if (!ctx || slot < 0 || slot >= ctx->num_exprs) {
        return qore_rt_raise_aot_prelink_error(ctx, slot, xsink, helper, target_kind,
            "invalid expression slot");
    }
    QoreValue expr = fromBits(ctx->exprs[slot]);
    if (expr.hasNode()) {
        return qore_rt_call_with_args_impl(ctx->exprs[slot], args, arg_cleanups, nargs, xsink);
    }

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    qore_rt_trace_aot_prelink(ctx, slot, helper, target_kind, "fallback-target",
        "serialized call expression is not available");
    if (target.is_static_method && target.method) {
        return qore_rt_call_static_method_direct_impl(target.method, target.variant,
            args, arg_cleanups, nargs, xsink, target.receiver_type_info);
    }
    if (target.method) {
        if (target.is_self_method) {
            return qore_rt_call_self_method_dispatch_impl(target, args, arg_cleanups, nargs, xsink);
        }
        return qore_rt_call_method_direct_impl(target.method, args, arg_cleanups, nargs, xsink);
    }
    if (target.func) {
        if (target.variant) {
            return qore_rt_call_function_direct_impl(target.func, target.variant,
                target.pgm, args, arg_cleanups, nargs, xsink);
        }
        return qore_rt_call_function_dynamic_impl(target.func, target.pgm, args,
            arg_cleanups, nargs, xsink);
    }
    if (qore_rt_aot_strict_prelink()) {
        return qore_rt_raise_aot_prelink_error(ctx, slot, xsink, helper, target_kind,
            "serialized call expression and pre-resolved target are not available");
    }
    return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink, helper,
        "serialized call expression and pre-resolved target are not available");
}

static uint64_t qore_rt_missing_prelinked_call_target(QoreAOTContext* ctx, int32_t slot,
        ExceptionSink* xsink, const char* helper, const char* target_kind,
        const char* reason) {
    qore_rt_trace_aot_prelink(ctx, slot, helper, target_kind, "missing", reason);
    if (qore_rt_aot_strict_prelink()) {
        return qore_rt_raise_aot_prelink_error(ctx, slot, xsink, helper, target_kind, reason);
    }
    return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink, helper, reason);
}

static uint64_t qore_rt_call_implicit_self_copy_aot(QoreAOTContext* ctx, int32_t slot,
        int nargs, ExceptionSink* xsink) {
    if (nargs != 0) {
        return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
            "qore_rt_call_implicit_self_copy_aot", "copy() cannot have arguments");
    }
    RuntimeConfig& rc = rc_get_current_ref();
    QoreObject* self = rc.getObject() ? rc.getObject() : runtime_get_stack_object();
    if (!self) {
        if (xsink && !*xsink) {
            xsink->raiseException("AOT-CALL-ERROR",
                "qore_rt_call_implicit_self_copy_aot: no self object for expression slot %d", slot);
        }
        return toBits(QoreValue());
    }
    return toBits(self->getClass()->execCopy(self, xsink));
}

static bool qore_rt_is_implicit_self_copy_slot(QoreAOTContext* ctx, int32_t slot) {
    if (!ctx || slot < 0 || slot >= ctx->num_exprs) {
        return false;
    }
    QoreValue expr;
    std::memcpy(&expr, &ctx->exprs[slot], sizeof(expr));
    const auto* self_call = dynamic_cast<const SelfFunctionCallNode*>(expr.getInternalNode());
    return self_call && self_call->getName() && !strcmp(self_call->getName(), "copy");
}

extern "C" DLLEXPORT uint64_t qore_rt_invoke_expr_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_raise_aot_ast_fallback(ctx, idx, xsink, "qore_rt_invoke_expr_aot",
        "generic expression evaluation is forbidden in AOT");
}

// --- Phase 2B Step 5: invoke_expr throwing wrappers ---

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_invoke_expr_throwing(
        uint64_t expr_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_invoke_expr(expr_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_invoke_expr_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_invoke_expr_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

//! Return the raw QoreValue bits of an expression slot without evaluating.
//!
//! Used by AOT-lowered `RefForeachInit`, which needs the
//! `ParseReferenceNode*` POINTER to hand to
//! `qore_rt_ref_foreach_init` (the helper does its own
//! `evalToRef` inside).  The historical AOT path used
//! `qore_rt_invoke_expr_aot` here, which `eval()`s the expression
//! and returned a `ReferenceNode*` — `qore_rt_ref_foreach_init`
//! then `reinterpret_cast`ed that as a `ParseReferenceNode*` and
//! called `evalToRef` on it, SIGSEGVing in the vtable because
//! `ReferenceNode`'s vtable has no `evalToRef`.  Equivalent to
//! JIT mode which embeds the pointer bits as a constant.
extern "C" DLLEXPORT uint64_t qore_rt_get_expr_bits_aot(QoreAOTContext* ctx, int32_t idx) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return ctx->exprs[idx];
}

extern "C" DLLEXPORT uint64_t qore_rt_vrn_construct_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    if (expr.hasNode()) {
        if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(expr.getInternalNode())) {
            return toBits(vrn->constructValue(xsink));
        }
        if (auto* nhd = dynamic_cast<const NewHashDeclNode*>(expr.getInternalNode())) {
            return qore_rt_new_hash_decl(nhd, xsink);
        }
        if (dynamic_cast<const NewObjectCallNode*>(expr.getInternalNode())
                || dynamic_cast<const ScopedObjectCallNode*>(expr.getInternalNode())) {
            // QoreValue::eval(bool&) requires needs_deref==true on entry (the deref
            // contract; debug-asserted). These nodes always evaluate to a new owned
            // object, so the returned (owned) value is correct to bake into the slot.
            bool needs_deref = true;
            return toBits(expr.eval(needs_deref, xsink));
        }
        if (auto* fcn = dynamic_cast<const FunctionCallNode*>(expr.getInternalNode())) {
            const char* name = fcn->getName();
            if (name && !strcmp(name, "create_object")) {
                bool needs_deref = true;
                return toBits(expr.eval(needs_deref, xsink));
            }
        }
    }
    return qore_rt_raise_aot_ast_fallback(ctx, idx, xsink, "qore_rt_vrn_construct_aot",
        "unexpected non-VarRefNewObject constructor expression");
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_load_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_load(ctx->exprs[idx], xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_store_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_store(ctx->exprs[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_store_weak_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_store_weak(ctx->exprs[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_unary_aot(int op, QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_unary(op, ctx->exprs[idx], xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_binary_aot(int op, QoreAOTContext* ctx, int32_t idx, uint64_t val,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_binary(op, ctx->exprs[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_ternary_aot(int op, QoreAOTContext* ctx, int32_t idx, uint64_t first,
        uint64_t second, uint64_t third, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_ternary(op, ctx->exprs[idx], first, second, third, xsink);
}

// --- LValuePath runtime helpers ---
// Copy path steps and patch dynamic operands from NaN-boxed array.
// Note: slice_values stores BORROWED QoreValues aliasing the dyn_vals array;
// the caller must not use path_copy beyond the lifetime of dyn_vals.
static void patchLVPath(std::vector<LVPathStep>& path_copy,
        const QoreIRLValuePathInstruction* inst,
        const uint64_t* dyn_vals) {
    path_copy = inst->path;
    int dyn_idx = 0;
    for (auto& step : path_copy) {
        if (step.kind == LVPathStepKind::HashKey && step.operand_idx != UINT32_MAX) {
            QoreValue key_val = fromBits(dyn_vals[dyn_idx++]);
            step.slice_values.clear();
            step.slice_values.push_back(key_val);
            QoreStringValueHelper key_str(key_val);
            step.name = key_str->c_str();
        } else if (step.kind == LVPathStepKind::ListIndex && step.operand_idx != UINT32_MAX) {
            QoreValue idx_val = fromBits(dyn_vals[dyn_idx++]);
            step.index = idx_val.getAsBigInt();
        } else if (step.kind == LVPathStepKind::HashKeySlice
                || step.kind == LVPathStepKind::ListIndexSlice
                || step.kind == LVPathStepKind::ListRangeSlice) {
            step.slice_values.clear();
            step.slice_values.reserve(step.slice_operand_ids.size());
            for (size_t i = 0; i < step.slice_operand_ids.size(); ++i) {
                step.slice_values.push_back(fromBits(dyn_vals[dyn_idx++]));
            }
        }
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_lv_path_assign(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    if (*xsink) {
        return toBits(QoreValue());
    }
    std::vector<LVPathStep> path_copy;
    patchLVPath(path_copy, inst, dyn_vals);

    QoreValue val = fromBits(rhs_bits);
    ValueHolder val_holder(val.refSelf(), xsink);
    QoreValue assign_val = val;
    ValueHolder eval_holder(xsink);
    qore_type_t val_type = val.getType();
    if (!inst->weak && (val_type == NT_WEAKREF || val_type == NT_WEAKREF_HASH || val_type == NT_WEAKREF_LIST)) {
        eval_holder = val.eval(xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
        assign_val = *eval_holder;
    }

    LValueHelper lvh(xsink);
    if (lvh.navigatePath(path_copy.data(), path_copy.size(), false)) {
        return toBits(QoreValue());
    }
    if (lvh.assign(assign_val.refSelf(), "<lvalue path assign>", true, inst->weak)) {
        return toBits(QoreValue());
    }
    return toBits(lvh.getReferencedValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_lv_path_compound(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    if (*xsink) {
        return toBits(QoreValue());
    }
    std::vector<LVPathStep> path_copy;
    patchLVPath(path_copy, inst, dyn_vals);

    QoreValue rhs = fromBits(rhs_bits);
    ValueHolder rhs_holder(rhs.refSelf(), xsink);

    LValueHelper lvh(xsink);
    if (lvh.navigatePath(path_copy.data(), path_copy.size(), false)) {
        return toBits(QoreValue());
    }
    QoreValue res;
    switch (inst->compound_op) {
        case LVCompoundOp::AddAssign:
            res = doPlusEqualsOnLValue(lvh, rhs, xsink);
            break;
        case LVCompoundOp::SubAssign:
            res = doMinusEqualsOnLValue(lvh, rhs, xsink);
            break;
        default: {
            qore_type_t vtype = lvh.getType();
            if (vtype == NT_NUMBER || rhs.getType() == NT_NUMBER) {
                switch (inst->compound_op) {
                    case LVCompoundOp::MulAssign: lvh.multiplyEqualsNumber(rhs); break;
                    case LVCompoundOp::DivAssign: lvh.divideEqualsNumber(rhs); break;
                    default: res = QoreValue(); break;
                }
            } else if (vtype == NT_FLOAT || rhs.getType() == NT_FLOAT) {
                double rv = rhs.getAsFloat();
                switch (inst->compound_op) {
                    case LVCompoundOp::MulAssign: res = lvh.multiplyEqualsFloat(rv); break;
                    case LVCompoundOp::DivAssign: res = lvh.divideEqualsFloat(rv); break;
                    default: break;
                }
            } else {
                int64 rv = rhs.getAsBigInt();
                switch (inst->compound_op) {
                    case LVCompoundOp::MulAssign: res = lvh.multiplyEqualsBigInt(rv); break;
                    case LVCompoundOp::DivAssign: res = lvh.divideEqualsBigInt(rv); break;
                    case LVCompoundOp::ModAssign: res = lvh.modulaEqualsBigInt(rv); break;
                    case LVCompoundOp::AndAssign: res = lvh.andEqualsBigInt(rv); break;
                    case LVCompoundOp::OrAssign: res = lvh.orEqualsBigInt(rv); break;
                    case LVCompoundOp::XorAssign: res = lvh.xorEqualsBigInt(rv); break;
                    case LVCompoundOp::ShlAssign: res = lvh.shiftLeftEqualsBigInt(rv); break;
                    case LVCompoundOp::ShrAssign: res = lvh.shiftRightEqualsBigInt(rv); break;
                    default: break;
                }
            }
            break;
        }
    }
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(res);
}

// Shared helper for HashKeySlice terminal step: iterates the resolved
// slice keys, removes each via takeKeyValue / takeMembers, and returns
// the aggregated result hash (or NOTHING for Delete).
// Callable from both the JIT runtime (qore_rt_lv_path_unary) and the
// IR interpreter (QoreIRInterpreter.cpp LValuePathUnary dispatch).
// Precondition: `lvh` is navigated to the parent container; `ct` is the
// container's type (NT_HASH or NT_OBJECT); `last_step` is the terminal
// HashKeySlice step with `slice_values` populated.
constexpr size_t QORE_RT_LVALUE_SLICE_CANCEL_INTERVAL = 16 * 1024;
constexpr size_t QORE_RT_LVALUE_SLICE_CANCEL_MASK = QORE_RT_LVALUE_SLICE_CANCEL_INTERVAL - 1;

static inline bool qore_rt_check_lvalue_slice_cancel(ExceptionSink* xsink, size_t& i, const char* operation) {
    return ((++i & QORE_RT_LVALUE_SLICE_CANCEL_MASK) == 0) && qore_check_cancel(xsink, operation);
}

QoreValue executeLVHashKeySliceRemove(LValueHelper& lvh, qore_type_t ct,
        const LVPathStep& last_step, LVUnaryOp unary_op,
        ExceptionSink* xsink) {
    const bool is_delete = (unary_op == LVUnaryOp::Delete);
    auto for_each_key = [&](auto&& cb) -> bool {
        size_t cancel_i = 0;
        for (const QoreValue& operand : last_step.slice_values) {
            if (operand.getType() == NT_LIST) {
                ConstListIterator li(operand.get<const QoreListNode>());
                while (li.next()) {
                    if (qore_rt_check_lvalue_slice_cancel(xsink, cancel_i, "lvalue hash slice remove")) {
                        return true;
                    }
                    if (cb(li.getValue())) {
                        return true;
                    }
                }
            } else {
                if (qore_rt_check_lvalue_slice_cancel(xsink, cancel_i, "lvalue hash slice remove")) {
                    return true;
                }
                if (cb(operand)) {
                    return true;
                }
            }
        }
        return false;
    };

    if (ct == NT_HASH) {
        lvh.ensureUnique();
        QoreHashNode* h = lvh.getValue().get<QoreHashNode>();
        if (!h) {
            return QoreValue();
        }
        qore_hash_private* hp = qore_hash_private::get(*h);
        const unsigned old_count = qore_hash_private::getScanCount(*h);
        ReferenceHolder<QoreHashNode> rvh(new QoreHashNode(autoTypeInfo), xsink);
        if (for_each_key([&](const QoreValue& key_val) -> bool {
            QoreStringValueHelper mem(key_val, QCS_DEFAULT, xsink);
            if (*xsink) {
                return true;
            }
            bool exists;
            QoreValue n = hp->takeKeyValueIntern(mem->c_str(), exists);
            if (!exists) {
                return false;
            }
            // note that no exception can occur here
            rvh->setKeyValue(mem->c_str(), n, xsink);
            if (*xsink) {
                return true;
            }
            return false;
        })) {
            return QoreValue();
        }
        if (old_count && !qore_hash_private::getScanCount(*h)) {
            lvh.setDelta(-1);
        }
        if (is_delete) {
            return QoreValue();  // ReferenceHolder frees rvh
        }
        return rvh.release();
    }
    if (ct == NT_OBJECT || ct == NT_WEAKREF) {
        QoreObject* o = ct == NT_OBJECT
            ? lvh.getValue().get<QoreObject>()
            : lvh.getValue().get<const WeakReferenceNode>()->get();
        if (!o) {
            return QoreValue();
        }
        // Build a plain QoreListNode of string keys so we can reuse
        // qore_object_private::takeMembers (which handles class-context
        // access checks + multi-class internal data + scan tracking).
        ReferenceHolder<QoreListNode> key_list(new QoreListNode(autoTypeInfo), xsink);
        if (for_each_key([&](const QoreValue& key_val) -> bool {
            key_list->push(key_val.refSelf(), xsink);
            if (*xsink) {
                return true;
            }
            return false;
        })) {
            return QoreValue();
        }
        QoreLValueGeneric rv;
        qore_object_private::takeMembers(*o, rv, lvh, *key_list);
        if (*xsink) {
            rv.removeValue(true).discard(xsink);
            return QoreValue();
        }
        QoreValue result = rv.removeValue(true);
        if (is_delete) {
            result.discard(xsink);
            return QoreValue();
        }
        return result;
    }
    return QoreValue();
}

// Shared helper for ListIndexSlice terminal step: resolves indices from
// `slice_values`, expanding any runtime-list operand (produced by a
// range operator) into its integer components, then removes/deletes
// each index from the list/string/binary container in reverse-sorted
// order so earlier indexes are not shifted.
// Returns the aggregated removed-content collection for Remove, or
// NOTHING for Delete.
QoreValue executeLVListIndexSliceRemove(LValueHelper& lvh, qore_type_t ct,
        const LVPathStep& last_step, LVUnaryOp unary_op,
        ExceptionSink* xsink) {
    const bool is_delete = (unary_op == LVUnaryOp::Delete);

    // Helper: push an int operand OR iterate a list operand (range-produced)
    // pushing its entries onto the caller-provided ind-collector.  Matches
    // the AST path's treatment of range operators inside slice lists.
    auto each_index = [&](const QoreValue& operand, auto&& cb) -> bool {
        if (operand.getType() == NT_LIST) {
            ConstListIterator li(operand.get<const QoreListNode>());
            size_t cancel_i = 0;
            while (li.next()) {
                if (qore_rt_check_lvalue_slice_cancel(xsink, cancel_i, "lvalue list slice remove")) {
                    return true;
                }
                if (cb(li.getValue().getAsBigInt(), true)) {
                    return true;
                }
            }
        } else {
            return cb(operand.getAsBigInt(), false);
        }
        return false;
    };

    if (ct == NT_LIST) {
        lvh.ensureUnique();
        QoreListNode* l = lvh.getValue().get<QoreListNode>();
        if (!l) {
            return QoreValue();
        }
        const QoreTypeInfo* vtype = nullptr;
        bool vcommon = false;
        ReferenceHolder<QoreListNode> v(new QoreListNode(autoTypeInfo), xsink);
        // Reverse-sorted set of indexes to remove (matches AST path).
        std::set<int64_t, std::greater<int64_t>> iset;
        size_t iter_i = 0;
        size_t cancel_i = 0;
        for (const QoreValue& operand : last_step.slice_values) {
            if (qore_rt_check_lvalue_slice_cancel(xsink, cancel_i, "lvalue list slice remove")) {
                return QoreValue();
            }
            if (each_index(operand, [&](int64_t ind, bool is_range) -> bool {
                QoreValue p{};
                bool push;
                if (ind >= 0 && ind < (int64_t)l->size()) {
                    iset.insert(ind);
                    p = l->getReferencedEntry(ind);
                    push = true;
                } else {
                    push = !is_range
                            || (runtime_get_parse_options() & PO_BROKEN_LIST_RANGE);
                }
                if (push) {
                    if (!iter_i) {
                        vtype = p.getTypeInfo();
                        vcommon = true;
                    } else if (vcommon
                            && !QoreTypeInfo::matchCommonType(vtype, p.getTypeInfo())) {
                        vcommon = false;
                    }
                    v->push(p, nullptr);
                }
                ++iter_i;
                return false;
            })) {
                return QoreValue();
            }
        }
        if (vtype && vtype != anyTypeInfo) {
            qore_list_private::get(**v)->complexTypeInfo
                    = qore_get_complex_list_type(vtype);
        }
        // Dereference removed entries outside the lvalue lock for safety,
        // matching the AST path's `ReferenceHolder<QoreListNode> holder`.
        ReferenceHolder<QoreListNode> holder(xsink);
        cancel_i = 0;
        for (auto& i : iset) {
            if (qore_rt_check_lvalue_slice_cancel(xsink, cancel_i, "lvalue list slice remove")) {
                return QoreValue();
            }
            QoreValue ve = qore_list_private::get(*l)->spliceSingle(i);
            if (ve.isReferenceCounted()) {
                if (!holder) {
                    holder = new QoreListNode(autoTypeInfo);
                }
                holder->push(ve, xsink);
                if (*xsink) {
                    return QoreValue();
                }
            }
        }
        if (needs_scan(*v)) {
            if (!qore_list_private::getScanCount(*l)) {
                lvh.setDelta(-1);
            }
        }
        if (is_delete) {
            return QoreValue();
        }
        return v.release();
    }
    if (ct == NT_STRING) {
        lvh.ensureUnique();
        QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
        if (!str) {
            return QoreValue();
        }
        SimpleRefHolder<QoreStringNode> v(new QoreStringNode(str->getEncoding()));
        size_t len = str->length();
        std::set<int64_t, std::greater<int64_t>> iset;
        size_t cancel_i = 0;
        for (const QoreValue& operand : last_step.slice_values) {
            if (qore_rt_check_lvalue_slice_cancel(xsink, cancel_i, "lvalue string slice remove")) {
                return QoreValue();
            }
            bool had_error = false;
            if (each_index(operand, [&](int64_t ind, bool /*is_range*/) -> bool {
                if (had_error) return true;
                if (ind >= 0 && ind < (int64_t)len) {
                    iset.insert(ind);
                    int cp = str->getUnicodePoint(ind, xsink);
                    if (*xsink) {
                        had_error = true;
                        return true;
                    }
                    if (v->concatUnicode(cp, xsink)) {
                        had_error = true;
                        return true;
                    }
                }
                return false;
            })) {
                if (*xsink) {
                    return QoreValue();
                }
                break;
            }
            if (had_error) break;
        }
        // Collapse the string by splicing removed characters — isolate from any
        // prior exception so partial removals still land.
        {
            ExceptionSink xsink2;
            cancel_i = 0;
            for (auto& i : iset) {
                if (qore_rt_check_lvalue_slice_cancel(xsink, cancel_i, "lvalue string slice remove")) {
                    return QoreValue();
                }
                str->splice(i, 1, &xsink2);
                if (xsink2) break;
            }
            if (xsink2) {
                xsink->assimilate(xsink2);
            }
        }
        if (is_delete) {
            return QoreValue();
        }
        return v.release();
    }
    if (ct == NT_BINARY) {
        lvh.ensureUnique();
        BinaryNode* bin = lvh.getValue().get<BinaryNode>();
        if (!bin) {
            return QoreValue();
        }
        SimpleRefHolder<BinaryNode> v(new BinaryNode);
        std::set<int64_t, std::greater<int64_t>> iset;
        size_t cancel_i = 0;
        for (const QoreValue& operand : last_step.slice_values) {
            if (qore_rt_check_lvalue_slice_cancel(xsink, cancel_i, "lvalue binary slice remove")) {
                return QoreValue();
            }
            if (each_index(operand, [&](int64_t ind, bool /*is_range*/) -> bool {
                if (ind >= 0 && ind < (int64_t)bin->size()) {
                    iset.insert(ind);
                    bin->substr(**v, ind, 1);
                }
                return false;
            })) {
                return QoreValue();
            }
        }
        cancel_i = 0;
        for (auto& i : iset) {
            if (qore_rt_check_lvalue_slice_cancel(xsink, cancel_i, "lvalue binary slice remove")) {
                return QoreValue();
            }
            bin->splice(i, 1);
        }
        if (is_delete) {
            return QoreValue();
        }
        return v.release();
    }
    return QoreValue();
}

// Shared helper for ListRangeSlice terminal step: removes/deletes a contiguous
// effective range from a list/string/binary lvalue, matching
// QoreSquareBracketsRangeOperatorNode + LValueRemoveHelper semantics.
QoreValue executeLVListRangeSliceRemove(LValueHelper& lvh, qore_type_t ct,
        const LVPathStep& last_step, LVUnaryOp unary_op,
        ExceptionSink* xsink) {
    const bool is_delete = (unary_op == LVUnaryOp::Delete);
    if (last_step.slice_values.size() != 2) {
        xsink->raiseException("IR-EXEC-ERROR", "ListRangeSlice remove requires start and stop operands");
        return QoreValue();
    }

    bool broken_list_range = static_cast<bool>(runtime_get_parse_options() & PO_BROKEN_LIST_RANGE);
    bool negative_offsets = runtime_check_parse_option(PO_NEGATIVE_OFFSETS);
    int64 start = 0;
    int64 stop = 0;
    int64 seq_size = 0;
    QoreValue seq = lvh.getValue();
    if (!QoreSquareBracketsRangeOperatorNode::getEffectiveRange(seq, start, stop, seq_size,
            last_step.slice_values[0], last_step.slice_values[1], broken_list_range, negative_offsets, xsink)) {
        if (*xsink || is_delete) {
            return QoreValue();
        }
        if (ct == NT_LIST) {
            ReferenceHolder<QoreListNode> v(new QoreListNode(autoTypeInfo), xsink);
            if (broken_list_range) {
                int64 d = stop - start;
                if (d < 0) {
                    d = -d;
                }
                ++d;
                while (d--) {
                    v->push(QoreValue(), xsink);
                    if (*xsink) {
                        return QoreValue();
                    }
                }
            }
            return v.release();
        }
        if (ct == NT_STRING) {
            return QoreValue::makeStringValue("");
        }
        if (ct == NT_BINARY) {
            return QoreValue(new BinaryNode);
        }
        return QoreValue();
    }

    bool reverse = false;
    if (stop < start) {
        reverse = true;
        std::swap(start, stop);
    }

    if (ct == NT_LIST) {
        lvh.ensureUnique();
        QoreListNode* l = lvh.getValue().get<QoreListNode>();
        if (!l) {
            return QoreValue();
        }
        size_t orig_size = l->size();
        ReferenceHolder<QoreListNode> nl(l->extract(start, stop - start + 1), xsink);
        if (stop >= static_cast<int64>(orig_size)) {
            qore_list_private::get(**nl)->resize(nl->size() + stop - orig_size + 1);
        }
        if (reverse) {
            nl = nl->reverse();
        }
        if (is_delete) {
            return QoreValue();
        }
        return nl.release();
    }

    if (ct == NT_STRING) {
        lvh.ensureUnique();
        QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
        if (!str) {
            return QoreValue();
        }
        SimpleRefHolder<QoreStringNode> ns(str->extract(start, stop - start + 1, xsink));
        if (*xsink) {
            return QoreValue();
        }
        if (reverse) {
            ns = ns->reverse();
        }
        if (is_delete) {
            return QoreValue();
        }
        return ns.release();
    }

    if (ct == NT_BINARY) {
        lvh.ensureUnique();
        BinaryNode* bin = lvh.getValue().get<BinaryNode>();
        if (!bin) {
            return QoreValue();
        }
        SimpleRefHolder<BinaryNode> nb(new BinaryNode);
        bin->splice(start, stop - start + 1, nullptr, 0, *nb);
        if (reverse) {
            BinaryNode* rb = new BinaryNode;
            for (size_t i = 0; i < nb->size(); ++i) {
                rb->append(static_cast<const char*>(nb->getPtr()) + nb->size() - i - 1, 1);
            }
            nb = rb;
        }
        if (is_delete) {
            return QoreValue();
        }
        return nb.release();
    }

    return QoreValue();
}

extern "C" DLLEXPORT uint64_t qore_rt_lv_path_unary(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
        ExceptionSink* xsink) {
    if (*xsink) {
        return toBits(QoreValue());
    }
    std::vector<LVPathStep> path_copy;
    patchLVPath(path_copy, inst, dyn_vals);

    bool is_remove = (inst->unary_op == LVUnaryOp::Remove || inst->unary_op == LVUnaryOp::Delete);
    QoreValue res;

    auto finish_delete_result = [&](QoreValue& value) -> bool {
        if (inst->unary_op != LVUnaryOp::Delete) {
            return true;
        }
        if (value.getType() == NT_OBJECT) {
            QoreObject* o = value.get<QoreObject>();
            if (o->isSystemObject()) {
                xsink->raiseException("SYSTEM-OBJECT-ERROR",
                    "cannot delete a system constant object (class '%s')", o->getClassName());
            } else {
                o->doDelete(xsink);
            }
        }
        value.discard(xsink);
        value = QoreValue();
        return !*xsink;
    };

    if (is_remove && path_copy.size() == 1
            && path_copy[0].kind == LVPathStepKind::SelfMember) {
        QoreObject* obj = runtime_get_stack_object();
        if (!obj) {
            xsink->raiseException("LVALUE-ERROR",
                "no object context for self member remove");
            return toBits(QoreValue());
        }
        if (qore_rt_check_closure_self_valid(obj, xsink)) {
            return toBits(QoreValue());
        }
        res = qore_object_private::takeMember(*obj, xsink,
            path_copy[0].name.c_str(), false);
        if (*xsink || !finish_delete_result(res)) {
            return toBits(QoreValue());
        }
        if (!inst->ref_rv) {
            res.discard(xsink);
            return toBits(QoreValue());
        }
        return toBits(res);
    }

    // Multi-step hash/list remove/delete: navigate to the PARENT container,
    // then drop the final key/element.  LValueHelper::remove() only clears
    // the value without removing the hash key; we need container-level
    // removal.  Mirrors the IR-interpreter path at QoreIRInterpreter.cpp
    // (case LValuePathUnary).
    //
    // Object members and any non-container parent state fall through to the
    // legacy single-step navigate + lvh.remove() route below.
    bool handled_multistep_remove = false;
    if (is_remove && path_copy.size() >= 2) {
        const LVPathStep& last_step = path_copy.back();
        bool last_is_hash = (last_step.kind == LVPathStepKind::HashKeyConst
                || last_step.kind == LVPathStepKind::HashKey);
        bool last_is_list = (last_step.kind == LVPathStepKind::ListIndex);
        bool last_is_hash_slice = (last_step.kind == LVPathStepKind::HashKeySlice);
        bool last_is_hash_list = last_is_hash && last_step.slice_values.size() == 1
                && last_step.slice_values[0].getType() == NT_LIST;
        bool last_is_list_slice = (last_step.kind == LVPathStepKind::ListIndexSlice);
        bool last_is_list_range = (last_step.kind == LVPathStepKind::ListRangeSlice);
        if (last_is_hash || last_is_list || last_is_hash_slice || last_is_hash_list || last_is_list_slice
                || last_is_list_range) {
            LValueHelper lvh(xsink);
            // for_remove=true on the parent walk: don't vivify intermediate
            // containers just to remove a nested key that doesn't exist.
            if (lvh.navigatePath(path_copy.data(), path_copy.size() - 1, true)) {
                // Parent slot missing (or an exception was raised): nothing
                // to remove in either case.
                return toBits(QoreValue());
            }
            QoreValue container = lvh.getValue();
            qore_type_t ct = container.getType();
            if ((last_is_hash_slice || last_is_hash_list)
                    && (ct == NT_HASH || ct == NT_OBJECT || ct == NT_WEAKREF)) {
                res = executeLVHashKeySliceRemove(lvh, ct, last_step,
                        inst->unary_op, xsink);
                handled_multistep_remove = true;
            } else if (last_is_hash && ct == NT_HASH) {
                lvh.ensureUnique();
                QoreHashNode* h = lvh.getValue().get<QoreHashNode>();
                res = h->takeKeyValue(last_step.name.c_str());
                finish_delete_result(res);
                handled_multistep_remove = true;
            } else if (last_is_hash && (ct == NT_OBJECT || ct == NT_WEAKREF)) {
                QoreObject* o = ct == NT_OBJECT
                    ? lvh.getValue().get<QoreObject>()
                    : lvh.getValue().get<const WeakReferenceNode>()->get();
                if (o) {
                    res = qore_object_private::takeMember(*o, lvh, last_step.name.c_str());
                    finish_delete_result(res);
                }
                handled_multistep_remove = true;
            } else if (last_is_list && ct == NT_LIST) {
                lvh.ensureUnique();
                QoreListNode* l = lvh.getValue().get<QoreListNode>();
                int64_t idx = last_step.index;
                if (runtime_check_parse_option(PO_NEGATIVE_OFFSETS) && idx < 0) {
                    idx += static_cast<int64_t>(l->size());
                }
                if (idx >= 0 && static_cast<size_t>(idx) < l->size()) {
                    if (inst->unary_op == LVUnaryOp::Remove) {
                        res = l->retrieveEntry(static_cast<size_t>(idx)).refSelf();
                    }
                    l->setEntry(static_cast<size_t>(idx), QoreValue(), xsink);
                }
                handled_multistep_remove = true;
            } else if (last_is_hash_slice && (ct == NT_HASH || ct == NT_OBJECT || ct == NT_WEAKREF)) {
                res = executeLVHashKeySliceRemove(lvh, ct, last_step,
                        inst->unary_op, xsink);
                handled_multistep_remove = true;
            } else if (last_is_list_slice
                    && (ct == NT_LIST || ct == NT_STRING || ct == NT_BINARY)) {
                res = executeLVListIndexSliceRemove(lvh, ct, last_step,
                        inst->unary_op, xsink);
                handled_multistep_remove = true;
            } else if (last_is_list_range
                    && (ct == NT_LIST || ct == NT_STRING || ct == NT_BINARY)) {
                res = executeLVListRangeSliceRemove(lvh, ct, last_step,
                        inst->unary_op, xsink);
                handled_multistep_remove = true;
            } else if (ct == NT_NOTHING) {
                // Parent slot is empty: there is nothing to remove.
                return toBits(QoreValue());
            }
            // Fall through for NT_OBJECT / NT_WEAKREF / other parent types:
            // the legacy navigate + lvh.remove() path handles them.
        }
    }

    if (handled_multistep_remove) {
        if (*xsink) {
            return toBits(QoreValue());
        }
        if (!inst->ref_rv) {
            res.discard(xsink);
            return toBits(QoreValue());
        }
        return toBits(res);
    }

    if (is_remove) {
        // Single-step remove/delete of a reference local must act on the
        // referenced lvalue, not clear the reference variable itself.
        if (path_copy.size() == 1
                && (path_copy[0].kind == LVPathStepKind::LocalVar
                    || path_copy[0].kind == LVPathStepKind::ClosureVar)) {
            const LocalVar* lv = static_cast<const LocalVar*>(path_copy[0].ref_ptr);
            ReferenceNode* ref = nullptr;
            if (lv) {
                if (!lv->closureUse()) {
                    LocalVarValue* lvv = thread_try_find_lvar(lv);
                    if (lvv && lvv->val.getType() == NT_REFERENCE) {
                        ref = reinterpret_cast<ReferenceNode*>(lvv->val.v.n);
                    }
                } else {
                    ClosureVarValue* cvv = nullptr;
                    if (thread_has_runtime_closure_env()) {
                        cvv = thread_try_find_closure_var_in_current_frame(lv->getName());
                        if (!cvv) {
                            cvv = thread_try_get_runtime_closure_var(lv);
                        }
                    }
                    if (!cvv) {
                        cvv = thread_try_find_closure_var(lv->getName());
                        if (!cvv) {
                            cvv = thread_try_get_runtime_closure_var(lv);
                        }
                    }
                    if (cvv && cvv->val.getType() == NT_REFERENCE) {
                        ref = reinterpret_cast<ReferenceNode*>(cvv->val.v.n);
                    }
                }
            }
            if (ref) {
                bool is_delete = (inst->unary_op == LVUnaryOp::Delete);
                LValueRemoveHelper lvrh(*ref, xsink, is_delete);
                if (lvrh && !*xsink) {
                    if (is_delete) {
                        lvrh.deleteLValue();
                        res = QoreValue();
                    } else {
                        res = lvrh.removeValue();
                    }
                }
                if (*xsink) {
                    return toBits(QoreValue());
                }
                if (!inst->ref_rv) {
                    res.discard(xsink);
                    return toBits(QoreValue());
                }
                return toBits(res);
            }
        }

        {
            LValueHelper lvh(xsink);
            if (lvh.navigatePath(path_copy.data(), path_copy.size(), true)) {
                return toBits(QoreValue());
            }
            if (inst->unary_op == LVUnaryOp::Remove) {
                bool static_assignment = false;
                res = lvh.remove(static_assignment);
                if (static_assignment) {
                    res = QoreValue();
                }
            } else {
                bool static_assignment = false;
                res = lvh.remove(static_assignment);
                if (res.getType() == NT_OBJECT) {
                    QoreObject* o = res.get<QoreObject>();
                    if (!o->isSystemObject()) {
                        o->doDelete(xsink);
                    }
                    if (!static_assignment) {
                        res.discard(xsink);
                    }
                    res = QoreValue();
                } else if (static_assignment) {
                    res = QoreValue();
                }
            }
        }
    } else {
        // Pop, Shift, Trim, Chomp should not vivify containers (use for_remove=true)
        bool no_vivify = (inst->unary_op == LVUnaryOp::Pop
            || inst->unary_op == LVUnaryOp::Shift
            || inst->unary_op == LVUnaryOp::Trim
            || inst->unary_op == LVUnaryOp::Chomp);
        LValueHelper lvh(xsink);
        if (lvh.navigatePath(path_copy.data(), path_copy.size(), no_vivify)) {
            return toBits(QoreValue());
        }
        switch (inst->unary_op) {
            case LVUnaryOp::PreInc: {
                qore_type_t t = lvh.getType();
                if (t == NT_NUMBER) {
                    lvh.preIncrementNumber();
                    res = lvh.getReferencedValue();
                } else if (t == NT_FLOAT) {
                    res = lvh.preIncrementFloat();
                } else {
                    res = lvh.preIncrementBigInt();
                }
                break;
            }
            case LVUnaryOp::PreDec: {
                qore_type_t t = lvh.getType();
                if (t == NT_NUMBER) {
                    lvh.preDecrementNumber();
                    res = lvh.getReferencedValue();
                } else if (t == NT_FLOAT) {
                    res = lvh.preDecrementFloat();
                } else {
                    res = lvh.preDecrementBigInt();
                }
                break;
            }
            case LVUnaryOp::PostInc: {
                qore_type_t t = lvh.getType();
                if (t == NT_NUMBER) {
                    QoreNumberNode* n = lvh.postIncrementNumber(true);
                    if (n) {
                        res = n;
                    }
                } else if (t == NT_FLOAT) {
                    res = lvh.postIncrementFloat();
                } else {
                    res = lvh.postIncrementBigInt();
                }
                break;
            }
            case LVUnaryOp::PostDec: {
                qore_type_t t = lvh.getType();
                if (t == NT_NUMBER) {
                    QoreNumberNode* n = lvh.postDecrementNumber(true);
                    if (n) {
                        res = n;
                    }
                } else if (t == NT_FLOAT) {
                    res = lvh.postDecrementFloat();
                } else {
                    res = lvh.postDecrementBigInt();
                }
                break;
            }
            case LVUnaryOp::Shift:
                if (lvh.getType() == NT_LIST) {
                    lvh.ensureUnique();
                    QoreListNode* l = lvh.getValue().get<QoreListNode>();
                    if (l && l->size() > 0) {
                        res = l->shift();
                    }
                }
                break;
            case LVUnaryOp::Pop:
                if (lvh.getType() == NT_LIST) {
                    lvh.ensureUnique();
                    QoreListNode* l = lvh.getValue().get<QoreListNode>();
                    if (l && l->size() > 0) {
                        res = l->pop();
                    }
                }
                break;
            case LVUnaryOp::Trim:
                if (lvh.getType() == NT_STRING) {
                    lvh.ensureUnique();
                    QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
                    if (str) {
                        str->trim(xsink);
                    }
                    // Mirror QoreTrimOperatorNode::evalImpl: return the trimmed
                    // value so callers like `return trim ct;` see the result.
                    res = lvh.getReferencedValue();
                } else if (lvh.getType() == NT_LIST) {
                    lvh.ensureUnique();
                    QoreListNode* l = lvh.getValue().get<QoreListNode>();
                    if (l) {
                        qore_list_private* ll = qore_list_private::get(*l);
                        for (size_t i = 0, e = l->size(); i < e; ++i) {
                            QoreValue& v = ll->getEntryReference(i);
                            if (v.getType() == NT_STRING) {
                                ensure_unique(v, xsink);
                                if (*xsink || v.get<QoreStringNode>()->trim(xsink)) {
                                    return toBits(QoreValue());
                                }
                            }
                        }
                    }
                    res = lvh.getReferencedValue();
                } else if (lvh.getType() == NT_HASH) {
                    lvh.ensureUnique();
                    QoreHashNode* h = lvh.getValue().get<QoreHashNode>();
                    if (h) {
                        HashIterator hi(h);
                        while (hi.next()) {
                            if (hi.get().getType() == NT_STRING) {
                                QoreValue& v = (*qhi_priv::get(hi)->i)->val;
                                ensure_unique(v, xsink);
                                if (*xsink || v.get<QoreStringNode>()->trim(xsink)) {
                                    return toBits(QoreValue());
                                }
                            }
                        }
                    }
                    res = lvh.getReferencedValue();
                }
                break;
            case LVUnaryOp::Chomp:
                if (lvh.getType() == NT_STRING) {
                    lvh.ensureUnique();
                    QoreStringNode* str = lvh.getValue().get<QoreStringNode>();
                    if (str) {
                        // Mirror QoreChompOperatorNode::evalImpl: return count.
                        res = QoreValue(static_cast<int64>(str->chomp()));
                    }
                } else if (lvh.getType() == NT_LIST) {
                    lvh.ensureUnique();
                    QoreListNode* l = lvh.getValue().get<QoreListNode>();
                    if (l) {
                        int64 count = 0;
                        qore_list_private* ll = qore_list_private::get(*l);
                        for (size_t i = 0, e = l->size(); i < e; ++i) {
                            QoreValue& v = ll->getEntryReference(i);
                            if (v.getType() == NT_STRING) {
                                ensure_unique(v, xsink);
                                if (*xsink) {
                                    return toBits(QoreValue());
                                }
                                count += static_cast<int64>(v.get<QoreStringNode>()->chomp());
                            }
                        }
                        res = QoreValue(count);
                    }
                } else if (lvh.getType() == NT_HASH) {
                    lvh.ensureUnique();
                    QoreHashNode* h = lvh.getValue().get<QoreHashNode>();
                    if (h) {
                        int64 count = 0;
                        HashIterator hi(h);
                        while (hi.next()) {
                            if (hi.get().getType() == NT_STRING) {
                                QoreValue& v = (*qhi_priv::get(hi)->i)->val;
                                ensure_unique(v, xsink);
                                if (*xsink) {
                                    return toBits(QoreValue());
                                }
                                count += static_cast<int64>(v.get<QoreStringNode>()->chomp());
                            }
                        }
                        res = QoreValue(count);
                    }
                }
                break;
            default: break;
        }
    }
    if (*xsink) {
        return toBits(QoreValue());
    }
    if (is_remove && !inst->ref_rv) {
        res.discard(xsink);
        return toBits(QoreValue());
    }
    return toBits(res);
}

extern "C" DLLEXPORT uint64_t qore_rt_lv_path_binary_mut(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    if (*xsink) {
        return toBits(QoreValue());
    }
    std::vector<LVPathStep> path_copy;
    patchLVPath(path_copy, inst, dyn_vals);

    QoreValue rhs = fromBits(rhs_bits);

    LValueHelper lvh(xsink);
    if (lvh.navigatePath(path_copy.data(), path_copy.size(), false)) {
        return toBits(QoreValue());
    }

    QoreValue res;
    switch (inst->binary_mut_op) {
        case LVBinaryMutOp::Push:
        case LVBinaryMutOp::Unshift: {
            // Auto-vivify NOTHING to empty list (mirror IR interpreter).
            // Without this, ensureUnique() null-derefs on a NOTHING slot and
            // `push list_member, val` crashes for members declared without an
            // initializer (default value NOTHING, not []).
            if (lvh.getType() == NT_NOTHING) {
                const QoreTypeInfo* vti = lvh.getTypeInfo();
                if (QoreTypeInfo::parseAcceptsReturns(vti, NT_LIST)) {
                    const QoreTypeInfo* lti = vti == autoTypeInfo
                        ? autoTypeInfo
                        : QoreTypeInfo::getReturnComplexListOrNothing(vti);
                    if (lvh.assign(new QoreListNode(lti))) {
                        break;
                    }
                }
            }
            if (lvh.getType() != NT_LIST) {
                if (runtime_check_parse_option(PO_STRICT_ARGS)) {
                    xsink->raiseException(
                        inst->binary_mut_op == LVBinaryMutOp::Push
                            ? "PUSH-ERROR" : "UNSHIFT-ERROR",
                        "the lvalue argument is type \"%s\"; expecting \"list\"",
                        lvh.getTypeName());
                }
                break;
            }
            lvh.ensureUnique();
            QoreListNode* l = lvh.getValue().get<QoreListNode>();
            if (inst->binary_mut_op == LVBinaryMutOp::Push) {
                l->push(rhs.refSelf(), xsink);
            } else {
                l->insert(rhs.refSelf(), xsink);
            }
            break;
        }
        case LVBinaryMutOp::RegexSubst: {
            if (!lvh.checkType(NT_STRING)) {
                break;
            }
            if (inst->pattern_expr.hasNode()) {
                auto* regex_op = dynamic_cast<const QoreRegexSubstOperatorNode*>(
                    inst->pattern_expr.getInternalNode());
                if (regex_op && regex_op->getRegexSubst()) {
                    QoreStringNodeValueHelper str(lvh.getValue());
                    QoreStringNode* nv = regex_op->getRegexSubst()->exec(*str, xsink);
                    if (!*xsink && nv) {
                        lvh.assign(nv);
                        if (inst->ref_rv) {
                            res = nv->refSelf();
                        }
                    }
                }
            }
            break;
        }
        case LVBinaryMutOp::Transliterate: {
            if (!lvh.checkType(NT_STRING)) {
                break;
            }
            if (inst->pattern_expr.hasNode()) {
                auto* trans_op = dynamic_cast<const QoreTransliterationOperatorNode*>(
                    inst->pattern_expr.getInternalNode());
                if (trans_op && trans_op->getTransliteration()) {
                    QoreStringNodeValueHelper str(lvh.getValue());
                    QoreStringNode* nv = trans_op->getTransliteration()->exec(*str, xsink);
                    if (!*xsink && nv) {
                        lvh.assign(nv);
                        if (inst->ref_rv) {
                            res = nv->refSelf();
                        }
                    }
                }
            }
            break;
        }
    }
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(res);
}

extern "C" DLLEXPORT uint64_t qore_rt_lv_path_ternary(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
        uint64_t a_bits, uint64_t b_bits, uint64_t c_bits,
        ExceptionSink* xsink) {
    // Resolve dynamic key/index operands
    uint32_t dyn_idx = 0;
    for (auto& step : inst->path) {
        if (step.kind == LVPathStepKind::HashKey && step.operand_idx != UINT32_MAX) {
            QoreValue key_val = fromBits(dyn_vals[dyn_idx++]);
            QoreStringValueHelper key_str(key_val);
            step.name = key_str->c_str();
        } else if (step.kind == LVPathStepKind::ListIndex && step.operand_idx != UINT32_MAX) {
            QoreValue idx_val = fromBits(dyn_vals[dyn_idx++]);
            step.index = idx_val.getAsBigInt();
        }
    }
    QoreValue offset_val = fromBits(a_bits);
    QoreValue length_val = fromBits(b_bits);
    QoreValue replacement_val = fromBits(c_bits);
    // For extract without replacement, avoid vivification
    bool no_vivify = (inst->ternary_op == LVTernaryOp::Extract && replacement_val.isNothing());
    ReferenceHolder<QoreListNode> removed_list(xsink);
    LValueHelper lvh(xsink);
    if (lvh.navigatePath(inst->path.data(), inst->path.size(), no_vivify)) {
        if (no_vivify && !*xsink) {
            return toBits(QoreValue());
        }
        return toBits(QoreValue());
    }
    QoreValue res;
    qore_type_t vt = lvh.getType();
    if (vt == NT_NOTHING) {
        const QoreTypeInfo* ti = lvh.getTypeInfo();
        if (ti == softListTypeInfo || ti == listTypeInfo || ti == stringTypeInfo
                || ti == softStringTypeInfo) {
            if (!lvh.assign(QoreTypeInfo::getDefaultQoreValue(ti))) {
                vt = lvh.getType();
            }
        }
    }
    if (vt == NT_NOTHING) {
        // Nothing to extract/splice — return NOTHING
    } else if (vt != NT_LIST && vt != NT_STRING && vt != NT_BINARY) {
        xsink->raiseException("EXTRACT-ERROR",
            "first (lvalue) argument to the extract operator is not a list, "
            "string, or binary object");
    } else {
        lvh.ensureUnique();
        size_t offset = static_cast<size_t>(offset_val.getAsBigInt());
        if (inst->ternary_op == LVTernaryOp::Splice) {
            if (vt == NT_LIST) {
                QoreListNode* vl = lvh.getValue().get<QoreListNode>();
                if (length_val.isNothing() && replacement_val.isNothing()) {
                    removed_list = vl->splice(offset);
                } else {
                    size_t length = static_cast<size_t>(length_val.getAsBigInt());
                    if (replacement_val.isNothing()) {
                        removed_list = vl->splice(offset, length);
                    } else {
                        removed_list = vl->splice(offset, length, replacement_val, xsink);
                    }
                }
            } else if (vt == NT_STRING) {
                QoreStringNode* vs = lvh.getValue().get<QoreStringNode>();
                if (length_val.isNothing() && replacement_val.isNothing()) {
                    vs->splice(offset, xsink);
                } else {
                    size_t length = static_cast<size_t>(length_val.getAsBigInt());
                    if (replacement_val.isNothing()) {
                        vs->splice(offset, length, xsink);
                    } else {
                        vs->splice(offset, length, replacement_val, xsink);
                    }
                }
            } else { // NT_BINARY
                BinaryNode* b = lvh.getValue().get<BinaryNode>();
                if (length_val.isNothing() && replacement_val.isNothing()) {
                    b->splice(offset, b->size());
                } else {
                    size_t length = static_cast<size_t>(length_val.getAsBigInt());
                    if (replacement_val.isNothing()) {
                        b->splice(offset, length);
                    } else {
                        if (replacement_val.getType() == NT_BINARY) {
                            const BinaryNode* b1 = replacement_val.get<const BinaryNode>();
                            b->splice(offset, length, b1->getPtr(), b1->size());
                        } else {
                            QoreStringNodeValueHelper sv(replacement_val);
                            if (!sv->strlen()) {
                                b->splice(offset, length);
                            } else {
                                b->splice(offset, length, sv->getBuffer(), sv->size());
                            }
                        }
                    }
                }
            }
            if (inst->ref_rv && !*xsink) {
                res = lvh.getReferencedValue();
            }
        } else {
            if (vt == NT_LIST) {
                QoreListNode* vl = lvh.getValue().get<QoreListNode>();
                if (length_val.isNothing() && replacement_val.isNothing()) {
                    res = vl->extract(offset);
                } else {
                    size_t length = static_cast<size_t>(length_val.getAsBigInt());
                    if (replacement_val.isNothing()) {
                        res = vl->extract(offset, length);
                    } else {
                        res = vl->extract(offset, length, replacement_val, xsink);
                    }
                }
            } else if (vt == NT_STRING) {
                QoreStringNode* vs = lvh.getValue().get<QoreStringNode>();
                if (length_val.isNothing() && replacement_val.isNothing()) {
                    res = vs->extract(offset, xsink);
                } else {
                    size_t length = static_cast<size_t>(length_val.getAsBigInt());
                    if (replacement_val.isNothing()) {
                        res = vs->extract(offset, length, xsink);
                    } else {
                        res = vs->extract(offset, length, replacement_val, xsink);
                    }
                }
            } else { // NT_BINARY
                BinaryNode* b = lvh.getValue().get<BinaryNode>();
                BinaryNode* bout = new BinaryNode;
                if (length_val.isNothing() && replacement_val.isNothing()) {
                    b->splice(offset, b->size(), bout);
                } else {
                    size_t length = static_cast<size_t>(length_val.getAsBigInt());
                    if (replacement_val.isNothing()) {
                        b->splice(offset, length, bout);
                    } else {
                        if (replacement_val.getType() == NT_BINARY) {
                            const BinaryNode* b1 = replacement_val.get<const BinaryNode>();
                            b->splice(offset, length, b1->getPtr(), b1->size(), bout);
                        } else {
                            QoreStringNodeValueHelper sv(replacement_val);
                            if (!sv->strlen()) {
                                b->splice(offset, length, bout);
                            } else {
                                b->splice(offset, length, sv->getBuffer(), sv->size(), bout);
                            }
                        }
                    }
                }
                res = bout;
            }
        }
    }
    if (*xsink) {
        res.discard(xsink);
        return toBits(QoreValue());
    }
    if (!inst->ref_rv) {
        res.discard(xsink);
        return toBits(QoreValue());
    }
    return toBits(res);
}

// AOT wrappers
extern "C" DLLEXPORT uint64_t qore_rt_lv_path_assign_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    if (!ctx || slot < 0 || slot >= ctx->num_lv_path_insts) {
        xsink->raiseException("AOT-INTERNAL-ERROR",
            "LValuePath assign: slot %d out of range (num_lv_path_insts=%d)",
            slot, ctx ? ctx->num_lv_path_insts : -1);
        return toBits(QoreValue());
    }
    return qore_rt_lv_path_assign(ctx->lv_path_insts[slot], dyn_vals, rhs_bits, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lv_path_compound_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    if (!ctx || slot < 0 || slot >= ctx->num_lv_path_insts) {
        xsink->raiseException("AOT-INTERNAL-ERROR",
            "LValuePath op: slot %d out of range (num_lv_path_insts=%d)",
            slot, ctx ? ctx->num_lv_path_insts : -1);
        return toBits(QoreValue());
    }
    return qore_rt_lv_path_compound(ctx->lv_path_insts[slot], dyn_vals, rhs_bits, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lv_path_unary_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
        ExceptionSink* xsink) {
    if (!ctx || slot < 0 || slot >= ctx->num_lv_path_insts) {
        xsink->raiseException("AOT-INTERNAL-ERROR",
            "LValuePath op: slot %d out of range (num_lv_path_insts=%d)",
            slot, ctx ? ctx->num_lv_path_insts : -1);
        return toBits(QoreValue());
    }
    return qore_rt_lv_path_unary(ctx->lv_path_insts[slot], dyn_vals, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lv_path_binary_mut_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    if (!ctx || slot < 0 || slot >= ctx->num_lv_path_insts) {
        xsink->raiseException("AOT-INTERNAL-ERROR",
            "LValuePath op: slot %d out of range (num_lv_path_insts=%d)",
            slot, ctx ? ctx->num_lv_path_insts : -1);
        return toBits(QoreValue());
    }
    return qore_rt_lv_path_binary_mut(ctx->lv_path_insts[slot], dyn_vals, rhs_bits, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lv_path_ternary_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
        uint64_t a_bits, uint64_t b_bits, uint64_t c_bits,
        ExceptionSink* xsink) {
    if (!ctx || slot < 0 || slot >= ctx->num_lv_path_insts) {
        xsink->raiseException("AOT-INTERNAL-ERROR",
            "LValuePath op: slot %d out of range (num_lv_path_insts=%d)",
            slot, ctx ? ctx->num_lv_path_insts : -1);
        return toBits(QoreValue());
    }
    return qore_rt_lv_path_ternary(ctx->lv_path_insts[slot], dyn_vals, a_bits, b_bits, c_bits, xsink);
}

// --- Phase 2B Step 5: Lvalue ops category throwing wrappers ---

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_load_throwing(
        uint64_t lvalue_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_load(lvalue_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_store_throwing(
        uint64_t lvalue_bits, uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_store(lvalue_bits, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_store_weak_throwing(
        uint64_t lvalue_bits, uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_store_weak(lvalue_bits, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_unary_throwing(
        int opcode, uint64_t lvalue_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_unary(opcode, lvalue_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_binary_throwing(
        int opcode, uint64_t lvalue_bits, uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_binary(opcode, lvalue_bits, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_ternary_throwing(
        int opcode, uint64_t lvalue_bits, uint64_t first_bits, uint64_t second_bits,
        uint64_t third_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_ternary(opcode, lvalue_bits, first_bits,
            second_bits, third_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_ternary_throwing(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
        uint64_t a_bits, uint64_t b_bits, uint64_t c_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_ternary(inst, dyn_vals, a_bits, b_bits,
            c_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_assign_throwing(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_assign(inst, dyn_vals, rhs_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_compound_throwing(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_compound(inst, dyn_vals, rhs_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_unary_throwing(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_unary(inst, dyn_vals, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_binary_mut_throwing(
        QoreIRLValuePathInstruction* inst, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_binary_mut(inst, dyn_vals, rhs_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_load_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_load_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_store_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_store_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_store_weak_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_store_weak_aot(ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_unary_aot_throwing(
        int op, QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_unary_aot(op, ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_binary_aot_throwing(
        int op, QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_binary_aot(op, ctx, idx, val, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lvalue_ternary_aot_throwing(
        int op, QoreAOTContext* ctx, int32_t idx, uint64_t first, uint64_t second,
        uint64_t third, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lvalue_ternary_aot(op, ctx, idx, first, second, third,
            xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_ternary_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
        uint64_t a_bits, uint64_t b_bits, uint64_t c_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_ternary_aot(ctx, slot, dyn_vals, a_bits,
            b_bits, c_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_assign_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_assign_aot(ctx, slot, dyn_vals, rhs_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_compound_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_compound_aot(ctx, slot, dyn_vals, rhs_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_unary_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_unary_aot(ctx, slot, dyn_vals, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_lv_path_binary_mut_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* dyn_vals,
        uint64_t rhs_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_lv_path_binary_mut_aot(ctx, slot, dyn_vals, rhs_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

static uint64_t qore_rt_call_static_method_direct_impl(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const QoreTypeInfo* receiver_type_info = nullptr);

extern "C" DLLEXPORT uint64_t qore_rt_call_with_args_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, nullptr, nargs, xsink,
            "qore_rt_call_with_args_aot", "call", "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.is_static_method && target.method) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_with_args_aot", "static_method", "use");
        return qore_rt_call_static_method_direct_impl(target.method, target.variant,
                args, nullptr, nargs, xsink, target.receiver_type_info);
    }
    if (target.method) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_with_args_aot", "method", "use");
        if (target.is_self_method) {
            return qore_rt_call_self_method_dispatch_impl(target, args, nullptr,
                nargs, xsink);
        }
        return qore_rt_call_method_direct(target.method, args, nargs, xsink);
    }
    if (target.func) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_with_args_aot", "function", "use");
        if (target.variant) {
            if (!qore_rt_user_fast_call_eligible(target.variant)) {
                return qore_rt_call_function_direct(target.func, target.variant,
                    target.pgm, args, nargs, xsink);
            }
            return qore_rt_call_fast(target.func, target.variant, target.pgm, args, nargs, xsink);
        }
        return qore_rt_call_function_dynamic(target.func, target.pgm, args, nargs, xsink);
    }
    if (qore_rt_is_implicit_self_copy_slot(ctx, slot)) {
        return qore_rt_call_implicit_self_copy_aot(ctx, slot, nargs, xsink);
    }
    return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
        "qore_rt_call_with_args_aot", "call", "missing pre-resolved call target");
}

extern "C" DLLEXPORT uint64_t qore_rt_call_with_args_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, arg_cleanups, nargs, xsink,
            "qore_rt_call_with_args_aot_consume_args", "call", "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.is_static_method && target.method) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_with_args_aot_consume_args",
            "static_method", "use");
        return qore_rt_call_static_method_direct_impl(target.method,
                target.variant, args, arg_cleanups, nargs, xsink,
                target.receiver_type_info);
    }
    if (target.method) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_with_args_aot_consume_args", "method", "use");
        if (target.is_self_method) {
            return qore_rt_call_self_method_dispatch_impl(target, args,
                arg_cleanups, nargs, xsink);
        }
        return qore_rt_call_method_direct_impl(target.method, args, arg_cleanups,
            nargs, xsink);
    }
    if (target.func) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_with_args_aot_consume_args",
            "function", "use");
        if (target.variant) {
            return qore_rt_call_function_direct_impl(target.func, target.variant,
                target.pgm, args, arg_cleanups, nargs, xsink);
        }
        return qore_rt_call_function_dynamic_impl(target.func, target.pgm, args,
            arg_cleanups, nargs, xsink);
    }
    if (qore_rt_is_implicit_self_copy_slot(ctx, slot)) {
        if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
            return toBits(QoreValue());
        }
        return qore_rt_call_implicit_self_copy_aot(ctx, slot, nargs, xsink);
    }
    return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
        "qore_rt_call_with_args_aot_consume_args", "call", "missing pre-resolved call target");
}

static uint64_t qore_rt_call_direct_aot_impl(QoreAOTContext* ctx, int32_t slot,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, arg_cleanups, nargs, xsink,
            "qore_rt_call_direct_aot", "function", "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }

    // Use pre-resolved call target (populated during buildAOTContext) to avoid per-call dynamic_cast
    const QoreAOTCallTarget& target = ctx->call_targets[slot];

    // Fast path: pre-resolved user variant — inline the call_fast logic to avoid double
    // check_stack and extra function call overhead (critical for tight recursive calls)
    const UserVariantBase* resolved_uvb = target.uvb;
    if (!resolved_uvb && target.variant) {
        resolved_uvb = target.variant->getUserVariantBase();
    }
    if (resolved_uvb && resolved_uvb->isStaticallyFastCallEligible()) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_direct_aot", "function", "use");
        const UserVariantBase* uvb = resolved_uvb;

        const UserSignature* sig = uvb->getUserSignature();
        unsigned num_params = sig->numParams();

        // Capture caller's program before ptcch switch.
        QoreProgram* caller_pgm = getProgram();
        QoreProgram* exec_pgm = target.pgm ? target.pgm : uvb->pgm;

        // Set up program thread context (only if program differs from caller's program)
        std::optional<ProgramThreadCountContextHelper> ptcch;
        if (exec_pgm != caller_pgm) {
            ptcch.emplace(xsink, exec_pgm, true);
            if (*xsink) {
                return toBits(QoreValue());
            }
        }
        // This is a normal function call, not a closure invocation.  Do not let a
        // caller closure's captured LocalVar* map shadow the callee's own closure-use
        // locals when the callee is entered through the AOT direct-call path.
        ThreadSafeLocalVarRuntimeEnvironmentHelper closure_env_clear(nullptr);

        ThreadFrameBoundaryHelper tfbh(true);

        // Instantiate parameter locals directly from NaN-boxed args
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            return toBits(QoreValue());
        }

        // Build argv for excess arguments (varargs)
        ReferenceHolder<QoreListNode> argv(xsink);
        if (nargs > (int)num_params) {
            argv = new QoreListNode(autoTypeInfo);
            qore_list_private* argv_priv = qore_list_private::get(**argv);
            argv_priv->reserve(nargs - num_params);
            for (int i = num_params; i < nargs; ++i) {
                QoreValue val = fromBits(args[i]);
                if (val.hasNode()) {
                    val.refSelf();
                }
                argv_priv->pushIntern(val);
            }
        }
        if (sig->argvid) {
            sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
        }
        if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
            if (sig->argvid) {
                sig->argvid->uninstantiate(xsink);
            }
            for (int i = static_cast<int>(num_params) - 1; i >= 0; --i) {
                sig->lv[i]->uninstantiate(xsink);
            }
            return toBits(QoreValue());
        }

        const QoreIRFunction* ir = uvb->getCachedIR();
        const std::string& call_name = ir ? ir->getDisplayName() : jit_empty_call_name;

        QoreValue val{};
        {
            ArgvContextHelper argv_helper(argv.release(), xsink);
            if (uvb->hasCachedFunction()) {
                execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                    return uvb->execCachedFunction(xs, inv);
                }, val, xsink, caller_pgm, nullptr, exec_pgm);
            } else if (uvb->getCachedIR()) {
                const QoreIRFunction* callee_ir = uvb->getCachedIR();
                execJITWithDeopt(uvb, call_name, [callee_ir, uvb, exec_pgm](ExceptionSink* xs, bool& inv) -> uint64_t {
                    QoreValue ir_return_value;
                    bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                        nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                        uvb->getStatementBlock(), exec_pgm);
                    if (!ok && !*xs) {
                        inv = true;
                        return 0;
                    }
                    return toBits(ir_return_value);
                }, val, xsink, caller_pgm, nullptr, exec_pgm);
            } else {
                execJITWithDeopt(uvb, call_name, [uvb, sig](ExceptionSink* xs, bool& inv) -> uint64_t {
                    QoreValue ast_return_value;
                    StatementBlock* stmts = uvb->getStatementBlock();
                    if (stmts) {
                        const QoreTypeInfo* old_rti = saveReturnTypeInfo(qore_rt_get_effective_return_type(sig));
                        ast_return_value = stmts->exec(xs);
                        saveReturnTypeInfo(old_rti);
                    }
                    return toBits(ast_return_value);
                }, val, xsink, caller_pgm, nullptr, exec_pgm);
            }
        }

        if (sig->argvid) {
            sig->argvid->uninstantiate(xsink);
        }
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }

        if (!*xsink) {
            const QoreTypeInfo* rt = qore_rt_get_effective_return_type(sig);
            if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
                QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
                if (*xsink) {
                    xsink->overrideLocation(*sig->getParseLocation());
                    xsink->appendLastDescription(": block missing return statement");
                }
            } else {
                QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
            }
        }

        return toBits(val);
    }

    // Medium path: pre-resolved function but no user variant (builtin)
    if (target.func) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_direct_aot", "function", "use");
        if (target.variant) {
            if (arg_cleanups) {
                return qore_rt_call_function_direct_impl(target.func,
                        target.variant, target.pgm, args, arg_cleanups, nargs,
                        xsink);
            }
            if (!qore_rt_user_fast_call_eligible(target.variant)) {
                return qore_rt_call_function_direct(target.func, target.variant,
                        target.pgm, args, nargs, xsink);
            }
            return qore_rt_call_fast(target.func, target.variant, target.pgm, args, nargs, xsink);
        }
        // No variant resolved — use dynamic dispatch for proper overload resolution
        return qore_rt_call_function_dynamic_impl(target.func, target.pgm, args,
                arg_cleanups, nargs, xsink);
    }

    return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
        "qore_rt_call_direct_aot", "function", "missing pre-resolved direct call target");
}

extern "C" DLLEXPORT QoreAOTContext* qore_rt_get_aot_call_target_context(
        QoreAOTContext* ctx, int32_t slot, ExceptionSink* xsink) {
    if (!ctx || slot < 0 || slot >= ctx->num_exprs || !ctx->call_targets) {
        if (xsink) {
            xsink->raiseException("AOT-ERROR",
                "invalid AOT direct-call slot %d while resolving callee context", slot);
        }
        return nullptr;
    }

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    const UserVariantBase* uvb = target.uvb;
    if (!uvb && target.variant) {
        uvb = target.variant->getUserVariantBase();
    }
    if (!uvb || !uvb->isStaticallyFastCallEligible() || !uvb->hasCachedAOT()) {
        if (xsink) {
            xsink->raiseException("AOT-ERROR",
                "missing cached AOT context for direct-call slot %d", slot);
        }
        return nullptr;
    }
    return uvb->getCachedAOTContext();
}

extern "C" DLLEXPORT QoreAOTContext* qore_rt_try_get_aot_call_target_context(
        QoreAOTContext* ctx, int32_t slot) {
    return qore_rt_get_aot_call_target_context(ctx, slot, nullptr);
}

extern "C" DLLEXPORT int qore_rt_object_is_valid(uint64_t value) {
    QoreValue receiver = fromBits(value);
    return receiver.getType() == NT_OBJECT && receiver.get<QoreObject>()->isValid();
}

extern "C" DLLEXPORT uint64_t qore_rt_call_direct_aot(QoreAOTContext* ctx,
        int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    return qore_rt_call_direct_aot_impl(ctx, slot, args, nullptr, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_direct_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    return qore_rt_call_direct_aot_impl(ctx, slot, args, arg_cleanups, nargs,
            xsink);
}

// Forward declarations for AOT helpers defined later in this file that
// the throwing wrappers below need to call.
extern "C" DLLEXPORT uint64_t qore_rt_call_method_direct_aot(QoreAOTContext* ctx,
        int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_rt_call_method_fast_aot(QoreAOTContext* ctx,
        int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_rt_call_method_direct_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_rt_call_method_fast_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_fast_aot(QoreAOTContext* ctx,
        int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_direct_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_pseudo_method_direct_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink);

//! Throwing variant of qore_rt_call_direct_aot for the C++ EH prototype.
/** Callers emit CreateInvoke on this function and wire the unwind edge to a
    landing pad that cleans up live temps and returns NOTHING. On exception
    it throws QoreJITException which LLVM's Itanium unwinder propagates. On
    success it returns the call result exactly like qore_rt_call_direct_aot.

    This wrapper exists so we can roll out EH-style invoke sites incrementally
    without breaking every existing CreateCall caller of the base helper.
    Each throwing variant is a thin tail-call style wrapper: forward to the
    base helper, then throw if xsink is set.
*/
extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_direct_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_direct_aot(ctx, slot, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_direct_aot_consume_args_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_direct_aot_consume_args(ctx, slot, args,
            arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_with_args_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_with_args_aot(ctx, slot, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_with_args_aot_consume_args_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_with_args_aot_consume_args(ctx, slot, args,
            arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_static_method_direct_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_static_method_direct_aot(ctx, slot, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_static_method_direct_aot_consume_args_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_static_method_direct_aot_consume_args(ctx,
            slot, args, arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_method_direct_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_method_direct_aot(ctx, slot, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_method_direct_aot_consume_args_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_method_direct_aot_consume_args(ctx, slot,
            args, arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_method_fast_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_method_fast_aot(ctx, slot, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_method_fast_aot_consume_args_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_method_fast_aot_consume_args(ctx, slot,
            args, arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_static_method_fast_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_static_method_fast_aot(ctx, slot, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_dot_eval_method_direct_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_dot_eval_method_direct_aot(ctx, slot, base_bits,
            args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_dot_eval_object_method_direct_aot_throwing(QoreAOTContext* ctx, int32_t slot,
        uint64_t base_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_dot_eval_object_method_direct_aot(ctx, slot, base_bits,
        args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_dot_eval_method_direct_aot_consume_args_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t base_bits, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_dot_eval_method_direct_aot_consume_args(ctx, slot,
            base_bits, args, arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_dot_eval_pseudo_method_direct_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_dot_eval_pseudo_method_direct_aot(ctx, slot, base_bits,
            args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_dot_eval_pseudo_method_direct_aot_consume_args_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t base_bits, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_dot_eval_pseudo_method_direct_aot_consume_args(ctx,
            slot, base_bits, args, arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_object_nb_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_object_nb_aot(ctx, slot, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_object_nb_aot_consume_args_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_object_nb_aot_consume_args(ctx, slot, args,
        arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_closure_fast_throwing(
        uint64_t ref_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_closure_fast(ref_bits, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_call_closure_fast_consume_args_throwing(
        uint64_t ref_bits, uint64_t* args, uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_closure_fast_consume_args(ref_bits, args,
        arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_call_immediate_closure_throwing(const QoreClosureParseNode* cn,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_immediate_closure(cn, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_call_immediate_closure_consume_args_throwing(
        const QoreClosureParseNode* cn, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_immediate_closure_consume_args(cn, args,
        arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_call_immediate_closure_aot_throwing(QoreAOTContext* ctx, int32_t idx,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_immediate_closure_aot(ctx, idx, args, nargs,
        xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_call_immediate_closure_aot_consume_args_throwing(QoreAOTContext* ctx,
        int32_t idx, uint64_t* args, uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_call_immediate_closure_aot_consume_args(ctx, idx,
        args, arg_cleanups, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

static uint64_t qore_rt_call_self_recursive_aot_impl(AotFunctionPtr self_fn,
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    // Lightweight self-recursive call for AOT: eliminates ProgramThreadCountContextHelper,
    // QoreJITStackLocation, execJITWithDeopt wrapper, acceptAssignment, and
    // execCachedFunction indirection.  Calls the AOT function directly via function pointer.
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }

    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, arg_cleanups, nargs, xsink,
            "qore_rt_call_self_recursive_aot", "function", "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    const UserVariantBase* uvb = target.uvb;
    if (!uvb) {
        // Defensive fallback (should not happen for self-recursive)
        if (arg_cleanups) {
            return qore_rt_call_direct_aot_consume_args(ctx, slot, args,
                    arg_cleanups, nargs, xsink);
        }
        return qore_rt_call_direct_aot(ctx, slot, args, nargs, xsink);
    }
    if (!uvb->isStaticallyFastCallEligible()) {
        if (arg_cleanups) {
            return qore_rt_call_direct_aot_consume_args(ctx, slot, args,
                    arg_cleanups, nargs, xsink);
        }
        return qore_rt_call_direct_aot(ctx, slot, args, nargs, xsink);
    }
    qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_self_recursive_aot", "function", "use");

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // A non-closure self-recursive call must not inherit the caller's closure runtime
    // environment.  If the recursive call is made from a nested closure that captured
    // this function's locals, the same LocalVar* would otherwise resolve to the outer
    // frame's captured CVV and skip instantiating this recursive frame's own CVV.
    ThreadSafeLocalVarRuntimeEnvironmentHelper closure_env_clear(nullptr);

    // Recursive calls need a frame boundary so closure-use locals are resolved in the
    // current recursive frame instead of reusing the caller frame's closure variable.
    ThreadFrameBoundaryHelper tfbh(true);

    // Instantiate params on thread-local stack
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Argv for excess args (rare for self-recursive)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        if (sig->argvid) {
            sig->argvid->uninstantiate(xsink);
        }
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
        return toBits(QoreValue());
    }

    // Body locals — use getBodyLocals() for AOT (same as execJITWithDeopt).
    // AOT StoreLocal syncs non-closure body locals through the runtime stack
    // for ownership, so the stack slots must exist even when the IR classifier
    // marks every body local IR-only.
    bool skip_body_locals = false;
    const std::vector<LocalVar*>& body_locals = uvb->getBodyLocals();
    QoreParseOptions po = uvb->getParseOptions(uvb->pgm->getParseOptions());
    if (!skip_body_locals) {
        for (LocalVar* lv : body_locals) {
            if (lv->closureUse()) {
                continue;
            }
            lv->instantiate(po);
        }
    }

    // Call AOT function directly — no deopt, no stack location
    uint64_t result_bits;
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        result_bits = self_fn(ctx, xsink);
    }

    // Uninstantiate body locals
    if (!skip_body_locals) {
        for (int i = (int)body_locals.size() - 1; i >= 0; --i) {
            if (body_locals[i]->closureUse()) {
                continue;
            }
            body_locals[i]->uninstantiate(xsink);
        }
    }

    // Uninstantiate argv + params (LIFO order)
    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // No return type coercion — self-recursive, same return type
    return result_bits;
}

extern "C" DLLEXPORT uint64_t qore_rt_call_self_recursive_aot(AotFunctionPtr self_fn,
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    return qore_rt_call_self_recursive_aot_impl(self_fn, ctx, slot, args,
            nullptr, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_self_recursive_aot_consume_args(
        AotFunctionPtr self_fn, QoreAOTContext* ctx, int32_t slot,
        uint64_t* args, uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink) {
    return qore_rt_call_self_recursive_aot_impl(self_fn, ctx, slot, args,
            arg_cleanups, nargs, xsink);
}

static uint64_t dot_eval_fallback_with_args(QoreValue base, const char* method_name,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation = nullptr);

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_with_base_aot(QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
        ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    // For DOT_EVAL_TARGET slots, the expression is NOTHING and the method dispatch info
    // is stored in call_targets.  Use the pre-resolved method/class for dispatch.
    QoreValue expr;
    std::memcpy(&expr, &ctx->exprs[slot], sizeof(expr));
    if (!expr.hasNode()) {
        const QoreAOTCallTarget& target = ctx->call_targets[slot];
        if (target.method) {
            return target.is_pseudo
                ? qore_rt_dot_eval_pseudo_method_direct(base_bits, target.method, target.qc,
                    target.variant, nullptr, 0, xsink)
                : qore_rt_dot_eval_method_direct(base_bits, target.method, target.qc,
                    target.variant, nullptr, 0, xsink);
        }
        if (target.method_name) {
            QoreValue base = fromBits(base_bits);
            return dot_eval_fallback_with_args(base, target.method_name, nullptr,
                nullptr, 0, xsink);
        }
        return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
            "qore_rt_dot_eval_with_base_aot", "empty dot-eval slot without pre-resolved target");
    }

    return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
        "qore_rt_dot_eval_with_base_aot", "missing pre-resolved dot-eval target");
}

// --- Regex op with pre-evaluated operand helper ---

#include "qore/intern/QoreRegexMatchOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreRegex.h"

extern "C" DLLEXPORT uint64_t qore_rt_regex_op_with_operand(int32_t opcode, uint64_t expr_bits, uint64_t operand_bits,
        ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return qore_rt_invoke_expr(expr_bits, xsink);
    }

    QoreValue operand = fromBits(operand_bits);
    QoreIROpcode op = static_cast<QoreIROpcode>(opcode);

    // Get the regex from the operator node
    QoreRegex* regex = nullptr;
    if (auto* match_node = dynamic_cast<const QoreRegexMatchOperatorNode*>(expr.getInternalNode())) {
        regex = match_node->getRegex();
    }

    if (!regex) {
        // Fallback: shouldn't happen but be safe
        return qore_rt_invoke_expr(expr_bits, xsink);
    }

    QoreStringNodeValueHelper str(operand);

    switch (op) {
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool: {
            bool match = regex->exec(*str, xsink);
            return toBits(QoreValue(match));
        }
        case QoreIROpcode::RegexNMatchBool: {
            bool match = !regex->exec(*str, xsink);
            return toBits(QoreValue(match));
        }
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList: {
            QoreListNode* result = regex->extractSubstrings(*str, xsink);
            return toBits(QoreValue(result));
        }
        default:
            return qore_rt_invoke_expr(expr_bits, xsink);
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_regex_op_with_operand_aot(QoreAOTContext* ctx, int32_t opcode, int32_t slot,
        uint64_t operand_bits, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    QoreValue expr = fromBits(ctx->exprs[slot]);
    if (!expr.hasNode()) {
        return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
            "qore_rt_regex_op_with_operand_aot", "missing regex expression node");
    }

    QoreValue operand = fromBits(operand_bits);
    QoreIROpcode op = static_cast<QoreIROpcode>(opcode);

    QoreRegex* regex = nullptr;
    if (auto* match_node = dynamic_cast<const QoreRegexMatchOperatorNode*>(expr.getInternalNode())) {
        regex = match_node->getRegex();
    }
    if (!regex) {
        return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
            "qore_rt_regex_op_with_operand_aot", "missing pre-resolved regex object");
    }

    QoreStringNodeValueHelper str(operand);
    switch (op) {
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool: {
            bool match = regex->exec(*str, xsink);
            return toBits(QoreValue(match));
        }
        case QoreIROpcode::RegexNMatchBool: {
            bool match = !regex->exec(*str, xsink);
            return toBits(QoreValue(match));
        }
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList: {
            QoreListNode* result = regex->extractSubstrings(*str, xsink);
            return toBits(QoreValue(result));
        }
        default:
            return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
                "qore_rt_regex_op_with_operand_aot", "unsupported regex opcode");
    }
}

// AOT mode: regex op with pattern string instead of expr slot.
// Compiles the regex at each call (no caching — regex compilation is fast relative to
// the string matching it precedes). Eliminates EXPR_TREE for regex expression slots.
//
// The `global` parameter mirrors QoreRegex::setGlobal(); without it, AOT-compiled
// `=~ x/.../g` (regex extract) would only return the first match because the
// freshly-constructed QoreRegex defaults to non-global. The flag lives on
// QoreRegex separately from the PCRE options bitfield, so it must be plumbed
// through explicitly.
extern "C" DLLEXPORT uint64_t qore_rt_regex_op_by_pattern(int32_t opcode, const char* pattern,
        int64_t options, int32_t global, uint64_t operand_bits, ExceptionSink* xsink) {
    QoreValue operand = fromBits(operand_bits);
    QoreStringNodeValueHelper str(operand);

    // Compile the regex from the pattern
    QoreString pat_str(pattern);
    QoreRegex regex(&pat_str, static_cast<int>(options), xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    if (global) {
        regex.setGlobal();
    }

    QoreIROpcode op = static_cast<QoreIROpcode>(opcode);
    switch (op) {
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool: {
            bool match = regex.exec(*str, xsink);
            return toBits(QoreValue(match));
        }
        case QoreIROpcode::RegexNMatchBool: {
            bool match = !regex.exec(*str, xsink);
            return toBits(QoreValue(match));
        }
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList: {
            QoreListNode* result = regex.extractSubstrings(*str, xsink);
            return toBits(QoreValue(result));
        }
        default:
            if (xsink) {
                xsink->raiseException("AOT-ERROR", "unsupported regex opcode %d", opcode);
            }
            return toBits(QoreValue());
    }
}

// --- Iterator helpers ---

// Creates an iterator from an iterable value.
// Returns a pointer to FunctionalOperatorInterface (as i64), or 0 if iterable is NOTHING.
// The iterator_func parameter is optional (can be nullptr) for use with FunctionalOperator expressions.
extern "C" DLLEXPORT void* qore_rt_iterator_create(uint64_t iterable_bits, void* iterator_func, ExceptionSink* xsink) {
    QoreValue iterable = fromBits(iterable_bits);

    FunctionalOperator::FunctionalValueType value_type;
    FunctionalOperatorInterface* iter = nullptr;

    if (iterator_func) {
        FunctionalOperator* func_op = reinterpret_cast<FunctionalOperator*>(iterator_func);
        iter = func_op->getFunctionalIterator(value_type, xsink);
    } else {
        iter = FunctionalOperatorInterface::getFunctionalIterator(value_type, iterable, true,
            "foreach statement", xsink);
    }

    // Return nullptr on exception or NOTHING value type
    if ((xsink && *xsink) || value_type == FunctionalOperator::nothing) {
        delete iter;
        return nullptr;
    }

    return iter;
}

extern "C" DLLEXPORT void* qore_rt_iterator_create_iterate(uint64_t iterable_bits, ExceptionSink* xsink) {
    QoreValue iterable = fromBits(iterable_bits);

    FunctionalOperator::FunctionalValueType value_type;
    FunctionalOperatorInterface* iter = QoreIterateOperatorNode::getFunctionalIterator(value_type, iterable,
        nullptr, "streaming operator expression", xsink);

    if ((xsink && *xsink) || value_type == FunctionalOperator::nothing) {
        delete iter;
        return nullptr;
    }

    return iter;
}

extern "C" DLLEXPORT uint64_t qore_rt_iterate_value(uint64_t source_bits, ExceptionSink* xsink) {
    QoreValue source = fromBits(source_bits);
    return toBits(QoreIterateOperatorNode::evalIteratorValue(source,
        QoreIterateOperatorNode::getElementTypeInfo(source, source.getTypeInfo()), xsink));
}

/// AOT version: looks up iterator_func pointer from AOT context by slot index
extern "C" DLLEXPORT void* qore_rt_iterator_create_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t iterable_bits, ExceptionSink* xsink) {
    assert(ctx);
    // Negative slot (-1) means null iterator_func (no custom iteration function)
    // Otherwise, look up the FunctionalOperator* pointer from the exprs array
    void* iterator_func = nullptr;
    if (slot >= 0 && slot < ctx->num_exprs) {
        iterator_func = reinterpret_cast<void*>(ctx->exprs[slot]);
    }
    return qore_rt_iterator_create(iterable_bits, iterator_func, xsink);
}

// Advances the iterator and returns the next value.
// Returns: 1 if done (iterator exhausted), 0 if has more values.
// On success (not done), stores the current value in *out_value.
// On done or exception, the iterator is deleted.
extern "C" DLLEXPORT int64_t qore_rt_iterator_next(void* iter_ptr, uint64_t* out_value, ExceptionSink* xsink) {
    if (!iter_ptr) {
        // Empty iterator (was NOTHING) - already done
        *out_value = 0;  // VAL_NOTHING - keep alloca consistent for decref-before-overwrite
        return 1;
    }

    FunctionalOperatorInterface* iter = reinterpret_cast<FunctionalOperatorInterface*>(iter_ptr);
    ValueOptionalRefHolder val(xsink);
    bool done = iter->getNext(val, xsink);

    if (xsink && *xsink) {
        // Exception occurred - clean up iterator
        delete iter;
        *out_value = 0;  // VAL_NOTHING - keep alloca consistent for decref-before-overwrite
        return 1;
    }

    if (done) {
        // Iterator exhausted - clean up
        delete iter;
        *out_value = 0;  // VAL_NOTHING - keep alloca consistent for decref-before-overwrite
        return 1;
    }

    // Store current value and continue
    *out_value = toBits(val.takeReferencedValue());
    return 0;
}

// Cleans up an active iterator on non-normal function exit paths.
// Called from JIT-compiled code's exit cleanup when a foreach body is exited
// by return/throw before the iterator is exhausted.
extern "C" DLLEXPORT void qore_rt_iterator_cleanup(void* iter_ptr) {
    if (iter_ptr) {
        delete reinterpret_cast<FunctionalOperatorInterface*>(iter_ptr);
    }
}

// --- Reference foreach helpers ---

// Opaque state for reference foreach iteration
struct RefForeachState {
    ReferenceNode* vr = nullptr;      // runtime reference (one ref owned)
    QoreValue tlist;                   // original value (one ref owned)
    QoreListNode* l_tlist = nullptr;   // pointer to list in tlist (borrowed, not owned)
    QoreValue ln;                      // result accumulator (one ref owned)
};

// Initialize reference foreach state from a ParseReferenceNode expression.
// Returns an opaque state pointer (as uint64_t), or 0 on error.
extern "C" DLLEXPORT uint64_t qore_rt_ref_foreach_init(uint64_t parse_ref_bits, ExceptionSink* xsink) {
    QoreValue parse_ref = fromBits(parse_ref_bits);
    ParseReferenceNode* r = parse_ref.get<ParseReferenceNode>();
    if (!r) {
        xsink->raiseException("FOREACH-ERROR", "reference foreach: expected a reference expression");
        return 0;
    }

    auto* state = new RefForeachState();

    // Evaluate ParseReferenceNode to get runtime ReferenceNode
    state->vr = r->evalToRef(xsink);
    if (*xsink) {
        delete state;
        return 0;
    }

    // Get the current value of the lvalue expression
    state->tlist = state->vr->eval(xsink);
    if (*xsink) {
        state->vr->deref(xsink);
        delete state;
        return 0;
    }

    state->l_tlist = (state->tlist.getType() == NT_LIST)
        ? state->tlist.get<QoreListNode>() : nullptr;

    // Create result accumulator
    if (state->l_tlist) {
        state->ln = new QoreListNode(autoTypeInfo);
    }

    return reinterpret_cast<uint64_t>(state);
}

// Get the iteration count for a reference foreach state.
// Returns 0 for NOTHING/empty, list size for lists, 1 for scalars.
extern "C" DLLEXPORT int64_t qore_rt_ref_foreach_size(uint64_t state_ptr) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    if (!state) {
        return 0;
    }
    if (state->l_tlist) {
        return state->l_tlist->empty() ? 0 : static_cast<int64_t>(state->l_tlist->size());
    }
    return state->tlist.isNothing() ? 0 : 1;
}

// Get the element at the given index from the reference foreach state.
// Returns a referenced value suitable for assignment to the loop variable.
extern "C" DLLEXPORT uint64_t qore_rt_ref_foreach_get_entry(uint64_t state_ptr, int64_t index,
        ExceptionSink* xsink) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    QoreValue entry;
    if (state->l_tlist) {
        entry = state->l_tlist->getReferencedEntry(static_cast<size_t>(index));
    } else {
        // Scalar: return the value (first and only iteration)
        entry = state->tlist.refSelf();
    }
    if (getenv("QORE_AOT_TRACE_REF_FOREACH")) {
        fprintf(stderr, "[ref-foreach] get_entry state=%p l_tlist=%p index=%ld "
            "entry_type=%d\n",
            (void*)state, (const void*)(state ? state->l_tlist : nullptr),
            (long)index, (int)entry.getType());
        fflush(stderr);
    }
    return toBits(entry);
}

// Record the modified loop variable value after body execution.
extern "C" DLLEXPORT void qore_rt_ref_foreach_record(uint64_t state_ptr, uint64_t value_bits,
        ExceptionSink* xsink) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    QoreValue value = fromBits(value_bits);
    if (state->l_tlist) {
        state->ln.get<QoreListNode>()->push(value.refSelf(), nullptr);
    } else {
        state->ln.discard(nullptr);
        state->ln = value.refSelf();
    }
}

// Finalize: optionally fill remaining elements (on break), write back to reference, and clean up.
// If *xsink is set (exception path), does cleanup without write-back.
// fill_remaining: 1 = fill remaining elements from original (break case), 0 = write back as-is
extern "C" DLLEXPORT void qore_rt_ref_foreach_finalize(uint64_t state_ptr, int64_t fill_remaining,
        ExceptionSink* xsink) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    if (!state) {
        return;
    }

    if (*xsink) {
        // Exception path: clean up without write-back
        state->ln.discard(xsink);
        state->tlist.discard(xsink);
        state->vr->deref(xsink);
        delete state;
        return;
    }

    // Fill remaining elements if result list is shorter than original (break case)
    if (fill_remaining && state->l_tlist && state->ln.getType() == NT_LIST) {
        QoreListNode* result = state->ln.get<QoreListNode>();
        size_t result_size = result->size();
        size_t orig_size = state->l_tlist->size();
        for (size_t i = result_size; i < orig_size; ++i) {
            result->push(state->l_tlist->getReferencedEntry(i), nullptr);
        }
    }

    // Write the value back to the lvalue reference
    {
        LValueHelper val(*state->vr, xsink);
        if (val) {
            QoreValue result = state->ln;
            state->ln = QoreValue();  // prevent double-free
            val.assign(result);
        } else {
            state->ln.discard(xsink);
            state->ln = QoreValue();
        }
    }

    state->tlist.discard(xsink);
    state->vr->deref(xsink);
    delete state;
}

// Clean up reference foreach state without write-back (exception/early-exit paths).
extern "C" DLLEXPORT void qore_rt_ref_foreach_cleanup(uint64_t state_ptr, ExceptionSink* xsink) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    if (!state) {
        return;
    }
    state->ln.discard(xsink);
    state->tlist.discard(xsink);
    state->vr->deref(xsink);
    delete state;
}

// --- Native `context` statement helpers (paired with IR opcodes 140, 361-363) ---
//
// Lifetime: qore_rt_context_init pushes a Context frame onto the thread-local
// context stack (via `Context::Context`) and returns its pointer as uint64_t.
// On failure (xsink set during hash evaluation / where filter / sort), the
// helper cleans up (`Context::deref` pops stack + derefs hash) and returns 0.
// Callers MUST call qore_rt_context_destroy on every success return.
//
// The Context ctor evaluates `where_exp` and `sort_exp` per-row internally
// against itself (already on top of the stack) — those expressions are AST
// trees passed through as opaque QoreValue bits.

extern "C" DLLEXPORT uint64_t qore_rt_context_init(const char* name, uint64_t exp_bits,
        uint64_t where_bits, uint64_t sort_bits, int sort_type, ExceptionSink* xsink) {
    QoreValue exp = fromBits(exp_bits);
    QoreValue where_exp = fromBits(where_bits);
    QoreValue sort_exp = fromBits(sort_bits);

    if (!exp && !get_context_stack()) {
        if (xsink) {
            xsink->raiseException("CONTEXT-EXCEPTION",
                "cannot create a subcontext without an active parent context");
        }
        return 0;
    }

    // `new Context(...)` pushes itself on the thread-local stack before
    // evaluating exp/where/sort, matching AST semantics.  On failure,
    // deref() pops the stack and frees (also derefs the data hash on !sub).
    Context* ctx = new Context(const_cast<char*>(name && *name ? name : nullptr),
        xsink, exp, where_exp, sort_type, sort_exp);
    if (xsink && *xsink) {
        ctx->deref(xsink);
        return 0;
    }
    return reinterpret_cast<uint64_t>(ctx);
}

extern "C" DLLEXPORT int64_t qore_rt_context_max_pos(uint64_t state_ptr) {
    Context* ctx = reinterpret_cast<Context*>(state_ptr);
    return ctx ? static_cast<int64_t>(ctx->max_pos) : 0;
}

extern "C" DLLEXPORT void qore_rt_context_set_pos(uint64_t state_ptr, int64_t index) {
    Context* ctx = reinterpret_cast<Context*>(state_ptr);
    if (ctx) {
        ctx->pos = static_cast<int>(index);
    }
}

extern "C" DLLEXPORT void qore_rt_context_destroy(uint64_t state_ptr, ExceptionSink* xsink) {
    Context* ctx = reinterpret_cast<Context*>(state_ptr);
    if (ctx) {
        ctx->deref(xsink);
    }
}

// Throwing wrapper for invoke-based EH (QORE_AOT_EH=1).  Matches the pattern
// used by qore_rt_ref_foreach_init_throwing / iterator_*_throwing.
extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_context_init_throwing(
        const char* name, uint64_t exp_bits, uint64_t where_bits, uint64_t sort_bits,
        int sort_type, ExceptionSink* xsink) {
    uint64_t result = qore_rt_context_init(name, exp_bits, where_bits, sort_bits, sort_type,
        xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT uint64_t qore_rt_context_ref_at(const char* key, int32_t stack_offset,
        ExceptionSink* xsink) {
    if (!key || !*key) {
        xsink->raiseException("CONTEXT-EXCEPTION", "empty context reference");
        return toBits(QoreValue());
    }

    Context* ctx = get_context_stack();
    for (int32_t i = 0; i < stack_offset && ctx; ++i) {
        if (i && !(i % 100) && qore_check_cancel(xsink, "context reference stack walk")) {
            return toBits(QoreValue());
        }
        ctx = ctx->next;
    }
    if (!ctx) {
        xsink->raiseException("CONTEXT-EXCEPTION",
            "context reference '%s' out of context", key);
        return toBits(QoreValue());
    }
    return toBits(ctx->eval(key, xsink));
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_context_ref_at_throwing(
        const char* key, int32_t stack_offset, ExceptionSink* xsink) {
    uint64_t result = qore_rt_context_ref_at(key, stack_offset, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT uint64_t qore_rt_context_row(ExceptionSink* xsink) {
    Context* ctx = get_context_stack();
    if (!ctx) {
        xsink->raiseException("CONTEXT-EXCEPTION",
            "context row reference \"%%%%\" encountered out of context");
        return toBits(QoreValue());
    }
    return toBits(QoreValue(ctx->getRow(xsink)));
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_context_row_throwing(
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_context_row(xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

// --- Phase 2B Step 5: Iterator category throwing wrappers ---

extern "C" DLLEXPORT __attribute__((noinline)) void* qore_rt_iterator_create_throwing(
        uint64_t iterable_bits, void* iterator_func, ExceptionSink* xsink) {
    void* result = qore_rt_iterator_create(iterable_bits, iterator_func, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void* qore_rt_iterator_create_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t iterable_bits, ExceptionSink* xsink) {
    void* result = qore_rt_iterator_create_aot(ctx, slot, iterable_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void* qore_rt_iterator_create_iterate_throwing(
        uint64_t iterable_bits, ExceptionSink* xsink) {
    void* result = qore_rt_iterator_create_iterate(iterable_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_iterate_value_throwing(
        uint64_t source_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_iterate_value(source_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void* qore_rt_iterator_create_reverse_throwing(
        uint64_t iterable_bits, ExceptionSink* xsink) {
    void* result = qore_rt_iterator_create_reverse(iterable_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) int64_t qore_rt_iterator_next_throwing(
        void* iter_ptr, uint64_t* out_value, ExceptionSink* xsink) {
    int64_t result = qore_rt_iterator_next(iter_ptr, out_value, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_ref_foreach_init_throwing(
        uint64_t parse_ref_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_ref_foreach_init(parse_ref_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_ref_foreach_get_entry_throwing(
        uint64_t state_ptr, int64_t index, ExceptionSink* xsink) {
    uint64_t result = qore_rt_ref_foreach_get_entry(state_ptr, index, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_ref_foreach_record_throwing(
        uint64_t state_ptr, uint64_t value_bits, ExceptionSink* xsink) {
    qore_rt_ref_foreach_record(state_ptr, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_ref_foreach_finalize_throwing(
        uint64_t state_ptr, int64_t fill_remaining, ExceptionSink* xsink) {
    qore_rt_ref_foreach_finalize(state_ptr, fill_remaining, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT __attribute__((noinline)) void qore_rt_ref_foreach_cleanup_throwing(
        uint64_t state_ptr, ExceptionSink* xsink) {
    qore_rt_ref_foreach_cleanup(state_ptr, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

// --- Direct dot-eval method call (pre-evaluated base + args) ---

static QoreListNode* buildArgListFromNanBoxed(uint64_t* args, int nargs, ExceptionSink* xsink) {
    if (nargs <= 0) {
        return nullptr;
    }
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    QoreListNode* arg_list = new QoreListNode(autoTypeInfo);
    qore_list_private* priv = qore_list_private::get(*arg_list);
    priv->reserve(nargs);
    for (int i = 0; i < nargs; ++i) {
        QoreValue val = fromBits(args[i]);
        if (val.hasNode()) {
            val.refSelf();
        }
        priv->pushIntern(val);
    }
    return arg_list;
}

// Constructor call with pre-evaluated NaN-boxed args.
// Used by IR interpreter and JIT LLVM codegen to avoid AST fallback for
// NewObject — each constructor arg is computed as a separate IR operand and
// passed as a NaN-boxed value. execConstructor + CodeEvaluationHelper handle
// default args and type coercion; pre-evaluated values pass through eval() as
// a no-op since only AST nodes need re-evaluation.
static std::string qore_rt_make_aot_variant_signature(const AbstractQoreFunctionVariant* variant) {
    if (!variant || !variant->getSignature()) {
        return std::string();
    }
    std::string rv("(");
    const type_vec_t& types = variant->getSignature()->getTypeList();
    for (size_t i = 0; i < types.size(); ++i) {
        if (i > 0) {
            rv.append(",");
        }
        rv.append(qore_get_aot_serializable_type_path(types[i]));
    }
    rv.append(")");
    return rv;
}

static const AbstractQoreFunctionVariant* qore_rt_find_constructor_variant_by_aot_signature(
        const QoreClass* qc, const char* variant_sig) {
    if (!qc || !variant_sig || !*variant_sig) {
        return nullptr;
    }
    const QoreMethod* cons = qc->getConstructor();
    if (!cons) {
        return nullptr;
    }
    const QoreFunction* cf = qore_method_private::get(*cons)->getFunction();
    if (!cf) {
        return nullptr;
    }
    if (const AbstractQoreFunctionVariant* v = cf->findVariantBySignatureText(variant_sig)) {
        return v;
    }
    QoreFunctionIterator vi(*cf);
    while (vi.next()) {
        const AbstractQoreFunctionVariant* v = vi.getVariant();
        if (qore_rt_make_aot_variant_signature(v) == variant_sig) {
            return v;
        }
    }
    return nullptr;
}

static const QoreClass* qore_rt_resolve_new_object_class(const QoreClass* qc,
        const AbstractQoreFunctionVariant*& variant) {
    QoreProgram* exec_pgm = getProgram();
    if (!qc || !exec_pgm) {
        return qc;
    }

    const qore_class_private* priv = qore_class_private::get(*qc);
    QoreProgram* owner_pgm = priv && priv->ns ? priv->ns->getProgram() : nullptr;
    if (owner_pgm == exec_pgm) {
        return qc;
    }

    std::string class_ref = qore_aot_encode_class_ref(qc);
    const QoreClass* mapped = qore_aot_resolve_class_ref(exec_pgm, class_ref.c_str(), false);
    if (!mapped || mapped == qc) {
        return qc;
    }

    if (variant) {
        std::string variant_sig = qore_rt_make_aot_variant_signature(variant);
        variant = qore_rt_find_constructor_variant_by_aot_signature(mapped, variant_sig.c_str());
    }
    return mapped;
}

extern "C" DLLEXPORT uint64_t qore_rt_new_object_nb(const QoreClass* qc,
        const AbstractQoreFunctionVariant* variant, const QoreTypeInfo* object_type_info,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    if (!qc) {
        xsink->raiseException("AOT-ERROR", "null class pointer in new object call");
        return toBits(QoreValue());
    }
    qc = qore_rt_resolve_new_object_class(qc, variant);
    ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    RuntimeConfig& rc = rc_get_current_ref();
    object_type_info = qore_substitute_type_params_if_needed(object_type_info);
    // Pass nullptr (not *arg_list) when nargs=0: buildArgListFromNanBoxed
    // returns null for empty lists and *arg_list on a null holder is UB.
    return toBits(qore_class_private::execConstructor(*qc, rc, variant,
        nargs > 0 ? *arg_list : nullptr, xsink, object_type_info));
}

extern "C" DLLEXPORT uint64_t qore_rt_new_object_by_path_nb(const char* class_path,
        const char* variant_sig, const QoreTypeInfo* object_type_info, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    const QoreClass* qc = class_path && *class_path
        ? qore_aot_resolve_class_ref(getProgram(), class_path, false) : nullptr;
    if (!qc) {
        xsink->raiseException("AOT-ERROR",
            "cannot resolve class '%s' for AOT new object call",
            class_path && *class_path ? class_path : "<missing>");
        return toBits(QoreValue());
    }
    const AbstractQoreFunctionVariant* variant =
        qore_rt_find_constructor_variant_by_aot_signature(qc, variant_sig);
    return qore_rt_new_object_nb(qc, variant, object_type_info, args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_object_nb_consume_args(
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        const QoreTypeInfo* object_type_info, uint64_t* args, uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink) {
    if (!qc) {
        xsink->raiseException("AOT-ERROR", "null class pointer in new object call");
        clearConsumedArgCleanups(arg_cleanups, nargs, xsink);
        return toBits(QoreValue());
    }
    qc = qore_rt_resolve_new_object_class(qc, variant);
    ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        return toBits(QoreValue());
    }
    if (*xsink) {
        return toBits(QoreValue());
    }
    RuntimeConfig& rc = rc_get_current_ref();
    object_type_info = qore_substitute_type_params_if_needed(object_type_info);
    return toBits(qore_class_private::execConstructor(*qc, rc, variant,
        nargs > 0 ? *arg_list : nullptr, xsink, object_type_info));
}

extern "C" DLLEXPORT uint64_t qore_rt_new_object_by_path_nb_consume_args(const char* class_path,
        const char* variant_sig, const QoreTypeInfo* object_type_info, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    const QoreClass* qc = class_path && *class_path
        ? qore_aot_resolve_class_ref(getProgram(), class_path, false) : nullptr;
    if (!qc) {
        xsink->raiseException("AOT-ERROR",
            "cannot resolve class '%s' for AOT new object call",
            class_path && *class_path ? class_path : "<missing>");
        clearConsumedArgCleanups(arg_cleanups, nargs, xsink);
        return toBits(QoreValue());
    }
    const AbstractQoreFunctionVariant* variant =
        qore_rt_find_constructor_variant_by_aot_signature(qc, variant_sig);
    return qore_rt_new_object_nb_consume_args(qc, variant, object_type_info, args,
        arg_cleanups, nargs, xsink);
}

// AOT variant: resolve qc/variant from the per-function call_targets slot
// (populated at module load time from serialized class_path + variant_sig).
extern "C" DLLEXPORT uint64_t qore_rt_new_object_nb_aot(QoreAOTContext* ctx,
        int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    const QoreClass* qc = target.qc;
    if (!qc && target.class_path && *target.class_path) {
        qc = qore_aot_resolve_class_ref(ctx->pgm, target.class_path, false);
    }
    if (!qc) {
        xsink->raiseException("AOT-ERROR",
            "cannot resolve class '%s' for AOT new object call target slot %d",
            target.class_path && *target.class_path ? target.class_path : "<missing>", slot);
        return toBits(QoreValue());
    }
    return qore_rt_new_object_nb(qc, target.variant, target.object_type_info, args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_object_nb_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    const QoreClass* qc = target.qc;
    if (!qc && target.class_path && *target.class_path) {
        qc = qore_aot_resolve_class_ref(ctx->pgm, target.class_path, false);
    }
    if (!qc) {
        xsink->raiseException("AOT-ERROR",
            "cannot resolve class '%s' for AOT new object call target slot %d",
            target.class_path && *target.class_path ? target.class_path : "<missing>", slot);
        clearConsumedArgCleanups(arg_cleanups, nargs, xsink);
        return toBits(QoreValue());
    }
    return qore_rt_new_object_nb_consume_args(qc, target.variant, target.object_type_info, args,
        arg_cleanups, nargs, xsink);
}

// Dispatch a method call on a QoreObject with pre-evaluated args.
// Same logic as AbstractMethodCallNode::exec(): if the runtime class matches
// the parse-time class, use the resolved method pointer directly; otherwise
// fall back to name-based lookup.
// Try fast dispatch for a user method with cached JIT function on a matching object
// Returns true if the fast path was taken, false if caller should use the slow path
static bool try_dispatch_method_fast(QoreObject* o, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        uint64_t& result) {
    if (!variant) {
        return false;
    }
    // copy() must go through dispatch_method_on_object → execCopy to create a new object
    if (!strcmp(method->getName(), "copy")) {
        return false;
    }
    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        // Builtin method — fall back to slow path for proper soft type coercion
        return false;
    }
    if (!qore_rt_method_fast_call_eligible(variant)) {
        return false;
    }
    if (!uvb->hasCachedFunction() && !uvb->getCachedIR()) {
        return false;
    }
    if (o->getClass() != qc && o->getClass() != method->getClass()) {
        return false;
    }
    if (!o->isValid()) {
        xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has "
            "already been deleted", qc->getName(), method->getName());
        result = toBits(QoreValue());
        return true;
    }
    const QoreTypeInfo* receiver_type_info = qore_get_object_receiver_type_info(o);

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Capture caller's program before ptcch switch.
    QoreProgram* caller_pgm = getProgram();
    QoreProgram* exec_pgm = qore_rt_method_execution_program(method, uvb);

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, exec_pgm, true);
    if (*xsink) {
        result = toBits(QoreValue());
        return true;
    }
    // This fast dot-eval path bypasses CodeEvaluationHelper/UserVariantExecHelper,
    // so it must establish the callee frame itself, like qore_rt_call_method_fast().
    ThreadFrameBoundaryHelper tfbh(true);

    // Push self object onto the method call stack (for runtime_get_stack_object())
    ObjectSubstitutionHelper osh(o, qore_class_private::get(*method->getClass()));

    // Check if callee IR supports direct param passing (bypass TLS entirely)
    const QoreIRFunction* ir = uvb->getCachedIR();
    bool use_direct_params = ir && ir->isDirectParamsRuntimeSafe()
        && !uvb->hasCachedFunction() && nargs >= (int)num_params;

    LocalVar* selfid = sig->selfid ? sig->selfid : findIRSelfLocal(ir);
    bool selfid_instantiated = selfid;
    if (selfid_instantiated) {
        selfid->instantiateSelf(o);
    }

    if (!use_direct_params) {
        // Standard path: push params to TLS
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            if (selfid_instantiated) {
                selfid->uninstantiateSelf();
            }
            result = toBits(QoreValue());
            return true;
        }
    }
    // else: direct_params path — params are pre-populated in the IR slot cache.
    // selfid still lives in TLS because IR can load `self` explicitly.

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Use cached IR name when available (zero allocation); fall back to building the name
    std::string call_name_buf;
    if (!ir) {
        call_name_buf = std::string(method->getClass()->getName()) + "::" + method->getName();
    }
    const std::string& call_name = ir ? ir->getDisplayName() : call_name_buf;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        if (use_direct_params) {
            // Direct params path: pass args straight to IR slot cache, no TLS
            IRDirectParams dp{args, nargs, arg_cleanups};
            execJITWithDeopt(uvb, call_name, [ir, uvb, exec_pgm, &dp](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), exec_pgm, false, &dp);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink, caller_pgm, receiver_type_info, exec_pgm);
        } else if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink, caller_pgm, receiver_type_info, exec_pgm);
        } else {
            // IR fast path (standard TLS): execute IR directly without QoreListNode.
            execJITWithDeopt(uvb, call_name, [ir, uvb, exec_pgm](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), exec_pgm);
                if (!ok && !*xs) {
                    inv = true;  // Request deopt to AST
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink, caller_pgm, receiver_type_info, exec_pgm);
        }
    }

    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        if (sig->argvid) {
            sig->argvid->uninstantiate(xsink);
        }
        if (!use_direct_params) {
            for (int i = static_cast<int>(num_params) - 1; i >= 0; --i) {
                sig->lv[i]->uninstantiate(xsink);
            }
        }
        if (selfid_instantiated) {
            selfid->uninstantiateSelf();
        }
        result = toBits(QoreValue());
        return true;
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    if (!use_direct_params) {
        // Standard path: uninstantiate params from TLS
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
    }
    if (selfid_instantiated) {
        selfid->uninstantiateSelf();
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = qore_rt_get_effective_return_type(sig, receiver_type_info);
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    result = toBits(val);
    return true;
}

static uint64_t dispatch_method_on_object(QoreObject* o, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        QoreListNode* arg_list, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation = nullptr) {
    // copy() is a special operation that creates a new object — must use execCopy,
    // not regular method dispatch which would call the copy variant as a normal method
    if (!strcmp(method->getName(), "copy")) {
        return toBits(o->getClass()->execCopy(o, xsink));
    }
    if (o->getClass() == qc || o->getClass() == method->getClass()) {
        if (!o->isValid()) {
            xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has "
                "already been deleted", qc->getName(), method->getName());
            return toBits(QoreValue());
        }
        // Use evalTmpArgs to preserve ReferenceNode values in the pre-evaluated arg list while still honoring the
        // parse-selected overload variant when one is available.
        return toBits(qore_method_private::evalTmpArgs(*method, xsink, rc_get_current_ref(), o, arg_list, nullptr,
            variant, nullptr, explicit_type_param_instantiation));
    }
    // Class mismatch — name-based lookup (virtual dispatch to the runtime class)
    // Pass the runtime class context so that private method access checks succeed
    // when a base class method calls a private method on self and the runtime type
    // is a derived class (mirrors AbstractMethodCallNode::exec() in AST mode).
    // issue #3596: do not use the context class if it's not compatible with "o" —
    // otherwise runtimeFindCommittedMethodForEval picks up a private:internal method
    // from the caller's class for an unrelated target object.
    const qore_class_private* class_ctx = runtime_get_class();
    if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*o->getClass(), class_ctx)) {
        class_ctx = nullptr;
    }
    RuntimeConfig& rc = rc_get_current_ref();
    const qore_class_private* priv = qore_class_private::get(*o->getClass());
    return toBits(priv->evalMethod(o, method->getName(), arg_list, class_ctx, rc, xsink,
        explicit_type_param_instantiation));
}

static uint64_t qore_rt_dot_eval_pseudo_method_direct_impl(uint64_t base_bits,
        const QoreMethod* method, const QoreClass* qc,
        const AbstractQoreFunctionVariant* variant, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation = nullptr);

static uint64_t dot_eval_fallback_with_args(QoreValue base, const char* method_name,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation);

static uint64_t qore_rt_dot_eval_method_direct_impl(uint64_t base_bits, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation) {
    // method/qc may be null for abstract/unresolved method calls generated by IR lowering
    // to pass pre-evaluated args (avoids EXPR_TREE local variable access issues in AOT).
    // In this case, the method name must be retrieved from the embedded expression by the
    // AOT call target resolver. For the JIT path (this function), we cannot resolve without
    // the method name, so we fall through to the pseudo-method dispatch below.
    // This path is hit from the IR interpreter's null-method fallback, which already handles
    // this case via dot_eval_fallback_with_args before reaching here.

    QoreValue raw_base = fromBits(base_bits);
    ValueEvalOptimizedRefHolder base_holder(xsink);
    QoreValue base;
    if (qore_rt_dot_eval_preserve_raw_base(raw_base)) {
        base = raw_base;
    } else {
        base_holder.eval(raw_base);
        base = *base_holder;
    }
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }

    switch (base.getType()) {
        case NT_WEAKREF: {
            QoreObject* o = base.get<WeakReferenceNode>()->get();
            if (!o) {
                xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on a deleted weak reference",
                    qc->getName(), method->getName());
                return toBits(QoreValue());
            }
            // Try fast path for JIT-compiled user methods (avoids building QoreListNode)
            uint64_t result;
            if (!explicit_type_param_instantiation
                    && try_dispatch_method_fast(o, method, qc, variant, args, arg_cleanups, nargs, xsink, result)) {
                return result;
            }
            ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
            if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
                return toBits(QoreValue());
            }
            return dispatch_method_on_object(o, method, qc, variant, *arg_list, xsink,
                explicit_type_param_instantiation);
        }

        case NT_OBJECT: {
            QoreObject* o = const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(base.getInternalNode()));
            // Try fast path for JIT-compiled user methods (avoids building QoreListNode)
            uint64_t result;
            if (!explicit_type_param_instantiation
                    && try_dispatch_method_fast(o, method, qc, variant, args, arg_cleanups, nargs, xsink, result)) {
                return result;
            }
            ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
            if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
                return toBits(QoreValue());
            }
            return dispatch_method_on_object(o, method, qc, variant, *arg_list, xsink,
                explicit_type_param_instantiation);
        }

        case NT_HASH: {
            const AbstractQoreNode* ref = check_call_ref(base.getInternalNode(), method->getName());
            if (ref) {
                ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
                if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
                    return toBits(QoreValue());
                }
                return toBits(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(*arg_list, xsink));
            }
            break;
        }

        case NT_WEAKREF_HASH: {
            const AbstractQoreNode* ref = check_call_ref(base.get<WeakHashReferenceNode>()->get(), method->getName());
            if (ref) {
                ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
                if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
                    return toBits(QoreValue());
                }
                return toBits(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(*arg_list, xsink));
            }
            break;
        }

        default:
            break;
    }

    // Non-object: dispatch to pseudo-method path
    return qore_rt_dot_eval_pseudo_method_direct_impl(base_bits, method, qc, variant,
        args, arg_cleanups, nargs, xsink, explicit_type_param_instantiation);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct(uint64_t base_bits, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    return qore_rt_dot_eval_method_direct_impl(base_bits, method, qc, variant,
        args, nullptr, nargs, xsink, nullptr);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct_with_inst(uint64_t base_bits,
        const QoreMethod* method, const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, const QoreTypeParamInstantiation* explicit_type_param_instantiation,
        ExceptionSink* xsink) {
    return qore_rt_dot_eval_method_direct_impl(base_bits, method, qc, variant,
        args, nullptr, nargs, xsink, explicit_type_param_instantiation);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct_consume_args(uint64_t base_bits,
        const QoreMethod* method, const QoreClass* qc,
        const AbstractQoreFunctionVariant* variant, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    return qore_rt_dot_eval_method_direct_impl(base_bits, method, qc, variant,
        args, arg_cleanups, nargs, xsink, nullptr);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct_with_inst_consume_args(uint64_t base_bits,
        const QoreMethod* method, const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, uint64_t** arg_cleanups, int nargs,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation, ExceptionSink* xsink) {
    return qore_rt_dot_eval_method_direct_impl(base_bits, method, qc, variant,
        args, arg_cleanups, nargs, xsink, explicit_type_param_instantiation);
}

static uint64_t qore_rt_dot_eval_pseudo_method_direct_impl(uint64_t base_bits, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation) {
    assert(method);
    assert(qc);

    QoreValue raw_base = fromBits(base_bits);
    ValueEvalOptimizedRefHolder base_holder(xsink);
    QoreValue base;
    if (qore_rt_dot_eval_preserve_raw_base(raw_base)) {
        base = raw_base;
    } else {
        base_holder.eval(raw_base);
        base = *base_holder;
    }
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }

    // Match QoreDotEvalOperatorNode::evalWithBase(): objects and hashes get
    // normal name-based dispatch before pseudo-method dispatch.  This preserves
    // user method overrides such as className() and hash member callrefs such as
    // h.size().
    switch (base.getType()) {
        case NT_OBJECT:
        case NT_WEAKREF:
        case NT_HASH:
        case NT_WEAKREF_HASH:
            return dot_eval_fallback_with_args(base, method->getName(), args, arg_cleanups, nargs, xsink,
                explicit_type_param_instantiation);
        default:
            break;
    }

    // Unwrap weak references — pseudo method handlers expect the underlying value
    if (base.getType() == NT_WEAKREF) {
        QoreObject* o = base.get<const WeakReferenceNode>()->get();
        if (!o || !o->isValid()) {
            xsink->raiseException("OBJECT-ALREADY-DELETED",
                "cannot call %s::%s() on a deleted weak reference",
                qc->getName(), method->getName());
            return toBits(QoreValue());
        }
        base = QoreValue(o);
    }

    ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        return toBits(QoreValue());
    }
    RuntimeConfig& rc = rc_get_current_ref();

    // If base is nothing and class is not <nothing>, use <nothing> pseudo class
    if (base.isNothing() && qc != QC_PSEUDONOTHING) {
        return toBits(qore_class_private::evalPseudoMethod(QC_PSEUDONOTHING, base, method->getName(),
            *arg_list, rc, xsink));
    }
    return toBits(qore_class_private::evalPseudoMethod(qc, method, variant, base, *arg_list, rc, xsink));
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_pseudo_method_direct(uint64_t base_bits,
        const QoreMethod* method, const QoreClass* qc,
        const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    return qore_rt_dot_eval_pseudo_method_direct_impl(base_bits, method, qc,
        variant, args, nullptr, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_pseudo_method_direct_with_inst(uint64_t base_bits,
        const QoreMethod* method, const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, const QoreTypeParamInstantiation* explicit_type_param_instantiation,
        ExceptionSink* xsink) {
    return qore_rt_dot_eval_pseudo_method_direct_impl(base_bits, method, qc,
        variant, args, nullptr, nargs, xsink, explicit_type_param_instantiation);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_pseudo_method_direct_consume_args(
        uint64_t base_bits, const QoreMethod* method, const QoreClass* qc,
        const AbstractQoreFunctionVariant* variant, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    return qore_rt_dot_eval_pseudo_method_direct_impl(base_bits, method, qc,
        variant, args, arg_cleanups, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_pseudo_method_direct_with_inst_consume_args(
        uint64_t base_bits, const QoreMethod* method, const QoreClass* qc,
        const AbstractQoreFunctionVariant* variant, uint64_t* args,
        uint64_t** arg_cleanups, int nargs,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation, ExceptionSink* xsink) {
    return qore_rt_dot_eval_pseudo_method_direct_impl(base_bits, method, qc,
        variant, args, arg_cleanups, nargs, xsink, explicit_type_param_instantiation);
}

// Fallback for unresolved method calls: use the pre-evaluated args
// from LLVM with a name-based runtime method dispatch
static uint64_t dot_eval_fallback_with_args(QoreValue base, const char* method_name,
        uint64_t* args, uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation) {
    QoreValue raw_base = base;
    ValueEvalOptimizedRefHolder base_holder(xsink);
    if (!qore_rt_dot_eval_preserve_raw_base(raw_base)) {
        base_holder.eval(raw_base);
        base = *base_holder;
    }
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }

    ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Unwrap weak references — method dispatch should be transparent
    if (base.getType() == NT_WEAKREF) {
        QoreObject* o = base.get<const WeakReferenceNode>()->get();
        if (!o || !o->isValid()) {
            xsink->raiseException("OBJECT-ALREADY-DELETED",
                "cannot call '%s()' on a deleted weak reference", method_name);
            return toBits(QoreValue());
        }
        base = QoreValue(o);
    }

    // For objects, use name-based method lookup with evalTmpArgs to preserve references
    if (base.getType() == NT_OBJECT) {
        QoreObject* o = const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(base.getInternalNode()));
        // copy() must go through execCopy to create a new object
        if (!strcmp(method_name, "copy")) {
            return toBits(o->getClass()->execCopy(o, xsink));
        }
        // issue #3596: do not use the context class if it's not compatible with "o".
        // When calling obj.method() on a DIFFERENT object (not self), the caller's
        // runtime class context must not leak into the method lookup — otherwise
        // runtimeFindCommittedMethodForEval picks up a private:internal method from
        // the caller's class even though the target object is a completely unrelated
        // class. Mirrors MethodCallNode::exec() in FunctionCallNode.cpp:928.
        const qore_class_private* class_ctx = runtime_get_class();
        if (class_ctx && !qore_class_private::parseCheckPrivateClassAccess(*o->getClass(), class_ctx)) {
            class_ctx = nullptr;
        }
        RuntimeConfig& rc = rc_get_current_ref();
        const qore_class_private* priv = qore_class_private::get(*o->getClass());
        return toBits(priv->evalMethod(o, method_name, *arg_list, class_ctx, rc, xsink,
            explicit_type_param_instantiation));
    }

    // Check for hash member closures/call references (e.g. h.f() where h.f is a closure)
    if (base.getType() == NT_HASH) {
        const AbstractQoreNode* ref = check_call_ref(base.getInternalNode(), method_name);
        if (ref) {
            return toBits(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(*arg_list, xsink));
        }
    } else if (base.getType() == NT_WEAKREF_HASH) {
        const AbstractQoreNode* ref = check_call_ref(base.get<WeakHashReferenceNode>()->get(), method_name);
        if (ref) {
            return toBits(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(*arg_list, xsink));
        }
    }

    // For non-objects, use pseudo-class lookup
    return toBits(pseudo_classes_eval(base, method_name, *arg_list, xsink));
}

DLLLOCAL uint64_t dot_eval_fallback_with_args(QoreValue base, const char* method_name,
        uint64_t* args, int nargs, ExceptionSink* xsink,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation) {
    return dot_eval_fallback_with_args(base, method_name, args, nullptr, nargs,
        xsink, explicit_type_param_instantiation);
}

// Exported wrapper for unresolved/abstract method calls from JIT LLVM code.
// Uses name-based dispatch with pre-evaluated args (avoids EXPR_TREE local variable issues).
extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_by_name(uint64_t base_bits, const char* method_name,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    QoreValue base = fromBits(base_bits);
    return dot_eval_fallback_with_args(base, method_name, args, nullptr, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_by_name_with_inst(uint64_t base_bits,
        const char* method_name, uint64_t* args, int nargs,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation, ExceptionSink* xsink) {
    QoreValue base = fromBits(base_bits);
    return dot_eval_fallback_with_args(base, method_name, args, nullptr, nargs, xsink,
        explicit_type_param_instantiation);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_by_name_consume_args(uint64_t base_bits,
        const char* method_name, uint64_t* args, uint64_t** arg_cleanups, int nargs,
        ExceptionSink* xsink) {
    QoreValue base = fromBits(base_bits);
    return dot_eval_fallback_with_args(base, method_name, args, arg_cleanups, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_by_name_with_inst_consume_args(uint64_t base_bits,
        const char* method_name, uint64_t* args, uint64_t** arg_cleanups, int nargs,
        const QoreTypeParamInstantiation* explicit_type_param_instantiation, ExceptionSink* xsink) {
    QoreValue base = fromBits(base_bits);
    return dot_eval_fallback_with_args(base, method_name, args, arg_cleanups, nargs, xsink,
        explicit_type_param_instantiation);
}

static const QoreTypeParamInstantiation* qore_rt_get_dot_eval_explicit_type_instantiation(
        QoreAOTContext* ctx, int32_t slot) {
    QoreValue expr = fromBits(ctx->exprs[slot]);
    auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(expr.getInternalNode());
    if (!dot_eval) {
        return nullptr;
    }
    MethodCallNode* call = dot_eval->getMethodCall();
    return call ? call->getExplicitTypeParamInstantiation() : nullptr;
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t base_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    // Use pre-resolved method target to avoid per-call dynamic_cast
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    const QoreTypeParamInstantiation* explicit_inst = qore_rt_get_dot_eval_explicit_type_instantiation(ctx, slot);
    if (target.method) {
        return target.is_pseudo
            ? qore_rt_dot_eval_pseudo_method_direct_impl(base_bits, target.method, target.qc,
                target.variant, args, nullptr, nargs, xsink, explicit_inst)
            : qore_rt_dot_eval_method_direct_impl(base_bits, target.method, target.qc,
                target.variant, args, nullptr, nargs, xsink, explicit_inst);
    }
    // Pre-resolved with name but no method pointer — use name-based dispatch
    if (target.method_name) {
        QoreValue base = fromBits(base_bits);
        return dot_eval_fallback_with_args(base, target.method_name, args, nullptr, nargs, xsink, explicit_inst);
    }

    return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
        "qore_rt_dot_eval_method_direct_aot", "missing pre-resolved dot-eval method target");
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_object_method_direct_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t base_bits, uint64_t* args,
        int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    QoreValue base = fromBits(base_bits);
    if (!target.method || target.is_pseudo || base.getType() != NT_OBJECT) {
        return qore_rt_dot_eval_method_direct_aot(ctx, slot, base_bits, args, nargs, xsink);
    }

    QoreObject* object = base.get<QoreObject>();
    uint64_t result;
    if (try_dispatch_method_fast(object, target.method, target.qc,
            target.variant, args, nullptr, nargs, xsink, result)) {
        return result;
    }
    ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
    return dispatch_method_on_object(object, target.method, target.qc, target.variant,
        *arg_list, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_object_getter_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
        const char* member_name, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    QoreValue base = fromBits(base_bits);
    if (!target.method || !target.qc || target.is_pseudo
            || base.getType() != NT_OBJECT) {
        return qore_rt_dot_eval_object_method_direct_aot(
            ctx, slot, base_bits, nullptr, 0, xsink);
    }
    QoreObject* object = base.get<QoreObject>();
    if (!object->isValid() || object->getClass() != target.qc) {
        return qore_rt_dot_eval_object_method_direct_aot(
            ctx, slot, base_bits, nullptr, 0, xsink);
    }
    ValueHolder value(object->getReferencedMemberNoMethod(
        member_name, target.qc, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(value->needsEval() ? value->eval(xsink) : value.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct_aot_consume_args(QoreAOTContext* ctx,
        int32_t slot, uint64_t base_bits, uint64_t* args, uint64_t** arg_cleanups,
        int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    const QoreTypeParamInstantiation* explicit_inst = qore_rt_get_dot_eval_explicit_type_instantiation(ctx, slot);
    if (target.method) {
        return target.is_pseudo
            ? qore_rt_dot_eval_pseudo_method_direct_impl(base_bits, target.method,
                target.qc, target.variant, args, arg_cleanups, nargs, xsink, explicit_inst)
            : qore_rt_dot_eval_method_direct_impl(base_bits, target.method,
                target.qc, target.variant, args, arg_cleanups, nargs, xsink, explicit_inst);
    }
    if (target.method_name) {
        QoreValue base = fromBits(base_bits);
        return dot_eval_fallback_with_args(base, target.method_name, args,
            arg_cleanups, nargs, xsink, explicit_inst);
    }

    return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
        "qore_rt_dot_eval_method_direct_aot_consume_args",
        "missing pre-resolved dot-eval method target");
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_pseudo_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t base_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    // Use pre-resolved method target to avoid per-call dynamic_cast
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    const QoreTypeParamInstantiation* explicit_inst = qore_rt_get_dot_eval_explicit_type_instantiation(ctx, slot);
    if (target.method) {
        return qore_rt_dot_eval_pseudo_method_direct_impl(base_bits, target.method, target.qc,
            target.variant, args, nullptr, nargs, xsink, explicit_inst);
    }
    if (target.method_name) {
        QoreValue base = fromBits(base_bits);
        return dot_eval_fallback_with_args(base, target.method_name, args, nullptr, nargs, xsink, explicit_inst);
    }

    return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
        "qore_rt_dot_eval_pseudo_method_direct_aot", "missing pre-resolved pseudo dot-eval target");
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_pseudo_method_direct_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t base_bits, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    const QoreTypeParamInstantiation* explicit_inst = qore_rt_get_dot_eval_explicit_type_instantiation(ctx, slot);
    if (target.method) {
        return qore_rt_dot_eval_pseudo_method_direct_impl(base_bits,
            target.method, target.qc, target.variant, args, arg_cleanups,
            nargs, xsink, explicit_inst);
    }
    if (target.method_name) {
        QoreValue base = fromBits(base_bits);
        return dot_eval_fallback_with_args(base, target.method_name, args,
            arg_cleanups, nargs, xsink, explicit_inst);
    }

    return qore_rt_raise_aot_ast_fallback(ctx, slot, xsink,
        "qore_rt_dot_eval_pseudo_method_direct_aot_consume_args",
        "missing pre-resolved pseudo dot-eval target");
}

// --- Direct static method call (pre-evaluated args) ---

static uint64_t qore_rt_call_static_method_direct_impl(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink,
        const QoreTypeInfo* receiver_type_info) {
    if (!method) {
        xsink->raiseException("JIT-ERROR", "null method pointer in static method direct call");
        return toBits(QoreValue());
    }

    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }

    // variant may be nullptr for AOT-deserialized StaticMethodCallNode nodes
    // (resolveExprSlot creates nodes without a resolved variant pointer).
    // Fall through to the slow path in that case.
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    if (!uvb || !qore_rt_method_fast_call_eligible(variant)
            || (receiver_type_info && QoreTypeInfo::getParameterizedClassType(receiver_type_info))
            || (!uvb->hasCachedFunction() && !uvb->getCachedIR())) {
        // Use evalTmpArgs to preserve ReferenceNode values in pre-evaluated args
        ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
        if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
            return toBits(QoreValue());
        }
        return toBits(qore_method_private::evalTmpArgs(*method, xsink, rc_get_current_ref(), nullptr, *arg_list,
            nullptr, variant, receiver_type_info));
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Capture the caller's program BEFORE ProgramThreadCountContextHelper switches
    // current_pgm to uvb->pgm.  We need this for QoreJITStackLocation so that
    // Program::getCallerCapabilityMask() walks back to the caller's parse options,
    // matching the source/CEH semantics where the CEH frame captures the caller's
    // current_pgm at push time.
    QoreProgram* caller_pgm = getProgram();
    QoreProgram* exec_pgm = qore_rt_method_execution_program(method, uvb);

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, exec_pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }
    // Push frame boundary so that get_local_vars()/set_local_var_value() can correctly
    // determine call-stack depth for debugger introspection.
    ThreadFrameBoundaryHelper tfbh(true);

    // Instantiate parameter locals directly from NaN-boxed args
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink, receiver_type_info) < 0) {
        return toBits(QoreValue());
    }

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable (if the function has an argv parameter)
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }
    if (clearConsumedArgCleanups(arg_cleanups, nargs, xsink) < 0) {
        if (sig->argvid) {
            sig->argvid->uninstantiate(xsink);
        }
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
        return toBits(QoreValue());
    }

    // Use cached IR name when available (zero allocation); fall back to building the name
    const QoreIRFunction* ir = uvb->getCachedIR();
    std::string call_name_buf;
    if (!ir) {
        call_name_buf = std::string(method->getClass()->getName()) + "::" + method->getName();
    }
    const std::string& call_name = ir ? ir->getDisplayName() : call_name_buf;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        // Set class context for private method access (matches AST's
        // ObjectSubstitutionHelper in StaticMethodFunction::evalMethod)
        ClassOnlySubstitutionHelper cosh(qore_class_private::get(*method->getClass()));
        if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink, caller_pgm, receiver_type_info, exec_pgm);
        } else {
            // IR fast path: execute IR directly without QoreListNode construction.
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            execJITWithDeopt(uvb, call_name, [callee_ir, uvb, exec_pgm](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), exec_pgm);
                if (!ok && !*xs) {
                    inv = true;  // Request deopt to AST
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink, caller_pgm, receiver_type_info, exec_pgm);
        }
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    // Uninstantiate parameter locals in reverse order
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = qore_rt_get_effective_return_type(sig, receiver_type_info);
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_direct(
        const QoreMethod* method, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    return qore_rt_call_static_method_direct_impl(method, variant, args,
            nullptr, nargs, xsink, nullptr);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_direct_v2(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink) {
    // Fast path for AOT: delegates to qore_rt_call_static_method_direct with guaranteed
    // non-null variant embedded as integer constant at compile time.
    // This allows the fast path (direct variant access without null check) to be taken.
    return qore_rt_call_static_method_direct(method, variant, args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, nullptr, nargs, xsink,
            "qore_rt_call_static_method_direct_aot", "static_method",
            "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }

    // Use pre-resolved method target (populated during buildAOTContext) to avoid per-call
    // dynamic_cast and nullptr variant slow path
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.method) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_static_method_direct_aot",
            "static_method", "use");
        return qore_rt_call_static_method_direct_impl(target.method, target.variant, args, nullptr, nargs, xsink,
                target.receiver_type_info);
    }
    if (target.func) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_static_method_direct_aot",
            "function", "use");
        if (target.variant) {
            if (!qore_rt_user_fast_call_eligible(target.variant)) {
                return qore_rt_call_function_direct(target.func, target.variant,
                    target.pgm, args, nargs, xsink);
            }
            return qore_rt_call_fast(target.func, target.variant, target.pgm, args, nargs, xsink);
        }
        return qore_rt_call_function_dynamic(target.func, target.pgm, args, nargs, xsink);
    }

    return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
        "qore_rt_call_static_method_direct_aot", "static_method",
        "missing pre-resolved static method target");
}

extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_direct_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, arg_cleanups, nargs, xsink,
            "qore_rt_call_static_method_direct_aot_consume_args", "static_method",
            "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.method) {
        qore_rt_trace_aot_prelink(ctx, slot,
            "qore_rt_call_static_method_direct_aot_consume_args", "static_method", "use");
        return qore_rt_call_static_method_direct_impl(target.method,
                target.variant, args, arg_cleanups, nargs, xsink,
                target.receiver_type_info);
    }
    if (target.func) {
        qore_rt_trace_aot_prelink(ctx, slot,
            "qore_rt_call_static_method_direct_aot_consume_args", "function", "use");
        if (target.variant) {
            return qore_rt_call_function_direct_impl(target.func, target.variant,
                target.pgm, args, arg_cleanups, nargs, xsink);
        }
        return qore_rt_call_function_dynamic_impl(target.func, target.pgm, args,
            arg_cleanups, nargs, xsink);
    }

    return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
        "qore_rt_call_static_method_direct_aot_consume_args", "static_method",
        "missing pre-resolved static method target");
}

extern "C" DLLEXPORT uint64_t qore_rt_call_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, nullptr, nargs, xsink,
            "qore_rt_call_method_direct_aot", "method", "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }

    // Use pre-resolved method target to avoid per-call dynamic_cast
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.method) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_method_direct_aot", "method", "use");
        if (target.is_self_method) {
            return qore_rt_call_self_method_dispatch_impl(target, args, nullptr, nargs, xsink);
        }
        return qore_rt_call_method_direct(target.method, args, nargs, xsink);
    }

    return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
        "qore_rt_call_method_direct_aot", "method", "missing pre-resolved method target");
}

extern "C" DLLEXPORT uint64_t qore_rt_call_method_direct_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, arg_cleanups, nargs, xsink,
            "qore_rt_call_method_direct_aot_consume_args", "method",
            "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.method) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_method_direct_aot_consume_args",
            "method", "use");
        if (target.is_self_method) {
            return qore_rt_call_self_method_dispatch_impl(target, args, arg_cleanups, nargs, xsink);
        }
        return qore_rt_call_method_direct_impl(target.method, args, arg_cleanups,
            nargs, xsink);
    }

    return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
        "qore_rt_call_method_direct_aot_consume_args", "method",
        "missing pre-resolved method target");
}

//! Fast path for AOT method calls: uses pre-resolved variant when available (avoids overload resolution)
extern "C" DLLEXPORT uint64_t qore_rt_call_method_fast_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, nullptr, nargs, xsink,
            "qore_rt_call_method_fast_aot", "method", "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    // Use fast path if variant is available and statically eligible for fast calls
    if (target.method && !target.is_self_method && target.uvb
            && qore_rt_method_fast_call_eligible(target.variant)) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_method_fast_aot", "method", "use");
        return qore_rt_call_method_fast(target.method, target.variant, args, nargs, xsink);
    }
    if (!target.method) {
        return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
            "qore_rt_call_method_fast_aot", "method", "missing pre-resolved method target");
    }

    // Fall back to standard method dispatch (with overload resolution)
    qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_method_fast_aot", "method", "use");
    if (target.is_self_method) {
        return qore_rt_call_self_method_dispatch_impl(target, args, nullptr, nargs, xsink);
    }
    return qore_rt_call_method_direct(target.method, args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_method_fast_aot_consume_args(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args,
        uint64_t** arg_cleanups, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, arg_cleanups, nargs, xsink,
            "qore_rt_call_method_fast_aot_consume_args", "method",
            "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.method && !target.is_self_method && target.uvb
            && qore_rt_method_fast_call_eligible(target.variant)) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_method_fast_aot_consume_args",
            "method", "use");
        return qore_rt_call_method_fast_impl(target.method, target.variant, args,
            arg_cleanups, nargs, xsink);
    }

    if (target.method) {
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_method_fast_aot_consume_args",
            "method", "use");
        if (target.is_self_method) {
            return qore_rt_call_self_method_dispatch_impl(target, args, arg_cleanups, nargs, xsink);
        }
        return qore_rt_call_method_direct_impl(target.method, args, arg_cleanups,
            nargs, xsink);
    }

    return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
        "qore_rt_call_method_fast_aot_consume_args", "method",
        "missing pre-resolved method target");
}

//! Fast path for AOT static method calls: uses pre-resolved variant when available
extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_fast_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    if (qore_rt_aot_disable_prelinked_calls()) {
        return qore_rt_call_slot_dynamic_fallback(ctx, slot, args, nullptr, nargs, xsink,
            "qore_rt_call_static_method_fast_aot", "static_method",
            "QORE_AOT_DISABLE_PRELINKED_CALLS");
    }

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (!target.method) {
        if (target.func) {
            qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_static_method_fast_aot",
                "function", "use");
            if (target.variant) {
                if (!qore_rt_user_fast_call_eligible(target.variant)) {
                    return qore_rt_call_function_direct(target.func, target.variant,
                        target.pgm, args, nargs, xsink);
                }
                return qore_rt_call_fast(target.func, target.variant, target.pgm, args, nargs, xsink);
            }
            return qore_rt_call_function_dynamic(target.func, target.pgm, args, nargs, xsink);
        }
        return qore_rt_missing_prelinked_call_target(ctx, slot, xsink,
            "qore_rt_call_static_method_fast_aot", "static_method",
            "missing pre-resolved static method target");
    }
    // Check if the variant is statically eligible for fast calls (not synchronized, no default args)
    if (target.uvb && qore_rt_method_fast_call_eligible(target.variant)) {
        // Use fast call path directly
        qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_static_method_fast_aot",
            "static_method", "use");
        return qore_rt_call_static_method_direct_impl(target.method, target.variant, args, nullptr, nargs, xsink,
                target.receiver_type_info);
    }

    // Fall back to standard static method dispatch
    qore_rt_trace_aot_prelink(ctx, slot, "qore_rt_call_static_method_fast_aot",
        "static_method", "use");
    return qore_rt_call_static_method_direct_impl(target.method, target.variant, args, nullptr, nargs, xsink,
            target.receiver_type_info);
}

extern "C" DLLEXPORT uint64_t qore_rt_switch_case_match(const void* case_node_ptr, uint64_t switch_val_bits,
        ExceptionSink* xsink) {
    const CaseNode* cn = reinterpret_cast<const CaseNode*>(case_node_ptr);
    QoreValue switch_val;
    std::memcpy(&switch_val, &switch_val_bits, sizeof(switch_val));
    bool match = cn->matches(switch_val, xsink);
    return toBits(QoreValue(match));
}

// AOT-safe switch case match: case value embedded directly (no CaseNode pointer)
extern "C" DLLEXPORT uint64_t qore_rt_switch_case_match_value(uint64_t case_val_bits,
        uint64_t switch_val_bits, ExceptionSink* xsink) {
    QoreValue case_val = fromBits(case_val_bits);
    QoreValue switch_val = fromBits(switch_val_bits);
    ValueEvalOptimizedRefHolder case_eval(case_val, xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue(false));
    }
    QoreValue eval_case_val = case_eval.takeReferencedValue();
    bool match = qore_switch_case_equal(switch_val, eval_case_val, xsink);
    eval_case_val.discard(xsink);
    return toBits(QoreValue(match));
}

// AOT-safe switch case match: case value loaded from expression slot at runtime
// (node values like QoreStringNode contain process-specific pointers that can't
// be embedded as LLVM constants in AOT .qmod files)
extern "C" DLLEXPORT uint64_t qore_rt_switch_case_match_value_aot(QoreAOTContext* ctx,
        int32_t case_slot, uint64_t switch_val_bits, ExceptionSink* xsink) {
    return qore_rt_switch_case_match_value(ctx->exprs[case_slot], switch_val_bits, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_pseudo_list_bool_guarded(uint64_t base_bits,
        const QoreMethod* method, const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        int32_t invert_empty, ExceptionSink* xsink) {
    QoreValue base = fromBits(base_bits);
    if (base.getType() == NT_LIST) {
        bool empty = base.get<const QoreListNode>()->empty();
        return toBits(QoreValue(invert_empty ? !empty : empty));
    }
    if (base.isNothing()) {
        return toBits(QoreValue(!invert_empty));
    }
    return qore_rt_dot_eval_pseudo_method_direct(base_bits, method, qc, variant, nullptr, 0, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_pseudo_list_bool_guarded_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t base_bits, int32_t invert_empty, ExceptionSink* xsink) {
    QoreValue base = fromBits(base_bits);
    if (base.getType() == NT_LIST) {
        bool empty = base.get<const QoreListNode>()->empty();
        return toBits(QoreValue(invert_empty ? !empty : empty));
    }
    if (base.isNothing()) {
        return toBits(QoreValue(!invert_empty));
    }
    return qore_rt_dot_eval_pseudo_method_direct_aot(ctx, slot, base_bits, nullptr, 0, xsink);
}

static uint64_t qore_rt_list_first_last(uint64_t base_bits, bool last) {
    QoreValue base = fromBits(base_bits);
    const QoreListNode* list = base.get<const QoreListNode>();
    size_t size = list->size();
    if (!size) {
        return toBits(QoreValue());
    }

    QoreValue result = list->retrieveEntry(last ? size - 1 : 0);
    if (result.hasNode()) {
        result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_pseudo_list_value_guarded(uint64_t base_bits,
        const QoreMethod* method, const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        int32_t last, ExceptionSink* xsink) {
    QoreValue base = fromBits(base_bits);
    if (base.getType() == NT_LIST) {
        return qore_rt_list_first_last(base_bits, last != 0);
    }
    return qore_rt_dot_eval_pseudo_method_direct(base_bits, method, qc, variant, nullptr, 0, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_pseudo_list_value_guarded_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t base_bits, int32_t last, ExceptionSink* xsink) {
    QoreValue base = fromBits(base_bits);
    if (base.getType() == NT_LIST) {
        return qore_rt_list_first_last(base_bits, last != 0);
    }
    return qore_rt_dot_eval_pseudo_method_direct_aot(ctx, slot, base_bits, nullptr, 0, xsink);
}

// ============================================================================
// Phase 2: Optimized pseudo-method helpers for LLVM JIT (faster than dispatch)
// ============================================================================

//! Fast pseudo-method: typeCode() - return type code as NaN-boxed int
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_typeCode(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    return toBits(QoreValue(static_cast<int64_t>(v.getType())));
}

//! Fast pseudo-method: size()/strlen() - return byte/container size as NaN-boxed int.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_size(uint64_t val_bits) {
    // Mirror `<type>::size()` and `<string>::strlen()`. `<string>::length()`
    // is character-oriented and must use qore_rt_pseudo_length().
    QoreValue v = fromBits(val_bits);
    int64_t size = 0;
    switch (v.getType()) {
        case NT_LIST:
            size = static_cast<int64_t>(v.get<const QoreListNode>()->size());
            break;
        case NT_STRING:
            {
                QoreStringValueHelper str(v);
                size = static_cast<int64_t>(str->strlen());
            }
            break;
        case NT_HASH:
            size = static_cast<int64_t>(v.get<const QoreHashNode>()->size());
            break;
        case NT_BINARY:
            size = static_cast<int64_t>(v.get<const BinaryNode>()->size());
            break;
        default:
            break;
    }
    return toBits(QoreValue(size));
}

//! Fast pseudo-method: <string>::length() - return character length as NaN-boxed int.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_length(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    if (v.getType() == NT_STRING) {
        QoreStringValueHelper str(v);
        return toBits(QoreValue(static_cast<int64_t>(str->length())));
    }
    return toBits(QoreValue(static_cast<int64_t>(0)));
}

//! Fast pseudo-method: empty() - return true if size == 0
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_empty(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    // Container types measure their own content; every other type
    // (NOTHING, bool, int, float, number, date, binary-less, ...) inherits
    // `<value>::empty()` which returns true unconditionally. Previously the
    // default branch returned false, so e.g. NOTHING.empty() produced false —
    // the opposite of the AST/pseudo-class behavior.
    bool is_empty = true;
    switch (v.getType()) {
        case NT_LIST:
            is_empty = v.get<const QoreListNode>()->empty();
            break;
        case NT_STRING:
            {
                QoreStringValueHelper str(v);
                is_empty = str->strlen() == 0;
            }
            break;
        case NT_HASH:
            is_empty = v.get<const QoreHashNode>()->empty();
            break;
        case NT_BINARY:
            is_empty = v.get<const BinaryNode>()->empty();
            break;
        default:
            break;
    }
    return toBits(QoreValue(is_empty));
}

//! Fast pseudo-method: val() - true if the value has content
/** Mirror of `<type>::val()` pseudo-method dispatch: each primitive pseudo
    class defines its own val() (int != 0, bool, date.hasValue, number != 0,
    etc.). `<value>::val()` is the fallback and returns false — so the
    default branch here must stay false for types we don't specifically
    recognize (callref and object are RET_VALUE_ONLY/may raise — leave to
    the generic dispatch in qore_rt_dot_eval_pseudo_method_direct).
*/
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_val(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    bool has_value = false;
    switch (v.getType()) {
        case NT_LIST:
            has_value = !v.get<const QoreListNode>()->empty();
            break;
        case NT_STRING:
            {
                QoreStringValueHelper str(v);
                has_value = str->strlen() != 0;
            }
            break;
        case NT_HASH:
            has_value = !v.get<const QoreHashNode>()->empty();
            break;
        case NT_BINARY:
            has_value = !v.get<const BinaryNode>()->empty();
            break;
        case NT_BOOLEAN:
            has_value = v.getAsBool();
            break;
        case NT_INT:
            has_value = v.getAsBigInt() != 0;
            break;
        case NT_FLOAT:
            has_value = v.getAsFloat() != 0.0;
            break;
        case NT_NUMBER: {
            const QoreNumberNode* n = v.get<const QoreNumberNode>();
            has_value = n && !n->zero();
            break;
        }
        case NT_DATE: {
            const DateTimeNode* dt = v.get<const DateTimeNode>();
            has_value = dt && dt->hasValue();
            break;
        }
        default:
            break;
    }
    return toBits(QoreValue(has_value));
}

//! Fast no-guard pseudo-method: <string>::empty() for bases proven to be assigned strings.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_empty_noguard(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    if (v.isShortString()) {
        return toBits(QoreValue(v.shortStringLen() == 0));
    }
    if (v.getType() == NT_STRING) {
        const QoreStringNode* str = v.get<const QoreStringNode>();
        return toBits(QoreValue(!str || str->empty()));
    }
    return qore_rt_pseudo_empty(val_bits);
}

//! Fast no-guard pseudo-method: <string>::val() for bases proven to be assigned strings.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_val_noguard(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    if (v.isShortString()) {
        return toBits(QoreValue(v.shortStringLen() != 0));
    }
    if (v.getType() == NT_STRING) {
        const QoreStringNode* str = v.get<const QoreStringNode>();
        return toBits(QoreValue(str && !str->empty()));
    }
    return qore_rt_pseudo_val(val_bits);
}

//! Fast no-guard pseudo-method: <string>::size()/strlen() for bases proven to be assigned strings.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_size_noguard(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    if (v.isShortString()) {
        return toBits(QoreValue(static_cast<int64_t>(v.shortStringLen())));
    }
    if (v.getType() == NT_STRING) {
        const QoreStringNode* str = v.get<const QoreStringNode>();
        return toBits(QoreValue(static_cast<int64_t>(str ? str->strlen() : 0)));
    }
    return qore_rt_pseudo_size(val_bits);
}

static size_t qore_short_string_utf8_length(QoreValue v) {
    assert(v.isShortString());
    char buf[7];
    v.getShortString(buf);

    const char* p = buf;
    const char* end = buf + v.shortStringLen();
    size_t len = 0;
    while (p < end) {
        if (static_cast<unsigned char>(*p) < 0x80) {
            ++p;
            ++len;
            continue;
        }

        qore_offset_t char_len = q_UTF8_get_char_len(p, end - p);
        if (char_len <= 0) {
            return len;
        }
        p += char_len;
        ++len;
    }
    return len;
}

//! Fast no-guard pseudo-method: <string>::length() for bases proven to be assigned strings.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_length_noguard(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    if (v.isShortString()) {
        return toBits(QoreValue(static_cast<int64_t>(qore_short_string_utf8_length(v))));
    }
    if (v.getType() == NT_STRING) {
        const QoreStringNode* str = v.get<const QoreStringNode>();
        return toBits(QoreValue(static_cast<int64_t>(str ? str->length() : 0)));
    }
    return qore_rt_pseudo_length(val_bits);
}

//! Fast no-guard pseudo-method: <string>::sizep() for bases known as string/NOTHING.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_sizep_noguard(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    return toBits(QoreValue(v.isShortString() || v.getType() == NT_STRING));
}

//! Fast no-guard pseudo-method: <string>::intp() for bases known as string/NOTHING/NULL.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_intp_noguard(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    if (!v.isShortString() && v.getType() != NT_STRING) {
        return toBits(QoreValue(false));
    }
    QoreStringValueHelper str(v);
    if (str->empty()) {
        return toBits(QoreValue(false));
    }
    const char* data = str->c_str();
    char c = data[0];
    if (c == '-') {
        c = data[1];
    }
    return toBits(QoreValue(isdigit(static_cast<unsigned char>(c)) ? true : false));
}

//! Fast no-guard pseudo-method: <string>::strp() for bases known as string/NOTHING/NULL.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_strp_noguard(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    return toBits(QoreValue(v.isShortString() || v.getType() == NT_STRING));
}

//! Fast no-guard pseudo-methods: <string>::startsWith()/endsWith()/contains() for assigned string operands.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_predicate_noguard(uint64_t val_bits,
        uint64_t arg_bits, int32_t predicate, ExceptionSink* xsink) {
    QoreValue v = fromBits(val_bits);
    QoreStringValueHelper str(v);
    QoreValue arg = fromBits(arg_bits);

    auto eval_predicate = [&](const char* pattern_ptr) -> uint64_t {
        bool result = false;
        switch (predicate) {
            case 0:
                result = str->startsWith(pattern_ptr);
                break;
            case 1:
                result = str->endsWith(pattern_ptr);
                break;
            case 2:
                result = str->find(pattern_ptr) >= 0;
                break;
            default:
                if (xsink) {
                    xsink->raiseException("IR-EXEC-ERROR", "invalid string predicate fast-path id %d",
                        static_cast<int>(predicate));
                }
                return toBits(QoreValue());
        }
        return toBits(QoreValue(result));
    };

    if (arg.isShortString() && str->getEncoding() == QCS_UTF8) {
        char short_pattern[7];
        arg.getShortString(short_pattern);
        return eval_predicate(short_pattern);
    }

    QoreStringValueHelper pattern(arg, str->getEncoding(), xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return eval_predicate(pattern->c_str());
}

//! Fast no-guard pseudo-method: <string>::find() for assigned string base and substring operands.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_find_noguard(uint64_t val_bits,
        uint64_t substring_bits, int64_t offset, ExceptionSink* xsink) {
    QoreValue v = fromBits(val_bits);
    QoreStringValueHelper str(v);
    QoreValue substring = fromBits(substring_bits);
    QoreStringValueHelper pattern(substring, str->getEncoding(), xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    qore_offset_t result = str->index(**pattern, offset, xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(static_cast<int64_t>(result)));
}

//! Fast no-guard pseudo-method: <string>::rfind() for assigned string base and substring operands.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_rfind_noguard(uint64_t val_bits,
        uint64_t substring_bits, int64_t offset, ExceptionSink* xsink) {
    QoreValue v = fromBits(val_bits);
    QoreStringValueHelper str(v);
    QoreValue substring = fromBits(substring_bits);
    QoreStringValueHelper pattern(substring, str->getEncoding(), xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    qore_offset_t result = str->rindex(**pattern, offset, xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(static_cast<int64_t>(result)));
}

//! Fast no-guard pseudo-method: <string>::substr() for assigned string base and int operands.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_substr_noguard(uint64_t val_bits,
        int64_t start, int64_t length, int32_t has_length, ExceptionSink* xsink) {
    QoreValue v = fromBits(val_bits);
    QoreStringNodeValueHelper str(v);
    QoreStringNode* result = has_length
        ? str->substr(start, length, xsink)
        : str->substr(start, xsink);
    if (xsink && *xsink) {
        return toBits(QoreValue());
    }
    if (!result) {
        result = new QoreStringNode(str->getEncoding());
    }
    return toBits(QoreValue(result));
}

static uint64_t qore_rt_pseudo_string_case_noguard(uint64_t val_bits, ExceptionSink* xsink, bool upper) {
    QoreValue v = fromBits(val_bits);
    QoreStringValueHelper str(v);
    SimpleRefHolder<QoreStringNode> rv(new QoreStringNode(str->getEncoding()));
    int rc = upper ? do_toupper(*(*rv), *str, xsink) : do_tolower(*(*rv), *str, xsink);
    if (rc || *xsink) {
        return toBits(QoreValue());
    }
    return toBits(rv.release());
}

//! Fast no-guard pseudo-method: <string>::lwr() for bases proven to be assigned strings.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_lwr_noguard(uint64_t val_bits, ExceptionSink* xsink) {
    return qore_rt_pseudo_string_case_noguard(val_bits, xsink, false);
}

//! Fast no-guard pseudo-method: <string>::upr() for bases proven to be assigned strings.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_upr_noguard(uint64_t val_bits, ExceptionSink* xsink) {
    return qore_rt_pseudo_string_case_noguard(val_bits, xsink, true);
}

//! Fast pseudo-method: <string>::toInt() for bases known as string/NOTHING/NULL.
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_string_to_int_noguard(uint64_t val_bits, ExceptionSink* xsink) {
    QoreValue v = fromBits(val_bits);
    if (!v.isShortString() && v.getType() != NT_STRING) {
        return toBits(QoreValue(v.getAsBigInt()));
    }
    if (v.isShortString()) {
        char buf[7];
        v.getShortString(buf);
        return toBits(QoreValue(strtoll(buf, nullptr, 10)));
    }
    QoreStringValueHelper str(v);
    if (!str->getEncoding()->isAsciiCompat()) {
        if (xsink) {
            xsink->raiseException("UNSUPPORTED-ENCODING", "cannot convert string in non-ASCII-compatible "
                "encoding \"%s\" to an integer", str->getEncoding()->getCode());
        }
        return toBits(QoreValue());
    }
    return toBits(QoreValue(strtoll(str->c_str(), nullptr, 10)));
}

//! Fast pseudo-method: type() - return type name string
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_type(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    return toBits(QoreValue::makeStringValue(v.getTypeName()));
}

//! Fast pseudo-method: <value>::toNumber().
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_toNumber(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    return toBits(QoreValue(QoreNumberNode::toNumber(v)));
}

namespace {
QoreValue doBackgroundWithLocation(const QoreProgramLocation* node_loc, QoreValue expr, ExceptionSink* xsink);
}

//! Background self-method call (JIT mode): takes SelfFunctionCallNode* for method identity
//! and pre-evaluated args — avoids EXPR_TREE serialization in AOT
extern "C" DLLEXPORT uint64_t qore_rt_background_self_call(
        const SelfFunctionCallNode* sfcn, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(sfcn);

    // Build QoreListNode from pre-evaluated args
    QoreListNode* arg_list = nullptr;
    if (nargs > 0) {
        arg_list = new QoreListNode(autoTypeInfo);
        qore_list_private* priv = qore_list_private::get(*arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; i++) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }

    // Create SetSelfFunctionCallNode which captures current self/class context
    // do_op_background copies the expression via copy_value_and_resolve_lvar_refs;
    // the original must be freed after. SetSelfFunctionCallNode::deref(ExceptionSink*)
    // shadows AbstractQoreNode::deref and only handles self — so we must call both:
    // the shadowing deref for self, then discard for the node itself.
    SetSelfFunctionCallNode* call_node = new SetSelfFunctionCallNode(*sfcn, arg_list);
    QoreValue result = doBackgroundWithLocation(sfcn->loc, QoreValue(call_node), xsink);
    call_node->deref(xsink);           // derefs self (the shadowing override)
    QoreValue(call_node).discard(xsink); // decrements refcount and frees the node
    return toBits(result);
}

//! Background self-method call (AOT mode): takes method name string for runtime resolution
extern "C" DLLEXPORT uint64_t qore_rt_background_self_call_aot(
        const char* method_name, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(method_name);

    // Get current self + class context
    QoreObject* self = runtime_get_stack_object();
    if (!self) {
        xsink->raiseException("BACKGROUND-ERROR", "no current object for background method call to '%s'",
            method_name);
        return toBits(QoreValue());
    }

    // Resolve method by name on the current class
    const qore_class_private* cls = runtime_get_class();
    if (!cls) {
        cls = qore_class_private::get(*self->getClass());
    }
    ClassAccess access = Public;
    const QoreMethod* method = cls->runtimeFindCommittedMethod(method_name, access, cls);
    if (!method) {
        xsink->raiseException("BACKGROUND-ERROR", "cannot resolve method '%s' on class '%s'",
            method_name, cls->name.c_str());
        return toBits(QoreValue());
    }

    // Build QoreListNode from pre-evaluated args
    QoreListNode* arg_list = nullptr;
    if (nargs > 0) {
        arg_list = new QoreListNode(autoTypeInfo);
        qore_list_private* priv = qore_list_private::get(*arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; i++) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }

    // Create a temporary SelfFunctionCallNode with resolved method identity
    // strdup gives NamedScope ownership of the name string (del=true)
    SelfFunctionCallNode temp_sfcn(&loc_builtin, strdup(method_name),
        nullptr, method, method->getClass(), cls);

    // Create SetSelfFunctionCallNode which captures current self/class context and takes arg_list ownership
    // do_op_background copies the expression; the original must be freed after.
    // SetSelfFunctionCallNode::deref(xsink) shadows AbstractQoreNode::deref and only handles self.
    SetSelfFunctionCallNode* call_node = new SetSelfFunctionCallNode(temp_sfcn, arg_list);
    QoreValue result = doBackgroundWithLocation(&loc_builtin, QoreValue(call_node), xsink);
    call_node->deref(xsink);           // derefs self (the shadowing override)
    QoreValue(call_node).discard(xsink); // decrements refcount and frees the node
    return toBits(result);
}

namespace {
// Helper — build a QoreListNode owning a ref on each boxed QoreValue in args[].
// Returns nullptr for zero args (FunctionCallNode / crlr_list_copy semantics).
QoreListNode* bgBuildArgList(uint64_t* args, int nargs) {
    if (nargs <= 0) {
        return nullptr;
    }
    QoreListNode* arg_list = new QoreListNode(autoTypeInfo);
    qore_list_private* priv = qore_list_private::get(*arg_list);
    priv->reserve(nargs);
    for (int i = 0; i < nargs; ++i) {
        QoreValue val = fromBits(args[i]);
        if (val.hasNode()) {
            val.refSelf();
        }
        priv->pushIntern(val);
    }
    return arg_list;
}

QoreValue doBackgroundWithLocation(const QoreProgramLocation* node_loc, QoreValue expr, ExceptionSink* xsink) {
    const QoreProgramLocation* loc = get_runtime_location();
    QoreProgramLocationHelper loc_helper(loc ? loc : (node_loc ? node_loc : &loc_builtin));
    return do_op_background(expr, xsink);
}
} // anonymous

//! Background free function call (JIT mode): takes FunctionCallNode* for identity
//! and pre-evaluated args — avoids EXPR_TREE serialization and lets the spawned
//! thread participate in JIT/IR tiering via the FunctionCallNode's evalImpl.
extern "C" DLLEXPORT uint64_t qore_rt_background_function_call(
        const FunctionCallNode* fcn, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(fcn);
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    // Clone with pre-evaluated args (tmp_args=true via AbstractFunctionCallNode copy ctor).
    // do_op_background will make its own copy via crlr_fcall_copy; we release the
    // original here so the local FunctionCallNode's destructor cleans up.
    FunctionCallNode* call_node = new FunctionCallNode(*fcn, arg_list);
    QoreValue result = doBackgroundWithLocation(fcn->loc, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

//! Background static method call (JIT mode)
extern "C" DLLEXPORT uint64_t qore_rt_background_static_method_call(
        const StaticMethodCallNode* smcn, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(smcn);
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    StaticMethodCallNode* call_node = new StaticMethodCallNode(*smcn, arg_list);
    QoreValue result = doBackgroundWithLocation(smcn->loc, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

//! Background dot-eval method call (JIT mode): takes the QoreDotEvalOperatorNode
//! as template (for method identity / name) + a pre-evaluated receiver bit-value
//! + pre-evaluated args.  Rebuilds a new QoreDotEvalOperatorNode wrapping a fresh
//! MethodCallNode and hands it to do_op_background.
extern "C" DLLEXPORT uint64_t qore_rt_background_dot_eval_call(
        const QoreDotEvalOperatorNode* devn, uint64_t recv_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(devn);
    MethodCallNode* source_m = devn->getMethodCall();
    assert(source_m);
    QoreValue recv = fromBits(recv_bits);
    if (recv.hasNode()) {
        recv.refSelf();  // node takes ownership of this ref
    }
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    MethodCallNode* new_m = new MethodCallNode(*source_m, arg_list);
    QoreDotEvalOperatorNode* call_node =
        new QoreDotEvalOperatorNode(devn->loc, recv, new_m);
    QoreValue result = doBackgroundWithLocation(devn->loc, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

//! Background call-ref / closure / method-ref invocation (JIT mode):
//! takes the CallReferenceCallNode as template (source loc) + a pre-evaluated
//! callable bit-value + pre-evaluated args.
extern "C" DLLEXPORT uint64_t qore_rt_background_call_ref_call(
        const CallReferenceCallNode* crcn, uint64_t callee_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(crcn);
    QoreValue callee = fromBits(callee_bits);
    if (callee.hasNode()) {
        callee.refSelf();
    }
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    CallReferenceCallNode* call_node =
        new CallReferenceCallNode(crcn->loc, callee, arg_list);
    QoreValue result = doBackgroundWithLocation(crcn->loc, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

// === AOT variants for the four new background shapes ===
//
// In AOT mode, the compiled code doesn't embed AST pointers (they're not stable
// across runs).  Instead, the per-function QoreAOTContext holds a slot table
// (`exprs[]`) populated at load time with freshly-deserialised expression
// pointers.  Each AOT helper below reads the serialised QoreBackgroundOperatorNode
// from the slot and dispatches on its inner-expression type — identical to the
// JIT path but with the node sourced from the slot rather than a linker-embedded
// pointer.  This lets AOT-loaded modules avoid the qore_rt_invoke_expr_aot AST
// trampoline for backgrounded calls.

static const QoreBackgroundOperatorNode* getAOTBackgroundSlotOp(QoreAOTContext* ctx, int32_t slot,
        const char* helper, ExceptionSink* xsink) {
    if (!ctx || slot < 0 || slot >= ctx->num_exprs) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "%s received invalid background expression slot %d", helper, slot);
        return nullptr;
    }
    QoreValue bg_expr = fromBits(ctx->exprs[slot]);
    auto* bg_op = dynamic_cast<const QoreBackgroundOperatorNode*>(bg_expr.getInternalNode());
    if (!bg_op) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "%s cannot resolve background expression slot %d", helper, slot);
        return nullptr;
    }
    return bg_op;
}

extern "C" DLLEXPORT uint64_t qore_rt_background_function_call_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    auto* bg_op = getAOTBackgroundSlotOp(ctx, slot, "qore_rt_background_function_call_aot", xsink);
    if (!bg_op) {
        return 0;
    }
    auto* fcn = dynamic_cast<const FunctionCallNode*>(bg_op->getExp().getInternalNode());
    if (!fcn) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "background expression slot %d is not a function call", slot);
        return 0;
    }
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    FunctionCallNode* call_node = new FunctionCallNode(*fcn, arg_list);
    QoreValue result = doBackgroundWithLocation(bg_op->loc, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_background_static_method_call_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    auto* bg_op = getAOTBackgroundSlotOp(ctx, slot, "qore_rt_background_static_method_call_aot", xsink);
    if (!bg_op) {
        return 0;
    }
    auto* smcn = dynamic_cast<const StaticMethodCallNode*>(bg_op->getExp().getInternalNode());
    if (!smcn) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "background expression slot %d is not a static method call", slot);
        return 0;
    }
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    StaticMethodCallNode* call_node = new StaticMethodCallNode(*smcn, arg_list);
    QoreValue result = doBackgroundWithLocation(bg_op->loc, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_background_dot_eval_call_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t recv_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    auto* bg_op = getAOTBackgroundSlotOp(ctx, slot, "qore_rt_background_dot_eval_call_aot", xsink);
    if (!bg_op) {
        return 0;
    }
    auto* devn = dynamic_cast<const QoreDotEvalOperatorNode*>(bg_op->getExp().getInternalNode());
    if (!devn) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "background expression slot %d is not a dot-eval method call", slot);
        return 0;
    }
    MethodCallNode* source_m = devn->getMethodCall();
    if (!source_m) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "background expression slot %d has no dot-eval method call", slot);
        return 0;
    }
    QoreValue recv = fromBits(recv_bits);
    if (recv.hasNode()) {
        recv.refSelf();
    }
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    MethodCallNode* new_m = new MethodCallNode(*source_m, arg_list);
    QoreDotEvalOperatorNode* call_node =
        new QoreDotEvalOperatorNode(devn->loc, recv, new_m);
    QoreValue result = doBackgroundWithLocation(bg_op->loc, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_background_dot_eval_name_call_aot(
        const char* method_name, uint64_t recv_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(method_name);
    QoreValue recv = fromBits(recv_bits);
    if (recv.hasNode()) {
        recv.refSelf();
    }
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    MethodCallNode* new_m = new MethodCallNode(&loc_builtin, strdup(method_name), arg_list);
    QoreDotEvalOperatorNode* call_node =
        new QoreDotEvalOperatorNode(&loc_builtin, recv, new_m);
    QoreValue result = doBackgroundWithLocation(&loc_builtin, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_background_static_method_name_call_aot(
        const char* qualified_name, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(qualified_name);
    NamedScope scope(qualified_name);
    if (scope.size() < 2) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "invalid static background call target '%s'", qualified_name);
        return 0;
    }

    const QoreClass* qc = qore_root_ns_private::get(*(getRootNS()))->runtimeFindScopedClassWithMethod(scope);
    if (!qc) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "cannot resolve class for static background call '%s'", qualified_name);
        return 0;
    }

    ClassAccess access;
    const QoreMethod* method = qc->findStaticMethod(scope.getIdentifier(), access);
    if (!method) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "cannot resolve static method for background call '%s'", qualified_name);
        return 0;
    }

    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    StaticMethodCallNode source(&loc_builtin, method, nullptr);
    StaticMethodCallNode* call_node = new StaticMethodCallNode(source, arg_list);
    QoreValue result = doBackgroundWithLocation(&loc_builtin, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_background_call_ref_value_aot(
        uint64_t callee_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    QoreValue callee = fromBits(callee_bits);
    if (callee.hasNode()) {
        callee.refSelf();
    }
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    CallReferenceCallNode* call_node =
        new CallReferenceCallNode(&loc_builtin, callee, arg_list);
    QoreValue result = doBackgroundWithLocation(&loc_builtin, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_background_call_ref_call_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t callee_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    auto* bg_op = getAOTBackgroundSlotOp(ctx, slot, "qore_rt_background_call_ref_call_aot", xsink);
    if (!bg_op) {
        return 0;
    }
    auto* crcn = dynamic_cast<const CallReferenceCallNode*>(bg_op->getExp().getInternalNode());
    if (!crcn) {
        xsink->raiseException("AOT-BACKGROUND-ERROR",
            "background expression slot %d is not a call-reference call", slot);
        return 0;
    }
    QoreValue callee = fromBits(callee_bits);
    if (callee.hasNode()) {
        callee.refSelf();
    }
    QoreListNode* arg_list = bgBuildArgList(args, nargs);
    CallReferenceCallNode* call_node =
        new CallReferenceCallNode(crcn->loc, callee, arg_list);
    QoreValue result = doBackgroundWithLocation(bg_op->loc, QoreValue(call_node), xsink);
    QoreValue(call_node).discard(xsink);
    return toBits(result);
}

// --- Phase 2B Step 5: specialized helpers throwing wrappers ---
// (placed at end of file - all base helpers are defined above)

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_closure_throwing(
        const QoreClosureParseNode* cn, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_closure(cn, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_closure_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_closure_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_call_ref_throwing(
        uint64_t expr_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_call_ref(expr_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_method_ref_throwing(
        uint64_t expr_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_method_ref(expr_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_static_method_call_ref_aot_throwing(
        const char* class_path, const char* method_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_static_method_call_ref_aot(class_path, method_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_local_method_call_ref_aot_throwing(
        const char* class_path, const char* method_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_local_method_call_ref_aot(class_path, method_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_function_call_ref_aot_throwing(
        const char* function_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_function_call_ref_aot(function_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_self_method_ref_aot_throwing(
        const char* method_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_self_method_ref_aot(method_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_parse_ref_throwing(
        const ParseReferenceNode* node, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_parse_ref(node, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_parse_ref_resolved_hash_key_throwing(
        const ParseReferenceNode* node, uint64_t key_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_parse_ref_resolved_hash_key(node, key_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_object_method_ref_aot_throwing(
        uint64_t object_bits, const char* method_name, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_object_method_ref_aot(object_bits, method_name, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_parse_ref_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_parse_ref_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_create_parse_ref_aot_resolved_hash_key_throwing(
        QoreAOTContext* ctx, int32_t idx, uint64_t key_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_create_parse_ref_aot_resolved_hash_key(ctx, idx, key_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_hash_decl_throwing(
        const NewHashDeclNode* node, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_hash_decl(node, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_hash_decl_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_hash_decl_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_hash_throwing(
        const NewComplexHashNode* node, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_hash(node, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_hash_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_hash_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_list_throwing(
        const NewComplexListNode* node, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_list(node, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_list_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_list_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_buffer_throwing(
        const NewComplexBufferNode* node, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_buffer(node, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_buffer_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_buffer_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_hash_from_hash_throwing(
        const QoreTypeInfo* typeInfo, uint64_t hash_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_hash_from_hash(typeInfo, hash_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_hash_from_hash_by_type_path_throwing(
        const char* type_path, uint64_t hash_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_hash_from_hash_by_type_path(type_path, hash_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_list_from_value_throwing(
        const QoreTypeInfo* typeInfo, uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_list_from_value(typeInfo, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_list_from_value_by_type_path_throwing(
        const char* type_path, uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_list_from_value_by_type_path(type_path, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_buffer_from_value_throwing(
        const QoreTypeInfo* typeInfo, uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_buffer_from_value(typeInfo, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_buffer_from_value_by_type_path_throwing(
        const char* type_path, uint64_t value_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_buffer_from_value_by_type_path(type_path, value_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_complex_buffer_from_value_kind_throwing(
        const QoreTypeInfo* typeInfo, uint64_t value_bits, int32_t init_kind, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_buffer_from_value_kind(typeInfo, value_bits, init_kind, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t
qore_rt_new_complex_buffer_from_value_kind_by_type_path_throwing(const char* type_path, uint64_t value_bits,
        int32_t init_kind, ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_complex_buffer_from_value_kind_by_type_path(type_path, value_bits, init_kind,
        xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_vrn_construct_throwing(
        const VarRefNewObjectNode* vrn, ExceptionSink* xsink) {
    uint64_t result = qore_rt_vrn_construct(vrn, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_vrn_construct_aot_throwing(
        QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    uint64_t result = qore_rt_vrn_construct_aot(ctx, idx, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_hash_decl_from_hash_throwing(
        const TypedHashDecl* hd, uint64_t hash_bits, int32_t runtime_check,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_hash_decl_from_hash(hd, hash_bits, runtime_check, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_hash_decl_from_hash_by_path_throwing(
        const char* hd_path, uint64_t hash_bits, int32_t runtime_check,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_hash_decl_from_hash_by_path(hd_path, hash_bits,
            runtime_check, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_new_hash_decl_from_hash_by_path_cached_throwing(
        QoreAOTContext* ctx, const char* hd_path, uint64_t hash_bits, int32_t runtime_check,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_new_hash_decl_from_hash_by_path_cached(ctx, hd_path, hash_bits,
            runtime_check, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_exec_statement_throwing(
        int opcode, const AbstractStatement* stmt, ExceptionSink* xsink) {
    uint64_t result = qore_rt_exec_statement(opcode, stmt, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_instanceof_by_type_path_throwing(
        uint64_t val_bits, const char* type_path, ExceptionSink* xsink) {
    uint64_t result = qore_rt_instanceof_by_type_path(val_bits, type_path, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_self_call_throwing(
        const SelfFunctionCallNode* sfcn, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_self_call(sfcn, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_self_call_aot_throwing(
        const char* method_name, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_self_call_aot(method_name, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_function_call_throwing(
        const FunctionCallNode* fcn, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_function_call(fcn, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_static_method_call_throwing(
        const StaticMethodCallNode* smcn, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_static_method_call(smcn, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_dot_eval_call_throwing(
        const QoreDotEvalOperatorNode* devn, uint64_t recv_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_dot_eval_call(devn, recv_bits, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_call_ref_call_throwing(
        const CallReferenceCallNode* crcn, uint64_t callee_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_call_ref_call(crcn, callee_bits, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_function_call_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_function_call_aot(ctx, slot, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_static_method_call_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_static_method_call_aot(ctx, slot, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_dot_eval_call_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t recv_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_dot_eval_call_aot(ctx, slot, recv_bits,
        args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_dot_eval_name_call_aot_throwing(
        const char* method_name, uint64_t recv_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_dot_eval_name_call_aot(method_name, recv_bits,
        args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_static_method_name_call_aot_throwing(
        const char* qualified_name, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_static_method_name_call_aot(qualified_name, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_call_ref_value_aot_throwing(
        uint64_t callee_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_call_ref_value_aot(callee_bits, args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_background_call_ref_call_aot_throwing(
        QoreAOTContext* ctx, int32_t slot, uint64_t callee_bits,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    uint64_t result = qore_rt_background_call_ref_call_aot(ctx, slot, callee_bits,
        args, nargs, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

// --- Phase 2B Step 5: Regex/switch throwing wrappers ---

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_switch_regex_match_throwing(
        uint64_t regex_case_ptr, uint64_t switch_val_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_switch_regex_match(regex_case_ptr, switch_val_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_regex_op_with_operand_throwing(
        int32_t opcode, uint64_t expr_bits, uint64_t operand_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_regex_op_with_operand(opcode, expr_bits, operand_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_regex_op_with_operand_aot_throwing(
        QoreAOTContext* ctx, int32_t opcode, int32_t slot, uint64_t operand_bits,
        ExceptionSink* xsink) {
    uint64_t result = qore_rt_regex_op_with_operand_aot(ctx, opcode, slot, operand_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_regex_op_by_pattern_throwing(
        int32_t opcode, const char* pattern, int64_t options, int32_t global,
        uint64_t operand_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_regex_op_by_pattern(opcode, pattern, options, global,
            operand_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_switch_case_match_throwing(
        const void* case_node_ptr, uint64_t switch_val_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_switch_case_match(case_node_ptr, switch_val_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_switch_case_match_value_throwing(
        uint64_t case_val_bits, uint64_t switch_val_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_switch_case_match_value(case_val_bits, switch_val_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_switch_case_match_value_aot_throwing(
        QoreAOTContext* ctx, int32_t case_slot, uint64_t switch_val_bits, ExceptionSink* xsink) {
    uint64_t result = qore_rt_switch_case_match_value_aot(ctx, case_slot, switch_val_bits, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

// --- Phase 2B Step 5: Fast-path slow-route throwing wrappers ---
// These are called on the slow path inside emitAny*FastPath helpers when
// the fast int/float checks fail; they can raise exceptions via the
// runtime IR interpreter.

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_binary_op_throwing(
        int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_binary_op(opcode, left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_unary_op_throwing(
        int opcode, uint64_t operand, ExceptionSink* xsink) {
    uint64_t result = qore_rt_unary_op(opcode, operand, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_comparison_op_throwing(
        int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_comparison_op(opcode, left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_ternary_op_throwing(
        int opcode, uint64_t a, uint64_t b, uint64_t c, ExceptionSink* xsink) {
    uint64_t result = qore_rt_ternary_op(opcode, a, b, c, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

// add_any/sub_any/mul_any/div_any/mod_any: used by emitAnyArithFastPath.
// Compound assignment has separate helpers because its dynamic semantics can
// differ from plain binary operations (for example timeout +=/-= date).
extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_add_any_throwing(
        uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_add_any(left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_sub_any_throwing(
        uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_sub_any(left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_mul_any_throwing(
        uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_mul_any(left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_add_assign_any_throwing(
        uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_add_assign_any(left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_sub_assign_any_throwing(
        uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_sub_assign_any(left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_mul_assign_any_throwing(
        uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_mul_assign_any(left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_div_any_throwing(
        uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_div_any(left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

extern "C" DLLEXPORT __attribute__((noinline)) uint64_t qore_rt_mod_any_throwing(
        uint64_t left, uint64_t right, ExceptionSink* xsink) {
    uint64_t result = qore_rt_mod_any(left, right, xsink);
    if (xsink && *xsink) {
        throw QoreJITException();
    }
    return result;
}

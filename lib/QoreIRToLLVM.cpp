/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRToLLVM.cpp

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

#include "qore/intern/QoreIRToLLVM.h"

#include "qore/intern/LocalVar.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreIR.h"
// Compile-time guard: forces review of LLVM lowering when opcodes change.
// Update this value after verifying the new opcode is handled (or deliberately
// falls through to the default case).
static_assert(QORE_IR_MAX_OPCODE == 343,
    "New IR opcode added — review QoreIRToLLVM.cpp dispatch switch "
    "and update this assertion.  Also check QoreIRInterpreter.cpp.");
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/CaseNodeRegex.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/SelfVarrefNode.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/ParseReferenceNode.h"
#include "qore/intern/QoreOperatorNode.h"
#include "qore/intern/QorePseudoMethods.h"
#include "qore/intern/QoreCastOperatorNode.h"
#include <qore/QoreStringNode.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstring>

// NaN-boxing constants matching QoreValue.h
static constexpr uint64_t TAG_INT48          = 0xFFF9000000000000ULL;
static constexpr uint64_t PAYLOAD_MASK       = 0x0000FFFFFFFFFFFFULL;
static constexpr uint64_t TAG_MASK           = 0xFFFF000000000000ULL;  // Phase 4: NaN-boxing tag extraction
static constexpr uint64_t TAG_POINTER        = 0xFFFA000000000000ULL;  // Phase 4: Tag for pointer type
static constexpr uint64_t DOUBLE_ENCODE_OFFSET = 0x0001000000000000ULL;
static constexpr uint64_t VAL_NOTHING        = 0;
static constexpr uint64_t VAL_NULL           = 0xFFFB000000000001ULL;
static constexpr uint64_t VAL_FALSE          = 0xFFFB000000000002ULL;
static constexpr uint64_t VAL_TRUE           = 0xFFFB000000000003ULL;
// 48-bit signed integer range
static constexpr int64_t INT48_MIN           = -(1LL << 47);
static constexpr int64_t INT48_MAX           = (1LL << 47) - 1;

void QoreIRToLLVM::initTypes() {
    i64_type = llvm::Type::getInt64Ty(ctx);
    i32_type = llvm::Type::getInt32Ty(ctx);
    i1_type = llvm::Type::getInt1Ty(ctx);
    double_type = llvm::Type::getDoubleTy(ctx);
    ptr_type = llvm::PointerType::getUnqual(ctx);
    void_type = llvm::Type::getVoidTy(ctx);
}

void QoreIRToLLVM::declareRuntimeHelpers(llvm::Module& module) {
    // Arithmetic helpers: (i64, i64, ptr) -> i64
    auto* arith_ft = llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_add_any", arith_ft);
    module.getOrInsertFunction("qore_rt_sub_any", arith_ft);
    module.getOrInsertFunction("qore_rt_mul_any", arith_ft);
    module.getOrInsertFunction("qore_rt_div_any", arith_ft);
    module.getOrInsertFunction("qore_rt_mod_any", arith_ft);

    // Integer division: (i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_div_int", arith_ft);
    module.getOrInsertFunction("qore_rt_mod_int", arith_ft);

    // Float division: (double, double, ptr) -> double
    auto* fdiv_ft = llvm::FunctionType::get(double_type, {double_type, double_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_div_float", fdiv_ft);

    // Conversion helpers
    module.getOrInsertFunction("qore_rt_to_int", llvm::FunctionType::get(i64_type, {i64_type}, false));
    module.getOrInsertFunction("qore_rt_to_float", llvm::FunctionType::get(double_type, {i64_type}, false));
    module.getOrInsertFunction("qore_rt_to_bool", llvm::FunctionType::get(i64_type, {i64_type}, false));

    // Refcount helpers
    module.getOrInsertFunction("qore_rt_incref", llvm::FunctionType::get(void_type, {i64_type}, false));
    module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_decref_nothrow",
            llvm::FunctionType::get(void_type, {i64_type}, false));

    // Exception helpers
    module.getOrInsertFunction("qore_rt_throw",
            llvm::FunctionType::get(void_type, {ptr_type, ptr_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_throw_value",
            llvm::FunctionType::get(void_type, {ptr_type, i64_type}, false));
    module.getOrInsertFunction("qore_rt_has_exception",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));

    // Guard helpers
    module.getOrInsertFunction("qore_rt_guard_not_nothing",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    module.getOrInsertFunction("qore_rt_guard_int",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    module.getOrInsertFunction("qore_rt_guard_float",
            llvm::FunctionType::get(i64_type, {i64_type}, false));

    // Boxing helpers
    module.getOrInsertFunction("qore_rt_box_big_int",
            llvm::FunctionType::get(i64_type, {i64_type}, false));

    // Invoke helpers
    module.getOrInsertFunction("qore_rt_invoke_expr",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_make_string",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));
    module.getOrInsertFunction("qore_rt_catch_exception",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));
    module.getOrInsertFunction("qore_rt_catch_end",
            llvm::FunctionType::get(void_type, {ptr_type}, false));
    module.getOrInsertFunction("qore_rt_rethrow",
            llvm::FunctionType::get(void_type, {ptr_type}, false));

    // Deopt helper: void qore_rt_deopt(void* deopt_counter_ptr)
    module.getOrInsertFunction("qore_rt_deopt",
            llvm::FunctionType::get(void_type, {ptr_type}, false));

    // Local variable helpers
    module.getOrInsertFunction("qore_rt_instantiate_local",
            llvm::FunctionType::get(void_type, {ptr_type}, false));
    module.getOrInsertFunction("qore_rt_assign_local",
            llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_assign_local_no_coerce",
            llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_coerce_value",
            llvm::FunctionType::get(i64_type, {ptr_type, i64_type, ptr_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_load_local",
            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_uninstantiate_local",
            llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false));

    // Generic opcode dispatch helpers: (i32, i64, i64, ptr) -> i64
    auto* binary_op_ft = llvm::FunctionType::get(i64_type,
            {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_binary_op", binary_op_ft);
    module.getOrInsertFunction("qore_rt_comparison_op", binary_op_ft);

    // (i32, i64, ptr) -> i64
    auto* unary_op_ft = llvm::FunctionType::get(i64_type,
            {llvm::Type::getInt32Ty(ctx), i64_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_unary_op", unary_op_ft);

    // (i32, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_expr_op", unary_op_ft);

    // (i32, i64, i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_ternary_op",
            llvm::FunctionType::get(i64_type,
                {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, i64_type, ptr_type}, false));

    // Variable access helpers
    // load_global/load_thread_local: (ptr, ptr) -> i64
    auto* load_var_ft = llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_load_global", load_var_ft);
    module.getOrInsertFunction("qore_rt_load_thread_local", load_var_ft);
    module.getOrInsertFunction("qore_rt_load_closure", load_var_ft);

    // store_global/store_thread_local: (ptr, i64, ptr) -> void
    auto* store_var_ft = llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_store_global", store_var_ft);
    module.getOrInsertFunction("qore_rt_store_thread_local", store_var_ft);
    module.getOrInsertFunction("qore_rt_store_closure", store_var_ft);

    // LValue operation helpers
    // lvalue_load: (i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_load",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
    // lvalue_store: (i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_store",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
    // lvalue_unary: (i32, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_unary", unary_op_ft);
    // lvalue_binary: (i32, i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_binary", binary_op_ft);
    // lvalue_ternary: (i32, i64, i64, i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_ternary",
            llvm::FunctionType::get(i64_type, {i32_type, i64_type, i64_type, i64_type, i64_type, ptr_type}, false));

    // Container construction helpers
    // make_list: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_make_list",
            llvm::FunctionType::get(i64_type,
                {ptr_type, llvm::Type::getInt32Ty(ctx), ptr_type}, false));
    // make_hash: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_make_hash",
            llvm::FunctionType::get(i64_type,
                {ptr_type, llvm::Type::getInt32Ty(ctx), ptr_type}, false));

    // Statement execution helpers
    // exec_statement: (i32, ptr, ptr) -> i64
    module.getOrInsertFunction("qore_rt_exec_statement",
            llvm::FunctionType::get(i64_type,
                {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false));
    // thread_exit: (ptr) -> void
    module.getOrInsertFunction("qore_rt_thread_exit",
            llvm::FunctionType::get(void_type, {ptr_type}, false));

    // On-block-exit handler helpers
    // push_on_block_exit: (i32, ptr) -> void
    module.getOrInsertFunction("qore_rt_push_on_block_exit",
            llvm::FunctionType::get(void_type, {llvm::Type::getInt32Ty(ctx), ptr_type}, false));
    // get_on_block_exit_count: () -> i64
    module.getOrInsertFunction("qore_rt_get_on_block_exit_count",
            llvm::FunctionType::get(i64_type, {}, false));
    // exec_on_block_exit: (i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_exec_on_block_exit",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

    // Guard type helper: (i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_guard_type",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));

    // Date construction helper: (i64, i64) -> i64
    module.getOrInsertFunction("qore_rt_make_date",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));

    // Specialized access helpers (Phase 5b)
    // hash_key_access: (i64, ptr, ptr) -> i64
    module.getOrInsertFunction("qore_rt_hash_key_access",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
    // list_index_access: (i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_list_index_access",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
    // string_concat: (i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_string_concat",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));

    // Optimized list iteration helpers (higher-order optimization)
    // list_size: (i64) -> i64
    module.getOrInsertFunction("qore_rt_list_size",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    // list_get_int: (i64, i64) -> i64
    module.getOrInsertFunction("qore_rt_list_get_int",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
    // list_get_float: (i64, i64) -> double
    module.getOrInsertFunction("qore_rt_list_get_float",
            llvm::FunctionType::get(double_type, {i64_type, i64_type}, false));
    // list_get_value: (i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_list_get_value",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
    // create_sized_list: (i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_create_sized_list",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
    // list_set_int: (i64, i64, i64) -> void
    module.getOrInsertFunction("qore_rt_list_set_int",
            llvm::FunctionType::get(void_type, {i64_type, i64_type, i64_type}, false));
    // list_set_float: (i64, i64, double) -> void
    module.getOrInsertFunction("qore_rt_list_set_float",
            llvm::FunctionType::get(void_type, {i64_type, i64_type, double_type}, false));
    // list_set_value: (i64, i64, i64) -> void
    module.getOrInsertFunction("qore_rt_list_set_value",
            llvm::FunctionType::get(void_type, {i64_type, i64_type, i64_type}, false));
    // get_object_class: (i64) -> i64
    module.getOrInsertFunction("qore_rt_get_object_class",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    // call_closure_fast: (i64, ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_call_closure_fast",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type, i32_type, ptr_type}, false));

    // AOT context-based helpers (Phase 7b)
    // load_local_aot: (ptr, i32, ptr) -> i64
    auto* aot_load_local_ft = llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_load_local_aot", aot_load_local_ft);
    // assign_local_aot: (ptr, i32, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_assign_local_aot",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
    // assign_local_no_coerce_aot: (ptr, i32, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_assign_local_no_coerce_aot",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
    // instantiate_local_aot: (ptr, i32) -> void
    module.getOrInsertFunction("qore_rt_instantiate_local_aot",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false));
    // uninstantiate_local_aot: (ptr, i32, ptr) -> void
    module.getOrInsertFunction("qore_rt_uninstantiate_local_aot",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
    // load_global_aot: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_load_global_aot", aot_load_local_ft);
    // store_global_aot: (ptr, i32, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_store_global_aot",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
    // load_thread_local_aot: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_load_thread_local_aot", aot_load_local_ft);
    // store_thread_local_aot: (ptr, i32, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_store_thread_local_aot",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
    // load_closure_aot: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_load_closure_aot", aot_load_local_ft);
    // store_closure_aot: (ptr, i32, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_store_closure_aot",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
    // invoke_expr_aot: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_invoke_expr_aot", aot_load_local_ft);
    // lvalue_load_aot: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_load_aot", aot_load_local_ft);
    // lvalue_store_aot: (ptr, i32, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_store_aot",
            llvm::FunctionType::get(i64_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
    // lvalue_unary_aot: (i32, ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_unary_aot",
            llvm::FunctionType::get(i64_type, {i32_type, ptr_type, i32_type, ptr_type}, false));
    // lvalue_binary_aot: (i32, ptr, i32, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_binary_aot",
            llvm::FunctionType::get(i64_type, {i32_type, ptr_type, i32_type, i64_type, ptr_type}, false));
    // lvalue_ternary_aot: (i32, ptr, i32, i64, i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_lvalue_ternary_aot",
            llvm::FunctionType::get(i64_type,
                {i32_type, ptr_type, i32_type, i64_type, i64_type, i64_type, ptr_type}, false));

    // Call with pre-evaluated args: (i64, ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_call_with_args",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type, i32_type, ptr_type}, false));
    // AOT variant: (ptr, i32, ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_call_with_args_aot",
            llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));

    // Iterator helpers for foreach loops
    // iterator_create: (i64, ptr, ptr) -> ptr
    module.getOrInsertFunction("qore_rt_iterator_create",
            llvm::FunctionType::get(ptr_type, {i64_type, ptr_type, ptr_type}, false));
    // iterator_next: (ptr, ptr, ptr) -> i64
    module.getOrInsertFunction("qore_rt_iterator_next",
            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type, ptr_type}, false));
    // iterator_cleanup: (ptr) -> void — deletes non-null iterator on abnormal exit
    module.getOrInsertFunction("qore_rt_iterator_cleanup",
            llvm::FunctionType::get(void_type, {ptr_type}, false));

    // Reference foreach helpers
    // ref_foreach_init: (i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_ref_foreach_init",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
    // ref_foreach_size: (i64) -> i64
    module.getOrInsertFunction("qore_rt_ref_foreach_size",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    // ref_foreach_get_entry: (i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_ref_foreach_get_entry",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
    // ref_foreach_record: (i64, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_ref_foreach_record",
            llvm::FunctionType::get(void_type, {i64_type, i64_type, ptr_type}, false));
    // ref_foreach_finalize: (i64, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_ref_foreach_finalize",
            llvm::FunctionType::get(void_type, {i64_type, i64_type, ptr_type}, false));
    // ref_foreach_cleanup: (i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_ref_foreach_cleanup",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
}

llvm::FunctionCallee QoreIRToLLVM::getHelper(llvm::Module& module, const char* name, llvm::FunctionType* ft) {
    return module.getOrInsertFunction(name, ft);
}

llvm::Value* QoreIRToLLVM::getVal(uint32_t id, std::string& error) {
    auto it = values.find(id);
    if (it == values.end()) {
        error = "missing LLVM value for IR value %" + std::to_string(id);
        // Debug: show available values when lookup fails
        if (getenv("QORE_LLVM_DEBUG")) {
            fprintf(stderr, "LLVM-DEBUG: getVal(%%%u) failed; available values:", id);
            for (auto& kv : values) {
                fprintf(stderr, " %%%u", kv.first);
            }
            fprintf(stderr, "\n");
        }
        return nullptr;
    }
    return it->second;
}

// NaN-boxing: encode a native int64_t into the QoreValue i64 representation.
// For inline ints (48-bit range): bits = TAG_INT48 | (val & PAYLOAD_MASK)
// For out-of-range: calls qore_rt_box_big_int() to allocate a QoreBigIntNode
llvm::Value* QoreIRToLLVM::boxInt(llvm::Value* int_val) {
    // For compile-time constants, check range statically to use fast inline encoding
    if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(int_val)) {
        int64_t val = ci->getSExtValue();
        if (val >= INT48_MIN && val <= INT48_MAX) {
            llvm::Value* masked = builder->CreateAnd(int_val,
                llvm::ConstantInt::get(i64_type, PAYLOAD_MASK));
            return builder->CreateOr(masked, llvm::ConstantInt::get(i64_type, TAG_INT48));
        }
    }
    // For runtime values or out-of-range constants, call runtime helper.
    // qore_rt_box_big_int handles both inline INT48 encoding and QoreBigIntNode allocation.
    // We avoid creating LLVM branch blocks here to prevent block structure interference
    // during PHI fixup and other multi-block lowering contexts.
    auto helper = current_module->getOrInsertFunction("qore_rt_box_big_int",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    return builder->CreateCall(helper, {int_val});
}

// NaN-boxing: encode a native double into the QoreValue i64 representation.
// bits = bitcast_to_i64(double) + DOUBLE_ENCODE_OFFSET
llvm::Value* QoreIRToLLVM::boxFloat(llvm::Value* float_val) {
    llvm::Value* raw_bits = builder->CreateBitCast(float_val, i64_type);
    return builder->CreateAdd(raw_bits, llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET));
}

// NaN-boxing: encode a native bool (i1) into QoreValue
llvm::Value* QoreIRToLLVM::boxBool(llvm::Value* bool_val) {
    return builder->CreateSelect(bool_val,
            llvm::ConstantInt::get(i64_type, VAL_TRUE),
            llvm::ConstantInt::get(i64_type, VAL_FALSE));
}

llvm::Value* QoreIRToLLVM::boxNothing() {
    return llvm::ConstantInt::get(i64_type, VAL_NOTHING);
}

// Unbox: extract native int64_t from NaN-boxed QoreValue
// Sign-extend from 48 bits: val = payload; if (val & (1<<47)) val |= 0xFFFF000000000000
llvm::Value* QoreIRToLLVM::unboxInt(llvm::Value* qv) {
    llvm::Value* payload = builder->CreateAnd(qv, llvm::ConstantInt::get(i64_type, PAYLOAD_MASK));
    // Shift left 16, then arithmetic shift right 16 to sign-extend from bit 47
    llvm::Value* shifted = builder->CreateShl(payload, llvm::ConstantInt::get(i64_type, 16));
    return builder->CreateAShr(shifted, llvm::ConstantInt::get(i64_type, 16));
}

// Unbox: extract native double from NaN-boxed QoreValue
// rawBits = qv - DOUBLE_ENCODE_OFFSET; double = bitcast(rawBits)
llvm::Value* QoreIRToLLVM::unboxFloat(llvm::Value* qv) {
    llvm::Value* raw_bits = builder->CreateSub(qv, llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET));
    return builder->CreateBitCast(raw_bits, double_type);
}

// Unbox: extract native bool from NaN-boxed QoreValue
llvm::Value* QoreIRToLLVM::unboxBool(llvm::Value* qv) {
    return builder->CreateICmpEQ(qv, llvm::ConstantInt::get(i64_type, VAL_TRUE));
}

// Phase 4: Check if value has a node (inline NaN-boxing check)
// Returns true if the value is a pointer (tag == TAG_POINTER) with non-null address
// Uses bit operations to extract tag and check for pointer type
llvm::Value* QoreIRToLLVM::hasNodeInline(llvm::Value* qv) {
    // Extract tag: tag = qv & 0xFFFF000000000000
    llvm::Value* tag = builder->CreateAnd(qv, llvm::ConstantInt::get(i64_type, TAG_MASK));
    // Check if tag == TAG_POINTER (0xFFFA000000000000)
    llvm::Value* is_pointer = builder->CreateICmpEQ(tag, llvm::ConstantInt::get(i64_type, TAG_POINTER));

    // Extract pointer: ptr = qv & 0x0000FFFFFFFFFFFF
    llvm::Value* payload = builder->CreateAnd(qv, llvm::ConstantInt::get(i64_type, PAYLOAD_MASK));
    // Check if pointer is non-null
    llvm::Value* ptr_not_null = builder->CreateICmpNE(payload, llvm::ConstantInt::get(i64_type, 0));

    // Return (is_pointer && ptr_not_null)
    return builder->CreateAnd(is_pointer, ptr_not_null);
}

// Phase 4: Inline qore_rt_ref - reference count a value if it's a node
// Emits: has_node ? qore_rt_refself(val) : val
// This avoids external function call overhead for the type check
// Uses LLVM select to avoid extra basic blocks - inline conditional evaluation
llvm::Value* QoreIRToLLVM::emitHelperRef(llvm::Module& module, llvm::Value* val) {
    // Check if value has a node (inline check using NaN-boxing)
    llvm::Value* has_node = hasNodeInline(val);

    // Get refself function
    auto refself_fn = module.getOrInsertFunction("qore_rt_refself",
            llvm::FunctionType::get(i64_type, {i64_type}, false));

    // Call refself (will only be used if has_node is true)
    llvm::Value* ref_result = builder->CreateCall(refself_fn, {val});

    // Use select: if has_node then ref_result else val
    // This is more efficient than branching for simple cases
    return builder->CreateSelect(has_node, ref_result, val);
}

// Ensure a value is a native int64_t for typed int operations.
// Handles: native i64 → pass through, NaN-boxed (INT48 or big int) → runtime conversion.
llvm::Value* QoreIRToLLVM::ensureIntType(llvm::Value* val, uint32_t value_id) {
    if (!nanboxed_values.count(value_id)) {
        return val;  // Already native i64
    }
    // NaN-boxed value: call runtime to extract int (handles both INT48 and QoreBigIntNode)
    auto to_int = current_module->getOrInsertFunction("qore_rt_to_int",
        llvm::FunctionType::get(i64_type, {i64_type}, false));
    return builder->CreateCall(to_int, {val});
}

// Inline fast-path for ensureIntType: check INT48 tag inline, call runtime only for big ints.
// Safe to use in typed int ops (AddInt, SubInt, etc.) where branch creation doesn't interfere
// with PHI fixup or block ordering.
llvm::Value* QoreIRToLLVM::ensureIntTypeInline(llvm::Value* val, uint32_t value_id) {
    if (val->getType()->isPointerTy()) {
        return builder->CreatePtrToInt(val, i64_type);
    }
    if (!nanboxed_values.count(value_id)) {
        return val;  // Already native i64
    }
    // Inline tag check + sign-extend for INT48, runtime call for QoreBigIntNode
    auto* cur_func = builder->GetInsertBlock()->getParent();
    auto* fast_bb = llvm::BasicBlock::Create(ctx, "int48_fast", cur_func);
    auto* slow_bb = llvm::BasicBlock::Create(ctx, "int_slow", cur_func);
    auto* merge_bb = llvm::BasicBlock::Create(ctx, "int_merge", cur_func);

    // Check if top 16 bits match TAG_INT48 (0xFFF9)
    llvm::Value* tag = builder->CreateLShr(val, llvm::ConstantInt::get(i64_type, 48));
    llvm::Value* is_int48 = builder->CreateICmpEQ(tag, llvm::ConstantInt::get(i64_type, 0xFFF9));
    builder->CreateCondBr(is_int48, fast_bb, slow_bb);

    // Fast path: inline sign-extend from 48 bits
    builder->SetInsertPoint(fast_bb);
    llvm::Value* payload = builder->CreateAnd(val, llvm::ConstantInt::get(i64_type, PAYLOAD_MASK));
    llvm::Value* shifted = builder->CreateShl(payload, llvm::ConstantInt::get(i64_type, 16));
    llvm::Value* fast_result = builder->CreateAShr(shifted, llvm::ConstantInt::get(i64_type, 16));
    builder->CreateBr(merge_bb);

    // Slow path: call runtime (handles QoreBigIntNode)
    builder->SetInsertPoint(slow_bb);
    auto to_int = current_module->getOrInsertFunction("qore_rt_to_int",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    llvm::Value* slow_result = builder->CreateCall(to_int, {val});
    builder->CreateBr(merge_bb);

    // Merge
    builder->SetInsertPoint(merge_bb);
    auto* phi = builder->CreatePHI(i64_type, 2);
    phi->addIncoming(fast_result, fast_bb);
    phi->addIncoming(slow_result, slow_bb);
    return phi;
}

// Inline fast-path for boxInt: check INT48 range inline, call runtime only for big ints.
// Safe to use in StoreLocal and other contexts where branch creation doesn't interfere
// with PHI fixup or block ordering.
llvm::Value* QoreIRToLLVM::boxIntInline(llvm::Value* int_val) {
    // For compile-time constants, check range statically (same as boxInt)
    if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(int_val)) {
        int64_t val = ci->getSExtValue();
        if (val >= INT48_MIN && val <= INT48_MAX) {
            llvm::Value* masked = builder->CreateAnd(int_val,
                    llvm::ConstantInt::get(i64_type, PAYLOAD_MASK));
            return builder->CreateOr(masked, llvm::ConstantInt::get(i64_type, TAG_INT48));
        }
    }
    // Runtime range check with inline INT48 encoding
    auto* cur_func = builder->GetInsertBlock()->getParent();
    auto* inline_bb = llvm::BasicBlock::Create(ctx, "box_inline", cur_func);
    auto* big_bb = llvm::BasicBlock::Create(ctx, "box_big", cur_func);
    auto* merge_bb = llvm::BasicBlock::Create(ctx, "box_merge", cur_func);

    llvm::Value* ge_min = builder->CreateICmpSGE(int_val,
            llvm::ConstantInt::get(i64_type, INT48_MIN));
    llvm::Value* le_max = builder->CreateICmpSLE(int_val,
            llvm::ConstantInt::get(i64_type, INT48_MAX));
    llvm::Value* in_range = builder->CreateAnd(ge_min, le_max);
    builder->CreateCondBr(in_range, inline_bb, big_bb);

    // Inline path: TAG_INT48 | (val & PAYLOAD_MASK)
    builder->SetInsertPoint(inline_bb);
    llvm::Value* masked = builder->CreateAnd(int_val,
            llvm::ConstantInt::get(i64_type, PAYLOAD_MASK));
    llvm::Value* inline_result = builder->CreateOr(masked,
            llvm::ConstantInt::get(i64_type, TAG_INT48));
    builder->CreateBr(merge_bb);

    // Big path: call runtime
    builder->SetInsertPoint(big_bb);
    auto helper = current_module->getOrInsertFunction("qore_rt_box_big_int",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    llvm::Value* big_result = builder->CreateCall(helper, {int_val});
    builder->CreateBr(merge_bb);

    // Merge
    builder->SetInsertPoint(merge_bb);
    auto* phi = builder->CreatePHI(i64_type, 2);
    phi->addIncoming(inline_result, inline_bb);
    phi->addIncoming(big_result, big_bb);
    return phi;
}

// Ensure a value is a native double for float operations.
// Handles: native double → pass through, NaN-boxed (int or float) → runtime conversion,
// native i64 → SIToFP
llvm::Value* QoreIRToLLVM::ensureFloatType(llvm::Value* val, uint32_t value_id, llvm::Module& module) {
    if (val->getType() == double_type) {
        // Already a native double
        return val;
    }
    if (val->getType() == i64_type) {
        if (nanboxed_values.count(value_id)) {
            // NaN-boxed value (could be int OR float) - use runtime conversion
            // that handles both NaN-boxed types correctly
            auto to_float = module.getOrInsertFunction("qore_rt_to_float",
                llvm::FunctionType::get(double_type, {i64_type}, false));
            return builder->CreateCall(to_float, {val});
        } else {
            // Native integer - convert to float
            return builder->CreateSIToFP(val, double_type);
        }
    }
    // For other integer types (e.g. i1 from boolean), convert to float
    if (val->getType()->isIntegerTy()) {
        return builder->CreateSIToFP(val, double_type);
    }
    // Already a floating-point type but not double (e.g. float), widen
    if (val->getType()->isFloatingPointTy()) {
        return builder->CreateFPCast(val, double_type);
    }
    // Unreachable: all LLVM value types should be handled above
    assert(false && "ensureFloatType: unexpected LLVM type");
    return val;
}

void QoreIRToLLVM::collectLocals(const QoreIRFunction& func) {
    function_locals.clear();
    entry_locals.clear();
    entry_locals_set.clear();
    instantiated_non_entry_locals.clear();
    block_scoped_locals.clear();
    local_cleanup_allocas.clear();

    // First pass: identify block-scoped locals (those with explicit UninstantiateLocal)
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (inst_ptr && inst_ptr->opcode == QoreIROpcode::UninstantiateLocal) {
                const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst_ptr.get());
                if (linst->local) {
                    block_scoped_locals.insert(reinterpret_cast<const void*>(linst->local));
                }
            }
        }
    }

    // Second pass: classify locals as entry-block vs non-entry-block
    // Block-scoped locals are excluded from entry_locals even if first seen in the entry block,
    // because they need mid-function destruction (not just function-exit cleanup).
    std::unordered_set<const void*> seen;
    bool is_first_block = true;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            if (inst->opcode == QoreIROpcode::LoadLocal || inst->opcode == QoreIROpcode::StoreLocal) {
                const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
                if (linst->local && seen.insert(linst->local).second) {
                    auto key = reinterpret_cast<const void*>(linst->local);
                    // Skip outer-scope variables: when pre_instantiated_locals is set,
                    // variables NOT in it are from an outer scope (e.g. top-level locals
                    // accessed from a sub).  These are already on the thread-local stack
                    // and must not be instantiated/uninstantiated or cached in allocas —
                    // they'll be accessed via qore_rt_load_local() on each use.
                    if (pre_instantiated_locals && !pre_instantiated_locals->count(key)) {
                        continue;
                    }
                    function_locals.push_back(linst->local);
                    // Track if this local is first accessed in the entry block
                    // but exclude block-scoped locals (they have their own lifecycle)
                    if (is_first_block && !block_scoped_locals.count(key)) {
                        entry_locals.push_back(linst->local);
                        entry_locals_set.insert(key);
                    }
                }
            }
            // Fused local int opcodes reference locals directly (not via
            // LoadLocal/StoreLocal); ensure they are discovered for alloca creation
            if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                const auto* fused = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
                for (LocalVar* var : {fused->target, fused->source}) {
                    if (var && seen.insert(var).second) {
                        auto key = reinterpret_cast<const void*>(var);
                        if (pre_instantiated_locals && !pre_instantiated_locals->count(key)) {
                            continue;
                        }
                        function_locals.push_back(var);
                        if (is_first_block && !block_scoped_locals.count(key)) {
                            entry_locals.push_back(var);
                            entry_locals_set.insert(key);
                        }
                    }
                }
            }
            if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                const auto* fused = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst);
                if (fused->local && seen.insert(fused->local).second) {
                    auto key = reinterpret_cast<const void*>(fused->local);
                    if (!pre_instantiated_locals || pre_instantiated_locals->count(key)) {
                        function_locals.push_back(fused->local);
                        if (is_first_block && !block_scoped_locals.count(key)) {
                            entry_locals.push_back(fused->local);
                            entry_locals_set.insert(key);
                        }
                    }
                }
            }
            if (inst->opcode == QoreIROpcode::BranchIfLtLocalInt) {
                const auto* fused = static_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst);
                for (LocalVar* var : {fused->lhs, fused->rhs}) {
                    if (var && seen.insert(var).second) {
                        auto key = reinterpret_cast<const void*>(var);
                        if (pre_instantiated_locals && !pre_instantiated_locals->count(key)) {
                            continue;
                        }
                        function_locals.push_back(var);
                        if (is_first_block && !block_scoped_locals.count(key)) {
                            entry_locals.push_back(var);
                            entry_locals_set.insert(key);
                        }
                    }
                }
            }
        }
        is_first_block = false;
    }
}

void QoreIRToLLVM::emitLocalInstantiation(llvm::Module& module) {
    // Only instantiate entry-block locals at function entry.
    // Non-entry-block locals are instantiated lazily on first store.
    if (aot_mode) {
        auto helper = module.getOrInsertFunction("qore_rt_instantiate_local_aot",
                llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false));
        for (LocalVar* var : entry_locals) {
            if (pre_instantiated_locals &&
                    pre_instantiated_locals->count(reinterpret_cast<const void*>(var))) {
                continue;
            }
            int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                    reinterpret_cast<const void*>(var));
            builder->CreateCall(helper, {aot_ctx_arg,
                    llvm::ConstantInt::get(i32_type, slot)});
        }
    } else {
        auto helper = module.getOrInsertFunction("qore_rt_instantiate_local",
                llvm::FunctionType::get(void_type, {ptr_type}, false));
        for (LocalVar* var : entry_locals) {
            if (pre_instantiated_locals &&
                    pre_instantiated_locals->count(reinterpret_cast<const void*>(var))) {
                continue;
            }
            llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(var));
            llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
            builder->CreateCall(helper, {var_as_ptr});
        }
    }
}

void QoreIRToLLVM::preCreateLocalAllocas(llvm::Module& module, llvm::Function* llvm_func) {
    llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
    llvm::IRBuilder<> alloca_builder(entry, entry->begin());
    for (LocalVar* var : function_locals) {
        auto key = reinterpret_cast<const void*>(var);
        if (local_allocas.count(key)) {
            continue;  // Already created
        }
        bool is_native_int = native_int_locals.count(key) > 0;
        bool is_native_float = native_float_locals.count(key) > 0;
        llvm::Type* alloca_type = is_native_float ? double_type : i64_type;
        llvm::AllocaInst* alloca = alloca_builder.CreateAlloca(alloca_type, nullptr, "local");

        // Approach B fast entry: initialize param allocas from LLVM function arguments
        if (fast_entry_args) {
            auto fa_it = fast_entry_args->find(key);
            if (fa_it != fast_entry_args->end()) {
                llvm::Value* arg_val = fa_it->second;
                // Increment refcount: the callee borrows from the caller but needs its own ref
                // for cleanup safety (Return does incref, cleanup does decref).
                auto incref_fn = module.getOrInsertFunction("qore_rt_incref",
                        llvm::FunctionType::get(void_type, {i64_type}, false));
                alloca_builder.CreateCall(incref_fn, {arg_val});

                if (is_native_int) {
                    auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    llvm::Value* native_val = alloca_builder.CreateCall(to_int, {arg_val});
                    alloca_builder.CreateStore(native_val, alloca);
                    // Native int: incref'd value cleaned up via preinstantiated_entry_loads
                    preinstantiated_entry_loads.push_back(arg_val);
                } else if (is_native_float) {
                    auto to_float = module.getOrInsertFunction("qore_rt_to_float",
                            llvm::FunctionType::get(double_type, {i64_type}, false));
                    llvm::Value* native_val = alloca_builder.CreateCall(to_float, {arg_val});
                    alloca_builder.CreateStore(native_val, alloca);
                    // Native float: incref'd value cleaned up via preinstantiated_entry_loads
                    preinstantiated_entry_loads.push_back(arg_val);
                } else {
                    alloca_builder.CreateStore(arg_val, alloca);
                    // NaN-boxed: track alloca for load+decref at exit.  Combined with
                    // decref-before-store in StoreLocal, handles param reassignment.
                    fast_entry_param_allocas.push_back(alloca);
                }
                local_allocas[key] = alloca;
                continue;
            }
            // Not a fast entry param → initialize to default (body local)
            if (is_native_int) {
                alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, 0), alloca);
            } else if (is_native_float) {
                alloca_builder.CreateStore(llvm::ConstantFP::get(double_type, 0.0), alloca);
            } else {
                alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), alloca);
            }
            local_allocas[key] = alloca;
            continue;
        }

        if (pre_instantiated_locals && pre_instantiated_locals->count(key)
                && !ir_only_body_locals.count(key)) {
            // Pre-instantiated and NOT IR-only: initialize from runtime stack
            if (aot_mode) {
                auto load_fn = module.getOrInsertFunction("qore_rt_load_local_aot",
                    llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                llvm::Value* init_val = alloca_builder.CreateCall(load_fn,
                    {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                if (is_native_int) {
                    auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    llvm::Value* native_val = alloca_builder.CreateCall(to_int, {init_val});
                    alloca_builder.CreateStore(native_val, alloca);
                } else if (is_native_float) {
                    auto to_float = module.getOrInsertFunction("qore_rt_to_float",
                            llvm::FunctionType::get(double_type, {i64_type}, false));
                    llvm::Value* native_val = alloca_builder.CreateCall(to_float, {init_val});
                    alloca_builder.CreateStore(native_val, alloca);
                } else {
                    alloca_builder.CreateStore(init_val, alloca);
                }
                preinstantiated_entry_loads.push_back(init_val);
            } else {
                auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                    llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(var));
                llvm::Value* var_as_ptr = alloca_builder.CreateIntToPtr(var_ptr, ptr_type);
                llvm::Value* init_val = alloca_builder.CreateCall(load_fn,
                    {var_as_ptr, xsink_arg});
                if (is_native_int) {
                    auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    llvm::Value* native_val = alloca_builder.CreateCall(to_int, {init_val});
                    alloca_builder.CreateStore(native_val, alloca);
                } else if (is_native_float) {
                    auto to_float = module.getOrInsertFunction("qore_rt_to_float",
                            llvm::FunctionType::get(double_type, {i64_type}, false));
                    llvm::Value* native_val = alloca_builder.CreateCall(to_float, {init_val});
                    alloca_builder.CreateStore(native_val, alloca);
                } else {
                    alloca_builder.CreateStore(init_val, alloca);
                }
                preinstantiated_entry_loads.push_back(init_val);
            }
        } else {
            // Body local: initialize to default value
            if (is_native_int) {
                alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, 0), alloca);
            } else if (is_native_float) {
                alloca_builder.CreateStore(llvm::ConstantFP::get(double_type, 0.0), alloca);
            } else {
                alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), alloca);
            }
        }
        local_allocas[key] = alloca;
    }
}

void QoreIRToLLVM::emitLocalUninstantiation(llvm::Module& module) {
    // Only uninstantiate entry-block locals at function exit.
    // Non-entry-block locals have their own UninstantiateLocal instructions
    // in the IR that handle their lifecycle.
    if (aot_mode) {
        auto helper = module.getOrInsertFunction("qore_rt_uninstantiate_local_aot",
                llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
        for (auto it = entry_locals.rbegin(); it != entry_locals.rend(); ++it) {
            if (pre_instantiated_locals &&
                    pre_instantiated_locals->count(reinterpret_cast<const void*>(*it))) {
                continue;
            }
            int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                    reinterpret_cast<const void*>(*it));
            builder->CreateCall(helper, {aot_ctx_arg,
                    llvm::ConstantInt::get(i32_type, slot), xsink_arg});
        }
    } else {
        auto helper = module.getOrInsertFunction("qore_rt_uninstantiate_local",
                llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false));
        for (auto it = entry_locals.rbegin(); it != entry_locals.rend(); ++it) {
            if (pre_instantiated_locals &&
                    pre_instantiated_locals->count(reinterpret_cast<const void*>(*it))) {
                continue;
            }
            llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(*it));
            llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
            builder->CreateCall(helper, {var_as_ptr, xsink_arg});
        }
    }
}

void QoreIRToLLVM::emitOnBlockExitExec(llvm::Module& module) {
    if (!obe_saved_count) {
        return;
    }
    auto helper = module.getOrInsertFunction("qore_rt_exec_on_block_exit",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    builder->CreateCall(helper, {obe_saved_count, xsink_arg});
}

void QoreIRToLLVM::emitPreinstantiatedCleanup(llvm::Module& module) {
    auto helper = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

    // Standard pre-instantiated entry loads: decref the originally loaded value
    for (llvm::Value* entry_val : preinstantiated_entry_loads) {
        builder->CreateCall(helper, {entry_val, xsink_arg});
    }

    // Fast entry param allocas: load current value and decref.
    // This correctly handles both:
    //   - Non-reassigned params: decrefs the original incref'd value
    //   - Reassigned params: decrefs the final value (intermediate values were
    //     decreff'd by decref-before-store in StoreLocal for IR-only locals)
    for (llvm::AllocaInst* alloca : fast_entry_param_allocas) {
        llvm::Value* val = builder->CreateLoad(i64_type, alloca);
        builder->CreateCall(helper, {val, xsink_arg});
    }
}

void QoreIRToLLVM::emitInvokeCleanup(llvm::Module& module) {
    if (invoke_result_allocas.empty()) {
        return;
    }
    auto helper = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    for (llvm::Value* alloca_ptr : invoke_result_allocas) {
        llvm::Value* val = builder->CreateLoad(i64_type, alloca_ptr);
        builder->CreateCall(helper, {val, xsink_arg});
    }
}

void QoreIRToLLVM::emitIteratorCleanup(llvm::Module& module) {
    if (iterator_cleanup_allocas.empty()) {
        return;
    }
    auto helper = module.getOrInsertFunction("qore_rt_iterator_cleanup",
            llvm::FunctionType::get(void_type, {ptr_type}, false));
    for (llvm::Value* alloca_ptr : iterator_cleanup_allocas) {
        llvm::Value* iter_ptr = builder->CreateLoad(ptr_type, alloca_ptr);
        builder->CreateCall(helper, {iter_ptr});
    }
}

void QoreIRToLLVM::trackResultForCleanup(llvm::Value* result, uint32_t result_id,
        llvm::Function* llvm_func) {
    llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
    llvm::IRBuilder<> alloca_builder(entry, entry->begin());
    llvm::AllocaInst* cleanup_alloca = alloca_builder.CreateAlloca(i64_type,
            nullptr, "cleanup");
    alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
            cleanup_alloca);
    // Decref previous value before overwriting (handles loop bodies where
    // the same alloca is stored to each iteration; first iteration old_val
    // = NOTHING which is a no-op for decref)
    auto decref_fn = current_module->getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup_alloca);
    builder->CreateStore(result, cleanup_alloca);
    builder->CreateCall(decref_fn, {old_val, xsink_arg});
    invoke_result_allocas.push_back(cleanup_alloca);
    invoke_alloca_map[result_id] = cleanup_alloca;
}

// Walk a lvalue AST expression to find the root LocalVar* key (for alloca lookup).
// Returns nullptr if the root is not a local variable.
static const void* findLvalueRootLocalKey(const QoreValue& lvalue) {
    if (!lvalue.hasNode()) {
        return nullptr;
    }
    const AbstractQoreNode* node = lvalue.getInternalNode();
    while (node) {
        if (auto* var_ref = dynamic_cast<const VarRefNode*>(node)) {
            qore_var_t type = var_ref->getType();
            if (type == VT_LOCAL || type == VT_LOCAL_TS) {
                return reinterpret_cast<const void*>(var_ref->ref.id);
            }
            return nullptr;
        }
        // Walk through binary operators (e.g., rv.body → walk left to find rv)
        if (auto* bin_op = dynamic_cast<const QoreBinaryOperatorNode<>*>(node)) {
            QoreValue left = bin_op->getLeft();
            node = left.hasNode() ? left.getInternalNode() : nullptr;
        } else {
            return nullptr;
        }
    }
    return nullptr;
}

void QoreIRToLLVM::reloadLocalFromRuntime(const void* key, llvm::Module& module,
        llvm::Function* llvm_func) {
    auto alloca_it = local_allocas.find(key);
    if (alloca_it == local_allocas.end()) {
        return;
    }

    // Skip IR-only locals — they are never modified by AST callbacks,
    // so their LLVM alloca cache is always current.
    if (ir_only_locals_set && ir_only_locals_set->count(key)) {
        return;
    }

    // Only reload locals that are guaranteed to be instantiated at all times:
    // - Entry-block locals (instantiated at function entry)
    // - Pre-instantiated locals (instantiated by caller)
    // Skip non-entry-block locals as they may have been uninstantiated.
    bool is_entry_local = entry_locals_set.count(key) > 0;
    bool is_pre_instantiated = pre_instantiated_locals && pre_instantiated_locals->count(key);
    if (!is_entry_local && !is_pre_instantiated) {
        return;
    }

    auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

    // Get or create the reload tracker alloca for this local
    auto tracker_it = local_reload_trackers.find(key);
    if (tracker_it == local_reload_trackers.end()) {
        llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
        llvm::IRBuilder<> alloca_builder(entry, entry->begin());
        llvm::AllocaInst* tracker = alloca_builder.CreateAlloca(i64_type,
                nullptr, "reload_tracker");
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), tracker);
        local_reload_trackers[key] = tracker;
        // Register for cleanup at function exit
        invoke_result_allocas.push_back(tracker);
        tracker_it = local_reload_trackers.find(key);
    }

    // Decref the previous reload value (no-op for ints/floats/bools/nothing)
    llvm::Value* old_reload = builder->CreateLoad(i64_type, tracker_it->second);
    builder->CreateCall(decref_fn, {old_reload, xsink_arg});

    // Load new value from runtime stack
    llvm::Value* reloaded;
    if (aot_mode) {
        auto load_fn = module.getOrInsertFunction("qore_rt_load_local_aot",
                llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
        reloaded = builder->CreateCall(load_fn, {aot_ctx_arg,
                llvm::ConstantInt::get(i32_type, slot), xsink_arg});
    } else {
        auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(key));
        llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
        reloaded = builder->CreateCall(load_fn, {var_as_ptr, xsink_arg});
    }

    // Update both the local alloca cache and the reload tracker
    builder->CreateStore(reloaded, alloca_it->second);
    builder->CreateStore(reloaded, tracker_it->second);
}

void QoreIRToLLVM::clearLocalReloadTracker(const void* key, llvm::Module& module,
        llvm::Function* llvm_func) {
    auto tracker_it = local_reload_trackers.find(key);
    if (tracker_it == local_reload_trackers.end()) {
        // Tracker doesn't exist yet at compile time — proactively create it so
        // the generated decref code runs on EVERY loop iteration at runtime.
        // Without this, the tracker gets created by reloadLocalFromRuntime()
        // (called AFTER this function) with +1 ref that's never cleared,
        // causing refcount inflation → copy-on-write → O(n²) for container ops.
        //
        // Apply the same eligibility checks as reloadLocalFromRuntime():
        auto alloca_it = local_allocas.find(key);
        if (alloca_it == local_allocas.end()) {
            return;
        }
        if (ir_only_locals_set && ir_only_locals_set->count(key)) {
            return;
        }
        bool is_entry_local = entry_locals_set.count(key) > 0;
        bool is_pre_instantiated = pre_instantiated_locals && pre_instantiated_locals->count(key);
        if (!is_entry_local && !is_pre_instantiated) {
            return;
        }

        // Create entry-block alloca initialized to VAL_NOTHING
        llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
        llvm::IRBuilder<> alloca_builder(entry, entry->begin());
        llvm::AllocaInst* tracker = alloca_builder.CreateAlloca(i64_type,
                nullptr, "reload_tracker");
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), tracker);
        local_reload_trackers[key] = tracker;
        invoke_result_allocas.push_back(tracker);
        tracker_it = local_reload_trackers.find(key);
    }

    auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    llvm::Value* old_val = builder->CreateLoad(i64_type, tracker_it->second);
    builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), tracker_it->second);
    builder->CreateCall(decref_fn, {old_val, xsink_arg});
}

void QoreIRToLLVM::clearAllLocalReloadTrackers(llvm::Module& module, llvm::Function* llvm_func) {
    for (auto& [key, alloca] : local_allocas) {
        clearLocalReloadTracker(key, module, llvm_func);
    }
}

llvm::AllocaInst* QoreIRToLLVM::emitPreDecrefAndClearTracker(uint32_t result_id,
        const QoreIRLValueInstruction* lvinst,
        llvm::Module& module, llvm::Function* llvm_func) {
    // Ensure cleanup alloca exists for this result
    llvm::AllocaInst* ca;
    auto it = invoke_alloca_map.find(result_id);
    if (it != invoke_alloca_map.end()) {
        ca = static_cast<llvm::AllocaInst*>(it->second);
    } else {
        llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                llvm_func->getEntryBlock().begin());
        ca = ab.CreateAlloca(i64_type, nullptr, "lv_cleanup");
        ab.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), ca);
        invoke_result_allocas.push_back(ca);
        invoke_alloca_map[result_id] = ca;
    }

    // Decref old value (first-iteration decref of NOTHING is a no-op)
    auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    llvm::Value* old_val = builder->CreateLoad(i64_type, ca);
    builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), ca);
    builder->CreateCall(decref_fn, {old_val, xsink_arg});

    // Clear the reload tracker for the lvalue target local
    const void* local_key = findLvalueRootLocalKey(lvinst->lvalue);
    if (local_key) {
        clearLocalReloadTracker(local_key, module, llvm_func);
    }

    return ca;
}

bool QoreIRToLLVM::buildArgsArray(const QoreIRInstruction* inst, int arg_start,
        llvm::Function* llvm_func, llvm::Value*& args_array, int& nargs,
        std::string& error) {
    nargs = static_cast<int>(inst->operands.size()) - arg_start;
    if (nargs > 0) {
        // Hoist alloca to entry block to avoid stack overflow in loops
        llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                llvm_func->getEntryBlock().begin());
        args_array = ab.CreateAlloca(i64_type,
                llvm::ConstantInt::get(i32_type, nargs));
        for (int i = 0; i < nargs; ++i) {
            auto* arg_val = getVal(inst->operands[arg_start + i].id, error);
            if (!arg_val) { return false; }
            llvm::Value* arg_boxed = boxValue(arg_val, inst->operands[arg_start + i].id);
            llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                    llvm::ConstantInt::get(i32_type, i));
            builder->CreateStore(arg_boxed, gep);
        }
    } else {
        args_array = builder->CreateIntToPtr(
                llvm::ConstantInt::get(i64_type, 0), ptr_type);
    }
    return true;
}

void QoreIRToLLVM::reloadAllLocalsFromRuntime(llvm::Module& module, llvm::Function* llvm_func) {
    // Phase 4: Skip entirely if all locals are IR-only — no AST callback can
    // modify any local, so the LLVM alloca cache is always current.
    if (all_locals_ir_only) {
        return;
    }
    for (auto& [key, alloca] : local_allocas) {
        // Skip IR-only locals — they cannot be modified by AST callbacks
        if (ir_only_locals_set && ir_only_locals_set->count(key)) {
            continue;
        }
        reloadLocalFromRuntime(key, module, llvm_func);
    }
}

llvm::Value* QoreIRToLLVM::boxValue(llvm::Value* val, uint32_t id) {
    if (nanboxed_values.count(id)) {
        return val;  // Already NaN-boxed
    }
    if (val->getType() == i64_type) {
        llvm::Value* result = boxInt(val);
        // boxInt for runtime values calls qore_rt_box_big_int which may allocate
        // a QoreBigIntNode (refcount=1).  Track the result for cleanup at function
        // exit so the temp ref is released.  For compile-time constants in the
        // 48-bit range, boxInt returns inline encoding (no allocation, no tracking
        // needed).  For all other cases (runtime values or out-of-range constants),
        // the result may hold a heap-allocated ref that must be released.
        if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(val)) {
            int64_t v = ci->getSExtValue();
            if (v >= INT48_MIN && v <= INT48_MAX) {
                return result;  // Inline encoding, no allocation
            }
        }
        // Runtime value or out-of-range constant: track for cleanup
        llvm::Function* func = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* entry = &func->getEntryBlock();
        llvm::IRBuilder<> alloca_builder(entry, entry->begin());
        auto* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr, "box_cleanup");
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
        // Decref previous value before overwriting (handles loop bodies where
        // the same alloca is stored to each iteration; first iteration old_val
        // = NOTHING which is a no-op for decref)
        auto decref_fn = current_module->getOrInsertFunction("qore_rt_decref",
                llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
        llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup);
        builder->CreateStore(result, cleanup);
        builder->CreateCall(decref_fn, {old_val, xsink_arg});
        invoke_result_allocas.push_back(cleanup);
        return result;
    }
    if (val->getType() == double_type) {
        return boxFloat(val);
    }
    if (val->getType() == i1_type) {
        return boxBool(val);
    }
    // Assume it's already i64 NaN-boxed if none of the above
    return val;
}

llvm::DIFile* QoreIRToLLVM::getDIFile(const char* file_path) {
    if (!file_path) {
        // Fallback for synthetic instructions with no source file
        auto it = di_file_cache.find(nullptr);
        if (it != di_file_cache.end()) {
            return it->second;
        }
        llvm::DIFile* f = active_di_builder->createFile("<jit>", ".");
        di_file_cache[nullptr] = f;
        return f;
    }

    auto it = di_file_cache.find(file_path);
    if (it != di_file_cache.end()) {
        return it->second;
    }

    // Split file path into directory + filename
    llvm::StringRef path(file_path);
    llvm::StringRef dir = "";
    llvm::StringRef filename = path;

    auto slash_pos = path.rfind('/');
    if (slash_pos != llvm::StringRef::npos) {
        dir = path.substr(0, slash_pos);
        filename = path.substr(slash_pos + 1);
    }

    llvm::DIFile* f = active_di_builder->createFile(filename, dir);
    di_file_cache[file_path] = f;
    return f;
}

void QoreIRToLLVM::setDebugLocation(const QoreIRInstruction* inst) {
    if (!di_sp) {
        return;
    }
    unsigned line = 0;
    if (inst->loc && inst->loc->start_line > 0) {
        line = static_cast<unsigned>(inst->loc->start_line);
    }
    builder->SetCurrentDebugLocation(llvm::DILocation::get(ctx, line, 0, di_sp));
}

llvm::BasicBlock* QoreIRToLLVM::getOrCreateJitDeoptBlock(llvm::Module& module,
        llvm::Function* llvm_func) {
    if (jit_deopt_block) {
        return jit_deopt_block;
    }
    // Create the block but leave it empty — body is emitted during finalization
    // (after all instructions are lowered) so that emitIteratorCleanup and
    // emitInvokeCleanup cover ALL tracked allocas.  Guards can appear after
    // ConstString/Invoke instructions; emitting cleanup at creation time would
    // miss allocas not yet created.
    jit_deopt_block = llvm::BasicBlock::Create(ctx, "jit_deopt", llvm_func);
    return jit_deopt_block;
}

void QoreIRToLLVM::emitExceptionCheck(llvm::Module& module, llvm::Function* llvm_func,
        const QoreIRInstruction* inst) {
    llvm::BasicBlock* exception_block = nullptr;
    if (inst->exception_target) {
        auto except_it = block_map.find(inst->exception_target);
        if (except_it != block_map.end()) {
            exception_block = except_it->second;
        }
    }
    if (!exception_block) {
        // Outside try block: use the function-level error return block.
        // Create it lazily on first use — terminator is added later in
        // finalizeErrorReturnBlock() after all invoke_result_allocas are known.
        if (!error_return_block) {
            error_return_block = llvm::BasicBlock::Create(ctx, "error_return", llvm_func);
        }
        exception_block = error_return_block;
    }
    auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));
    llvm::Value* ex_check = builder->CreateCall(has_ex, {xsink_arg});
    llvm::Value* has_exception = builder->CreateICmpNE(ex_check,
            llvm::ConstantInt::get(i64_type, 0));
    llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "no_exception", llvm_func);
    if (getenv("QORE_LLVM_DEBUG")) {
        fprintf(stderr, "LLVM-EXCEPT-CHECK: creating no_exception block, moving from %s to %s\n",
                builder->GetInsertBlock()->getName().str().c_str(),
                cont->getName().str().c_str());
        fflush(stderr);
    }
    builder->CreateCondBr(has_exception, exception_block, cont);
    builder->SetInsertPoint(cont);
}

bool QoreIRToLLVM::lowerFunction(const QoreIRFunction& func, llvm::Module& module, std::string& error) {
    current_ir_func = &func;
    current_module = &module;
    initTypes();
    declareRuntimeHelpers(module);

    // Clear COW tracking for new function
    cow_modified_locals.clear();

    // Determine function name: use fast_entry_name if set (Approach B)
    const std::string& fn_name = fast_entry_name.empty() ? func.name : fast_entry_name;
    bool is_fast_entry = !fast_entry_name.empty();

    // Check if the function already exists in the module (forward-declared)
    llvm::Function* llvm_func = module.getFunction(fn_name);

    if (!llvm_func) {
        // Function not forward-declared — create it now
        llvm::FunctionType* fn_type;
        if (aot_mode) {
            fn_type = llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false);
        } else {
            fn_type = llvm::FunctionType::get(i64_type, {ptr_type}, false);
        }
        llvm_func = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
                fn_name, module);
    }

    if (!llvm_func) {
        error = "failed to create LLVM function '" + fn_name + "'";
        return false;
    }

    // RAII cleanup: remove incomplete function from module on failure
    struct FunctionCleanup {
        llvm::Function* func;
        llvm::DISubprogram** di_sp_ptr;
        bool committed = false;
        ~FunctionCleanup() {
            if (!committed && func) {
                // Detach subprogram before erasing to avoid orphan metadata
                // that confuses module-level debug info verification
                if (func->getSubprogram()) {
                    func->setSubprogram(nullptr);
                }
                func->eraseFromParent();
                // Clear the subprogram pointer so the reset path doesn't reference it
                if (di_sp_ptr) {
                    *di_sp_ptr = nullptr;
                }
            }
        }
    } func_cleanup{llvm_func, &di_sp};

    // Name parameters and find xsink_arg
    if (aot_mode) {
        aot_ctx_arg = llvm_func->getArg(0);
        aot_ctx_arg->setName("ctx");
        xsink_arg = llvm_func->getArg(1);
        xsink_arg->setName("xsink");
    } else if (is_fast_entry) {
        // Fast entry: params are i64 args, xsink is the last arg
        unsigned num_args = llvm_func->arg_size();
        xsink_arg = llvm_func->getArg(num_args - 1);
        xsink_arg->setName("xsink");
    } else {
        xsink_arg = llvm_func->getArg(0);
        xsink_arg->setName("xsink");
    }

    // Propagate pre-instantiated locals set — always point to the set, even
    // when empty.  An empty set means ALL LoadLocal targets are outer-scope
    // variables (e.g. on_block_exit handler bodies that only reference enclosing
    // function params).  A nullptr disables outer-scope checks entirely.
    pre_instantiated_locals = &func.pre_instantiated_locals;

    // Propagate IR-only locals set for optimization (skip runtime sync for these)
    ir_only_locals_set = func.ir_only_locals.empty()
        ? nullptr : &func.ir_only_locals;

    // Phase 4: Check if ALL locals are IR-only, enabling bulk skip of
    // reloadAllLocalsFromRuntime() after calls.
    all_locals_ir_only = ir_only_locals_set
        && func.total_local_count > 0
        && ir_only_locals_set->size() == func.total_local_count;

    // Phase A: Identify IR-only body locals — these can skip thread-local stack
    // instantiation in the fast call path and have allocas initialized to NOTHING.
    ir_only_body_locals.clear();
    if (ir_only_locals_set && !func.all_body_locals.empty()) {
        for (LocalVar* lv : func.all_body_locals) {
            const void* key = reinterpret_cast<const void*>(lv);
            if (ir_only_locals_set->count(key)) {
                ir_only_body_locals.insert(key);
            }
        }
    }

    // Phase 3: Identify IR-only locals that can use native (unboxed) allocas.
    // Typed int/float locals that are IR-only skip boxing/unboxing overhead entirely.
    native_int_locals.clear();
    native_float_locals.clear();
    if (ir_only_locals_set) {
        for (const void* key : *ir_only_locals_set) {
            const LocalVar* lv = reinterpret_cast<const LocalVar*>(key);
            const QoreTypeInfo* ti = lv->getTypeInfo();
            if (QoreTypeInfo::isType(ti, NT_INT)) {
                native_int_locals.insert(key);
            } else if (QoreTypeInfo::isType(ti, NT_FLOAT)) {
                native_float_locals.insert(key);
            }
        }
    }

    // Phase 5c: Set up DWARF debug info
    di_file_cache.clear();

    if (shared_di_builder) {
        // Multi-function module: use shared DIBuilder and compile unit
        active_di_builder = shared_di_builder;
        di_cu = shared_di_cu;
    } else {
        // Single-function module: create owned DIBuilder and compile unit
        di_builder = std::make_unique<llvm::DIBuilder>(module);
        active_di_builder = di_builder.get();
    }

    // Find the first valid source file from the function's instructions
    const char* func_file = nullptr;
    unsigned func_line = 0;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (inst_ptr && inst_ptr->loc && inst_ptr->loc->getFile()) {
                func_file = inst_ptr->loc->getFile();
                if (inst_ptr->loc->start_line > 0) {
                    func_line = static_cast<unsigned>(inst_ptr->loc->start_line);
                }
                break;
            }
        }
        if (func_file) {
            break;
        }
    }

    llvm::DIFile* di_file = getDIFile(func_file);

    if (!shared_di_builder) {
        // Single-function module: create compile unit
        di_cu = active_di_builder->createCompileUnit(
            llvm::dwarf::DW_LANG_lo_user,  // custom Qore language
            di_file,
            "Qore JIT",                    // producer
            false,                          // isOptimized
            "",                             // flags
            0                               // runtime version
        );
    }

    // Create subroutine type (opaque — JIT ABI is uint64_t(ExceptionSink*))
    llvm::DISubroutineType* di_func_type = active_di_builder->createSubroutineType(
        active_di_builder->getOrCreateTypeArray({}));

    // Create subprogram for this function
    di_sp = active_di_builder->createFunction(
        di_file,                        // scope
        func.name,                      // name
        func.name,                      // linkage name
        di_file,                        // file
        func_line,                      // line number
        di_func_type,                   // type
        func_line,                      // scope line
        llvm::DINode::FlagPrototyped,   // flags
        llvm::DISubprogram::SPFlagDefinition  // spflags
    );
    llvm_func->setSubprogram(di_sp);

    // Add module flags for DWARF (only if not already set)
    if (!module.getModuleFlag("Dwarf Version")) {
        module.addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
    }
    if (!module.getModuleFlag("Debug Info Version")) {
        module.addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                llvm::DEBUG_METADATA_VERSION);
    }

    // Create LLVM basic blocks for all IR blocks
    block_map.clear();
    final_block_map.clear();
    block_map.reserve(func.blocks.size());
    final_block_map.reserve(func.blocks.size());
    for (const auto& block : func.blocks) {
        block_map[block.get()] = llvm::BasicBlock::Create(ctx, block->name, llvm_func);
        if (getenv("QORE_LLVM_DEBUG")) {
            fprintf(stderr, "LLVM-BLOCK-CREATE: %s (IR=%p LLVM=%p)\n",
                    block->name.c_str(), (void*)block.get(), (void*)block_map[block.get()]);
            fflush(stderr);
        }
    }

    // Create IR builder
    builder = std::make_unique<llvm::IRBuilder<>>(ctx);

    // Clear value and local maps
    values.clear();
    local_allocas.clear();
    nanboxed_values.clear();
    preinstantiated_entry_loads.clear();
    invoke_result_allocas.clear();
    invoke_alloca_map.clear();
    iterator_cleanup_allocas.clear();
    pending_phis.clear();
    local_reload_trackers.clear();
    error_return_block = nullptr;
    jit_deopt_block = nullptr;

    // Collect all unique LocalVar* pointers from the function and emit
    // instantiation calls at the start of the entry block so the Qore
    // thread-local variable stack is properly set up before any code runs.
    // Pre-instantiated locals (tiered compilation) are skipped.
    collectLocals(func);
    obe_saved_count = nullptr;
    scope_obe_counts.clear();
    if (!func.blocks.empty()) {
        builder->SetInsertPoint(block_map[func.blocks.front().get()]);
        emitLocalInstantiation(module);
        preCreateLocalAllocas(module, llvm_func);

        // Save on_block_exit handler count at function entry so we can
        // execute handlers registered during this function at exit.
        auto obe_count_fn = module.getOrInsertFunction("qore_rt_get_on_block_exit_count",
                llvm::FunctionType::get(i64_type, {}, false));
        obe_saved_count = builder->CreateCall(obe_count_fn, {});
    }

    // Lower each block
    for (const auto& block : func.blocks) {
        llvm::BasicBlock* llvm_block = block_map[block.get()];
        if (!llvm_block) {
            error = "missing LLVM basic block mapping for '" + block->name + "'";
            return false;
        }
        if (getenv("QORE_LLVM_DEBUG")) {
            fprintf(stderr, "LLVM-BLOCK-PROCESS: %s (IR=%p LLVM=%p) instructions=%zu\n",
                    block->name.c_str(), (void*)block.get(), (void*)llvm_block, block->instructions.size());
            fflush(stderr);
        }
        builder->SetInsertPoint(llvm_block);

        for (const auto& inst_ptr : block->instructions) {
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            // If the current block already has a terminator (e.g., from an Invoke that
            // created a conditional branch), skip remaining instructions in this block.
            if (builder->GetInsertBlock()->getTerminator()) {
                if (getenv("QORE_LLVM_DEBUG")) {
                    fprintf(stderr, "LLVM-SKIP: block %s already has terminator\n",
                            builder->GetInsertBlock()->getName().str().c_str());
                    fflush(stderr);
                }
                break;
            }
            // Phase 5c: Set debug location for this instruction
            setDebugLocation(inst);
            if (getenv("QORE_LLVM_DEBUG")) {
                fprintf(stderr, "LLVM-INST: opcode=%d in block=%s\n",
                        static_cast<int>(inst->opcode), builder->GetInsertBlock()->getName().str().c_str());
                fflush(stderr);
            }
            if (!lowerInstruction(inst, llvm_func, module, error)) {
                if (getenv("QORE_LLVM_DEBUG")) {
                    fprintf(stderr, "LLVM-FAIL: opcode=%d result=%%%u operands=[",
                            static_cast<int>(inst->opcode), inst->result.id);
                    for (size_t oi = 0; oi < inst->operands.size(); ++oi) {
                        fprintf(stderr, "%s%%%u", oi ? "," : "", inst->operands[oi].id);
                    }
                    fprintf(stderr, "] in block=%s error=%s\n",
                            builder->GetInsertBlock()->getName().str().c_str(),
                            error.c_str());
                    fflush(stderr);
                }
                return false;
            }
        }

        // Record the final block after lowering all instructions.
        // When lowering creates intermediate blocks (e.g., for comparisons or guards),
        // the builder ends up in a different block than the initial one.
        // This is needed for correct PHI predecessor resolution.
        final_block_map[block.get()] = builder->GetInsertBlock();

        // Verify the final insert block has a terminator.
        // Note: the insert block may have changed (e.g., guards create continuation blocks).
        // We check the original block; if it was terminated by Invoke or similar, that's fine.
        if (!llvm_block->getTerminator()) {
            // The builder may have moved to a guard continuation block; check that too
            if (!builder->GetInsertBlock()->getTerminator()) {
                if (getenv("QORE_LLVM_DEBUG")) {
                    fprintf(stderr, "LLVM-NO-TERM: block %s and insert block %s both have no terminator\n",
                            block->name.c_str(), builder->GetInsertBlock()->getName().str().c_str());
                    fflush(stderr);
                }
                error = "missing terminator in lowered block '" + block->name + "'";
                return false;
            }
        }
    }

    // PHI fixup pass: add incoming values now that all blocks are lowered
    for (auto& [phi_node, phi_inst] : pending_phis) {
        if (getenv("QORE_LLVM_DEBUG")) {
            llvm::BasicBlock* phi_bb = phi_node->getParent();
            fprintf(stderr, "PHI-FIXUP-START: PHI in block=%s predecessors=[",
                    phi_bb->getName().str().c_str());
            bool first = true;
            for (auto it = llvm::pred_begin(phi_bb), et = llvm::pred_end(phi_bb); it != et; ++it) {
                fprintf(stderr, "%s%s", first ? "" : ",", (*it)->getName().str().c_str());
                first = false;
            }
            fprintf(stderr, "] incoming_count=%zu\n", phi_inst->incoming.size());
            fflush(stderr);
        }
        for (const auto& inc : phi_inst->incoming) {
            llvm::Value* val = getVal(inc.value.id, error);
            if (!val) {
                return false;
            }
            // Use final_block_map to get the actual LLVM predecessor block.
            // When lowering creates intermediate blocks (e.g., cmp_merge for comparisons),
            // the IR block maps to a different final LLVM block than the initial one.
            llvm::BasicBlock* bb = final_block_map[inc.block];
            if (!bb) {
                // Fall back to block_map if final_block_map doesn't have an entry
                // (shouldn't happen, but be defensive)
                bb = block_map[inc.block];
            }
            if (!bb) {
                error = "PHI incoming block not found";
                return false;
            }
            // Box to i64 if needed (PHI type is i64 for NaN-boxed values)
            // IMPORTANT: Set builder insert point to BEFORE the terminator of the predecessor block
            // so that boxing instructions are placed in the correct block, not in whatever
            // block the builder was left pointing at (which may already have a terminator)
            if (getenv("QORE_LLVM_DEBUG")) {
                llvm::BasicBlock* bm_bb = block_map[inc.block];
                fprintf(stderr, "PHI-FIXUP: incoming IR=%s block_map=%s final_block_map=%s used=%s "
                        "insert_before_term=%s\n",
                        inc.block ? inc.block->name.c_str() : "NULL",
                        bm_bb ? bm_bb->getName().str().c_str() : "NULL",
                        final_block_map[inc.block]
                            ? final_block_map[inc.block]->getName().str().c_str() : "NULL",
                        bb->getName().str().c_str(),
                        bb->getTerminator() ? bb->getTerminator()->getOpcodeName() : "NO_TERM");
                fflush(stderr);
            }
            if (bb->getTerminator()) {
                builder->SetInsertPoint(bb->getTerminator());
            } else {
                builder->SetInsertPoint(bb);
            }
            val = boxValue(val, inc.value.id);
            phi_node->addIncoming(val, bb);
        }
    }
    pending_phis.clear();

    // Finalize the error return block (if used): fire on_block_exit handlers,
    // emit cleanup for all tracked allocas and pre-instantiated locals before
    // returning NOTHING.
    if (error_return_block) {
        builder->SetInsertPoint(error_return_block);
        emitOnBlockExitExec(module);
        emitIteratorCleanup(module);
        emitPreinstantiatedCleanup(module);
        emitInvokeCleanup(module);
        emitLocalUninstantiation(module);
        builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
    }

    // Finalize the JIT deopt block (if used): clean up all tracked allocas
    // (iterator and invoke result) before requesting deopt and returning.
    // Deferred from getOrCreateJitDeoptBlock() so that cleanup covers ALL
    // allocas from the entire function, not just those created before the
    // first guard instruction.  Body locals are pre-instantiated by the
    // caller and cleaned up there; on_block_exit handlers are NOT fired here
    // because the caller will re-execute the function via AST fallback.
    if (jit_deopt_block) {
        builder->SetInsertPoint(jit_deopt_block);
        emitIteratorCleanup(module);
        emitPreinstantiatedCleanup(module);
        emitInvokeCleanup(module);
        // Call qore_rt_request_jit_deopt(deopt_counter_ptr) to set the
        // thread-local deopt flag and increment the deopt counter
        auto deopt_fn = module.getOrInsertFunction("qore_rt_request_jit_deopt",
                llvm::FunctionType::get(void_type, {ptr_type}, false));
        llvm::Value* counter_ptr = builder->CreateIntToPtr(
            llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(deopt_counter_ptr)),
            ptr_type);
        builder->CreateCall(deopt_fn, {counter_ptr});
        builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
    }

    // Phase 5c: Finalize debug info before verification
    if (shared_di_builder) {
        // Shared mode: don't finalize or destroy — caller handles that
    } else if (di_builder) {
        // Owned mode: finalize now before verification
        di_builder->finalize();
    }

    // Check all blocks have terminators before LLVM verification
    for (auto& bb : *llvm_func) {
        if (!bb.getTerminator()) {
            error = "missing terminator in LLVM block '" + bb.getName().str() + "'";
            return false;
        }
    }

    // Verify the generated LLVM IR
    // In shared debug info mode, skip per-function verification because the DIBuilder
    // hasn't been finalized yet (unfinalised metadata causes spurious "!dbg attachment
    // points at wrong subprogram" errors).  Module-level verification runs after all
    // functions are lowered and the shared DIBuilder is finalized by the caller.
    // Verify the generated LLVM IR
    {
        std::string verify_error;
        llvm::raw_string_ostream verify_os(verify_error);
        if (llvm::verifyFunction(*llvm_func, &verify_os)) {
            verify_os.flush();
            if (shared_di_builder &&
                    verify_error.find("!dbg attachment points at wrong subprogram") != std::string::npos) {
                // In shared debug info mode, the DIBuilder hasn't been finalized yet.
                // Ignore spurious dbg errors — module-level verification runs after finalization.
            } else {
                error = "LLVM verification failed: " + verify_error;
                return false;
            }
        }
    }

    // Phase 5c: Reset debug info state for next function
    if (!shared_di_builder) {
        di_builder.reset();
    }
    active_di_builder = nullptr;
    di_cu = nullptr;
    di_sp = nullptr;
    di_file_cache.clear();

    // Reset per-function state
    current_ir_func = nullptr;
    current_module = nullptr;

    // Commit the function - don't clean it up on return
    func_cleanup.committed = true;
    return true;
}

bool QoreIRToLLVM::lowerInstruction(const QoreIRInstruction* inst, llvm::Function* llvm_func,
        llvm::Module& module, std::string& error) {
    printd(3, "LLVM-LOWER: opcode=%d result=%%%d\n", (int)inst->opcode, inst->result.id);
    switch (inst->opcode) {
        // === Constants ===
        case QoreIROpcode::ConstInt: {
            const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
            // Store as native i64 for typed int operations
            values[inst->result.id] = llvm::ConstantInt::get(i64_type, cinst->constant.int_value, true);
            return true;
        }
        case QoreIROpcode::ConstFloat: {
            const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
            values[inst->result.id] = llvm::ConstantFP::get(double_type, cinst->constant.float_value);
            return true;
        }
        case QoreIROpcode::ConstBool: {
            const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
            values[inst->result.id] = llvm::ConstantInt::get(i1_type, cinst->constant.bool_value ? 1 : 0);
            return true;
        }
        case QoreIROpcode::ConstNothing: {
            // NOTHING = 0 as NaN-boxed i64
            values[inst->result.id] = llvm::ConstantInt::get(i64_type, VAL_NOTHING);
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::ConstNull: {
            values[inst->result.id] = llvm::ConstantInt::get(i64_type, VAL_NULL);
            nanboxed_values.insert(inst->result.id);
            return true;
        }

        // === Typed integer arithmetic (native i64) ===
        case QoreIROpcode::AddInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAdd(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::SubInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateSub(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::MulInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateMul(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::DivInt: {
            // Phase 2E: Inline zero-check with native division for non-zero case
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_int = ensureIntTypeInline(lhs, inst->operands[0].id);
            llvm::Value* r_int = ensureIntTypeInline(rhs, inst->operands[1].id);
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* is_zero = builder->CreateICmpEQ(r_int, zero);

            llvm::BasicBlock* div_zero_bb = llvm::BasicBlock::Create(ctx, "div_zero", llvm_func);
            llvm::BasicBlock* div_ok_bb = llvm::BasicBlock::Create(ctx, "div_ok", llvm_func);
            llvm::BasicBlock* div_merge_bb = llvm::BasicBlock::Create(ctx, "div_merge", llvm_func);
            builder->CreateCondBr(is_zero, div_zero_bb, div_ok_bb);

            // Division by zero path: call runtime helper to raise exception
            builder->SetInsertPoint(div_zero_bb);
            auto helper = module.getOrInsertFunction("qore_rt_div_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* exc_result = builder->CreateCall(helper, {l_int, r_int, xsink_arg});
            builder->CreateBr(div_merge_bb);

            // Normal path: native division
            builder->SetInsertPoint(div_ok_bb);
            llvm::Value* div_result = builder->CreateSDiv(l_int, r_int);
            builder->CreateBr(div_merge_bb);

            // Merge results
            builder->SetInsertPoint(div_merge_bb);
            llvm::PHINode* phi = builder->CreatePHI(i64_type, 2, "div_result");
            phi->addIncoming(exc_result, div_zero_bb);
            phi->addIncoming(div_result, div_ok_bb);
            values[inst->result.id] = phi;
            return true;
        }
        case QoreIROpcode::ModInt: {
            // Phase 2E: Inline zero-check with native modulo for non-zero case
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_int = ensureIntTypeInline(lhs, inst->operands[0].id);
            llvm::Value* r_int = ensureIntTypeInline(rhs, inst->operands[1].id);
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* is_zero = builder->CreateICmpEQ(r_int, zero);

            llvm::BasicBlock* mod_zero_bb = llvm::BasicBlock::Create(ctx, "mod_zero", llvm_func);
            llvm::BasicBlock* mod_ok_bb = llvm::BasicBlock::Create(ctx, "mod_ok", llvm_func);
            llvm::BasicBlock* mod_merge_bb = llvm::BasicBlock::Create(ctx, "mod_merge", llvm_func);
            builder->CreateCondBr(is_zero, mod_zero_bb, mod_ok_bb);

            // Division by zero path: call runtime helper to raise exception
            builder->SetInsertPoint(mod_zero_bb);
            auto helper = module.getOrInsertFunction("qore_rt_mod_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* exc_result = builder->CreateCall(helper, {l_int, r_int, xsink_arg});
            builder->CreateBr(mod_merge_bb);

            // Normal path: native modulo
            builder->SetInsertPoint(mod_ok_bb);
            llvm::Value* mod_result = builder->CreateSRem(l_int, r_int);
            builder->CreateBr(mod_merge_bb);

            // Merge results
            builder->SetInsertPoint(mod_merge_bb);
            llvm::PHINode* phi = builder->CreatePHI(i64_type, 2, "mod_result");
            phi->addIncoming(exc_result, mod_zero_bb);
            phi->addIncoming(mod_result, mod_ok_bb);
            values[inst->result.id] = phi;
            return true;
        }

        // === Typed float arithmetic (native double) ===
        // Note: Operands may be NaN-boxed floats, native doubles, or NaN-boxed integers
        // (when using mixed int/float types). Convert to native double as needed.
        case QoreIROpcode::AddFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFAdd(l_float, r_float);
            return true;
        }
        case QoreIROpcode::SubFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFSub(l_float, r_float);
            return true;
        }
        case QoreIROpcode::MulFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFMul(l_float, r_float);
            return true;
        }
        case QoreIROpcode::DivFloat: {
            // Phase 2E: Inline zero-check with native division for non-zero case
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            llvm::Value* zero = llvm::ConstantFP::get(double_type, 0.0);
            llvm::Value* is_zero = builder->CreateFCmpOEQ(r_float, zero);

            llvm::BasicBlock* fdiv_zero_bb = llvm::BasicBlock::Create(ctx, "fdiv_zero", llvm_func);
            llvm::BasicBlock* fdiv_ok_bb = llvm::BasicBlock::Create(ctx, "fdiv_ok", llvm_func);
            llvm::BasicBlock* fdiv_merge_bb = llvm::BasicBlock::Create(ctx, "fdiv_merge", llvm_func);
            builder->CreateCondBr(is_zero, fdiv_zero_bb, fdiv_ok_bb);

            // Division by zero path: call runtime helper to raise exception
            builder->SetInsertPoint(fdiv_zero_bb);
            auto helper = module.getOrInsertFunction("qore_rt_div_float",
                    llvm::FunctionType::get(double_type, {double_type, double_type, ptr_type}, false));
            llvm::Value* exc_result = builder->CreateCall(helper, {l_float, r_float, xsink_arg});
            builder->CreateBr(fdiv_merge_bb);

            // Normal path: native division
            builder->SetInsertPoint(fdiv_ok_bb);
            llvm::Value* div_result = builder->CreateFDiv(l_float, r_float);
            builder->CreateBr(fdiv_merge_bb);

            // Merge results
            builder->SetInsertPoint(fdiv_merge_bb);
            llvm::PHINode* phi = builder->CreatePHI(double_type, 2, "fdiv_result");
            phi->addIncoming(exc_result, fdiv_zero_bb);
            phi->addIncoming(div_result, fdiv_ok_bb);
            values[inst->result.id] = phi;
            return true;
        }

        // === Dynamic (.any) arithmetic via runtime helpers ===
        // Phase 5b: AddAny/SubAny/MulAny use inline fast-paths for int+int and float+float
        case QoreIROpcode::AddAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            // Use string_concat as slow-path fallback for AddAny — it handles both
            // string concatenation and falls back to qore_rt_add_any internally for
            // non-string types.  The fast-path checks int+int and float+float inline.
            llvm::Value* result = emitAnyArithFastPath(
                llvm::Instruction::Add, llvm::Instruction::FAdd,
                "qore_rt_add_any", lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::AddString: {
            // Typed string concatenation - skip type checks, call typed helper directly
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_string_add_typed",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::StringConcat: {
            // Multi-string concatenation - a + b + c + d in single pass
            llvm::Value* args_array;
            int nargs;
            if (!buildArgsArray(inst, 0, llvm_func, args_array, nargs, error)) {
                return false;
            }
            auto helper = module.getOrInsertFunction("qore_rt_string_concat_multi",
                    llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {args_array,
                    llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::SubAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyArithFastPath(
                llvm::Instruction::Sub, llvm::Instruction::FSub,
                "qore_rt_sub_any", lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MulAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyArithFastPath(
                llvm::Instruction::Mul, llvm::Instruction::FMul,
                "qore_rt_mul_any", lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::DivAny:
        case QoreIROpcode::ModAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            const char* helper_name = (inst->opcode == QoreIROpcode::DivAny)
                ? "qore_rt_div_any" : "qore_rt_mod_any";
            auto helper = module.getOrInsertFunction(helper_name,
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }

        // === Number arithmetic operations ===
        case QoreIROpcode::AddNumber:
        case QoreIROpcode::SubNumber:
        case QoreIROpcode::MulNumber:
        case QoreIROpcode::DivNumber: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            const char* helper_name = nullptr;
            switch (inst->opcode) {
                case QoreIROpcode::AddNumber: helper_name = "qore_rt_number_add"; break;
                case QoreIROpcode::SubNumber: helper_name = "qore_rt_number_sub"; break;
                case QoreIROpcode::MulNumber: helper_name = "qore_rt_number_mul"; break;
                case QoreIROpcode::DivNumber: helper_name = "qore_rt_number_div"; break;
                default: return false;
            }
            auto helper = module.getOrInsertFunction(helper_name,
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }

        // === Bitwise integer operations ===
        case QoreIROpcode::AndInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAnd(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::OrInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateOr(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::XorInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateXor(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::ShlInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateShl(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::ShrInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAShr(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }

        // === Unary operations ===
        case QoreIROpcode::UnaryMinusInt: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            values[inst->result.id] = builder->CreateNeg(
                ensureIntTypeInline(val, inst->operands[0].id));
            return true;
        }
        case QoreIROpcode::UnaryMinusFloat: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            values[inst->result.id] = builder->CreateFNeg(val);
            return true;
        }

        // === Typed integer comparisons (native i64 → i1) ===
        case QoreIROpcode::EqInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpEQ(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::NeInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpNE(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::LtInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpSLT(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::LeInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpSLE(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::GtInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpSGT(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::GeInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpSGE(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }

        // === Typed float comparisons (native double → i1) ===
        // Note: Operands may be NaN-boxed floats, native doubles, or native integers
        // (when comparing float with int literal). Convert to native double as needed.
        case QoreIROpcode::EqFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFCmpOEQ(l_float, r_float);
            return true;
        }
        case QoreIROpcode::NeFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFCmpONE(l_float, r_float);
            return true;
        }
        case QoreIROpcode::LtFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFCmpOLT(l_float, r_float);
            return true;
        }
        case QoreIROpcode::LeFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFCmpOLE(l_float, r_float);
            return true;
        }
        case QoreIROpcode::GtFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFCmpOGT(l_float, r_float);
            return true;
        }
        case QoreIROpcode::GeFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFCmpOGE(l_float, r_float);
            return true;
        }

        // === Logical operations ===
        case QoreIROpcode::Not: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            if (val->getType() == i1_type) {
                // Native boolean: just negate
                values[inst->result.id] = builder->CreateNot(val);
            } else if (val->getType() == i64_type && nanboxed_values.count(inst->operands[0].id)) {
                // NaN-boxed value: use qore_rt_to_bool to properly interpret the value
                auto helper = module.getOrInsertFunction("qore_rt_to_bool",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                llvm::Value* bool_val = builder->CreateCall(helper, {val});
                values[inst->result.id] = builder->CreateICmpEQ(bool_val,
                        llvm::ConstantInt::get(i64_type, 0));
            } else if (val->getType() == i64_type) {
                // Raw int: compare with 0
                values[inst->result.id] = builder->CreateICmpEQ(val,
                        llvm::ConstantInt::get(i64_type, 0));
            } else if (val->getType() == double_type) {
                values[inst->result.id] = builder->CreateFCmpOEQ(val,
                        llvm::ConstantFP::get(double_type, 0.0));
            } else {
                values[inst->result.id] = builder->CreateICmpEQ(val,
                        llvm::ConstantInt::get(i64_type, 0));
            }
            return true;
        }
        case QoreIROpcode::ToBool: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            if (val->getType() == i1_type) {
                values[inst->result.id] = val;
            } else if (val->getType() == i64_type && nanboxed_values.count(inst->operands[0].id)) {
                // NaN-boxed value: use qore_rt_to_bool to properly interpret the value
                auto helper = module.getOrInsertFunction("qore_rt_to_bool",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                llvm::Value* bool_val = builder->CreateCall(helper, {val});
                values[inst->result.id] = builder->CreateICmpNE(bool_val,
                        llvm::ConstantInt::get(i64_type, 0));
            } else if (val->getType() == i64_type) {
                values[inst->result.id] = builder->CreateICmpNE(val,
                        llvm::ConstantInt::get(i64_type, 0));
            } else if (val->getType() == double_type) {
                values[inst->result.id] = builder->CreateFCmpONE(val,
                        llvm::ConstantFP::get(double_type, 0.0));
            } else {
                error = "unsupported type for ToBool lowering";
                return false;
            }
            return true;
        }

        // === Local variable operations ===
        case QoreIROpcode::LoadLocal: {
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            auto key = reinterpret_cast<const void*>(linst->local);
            bool is_native_int = native_int_locals.count(key) > 0;
            bool is_native_float = native_float_locals.count(key) > 0;


            // Closure-bound locals must always be read from the runtime stack
            // because closures can modify the value between IR instructions.
            // The alloca cache becomes stale after any call that may invoke a closure.
            //
            // qore_rt_load_local() returns a ref-counted value (refSelf'd).  This
            // is an independent reference — not aliased to the local alloca — so it
            // must be tracked for cleanup.  Without tracking, consumers like MakeList
            // (which do their own refSelf) leave the LoadLocal ref leaked.
            if (linst->local && linst->local->closureUse()) {
                llvm::Value* result;
                if (aot_mode) {
                    auto load_fn = module.getOrInsertFunction("qore_rt_load_local_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    result = builder->CreateCall(load_fn,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                    result = builder->CreateCall(load_fn, {var_as_ptr, xsink_arg});
                }
                values[inst->result.id] = result;
                nanboxed_values.insert(inst->result.id);
                // Track the ref-counted result for cleanup.  Also associate the
                // tracking alloca with the loaded local's block-scope cleanup so
                // the ref is dropped when the local is uninstantiated — not delayed
                // until function exit.  This ensures timely object destruction at
                // block scope exit (matching AST mode behavior).
                trackResultForCleanup(result, inst->result.id, llvm_func);
                if (block_scoped_locals.count(key)) {
                    auto alloca_it = invoke_alloca_map.find(inst->result.id);
                    if (alloca_it != invoke_alloca_map.end()) {
                        local_cleanup_allocas[key].push_back(alloca_it->second);
                    }
                }
                return true;
            }

            // Outer-scope variables (not in pre_instantiated_locals) must always be
            // read from the runtime stack, like closure-bound locals.  They're already
            // on the thread-local variable stack from the calling scope and must not be
            // cached in allocas (which would be initialized to NOTHING and become stale
            // after any call that modifies the outer variable).
            if (linst->local && pre_instantiated_locals
                    && !pre_instantiated_locals->count(key)) {
                llvm::Value* result;
                if (aot_mode) {
                    auto load_fn = module.getOrInsertFunction("qore_rt_load_local_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    result = builder->CreateCall(load_fn,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                    result = builder->CreateCall(load_fn, {var_as_ptr, xsink_arg});
                }
                values[inst->result.id] = result;
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(result, inst->result.id, llvm_func);
                return true;
            }


            // If this local underwent COW, reload fresh from runtime stack instead of
            // using cached alloca, which may have the old pre-COW value.
            if (linst->local && cow_modified_locals.count(linst->local)) {
                llvm::Value* result;
                if (aot_mode) {
                    auto load_fn = module.getOrInsertFunction("qore_rt_load_local_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    result = builder->CreateCall(load_fn,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                    result = builder->CreateCall(load_fn, {var_as_ptr, xsink_arg});
                }
                values[inst->result.id] = result;
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(result, inst->result.id, llvm_func);
                return true;
            }

            auto it = local_allocas.find(key);
            if (it == local_allocas.end()) {
                // Create alloca in entry block for this local
                llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                llvm::Type* alloca_type = is_native_float ? double_type : i64_type;
                llvm::AllocaInst* alloca = alloca_builder.CreateAlloca(alloca_type, nullptr, "local");
                // For pre-instantiated locals (tiered compilation: params, argvid, selfid,
                // body locals), initialize from the Qore runtime stack so the JIT sees
                // the values set up by the calling convention.  For other locals (nested
                // block vars), initialize to NOTHING (they're instantiated by
                // emitLocalInstantiation which runs after these entry-block allocas).
                if (linst->local && pre_instantiated_locals &&
                        pre_instantiated_locals->count(key)) {
                    if (ir_only_body_locals.count(key)) {
                        // IR-only body local: not on the runtime stack (fast call path
                        // skips instantiation), so initialize alloca to default value
                        if (is_native_int) {
                            alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, 0), alloca);
                        } else if (is_native_float) {
                            alloca_builder.CreateStore(llvm::ConstantFP::get(double_type, 0.0), alloca);
                        } else {
                            alloca_builder.CreateStore(
                                    llvm::ConstantInt::get(i64_type, VAL_NOTHING), alloca);
                        }
                    } else if (aot_mode) {
                        auto load_fn = module.getOrInsertFunction("qore_rt_load_local_aot",
                            llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                        llvm::Value* init_val = alloca_builder.CreateCall(load_fn,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                        if (is_native_int) {
                            // Convert NaN-boxed value from runtime to native int
                            // Use ensureIntType (safe in entry block context)
                            auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                                    llvm::FunctionType::get(i64_type, {i64_type}, false));
                            llvm::Value* native_val = alloca_builder.CreateCall(to_int, {init_val});
                            alloca_builder.CreateStore(native_val, alloca);
                        } else if (is_native_float) {
                            auto to_float = module.getOrInsertFunction("qore_rt_to_float",
                                    llvm::FunctionType::get(double_type, {i64_type}, false));
                            llvm::Value* native_val = alloca_builder.CreateCall(to_float, {init_val});
                            alloca_builder.CreateStore(native_val, alloca);
                        } else {
                            alloca_builder.CreateStore(init_val, alloca);
                        }
                        preinstantiated_entry_loads.push_back(init_val);
                    } else {
                        auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                        llvm::Value* var_as_ptr = alloca_builder.CreateIntToPtr(var_ptr, ptr_type);
                        llvm::Value* init_val = alloca_builder.CreateCall(load_fn,
                            {var_as_ptr, xsink_arg});
                        if (is_native_int) {
                            auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                                    llvm::FunctionType::get(i64_type, {i64_type}, false));
                            llvm::Value* native_val = alloca_builder.CreateCall(to_int, {init_val});
                            alloca_builder.CreateStore(native_val, alloca);
                        } else if (is_native_float) {
                            auto to_float = module.getOrInsertFunction("qore_rt_to_float",
                                    llvm::FunctionType::get(double_type, {i64_type}, false));
                            llvm::Value* native_val = alloca_builder.CreateCall(to_float, {init_val});
                            alloca_builder.CreateStore(native_val, alloca);
                        } else {
                            alloca_builder.CreateStore(init_val, alloca);
                        }
                        preinstantiated_entry_loads.push_back(init_val);
                    }
                } else {
                    // Initialize to default: 0 for native int, 0.0 for native float, NOTHING for boxed
                    if (is_native_int) {
                        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, 0), alloca);
                    } else if (is_native_float) {
                        alloca_builder.CreateStore(llvm::ConstantFP::get(double_type, 0.0), alloca);
                    } else {
                        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), alloca);
                    }
                }
                local_allocas[key] = alloca;
                it = local_allocas.find(key);
            }
            if (is_native_float) {
                values[inst->result.id] = builder->CreateLoad(double_type, it->second);
                // NOT in nanboxed_values — this is a native double
            } else {
                llvm::Value* loaded = builder->CreateLoad(i64_type, it->second);
                if (!is_native_int) {
                    // Untyped locals (auto/any) may hold reference nodes (NT_REFERENCE)
                    // when assigned from container iterators (e.g., ListIterator::getValue()
                    // returns raw references stored via \var).  In AST mode,
                    // VarRefNode::evalImpl() transparently dereferences these; the JIT
                    // must do the same via a runtime helper.
                    // Only untyped locals need this check — typed locals can't hold raw
                    // reference nodes because the type system prevents it.
                    if (linst->local
                            && !QoreTypeInfo::hasType(linst->local->getTypeInfo())) {
                        auto deref_fn = module.getOrInsertFunction(
                            "qore_rt_deref_if_reference",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                        loaded = builder->CreateCall(deref_fn, {loaded, xsink_arg});
                    }
                    nanboxed_values.insert(inst->result.id);
                }
                values[inst->result.id] = loaded;
                // Native int: NOT in nanboxed_values — ensureIntTypeInline skips unboxing
            }
            return true;
        }
        case QoreIROpcode::StoreLocal: {
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            auto key = reinterpret_cast<const void*>(linst->local);
            bool is_native_int = native_int_locals.count(key) > 0;
            bool is_native_float = native_float_locals.count(key) > 0;

            // Outer-scope variables: assign directly to the thread-local stack via
            // qore_rt_assign_local() without creating an alloca or lazy instantiation.
            if (linst->local && pre_instantiated_locals
                    && !pre_instantiated_locals->count(key)) {
                // Box the value for the runtime assign helper
                llvm::Value* boxed;
                if (nanboxed_values.count(inst->operands[0].id)) {
                    boxed = val;
                } else if (val->getType() == i64_type) {
                    boxed = boxIntInline(val);
                } else if (val->getType() == double_type) {
                    boxed = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed = boxBool(val);
                } else {
                    boxed = val;
                }

                // Check if complex-typed: apply coercion before runtime assignment
                bool is_complex_typed_outer = QoreTypeInfo::isComplex(linst->local->getTypeInfo())
                        && !QoreTypeInfo::isReference(linst->local->getTypeInfo());
                if (is_complex_typed_outer) {
                    // Apply type coercion for outer-scope complex-typed locals
                    llvm::Function* func = builder->GetInsertBlock()->getParent();
                    llvm::BasicBlock* entry = &func->getEntryBlock();
                    llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                    auto* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr,
                            "coerce_cleanup_outer");
                    alloca_builder.CreateStore(
                            llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);

                    llvm::Value* coerced;
                    if (aot_mode) {
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                        auto coerce_fn = module.getOrInsertFunction("qore_rt_coerce_value_aot",
                                llvm::FunctionType::get(i64_type,
                                        {ptr_type, i32_type, i64_type, ptr_type, ptr_type}, false));
                        coerced = builder->CreateCall(coerce_fn,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                                 boxed, cleanup, xsink_arg});
                    } else {
                        auto coerce_fn = module.getOrInsertFunction("qore_rt_coerce_value",
                                llvm::FunctionType::get(i64_type, {ptr_type, i64_type, ptr_type, ptr_type}, false));
                        llvm::Value* ti_ptr = llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(linst->local->getTypeInfo()));
                        llvm::Value* ti_as_ptr = builder->CreateIntToPtr(ti_ptr, ptr_type);
                        coerced = builder->CreateCall(coerce_fn,
                                {ti_as_ptr, boxed, cleanup, xsink_arg});
                    }
                    emitExceptionCheck(module, llvm_func, inst);
                    boxed = coerced;
                    invoke_result_allocas.push_back(cleanup);
                }

                if (aot_mode) {
                    const char* helper_name = is_complex_typed_outer ? "qore_rt_assign_local_no_coerce_aot"
                            : "qore_rt_assign_local_aot";
                    auto assign_helper = module.getOrInsertFunction(helper_name,
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    builder->CreateCall(assign_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), boxed, xsink_arg});
                } else {
                    const char* helper_name = is_complex_typed_outer ? "qore_rt_assign_local_no_coerce"
                            : "qore_rt_assign_local";
                    auto assign_helper = module.getOrInsertFunction(helper_name,
                            llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                    builder->CreateCall(assign_helper, {var_as_ptr, boxed, xsink_arg});
                }
                if (inst->result.isValid()) {
                    // The result is a borrowed reference: qore_rt_assign_local()
                    // transferred ownership to the thread-local variable stack.
                    // This is safe because:
                    // (1) the value remains live on the variable stack until the
                    //     enclosing scope uninstantiates the variable,
                    // (2) SSA guarantees single-use so no intervening reassignment
                    //     can invalidate the borrowed ref before it's consumed, and
                    // (3) nanboxed marking prevents trackResultForCleanup / double-deref
                    //     at function cleanup time.
                    values[inst->result.id] = boxed;
                    nanboxed_values.insert(inst->result.id);
                }
                return true;
            }

            // For non-entry-block locals that are not pre-instantiated:
            // emit instantiation on first store (lazy instantiation).
            //
            // INVARIANT: The first StoreLocal encountered during IR lowering must be the
            // first one to execute at runtime. This holds because:
            // 1. Block-scoped variables in Qore are declared at a single point
            // 2. IR generation emits InstantiateLocal/StoreLocal at the declaration site
            // 3. The variable cannot be stored to before its declaration due to scoping rules
            //
            // If IR generation ever allows multiple entry points to a block-scoped variable
            // (e.g., via computed gotos), this assumption would need to be revisited.
            bool is_entry_local = entry_locals_set.count(key) > 0;
            bool is_pre_instantiated = pre_instantiated_locals && pre_instantiated_locals->count(key);
            if (!is_entry_local && !is_pre_instantiated &&
                    instantiated_non_entry_locals.insert(key).second) {
                // First store to this non-entry local - emit instantiation
                if (aot_mode) {
                    auto inst_helper = module.getOrInsertFunction("qore_rt_instantiate_local_aot",
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    builder->CreateCall(inst_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot)});
                } else {
                    auto inst_helper = module.getOrInsertFunction("qore_rt_instantiate_local",
                            llvm::FunctionType::get(void_type, {ptr_type}, false));
                    llvm::Value* var_ptr_inst = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr_inst = builder->CreateIntToPtr(var_ptr_inst, ptr_type);
                    builder->CreateCall(inst_helper, {var_as_ptr_inst});
                }
            }

            auto it = local_allocas.find(key);
            if (it == local_allocas.end()) {
                llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                if (is_native_float) {
                    llvm::AllocaInst* alloca = alloca_builder.CreateAlloca(double_type, nullptr, "local");
                    alloca_builder.CreateStore(llvm::ConstantFP::get(double_type, 0.0), alloca);
                    local_allocas[key] = alloca;
                } else {
                    llvm::AllocaInst* alloca = alloca_builder.CreateAlloca(i64_type, nullptr, "local");
                    if (is_native_int) {
                        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, 0), alloca);
                    } else {
                        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), alloca);
                    }
                    local_allocas[key] = alloca;
                }
                it = local_allocas.find(key);
            }

            // Native int local: store native i64 directly (no boxing)
            if (is_native_int) {
                llvm::Value* native_val = ensureIntTypeInline(val, inst->operands[0].id);
                builder->CreateStore(native_val, it->second);
                if (inst->result.isValid()) {
                    values[inst->result.id] = native_val;
                    // NOT nanboxed
                }
                return true;
            }

            // Native float local: store native double directly (no boxing)
            if (is_native_float) {
                llvm::Value* native_val = ensureFloatType(val, inst->operands[0].id, module);
                builder->CreateStore(native_val, it->second);
                if (inst->result.isValid()) {
                    values[inst->result.id] = native_val;
                    // NOT nanboxed
                }
                return true;
            }

            // Standard (NaN-boxed) local: box the value to NaN-boxed i64.
            // Use inline boxing for int values (safe — not in PHI fixup context).
            llvm::Value* boxed;
            bool need_box_cleanup = false;
            if (nanboxed_values.count(inst->operands[0].id)) {
                boxed = val;  // Already NaN-boxed
            } else if (val->getType() == i64_type) {
                boxed = boxIntInline(val);
                // boxIntInline may allocate a QoreBigIntNode (big path).  Track the
                // result for cleanup at function exit.  For the inline INT48 path, the
                // value is a tagged immediate — decref is a no-op.
                need_box_cleanup = true;
                if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(val)) {
                    int64_t v = ci->getSExtValue();
                    if (v >= INT48_MIN && v <= INT48_MAX) {
                        need_box_cleanup = false;  // Inline constant — no allocation
                    }
                }
            } else if (val->getType() == double_type) {
                boxed = boxFloat(val);
            } else if (val->getType() == i1_type) {
                boxed = boxBool(val);
            } else {
                boxed = val;  // Assume already NaN-boxed
            }
            if (need_box_cleanup) {
                // Runtime value or out-of-range constant: track for cleanup
                llvm::Function* func = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* entry = &func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                auto* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr,
                        "box_cleanup");
                alloca_builder.CreateStore(
                        llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                auto decref_fn = current_module->getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type},
                                false));
                llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup);
                builder->CreateStore(boxed, cleanup);
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
                invoke_result_allocas.push_back(cleanup);
            }
            // Fast entry mode: decref old alloca value before overwrite to prevent leaks
            // when the local is reassigned (e.g., param overwrite, loop body re-execution).
            // In fast entry mode, the alloca is the sole owner of the value (no runtime stack
            // copy), so it's safe to decref on overwrite.  In standard entry mode, the runtime
            // stack also holds a reference, so decref here would cause a double-free — the
            // runtime stack cleanup handles it instead via qore_rt_assign_local().
            if (fast_entry_args && ir_only_locals_set && ir_only_locals_set->count(key)) {
                llvm::Value* old_val = builder->CreateLoad(i64_type, it->second);
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
            }
            // Complex type coercion: acceptAssignment() sets complexTypeInfo on
            // lists/hashes so runtime variant resolution can match typed signatures
            // like translate(list<hash<auto>>, string).  This must happen BEFORE
            // storing to the alloca because LoadLocal reads from the alloca directly.
            bool is_ir_only = ir_only_locals_set && ir_only_locals_set->count(key);
            bool is_complex_typed = linst->local
                && QoreTypeInfo::isComplex(linst->local->getTypeInfo())
                && !QoreTypeInfo::isReference(linst->local->getTypeInfo());

            if (is_complex_typed && !is_ir_only) {
                // Apply type coercion: stores complexTypeInfo on the value for runtime
                // variant matching (e.g., list<hash<auto>> matches typed signatures).
                // Must happen BEFORE storing to alloca. Coerce once here; use no-coerce
                // assign variant below to avoid double-coercion on the runtime stack.
                llvm::Function* func = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* entry = &func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                auto* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr,
                        "coerce_cleanup");
                alloca_builder.CreateStore(
                        llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);

                llvm::Value* coerced;
                if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    auto coerce_fn = module.getOrInsertFunction("qore_rt_coerce_value_aot",
                            llvm::FunctionType::get(i64_type,
                                    {ptr_type, i32_type, i64_type, ptr_type, ptr_type}, false));
                    coerced = builder->CreateCall(coerce_fn,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                             boxed, cleanup, xsink_arg});
                } else {
                    auto coerce_fn = module.getOrInsertFunction("qore_rt_coerce_value",
                            llvm::FunctionType::get(i64_type, {ptr_type, i64_type, ptr_type, ptr_type}, false));
                    llvm::Value* ti_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local->getTypeInfo()));
                    llvm::Value* ti_as_ptr = builder->CreateIntToPtr(ti_ptr, ptr_type);
                    coerced = builder->CreateCall(coerce_fn,
                            {ti_as_ptr, boxed, cleanup, xsink_arg});
                }
                emitExceptionCheck(module, llvm_func, inst);
                boxed = coerced;
                invoke_result_allocas.push_back(cleanup);
            }

            builder->CreateStore(boxed, it->second);
            if (inst->result.isValid()) {
                values[inst->result.id] = boxed;
            }
            // Track cleanup alloca for block-scoped locals: if the stored value has
            // an invoke-result cleanup alloca, record the mapping so UninstantiateLocal
            // can clear it to allow timely destruction at block scope exit.
            if (block_scoped_locals.count(key)) {
                auto alloca_it = invoke_alloca_map.find(inst->operands[0].id);
                if (alloca_it != invoke_alloca_map.end()) {
                    local_cleanup_allocas[key].push_back(alloca_it->second);
                }
            }
            // Sync to Qore thread-local variable stack so AST callbacks can resolve this local.
            // Skip sync for IR-only locals — they are never accessed by AST callbacks.
            if (linst->local && !is_ir_only) {
                // For complex-typed locals, use no-coerce variant because coercion was already
                // applied above via qore_rt_coerce_value. This avoids double-coercion.
                const char* aot_helper_name = is_complex_typed ? "qore_rt_assign_local_no_coerce_aot"
                        : "qore_rt_assign_local_aot";
                const char* jit_helper_name = is_complex_typed ? "qore_rt_assign_local_no_coerce"
                        : "qore_rt_assign_local";

                if (aot_mode) {
                    auto assign_helper = module.getOrInsertFunction(aot_helper_name,
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    builder->CreateCall(assign_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), boxed, xsink_arg});
                } else {
                    auto assign_helper = module.getOrInsertFunction(jit_helper_name,
                            llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                    builder->CreateCall(assign_helper, {var_as_ptr, boxed, xsink_arg});
                }
                // Check for exceptions from type-checked assignment (e.g. RUNTIME-TYPE-ERROR
                // when assigning NOTHING to a typed variable like hash<ExceptionInfo>)
                emitExceptionCheck(module, llvm_func, inst);
                // If the variable holds a reference, qore_rt_assign_local wrote through
                // the reference to another variable on the thread-local stack.  Reload all
                // local allocas from runtime to prevent stale reads from the target variable.
                if (QoreTypeInfo::isReference(linst->local->getTypeInfo())) {
                    reloadAllLocalsFromRuntime(module, llvm_func);
                }
            }
            return true;
        }
        case QoreIROpcode::UninstantiateLocal: {
            // Cleanup for block-scoped local variables at scope exit.
            // This pairs with the lazy instantiation in StoreLocal - we only uninstantiate
            // locals that were actually instantiated during this execution path.
            // See the INVARIANT comment in StoreLocal for the assumption this relies on.
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            if (!linst->local) {
                // No local variable specified - this is a no-op
                return true;
            }
            auto key = reinterpret_cast<const void*>(linst->local);

            // Pre-instantiated or entry-block block-scoped locals: clear the value
            // on the runtime stack to trigger destructors at block scope exit.
            // The entry stays on the stack so the caller/epilogue can pop it later.
            bool is_pre_instantiated = pre_instantiated_locals && pre_instantiated_locals->count(key);
            bool is_entry_block_scoped = entry_locals_set.count(key) && block_scoped_locals.count(key);
            if (is_pre_instantiated || is_entry_block_scoped) {
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

                // First, clear any invoke-result cleanup allocas that hold references
                // to values stored in this local.  This drops the intermediate reference
                // so the object can be destroyed when the runtime stack is cleared.
                // NOTE: We do NOT clear the local_cleanup_allocas list here because
                // multiple code paths (break, continue, normal exit) may each have
                // their own UninstantiateLocal for the same variable.  Each path must
                // independently generate decref code for the cleanup allocas.  At
                // runtime, only one path executes, and the alloca is reset to NOTHING
                // after decref, so duplicate paths (if somehow both reached) are safe.
                auto cleanup_it = local_cleanup_allocas.find(key);
                if (cleanup_it != local_cleanup_allocas.end()) {
                    for (llvm::Value* cleanup_alloca : cleanup_it->second) {
                        llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup_alloca);
                        builder->CreateStore(
                                llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup_alloca);
                        builder->CreateCall(decref_fn, {old_val, xsink_arg});
                    }
                }

                // Clear the reload tracker for this local (if it exists).
                // reloadAllLocalsFromRuntime() creates refSelf'd values in reload
                // trackers via qore_rt_load_local() — if we don't clear them here,
                // the extra reference prevents timely destruction at block scope exit.
                auto tracker_it = local_reload_trackers.find(key);
                if (tracker_it != local_reload_trackers.end()) {
                    llvm::Value* old_tracker = builder->CreateLoad(i64_type, tracker_it->second);
                    builder->CreateStore(
                            llvm::ConstantInt::get(i64_type, VAL_NOTHING), tracker_it->second);
                    builder->CreateCall(decref_fn, {old_tracker, xsink_arg});
                }

                // Clear the runtime stack/cvstack entry (drops refSelf'd reference).
                // The cleanup alloca decref above dropped the original reference from
                // new_object/invoke.  Together, all references are dropped and the
                // destructor fires.
                // Skip for IR-only body locals: they are not on the thread-local stack
                // (fast call path skips instantiation when areAllBodyLocalsIROnly()),
                // so there is nothing to clear on the runtime stack.
                if (!ir_only_body_locals.count(key)) {
                    if (aot_mode) {
                        auto clear_helper = module.getOrInsertFunction("qore_rt_clear_local_aot",
                                llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                        builder->CreateCall(clear_helper, {aot_ctx_arg,
                                llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                    } else {
                        auto clear_helper = module.getOrInsertFunction("qore_rt_clear_local",
                                llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false));
                        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(linst->local));
                        llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                        builder->CreateCall(clear_helper, {var_as_ptr, xsink_arg});
                    }
                }

                // Reset the local alloca to default (no decref — the cleanup alloca
                // decref above and clear_local above handle the two references).
                auto alloca_it = local_allocas.find(key);
                if (alloca_it != local_allocas.end()) {
                    bool is_native_int = native_int_locals.count(key) > 0;
                    bool is_native_float = native_float_locals.count(key) > 0;
                    if (is_native_int) {
                        builder->CreateStore(llvm::ConstantInt::get(i64_type, 0), alloca_it->second);
                    } else if (is_native_float) {
                        builder->CreateStore(llvm::ConstantFP::get(double_type, 0.0), alloca_it->second);
                    } else {
                        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                                alloca_it->second);
                    }
                }
                return true;
            }

            // Skip entry-block locals that are NOT block-scoped - they are uninstantiated
            // by emitLocalUninstantiation at function return
            if (entry_locals_set.count(key)) {
                return true;
            }

            // Skip non-entry locals that were never instantiated (never had a first store).
            // This check is critical: if StoreLocal was never executed for this local
            // (e.g., control flow skipped the declaration), we must not uninstantiate it.
            if (instantiated_non_entry_locals.count(key) == 0) {
                return true;
            }

            // For block-scoped locals: clear cleanup allocas and reload trackers
            // before uninstantiating.  trackResultForCleanup holds the original creation
            // reference while assign_local's refSelf and reloadAllLocalsFromRuntime's
            // refSelf may add more.  We must drop all of them here.
            {
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                auto cleanup_it = local_cleanup_allocas.find(key);
                if (cleanup_it != local_cleanup_allocas.end() && !cleanup_it->second.empty()) {
                    for (llvm::Value* cleanup_alloca : cleanup_it->second) {
                        llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup_alloca);
                        builder->CreateStore(
                                llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup_alloca);
                        builder->CreateCall(decref_fn, {old_val, xsink_arg});
                    }
                    cleanup_it->second.clear();
                }
                auto tracker_it = local_reload_trackers.find(key);
                if (tracker_it != local_reload_trackers.end()) {
                    llvm::Value* old_tracker = builder->CreateLoad(i64_type, tracker_it->second);
                    builder->CreateStore(
                            llvm::ConstantInt::get(i64_type, VAL_NOTHING), tracker_it->second);
                    builder->CreateCall(decref_fn, {old_tracker, xsink_arg});
                }
            }

            // Call runtime helper to uninstantiate the local variable
            if (aot_mode) {
                auto it = aot_slots->local_slots.find(linst->local);
                if (it == aot_slots->local_slots.end()) {
                    error = "UninstantiateLocal: local variable not in AOT slot map";
                    return false;
                }
                int32_t slot_idx = it->second;
                auto helper = module.getOrInsertFunction("qore_rt_uninstantiate_local_aot",
                        llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
                builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot_idx), xsink_arg});
            } else {
                auto helper = module.getOrInsertFunction("qore_rt_uninstantiate_local",
                        llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false));
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                builder->CreateCall(helper, {var_as_ptr, xsink_arg});
            }

            // Reset local alloca to default value to prevent stale reads after uninstantiation
            {
                auto alloca_it = local_allocas.find(key);
                if (alloca_it != local_allocas.end()) {
                    bool is_native_int = native_int_locals.count(key) > 0;
                    bool is_native_float = native_float_locals.count(key) > 0;
                    if (is_native_int) {
                        builder->CreateStore(llvm::ConstantInt::get(i64_type, 0),
                                alloca_it->second);
                    } else if (is_native_float) {
                        builder->CreateStore(llvm::ConstantFP::get(double_type, 0.0),
                                alloca_it->second);
                    } else {
                        builder->CreateStore(
                                llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                                alloca_it->second);
                    }
                }
            }
            return true;
        }

        // === Phi nodes ===
        case QoreIROpcode::Phi: {
            const auto* phi = static_cast<const QoreIRPhiInstruction*>(inst);
            // Phi type: use i64 as the common type for NaN-boxed values.
            llvm::PHINode* phi_node = builder->CreatePHI(i64_type, phi->incoming.size());
            values[inst->result.id] = phi_node;
            // PHI carries NaN-boxed i64 values (incoming values are boxed in fixup pass)
            nanboxed_values.insert(inst->result.id);
            // Store for fixup pass after all blocks are lowered (incoming values
            // may not be lowered yet due to forward edges).
            pending_phis.push_back({phi_node, phi});
            return true;
        }

        // === Refcount operations ===
        case QoreIROpcode::Incref: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* boxed = val;
            if (val->getType() != i64_type) {
                if (val->getType() == double_type) {
                    boxed = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed = boxBool(val);
                }
            }
            auto helper = module.getOrInsertFunction("qore_rt_incref",
                    llvm::FunctionType::get(void_type, {i64_type}, false));
            builder->CreateCall(helper, {boxed});
            return true;
        }
        case QoreIROpcode::Decref: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* boxed = val;
            if (val->getType() != i64_type) {
                if (val->getType() == double_type) {
                    boxed = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed = boxBool(val);
                }
            }
            auto helper = module.getOrInsertFunction("qore_rt_decref",
                    llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
            builder->CreateCall(helper, {boxed, xsink_arg});
            return true;
        }
        case QoreIROpcode::DecrefNoThrow: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* boxed = val;
            if (val->getType() != i64_type) {
                if (val->getType() == double_type) {
                    boxed = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed = boxBool(val);
                }
            }
            auto helper = module.getOrInsertFunction("qore_rt_decref_nothrow",
                    llvm::FunctionType::get(void_type, {i64_type}, false));
            builder->CreateCall(helper, {boxed});
            return true;
        }

        // === Guard operations ===
        case QoreIROpcode::GuardNotNothing: {
            const auto* ginst = static_cast<const QoreIRGuardInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            // Box if needed
            llvm::Value* boxed = val;
            if (val->getType() != i64_type) {
                if (val->getType() == double_type) {
                    boxed = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed = boxBool(val);
                }
            }
            // Profile-informed: use inline check when profile shows value is rarely NOTHING
            llvm::Value* guard_pass;
            bool profile_hot = false;
            if (current_ir_func && ginst->guard_id < current_ir_func->guard_profile_count) {
                const TypeProfile& prof = current_ir_func->guard_profiles[ginst->guard_id];
                if (prof.total() >= 50 && prof.nothing_count.load(std::memory_order_relaxed) == 0) {
                    profile_hot = true;
                }
            }
            if (profile_hot) {
                // Inline check: NOTHING is represented as 0
                guard_pass = builder->CreateICmpNE(boxed,
                    llvm::ConstantInt::get(i64_type, VAL_NOTHING), "guard_not_nothing_inline");
            } else {
                auto helper = module.getOrInsertFunction("qore_rt_guard_not_nothing",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                llvm::Value* guard_result = builder->CreateCall(helper, {boxed});
                guard_pass = builder->CreateICmpNE(guard_result,
                        llvm::ConstantInt::get(i64_type, 0));
            }
            // Branch: pass → continue, fail → JIT deopt (return to evalTiered
            // which re-executes via AST, matching IR interpreter behavior)
            if (ginst->deopt_target) {
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                llvm::BasicBlock* deopt = getOrCreateJitDeoptBlock(module, llvm_func);
                if (profile_hot) {
                    auto* weights = llvm::MDBuilder(ctx).createBranchWeights(999, 1);
                    builder->CreateCondBr(guard_pass, cont, deopt, weights);
                } else {
                    builder->CreateCondBr(guard_pass, cont, deopt);
                }
                builder->SetInsertPoint(cont);
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = guard_pass;
            }
            return true;
        }
        case QoreIROpcode::GuardInt: {
            const auto* ginst = static_cast<const QoreIRGuardInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* boxed = val;
            if (val->getType() != i64_type) {
                if (val->getType() == double_type) {
                    boxed = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed = boxBool(val);
                }
            }
            // Profile-informed: use inline NaN-boxing check instead of runtime call
            // Int check: value < TAG_INT48 (int values are in the lower range)
            llvm::Value* guard_pass;
            bool profile_hot = false;
            if (current_ir_func && ginst->guard_id < current_ir_func->guard_profile_count) {
                const TypeProfile& prof = current_ir_func->guard_profiles[ginst->guard_id];
                if (prof.total() >= 50 && prof.dominantType() == NT_INT) {
                    profile_hot = true;
                }
            }
            if (profile_hot || boxed->getType() == i64_type) {
                // Inline check: NaN-boxed int has bits < TAG_INT48
                guard_pass = builder->CreateICmpULT(boxed,
                    llvm::ConstantInt::get(i64_type, TAG_INT48), "guard_int_inline");
            } else {
                auto helper = module.getOrInsertFunction("qore_rt_guard_int",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                llvm::Value* guard_result = builder->CreateCall(helper, {boxed});
                guard_pass = builder->CreateICmpNE(guard_result,
                        llvm::ConstantInt::get(i64_type, 0));
            }
            if (ginst->deopt_target) {
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                llvm::BasicBlock* deopt = getOrCreateJitDeoptBlock(module, llvm_func);
                if (profile_hot) {
                    auto* weights = llvm::MDBuilder(ctx).createBranchWeights(999, 1);
                    builder->CreateCondBr(guard_pass, cont, deopt, weights);
                } else {
                    builder->CreateCondBr(guard_pass, cont, deopt);
                }
                builder->SetInsertPoint(cont);
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = guard_pass;
            }
            return true;
        }
        case QoreIROpcode::GuardFloat: {
            const auto* ginst = static_cast<const QoreIRGuardInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* boxed = val;
            if (val->getType() != i64_type) {
                if (val->getType() == double_type) {
                    boxed = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed = boxBool(val);
                }
            }
            // Profile-informed: use inline NaN-boxing check for float
            llvm::Value* guard_pass;
            bool profile_hot = false;
            if (current_ir_func && ginst->guard_id < current_ir_func->guard_profile_count) {
                const TypeProfile& prof = current_ir_func->guard_profiles[ginst->guard_id];
                if (prof.total() >= 50 && prof.dominantType() == NT_FLOAT) {
                    profile_hot = true;
                }
            }
            if (profile_hot) {
                // Inline check: NaN-boxed float: bits > DOUBLE_ENCODE_OFFSET && bits < TAG_INT48
                llvm::Value* above_offset = builder->CreateICmpUGT(boxed,
                    llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET));
                llvm::Value* below_int = builder->CreateICmpULT(boxed,
                    llvm::ConstantInt::get(i64_type, TAG_INT48));
                guard_pass = builder->CreateAnd(above_offset, below_int, "guard_float_inline");
            } else {
                auto helper = module.getOrInsertFunction("qore_rt_guard_float",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                llvm::Value* guard_result = builder->CreateCall(helper, {boxed});
                guard_pass = builder->CreateICmpNE(guard_result,
                        llvm::ConstantInt::get(i64_type, 0));
            }
            if (ginst->deopt_target) {
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                llvm::BasicBlock* deopt = getOrCreateJitDeoptBlock(module, llvm_func);
                if (profile_hot) {
                    auto* weights = llvm::MDBuilder(ctx).createBranchWeights(999, 1);
                    builder->CreateCondBr(guard_pass, cont, deopt, weights);
                } else {
                    builder->CreateCondBr(guard_pass, cont, deopt);
                }
                builder->SetInsertPoint(cont);
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = guard_pass;
            }
            return true;
        }

        // === Control flow ===
        case QoreIROpcode::Br: {
            const auto* br = static_cast<const QoreIRBranchInstruction*>(inst);
            auto it = block_map.find(br->target);
            if (it == block_map.end()) {
                error = "branch target block not found";
                return false;
            }
            builder->CreateBr(it->second);
            return true;
        }
        case QoreIROpcode::BrIf: {
            const auto* br = static_cast<const QoreIRBranchIfInstruction*>(inst);
            auto* cond_val = getVal(br->condition.id, error);
            if (!cond_val) { return false; }
            auto t_it = block_map.find(br->true_target);
            auto f_it = block_map.find(br->false_target);
            if (t_it == block_map.end() || f_it == block_map.end()) {
                error = "branch target block not found";
                return false;
            }
            // Convert to i1 if not already
            llvm::Value* cond;
            if (cond_val->getType() == i1_type) {
                printd(3, "BranchIf: condition %%%d is i1\n", br->condition.id);
                cond = cond_val;
            } else if (cond_val->getType() == i64_type
                    && nanboxed_values.count(br->condition.id)) {
                // NaN-boxed value: inline fast-paths for all value types,
                // only fall back to qore_rt_to_bool for pointer/node types
                // (strings, hashes, lists, objects).
                //
                // Dispatch by tag (top 16 bits of NaN-boxed value):
                //   0x0000 (NOTHING)  -> false
                //   0xFFF9 (INT48)    -> payload != 0
                //   0xFFFB (SPECIAL)  -> val == VAL_TRUE
                //   < 0xFFF9 (DOUBLE) -> decode to double, != 0.0
                //   0xFFFA (POINTER)  -> qore_rt_to_bool (slow path)
                printd(3, "BranchIf: condition %%%d is NaN-boxed i64 -> inline bool\n",
                    br->condition.id);

                llvm::BasicBlock* bb_true = llvm::BasicBlock::Create(ctx, "brif.true", llvm_func);
                llvm::BasicBlock* bb_false = llvm::BasicBlock::Create(ctx, "brif.false", llvm_func);
                llvm::BasicBlock* bb_chk_tag = llvm::BasicBlock::Create(ctx, "brif.chk_tag", llvm_func);
                llvm::BasicBlock* bb_int48 = llvm::BasicBlock::Create(ctx, "brif.int48", llvm_func);
                llvm::BasicBlock* bb_special = llvm::BasicBlock::Create(ctx, "brif.special", llvm_func);
                llvm::BasicBlock* bb_double = llvm::BasicBlock::Create(ctx, "brif.double", llvm_func);
                llvm::BasicBlock* bb_slow = llvm::BasicBlock::Create(ctx, "brif.slow", llvm_func);
                llvm::BasicBlock* bb_merge = llvm::BasicBlock::Create(ctx, "brif.merge", llvm_func);

                // NOTHING (0) is the most common falsy value -> check first
                llvm::Value* is_nothing = builder->CreateICmpEQ(cond_val,
                        llvm::ConstantInt::get(i64_type, VAL_NOTHING));
                builder->CreateCondBr(is_nothing, bb_false, bb_chk_tag);

                // Extract tag (top 16 bits) and dispatch
                builder->SetInsertPoint(bb_chk_tag);
                llvm::Value* tag = builder->CreateLShr(cond_val,
                        llvm::ConstantInt::get(i64_type, 48));
                llvm::SwitchInst* sw = builder->CreateSwitch(tag, bb_double, 3);
                sw->addCase(llvm::ConstantInt::get(
                        static_cast<llvm::IntegerType*>(i64_type), 0xFFF9), bb_int48);
                sw->addCase(llvm::ConstantInt::get(
                        static_cast<llvm::IntegerType*>(i64_type), 0xFFFB), bb_special);
                sw->addCase(llvm::ConstantInt::get(
                        static_cast<llvm::IntegerType*>(i64_type), 0xFFFA), bb_slow);

                // INT48: truthy if 48-bit payload != 0
                builder->SetInsertPoint(bb_int48);
                llvm::Value* payload = builder->CreateAnd(cond_val,
                        llvm::ConstantInt::get(i64_type, PAYLOAD_MASK));
                llvm::Value* int_truthy = builder->CreateICmpNE(payload,
                        llvm::ConstantInt::get(i64_type, 0));
                builder->CreateCondBr(int_truthy, bb_true, bb_false);

                // SPECIAL (0xFFFB): TRUE/FALSE/NULL - only VAL_TRUE is truthy
                builder->SetInsertPoint(bb_special);
                llvm::Value* is_true = builder->CreateICmpEQ(cond_val,
                        llvm::ConstantInt::get(i64_type, VAL_TRUE));
                builder->CreateCondBr(is_true, bb_true, bb_false);

                // DOUBLE: subtract offset, bitcast to double, compare != 0.0
                builder->SetInsertPoint(bb_double);
                llvm::Value* raw_bits = builder->CreateSub(cond_val,
                        llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET));
                llvm::Value* dval = builder->CreateBitCast(raw_bits, double_type);
                llvm::Value* dbl_truthy = builder->CreateFCmpONE(dval,
                        llvm::ConstantFP::get(double_type, 0.0));
                builder->CreateCondBr(dbl_truthy, bb_true, bb_false);

                // Slow path: pointer/node -> call qore_rt_to_bool
                builder->SetInsertPoint(bb_slow);
                auto helper = module.getOrInsertFunction("qore_rt_to_bool",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                llvm::Value* bool_val = builder->CreateCall(helper, {cond_val});
                llvm::Value* slow_result = builder->CreateICmpNE(bool_val,
                        llvm::ConstantInt::get(i64_type, 0));
                builder->CreateCondBr(slow_result, bb_true, bb_false);

                // Merge
                builder->SetInsertPoint(bb_true);
                builder->CreateBr(bb_merge);
                builder->SetInsertPoint(bb_false);
                builder->CreateBr(bb_merge);
                builder->SetInsertPoint(bb_merge);
                llvm::PHINode* phi = builder->CreatePHI(i1_type, 2, "brif.result");
                phi->addIncoming(llvm::ConstantInt::getTrue(ctx), bb_true);
                phi->addIncoming(llvm::ConstantInt::getFalse(ctx), bb_false);
                cond = phi;
            } else if (cond_val->getType() == i64_type) {
                printd(3, "BranchIf: condition %%%d is raw i64 -> compare against 0\n", br->condition.id);
                cond = builder->CreateICmpNE(cond_val, llvm::ConstantInt::get(i64_type, 0));
            } else if (cond_val->getType() == double_type) {
                cond = builder->CreateFCmpONE(cond_val, llvm::ConstantFP::get(double_type, 0.0));
            } else {
                error = "unsupported type for BrIf condition";
                return false;
            }
            builder->CreateCondBr(cond, t_it->second, f_it->second);
            return true;
        }
        case QoreIROpcode::AddAssignLocalInt: {
            const auto* fused = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
            auto target_key = reinterpret_cast<const void*>(fused->target);
            auto source_key = reinterpret_cast<const void*>(fused->source);
            auto target_it = local_allocas.find(target_key);
            auto source_it = local_allocas.find(source_key);
            if (target_it == local_allocas.end() || source_it == local_allocas.end()) {
                error = "AddAssignLocalInt: local alloca not found";
                return false;
            }
            bool target_native = native_int_locals.count(target_key) > 0;
            bool source_native = native_int_locals.count(source_key) > 0;
            // Load and unbox values to native int
            llvm::Value* target_raw = builder->CreateLoad(i64_type, target_it->second, "add.target");
            llvm::Value* source_raw = builder->CreateLoad(i64_type, source_it->second, "add.source");
            llvm::Value* target_int = target_raw;
            llvm::Value* source_int = source_raw;
            if (!target_native) {
                auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                target_int = builder->CreateCall(to_int, {target_raw});
            }
            if (!source_native) {
                auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                source_int = builder->CreateCall(to_int, {source_raw});
            }
            llvm::Value* result = builder->CreateAdd(target_int, source_int, "add.result");
            // Store back: box if non-native, store raw if native
            if (target_native) {
                builder->CreateStore(result, target_it->second);
            } else {
                llvm::Value* boxed = boxIntInline(result);
                builder->CreateStore(boxed, target_it->second);
                // Sync to runtime stack for non-ir-only locals
                bool is_ir_only = ir_only_locals_set && ir_only_locals_set->count(target_key);
                if (!is_ir_only) {
                    if (aot_mode) {
                        auto assign_fn = module.getOrInsertFunction("qore_rt_assign_local_aot",
                                llvm::FunctionType::get(void_type,
                                        {ptr_type, i32_type, i64_type, ptr_type}, false));
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(target_key);
                        builder->CreateCall(assign_fn, {aot_ctx_arg,
                                llvm::ConstantInt::get(i32_type, slot), boxed, xsink_arg});
                    } else {
                        auto assign_fn = module.getOrInsertFunction("qore_rt_assign_local",
                                llvm::FunctionType::get(void_type,
                                        {ptr_type, i64_type, ptr_type}, false));
                        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(fused->target));
                        llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                        builder->CreateCall(assign_fn, {var_as_ptr, boxed, xsink_arg});
                    }
                }
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = result;
                // NOT nanboxed — native int result
            }
            return true;
        }
        case QoreIROpcode::IncrementLocalInt: {
            const auto* fused = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst);
            auto key = reinterpret_cast<const void*>(fused->local);
            auto it = local_allocas.find(key);
            if (it == local_allocas.end()) {
                error = "IncrementLocalInt: local alloca not found";
                return false;
            }
            bool is_native = native_int_locals.count(key) > 0;
            llvm::Value* local_raw = builder->CreateLoad(i64_type, it->second, "inc.val");
            llvm::Value* local_int = local_raw;
            if (!is_native) {
                auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                local_int = builder->CreateCall(to_int, {local_raw});
            }
            llvm::Value* result = builder->CreateAdd(local_int,
                    llvm::ConstantInt::get(i64_type, fused->delta), "inc.result");
            if (is_native) {
                builder->CreateStore(result, it->second);
            } else {
                llvm::Value* boxed = boxIntInline(result);
                builder->CreateStore(boxed, it->second);
                bool is_ir_only = ir_only_locals_set && ir_only_locals_set->count(key);
                if (!is_ir_only) {
                    if (aot_mode) {
                        auto assign_fn = module.getOrInsertFunction("qore_rt_assign_local_aot",
                                llvm::FunctionType::get(void_type,
                                        {ptr_type, i32_type, i64_type, ptr_type}, false));
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                        builder->CreateCall(assign_fn, {aot_ctx_arg,
                                llvm::ConstantInt::get(i32_type, slot), boxed, xsink_arg});
                    } else {
                        auto assign_fn = module.getOrInsertFunction("qore_rt_assign_local",
                                llvm::FunctionType::get(void_type,
                                        {ptr_type, i64_type, ptr_type}, false));
                        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(fused->local));
                        llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                        builder->CreateCall(assign_fn, {var_as_ptr, boxed, xsink_arg});
                    }
                }
            }
            return true;
        }
        case QoreIROpcode::BranchIfLtLocalInt: {
            const auto* fused = static_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst);
            auto t_it = block_map.find(fused->true_target);
            auto f_it = block_map.find(fused->false_target);
            if (t_it == block_map.end() || f_it == block_map.end()) {
                error = "branch target block not found";
                return false;
            }
            auto lhs_key = reinterpret_cast<const void*>(fused->lhs);
            auto rhs_key = reinterpret_cast<const void*>(fused->rhs);
            auto lhs_it = local_allocas.find(lhs_key);
            auto rhs_it = local_allocas.find(rhs_key);
            if (lhs_it == local_allocas.end() || rhs_it == local_allocas.end()) {
                error = "BranchIfLtLocalInt: local alloca not found";
                return false;
            }
            bool lhs_native = native_int_locals.count(lhs_key) > 0;
            bool rhs_native = native_int_locals.count(rhs_key) > 0;
            llvm::Value* lhs_raw = builder->CreateLoad(i64_type, lhs_it->second, "lt.lhs");
            llvm::Value* rhs_raw = builder->CreateLoad(i64_type, rhs_it->second, "lt.rhs");
            llvm::Value* lhs_int = lhs_raw;
            llvm::Value* rhs_int = rhs_raw;
            if (!lhs_native) {
                auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                lhs_int = builder->CreateCall(to_int, {lhs_raw});
            }
            if (!rhs_native) {
                auto to_int = module.getOrInsertFunction("qore_rt_to_int",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                rhs_int = builder->CreateCall(to_int, {rhs_raw});
            }
            llvm::Value* cmp = builder->CreateICmpSLT(lhs_int, rhs_int, "lt.cmp");
            builder->CreateCondBr(cmp, t_it->second, f_it->second);
            return true;
        }
        case QoreIROpcode::SwitchInt: {
            const auto* sw = static_cast<const QoreIRSwitchIntInstruction*>(inst);
            auto* switch_val = getVal(sw->switch_val.id, error);
            if (!switch_val) { return false; }

            // Get default block
            auto def_it = block_map.find(sw->default_target);
            if (def_it == block_map.end()) {
                error = "switch default target block not found";
                return false;
            }

            // Unbox if needed to get raw i64
            llvm::Value* val_i64;
            if (switch_val->getType() == i64_type && nanboxed_values.count(sw->switch_val.id)) {
                val_i64 = ensureIntType(switch_val, sw->switch_val.id);
            } else if (switch_val->getType() == i64_type) {
                val_i64 = switch_val;
            } else {
                error = "SwitchInt requires i64 value";
                return false;
            }

            // Create LLVM switch instruction
            llvm::SwitchInst* llvm_switch = builder->CreateSwitch(val_i64, def_it->second,
                    static_cast<unsigned>(sw->cases.size()));

            // Add cases
            for (const auto& c : sw->cases) {
                auto case_it = block_map.find(c.target);
                if (case_it == block_map.end()) {
                    error = "switch case target block not found";
                    return false;
                }
                llvm_switch->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i64_type), c.value),
                        case_it->second);
            }
            return true;
        }
        case QoreIROpcode::SwitchString: {
            const auto* sw = static_cast<const QoreIRSwitchStringInstruction*>(inst);
            auto* switch_val = getVal(sw->switch_val.id, error);
            if (!switch_val) { return false; }

            // Get default block
            auto def_it = block_map.find(sw->default_target);
            if (def_it == block_map.end()) {
                error = "switch default target block not found";
                return false;
            }

            // Box the value if needed
            llvm::Value* val_boxed = boxValue(switch_val, sw->switch_val.id);

            // Create global array of case strings
            std::vector<llvm::Constant*> case_str_ptrs;
            for (const auto& c : sw->cases) {
                // Create global string constant
                llvm::Constant* str_const = builder->CreateGlobalString(c.value);
                case_str_ptrs.push_back(str_const);
            }

            // Create array type and global array
            llvm::ArrayType* arr_type = llvm::ArrayType::get(ptr_type,
                    static_cast<uint64_t>(sw->cases.size()));
            llvm::Constant* arr_init = llvm::ConstantArray::get(arr_type, case_str_ptrs);
            llvm::GlobalVariable* case_arr = new llvm::GlobalVariable(module, arr_type, true,
                    llvm::GlobalValue::PrivateLinkage, arr_init, "switch_string_cases");

            // Get pointer to first element
            llvm::Value* arr_ptr = builder->CreateBitCast(case_arr, ptr_type);

            // Call runtime helper: int32_t qore_rt_switch_string_lookup(uint64_t, const char**, int32_t)
            auto helper = module.getOrInsertFunction("qore_rt_switch_string_lookup",
                    llvm::FunctionType::get(i32_type,
                            {i64_type, ptr_type, i32_type}, false));
            llvm::Value* case_idx = builder->CreateCall(helper,
                    {val_boxed, arr_ptr,
                     llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i32_type),
                             static_cast<int32_t>(sw->cases.size()))});

            // Create LLVM switch on the case index
            llvm::SwitchInst* llvm_switch = builder->CreateSwitch(case_idx, def_it->second,
                    static_cast<unsigned>(sw->cases.size()));

            // Add cases (index 0, 1, 2, ...)
            for (size_t i = 0; i < sw->cases.size(); ++i) {
                auto case_it = block_map.find(sw->cases[i].target);
                if (case_it == block_map.end()) {
                    error = "switch case target block not found";
                    return false;
                }
                llvm_switch->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i32_type),
                        static_cast<int32_t>(i)), case_it->second);
            }
            return true;
        }
        case QoreIROpcode::Return: {
            const auto* ret = static_cast<const QoreIRReturnInstruction*>(inst);
            // Box the return value BEFORE cleanup so we can incref it.  Cleanup
            // (invoke cleanup, local uninstantiation) may deref values that the
            // return value references — e.g. a CatchException result stored into
            // a local.  We must take our own reference first, mirroring the IR
            // interpreter's val.refSelf() in its Return handler.
            llvm::Value* boxed_ret = nullptr;
            if (ret->has_value) {
                auto* val = getVal(ret->value.id, error);
                if (!val) { return false; }
                if (nanboxed_values.count(ret->value.id)) {
                    boxed_ret = val;
                } else if (val->getType() == i64_type) {
                    boxed_ret = boxIntInline(val);
                } else if (val->getType() == double_type) {
                    boxed_ret = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed_ret = boxBool(val);
                } else {
                    error = "unsupported return value type for LLVM lowering";
                    return false;
                }
                // Take a reference to the return value before cleanup.
                // emitInvokeCleanup will deref the invoke alloca (if any),
                // balancing this incref. Net refcount change = 0 (correct).
                auto incref_fn = module.getOrInsertFunction("qore_rt_incref",
                        llvm::FunctionType::get(void_type, {i64_type}, false));
                builder->CreateCall(incref_fn, {boxed_ret});
            }
            // Execute on_block_exit handlers before cleanup
            emitOnBlockExitExec(module);
            // Delete active iterators (from foreach body early exit)
            emitIteratorCleanup(module);
            // Release entry-load refs for pre-instantiated locals (tiered compilation)
            emitPreinstantiatedCleanup(module);
            // Release Invoke/ConstString result refs
            emitInvokeCleanup(module);
            // Uninstantiate locals before returning (pre-instantiated locals are skipped internally)
            emitLocalUninstantiation(module);
            if (boxed_ret) {
                builder->CreateRet(boxed_ret);
            } else {
                builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            }
            return true;
        }
        case QoreIROpcode::ReturnNothing: {
            emitOnBlockExitExec(module);
            emitIteratorCleanup(module);
            emitPreinstantiatedCleanup(module);
            emitInvokeCleanup(module);
            emitLocalUninstantiation(module);
            builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            return true;
        }

        // === String constants ===
        case QoreIROpcode::ConstString: {
            const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
            // Create a global constant string and call qore_rt_make_string to produce a QoreStringNode
            llvm::Constant* str_const = builder->CreateGlobalString(cinst->constant.string_value);
            auto helper = module.getOrInsertFunction("qore_rt_make_string",
                    llvm::FunctionType::get(i64_type, {ptr_type}, false));
            llvm::Value* str_result = builder->CreateCall(helper, {str_const});
            values[inst->result.id] = str_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(str_result, inst->result.id, llvm_func);
            return true;
        }

        // === Invoke (expression call with exception edges) ===
        case QoreIROpcode::Invoke: {
            const auto* inv = static_cast<const QoreIRInvokeInstruction*>(inst);
            llvm::Value* result;

            // Dispatch based on invoke_opcode and operand availability to avoid
            // double-evaluating pre-evaluated operands via qore_rt_invoke_expr
            if (!inv->operands.empty() && isUnaryInvokeOpcode(inv->invoke_opcode)
                    && inv->operands.size() >= 1) {
                // Unary invoke: use pre-evaluated operand with qore_rt_unary_op
                auto* val = getVal(inv->operands[0].id, error);
                if (!val) { return false; }
                llvm::Value* val_boxed = boxValue(val, inv->operands[0].id);
                llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                        static_cast<int>(inv->invoke_opcode));
                auto helper = module.getOrInsertFunction("qore_rt_unary_op",
                        llvm::FunctionType::get(i64_type,
                            {llvm::Type::getInt32Ty(ctx), i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {opcode_val, val_boxed, xsink_arg});
                // No reloadAllLocalsFromRuntime — pure computation

            } else if (!inv->operands.empty() && isBinaryInvokeOpcode(inv->invoke_opcode)
                    && inv->operands.size() >= 2) {
                // Binary invoke: use pre-evaluated operands with qore_rt_binary_op
                auto* lhs = getVal(inv->operands[0].id, error);
                auto* rhs = getVal(inv->operands[1].id, error);
                if (!lhs || !rhs) { return false; }
                llvm::Value* lhs_boxed = boxValue(lhs, inv->operands[0].id);
                llvm::Value* rhs_boxed = boxValue(rhs, inv->operands[1].id);
                llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                        static_cast<int>(inv->invoke_opcode));
                auto helper = module.getOrInsertFunction("qore_rt_binary_op",
                        llvm::FunctionType::get(i64_type,
                            {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {opcode_val, lhs_boxed, rhs_boxed, xsink_arg});
                // No reloadAllLocalsFromRuntime — pure computation

            } else if (!inv->operands.empty() && isDotEvalInvokeOpcode(inv->invoke_opcode)) {
                // Try specialized hash key access first (DotEvalHash/DotEvalAny in try/catch)
                if ((inv->invoke_opcode == QoreIROpcode::DotEvalHash
                        || inv->invoke_opcode == QoreIROpcode::DotEvalAny)
                        && tryEmitHashKeyAccess(inst, module, llvm_func)) {
                    result = values[inst->result.id];
                    // Hash key access is a simple lookup, no local reload needed
                } else {
                    // DotEval invoke: use pre-evaluated base with qore_rt_dot_eval_with_base
                    auto* base = getVal(inv->operands[0].id, error);
                    if (!base) { return false; }
                    llvm::Value* base_boxed = boxValue(base, inv->operands[0].id);

                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));

                    if (aot_mode) {
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                        auto helper = module.getOrInsertFunction("qore_rt_dot_eval_with_base_aot",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, i32_type, i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {aot_ctx_arg,
                                llvm::ConstantInt::get(i32_type, slot), base_boxed, xsink_arg});
                    } else {
                        llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                        auto helper = module.getOrInsertFunction("qore_rt_dot_eval_with_base",
                                llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {expr_const, base_boxed, xsink_arg});
                    }
                    // DotEval method calls can modify locals through side effects
                    reloadAllLocalsFromRuntime(module, llvm_func);
                }

            } else if (!inv->operands.empty() && isRegexInvokeOpcode(inv->invoke_opcode)) {
                // Regex invoke: use pre-evaluated operand with qore_rt_regex_op_with_operand
                auto* operand = getVal(inv->operands[0].id, error);
                if (!operand) { return false; }
                llvm::Value* operand_boxed = boxValue(operand, inv->operands[0].id);
                llvm::Value* opcode_val = llvm::ConstantInt::get(i32_type,
                        static_cast<int>(inv->invoke_opcode));

                QoreValue expr_val = inv->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));

                if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_regex_op_with_operand_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i32_type, i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg, opcode_val,
                            llvm::ConstantInt::get(i32_type, slot), operand_boxed, xsink_arg});
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_regex_op_with_operand",
                            llvm::FunctionType::get(i64_type,
                                {i32_type, i64_type, i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {opcode_val, expr_const, operand_boxed,
                            xsink_arg});
                }
                // Regex ops don't modify locals — no reload needed

            } else if (!inv->operands.empty() && isCallInvokeOpcode(inv->invoke_opcode)) {
                // Call invoke: build args array from pre-evaluated operands
                int arg_start = (inv->invoke_opcode == QoreIROpcode::CallIndirect) ? 1 : 0;
                int nargs = static_cast<int>(inv->operands.size()) - arg_start;

                // Hoist alloca to entry block to avoid stack overflow in loops
                llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                        llvm_func->getEntryBlock().begin());
                llvm::Value* args_array = ab.CreateAlloca(i64_type,
                        llvm::ConstantInt::get(i32_type, nargs));
                std::vector<llvm::Value*> boxed_args;
                for (int i = 0; i < nargs; ++i) {
                    auto* arg_val = getVal(inv->operands[arg_start + i].id, error);
                    if (!arg_val) { return false; }
                    llvm::Value* arg_boxed = boxValue(arg_val,
                            inv->operands[arg_start + i].id);
                    boxed_args.push_back(arg_boxed);
                    llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                            llvm::ConstantInt::get(i32_type, i));
                    builder->CreateStore(arg_boxed, gep);
                }

                QoreValue expr_val = inv->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));

                if (inv->invoke_opcode == QoreIROpcode::CallDirect && !aot_mode) {
                    // CallDirect: extract function/variant/pgm from the FunctionCallNode
                    const auto* call = dynamic_cast<const FunctionCallNode*>(
                            inv->expr.getInternalNode());
                    assert(call);
                    const QoreFunction* func = call->getFunction();
                    assert(func);

                    // Check if callee is in the batch module
                    if (batch_callees && call->getVariant()
                            && batch_callees->count(call->getVariant())) {
                        const auto& callee_info = batch_callees->at(call->getVariant());
                        if (callee_info.approach_b_eligible) {
                            // Approach B: direct LLVM call to fast entry function
                            llvm::Function* fast_fn = module.getFunction(
                                    callee_info.fast_name);
                            assert(fast_fn && "Approach B fast entry must be in module");

                            std::vector<llvm::Value*> call_args;
                            for (unsigned i = 0; i < callee_info.num_params; ++i) {
                                if (i < boxed_args.size()) {
                                    call_args.push_back(boxed_args[i]);
                                } else {
                                    call_args.push_back(llvm::ConstantInt::get(
                                            i64_type, VAL_NOTHING));
                                }
                            }
                            call_args.push_back(xsink_arg);
                            result = builder->CreateCall(fast_fn, call_args);
                        } else {
                            llvm::Function* callee_fn = module.getFunction(callee_info.name);
                            assert(callee_fn
                                    && "batch callee must be forward-declared in module");

                            llvm::Value* variant_ptr = builder->CreateIntToPtr(
                                    llvm::ConstantInt::get(i64_type,
                                        reinterpret_cast<uint64_t>(call->getVariant())),
                                    ptr_type);

                            auto helper = module.getOrInsertFunction(
                                    "qore_rt_call_fast_with_target",
                                    llvm::FunctionType::get(i64_type,
                                        {ptr_type, ptr_type, ptr_type, i32_type, ptr_type},
                                        false));
                            result = builder->CreateCall(helper, {callee_fn, variant_ptr,
                                    args_array, llvm::ConstantInt::get(i32_type, nargs),
                                    xsink_arg});
                        }
                    } else {
                        llvm::Value* func_ptr = builder->CreateIntToPtr(
                                llvm::ConstantInt::get(i64_type,
                                    reinterpret_cast<uint64_t>(func)), ptr_type);
                        llvm::Value* variant_ptr = builder->CreateIntToPtr(
                                llvm::ConstantInt::get(i64_type,
                                    reinterpret_cast<uint64_t>(call->getVariant())), ptr_type);
                        llvm::Value* pgm_ptr = builder->CreateIntToPtr(
                                llvm::ConstantInt::get(i64_type,
                                    reinterpret_cast<uint64_t>(call->getProgram())), ptr_type);

                        // Use fast call if eligible
                        const char* call_name = "qore_rt_call_function_direct";
                        if (call->getVariant()) {
                            const UserVariantBase* uvb = call->getVariant()->getUserVariantBase();
                            if (uvb && uvb->isStaticallyFastCallEligible()) {
                                call_name = "qore_rt_call_fast";
                            }
                        }

                        auto helper = module.getOrInsertFunction(call_name,
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, ptr_type, ptr_type, ptr_type, i32_type, ptr_type},
                                    false));
                        result = builder->CreateCall(helper, {func_ptr, variant_ptr, pgm_ptr,
                                args_array, llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                    }
                } else if (aot_mode && inv->invoke_opcode == QoreIROpcode::CallDirect) {
                    // AOT CallDirect: use fast direct call to resolve and dispatch
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_call_direct_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                } else if (inv->invoke_opcode == QoreIROpcode::CallIndirect && !aot_mode) {
                    // CallIndirect invoke: use fast path — pass pre-evaluated call reference
                    // directly to qore_rt_call_ref_fast() instead of AST expression
                    auto* ref_val = getVal(inv->operands[0].id, error);
                    if (!ref_val) { return false; }
                    llvm::Value* ref_boxed = boxValue(ref_val, inv->operands[0].id);
                    auto helper = module.getOrInsertFunction("qore_rt_call_ref_fast",
                            llvm::FunctionType::get(i64_type,
                                {i64_type, ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {ref_boxed, args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                } else if (inv->invoke_opcode == QoreIROpcode::CallStaticDirect && !aot_mode) {
                    // CallStaticDirect invoke: call static method directly with embedded pointer
                    const auto* static_call = dynamic_cast<const StaticMethodCallNode*>(
                            inv->expr.getInternalNode());
                    assert(static_call);
                    llvm::Value* method_ptr = builder->CreateIntToPtr(
                            llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(static_call->getMethod())), ptr_type);
                    llvm::Value* variant_ptr = builder->CreateIntToPtr(
                            llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(static_call->getVariant())), ptr_type);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_call_static_method_direct",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type, ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {method_ptr, variant_ptr, args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                } else if (aot_mode && inv->invoke_opcode == QoreIROpcode::CallStaticDirect) {
                    // AOT mode: use expression slot with expression deserialization
                    // Get the expression slot for runtime reconstruction via resolveExprSlot
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_call_static_method_direct_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                } else if (inv->invoke_opcode == QoreIROpcode::CallClosureDirect) {
                    // CallClosureDirect invoke: call closure/callref directly
                    // operands[0] = closure value, operands[1..] = args
                    auto* ref = getVal(inv->operands[0].id, error);
                    if (!ref) { return false; }
                    llvm::Value* ref_boxed = boxValue(ref, inv->operands[0].id);

                    // Compute number of arguments from operand count (known at compile time)
                    int closure_nargs = static_cast<int>(inst->operands.size()) - 1;

                    if (closure_nargs == 0) {
                        // Fast path for 0-argument closure calls (no QoreListNode allocation)
                        auto helper = module.getOrInsertFunction("qore_rt_call_closure_0",
                                llvm::FunctionType::get(i64_type,
                                    {i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {ref_boxed, xsink_arg});
                    } else if (closure_nargs == 1) {
                        // Fast path for 1-argument closure calls (stack-allocated QoreListNode)
                        auto* arg_val = getVal(inst->operands[1].id, error);
                        if (!arg_val) { return false; }
                        llvm::Value* arg_boxed = boxValue(arg_val, inst->operands[1].id);
                        auto helper = module.getOrInsertFunction("qore_rt_call_closure_1",
                                llvm::FunctionType::get(i64_type,
                                    {i64_type, i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {ref_boxed, arg_boxed, xsink_arg});
                    } else {
                        // Standard path for >1 arguments
                        llvm::Value* closure_args_array;
                        if (!buildArgsArray(inst, 1, llvm_func, closure_args_array, closure_nargs, error)) {
                            return false;
                        }

                        auto helper = module.getOrInsertFunction("qore_rt_call_closure_fast",
                                llvm::FunctionType::get(i64_type,
                                    {i64_type, ptr_type, i32_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {ref_boxed, closure_args_array,
                                llvm::ConstantInt::get(i32_type, closure_nargs), xsink_arg});
                    }
                } else if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_call_with_args_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_call_with_args",
                            llvm::FunctionType::get(i64_type,
                                {i64_type, ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {expr_const, args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                }

                // Calls can modify locals through side effects
                reloadAllLocalsFromRuntime(module, llvm_func);

            } else if (inv->invoke_opcode == QoreIROpcode::HashKeyAccess) {
                // HashKeyAccess invoke: use key name stored on the invoke instruction
                auto* base = getVal(inst->operands[0].id, error);
                if (!base) {
                    return false;
                }
                llvm::Value* base_boxed = boxValue(base, inst->operands[0].id);
                llvm::Constant* key_const = builder->CreateGlobalString(inv->invoke_key_name,
                        "hash_key");
                auto helper = module.getOrInsertFunction("qore_rt_hash_key_access",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {base_boxed, key_const, xsink_arg});
                // HashKeyAccess doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::LoadSelfMember) {
                // LoadSelfMember invoke: extract member name from the SelfVarrefNode AST
                const auto* self_ref = dynamic_cast<const SelfVarrefNode*>(inv->expr.getInternalNode());
                assert(self_ref);
                llvm::Constant* name_const = builder->CreateGlobalString(self_ref->str,
                        "self_member_name");
                auto helper = module.getOrInsertFunction("qore_rt_load_self_member",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {name_const, xsink_arg});
                // LoadSelfMember doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::NewObject) {
                // NewObject invoke: call constructor directly
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    // Extract class/variant/args from the AST node
                    const QoreClass* qc = nullptr;
                    const AbstractQoreFunctionVariant* variant = nullptr;
                    const QoreListNode* args = nullptr;
                    if (auto* new_obj = dynamic_cast<const NewObjectCallNode*>(
                            inv->expr.getInternalNode())) {
                        qc = new_obj->getClass();
                        variant = new_obj->getVariant();
                        args = new_obj->getArgs();
                    } else if (auto* scoped_obj = dynamic_cast<const ScopedObjectCallNode*>(
                            inv->expr.getInternalNode())) {
                        qc = scoped_obj->oc;
                        variant = scoped_obj->getVariant();
                        args = scoped_obj->getArgs();
                    } else if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(
                            inv->expr.getInternalNode())) {
                        qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
                        variant = vrn->getVariant();
                        args = vrn->getArgs();
                    }
                    assert(qc);
                    llvm::Value* qc_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(qc));
                    llvm::Value* qc_as_ptr = builder->CreateIntToPtr(qc_ptr, ptr_type);
                    llvm::Value* variant_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(variant));
                    llvm::Value* variant_as_ptr = builder->CreateIntToPtr(variant_ptr, ptr_type);
                    llvm::Value* args_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(args));
                    llvm::Value* args_as_ptr = builder->CreateIntToPtr(args_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_new_object",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type, ptr_type, ptr_type}, false));
                    result = builder->CreateCall(helper,
                            {qc_as_ptr, variant_as_ptr, args_as_ptr, xsink_arg});
                }
                // Constructor can modify locals through side effects
                reloadAllLocalsFromRuntime(module, llvm_func);

            } else if (inv->invoke_opcode == QoreIROpcode::LoadStaticVar) {
                // LoadStaticVar invoke: extract QoreVarInfo* from the StaticClassVarRefNode AST
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    const auto* static_var = dynamic_cast<const StaticClassVarRefNode*>(
                            inv->expr.getInternalNode());
                    assert(static_var);
                    llvm::Value* vi_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(&static_var->vi));
                    llvm::Value* vi_as_ptr = builder->CreateIntToPtr(vi_ptr, ptr_type);
                    llvm::Constant* name_const = builder->CreateGlobalString(static_var->str,
                            "static_var_name");
                    auto helper = module.getOrInsertFunction("qore_rt_load_static_var",
                            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {vi_as_ptr, name_const, xsink_arg});
                }
                // LoadStaticVar doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::LoadConstant) {
                // LoadConstant invoke: load constant value
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    const auto* rt_const = dynamic_cast<const RuntimeConstantRefNode*>(
                            inv->expr.getInternalNode());
                    assert(rt_const);
                    llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(rt_const));
                    llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_load_constant",
                            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {node_as_ptr, xsink_arg});
                }
                // LoadConstant doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::CreateClosure) {
                // CreateClosure invoke: create closure/lambda
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    const auto* closure = dynamic_cast<const QoreClosureParseNode*>(
                            inv->expr.getInternalNode());
                    assert(closure);
                    llvm::Value* cn_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(closure));
                    llvm::Value* cn_as_ptr = builder->CreateIntToPtr(cn_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_create_closure",
                            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {cn_as_ptr, xsink_arg});
                }
                // CreateClosure doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::CreateCallRef) {
                // CreateCallRef invoke
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    QoreValue expr_val = inv->expr;
                    uint64_t bits;
                    std::memcpy(&bits, &expr_val, sizeof(bits));
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, bits);
                    auto helper = module.getOrInsertFunction("qore_rt_create_call_ref",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {expr_const, xsink_arg});
                }
                // CreateCallRef doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::CreateMethodRef) {
                // CreateMethodRef invoke
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    QoreValue expr_val = inv->expr;
                    uint64_t bits;
                    std::memcpy(&bits, &expr_val, sizeof(bits));
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, bits);
                    auto helper = module.getOrInsertFunction("qore_rt_create_method_ref",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {expr_const, xsink_arg});
                }
                // CreateMethodRef doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::CreateParseRef) {
                // CreateParseRef invoke
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    const auto* parse_ref = dynamic_cast<const ParseReferenceNode*>(
                            inv->expr.getInternalNode());
                    assert(parse_ref);
                    llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(parse_ref));
                    llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_create_parse_ref",
                            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {node_as_ptr, xsink_arg});
                }
                // CreateParseRef may access locals via lvalue resolution
                reloadAllLocalsFromRuntime(module, llvm_func);

            } else if (inv->invoke_opcode == QoreIROpcode::NewHashDecl
                    || inv->invoke_opcode == QoreIROpcode::NewComplexHash
                    || inv->invoke_opcode == QoreIROpcode::NewComplexList) {
                // Typed container construction invoke
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    QoreValue expr_val = inv->expr;
                    uint64_t bits;
                    std::memcpy(&bits, &expr_val, sizeof(bits));
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {expr_const, xsink_arg});
                }
                // Typed container construction doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::VrnConstruct) {
                // VarRefNewObjectNode construction invoke (non-object types)
                // Use dedicated AOT helper to avoid double-assign (eval() assigns + StoreLocal assigns)
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_vrn_construct_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(
                            inv->expr.getInternalNode());
                    assert(vrn);
                    llvm::Value* vrn_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(vrn));
                    llvm::Value* vrn_as_ptr = builder->CreateIntToPtr(vrn_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_vrn_construct",
                            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {vrn_as_ptr, xsink_arg});
                }
                // VrnConstruct doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::ListPush) {
                // ListPush invoke: native list push with pre-evaluated operands
                auto* list = getVal(inv->operands[0].id, error);
                if (!list) { return false; }
                auto* val = getVal(inv->operands[1].id, error);
                if (!val) { return false; }
                llvm::Value* list_boxed = boxValue(list, inv->operands[0].id);
                llvm::Value* val_boxed = boxValue(val, inv->operands[1].id);
                auto push_fn = module.getOrInsertFunction("qore_rt_list_push",
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(push_fn, {list_boxed, val_boxed, xsink_arg});
                // ListPush doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::CastList
                    || inv->invoke_opcode == QoreIROpcode::CastHash
                    || inv->invoke_opcode == QoreIROpcode::CastObject
                    || inv->invoke_opcode == QoreIROpcode::CastEnum
                    || inv->invoke_opcode == QoreIROpcode::CastAny) {
                // Cast invoke: native cast with pre-evaluated inner value (operand[0])
                auto* inner_val = getVal(inv->operands[0].id, error);
                if (!inner_val) { return false; }
                llvm::Value* inner_boxed = boxValue(inner_val, inv->operands[0].id);
                QoreValue expr_val = inv->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_cast_with_inner_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), inner_boxed, xsink_arg});
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_cast_with_inner",
                            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {expr_const, inner_boxed, xsink_arg});
                }
                // Cast doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::StoreLValue
                    && !inv->operands.empty()) {
                // StoreLValue invoke: use pre-evaluated RHS operand and weak flag.
                // Extract lvalue from the assignment expression (inv->expr).
                auto* assign = dynamic_cast<const QoreAssignmentOperatorNode*>(
                    inv->expr.getInternalNode());
                if (!assign) {
                    error = "StoreLValue invoke: expr is not an assignment operator";
                    return false;
                }
                auto* val = getVal(inv->operands[0].id, error);
                if (!val) { return false; }
                QoreValue lv = assign->getLeft();
                uint64_t lv_bits;
                std::memcpy(&lv_bits, &lv, sizeof(lv_bits));
                llvm::Value* val_boxed = boxValue(val, inv->operands[0].id);
                // Clear reload tracker for lvalue target local
                {
                    const void* local_key = findLvalueRootLocalKey(lv);
                    if (local_key) {
                        clearLocalReloadTracker(local_key, module, llvm_func);
                    }
                }
                if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(lv_bits);
                    const char* fn_name = inv->weak
                        ? "qore_rt_lvalue_store_weak_aot" : "qore_rt_lvalue_store_aot";
                    auto helper = module.getOrInsertFunction(fn_name,
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), val_boxed, xsink_arg});
                } else {
                    llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                    const char* fn_name = inv->weak
                        ? "qore_rt_lvalue_store_weak" : "qore_rt_lvalue_store";
                    auto helper = module.getOrInsertFunction(fn_name,
                            llvm::FunctionType::get(i64_type,
                                {i64_type, i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {lv_const, val_boxed, xsink_arg});
                }
                // Reload the lvalue target local from runtime
                {
                    const void* local_key = findLvalueRootLocalKey(lv);
                    if (local_key) {
                        reloadLocalFromRuntime(local_key, module, llvm_func);
                    }
                }

            } else if (inv->invoke_opcode == QoreIROpcode::RefForeachInit) {
                // RefForeachInit invoke: initialize reference foreach state
                llvm::Value* parse_ref_bits_val;
                if (aot_mode) {
                    // AOT: use expression slot for the ParseReferenceNode
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto aot_helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    llvm::Value* ref_val = builder->CreateCall(aot_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                    parse_ref_bits_val = ref_val;
                } else {
                    // JIT: pass the ParseReferenceNode QoreValue bits directly
                    QoreValue expr_val = inv->expr;
                    uint64_t bits;
                    std::memcpy(&bits, &expr_val, sizeof(bits));
                    parse_ref_bits_val = llvm::ConstantInt::get(i64_type, bits);
                }
                auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_init",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {parse_ref_bits_val, xsink_arg});
                // State handle is not nanboxed — handled specially below

            } else if (inv->invoke_opcode == QoreIROpcode::RefForeachGetEntry) {
                // RefForeachGetEntry invoke: get element at index from reference foreach state
                // operands: state, index
                auto* state_val = getVal(inv->operands[0].id, error);
                if (!state_val) { return false; }
                auto* index_val = getVal(inv->operands[1].id, error);
                if (!index_val) { return false; }
                auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_get_entry",
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {state_val, index_val, xsink_arg});
                // Result is nanboxed — handled by common tail below

            } else {
                // Fallback: evaluate the full AST expression via qore_rt_invoke_expr
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {expr_const, xsink_arg});
                }
                // Reload after fallback — AST eval can modify any local
                reloadAllLocalsFromRuntime(module, llvm_func);
            }

            values[inst->result.id] = result;
            // RefForeachInit returns an opaque state handle — not a nanboxed QoreValue
            if (inv->invoke_opcode != QoreIROpcode::RefForeachInit) {
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(result, inst->result.id, llvm_func);
            }

            // Check for exception and branch accordingly
            auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
                    llvm::FunctionType::get(i64_type, {ptr_type}, false));
            llvm::Value* ex_check = builder->CreateCall(has_ex, {xsink_arg});
            llvm::Value* has_exception = builder->CreateICmpNE(ex_check,
                    llvm::ConstantInt::get(i64_type, 0));

            // Find normal and exception target blocks
            auto normal_it = block_map.find(inv->normal_target);
            auto except_it = block_map.find(inv->exception_target);
            if (normal_it == block_map.end()) {
                error = "invoke normal target block not found";
                return false;
            }
            if (except_it == block_map.end()) {
                error = "invoke exception target block not found";
                return false;
            }
            builder->CreateCondBr(has_exception, except_it->second, normal_it->second);
            return true;
        }

        // === Landing pad: execute on_error/on_exit handlers for scopes within the try body ===
        case QoreIROpcode::LandingPad: {
            const auto* lp_inst = static_cast<const QoreIRLandingPadInstruction*>(inst);
            if (lp_inst->try_scope_id != 0) {
                auto it = scope_obe_counts.find(lp_inst->try_scope_id);
                if (it != scope_obe_counts.end()) {
                    llvm::Value* saved_count = it->second;
                    auto helper = module.getOrInsertFunction("qore_rt_exec_on_block_exit",
                            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                    builder->CreateCall(helper, {saved_count, xsink_arg});
                    // On-block-exit handlers execute through the AST path and can modify
                    // any local variable on the thread-local stack
                    reloadAllLocalsFromRuntime(module, llvm_func);
                }
            }
            return true;
        }

        // === Catch exception ===
        case QoreIROpcode::CatchException: {
            // qore_rt_catch_exception: sets td->catchException for rethrow support,
            // returns NaN-boxed exception info hash
            auto helper = module.getOrInsertFunction("qore_rt_catch_exception",
                    llvm::FunctionType::get(i64_type, {ptr_type}, false));
            llvm::Value* catch_result = builder->CreateCall(helper, {xsink_arg});
            values[inst->result.id] = catch_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(catch_result, inst->result.id, llvm_func);
            return true;
        }

        // === Catch cleanup ===
        case QoreIROpcode::CatchCleanup: {
            // Restore previous td->catchException and delete the caught exception
            auto helper = module.getOrInsertFunction("qore_rt_catch_end",
                    llvm::FunctionType::get(void_type, {ptr_type}, false));
            builder->CreateCall(helper, {xsink_arg});
            return true;
        }

        // === Throw ===
        case QoreIROpcode::Throw: {
            const auto* throw_inst = static_cast<const QoreIRThrowInstruction*>(inst);
            if (!inst->operands.empty()) {
                // Throw takes a single NaN-boxed operand (typically a list of err, desc[, arg])
                auto* val = getVal(inst->operands[0].id, error);
                if (!val) { return false; }
                llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
                auto helper = module.getOrInsertFunction("qore_rt_throw_value",
                        llvm::FunctionType::get(void_type, {ptr_type, i64_type}, false));
                builder->CreateCall(helper, {xsink_arg, val_boxed});
            }
            // Branch to exception target if present
            if (throw_inst->exception_target) {
                auto it = block_map.find(throw_inst->exception_target);
                if (it != block_map.end()) {
                    builder->CreateBr(it->second);
                    return true;
                }
            }
            // No exception target: execute on_block_exit, uninstantiate locals and return NOTHING
            emitOnBlockExitExec(module);
            emitIteratorCleanup(module);
            emitPreinstantiatedCleanup(module);
            emitInvokeCleanup(module);
            emitLocalUninstantiation(module);
            builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            return true;
        }

        // === Rethrow ===
        case QoreIROpcode::Rethrow: {
            const auto* rethrow_inst = static_cast<const QoreIRThrowInstruction*>(inst);
            if (!rethrow_inst->operands.empty()) {
                // Rethrow with args: replaceTop() + rethrow + catch_end
                llvm::Value* args_val = getVal(rethrow_inst->operands[0].id, error);
                if (!args_val) {
                    return false;
                }
                args_val = boxValue(args_val, rethrow_inst->operands[0].id);
                auto rethrow_args_helper = module.getOrInsertFunction("qore_rt_rethrow_with_args",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                builder->CreateCall(rethrow_args_helper, {args_val, xsink_arg});
            } else {
                // Plain rethrow: copy exception from td->catchException into xsink,
                // clean up catch scope.  qore_rt_rethrow() handles 1 catch scope.
                auto rethrow_helper = module.getOrInsertFunction("qore_rt_rethrow",
                        llvm::FunctionType::get(void_type, {ptr_type}, false));
                builder->CreateCall(rethrow_helper, {xsink_arg});
            }
            // Clean up additional catch scopes beyond the innermost one
            if (rethrow_inst->catch_depth > 1) {
                auto catch_end_helper = module.getOrInsertFunction("qore_rt_catch_end",
                        llvm::FunctionType::get(void_type, {ptr_type}, false));
                for (int i = 1; i < rethrow_inst->catch_depth; ++i) {
                    builder->CreateCall(catch_end_helper, {xsink_arg});
                }
            }
            // Branch to outer exception handler if inside nested try/catch
            if (rethrow_inst->exception_target) {
                auto it = block_map.find(rethrow_inst->exception_target);
                if (it != block_map.end()) {
                    builder->CreateBr(it->second);
                    return true;
                }
            }
            // No outer handler: return from function; caller sees exception in xsink
            emitOnBlockExitExec(module);
            emitIteratorCleanup(module);
            emitPreinstantiatedCleanup(module);
            emitInvokeCleanup(module);
            emitLocalUninstantiation(module);
            builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            return true;
        }

        // === Expression-based operations (delegate to runtime) ===
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic: {
            // These are expression-based ops that need the AST node.
            // They should have been lowered as Invoke instructions for exception safety,
            // but if they appear bare, delegate to the runtime invoke helper.
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            llvm::Value* call_result;

            if (!inst->operands.empty()) {
                int arg_start = (inst->opcode == QoreIROpcode::CallIndirect) ? 1 : 0;
                int nargs = static_cast<int>(inst->operands.size()) - arg_start;

                // Hoist alloca to entry block to avoid stack overflow in loops
                llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                        llvm_func->getEntryBlock().begin());
                llvm::Value* args_array = ab.CreateAlloca(i64_type,
                        llvm::ConstantInt::get(i32_type, nargs));
                for (int i = 0; i < nargs; ++i) {
                    auto* arg_val = getVal(inst->operands[arg_start + i].id, error);
                    if (!arg_val) { return false; }
                    llvm::Value* arg_boxed = boxValue(arg_val,
                            inst->operands[arg_start + i].id);
                    llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                            llvm::ConstantInt::get(i32_type, i));
                    builder->CreateStore(arg_boxed, gep);
                }

                if (inst->opcode == QoreIROpcode::CallIndirect && !aot_mode) {
                    // CallIndirect: use fast path — pass pre-evaluated call reference
                    // directly to qore_rt_call_ref_fast() instead of AST expression
                    auto* ref_val = getVal(inst->operands[0].id, error);
                    if (!ref_val) { return false; }
                    llvm::Value* ref_boxed = boxValue(ref_val, inst->operands[0].id);
                    auto helper = module.getOrInsertFunction("qore_rt_call_ref_fast",
                            llvm::FunctionType::get(i64_type,
                                {i64_type, ptr_type, i32_type, ptr_type}, false));
                    call_result = builder->CreateCall(helper, {ref_boxed, args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                } else if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_call_with_args_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));
                    call_result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_call_with_args",
                            llvm::FunctionType::get(i64_type,
                                {i64_type, ptr_type, i32_type, ptr_type}, false));
                    call_result = builder->CreateCall(helper, {expr_const, args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                }
            } else {
                // No operands: fall back to qore_rt_invoke_expr
                if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    call_result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    call_result = builder->CreateCall(helper, {expr_const, xsink_arg});
                }
            }

            // Qore's scoping allows callees to access the caller's non-IR-only
            // locals through the TLS variable stack, so we must reload after every
            // call.  reloadAllLocalsFromRuntime already skips IR-only locals.
            reloadAllLocalsFromRuntime(module, llvm_func);

            values[inst->result.id] = call_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(call_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === CallDirect (resolved function call, skips AST round-trip) ===
        case QoreIROpcode::CallDirect: {
            const auto* direct_inst = static_cast<const QoreIRCallDirectInstruction*>(inst);

            // Check if this is an Approach B call (direct LLVM arg passing — no args_array needed)
            int nargs = static_cast<int>(inst->operands.size());
            bool is_approach_b = !aot_mode && batch_callees && direct_inst->variant
                    && batch_callees->count(direct_inst->variant)
                    && batch_callees->at(direct_inst->variant).approach_b_eligible;

            // Box args and optionally build args_array (skipped for Approach B)
            llvm::Value* args_array = nullptr;
            std::vector<llvm::Value*> boxed_args;
            if (nargs > 0) {
                for (int i = 0; i < nargs; ++i) {
                    auto* arg_val = getVal(inst->operands[i].id, error);
                    if (!arg_val) { return false; }
                    boxed_args.push_back(boxValue(arg_val, inst->operands[i].id));
                }
                if (!is_approach_b) {
                    llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                            llvm_func->getEntryBlock().begin());
                    args_array = ab.CreateAlloca(i64_type,
                            llvm::ConstantInt::get(i32_type, nargs));
                    for (int i = 0; i < nargs; ++i) {
                        llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                                llvm::ConstantInt::get(i32_type, i));
                        builder->CreateStore(boxed_args[i], gep);
                    }
                }
            }
            if (!args_array) {
                args_array = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, 0), ptr_type);
            }

            llvm::Value* call_result;
            if (aot_mode) {
                // AOT: use fast direct call helper to resolve function from expression slot
                QoreValue expr_val = direct_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_call_direct_aot",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));
                call_result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), args_array,
                        llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            } else if (batch_callees && direct_inst->variant
                    && batch_callees->count(direct_inst->variant)) {
                const auto& callee_info = batch_callees->at(direct_inst->variant);
                if (callee_info.approach_b_eligible) {
                    // Approach B: direct LLVM call to fast entry function.
                    // Args are passed directly as i64 values (NaN-boxed), bypassing
                    // the runtime helper entirely so LLVM can optimize across the call.
                    llvm::Function* fast_fn = module.getFunction(callee_info.fast_name);
                    assert(fast_fn && "Approach B fast entry must be in module");

                    std::vector<llvm::Value*> call_args;
                    for (unsigned i = 0; i < callee_info.num_params; ++i) {
                        if (i < boxed_args.size()) {
                            call_args.push_back(boxed_args[i]);
                        } else {
                            // Pad with VAL_NOTHING for missing args
                            call_args.push_back(
                                    llvm::ConstantInt::get(i64_type, VAL_NOTHING));
                        }
                    }
                    call_args.push_back(xsink_arg);
                    call_result = builder->CreateCall(fast_fn, call_args);
                } else {
                    // Batch compilation: callee is in the same LLVM module.
                    // Call qore_rt_call_fast_with_target() with the in-module function pointer
                    // so the callee's native code is called directly without going through
                    // execCachedFunction() / hasCachedFunction() indirection.
                    llvm::Function* callee_fn = module.getFunction(callee_info.name);
                    assert(callee_fn && "batch callee must be forward-declared in module");

                    llvm::Value* variant_ptr = builder->CreateIntToPtr(
                            llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(direct_inst->variant)), ptr_type);

                    auto helper = module.getOrInsertFunction("qore_rt_call_fast_with_target",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type, ptr_type, i32_type, ptr_type}, false));
                    call_result = builder->CreateCall(helper, {callee_fn, variant_ptr,
                            args_array, llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                }
            } else if (direct_inst->is_self_recursive && direct_inst->variant != nullptr) {
                // Lightweight path for self-recursive calls: skip ProgramThreadCountContextHelper,
                // ArgvContextHelper, and return type coercion overhead
                llvm::Value* variant_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(direct_inst->variant)), ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_call_self_recursive",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, i32_type, ptr_type}, false));
                call_result = builder->CreateCall(helper, {variant_ptr, args_array,
                        llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            } else {
                // JIT: call function directly with embedded pointers
                llvm::Value* func_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(direct_inst->func)), ptr_type);
                llvm::Value* variant_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(direct_inst->variant)), ptr_type);
                llvm::Value* pgm_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(direct_inst->pgm)), ptr_type);

                // Use fast call path if variant is eligible (no default args, not
                // synchronized). qore_rt_call_fast has the same signature as
                // qore_rt_call_function_direct and falls back internally if the
                // callee is not yet JIT-compiled.
                const char* call_name = "qore_rt_call_function_direct";
                if (direct_inst->variant) {
                    const UserVariantBase* uvb = direct_inst->variant->getUserVariantBase();
                    if (uvb && uvb->isStaticallyFastCallEligible()) {
                        call_name = "qore_rt_call_fast";
                    }
                }

                auto helper = module.getOrInsertFunction(call_name,
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, ptr_type, ptr_type, i32_type, ptr_type},
                            false));
                call_result = builder->CreateCall(helper, {func_ptr, variant_ptr, pgm_ptr,
                        args_array, llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            }

            // Qore's scoping allows callees to access the caller's non-IR-only
            // locals through the TLS variable stack, so we must reload after every
            // call.  reloadAllLocalsFromRuntime already skips IR-only locals.
            reloadAllLocalsFromRuntime(module, llvm_func);

            values[inst->result.id] = call_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(call_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === CallMethodDirect (devirtualized method call) ===
        case QoreIROpcode::CallMethodDirect: {
            const auto* direct_inst = static_cast<const QoreIRCallMethodDirectInstruction*>(inst);

            // Build args array from operands
            llvm::Value* args_array;
            int nargs;
            if (!buildArgsArray(inst, 0, llvm_func, args_array, nargs, error)) {
                return false;
            }

            llvm::Value* call_result;
            if (aot_mode && direct_inst->expr) {
                // AOT mode: use expression slot to look up the class and method at runtime
                QoreValue expr_val = direct_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_call_method_direct_aot",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));
                call_result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), args_array,
                        llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            } else {
                // JIT mode: use pointer constants (valid within same process)
                // Pass method pointer directly to runtime helper as a pointer constant
                llvm::Value* method_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(direct_inst->method)),
                        ptr_type);

                // Use fast call path if variant is eligible (no default args, not synchronized)
                if (direct_inst->variant) {
                    const UserVariantBase* uvb = direct_inst->variant->getUserVariantBase();
                    if (uvb && uvb->isStaticallyFastCallEligible()) {
                        llvm::Value* variant_ptr = builder->CreateIntToPtr(
                                llvm::ConstantInt::get(i64_type,
                                    reinterpret_cast<uint64_t>(direct_inst->variant)), ptr_type);
                        auto helper = module.getOrInsertFunction("qore_rt_call_method_fast",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, ptr_type, ptr_type, i32_type, ptr_type}, false));
                        call_result = builder->CreateCall(helper, {method_ptr, variant_ptr, args_array,
                                llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                    } else {
                        auto helper = module.getOrInsertFunction("qore_rt_call_method_direct",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, ptr_type, i32_type, ptr_type}, false));
                        call_result = builder->CreateCall(helper, {method_ptr, args_array,
                                llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                    }
                } else {
                    auto helper = module.getOrInsertFunction("qore_rt_call_method_direct",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type, i32_type, ptr_type}, false));
                    call_result = builder->CreateCall(helper, {method_ptr, args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                }
            }

            // Qore's scoping allows callees to access the caller's non-IR-only
            // locals through the TLS variable stack, so we must reload after every
            // call.  reloadAllLocalsFromRuntime already skips IR-only locals.
            reloadAllLocalsFromRuntime(module, llvm_func);

            values[inst->result.id] = call_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(call_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === InvokeMethodDirect (devirtualized method call with exception routing) ===
        case QoreIROpcode::InvokeMethodDirect: {
            const auto* invoke_inst = static_cast<const QoreIRInvokeMethodDirectInstruction*>(inst);

            // Build args array from operands
            llvm::Value* args_array;
            int nargs;
            if (!buildArgsArray(inst, 0, llvm_func, args_array, nargs, error)) {
                return false;
            }

            llvm::Value* call_result;
            if (aot_mode && invoke_inst->expr) {
                // AOT mode: use expression slot to look up the class and method at runtime
                QoreValue expr_val = invoke_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_call_method_direct_aot",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));
                call_result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), args_array,
                        llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            } else {
                // JIT mode: use pointer constants (valid within same process)
                // Pass method pointer directly to runtime helper as a pointer constant
                llvm::Value* method_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(invoke_inst->method)),
                        ptr_type);

                // Use fast call path if variant is eligible (no default args, not synchronized)
                if (invoke_inst->variant) {
                    const UserVariantBase* uvb = invoke_inst->variant->getUserVariantBase();
                    if (uvb && uvb->isStaticallyFastCallEligible()) {
                        llvm::Value* variant_ptr = builder->CreateIntToPtr(
                                llvm::ConstantInt::get(i64_type,
                                    reinterpret_cast<uint64_t>(invoke_inst->variant)), ptr_type);
                        auto helper = module.getOrInsertFunction("qore_rt_call_method_fast",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, ptr_type, ptr_type, i32_type, ptr_type}, false));
                        call_result = builder->CreateCall(helper, {method_ptr, variant_ptr, args_array,
                                llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                    } else {
                        auto helper = module.getOrInsertFunction("qore_rt_call_method_direct",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, ptr_type, i32_type, ptr_type}, false));
                        call_result = builder->CreateCall(helper, {method_ptr, args_array,
                                llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                    }
                } else {
                    auto helper = module.getOrInsertFunction("qore_rt_call_method_direct",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type, i32_type, ptr_type}, false));
                    call_result = builder->CreateCall(helper, {method_ptr, args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                }
            }

            // Qore's scoping allows callees to access the caller's non-IR-only
            // locals through the TLS variable stack, so we must reload after every
            // call.  reloadAllLocalsFromRuntime already skips IR-only locals.
            reloadAllLocalsFromRuntime(module, llvm_func);

            values[inst->result.id] = call_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(call_result, inst->result.id, llvm_func);

            // Check for exception and branch accordingly (like Invoke)
            auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
                    llvm::FunctionType::get(i64_type, {ptr_type}, false));
            llvm::Value* ex_check = builder->CreateCall(has_ex, {xsink_arg});
            llvm::Value* has_exception = builder->CreateICmpNE(ex_check,
                    llvm::ConstantInt::get(i64_type, 0));

            // Find normal and exception target blocks
            auto normal_it = block_map.find(invoke_inst->normal_target);
            auto except_it = block_map.find(invoke_inst->exception_target);
            if (normal_it == block_map.end()) {
                error = "invoke.method.direct normal target block not found";
                return false;
            }
            if (except_it == block_map.end()) {
                error = "invoke.method.direct exception target block not found";
                return false;
            }
            builder->CreateCondBr(has_exception, except_it->second, normal_it->second);
            return true;
        }

        // === CallStaticDirect (resolved static method call, skips AST round-trip) ===
        case QoreIROpcode::CallStaticDirect: {
            const auto* direct_inst = static_cast<const QoreIRCallStaticDirectInstruction*>(inst);

            // Build args array from operands
            llvm::Value* args_array;
            int nargs;
            if (!buildArgsArray(inst, 0, llvm_func, args_array, nargs, error)) {
                return false;
            }

            llvm::Value* call_result;
            if (aot_mode) {
                // AOT mode: always use expression slot — embedded pointer optimization is only valid
                // in JIT mode (same process). In AOT, compile-time pointers are invalid at runtime.
                QoreValue expr_val = direct_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_call_static_method_direct_aot",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));
                call_result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), args_array,
                        llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            } else {
                llvm::Value* method_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(direct_inst->method)), ptr_type);
                llvm::Value* variant_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(direct_inst->variant)), ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_call_static_method_direct",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, ptr_type, i32_type, ptr_type}, false));
                call_result = builder->CreateCall(helper, {method_ptr, variant_ptr, args_array,
                        llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            }

            // Qore's scoping allows callees to access the caller's non-IR-only
            // locals through the TLS variable stack, so we must reload after every
            // call.  reloadAllLocalsFromRuntime already skips IR-only locals.
            reloadAllLocalsFromRuntime(module, llvm_func);

            values[inst->result.id] = call_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(call_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === DotEvalMethodDirect (resolved dot-eval method call) ===
        case QoreIROpcode::DotEvalMethodDirect: {
            const auto* direct_inst = static_cast<const QoreIRDotEvalMethodDirectInstruction*>(inst);

            // operands[0] = base, operands[1..n-1] = args
            auto* base_val = getVal(inst->operands[0].id, error);
            if (!base_val) { return false; }
            llvm::Value* base_boxed = boxValue(base_val, inst->operands[0].id);

            llvm::Value* args_array;
            int nargs;
            if (!buildArgsArray(inst, 1, llvm_func, args_array, nargs, error)) {
                return false;
            }

            llvm::Value* call_result;
            if (aot_mode) {
                QoreValue expr_val = direct_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                const char* helper_name = direct_inst->pseudo
                        ? "qore_rt_dot_eval_pseudo_method_direct_aot"
                        : "qore_rt_dot_eval_method_direct_aot";
                auto helper = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, i64_type, ptr_type, i32_type, ptr_type}, false));
                call_result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), base_boxed, args_array,
                        llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            } else {
                llvm::Value* method_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(direct_inst->method)), ptr_type);
                llvm::Value* qc_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(direct_inst->qc)), ptr_type);
                llvm::Value* variant_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(direct_inst->variant)), ptr_type);
                const char* helper_name = direct_inst->pseudo
                        ? "qore_rt_dot_eval_pseudo_method_direct"
                        : "qore_rt_dot_eval_method_direct";
                auto helper = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(i64_type,
                            {i64_type, ptr_type, ptr_type, ptr_type, ptr_type, i32_type, ptr_type},
                            false));
                call_result = builder->CreateCall(helper, {base_boxed, method_ptr, qc_ptr,
                        variant_ptr, args_array, llvm::ConstantInt::get(i32_type, nargs),
                        xsink_arg});
            }

            // Qore's scoping allows callees to access the caller's non-IR-only
            // locals through the TLS variable stack, so we must reload after every
            // call.  reloadAllLocalsFromRuntime already skips IR-only locals.
            reloadAllLocalsFromRuntime(module, llvm_func);

            values[inst->result.id] = call_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(call_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === InvokeDotEvalMethodDirect (resolved dot-eval with exception routing) ===
        case QoreIROpcode::InvokeDotEvalMethodDirect: {
            const auto* invoke_inst =
                    static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst);

            // operands[0] = base, operands[1..n-1] = args
            auto* base_val = getVal(inst->operands[0].id, error);
            if (!base_val) { return false; }
            llvm::Value* base_boxed = boxValue(base_val, inst->operands[0].id);

            llvm::Value* args_array;
            int nargs;
            if (!buildArgsArray(inst, 1, llvm_func, args_array, nargs, error)) {
                return false;
            }

            llvm::Value* call_result;
            if (aot_mode) {
                QoreValue expr_val = invoke_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                const char* helper_name = invoke_inst->pseudo
                        ? "qore_rt_dot_eval_pseudo_method_direct_aot"
                        : "qore_rt_dot_eval_method_direct_aot";
                auto helper = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, i64_type, ptr_type, i32_type, ptr_type}, false));
                call_result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), base_boxed, args_array,
                        llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
            } else {
                llvm::Value* method_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(invoke_inst->method)), ptr_type);
                llvm::Value* qc_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(invoke_inst->qc)), ptr_type);
                llvm::Value* variant_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(invoke_inst->variant)), ptr_type);
                const char* helper_name = invoke_inst->pseudo
                        ? "qore_rt_dot_eval_pseudo_method_direct"
                        : "qore_rt_dot_eval_method_direct";
                auto helper = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(i64_type,
                            {i64_type, ptr_type, ptr_type, ptr_type, ptr_type, i32_type, ptr_type},
                            false));
                call_result = builder->CreateCall(helper, {base_boxed, method_ptr, qc_ptr,
                        variant_ptr, args_array, llvm::ConstantInt::get(i32_type, nargs),
                        xsink_arg});
            }

            // Qore's scoping allows callees to access the caller's non-IR-only
            // locals through the TLS variable stack, so we must reload after every
            // call.  reloadAllLocalsFromRuntime already skips IR-only locals.
            reloadAllLocalsFromRuntime(module, llvm_func);

            values[inst->result.id] = call_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(call_result, inst->result.id, llvm_func);

            // Check for exception and branch accordingly
            auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
                    llvm::FunctionType::get(i64_type, {ptr_type}, false));
            llvm::Value* ex_check = builder->CreateCall(has_ex, {xsink_arg});
            llvm::Value* has_exception = builder->CreateICmpNE(ex_check,
                    llvm::ConstantInt::get(i64_type, 0));

            auto normal_it = block_map.find(invoke_inst->normal_target);
            auto except_it = block_map.find(invoke_inst->exception_target);
            if (normal_it == block_map.end()) {
                error = "invoke.dot.eval.method.direct normal target block not found";
                return false;
            }
            if (except_it == block_map.end()) {
                error = "invoke.dot.eval.method.direct exception target block not found";
                return false;
            }
            builder->CreateCondBr(has_exception, except_it->second, normal_it->second);
            return true;
        }

        // === IsNullOrNothing ===
        case QoreIROpcode::IsNullOrNothing: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* boxed = val;
            if (val->getType() != i64_type) {
                if (val->getType() == double_type) {
                    boxed = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed = boxBool(val);
                }
            }
            // NOTHING = 0, NULL = VAL_NULL
            llvm::Value* is_nothing = builder->CreateICmpEQ(boxed, llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            llvm::Value* is_null = builder->CreateICmpEQ(boxed, llvm::ConstantInt::get(i64_type, VAL_NULL));
            values[inst->result.id] = builder->CreateOr(is_nothing, is_null);
            return true;
        }

        // === ConstDate ===
        case QoreIROpcode::ConstDate: {
            const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
            llvm::Value* us_val = llvm::ConstantInt::get(i64_type, cinst->constant.date_microseconds);
            llvm::Value* rel_val = llvm::ConstantInt::get(i64_type,
                    cinst->constant.date_is_relative ? 1 : 0);
            auto helper = module.getOrInsertFunction("qore_rt_make_date",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {us_val, rel_val});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }

        // === ConstEnum ===
        case QoreIROpcode::ConstEnum: {
            const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                QoreValue enum_val = QoreValue::makeEnum(cinst->constant.enum_member);
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &enum_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* member_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(cinst->constant.enum_member));
                auto helper = module.getOrInsertFunction("qore_rt_make_enum",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                result = builder->CreateCall(helper, {member_ptr});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            // TAG_ENUM values are zero-allocation, no cleanup needed
            return true;
        }

        // === Dynamic comparison operations ===
        // Phase 5b: EqAny..GeAny use inline fast-paths for int-vs-int and float-vs-float
        case QoreIROpcode::EqAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCmpFastPath(llvm::CmpInst::ICMP_EQ,
                llvm::CmpInst::FCMP_OEQ, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::EqString: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_string_eq_typed",
                llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::NeAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCmpFastPath(llvm::CmpInst::ICMP_NE,
                llvm::CmpInst::FCMP_ONE, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::NeString: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_string_ne_typed",
                llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::LtString: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_string_lt_typed",
                llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::LeString: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_string_le_typed",
                llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::GtString: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_string_gt_typed",
                llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::GeString: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_string_ge_typed",
                llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::LtAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCmpFastPath(llvm::CmpInst::ICMP_SLT,
                llvm::CmpInst::FCMP_OLT, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::LeAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCmpFastPath(llvm::CmpInst::ICMP_SLE,
                llvm::CmpInst::FCMP_OLE, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::GtAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCmpFastPath(llvm::CmpInst::ICMP_SGT,
                llvm::CmpInst::FCMP_OGT, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::GeAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCmpFastPath(llvm::CmpInst::ICMP_SGE,
                llvm::CmpInst::FCMP_OGE, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        // === Spaceship comparisons (native lowering) ===
        case QoreIROpcode::CmpInt: {
            // Returns -1 if l < r, 1 if l > r, 0 if l == r
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l = ensureIntTypeInline(lhs, inst->operands[0].id);
            llvm::Value* r = ensureIntTypeInline(rhs, inst->operands[1].id);
            llvm::Value* lt = builder->CreateICmpSLT(l, r);
            llvm::Value* gt = builder->CreateICmpSGT(l, r);
            llvm::Value* neg_one = llvm::ConstantInt::get(i64_type, -1);
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            // result = lt ? -1 : (gt ? 1 : 0)
            llvm::Value* inner = builder->CreateSelect(gt, one, zero);
            values[inst->result.id] = builder->CreateSelect(lt, neg_one, inner);
            return true;
        }
        case QoreIROpcode::CmpFloat: {
            // Returns -1 if l < r, 1 if l > r, 0 if l == r
            // NaN comparison raises exception - check with fcmp uno and branch to helper
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            // lhs/rhs are native doubles (not boxed) for typed floats
            llvm::Value* l = lhs;
            llvm::Value* r = rhs;

            // Check for NaN: fcmp uno returns true if either operand is NaN
            llvm::Value* is_nan = builder->CreateFCmpUNO(l, r);

            llvm::BasicBlock* nan_bb = llvm::BasicBlock::Create(ctx, "cmp_nan", llvm_func);
            llvm::BasicBlock* ok_bb = llvm::BasicBlock::Create(ctx, "cmp_ok", llvm_func);
            builder->CreateCondBr(is_nan, nan_bb, ok_bb);

            // NaN path: call runtime helper to raise exception
            builder->SetInsertPoint(nan_bb);
            llvm::Value* lhs_boxed = boxFloat(l);
            llvm::Value* rhs_boxed = boxFloat(r);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(QoreIROpcode::CmpFloat));
            auto helper = module.getOrInsertFunction("qore_rt_comparison_op",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false));
            llvm::Value* nan_result = builder->CreateCall(helper,
                    {opcode_val, lhs_boxed, rhs_boxed, xsink_arg});
            llvm::BasicBlock* nan_end = builder->GetInsertBlock();

            // OK path: native comparison
            builder->SetInsertPoint(ok_bb);
            llvm::Value* lt = builder->CreateFCmpOLT(l, r);
            llvm::Value* gt = builder->CreateFCmpOGT(l, r);
            llvm::Value* neg_one = llvm::ConstantInt::get(i64_type, -1);
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* inner = builder->CreateSelect(gt, one, zero);
            llvm::Value* ok_result = builder->CreateSelect(lt, neg_one, inner);
            llvm::BasicBlock* ok_end = builder->GetInsertBlock();

            // Merge
            llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "cmp_merge", llvm_func);
            builder->CreateBr(merge_bb);
            // Also need to branch from nan_bb
            builder->SetInsertPoint(nan_end);
            builder->CreateBr(merge_bb);

            builder->SetInsertPoint(merge_bb);
            llvm::PHINode* phi = builder->CreatePHI(i64_type, 2);
            phi->addIncoming(ok_result, ok_end);
            phi->addIncoming(nan_result, nan_end);
            values[inst->result.id] = phi;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::CmpString: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_string_cmp_typed",
                llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::CmpAny: {
            // Fast-path for int+int and float+float, fallback to runtime
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCmpSpaceshipFastPath(
                    lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Hard equality comparisons (with fast-path) ===
        case QoreIROpcode::EqHard: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitHardEqualityFastPath(true, lhs_boxed, rhs_boxed,
                    llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::NeHard: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitHardEqualityFastPath(false, lhs_boxed, rhs_boxed,
                    llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }

        // === Dynamic bitwise/shift operations (with int fast-path) ===
        case QoreIROpcode::AndAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::And, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::AndAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::OrAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::Or, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::OrAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::XorAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::Xor, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::XorAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::ShlAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::Shl, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::ShlAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::ShrAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::AShr, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::ShrAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Dynamic unary operations (with int/float fast-path) ===
        case QoreIROpcode::UnaryMinusAny: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            llvm::Value* result = emitAnyUnaryFastPath(true,
                    static_cast<int>(QoreIROpcode::UnaryMinusAny),
                    val_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::UnaryPlusAny: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            llvm::Value* result = emitAnyUnaryFastPath(false,
                    static_cast<int>(QoreIROpcode::UnaryPlusAny),
                    val_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Variable access operations ===
        case QoreIROpcode::LoadGlobal:
        case QoreIROpcode::LoadThreadLocal: {
            const auto* vinst = static_cast<const QoreIRVarInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                const char* helper_name = (inst->opcode == QoreIROpcode::LoadGlobal)
                        ? "qore_rt_load_global_aot" : "qore_rt_load_thread_local_aot";
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getGlobalSlot(
                        reinterpret_cast<const void*>(vinst->var));
                auto helper = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                const char* helper_name = (inst->opcode == QoreIROpcode::LoadGlobal)
                        ? "qore_rt_load_global" : "qore_rt_load_thread_local";
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(vinst->var));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                auto helper = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {var_as_ptr, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            // qore_rt_load_global/thread_local returns +1 ref; track for cleanup at exit
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::LoadImplicitArg: {
            const auto* impl_arg_inst = static_cast<const QoreIRImplicitArgInstruction*>(inst);
            auto helper = module.getOrInsertFunction("qore_rt_load_implicit_arg",
                    llvm::FunctionType::get(i64_type, {i32_type, ptr_type}, false));
            llvm::Value* offset_val = llvm::ConstantInt::get(i32_type, impl_arg_inst->offset);
            llvm::Value* result = builder->CreateCall(helper, {offset_val, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::LoadImplicitArgv: {
            auto helper = module.getOrInsertFunction("qore_rt_load_implicit_argv",
                    llvm::FunctionType::get(i64_type, {ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::LoadImplicitElement: {
            auto helper = module.getOrInsertFunction("qore_rt_load_implicit_element",
                    llvm::FunctionType::get(i64_type, {ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::PushImplicitArg: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_push_implicit_arg",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {val_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);  // Old context is a nanboxed QoreListNode*
            return true;
        }
        case QoreIROpcode::SetImplicitArgv: {
            auto* argv_val = getVal(inst->operands[0].id, error);
            if (!argv_val) { return false; }
            llvm::Value* argv_boxed = boxValue(argv_val, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_set_implicit_argv",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {argv_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);  // Old context is a nanboxed QoreListNode*
            return true;
        }
        case QoreIROpcode::PopImplicitArg: {
            auto* old_ctx = getVal(inst->operands[0].id, error);
            if (!old_ctx) { return false; }
            llvm::Value* old_ctx_boxed = boxValue(old_ctx, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_pop_implicit_arg",
                    llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
            builder->CreateCall(helper, {old_ctx_boxed, xsink_arg});
            return true;
        }
        case QoreIROpcode::PushImplicitElement: {
            auto* idx = getVal(inst->operands[0].id, error);
            if (!idx) { return false; }
            // Ensure native int64 for element index
            llvm::Value* idx_unboxed = ensureIntTypeInline(idx, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_push_implicit_element",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {idx_unboxed, xsink_arg});
            values[inst->result.id] = result;
            // Result is raw int64 (old element index), not nanboxed
            return true;
        }
        case QoreIROpcode::PopImplicitElement: {
            auto* old_elem = getVal(inst->operands[0].id, error);
            if (!old_elem) { return false; }
            // Old element is raw int64
            auto helper = module.getOrInsertFunction("qore_rt_pop_implicit_element",
                    llvm::FunctionType::get(void_type, {i64_type}, false));
            builder->CreateCall(helper, {old_elem});
            return true;
        }
        case QoreIROpcode::CreateEmptyList: {
            auto helper = module.getOrInsertFunction("qore_rt_create_empty_list",
                    llvm::FunctionType::get(i64_type, {ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::ListAppend: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            auto* value = getVal(inst->operands[1].id, error);
            if (!value) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* value_boxed = boxValue(value, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_list_append",
                    llvm::FunctionType::get(void_type, {i64_type, i64_type, ptr_type}, false));
            builder->CreateCall(helper, {list_boxed, value_boxed, xsink_arg});
            return true;
        }
        case QoreIROpcode::ListPush: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            auto* val = getVal(inst->operands[1].id, error);
            if (!val) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* val_boxed = boxValue(val, inst->operands[1].id);
            auto push_fn = module.getOrInsertFunction("qore_rt_list_push",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(push_fn, {list_boxed, val_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::CreateSizedList: {
            auto* cap = getVal(inst->operands[0].id, error);
            if (!cap) { return false; }
            llvm::Value* cap_int = ensureIntTypeInline(cap, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_create_sized_list",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {cap_int, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::ListSize: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            // Result is native i64, not nanboxed
            return true;
        }
        case QoreIROpcode::ListGetInt: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            auto* idx = getVal(inst->operands[1].id, error);
            if (!idx) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* idx_int = ensureIntTypeInline(idx, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_list_get_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, idx_int});
            values[inst->result.id] = result;
            // Result is native i64, not nanboxed
            return true;
        }
        case QoreIROpcode::ListGetFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            auto* idx = getVal(inst->operands[1].id, error);
            if (!idx) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* idx_int = ensureIntTypeInline(idx, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_list_get_float",
                    llvm::FunctionType::get(double_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, idx_int});
            values[inst->result.id] = result;
            // Result is native double, not nanboxed
            return true;
        }
        case QoreIROpcode::ListGetValue: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            auto* idx = getVal(inst->operands[1].id, error);
            if (!idx) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* idx_int = ensureIntTypeInline(idx, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_list_get_value",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, idx_int, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::ListGetValueNoRef: {
            // In LLVM/JIT mode, treat as ListGetValue (with refSelf + cleanup tracking)
            // because the JIT cleanup model requires owned references.
            // The noref optimization is only safe in the interpreter path.
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            auto* idx = getVal(inst->operands[1].id, error);
            if (!idx) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* idx_int = ensureIntTypeInline(idx, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_list_get_value",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, idx_int, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::ListSetInt: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            auto* idx = getVal(inst->operands[1].id, error);
            if (!idx) { return false; }
            auto* val = getVal(inst->operands[2].id, error);
            if (!val) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* idx_int = ensureIntTypeInline(idx, inst->operands[1].id);
            llvm::Value* val_int = ensureIntTypeInline(val, inst->operands[2].id);
            auto helper = module.getOrInsertFunction("qore_rt_list_set_int",
                    llvm::FunctionType::get(void_type, {i64_type, i64_type, i64_type}, false));
            builder->CreateCall(helper, {list_boxed, idx_int, val_int});
            return true;
        }
        case QoreIROpcode::ListSetFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            auto* idx = getVal(inst->operands[1].id, error);
            if (!idx) { return false; }
            auto* val = getVal(inst->operands[2].id, error);
            if (!val) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* idx_int = ensureIntTypeInline(idx, inst->operands[1].id);
            llvm::Value* val_float = ensureFloatType(val, inst->operands[2].id, module);
            auto helper = module.getOrInsertFunction("qore_rt_list_set_float",
                    llvm::FunctionType::get(void_type, {i64_type, i64_type, double_type}, false));
            builder->CreateCall(helper, {list_boxed, idx_int, val_float});
            return true;
        }
        case QoreIROpcode::ListSetValue: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            auto* idx = getVal(inst->operands[1].id, error);
            if (!idx) { return false; }
            auto* val = getVal(inst->operands[2].id, error);
            if (!val) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* idx_int = ensureIntTypeInline(idx, inst->operands[1].id);
            llvm::Value* val_boxed = boxValue(val, inst->operands[2].id);
            // refSelf before ownership transfer: the value may also be tracked by
            // trackResultForCleanup (invoke results) or boxValue's internal cleanup
            // (big int boxing), so the list needs its own reference
            auto refself_fn = module.getOrInsertFunction("qore_rt_refself",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* val_ref = builder->CreateCall(refself_fn, {val_boxed});
            auto helper = module.getOrInsertFunction("qore_rt_list_set_value",
                    llvm::FunctionType::get(void_type, {i64_type, i64_type, i64_type}, false));
            builder->CreateCall(helper, {list_boxed, idx_int, val_ref});
            return true;
        }
        case QoreIROpcode::GetObjectClass: {
            auto* obj = getVal(inst->operands[0].id, error);
            if (!obj) { return false; }
            llvm::Value* obj_boxed = boxValue(obj, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_get_object_class",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {obj_boxed});
            values[inst->result.id] = result;
            // Result is raw class pointer as i64, not nanboxed
            return true;
        }
        case QoreIROpcode::CallClosureDirect: {
            // operands[0] = closure/callref value, operands[1..n] = args
            auto* ref = getVal(inst->operands[0].id, error);
            if (!ref) { return false; }
            llvm::Value* ref_boxed = boxValue(ref, inst->operands[0].id);

            // Build args array from operands[1..]
            llvm::Value* args_array;
            int nargs;
            if (!buildArgsArray(inst, 1, llvm_func, args_array, nargs, error)) {
                return false;
            }

            auto helper = module.getOrInsertFunction("qore_rt_call_closure_fast",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type, i32_type, ptr_type}, false));
            llvm::Value* nargs_val = llvm::ConstantInt::get(i32_type, nargs);
            llvm::Value* result = builder->CreateCall(helper, {ref_boxed, args_array, nargs_val, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::StoreGlobal:
        case QoreIROpcode::StoreThreadLocal: {
            const auto* vinst = static_cast<const QoreIRVarInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            if (aot_mode) {
                const char* helper_name = (inst->opcode == QoreIROpcode::StoreGlobal)
                        ? "qore_rt_store_global_aot" : "qore_rt_store_thread_local_aot";
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getGlobalSlot(
                        reinterpret_cast<const void*>(vinst->var));
                auto helper = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
                builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), val_boxed, xsink_arg});
            } else {
                const char* helper_name = (inst->opcode == QoreIROpcode::StoreGlobal)
                        ? "qore_rt_store_global" : "qore_rt_store_thread_local";
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(vinst->var));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                auto helper = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
                builder->CreateCall(helper, {var_as_ptr, val_boxed, xsink_arg});
            }
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::LoadClosure: {
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                        reinterpret_cast<const void*>(linst->local));
                auto helper = module.getOrInsertFunction("qore_rt_load_closure_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_load_local",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {var_as_ptr, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::StoreClosure: {
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                        reinterpret_cast<const void*>(linst->local));
                auto helper = module.getOrInsertFunction("qore_rt_store_closure_aot",
                        llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
                builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), val_boxed, xsink_arg});
            } else {
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_assign_local",
                        llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
                builder->CreateCall(helper, {var_as_ptr, val_boxed, xsink_arg});
            }
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::LoadArg: {
            // LoadArg loads argument by index from the internal args vector.
            // In JIT context, args are typically pre-instantiated as local variables.
            // If this opcode appears, we return NOTHING (args aren't accessible from JIT).
            values[inst->result.id] = llvm::ConstantInt::get(i64_type, VAL_NOTHING);
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::HashKeyAccess: {
            const auto* hka_inst = static_cast<const QoreIRHashKeyAccessInstruction*>(inst);
            auto* base = getVal(inst->operands[0].id, error);
            if (!base) {
                return false;
            }
            llvm::Value* base_boxed = boxValue(base, inst->operands[0].id);
            llvm::Constant* key_const = builder->CreateGlobalString(hka_inst->key_name,
                    "hash_key");
            auto helper = module.getOrInsertFunction("qore_rt_hash_key_access",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper,
                    {base_boxed, key_const, xsink_arg});
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(values[inst->result.id], inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::HashKeyAccessInt: {
            const auto* hka_inst = static_cast<const QoreIRHashKeyAccessInstruction*>(inst);
            auto* base = getVal(inst->operands[0].id, error);
            if (!base) {
                return false;
            }
            llvm::Value* base_boxed = boxValue(base, inst->operands[0].id);
            llvm::Constant* key_const = builder->CreateGlobalString(hka_inst->key_name,
                    "hash_key_int");
            auto helper = module.getOrInsertFunction("qore_rt_hash_key_access_int",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {base_boxed, key_const});
            // Result is native int64, NOT nanboxed — no trackResultForCleanup needed
            return true;
        }
        case QoreIROpcode::HashKeyStore: {
            const auto* hks_inst = static_cast<const QoreIRHashKeyStoreInstruction*>(inst);
            std::string err;
            auto* hash_v = getVal(inst->operands[0].id, err);
            auto* val_v  = getVal(inst->operands[1].id, err);
            if (!hash_v || !val_v) {
                error = err;
                return false;
            }
            llvm::Value* hash_boxed = boxValue(hash_v, inst->operands[0].id);
            llvm::Value* val_boxed  = boxValue(val_v,  inst->operands[1].id);
            llvm::Constant* key_c = builder->CreateGlobalString(hks_inst->key_name, "hks_key");

            llvm::Value* call_result;
            if (aot_mode) {
                // AOT: pass ctx + pre-registered local slot index for COW update
                uint32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                        reinterpret_cast<const void*>(hks_inst->container->ref.id));
                llvm::Value* slot_val = llvm::ConstantInt::get(i32_type, slot);
                auto fn = module.getOrInsertFunction("qore_rt_hash_key_store_cow_aot",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, i64_type, ptr_type, i64_type, ptr_type}, false));
                call_result = builder->CreateCall(fn,
                        {aot_ctx_arg, slot_val, hash_boxed, key_c, val_boxed, xsink_arg});
            } else {
                // JIT: pass LocalVar* directly
                auto var_int = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(hks_inst->container->ref.id));
                auto* var_ptr = builder->CreateIntToPtr(var_int, ptr_type);
                auto fn = module.getOrInsertFunction("qore_rt_hash_key_store_cow",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i64_type, ptr_type, i64_type, ptr_type}, false));
                call_result = builder->CreateCall(fn, {var_ptr, hash_boxed, key_c, val_boxed, xsink_arg});
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = call_result;
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(call_result, inst->result.id, llvm_func);
            }
            emitExceptionCheck(module, llvm_func, inst);

            // CRITICAL: After HashKeyStore COW, reload the hash from the LocalVar.
            // qore_rt_hash_key_store_cow updates the LocalVar, but the hash value
            // in values[] array might be stale if COW created a copy. For single-pass
            // operations like `h{"x"} += 3`, we must update values[] immediately.
            uint32_t hash_operand_id = inst->operands[0].id;
            if (aot_mode) {
                // AOT: reload from context slot
                auto load_fn = module.getOrInsertFunction("qore_rt_load_local_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                uint32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                        reinterpret_cast<const void*>(hks_inst->container->ref.id));
                llvm::Value* updated_hash = builder->CreateCall(load_fn,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                values[hash_operand_id] = updated_hash;
                nanboxed_values.insert(hash_operand_id);
                trackResultForCleanup(updated_hash, hash_operand_id, llvm_func);
            } else {
                // JIT: reload from LocalVar pointer
                auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                auto var_int = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(hks_inst->container->ref.id));
                auto* var_ptr = builder->CreateIntToPtr(var_int, ptr_type);
                llvm::Value* updated_hash = builder->CreateCall(load_fn, {var_ptr, xsink_arg});
                values[hash_operand_id] = updated_hash;
                nanboxed_values.insert(hash_operand_id);
                trackResultForCleanup(updated_hash, hash_operand_id, llvm_func);

                // JIT: also update the alloca cache so subsequent LoadLocal(h) reads
                // the post-COW hash, not the stale pre-COW value cached in the alloca.
                const void* container_key =
                    reinterpret_cast<const void*>(hks_inst->container->ref.id);
                reloadLocalFromRuntime(container_key, module, llvm_func);
            }

            return true;
        }
        case QoreIROpcode::ListIndexStore: {
            const auto* lis_inst = static_cast<const QoreIRListIndexStoreInstruction*>(inst);
            std::string err;
            auto* list_v = getVal(lis_inst->operands[0].id, err);
            auto* val_v  = getVal(lis_inst->operands[1].id, err);
            auto* idx_v  = getVal(lis_inst->operands[2].id, err);
            if (!list_v || !val_v || !idx_v) {
                error = err;
                return false;
            }
            llvm::Value* list_boxed = boxValue(list_v, lis_inst->operands[0].id);
            llvm::Value* val_boxed  = boxValue(val_v,  lis_inst->operands[1].id);
            // Index is already i64, but may be in a nanboxed slot; extract it
            llvm::Value* index_i64;
            if (nanboxed_values.count(lis_inst->operands[2].id)) {
                // Index is nanboxed; extract as i64
                auto unbox_fn = module.getOrInsertFunction("qore_rt_get_int64",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                index_i64 = builder->CreateCall(unbox_fn, {idx_v, xsink_arg});
            } else {
                // Index is already a native i64
                index_i64 = idx_v;
            }

            llvm::Value* call_result;
            if (aot_mode) {
                // AOT: pass ctx + pre-registered local slot index for COW update
                uint32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                        reinterpret_cast<const void*>(lis_inst->container->ref.id));
                llvm::Value* slot_val = llvm::ConstantInt::get(i32_type, slot);
                auto fn = module.getOrInsertFunction("qore_rt_list_index_store_cow_aot",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, i64_type, i64_type, i64_type, ptr_type}, false));
                call_result = builder->CreateCall(fn,
                        {aot_ctx_arg, slot_val, list_boxed, index_i64, val_boxed, xsink_arg});
            } else {
                // JIT: pass LocalVar* directly
                auto var_int = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(lis_inst->container->ref.id));
                auto* var_ptr = builder->CreateIntToPtr(var_int, ptr_type);
                auto fn = module.getOrInsertFunction("qore_rt_list_index_store_cow",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i64_type, i64_type, i64_type, ptr_type}, false));
                call_result = builder->CreateCall(fn, {var_ptr, list_boxed, index_i64, val_boxed, xsink_arg});
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = call_result;
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(call_result, inst->result.id, llvm_func);
            }
            emitExceptionCheck(module, llvm_func, inst);

            // CRITICAL: After ListIndexStore COW, reload the list from the LocalVar.
            // Same pattern as HashKeyStore.
            uint32_t list_operand_id = lis_inst->operands[0].id;
            if (aot_mode) {
                // AOT: reload from context slot
                auto load_fn = module.getOrInsertFunction("qore_rt_load_local_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                uint32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                        reinterpret_cast<const void*>(lis_inst->container->ref.id));
                llvm::Value* updated_list = builder->CreateCall(load_fn,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                values[list_operand_id] = updated_list;
                nanboxed_values.insert(list_operand_id);
                trackResultForCleanup(updated_list, list_operand_id, llvm_func);
            } else {
                // JIT: reload from LocalVar pointer
                auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                auto var_int = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(lis_inst->container->ref.id));
                auto* var_ptr = builder->CreateIntToPtr(var_int, ptr_type);
                llvm::Value* updated_list = builder->CreateCall(load_fn, {var_ptr, xsink_arg});
                values[list_operand_id] = updated_list;
                nanboxed_values.insert(list_operand_id);
                trackResultForCleanup(updated_list, list_operand_id, llvm_func);

                // JIT: also update the alloca cache so subsequent LoadLocal(l) reads
                // the post-COW list, not the stale pre-COW value cached in the alloca.
                const void* container_key =
                    reinterpret_cast<const void*>(lis_inst->container->ref.id);
                reloadLocalFromRuntime(container_key, module, llvm_func);
            }

            return true;
        }
        case QoreIROpcode::LoadSelfMember: {
            const auto* sminst = static_cast<const QoreIRSelfMemberInstruction*>(inst);
            llvm::Constant* name_const = builder->CreateGlobalString(sminst->member_name,
                    "self_member_name");
            auto helper = module.getOrInsertFunction("qore_rt_load_self_member",
                    llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {name_const, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::NewObject: {
            const auto* noinst = static_cast<const QoreIRNewObjectInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                // AOT: delegate to AST expression evaluation
                QoreValue expr_val = noinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                // JIT: call constructor directly
                llvm::Value* qc_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(noinst->qc));
                llvm::Value* qc_as_ptr = builder->CreateIntToPtr(qc_ptr, ptr_type);
                llvm::Value* variant_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(noinst->variant));
                llvm::Value* variant_as_ptr = builder->CreateIntToPtr(variant_ptr, ptr_type);
                llvm::Value* args_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(noinst->args));
                llvm::Value* args_as_ptr = builder->CreateIntToPtr(args_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_new_object",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {qc_as_ptr, variant_as_ptr, args_as_ptr, xsink_arg});
            }
            // Constructor evaluates args from AST and runs constructor body;
            // either can modify locals through reference parameters
            reloadAllLocalsFromRuntime(module, llvm_func);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::LoadStaticVar: {
            const auto* svinst = static_cast<const QoreIRStaticVarInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                // AOT: use expression slot to resolve the StaticClassVarRefNode at load time
                QoreValue expr_val = svinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                // JIT: pass QoreVarInfo* and var_name directly
                llvm::Value* vi_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(svinst->vi));
                llvm::Value* vi_as_ptr = builder->CreateIntToPtr(vi_ptr, ptr_type);
                llvm::Constant* name_const = builder->CreateGlobalString(svinst->var_name,
                        "static_var_name");
                auto helper = module.getOrInsertFunction("qore_rt_load_static_var",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {vi_as_ptr, name_const, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::LoadConstant: {
            const auto* lcinst = static_cast<const QoreIRLoadConstantInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                QoreValue expr_val = lcinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(lcinst->node));
                llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_load_constant",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {node_as_ptr, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::CreateClosure: {
            const auto* ccinst = static_cast<const QoreIRCreateClosureInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                QoreValue expr_val = ccinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* cn_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(ccinst->closure_node));
                llvm::Value* cn_as_ptr = builder->CreateIntToPtr(cn_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_create_closure",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {cn_as_ptr, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::CreateCallRef: {
            const auto* crinst = static_cast<const QoreIRCreateCallRefInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                QoreValue expr_val = crinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                QoreValue expr_val = crinst->expr;
                uint64_t bits;
                std::memcpy(&bits, &expr_val, sizeof(bits));
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, bits);
                auto helper = module.getOrInsertFunction("qore_rt_create_call_ref",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {expr_const, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::CreateMethodRef: {
            const auto* mrinst = static_cast<const QoreIRCreateMethodRefInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                QoreValue expr_val = mrinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                QoreValue expr_val = mrinst->expr;
                uint64_t bits;
                std::memcpy(&bits, &expr_val, sizeof(bits));
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, bits);
                auto helper = module.getOrInsertFunction("qore_rt_create_method_ref",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {expr_const, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::CreateParseRef: {
            const auto* prinst = static_cast<const QoreIRCreateParseRefInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                QoreValue expr_val = prinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(prinst->node));
                llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_create_parse_ref",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {node_as_ptr, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::NewHashDecl: {
            const auto* nhdinst = static_cast<const QoreIRNewHashDeclInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                QoreValue expr_val = nhdinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(nhdinst->node));
                llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_new_hash_decl",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {node_as_ptr, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::NewComplexHash: {
            const auto* nchinst = static_cast<const QoreIRNewComplexHashInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                QoreValue expr_val = nchinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(nchinst->node));
                llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_new_complex_hash",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {node_as_ptr, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::NewComplexList: {
            const auto* nclinst = static_cast<const QoreIRNewComplexListInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                QoreValue expr_val = nclinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(nclinst->node));
                llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_new_complex_list",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {node_as_ptr, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::VrnConstruct: {
            const auto* vrninst = static_cast<const QoreIRVrnConstructInstruction*>(inst);
            llvm::Value* result;
            // Use dedicated AOT helper to avoid double-assign (eval() assigns + StoreLocal assigns)
            if (aot_mode) {
                QoreValue expr_val = vrninst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_vrn_construct_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* vrn_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(vrninst->vrn));
                llvm::Value* vrn_as_ptr = builder->CreateIntToPtr(vrn_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_vrn_construct",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {vrn_as_ptr, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::HashSetKeyValue: {
            auto* hash_val = getVal(inst->operands[0].id, error);
            if (!hash_val) { return false; }
            auto* key_val = getVal(inst->operands[1].id, error);
            if (!key_val) { return false; }
            auto* value_val = getVal(inst->operands[2].id, error);
            if (!value_val) { return false; }
            // All three operands must be NaN-boxed i64 for the runtime helper.
            // Check LLVM types to use correct boxing: i1 -> boxBool,
            // double -> boxFloat, i64 -> boxIntInline (ConstBool is i1,
            // ConstFloat is double — boxIntInline would misinterpret them)
            if (!nanboxed_values.count(inst->operands[0].id)) {
                if (hash_val->getType() == i1_type) {
                    hash_val = boxBool(hash_val);
                } else if (hash_val->getType() == double_type) {
                    hash_val = boxFloat(hash_val);
                } else {
                    hash_val = boxIntInline(hash_val);
                }
            }
            if (!nanboxed_values.count(inst->operands[1].id)) {
                if (key_val->getType() == i1_type) {
                    key_val = boxBool(key_val);
                } else if (key_val->getType() == double_type) {
                    key_val = boxFloat(key_val);
                } else {
                    key_val = boxIntInline(key_val);
                }
            }
            if (!nanboxed_values.count(inst->operands[2].id)) {
                if (value_val->getType() == i1_type) {
                    value_val = boxBool(value_val);
                } else if (value_val->getType() == double_type) {
                    value_val = boxFloat(value_val);
                } else {
                    value_val = boxIntInline(value_val);
                }
            }
            auto helper = module.getOrInsertFunction("qore_rt_hash_set_key_value",
                    llvm::FunctionType::get(void_type,
                        {i64_type, i64_type, i64_type, ptr_type}, false));
            builder->CreateCall(helper, {hash_val, key_val, value_val, xsink_arg});
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::IteratorCreateReverse: {
            auto* iterable_val = getVal(inst->operands[0].id, error);
            if (!iterable_val) { return false; }
            // Box the iterable value if needed
            llvm::Value* iterable_boxed;
            if (nanboxed_values.count(inst->operands[0].id)) {
                iterable_boxed = iterable_val;
            } else if (iterable_val->getType() == i64_type) {
                iterable_boxed = boxIntInline(iterable_val);
            } else if (iterable_val->getType() == double_type) {
                iterable_boxed = boxFloat(iterable_val);
            } else {
                error = "unsupported iterable type for IteratorCreateReverse";
                return false;
            }
            auto helper = module.getOrInsertFunction("qore_rt_iterator_create_reverse",
                    llvm::FunctionType::get(ptr_type, {i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {iterable_boxed, xsink_arg});
            values[inst->result.id] = result;
            // Iterator pointer is NOT nanboxed — it's an opaque ptr

            // Track active iterator for cleanup on non-normal exit
            {
                llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                        llvm_func->getEntryBlock().begin());
                llvm::AllocaInst* iter_alloca = ab.CreateAlloca(ptr_type, nullptr, "iter_cleanup");
                ab.CreateStore(llvm::ConstantPointerNull::get(
                        llvm::PointerType::get(ctx, 0)), iter_alloca);
                builder->CreateStore(result, iter_alloca);
                iterator_cleanup_allocas.push_back(iter_alloca);
                invoke_alloca_map[inst->result.id] = iter_alloca;
            }

            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === LValue operations ===
        case QoreIROpcode::LoadLValue: {
            const auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst);
            QoreValue lv = lvinst->lvalue;
            uint64_t lv_bits;
            std::memcpy(&lv_bits, &lv, sizeof(lv_bits));
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_load_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_load",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {lv_const, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::StoreLValue: {
            const auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            QoreValue lv = lvinst->lvalue;
            uint64_t lv_bits;
            std::memcpy(&lv_bits, &lv, sizeof(lv_bits));
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            // Clear the reload tracker for the lvalue target local (same pattern
            // as compound assign — prevents refcount inflation in loops).
            {
                const void* local_key = findLvalueRootLocalKey(lvinst->lvalue);
                if (local_key) {
                    clearLocalReloadTracker(local_key, module, llvm_func);
                }
            }
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(lv_bits);
                const char* fn_name = lvinst->weak
                    ? "qore_rt_lvalue_store_weak_aot" : "qore_rt_lvalue_store_aot";
                auto helper = module.getOrInsertFunction(fn_name,
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), val_boxed, xsink_arg});
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                const char* fn_name = lvinst->weak
                    ? "qore_rt_lvalue_store_weak" : "qore_rt_lvalue_store";
                auto helper = module.getOrInsertFunction(fn_name,
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {lv_const, val_boxed, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            {
                const void* local_key = findLvalueRootLocalKey(lvinst->lvalue);
                if (local_key) {
                    reloadLocalFromRuntime(local_key, module, llvm_func);
                }
            }
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue: {
            const auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst);
            QoreValue lv = lvinst->lvalue;
            uint64_t lv_bits;
            std::memcpy(&lv_bits, &lv, sizeof(lv_bits));
            emitPreDecrefAndClearTracker(inst->result.id, lvinst, module, llvm_func);
            llvm::Value* opcode_val = llvm::ConstantInt::get(i32_type,
                    static_cast<int>(inst->opcode));
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_unary_aot",
                        llvm::FunctionType::get(i64_type,
                            {i32_type, ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {opcode_val, aot_ctx_arg,
                         llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_unary",
                        llvm::FunctionType::get(i64_type,
                            {i32_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {opcode_val, lv_const, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            {
                const void* local_key = findLvalueRootLocalKey(lvinst->lvalue);
                if (local_key) {
                    reloadLocalFromRuntime(local_key, module, llvm_func);
                }
            }
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::AddAssignLValue:
        case QoreIROpcode::SubAssignLValue:
        case QoreIROpcode::MulAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue: {
            const auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            QoreValue lv = lvinst->lvalue;
            uint64_t lv_bits;
            std::memcpy(&lv_bits, &lv, sizeof(lv_bits));
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            llvm::Value* lv_bits_or_slot;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(lv_bits);
                lv_bits_or_slot = llvm::ConstantInt::get(i32_type, slot);
            } else {
                lv_bits_or_slot = llvm::ConstantInt::get(i64_type, lv_bits);
            }

            // Determine fast-path ops based on opcode
            int iop = -1;
            int fop = -1;
            bool nothing = false;
            switch (inst->opcode) {
                case QoreIROpcode::AddAssignLValue:
                    iop = llvm::Instruction::Add; fop = llvm::Instruction::FAdd;
                    break;
                case QoreIROpcode::SubAssignLValue:
                    iop = llvm::Instruction::Sub; fop = llvm::Instruction::FSub;
                    break;
                case QoreIROpcode::MulAssignLValue:
                    iop = llvm::Instruction::Mul; fop = llvm::Instruction::FMul;
                    break;
                case QoreIROpcode::AndAssignLValue:
                    iop = llvm::Instruction::And;
                    break;
                case QoreIROpcode::OrAssignLValue:
                    iop = llvm::Instruction::Or;
                    break;
                case QoreIROpcode::XorAssignLValue:
                    iop = llvm::Instruction::Xor;
                    break;
                case QoreIROpcode::ShlAssignLValue:
                    iop = llvm::Instruction::Shl;
                    break;
                case QoreIROpcode::ShrAssignLValue:
                    iop = llvm::Instruction::AShr;
                    break;
                default:
                    // All other opcodes do not have inline fast paths; iop and fop remain -1
                    break;
            }

            // Pre-decref old result and clear reload tracker before the lvalue
            // operation.  The old +1 ref on the list/hash would make the container
            // appear shared (refcount > 1), triggering copy-on-write and turning
            // O(1) appends into O(n) copies.
            llvm::AllocaInst* ca = emitPreDecrefAndClearTracker(
                    inst->result.id, lvinst, module, llvm_func);

            llvm::Value* result = emitLValueCompoundAssignFastPath(
                    inst, val_boxed, lv_bits_or_slot, iop, fop, nothing,
                    llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            // Store result in alloca for cleanup (decref handled by pre-decref above
            // on next iteration, and by invoke_result_allocas at function exit)
            builder->CreateStore(result, ca);
            {
                const void* local_key = findLvalueRootLocalKey(lvinst->lvalue);
                if (local_key) {
                    reloadLocalFromRuntime(local_key, module, llvm_func);
                }
            }
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue: {
            // Div/mod need zero-check (skip inline fast path)
            const auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            QoreValue lv = lvinst->lvalue;
            uint64_t lv_bits;
            std::memcpy(&lv_bits, &lv, sizeof(lv_bits));
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            emitPreDecrefAndClearTracker(inst->result.id, lvinst, module, llvm_func);
            llvm::Value* opcode_val = llvm::ConstantInt::get(i32_type,
                    static_cast<int>(inst->opcode));
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_binary_aot",
                        llvm::FunctionType::get(i64_type,
                            {i32_type, ptr_type, i32_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {opcode_val, aot_ctx_arg,
                         llvm::ConstantInt::get(i32_type, slot), val_boxed, xsink_arg});
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_binary",
                        llvm::FunctionType::get(i64_type,
                            {i32_type, i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {opcode_val, lv_const, val_boxed, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            {
                const void* local_key = findLvalueRootLocalKey(lvinst->lvalue);
                if (local_key) {
                    reloadLocalFromRuntime(local_key, module, llvm_func);
                }
            }
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::SpliceLValue: {
            // Splice is a ternary lvalue op: lvalue, offset, length, replacement
            const auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst);
            if (inst->operands.size() < 3) {
                error = "SpliceLValue requires 3 operands (offset, length, replacement)";
                return false;
            }
            auto* first_val = getVal(inst->operands[0].id, error);
            if (!first_val) { return false; }
            auto* second_val = getVal(inst->operands[1].id, error);
            if (!second_val) { return false; }
            auto* third_val = getVal(inst->operands[2].id, error);
            if (!third_val) { return false; }
            QoreValue lv = lvinst->lvalue;
            uint64_t lv_bits;
            std::memcpy(&lv_bits, &lv, sizeof(lv_bits));
            llvm::Value* first_boxed = boxValue(first_val, inst->operands[0].id);
            llvm::Value* second_boxed = boxValue(second_val, inst->operands[1].id);
            llvm::Value* third_boxed = boxValue(third_val, inst->operands[2].id);
            emitPreDecrefAndClearTracker(inst->result.id, lvinst, module, llvm_func);
            llvm::Value* opcode_val = llvm::ConstantInt::get(i32_type,
                    static_cast<int>(inst->opcode));
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_ternary_aot",
                        llvm::FunctionType::get(i64_type,
                            {i32_type, ptr_type, i32_type, i64_type, i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {opcode_val, aot_ctx_arg,
                         llvm::ConstantInt::get(i32_type, slot), first_boxed, second_boxed, third_boxed, xsink_arg});
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_ternary",
                        llvm::FunctionType::get(i64_type,
                            {i32_type, i64_type, i64_type, i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {opcode_val, lv_const, first_boxed, second_boxed, third_boxed, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            {
                const void* local_key = findLvalueRootLocalKey(lvinst->lvalue);
                if (local_key) {
                    reloadLocalFromRuntime(local_key, module, llvm_func);
                }
            }
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Container construction ===
        case QoreIROpcode::MakeList: {
            // Allocate stack array and fill with NaN-boxed operand values
            int count = static_cast<int>(inst->operands.size());
            llvm::Value* count_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), count);
            // Hoist alloca to entry block to avoid stack overflow in loops
            llvm::IRBuilder<> ab_list(&llvm_func->getEntryBlock(),
                    llvm_func->getEntryBlock().begin());
            llvm::Value* arr = ab_list.CreateAlloca(i64_type,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), count));
            for (int i = 0; i < count; i++) {
                auto* elem = getVal(inst->operands[i].id, error);
                if (!elem) { return false; }
                llvm::Value* elem_boxed = boxValue(elem, inst->operands[i].id);
                llvm::Value* gep = builder->CreateGEP(i64_type, arr,
                        {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), i)});
                builder->CreateStore(elem_boxed, gep);
            }
            auto helper = module.getOrInsertFunction("qore_rt_make_list",
                    llvm::FunctionType::get(i64_type,
                        {ptr_type, llvm::Type::getInt32Ty(ctx), ptr_type}, false));
            llvm::Value* list_result = builder->CreateCall(helper, {arr, count_val, xsink_arg});
            values[inst->result.id] = list_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(list_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::MakeHash: {
            // Operands are alternating keys and values
            int pair_count = static_cast<int>(inst->operands.size() / 2);
            int total = static_cast<int>(inst->operands.size());
            llvm::Value* count_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), pair_count);
            // Hoist alloca to entry block to avoid stack overflow in loops
            llvm::IRBuilder<> ab_hash(&llvm_func->getEntryBlock(),
                    llvm_func->getEntryBlock().begin());
            llvm::Value* arr = ab_hash.CreateAlloca(i64_type,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), total));
            for (int i = 0; i < total; i++) {
                auto* elem = getVal(inst->operands[i].id, error);
                if (!elem) { return false; }
                llvm::Value* elem_boxed = boxValue(elem, inst->operands[i].id);
                llvm::Value* gep = builder->CreateGEP(i64_type, arr,
                        {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), i)});
                builder->CreateStore(elem_boxed, gep);
            }
            auto helper = module.getOrInsertFunction("qore_rt_make_hash",
                    llvm::FunctionType::get(i64_type,
                        {ptr_type, llvm::Type::getInt32Ty(ctx), ptr_type}, false));
            llvm::Value* hash_result = builder->CreateCall(helper, {arr, count_val, xsink_arg});
            values[inst->result.id] = hash_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(hash_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Typed integer compound assignments (native i64) ===
        case QoreIROpcode::AddAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAdd(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::SubAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateSub(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::MulAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateMul(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::AndAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAnd(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::OrAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateOr(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::XorAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateXor(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::ShlAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateShl(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }
        case QoreIROpcode::ShrAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAShr(
                ensureIntTypeInline(lhs, inst->operands[0].id),
                ensureIntTypeInline(rhs, inst->operands[1].id));
            return true;
        }

        // === Typed float compound assignments (native double) ===
        case QoreIROpcode::AddAssignFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFAdd(l_float, r_float);
            return true;
        }
        case QoreIROpcode::SubAssignFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFSub(l_float, r_float);
            return true;
        }
        case QoreIROpcode::MulAssignFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            values[inst->result.id] = builder->CreateFMul(l_float, r_float);
            return true;
        }

        // === Any-typed compound assignments with fast-path ===
        case QoreIROpcode::AddAssignAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCompoundAssignFastPath(
                llvm::Instruction::Add, llvm::Instruction::FAdd,
                "qore_rt_add_any", lhs_boxed, rhs_boxed, llvm_func, module, true);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::SubAssignAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCompoundAssignFastPath(
                llvm::Instruction::Sub, llvm::Instruction::FSub,
                "qore_rt_sub_any", lhs_boxed, rhs_boxed, llvm_func, module, false);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MulAssignAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyCompoundAssignFastPath(
                llvm::Instruction::Mul, llvm::Instruction::FMul,
                "qore_rt_mul_any", lhs_boxed, rhs_boxed, llvm_func, module, false);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::AndAssignAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::And, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::AndAssignAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::OrAssignAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::Or, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::OrAssignAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::XorAssignAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::Xor, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::XorAssignAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::ShlAssignAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::Shl, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::ShlAssignAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::ShrAssignAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* result = emitAnyBitwiseFastPath(
                llvm::Instruction::AShr, "qore_rt_binary_op",
                static_cast<int>(QoreIROpcode::ShrAssignAny),
                lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }

        // === Division/Modulo compound assignments with inline zero-check ===
        case QoreIROpcode::DivAssignInt: {
            // Phase 2E: Inline zero-check with native division for non-zero case
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_int = ensureIntTypeInline(lhs, inst->operands[0].id);
            llvm::Value* r_int = ensureIntTypeInline(rhs, inst->operands[1].id);
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* is_zero = builder->CreateICmpEQ(r_int, zero);

            llvm::BasicBlock* div_zero_bb = llvm::BasicBlock::Create(ctx, "diva_zero", llvm_func);
            llvm::BasicBlock* div_ok_bb = llvm::BasicBlock::Create(ctx, "diva_ok", llvm_func);
            llvm::BasicBlock* div_merge_bb = llvm::BasicBlock::Create(ctx, "diva_merge", llvm_func);
            builder->CreateCondBr(is_zero, div_zero_bb, div_ok_bb);

            // Division by zero path: call runtime helper to raise exception
            builder->SetInsertPoint(div_zero_bb);
            auto helper = module.getOrInsertFunction("qore_rt_div_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* exc_result = builder->CreateCall(helper, {l_int, r_int, xsink_arg});
            builder->CreateBr(div_merge_bb);

            // Normal path: native division (result is raw i64)
            builder->SetInsertPoint(div_ok_bb);
            llvm::Value* div_result = builder->CreateSDiv(l_int, r_int);
            builder->CreateBr(div_merge_bb);

            // Merge results
            builder->SetInsertPoint(div_merge_bb);
            llvm::PHINode* phi = builder->CreatePHI(i64_type, 2, "diva_result");
            phi->addIncoming(exc_result, div_zero_bb);
            phi->addIncoming(div_result, div_ok_bb);
            values[inst->result.id] = phi;
            return true;
        }
        case QoreIROpcode::DivAssignFloat: {
            // Phase 2E: Inline zero-check with native division for non-zero case
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_float = ensureFloatType(lhs, inst->operands[0].id, module);
            llvm::Value* r_float = ensureFloatType(rhs, inst->operands[1].id, module);
            llvm::Value* zero = llvm::ConstantFP::get(double_type, 0.0);
            llvm::Value* is_zero = builder->CreateFCmpOEQ(r_float, zero);

            llvm::BasicBlock* fdiv_zero_bb = llvm::BasicBlock::Create(ctx, "fdiva_zero", llvm_func);
            llvm::BasicBlock* fdiv_ok_bb = llvm::BasicBlock::Create(ctx, "fdiva_ok", llvm_func);
            llvm::BasicBlock* fdiv_merge_bb = llvm::BasicBlock::Create(ctx, "fdiva_merge", llvm_func);
            builder->CreateCondBr(is_zero, fdiv_zero_bb, fdiv_ok_bb);

            // Division by zero path: call runtime helper to raise exception
            builder->SetInsertPoint(fdiv_zero_bb);
            auto helper = module.getOrInsertFunction("qore_rt_div_float",
                    llvm::FunctionType::get(double_type, {double_type, double_type, ptr_type}, false));
            llvm::Value* exc_result = builder->CreateCall(helper, {l_float, r_float, xsink_arg});
            builder->CreateBr(fdiv_merge_bb);

            // Normal path: native division (result is raw double)
            builder->SetInsertPoint(fdiv_ok_bb);
            llvm::Value* div_result = builder->CreateFDiv(l_float, r_float);
            builder->CreateBr(fdiv_merge_bb);

            // Merge results
            builder->SetInsertPoint(fdiv_merge_bb);
            llvm::PHINode* phi = builder->CreatePHI(double_type, 2, "fdiva_result");
            phi->addIncoming(exc_result, fdiv_zero_bb);
            phi->addIncoming(div_result, fdiv_ok_bb);
            values[inst->result.id] = phi;
            return true;
        }
        case QoreIROpcode::ModAssignInt: {
            // Phase 2E: Inline zero-check with native modulo for non-zero case
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_int = ensureIntTypeInline(lhs, inst->operands[0].id);
            llvm::Value* r_int = ensureIntTypeInline(rhs, inst->operands[1].id);
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* is_zero = builder->CreateICmpEQ(r_int, zero);

            llvm::BasicBlock* mod_zero_bb = llvm::BasicBlock::Create(ctx, "moda_zero", llvm_func);
            llvm::BasicBlock* mod_ok_bb = llvm::BasicBlock::Create(ctx, "moda_ok", llvm_func);
            llvm::BasicBlock* mod_merge_bb = llvm::BasicBlock::Create(ctx, "moda_merge", llvm_func);
            builder->CreateCondBr(is_zero, mod_zero_bb, mod_ok_bb);

            // Division by zero path: call runtime helper to raise exception
            builder->SetInsertPoint(mod_zero_bb);
            auto helper = module.getOrInsertFunction("qore_rt_mod_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* exc_result = builder->CreateCall(helper, {l_int, r_int, xsink_arg});
            builder->CreateBr(mod_merge_bb);

            // Normal path: native modulo (result is raw i64)
            builder->SetInsertPoint(mod_ok_bb);
            llvm::Value* mod_result = builder->CreateSRem(l_int, r_int);
            builder->CreateBr(mod_merge_bb);

            // Merge results
            builder->SetInsertPoint(mod_merge_bb);
            llvm::PHINode* phi = builder->CreatePHI(i64_type, 2, "moda_result");
            phi->addIncoming(exc_result, mod_zero_bb);
            phi->addIncoming(mod_result, mod_ok_bb);
            values[inst->result.id] = phi;
            return true;
        }
        // === Optimized fold operations (native LLVM loops) ===
        case QoreIROpcode::FoldlSumInt:
            return emitFoldLoop(inst, module, llvm_func, "foldl_sum", false,
                    llvm::ConstantInt::get(i64_type, 0), false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateAdd(acc, elem);
                    }, error);
        case QoreIROpcode::FoldlSumFloat:
            return emitFoldLoop(inst, module, llvm_func, "foldl_sumf", true,
                    llvm::ConstantFP::get(double_type, 0.0), false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateFAdd(acc, elem);
                    }, error);
        case QoreIROpcode::FoldlProdInt:
            return emitFoldLoop(inst, module, llvm_func, "foldl_prod", false,
                    llvm::ConstantInt::get(i64_type, 1), false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateMul(acc, elem);
                    }, error);
        case QoreIROpcode::FoldlProdFloat:
            return emitFoldLoop(inst, module, llvm_func, "foldl_prodf", true,
                    llvm::ConstantFP::get(double_type, 1.0), false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateFMul(acc, elem);
                    }, error);
        case QoreIROpcode::FoldlDiffInt:
            return emitFoldLoop(inst, module, llvm_func, "foldl_diff", false,
                    nullptr, false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateSub(acc, elem);
                    }, error);
        case QoreIROpcode::FoldlDiffFloat:
            return emitFoldLoop(inst, module, llvm_func, "foldl_difff", true,
                    nullptr, false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateFSub(acc, elem);
                    }, error);
        case QoreIROpcode::FoldlMinInt:
            return emitFoldLoop(inst, module, llvm_func, "foldl_min", false,
                    nullptr, true,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        llvm::Value* cmp = builder->CreateICmpSLT(elem, acc);
                        return builder->CreateSelect(cmp, elem, acc);
                    }, error);
        case QoreIROpcode::FoldlMinFloat:
            return emitFoldLoop(inst, module, llvm_func, "foldl_minf", true,
                    nullptr, true,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        llvm::Value* cmp = builder->CreateFCmpOLT(elem, acc);
                        return builder->CreateSelect(cmp, elem, acc);
                    }, error);
        case QoreIROpcode::FoldlMaxInt:
            return emitFoldLoop(inst, module, llvm_func, "foldl_max", false,
                    nullptr, true,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        llvm::Value* cmp = builder->CreateICmpSGT(elem, acc);
                        return builder->CreateSelect(cmp, elem, acc);
                    }, error);
        case QoreIROpcode::FoldlMaxFloat:
            return emitFoldLoop(inst, module, llvm_func, "foldl_maxf", true,
                    nullptr, true,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        llvm::Value* cmp = builder->CreateFCmpOGT(elem, acc);
                        return builder->CreateSelect(cmp, elem, acc);
                    }, error);
        // === Optimized foldr operations (native LLVM loops) ===
        // Sum and Prod are commutative, so forward iteration gives same result as reverse
        case QoreIROpcode::FoldrSumInt:
            return emitFoldLoop(inst, module, llvm_func, "foldr_sum", false,
                    llvm::ConstantInt::get(i64_type, 0), false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateAdd(acc, elem);
                    }, error);
        case QoreIROpcode::FoldrSumFloat:
            return emitFoldLoop(inst, module, llvm_func, "foldr_sumf", true,
                    llvm::ConstantFP::get(double_type, 0.0), false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateFAdd(acc, elem);
                    }, error);
        case QoreIROpcode::FoldrProdInt:
            return emitFoldLoop(inst, module, llvm_func, "foldr_prod", false,
                    llvm::ConstantInt::get(i64_type, 1), false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateMul(acc, elem);
                    }, error);
        case QoreIROpcode::FoldrProdFloat:
            return emitFoldLoop(inst, module, llvm_func, "foldr_prodf", true,
                    llvm::ConstantFP::get(double_type, 1.0), false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateFMul(acc, elem);
                    }, error);
        case QoreIROpcode::FoldrDiffInt:
            return emitFoldReverseLoop(inst, module, llvm_func, "foldr_diff", false,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateSub(acc, elem);
                    }, error);
        case QoreIROpcode::FoldrDiffFloat:
            return emitFoldReverseLoop(inst, module, llvm_func, "foldr_difff", true,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        return builder->CreateFSub(acc, elem);
                    }, error);
        // Min/Max are commutative (order-independent), so forward iteration gives same result
        case QoreIROpcode::FoldrMinInt:
            return emitFoldLoop(inst, module, llvm_func, "foldr_min", false,
                    nullptr, true,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        llvm::Value* cmp = builder->CreateICmpSLT(elem, acc);
                        return builder->CreateSelect(cmp, elem, acc);
                    }, error);
        case QoreIROpcode::FoldrMinFloat:
            return emitFoldLoop(inst, module, llvm_func, "foldr_minf", true,
                    nullptr, true,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        llvm::Value* cmp = builder->CreateFCmpOLT(elem, acc);
                        return builder->CreateSelect(cmp, elem, acc);
                    }, error);
        case QoreIROpcode::FoldrMaxInt:
            return emitFoldLoop(inst, module, llvm_func, "foldr_max", false,
                    nullptr, true,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        llvm::Value* cmp = builder->CreateICmpSGT(elem, acc);
                        return builder->CreateSelect(cmp, elem, acc);
                    }, error);
        case QoreIROpcode::FoldrMaxFloat:
            return emitFoldLoop(inst, module, llvm_func, "foldr_maxf", true,
                    nullptr, true,
                    [this](llvm::Value* acc, llvm::Value* elem) {
                        llvm::Value* cmp = builder->CreateFCmpOGT(elem, acc);
                        return builder->CreateSelect(cmp, elem, acc);
                    }, error);
        // === Optimized map operations (native runtime helpers) ===
        case QoreIROpcode::MapScaleInt: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_int = ensureIntTypeInline(scale, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_map_scale_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_int});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MapScaleFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_float = ensureFloatType(scale, inst->operands[1].id, module);
            auto helper = module.getOrInsertFunction("qore_rt_map_scale_float",
                    llvm::FunctionType::get(i64_type, {i64_type, double_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_float});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MapOffsetInt: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* offset = getVal(inst->operands[1].id, error);
            if (!list || !offset) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* offset_int = ensureIntTypeInline(offset, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_map_offset_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, offset_int});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MapOffsetFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* offset = getVal(inst->operands[1].id, error);
            if (!list || !offset) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* offset_float = ensureFloatType(offset, inst->operands[1].id, module);
            auto helper = module.getOrInsertFunction("qore_rt_map_offset_float",
                    llvm::FunctionType::get(i64_type, {i64_type, double_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, offset_float});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MapSquareInt: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_map_square_int",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MapSquareFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_map_square_float",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        // === Fully specialized hash-key map operations ===
        case QoreIROpcode::MapHashKeyValue: {
            const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Constant* key_const = builder->CreateGlobalString(mhk->key1, "map_hk_key");
            auto helper = module.getOrInsertFunction("qore_rt_map_hash_key_value",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, key_const});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MapHashKeyInt: {
            const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Constant* key_const = builder->CreateGlobalString(mhk->key1, "map_hk_key");
            auto helper = module.getOrInsertFunction("qore_rt_map_hash_key_int",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, key_const});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MapHashKeyOffsetInt: {
            const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
            auto* list = getVal(inst->operands[0].id, error);
            auto* offset = getVal(inst->operands[1].id, error);
            if (!list || !offset) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* offset_int = ensureIntTypeInline(offset, inst->operands[1].id);
            llvm::Constant* key_const = builder->CreateGlobalString(mhk->key1, "map_hk_key");
            auto helper = module.getOrInsertFunction("qore_rt_map_hash_key_offset_int",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, key_const, offset_int});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::MapHashKeyScaleInt: {
            const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_int = ensureIntTypeInline(scale, inst->operands[1].id);
            llvm::Constant* key_const = builder->CreateGlobalString(mhk->key1, "map_hk_key");
            auto helper = module.getOrInsertFunction("qore_rt_map_hash_key_scale_int",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, key_const, scale_int});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::HashMapTwoKeys: {
            const auto* mhk = static_cast<const QoreIRMapHashKeyInstruction*>(inst);
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Constant* key1_const = builder->CreateGlobalString(mhk->key1, "map_hk_key1");
            llvm::Constant* key2_const = builder->CreateGlobalString(mhk->key2, "map_hk_key2");
            auto helper = module.getOrInsertFunction("qore_rt_hash_map_two_keys",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, key1_const, key2_const});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        // === Optimized select operations (native runtime helpers) ===
        case QoreIROpcode::SelectPositiveInt: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_select_positive_int",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::SelectPositiveFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_select_positive_float",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::SelectNonZeroInt: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_select_nonzero_int",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::SelectNonZeroFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_select_nonzero_float",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        // === Fused map+select operations (runtime helpers) ===
        case QoreIROpcode::FusedMapSelectScalePositiveInt: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_int = ensureIntTypeInline(scale, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_select_scale_positive_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_int});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::FusedMapSelectScalePositiveFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_float = ensureFloatType(scale, inst->operands[1].id, module);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_select_scale_positive_float",
                    llvm::FunctionType::get(i64_type, {i64_type, double_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_float});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::FusedMapSelectOffsetPositiveInt: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* offset = getVal(inst->operands[1].id, error);
            if (!list || !offset) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* offset_int = ensureIntTypeInline(offset, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_select_offset_positive_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, offset_int});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::FusedMapSelectOffsetPositiveFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* offset = getVal(inst->operands[1].id, error);
            if (!list || !offset) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* offset_float = ensureFloatType(offset, inst->operands[1].id, module);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_select_offset_positive_float",
                    llvm::FunctionType::get(i64_type, {i64_type, double_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, offset_float});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::FusedMapSelectSquarePositiveInt: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_select_square_positive_int",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::FusedMapSelectSquarePositiveFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_select_square_positive_float",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        // Fused map+foldl operations (single pass, no intermediate list)
        case QoreIROpcode::FusedMapFoldlSumScaleInt: {
            // foldl $1 + $2, (map $1 * c, list) -> sum(list[i] * c)
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_int = ensureIntTypeInline(scale, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_foldl_sum_scale_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_int});
            values[inst->result.id] = result;
            // Result is unboxed int, mark not nanboxed
            return true;
        }
        case QoreIROpcode::FusedMapFoldlSumScaleFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_float = ensureFloatType(scale, inst->operands[1].id, module);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_foldl_sum_scale_float",
                    llvm::FunctionType::get(double_type, {i64_type, double_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_float});
            values[inst->result.id] = result;
            // Result is unboxed float, mark not nanboxed
            return true;
        }
        case QoreIROpcode::FusedMapFoldlSumSquareInt: {
            // foldl $1 + $2, (map $1 * $1, list) -> sum(list[i]^2)
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_foldl_sum_square_int",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            // Result is unboxed int
            return true;
        }
        case QoreIROpcode::FusedMapFoldlSumSquareFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_foldl_sum_square_float",
                    llvm::FunctionType::get(double_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            // Result is unboxed float
            return true;
        }
        case QoreIROpcode::FusedMapFoldlProdScaleInt: {
            // foldl $1 * $2, (map $1 * c, list) -> prod(list[i] * c)
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_int = ensureIntTypeInline(scale, inst->operands[1].id);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_foldl_prod_scale_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_int});
            values[inst->result.id] = result;
            // Result is unboxed int
            return true;
        }
        case QoreIROpcode::FusedMapFoldlProdScaleFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_float = ensureFloatType(scale, inst->operands[1].id, module);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_foldl_prod_scale_float",
                    llvm::FunctionType::get(double_type, {i64_type, double_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_float});
            values[inst->result.id] = result;
            // Result is unboxed float
            return true;
        }

        // === Division/Modulo Any compound assignments (keep using runtime) ===
        case QoreIROpcode::DivAssignAny:
        case QoreIROpcode::ModAssignAny:
        // === Higher-order operations (binary via runtime) ===
        case QoreIROpcode::FoldlAny:
        case QoreIROpcode::FoldlInt:
        case QoreIROpcode::FoldlFloat:
        case QoreIROpcode::FoldrAny:
        case QoreIROpcode::FoldrInt:
        case QoreIROpcode::FoldrFloat:
        case QoreIROpcode::MapAny:
        case QoreIROpcode::MapInt:
        case QoreIROpcode::MapFloat:
        case QoreIROpcode::SelectAny:
        case QoreIROpcode::SelectInt:
        case QoreIROpcode::SelectFloat:
        // === Range operations (binary via runtime) ===
        case QoreIROpcode::RangeAny:
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat:
        case QoreIROpcode::RangeDate: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            auto helper = module.getOrInsertFunction("qore_rt_binary_op",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper,
                    {opcode_val, lhs_boxed, rhs_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        // === Range slice operations (ternary: source, start, stop) ===
        case QoreIROpcode::RangeSliceAny:
        case QoreIROpcode::RangeSliceInt:
        case QoreIROpcode::RangeSliceFloat: {
            auto* first = getVal(inst->operands[0].id, error);
            auto* second = getVal(inst->operands[1].id, error);
            auto* third = getVal(inst->operands[2].id, error);
            if (!first || !second || !third) { return false; }
            llvm::Value* first_boxed = boxValue(first, inst->operands[0].id);
            llvm::Value* second_boxed = boxValue(second, inst->operands[1].id);
            llvm::Value* third_boxed = boxValue(third, inst->operands[2].id);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            auto helper = module.getOrInsertFunction("qore_rt_ternary_op",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper,
                    {opcode_val, first_boxed, second_boxed, third_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Regex operations (use pre-evaluated operand to avoid double-evaluation) ===
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList: {
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            llvm::Value* result;

            if (!inst->operands.empty()) {
                // Use pre-evaluated operand with qore_rt_regex_op_with_operand
                auto* operand = getVal(inst->operands[0].id, error);
                if (!operand) { return false; }
                llvm::Value* operand_boxed = boxValue(operand, inst->operands[0].id);
                llvm::Value* opcode_val = llvm::ConstantInt::get(i32_type,
                        static_cast<int>(inst->opcode));

                if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_regex_op_with_operand_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i32_type, i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg, opcode_val,
                            llvm::ConstantInt::get(i32_type, slot), operand_boxed, xsink_arg});
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_regex_op_with_operand",
                            llvm::FunctionType::get(i64_type,
                                {i32_type, i64_type, i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {opcode_val, expr_const, operand_boxed,
                            xsink_arg});
                }
            } else {
                // Fallback to qore_rt_invoke_expr if no operands
                if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                            llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {expr_const, xsink_arg});
                }
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Switch regex match (uses CaseNodeRegex::matches()) ===
        case QoreIROpcode::SwitchRegexMatch: {
            const auto* regex_inst = static_cast<const QoreIRSwitchRegexMatchInstruction*>(inst);
            if (regex_inst->operands.empty() || !regex_inst->regex_case) {
                error = "SwitchRegexMatch requires operand and regex_case";
                return false;
            }
            auto* operand = getVal(regex_inst->operands[0].id, error);
            if (!operand) { return false; }
            llvm::Value* operand_boxed = boxValue(operand, regex_inst->operands[0].id);

            // In AOT mode, use slot-based indirection; in JIT mode, embed pointer directly
            llvm::Value* regex_case_ptr;
            if (aot_mode) {
                // AOT: use slot-based indirection via context array
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getRegexCaseSlot(
                        reinterpret_cast<const void*>(regex_inst->regex_case));
                auto get_helper = module.getOrInsertFunction("qore_rt_get_regex_case_aot",
                        llvm::FunctionType::get(ptr_type, {ptr_type, i32_type}, false));
                llvm::Value* rc_ptr = builder->CreateCall(get_helper,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot)});
                // qore_rt_switch_regex_match takes i64 for first arg — cast
                regex_case_ptr = builder->CreatePtrToInt(rc_ptr, i64_type);
            } else {
                // JIT: embed pointer directly (valid — same process)
                regex_case_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(regex_inst->regex_case));
            }

            auto helper = module.getOrInsertFunction("qore_rt_switch_regex_match",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper,
                    {regex_case_ptr, operand_boxed, xsink_arg});

            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Lvalue-modifying expression ops (modify variables via AST LValueHelper) ===
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::ExtractAny:
        case QoreIROpcode::ExtractList:
        case QoreIROpcode::ExtractString:
        case QoreIROpcode::ExtractBinary:
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::RemoveList:
        case QoreIROpcode::RemoveHash:
        case QoreIROpcode::RemoveObject:
        case QoreIROpcode::RemoveString:
        case QoreIROpcode::RemoveBinary:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::RegexSubstString:
        case QoreIROpcode::TrimAny:
        case QoreIROpcode::TrimString:
        case QoreIROpcode::ChompAny:
        case QoreIROpcode::ChompString:
        case QoreIROpcode::TransliterateAny:
        case QoreIROpcode::TransliterateString:
        case QoreIROpcode::ListAssignAny: {
            // These ops modify variables via LValueHelper through the runtime variable stack;
            // reload all local allocas after execution to pick up modified values
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            // Clear reload trackers before AST delegation to prevent refcount
            // inflation → copy-on-write → O(n²) for container ops in loops
            clearAllLocalReloadTrackers(module, llvm_func);
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {expr_const, xsink_arg});
            }
            reloadAllLocalsFromRuntime(module, llvm_func);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Exists: use pre-evaluated operand to avoid double-evaluation ===
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool: {
            // Use the pre-evaluated operand[0] — do NOT re-evaluate the AST expr
            // which would double-evaluate the inner expression (e.g., function call)
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            // exists returns true if value is NOT NOTHING
            llvm::Value* is_not_nothing = builder->CreateICmpNE(val_boxed,
                    llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            llvm::Value* result = boxBool(is_not_nothing);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            // No trackResultForCleanup — boolean value, no heap allocation
            // No emitExceptionCheck — exists cannot throw
            return true;
        }

        // === Pure expression ops (no variable modification) ===
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt: {
            // Pure expression ops — delegate to qore_rt_invoke_expr via the AST node
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {expr_const, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        // DotEval opcodes: use pre-evaluated base to avoid double-evaluation
        // Phase 5b: Try specialized hash key access and list index access before generic path
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject: {
            // Try specialized hash key access for DotEvalHash/DotEvalAny
            if ((inst->opcode == QoreIROpcode::DotEvalHash
                    || inst->opcode == QoreIROpcode::DotEvalAny)
                    && tryEmitHashKeyAccess(inst, module, llvm_func)) {
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(values[inst->result.id], inst->result.id, llvm_func);
                emitExceptionCheck(module, llvm_func, inst);
                return true;
            }
            // Try specialized list index access for DotEvalAny
            if (inst->opcode == QoreIROpcode::DotEvalAny
                    && tryEmitListIndexAccess(inst, module, llvm_func)) {
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(values[inst->result.id], inst->result.id, llvm_func);
                emitExceptionCheck(module, llvm_func, inst);
                return true;
            }
            // Use pre-evaluated base with qore_rt_dot_eval_with_base
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            llvm::Value* result;
            if (!inst->operands.empty()) {
                std::string dot_err;
                auto* base = getVal(inst->operands[0].id, dot_err);
                if (base) {
                    llvm::Value* base_boxed = boxValue(base, inst->operands[0].id);
                    if (aot_mode) {
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                        auto helper = module.getOrInsertFunction("qore_rt_dot_eval_with_base_aot",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, i32_type, i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {aot_ctx_arg,
                                llvm::ConstantInt::get(i32_type, slot), base_boxed, xsink_arg});
                    } else {
                        llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                        auto helper = module.getOrInsertFunction("qore_rt_dot_eval_with_base",
                                llvm::FunctionType::get(i64_type,
                                    {i64_type, i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {expr_const, base_boxed, xsink_arg});
                    }
                    // DotEval method calls can modify locals through side effects
                    reloadAllLocalsFromRuntime(module, llvm_func);
                    values[inst->result.id] = result;
                    nanboxed_values.insert(inst->result.id);
                    trackResultForCleanup(result, inst->result.id, llvm_func);
                    emitExceptionCheck(module, llvm_func, inst);
                    return true;
                }
            }
            // Fallback: no operands available, use qore_rt_invoke_expr
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {expr_const, xsink_arg});
            }
            // Reload after fallback — AST eval can modify any local
            reloadAllLocalsFromRuntime(module, llvm_func);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        // Cast operations: native cast with pre-evaluated inner value (operand[0])
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
        case QoreIROpcode::CastAny: {
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            // operand[0] is the pre-evaluated inner value
            auto* inner_val = getVal(inst->operands[0].id, error);
            if (!inner_val) { return false; }
            llvm::Value* inner_boxed = boxValue(inner_val, inst->operands[0].id);
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_cast_with_inner_aot",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), inner_boxed, xsink_arg});
            } else {
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_cast_with_inner",
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {expr_const, inner_boxed, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        // Remaining expression-based ops (non-DotEval)
        case QoreIROpcode::MapSelectList:
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::HashMap:
        case QoreIROpcode::HashMapSelect:
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMapSelectAny:
        case QoreIROpcode::InvokeSimError: {
            // These are expression-based ops — delegate to qore_rt_invoke_expr via the AST node
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            } else {
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {expr_const, xsink_arg});
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Statement operations ===
        case QoreIROpcode::Foreach: {
            const auto* sinst = static_cast<const QoreIRForeachInstruction*>(inst);
            if (aot_mode) {
                // AOT mode: use slot index via qore_rt_exec_foreach_aot()
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getStmtSlot(
                        reinterpret_cast<const void*>(sinst->stmt));
                auto helper = module.getOrInsertFunction("qore_rt_exec_foreach_aot",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type}, false));
                llvm::Value* result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                values[inst->result.id] = result;
            } else {
                // JIT mode: embed raw ForEachStatement* pointer
                llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                        static_cast<int>(inst->opcode));
                llvm::Value* stmt_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(sinst->stmt));
                llvm::Value* stmt_as_ptr = builder->CreateIntToPtr(stmt_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_exec_statement",
                        llvm::FunctionType::get(i64_type,
                            {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false));
                llvm::Value* result = builder->CreateCall(helper,
                        {opcode_val, stmt_as_ptr, xsink_arg});
                values[inst->result.id] = result;
            }
            nanboxed_values.insert(inst->result.id);
            // Foreach-reference executes through the AST path and can modify
            // any local variable; reload all local allocas after execution.
            reloadAllLocalsFromRuntime(module, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::OnBlockExit: {
            const auto* sinst = static_cast<const QoreIROnBlockExitInstruction*>(inst);
            // Register the on_block_exit handler for deferred execution at function exit.
            // The IR interpreter records handlers and executes them at return time;
            // the JIT does the same via thread-local runtime helpers.
            llvm::Value* type_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(sinst->stmt->getType()));
            StatementBlock* code = sinst->stmt->getCode();
            if (aot_mode) {
                // AOT mode: use slot index instead of raw pointer
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getStmtSlot(
                        reinterpret_cast<const void*>(code));
                auto helper = module.getOrInsertFunction("qore_rt_push_on_block_exit_aot",
                        llvm::FunctionType::get(void_type,
                            {ptr_type, i32_type, llvm::Type::getInt32Ty(ctx)}, false));
                builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), type_val});
            } else if (sinst->handler_ir) {
                // Try to compile handler body as a native LLVM function in the same module
                QoreIRToLLVM handler_lowering(ctx);
                // Share debug info so the handler is in the same compile unit
                if (active_di_builder && di_cu) {
                    handler_lowering.setSharedDebugInfo(active_di_builder, di_cu);
                }
                std::string handler_error;
                bool handler_compiled = handler_lowering.lowerFunction(
                        *sinst->handler_ir, module, handler_error);
                llvm::Function* handler_fn = nullptr;
                if (handler_compiled) {
                    handler_fn = module.getFunction(sinst->handler_ir->name);
                }
                if (handler_compiled && handler_fn) {
                    // Push compiled handler via native runtime helper
                    if (getenv("QORE_LLVM_DEBUG")) {
                        fprintf(stderr, "QoreIRToLLVM: compiled on_block_exit handler '%s' "
                                "to native LLVM\n", sinst->handler_ir->name.c_str());
                    }
                    llvm::Value* code_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(code));
                    llvm::Value* code_as_ptr = builder->CreateIntToPtr(code_ptr, ptr_type);
                    llvm::Value* handler_func_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(sinst->handler_ir.get()));
                    llvm::Value* handler_func_as_ptr = builder->CreateIntToPtr(
                            handler_func_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_push_compiled_handler",
                            llvm::FunctionType::get(void_type,
                                {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type, ptr_type},
                                false));
                    builder->CreateCall(helper,
                            {type_val, code_as_ptr, handler_fn, handler_func_as_ptr});
                } else {
                    // LLVM lowering failed — fall back to IR interpreter path
                    if (getenv("QORE_LLVM_DEBUG")) {
                        fprintf(stderr, "QoreIRToLLVM: handler '%s' LLVM lowering failed: "
                                "%s; falling back to IR interpreter\n",
                                sinst->handler_ir->name.c_str(), handler_error.c_str());
                    }
                    llvm::Value* code_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(code));
                    llvm::Value* code_as_ptr = builder->CreateIntToPtr(code_ptr, ptr_type);
                    llvm::Value* handler_ir_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(sinst->handler_ir.get()));
                    llvm::Value* handler_ir_as_ptr = builder->CreateIntToPtr(
                            handler_ir_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_push_on_block_exit_ir",
                            llvm::FunctionType::get(void_type,
                                {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false));
                    builder->CreateCall(helper, {type_val, code_as_ptr, handler_ir_as_ptr});
                }
            } else {
                // AST fallback
                llvm::Value* code_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(code));
                llvm::Value* code_as_ptr = builder->CreateIntToPtr(code_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_push_on_block_exit",
                        llvm::FunctionType::get(void_type,
                            {llvm::Type::getInt32Ty(ctx), ptr_type}, false));
                builder->CreateCall(helper, {type_val, code_as_ptr});
            }
            // OnBlockExit produces NOTHING as its result
            values[inst->result.id] = llvm::ConstantInt::get(i64_type, VAL_NOTHING);
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::Context: {
            const auto* sinst = static_cast<const QoreIRContextInstruction*>(inst);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            llvm::Value* stmt_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(sinst->stmt));
            llvm::Value* stmt_as_ptr = builder->CreateIntToPtr(stmt_ptr, ptr_type);
            auto helper = module.getOrInsertFunction("qore_rt_exec_statement",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            // Context executes through the AST path and can modify locals
            reloadAllLocalsFromRuntime(module, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::Summarize: {
            const auto* sinst = static_cast<const QoreIRSummarizeInstruction*>(inst);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            llvm::Value* stmt_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(sinst->stmt));
            llvm::Value* stmt_as_ptr = builder->CreateIntToPtr(stmt_ptr, ptr_type);
            auto helper = module.getOrInsertFunction("qore_rt_exec_statement",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            // Summarize executes through the AST path and can modify locals
            reloadAllLocalsFromRuntime(module, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::Debug: {
            const auto* sinst = static_cast<const QoreIRDebugInstruction*>(inst);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            llvm::Value* stmt_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(sinst->stmt));
            llvm::Value* stmt_as_ptr = builder->CreateIntToPtr(stmt_ptr, ptr_type);
            auto helper = module.getOrInsertFunction("qore_rt_exec_statement",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            reloadAllLocalsFromRuntime(module, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::Assert: {
            const auto* sinst = static_cast<const QoreIRAssertInstruction*>(inst);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            llvm::Value* stmt_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(sinst->stmt));
            llvm::Value* stmt_as_ptr = builder->CreateIntToPtr(stmt_ptr, ptr_type);
            auto helper = module.getOrInsertFunction("qore_rt_exec_statement",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            reloadAllLocalsFromRuntime(module, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::ThreadExit: {
            auto helper = module.getOrInsertFunction("qore_rt_thread_exit",
                    llvm::FunctionType::get(void_type, {ptr_type}, false));
            builder->CreateCall(helper, {xsink_arg});
            // Thread exit raises an exception; execute on_block_exit, uninstantiate locals and return
            emitOnBlockExitExec(module);
            emitIteratorCleanup(module);
            emitPreinstantiatedCleanup(module);
            emitInvokeCleanup(module);
            emitLocalUninstantiation(module);
            builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            return true;
        }

        // === Guard type ===
        case QoreIROpcode::GuardType: {
            const auto* ginst = static_cast<const QoreIRGuardInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* boxed = boxValue(val, inst->operands[0].id);
            // Pass the QoreTypeInfo* as a constant pointer
            llvm::Value* ti_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(ginst->type_info));
            llvm::Value* ti_as_ptr = builder->CreateIntToPtr(ti_ptr, ptr_type);
            auto helper = module.getOrInsertFunction("qore_rt_guard_type",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            llvm::Value* guard_result = builder->CreateCall(helper, {boxed, ti_as_ptr});
            llvm::Value* guard_pass = builder->CreateICmpNE(guard_result,
                    llvm::ConstantInt::get(i64_type, 0));
            if (ginst->deopt_target) {
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                llvm::BasicBlock* deopt = getOrCreateJitDeoptBlock(module, llvm_func);
                builder->CreateCondBr(guard_pass, cont, deopt);
                builder->SetInsertPoint(cont);
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = guard_pass;
            }
            return true;
        }

        // === Scope enter/exit for on_exit handler management ===
        case QoreIROpcode::ScopeEnter: {
            const auto* sinst = static_cast<const QoreIRScopeEnterInstruction*>(inst);
            // Save the current on_block_exit handler count for this scope
            auto obe_count_fn = module.getOrInsertFunction("qore_rt_get_on_block_exit_count",
                    llvm::FunctionType::get(i64_type, {}, false));
            llvm::Value* count = builder->CreateCall(obe_count_fn, {});
            scope_obe_counts[sinst->scope_id] = count;
            // ScopeEnter produces NOTHING as its result
            if (inst->result.isValid()) {
                values[inst->result.id] = llvm::ConstantInt::get(i64_type, VAL_NOTHING);
                nanboxed_values.insert(inst->result.id);
            }
            return true;
        }
        case QoreIROpcode::ScopeExit: {
            const auto* sinst = static_cast<const QoreIRScopeExitInstruction*>(inst);
            // Execute on_block_exit handlers registered since the matching ScopeEnter
            auto it = scope_obe_counts.find(sinst->scope_id);
            if (it != scope_obe_counts.end()) {
                llvm::Value* saved_count = it->second;
                // The runtime helper determines error state by checking xsink->isException()
                // at the time of the call, which correctly handles on_error/on_success semantics
                auto helper = module.getOrInsertFunction("qore_rt_exec_on_block_exit",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                builder->CreateCall(helper, {saved_count, xsink_arg});
                // On-block-exit handlers execute through the AST path and can modify
                // any local variable on the thread-local stack. Reload all local
                // allocas so subsequent LoadLocal sees the updated values.
                reloadAllLocalsFromRuntime(module, llvm_func);
            }
            // ScopeExit produces NOTHING as its result
            if (inst->result.isValid()) {
                values[inst->result.id] = llvm::ConstantInt::get(i64_type, VAL_NOTHING);
                nanboxed_values.insert(inst->result.id);
            }
            return true;
        }

        // === Iterator operations ===
        case QoreIROpcode::IteratorCreate: {
            const auto* iter_inst = static_cast<const QoreIRIteratorCreateInstruction*>(inst);
            // Get the iterable value (NaN-boxed)
            auto* iterable_val = getVal(iter_inst->iterable.id, error);
            if (!iterable_val) {
                return false;
            }
            // Box the iterable value if needed
            llvm::Value* iterable_boxed;
            if (nanboxed_values.count(iter_inst->iterable.id)) {
                iterable_boxed = iterable_val;
            } else if (iterable_val->getType() == i64_type) {
                iterable_boxed = boxIntInline(iterable_val);
            } else if (iterable_val->getType() == double_type) {
                iterable_boxed = boxFloat(iterable_val);
            } else {
                error = "unsupported iterable type for IteratorCreate";
                return false;
            }

            llvm::Value* result;
            if (aot_mode) {
                // AOT mode: use slot-based lookup for iterator_func pointer
                // Use -1 as sentinel when iterator_func is null
                int32_t slot = -1;
                if (iter_inst->iterator_func) {
                    uint64_t func_bits = reinterpret_cast<uint64_t>(iter_inst->iterator_func);
                    slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(func_bits);
                }
                auto helper = module.getOrInsertFunction("qore_rt_iterator_create_aot",
                        llvm::FunctionType::get(ptr_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), iterable_boxed, xsink_arg});
            } else {
                // JIT mode: embed iterator_func pointer directly
                llvm::Value* iter_func_ptr;
                if (iter_inst->iterator_func) {
                    iter_func_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(iter_inst->iterator_func));
                    iter_func_ptr = builder->CreateIntToPtr(iter_func_ptr, ptr_type);
                } else {
                    // Create null pointer: i64 0 -> inttoptr -> ptr
                    iter_func_ptr = builder->CreateIntToPtr(
                            llvm::ConstantInt::get(i64_type, 0), ptr_type);
                }
                // Call qore_rt_iterator_create(iterable, iterator_func, xsink) -> ptr
                auto helper = module.getOrInsertFunction("qore_rt_iterator_create",
                        llvm::FunctionType::get(ptr_type, {i64_type, ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {iterable_boxed, iter_func_ptr, xsink_arg});
            }
            // Store the iterator pointer as the result
            values[inst->result.id] = result;
            // Don't mark as nanboxed - it's a raw pointer

            // Track active iterator for cleanup on non-normal exit (return/throw
            // inside foreach body before iterator is exhausted).
            // Alloca hoisted to entry block, initialized to nullptr.
            {
                llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                        llvm_func->getEntryBlock().begin());
                llvm::AllocaInst* iter_alloca = ab.CreateAlloca(ptr_type, nullptr, "iter_cleanup");
                ab.CreateStore(llvm::ConstantPointerNull::get(
                        llvm::PointerType::get(ctx, 0)), iter_alloca);
                // Store the iterator pointer in the cleanup alloca
                builder->CreateStore(result, iter_alloca);
                iterator_cleanup_allocas.push_back(iter_alloca);
                // Store the alloca in invoke_alloca_map so IteratorNext can null it on done
                invoke_alloca_map[inst->result.id] = iter_alloca;
            }

            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::IteratorNext: {
            const auto* iter_inst = static_cast<const QoreIRIteratorNextInstruction*>(inst);
            // Get the iterator pointer
            auto* iter_ptr = getVal(iter_inst->iterator.id, error);
            if (!iter_ptr) {
                return false;
            }
            // Ensure iter_ptr is a pointer type
            if (iter_ptr->getType() != ptr_type) {
                if (iter_ptr->getType() == i64_type) {
                    iter_ptr = builder->CreateIntToPtr(iter_ptr, ptr_type);
                } else {
                    error = "IteratorNext: iterator value must be pointer or i64";
                    return false;
                }
            }
            // Hoist alloca to entry block, initialized to VAL_NOTHING for safe
            // decref-before-overwrite on first iteration
            llvm::IRBuilder<> ab_iter(&llvm_func->getEntryBlock(),
                    llvm_func->getEntryBlock().begin());
            llvm::Value* out_val_ptr = ab_iter.CreateAlloca(i64_type, nullptr, "iter_out_val");
            ab_iter.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), out_val_ptr);
            // Decref previous iterator value before overwrite (first iteration
            // decrefs NOTHING which is a no-op; subsequent iterations decref the
            // pair hash / value from the previous iteration)
            auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                    llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
            llvm::Value* old_iter_val = builder->CreateLoad(i64_type, out_val_ptr, "old_iter_val");
            builder->CreateCall(decref_fn, {old_iter_val, xsink_arg});
            // Null out the iterator cleanup alloca BEFORE the call.
            // qore_rt_iterator_next deletes the iterator on done/exception;
            // nulling first ensures emitIteratorCleanup won't double-delete
            // if emitExceptionCheck branches to error_return_block.
            auto iter_alloca_it = invoke_alloca_map.find(iter_inst->iterator.id);
            if (iter_alloca_it != invoke_alloca_map.end()) {
                builder->CreateStore(llvm::ConstantPointerNull::get(
                        llvm::PointerType::get(ctx, 0)), iter_alloca_it->second);
            }
            // Call qore_rt_iterator_next(iter_ptr, out_val_ptr, xsink) -> i64 (1=done, 0=continue)
            // Always writes to out_val_ptr (VAL_NOTHING on done/exception, value on continue)
            auto helper = module.getOrInsertFunction("qore_rt_iterator_next",
                    llvm::FunctionType::get(i64_type, {ptr_type, ptr_type, ptr_type}, false));
            llvm::Value* done_flag = builder->CreateCall(helper, {iter_ptr, out_val_ptr, xsink_arg});
            // Check for exception
            emitExceptionCheck(module, llvm_func, inst);
            // Restore iterator pointer in cleanup alloca if NOT done (iterator still alive)
            if (iter_alloca_it != invoke_alloca_map.end()) {
                llvm::Value* is_not_done = builder->CreateICmpEQ(done_flag,
                        llvm::ConstantInt::get(i64_type, 0));
                llvm::Value* restored_ptr = builder->CreateSelect(is_not_done, iter_ptr,
                        llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx, 0)));
                builder->CreateStore(restored_ptr, iter_alloca_it->second);
            }
            // Load the output value (will be used if not done)
            llvm::Value* out_val = builder->CreateLoad(i64_type, out_val_ptr, "iter_val");
            values[inst->result.id] = out_val;
            nanboxed_values.insert(inst->result.id);
            // Track alloca for cleanup at function exit (decrefs last value or NOTHING)
            invoke_result_allocas.push_back(static_cast<llvm::AllocaInst*>(out_val_ptr));
            // Branch based on done_flag
            auto done_it = block_map.find(iter_inst->done_target);
            auto cont_it = block_map.find(iter_inst->continue_target);
            if (done_it == block_map.end() || cont_it == block_map.end()) {
                error = "IteratorNext: target block not found";
                return false;
            }
            llvm::Value* is_done = builder->CreateICmpNE(done_flag, llvm::ConstantInt::get(i64_type, 0));
            builder->CreateCondBr(is_done, done_it->second, cont_it->second);
            return true;
        }

        // === Reference foreach operations ===
        case QoreIROpcode::RefForeachInit: {
            const auto* rfi = static_cast<const QoreIRRefForeachInitInstruction*>(inst);
            llvm::Value* parse_ref_bits_val;
            if (aot_mode) {
                // AOT: use expression slot for the ParseReferenceNode
                QoreValue expr_val = rfi->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                // Evaluate the ParseReferenceNode via AOT to get a runtime reference
                llvm::Value* ref_val = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                parse_ref_bits_val = ref_val;
            } else {
                // JIT: pass the ParseReferenceNode QoreValue bits directly as a constant
                QoreValue expr_val = rfi->expr;
                uint64_t bits;
                std::memcpy(&bits, &expr_val, sizeof(bits));
                parse_ref_bits_val = llvm::ConstantInt::get(i64_type, bits);
            }
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_init",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {parse_ref_bits_val, xsink_arg});
            values[inst->result.id] = result;
            // State handle is an opaque uint64_t, not nanboxed
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::RefForeachSize: {
            // Get the state handle operand
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {state_val});
            values[inst->result.id] = result;
            // Result is a plain int64, not nanboxed
            return true;
        }
        case QoreIROpcode::RefForeachGetEntry: {
            // operands: state, index
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto* index_val = getVal(inst->operands[1].id, error);
            if (!index_val) { return false; }
            // Ensure index is i64 (it may be a typed int that needs unboxing)
            if (index_val->getType() != i64_type) {
                error = "RefForeachGetEntry: index must be i64";
                return false;
            }
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_get_entry",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {state_val, index_val, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::RefForeachRecord: {
            // operands: state, value
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto* value_val = getVal(inst->operands[1].id, error);
            if (!value_val) { return false; }
            // Box the value if needed — it must be a nanboxed QoreValue
            llvm::Value* value_boxed;
            if (nanboxed_values.count(inst->operands[1].id)) {
                value_boxed = value_val;
            } else if (value_val->getType() == i64_type) {
                value_boxed = boxIntInline(value_val);
            } else if (value_val->getType() == double_type) {
                value_boxed = boxFloat(value_val);
            } else if (value_val->getType() == llvm::Type::getInt1Ty(ctx)) {
                value_boxed = boxBool(value_val);
            } else {
                error = "RefForeachRecord: unsupported value type";
                return false;
            }
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_record",
                    llvm::FunctionType::get(void_type, {i64_type, i64_type, ptr_type}, false));
            builder->CreateCall(helper, {state_val, value_boxed, xsink_arg});
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::RefForeachFinalize: {
            // operands: state, fill_remaining
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto* fill_val = getVal(inst->operands[1].id, error);
            if (!fill_val) { return false; }
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_finalize",
                    llvm::FunctionType::get(void_type, {i64_type, i64_type, ptr_type}, false));
            builder->CreateCall(helper, {state_val, fill_val, xsink_arg});
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::RefForeachCleanup: {
            // operands: state
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_cleanup",
                    llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
            builder->CreateCall(helper, {state_val, xsink_arg});
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        case QoreIROpcode::SwitchCaseMatch: {
            // operands[0] = switch value (NaN-boxed)
            // case_node = pointer to CaseNode (compile-time constant)
            auto* case_inst = static_cast<const QoreIRSwitchCaseMatchInstruction*>(inst);
            auto* switch_val = getVal(inst->operands[0].id, error);
            if (!switch_val) { return false; }
            llvm::Value* switch_boxed = boxValue(switch_val, inst->operands[0].id);
            // Pass CaseNode* as an i64 constant (pointer embedded at compile time)
            auto* case_node_ptr = llvm::ConstantInt::get(i64_type,
                reinterpret_cast<uint64_t>(case_inst->case_node));
            auto* case_node_val = builder->CreateIntToPtr(case_node_ptr, ptr_type);
            auto helper = module.getOrInsertFunction("qore_rt_switch_case_match",
                    llvm::FunctionType::get(i64_type, {ptr_type, i64_type, ptr_type}, false));
            auto* result = builder->CreateCall(helper, {case_node_val, switch_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        default:
            error = "unsupported IR opcode for LLVM lowering: " + std::to_string(static_cast<int>(inst->opcode));
            return false;
    }
}

// Phase 5b: Try to emit specialized hash key access.
// Inspects the AST expr at JIT compile time; if it's a QoreHashObjectDereferenceOperatorNode
// with a constant string key, emits qore_rt_hash_key_access instead of qore_rt_invoke_expr.
bool QoreIRToLLVM::tryEmitHashKeyAccess(const QoreIRInstruction* inst, llvm::Module& module,
        llvm::Function* llvm_func) {
    // Extract expr from either QoreIRExprInstruction or QoreIRInvokeInstruction
    QoreValue expr_val;
    if (inst->opcode == QoreIROpcode::Invoke) {
        expr_val = static_cast<const QoreIRInvokeInstruction*>(inst)->expr;
    } else {
        expr_val = static_cast<const QoreIRExprInstruction*>(inst)->expr;
    }
    if (!expr_val.hasNode()) {
        return false;
    }

    // Check if the expression is a hash key access operator
    const AbstractQoreNode* node = expr_val.getInternalNode();
    if (node->getType() != NT_OPERATOR) {
        return false;
    }

    const char* key_str = nullptr;

    // Check for QoreHashObjectDereferenceOperatorNode: $hash{"key"} syntax
    auto* hash_deref = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node);
    if (hash_deref) {
        // Check if the right side is a constant string key (not a list/hash slice)
        QoreValue right_val = hash_deref->getRight();
        if (right_val.hasNode() && right_val.getType() == NT_STRING) {
            key_str = right_val.get<const QoreStringNode>()->c_str();
        }
    }

    // Check for QoreDotEvalOperatorNode: $hash.key syntax
    if (!key_str) {
        auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(node);
        if (dot_eval) {
            MethodCallNode* m = dot_eval->getMethodCall();
            // Only handle simple hash key access (no resolved class/method, no args)
            // Also verify the source type is a hash type — if the source is "auto" or
            // another non-hash type, this could be a pseudo-method call (e.g., $1.toString())
            // that would be incorrectly optimized as a hash key access
            if (m && !m->getClass() && !m->getMethod() && m->getRawName()
                    && (!m->getArgs() || m->getArgs()->empty())
                    && QoreTypeInfo::isHashType(m->getSourceType())) {
                // For optional types (*hash<auto>), pseudo-methods may not be resolved at
                // parse time. Check if the name matches a hash pseudo-method to avoid
                // incorrectly treating e.g. h.keys() as hash key access "keys".
                // Use qore_pseudo_get_class + findLocalMethod instead of
                // pseudo_classes_find_method to avoid runtime_get_class() TLS access,
                // which crashes when called from the background JIT compilation thread.
                const QoreClass* pqc = qore_pseudo_get_class(NT_HASH);
                if (!pqc || !pqc->findLocalMethod(m->getRawName())) {
                    key_str = m->getRawName();
                }
            }
        }
    }

    if (!key_str) {
        return false;
    }

    // The left operand is an expression that we need to evaluate.
    // We still need the AST evaluation for the left side, so we emit:
    //   %left = call qore_rt_invoke_expr(left_expr_bits, xsink)
    //   %result = call qore_rt_hash_key_access(%left, key_ptr, xsink)
    //   call qore_rt_decref(%left, xsink)  ; release the evaluated left value
    //
    // But this would be *more* expensive than just evaluating the whole expression.
    // Instead, we use the operand from the IR instruction if available.
    // DotEvalHash/DotEvalAny instructions have their operand as operands[0].
    if (inst->operands.empty()) {
        return false;
    }

    std::string ignored_err;
    auto* obj_val = getVal(inst->operands[0].id, ignored_err);
    if (!obj_val) {
        return false;
    }

    // Box the operand if needed
    llvm::Value* obj_boxed = boxValue(obj_val, inst->operands[0].id);

    // Create global constant for the key string
    llvm::Constant* key_const = builder->CreateGlobalString(key_str, "hash_key");

    // Call qore_rt_hash_key_access(obj_boxed, key_ptr, xsink)
    auto helper = module.getOrInsertFunction("qore_rt_hash_key_access",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
    values[inst->result.id] = builder->CreateCall(helper, {obj_boxed, key_const, xsink_arg});
    return true;
}

// Phase 5b: Try to emit specialized list index access.
// Inspects the AST expr at JIT compile time; if it's a QoreSquareBracketsOperatorNode
// with a known integer index operand, emits qore_rt_list_index_access.
bool QoreIRToLLVM::tryEmitListIndexAccess(const QoreIRInstruction* inst, llvm::Module& module,
        llvm::Function* llvm_func) {
    const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
    if (!expr_inst->expr.hasNode()) {
        return false;
    }

    const AbstractQoreNode* node = expr_inst->expr.getInternalNode();
    if (node->getType() != NT_OPERATOR) {
        return false;
    }

    auto* sq_brackets = dynamic_cast<const QoreSquareBracketsOperatorNode*>(node);
    if (!sq_brackets) {
        return false;
    }

    // We need two operands: the list value and the index value
    if (inst->operands.size() < 2) {
        return false;
    }

    std::string ignored_err;
    auto* list_val = getVal(inst->operands[0].id, ignored_err);
    auto* idx_val = getVal(inst->operands[1].id, ignored_err);
    if (!list_val || !idx_val) {
        return false;
    }

    // Box the list operand and unbox the index
    llvm::Value* list_boxed = boxValue(list_val, inst->operands[0].id);

    // The index needs to be an i64 (native int)
    llvm::Value* idx_int;
    if (idx_val->getType() == i64_type) {
        idx_int = ensureIntTypeInline(idx_val, inst->operands[1].id);
    } else {
        return false;  // Can't determine index type
    }

    auto helper = module.getOrInsertFunction("qore_rt_list_index_access",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
    values[inst->result.id] = builder->CreateCall(helper, {list_boxed, idx_int, xsink_arg});
    return true;
}

// Phase 5b: Emit inline LLVM fast-path for .any arithmetic.
// Checks if both operands are int48-tagged → native int op → box result.
// Checks if both operands are double-encoded → native float op → box result.
// Otherwise falls back to the specified runtime helper.
llvm::Value* QoreIRToLLVM::emitAnyArithFastPath(llvm::Instruction::BinaryOps int_op,
        llvm::Instruction::BinaryOps float_op, const char* slow_helper,
        llvm::Value* lhs, llvm::Value* rhs,
        llvm::Function* llvm_func, llvm::Module& module) {
    // Constants for tag checking
    llvm::Value* tag_mask = llvm::ConstantInt::get(i64_type, 0xFFFF000000000000ULL);
    llvm::Value* tag_int48 = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* double_offset = llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET);

    // Check if both are int48-tagged
    llvm::Value* lhs_tag = builder->CreateAnd(lhs, tag_mask);
    llvm::Value* rhs_tag = builder->CreateAnd(rhs, tag_mask);
    llvm::Value* lhs_is_int = builder->CreateICmpEQ(lhs_tag, tag_int48);
    llvm::Value* rhs_is_int = builder->CreateICmpEQ(rhs_tag, tag_int48);
    llvm::Value* both_int = builder->CreateAnd(lhs_is_int, rhs_is_int);

    // Create basic blocks
    llvm::BasicBlock* fast_int_bb = llvm::BasicBlock::Create(ctx, "fast_int", llvm_func);
    llvm::BasicBlock* check_float_bb = llvm::BasicBlock::Create(ctx, "check_float", llvm_func);
    llvm::BasicBlock* fast_float_bb = llvm::BasicBlock::Create(ctx, "fast_float", llvm_func);
    llvm::BasicBlock* slow_bb = llvm::BasicBlock::Create(ctx, "slow_path", llvm_func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "merge", llvm_func);

    builder->CreateCondBr(both_int, fast_int_bb, check_float_bb);

    // Fast int path: unbox, native op, box
    builder->SetInsertPoint(fast_int_bb);
    llvm::Value* l_int = unboxInt(lhs);
    llvm::Value* r_int = unboxInt(rhs);
    llvm::Value* int_result = builder->CreateBinOp(int_op, l_int, r_int);
    llvm::Value* int_boxed = boxIntInline(int_result);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_int_end = builder->GetInsertBlock();

    // Check float path: double-encoded values satisfy
    //   bits > DOUBLE_ENCODE_OFFSET && bits < DOUBLE_BOUNDARY (== TAG_INT48)
    builder->SetInsertPoint(check_float_bb);
    llvm::Value* double_boundary = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* lhs_gt_offset = builder->CreateICmpUGT(lhs, double_offset);
    llvm::Value* lhs_lt_boundary = builder->CreateICmpULT(lhs, double_boundary);
    llvm::Value* lhs_is_float = builder->CreateAnd(lhs_gt_offset, lhs_lt_boundary);
    llvm::Value* rhs_gt_offset = builder->CreateICmpUGT(rhs, double_offset);
    llvm::Value* rhs_lt_boundary = builder->CreateICmpULT(rhs, double_boundary);
    llvm::Value* rhs_is_float = builder->CreateAnd(rhs_gt_offset, rhs_lt_boundary);
    llvm::Value* both_float = builder->CreateAnd(lhs_is_float, rhs_is_float);
    builder->CreateCondBr(both_float, fast_float_bb, slow_bb);

    // Fast float path: unbox, native op, box
    builder->SetInsertPoint(fast_float_bb);
    llvm::Value* l_float = unboxFloat(lhs);
    llvm::Value* r_float = unboxFloat(rhs);
    llvm::Value* float_result = builder->CreateBinOp(float_op, l_float, r_float);
    llvm::Value* float_boxed = boxFloat(float_result);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_float_end = builder->GetInsertBlock();

    // Slow path: call runtime helper
    builder->SetInsertPoint(slow_bb);
    auto helper = module.getOrInsertFunction(slow_helper,
            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
    llvm::Value* slow_result = builder->CreateCall(helper, {lhs, rhs, xsink_arg});
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* slow_end = builder->GetInsertBlock();

    // Merge with PHI
    builder->SetInsertPoint(merge_bb);
    llvm::PHINode* phi = builder->CreatePHI(i64_type, 3);
    phi->addIncoming(int_boxed, fast_int_end);
    phi->addIncoming(float_boxed, fast_float_end);
    phi->addIncoming(slow_result, slow_end);

    return phi;
}

// Phase 5b: Emit inline LLVM fast-path for .any comparisons.
// Checks int-vs-int and float-vs-float for native comparison, then boxes result as bool.
// Falls back to qore_rt_comparison_op for mixed types.
llvm::Value* QoreIRToLLVM::emitAnyCmpFastPath(llvm::CmpInst::Predicate int_pred,
        llvm::CmpInst::Predicate float_pred, int opcode,
        llvm::Value* lhs, llvm::Value* rhs,
        llvm::Function* llvm_func, llvm::Module& module) {
    // Constants for tag checking
    llvm::Value* tag_mask = llvm::ConstantInt::get(i64_type, 0xFFFF000000000000ULL);
    llvm::Value* tag_int48 = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* double_offset = llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET);

    // Check if both are int48-tagged
    llvm::Value* lhs_tag = builder->CreateAnd(lhs, tag_mask);
    llvm::Value* rhs_tag = builder->CreateAnd(rhs, tag_mask);
    llvm::Value* lhs_is_int = builder->CreateICmpEQ(lhs_tag, tag_int48);
    llvm::Value* rhs_is_int = builder->CreateICmpEQ(rhs_tag, tag_int48);
    llvm::Value* both_int = builder->CreateAnd(lhs_is_int, rhs_is_int);

    // Create basic blocks
    llvm::BasicBlock* fast_int_bb = llvm::BasicBlock::Create(ctx, "cmp_fast_int", llvm_func);
    llvm::BasicBlock* check_float_bb = llvm::BasicBlock::Create(ctx, "cmp_check_float", llvm_func);
    llvm::BasicBlock* fast_float_bb = llvm::BasicBlock::Create(ctx, "cmp_fast_float", llvm_func);
    llvm::BasicBlock* slow_bb = llvm::BasicBlock::Create(ctx, "cmp_slow", llvm_func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "cmp_merge", llvm_func);

    builder->CreateCondBr(both_int, fast_int_bb, check_float_bb);

    // Fast int path: unbox, compare, box bool
    builder->SetInsertPoint(fast_int_bb);
    llvm::Value* l_int = unboxInt(lhs);
    llvm::Value* r_int = unboxInt(rhs);
    llvm::Value* int_cmp = builder->CreateICmp(int_pred, l_int, r_int);
    llvm::Value* int_boxed = boxBool(int_cmp);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_int_end = builder->GetInsertBlock();

    // Check float path: double-encoded values satisfy
    //   bits > DOUBLE_ENCODE_OFFSET && bits < DOUBLE_BOUNDARY (== TAG_INT48)
    builder->SetInsertPoint(check_float_bb);
    llvm::Value* double_boundary = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* lhs_gt_offset = builder->CreateICmpUGT(lhs, double_offset);
    llvm::Value* lhs_lt_boundary = builder->CreateICmpULT(lhs, double_boundary);
    llvm::Value* lhs_is_float = builder->CreateAnd(lhs_gt_offset, lhs_lt_boundary);
    llvm::Value* rhs_gt_offset = builder->CreateICmpUGT(rhs, double_offset);
    llvm::Value* rhs_lt_boundary = builder->CreateICmpULT(rhs, double_boundary);
    llvm::Value* rhs_is_float = builder->CreateAnd(rhs_gt_offset, rhs_lt_boundary);
    llvm::Value* both_float = builder->CreateAnd(lhs_is_float, rhs_is_float);
    builder->CreateCondBr(both_float, fast_float_bb, slow_bb);

    // Fast float path: unbox, compare, box bool
    builder->SetInsertPoint(fast_float_bb);
    llvm::Value* l_float = unboxFloat(lhs);
    llvm::Value* r_float = unboxFloat(rhs);
    llvm::Value* float_cmp = builder->CreateFCmp(float_pred, l_float, r_float);
    llvm::Value* float_boxed = boxBool(float_cmp);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_float_end = builder->GetInsertBlock();

    // Slow path: call runtime comparison helper
    builder->SetInsertPoint(slow_bb);
    llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), opcode);
    auto helper = module.getOrInsertFunction("qore_rt_comparison_op",
            llvm::FunctionType::get(i64_type,
                {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false));
    llvm::Value* slow_result = builder->CreateCall(helper,
            {opcode_val, lhs, rhs, xsink_arg});
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* slow_end = builder->GetInsertBlock();

    // Merge with PHI
    builder->SetInsertPoint(merge_bb);
    llvm::PHINode* phi = builder->CreatePHI(i64_type, 3);
    phi->addIncoming(int_boxed, fast_int_end);
    phi->addIncoming(float_boxed, fast_float_end);
    phi->addIncoming(slow_result, slow_end);

    return phi;
}

// Emit inline LLVM fast-path for .any compound assignments (AddAssignAny/SubAssignAny/etc).
// Type-checks operands for int+int and float+float, falls back to helper for mixed types.
// For AddAssignAny (handle_nothing=true), returns rhs if lhs is NOTHING.
llvm::Value* QoreIRToLLVM::emitAnyCompoundAssignFastPath(llvm::Instruction::BinaryOps int_op,
        llvm::Instruction::BinaryOps float_op, const char* slow_helper,
        llvm::Value* lhs, llvm::Value* rhs,
        llvm::Function* llvm_func, llvm::Module& module, bool handle_nothing) {
    // Constants for tag checking
    llvm::Value* tag_mask = llvm::ConstantInt::get(i64_type, 0xFFFF000000000000ULL);
    llvm::Value* tag_int48 = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* double_offset = llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET);
    llvm::Value* nothing_val = llvm::ConstantInt::get(i64_type, VAL_NOTHING);

    // Create basic blocks
    llvm::BasicBlock* check_int_bb = handle_nothing
        ? llvm::BasicBlock::Create(ctx, "ca_check_int", llvm_func)
        : nullptr;
    llvm::BasicBlock* fast_int_bb = llvm::BasicBlock::Create(ctx, "ca_fast_int", llvm_func);
    llvm::BasicBlock* check_float_bb = llvm::BasicBlock::Create(ctx, "ca_check_float", llvm_func);
    llvm::BasicBlock* fast_float_bb = llvm::BasicBlock::Create(ctx, "ca_fast_float", llvm_func);
    llvm::BasicBlock* slow_bb = llvm::BasicBlock::Create(ctx, "ca_slow_path", llvm_func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "ca_merge", llvm_func);

    // For AddAssignAny, if lhs is NOTHING, return rhs with incremented ref count
    llvm::BasicBlock* nothing_bb = nullptr;
    llvm::Value* nothing_result = nullptr;
    if (handle_nothing) {
        nothing_bb = llvm::BasicBlock::Create(ctx, "ca_nothing", llvm_func);
        llvm::Value* lhs_is_nothing = builder->CreateICmpEQ(lhs, nothing_val);
        builder->CreateCondBr(lhs_is_nothing, nothing_bb, check_int_bb);

        // Nothing path: Phase 4 - inline qore_rt_ref using select
        // Checks if rhs has a node inline via NaN-boxing, selects refself result or original value
        builder->SetInsertPoint(nothing_bb);
        nothing_result = emitHelperRef(module, rhs);
        builder->CreateBr(merge_bb);
    }

    // Check if both are int48-tagged
    if (handle_nothing) {
        builder->SetInsertPoint(check_int_bb);
    }
    llvm::Value* lhs_tag = builder->CreateAnd(lhs, tag_mask);
    llvm::Value* rhs_tag = builder->CreateAnd(rhs, tag_mask);
    llvm::Value* lhs_is_int = builder->CreateICmpEQ(lhs_tag, tag_int48);
    llvm::Value* rhs_is_int = builder->CreateICmpEQ(rhs_tag, tag_int48);
    llvm::Value* both_int = builder->CreateAnd(lhs_is_int, rhs_is_int);

    builder->CreateCondBr(both_int, fast_int_bb, check_float_bb);

    // Fast int path: unbox, native op, box
    builder->SetInsertPoint(fast_int_bb);
    llvm::Value* l_int = unboxInt(lhs);
    llvm::Value* r_int = unboxInt(rhs);
    llvm::Value* int_result = builder->CreateBinOp(int_op, l_int, r_int);
    llvm::Value* int_boxed = boxIntInline(int_result);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_int_end = builder->GetInsertBlock();

    // Check float path: double-encoded values satisfy
    //   bits > DOUBLE_ENCODE_OFFSET && bits < DOUBLE_BOUNDARY (== TAG_INT48)
    builder->SetInsertPoint(check_float_bb);
    llvm::Value* double_boundary = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* lhs_gt_offset = builder->CreateICmpUGT(lhs, double_offset);
    llvm::Value* lhs_lt_boundary = builder->CreateICmpULT(lhs, double_boundary);
    llvm::Value* lhs_is_float = builder->CreateAnd(lhs_gt_offset, lhs_lt_boundary);
    llvm::Value* rhs_gt_offset = builder->CreateICmpUGT(rhs, double_offset);
    llvm::Value* rhs_lt_boundary = builder->CreateICmpULT(rhs, double_boundary);
    llvm::Value* rhs_is_float = builder->CreateAnd(rhs_gt_offset, rhs_lt_boundary);
    llvm::Value* both_float = builder->CreateAnd(lhs_is_float, rhs_is_float);
    builder->CreateCondBr(both_float, fast_float_bb, slow_bb);

    // Fast float path: unbox, native op, box
    builder->SetInsertPoint(fast_float_bb);
    llvm::Value* l_float = unboxFloat(lhs);
    llvm::Value* r_float = unboxFloat(rhs);
    llvm::Value* float_result = builder->CreateBinOp(float_op, l_float, r_float);
    llvm::Value* float_boxed = boxFloat(float_result);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_float_end = builder->GetInsertBlock();

    // Slow path: call runtime helper
    builder->SetInsertPoint(slow_bb);
    auto ca_helper = module.getOrInsertFunction(slow_helper,
            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
    llvm::Value* slow_result_val = builder->CreateCall(ca_helper, {lhs, rhs, xsink_arg});
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* slow_end = builder->GetInsertBlock();

    // Merge with PHI
    builder->SetInsertPoint(merge_bb);
    int incoming_count = handle_nothing ? 4 : 3;
    llvm::PHINode* ca_phi = builder->CreatePHI(i64_type, incoming_count);
    if (handle_nothing) {
        ca_phi->addIncoming(nothing_result, nothing_bb);
    }
    ca_phi->addIncoming(int_boxed, fast_int_end);
    ca_phi->addIncoming(float_boxed, fast_float_end);
    ca_phi->addIncoming(slow_result_val, slow_end);

    return ca_phi;
}

// Emit inline LLVM fast-path for lvalue compound assignments (AddAssignLValue, etc.).
// Loads the current lvalue value, checks NOTHING (for AddAssignLValue only), then
// checks INT48+INT48 and float+float for native arithmetic, falling back to
// qore_rt_lvalue_binary for complex types.
//
// CRITICAL: The loaded value must be decref'd BEFORE the slow path call to
// qore_rt_lvalue_binary.  The load adds +1 refcount; if the lvalue holds a
// container (list, hash, object) with refcount 1, the load makes it 2.  The slow
// path accesses the lvalue independently and sees refcount > 1, triggering
// copy-on-write — turning O(1) in-place appends into O(n) copies per iteration.
// Each fast path (int, float) decrefs after using the loaded value; the slow path
// decrefs before calling qore_rt_lvalue_binary.
//
// Exception safety: qore_rt_lvalue_load returns NOTHING on error (see evalLValueLoad),
// so emitExceptionCheck after lvalue_load cannot leak a ref-counted value.
llvm::Value* QoreIRToLLVM::emitLValueCompoundAssignFastPath(
        const QoreIRInstruction* inst,
        llvm::Value* val_boxed, llvm::Value* lv_bits_or_slot,
        int int_op, int float_op,
        bool handle_nothing,
        llvm::Function* llvm_func, llvm::Module& module) {
    // Step 1: Load the current lvalue value
    llvm::Value* current;
    if (aot_mode) {
        auto load_fn = module.getOrInsertFunction("qore_rt_lvalue_load_aot",
                llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
        current = builder->CreateCall(load_fn, {aot_ctx_arg, lv_bits_or_slot, xsink_arg});
    } else {
        auto load_fn = module.getOrInsertFunction("qore_rt_lvalue_load",
                llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
        current = builder->CreateCall(load_fn, {lv_bits_or_slot, xsink_arg});
    }
    emitExceptionCheck(module, llvm_func, inst);

    // Helper lambda: emit lvalue_store call (JIT or AOT) to reduce duplication
    auto emitLValueStore = [&](llvm::Value* store_val) -> llvm::Value* {
        if (aot_mode) {
            auto store_fn = module.getOrInsertFunction("qore_rt_lvalue_store_aot",
                    llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, i64_type, ptr_type}, false));
            return builder->CreateCall(store_fn,
                    {aot_ctx_arg, lv_bits_or_slot, store_val, xsink_arg});
        }
        auto store_fn = module.getOrInsertFunction("qore_rt_lvalue_store",
                llvm::FunctionType::get(i64_type,
                    {i64_type, i64_type, ptr_type}, false));
        return builder->CreateCall(store_fn,
                {lv_bits_or_slot, store_val, xsink_arg});
    };

    // Constants
    llvm::Value* tag_mask = llvm::ConstantInt::get(i64_type, 0xFFFF000000000000ULL);
    llvm::Value* tag_int48 = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* double_offset = llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET);

    // Determine opcode for slow path
    llvm::Value* opcode_val = llvm::ConstantInt::get(i32_type, static_cast<int>(inst->opcode));

    // Create basic blocks
    bool has_int_path = (int_op >= 0);
    bool has_float_path = (float_op >= 0);

    llvm::BasicBlock* nothing_bb = handle_nothing
        ? llvm::BasicBlock::Create(ctx, "lca_nothing", llvm_func) : nullptr;
    llvm::BasicBlock* check_int_bb = has_int_path
        ? llvm::BasicBlock::Create(ctx, "lca_check_int", llvm_func) : nullptr;
    llvm::BasicBlock* fast_int_bb = has_int_path
        ? llvm::BasicBlock::Create(ctx, "lca_fast_int", llvm_func) : nullptr;
    llvm::BasicBlock* check_float_bb = has_float_path
        ? llvm::BasicBlock::Create(ctx, "lca_check_float", llvm_func) : nullptr;
    llvm::BasicBlock* fast_float_bb = has_float_path
        ? llvm::BasicBlock::Create(ctx, "lca_fast_float", llvm_func) : nullptr;
    llvm::BasicBlock* slow_bb = llvm::BasicBlock::Create(ctx, "lca_slow", llvm_func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "lca_merge", llvm_func);

    // Decref helper for the loaded value (each path decrefs at the right point)
    auto emitDecrefCurrent = [&]() {
        auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
        builder->CreateCall(decref_fn, {current, xsink_arg});
    };

    // Determine first block after NOTHING check
    llvm::BasicBlock* after_nothing_bb = has_int_path ? check_int_bb
        : (has_float_path ? check_float_bb : slow_bb);

    // Step 2: NOTHING check (only for AddAssignLValue)
    if (handle_nothing) {
        llvm::Value* nothing_val = llvm::ConstantInt::get(i64_type, VAL_NOTHING);
        llvm::Value* is_nothing = builder->CreateICmpEQ(current, nothing_val);
        builder->CreateCondBr(is_nothing, nothing_bb, after_nothing_bb);
    } else {
        builder->CreateBr(after_nothing_bb);
    }

    // NOTHING path: store rhs directly (handles hash/list type conversion via lvalue_store)
    llvm::Value* nothing_result = nullptr;
    llvm::BasicBlock* nothing_end = nullptr;
    if (handle_nothing) {
        builder->SetInsertPoint(nothing_bb);
        nothing_result = emitLValueStore(val_boxed);
        // current is NOTHING here — no decref needed (scalar, no refcount)
        builder->CreateBr(merge_bb);
        nothing_end = builder->GetInsertBlock();
    }

    // Step 3: INT48+INT48 fast path
    llvm::Value* int_result = nullptr;
    llvm::BasicBlock* fast_int_end = nullptr;
    if (has_int_path) {
        builder->SetInsertPoint(check_int_bb);
        llvm::Value* current_tag = builder->CreateAnd(current, tag_mask);
        llvm::Value* rhs_tag = builder->CreateAnd(val_boxed, tag_mask);
        llvm::Value* current_is_int = builder->CreateICmpEQ(current_tag, tag_int48);
        llvm::Value* rhs_is_int = builder->CreateICmpEQ(rhs_tag, tag_int48);
        llvm::Value* both_int = builder->CreateAnd(current_is_int, rhs_is_int);

        llvm::BasicBlock* after_int_bb = has_float_path ? check_float_bb : slow_bb;
        builder->CreateCondBr(both_int, fast_int_bb, after_int_bb);

        // Fast int path: native op + lvalue_store + decref loaded value
        builder->SetInsertPoint(fast_int_bb);
        llvm::Value* l_int = unboxInt(current);
        llvm::Value* r_int = unboxInt(val_boxed);
        llvm::Value* native_result = builder->CreateBinOp(
                static_cast<llvm::Instruction::BinaryOps>(int_op), l_int, r_int);
        llvm::Value* result_boxed = boxIntInline(native_result);
        int_result = emitLValueStore(result_boxed);
        // current is INT48 here — no decref needed (scalar, no refcount)
        builder->CreateBr(merge_bb);
        fast_int_end = builder->GetInsertBlock();
    }

    // Step 4: float+float fast path
    llvm::Value* float_result = nullptr;
    llvm::BasicBlock* fast_float_end = nullptr;
    if (has_float_path) {
        builder->SetInsertPoint(check_float_bb);
        llvm::Value* double_boundary = llvm::ConstantInt::get(i64_type, TAG_INT48);
        llvm::Value* lhs_gt_offset = builder->CreateICmpUGT(current, double_offset);
        llvm::Value* lhs_lt_boundary = builder->CreateICmpULT(current, double_boundary);
        llvm::Value* lhs_is_float = builder->CreateAnd(lhs_gt_offset, lhs_lt_boundary);
        llvm::Value* rhs_gt_offset = builder->CreateICmpUGT(val_boxed, double_offset);
        llvm::Value* rhs_lt_boundary = builder->CreateICmpULT(val_boxed, double_boundary);
        llvm::Value* rhs_is_float = builder->CreateAnd(rhs_gt_offset, rhs_lt_boundary);
        llvm::Value* both_float = builder->CreateAnd(lhs_is_float, rhs_is_float);
        builder->CreateCondBr(both_float, fast_float_bb, slow_bb);

        // Fast float path: native op + lvalue_store + decref loaded value
        builder->SetInsertPoint(fast_float_bb);
        llvm::Value* l_float = unboxFloat(current);
        llvm::Value* r_float = unboxFloat(val_boxed);
        llvm::Value* flt_result = builder->CreateBinOp(
                static_cast<llvm::Instruction::BinaryOps>(float_op), l_float, r_float);
        llvm::Value* flt_boxed = boxFloat(flt_result);
        float_result = emitLValueStore(flt_boxed);
        // current is a NaN-boxed float here — no decref needed (scalar, no refcount)
        builder->CreateBr(merge_bb);
        fast_float_end = builder->GetInsertBlock();
    }

    // Step 5: Slow path — decref loaded value FIRST, then fall back to qore_rt_lvalue_binary.
    // The decref restores the lvalue's original refcount so that in-place modification
    // (e.g. list append, hash merge) works without triggering copy-on-write.
    builder->SetInsertPoint(slow_bb);
    emitDecrefCurrent();
    llvm::Value* slow_result;
    if (aot_mode) {
        auto binary_fn = module.getOrInsertFunction("qore_rt_lvalue_binary_aot",
                llvm::FunctionType::get(i64_type,
                    {i32_type, ptr_type, i32_type, i64_type, ptr_type}, false));
        slow_result = builder->CreateCall(binary_fn,
                {opcode_val, aot_ctx_arg, lv_bits_or_slot, val_boxed, xsink_arg});
    } else {
        auto binary_fn = module.getOrInsertFunction("qore_rt_lvalue_binary",
                llvm::FunctionType::get(i64_type,
                    {i32_type, i64_type, i64_type, ptr_type}, false));
        slow_result = builder->CreateCall(binary_fn,
                {opcode_val, lv_bits_or_slot, val_boxed, xsink_arg});
    }
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* slow_end = builder->GetInsertBlock();

    // Step 6: Merge with PHI
    builder->SetInsertPoint(merge_bb);
    int incoming = 1; // slow always
    if (handle_nothing) { ++incoming; }
    if (has_int_path) { ++incoming; }
    if (has_float_path) { ++incoming; }
    llvm::PHINode* phi = builder->CreatePHI(i64_type, incoming);
    if (handle_nothing) {
        phi->addIncoming(nothing_result, nothing_end);
    }
    if (has_int_path) {
        phi->addIncoming(int_result, fast_int_end);
    }
    if (has_float_path) {
        phi->addIncoming(float_result, fast_float_end);
    }
    phi->addIncoming(slow_result, slow_end);

    return phi;
}

// Emit inline LLVM fast-path for .any bitwise compound assignments (AndAssignAny/etc).
// Type-checks operands for int+int, falls back to qore_rt_binary_op for non-int types.
llvm::Value* QoreIRToLLVM::emitAnyBitwiseFastPath(llvm::Instruction::BinaryOps int_op,
        const char* slow_helper, int opcode_val_int,
        llvm::Value* lhs, llvm::Value* rhs,
        llvm::Function* llvm_func, llvm::Module& module) {
    // Constants for tag checking
    llvm::Value* tag_mask = llvm::ConstantInt::get(i64_type, 0xFFFF000000000000ULL);
    llvm::Value* tag_int48 = llvm::ConstantInt::get(i64_type, TAG_INT48);

    // Check if both are int48-tagged
    llvm::Value* lhs_tag = builder->CreateAnd(lhs, tag_mask);
    llvm::Value* rhs_tag = builder->CreateAnd(rhs, tag_mask);
    llvm::Value* lhs_is_int = builder->CreateICmpEQ(lhs_tag, tag_int48);
    llvm::Value* rhs_is_int = builder->CreateICmpEQ(rhs_tag, tag_int48);
    llvm::Value* both_int = builder->CreateAnd(lhs_is_int, rhs_is_int);

    // Create basic blocks
    llvm::BasicBlock* fast_int_bb = llvm::BasicBlock::Create(ctx, "bw_fast_int", llvm_func);
    llvm::BasicBlock* slow_bb = llvm::BasicBlock::Create(ctx, "bw_slow_path", llvm_func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "bw_merge", llvm_func);

    builder->CreateCondBr(both_int, fast_int_bb, slow_bb);

    // Fast int path: unbox, native op, box
    builder->SetInsertPoint(fast_int_bb);
    llvm::Value* l_int = unboxInt(lhs);
    llvm::Value* r_int = unboxInt(rhs);
    llvm::Value* int_result = builder->CreateBinOp(int_op, l_int, r_int);
    llvm::Value* int_boxed = boxIntInline(int_result);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_int_end = builder->GetInsertBlock();

    // Slow path: call runtime helper (qore_rt_binary_op)
    builder->SetInsertPoint(slow_bb);
    llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), opcode_val_int);
    auto bw_helper = module.getOrInsertFunction(slow_helper,
            llvm::FunctionType::get(i64_type,
                {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false));
    llvm::Value* slow_result_val = builder->CreateCall(bw_helper,
            {opcode_val, lhs, rhs, xsink_arg});
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* slow_end = builder->GetInsertBlock();

    // Merge with PHI
    builder->SetInsertPoint(merge_bb);
    llvm::PHINode* bw_phi = builder->CreatePHI(i64_type, 2);
    bw_phi->addIncoming(int_boxed, fast_int_end);
    bw_phi->addIncoming(slow_result_val, slow_end);

    return bw_phi;
}

// Emit inline LLVM fast-path for .any unary operations (UnaryMinusAny/UnaryPlusAny).
// Type-checks operand for int or float, falls back to qore_rt_unary_op for other types.
llvm::Value* QoreIRToLLVM::emitAnyUnaryFastPath(bool is_minus, int opcode_val_int,
        llvm::Value* operand, llvm::Function* llvm_func, llvm::Module& module) {
    // Constants for tag checking
    llvm::Value* tag_mask = llvm::ConstantInt::get(i64_type, 0xFFFF000000000000ULL);
    llvm::Value* tag_int48 = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* double_offset = llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET);

    // Check if operand is int48-tagged
    llvm::Value* op_tag = builder->CreateAnd(operand, tag_mask);
    llvm::Value* is_int = builder->CreateICmpEQ(op_tag, tag_int48);

    // Create basic blocks
    llvm::BasicBlock* fast_int_bb = llvm::BasicBlock::Create(ctx, "unary_fast_int", llvm_func);
    llvm::BasicBlock* check_float_bb = llvm::BasicBlock::Create(ctx, "unary_check_float", llvm_func);
    llvm::BasicBlock* fast_float_bb = llvm::BasicBlock::Create(ctx, "unary_fast_float", llvm_func);
    llvm::BasicBlock* slow_bb = llvm::BasicBlock::Create(ctx, "unary_slow", llvm_func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "unary_merge", llvm_func);

    builder->CreateCondBr(is_int, fast_int_bb, check_float_bb);

    // Fast int path: unbox, negate (or identity for plus), box
    builder->SetInsertPoint(fast_int_bb);
    llvm::Value* int_val = unboxInt(operand);
    llvm::Value* int_result = is_minus ? builder->CreateNeg(int_val) : int_val;
    llvm::Value* int_boxed = boxIntInline(int_result);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_int_end = builder->GetInsertBlock();

    // Check float path: double-encoded values satisfy
    //   bits > DOUBLE_ENCODE_OFFSET && bits < DOUBLE_BOUNDARY (== TAG_INT48)
    builder->SetInsertPoint(check_float_bb);
    llvm::Value* double_boundary = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* gt_offset = builder->CreateICmpUGT(operand, double_offset);
    llvm::Value* lt_boundary = builder->CreateICmpULT(operand, double_boundary);
    llvm::Value* is_float = builder->CreateAnd(gt_offset, lt_boundary);
    builder->CreateCondBr(is_float, fast_float_bb, slow_bb);

    // Fast float path: unbox, fneg (or identity for plus), box
    builder->SetInsertPoint(fast_float_bb);
    llvm::Value* float_val = unboxFloat(operand);
    llvm::Value* float_result = is_minus ? builder->CreateFNeg(float_val) : float_val;
    llvm::Value* float_boxed = boxFloat(float_result);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_float_end = builder->GetInsertBlock();

    // Slow path: call runtime helper
    builder->SetInsertPoint(slow_bb);
    llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), opcode_val_int);
    auto helper = module.getOrInsertFunction("qore_rt_unary_op",
            llvm::FunctionType::get(i64_type,
                {llvm::Type::getInt32Ty(ctx), i64_type, ptr_type}, false));
    llvm::Value* slow_result = builder->CreateCall(helper,
            {opcode_val, operand, xsink_arg});
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* slow_end = builder->GetInsertBlock();

    // Merge with PHI
    builder->SetInsertPoint(merge_bb);
    llvm::PHINode* phi = builder->CreatePHI(i64_type, 3);
    phi->addIncoming(int_boxed, fast_int_end);
    phi->addIncoming(float_boxed, fast_float_end);
    phi->addIncoming(slow_result, slow_end);

    return phi;
}

// Emit inline LLVM fast-path for CmpAny (spaceship operator).
// Type-checks operands for int+int and float+float, falls back to qore_rt_comparison_op.
llvm::Value* QoreIRToLLVM::emitAnyCmpSpaceshipFastPath(llvm::Value* lhs, llvm::Value* rhs,
        llvm::Function* llvm_func, llvm::Module& module) {
    // Constants for tag checking
    llvm::Value* tag_mask = llvm::ConstantInt::get(i64_type, 0xFFFF000000000000ULL);
    llvm::Value* tag_int48 = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* double_offset = llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET);

    // Check if both are int48-tagged
    llvm::Value* lhs_tag = builder->CreateAnd(lhs, tag_mask);
    llvm::Value* rhs_tag = builder->CreateAnd(rhs, tag_mask);
    llvm::Value* lhs_is_int = builder->CreateICmpEQ(lhs_tag, tag_int48);
    llvm::Value* rhs_is_int = builder->CreateICmpEQ(rhs_tag, tag_int48);
    llvm::Value* both_int = builder->CreateAnd(lhs_is_int, rhs_is_int);

    // Create basic blocks
    llvm::BasicBlock* fast_int_bb = llvm::BasicBlock::Create(ctx, "ss_fast_int", llvm_func);
    llvm::BasicBlock* check_float_bb = llvm::BasicBlock::Create(ctx, "ss_check_float", llvm_func);
    llvm::BasicBlock* fast_float_bb = llvm::BasicBlock::Create(ctx, "ss_fast_float", llvm_func);
    llvm::BasicBlock* slow_bb = llvm::BasicBlock::Create(ctx, "ss_slow", llvm_func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "ss_merge", llvm_func);

    builder->CreateCondBr(both_int, fast_int_bb, check_float_bb);

    // Fast int path: unbox, compare, return -1/0/1
    builder->SetInsertPoint(fast_int_bb);
    llvm::Value* l_int = unboxInt(lhs);
    llvm::Value* r_int = unboxInt(rhs);
    llvm::Value* lt_int = builder->CreateICmpSLT(l_int, r_int);
    llvm::Value* gt_int = builder->CreateICmpSGT(l_int, r_int);
    llvm::Value* neg_one = llvm::ConstantInt::get(i64_type, -1);
    llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
    llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
    llvm::Value* inner_int = builder->CreateSelect(gt_int, one, zero);
    llvm::Value* int_result = builder->CreateSelect(lt_int, neg_one, inner_int);
    // Box the result as int
    llvm::Value* int_boxed = boxIntInline(int_result);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_int_end = builder->GetInsertBlock();

    // Check float path
    builder->SetInsertPoint(check_float_bb);
    llvm::Value* double_boundary = llvm::ConstantInt::get(i64_type, TAG_INT48);
    llvm::Value* lhs_gt_offset = builder->CreateICmpUGT(lhs, double_offset);
    llvm::Value* lhs_lt_boundary = builder->CreateICmpULT(lhs, double_boundary);
    llvm::Value* lhs_is_float = builder->CreateAnd(lhs_gt_offset, lhs_lt_boundary);
    llvm::Value* rhs_gt_offset = builder->CreateICmpUGT(rhs, double_offset);
    llvm::Value* rhs_lt_boundary = builder->CreateICmpULT(rhs, double_boundary);
    llvm::Value* rhs_is_float = builder->CreateAnd(rhs_gt_offset, rhs_lt_boundary);
    llvm::Value* both_float = builder->CreateAnd(lhs_is_float, rhs_is_float);
    builder->CreateCondBr(both_float, fast_float_bb, slow_bb);

    // Fast float path: unbox, compare (with NaN check via runtime for safety)
    builder->SetInsertPoint(fast_float_bb);
    llvm::Value* l_float = unboxFloat(lhs);
    llvm::Value* r_float = unboxFloat(rhs);
    // Check for NaN - if either is NaN, go to slow path for exception
    llvm::Value* is_nan = builder->CreateFCmpUNO(l_float, r_float);
    llvm::BasicBlock* float_nan_bb = llvm::BasicBlock::Create(ctx, "ss_float_nan", llvm_func);
    llvm::BasicBlock* float_ok_bb = llvm::BasicBlock::Create(ctx, "ss_float_ok", llvm_func);
    builder->CreateCondBr(is_nan, float_nan_bb, float_ok_bb);

    // Float NaN path: go to slow path for exception
    builder->SetInsertPoint(float_nan_bb);
    builder->CreateBr(slow_bb);

    // Float OK path: native comparison
    builder->SetInsertPoint(float_ok_bb);
    llvm::Value* lt_float = builder->CreateFCmpOLT(l_float, r_float);
    llvm::Value* gt_float = builder->CreateFCmpOGT(l_float, r_float);
    llvm::Value* inner_float = builder->CreateSelect(gt_float, one, zero);
    llvm::Value* float_result = builder->CreateSelect(lt_float, neg_one, inner_float);
    llvm::Value* float_boxed = boxIntInline(float_result);
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* fast_float_end = builder->GetInsertBlock();

    // Slow path: call runtime helper
    builder->SetInsertPoint(slow_bb);
    llvm::Value* opcode_val_cmp = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
            static_cast<int>(QoreIROpcode::CmpAny));
    auto cmp_helper = module.getOrInsertFunction("qore_rt_comparison_op",
            llvm::FunctionType::get(i64_type,
                {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false));
    llvm::Value* slow_result_cmp = builder->CreateCall(cmp_helper,
            {opcode_val_cmp, lhs, rhs, xsink_arg});
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* slow_end = builder->GetInsertBlock();

    // Merge with PHI
    builder->SetInsertPoint(merge_bb);
    llvm::PHINode* ss_phi = builder->CreatePHI(i64_type, 3);
    ss_phi->addIncoming(int_boxed, fast_int_end);
    ss_phi->addIncoming(float_boxed, fast_float_end);
    ss_phi->addIncoming(slow_result_cmp, slow_end);

    return ss_phi;
}

// Emit inline LLVM fast-path for EqHard/NeHard (=== and !==).
// Fast-path logic based on QoreValue::isEqualHard():
// - If bits are equal and NOT a float → true
// - If bits are equal and IS a float → check NaN (NaN !== NaN)
// - Otherwise → runtime helper
llvm::Value* QoreIRToLLVM::emitHardEqualityFastPath(bool is_eq, llvm::Value* lhs, llvm::Value* rhs,
        llvm::Function* llvm_func, llvm::Module& module) {
    // Constants
    llvm::Value* double_offset = llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET);
    llvm::Value* tag_int48 = llvm::ConstantInt::get(i64_type, TAG_INT48);

    // Create basic blocks
    llvm::BasicBlock* bits_equal_bb = llvm::BasicBlock::Create(ctx, "he_bits_equal", llvm_func);
    llvm::BasicBlock* float_nan_check_bb = llvm::BasicBlock::Create(ctx, "he_float_nan", llvm_func);
    llvm::BasicBlock* slow_bb = llvm::BasicBlock::Create(ctx, "he_slow", llvm_func);
    llvm::BasicBlock* result_true_bb = llvm::BasicBlock::Create(ctx, "he_true", llvm_func);
    llvm::BasicBlock* result_false_bb = llvm::BasicBlock::Create(ctx, "he_false", llvm_func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(ctx, "he_merge", llvm_func);

    // Check if bits are equal
    llvm::Value* bits_equal = builder->CreateICmpEQ(lhs, rhs);
    builder->CreateCondBr(bits_equal, bits_equal_bb, slow_bb);

    // Bits equal path: check if it's a float (need NaN check)
    builder->SetInsertPoint(bits_equal_bb);
    // Float-encoded values satisfy: bits > DOUBLE_ENCODE_OFFSET && bits < TAG_INT48
    llvm::Value* gt_offset = builder->CreateICmpUGT(lhs, double_offset);
    llvm::Value* lt_boundary = builder->CreateICmpULT(lhs, tag_int48);
    llvm::Value* is_float = builder->CreateAnd(gt_offset, lt_boundary);
    builder->CreateCondBr(is_float, float_nan_check_bb, result_true_bb);

    // Float NaN check: NaN !== NaN even if bits are identical
    builder->SetInsertPoint(float_nan_check_bb);
    llvm::Value* float_val = unboxFloat(lhs);
    // NaN check: fcmp ord returns false if either operand is NaN
    // So if float_val == float_val returns false, it's NaN
    llvm::Value* is_ordered = builder->CreateFCmpORD(float_val, float_val);
    // If ordered (not NaN), result is true; if unordered (NaN), result is false
    builder->CreateCondBr(is_ordered, result_true_bb, result_false_bb);

    // Slow path: call runtime helper for complex cases (different bits, strings, etc.)
    builder->SetInsertPoint(slow_bb);
    QoreIROpcode opcode = is_eq ? QoreIROpcode::EqHard : QoreIROpcode::NeHard;
    llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
            static_cast<int>(opcode));
    auto helper = module.getOrInsertFunction("qore_rt_comparison_op",
            llvm::FunctionType::get(i64_type,
                {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false));
    llvm::Value* slow_result = builder->CreateCall(helper,
            {opcode_val, lhs, rhs, xsink_arg});
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* slow_end = builder->GetInsertBlock();

    // True result
    builder->SetInsertPoint(result_true_bb);
    llvm::Value* true_result = is_eq ? boxBool(builder->getTrue()) : boxBool(builder->getFalse());
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* true_end = builder->GetInsertBlock();

    // False result
    builder->SetInsertPoint(result_false_bb);
    llvm::Value* false_result = is_eq ? boxBool(builder->getFalse()) : boxBool(builder->getTrue());
    builder->CreateBr(merge_bb);
    llvm::BasicBlock* false_end = builder->GetInsertBlock();

    // Merge with PHI
    builder->SetInsertPoint(merge_bb);
    llvm::PHINode* he_phi = builder->CreatePHI(i64_type, 3);
    he_phi->addIncoming(true_result, true_end);
    he_phi->addIncoming(false_result, false_end);
    he_phi->addIncoming(slow_result, slow_end);

    return he_phi;
}

bool QoreIRToLLVM::emitFoldLoop(const QoreIRInstruction* inst, llvm::Module& module,
        llvm::Function* llvm_func, const char* label, bool is_float,
        llvm::Value* identity_val, bool empty_nothing,
        std::function<llvm::Value*(llvm::Value*, llvm::Value*)> accumulate,
        std::string& error) {
    auto* list = getVal(inst->operands[0].id, error);
    if (!list) {
        return false;
    }
    llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

    // Get list size
    auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

    llvm::Value* zero_i = llvm::ConstantInt::get(i64_type, 0);
    llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);

    llvm::Type* elem_type = is_float ? double_type : i64_type;
    const char* get_name = is_float ? "qore_rt_list_get_float" : "qore_rt_list_get_int";
    llvm::Type* get_ret_type = elem_type;
    auto get_helper = module.getOrInsertFunction(get_name,
            llvm::FunctionType::get(get_ret_type, {i64_type, i64_type}, false));

    if (identity_val) {
        // Identity-init opcodes (Sum, Prod): iterate from 0, identity as initial accumulator
        std::string loop_name = std::string(label) + "_loop";
        std::string exit_name = std::string(label) + "_exit";

        llvm::BasicBlock* preheader = builder->GetInsertBlock();
        llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, loop_name, llvm_func);
        llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, exit_name, llvm_func);

        llvm::Value* is_empty = builder->CreateICmpEQ(size, zero_i);
        builder->CreateCondBr(is_empty, exit_bb, loop_bb);

        // Loop body
        builder->SetInsertPoint(loop_bb);
        llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
        llvm::PHINode* acc_phi = builder->CreatePHI(elem_type, 2, "acc");

        llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});
        llvm::Value* new_acc = accumulate(acc_phi, elem);

        llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
        llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
        builder->CreateCondBr(done, exit_bb, loop_bb);

        idx_phi->addIncoming(zero_i, preheader);
        idx_phi->addIncoming(next_idx, loop_bb);
        acc_phi->addIncoming(identity_val, preheader);
        acc_phi->addIncoming(new_acc, loop_bb);

        // Exit block
        builder->SetInsertPoint(exit_bb);
        llvm::PHINode* result_phi = builder->CreatePHI(elem_type, 2,
                std::string(label) + "_result");
        result_phi->addIncoming(identity_val, preheader);
        result_phi->addIncoming(new_acc, loop_bb);

        values[inst->result.id] = result_phi;
        return true;
    }

    // First-element-init opcodes (Diff, Min, Max): first element as initial accumulator
    std::string empty_name = std::string(label) + "_empty";
    std::string init_name = std::string(label) + "_init";
    std::string loop_name = std::string(label) + "_loop";
    std::string exit_name = std::string(label) + "_exit";

    llvm::BasicBlock* preheader = builder->GetInsertBlock();
    llvm::BasicBlock* empty_bb = llvm::BasicBlock::Create(ctx, empty_name, llvm_func);
    llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(ctx, init_name, llvm_func);
    llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, loop_name, llvm_func);
    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, exit_name, llvm_func);

    llvm::Value* is_empty = builder->CreateICmpEQ(size, zero_i);
    builder->CreateCondBr(is_empty, empty_bb, init_bb);

    // Empty block
    builder->SetInsertPoint(empty_bb);
    builder->CreateBr(exit_bb);

    // Init block: get first element
    builder->SetInsertPoint(init_bb);
    llvm::Value* first_elem = builder->CreateCall(get_helper, {list_boxed, zero_i});
    llvm::Value* has_more = builder->CreateICmpUGT(size, one);
    builder->CreateCondBr(has_more, loop_bb, exit_bb);

    // Loop body (starts from index 1)
    builder->SetInsertPoint(loop_bb);
    llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
    llvm::PHINode* acc_phi = builder->CreatePHI(elem_type, 2, "acc");

    llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});
    llvm::Value* new_acc = accumulate(acc_phi, elem);

    llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
    llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
    builder->CreateCondBr(done, exit_bb, loop_bb);

    idx_phi->addIncoming(one, init_bb);
    idx_phi->addIncoming(next_idx, loop_bb);
    acc_phi->addIncoming(first_elem, init_bb);
    acc_phi->addIncoming(new_acc, loop_bb);

    // Exit block
    builder->SetInsertPoint(exit_bb);

    if (empty_nothing) {
        // Min/Max: empty list returns NOTHING (nanboxed i64)
        // Result PHI is i64 (nanboxed) — box first_elem and new_acc
        llvm::Value* empty_result = llvm::ConstantInt::get(i64_type, 0);  // VAL_NOTHING

        // Box first_elem — boxIntInline/boxFloat may create branches
        // We need to insert boxing BEFORE the exit_bb PHI, so we create intermediate blocks
        llvm::BasicBlock* box_init_bb = llvm::BasicBlock::Create(ctx,
                std::string(label) + "_box_init", llvm_func, exit_bb);
        llvm::BasicBlock* box_loop_bb = llvm::BasicBlock::Create(ctx,
                std::string(label) + "_box_loop", llvm_func, exit_bb);

        // Redirect init_bb's branch to exit_bb to box_init_bb instead
        // We need to fix up the init_bb terminator — replace exit target with box_init_bb
        // The init_bb has a conditional branch: has_more ? loop_bb : exit_bb
        // We need to change exit_bb to box_init_bb
        llvm::Instruction* init_term = init_bb->getTerminator();
        for (unsigned i = 0; i < init_term->getNumSuccessors(); ++i) {
            if (init_term->getSuccessor(i) == exit_bb) {
                init_term->setSuccessor(i, box_init_bb);
            }
        }

        // Similarly redirect loop_bb's branch to exit_bb to box_loop_bb
        llvm::Instruction* loop_term = loop_bb->getTerminator();
        for (unsigned i = 0; i < loop_term->getNumSuccessors(); ++i) {
            if (loop_term->getSuccessor(i) == exit_bb) {
                loop_term->setSuccessor(i, box_loop_bb);
            }
        }

        // Box first_elem in box_init_bb
        builder->SetInsertPoint(box_init_bb);
        llvm::Value* first_boxed;
        if (is_float) {
            first_boxed = boxFloat(first_elem);
        } else {
            first_boxed = boxIntInline(first_elem);
        }
        llvm::BasicBlock* box_init_end = builder->GetInsertBlock();
        builder->CreateBr(exit_bb);

        // Box new_acc in box_loop_bb
        builder->SetInsertPoint(box_loop_bb);
        llvm::Value* loop_boxed;
        if (is_float) {
            loop_boxed = boxFloat(new_acc);
        } else {
            loop_boxed = boxIntInline(new_acc);
        }
        llvm::BasicBlock* box_loop_end = builder->GetInsertBlock();
        builder->CreateBr(exit_bb);

        builder->SetInsertPoint(exit_bb);
        llvm::PHINode* result_phi = builder->CreatePHI(i64_type, 3,
                std::string(label) + "_result");
        result_phi->addIncoming(empty_result, empty_bb);
        result_phi->addIncoming(first_boxed, box_init_end);
        result_phi->addIncoming(loop_boxed, box_loop_end);

        values[inst->result.id] = result_phi;
        nanboxed_values.insert(inst->result.id);
    } else {
        // Diff: empty list returns 0/0.0 (raw typed value)
        llvm::Value* empty_result;
        if (is_float) {
            empty_result = llvm::ConstantFP::get(double_type, 0.0);
        } else {
            empty_result = zero_i;
        }

        llvm::PHINode* result_phi = builder->CreatePHI(elem_type, 3,
                std::string(label) + "_result");
        result_phi->addIncoming(empty_result, empty_bb);
        result_phi->addIncoming(first_elem, init_bb);
        result_phi->addIncoming(new_acc, loop_bb);

        values[inst->result.id] = result_phi;
    }

    return true;
}

bool QoreIRToLLVM::emitFoldReverseLoop(const QoreIRInstruction* inst, llvm::Module& module,
        llvm::Function* llvm_func, const char* label, bool is_float,
        std::function<llvm::Value*(llvm::Value*, llvm::Value*)> accumulate,
        std::string& error) {
    auto* list = getVal(inst->operands[0].id, error);
    if (!list) {
        return false;
    }
    llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

    // Get list size
    auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

    llvm::Value* zero_i = llvm::ConstantInt::get(i64_type, 0);
    llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
    llvm::Value* two = llvm::ConstantInt::get(i64_type, 2);

    llvm::Type* elem_type = is_float ? double_type : i64_type;
    const char* get_name = is_float ? "qore_rt_list_get_float" : "qore_rt_list_get_int";
    auto get_helper = module.getOrInsertFunction(get_name,
            llvm::FunctionType::get(elem_type, {i64_type, i64_type}, false));

    std::string empty_name = std::string(label) + "_empty";
    std::string init_name = std::string(label) + "_init";
    std::string loop_name = std::string(label) + "_loop";
    std::string exit_name = std::string(label) + "_exit";

    llvm::BasicBlock* preheader = builder->GetInsertBlock();
    llvm::BasicBlock* empty_bb = llvm::BasicBlock::Create(ctx, empty_name, llvm_func);
    llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(ctx, init_name, llvm_func);
    llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, loop_name, llvm_func);
    llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, exit_name, llvm_func);

    // Check if list is empty
    llvm::Value* is_empty = builder->CreateICmpEQ(size, zero_i);
    builder->CreateCondBr(is_empty, empty_bb, init_bb);

    // Empty block
    builder->SetInsertPoint(empty_bb);
    builder->CreateBr(exit_bb);

    // Init block: get last element as initial accumulator
    builder->SetInsertPoint(init_bb);
    llvm::Value* last_idx = builder->CreateSub(size, one);
    llvm::Value* last_elem = builder->CreateCall(get_helper, {list_boxed, last_idx});
    llvm::Value* start_idx = builder->CreateSub(size, two);

    llvm::Value* has_more = builder->CreateICmpUGT(size, one);
    builder->CreateCondBr(has_more, loop_bb, exit_bb);

    // Loop body - count down from size-2 to 0
    builder->SetInsertPoint(loop_bb);
    llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
    llvm::PHINode* acc_phi = builder->CreatePHI(elem_type, 2, "acc");

    llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});
    llvm::Value* new_acc = accumulate(acc_phi, elem);

    // Decrement index and check loop condition (signed comparison)
    llvm::Value* prev_idx = builder->CreateSub(idx_phi, one);
    llvm::Value* done = builder->CreateICmpSLT(prev_idx, zero_i);
    builder->CreateCondBr(done, exit_bb, loop_bb);

    idx_phi->addIncoming(start_idx, init_bb);
    idx_phi->addIncoming(prev_idx, loop_bb);
    acc_phi->addIncoming(last_elem, init_bb);
    acc_phi->addIncoming(new_acc, loop_bb);

    // Exit block
    builder->SetInsertPoint(exit_bb);
    llvm::Value* empty_result;
    if (is_float) {
        empty_result = llvm::ConstantFP::get(double_type, 0.0);
    } else {
        empty_result = zero_i;
    }

    llvm::PHINode* result_phi = builder->CreatePHI(elem_type, 3,
            std::string(label) + "_result");
    result_phi->addIncoming(empty_result, empty_bb);
    result_phi->addIncoming(last_elem, init_bb);
    result_phi->addIncoming(new_acc, loop_bb);

    values[inst->result.id] = result_phi;
    return true;
}

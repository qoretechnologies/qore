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

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/QoreJIT.h"
#include "qore/intern/QoreIRToLLVM.h"

#include "qore/intern/LocalVar.h"
#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreClassIntern.h"
// Compile-time guard: forces review of LLVM lowering when opcodes change.
// Update this value after verifying the new opcode is handled (or deliberately
// falls through to the default case).
static_assert(QORE_IR_MAX_OPCODE == 369,
    "New IR opcode added — review QoreIRToLLVM.cpp dispatch switch "
    "and update this assertion.  Also check QoreIRInterpreter.cpp.");

static bool isFastFunctionCallEligible(const AbstractQoreFunctionVariant* variant) {
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    return uvb && uvb->isStaticallyFastCallEligible();
}

static bool isFastMethodCallEligible(const AbstractQoreFunctionVariant* variant) {
    const auto* mvb = dynamic_cast<const MethodVariantBase*>(variant);
    return mvb
        ? mvb->isStaticallyFastMethodCallEligible()
        : isFastFunctionCallEligible(variant);
}

#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/CaseNodeRegex.h"
#include "qore/intern/QoreRegexMatchOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/SelfVarrefNode.h"
#include "qore/intern/StaticClassVarRefNode.h"
#include "qore/intern/FunctionCallNode.h"
#include "qore/intern/CallReferenceCallNode.h"
#include "qore/intern/ScopedObjectCallNode.h"
#include "qore/intern/QoreBackgroundOperatorNode.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreClosureParseNode.h"
#include "qore/intern/ParseReferenceNode.h"
#include "qore/intern/ObjectMethodReferenceNode.h"
#include "qore/intern/QoreOperatorNode.h"
#include "qore/intern/QorePseudoMethods.h"
#include "qore/intern/QoreCastOperatorNode.h"
#include "qore/intern/QoreInstanceOfOperatorNode.h"
#include <qore/QoreStringNode.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <limits>
#include <llvm/Support/raw_ostream.h>

#include <cstdio>
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

static std::string formatLLVMDateOffset(int utc_offset) {
    char buf[16];
    char sign = utc_offset < 0 ? '-' : '+';
    int offset = utc_offset < 0 ? -utc_offset : utc_offset;
    int hours = offset / 3600;
    int minutes = (offset % 3600) / 60;
    int seconds = offset % 60;
    if (seconds) {
        snprintf(buf, sizeof(buf), "%c%02d:%02d:%02d", sign, hours, minutes, seconds);
    } else {
        snprintf(buf, sizeof(buf), "%c%02d:%02d", sign, hours, minutes);
    }
    return buf;
}

static std::string getLLVMDateZoneName(const AbstractQoreZoneInfo* zone) {
    if (dynamic_cast<const QoreOffsetZoneInfo*>(zone)) {
        return formatLLVMDateOffset(AbstractQoreZoneInfo::getUTCOffset(zone));
    }

    const char* region = AbstractQoreZoneInfo::getRegionName(zone);
    if (region && *region) {
        return region;
    }

    int utc_offset = AbstractQoreZoneInfo::getUTCOffset(zone);
    return utc_offset ? formatLLVMDateOffset(utc_offset) : "UTC";
}

void QoreIRToLLVM::initTypes() {
    i64_type = llvm::Type::getInt64Ty(ctx);
    i32_type = llvm::Type::getInt32Ty(ctx);
    i1_type = llvm::Type::getInt1Ty(ctx);
    double_type = llvm::Type::getDoubleTy(ctx);
    ptr_type = llvm::PointerType::getUnqual(ctx);
    void_type = llvm::Type::getVoidTy(ctx);
}

llvm::Value* QoreIRToLLVM::getTypeInfoPointerArg(const QoreTypeInfo* ti) {
    if (!ti) {
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_type));
    }
    llvm::Value* ti_ptr = llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(ti));
    return builder->CreateIntToPtr(ti_ptr, ptr_type);
}

llvm::Value* QoreIRToLLVM::getTypePathArg(const QoreTypeInfo* ti) {
    return builder->CreateGlobalStringPtr(ti ? QoreTypeInfo::getPath(ti) : "");
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
    module.getOrInsertFunction("qore_rt_is_null_or_nothing",
            llvm::FunctionType::get(i64_type, {i64_type}, false));

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
    auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));
    // Mark as nounwind to enable CSE (common subexpression elimination)
    // Combined with the __attribute__((pure)) on the C++ function, this allows LLVM
    // to eliminate redundant exception checks in tight loops
    auto* has_ex_fn = llvm::cast<llvm::Function>(has_ex.getCallee());
    has_ex_fn->addFnAttr(llvm::Attribute::NoUnwind);

    // C++ exception check-and-throw for invoke/landingpad EH
    // Throws QoreJITException if xsink has an exception
    module.getOrInsertFunction("qore_rt_check_throw",
            llvm::FunctionType::get(void_type, {ptr_type}, false));

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
    module.getOrInsertFunction("qore_rt_make_weak_value",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));

    // Invoke helpers
    module.getOrInsertFunction("qore_rt_invoke_expr",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_make_string",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));
    module.getOrInsertFunction("qore_rt_make_string_len",
            llvm::FunctionType::get(i64_type, {ptr_type, i64_type}, false));
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

    // Container construction helpers: final ptr is QoreTypeInfo* for JIT or a type-path string for AOT.
    auto* make_seq_ft = llvm::FunctionType::get(i64_type,
            {ptr_type, llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_make_list", make_seq_ft);
    module.getOrInsertFunction("qore_rt_make_list_by_type_path", make_seq_ft);
    module.getOrInsertFunction("qore_rt_make_hash", make_seq_ft);
    module.getOrInsertFunction("qore_rt_make_hash_by_type_path", make_seq_ft);
    auto* make_hash_const_keys_ft = llvm::FunctionType::get(i64_type,
            {ptr_type, ptr_type, llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_make_hash_const_keys", make_hash_const_keys_ft);
    module.getOrInsertFunction("qore_rt_make_hash_const_keys_by_type_path", make_hash_const_keys_ft);

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
    // exec_on_block_exit_impl: (i64, ptr, i1) -> void  [with inline_lowered flag]
    module.getOrInsertFunction("qore_rt_exec_on_block_exit_impl",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type,
                llvm::Type::getInt1Ty(ctx)}, false));

    // Guard type helper: (i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_guard_type",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));

    // InstanceOf helper: (i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_instanceof",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));

    // Date construction helper: (i64, i64) -> i64
    module.getOrInsertFunction("qore_rt_make_date",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
    // Zone-preserving date construction helper:
    // (epoch_us, is_relative, zone_name, rel_y, rel_mo, rel_d, rel_h, rel_m, rel_s, rel_us) -> i64
    module.getOrInsertFunction("qore_rt_make_date_ex",
            llvm::FunctionType::get(i64_type,
                {i64_type, i64_type, ptr_type, i64_type, i64_type, i64_type, i64_type, i64_type, i64_type, i64_type},
                false));

    // Specialized access helpers (Phase 5b)
    // hash_key_access: (i64, ptr, ptr) -> i64
    module.getOrInsertFunction("qore_rt_hash_key_access",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
    // list_index_access: (i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_list_index_access",
            llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
    // list_assignment_value: (i64, i64, ptr) -> i64
    module.getOrInsertFunction("qore_rt_list_assignment_value",
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
    // typed list construction helpers: type argument is either QoreTypeInfo* (JIT) or type path (AOT)
    module.getOrInsertFunction("qore_rt_create_empty_list_typed",
            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_create_empty_list_by_type_path",
            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_create_sized_list_typed",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_create_sized_list_by_type_path",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
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
    // cleanup_run_allocas: (ptr, i32, ptr) -> void
    module.getOrInsertFunction("qore_rt_cleanup_run_allocas",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
    // reload_local_if_stale: (ptr, ptr, ptr, ptr, ptr, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_reload_local_if_stale",
            llvm::FunctionType::get(void_type,
                {ptr_type, ptr_type, ptr_type, ptr_type, ptr_type, i64_type, ptr_type}, false));
    // reload_local_if_stale_aot: (ptr, i32, ptr, ptr, ptr, ptr, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_reload_local_if_stale_aot",
            llvm::FunctionType::get(void_type,
                {ptr_type, i32_type, ptr_type, ptr_type, ptr_type, ptr_type, i64_type, ptr_type},
                false));
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
    // pop_closure_var_aot: (ptr, i32, ptr) -> void (proper cvstack pop for closure vars)
    module.getOrInsertFunction("qore_rt_pop_closure_var_aot",
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
    // load_constant_aot: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_load_constant_aot", aot_load_local_ft);
    // create_closure_aot: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_create_closure_aot", aot_load_local_ft);
    // create_parse_ref_aot: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_create_parse_ref_aot", aot_load_local_ft);
    // typed container construction AOT helpers: (ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_new_hash_decl_aot", aot_load_local_ft);
    module.getOrInsertFunction("qore_rt_new_complex_hash_aot", aot_load_local_ft);
    module.getOrInsertFunction("qore_rt_new_complex_list_aot", aot_load_local_ft);
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

    // Native `context` statement helpers (paired with opcodes 140, 361-363).
    // context_init:     (ptr, i64, i64, i64, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_context_init",
            llvm::FunctionType::get(i64_type,
                {ptr_type, i64_type, i64_type, i64_type, i32_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_context_init_throwing",
            llvm::FunctionType::get(i64_type,
                {ptr_type, i64_type, i64_type, i64_type, i32_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_context_ref_at",
            llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_context_ref_at_throwing",
            llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_context_row",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));
    module.getOrInsertFunction("qore_rt_context_row_throwing",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));
    // context_max_pos:  (i64) -> i64
    module.getOrInsertFunction("qore_rt_context_max_pos",
            llvm::FunctionType::get(i64_type, {i64_type}, false));
    // context_set_pos:  (i64, i64) -> void
    module.getOrInsertFunction("qore_rt_context_set_pos",
            llvm::FunctionType::get(void_type, {i64_type, i64_type}, false));
    // context_destroy:  (i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_context_destroy",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

    // backquote: (ptr cmd, ptr xsink) -> i64 nan-boxed string
    module.getOrInsertFunction("qore_rt_backquote",
            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_backquote_throwing",
            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));

    // find: (i64 exp, i64 source, i64 where, ptr xsink) -> i64 nan-boxed result
    module.getOrInsertFunction("qore_rt_find",
            llvm::FunctionType::get(i64_type,
                {i64_type, i64_type, i64_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_find_throwing",
            llvm::FunctionType::get(i64_type,
                {i64_type, i64_type, i64_type, ptr_type}, false));
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
    llvm::Value* is_colliding_nan = builder->CreateICmpUGE(raw_bits,
        llvm::ConstantInt::get(i64_type, 0xFFF8000000000000ULL));
    llvm::Value* positive_nan_bits = builder->CreateAnd(raw_bits,
        llvm::ConstantInt::get(i64_type, 0x7FFFFFFFFFFFFFFFULL));
    llvm::Value* safe_bits = builder->CreateSelect(is_colliding_nan, positive_nan_bits, raw_bits);
    return builder->CreateAdd(safe_bits, llvm::ConstantInt::get(i64_type, DOUBLE_ENCODE_OFFSET));
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
    closure_pre_inst_flags.clear();
    operand_remaining_uses.clear();

    // First pass: identify block-scoped locals (those with explicit UninstantiateLocal
    // or InstantiateLocal)
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (inst_ptr && (inst_ptr->opcode == QoreIROpcode::UninstantiateLocal
                    || inst_ptr->opcode == QoreIROpcode::InstantiateLocal)) {
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
    auto is_outer_scope_local = [&](const void* key) -> bool {
        return pre_instantiated_locals
            && !pre_instantiated_locals->count(key)
            && !block_scoped_locals.count(key);
    };
    bool is_first_block = true;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            if (inst->opcode == QoreIROpcode::LoadLocal || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::InstantiateLocal) {
                const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
                if (linst->local && seen.insert(linst->local).second) {
                    auto key = reinterpret_cast<const void*>(linst->local);
                    // Skip outer-scope variables: when pre_instantiated_locals is set,
                    // variables NOT in it are from an outer scope (e.g. top-level locals
                    // accessed from a sub).  These are already on the thread-local stack
                    // and must not be instantiated/uninstantiated or cached in allocas —
                    // they'll be accessed via qore_rt_load_local() on each use.
                    // Locals with explicit InstantiateLocal/UninstantiateLocal are
                    // callee-owned block-scoped locals even if all_body_locals
                    // collection missed them; they still need an alloca and local
                    // lifecycle handling.
                    if (is_outer_scope_local(key)) {
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
                        if (is_outer_scope_local(key)) {
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
                    if (!is_outer_scope_local(key)) {
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
                        if (is_outer_scope_local(key)) {
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
        std::unordered_set<const void*> body_local_set;
        if (current_ir_func) {
            for (LocalVar* var : current_ir_func->all_body_locals) {
                body_local_set.insert(reinterpret_cast<const void*>(var));
            }
        }
        // Track which callee-owned closure-use vars we've already instantiated
        // at function entry so we can instantiate any closure-use body locals
        // that were not classified as entry_locals.
        std::unordered_set<const void*> instantiated_closure_use;
        for (LocalVar* var : entry_locals) {
            const void* key = reinterpret_cast<const void*>(var);
            if (pre_instantiated_locals &&
                    pre_instantiated_locals->count(key)) {
                // Signature locals are genuinely pre-instantiated by the caller.
                // Closure-use body locals are only in pre_instantiated_locals for
                // ownership/membership and must be managed by the AOT callee.
                if (!var->closureUse() || !body_local_set.count(key)) {
                    continue;
                }
            }
            int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                    key);
            builder->CreateCall(helper, {aot_ctx_arg,
                    llvm::ConstantInt::get(i32_type, slot)});
            if (var->closureUse()) {
                instantiated_closure_use.insert(key);
            }
        }
        // Also instantiate non-block-scoped closure-use body locals that are
        // NOT entry_locals.  Without a current-frame CVV, the first access to
        // such a var can lazily instantiate it via LocalVar::getLValue, which
        // uses a name-based walk-all lookup; for recursive calls, that lookup
        // can find an outer frame's CVV and cause cross-frame aliasing.  Locals
        // with explicit block scope are instantiated at closure load/store and
        // popped by the explicit UninstantiateLocal lowering.
        if (current_ir_func) {
            for (LocalVar* var : current_ir_func->all_body_locals) {
                if (!var || !var->closureUse()) {
                    continue;
                }
                const void* key = reinterpret_cast<const void*>(var);
                if (instantiated_closure_use.count(key)) {
                    continue;
                }
                if (block_scoped_locals.count(key)) {
                    continue;
                }
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot)});
                instantiated_closure_use.insert(key);
            }
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
                    fast_entry_param_allocas_by_local[key] = alloca;
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

        // AOT body locals are pre-instantiated with NOTHING by the runtime
        // frame wrapper.  Loading them back from the runtime stack at entry
        // only adds thousands of calls; StoreLocal/lvalue mutation paths force
        // a reload after publishing an actual value.  Closure-use vars are also
        // skipped because LLVM manages their cvstack lifetime explicitly.
        bool is_aot_body_local = aot_body_locals.count(key) > 0;
        bool skip_pre_inst_load = aot_mode && (var->closureUse() || is_aot_body_local);
        if (pre_instantiated_locals && pre_instantiated_locals->count(key)
                && !ir_only_body_locals.count(key) && !skip_pre_inst_load) {
            // Pre-instantiated and NOT IR-only: initialize from runtime stack
            llvm::AllocaInst* boxed_cleanup = nullptr;
            if (!is_native_int && !is_native_float) {
                boxed_cleanup = alloca_builder.CreateAlloca(i64_type, nullptr,
                        "preinst_cleanup");
                alloca_builder.CreateStore(
                        llvm::ConstantInt::get(i64_type, VAL_NOTHING), boxed_cleanup);
                preinstantiated_entry_cleanup_allocas.push_back(boxed_cleanup);
                preinstantiated_entry_cleanup_by_local[key] = boxed_cleanup;
            }
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
                if (boxed_cleanup) {
                    alloca_builder.CreateStore(init_val, boxed_cleanup);
                } else {
                    preinstantiated_entry_loads.push_back(init_val);
                }
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
                if (boxed_cleanup) {
                    alloca_builder.CreateStore(init_val, boxed_cleanup);
                } else {
                    preinstantiated_entry_loads.push_back(init_val);
                }
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

    // Create i1 flag allocas for pre-instantiated closure-use block-scoped locals.
    // These track whether the CVV is currently on the cvstack:
    //   true  = instantiated (normal state and evalTiered's initial pre-instantiation)
    //   false = popped by UninstantiateLocal (needs re-push before next StoreLocal or exit)
    if (!aot_mode && pre_instantiated_locals) {
        llvm::Type* i1_type = llvm::Type::getInt1Ty(module.getContext());
        for (LocalVar* var : function_locals) {
            auto key = reinterpret_cast<const void*>(var);
            if (pre_instantiated_locals->count(key)
                    && block_scoped_locals.count(key)
                    && var->closureUse()
                    && !ir_only_body_locals.count(key)) {
                llvm::AllocaInst* flag = alloca_builder.CreateAlloca(i1_type, nullptr,
                        "closure_pre_inst_active");
                // Starts as true: evalTiered pre-instantiated this var before execute()
                alloca_builder.CreateStore(llvm::ConstantInt::get(i1_type, 1), flag);
                closure_pre_inst_flags[key] = flag;
            }
        }
    }
}

void QoreIRToLLVM::emitLocalUninstantiation(llvm::Module& module) {
    // Uninstantiate entry-block locals at function exit.
    // Non-entry-block locals have their own UninstantiateLocal instructions
    // in the IR that handle their lifecycle.
    if (aot_mode) {
        auto helper = module.getOrInsertFunction("qore_rt_uninstantiate_local_aot",
                llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
        auto pop_helper = module.getOrInsertFunction("qore_rt_pop_closure_var_aot",
                llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
        std::unordered_set<const void*> body_local_set;
        if (current_ir_func) {
            for (LocalVar* var : current_ir_func->all_body_locals) {
                body_local_set.insert(reinterpret_cast<const void*>(var));
            }
        }
        // First pop closure-use body locals that were pre-instantiated at function
        // entry by emitLocalInstantiation (the all_body_locals loop). These are NOT
        // in entry_locals, so the entry_locals reverse loop below won't cover them.
        // They must be popped BEFORE entry_locals (reverse of instantiation order:
        // entry_locals were instantiated first, body locals second).
        if (current_ir_func) {
            std::unordered_set<const void*> entry_local_set;
            for (LocalVar* var : entry_locals) {
                entry_local_set.insert(reinterpret_cast<const void*>(var));
            }
            // Reverse iterate to match instantiation order symmetry
            for (auto it = current_ir_func->all_body_locals.rbegin();
                    it != current_ir_func->all_body_locals.rend(); ++it) {
                LocalVar* var = *it;
                if (!var || !var->closureUse()) {
                    continue;
                }
                const void* key = reinterpret_cast<const void*>(var);
                if (entry_local_set.count(key)) {
                    continue;  // Already handled by the entry_locals loop below
                }
                if (block_scoped_locals.count(key)) {
                    continue;  // Popped by the explicit UninstantiateLocal lowering
                }
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                builder->CreateCall(pop_helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), xsink_arg});
            }
        }
        for (auto it = entry_locals.rbegin(); it != entry_locals.rend(); ++it) {
            const void* key = reinterpret_cast<const void*>(*it);
            if (pre_instantiated_locals &&
                    pre_instantiated_locals->count(key)) {
                // Signature locals are caller-owned and must be popped by the
                // dispatch helper. Closure-use body locals are callee-owned and
                // were instantiated by emitLocalInstantiation, so pop them here.
                if ((*it)->closureUse() && body_local_set.count(key)) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                            key);
                    builder->CreateCall(pop_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                }
                continue;
            }
            int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                    key);
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

void QoreIRToLLVM::emitPreInstClosureReInstantiation(llvm::Module& module) {
    if (closure_pre_inst_flags.empty()) {
        return;
    }
    // For each pre-instantiated closure-use block-scoped local whose CVV was popped
    // mid-execution (flag == false), push an empty CVV so evalTiered's cleanup can
    // pop exactly one CVV per var (maintaining the cvstack invariant).
    llvm::Function* func = builder->GetInsertBlock()->getParent();
    llvm::Type* i1_type = llvm::Type::getInt1Ty(module.getContext());
    auto inst_helper = module.getOrInsertFunction("qore_rt_instantiate_local",
            llvm::FunctionType::get(void_type, {ptr_type}, false));

    for (auto& [key, flag] : closure_pre_inst_flags) {
        llvm::Value* is_active = builder->CreateLoad(i1_type, flag);
        // If active, no re-instantiation needed
        llvm::BasicBlock* skip_block = llvm::BasicBlock::Create(
                module.getContext(), "closure_reinst_skip", func);
        llvm::BasicBlock* reinst_block = llvm::BasicBlock::Create(
                module.getContext(), "closure_reinst_exit", func);
        builder->CreateCondBr(is_active, skip_block, reinst_block);

        builder->SetInsertPoint(reinst_block);
        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                reinterpret_cast<uint64_t>(key));
        llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
        builder->CreateCall(inst_helper, {var_as_ptr});
        builder->CreateBr(skip_block);

        builder->SetInsertPoint(skip_block);
    }
}

void QoreIRToLLVM::syncLocalsToRuntimeForHandlers(llvm::Module& module) {
    if (!has_on_block_exit_handlers || local_allocas.empty()) {
        return;
    }

    auto sync_jit = module.getOrInsertFunction("qore_rt_sync_local",
            llvm::FunctionType::get(void_type, {ptr_type, i64_type}, false));
    auto sync_aot = module.getOrInsertFunction("qore_rt_sync_local_aot",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type}, false));
    auto decref_nothrow = module.getOrInsertFunction("qore_rt_decref_nothrow",
            llvm::FunctionType::get(void_type, {i64_type}, false));

    for (auto& [key, alloca] : local_allocas) {
        if (ir_only_locals_set && ir_only_locals_set->count(key)) {
            continue;
        }

        auto* local = reinterpret_cast<const LocalVar*>(key);
        if (local && local->closureUse()) {
            continue;
        }

        ensureLocalCacheFresh(key, module, builder->GetInsertBlock()->getParent());

        llvm::Value* val;
        bool boxed_temp = false;
        if (native_int_locals.count(key)) {
            val = boxInt(builder->CreateLoad(i64_type, alloca));
            boxed_temp = true;
        } else if (native_float_locals.count(key)) {
            val = boxFloat(builder->CreateLoad(double_type, alloca));
        } else {
            val = builder->CreateLoad(i64_type, alloca);
        }

        if (aot_mode) {
            int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
            builder->CreateCall(sync_aot, {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), val});
        } else {
            llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(key));
            llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
            builder->CreateCall(sync_jit, {var_as_ptr, val});
        }

        if (boxed_temp) {
            builder->CreateCall(decref_nothrow, {val});
        }
    }
}

llvm::Value* QoreIRToLLVM::beginNativeHandlerSlotCache(llvm::Module& module) {
    if (!has_on_block_exit_handlers || !current_ir_func) {
        return nullptr;
    }

    int32_t slot_count = current_ir_func->local_var_slots.empty()
        ? 0 : static_cast<int32_t>(current_ir_func->max_local_slot_id) + 1;
    auto begin_helper = module.getOrInsertFunction("qore_rt_begin_native_ir_slot_cache",
            llvm::FunctionType::get(ptr_type, {i32_type}, false));
    llvm::Value* guard = builder->CreateCall(begin_helper,
            {llvm::ConstantInt::get(i32_type, slot_count)});

    if (slot_count == 0 || local_allocas.empty()) {
        return guard;
    }

    auto set_jit = module.getOrInsertFunction("qore_rt_set_native_ir_slot_cache_value",
            llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type, i64_type}, false));
    auto set_aot = module.getOrInsertFunction("qore_rt_set_native_ir_slot_cache_value_aot",
            llvm::FunctionType::get(void_type,
                {ptr_type, ptr_type, i32_type, i32_type, i64_type}, false));
    auto decref_nothrow = module.getOrInsertFunction("qore_rt_decref_nothrow",
            llvm::FunctionType::get(void_type, {i64_type}, false));

    for (const auto& [local, ir_slot] : current_ir_func->local_var_slots) {
        if (!local || ir_slot >= static_cast<uint32_t>(slot_count)) {
            continue;
        }
        if (local->closureUse()) {
            // Closure-use locals live on the cvstack / closure environment.  Their
            // LLVM allocas are not authoritative, so publishing them through the
            // native handler slot cache can overwrite the real CVV value.
            continue;
        }

        const void* key = reinterpret_cast<const void*>(local);
        auto it = local_allocas.find(key);
        if (it == local_allocas.end()) {
            continue;
        }
        ensureLocalCacheFresh(key, module, builder->GetInsertBlock()->getParent());

        llvm::Value* val;
        bool boxed_temp = false;
        if (native_int_locals.count(key)) {
            val = boxInt(builder->CreateLoad(i64_type, it->second));
            boxed_temp = true;
        } else if (native_float_locals.count(key)) {
            val = boxFloat(builder->CreateLoad(double_type, it->second));
        } else {
            val = builder->CreateLoad(i64_type, it->second);
        }

        if (aot_mode) {
            int32_t local_slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
            builder->CreateCall(set_aot, {aot_ctx_arg, guard,
                    llvm::ConstantInt::get(i32_type, ir_slot),
                    llvm::ConstantInt::get(i32_type, local_slot), val});
        } else {
            llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(local));
            llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
            builder->CreateCall(set_jit, {guard, llvm::ConstantInt::get(i32_type, ir_slot), var_as_ptr, val});
        }

        if (boxed_temp) {
            builder->CreateCall(decref_nothrow, {val});
        }
    }

    return guard;
}

void QoreIRToLLVM::endNativeHandlerSlotCache(llvm::Module& module, llvm::Value* guard) {
    if (!guard) {
        return;
    }

    auto end_helper = module.getOrInsertFunction("qore_rt_end_native_ir_slot_cache",
            llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false));
    builder->CreateCall(end_helper, {guard, xsink_arg});
}

void QoreIRToLLVM::emitOnBlockExitExec(llvm::Module& module) {
    if (!obe_saved_count) {
        return;
    }
    llvm::Function* llvm_func = builder->GetInsertBlock()->getParent();
    syncLocalsToRuntimeForHandlers(module);
    llvm::Value* native_slot_cache = beginNativeHandlerSlotCache(module);
    auto helper = module.getOrInsertFunction("qore_rt_exec_on_block_exit",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    builder->CreateCall(helper, {obe_saved_count, xsink_arg});
    endNativeHandlerSlotCache(module, native_slot_cache);
    // On-block-exit handlers execute user code and can mutate AOT body locals
    // through the runtime local stack.  Refresh eagerly here: fused local ops
    // can read allocas directly, so a lazy epoch alone is not sufficient.
    reloadAllLocalsFromRuntime(module, llvm_func, false, true);
}

void QoreIRToLLVM::emitPreinstantiatedCleanup(llvm::Module& module) {
    // Re-instantiate closure-use pre-instantiated block-scoped locals that were popped
    // mid-execution (e.g. loop-body closure captures) so evalTiered's cleanup can pop
    // exactly one CVV per var.  Must happen before function return.
    emitPreInstClosureReInstantiation(module);

    auto helper = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

    // Standard pre-instantiated entry loads: decref the originally loaded value
    for (llvm::Value* entry_val : preinstantiated_entry_loads) {
        builder->CreateCall(helper, {entry_val, xsink_arg});
    }

    // Boxed pre-instantiated entry loads use cleanup slots so lvalue mutation
    // can clear them before function exit.  If already cleared, this is a no-op.
    for (llvm::AllocaInst* alloca : preinstantiated_entry_cleanup_allocas) {
        llvm::Value* val = builder->CreateLoad(i64_type, alloca);
        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                alloca);
        builder->CreateCall(helper, {val, xsink_arg});
    }

    // Fast entry param allocas: load current value and decref.
    // This correctly handles both:
    //   - Non-reassigned params: decrefs the original incref'd value
    //   - Reassigned params: decrefs the final value (intermediate values were
    //     decreff'd by decref-before-store in StoreLocal for IR-only locals)
    for (llvm::AllocaInst* alloca : fast_entry_param_allocas) {
        llvm::Value* val = builder->CreateLoad(i64_type, alloca);
        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                alloca);
        builder->CreateCall(helper, {val, xsink_arg});
    }

    // Boxed IR-only locals not represented on the runtime stack own their
    // current alloca value directly.
    for (llvm::AllocaInst* alloca : owned_ir_local_allocas) {
        llvm::Value* val = builder->CreateLoad(i64_type, alloca);
        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                alloca);
        builder->CreateCall(helper, {val, xsink_arg});
    }
}

void QoreIRToLLVM::emitInvokeCleanup(llvm::Module& module) {
    if (invoke_result_allocas.empty()) {
        return;
    }
    if (invoke_cleanup_array && !invoke_cleanup_array_overflow
            && invoke_cleanup_array_count == invoke_result_allocas.size()
            && invoke_cleanup_array_count >= 32) {
        auto helper = module.getOrInsertFunction("qore_rt_cleanup_run_allocas",
                llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
        builder->CreateCall(helper, {invoke_cleanup_array,
            llvm::ConstantInt::get(i32_type, invoke_cleanup_array_count), xsink_arg});
        return;
    }
    auto helper = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    // Match qore_rt_cleanup_run_allocas(): invoke-result cleanups are
    // registered in construction order, so destroy them in LIFO order.
    for (auto it = invoke_result_allocas.rbegin(); it != invoke_result_allocas.rend(); ++it) {
        llvm::Value* alloca_ptr = *it;
        llvm::Value* val = builder->CreateLoad(i64_type, alloca_ptr);
        builder->CreateCall(helper, {val, xsink_arg});
        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
            alloca_ptr);
    }
}

void QoreIRToLLVM::emitDiscardTemps(llvm::Module& module) {
    TempCleanupMark mark;
    if (!temp_cleanup_marks.empty()) {
        mark = temp_cleanup_marks.back();
        temp_cleanup_marks.pop_back();
    }

    auto helper = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

    for (size_t i = invoke_result_allocas.size(); i > mark.invoke_alloca_count; --i) {
        llvm::Value* alloca_ptr = invoke_result_allocas[i - 1];
        if (persistent_cleanup_allocas.count(alloca_ptr)) {
            continue;
        }
        llvm::Value* val = builder->CreateLoad(i64_type, alloca_ptr);
        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
            alloca_ptr);
        builder->CreateCall(helper, {val, xsink_arg});
    }

    if (pending_ssa_cleanup.size() > mark.pending_ssa_count) {
        llvm::BasicBlock* cur = builder->GetInsertBlock();
        for (size_t i = pending_ssa_cleanup.size(); i > mark.pending_ssa_count; --i) {
            const SsaCleanupEntry& e = pending_ssa_cleanup[i - 1];
            if (e.def_bb == cur || dominates(e.def_bb, cur)) {
                builder->CreateCall(helper, {e.value, xsink_arg});
            } else {
                llvm::AllocaInst* alloca = promoteSsaEntryToAlloca(e.result_id,
                    module, builder->GetInsertBlock()->getParent());
                if (alloca) {
                    llvm::Value* val = builder->CreateLoad(i64_type, alloca);
                    builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                        alloca);
                    builder->CreateCall(helper, {val, xsink_arg});
                }
            }
        }
        pending_ssa_cleanup.resize(mark.pending_ssa_count);
    }
}

unsigned QoreIRToLLVM::estimateInvokeCleanupArrayCapacity(const QoreIRFunction& func) const {
    size_t inst_count = 0;
    for (const auto& block : func.blocks) {
        inst_count += block->instructions.size();
    }

    size_t rough = static_cast<size_t>(func.max_value_id)
        + (inst_count * 2)
        + (static_cast<size_t>(func.total_local_count) * 8);
    if (rough < 32) {
        return 0;
    }
    size_t capacity = rough + 16;
    if (capacity > static_cast<size_t>(std::numeric_limits<unsigned>::max())) {
        return std::numeric_limits<unsigned>::max();
    }
    return static_cast<unsigned>(capacity);
}

void QoreIRToLLVM::registerInvokeCleanupAlloca(llvm::Value* alloca_ptr) {
    invoke_result_allocas.push_back(alloca_ptr);
    if (!invoke_cleanup_array || invoke_cleanup_array_overflow) {
        return;
    }
    if (invoke_cleanup_array_count >= invoke_cleanup_array_capacity) {
        invoke_cleanup_array_overflow = true;
        return;
    }

    llvm::BasicBlock& entry = *invoke_cleanup_array->getParent();
    llvm::Instruction* insert_after = invoke_cleanup_array;
    if (auto* inst = llvm::dyn_cast<llvm::Instruction>(alloca_ptr)) {
        if (inst->getParent() == &entry && invoke_cleanup_array->comesBefore(inst)) {
            insert_after = inst;
        }
    }

    llvm::IRBuilder<> reg_builder(ctx);
    if (llvm::Instruction* next = insert_after->getNextNode()) {
        reg_builder.SetInsertPoint(next);
    } else {
        reg_builder.SetInsertPoint(&entry);
    }

    llvm::Value* idx = llvm::ConstantInt::get(i32_type, invoke_cleanup_array_count++);
    llvm::Value* gep = reg_builder.CreateGEP(ptr_type, invoke_cleanup_array, idx,
            "cleanup_slot_ptr");
    reg_builder.CreateStore(alloca_ptr, gep);
}

void QoreIRToLLVM::registerPersistentCleanupAlloca(llvm::Value* alloca_ptr) {
    persistent_cleanup_allocas.insert(alloca_ptr);
    registerInvokeCleanupAlloca(alloca_ptr);
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
        builder->CreateStore(llvm::ConstantPointerNull::get(
            llvm::cast<llvm::PointerType>(ptr_type)), alloca_ptr);
    }
}

void QoreIRToLLVM::emitLateExitCleanup(llvm::Function* llvm_func,
        llvm::Module& module) {
    if (invoke_result_allocas.empty() && iterator_cleanup_allocas.empty()) {
        return;
    }

    std::vector<llvm::Instruction*> exits;
    for (llvm::BasicBlock& bb : *llvm_func) {
        llvm::Instruction* term = bb.getTerminator();
        if (term && (llvm::isa<llvm::ReturnInst>(term)
                || llvm::isa<llvm::ResumeInst>(term))) {
            exits.push_back(term);
        }
    }

    llvm::BasicBlock* saved_insert = builder->GetInsertBlock();
    auto saved_ip = builder->GetInsertPoint();
    for (llvm::Instruction* term : exits) {
        builder->SetInsertPoint(term);
        emitIteratorCleanup(module);
        emitInvokeCleanup(module);
    }
    if (saved_insert) {
        builder->SetInsertPoint(saved_insert, saved_ip);
    }
}

void QoreIRToLLVM::emitLifetimeAnnotations(llvm::Function* llvm_func) {
    // Annotate entry-block alloca lifetimes so SROA/mem2reg can reason about
    // when each alloca's storage is live and promote promotable ones to SSA.
    // Without this, large functions accumulate mutually-aliasing alloca
    // forests that make LLVM's -O3 pipeline go quadratic.
    //
    // Strategy: post-process after all lowering is done.
    //   - lifetime.start goes immediately after each entry-block alloca
    //   - lifetime.end goes immediately before every ret / resume terminator
    // Whole-function lifetimes, but still enabling SROA since storage is
    // explicitly marked dead before entry and after every exit.  Cleanup-
    // flag allocas are excluded because they're read at function exit by
    // the shared cleanup sequence and cannot be promoted regardless —
    // annotating them would just add intrinsic-call noise.
    const llvm::DataLayout& dl = llvm_func->getParent()->getDataLayout();
    llvm::BasicBlock& entry = llvm_func->getEntryBlock();

    // Cleanup-flag allocas cannot be promoted (read at function exit by the
    // shared cleanup sequence).  Only annotate promotable kinds.
    auto isPromotable = [](const llvm::StringRef& name) {
        if (name.empty()) return false;
        if (name.starts_with("cleanup")) return false;
        if (name.starts_with("lv_cleanup")) return false;
        if (name.starts_with("lvp_cleanup")) return false;
        if (name.starts_with("box_cleanup")) return false;
        if (name.starts_with("coerce_cleanup")) return false;
        return true;
    };

    std::vector<llvm::AllocaInst*> allocas;
    allocas.reserve(32);
    for (llvm::Instruction& inst : entry) {
        auto* ai = llvm::dyn_cast<llvm::AllocaInst>(&inst);
        if (!ai) continue;
        if (!isPromotable(ai->getName())) continue;
        allocas.push_back(ai);
    }
    if (allocas.empty()) {
        return;
    }

    // Compute a constant byte size per alloca (or -1 if dynamic).  Lifetime
    // intrinsics require a constant size; -1 is the "unknown / conservative
    // whole-alloca" sentinel.
    auto sizeFor = [&](llvm::AllocaInst* ai) -> int64_t {
        llvm::TypeSize ts = dl.getTypeAllocSize(ai->getAllocatedType());
        if (ts.isScalable()) {
            return -1;
        }
        uint64_t elem = ts.getFixedValue();
        llvm::Value* count = ai->getArraySize();
        if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(count)) {
            return static_cast<int64_t>(elem * ci->getZExtValue());
        }
        return -1;
    };

    // Insert lifetime.start right after each alloca.
    llvm::IRBuilder<> start_builder(llvm_func->getContext());
    for (llvm::AllocaInst* ai : allocas) {
        int64_t sz = sizeFor(ai);
        if (sz <= 0) {
            sz = -1;
        }
        llvm::Instruction* next = ai->getNextNode();
        if (next) {
            start_builder.SetInsertPoint(next);
        } else {
            start_builder.SetInsertPoint(&entry);
        }
        start_builder.CreateLifetimeStart(ai,
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvm_func->getContext()), sz));
    }

    // Emit lifetime.end before every ret / resume terminator.
    llvm::IRBuilder<> end_builder(llvm_func->getContext());
    for (llvm::BasicBlock& bb : *llvm_func) {
        llvm::Instruction* term = bb.getTerminator();
        if (!term) {
            continue;
        }
        if (!llvm::isa<llvm::ReturnInst>(term) && !llvm::isa<llvm::ResumeInst>(term)) {
            continue;
        }
        end_builder.SetInsertPoint(term);
        for (llvm::AllocaInst* ai : allocas) {
            int64_t sz = sizeFor(ai);
            if (sz <= 0) {
                sz = -1;
            }
            end_builder.CreateLifetimeEnd(ai,
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(llvm_func->getContext()), sz));
        }
    }
}

bool QoreIRToLLVM::isOnStraightLineChain(llvm::BasicBlock* bb) {
    if (!bb || !entry_block_for_idom) {
        return false;
    }
    if (bb == entry_block_for_idom) {
        return true;
    }
    // Walk idom chain; if every hop has exactly one predecessor and we
    // eventually hit entry, bb is on the single-pred chain.  Lazy-
    // compute each hop so mid-block helper BBs (invoke cont, guard
    // continuation) are covered without an explicit instrumenting hook.
    llvm::BasicBlock* cur = bb;
    std::unordered_set<llvm::BasicBlock*> visited;
    visited.reserve(16);
    while (cur && cur != entry_block_for_idom) {
        if (!visited.insert(cur).second) {
            return false;
        }
        llvm::BasicBlock* idom = getOrComputeImmediateDominator(cur);
        if (!idom) {
            return false;
        }
        cur = idom;
    }
    return cur == entry_block_for_idom;
}

bool QoreIRToLLVM::canUseSsaCleanup(llvm::BasicBlock* current_bb) {
    if (!aot_eh_enabled) {
        return false;
    }
    if (!last_call_was_invoke_eh) {
        return false;
    }
    // Disabled: SSA-direct tracking triggers "Instruction does not dominate
    // all uses" verifier errors on HttpServer functions with try/catch +
    // later emitCondBrWithSsaPreamble. Values pushed on the straight-line
    // chain before a try block remain in pending_ssa_cleanup after the
    // try exits normally; at a later CondBr in post-try-merge (which has
    // multiple preds and thus isn't dominated by the try body), the
    // promotePendingSsaToAllocas store references the SSA value from a
    // non-dominating def.
    //
    // HS profiling (2026-04-18): 43 of 198 HttpServer functions fail to
    // AOT-compile with this error. Net effect: those functions fall back
    // to source interpretation at runtime — a performance regression in
    // disguise. Keeping the EH invoke infrastructure (per-invoke LPs with
    // shared function_unwind_lp fast path) but disabling SSA-direct
    // tracking restores all 198 functions to AOT compilation with
    // compile-time equivalent to the noEH baseline (~836s solo).
    //
    // Re-enabling SSA-direct requires scope-boundary handling: pending
    // entries must be promoted/decref'd when leaving the scope they were
    // pushed in (try body, catch arm, etc.), not lazily at the next
    // CondBr. See design/aot-eh-cleanup-dominance.md for the
    // detailed analysis. Until that work lands, the conservative path is
    // correct.
    (void)current_bb;
    last_call_was_invoke_eh = false;
    return false;
}

llvm::BasicBlock* QoreIRToLLVM::createPerInvokeCleanupLP(llvm::Module& module,
        llvm::Function* llvm_func, llvm::BasicBlock* invoke_bb) {
    // Fast path — nothing SSA-direct live at this invoke site: reuse the
    // shared function-level unwind landing pad.  Avoids exploding the
    // BB count on functions with many EH invokes and no SSA-direct
    // cleanup (deferred-mode functions, functions that promoted early).
    bool has_ssa_entries = false;
    for (auto it = pending_ssa_cleanup.rbegin(); it != pending_ssa_cleanup.rend(); ++it) {
        if (it->def_bb == invoke_bb || dominates(it->def_bb, invoke_bb)) {
            has_ssa_entries = true;
            break;
        }
    }
    if (!has_ssa_entries) {
        return getOrCreateFunctionUnwindLP(module, llvm_func);
    }

    llvm::BasicBlock* lp_bb = llvm::BasicBlock::Create(ctx, "inv_lp", llvm_func);

    // Remember the builder's insert point so we can restore it after
    // populating the LP (caller's control flow continues on the normal
    // edge of the invoke, not on the unwind edge).
    auto* saved_insert = builder->GetInsertBlock();
    auto saved_ip = builder->GetInsertPoint();

    builder->SetInsertPoint(lp_bb);
    llvm::Type* lp_type = llvm::StructType::get(ctx, {ptr_type, i32_type});
    llvm::LandingPadInst* lp = builder->CreateLandingPad(lp_type, 0);
    lp->setCleanup(true);

    // The LP is dominated only by invoke_bb.  A pending_ssa_cleanup entry
    // may be referenced here only if its def_bb dominates invoke_bb —
    // LLVM SSA dominance is transitive, so dominance at invoke_bb
    // implies dominance at the LP's unwind edge.
    auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    for (auto it = pending_ssa_cleanup.rbegin(); it != pending_ssa_cleanup.rend(); ++it) {
        if (!dominates(it->def_bb, invoke_bb) && it->def_bb != invoke_bb) {
            continue;
        }
        builder->CreateCall(decref_fn, {it->value, xsink_arg});
    }

    // Lazily create the shared common-cleanup block; all per-invoke LPs
    // funnel into it to keep the cleanup tail out of hot paths.
    if (!function_common_cleanup) {
        function_common_cleanup = llvm::BasicBlock::Create(ctx,
                "function_common_cleanup", llvm_func);
    }
    builder->CreateBr(function_common_cleanup);

    common_cleanup_phi_preds.push_back(lp_bb);
    common_cleanup_phi_values.push_back(lp);

    if (saved_insert) {
        builder->SetInsertPoint(saved_insert, saved_ip);
    }
    return lp_bb;
}

void QoreIRToLLVM::finalizeFunctionCommonCleanup(llvm::Module& module) {
    if (!function_common_cleanup) {
        return;
    }
    auto* saved_insert = builder->GetInsertBlock();
    auto saved_ip = builder->GetInsertPoint();

    builder->SetInsertPoint(function_common_cleanup);

    llvm::Type* lp_type = llvm::StructType::get(ctx, {ptr_type, i32_type});
    llvm::PHINode* phi = builder->CreatePHI(lp_type,
            static_cast<unsigned>(common_cleanup_phi_preds.size()), "lp_merged");
    for (size_t i = 0; i < common_cleanup_phi_preds.size(); ++i) {
        phi->addIncoming(common_cleanup_phi_values[i], common_cleanup_phi_preds[i]);
    }

    // Shared tail mirrors error_return_block's non-per-temp cleanup set.
    // Per-temp SSA decrefs were handled per-LP before branching here.
    // invoke_result_allocas still carries non-per-temp allocas (coerce,
    // reload tracker, promoted per-temps if any) — always decref them.
    emitOnBlockExitExec(module);
    emitIteratorCleanup(module);
    emitPreinstantiatedCleanup(module);
    emitInvokeCleanup(module);
    emitPreInstClosureReInstantiation(module);
    emitLocalUninstantiation(module);
    builder->CreateResume(phi);

    if (saved_insert) {
        builder->SetInsertPoint(saved_insert, saved_ip);
    }
}

void QoreIRToLLVM::emitPendingSsaCleanup(llvm::Module& module) {
    if (pending_ssa_cleanup.empty()) {
        return;
    }
    auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    llvm::BasicBlock* cur = builder->GetInsertBlock();
    for (auto it = pending_ssa_cleanup.rbegin(); it != pending_ssa_cleanup.rend(); ++it) {
        if (!dominates(it->def_bb, cur) && it->def_bb != cur) {
            continue;
        }
        builder->CreateCall(decref_fn, {it->value, xsink_arg});
    }
}

void QoreIRToLLVM::promotePendingSsaToAllocas(llvm::Module& module,
        llvm::Function* llvm_func) {
    if (pending_ssa_cleanup.empty()) {
        return;
    }
    llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
    llvm::IRBuilder<> alloca_builder(entry, entry->begin());
    for (const SsaCleanupEntry& e : pending_ssa_cleanup) {
        llvm::AllocaInst* alloca = alloca_builder.CreateAlloca(i64_type, nullptr,
                "cleanup_promoted");
        // Init to NOTHING so block-scoped alloca safety holds: if the
        // store we emit next is bypassed (shouldn't happen in well-formed
        // IR), emitInvokeCleanup still sees a safe no-op value.
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                alloca);
        builder->CreateStore(e.value, alloca);
        registerInvokeCleanupAlloca(alloca);
    }
    pending_ssa_cleanup.clear();
}

void QoreIRToLLVM::emitCondBrWithSsaPreamble(llvm::Module& module,
        llvm::Function* llvm_func, llvm::Value* cond,
        llvm::BasicBlock* exception_target, llvm::BasicBlock* normal_target) {
    // Spill pending SSA-direct entries to cleanup allocas BEFORE the
    // branch so both the exception and normal edges see an empty
    // pending_ssa_cleanup downstream (no per-path divergence in the
    // flat cleanup list).  The allocas are then decref'd by the shared
    // target's emitInvokeCleanup (error_return_block, function_common_
    // cleanup, Return's tail).  A per-site SSA-decref preamble BB
    // variant was tried for targets with no existing preds: it did
    // save a few allocas (Logger 1001 -> 969) but the extra BBs
    // regressed HttpServer compile time from 13m26s back to 15m38s —
    // LLVM codegen's per-BB overhead exceeded the alloca-elimination
    // win.  Stick with the in-block promote.
    promotePendingSsaToAllocas(module, llvm_func);
    builder->CreateCondBr(cond, exception_target, normal_target);
}

llvm::AllocaInst* QoreIRToLLVM::promoteSsaEntryToAlloca(uint32_t result_id,
        llvm::Module& module, llvm::Function* llvm_func) {
    auto map_it = invoke_alloca_map.find(result_id);
    if (map_it == invoke_alloca_map.end()) {
        return nullptr;
    }
    if (map_it->second) {
        return static_cast<llvm::AllocaInst*>(map_it->second);
    }
    auto val_it = values.find(result_id);
    if (val_it == values.end() || !val_it->second) {
        return nullptr;
    }
    llvm::Value* val = val_it->second;
    llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
    llvm::IRBuilder<> alloca_builder(entry, entry->begin());
    llvm::AllocaInst* ca = alloca_builder.CreateAlloca(i64_type, nullptr,
            "cleanup_promoted");
    alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), ca);
    builder->CreateStore(val, ca);
    registerInvokeCleanupAlloca(ca);
    invoke_alloca_map[result_id] = ca;
    // Remove the matching SSA entry so the normal-exit and per-invoke LP
    // cleanup paths don't double-decref the promoted value.
    for (auto it = pending_ssa_cleanup.begin(); it != pending_ssa_cleanup.end(); ) {
        if (it->value == val) {
            it = pending_ssa_cleanup.erase(it);
        } else {
            ++it;
        }
    }
    return ca;
}

// Conservative Step 1 idom computation: exactly one predecessor ->
// that predecessor is the idom; otherwise nullptr.  Shared by the
// explicit update hook in the block-iteration loop and the lazy path
// used inside dominance queries for mid-block helper BBs (invoke cont,
// guard continuation, check-cont, etc.) that aren't in func.blocks.
llvm::BasicBlock* QoreIRToLLVM::getOrComputeImmediateDominator(
        llvm::BasicBlock* bb) {
    if (!bb) {
        return nullptr;
    }
    if (bb == entry_block_for_idom) {
        immediate_dominator[bb] = nullptr;
        return nullptr;
    }
    auto it = immediate_dominator.find(bb);
    if (it != immediate_dominator.end()) {
        return it->second;
    }
    // Lazy compute based on current LLVM predecessors.  Note: LLVM
    // predecessors() reflects the CFG wired up to this point in lowering.
    // Backedges from not-yet-lowered blocks are invisible, which is fine
    // for Step 2's strict predicate (values pushed later have their idom
    // recomputed at their use sites if they're ever re-queried).
    llvm::BasicBlock* single = nullptr;
    int count = 0;
    for (llvm::BasicBlock* pred : llvm::predecessors(bb)) {
        ++count;
        if (count > 1) {
            single = nullptr;
            break;
        }
        single = pred;
    }
    llvm::BasicBlock* idom = (count == 1) ? single : nullptr;
    immediate_dominator[bb] = idom;
    return idom;
}

void QoreIRToLLVM::updateImmediateDominator(llvm::BasicBlock* bb) {
    (void)getOrComputeImmediateDominator(bb);
}

bool QoreIRToLLVM::dominates(llvm::BasicBlock* candidate, llvm::BasicBlock* target) {
    if (!candidate || !target) {
        return false;
    }
    if (candidate == target) {
        return true;
    }
    // Walk idom chain from target upward, lazily filling in idoms for
    // mid-block helper BBs.  Defensive visited set guards against
    // malformed idom entries producing cycles.
    llvm::BasicBlock* cur = target;
    std::unordered_set<llvm::BasicBlock*> visited;
    visited.reserve(8);
    while (cur) {
        llvm::BasicBlock* next = getOrComputeImmediateDominator(cur);
        if (!next) {
            return false;
        }
        if (next == candidate) {
            return true;
        }
        if (!visited.insert(next).second) {
            return false;
        }
        cur = next;
    }
    return false;
}

void QoreIRToLLVM::trackResultForCleanup(llvm::Value* result, uint32_t result_id,
        llvm::Function* llvm_func) {
    // Phase 2B — SSA-direct path: when the just-emitted call went through
    // emitMaybeInvoke's EH path AND the current block is on the entry
    // single-pred chain, track the result as an SSA entry whose lifetime
    // is handled by per-invoke cleanup LPs and normal-exit emitPendingSsaCleanup.
    // No entry-block alloca is allocated — SROA/mem2reg can now collapse
    // the flag-alloca forest that used to dominate HttpServer's compile time.
    if (canUseSsaCleanup(builder->GetInsertBlock())) {
        pending_ssa_cleanup.push_back({result, builder->GetInsertBlock(), result_id});
        // Sentinel entry in invoke_alloca_map: some callers look up the
        // result's alloca to clear it (DotEval base release, StoreLocal
        // tracking).  Use nullptr to signal "SSA-direct, no alloca".
        invoke_alloca_map[result_id] = nullptr;
        return;
    }

    llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
    llvm::IRBuilder<> alloca_builder(entry, entry->begin());
    llvm::AllocaInst* cleanup_alloca = alloca_builder.CreateAlloca(i64_type,
            nullptr, "cleanup");
    alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
            cleanup_alloca);

    // Decref previous value before overwriting.  This is required even when
    // exception checks are deferred: ordinary AOT functions can contain loops,
    // so the same cleanup alloca may be reused many times before function exit.
    auto decref_fn = current_module->getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup_alloca);
    builder->CreateStore(result, cleanup_alloca);
    builder->CreateCall(decref_fn, {old_val, xsink_arg});

    registerInvokeCleanupAlloca(cleanup_alloca);
    invoke_alloca_map[result_id] = cleanup_alloca;
}

// Walk a lvalue AST expression to find the root LocalVar* key (for alloca lookup).
// Returns nullptr if the root is not a local variable.
static LocalVar* findLvalueRootLocalVar(const QoreValue& lvalue) {
    if (!lvalue.hasNode()) {
        return nullptr;
    }
    const AbstractQoreNode* node = lvalue.getInternalNode();
    while (node) {
        if (auto* var_ref = dynamic_cast<const VarRefNode*>(node)) {
            qore_var_t type = var_ref->getType();
            if (type == VT_LOCAL || type == VT_LOCAL_TS || type == VT_CLOSURE) {
                return var_ref->ref.id;
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

static const void* findLvalueRootLocalKey(const QoreValue& lvalue) {
    return reinterpret_cast<const void*>(findLvalueRootLocalVar(lvalue));
}

const void* QoreIRToLLVM::findLVPathRootLocalKey(
        const QoreIRLValuePathInstruction* path_inst) const {
    if (!path_inst || path_inst->path.empty()) {
        return nullptr;
    }

    const LVPathStep& root = path_inst->path[0];
    if (root.kind != LVPathStepKind::LocalVar
            && root.kind != LVPathStepKind::ClosureVar) {
        return nullptr;
    }

    if (root.ref_ptr) {
        return root.ref_ptr;
    }

    if (const void* key = findLvalueRootLocalKey(path_inst->delete_lvalue_expr)) {
        return key;
    }

    auto find_ir_slot = [this](uint32_t slot_id) -> const void* {
        if (!current_ir_func || slot_id == UINT32_MAX) {
            return nullptr;
        }
        for (const auto& [local, slot] : current_ir_func->local_var_slots) {
            if (slot == slot_id) {
                return reinterpret_cast<const void*>(local);
            }
        }
        return nullptr;
    };

    if (const void* key = find_ir_slot(path_inst->lvalue_slot_id)) {
        return key;
    }
    if (const void* key = find_ir_slot(root.slot_id)) {
        return key;
    }

    // Deserialized AOT-only paths may carry AOT local slots instead of IR-local
    // slots. Keep this as a fallback for handler/closure contexts.
    if (!aot_slots || root.slot_id == UINT32_MAX) {
        return nullptr;
    }
    for (const auto& [key, slot] : aot_slots->local_slots) {
        if (slot == static_cast<int32_t>(root.slot_id)) {
            return key;
        }
    }
    return nullptr;
}

bool QoreIRToLLVM::canReloadLocalFromRuntime(const void* key, bool honor_reload_exempt) const {
    if (local_allocas.find(key) == local_allocas.end()) {
        return false;
    }

    // Fast-entry parameters live only in LLVM allocas owned by the direct-call
    // ABI.  Reloading them from the runtime local stack can alias an outer
    // wrapper frame and gives the callee a mismatched cleanup responsibility.
    if (fast_entry_args && fast_entry_args->count(key)) {
        return false;
    }

    // Skip IR-only locals — they are never modified by AST callbacks,
    // so their LLVM alloca cache is always current.
    if (honor_reload_exempt && reload_exempt_locals_set && reload_exempt_locals_set->count(key)) {
        return false;
    }

    // Native locals are only enabled for IR-only locals. Keep this defensive
    // guard so a future classifier change cannot store boxed values into a
    // native alloca through the runtime reload path.
    if (native_int_locals.count(key) || native_float_locals.count(key)) {
        return false;
    }

    const LocalVar* var_ptr = reinterpret_cast<const LocalVar*>(key);
    if (var_ptr && var_ptr->isSelf()) {
        return false;
    }
    if (var_ptr && var_ptr->closureUse()) {
        return false;
    }
    if (weak_assigned_locals.count(key)) {
        return false;
    }

    bool is_entry_local = entry_locals_set.count(key) > 0;
    bool is_pre_instantiated = pre_instantiated_locals && pre_instantiated_locals->count(key);
    return is_entry_local || is_pre_instantiated;
}

llvm::AllocaInst* QoreIRToLLVM::getOrCreateLocalReloadEpoch(llvm::Function* llvm_func) {
    if (local_reload_epoch) {
        return local_reload_epoch;
    }
    llvm::IRBuilder<> alloca_builder(&llvm_func->getEntryBlock(),
            llvm_func->getEntryBlock().begin());
    local_reload_epoch = alloca_builder.CreateAlloca(i64_type, nullptr,
            "local_reload_epoch");
    alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, 0),
            local_reload_epoch);
    return local_reload_epoch;
}

llvm::AllocaInst* QoreIRToLLVM::getOrCreateLocalValidEpoch(const void* key,
        llvm::Function* llvm_func) {
    auto it = local_valid_epochs.find(key);
    if (it != local_valid_epochs.end()) {
        return it->second;
    }
    llvm::IRBuilder<> alloca_builder(&llvm_func->getEntryBlock(),
            llvm_func->getEntryBlock().begin());
    llvm::AllocaInst* valid_epoch = alloca_builder.CreateAlloca(i64_type,
            nullptr, "local_valid_epoch");
    alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, 0),
            valid_epoch);
    local_valid_epochs[key] = valid_epoch;
    return valid_epoch;
}

void QoreIRToLLVM::markLocalCacheFresh(const void* key, llvm::Function* llvm_func) {
    if (!local_reload_epoch || !canReloadLocalFromRuntime(key)) {
        return;
    }
    llvm::Value* epoch = builder->CreateLoad(i64_type, local_reload_epoch);
    llvm::AllocaInst* valid_epoch = getOrCreateLocalValidEpoch(key, llvm_func);
    builder->CreateStore(epoch, valid_epoch);
}

void QoreIRToLLVM::ensureLocalCacheFresh(const void* key, llvm::Module& module,
        llvm::Function* llvm_func) {
    if (!local_reload_epoch || !canReloadLocalFromRuntime(key)) {
        return;
    }

    auto alloca_it = local_allocas.find(key);
    if (alloca_it == local_allocas.end()) {
        return;
    }

    auto tracker_it = local_reload_trackers.find(key);
    if (tracker_it == local_reload_trackers.end()) {
        llvm::IRBuilder<> alloca_builder(&llvm_func->getEntryBlock(),
                llvm_func->getEntryBlock().begin());
        llvm::AllocaInst* tracker = alloca_builder.CreateAlloca(i64_type,
                nullptr, "reload_tracker");
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                tracker);
        local_reload_trackers[key] = tracker;
        registerPersistentCleanupAlloca(tracker);
        tracker_it = local_reload_trackers.find(key);
    }

    auto deferred_it = local_reload_deferred.find(key);
    if (deferred_it == local_reload_deferred.end()) {
        llvm::IRBuilder<> alloca_builder(&llvm_func->getEntryBlock(),
                llvm_func->getEntryBlock().begin());
        llvm::AllocaInst* deferred = alloca_builder.CreateAlloca(i64_type,
                nullptr, "reload_deferred");
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                deferred);
        local_reload_deferred[key] = deferred;
        registerPersistentCleanupAlloca(deferred);
        deferred_it = local_reload_deferred.find(key);
    }

    llvm::AllocaInst* valid_epoch = getOrCreateLocalValidEpoch(key, llvm_func);
    llvm::Value* epoch = builder->CreateLoad(i64_type, local_reload_epoch);
    if (aot_mode) {
        auto helper = module.getOrInsertFunction("qore_rt_reload_local_if_stale_aot",
                llvm::FunctionType::get(void_type,
                    {ptr_type, i32_type, ptr_type, ptr_type, ptr_type, ptr_type,
                     i64_type, ptr_type}, false));
        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
        builder->CreateCall(helper,
                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                 alloca_it->second, tracker_it->second, deferred_it->second,
                 valid_epoch, epoch, xsink_arg});
    } else {
        auto helper = module.getOrInsertFunction("qore_rt_reload_local_if_stale",
                llvm::FunctionType::get(void_type,
                    {ptr_type, ptr_type, ptr_type, ptr_type, ptr_type, i64_type,
                     ptr_type}, false));
        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                reinterpret_cast<uint64_t>(key));
        llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
        builder->CreateCall(helper,
                {var_as_ptr, alloca_it->second, tracker_it->second,
                 deferred_it->second, valid_epoch, epoch, xsink_arg});
    }
}

void QoreIRToLLVM::reloadLocalFromRuntime(const void* key, llvm::Module& module,
        llvm::Function* llvm_func, bool honor_reload_exempt) {
    auto alloca_it = local_allocas.find(key);
    if (!canReloadLocalFromRuntime(key, honor_reload_exempt)) {
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
        registerPersistentCleanupAlloca(tracker);
        tracker_it = local_reload_trackers.find(key);
    }

    // Get or create the deferred decref alloca for this local.
    // When a reload replaces the tracker value, the old tracker value is moved
    // here instead of being decrefd immediately.  This prevents use-after-free:
    // LoadLocal reads from the alloca (same value as the tracker); if we decrefd
    // the old tracker immediately, any live SSA value from that LoadLocal would
    // become a dangling pointer.  By deferring the decref by one reload cycle,
    // the old value survives until the next reload, by which time the SSA value
    // has been consumed by whatever operation used it.
    auto deferred_it = local_reload_deferred.find(key);
    if (deferred_it == local_reload_deferred.end()) {
        llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
        llvm::IRBuilder<> alloca_builder(entry, entry->begin());
        llvm::AllocaInst* deferred = alloca_builder.CreateAlloca(i64_type,
                nullptr, "reload_deferred");
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), deferred);
        local_reload_deferred[key] = deferred;
        // Register for cleanup at function exit
        registerPersistentCleanupAlloca(deferred);
        deferred_it = local_reload_deferred.find(key);
    }

    // Read the old tracker value (which might be aliased by a live SSA from LoadLocal)
    llvm::Value* old_tracker = builder->CreateLoad(i64_type, tracker_it->second);

    // Read the deferred value (from two reload cycles ago — safe to free)
    llvm::Value* old_deferred = builder->CreateLoad(i64_type, deferred_it->second);

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

    // Update alloca cache, tracker, and deferred:
    // - alloca and tracker get the new value
    // - deferred gets the old tracker value (survives until next reload)
    // - decref the old deferred value (from two cycles ago, no live SSA refs)
    builder->CreateStore(reloaded, alloca_it->second);
    builder->CreateStore(reloaded, tracker_it->second);
    builder->CreateStore(old_tracker, deferred_it->second);
    builder->CreateCall(decref_fn, {old_deferred, xsink_arg});
    markLocalCacheFresh(key, llvm_func);
}

void QoreIRToLLVM::retainLocalCacheValue(const void* key, llvm::Value* value,
        llvm::Module& module, llvm::Function* llvm_func, bool honor_reload_exempt) {
    auto alloca_it = local_allocas.find(key);
    if (alloca_it == local_allocas.end() || !value
            || !canReloadLocalFromRuntime(key, honor_reload_exempt)) {
        return;
    }

    auto tracker_it = local_reload_trackers.find(key);
    if (tracker_it == local_reload_trackers.end()) {
        llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
        llvm::IRBuilder<> alloca_builder(entry, entry->begin());
        llvm::AllocaInst* tracker = alloca_builder.CreateAlloca(i64_type,
                nullptr, "reload_tracker");
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), tracker);
        local_reload_trackers[key] = tracker;
        registerPersistentCleanupAlloca(tracker);
        tracker_it = local_reload_trackers.find(key);
    }

    auto deferred_it = local_reload_deferred.find(key);
    if (deferred_it == local_reload_deferred.end()) {
        llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
        llvm::IRBuilder<> alloca_builder(entry, entry->begin());
        llvm::AllocaInst* deferred = alloca_builder.CreateAlloca(i64_type,
                nullptr, "reload_deferred");
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), deferred);
        local_reload_deferred[key] = deferred;
        registerPersistentCleanupAlloca(deferred);
        deferred_it = local_reload_deferred.find(key);
    }

    auto incref_fn = module.getOrInsertFunction("qore_rt_incref",
            llvm::FunctionType::get(void_type, {i64_type}, false));
    auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

    llvm::Value* old_tracker = builder->CreateLoad(i64_type, tracker_it->second);
    llvm::Value* old_deferred = builder->CreateLoad(i64_type, deferred_it->second);

    builder->CreateCall(incref_fn, {value});
    builder->CreateStore(value, alloca_it->second);
    builder->CreateStore(value, tracker_it->second);
    builder->CreateStore(old_tracker, deferred_it->second);
    builder->CreateCall(decref_fn, {old_deferred, xsink_arg});
    markLocalCacheFresh(key, llvm_func);
}

void QoreIRToLLVM::clearLocalReloadTracker(const void* key, llvm::Module& module,
        llvm::Function* llvm_func) {
    const LocalVar* var_ptr = reinterpret_cast<const LocalVar*>(key);
    if (var_ptr && (var_ptr->isSelf() || var_ptr->closureUse())) {
        return;
    }

    auto tracker_it = local_reload_trackers.find(key);
    if (tracker_it == local_reload_trackers.end()) {
        // Tracker doesn't exist yet at compile time — proactively create it so
        // the generated decref code runs on EVERY loop iteration at runtime.
        // Without this, the tracker gets created by reloadLocalFromRuntime()
        // (called AFTER this function) with +1 ref that's never cleared,
        // causing refcount inflation → copy-on-write → O(n²) for container ops.
        //
        // Apply the same eligibility checks as reloadLocalFromRuntime().
        if (!canReloadLocalFromRuntime(key)) {
            return;
        }

        // Create entry-block alloca initialized to VAL_NOTHING
        llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
        llvm::IRBuilder<> alloca_builder(entry, entry->begin());
        llvm::AllocaInst* tracker = alloca_builder.CreateAlloca(i64_type,
                nullptr, "reload_tracker");
        alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), tracker);
        local_reload_trackers[key] = tracker;
        registerPersistentCleanupAlloca(tracker);
        tracker_it = local_reload_trackers.find(key);
    }

    auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    llvm::Value* old_val = builder->CreateLoad(i64_type, tracker_it->second);
    builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), tracker_it->second);
    builder->CreateCall(decref_fn, {old_val, xsink_arg});

    // Also clear the deferred alloca if it exists
    auto deferred_it = local_reload_deferred.find(key);
    if (deferred_it != local_reload_deferred.end()) {
        llvm::Value* old_deferred = builder->CreateLoad(i64_type, deferred_it->second);
        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), deferred_it->second);
        builder->CreateCall(decref_fn, {old_deferred, xsink_arg});
    }
}

void QoreIRToLLVM::clearLocalCachedValue(const void* key, llvm::Module& module,
        llvm::Function* llvm_func, LocalCacheClearMode mode) {
    const LocalVar* var_ptr = reinterpret_cast<const LocalVar*>(key);
    if (var_ptr && var_ptr->isSelf()) {
        return;
    }

    auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

    if (mode == LocalCacheClearMode::IncludeFastEntryOwner) {
        auto fast_it = fast_entry_param_allocas_by_local.find(key);
        if (fast_it != fast_entry_param_allocas_by_local.end()) {
            llvm::Value* old_val = builder->CreateLoad(i64_type, fast_it->second);
            builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                    fast_it->second);
            builder->CreateCall(decref_fn, {old_val, xsink_arg});
        }
    }

    auto cleanup_it = preinstantiated_entry_cleanup_by_local.find(key);
    if (cleanup_it == preinstantiated_entry_cleanup_by_local.end()) {
        return;
    }

    llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup_it->second);
    builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
            cleanup_it->second);
    builder->CreateCall(decref_fn, {old_val, xsink_arg});

    auto alloca_it = local_allocas.find(key);
    if (alloca_it == local_allocas.end()) {
        return;
    }

    if (native_int_locals.count(key)) {
        builder->CreateStore(llvm::ConstantInt::get(i64_type, 0),
                alloca_it->second);
    } else if (native_float_locals.count(key)) {
        builder->CreateStore(llvm::ConstantFP::get(double_type, 0.0),
                alloca_it->second);
    } else {
        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                alloca_it->second);
    }
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
        registerInvokeCleanupAlloca(ca);
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
        clearLocalCachedValue(local_key, module, llvm_func,
            LocalCacheClearMode::DuplicateRefsOnly);
        clearLocalReloadTracker(local_key, module, llvm_func);
    }

    return ca;
}

void QoreIRToLLVM::releaseCleanupForValueId(uint32_t value_id,
        llvm::Module& module) {
    auto alloca_it = invoke_alloca_map.find(value_id);
    if (alloca_it == invoke_alloca_map.end()) {
        return;
    }

    auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    if (alloca_it->second) {
        llvm::Value* old_val = builder->CreateLoad(i64_type,
                alloca_it->second);
        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                alloca_it->second);
        builder->CreateCall(decref_fn, {old_val, xsink_arg});
        invoke_alloca_map.erase(alloca_it);
        return;
    }

    // SSA-direct cleanup sentinel: decref the SSA value directly and erase the
    // pending entry so normal-exit cleanup does not decref it again.
    auto val_it = values.find(value_id);
    if (val_it == values.end() || !val_it->second) {
        return;
    }
    llvm::Value* val = val_it->second;
    builder->CreateCall(decref_fn, {val, xsink_arg});
    for (auto pit = pending_ssa_cleanup.begin();
            pit != pending_ssa_cleanup.end(); ) {
        if (pit->value == val) {
            pit = pending_ssa_cleanup.erase(pit);
        } else {
            ++pit;
        }
    }
    invoke_alloca_map.erase(alloca_it);
}

void QoreIRToLLVM::consumeValueUse(uint32_t value_id, llvm::Module& module,
        bool release_weak_load) {
    if (!value_id) {
        return;
    }

    auto uses_it = operand_remaining_uses.find(value_id);
    if (uses_it != operand_remaining_uses.end()) {
        --uses_it->second;
    }

    if (!release_weak_load || !weak_load_result_ids.count(value_id)) {
        return;
    }

    if (uses_it != operand_remaining_uses.end() && uses_it->second <= 0) {
        releaseCleanupForValueId(value_id, module);
        weak_load_result_ids.erase(value_id);
    }
}

void QoreIRToLLVM::releaseDotEvalBaseIfCurrentUseIsLast(
        const QoreIRInstruction* inst, llvm::Module& module) {
    if (!inst || inst->operands.empty()) {
        return;
    }

    uint32_t base_id = inst->operands[0].id;
    auto uses_it = operand_remaining_uses.find(base_id);
    if (uses_it == operand_remaining_uses.end() || uses_it->second > 1) {
        return;
    }

    releaseCleanupForValueId(base_id, module);
    weak_load_result_ids.erase(base_id);
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

llvm::Value* QoreIRToLLVM::buildArgCleanupArray(const QoreIRInstruction* inst,
        int arg_start, llvm::Function* llvm_func, int nargs, bool& has_cleanup) {
    has_cleanup = false;
    if (nargs <= 0) {
        return llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(ptr_type));
    }

    llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
            llvm_func->getEntryBlock().begin());
    llvm::Value* cleanup_array = ab.CreateAlloca(ptr_type,
            llvm::ConstantInt::get(i32_type, nargs));
    llvm::Value* null_ptr = llvm::ConstantPointerNull::get(
            llvm::cast<llvm::PointerType>(ptr_type));

    for (int i = 0; i < nargs; ++i) {
        llvm::Value* cleanup_ptr = null_ptr;
        uint32_t value_id = inst->operands[arg_start + i].id;
        auto alloca_it = invoke_alloca_map.find(value_id);
        if (alloca_it != invoke_alloca_map.end()) {
            cleanup_ptr = alloca_it->second;
            if (!cleanup_ptr) {
                cleanup_ptr = promoteSsaEntryToAlloca(value_id, *current_module,
                        llvm_func);
            }
            if (cleanup_ptr) {
                has_cleanup = true;
            } else {
                cleanup_ptr = null_ptr;
            }
        }
        llvm::Value* gep = builder->CreateGEP(ptr_type, cleanup_array,
                llvm::ConstantInt::get(i32_type, i));
        builder->CreateStore(cleanup_ptr, gep);
    }

    return cleanup_array;
}

void QoreIRToLLVM::reloadAllLocalsFromRuntime(llvm::Module& module, llvm::Function* llvm_func,
        bool honor_reload_exempt, bool eager) {
    // Phase 4: Skip entirely if all locals are invisible to AST callbacks,
    // so the LLVM alloca cache is always current after runtime helper calls.
    if (honor_reload_exempt && all_locals_reload_exempt) {
        return;
    }
    bool has_reloadable_local = false;
    for (auto& [key, alloca] : local_allocas) {
        (void)alloca;
        if (canReloadLocalFromRuntime(key, honor_reload_exempt)) {
            if (eager) {
                reloadLocalFromRuntime(key, module, llvm_func, honor_reload_exempt);
                continue;
            }
            has_reloadable_local = true;
            break;
        }
    }
    if (eager) {
        return;
    }
    if (!has_reloadable_local) {
        return;
    }
    llvm::AllocaInst* epoch_alloca = getOrCreateLocalReloadEpoch(llvm_func);
    llvm::Value* epoch = builder->CreateLoad(i64_type, epoch_alloca);
    llvm::Value* next_epoch = builder->CreateAdd(epoch,
            llvm::ConstantInt::get(i64_type, 1), "local_reload_epoch_next");
    builder->CreateStore(next_epoch, epoch_alloca);
    (void)module;
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
        registerInvokeCleanupAlloca(cleanup);
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

void QoreIRToLLVM::emitRuntimeLocationUpdate(const QoreIRInstruction* inst, llvm::Module& module) {
    if (!loc_cache_ptr || !stmt_cache_ptr) {
        return;
    }
    if (!inst->loc || inst->loc->start_line <= 0) {
        return;
    }
    if (inst->loc->start_line == last_runtime_line) {
        return;
    }
    last_runtime_line = inst->loc->start_line;

    if (aot_mode) {
        // AOT mode: call runtime helper to update location from ctx->locs table
        auto it = aot_loc_slots.find(inst->loc);
        int32_t loc_index;
        if (it != aot_loc_slots.end()) {
            loc_index = it->second;
        } else {
            loc_index = static_cast<int32_t>(aot_loc_table.size());
            aot_loc_slots[inst->loc] = loc_index;
            // Copy location data by value immediately — the table owns the data,
            // eliminating any dependency on inst->loc pointer lifetime.
            AOTLocEntry entry;
            entry.start_line = inst->loc->start_line;
            entry.end_line = inst->loc->end_line;
            const char* f = inst->loc->getFile();
            if (f) {
                entry.file = f;
            }
            aot_loc_table.push_back(std::move(entry));
        }
        auto helper = module.getOrInsertFunction("qore_rt_set_runtime_loc_aot",
            llvm::FunctionType::get(llvm::Type::getVoidTy(ctx),
                {ptr_type, llvm::Type::getInt32Ty(ctx)}, false));
        builder->CreateCall(helper, {aot_ctx_arg,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), loc_index)});
    } else {
        // JIT mode: inline store of statement + location pointers
        builder->CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(ctx)),
            stmt_cache_ptr);
        llvm::Value* loc_val = builder->CreateIntToPtr(
            llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(inst->loc)),
            llvm::PointerType::getUnqual(ctx));
        builder->CreateStore(loc_val, loc_cache_ptr);
    }
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

llvm::BasicBlock* QoreIRToLLVM::getOrCreateFunctionUnwindLP(llvm::Module& module,
        llvm::Function* llvm_func) {
    if (function_unwind_lp) {
        return function_unwind_lp;
    }
    // Create an empty block for the landing pad — its terminator and the
    // landingpad instruction itself are emitted during finalization, after
    // all invoke sites have been lowered and invoke_result_allocas is
    // complete. Keeping the LP empty during body lowering avoids ordering
    // issues between invoke emission and the LP's landingpad instruction.
    function_unwind_lp = llvm::BasicBlock::Create(ctx, "func_unwind_lp", llvm_func);
    return function_unwind_lp;
}

llvm::Value* QoreIRToLLVM::emitMaybeInvoke(llvm::FunctionCallee normal_helper,
        llvm::FunctionCallee throwing_helper,
        llvm::ArrayRef<llvm::Value*> args,
        llvm::Module& module, llvm::Function* llvm_func,
        const QoreIRInstruction* inst) {
    auto mark_fallback = [&](llvm::Value* call, llvm::FunctionCallee callee) {
        const auto* fn = llvm::dyn_cast<llvm::Function>(callee.getCallee());
        if (!fn) {
            return;
        }
        llvm::StringRef name = fn->getName();
        if (name == "qore_rt_invoke_expr"
                || name == "qore_rt_invoke_expr_throwing"
                || name == "qore_rt_invoke_expr_aot"
                || name == "qore_rt_invoke_expr_aot_throwing") {
            annotateAotExecutableExprFallback(call, inst);
        }
    };

    if (aot_eh_enabled && inst && !inst->exception_target) {
        // Mark the throwing wrapper noinline so the LLVM inliner doesn't try to
        // expand its body into every caller — that turns 1 invoke + 1 cont block
        // per call site into N invokes + N cont blocks (one per inlined helper),
        // and SimplifyCFG then runs many quadratic passes over the inflated CFG.
        // Measured: 60s+ at -O1 vs 0.6s with noinline applied. The inliner respects
        // module-level attributes set on the function symbol, but
        // getOrInsertFunction creates a fresh declaration each call — so we set
        // the attribute every time. It's idempotent on the underlying Function.
        if (auto* fn = llvm::dyn_cast<llvm::Function>(throwing_helper.getCallee())) {
            fn->addFnAttr(llvm::Attribute::NoInline);
            // Cold hint: the throw path is rare; tells the optimizer not to
            // spend cycles trying to vectorize / inline through this edge.
            fn->addFnAttr(llvm::Attribute::Cold);
        }

        // Phase 2B — per-invoke cleanup LP: capture the snapshot of
        // pending_ssa_cleanup that dominates THIS invoke's block.  Falls
        // back to the legacy shared function-level LP when SSA-direct is
        // disabled for the function (no pending entries and no common
        // cleanup block => legacy path still works for future uses).
        llvm::BasicBlock* invoke_bb = builder->GetInsertBlock();
        llvm::BasicBlock* lp = createPerInvokeCleanupLP(module, llvm_func, invoke_bb);
        llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "call_cont", llvm_func);
        llvm::Value* result = builder->CreateInvoke(throwing_helper, cont, lp, args);
        mark_fallback(result, throwing_helper);
        builder->SetInsertPoint(cont);
        // Record that the just-produced result was born on the EH path.
        // trackResultForCleanup reads + consumes this to decide whether
        // to track the result SSA-direct or via the legacy cleanup alloca.
        last_call_was_invoke_eh = true;
        skip_next_exception_check = true;
        return result;
    }
    llvm::Value* result = builder->CreateCall(normal_helper, args);
    mark_fallback(result, normal_helper);
    return result;
}

bool QoreIRToLLVM::tryEmitDecomposedBackground(const QoreValue& expr_val,
        const std::vector<QoreIRValue>& operands,
        llvm::Module& module, llvm::Function* llvm_func,
        const QoreIRInstruction* inst,
        bool throwing_ok,
        llvm::Value** result_out) {
    if (operands.empty()) {
        return false;
    }
    const auto* bg_op = dynamic_cast<const QoreBackgroundOperatorNode*>(
        expr_val.getInternalNode());
    if (!bg_op) {
        return false;
    }
    const AbstractQoreNode* inner = bg_op->getExp().getInternalNode();
    if (!inner) {
        return false;
    }

    std::string error_dummy;

    // Helper: given an operand range, build an alloca'd i64 array of boxed values.
    // Returns a null pointer when empty range (zero args).
    bool build_args_failed = false;
    auto build_args_array = [&](size_t first, size_t end) -> llvm::Value* {
        int count = (int)(end - first);
        if (count <= 0) {
            return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_type));
        }
        llvm::Value* arr = builder->CreateAlloca(i64_type,
            llvm::ConstantInt::get(i32_type, count));
        for (int i = 0; i < count; ++i) {
            auto* val = getVal(operands[first + i].id, error_dummy);
            if (!val) {
                build_args_failed = true;
                return nullptr;
            }
            llvm::Value* boxed = boxValue(val, operands[first + i].id);
            builder->CreateStore(boxed, builder->CreateGEP(i64_type, arr,
                llvm::ConstantInt::get(i32_type, i)));
        }
        return arr;
    };

    // Signature shared by the four new per-shape helpers:
    //   uint64_t helper(const Node*, uint64_t* args, int nargs, ExceptionSink*)
    auto call_with_node_helper = [&](const char* normal_name, const char* throwing_name,
            const void* node_ptr, size_t args_first, size_t args_end,
            llvm::Value** out) -> bool {
        int nargs = (int)(args_end - args_first);
        llvm::Value* args_array = build_args_array(args_first, args_end);
        if (build_args_failed) {
            return false;
        }
        llvm::Value* node_as_ptr = builder->CreateIntToPtr(
            llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(node_ptr)),
            ptr_type);
        auto ft = llvm::FunctionType::get(i64_type,
            {ptr_type, ptr_type, i32_type, ptr_type}, false);
        auto helper = module.getOrInsertFunction(normal_name, ft);
        if (throwing_ok) {
            auto helper_throwing = module.getOrInsertFunction(throwing_name, ft);
            *out = emitMaybeInvoke(helper, helper_throwing,
                {node_as_ptr, args_array, llvm::ConstantInt::get(i32_type, nargs),
                 xsink_arg}, module, llvm_func, inst);
        } else {
            *out = builder->CreateCall(helper,
                {node_as_ptr, args_array, llvm::ConstantInt::get(i32_type, nargs),
                 xsink_arg});
        }
        return true;
    };

    // Signature for dot-eval / call-ref helpers (receiver / callref boxed as arg):
    //   uint64_t helper(const Node*, uint64_t recv_or_callref, uint64_t* args,
    //                   int nargs, ExceptionSink*)
    auto call_with_node_and_recv = [&](const char* normal_name, const char* throwing_name,
            const void* node_ptr, size_t recv_idx, size_t args_first, size_t args_end,
            llvm::Value** out) -> bool {
        auto* recv_val = getVal(operands[recv_idx].id, error_dummy);
        if (!recv_val) {
            return false;
        }
        llvm::Value* recv_boxed = boxValue(recv_val, operands[recv_idx].id);
        int nargs = (int)(args_end - args_first);
        llvm::Value* args_array = build_args_array(args_first, args_end);
        if (build_args_failed) {
            return false;
        }
        llvm::Value* node_as_ptr = builder->CreateIntToPtr(
            llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(node_ptr)),
            ptr_type);
        auto ft = llvm::FunctionType::get(i64_type,
            {ptr_type, i64_type, ptr_type, i32_type, ptr_type}, false);
        auto helper = module.getOrInsertFunction(normal_name, ft);
        if (throwing_ok) {
            auto helper_throwing = module.getOrInsertFunction(throwing_name, ft);
            *out = emitMaybeInvoke(helper, helper_throwing,
                {node_as_ptr, recv_boxed, args_array,
                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                module, llvm_func, inst);
        } else {
            *out = builder->CreateCall(helper,
                {node_as_ptr, recv_boxed, args_array,
                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
        }
        return true;
    };

    // AOT slot-based call.  Captures the enclosing `expr_val` QoreValue (the
    // full QoreBackgroundOperatorNode), looks up its slot, and emits a helper
    // call with (ctx, slot, args, nargs, xsink).  Returns the boxed call
    // result via *out.  Signature:
    //   uint64_t helper(QoreAOTContext*, int32_t slot, uint64_t* args,
    //                   int nargs, ExceptionSink*)
    auto emit_aot_slot_call = [&](const char* normal_name, const char* throwing_name,
            size_t args_first, size_t args_end, llvm::Value** out) -> bool {
        QoreValue expr_copy = expr_val;
        uint64_t expr_bits;
        std::memcpy(&expr_bits, &expr_copy, sizeof(expr_bits));
        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
        int nargs = (int)(args_end - args_first);
        llvm::Value* args_array = build_args_array(args_first, args_end);
        if (build_args_failed) { return false; }
        auto ft = llvm::FunctionType::get(i64_type,
            {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
        auto helper = module.getOrInsertFunction(normal_name, ft);
        if (throwing_ok) {
            auto helper_throwing = module.getOrInsertFunction(throwing_name, ft);
            *out = emitMaybeInvoke(helper, helper_throwing,
                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), args_array,
                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                module, llvm_func, inst);
        } else {
            *out = builder->CreateCall(helper,
                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), args_array,
                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
        }
        return true;
    };

    // AOT slot-based call with pre-evaluated receiver/callref.  Signature:
    //   uint64_t helper(QoreAOTContext*, int32_t slot, uint64_t recv_or_callref,
    //                   uint64_t* args, int nargs, ExceptionSink*)
    auto emit_aot_slot_call_with_recv = [&](const char* normal_name,
            const char* throwing_name, size_t recv_idx,
            size_t args_first, size_t args_end, llvm::Value** out) -> bool {
        auto* recv_val = getVal(operands[recv_idx].id, error_dummy);
        if (!recv_val) { return false; }
        llvm::Value* recv_boxed = boxValue(recv_val, operands[recv_idx].id);
        QoreValue expr_copy = expr_val;
        uint64_t expr_bits;
        std::memcpy(&expr_bits, &expr_copy, sizeof(expr_bits));
        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
        int nargs = (int)(args_end - args_first);
        llvm::Value* args_array = build_args_array(args_first, args_end);
        if (build_args_failed) { return false; }
        auto ft = llvm::FunctionType::get(i64_type,
            {ptr_type, i32_type, i64_type, ptr_type, i32_type, ptr_type}, false);
        auto helper = module.getOrInsertFunction(normal_name, ft);
        if (throwing_ok) {
            auto helper_throwing = module.getOrInsertFunction(throwing_name, ft);
            *out = emitMaybeInvoke(helper, helper_throwing,
                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), recv_boxed,
                 args_array, llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                module, llvm_func, inst);
        } else {
            *out = builder->CreateCall(helper,
                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), recv_boxed,
                 args_array, llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
        }
        return true;
    };

    // background self.method(args) — supported in JIT and AOT (existing path)
    if (auto* sfcn = dynamic_cast<const SelfFunctionCallNode*>(inner)) {
        if (!sfcn->getMethod()) {
            return false;
        }
        const QoreParseListNode* pargs = sfcn->getParseArgs();
        const QoreListNode* sargs = sfcn->getArgs();
        size_t actual_nargs = pargs ? pargs->size() : (sargs ? sargs->size() : 0);
        if (aot_mode) {
            // AOT: use name-based helper (method resolved on spawned thread)
            const char* method_name = sfcn->getName();
            llvm::Value* name_ptr = builder->CreateGlobalStringPtr(method_name);
            llvm::Value* args_array = build_args_array(0, actual_nargs);
            if (build_args_failed) { return false; }
            auto ft = llvm::FunctionType::get(i64_type,
                {ptr_type, ptr_type, i32_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_background_self_call_aot", ft);
            if (throwing_ok) {
                auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_background_self_call_aot_throwing", ft);
                *result_out = emitMaybeInvoke(helper, helper_throwing,
                    {name_ptr, args_array,
                     llvm::ConstantInt::get(i32_type, (int)actual_nargs), xsink_arg},
                    module, llvm_func, inst);
            } else {
                *result_out = builder->CreateCall(helper,
                    {name_ptr, args_array,
                     llvm::ConstantInt::get(i32_type, (int)actual_nargs), xsink_arg});
            }
            return true;
        }
        return call_with_node_helper("qore_rt_background_self_call",
            "qore_rt_background_self_call_throwing",
            sfcn, 0, actual_nargs, result_out);
    }

    // background foo(args) — free function call
    if (auto* fcn = dynamic_cast<const FunctionCallNode*>(inner)) {
        if (!fcn->getFunction()) {
            return false;
        }
        const QoreParseListNode* pargs = fcn->getParseArgs();
        const QoreListNode* fargs = fcn->getArgs();
        size_t actual_nargs = pargs ? pargs->size() : (fargs ? fargs->size() : 0);
        if (aot_mode) {
            return emit_aot_slot_call("qore_rt_background_function_call_aot",
                "qore_rt_background_function_call_aot_throwing",
                0, actual_nargs, result_out);
        }
        return call_with_node_helper("qore_rt_background_function_call",
            "qore_rt_background_function_call_throwing",
            fcn, 0, actual_nargs, result_out);
    }
    // background Class::staticMethod(args)
    if (auto* smcn = dynamic_cast<const StaticMethodCallNode*>(inner)) {
        if (!smcn->getMethod()) {
            return false;
        }
        const QoreParseListNode* pargs = smcn->getParseArgs();
        const QoreListNode* sargs = smcn->getArgs();
        size_t actual_nargs = pargs ? pargs->size() : (sargs ? sargs->size() : 0);
        if (aot_mode) {
            return emit_aot_slot_call("qore_rt_background_static_method_call_aot",
                "qore_rt_background_static_method_call_aot_throwing",
                0, actual_nargs, result_out);
        }
        return call_with_node_helper("qore_rt_background_static_method_call",
            "qore_rt_background_static_method_call_throwing",
            smcn, 0, actual_nargs, result_out);
    }
    // background obj.method(args) — receiver in operands[0], args after
    if (auto* devn = dynamic_cast<const QoreDotEvalOperatorNode*>(inner)) {
        MethodCallNode* m = devn->getMethodCall();
        if (!m) {
            return false;
        }
        if (aot_mode) {
            return emit_aot_slot_call_with_recv("qore_rt_background_dot_eval_call_aot",
                "qore_rt_background_dot_eval_call_aot_throwing",
                /*recv_idx*/0, /*args_first*/1, /*args_end*/operands.size(),
                result_out);
        }
        return call_with_node_and_recv("qore_rt_background_dot_eval_call",
            "qore_rt_background_dot_eval_call_throwing",
            devn, /*recv_idx*/0, /*args_first*/1, /*args_end*/operands.size(),
            result_out);
    }
    // background callref(args) — call-ref in operands[0], args after
    if (auto* crcn = dynamic_cast<const CallReferenceCallNode*>(inner)) {
        if (aot_mode) {
            return emit_aot_slot_call_with_recv("qore_rt_background_call_ref_call_aot",
                "qore_rt_background_call_ref_call_aot_throwing",
                /*recv_idx*/0, /*args_first*/1, /*args_end*/operands.size(),
                result_out);
        }
        return call_with_node_and_recv("qore_rt_background_call_ref_call",
            "qore_rt_background_call_ref_call_throwing",
            crcn, /*recv_idx*/0, /*args_first*/1, /*args_end*/operands.size(),
            result_out);
    }

    return false;
}

bool QoreIRToLLVM::tryEmitBackgroundMetadata(const QoreIRBackgroundInstruction* bg_inst,
        llvm::Module& module, llvm::Function* llvm_func,
        bool throwing_ok,
        llvm::Value** result_out) {
    if (!bg_inst || bg_inst->operands.empty()) {
        return false;
    }

    std::string error_dummy;
    bool build_args_failed = false;
    auto build_args_array = [&](size_t first, size_t end) -> llvm::Value* {
        int count = (int)(end - first);
        if (count <= 0) {
            return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_type));
        }
        llvm::Value* arr = builder->CreateAlloca(i64_type,
            llvm::ConstantInt::get(i32_type, count));
        for (int i = 0; i < count; ++i) {
            const QoreIRValue& op = bg_inst->operands[first + i];
            auto* val = getVal(op.id, error_dummy);
            if (!val) {
                build_args_failed = true;
                return nullptr;
            }
            llvm::Value* boxed = boxValue(val, op.id);
            builder->CreateStore(boxed, builder->CreateGEP(i64_type, arr,
                llvm::ConstantInt::get(i32_type, i)));
        }
        return arr;
    };

    if (bg_inst->kind == QoreIRBackgroundKind::DotEval) {
        auto* recv_val = getVal(bg_inst->operands[0].id, error_dummy);
        if (!recv_val) {
            return false;
        }
        llvm::Value* recv_boxed = boxValue(recv_val, bg_inst->operands[0].id);
        llvm::Value* args_array = build_args_array(1, bg_inst->operands.size());
        if (build_args_failed) {
            return false;
        }
        llvm::Value* name_ptr = builder->CreateGlobalStringPtr(bg_inst->name);
        int nargs = (int)bg_inst->operands.size() - 1;
        auto ft = llvm::FunctionType::get(i64_type,
            {ptr_type, i64_type, ptr_type, i32_type, ptr_type}, false);
        auto helper = module.getOrInsertFunction(
            "qore_rt_background_dot_eval_name_call_aot", ft);
        if (throwing_ok) {
            auto helper_throwing = module.getOrInsertFunction(
                "qore_rt_background_dot_eval_name_call_aot_throwing", ft);
            *result_out = emitMaybeInvoke(helper, helper_throwing,
                {name_ptr, recv_boxed, args_array,
                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                module, llvm_func, bg_inst);
        } else {
            *result_out = builder->CreateCall(helper,
                {name_ptr, recv_boxed, args_array,
                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
        }
        return true;
    }

    return false;
}

void QoreIRToLLVM::finalizeFunctionUnwindLP(llvm::Module& module) {
    if (!function_unwind_lp) {
        return;
    }
    // Emit the shared function-level unwind landing pad:
    //
    //   %lp = landingpad { ptr, i32 } cleanup
    //   ; cleanup sequence mirrors error_return_block
    //   resume { ptr, i32 } %lp
    //
    // `cleanup`-only semantics means the personality runs our cleanup and
    // then resumes unwinding — the exception propagates to the caller's
    // invoke unwind edge (or terminates if no caller frame catches). This
    // is critical: a catching LP would swallow the exception silently while
    // xsink is still populated, leaving the caller to see a spurious
    // normal return with xsink set. The resume keeps exception flow and
    // xsink state in sync.
    //
    // Only the outermost AOT↔AST boundary (UserVariantBase::evalTiered
    // wrapper) needs to catch-and-convert; per-function LPs resume.
    auto* saved_insert = builder->GetInsertBlock();
    auto saved_ip = builder->GetInsertPoint();
    builder->SetInsertPoint(function_unwind_lp);

    llvm::Type* lp_type = llvm::StructType::get(ctx, {ptr_type, i32_type});
    llvm::LandingPadInst* lp = builder->CreateLandingPad(lp_type, 0);
    lp->setCleanup(true);

    // Same cleanup sequence as error_return_block to keep the unwind path
    // and the normal-check error path identical in behavior.
    emitOnBlockExitExec(module);
    emitIteratorCleanup(module);
    emitPreinstantiatedCleanup(module);
    emitInvokeCleanup(module);
    emitPreInstClosureReInstantiation(module);
    emitLocalUninstantiation(module);
    // Resume the in-flight exception — propagates to the caller frame.
    builder->CreateResume(lp);

    if (saved_insert) {
        builder->SetInsertPoint(saved_insert, saved_ip);
    }
}

void QoreIRToLLVM::emitExceptionCheck(llvm::Module& module, llvm::Function* llvm_func,
        const QoreIRInstruction* inst) {
    // For deferred exception checking (init functions): skip per-instruction checks
    // for non-try-block instructions.  Set flag to emit consolidated check at end.
    if (deferred_exception_checking && !inst->exception_target) {
        deferred_check_needed = true;
        return;
    }

    // C++ EH prototype: the invoke site just emitted a CreateInvoke whose
    // normal destination we're currently in. xsink is guaranteed clean on
    // this edge (the throwing wrapper unwinds when it's set), so the check
    // would be dead code. One-shot flag — subsequent calls in the same
    // cont block still get their own checks.
    if (skip_next_exception_check) {
        skip_next_exception_check = false;
        return;
    }

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
    // Inline exception check as direct memory loads instead of function call
    // ExceptionSink::priv at offset 0 -> qore_es_private*
    // qore_es_private::head at offset 0 within priv -> QoreException*
    // qore_es_private::thread_exit at offset 20 within priv -> bool

    // Load priv pointer from xsink at offset 0
    auto* xsink_priv_ptr = builder->CreateLoad(ptr_type, xsink_arg, "xsink_priv_ptr");

    // Load head pointer from priv at offset 0
    auto* head_ptr = builder->CreateLoad(ptr_type, xsink_priv_ptr, "priv_head");

    // Check if head != nullptr
    auto* has_exception_head = builder->CreateICmpNE(head_ptr,
            llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_type)),
            "has_exception_head");

    // Also check thread_exit at offset 20 within priv
    // thread_exit is a bool at offset 20 in qore_es_private
    auto* thread_exit_ptr = builder->CreateGEP(llvm::Type::getInt8Ty(ctx),
            xsink_priv_ptr, {llvm::ConstantInt::get(i64_type, 20)}, "thread_exit_ptr");
    auto* thread_exit_byte = builder->CreateLoad(llvm::Type::getInt8Ty(ctx), thread_exit_ptr, "thread_exit_byte");
    auto* has_exception_thread_exit = builder->CreateICmpNE(thread_exit_byte,
            llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx), 0), "has_thread_exit");

    // Combine: exception exists if (head != nullptr) OR (thread_exit)
    auto* has_exception = builder->CreateOr(has_exception_head, has_exception_thread_exit, "has_exception");
    llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "no_exception", llvm_func);
    // Phase 2B — route the exception edge through a per-site preamble
    // BB that decrefs the current pending_ssa_cleanup snapshot before
    // branching to `exception_block`.  Preamble is dominated by the
    // current block, so chain-pushed entries are all visible; normal
    // edge keeps pending_ssa_cleanup intact for downstream uses /
    // per-invoke LPs.  This is what actually removes the cleanup-alloca
    // forest that pushes HttpServer's LLVM IR into quadratic codegen.
    emitCondBrWithSsaPreamble(module, llvm_func, has_exception,
            exception_block, cont);
    builder->SetInsertPoint(cont);
}

static const char* qoreIROpcodeDiagnosticName(QoreIROpcode op) {
    return getOpcodeName(static_cast<int>(op));
}

static bool canEmitAotInvokeExprFallback(const QoreIRInstruction* inst) {
    if (inst->opcode == QoreIROpcode::Invoke) {
        return true;
    }

    QoreIROpcode op = inst->opcode;
    switch (op) {
        case QoreIROpcode::ConstEnum:
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::LoadStaticVar:
        case QoreIROpcode::LoadConstant:
        case QoreIROpcode::CreateClosure:
        case QoreIROpcode::CreateCallRef:
        case QoreIROpcode::CreateMethodRef:
        case QoreIROpcode::CreateParseRef:
        case QoreIROpcode::NewHashDecl:
        case QoreIROpcode::NewComplexHash:
        case QoreIROpcode::NewComplexList:
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
        case QoreIROpcode::ListAssignAny:
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::HashMap:
        case QoreIROpcode::HashMapSelect:
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMapSelectAny:
        case QoreIROpcode::InvokeSimError:
            return true;
        default:
            return false;
    }
}

static std::string formatQoreIRFallbackInstruction(const QoreIRInstruction* inst) {
    std::string desc = "IR opcode ";
    if (const char* name = qoreIROpcodeDiagnosticName(inst->opcode)) {
        desc += name;
    } else {
        desc += "#";
        desc += std::to_string(static_cast<int>(inst->opcode));
    }

    const QoreValue* expr = nullptr;
    if (auto* invoke = dynamic_cast<const QoreIRInvokeInstruction*>(inst)) {
        desc += " (invoke opcode ";
        if (const char* name = qoreIROpcodeDiagnosticName(invoke->invoke_opcode)) {
            desc += name;
        } else {
            desc += "#";
            desc += std::to_string(static_cast<int>(invoke->invoke_opcode));
        }
        desc += ")";
        expr = &invoke->expr;
    } else if (auto* expr_inst = dynamic_cast<const QoreIRExprInstruction*>(inst)) {
        expr = &expr_inst->expr;
    } else if (auto* static_var = dynamic_cast<const QoreIRStaticVarInstruction*>(inst)) {
        expr = &static_var->expr;
    } else if (auto* load_const = dynamic_cast<const QoreIRLoadConstantInstruction*>(inst)) {
        expr = &load_const->expr;
    } else if (auto* create_closure = dynamic_cast<const QoreIRCreateClosureInstruction*>(inst)) {
        expr = &create_closure->expr;
    } else if (auto* create_call_ref = dynamic_cast<const QoreIRCreateCallRefInstruction*>(inst)) {
        expr = &create_call_ref->expr;
    } else if (auto* create_method_ref = dynamic_cast<const QoreIRCreateMethodRefInstruction*>(inst)) {
        expr = &create_method_ref->expr;
    } else if (auto* create_parse_ref = dynamic_cast<const QoreIRCreateParseRefInstruction*>(inst)) {
        expr = &create_parse_ref->expr;
    } else if (auto* new_hd = dynamic_cast<const QoreIRNewHashDeclInstruction*>(inst)) {
        expr = &new_hd->expr;
    } else if (auto* new_ch = dynamic_cast<const QoreIRNewComplexHashInstruction*>(inst)) {
        expr = &new_ch->expr;
    } else if (auto* new_cl = dynamic_cast<const QoreIRNewComplexListInstruction*>(inst)) {
        expr = &new_cl->expr;
    }

    if (expr) {
        desc += ", expression qtype=";
        desc += std::to_string(expr->getType());
        desc += " (";
        desc += expr->getTypeName();
        desc += ")";
        if (expr->hasNode()) {
            const AbstractQoreNode* node = expr->getInternalNode();
            if (node) {
                desc += ", node=";
                desc += node->getTypeName();
                desc += " (";
                desc += std::to_string(node->getType());
                desc += ")";
                desc += ", needs-eval=";
                desc += node->needs_eval() ? "true" : "false";
                if (const auto* pn = dynamic_cast<const ParseNode*>(node)) {
                    if (pn->loc) {
                        desc += ", expr-location=";
                        const char* file = pn->loc->getFileValue();
                        desc += (file && *file) ? file : "<unknown>";
                        if (pn->loc->start_line >= 0) {
                            desc += ":";
                            desc += std::to_string(pn->loc->start_line);
                            if (pn->loc->end_line >= 0 && pn->loc->end_line != pn->loc->start_line) {
                                desc += "-";
                                desc += std::to_string(pn->loc->end_line);
                            }
                        }
                        if (pn->loc->getSource() && *pn->loc->getSource()) {
                            desc += ", source=";
                            desc += pn->loc->getSource();
                        }
                        if (pn->loc->offset) {
                            desc += ", offset=";
                            desc += std::to_string(pn->loc->offset);
                        }
                    }
                }
            } else {
                desc += ", node=<null>";
            }
        }
    }

    if (inst->loc && inst->loc->getFile()) {
        desc += " at ";
        desc += inst->loc->getFile();
        desc += ":";
        desc += std::to_string(inst->loc->start_line);
    }

    return desc;
}

static void appendQoreIRInstructionDiagnostic(std::string& error,
        const QoreIRInstruction* inst) {
    if (!inst) {
        return;
    }
    if (!error.empty()) {
        error += "; ";
    }
    error += formatQoreIRFallbackInstruction(inst);
}

static bool setAotExpressionFallbackError(std::string& error,
        const QoreIRInstruction* inst, const char* reason) {
    error = "unsupported AOT expression lowering";
    if (reason && *reason) {
        error += ": ";
        error += reason;
    }
    appendQoreIRInstructionDiagnostic(error, inst);
    error += "; add native IR lowering instead";
    return false;
}

void QoreIRToLLVM::annotateAotExecutableExprFallback(llvm::Value* call,
        const QoreIRInstruction* inst) const {
    if (!call || !inst) {
        return;
    }
    auto* llvm_inst = llvm::dyn_cast<llvm::Instruction>(call);
    if (!llvm_inst) {
        return;
    }
    llvm_inst->setMetadata("qore.aot.fallback_ir",
        llvm::MDNode::get(ctx, llvm::MDString::get(ctx, formatQoreIRFallbackInstruction(inst))));
}

static const QoreIRInstruction* findFallbackIRInstruction(const QoreIRFunction* func,
        const llvm::DILocation* dbg_loc) {
    if (!func || !dbg_loc || !dbg_loc->getLine()) {
        return nullptr;
    }

    unsigned line = dbg_loc->getLine();
    const QoreIRInstruction* first_line_match = nullptr;
    uint64_t checked = 0;
    for (const auto& block : func->blocks) {
        for (const auto& inst : block->instructions) {
            if (++checked % 100 == 0 && qore_check_cancel(nullptr, "AOT fallback diagnostic scan")) {
                return first_line_match;
            }
            const QoreProgramLocation* loc = inst->loc;
            if (!loc || loc->start_line <= 0) {
                continue;
            }
            int end_line = loc->end_line > 0 ? loc->end_line : loc->start_line;
            if (line >= static_cast<unsigned>(loc->start_line)
                    && line <= static_cast<unsigned>(end_line)) {
                if (!first_line_match) {
                    first_line_match = inst.get();
                }
                if (canEmitAotInvokeExprFallback(inst.get())) {
                    return inst.get();
                }
            }
        }
    }

    return first_line_match;
}

bool QoreIRToLLVM::checkNoAotExecutableExprFallback(llvm::Function* llvm_func, std::string& error) const {
    if (!aot_mode || !llvm_func) {
        return true;
    }

    uint64_t checked = 0;
    for (const auto& bb : *llvm_func) {
        for (const auto& inst : bb) {
            if (++checked % 100 == 0 && qore_check_cancel(nullptr, "AOT executable fallback scan")) {
                error = "AOT executable expression fallback scan cancelled in function '";
                error += llvm_func->getName().str();
                error += "'";
                return false;
            }
            const auto* call = llvm::dyn_cast<llvm::CallBase>(&inst);
            if (!call) {
                continue;
            }
            const llvm::Function* callee = call->getCalledFunction();
            if (!callee) {
                continue;
            }
            llvm::StringRef name = callee->getName();
            if (name != "qore_rt_invoke_expr"
                    && name != "qore_rt_invoke_expr_throwing"
                    && name != "qore_rt_invoke_expr_aot"
                    && name != "qore_rt_invoke_expr_aot_throwing") {
                continue;
            }

            error = "AOT executable expression fallback is disabled in function '";
            error += llvm_func->getName().str();
            error += "': generated call to ";
            error += name.str();

            bool have_ir_inst = false;
            if (llvm::MDNode* md = call->getMetadata("qore.aot.fallback_ir")) {
                if (md->getNumOperands() > 0) {
                    if (auto* mds = llvm::dyn_cast<llvm::MDString>(md->getOperand(0).get())) {
                        error += "; ";
                        error += mds->getString().str();
                        have_ir_inst = true;
                    }
                }
            }

            llvm::DebugLoc dbg_loc = call->getDebugLoc();
            if (dbg_loc) {
                const llvm::DILocation* loc = dbg_loc.get();
                std::string file = loc->getFilename().str();
                std::string dir = loc->getDirectory().str();
                if (!file.empty()) {
                    if (!dir.empty() && file[0] != '/') {
                        file = dir + "/" + file;
                    }
                    error += " at ";
                    error += file;
                    if (loc->getLine()) {
                        error += ":";
                        error += std::to_string(loc->getLine());
                    }
                }
                const QoreIRInstruction* ir_inst = have_ir_inst ? nullptr
                    : findFallbackIRInstruction(current_ir_func, loc);
                if (ir_inst) {
                    error += "; ";
                    error += formatQoreIRFallbackInstruction(ir_inst);
                }
            }
            error += "; add native IR lowering instead";
            return false;
        }
    }

    return true;
}

bool QoreIRToLLVM::lowerFunction(const QoreIRFunction& func, llvm::Module& module, std::string& error) {
    current_ir_func = &func;
    current_module = &module;
    initTypes();
    declareRuntimeHelpers(module);

    // Clear COW tracking for new function
    cow_modified_locals.clear();

    // Reset deferred exception checking flag for new function
    deferred_check_needed = false;

    // C++ EH prototype (QORE_AOT_EH=1): reset per-function landing pad state.
    // Enable only in aot_mode since JIT mode's non-AOT paths don't have
    // qore_rt_call_direct_aot_throwing wired up yet.
    //
    // Per-function gating: count call-like IR instructions and downgrade to
    // the check-based path for functions above QORE_AOT_EH_MAX_CALLS (default
    // 100). LLVM SimplifyCFG runs ~1s per pass over 500-cont-block CFGs, and
    // the optimizer runs SimplifyCFG many times, so large functions blow
    // compile time from seconds to minutes. Module-level functions (qmod) are
    // usually small enough to stay under the threshold; only standalone test
    // executables with monolithic test methods hit it.
    function_unwind_lp = nullptr;
    skip_next_exception_check = false;

    // Phase 2B — reset per-function SSA-direct cleanup tracking and the
    // incremental idom map.  entry_block_for_idom is set below, once the
    // LLVM function has been created and its entry block exists.
    pending_ssa_cleanup.clear();
    temp_cleanup_marks.clear();
    immediate_dominator.clear();
    entry_block_for_idom = nullptr;
    last_call_was_invoke_eh = false;
    function_common_cleanup = nullptr;
    common_cleanup_phi_preds.clear();
    common_cleanup_phi_values.clear();
    {
        // Phase E (2026-04-18 p75) originally enabled invoke-based EH by
        // default in aot_mode, claiming "25% compile time improvement"
        // and "1-3% runtime cost" on HttpServer.  The invoke-based path
        // emits one LLVM `invoke` (BB-terminating) per potentially-
        // throwing runtime helper call; all invokes share a single
        // function-level landingpad, but LLVM still splits the CFG
        // into one continuation block per invoke site.
        //
        // Scale-up to the qlib AOT build (2026-04-19) surfaced the
        // downside: module-init closures with many registration calls
        // (e.g. ZohoInventoryDataProvider init has 64
        // `registerFactory`/`registerAction` statements that lower to
        // ~1478 invokes in a single LLVM function) hit LLVM
        // SimplifyCFG's super-linear per-BB behavior and push compile
        // time past 60 minutes.  The already-wired
        // QORE_AOT_EH_MAX_CALLS per-function gate would downgrade
        // those to the check-based path, but (a) its default was 0
        // (disabled) and (b) even at 100 the IR-opcode count for the
        // Zoho closure doesn't exceed it — one IR opcode can expand
        // to many LLVM invokes via the runtime helpers.  A threshold
        // on IR opcodes is the wrong layer to gate at.
        //
        // Re-measured runtime cost: `parse_to_qore_value` (a tagged
        // hot-path function) shows ~0% delta between invoke-based
        // full-O3 and check-based full-O3 (79.5 µs vs 80.8 µs per call
        // across 300K-call runs — within noise, check-based actually
        // slightly faster on median).  Per-call cost is dominated by
        // Qore value-semantics overhead (refcount, type dispatch,
        // marshaling) — LLVM's function-level optimizations don't
        // move the needle when every call is into opaque runtime
        // helpers.  The original "1-3% runtime cost" claim may have
        // held for a specific micro-benchmark but doesn't generalize.
        //
        // Flip the default: check-based EH is now the default in
        // aot_mode.  Invoke-based remains available via explicit
        // `QORE_AOT_EH=1` for experimentation.  QORE_AOT_NO_EH kept
        // for compat (same behavior as the new default).  The
        // per-function QORE_AOT_EH_MAX_CALLS gate is still honored
        // when invoke-based is explicitly enabled.
        static const bool env_no_eh = getenv("QORE_AOT_NO_EH") != nullptr;
        static const bool env_explicit_eh = getenv("QORE_AOT_EH") != nullptr;
        static const int env_eh_max_calls = []() {
            const char* s = getenv("QORE_AOT_EH_MAX_CALLS");
            return s ? std::atoi(s) : 100;
        }();
        aot_eh_enabled = !env_no_eh && env_explicit_eh && aot_mode;
        if (aot_eh_enabled && env_eh_max_calls > 0) {
            int call_like = 0;
            for (const auto& block : func.blocks) {
                for (const auto& inst : block->instructions) {
                    auto op = inst->opcode;
                    // Count opcodes that lower to potentially-throwing helpers.
                    if (op == QoreIROpcode::Invoke
                            || op == QoreIROpcode::CallDirect
                            || op == QoreIROpcode::CallStaticDirect
                            || op == QoreIROpcode::CallMethodDirect
                            || op == QoreIROpcode::DotEvalMethodDirect
                            || op == QoreIROpcode::InvokeMethodDirect
                            || op == QoreIROpcode::InvokeDotEvalMethodDirect) {
                        ++call_like;
                        if (call_like > env_eh_max_calls) {
                            break;
                        }
                    }
                }
                if (call_like > env_eh_max_calls) {
                    break;
                }
            }
            if (call_like > env_eh_max_calls) {
                aot_eh_enabled = false;
            }
        }
    }

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

    // Set C++ exception handling personality function for invoke/landingpad EH.
    // This tells LLVM to use the GNU C++ personality for stack unwinding when
    // qore_rt_check_throw() throws a QoreJITException.
    {
        auto personality = module.getOrInsertFunction("__gxx_personality_v0",
            llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), true));
        llvm_func->setPersonalityFn(llvm::cast<llvm::Constant>(personality.getCallee()));
    }

    // Phase 5 experiment: mark huge functions OptimizeNone+NoInline to cut
    // LLVM codegen cost. SelectionDAG scales superlinearly with function
    // size; handleRequest (~500 IR BBs) is 87% of HttpServer.qm compile time.
    // Attribute forces LLVM to skip function-level opt passes and (in some
    // configurations) triggers FastISel instead of SelectionDAG, trading
    // runtime perf for compile speed.
    //
    // Gated by QORE_AOT_BIG_FN_THRESHOLD (default disabled; set to a BB
    // count like 300 to enable). Set to 0 to disable entirely. This is an
    // experiment — if it helps, Phase 5 outlining becomes less urgent.
    // Preliminary IR-block-count check — tags functions like
    // HttpServer::handleRequest whose IR already has hundreds of blocks.
    // Doesn't catch cases where the IR is structurally small but lowers
    // to a huge LLVM function (e.g. module-init closures whose IR is a
    // single block but unfurls into >1000 LLVM BBs via exception-aware
    // invokes + landingpad/continuation BBs per registration call).  A
    // post-lowering pass at the end of lowerFunction re-checks against
    // llvm_func->size() and can retro-tag those.
    {
        static const size_t big_fn_threshold = []() {
            const char* s = getenv("QORE_AOT_BIG_FN_THRESHOLD");
            return s ? static_cast<size_t>(std::atoi(s)) : size_t(0);
        }();
        if (big_fn_threshold > 0 && func.blocks.size() >= big_fn_threshold) {
            llvm_func->addFnAttr(llvm::Attribute::OptimizeNone);
            llvm_func->addFnAttr(llvm::Attribute::NoInline);
            if (getenv("QORE_AOT_DEBUG")) {
                fprintf(stderr, "AOT: OptimizeNone for '%s' (IR %zu blocks >= threshold %zu)\n",
                        fn_name.c_str(), func.blocks.size(), big_fn_threshold);
            }
        }
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
    if (aot_mode && is_fast_entry) {
        // AOT fast entry: (i64 p1, ..., ptr ctx, ptr xsink)
        unsigned num_args = llvm_func->arg_size();
        aot_ctx_arg = llvm_func->getArg(num_args - 2);
        aot_ctx_arg->setName("ctx");
        xsink_arg = llvm_func->getArg(num_args - 1);
        xsink_arg->setName("xsink");
    } else if (aot_mode) {
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
    reload_exempt_locals_set = ir_only_locals_set;

    // In AOT mode, remove pre-instantiated body locals from the IR-only set.
    // evalTiered pre-instantiates ALL body locals from all_body_locals on the
    // runtime stack, so StoreLocal must sync via qore_rt_assign_local_aot (which
    // adds a reference).  Without sync, the alloca holds the only reference while
    // the cleanup alloca also tracks the source value for decref → double-free on
    // function exit.  Parameters and other non-body locals remain IR-only.
    aot_adjusted_ir_only.clear();
    if (aot_mode && ir_only_locals_set && !func.all_body_locals.empty()) {
        aot_adjusted_ir_only = *ir_only_locals_set;
        for (LocalVar* lv : func.all_body_locals) {
            const void* key = reinterpret_cast<const void*>(lv);
            aot_adjusted_ir_only.erase(key);
        }
        ir_only_locals_set = aot_adjusted_ir_only.empty()
            ? nullptr : &aot_adjusted_ir_only;
    }

    // Phase 4: Check if ALL locals are reload-exempt, enabling bulk skip of
    // reloadAllLocalsFromRuntime() after calls.
    all_locals_reload_exempt = reload_exempt_locals_set
        && func.total_local_count > 0
        && reload_exempt_locals_set->size() == func.total_local_count;

    // Phase A: Identify body locals that can skip entry loads.
    aot_body_locals.clear();
    if (aot_mode && !func.all_body_locals.empty()) {
        for (LocalVar* lv : func.all_body_locals) {
            aot_body_locals.insert(reinterpret_cast<const void*>(lv));
        }
    }

    // Identify IR-only body locals — these can skip thread-local stack
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

    // Phase 1 (compile-time opt roadmap): skip DWARF entirely when
    // emit_debug_info is disabled.  Saves per-function LLVM work for
    // DISubprogram/DILocation creation + backend DWARF table emission.
    // di_sp stays null, which makes setDebugLocation a no-op.
    if (emit_debug_info) {
    if (shared_di_builder) {
        // Multi-function module: use shared DIBuilder and compile unit
        active_di_builder = shared_di_builder;
        di_cu = shared_di_cu;
    } else {
        // Single-function module: create owned DIBuilder and compile unit
        di_builder = std::make_unique<llvm::DIBuilder>(module);
        active_di_builder = di_builder.get();
    }
    } else {
        active_di_builder = nullptr;
        di_cu = nullptr;
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

    if (emit_debug_info) {
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
    preinstantiated_entry_cleanup_allocas.clear();
    preinstantiated_entry_cleanup_by_local.clear();
    fast_entry_param_allocas.clear();
    fast_entry_param_allocas_by_local.clear();
    owned_ir_local_allocas.clear();
    owned_ir_local_alloca_keys.clear();
    invoke_result_allocas.clear();
    persistent_cleanup_allocas.clear();
    temp_cleanup_marks.clear();
    invoke_cleanup_array = nullptr;
    invoke_cleanup_array_capacity = estimateInvokeCleanupArrayCapacity(func);
    invoke_cleanup_array_count = 0;
    invoke_cleanup_array_overflow = false;
    invoke_alloca_map.clear();
    iterator_cleanup_allocas.clear();
    pending_phis.clear();
    local_reload_trackers.clear();
    local_reload_deferred.clear();
    local_reload_epoch = nullptr;
    local_valid_epochs.clear();
    error_return_block = nullptr;
    jit_deopt_block = nullptr;
    landingpad_blocks.clear();
    has_on_block_exit_handlers = false;

    if (invoke_cleanup_array_capacity && !func.blocks.empty()) {
        llvm::BasicBlock& entry = llvm_func->getEntryBlock();
        llvm::IRBuilder<> ab(&entry, entry.begin());
        invoke_cleanup_array = ab.CreateAlloca(ptr_type,
                llvm::ConstantInt::get(i32_type, invoke_cleanup_array_capacity),
                "cleanup_slots");
    }

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

    // AOT fast entry: emit check_stack at function start and redirect first block
    // to a body continuation block (the runtime helper doesn't do this for fast entry)
    if (aot_mode && is_fast_entry && !func.blocks.empty()) {
        auto check_fn = module.getOrInsertFunction("qore_rt_check_stack",
                llvm::FunctionType::get(i32_type, {ptr_type}, false));
        llvm::Value* err = builder->CreateCall(check_fn, {xsink_arg});
        llvm::Value* ok = builder->CreateICmpEQ(err, llvm::ConstantInt::get(i32_type, 0));

        llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(ctx, "body", llvm_func);
        llvm::BasicBlock* overflow_bb = llvm::BasicBlock::Create(ctx, "stack_overflow",
                llvm_func);
        builder->CreateCondBr(ok, body_bb, overflow_bb);

        // Stack overflow: return NOTHING (nanboxed)
        builder->SetInsertPoint(overflow_bb);
        builder->CreateRet(llvm::ConstantInt::get(i64_type, 0));

        // Remap first IR block to body continuation so block-lowering loop uses it
        block_map[func.blocks.front().get()] = body_bb;
        builder->SetInsertPoint(body_bb);
    }

    // Initialize runtime location tracking: cache TLS pointers for per-line updates.
    // JIT mode: inst->loc pointers embedded directly (valid in current process).
    // AOT mode: location indices into ctx->locs[] table (populated at load time).
    loc_cache_ptr = nullptr;
    stmt_cache_ptr = nullptr;
    last_runtime_line = -1;
    aot_loc_slots.clear();
    aot_loc_table.clear();
    {
        auto loc_fn = module.getOrInsertFunction("qore_rt_get_loc_ptr",
            llvm::FunctionType::get(ptr_type, {}, false));
        loc_cache_ptr = builder->CreateCall(loc_fn, {}, "loc_ptr");
        auto stmt_fn = module.getOrInsertFunction("qore_rt_get_stmt_ptr",
            llvm::FunctionType::get(ptr_type, {}, false));
        stmt_cache_ptr = builder->CreateCall(stmt_fn, {}, "stmt_ptr");
        // AOT mode: no preloading needed — qore_rt_set_runtime_loc_aot handles
        // null checks and TLS access internally per line change.
    }

    // Compute remaining use counts for each register and identify registers
    // that are only used as DotEval bases (safe for _for_call variant).
    operand_remaining_uses.clear();
    dot_eval_only_bases.clear();
    weak_assigned_locals.clear();
    weak_load_result_ids.clear();
    // First: collect all register IDs used as DotEval bases
    std::unordered_set<uint32_t> dot_eval_base_candidates;
    std::unordered_set<uint32_t> non_dot_eval_uses;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            if (!inst_ptr) {
                continue;
            }
            for (const auto& op : inst_ptr->operands) {
                operand_remaining_uses[op.id]++;
            }
            if (inst_ptr->opcode == QoreIROpcode::StoreLocal) {
                const auto* local_inst =
                    static_cast<const QoreIRLocalInstruction*>(inst_ptr.get());
                if (local_inst->weak && local_inst->local) {
                    weak_assigned_locals.insert(
                        reinterpret_cast<const void*>(local_inst->local));
                }
            }
            // Count non-operand value uses (stored in dedicated members, not operands)
            switch (inst_ptr->opcode) {
                case QoreIROpcode::Return: {
                    const auto* ret = static_cast<const QoreIRReturnInstruction*>(inst_ptr.get());
                    if (ret->has_value) {
                        operand_remaining_uses[ret->value.id]++;
                        non_dot_eval_uses.insert(ret->value.id);
                    }
                    break;
                }
                case QoreIROpcode::BrIf: {
                    const auto* br = static_cast<const QoreIRBranchIfInstruction*>(inst_ptr.get());
                    operand_remaining_uses[br->condition.id]++;
                    non_dot_eval_uses.insert(br->condition.id);
                    break;
                }
                case QoreIROpcode::SwitchInt: {
                    const auto* sw = static_cast<const QoreIRSwitchIntInstruction*>(inst_ptr.get());
                    operand_remaining_uses[sw->switch_val.id]++;
                    non_dot_eval_uses.insert(sw->switch_val.id);
                    break;
                }
                case QoreIROpcode::SwitchString: {
                    const auto* sw = static_cast<const QoreIRSwitchStringInstruction*>(inst_ptr.get());
                    operand_remaining_uses[sw->switch_val.id]++;
                    non_dot_eval_uses.insert(sw->switch_val.id);
                    break;
                }
                case QoreIROpcode::Phi: {
                    const auto* phi = static_cast<const QoreIRPhiInstruction*>(inst_ptr.get());
                    for (const auto& inc : phi->incoming) {
                        operand_remaining_uses[inc.value.id]++;
                        non_dot_eval_uses.insert(inc.value.id);
                    }
                    break;
                }
                default:
                    break;
            }

            // Track which registers are used as DotEval bases vs other uses
            bool is_dot_eval = false;
            if (inst_ptr->opcode == QoreIROpcode::DotEvalMethodDirect
                    || inst_ptr->opcode == QoreIROpcode::InvokeDotEvalMethodDirect) {
                is_dot_eval = true;
            } else if (inst_ptr->opcode == QoreIROpcode::Invoke && !inst_ptr->operands.empty()) {
                const auto* inv = static_cast<const QoreIRInvokeInstruction*>(inst_ptr.get());
                if (isDotEvalInvokeOpcode(inv->invoke_opcode)) {
                    is_dot_eval = true;
                }
            }

            if (is_dot_eval && !inst_ptr->operands.empty()) {
                // operands[0] is the base
                dot_eval_base_candidates.insert(inst_ptr->operands[0].id);
                // operands[1..] are args — these are non-DotEval uses
                for (size_t i = 1; i < inst_ptr->operands.size(); ++i) {
                    non_dot_eval_uses.insert(inst_ptr->operands[i].id);
                }
            } else {
                // All operands of non-DotEval instructions are non-DotEval uses
                for (const auto& op : inst_ptr->operands) {
                    non_dot_eval_uses.insert(op.id);
                }
            }
        }
    }
    // A register is a "DotEval-only base" if it appears as a base but never
    // as a non-DotEval operand (including DotEval args)
    for (uint32_t id : dot_eval_base_candidates) {
        if (!non_dot_eval_uses.count(id)) {
            dot_eval_only_bases.insert(id);
        }
    }

    // Phase 2B — pin the LLVM function's entry block as the dominance root.
    // After fast-entry remap (body_bb) or direct lowering, the true entry
    // is always llvm_func->getEntryBlock(); record it with a nullptr idom
    // so the dominance walk terminates cleanly there.
    entry_block_for_idom = &llvm_func->getEntryBlock();
    immediate_dominator[entry_block_for_idom] = nullptr;

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

        // Phase 2B — record this block's immediate dominator based on its
        // currently-wired predecessors.  No behavioural change in Step 1;
        // consumed by Step 2's per-invoke cleanup LP predicate.
        updateImmediateDominator(llvm_block);

        for (const auto& inst_ptr : block->instructions) {
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            // Phase 2B — clear the EH-invoke one-shot flag at instruction
            // boundary so a dropped trackResultForCleanup from a prior
            // instruction cannot leak SSA-direct semantics into a later
            // non-EH call in the same block.  emitMaybeInvoke's EH path
            // re-sets it for the just-produced result.
            last_call_was_invoke_eh = false;

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
            // Update runtime_loc for exception stack traces (per source line change)
            // Skip for PHI nodes — LLVM requires PHI nodes at the top of basic blocks;
            // emitting a runtime call before the PHI would violate this constraint.
            if (inst->opcode != QoreIROpcode::Phi) {
                emitRuntimeLocationUpdate(inst, module);
            }
            if (getenv("QORE_LLVM_DEBUG")) {
                fprintf(stderr, "LLVM-INST: opcode=%d in block=%s\n",
                        static_cast<int>(inst->opcode), builder->GetInsertBlock()->getName().str().c_str());
                fflush(stderr);
            }
            // Flush any deferred exception check BEFORE entering a try scope.
            // In deferred_exception_checking mode we skip xsink checks for
            // instructions outside try/catch; if we then reach an instruction
            // whose exception handling routes to a catch block, any xsink
            // left dirty from a prior skipped instruction would be caught by
            // that catch handler (with locals unassigned). Detect "inside
            // try scope" via the instruction's exception_target (base class
            // field for check-based instructions, or the derived field for
            // Invoke-family opcodes), then flush to error_return_block if
            // the target's first instruction is a LandingPad (= catch block).
            if (deferred_exception_checking && deferred_check_needed
                    && inst->opcode != QoreIROpcode::Phi) {
                QoreIRBasicBlock* eh_target = inst->exception_target;
                if (!eh_target) {
                    switch (inst->opcode) {
                        case QoreIROpcode::Invoke:
                            eh_target = static_cast<const QoreIRInvokeInstruction*>(inst)
                                    ->exception_target;
                            break;
                        case QoreIROpcode::InvokeMethodDirect:
                            eh_target = static_cast<const QoreIRInvokeMethodDirectInstruction*>(inst)
                                    ->exception_target;
                            break;
                        case QoreIROpcode::InvokeDotEvalMethodDirect:
                            eh_target = static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(
                                    inst)->exception_target;
                            break;
                        default:
                            break;
                    }
                }
                bool target_is_catch = false;
                if (eh_target && !eh_target->instructions.empty()) {
                    target_is_catch = (eh_target->instructions.front()->opcode
                            == QoreIROpcode::LandingPad);
                }
                if (target_is_catch) {
                    auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
                            llvm::FunctionType::get(i64_type, {ptr_type}, false));
                    llvm::Value* ex_check = builder->CreateCall(has_ex, {xsink_arg});
                    llvm::Value* has_exception = builder->CreateICmpNE(ex_check,
                            llvm::ConstantInt::get(i64_type, 0));
                    if (!error_return_block) {
                        error_return_block = llvm::BasicBlock::Create(ctx, "error_return", llvm_func);
                    }
                    llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "pre_try_flush_cont",
                            llvm_func);
                    // Phase 2B — route exception edge through a preamble
                    // that decrefs pending SSA; keeps normal flow SSA.
                    emitCondBrWithSsaPreamble(module, llvm_func, has_exception,
                            error_return_block, cont);
                    builder->SetInsertPoint(cont);
                    deferred_check_needed = false;
                }
            }
            if (!lowerInstruction(inst, llvm_func, module, error)) {
                appendQoreIRInstructionDiagnostic(error, inst);
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

            // Release DotEval BASE cleanup allocas at last use: when the base
            // operand (operands[0]) of a DotEvalMethodDirect or InvokeDotEvalMethodDirect
            // reaches its last use, release its cleanup alloca immediately.
            // This is critical for weak member dereferences (LoadSelfMember on
            // WeakReferenceNode members) — without early release, the temporary strong
            // reference keeps the target object alive for the entire function lifetime,
            // preventing destructor calls and causing shutdown hangs.
            // We ONLY apply this for DotEval base values because:
            // (1) DotEval results are independent of the base (no borrowing)
            // (2) The method call is complete — base is fully consumed
            // (3) Other instructions (StoreLocal) may produce results that BORROW
            //     from their operand's cleanup alloca — releasing those is unsafe.
            // Decrement use counts for operands after the instruction has
            // consumed them.  Terminator-only operands such as BrIf/Switch
            // conditions are handled inside their lowerers so cleanup IR can be
            // emitted before the branch/switch terminator.
            for (const auto& op : inst->operands) {
                consumeValueUse(op.id, module, false);
            }

            // Release DotEval BASE cleanup allocas at last use.
            // Also covers Invoke instructions with DotEval invoke_opcodes
            // (e.g., parent.logDebug(...) compiled as Invoke + DotEvalAny/DotEvalHash).
            bool is_dot_eval_base_release = false;
            if (inst->opcode == QoreIROpcode::DotEvalMethodDirect
                    || inst->opcode == QoreIROpcode::InvokeDotEvalMethodDirect) {
                is_dot_eval_base_release = true;
            } else if (inst->opcode == QoreIROpcode::Invoke && !inst->operands.empty()) {
                const auto* inv = static_cast<const QoreIRInvokeInstruction*>(inst);
                if (isDotEvalInvokeOpcode(inv->invoke_opcode)) {
                    is_dot_eval_base_release = true;
                }
            }
            if (is_dot_eval_base_release
                    && !inst->operands.empty()
                    && !builder->GetInsertBlock()->getTerminator()) {
                uint32_t base_id = inst->operands[0].id;
                auto uses_it = operand_remaining_uses.find(base_id);
                if (uses_it != operand_remaining_uses.end() && uses_it->second <= 0) {
                    releaseCleanupForValueId(base_id, module);
                }
            }

            // Weak-assigned LoadLocal resolves a WeakReferenceNode to a
            // temporary strong ref.  Release that temp at last use instead of
            // waiting for function exit; otherwise long-running loops keep the
            // referent alive and weak refs never observe deletion.
            if (!weak_load_result_ids.empty()
                    && !builder->GetInsertBlock()->getTerminator()) {
                for (const auto& op : inst->operands) {
                    if (!weak_load_result_ids.count(op.id)) {
                        continue;
                    }
                    auto uses_it = operand_remaining_uses.find(op.id);
                    if (uses_it != operand_remaining_uses.end()
                            && uses_it->second <= 0) {
                        releaseCleanupForValueId(op.id, module);
                        weak_load_result_ids.erase(op.id);
                    }
                }
            }

            // Decomposed background helpers copy/ref the call expression and
            // argument values into the spawned thread before returning.  Once the
            // helper returns on the no-exception path, any last-use foreground
            // operand temps must be dropped immediately; otherwise a loaded
            // closure/call-ref can keep captured values alive until function exit.
            if (inst->opcode == QoreIROpcode::BackgroundInt
                    && !inst->operands.empty()
                    && !builder->GetInsertBlock()->getTerminator()) {
                for (const auto& op : inst->operands) {
                    auto uses_it = operand_remaining_uses.find(op.id);
                    if (uses_it != operand_remaining_uses.end() && uses_it->second <= 0) {
                        releaseCleanupForValueId(op.id, module);
                    }
                }
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

    // C++ EH prototype: finalize the shared function-level unwind landing pad
    // if any invoke emitted during body lowering needed it.
    finalizeFunctionUnwindLP(module);

    // Phase 2B — finalize the shared common-cleanup block if any
    // per-invoke LP fed into it.  Populates the phi + shared tail +
    // resume; a no-op when no EH invoke was emitted or SSA-direct
    // wasn't active for this function.
    finalizeFunctionCommonCleanup(module);

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

    emitLateExitCleanup(llvm_func, module);

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

    // Emit llvm.lifetime.start/end annotations for entry-block allocas.
    // Must run after all terminators are in place (we walk ret/resume sites).
    emitLifetimeAnnotations(llvm_func);

    if (!checkNoAotExecutableExprFallback(llvm_func, error)) {
        return false;
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
                if (getenv("QORE_DUMP_IR_ON_VERIFY_FAIL")) {
                    llvm_func->print(llvm::errs());
                }
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

    // Post-lowering big-fn check: some functions start with a modest IR
    // block count but explode in LLVM due to exception-aware invokes and
    // slot-call expansion.  The flagship case is Qore user module init
    // closures: a single IR block with N registration calls lowers to
    // ~20-30 LLVM BBs per call (each call site gets a landingpad BB and
    // a continuation BB, plus type-dispatch branches from slot
    // expansion).  ZohoInventoryDataProvider's module init is IR-size 1
    // but LLVM-size 1482 BBs / 38145 insts, which causes LLVM's
    // O3 pipeline to hang indefinitely (SelectionDAG+RegAlloc
    // super-linearity).  Re-check against the final LLVM BB count and
    // retro-tag if needed so the OptimizeNone attribute applies even
    // for IR-compact-but-LLVM-huge functions.
    if (!llvm_func->hasFnAttribute(llvm::Attribute::OptimizeNone)) {
        static const size_t big_fn_threshold = []() {
            const char* s = getenv("QORE_AOT_BIG_FN_THRESHOLD");
            return s ? static_cast<size_t>(std::atoi(s)) : size_t(0);
        }();
        if (big_fn_threshold > 0 && llvm_func->size() >= big_fn_threshold) {
            llvm_func->addFnAttr(llvm::Attribute::OptimizeNone);
            llvm_func->addFnAttr(llvm::Attribute::NoInline);
            if (getenv("QORE_AOT_DEBUG")) {
                fprintf(stderr,
                    "AOT: OptimizeNone for '%s' (LLVM %zu blocks >= threshold %zu, IR was %zu blocks)\n",
                    fn_name.c_str(),
                    (size_t)llvm_func->size(),
                    big_fn_threshold,
                    func.blocks.size());
            }
        }
    }

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
                "qore_rt_add_any", inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                "qore_rt_sub_any", inst, lhs_boxed, rhs_boxed, llvm_func, module);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        // Timeout arithmetic: `int + date` where int is ms (Qore timeout type).
        // Both operands are boxed; the runtime helper handles the int→date
        // conversion and the date addition/subtraction.
        case QoreIROpcode::AddTimeout:
        case QoreIROpcode::SubTimeout: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            const char* helper_name = (inst->opcode == QoreIROpcode::AddTimeout)
                ? "qore_rt_add_timeout" : "qore_rt_sub_timeout";
            auto helper = module.getOrInsertFunction(helper_name,
                llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed, xsink_arg});
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
                "qore_rt_mul_any", inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
            values[inst->result.id] = builder->CreateFCmpUNE(l_float, r_float);
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

        case QoreIROpcode::ToString: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_to_string",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {val_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            return true;
        }
        case QoreIROpcode::Sprintf: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            auto sp_ft = llvm::FunctionType::get(i64_type,
                    {i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_sprintf", sp_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_sprintf_throwing", sp_ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {val_boxed, xsink_arg}, module, llvm_func, inst);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            // Check for exception
            emitExceptionCheck(module, llvm_func, inst);
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
                        // Phase 2B — SSA-direct sentinel (nullptr) requires
                        // promotion so block-scope UninstantiateLocal can
                        // drop the ref at block exit (not at function exit).
                        llvm::Value* ca = alloca_it->second;
                        if (!ca) {
                            ca = promoteSsaEntryToAlloca(inst->result.id, module,
                                    llvm_func);
                        }
                        if (ca) {
                            local_cleanup_allocas[key].push_back(ca);
                        }
                    }
                }
                return true;
            }

            // Weak-assigned locals must be read through LocalVar::eval() on
            // every load.  The raw alloca/runtime slot contains a
            // WeakReferenceNode; evaluating it returns the current target with
            // a temporary strong ref, or NOTHING after the target is deleted.
            if (linst->local && weak_assigned_locals.count(key)) {
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
                weak_load_result_ids.insert(inst->result.id);
                return true;
            }

            // Outer-scope variables (not in pre_instantiated_locals and not block-scoped)
            // must always be read from the runtime stack, like closure-bound locals.
            // They're already on the thread-local variable stack from the calling scope
            // and must not be cached in allocas (which would be initialized to NOTHING
            // and become stale after any call that modifies the outer variable).
            if (linst->local && pre_instantiated_locals
                    && !pre_instantiated_locals->count(key)
                    && !block_scoped_locals.count(key)) {
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
                    if (ir_only_body_locals.count(key) || aot_body_locals.count(key)) {
                        // IR-only body local: not on the runtime stack (fast call path
                        // skips instantiation), so initialize alloca to default value.
                        // AOT body local: runtime stack slot exists but starts as NOTHING;
                        // avoid an entry qore_rt_load_local_aot() and reload after stores.
                        if (is_native_int) {
                            alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, 0), alloca);
                        } else if (is_native_float) {
                            alloca_builder.CreateStore(llvm::ConstantFP::get(double_type, 0.0), alloca);
                        } else {
                            alloca_builder.CreateStore(
                                    llvm::ConstantInt::get(i64_type, VAL_NOTHING), alloca);
                        }
                    } else if (aot_mode) {
                        llvm::AllocaInst* boxed_cleanup = nullptr;
                        if (!is_native_int && !is_native_float) {
                            boxed_cleanup = alloca_builder.CreateAlloca(i64_type,
                                    nullptr, "preinst_cleanup");
                            alloca_builder.CreateStore(
                                    llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                                    boxed_cleanup);
                            preinstantiated_entry_cleanup_allocas.push_back(
                                    boxed_cleanup);
                            preinstantiated_entry_cleanup_by_local[key] =
                                    boxed_cleanup;
                        }
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
                        if (boxed_cleanup) {
                            alloca_builder.CreateStore(init_val, boxed_cleanup);
                        } else {
                            preinstantiated_entry_loads.push_back(init_val);
                        }
                    } else {
                        llvm::AllocaInst* boxed_cleanup = nullptr;
                        if (!is_native_int && !is_native_float) {
                            boxed_cleanup = alloca_builder.CreateAlloca(i64_type,
                                    nullptr, "preinst_cleanup");
                            alloca_builder.CreateStore(
                                    llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                                    boxed_cleanup);
                            preinstantiated_entry_cleanup_allocas.push_back(
                                    boxed_cleanup);
                            preinstantiated_entry_cleanup_by_local[key] =
                                    boxed_cleanup;
                        }
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
                        if (boxed_cleanup) {
                            alloca_builder.CreateStore(init_val, boxed_cleanup);
                        } else {
                            preinstantiated_entry_loads.push_back(init_val);
                        }
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
            ensureLocalCacheFresh(key, module, llvm_func);
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
            // Coerce/strip helpers write an owned value through cleanup_ptr,
            // so loop re-execution must release the previous slot value first.
            auto clear_cleanup_before_reuse = [&](llvm::Value* cleanup) {
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup);
                builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
            };
            auto clear_cleanup_alloca = [&](llvm::Value* cleanup) {
                if (!cleanup) {
                    return;
                }
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup);
                builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
            };
            auto clear_consumed_operand_cleanup = [&](uint32_t value_id) {
                auto uses_it = operand_remaining_uses.find(value_id);
                if (uses_it != operand_remaining_uses.end() && uses_it->second > 1) {
                    return;
                }
                auto alloca_it = invoke_alloca_map.find(value_id);
                if (alloca_it == invoke_alloca_map.end()) {
                    return;
                }
                llvm::Value* ca = alloca_it->second;
                if (!ca) {
                    ca = promoteSsaEntryToAlloca(value_id, module, llvm_func);
                }
                clear_cleanup_alloca(ca);
            };
            auto track_owned_value_cleanup = [&](llvm::Value* owned, const char* name) -> llvm::AllocaInst* {
                llvm::Function* func = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* entry = &func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                llvm::AllocaInst* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr, name);
                alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup);
                builder->CreateStore(owned, cleanup);
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
                registerInvokeCleanupAlloca(cleanup);
                return cleanup;
            };

            // Outer-scope variables: assign directly to the thread-local stack via
            // qore_rt_assign_local() without creating an alloca or lazy instantiation.
            // Explicit block-scoped locals are callee-owned even if not present in
            // pre_instantiated_locals, so they must stay on the local-allocation path.
            // In AOT mode, closure-use body locals are excluded from pre_instantiated_locals
            // (to avoid cvstack ordering issues), but they are NOT outer-scope variables —
            // they need lazy instantiation below, not the runtime assign helper (which would
            // fail because the variable was never instantiated on the cvstack).
            if (linst->local && pre_instantiated_locals
                    && !pre_instantiated_locals->count(key)
                    && !entry_locals_set.count(key)
                    && !block_scoped_locals.count(key)
                    && !(aot_mode && linst->is_closure)) {
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

                // Check if typed: apply coercion or type stripping
                const QoreTypeInfo* outer_ti = linst->local->getTypeInfoForLValue();
                bool is_complex_typed_outer = QoreTypeInfo::isComplex(outer_ti)
                        && !QoreTypeInfo::isReference(outer_ti);
                bool needs_type_strip_outer = !QoreTypeInfo::isComplex(outer_ti)
                        && !QoreTypeInfo::isReference(outer_ti)
                        && (QoreTypeInfo::isHashType(outer_ti)
                            || QoreTypeInfo::isListType(outer_ti));
                bool needs_scalar_coerce_outer = QoreTypeInfo::hasType(outer_ti)
                        && !QoreTypeInfo::isReference(outer_ti)
                        && !is_complex_typed_outer
                        && !needs_type_strip_outer
                        && !QoreTypeInfo::getTypedHash(outer_ti);
                bool needs_plain_any_outer = outer_ti == anyTypeInfo || outer_ti == autoNoNarrowTypeInfo;
                bool needs_value_coerce_outer = is_complex_typed_outer || needs_scalar_coerce_outer
                    || needs_plain_any_outer;
                llvm::Value* consumed_cleanup_outer = nullptr;
                if (needs_value_coerce_outer) {
                    // Apply assignment coercion before publishing the value to
                    // keep any StoreLocal result aligned with the runtime stack.
                    llvm::Function* func = builder->GetInsertBlock()->getParent();
                    llvm::BasicBlock* entry = &func->getEntryBlock();
                    llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                    auto* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr,
                            "coerce_cleanup_outer");
                    alloca_builder.CreateStore(
                            llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                    clear_cleanup_before_reuse(cleanup);
                    consumed_cleanup_outer = cleanup;

                    llvm::Value* coerced;
                    if (aot_mode) {
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                        auto cv_aot_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i64_type, ptr_type, ptr_type}, false);
                        auto coerce_fn = module.getOrInsertFunction(
                                "qore_rt_coerce_value_aot", cv_aot_ft);
                        auto coerce_fn_throwing = module.getOrInsertFunction(
                                "qore_rt_coerce_value_aot_throwing", cv_aot_ft);
                        coerced = emitMaybeInvoke(coerce_fn, coerce_fn_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                                 boxed, cleanup, xsink_arg},
                                module, llvm_func, inst);
                    } else {
                        auto cv_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i64_type, ptr_type, ptr_type}, false);
                        auto coerce_fn = module.getOrInsertFunction(
                                "qore_rt_coerce_value", cv_ft);
                        auto coerce_fn_throwing = module.getOrInsertFunction(
                                "qore_rt_coerce_value_throwing", cv_ft);
                        llvm::Value* ti_ptr = llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(outer_ti));
                        llvm::Value* ti_as_ptr = builder->CreateIntToPtr(ti_ptr, ptr_type);
                        coerced = emitMaybeInvoke(coerce_fn, coerce_fn_throwing,
                                {ti_as_ptr, boxed, cleanup, xsink_arg},
                                module, llvm_func, inst);
                    }
                    emitExceptionCheck(module, llvm_func, inst);
                    boxed = coerced;
                    registerInvokeCleanupAlloca(cleanup);
                } else if (needs_type_strip_outer) {
                    llvm::Function* func = builder->GetInsertBlock()->getParent();
                    llvm::BasicBlock* entry = &func->getEntryBlock();
                    llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                    auto* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr,
                            "strip_cleanup_outer");
                    alloca_builder.CreateStore(
                            llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                    clear_cleanup_before_reuse(cleanup);
                    consumed_cleanup_outer = cleanup;

                    llvm::Value* stripped;
                    if (aot_mode) {
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                        auto cv_aot_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i64_type, ptr_type, ptr_type}, false);
                        auto strip_fn = module.getOrInsertFunction(
                                "qore_rt_coerce_value_aot", cv_aot_ft);
                        auto strip_fn_throwing = module.getOrInsertFunction(
                                "qore_rt_coerce_value_aot_throwing", cv_aot_ft);
                        stripped = emitMaybeInvoke(strip_fn, strip_fn_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                                 boxed, cleanup, xsink_arg},
                                module, llvm_func, inst);
                    } else {
                        auto cv_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i64_type, ptr_type, ptr_type}, false);
                        auto strip_fn = module.getOrInsertFunction(
                                "qore_rt_coerce_value", cv_ft);
                        auto strip_fn_throwing = module.getOrInsertFunction(
                                "qore_rt_coerce_value_throwing", cv_ft);
                        llvm::Value* ti_ptr = llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(outer_ti));
                        llvm::Value* ti_as_ptr = builder->CreateIntToPtr(ti_ptr, ptr_type);
                        stripped = emitMaybeInvoke(strip_fn, strip_fn_throwing,
                                {ti_as_ptr, boxed, cleanup, xsink_arg},
                                module, llvm_func, inst);
                    }
                    emitExceptionCheck(module, llvm_func, inst);
                    boxed = stripped;
                    registerInvokeCleanupAlloca(cleanup);
                }
                if (linst->weak) {
                    auto weak_fn = module.getOrInsertFunction("qore_rt_make_weak_value",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    llvm::Value* weak_boxed = builder->CreateCall(weak_fn, {boxed, xsink_arg});
                    track_owned_value_cleanup(weak_boxed, "weak_cleanup_outer");
                    clear_cleanup_alloca(consumed_cleanup_outer);
                    clear_consumed_operand_cleanup(inst->operands[0].id);
                    boxed = weak_boxed;
                }

                if (aot_mode) {
                    bool use_no_coerce_outer = linst->weak || needs_value_coerce_outer || needs_type_strip_outer;
                    const char* helper_name = use_no_coerce_outer ? "qore_rt_assign_local_no_coerce_aot"
                            : "qore_rt_assign_local_aot";
                    auto assign_helper = module.getOrInsertFunction(helper_name,
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    builder->CreateCall(assign_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), boxed, xsink_arg});
                } else {
                    bool use_no_coerce_outer_jit = linst->weak || needs_value_coerce_outer || needs_type_strip_outer;
                    const char* helper_name = use_no_coerce_outer_jit ? "qore_rt_assign_local_no_coerce"
                            : "qore_rt_assign_local";
                    auto assign_helper = module.getOrInsertFunction(helper_name,
                            llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                    builder->CreateCall(assign_helper, {var_as_ptr, boxed, xsink_arg});
                }
                if (!linst->weak) {
                    clear_cleanup_alloca(consumed_cleanup_outer);
                    clear_consumed_operand_cleanup(inst->operands[0].id);
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
                markLocalCacheFresh(key, llvm_func);
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
                markLocalCacheFresh(key, llvm_func);
                if (inst->result.isValid()) {
                    values[inst->result.id] = native_val;
                    // NOT nanboxed
                }
                return true;
            }

            bool is_ir_only = ir_only_locals_set && ir_only_locals_set->count(key);

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
                registerInvokeCleanupAlloca(cleanup);
            }
            // Type handling before storing to the local alloca.
            bool is_aot_body_local = aot_mode && aot_body_locals.count(key);
            const QoreTypeInfo* local_ti = linst->local ? linst->local->getTypeInfoForLValue() : nullptr;

            // Case 1: Complex hash/list types (not hashdecl) need type coercion
            // via acceptAssignment() to set complexTypeInfo for runtime variant
            // matching (e.g., list<hash<auto>> matches typed signatures).
            // Hashdecl types must NOT go through coercion — it strips hashdecl
            // annotations.  Use getTypedHash() to detect hashdecl types.
            bool is_complex_typed = linst->local
                && QoreTypeInfo::isComplex(local_ti)
                && !QoreTypeInfo::isReference(local_ti)
                && !QoreTypeInfo::getTypedHash(local_ti);

            // Case 2: Plain hash/list types need type STRIPPING.
            // When a hash literal like (key: 1) is created by qore_rt_make_hash,
            // it gets a narrowed type (e.g., hash<string, int>) from value
            // inference. Storing this to a plain "hash" variable must strip the
            // narrowed type to prevent spurious RUNTIME-TYPE-ERROR when
            // heterogeneous values are later assigned to hash keys.
            // Unlike complex coercion (which copies), this modifies in place.
            bool needs_type_strip = linst->local
                && !QoreTypeInfo::isComplex(local_ti)
                && !QoreTypeInfo::isReference(local_ti)
                && (QoreTypeInfo::isHashType(local_ti)
                    || QoreTypeInfo::isListType(local_ti));

            // Case 3: Scalar typed locals also need assignment coercion before
            // storing into the LLVM alloca.  Otherwise a softint local assigned
            // from a string is coerced on the runtime stack but remains a string
            // in the native local cache, so later native uses see the wrong type.
            bool needs_scalar_coerce = linst->local
                && QoreTypeInfo::hasType(local_ti)
                && !QoreTypeInfo::isReference(local_ti)
                && !is_complex_typed
                && !needs_type_strip
                && !QoreTypeInfo::getTypedHash(local_ti);
            bool needs_plain_any = local_ti == anyTypeInfo || local_ti == autoNoNarrowTypeInfo;
            bool needs_value_coerce = is_complex_typed || needs_scalar_coerce || needs_plain_any;
            llvm::Value* consumed_cleanup = nullptr;

            // IR-only locals still need their cached alloca value to match
            // the declared container type.  Only runtime-stack sync is
            // skipped for IR-only locals; otherwise a literal like
            // {"initialized": False} stored into hash<auto!> keeps its
            // inferred hash<bool> type and later key writes reject floats.
            if (needs_value_coerce) {
                // Assignment coercion must happen BEFORE storing to alloca.
                // Coerce once here; use no-coerce assign variant below to avoid
                // double-coercion on the runtime stack.
                llvm::Function* func = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* entry = &func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                auto* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr,
                        "coerce_cleanup");
                alloca_builder.CreateStore(
                        llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                clear_cleanup_before_reuse(cleanup);
                consumed_cleanup = cleanup;

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
                            reinterpret_cast<uint64_t>(local_ti));
                    llvm::Value* ti_as_ptr = builder->CreateIntToPtr(ti_ptr, ptr_type);
                    coerced = builder->CreateCall(coerce_fn,
                            {ti_as_ptr, boxed, cleanup, xsink_arg});
                }
                emitExceptionCheck(module, llvm_func, inst);
                boxed = coerced;
                registerInvokeCleanupAlloca(cleanup);
                if (block_scoped_locals.count(key)) {
                    local_cleanup_allocas[key].push_back(cleanup);
                }
            } else if (needs_type_strip) {
                // Plain hash/list type stripping must mirror lvalue assignment:
                // typed nested containers are deep-copied without type metadata
                // so later writes through the untyped local do not enforce the
                // source hashdecl or complex value type.
                llvm::Function* func = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* entry = &func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                auto* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr,
                        "strip_cleanup");
                alloca_builder.CreateStore(
                        llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                clear_cleanup_before_reuse(cleanup);
                consumed_cleanup = cleanup;

                llvm::Value* stripped;
                if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    auto strip_fn = module.getOrInsertFunction("qore_rt_coerce_value_aot",
                            llvm::FunctionType::get(i64_type,
                                    {ptr_type, i32_type, i64_type, ptr_type, ptr_type}, false));
                    stripped = builder->CreateCall(strip_fn,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                             boxed, cleanup, xsink_arg});
                } else {
                    auto strip_fn = module.getOrInsertFunction("qore_rt_coerce_value",
                            llvm::FunctionType::get(i64_type, {ptr_type, i64_type, ptr_type, ptr_type}, false));
                    llvm::Value* ti_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(local_ti));
                    llvm::Value* ti_as_ptr = builder->CreateIntToPtr(ti_ptr, ptr_type);
                    stripped = builder->CreateCall(strip_fn,
                            {ti_as_ptr, boxed, cleanup, xsink_arg});
                }
                emitExceptionCheck(module, llvm_func, inst);
                boxed = stripped;
                registerInvokeCleanupAlloca(cleanup);
                if (block_scoped_locals.count(key)) {
                    local_cleanup_allocas[key].push_back(cleanup);
                }
            }
            if (linst->weak) {
                auto weak_fn = module.getOrInsertFunction("qore_rt_make_weak_value",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                llvm::Value* weak_boxed = builder->CreateCall(weak_fn, {boxed, xsink_arg});
                llvm::AllocaInst* weak_cleanup = track_owned_value_cleanup(weak_boxed, "weak_cleanup");
                if (block_scoped_locals.count(key)) {
                    local_cleanup_allocas[key].push_back(weak_cleanup);
                }
                clear_cleanup_alloca(consumed_cleanup);
                clear_consumed_operand_cleanup(inst->operands[0].id);
                boxed = weak_boxed;
            }

            if (is_ir_only) {
                // IR-only locals do not publish this value to the runtime stack,
                // so the local cache (or its associated cleanup slot) must own a
                // reference independent of statement temporaries.
                llvm::Value* owned_boxed = emitHelperRef(module, boxed);
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

                auto preinst_cleanup = preinstantiated_entry_cleanup_by_local.find(key);
                if (preinst_cleanup != preinstantiated_entry_cleanup_by_local.end()) {
                    // Pre-instantiated params keep ownership in their cleanup slot;
                    // the local alloca borrows the same value.
                    llvm::Value* old_val = builder->CreateLoad(i64_type, preinst_cleanup->second);
                    builder->CreateStore(owned_boxed, preinst_cleanup->second);
                    builder->CreateCall(decref_fn, {old_val, xsink_arg});
                } else {
                    // Fast-entry params and pure IR-only body locals own the value
                    // directly in the local alloca.  Take the new ref before
                    // dropping the old one so `x = x` remains safe.
                    llvm::Value* old_val = builder->CreateLoad(i64_type, it->second);
                    builder->CreateCall(decref_fn, {old_val, xsink_arg});

                    if (!fast_entry_param_allocas_by_local.count(key)
                            && owned_ir_local_alloca_keys.insert(key).second) {
                        if (auto* local_ai = llvm::dyn_cast<llvm::AllocaInst>(it->second)) {
                            owned_ir_local_allocas.push_back(local_ai);
                        }
                    }
                }

                boxed = owned_boxed;
            }

            if (!is_ir_only) {
                clearLocalCachedValue(key, module, llvm_func,
                    LocalCacheClearMode::IncludeFastEntryOwner);
                clearLocalReloadTracker(key, module, llvm_func);
            }
            builder->CreateStore(boxed, it->second);
            markLocalCacheFresh(key, llvm_func);
            if (inst->result.isValid()) {
                values[inst->result.id] = boxed;
            }
            // Track cleanup alloca for block-scoped locals: if the stored value has
            // an invoke-result cleanup alloca, record the mapping so UninstantiateLocal
            // can clear it to allow timely destruction at block scope exit.
            if (block_scoped_locals.count(key)) {
                auto alloca_it = invoke_alloca_map.find(inst->operands[0].id);
                if (alloca_it != invoke_alloca_map.end()) {
                    llvm::Value* ca = alloca_it->second;
                    if (!ca) {
                        ca = promoteSsaEntryToAlloca(inst->operands[0].id, module,
                                llvm_func);
                    }
                    if (ca) {
                        local_cleanup_allocas[key].push_back(ca);
                    }
                }
            }
            // Sync to Qore thread-local variable stack so AST callbacks can resolve this local.
            // Skip sync for IR-only locals unless this is a weak assignment: weak loads
            // deliberately go through LocalVar::eval() on every read to observe deleted targets.
            if (linst->local && (!is_ir_only || linst->weak)) {
                // For closure-captured pre-instantiated block-scoped locals (loop-body vars):
                // the CVV may have been popped by a previous UninstantiateLocal.  Re-instantiate
                // (push fresh CVV) before assigning so qore_rt_assign_local finds it on the stack.
                auto flag_it = closure_pre_inst_flags.find(key);
                if (!aot_mode && flag_it != closure_pre_inst_flags.end()) {
                    llvm::Type* i1_type = llvm::Type::getInt1Ty(module.getContext());
                    llvm::Value* is_active = builder->CreateLoad(i1_type, flag_it->second);
                    llvm::BasicBlock* need_reinst = llvm::BasicBlock::Create(
                            module.getContext(), "closure_reinst_store", llvm_func);
                    llvm::BasicBlock* after_reinst = llvm::BasicBlock::Create(
                            module.getContext(), "after_closure_reinst", llvm_func);
                    builder->CreateCondBr(is_active, after_reinst, need_reinst);

                    builder->SetInsertPoint(need_reinst);
                    auto inst_helper = module.getOrInsertFunction("qore_rt_instantiate_local",
                            llvm::FunctionType::get(void_type, {ptr_type}, false));
                    llvm::Value* var_ptr_inst = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr_inst = builder->CreateIntToPtr(var_ptr_inst, ptr_type);
                    builder->CreateCall(inst_helper, {var_as_ptr_inst});
                    builder->CreateStore(llvm::ConstantInt::get(i1_type, 1), flag_it->second);
                    builder->CreateBr(after_reinst);

                    builder->SetInsertPoint(after_reinst);
                }

                // For pre-coerced, type-stripped, or hashdecl locals, use no-coerce variant.
                // Typed values: coercion was already applied above via qore_rt_coerce_value.
                // Type-stripped: qore_rt_coerce_value already produced the
                // plain hash/list value.
                // Hashdecl types: runtime type checking via acceptAssignment rejects hashes
                // that have complexTypeInfo set instead of hashdecl (a valid state that the
                // IR interpreter's fast path accepts). Using no-coerce aligns with IR behavior.
                bool use_no_coerce = needs_value_coerce || needs_type_strip
                    || linst->weak || QoreTypeInfo::getTypedHash(local_ti);
                const char* aot_helper_name = use_no_coerce ? "qore_rt_assign_local_no_coerce_aot"
                        : "qore_rt_assign_local_aot";
                const char* aot_helper_throwing_name = use_no_coerce
                        ? "qore_rt_assign_local_no_coerce_aot_throwing"
                        : "qore_rt_assign_local_aot_throwing";
                const char* jit_helper_name = use_no_coerce ? "qore_rt_assign_local_no_coerce"
                        : "qore_rt_assign_local";
                const char* jit_helper_throwing_name = use_no_coerce
                        ? "qore_rt_assign_local_no_coerce_throwing"
                        : "qore_rt_assign_local_throwing";

                if (aot_mode) {
                    auto aot_ft = llvm::FunctionType::get(void_type,
                            {ptr_type, i32_type, i64_type, ptr_type}, false);
                    auto assign_helper = module.getOrInsertFunction(aot_helper_name, aot_ft);
                    auto assign_helper_throwing = module.getOrInsertFunction(
                            aot_helper_throwing_name, aot_ft);
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    emitMaybeInvoke(assign_helper, assign_helper_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                             boxed, xsink_arg},
                            module, llvm_func, inst);
                } else {
                    auto jit_ft = llvm::FunctionType::get(void_type,
                            {ptr_type, i64_type, ptr_type}, false);
                    auto assign_helper = module.getOrInsertFunction(jit_helper_name, jit_ft);
                    auto assign_helper_throwing = module.getOrInsertFunction(
                            jit_helper_throwing_name, jit_ft);
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                    emitMaybeInvoke(assign_helper, assign_helper_throwing,
                            {var_as_ptr, boxed, xsink_arg}, module, llvm_func, inst);
                }
                // Check for exceptions from type-checked assignment (e.g. RUNTIME-TYPE-ERROR
                // when assigning NOTHING to a typed variable like hash<ExceptionInfo>)
                emitExceptionCheck(module, llvm_func, inst);
                // qore_rt_assign_local* may normalize the stored value (for
                // example, a heap-backed QoreBigIntNode assigned to an int local is
                // stored as an inline scalar and the temporary node is dereferenced).
                // AOT body locals are different: after StoreLocal publishes the
                // value, the runtime stack owns a +1 ref and the LLVM alloca may
                // safely borrow that same value.  When this lowering already used
                // the no-coerce helper (or the local has no assignment type), the
                // runtime slot stores the same bits already in the alloca.  Keep
                // the cache correct by retaining those bits directly instead of
                // emitting a qore_rt_load_local_aot() round trip.
                if (QoreTypeInfo::isReference(linst->local->getTypeInfo())) {
                    // Reference locals write through to another variable; any local
                    // cache could now be stale.
                    reloadAllLocalsFromRuntime(module, llvm_func);
                } else if (is_aot_body_local
                        && !linst->weak
                        && (use_no_coerce || !QoreTypeInfo::hasType(local_ti))) {
                    retainLocalCacheValue(key, boxed, module, llvm_func, false);
                } else {
                    reloadLocalFromRuntime(key, module, llvm_func, false);
                }
                if (!linst->weak) {
                    clear_cleanup_alloca(consumed_cleanup);
                    clear_consumed_operand_cleanup(inst->operands[0].id);
                }
            }
            return true;
        }
        case QoreIROpcode::InstantiateLocal: {
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            if (!linst->local || !linst->local->closureUse()) {
                return true;
            }
            auto key = reinterpret_cast<const void*>(linst->local);
            // Only instantiate if not already done
            if (instantiated_non_entry_locals.insert(key).second) {
                if (aot_mode) {
                    auto helper = module.getOrInsertFunction("qore_rt_instantiate_local_aot",
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot)});
                } else {
                    auto helper = module.getOrInsertFunction("qore_rt_instantiate_local",
                            llvm::FunctionType::get(void_type, {ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                    builder->CreateCall(helper, {var_as_ptr});
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
            bool is_ir_only_local = (ir_only_locals_set && ir_only_locals_set->count(key))
                    || ir_only_body_locals.count(key);
            auto clear_owned_ir_only_local = [&]() -> bool {
                if (!is_ir_only_local) {
                    return false;
                }

                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));

                auto preinst_cleanup = preinstantiated_entry_cleanup_by_local.find(key);
                if (preinst_cleanup != preinstantiated_entry_cleanup_by_local.end()) {
                    llvm::Value* old_val = builder->CreateLoad(i64_type, preinst_cleanup->second);
                    builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                            preinst_cleanup->second);
                    builder->CreateCall(decref_fn, {old_val, xsink_arg});
                    return true;
                }

                if (owned_ir_local_alloca_keys.count(key)
                        || fast_entry_param_allocas_by_local.count(key)) {
                    auto alloca_it = local_allocas.find(key);
                    if (alloca_it != local_allocas.end()) {
                        llvm::Value* old_val = builder->CreateLoad(i64_type, alloca_it->second);
                        builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                                alloca_it->second);
                        builder->CreateCall(decref_fn, {old_val, xsink_arg});
                        return true;
                    }
                }

                return false;
            };

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

                // Clear the reload tracker and deferred alloca for this local
                // (if they exist).  Lazy reloads create refSelf'd values in reload
                // trackers via qore_rt_load_local(); if we don't clear them here,
                // the extra reference prevents timely destruction at block scope
                // exit.  The deferred alloca holds the previous tracker value
                // (from the prior reload cycle) and must also be cleared.
                auto tracker_it = local_reload_trackers.find(key);
                if (tracker_it != local_reload_trackers.end()) {
                    llvm::Value* old_tracker = builder->CreateLoad(i64_type, tracker_it->second);
                    builder->CreateStore(
                            llvm::ConstantInt::get(i64_type, VAL_NOTHING), tracker_it->second);
                    builder->CreateCall(decref_fn, {old_tracker, xsink_arg});
                }
                auto deferred_it = local_reload_deferred.find(key);
                if (deferred_it != local_reload_deferred.end()) {
                    llvm::Value* old_deferred = builder->CreateLoad(i64_type, deferred_it->second);
                    builder->CreateStore(
                            llvm::ConstantInt::get(i64_type, VAL_NOTHING), deferred_it->second);
                    builder->CreateCall(decref_fn, {old_deferred, xsink_arg});
                }

                // IR-only locals are not necessarily represented on the runtime
                // stack.  If their local cache/cleanup slot owns a reference,
                // drop it at block scope exit instead of waiting for function
                // exit; otherwise destructors and CoW-sensitive refcounts are
                // delayed past the source-level lifetime.
                clear_owned_ir_only_local();

                // Clear the runtime stack/cvstack entry (drops refSelf'd reference).
                // The cleanup alloca decref above dropped the original reference from
                // new_object/invoke.  Together, all references are dropped and the
                // destructor fires.
                // Skip for IR-only body locals: they are not on the thread-local stack
                // (fast call path skips instantiation when areAllBodyLocalsIROnly()),
                // so there is nothing to clear on the runtime stack.
                if (!ir_only_body_locals.count(key)) {
                    if (linst->is_closure) {
                        // Closure-captured block-scoped variables: pop the ClosureVarValue
                        // from the cvstack (giving each iteration an independent binding so
                        // closures capture their own CVV).
                        if (aot_mode) {
                            // In AOT mode, closure-use vars are lazily instantiated
                            // (by StoreLocal or by LocalVar::getLValue/eval at runtime).
                            // qore_rt_pop_closure_var_aot safely checks if the var is
                            // on the cvstack before popping.
                            auto uninst_helper = module.getOrInsertFunction("qore_rt_pop_closure_var_aot",
                                    llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
                            int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                            builder->CreateCall(uninst_helper, {aot_ctx_arg,
                                    llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                        } else {
                            // qore_rt_uninstantiate_local handles both loop-body and
                            // block-exit cases: it clears the CVV only when the
                            // refcount drops to 1, matching the cycle-aware semantics
                            // needed for both (DGC handles cycle collection when
                            // other references keep the CVV alive).
                            auto uninst_helper = module.getOrInsertFunction("qore_rt_uninstantiate_local",
                                    llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false));
                            llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                                    reinterpret_cast<uint64_t>(linst->local));
                            llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                            builder->CreateCall(uninst_helper, {var_as_ptr, xsink_arg});
                            // Clear the is_active flag so emitPreInstClosureReInstantiation and
                            // StoreLocal know to re-instantiate before next use.
                            auto flag_it = closure_pre_inst_flags.find(key);
                            if (flag_it != closure_pre_inst_flags.end()) {
                                llvm::Type* i1_type = llvm::Type::getInt1Ty(module.getContext());
                                builder->CreateStore(
                                        llvm::ConstantInt::get(i1_type, 0), flag_it->second);
                            }
                        }
                    } else {
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
            // reference while assign_local's refSelf and lazy local reloads may add more.
            // We must drop all of them here.
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
                auto deferred_it2 = local_reload_deferred.find(key);
                if (deferred_it2 != local_reload_deferred.end()) {
                    llvm::Value* old_deferred = builder->CreateLoad(i64_type, deferred_it2->second);
                    builder->CreateStore(
                            llvm::ConstantInt::get(i64_type, VAL_NOTHING), deferred_it2->second);
                    builder->CreateCall(decref_fn, {old_deferred, xsink_arg});
                }

                // Drop IR-only owned local refs at source block exit.  Non-IR-only
                // locals are handled by the runtime uninstantiate helper below.
                clear_owned_ir_only_local();
            }

            // Call runtime helper to uninstantiate the local variable
            if (aot_mode) {
                auto it = aot_slots->local_slots.find(linst->local);
                if (it == aot_slots->local_slots.end()) {
                    error = "UninstantiateLocal: local variable not in AOT slot map";
                    return false;
                }
                int32_t slot_idx = it->second;
                // For closure-use vars not pre-instantiated by evalTiered: proper pop
                if (linst->local && linst->local->closureUse()) {
                    auto helper = module.getOrInsertFunction("qore_rt_pop_closure_var_aot",
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
                    builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot_idx), xsink_arg});
                } else {
                    auto helper = module.getOrInsertFunction("qore_rt_uninstantiate_local_aot",
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
                    builder->CreateCall(helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot_idx), xsink_arg});
                }
            } else {
                // qore_rt_uninstantiate_local handles closure-use vars (clears
                // CVV when refs==1) and non-closure vars (plain uninstantiate)
                // uniformly, so there is no separate block-exit variant.
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
        case QoreIROpcode::RefSelf: {
            if (inst->operands.empty()) {
                error = "RefSelf: missing operand";
                return false;
            }
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* boxed = boxValue(val, inst->operands[0].id);
            llvm::Value* retained = emitHelperRef(module, boxed);
            values[inst->result.id] = retained;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(retained, inst->result.id, llvm_func);
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
            if (br->target && br->target->is_loop_header) {
                auto check_cancel = module.getOrInsertFunction("qore_rt_check_cancel",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                llvm::Value* operation = builder->CreateGlobalString("IR loop", "loop_cancel_operation");
                builder->CreateCall(check_cancel, {xsink_arg, operation});
                emitExceptionCheck(module, llvm_func, inst);
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
            consumeValueUse(br->condition.id, module);
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
            ensureLocalCacheFresh(target_key, module, llvm_func);
            ensureLocalCacheFresh(source_key, module, llvm_func);
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
            markLocalCacheFresh(target_key, llvm_func);
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
            ensureLocalCacheFresh(key, module, llvm_func);
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
            markLocalCacheFresh(key, llvm_func);
            if (inst->result.isValid()) {
                values[inst->result.id] = result;
                // NOT nanboxed — native int result
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

            consumeValueUse(sw->switch_val.id, module);

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

            consumeValueUse(sw->switch_val.id, module);

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
                // Deferred exception check for init functions (Phase 3: LLVM hang fix).
                // Placed before incref so the exception path (→ error_return_block) has no
                // unmatched incref. The success path (→ cont) handles incref + cleanup + ret.
                if (deferred_exception_checking && deferred_check_needed) {
                    auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
                            llvm::FunctionType::get(i64_type, {ptr_type}, false));
                    llvm::Value* ex_check = builder->CreateCall(has_ex, {xsink_arg});
                    llvm::Value* has_exception = builder->CreateICmpNE(ex_check,
                            llvm::ConstantInt::get(i64_type, 0));
                    if (!error_return_block) {
                        error_return_block = llvm::BasicBlock::Create(ctx, "error_return",
                            static_cast<llvm::Function*>(builder->GetInsertBlock()->getParent()));
                    }
                    llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "no_exception",
                        static_cast<llvm::Function*>(builder->GetInsertBlock()->getParent()));
                    // Phase 2B — route exception edge through a preamble
                    // that decrefs pending SSA; normal cont flow keeps it.
                    emitCondBrWithSsaPreamble(module, llvm_func, has_exception,
                            error_return_block, cont);
                    builder->SetInsertPoint(cont);
                    // Fall through — incref + cleanup + ret now emitted into cont
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
            // Phase 2B — release SSA-direct cleanup entries that dominate
            // this return site.  The incref above ensured the returned
            // value's pending entry (if any) cancels out to net-zero ref
            // change; other entries are unwound here.
            emitPendingSsaCleanup(module);
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
            // Deferred exception check for init functions (Phase 3: LLVM hang fix)
            if (deferred_exception_checking && deferred_check_needed) {
                auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
                        llvm::FunctionType::get(i64_type, {ptr_type}, false));
                llvm::Value* ex_check = builder->CreateCall(has_ex, {xsink_arg});
                llvm::Value* has_exception = builder->CreateICmpNE(ex_check,
                        llvm::ConstantInt::get(i64_type, 0));
                if (!error_return_block) {
                    error_return_block = llvm::BasicBlock::Create(ctx, "error_return",
                        static_cast<llvm::Function*>(builder->GetInsertBlock()->getParent()));
                }
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "no_exception",
                    static_cast<llvm::Function*>(builder->GetInsertBlock()->getParent()));
                // Phase 2B — route exception edge through a preamble
                // that decrefs pending SSA; keeps normal flow SSA.
                emitCondBrWithSsaPreamble(module, llvm_func, has_exception,
                        error_return_block, cont);
                builder->SetInsertPoint(cont);
            }
            emitOnBlockExitExec(module);
            emitIteratorCleanup(module);
            emitPreinstantiatedCleanup(module);
            emitInvokeCleanup(module);
            emitPendingSsaCleanup(module);
            emitLocalUninstantiation(module);
            builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            return true;
        }

        // === String constants ===
        case QoreIROpcode::ConstString: {
            const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
            // Create a global constant string and pass an explicit byte length so embedded NULs are preserved.
            llvm::Constant* str_const = builder->CreateGlobalString(cinst->constant.string_value);
            auto helper = module.getOrInsertFunction("qore_rt_make_string_len",
                    llvm::FunctionType::get(i64_type, {ptr_type, i64_type}, false));
            llvm::Value* len = llvm::ConstantInt::get(i64_type, cinst->constant.string_value.size());
            llvm::Value* str_result = builder->CreateCall(helper, {str_const, len});
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

            } else if (inv->invoke_opcode == QoreIROpcode::AddString
                    && inv->operands.size() >= 2) {
                auto* lhs = getVal(inv->operands[0].id, error);
                auto* rhs = getVal(inv->operands[1].id, error);
                if (!lhs || !rhs) { return false; }
                llvm::Value* lhs_boxed = boxValue(lhs, inv->operands[0].id);
                llvm::Value* rhs_boxed = boxValue(rhs, inv->operands[1].id);
                auto helper = module.getOrInsertFunction("qore_rt_string_add_typed",
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed, xsink_arg});
                // String concatenation does not modify locals.

            } else if (inv->invoke_opcode == QoreIROpcode::StringConcat
                    && !inv->operands.empty()) {
                llvm::Value* args_array;
                int nargs;
                if (!buildArgsArray(inst, 0, llvm_func, args_array, nargs, error)) {
                    return false;
                }
                auto helper = module.getOrInsertFunction("qore_rt_string_concat_multi",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper, {args_array,
                        llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                // String concatenation does not modify locals.

            } else if ((inv->invoke_opcode == QoreIROpcode::EqString
                        || inv->invoke_opcode == QoreIROpcode::NeString
                        || inv->invoke_opcode == QoreIROpcode::LtString
                        || inv->invoke_opcode == QoreIROpcode::LeString
                        || inv->invoke_opcode == QoreIROpcode::GtString
                        || inv->invoke_opcode == QoreIROpcode::GeString)
                    && inv->operands.size() >= 2) {
                auto* lhs = getVal(inv->operands[0].id, error);
                auto* rhs = getVal(inv->operands[1].id, error);
                if (!lhs || !rhs) { return false; }
                llvm::Value* lhs_boxed = boxValue(lhs, inv->operands[0].id);
                llvm::Value* rhs_boxed = boxValue(rhs, inv->operands[1].id);

                const char* helper_name = nullptr;
                bool helper_takes_xsink = false;
                switch (inv->invoke_opcode) {
                    case QoreIROpcode::EqString:
                        helper_name = "qore_rt_string_eq_typed";
                        helper_takes_xsink = true;
                        break;
                    case QoreIROpcode::NeString:
                        helper_name = "qore_rt_string_ne_typed";
                        helper_takes_xsink = true;
                        break;
                    case QoreIROpcode::LtString:
                        helper_name = "qore_rt_string_lt_typed";
                        break;
                    case QoreIROpcode::LeString:
                        helper_name = "qore_rt_string_le_typed";
                        break;
                    case QoreIROpcode::GtString:
                        helper_name = "qore_rt_string_gt_typed";
                        break;
                    case QoreIROpcode::GeString:
                        helper_name = "qore_rt_string_ge_typed";
                        break;
                    default:
                        break;
                }
                if (helper_takes_xsink) {
                    auto helper = module.getOrInsertFunction(helper_name,
                            llvm::FunctionType::get(i64_type,
                                {i64_type, i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed, xsink_arg});
                } else {
                    auto helper = module.getOrInsertFunction(helper_name,
                            llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
                    result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed});
                }
                // String comparisons do not modify locals.

            } else if (inv->invoke_opcode == QoreIROpcode::ListAssignAny
                    && inv->operands.size() >= 2) {
                auto* rhs = getVal(inv->operands[0].id, error);
                auto* idx = getVal(inv->operands[1].id, error);
                if (!rhs || !idx) { return false; }
                llvm::Value* rhs_boxed = boxValue(rhs, inv->operands[0].id);
                llvm::Value* idx_int;
                if (nanboxed_values.count(inv->operands[1].id)) {
                    auto unbox_fn = module.getOrInsertFunction("qore_rt_get_int64",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    idx_int = builder->CreateCall(unbox_fn, {idx, xsink_arg});
                } else {
                    idx_int = idx;
                }
                auto helper = module.getOrInsertFunction("qore_rt_list_assignment_value",
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {rhs_boxed, idx_int, xsink_arg});
                // List-assignment extraction does not modify locals.

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
                    // Extract regex pattern at codegen time — avoids EXPR_TREE slot
                    QoreRegex* re = nullptr;
                    if (auto* mn = dynamic_cast<const QoreRegexMatchOperatorNode*>(
                            inv->expr.getInternalNode())) {
                        re = mn->getRegex();
                    } else if (auto* nmn = dynamic_cast<const QoreRegexNMatchOperatorNode*>(
                            inv->expr.getInternalNode())) {
                        re = nmn->getRegex();
                    } else if (auto* exn = dynamic_cast<const QoreRegexExtractOperatorNode*>(
                            inv->expr.getInternalNode())) {
                        re = exn->getRegex();
                    }
                    if (re && re->getPatternCStr()) {
                        llvm::Value* pattern_ptr = builder->CreateGlobalStringPtr(re->getPatternCStr());
                        llvm::Value* options_val = llvm::ConstantInt::get(i64_type, re->getOptions());
                        // Plumb the regex global flag (e.g. /g) — lives separately from
                        // PCRE options on QoreRegex. Without this, RegexExtract /g
                        // returns only the first match in AOT.
                        llvm::Value* global_val = llvm::ConstantInt::get(i32_type,
                            re->isGlobal() ? 1 : 0);
                        auto robp_ft = llvm::FunctionType::get(i64_type,
                                {i32_type, ptr_type, i64_type, i32_type, i64_type, ptr_type},
                                false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_regex_op_by_pattern", robp_ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_regex_op_by_pattern_throwing", robp_ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {opcode_val, pattern_ptr, options_val, global_val,
                                 operand_boxed, xsink_arg},
                                module, llvm_func, inst);
                    } else {
                        // Fallback to slot-based dispatch
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                        auto rowo_aot_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i32_type, i64_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_regex_op_with_operand_aot", rowo_aot_ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_regex_op_with_operand_aot_throwing", rowo_aot_ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, opcode_val,
                                 llvm::ConstantInt::get(i32_type, slot),
                                 operand_boxed, xsink_arg},
                                module, llvm_func, inst);
                    }
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto rowo_ft = llvm::FunctionType::get(i64_type,
                            {i32_type, i64_type, i64_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_regex_op_with_operand", rowo_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_regex_op_with_operand_throwing", rowo_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {opcode_val, expr_const, operand_boxed, xsink_arg},
                            module, llvm_func, inst);
                }
                // Regex ops don't modify locals — no reload needed

            } else if (isCallInvokeOpcode(inv->invoke_opcode)
                    && (!inv->operands.empty()
                        || inv->invoke_opcode == QoreIROpcode::Call
                        || inv->invoke_opcode == QoreIROpcode::CallDirect
                        || inv->invoke_opcode == QoreIROpcode::CallMethod
                        || inv->invoke_opcode == QoreIROpcode::CallMethodDirect
                        || inv->invoke_opcode == QoreIROpcode::CallStatic
                        || inv->invoke_opcode == QoreIROpcode::CallStaticDirect)) {
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
                bool has_arg_cleanups = false;
                llvm::Value* arg_cleanups = buildArgCleanupArray(inst, arg_start,
                        llvm_func, nargs, has_arg_cleanups);

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
                            if (has_arg_cleanups) {
                                auto clear_helper = module.getOrInsertFunction(
                                        "qore_rt_clear_arg_cleanups",
                                        llvm::FunctionType::get(void_type,
                                            {ptr_type, i32_type, ptr_type}, false));
                                builder->CreateCall(clear_helper, {arg_cleanups,
                                        llvm::ConstantInt::get(i32_type, nargs),
                                        xsink_arg});
                            }
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
                    // AOT CallDirect: check for self-recursive fast entry first.
                    // Must compare QoreFunction IDENTITY (pointer), not base
                    // names — otherwise a caller in namespace `OMQ2` whose
                    // body invokes `Util::substitute_env_vars` matched the
                    // base-name-only check against its own ir_func->name
                    // `substitute_env_vars(string)`, emitted a direct LLVM
                    // call to `OMQ2::substitute_env_vars(string)_fast`, and
                    // infinite-recursed in the compiled body.  The
                    // FunctionEntry (FE) held by the FunctionCallNode
                    // uniquely identifies the resolved target; compare FE
                    // pointer to the current function's FE.
                    bool is_self_rec = false;
                    const FunctionCallNode* self_call = nullptr;
                    if (!aot_self_recursive_fast_entry.empty()) {
                        self_call = dynamic_cast<const FunctionCallNode*>(
                                inv->expr.getInternalNode());
                        if (self_call && self_call->getFunctionEntry()
                                && current_ir_func
                                && aot_self_recursive_fe
                                && self_call->getFunctionEntry() == aot_self_recursive_fe) {
                            is_self_rec = true;
                        }
                    }
                    if (is_self_rec && isFastFunctionCallEligible(self_call->getVariant())) {
                        // AOT Approach B self-recursive: direct LLVM call to fast entry
                        llvm::Function* fast_fn = module.getFunction(
                                aot_self_recursive_fast_entry);
                        assert(fast_fn
                                && "AOT self-recursive fast entry must be in module");
                        unsigned fast_num_params = fast_fn->arg_size() - 2;
                        std::vector<llvm::Value*> call_args;
                        for (unsigned i = 0; i < fast_num_params; ++i) {
                            if (i < boxed_args.size()) {
                                call_args.push_back(boxed_args[i]);
                            } else {
                                call_args.push_back(llvm::ConstantInt::get(
                                        i64_type, VAL_NOTHING));
                            }
                        }
                        call_args.push_back(aot_ctx_arg);
                        call_args.push_back(xsink_arg);
                        result = builder->CreateCall(fast_fn, call_args);
                        if (has_arg_cleanups) {
                            auto clear_helper = module.getOrInsertFunction(
                                    "qore_rt_clear_arg_cleanups",
                                    llvm::FunctionType::get(void_type,
                                        {ptr_type, i32_type, ptr_type}, false));
                            builder->CreateCall(clear_helper, {arg_cleanups,
                                    llvm::ConstantInt::get(i32_type, nargs),
                                    xsink_arg});
                        }
                    } else {
                        // Generic AOT CallDirect: use fast direct call helper
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(
                                expr_bits);
                        if (has_arg_cleanups) {
                            auto ft = llvm::FunctionType::get(i64_type,
                                    {ptr_type, i32_type, ptr_type, ptr_type, i32_type, ptr_type}, false);
                            auto helper = module.getOrInsertFunction(
                                    "qore_rt_call_direct_aot_consume_args", ft);
                            auto helper_throwing = module.getOrInsertFunction(
                                    "qore_rt_call_direct_aot_consume_args_throwing", ft);
                            result = emitMaybeInvoke(helper, helper_throwing,
                                    {aot_ctx_arg,
                                     llvm::ConstantInt::get(i32_type, slot),
                                     args_array, arg_cleanups,
                                     llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                    module, llvm_func, inst);
                        } else {
                            auto ft = llvm::FunctionType::get(i64_type,
                                    {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                            auto helper = module.getOrInsertFunction(
                                    "qore_rt_call_direct_aot", ft);
                            auto helper_throwing = module.getOrInsertFunction(
                                    "qore_rt_call_direct_aot_throwing", ft);
                            result = emitMaybeInvoke(helper, helper_throwing,
                                    {aot_ctx_arg,
                                     llvm::ConstantInt::get(i32_type, slot),
                                     args_array,
                                     llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                    module, llvm_func, inst);
                        }
                    }
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
                    if (has_arg_cleanups) {
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_call_static_method_direct_aot_consume_args", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_call_static_method_direct_aot_consume_args_throwing", ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                                 args_array, arg_cleanups,
                                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                module, llvm_func, inst);
                    } else {
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_call_static_method_direct_aot", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_call_static_method_direct_aot_throwing", ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), args_array,
                                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                module, llvm_func, inst);
                    }
                } else if (inv->invoke_opcode == QoreIROpcode::CallClosureDirect) {
                    // CallClosureDirect invoke: call closure/callref directly
                    // operands[0] = closure value, operands[1..] = args
                    auto* ref = getVal(inv->operands[0].id, error);
                    if (!ref) { return false; }
                    llvm::Value* ref_boxed = boxValue(ref, inv->operands[0].id);

                    // Compute number of arguments from operand count (known at compile time)
                    int closure_nargs = static_cast<int>(inst->operands.size()) - 1;
                    bool has_closure_arg_cleanups = false;
                    llvm::Value* closure_arg_cleanups = buildArgCleanupArray(inst, 1,
                            llvm_func, closure_nargs, has_closure_arg_cleanups);

                    if (closure_nargs == 0) {
                        // Fast path for 0-argument closure calls (no QoreListNode allocation)
                        auto helper = module.getOrInsertFunction("qore_rt_call_closure_0",
                                llvm::FunctionType::get(i64_type,
                                    {i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {ref_boxed, xsink_arg});
                    } else if (closure_nargs == 1 && !has_closure_arg_cleanups) {
                        // Fast path for 1-argument closure calls (stack-allocated QoreListNode)
                        auto* arg_val = getVal(inst->operands[1].id, error);
                        if (!arg_val) { return false; }
                        llvm::Value* arg_boxed = boxValue(arg_val, inst->operands[1].id);
                        auto helper = module.getOrInsertFunction("qore_rt_call_closure_1",
                                llvm::FunctionType::get(i64_type,
                                    {i64_type, i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {ref_boxed, arg_boxed, xsink_arg});
                    } else {
                        // Standard path for >1 arguments, or any argument list with
                        // caller-owned temporary cleanups to consume after params bind.
                        llvm::Value* closure_args_array;
                        if (!buildArgsArray(inst, 1, llvm_func, closure_args_array, closure_nargs, error)) {
                            return false;
                        }

                        if (has_closure_arg_cleanups) {
                            auto ft_cf = llvm::FunctionType::get(i64_type,
                                    {i64_type, ptr_type, ptr_type, i32_type, ptr_type}, false);
                            auto helper = module.getOrInsertFunction(
                                    "qore_rt_call_closure_fast_consume_args", ft_cf);
                            auto helper_throwing = module.getOrInsertFunction(
                                    "qore_rt_call_closure_fast_consume_args_throwing", ft_cf);
                            result = emitMaybeInvoke(helper, helper_throwing,
                                    {ref_boxed, closure_args_array, closure_arg_cleanups,
                                     llvm::ConstantInt::get(i32_type, closure_nargs), xsink_arg},
                                    module, llvm_func, inst);
                        } else {
                            auto ft_cf = llvm::FunctionType::get(i64_type,
                                    {i64_type, ptr_type, i32_type, ptr_type}, false);
                            auto helper = module.getOrInsertFunction(
                                    "qore_rt_call_closure_fast", ft_cf);
                            auto helper_throwing = module.getOrInsertFunction(
                                    "qore_rt_call_closure_fast_throwing", ft_cf);
                            result = emitMaybeInvoke(helper, helper_throwing,
                                    {ref_boxed, closure_args_array,
                                     llvm::ConstantInt::get(i32_type, closure_nargs), xsink_arg},
                                    module, llvm_func, inst);
                        }
                    }
                } else if (aot_mode) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    if (has_arg_cleanups) {
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_call_with_args_aot_consume_args", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_call_with_args_aot_consume_args_throwing", ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                                 args_array, arg_cleanups,
                                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                module, llvm_func, inst);
                    } else {
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction("qore_rt_call_with_args_aot", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_call_with_args_aot_throwing", ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), args_array,
                                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                module, llvm_func, inst);
                    }
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
                // Use _for_call variant for dot-eval-only bases (see LoadSelfMember case)
                const char* helper_name = dot_eval_only_bases.count(inst->result.id)
                        ? "qore_rt_load_self_member_for_call"
                        : "qore_rt_load_self_member";
                auto helper = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                result = builder->CreateCall(helper, {name_const, xsink_arg});
                // LoadSelfMember doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::NewObject) {
                // NewObject invoke: build NaN-boxed arg array from operands.
                // AOT mode: use slot-based qc/variant lookup via call_targets.
                // JIT mode: extract qc/variant from inv->expr metadata and bake.
                int nargs = static_cast<int>(inv->operands.size());
                llvm::Value* args_array;
                if (nargs > 0) {
                    llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                            llvm_func->getEntryBlock().begin());
                    args_array = ab.CreateAlloca(i64_type,
                            llvm::ConstantInt::get(i32_type, nargs), "new_obj_args");
                    for (int i = 0; i < nargs; ++i) {
                        auto* arg_val = getVal(inv->operands[i].id, error);
                        if (!arg_val) { return false; }
                        llvm::Value* boxed = boxValue(arg_val, inv->operands[i].id);
                        llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                                llvm::ConstantInt::get(i32_type, i));
                        builder->CreateStore(boxed, gep);
                    }
                } else {
                    args_array = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, 0), ptr_type);
                }
                llvm::Value* nargs_val = llvm::ConstantInt::get(i32_type, nargs);
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction("qore_rt_new_object_nb_aot", ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_new_object_nb_aot_throwing", ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                             args_array, nargs_val, xsink_arg},
                            module, llvm_func, inst);
                } else {
                    const QoreClass* qc = nullptr;
                    const AbstractQoreFunctionVariant* variant = nullptr;
                    if (auto* new_obj = dynamic_cast<const NewObjectCallNode*>(
                            inv->expr.getInternalNode())) {
                        qc = new_obj->getClass();
                        variant = new_obj->getVariant();
                    } else if (auto* scoped_obj = dynamic_cast<const ScopedObjectCallNode*>(
                            inv->expr.getInternalNode())) {
                        qc = scoped_obj->oc;
                        variant = scoped_obj->getVariant();
                    } else if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(
                            inv->expr.getInternalNode())) {
                        qc = QoreTypeInfo::getUniqueReturnClass(vrn->getTypeInfo());
                        variant = vrn->getVariant();
                    }
                    assert(qc);
                    llvm::Value* qc_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(qc));
                    llvm::Value* qc_as_ptr = builder->CreateIntToPtr(qc_ptr, ptr_type);
                    llvm::Value* variant_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(variant));
                    llvm::Value* variant_as_ptr = builder->CreateIntToPtr(variant_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_new_object_nb",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type, ptr_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper,
                            {qc_as_ptr, variant_as_ptr, args_array, nargs_val, xsink_arg});
                }
                // Constructor can modify locals through side effects
                reloadAllLocalsFromRuntime(module, llvm_func);

            } else if (inv->invoke_opcode == QoreIROpcode::LoadStaticVar) {
                // LoadStaticVar invoke: resolve by class path/name in AOT, direct pointer in JIT
                if (aot_mode) {
                    const auto* static_var = dynamic_cast<const StaticClassVarRefNode*>(
                            inv->expr.getInternalNode());
                    if (!static_var) {
                        error = "AOT LoadStaticVar requires StaticClassVarRefNode metadata";
                        return false;
                    }
                    llvm::Value* class_path = builder->CreateGlobalStringPtr(
                            static_var->qc.getNamespacePath(), "static_var_class_path");
                    llvm::Value* var_name = builder->CreateGlobalStringPtr(
                            static_var->str, "static_var_name");
                    auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction("qore_rt_load_static_var_by_path", ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_load_static_var_by_path_throwing", ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {class_path, var_name, xsink_arg}, module, llvm_func, inst);
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
                    auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction("qore_rt_load_constant_aot", ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_load_constant_aot_throwing", ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                            module, llvm_func, inst);
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
                    auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction("qore_rt_create_closure_aot", ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_create_closure_aot_throwing", ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                            module, llvm_func, inst);
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
                    auto* node = inv->expr.getInternalNode();
                    auto* mcr = dynamic_cast<const LocalMethodCallReferenceNode*>(node);
                    const QoreMethod* method = mcr ? mcr->getMethod() : nullptr;
                    const QoreClass* qc = method ? method->getClass() : nullptr;
                    if (method && !method->isStatic() && qc) {
                        llvm::Value* class_path = builder->CreateGlobalStringPtr(
                                qc->getNamespacePath(), "local_method_call_ref_class_path");
                        llvm::Value* method_name = builder->CreateGlobalStringPtr(
                                method->getName(), "local_method_call_ref_method_name");
                        auto cr_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_create_local_method_call_ref_aot", cr_ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_create_local_method_call_ref_aot_throwing", cr_ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {class_path, method_name, xsink_arg}, module, llvm_func, inst);
                    } else {
                        auto* scr = dynamic_cast<const LocalStaticMethodCallReferenceNode*>(node);
                        method = scr ? scr->getMethod() : nullptr;
                        qc = method ? method->getClass() : nullptr;
                        if (method && method->isStatic() && qc) {
                            llvm::Value* class_path = builder->CreateGlobalStringPtr(
                                    qc->getNamespacePath(), "static_call_ref_class_path");
                            llvm::Value* method_name = builder->CreateGlobalStringPtr(
                                    method->getName(), "static_call_ref_method_name");
                            auto cr_ft = llvm::FunctionType::get(i64_type,
                                    {ptr_type, ptr_type, ptr_type}, false);
                            auto helper = module.getOrInsertFunction(
                                    "qore_rt_create_static_method_call_ref_aot", cr_ft);
                            auto helper_throwing = module.getOrInsertFunction(
                                    "qore_rt_create_static_method_call_ref_aot_throwing", cr_ft);
                            result = emitMaybeInvoke(helper, helper_throwing,
                                    {class_path, method_name, xsink_arg}, module, llvm_func, inst);
                        } else if (auto* fcr = dynamic_cast<const LocalFunctionCallReferenceNode*>(node)) {
                            QoreFunction* func = fcr->getFunction();
                            if (!func) {
                                error = "unsupported AOT call reference lowering: function call reference has no "
                                    "function metadata";
                                return false;
                            }
                            llvm::Value* function_name = builder->CreateGlobalStringPtr(
                                    func->getName(), "function_call_ref_name");
                            auto cr_ft = llvm::FunctionType::get(i64_type,
                                    {ptr_type, ptr_type}, false);
                            auto helper = module.getOrInsertFunction(
                                    "qore_rt_create_function_call_ref_aot", cr_ft);
                            auto helper_throwing = module.getOrInsertFunction(
                                    "qore_rt_create_function_call_ref_aot_throwing", cr_ft);
                            result = emitMaybeInvoke(helper, helper_throwing,
                                    {function_name, xsink_arg}, module, llvm_func, inst);
                        } else {
                            error = "unsupported AOT call reference lowering: only resolved method, static method, "
                                "and function call references are native";
                            return false;
                        }
                    }
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
                    auto* node = inv->expr.getInternalNode();
                    if (auto* smr = dynamic_cast<const ParseSelfMethodReferenceNode*>(node)) {
                        llvm::Value* method_name = builder->CreateGlobalStringPtr(
                                smr->getMethodName(), "self_method_ref_name");
                        auto mr_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_create_self_method_ref_aot", mr_ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_create_self_method_ref_aot_throwing", mr_ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {method_name, xsink_arg}, module, llvm_func, inst);
                    } else if (auto* omr = dynamic_cast<const ParseObjectMethodReferenceNode*>(node)) {
                        if (inv->operands.empty()) {
                            error = "unsupported AOT object method reference lowering: missing object operand";
                            return false;
                        }
                        llvm::Value* obj_val = getVal(inv->operands[0].id, error);
                        if (!obj_val) {
                            return false;
                        }
                        llvm::Value* obj_boxed = boxValue(obj_val, inv->operands[0].id);
                        llvm::Value* method_name = builder->CreateGlobalStringPtr(
                                omr->getMethodName(), "object_method_ref_name");
                        auto mr_ft = llvm::FunctionType::get(i64_type,
                                {i64_type, ptr_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_create_object_method_ref_aot", mr_ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_create_object_method_ref_aot_throwing", mr_ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {obj_boxed, method_name, xsink_arg}, module, llvm_func, inst);
                    } else {
                        error = "unsupported AOT method reference lowering: unsupported method reference node";
                        return false;
                    }
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
                    // Try native AOT lowering: resolve local slot at compile time
                    bool native_lowered = false;
                    if (auto* prn = dynamic_cast<const ParseReferenceNode*>(
                            inv->expr.getInternalNode())) {
                        const QoreValue& lv_expr = prn->getLVExp();
                        if (lv_expr.getType() == NT_VARREF) {
                            auto* vrn = lv_expr.get<VarRefNode>();
                            if (vrn->getType() == VT_LOCAL || vrn->getType() == VT_LOCAL_TS) {
                                int32_t local_slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                                    reinterpret_cast<const void*>(vrn->ref.id));
                                auto helper = module.getOrInsertFunction(
                                    "qore_rt_create_local_ref_aot",
                                    llvm::FunctionType::get(i64_type,
                                        {ptr_type, i32_type, ptr_type}, false));
                                result = builder->CreateCall(helper, {aot_ctx_arg,
                                    llvm::ConstantInt::get(i32_type, local_slot), xsink_arg});
                                native_lowered = true;
                            }
                        }
                        // Complex hash member access: \member{key} with pre-evaluated key operand
                        if (!native_lowered && !inv->operands.empty() && lv_expr.hasNode()) {
                            auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(
                                lv_expr.getInternalNode());
                            if (hd) {
                                // Extract member name from the left side (SelfVarrefNode)
                                const QoreValue& left = hd->getLeft();
                                if (left.hasNode() && left.getType() == NT_SELF_VARREF) {
                                    auto* svn = left.get<SelfVarrefNode>();
                                    const char* member_name = svn->str;
                                    auto* key_val = getVal(inv->operands[0].id, error);
                                    if (key_val) {
                                        llvm::Value* key_boxed = boxValue(key_val, inv->operands[0].id);
                                        llvm::Value* name_ptr = builder->CreateGlobalStringPtr(member_name);
                                        llvm::Value* type_ptr = builder->CreateGlobalStringPtr(
                                            prn->getTypeInfo() ? QoreTypeInfo::getPath(prn->getTypeInfo()) : "");
                                        auto helper = module.getOrInsertFunction(
                                            "qore_rt_create_member_hash_ref_aot",
                                            llvm::FunctionType::get(i64_type,
                                                {ptr_type, i64_type, ptr_type, ptr_type}, false));
                                        result = builder->CreateCall(helper,
                                            {name_ptr, key_boxed, type_ptr, xsink_arg});
                                        native_lowered = true;
                                    }
                                }
                            }
                        }
                    }
                    if (!native_lowered) {
                        QoreValue expr_val = inv->expr;
                        uint64_t expr_bits;
                        std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction("qore_rt_create_parse_ref_aot", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_create_parse_ref_aot_throwing", ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                                module, llvm_func, inst);
                    }
                } else {
                    const auto* parse_ref = dynamic_cast<const ParseReferenceNode*>(
                            inv->expr.getInternalNode());
                    if (parse_ref) {
                        llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(parse_ref));
                        llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                        auto helper = module.getOrInsertFunction("qore_rt_create_parse_ref",
                                llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {node_as_ptr, xsink_arg});
                    } else {
                        QoreValue expr_val = inv->expr;
                        uint64_t bits;
                        std::memcpy(&bits, &expr_val, sizeof(bits));
                        llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, bits);
                        auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                                llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {expr_const, xsink_arg});
                    }
                }
                // CreateParseRef may access locals via lvalue resolution
                reloadAllLocalsFromRuntime(module, llvm_func);

            } else if (inv->invoke_opcode == QoreIROpcode::NewHashDecl
                    || inv->invoke_opcode == QoreIROpcode::NewComplexHash
                    || inv->invoke_opcode == QoreIROpcode::NewComplexList) {
                // Typed container construction invoke
                if (aot_mode) {
                    return setAotExpressionFallbackError(error, inst,
                            "typed container construction invoke has no native lowering");
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

            } else if (inv->invoke_opcode == QoreIROpcode::NewHashDeclFromHash) {
                // Hashdecl construction from pre-lowered hash operand
                // Extract TypedHashDecl and runtime_check from the VarRefNewObjectNode in expr
                auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(
                        inv->expr.getInternalNode());
                assert(vrn);
                const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(
                        vrn->getTypeInfo());
                assert(hd);
                auto* hash_val = getVal(inv->operands[0].id, error);
                if (!hash_val) { return false; }
                llvm::Value* hash_boxed = boxValue(hash_val, inv->operands[0].id);
                llvm::Value* rtcheck = llvm::ConstantInt::get(i32_type,
                        vrn->getRuntimeCheck() ? 1 : 0);
                if (aot_mode) {
                    // AOT: resolve hashdecl by namespace path at runtime
                    std::string hd_path = hd->getNamespacePath();
                    llvm::Value* hd_path_str = builder->CreateGlobalString(hd_path, "hd_path");
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_new_hash_decl_from_hash_by_path",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i64_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper,
                            {hd_path_str, hash_boxed, rtcheck, xsink_arg});
                } else {
                    // JIT: direct pointer is valid within the same process
                    llvm::Value* hd_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(hd));
                    llvm::Value* hd_as_ptr = builder->CreateIntToPtr(hd_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_new_hash_decl_from_hash",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i64_type, i32_type, ptr_type}, false));
                    result = builder->CreateCall(helper,
                            {hd_as_ptr, hash_boxed, rtcheck, xsink_arg});
                }
                // NewHashDeclFromHash doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::ListPush) {
                // ListPush invoke: native list push with pre-evaluated operands
                auto* list = getVal(inv->operands[0].id, error);
                if (!list) { return false; }
                auto* val = getVal(inv->operands[1].id, error);
                if (!val) { return false; }
                llvm::Value* list_boxed = boxValue(list, inv->operands[0].id);
                llvm::Value* val_boxed = boxValue(val, inv->operands[1].id);
                llvm::Value* type_arg = aot_mode
                    ? getTypePathArg(inv->element_type) : getTypeInfoPointerArg(inv->element_type);
                const char* helper_name = aot_mode ? "qore_rt_list_push_by_type_path" : "qore_rt_list_push_typed";
                auto push_fn = module.getOrInsertFunction(helper_name,
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type, ptr_type}, false));
                result = builder->CreateCall(push_fn, {list_boxed, val_boxed, type_arg, xsink_arg});
                // ListPush doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::InstanceOfBool) {
                // InstanceOfBool invoke: native instanceof with pre-evaluated operand
                auto* val = getVal(inv->operands[0].id, error);
                if (!val) { return false; }
                llvm::Value* val_boxed = boxValue(val, inv->operands[0].id);
                auto* io_node = static_cast<const QoreInstanceOfOperatorNode*>(
                    inv->expr.getInternalNode());
                if (!aot_mode) {
                    const QoreTypeInfo* ti = io_node->getInstanceTypeInfo();
                    llvm::Value* ti_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(ti));
                    llvm::Value* ti_as_ptr = builder->CreateIntToPtr(ti_ptr, ptr_type);
                    auto helper = module.getOrInsertFunction("qore_rt_instanceof",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    result = builder->CreateCall(helper, {val_boxed, ti_as_ptr});
                } else {
                    const QoreTypeInfo* ti = io_node->getInstanceTypeInfo();
                    std::string type_path = ti ? QoreTypeInfo::getPath(ti) : "";
                    llvm::Value* type_path_ptr = builder->CreateGlobalStringPtr(type_path);
                    auto helper = module.getOrInsertFunction("qore_rt_instanceof_by_type_path",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
                    result = builder->CreateCall(helper,
                        {val_boxed, type_path_ptr, xsink_arg});
                }
                // instanceof doesn't modify locals — no reload needed

            } else if (inv->invoke_opcode == QoreIROpcode::BackgroundInt
                    && !inv->operands.empty()
                    && tryEmitDecomposedBackground(inv->expr, inv->operands,
                        module, llvm_func, inst, /*throwing_ok*/false, &result)) {
                // Decomposed background call path — one of the five supported
                // inner-call shapes.  Falls through to AST eval below when the
                // shape is unsupported (AOT mode on non-self shapes, etc.).
                // Background call may modify closure-captured vars but not locals

            } else if (inv->invoke_opcode == QoreIROpcode::CastList
                    || inv->invoke_opcode == QoreIROpcode::CastHash
                    || inv->invoke_opcode == QoreIROpcode::CastComplexHash
                    || inv->invoke_opcode == QoreIROpcode::CastObject
                    || inv->invoke_opcode == QoreIROpcode::CastEnum
                    || inv->invoke_opcode == QoreIROpcode::CastAny) {
                // Cast invoke: native cast with pre-evaluated inner value (operand[0])
                auto* inner_val = getVal(inv->operands[0].id, error);
                if (!inner_val) { return false; }
                llvm::Value* inner_boxed = boxValue(inner_val, inv->operands[0].id);
                if (aot_mode) {
                    // AOT: extract type path at compile time, resolve at runtime
                    auto* cast_node = dynamic_cast<const QoreCastOperatorNode*>(
                        inv->expr.getInternalNode());
                    if (cast_node) {
                        const QoreTypeInfo* ti = cast_node->getCastTypeInfo();
                        std::string type_path = ti ? QoreTypeInfo::getPath(ti)
                            : (inv->invoke_opcode == QoreIROpcode::CastList ? "list" : "");
                        llvm::Value* type_path_ptr = builder->CreateGlobalStringPtr(type_path);
                        llvm::Value* or_nothing_val = llvm::ConstantInt::get(i64_type,
                                cast_node->isOrNothing() ? 1 : 0);
                        auto helper = module.getOrInsertFunction("qore_rt_cast_by_type_path_aot",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, i64_type, ptr_type, i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper,
                                {aot_ctx_arg, inner_boxed, type_path_ptr, or_nothing_val, xsink_arg});
                    } else {
                        // Fallback to expr slot
                        QoreValue expr_val = inv->expr;
                        uint64_t expr_bits;
                        std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                        auto helper = module.getOrInsertFunction("qore_rt_cast_with_inner_aot",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, i32_type, i64_type, ptr_type}, false));
                        result = builder->CreateCall(helper, {aot_ctx_arg,
                                llvm::ConstantInt::get(i32_type, slot), inner_boxed, xsink_arg});
                    }
                } else {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
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
                if (LocalVar* root_local = findLvalueRootLocalVar(lv);
                        root_local && root_local->closureUse()) {
                    const void* local_key = reinterpret_cast<const void*>(root_local);
                    if (aot_mode) {
                        auto inst_helper = module.getOrInsertFunction(
                                "qore_rt_instantiate_local_aot",
                                llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false));
                        int32_t local_slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(local_key);
                        builder->CreateCall(inst_helper, {aot_ctx_arg,
                                llvm::ConstantInt::get(i32_type, local_slot)});
                    } else {
                        auto inst_helper = module.getOrInsertFunction(
                                "qore_rt_instantiate_local",
                                llvm::FunctionType::get(void_type, {ptr_type}, false));
                        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                                reinterpret_cast<uint64_t>(root_local));
                        builder->CreateCall(inst_helper, {builder->CreateIntToPtr(var_ptr, ptr_type)});
                    }
                }
                // Clear reload tracker for lvalue target local
                {
                    const void* local_key = findLvalueRootLocalKey(lv);
                    if (local_key) {
                        clearLocalCachedValue(local_key, module, llvm_func,
                            LocalCacheClearMode::DuplicateRefsOnly);
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
                // RefForeachInit invoke: initialize reference foreach state.
                // See the non-invoke RefForeachInit path (case block below)
                // for the full rationale — the callee needs the raw
                // ParseReferenceNode* bits, not an evaluation of the node.
                llvm::Value* parse_ref_bits_val;
                if (aot_mode) {
                    QoreValue expr_val = inv->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto aot_helper = module.getOrInsertFunction("qore_rt_get_expr_bits_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type}, false));
                    llvm::Value* bits_val = builder->CreateCall(aot_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot)});
                    parse_ref_bits_val = bits_val;
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
                // Index may flow through a nan-boxed PHI (loop counter); unbox
                // to plain i64 before passing to the runtime helper.
                llvm::Value* index_unboxed = ensureIntTypeInline(index_val, inv->operands[1].id);
                auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_get_entry",
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {state_val, index_unboxed, xsink_arg});
                // Result is nanboxed — handled by common tail below

            } else if (inv->invoke_opcode == QoreIROpcode::RangeSliceAny
                    || inv->invoke_opcode == QoreIROpcode::RangeSliceInt
                    || inv->invoke_opcode == QoreIROpcode::RangeSliceFloat) {
                // RangeSlice invoke: ternary op with pre-evaluated operands
                auto* first = getVal(inv->operands[0].id, error);
                if (!first) { return false; }
                auto* second = getVal(inv->operands[1].id, error);
                if (!second) { return false; }
                auto* third = getVal(inv->operands[2].id, error);
                if (!third) { return false; }
                llvm::Value* first_boxed = boxValue(first, inv->operands[0].id);
                llvm::Value* second_boxed = boxValue(second, inv->operands[1].id);
                llvm::Value* third_boxed = boxValue(third, inv->operands[2].id);
                llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                        static_cast<int>(inv->invoke_opcode));
                auto helper = module.getOrInsertFunction("qore_rt_ternary_op",
                        llvm::FunctionType::get(i64_type,
                            {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {opcode_val, first_boxed, second_boxed, third_boxed, xsink_arg});

            } else {
                if (aot_mode) {
                    return setAotExpressionFallbackError(error, inst,
                            "invoke opcode has no native lowering");
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
                    syncLocalsToRuntimeForHandlers(module);
                    llvm::Value* native_slot_cache = beginNativeHandlerSlotCache(module);
                    builder->CreateCall(helper, {saved_count, xsink_arg});
                    endNativeHandlerSlotCache(module, native_slot_cache);
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
            emitPendingSsaCleanup(module);
            emitLocalUninstantiation(module);
            builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            return true;
        }

        // === Rethrow ===
        case QoreIROpcode::Rethrow: {
            const auto* rethrow_inst = static_cast<const QoreIRThrowInstruction*>(inst);
            int catch_cleanup_start = 0;
            if (!rethrow_inst->synthetic) {
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
                catch_cleanup_start = 1;
            }
            // Synthetic rethrows (foreach/context cleanup) only propagate the
            // exception already in xsink.  They must not read td->catchException,
            // but still need to release any catch scopes recorded on the IR node.
            if (rethrow_inst->catch_depth > catch_cleanup_start) {
                auto catch_end_helper = module.getOrInsertFunction("qore_rt_catch_end",
                        llvm::FunctionType::get(void_type, {ptr_type}, false));
                for (int i = catch_cleanup_start; i < rethrow_inst->catch_depth; ++i) {
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
            emitPendingSsaCleanup(module);
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

            if (!inst->operands.empty()
                    || inst->opcode == QoreIROpcode::Call
                    || inst->opcode == QoreIROpcode::CallMethod
                    || inst->opcode == QoreIROpcode::CallStatic) {
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
                bool has_arg_cleanups = false;
                llvm::Value* arg_cleanups = buildArgCleanupArray(inst, arg_start,
                        llvm_func, nargs, has_arg_cleanups);

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
                    if (has_arg_cleanups) {
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_call_with_args_aot_consume_args", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_call_with_args_aot_consume_args_throwing", ft);
                        call_result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                                 args_array, arg_cleanups,
                                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                module, llvm_func, inst);
                    } else {
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction("qore_rt_call_with_args_aot", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_call_with_args_aot_throwing", ft);
                        call_result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), args_array,
                                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                module, llvm_func, inst);
                    }
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto helper = module.getOrInsertFunction("qore_rt_call_with_args",
                            llvm::FunctionType::get(i64_type,
                                {i64_type, ptr_type, i32_type, ptr_type}, false));
                    call_result = builder->CreateCall(helper, {expr_const, args_array,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                }
            } else {
                if (aot_mode) {
                    return setAotExpressionFallbackError(error, inst,
                            "call instruction has no lowered operands");
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
            releaseDotEvalBaseIfCurrentUseIsLast(inst, module);
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
            bool has_arg_cleanups = false;
            llvm::Value* arg_cleanups = buildArgCleanupArray(inst, 0, llvm_func,
                    nargs, has_arg_cleanups);

            llvm::Value* call_result;
            if (aot_mode && direct_inst->is_self_recursive
                    && isFastFunctionCallEligible(direct_inst->variant)
                    && !aot_self_recursive_fast_entry.empty()) {
                // AOT Approach B self-recursive: direct LLVM call to fast entry
                // Completely bypasses the runtime helper — no TLS param instantiation,
                // no ThreadFrameBoundaryHelper, no execJITWithDeopt overhead.
                // check_stack is at the fast entry's function prologue.
                llvm::Function* fast_fn = module.getFunction(
                        aot_self_recursive_fast_entry);
                assert(fast_fn && "AOT self-recursive fast entry must be in module");

                unsigned fast_num_params = fast_fn->arg_size() - 2;  // minus ctx and xsink
                std::vector<llvm::Value*> call_args;
                for (unsigned i = 0; i < fast_num_params; ++i) {
                    if (i < boxed_args.size()) {
                        call_args.push_back(boxed_args[i]);
                    } else {
                        call_args.push_back(
                                llvm::ConstantInt::get(i64_type, VAL_NOTHING));
                    }
                }
                call_args.push_back(aot_ctx_arg);
                call_args.push_back(xsink_arg);
                call_result = builder->CreateCall(fast_fn, call_args);
                if (has_arg_cleanups) {
                    auto clear_helper = module.getOrInsertFunction(
                            "qore_rt_clear_arg_cleanups",
                            llvm::FunctionType::get(void_type,
                                {ptr_type, i32_type, ptr_type}, false));
                    builder->CreateCall(clear_helper, {arg_cleanups,
                            llvm::ConstantInt::get(i32_type, nargs), xsink_arg});
                }
            } else if (aot_mode) {
                // AOT: resolve expression slot for this call
                QoreValue expr_val = direct_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);

                if (direct_inst->is_self_recursive
                        && isFastFunctionCallEligible(direct_inst->variant)) {
                    // Self-recursive AOT (no fast entry available): lightweight helper
                    if (has_arg_cleanups) {
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_call_self_recursive_aot_consume_args",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, ptr_type, i32_type, ptr_type, ptr_type,
                                     i32_type, ptr_type},
                                    false));
                        call_result = builder->CreateCall(helper,
                                {llvm_func, aot_ctx_arg,
                                 llvm::ConstantInt::get(i32_type, slot), args_array,
                                 arg_cleanups, llvm::ConstantInt::get(i32_type, nargs),
                                 xsink_arg});
                    } else {
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_call_self_recursive_aot",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, ptr_type, i32_type, ptr_type,
                                     i32_type, ptr_type},
                                    false));
                        call_result = builder->CreateCall(helper, {llvm_func,
                                aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                                args_array, llvm::ConstantInt::get(i32_type, nargs),
                                xsink_arg});
                    }
                } else {
                    // Generic AOT call path
                    if (has_arg_cleanups) {
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_call_direct_aot_consume_args", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_call_direct_aot_consume_args_throwing", ft);
                        call_result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg,
                                 llvm::ConstantInt::get(i32_type, slot), args_array,
                                 arg_cleanups, llvm::ConstantInt::get(i32_type, nargs),
                                 xsink_arg},
                                module, llvm_func, inst);
                    } else {
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction("qore_rt_call_direct_aot", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_call_direct_aot_throwing", ft);
                        call_result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg,
                                 llvm::ConstantInt::get(i32_type, slot), args_array,
                                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                module, llvm_func, inst);
                    }
                }
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
                    if (has_arg_cleanups) {
                        auto clear_helper = module.getOrInsertFunction(
                                "qore_rt_clear_arg_cleanups",
                                llvm::FunctionType::get(void_type,
                                    {ptr_type, i32_type, ptr_type}, false));
                        builder->CreateCall(clear_helper, {arg_cleanups,
                                llvm::ConstantInt::get(i32_type, nargs),
                                xsink_arg});
                    }
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
            } else if (direct_inst->is_self_recursive && direct_inst->variant != nullptr
                    && isFastFunctionCallEligible(direct_inst->variant)) {
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
            bool has_arg_cleanups = false;
            llvm::Value* arg_cleanups = buildArgCleanupArray(inst, 0, llvm_func,
                    nargs, has_arg_cleanups);

            llvm::Value* call_result;
            if (aot_mode && direct_inst->expr) {
                // AOT mode: use expression slot to look up the class and method at runtime
                QoreValue expr_val = direct_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);

                // Use fast path if variant is compile-time known and eligible
                const char* helper_name = "qore_rt_call_method_direct_aot";
                const char* helper_name_throwing = "qore_rt_call_method_direct_aot_throwing";
                if (direct_inst->variant) {
                    const UserVariantBase* uvb = direct_inst->variant->getUserVariantBase();
                    if (uvb && isFastMethodCallEligible(direct_inst->variant)) {
                        helper_name = "qore_rt_call_method_fast_aot";
                        helper_name_throwing = "qore_rt_call_method_fast_aot_throwing";
                    }
                }

                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(helper_name, ft);
                auto helper_throwing = module.getOrInsertFunction(helper_name_throwing, ft);
                call_result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), args_array,
                         llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                        module, llvm_func, inst);
            } else {
                // JIT mode: use pointer constants (valid within same process)
                // Pass method pointer directly to runtime helper as a pointer constant
                llvm::Value* method_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(direct_inst->method)),
                        ptr_type);

                // Use fast call path if variant is eligible (no default args, not synchronized)
                if (direct_inst->variant) {
                    const UserVariantBase* uvb = direct_inst->variant->getUserVariantBase();
                    if (uvb && isFastMethodCallEligible(direct_inst->variant)) {
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
            bool has_arg_cleanups = false;
            llvm::Value* arg_cleanups = buildArgCleanupArray(inst, 0, llvm_func,
                    nargs, has_arg_cleanups);

            llvm::Value* call_result;
            if (aot_mode && invoke_inst->expr) {
                // AOT mode: use expression slot to look up the class and method at runtime
                QoreValue expr_val = invoke_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);

                // Use fast path if variant is compile-time known and eligible
                const char* helper_name = "qore_rt_call_method_direct_aot";
                const char* helper_name_throwing = "qore_rt_call_method_direct_aot_throwing";
                if (invoke_inst->variant) {
                    const UserVariantBase* uvb = invoke_inst->variant->getUserVariantBase();
                    if (uvb && isFastMethodCallEligible(invoke_inst->variant)) {
                        helper_name = "qore_rt_call_method_fast_aot";
                        helper_name_throwing = "qore_rt_call_method_fast_aot_throwing";
                    }
                }

                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(helper_name, ft);
                auto helper_throwing = module.getOrInsertFunction(helper_name_throwing, ft);
                call_result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), args_array,
                         llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                        module, llvm_func, inst);
            } else {
                // JIT mode: use pointer constants (valid within same process)
                // Pass method pointer directly to runtime helper as a pointer constant
                llvm::Value* method_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(invoke_inst->method)),
                        ptr_type);

                // Use fast call path if variant is eligible (no default args, not synchronized)
                if (invoke_inst->variant) {
                    const UserVariantBase* uvb = invoke_inst->variant->getUserVariantBase();
                    if (uvb && isFastMethodCallEligible(invoke_inst->variant)) {
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
            bool has_arg_cleanups = false;
            llvm::Value* arg_cleanups = buildArgCleanupArray(inst, 0, llvm_func,
                    nargs, has_arg_cleanups);

            llvm::Value* call_result;
            if (aot_mode) {
                // AOT mode: always use expression slot — embedded pointer optimization is only valid
                // in JIT mode (same process). In AOT, compile-time pointers are invalid at runtime.
                QoreValue expr_val = direct_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);

                const char* helper_name = "qore_rt_call_static_method_direct_aot";
                const char* helper_name_throwing =
                        "qore_rt_call_static_method_direct_aot_throwing";
                if (has_arg_cleanups) {
                    helper_name = "qore_rt_call_static_method_direct_aot_consume_args";
                    helper_name_throwing =
                            "qore_rt_call_static_method_direct_aot_consume_args_throwing";
                } else if (direct_inst->variant) {
                    const UserVariantBase* uvb = direct_inst->variant->getUserVariantBase();
                    if (uvb && isFastMethodCallEligible(direct_inst->variant)) {
                        helper_name = "qore_rt_call_static_method_fast_aot";
                        helper_name_throwing = "qore_rt_call_static_method_fast_aot_throwing";
                    }
                }

                auto ft = has_arg_cleanups
                        ? llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, ptr_type, i32_type, ptr_type}, false)
                        : llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(helper_name, ft);
                auto helper_throwing = module.getOrInsertFunction(helper_name_throwing, ft);
                if (has_arg_cleanups) {
                    call_result = emitMaybeInvoke(helper, helper_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                             args_array, arg_cleanups,
                             llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                            module, llvm_func, inst);
                } else {
                    call_result = emitMaybeInvoke(helper, helper_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), args_array,
                             llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                            module, llvm_func, inst);
                }
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

            // Check for optimizable pseudo-methods (no arguments, known fast paths)
            if (direct_inst->pseudo && nargs == 0) {
                const char* method_name = direct_inst->method->getName();

                if (!strcmp(method_name, "typeCode")) {
                    // Fast: typeCode() returns NaN-boxed int
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_typeCode",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "size")) {
                    // Fast: size() returns NaN-boxed int
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_size",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "strlen")) {
                    // Fast: strlen() returns byte length as a NaN-boxed int
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_size",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "length")) {
                    // Fast: length() returns character count as a NaN-boxed int
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_length",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "empty")) {
                    // Fast: empty() returns NaN-boxed bool
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_empty",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "val")) {
                    // Fast: val() returns NaN-boxed bool (opposite of empty)
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_val",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "type")) {
                    // Fast: type() returns NaN-boxed string
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_type",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else {
                    // Unsupported pseudo-method, use generic dispatch
                    if (aot_mode) {
                        QoreValue expr_val = direct_inst->expr;
                        uint64_t expr_bits;
                        std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i64_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_dot_eval_pseudo_method_direct_aot", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_dot_eval_pseudo_method_direct_aot_throwing", ft);
                        call_result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                                 base_boxed, args_array,
                                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                module, llvm_func, inst);
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
                        auto helper = module.getOrInsertFunction("qore_rt_dot_eval_pseudo_method_direct",
                                llvm::FunctionType::get(i64_type,
                                    {i64_type, ptr_type, ptr_type, ptr_type, ptr_type, i32_type, ptr_type},
                                    false));
                        call_result = builder->CreateCall(helper, {base_boxed, method_ptr, qc_ptr,
                                variant_ptr, args_array, llvm::ConstantInt::get(i32_type, nargs),
                                xsink_arg});
                    }
                }
            } else if (aot_mode) {
                QoreValue expr_val = direct_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                const char* helper_name = direct_inst->pseudo
                        ? "qore_rt_dot_eval_pseudo_method_direct_aot"
                        : "qore_rt_dot_eval_method_direct_aot";
                const char* helper_name_throwing = direct_inst->pseudo
                        ? "qore_rt_dot_eval_pseudo_method_direct_aot_throwing"
                        : "qore_rt_dot_eval_method_direct_aot_throwing";
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, i64_type, ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(helper_name, ft);
                auto helper_throwing = module.getOrInsertFunction(helper_name_throwing, ft);
                call_result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                         base_boxed, args_array,
                         llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                        module, llvm_func, inst);
            } else if (direct_inst->method && direct_inst->qc) {
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
            } else {
                // Unresolved method (abstract/dynamic): use name-based dispatch with
                // pre-evaluated args via the stored method name
                const char* method_name = direct_inst->fallback_method_name
                    ? direct_inst->fallback_method_name : "";
                llvm::Value* name_ptr = builder->CreateGlobalStringPtr(method_name);
                auto helper = module.getOrInsertFunction(
                        "qore_rt_dot_eval_method_by_name",
                        llvm::FunctionType::get(i64_type,
                            {i64_type, ptr_type, ptr_type, i32_type, ptr_type}, false));
                call_result = builder->CreateCall(helper, {base_boxed, name_ptr, args_array,
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

            // Check for optimizable pseudo-methods (no arguments, known fast paths)
            if (invoke_inst->pseudo && nargs == 0) {
                const char* method_name = invoke_inst->method->getName();

                if (!strcmp(method_name, "typeCode")) {
                    // Fast: typeCode() returns NaN-boxed int
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_typeCode",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "size")) {
                    // Fast: size() returns NaN-boxed int
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_size",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "strlen")) {
                    // Fast: strlen() returns byte length as a NaN-boxed int
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_size",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "length")) {
                    // Fast: length() returns character count as a NaN-boxed int
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_length",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "empty")) {
                    // Fast: empty() returns NaN-boxed bool
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_empty",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "val")) {
                    // Fast: val() returns NaN-boxed bool (opposite of empty)
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_val",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else if (!strcmp(method_name, "type")) {
                    // Fast: type() returns NaN-boxed string
                    auto helper = module.getOrInsertFunction("qore_rt_pseudo_type",
                            llvm::FunctionType::get(i64_type, {i64_type}, false));
                    call_result = builder->CreateCall(helper, {base_boxed});
                } else {
                    // Unsupported pseudo-method, use generic dispatch
                    if (aot_mode) {
                        QoreValue expr_val = invoke_inst->expr;
                        uint64_t expr_bits;
                        std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                        auto ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i64_type, ptr_type, i32_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_dot_eval_pseudo_method_direct_aot", ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_dot_eval_pseudo_method_direct_aot_throwing", ft);
                        call_result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                                 base_boxed, args_array,
                                 llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                                module, llvm_func, inst);
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
                        auto helper = module.getOrInsertFunction("qore_rt_dot_eval_pseudo_method_direct",
                                llvm::FunctionType::get(i64_type,
                                    {i64_type, ptr_type, ptr_type, ptr_type, ptr_type, i32_type, ptr_type},
                                    false));
                        call_result = builder->CreateCall(helper, {base_boxed, method_ptr, qc_ptr,
                                variant_ptr, args_array, llvm::ConstantInt::get(i32_type, nargs),
                                xsink_arg});
                    }
                }
            } else if (aot_mode) {
                QoreValue expr_val = invoke_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                const char* helper_name = invoke_inst->pseudo
                        ? "qore_rt_dot_eval_pseudo_method_direct_aot"
                        : "qore_rt_dot_eval_method_direct_aot";
                const char* helper_name_throwing = invoke_inst->pseudo
                        ? "qore_rt_dot_eval_pseudo_method_direct_aot_throwing"
                        : "qore_rt_dot_eval_method_direct_aot_throwing";
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, i64_type, ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(helper_name, ft);
                auto helper_throwing = module.getOrInsertFunction(helper_name_throwing, ft);
                call_result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                         base_boxed, args_array,
                         llvm::ConstantInt::get(i32_type, nargs), xsink_arg},
                        module, llvm_func, inst);
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
            releaseDotEvalBaseIfCurrentUseIsLast(inst, module);

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
            if (val->getType() != i64_type || !nanboxed_values.count(inst->operands[0].id)) {
                values[inst->result.id] = llvm::ConstantInt::get(i1_type, 0);
                return true;
            }
            auto helper = module.getOrInsertFunction("qore_rt_is_null_or_nothing",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {val});
            values[inst->result.id] = builder->CreateICmpNE(result, llvm::ConstantInt::get(i64_type, 0));
            return true;
        }

        // === ConstDate ===
        case QoreIROpcode::ConstDate: {
            const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
            llvm::Value* us_val = llvm::ConstantInt::get(i64_type, cinst->constant.date_microseconds);
            llvm::Value* rel_val = llvm::ConstantInt::get(i64_type,
                    cinst->constant.date_is_relative ? 1 : 0);
            std::string zone_name = (!cinst->constant.date_is_relative && cinst->constant.date_zone_set)
                ? getLLVMDateZoneName(cinst->constant.date_zone)
                : "";
            llvm::Value* zone_ptr = builder->CreateGlobalStringPtr(zone_name);
            auto helper = module.getOrInsertFunction("qore_rt_make_date_ex",
                    llvm::FunctionType::get(i64_type,
                        {i64_type, i64_type, ptr_type, i64_type, i64_type, i64_type, i64_type, i64_type, i64_type,
                            i64_type},
                        false));
            llvm::Value* result = builder->CreateCall(helper, {
                us_val,
                rel_val,
                zone_ptr,
                llvm::ConstantInt::get(i64_type, cinst->constant.rel_years),
                llvm::ConstantInt::get(i64_type, cinst->constant.rel_months),
                llvm::ConstantInt::get(i64_type, cinst->constant.rel_days),
                llvm::ConstantInt::get(i64_type, cinst->constant.rel_hours),
                llvm::ConstantInt::get(i64_type, cinst->constant.rel_minutes),
                llvm::ConstantInt::get(i64_type, cinst->constant.rel_seconds),
                llvm::ConstantInt::get(i64_type, cinst->constant.rel_us),
            });
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
                auto helper = module.getOrInsertFunction("qore_rt_get_expr_bits_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type}, false));
                result = builder->CreateCall(helper,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot)});
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {lhs_boxed, rhs_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                    inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                    inst, val_boxed, llvm_func, module);
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
                    inst, val_boxed, llvm_func, module);
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
                const char* helper_throwing_name = (inst->opcode == QoreIROpcode::LoadGlobal)
                        ? "qore_rt_load_global_aot_throwing"
                        : "qore_rt_load_thread_local_aot_throwing";
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getGlobalSlot(
                        reinterpret_cast<const void*>(vinst->var));
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(helper_name, ft);
                auto helper_throwing = module.getOrInsertFunction(helper_throwing_name, ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else {
                const char* helper_name = (inst->opcode == QoreIROpcode::LoadGlobal)
                        ? "qore_rt_load_global" : "qore_rt_load_thread_local";
                const char* helper_throwing_name = (inst->opcode == QoreIROpcode::LoadGlobal)
                        ? "qore_rt_load_global_throwing"
                        : "qore_rt_load_thread_local_throwing";
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(vinst->var));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                auto ft = llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(helper_name, ft);
                auto helper_throwing = module.getOrInsertFunction(helper_throwing_name, ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {var_as_ptr, xsink_arg}, module, llvm_func, inst);
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
            const char* helper_name = aot_mode
                ? "qore_rt_create_empty_list_by_type_path" : "qore_rt_create_empty_list_typed";
            auto helper = module.getOrInsertFunction(helper_name,
                    llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
            llvm::Value* type_arg = aot_mode
                ? getTypePathArg(inst->element_type) : getTypeInfoPointerArg(inst->element_type);
            llvm::Value* result = builder->CreateCall(helper, {type_arg, xsink_arg});
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
            llvm::Value* type_arg = aot_mode
                ? getTypePathArg(inst->element_type) : getTypeInfoPointerArg(inst->element_type);
            const char* helper_name = aot_mode ? "qore_rt_list_push_by_type_path" : "qore_rt_list_push_typed";
            const char* throwing_name = aot_mode
                ? "qore_rt_list_push_by_type_path_throwing" : "qore_rt_list_push_typed_throwing";
            auto push_ft = llvm::FunctionType::get(i64_type,
                    {i64_type, i64_type, ptr_type, ptr_type}, false);
            auto push_fn = module.getOrInsertFunction(helper_name, push_ft);
            auto push_fn_throwing = module.getOrInsertFunction(throwing_name, push_ft);
            llvm::Value* result = emitMaybeInvoke(push_fn, push_fn_throwing,
                    {list_boxed, val_boxed, type_arg, xsink_arg}, module, llvm_func, inst);
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
            const char* helper_name = aot_mode
                ? "qore_rt_create_sized_list_by_type_path" : "qore_rt_create_sized_list_typed";
            auto helper = module.getOrInsertFunction(helper_name,
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type, ptr_type}, false));
            llvm::Value* type_arg = aot_mode
                ? getTypePathArg(inst->element_type) : getTypeInfoPointerArg(inst->element_type);
            llvm::Value* result = builder->CreateCall(helper, {cap_int, type_arg, xsink_arg});
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
            bool has_arg_cleanups = false;
            llvm::Value* arg_cleanups = buildArgCleanupArray(inst, 1, llvm_func,
                    nargs, has_arg_cleanups);

            llvm::Value* nargs_val = llvm::ConstantInt::get(i32_type, nargs);
            llvm::Value* result;
            if (has_arg_cleanups) {
                auto ft = llvm::FunctionType::get(i64_type,
                        {i64_type, ptr_type, ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(
                        "qore_rt_call_closure_fast_consume_args", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_call_closure_fast_consume_args_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {ref_boxed, args_array, arg_cleanups, nargs_val, xsink_arg},
                        module, llvm_func, inst);
            } else {
                auto ft = llvm::FunctionType::get(i64_type,
                        {i64_type, ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_call_closure_fast", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_call_closure_fast_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {ref_boxed, args_array, nargs_val, xsink_arg},
                        module, llvm_func, inst);
            }
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
            auto clear_cleanup_alloca = [&](llvm::Value* cleanup) {
                if (!cleanup) {
                    return;
                }
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup);
                builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
            };
            auto clear_consumed_operand_cleanup = [&](uint32_t value_id) {
                auto uses_it = operand_remaining_uses.find(value_id);
                if (uses_it != operand_remaining_uses.end() && uses_it->second > 1) {
                    return;
                }
                auto alloca_it = invoke_alloca_map.find(value_id);
                if (alloca_it == invoke_alloca_map.end()) {
                    return;
                }
                llvm::Value* ca = alloca_it->second;
                if (!ca) {
                    ca = promoteSsaEntryToAlloca(value_id, module, llvm_func);
                }
                clear_cleanup_alloca(ca);
            };
            auto track_owned_value_cleanup = [&](llvm::Value* owned, const char* name) {
                llvm::Function* func = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* entry = &func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                llvm::AllocaInst* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr, name);
                alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup);
                builder->CreateStore(owned, cleanup);
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
                registerInvokeCleanupAlloca(cleanup);
            };
            if (vinst->weak) {
                auto weak_fn = module.getOrInsertFunction("qore_rt_make_weak_value",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                llvm::Value* weak_boxed = builder->CreateCall(weak_fn, {val_boxed, xsink_arg});
                track_owned_value_cleanup(weak_boxed, "weak_var_cleanup");
                clear_consumed_operand_cleanup(inst->operands[0].id);
                val_boxed = weak_boxed;
            }
            if (aot_mode) {
                const char* helper_name = (inst->opcode == QoreIROpcode::StoreGlobal)
                        ? "qore_rt_store_global_aot" : "qore_rt_store_thread_local_aot";
                const char* helper_throwing_name = (inst->opcode == QoreIROpcode::StoreGlobal)
                        ? "qore_rt_store_global_aot_throwing"
                        : "qore_rt_store_thread_local_aot_throwing";
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getGlobalSlot(
                        reinterpret_cast<const void*>(vinst->var));
                auto ft = llvm::FunctionType::get(void_type,
                        {ptr_type, i32_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(helper_name, ft);
                auto helper_throwing = module.getOrInsertFunction(helper_throwing_name, ft);
                emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                         val_boxed, xsink_arg},
                        module, llvm_func, inst);
            } else {
                const char* helper_name = (inst->opcode == QoreIROpcode::StoreGlobal)
                        ? "qore_rt_store_global" : "qore_rt_store_thread_local";
                const char* helper_throwing_name = (inst->opcode == QoreIROpcode::StoreGlobal)
                        ? "qore_rt_store_global_throwing"
                        : "qore_rt_store_thread_local_throwing";
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(vinst->var));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                auto ft = llvm::FunctionType::get(void_type,
                        {ptr_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(helper_name, ft);
                auto helper_throwing = module.getOrInsertFunction(helper_throwing_name, ft);
                emitMaybeInvoke(helper, helper_throwing,
                        {var_as_ptr, val_boxed, xsink_arg},
                        module, llvm_func, inst);
            }
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::LoadClosure: {
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            llvm::Value* result;
            if (aot_mode) {
                const void* key = reinterpret_cast<const void*>(linst->local);
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                if (linst->local && linst->local->closureUse()
                        && aot_body_locals.count(key)) {
                    auto inst_helper = module.getOrInsertFunction(
                            "qore_rt_instantiate_local_aot",
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false));
                    builder->CreateCall(inst_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot)});
                }
                auto lc_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_load_closure_aot", lc_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_load_closure_aot_throwing", lc_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                auto ll_ft = llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_load_local", ll_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_load_local_throwing", ll_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {var_as_ptr, xsink_arg}, module, llvm_func, inst);
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
            auto clear_cleanup_alloca = [&](llvm::Value* cleanup) {
                if (!cleanup) {
                    return;
                }
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup);
                builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
            };
            auto clear_consumed_operand_cleanup = [&](uint32_t value_id) {
                auto uses_it = operand_remaining_uses.find(value_id);
                if (uses_it != operand_remaining_uses.end() && uses_it->second > 1) {
                    return;
                }
                auto alloca_it = invoke_alloca_map.find(value_id);
                if (alloca_it == invoke_alloca_map.end()) {
                    return;
                }
                llvm::Value* ca = alloca_it->second;
                if (!ca) {
                    ca = promoteSsaEntryToAlloca(value_id, module, llvm_func);
                }
                clear_cleanup_alloca(ca);
            };
            auto track_owned_value_cleanup = [&](llvm::Value* owned, const char* name) -> llvm::AllocaInst* {
                llvm::Function* func = builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* entry = &func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                llvm::AllocaInst* cleanup = alloca_builder.CreateAlloca(i64_type, nullptr, name);
                alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), cleanup);
                llvm::Value* old_val = builder->CreateLoad(i64_type, cleanup);
                builder->CreateStore(owned, cleanup);
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
                registerInvokeCleanupAlloca(cleanup);
                return cleanup;
            };
            {
                auto key = reinterpret_cast<const void*>(linst->local);
                if (linst->weak) {
                    auto weak_fn = module.getOrInsertFunction("qore_rt_make_weak_value",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    llvm::Value* weak_boxed = builder->CreateCall(weak_fn, {val_boxed, xsink_arg});
                    llvm::AllocaInst* weak_cleanup = track_owned_value_cleanup(
                            weak_boxed, "weak_closure_cleanup");
                    local_cleanup_allocas[key].push_back(weak_cleanup);
                    clear_consumed_operand_cleanup(inst->operands[0].id);
                    val_boxed = weak_boxed;
                } else {
                    // Track the input value's cleanup alloca for this closure variable
                    // so UninstantiateLocal can release it at block scope exit.
                    // Without this, the original object ref in the invoke result alloca
                    // persists until function exit, preventing deterministic destruction.
                    auto alloca_it = invoke_alloca_map.find(inst->operands[0].id);
                    if (alloca_it != invoke_alloca_map.end()) {
                        llvm::Value* ca = alloca_it->second;
                        if (!ca) {
                            ca = promoteSsaEntryToAlloca(inst->operands[0].id, module,
                                    llvm_func);
                        }
                        if (ca) {
                            local_cleanup_allocas[key].push_back(ca);
                        }
                    }
                }
            }
            if (aot_mode) {
                const void* key = reinterpret_cast<const void*>(linst->local);
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                if (linst->local && linst->local->closureUse()
                        && aot_body_locals.count(key)) {
                    auto inst_helper = module.getOrInsertFunction(
                            "qore_rt_instantiate_local_aot",
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false));
                    builder->CreateCall(inst_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot)});
                }
                auto sc_ft = llvm::FunctionType::get(void_type,
                        {ptr_type, i32_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_store_closure_aot", sc_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_store_closure_aot_throwing", sc_ft);
                emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                         val_boxed, xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                auto al_ft = llvm::FunctionType::get(void_type,
                        {ptr_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_assign_local", al_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_assign_local_throwing", al_ft);
                emitMaybeInvoke(helper, helper_throwing,
                        {var_as_ptr, val_boxed, xsink_arg},
                        module, llvm_func, inst);
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
            auto hka_ft = llvm::FunctionType::get(i64_type,
                    {i64_type, ptr_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_hash_key_access", hka_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_hash_key_access_throwing", hka_ft);
            values[inst->result.id] = emitMaybeInvoke(helper, helper_throwing,
                    {base_boxed, key_const, xsink_arg}, module, llvm_func, inst);
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
            llvm::Value* result = builder->CreateCall(helper, {base_boxed, key_const});
            values[inst->result.id] = result;
            // Result is now nanboxed (may be NOTHING for missing keys)
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::ListIndexAccess: {
            std::string err;
            auto* list_v = getVal(inst->operands[0].id, err);
            auto* idx_v = getVal(inst->operands[1].id, err);
            if (!list_v || !idx_v) {
                error = err;
                return false;
            }
            llvm::Value* list_boxed = boxValue(list_v, inst->operands[0].id);
            // Index must be native i64
            llvm::Value* idx_int;
            if (nanboxed_values.count(inst->operands[1].id)) {
                auto unbox_fn = module.getOrInsertFunction("qore_rt_get_int64",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                idx_int = builder->CreateCall(unbox_fn, {idx_v, xsink_arg});
            } else {
                idx_int = idx_v;
            }
            auto lia_ft = llvm::FunctionType::get(i64_type,
                    {i64_type, i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_list_index_access", lia_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_list_index_access_throwing", lia_ft);
            values[inst->result.id] = emitMaybeInvoke(helper, helper_throwing,
                    {list_boxed, idx_int, xsink_arg}, module, llvm_func, inst);
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(values[inst->result.id], inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
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
            const void* container_key = hks_inst->container_lv
                    ? reinterpret_cast<const void*>(hks_inst->container_lv)
                    : (hks_inst->container
                        ? reinterpret_cast<const void*>(hks_inst->container->ref.id)
                        : nullptr);
            if (!container_key) {
                error = "HashKeyStore: missing container local";
                return false;
            }
            clearLocalCachedValue(container_key, module, llvm_func,
                LocalCacheClearMode::DuplicateRefsOnly);
            clearLocalReloadTracker(container_key, module, llvm_func);

            llvm::Value* call_result;
            if (aot_mode) {
                // AOT: pass ctx + pre-registered local slot index for COW update
                uint32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(container_key);
                llvm::Value* slot_val = llvm::ConstantInt::get(i32_type, slot);
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, i64_type, ptr_type, i64_type, ptr_type}, false);
                auto fn = module.getOrInsertFunction("qore_rt_hash_key_store_cow_aot", ft);
                auto fn_throwing = module.getOrInsertFunction(
                        "qore_rt_hash_key_store_cow_aot_throwing", ft);
                call_result = emitMaybeInvoke(fn, fn_throwing,
                        {aot_ctx_arg, slot_val, hash_boxed, key_c, val_boxed, xsink_arg},
                        module, llvm_func, inst);
            } else {
                // JIT: pass LocalVar* directly
                auto var_int = llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(container_key));
                auto* var_ptr = builder->CreateIntToPtr(var_int, ptr_type);
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i64_type, ptr_type, i64_type, ptr_type}, false);
                auto fn = module.getOrInsertFunction("qore_rt_hash_key_store_cow", ft);
                auto fn_throwing = module.getOrInsertFunction(
                        "qore_rt_hash_key_store_cow_throwing", ft);
                call_result = emitMaybeInvoke(fn, fn_throwing,
                        {var_ptr, hash_boxed, key_c, val_boxed, xsink_arg},
                        module, llvm_func, inst);
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
            // Reload the hash from the runtime after potential COW.
            // reloadLocalFromRuntime updates the alloca cache so subsequent
            // LoadLocal(h) reads the post-COW hash (not a stale cached value).
            // This is critical for both JIT and AOT modes.
            reloadLocalFromRuntime(container_key, module, llvm_func);

            return true;
        }
        case QoreIROpcode::HashKeyStoreDynamic: {
            const auto* hksd_inst = static_cast<const QoreIRHashKeyStoreDynamicInstruction*>(inst);
            std::string err;
            auto* hash_v = getVal(inst->operands[0].id, err);
            auto* val_v  = getVal(inst->operands[1].id, err);
            auto* key_v  = getVal(inst->operands[2].id, err);
            if (!hash_v || !val_v || !key_v) {
                error = err;
                return false;
            }
            llvm::Value* hash_boxed = boxValue(hash_v, inst->operands[0].id);
            llvm::Value* val_boxed  = boxValue(val_v,  inst->operands[1].id);
            llvm::Value* key_boxed  = boxValue(key_v,  inst->operands[2].id);
            const void* container_key = hksd_inst->container_lv
                    ? reinterpret_cast<const void*>(hksd_inst->container_lv)
                    : (hksd_inst->container
                        ? reinterpret_cast<const void*>(hksd_inst->container->ref.id)
                        : nullptr);
            if (!container_key) {
                error = "HashKeyStoreDynamic: missing container local";
                return false;
            }
            clearLocalCachedValue(container_key, module, llvm_func,
                LocalCacheClearMode::DuplicateRefsOnly);
            clearLocalReloadTracker(container_key, module, llvm_func);

            llvm::Value* call_result;
            if (aot_mode) {
                uint32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(container_key);
                llvm::Value* slot_val = llvm::ConstantInt::get(i32_type, slot);
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, i64_type, i64_type, i64_type, ptr_type}, false);
                auto fn = module.getOrInsertFunction("qore_rt_hash_key_store_dynamic_cow_aot", ft);
                auto fn_throwing = module.getOrInsertFunction(
                        "qore_rt_hash_key_store_dynamic_cow_aot_throwing", ft);
                call_result = emitMaybeInvoke(fn, fn_throwing,
                        {aot_ctx_arg, slot_val, hash_boxed, key_boxed, val_boxed, xsink_arg},
                        module, llvm_func, inst);
            } else {
                auto var_int = llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(container_key));
                auto* var_ptr = builder->CreateIntToPtr(var_int, ptr_type);
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i64_type, i64_type, i64_type, ptr_type}, false);
                auto fn = module.getOrInsertFunction("qore_rt_hash_key_store_dynamic_cow", ft);
                auto fn_throwing = module.getOrInsertFunction(
                        "qore_rt_hash_key_store_dynamic_cow_throwing", ft);
                call_result = emitMaybeInvoke(fn, fn_throwing,
                        {var_ptr, hash_boxed, key_boxed, val_boxed, xsink_arg},
                        module, llvm_func, inst);
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = call_result;
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(call_result, inst->result.id, llvm_func);
            }
            emitExceptionCheck(module, llvm_func, inst);
            reloadLocalFromRuntime(container_key, module, llvm_func);
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
            const void* container_key = lis_inst->container
                    ? reinterpret_cast<const void*>(lis_inst->container->ref.id)
                    : nullptr;
            if (!container_key) {
                error = "ListIndexStore: missing container local";
                return false;
            }
            clearLocalCachedValue(container_key, module, llvm_func,
                LocalCacheClearMode::DuplicateRefsOnly);
            clearLocalReloadTracker(container_key, module, llvm_func);

            llvm::Value* call_result;
            if (aot_mode) {
                // AOT: pass ctx + pre-registered local slot index for COW update
                uint32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(container_key);
                llvm::Value* slot_val = llvm::ConstantInt::get(i32_type, slot);
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, i64_type, i64_type, i64_type, ptr_type}, false);
                auto fn = module.getOrInsertFunction("qore_rt_list_index_store_cow_aot", ft);
                auto fn_throwing = module.getOrInsertFunction(
                        "qore_rt_list_index_store_cow_aot_throwing", ft);
                call_result = emitMaybeInvoke(fn, fn_throwing,
                        {aot_ctx_arg, slot_val, list_boxed, index_i64, val_boxed, xsink_arg},
                        module, llvm_func, inst);
            } else {
                // JIT: pass LocalVar* directly
                auto var_int = llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(container_key));
                auto* var_ptr = builder->CreateIntToPtr(var_int, ptr_type);
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i64_type, i64_type, i64_type, ptr_type}, false);
                auto fn = module.getOrInsertFunction("qore_rt_list_index_store_cow", ft);
                auto fn_throwing = module.getOrInsertFunction(
                        "qore_rt_list_index_store_cow_throwing", ft);
                call_result = emitMaybeInvoke(fn, fn_throwing,
                        {var_ptr, list_boxed, index_i64, val_boxed, xsink_arg},
                        module, llvm_func, inst);
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = call_result;
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(call_result, inst->result.id, llvm_func);
            }
            emitExceptionCheck(module, llvm_func, inst);

            // CRITICAL: After ListIndexStore COW, reload the list from the LocalVar.
            // reloadLocalFromRuntime updates the alloca cache so subsequent
            // LoadLocal(l) reads the post-COW list (not a stale cached value).
            // This is critical for both JIT and AOT modes.
            reloadLocalFromRuntime(container_key, module, llvm_func);

            return true;
        }
        case QoreIROpcode::LoadSelfMember: {
            const auto* sminst = static_cast<const QoreIRSelfMemberInstruction*>(inst);
            llvm::Constant* name_const = builder->CreateGlobalString(sminst->member_name,
                    "self_member_name");
            // Use _for_call variant when this result is only used as a DotEval base.
            // This returns raw values (including WeakReferenceNode) without evaluating
            // them, avoiding temporary strong references from weak member dereferences.
            // All DotEval helpers handle NT_WEAKREF correctly.
            const char* helper_name = dot_eval_only_bases.count(inst->result.id)
                    ? "qore_rt_load_self_member_for_call"
                    : "qore_rt_load_self_member";
            auto helper = module.getOrInsertFunction(helper_name,
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
            // Build NaN-boxed arg array from pre-computed IR operand values.
            int nargs = static_cast<int>(noinst->operands.size());
            llvm::Value* args_array;
            if (nargs > 0) {
                llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                        llvm_func->getEntryBlock().begin());
                args_array = ab.CreateAlloca(i64_type,
                        llvm::ConstantInt::get(i32_type, nargs), "new_obj_args");
                for (int i = 0; i < nargs; ++i) {
                    auto* arg_val = getVal(noinst->operands[i].id, error);
                    if (!arg_val) { return false; }
                    llvm::Value* boxed = boxValue(arg_val, noinst->operands[i].id);
                    llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                            llvm::ConstantInt::get(i32_type, i));
                    builder->CreateStore(boxed, gep);
                }
            } else {
                args_array = builder->CreateIntToPtr(
                    llvm::ConstantInt::get(i64_type, 0), ptr_type);
            }
            llvm::Value* nargs_val = llvm::ConstantInt::get(i32_type, nargs);
            llvm::Value* result;
            if (aot_mode) {
                // AOT mode: load qc/variant from ctx->call_targets[slot] at runtime.
                // Slot key is the bits of the metadata expr.
                QoreValue expr_val = noinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_new_object_nb_aot", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_new_object_nb_aot_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                         args_array, nargs_val, xsink_arg},
                        module, llvm_func, inst);
            } else {
                // JIT mode: bake qc/variant as constants (valid within the same program).
                llvm::Value* qc_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(noinst->qc));
                llvm::Value* qc_as_ptr = builder->CreateIntToPtr(qc_ptr, ptr_type);
                llvm::Value* variant_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(noinst->variant));
                llvm::Value* variant_as_ptr = builder->CreateIntToPtr(variant_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_new_object_nb",
                        llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, ptr_type, i32_type, ptr_type}, false));
                result = builder->CreateCall(helper,
                        {qc_as_ptr, variant_as_ptr, args_array, nargs_val, xsink_arg});
            }
            // Constructor runs constructor body; can modify locals through ref params
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
                const auto* static_var = dynamic_cast<const StaticClassVarRefNode*>(
                        svinst->expr.getInternalNode());
                if (!static_var) {
                    error = "AOT LoadStaticVar requires StaticClassVarRefNode metadata";
                    return false;
                }
                llvm::Value* class_path = builder->CreateGlobalStringPtr(
                        static_var->qc.getNamespacePath(), "static_var_class_path");
                llvm::Value* var_name = builder->CreateGlobalStringPtr(
                        static_var->str, "static_var_name");
                auto lsv_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_load_static_var_by_path", lsv_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_load_static_var_by_path_throwing", lsv_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {class_path, var_name, xsink_arg}, module, llvm_func, inst);
            } else {
                // JIT: pass QoreVarInfo* and var_name directly
                llvm::Value* vi_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(svinst->vi));
                llvm::Value* vi_as_ptr = builder->CreateIntToPtr(vi_ptr, ptr_type);
                llvm::Constant* name_const = builder->CreateGlobalString(svinst->var_name,
                        "static_var_name");
                auto lsv_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_load_static_var", lsv_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_load_static_var_throwing", lsv_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {vi_as_ptr, name_const, xsink_arg},
                        module, llvm_func, inst);
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
                auto lc_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_load_constant_aot", lc_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_load_constant_aot_throwing", lc_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else if (lcinst->node) {
                llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(lcinst->node));
                llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                auto lc_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_load_constant", lc_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_load_constant_throwing", lc_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {node_as_ptr, xsink_arg}, module, llvm_func, inst);
            } else {
                // No RuntimeConstantRefNode — expr holds the value directly (e.g., number,
                // binary, object, or container constants). Return expr.refSelf().
                QoreValue expr_val = lcinst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                auto helper = module.getOrInsertFunction("qore_rt_load_constant_value",
                        llvm::FunctionType::get(i64_type, {i64_type}, false));
                result = builder->CreateCall(helper,
                        {llvm::ConstantInt::get(i64_type, expr_bits)});
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
                auto cc_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_create_closure_aot", cc_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_create_closure_aot_throwing", cc_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* cn_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(ccinst->closure_node));
                llvm::Value* cn_as_ptr = builder->CreateIntToPtr(cn_ptr, ptr_type);
                auto cc_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_create_closure", cc_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_create_closure_throwing", cc_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {cn_as_ptr, xsink_arg}, module, llvm_func, inst);
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
                auto* node = crinst->expr.getInternalNode();
                auto* mcr = dynamic_cast<const LocalMethodCallReferenceNode*>(node);
                const QoreMethod* method = mcr ? mcr->getMethod() : nullptr;
                const QoreClass* qc = method ? method->getClass() : nullptr;
                if (method && !method->isStatic() && qc) {
                    llvm::Value* class_path = builder->CreateGlobalStringPtr(
                            qc->getNamespacePath(), "local_method_call_ref_class_path");
                    llvm::Value* method_name = builder->CreateGlobalStringPtr(
                            method->getName(), "local_method_call_ref_method_name");
                    auto cr_ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_create_local_method_call_ref_aot", cr_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_create_local_method_call_ref_aot_throwing", cr_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {class_path, method_name, xsink_arg}, module, llvm_func, inst);
                } else {
                    auto* scr = dynamic_cast<const LocalStaticMethodCallReferenceNode*>(node);
                    method = scr ? scr->getMethod() : nullptr;
                    qc = method ? method->getClass() : nullptr;
                    if (method && method->isStatic() && qc) {
                        llvm::Value* class_path = builder->CreateGlobalStringPtr(
                                qc->getNamespacePath(), "static_call_ref_class_path");
                        llvm::Value* method_name = builder->CreateGlobalStringPtr(
                                method->getName(), "static_call_ref_method_name");
                        auto cr_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_create_static_method_call_ref_aot", cr_ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_create_static_method_call_ref_aot_throwing", cr_ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {class_path, method_name, xsink_arg}, module, llvm_func, inst);
                    } else if (auto* fcr = dynamic_cast<const LocalFunctionCallReferenceNode*>(node)) {
                        QoreFunction* func = fcr->getFunction();
                        if (!func) {
                            error = "unsupported AOT call reference lowering: function call reference has no "
                                "function metadata";
                            return false;
                        }
                        llvm::Value* function_name = builder->CreateGlobalStringPtr(
                                func->getName(), "function_call_ref_name");
                        auto cr_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_create_function_call_ref_aot", cr_ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_create_function_call_ref_aot_throwing", cr_ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {function_name, xsink_arg}, module, llvm_func, inst);
                    } else {
                        error = "unsupported AOT call reference lowering: only resolved method, static method, "
                            "and function call references are native";
                        return false;
                    }
                }
            } else {
                QoreValue expr_val = crinst->expr;
                uint64_t bits;
                std::memcpy(&bits, &expr_val, sizeof(bits));
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, bits);
                auto cr_ft = llvm::FunctionType::get(i64_type,
                        {i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_create_call_ref", cr_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_create_call_ref_throwing", cr_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {expr_const, xsink_arg}, module, llvm_func, inst);
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
                auto* node = mrinst->expr.getInternalNode();
                if (auto* smr = dynamic_cast<const ParseSelfMethodReferenceNode*>(node)) {
                    llvm::Value* method_name = builder->CreateGlobalStringPtr(
                            smr->getMethodName(), "self_method_ref_name");
                    auto mr_ft = llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_create_self_method_ref_aot", mr_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_create_self_method_ref_aot_throwing", mr_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {method_name, xsink_arg}, module, llvm_func, inst);
                } else if (auto* omr = dynamic_cast<const ParseObjectMethodReferenceNode*>(node)) {
                    if (mrinst->operands.empty()) {
                        error = "unsupported AOT object method reference lowering: missing object operand";
                        return false;
                    }
                    llvm::Value* obj_val = getVal(mrinst->operands[0].id, error);
                    if (!obj_val) {
                        return false;
                    }
                    llvm::Value* obj_boxed = boxValue(obj_val, mrinst->operands[0].id);
                    llvm::Value* method_name = builder->CreateGlobalStringPtr(
                            omr->getMethodName(), "object_method_ref_name");
                    auto mr_ft = llvm::FunctionType::get(i64_type,
                            {i64_type, ptr_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_create_object_method_ref_aot", mr_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_create_object_method_ref_aot_throwing", mr_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {obj_boxed, method_name, xsink_arg}, module, llvm_func, inst);
                } else {
                    error = "unsupported AOT method reference lowering: unsupported method reference node";
                    return false;
                }
            } else {
                QoreValue expr_val = mrinst->expr;
                uint64_t bits;
                std::memcpy(&bits, &expr_val, sizeof(bits));
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, bits);
                auto mr_ft = llvm::FunctionType::get(i64_type,
                        {i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_create_method_ref", mr_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_create_method_ref_throwing", mr_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {expr_const, xsink_arg}, module, llvm_func, inst);
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
                // Try native AOT lowering: resolve local slot at compile time
                bool native_lowered = false;
                if (prinst->node) {
                    const QoreValue& lv_expr = prinst->node->getLVExp();
                    if (lv_expr.getType() == NT_VARREF) {
                        auto* vrn = lv_expr.get<VarRefNode>();
                        if (vrn->getType() == VT_LOCAL || vrn->getType() == VT_LOCAL_TS) {
                            int32_t local_slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(
                                reinterpret_cast<const void*>(vrn->ref.id));
                            auto helper = module.getOrInsertFunction(
                                "qore_rt_create_local_ref_aot",
                                llvm::FunctionType::get(i64_type,
                                    {ptr_type, i32_type, ptr_type}, false));
                            result = builder->CreateCall(helper, {aot_ctx_arg,
                                llvm::ConstantInt::get(i32_type, local_slot), xsink_arg});
                            native_lowered = true;
                        }
                    }
                    // Complex hash member access: \member{key} with pre-evaluated key operand
                    if (!native_lowered && !prinst->operands.empty() && lv_expr.hasNode()) {
                        auto* hd = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(
                            lv_expr.getInternalNode());
                        if (hd) {
                            const QoreValue& left = hd->getLeft();
                            if (left.hasNode() && left.getType() == NT_SELF_VARREF) {
                                auto* svn = left.get<SelfVarrefNode>();
                                const char* member_name = svn->str;
                                auto* key_val = getVal(prinst->operands[0].id, error);
                                if (key_val) {
                                    llvm::Value* key_boxed = boxValue(key_val, prinst->operands[0].id);
                                    llvm::Value* name_ptr = builder->CreateGlobalStringPtr(member_name);
                                    llvm::Value* type_ptr = builder->CreateGlobalStringPtr(
                                        prinst->node->getTypeInfo()
                                            ? QoreTypeInfo::getPath(prinst->node->getTypeInfo()) : "");
                                    auto helper = module.getOrInsertFunction(
                                        "qore_rt_create_member_hash_ref_aot",
                                        llvm::FunctionType::get(i64_type,
                                            {ptr_type, i64_type, ptr_type, ptr_type}, false));
                                    result = builder->CreateCall(helper,
                                        {name_ptr, key_boxed, type_ptr, xsink_arg});
                                    native_lowered = true;
                                }
                            }
                        }
                    }
                }
                if (!native_lowered) {
                    QoreValue expr_val = prinst->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto pr_ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction("qore_rt_create_parse_ref_aot", pr_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_create_parse_ref_aot_throwing", pr_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                            module, llvm_func, inst);
                }
            } else {
                if (prinst->node) {
                    llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(prinst->node));
                    llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                    auto pr_ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction("qore_rt_create_parse_ref", pr_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_create_parse_ref_throwing", pr_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {node_as_ptr, xsink_arg}, module, llvm_func, inst);
                } else {
                    QoreValue expr_val = prinst->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto ie_ft = llvm::FunctionType::get(i64_type,
                            {i64_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction("qore_rt_invoke_expr", ie_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_invoke_expr_throwing", ie_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {expr_const, xsink_arg}, module, llvm_func, inst);
                }
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
                auto nhd_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_new_hash_decl_aot", nhd_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_new_hash_decl_aot_throwing", nhd_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(nhdinst->node));
                llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                auto nhd_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_new_hash_decl", nhd_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_new_hash_decl_throwing", nhd_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {node_as_ptr, xsink_arg}, module, llvm_func, inst);
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
                auto nch_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_new_complex_hash_aot", nch_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_new_complex_hash_aot_throwing", nch_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(nchinst->node));
                llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                auto nch_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_new_complex_hash", nch_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_new_complex_hash_throwing", nch_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {node_as_ptr, xsink_arg}, module, llvm_func, inst);
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
                auto ncl_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_new_complex_list_aot", ncl_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_new_complex_list_aot_throwing", ncl_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* node_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(nclinst->node));
                llvm::Value* node_as_ptr = builder->CreateIntToPtr(node_ptr, ptr_type);
                auto ncl_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_new_complex_list", ncl_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_new_complex_list_throwing", ncl_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {node_as_ptr, xsink_arg}, module, llvm_func, inst);
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
                auto vc_aot_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(
                        "qore_rt_vrn_construct_aot", vc_aot_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_vrn_construct_aot_throwing", vc_aot_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* vrn_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(vrninst->vrn));
                llvm::Value* vrn_as_ptr = builder->CreateIntToPtr(vrn_ptr, ptr_type);
                auto vc_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_vrn_construct", vc_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_vrn_construct_throwing", vc_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {vrn_as_ptr, xsink_arg}, module, llvm_func, inst);
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::NewHashDeclFromHash: {
            const auto* nhdfh_inst = static_cast<const QoreIRNewHashDeclFromHashInstruction*>(inst);
            auto* hash_val = getVal(inst->operands[0].id, error);
            if (!hash_val) { return false; }
            llvm::Value* hash_boxed = boxValue(hash_val, inst->operands[0].id);
            llvm::Value* rtcheck = llvm::ConstantInt::get(i32_type,
                    nhdfh_inst->runtime_check ? 1 : 0);
            llvm::Value* result;
            if (aot_mode) {
                // AOT: resolve hashdecl by namespace path at runtime
                std::string hd_path = nhdfh_inst->hd->getNamespacePath();
                llvm::Value* hd_path_str = builder->CreateGlobalString(hd_path, "hd_path");
                auto nhdfhp_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i64_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(
                        "qore_rt_new_hash_decl_from_hash_by_path", nhdfhp_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_new_hash_decl_from_hash_by_path_throwing", nhdfhp_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {hd_path_str, hash_boxed, rtcheck, xsink_arg},
                        module, llvm_func, inst);
            } else {
                // JIT: direct pointer is valid within the same process
                llvm::Value* hd_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(nhdfh_inst->hd));
                llvm::Value* hd_as_ptr = builder->CreateIntToPtr(hd_ptr, ptr_type);
                auto nhdfh_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i64_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(
                        "qore_rt_new_hash_decl_from_hash", nhdfh_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_new_hash_decl_from_hash_throwing", nhdfh_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {hd_as_ptr, hash_boxed, rtcheck, xsink_arg},
                        module, llvm_func, inst);
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
            auto hskv_ft = llvm::FunctionType::get(void_type,
                    {i64_type, i64_type, i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_hash_set_key_value", hskv_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_hash_set_key_value_throwing", hskv_ft);
            emitMaybeInvoke(helper, helper_throwing,
                    {hash_val, key_val, value_val, xsink_arg},
                    module, llvm_func, inst);
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
            auto ft = llvm::FunctionType::get(ptr_type,
                    {i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_iterator_create_reverse", ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_iterator_create_reverse_throwing", ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {iterable_boxed, xsink_arg}, module, llvm_func, inst);
            values[inst->result.id] = result;
            // Iterator pointer is NOT nanboxed — it's an opaque ptr

            // Track active iterator for cleanup on non-normal exit
            {
                llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                        llvm_func->getEntryBlock().begin());
                llvm::AllocaInst* iter_alloca = ab.CreateAlloca(ptr_type, nullptr, "iter_cleanup");
                ab.CreateStore(llvm::ConstantPointerNull::get(
                        llvm::PointerType::get(ctx, 0)), iter_alloca);
                // A loop can exit before IteratorNext exhausts and deletes the
                // iterator. If this IteratorCreate is reached again, release the
                // previous active iterator before overwriting the cleanup slot.
                auto cleanup_helper = module.getOrInsertFunction("qore_rt_iterator_cleanup",
                        llvm::FunctionType::get(void_type, {ptr_type}, false));
                llvm::Value* old_iter_ptr = builder->CreateLoad(ptr_type, iter_alloca);
                builder->CreateStore(llvm::ConstantPointerNull::get(
                        llvm::PointerType::get(ctx, 0)), iter_alloca);
                builder->CreateCall(cleanup_helper, {old_iter_ptr});
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
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_load_aot", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_load_aot_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto ft = llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_load", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_load_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {lv_const, xsink_arg}, module, llvm_func, inst);
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
            if (LocalVar* root_local = findLvalueRootLocalVar(lvinst->lvalue);
                    root_local && root_local->closureUse()) {
                const void* local_key = reinterpret_cast<const void*>(root_local);
                if (aot_mode) {
                    auto inst_helper = module.getOrInsertFunction(
                            "qore_rt_instantiate_local_aot",
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false));
                    int32_t local_slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(local_key);
                    builder->CreateCall(inst_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, local_slot)});
                } else {
                    auto inst_helper = module.getOrInsertFunction(
                            "qore_rt_instantiate_local",
                            llvm::FunctionType::get(void_type, {ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(root_local));
                    builder->CreateCall(inst_helper, {builder->CreateIntToPtr(var_ptr, ptr_type)});
                }
            }
            // Clear the reload tracker for the lvalue target local (same pattern
            // as compound assign — prevents refcount inflation in loops).
            {
                const void* local_key = findLvalueRootLocalKey(lvinst->lvalue);
                if (local_key) {
                    clearLocalCachedValue(local_key, module, llvm_func,
                        LocalCacheClearMode::DuplicateRefsOnly);
                    clearLocalReloadTracker(local_key, module, llvm_func);
                }
            }
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(lv_bits);
                const char* fn_name = lvinst->weak
                    ? "qore_rt_lvalue_store_weak_aot" : "qore_rt_lvalue_store_aot";
                const char* fn_throwing_name = lvinst->weak
                    ? "qore_rt_lvalue_store_weak_aot_throwing"
                    : "qore_rt_lvalue_store_aot_throwing";
                auto ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i32_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(fn_name, ft);
                auto helper_throwing = module.getOrInsertFunction(fn_throwing_name, ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                         val_boxed, xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                const char* fn_name = lvinst->weak
                    ? "qore_rt_lvalue_store_weak" : "qore_rt_lvalue_store";
                const char* fn_throwing_name = lvinst->weak
                    ? "qore_rt_lvalue_store_weak_throwing"
                    : "qore_rt_lvalue_store_throwing";
                auto ft = llvm::FunctionType::get(i64_type,
                        {i64_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(fn_name, ft);
                auto helper_throwing = module.getOrInsertFunction(fn_throwing_name, ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {lv_const, val_boxed, xsink_arg},
                        module, llvm_func, inst);
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
        case QoreIROpcode::ShiftLValue: {
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
                auto ft = llvm::FunctionType::get(i64_type,
                        {i32_type, ptr_type, i32_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_unary_aot", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_unary_aot_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {opcode_val, aot_ctx_arg,
                         llvm::ConstantInt::get(i32_type, slot), xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto ft = llvm::FunctionType::get(i64_type,
                        {i32_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_unary", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_unary_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {opcode_val, lv_const, xsink_arg},
                        module, llvm_func, inst);
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
        case QoreIROpcode::UnshiftLValue: {
            // UnshiftLValue is a binary lvalue op (lvalue + right operand)
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
                auto ft = llvm::FunctionType::get(i64_type,
                        {i32_type, ptr_type, i32_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_binary_aot", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_binary_aot_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {opcode_val, aot_ctx_arg,
                         llvm::ConstantInt::get(i32_type, slot), val_boxed, xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto ft = llvm::FunctionType::get(i64_type,
                        {i32_type, i64_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_binary", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_binary_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {opcode_val, lv_const, val_boxed, xsink_arg},
                        module, llvm_func, inst);
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
                auto ft = llvm::FunctionType::get(i64_type,
                        {i32_type, ptr_type, i32_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_binary_aot", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_binary_aot_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {opcode_val, aot_ctx_arg,
                         llvm::ConstantInt::get(i32_type, slot), val_boxed, xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto ft = llvm::FunctionType::get(i64_type,
                        {i32_type, i64_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_binary", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_binary_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {opcode_val, lv_const, val_boxed, xsink_arg},
                        module, llvm_func, inst);
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
                auto ft = llvm::FunctionType::get(i64_type,
                        {i32_type, ptr_type, i32_type, i64_type, i64_type, i64_type, ptr_type},
                        false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_ternary_aot", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_ternary_aot_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {opcode_val, aot_ctx_arg,
                         llvm::ConstantInt::get(i32_type, slot),
                         first_boxed, second_boxed, third_boxed, xsink_arg},
                        module, llvm_func, inst);
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto ft = llvm::FunctionType::get(i64_type,
                        {i32_type, i64_type, i64_type, i64_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_ternary", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_lvalue_ternary_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {opcode_val, lv_const, first_boxed, second_boxed, third_boxed, xsink_arg},
                        module, llvm_func, inst);
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
            const auto* ml = static_cast<const QoreIRMakeListInstruction*>(inst);
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
            // AOT cannot embed compile-time QoreTypeInfo* values; pass a stable
            // serialized type path and resolve it in the runtime Program instead.
            llvm::Value* ti_arg;
            const char* helper_name = "qore_rt_make_list";
            const char* helper_throwing_name = "qore_rt_make_list_throwing";
            if (aot_mode && ml->typeInfo) {
                ti_arg = getTypePathArg(ml->typeInfo);
                helper_name = "qore_rt_make_list_by_type_path";
                helper_throwing_name = "qore_rt_make_list_by_type_path_throwing";
            } else {
                ti_arg = aot_mode
                    ? llvm::ConstantPointerNull::get(llvm::dyn_cast<llvm::PointerType>(ptr_type))
                    : getTypeInfoPointerArg(ml->typeInfo);
            }
            auto ml_ft = llvm::FunctionType::get(i64_type,
                    {ptr_type, llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction(helper_name, ml_ft);
            auto helper_throwing = module.getOrInsertFunction(helper_throwing_name, ml_ft);
            llvm::Value* list_result = emitMaybeInvoke(helper, helper_throwing,
                    {arr, count_val, ti_arg, xsink_arg}, module, llvm_func, inst);
            values[inst->result.id] = list_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(list_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::MakeHash: {
            const auto* mh = static_cast<const QoreIRMakeHashInstruction*>(inst);
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
            // AOT cannot embed compile-time QoreTypeInfo* values; pass a stable
            // serialized type path and resolve it in the runtime Program instead.
            llvm::Value* ti_arg;
            const char* helper_name = "qore_rt_make_hash";
            const char* helper_throwing_name = "qore_rt_make_hash_throwing";
            if (aot_mode && mh->typeInfo) {
                ti_arg = getTypePathArg(mh->typeInfo);
                helper_name = "qore_rt_make_hash_by_type_path";
                helper_throwing_name = "qore_rt_make_hash_by_type_path_throwing";
            } else {
                ti_arg = aot_mode
                    ? llvm::ConstantPointerNull::get(llvm::dyn_cast<llvm::PointerType>(ptr_type))
                    : getTypeInfoPointerArg(mh->typeInfo);
            }
            auto mh_ft = llvm::FunctionType::get(i64_type,
                    {ptr_type, llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction(helper_name, mh_ft);
            auto helper_throwing = module.getOrInsertFunction(helper_throwing_name, mh_ft);
            llvm::Value* hash_result = emitMaybeInvoke(helper, helper_throwing,
                    {arr, count_val, ti_arg, xsink_arg}, module, llvm_func, inst);
            values[inst->result.id] = hash_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(hash_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        case QoreIROpcode::MakeHashConstKeys: {
            const auto* mhck = static_cast<const QoreIRMakeHashConstKeysInstruction*>(inst);
            int count = static_cast<int>(mhck->keys.size());
            assert(count == static_cast<int>(inst->operands.size()));
            // Hoist allocas to entry block
            llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                    llvm_func->getEntryBlock().begin());
            // Keys array: const char* pointers
            llvm::Value* keys_arr = ab.CreateAlloca(ptr_type,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), count));
            // Values array: NaN-boxed uint64_t
            llvm::Value* vals_arr = ab.CreateAlloca(i64_type,
                    llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), count));
            for (int i = 0; i < count; i++) {
                // Store constant key pointer
                llvm::Value* key_str = builder->CreateGlobalString(mhck->keys[i], "hck_" + mhck->keys[i]);
                llvm::Value* key_gep = builder->CreateGEP(ptr_type, keys_arr,
                        {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), i)});
                builder->CreateStore(key_str, key_gep);
                // Store boxed value
                auto* val = getVal(inst->operands[i].id, error);
                if (!val) { return false; }
                llvm::Value* val_boxed = boxValue(val, inst->operands[i].id);
                llvm::Value* val_gep = builder->CreateGEP(i64_type, vals_arr,
                        {llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), i)});
                builder->CreateStore(val_boxed, val_gep);
            }
            // AOT cannot embed compile-time QoreTypeInfo* values; pass a stable
            // serialized type path and resolve it in the runtime Program instead.
            llvm::Value* ti_arg;
            const char* helper_name = "qore_rt_make_hash_const_keys";
            const char* helper_throwing_name = "qore_rt_make_hash_const_keys_throwing";
            if (aot_mode && mhck->typeInfo) {
                ti_arg = getTypePathArg(mhck->typeInfo);
                helper_name = "qore_rt_make_hash_const_keys_by_type_path";
                helper_throwing_name = "qore_rt_make_hash_const_keys_by_type_path_throwing";
            } else {
                ti_arg = aot_mode
                    ? llvm::ConstantPointerNull::get(llvm::dyn_cast<llvm::PointerType>(ptr_type))
                    : getTypeInfoPointerArg(mhck->typeInfo);
            }
            auto mhck_ft = llvm::FunctionType::get(i64_type,
                    {ptr_type, ptr_type, llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type},
                    false);
            auto helper = module.getOrInsertFunction(helper_name, mhck_ft);
            auto helper_throwing = module.getOrInsertFunction(helper_throwing_name, mhck_ft);
            llvm::Value* hash_result = emitMaybeInvoke(helper, helper_throwing,
                    {keys_arr, vals_arr, llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), count),
                     ti_arg, xsink_arg},
                    module, llvm_func, inst);
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
                "qore_rt_add_assign_any", inst, lhs_boxed, rhs_boxed, llvm_func, module, true);
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
                "qore_rt_sub_assign_any", inst, lhs_boxed, rhs_boxed, llvm_func, module, false);
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
                "qore_rt_mul_assign_any", inst, lhs_boxed, rhs_boxed, llvm_func, module, false);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
                inst, lhs_boxed, rhs_boxed, llvm_func, module);
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
        // All return nanboxed uint64_t (may be NOTHING for empty lists)
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
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::FusedMapFoldlSumScaleFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_float = ensureFloatType(scale, inst->operands[1].id, module);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_foldl_sum_scale_float",
                    llvm::FunctionType::get(i64_type, {i64_type, double_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_float});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
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
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::FusedMapFoldlSumSquareFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_foldl_sum_square_float",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
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
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::FusedMapFoldlProdScaleFloat: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_float = ensureFloatType(scale, inst->operands[1].id, module);
            auto helper = module.getOrInsertFunction("qore_rt_fused_map_foldl_prod_scale_float",
                    llvm::FunctionType::get(i64_type, {i64_type, double_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {list_boxed, scale_float});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
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
                    // Extract regex pattern at codegen time — avoids EXPR_TREE slot
                    QoreRegex* re = nullptr;
                    if (auto* mn = dynamic_cast<const QoreRegexMatchOperatorNode*>(
                            expr_inst->expr.getInternalNode())) {
                        re = mn->getRegex();
                    } else if (auto* nmn = dynamic_cast<const QoreRegexNMatchOperatorNode*>(
                            expr_inst->expr.getInternalNode())) {
                        re = nmn->getRegex();
                    } else if (auto* exn = dynamic_cast<const QoreRegexExtractOperatorNode*>(
                            expr_inst->expr.getInternalNode())) {
                        re = exn->getRegex();
                    }
                    if (re && re->getPatternCStr()) {
                        llvm::Value* pattern_ptr = builder->CreateGlobalStringPtr(re->getPatternCStr());
                        llvm::Value* options_val = llvm::ConstantInt::get(i64_type, re->getOptions());
                        // Plumb the regex global flag (e.g. /g) — lives separately from
                        // PCRE options on QoreRegex. Without this, RegexExtract /g
                        // returns only the first match in AOT.
                        llvm::Value* global_val = llvm::ConstantInt::get(i32_type,
                            re->isGlobal() ? 1 : 0);
                        auto robp_ft = llvm::FunctionType::get(i64_type,
                                {i32_type, ptr_type, i64_type, i32_type, i64_type, ptr_type},
                                false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_regex_op_by_pattern", robp_ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_regex_op_by_pattern_throwing", robp_ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {opcode_val, pattern_ptr, options_val, global_val,
                                 operand_boxed, xsink_arg},
                                module, llvm_func, inst);
                    } else {
                        // Fallback to slot-based dispatch
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                        auto rowo_aot_ft = llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type, i32_type, i64_type, ptr_type}, false);
                        auto helper = module.getOrInsertFunction(
                                "qore_rt_regex_op_with_operand_aot", rowo_aot_ft);
                        auto helper_throwing = module.getOrInsertFunction(
                                "qore_rt_regex_op_with_operand_aot_throwing", rowo_aot_ft);
                        result = emitMaybeInvoke(helper, helper_throwing,
                                {aot_ctx_arg, opcode_val,
                                 llvm::ConstantInt::get(i32_type, slot),
                                 operand_boxed, xsink_arg},
                                module, llvm_func, inst);
                    }
                } else {
                    llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                    auto rowo_ft = llvm::FunctionType::get(i64_type,
                            {i32_type, i64_type, i64_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_regex_op_with_operand", rowo_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_regex_op_with_operand_throwing", rowo_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {opcode_val, expr_const, operand_boxed, xsink_arg},
                            module, llvm_func, inst);
                }
            } else {
                if (aot_mode) {
                    return setAotExpressionFallbackError(error, inst,
                            "regex expression is missing a lowered operand");
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

            auto srm_ft = llvm::FunctionType::get(i64_type,
                    {i64_type, i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_switch_regex_match", srm_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_switch_regex_match_throwing", srm_ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {regex_case_ptr, operand_boxed, xsink_arg},
                    module, llvm_func, inst);

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
            if (inst->opcode == QoreIROpcode::ListAssignAny && inst->operands.size() >= 2) {
                auto* rhs = getVal(inst->operands[0].id, error);
                auto* idx = getVal(inst->operands[1].id, error);
                if (!rhs || !idx) { return false; }
                llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[0].id);
                llvm::Value* idx_int;
                if (nanboxed_values.count(inst->operands[1].id)) {
                    auto unbox_fn = module.getOrInsertFunction("qore_rt_get_int64",
                            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                    idx_int = builder->CreateCall(unbox_fn, {idx, xsink_arg});
                } else {
                    idx_int = idx;
                }
                auto helper = module.getOrInsertFunction("qore_rt_list_assignment_value",
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_list_assignment_value_throwing",
                        llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
                llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                        {rhs_boxed, idx_int, xsink_arg}, module, llvm_func, inst);
                values[inst->result.id] = result;
                nanboxed_values.insert(inst->result.id);
                trackResultForCleanup(result, inst->result.id, llvm_func);
                emitExceptionCheck(module, llvm_func, inst);
                return true;
            }
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            // Clear reload trackers before AST delegation to prevent refcount
            // inflation → copy-on-write → O(n²) for container ops in loops
            clearAllLocalReloadTrackers(module, llvm_func);
            llvm::Value* result;
            if (aot_mode) {
                return setAotExpressionFallbackError(error, inst,
                        "lvalue-modifying expression is missing decomposed operands");
            } else {
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                auto ie_ft = llvm::FunctionType::get(i64_type,
                        {i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr", ie_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_invoke_expr_throwing", ie_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {expr_const, xsink_arg}, module, llvm_func, inst);
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

        // === IsCollectionType: runtime type check (1 if NT_LIST or NT_OBJECT) ===
        case QoreIROpcode::IsCollectionType: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_is_collection_type",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result_int = builder->CreateCall(helper, {val_boxed});
            // Result is native i64 (0 or 1), treat as native int (not nanboxed)
            values[inst->result.id] = result_int;
            return true;
        }

        // === InstanceOf: native type check with pre-evaluated operand ===
        case QoreIROpcode::InstanceOfBool: {
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            if (!aot_mode) {
                // JIT mode: use pre-evaluated operand + type info pointer
                auto* val = getVal(inst->operands[0].id, error);
                if (!val) { return false; }
                llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
                auto* io_node = static_cast<const QoreInstanceOfOperatorNode*>(
                    expr_inst->expr.getInternalNode());
                const QoreTypeInfo* ti = io_node->getInstanceTypeInfo();
                llvm::Value* ti_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(ti));
                llvm::Value* ti_as_ptr = builder->CreateIntToPtr(ti_ptr, ptr_type);
                auto helper = module.getOrInsertFunction("qore_rt_instanceof",
                        llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
                llvm::Value* result = builder->CreateCall(helper, {val_boxed, ti_as_ptr});
                values[inst->result.id] = result;
                nanboxed_values.insert(inst->result.id);
                // No trackResultForCleanup — boolean result, no heap allocation
                // No emitExceptionCheck — instanceof cannot throw
                return true;
            }
            // AOT mode: extract type path at codegen time, embed as string constant
            auto* io_node = static_cast<const QoreInstanceOfOperatorNode*>(
                expr_inst->expr.getInternalNode());
            if (io_node) {
                auto* val = getVal(inst->operands[0].id, error);
                if (!val) { return false; }
                llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
                const QoreTypeInfo* ti = io_node->getInstanceTypeInfo();
                std::string type_path = ti ? QoreTypeInfo::getPath(ti) : "";
                llvm::Value* type_path_ptr = builder->CreateGlobalStringPtr(type_path);
                auto iobtp_ft = llvm::FunctionType::get(i64_type,
                        {i64_type, ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(
                        "qore_rt_instanceof_by_type_path", iobtp_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_instanceof_by_type_path_throwing", iobtp_ft);
                llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                        {val_boxed, type_path_ptr, xsink_arg},
                        module, llvm_func, inst);
                values[inst->result.id] = result;
                nanboxed_values.insert(inst->result.id);
                return true;
            }
            return setAotExpressionFallbackError(error, inst,
                    "instanceof expression has no compile-time type metadata");
        }

        // === Unary ops with pre-evaluated operands (Keys, Elements) ===
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt: {
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            llvm::Value* result;
            if (!expr_inst->operands.empty()) {
                // Pre-evaluated operand: dispatch via qore_rt_unary_op (no expr slot needed)
                auto* operand = getVal(inst->operands[0].id, error);
                if (!operand) { return false; }
                llvm::Value* operand_boxed = boxValue(operand, inst->operands[0].id);
                llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
                auto helper = module.getOrInsertFunction("qore_rt_unary_op",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {opcode_val, operand_boxed, xsink_arg});
            } else {
                if (aot_mode) {
                    return setAotExpressionFallbackError(error, inst,
                            "unary expression is missing a lowered operand");
                } else {
                    QoreValue expr_val = expr_inst->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
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

        // === Background op ===
        case QoreIROpcode::BackgroundInt: {
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            llvm::Value* result = nullptr;
            const auto* bg_inst = dynamic_cast<const QoreIRBackgroundInstruction*>(inst);
            if (bg_inst && aot_mode
                    && tryEmitBackgroundMetadata(bg_inst, module, llvm_func,
                        /*throwing_ok*/true, &result)) {
                // Native AOT background metadata path; no EXPR_TREE slot needed.
            } else if (!expr_inst->operands.empty()
                    && tryEmitDecomposedBackground(expr_inst->expr, expr_inst->operands,
                        module, llvm_func, inst, /*throwing_ok*/true, &result)) {
                // Decomposed path emitted — result is set.
            } else {
                if (aot_mode) {
                    return setAotExpressionFallbackError(error, inst,
                            "background expression has no native lowering");
                } else {
                    QoreValue expr_val = expr_inst->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
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
            // Fallback: no operands available.
            if (aot_mode) {
                return setAotExpressionFallbackError(error, inst,
                        "dot-eval expression is missing a lowered base operand");
            } else {
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                auto ie_ft = llvm::FunctionType::get(i64_type,
                        {i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr", ie_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_invoke_expr_throwing", ie_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {expr_const, xsink_arg}, module, llvm_func, inst);
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
        case QoreIROpcode::CastComplexHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
        case QoreIROpcode::CastAny: {
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            // operand[0] is the pre-evaluated inner value
            auto* inner_val = getVal(inst->operands[0].id, error);
            if (!inner_val) { return false; }
            llvm::Value* inner_boxed = boxValue(inner_val, inst->operands[0].id);
            llvm::Value* result;
            if (aot_mode) {
                // AOT: extract type path at compile time, resolve at runtime
                auto* cast_node = dynamic_cast<const QoreCastOperatorNode*>(
                    expr_inst->expr.getInternalNode());
                if (cast_node) {
                    const QoreTypeInfo* ti = cast_node->getCastTypeInfo();
                    std::string type_path = ti ? QoreTypeInfo::getPath(ti)
                        : (inst->opcode == QoreIROpcode::CastList ? "list" : "");
                    llvm::Value* type_path_ptr = builder->CreateGlobalStringPtr(type_path);
                    llvm::Value* or_nothing_val = llvm::ConstantInt::get(i64_type,
                            cast_node->isOrNothing() ? 1 : 0);
                    auto cbtp_ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i64_type, ptr_type, i64_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_cast_by_type_path_aot", cbtp_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_cast_by_type_path_aot_throwing", cbtp_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {aot_ctx_arg, inner_boxed, type_path_ptr, or_nothing_val, xsink_arg},
                            module, llvm_func, inst);
                } else {
                    // Fallback to expr slot if cast node is unavailable
                    QoreValue expr_val = expr_inst->expr;
                    uint64_t expr_bits;
                    std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                    auto cwi_aot_ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, i64_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_cast_with_inner_aot", cwi_aot_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_cast_with_inner_aot_throwing", cwi_aot_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                             inner_boxed, xsink_arg},
                            module, llvm_func, inst);
                }
            } else {
                QoreValue expr_val = expr_inst->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                auto cwi_ft = llvm::FunctionType::get(i64_type,
                        {i64_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction(
                        "qore_rt_cast_with_inner", cwi_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_cast_with_inner_throwing", cwi_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {expr_const, inner_boxed, xsink_arg},
                        module, llvm_func, inst);
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
            // These expression forms must be decomposed before AOT codegen.
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            llvm::Value* result;
            if (aot_mode) {
                return setAotExpressionFallbackError(error, inst,
                        "map/select expression has no native lowering");
            } else {
                llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
                auto ie_ft = llvm::FunctionType::get(i64_type,
                        {i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_invoke_expr", ie_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_invoke_expr_throwing", ie_ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {expr_const, xsink_arg}, module, llvm_func, inst);
            }
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Statement operations ===
        case QoreIROpcode::OnBlockExit: {
            const auto* sinst = static_cast<const QoreIROnBlockExitInstruction*>(inst);
            has_on_block_exit_handlers = true;
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
            // Native ContextInit: call qore_rt_context_init with (name, exp_bits,
            // where_bits, sort_bits, sort_type).  Returns the Context* as i64
            // (0 on failure with xsink set).  This is a pure i64 — NOT nan-boxed.
            const auto* cinst = static_cast<const QoreIRContextInstruction*>(inst);

            // Name → global string pointer.
            llvm::Value* name_ptr;
            if (cinst->name.empty()) {
                name_ptr = llvm::ConstantPointerNull::get(
                        llvm::PointerType::get(ctx, 0));
            } else {
                name_ptr = builder->CreateGlobalStringPtr(cinst->name);
            }

            auto makeExprBits = [&](const QoreValue& expr) -> llvm::Value* {
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr, sizeof(expr_bits));
                if (aot_mode && expr_bits != 0) {
                    // Route through the AOT expr-slot table so the pointer
                    // survives deserialize/relocate.
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)
                            ->getExprSlot(expr_bits);
                    auto h = module.getOrInsertFunction(
                            "qore_rt_get_expr_bits_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type}, false));
                    return builder->CreateCall(h, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot)});
                }
                return llvm::ConstantInt::get(i64_type, expr_bits);
            };

            llvm::Value* exp_bits = makeExprBits(cinst->exp);
            llvm::Value* where_bits = makeExprBits(cinst->where_exp);
            llvm::Value* sort_bits = makeExprBits(cinst->sort_exp);
            llvm::Value* sort_type_val = llvm::ConstantInt::get(
                    llvm::Type::getInt32Ty(ctx), cinst->sort_type);

            auto ci_ft = llvm::FunctionType::get(i64_type,
                    {ptr_type, i64_type, i64_type, i64_type,
                     llvm::Type::getInt32Ty(ctx), ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_context_init", ci_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_context_init_throwing", ci_ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {name_ptr, exp_bits, where_bits, sort_bits, sort_type_val, xsink_arg},
                    module, llvm_func, inst);
            values[inst->result.id] = result;
            // Context ctor may evaluate user expressions (where/sort) and
            // thus touch thread-local locals (via %field refs in those
            // expressions — they go through the AST expr path).  Reload.
            reloadAllLocalsFromRuntime(module, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::ContextRef: {
            const auto* cri = static_cast<const QoreIRContextRefInstruction*>(inst);
            llvm::Value* key_ptr = builder->CreateGlobalStringPtr(cri->key);
            llvm::Value* stack_offset = llvm::ConstantInt::get(i32_type, cri->stack_offset);
            auto ft = llvm::FunctionType::get(i64_type,
                    {ptr_type, i32_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_context_ref_at", ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_context_ref_at_throwing", ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {key_ptr, stack_offset, xsink_arg}, module, llvm_func, inst);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::ContextRow: {
            auto ft = llvm::FunctionType::get(i64_type, {ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_context_row", ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_context_row_throwing", ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {xsink_arg}, module, llvm_func, inst);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::ContextMaxPos: {
            // Pure i64 helper: no EH, no side effects.
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto helper = module.getOrInsertFunction("qore_rt_context_max_pos",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {state_val});
            values[inst->result.id] = result;
            return true;
        }
        case QoreIROpcode::ContextSetPos: {
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto* index_val = getVal(inst->operands[1].id, error);
            if (!index_val) { return false; }
            if (index_val->getType() != i64_type) {
                // Index may flow through a nan-boxed PHI (loop counter); unbox.
                index_val = ensureIntTypeInline(index_val, inst->operands[1].id);
            }
            auto helper = module.getOrInsertFunction("qore_rt_context_set_pos",
                    llvm::FunctionType::get(void_type,
                        {i64_type, i64_type}, false));
            builder->CreateCall(helper, {state_val, index_val});
            // Void result — OPCODE_REGISTRY produces_result=false, no SSA value.
            return true;
        }
        case QoreIROpcode::ContextDestroy: {
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto helper = module.getOrInsertFunction("qore_rt_context_destroy",
                    llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
            builder->CreateCall(helper, {state_val, xsink_arg});
            // Void result — OPCODE_REGISTRY produces_result=false, no SSA value.
            return true;
        }
        case QoreIROpcode::Backquote: {
            const auto* binst = static_cast<const QoreIRBackquoteInstruction*>(inst);
            llvm::Value* cmd_ptr = builder->CreateGlobalStringPtr(binst->command);
            auto bq_ft = llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_backquote", bq_ft);
            auto helper_throwing = module.getOrInsertFunction("qore_rt_backquote_throwing", bq_ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {cmd_ptr, xsink_arg}, module, llvm_func, inst);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::Find: {
            const auto* finst = static_cast<const QoreIRFindInstruction*>(inst);
            auto makeExprBits = [&](const QoreValue& v) -> llvm::Value* {
                uint64_t expr_bits = toBits(v);
                if (aot_mode && expr_bits != 0) {
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)
                            ->getExprSlot(expr_bits);
                    auto h = module.getOrInsertFunction(
                            "qore_rt_get_expr_bits_aot",
                            llvm::FunctionType::get(i64_type,
                                {ptr_type, i32_type}, false));
                    return builder->CreateCall(h, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot)});
                }
                return llvm::ConstantInt::get(i64_type, expr_bits);
            };

            llvm::Value* exp_bits = makeExprBits(finst->exp);
            llvm::Value* find_exp_bits = makeExprBits(finst->find_exp);
            llvm::Value* where_bits = makeExprBits(finst->where);
            auto find_ft = llvm::FunctionType::get(i64_type,
                    {i64_type, i64_type, i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_find", find_ft);
            auto helper_throwing = module.getOrInsertFunction("qore_rt_find_throwing", find_ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {exp_bits, find_exp_bits, where_bits, xsink_arg}, module, llvm_func, inst);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
            // Find evaluates its sub-expressions through the AST path and can
            // update thread-local locals via context references.
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
            auto es_ft = llvm::FunctionType::get(i64_type,
                    {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_exec_statement", es_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_exec_statement_throwing", es_ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {opcode_val, stmt_as_ptr, xsink_arg}, module, llvm_func, inst);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            // Summarize executes through the AST path and can modify locals
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
            emitPendingSsaCleanup(module);
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

        case QoreIROpcode::PushTempMark: {
            temp_cleanup_marks.push_back({
                invoke_result_allocas.size(),
                pending_ssa_cleanup.size()
            });
            return true;
        }
        case QoreIROpcode::DebugBlock: {
            return true;
        }
        case QoreIROpcode::DiscardTemps: {
            emitDiscardTemps(module);
            return true;
        }

        // Phase 1.5 init-expression outlining: invoke an AOT-emitted
        // helper function by LLVM symbol name.  Helper ABI matches the
        // standard AOT init-function signature (ptr ctx, ptr xsink) →
        // i64 nan-boxed value.  After the call, propagate any thrown
        // exception through the same xsink-based mechanism used by
        // other AOT helper calls.
        case QoreIROpcode::CallAOTHelper: {
            const auto* cah = static_cast<const QoreIRCallAOTHelperInstruction*>(inst);
            auto* helper_ft = llvm::FunctionType::get(i64_type,
                    {ptr_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction(cah->helper_name, helper_ft);
            llvm::Value* result = builder->CreateCall(helper,
                    {aot_ctx_arg, xsink_arg});
            emitExceptionCheck(module, llvm_func, inst);
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);
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
                // Call the implementation function with inline_lowered flag
                // If inline_lowered=true, skip handler execution (handlers were already inlined)
                // If inline_lowered=false, execute handlers normally
                auto helper = module.getOrInsertFunction("qore_rt_exec_on_block_exit_impl",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type, builder->getInt1Ty()}, false));
                llvm::Value* inline_lowered_val = llvm::ConstantInt::get(builder->getInt1Ty(), sinst->inline_lowered ? 1 : 0);
                syncLocalsToRuntimeForHandlers(module);
                llvm::Value* native_slot_cache = beginNativeHandlerSlotCache(module);
                builder->CreateCall(helper, {saved_count, xsink_arg, inline_lowered_val});
                endNativeHandlerSlotCache(module, native_slot_cache);
                // On-block-exit handlers execute through the AST path and can modify
                // any local variable on the thread-local stack. Reload all local
                // allocas so subsequent LoadLocal sees the updated values.
                // (Only needed if handlers actually executed, but safe to do always)
                if (!sinst->inline_lowered) {
                    reloadAllLocalsFromRuntime(module, llvm_func);
                    emitExceptionCheck(module, llvm_func, inst);
                }
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
                auto ft = llvm::FunctionType::get(ptr_type,
                        {ptr_type, i32_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_iterator_create_aot", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_iterator_create_aot_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                         iterable_boxed, xsink_arg},
                        module, llvm_func, inst);
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
                auto ft = llvm::FunctionType::get(ptr_type,
                        {i64_type, ptr_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_iterator_create", ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_iterator_create_throwing", ft);
                result = emitMaybeInvoke(helper, helper_throwing,
                        {iterable_boxed, iter_func_ptr, xsink_arg},
                        module, llvm_func, inst);
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
                // A foreach can exit early (for example via break) before
                // IteratorNext exhausts and deletes the iterator. If this
                // IteratorCreate is reached again, release the previous active
                // iterator before overwriting the cleanup slot.
                auto cleanup_helper = module.getOrInsertFunction("qore_rt_iterator_cleanup",
                        llvm::FunctionType::get(void_type, {ptr_type}, false));
                llvm::Value* old_iter_ptr = builder->CreateLoad(ptr_type, iter_alloca);
                builder->CreateStore(llvm::ConstantPointerNull::get(
                        llvm::PointerType::get(ctx, 0)), iter_alloca);
                builder->CreateCall(cleanup_helper, {old_iter_ptr});
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
            auto iter_next_ft = llvm::FunctionType::get(i64_type,
                    {ptr_type, ptr_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_iterator_next", iter_next_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_iterator_next_throwing", iter_next_ft);
            llvm::Value* done_flag = emitMaybeInvoke(helper, helper_throwing,
                    {iter_ptr, out_val_ptr, xsink_arg}, module, llvm_func, inst);
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
            registerInvokeCleanupAlloca(static_cast<llvm::AllocaInst*>(out_val_ptr));
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
                // AOT: load the ParseReferenceNode* POINTER bits from the
                // expression slot — NOT an evaluation of the node.
                // `qore_rt_ref_foreach_init` does the `evalToRef` itself
                // (needs the parse node to construct a runtime
                // ReferenceNode with correct lvalue semantics).  Matches
                // the JIT path's `llvm::ConstantInt::get(bits)`, just
                // indirected through the AOT slot table so the address
                // survives load-time relocation.  Historical bug:
                // called `qore_rt_invoke_expr_aot` here, which
                // `eval()`s the node and returned a `ReferenceNode*`
                // — the callee then `reinterpret_cast`ed that as a
                // `ParseReferenceNode*` and SIGSEGV'd on the wrong
                // vtable entry for `evalToRef`.
                QoreValue expr_val = rfi->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(expr_bits);
                auto helper = module.getOrInsertFunction("qore_rt_get_expr_bits_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type}, false));
                llvm::Value* bits_val = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot)});
                parse_ref_bits_val = bits_val;
            } else {
                // JIT: pass the ParseReferenceNode QoreValue bits directly as a constant
                QoreValue expr_val = rfi->expr;
                uint64_t bits;
                std::memcpy(&bits, &expr_val, sizeof(bits));
                parse_ref_bits_val = llvm::ConstantInt::get(i64_type, bits);
            }
            auto rfi_ft = llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_init", rfi_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_ref_foreach_init_throwing", rfi_ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {parse_ref_bits_val, xsink_arg}, module, llvm_func, inst);
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
            if (index_val->getType() != i64_type) {
                error = "RefForeachGetEntry: index must be i64";
                return false;
            }
            // Index may flow through a nan-boxed PHI (loop counter); unbox
            // to plain i64 before passing to the runtime helper.
            llvm::Value* index_unboxed = ensureIntTypeInline(index_val, inst->operands[1].id);
            auto rfge_ft = llvm::FunctionType::get(i64_type,
                    {i64_type, i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_get_entry", rfge_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_ref_foreach_get_entry_throwing", rfge_ft);
            llvm::Value* result = emitMaybeInvoke(helper, helper_throwing,
                    {state_val, index_unboxed, xsink_arg}, module, llvm_func, inst);
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
            auto rfr_ft = llvm::FunctionType::get(void_type,
                    {i64_type, i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_record", rfr_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_ref_foreach_record_throwing", rfr_ft);
            emitMaybeInvoke(helper, helper_throwing,
                    {state_val, value_boxed, xsink_arg}, module, llvm_func, inst);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::RefForeachFinalize: {
            // operands: state, fill_remaining
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto* fill_val = getVal(inst->operands[1].id, error);
            if (!fill_val) { return false; }
            auto rff_ft = llvm::FunctionType::get(void_type,
                    {i64_type, i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_finalize", rff_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_ref_foreach_finalize_throwing", rff_ft);
            emitMaybeInvoke(helper, helper_throwing,
                    {state_val, fill_val, xsink_arg}, module, llvm_func, inst);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::RefForeachCleanup: {
            // operands: state
            auto* state_val = getVal(inst->operands[0].id, error);
            if (!state_val) { return false; }
            auto rfc_ft = llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false);
            auto helper = module.getOrInsertFunction("qore_rt_ref_foreach_cleanup", rfc_ft);
            auto helper_throwing = module.getOrInsertFunction(
                    "qore_rt_ref_foreach_cleanup_throwing", rfc_ft);
            emitMaybeInvoke(helper, helper_throwing,
                    {state_val, xsink_arg}, module, llvm_func, inst);
            // Cleanup is also used while an exception is already pending in
            // xsink.  Match the IR interpreter: do not short-circuit after the
            // helper, so the following synthetic rethrow can route the pending
            // exception to the enclosing handler.
            return true;
        }

        case QoreIROpcode::SwitchCaseMatch: {
            // operands[0] = switch value (NaN-boxed)
            // case_node = pointer to CaseNode (compile-time constant)
            auto* case_inst = static_cast<const QoreIRSwitchCaseMatchInstruction*>(inst);
            auto* switch_val = getVal(inst->operands[0].id, error);
            if (!switch_val) { return false; }
            llvm::Value* switch_boxed = boxValue(switch_val, inst->operands[0].id);
            if (aot_mode) {
                // AOT: case constant values that contain heap-allocated nodes
                // (e.g., QoreStringNode) have process-specific pointers that can't
                // be embedded as LLVM constants. Use expression slot indirection
                // for node values; immediate values (int, bool, short strings) are
                // safe to embed directly.
                QoreValue case_val = case_inst->case_node->val;
                // Unwrap enum values at compile time (matches CaseNode::matches semantics)
                if (case_val.isEnum()) {
                    case_val = case_val.getEnumMember()->getValue();
                }
                uint64_t case_bits;
                std::memcpy(&case_bits, &case_val, sizeof(case_bits));
                llvm::Value* result;
                if (case_val.hasNode()) {
                    // Node value: load from expression slot at runtime
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(case_bits);
                    auto scmva_ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, i64_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_switch_case_match_value_aot", scmva_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_switch_case_match_value_aot_throwing", scmva_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot),
                             switch_boxed, xsink_arg},
                            module, llvm_func, inst);
                } else {
                    // Immediate value: safe to embed as constant (no pointers)
                    llvm::Value* case_const = llvm::ConstantInt::get(i64_type, case_bits);
                    auto scmv_ft = llvm::FunctionType::get(i64_type,
                            {i64_type, i64_type, ptr_type}, false);
                    auto helper = module.getOrInsertFunction(
                            "qore_rt_switch_case_match_value", scmv_ft);
                    auto helper_throwing = module.getOrInsertFunction(
                            "qore_rt_switch_case_match_value_throwing", scmv_ft);
                    result = emitMaybeInvoke(helper, helper_throwing,
                            {case_const, switch_boxed, xsink_arg},
                            module, llvm_func, inst);
                }
                values[inst->result.id] = result;
                nanboxed_values.insert(inst->result.id);
            } else {
                // JIT: embed CaseNode* directly (valid — same process)
                auto* case_node_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(case_inst->case_node));
                auto* case_node_val = builder->CreateIntToPtr(case_node_ptr, ptr_type);
                auto scm_ft = llvm::FunctionType::get(i64_type,
                        {ptr_type, i64_type, ptr_type}, false);
                auto helper = module.getOrInsertFunction("qore_rt_switch_case_match", scm_ft);
                auto helper_throwing = module.getOrInsertFunction(
                        "qore_rt_switch_case_match_throwing", scm_ft);
                auto* result = emitMaybeInvoke(helper, helper_throwing,
                        {case_node_val, switch_boxed, xsink_arg},
                        module, llvm_func, inst);
                values[inst->result.id] = result;
                nanboxed_values.insert(inst->result.id);
            }
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Container access with pre-evaluated operands ===
        case QoreIROpcode::HashDerefDynamic:
        case QoreIROpcode::ListIndexDynamic: {
            if (inst->operands.size() >= 2) {
                auto* lhs = getVal(inst->operands[0].id, error);
                if (!lhs) { return false; }
                llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
                if (inst->opcode == QoreIROpcode::ListIndexDynamic) {
                    const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
                    if (!expr_inst->list_selector_kinds.empty()) {
                        int num_selectors = static_cast<int>(inst->operands.size()) - 1;
                        llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                                llvm_func->getEntryBlock().begin());
                        llvm::Value* selector_array = ab.CreateAlloca(i64_type,
                                llvm::ConstantInt::get(i32_type, num_selectors));
                        for (int i = 0; i < num_selectors; ++i) {
                            auto* selector = getVal(inst->operands[i + 1].id, error);
                            if (!selector) { return false; }
                            llvm::Value* boxed = boxValue(selector, inst->operands[i + 1].id);
                            llvm::Value* gep = builder->CreateGEP(i64_type, selector_array,
                                    llvm::ConstantInt::get(i32_type, i));
                            builder->CreateStore(boxed, gep);
                        }
                        std::string kind_bytes(reinterpret_cast<const char*>(
                            expr_inst->list_selector_kinds.data()), expr_inst->list_selector_kinds.size());
                        llvm::Value* kinds_ptr = builder->CreateGlobalStringPtr(
                            llvm::StringRef(kind_bytes.data(), kind_bytes.size()), "list_selector_kinds");
                        auto helper = module.getOrInsertFunction("qore_rt_list_index_selectors",
                            llvm::FunctionType::get(i64_type,
                                {i64_type, ptr_type, i32_type, ptr_type, ptr_type}, false));
                        llvm::Value* result = builder->CreateCall(helper,
                            {lhs_boxed, kinds_ptr,
                             llvm::ConstantInt::get(i32_type,
                                static_cast<int32_t>(expr_inst->list_selector_kinds.size())),
                             selector_array, xsink_arg});
                        values[inst->result.id] = result;
                        nanboxed_values.insert(inst->result.id);
                        trackResultForCleanup(result, inst->result.id, llvm_func);
                        emitExceptionCheck(module, llvm_func, inst);
                        reloadAllLocalsFromRuntime(module, llvm_func);
                        return true;
                    }
                }
                auto* rhs = getVal(inst->operands[1].id, error);
                if (!rhs) { return false; }
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
                // Container access can modify locals through side effects (object member access)
                reloadAllLocalsFromRuntime(module, llvm_func);
                return true;
            }
            // No operands — fall through to error
            error = "HashDerefDynamic/ListIndexDynamic without operands";
            return false;
        }

        // LValuePath opcodes: call runtime helpers with instruction pointer + dynamic operands
        case QoreIROpcode::LValuePathAssign:
        case QoreIROpcode::LValuePathCompound:
        case QoreIROpcode::LValuePathUnary:
        case QoreIROpcode::LValuePathBinaryMut:
        case QoreIROpcode::LValuePathTernary: {
            const auto* path_inst = static_cast<const QoreIRLValuePathInstruction*>(inst);

            // Count dynamic operands (single-value steps with operand_idx != UINT32_MAX,
            // plus each SSA id inside slice steps)
            int num_dyn = 0;
            for (const auto& step : path_inst->path) {
                if (step.operand_idx != UINT32_MAX) {
                    ++num_dyn;
                }
                if (step.kind == LVPathStepKind::HashKeySlice
                        || step.kind == LVPathStepKind::ListIndexSlice
                        || step.kind == LVPathStepKind::ListRangeSlice) {
                    num_dyn += static_cast<int>(step.slice_operand_ids.size());
                }
            }

            // Build dynamic operands array (alloca in entry block)
            llvm::Value* dyn_array;
            if (num_dyn > 0) {
                llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                        llvm_func->getEntryBlock().begin());
                dyn_array = ab.CreateAlloca(i64_type,
                        llvm::ConstantInt::get(i32_type, num_dyn));
                int dyn_idx = 0;
                for (const auto& step : path_inst->path) {
                    if (step.operand_idx != UINT32_MAX) {
                        auto* dyn_val = getVal(step.operand_idx, error);
                        if (!dyn_val) { return false; }
                        llvm::Value* boxed = boxValue(dyn_val, step.operand_idx);
                        llvm::Value* gep = builder->CreateGEP(i64_type, dyn_array,
                                llvm::ConstantInt::get(i32_type, dyn_idx++));
                        builder->CreateStore(boxed, gep);
                    }
                    if (step.kind == LVPathStepKind::HashKeySlice
                            || step.kind == LVPathStepKind::ListIndexSlice
                            || step.kind == LVPathStepKind::ListRangeSlice) {
                        for (uint32_t sid : step.slice_operand_ids) {
                            auto* dyn_val = getVal(sid, error);
                            if (!dyn_val) { return false; }
                            llvm::Value* boxed = boxValue(dyn_val, sid);
                            llvm::Value* gep = builder->CreateGEP(i64_type, dyn_array,
                                    llvm::ConstantInt::get(i32_type, dyn_idx++));
                            builder->CreateStore(boxed, gep);
                        }
                    }
                }
            } else {
                dyn_array = llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx, 0));
            }

            const void* local_key = findLVPathRootLocalKey(path_inst);
            if (local_key) {
                clearLocalCachedValue(local_key, module, llvm_func,
                    LocalCacheClearMode::DuplicateRefsOnly);
                clearLocalReloadTracker(local_key, module, llvm_func);
            }

            // Pre-decref old result if result is used
            if (inst->result.isValid()) {
                // Create or reuse cleanup alloca
                llvm::AllocaInst* ca;
                auto it = invoke_alloca_map.find(inst->result.id);
                if (it != invoke_alloca_map.end()) {
                    ca = static_cast<llvm::AllocaInst*>(it->second);
                } else {
                    llvm::IRBuilder<> ab(&llvm_func->getEntryBlock(),
                            llvm_func->getEntryBlock().begin());
                    ca = ab.CreateAlloca(i64_type, nullptr, "lvp_cleanup");
                    ab.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), ca);
                    registerInvokeCleanupAlloca(ca);
                    invoke_alloca_map[inst->result.id] = ca;
                }
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                llvm::Value* old_val = builder->CreateLoad(i64_type, ca);
                builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), ca);
                builder->CreateCall(decref_fn, {old_val, xsink_arg});
            }

            // Get instruction pointer (JIT: ConstantInt, AOT: slot lookup)
            llvm::Value* inst_ptr_val;
            int32_t aot_slot = -1;
            if (aot_slots) {
                aot_slot = const_cast<AOTSlotMap*>(aot_slots)->getLVPathSlot(
                    reinterpret_cast<const void*>(path_inst));
            }

            if (aot_slots) {
                // AOT mode: pass ctx + slot
                inst_ptr_val = nullptr;  // not used directly
            } else {
                // JIT mode: embed instruction pointer
                inst_ptr_val = builder->CreateIntToPtr(
                    llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uintptr_t>(path_inst)),
                    ptr_type);
            }

            // Emit the call based on opcode
            llvm::Value* result_val;
            switch (inst->opcode) {
                case QoreIROpcode::LValuePathAssign: {
                    auto* rhs_val = getVal(inst->operands[0].id, error);
                    if (!rhs_val) { return false; }
                    llvm::Value* rhs_boxed = boxValue(rhs_val, inst->operands[0].id);
                    if (aot_slots) {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, i64_type, ptr_type}, false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_assign_aot", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_assign_aot_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, aot_slot),
                             dyn_array, rhs_boxed, xsink_arg},
                            module, llvm_func, inst);
                    } else {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, i64_type, ptr_type}, false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_assign", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_assign_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {inst_ptr_val, dyn_array, rhs_boxed, xsink_arg},
                            module, llvm_func, inst);
                    }
                    break;
                }
                case QoreIROpcode::LValuePathCompound: {
                    auto* rhs_val = getVal(inst->operands[0].id, error);
                    if (!rhs_val) { return false; }
                    llvm::Value* rhs_boxed = boxValue(rhs_val, inst->operands[0].id);
                    if (aot_slots) {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, i64_type, ptr_type}, false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_compound_aot", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_compound_aot_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, aot_slot),
                             dyn_array, rhs_boxed, xsink_arg},
                            module, llvm_func, inst);
                    } else {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, i64_type, ptr_type}, false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_compound", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_compound_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {inst_ptr_val, dyn_array, rhs_boxed, xsink_arg},
                            module, llvm_func, inst);
                    }
                    break;
                }
                case QoreIROpcode::LValuePathUnary: {
                    if (aot_slots) {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, ptr_type}, false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_unary_aot", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_unary_aot_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, aot_slot),
                             dyn_array, xsink_arg},
                            module, llvm_func, inst);
                    } else {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, ptr_type}, false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_unary", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_unary_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {inst_ptr_val, dyn_array, xsink_arg},
                            module, llvm_func, inst);
                    }
                    break;
                }
                case QoreIROpcode::LValuePathBinaryMut: {
                    // operands[0] = RHS for push/unshift; empty for regex subst/transliterate
                    llvm::Value* rhs_boxed;
                    if (!inst->operands.empty()) {
                        auto* rhs_val = getVal(inst->operands[0].id, error);
                        if (!rhs_val) { return false; }
                        rhs_boxed = boxValue(rhs_val, inst->operands[0].id);
                    } else {
                        rhs_boxed = llvm::ConstantInt::get(i64_type, VAL_NOTHING);
                    }
                    if (aot_slots) {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, i64_type, ptr_type}, false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_binary_mut_aot", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_binary_mut_aot_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, aot_slot),
                             dyn_array, rhs_boxed, xsink_arg},
                            module, llvm_func, inst);
                    } else {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, i64_type, ptr_type}, false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_binary_mut", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_binary_mut_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {inst_ptr_val, dyn_array, rhs_boxed, xsink_arg},
                            module, llvm_func, inst);
                    }
                    break;
                }
                case QoreIROpcode::LValuePathTernary: {
                    // operands[0]=offset, [1]=length, [2]=replacement
                    auto* offset_v = (inst->operands.size() > 0)
                        ? getVal(inst->operands[0].id, error) : nullptr;
                    auto* length_v = (inst->operands.size() > 1)
                        ? getVal(inst->operands[1].id, error) : nullptr;
                    auto* replace_v = (inst->operands.size() > 2)
                        ? getVal(inst->operands[2].id, error) : nullptr;
                    llvm::Value* a_boxed = offset_v
                        ? boxValue(offset_v, inst->operands[0].id)
                        : llvm::ConstantInt::get(i64_type, VAL_NOTHING);
                    llvm::Value* b_boxed = length_v
                        ? boxValue(length_v, inst->operands[1].id)
                        : llvm::ConstantInt::get(i64_type, VAL_NOTHING);
                    llvm::Value* c_boxed = replace_v
                        ? boxValue(replace_v, inst->operands[2].id)
                        : llvm::ConstantInt::get(i64_type, VAL_NOTHING);
                    if (aot_slots) {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, i32_type, ptr_type, i64_type, i64_type, i64_type, ptr_type},
                            false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_ternary_aot", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_ternary_aot_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, aot_slot),
                             dyn_array, a_boxed, b_boxed, c_boxed, xsink_arg},
                            module, llvm_func, inst);
                    } else {
                        auto ft = llvm::FunctionType::get(i64_type,
                            {ptr_type, ptr_type, i64_type, i64_type, i64_type, ptr_type}, false);
                        auto fn = module.getOrInsertFunction("qore_rt_lv_path_ternary", ft);
                        auto fn_throwing = module.getOrInsertFunction(
                            "qore_rt_lv_path_ternary_throwing", ft);
                        result_val = emitMaybeInvoke(fn, fn_throwing,
                            {inst_ptr_val, dyn_array, a_boxed, b_boxed, c_boxed, xsink_arg},
                            module, llvm_func, inst);
                    }
                    break;
                }
                default:
                    error = "unexpected LValuePath opcode";
                    return false;
            }

            // Store result and track for cleanup.  If lowering deliberately
            // invalidated the result (statement-form remove/delete/etc.), the
            // runtime helper still returns an owned value; discard it here to
            // match the IR interpreter's invalid-result epilogue.
            if (inst->result.isValid()) {
                values[inst->result.id] = result_val;
                nanboxed_values.insert(inst->result.id);
                // Store in an existing cleanup alloca when this lvalue path
                // was prepared by a compound/update helper; otherwise create
                // normal result cleanup tracking for owned results such as
                // `remove x` used as a call argument.
                auto it = invoke_alloca_map.find(inst->result.id);
                if (it != invoke_alloca_map.end()) {
                    if (it->second) {
                        builder->CreateStore(result_val,
                            static_cast<llvm::AllocaInst*>(it->second));
                    } else {
                        promoteSsaEntryToAlloca(inst->result.id, module,
                                llvm_func);
                    }
                } else {
                    trackResultForCleanup(result_val, inst->result.id,
                            llvm_func);
                }
            } else {
                auto decref_fn = module.getOrInsertFunction("qore_rt_decref",
                        llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
                builder->CreateCall(decref_fn, {result_val, xsink_arg});
            }

            // Duplicate cached refs for the root local were cleared before the
            // runtime lvalue helper so they do not make containers look shared
            // and force unnecessary COW. Refresh that known root immediately;
            // lazy bulk invalidation can otherwise leave control-flow tests
            // reading the cleared/stale cache.
            if (local_key && canReloadLocalFromRuntime(local_key, false)) {
                reloadLocalFromRuntime(local_key, module, llvm_func, false);
            } else {
                // Conservative fallback: reference/global paths can still make
                // unrelated local caches stale through aliases or callbacks.
                reloadAllLocalsFromRuntime(module, llvm_func);
            }

            // Exception check
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        default:
            error = "unsupported IR opcode for LLVM lowering: ";
            error += qoreIROpcodeDiagnosticName(inst->opcode);
            error += " (";
            error += std::to_string(static_cast<int>(inst->opcode));
            error += ")";
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
        const QoreIRInstruction* inst,
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
    auto ft = llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false);
    auto helper = module.getOrInsertFunction(slow_helper, ft);
    std::string throwing_name = std::string(slow_helper) + "_throwing";
    auto helper_throwing = module.getOrInsertFunction(throwing_name, ft);
    llvm::Value* slow_result = emitMaybeInvoke(helper, helper_throwing,
            {lhs, rhs, xsink_arg}, module, llvm_func, inst);
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
        const QoreIRInstruction* inst,
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
    auto cmp_ft = llvm::FunctionType::get(i64_type,
            {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false);
    auto helper = module.getOrInsertFunction("qore_rt_comparison_op", cmp_ft);
    auto helper_throwing = module.getOrInsertFunction(
            "qore_rt_comparison_op_throwing", cmp_ft);
    llvm::Value* slow_result = emitMaybeInvoke(helper, helper_throwing,
            {opcode_val, lhs, rhs, xsink_arg}, module, llvm_func, inst);
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
        const QoreIRInstruction* inst,
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
    auto ca_ft = llvm::FunctionType::get(i64_type,
            {i64_type, i64_type, ptr_type}, false);
    auto ca_helper = module.getOrInsertFunction(slow_helper, ca_ft);
    std::string ca_throwing_name = std::string(slow_helper) + "_throwing";
    auto ca_helper_throwing = module.getOrInsertFunction(ca_throwing_name, ca_ft);
    llvm::Value* slow_result_val = emitMaybeInvoke(ca_helper, ca_helper_throwing,
            {lhs, rhs, xsink_arg}, module, llvm_func, inst);
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
        auto binary_ft = llvm::FunctionType::get(i64_type,
                {i32_type, ptr_type, i32_type, i64_type, ptr_type}, false);
        auto binary_fn = module.getOrInsertFunction(
                "qore_rt_lvalue_binary_aot", binary_ft);
        auto binary_fn_throwing = module.getOrInsertFunction(
                "qore_rt_lvalue_binary_aot_throwing", binary_ft);
        slow_result = emitMaybeInvoke(binary_fn, binary_fn_throwing,
                {opcode_val, aot_ctx_arg, lv_bits_or_slot, val_boxed, xsink_arg},
                module, llvm_func, inst);
    } else {
        auto binary_ft = llvm::FunctionType::get(i64_type,
                {i32_type, i64_type, i64_type, ptr_type}, false);
        auto binary_fn = module.getOrInsertFunction(
                "qore_rt_lvalue_binary", binary_ft);
        auto binary_fn_throwing = module.getOrInsertFunction(
                "qore_rt_lvalue_binary_throwing", binary_ft);
        slow_result = emitMaybeInvoke(binary_fn, binary_fn_throwing,
                {opcode_val, lv_bits_or_slot, val_boxed, xsink_arg},
                module, llvm_func, inst);
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
        const QoreIRInstruction* inst,
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
    auto bw_ft = llvm::FunctionType::get(i64_type,
            {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false);
    auto bw_helper = module.getOrInsertFunction(slow_helper, bw_ft);
    std::string bw_throwing_name = std::string(slow_helper) + "_throwing";
    auto bw_helper_throwing = module.getOrInsertFunction(bw_throwing_name, bw_ft);
    llvm::Value* slow_result_val = emitMaybeInvoke(bw_helper, bw_helper_throwing,
            {opcode_val, lhs, rhs, xsink_arg}, module, llvm_func, inst);
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
        const QoreIRInstruction* inst,
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
    auto u_ft = llvm::FunctionType::get(i64_type,
            {llvm::Type::getInt32Ty(ctx), i64_type, ptr_type}, false);
    auto helper = module.getOrInsertFunction("qore_rt_unary_op", u_ft);
    auto helper_throwing = module.getOrInsertFunction(
            "qore_rt_unary_op_throwing", u_ft);
    llvm::Value* slow_result = emitMaybeInvoke(helper, helper_throwing,
            {opcode_val, operand, xsink_arg}, module, llvm_func, inst);
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
llvm::Value* QoreIRToLLVM::emitAnyCmpSpaceshipFastPath(const QoreIRInstruction* inst,
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
    auto cmp_ft = llvm::FunctionType::get(i64_type,
            {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false);
    auto cmp_helper = module.getOrInsertFunction("qore_rt_comparison_op", cmp_ft);
    auto cmp_helper_throwing = module.getOrInsertFunction(
            "qore_rt_comparison_op_throwing", cmp_ft);
    llvm::Value* slow_result_cmp = emitMaybeInvoke(cmp_helper, cmp_helper_throwing,
            {opcode_val_cmp, lhs, rhs, xsink_arg}, module, llvm_func, inst);
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
        // Empty list returns NOTHING (matching Qore foldl semantics)
        std::string loop_name = std::string(label) + "_loop";
        std::string exit_name = std::string(label) + "_exit";
        std::string empty_name = std::string(label) + "_empty";
        std::string box_name = std::string(label) + "_box";

        llvm::BasicBlock* preheader = builder->GetInsertBlock();
        llvm::BasicBlock* empty_bb = llvm::BasicBlock::Create(ctx, empty_name, llvm_func);
        llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, loop_name, llvm_func);
        llvm::BasicBlock* box_bb = llvm::BasicBlock::Create(ctx, box_name, llvm_func);
        llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, exit_name, llvm_func);

        llvm::Value* is_empty = builder->CreateICmpEQ(size, zero_i);
        builder->CreateCondBr(is_empty, empty_bb, loop_bb);

        // Empty list: return NOTHING (nanboxed)
        builder->SetInsertPoint(empty_bb);
        llvm::Value* nothing_val = llvm::ConstantInt::get(i64_type, 0);  // VAL_NOTHING
        builder->CreateBr(exit_bb);

        // Loop body
        builder->SetInsertPoint(loop_bb);
        llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
        llvm::PHINode* acc_phi = builder->CreatePHI(elem_type, 2, "acc");

        llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});
        llvm::Value* new_acc = accumulate(acc_phi, elem);

        llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
        llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
        builder->CreateCondBr(done, box_bb, loop_bb);

        idx_phi->addIncoming(zero_i, preheader);
        idx_phi->addIncoming(next_idx, loop_bb);
        acc_phi->addIncoming(identity_val, preheader);
        acc_phi->addIncoming(new_acc, loop_bb);

        // Box the result to nanboxed format
        builder->SetInsertPoint(box_bb);
        llvm::Value* boxed_result;
        if (is_float) {
            boxed_result = boxFloat(new_acc);
        } else {
            boxed_result = boxIntInline(new_acc);
        }
        // boxFloat/boxIntInline may create blocks — use the current insert block
        llvm::BasicBlock* box_exit = builder->GetInsertBlock();
        builder->CreateBr(exit_bb);

        // Exit block: merge NOTHING (empty) and boxed result (non-empty)
        builder->SetInsertPoint(exit_bb);
        llvm::PHINode* result_phi = builder->CreatePHI(i64_type, 2,
                std::string(label) + "_result");
        result_phi->addIncoming(nothing_val, empty_bb);
        result_phi->addIncoming(boxed_result, box_exit);

        values[inst->result.id] = result_phi;
        nanboxed_values.insert(inst->result.id);
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

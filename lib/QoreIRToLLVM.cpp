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

#include "qore/intern/QoreAOT.h"
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/OnBlockExitStatement.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include "qore/intern/VarRefNode.h"
#include "qore/intern/QoreOperatorNode.h"
#include <qore/QoreStringNode.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <cstring>

// NaN-boxing constants matching QoreValue.h
static constexpr uint64_t TAG_INT48          = 0xFFF9000000000000ULL;
static constexpr uint64_t PAYLOAD_MASK       = 0x0000FFFFFFFFFFFFULL;
static constexpr uint64_t DOUBLE_ENCODE_OFFSET = 0x0001000000000000ULL;
static constexpr uint64_t VAL_NOTHING        = 0;
static constexpr uint64_t VAL_NULL           = 0xFFFB000000000001ULL;
static constexpr uint64_t VAL_FALSE          = 0xFFFB000000000002ULL;
static constexpr uint64_t VAL_TRUE           = 0xFFFB000000000003ULL;

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

    // Invoke helpers
    module.getOrInsertFunction("qore_rt_invoke_expr",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_make_string",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));
    module.getOrInsertFunction("qore_rt_catch_exception",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));

    // Deopt helper
    module.getOrInsertFunction("qore_rt_deopt",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));

    // Local variable helpers
    module.getOrInsertFunction("qore_rt_instantiate_local",
            llvm::FunctionType::get(void_type, {ptr_type}, false));
    module.getOrInsertFunction("qore_rt_assign_local",
            llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_load_local",
            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
    module.getOrInsertFunction("qore_rt_uninstantiate_local",
            llvm::FunctionType::get(void_type, {ptr_type}, false));

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

    // AOT context-based helpers (Phase 7b)
    // load_local_aot: (ptr, i32, ptr) -> i64
    auto* aot_load_local_ft = llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false);
    module.getOrInsertFunction("qore_rt_load_local_aot", aot_load_local_ft);
    // assign_local_aot: (ptr, i32, i64, ptr) -> void
    module.getOrInsertFunction("qore_rt_assign_local_aot",
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

    // Call with pre-evaluated args: (i64, ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_call_with_args",
            llvm::FunctionType::get(i64_type, {i64_type, ptr_type, i32_type, ptr_type}, false));
    // AOT variant: (ptr, i32, ptr, i32, ptr) -> i64
    module.getOrInsertFunction("qore_rt_call_with_args_aot",
            llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type, i32_type, ptr_type}, false));
}

llvm::FunctionCallee QoreIRToLLVM::getHelper(llvm::Module& module, const char* name, llvm::FunctionType* ft) {
    return module.getOrInsertFunction(name, ft);
}

llvm::Value* QoreIRToLLVM::getVal(uint32_t id, std::string& error) {
    auto it = values.find(id);
    if (it == values.end()) {
        error = "missing LLVM value for IR value %" + std::to_string(id);
        return nullptr;
    }
    return it->second;
}

// NaN-boxing: encode a native int64_t into the QoreValue i64 representation.
// For inline ints: bits = TAG_INT48 | (val & PAYLOAD_MASK)
// For out-of-range: would need runtime call (not handled here; typed .int ops stay in range)
llvm::Value* QoreIRToLLVM::boxInt(llvm::Value* int_val) {
    // Mask to 48 bits
    llvm::Value* masked = builder->CreateAnd(int_val, llvm::ConstantInt::get(i64_type, PAYLOAD_MASK));
    // OR with tag
    return builder->CreateOr(masked, llvm::ConstantInt::get(i64_type, TAG_INT48));
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

void QoreIRToLLVM::collectLocals(const QoreIRFunction& func) {
    function_locals.clear();
    std::unordered_set<const void*> seen;
    for (const auto& block : func.blocks) {
        for (const auto& inst_ptr : block->instructions) {
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            if (inst->opcode == QoreIROpcode::LoadLocal || inst->opcode == QoreIROpcode::StoreLocal) {
                const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
                if (linst->local && seen.insert(linst->local).second) {
                    function_locals.push_back(linst->local);
                }
            }
        }
    }
}

void QoreIRToLLVM::emitLocalInstantiation(llvm::Module& module) {
    if (aot_mode) {
        auto helper = module.getOrInsertFunction("qore_rt_instantiate_local_aot",
                llvm::FunctionType::get(void_type, {ptr_type, i32_type}, false));
        for (LocalVar* var : function_locals) {
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
        for (LocalVar* var : function_locals) {
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

void QoreIRToLLVM::emitLocalUninstantiation(llvm::Module& module) {
    if (aot_mode) {
        auto helper = module.getOrInsertFunction("qore_rt_uninstantiate_local_aot",
                llvm::FunctionType::get(void_type, {ptr_type, i32_type, ptr_type}, false));
        for (auto it = function_locals.rbegin(); it != function_locals.rend(); ++it) {
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
                llvm::FunctionType::get(void_type, {ptr_type}, false));
        for (auto it = function_locals.rbegin(); it != function_locals.rend(); ++it) {
            if (pre_instantiated_locals &&
                    pre_instantiated_locals->count(reinterpret_cast<const void*>(*it))) {
                continue;
            }
            builder->CreateCall(helper, {xsink_arg});
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
    if (preinstantiated_entry_loads.empty()) {
        return;
    }
    auto helper = module.getOrInsertFunction("qore_rt_decref",
            llvm::FunctionType::get(void_type, {i64_type, ptr_type}, false));
    for (llvm::Value* entry_val : preinstantiated_entry_loads) {
        builder->CreateCall(helper, {entry_val, xsink_arg});
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

void QoreIRToLLVM::trackResultForCleanup(llvm::Value* result, uint32_t result_id,
        llvm::Function* llvm_func) {
    llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
    llvm::IRBuilder<> alloca_builder(entry, entry->begin());
    llvm::AllocaInst* cleanup_alloca = alloca_builder.CreateAlloca(i64_type,
            nullptr, "cleanup");
    alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
            cleanup_alloca);
    builder->CreateStore(result, cleanup_alloca);
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

void QoreIRToLLVM::reloadAllLocalsFromRuntime(llvm::Module& module, llvm::Function* llvm_func) {
    for (auto& [key, alloca] : local_allocas) {
        reloadLocalFromRuntime(key, module, llvm_func);
    }
}

llvm::Value* QoreIRToLLVM::boxValue(llvm::Value* val, uint32_t id) {
    if (nanboxed_values.count(id)) {
        return val;  // Already NaN-boxed
    }
    if (val->getType() == i64_type) {
        return boxInt(val);
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
        llvm::DIFile* f = di_builder->createFile("<jit>", ".");
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

    llvm::DIFile* f = di_builder->createFile(filename, dir);
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

void QoreIRToLLVM::emitExceptionCheck(llvm::Module& module, llvm::Function* llvm_func,
        const QoreIRInstruction* inst) {
    if (!inst->exception_target) {
        return;
    }
    auto except_it = block_map.find(inst->exception_target);
    if (except_it == block_map.end()) {
        return;
    }
    auto has_ex = module.getOrInsertFunction("qore_rt_has_exception",
            llvm::FunctionType::get(i64_type, {ptr_type}, false));
    llvm::Value* ex_check = builder->CreateCall(has_ex, {xsink_arg});
    llvm::Value* has_exception = builder->CreateICmpNE(ex_check,
            llvm::ConstantInt::get(i64_type, 0));
    llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "no_exception", llvm_func);
    builder->CreateCondBr(has_exception, except_it->second, cont);
    builder->SetInsertPoint(cont);
}

bool QoreIRToLLVM::lowerFunction(const QoreIRFunction& func, llvm::Module& module, std::string& error) {
    initTypes();
    declareRuntimeHelpers(module);

    // Function signature depends on AOT mode:
    //   JIT mode: uint64_t fname(ExceptionSink* xsink)
    //   AOT mode: uint64_t fname(QoreAOTContext* ctx, ExceptionSink* xsink)
    llvm::FunctionType* fn_type;
    if (aot_mode) {
        fn_type = llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false);
    } else {
        fn_type = llvm::FunctionType::get(i64_type, {ptr_type}, false);
    }
    llvm::Function* llvm_func = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
            func.name, module);
    if (!llvm_func) {
        error = "failed to create LLVM function '" + func.name + "'";
        return false;
    }

    // Name parameters
    if (aot_mode) {
        aot_ctx_arg = llvm_func->getArg(0);
        aot_ctx_arg->setName("ctx");
        xsink_arg = llvm_func->getArg(1);
        xsink_arg->setName("xsink");
    } else {
        xsink_arg = llvm_func->getArg(0);
        xsink_arg->setName("xsink");
    }

    // Propagate pre-instantiated locals set
    pre_instantiated_locals = func.pre_instantiated_locals.empty()
        ? nullptr : &func.pre_instantiated_locals;

    // Phase 5c: Set up DWARF debug info
    di_builder = std::make_unique<llvm::DIBuilder>(module);
    di_file_cache.clear();

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

    // Create compile unit with a custom language ID for Qore
    di_cu = di_builder->createCompileUnit(
        llvm::dwarf::DW_LANG_lo_user,  // custom Qore language
        di_file,
        "Qore JIT",                    // producer
        false,                          // isOptimized
        "",                             // flags
        0                               // runtime version
    );

    // Create subroutine type (opaque — JIT ABI is uint64_t(ExceptionSink*))
    llvm::DISubroutineType* di_func_type = di_builder->createSubroutineType(
        di_builder->getOrCreateTypeArray({}));

    // Create subprogram for this function
    di_sp = di_builder->createFunction(
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
    block_map.reserve(func.blocks.size());
    for (const auto& block : func.blocks) {
        block_map[block.get()] = llvm::BasicBlock::Create(ctx, block->name, llvm_func);
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
    pending_phis.clear();
    local_reload_trackers.clear();

    // Collect all unique LocalVar* pointers from the function and emit
    // instantiation calls at the start of the entry block so the Qore
    // thread-local variable stack is properly set up before any code runs.
    // Pre-instantiated locals (tiered compilation) are skipped.
    collectLocals(func);
    obe_saved_count = nullptr;
    if (!func.blocks.empty()) {
        builder->SetInsertPoint(block_map[func.blocks.front().get()]);
        emitLocalInstantiation(module);

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
        builder->SetInsertPoint(llvm_block);

        for (const auto& inst_ptr : block->instructions) {
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            // If the current block already has a terminator (e.g., from an Invoke that
            // created a conditional branch), skip remaining instructions in this block.
            if (builder->GetInsertBlock()->getTerminator()) {
                break;
            }
            // Phase 5c: Set debug location for this instruction
            setDebugLocation(inst);
            if (!lowerInstruction(inst, llvm_func, module, error)) {
                return false;
            }
        }

        // Verify the final insert block has a terminator.
        // Note: the insert block may have changed (e.g., guards create continuation blocks).
        // We check the original block; if it was terminated by Invoke or similar, that's fine.
        if (!llvm_block->getTerminator()) {
            // The builder may have moved to a guard continuation block; check that too
            if (!builder->GetInsertBlock()->getTerminator()) {
                error = "missing terminator in lowered block '" + block->name + "'";
                return false;
            }
        }
    }

    // PHI fixup pass: add incoming values now that all blocks are lowered
    for (auto& [phi_node, phi_inst] : pending_phis) {
        for (const auto& inc : phi_inst->incoming) {
            llvm::Value* val = getVal(inc.value.id, error);
            if (!val) {
                return false;
            }
            // Box to i64 if needed (PHI type is i64 for NaN-boxed values)
            val = boxValue(val, inc.value.id);
            llvm::BasicBlock* bb = block_map[inc.block];
            if (!bb) {
                error = "PHI incoming block not found";
                return false;
            }
            phi_node->addIncoming(val, bb);
        }
    }
    pending_phis.clear();

    // Phase 5c: Finalize debug info before verification
    if (di_builder) {
        di_builder->finalize();
    }

    // Verify the generated LLVM IR
    std::string verify_error;
    llvm::raw_string_ostream verify_os(verify_error);
    if (llvm::verifyFunction(*llvm_func, &verify_os)) {
        error = "LLVM verification failed: " + verify_error;
        return false;
    }

    // Phase 5c: Reset debug info state for next function
    di_builder.reset();
    di_cu = nullptr;
    di_sp = nullptr;
    di_file_cache.clear();

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
            return true;
        }
        case QoreIROpcode::ConstNull: {
            values[inst->result.id] = llvm::ConstantInt::get(i64_type, VAL_NULL);
            return true;
        }

        // === Typed integer arithmetic (native i64) ===
        case QoreIROpcode::AddInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAdd(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::SubInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateSub(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::MulInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateMul(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::DivInt: {
            // Division by zero must be checked at runtime via helper
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            auto helper = module.getOrInsertFunction("qore_rt_div_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {unboxInt(lhs), unboxInt(rhs), xsink_arg});
            return true;
        }
        case QoreIROpcode::ModInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            auto helper = module.getOrInsertFunction("qore_rt_mod_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {unboxInt(lhs), unboxInt(rhs), xsink_arg});
            return true;
        }

        // === Typed float arithmetic (native double) ===
        case QoreIROpcode::AddFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFAdd(lhs, rhs);
            return true;
        }
        case QoreIROpcode::SubFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFSub(lhs, rhs);
            return true;
        }
        case QoreIROpcode::MulFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFMul(lhs, rhs);
            return true;
        }
        case QoreIROpcode::DivFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            auto helper = module.getOrInsertFunction("qore_rt_div_float",
                    llvm::FunctionType::get(double_type, {double_type, double_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {lhs, rhs, xsink_arg});
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

        // === Bitwise integer operations ===
        case QoreIROpcode::AndInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAnd(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::OrInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateOr(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::XorInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateXor(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::ShlInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateShl(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::ShrInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAShr(unboxInt(lhs), unboxInt(rhs));
            return true;
        }

        // === Unary operations ===
        case QoreIROpcode::UnaryMinusInt: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            values[inst->result.id] = builder->CreateNeg(unboxInt(val));
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
            values[inst->result.id] = builder->CreateICmpEQ(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::NeInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpNE(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::LtInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpSLT(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::LeInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpSLE(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::GtInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpSGT(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::GeInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateICmpSGE(unboxInt(lhs), unboxInt(rhs));
            return true;
        }

        // === Typed float comparisons (native double → i1) ===
        case QoreIROpcode::EqFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFCmpOEQ(lhs, rhs);
            return true;
        }
        case QoreIROpcode::NeFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFCmpONE(lhs, rhs);
            return true;
        }
        case QoreIROpcode::LtFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFCmpOLT(lhs, rhs);
            return true;
        }
        case QoreIROpcode::LeFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFCmpOLE(lhs, rhs);
            return true;
        }
        case QoreIROpcode::GtFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFCmpOGT(lhs, rhs);
            return true;
        }
        case QoreIROpcode::GeFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFCmpOGE(lhs, rhs);
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
            auto it = local_allocas.find(key);
            if (it == local_allocas.end()) {
                // Create alloca in entry block for this local
                llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                llvm::AllocaInst* alloca = alloca_builder.CreateAlloca(i64_type, nullptr, "local");
                // For pre-instantiated locals (tiered compilation: params, argvid, selfid,
                // body locals), initialize from the Qore runtime stack so the JIT sees
                // the values set up by the calling convention.  For other locals (nested
                // block vars), initialize to NOTHING (they're instantiated by
                // emitLocalInstantiation which runs after these entry-block allocas).
                if (linst->local && pre_instantiated_locals &&
                        pre_instantiated_locals->count(key)) {
                    if (aot_mode) {
                        auto load_fn = module.getOrInsertFunction("qore_rt_load_local_aot",
                            llvm::FunctionType::get(i64_type, {ptr_type, i32_type, ptr_type}, false));
                        int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                        llvm::Value* init_val = alloca_builder.CreateCall(load_fn,
                            {aot_ctx_arg, llvm::ConstantInt::get(i32_type, slot), xsink_arg});
                        alloca_builder.CreateStore(init_val, alloca);
                        preinstantiated_entry_loads.push_back(init_val);
                    } else {
                        auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                        llvm::Value* var_as_ptr = alloca_builder.CreateIntToPtr(var_ptr, ptr_type);
                        llvm::Value* init_val = alloca_builder.CreateCall(load_fn,
                            {var_as_ptr, xsink_arg});
                        alloca_builder.CreateStore(init_val, alloca);
                        preinstantiated_entry_loads.push_back(init_val);
                    }
                } else {
                    alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), alloca);
                }
                local_allocas[key] = alloca;
                it = local_allocas.find(key);
            }
            values[inst->result.id] = builder->CreateLoad(i64_type, it->second);
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::StoreLocal: {
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            auto key = reinterpret_cast<const void*>(linst->local);
            auto it = local_allocas.find(key);
            if (it == local_allocas.end()) {
                llvm::BasicBlock* entry = &llvm_func->getEntryBlock();
                llvm::IRBuilder<> alloca_builder(entry, entry->begin());
                llvm::AllocaInst* alloca = alloca_builder.CreateAlloca(i64_type, nullptr, "local");
                alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), alloca);
                local_allocas[key] = alloca;
                it = local_allocas.find(key);
            }
            // Box the value to NaN-boxed i64.  Values already NaN-boxed (from Invoke,
            // CatchException, make_string, .any ops, LoadLocal) must not be re-boxed.
            llvm::Value* boxed;
            if (nanboxed_values.count(inst->operands[0].id)) {
                boxed = val;  // Already NaN-boxed
            } else if (val->getType() == double_type) {
                boxed = boxFloat(val);
            } else if (val->getType() == i1_type) {
                boxed = boxBool(val);
            } else if (val->getType() == i64_type) {
                boxed = boxInt(val);
            } else {
                error = "unsupported type for StoreLocal";
                return false;
            }
            builder->CreateStore(boxed, it->second);
            if (inst->result.isValid()) {
                values[inst->result.id] = boxed;
            }
            // Sync to Qore thread-local variable stack so AST callbacks can resolve this local
            if (linst->local) {
                if (aot_mode) {
                    auto assign_helper = module.getOrInsertFunction("qore_rt_assign_local_aot",
                            llvm::FunctionType::get(void_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
                    int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getLocalSlot(key);
                    builder->CreateCall(assign_helper, {aot_ctx_arg,
                            llvm::ConstantInt::get(i32_type, slot), boxed, xsink_arg});
                } else {
                    auto assign_helper = module.getOrInsertFunction("qore_rt_assign_local",
                            llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                            reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                    builder->CreateCall(assign_helper, {var_as_ptr, boxed, xsink_arg});
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
            // Call guard helper
            auto helper = module.getOrInsertFunction("qore_rt_guard_not_nothing",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* guard_result = builder->CreateCall(helper, {boxed});
            llvm::Value* guard_pass = builder->CreateICmpNE(guard_result,
                    llvm::ConstantInt::get(i64_type, 0));
            // Branch: pass → continue (next instruction in same block),
            //         fail → deopt_target
            if (ginst->deopt_target) {
                auto deopt_it = block_map.find(ginst->deopt_target);
                if (deopt_it == block_map.end()) {
                    error = "guard deopt target block not found";
                    return false;
                }
                // Create a continuation block
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                builder->CreateCondBr(guard_pass, cont, deopt_it->second);
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
            auto helper = module.getOrInsertFunction("qore_rt_guard_int",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* guard_result = builder->CreateCall(helper, {boxed});
            llvm::Value* guard_pass = builder->CreateICmpNE(guard_result,
                    llvm::ConstantInt::get(i64_type, 0));
            if (ginst->deopt_target) {
                auto deopt_it = block_map.find(ginst->deopt_target);
                if (deopt_it == block_map.end()) {
                    error = "guard deopt target block not found";
                    return false;
                }
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                builder->CreateCondBr(guard_pass, cont, deopt_it->second);
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
            auto helper = module.getOrInsertFunction("qore_rt_guard_float",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* guard_result = builder->CreateCall(helper, {boxed});
            llvm::Value* guard_pass = builder->CreateICmpNE(guard_result,
                    llvm::ConstantInt::get(i64_type, 0));
            if (ginst->deopt_target) {
                auto deopt_it = block_map.find(ginst->deopt_target);
                if (deopt_it == block_map.end()) {
                    error = "guard deopt target block not found";
                    return false;
                }
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                builder->CreateCondBr(guard_pass, cont, deopt_it->second);
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
                // NaN-boxed boolean: VAL_FALSE (0xFFFB000000000002) is non-zero,
                // so we must compare against VAL_TRUE rather than against 0.
                // This path is reached when lowerConditionValue() determines the
                // expression is already boolean (e.g. a comparison operator) and
                // skips the ToBool wrapper.
                printd(3, "BranchIf: condition %%%d is NaN-boxed i64 -> compare against VAL_TRUE\n", br->condition.id);
                cond = builder->CreateICmpEQ(cond_val,
                        llvm::ConstantInt::get(i64_type, VAL_TRUE));
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
                    boxed_ret = boxInt(val);
                } else if (val->getType() == double_type) {
                    boxed_ret = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed_ret = boxBool(val);
                } else {
                    error = "unsupported return value type for LLVM lowering";
                    return false;
                }
                // Take a reference to the return value before cleanup.
                auto incref_fn = module.getOrInsertFunction("qore_rt_incref",
                        llvm::FunctionType::get(void_type, {i64_type}, false));
                builder->CreateCall(incref_fn, {boxed_ret});

                // If returning an Invoke/ConstString result directly, clear its
                // cleanup alloca so emitInvokeCleanup won't double-decref.
                auto alloca_it = invoke_alloca_map.find(ret->value.id);
                if (alloca_it != invoke_alloca_map.end()) {
                    builder->CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING),
                            alloca_it->second);
                }
            }
            // Execute on_block_exit handlers before cleanup
            emitOnBlockExitExec(module);
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

                // Alloca for args array
                llvm::Value* args_array = builder->CreateAlloca(i64_type,
                        llvm::ConstantInt::get(i32_type, nargs));
                for (int i = 0; i < nargs; ++i) {
                    auto* arg_val = getVal(inv->operands[arg_start + i].id, error);
                    if (!arg_val) { return false; }
                    llvm::Value* arg_boxed = boxValue(arg_val,
                            inv->operands[arg_start + i].id);
                    llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                            llvm::ConstantInt::get(i32_type, i));
                    builder->CreateStore(arg_boxed, gep);
                }

                QoreValue expr_val = inv->expr;
                uint64_t expr_bits;
                std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));

                if (aot_mode) {
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
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(result, inst->result.id, llvm_func);

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

        // === Landing pad (no-op in our model; exception state is in ExceptionSink) ===
        case QoreIROpcode::LandingPad: {
            // In our JIT model, landing pads don't need special LLVM lowering
            // since we use ExceptionSink polling rather than C++ exceptions.
            return true;
        }

        // === Catch exception ===
        case QoreIROpcode::CatchException: {
            auto helper = module.getOrInsertFunction("qore_rt_catch_exception",
                    llvm::FunctionType::get(i64_type, {ptr_type}, false));
            llvm::Value* catch_result = builder->CreateCall(helper, {xsink_arg});
            values[inst->result.id] = catch_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(catch_result, inst->result.id, llvm_func);
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
            emitPreinstantiatedCleanup(module);
            emitInvokeCleanup(module);
            emitLocalUninstantiation(module);
            builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            return true;
        }

        // === Rethrow ===
        case QoreIROpcode::Rethrow: {
            // Rethrow: exception is already in the ExceptionSink.
            // Execute on_block_exit handlers, then return NOTHING; caller sees the exception.
            emitOnBlockExitExec(module);
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
                // Use pre-evaluated operands with qore_rt_call_with_args
                int arg_start = (inst->opcode == QoreIROpcode::CallIndirect) ? 1 : 0;
                int nargs = static_cast<int>(inst->operands.size()) - arg_start;

                llvm::Value* args_array = builder->CreateAlloca(i64_type,
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

                if (aot_mode) {
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

            // Calls can modify local variables through side effects;
            // reload all local allocas from the runtime variable stack
            reloadAllLocalsFromRuntime(module, llvm_func);

            values[inst->result.id] = call_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(call_result, inst->result.id, llvm_func);
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
        // CmpAny, CmpInt, CmpFloat, EqHard, NeHard: stay on generic path (complex semantics)
        case QoreIROpcode::CmpAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            auto helper = module.getOrInsertFunction("qore_rt_comparison_op",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper,
                    {opcode_val, lhs_boxed, rhs_boxed, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Dynamic bitwise/shift operations ===
        case QoreIROpcode::AndAny:
        case QoreIROpcode::OrAny:
        case QoreIROpcode::XorAny:
        case QoreIROpcode::ShlAny:
        case QoreIROpcode::ShrAny: {
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

        // === Dynamic unary operations ===
        case QoreIROpcode::UnaryMinusAny:
        case QoreIROpcode::UnaryPlusAny: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            auto helper = module.getOrInsertFunction("qore_rt_unary_op",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper,
                    {opcode_val, val_boxed, xsink_arg});
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
            llvm::Value* result;
            if (aot_mode) {
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getExprSlot(lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_store_aot",
                        llvm::FunctionType::get(i64_type, {ptr_type, i32_type, i64_type, ptr_type}, false));
                result = builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), val_boxed, xsink_arg});
            } else {
                llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
                auto helper = module.getOrInsertFunction("qore_rt_lvalue_store",
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
        case QoreIROpcode::DivAssignLValue:
        case QoreIROpcode::ModAssignLValue:
        case QoreIROpcode::AndAssignLValue:
        case QoreIROpcode::OrAssignLValue:
        case QoreIROpcode::XorAssignLValue:
        case QoreIROpcode::ShlAssignLValue:
        case QoreIROpcode::ShrAssignLValue:
        case QoreIROpcode::SpliceLValue: {
            const auto* lvinst = static_cast<const QoreIRLValueInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            QoreValue lv = lvinst->lvalue;
            uint64_t lv_bits;
            std::memcpy(&lv_bits, &lv, sizeof(lv_bits));
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
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

        // === Container construction ===
        case QoreIROpcode::MakeList: {
            // Allocate stack array and fill with NaN-boxed operand values
            int count = static_cast<int>(inst->operands.size());
            llvm::Value* count_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), count);
            llvm::Value* arr = builder->CreateAlloca(i64_type,
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
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::MakeHash: {
            // Operands are alternating keys and values
            int pair_count = static_cast<int>(inst->operands.size() / 2);
            int total = static_cast<int>(inst->operands.size());
            llvm::Value* count_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), pair_count);
            llvm::Value* arr = builder->CreateAlloca(i64_type,
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
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Compound assignment operations (binary via runtime) ===
        case QoreIROpcode::ShlAssignInt:
        case QoreIROpcode::ShlAssignAny:
        case QoreIROpcode::ShrAssignInt:
        case QoreIROpcode::ShrAssignAny:
        case QoreIROpcode::AddAssignInt:
        case QoreIROpcode::AddAssignFloat:
        case QoreIROpcode::AddAssignAny:
        case QoreIROpcode::SubAssignInt:
        case QoreIROpcode::SubAssignFloat:
        case QoreIROpcode::SubAssignAny:
        case QoreIROpcode::MulAssignInt:
        case QoreIROpcode::MulAssignFloat:
        case QoreIROpcode::MulAssignAny:
        case QoreIROpcode::DivAssignInt:
        case QoreIROpcode::DivAssignFloat:
        case QoreIROpcode::DivAssignAny:
        case QoreIROpcode::ModAssignInt:
        case QoreIROpcode::ModAssignAny:
        case QoreIROpcode::AndAssignInt:
        case QoreIROpcode::AndAssignAny:
        case QoreIROpcode::OrAssignInt:
        case QoreIROpcode::OrAssignAny:
        case QoreIROpcode::XorAssignInt:
        case QoreIROpcode::XorAssignAny:
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
        case QoreIROpcode::RangeDate:
        case QoreIROpcode::RangeSliceAny:
        case QoreIROpcode::RangeSliceInt:
        case QoreIROpcode::RangeSliceFloat: {
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

        // === Expression-based operations (delegate to runtime via expr) ===
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
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::RegexSubstString:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::TrimAny:
        case QoreIROpcode::TrimString:
        case QoreIROpcode::ChompAny:
        case QoreIROpcode::ChompString:
        case QoreIROpcode::TransliterateAny:
        case QoreIROpcode::TransliterateString:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::ListAssignAny:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::ElementsInt: {
            // Non-DotEval expression ops — delegate to qore_rt_invoke_expr via the AST node
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
        case QoreIROpcode::CastAny:
        case QoreIROpcode::CastList:
        case QoreIROpcode::CastHash:
        case QoreIROpcode::CastObject:
        case QoreIROpcode::CastEnum:
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
            llvm::Value* code_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(code));
            llvm::Value* code_as_ptr = builder->CreateIntToPtr(code_ptr, ptr_type);
            auto helper = module.getOrInsertFunction("qore_rt_push_on_block_exit",
                    llvm::FunctionType::get(void_type,
                        {llvm::Type::getInt32Ty(ctx), ptr_type}, false));
            builder->CreateCall(helper, {type_val, code_as_ptr});
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
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::ThreadExit: {
            auto helper = module.getOrInsertFunction("qore_rt_thread_exit",
                    llvm::FunctionType::get(void_type, {ptr_type}, false));
            builder->CreateCall(helper, {xsink_arg});
            // Thread exit raises an exception; execute on_block_exit, uninstantiate locals and return
            emitOnBlockExitExec(module);
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
                auto deopt_it = block_map.find(ginst->deopt_target);
                if (deopt_it == block_map.end()) {
                    error = "guard deopt target block not found";
                    return false;
                }
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                builder->CreateCondBr(guard_pass, cont, deopt_it->second);
                builder->SetInsertPoint(cont);
            }
            if (inst->result.isValid()) {
                values[inst->result.id] = guard_pass;
            }
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
    const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
    if (!expr_inst->expr.hasNode()) {
        return false;
    }

    // Check if the expression is a QoreHashObjectDereferenceOperatorNode
    const AbstractQoreNode* node = expr_inst->expr.getInternalNode();
    if (node->getType() != NT_OPERATOR) {
        return false;
    }

    auto* hash_deref = dynamic_cast<const QoreHashObjectDereferenceOperatorNode*>(node);
    if (!hash_deref) {
        return false;
    }

    // Check if the right side is a constant string key (not a list/hash slice)
    QoreValue right_val = hash_deref->getRight();
    if (!right_val.hasNode() || right_val.getType() != NT_STRING) {
        return false;
    }

    const QoreStringNode* key_node = right_val.get<const QoreStringNode>();
    const char* key_str = key_node->c_str();

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
    if (nanboxed_values.count(inst->operands[1].id)) {
        idx_int = unboxInt(idx_val);
    } else if (idx_val->getType() == i64_type) {
        idx_int = idx_val;
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
    llvm::Value* int_boxed = boxInt(int_result);
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

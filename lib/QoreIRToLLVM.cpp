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
#include "qore/intern/CaseNodeRegex.h"
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
#include <llvm/IR/MDBuilder.h>
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

    // Deopt helper: void qore_rt_deopt(void* deopt_counter_ptr)
    module.getOrInsertFunction("qore_rt_deopt",
            llvm::FunctionType::get(void_type, {ptr_type}, false));

    // Local variable helpers
    module.getOrInsertFunction("qore_rt_instantiate_local",
            llvm::FunctionType::get(void_type, {ptr_type}, false));
    module.getOrInsertFunction("qore_rt_assign_local",
            llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
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

    // Iterator helpers for foreach loops
    // iterator_create: (i64, ptr, ptr) -> ptr
    module.getOrInsertFunction("qore_rt_iterator_create",
            llvm::FunctionType::get(ptr_type, {i64_type, ptr_type, ptr_type}, false));
    // iterator_next: (ptr, ptr, ptr) -> i64
    module.getOrInsertFunction("qore_rt_iterator_next",
            llvm::FunctionType::get(i64_type, {ptr_type, ptr_type, ptr_type}, false));
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
    entry_locals.clear();
    entry_locals_set.clear();
    instantiated_non_entry_locals.clear();
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
                    function_locals.push_back(linst->local);
                    // Track if this local is first accessed in the entry block
                    if (is_first_block) {
                        entry_locals.push_back(linst->local);
                        entry_locals_set.insert(reinterpret_cast<const void*>(linst->local));
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
        llvm::AllocaInst* alloca = alloca_builder.CreateAlloca(i64_type, nullptr, "local");
        if (pre_instantiated_locals && pre_instantiated_locals->count(key)) {
            // Pre-instantiated: initialize from runtime stack
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
                    reinterpret_cast<uint64_t>(var));
                llvm::Value* var_as_ptr = alloca_builder.CreateIntToPtr(var_ptr, ptr_type);
                llvm::Value* init_val = alloca_builder.CreateCall(load_fn,
                    {var_as_ptr, xsink_arg});
                alloca_builder.CreateStore(init_val, alloca);
                preinstantiated_entry_loads.push_back(init_val);
            }
        } else {
            // Body local: initialize to NOTHING
            alloca_builder.CreateStore(llvm::ConstantInt::get(i64_type, VAL_NOTHING), alloca);
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
    if (getenv("QORE_LLVM_DEBUG")) {
        fprintf(stderr, "LLVM-EXCEPT-CHECK: creating no_exception block, moving from %s to %s\n",
                builder->GetInsertBlock()->getName().str().c_str(),
                cont->getName().str().c_str());
        fflush(stderr);
    }
    builder->CreateCondBr(has_exception, except_it->second, cont);
    builder->SetInsertPoint(cont);
}

bool QoreIRToLLVM::lowerFunction(const QoreIRFunction& func, llvm::Module& module, std::string& error) {
    current_ir_func = &func;
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

    // RAII cleanup: remove incomplete function from module on failure
    struct FunctionCleanup {
        llvm::Function* func;
        bool committed = false;
        ~FunctionCleanup() {
            if (!committed && func) {
                func->eraseFromParent();
            }
        }
    } func_cleanup{llvm_func};

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
    pending_phis.clear();
    local_reload_trackers.clear();

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
                fprintf(stderr, "PHI-FIXUP: incoming from block=%s insert_before_term=%s\n",
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

    // Phase 5c: Finalize debug info before verification
    if (di_builder) {
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
            // Phase 2E: Inline zero-check with native division for non-zero case
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* l_int = unboxInt(lhs);
            llvm::Value* r_int = unboxInt(rhs);
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
            llvm::Value* l_int = unboxInt(lhs);
            llvm::Value* r_int = unboxInt(rhs);
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
        case QoreIROpcode::AddFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            // Unbox if NaN-boxed (operands may come from LoadLocal which returns i64)
            llvm::Value* l_float = nanboxed_values.count(inst->operands[0].id) ? unboxFloat(lhs) : lhs;
            llvm::Value* r_float = nanboxed_values.count(inst->operands[1].id) ? unboxFloat(rhs) : rhs;
            values[inst->result.id] = builder->CreateFAdd(l_float, r_float);
            return true;
        }
        case QoreIROpcode::SubFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            // Unbox if NaN-boxed (operands may come from LoadLocal which returns i64)
            llvm::Value* l_float = nanboxed_values.count(inst->operands[0].id) ? unboxFloat(lhs) : lhs;
            llvm::Value* r_float = nanboxed_values.count(inst->operands[1].id) ? unboxFloat(rhs) : rhs;
            values[inst->result.id] = builder->CreateFSub(l_float, r_float);
            return true;
        }
        case QoreIROpcode::MulFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            // Unbox if NaN-boxed (operands may come from LoadLocal which returns i64)
            llvm::Value* l_float = nanboxed_values.count(inst->operands[0].id) ? unboxFloat(lhs) : lhs;
            llvm::Value* r_float = nanboxed_values.count(inst->operands[1].id) ? unboxFloat(rhs) : rhs;
            values[inst->result.id] = builder->CreateFMul(l_float, r_float);
            return true;
        }
        case QoreIROpcode::DivFloat: {
            // Phase 2E: Inline zero-check with native division for non-zero case
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            // Unbox if NaN-boxed (operands may come from LoadLocal which returns i64)
            llvm::Value* l_float = nanboxed_values.count(inst->operands[0].id) ? unboxFloat(lhs) : lhs;
            llvm::Value* r_float = nanboxed_values.count(inst->operands[1].id) ? unboxFloat(rhs) : rhs;
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
            int nargs = static_cast<int>(inst->operands.size());
            llvm::Value* args_array = builder->CreateAlloca(i64_type,
                    llvm::ConstantInt::get(i32_type, nargs));
            for (int i = 0; i < nargs; ++i) {
                auto* val = getVal(inst->operands[i].id, error);
                if (!val) { return false; }
                llvm::Value* boxed = boxValue(val, inst->operands[i].id);
                llvm::Value* ptr = builder->CreateGEP(i64_type, args_array,
                        llvm::ConstantInt::get(i32_type, i));
                builder->CreateStore(boxed, ptr);
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

            // Skip if this was pre-instantiated by the caller (will be cleaned up by caller)
            if (pre_instantiated_locals && pre_instantiated_locals->count(key)) {
                return true;
            }

            // Skip entry-block locals - they are uninstantiated by emitLocalUninstantiation
            // at function return, not by explicit UninstantiateLocal instructions
            if (entry_locals_set.count(key)) {
                return true;
            }

            // Skip non-entry locals that were never instantiated (never had a first store).
            // This check is critical: if StoreLocal was never executed for this local
            // (e.g., control flow skipped the declaration), we must not uninstantiate it.
            if (instantiated_non_entry_locals.count(key) == 0) {
                return true;
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
            // Branch: pass → continue (next instruction in same block),
            //         fail → deopt_target (via deopt counter block if profiled)
            if (ginst->deopt_target) {
                auto deopt_it = block_map.find(ginst->deopt_target);
                if (deopt_it == block_map.end()) {
                    error = "guard deopt target block not found";
                    return false;
                }
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                llvm::BasicBlock* fail_target = deopt_it->second;
                // When profiled and deopt counter is set, create intermediate block to track failures
                if (profile_hot && deopt_counter_ptr) {
                    llvm::BasicBlock* deopt_block = llvm::BasicBlock::Create(ctx, "guard_deopt", llvm_func);
                    auto* weights = llvm::MDBuilder(ctx).createBranchWeights(999, 1);
                    builder->CreateCondBr(guard_pass, cont, deopt_block, weights);
                    builder->SetInsertPoint(deopt_block);
                    auto deopt_fn = module.getOrInsertFunction("qore_rt_deopt",
                            llvm::FunctionType::get(void_type, {ptr_type}, false));
                    llvm::Value* counter_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(deopt_counter_ptr)),
                        ptr_type);
                    builder->CreateCall(deopt_fn, {counter_ptr});
                    builder->CreateBr(fail_target);
                } else if (profile_hot) {
                    auto* weights = llvm::MDBuilder(ctx).createBranchWeights(999, 1);
                    builder->CreateCondBr(guard_pass, cont, fail_target, weights);
                } else {
                    builder->CreateCondBr(guard_pass, cont, fail_target);
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
                auto deopt_it = block_map.find(ginst->deopt_target);
                if (deopt_it == block_map.end()) {
                    error = "guard deopt target block not found";
                    return false;
                }
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                llvm::BasicBlock* fail_target = deopt_it->second;
                if (profile_hot && deopt_counter_ptr) {
                    llvm::BasicBlock* deopt_block = llvm::BasicBlock::Create(ctx, "guard_deopt", llvm_func);
                    auto* weights = llvm::MDBuilder(ctx).createBranchWeights(999, 1);
                    builder->CreateCondBr(guard_pass, cont, deopt_block, weights);
                    builder->SetInsertPoint(deopt_block);
                    auto deopt_fn = module.getOrInsertFunction("qore_rt_deopt",
                            llvm::FunctionType::get(void_type, {ptr_type}, false));
                    llvm::Value* counter_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(deopt_counter_ptr)),
                        ptr_type);
                    builder->CreateCall(deopt_fn, {counter_ptr});
                    builder->CreateBr(fail_target);
                } else if (profile_hot) {
                    auto* weights = llvm::MDBuilder(ctx).createBranchWeights(999, 1);
                    builder->CreateCondBr(guard_pass, cont, fail_target, weights);
                } else {
                    builder->CreateCondBr(guard_pass, cont, fail_target);
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
                auto deopt_it = block_map.find(ginst->deopt_target);
                if (deopt_it == block_map.end()) {
                    error = "guard deopt target block not found";
                    return false;
                }
                llvm::BasicBlock* cont = llvm::BasicBlock::Create(ctx, "guard_pass", llvm_func);
                llvm::BasicBlock* fail_target = deopt_it->second;
                if (profile_hot && deopt_counter_ptr) {
                    llvm::BasicBlock* deopt_block = llvm::BasicBlock::Create(ctx, "guard_deopt", llvm_func);
                    auto* weights = llvm::MDBuilder(ctx).createBranchWeights(999, 1);
                    builder->CreateCondBr(guard_pass, cont, deopt_block, weights);
                    builder->SetInsertPoint(deopt_block);
                    auto deopt_fn = module.getOrInsertFunction("qore_rt_deopt",
                            llvm::FunctionType::get(void_type, {ptr_type}, false));
                    llvm::Value* counter_ptr = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(deopt_counter_ptr)),
                        ptr_type);
                    builder->CreateCall(deopt_fn, {counter_ptr});
                    builder->CreateBr(fail_target);
                } else if (profile_hot) {
                    auto* weights = llvm::MDBuilder(ctx).createBranchWeights(999, 1);
                    builder->CreateCondBr(guard_pass, cont, fail_target, weights);
                } else {
                    builder->CreateCondBr(guard_pass, cont, fail_target);
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
                val_i64 = unboxInt(switch_val);
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
            llvm::Value* arr_ptr = builder->CreateBitCast(case_arr,
                    llvm::PointerType::get(ptr_type, 0));

            // Call runtime helper: int32_t qore_rt_switch_string_lookup(uint64_t, const char**, int32_t)
            auto helper = module.getOrInsertFunction("qore_rt_switch_string_lookup",
                    llvm::FunctionType::get(i32_type,
                            {i64_type, llvm::PointerType::get(ptr_type, 0), i32_type}, false));
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

        // === CallMethodDirect (devirtualized method call) ===
        case QoreIROpcode::CallMethodDirect: {
            const auto* direct_inst = static_cast<const QoreIRCallMethodDirectInstruction*>(inst);

            // Build args array from operands
            int nargs = static_cast<int>(inst->operands.size());

            llvm::Value* args_array;
            if (nargs > 0) {
                args_array = builder->CreateAlloca(i64_type,
                        llvm::ConstantInt::get(i32_type, nargs));
                for (int i = 0; i < nargs; ++i) {
                    auto* arg_val = getVal(inst->operands[i].id, error);
                    if (!arg_val) { return false; }
                    llvm::Value* arg_boxed = boxValue(arg_val, inst->operands[i].id);
                    llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                            llvm::ConstantInt::get(i32_type, i));
                    builder->CreateStore(arg_boxed, gep);
                }
            } else {
                // For zero args, pass a null pointer using i64 constant cast to pointer
                args_array = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, 0), ptr_type);
            }

            // Pass method pointer directly to runtime helper as a pointer constant
            llvm::Value* method_ptr = builder->CreateIntToPtr(
                    llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(direct_inst->method)),
                    ptr_type);

            auto helper = module.getOrInsertFunction("qore_rt_call_method_direct",
                    llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type, i32_type, ptr_type}, false));
            llvm::Value* call_result = builder->CreateCall(helper, {method_ptr, args_array,
                    llvm::ConstantInt::get(i32_type, nargs), xsink_arg});

            // Calls can modify local variables through side effects
            reloadAllLocalsFromRuntime(module, llvm_func);

            values[inst->result.id] = call_result;
            nanboxed_values.insert(inst->result.id);
            trackResultForCleanup(call_result, inst->result.id, llvm_func);
            return true;
        }

        // === InvokeMethodDirect (devirtualized method call with exception routing) ===
        case QoreIROpcode::InvokeMethodDirect: {
            const auto* invoke_inst = static_cast<const QoreIRInvokeMethodDirectInstruction*>(inst);

            // Build args array from operands
            int nargs = static_cast<int>(inst->operands.size());

            llvm::Value* args_array;
            if (nargs > 0) {
                args_array = builder->CreateAlloca(i64_type,
                        llvm::ConstantInt::get(i32_type, nargs));
                for (int i = 0; i < nargs; ++i) {
                    auto* arg_val = getVal(inst->operands[i].id, error);
                    if (!arg_val) { return false; }
                    llvm::Value* arg_boxed = boxValue(arg_val, inst->operands[i].id);
                    llvm::Value* gep = builder->CreateGEP(i64_type, args_array,
                            llvm::ConstantInt::get(i32_type, i));
                    builder->CreateStore(arg_boxed, gep);
                }
            } else {
                // For zero args, pass a null pointer using i64 constant cast to pointer
                args_array = builder->CreateIntToPtr(
                        llvm::ConstantInt::get(i64_type, 0), ptr_type);
            }

            // Pass method pointer directly to runtime helper as a pointer constant
            llvm::Value* method_ptr = builder->CreateIntToPtr(
                    llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(invoke_inst->method)),
                    ptr_type);

            auto helper = module.getOrInsertFunction("qore_rt_call_method_direct",
                    llvm::FunctionType::get(i64_type,
                        {ptr_type, ptr_type, i32_type, ptr_type}, false));
            llvm::Value* call_result = builder->CreateCall(helper, {method_ptr, args_array,
                    llvm::ConstantInt::get(i32_type, nargs), xsink_arg});

            // Calls can modify local variables through side effects
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
            llvm::Value* l = unboxInt(lhs);
            llvm::Value* r = unboxInt(rhs);
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
            // Unbox to get raw int64
            llvm::Value* idx_unboxed = unboxInt(idx);
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
            trackResultForCleanup(list_result, inst->result.id, llvm_func);
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
            trackResultForCleanup(hash_result, inst->result.id, llvm_func);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Typed integer compound assignments (native i64) ===
        case QoreIROpcode::AddAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAdd(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::SubAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateSub(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::MulAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateMul(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::AndAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAnd(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::OrAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateOr(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::XorAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateXor(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::ShlAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateShl(unboxInt(lhs), unboxInt(rhs));
            return true;
        }
        case QoreIROpcode::ShrAssignInt: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateAShr(unboxInt(lhs), unboxInt(rhs));
            return true;
        }

        // === Typed float compound assignments (native double) ===
        case QoreIROpcode::AddAssignFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFAdd(lhs, rhs);
            return true;
        }
        case QoreIROpcode::SubAssignFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFSub(lhs, rhs);
            return true;
        }
        case QoreIROpcode::MulAssignFloat: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            values[inst->result.id] = builder->CreateFMul(lhs, rhs);
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
            llvm::Value* l_int = unboxInt(lhs);
            llvm::Value* r_int = unboxInt(rhs);
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
            llvm::Value* l_float = unboxFloat(lhs);
            llvm::Value* r_float = unboxFloat(rhs);
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
            llvm::Value* l_int = unboxInt(lhs);
            llvm::Value* r_int = unboxInt(rhs);
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
        case QoreIROpcode::FoldlSumInt: {
            // Native LLVM loop for sum reduction on integer list
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_sum_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_sum_exit", llvm_func);

            // Check if list is empty
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* is_empty = builder->CreateICmpEQ(size, zero);
            builder->CreateCondBr(is_empty, exit_bb, loop_bb);

            // Loop body
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(i64_type, 2, "acc");

            // Get element at index
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Add to accumulator
            llvm::Value* new_acc = builder->CreateAdd(acc_phi, elem);

            // Increment index and check loop condition
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes
            idx_phi->addIncoming(zero, preheader);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(zero, preheader);
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(i64_type, 2, "sum_result");
            result_phi->addIncoming(zero, preheader);
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        case QoreIROpcode::FoldlSumFloat: {
            // Native LLVM loop for sum reduction on float list
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_sumf_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_sumf_exit", llvm_func);

            // Check if list is empty
            llvm::Value* zero_i = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* zero_f = llvm::ConstantFP::get(double_type, 0.0);
            llvm::Value* is_empty = builder->CreateICmpEQ(size, zero_i);
            builder->CreateCondBr(is_empty, exit_bb, loop_bb);

            // Loop body
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(double_type, 2, "acc");

            // Get element at index
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_float",
                    llvm::FunctionType::get(double_type, {i64_type, i64_type}, false));
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Add to accumulator
            llvm::Value* new_acc = builder->CreateFAdd(acc_phi, elem);

            // Increment index and check loop condition
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes
            idx_phi->addIncoming(zero_i, preheader);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(zero_f, preheader);
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(double_type, 2, "sumf_result");
            result_phi->addIncoming(zero_f, preheader);
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        case QoreIROpcode::FoldlProdInt: {
            // Native LLVM loop for product reduction on integer list
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_prod_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_prod_exit", llvm_func);

            // Check if list is empty
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* is_empty = builder->CreateICmpEQ(size, zero);
            builder->CreateCondBr(is_empty, exit_bb, loop_bb);

            // Loop body
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(i64_type, 2, "acc");

            // Get element at index
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Multiply with accumulator
            llvm::Value* new_acc = builder->CreateMul(acc_phi, elem);

            // Increment index and check loop condition
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes
            idx_phi->addIncoming(zero, preheader);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(one, preheader);  // Product identity is 1
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(i64_type, 2, "prod_result");
            result_phi->addIncoming(one, preheader);  // Empty list product is 1
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        case QoreIROpcode::FoldlProdFloat: {
            // Native LLVM loop for product reduction on float list
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_prodf_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_prodf_exit", llvm_func);

            // Check if list is empty
            llvm::Value* zero_i = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* one_f = llvm::ConstantFP::get(double_type, 1.0);
            llvm::Value* is_empty = builder->CreateICmpEQ(size, zero_i);
            builder->CreateCondBr(is_empty, exit_bb, loop_bb);

            // Loop body
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(double_type, 2, "acc");

            // Get element at index
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_float",
                    llvm::FunctionType::get(double_type, {i64_type, i64_type}, false));
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Multiply with accumulator
            llvm::Value* new_acc = builder->CreateFMul(acc_phi, elem);

            // Increment index and check loop condition
            llvm::Value* one_i = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one_i);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes
            idx_phi->addIncoming(zero_i, preheader);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(one_f, preheader);  // Product identity is 1.0
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(double_type, 2, "prodf_result");
            result_phi->addIncoming(one_f, preheader);  // Empty list product is 1.0
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        case QoreIROpcode::FoldlDiffInt: {
            // Native LLVM loop for difference reduction on integer list
            // foldl $1 - $2 means: list[0] - list[1] - list[2] - ...
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_diff_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_diff_exit", llvm_func);

            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);

            // Get first element as initial accumulator
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* first_elem = builder->CreateCall(get_helper, {list_boxed, zero});

            // Check if list has only one element (no loop needed)
            llvm::Value* has_more = builder->CreateICmpUGT(size, one);
            builder->CreateCondBr(has_more, loop_bb, exit_bb);

            // Loop body - start from index 1
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(i64_type, 2, "acc");

            // Get element at index
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Subtract from accumulator
            llvm::Value* new_acc = builder->CreateSub(acc_phi, elem);

            // Increment index and check loop condition
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes - start from index 1 with first element as accumulator
            idx_phi->addIncoming(one, preheader);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(first_elem, preheader);
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(i64_type, 2, "diff_result");
            result_phi->addIncoming(first_elem, preheader);  // Single element case
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        case QoreIROpcode::FoldlDiffFloat: {
            // Native LLVM loop for difference reduction on float list
            // foldl $1 - $2 means: list[0] - list[1] - list[2] - ...
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_difff_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_difff_exit", llvm_func);

            llvm::Value* zero_i = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);

            // Get first element as initial accumulator
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_float",
                    llvm::FunctionType::get(double_type, {i64_type, i64_type}, false));
            llvm::Value* first_elem = builder->CreateCall(get_helper, {list_boxed, zero_i});

            // Check if list has only one element (no loop needed)
            llvm::Value* has_more = builder->CreateICmpUGT(size, one);
            builder->CreateCondBr(has_more, loop_bb, exit_bb);

            // Loop body - start from index 1
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(double_type, 2, "acc");

            // Get element at index
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Subtract from accumulator
            llvm::Value* new_acc = builder->CreateFSub(acc_phi, elem);

            // Increment index and check loop condition
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes - start from index 1 with first element as accumulator
            idx_phi->addIncoming(one, preheader);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(first_elem, preheader);
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(double_type, 2, "difff_result");
            result_phi->addIncoming(first_elem, preheader);  // Single element case
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        case QoreIROpcode::FoldlMinInt: {
            // Native LLVM loop for min reduction on integer list
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(ctx, "foldl_min_init", llvm_func);
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_min_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_min_exit", llvm_func);

            // Check if list is empty (return first element as initial value)
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* is_empty = builder->CreateICmpEQ(size, zero);
            builder->CreateCondBr(is_empty, exit_bb, init_bb);

            // Init block: get first element as initial accumulator
            builder->SetInsertPoint(init_bb);
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* first_elem = builder->CreateCall(get_helper, {list_boxed, zero});
            llvm::Value* has_more = builder->CreateICmpUGT(size, one);
            builder->CreateCondBr(has_more, loop_bb, exit_bb);

            // Loop body (starts from index 1)
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(i64_type, 2, "acc");

            // Get element at index
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Compare and select minimum
            llvm::Value* cmp = builder->CreateICmpSLT(elem, acc_phi);
            llvm::Value* new_acc = builder->CreateSelect(cmp, elem, acc_phi);

            // Increment index and check loop condition
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes
            idx_phi->addIncoming(one, init_bb);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(first_elem, init_bb);
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(i64_type, 3, "min_result");
            result_phi->addIncoming(zero, preheader);  // Empty list returns 0
            result_phi->addIncoming(first_elem, init_bb);  // Single element
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        case QoreIROpcode::FoldlMinFloat: {
            // Native LLVM loop for min reduction on float list
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(ctx, "foldl_minf_init", llvm_func);
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_minf_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_minf_exit", llvm_func);

            // Check if list is empty
            llvm::Value* zero_i = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* zero_f = llvm::ConstantFP::get(double_type, 0.0);
            llvm::Value* is_empty = builder->CreateICmpEQ(size, zero_i);
            builder->CreateCondBr(is_empty, exit_bb, init_bb);

            // Init block: get first element as initial accumulator
            builder->SetInsertPoint(init_bb);
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_float",
                    llvm::FunctionType::get(double_type, {i64_type, i64_type}, false));
            llvm::Value* first_elem = builder->CreateCall(get_helper, {list_boxed, zero_i});
            llvm::Value* has_more = builder->CreateICmpUGT(size, one);
            builder->CreateCondBr(has_more, loop_bb, exit_bb);

            // Loop body (starts from index 1)
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(double_type, 2, "acc");

            // Get element at index
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Compare and select minimum
            llvm::Value* cmp = builder->CreateFCmpOLT(elem, acc_phi);
            llvm::Value* new_acc = builder->CreateSelect(cmp, elem, acc_phi);

            // Increment index and check loop condition
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes
            idx_phi->addIncoming(one, init_bb);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(first_elem, init_bb);
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(double_type, 3, "minf_result");
            result_phi->addIncoming(zero_f, preheader);  // Empty list returns 0.0
            result_phi->addIncoming(first_elem, init_bb);  // Single element
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        case QoreIROpcode::FoldlMaxInt: {
            // Native LLVM loop for max reduction on integer list
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(ctx, "foldl_max_init", llvm_func);
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_max_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_max_exit", llvm_func);

            // Check if list is empty
            llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* is_empty = builder->CreateICmpEQ(size, zero);
            builder->CreateCondBr(is_empty, exit_bb, init_bb);

            // Init block: get first element as initial accumulator
            builder->SetInsertPoint(init_bb);
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_int",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type}, false));
            llvm::Value* first_elem = builder->CreateCall(get_helper, {list_boxed, zero});
            llvm::Value* has_more = builder->CreateICmpUGT(size, one);
            builder->CreateCondBr(has_more, loop_bb, exit_bb);

            // Loop body (starts from index 1)
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(i64_type, 2, "acc");

            // Get element at index
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Compare and select maximum
            llvm::Value* cmp = builder->CreateICmpSGT(elem, acc_phi);
            llvm::Value* new_acc = builder->CreateSelect(cmp, elem, acc_phi);

            // Increment index and check loop condition
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes
            idx_phi->addIncoming(one, init_bb);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(first_elem, init_bb);
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(i64_type, 3, "max_result");
            result_phi->addIncoming(zero, preheader);  // Empty list returns 0
            result_phi->addIncoming(first_elem, init_bb);  // Single element
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        case QoreIROpcode::FoldlMaxFloat: {
            // Native LLVM loop for max reduction on float list
            auto* list = getVal(inst->operands[0].id, error);
            if (!list) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);

            // Get list size
            auto size_helper = module.getOrInsertFunction("qore_rt_list_size",
                    llvm::FunctionType::get(i64_type, {i64_type}, false));
            llvm::Value* size = builder->CreateCall(size_helper, {list_boxed});

            // Create loop blocks
            llvm::BasicBlock* preheader = builder->GetInsertBlock();
            llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(ctx, "foldl_maxf_init", llvm_func);
            llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(ctx, "foldl_maxf_loop", llvm_func);
            llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(ctx, "foldl_maxf_exit", llvm_func);

            // Check if list is empty
            llvm::Value* zero_i = llvm::ConstantInt::get(i64_type, 0);
            llvm::Value* one = llvm::ConstantInt::get(i64_type, 1);
            llvm::Value* zero_f = llvm::ConstantFP::get(double_type, 0.0);
            llvm::Value* is_empty = builder->CreateICmpEQ(size, zero_i);
            builder->CreateCondBr(is_empty, exit_bb, init_bb);

            // Init block: get first element as initial accumulator
            builder->SetInsertPoint(init_bb);
            auto get_helper = module.getOrInsertFunction("qore_rt_list_get_float",
                    llvm::FunctionType::get(double_type, {i64_type, i64_type}, false));
            llvm::Value* first_elem = builder->CreateCall(get_helper, {list_boxed, zero_i});
            llvm::Value* has_more = builder->CreateICmpUGT(size, one);
            builder->CreateCondBr(has_more, loop_bb, exit_bb);

            // Loop body (starts from index 1)
            builder->SetInsertPoint(loop_bb);
            llvm::PHINode* idx_phi = builder->CreatePHI(i64_type, 2, "idx");
            llvm::PHINode* acc_phi = builder->CreatePHI(double_type, 2, "acc");

            // Get element at index
            llvm::Value* elem = builder->CreateCall(get_helper, {list_boxed, idx_phi});

            // Compare and select maximum
            llvm::Value* cmp = builder->CreateFCmpOGT(elem, acc_phi);
            llvm::Value* new_acc = builder->CreateSelect(cmp, elem, acc_phi);

            // Increment index and check loop condition
            llvm::Value* next_idx = builder->CreateAdd(idx_phi, one);
            llvm::Value* done = builder->CreateICmpEQ(next_idx, size);
            builder->CreateCondBr(done, exit_bb, loop_bb);

            // Update PHI nodes
            idx_phi->addIncoming(one, init_bb);
            idx_phi->addIncoming(next_idx, loop_bb);
            acc_phi->addIncoming(first_elem, init_bb);
            acc_phi->addIncoming(new_acc, loop_bb);

            // Exit block - merge results
            builder->SetInsertPoint(exit_bb);
            llvm::PHINode* result_phi = builder->CreatePHI(double_type, 3, "maxf_result");
            result_phi->addIncoming(zero_f, preheader);  // Empty list returns 0.0
            result_phi->addIncoming(first_elem, init_bb);  // Single element
            result_phi->addIncoming(new_acc, loop_bb);

            values[inst->result.id] = result_phi;
            return true;
        }
        // === Optimized map operations (native runtime helpers) ===
        case QoreIROpcode::MapScaleInt: {
            auto* list = getVal(inst->operands[0].id, error);
            auto* scale = getVal(inst->operands[1].id, error);
            if (!list || !scale) { return false; }
            llvm::Value* list_boxed = boxValue(list, inst->operands[0].id);
            llvm::Value* scale_int = nanboxed_values.count(inst->operands[1].id)
                ? unboxInt(scale) : scale;
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
            llvm::Value* scale_float = nanboxed_values.count(inst->operands[1].id)
                ? unboxFloat(scale) : scale;
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
            llvm::Value* offset_int = nanboxed_values.count(inst->operands[1].id)
                ? unboxInt(offset) : offset;
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
            llvm::Value* offset_float = nanboxed_values.count(inst->operands[1].id)
                ? unboxFloat(offset) : offset;
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
            llvm::Value* scale_int = nanboxed_values.count(inst->operands[1].id)
                ? unboxInt(scale) : scale;
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
            llvm::Value* scale_float = nanboxed_values.count(inst->operands[1].id)
                ? unboxFloat(scale) : scale;
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
            llvm::Value* offset_int = nanboxed_values.count(inst->operands[1].id)
                ? unboxInt(offset) : offset;
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
            llvm::Value* offset_float = nanboxed_values.count(inst->operands[1].id)
                ? unboxFloat(offset) : offset;
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
            llvm::Value* scale_int = nanboxed_values.count(inst->operands[1].id)
                ? unboxInt(scale) : scale;
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
            llvm::Value* scale_float = nanboxed_values.count(inst->operands[1].id)
                ? unboxFloat(scale) : scale;
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
            llvm::Value* scale_int = nanboxed_values.count(inst->operands[1].id)
                ? unboxInt(scale) : scale;
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
            llvm::Value* scale_float = nanboxed_values.count(inst->operands[1].id)
                ? unboxFloat(scale) : scale;
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

            // Pass the CaseNodeRegex pointer directly to the runtime helper
            llvm::Value* regex_case_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(regex_inst->regex_case));

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

        // === Pure expression ops (no variable modification) ===
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::KeysList:
        case QoreIROpcode::KeysHash:
        case QoreIROpcode::InstanceOfBool:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ExistsBool:
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
            if (aot_mode) {
                // AOT mode: use slot index instead of raw pointer
                int32_t slot = const_cast<AOTSlotMap*>(aot_slots)->getStmtSlot(
                        reinterpret_cast<const void*>(code));
                auto helper = module.getOrInsertFunction("qore_rt_push_on_block_exit_aot",
                        llvm::FunctionType::get(void_type,
                            {ptr_type, i32_type, llvm::Type::getInt32Ty(ctx)}, false));
                builder->CreateCall(helper, {aot_ctx_arg,
                        llvm::ConstantInt::get(i32_type, slot), type_val});
            } else {
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
                iterable_boxed = boxInt(iterable_val);
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
            // Allocate stack space for the output value
            llvm::Value* out_val_ptr = builder->CreateAlloca(i64_type, nullptr, "iter_out_val");
            // Call qore_rt_iterator_next(iter_ptr, out_val_ptr, xsink) -> i64 (1=done, 0=continue)
            auto helper = module.getOrInsertFunction("qore_rt_iterator_next",
                    llvm::FunctionType::get(i64_type, {ptr_type, ptr_type, ptr_type}, false));
            llvm::Value* done_flag = builder->CreateCall(helper, {iter_ptr, out_val_ptr, xsink_arg});
            // Check for exception
            emitExceptionCheck(module, llvm_func, inst);
            // Load the output value (will be used if not done)
            llvm::Value* out_val = builder->CreateLoad(i64_type, out_val_ptr, "iter_val");
            values[inst->result.id] = out_val;
            nanboxed_values.insert(inst->result.id);
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

        // Nothing path: call qore_rt_ref to increment reference count (equivalent to refSelf)
        builder->SetInsertPoint(nothing_bb);
        auto ref_fn = module.getOrInsertFunction("qore_rt_ref",
                llvm::FunctionType::get(i64_type, {i64_type}, false));
        nothing_result = builder->CreateCall(ref_fn, {rhs});
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
    llvm::Value* int_boxed = boxInt(int_result);
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
    llvm::Value* int_boxed = boxInt(int_result);
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
    llvm::Value* int_boxed = boxInt(int_result);
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
    llvm::Value* float_boxed = boxInt(float_result);
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

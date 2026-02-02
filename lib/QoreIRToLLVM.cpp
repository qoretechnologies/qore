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

#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreHashObjectDereferenceOperatorNode.h"
#include "qore/intern/QoreSquareBracketsOperatorNode.h"
#include <qore/QoreStringNode.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

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
    auto helper = module.getOrInsertFunction("qore_rt_instantiate_local",
            llvm::FunctionType::get(void_type, {ptr_type}, false));
    for (LocalVar* var : function_locals) {
        // Skip locals already instantiated by the caller (tiered compilation)
        if (pre_instantiated_locals &&
                pre_instantiated_locals->count(reinterpret_cast<const void*>(var))) {
            continue;
        }
        llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type, reinterpret_cast<uint64_t>(var));
        llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
        builder->CreateCall(helper, {var_as_ptr});
    }
}

void QoreIRToLLVM::emitLocalUninstantiation(llvm::Module& module) {
    auto helper = module.getOrInsertFunction("qore_rt_uninstantiate_local",
            llvm::FunctionType::get(void_type, {ptr_type}, false));
    // Uninstantiate in reverse order (LIFO — matching the thread-local variable stack)
    for (auto it = function_locals.rbegin(); it != function_locals.rend(); ++it) {
        // Skip locals managed by the caller (tiered compilation)
        if (pre_instantiated_locals &&
                pre_instantiated_locals->count(reinterpret_cast<const void*>(*it))) {
            continue;
        }
        builder->CreateCall(helper, {xsink_arg});
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

    // Function signature: uint64_t fname(ExceptionSink* xsink)
    llvm::FunctionType* fn_type = llvm::FunctionType::get(i64_type, {ptr_type}, false);
    llvm::Function* llvm_func = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
            func.name, module);
    if (!llvm_func) {
        error = "failed to create LLVM function '" + func.name + "'";
        return false;
    }

    // Name the ExceptionSink* parameter
    xsink_arg = llvm_func->getArg(0);
    xsink_arg->setName("xsink");

    // Propagate pre-instantiated locals set
    pre_instantiated_locals = func.pre_instantiated_locals.empty()
        ? nullptr : &func.pre_instantiated_locals;

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

    // Collect all unique LocalVar* pointers from the function and emit
    // instantiation calls at the start of the entry block so the Qore
    // thread-local variable stack is properly set up before any code runs.
    // Pre-instantiated locals (tiered compilation) are skipped.
    collectLocals(func);
    if (!func.blocks.empty()) {
        builder->SetInsertPoint(block_map[func.blocks.front().get()]);
        emitLocalInstantiation(module);
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

    // Verify the generated LLVM IR
    std::string verify_error;
    llvm::raw_string_ostream verify_os(verify_error);
    if (llvm::verifyFunction(*llvm_func, &verify_os)) {
        error = "LLVM verification failed: " + verify_error;
        return false;
    }

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
            values[inst->result.id] = emitAnyArithFastPath(
                llvm::Instruction::Add, llvm::Instruction::FAdd,
                "qore_rt_add_any", lhs_boxed, rhs_boxed, llvm_func, module);
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::SubAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            values[inst->result.id] = emitAnyArithFastPath(
                llvm::Instruction::Sub, llvm::Instruction::FSub,
                "qore_rt_sub_any", lhs_boxed, rhs_boxed, llvm_func, module);
            nanboxed_values.insert(inst->result.id);
            return true;
        }
        case QoreIROpcode::MulAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            llvm::Value* lhs_boxed = boxValue(lhs, inst->operands[0].id);
            llvm::Value* rhs_boxed = boxValue(rhs, inst->operands[1].id);
            values[inst->result.id] = emitAnyArithFastPath(
                llvm::Instruction::Mul, llvm::Instruction::FMul,
                "qore_rt_mul_any", lhs_boxed, rhs_boxed, llvm_func, module);
            nanboxed_values.insert(inst->result.id);
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
            values[inst->result.id] = builder->CreateCall(helper, {lhs_boxed, rhs_boxed, xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
                    auto load_fn = module.getOrInsertFunction("qore_rt_load_local",
                        llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
                    llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                    llvm::Value* var_as_ptr = alloca_builder.CreateIntToPtr(var_ptr, ptr_type);
                    llvm::Value* init_val = alloca_builder.CreateCall(load_fn,
                        {var_as_ptr, xsink_arg});
                    alloca_builder.CreateStore(init_val, alloca);
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
                auto assign_helper = module.getOrInsertFunction("qore_rt_assign_local",
                        llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
                llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                        reinterpret_cast<uint64_t>(linst->local));
                llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
                builder->CreateCall(assign_helper, {var_as_ptr, boxed, xsink_arg});
            }
            return true;
        }

        // === Phi nodes ===
        case QoreIROpcode::Phi: {
            const auto* phi = static_cast<const QoreIRPhiInstruction*>(inst);
            // Phi type: use i64 as the common type for NaN-boxed values.
            // We'll need to box incoming values that aren't i64.
            llvm::PHINode* phi_node = builder->CreatePHI(i64_type, phi->incoming.size());
            values[inst->result.id] = phi_node;
            // Note: incoming values may not be lowered yet (forward edges).
            // We'll do a fixup pass after all blocks are lowered.
            // For now, store the phi instruction and resolve later.
            // LLVM PHI nodes allow adding incoming values after creation.
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
            // Uninstantiate locals before returning (pre-instantiated locals are skipped internally)
            emitLocalUninstantiation(module);
            if (ret->has_value) {
                auto* val = getVal(ret->value.id, error);
                if (!val) { return false; }
                // Box the return value to NaN-boxed i64
                llvm::Value* boxed;
                if (nanboxed_values.count(ret->value.id)) {
                    boxed = val;  // Already NaN-boxed
                } else if (val->getType() == i64_type) {
                    boxed = boxInt(val);
                } else if (val->getType() == double_type) {
                    boxed = boxFloat(val);
                } else if (val->getType() == i1_type) {
                    boxed = boxBool(val);
                } else {
                    error = "unsupported return value type for LLVM lowering";
                    return false;
                }
                builder->CreateRet(boxed);
            } else {
                builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            }
            return true;
        }
        case QoreIROpcode::ReturnNothing: {
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
            values[inst->result.id] = builder->CreateCall(helper, {str_const});
            nanboxed_values.insert(inst->result.id);
            return true;
        }

        // === Invoke (expression call with exception edges) ===
        case QoreIROpcode::Invoke: {
            const auto* inv = static_cast<const QoreIRInvokeInstruction*>(inst);
            // The invoke instruction evaluates an AST expression node.
            // We pass the expression's NaN-boxed bits to the runtime helper.
            // The expr QoreValue holds a reference to the AST node.
            QoreValue expr_val = inv->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);

            auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            llvm::Value* result = builder->CreateCall(helper, {expr_const, xsink_arg});
            values[inst->result.id] = result;
            nanboxed_values.insert(inst->result.id);

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
            values[inst->result.id] = builder->CreateCall(helper, {xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
            // No exception target: uninstantiate locals and return NOTHING
            emitLocalUninstantiation(module);
            builder->CreateRet(llvm::ConstantInt::get(i64_type, VAL_NOTHING));
            return true;
        }

        // === Rethrow ===
        case QoreIROpcode::Rethrow: {
            // Rethrow: exception is already in the ExceptionSink.
            // Just return NOTHING; the caller will see the exception.
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
            llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);

            auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {expr_const, xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
            values[inst->result.id] = builder->CreateCall(helper, {us_val, rel_val});
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
            values[inst->result.id] = emitAnyCmpFastPath(llvm::CmpInst::ICMP_EQ,
                llvm::CmpInst::FCMP_OEQ, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
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
            values[inst->result.id] = emitAnyCmpFastPath(llvm::CmpInst::ICMP_NE,
                llvm::CmpInst::FCMP_ONE, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
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
            values[inst->result.id] = emitAnyCmpFastPath(llvm::CmpInst::ICMP_SLT,
                llvm::CmpInst::FCMP_OLT, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
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
            values[inst->result.id] = emitAnyCmpFastPath(llvm::CmpInst::ICMP_SLE,
                llvm::CmpInst::FCMP_OLE, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
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
            values[inst->result.id] = emitAnyCmpFastPath(llvm::CmpInst::ICMP_SGT,
                llvm::CmpInst::FCMP_OGT, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
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
            values[inst->result.id] = emitAnyCmpFastPath(llvm::CmpInst::ICMP_SGE,
                llvm::CmpInst::FCMP_OGE, static_cast<int>(inst->opcode),
                lhs_boxed, rhs_boxed, llvm_func, module);
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
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, lhs_boxed, rhs_boxed, xsink_arg});
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
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, lhs_boxed, rhs_boxed, xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, val_boxed, xsink_arg});
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }

        // === Variable access operations ===
        case QoreIROpcode::LoadGlobal:
        case QoreIROpcode::LoadThreadLocal: {
            const auto* vinst = static_cast<const QoreIRVarInstruction*>(inst);
            const char* helper_name = (inst->opcode == QoreIROpcode::LoadGlobal)
                    ? "qore_rt_load_global" : "qore_rt_load_thread_local";
            llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(vinst->var));
            llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
            auto helper = module.getOrInsertFunction(helper_name,
                    llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {var_as_ptr, xsink_arg});
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::StoreGlobal:
        case QoreIROpcode::StoreThreadLocal: {
            const auto* vinst = static_cast<const QoreIRVarInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            const char* helper_name = (inst->opcode == QoreIROpcode::StoreGlobal)
                    ? "qore_rt_store_global" : "qore_rt_store_thread_local";
            llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(vinst->var));
            llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            auto helper = module.getOrInsertFunction(helper_name,
                    llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
            builder->CreateCall(helper, {var_as_ptr, val_boxed, xsink_arg});
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::LoadClosure: {
            // LoadClosure uses QoreIRLocalInstruction with LocalVar* — access via
            // thread-local variable stack using qore_rt_load_local (same as LoadLocal
            // but without the alloca caching since closure vars live on the stack)
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(linst->local));
            llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
            auto helper = module.getOrInsertFunction("qore_rt_load_local",
                    llvm::FunctionType::get(i64_type, {ptr_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {var_as_ptr, xsink_arg});
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::StoreClosure: {
            const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            llvm::Value* var_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(linst->local));
            llvm::Value* var_as_ptr = builder->CreateIntToPtr(var_ptr, ptr_type);
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_assign_local",
                    llvm::FunctionType::get(void_type, {ptr_type, i64_type, ptr_type}, false));
            builder->CreateCall(helper, {var_as_ptr, val_boxed, xsink_arg});
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
            llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
            auto helper = module.getOrInsertFunction("qore_rt_lvalue_load",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {lv_const, xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
            llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            auto helper = module.getOrInsertFunction("qore_rt_lvalue_store",
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {lv_const, val_boxed, xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
            llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            auto helper = module.getOrInsertFunction("qore_rt_lvalue_unary",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, lv_const, xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
            llvm::Value* lv_const = llvm::ConstantInt::get(i64_type, lv_bits);
            llvm::Value* val_boxed = boxValue(val, inst->operands[0].id);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            auto helper = module.getOrInsertFunction("qore_rt_lvalue_binary",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), i64_type, i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, lv_const, val_boxed, xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
            values[inst->result.id] = builder->CreateCall(helper, {arr, count_val, xsink_arg});
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
            values[inst->result.id] = builder->CreateCall(helper, {arr, count_val, xsink_arg});
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
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, lhs_boxed, rhs_boxed, xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexNMatchBool:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList:
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
        case QoreIROpcode::ElementsInt:
        // Phase 5b: Try specialized hash key access and list index access before generic path
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalAny: {
            // Try specialized hash key access
            if (tryEmitHashKeyAccess(inst, module, llvm_func)) {
                nanboxed_values.insert(inst->result.id);
                emitExceptionCheck(module, llvm_func, inst);
                return true;
            }
            // Try specialized list index access
            if (tryEmitListIndexAccess(inst, module, llvm_func)) {
                nanboxed_values.insert(inst->result.id);
                emitExceptionCheck(module, llvm_func, inst);
                return true;
            }
            // Fall through to generic path
            const auto* expr_inst = static_cast<const QoreIRExprInstruction*>(inst);
            QoreValue expr_val = expr_inst->expr;
            uint64_t expr_bits;
            std::memcpy(&expr_bits, &expr_val, sizeof(expr_bits));
            llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
            auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {expr_const, xsink_arg});
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalObject:
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
            llvm::Value* expr_const = llvm::ConstantInt::get(i64_type, expr_bits);
            auto helper = module.getOrInsertFunction("qore_rt_invoke_expr",
                    llvm::FunctionType::get(i64_type, {i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {expr_const, xsink_arg});
            nanboxed_values.insert(inst->result.id);
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
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::OnBlockExit: {
            const auto* sinst = static_cast<const QoreIROnBlockExitInstruction*>(inst);
            llvm::Value* opcode_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),
                    static_cast<int>(inst->opcode));
            llvm::Value* stmt_ptr = llvm::ConstantInt::get(i64_type,
                    reinterpret_cast<uint64_t>(sinst->stmt));
            llvm::Value* stmt_as_ptr = builder->CreateIntToPtr(stmt_ptr, ptr_type);
            auto helper = module.getOrInsertFunction("qore_rt_exec_statement",
                    llvm::FunctionType::get(i64_type,
                        {llvm::Type::getInt32Ty(ctx), ptr_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
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
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
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
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
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
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
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
            values[inst->result.id] = builder->CreateCall(helper,
                    {opcode_val, stmt_as_ptr, xsink_arg});
            nanboxed_values.insert(inst->result.id);
            emitExceptionCheck(module, llvm_func, inst);
            return true;
        }
        case QoreIROpcode::ThreadExit: {
            auto helper = module.getOrInsertFunction("qore_rt_thread_exit",
                    llvm::FunctionType::get(void_type, {ptr_type}, false));
            builder->CreateCall(helper, {xsink_arg});
            // Thread exit raises an exception; uninstantiate locals and return
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

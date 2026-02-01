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
        case QoreIROpcode::AddAny:
        case QoreIROpcode::SubAny:
        case QoreIROpcode::MulAny:
        case QoreIROpcode::DivAny:
        case QoreIROpcode::ModAny: {
            auto* lhs = getVal(inst->operands[0].id, error);
            auto* rhs = getVal(inst->operands[1].id, error);
            if (!lhs || !rhs) { return false; }
            const char* helper_name = nullptr;
            switch (inst->opcode) {
                case QoreIROpcode::AddAny: helper_name = "qore_rt_add_any"; break;
                case QoreIROpcode::SubAny: helper_name = "qore_rt_sub_any"; break;
                case QoreIROpcode::MulAny: helper_name = "qore_rt_mul_any"; break;
                case QoreIROpcode::DivAny: helper_name = "qore_rt_div_any"; break;
                case QoreIROpcode::ModAny: helper_name = "qore_rt_mod_any"; break;
                default: break;
            }
            auto helper = module.getOrInsertFunction(helper_name,
                    llvm::FunctionType::get(i64_type, {i64_type, i64_type, ptr_type}, false));
            values[inst->result.id] = builder->CreateCall(helper, {lhs, rhs, xsink_arg});
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
            // If the value is i1, just negate it; if i64, compare with 0
            if (val->getType() == i1_type) {
                values[inst->result.id] = builder->CreateNot(val);
            } else {
                llvm::Value* cmp = builder->CreateICmpEQ(val, llvm::ConstantInt::get(i64_type, 0));
                values[inst->result.id] = cmp;
            }
            return true;
        }
        case QoreIROpcode::ToBool: {
            auto* val = getVal(inst->operands[0].id, error);
            if (!val) { return false; }
            if (val->getType() == i1_type) {
                values[inst->result.id] = val;
            } else if (val->getType() == i64_type) {
                values[inst->result.id] = builder->CreateICmpNE(val, llvm::ConstantInt::get(i64_type, 0));
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
                cond = cond_val;
            } else if (cond_val->getType() == i64_type) {
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
            if (inst->operands.size() >= 2) {
                // throw with err and desc operands
                auto* err_val = getVal(inst->operands[0].id, error);
                auto* desc_val = getVal(inst->operands[1].id, error);
                if (!err_val || !desc_val) { return false; }
                auto helper = module.getOrInsertFunction("qore_rt_throw",
                        llvm::FunctionType::get(void_type, {ptr_type, ptr_type, ptr_type}, false));
                builder->CreateCall(helper, {xsink_arg, err_val, desc_val});
            }
            // Branch to exception target if present
            if (throw_inst->exception_target) {
                auto it = block_map.find(throw_inst->exception_target);
                if (it != block_map.end()) {
                    builder->CreateBr(it->second);
                    return true;
                }
            }
            // No exception target; return NOTHING
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

        default:
            error = "unsupported IR opcode for LLVM lowering: " + std::to_string(static_cast<int>(inst->opcode));
            return false;
    }
}

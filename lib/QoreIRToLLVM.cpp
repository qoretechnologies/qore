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

#ifdef QORE_JIT_ENABLED
#include "qore/intern/QoreIR.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include <unordered_map>

bool QoreIRToLLVM::lowerFunction(const QoreIRFunction& func, llvm::Module& module, std::string& error) {
    auto* i64_type = llvm::Type::getInt64Ty(ctx);
    llvm::FunctionType* fn_type = llvm::FunctionType::get(i64_type, false);
    llvm::Function* llvm_func = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, func.name, module);
    if (!llvm_func) {
        error = "failed to create LLVM function";
        return false;
    }

    std::unordered_map<const QoreIRBasicBlock*, llvm::BasicBlock*> block_map;
    block_map.reserve(func.blocks.size());
    for (const auto& block : func.blocks) {
        block_map[block.get()] = llvm::BasicBlock::Create(ctx, block->name, llvm_func);
    }

    std::unordered_map<uint32_t, llvm::Value*> values;
    llvm::IRBuilder<> builder(ctx);

    for (const auto& block : func.blocks) {
        llvm::BasicBlock* llvm_block = block_map[block.get()];
        if (!llvm_block) {
            error = "missing LLVM basic block mapping";
            return false;
        }
        builder.SetInsertPoint(llvm_block);

        for (const auto& inst_ptr : block->instructions) {
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            switch (inst->opcode) {
                case QoreIROpcode::ConstInt: {
                    const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
                    llvm::Value* val = llvm::ConstantInt::get(i64_type, cinst->constant.int_value, true);
                    values[inst->result.id] = val;
                    break;
                }
                case QoreIROpcode::ConstBool: {
                    const auto* cinst = static_cast<const QoreIRConstInstruction*>(inst);
                    llvm::Value* val = llvm::ConstantInt::get(i64_type, cinst->constant.bool_value ? 1 : 0);
                    values[inst->result.id] = val;
                    break;
                }
                case QoreIROpcode::AddInt:
                case QoreIROpcode::SubInt:
                case QoreIROpcode::MulInt:
                case QoreIROpcode::DivInt:
                case QoreIROpcode::ModInt: {
                    if (inst->operands.size() != 2) {
                        error = "binary int op missing operands";
                        return false;
                    }
                    auto lhs_it = values.find(inst->operands[0].id);
                    auto rhs_it = values.find(inst->operands[1].id);
                    if (lhs_it == values.end() || rhs_it == values.end()) {
                        error = "binary int op operand missing";
                        return false;
                    }
                    llvm::Value* lhs = lhs_it->second;
                    llvm::Value* rhs = rhs_it->second;
                    llvm::Value* res = nullptr;
                    switch (inst->opcode) {
                        case QoreIROpcode::AddInt:
                            res = builder.CreateAdd(lhs, rhs);
                            break;
                        case QoreIROpcode::SubInt:
                            res = builder.CreateSub(lhs, rhs);
                            break;
                        case QoreIROpcode::MulInt:
                            res = builder.CreateMul(lhs, rhs);
                            break;
                        case QoreIROpcode::DivInt:
                            res = builder.CreateSDiv(lhs, rhs);
                            break;
                        case QoreIROpcode::ModInt:
                            res = builder.CreateSRem(lhs, rhs);
                            break;
                        default:
                            break;
                    }
                    if (!res) {
                        error = "binary int op lowering failed";
                        return false;
                    }
                    values[inst->result.id] = res;
                    break;
                }
                case QoreIROpcode::Br: {
                    const auto* br = static_cast<const QoreIRBranchInstruction*>(inst);
                    auto it = block_map.find(br->target);
                    if (it == block_map.end()) {
                        error = "branch target missing";
                        return false;
                    }
                    builder.CreateBr(it->second);
                    break;
                }
                case QoreIROpcode::BrIf: {
                    const auto* br = static_cast<const QoreIRBranchIfInstruction*>(inst);
                    auto cond_it = values.find(br->condition.id);
                    if (cond_it == values.end()) {
                        error = "branch condition missing";
                        return false;
                    }
                    auto t_it = block_map.find(br->true_target);
                    auto f_it = block_map.find(br->false_target);
                    if (t_it == block_map.end() || f_it == block_map.end()) {
                        error = "branch target missing";
                        return false;
                    }
                    llvm::Value* cond_val = cond_it->second;
                    llvm::Value* zero = llvm::ConstantInt::get(i64_type, 0);
                    llvm::Value* cond = builder.CreateICmpNE(cond_val, zero);
                    builder.CreateCondBr(cond, t_it->second, f_it->second);
                    break;
                }
                case QoreIROpcode::Return: {
                    const auto* ret = static_cast<const QoreIRReturnInstruction*>(inst);
                    if (ret->has_value) {
                        auto it = values.find(ret->value.id);
                        if (it == values.end()) {
                            error = "return value missing";
                            return false;
                        }
                        builder.CreateRet(it->second);
                    } else {
                        builder.CreateRet(llvm::ConstantInt::get(i64_type, 0));
                    }
                    break;
                }
                case QoreIROpcode::ReturnNothing: {
                    builder.CreateRet(llvm::ConstantInt::get(i64_type, 0));
                    break;
                }
                default:
                    error = "unsupported opcode for LLVM lowering";
                    return false;
            }
        }
        if (!llvm_block->getTerminator()) {
            error = "missing terminator in lowered block";
            return false;
        }
    }

    return true;
}

bool QoreIRToLLVM::lowerInvoke(const QoreIRFunction& func, std::string& error) {
    (void)func;
    error = "IR invoke/landingpad lowering not implemented";
    return false;
}
#endif

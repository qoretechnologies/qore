/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRVerifier.cpp

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

#include <qore/intern/QoreIRVerifier.h>

#include <unordered_set>

#include <qore/intern/QoreIR.h>

static bool isTerminator(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Invoke:
        case QoreIROpcode::Br:
        case QoreIROpcode::BrIf:
        case QoreIROpcode::Return:
        case QoreIROpcode::ReturnNothing:
        case QoreIROpcode::Throw:
        case QoreIROpcode::Rethrow:
        case QoreIROpcode::InvokeSimError:
        case QoreIROpcode::ThreadExit:
            return true;
        default:
            return false;
    }
}

static bool requiresResult(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::ConstInt:
        case QoreIROpcode::ConstFloat:
        case QoreIROpcode::ConstBool:
        case QoreIROpcode::ConstNothing:
        case QoreIROpcode::ConstNull:
        case QoreIROpcode::ConstString:
        case QoreIROpcode::ConstDate:
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::AddAny:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::SubAny:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::MulAny:
        case QoreIROpcode::DivInt:
        case QoreIROpcode::DivFloat:
        case QoreIROpcode::DivAny:
        case QoreIROpcode::ModInt:
        case QoreIROpcode::ModAny:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::AndAny:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::OrAny:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::XorAny:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShlAny:
        case QoreIROpcode::ShrInt:
        case QoreIROpcode::ShrAny:
        case QoreIROpcode::ShlAssignInt:
        case QoreIROpcode::ShlAssignAny:
        case QoreIROpcode::ShrAssignInt:
        case QoreIROpcode::ShrAssignAny:
        case QoreIROpcode::AddAssignInt:
        case QoreIROpcode::AddAssignAny:
        case QoreIROpcode::SubAssignInt:
        case QoreIROpcode::SubAssignAny:
        case QoreIROpcode::MulAssignInt:
        case QoreIROpcode::MulAssignAny:
        case QoreIROpcode::DivAssignInt:
        case QoreIROpcode::DivAssignAny:
        case QoreIROpcode::ModAssignInt:
        case QoreIROpcode::ModAssignAny:
        case QoreIROpcode::AndAssignInt:
        case QoreIROpcode::AndAssignAny:
        case QoreIROpcode::OrAssignInt:
        case QoreIROpcode::OrAssignAny:
        case QoreIROpcode::XorAssignInt:
        case QoreIROpcode::XorAssignAny:
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeAny:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LtAny:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::LeAny:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GtAny:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::GeAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::CmpAny:
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::Phi:
        case QoreIROpcode::UnaryPlusAny:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        case QoreIROpcode::UnaryMinusAny:
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
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::HashMapAny:
        case QoreIROpcode::HashMapSelectAny:
        case QoreIROpcode::RangeAny:
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat:
        case QoreIROpcode::RangeSliceAny:
        case QoreIROpcode::RangeSliceInt:
        case QoreIROpcode::RangeSliceFloat:
        case QoreIROpcode::MakeList:
        case QoreIROpcode::CastAny:
        case QoreIROpcode::ExtractAny:
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::LoadLocal:
        case QoreIROpcode::LoadArg:
        case QoreIROpcode::LoadClosure:
        case QoreIROpcode::LoadGlobal:
        case QoreIROpcode::LoadThreadLocal:
        case QoreIROpcode::LoadLValue:
        case QoreIROpcode::PreIncLValue:
        case QoreIROpcode::PreDecLValue:
        case QoreIROpcode::PostIncLValue:
        case QoreIROpcode::PostDecLValue:
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
        case QoreIROpcode::ShiftLValue:
        case QoreIROpcode::UnshiftLValue:
        case QoreIROpcode::SpliceLValue:
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::Invoke:
        case QoreIROpcode::CatchException:
            return true;
        default:
            return false;
    }
}

static int expectedOperands(QoreIROpcode op) {
    switch (op) {
        case QoreIROpcode::Foreach:
        case QoreIROpcode::OnBlockExit:
        case QoreIROpcode::ThreadExit:
        case QoreIROpcode::Debug:
            return 0;
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::AddAny:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::SubAny:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::MulAny:
        case QoreIROpcode::DivInt:
        case QoreIROpcode::DivFloat:
        case QoreIROpcode::DivAny:
        case QoreIROpcode::ModInt:
        case QoreIROpcode::ModAny:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::AndAny:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::OrAny:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::XorAny:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShlAny:
        case QoreIROpcode::ShrInt:
        case QoreIROpcode::ShrAny:
        case QoreIROpcode::ShlAssignInt:
        case QoreIROpcode::ShlAssignAny:
        case QoreIROpcode::ShrAssignInt:
        case QoreIROpcode::ShrAssignAny:
        case QoreIROpcode::AddAssignInt:
        case QoreIROpcode::AddAssignAny:
        case QoreIROpcode::SubAssignInt:
        case QoreIROpcode::SubAssignAny:
        case QoreIROpcode::MulAssignInt:
        case QoreIROpcode::MulAssignAny:
        case QoreIROpcode::DivAssignInt:
        case QoreIROpcode::DivAssignAny:
        case QoreIROpcode::ModAssignInt:
        case QoreIROpcode::ModAssignAny:
        case QoreIROpcode::AndAssignInt:
        case QoreIROpcode::AndAssignAny:
        case QoreIROpcode::OrAssignInt:
        case QoreIROpcode::OrAssignAny:
        case QoreIROpcode::XorAssignInt:
        case QoreIROpcode::XorAssignAny:
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqAny:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeAny:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LtAny:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::LeAny:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GtAny:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::GeAny:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::CmpAny:
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
        case QoreIROpcode::RangeAny:
        case QoreIROpcode::RangeInt:
        case QoreIROpcode::RangeFloat:
            return 2;
        case QoreIROpcode::MapSelectAny:
        case QoreIROpcode::HashMapAny:
            return 3;
        case QoreIROpcode::HashMapSelectAny:
            return 4;
        case QoreIROpcode::CastAny:
            return 1;
        case QoreIROpcode::ExtractAny:
            return 4;
        case QoreIROpcode::RemoveAny:
        case QoreIROpcode::KeysAny:
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool:
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexSubstAny:
        case QoreIROpcode::ExistsAny:
        case QoreIROpcode::ElementsAny:
        case QoreIROpcode::DotEvalAny:
            return 1;
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::IsNullOrNothing:
        case QoreIROpcode::UnaryPlusAny:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
        case QoreIROpcode::UnaryMinusAny:
        case QoreIROpcode::Incref:
        case QoreIROpcode::Decref:
        case QoreIROpcode::DecrefNoThrow:
        case QoreIROpcode::GuardInt:
        case QoreIROpcode::GuardFloat:
        case QoreIROpcode::GuardType:
        case QoreIROpcode::GuardNotNothing:
            return 1;
        case QoreIROpcode::LoadArg:
        case QoreIROpcode::LoadClosure:
            return -1;
        case QoreIROpcode::StoreLocal:
        case QoreIROpcode::StoreClosure:
        case QoreIROpcode::StoreGlobal:
        case QoreIROpcode::StoreThreadLocal:
        case QoreIROpcode::StoreLValue:
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
        case QoreIROpcode::UnshiftLValue:
            return 1;
        case QoreIROpcode::Throw:
            return 1;
        case QoreIROpcode::InvokeSimError:
            return 0;
        case QoreIROpcode::RangeSliceAny:
        case QoreIROpcode::RangeSliceInt:
        case QoreIROpcode::RangeSliceFloat:
        case QoreIROpcode::SpliceLValue:
            return 3;
        case QoreIROpcode::ShiftLValue:
            return 0;
        case QoreIROpcode::MakeList:
            return -1;
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::Invoke:
            return -1;
        case QoreIROpcode::Phi:
            return -1;
        default:
            return 0;
    }
}

bool QoreIRVerifier::verify(const QoreIRFunction& func, std::string& error) {
    if (func.blocks.empty()) {
        error = "function has no basic blocks";
        return false;
    }
    std::unordered_set<const QoreIRBasicBlock*> block_set;
    for (const auto& block : func.blocks) {
        if (!block_set.insert(block.get()).second) {
            error = "duplicate basic block pointer";
            return false;
        }
        if (block->instructions.empty()) {
            error = "basic block '" + block->name + "' has no instructions";
            return false;
        }
        const QoreIRInstruction* last = block->instructions.back().get();
        if (!isTerminator(last->opcode)) {
            error = "basic block '" + block->name + "' missing terminator";
            return false;
        }
    }
    std::unordered_set<uint32_t> value_ids;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (requiresResult(inst->opcode)) {
                if (!inst->result.isValid()) {
                    error = "instruction missing result value";
                    return false;
                }
                if (!value_ids.insert(inst->result.id).second) {
                    error = "duplicate result value id";
                    return false;
                }
            } else if (inst->result.isValid()) {
                error = "unexpected result value";
                return false;
            }
        }
    }
    for (const auto& block : func.blocks) {
        for (const auto& inst : block->instructions) {
            if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->normal_target
                    || block_set.find(invoke_inst->normal_target) == block_set.end()
                    || !invoke_inst->exception_target
                    || block_set.find(invoke_inst->exception_target) == block_set.end()) {
                    error = "invoke missing valid targets";
                    return false;
                }
            }
            int expected = expectedOperands(inst->opcode);
            if (expected >= 0 && expected != static_cast<int>(inst->operands.size())) {
                error = "unexpected operand count";
                return false;
            }
            if ((inst->opcode == QoreIROpcode::LoadArg || inst->opcode == QoreIROpcode::LoadClosure)
                    && inst->operands.size() > 1) {
                error = "load.arg/load.closure only support zero or one operand";
                return false;
            }
            for (const auto& op : inst->operands) {
                if (!op.isValid()) {
                    error = "invalid operand value id";
                    return false;
                }
                if (value_ids.find(op.id) == value_ids.end()) {
                    error = "operand references undefined value";
                    return false;
                }
            }
            if (inst->opcode == QoreIROpcode::Br) {
                auto* br = dynamic_cast<const QoreIRBranchInstruction*>(inst.get());
                if (!br || !br->target) {
                    error = "branch missing target";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::BrIf) {
                auto* br = dynamic_cast<const QoreIRBranchIfInstruction*>(inst.get());
                if (!br || !br->condition.isValid()) {
                    error = "branch-if missing condition";
                    return false;
                }
                if (!br->true_target || !br->false_target) {
                    error = "branch-if missing target";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Return
                    || inst->opcode == QoreIROpcode::ReturnNothing) {
                auto* ret = dynamic_cast<const QoreIRReturnInstruction*>(inst.get());
                if (!ret) {
                    error = "return instruction malformed";
                    return false;
                }
                if (inst->opcode == QoreIROpcode::Return && !ret->has_value) {
                    error = "return missing value";
                    return false;
                }
                if (inst->opcode == QoreIROpcode::ReturnNothing && ret->has_value) {
                    error = "return.nothing has value";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Phi) {
                auto* phi = dynamic_cast<const QoreIRPhiInstruction*>(inst.get());
                if (!phi) {
                    error = "phi instruction malformed";
                    return false;
                }
                if (phi->incoming.empty()) {
                    error = "phi missing incoming values";
                    return false;
                }
                if (phi->incoming.size() != inst->operands.size()) {
                    error = "phi operands do not match incoming values";
                    return false;
                }
                for (const auto& incoming : phi->incoming) {
                    if (!incoming.block || block_set.find(incoming.block) == block_set.end()) {
                        error = "phi references unknown block";
                        return false;
                    }
                }
            } else if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::LoadClosure
                    || inst->opcode == QoreIROpcode::StoreClosure) {
                auto* local_inst = dynamic_cast<const QoreIRLocalInstruction*>(inst.get());
                if (!local_inst || !local_inst->local) {
                    error = "local instruction missing local";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::LoadGlobal
                    || inst->opcode == QoreIROpcode::StoreGlobal
                    || inst->opcode == QoreIROpcode::LoadThreadLocal
                    || inst->opcode == QoreIROpcode::StoreThreadLocal) {
                auto* var_inst = dynamic_cast<const QoreIRVarInstruction*>(inst.get());
                if (!var_inst || !var_inst->var) {
                    error = "variable instruction missing var";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::LoadLValue
                    || inst->opcode == QoreIROpcode::StoreLValue
                    || inst->opcode == QoreIROpcode::PreIncLValue
                    || inst->opcode == QoreIROpcode::PreDecLValue
                    || inst->opcode == QoreIROpcode::PostIncLValue
                    || inst->opcode == QoreIROpcode::PostDecLValue
                    || inst->opcode == QoreIROpcode::AddAssignLValue
                    || inst->opcode == QoreIROpcode::SubAssignLValue
                    || inst->opcode == QoreIROpcode::MulAssignLValue
                    || inst->opcode == QoreIROpcode::DivAssignLValue
                    || inst->opcode == QoreIROpcode::ModAssignLValue
                    || inst->opcode == QoreIROpcode::AndAssignLValue
                    || inst->opcode == QoreIROpcode::OrAssignLValue
                    || inst->opcode == QoreIROpcode::XorAssignLValue
                    || inst->opcode == QoreIROpcode::ShlAssignLValue
                    || inst->opcode == QoreIROpcode::ShrAssignLValue) {
                auto* lv_inst = dynamic_cast<const QoreIRLValueInstruction*>(inst.get());
                if (!lv_inst || !lv_inst->lvalue.hasNode()) {
                    error = "lvalue instruction missing lvalue";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Call
                    || inst->opcode == QoreIROpcode::CallIndirect
                    || inst->opcode == QoreIROpcode::CallMethod
                    || inst->opcode == QoreIROpcode::CallStatic
                    || inst->opcode == QoreIROpcode::CastAny
                    || inst->opcode == QoreIROpcode::ExtractAny
                    || inst->opcode == QoreIROpcode::RemoveAny
                    || inst->opcode == QoreIROpcode::KeysAny
                    || inst->opcode == QoreIROpcode::RegexMatchAny
                    || inst->opcode == QoreIROpcode::RegexMatchBool
                    || inst->opcode == QoreIROpcode::RegexExtractAny
                    || inst->opcode == QoreIROpcode::RegexSubstAny
                    || inst->opcode == QoreIROpcode::ExistsAny
                    || inst->opcode == QoreIROpcode::ElementsAny
                    || inst->opcode == QoreIROpcode::DotEvalAny) {
                auto* expr_inst = dynamic_cast<const QoreIRExprInstruction*>(inst.get());
                if (!expr_inst || !expr_inst->expr.hasNode()) {
                    error = "expr instruction missing expr";
                    return false;
                }
            } else if (inst->opcode == QoreIROpcode::Invoke) {
                auto* invoke_inst = dynamic_cast<const QoreIRInvokeInstruction*>(inst.get());
                if (!invoke_inst || !invoke_inst->expr.hasNode()) {
                    error = "invoke instruction missing expr";
                    return false;
                }
            }
        }
    }
    return true;
}

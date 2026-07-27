/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreJIT.cpp

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

#include <qore/Qore.h>
#include "qore/intern/QoreJIT.h"
#include "qore/intern/QoreIRAnalysis.h"
#include "qore/intern/qore_thread_intern.h"

#include <cstdio>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/ExecutionEngine/Orc/Debugging/DebuggerSupport.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include "qore/intern/QoreIRToLLVM.h"
#include "qore/intern/JITRuntime.h"

#include <cstring>

#include "qore/intern/QoreIR.h"
#include "qore/intern/Function.h"
#include "qore/intern/QoreIRInterpreter.h"

// Thread-local flag to detect when JIT cleared the stack location for exception safety
thread_local bool jit_cleared_stack_location = false;

DLLLOCAL void set_jit_cleared_stack_flag(bool cleared) {
    jit_cleared_stack_location = cleared;
}

DLLLOCAL bool is_jit_cleared_stack() {
    return jit_cleared_stack_location;
}

static void qore_ir_add_successor(std::vector<const QoreIRBasicBlock*>& successors,
        const QoreIRBasicBlock* target) {
    if (target) {
        successors.push_back(target);
    }
}

static std::vector<const QoreIRBasicBlock*> qore_ir_get_normal_successors(
        const QoreIRFunction& ir_func, size_t block_index, bool* cancelled = nullptr) {
    std::vector<const QoreIRBasicBlock*> successors;
    const QoreIRBasicBlock* block = ir_func.blocks[block_index].get();
    const QoreIRBasicBlock* next_block = block_index + 1 < ir_func.blocks.size()
        ? ir_func.blocks[block_index + 1].get() : nullptr;
    if (!block || block->instructions.empty()) {
        qore_ir_add_successor(successors, next_block);
        return successors;
    }

    const QoreIRInstruction* inst = block->instructions.back().get();
    switch (inst->opcode) {
        case QoreIROpcode::Br: {
            const auto* br = static_cast<const QoreIRBranchInstruction*>(inst);
            qore_ir_add_successor(successors, br->target);
            break;
        }
        case QoreIROpcode::BrIf: {
            const auto* br = static_cast<const QoreIRBranchIfInstruction*>(inst);
            qore_ir_add_successor(successors, br->true_target);
            qore_ir_add_successor(successors, br->false_target);
            break;
        }
        case QoreIROpcode::Invoke: {
            const auto* invoke = static_cast<const QoreIRInvokeInstruction*>(inst);
            qore_ir_add_successor(successors, invoke->normal_target);
            break;
        }
        case QoreIROpcode::InvokeMethodDirect: {
            const auto* invoke = static_cast<const QoreIRInvokeMethodDirectInstruction*>(inst);
            qore_ir_add_successor(successors, invoke->normal_target);
            break;
        }
        case QoreIROpcode::InvokeDotEvalMethodDirect: {
            const auto* invoke = static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction*>(inst);
            qore_ir_add_successor(successors, invoke->normal_target);
            break;
        }
        case QoreIROpcode::SwitchInt: {
            const auto* sw = static_cast<const QoreIRSwitchIntInstruction*>(inst);
            qore_ir_add_successor(successors, sw->default_target);
            size_t case_count = 0;
            for (const auto& c : sw->cases) {
                if (++case_count % 100 == 0
                        && qore_check_cancel(nullptr, "IR native local switch successor analysis")) {
                    if (cancelled) {
                        *cancelled = true;
                    }
                    return {};
                }
                qore_ir_add_successor(successors, c.target);
            }
            break;
        }
        case QoreIROpcode::SwitchString: {
            const auto* sw = static_cast<const QoreIRSwitchStringInstruction*>(inst);
            qore_ir_add_successor(successors, sw->default_target);
            size_t case_count = 0;
            for (const auto& c : sw->cases) {
                if (++case_count % 100 == 0
                        && qore_check_cancel(nullptr, "IR native local switch successor analysis")) {
                    if (cancelled) {
                        *cancelled = true;
                    }
                    return {};
                }
                qore_ir_add_successor(successors, c.target);
            }
            break;
        }
        case QoreIROpcode::IteratorNext:
        case QoreIROpcode::TypedForeachNextInt:
        case QoreIROpcode::TypedForeachNextFloat:
        case QoreIROpcode::TypedForeachNextBool:
        case QoreIROpcode::TypedForeachNextString: {
            const auto* iter = static_cast<const QoreIRIteratorNextInstruction*>(inst);
            qore_ir_add_successor(successors, iter->continue_target);
            qore_ir_add_successor(successors, iter->done_target);
            break;
        }
        case QoreIROpcode::BranchIfLtLocalInt: {
            const auto* br = static_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst);
            qore_ir_add_successor(successors, br->true_target);
            qore_ir_add_successor(successors, br->false_target);
            break;
        }
        case QoreIROpcode::Return:
        case QoreIROpcode::ReturnNothing:
        case QoreIROpcode::Throw:
        case QoreIROpcode::Rethrow:
            break;
        default:
            qore_ir_add_successor(successors, next_block);
            break;
    }
    return successors;
}

static bool qore_ir_opcode_uses_expr_lvalue_mutation(QoreIROpcode op) {
    switch (op) {
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
            return true;
        default:
            return false;
    }
}

static void qore_ir_insert_local_key(std::unordered_set<const void*>& keys, const LocalVar* local) {
    if (local) {
        keys.insert(reinterpret_cast<const void*>(local));
    }
}

static const void* qore_ir_get_lvalue_path_root_key(const QoreIRLValuePathInstruction& inst,
        const std::unordered_map<uint32_t, const void*>& slot_to_key) {
    if (inst.path.empty()) {
        return nullptr;
    }
    const LVPathStep& root = inst.path.front();
    if (root.kind != LVPathStepKind::LocalVar) {
        return nullptr;
    }
    if (root.ref_ptr) {
        return root.ref_ptr;
    }
    auto it = slot_to_key.find(root.slot_id);
    return it == slot_to_key.end() ? nullptr : it->second;
}

static const void* qore_ir_get_lvalue_instruction_key(const QoreIRLValueInstruction& inst,
        const std::unordered_map<uint32_t, const void*>& slot_to_key) {
    if (!inst.hasLocalTarget()) {
        return nullptr;
    }
    auto it = slot_to_key.find(inst.lvalue_slot_id);
    return it == slot_to_key.end() ? nullptr : it->second;
}

static const void* qore_ir_get_local_instruction_key(const QoreIRLocalInstruction& inst,
        const std::unordered_map<uint32_t, const void*>& slot_to_key) {
    if (inst.local) {
        return reinterpret_cast<const void*>(inst.local);
    }
    auto it = slot_to_key.find(inst.slot_id);
    return it == slot_to_key.end() ? nullptr : it->second;
}

static std::unordered_set<const void*> qore_ir_collect_local_keys(const QoreIRFunction& ir_func,
        bool* cancelled = nullptr) {
    std::unordered_set<const void*> keys;
    size_t param_count = 0;
    for (const auto& entry : ir_func.param_local_vars) {
        if (++param_count % 100 == 0
                && qore_check_cancel(nullptr, "IR native local parameter key collection")) {
            if (cancelled) {
                *cancelled = true;
            }
            return keys;
        }
        qore_ir_insert_local_key(keys, entry.second);
    }

    std::unordered_map<uint32_t, const void*> slot_to_key;
    size_t slot_count = 0;
    for (const auto& entry : ir_func.local_var_slots) {
        if (++slot_count % 100 == 0
                && qore_check_cancel(nullptr, "IR native local slot key collection")) {
            if (cancelled) {
                *cancelled = true;
            }
            return keys;
        }
        const void* key = reinterpret_cast<const void*>(entry.first);
        slot_to_key.emplace(entry.second, key);
        keys.insert(key);
    }

    size_t block_count = 0;
    for (const auto& block : ir_func.blocks) {
        if (++block_count % 100 == 0
                && qore_check_cancel(nullptr, "IR native local key collection")) {
            if (cancelled) {
                *cancelled = true;
            }
            return keys;
        }
        size_t inst_count = 0;
        for (const auto& inst_ptr : block->instructions) {
            if (++inst_count % 100 == 0
                    && qore_check_cancel(nullptr, "IR native local instruction key collection")) {
                if (cancelled) {
                    *cancelled = true;
                }
                return keys;
            }
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal
                    || inst->opcode == QoreIROpcode::UninstantiateLocal
                    || inst->opcode == QoreIROpcode::InstantiateLocal) {
                const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
                if (const void* key = qore_ir_get_local_instruction_key(*linst, slot_to_key)) {
                    keys.insert(key);
                }
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                const auto* fused = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
                qore_ir_insert_local_key(keys, fused->target);
                qore_ir_insert_local_key(keys, fused->source);
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                const auto* fused = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst);
                qore_ir_insert_local_key(keys, fused->local);
            } else if (inst->opcode == QoreIROpcode::BranchIfLtLocalInt) {
                const auto* fused = static_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst);
                qore_ir_insert_local_key(keys, fused->lhs);
                qore_ir_insert_local_key(keys, fused->rhs);
            } else if (inst->opcode == QoreIROpcode::LValuePathAssign
                    || inst->opcode == QoreIROpcode::LValuePathCompound
                    || inst->opcode == QoreIROpcode::LValuePathUnary
                    || inst->opcode == QoreIROpcode::LValuePathBinaryMut
                    || inst->opcode == QoreIROpcode::LValuePathTernary) {
                const auto* lvalue = static_cast<const QoreIRLValuePathInstruction*>(inst);
                if (const void* key = qore_ir_get_lvalue_path_root_key(*lvalue, slot_to_key)) {
                    keys.insert(key);
                }
            } else if (inst->opcode == QoreIROpcode::StoreLValue
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
                    || inst->opcode == QoreIROpcode::ShrAssignLValue
                    || inst->opcode == QoreIROpcode::ShiftLValue
                    || inst->opcode == QoreIROpcode::UnshiftLValue
                    || inst->opcode == QoreIROpcode::SpliceLValue) {
                const auto* lvalue = static_cast<const QoreIRLValueInstruction*>(inst);
                if (const void* key = qore_ir_get_lvalue_instruction_key(*lvalue, slot_to_key)) {
                    keys.insert(key);
                }
            }
        }
    }
    return keys;
}

static std::unordered_set<const void*> qore_ir_intersect_assigned(
        const std::vector<std::unordered_set<const void*>>& out_sets,
        const std::vector<size_t>& preds, bool* cancelled = nullptr) {
    if (preds.empty()) {
        return {};
    }
    std::unordered_set<const void*> rv = out_sets[preds.front()];
    for (size_t i = 1; i < preds.size(); ++i) {
        if (i && !(i % 100)
                && qore_check_cancel(nullptr, "IR native local predecessor intersection")) {
            if (cancelled) {
                *cancelled = true;
            }
            return rv;
        }
        const auto& pred_out = out_sets[preds[i]];
        size_t local_count = 0;
        for (auto it = rv.begin(); it != rv.end();) {
            if (++local_count % 100 == 0
                    && qore_check_cancel(nullptr, "IR native local assigned-set intersection")) {
                if (cancelled) {
                    *cancelled = true;
                }
                return rv;
            }
            if (!pred_out.count(*it)) {
                it = rv.erase(it);
            } else {
                ++it;
            }
        }
    }
    return rv;
}

std::unordered_set<const void*> qore_ir_get_native_unsafe_locals(
        const QoreIRFunction& ir_func, const std::unordered_set<const void*>& initially_assigned) {
    bool cancelled = false;
    std::unordered_set<const void*> all_keys = qore_ir_collect_local_keys(ir_func, &cancelled);
    if (cancelled) {
        return all_keys;
    }
    if (ir_func.blocks.empty()) {
        return all_keys;
    }

    std::unordered_map<const QoreIRBasicBlock*, size_t> block_index;
    for (size_t i = 0; i < ir_func.blocks.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "IR native local block index setup")) {
            return all_keys;
        }
        block_index.emplace(ir_func.blocks[i].get(), i);
    }

    std::vector<std::vector<size_t>> preds(ir_func.blocks.size());
    for (size_t i = 0; i < ir_func.blocks.size(); ++i) {
        if (i && !(i % 100) && qore_check_cancel(nullptr, "IR native local successor analysis")) {
            return all_keys;
        }
        bool successor_cancelled = false;
        for (const QoreIRBasicBlock* succ : qore_ir_get_normal_successors(
                    ir_func, i, &successor_cancelled)) {
            auto it = block_index.find(succ);
            if (it != block_index.end()) {
                preds[it->second].push_back(i);
            }
        }
        if (successor_cancelled) {
            return all_keys;
        }
    }

    std::unordered_map<uint32_t, const void*> slot_to_key;
    size_t slot_count = 0;
    for (const auto& entry : ir_func.local_var_slots) {
        if (++slot_count % 100 == 0
                && qore_check_cancel(nullptr, "IR native local transfer slot setup")) {
            return all_keys;
        }
        slot_to_key.emplace(entry.second, reinterpret_cast<const void*>(entry.first));
    }

    std::unordered_set<const void*> assigned_universe = all_keys;
    assigned_universe.insert(initially_assigned.begin(), initially_assigned.end());
    std::vector<std::unordered_set<const void*>> in_sets(ir_func.blocks.size());
    std::vector<std::unordered_set<const void*>> out_sets(ir_func.blocks.size());
    for (size_t i = 0; i < out_sets.size(); ++i) {
        if (i && !(i % 10)
                && qore_check_cancel(nullptr, "IR native local lattice initialization")) {
            return all_keys;
        }
        out_sets[i] = assigned_universe;
    }

    auto transfer_block = [&](size_t block_id,
            std::unordered_set<const void*>& assigned,
            std::unordered_set<const void*>* unsafe) -> bool {
        size_t inst_count = 0;
        for (const auto& inst_ptr : ir_func.blocks[block_id]->instructions) {
            const QoreIRInstruction* inst = inst_ptr.get();
            if (!inst) {
                continue;
            }
            if (++inst_count % 100 == 0
                    && qore_check_cancel(nullptr, "IR native local assigned-state transfer")) {
                return false;
            }

            if (inst->opcode == QoreIROpcode::LoadLocal) {
                if (unsafe) {
                    const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
                    const void* key = qore_ir_get_local_instruction_key(*linst, slot_to_key);
                    if (key && !assigned.count(key)) {
                        unsafe->insert(key);
                    }
                }
            } else if (inst->opcode == QoreIROpcode::StoreLocal) {
                const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
                if (const void* key = qore_ir_get_local_instruction_key(*linst, slot_to_key)) {
                    assigned.insert(key);
                }
            } else if (inst->opcode == QoreIROpcode::UninstantiateLocal) {
                const auto* linst = static_cast<const QoreIRLocalInstruction*>(inst);
                if (const void* key = qore_ir_get_local_instruction_key(*linst, slot_to_key)) {
                    assigned.erase(key);
                }
            } else if (inst->opcode == QoreIROpcode::AddAssignLocalInt) {
                const auto* fused = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
                const void* source_key = reinterpret_cast<const void*>(fused->source);
                if (unsafe && source_key && !assigned.count(source_key)) {
                    unsafe->insert(source_key);
                }
                if (fused->target) {
                    assigned.insert(reinterpret_cast<const void*>(fused->target));
                }
            } else if (inst->opcode == QoreIROpcode::IncrementLocalInt) {
                const auto* fused = static_cast<const QoreIRIncrementLocalIntInstruction*>(inst);
                if (fused->local) {
                    assigned.insert(reinterpret_cast<const void*>(fused->local));
                }
            } else if (inst->opcode == QoreIROpcode::BranchIfLtLocalInt) {
                if (unsafe) {
                    const auto* fused = static_cast<const QoreIRBranchIfLtLocalIntInstruction*>(inst);
                    const void* lhs_key = reinterpret_cast<const void*>(fused->lhs);
                    const void* rhs_key = reinterpret_cast<const void*>(fused->rhs);
                    if (lhs_key && !assigned.count(lhs_key)) {
                        unsafe->insert(lhs_key);
                    }
                    if (rhs_key && !assigned.count(rhs_key)) {
                        unsafe->insert(rhs_key);
                    }
                }
            } else if (qore_ir_opcode_uses_expr_lvalue_mutation(inst->opcode)) {
                if (unsafe) {
                    unsafe->insert(all_keys.begin(), all_keys.end());
                }
                assigned.clear();
            } else if (inst->opcode == QoreIROpcode::LValuePathAssign
                    || inst->opcode == QoreIROpcode::LValuePathCompound
                    || inst->opcode == QoreIROpcode::LValuePathUnary
                    || inst->opcode == QoreIROpcode::LValuePathBinaryMut
                    || inst->opcode == QoreIROpcode::LValuePathTernary) {
                const auto* lvalue = static_cast<const QoreIRLValuePathInstruction*>(inst);
                if (const void* key = qore_ir_get_lvalue_path_root_key(*lvalue, slot_to_key)) {
                    if (unsafe) {
                        unsafe->insert(key);
                    }
                    assigned.erase(key);
                }
            } else if (inst->opcode == QoreIROpcode::StoreLValue
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
                    || inst->opcode == QoreIROpcode::ShrAssignLValue
                    || inst->opcode == QoreIROpcode::ShiftLValue
                    || inst->opcode == QoreIROpcode::UnshiftLValue
                    || inst->opcode == QoreIROpcode::SpliceLValue) {
                const auto* lvalue = static_cast<const QoreIRLValueInstruction*>(inst);
                if (const void* key = qore_ir_get_lvalue_instruction_key(*lvalue, slot_to_key)) {
                    if (unsafe) {
                        unsafe->insert(key);
                    }
                    assigned.erase(key);
                }
            }
        }
        return true;
    };

    // Definite assignment is a forward must-analysis. Start non-entry blocks at
    // the top element and converge before inspecting reads; recording unsafe
    // reads during convergence would permanently reject locals on loop
    // backedges based on incomplete predecessor facts.
    bool changed = true;
    size_t iterations = 0;
    while (changed) {
        changed = false;
        if (++iterations % 10 == 0 && qore_check_cancel(nullptr, "IR native local assigned-state analysis")) {
            return all_keys;
        }
        for (size_t i = 0; i < ir_func.blocks.size(); ++i) {
            if (i && !(i % 100)
                    && qore_check_cancel(nullptr, "IR native local assigned-state block transfer")) {
                return all_keys;
            }
            bool intersect_cancelled = false;
            std::unordered_set<const void*> assigned = i == 0
                ? initially_assigned : qore_ir_intersect_assigned(out_sets,
                    preds[i], &intersect_cancelled);
            if (intersect_cancelled) {
                return all_keys;
            }
            if (in_sets[i] != assigned) {
                in_sets[i] = assigned;
            }
            if (!transfer_block(i, assigned, nullptr)) {
                return all_keys;
            }

            if (out_sets[i] != assigned) {
                out_sets[i] = std::move(assigned);
                changed = true;
            }
        }
    }

    std::unordered_set<const void*> unsafe;
    for (size_t i = 0; i < ir_func.blocks.size(); ++i) {
        if (i && !(i % 100)
                && qore_check_cancel(nullptr, "IR native local unsafe-use classification")) {
            return all_keys;
        }
        std::unordered_set<const void*> assigned = in_sets[i];
        if (!transfer_block(i, assigned, &unsafe)) {
            return all_keys;
        }
    }
    return unsafe;
}

std::vector<BatchCalleeParamKind> qore_ir_get_fast_entry_param_kinds(
        const QoreIRFunction& ir_func, const UserSignature* sig) {
    unsigned num_params = sig ? sig->numParams() : 0;
    std::vector<BatchCalleeParamKind> kinds(num_params, BatchCalleeParamKind::Boxed);
    if (!sig || ir_func.ir_only_locals.empty()) {
        return kinds;
    }

    std::unordered_set<const void*> initially_assigned;
    for (unsigned i = 0; i < num_params; ++i) {
        if (i && !(i % 100)
                && qore_check_cancel(nullptr, "IR fast-entry parameter assigned-state setup")) {
            return kinds;
        }
        qore_ir_insert_local_key(initially_assigned, sig->lv[i]);
    }
    std::unordered_set<const void*> native_unsafe_locals
        = qore_ir_get_native_unsafe_locals(ir_func, initially_assigned);

    for (unsigned i = 0; i < num_params; ++i) {
        if (i && !(i % 100)
                && qore_check_cancel(nullptr, "IR fast-entry parameter kind analysis")) {
            return kinds;
        }
        const LocalVar* lv = sig->lv[i];
        if (!lv) {
            continue;
        }
        const void* key = reinterpret_cast<const void*>(lv);
        if (!qore_ir_fast_entry_param_is_private(ir_func, lv)
                || native_unsafe_locals.count(key)) {
            continue;
        }

        const QoreTypeInfo* ti = ir_func.specializeType(lv->getTypeInfo());
        if (QoreTypeInfo::isType(ti, NT_INT) && !QoreTypeInfo::getReturnEnum(ti)) {
            kinds[i] = BatchCalleeParamKind::NativeInt;
        } else if (QoreTypeInfo::isType(ti, NT_FLOAT)) {
            kinds[i] = BatchCalleeParamKind::NativeFloat;
        } else if (QoreTypeInfo::isType(ti, NT_BOOLEAN)) {
            kinds[i] = BatchCalleeParamKind::NativeBool;
        }
    }
    return kinds;
}

bool qore_ir_fast_entry_param_is_private(
        const QoreIRFunction& ir_func, const LocalVar* local) {
    if (!local || local->closureUse()
            || QoreTypeInfo::isReference(local->getTypeInfo())) {
        return false;
    }
    const void* key = reinterpret_cast<const void*>(local);
    if (ir_func.ir_only_locals.count(key)) {
        return true;
    }
    if (std::getenv("QORE_DISABLE_UNUSED_FAST_ENTRY_PARAMS")) {
        return false;
    }

    // A safe local referenced by lowered IR would be in ir_only_locals.
    // Therefore absence from both local inventories proves that the parameter
    // is unused. Object locals are excluded because their lifetime still
    // requires the runtime stack even when their value is not loaded.
    return !ir_func.has_opaque_ast_local_access
        && !ir_func.isAstVisibleLocal(key)
        && !local->isTopLevel()
        && !QoreTypeInfo::getUniqueReturnClass(local->getTypeInfo());
}

std::vector<BatchCalleeParamKind> qore_ir_get_signature_param_kinds(
        const UserSignature* sig) {
    unsigned num_params = sig ? sig->numParams() : 0;
    std::vector<BatchCalleeParamKind> kinds(num_params, BatchCalleeParamKind::Boxed);
    for (unsigned i = 0; i < num_params; ++i) {
        if (i && !(i % 100)
                && qore_check_cancel(nullptr,
                    "IR typed signature parameter ABI analysis")) {
            return std::vector<BatchCalleeParamKind>(
                num_params, BatchCalleeParamKind::Boxed);
        }
        const LocalVar* lv = sig->lv[i];
        const QoreTypeInfo* ti = lv ? lv->getTypeInfo() : nullptr;
        if (QoreTypeInfo::isType(ti, NT_INT) && !QoreTypeInfo::getReturnEnum(ti)) {
            kinds[i] = BatchCalleeParamKind::NativeInt;
        } else if (QoreTypeInfo::isType(ti, NT_FLOAT)) {
            kinds[i] = BatchCalleeParamKind::NativeFloat;
        } else if (QoreTypeInfo::isType(ti, NT_BOOLEAN)) {
            kinds[i] = BatchCalleeParamKind::NativeBool;
        }
    }
    return kinds;
}

BatchCalleeParamKind qore_ir_get_scalar_local_kind(const LocalVar* local) {
    const QoreTypeInfo* type = local ? local->getTypeInfo() : nullptr;
    if (!type || QoreTypeInfo::isReference(type)
            || QoreTypeInfo::parseAcceptsReturns(type, NT_NOTHING)) {
        return BatchCalleeParamKind::Boxed;
    }
    if (QoreTypeInfo::isType(type, NT_INT) && !QoreTypeInfo::getReturnEnum(type)) {
        return BatchCalleeParamKind::NativeInt;
    }
    if (QoreTypeInfo::isType(type, NT_FLOAT)) {
        return BatchCalleeParamKind::NativeFloat;
    }
    if (QoreTypeInfo::isType(type, NT_BOOLEAN)) {
        return BatchCalleeParamKind::NativeBool;
    }
    return BatchCalleeParamKind::Boxed;
}

std::vector<uint8_t> qore_ir_get_fast_entry_param_rejects_nothing(
        const UserSignature* sig, const QoreIRFunction* ir_func) {
    unsigned num_params = sig ? sig->numParams() : 0;
    std::vector<uint8_t> rejects(num_params, 0);
    if (!sig) {
        return rejects;
    }

    for (unsigned i = 0; i < num_params; ++i) {
        if (i && !(i % 100)
                && qore_check_cancel(nullptr, "IR fast-entry parameter NOTHING metadata analysis")) {
            return rejects;
        }
        const LocalVar* lv = sig->lv[i];
        if (!lv) {
            continue;
        }
        const QoreTypeInfo* ti = ir_func
            ? ir_func->specializeType(lv->getTypeInfo()) : lv->getTypeInfo();
        rejects[i] = QoreTypeInfo::hasType(ti)
            && !QoreTypeInfo::parseAcceptsReturns(ti, NT_NOTHING);
    }
    return rejects;
}

BatchCalleeReturnKind qore_ir_get_fast_entry_return_kind(
        const AbstractQoreFunctionVariant* variant, bool never_returns_nothing,
        const QoreIRFunction* ir_func) {
    if (!variant || !never_returns_nothing || std::getenv("QORE_DISABLE_NATIVE_FAST_RETURNS")) {
        return BatchCalleeReturnKind::Boxed;
    }
    const AbstractFunctionSignature* sig
        = const_cast<AbstractQoreFunctionVariant*>(variant)->getSignature();
    if (!sig) {
        return BatchCalleeReturnKind::Boxed;
    }
    const QoreTypeInfo* ti = ir_func
        ? ir_func->specializeType(sig->getReturnTypeInfo())
        : sig->getReturnTypeInfo();
    if (QoreTypeInfo::isType(ti, NT_INT) && !QoreTypeInfo::getReturnEnum(ti)) {
        return BatchCalleeReturnKind::NativeInt;
    }
    if (QoreTypeInfo::isType(ti, NT_FLOAT)) {
        return BatchCalleeReturnKind::NativeFloat;
    }
    if (QoreTypeInfo::isType(ti, NT_BOOLEAN)) {
        return BatchCalleeReturnKind::NativeBool;
    }
    return BatchCalleeReturnKind::Boxed;
}

static llvm::Type* qore_jit_fast_entry_param_type(BatchCalleeParamKind kind,
        llvm::Type* i64_ty, llvm::Type* double_ty) {
    if (kind == BatchCalleeParamKind::NativeFloat) {
        return double_ty;
    }
    if (kind == BatchCalleeParamKind::NativeBool) {
        return llvm::Type::getInt1Ty(i64_ty->getContext());
    }
    return i64_ty;
}

// Tiered compilation threshold defaults (overridable via QORE_IR_THRESHOLD / QORE_JIT_THRESHOLD env vars)
uint64_t QoreJIT::ir_threshold = []() -> uint64_t {
    const char* env = getenv("QORE_IR_THRESHOLD");
    return env ? strtoull(env, nullptr, 10) : 100;
}();
uint64_t QoreJIT::jit_threshold = []() -> uint64_t {
    const char* env = getenv("QORE_JIT_THRESHOLD");
    return env ? strtoull(env, nullptr, 10) : 1000;
}();

// JIT optimization level: default O2, overridable via QORE_JIT_OPT_LEVEL env var
int QoreJIT::jit_opt_level = -1;  // -1 = not yet initialized

int QoreJIT::getJITOptLevel() {
    if (jit_opt_level < 0) {
        const char* env = getenv("QORE_JIT_OPT_LEVEL");
        if (env) {
            int level = atoi(env);
            jit_opt_level = (level >= 0 && level <= 3) ? level : 2;
        } else {
            jit_opt_level = 2;
        }
    }
    return jit_opt_level;
}

//! Run LLVM optimization passes on a module before JIT compilation
static void optimizeModule(llvm::Module& module, int opt_level) {
    if (opt_level <= 0) {
        return;
    }

    llvm::OptimizationLevel llvm_opt;
    switch (opt_level) {
        case 1: llvm_opt = llvm::OptimizationLevel::O1; break;
        case 3: llvm_opt = llvm::OptimizationLevel::O3; break;
        default: llvm_opt = llvm::OptimizationLevel::O2; break;
    }

    // Create a target machine for the native target (needed by PassBuilder for target-specific opts)
    std::string triple_str = llvm::sys::getDefaultTargetTriple();
    llvm::Triple triple(triple_str);
    std::string target_error;
    auto* target = llvm::TargetRegistry::lookupTarget(triple_str, target_error);
    if (!target) {
        return;  // Fall back to unoptimized if target lookup fails
    }
#if LLVM_VERSION_MAJOR >= 21
    // LLVM 21+: createTargetMachine expects Triple object
    auto* tm = target->createTargetMachine(triple, "generic", "",
        llvm::TargetOptions{}, llvm::Reloc::PIC_);
#else
    // LLVM 18-20: createTargetMachine expects StringRef
    auto* tm = target->createTargetMachine(triple_str, "generic", "",
        llvm::TargetOptions{}, llvm::Reloc::PIC_);
#endif
    if (!tm) {
        return;
    }

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;
    llvm::PassBuilder PB(tm);
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
    auto MPM = PB.buildPerModuleDefaultPipeline(llvm_opt);
    MPM.run(module, MAM);
    size_t pruned_calls = QoreIRToLLVM::pruneNoopDecrefs(module);
    if (pruned_calls && getenv("QORE_IR_OPT_STATS")) {
        fprintf(stderr, "IR-OPT-LLVM: pruned-noop-decrefs=%zu\n",
            pruned_calls);
    }

    delete tm;
}

QoreJIT& QoreJIT::instance() {
    static QoreJIT jit;
    return jit;
}

bool QoreJIT::isEnabled() const {
    return true;
}

bool QoreJIT::canJit(int64 parse_options, std::string& reason) const {
    if ((parse_options & PO_MODERN) != PO_MODERN) {
        reason = "requires %modern (PO_MODERN)";
        return false;
    }
    return true;
}

bool QoreJIT::initialize(std::string& error) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

#if LLVM_VERSION_MAJOR == 20 && defined(__aarch64__)
    // Workaround: LLVM 20's greedy/basic register allocators crash on certain
    // valid IR patterns on aarch64 (complex control flow with many phi nodes).
    // https://github.com/llvm/llvm-project/issues/181566
    // Use CodeGenOptLevel::None to select the fast register allocator instead.
    // IR-level optimizations are still applied separately via optimizeModule().
    auto jtmb = llvm::orc::JITTargetMachineBuilder::detectHost();
    if (!jtmb) {
        error = llvm::toString(jtmb.takeError());
        return false;
    }
    jtmb->setCodeGenOptLevel(llvm::CodeGenOptLevel::None);
    auto jit_or_err = llvm::orc::LLJITBuilder()
        .setJITTargetMachineBuilder(std::move(*jtmb))
        .create();
#else
    auto jit_or_err = llvm::orc::LLJITBuilder().create();
#endif
    if (!jit_or_err) {
        error = llvm::toString(jit_or_err.takeError());
        return false;
    }
    jit = std::move(*jit_or_err);

    // Phase 5c: Enable GDB/LLDB debugger support for JIT-compiled code
    // This registers JIT-compiled functions with the debugger via the GDB JIT interface,
    // allowing debuggers to see symbols and source locations in JIT-compiled code.
    // Requires GDB 7.0+ or LLDB with jit-loader.gdb plugin enabled.
    // Thread-safe: compile_mutex serializes all JIT compilations.
    if (auto err = llvm::orc::enableDebuggerSupport(*jit)) {
        // Non-fatal: JIT works fine without debugger support
        printd(1, "QoreJIT::init() WARNING: failed to enable debugger support: %s\n",
            llvm::toString(std::move(err)).c_str());
    }

    if (!registerRuntimeSymbols(error)) {
        jit.reset();
        return false;
    }

    return true;
}

bool QoreJIT::registerRuntimeSymbols(std::string& error) {
    if (symbols_registered) {
        return true;
    }

    auto& es = jit->getExecutionSession();
    auto& dl = jit->getDataLayout();
    auto& jd = jit->getMainJITDylib();

    // Build symbol map for all C ABI runtime helpers
    llvm::orc::SymbolMap symbols;
    std::string symbol_error;

    auto addSymbol = [&](const char* name, void* addr) {
        if (!symbol_error.empty()) {
            return;
        }
        if (!name || !*name) {
            symbol_error = "JIT runtime helper registry contains an unnamed symbol";
            return;
        }
        if (!addr) {
            symbol_error = "JIT runtime helper '" + std::string(name) + "' has a null address";
            return;
        }
        auto flags = llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable;
        auto [it, inserted] = symbols.insert({es.intern(name), {llvm::orc::ExecutorAddr::fromPtr(addr), flags}});
        if (!inserted) {
            symbol_error = "duplicate JIT runtime helper symbol '" + std::string(name) + "'";
        }
    };

    std::string registry_error;
    if (!qore_jit_validate_runtime_symbols(registry_error)) {
        error = "failed to validate JIT runtime helper registry: " + registry_error;
        return false;
    }

    size_t symbol_count = 0;
    const QoreJITRuntimeSymbolInfo* runtime_symbols = qore_jit_get_runtime_symbols(symbol_count);
    for (size_t i = 0; i < symbol_count; ++i) {
        addSymbol(runtime_symbols[i].name, runtime_symbols[i].address);
    }

    if (!symbol_error.empty()) {
        error = "failed to validate JIT runtime helper registry: " + symbol_error;
        return false;
    }

    auto err = jd.define(llvm::orc::absoluteSymbols(std::move(symbols)));
    if (err) {
        error = "failed to register JIT runtime symbols: " + llvm::toString(std::move(err));
        return false;
    }

    symbols_registered = true;
    return true;
}

bool QoreJIT::compileFunction(const QoreIRFunction& func, std::string& error,
        void* deopt_counter) {
    // Thread-safe initialization using std::call_once
    std::call_once(init_flag, [this]() {
        init_success = initialize(init_error);
    });
    if (!init_success) {
        error = init_error;
        return false;
    }

    // Check if already compiled (fast path without compile lock)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(func.name) != compiled_functions.end()) {
            return true;
        }
    }

    // Serialize the entire compilation pipeline: LLVM's code generation (MCStreamer,
    // DWARF emission, debugger support) is not thread-safe for concurrent compilations.
    std::lock_guard<std::mutex> compile_lock(compile_mutex);

    return compileFunctionInternal(func, error, deopt_counter);
}

bool QoreJIT::compileFunctionLocked(const QoreIRFunction& func, std::string& error,
        void* deopt_counter) {
    // Thread-safe initialization using std::call_once
    std::call_once(init_flag, [this]() {
        init_success = initialize(init_error);
    });
    if (!init_success) {
        error = init_error;
        return false;
    }

    // Check if already compiled (fast path)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(func.name) != compiled_functions.end()) {
            return true;
        }
    }

    return compileFunctionInternal(func, error, deopt_counter);
}

bool QoreJIT::compileFunctionInternal(const QoreIRFunction& func, std::string& error,
        void* deopt_counter) {
    // Copy func.name before any LLVM operations.
    // On Linux, LLVM 21's addIRModule()/lookup() can corrupt adjacent heap memory
    // (specifically the std::string::_M_string_length field) when compiling closures
    // or functions with complex IR patterns.  The local copy is made before any LLVM
    // heap activity and remains valid even if the original is corrupted.
    const std::string func_name = func.name;

    // Re-check cache under compile lock (another thread may have compiled this function)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(func_name) != compiled_functions.end()) {
            return true;
        }
    }

    // Create a new LLVM context and module for this compilation
    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto module = std::make_unique<llvm::Module>("qore_jit_" + func_name, *ctx);
    module->setDataLayout(jit->getDataLayout());

    // Lower IR to LLVM IR
    // NOTE: the lowering object must be destroyed before we transfer the module/context
    // to the JIT (addIRModule + lookup below), because QoreIRToLLVM holds an IRBuilder
    // with DebugLoc metadata references into the LLVMContext.  After addIRModule() +
    // lookup() trigger materialization, the JIT may free the context; destroying the
    // IRBuilder afterwards would crash in MetadataTracking::untrack().
    {
        QoreIRToLLVM lowering(*ctx);
        if (deopt_counter) {
            lowering.setDeoptCounter(deopt_counter);
        }
        if (!lowering.lowerFunction(func, *module, error)) {
            return false;
        }
    }

    // Run LLVM optimization passes
    optimizeModule(*module, getJITOptLevel());

    // Dump LLVM IR if requested (after optimization)
    if (getenv("QORE_DUMP_LLVM_IR")) {
        llvm::raw_fd_ostream llvm_dump(2, false);
        llvm_dump << "=== LLVM IR for " << func_name << " ===\n";
        module->print(llvm_dump, nullptr);
        llvm_dump << "=== END LLVM IR ===\n";
    }

    // Add the module to the JIT
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx));
    auto err = jit->addIRModule(std::move(tsm));
    if (err) {
        error = "failed to add module to JIT: " + llvm::toString(std::move(err));
        return false;
    }

    // Look up the compiled function (triggers materialization/code generation inline)
    auto sym = jit->lookup(func_name);
    if (!sym) {
        error = "failed to look up compiled function '" + func_name + "': " + llvm::toString(sym.takeError());
        return false;
    }

    auto fn_ptr = sym->toPtr<uint64_t(ExceptionSink*)>();

    // Cache the compiled function pointer
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        compiled_functions[func_name] = fn_ptr;
    }

    return true;
}

bool QoreJIT::compileFunctionBatch(const QoreIRFunction& root_func, std::string& error,
        void* root_deopt_counter,
        const std::vector<BatchCallee>& callees) {
    // Thread-safe initialization using std::call_once
    std::call_once(init_flag, [this]() {
        init_success = initialize(init_error);
    });
    if (!init_success) {
        error = init_error;
        return false;
    }

    // Check if already compiled (fast path without compile lock)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(root_func.name) != compiled_functions.end()) {
            return true;
        }
    }

    // Serialize the entire compilation pipeline
    std::lock_guard<std::mutex> compile_lock(compile_mutex);

    return compileFunctionBatchInternal(root_func, error, root_deopt_counter, callees);
}

bool QoreJIT::compileFunctionBatchLocked(const QoreIRFunction& root_func, std::string& error,
        void* root_deopt_counter,
        const std::vector<BatchCallee>& callees) {
    // Thread-safe initialization using std::call_once
    std::call_once(init_flag, [this]() {
        init_success = initialize(init_error);
    });
    if (!init_success) {
        error = init_error;
        return false;
    }

    // Check if already compiled (fast path)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(root_func.name) != compiled_functions.end()) {
            return true;
        }
    }

    return compileFunctionBatchInternal(root_func, error, root_deopt_counter, callees);
}

bool QoreJIT::compileFunctionBatchInternal(const QoreIRFunction& root_func, std::string& error,
        void* root_deopt_counter,
        const std::vector<BatchCallee>& callees,
        QoreIRFunction* rewrite_root) {
    // Copy root_func.name before any LLVM operations (same heap-corruption workaround
    // as compileFunctionInternal — see comment there for details).
    const std::string root_func_name = root_func.name;

    // Re-check cache under compile lock
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (compiled_functions.find(root_func_name) != compiled_functions.end()) {
            return true;
        }
    }

    // Create a shared LLVM context and module for the batch
    auto ctx = std::make_unique<llvm::LLVMContext>();
    auto module = std::make_unique<llvm::Module>("qore_jit_batch_" + root_func_name, *ctx);
    module->setDataLayout(jit->getDataLayout());

    std::vector<std::pair<const AbstractQoreFunctionVariant*, const QoreIRFunction*>>
        effect_functions;
    effect_functions.reserve(callees.size());
    size_t effect_function_count = 0;
    for (const auto& callee : callees) {
        if (++effect_function_count % 100 == 0
                && qore_check_cancel(nullptr, "JIT batch effect function collection")) {
            error = "cancelled during JIT batch effect function collection";
            return false;
        }
        effect_functions.emplace_back(callee.variant, callee.ir_func);
    }
    std::unordered_map<const AbstractQoreFunctionVariant*, QoreIRFunctionEffectSummary>
        effect_summaries;
    if (!qore_ir_compute_function_effect_summaries(effect_functions, effect_summaries)) {
        error = "cancelled during JIT batch function effect analysis";
        return false;
    }

    // Build the batch callee map: variant → BatchCalleeInfo
    // This tells the lowerer which CallDirect targets are in-module
    // and whether they support direct LLVM arg passing (Approach B)
    std::unordered_map<const AbstractQoreFunctionVariant*, BatchCalleeInfo> batch_callee_map;
    for (const auto& callee : callees) {
        BatchCalleeInfo info;
        info.name = callee.ir_func->name;
        info.approach_b_eligible = callee.approach_b_eligible;
        info.num_params = callee.num_params;
        auto summary = effect_summaries.find(callee.variant);
        info.may_invalidate_external_caches = summary == effect_summaries.end()
            || summary->second.may_invalidate_external_caches;
        info.may_modify_runtime_locals = summary == effect_summaries.end()
            || summary->second.may_modify_runtime_locals;
        if (summary != effect_summaries.end()) {
            info.modified_runtime_locals =
                summary->second.modified_runtime_locals;
        }
        info.never_returns_nothing = summary != effect_summaries.end()
            && summary->second.never_returns_nothing;
        info.return_kind = qore_ir_get_fast_entry_return_kind(
            callee.variant, info.never_returns_nothing);
        info.capture_locals = callee.capture_locals;
        info.capture_kinds.reserve(info.capture_locals.size());
        for (size_t i = 0; i < info.capture_locals.size(); ++i) {
            if (i && !(i % 100)
                    && qore_check_cancel(nullptr,
                        "JIT batch closure capture ABI classification")) {
                error = "cancelled during JIT batch closure capture ABI classification";
                return false;
            }
            info.capture_kinds.push_back(
                qore_ir_get_scalar_local_kind(info.capture_locals[i]));
        }
        {
            const UserVariantBase* uvb = callee.variant->getUserVariantBase();
            const UserSignature* sig = uvb ? uvb->getUserSignature() : nullptr;
            info.param_kinds = qore_ir_get_fast_entry_param_kinds(*callee.ir_func, sig);
            info.param_rejects_nothing = qore_ir_get_fast_entry_param_rejects_nothing(sig);
        }
        if (info.approach_b_eligible) {
            info.fast_name = callee.ir_func->name + "_fast";
        }
        batch_callee_map[callee.variant] = std::move(info);
    }
    if (!std::getenv("QORE_DISABLE_JIT_INTERPROCEDURAL_SUMMARIES")) {
        if (!qore_ir_resolve_batch_function_summaries(
                effect_functions, batch_callee_map)) {
            error = "cancelled during JIT batch function summary analysis";
            return false;
        }
    }
    if (rewrite_root
            && !std::getenv("QORE_DISABLE_JIT_AGGREGATE_RETURN_PROJECTION")) {
        size_t projections = qore_ir_fuse_batch_aggregate_return_projections(
            *rewrite_root, batch_callee_map);
        size_t late_specializations = 0;
        if (projections
                && !std::getenv("QORE_DISABLE_JIT_LATE_SPECIALIZATION")) {
            constexpr size_t max_rounds = 4;
            for (size_t round = 0; round < max_rounds; ++round) {
                size_t changes = 0;
                changes += qore_ir_propagate_exact_boxed_local_facts(
                    *rewrite_root, true);
                changes += qore_ir_specialize_proven_boxed_operations(
                    *rewrite_root);
                changes += qore_ir_specialize_proven_collection_operations(
                    *rewrite_root);
                changes += qore_ir_specialize_proven_native_operations(
                    *rewrite_root);
                late_specializations += changes;
                if (!changes) {
                    break;
                }
            }
        }
        if (std::getenv("QORE_JIT_TIMING")) {
            fprintf(stderr,
                "[BG-JIT] fused %zu aggregate-return projection group(s)"
                " and applied %zu late specialization(s) in '%s'\n",
                projections, late_specializations,
                root_func_name.c_str());
        }
    }

    // Determine which callees need their bodies compiled vs just forward-declared.
    // Callees already JIT-compiled just need a declaration (LLVM will resolve the
    // existing symbol); callees not yet compiled need full lowering.
    std::unordered_set<std::string> already_compiled;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        for (const auto& callee : callees) {
            if (compiled_functions.find(callee.ir_func->name) != compiled_functions.end()) {
                already_compiled.insert(callee.ir_func->name);
            }
        }
    }

    // Pre-copy all callee names before any LLVM operations.
    // After addIRModule()/lookup(), LLVM 21 on Linux can corrupt adjacent heap memory
    // (specifically std::string::_M_string_length), so we capture callee names now
    // while the ir_func pointers and their name fields are guaranteed intact.
    std::vector<std::string> callee_names;
    callee_names.reserve(callees.size());
    for (const auto& callee : callees) {
        callee_names.push_back(callee.ir_func->name);
    }

    // Forward-declare all callee functions in the module so they can be referenced
    // before their bodies are lowered.  All JIT functions share the same signature:
    // uint64_t fname(ExceptionSink* xsink)
    auto* i64_ty = llvm::Type::getInt64Ty(*ctx);
    auto* ptr_ty = llvm::PointerType::get(*ctx, 0);
    auto* fn_type = llvm::FunctionType::get(i64_ty, {ptr_ty}, false);
    for (const auto& callee : callees) {
        llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
                callee.ir_func->name, *module);
    }

    // Forward-declare fast entry functions for Approach B eligible callees:
    // uint64_t callee_fast(arg0, arg1, ..., ptr xsink)
    for (const auto& callee : callees) {
        auto info_it = batch_callee_map.find(callee.variant);
        if (info_it == batch_callee_map.end()
                || !info_it->second.approach_b_eligible) {
            continue;
        }
        const std::vector<BatchCalleeParamKind>* param_kinds =
            info_it != batch_callee_map.end() ? &info_it->second.param_kinds : nullptr;
        auto* double_ty = llvm::Type::getDoubleTy(*ctx);
        std::vector<llvm::Type*> fast_params;
        fast_params.reserve(callee.num_params + callee.capture_locals.size() + 1);
        for (unsigned i = 0; i < callee.num_params; ++i) {
            if (i && !(i % 100)
                    && qore_check_cancel(nullptr, "JIT batch fast-entry declaration parameter setup")) {
                error = "cancelled during JIT batch fast-entry declaration parameter setup";
                return false;
            }
            BatchCalleeParamKind kind = param_kinds && i < param_kinds->size()
                ? (*param_kinds)[i] : BatchCalleeParamKind::Boxed;
            fast_params.push_back(qore_jit_fast_entry_param_type(kind, i64_ty, double_ty));
        }
        for (size_t i = 0; i < info_it->second.capture_kinds.size(); ++i) {
            if (i && !(i % 100)
                    && qore_check_cancel(nullptr,
                        "JIT batch closure capture declaration")) {
                error = "cancelled during JIT batch closure capture declaration";
                return false;
            }
            fast_params.push_back(qore_jit_fast_entry_param_type(
                info_it->second.capture_kinds[i], i64_ty, double_ty));
        }
        fast_params.push_back(ptr_ty);  // xsink
        llvm::Type* fast_return_ty = i64_ty;
        if (info_it->second.return_kind == BatchCalleeReturnKind::NativeFloat) {
            fast_return_ty = double_ty;
        } else if (info_it->second.return_kind == BatchCalleeReturnKind::NativeBool) {
            fast_return_ty = llvm::Type::getInt1Ty(*ctx);
        }
        auto* fast_fn_type = llvm::FunctionType::get(fast_return_ty, fast_params, false);
        std::string fast_name = callee.ir_func->name + "_fast";
        llvm::Function* fast_fn = llvm::Function::Create(fast_fn_type,
                llvm::Function::ExternalLinkage, fast_name, *module);
        // Mark as InlineHint for LLVM's inliner
        fast_fn->addFnAttr(llvm::Attribute::InlineHint);
    }

    // Also forward-declare the root function
    llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage,
            root_func.name, *module);

    // Create shared debug info for all functions in this batch module
    llvm::DIBuilder di_builder(*module);
    auto* di_file = di_builder.createFile("<jit-batch>", ".");
    auto* di_cu = di_builder.createCompileUnit(
        llvm::dwarf::DW_LANG_lo_user, di_file, "Qore JIT Batch", false, "", 0);
    if (!module->getModuleFlag("Dwarf Version")) {
        module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 5);
    }
    if (!module->getModuleFlag("Debug Info Version")) {
        module->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                llvm::DEBUG_METADATA_VERSION);
    }

    // Lower bodies for callees that aren't already compiled
    for (const auto& callee : callees) {
        if (!already_compiled.count(callee.ir_func->name)) {
            // Lower standard entry (only if not already compiled)
            QoreIRToLLVM lowering(*ctx);
            if (callee.deopt_counter) {
                lowering.setDeoptCounter(callee.deopt_counter);
            }
            // Callees also get the batch map so they can call each other directly
            lowering.setBatchCallees(&batch_callee_map);
            lowering.setSharedDebugInfo(&di_builder, di_cu);
            if (!lowering.lowerFunction(*callee.ir_func, *module, error)) {
                // If a callee fails to lower, fall back to compiling root alone
                printd(2, "QoreJIT::compileFunctionBatch() callee '%s' lowering failed: %s\n",
                    callee.ir_func->name.c_str(), error.c_str());
                error.clear();
                return compileFunctionInternal(root_func, error, root_deopt_counter);
            }
        }

        // Lower fast entry for Approach B eligible callees (even if standard entry
        // is already compiled — the fast entry is new and needs its body)
        auto fast_info = batch_callee_map.find(callee.variant);
        if (fast_info != batch_callee_map.end()
                && fast_info->second.approach_b_eligible) {
            std::string fast_name = callee.ir_func->name + "_fast";
            llvm::Function* fast_fn = module->getFunction(fast_name);
            assert(fast_fn && "fast entry function must be forward-declared");

            // Build param mapping: LocalVar* → LLVM function arg value
            const UserVariantBase* uvb = callee.variant->getUserVariantBase();
            const UserSignature* sig = uvb->getUserSignature();
            std::unordered_map<const void*, llvm::Value*> param_map;
            std::unordered_map<const void*, BatchCalleeParamKind> param_kind_map;
            const auto& callee_info = batch_callee_map.at(callee.variant);
            for (unsigned i = 0; i < callee.num_params; ++i) {
                const void* key = reinterpret_cast<const void*>(sig->lv[i]);
                param_map[key] = fast_fn->getArg(i);
                param_kind_map[key] = i < callee_info.param_kinds.size()
                    ? callee_info.param_kinds[i] : BatchCalleeParamKind::Boxed;
                fast_fn->getArg(i)->setName(std::string("arg") + std::to_string(i));
            }
            for (size_t i = 0; i < callee_info.capture_locals.size(); ++i) {
                if (i && !(i % 100)
                        && qore_check_cancel(nullptr,
                            "JIT batch closure capture mapping")) {
                    error = "cancelled during JIT batch closure capture mapping";
                    return false;
                }
                const void* key = reinterpret_cast<const void*>(
                    callee_info.capture_locals[i]);
                unsigned arg_index = callee.num_params + static_cast<unsigned>(i);
                param_map[key] = fast_fn->getArg(arg_index);
                param_kind_map[key] = callee_info.capture_kinds[i];
                fast_fn->getArg(arg_index)->setName(
                    std::string("capture") + std::to_string(i));
            }

            QoreIRToLLVM fast_lowering(*ctx);
            if (callee.deopt_counter) {
                fast_lowering.setDeoptCounter(callee.deopt_counter);
            }
            fast_lowering.setBatchCallees(&batch_callee_map);
            fast_lowering.setSharedDebugInfo(&di_builder, di_cu);
            const QoreTypeInfo* fast_return_type = sig->getReturnTypeInfo();
            bool fast_rejects_nothing_return = QoreTypeInfo::hasType(fast_return_type)
                && !QoreTypeInfo::parseAcceptsReturns(fast_return_type, NT_NOTHING);
            fast_lowering.setFastEntryMode(fast_name, &param_map, &param_kind_map,
                nullptr, callee_info.return_kind, fast_rejects_nothing_return);
            if (!fast_lowering.lowerFunction(*callee.ir_func, *module, error)) {
                // Fast entry failure is non-fatal: fall back to standard entry
                printd(2, "QoreJIT::compileFunctionBatch() fast entry '%s' lowering failed: %s\n",
                    fast_name.c_str(), error.c_str());
                error.clear();
                // Remove the fast entry from the batch map so callers don't try to use it
                auto map_it = batch_callee_map.find(callee.variant);
                if (map_it != batch_callee_map.end()) {
                    map_it->second.approach_b_eligible = false;
                }
            }
        }
    }

    // Lower the root function last (can call all callees)
    {
        QoreIRToLLVM lowering(*ctx);
        if (root_deopt_counter) {
            lowering.setDeoptCounter(root_deopt_counter);
        }
        lowering.setBatchCallees(&batch_callee_map);
        lowering.setSharedDebugInfo(&di_builder, di_cu);
        if (!lowering.lowerFunction(root_func, *module, error)) {
            return false;
        }
    }

    // Finalize shared debug info after all functions are lowered
    di_builder.finalize();

    // Run LLVM optimization passes
    optimizeModule(*module, getJITOptLevel());

    // Dump LLVM IR if requested (after optimization)
    if (getenv("QORE_DUMP_LLVM_IR")) {
        llvm::raw_fd_ostream llvm_dump(2, false);
        llvm_dump << "=== LLVM IR (batch) for " << root_func_name << " ===\n";
        module->print(llvm_dump, nullptr);
        llvm_dump << "=== END LLVM IR ===\n";
    }

    // Add the module to the JIT
    auto tsm = llvm::orc::ThreadSafeModule(std::move(module), std::move(ctx));
    auto err = jit->addIRModule(std::move(tsm));
    if (err) {
        error = "failed to add batch module to JIT: " + llvm::toString(std::move(err));
        return false;
    }

    // Look up the root function (triggers materialization/code generation)
    auto sym = jit->lookup(root_func_name);
    if (!sym) {
        error = "failed to look up compiled function '" + root_func_name + "': "
            + llvm::toString(sym.takeError());
        return false;
    }
    auto root_fn_ptr = sym->toPtr<uint64_t(ExceptionSink*)>();

    // Cache the root function
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        compiled_functions[root_func_name] = root_fn_ptr;
    }

    // Look up and cache all callee functions.
    // Use pre-copied callee_names (not callee.ir_func->name) as LLVM may have
    // corrupted adjacent heap memory during addIRModule()/lookup() above.
    for (size_t i = 0; i < callees.size(); ++i) {
        if (callees[i].batch_only) {
            continue;
        }
        auto callee_sym = jit->lookup(callee_names[i]);
        if (!callee_sym) {
            // Non-fatal: callees that fail lookup will fall back to runtime dispatch
            printd(2, "QoreJIT::compileFunctionBatch() callee '%s' lookup failed\n",
                callee_names[i].c_str());
            continue;
        }
        auto callee_fn_ptr = callee_sym->toPtr<uint64_t(ExceptionSink*)>();
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            compiled_functions[callee_names[i]] = callee_fn_ptr;
        }
    }

    return true;
}

JitFunctionPtr QoreJIT::lookupFunction(const std::string& name) const {
    std::lock_guard<std::mutex> lock(cache_mutex);
    auto it = compiled_functions.find(name);
    if (it == compiled_functions.end()) {
        return nullptr;
    }
    return it->second;
}

bool QoreJIT::executeWithFallback(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
        std::string& error, const std::unordered_set<const LocalVar*>* pre_instantiated) {
    // Pre-copy name before compileFunction() — LLVM 21 corrupts adjacent heap on Linux
    const std::string func_name = func.name;
    if (compileFunction(func, error)) {
        JitFunctionPtr fn = lookupFunction(func_name);
        if (fn) {
            // Execute the JIT-compiled function
            // NOTE: Do NOT clear the runtime stack location here.
            // JIT-compiled code calls into normal Qore functions which manage
            // their own stack locations via CodeEvaluationHelper. Clearing
            // the stack breaks exception call stacks.
            uint64_t result_bits = fn(xsink);

            // Reconstruct QoreValue from NaN-boxed bits
            QoreValue result;
            std::memcpy(&result, &result_bits, sizeof(result));
            return_value = result;
            return true;
        }
    }
    // Fallback to IR interpreter
    if (deopt_policy == DeoptPolicy::DisableJit) {
        return false;
    }
    return QoreIRInterpreter::execute(func, return_value, xsink, nullptr, nullptr, nullptr,
        pre_instantiated);
}

void QoreJIT::setDeoptPolicy(DeoptPolicy policy) {
    deopt_policy = policy;
}

uint64_t QoreJIT::getIRThreshold() {
    return ir_threshold;
}

uint64_t QoreJIT::getJITThreshold() {
    return jit_threshold;
}

void QoreJIT::setIRThreshold(uint64_t t) {
    ir_threshold = t;
}

void QoreJIT::setJITThreshold(uint64_t t) {
    jit_threshold = t;
}

void QoreJIT::startBackgroundThread() {
    // Start the background compilation thread if not already running
    if (!bg_thread_running.exchange(true, std::memory_order_acq_rel)) {
        bg_compile_thread = std::thread(&QoreJIT::bgCompileThreadLoop, this);
    }
}

void QoreJIT::releaseBgCompileWorkRefs(BgCompileWork& work) {
    for (auto* v : work.variant_refs) {
        v->deref();
    }
    work.variant_refs.clear();
}

void QoreJIT::finishBgCompileWork(BgCompileWork& work) {
    releaseBgCompileWorkRefs(work);

    bg_active_work.fetch_sub(1, std::memory_order_acq_rel);
    {
        // Clear the in-progress claim under bg_queue_mutex so a per-Program waiter
        // blocked on bg_in_progress is released as soon as this compile finishes,
        // even when other Programs' work remains in the queue.
        std::lock_guard<std::mutex> lock(bg_queue_mutex);
        bg_in_progress = nullptr;
    }
    // Always notify: both the shutdown waiter (queue empty && active_work == 0)
    // and per-Program waiters (in-progress no longer references their Program)
    // re-evaluate their predicates here.
    bg_queue_empty_cv.notify_all();
}

void QoreJIT::bgCompileThreadLoop() {
    // Background worker thread loop — compiles functions while main thread executes IR
    while (bg_thread_running.load(std::memory_order_acquire)) {
        BgCompileWork work;
        {
            std::unique_lock<std::mutex> lock(bg_queue_mutex);
            // Wait for work or shutdown signal
            bg_queue_cv.wait(lock, [this]() {
                return !bg_compile_queue.empty() || !bg_thread_running.load(std::memory_order_acquire);
            });
            if (bg_compile_queue.empty()) {
                // Check if all work is truly complete (queue empty AND no in-progress compilations)
                if (bg_active_work.load(std::memory_order_acquire) == 0) {
                    bg_queue_empty_cv.notify_all();
                }
                continue;  // Shutdown signal or spurious wakeup, check loop condition
            }
            work = std::move(bg_compile_queue.front());
            bg_compile_queue.pop();
            // Publish the in-progress work under bg_queue_mutex (before releasing
            // it to compile) so waitForBgCompileQueue(QoreProgram*) sees the claim
            // atomically with the pop and cannot race past an in-flight compile of
            // its Program.  Cleared in finishBgCompileWork() on every exit path.
            bg_in_progress = &work;
        }

        // Acquire compile lock (blocking is OK here — this is a dedicated background thread)
        std::lock_guard<std::mutex> lock(compile_mutex);

        // Pre-copy function names before any LLVM operations.
        // LLVM 21's addIRModule()/lookup() corrupts adjacent heap memory
        // (std::string::_M_string_length) on Linux, making any post-compilation
        // access to ir_func->name crash with a multi-terabyte strlen.
        const std::string saved_func_name = work.ir_func->name;
        std::vector<std::string> saved_callee_names;
        if (work.has_callees) {
            saved_callee_names.reserve(work.callees.size());
            for (const auto& callee : work.callees) {
                saved_callee_names.push_back(callee.ir_func->name);
            }
        }

        std::string error;
        bool success;

        if (getenv("QORE_JIT_TIMING")) {
            fprintf(stderr, "[BG-JIT] compiling '%s' on background thread\n", saved_func_name.c_str());
        }

        if (work.has_callees) {
            // Batch compile: root function + callees
            success = compileFunctionBatchInternal(*work.ir_func, error,
                work.deopt_ptr, work.callees, work.owned_ir_func.get());
        } else {
            // Single-function compilation
            success = compileFunctionInternal(*work.ir_func, error, work.deopt_ptr);
        }

        if (!success) {
            if (getenv("QORE_JIT_TIMING")) {
                fprintf(stderr, "[BG-JIT] failed to compile '%s': %s\n", saved_func_name.c_str(), error.c_str());
            }
            // Mark compilation as failed; uvb->jit_compile_failed will be set by evalTiered
            finishBgCompileWork(work);
            continue;
        }

        // Defensive check: only process if uvb is still valid (if jit_compile_state != 1, compilation was cancelled/failed)
        const UserVariantBase* root_uvb = work.uvb;
        if (!root_uvb) {
            if (getenv("QORE_JIT_TIMING")) {
                fprintf(stderr, "[BG-JIT] work item uvb is null, skipping\n");
            }
            finishBgCompileWork(work);
            continue;
        }

        // Verify the work item is still pending by checking compile state
        int state = const_cast<UserVariantBase*>(root_uvb)->jit_compile_state.load(std::memory_order_acquire);
        if (state != 1) {
            // Compilation was cancelled, failed, or already completed
            if (getenv("QORE_JIT_TIMING")) {
                fprintf(stderr, "[BG-JIT] compilation state is %d (not pending), skipping '%s'\n", state, saved_func_name.c_str());
            }
            finishBgCompileWork(work);
            continue;
        }

        // Lookup the compiled function
        JitFunctionPtr fn = lookupFunction(saved_func_name);
        if (!fn) {
            if (getenv("QORE_JIT_TIMING")) {
                fprintf(stderr, "[BG-JIT] lookup failed for '%s'\n", saved_func_name.c_str());
            }
            // Mark as failed
            const_cast<UserVariantBase*>(root_uvb)->jit_compile_state.store(2, std::memory_order_release);
            finishBgCompileWork(work);
            continue;
        }

        // Register the compiled function for the root uvb
        // Cast away const to set cached_jit_fn and tier
        UserVariantBase* mutable_uvb = const_cast<UserVariantBase*>(root_uvb);
        // Atomic store with release ordering ensures visibility before tier update
        mutable_uvb->cached_jit_fn.store(fn, std::memory_order_release);
        mutable_uvb->jit_compile_state.store(2, std::memory_order_relaxed);
        mutable_uvb->current_tier.store(UserVariantBase::TIER_JIT, std::memory_order_release);

        if (getenv("QORE_JIT_TIMING")) {
            fprintf(stderr, "[BG-JIT] compiled '%s' and promoted to JIT tier\n", saved_func_name.c_str());
        }

        // Register compiled function pointers for batch-compiled callees
        if (work.has_callees) {
            for (size_t i = 0; i < work.callees.size(); ++i) {
                const auto& callee = work.callees[i];
                if (callee.batch_only) {
                    continue;
                }
                JitFunctionPtr callee_fn = lookupFunction(saved_callee_names[i]);
                if (callee_fn && callee.variant) {
                    const UserVariantBase* callee_uvb = callee.variant->getUserVariantBase();
                    if (callee_uvb) {
                        UserVariantBase* mutable_callee = const_cast<UserVariantBase*>(callee_uvb);
                        mutable_callee->registerPrecompiledFunction(callee_fn);
                        if (getenv("QORE_JIT_TIMING")) {
                            fprintf(stderr, "[BG-JIT] batch-promoted callee '%s' to JIT tier\n",
                                    saved_callee_names[i].c_str());
                        }
                    }
                }
            }
        }

        finishBgCompileWork(work);
    }
}

void QoreJIT::enqueueBgCompile(const AbstractQoreFunctionVariant* variant, const QoreIRFunction* ir_func,
        void* deopt_counter, const std::vector<BatchCallee>* callees,
        std::shared_ptr<QoreIRFunction> owned_ir_func) {
    // NOTE: callers (evalTiered) already guard with CAS 0→1 on jit_compile_state
    // so we do NOT re-check here — the state is already 1 when we're called.
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;

    // Skip if LLVM is not initialized.  This is expected in tiered/jit mode at
    // parse-commit time: LLVM is not brought up until the first synchronous
    // compile (e.g. the top-level block via executeWithFallback).  The caller
    // already claimed the compile slot (jit_compile_state 0→1); release it here so
    // the function can be re-submitted on a later call once LLVM is available
    // (via the execution-count threshold), instead of being stuck at the IR tier
    // forever — the CAS 0→1 in attemptJITCompilation would otherwise never
    // succeed again.
    if (!jit) {
        if (uvb) {
            const_cast<UserVariantBase*>(uvb)->jit_compile_state.store(0, std::memory_order_release);
        }
        return;
    }

    // Ensure background thread is running
    startBackgroundThread();

    if (!uvb) {
        return;
    }

    // Increment active work counter before adding to queue
    bg_active_work.fetch_add(1, std::memory_order_relaxed);

    BgCompileWork work;
    work.uvb = uvb;
    work.owned_ir_func = std::move(owned_ir_func);
    work.ir_func = work.owned_ir_func
        ? work.owned_ir_func.get() : ir_func;
    work.deopt_ptr = deopt_counter;
    work.variant_refs.reserve(1 + (callees ? callees->size() : 0));
    work.variant_refs.push_back(const_cast<AbstractQoreFunctionVariant*>(variant)->ref());
    if (callees) {
        work.callees = *callees;
        work.has_callees = true;
        for (const auto& callee : work.callees) {
            if (callee.variant) {
                work.variant_refs.push_back(const_cast<AbstractQoreFunctionVariant*>(callee.variant)->ref());
            }
        }
    } else {
        work.has_callees = false;
    }

    {
        std::lock_guard<std::mutex> lock(bg_queue_mutex);
        bg_compile_queue.push(std::move(work));
    }
    bg_queue_cv.notify_one();

    if (getenv("QORE_JIT_TIMING")) {
        fprintf(stderr, "[BG-JIT] enqueued compilation of '%s'\n", ir_func->name.c_str());
    }
}

void QoreJIT::waitForBgCompileQueue() {
    // Wait for all pending AND in-progress compilations to complete.
    // Both queue must be empty AND all active work must be done.
    std::unique_lock<std::mutex> lock(bg_queue_mutex);
    bg_queue_empty_cv.wait(lock, [this]() {
        return bg_compile_queue.empty() && bg_active_work.load(std::memory_order_acquire) == 0;
    });
}

bool QoreJIT::workReferencesPgm(const BgCompileWork& work, const QoreProgram* pgm) {
    // A null ir_func->pgm means the owning Program of this work item is unknown (e.g. closure IR
    // funcs created without a pgm binding). Such items must be treated as referencing ANY program
    // so they are always drained before any Program tears down (and frees) its source IR; otherwise
    // a background lowering can race a use-after-free on the source QoreIR being compiled.
    if (work.ir_func && (work.ir_func->pgm == pgm || !work.ir_func->pgm)) {
        return true;
    }
    for (const auto& callee : work.callees) {
        if (callee.ir_func && (callee.ir_func->pgm == pgm || !callee.ir_func->pgm)) {
            return true;
        }
    }
    return false;
}

void QoreJIT::waitForBgCompileQueue(QoreProgram* pgm) {
    if (!pgm) {
        return;
    }
    // Cancel still-queued compiles that reference this Program's IR (the Program
    // is being destroyed; compiling its IR would be pointless and would read
    // soon-to-be-freed program data), and wait only for an in-progress compile
    // that references it.  Compiles belonging to other Programs stay queued and
    // running, so Program teardown is no longer serialized behind the whole
    // global queue.
    std::vector<BgCompileWork> cancelled;
    {
        std::unique_lock<std::mutex> lock(bg_queue_mutex);
        if (!bg_compile_queue.empty()) {
            std::queue<BgCompileWork> kept;
            while (!bg_compile_queue.empty()) {
                BgCompileWork w = std::move(bg_compile_queue.front());
                bg_compile_queue.pop();
                if (workReferencesPgm(w, pgm)) {
                    cancelled.push_back(std::move(w));
                } else {
                    kept.push(std::move(w));
                }
            }
            bg_compile_queue = std::move(kept);
        }
        // Wait until the in-progress compile (if any) no longer touches pgm.  The
        // bg thread clears bg_in_progress under bg_queue_mutex and notifies, and
        // it does not hold bg_queue_mutex while compiling, so this cannot deadlock.
        bg_queue_empty_cv.wait(lock, [this, pgm]() {
            return !bg_in_progress || !workReferencesPgm(*bg_in_progress, pgm);
        });
    }
    // Release refs (which may destroy the now-orphaned variants) outside the lock.
    for (auto& w : cancelled) {
        // Reset the root compile state so the function can be re-submitted later
        // if its Program outlives this one (it was cancelled only because some
        // referenced callee belonged to the dying Program).
        if (w.uvb) {
            const_cast<UserVariantBase*>(w.uvb)->jit_compile_state.store(0, std::memory_order_release);
        }
        releaseBgCompileWorkRefs(w);
        int remaining = bg_active_work.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (remaining == 0) {
            std::lock_guard<std::mutex> lock(bg_queue_mutex);
            bg_queue_empty_cv.notify_all();
        }
    }
}

void QoreJIT::shutdown() {
    // Ensure background thread is stopped BEFORE any other shutdown
    bool was_running = bg_thread_running.exchange(false, std::memory_order_acq_rel);
    if (was_running) {
        // Signal the thread to wake up and exit
        {
            std::lock_guard<std::mutex> lock(bg_queue_mutex);
        }  // Release lock before notify
        bg_queue_cv.notify_one();
        // Join the background thread (it should exit quickly since bg_thread_running is false)
        if (bg_compile_thread.joinable()) {
            bg_compile_thread.join();
        }
    }

    // Process any remaining work items synchronously before shutting down LLVM
    // This ensures all pending compilations complete before context destruction
    while (true) {
        BgCompileWork work;
        {
            std::lock_guard<std::mutex> lock(bg_queue_mutex);
            if (bg_compile_queue.empty()) {
                break;
            }
            work = std::move(bg_compile_queue.front());
            bg_compile_queue.pop();
        }

        // Pre-copy name before compilation — LLVM 21 corrupts adjacent heap on Linux
        const std::string saved_func_name = work.ir_func->name;

        // Compile any remaining work synchronously
        std::string error;
        bool success;
        if (work.has_callees) {
            success = compileFunctionBatchInternal(*work.ir_func, error,
                work.deopt_ptr, work.callees, work.owned_ir_func.get());
        } else {
            success = compileFunctionInternal(*work.ir_func, error, work.deopt_ptr);
        }

        if (success) {
            JitFunctionPtr fn = lookupFunction(saved_func_name);
            if (fn && work.uvb) {
                UserVariantBase* mutable_uvb = const_cast<UserVariantBase*>(work.uvb);
                mutable_uvb->cached_jit_fn.store(fn, std::memory_order_release);
                mutable_uvb->jit_compile_state.store(2, std::memory_order_relaxed);
                mutable_uvb->current_tier.store(UserVariantBase::TIER_JIT, std::memory_order_release);
            }
        } else if (work.uvb) {
            UserVariantBase* mutable_uvb = const_cast<UserVariantBase*>(work.uvb);
            mutable_uvb->jit_compile_state.store(2, std::memory_order_release);
        }
        finishBgCompileWork(work);
    }

    // Then shut down LLVM (acquire compile_mutex to serialize with any ongoing compilation)
    std::lock_guard<std::mutex> compile_lock(compile_mutex);
    jit.reset();
    symbols_registered = false;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        compiled_functions.clear();
    }
    init_success = false;
}

bool QoreJIT::tryAcquireCompileLock() {
    return compile_mutex.try_lock();
}

void QoreJIT::releaseCompileLock() {
    compile_mutex.unlock();
}

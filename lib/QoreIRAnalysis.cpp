/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRAnalysis.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    This source is released under the MIT license; see README-LICENSE.
*/

#include <qore/intern/QoreIRAnalysis.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/QoreJITIncludes.h>

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <limits>
#include <unordered_set>

static bool qore_ir_analysis_cancelled(size_t& count, const char* operation) {
    return ++count % 100 == 0 && qore_check_cancel(nullptr, operation);
}

bool qore_ir_visit_value_operands(const QoreIRInstruction& inst, const QoreIRValueVisitor& visitor,
        size_t* check_count, const char* operation) {
    auto visit = [&](QoreIRValue operand) {
        if (check_count && qore_ir_analysis_cancelled(*check_count, operation)) {
            return false;
        }
        visitor(operand);
        return true;
    };
    for (QoreIRValue operand : inst.operands) {
        if (!visit(operand)) {
            return false;
        }
    }

    switch (inst.opcode) {
        case QoreIROpcode::BrIf:
            return visit(static_cast<const QoreIRBranchIfInstruction&>(inst).condition);
        case QoreIROpcode::SwitchInt:
            return visit(static_cast<const QoreIRSwitchIntInstruction&>(inst).switch_val);
        case QoreIROpcode::SwitchString:
            return visit(static_cast<const QoreIRSwitchStringInstruction&>(inst).switch_val);
        case QoreIROpcode::IteratorCreate:
        case QoreIROpcode::IteratorCreateIterate:
            return visit(static_cast<const QoreIRIteratorCreateInstruction&>(inst).iterable);
        case QoreIROpcode::IteratorNext:
            return visit(static_cast<const QoreIRIteratorNextInstruction&>(inst).iterator);
        case QoreIROpcode::Return: {
            const auto& ret = static_cast<const QoreIRReturnInstruction&>(inst);
            if (ret.has_value) {
                return visit(ret.value);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

static void qore_ir_visit_unique_successor(QoreIRBasicBlock* block,
        std::unordered_set<QoreIRBasicBlock*>& visited, const QoreIRBlockVisitor& visitor) {
    if (block && visited.insert(block).second) {
        visitor(block);
    }
}

void qore_ir_visit_successors(const QoreIRInstruction& inst, const QoreIRBlockVisitor& visitor) {
    std::unordered_set<QoreIRBasicBlock*> visited;
    auto visit = [&](QoreIRBasicBlock* block) {
        qore_ir_visit_unique_successor(block, visited, visitor);
    };

    switch (inst.opcode) {
        case QoreIROpcode::Br:
            visit(static_cast<const QoreIRBranchInstruction&>(inst).target);
            break;
        case QoreIROpcode::BrIf: {
            const auto& branch = static_cast<const QoreIRBranchIfInstruction&>(inst);
            visit(branch.true_target);
            visit(branch.false_target);
            break;
        }
        case QoreIROpcode::BranchIfLtLocalInt: {
            const auto& branch = static_cast<const QoreIRBranchIfLtLocalIntInstruction&>(inst);
            visit(branch.true_target);
            visit(branch.false_target);
            break;
        }
        case QoreIROpcode::SwitchInt: {
            const auto& switch_inst = static_cast<const QoreIRSwitchIntInstruction&>(inst);
            visit(switch_inst.default_target);
            for (const QoreIRSwitchCase& c : switch_inst.cases) {
                visit(c.target);
            }
            break;
        }
        case QoreIROpcode::SwitchString: {
            const auto& switch_inst = static_cast<const QoreIRSwitchStringInstruction&>(inst);
            visit(switch_inst.default_target);
            for (const QoreIRSwitchStringCase& c : switch_inst.cases) {
                visit(c.target);
            }
            break;
        }
        case QoreIROpcode::IteratorNext: {
            const auto& next = static_cast<const QoreIRIteratorNextInstruction&>(inst);
            visit(next.continue_target);
            visit(next.done_target);
            break;
        }
        case QoreIROpcode::InvokeMethodDirect: {
            const auto& invoke = static_cast<const QoreIRInvokeMethodDirectInstruction&>(inst);
            visit(invoke.normal_target);
            break;
        }
        case QoreIROpcode::InvokeDotEvalMethodDirect: {
            const auto& invoke = static_cast<const QoreIRInvokeDotEvalMethodDirectInstruction&>(inst);
            visit(invoke.normal_target);
            break;
        }
        case QoreIROpcode::Invoke:
            visit(static_cast<const QoreIRInvokeInstruction&>(inst).normal_target);
            break;
        default:
            break;
    }
}

QoreIRControlFlowGraph::QoreIRControlFlowGraph(const QoreIRFunction& func) {
    blocks.reserve(func.blocks.size());
    size_t check_count = 0;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR control-flow analysis")) {
            cancelled = true;
            return;
        }
        block_ids.emplace(block.get(), blocks.size());
        blocks.push_back(block.get());
    }

    successors.resize(blocks.size());
    predecessors.resize(blocks.size());
    for (size_t block_id = 0; block_id < blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count, "IR control-flow analysis")) {
            cancelled = true;
            return;
        }
        const QoreIRBasicBlock* block = blocks[block_id];
        if (!block || block->instructions.empty()) {
            continue;
        }
        qore_ir_visit_successors(*block->instructions.back(), [&](QoreIRBasicBlock* successor) {
            auto it = block_ids.find(successor);
            if (it == block_ids.end()) {
                return;
            }
            successors[block_id].push_back(it->second);
            predecessors[it->second].push_back(block_id);
        });
    }

    reachable.assign(blocks.size(), 0);
    if (!blocks.empty()) {
        std::vector<size_t> worklist{0};
        reachable[0] = 1;
        while (!worklist.empty()) {
            if (qore_ir_analysis_cancelled(check_count, "IR reachability analysis")) {
                cancelled = true;
                return;
            }
            size_t block_id = worklist.back();
            worklist.pop_back();
            for (size_t successor : successors[block_id]) {
                if (!reachable[successor]) {
                    reachable[successor] = 1;
                    worklist.push_back(successor);
                }
            }
        }
    }

    const size_t word_count = (blocks.size() + 63) / 64;
    dominators.assign(blocks.size(), std::vector<uint64_t>(word_count, 0));
    for (size_t block_id = 0; block_id < blocks.size(); ++block_id) {
        if (qore_ir_analysis_cancelled(check_count, "IR dominator analysis")) {
            cancelled = true;
            return;
        }
        if (!reachable[block_id]) {
            if (word_count) {
                dominators[block_id][block_id / 64] |= uint64_t(1) << (block_id % 64);
            }
            continue;
        }
        if (block_id == 0) {
            dominators[block_id][0] = 1;
            continue;
        }
        for (size_t candidate = 0; candidate < blocks.size(); ++candidate) {
            if (reachable[candidate]) {
                dominators[block_id][candidate / 64] |= uint64_t(1) << (candidate % 64);
            }
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t block_id = 1; block_id < blocks.size(); ++block_id) {
            if (qore_ir_analysis_cancelled(check_count, "IR dominator analysis")) {
                cancelled = true;
                return;
            }
            if (!reachable[block_id]) {
                continue;
            }
            std::vector<uint64_t> next(word_count, std::numeric_limits<uint64_t>::max());
            bool saw_predecessor = false;
            for (size_t predecessor : predecessors[block_id]) {
                if (!reachable[predecessor]) {
                    continue;
                }
                saw_predecessor = true;
                for (size_t word = 0; word < word_count; ++word) {
                    next[word] &= dominators[predecessor][word];
                }
            }
            if (!saw_predecessor) {
                std::fill(next.begin(), next.end(), 0);
            }
            next[block_id / 64] |= uint64_t(1) << (block_id % 64);
            if (next != dominators[block_id]) {
                dominators[block_id] = std::move(next);
                changed = true;
            }
        }
    }
}

bool QoreIRControlFlowGraph::dominates(size_t dominator, size_t block) const {
    return dominator < blocks.size() && block < blocks.size() && reachable[block]
        && (dominators[block][dominator / 64] & (uint64_t(1) << (dominator % 64)));
}

std::vector<QoreIRNaturalLoop> qore_ir_find_natural_loops(const QoreIRControlFlowGraph& cfg) {
    if (cfg.cancelled) {
        return {};
    }
    std::unordered_map<size_t, std::unordered_set<size_t>> loops_by_header;
    size_t check_count = 0;
    for (size_t tail = 0; tail < cfg.blocks.size(); ++tail) {
        if (qore_ir_analysis_cancelled(check_count, "IR natural-loop analysis")) {
            return {};
        }
        if (!cfg.reachable[tail]) {
            continue;
        }
        for (size_t header : cfg.successors[tail]) {
            if (!cfg.dominates(header, tail)) {
                continue;
            }
            auto& loop_blocks = loops_by_header[header];
            loop_blocks.insert(header);
            if (loop_blocks.insert(tail).second && tail != header) {
                std::vector<size_t> worklist{tail};
                while (!worklist.empty()) {
                    if (qore_ir_analysis_cancelled(check_count, "IR natural-loop analysis")) {
                        return {};
                    }
                    size_t block = worklist.back();
                    worklist.pop_back();
                    for (size_t predecessor : cfg.predecessors[block]) {
                        if (loop_blocks.insert(predecessor).second && predecessor != header) {
                            worklist.push_back(predecessor);
                        }
                    }
                }
            }
        }
    }

    std::vector<QoreIRNaturalLoop> loops;
    for (auto& [header, loop_blocks] : loops_by_header) {
        if (qore_ir_analysis_cancelled(check_count, "IR natural-loop analysis")) {
            return {};
        }
        std::vector<size_t> outside_predecessors;
        for (size_t predecessor : cfg.predecessors[header]) {
            if (!loop_blocks.count(predecessor)) {
                outside_predecessors.push_back(predecessor);
            }
        }
        if (outside_predecessors.size() != 1) {
            continue;
        }
        size_t preheader = outside_predecessors.front();
        if (cfg.successors[preheader].size() != 1 || cfg.successors[preheader].front() != header) {
            continue;
        }
        QoreIRBasicBlock* preheader_block = cfg.blocks[preheader];
        if (!preheader_block || preheader_block->instructions.empty()
                || preheader_block->instructions.back()->opcode != QoreIROpcode::Br) {
            continue;
        }
        // The LLVM emitter currently resolves ordinary SSA operands in block-list
        // order.  Keep the preheader before every loop block until that backend
        // contract is replaced by dominance-order lowering.
        if (std::any_of(loop_blocks.begin(), loop_blocks.end(), [preheader](size_t block) {
                return block < preheader;
            })) {
            continue;
        }
        QoreIRNaturalLoop loop;
        loop.header = header;
        loop.preheader = preheader;
        loop.blocks.assign(loop_blocks.begin(), loop_blocks.end());
        std::sort(loop.blocks.begin(), loop.blocks.end());
        loops.push_back(std::move(loop));
    }
    std::sort(loops.begin(), loops.end(), [](const QoreIRNaturalLoop& left, const QoreIRNaturalLoop& right) {
        return left.blocks.size() < right.blocks.size();
    });
    return loops;
}

static bool qore_ir_is_native_scalar_pure_opcode(QoreIROpcode opcode) {
    switch (opcode) {
        case QoreIROpcode::AddInt:
        case QoreIROpcode::AddFloat:
        case QoreIROpcode::SubInt:
        case QoreIROpcode::SubFloat:
        case QoreIROpcode::MulInt:
        case QoreIROpcode::MulFloat:
        case QoreIROpcode::AndInt:
        case QoreIROpcode::OrInt:
        case QoreIROpcode::XorInt:
        case QoreIROpcode::ShlInt:
        case QoreIROpcode::ShrInt:
        case QoreIROpcode::EqInt:
        case QoreIROpcode::EqFloat:
        case QoreIROpcode::NeInt:
        case QoreIROpcode::NeFloat:
        case QoreIROpcode::LtInt:
        case QoreIROpcode::LtFloat:
        case QoreIROpcode::LeInt:
        case QoreIROpcode::LeFloat:
        case QoreIROpcode::GtInt:
        case QoreIROpcode::GtFloat:
        case QoreIROpcode::GeInt:
        case QoreIROpcode::GeFloat:
        case QoreIROpcode::CmpInt:
        case QoreIROpcode::CmpFloat:
        case QoreIROpcode::ToBool:
        case QoreIROpcode::Not:
        case QoreIROpcode::UnaryMinusInt:
        case QoreIROpcode::UnaryMinusFloat:
            return true;
        default:
            return false;
    }
}

static bool qore_ir_is_native_scalar_constant(QoreIROpcode opcode) {
    return opcode == QoreIROpcode::ConstInt || opcode == QoreIROpcode::ConstFloat
        || opcode == QoreIROpcode::ConstBool;
}

struct QoreIRScalarUse {
    const QoreIRInstruction* inst = nullptr;
    size_t block_id = 0;
};

using QoreIRScalarUses = std::unordered_map<uint32_t, std::vector<QoreIRScalarUse>>;

static bool qore_ir_collect_scalar_uses(const QoreIRFunction& func, QoreIRScalarUses& uses,
        size_t& check_count) {
    uses.clear();
    for (size_t block_id = 0; block_id < func.blocks.size(); ++block_id) {
        for (const auto& inst : func.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR scalar use analysis")) {
                return false;
            }
            if (!qore_ir_visit_value_operands(*inst, [&](QoreIRValue operand) {
                if (operand.isValid()) {
                    uses[operand.id].push_back({inst.get(), block_id});
                }
            }, &check_count, "IR scalar use analysis")) {
                return false;
            }
        }
    }
    return true;
}

static bool qore_ir_is_nonconsuming_scalar_use(const QoreIRFunction& func,
        const QoreIRInstruction& inst, bool allow_ir_only_store) {
    if (qore_ir_is_native_scalar_pure_opcode(inst.opcode)) {
        return true;
    }
    switch (inst.opcode) {
        case QoreIROpcode::BrIf:
        case QoreIROpcode::SwitchInt:
        case QoreIROpcode::AddAssignLocalInt:
            return true;
        case QoreIROpcode::StoreLocal: {
            if (!allow_ir_only_store) {
                return false;
            }
            const auto& store = static_cast<const QoreIRLocalInstruction&>(inst);
            return store.local
                && func.ir_only_locals.count(reinterpret_cast<const void*>(store.local));
        }
        default:
            return false;
    }
}

static bool qore_ir_has_only_nonconsuming_scalar_uses(const QoreIRFunction& func,
        QoreIRValue result, const QoreIRScalarUses& uses, bool allow_ir_only_store,
        size_t& check_count, bool& cancelled,
        const std::unordered_set<size_t>* required_blocks = nullptr) {
    auto use_it = uses.find(result.id);
    if (use_it == uses.end() || use_it->second.empty()) {
        return false;
    }
    for (const QoreIRScalarUse& use : use_it->second) {
        if (qore_ir_analysis_cancelled(check_count, "IR scalar use validation")) {
            cancelled = true;
            return false;
        }
        if (!use.inst || (required_blocks && !required_blocks->count(use.block_id))
                || !qore_ir_is_nonconsuming_scalar_use(func, *use.inst, allow_ir_only_store)) {
            return false;
        }
    }
    return true;
}

static void qore_ir_rewrite_value(QoreIRValue& value,
        const std::unordered_map<uint32_t, QoreIRValue>& replacements) {
    auto it = replacements.find(value.id);
    if (it != replacements.end()) {
        value = it->second;
    }
}

static bool qore_ir_rewrite_value_operands(QoreIRInstruction& inst,
        const std::unordered_map<uint32_t, QoreIRValue>& replacements,
        size_t& check_count, bool honor_cancellation) {
    for (QoreIRValue& operand : inst.operands) {
        if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")
                && honor_cancellation) {
            return false;
        }
        qore_ir_rewrite_value(operand, replacements);
    }

    switch (inst.opcode) {
        case QoreIROpcode::BrIf:
            qore_ir_rewrite_value(static_cast<QoreIRBranchIfInstruction&>(inst).condition, replacements);
            break;
        case QoreIROpcode::SwitchInt:
            qore_ir_rewrite_value(static_cast<QoreIRSwitchIntInstruction&>(inst).switch_val, replacements);
            break;
        case QoreIROpcode::SwitchString:
            qore_ir_rewrite_value(static_cast<QoreIRSwitchStringInstruction&>(inst).switch_val, replacements);
            break;
        case QoreIROpcode::IteratorCreate:
        case QoreIROpcode::IteratorCreateIterate:
            qore_ir_rewrite_value(static_cast<QoreIRIteratorCreateInstruction&>(inst).iterable, replacements);
            break;
        case QoreIROpcode::IteratorNext:
            qore_ir_rewrite_value(static_cast<QoreIRIteratorNextInstruction&>(inst).iterator, replacements);
            break;
        case QoreIROpcode::Phi: {
            auto& phi = static_cast<QoreIRPhiInstruction&>(inst);
            for (QoreIRPhiIncoming& incoming : phi.incoming) {
                if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")
                        && honor_cancellation) {
                    return false;
                }
                qore_ir_rewrite_value(incoming.value, replacements);
            }
            break;
        }
        case QoreIROpcode::LValuePathAssign:
        case QoreIROpcode::LValuePathCompound:
        case QoreIROpcode::LValuePathUnary:
        case QoreIROpcode::LValuePathBinaryMut:
        case QoreIROpcode::LValuePathTernary: {
            auto& path = static_cast<QoreIRLValuePathInstruction&>(inst);
            for (LVPathStep& step : path.path) {
                if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")
                        && honor_cancellation) {
                    return false;
                }
                if (step.operand_idx != UINT32_MAX) {
                    QoreIRValue operand(step.operand_idx);
                    qore_ir_rewrite_value(operand, replacements);
                    step.operand_idx = operand.id;
                }
                for (uint32_t& id : step.slice_operand_ids) {
                    if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")
                            && honor_cancellation) {
                        return false;
                    }
                    QoreIRValue operand(id);
                    qore_ir_rewrite_value(operand, replacements);
                    id = operand.id;
                }
            }
            break;
        }
        case QoreIROpcode::Return: {
            auto& ret = static_cast<QoreIRReturnInstruction&>(inst);
            if (ret.has_value) {
                qore_ir_rewrite_value(ret.value, replacements);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

struct QoreIRScalarExpressionKey {
    QoreIROpcode opcode = QoreIROpcode::ConstNothing;
    uint64_t constant_bits = 0;
    std::vector<uint32_t> operands;

    bool operator==(const QoreIRScalarExpressionKey& other) const {
        return opcode == other.opcode && constant_bits == other.constant_bits
            && operands == other.operands;
    }
};

struct QoreIRScalarExpressionKeyHash {
    size_t operator()(const QoreIRScalarExpressionKey& key) const {
        size_t hash = static_cast<size_t>(key.opcode) * 0x9e3779b1U;
        hash ^= static_cast<size_t>(key.constant_bits ^ (key.constant_bits >> 32));
        for (uint32_t operand : key.operands) {
            hash ^= static_cast<size_t>(operand) + 0x9e3779b9U + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

static QoreIRScalarExpressionKey qore_ir_get_scalar_expression_key(const QoreIRInstruction& inst) {
    QoreIRScalarExpressionKey key;
    key.opcode = inst.opcode;
    key.operands.reserve(inst.operands.size());
    for (QoreIRValue operand : inst.operands) {
        key.operands.push_back(operand.id);
    }
    if (qore_ir_is_native_scalar_constant(inst.opcode)) {
        const auto& constant = static_cast<const QoreIRConstInstruction&>(inst).constant;
        if (inst.opcode == QoreIROpcode::ConstInt) {
            key.constant_bits = static_cast<uint64_t>(constant.int_value);
        } else if (inst.opcode == QoreIROpcode::ConstFloat) {
            key.constant_bits = std::bit_cast<uint64_t>(constant.float_value);
        } else {
            key.constant_bits = constant.bool_value ? 1 : 0;
        }
    }
    return key;
}

static bool qore_ir_is_forwardable_scalar_load(const QoreIRFunction& func,
        const QoreIRInstruction& inst) {
    if (inst.opcode != QoreIROpcode::LoadLocal) {
        return false;
    }
    const auto& load = static_cast<const QoreIRLocalInstruction&>(inst);
    if (!load.local || load.is_closure || load.is_ref
            || !func.ir_only_locals.count(reinterpret_cast<const void*>(load.local))) {
        return false;
    }
    const QoreIRValueFacts* facts = func.getValueFacts(inst.result);
    if (!facts || facts->assigned_state != QoreIRAssignedState::Assigned || !facts->never_nothing) {
        return false;
    }
    return facts->representation == QoreIRValueRepresentation::NativeInt
        || facts->representation == QoreIRValueRepresentation::NativeFloat
        || facts->representation == QoreIRValueRepresentation::NativeBool;
}

static bool qore_ir_may_mutate_unknown_local(QoreIROpcode opcode) {
    switch (opcode) {
        case QoreIROpcode::NewObject:
        case QoreIROpcode::CreateParseRef:
        case QoreIROpcode::Call:
        case QoreIROpcode::CallIndirect:
        case QoreIROpcode::CallDirect:
        case QoreIROpcode::CallMethod:
        case QoreIROpcode::CallMethodDirect:
        case QoreIROpcode::InvokeMethodDirect:
        case QoreIROpcode::CallStatic:
        case QoreIROpcode::CallStaticDirect:
        case QoreIROpcode::DotEvalAny:
        case QoreIROpcode::DotEvalInt:
        case QoreIROpcode::DotEvalFloat:
        case QoreIROpcode::DotEvalString:
        case QoreIROpcode::DotEvalDate:
        case QoreIROpcode::DotEvalList:
        case QoreIROpcode::DotEvalHash:
        case QoreIROpcode::DotEvalObject:
        case QoreIROpcode::DotEvalMethodDirect:
        case QoreIROpcode::InvokeDotEvalMethodDirect:
        case QoreIROpcode::CallClosureDirect:
        case QoreIROpcode::BackgroundInt:
        case QoreIROpcode::Invoke:
        case QoreIROpcode::InvokeSimError:
        case QoreIROpcode::OnBlockExit:
        case QoreIROpcode::ScopeExit:
        case QoreIROpcode::Backquote:
        case QoreIROpcode::StoreLValue:
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
        case QoreIROpcode::PopAny:
        case QoreIROpcode::PushAny:
        case QoreIROpcode::SpliceLValue:
        case QoreIROpcode::LValuePathAssign:
        case QoreIROpcode::LValuePathCompound:
        case QoreIROpcode::LValuePathUnary:
        case QoreIROpcode::LValuePathBinaryMut:
        case QoreIROpcode::LValuePathTernary:
            return true;
        default:
            return false;
    }
}

struct QoreIRScalarCSEStats {
    size_t loads_forwarded = 0;
    size_t expressions_eliminated = 0;
};

static size_t qore_ir_fold_constant_branches(QoreIRFunction& func) {
    size_t check_count = 0;
    std::unordered_map<uint32_t, bool> constants;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR constant branch folding")) {
            return 0;
        }
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR constant branch folding")) {
                return 0;
            }
            if (inst->opcode == QoreIROpcode::ConstBool && inst->result.isValid()) {
                constants.emplace(inst->result.id,
                    static_cast<const QoreIRConstInstruction&>(*inst).constant.bool_value);
            }
        }
    }

    struct Replacement {
        std::unique_ptr<QoreIRInstruction>* slot = nullptr;
        QoreIRBasicBlock* source = nullptr;
        QoreIRBasicBlock* target = nullptr;
        QoreIRBasicBlock* removed_target = nullptr;
    };
    std::vector<Replacement> replacements;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR constant branch folding")) {
            return 0;
        }
        if (block->instructions.empty()) {
            continue;
        }
        std::unique_ptr<QoreIRInstruction>& terminator = block->instructions.back();
        if (terminator->opcode != QoreIROpcode::BrIf || terminator->exception_target) {
            continue;
        }
        const auto& branch = static_cast<const QoreIRBranchIfInstruction&>(*terminator);
        auto constant = constants.find(branch.condition.id);
        if (constant == constants.end()) {
            continue;
        }
        QoreIRBasicBlock* target = constant->second ? branch.true_target : branch.false_target;
        QoreIRBasicBlock* removed_target = constant->second ? branch.false_target : branch.true_target;
        replacements.push_back({&terminator, block.get(), target,
            target == removed_target ? nullptr : removed_target});
    }

    size_t folded = 0;
    std::unordered_map<QoreIRBasicBlock*, std::unordered_set<QoreIRBasicBlock*>> removed_predecessors;
    for (const Replacement& replacement : replacements) {
        if (qore_ir_analysis_cancelled(check_count, "IR constant branch folding")) {
            break;
        }
        const QoreIRInstruction& original = **replacement.slot;
        auto branch = std::make_unique<QoreIRBranchInstruction>();
        branch->target = replacement.target;
        branch->cached_start_line = original.cached_start_line;
        branch->intrinsic = original.intrinsic;
        branch->loc = original.loc;
        branch->exception_target = original.exception_target;
        branch->element_type = original.element_type;
        branch->temp_scope_id = original.temp_scope_id;
        *replacement.slot = std::move(branch);
        if (replacement.removed_target) {
            removed_predecessors[replacement.removed_target].insert(replacement.source);
        }
        ++folded;
    }
    // Branch replacement is already committed; finish all matching phi updates
    // even after cancellation so the IR always has one incoming value per edge.
    for (const auto& [target, predecessors] : removed_predecessors) {
        (void)qore_ir_analysis_cancelled(check_count, "IR constant branch folding");
        for (const auto& inst : target->instructions) {
            (void)qore_ir_analysis_cancelled(check_count, "IR constant branch folding");
            if (inst->opcode != QoreIROpcode::Phi) {
                continue;
            }
            auto& phi = static_cast<QoreIRPhiInstruction&>(*inst);
            phi.incoming.erase(std::remove_if(phi.incoming.begin(), phi.incoming.end(),
                [&](const QoreIRPhiIncoming& incoming) {
                    (void)qore_ir_analysis_cancelled(check_count, "IR constant branch folding");
                    return predecessors.count(incoming.block);
                }), phi.incoming.end());
            phi.operands.clear();
            phi.operands.reserve(phi.incoming.size());
            for (const QoreIRPhiIncoming& incoming : phi.incoming) {
                (void)qore_ir_analysis_cancelled(check_count, "IR constant branch folding");
                phi.operands.push_back(incoming.value);
            }
        }
    }
    return folded;
}

static QoreIRScalarCSEStats qore_ir_eliminate_common_scalar_expressions(QoreIRFunction& func) {
    size_t check_count = 0;
    QoreIRScalarUses uses;
    if (!qore_ir_collect_scalar_uses(func, uses, check_count)) {
        return {};
    }
    bool cancelled = false;
    std::unordered_map<uint32_t, QoreIRValue> replacements;
    std::unordered_set<const QoreIRInstruction*> eliminated;
    std::unordered_set<const QoreIRInstruction*> forwarded_loads;
    for (const auto& block : func.blocks) {
        if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")) {
            return {};
        }
        std::unordered_map<QoreIRScalarExpressionKey, QoreIRValue,
            QoreIRScalarExpressionKeyHash> available;
        std::unordered_map<const LocalVar*, QoreIRValue> available_loads;
        for (const auto& inst_ptr : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination")) {
                return {};
            }
            QoreIRInstruction& inst = *inst_ptr;
            if (!qore_ir_rewrite_value_operands(inst, replacements, check_count, true)) {
                return {};
            }

            const LocalVar* written_local = nullptr;
            if (inst.opcode == QoreIROpcode::StoreLocal
                    || inst.opcode == QoreIROpcode::StoreClosure
                    || inst.opcode == QoreIROpcode::InstantiateLocal
                    || inst.opcode == QoreIROpcode::UninstantiateLocal) {
                written_local = static_cast<const QoreIRLocalInstruction&>(inst).local;
            } else if (inst.opcode == QoreIROpcode::AddAssignLocalInt) {
                written_local = static_cast<const QoreIRAddAssignLocalIntInstruction&>(inst).target;
            } else if (inst.opcode == QoreIROpcode::IncrementLocalInt) {
                written_local = static_cast<const QoreIRIncrementLocalIntInstruction&>(inst).local;
            }
            if (written_local) {
                available_loads.erase(written_local);
            } else if (qore_ir_may_mutate_unknown_local(inst.opcode)) {
                available_loads.clear();
            }

            if (qore_ir_is_forwardable_scalar_load(func, inst)
                    && qore_ir_has_only_nonconsuming_scalar_uses(func, inst.result, uses, true,
                        check_count, cancelled)) {
                const auto& load = static_cast<const QoreIRLocalInstruction&>(inst);
                auto [load_it, inserted] = available_loads.emplace(load.local, inst.result);
                if (!inserted) {
                    replacements.emplace(inst.result.id, load_it->second);
                    eliminated.insert(&inst);
                    forwarded_loads.insert(&inst);
                }
                continue;
            }
            if (cancelled) {
                return {};
            }

            bool candidate = inst.result.isValid() && !inst.exception_target
                && (qore_ir_is_native_scalar_constant(inst.opcode)
                    || qore_ir_is_native_scalar_pure_opcode(inst.opcode));
            if (!candidate
                    || !qore_ir_has_only_nonconsuming_scalar_uses(func, inst.result, uses, true,
                        check_count, cancelled)) {
                if (cancelled) {
                    return {};
                }
                continue;
            }
            QoreIRScalarExpressionKey key = qore_ir_get_scalar_expression_key(inst);
            auto [available_it, inserted] = available.emplace(std::move(key), inst.result);
            if (inserted) {
                continue;
            }
            replacements.emplace(inst.result.id, available_it->second);
            eliminated.insert(&inst);
        }
    }

    if (eliminated.empty()) {
        return {};
    }

    for (const auto& block : func.blocks) {
        (void)qore_ir_analysis_cancelled(check_count, "IR scalar common-expression elimination");
        auto& instructions = block->instructions;
        for (auto it = instructions.begin(); it != instructions.end();) {
            // Once commit starts, finish all rewrites even if cancellation is
            // requested so no use can reference an erased definition.
            if (++check_count % 100 == 0) {
                (void)qore_check_cancel(nullptr, "IR scalar common-expression elimination");
            }
            if (eliminated.count(it->get())) {
                it = instructions.erase(it);
            } else {
                (void)qore_ir_rewrite_value_operands(**it, replacements, check_count, false);
                ++it;
            }
        }
    }
    return {
        forwarded_loads.size(),
        eliminated.size() - forwarded_loads.size(),
    };
}

static bool qore_ir_collect_mutated_locals(const QoreIRNaturalLoop& loop, const QoreIRControlFlowGraph& cfg,
        std::unordered_set<const LocalVar*>& mutated, size_t& check_count) {
    for (size_t block_id : loop.blocks) {
        for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR loop mutation analysis")) {
                return false;
            }
            const QoreIRInstruction* inst = inst_ptr.get();
            switch (inst->opcode) {
                case QoreIROpcode::StoreLocal:
                case QoreIROpcode::StoreClosure:
                case QoreIROpcode::UninstantiateLocal:
                case QoreIROpcode::InstantiateLocal:
                    mutated.insert(static_cast<const QoreIRLocalInstruction*>(inst)->local);
                    break;
                case QoreIROpcode::AddAssignLocalInt: {
                    const auto* local_inst = static_cast<const QoreIRAddAssignLocalIntInstruction*>(inst);
                    mutated.insert(local_inst->target);
                    break;
                }
                case QoreIROpcode::IncrementLocalInt:
                    mutated.insert(static_cast<const QoreIRIncrementLocalIntInstruction*>(inst)->local);
                    break;
                default:
                    break;
            }
        }
    }
    return true;
}

static bool qore_ir_is_hoistable_load(const QoreIRFunction& func, const QoreIRInstruction& inst,
        const std::unordered_set<const LocalVar*>& mutated) {
    if (inst.opcode != QoreIROpcode::LoadLocal) {
        return false;
    }
    const auto& load = static_cast<const QoreIRLocalInstruction&>(inst);
    if (!load.local || load.is_closure || load.is_ref || mutated.count(load.local)
            || !func.ir_only_locals.count(reinterpret_cast<const void*>(load.local))) {
        return false;
    }
    const QoreIRValueFacts* facts = func.getValueFacts(inst.result);
    if (!facts || facts->assigned_state != QoreIRAssignedState::Assigned || !facts->never_nothing) {
        return false;
    }
    return facts->representation == QoreIRValueRepresentation::NativeInt
        || facts->representation == QoreIRValueRepresentation::NativeFloat
        || facts->representation == QoreIRValueRepresentation::NativeBool;
}

static bool qore_ir_is_borrowed_list_element_consumer(const QoreIRInstruction& inst,
        uint32_t value_id) {
    switch (inst.opcode) {
        case QoreIROpcode::HashKeyAccess:
        case QoreIROpcode::HashKeyAccessInt:
        case QoreIROpcode::EqHard:
        case QoreIROpcode::NeHard:
        case QoreIROpcode::ToBool:
            return true;
        case QoreIROpcode::ListAppend:
            return inst.operands.size() == 2 && inst.operands[1].id == value_id;
        case QoreIROpcode::ListSetValue:
            return inst.operands.size() == 3 && inst.operands[2].id == value_id;
        case QoreIROpcode::HashSetKeyValue:
            return inst.operands.size() == 3
                && (inst.operands[1].id == value_id || inst.operands[2].id == value_id);
        default:
            return false;
    }
}

struct QoreIRBorrowedLoopSafety {
    bool may_mutate_unknown = false;
    std::unordered_set<uint32_t> mutated_lists;
};

static bool qore_ir_collect_borrowed_loop_safety(const QoreIRNaturalLoop& loop,
        const QoreIRControlFlowGraph& cfg, QoreIRBorrowedLoopSafety& safety,
        size_t& check_count) {
    for (size_t block_id : loop.blocks) {
        for (const auto& inst : cfg.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR borrowed list source analysis")) {
                return false;
            }
            if (qore_ir_may_mutate_unknown_local(inst->opcode)) {
                safety.may_mutate_unknown = true;
            }
            switch (inst->opcode) {
                case QoreIROpcode::ListAppend:
                case QoreIROpcode::ListSetInt:
                case QoreIROpcode::ListSetFloat:
                case QoreIROpcode::ListSetValue:
                    if (!inst->operands.empty()) {
                        safety.mutated_lists.insert(inst->operands[0].id);
                    }
                    break;
                case QoreIROpcode::ListPush:
                    // A separately loaded local can alias the source list.
                    safety.may_mutate_unknown = true;
                    break;
                default:
                    break;
            }
        }
    }
    return true;
}

static size_t qore_ir_mark_borrowed_list_reads(const QoreIRControlFlowGraph& cfg,
        const std::vector<QoreIRNaturalLoop>& loops,
        const QoreIRScalarUses& uses, size_t& check_count) {
    std::unordered_map<uint32_t, size_t> definition_blocks;
    for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
        for (const auto& inst : cfg.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR borrowed list definition analysis")) {
                return 0;
            }
            if (inst->result.isValid()) {
                definition_blocks[inst->result.id] = block_id;
            }
        }
    }

    size_t changed = 0;
    for (const QoreIRNaturalLoop& loop : loops) {
        QoreIRBorrowedLoopSafety safety;
        if (!qore_ir_collect_borrowed_loop_safety(loop, cfg, safety, check_count)) {
            return changed;
        }
        if (safety.may_mutate_unknown) {
            continue;
        }
        std::unordered_set<size_t> loop_blocks(loop.blocks.begin(), loop.blocks.end());
        for (size_t block_id : loop.blocks) {
            for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR borrowed list read analysis")) {
                    return changed;
                }
                QoreIRInstruction& inst = *inst_ptr;
                if (inst.opcode != QoreIROpcode::ListGetValue || inst.operands.size() != 2
                        || !inst.result.isValid()) {
                    continue;
                }
                uint32_t source_id = inst.operands[0].id;
                auto source_def = definition_blocks.find(source_id);
                if (source_def != definition_blocks.end() && loop_blocks.count(source_def->second)) {
                    continue;
                }
                auto use_it = uses.find(inst.result.id);
                if (use_it == uses.end() || use_it->second.empty()) {
                    continue;
                }
                bool safe_uses = true;
                for (const QoreIRScalarUse& use : use_it->second) {
                    if (qore_ir_analysis_cancelled(check_count, "IR borrowed list use analysis")) {
                        return changed;
                    }
                    if (!use.inst || !loop_blocks.count(use.block_id)
                            || !qore_ir_is_borrowed_list_element_consumer(*use.inst, inst.result.id)) {
                        safe_uses = false;
                        break;
                    }
                }
                if (!safe_uses || safety.mutated_lists.count(source_id)) {
                    continue;
                }
                inst.opcode = QoreIROpcode::ListGetValueNoRef;
                ++changed;
            }
        }
    }
    return changed;
}

static size_t qore_ir_mark_in_place_list_pushes(QoreIRFunction& func,
        const QoreIRControlFlowGraph& cfg, const QoreIRScalarUses& uses,
        size_t& check_count) {
    std::unordered_map<uint32_t, QoreIRInstruction*> definitions;
    std::unordered_set<LocalVar*> candidate_locals;
    for (const auto& block : cfg.blocks) {
        for (const auto& inst : block->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR in-place list push definition analysis")) {
                return 0;
            }
            if (inst->result.isValid()) {
                definitions[inst->result.id] = inst.get();
            }
            if (inst->opcode == QoreIROpcode::LoadLocal
                    || inst->opcode == QoreIROpcode::StoreLocal) {
                auto* local_inst = static_cast<QoreIRLocalInstruction*>(inst.get());
                if (local_inst->local && !local_inst->is_ref && !local_inst->is_closure
                        && func.ir_only_locals.count(
                            reinterpret_cast<const void*>(local_inst->local))) {
                    candidate_locals.insert(local_inst->local);
                }
            }
        }
    }

    auto get_loaded_local = [&](QoreIRValue value) -> LocalVar* {
        auto def_it = definitions.find(value.id);
        if (def_it == definitions.end() || def_it->second->opcode != QoreIROpcode::LoadLocal) {
            return nullptr;
        }
        auto* load = static_cast<QoreIRLocalInstruction*>(def_it->second);
        return load->is_ref || load->is_closure ? nullptr : load->local;
    };
    auto get_paired_push_store = [&](const QoreIRInstruction& push,
            LocalVar* local) -> const QoreIRLocalInstruction* {
        if (push.opcode != QoreIROpcode::ListPush || push.operands.size() != 2
                || !push.result.isValid() || get_loaded_local(push.operands[0]) != local) {
            return nullptr;
        }
        auto use_it = uses.find(push.result.id);
        if (use_it == uses.end() || use_it->second.size() != 1 || !use_it->second[0].inst
                || use_it->second[0].inst->opcode != QoreIROpcode::StoreLocal) {
            return nullptr;
        }
        auto* store = static_cast<const QoreIRLocalInstruction*>(use_it->second[0].inst);
        return store->local == local && !store->weak && store->operands.size() == 1
                && store->operands[0].id == push.result.id ? store : nullptr;
    };
    auto is_exclusive_fresh_store = [&](const QoreIRLocalInstruction& store) {
        if (store.weak || store.operands.size() != 1) {
            return false;
        }
        auto def_it = definitions.find(store.operands[0].id);
        if (def_it == definitions.end()
                || (def_it->second->opcode != QoreIROpcode::MakeList
                    && def_it->second->opcode != QoreIROpcode::CreateEmptyList
                    && def_it->second->opcode != QoreIROpcode::CreateSizedList)) {
            return false;
        }
        auto use_it = uses.find(def_it->second->result.id);
        return use_it != uses.end() && use_it->second.size() == 1
            && use_it->second[0].inst == &store;
    };
    auto is_read_only_list_use = [](const QoreIRInstruction& inst, QoreIRValue value) {
        if (inst.operands.empty() || inst.operands[0].id != value.id) {
            return false;
        }
        switch (inst.opcode) {
            case QoreIROpcode::ListSize:
            case QoreIROpcode::ListGetInt:
            case QoreIROpcode::ListGetFloat:
            case QoreIROpcode::ListGetValue:
            case QoreIROpcode::ListGetValueNoRef:
            case QoreIROpcode::ListIndexDynamic:
                return true;
            default:
                return false;
        }
    };

    using FreshLocalSet = std::unordered_set<LocalVar*>;
    bool cancelled = false;
    auto transfer_block = [&](size_t block_id, FreshLocalSet state, bool mark,
            size_t& changed) -> FreshLocalSet {
        for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
            if (qore_ir_analysis_cancelled(check_count, "IR in-place list push escape analysis")) {
                cancelled = true;
                return state;
            }
            QoreIRInstruction& inst = *inst_ptr;
            if (!qore_ir_visit_value_operands(inst, [&](QoreIRValue operand) {
                LocalVar* local = get_loaded_local(operand);
                if (!local || !candidate_locals.count(local)) {
                    return;
                }
                bool preserves_freshness = is_read_only_list_use(inst, operand)
                    || (inst.opcode == QoreIROpcode::ListPush
                        && !inst.operands.empty() && inst.operands[0].id == operand.id
                        && get_paired_push_store(inst, local));
                if (!preserves_freshness) {
                    state.erase(local);
                }
            }, &check_count, "IR in-place list push escape analysis")) {
                cancelled = true;
                return state;
            }
            if (inst.opcode == QoreIROpcode::ListPush && inst.operands.size() == 2) {
                LocalVar* local = get_loaded_local(inst.operands[0]);
                const QoreIRLocalInstruction* store = local
                    ? get_paired_push_store(inst, local) : nullptr;
                const QoreIRValueFacts* facts = func.getValueFacts(inst.operands[0]);
                if (mark && store && state.count(local) && facts
                        && facts->assigned_state == QoreIRAssignedState::Assigned
                        && facts->never_nothing && QoreTypeInfo::isListType(facts->type_info)) {
                    inst.list_push_in_place = true;
                    const_cast<QoreIRLocalInstruction*>(store)->redundant_store = true;
                    ++changed;
                }
            }
            if (inst.opcode == QoreIROpcode::StoreLocal) {
                auto& store = static_cast<QoreIRLocalInstruction&>(inst);
                if (store.local && candidate_locals.count(store.local)) {
                    if (is_exclusive_fresh_store(store)) {
                        state.insert(store.local);
                    } else {
                        auto def_it = store.operands.empty()
                            ? definitions.end() : definitions.find(store.operands[0].id);
                        if (def_it == definitions.end()
                                || !get_paired_push_store(*def_it->second, store.local)) {
                            state.erase(store.local);
                        }
                    }
                }
            }
        }
        return state;
    };

    std::vector<FreshLocalSet> fresh_in(cfg.blocks.size(), candidate_locals);
    std::vector<FreshLocalSet> fresh_out(cfg.blocks.size(), candidate_locals);
    std::vector<size_t> worklist;
    std::vector<uint8_t> queued(cfg.blocks.size(), 0);
    for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
        if (cfg.reachable[block_id]) {
            worklist.push_back(block_id);
            queued[block_id] = 1;
        }
    }
    size_t ignored_changed = 0;
    while (!worklist.empty()) {
        if (qore_ir_analysis_cancelled(check_count, "IR in-place list push dataflow")) {
            return 0;
        }
        size_t block_id = worklist.back();
        worklist.pop_back();
        queued[block_id] = 0;
        FreshLocalSet input;
        if (block_id) {
            bool first = true;
            for (size_t predecessor : cfg.predecessors[block_id]) {
                if (!cfg.reachable[predecessor]) {
                    continue;
                }
                if (first) {
                    input = fresh_out[predecessor];
                    first = false;
                    continue;
                }
                for (auto it = input.begin(); it != input.end();) {
                    if (!fresh_out[predecessor].count(*it)) {
                        it = input.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
        fresh_in[block_id] = input;
        FreshLocalSet output = transfer_block(block_id, input, false, ignored_changed);
        if (cancelled) {
            return 0;
        }
        if (output == fresh_out[block_id]) {
            continue;
        }
        fresh_out[block_id] = std::move(output);
        for (size_t successor : cfg.successors[block_id]) {
            if (!queued[successor]) {
                queued[successor] = 1;
                worklist.push_back(successor);
            }
        }
    }

    size_t changed = 0;
    for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
        if (cfg.reachable[block_id]) {
            transfer_block(block_id, fresh_in[block_id], true, changed);
            if (cancelled) {
                return changed;
            }
        }
    }
    return changed;
}

void qore_ir_optimize(QoreIRFunction& func, QoreIROptimizationStats* stats) {
    QoreIROptimizationStats local_stats;
    if (getenv("QORE_DISABLE_IR_OPT")) {
        if (stats) {
            *stats = local_stats;
        }
        return;
    }
    QoreIRControlFlowGraph cfg(func);
    if (cfg.cancelled) {
        if (stats) {
            *stats = local_stats;
        }
        return;
    }
    std::vector<QoreIRNaturalLoop> loops = qore_ir_find_natural_loops(cfg);
    size_t check_count = 0;
    bool has_list_push = false;
    if (!getenv("QORE_DISABLE_IR_IN_PLACE_LIST_PUSH")) {
        for (const QoreIRBasicBlock* block : cfg.blocks) {
            for (const auto& inst : block->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR list push discovery")) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
                if (inst->opcode == QoreIROpcode::ListPush) {
                    has_list_push = true;
                    break;
                }
            }
            if (has_list_push) {
                break;
            }
        }
    }
    QoreIRScalarUses uses;
    if ((!loops.empty() || has_list_push)
            && !qore_ir_collect_scalar_uses(func, uses, check_count)) {
        if (stats) {
            *stats = local_stats;
        }
        return;
    }
    if (!getenv("QORE_DISABLE_IR_BORROWED_LIST_READS")) {
        local_stats.borrowed_list_reads = qore_ir_mark_borrowed_list_reads(
            cfg, loops, uses, check_count);
    }
    if (!getenv("QORE_DISABLE_IR_IN_PLACE_LIST_PUSH")) {
        local_stats.in_place_list_pushes = qore_ir_mark_in_place_list_pushes(
            func, cfg, uses, check_count);
    }
    if (getenv("QORE_DISABLE_IR_LICM")) {
        loops.clear();
    }
    local_stats.loops_analyzed = loops.size();

    for (const QoreIRNaturalLoop& loop : loops) {
        if (qore_ir_analysis_cancelled(check_count, "IR loop-invariant code motion")) {
            break;
        }
        std::unordered_set<size_t> loop_blocks(loop.blocks.begin(), loop.blocks.end());
        std::unordered_map<uint32_t, size_t> definition_blocks;
        std::unordered_map<uint32_t, QoreIRInstruction*> definitions;
        for (size_t block_id = 0; block_id < cfg.blocks.size(); ++block_id) {
            for (const auto& inst : cfg.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR loop-invariant code motion")) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
                if (inst->result.isValid()) {
                    definition_blocks[inst->result.id] = block_id;
                    definitions[inst->result.id] = inst.get();
                }
            }
        }

        std::unordered_set<const LocalVar*> mutated;
        if (!qore_ir_collect_mutated_locals(loop, cfg, mutated, check_count)) {
            break;
        }
        bool loop_may_invalidate_loads = false;
        for (size_t block_id : loop.blocks) {
            for (const auto& inst : cfg.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR loop invalidation analysis")) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
                if (qore_ir_may_mutate_unknown_local(inst->opcode)) {
                    loop_may_invalidate_loads = true;
                    break;
                }
            }
            if (loop_may_invalidate_loads) {
                break;
            }
        }
        std::unordered_set<uint32_t> safe_repeated_values;
        bool cancelled = false;
        for (size_t block_id : loop.blocks) {
            for (const auto& inst : cfg.blocks[block_id]->instructions) {
                if (qore_ir_analysis_cancelled(check_count, "IR loop use validation")) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
                if (inst->result.isValid()
                        && qore_ir_has_only_nonconsuming_scalar_uses(func, inst->result, uses, false,
                            check_count, cancelled, &loop_blocks)) {
                    safe_repeated_values.insert(inst->result.id);
                }
                if (cancelled) {
                    if (stats) {
                        *stats = local_stats;
                    }
                    return;
                }
            }
        }
        std::unordered_set<const QoreIRInstruction*> selected;
        std::vector<QoreIRInstruction*> order;
        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t block_id : loop.blocks) {
                for (const auto& inst_ptr : cfg.blocks[block_id]->instructions) {
                    if (qore_ir_analysis_cancelled(check_count, "IR loop-invariant code motion")) {
                        if (stats) {
                            *stats = local_stats;
                        }
                        return;
                    }
                    QoreIRInstruction* inst = inst_ptr.get();
                    if (selected.count(inst) || !inst->result.isValid() || inst->exception_target) {
                        continue;
                    }
                    bool candidate = qore_ir_is_native_scalar_constant(inst->opcode)
                        || qore_ir_is_hoistable_load(func, *inst, mutated)
                        || qore_ir_is_native_scalar_pure_opcode(inst->opcode);
                    if (!candidate
                            || (inst->opcode == QoreIROpcode::LoadLocal && loop_may_invalidate_loads)
                            || !safe_repeated_values.count(inst->result.id)) {
                        continue;
                    }
                    bool operands_invariant = true;
                    if (!qore_ir_visit_value_operands(*inst, [&](QoreIRValue operand) {
                        auto def = definition_blocks.find(operand.id);
                        if (def == definition_blocks.end() || !loop_blocks.count(def->second)) {
                            return;
                        }
                        auto defining = definitions.find(operand.id);
                        QoreIRInstruction* defining_inst = defining == definitions.end() ? nullptr : defining->second;
                        if (!defining_inst || !selected.count(defining_inst)) {
                            operands_invariant = false;
                        }
                    }, &check_count, "IR loop-invariant operand analysis")) {
                        if (stats) {
                            *stats = local_stats;
                        }
                        return;
                    }
                    if (!operands_invariant) {
                        continue;
                    }
                    selected.insert(inst);
                    order.push_back(inst);
                    changed = true;
                }
            }
        }
        if (order.empty()) {
            continue;
        }

        std::unordered_map<QoreIRInstruction*, std::unique_ptr<QoreIRInstruction>> moved;
        for (size_t block_id : loop.blocks) {
            auto& instructions = cfg.blocks[block_id]->instructions;
            for (auto it = instructions.begin(); it != instructions.end();) {
                if (selected.count(it->get())) {
                    QoreIRInstruction* raw = it->get();
                    moved.emplace(raw, std::move(*it));
                    it = instructions.erase(it);
                } else {
                    ++it;
                }
            }
        }
        auto& preheader_instructions = cfg.blocks[loop.preheader]->instructions;
        auto insert_at = preheader_instructions.end() - 1;
        for (QoreIRInstruction* inst : order) {
            auto moved_it = moved.find(inst);
            if (moved_it == moved.end()) {
                continue;
            }
            insert_at = preheader_instructions.insert(insert_at, std::move(moved_it->second));
            ++insert_at;
            ++local_stats.instructions_hoisted;
        }
    }

    if (!getenv("QORE_DISABLE_IR_CSE")) {
        QoreIRScalarCSEStats cse_stats = qore_ir_eliminate_common_scalar_expressions(func);
        local_stats.scalar_loads_forwarded = cse_stats.loads_forwarded;
        local_stats.scalar_expressions_eliminated = cse_stats.expressions_eliminated;
    }

    if (!getenv("QORE_DISABLE_IR_CONST_FOLD")) {
        local_stats.constant_branches_folded = qore_ir_fold_constant_branches(func);
    }

    if (stats) {
        *stats = local_stats;
    }
}

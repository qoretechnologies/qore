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
#include <cstdlib>
#include <limits>
#include <unordered_set>

static bool qore_ir_analysis_cancelled(size_t& count, const char* operation) {
    return ++count % 100 == 0 && qore_check_cancel(nullptr, operation);
}

void qore_ir_visit_value_operands(const QoreIRInstruction& inst, const QoreIRValueVisitor& visitor) {
    for (QoreIRValue operand : inst.operands) {
        visitor(operand);
    }

    switch (inst.opcode) {
        case QoreIROpcode::BrIf:
            visitor(static_cast<const QoreIRBranchIfInstruction&>(inst).condition);
            break;
        case QoreIROpcode::SwitchInt:
            visitor(static_cast<const QoreIRSwitchIntInstruction&>(inst).switch_val);
            break;
        case QoreIROpcode::SwitchString:
            visitor(static_cast<const QoreIRSwitchStringInstruction&>(inst).switch_val);
            break;
        case QoreIROpcode::IteratorCreate:
        case QoreIROpcode::IteratorCreateIterate:
            visitor(static_cast<const QoreIRIteratorCreateInstruction&>(inst).iterable);
            break;
        case QoreIROpcode::IteratorNext:
            visitor(static_cast<const QoreIRIteratorNextInstruction&>(inst).iterator);
            break;
        case QoreIROpcode::Return: {
            const auto& ret = static_cast<const QoreIRReturnInstruction&>(inst);
            if (ret.has_value) {
                visitor(ret.value);
            }
            break;
        }
        default:
            break;
    }
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
    local_stats.loops_analyzed = loops.size();
    size_t check_count = 0;

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
                    if (!candidate) {
                        continue;
                    }
                    bool operands_invariant = true;
                    qore_ir_visit_value_operands(*inst, [&](QoreIRValue operand) {
                        auto def = definition_blocks.find(operand.id);
                        if (def == definition_blocks.end() || !loop_blocks.count(def->second)) {
                            return;
                        }
                        auto defining = definitions.find(operand.id);
                        QoreIRInstruction* defining_inst = defining == definitions.end() ? nullptr : defining->second;
                        if (!defining_inst || !selected.count(defining_inst)) {
                            operands_invariant = false;
                        }
                    });
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

    if (stats) {
        *stats = local_stats;
    }
}

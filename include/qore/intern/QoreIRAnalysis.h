/* -*- indent-tabs-mode: nil -*- */
/*
    QoreIRAnalysis.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    This source is released under the MIT license; see README-LICENSE.
*/

#ifndef _QORE_INTERN_QOREIRANALYSIS_H
#define _QORE_INTERN_QOREIRANALYSIS_H

#include <qore/intern/QoreIR.h>

#include <functional>
#include <unordered_map>
#include <vector>

using QoreIRValueVisitor = std::function<void(QoreIRValue)>;
using QoreIRBlockVisitor = std::function<void(QoreIRBasicBlock*)>;

//! Visit every SSA operand exactly once, including dedicated instruction fields.
//! @param inst instruction whose operands are visited
//! @param visitor callback invoked once per operand
void qore_ir_visit_value_operands(const QoreIRInstruction& inst, const QoreIRValueVisitor& visitor);

//! Visit every normal control-flow successor exactly once.
//! @param inst control-flow instruction whose successors are visited
//! @param visitor callback invoked once per successor
void qore_ir_visit_successors(const QoreIRInstruction& inst, const QoreIRBlockVisitor& visitor);

struct QoreIRControlFlowGraph {
    //! Build reachability, predecessor, successor, and dominator information.
    //! @param func lowered IR function to analyze
    explicit QoreIRControlFlowGraph(const QoreIRFunction& func);

    //! @param dominator candidate dominator block ID
    //! @param block candidate dominated block ID
    //! @return true if @p dominator dominates @p block
    bool dominates(size_t dominator, size_t block) const;

    std::vector<QoreIRBasicBlock*> blocks;
    std::unordered_map<const QoreIRBasicBlock*, size_t> block_ids;
    std::vector<std::vector<size_t>> successors;
    std::vector<std::vector<size_t>> predecessors;
    std::vector<std::vector<uint64_t>> dominators;
    std::vector<uint8_t> reachable;
    bool cancelled = false;
};

struct QoreIRNaturalLoop {
    size_t header = 0;
    size_t preheader = 0;
    std::vector<size_t> blocks;
};

//! Find natural loops with a unique unconditional preheader.
//! @param cfg control-flow graph to analyze
//! @return natural loops ordered from smallest to largest
std::vector<QoreIRNaturalLoop> qore_ir_find_natural_loops(const QoreIRControlFlowGraph& cfg);

struct QoreIROptimizationStats {
    size_t loops_analyzed = 0;
    size_t instructions_hoisted = 0;
};

//! Run conservative, semantics-preserving optimizations on a lowered function.
//! @param func function to optimize in place
//! @param stats optional output statistics
void qore_ir_optimize(QoreIRFunction& func, QoreIROptimizationStats* stats = nullptr);

#endif

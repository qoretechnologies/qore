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
#include <unordered_set>
#include <utility>
#include <vector>

using QoreIRValueVisitor = std::function<void(QoreIRValue)>;
using QoreIRBlockVisitor = std::function<void(QoreIRBasicBlock*)>;

//! Visit every SSA operand exactly once, including dedicated instruction fields.
//! @param inst instruction whose operands are visited
//! @param visitor callback invoked once per operand
//! @param check_count optional shared cooperative-cancellation counter
//! @param operation cancellation operation name when @a check_count is provided
//! @return false when cancellation was requested
bool qore_ir_visit_value_operands(const QoreIRInstruction& inst, const QoreIRValueVisitor& visitor,
    size_t* check_count = nullptr, const char* operation = nullptr);

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
    size_t scalar_loads_forwarded = 0;
    size_t scalar_expressions_eliminated = 0;
    size_t fixed_lists_scalarized = 0;
    size_t scalar_list_queries_folded = 0;
    size_t typed_foreach_loops = 0;
    size_t constant_branches_folded = 0;
    size_t borrowed_list_reads = 0;
    size_t bounded_typed_list_reads = 0;
    size_t in_place_list_pushes = 0;
};

//! Conservative interprocedural effects used to preserve caller-side caches.
struct QoreIRFunctionEffectSummary {
    bool may_invalidate_external_caches = true;
    bool never_returns_nothing = false;
    //! True for parameters whose value is only observed during the call.
    //! A returned value is allowed because return lowering takes an owning ref.
    std::vector<uint8_t> param_noescape;
    //! True when the callee can rebind or mutate the parameter value locally.
    //! Pass-by-value parameter changes do not by themselves invalidate caller
    //! lvalue caches, but callers can use this bit to preserve COW semantics.
    std::vector<uint8_t> param_may_modify;
};

//! Return exact scalar closure captures that can be passed by value to a
//! nonescaping immediate call. The proof rejects writes, references, nested
//! closures, opaque AST access, and optional scalar declarations.
DLLLOCAL bool qore_ir_get_readonly_scalar_closure_captures(
    const QoreIRFunction& func, const LVarSet* captures,
    std::vector<const LocalVar*>& result);

//! Return true when one instruction can mutate state visible to its caller.
//! Direct function calls without reference arguments are handled by the
//! interprocedural fixed-point analysis and therefore return false here.
bool qore_ir_instruction_may_invalidate_caller_caches(
    const QoreIRFunction& func, const QoreIRInstruction* inst);

//! Compute conservative fixed-point effect summaries for a closed function set.
//! Unknown callees and calls with reference arguments are treated as mutating.
//! @param functions variant-to-IR pairs in the analyzed compilation group
//! @param summaries output summaries keyed by function variant
//! @return false when cooperative cancellation was requested
bool qore_ir_compute_function_effect_summaries(
    const std::vector<std::pair<const AbstractQoreFunctionVariant*, const QoreIRFunction*>>& functions,
    std::unordered_map<const AbstractQoreFunctionVariant*, QoreIRFunctionEffectSummary>& summaries);

//! Returns locals that cannot safely use native scalar storage because a load may
//! observe NOTHING, a lvalue mutation bypasses StoreLocal, or the analysis was
//! cancelled. @p initially_assigned contains locals that are assigned on function
//! entry, normally signature parameters.
DLLLOCAL std::unordered_set<const void*> qore_ir_get_native_unsafe_locals(
    const QoreIRFunction& ir_func, const std::unordered_set<const void*>& initially_assigned);

//! Run conservative, semantics-preserving optimizations on a lowered function.
//! @param func function to optimize in place
//! @param stats optional output statistics
void qore_ir_optimize(QoreIRFunction& func, QoreIROptimizationStats* stats = nullptr);

using QoreIRParamNoEscapeQuery =
    std::function<bool(const AbstractQoreFunctionVariant*, size_t)>;

//! Refine fresh-list mutation after interprocedural AOT effects are available.
//! @param func function to optimize in place
//! @param param_noescape returns true for proven nonescaping callee arguments
//! @return number of newly specialized list pushes
size_t qore_ir_optimize_fresh_list_calls(QoreIRFunction& func,
    const QoreIRParamNoEscapeQuery& param_noescape);

#endif

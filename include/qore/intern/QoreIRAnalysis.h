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
    size_t local_value_facts_refined = 0;
    size_t dense_list_facts_refined = 0;
    size_t dense_identity_map_joins_elided = 0;
    size_t native_local_loads_promoted = 0;
    size_t native_local_stores_eliminated = 0;
    size_t scalar_expressions_eliminated = 0;
    size_t fixed_lists_scalarized = 0;
    size_t fixed_hashes_scalarized = 0;
    size_t scalar_list_queries_folded = 0;
    size_t typed_foreach_loops = 0;
    size_t constant_branches_folded = 0;
    size_t borrowed_list_reads = 0;
    size_t bounded_typed_list_reads = 0;
    size_t bounded_boxed_direct_reads = 0;
    size_t in_place_list_pushes = 0;
    size_t in_place_string_appends = 0;
};

//! Conservative interprocedural effects used to preserve caller-side caches.
struct QoreIRFunctionEffectSummary {
    bool may_invalidate_external_caches = true;
    //! True when the callee can change a runtime local visible to its caller.
    //! This is narrower than may_invalidate_external_caches: writes to proven
    //! non-reference globals remain externally visible but cannot rebind a
    //! caller local.
    bool may_modify_runtime_locals = true;
    //! Exact runtime locals modified when may_modify_runtime_locals is false.
    //! Pointer identities are valid within one IR/JIT/AOT compilation group.
    std::vector<const void*> modified_runtime_locals;
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

//! Return the local directly written or invalidated by one instruction.
//! Covers ordinary stores, fused scalar writes, and local-rooted lvalue paths.
DLLLOCAL const LocalVar* qore_ir_get_written_local(
    const QoreIRInstruction* inst);

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

//! Return true unless an exact user variant proves that every parameter is
//! passed by value.
DLLLOCAL bool qore_ir_variant_has_reference_params(
    const AbstractQoreFunctionVariant* variant);

//! Returns locals that cannot safely use native scalar storage because a load may
//! observe NOTHING, a lvalue mutation bypasses StoreLocal, or the analysis was
//! cancelled. @p initially_assigned contains locals that are assigned on function
//! entry, normally signature parameters.
DLLLOCAL std::unordered_set<const void*> qore_ir_get_native_unsafe_locals(
    const QoreIRFunction& ir_func, const std::unordered_set<const void*>& initially_assigned);

//! Run conservative, semantics-preserving optimizations on a lowered function.
//! @param func function to optimize in place
//! @param stats optional output statistics
//! @param enable_licm enables loop-invariant code motion; pathological AOT
//! functions selected for outlining disable it so values are not hoisted
//! across prospective helper boundaries
void qore_ir_optimize(QoreIRFunction& func, QoreIROptimizationStats* stats = nullptr,
    bool enable_licm = true);

using QoreIRParamNoEscapeQuery =
    std::function<bool(const AbstractQoreFunctionVariant*, size_t)>;

//! Return true when an exact callee argument is a pure list-size consumer.
using QoreIRFreshListSizeQuery =
    std::function<bool(const AbstractQoreFunctionVariant*, size_t)>;

//! Return the constant key when an exact callee argument is a pure typed-hash
//! key consumer.
using QoreIRFreshHashKeyQuery = std::function<bool(
    const AbstractQoreFunctionVariant*, size_t, std::string&)>;

enum class QoreIRAggregateProjectionQueryKind : uint8_t {
    DiscardResult,
    ListSize,
    ListIndexInt,
    ListIndexFloat,
    ListIndexValue,
    ListIndexDynamicValue,
    HashKeyInt,
    HashKeyValue,
    HashKeyDynamicValue,
    AggregateSizeValue,
    AggregateExistsValue,
    AggregateValValue,
    AggregateEmptyValue,
};

using QoreIRAggregateProjectionQuery = std::function<bool(
    const AbstractQoreFunctionVariant*, const QoreIRInstruction*,
    QoreIRAggregateProjectionQueryKind, int64_t, const std::string&,
    int16_t&, int64_t&, int64_t&, double&,
    QoreIRCallDirectInstruction::AOTAggregateProjectionKind&,
    std::vector<QoreIRCallDirectInstruction::
        AOTAggregateProjectionDescriptor>&, std::vector<std::string>&)>;

//! Resolve an exact aggregate consumer call to a supported projection query.
using QoreIRAggregateConsumerQuery = std::function<bool(
    const AbstractQoreFunctionVariant*, const QoreIRInstruction*, size_t&,
    QoreIRAggregateProjectionQueryKind&, int64_t&, std::string&)>;

//! Refine fresh-list mutation after interprocedural AOT effects are available.
//! @param func function to optimize in place
//! @param param_noescape returns true for proven nonescaping callee arguments
//! @return number of newly specialized list pushes
size_t qore_ir_optimize_fresh_list_calls(QoreIRFunction& func,
    const QoreIRParamNoEscapeQuery& param_noescape);

//! Replace exact pure list-size calls on fresh scalar list literals with their
//! cardinality. Element-producing instructions remain in place so side effects
//! and exceptions retain source evaluation order.
//! @param func function to optimize in place
//! @param is_list_size returns true for exact pure list-size callee arguments
//! @return number of fresh list allocations and calls eliminated
size_t qore_ir_fold_fresh_list_size_calls(QoreIRFunction& func,
    const QoreIRFreshListSizeQuery& is_list_size);

//! Replace exact pure constant-key calls on fresh scalar hash literals with the
//! corresponding value operand. Other element-producing instructions remain
//! in place so side effects and exceptions retain source evaluation order.
//! @param func function to optimize in place
//! @param get_hash_key returns the key for exact pure typed-hash consumers
//! @return number of fresh hash allocations and calls eliminated
size_t qore_ir_fold_fresh_hash_key_calls(QoreIRFunction& func,
    const QoreIRFreshHashKeyQuery& get_hash_key);

//! Fuse a fresh fixed aggregate producer call with native projections.
/** A sole projection is emitted through the compact AOT call form. A fresh
    aggregate stored in an IR-only local is virtualized when every load is a
    supported native projection dominated by the definition.
    @param func function to optimize in place
    @param get_projection resolves exact aggregate producers
    @param get_consumer optionally resolves exact calls consuming an aggregate
    @return number of aggregate producer/consumer groups optimized */
size_t qore_ir_fuse_aggregate_return_projections(QoreIRFunction& func,
        const QoreIRAggregateProjectionQuery& get_projection,
        const QoreIRAggregateConsumerQuery& get_consumer = {},
        size_t* borrowed_projections = nullptr);

//! Replace dynamic numeric arithmetic and comparisons with native opcodes when
//! late AOT facts prove that every operand is assigned and has the required
//! representation.
//! @param func function to optimize in place
//! @param exception_edges_elided optional counter for throwing Invoke
//! terminators replaced with non-throwing native operations
//! @return number of instructions specialized
size_t qore_ir_specialize_proven_native_operations(QoreIRFunction& func,
    size_t* exception_edges_elided = nullptr);

using QoreIRExactBoxedReturnQuery = std::function<const QoreTypeInfo*(
    const AbstractQoreFunctionVariant*)>;

//! Import exact assigned boxed result facts for resolved calls.
size_t qore_ir_import_exact_boxed_call_facts(QoreIRFunction& func,
    const QoreIRExactBoxedReturnQuery& get_return_type);

//! Refine exact assigned boxed facts through safe local stores and CFG joins.
size_t qore_ir_propagate_exact_boxed_local_facts(QoreIRFunction& func,
    bool propagate_positive = true);

//! Specialize pseudo-method flags and generic collection operations from late facts.
size_t qore_ir_specialize_proven_boxed_operations(QoreIRFunction& func);

//! Mark assigned exact list and binary pseudo-method receivers for no-guard lowering.
size_t qore_ir_specialize_proven_collection_operations(QoreIRFunction& func);

using QoreIRCollectionProducerQuery = std::function<bool(
    const AbstractQoreFunctionVariant*, const QoreIRCallDirectInstruction*)>;

//! Fuse an imported optional integer collection projection with Eq/Ne.
size_t qore_ir_fuse_collection_producer_consumers(QoreIRFunction& func,
    const QoreIRCollectionProducerQuery& is_supported);

using QoreIRBoxedReturnParamQuery = std::function<bool(
    const AbstractQoreFunctionVariant*, const QoreIRInstruction*,
    int8_t&)>;

/** Proves that all values are assigned at the point immediately before an instruction.
    @param func the IR function containing the instruction and values
    @param point the instruction defining the analysis point
    @param values the values that must all be assigned and non-NOTHING
    @return true only when every value is proven assigned on every reachable path
 */
bool qore_ir_values_proven_assigned_at(const QoreIRFunction& func,
    const QoreIRInstruction* point, const std::vector<QoreIRValue>& values);

//! Return true when a fresh hash initializer already enforces the target value type.
/** The proof requires either an input-identical typed hash producer or assigned,
    non-NOTHING, input-identical facts for every value in a fresh hash literal.
    Missing facts, dynamic values, and non-literal hashes remain on the checked path. */
bool qore_ir_complex_hash_initializer_prechecked(const QoreIRFunction& func,
    const QoreIRInstruction* initializer, const QoreTypeInfo* target_type);

//! Return true when every constant key in a fresh hash literal is a target hashdecl member.
/** This proof removes only the redundant unknown-key scan. Member value type
    acceptance, defaults, and declaration-order normalization remain mandatory. */
bool qore_ir_hashdecl_literal_keys_prechecked(const QoreIRInstruction* initializer,
    const TypedHashDecl* target);

//! Return true when every constant-key hashdecl initializer value is already accepted.
/** The proof requires assigned, non-NOTHING, input-identical facts for each
    literal operand and the corresponding target member. Missing facts and
    values requiring conversions remain on the checked path.
    @param func function containing the initializer
    @param initializer candidate constant-key hash literal
    @param target target hashdecl
    @param operands_native_and_assigned true when the caller has already
           established assigned, non-boxed scalar facts for every operand
    @return true only when runtime member value checks can be elided */
bool qore_ir_hashdecl_literal_values_prechecked(const QoreIRFunction& func,
    const QoreIRInstruction* initializer, const TypedHashDecl* target,
    bool operands_native_and_assigned = false);

//! Return true when a literal contains every target member in declaration order.
/** @param initializer candidate constant-key hash literal
    @param target target hashdecl
    @return true only when runtime layout normalization can be elided */
bool qore_ir_hashdecl_literal_layout_prechecked(
    const QoreIRInstruction* initializer, const TypedHashDecl* target);

//! Replace an exact pure boxed passthrough call with its already-owned argument.
size_t qore_ir_fold_boxed_return_param_calls(QoreIRFunction& func,
    const QoreIRBoxedReturnParamQuery& get_return_param);

using QoreIRStringProducerQuery = std::function<bool(
    const AbstractQoreFunctionVariant*,
    const QoreIRStringConsumerCallInstruction*,
    QoreIRCallDirectInstruction::AOTStringConsumerKind)>;

//! Fuse exact string producers with sole supported consumers.
/** Supports direct consumers, single-assignment non-escaping string locals,
    and compatible producer PHIs. */
//! @param func function to optimize in place
//! @param is_supported returns true for calls with a compatible producer summary
//! @return number of producer/consumer pairs fused
size_t qore_ir_fuse_string_producer_consumers(QoreIRFunction& func,
    const QoreIRStringProducerQuery& is_supported);

//! Fuse exact string case transformations with supported sole consumers.
size_t qore_ir_fuse_string_transform_consumers(QoreIRFunction& func);

#endif

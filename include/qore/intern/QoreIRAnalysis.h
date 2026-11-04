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
    //! @param include_exception_edges include instruction exception-handler edges in the graph
    explicit QoreIRControlFlowGraph(const QoreIRFunction& func, bool include_exception_edges = false);

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
    size_t unreachable_blocks_pruned = 0;
    size_t borrowed_list_reads = 0;
    size_t bounded_typed_list_reads = 0;
    size_t bounded_boxed_direct_reads = 0;
    size_t in_place_list_pushes = 0;
    size_t in_place_string_appends = 0;
    size_t pure_calls_folded = 0;
    size_t pure_calls_commoned = 0;
    size_t redundant_phis_eliminated = 0;
    //! the checks of return values (CheckReturnValue) removed because the value facts prove them
    size_t return_checks_elided = 0;
};

//! Where the values produced by compile-time folding are consumed.
enum class QoreIRFoldTarget : unsigned char {
    //! the folded value is observed by the process that computed it (interpreter and JIT), so
    //! @ref QCF_PURE is enough
    Runtime,
    //! the folded value is written into an AOT image that runs on other hosts, so @ref QCF_PURE
    //! must be accompanied by @ref QCF_HOST_PORTABLE
    Image,
};

//! Everything compile-time folding needs beyond the function itself.
struct QoreIRFoldContext {
    //! where the folded values are consumed; the default is the conservative AOT-image answer, so
    //! a caller that folds for its own process must say so explicitly
    QoreIRFoldTarget target = QoreIRFoldTarget::Image;
    //! program context used to evaluate a call at compile time
    /** AOT lowering must supply this: neither the IR function nor the call instruction carries a
        program there.  When null the folder falls back to the call instruction's program and then
        to the current thread's program, which is what the interpreter and JIT paths have. */
    QoreProgram* pgm = nullptr;
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

//! Returns true if running the function can run a reflection builtin on its thread, apart from its exact callees
/** Reflection (get_local_vars(), set_local_var_value()) inspects only the frames of the current thread, so a frame can
    be reflected only while code that the function runs calls a reflection builtin.  The function may reflect if it
    calls a reflection builtin or a builtin that can run user code, makes a call not resolved to an exact variant, or
    handles a value that can hold an object (a released object runs its destructor).  See
    design/aot-reflection-visible-locals.md.
    @param func the function, lowered for a program that allows the debugger
    @param callees receives the exact user variants called, whose own effect decides the function's
    @param lvalue_release_reflects whether an update of a member that may release an object counts as possibly
    reflecting (its result is a scalar, so the other rules miss it): always for the IR and JIT tiers, which keep the
    parameters and the body locals of a reflection-free frame private; for AOT only when the analyzed frame has body
    locals, since its native method entries publish and reload the parameters around such an update
    (AOTEffectMemberEntry, with and without its effect entry), but its body locals stay private.  An update of a
    static, a global or a captured variable, or a write through a reference, that may release an object always counts:
    no entry reloads the parameters around it
    @return true if the function may reflect by itself; then \a callees is incomplete
*/
DLLLOCAL bool qore_ir_function_may_reflect_locally(const QoreIRFunction& func,
    std::vector<const AbstractQoreFunctionVariant*>& callees, bool lvalue_release_reflects = true);

//! Returns the parameters that the function may access after an instruction that may reflect
/** A reflection builtin run by a callee, a callback or a destructor can replace a parameter of the function's frame
    (set_local_var_value()); the function observes that only if it accesses the parameter afterwards, in the same
    block or a block reachable from it, before assigning it.  The rules are those of qore_ir_function_may_reflect_locally(), except that an
    exact user callee counts as reflecting; an instruction that is not a plain local access or a local-free scalar
    operation counts as accessing every parameter.
    @param func the function, lowered for a program that allows the debugger
    @param params receives the keys (LocalVar pointers) of the parameters accessed after a possible reflection
    @return false if cancelled; then \a params is incomplete and every parameter must be assumed
*/
DLLLOCAL bool qore_ir_get_params_read_after_reflection(const QoreIRFunction& func,
    std::unordered_set<const void*>& params);

//! Returns true if a value of the type can be or hold an object, whose destructor can run when it is released
//! @param ti the type; nullptr means any type
//! @param depth the nesting depth of complex container types already examined
DLLLOCAL bool qore_ir_type_may_hold_object(const QoreTypeInfo* ti, int depth = 0);

//! Returns true if assigning a variable of the type can release a value that can be or hold an object
/** As qore_ir_type_may_hold_object(), except that for a reference, the type that it refers to decides: a write through
    a reference replaces the value of the variable that it refers to, while releasing the reference itself releases
    no object.
    @param ti the declared type of the variable; nullptr means any type
*/
DLLLOCAL bool qore_ir_lvalue_type_may_hold_object(const QoreTypeInfo* ti);

//! Returns true if an instruction can release a value that may hold an object when it changes an lvalue
/** An assignment, a compound assignment, a removal, an element store, a scope exit of a local, or another update of a
    variable releases the value it replaces or removes, and the destructor of an object released runs user code, which
    can replace a local of the frame by name (set_local_var_value(), see design/aot-reflection-visible-locals.md).  The
    declared type of the variable decides (a reference: the type that it refers to); a member, whose type the
    instruction does not carry, or a variable of unknown type, may hold an object.  A value that the instruction
    removes and returns is released with the temporaries of its statement, or right away if it is not used.

    In a frame whose locals reflection can replace (QoreIRFunction::reflective_frame), the execution tiers reload the
    frame's locals from the runtime stack after such an instruction, as after a call.
*/
DLLLOCAL bool qore_ir_instruction_may_release_object(const QoreIRInstruction* inst);

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

//! Collect resolved direct callees represented by a lowered function.
/** Closure values are tracked through CreateClosure/CallClosureDirect pairs.
    The output is duplicate-free and excludes unresolved or dynamic calls. */
DLLLOCAL bool qore_ir_collect_resolved_callees(
    const QoreIRFunction& func,
    std::vector<const AbstractQoreFunctionVariant*>& callees);

//! Returns locals that cannot safely use native scalar storage because a load may
//! observe NOTHING, a lvalue mutation bypasses StoreLocal, or the analysis was
//! cancelled. @p initially_assigned contains locals that are assigned on function
//! entry, normally signature parameters.
DLLLOCAL std::unordered_set<const void*> qore_ir_get_native_unsafe_locals(
    const QoreIRFunction& ir_func, const std::unordered_set<const void*>& initially_assigned);

//! The definite assignment of the local variables of a function (see qore_ir_analyze_local_assignment())
struct QoreIRLocalAssignment {
    //! the locals that cannot use native scalar storage, as qore_ir_get_native_unsafe_locals() returns them
    std::unordered_set<const void*> unsafe_locals;
    //! the loads of local variables (LoadLocal) that may observe a variable that is not assigned
    std::unordered_set<const QoreIRInstruction*> unassigned_loads;
    //! false if the analysis was cancelled: every local is unsafe, and no load is known to observe an assignment
    bool complete = false;
};

//! Analyzes the definite assignment of the local variables of a function, as qore_ir_get_native_unsafe_locals()
//! does, and also finds each load that may observe a variable that is not assigned: a variable that a load observes
//! before its assignment is unsafe for all its loads, but its other loads can still observe an assignment
DLLLOCAL QoreIRLocalAssignment qore_ir_analyze_local_assignment(
    const QoreIRFunction& ir_func, const std::unordered_set<const void*>& initially_assigned);

//! Run conservative, semantics-preserving optimizations on a lowered function.
//! @param func function to optimize in place
//! @param stats optional output statistics
//! @param enable_licm enables loop-invariant code motion; pathological AOT
//! functions selected for outlining disable it so values are not hoisted
//! across prospective helper boundaries
//! @param fold_context what compile-time folding may assume and what it evaluates with
void qore_ir_optimize(QoreIRFunction& func, QoreIROptimizationStats* stats = nullptr,
    bool enable_licm = true, const QoreIRFoldContext& fold_context = QoreIRFoldContext());

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

//! Records the unfused form of calls that an optimization pass fuses (QoreIRDebuggerAlternative)
/** A pass that fuses a call of a user function or method with the instructions consuming its result, or replaces the
    call by its known result, keeps the call's unfused form when the callee's Program can be debugged: plan() it
    before the pass changes the call, take() the consumers out of each block before the pass erases them, and
    finish() after the pass.  The native code takes the unfused form while a debugger is attached.
*/
class QoreIRDebuggerAlternativeBuilder {
public:
    DLLLOCAL explicit QoreIRDebuggerAlternativeBuilder(QoreIRFunction& func) : func(func) {
    }

    //! Returns true if the callee of a call can be debugged (its Program allows debugging)
    DLLLOCAL static bool calleeMayBeDebugged(const QoreIRInstruction* call);

    //! Returns the resolved callee of a call, as calleeMayBeDebugged() resolves it, or nullptr if it is not known
    /** The lowering of a call that keeps an unfused form gates it on this callee's Program.
    */
    DLLLOCAL static const AbstractQoreFunctionVariant* resolvedCallee(const QoreIRInstruction* call);

    //! Returns true if the call needs an unfused form: a builder is enabled and its callee can be debugged
    DLLLOCAL bool needs(const QoreIRInstruction* call) const {
        return enabled && calleeMayBeDebugged(call);
    }

    //! Plans the unfused form of a call that the pass fuses
    /** @param call the instruction that the native code runs, which keeps the unfused form
        @param call_result the call's own result, before the pass replaces it
        @param call_nargs the number of the call's argument operands
        @param consumers the instructions that the pass eliminates, in their order; the last one defines the fused
        result; empty if the pass replaces the call's result
        @param available values available at the call besides its operands, such as operands that the pass appends
        @param aliases values that the consumers use in place of others: a local's load of the call's result (the
        call's result), or a load that the pass moves before the call (its moved copy)

        @return false if the consumers use a value that is not available at the call (other than a constant), so
        the pass must not fuse the call; true otherwise, also if the callee cannot be debugged
    */
    DLLLOCAL bool plan(QoreIRInstruction* call, QoreIRValue call_result, uint32_t call_nargs,
            std::vector<QoreIRInstruction*> consumers, const std::vector<QoreIRValue>& available = {},
            const std::unordered_map<uint32_t, QoreIRValue>& aliases = {});

    //! Plans the unfused form of a call shared by several calls that the pass fuses with one consumer (a phi)
    /** @param calls the fused calls, each of which makes the ordinary call while a debugger is attached
        @param base the value that \a consumer takes from the calls (the phi of their results)
        @param consumer the eliminated consumer, whose other operands must be constants
    */
    DLLLOCAL bool planShared(const std::vector<QoreIRInstruction*>& calls, QoreIRValue base,
            QoreIRInstruction* consumer);

    //! Plans a call that the pass keeps although native code takes its result from another value (a fold)
    /** @param call the call, which stays in its block; the pass leaves its uses
        @param native_value the value that native code gives its result, available at the call
        @param prefix eliminated instructions that the call's arguments need, in their order
    */
    DLLLOCAL bool planFold(QoreIRInstruction* call, QoreIRValue native_value,
            std::vector<QoreIRInstruction*> prefix = {});

    //! Plans a call that the pass keeps although native code does not make it, as its result is not used
    /** @param call the call, which stays in its block
        @param prefix eliminated instructions that the call's arguments need, in their order
        @param site a site from newSite() whose keyed instructions (planDerived()) take the call's result, or 0
        @param gated true if the call follows \a site, whose first call made the ordinary call, instead of storing
        its result for it
    */
    DLLLOCAL bool planEventsOnly(QoreIRInstruction* call, std::vector<QoreIRInstruction*> prefix = {},
            uint32_t site = 0, bool gated = false);

    //! Plans an instruction that the pass keeps although native code takes its result from another value
    /** The unfused form takes the call's ordinary result and lowers copies of \a consumers from it.  Unlike plan(),
        the consumers stay where they are; the copies are made now, with the facts that their results have now.

        @param kept the instruction that keeps the unfused form: the call (\a site 0), which makes the ordinary call
        while a debugger is attached, or a consumer of the call keyed on the call's \a site, which takes the result
        of the call's ordinary call when the call made it (planEventsOnly())
        @param kind Fold: native code gives \a kept's result \a native_value; Consumers: \a kept is a fused call
        @param native_value for Fold, the value that native code gives \a kept's result, available at \a kept
        @param site the site of the call, for a consumer keyed on it, or 0
        @param call_result the call's result, which the copies take in place of the values in \a aliases
        @param consumers the instructions copied, in their order; the last one defines the result
        @param aliases the values that the copies take from the call's result (\a call_result) or from the
        copy of a reload (\a reloads)
        @param reloads the values whose copies load their local again after the ordinary call (a guarded index)
        @param at_consumers true if the unfused form runs where the consumers are, so that their other operands are
        available; false if it runs at the call, so that their other operands must be constants
        @param site_gated true if \a kept is a call that follows \a site, making its own ordinary call when the site's
        first call made its ordinary call (a call whose arguments the site's calls give), instead of being keyed on it

        @return false if a copy cannot be made, so the pass must not fuse; true otherwise
    */
    DLLLOCAL bool planDerived(QoreIRInstruction* kept, QoreIRDebuggerAlternative::Kind kind,
            QoreIRValue native_value, uint32_t site, QoreIRValue call_result,
            const std::vector<const QoreIRInstruction*>& consumers,
            const std::unordered_map<uint32_t, QoreIRValue>& aliases,
            const std::unordered_map<uint32_t, const QoreIRInstruction*>& reloads, bool at_consumers,
            bool site_gated = false);

    //! Sets the native value of the last planDerived() plan of \a kept, for a value that the pass makes later
    DLLLOCAL void setNativeValue(const QoreIRInstruction* kept, QoreIRValue native_value);

    //! Returns a copy of a consumer instruction, without its unfused form, or nullptr if it cannot be copied
    DLLLOCAL static std::unique_ptr<QoreIRInstruction> cloneConsumer(const QoreIRInstruction& inst);

    //! Returns a new call site for planKeyed()
    DLLLOCAL uint32_t newSite() {
        return func.createValue().id;
    }

    //! Plans the unfused form of a fused call that shares the ordinary call of a site with other fused calls
    /** As plan(); \a store is true for the first call of the site, which makes the ordinary call.
    */
    DLLLOCAL bool planKeyed(uint32_t site, bool store, QoreIRInstruction* call, QoreIRValue call_result,
            uint32_t call_nargs, std::vector<QoreIRInstruction*> consumers,
            const std::unordered_map<uint32_t, QoreIRValue>& aliases = {});

    //! Returns true if an instruction keeps a planned unfused form
    DLLLOCAL bool planned(const QoreIRInstruction* inst) const {
        for (const Plan& plan : plans) {
            if (plan.call == inst) {
                return true;
            }
        }
        return false;
    }

    //! Returns the number of plans, for rollback()
    DLLLOCAL size_t planCount() const {
        return plans.size();
    }

    //! Drops the plans made after planCount() returned \a count, for a candidate that the pass does not fuse
    DLLLOCAL void rollback(size_t count) {
        if (plans.size() > count) {
            plans.erase(plans.begin() + static_cast<std::ptrdiff_t>(count), plans.end());
        }
    }

    //! Takes the planned consumers out of a block before the pass erases its eliminated instructions
    DLLLOCAL void take(std::vector<std::unique_ptr<QoreIRInstruction>>& instructions);

    //! Attaches the planned unfused forms whose consumers were all taken
    DLLLOCAL void finish();

private:
    struct Plan {
        QoreIRInstruction* call = nullptr;
        QoreIRValue call_result{};
        uint32_t call_nargs = 0;
        std::vector<QoreIRInstruction*> consumers;
        std::vector<const QoreIRInstruction*> constants;
        std::unordered_map<uint32_t, QoreIRValue> aliases;
        bool has_result_facts = false;
        QoreIRValueFacts result_facts;
        bool has_call_facts = false;
        QoreIRValueFacts call_facts;
        QoreIRDebuggerAlternative::Kind kind = QoreIRDebuggerAlternative::Kind::Consumers;
        std::vector<QoreIRInstruction*> prefix;
        QoreIRValue native_value{};
        //! other calls that share the alternative
        std::vector<QoreIRInstruction*> shared_calls;
        uint32_t site = 0;
        bool site_store = false;
        bool site_keyed = false;
        bool site_gated = false;
        //! the unfused form made by planDerived()
        std::unique_ptr<QoreIRDebuggerAlternative> derived;
    };
    //! Builds the definitions of the function's values once
    DLLLOCAL bool buildDefinitions();

    QoreIRFunction& func;
    bool enabled = true;
    std::vector<Plan> plans;
    std::unordered_map<const QoreIRInstruction*, std::unique_ptr<QoreIRInstruction>> taken;
    std::unordered_map<uint32_t, const QoreIRInstruction*> definitions;
    bool definitions_built = false;
    size_t check_count = 0;
};

using QoreIRCollectionProducerQuery = std::function<bool(
    const AbstractQoreFunctionVariant*, const QoreIRCallDirectInstruction*)>;

//! Fuse an imported optional integer collection projection with Eq/Ne.
size_t qore_ir_fuse_collection_producer_consumers(QoreIRFunction& func,
    const QoreIRCollectionProducerQuery& is_supported);

using QoreIRBoxedReturnParamQuery = std::function<bool(
    const AbstractQoreFunctionVariant*, const QoreIRInstruction*,
    int8_t&)>;

/** Proves that all values are assigned at the point immediately before an instruction.

    Plain locals and closure-scoped locals (top-level script variables, captured locals, and any
    local whose address has been taken with \\var, which the parser converts to a thread-safe
    closure-scoped local) are both proven from a flow-sensitive must-analysis over the CFG rather
    than from the parse-time value facts. This matters because a callee reached through a
    reference argument can leave the target untouched or write NOTHING back to it, so the
    assigned state must be killed at every call that passes a reference.

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

//! Replace a native integer conversion with its decimal width when its sole
//! consumer is string size() or length().
size_t qore_ir_fuse_native_int_string_measure_consumers(QoreIRFunction& func);

using QoreIRObjectIntStringMeasureQuery = std::function<bool(
    const QoreIRDotEvalMethodDirectInstruction*, int8_t&)>;

//! Fuse an exact explicit-receiver string(int-param) method with a sole
//! size() or length() consumer.
size_t qore_ir_fuse_object_int_string_measure_consumers(
    QoreIRFunction& func,
    const QoreIRObjectIntStringMeasureQuery& is_supported);

//! Fuse exact string case transformations with supported sole consumers.
size_t qore_ir_fuse_string_transform_consumers(QoreIRFunction& func);

//! Returns true if a returned value with the given facts must be converted or checked by the declared return type
/** Defined in lib/QoreIRToLLVM.cpp
*/
DLLLOCAL bool qore_ir_return_needs_coercion(const QoreTypeInfo* return_type, const QoreIRValueFacts* value_facts);

//! Returns true if a returned value with the given facts may need a conversion or type check by the declared return
//! type, apart from the rejection of a NOTHING value
/** Defined in lib/QoreIRToLLVM.cpp
*/
DLLLOCAL bool qore_ir_return_value_needs_conversion(const QoreTypeInfo* return_type,
    const QoreIRValueFacts* value_facts);

//! Returns true if the declared return type converts or rejects a missing return value
/** Defined in lib/QoreIRToLLVM.cpp
*/
DLLLOCAL bool qore_ir_nothing_return_needs_filter(const QoreTypeInfo* return_type);

#endif

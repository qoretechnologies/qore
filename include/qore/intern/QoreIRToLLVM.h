/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRToLLVM.h

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

#ifndef _QORE_QOREIRTOLLVM_H
#define _QORE_QOREIRTOLLVM_H

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "qore/intern/QoreIR.h"

class LocalVar;
class FunctionEntry;

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

class QoreIRFunction;
class QoreIRBasicBlock;
class QoreIRInstruction;
class QoreIRPhiInstruction;
class AbstractQoreFunctionVariant;
struct AOTSlotMap;
struct BatchCalleeInfo;

class QoreIRToLLVM {
public:
    explicit QoreIRToLLVM(llvm::LLVMContext& context) : ctx(context) {
    }

    //! Lower an entire QoreIRFunction to an LLVM function in the given module.
    //! The generated function has signature: uint64_t fname(ExceptionSink* xsink)
    //! or in AOT mode: uint64_t fname(QoreAOTContext* ctx, ExceptionSink* xsink)
    //! Returns the NaN-boxed QoreValue as uint64_t.
    bool lowerFunction(const QoreIRFunction& func, llvm::Module& module, std::string& error);

    //! Enable AOT mode with the given slot map.
    //! When set, process-specific opcodes emit _aot helper calls with slot indices
    //! instead of inttoptr patterns with embedded pointers.
    void setAOTMode(const AOTSlotMap* slots) {
        aot_mode = true;
        aot_slots = slots;
    }

    //! Set deopt counter pointer for profile-informed guard failure tracking.
    //! When set, profiled guard failure paths emit a call to qore_rt_deopt()
    //! that increments this counter. The evalTiered() path checks the counter
    //! to trigger JIT recompilation with updated type profiles.
    void setDeoptCounter(void* counter) {
        deopt_counter_ptr = counter;
    }

    //! Set batch callees for multi-function module compilation.
    //! When set, CallDirect instructions targeting variants in this map will emit
    //! direct LLVM calls to the in-module function instead of going through
    //! qore_rt_call_fast().  Maps variant pointer → BatchCalleeInfo.
    void setBatchCallees(const std::unordered_map<const AbstractQoreFunctionVariant*,
            BatchCalleeInfo>* callees) {
        batch_callees = callees;
    }

    //! Set fast entry mode for Approach B: parameters are passed as LLVM function
    //! arguments instead of being loaded from the thread-local variable stack.
    //! @param name the LLVM function name for the fast entry (e.g., "fname_fast")
    //! @param args maps LocalVar* (as void*) → LLVM Value* for each parameter
    void setFastEntryMode(const std::string& name,
            const std::unordered_map<const void*, llvm::Value*>* args) {
        fast_entry_name = name;
        fast_entry_args = args;
    }

    //! Set the name of an AOT self-recursive fast entry function.
    //! When set, self-recursive CallDirect instructions in AOT mode emit direct
    //! LLVM calls to this function instead of going through qore_rt_call_direct_aot.
    //!
    //! @p fe identifies the function whose fast-entry @p name refers to;
    //! self-recursion is detected by FE pointer equality rather than base
    //! name, avoiding cross-namespace mis-matches (`OMQ::foo` → `Util::foo`
    //! previously tripped the self-recursion path because base names match).
    void setAOTSelfRecursiveFastEntry(const std::string& name,
            const FunctionEntry* fe = nullptr) {
        aot_self_recursive_fast_entry = name;
        aot_self_recursive_fe = fe;
    }

    //! Set shared debug info for multi-function module compilation (AOT/batch).
    //! When set, the lowerer uses the shared DIBuilder/DICompileUnit instead of
    //! creating and finalizing its own per-function.  The caller is responsible
    //! for creating the DIBuilder+CU before lowering and finalizing after.
    void setSharedDebugInfo(llvm::DIBuilder* builder, llvm::DICompileUnit* cu) {
        shared_di_builder = builder;
        shared_di_cu = cu;
    }

    //! Control whether DWARF debug info is emitted (default: true).
    //! When false, skip DISubprogram creation and setCurrentDebugLocation
    //! is a no-op.  Caller at the module level is also responsible for
    //! skipping DIBuilder/DICompileUnit creation + finalization.
    void setEmitDebugInfo(bool v) {
        emit_debug_info = v;
    }

    //! Enable deferred exception checking for code proven not to observe exceptions
    //! before function exit.
    //! This is not semantics-preserving for ordinary code: later instructions can
    //! otherwise run after a dirty ExceptionSink and observe uninitialized values.
    void setDeferredExceptionChecking(bool v) {
        deferred_exception_checking = v;
    }

    //! Owned AOT location entry — stores location data by value, not by pointer.
    //! Data is captured during LLVM codegen when the IR function is alive.
    struct AOTLocEntry {
        int start_line = 0;
        int end_line = 0;
        std::string file;
    };

    //! Get the AOT location table built during LLVM codegen.
    //! Returns owned location entries (safe — no dangling pointer risk).
    const std::vector<AOTLocEntry>& getAOTLocTable() const {
        return aot_loc_table;
    }

private:
    llvm::LLVMContext& ctx;

    // Type cache
    llvm::Type* i64_type = nullptr;
    llvm::Type* i32_type = nullptr;
    llvm::Type* i1_type = nullptr;
    llvm::Type* double_type = nullptr;
    llvm::Type* ptr_type = nullptr;
    llvm::Type* void_type = nullptr;

    // The ExceptionSink* parameter for the current function
    llvm::Value* xsink_arg = nullptr;

    // AOT mode: emit _aot helper calls with slot indices instead of embedded pointers
    bool aot_mode = false;
    const AOTSlotMap* aot_slots = nullptr;
    llvm::Value* aot_ctx_arg = nullptr;   //!< QoreAOTContext* first parameter in AOT mode

    // Deferred exception checking for code proven not to observe exceptions before
    // function exit. Ordinary AOT code must keep this disabled to preserve Qore's
    // immediate exception propagation semantics.
    bool deferred_exception_checking = false;
    bool deferred_check_needed = false;        // set when a throwable was skipped

    // Deopt counter: pointer to variant's deopt_count atomic for guard failure tracking
    void* deopt_counter_ptr = nullptr;

    // Batch callees: variant → BatchCalleeInfo for multi-function module compilation.
    // When a CallDirect targets a variant in this map, emit a direct LLVM call instead
    // of going through qore_rt_call_fast().  For Approach B eligible callees, emit
    // direct LLVM calls to the fast entry function.
    const std::unordered_map<const AbstractQoreFunctionVariant*, BatchCalleeInfo>* batch_callees = nullptr;

    // Approach B fast entry: LLVM function name override and parameter mapping.
    // When fast_entry_name is non-empty, lowerFunction uses it instead of func.name
    // and initializes params from fast_entry_args instead of qore_rt_load_local().
    std::string fast_entry_name;
    const std::unordered_map<const void*, llvm::Value*>* fast_entry_args = nullptr;

    // AOT self-recursive fast entry: when set, self-recursive CallDirect in AOT mode
    // emits direct LLVM calls to this function instead of qore_rt_call_direct_aot.
    std::string aot_self_recursive_fast_entry;
    // FunctionEntry pointer of the function whose fast-entry the above
    // name refers to; `is_self_rec` matches on FE identity (pointer
    // equality) rather than base-name string equality, so a call to a
    // same-named function in another namespace (e.g. `OMQ::foo` calling
    // `Util::foo`) is not mis-identified as self-recursion.
    const FunctionEntry* aot_self_recursive_fe = nullptr;

    // IR builder
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // Block mapping: QoreIR blocks → LLVM blocks (initial blocks)
    std::unordered_map<const QoreIRBasicBlock*, llvm::BasicBlock*> block_map;

    // Final block mapping: QoreIR blocks → LLVM blocks after lowering
    // When lowering creates intermediate blocks (e.g., cmp_merge), the builder
    // ends up in a different block than the initial one. This map tracks where
    // each IR block's instructions end up, for correct PHI predecessor resolution.
    std::unordered_map<const QoreIRBasicBlock*, llvm::BasicBlock*> final_block_map;

    // Value mapping: QoreIR value IDs → LLVM values
    std::unordered_map<uint32_t, llvm::Value*> values;

    // Local variable allocas (LocalVar* address → alloca)
    std::unordered_map<const void*, llvm::Value*> local_allocas;

    // Track LocalVars that underwent COW in HashKeyStore/ListIndexStore
    // These need fresh reload from runtime stack on next LoadLocal to get updated value
    std::unordered_set<LocalVar*> cow_modified_locals;

    // Ordered list of unique LocalVar* pointers that need instantiation/uninstantiation
    std::vector<LocalVar*> function_locals;

    // Locals first accessed in the entry block (should be instantiated at function entry)
    std::vector<LocalVar*> entry_locals;

    // Set of entry-block locals (for quick lookup)
    std::unordered_set<const void*> entry_locals_set;

    // Track which non-entry-block locals have had their instantiation code emitted
    std::unordered_set<const void*> instantiated_non_entry_locals;

    // Set of locals that have explicit UninstantiateLocal instructions in the IR
    // (block-scoped locals that need mid-function destruction)
    std::unordered_set<const void*> block_scoped_locals;

    // Set of all body locals owned by the current IR function.  This is
    // intentionally separate from pre_instantiated_locals: IR-only body locals
    // can be callee-owned without being present on the runtime local stack.
    std::unordered_set<const void*> current_body_locals;

    // For pre-instantiated closure-use block-scoped locals (loop-body closure captures):
    // i1 alloca flags tracking whether the CVV is currently on the cvstack.
    // Initialized to true (evalTiered pre-instantiates them).  Cleared to false by
    // UninstantiateLocal (pop-only).  Set to true by StoreLocal (conditional re-push).
    // Used by emitPreInstClosureReInstantiation() at function exit to re-push any
    // remaining popped CVVs so evalTiered's cleanup can pop exactly one per variable.
    std::unordered_map<const void*, llvm::AllocaInst*> closure_pre_inst_flags;

    // Track which value IDs already contain NaN-boxed i64 (from Invoke, Call, CatchException,
    // make_string, .any ops, LoadLocal).  Values NOT in this set are raw typed values.
    std::unordered_set<uint32_t> nanboxed_values;

    // Set of LocalVar* (as void*) that are pre-instantiated by the caller (tiered
    // compilation); skip qore_rt_instantiate_local / qore_rt_uninstantiate_local for these.
    const std::unordered_set<const void*>* pre_instantiated_locals = nullptr;

    // Set of LocalVar* (as void*) that are only accessed by LoadLocal/StoreLocal
    // in fully-lowered IR code.  For these locals, skip qore_rt_assign_local
    // and related runtime-stack synchronization when ownership permits.
    const std::unordered_set<const void*>* ir_only_locals_set = nullptr;

    // Original IR-only set used only for reload decisions. AOT may remove body
    // locals from ir_only_locals_set so StoreLocal still syncs with the runtime
    // stack for ownership, but those locals are still invisible to AST callbacks.
    const std::unordered_set<const void*>* reload_exempt_locals_set = nullptr;

    // Phase 4: True when ALL locals in the function are invisible to AST
    // callbacks, meaning reloadAllLocalsFromRuntime() can be skipped entirely
    // after calls.
    bool all_locals_reload_exempt = false;

    // Sets of IR-only locals that use native (unboxed) allocas for typed int/float.
    // LoadLocal from these returns native i64/double (NOT nanboxed).
    // StoreLocal to these stores native i64/double (skips boxing).
    std::unordered_set<const void*> native_int_locals;
    std::unordered_set<const void*> native_float_locals;

    // Set of body locals that are IR-only — these can have their allocas initialized
    // to NOTHING directly instead of loading from the runtime stack (since the fast
    // call path skips their instantiation when all body locals are IR-only).
    std::unordered_set<const void*> ir_only_body_locals;

    // AOT body locals are pre-instantiated by the runtime frame wrapper with
    // an initial NOTHING value. LLVM allocas can therefore start as NOTHING
    // without an entry qore_rt_load_local_aot(); StoreLocal/lvalue mutation
    // paths force a reload after publishing a real value to the runtime stack.
    std::unordered_set<const void*> aot_body_locals;

    // AOT-adjusted IR-only set: removes pre-instantiated body locals from the
    // IR-only set so that StoreLocal syncs them to the runtime stack (fixing
    // double-free when body locals are pre-instantiated by evalTiered).
    std::unordered_set<const void*> aot_adjusted_ir_only;

    // Lazy local-cache invalidation. Calls that can execute AST/Qore code bump
    // the function epoch; LoadLocal reloads only the specific stale local it
    // reads. This keeps LLVM IR size O(calls + local reads), not O(calls * locals).
    llvm::AllocaInst* local_reload_epoch = nullptr;
    std::unordered_map<const void*, llvm::AllocaInst*> local_valid_epochs;

    // Saved on_block_exit handler count at function entry (for LIFO cleanup)
    llvm::Value* obe_saved_count = nullptr;

    // True when the current function contains deferred on_block_exit handlers.
    bool has_on_block_exit_handlers = false;

    // Per-scope saved on_block_exit counts (scope_id -> saved count value)
    // Used by ScopeEnter/ScopeExit for nested on_exit handler execution
    std::unordered_map<uint32_t, llvm::Value*> scope_obe_counts;

    // Entry-block load values for native pre-instantiated locals (tiered
    // compilation). qore_rt_load_local creates +1 ref at entry; these must be
    // decref'd at function exit. Boxed locals use cleanup allocas below so
    // lvalue mutation can clear the cached ref before the exit path.
    std::vector<llvm::Value*> preinstantiated_entry_loads;

    // Cleanup slots for boxed pre-instantiated entry loads, keyed by LocalVar*.
    // The alloca cache borrows this same +1 ref. Lvalue operations that mutate
    // the local clear the slot before the runtime mutation so cached refs do not
    // delay destruction or force copy-on-write.
    std::vector<llvm::AllocaInst*> preinstantiated_entry_cleanup_allocas;
    std::unordered_map<const void*, llvm::AllocaInst*> preinstantiated_entry_cleanup_by_local;

    // Allocas for fast entry param locals (Approach B).
    // At exit, the current alloca value is loaded and decref'd.  StoreLocal
    // keeps IR-only boxed locals owned by their local alloca so statement-temp
    // cleanup can run without dangling cached locals.
    std::vector<llvm::AllocaInst*> fast_entry_param_allocas;
    std::unordered_map<const void*, llvm::AllocaInst*> fast_entry_param_allocas_by_local;

    // Boxed IR-only locals that are neither fast-entry params nor backed by a
    // pre-instantiated cleanup slot. Their alloca owns the current value and
    // must be decref'd at function exit.
    std::vector<llvm::AllocaInst*> owned_ir_local_allocas;
    std::unordered_set<const void*> owned_ir_local_alloca_keys;

    // Allocas for Invoke/ConstString results that need cleanup at function exit.
    // qore_rt_invoke_expr returns +1 ref; these allocas track the results so they
    // can be decref'd at exit (matching the IR interpreter's cleanup vector).
    std::vector<llvm::Value*> invoke_result_allocas;

    // Entry-block array of pointers to invoke_result_allocas slots.  Large
    // functions use qore_rt_cleanup_run_allocas() to avoid emitting thousands
    // of load/decref/store triples at every cleanup exit.
    llvm::AllocaInst* invoke_cleanup_array = nullptr;
    unsigned invoke_cleanup_array_capacity = 0;
    unsigned invoke_cleanup_array_count = 0;
    bool invoke_cleanup_array_overflow = false;
    // Map from value ID to invoke-result alloca (for clearing at Return)
    std::unordered_map<uint32_t, llvm::Value*> invoke_alloca_map;
    // Cleanup allocas whose lifetime is tied to a local cache rather than an
    // expression temporary.  They are cleaned on function/error exits, but
    // discard.temps must not clear them while the local alloca still borrows
    // their owned reference.
    std::unordered_set<llvm::Value*> persistent_cleanup_allocas;
    // Map from local key (LocalVar* as void*) to invoke-result cleanup allocas
    // that hold references to values stored in this local.  Used by
    // UninstantiateLocal to clear cleanup allocas when block-scoped locals
    // are destroyed, preventing deferred-to-exit cleanup from keeping objects alive.
    std::unordered_map<const void*, std::vector<llvm::Value*>> local_cleanup_allocas;

    // Phase 2B — SSA-direct cleanup tracking.  An alternative to the flat
    // cleanup-alloca forest: when a refcounted temp would normally be
    // tracked via invoke_result_allocas, it can instead be tracked as an
    // SSA value along with its defining basic block.  Per-invoke landing
    // pads then filter these entries by dominance so only values that
    // dominate the invoke site are decref'd on the unwind path, which
    // preserves LLVM SSA dominance.  Populated by trackResultForCleanup
    // when canUseSsaCleanup's predicate holds.
    struct SsaCleanupEntry {
        llvm::Value* value = nullptr;
        llvm::BasicBlock* def_bb = nullptr;
        uint32_t result_id = 0;
    };
    std::vector<SsaCleanupEntry> pending_ssa_cleanup;

    struct TempCleanupMark {
        size_t invoke_alloca_count = 0;
        size_t pending_ssa_count = 0;
    };
    std::vector<TempCleanupMark> temp_cleanup_marks;

    // Incremental block-level immediate-dominator map, populated as
    // blocks are entered during lowering.  Conservative approximation:
    // single-predecessor -> that predecessor; zero or multiple
    // predecessors -> nullptr (non-dominating).  This is sufficient
    // for the strict single-pred-chain predicate; broader dominance
    // (catch-block pushes dominating post-try code, etc.) needs full
    // Cooper-Harvey-Kennedy — deferred to Step 3.
    std::unordered_map<llvm::BasicBlock*, llvm::BasicBlock*> immediate_dominator;

    // Entry block of the currently-lowered LLVM function; its idom is
    // conceptually nullptr.  Cached at function start for the dominance
    // walk to terminate cleanly.
    llvm::BasicBlock* entry_block_for_idom = nullptr;

    // Remaining use count for each value register ID.  Used to track when a
    // register's last use is reached, enabling early release of DotEval base
    // cleanup allocas.
    std::unordered_map<uint32_t, int> operand_remaining_uses;

    // Set of register IDs that are ONLY used as DotEval bases (operands[0]
    // of DotEvalMethodDirect, InvokeDotEvalMethodDirect, or Invoke with a
    // DotEval invoke_opcode).  For these registers, LoadSelfMember uses the
    // _for_call variant that returns the raw value without evaluating
    // WeakReferenceNode — safe because all DotEval helpers handle NT_WEAKREF.
    // This avoids creating temporary strong references from weak member
    // dereferences that would keep objects alive for the function lifetime.
    std::unordered_set<uint32_t> dot_eval_only_bases;

    // Locals assigned through the weak assignment operator. Loads must go
    // through LocalVar::eval() so weak refs are resolved to the current target
    // (or NOTHING if the target has already been deleted), rather than reading
    // the cached WeakReferenceNode from an LLVM alloca.
    std::unordered_set<const void*> weak_assigned_locals;

    // Result IDs produced by LoadLocal/LoadClosure on weak-assigned locals.
    // These loads return temporary strong refs that must be released at last
    // use; otherwise a loop condition like `while (weak)` keeps the referent
    // alive until function exit in LLVM mode.
    std::unordered_set<uint32_t> weak_load_result_ids;

    // Allocas tracking active iterator pointers from IteratorCreate/IteratorCreateReverse.
    // On normal exit (IteratorNext done), the alloca is nulled out.
    // On abnormal exit (return/throw inside foreach body), emitIteratorCleanup()
    // calls qore_rt_iterator_cleanup() for each non-null alloca.
    std::vector<llvm::Value*> iterator_cleanup_allocas;

    // Reload tracker allocas for local variables modified by lvalue operations.
    // Each tracker alloca holds the most recent qore_rt_load_local reload value
    // (+1 ref) so it can be decref'd before being replaced or at function exit.
    std::unordered_map<const void*, llvm::Value*> local_reload_trackers;

    // Deferred decref allocas for reload trackers.  When a reload replaces the
    // tracker value, the old value is moved here instead of being decrefd
    // immediately.  This prevents a use-after-free: LoadLocal reads from the
    // alloca (same value as the tracker), and if the reload decrefs the tracker
    // immediately, any live SSA value from LoadLocal becomes a dangling pointer.
    // Deferring by one cycle ensures the old value survives until the next
    // reload, by which time the SSA value has been consumed.
    std::unordered_map<const void*, llvm::Value*> local_reload_deferred;

    // Error return block: used for exception cleanup when outside a try block.
    // When a Call/CallDirect/CallMethodDirect throws outside a try, execution
    // jumps here to return NOTHING immediately.  Created lazily on first use.
    llvm::BasicBlock* error_return_block = nullptr;

    // JIT deopt block: on guard failure, sets thread-local deopt flag and branches
    // to error_return_block.  evalTiered() checks the flag and re-executes via AST.
    // Created lazily on first guard with deopt_target.
    llvm::BasicBlock* jit_deopt_block = nullptr;

    // Shared landingpad blocks: maps exception handler block → landingpad block.
    // Multiple invoke instructions targeting the same handler share a single
    // landingpad, avoiding BB explosion.
    std::unordered_map<llvm::BasicBlock*, llvm::BasicBlock*> landingpad_blocks;

    // C++ EH prototype (QORE_AOT_EH=1): when set, AOT CallDirect call sites
    // are emitted as CreateInvoke on a throwing wrapper (qore_rt_call_direct_aot_throwing)
    // with the unwind edge pointing at a shared function-level landing pad.
    // The landing pad cleans up tracked temps and returns NOTHING with xsink
    // already populated by the thrown QoreJITException's underlying raise site.
    // Gated by env var so existing check-based path remains default.
    bool aot_eh_enabled = false;

    // Set by an EH invoke emission site to tell the NEXT emitExceptionCheck
    // call to skip (control is on the invoke's normal edge where xsink is
    // known clean). Cleared by emitExceptionCheck after being observed.
    // One-shot only — subsequent calls in the same cont block still need
    // their own checks.
    bool skip_next_exception_check = false;

    // Lazily-created shared unwind landing pad for the C++ EH prototype.
    // Reset at function entry, populated on first invoke that needs it, and
    // terminated with a ret NOTHING after invoke_result_allocas are finalized.
    llvm::BasicBlock* function_unwind_lp = nullptr;

    // Phase 2B — per-invoke landing pads: set by emitMaybeInvoke's EH path
    // whenever it just emitted a CreateInvoke + per-invoke cleanup LP.
    // trackResultForCleanup reads + consumes the flag to decide SSA-direct
    // vs legacy alloca tracking for the just-produced result.
    bool last_call_was_invoke_eh = false;

    // Phase 2B — shared common-cleanup block.  All per-invoke LPs branch
    // here after emitting their own filtered pending_ssa_cleanup decrefs.
    // Populated by finalizeFunctionCommonCleanup with a phi that merges
    // landingpad values from every feeding LP, followed by the shared
    // cleanup tail (on_block_exit, iterator, preinstantiated, invoke
    // (allocas), local uninstantiation) and a resume that rethrows.
    llvm::BasicBlock* function_common_cleanup = nullptr;

    // Phase 2B — incoming edges feeding function_common_cleanup's phi.
    // Populated by createPerInvokeCleanupLP; consumed by
    // finalizeFunctionCommonCleanup.  Parallel arrays for clarity.
    std::vector<llvm::BasicBlock*> common_cleanup_phi_preds;
    std::vector<llvm::Value*> common_cleanup_phi_values;

    // Returns the function-level unwind landing pad, creating it if needed.
    // The block starts with a `landingpad { ptr, i32 } cleanup` instruction and
    // is terminated later (in finalizeFunctionUnwindLP) after all live temps
    // have been tracked. Only used when aot_eh_enabled is true.
    llvm::BasicBlock* getOrCreateFunctionUnwindLP(llvm::Module& module,
            llvm::Function* llvm_func);

    // Populate and terminate the function-level unwind landing pad after all
    // instructions have been lowered (so all invoke_result_allocas are known).
    void finalizeFunctionUnwindLP(llvm::Module& module);

    // C++ EH prototype helper: emit either a CreateCall (default) or a
    // CreateInvoke (when aot_eh_enabled and the instruction is not inside a
    // try block) targeting the given normal/throwing helper pair. Sets
    // skip_next_exception_check on the EH path so the surrounding
    // emitExceptionCheck call becomes a no-op for this site. Both helpers
    // must have identical signatures and return a NaN-boxed i64.
    llvm::Value* emitMaybeInvoke(llvm::FunctionCallee normal_helper,
            llvm::FunctionCallee throwing_helper,
            llvm::ArrayRef<llvm::Value*> args,
            llvm::Module& module, llvm::Function* llvm_func,
            const QoreIRInstruction* inst);

    // Deferred PHI nodes: (LLVM PHI, IR PHI instruction) pairs to fixup after all blocks lowered
    std::vector<std::pair<llvm::PHINode*, const QoreIRPhiInstruction*>> pending_phis;

    // Pointer to current IR function being lowered (for reading type profiles)
    const QoreIRFunction* current_ir_func = nullptr;

    // Pointer to current LLVM module (set during lowerFunction)
    llvm::Module* current_module = nullptr;

    // Phase 5c: Debug info (DWARF)
    // Controls whether DWARF is emitted (-g / --strip-debug-info).
    // Default true to match existing behavior; qcc flag flips it.
    bool emit_debug_info = true;
    // Owned DIBuilder for single-function-per-module case
    std::unique_ptr<llvm::DIBuilder> di_builder;
    // Shared DIBuilder/CU for multi-function-per-module case (AOT/batch)
    llvm::DIBuilder* shared_di_builder = nullptr;
    llvm::DICompileUnit* shared_di_cu = nullptr;
    // Active DIBuilder pointer (points to either owned or shared)
    llvm::DIBuilder* active_di_builder = nullptr;
    llvm::DICompileUnit* di_cu = nullptr;
    llvm::DISubprogram* di_sp = nullptr;
    std::unordered_map<const char*, llvm::DIFile*> di_file_cache;

    // Runtime location tracking: per-function cached TLS pointers
    llvm::Value* loc_cache_ptr = nullptr;   //!< Cached ptr-to-ptr for runtime_loc TLS variable
    llvm::Value* stmt_cache_ptr = nullptr;  //!< Cached ptr-to-ptr for runtime_statement TLS variable
    int last_runtime_line = -1;             //!< Last source line emitted for location tracking

    //! AOT location dedup: maps QoreProgramLocation* → slot index (AOT mode only).
    //! The pointer is used only as a dedup key during the single LLVM codegen pass.
    std::unordered_map<const QoreProgramLocation*, int32_t> aot_loc_slots;
    //! AOT location table: owns location data captured during LLVM codegen.
    std::vector<AOTLocEntry> aot_loc_table;

    //! Emit a runtime_loc update if the instruction's source line changed
    void emitRuntimeLocationUpdate(const QoreIRInstruction* inst, llvm::Module& module);

    // Initialize types and helpers
    void initTypes();

    // Declare external runtime helper functions in the module
    void declareRuntimeHelpers(llvm::Module& module);

    // Lower a single instruction; returns false on unsupported opcode
    bool lowerInstruction(const QoreIRInstruction* inst, llvm::Function* llvm_func,
            llvm::Module& module, std::string& error);

    //! Reject AOT codegen that would execute serialized expression trees.
    bool checkNoAotExecutableExprFallback(llvm::Function* llvm_func, std::string& error) const;

    //! Record the exact IR instruction that emitted a forbidden AOT expression fallback.
    void annotateAotExecutableExprFallback(llvm::Value* call, const QoreIRInstruction* inst) const;

    // Helpers to get/create LLVM values
    llvm::Value* getVal(uint32_t id, std::string& error);

    // NaN-boxing helpers: encode typed LLVM values into i64 QoreValue representation
    llvm::Value* boxInt(llvm::Value* int_val);
    llvm::Value* boxFloat(llvm::Value* float_val);
    llvm::Value* boxBool(llvm::Value* bool_val);
    llvm::Value* boxNothing();

    // Unboxing helpers: extract typed values from i64 QoreValue
    llvm::Value* unboxInt(llvm::Value* qv);
    llvm::Value* unboxFloat(llvm::Value* qv);
    llvm::Value* unboxBool(llvm::Value* qv);

    // Phase 4: LLVM IR helper inlining helpers
    // Check if value has a node (inline NaN-boxing check) - returns bool (i1)
    llvm::Value* hasNodeInline(llvm::Value* qv);

    // Inline qore_rt_ref - reference count a value if it's a node
    // Returns the value unchanged if not a node, or qore_rt_refself result if node
    llvm::Value* emitHelperRef(llvm::Module& module, llvm::Value* val);

    // Ensure a value is a native int64_t for typed int operations.
    // Handles: native i64 (pass through), NaN-boxed INT48 or big int (runtime conversion).
    llvm::Value* ensureIntType(llvm::Value* val, uint32_t value_id);

    // Inline fast-path version of ensureIntType for typed int ops (NOT in PHI fixup context).
    // Uses LLVM branches to check INT48 tag and sign-extend inline, falling back to runtime
    // only for QoreBigIntNode values.  ~2x faster than ensureIntType for common INT48 values.
    llvm::Value* ensureIntTypeInline(llvm::Value* val, uint32_t value_id);

    // Inline fast-path version of boxInt for StoreLocal (NOT in PHI fixup/boxValue context).
    // Uses LLVM branches for INT48 range check, falling back to runtime for big ints.
    llvm::Value* boxIntInline(llvm::Value* int_val);

    // Ensure a value is a native double for float operations
    // Handles NaN-boxed values (int or float), native i64, and native doubles
    llvm::Value* ensureFloatType(llvm::Value* val, uint32_t value_id, llvm::Module& module);

    // Get a declared runtime helper function
    llvm::FunctionCallee getHelper(llvm::Module& module, const char* name, llvm::FunctionType* ft);

    // Return either a JIT-safe QoreTypeInfo* constant or an AOT-safe type-path string.
    llvm::Value* getTypeInfoPointerArg(const QoreTypeInfo* ti);
    llvm::Value* getTypePathArg(const QoreTypeInfo* ti);
    const QoreTypeInfo* specializeType(const QoreTypeInfo* ti) const;

    // Collect all unique LocalVar* pointers from the IR function
    void collectLocals(const QoreIRFunction& func);

    // Emit qore_rt_instantiate_local calls for all function locals at current insert point
    void emitLocalInstantiation(llvm::Module& module);

    // Pre-create LLVM allocas for all function locals so reloadAllLocalsFromRuntime can find them
    void preCreateLocalAllocas(llvm::Module& module, llvm::Function* llvm_func);

    // Emit qore_rt_uninstantiate_local calls for all function locals at current insert point
    void emitLocalUninstantiation(llvm::Module& module);

    // Emit qore_rt_exec_on_block_exit call to execute registered on_block_exit handlers
    void emitOnBlockExitExec(llvm::Module& module);

    // Publish current LLVM local allocas to the runtime local stack before
    // deferred handlers execute through AST/IR and read parent locals.
    void syncLocalsToRuntimeForHandlers(llvm::Module& module);

    // Install an exact parent slot cache for deferred handler IR executed by a
    // native/JIT/AOT parent.  This avoids name-based TLS lookup collisions.
    llvm::Value* beginNativeHandlerSlotCache(llvm::Module& module);
    void endNativeHandlerSlotCache(llvm::Module& module, llvm::Value* guard);

    // Emit qore_rt_decref calls for pre-instantiated local entry loads
    void emitPreinstantiatedCleanup(llvm::Module& module);

    // Conditionally re-instantiate pre-instantiated closure-use block-scoped locals
    // that were popped mid-execution (e.g. loop-body closure captures).  Called at
    // every function exit point so evalTiered's cleanup can pop exactly one CVV per var.
    void emitPreInstClosureReInstantiation(llvm::Module& module);

    // Emit qore_rt_decref calls for tracked runtime call results
    void emitInvokeCleanup(llvm::Module& module);

    // Emit statement/condition-boundary temp cleanup for values tracked since
    // the latest PushTempMark.
    void emitDiscardTemps(llvm::Module& module);

    // Register an alloca-backed cleanup slot and, for large functions, add its
    // address to the entry-block cleanup pointer array used by
    // qore_rt_cleanup_run_allocas().
    void registerInvokeCleanupAlloca(llvm::Value* alloca_ptr);
    void registerPersistentCleanupAlloca(llvm::Value* alloca_ptr);

    // Conservative upper bound for the cleanup pointer array.  Overflow falls
    // back to the expanded cleanup path, so the estimate only affects whether
    // the compact helper path is available.
    unsigned estimateInvokeCleanupArrayCapacity(const QoreIRFunction& func) const;

    // Emit qore_rt_iterator_cleanup calls for active iterators
    void emitIteratorCleanup(llvm::Module& module);

    // Emit llvm.lifetime.start after each entry-block alloca and
    // llvm.lifetime.end before each ret/resume terminator.  Helps SROA/mem2reg
    // understand alloca lifetimes and promote more storage to SSA, which
    // collapses the flag-alloca forest that makes -O3 compile times pathological
    // on large functions.  Called once per function, after all lowering and
    // finalization is complete but before IR verification.
    void emitLifetimeAnnotations(llvm::Function* llvm_func);

    // Phase 2B — incremental idom update.  Called at each new LLVM basic
    // block entry during lowering.  Records the block's immediate
    // dominator based on its currently-wired predecessors: exactly one
    // predecessor -> that predecessor; otherwise nullptr.  Broader
    // dominance (full Cooper-Harvey-Kennedy) is deferred to Step 3.
    void updateImmediateDominator(llvm::BasicBlock* bb);

    // Phase 2B — get (or lazily compute) the immediate dominator of a
    // block.  Covers mid-block helper BBs (invoke cont, guard
    // continuation, check-cont) that aren't iterated by the top-level
    // block-lowering loop.  Caches the result in immediate_dominator.
    llvm::BasicBlock* getOrComputeImmediateDominator(llvm::BasicBlock* bb);

    // Phase 2B — block-level dominance query.  Returns true if
    // `candidate` dominates `target` per the incremental
    // immediate_dominator map.  Walks up the idom chain from target
    // until candidate is found or the chain terminates.
    bool dominates(llvm::BasicBlock* candidate, llvm::BasicBlock* target);

    // Phase 2B — conservative predicate: is `bb` reachable from the
    // function entry along a single-predecessor-only chain?  Used by the
    // strict-mode Step 2 predicate so we only push SSA-direct entries
    // where cleanup paths are trivially dominance-safe.
    bool isOnStraightLineChain(llvm::BasicBlock* bb);

    // Phase 2B — decide whether to track a refcounted temp as an SSA
    // entry or via the legacy cleanup alloca.  Returns true only when
    // aot_eh_enabled && the just-emitted call was an EH invoke &&
    // current_bb is on the entry single-pred chain.  Consumes
    // `last_call_was_invoke_eh` (clears it) when true.  Deferred-mode
    // functions are eligible: the deferred tail-poll sites promote
    // pending SSA to allocas before branching to error_return_block.
    bool canUseSsaCleanup(llvm::BasicBlock* current_bb);

    // Phase 2B — emit a per-invoke cleanup landing pad right after an
    // EH-path CreateInvoke.  Fast path: when no pending_ssa_cleanup
    // entry dominates `invoke_bb`, reuses the shared function-level
    // unwind landing pad (avoids an explosion of empty per-invoke LP
    // BBs — Logger dropped from 452 to 52 inv_lp BBs after this).
    // Slow path: creates a fresh BB with a landingpad { ptr, i32 }
    // cleanup, derefs the dominating snapshot of pending_ssa_cleanup
    // in LIFO order, and branches to function_common_cleanup.  Records
    // the per-LP value for the shared phi used by
    // finalizeFunctionCommonCleanup.  Returns the LP BB to use.
    llvm::BasicBlock* createPerInvokeCleanupLP(llvm::Module& module,
            llvm::Function* llvm_func, llvm::BasicBlock* invoke_bb);

    // Phase 2B — terminate the shared common-cleanup block after all
    // lowering is complete.  Emits a phi over all feeding LP edges,
    // then the shared tail (on_block_exit, iterators, preinstantiated
    // locals, invoke allocas, local uninstantiation) and resume().
    void finalizeFunctionCommonCleanup(llvm::Module& module);

    // Phase 2B — emit decrefs for every pending_ssa_cleanup entry whose
    // def_bb dominates the current insert block.  Used by Return /
    // ReturnNothing / Throw / Rethrow / ThreadExit / error_return_block
    // finalization / jit_deopt_block finalization to release SSA-tracked
    // temps along normal exit paths.
    void emitPendingSsaCleanup(llvm::Module& module);

    // Emit idempotent temp cleanup before every function exit after all
    // blocks have been lowered.  This covers returns that were emitted before
    // later blocks created additional cleanup allocas.
    void emitLateExitCleanup(llvm::Function* llvm_func, llvm::Module& module);

    // Phase 2B — spill all currently-pending SSA-direct cleanup entries
    // to per-temp cleanup allocas and clear pending_ssa_cleanup.  Used
    // at the point of a control-flow branch to a shared cleanup target
    // (error_return_block, jit_deopt_block) whose idom is not known to
    // dominate the SSA value's def_bb.  After promotion, the shared
    // block's emitInvokeCleanup covers the cleanup instead.
    void promotePendingSsaToAllocas(llvm::Module& module,
            llvm::Function* llvm_func);

    // Phase 2B — spill pending_ssa_cleanup to cleanup allocas then emit
    // `CondBr(cond, exception_target, normal_target)`.  Promotion runs
    // in the current block BEFORE the branch so both the exception and
    // normal edges see an empty pending_ssa_cleanup downstream; the
    // promoted allocas are decref'd by the shared target's emitInvoke-
    // Cleanup.  A per-site SSA-decref preamble BB on the exception edge
    // was tried and reverted — with conservative single-pred idom it
    // produced SSA dominance violations for catch-block pushes whose
    // decrefs landed in post-try merge BBs.  Used by emitException-
    // Check and the deferred-tail checks (pre-try-flush / Return /
    // ReturnNothing deferred paths).
    void emitCondBrWithSsaPreamble(llvm::Module& module,
            llvm::Function* llvm_func, llvm::Value* cond,
            llvm::BasicBlock* exception_target, llvm::BasicBlock* normal_target);

    // Phase 2B — promote a single SSA-direct cleanup entry (identified
    // by its IR result id) to a cleanup alloca.  Used when downstream
    // bookkeeping — local_cleanup_allocas for block-scoped locals, etc.
    // — needs a concrete alloca it can reference.  Returns the new (or
    // existing) alloca, or nullptr if the id isn't tracked.  A no-op
    // when the id already maps to a real alloca.
    llvm::AllocaInst* promoteSsaEntryToAlloca(uint32_t result_id,
            llvm::Module& module, llvm::Function* llvm_func);

    // Track a runtime helper result for cleanup at function exit.
    // Creates an alloca initialized to NOTHING in the entry block, stores the result,
    // and registers it for decref at exit.  For Return, the alloca can be cleared
    // via invoke_alloca_map so the returned value isn't decremented.
    void trackResultForCleanup(llvm::Value* result, uint32_t result_id,
            llvm::Function* llvm_func);

    // Box any typed LLVM value to NaN-boxed i64, handling already-boxed values
    llvm::Value* boxValue(llvm::Value* val, uint32_t id);

    // Emit exception check: if xsink has exception, branch to exception_target
    void emitExceptionCheck(llvm::Module& module, llvm::Function* llvm_func,
            const QoreIRInstruction* inst);

    // Get or create the per-function JIT deopt block.
    // On guard failure, branches here to set the thread-local deopt flag
    // and return to evalTiered() which re-executes via AST.
    llvm::BasicBlock* getOrCreateJitDeoptBlock(llvm::Module& module,
            llvm::Function* llvm_func);

    // Phase 5b: Try to emit a specialized hash key access instead of qore_rt_invoke_expr.
    // Returns true if specialized code was emitted, false to fall through to generic path.
    bool tryEmitHashKeyAccess(const QoreIRInstruction* inst, llvm::Module& module,
            llvm::Function* llvm_func);

    // Try the decomposed `background` lowering for one of five inner-call shapes
    // (self.method / foo(args) / Class::sm(args) / obj.method(args) / callref(args)).
    // On success writes the call result to *result and returns true; on non-match
    // (empty operands or unsupported inner shape) returns false leaving *result
    // untouched so the caller emits the AST fallback path.  In AOT mode, only the
    // self.method shape has a dedicated helper (qore_rt_background_self_call_aot);
    // other shapes fall through to AST eval since they'd need name-based lookup.
    // @param expr_val        The QoreBackgroundOperatorNode wrapped as QoreValue
    // @param operands        Pre-evaluated operands (layout described above)
    // @param throwing_ok     Allow invoke+throwing-helper pair for EH mode (pass
    //                        false when the caller emits its own exception check)
    bool tryEmitDecomposedBackground(const QoreValue& expr_val,
            const std::vector<QoreIRValue>& operands,
            llvm::Module& module, llvm::Function* llvm_func,
            const QoreIRInstruction* inst,
            bool throwing_ok,
            llvm::Value** result);

    bool tryEmitBackgroundMetadata(const QoreIRBackgroundInstruction* inst,
            llvm::Module& module, llvm::Function* llvm_func,
            bool throwing_ok,
            llvm::Value** result);

    // Phase 5b: Try to emit a specialized list index access instead of qore_rt_invoke_expr.
    // Returns true if specialized code was emitted, false to fall through to generic path.
    bool tryEmitListIndexAccess(const QoreIRInstruction* inst, llvm::Module& module,
            llvm::Function* llvm_func);

    // Phase 5c: Get or create DIFile from a file path
    llvm::DIFile* getDIFile(const char* file_path);

    // Phase 5c: Set IRBuilder debug location from instruction's source location
    void setDebugLocation(const QoreIRInstruction* inst);

    // NaN-boxed type predicates shared by inline fast paths.
    llvm::Value* emitIsBoxedInt48(llvm::Value* qv);
    llvm::Value* emitIsBoxedFloat(llvm::Value* qv);

    // Phase 5b: Emit inline LLVM fast-path for .any arithmetic (AddAny/SubAny/MulAny).
    // Type-checks operands for int+int and float+float, falls back to helper for mixed types.
    llvm::Value* emitAnyArithFastPath(llvm::Instruction::BinaryOps int_op,
            llvm::Instruction::BinaryOps float_op, const char* slow_helper,
            const QoreIRInstruction* inst,
            llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module);

    // Reload a specific local variable's alloca from the Qore runtime stack.
    // Called after lvalue operations (PostInc, StoreLvalue, etc.) that modify the
    // runtime stack without updating the LLVM alloca cache.
    void reloadLocalFromRuntime(const void* key, llvm::Module& module, llvm::Function* llvm_func,
            bool honor_reload_exempt = true);

    // Retain a just-assigned value directly in the local alloca cache without
    // loading it back from the runtime stack.
    void retainLocalCacheValue(const void* key, llvm::Value* value, llvm::Module& module,
            llvm::Function* llvm_func, bool honor_reload_exempt = true);

    // Clear the reload tracker for a specific local, releasing the +1 reference
    // held from a previous reloadLocalFromRuntime() call.  Called before lvalue
    // compound operations (+=, -=, etc.) so that the container's refcount drops to 1,
    // enabling copy-on-write to skip the copy (in-place modification).
    void clearLocalReloadTracker(const void* key, llvm::Module& module, llvm::Function* llvm_func);

    enum class LocalCacheClearMode {
        DuplicateRefsOnly,
        IncludeFastEntryOwner,
    };

    // Clear cached +1 local values. For fast-entry params the alloca is the
    // local's real owner, not a duplicate cache; lvalue mutation must keep it
    // counted so copy-on-write can detect caller sharing.
    void clearLocalCachedValue(const void* key, llvm::Module& module,
            llvm::Function* llvm_func, LocalCacheClearMode mode);

    // Resolve the root local key for structured lvalue-path instructions. In
    // AOT mode LVPath roots may carry only a slot id, so this must reverse-map
    // the slot through AOTSlotMap instead of relying on ref_ptr.
    const void* findLVPathRootLocalKey(const QoreIRLValuePathInstruction* path_inst) const;

    // Clear all reload trackers for all locals.  Called before AST-delegated
    // lvalue operations (PushAny, etc.) to prevent refcount inflation.
    void clearAllLocalReloadTrackers(llvm::Module& module, llvm::Function* llvm_func);

    // Pre-decref old result and clear reload tracker before lvalue compound
    // operations.  Ensures the cleanup alloca exists (creating in the entry
    // block if necessary), decrefs the previous value stored in it, and clears
    // the reload tracker for the lvalue target local.  Returns the alloca so
    // callers can store the new result into it after the operation.
    llvm::AllocaInst* emitPreDecrefAndClearTracker(uint32_t result_id,
            const QoreIRLValueInstruction* lvinst,
            llvm::Module& module, llvm::Function* llvm_func);

    // Drop the cleanup entry for a ref-counted temporary result immediately.
    // Handles both alloca-backed and SSA-direct cleanup tracking.
    void releaseCleanupForValueId(uint32_t value_id, llvm::Module& module);

    // Mark a lowered IR value use as consumed.  If it is the last use of a
    // weak-local load, drop the temporary strong reference immediately.
    void consumeValueUse(uint32_t value_id, llvm::Module& module,
            bool release_weak_load = true);

    // Dot-eval calls consume their base during the call.  EH invoke lowerers
    // create a terminator before lowerFunction's post-instruction cleanup can
    // emit IR, so release a last-use base at the call boundary.
    void releaseDotEvalBaseIfCurrentUseIsLast(const QoreIRInstruction* inst,
            llvm::Module& module);

    // Build an entry-block alloca'd array of NaN-boxed args from operands[arg_start..].
    // Sets args_array and nargs.  Returns false on error.
    // If nargs == 0, args_array is set to a null pointer.
    bool buildArgsArray(const QoreIRInstruction* inst, int arg_start,
            llvm::Function* llvm_func, llvm::Value*& args_array, int& nargs,
            std::string& error);

    // Build an entry-block alloca'd array of cleanup slot pointers for
    // operands[arg_start..].  The runtime uses this to clear consumed
    // call-argument temporaries after callee parameter instantiation.
    llvm::Value* buildArgCleanupArray(const QoreIRInstruction* inst, int arg_start,
            llvm::Function* llvm_func, int nargs, bool& has_cleanup);

    // Returns true if a local can be lazily reloaded from the runtime stack.
    bool canReloadLocalFromRuntime(const void* key, bool honor_reload_exempt = true) const;

    // Per-function epoch helpers for lazy local-cache invalidation.
    llvm::AllocaInst* getOrCreateLocalReloadEpoch(llvm::Function* llvm_func);
    llvm::AllocaInst* getOrCreateLocalValidEpoch(const void* key,
            llvm::Function* llvm_func);
    void markLocalCacheFresh(const void* key, llvm::Function* llvm_func);
    void ensureLocalCacheFresh(const void* key, llvm::Module& module,
            llvm::Function* llvm_func);

    // Invalidate all reloadable local variable allocas. Called after calls that
    // can modify locals through the Qore runtime stack; actual reloads are lazy.
    void reloadAllLocalsFromRuntime(llvm::Module& module, llvm::Function* llvm_func,
            bool honor_reload_exempt = true, bool eager = false);

    // Phase 5b: Emit inline LLVM fast-path for .any comparisons (EqAny/NeAny/etc).
    // Type-checks operands for int-vs-int and float-vs-float, falls back to helper for mixed types.
    llvm::Value* emitAnyCmpFastPath(llvm::CmpInst::Predicate int_pred,
            llvm::CmpInst::Predicate float_pred, int opcode,
            const QoreIRInstruction* inst,
            llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module);

    // Emit inline LLVM fast-path for .any compound assignments (AddAssignAny/SubAssignAny/etc).
    // Type-checks operands for int+int and float+float, falls back to helper for mixed types.
    // For AddAssignAny (handle_nothing=true), returns rhs if lhs is NOTHING.
    llvm::Value* emitAnyCompoundAssignFastPath(llvm::Instruction::BinaryOps int_op,
            llvm::Instruction::BinaryOps float_op, const char* slow_helper,
            const QoreIRInstruction* inst,
            llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module, bool handle_nothing);

    // Emit inline LLVM fast-path for lvalue compound assignments (AddAssignLValue, etc.).
    // Loads the current lvalue value, checks NOTHING (for AddAssignLValue only), then
    // checks INT48+INT48 and float+float for native arithmetic, falling back to
    // qore_rt_lvalue_binary for complex types.  The loaded value is decref'd after use.
    // int_op/float_op may be omitted (pass -1) for ops like div/mod that should skip the
    // fast path.  handle_nothing should be true only for AddAssignLValue.
    llvm::Value* emitLValueCompoundAssignFastPath(
            const QoreIRInstruction* inst,
            llvm::Value* val_boxed, llvm::Value* lv_bits_or_slot,
            int int_op, int float_op,
            bool handle_nothing,
            llvm::Function* llvm_func, llvm::Module& module);

    // Emit inline LLVM fast-path for .any bitwise compound assignments (AndAssignAny/etc).
    // Type-checks operands for int+int, falls back to qore_rt_binary_op for non-int types.
    llvm::Value* emitAnyBitwiseFastPath(llvm::Instruction::BinaryOps int_op,
            const char* slow_helper, int opcode,
            const QoreIRInstruction* inst,
            llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module);

    // Emit inline LLVM fast-path for .any unary operations (UnaryMinusAny/UnaryPlusAny).
    // Type-checks operand for int or float, falls back to qore_rt_unary_op for other types.
    // is_minus=true for UnaryMinusAny, false for UnaryPlusAny.
    llvm::Value* emitAnyUnaryFastPath(bool is_minus, int opcode,
            const QoreIRInstruction* inst,
            llvm::Value* operand, llvm::Function* llvm_func, llvm::Module& module);

    // Emit inline LLVM fast-path for CmpAny (spaceship operator).
    // Type-checks operands for int+int and float+float, falls back to qore_rt_comparison_op.
    // Returns boxed int (-1, 0, or 1).
    llvm::Value* emitAnyCmpSpaceshipFastPath(const QoreIRInstruction* inst,
            llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module);

    // Emit inline LLVM fast-path for EqHard/NeHard (=== and !==).
    // Fast-path: if bits are equal and not a float → return true/false
    // For floats with equal bits, check NaN (NaN !== NaN).
    // is_eq=true for EqHard (===), false for NeHard (!==).
    llvm::Value* emitHardEqualityFastPath(bool is_eq, llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module);

    //! Emit a forward-iteration fold loop over a list.
    //! @param inst the IR instruction (for result id + operand)
    //! @param module LLVM module
    //! @param llvm_func LLVM function
    //! @param label block name prefix (e.g., "foldl_sum")
    //! @param is_float true for float element access, false for int
    //! @param identity_val initial accumulator for identity-init opcodes (sum→0, prod→1);
    //!        nullptr for first-element-init opcodes (diff/min/max)
    //! @param empty_nothing true if empty list returns NOTHING (min/max); false returns identity/0
    //! @param accumulate lambda (acc, elem) → new_acc
    //! @param error error string for reporting
    //! @return true on success
    bool emitFoldLoop(const QoreIRInstruction* inst, llvm::Module& module,
            llvm::Function* llvm_func, const char* label, bool is_float,
            llvm::Value* identity_val, bool empty_nothing,
            std::function<llvm::Value*(llvm::Value*, llvm::Value*)> accumulate,
            std::string& error);

    //! Emit a reverse-iteration fold loop over a list (for FoldrDiff only).
    //! @param inst the IR instruction (for result id + operand)
    //! @param module LLVM module
    //! @param llvm_func LLVM function
    //! @param label block name prefix (e.g., "foldr_diff")
    //! @param is_float true for float element access, false for int
    //! @param accumulate lambda (acc, elem) → new_acc
    //! @param error error string for reporting
    //! @return true on success
    bool emitFoldReverseLoop(const QoreIRInstruction* inst, llvm::Module& module,
            llvm::Function* llvm_func, const char* label, bool is_float,
            std::function<llvm::Value*(llvm::Value*, llvm::Value*)> accumulate,
            std::string& error);

    //! Inline a small callee's IR directly into the caller's LLVM function
    //! Phase 3: Aggressive inlining for ≤20 instruction callees
    //! @param callee_ir the QoreIRFunction to inline
    //! @param call_inst the calling instruction (for operands and result ID)
    //! @param llvm_func the caller's LLVM function
    //! @param module LLVM module for helper functions
    //! @param error error string for reporting
    //! @return LLVM value representing the inlined call result, or nullptr on error
    llvm::Value* emitInlinedCallee(const QoreIRFunction& callee_ir,
            const QoreIRInstruction* call_inst, llvm::Function* llvm_func,
            llvm::Module& module, std::string& error);
};

#endif

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
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <unordered_set>
#include <vector>
#include "qore/intern/QoreIR.h"
#include "qore/intern/QoreJIT.h"

class LocalVar;
class FunctionEntry;

#include <llvm/ADT/MapVector.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

class QoreIRFunction;
struct QoreJITObjectLayout;
class QoreIRBasicBlock;
class QoreIRInstruction;
class QoreIRPhiInstruction;
class AbstractQoreFunctionVariant;
struct AOTSlotMap;
class QoreIRToLLVM {
public:
    explicit QoreIRToLLVM(llvm::LLVMContext& context) : ctx(context) {
    }

    //! Lower an entire QoreIRFunction to an LLVM function in the given module.
    //! The generated function has signature: uint64_t fname(ExceptionSink* xsink)
    //! or in AOT mode: uint64_t fname(QoreAOTContext* ctx, ExceptionSink* xsink)
    //! Returns the NaN-boxed QoreValue as uint64_t.
    bool lowerFunction(const QoreIRFunction& func, llvm::Module& module, std::string& error);

    //! Returns the type of the constant locations that AOT code holds in its image: QoreProgramLocation
    static llvm::StructType* getAotLocationType(llvm::LLVMContext& ctx);

    //! Returns the location of the body of the AOT function with the given symbol, lowered into the module, for its
    //! function table entry (QoreAOTFunc::body_loc), or a null pointer if unknown
    static llvm::Constant* getAotBodyLocation(llvm::Module& module, const std::string& symbol);

    //! Checks that the layout of the constant locations of AOT code in a module with the given data layout is the
    //! layout of QoreProgramLocation in this process; returns false with an error if not
    static bool checkAotLocationLayout(const llvm::DataLayout& dl, std::string& error);

    //! Remove decref calls whose value is proven to be NOTHING.
    //! @return the number of calls removed
    static size_t pruneNoopDecrefs(llvm::Module& module);

    //! Enable AOT mode with the given slot map.
    //! When set, process-specific opcodes emit _aot helper calls with slot indices
    //! instead of inttoptr patterns with embedded pointers.
    void setAOTMode(const AOTSlotMap* slots) {
        aot_mode = true;
        aot_slots = slots;
    }

    //! Control immediate-closure dispatch lowering in AOT mode.
    /** Init-function compilation does not emit native closure bodies, so its
        lowerers must use the generic runtime helper instead of declaring a
        dispatch symbol that will not be defined. */
    void setAOTDirectClosureFastEntry(bool v) {
        aot_direct_closure_fast_entry = v;
    }

    //! Sets the name of the native function generated for the IR function, if not its own name
    /** Lets a function be compiled again under another symbol without changing the IR function, which is shared by
        every thread running it.
    */
    void setSymbolName(const std::string& name) {
        symbol_name = name;
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

    //! Find a batch callee and record its body-summary provenance when AOT
    //! object dependency collection is active.
    std::unordered_map<const AbstractQoreFunctionVariant*,
        BatchCalleeInfo>::const_iterator findBatchCallee(
            const AbstractQoreFunctionVariant* variant) const;

    //! Return a batch callee through the dependency-recording lookup.
    const BatchCalleeInfo* getBatchCallee(
        const AbstractQoreFunctionVariant* variant) const;

    //! Set fast entry mode for Approach B: parameters are passed as LLVM function
    //! arguments instead of being loaded from the thread-local variable stack.
    //! @param name the LLVM function name for the fast entry (e.g., "fname_fast")
    //! @param args maps LocalVar* (as void*) → LLVM Value* for each parameter
    //! @param arg_kinds maps parameters to their native or boxed ABI representation
    //! @param borrowed_args boxed parameters proven not to escape the call
    //! @param return_kind native or boxed fast-entry return ABI
    //! @param return_type the declared return type from the signature; a fast entry applies it to its return
    //! values itself, since it is called directly instead of through a runtime helper that applies it
    void setFastEntryMode(const std::string& name,
            const std::unordered_map<const void*, llvm::Value*>* args,
            const std::unordered_map<const void*, BatchCalleeParamKind>* arg_kinds = nullptr,
            const std::unordered_set<const void*>* borrowed_args = nullptr,
            BatchCalleeReturnKind return_kind = BatchCalleeReturnKind::Boxed,
            const QoreTypeInfo* return_type = nullptr) {
        fast_entry_name = name;
        fast_entry_args = args;
        fast_entry_arg_kinds = arg_kinds;
        fast_entry_borrowed_args = borrowed_args;
        fast_entry_return_kind = return_kind;
        fast_entry_return_type = return_type;
        fast_entry_rejects_nothing_return = QoreTypeInfo::hasType(return_type)
            && !QoreTypeInfo::parseAcceptsReturns(return_type, NT_NOTHING);
    }

    //! Set the name of an AOT self-recursive fast entry function.
    //! When set, self-recursive CallDirect instructions in AOT mode emit direct
    //! LLVM calls to this function instead of going through qore_rt_call_direct_aot.
    //!
    //! @p fe identifies the function whose fast-entry @p name refers to;
    //! self-recursion is detected by FE pointer equality rather than base
    //! name, avoiding cross-namespace mis-matches (`OMQ::foo` → `Util::foo`
    //! previously tripped the self-recursion path because base names match).
    //! FE identity is necessary but not sufficient — see
    //! isSelfRecursiveVariant().
    void setAOTSelfRecursiveFastEntry(const std::string& name,
            const FunctionEntry* fe = nullptr,
            const std::vector<BatchCalleeParamKind>* param_kinds = nullptr,
            const std::vector<uint8_t>* param_rejects_nothing = nullptr,
            BatchCalleeReturnKind return_kind = BatchCalleeReturnKind::Boxed) {
        aot_self_recursive_fast_entry = name;
        aot_self_recursive_fe = fe;
        aot_self_recursive_param_kinds = param_kinds ? *param_kinds
            : std::vector<BatchCalleeParamKind>();
        aot_self_recursive_param_rejects_nothing = param_rejects_nothing
            ? *param_rejects_nothing : std::vector<uint8_t>();
        aot_self_recursive_return_kind = return_kind;
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
    //! the current object (NaN-boxed), read at the start of the function by the first check of a call through self
    //! (QoreIROpcode::CheckCallTarget with no operand, issue #5823); null until then
    llvm::Value* current_self_bits = nullptr;
    bool aot_direct_closure_fast_entry = true;

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

    // Callee AOT contexts loaded once in the logical function entry, keyed by
    // expression slot. The context mapping is immutable during an invocation.
    std::unordered_map<int32_t, llvm::Value*> aot_call_target_contexts;

    // Exact-class checks for stable local receivers, materialized once in the
    // logical function entry. Object validity remains guarded at each call.
    std::unordered_map<int32_t, std::unordered_map<const LocalVar*, llvm::Value*>>
        aot_exact_class_guards;
    std::unordered_map<uint32_t, const LocalVar*> stable_exact_receiver_loads;
    std::unordered_map<uint32_t, const LocalVar*> stable_scalar_loads;

    using NativeConversionCache =
        std::unordered_map<const LocalVar*,
            std::pair<llvm::BasicBlock*, llvm::Value*>>;
    NativeConversionCache stable_int_conversions;
    NativeConversionCache stable_float_conversions;

    // Approach B fast entry: LLVM function name override and parameter mapping.
    // When fast_entry_name is non-empty, lowerFunction uses it instead of func.name
    // and initializes params from fast_entry_args instead of qore_rt_load_local().
    std::string fast_entry_name;
    //! the name of the generated native function, if not the IR function's own name; see setSymbolName()
    std::string symbol_name;
    const std::unordered_map<const void*, llvm::Value*>* fast_entry_args = nullptr;
    const std::unordered_map<const void*, BatchCalleeParamKind>* fast_entry_arg_kinds = nullptr;
    //! Proven noescape boxed parameters that borrow the caller's reference.
    const std::unordered_set<const void*>* fast_entry_borrowed_args = nullptr;
    BatchCalleeReturnKind fast_entry_return_kind = BatchCalleeReturnKind::Boxed;
    bool fast_entry_rejects_nothing_return = false;
    //! the declared return type of a fast entry
    const QoreTypeInfo* fast_entry_return_type = nullptr;

    // AOT self-recursive fast entry: when set, self-recursive CallDirect in AOT mode
    // emits direct LLVM calls to this function instead of qore_rt_call_direct_aot.
    std::string aot_self_recursive_fast_entry;
    // FunctionEntry pointer of the function whose fast-entry the above
    // name refers to; `is_self_rec` matches on FE identity (pointer
    // equality) rather than base-name string equality, so a call to a
    // same-named function in another namespace (e.g. `OMQ::foo` calling
    // `Util::foo`) is not mis-identified as self-recursion.
    const FunctionEntry* aot_self_recursive_fe = nullptr;
    std::vector<BatchCalleeParamKind> aot_self_recursive_param_kinds;
    std::vector<uint8_t> aot_self_recursive_param_rejects_nothing;
    BatchCalleeReturnKind aot_self_recursive_return_kind = BatchCalleeReturnKind::Boxed;

    // IR builder
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // Block mapping: QoreIR blocks → LLVM blocks (initial blocks)
    std::unordered_map<const QoreIRBasicBlock*, llvm::BasicBlock*> block_map;

    // Final block mapping: QoreIR blocks → LLVM blocks after lowering
    // When lowering creates intermediate blocks (e.g., cmp_merge), the builder
    // ends up in a different block than the initial one. This map tracks where
    // each IR block's instructions end up, for correct PHI predecessor resolution.
    std::unordered_map<const QoreIRBasicBlock*, llvm::BasicBlock*> final_block_map;

    // Per-edge block mapping: (IR predecessor block, IR successor block) → the LLVM
    // block that outgoing edge actually originates from.  Required when lowering a
    // block's terminator emits its outgoing branches from DIFFERENT LLVM blocks —
    // e.g. TypedForeachNext*, where the done edge leaves the bounds-check block
    // while the continue edge leaves the value-extraction block — so a single
    // final_block_map entry cannot describe both edges for PHI wiring.
    std::map<std::pair<const QoreIRBasicBlock*, const QoreIRBasicBlock*>,
        llvm::BasicBlock*> edge_block_map;

    // Value mapping: QoreIR value IDs → LLVM values
    std::unordered_map<uint32_t, llvm::Value*> values;
    std::unordered_map<uint32_t, const QoreIRInstruction*> value_definitions;

    struct AOTHashStringExtractionCacheEntry {
        const QoreIRBasicBlock* block = nullptr;
        const LocalVar* local = nullptr;
        std::vector<std::string> keys;
        llvm::Value* success = nullptr;
        std::vector<std::pair<llvm::Value*, size_t>> sources;
        const QoreIRInstruction* instruction = nullptr;
        llvm::BasicBlock* dominance_block = nullptr;
        bool cross_call_eligible = false;
        bool imported_across_block = false;
    };

    // Borrowed hash-string values shared by adjacent imported pure calls.
    std::vector<AOTHashStringExtractionCacheEntry>
        aot_hash_string_extraction_cache;
    const QoreIRInstruction* aot_hash_string_extraction_instruction = nullptr;
    size_t aot_hash_string_extraction_reuses = 0;
    size_t aot_hash_string_extraction_overlap_reuses = 0;
    size_t aot_hash_string_extraction_cross_block_reuses = 0;

    // Typed collection state proven or materialized while lowering the current function.
    std::unordered_map<uint32_t, llvm::Value*> typed_list_data_ptrs;
    std::unordered_set<uint32_t> fixed_typed_list_outputs;
    std::unordered_set<uint32_t> reserve_typed_list_outputs;
    std::unordered_map<uint32_t, BatchCalleeReturnKind> native_call_result_kinds;
    std::unordered_set<uint32_t> direct_typed_list_read_sources;
    std::unordered_set<uint32_t> elided_typed_foreach_refself_values;
    std::unordered_set<uint32_t> reusable_hashdecl_literal_values;
    // Single-use declaration initializers that can construct the target
    // container representation directly.
    std::unordered_map<uint32_t, const QoreTypeInfo*>
        fresh_container_init_types;
    std::unordered_set<uint32_t> exact_fresh_container_values;
    size_t fused_fresh_container_inits = 0;
    size_t assigned_hash_guard_elisions = 0;

    //! The alloca that caches the value of a local variable in native code
    /** The variable of a foreach loop is assigned each element as it is (AssignmentMode::Element), so the cache of
        such a variable can hold a weak reference, which reads as its target: loadLocalCacheValue() loads the value
        as the program reads it, and storage() gives the alloca itself, to the code that stores, copies or releases
        the stored value.  A value loaded through storage() would be the weak reference;
        examples/test/ir/ir-local-slot-reads.qtest guards against such loads.
    */
    class LocalCacheSlot {
    public:
        DLLLOCAL LocalCacheSlot() = default;

        DLLLOCAL explicit LocalCacheSlot(llvm::Value* cache) : cache(cache) {
        }

        //! Returns the alloca, to store, copy or release the stored value
        DLLLOCAL llvm::Value* storage() const {
            return cache;
        }

    private:
        llvm::Value* cache = nullptr;
    };

    // Local variable allocas (LocalVar* address → alloca)
    // These maps drive instruction emission, so iterate in registration order rather than pointer-hash order.
    // Pointer values vary with allocator layout, even for identical source and prefix mappings.
    using LocalAllocaMap = llvm::MapVector<const void*, LocalCacheSlot>;
    LocalAllocaMap local_allocas;

    //! Loads the boxed value of a local variable from its cache as the program reads it
    /** The value of an IR-only variable of a foreach loop that holds a weak reference is its target, borrowed from
        the weak reference in the cache; a reference held by such a variable is dereferenced.  Every other value is
        loaded as it is.  The runtime helper that reads such a variable is only called when the flag of a typed
        variable is set (see element_local_read_flags), or for a node held by an untyped variable; with
        \a load_builder, which inserts outside the current block (as in the entry block), it is called for any value,
        since no block can be split there.
    */
    DLLLOCAL llvm::Value* loadLocalCacheValue(const void* key, const LocalCacheSlot& slot, llvm::Module& module,
            llvm::IRBuilder<>* load_builder = nullptr, const char* name = "");

    //! Emits the call that raises the error of a return statement that returns no value
    /** The error is the one the AST interpreter raises for such a return statement ("<return statement> expects
        type ..."), for the given declared return type.
    */
    DLLLOCAL void emitReturnStatementNothingError(llvm::Module& module, const QoreTypeInfo* return_type);

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

    //! The locals that an on_block_exit handler of the current function declares in a scope of its own
    /** These have explicit InstantiateLocal and UninstantiateLocal instructions in the IR of the handler, not in the
        IR of the function.  The handler instantiates and pops them at its own scope: instantiating a closure-bound
        one at the function's entry as well put its variable below the variables that the function declares, so that
        popping it at the end of its scope, which pops the top of the closure variable stack, popped one of them
        instead (qore#5707)
    */
    std::unordered_set<const void*> handler_block_scoped_locals;

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
    llvm::MapVector<const void*, llvm::AllocaInst*> closure_pre_inst_flags;

    // Track which value IDs already contain NaN-boxed i64 (from Invoke, Call, CatchException,
    // make_string, .any ops, LoadLocal).  Values NOT in this set are raw typed values.
    std::unordered_set<uint32_t> nanboxed_values;

    // Boxed source values retained when a typed collection read is unboxed.
    // Generic calls and NOTHING guards must observe the source value rather
    // than re-boxing a native zero produced from a sparse list entry.
    std::unordered_map<uint32_t, llvm::Value*> native_boxed_sources;
    std::unordered_map<uint32_t, BatchCalleeParamKind>
        native_boxed_source_kinds;

    // Value IDs known not to be NOTHING.  This is intentionally narrower than
    // assigned-state: only typed locals proven assigned on every load contribute
    // here, so auto/any values assigned to NOTHING are not misclassified.
    std::unordered_set<uint32_t> known_not_nothing_values;

    // LocalVar* keys whose LoadLocal results can be marked known-not-NOTHING.
    std::unordered_set<const void*> assigned_non_nothing_locals;

    // Set of LocalVar* (as void*) that are pre-instantiated by the caller (tiered
    // compilation); skip qore_rt_instantiate_local / qore_rt_uninstantiate_local for these.
    const std::unordered_set<const void*>* pre_instantiated_locals = nullptr;
    //! the pre-instantiated locals of a handler function in native code: those of its IR function and the locals that
    //! it declares other than closure-bound ones (see lowerFunction(), #6002)
    std::unordered_set<const void*> handler_pre_instantiated_locals;

    // Set of LocalVar* (as void*) that are only accessed by LoadLocal/StoreLocal
    // in fully-lowered IR code.  For these locals, skip qore_rt_assign_local
    // and related runtime-stack synchronization when ownership permits.
    const std::unordered_set<const void*>* ir_only_locals_set = nullptr;

    // Original IR-only set used only for reload decisions. AOT may remove body
    // locals that need runtime ownership from ir_only_locals_set so StoreLocal
    // still syncs them, but those locals remain invisible to AST callbacks.
    const std::unordered_set<const void*>* reload_exempt_locals_set = nullptr;

    // Phase 4: True when ALL locals in the function are invisible to AST
    // callbacks, meaning reloadAllLocalsFromRuntime() can be skipped entirely
    // after calls.
    bool all_locals_reload_exempt = false;

    // Sets of IR-only locals that use native (unboxed) allocas.
    std::unordered_set<const void*> native_int_locals;
    std::unordered_set<const void*> native_float_locals;
    std::unordered_set<const void*> native_bool_locals;

    // Set of body locals that are IR-only — these can have their allocas initialized
    // to NOTHING directly instead of loading from the runtime stack (since the fast
    // call path skips their instantiation when all body locals are IR-only).
    std::unordered_set<const void*> ir_only_body_locals;

    // AOT body locals are pre-instantiated by the runtime frame wrapper with an
    // initial NOTHING value. LLVM allocas can therefore start at their default
    // without an entry qore_rt_load_local_aot(); runtime-owned StoreLocal/lvalue
    // mutation paths reload after publishing a real value to the runtime stack.
    std::unordered_set<const void*> aot_body_locals;

    // AOT-adjusted IR-only set: removes pre-instantiated body locals that need
    // runtime-stack ownership. Proven-assigned native int/float locals can stay
    // IR-only because they carry no references and cannot trigger double-free.
    std::unordered_set<const void*> aot_adjusted_ir_only;
    //! The locals with IR storage (QoreIRFunction::hasIRStorage()) when the function has reference slot locals: the
    //! set that ir_only_locals_set points to, which then includes them
    std::unordered_set<const void*> ir_storage_locals;
    //! The body locals of the function being lowered, which hold a reference only if a store gives them one or the
    //! function creates one (see usesReferenceFlag())
    std::unordered_set<const void*> reference_flag_body_locals;
    //! True if the function being lowered creates a reference (\\var), so that any local of a type that can hold
    //! one can be given one (see usesReferenceFlag())
    bool function_creates_reference = false;

    // Lazy local-cache invalidation. Calls that can execute AST/Qore code bump
    // the function epoch; LoadLocal reloads only the specific stale local it
    // reads. This keeps LLVM IR size O(calls + local reads), not O(calls * locals).
    llvm::AllocaInst* local_reload_epoch = nullptr;
    llvm::MapVector<const void*, llvm::AllocaInst*> local_valid_epochs;
    bool local_reload_boundary_cleanup_pending = false;

    // Saved on_block_exit handler count at function entry (for LIFO cleanup)
    llvm::Value* obe_saved_count = nullptr;

    // Saved catch-scope stack depth.  With specialized tracking this is an i64
    // alloca populated on the first dynamically entered catch scope; otherwise
    // it is the legacy entry-time qore_rt_catch_depth() result.
    llvm::Value* catch_depth_saved = nullptr;

    // Number of runtime catch scopes currently active in this native frame.
    // Kept in an alloca so LLVM can promote it to SSA and remove unwind guards
    // from exits statically known to be balanced.
    llvm::AllocaInst* catch_scope_count = nullptr;

    // LLVM blocks where this frame enters a runtime catch scope.  Final exit
    // lowering uses forward reachability from these blocks to omit unwind
    // guards on exits that cannot possibly own an active catch scope.
    std::unordered_set<llvm::BasicBlock*> catch_entry_blocks;

    // True when the current function contains deferred on_block_exit handlers.
    bool has_on_block_exit_handlers = false;

    // Per-scope saved on_block_exit counts (scope_id -> saved count value)
    // Used by ScopeEnter/ScopeExit for nested on_exit handler execution
    std::unordered_map<uint32_t, llvm::Value*> scope_obe_counts;
    //! the number of catch scopes of the frame open at each ScopeEnter (catch_scope_count, or the depth of the catch
    //! stack without it): the landing pad of a try statement leaves the catch scopes opened in its try block (#5969)
    std::unordered_map<uint32_t, llvm::Value*> scope_catch_counts;

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
    // must be decref'd at block scope exit and function exit.  Body locals are
    // registered when preCreateLocalAllocas() creates their alloca, so every
    // scope exit and return sees them regardless of block lowering order.
    std::vector<llvm::AllocaInst*> owned_ir_local_allocas;
    std::unordered_set<const void*> owned_ir_local_alloca_keys;

    // Entry-block array of pointers to owned_ir_local_allocas slots.  The inline
    // cleanup emits one load/store/decref triple per IR-only local in the shared
    // error-return block; once SROA promotes those slots, every local needs a PHI
    // for every edge reaching that block, so the PHI count is
    // (locals x exit edges) and code generation becomes superlinear.  Storing the
    // slot addresses here keeps them in memory, so the cleanup is a single
    // qore_rt_cleanup_run_allocas() call and no PHI is required.  Prepared at
    // function finalization, when both factors are known, and only when their
    // product exceeds getCleanupPhiBudget() -- every other function keeps the
    // inline triples and full SSA promotion of its locals.
    llvm::AllocaInst* boxed_exit_cleanup_array = nullptr;
    unsigned boxed_exit_cleanup_array_count = 0;

    // Entry-block array of pointers to iterator_cleanup_allocas slots.  Same
    // problem and same remedy as local_cleanup_array: emitLateExitCleanup()
    // repeats the per-iterator load/release/clear sequence at every return and
    // resume, so once SROA promotes the slots the merged common return needs
    // (iterators x exits) PHI operands.  Prepared at function finalization.
    llvm::AllocaInst* iterator_cleanup_array = nullptr;
    unsigned iterator_cleanup_array_count = 0;
    //! The array form of the iterators that an exception exit releases with the statement temporaries: those without
    //! a place among the locals (see positioned_iterator_allocas), which the release of the locals releases instead
    llvm::AllocaInst* iterator_cleanup_unpositioned_array = nullptr;
    unsigned iterator_cleanup_unpositioned_count = 0;

    // Allocas for Invoke/ConstString results that need cleanup at function exit.
    // qore_rt_invoke_expr returns +1 ref; these allocas track the results so they
    // can be decref'd at exit (matching the IR interpreter's cleanup vector).
    std::vector<llvm::Value*> invoke_result_allocas;
    //! the slot of each of invoke_result_allocas in invoke_cleanup_array, if it has one: a long release sequence is
    //! emitted as one call of qore_rt_cleanup_release_slots() with a table of the slots (qore#5962)
    std::unordered_map<llvm::Value*, unsigned> invoke_cleanup_slot;

    // Entry-block array of pointers to invoke_result_allocas slots.  Large
    // functions use qore_rt_cleanup_run_allocas() to avoid emitting thousands
    // of load/decref/store triples at every cleanup exit.
    llvm::AllocaInst* invoke_cleanup_array = nullptr;
    unsigned invoke_cleanup_array_capacity = 0;
    unsigned invoke_cleanup_array_count = 0;
    bool invoke_cleanup_array_overflow = false;
    // Entry-block array of pointers to the slots of invoke_result_allocas that are not persistent cleanups, in
    // registration order.  Every exit of a function releases these temporaries before its on_block_exit handlers
    // (emitExitStatementCleanup()); emitted inline, that is a load/store/decref triple per temporary of the whole
    // function at every exit, so a function with many statements and many exits (ex: a large switch whose cases
    // return) grew quadratically -- 3.9 million LLVM instructions and over 6 GiB of compile memory for 1600
    // returning cases.  Large functions release them with one qore_rt_cleanup_run_allocas() call instead.
    llvm::AllocaInst* exit_temp_cleanup_array = nullptr;
    unsigned exit_temp_cleanup_array_count = 0;
    bool exit_temp_cleanup_array_overflow = false;
    // the slots of invoke_result_allocas that are not persistent cleanups
    size_t exit_temp_cleanup_count = 0;
    // Map from value ID to invoke-result alloca (for clearing at Return)
    std::unordered_map<uint32_t, llvm::Value*> invoke_alloca_map;
    //! the values that the Return instructions of the function return (qore#5941)
    /** When an exception interrupts a return (as one that a handler of a block that the return leaves raises), the
        AST interpreter keeps the value of the return until the function ends, after its locals: an exit of the
        function on an exception releases the cleanup allocas of these values after the locals
        (see holding_return_values).
    */
    std::unordered_set<uint32_t> return_value_ids;
    //! for each value of a Return instruction, the values of return_value_ids that hold it
    std::unordered_map<uint32_t, std::vector<uint32_t>> return_value_family;
    //! true between the release of the temporaries and that of the locals of an exit on an exception: the values of
    //! return_value_ids are released after the locals (see emitLocalUninstantiation())
    bool holding_return_values = false;
    //! Returns true if \a alloca_ptr is the cleanup alloca of one of return_value_ids
    bool isReturnValueAlloca(llvm::Value* alloca_ptr) const;
    //! the blocks at the start of the catch paths of try statements that release the values of the returns of their
    //! try blocks (QoreIRFunction::catch_interrupted_returns), filled once the function is lowered, as the catch
    //! block is lowered before the try block (qore#5941)
    std::vector<std::pair<llvm::BasicBlock*, const std::vector<uint32_t>*>> catch_return_release_blocks;
    //! Fills the blocks of catch_return_release_blocks
    void finalizeCatchReturnReleases(llvm::Module& module);
    //! Releases the cleanup allocas of return_value_ids, after the locals of an exit on an exception
    void emitHeldReturnValueRelease(llvm::Module& module);
    //! the value id of each cleanup alloca of invoke_alloca_map, for the order of the release of a statement's
    //! temporaries by an exception
    std::unordered_map<llvm::Value*, uint32_t> cleanup_alloca_value_ids;
    //! the class of each value for the order of the release of a statement's temporaries by an exception (see
    //! qore_ir_build_exception_release_groups())
    std::vector<uint32_t> exception_release_groups;

    //! Records the cleanup alloca of a value
    void setInvokeAlloca(uint32_t id, llvm::Value* alloca) {
        invoke_alloca_map[id] = alloca;
        cleanup_alloca_value_ids[alloca] = id;
    }
    //! the output slot of each IteratorNext, by its result id: it holds a reference to the current element, which
    //! ReleaseOwnedTemp releases when a return or an exception leaves the loop
    std::unordered_map<uint32_t, llvm::Value*> iterator_element_allocas;
    //! The release of the previous element of each IteratorNext with release values (QoreIRIteratorNextInstruction::
    //! release_values), and the values: the loop's body defines them after the instruction, so their cleanup slots
    //! are known only once the function is lowered (see finalizeIteratorReleases())
    std::vector<std::pair<llvm::CallInst*, std::vector<uint32_t>>> pending_iterator_releases;

    //! Emits, after the release of the previous element of each IteratorNext with release values, the release of the
    //! reference that the cleanup slot of each value holds, in their order, as ReleaseOwnedTemp releases it; a value
    //! without a cleanup slot of its own is left to its usual release
    void finalizeIteratorReleases(llvm::Module& module, llvm::Function* llvm_func);

    //! Emits the exception check of \a inst on the path where its helper's \a result is NOTHING, which the helpers of
    //! operations that can raise (DivAny, ModAny, DivNumber, AddString, StringConcat) return when they raise, so that
    //! the exception leaves the code where it is raised, as in the AST interpreter, at the cost of a comparison on the
    //! normal path: left pending, it let the code after it run, and a functional operator's next() took it for its
    //! own (qore#5670)
    void emitExceptionCheckIfNothing(llvm::Module& module, llvm::Function* llvm_func, const QoreIRInstruction* inst,
            llvm::Value* result);

    //! Emits an untyped division or modulo (DivAny, ModAny) of \a lhs by \a rhs: two integers with a divisor that is
    //! not zero, and two floats with a divisor that is not zero for a division, inline; any other operands by the
    //! helper (qore_rt_div_any(), qore_rt_mod_any()), whose exception raises where it happens
    //! (emitExceptionCheckIfNothing()); returns the NaN-boxed result
    llvm::Value* emitAnyDivModFastPath(bool modulo, const QoreIRInstruction* inst, llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module);
    //! The selector-segment loop of the iterator of a list of selectors created with QoreIRListSelectorFlag::Segment
    //! (qore#5711): its cursor and the selector needed next live in the frame of the function, and the iterator only
    //! keeps the sequence
    struct SelectorSegmentState {
        //! the cursor: an array of QORE_RT_SEGMENT_CURSOR_SIZE int64 values (see QoreRtSelectorSegmentCursor)
        llvm::AllocaInst* cursor = nullptr;
        //! the index of the selector needed next
        llvm::AllocaInst* selector = nullptr;
        //! the sequence, NaN-boxed, which dominates the loop
        llvm::Value* seq = nullptr;
        //! the QoreIRListSelectorFlag flags of the iterator
        int64_t flags = 0;
        //! the number of selectors
        int64_t count = 0;
    };
    //! the selector-segment loops of the function, by the value of their iterator
    std::unordered_map<uint32_t, SelectorSegmentState> selector_segment_states;
    //! the iterators of lists of selectors whose IteratorCreateSelectors has been lowered
    std::unordered_set<uint32_t> lowered_selector_iterators;
    //! fails the lowering if the iterator of a list of selectors is used before its creation is lowered
    /** The state of a selector-segment loop is registered when its IteratorCreateSelectors is lowered, so that the
        pending, feed and next steps of the loop, which use it, must be lowered after it; a step lowered first would
        use the iterator instead of the cursor (qore#5711).

        @return true if \a iterator is not the iterator of a list of selectors, or if its creation has been lowered
    */
    DLLLOCAL bool checkSelectorIteratorLowered(uint32_t iterator, std::string& error) const;
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

    //! The release of a local at an UninstantiateLocal lowered before the code that registers some of the allocas
    //! holding references to its value
    /** Blocks are lowered in layout order, and a block's exception cleanup (block.lvars.exception.cleanup.cont) is
        laid out before the statements of the block: its UninstantiateLocal of a local declared in the block finds
        none of the allocas that the local's stores register later (local_cleanup_allocas, the reload tracker and the
        deferred reload value), whose references then outlived the local's value, so that the destructor of an
        object it held ran after those of locals declared before it.  finalizeDeferredLocalReleases() releases them
        there once the function is lowered; they are all allocas of the entry block, which dominates every release
        point (a debug build asserts it).
    */
    struct DeferredLocalRelease {
        //! the local
        const void* key;
        //! the block of the UninstantiateLocal
        llvm::BasicBlock* block;
        //! the instruction the releases are inserted after (null: at the start of \a block), which the release of
        //! the local's value follows
        llvm::Instruction* after;
        //! the allocas released when the UninstantiateLocal was lowered
        std::vector<llvm::Value*> released;
    };
    std::vector<DeferredLocalRelease> deferred_local_releases;

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
        // Matches QoreIRInstruction::temp_scope_id of the PushTempMark that
        // produced this entry (0 = legacy/unpaired).  A DiscardTemps selects
        // its mark by this id rather than by stack position, so the pairing
        // survives basic-block reordering (e.g. short-circuit operators that
        // move a loop-body DiscardTemps after a sibling block's DiscardTemps).
        uint32_t scope_id = 0;
        //! the number of iterator_cleanup_allocas when the mark was pushed
        size_t iterator_count = 0;
    };
    std::vector<TempCleanupMark> temp_cleanup_marks;
    //! The release code of the exception edges to each handler block (see getExceptionEdgeTarget())
    /** An edge releases the temporaries of the temp scopes that the exception leaves, in the order in which the AST
        interpreter unwinds them: those of argument lists, then the receivers (qore_ir_order_exception_release()).  The
        edges of the checks of a statement to the same handler release the temporaries registered up to each check, and
        each of the two parts of a later check's order is mostly that of an earlier check with the temporaries
        registered after it in front.  So each part is a chain of blocks, each of which releases the temporaries that a
        check adds and continues with the block of the earlier check, instead of each edge releasing all of its
        temporaries itself, which made the code of a statement quadratic in its size (qore#5962).  The chain of the
        argument lists ends in a block that continues with the receivers of the edge, which the edge stores in \c
        selector; that of the receivers ends in the handler.
    */
    struct ExceptionEdgeChain {
        //! the temporaries that the last block of the chain releases with the blocks that it continues with
        std::vector<llvm::Value*> temps;
        //! the last block of the chain
        llvm::BasicBlock* block = nullptr;
    };
    struct ExceptionEdges {
        ExceptionEdgeChain lists;
        ExceptionEdgeChain receivers;
        //! the end of the chain of the argument lists: a switch on \c selector to the receivers of the edge
        llvm::BasicBlock* lists_end = nullptr;
        llvm::SwitchInst* lists_end_switch = nullptr;
        //! the receivers of the edge (0: none, then the handler), stored by each edge
        llvm::AllocaInst* selector = nullptr;
        //! the case of each block of the receivers' chain in lists_end_switch
        std::unordered_map<llvm::BasicBlock*, uint32_t> receiver_cases;
    };
    std::unordered_map<llvm::BasicBlock*, ExceptionEdges> exception_edges;
    //! the number of invoke_result_allocas when the mark of each temp scope was closed (see emitDiscardTemps())
    std::unordered_map<uint32_t, size_t> temp_scope_end;
    //! The marks of the temp scopes that were open when each temp scope was closed, up to its own mark (see
    //! getTempScopeMarks())
    /** The blocks are lowered in layout order, which is not the order of the control flow: a block that comes after
        the block holding the end of its statement (DiscardTemps) has its exception checks lowered once the mark of
        their temp scope is gone (qore#5997).  The join blocks of ?:, ??, ?*, && and || are placed after the blocks of
        their operands since qore#6080, so these operators no longer produce such blocks; the saved marks remain a
        safety net for any other construct that does.
    */
    std::unordered_map<uint32_t, std::vector<TempCleanupMark>> closed_temp_cleanup_marks;

    //! An exception check outside a try block whose edge to the error return is completed once the function has been
    //! lowered (see finalizeOrderedErrorReturnEdges())
    struct OrderedErrorReturnSite {
        //! the conditional branch of the exception check
        llvm::Instruction* check = nullptr;
        //! the branch to the error return: the check, or the branch of its implicit context restore edge
        llvm::Instruction* branch = nullptr;
        //! the temp scope of the raising instruction
        uint32_t scope_id = 0;
        //! the statement's cleanup allocas registered before the check: invoke_result_allocas from start to count
        size_t start = 0;
        size_t count = 0;
        //! the first of the cleanup allocas registered after the check (count, plus the SSA cleanups of enclosing
        //! scopes that the check put in allocas)
        size_t later_start = 0;
        //! the iterators of the statement's functional operators: iterator_cleanup_allocas from iter_start to iter_end
        size_t iter_start = 0;
        size_t iter_end = 0;
        llvm::DebugLoc loc;
    };
    std::vector<OrderedErrorReturnSite> ordered_error_return_sites;

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
    //! the block defining each value, for decisions that are only safe when a value is used in the block defining it
    /** The use counts above are static: a value defined before a loop and used once in its body counts one use,
        but that use runs on every iteration.
    */
    std::unordered_map<uint32_t, const QoreIRBasicBlock*> value_def_block;
    // the temp scope (QoreIRInstruction::temp_scope_id) of the instruction defining each value
    std::unordered_map<uint32_t, uint32_t> value_def_temp_scope;
    // the values created by a call instruction (qore_ir_call_arg_start() >= 0)
    std::unordered_set<uint32_t> value_defined_by_call;
    //! the call arguments that are list or hash literals or AddAny results whose release at the call's return can be
    //! observed, by value id (qore_ir_find_observable_temp_call_args())
    std::vector<uint8_t> observable_temp_call_args;
    // the values read from a variable (LoadLocal, LoadArg, LoadClosure, LoadGlobal, LoadThreadLocal): one that the
    // read gave a reference of its own to (a cleanup slot) and that a call takes as an argument is released when the
    // call returns, as any argument
    std::unordered_set<uint32_t> value_defined_by_variable_read;
    // non-zero for each value id of a call argument that a later execution of the call uses again without the value
    // being defined again, such as a read hoisted out of a loop (qore_ir_find_reused_call_args()): no call releases
    // it or takes its cleanup slot
    std::vector<uint8_t> reused_call_args;
    // the values read through a reference by a local with a reference flag -> the slot holding the reference that the
    // read took (see emitReferenceLocalRead()): the slot is not a cleanup slot of the value, as the value is the
    // cached value when the local holds no reference, so a call that takes the value as an argument releases the slot
    // only if it holds the value (qore_rt_release_reference_read())
    std::unordered_map<uint32_t, llvm::AllocaInst*> reference_read_slots;
    // the slot of the last reference read emitted by emitReferenceLocalRead()
    llvm::AllocaInst* last_reference_read_slot = nullptr;

    // Single-use or nonescaping stored closure values consumed directly by
    // CallClosureDirect. Their creation can be fused into the call without
    // changing the identity of any closure value visible to Qore code.
    std::unordered_map<uint32_t, const QoreIRCreateClosureInstruction*>
        immediate_closure_creates;
    // Exact single-variant function call references that are immutable at
    // their call sites.  AOT can dispatch these through the callee's typed
    // fast entry while retaining ordinary call-reference creation semantics.
    std::unordered_map<uint32_t, const AbstractQoreFunctionVariant*>
        known_function_call_refs;
    // Native scalar captures cached at creation for entry-assigned stored
    // closures whose captured parameters cannot be modified by the owner.
    std::unordered_map<uint32_t, std::vector<llvm::AllocaInst*>>
        stored_closure_capture_allocas;
    // Proven nonescaping immediate closures with read-only required scalar
    // parameter captures can initialize the native cache once at owner entry.
    std::unordered_set<uint32_t> entry_promoted_closure_captures;
    // Top-level closure locals can be changed by called code outside the owner
    // IR. Loads mapped here use a retained closure-identity guard before the
    // native direct path and retain the ordinary dynamic fallback.
    std::unordered_map<uint32_t, const QoreIRCreateClosureInstruction*>
        guarded_stored_closure_creates;
    // Function-lifetime retained references used for inline identity guards.
    std::unordered_map<uint32_t, llvm::AllocaInst*>
        guarded_stored_closure_identity_allocas;
    // StoreLocal/LoadLocal instructions eliminated when an entry-assigned,
    // nonescaping closure local is used only as a direct call target.
    std::unordered_set<const QoreIRInstruction*> elided_closure_local_accesses;

    // Comparison result IDs consumed exclusively by ToBool.  These can remain
    // native i1 through LLVM lowering instead of being boxed and decoded again.
    std::unordered_set<uint32_t> native_boolean_result_values;

    // Set of register IDs that are ONLY used as DotEval bases (operands[0]
    // of DotEvalMethodDirect, InvokeDotEvalMethodDirect, or Invoke with a
    // DotEval invoke_opcode).  For these registers, LoadSelfMember uses the
    // _for_call variant that returns the raw value without evaluating
    // WeakReferenceNode — safe because all DotEval helpers handle NT_WEAKREF.
    // This avoids creating temporary strong references from weak member
    // dereferences that would keep objects alive for the function lifetime.
    std::unordered_set<uint32_t> dot_eval_only_bases;
    //! The values read from a local's storage (LoadLocal and StoreLocal results) that a later write to the local can
    //! release while they still have a use (see qore_ir_find_local_reads_overwritten_before_use()): a value read
    //! from the local cache is borrowed from it, so each of these takes a reference of its own, released as a
    //! temporary's
    std::unordered_set<uint32_t> overwritten_local_reads;

    // Locals assigned through the weak assignment operator. Loads must go
    // through LocalVar::eval() so weak refs are resolved to the current target
    // (or NOTHING if the target has already been deleted), rather than reading
    // the cached WeakReferenceNode from an LLVM alloca.
    std::unordered_set<const void*> weak_assigned_locals;
    //! Returns true if a local is declared with a reference type: it is read through the runtime stack on every load
    /** Its value is that of the referenced lvalue, which can change without a store to the local (x = 10), so it is
        never read from its native cache, and a store to it writes the referenced lvalue.  A local of no reference
        type that holds a reference is handled by its reference flag instead (see usesReferenceFlag()).
    */
    bool readsThroughReference(const LocalVar* local) const {
        return local && QoreTypeInfo::isReference(local->getTypeInfo());
    }

    //! The reference flag of each local that has one: see localReferenceFlag()
    std::unordered_map<const void*, llvm::AllocaInst*> local_reference_flags;

    //! Returns true if a local can hold a reference without a reference type and so has a reference flag
    /** A local of no type, \c auto or \c any can hold a reference (<tt>auto r = \x;</tt>, a reference element of a
        container, a reference passed to a parameter), which it reads as the referenced lvalue and through which a
        store writes; its native cache then holds the reference itself (see qore_rt_load_local_raw()), and the flag
        records whether it does: a load dereferences the cached value only when the flag is set
        (qore_rt_read_reference_local()), so a local that never holds a reference reads its cache as any other.
        Closure-bound, weak-assigned and foreach variables have mechanisms of their own.
    */
    bool usesReferenceFlag(const void* key) const;

    //! Returns true if a local keeps a reference it holds in its native cache (QoreIRFunction::reference_slot_locals)
    bool isReferenceSlotLocal(const void* key) const {
        return current_ir_func && current_ir_func->reference_slot_locals.count(key)
            && ir_only_locals_set && ir_only_locals_set->count(key);
    }

    //! Returns true if a local is an object local whose object has a real reference from its native cache
    /** See QoreIRFunction::real_reference_locals: a store marks the object stored and releases the object replaced
        with its mark (qore_rt_object_slot_replace()), and a release of the cache releases the mark with the value
        (qore_rt_object_slot_release())
    */
    bool isRealReferenceLocal(const void* key) const {
        return current_ir_func && current_ir_func->hasRealReferenceStorage(key)
            && ir_only_locals_set && ir_only_locals_set->count(key);
    }

    //! Returns true if an alloca is the native cache of an object local (see isRealReferenceLocal())
    bool isRealReferenceAlloca(const llvm::AllocaInst* alloca) const;

    //! Returns the reference flag of a local (an i8 that is non-zero if its native cache holds a reference)
    /** Created in the entry block, cleared, the first time it is needed; null if the local has none (see
        usesReferenceFlag())
    */
    llvm::AllocaInst* localReferenceFlag(const void* key, llvm::Function* llvm_func);

    //! Emits the load of a local's value from the runtime stack for its native cache
    /** For a local with a reference flag (see usesReferenceFlag()), a reference that the local holds is loaded as the
        reference itself and the flag is set accordingly (qore_rt_load_local_raw()); any other local is loaded with
        qore_rt_load_local().  Every load that fills a local's native cache uses this function.

        @return the value loaded, with a reference for the cache
    */
    llvm::Value* emitLocalCacheLoad(const void* key, llvm::IRBuilder<>& b, llvm::Module& module);

    //! Emits the read of a local with a reference flag: the cached value, or the referenced value if it is a reference
    /** The referenced value is read only when the flag is set (qore_rt_read_reference_local()); it is owned and kept,
        for the consumers of the value returned, until the end of the statement (the read after the next one at this
        site releases it earlier); the AST interpreter releases it when the expression using it completes

        @param cached the value in the local's cache
        @param inst the load whose exception edge an exception raised by the read takes (the referenced lvalue can be
        gone), or nullptr if the caller checks for an exception itself
    */
    llvm::Value* emitReferenceLocalRead(const void* key, llvm::Value* cached, llvm::Module& module,
            llvm::Function* llvm_func, const QoreIRInstruction* inst = nullptr);

    //! Emits the reference flag of a value (an i8, non-zero if the value is a reference) at the builder's position
    /** Only a node can be a reference: qore_rt_value_is_reference() is called for a node only, and not at all for
        a value that the IR knows is never a reference (QoreIRFunction::valueNeverReference(), ex: a string constant)

        @param value_id the IR value of \a value
    */
    llvm::Value* emitValueIsReference(llvm::Value* value, QoreIRValue value_id, llvm::Module& module,
            llvm::Function* llvm_func);
    //! IR-only variables of foreach loops (AssignmentMode::Element): their caches can hold weak references, which
    //! read as their targets (see loadLocalCacheValue())
    std::unordered_set<const void*> ir_only_element_locals;

    //! The flags of the typed variables in ir_only_element_locals: whether the cache holds a weak reference
    /** Only a foreach store assigns a typed variable of a foreach loop a weak reference (any other store assigns its
        target), and it sets the flag; a read checks the flag instead of the value, so that a read of any other value
        makes no call.  Another store leaves the flag as it is: a flag that is still set only makes a read call the
        helper that reads the variable, which gives any other value as it is.  An untyped variable can also hold a
        reference, which any store can assign it, so its reads check the value itself.
    */
    std::unordered_map<const void*, llvm::AllocaInst*> element_local_read_flags;

    //! Returns the flag of a typed variable of a foreach loop in ir_only_element_locals (see
    //! element_local_read_flags), or nullptr for any other variable
    DLLLOCAL llvm::Value* getElementLocalReadFlag(const void* key, llvm::Function* llvm_func);

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
    //! The cleanup allocas of the iterators of foreach statements whose place among the local slots is known
    //! (QoreIRFunction::interpreter_iterator_slot_boundary), with the boundary, in the order they were lowered: an
    //! exception exit releases such an iterator at its place among the locals (see getExitReleaseSequence()), as the
    //! AST interpreter releases the value a foreach statement iterates when the statement ends, after the locals of
    //! the loop body and before the loop variable and the locals declared before the statement
    std::vector<std::pair<llvm::AllocaInst*, uint32_t>> positioned_iterator_allocas;
    std::unordered_set<const llvm::Value*> positioned_iterator_alloca_set;

    // Reload tracker allocas for local variables modified by lvalue operations.
    // Each tracker alloca holds the most recent qore_rt_load_local reload value
    // (+1 ref) so it can be decref'd before being replaced or at function exit.
    llvm::MapVector<const void*, llvm::Value*> local_reload_trackers;

    // Deferred decref allocas for reload trackers.  When a reload replaces the
    // tracker value, the old value is moved here instead of being decrefd
    // immediately.  This prevents a use-after-free: LoadLocal reads from the
    // alloca (same value as the tracker), and if the reload decrefs the tracker
    // immediately, any live SSA value from LoadLocal becomes a dangling pointer.
    // Deferring by one cycle ensures the old value survives until the next
    // reload, by which time the SSA value has been consumed.
    llvm::MapVector<const void*, llvm::Value*> local_reload_deferred;

    // Release reload tracker refs that survived a side-effecting statement.
    // Deferred reload refs keep pre-call SSA values alive through the rest of
    // the statement; at DiscardTemps those temps are dead and the refs can be
    // dropped before the next source statement observes destructor timing.
    void flushLocalReloadStateAtTempBoundary(llvm::Module& module);

    // Error return block: used for exception cleanup when outside a try block.
    // When a Call/CallDirect/CallMethodDirect throws outside a try, execution
    // jumps here to return NOTHING immediately.  Created lazily on first use.
    llvm::BasicBlock* error_return_block = nullptr;

    // JIT deopt block: on guard failure, sets thread-local deopt flag and branches
    // to error_return_block.  evalTiered() checks the flag and re-executes via AST.
    // Created lazily on first guard with deopt_target.
    llvm::BasicBlock* jit_deopt_block = nullptr;

    // Debug-step hook state (issue #5352).
    // The IR basic block currently being lowered.  Reset per function.
    const QoreIRBasicBlock* current_lowering_block_ = nullptr;

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
    //! the number of native call frames that the code being emitted runs in (emitPushNativeCallFrame() and friends),
    //! which a C++ exception thrown by a helper would leave on the thread's call stack, as it skips their pop; the
    //! code in a frame therefore calls the helpers that return their exceptions (emitMaybeInvoke())
    int open_native_call_frames = 0;

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
    //! the flagged PushImplicitArg / PushImplicitElement instructions open at each instruction that leaves the
    //! function on an exception, outermost first (see qore_ir_find_open_implicit_contexts())
    std::unordered_map<const QoreIRInstruction*, std::vector<const QoreIRInstruction*>> open_implicit_contexts;
    //! Returns the block that restores the implicit arguments and elements open at \a inst and then branches to
    //! \a target, or \a target if none is open
    llvm::BasicBlock* getImplicitContextRestoreEdge(llvm::Module& module, llvm::Function* llvm_func,
            const QoreIRInstruction* inst, llvm::BasicBlock* target);
    //! Emits, at the insertion point, the restoration of the implicit arguments and elements open at \a inst,
    //! innermost first, on a path where an exception leaves the function from \a inst (see
    //! getImplicitContextRestoreEdge())
    void emitImplicitContextRestore(llvm::Module& module, const QoreIRInstruction* inst);

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

    //! Emits the construction of a typed hash or list from its lowered initializer (the instruction's operand 0)
    /** For NewComplexHash and NewComplexList with their initializer lowered as an operand (issue #5704): calls the
        runtime helper that builds the value from the initializer, by type pointer (JIT) or type path (AOT), with the
        hash initializer's precheck as for VrnConstruct.

        @param maybe_invoke true to emit the call with emitMaybeInvoke(), false for a plain call (in an Invoke)

        @return the NaN-boxed result, or nullptr with \a error set
    */
    llvm::Value* emitComplexConstructFromOperand(const QoreTypeInfo* type_info, bool is_hash,
            const QoreIRInstruction* inst, bool maybe_invoke, llvm::Module& module, llvm::Function* llvm_func,
            std::string& error);

    // AOT batch fast entries are only valid when the callee's own AOT context
    // is cached at runtime.  Emit a guarded fast-entry call with a normal AOT
    // helper fallback for linked/source-stripped cases where the fast entry is
    // visible but the callee context is not cached.
    llvm::Value* emitAotBatchFastEntryOrFallback(llvm::Module& module,
            llvm::Function* llvm_func, const QoreIRInstruction* inst,
            int32_t slot, llvm::Function* fast_fn,
            const BatchCalleeInfo& callee_info,
            const std::vector<llvm::Value*>& raw_args,
            const std::vector<uint32_t>& raw_arg_ids,
            const std::vector<llvm::Value*>& boxed_args,
            llvm::Value* args_array, llvm::Value* arg_cleanups,
            int nargs, bool has_arg_cleanups, const char* fallback_name,
            const char* fallback_consume_name, std::string& error,
            llvm::Value* object_base = nullptr,
            uint32_t object_base_id = UINT32_MAX,
            const char* fallback_throwing_name = nullptr,
            const char* fallback_consume_throwing_name = nullptr,
            bool require_exact_object_class = false,
            const std::vector<uint8_t>* date_kind_guards = nullptr);

    //! Returns @a can_use_fast_entry ANDed with the date kind checks @a date_kind_guards of a call's arguments
    /** @a date_kind_guards has a QoreDateKindGuardFlags entry (0: no check) per argument, as set by
        qore_ir_fast_entry_args_need_no_binding(); the check of a date argument for a date<absolute> or date<relative>
        parameter that the fast entry does not check (#5730)
    */
    llvm::Value* emitFastEntryDateKindGuards(llvm::Module& module, llvm::Value* can_use_fast_entry,
            const std::vector<uint8_t>& date_kind_guards, const std::vector<llvm::Value*>& raw_args,
            const std::vector<uint32_t>& raw_arg_ids, const std::vector<llvm::Value*>& boxed_args);

    //! Converts the NaN-boxed result of a generic fallback call to the return ABI of @a callee_info's fast entry
    //! and releases the boxed result if it was converted
    llvm::Value* emitFallbackResultToFastReturnKind(llvm::Module& module, const BatchCalleeInfo& callee_info,
            llvm::Value* fallback_result);

    // Emit an imported pure scalar leaf body. Returns nullptr when the summary
    // is absent, disabled, or not valid for the supplied native arguments.
    llvm::Value* emitAOTScalarLeaf(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args);

    // Emit a bounded pure native-integer expression summary.
    llvm::Value* emitAOTIntExpression(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args, llvm::Module& module,
            llvm::Function* fallback_fn, bool* proven_nothrow = nullptr);

    // Emit a bounded pure native-float expression summary without fast-math flags.
    llvm::Value* emitAOTFloatExpression(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args,
            llvm::Function* fallback_fn,
            bool* proven_nothrow = nullptr);

    // Emit an imported encoding-aware string operation. Returns nullptr when
    // no valid summary is available for the supplied fast-entry arguments.
    llvm::Value* emitAOTStringOp(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args, llvm::Module& module);

    //! Emit an imported bounded typed string expression.
    llvm::Value* emitAOTStringExpression(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args, llvm::Module& module,
            const QoreIRStringConsumerCallInstruction* fused_call = nullptr,
            llvm::Function* fallback_fn = nullptr);

    //! Emit an imported string producer directly into a supported consumer.
    llvm::Value* emitAOTStringProducerConsumer(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args,
            const QoreIRStringConsumerCallInstruction& call,
            llvm::Module& module, llvm::Function* fallback_fn = nullptr);

    //! Append and validate dynamic integer operands used only by a fused string consumer.
    bool appendAOTStringConsumerOperands(
            const QoreIRStringConsumerCallInstruction& call,
            std::vector<llvm::Value*>& raw_args,
            std::vector<uint32_t>& raw_arg_ids, std::string& error);

    // Emit an imported typed collection operation. Returns nullptr when no
    // valid summary is available for the supplied fast-entry arguments.
    llvm::Value* emitAOTCollectionOp(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args, llvm::Module& module,
            const QoreIRCallDirectInstruction* fused_call = nullptr);

    // Emit a bounded affine composition over one exact size/length source.
    llvm::Value* emitAOTComposedInt(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args, llvm::Module& module);

    // Emit a bounded affine expression over one argument and one callee-context local.
    llvm::Value* emitAOTContextInt(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args, llvm::Value* callee_ctx,
            llvm::Module& module, llvm::Function* fallback_fn);

    // Emit a bounded affine expression over one argument and one global lvalue.
    llvm::Value* emitAOTGlobalInt(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args, llvm::Value* callee_ctx,
            llvm::Module& module, llvm::Function* fallback_fn);

    // Emit any importable callee summary through one shared dispatch path.
    llvm::Value* emitAOTImportedSummary(const BatchCalleeInfo& info,
            const std::vector<llvm::Value*>& native_args, llvm::Value* callee_ctx,
            llvm::Module& module, llvm::Function* fallback_fn,
            bool* proven_nothrow = nullptr);

    llvm::Value* emitAOTFixedHashRemap(const BatchCalleeInfo& info,
            llvm::Value* boxed_arg, int32_t slot, llvm::Module& module,
            llvm::Function* llvm_func, const QoreIRInstruction* inst);

    // Load an imported exact self getter in its declared scalar representation.
    llvm::Value* emitAOTSelfGetter(const BatchCalleeInfo& info,
            const AbstractQoreFunctionVariant* variant, llvm::Module& module,
            BatchCalleeReturnKind& return_kind);

    // Load an imported exact explicit-object getter in its declared scalar representation.
    llvm::Value* emitAOTObjectGetter(const BatchCalleeInfo& info,
            const AbstractQoreFunctionVariant* variant, int32_t slot,
            llvm::Value* base, bool rejects_nothing, llvm::Module& module,
            BatchCalleeReturnKind& return_kind);

    // Assign an imported exact-object member and return its declared scalar representation.
    llvm::Value* emitAOTObjectSetGet(const BatchCalleeInfo& info,
            const AbstractQoreFunctionVariant* variant, int32_t slot,
            llvm::Value* base, llvm::Value* args, int nargs,
            bool rejects_nothing, BatchCalleeReturnKind proven_return_kind,
            llvm::Module& module,
            BatchCalleeReturnKind& return_kind);

    BatchCalleeParamKind getFastEntryParamKind(const BatchCalleeInfo& info,
            unsigned index) const;
    bool fastEntryParamRejectsNothing(const BatchCalleeInfo& info,
            unsigned index) const;
    bool fastEntryArgumentKnownNotNothing(uint32_t value_id) const;
    BatchCalleeParamKind getFastEntryArgKind(const void* key) const;
    llvm::Value* getFastEntryCallArgument(const BatchCalleeInfo& info,
            unsigned index, const std::vector<llvm::Value*>& raw_args,
            const std::vector<uint32_t>& raw_arg_ids,
            const std::vector<llvm::Value*>& boxed_args,
            llvm::Module& module);
    bool fastEntryNativeArgsNeedNothingGuard(const BatchCalleeInfo& info,
            const std::vector<uint32_t>& raw_arg_ids) const;
    llvm::Constant* getNothingReturnValue() const;
    //! Emits the declared return type's conversion of a returned value, or of a missing return value when
    //! @p boxed is null, and returns the converted NaN-boxed value; the result is released by invoke cleanup
    llvm::Value* emitReturnTypeConversion(llvm::Module& module, llvm::Function* llvm_func,
            const QoreIRInstruction* inst, llvm::Value* boxed, const QoreTypeInfo* return_type);
    //! Converts a NaN-boxed return value to the fast-entry return ABI
    llvm::Value* getFastEntryReturnValue(llvm::Module& module, llvm::Value* boxed);
    bool selfRecursiveFastEntryArgsNeedNothingGuard(
            const std::vector<uint32_t>& raw_arg_ids) const;

    // Deferred PHI nodes to fixup after all blocks are lowered; ir_block is the IR
    // block containing the PHI so incoming edges can be resolved via edge_block_map
    struct PendingPhi {
        llvm::PHINode* node;
        const QoreIRPhiInstruction* inst;
        const QoreIRBasicBlock* ir_block;
        QoreIRPhiValueKind value_kind;
        //! the cleanup slot of a Phi of temporaries, which takes each incoming temporary's reference over at the end
        //! of its predecessor (see consumable_phis), or of a Phi in owning_phis; null for any other Phi
        llvm::AllocaInst* temp_cleanup = nullptr;
    };
    std::vector<PendingPhi> pending_phis;

    //! Phis of temporaries: QoreValue Phis whose every incoming value is used by the Phi only
    /** When every incoming value owns its reference in a cleanup slot (a temporary, such as the result of a call or of
        new), the Phi gets a cleanup slot of its own, which takes the reference over from the incoming value's slot at
        the end of each predecessor: the Phi is then a temporary as the incoming values are, which a call consumes as
        its argument as it consumes any temporary (f(new T(), c ? new T() : new T(), new T())).  Otherwise the
        incoming slots kept their references until the end of the statement, after the temporaries that the call
        consumed, which the AST interpreter releases last argument first when the call returns, as the callee's
        parameters.

        A loop header's Phi whose incoming values include the Phi itself (a path that keeps its value, such as the
        skip path of a select stage before a fold at the root of a fused pipeline) is one only when it can hold a
        temporary (see owning_phis): the Phi's own value is not a use of it, and stays in its slot on that edge.
    */
    std::unordered_set<uint32_t> consumable_phis;
    //! QoreValue Phis that hold a reference of their own in a cleanup slot, and that no call consumes
    /** A Phi that is not a Phi of temporaries gets a cleanup slot as one does, which takes over the incoming
        temporaries that it is the only user of (single_use_phi_incoming) and takes a reference of its own to its
        other incoming values (qore#5883), when it is:
        - a loop header's Phi among its own incoming values (the accumulator of a fold at the root of a fused pipeline
          with a select stage, which keeps its value for an element that the stage skips) unless all its values are
          constants: any other value can be released in the loop, such as an iterator's element, which the next
          IteratorNext releases, the value of a call or an operator (a string concatenation, a list), whose slot the
          next execution of the instruction replaces, or a value read from a variable, which code in the loop can
          assign.  It keeps its value in its
          slot on the edge back to the header; without the slot, it borrowed the element that the next IteratorNext
          released, or the value that the next element's operator replaced.  A Phi of temporaries among its own incoming values is one too, as its value can reach the header
          again, and no call consumes it
        - a loop header's Phi that is not a Phi of temporaries and that can hold such a value (the accumulator of a fold
          over a fused pipeline, whose value can be the value of a map stage, which the next IteratorNext releases, or
          of an operator, which the next element replaces)
        - a Phi whose only use is an incoming value of a Phi with a slot of its own and that has an incoming temporary
          that it is the only user of: the value of a fold over a fused pipeline on the edge that gives the
          accumulator, which is the first element or the value of the fold's expression.  The reference of its own
          that it takes to the first element is the one that the accumulator's Phi took to it otherwise, and the value
          of the expression, which it takes over, gave the accumulator a reference of its own as well
    */
    std::unordered_set<uint32_t> owning_phis;
    //! The Phis of owning_phis whose only use is an incoming value of a Phi in the block that their block goes on to:
    //! that Phi takes their slot over on every path, so the slot is empty when they get their next value, which then
    //! releases nothing (a fold's value on the edge that gives the accumulator, see the Phi fixup)
    //! (by Phi, with the Phi that uses it)
    std::unordered_map<uint32_t, uint32_t> emptied_phi_slots;
    //! The incoming values of QoreValue Phis that one Phi uses once, and no instruction outside the block that
    //! defines them (see consumable_phis)
    std::unordered_set<uint32_t> single_use_phi_incoming;

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
    //! Cached ptr-to-ptr for the thread's call site, the location of the call being made (QoreRcCallSiteHelper)
    llvm::Value* call_site_cache_ptr = nullptr;
    int last_runtime_line = -1;             //!< Last source line emitted for location tracking

    //! Returns the constant location that AOT code holds in its image for `loc`, one per distinct location of the
    //! module, or nullptr if `loc` has no line
    /** A location held by the image does not depend on the AOT context that the code runs with, which is not the
        code's own in a context-independent fast entry, and lives as long as the image, which is never unloaded.
    */
    llvm::Constant* getAotLocationConstant(const QoreProgramLocation* loc, llvm::Module& module);


    //! Emit a runtime_loc update if the instruction's source line changed
    void emitRuntimeLocationUpdate(const QoreIRInstruction* inst, llvm::Module& module);
    //! Sets the location of a call instruction as the thread's call site, for the frame of the called code
    /** As QoreRcCallSiteHelper: the frame of the called code takes the location, so that it is located at the call
        expression, as for an AST call; emitCallSiteClear() clears it after the call.
    */
    void emitCallSiteSet(const QoreIRInstruction* inst, llvm::Module& module);
    //! Clears the thread's call site after a call or when an exception leaves it; see emitCallSiteSet()
    void emitCallSiteClear();

    // Initialize types and helpers
    void initTypes();

    // Declare external runtime helper functions in the module
    void declareRuntimeHelpers(llvm::Module& module);

    // Lower a single instruction; returns false on unsupported opcode
    bool lowerInstruction(const QoreIRInstruction* inst, llvm::Function* llvm_func,
            llvm::Module& module, std::string& error);

    //! Lowers IteratorNextSelectors for the selector-segment loop of \a seg (qore#5711)
    /** Gives the value at the next index of the cursor, read from the list, or branches to the done target when the
        current selector has no index left; the element is held in an output slot as for the iterator
        (qore_rt_selector_iterator_next())
    */
    bool lowerSelectorSegmentNext(const QoreIRIteratorNextInstruction* iter_inst, const SelectorSegmentState& seg,
            llvm::Module& module, llvm::Function* llvm_func, std::string& error);

    //! Reject AOT codegen that would execute serialized expression trees.
    bool checkNoAotExecutableExprFallback(llvm::Function* llvm_func, std::string& error) const;

    //! Record the exact IR instruction that emitted a forbidden AOT expression fallback.
    void annotateAotExecutableExprFallback(llvm::Value* call, const QoreIRInstruction* inst) const;

    // Helpers to get/create LLVM values
    llvm::Value* getVal(uint32_t id, std::string& error);

    // NaN-boxing helpers: encode typed LLVM values into i64 QoreValue representation
    llvm::Value* boxInt(llvm::Value* int_val);
    //! Box a native double; the result owns a QoreBigFloatNode when boxFloatMayAllocate() is true
    //! and the double collides with a value tag (see QoreValue::rawDoubleCollidesWithTag()); without
    //! allow_branch, no block is created (for code inserted before an existing terminator)
    llvm::Value* boxFloat(llvm::Value* float_val, bool allow_branch = true);
    //! Returns false only for a double constant that boxFloat() encodes inline
    bool boxFloatMayAllocate(llvm::Value* float_val) const;
    //! Box a native double as a borrowed temp: a node allocated by boxFloat() is released at function exit
    llvm::Value* boxFloatTemp(llvm::Value* float_val, bool allow_branch = true);
    llvm::Value* boxBool(llvm::Value* bool_val);
    //! Box a native bool, double or int (an owned value, like boxInt()); nullptr for any other type
    llvm::Value* boxNative(llvm::Value* val);
    llvm::Value* boxNothing();

    // Unboxing helpers: extract typed values from i64 QoreValue
    llvm::Value* unboxInt(llvm::Value* qv);
    llvm::Value* unboxFloat(llvm::Value* qv);
    llvm::Value* unboxBool(llvm::Value* qv);

    // Phase 4: LLVM IR helper inlining helpers
    // Check if value has a node (inline NaN-boxing check) - returns bool (i1)
    llvm::Value* hasNodeInline(llvm::Value* qv);

    //! Emits the inline test that @a qv is an object that has not been deleted, with the layout of the running
    //! libqore (JIT only; QoreIROpcode::CheckCallTarget, issue #5784)
    llvm::Value* emitValidObjectTest(llvm::Value* qv, const QoreJITObjectLayout& layout, llvm::Function* llvm_func);

    // Inline qore_rt_ref - reference count a value if it's a node
    // Returns the value unchanged if not a node, or qore_rt_refself result if node
    llvm::Value* emitHelperRef(llvm::Module& module, llvm::Value* val);

    // Ensure a value is a native int64_t for typed int operations.
    // Handles: native i64 (pass through), NaN-boxed INT48 or big int (runtime conversion).
    llvm::Value* ensureIntType(llvm::Value* val, uint32_t value_id);

    llvm::Value* getCachedStableConversion(
        uint32_t value_id, const NativeConversionCache& cache) const;
    void cacheStableConversion(
        uint32_t value_id, llvm::Value* value, NativeConversionCache& cache);

    // Inline fast-path version of ensureIntType for typed int ops (NOT in PHI fixup context).
    // Uses LLVM branches to check INT48 tag and sign-extend inline, falling back to runtime
    // only for QoreBigIntNode values.  ~2x faster than ensureIntType for common INT48 values.
    llvm::Value* ensureIntTypeInline(llvm::Value* val, uint32_t value_id);
    //! Converts a boxed value to a native int: an integer in the boxed value inline, any other value with
    //! qore_rt_to_int()
    llvm::Value* emitBoxedToIntInline(llvm::Value* val);
    //! Returns the type path argument of the return type of a method that AOT-compiled code reads a member for
    //! directly (a getter, an assignment or a compound assignment that returns the member), or a null pointer
    //! when the return type accepts no value
    llvm::Value* getAOTGetterReturnTypePathArg(const AbstractQoreFunctionVariant* variant, bool rejects_nothing);

    // Preserve timeout's relative-date-to-milliseconds assignment semantics.
    llvm::Value* ensureTimeoutTypeInline(llvm::Value* val, uint32_t value_id);

    // Inline fast-path version of boxInt for StoreLocal (NOT in PHI fixup/boxValue context).
    // Uses LLVM branches for INT48 range check, falling back to runtime for big ints.
    llvm::Value* boxIntInline(llvm::Value* int_val);

    // Ensure a value is a native double for float operations
    // Handles NaN-boxed values (int or float), native i64, and native doubles
    llvm::Value* ensureFloatType(llvm::Value* val, uint32_t value_id, llvm::Module& module);

    // Inline fast-path version for NaN-boxed floats, with runtime conversion for
    // NOTHING and other representations.
    llvm::Value* ensureFloatTypeInline(llvm::Value* val, uint32_t value_id, llvm::Module& module);

    // Get a declared runtime helper function
    llvm::FunctionCallee getHelper(llvm::Module& module, const char* name, llvm::FunctionType* ft);

    // Return either a JIT-safe QoreTypeInfo* constant or an AOT-safe type-path string.
    llvm::Value* getTypeInfoPointerArg(const QoreTypeInfo* ti);
    llvm::Value* getTypePathArg(const QoreTypeInfo* ti);
    const QoreTypeInfo* specializeType(const QoreTypeInfo* ti) const;

    // Collect all unique LocalVar* pointers from the IR function
    void collectLocals(const QoreIRFunction& func);

    //! Records in QoreIRFunction::aot_native_local_flags the locals that the AOT-compiled standard entry of the
    //! function keeps in native storage, once the IR-only sets of the lowering are final (issue #5890)
    void recordAOTNativeLocals(const QoreIRFunction& func);

    // Emit qore_rt_instantiate_local calls for all function locals at current insert point
    void emitLocalInstantiation(llvm::Module& module);

    //! Emits the instantiation of a closure-use local that an assignment writes, in the current frame
    /** Idempotent at runtime: the helper does nothing when the variable already exists in the current frame.  An
        assignment can be the first write of a block-scoped closure-use local (such as a foreach variable), which is
        created lazily; without it, the variable would be resolved by name in a caller's frame.
    */
    void emitAssignedClosureLocalInstantiation(const LocalVar* root_local, llvm::Module& module);

    // Pre-create LLVM allocas for all function locals so reloadAllLocalsFromRuntime can find them
    void preCreateLocalAllocas(llvm::Module& module, llvm::Function* llvm_func);

    // Emit qore_rt_uninstantiate_local calls for all function locals at current insert point
    void emitLocalUninstantiation(llvm::Module& module);
    void emitLocalUninstantiationIntern(llvm::Module& module);

    //! The boxed slots emitPreinstantiatedCleanup() releases, in emission order.
    /** Pre-instantiated entry cleanup slots, then fast-entry parameter slots, then
        the IR-only locals that own their value directly.
    */
    std::vector<llvm::AllocaInst*> getBoxedExitCleanupSlots() const;

    //! An entry of the release of a function's locals at its exit: the native cache of a local, or a local on the
    //! runtime stack whose value is released (see getExitReleaseSequence())
    struct ExitReleaseEntry {
        llvm::AllocaInst* alloca = nullptr;
        LocalVar* var = nullptr;
        //! the AOT slot of \a var in AOT code, whose LocalVar is known only at runtime (-1: none)
        int32_t aot_slot = -1;
        //! true if \a var is a closure-bound variable that the frame instantiated, which is popped from the closure
        //! stack; false for one the caller instantiated, which only loses its value, and for a runtime-stack local
        bool pop_closure = false;
        //! true if \a alloca holds the iterator of a foreach statement (released with qore_rt_iterator_cleanup())
        bool iterator = false;
    };

    //! The locals released at the exit of the function, in declaration order (they are released last first)
    /** The boxed slots of getBoxedExitCleanupSlots(), and, unless \a runtime_locals is false, the closure-bound body
        locals and (except in AOT code) the body locals on the runtime stack that the frame instantiated, between the
        IR-only locals declared before and after them, so that a function left by an exception releases all of its
        locals last declared first, as the AST interpreter does, whichever storage they have; with \a cache_allocas, a
        runtime local is followed by the allocas of its native cache that hold references (released before the
        variable's value)
    */
    std::vector<ExitReleaseEntry> getExitReleaseSequence(bool runtime_locals, bool cache_allocas) const;

    //! Creates the alloca of the reference that a local's native cache replaced at its last reload, if it has none
    //! yet (see retainLocalCacheValueIntern()), so that a release emitted before it is created releases it too
    void ensureLocalReloadDeferred(const void* key, llvm::Function* llvm_func);

    //! Route boxed exit cleanup through qore_rt_cleanup_run_allocas() when the
    //! inline per-slot triples would need (slots x exit_edges) PHIs.
    /** @param llvm_func the function being finalized
        @param exit_edges the number of edges reaching the shared cleanup block

        @return true if the array was prepared; emitPreinstantiatedCleanup() then
        emits a single runtime call in place of the per-slot triples
    */
    bool prepareBoxedExitCleanupArray(llvm::Function* llvm_func, size_t exit_edges);

    //! Route iterator cleanup through qore_rt_iterator_cleanup_allocas() when the
    //! inline per-iterator sequence would need (iterators x exits) PHIs.
    /** @param llvm_func the function being finalized
        @param exits the number of return/resume sites that repeat the sequence

        @return true if the array was prepared; emitIteratorCleanup() then emits a
        single runtime call in place of the per-iterator sequence
    */
    bool prepareIteratorCleanupArray(llvm::Function* llvm_func, size_t exits);

    //! PHI-operand budget above which local cleanup switches to the array form.
    /** Overridable with QORE_JIT_CLEANUP_PHI_BUDGET; 0 disables the array form.
    */
    static size_t getCleanupPhiBudget();

    //! Tracked-local count at which the temp-boundary reload-chain flush switches
    //! from inline branches to runtime helper calls.
    /** The inline form costs two basic blocks per tracked local at every temp
        boundary.  Overridable with QORE_JIT_RELOAD_CHAIN_CALL_THRESHOLD; 0 keeps
        the inline form for every function.
    */
    static size_t getReloadChainCallThreshold();

    // Emit qore_rt_exec_on_block_exit call to execute registered on_block_exit handlers
    void emitOnBlockExitExec(llvm::Module& module);

    //! Branches to a new block that runs only when handlers were registered after \a saved_count, and continues there
    /** @return the block where the code continues after the handlers; endPendingHandlerRun() branches to it */
    llvm::BasicBlock* beginPendingHandlerRun(llvm::Module& module, llvm::Value* saved_count);

    //! Ends the block started by beginPendingHandlerRun() and continues in \a done_bb
    void endPendingHandlerRun(llvm::BasicBlock* done_bb);

    // Emit qore_rt_catch_unwind(catch_depth_saved, xsink) at the current insert
    // point only when specialized tracking proves a runtime catch scope remains
    // active; no-op for functions without catch blocks.
    void emitCatchUnwind(llvm::Module& module);

    // Update specialized runtime catch-scope state around CatchException,
    // CatchCleanup, and Rethrow lowering.
    void emitCatchScopeEnter(llvm::Module& module);
    void emitCatchScopeExit(unsigned count);

    // Publish current LLVM local allocas to the runtime local stack before
    // deferred handlers execute through AST/IR and read parent locals.
    void syncLocalsToRuntimeForHandlers(llvm::Module& module);

    //! Publishes the locals to the runtime local stack before handlers run out of line (see the definition)
    void publishLocalsForHandlers(llvm::Module& module);

    //! Records the call that runs handlers out of line, around which finalizeHandlerSites() releases and reloads the
    //! reload storage of the locals that the handlers can assign
    /** @param reload true if the locals whose storage is released are reloaded right after the call; false if the
        caller reloads every local itself
    */
    void recordHandlerSite(llvm::CallInst* call, bool reload);

    //! Emits the release before and the reload after each call recorded by recordHandlerSite(), once the function is
    //! lowered and its reload storage is known
    void finalizeHandlerSites(llvm::Module& module, llvm::Function* llvm_func);

    //! Releases the reload trackers and deferred slots of the locals that handlers can assign, last declared first,
    //! at the builder's insertion point, and returns the locals released, last declared first
    std::vector<const void*> releaseLocalReloadStorage(llvm::Module& module);

    //! a call that runs handlers out of line (see recordHandlerSite())
    struct HandlerSite {
        llvm::CallInst* call;
        bool reload;
    };
    std::vector<HandlerSite> handler_sites;

    //! true if the reload storage of the local is released for handlers run out of line (see handler_locals)
    bool releasesReloadStorageForHandlers(const void* key) const;

    //! the locals that the function's handlers reference (QoreIRFunction::collectHandlerLocals()), whose reload
    //! storage is released for handlers run out of line; when handler_locals_known is false, every local's
    std::unordered_set<const void*> handler_locals;
    bool handler_locals_known = false;

    //! Returns the keys of a map of locals in reverse declaration order (their IR slot ids, highest first)
    template <typename Map>
    std::vector<const void*> getLocalsInReverseDeclarationOrder(const Map& map) const;

    // Emit qore_rt_decref calls for pre-instantiated local entry loads
    //! @param exception_exit true for an exit by an exception, which releases the body locals on the runtime stack
    //! with the IR-only locals, in declaration order (see getExitReleaseSequence())
    void emitPreinstantiatedCleanup(llvm::Module& module, bool exception_exit = false);

    // Conditionally re-instantiate pre-instantiated closure-use block-scoped locals
    // that were popped mid-execution (e.g. loop-body closure captures).  Called at
    // every function exit point so evalTiered's cleanup can pop exactly one CVV per var.
    void emitPreInstClosureReInstantiation(llvm::Module& module);

    // Emit qore_rt_decref calls for tracked runtime call results
    void emitInvokeCleanup(llvm::Module& module);

    // Releases the temporaries of the statements still running when the function exits - the tracked call
    // results that are not persistent cleanups (local reload trackers) and the pending SSA cleanups - and the
    // iterators of the foreach statements left, before the on_block_exit handlers of the function's blocks run, as
    // the AST interpreter releases them when the statements unwind; the persistent cleanups follow in
    // emitInvokeCleanup()
    //! @param exception_exit true for an exit by an exception, whose release of the locals releases the iterators
    //! that have a place among them (positioned_iterator_allocas), and which releases the other iterators (those of
    //! functional operators) first, before the temporaries, as the AST interpreter unwinds the operators
    void emitExitStatementCleanup(llvm::Module& module, bool pending_ssa, bool exception_exit = false);

    // Returns the block that an exception raised by \a inst branches to on its way to the in-frame handler
    // \a handler: a block that first releases the temporaries of the temp scopes that the exception leaves - the
    // tracked call results registered since the mark of the first temp scope opened after the handler's
    // (QoreIRBasicBlock::getHandlerTempScopeId()), or of the instruction's own scope - as the AST interpreter releases
    // them when the expressions and statements unwind and the IR interpreter drains them before the handler runs; or
    // \a handler itself if there are none.  Pending SSA cleanups are promoted to cleanup allocas first.
    llvm::BasicBlock* getExceptionEdgeTarget(llvm::Module& module, llvm::Function* llvm_func,
            const QoreIRInstruction* inst, llvm::BasicBlock* handler);

    // Returns the mark of the temp scope of \a inst if an exception that \a inst raises outside a try block can
    // release the statement's temporaries in their own order on its way to the error return; nullptr if the
    // instruction has no temp scope, its statement has pending SSA cleanups or too many temporaries
    const TempCleanupMark* getOrderedErrorReturnMark(const QoreIRInstruction* inst) const;

    // Returns the marks of the temp scopes open at \a inst, innermost last: temp_cleanup_marks if the mark of the
    // instruction's temp scope is open, or those open when it was closed (closed_temp_cleanup_marks) if its block is
    // lowered after the end of its scope; temp_cleanup_marks if neither has it
    const std::vector<TempCleanupMark>& getTempScopeMarks(const QoreIRInstruction* inst) const;

    // Completes the edges of the exception checks outside a try block (ordered_error_return_sites) once every cleanup
    // alloca of the function is known: an edge that releases the iterators of the statement's functional operators
    // and then its temporaries in the order in which the AST interpreter unwinds them, on the way to the error
    // return, where the error return's own release gives another order.  The temporaries are those registered before
    // the check and, if the check is in a loop, those registered after it in the statement that the loop stores and
    // does not release before it comes back to the check: a value of the previous element that such a temporary
    // still holds was released before the others (qore#5884)
    void finalizeOrderedErrorReturnEdges(llvm::Module& module, llvm::Function* llvm_func);

    // Emits the release of the iterators of functional operators (those without a place among the locals) of
    // iterator_cleanup_allocas from \a start to \a end, last created first: an exception that leaves the function
    // releases them before the statement's temporaries, as the AST interpreter unwinds the operators
    void emitOperatorIteratorRelease(llvm::Module& module, size_t start, size_t end);

    // Emits, at the insertion point, the release of \a temps (from orderExceptionTemps()), in their order: a
    // throw or a synthetic rethrow that leaves the function releases them so before the statement's exit cleanup,
    // as an exception check does on its edge
    void emitOrderedErrorReturnTemps(llvm::Module& module, const std::vector<llvm::Value*>& temps);

    // Gives the cleanup allocas of a statement's temporaries, registered since \a start, in the order of their release
    // when an exception leaves the statement (see qore_ir_order_exception_release()); returns true if it differs from
    // the reverse of the registration order.  \a observable_only leaves out the values whose release cannot run a
    // destructor
    bool orderExceptionTemps(size_t start, bool observable_only, std::vector<llvm::Value*>& temps,
            size_t* receivers = nullptr);
    //! Emits, at the insertion point, the release of \a temps from \a begin to \a end in their order: one call with a
    //! table of their slots in invoke_cleanup_array for a long sequence, as each release inline would make the code of
    //! a large statement large (qore#5962)
    void emitTempReleaseSequence(llvm::Module& module, const std::vector<llvm::Value*>& temps, size_t begin,
            size_t end);
    //! Emits a block that releases \a temps from \a begin to \a end in their order and continues with \a next
    llvm::BasicBlock* emitExceptionTempRelease(llvm::Module& module, llvm::Function* llvm_func,
            const std::vector<llvm::Value*>& temps, size_t begin, size_t end, llvm::BasicBlock* next);
    //! Returns the block of \a chain that releases \a temps from \a begin to \a end and continues with \a tail,
    //! continuing the chain (see ExceptionEdges)
    llvm::BasicBlock* getExceptionEdgeChainBlock(llvm::Module& module, llvm::Function* llvm_func,
            ExceptionEdgeChain& chain, const std::vector<llvm::Value*>& temps, size_t begin, size_t end,
            llvm::BasicBlock* tail);
    //! the same for the cleanup allocas of \a indices (positions in invoke_result_allocas, in increasing order)
    bool orderExceptionTemps(const std::vector<size_t>& indices, bool observable_only,
            std::vector<llvm::Value*>& temps, size_t* receivers = nullptr);

    // Emit statement/condition-boundary temp cleanup for values tracked since
    // the latest PushTempMark.
    //! Releases the temps registered since the mark of \a scope_id; \a statement_end also releases the local
    //! reload state that is kept until the end of a statement (false for DiscardTempsKeep, which closes the scope
    //! of an operand in the middle of a statement)
    void emitDiscardTemps(llvm::Module& module, uint32_t scope_id = 0, bool statement_end = true);

    // Register an alloca-backed cleanup slot and, for large functions, add its
    // address to the entry-block cleanup pointer array used by
    // qore_rt_cleanup_run_allocas().
    void registerInvokeCleanupAlloca(llvm::Value* alloca_ptr);
    void registerPersistentCleanupAlloca(llvm::Value* alloca_ptr);

    //! Sizes the invoke cleanup pointer array to the slots actually registered
    /** Erases the array and its registration stores when no exit reads it.  Must be called
        after every emitInvokeCleanup() call for the function has been emitted.
    */
    void finalizeInvokeCleanupArray();

    //! Stores the address of a cleanup slot at the next index of an entry-block cleanup pointer array
    void storeCleanupSlotAddress(llvm::AllocaInst* array, unsigned idx, llvm::Value* alloca_ptr);

    //! Sizes a cleanup pointer array to the slots registered, or erases it with its registration stores when no
    //! exit reads it
    void finalizeCleanupSlotArray(llvm::AllocaInst* array, unsigned count, unsigned capacity, const char* what);

    // Conservative upper bound for the cleanup pointer array.  Overflow falls
    // back to the expanded cleanup path, so the estimate only affects whether
    // the compact helper path is available; finalizeInvokeCleanupArray() sizes
    // the emitted array to the slots actually used.
    unsigned estimateInvokeCleanupArrayCapacity(const QoreIRFunction& func) const;

    //! @return true if @p variant is the exact variant this function is being lowered from
    /** Every AOT self-recursion path below re-enters the caller's own compiled body, so the
        callee must be this exact variant.  Function identity is not enough: all overloads
        of a function share one QoreFunction and one FunctionEntry, and a sibling overload
        dispatched through those paths runs this body with the sibling's arguments.
        QoreIRBuilder::createCallDirect() applies the same test when it sets
        `is_self_recursive`; re-applying it at emission also covers the FunctionEntry-keyed
        Invoke path, which derives self-recursion on its own, and neutralizes a stale flag
        restored from an AOT binary written before that test compared variants. */
    bool isSelfRecursiveVariant(const AbstractQoreFunctionVariant* variant) const;

    //! Emits the release of the active iterators (qore_rt_iterator_cleanup())
    //! @param skip_positioned true to leave the iterators that the release of the locals releases at their places
    //! (see positionIterators()); the array form then releases those of iterator_cleanup_unpositioned_array
    void emitIteratorCleanup(llvm::Module& module, bool skip_positioned = false);

    //! Registers the cleanup alloca of the iterator that \a inst creates in positioned_iterator_allocas if the IR
    //! gives it a place among the local slots
    void registerPositionedIterator(const QoreIRInstruction* inst, llvm::Value* iter_alloca);

    //! Returns true if an exception exit releases the iterators of positioned_iterator_allocas at their places among
    //! the locals, in the inline form and in the array forms of the exit release and of the iterator release alike
    bool positionIterators() const {
        return !positioned_iterator_allocas.empty();
    }

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

    //! Releases the allocas registered for a local after an UninstantiateLocal of it was lowered (see
    //! DeferredLocalRelease), before the release of the local's value there
    void finalizeDeferredLocalReleases(llvm::Module& module);

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
    //! With \a defer_to (see getInvokeResultDeferBlock()), the result replaces the value of the alloca at the start of
    //! that block, the normal continuation of an IR invoke, rather than where it is emitted; returns true if it does,
    //! and the exception edge of the invoke must then release the result (getInvokeResultDiscardEdge())
    bool trackResultForCleanup(llvm::Value* result, uint32_t result_id,
            llvm::Function* llvm_func, llvm::BasicBlock* defer_to = nullptr);

    //! Returns the LLVM block of \a normal_target, the normal continuation of an IR invoke, if the cleanup slot of the
    //! invoke's result can be set there (see trackResultForCleanup()), otherwise nullptr
    /** The IR interpreter does not set the value of an instruction that raises an exception: the value that its
        previous execution gave (an element of a functional operator's loop) stays in its slot, and the statement
        releases it with its other temporaries after the operator's values, as the AST interpreter does (qore#5830).
        Native code therefore replaces the slot's value on the normal edge only, at the start of the continuation, which
        costs the normal path nothing.  The block must not be lowered yet, start with no PHI node and have the invoke's
        block as its only predecessor, which dominates it.
    */
    llvm::BasicBlock* getInvokeResultDeferBlock(const QoreIRBasicBlock* normal_target,
        const QoreIRBasicBlock* exception_target);

    //! Returns a block that releases \a result, the value of an IR invoke that raised an exception, and branches to
    //! \a target; for an invoke whose result trackResultForCleanup() deferred to its normal continuation
    llvm::BasicBlock* getInvokeResultDiscardEdge(llvm::Module& module, llvm::Function* llvm_func, llvm::Value* result,
            llvm::BasicBlock* target);

    //! the number of predecessors of each block of the function being lowered, over its normal and exception edges,
    //! computed when first needed (getInvokeResultDeferBlock())
    std::unordered_map<const QoreIRBasicBlock*, uint32_t> ir_block_predecessor_counts;
    bool ir_block_predecessor_counts_valid = false;

    // Track a boxed temp that may own a heap node (boxInt()/boxFloat() result) for release at
    // function exit; each store releases the previous value held by the same cleanup slot.
    void trackBoxedTempCleanup(llvm::Value* boxed);

    // Box any typed LLVM value to NaN-boxed i64, handling already-boxed values
    llvm::Value* boxValue(llvm::Value* val, uint32_t id,
        bool allow_dynamic_inline = true);

    // Emit exception check: if xsink has exception, branch to exception_target
    // unless the caller requests the function-level error path.
    void emitExceptionCheck(llvm::Module& module, llvm::Function* llvm_func,
            const QoreIRInstruction* inst, bool force_function_error = false);

    // Emit the in-place debug-step hook (issue #5352) at a DebugBlock (synthetic=true,
    // block-entry → dbgSyntheticBlockStep) or DebugStatement (synthetic=false,
    // per-statement → dbgStep) instruction.  Gated by an inline attached-debugger check so
    // non-debug JIT execution pays at most one predictable not-taken branch.  No-op in AOT mode.
    void emitDebugStepHook(llvm::Module& module, llvm::Function* llvm_func,
            const QoreIRInstruction* inst, bool synthetic);

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
    // self.method shape has a dedicated helper (qore_rt_background_self_call_aot_v2);
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

    //! Emits the helper of a background expression and the call starting the thread with it
    /** The helper (QoreIRBackgroundInstruction::helper_ir) is lowered to a native function in the same module, named
        after the function that starts the thread (<function>_bg_<n>), and shares its AOT context in AOT mode.

        @param inst the background instruction, with QoreIRBackgroundKind::Helper
        @param result the value of the instruction (the TID of the new thread)
        @param error the error, if the helper cannot be lowered

        @return true if the code was emitted
    */
    bool emitBackgroundHelper(const QoreIRBackgroundInstruction* inst, llvm::Module& module,
            llvm::Function* llvm_func, llvm::Value** result, std::string& error);

    //! Emits a call that native code makes directly to a native entry of user code, with the frame of the call
    /** A native entry called directly (a fast entry) pushes no call stack frame; the caller pushes it around the
        call (qore_rt_push_native_call_frame()) in storage of its own frame, and pops it when the call returns, with
        or without an exception (native entries report exceptions by returning), before the exception is checked.
        When the callee is a plain function called from code that can run with an object or a class
        (nativeCallNeedsFunctionContext()), the call also switches to the function's context
        (emitPushNativeFunctionContext()).

        @param fn the native entry
        @param args the arguments of the call
        @param callee the variant called, which names the frame
    */
    llvm::CallInst* emitNativeCall(llvm::FunctionCallee fn, llvm::ArrayRef<llvm::Value*> args,
            const AbstractQoreFunctionVariant* callee, llvm::Module& module, llvm::Function* llvm_func,
            const llvm::Twine& name = "");

    //! Emits a call that AOT-compiled code makes directly to a native entry of user code, with its frame
    /** As emitNativeCall(); the callee is the call target of \a target_slot in this function's context, and the
        frame is located at \a inst's location (qore_rt_push_native_call_frame_aot()).
    */
    llvm::CallInst* emitNativeCallAOT(llvm::FunctionCallee fn, llvm::ArrayRef<llvm::Value*> args,
            int32_t target_slot, const QoreIRInstruction* inst, llvm::Module& module, llvm::Function* llvm_func,
            const llvm::Twine& name = "");

    //! Emits the push of the frame of a direct native call in JIT code; see emitNativeCall()
    void emitPushNativeCallFrame(const AbstractQoreFunctionVariant* callee, llvm::Module& module,
            llvm::Function* llvm_func);
    //! Emits the push of the frame of a direct native call in AOT code; see emitNativeCallAOT()
    /** @param inline_body true if the callee runs inline, at its body's location
        (qore_rt_push_native_inline_call_frame_aot())
    */
    llvm::CallInst* emitPushNativeCallFrameAOT(int32_t target_slot, const QoreIRInstruction* inst,
            llvm::Module& module, llvm::Function* llvm_func, bool inline_body = false);
    //! Emits the push of the frame of a direct native call without a call target in AOT code, naming the callee
    /** @param callee_loc the location of the callee's body if the callee runs inline, or a null pointer
    */
    llvm::CallInst* emitPushNativeCallFrameAOTNamed(const std::string& call_name, const QoreIRInstruction* inst,
            llvm::Module& module, llvm::Function* llvm_func, llvm::Constant* callee_loc);
    //! Returns the location of a call that AOT code makes, for the frame of the call: the constant location of the
    //! instruction, or a null pointer
    llvm::Constant* getAotCallLocation(const QoreIRInstruction* inst, llvm::Module& module);
    //! Emits the pop of the frame pushed by emitPushNativeCallFrame() or emitPushNativeCallFrameAOT()
    void emitPopNativeCallFrame(llvm::Module& module);
    //! Returns true if a direct native call of \a callee from the function being lowered must switch to the
    //! callee's object and class context
    /** A call of user code runs a plain function's body without an object or a class (UserVariantBase::eval()).
        The function being lowered runs in that context when it is a plain function, as each call path into its body
        establishes it; methods, closures (which run in the context they were created in) and top-level code can run
        with an object or a class.  The callee of a closure runs in the caller's context, as it was created there; AOT
        code calls the fast entries of methods with the context that they establish, and JIT batches take no method
        callees.
    */
    bool nativeCallNeedsFunctionContext(const AbstractQoreFunctionVariant* callee) const;
    //! Emits the switch to the context of a plain function for a direct call of its fast entry
    /** (qore_rt_push_native_function_context_v1()); emitPopNativeFunctionContext() restores the caller's context
        after the call returns, with or without an exception.
        @return the call of the switch, which the caller removes if the call is replaced by a summary of the body
        that does not observe the context
    */
    llvm::CallInst* emitPushNativeFunctionContext(llvm::Module& module, llvm::Function* llvm_func);
    //! Emits the restoration of the context replaced by emitPushNativeFunctionContext()
    void emitPopNativeFunctionContext(llvm::Module& module);

    //! The instructions that push the frame of an AOT call whose callee may run inline, as a body summary
    struct AotInlineCallFrame {
        llvm::Instruction* push = nullptr;
    };
    //! Emits the push of the frame of an AOT call whose callee may run inline, which makes the location of the
    //! callee's body the runtime location, where the inline code runs
    /** @param target_slot the expression slot of the call, whose call target names the frame and gives the location
        of the callee's body (UserVariantBase::getAOTBodyLocation()), or -1 if the call has none
        @param callee the callee, an entry of batch_callees

        A call without a call target, or one made by a function that may run with the context of its caller
        (mayRunWithCallerContext()), whose context does not have the call's call target, has a frame named at compile
        time, located at the body location of a callee of this compilation (BatchCalleeInfo::body_loc_file).
    */
    AotInlineCallFrame emitPushAotInlineCallFrame(int32_t target_slot, const BatchCalleeInfo& callee,
            const QoreIRInstruction* inst, llvm::Module& module, llvm::Function* llvm_func);
    //! Returns true if the function being lowered may run with the AOT context of its caller: a fast entry that is
    //! context-independent or whose callee information is unknown
    bool mayRunWithCallerContext() const;
    //! Returns the variant of an entry of batch_callees, or nullptr if it is not one
    const AbstractQoreFunctionVariant* getBatchCalleeVariant(const BatchCalleeInfo& callee);
    //! the variants of the entries of batch_callees, by entry, built on first use (getBatchCalleeVariant())
    std::unordered_map<const BatchCalleeInfo*, const AbstractQoreFunctionVariant*> batch_callee_variants;
    //! Pushes the frame of an AOT call of a method whose callee may run inline, as emitPushAotInlineCallFrame(), for
    //! the call target of the call's expression, if it has one
    AotInlineCallFrame emitPushAotInlineMethodCallFrame(QoreValue expr, const BatchCalleeInfo& callee,
            const QoreIRInstruction* inst, llvm::Module& module, llvm::Function* llvm_func);
    //! Ends the frame pushed by emitPushAotInlineCallFrame()
    /** @param keep true if the call is made or its inline code may raise an exception: the frame is popped;
        otherwise the push is removed, as nothing can see the frame of inline code that raises no exception (it makes
        no calls and cannot block)
    */
    void endAotInlineCallFrame(AotInlineCallFrame& frame, bool keep, llvm::Module& module);
    //! Returns the AOT expression slot of a direct function call, whose call target is the callee
    int32_t getCallDirectExprSlot(const QoreIRCallDirectInstruction& call);
    //! Creates the storage of the frames of the native calls of the function being lowered, once
    void ensureNativeCallFrameStorage(llvm::Module& module, llvm::Function* llvm_func);

    //! The storage of the frames of the native calls of the function being lowered, created by the first one
    /** One is enough: a native call's frame is popped before the function makes another call. */
    llvm::AllocaInst* native_call_frame_storage = nullptr;
    //! The current thread's data for the frames of the native calls, got once in the entry block
    llvm::Value* native_call_thread_data = nullptr;
    //! The storage of the context of the plain functions whose fast entries the function being lowered calls
    //! directly, created by the first call that switches to it (emitPushNativeFunctionContext())
    /** One is enough, as for native_call_frame_storage */
    llvm::AllocaInst* native_function_context_storage = nullptr;

    //! The number of background helpers emitted for the function being lowered
    unsigned bg_helper_count = 0;
    //! The number of on_block_exit handlers emitted for the function being lowered
    unsigned obe_handler_count = 0;
    //! The native functions emitted for the on_block_exit handlers of the function being lowered, by handler IR
    std::unordered_map<const QoreIRFunction*, llvm::Function*> obe_handler_fns;

    //! Lowers an IR function that the function being lowered uses to a native function of its own in the module
    /** Used for the helpers of background expressions and the bodies of on_block_exit handlers.  The nested function
        takes the settings of this lowering; in AOT mode it shares the AOT context of the function being lowered: its
        slot map and its location table, whose entries for the nested function are appended to this lowering's.

        @param func the IR function to lower
        @param name the name of the native function
        @param module the module to emit it in
        @param error the error, if it cannot be lowered

        @return the native function, or nullptr if an error was raised
    */
    llvm::Function* lowerNestedFunction(const QoreIRFunction& func, const std::string& name, llvm::Module& module,
            std::string& error);

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

    // Stores a new reference to value in the local's alloca and reload tracker without
    // checking whether the local can be reloaded; the tracker's previous value is released
    // one cycle later, so SSA values still borrowing it stay valid.
    void retainLocalCacheValueIntern(const void* key, llvm::Value* local_alloca, llvm::Value* value,
            llvm::Module& module, llvm::Function* llvm_func);

    // Clear the reload tracker for a specific local, releasing the +1 reference
    // held from a previous reloadLocalFromRuntime() call.  Called before lvalue
    // compound operations (+=, -=, etc.) so that the container's refcount drops to 1,
    // enabling copy-on-write to skip the copy (in-place modification).  The tracker is created when it does not
    // exist yet and the local can be reloaded (see canReloadLocalFromRuntime() for honor_reload_exempt), so that the
    // clear is emitted for a later execution of the same code (a loop) as well.
    void clearLocalReloadTracker(const void* key, llvm::Module& module, llvm::Function* llvm_func,
            bool honor_reload_exempt = true);

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

    // A dot-eval call whose base is the strong reference that a load of a weak
    // local took releases it at its last use; EH invoke lowerers create a
    // terminator before lowerFunction's post-instruction cleanup can emit IR, so
    // it is released at the call boundary.  Any other base is a temporary that
    // lives until the end of the statement (DiscardTemps), as in the AST.
    void releaseDotEvalBaseIfCurrentUseIsLast(const QoreIRInstruction* inst,
            llvm::Module& module);

    // Releases the temporary arguments of a call that the call consumes - their last use, in the block that
    // defines them - last argument first, as the AST interpreter releases the parameters that took them over when
    // the call returns; \a use_pending
    // is true when the uses of the call's operands have not been consumed yet (in an invoke lowerer).  Returns true
    // if any was released, as releasing can raise an exception
    bool releaseConsumedCallArgs(const QoreIRInstruction* inst, llvm::Module& module, bool use_pending);

    // Build an entry-block alloca'd array of NaN-boxed args from operands[arg_start..].
    // Sets args_array and nargs.  Returns false on error.
    // If nargs == 0, args_array is set to a null pointer.
    bool buildArgsArray(const QoreIRInstruction* inst, int arg_start,
            llvm::Function* llvm_func, llvm::Value*& args_array, int& nargs,
            std::string& error);

    // Emit the assigned branch of an argument-taking string pseudo-method
    // whose statically typed operands may still contain NOTHING.  Leaves the
    // builder on the generic fallback block when a fast path was emitted.
    llvm::Value* emitGuardedStringPseudoFastPath(const QoreIRInstruction* inst,
            QoreIRIntrinsic intrinsic, bool pseudo,
            bool base_known_string, bool base_assigned_string,
            bool arg0_known_string, bool arg0_assigned_string,
            bool arg0_assigned_int, bool arg1_assigned_int,
            llvm::Value* base_boxed, llvm::Module& module,
            llvm::Function* llvm_func, llvm::BasicBlock*& fast_end,
            llvm::BasicBlock*& merge_block, bool& result_needs_cleanup,
            std::string& error);

    // Collect fast-entry arguments without eagerly boxing native values.  When
    // a runtime fallback is possible, args_array is allocated but native slots
    // are populated only on the fallback edge.
    bool buildAotFastEntryArgs(const QoreIRInstruction* inst, int arg_start,
            llvm::Function* llvm_func, const BatchCalleeInfo& callee_info,
            bool needs_fallback_array, llvm::Value*& args_array, int& nargs,
            std::vector<llvm::Value*>& raw_args,
            std::vector<uint32_t>& raw_arg_ids,
            std::vector<llvm::Value*>& boxed_args, std::string& error,
            int operand_count = -1);

    // Build an entry-block alloca'd array of cleanup slot pointers for
    // operands[arg_start..].  The runtime uses this to clear consumed
    // call-argument temporaries after callee parameter instantiation.
    llvm::Value* buildArgCleanupArray(const QoreIRInstruction* inst, int arg_start,
            llvm::Function* llvm_func, int nargs, bool& has_cleanup,
            int borrowed_prefix = 0);

    // Clear ownership slots after a direct fast-entry call.
    void emitArgCleanupClear(llvm::Module& module,
            llvm::Value* arg_cleanups, int nargs,
            bool callee_proven_nothrow);

    // Returns true if a local can be lazily reloaded from the runtime stack.
    bool canReloadLocalFromRuntime(const void* key, bool honor_reload_exempt = true) const;

    // Per-function epoch helpers for lazy local-cache invalidation.
    llvm::AllocaInst* getOrCreateLocalReloadEpoch(llvm::Function* llvm_func);
    llvm::AllocaInst* getOrCreateLocalValidEpoch(const void* key,
            llvm::Function* llvm_func);
    void markLocalCacheFresh(const void* key, llvm::Function* llvm_func);
    // Marks the cache of a local stale, so that the next ensureLocalCacheFresh() reloads it from the runtime stack
    // on every path (see clearLocalCachedValue())
    void markLocalCacheStale(const void* key, llvm::Function* llvm_func);
    void ensureLocalCacheFresh(const void* key, llvm::Module& module,
            llvm::Function* llvm_func);

    // Invalidate all reloadable local variable allocas. Called after calls that
    // can modify locals through the Qore runtime stack; actual reloads are lazy.
    void reloadAllLocalsFromRuntime(llvm::Module& module, llvm::Function* llvm_func,
            bool honor_reload_exempt = true, bool eager = false);
    void invalidateLocalsForCallee(const BatchCalleeInfo& info,
            llvm::Module& module, llvm::Function* llvm_func,
            bool honor_reload_exempt = true);

    // Phase 5b: Emit inline LLVM fast-path for .any comparisons (EqAny/NeAny/etc).
    // Type-checks operands for int-vs-int and float-vs-float, falls back to helper for mixed types.
    llvm::Value* emitAnyCmpFastPath(llvm::CmpInst::Predicate int_pred,
            llvm::CmpInst::Predicate float_pred, int opcode,
            const QoreIRInstruction* inst,
            llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module, bool native_result = false);

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

    enum class FusedMapMode {
        Scale,
        Square,
        Offset,
    };

    //! Emit an AOT-native fused map/fold loop without an intermediate list.
    bool emitFusedMapFoldLoop(const QoreIRInstruction* inst, llvm::Module& module,
            llvm::Function* llvm_func, const char* label, bool is_float,
            bool product, FusedMapMode map_mode, std::string& error);

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

// qore_ir_return_needs_coercion(), qore_ir_return_value_needs_conversion(),
// qore_ir_nothing_return_needs_filter() and qore_ir_function_returns_pass_through() are declared in QoreIRAnalysis.h

#endif

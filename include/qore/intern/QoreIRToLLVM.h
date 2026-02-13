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

class LocalVar;

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

    //! Set shared debug info for multi-function module compilation (AOT/batch).
    //! When set, the lowerer uses the shared DIBuilder/DICompileUnit instead of
    //! creating and finalizing its own per-function.  The caller is responsible
    //! for creating the DIBuilder+CU before lowering and finalizing after.
    void setSharedDebugInfo(llvm::DIBuilder* builder, llvm::DICompileUnit* cu) {
        shared_di_builder = builder;
        shared_di_cu = cu;
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

    // Track which value IDs already contain NaN-boxed i64 (from Invoke, Call, CatchException,
    // make_string, .any ops, LoadLocal).  Values NOT in this set are raw typed values.
    std::unordered_set<uint32_t> nanboxed_values;

    // Set of LocalVar* (as void*) that are pre-instantiated by the caller (tiered
    // compilation); skip qore_rt_instantiate_local / qore_rt_uninstantiate_local for these.
    const std::unordered_set<const void*>* pre_instantiated_locals = nullptr;

    // Set of LocalVar* (as void*) that are only accessed by LoadLocal/StoreLocal in
    // fully-lowered IR code.  For these locals, skip qore_rt_assign_local (runtime sync)
    // and reloadLocalFromRuntime (reload after calls).
    const std::unordered_set<const void*>* ir_only_locals_set = nullptr;

    // Phase 4: True when ALL locals in the function are IR-only, meaning
    // reloadAllLocalsFromRuntime() can be skipped entirely after calls.
    bool all_locals_ir_only = false;

    // Sets of IR-only locals that use native (unboxed) allocas for typed int/float.
    // LoadLocal from these returns native i64/double (NOT nanboxed).
    // StoreLocal to these stores native i64/double (skips boxing).
    std::unordered_set<const void*> native_int_locals;
    std::unordered_set<const void*> native_float_locals;

    // Set of body locals that are IR-only — these can have their allocas initialized
    // to NOTHING directly instead of loading from the runtime stack (since the fast
    // call path skips their instantiation when all body locals are IR-only).
    std::unordered_set<const void*> ir_only_body_locals;

    // Saved on_block_exit handler count at function entry (for LIFO cleanup)
    llvm::Value* obe_saved_count = nullptr;

    // Per-scope saved on_block_exit counts (scope_id -> saved count value)
    // Used by ScopeEnter/ScopeExit for nested on_exit handler execution
    std::unordered_map<uint32_t, llvm::Value*> scope_obe_counts;

    // Entry-block load values for pre-instantiated locals (tiered compilation).
    // qore_rt_load_local creates +1 ref at entry; these must be decref'd at function exit.
    std::vector<llvm::Value*> preinstantiated_entry_loads;

    // Allocas for fast entry param locals (Approach B).
    // At exit, the current alloca value is loaded and decref'd.  Combined with
    // decref-before-store in StoreLocal for IR-only locals, this correctly handles
    // both the initial param value and any reassignments.
    std::vector<llvm::AllocaInst*> fast_entry_param_allocas;

    // Allocas for Invoke/ConstString results that need cleanup at function exit.
    // qore_rt_invoke_expr returns +1 ref; these allocas track the results so they
    // can be decref'd at exit (matching the IR interpreter's cleanup vector).
    std::vector<llvm::Value*> invoke_result_allocas;
    // Map from value ID to invoke-result alloca (for clearing at Return)
    std::unordered_map<uint32_t, llvm::Value*> invoke_alloca_map;
    // Map from local key (LocalVar* as void*) to invoke-result cleanup allocas
    // that hold references to values stored in this local.  Used by
    // UninstantiateLocal to clear cleanup allocas when block-scoped locals
    // are destroyed, preventing deferred-to-exit cleanup from keeping objects alive.
    std::unordered_map<const void*, std::vector<llvm::Value*>> local_cleanup_allocas;

    // Reload tracker allocas for local variables modified by lvalue operations.
    // Each tracker alloca holds the most recent qore_rt_load_local reload value
    // (+1 ref) so it can be decref'd before being replaced or at function exit.
    std::unordered_map<const void*, llvm::Value*> local_reload_trackers;

    // Error return block: used for exception cleanup when outside a try block.
    // When a Call/CallDirect/CallMethodDirect throws outside a try, execution
    // jumps here to return NOTHING immediately.  Created lazily on first use.
    llvm::BasicBlock* error_return_block = nullptr;

    // Deferred PHI nodes: (LLVM PHI, IR PHI instruction) pairs to fixup after all blocks lowered
    std::vector<std::pair<llvm::PHINode*, const QoreIRPhiInstruction*>> pending_phis;

    // Pointer to current IR function being lowered (for reading type profiles)
    const QoreIRFunction* current_ir_func = nullptr;

    // Pointer to current LLVM module (set during lowerFunction)
    llvm::Module* current_module = nullptr;

    // Phase 5c: Debug info (DWARF)
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

    // Initialize types and helpers
    void initTypes();

    // Declare external runtime helper functions in the module
    void declareRuntimeHelpers(llvm::Module& module);

    // Lower a single instruction; returns false on unsupported opcode
    bool lowerInstruction(const QoreIRInstruction* inst, llvm::Function* llvm_func,
            llvm::Module& module, std::string& error);

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

    // Emit qore_rt_decref calls for pre-instantiated local entry loads
    void emitPreinstantiatedCleanup(llvm::Module& module);

    // Emit qore_rt_decref calls for tracked runtime call results
    void emitInvokeCleanup(llvm::Module& module);

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

    // Phase 5b: Try to emit a specialized hash key access instead of qore_rt_invoke_expr.
    // Returns true if specialized code was emitted, false to fall through to generic path.
    bool tryEmitHashKeyAccess(const QoreIRInstruction* inst, llvm::Module& module,
            llvm::Function* llvm_func);

    // Phase 5b: Try to emit a specialized list index access instead of qore_rt_invoke_expr.
    // Returns true if specialized code was emitted, false to fall through to generic path.
    bool tryEmitListIndexAccess(const QoreIRInstruction* inst, llvm::Module& module,
            llvm::Function* llvm_func);

    // Phase 5c: Get or create DIFile from a file path
    llvm::DIFile* getDIFile(const char* file_path);

    // Phase 5c: Set IRBuilder debug location from instruction's source location
    void setDebugLocation(const QoreIRInstruction* inst);

    // Phase 5b: Emit inline LLVM fast-path for .any arithmetic (AddAny/SubAny/MulAny).
    // Type-checks operands for int+int and float+float, falls back to helper for mixed types.
    llvm::Value* emitAnyArithFastPath(llvm::Instruction::BinaryOps int_op,
            llvm::Instruction::BinaryOps float_op, const char* slow_helper,
            llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module);

    // Reload a specific local variable's alloca from the Qore runtime stack.
    // Called after lvalue operations (PostInc, StoreLvalue, etc.) that modify the
    // runtime stack without updating the LLVM alloca cache.
    void reloadLocalFromRuntime(const void* key, llvm::Module& module, llvm::Function* llvm_func);

    // Clear the reload tracker for a specific local, releasing the +1 reference
    // held from a previous reloadLocalFromRuntime() call.  Called before lvalue
    // compound operations (+=, -=, etc.) so that the container's refcount drops to 1,
    // enabling copy-on-write to skip the copy (in-place modification).
    void clearLocalReloadTracker(const void* key, llvm::Module& module, llvm::Function* llvm_func);

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

    // Build an entry-block alloca'd array of NaN-boxed args from operands[arg_start..].
    // Sets args_array and nargs.  Returns false on error.
    // If nargs == 0, args_array is set to a null pointer.
    bool buildArgsArray(const QoreIRInstruction* inst, int arg_start,
            llvm::Function* llvm_func, llvm::Value*& args_array, int& nargs,
            std::string& error);

    // Reload all local variable allocas from the Qore runtime stack.
    // Called after Invoke instructions (which can modify any local via AST evaluation).
    void reloadAllLocalsFromRuntime(llvm::Module& module, llvm::Function* llvm_func);

    // Phase 5b: Emit inline LLVM fast-path for .any comparisons (EqAny/NeAny/etc).
    // Type-checks operands for int-vs-int and float-vs-float, falls back to helper for mixed types.
    llvm::Value* emitAnyCmpFastPath(llvm::CmpInst::Predicate int_pred,
            llvm::CmpInst::Predicate float_pred, int opcode,
            llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module);

    // Emit inline LLVM fast-path for .any compound assignments (AddAssignAny/SubAssignAny/etc).
    // Type-checks operands for int+int and float+float, falls back to helper for mixed types.
    // For AddAssignAny (handle_nothing=true), returns rhs if lhs is NOTHING.
    llvm::Value* emitAnyCompoundAssignFastPath(llvm::Instruction::BinaryOps int_op,
            llvm::Instruction::BinaryOps float_op, const char* slow_helper,
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
            llvm::Value* lhs, llvm::Value* rhs,
            llvm::Function* llvm_func, llvm::Module& module);

    // Emit inline LLVM fast-path for .any unary operations (UnaryMinusAny/UnaryPlusAny).
    // Type-checks operand for int or float, falls back to qore_rt_unary_op for other types.
    // is_minus=true for UnaryMinusAny, false for UnaryPlusAny.
    llvm::Value* emitAnyUnaryFastPath(bool is_minus, int opcode,
            llvm::Value* operand, llvm::Function* llvm_func, llvm::Module& module);

    // Emit inline LLVM fast-path for CmpAny (spaceship operator).
    // Type-checks operands for int+int and float+float, falls back to qore_rt_comparison_op.
    // Returns boxed int (-1, 0, or 1).
    llvm::Value* emitAnyCmpSpaceshipFastPath(llvm::Value* lhs, llvm::Value* rhs,
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
};

#endif

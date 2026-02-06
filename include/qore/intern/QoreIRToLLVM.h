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

    // Track which value IDs already contain NaN-boxed i64 (from Invoke, Call, CatchException,
    // make_string, .any ops, LoadLocal).  Values NOT in this set are raw typed values.
    std::unordered_set<uint32_t> nanboxed_values;

    // Set of LocalVar* (as void*) that are pre-instantiated by the caller (tiered
    // compilation); skip qore_rt_instantiate_local / qore_rt_uninstantiate_local for these.
    const std::unordered_set<const void*>* pre_instantiated_locals = nullptr;

    // Saved on_block_exit handler count at function entry (for LIFO cleanup)
    llvm::Value* obe_saved_count = nullptr;

    // Per-scope saved on_block_exit counts (scope_id -> saved count value)
    // Used by ScopeEnter/ScopeExit for nested on_exit handler execution
    std::unordered_map<uint32_t, llvm::Value*> scope_obe_counts;

    // Entry-block load values for pre-instantiated locals (tiered compilation).
    // qore_rt_load_local creates +1 ref at entry; these must be decref'd at function exit.
    std::vector<llvm::Value*> preinstantiated_entry_loads;

    // Allocas for Invoke/ConstString results that need cleanup at function exit.
    // qore_rt_invoke_expr returns +1 ref; these allocas track the results so they
    // can be decref'd at exit (matching the IR interpreter's cleanup vector).
    std::vector<llvm::Value*> invoke_result_allocas;
    // Map from value ID to invoke-result alloca (for clearing at Return)
    std::unordered_map<uint32_t, llvm::Value*> invoke_alloca_map;

    // Reload tracker allocas for local variables modified by lvalue operations.
    // Each tracker alloca holds the most recent qore_rt_load_local reload value
    // (+1 ref) so it can be decref'd before being replaced or at function exit.
    std::unordered_map<const void*, llvm::Value*> local_reload_trackers;

    // Deferred PHI nodes: (LLVM PHI, IR PHI instruction) pairs to fixup after all blocks lowered
    std::vector<std::pair<llvm::PHINode*, const QoreIRPhiInstruction*>> pending_phis;

    // Pointer to current IR function being lowered (for reading type profiles)
    const QoreIRFunction* current_ir_func = nullptr;

    // Phase 5c: Debug info (DWARF)
    std::unique_ptr<llvm::DIBuilder> di_builder;
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
};

#endif

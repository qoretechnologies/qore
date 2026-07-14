/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreJIT.h

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

#ifndef _QORE_QOREJIT_H
#define _QORE_QOREJIT_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <thread>
#include <queue>
#include <condition_variable>

#include <qore/QoreValue.h>
#include <qore/Restrictions.h>

class ExceptionSink;
class LocalVar;
class QoreIRFunction;
class AbstractQoreFunctionVariant;
class UserVariantBase;
class UserSignature;
class QoreProgram;
class QoreTypeInfo;

#include <llvm/ExecutionEngine/Orc/LLJIT.h>

//! JIT-compiled function signature: takes ExceptionSink*, returns NaN-boxed QoreValue as uint64_t
using JitFunctionPtr = uint64_t (*)(ExceptionSink*);

//! Check and clear the thread-local JIT deopt flag.
//! Returns true if a JIT guard failure requested deopt to AST.
//! Called by evalTiered() after JIT execution returns.
DLLLOCAL bool qore_jit_deopt_requested();

//! Info about a batch callee for LLVM lowering (Approach B: direct LLVM arg passing).
//! Used by QoreIRToLLVM to decide how to emit CallDirect instructions.
enum class BatchCalleeParamKind : uint8_t {
    Boxed = 0,
    NativeInt = 1,
    NativeFloat = 2,
};

enum class BatchCalleeReturnKind : uint8_t {
    Boxed = 0,
    NativeInt = 1,
    NativeFloat = 2,
};

enum class AOTScalarLeafKind : uint8_t {
    None = 0,
    IntBinary = 1,
    FloatBinary = 2,
    IntAffine = 3,
    IntSelectLhsIfTrue = 4,
    IntSelectRhsIfTrue = 5,
    IntAffineSelect = 6,
};

struct AOTScalarLeafInfo {
    AOTScalarLeafKind kind = AOTScalarLeafKind::None;
    uint16_t opcode = 0;
    int8_t lhs_param = -1;
    int8_t rhs_param = -1;
    int64_t lhs_int = 0;
    int64_t rhs_int = 0;
    double lhs_float = 0.0;
    double rhs_float = 0.0;
    int64_t true_scale = 0;
    int64_t true_offset = 0;
    int64_t false_scale = 0;
    int64_t false_offset = 0;
};

enum class AOTIntExpressionNodeKind : uint8_t {
    Param = 1,
    Constant = 2,
    Add = 3,
    Sub = 4,
    Mul = 5,
    And = 6,
    Or = 7,
    Xor = 8,
    Eq = 9,
    Ne = 10,
    Lt = 11,
    Le = 12,
    Gt = 13,
    Ge = 14,
    Select = 15,
};

constexpr size_t QORE_AOT_INT_EXPRESSION_MAX_NODES = 16;

//! One topologically ordered node in a bounded pure native-integer expression.
struct AOTIntExpressionNodeInfo {
    AOTIntExpressionNodeKind kind = AOTIntExpressionNodeKind::Constant;
    uint8_t lhs = UINT8_MAX;
    uint8_t rhs = UINT8_MAX;
    uint8_t third = UINT8_MAX;
    int8_t param = -1;
    int64_t constant = 0;
};

//! Bounded pure native-integer expression; the last node is the result.
struct AOTIntExpressionInfo {
    std::vector<AOTIntExpressionNodeInfo> nodes;

    explicit operator bool() const {
        return !nodes.empty();
    }
};

struct AOTFixedHashRemapInfo {
    std::vector<std::string> input_keys;
    std::vector<std::string> output_keys;
    const QoreTypeInfo* result_type_info = nullptr;

    explicit operator bool() const {
        return input_keys.size() == 2 && output_keys.size() == 2;
    }
};

enum class AOTStringOpKind : uint8_t {
    None = 0,
    Size = 1,
    Length = 2,
    StartsWith = 3,
    EndsWith = 4,
    Contains = 5,
    Find = 6,
    RFind = 7,
    Substr = 8,
};

struct AOTStringOpInfo {
    AOTStringOpKind kind = AOTStringOpKind::None;
    int8_t base_param = -1;
    int8_t arg0_param = -1;
    int8_t arg1_param = -1;

    explicit operator bool() const {
        return kind != AOTStringOpKind::None;
    }
};

enum class AOTCollectionOpKind : uint8_t {
    None = 0,
    ListSize = 1,
    ListIndex = 2,
    HashKeyInt = 3,
};

struct AOTCollectionOpInfo {
    AOTCollectionOpKind kind = AOTCollectionOpKind::None;
    int8_t base_param = -1;
    int8_t index_param = -1;
    bool string_index_char = false;
    std::string key;

    explicit operator bool() const {
        return kind != AOTCollectionOpKind::None;
    }
};

enum class AOTComposedIntSourceKind : uint8_t {
    None = 0,
    ListSize = 1,
    StringSize = 2,
    StringLength = 3,
};

//! Bounded affine composition of one typed size/length source and one int parameter.
struct AOTComposedIntInfo {
    AOTComposedIntSourceKind source_kind = AOTComposedIntSourceKind::None;
    int8_t base_param = -1;
    int8_t value_param = -1;
    int64_t source_scale = 0;
    int64_t value_scale = 0;
    int64_t offset = 0;

    explicit operator bool() const {
        return source_kind != AOTComposedIntSourceKind::None;
    }
};

//! Bounded affine expression over one native int argument and one captured int lvalue.
struct AOTContextIntInfo {
    int8_t value_param = -1;
    int32_t local_slot = -1;
    int64_t value_scale = 0;
    int64_t context_scale = 0;
    int64_t offset = 0;

    explicit operator bool() const {
        return local_slot >= 0;
    }
};

//! Bounded affine expression over one native int argument and one global int lvalue.
struct AOTGlobalIntInfo {
    int8_t value_param = -1;
    int32_t global_slot = -1;
    int64_t value_scale = 0;
    int64_t global_scale = 0;
    int64_t offset = 0;

    explicit operator bool() const {
        return global_slot >= 0;
    }
};

struct BatchCalleeInfo {
    std::string name;                    //!< Standard entry function name
    bool approach_b_eligible = false;    //!< True if fast entry exists
    bool implicit_self_method = false;   //!< Fast entry reuses the caller's self/class context
    bool context_independent_fast_entry = false; //!< True if fast entry does not require its own AOT context
    bool may_invalidate_external_caches = true; //!< Callee can mutate caller-visible runtime state
    bool never_returns_nothing = false; //!< Every normal return has an assigned non-NOTHING value
    BatchCalleeReturnKind return_kind = BatchCalleeReturnKind::Boxed; //!< Fast-entry return ABI
    std::string fast_name;               //!< Fast entry function name (if eligible)
    unsigned num_params = 0;             //!< Number of parameters
    std::vector<BatchCalleeParamKind> param_kinds; //!< Fast-entry parameter ABI kinds
    std::vector<uint8_t> param_rejects_nothing; //!< True for params that cannot accept NOTHING
    std::vector<uint8_t> param_noescape; //!< Boxed params that can remain borrowed for the call
    AOTScalarLeafInfo scalar_leaf;       //!< Importable pure scalar body summary
    AOTIntExpressionInfo int_expression; //!< Importable bounded pure native-int expression
    AOTFixedHashRemapInfo fixed_hash_remap; //!< Importable two-key hash remap body
    AOTStringOpInfo string_op;            //!< Importable encoding-aware string operation
    AOTCollectionOpInfo collection_op;    //!< Importable typed collection operation
    AOTComposedIntInfo composed_int;       //!< Importable bounded size/length expression
    AOTContextIntInfo context_int;         //!< Importable affine captured-int expression
    AOTGlobalIntInfo global_int;           //!< Importable affine global-int expression
    std::string object_getter_member;     //!< Exact final-object getter member name
};

//! Derive fast-entry parameter ABI kinds from lowered IR local metadata.
DLLLOCAL std::vector<BatchCalleeParamKind> qore_ir_get_fast_entry_param_kinds(
        const QoreIRFunction& ir_func, const UserSignature* sig);

//! Derive fast-entry parameter NOTHING rejection metadata from the signature.
DLLLOCAL std::vector<uint8_t> qore_ir_get_fast_entry_param_rejects_nothing(const UserSignature* sig);

//! Derive the fast-entry return ABI from the exact declared return type and
//! assigned-state summary.  A native ABI is never selected for a return that
//! can be NOTHING.
DLLLOCAL BatchCalleeReturnKind qore_ir_get_fast_entry_return_kind(
        const AbstractQoreFunctionVariant* variant, bool never_returns_nothing);

//! Returns locals that cannot safely use native scalar storage because a load may
//! observe NOTHING, a lvalue mutation bypasses StoreLocal, or the analysis was
//! cancelled. @p initially_assigned contains locals that are assigned on function
//! entry, normally signature parameters.
DLLLOCAL std::unordered_set<const void*> qore_ir_get_native_unsafe_locals(
        const QoreIRFunction& ir_func, const std::unordered_set<const void*>& initially_assigned);

class QoreJIT {
public:
    enum class DeoptPolicy {
        FallbackToInterpreter,
        DisableJit,
    };
    static QoreJIT& instance();

    bool isEnabled() const;
    bool initialize(std::string& error);
    bool canJit(int64 parse_options, std::string& reason) const;

    //! Compile an IR function to native code and cache the result.
    //! Returns true on success; on failure, sets error message.
    bool compileFunction(const QoreIRFunction& func, std::string& error,
            void* deopt_counter = nullptr);

    //! Batch-compile a root function and its direct callees into a shared LLVM module.
    //! This enables LLVM cross-function inlining and optimization.
    //! The callee_map maps variant → (IR function, deopt counter) for each callee.
    //! Returns true on success; on failure, sets error message.
    struct BatchCallee {
        const QoreIRFunction* ir_func;
        void* deopt_counter;
        const AbstractQoreFunctionVariant* variant;
        bool approach_b_eligible = false;    //!< True if callee can use direct LLVM arg passing
        unsigned num_params = 0;             //!< Number of parameters (for fast entry signature)
    };
    bool compileFunctionBatch(const QoreIRFunction& root_func, std::string& error,
            void* root_deopt_counter,
            const std::vector<BatchCallee>& callees);

    //! Look up a previously compiled function by name.
    //! Returns nullptr if not found.
    JitFunctionPtr lookupFunction(const std::string& name) const;

    //! Compile and execute, falling back to IR interpreter on failure.
    bool executeWithFallback(const QoreIRFunction& func, QoreValue& return_value, ExceptionSink* xsink,
            std::string& error, const std::unordered_set<const LocalVar*>* pre_instantiated = nullptr);
    void shutdown();
    void setDeoptPolicy(DeoptPolicy policy);

    //! Tiered compilation threshold accessors
    static uint64_t getIRThreshold();
    static uint64_t getJITThreshold();
    static void setIRThreshold(uint64_t t);
    static void setJITThreshold(uint64_t t);

    //! JIT optimization level (0-3, default 2, overridable via QORE_JIT_OPT_LEVEL)
    static int getJITOptLevel();

    //! Try to acquire the compilation mutex without blocking.
    //! Returns true if the lock was acquired; caller MUST call releaseCompileLock() after compilation.
    //! This prevents deadlocks when JIT compilation is triggered while Qore mutexes are held —
    //! if the lock is contended, the function stays at IR tier and retries on next call.
    bool tryAcquireCompileLock();

    //! Release the compilation mutex after a successful tryAcquireCompileLock().
    void releaseCompileLock();

    //! Compile an IR function to native code, assuming compile_mutex is already held.
    //! Use after tryAcquireCompileLock() returns true.
    bool compileFunctionLocked(const QoreIRFunction& func, std::string& error,
            void* deopt_counter = nullptr);

    //! Batch-compile functions, assuming compile_mutex is already held.
    //! Use after tryAcquireCompileLock() returns true.
    bool compileFunctionBatchLocked(const QoreIRFunction& root_func, std::string& error,
            void* root_deopt_counter,
            const std::vector<BatchCallee>& callees);

    //! Enqueue a function for background JIT compilation.
    //! The function stays at IR tier until compilation finishes in the background.
    //! Safe to call from any thread, including during tiered execution.
    void enqueueBgCompile(const AbstractQoreFunctionVariant* variant, const QoreIRFunction* ir_func,
            void* deopt_counter, const std::vector<BatchCallee>* callees = nullptr);

    //! Wait for all pending background compilations to complete.
    //! Used during shutdown.
    void waitForBgCompileQueue();

    //! Drain only the background compilations that reference the given Program.
    //! Called when a Program is being destroyed: its IR functions (and the
    //! program-owned data they reference, e.g. LocalVars and AST) are about to be
    //! freed, so any queued compile that touches this Program must be cancelled
    //! and any in-progress compile that touches it must be awaited — but compiles
    //! belonging to OTHER Programs are left running.  This avoids serializing every
    //! Program teardown behind the entire global compile queue.
    void waitForBgCompileQueue(QoreProgram* pgm);

private:
    QoreJIT() = default;
    ~QoreJIT() {
        // Ensure background thread is stopped before destructor completes
        shutdown();
    }
    QoreJIT(const QoreJIT&) = delete;
    QoreJIT& operator=(const QoreJIT&) = delete;

    std::unique_ptr<llvm::orc::LLJIT> jit;
    bool registerRuntimeSymbols(std::string& error);
    bool symbols_registered = false;
    mutable std::mutex cache_mutex;
    // Mutex to serialize JIT compilations (LLJIT operations are not fully thread-safe)
    std::mutex compile_mutex;
    std::unordered_map<std::string, JitFunctionPtr> compiled_functions;
    std::once_flag init_flag;
    bool init_success = false;
    std::string init_error;
    DeoptPolicy deopt_policy = DeoptPolicy::FallbackToInterpreter;

    //! Tiered compilation thresholds
    static uint64_t ir_threshold;
    static uint64_t jit_threshold;

    //! JIT optimization level (-1 = not yet initialized)
    static int jit_opt_level;

    //! Internal compilation logic (assumes compile_mutex is held)
    bool compileFunctionInternal(const QoreIRFunction& func, std::string& error,
            void* deopt_counter);
    bool compileFunctionBatchInternal(const QoreIRFunction& root_func, std::string& error,
            void* root_deopt_counter,
            const std::vector<BatchCallee>& callees);

    //! Background compilation work item
    struct BgCompileWork {
        const UserVariantBase* uvb;                         //!< function to compile
        const QoreIRFunction* ir_func;                      //!< IR representation
        void* deopt_ptr;                                    //!< deopt counter pointer
        std::vector<BatchCallee> callees;                   //!< direct callees (if any)
        std::vector<AbstractQoreFunctionVariant*> variant_refs; //!< refs keeping queued variants alive
        bool has_callees = false;                           //!< true if callees should be compiled
    };

    // Background compilation thread management
    std::thread bg_compile_thread;                          //!< dedicated background worker thread
    std::queue<BgCompileWork> bg_compile_queue;             //!< pending compilation work
    std::mutex bg_queue_mutex;                              //!< protects the queue
    std::condition_variable bg_queue_cv;                    //!< signals new work or queue empty
    std::atomic<bool> bg_thread_running{false};             //!< shutdown flag
    std::atomic<int> bg_active_work{0};                     //!< count of pending+in-progress compilations
    std::condition_variable bg_queue_empty_cv;              //!< signals when queue becomes empty
    //! The work item currently being compiled by the bg thread (nullptr when
    //! idle).  Protected by bg_queue_mutex; points at the bg thread's local while
    //! it holds compile_mutex.  Used by waitForBgCompileQueue(QoreProgram*) to
    //! decide whether the in-progress compile must be awaited.
    const BgCompileWork* bg_in_progress = nullptr;

    //! Background thread worker loop
    void bgCompileThreadLoop();

    //! Initialize and start the background compilation thread
    void startBackgroundThread();

    //! Release variant refs held by a background work item.
    void releaseBgCompileWorkRefs(BgCompileWork& work);

    //! True if a work item's root or any folded-in callee IR belongs to pgm.
    static bool workReferencesPgm(const BgCompileWork& work, const QoreProgram* pgm);

    //! Complete a background work item: release refs, update active count, and notify waiters.
    void finishBgCompileWork(BgCompileWork& work);
};

#endif

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
struct QoreTypeParamInstantiation;

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
    NativeBool = 3,
};

enum class BatchCalleeReturnKind : uint8_t {
    Boxed = 0,
    NativeInt = 1,
    NativeFloat = 2,
    NativeBool = 3,
};

//! Exact runtime kind of a proven assigned boxed return value.
//!
//! This enum is serialized in AOT symbol indexes; append new values only.
enum class BatchCalleeBoxedReturnKind : uint8_t {
    Unknown = 0,
    String = 1,
    List = 2,
    Hash = 3,
    Binary = 4,
    Date = 5,
    Object = 6,
    Number = 7,
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
    ListSize = 16,
    StringSize = 17,
    StringLength = 18,
    StringStartsWith = 19,
    StringEndsWith = 20,
    StringContains = 21,
    StringFind = 22,
    StringRFind = 23,
    Div = 24,
    Mod = 25,
    Shl = 26,
    Shr = 27,
    Neg = 28,
    HashKeyInt = 29,
    HashKeyStringSize = 30,
    HashKeyStringLength = 31,
};

constexpr size_t QORE_AOT_INT_EXPRESSION_MAX_NODES = 64;

//! One topologically ordered node in a bounded pure native-integer or boolean expression.
struct AOTIntExpressionNodeInfo {
    AOTIntExpressionNodeKind kind = AOTIntExpressionNodeKind::Constant;
    uint8_t lhs = UINT8_MAX;
    uint8_t rhs = UINT8_MAX;
    uint8_t third = UINT8_MAX;
    int8_t param = -1;
    int64_t constant = 0;
    std::string key;
};

//! Bounded pure native-integer or boolean expression; the last node is the result.
struct AOTIntExpressionInfo {
    std::vector<AOTIntExpressionNodeInfo> nodes;

    explicit operator bool() const {
        return !nodes.empty();
    }
};

enum class AOTFloatExpressionNodeKind : uint8_t {
    Param = 1,
    Constant = 2,
    Add = 3,
    Sub = 4,
    Mul = 5,
    Div = 6,
    Neg = 7,
    BoolParam = 8,
    //! Select a float value; lhs is the condition, rhs the true value, and param the false value.
    Select = 9,
    HashKeyFloat = 10,
};

constexpr size_t QORE_AOT_FLOAT_EXPRESSION_MAX_NODES = 64;

//! One topologically ordered node in a bounded pure native-float expression.
struct AOTFloatExpressionNodeInfo {
    AOTFloatExpressionNodeKind kind = AOTFloatExpressionNodeKind::Constant;
    uint8_t lhs = UINT8_MAX;
    uint8_t rhs = UINT8_MAX;
    int8_t param = -1;
    double constant = 0.0;
    std::string key;
};

//! Bounded pure native-float expression; the last node is the result.
struct AOTFloatExpressionInfo {
    std::vector<AOTFloatExpressionNodeInfo> nodes;

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
    Concat = 9,
    Concat3 = 10,
    IntToString = 11,
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

enum class AOTStringExpressionNodeKind : uint8_t {
    StringParam = 1,
    IntParam = 2,
    StringConstant = 3,
    IntConstant = 4,
    IntToString = 5,
    Concat = 6,
    Substr = 7,
    HashKeyString = 8,
};

constexpr size_t QORE_AOT_STRING_EXPRESSION_MAX_NODES = 64;

//! One topologically ordered node in a bounded typed string expression.
struct AOTStringExpressionNodeInfo {
    AOTStringExpressionNodeKind kind = AOTStringExpressionNodeKind::StringConstant;
    uint8_t lhs = UINT8_MAX;
    uint8_t rhs = UINT8_MAX;
    uint8_t third = UINT8_MAX;
    int8_t param = -1;
    int64_t int_constant = 0;
    std::string string_constant;
};

//! Bounded typed string expression; the last node is the result.
struct AOTStringExpressionInfo {
    std::vector<AOTStringExpressionNodeInfo> nodes;

    explicit operator bool() const {
        return !nodes.empty();
    }
};

enum class AOTCollectionOpKind : uint8_t {
    None = 0,
    ListSize = 1,
    ListIndex = 2,
    HashKeyInt = 3,
    HashKeyBoxed = 4,
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

enum class AOTAggregateReturnKind : uint8_t {
    None = 0,
    FixedList = 1,
    FixedHash = 2,
};

enum class AOTAggregateReturnValueKind : uint8_t {
    Parameter = 0,
    IntConstant = 1,
    FloatConstant = 2,
    BoolConstant = 3,
    Unknown = 4,
    IntParamAddConstant = 5,
    FloatParamAddConstant = 6,
    IntParamSelect = 7,
    FloatParamSelect = 8,
    BoolParamSelect = 9,
    IntParamBinary = 10,
    IntParamMulConstant = 11,
    BoolIntParamCompare = 12,
};

//! Fresh fixed aggregate whose values are native parameters, scalar constants,
//! bounded parameter expressions, or unknown when only the side-effect-free
//! aggregate shape is summarized.
struct AOTAggregateReturnInfo {
    AOTAggregateReturnKind kind = AOTAggregateReturnKind::None;
    std::vector<int8_t> value_params;
    std::vector<AOTAggregateReturnValueKind> value_kinds;
    std::vector<int64_t> value_ints;
    std::vector<double> value_floats;
    std::vector<std::string> keys;
    int8_t shape_condition_param = -1;
    uint8_t shape_true_size = 0;
    uint8_t shape_false_size = 0;

    explicit operator bool() const {
        return kind != AOTAggregateReturnKind::None;
    }

    bool hasConditionalShape() const {
        return shape_condition_param >= 0;
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
    std::string call_ref_path;           //!< Canonical Qore function path for exact call-reference dispatch
    bool single_variant_function = false; //!< True when call_ref_path names exactly one function variant
    bool approach_b_eligible = false;    //!< True if fast entry exists
    bool generic_specialized_fast_entry = false; //!< Fast entry is valid only for specialization_key
    bool implicit_self_method = false;   //!< Fast entry reuses the caller's self/class context
    bool context_independent_fast_entry = false; //!< True if fast entry does not require its own AOT context
    bool may_invalidate_external_caches = true; //!< Callee can mutate caller-visible runtime state
    bool may_modify_runtime_locals = true; //!< Callee can change unknown caller-visible runtime local slots
    std::vector<const void*> modified_runtime_locals; //!< Exact modified locals when effects are known
    bool never_returns_nothing = false; //!< Every normal return has an assigned non-NOTHING value
    BatchCalleeReturnKind return_kind = BatchCalleeReturnKind::Boxed; //!< Fast-entry return ABI
    BatchCalleeBoxedReturnKind boxed_return_kind = BatchCalleeBoxedReturnKind::Unknown;
        //!< Exact body-proven boxed runtime kind
    std::string fast_name;               //!< Fast entry function name (if eligible)
    std::string specialization_key;      //!< Concrete generic receiver/type-argument tuple
    const QoreTypeInfo* specialization_receiver_type_info = nullptr; //!< Parse-owned concrete receiver
    const QoreTypeParamInstantiation* specialization_type_param_instantiation = nullptr; //!< Parse-owned concrete args
    unsigned num_params = 0;             //!< Number of parameters
    std::vector<BatchCalleeParamKind> param_kinds; //!< Fast-entry parameter ABI kinds
    std::vector<uint8_t> param_rejects_nothing; //!< True for params that cannot accept NOTHING
    std::vector<uint8_t> param_noescape; //!< Boxed params that can remain borrowed for the call
    std::vector<uint8_t> param_may_modify; //!< Params rebound or mutated inside the callee
    std::vector<const LocalVar*> capture_locals; //!< Read-only scalar captures passed to fast entries
    std::vector<BatchCalleeParamKind> capture_kinds; //!< Native capture ABI kinds
    AOTScalarLeafInfo scalar_leaf;       //!< Importable pure scalar body summary
    AOTIntExpressionInfo int_expression; //!< Importable bounded pure native-int/bool expression
    AOTFloatExpressionInfo float_expression; //!< Importable bounded pure native-float expression
    AOTFixedHashRemapInfo fixed_hash_remap; //!< Importable two-key hash remap body
    AOTStringOpInfo string_op;            //!< Importable encoding-aware string operation
    AOTStringExpressionInfo string_expression; //!< Importable bounded typed string expression
    AOTCollectionOpInfo collection_op;    //!< Importable typed collection operation
    AOTAggregateReturnInfo aggregate_return; //!< Importable fresh fixed aggregate result
    int8_t forwarded_return_param = -1;   //!< Pure direct return parameter in the generic declared ABI
    int8_t boxed_return_param = -1;       //!< Direct boxed return parameter, or -1
    AOTComposedIntInfo composed_int;       //!< Importable bounded size/length expression
    AOTContextIntInfo context_int;         //!< Importable affine captured-int expression
    AOTGlobalIntInfo global_int;           //!< Importable affine global-int expression
    std::string object_getter_member;     //!< Exact final-object getter member name
    std::string object_set_get_member;    //!< Exact guarded object member assignment-return name
    int8_t object_set_get_param = -1;     //!< Parameter assigned by object_set_get_member
    std::string object_compound_get_member; //!< Exact guarded object member compound-return name
    int8_t object_compound_get_param = -1;  //!< RHS parameter for object_compound_get_member
    uint8_t object_compound_get_op = 0;      //!< LVCompoundOp value
};

//! Derive fast-entry parameter ABI kinds from lowered IR local metadata.
DLLLOCAL std::vector<BatchCalleeParamKind> qore_ir_get_fast_entry_param_kinds(
        const QoreIRFunction& ir_func, const UserSignature* sig);

//! Return true when a parameter can remain private to a direct fast entry.
//! Unused parameters qualify even though they have no LoadLocal/StoreLocal
//! instruction and therefore are not present in ir_only_locals.
DLLLOCAL bool qore_ir_fast_entry_param_is_private(
        const QoreIRFunction& ir_func, const LocalVar* local);

//! Derive a stable typed call ABI from exact declared parameter types.  Unlike
//! fast-entry kinds, these kinds do not depend on one lowered body and can be
//! reconstructed independently by an AOT closure caller and its dispatcher.
DLLLOCAL std::vector<BatchCalleeParamKind> qore_ir_get_signature_param_kinds(
        const UserSignature* sig);

//! Return the native ABI kind for an exact, non-optional scalar local.
DLLLOCAL BatchCalleeParamKind qore_ir_get_scalar_local_kind(const LocalVar* local);

//! Derive fast-entry parameter NOTHING rejection metadata from the signature.
DLLLOCAL std::vector<uint8_t> qore_ir_get_fast_entry_param_rejects_nothing(
        const UserSignature* sig, const QoreIRFunction* ir_func = nullptr);

//! Derive the fast-entry return ABI from the exact declared return type and
//! assigned-state summary.  A native ABI is never selected for a return that
//! can be NOTHING.
DLLLOCAL BatchCalleeReturnKind qore_ir_get_fast_entry_return_kind(
        const AbstractQoreFunctionVariant* variant, bool never_returns_nothing,
        const QoreIRFunction* ir_func = nullptr);

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
        bool batch_only = false;              //!< Do not publish or promote this internal callee
        std::vector<const LocalVar*> capture_locals; //!< Explicit read-only closure captures
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

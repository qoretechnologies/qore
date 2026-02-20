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

#include <llvm/ExecutionEngine/Orc/LLJIT.h>

//! JIT-compiled function signature: takes ExceptionSink*, returns NaN-boxed QoreValue as uint64_t
using JitFunctionPtr = uint64_t (*)(ExceptionSink*);

//! Check and clear the thread-local JIT deopt flag.
//! Returns true if a JIT guard failure requested deopt to AST.
//! Called by evalTiered() after JIT execution returns.
DLLLOCAL bool qore_jit_deopt_requested();

//! Info about a batch callee for LLVM lowering (Approach B: direct LLVM arg passing).
//! Used by QoreIRToLLVM to decide how to emit CallDirect instructions.
struct BatchCalleeInfo {
    std::string name;                    //!< Standard entry function name
    bool approach_b_eligible = false;    //!< True if fast entry exists
    std::string fast_name;               //!< Fast entry function name (if eligible)
    unsigned num_params = 0;             //!< Number of parameters
};

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
    void enqueueBgCompile(const UserVariantBase* uvb, const QoreIRFunction* ir_func,
            void* deopt_counter, const std::vector<BatchCallee>* callees = nullptr);

    //! Wait for all pending background compilations to complete.
    //! Used during shutdown.
    void waitForBgCompileQueue();

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
        bool has_callees = false;                           //!< true if callees should be compiled
    };

    // Background compilation thread management
    std::thread bg_compile_thread;                          //!< dedicated background worker thread
    std::queue<BgCompileWork> bg_compile_queue;             //!< pending compilation work
    std::mutex bg_queue_mutex;                              //!< protects the queue
    std::condition_variable bg_queue_cv;                    //!< signals new work or queue empty
    std::atomic<bool> bg_thread_running{false};             //!< shutdown flag
    std::condition_variable bg_queue_empty_cv;              //!< signals when queue becomes empty

    //! Background thread worker loop
    void bgCompileThreadLoop();

    //! Initialize and start the background compilation thread
    void startBackgroundThread();
};

#endif

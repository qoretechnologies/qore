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

private:
    QoreJIT() = default;
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
};

#endif

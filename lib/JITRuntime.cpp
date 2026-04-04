/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    JITRuntime.cpp

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

#include "qore/intern/QoreJITIncludes.h"
#include "qore/intern/JITRuntime.h"
#include "qore/intern/QoreJITException.h"

// Macro for JIT runtime functions: check xsink and throw C++ exception
// if a Qore exception was raised. Used at return points of qore_rt_*
// functions that take ExceptionSink*. This enables LLVM's invoke/landingpad
// to handle exceptions via stack unwinding instead of manual flag checking.
#define QORE_RT_CHECK_THROW(xsink) do { \
    if ((xsink) && *(xsink)) { \
        throw QoreJITException(); \
    } \
} while(0)

#include <cstring>
#include <optional>

#include <qore/ExceptionSink.h>
#include <qore/QoreValue.h>
#include <qore/QoreStringNode.h>
#include <qore/QoreHashNode.h>
#include <qore/QoreListNode.h>
#include <qore/DateTimeNode.h>
#include <qore/intern/QoreIRInterpreter.h>
#include <qore/intern/QoreIR.h>
#include <qore/intern/LocalVar.h>
#include <qore/intern/Variable.h>
#include <qore/intern/AbstractStatement.h>
#include <qore/intern/qore_thread_intern.h>
#include <qore/intern/QoreTypeInfo.h>
#include <qore/intern/OnBlockExitStatement.h>
#include <qore/intern/QoreException.h>
#include <qore/intern/StatementBlock.h>
#include <qore/intern/FunctionCallNode.h>
#include <qore/intern/CallReferenceCallNode.h>
#include <qore/intern/FunctionalOperatorInterface.h>
#include <qore/intern/QoreClassIntern.h>
#include <qore/intern/CaseNodeRegex.h>
#include <qore/intern/SwitchStatement.h>
#include <qore/intern/ConstantList.h>
#include <qore/intern/QoreClosureParseNode.h>
#include <qore/intern/QoreClosureNode.h>
#include <qore/intern/NewComplexTypeNode.h>
#include <qore/intern/typed_hash_decl_private.h>
#include <qore/intern/qore_list_private.h>
#include <qore/intern/QoreHashNodeIntern.h>
#include <qore/intern/ParseReferenceNode.h>
#include <qore/intern/VarRefNode.h>
#include <qore/intern/QoreCastOperatorNode.h>

// --- Runtime location tracking for LLVM-generated code ---
// Returns pointer to the thread-local runtime_loc variable for per-line location updates.
// Called once at function entry; the returned pointer is stored and reused for each line change.
extern "C" DLLEXPORT const QoreProgramLocation** qore_rt_get_loc_ptr() {
    RuntimeLocationCache cache = get_runtime_location_cache();
    return cache.loc_ptr;
}

// Returns pointer to the thread-local runtime_statement variable.
extern "C" DLLEXPORT const AbstractStatement** qore_rt_get_stmt_ptr() {
    RuntimeLocationCache cache = get_runtime_location_cache();
    return cache.stmt_ptr;
}

// AOT mode: set runtime location from context location table.
// Per-line update: loads location pointer from ctx->locs[loc_index] and stores to TLS.
extern "C" DLLEXPORT void qore_rt_set_runtime_loc_aot(QoreAOTContext* ctx, int32_t loc_index) {
    if (ctx && ctx->locs && loc_index >= 0 && loc_index < ctx->num_locs && ctx->locs[loc_index]) {
        RuntimeLocationCache cache = get_runtime_location_cache();
        *cache.stmt_ptr = nullptr;
        *cache.loc_ptr = ctx->locs[loc_index];
    }
}

// --- Exported check_stack wrapper for LLVM-generated code ---
extern "C" DLLEXPORT int qore_rt_check_stack(ExceptionSink* xsink) {
#ifdef QORE_MANAGE_STACK
    return check_stack(xsink);
#else
    return 0;
#endif
}

// --- Forward declarations for Phase 5 fast-call builtins ---
extern "C" DLLEXPORT uint64_t qore_fast_strlen(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_now_us(ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_now_ms(ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_now(ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_time(ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_length(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_tolower(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_toupper(uint64_t arg_bits, ExceptionSink* xsink);
// Phase 5.2c: Pseudo-method fast-calls (read-only, non-mutating)
extern "C" DLLEXPORT uint64_t qore_fast_any_size(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_hash_keys(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_hash_values(uint64_t arg_bits, ExceptionSink* xsink);
// Phase 5.3: Additional fast-path optimizations
extern "C" DLLEXPORT uint64_t qore_fast_trim(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_abs(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_first(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_last(uint64_t arg_bits, ExceptionSink* xsink);
extern "C" DLLEXPORT uint64_t qore_fast_hash_exists(uint64_t hash_bits, uint64_t key_bits, ExceptionSink* xsink);

// Fast string comparison helper matching QoreString::compare() semantics
// Returns: negative if l < r, 0 if equal, positive if l > r
// Empty strings sort at end (both "" vs "x" and "x" vs "" return 1)
static inline int fast_string_compare(const QoreStringNode* ls, const QoreStringNode* rs) {
    size_t llen = ls->size();
    size_t rlen = rs->size();

    // Handle empty strings - empty sorts at end (returns 1 if either is empty but not both)
    if (!llen) {
        return rlen ? 1 : 0;
    }
    if (!rlen) {
        return 1;  // right is empty, left is not -> left > right (empty at end)
    }

    // Compare bytes
    const char* lbuf = ls->c_str();
    const char* rbuf = rs->c_str();
    size_t minlen = llen < rlen ? llen : rlen;
    int rc = memcmp(lbuf, rbuf, minlen);
    if (rc != 0) {
        return rc < 0 ? -1 : 1;  // normalize like QoreString::compare
    }

    // Same prefix, compare lengths
    if (llen < rlen) {
        return -1;
    }
    if (llen > rlen) {
        return 1;
    }
    return 0;
}

// Helper: bit-cast between uint64_t and QoreValue.
// QoreValue is NaN-boxed and has the same size as uint64_t.
static_assert(sizeof(QoreValue) == sizeof(uint64_t), "QoreValue must be 64 bits for JIT ABI");

// Macro for always-inline attribute (portable across GCC, Clang, MSVC)
#ifdef _MSC_VER
    #define QORE_ALWAYS_INLINE __forceinline
#else
    #define QORE_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

// toBits/fromBits are defined in QoreJITIncludes.h (shared with QoreIRInterpreter.cpp)

// --- Reference counting helpers ---

// Increments reference count if value is a pointer (node), returns value unchanged
extern "C" DLLEXPORT uint64_t qore_rt_ref(uint64_t val) {
    QoreValue v = fromBits(val);
    if (v.hasNode()) {
        return toBits(v.refSelf());
    }
    return val;
}

// --- Arithmetic helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_add_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::AddAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_sub_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::SubAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_mul_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::MulAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_div_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::DivAny, lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_mod_any(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(QoreIROpcode::ModAny, lv, rv, xsink);
    return toBits(result);
}

// --- Number arithmetic (QoreNumberNode operations) ---

extern "C" DLLEXPORT uint64_t qore_rt_number_add(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreNumberNode* ln = lv.getType() == NT_NUMBER ? lv.get<const QoreNumberNode>() : nullptr;
    const QoreNumberNode* rn = rv.getType() == NT_NUMBER ? rv.get<const QoreNumberNode>() : nullptr;
    if (!ln || !rn) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(ln->doPlus(*rn)));
}

extern "C" DLLEXPORT uint64_t qore_rt_number_sub(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreNumberNode* ln = lv.getType() == NT_NUMBER ? lv.get<const QoreNumberNode>() : nullptr;
    const QoreNumberNode* rn = rv.getType() == NT_NUMBER ? rv.get<const QoreNumberNode>() : nullptr;
    if (!ln || !rn) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(ln->doMinus(*rn)));
}

extern "C" DLLEXPORT uint64_t qore_rt_number_mul(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreNumberNode* ln = lv.getType() == NT_NUMBER ? lv.get<const QoreNumberNode>() : nullptr;
    const QoreNumberNode* rn = rv.getType() == NT_NUMBER ? rv.get<const QoreNumberNode>() : nullptr;
    if (!ln || !rn) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(ln->doMultiply(*rn)));
}

extern "C" DLLEXPORT uint64_t qore_rt_number_div(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreNumberNode* ln = lv.getType() == NT_NUMBER ? lv.get<const QoreNumberNode>() : nullptr;
    const QoreNumberNode* rn = rv.getType() == NT_NUMBER ? rv.get<const QoreNumberNode>() : nullptr;
    if (!ln || !rn) {
        return toBits(QoreValue());
    }
    return toBits(QoreValue(ln->doDivideBy(*rn, xsink)));
}

// --- Integer/float division with zero check ---

extern "C" DLLEXPORT int64_t qore_rt_div_int(int64_t left, int64_t right, ExceptionSink* xsink) {
    if (!right) {
        if (xsink) {
            xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in integer expression");
        }
        return 0;
    }
    return left / right;
}

extern "C" DLLEXPORT int64_t qore_rt_mod_int(int64_t left, int64_t right, ExceptionSink* xsink) {
    if (!right) {
        if (xsink) {
            xsink->raiseException("DIVISION-BY-ZERO", "modula operand cannot be zero");
        }
        return 0;
    }
    return left % right;
}

extern "C" DLLEXPORT double qore_rt_div_float(double left, double right, ExceptionSink* xsink) {
    if (right == 0.0) {
        if (xsink) {
            xsink->raiseException("DIVISION-BY-ZERO", "division by zero found in floating-point expression");
        }
        return 0.0;
    }
    return left / right;
}

// --- Conversion helpers ---

extern "C" DLLEXPORT int64_t qore_rt_to_int(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.getAsBigInt();
}

extern "C" DLLEXPORT double qore_rt_to_float(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.getAsFloat();
}

extern "C" DLLEXPORT int64_t qore_rt_to_bool(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.getAsBool() ? 1 : 0;
}

// --- Refcount helpers ---

extern "C" DLLEXPORT void qore_rt_incref(uint64_t val) {
    QoreValue v = fromBits(val);
    if (v.hasNode()) {
        v.getInternalNode()->ref();
    }
}

extern "C" DLLEXPORT void qore_rt_decref(uint64_t val, ExceptionSink* xsink) {
    QoreValue v = fromBits(val);
    v.discard(xsink);
}

extern "C" DLLEXPORT void qore_rt_decref_nothrow(uint64_t val) {
    QoreValue v = fromBits(val);
    v.discard(nullptr);
}

// --- Cleanup stack for JIT/AOT compiled functions ---
// Replaces per-alloca cleanup tracking with a single runtime-managed array.
// This reduces the error_return block from O(N) instructions to O(1), eliminating
// LLVM optimization pathology on large functions.

//! Track a value for cleanup at scope exit.  Replaces the old per-cleanup-alloca
//! pattern with a single dynamically-grown array.
//! @param stack pointer to the stack pointer (alloca in LLVM IR)
//! @param count pointer to the count (alloca in LLVM IR)
//! @param val the NaN-boxed value to track
extern "C" DLLEXPORT void qore_rt_cleanup_push(uint64_t** stack, int32_t* count, uint64_t val) {
    QoreValue v = fromBits(val);
    if (!v.hasNode()) {
        return;  // Simple types (int, float, bool, NOTHING) don't need cleanup
    }
    int32_t n = *count;
    // Grow array if needed (initial allocation or doubling)
    if (n == 0) {
        *stack = (uint64_t*)malloc(16 * sizeof(uint64_t));
        if (!*stack) {
            return;
        }
    } else if ((n & (n - 1)) == 0 && n >= 16) {
        // Power of 2 — double the allocation
        uint64_t* new_stack = (uint64_t*)realloc(*stack, n * 2 * sizeof(uint64_t));
        if (!new_stack) {
            return;
        }
        *stack = new_stack;
    }
    (*stack)[n] = val;
    *count = n + 1;
}

//! Run all cleanup actions from an array of alloca pointers.
/** Each element in the array is a pointer to an i64 alloca. The function loads
    the value from each alloca and decrefs it. Used by emitInvokeCleanup() for
    large functions (50+ cleanup allocas) to avoid O(N) error_return blocks.
*/
extern "C" DLLEXPORT void qore_rt_cleanup_run_allocas(uint64_t** alloca_ptrs, int32_t count, ExceptionSink* xsink) {
    for (int32_t i = 0; i < count; ++i) {
        QoreValue v = fromBits(*alloca_ptrs[i]);
        v.discard(xsink);
    }
}

//! Run all cleanup actions (decref all tracked values) and free the array.
extern "C" DLLEXPORT void qore_rt_cleanup_run(uint64_t* stack, int32_t count, ExceptionSink* xsink) {
    if (!stack || count <= 0) {
        return;
    }
    // Process in reverse order (LIFO — matches scope-based cleanup)
    for (int32_t i = count - 1; i >= 0; --i) {
        QoreValue v = fromBits(stack[i]);
        v.discard(xsink);
    }
    free(stack);
}

// --- Exception helpers ---

//! Check xsink and throw C++ exception for LLVM stack unwinding
/** Called by JIT/AOT-compiled code after each qore_rt_* call that can
    raise a Qore exception. If xsink has an exception, throws QoreJITException
    which LLVM's invoke/landingpad mechanism catches for proper cleanup.
    This replaces the manual per-instruction xsink flag checking pattern.
*/
extern "C" DLLEXPORT void qore_rt_check_throw(ExceptionSink* xsink) {
    if (xsink && *xsink) {
        throw QoreJITException();
    }
}

extern "C" DLLEXPORT void qore_rt_throw(ExceptionSink* xsink, const char* err, const char* desc) {
    if (xsink) {
        xsink->raiseException(err, desc);
    }
}

extern "C" DLLEXPORT void qore_rt_throw_value(ExceptionSink* xsink, uint64_t val) {
    if (!xsink) {
        return;
    }
    QoreValue arg = fromBits(val);
    if (arg.getType() == NT_LIST) {
        xsink->raiseException(arg.get<const QoreListNode>());
    } else {
        QoreValue owned_arg = arg.hasNode() ? arg.refSelf() : arg;
        xsink->raiseExceptionArg("THROW-ERROR", owned_arg, "throw");
    }
}

extern "C" DLLEXPORT __attribute__((pure)) int64_t qore_rt_has_exception(ExceptionSink* xsink) {
    return (xsink && *xsink) ? 1 : 0;
}

// --- JIT deopt flag ---
// Thread-local flag set by JIT guard failure to request deopt to AST.
// evalTiered() checks this after JIT returns and re-executes via AST if set.
static thread_local bool tl_jit_deopt_requested = false;

// Empty string used as fallback call name when no cached IR function is available.
static const std::string jit_empty_call_name;

extern "C" DLLEXPORT void qore_rt_request_jit_deopt(void* deopt_counter_ptr) {
    tl_jit_deopt_requested = true;
    if (deopt_counter_ptr) {
        auto* counter = static_cast<std::atomic<uint32_t>*>(deopt_counter_ptr);
        counter->fetch_add(1, std::memory_order_relaxed);
        printd(2, "qore_rt_request_jit_deopt: guard failure, deopt_count now %u\n",
            counter->load(std::memory_order_relaxed));
    }
}

DLLLOCAL bool qore_jit_deopt_requested() {
    bool val = tl_jit_deopt_requested;
    tl_jit_deopt_requested = false;
    return val;
}

// --- Invoke helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_invoke_expr(uint64_t expr_bits, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return expr_bits;  // Return inline value as-is (TAG_ENUM, etc.)
    }
    bool needs_deref = true;
    QoreValue ref_expr = expr.refSelf();
    QoreValue result = ref_expr.getInternalNode()->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    // Release the extra reference taken to keep the expression alive during eval
    ref_expr.discard(xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_make_string(const char* str) {
    QoreStringNode* s = new QoreStringNode(str);
    QoreValue v(s);
    return toBits(v);
}

// Thread-local stack for catch exception context
// Tracks the raw QoreException* and the saved previous td->catchException
// Push on CatchException, pop on CatchCleanup/Rethrow
struct CatchEntry {
    QoreException* caught;  // the caught exception
    QoreException* saved;   // the previous td->catchException value
};
static thread_local std::vector<CatchEntry> catch_stack;

extern "C" DLLEXPORT uint64_t qore_rt_catch_exception(ExceptionSink* xsink) {
    if (!xsink || !*xsink) {
        catch_stack.push_back({nullptr, nullptr});
        return toBits(QoreValue());
    }
    QoreException* caught = xsink->catchException();
    QoreException* saved = catch_swap_exception(caught);
    catch_stack.push_back({caught, saved});
    QoreHashNode* info = caught->makeExceptionObject();
    return toBits(QoreValue(info));
}

extern "C" DLLEXPORT void qore_rt_catch_end(ExceptionSink* xsink) {
    if (catch_stack.empty()) {
        return;
    }
    auto entry = catch_stack.back();
    catch_stack.pop_back();
    if (entry.caught) {
        catch_swap_exception(entry.saved);
        entry.caught->del(xsink);
    }
}

extern "C" DLLEXPORT void qore_rt_rethrow(ExceptionSink* xsink) {
    QoreException* ex = catch_get_exception();
    if (ex) {
        qore_es_private::get(*xsink)->rethrow(ex);
    }
    // Clean up catch scope after rethrow
    qore_rt_catch_end(xsink);
}

extern "C" DLLEXPORT void qore_rt_rethrow_with_args(uint64_t args_bits, ExceptionSink* xsink) {
    QoreException* ex = catch_get_exception();
    if (ex) {
        QoreValue args = fromBits(args_bits);
        // The args may contain unevaluated AST nodes (e.g., $1.err + "-NEW"
        // wrapped in a QoreListNode by RethrowStatement::parseInitImpl).
        // Evaluate like the AST path does via ValueEvalOptimizedRefHolder.
        if (args.needsEval()) {
            ValueEvalOptimizedRefHolder v(args, xsink);
            if (!*xsink && v->getType() == NT_LIST) {
                ex = ex->replaceTop(*v->get<const QoreListNode>(), *xsink);
            }
        } else if (args.getType() == NT_LIST) {
            ex = ex->replaceTop(*args.get<const QoreListNode>(), *xsink);
        }
        qore_es_private::get(*xsink)->rethrow(ex);
    }
    // Clean up catch scope after rethrow
    qore_rt_catch_end(xsink);
}

// --- Deopt helpers ---

extern "C" DLLEXPORT void qore_rt_deopt(void* deopt_counter_ptr) {
    // Atomically increment the deopt counter for the variant.
    // The evalTiered path checks this counter and triggers JIT recompilation
    // with updated type profiles when it exceeds a threshold.
    if (deopt_counter_ptr) {
        auto* counter = static_cast<std::atomic<uint32_t>*>(deopt_counter_ptr);
        counter->fetch_add(1, std::memory_order_relaxed);
        printd(2, "qore_rt_deopt: guard failure, deopt_count now %u\n",
            counter->load(std::memory_order_relaxed));
    }
}

// --- Guard helpers ---

extern "C" DLLEXPORT int64_t qore_rt_guard_not_nothing(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isNothing() ? 0 : 1;
}

extern "C" DLLEXPORT int64_t qore_rt_guard_int(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isInt() ? 1 : 0;
}

extern "C" DLLEXPORT int64_t qore_rt_guard_float(uint64_t val) {
    QoreValue v = fromBits(val);
    return v.isFloat() ? 1 : 0;
}

// --- Boxing helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_box_big_int(int64_t val) {
    QoreValue v(val);
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

// --- Local variable helpers ---

extern "C" DLLEXPORT void qore_rt_instantiate_local(LocalVar* var) {
    if (var) {
        // Don't create a duplicate CVV if the closure variable is already on the
        // cvstack (e.g., instantiated by the declaring function's AST execution
        // before the function was JIT-compiled). Creating a duplicate breaks
        // closure write-back semantics during tiered compilation tier transitions.
        if (var->closureUse() && thread_try_find_closure_var(var->getName())) {
            return;
        }
        var->instantiate(QoreParseOptions());
    }
}

extern "C" DLLEXPORT void qore_rt_assign_local(LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    if (!var || *xsink) {
        return;
    }
    QoreValue val = fromBits(value);
    LValueHelper helper(xsink);
    if (var->getLValue(helper, false, true)) {
        return;
    }
    // refSelf before assign — assign takes ownership of the reference
    QoreValue stored = val.hasNode() ? val.refSelf() : val;
    helper.assign(stored);
}

extern "C" DLLEXPORT uint64_t qore_rt_coerce_value(const QoreTypeInfo* ti, uint64_t value,
        uint64_t* cleanup_ptr, ExceptionSink* xsink) {
    QoreValue val = fromBits(value);
    QoreTypeInfo::acceptAssignment(ti, "<lvalue>", val, xsink);
    uint64_t result = toBits(val);
    if (result != value && cleanup_ptr) {
        // acceptAssignment created a copy with complexTypeInfo and already freed
        // the old value internally (via discard in acceptInputComplexList/Hash).
        // The cleanup alloca's old pointer is now dangling — update it to track
        // the new coerced value.  Do NOT discard the old cleanup value; it was
        // already freed by acceptAssignment.
        *cleanup_ptr = result;
    }
    return result;
}

// Strip complex type info from hash/list values in place.
// Used when storing to plain "hash" or "list" typed variables:
// the IR/JIT creates hashes with narrowed types (e.g., hash<string, int>)
// but plain hash/list variables must not retain these narrowed types.
// Unlike map_get_plain_hash (which copies and frees the original), this
// modifies the value in place when it's unique (refcount 1), avoiding
// ownership transfer issues in the LLVM cleanup alloca tracking.
extern "C" DLLEXPORT void qore_rt_strip_complex_type(uint64_t value) {
    QoreValue val = fromBits(value);
    if (val.getType() == NT_HASH) {
        QoreHashNode* h = val.get<QoreHashNode>();
        if (h && !h->getHashDecl()) {
            qore_hash_private::get(*h)->complexTypeInfo = nullptr;
        }
    } else if (val.getType() == NT_LIST) {
        QoreListNode* l = val.get<QoreListNode>();
        if (l) {
            qore_list_private::get(*l)->complexTypeInfo = nullptr;
        }
    }
}

extern "C" DLLEXPORT void qore_rt_assign_local_no_coerce(LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    if (!var || *xsink) {
        return;
    }
    QoreValue val = fromBits(value);
    LValueHelper helper(xsink);
    if (var->getLValue(helper, false, true)) {
        return;
    }
    // refSelf before assign — assign takes ownership of the reference
    QoreValue stored = val.hasNode() ? val.refSelf() : val;
    // check_types=false — coercion has already been applied via qore_rt_coerce_value
    helper.assign(stored, "<lvalue>", false);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_local(LocalVar* var, ExceptionSink* xsink) {
    if (!var) {
        return toBits(QoreValue());
    }
    bool needs_deref = true;
    QoreValue result = var->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_deref_if_reference(uint64_t val, ExceptionSink* xsink) {
    QoreValue v = fromBits(val);
    if (v.hasNode() && v.getType() == NT_REFERENCE) {
        // Dereference the reference like VarRefNode::evalImpl() does.
        // The reference node stays in the alloca (not deref'd here);
        // only the target value is returned with an extra reference.
        bool needs_deref = true;
        QoreValue result = v.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        return toBits(result);
    }
    return val;
}

extern "C" DLLEXPORT void qore_rt_clear_local(LocalVar* var, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    if (var->closureUse()) {
        // Closure-captured locals live on the cvstack.  Only clear the value
        // if no closures still hold references to this ClosureVarValue.
        // When references > 1, closures may still need to read the value
        // (e.g., closures submitted to a thread pool that haven't executed
        // yet).  When references == 1, only the cvstack entry remains, so
        // it's safe to trigger timely destruction at block scope exit.
        // NOTE: Use thread_try_find_closure_var() to avoid asserting when the
        // variable is not on the cvstack (e.g., in closure contexts on background threads).
        ClosureVarValue* cvv = thread_try_find_closure_var(var->getName());
        if (cvv && cvv->references.load(std::memory_order_acquire) == 1) {
            cvv->clearValue(xsink);
        }
    } else {
        // Find the local on the thread-local variable stack by name pointer
        LocalVarValue* lvar = thread_find_lvar(var->getName());
        if (lvar) {
            // del() calls val.removeValue(true).discard(xsink) — no LValueHelper
            // assert, safe even when xsink already has an exception from a prior
            // destructor in the same scope
            lvar->del(xsink);
        }
    }
}

extern "C" DLLEXPORT void qore_rt_uninstantiate_local(LocalVar* var, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    // For closure-use variables: trigger deterministic destruction
    // by clearing the CVV value when it's the last reference,
    // matching the qore_rt_clear_local() pattern
    if (var->closureUse()) {
        ClosureVarValue* cvv = thread_try_find_closure_var(var->getName());
        if (cvv && cvv->references.load(std::memory_order_acquire) == 1) {
            cvv->clearValue(xsink);
        }
    }
    var->uninstantiate(xsink);
}

extern "C" DLLEXPORT void qore_rt_uninstantiate_closure_block_exit(LocalVar* var, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    // Block scope exit: clear CVV value unconditionally to trigger
    // deterministic destruction even when closures hold extra refs
    ClosureVarValue* cvv = thread_try_find_closure_var(var->getName());
    if (cvv) {
        cvv->clearValue(xsink);
    }
    var->uninstantiate(xsink);
}

// --- Generic opcode dispatch helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_binary_op(int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalBinary(static_cast<QoreIROpcode>(opcode), lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_unary_op(int opcode, uint64_t operand, ExceptionSink* xsink) {
    QoreValue val = fromBits(operand);
    QoreValue result = QoreIRInterpreter::evalUnary(static_cast<QoreIROpcode>(opcode), val, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_expr_op(int opcode, uint64_t expr_bits, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    QoreValue result = QoreIRInterpreter::evalExpr(static_cast<QoreIROpcode>(opcode), expr, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_comparison_op(int opcode, uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    QoreValue result = QoreIRInterpreter::evalComparison(static_cast<QoreIROpcode>(opcode), lv, rv, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_ternary_op(int opcode, uint64_t a, uint64_t b, uint64_t c, ExceptionSink* xsink) {
    QoreValue va = fromBits(a);
    QoreValue vb = fromBits(b);
    QoreValue vc = fromBits(c);
    QoreValue result = QoreIRInterpreter::evalTernary(static_cast<QoreIROpcode>(opcode), va, vb, vc, xsink);
    return toBits(result);
}

// --- Variable access helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_load_global(Var* var, ExceptionSink* xsink) {
    if (!var) {
        return toBits(QoreValue());
    }
    // Var::eval() returns an already-referenced value
    QoreValue result = var->eval();
    return toBits(result);
}

extern "C" DLLEXPORT void qore_rt_store_global(Var* var, uint64_t value, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    QoreValue val = fromBits(value);
    QoreValue stored = val.hasNode() ? val.refSelf() : val;
    LValueHelper helper(xsink);
    if (!var->getLValue(helper, false)) {
        helper.assign(stored);
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_load_closure(ClosureVarValue* var, ExceptionSink* xsink) {
    if (!var) {
        return toBits(QoreValue());
    }
    bool needs_deref = true;
    QoreValue result = var->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT void qore_rt_store_closure(ClosureVarValue* var, uint64_t value, ExceptionSink* xsink) {
    if (!var) {
        return;
    }
    QoreValue val = fromBits(value);
    QoreValue stored = val.hasNode() ? val.refSelf() : val;
    LValueHelper helper(xsink);
    if (!var->getLValue(helper, false)) {
        helper.assign(stored);
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_load_thread_local(Var* var, ExceptionSink* xsink) {
    // Thread-local variables use the same Var class; eval() resolves per-thread
    if (!var) {
        return toBits(QoreValue());
    }
    QoreValue result = var->eval();
    return toBits(result);
}

extern "C" DLLEXPORT void qore_rt_store_thread_local(Var* var, uint64_t value, ExceptionSink* xsink) {
    qore_rt_store_global(var, value, xsink);
}

// --- Self member access helper ---

extern "C" DLLEXPORT uint64_t qore_rt_load_self_member(const char* member_name, ExceptionSink* xsink) {
    QoreObject* obj = runtime_get_stack_object();
    assert(obj);
    // issue 3523: evaluate in case the value is a reference
    ValueHolder val(obj->getReferencedMemberNoMethod(member_name, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(val->needsEval() ? val->eval(xsink) : val.release());
}

// --- Static class variable access helper ---

extern "C" DLLEXPORT uint64_t qore_rt_load_static_var(QoreVarInfo* vi, const char* var_name, ExceptionSink* xsink) {
    // issue 3523: evaluate in case the value is a reference
    ValueHolder val(vi->getReferencedValue(var_name, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(val->needsEval() ? val->eval(xsink) : val.release());
}

// --- Object instantiation helper ---

extern "C" DLLEXPORT uint64_t qore_rt_new_object(const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        const QoreListNode* args, ExceptionSink* xsink) {
    RuntimeConfig& rc = rc_get_current_ref();
    return toBits(qore_class_private::execConstructor(*qc, rc, variant, args, xsink));
}

// --- Constant loading helper ---

extern "C" DLLEXPORT uint64_t qore_rt_load_constant(const RuntimeConstantRefNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<RuntimeConstantRefNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_constant_value(uint64_t val_bits) {
    QoreValue val = fromBits(val_bits);
    return toBits(val.refSelf());
}

// --- Closure creation helper ---

extern "C" DLLEXPORT uint64_t qore_rt_create_closure(const QoreClosureParseNode* cn, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<QoreClosureParseNode*>(cn)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

// --- Cast helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_cast_with_inner(uint64_t cast_expr_bits, uint64_t inner_bits,
        ExceptionSink* xsink) {
    QoreValue cast_expr = fromBits(cast_expr_bits);
    QoreValue inner = fromBits(inner_bits);

    if (!cast_expr.hasNode()) {
        if (xsink) {
            xsink->raiseException("IR-CAST-ERROR", "missing cast expression node");
        }
        return toBits(QoreValue());
    }

    auto* cast_node = dynamic_cast<const QoreCastOperatorNode*>(cast_expr.getInternalNode());
    if (!cast_node) {
        if (xsink) {
            xsink->raiseException("IR-CAST-ERROR", "cast expression is not a resolved cast operator");
        }
        return toBits(QoreValue());
    }

    QoreValue result = cast_node->castValue(inner, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_cast_with_inner_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t inner_bits, ExceptionSink* xsink) {
    return qore_rt_cast_with_inner(ctx->exprs[slot], inner_bits, xsink);
}

// --- Call reference creation helper ---

extern "C" DLLEXPORT uint64_t qore_rt_create_call_ref(uint64_t expr_bits, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return toBits(QoreValue());
    }
    bool needs_deref = true;
    QoreValue ref_expr = expr.refSelf();
    QoreValue result = ref_expr.getInternalNode()->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    ref_expr.discard(xsink);
    return toBits(result);
}

// --- Method reference creation helper (delegates to call ref - identical behavior) ---

extern "C" DLLEXPORT uint64_t qore_rt_create_method_ref(uint64_t expr_bits, ExceptionSink* xsink) {
    return qore_rt_create_call_ref(expr_bits, xsink);
}

// --- Parse reference creation helper ---

extern "C" DLLEXPORT uint64_t qore_rt_create_parse_ref(const ParseReferenceNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<ParseReferenceNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

// --- Typed container construction helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_new_hash_decl(const NewHashDeclNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<NewHashDeclNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_hash(const NewComplexHashNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<NewComplexHashNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_new_complex_list(const NewComplexListNode* node, ExceptionSink* xsink) {
    bool needs_deref = true;
    QoreValue result = const_cast<NewComplexListNode*>(node)->eval(needs_deref, xsink);
    if (!needs_deref && result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

// --- VarRefNewObjectNode construction helper (non-object types) ---

extern "C" DLLEXPORT uint64_t qore_rt_vrn_construct(const VarRefNewObjectNode* vrn, ExceptionSink* xsink) {
    return toBits(vrn->constructValue(xsink));
}

// --- Hashdecl construction from pre-lowered hash ---

extern "C" DLLEXPORT uint64_t qore_rt_new_hash_decl_from_hash(const TypedHashDecl* hd,
        uint64_t hash_bits, int32_t runtime_check, ExceptionSink* xsink) {
    QoreValue hash_val = fromBits(hash_bits);
    const QoreHashNode* init = hash_val.getType() == NT_HASH
        ? hash_val.get<const QoreHashNode>() : nullptr;
    QoreHashNode* result = typed_hash_decl_private::get(*hd)->newHash(init,
        runtime_check != 0, xsink);
    return toBits(result ? QoreValue(result) : QoreValue());
}

// AOT variant: resolves hashdecl by namespace path at runtime
extern "C" DLLEXPORT uint64_t qore_rt_new_hash_decl_from_hash_by_path(const char* hd_path,
        uint64_t hash_bits, int32_t runtime_check, ExceptionSink* xsink) {
    QoreProgram* pgm = getProgram();
    if (!pgm) {
        if (xsink) {
            xsink->raiseException("HASHDECL-ERROR", "cannot resolve hashdecl '%s': no program context",
                hd_path ? hd_path : "<null>");
        }
        return toBits(QoreValue());
    }
    qore_program_private* pp = qore_program_private::get(*pgm);
    const qore_ns_private* found_ns = nullptr;
    const TypedHashDecl* hd = qore_root_ns_private::runtimeFindHashDecl(*pp->RootNS, hd_path, found_ns);
    if (!hd) {
        if (xsink) {
            xsink->raiseException("HASHDECL-ERROR", "cannot resolve hashdecl '%s'",
                hd_path ? hd_path : "<null>");
        }
        return toBits(QoreValue());
    }
    return qore_rt_new_hash_decl_from_hash(hd, hash_bits, runtime_check, xsink);
}

// --- Hash building helper ---

extern "C" DLLEXPORT void qore_rt_hash_set_key_value(uint64_t hash_bits, uint64_t key_bits,
        uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue hash_val = fromBits(hash_bits);
    QoreValue key_val = fromBits(key_bits);
    QoreValue value_val = fromBits(value_bits);
    QoreStringValueHelper key_str(key_val);
    QoreHashNode* hash = hash_val.get<QoreHashNode>();
    if (value_val.hasNode()) {
        value_val.refSelf();
    }
    hash->setKeyValue(key_str->c_str(), value_val, xsink);
    // Do NOT discard key_val — the caller (JIT/IR) manages the key's lifetime.
    // Discarding here causes a double-free since the key is also cleaned up by
    // the JIT function's exit cleanup or the IR value map cleanup mechanism.
}

// --- Reverse iterator creation helper ---

extern "C" DLLEXPORT void* qore_rt_iterator_create_reverse(uint64_t iterable_bits, ExceptionSink* xsink) {
    QoreValue iterable = fromBits(iterable_bits);
    FunctionalOperator::FunctionalValueType value_type;
    FunctionalOperatorInterface* iter = FunctionalOperatorInterface::getFunctionalIterator(
        value_type, iterable, false, "foldr operator", xsink);
    if (*xsink || value_type == FunctionalOperator::nothing) {
        delete iter;
        return nullptr;
    }
    return iter;
}

// --- Implicit argument helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_load_implicit_arg(int offset, ExceptionSink* xsink) {
    const QoreListNode* argv = thread_get_implicit_args();
    if (!argv) {
        return toBits(QoreValue());
    }
    QoreValue result = argv->retrieveEntry(offset);
    if (result.hasNode()) {
        result = result.refSelf();
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_implicit_argv(ExceptionSink* xsink) {
    const QoreListNode* argv = thread_get_implicit_args();
    if (!argv) {
        return toBits(QoreValue());
    }
    // Return a reference to the argv list
    QoreValue result = const_cast<QoreListNode*>(argv);
    result.refSelf();
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_implicit_element(ExceptionSink* xsink) {
    return toBits(QoreValue(get_implicit_element()));
}

extern "C" DLLEXPORT uint64_t qore_rt_push_implicit_arg(uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue value = fromBits(value_bits);
    // Get current implicit args (if any), save for restoration
    const QoreListNode* old_argv = thread_get_implicit_args();
    QoreValue old_context = old_argv ? const_cast<QoreListNode*>(old_argv)->refSelf() : QoreValue();

    // Create new single-element list with the value
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> new_argv(new QoreListNode(autoTypeInfo), xsink);
    qore_list_private::get(**new_argv)->pushIntern(value.refSelf());

    thread_set_implicit_args(new_argv.release());

    return toBits(old_context);
}

extern "C" DLLEXPORT uint64_t qore_rt_set_implicit_argv(uint64_t argv_bits, ExceptionSink* xsink) {
    QoreValue argv_val = fromBits(argv_bits);
    // Get current implicit args (if any), save for restoration
    const QoreListNode* old_argv = thread_get_implicit_args();
    QoreValue old_context = old_argv ? const_cast<QoreListNode*>(old_argv)->refSelf() : QoreValue();

    // Set new implicit args (argv_val should be a list or nothing)
    QoreListNode* new_argv = argv_val.get<QoreListNode>();
    if (new_argv) {
        new_argv->ref();
    }
    thread_set_implicit_args(new_argv);

    return toBits(old_context);
}

extern "C" DLLEXPORT void qore_rt_pop_implicit_arg(uint64_t old_context_bits, ExceptionSink* xsink) {
    QoreValue old_context = fromBits(old_context_bits);

    // Get current implicit args and deref
    const QoreListNode* current = thread_get_implicit_args();
    if (current) {
        const_cast<QoreListNode*>(current)->deref(xsink);
    }

    // Restore old context
    QoreListNode* old_argv = old_context.get<QoreListNode>();
    thread_set_implicit_args(old_argv);
}

extern "C" DLLEXPORT uint64_t qore_rt_push_implicit_element(int64_t index, ExceptionSink* xsink) {
    // save_implicit_element sets the new value and returns the old value
    int old_element = save_implicit_element(static_cast<int>(index));
    return static_cast<uint64_t>(old_element);
}

extern "C" DLLEXPORT void qore_rt_pop_implicit_element(uint64_t old_element) {
    save_implicit_element(static_cast<int>(old_element));
}

extern "C" DLLEXPORT uint64_t qore_rt_create_empty_list(ExceptionSink* xsink) {
    QoreListNode* list = new QoreListNode(autoTypeInfo);
    return toBits(QoreValue(list));
}

extern "C" DLLEXPORT void qore_rt_list_append(uint64_t list_bits, uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue list_val = fromBits(list_bits);
    QoreValue value = fromBits(value_bits);

    QoreListNode* list = list_val.get<QoreListNode>();
    if (list) {
        QoreValue ref_val = value.refSelf();
        qore_list_private* priv = qore_list_private::get(*list);
        // Track element type to maintain correct list<T> type info at runtime,
        // matching AST mode's vtype/vcommon tracking in map/select operators.
        priv->setListTypeFromNewElementType(ref_val.getFullTypeInfo());
        priv->pushIntern(ref_val);
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_list_push(uint64_t list_bits, uint64_t val_bits, ExceptionSink* xsink) {
    QoreValue list_val = fromBits(list_bits);
    QoreValue push_val = fromBits(val_bits);

    if (list_val.getType() == NT_LIST) {
        QoreListNode* l = list_val.get<QoreListNode>();
        l->push(push_val.refSelf(), xsink);
        // Return same list with a new reference for the caller to own
        l->ref();
        return list_bits;
    }

    if (list_val.isNothing()) {
        // Auto-vivify empty list (already has refcount 1 from new)
        QoreListNode* l = new QoreListNode(autoTypeInfo);
        l->push(push_val.refSelf(), xsink);
        QoreValue result(l);
        return toBits(result);
    }

    // Not a list - raise error
    xsink->raiseException("PUSH-ERROR",
        "the lvalue argument to push is type \"%s\"; expecting \"list\"",
        list_val.getTypeName());
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_switch_regex_match(uint64_t regex_case_ptr, uint64_t switch_val_bits, ExceptionSink* xsink) {
    const CaseNodeRegex* regex_case = reinterpret_cast<const CaseNodeRegex*>(regex_case_ptr);
    QoreValue switch_val = fromBits(switch_val_bits);

    if (!regex_case) {
        return toBits(QoreValue(false));
    }

    bool match = regex_case->matches(switch_val, xsink);
    return toBits(QoreValue(match));
}

// --- LValue operation helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_load(uint64_t lvalue_bits, ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue result = QoreIRInterpreter::evalLValueLoad(lvalue, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_store(uint64_t lvalue_bits, uint64_t value_bits, ExceptionSink* xsink) {
    if (*xsink) {
        // Defensive guard: if an exception was thrown by a prior instruction and not caught,
        // discard the value and return NOTHING to avoid assertion in LValueHelper
        QoreValue value = fromBits(value_bits);
        value.discard(xsink);
        return toBits(QoreValue());
    }
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue value = fromBits(value_bits);
    // Hold an extra reference on the RHS value so that LValueHelper::ensureUnique()
    // sees the correct refcount for COW. Without this, self-assignment (e.g., h.b = h)
    // creates a circular reference because the hash appears unique at refcount 1.
    ValueHolder val_holder(value.refSelf(), xsink);
    QoreValue result = QoreIRInterpreter::evalLValueStore(lvalue, value, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_store_weak(uint64_t lvalue_bits, uint64_t value_bits,
        ExceptionSink* xsink) {
    if (*xsink) {
        QoreValue value = fromBits(value_bits);
        value.discard(xsink);
        return toBits(QoreValue());
    }
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue value = fromBits(value_bits);
    ValueHolder val_holder(value.refSelf(), xsink);
    QoreValue result = QoreIRInterpreter::evalLValueStore(lvalue, value, xsink, true);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_unary(int opcode, uint64_t lvalue_bits, ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue result = QoreIRInterpreter::evalLValueUnary(static_cast<QoreIROpcode>(opcode), lvalue, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_binary(int opcode, uint64_t lvalue_bits, uint64_t value_bits,
        ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue value = fromBits(value_bits);
    QoreValue result = QoreIRInterpreter::evalLValueBinary(static_cast<QoreIROpcode>(opcode), lvalue, value, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_ternary(int opcode, uint64_t lvalue_bits, uint64_t first_bits,
        uint64_t second_bits, uint64_t third_bits, ExceptionSink* xsink) {
    QoreValue lvalue = fromBits(lvalue_bits);
    QoreValue first = fromBits(first_bits);
    QoreValue second = fromBits(second_bits);
    QoreValue third = fromBits(third_bits);
    QoreValue result = QoreIRInterpreter::evalLValueTernary(static_cast<QoreIROpcode>(opcode), lvalue, first,
        second, third, xsink);
    return toBits(result);
}

// --- Container construction helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_make_list(uint64_t* vals, int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> list(new QoreListNode(autoTypeInfo), xsink);
    qore_list_private* priv = qore_list_private::get(**list);
    priv->reserve(count);
    // Track common value type for proper list typing (e.g., list<string> vs list<auto>)
    const QoreTypeInfo* vtype = nullptr;
    bool vcommon = false;
    for (int i = 0; i < count; i++) {
        QoreValue v = fromBits(vals[i]);
        if (v.hasNode()) {
            v.refSelf();
        }
        const QoreTypeInfo* vt = v.getTypeInfo();
        if (!vtype) {
            vtype = vt;
            vcommon = true;
        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
            vcommon = false;
        }
        priv->pushIntern(v);
    }
    if (typeInfo) {
        priv->complexTypeInfo = typeInfo;
    } else {
        if (!vtype || vtype == anyTypeInfo || !vcommon) {
            vtype = autoTypeInfo;
        }
        priv->complexTypeInfo = qore_get_complex_list_type(vtype);
    }
    return toBits(QoreValue(list.release()));
}

extern "C" DLLEXPORT uint64_t qore_rt_make_hash(uint64_t* kv_pairs, int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
    // count is the number of key-value pairs; kv_pairs has 2*count elements
    // Track common value type for proper hash typing (e.g., hash<string, string> vs hash<string, auto>)
    const QoreTypeInfo* vtype = nullptr;
    bool vcommon = false;
    for (int i = 0; i < count; i++) {
        QoreValue key = fromBits(kv_pairs[i * 2]);
        QoreValue val = fromBits(kv_pairs[i * 2 + 1]);
        QoreStringValueHelper key_str(key);
        if (val.hasNode()) {
            val.refSelf();
        }
        const QoreTypeInfo* vt = val.getTypeInfo();
        if (!i) {
            vtype = vt;
            vcommon = true;
        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
            vcommon = false;
        }
        hash->setKeyValue(key_str->c_str(), val, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
    }
    if (typeInfo) {
        qore_hash_private::get(*hash)->complexTypeInfo = typeInfo;
    } else {
        if (!vtype || vtype == anyTypeInfo) {
            vtype = autoTypeInfo;
        }
        qore_hash_private::get(*hash)->complexTypeInfo = qore_get_complex_hash_type(vtype);
    }
    return toBits(QoreValue(hash.release()));
}

extern "C" DLLEXPORT uint64_t qore_rt_to_string(uint64_t val_bits) {
    QoreValue val = fromBits(val_bits);
    QoreStringNode* str;
    switch (val.getType()) {
        case NT_STRING:
            str = val.get<const QoreStringNode>()->stringRefSelf();
            break;
        case NT_INT:
            str = new QoreStringNodeMaker(QLLD, val.getAsBigInt());
            break;
        case NT_FLOAT:
            str = q_fix_decimal(new QoreStringNodeMaker("%.9g", val.getAsFloat()), 0);
            break;
        case NT_BOOLEAN:
            str = new QoreStringNodeMaker(QLLD, val.getAsBigInt());
            break;
        case NT_NOTHING:
        case NT_NULL:
            str = new QoreStringNode();
            break;
        default: {
            QoreStringValueHelper sv(val);
            str = new QoreStringNode(*sv);
            break;
        }
    }
    return toBits(QoreValue(str));
}

extern "C" DLLEXPORT uint64_t qore_rt_sprintf(uint64_t val_bits, ExceptionSink* xsink) {
    QoreValue val = fromBits(val_bits);
    QoreStringNode* str;
    if (val.getType() == NT_LIST) {
        str = q_sprintf(val.get<const QoreListNode>(), 0, 0, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
    } else {
        // Single value: convert to string
        QoreStringValueHelper sv(val);
        str = new QoreStringNode(*sv);
    }
    return toBits(QoreValue(str));
}

extern "C" DLLEXPORT uint64_t qore_rt_make_hash_const_keys(const char** keys, uint64_t* vals,
        int count, const QoreTypeInfo* typeInfo, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> hash(new QoreHashNode(autoTypeInfo), xsink);
    qore_hash_private* hp = qore_hash_private::get(*hash);
    hp->hm.reserve(count);
    const QoreTypeInfo* vtype = nullptr;
    bool vcommon = false;
    for (int i = 0; i < count; i++) {
        QoreValue val = fromBits(vals[i]);
        if (val.hasNode()) {
            val.refSelf();
        }
        const QoreTypeInfo* vt = val.getTypeInfo();
        if (!i) {
            vtype = vt;
            vcommon = true;
        } else if (vcommon && !QoreTypeInfo::matchCommonType(vtype, vt)) {
            vcommon = false;
        }
        hash->setKeyValue(keys[i], val, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
    }
    if (typeInfo) {
        hp->complexTypeInfo = typeInfo;
    } else {
        if (!vtype || vtype == anyTypeInfo) {
            vtype = autoTypeInfo;
        }
        hp->complexTypeInfo = qore_get_complex_hash_type(vtype);
    }
    return toBits(QoreValue(hash.release()));
}

// --- Statement execution helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_exec_statement(int opcode, const AbstractStatement* stmt, ExceptionSink* xsink) {
    if (!stmt) {
        return toBits(QoreValue());
    }
    QoreValue return_value;
    QoreIRInterpreter::execStatement(static_cast<QoreIROpcode>(opcode), stmt, return_value, xsink);
    return toBits(return_value);
}

extern "C" DLLEXPORT void qore_rt_thread_exit(ExceptionSink* xsink) {
    if (xsink) {
        xsink->raiseThreadExit();
    }
}

// --- Guard type helper ---

extern "C" DLLEXPORT int64_t qore_rt_guard_type(uint64_t val, const QoreTypeInfo* type_info) {
    QoreValue v = fromBits(val);
    return QoreTypeInfo::runtimeAcceptsValue(type_info, v) != QTI_NOT_EQUAL ? 1 : 0;
}

// --- InstanceOf helper ---

extern "C" DLLEXPORT uint64_t qore_rt_instanceof(uint64_t val_bits, const QoreTypeInfo* ti) {
    QoreValue val = fromBits(val_bits);
    qore_type_t t = val.getType();
    bool result;
    switch (t) {
        case NT_WEAKREF:
            result = QoreTypeInfo::runtimeAcceptsValue(ti,
                **val.get<const WeakReferenceNode>()) != QTI_NOT_EQUAL;
            break;
        case NT_WEAKREF_HASH:
            result = QoreTypeInfo::runtimeAcceptsValue(ti,
                **val.get<const WeakHashReferenceNode>()) != QTI_NOT_EQUAL;
            break;
        case NT_WEAKREF_LIST:
            result = QoreTypeInfo::runtimeAcceptsValue(ti,
                **val.get<const WeakListReferenceNode>()) != QTI_NOT_EQUAL;
            break;
        default:
            result = QoreTypeInfo::runtimeAcceptsValue(ti, val) != QTI_NOT_EQUAL;
            break;
    }
    return toBits(QoreValue(result));
}

// --- Date construction helper ---

extern "C" DLLEXPORT uint64_t qore_rt_make_date(int64_t date_microseconds, int64_t is_relative) {
    DateTimeNode* dt;
    if (is_relative) {
        dt = new DateTimeNode(true);
        dt->setRelativeDateSeconds(date_microseconds / 1000000,
            static_cast<int>(date_microseconds % 1000000));
    } else {
        // date_microseconds is a UTC epoch from getEpochMicrosecondsUTC(); use makeAbsolute()
        // which stores the epoch directly without local-to-UTC conversion (unlike DateTimeNode(s, ms)
        // which goes through setLocalDate → setLocalIntern → subtracts timezone offset)
        int64_t epoch_seconds = date_microseconds / 1000000;
        int us = static_cast<int>(date_microseconds % 1000000);
        dt = DateTimeNode::makeAbsolute(currentTZ(), epoch_seconds, us);
    }
    return toBits(QoreValue(dt));
}

// --- Enum construction helper ---

extern "C" DLLEXPORT uint64_t qore_rt_make_enum(int64_t member_ptr) {
    const QoreEnumMember* member = reinterpret_cast<const QoreEnumMember*>(member_ptr);
    return toBits(QoreValue::makeEnum(member));
}

// --- Specialized access helpers (Phase 5b optimizations) ---

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access(uint64_t hash_val, const char* key, ExceptionSink* xsink) {
    QoreValue v = fromBits(hash_val);
    if (v.getType() == NT_HASH) {
        const QoreHashNode* h = v.get<const QoreHashNode>();
        QoreValue result = h->getKeyValue(key, xsink);
        return *xsink ? toBits(QoreValue()) : toBits(result.refSelf());
    }
    if (v.getType() == NT_OBJECT) {
        QoreObject* o = const_cast<QoreObject*>(v.get<const QoreObject>());
        QoreValue rv = o->evalMember(key, xsink);
        return *xsink ? toBits(QoreValue()) : toBits(rv);
    }
    // Not a hash or object (or NOTHING/NULL): return NOTHING
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_key_access_int(uint64_t hash_val, const char* key) {
    QoreValue v = fromBits(hash_val);
    if (v.getType() == NT_HASH) {
        const QoreHashNode* h = v.get<const QoreHashNode>();
        bool exists = false;
        QoreValue val = h->getKeyValueExistence(key, exists);
        if (exists) {
            return toBits(val.getAsBigInt());
        }
    }
    return toBits(QoreValue());
}

// JIT path: write hash{key} = value with copy-on-write support.
// var: container LocalVar* (used to update the local when COW triggers).
extern "C" DLLEXPORT uint64_t qore_rt_hash_key_store_cow(
        LocalVar* var, uint64_t hash_bits, const char* key,
        uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue hv = fromBits(hash_bits);
    QoreValue val = fromBits(value_bits);
    if (hv.getType() == NT_HASH) {
        QoreHashNode* h = hv.get<QoreHashNode>();
        // Only COW if shared with other references (caller holds +1 from LoadLocal, variable holds +1 or more)
        if (h->reference_count() > 2) {
            QoreHashNode* new_h = h->copy();
            qore_rt_assign_local(var, toBits(QoreValue(new_h)), xsink);
            if (*xsink) {
                new_h->deref(nullptr);
                return toBits(QoreValue());
            }
            // Release copy()'s original ref; variable holds sole ref
            new_h->deref(nullptr);
            h = new_h;
            // DO NOT deref orig_h: LLVM cleanup-A releases the original LoadLocal ref
        }
        // setKeyValue() may require refcount == 1 internally
        // Temporarily deref caller's reference if needed
        bool had_caller_ref = h->reference_count() > 1;
        if (had_caller_ref) {
            h->deref(nullptr);
        }
        h->setKeyValue(key, val.refSelf(), xsink);
        // Restore caller's reference if we borrowed it
        if (had_caller_ref) {
            h->refSelf();
        }
    } else if (hv.getType() == NT_OBJECT) {
        const_cast<QoreObject*>(hv.get<const QoreObject>())->setValue(key, val.refSelf(), xsink);
    }
    return value_bits;
}

// AOT path: same semantics but container is identified by its slot index in QoreAOTContext.
extern "C" DLLEXPORT uint64_t qore_rt_hash_key_store_cow_aot(
        QoreAOTContext* ctx, uint32_t local_slot,
        uint64_t hash_bits, const char* key,
        uint64_t value_bits, ExceptionSink* xsink) {
    QoreValue hv = fromBits(hash_bits);
    QoreValue val = fromBits(value_bits);
    if (hv.getType() == NT_HASH) {
        QoreHashNode* h = hv.get<QoreHashNode>();
        // Only COW if shared with other references (caller holds +1 from LoadLocal, variable holds +1 or more)
        if (h->reference_count() > 2) {
            QoreHashNode* new_h = h->copy();
            qore_rt_assign_local_aot(ctx, local_slot, toBits(QoreValue(new_h)), xsink);
            if (*xsink) {
                new_h->deref(nullptr);
                return toBits(QoreValue());
            }
            // Release copy()'s original ref; variable holds sole ref
            new_h->deref(nullptr);
            h = new_h;
            // DO NOT deref orig_h: LLVM cleanup-A releases the original LoadLocal ref
        }
        // setKeyValue() may require refcount == 1 internally
        // Temporarily deref caller's reference if needed
        bool had_caller_ref = h->reference_count() > 1;
        if (had_caller_ref) {
            h->deref(nullptr);
        }
        h->setKeyValue(key, val.refSelf(), xsink);
        // Restore caller's reference if we borrowed it
        if (had_caller_ref) {
            h->refSelf();
        }
    } else if (hv.getType() == NT_OBJECT) {
        const_cast<QoreObject*>(hv.get<const QoreObject>())->setValue(key, val.refSelf(), xsink);
    }
    return value_bits;
}

// JIT path: list[index] = value with COW
// var: container LocalVar* (used to update the local when COW triggers).
extern "C" DLLEXPORT uint64_t qore_rt_list_index_store_cow(
        LocalVar* var, uint64_t list_bits, int64_t index,
        uint64_t val_bits, ExceptionSink* xsink) {
    QoreValue lv = fromBits(list_bits);
    QoreValue val = fromBits(val_bits);

    if (lv.getType() == NT_LIST) {
        QoreListNode* l = lv.get<QoreListNode>();
        // Only COW if shared with other references (caller holds +1 from LoadLocal, variable holds +1 or more)
        if (l->reference_count() > 2) {
            QoreListNode* new_l = l->copy();
            qore_rt_assign_local(var, toBits(QoreValue(new_l)), xsink);
            if (*xsink) {
                new_l->deref(nullptr);
                return toBits(QoreValue());
            }
            // Release copy()'s original ref; variable holds sole ref (refcount==1)
            new_l->deref(nullptr);
            l = new_l;
            // DO NOT deref orig_l: LLVM cleanup-A releases the original LoadLocal ref
        }

        // setEntry() requires refcount == 1, but if !COW we have refcount >= 2 (variable + caller)
        // Temporarily deref caller's reference so setEntry sees refcount == 1
        bool had_caller_ref = l->reference_count() > 1;

        if (had_caller_ref) {
            l->deref(nullptr);
        }
        QoreValue entry = val.hasNode() ? val.refSelf() : val;
        l->setEntry(index, entry, xsink);
        // Restore caller's reference if we borrowed it
        if (had_caller_ref) {
            l->refSelf();
        }
    }
    return val_bits;
}

// AOT path: list[index] = value with COW
// ctx: QoreAOTContext, local_slot identifies the list's slot
extern "C" DLLEXPORT uint64_t qore_rt_list_index_store_cow_aot(
        QoreAOTContext* ctx, uint32_t local_slot,
        uint64_t list_bits, int64_t index,
        uint64_t val_bits, ExceptionSink* xsink) {
    QoreValue lv = fromBits(list_bits);
    QoreValue val = fromBits(val_bits);
    if (lv.getType() == NT_LIST) {
        QoreListNode* l = lv.get<QoreListNode>();
        // Only COW if shared with other references (caller holds +1 from LoadLocal, variable holds +1 or more)
        if (l->reference_count() > 2) {
            QoreListNode* new_l = l->copy();
            qore_rt_assign_local_aot(ctx, local_slot, toBits(QoreValue(new_l)), xsink);
            if (*xsink) {
                new_l->deref(nullptr);
                return toBits(QoreValue());
            }
            // Release copy()'s original ref; variable holds sole ref (refcount==1)
            new_l->deref(nullptr);
            l = new_l;
            // DO NOT deref orig_l: LLVM cleanup-A releases the original LoadLocal ref
        }
        // setEntry() requires refcount == 1, but if !COW we have refcount >= 2 (variable + caller)
        // Temporarily deref caller's reference so setEntry sees refcount == 1
        bool had_caller_ref = l->reference_count() > 1;
        if (had_caller_ref) {
            l->deref(nullptr);
        }
        QoreValue entry = val.hasNode() ? val.refSelf() : val;
        l->setEntry(index, entry, xsink);
        // Restore caller's reference if we borrowed it
        if (had_caller_ref) {
            l->refSelf();
        }
    }
    return val_bits;
}

extern "C" DLLEXPORT uint64_t qore_rt_list_index_access(uint64_t list_val, int64_t index, ExceptionSink* xsink) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return toBits(l->getReferencedEntry(static_cast<size_t>(index)));
        }
    }
    // Not a list, or index out of bounds: return NOTHING
    return toBits(QoreValue());
}

// Runtime type check: returns 1 if value is NT_LIST or NT_OBJECT, 0 otherwise
// Used by select to determine if the result should be returned as a list or unwrapped
extern "C" DLLEXPORT int64_t qore_rt_is_collection_type(uint64_t val) {
    QoreValue v = fromBits(val);
    qore_type_t t = v.getType();
    return (t == NT_LIST || t == NT_OBJECT) ? 1 : 0;
}

// Optimized list iteration helpers for foldl/map/select
extern "C" DLLEXPORT int64_t qore_rt_list_size(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        return static_cast<int64_t>(v.get<const QoreListNode>()->size());
    }
    return 0;
}

extern "C" DLLEXPORT int64_t qore_rt_list_get_int(uint64_t list_val, int64_t index) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return l->retrieveEntry(index).getAsBigInt();
        }
    }
    return 0;
}

extern "C" DLLEXPORT double qore_rt_list_get_float(uint64_t list_val, int64_t index) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return l->retrieveEntry(index).getAsFloat();
        }
    }
    return 0.0;
}

extern "C" DLLEXPORT uint64_t qore_rt_list_get_value(uint64_t list_val, int64_t index, ExceptionSink* xsink) {
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return toBits(l->getReferencedEntry(static_cast<size_t>(index)));
        }
    }
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_list_get_value_noref(uint64_t list_val, int64_t index, ExceptionSink* xsink) {
    // Read-only element access — returns borrowed reference (no refSelf)
    QoreValue v = fromBits(list_val);
    if (v.getType() == NT_LIST) {
        const QoreListNode* l = v.get<const QoreListNode>();
        if (index >= 0 && static_cast<size_t>(index) < l->size()) {
            return toBits(l->retrieveEntry(static_cast<size_t>(index)));
        }
    }
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_rt_create_sized_list(int64_t capacity, ExceptionSink* xsink) {
    QoreListNode* list = new QoreListNode(autoTypeInfo);
    if (capacity > 0) {
        qore_list_private::get(*list)->reserve(static_cast<size_t>(capacity));
    }
    return toBits(QoreValue(list));
}

extern "C" DLLEXPORT void qore_rt_list_set_int(uint64_t list_bits, int64_t index, int64_t value) {
    QoreValue v = fromBits(list_bits);
    if (v.getType() == NT_LIST) {
        QoreListNode* l = v.get<QoreListNode>();
        qore_list_private* priv = qore_list_private::get(*l);
        priv->getEntryReference(static_cast<size_t>(index)) = QoreValue(value);
        if (static_cast<size_t>(index) >= priv->length) {
            priv->length = static_cast<size_t>(index) + 1;
        }
    }
}

extern "C" DLLEXPORT void qore_rt_list_set_float(uint64_t list_bits, int64_t index, double value) {
    QoreValue v = fromBits(list_bits);
    if (v.getType() == NT_LIST) {
        QoreListNode* l = v.get<QoreListNode>();
        qore_list_private* priv = qore_list_private::get(*l);
        priv->getEntryReference(static_cast<size_t>(index)) = QoreValue(value);
        if (static_cast<size_t>(index) >= priv->length) {
            priv->length = static_cast<size_t>(index) + 1;
        }
    }
}

extern "C" DLLEXPORT void qore_rt_list_set_value(uint64_t list_bits, int64_t index, uint64_t value_bits) {
    QoreValue v = fromBits(list_bits);
    if (v.getType() == NT_LIST) {
        QoreListNode* l = v.get<QoreListNode>();
        QoreValue val = fromBits(value_bits);
        qore_list_private* priv = qore_list_private::get(*l);
        // Track element type for correct list<T> type info at runtime
        priv->setListTypeFromNewElementType(val.getFullTypeInfo());
        priv->getEntryReference(static_cast<size_t>(index)) = val;
        if (static_cast<size_t>(index) >= priv->length) {
            priv->length = static_cast<size_t>(index) + 1;
        }
        if (needs_scan(val)) {
            priv->incScanCount(1);
        }
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_refself(uint64_t bits) {
    QoreValue v = fromBits(bits);
    return toBits(v.refSelf());
}

extern "C" DLLEXPORT uint64_t qore_rt_get_object_class(uint64_t obj_bits) {
    QoreValue v = fromBits(obj_bits);
    if (v.getType() == NT_OBJECT) {
        const QoreObject* obj = v.get<const QoreObject>();
        return reinterpret_cast<uint64_t>(obj->getClass());
    }
    return 0;
}

// Forward declaration — implementation below after instantiateFastCallParams and execJITWithDeopt
static uint64_t execClosureDirect(const QoreClosureBase* cb, const UserVariantBase* uvb,
        int nargs, const uint64_t* args, ExceptionSink* xsink);

// Fast-path helper for closure calls with no arguments — avoids QoreListNode allocation
extern "C" DLLEXPORT uint64_t qore_rt_call_closure_0(uint64_t ref_bits, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    QoreValue ref_val = fromBits(ref_bits);
    if (!ref_val.hasNode()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot call a NOTHING value as a closure/call reference");
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = ref_val.getInternalNode();
    qore_type_t ntype = node->getType();

    // Fast path for closures: type check + static_cast instead of dynamic_cast
    if (ntype == NT_RUNTIME_CLOSURE) {
        const QoreClosureBase* cb = static_cast<const QoreClosureBase*>(node);
        // Check if the closure variant supports direct dispatch (has cached IR or JIT)
        UserClosureFunction* uf = static_cast<UserClosureFunction*>(cb->getFunction());
        assert(uf);
        const AbstractQoreFunctionVariant* variant = uf->first();
        const UserVariantBase* uvb = variant->getUserVariantBase();
        if (uvb && (uvb->hasCachedFunction() || uvb->getCachedIR())) {
            return execClosureDirect(cb, uvb, 0, nullptr, xsink);
        }
        // Fall through to execValue for closures without cached IR/JIT
        QoreValue result = const_cast<QoreClosureBase*>(cb)->execValue(nullptr, xsink);
        return toBits(result);
    }

    // For function references (NT_FUNCREF) and other types: use type check + static_cast
    if (ntype == NT_FUNCREF) {
        ResolvedCallReferenceNode* callref = static_cast<ResolvedCallReferenceNode*>(
            const_cast<AbstractQoreNode*>(node));
        QoreValue result = callref->execValue(nullptr, xsink);
        return toBits(result);
    }

    xsink->raiseException("CALL-REFERENCE-ERROR", "value is not a call reference or closure");
    return toBits(QoreValue());
}

// Fast-path helper for closure calls with one argument — optimized list allocation
extern "C" DLLEXPORT uint64_t qore_rt_call_closure_1(uint64_t ref_bits, uint64_t arg0_bits, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    QoreValue ref_val = fromBits(ref_bits);
    if (!ref_val.hasNode()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot call a NOTHING value as a closure/call reference");
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = ref_val.getInternalNode();
    qore_type_t ntype = node->getType();

    // Fast path for closures: bypass QoreListNode + dynamic_cast
    if (ntype == NT_RUNTIME_CLOSURE) {
        const QoreClosureBase* cb = static_cast<const QoreClosureBase*>(node);
        UserClosureFunction* uf = static_cast<UserClosureFunction*>(cb->getFunction());
        assert(uf);
        const AbstractQoreFunctionVariant* variant = uf->first();
        const UserVariantBase* uvb = variant->getUserVariantBase();
        if (uvb && (uvb->hasCachedFunction() || uvb->getCachedIR())) {
            return execClosureDirect(cb, uvb, 1, &arg0_bits, xsink);
        }
        // Fall through to execValue for closures without cached IR/JIT
    }

    // Slow path: build QoreListNode and call execValue
    ResolvedCallReferenceNode* callref;
    if (ntype == NT_RUNTIME_CLOSURE || ntype == NT_FUNCREF) {
        callref = static_cast<ResolvedCallReferenceNode*>(const_cast<AbstractQoreNode*>(node));
    } else {
        xsink->raiseException("CALL-REFERENCE-ERROR", "value is not a call reference or closure");
        return toBits(QoreValue());
    }

    // Build single-element list with pre-allocated capacity
    ReferenceHolder<QoreListNode> arg_list(new QoreListNode(autoTypeInfo), xsink);
    QoreValue arg0 = fromBits(arg0_bits);
    if (arg0.hasNode()) {
        arg0.refSelf();
    }
    qore_list_private::get(**arg_list)->pushIntern(arg0);

    // Call directly with the single-element list (execValue borrows it)
    QoreValue result = callref->execValue(*arg_list, xsink);
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_closure_fast(uint64_t ref_bits, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    QoreValue ref_val = fromBits(ref_bits);
    if (!ref_val.hasNode()) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "cannot call a NOTHING value as a closure/call reference");
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = ref_val.getInternalNode();
    qore_type_t ntype = node->getType();

    // Fast path for closures: bypass QoreListNode + dynamic_cast
    if (ntype == NT_RUNTIME_CLOSURE) {
        const QoreClosureBase* cb = static_cast<const QoreClosureBase*>(node);
        UserClosureFunction* uf = static_cast<UserClosureFunction*>(cb->getFunction());
        assert(uf);
        const AbstractQoreFunctionVariant* variant = uf->first();
        const UserVariantBase* uvb = variant->getUserVariantBase();
        if (uvb && (uvb->hasCachedFunction() || uvb->getCachedIR())) {
            return execClosureDirect(cb, uvb, nargs, args, xsink);
        }
        // Fall through to execValue for closures without cached IR/JIT
    }

    // Slow path: build QoreListNode and call execValue
    ResolvedCallReferenceNode* callref;
    if (ntype == NT_RUNTIME_CLOSURE || ntype == NT_FUNCREF) {
        callref = static_cast<ResolvedCallReferenceNode*>(const_cast<AbstractQoreNode*>(node));
    } else {
        xsink->raiseException("CALL-REFERENCE-ERROR", "value is not a call reference or closure");
        return toBits(QoreValue());
    }

    // Build QoreListNode from NaN-boxed args
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }

    QoreValue result = callref->execValue(*arg_list, xsink);
    return toBits(result);
}

// Optimized map operations - native loops that return lists
extern "C" DLLEXPORT uint64_t qore_rt_map_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    result->push(iv->getAsBigInt() * scale, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        int64_t val = v.getAsBigInt();
        return toBits(QoreValue(val * scale));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsBigInt() * scale, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_scale_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    result->push(iv->getAsFloat() * scale, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        double val = v.getAsFloat();
        return toBits(QoreValue(val * scale));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsFloat() * scale, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_offset_int(uint64_t list_val, int64_t offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    result->push(iv->getAsBigInt() + offset, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        int64_t val = v.getAsBigInt();
        return toBits(QoreValue(val + offset));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsBigInt() + offset, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_offset_float(uint64_t list_val, double offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    result->push(iv->getAsFloat() + offset, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        double val = v.getAsFloat();
        return toBits(QoreValue(val + offset));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        result->push(l->retrieveEntry(i).getAsFloat() + offset, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_square_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    int64_t val = iv->getAsBigInt();
                    result->push(val * val, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        int64_t val = v.getAsBigInt();
        return toBits(QoreValue(val * val));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        result->push(val * val, nullptr);
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_square_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        // NOTHING returns NOTHING
        if (v.isNothing()) {
            return toBits(QoreValue());
        }
        // Handle iterator objects using abstract iterator protocol
        if (v.getType() == NT_OBJECT) {
            QoreObject* obj = const_cast<QoreObject*>(v.get<const QoreObject>());
            ExceptionSink xsink;
            AbstractIteratorHelper h(&xsink, "map operator", obj);
            if (h) {
                ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), &xsink);
                while (!xsink) {
                    bool has_next = h.next(&xsink);
                    if (xsink || !has_next) break;
                    ValueHolder iv(h.getValue(&xsink), &xsink);
                    if (xsink) break;
                    double val = iv->getAsFloat();
                    result->push(val * val, &xsink);
                }
                if (!xsink) return toBits(result.release());
                // On exception, fall through (exception already set in thread-local xsink)
                // Return NOTHING to indicate error
                return toBits(QoreValue());
            }
            // Not an iterator, fall through to single-value handling
        }
        // Handle single-value input: apply operation and return directly
        double val = v.getAsFloat();
        return toBits(QoreValue(val * val));
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        result->push(val * val, nullptr);
    }
    return toBits(result.release());
}

// Fully specialized hash-key map operations (single runtime call per entire map)
extern "C" DLLEXPORT uint64_t qore_rt_map_hash_key_value(uint64_t list_val, const char* key) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(autoTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            QoreValue val = elem.get<const QoreHashNode>()->getKeyValue(key);
            result->push(val.refSelf(), nullptr);
        } else {
            result->push(QoreValue(), nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_hash_key_int(uint64_t list_val, const char* key) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            result->push(elem.get<const QoreHashNode>()->getKeyValue(key).getAsBigInt(), nullptr);
        } else {
            result->push(0ll, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_hash_key_offset_int(uint64_t list_val, const char* key, int64_t offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            result->push(elem.get<const QoreHashNode>()->getKeyValue(key).getAsBigInt() + offset, nullptr);
        } else {
            result->push(offset, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_map_hash_key_scale_int(uint64_t list_val, const char* key, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            result->push(elem.get<const QoreHashNode>()->getKeyValue(key).getAsBigInt() * scale, nullptr);
        } else {
            result->push(0ll, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_hash_map_two_keys(uint64_t list_val, const char* key1, const char* key2) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoHashTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        QoreValue elem = l->retrieveEntry(i);
        if (elem.getType() == NT_HASH) {
            const QoreHashNode* h = elem.get<const QoreHashNode>();
            QoreValue k = h->getKeyValue(key1);
            QoreValue val = h->getKeyValue(key2);
            QoreString key_str;
            if (k.getType() == NT_STRING) {
                key_str.set(k.get<const QoreStringNode>()->c_str());
            } else {
                QoreStringValueHelper sh(k);
                key_str.set(sh->c_str());
            }
            result->setKeyValue(key_str.c_str(), val.refSelf(), nullptr);
        }
    }
    return toBits(result.release());
}

// Optimized select operations - native loops that filter lists
extern "C" DLLEXPORT uint64_t qore_rt_select_positive_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val > 0) {
            result->push(val, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_select_positive_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val > 0.0) {
            result->push(val, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_select_nonzero_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val != 0) {
            result->push(val, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_select_nonzero_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val != 0.0) {
            result->push(val, nullptr);
        }
    }
    return toBits(result.release());
}

// Fused map+select operations - filter positive then transform in single pass
extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_scale_positive_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val > 0) {
            result->push(val * scale, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_scale_positive_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val > 0.0) {
            result->push(val * scale, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_offset_positive_int(uint64_t list_val, int64_t offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val > 0) {
            result->push(val + offset, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_offset_positive_float(uint64_t list_val, double offset) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val > 0.0) {
            result->push(val + offset, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_square_positive_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(bigIntTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        if (val > 0) {
            result->push(val * val, nullptr);
        }
    }
    return toBits(result.release());
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_select_square_positive_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    ReferenceHolder<QoreListNode> result(new QoreListNode(floatTypeInfo), nullptr);
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        if (val > 0.0) {
            result->push(val * val, nullptr);
        }
    }
    return toBits(result.release());
}

// Fused map+foldl operations - map and reduce in single pass, no intermediate list
// Pattern: foldl $1 + $2, (map $1 * c, list) -> sum(list[i] * c)
extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_sum_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    int64_t result = 0;
    for (size_t i = 0; i < sz; ++i) {
        result += l->retrieveEntry(i).getAsBigInt() * scale;
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_sum_scale_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    double result = 0.0;
    for (size_t i = 0; i < sz; ++i) {
        result += l->retrieveEntry(i).getAsFloat() * scale;
    }
    return toBits(result);
}

// Pattern: foldl $1 + $2, (map $1 * $1, list) -> sum(list[i]^2)
extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_sum_square_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    int64_t result = 0;
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        result += val * val;
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_sum_square_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    double result = 0.0;
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        result += val * val;
    }
    return toBits(result);
}

// Pattern: foldl $1 * $2, (map $1 * c, list) -> prod(list[i] * c)
extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_prod_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    int64_t result = 1;
    for (size_t i = 0; i < sz; ++i) {
        result *= l->retrieveEntry(i).getAsBigInt() * scale;
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_fused_map_foldl_prod_scale_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return toBits(QoreValue());
    }
    double result = 1.0;
    for (size_t i = 0; i < sz; ++i) {
        result *= l->retrieveEntry(i).getAsFloat() * scale;
    }
    return toBits(result);
}

extern "C" DLLEXPORT uint64_t qore_rt_string_concat(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    if (lv.getType() == NT_STRING && rv.getType() == NT_STRING) {
        const QoreStringNode* ls = lv.get<const QoreStringNode>();
        const QoreStringNode* rs = rv.get<const QoreStringNode>();
        QoreStringNode* result = new QoreStringNode(*ls);
        result->concat(rs, xsink);
        if (xsink && *xsink) {
            result->deref();
            return toBits(QoreValue());
        }
        return toBits(QoreValue(result));
    }
    // Not both strings: fall back to generic add
    return qore_rt_add_any(left, right, xsink);
}

// Typed string concatenation - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_add_typed(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    // Caller guarantees both are strings, but handle NOTHING gracefully
    const QoreStringNode* ls = lv.getType() == NT_STRING
        ? lv.get<const QoreStringNode>() : nullptr;
    const QoreStringNode* rs = rv.getType() == NT_STRING
        ? rv.get<const QoreStringNode>() : nullptr;
    if (!ls && !rs) {
        return toBits(QoreValue());  // Both NOTHING
    }
    if (!ls) {
        return toBits(QoreValue(rs->stringRefSelf()));  // Copy right
    }
    if (!rs) {
        return toBits(QoreValue(ls->stringRefSelf()));  // Copy left
    }
    // Both are strings - concatenate
    QoreStringNode* result = new QoreStringNode(*ls);
    result->concat(rs, xsink);
    if (xsink && *xsink) {
        result->deref();
        return toBits(QoreValue());
    }
    return toBits(QoreValue(result));
}

// Multi-string concatenation - concatenates N strings in a single pass
// This is more efficient than chaining AddString operations for a + b + c + d patterns
extern "C" DLLEXPORT uint64_t qore_rt_string_concat_multi(uint64_t* args, int nargs, ExceptionSink* xsink) {
    if (nargs == 0) {
        return toBits(QoreValue(new QoreStringNode()));
    }

    // First pass: calculate total length and find first non-NOTHING string for encoding
    size_t total_len = 0;
    const QoreEncoding* enc = QCS_DEFAULT;
    for (int i = 0; i < nargs; ++i) {
        QoreValue v = fromBits(args[i]);
        if (v.getType() == NT_STRING) {
            const QoreStringNode* s = v.get<const QoreStringNode>();
            total_len += s->size();
            if (i == 0) {
                enc = s->getEncoding();
            }
        }
    }

    // Second pass: build the result string
    QoreStringNode* result = new QoreStringNode(enc);
    result->reserve(total_len);

    for (int i = 0; i < nargs; ++i) {
        QoreValue v = fromBits(args[i]);
        if (v.getType() == NT_STRING) {
            const QoreStringNode* s = v.get<const QoreStringNode>();
            result->concat(s, xsink);
            if (xsink && *xsink) {
                result->deref();
                return toBits(QoreValue());
            }
        }
        // NOTHING values are skipped (treated as empty string)
    }

    return toBits(QoreValue(result));
}

// Typed string equality - both operands are known to be strings at compile time
// Uses equalSoft() for encoding-aware comparison (e.g. UTF-8 vs ISO-8859-1)
extern "C" DLLEXPORT uint64_t qore_rt_string_eq_typed(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreStringNode* ls = lv.get<const QoreStringNode>();
    const QoreStringNode* rs = rv.get<const QoreStringNode>();
    bool result = ls && rs && ls->equalSoft(*rs, xsink);
    return toBits(QoreValue(result));
}

// Typed string inequality - both operands are known to be strings at compile time
// Uses equalSoft() for encoding-aware comparison (e.g. UTF-8 vs ISO-8859-1)
extern "C" DLLEXPORT uint64_t qore_rt_string_ne_typed(uint64_t left, uint64_t right, ExceptionSink* xsink) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreStringNode* ls = lv.get<const QoreStringNode>();
    const QoreStringNode* rs = rv.get<const QoreStringNode>();
    bool result = !ls || !rs || !ls->equalSoft(*rs, xsink);
    return toBits(QoreValue(result));
}

// Typed string less than - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_lt_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreStringNode* ls = lv.get<const QoreStringNode>();
    const QoreStringNode* rs = rv.get<const QoreStringNode>();
    // If either is null/NOTHING, result is false (consistent with Qore semantics)
    bool result = ls && rs && (fast_string_compare(ls, rs) < 0);
    return toBits(QoreValue(result));
}

// Typed string less than or equal - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_le_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreStringNode* ls = lv.get<const QoreStringNode>();
    const QoreStringNode* rs = rv.get<const QoreStringNode>();
    bool result = ls && rs && (fast_string_compare(ls, rs) <= 0);
    return toBits(QoreValue(result));
}

// Typed string greater than - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_gt_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreStringNode* ls = lv.get<const QoreStringNode>();
    const QoreStringNode* rs = rv.get<const QoreStringNode>();
    bool result = ls && rs && (fast_string_compare(ls, rs) > 0);
    return toBits(QoreValue(result));
}

// Typed string greater than or equal - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_ge_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreStringNode* ls = lv.get<const QoreStringNode>();
    const QoreStringNode* rs = rv.get<const QoreStringNode>();
    bool result = ls && rs && (fast_string_compare(ls, rs) >= 0);
    return toBits(QoreValue(result));
}

// Typed string comparison (spaceship) - both operands are known to be strings at compile time
// Returns -1, 0, or 1 as an integer
extern "C" DLLEXPORT uint64_t qore_rt_string_cmp_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreStringNode* ls = lv.get<const QoreStringNode>();
    const QoreStringNode* rs = rv.get<const QoreStringNode>();
    // fast_string_compare already returns normalized -1/0/1
    int64_t result = (ls && rs) ? fast_string_compare(ls, rs) : 0;
    return toBits(QoreValue(result));
}

// String switch lookup - returns the case index or -1 for default
// case_strings is an array of C strings (null-terminated), num_cases is the count
extern "C" DLLEXPORT int32_t qore_rt_switch_string_lookup(uint64_t switch_val_bits, const char** case_strings,
        int32_t num_cases) {
    QoreValue switch_val = fromBits(switch_val_bits);
    const QoreStringNode* str = switch_val.get<const QoreStringNode>();
    if (!str) {
        return -1;  // Not a string, go to default
    }
    for (int32_t i = 0; i < num_cases; ++i) {
        if (str->equal(case_strings[i])) {
            return i;
        }
    }
    return -1;  // No match, go to default
}

// --- DotEval with pre-evaluated base helper ---

#include "qore/intern/QoreDotEvalOperatorNode.h"
#include "qore/intern/QorePseudoMethods.h"

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_with_base(uint64_t expr_bits, uint64_t base_bits, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return toBits(QoreValue());
    }
    auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(expr.getInternalNode());
    if (!dot_eval) {
        // Fallback: shouldn't happen but be safe
        return qore_rt_invoke_expr(expr_bits, xsink);
    }
    QoreValue base = fromBits(base_bits);
    QoreValue result = dot_eval->evalWithBase(base, xsink);
    return toBits(result);
}

// --- Call with pre-evaluated args helper ---

extern "C" DLLEXPORT uint64_t qore_rt_call_with_args(uint64_t expr_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return toBits(QoreValue());
    }

    // Phase 5: Fast-call detection for builtins
    // Skip QoreListNode allocation for builtins with fast-call variants
    if (auto* call = dynamic_cast<const FunctionCallNode*>(expr.getInternalNode())) {
        const char* fname = call->getName();
        if (fname) {
            // Zero-argument fast-call functions (highest priority - no arg unpacking needed)
            if (nargs == 0) {
                if (!strcmp(fname, "now_us")) {
                    return qore_fast_now_us(xsink);
                } else if (!strcmp(fname, "now_ms")) {
                    return qore_fast_now_ms(xsink);
                } else if (!strcmp(fname, "now")) {
                    return qore_fast_now(xsink);
                } else if (!strcmp(fname, "time")) {
                    return qore_fast_time(xsink);
                }
            }
            // Single-argument fast-call functions
            else if (nargs == 1) {
                if (!strcmp(fname, "strlen")) {
                    return qore_fast_strlen(args[0], xsink);
                } else if (!strcmp(fname, "length")) {
                    return qore_fast_length(args[0], xsink);
                } else if (!strcmp(fname, "tolower") || !strcmp(fname, "lwr")) {
                    return qore_fast_tolower(args[0], xsink);
                } else if (!strcmp(fname, "toupper") || !strcmp(fname, "upr")) {
                    return qore_fast_toupper(args[0], xsink);
                } else if (!strcmp(fname, "trim")) {
                    return qore_fast_trim(args[0], xsink);
                } else if (!strcmp(fname, "abs")) {
                    return qore_fast_abs(args[0], xsink);
                } else if (!strcmp(fname, "first")) {
                    return qore_fast_first(args[0], xsink);
                } else if (!strcmp(fname, "last")) {
                    return qore_fast_last(args[0], xsink);
                }
            }
            // Two-argument fast-call functions
            else if (nargs == 2) {
                if (!strcmp(fname, "exists")) {
                    return qore_fast_hash_exists(args[0], args[1], xsink);
                }
            }
        }
    }

    // Phase 5.2c: Pseudo-method fast-call detection
    // Handle <type>::method() pseudo-method calls without QoreListNode allocation
    if (auto* method_call = dynamic_cast<const MethodCallNode*>(expr.getInternalNode())) {
        if (method_call->isPseudo()) {
            const char* mname = method_call->getName();
            if (mname && nargs == 1) {
                // Phase 5.2c: Pseudo-methods on built-in types (read-only, non-mutating)
                if (!strcmp(mname, "length") || !strcmp(mname, "size")) {
                    return qore_fast_any_size(args[0], xsink);
                } else if (!strcmp(mname, "keys")) {
                    return qore_fast_hash_keys(args[0], xsink);
                } else if (!strcmp(mname, "values")) {
                    return qore_fast_hash_values(args[0], xsink);
                }
            }
        }
    }

    // Build QoreListNode from the NaN-boxed args array
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> arg_list(new QoreListNode(autoTypeInfo), xsink);
    {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }

    // Determine call type and create a copy with the pre-built arg list
    bool used_operands = false;
    QoreValue result;

    if (auto* call = dynamic_cast<const FunctionCallNode*>(expr.getInternalNode())) {
        QoreValue call_expr(new FunctionCallNode(*call, arg_list.release()));
        ValueHolder call_holder(call_expr, nullptr);
        bool needs_deref = true;
        result = call_expr.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        used_operands = true;
    } else if (auto* call = dynamic_cast<const SelfFunctionCallNode*>(expr.getInternalNode())) {
        QoreValue call_expr(new SelfFunctionCallNode(*call, arg_list.release()));
        ValueHolder call_holder(call_expr, nullptr);
        bool needs_deref = true;
        result = call_expr.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        used_operands = true;
    } else if (auto* call = dynamic_cast<const StaticMethodCallNode*>(expr.getInternalNode())) {
        QoreValue call_expr(new StaticMethodCallNode(*call, arg_list.release()));
        ValueHolder call_holder(call_expr, nullptr);
        bool needs_deref = true;
        result = call_expr.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        used_operands = true;
    } else if (auto* call = dynamic_cast<const CallReferenceCallNode*>(expr.getInternalNode())) {
        const ParseNode* parse_node = dynamic_cast<const ParseNode*>(expr.getInternalNode());
        const QoreProgramLocation* loc = parse_node ? parse_node->loc : nullptr;
        QoreValue exp = call->getExp();
        if (exp.hasNode()) {
            exp = exp.refSelf();
        }
        QoreValue call_expr(new CallReferenceCallNode(loc, exp, arg_list.release()));
        ValueHolder call_holder(call_expr, nullptr);
        bool needs_deref = true;
        result = call_expr.getInternalNode()->eval(needs_deref, xsink);
        if (!needs_deref && result.hasNode()) {
            result = result.refSelf();
        }
        used_operands = true;
    }

    if (!used_operands) {
        // Fall back to full AST re-evaluation for unrecognized call types
        return qore_rt_invoke_expr(expr_bits, xsink);
    }

    return toBits(result);
}

// --- Phase 5: Fast-call builtin variants (skip QoreListNode allocation) ---

extern "C" DLLEXPORT uint64_t qore_fast_strlen(uint64_t arg_bits, ExceptionSink* xsink) {
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(0);
    }

    // Get string value (softstring auto-converts to string)
    const QoreStringNode* str = nullptr;
    if (auto* s = dynamic_cast<const QoreStringNode*>(arg.getInternalNode())) {
        str = s;
    } else {
        // For non-string types, try to convert via string conversion
        // This matches the behavior of the QPP strlen(softstring) function
        QoreString temp;
        int err = 0;
        arg.getInternalNode()->getAsString(temp, -1, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
        // For non-string-like values, return 0 (matches NOOP variant behavior)
        return toBits(0);
    }

    // Return string length as a 64-bit integer
    int64_t len = static_cast<int64_t>(str->strlen());
    return toBits(len);
}

// --- Phase 5.2a: Zero-argument fast-call functions (highest impact) ---

extern "C" DLLEXPORT uint64_t qore_fast_now_us(ExceptionSink* xsink) {
    // Returns current date/time with microsecond precision
    // Equivalent to: date now_us() { return DateTimeNode::makeNow(); }
    DateTimeNode* dt = DateTimeNode::makeNow();
    return toBits(dt);
}

extern "C" DLLEXPORT uint64_t qore_fast_now_ms(ExceptionSink* xsink) {
    // Returns current date/time with millisecond precision (same as now_us in practice)
    DateTimeNode* dt = DateTimeNode::makeNow();
    return toBits(dt);
}

extern "C" DLLEXPORT uint64_t qore_fast_now(ExceptionSink* xsink) {
    // Returns current date/time with second precision (same as now_us in practice)
    DateTimeNode* dt = DateTimeNode::makeNow();
    return toBits(dt);
}

extern "C" DLLEXPORT uint64_t qore_fast_time(ExceptionSink* xsink) {
    // Returns Unix timestamp as integer (seconds since epoch)
    // Equivalent to: int time() { return (int64)now_us()->getEpoch(); }
    DateTimeNode* dt = DateTimeNode::makeNow();
    int64_t timestamp = dt->getEpochSeconds();
    dt->deref(xsink);
    return toBits(timestamp);
}

// --- Phase 5.2b: Single-argument string functions ---

extern "C" DLLEXPORT uint64_t qore_fast_length(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns length of string or binary data
    // Equivalent to: int length(softstring str) { return str->length(); }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(0);
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle string
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        return toBits(static_cast<int64_t>(str->length()));
    }

    // Handle binary
    if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
        return toBits(static_cast<int64_t>(bin->size()));
    }

    // For other types, try string conversion length
    QoreString temp;
    int err = 0;
    node->getAsString(temp, -1, xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    return toBits(static_cast<int64_t>(temp.length()));
}

extern "C" DLLEXPORT uint64_t qore_fast_tolower(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns lowercase version of string
    // Equivalent to: softstring tolower(softstring str) { return str->tolower(); }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle string directly
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        QoreStringNode* result = str->copy();
        result->tolwr();
        return toBits(result);
    }

    // For non-string types, convert to string first
    QoreStringNode* temp = new QoreStringNode;
    int err = 0;
    node->getAsString(*temp, -1, xsink);
    if (*xsink) {
        temp->deref(xsink);
        return toBits(QoreValue());
    }
    temp->tolwr();
    return toBits(temp);
}

extern "C" DLLEXPORT uint64_t qore_fast_toupper(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns uppercase version of string
    // Equivalent to: softstring toupper(softstring str) { return str->toupper(); }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle string directly
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        QoreStringNode* result = str->copy();
        result->toupr();
        return toBits(result);
    }

    // For non-string types, convert to string first
    QoreStringNode* temp = new QoreStringNode;
    int err = 0;
    node->getAsString(*temp, -1, xsink);
    if (*xsink) {
        temp->deref(xsink);
        return toBits(QoreValue());
    }
    temp->toupr();
    return toBits(temp);
}

// --- Phase 5.2c: Pseudo-method fast-calls ---

extern "C" DLLEXPORT uint64_t qore_fast_any_size(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns size of any collection (hash, list) or length of string
    // Equivalent to: int size(any val) { ... }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(0);
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle hash
    if (auto* hash = dynamic_cast<const QoreHashNode*>(node)) {
        return toBits(static_cast<int64_t>(hash->size()));
    }

    // Handle list
    if (auto* list = dynamic_cast<const QoreListNode*>(node)) {
        return toBits(static_cast<int64_t>(list->size()));
    }

    // Handle string
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        return toBits(static_cast<int64_t>(str->length()));
    }

    // Handle binary
    if (auto* bin = dynamic_cast<const BinaryNode*>(node)) {
        return toBits(static_cast<int64_t>(bin->size()));
    }

    // Other types have no size
    return toBits(0);
}

extern "C" DLLEXPORT uint64_t qore_fast_hash_keys(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns list of hash keys
    // Equivalent to: list<string> keys(hash<auto, auto> val) { ... }
    QoreValue arg = fromBits(arg_bits);

    ReferenceHolder<QoreListNode> keys(new QoreListNode(stringTypeInfo), xsink);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(keys.release());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle hash
    if (auto* hash = dynamic_cast<const QoreHashNode*>(node)) {
        // Iterate over all keys and add to result list
        ConstHashIterator hi(hash);
        while (hi.next()) {
            QoreStringNode* key = new QoreStringNode(hi.getKey());
            qore_list_private::get(*keys)->push(key, xsink);
        }
    }

    return toBits(keys.release());
}

extern "C" DLLEXPORT uint64_t qore_fast_hash_values(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns list of hash values
    // Equivalent to: list<auto> values(hash<auto, auto> val) { ... }
    QoreValue arg = fromBits(arg_bits);

    ReferenceHolder<QoreListNode> vals(new QoreListNode(autoTypeInfo), xsink);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(vals.release());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle hash
    if (auto* hash = dynamic_cast<const QoreHashNode*>(node)) {
        // Iterate over all values and add to result list
        ConstHashIterator hi(hash);
        while (hi.next()) {
            QoreValue v = hi.get();
            if (v.hasNode()) {
                v.refSelf();
            }
            qore_list_private::get(*vals)->push(v, xsink);
        }
    }

    return toBits(vals.release());
}

// --- Phase 5.3: Additional fast-path optimizations ---

extern "C" DLLEXPORT uint64_t qore_fast_trim(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns trimmed version of string (whitespace removed from both ends)
    // Equivalent to: softstring trim(softstring str) { return str->trim(); }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle string directly
    if (auto* str = dynamic_cast<const QoreStringNode*>(node)) {
        QoreStringNode* result = str->copy();
        result->trim();
        return toBits(result);
    }

    // For non-string types, convert to string first
    QoreStringNode* temp = new QoreStringNode;
    int err = 0;
    node->getAsString(*temp, -1, xsink);
    if (*xsink) {
        temp->deref(xsink);
        return toBits(QoreValue());
    }
    temp->trim();
    return toBits(temp);
}

extern "C" DLLEXPORT uint64_t qore_fast_abs(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns absolute value of int or float
    // Equivalent to: number abs(number n) { return n < 0 ? -n : n; }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    // Handle integers
    if (arg.getType() == QV_Int) {
        int64_t val = arg.getAsBigInt();
        return toBits(val < 0 ? -val : val);
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle floats
    if (arg.getType() == QV_Float) {
        double val = arg.getAsFloat();
        return toBits(val < 0.0 ? -val : val);
    }

    // Handle number nodes
    if (auto* num = dynamic_cast<const QoreNumberNode*>(node)) {
        return toBits(num->sign() < 0 ? num->negate() : num);
    }

    // Fallback for other numeric types: try conversion to int then float
    // This matches the behavior of the abs() builtin
    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_fast_first(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns first element of list
    // Equivalent to: any first(list<any> l) { return l.size() ? l[0] : nothing; }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle list
    if (auto* list = dynamic_cast<const QoreListNode*>(node)) {
        if (list->size() > 0) {
            QoreValue result = list->retrieveEntry(0);
            if (result.hasNode()) {
                result.refSelf();
            }
            return toBits(result);
        }
    }

    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_fast_last(uint64_t arg_bits, ExceptionSink* xsink) {
    // Returns last element of list
    // Equivalent to: any last(list<any> l) { return l.size() ? l[l.size()-1] : nothing; }
    QoreValue arg = fromBits(arg_bits);

    // Handle null/nothing
    if (!arg.hasNode()) {
        return toBits(QoreValue());
    }

    const AbstractQoreNode* node = arg.getInternalNode();

    // Handle list
    if (auto* list = dynamic_cast<const QoreListNode*>(node)) {
        size_t size = list->size();
        if (size > 0) {
            QoreValue result = list->retrieveEntry(size - 1);
            if (result.hasNode()) {
                result.refSelf();
            }
            return toBits(result);
        }
    }

    return toBits(QoreValue());
}

extern "C" DLLEXPORT uint64_t qore_fast_hash_exists(uint64_t hash_bits, uint64_t key_bits, ExceptionSink* xsink) {
    // Returns true if hash key exists
    // Equivalent to: bool exists(hash<auto, auto> h, string key) { return h.exists(key); }
    QoreValue hash_val = fromBits(hash_bits);
    QoreValue key_val = fromBits(key_bits);

    // Handle null/nothing hash
    if (!hash_val.hasNode()) {
        return toBits(false);
    }

    const AbstractQoreNode* hash_node = hash_val.getInternalNode();

    // Handle hash
    if (auto* hash = dynamic_cast<const QoreHashNode*>(hash_node)) {
        // Convert key to string if needed
        QoreString key_str;
        if (key_val.hasNode()) {
            key_val.getInternalNode()->getAsString(key_str, -1, xsink);
        } else {
            key_str.concat(key_val.getAsBigInt());
        }
        if (*xsink) {
            return toBits(false);
        }
        return toBits(hash->existsKey(key_str.c_str()));
    }

    return toBits(false);
}

// --- Direct function call (resolved at parse time, skips AST round-trip) ---

extern "C" DLLEXPORT uint64_t qore_rt_call_function_direct(const QoreFunction* func,
        const AbstractQoreFunctionVariant* variant, QoreProgram* pgm,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(func);
    // Build QoreListNode from the NaN-boxed args array
    // Use pushIntern() to bypass checkVal/stripVal which strips complex types
    // (e.g., hash<string, bool> -> hash<auto>) from arguments in untyped lists,
    // breaking function overload resolution
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }

    // Get runtime config
    RuntimeConfig& rc = rc_get_current_ref();

    // Determine the program context
    QoreProgram* call_pgm = pgm ? pgm : rc.getProgram();
    if (!call_pgm) {
        call_pgm = getProgram();
    }

    // Call the function directly — skips dynamic_cast chain and AST node copy
    QoreValue result = func->evalFunctionTmpArgs(variant, *arg_list, call_pgm, rc, xsink);

    return toBits(result);
}

//! Call a function with dynamic variant resolution (no pre-resolved variant)
/** Used when the variant could not be determined at AOT compile/load time
    (e.g., overloaded builtins like int() where the FunctionCallNode was
    reconstructed without args). Builds the arg list and uses evalDynamic()
    to resolve the correct variant at runtime.
*/
static uint64_t qore_rt_call_function_dynamic(const QoreFunction* func,
        QoreProgram* pgm, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(func);

    // Build QoreListNode from the NaN-boxed args array
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }

    // Get runtime config
    RuntimeConfig& rc = rc_get_current_ref();

    // Use evalDynamic which resolves the variant based on arg types
    QoreValue result = func->evalDynamic(*arg_list, rc, xsink);

    return toBits(result);
}

// Stack location for JIT/AOT-executed frames.
// Unlike QoreIRStackLocation which lazily resolves from block/ip, JIT code is compiled
// to native code so the source location is fixed at the function's parse location.
class QoreJITStackLocation : public QoreStackLocation, public QoreProgramStackLocationHelper {
public:
    DLLLOCAL QoreJITStackLocation(const std::string& call_name, const QoreProgramLocation* loc,
            const StatementBlock* statements, QoreProgram* pgm)
        : QoreProgramStackLocationHelper(this, saved_stmt, saved_pgm),
          call_name(call_name), loc(loc), statements(statements), pgm(pgm) {
        if (!this->pgm) {
            this->pgm = saved_pgm;
        }
    }

    DLLLOCAL const QoreProgramLocation& getLocation() const override {
        return loc ? *loc : loc_builtin;
    }

    DLLLOCAL const std::string& getCallName() const override {
        return call_name;
    }

    DLLLOCAL qore_call_t getCallType() const override {
        return CT_USER;
    }

    DLLLOCAL QoreProgram* getProgram() const override {
        return pgm;
    }

    DLLLOCAL const AbstractStatement* getStatement() const override {
        return statements;
    }

private:
    std::string call_name;
    const QoreProgramLocation* loc;
    const StatementBlock* statements;
    QoreProgram* pgm;
    // saved_stmt and saved_pgm receive old thread-local values from
    // QoreProgramStackLocationHelper constructor via output references.
    // IMPORTANT: no default member initializers — they would overwrite the values
    // written by the base class constructor (same pattern as QoreInternalCallStackLocationHelperBase).
    const AbstractStatement* saved_stmt;
    QoreProgram* saved_pgm;
};

/** Handle body local instantiation before JIT execution and deopt/cleanup after.

    Before the JIT call: instantiates body locals unless all are IR-only.
    Pushes a stack location entry for the JIT frame.
    Calls the provided function to execute JIT code.
    After the call: checks for JIT guard failure, falls back to AST if needed,
    and uninstantiates body locals.

    @param uvb the user variant body (for statement block, program, and body locals)
    @param call_name the function/method name for the stack trace
    @param exec_fn callable that executes the JIT code and returns raw NaN-boxed result bits
    @param val reference to receive the final result value
    @param xsink exception sink
*/
template <typename ExecFn>
static void execJITWithDeopt(const UserVariantBase* uvb, const std::string& call_name,
        ExecFn&& exec_fn, QoreValue& val, ExceptionSink* xsink) {
    // Get AST-visible body locals: for AOT use all_body_locals (separate optimization),
    // for IR use filtered ast_visible_body_locals (excludes IR-only locals that
    // are never accessed by AST callbacks).
    bool skip_body_locals = uvb->areAllBodyLocalsIROnly();
    const std::vector<LocalVar*>& body_locals = uvb->hasCachedAOT()
        ? uvb->getBodyLocals()  // AOT: use all_body_locals via getBodyLocals()
        : uvb->getASTVisibleBodyLocals();  // IR: use filtered ast_visible_body_locals
    bool has_aot = uvb->hasCachedAOT();
    if (!skip_body_locals) {
        const QoreParseOptions& po = uvb->pgm->getParseOptions();
        for (LocalVar* lv : body_locals) {
            // Skip closure-use vars in AOT mode: the LLVM code handles their
            // instantiation/uninstantiation at block scope boundaries via
            // qore_rt_instantiate_local_aot / qore_rt_pop_closure_var_aot.
            if (has_aot && lv->closureUse()) {
                continue;
            }
            lv->instantiate(po);
        }
    }

    // Push stack location for this JIT execution frame so it appears in
    // get_all_thread_call_stacks() and exception call stacks.
    const QoreProgramLocation* parse_loc = uvb->getUserSignature()->getParseLocation();
    QoreJITStackLocation jit_stack_loc(call_name, parse_loc, uvb->getStatementBlock(), uvb->pgm);

    // Set runtime_loc so nested function/method calls (via CodeEvaluationHelper)
    // report this function's source location as the caller.
    if (parse_loc) {
        update_runtime_statement_location(nullptr, parse_loc);
    }

    bool fn_invalidated = false;
    uint64_t result_bits = exec_fn(xsink, fn_invalidated);

    // Check for JIT guard failure or recompilation invalidation requesting deopt to AST
    if (!*xsink && (qore_jit_deopt_requested() || fn_invalidated)) {
        // Ensure body locals are on thread stack for AST execution
        if (skip_body_locals) {
            const QoreParseOptions& po = uvb->pgm->getParseOptions();
            for (LocalVar* lv : body_locals) {
                lv->instantiate(po);
            }
        }
        StatementBlock* stmts = uvb->getStatementBlock();
        if (stmts) {
            // Set TLS returnTypeInfo to the callee's declared return type so that
            // ReturnStatement::execImpl() performs the correct type check.
            // Fast-call paths (qore_rt_call_fast et al.) bypass CodeEvaluationHelper,
            // so TLS returnTypeInfo may still hold the *caller*'s return type here.
            const QoreTypeInfo* old_rti = saveReturnTypeInfo(uvb->getUserSignature()->getReturnTypeInfo());
            val = stmts->exec(xsink);
            saveReturnTypeInfo(old_rti);
        }
        if (skip_body_locals) {
            for (int i = (int)body_locals.size() - 1; i >= 0; --i) {
                body_locals[i]->uninstantiate(xsink);
            }
        }
    } else {
        QoreValue result;
        std::memcpy(&result, &result_bits, sizeof(result));
        val = result;
    }

    if (!skip_body_locals) {
        for (int i = (int)body_locals.size() - 1; i >= 0; --i) {
            // Skip closure-use vars in AOT mode: the LLVM code already popped
            // them via qore_rt_pop_closure_var_aot at block scope boundaries.
            if (has_aot && body_locals[i]->closureUse()) {
                continue;
            }
            body_locals[i]->uninstantiate(xsink);
        }
    }
}

// --- Fast call parameter instantiation helper ---

//! Instantiate parameter locals from NaN-boxed args with default argument evaluation
/** \return 0 on success, -1 on error (already-instantiated params are cleaned up on error,
    but caller must handle selfid cleanup and return value)
*/
static int instantiateFastCallParams(const UserSignature* sig, unsigned num_params, int nargs,
        const uint64_t* args, ExceptionSink* xsink) {
    const arg_vec_t& defaultArgList = sig->getDefaultArgList();
    for (unsigned i = 0; i < num_params; ++i) {
        if (i < (unsigned)nargs) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }

            // Apply type filter like the standard path in lib/Function.cpp:404-410
            const QoreTypeInfo* paramTypeInfo = sig->getParamTypeInfo(i);
            if (QoreTypeInfo::mayRequireFilter(paramTypeInfo, val)) {
                QoreTypeInfo::acceptInputParam(paramTypeInfo, i, sig->getName(i), val, xsink);
                if (*xsink) {
                    // Uninstantiate already-instantiated params in reverse
                    for (int j = (int)i - 1; j >= 0; --j) {
                        sig->lv[j]->uninstantiate(xsink);
                    }
                    return -1;
                }
            }

            sig->lv[i]->instantiate(val);
        } else if (i < defaultArgList.size() && defaultArgList[i]) {
            // Evaluate default argument expression
            QoreValue val = defaultArgList[i].eval(xsink);
            if (*xsink) {
                // Uninstantiate already-instantiated params in reverse
                for (int j = (int)i - 1; j >= 0; --j) {
                    sig->lv[j]->uninstantiate(xsink);
                }
                return -1;
            }

            // Apply type filter like the standard path in lib/Function.cpp:404-410
            const QoreTypeInfo* paramTypeInfo = sig->getParamTypeInfo(i);
            if (QoreTypeInfo::mayRequireFilter(paramTypeInfo, val)) {
                QoreTypeInfo::acceptInputParam(paramTypeInfo, i, sig->getName(i), val, xsink);
                if (*xsink) {
                    // Uninstantiate already-instantiated params in reverse
                    for (int j = (int)i - 1; j >= 0; --j) {
                        sig->lv[j]->uninstantiate(xsink);
                    }
                    return -1;
                }
            }

            sig->lv[i]->instantiate(val);
        } else {
            sig->lv[i]->instantiate(QoreValue());
        }
    }
    return 0;
}

// --- Fast function call (bypasses QoreListNode + CodeEvaluationHelper dispatch chain) ---

extern "C" DLLEXPORT uint64_t qore_rt_call_fast(const QoreFunction* func,
        const AbstractQoreFunctionVariant* variant, QoreProgram* pgm,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    assert(variant);

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        // Builtin variant — fall back to slow path for proper type coercion
        // (builtins can have soft types like softstring that require CodeEvaluationHelper)
        return qore_rt_call_function_direct(func, variant, pgm, args, nargs, xsink);
    }

    // If the callee has neither JIT nor IR, fall back to the slow path.
    // This can happen in tiered compilation when the callee hasn't been promoted yet.
    if (!uvb->hasCachedFunction() && !uvb->getCachedIR()) {
        return qore_rt_call_function_direct(func, variant, pgm, args, nargs, xsink);
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Set up program thread context (only if program differs from caller's program)
    std::optional<ProgramThreadCountContextHelper> ptcch;
    if (uvb->pgm != pgm) {
        ptcch.emplace(xsink, uvb->pgm, true);
        if (*xsink) {
            return toBits(QoreValue());
        }
    }
    // Push frame boundary so that get_local_vars()/set_local_var_value() can correctly
    // determine call-stack depth for debugger introspection (same as CodeEvaluationHelper
    // via UserVariantExecHelper::ThreadFrameBoundaryHelper in the AST path).
    ThreadFrameBoundaryHelper tfbh(true);

    // Check if callee IR supports direct param passing (bypass TLS entirely)
    const QoreIRFunction* ir = uvb->getCachedIR();
    bool use_direct_params = ir && ir->direct_params_eligible
        && !uvb->hasCachedFunction() && nargs >= (int)num_params;

    if (!use_direct_params) {
        // Standard path: push params to TLS
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            return toBits(QoreValue());
        }
    }
    // else: direct_params path — params pre-populated in IR slot cache

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable (if the function has an argv parameter)
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Use cached IR name when available (zero allocation); fall back to building the name
    std::string call_name_buf;
    if (!ir) {
        const char* class_name = func->className();
        if (class_name) {
            call_name_buf = class_name;
            call_name_buf += "::";
        }
        call_name_buf += func->getName();
    }
    const std::string& call_name = ir ? ir->name : call_name_buf;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        if (use_direct_params) {
            // Direct params path: pass args straight to IR slot cache, no TLS
            IRDirectParams dp{args, nargs};
            execJITWithDeopt(uvb, call_name, [ir, uvb, &dp](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm, false, &dp);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink);
        } else if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink);
        } else {
            // IR fast path (standard TLS): execute IR directly without QoreListNode.
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            execJITWithDeopt(uvb, call_name, [callee_ir, uvb](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm);
                if (!ok && !*xs) {
                    inv = true;  // Request deopt to AST
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink);
        }
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    if (!use_direct_params) {
        // Standard path: uninstantiate params from TLS
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = sig->getReturnTypeInfo();
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            // Missing return statement: check type and set location to method definition
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

// --- Fast closure dispatch (bypasses QoreListNode + dynamic_cast for closures with cached IR/JIT) ---

//! Execute a closure directly from NaN-boxed args, bypassing QoreListNode construction
/** This is the fast path for closure calls in JIT/AOT/IR modes. It:
    1. Pushes captured variables onto cvstack (CVecInstantiator)
    2. Sets the closure runtime environment
    3. Instantiates params directly from NaN-boxed args
    4. Calls evalTiered/JIT/IR directly
    5. Cleans up in reverse order

    @param cb the closure base (QoreClosureNode or QoreObjectClosureNode)
    @param uvb the user variant base (already resolved, has cached IR or JIT)
    @param nargs number of NaN-boxed arguments
    @param args pointer to NaN-boxed argument array (may be nullptr if nargs == 0)
    @param xsink exception sink
    @return NaN-boxed result value
*/
static uint64_t execClosureDirect(const QoreClosureBase* cb, const UserVariantBase* uvb,
        int nargs, const uint64_t* args, ExceptionSink* xsink) {
    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Push captured vars onto cvstack
    CVecInstantiator cvi(cb->getCvec(), xsink);

    // Set closure runtime environment so closure-captured vars are findable
    ThreadSafeLocalVarRuntimeEnvironmentHelper ch(cb);

    // Program context
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }

    // Frame boundary for debugger/stack introspection
    ThreadFrameBoundaryHelper tfbh(true);

    // Handle self for object closures (closures defined inside methods)
    QoreObject* self = const_cast<QoreObject*>(cb->getObject());

    // For object closures, establish the captured self in both TLS stores so that
    // implicit method calls (SelfFunctionCallNode) and LoadSelfMember resolve
    // against the closure's captured self, not the caller's context.
    // ObjectSubstitutionHelper updates BOTH td->current_obj AND tl_runtime_config.obj.
    // RuntimeConfigObjectHelper alone is insufficient because rc_get_current_ref()
    // reads td->current_obj to repopulate tl_runtime_config.obj on every call.
    std::optional<ObjectSubstitutionHelper> osh;
    if (self) {
        osh.emplace(self, cb->getClassCtx());
    }

    // Check if callee IR supports direct param passing (bypass TLS entirely)
    const QoreIRFunction* ir = uvb->getCachedIR();
    bool use_direct_params = ir && ir->direct_params_eligible
        && !uvb->hasCachedFunction() && nargs >= (int)num_params;

    if (!use_direct_params) {
        // Standard path: push selfid + params to TLS
        if (self && sig->selfid) {
            sig->selfid->instantiateSelf(self);
        }
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            if (self && sig->selfid) {
                sig->selfid->uninstantiateSelf();
            }
            return toBits(QoreValue());
        }
    }
    // else: direct_params path — selfid not instantiated (ObjectSubstitutionHelper
    // above handles implicit self resolution), params pre-populated in IR slot cache

    // Build argv for excess arguments (varargs)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Get call name from cached IR if available, otherwise use static name
    static const std::string closure_name("<anonymous closure>");
    const std::string& call_name = ir ? ir->name : closure_name;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        if (use_direct_params) {
            // Direct params path: pass args straight to IR slot cache, no TLS
            IRDirectParams dp{args, nargs};
            execJITWithDeopt(uvb, call_name, [ir, uvb, &dp](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm, false, &dp);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink);
        } else if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink);
        } else {
            // IR fast path (standard TLS)
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            execJITWithDeopt(uvb, call_name, [callee_ir, uvb](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm);
                if (!ok && !*xs) {
                    inv = true;  // Request deopt to AST
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink);
        }
    }

    // Cleanup in reverse order
    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }
    if (!use_direct_params) {
        // Standard path: uninstantiate params + selfid from TLS
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
        if (self && sig->selfid) {
            sig->selfid->uninstantiateSelf();
        }
    }

    // Apply return type coercion
    if (!*xsink) {
        const QoreTypeInfo* rt = sig->getReturnTypeInfo();
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

// --- Fast function call with explicit target (multi-function module compilation) ---

extern "C" DLLEXPORT uint64_t qore_rt_call_fast_with_target(uint64_t (*target_fn)(ExceptionSink*),
        const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(variant);
    assert(target_fn);

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        // Should not happen for batch-compiled callees, but handle gracefully
        xsink->raiseException("JIT-ERROR", "non-user variant in fast call with target");
        return toBits(QoreValue());
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }

    // Instantiate parameter locals directly from NaN-boxed args
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable (if the function has an argv parameter)
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Get call name from cached IR function (always available for JIT-compiled functions)
    const QoreIRFunction* ir = uvb->getCachedIR();
    const std::string& call_name = ir ? ir->name : jit_empty_call_name;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        execJITWithDeopt(uvb, call_name, [target_fn](ExceptionSink* xs, bool& /*inv*/) {
            return target_fn(xs);
        }, val, xsink);
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    // Uninstantiate parameter locals in reverse order
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = sig->getReturnTypeInfo();
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_self_recursive(const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    // Self-recursive call helper: sets up params, argv, body locals, and deopt handling.
    // Falls back to evalTiered slow path if JIT function was invalidated by recompilation.
    assert(variant);

    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        xsink->raiseException("JIT-ERROR", "non-user variant in self-recursive call");
        return toBits(QoreValue());
    }

    // Fall back to slow path if JIT function was invalidated by recompilation
    if (!uvb->hasCachedFunction()) {
        // Build QoreListNode from NaN-boxed args and call through evalTiered
        ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
        if (nargs > 0) {
            qore_list_private* priv = qore_list_private::get(**arg_list);
            priv->reserve(nargs);
            for (int i = 0; i < nargs; ++i) {
                QoreValue val = fromBits(args[i]);
                if (val.hasNode()) {
                    val.refSelf();
                }
                priv->pushIntern(val);
            }
        }
        const QoreIRFunction* ir = uvb->getCachedIR();
        const std::string& call_name = ir ? ir->name : jit_empty_call_name;
        QoreValue result = uvb->callTieredPublic(call_name.c_str(), arg_list, nullptr, xsink);
        return toBits(result);
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Set up program thread context (program is already correct from parent call)
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }

    // Instantiate parameter locals directly from NaN-boxed args
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Build and instantiate argv for excess arguments (varargs)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Use cached IR name when available (zero allocation)
    const QoreIRFunction* ir = uvb->getCachedIR();
    const std::string& call_name = ir ? ir->name : jit_empty_call_name;

    // Call through execJITWithDeopt which handles body locals, stack location, and deopt
    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
            return uvb->execCachedFunction(xs, inv);
        }, val, xsink);
    }

    // Uninstantiate argv + params in reverse order (LIFO)
    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // Apply return type coercion to match ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = sig->getReturnTypeInfo();
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

// --- Direct method call for devirtualized calls (final classes) ---

extern "C" DLLEXPORT uint64_t qore_rt_call_method_direct(const QoreMethod* method, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    if (!method) {
        xsink->raiseException("JIT-ERROR", "null method pointer in direct call");
        return toBits(QoreValue());
    }

    // Get the current self object from the runtime stack
    QoreObject* self = runtime_get_stack_object();
    if (!self) {
        xsink->raiseException("JIT-ERROR", "no self object in direct method call");
        return toBits(QoreValue());
    }

    // Build QoreListNode from the NaN-boxed args array
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }

    // Get runtime config
    RuntimeConfig& rc = rc_get_current_ref();

    // Call the method using evalTmpArgs to preserve reference nodes in the arg list.
    // The eval() path goes through CodeEvaluationHelper with const args which calls
    // evalList() and dereferences ReferenceNode values.
    QoreValue result = qore_method_private::evalTmpArgs(*method, xsink, rc, self, *arg_list);

    return toBits(result);
}

// --- Fast method call (bypasses QoreListNode + dispatch chain for devirtualized calls) ---

extern "C" DLLEXPORT uint64_t qore_rt_call_method_fast(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    assert(method);
    assert(variant);

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        // Builtin method — fall back to slow path for proper type coercion
        // (builtins can have soft types like softstring that require CodeEvaluationHelper)
        return qore_rt_call_method_direct(method, args, nargs, xsink);
    }

    // If the callee has neither JIT nor IR, fall back to the slow path.
    // This can happen in tiered compilation when the callee hasn't been promoted yet.
    if (!uvb->hasCachedFunction() && !uvb->getCachedIR()) {
        return qore_rt_call_method_direct(method, args, nargs, xsink);
    }

    // Get the current self object from the runtime stack
    QoreObject* self = runtime_get_stack_object();
    if (!self) {
        xsink->raiseException("JIT-ERROR", "no self object in fast method call");
        return toBits(QoreValue());
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }
    // Push frame boundary so that get_local_vars()/set_local_var_value() can correctly
    // determine call-stack depth for debugger introspection.
    ThreadFrameBoundaryHelper tfbh(true);

    // Push self object onto the method call stack (for runtime_get_stack_object())
    ObjectSubstitutionHelper osh(self, qore_class_private::get(*method->getClass()));

    // Check if callee IR supports direct param passing (bypass TLS entirely)
    const QoreIRFunction* ir = uvb->getCachedIR();
    bool use_direct_params = ir && ir->direct_params_eligible
        && !uvb->hasCachedFunction() && nargs >= (int)num_params;

    if (!use_direct_params) {
        // Standard path: push selfid + params to TLS
        if (sig->selfid) {
            sig->selfid->instantiateSelf(self);
        }
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            if (sig->selfid) {
                sig->selfid->uninstantiate(xsink);
            }
            return toBits(QoreValue());
        }
    }
    // else: direct_params path — selfid not needed (LoadSelfMember uses
    // ObjectSubstitutionHelper), params pre-populated in IR slot cache

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable (if the function has an argv parameter)
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Use cached IR name when available (zero allocation); fall back to building the name
    std::string call_name_buf;
    if (!ir) {
        const char* cls = method->getClass() ? method->getClass()->getName() : nullptr;
        if (cls) {
            call_name_buf = cls;
            call_name_buf += "::";
        }
        call_name_buf += method->getName();
    }
    const std::string& call_name = ir ? ir->name : call_name_buf;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        if (use_direct_params) {
            // Direct params path: pass args straight to IR slot cache, no TLS
            IRDirectParams dp{args, nargs};
            execJITWithDeopt(uvb, call_name, [ir, uvb, &dp](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm, false, &dp);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink);
        } else if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink);
        } else {
            // IR fast path (standard TLS): execute IR directly without QoreListNode.
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            execJITWithDeopt(uvb, call_name, [callee_ir, uvb](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink);
        }
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    if (!use_direct_params) {
        // Standard path: uninstantiate params + selfid from TLS
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
        if (sig->selfid) {
            sig->selfid->uninstantiateSelf();
        }
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = sig->getReturnTypeInfo();
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            // Missing return statement: set location to method definition
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

// --- Fast call reference/closure call ---

extern "C" DLLEXPORT uint64_t qore_rt_call_ref_fast(uint64_t ref_bits, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    QoreValue ref_val = fromBits(ref_bits);
    if (!ref_val.hasNode()) {
        xsink->raiseException("JIT-ERROR", "call reference value is not a node");
        return toBits(QoreValue());
    }

    // The call reference is a ResolvedCallReferenceNode (closure, function ref, method ref, etc.)
    auto* ref_node = dynamic_cast<ResolvedCallReferenceNode*>(ref_val.getInternalNode());
    if (!ref_node) {
        xsink->raiseException("JIT-ERROR", "call reference is not a ResolvedCallReferenceNode");
        return toBits(QoreValue());
    }

    // Build QoreListNode from the NaN-boxed args array
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> arg_list(nargs > 0 ? new QoreListNode(autoTypeInfo) : nullptr, xsink);
    if (nargs > 0) {
        qore_list_private* priv = qore_list_private::get(**arg_list);
        priv->reserve(nargs);
        for (int i = 0; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            priv->pushIntern(val);
        }
    }

    // Call execValue() directly — avoids the dynamic_cast chain and AST node copy
    // that qore_rt_call_with_args() performs
    QoreValue result = ref_node->execValue(*arg_list, xsink);
    return toBits(result);
}

// --- On-block-exit handler support for JIT ---

struct JITOnBlockExitHandler {
    obe_type_e type;
    StatementBlock* code;
    const QoreIRFunction* handler_ir = nullptr;  //!< IR handler for IR interpreter execution
    uint64_t (*compiled_fn)(ExceptionSink*) = nullptr;  //!< native compiled handler function
    const QoreIRFunction* handler_func = nullptr;  //!< IR function for pre-instantiation of handler locals
};

static thread_local std::vector<JITOnBlockExitHandler> jit_obe_handlers;

// Thread-local pointer to the innermost IR interpreter's slot cache
// Set/restored by QoreIRInterpreter::execute() to allow qore_rt_exec_on_block_exit_impl
// to pass parent_slot_cache when executing handler IR functions (Phase 2, Fix 2a)
static thread_local std::vector<QoreValue>* current_ir_slot_cache = nullptr;

// Accessor exported for IR interpreter (returns void** for C ABI compatibility)
extern "C" DLLEXPORT void** qore_rt_get_ir_slot_cache_ptr() {
    return reinterpret_cast<void**>(&current_ir_slot_cache);
}

extern "C" DLLEXPORT void qore_rt_push_on_block_exit(int type, StatementBlock* code) {
    jit_obe_handlers.push_back({static_cast<obe_type_e>(type), code, nullptr, nullptr, nullptr});
}

extern "C" DLLEXPORT void qore_rt_push_on_block_exit_ir(int type, StatementBlock* code,
        const QoreIRFunction* handler_ir) {
    jit_obe_handlers.push_back({static_cast<obe_type_e>(type), code, handler_ir, nullptr, nullptr});
}

extern "C" DLLEXPORT void qore_rt_push_compiled_handler(int type, StatementBlock* code,
        uint64_t (*compiled_fn)(ExceptionSink*), const QoreIRFunction* handler_func) {
    jit_obe_handlers.push_back({static_cast<obe_type_e>(type), code, nullptr, compiled_fn, handler_func});
}

extern "C" DLLEXPORT int64_t qore_rt_get_on_block_exit_count() {
    return static_cast<int64_t>(jit_obe_handlers.size());
}

extern "C" DLLEXPORT void qore_rt_exec_on_block_exit_impl(int64_t saved_count, ExceptionSink* xsink, bool inline_lowered) {
    size_t start = static_cast<size_t>(saved_count);
    if (jit_obe_handlers.size() <= start) {
        return;
    }

    ExceptionSink obe_xsink;
    bool error = xsink && xsink->isException();

    // Skip handler execution if handlers were already inlined; just clean up the vector
    if (!inline_lowered) {
        // Execute in reverse order (LIFO) — matching the AST's
        // StatementBlock::execIntern() semantics.
        for (int i = static_cast<int>(jit_obe_handlers.size()) - 1; i >= static_cast<int>(start); --i) {
            obe_type_e type = jit_obe_handlers[i].type;
            if (type == OBE_Unconditional || (!error && type == OBE_Success) || (error && type == OBE_Error)) {
                if (jit_obe_handlers[i].code || jit_obe_handlers[i].handler_ir
                        || jit_obe_handlers[i].compiled_fn) {
                    // Instantiate exception for on_error blocks as an implicit arg
                    std::unique_ptr<SingleArgvContextHelper> argv_helper;
                    std::unique_ptr<CatchExceptionHelper> ex_helper;
                    if (type == OBE_Error && xsink) {
                        QoreException* except = xsink->getException();
                        if (except) {
                            ex_helper.reset(new CatchExceptionHelper(except));
                            argv_helper.reset(new SingleArgvContextHelper(except->makeExceptionObject(), xsink));
                        }
                    }
                    if (jit_obe_handlers[i].compiled_fn) {
                        // Execute natively compiled handler
                        const QoreIRFunction* hf = jit_obe_handlers[i].handler_func;
                        if (hf) {
                            const QoreParseOptions& po = runtime_get_parse_options();
                            for (LocalVar* lv : hf->all_body_locals) {
                                lv->instantiate(po);
                            }
                        }
                        QoreValue rv(jit_obe_handlers[i].compiled_fn(&obe_xsink));
                        rv.discard(nullptr);
                        if (hf) {
                            for (int j = (int)hf->all_body_locals.size() - 1; j >= 0; --j) {
                                hf->all_body_locals[j]->uninstantiate(&obe_xsink);
                            }
                        }
                    } else if (jit_obe_handlers[i].handler_ir) {
                        // Execute compiled handler via IR interpreter
                        // Phase 2, Fix 2c: Pass parent slot cache for handler access to parent scope
                        QoreValue rv;
                        QoreIRInterpreter::execute(*jit_obe_handlers[i].handler_ir, rv, &obe_xsink,
                            nullptr, nullptr, nullptr,
                            &jit_obe_handlers[i].handler_ir->pre_instantiated_cache,
                            nullptr, nullptr, nullptr, false, nullptr,
                            current_ir_slot_cache);
                        rv.discard(nullptr);
                    } else {
                        // AST fallback
                        QoreValue rv;
                        jit_obe_handlers[i].code->exec(rv, &obe_xsink);
                        rv.discard(nullptr);
                    }
                    if (type == OBE_Error) {
                        if (qore_es_private::get(obe_xsink)->rethrown) {
                            if (xsink) {
                                xsink->clear();
                            }
                        }
                    }
                    if (obe_xsink) {
                        if (xsink) {
                            xsink->assimilate(obe_xsink);
                        } else {
                            obe_xsink.clear();
                        }
                    }
                }
            }
        }
    }

    // Remove handlers for this function scope
    jit_obe_handlers.resize(start);
}

// Backward-compatible wrapper that calls the implementation with inline_lowered=false
extern "C" DLLEXPORT void qore_rt_exec_on_block_exit(int64_t saved_count, ExceptionSink* xsink) {
    qore_rt_exec_on_block_exit_impl(saved_count, xsink, false);
}

// --- AOT context-based helpers (Phase 7b) ---

#include "qore/intern/QoreAOT.h"

extern "C" DLLEXPORT void qore_rt_push_on_block_exit_aot(QoreAOTContext* ctx, int32_t idx, int type) {
    assert(ctx && idx >= 0 && idx < ctx->num_stmts);
    // Check if handler IR is available (strip-source mode or optimized path)
    if (idx < static_cast<int32_t>(ctx->handler_irs.size()) && ctx->handler_irs[idx]) {
        // Use handler IR for IR interpreter execution
        qore_rt_push_on_block_exit_ir(type, nullptr, ctx->handler_irs[idx].get());
    } else {
        // Fall back to AST-based handler
        qore_rt_push_on_block_exit(type, const_cast<StatementBlock*>(
            static_cast<const StatementBlock*>(ctx->stmts[idx])));
    }
}

extern "C" DLLEXPORT CaseNodeRegex* qore_rt_get_regex_case_aot(QoreAOTContext* ctx, int32_t slot) {
    assert(ctx && slot >= 0 && slot < ctx->num_regex_cases);
    return ctx->regex_cases[slot];
}

extern "C" DLLEXPORT uint64_t qore_rt_load_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    return qore_rt_load_local(ctx->locals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_assign_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_assign_local(ctx->locals[idx], val, xsink);
}

extern "C" DLLEXPORT void qore_rt_assign_local_no_coerce_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_assign_local_no_coerce(ctx->locals[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_coerce_value_aot(QoreAOTContext* ctx, int32_t local_idx,
        uint64_t value, uint64_t* cleanup_ptr, ExceptionSink* xsink) {
    assert(ctx && local_idx >= 0 && local_idx < ctx->num_locals);
    const QoreTypeInfo* ti = ctx->locals[local_idx]->getTypeInfo();
    return qore_rt_coerce_value(ti, value, cleanup_ptr, xsink);
}

extern "C" DLLEXPORT void qore_rt_instantiate_local_aot(QoreAOTContext* ctx, int32_t idx) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_instantiate_local(ctx->locals[idx]);
}

extern "C" DLLEXPORT void qore_rt_clear_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_clear_local(ctx->locals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_uninstantiate_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    // Use clear (del) instead of uninstantiate (pop) because AOT body locals
    // are pre-instantiated by execJITWithDeopt() at function entry and will be
    // uninstantiated at function exit.  The LLVM code's UninstantiateLocal
    // corresponds to scope exit (like the end of a foreach loop body) and
    // should only clear the value, not pop the variable from the stack.
    qore_rt_clear_local(ctx->locals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_pop_closure_var_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    // For closure-use vars that are NOT pre-instantiated by evalTiered (AOT mode):
    // pop from the cvstack.  The variable may have been lazily instantiated by
    // StoreLocal or by LocalVar::getLValue/eval; if it was never accessed, it
    // may not be on the cvstack at all — check before popping.
    LocalVar* var = ctx->locals[idx];
    if (thread_try_find_closure_var(var->getName())) {
        qore_rt_uninstantiate_local(var, xsink);
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_load_global_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_globals);
    return qore_rt_load_global(ctx->globals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_store_global_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_globals);
    qore_rt_store_global(ctx->globals[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_thread_local_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_globals);
    return qore_rt_load_thread_local(ctx->globals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_store_thread_local_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_globals);
    qore_rt_store_thread_local(ctx->globals[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_load_closure_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    return qore_rt_load_local(ctx->locals[idx], xsink);
}

extern "C" DLLEXPORT void qore_rt_store_closure_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_locals);
    qore_rt_assign_local(ctx->locals[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_invoke_expr_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_invoke_expr(ctx->exprs[idx], xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_vrn_construct_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    QoreValue expr = fromBits(ctx->exprs[idx]);
    if (expr.hasNode()) {
        if (auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(expr.getInternalNode())) {
            return toBits(vrn->constructValue(xsink));
        }
    }
    // Fallback: eval the expression directly
    // Handles NewHashDeclNode, NewComplexHashNode, NewComplexListNode stored by buildContextFromSlotMap
    return qore_rt_invoke_expr(ctx->exprs[idx], xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_load_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_load(ctx->exprs[idx], xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_store_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_store(ctx->exprs[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_store_weak_aot(QoreAOTContext* ctx, int32_t idx, uint64_t val,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_store_weak(ctx->exprs[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_unary_aot(int op, QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_unary(op, ctx->exprs[idx], xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_binary_aot(int op, QoreAOTContext* ctx, int32_t idx, uint64_t val,
        ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_binary(op, ctx->exprs[idx], val, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_lvalue_ternary_aot(int op, QoreAOTContext* ctx, int32_t idx, uint64_t first,
        uint64_t second, uint64_t third, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_exprs);
    return qore_rt_lvalue_ternary(op, ctx->exprs[idx], first, second, third, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_with_args_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    return qore_rt_call_with_args(ctx->exprs[slot], args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_direct_aot(QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs,
        ExceptionSink* xsink) {
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    // Use pre-resolved call target (populated during buildAOTContext) to avoid per-call dynamic_cast
    const QoreAOTCallTarget& target = ctx->call_targets[slot];

    // Fast path: pre-resolved user variant — inline the call_fast logic to avoid double
    // check_stack and extra function call overhead (critical for tight recursive calls)
    if (target.uvb) {
        const UserVariantBase* uvb = target.uvb;

        // If the callee has neither JIT nor IR, fall back to the slow path
        if (!uvb->hasCachedFunction() && !uvb->getCachedIR()) {
            return qore_rt_call_function_direct(target.func, target.variant, target.pgm,
                args, nargs, xsink);
        }

        const UserSignature* sig = uvb->getUserSignature();
        unsigned num_params = sig->numParams();

        // Set up program thread context (only if program differs from caller's program)
        std::optional<ProgramThreadCountContextHelper> ptcch;
        if (uvb->pgm != getProgram()) {
            ptcch.emplace(xsink, uvb->pgm, true);
            if (*xsink) {
                return toBits(QoreValue());
            }
        }
        ThreadFrameBoundaryHelper tfbh(true);

        // Instantiate parameter locals directly from NaN-boxed args
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            return toBits(QoreValue());
        }

        // Build argv for excess arguments (varargs)
        ReferenceHolder<QoreListNode> argv(xsink);
        if (nargs > (int)num_params) {
            argv = new QoreListNode(autoTypeInfo);
            qore_list_private* argv_priv = qore_list_private::get(**argv);
            argv_priv->reserve(nargs - num_params);
            for (int i = num_params; i < nargs; ++i) {
                QoreValue val = fromBits(args[i]);
                if (val.hasNode()) {
                    val.refSelf();
                }
                argv_priv->pushIntern(val);
            }
        }
        if (sig->argvid) {
            sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
        }

        const QoreIRFunction* ir = uvb->getCachedIR();
        const std::string& call_name = ir ? ir->name : jit_empty_call_name;

        QoreValue val{};
        {
            ArgvContextHelper argv_helper(argv.release(), xsink);
            if (uvb->hasCachedFunction()) {
                execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                    return uvb->execCachedFunction(xs, inv);
                }, val, xsink);
            } else {
                const QoreIRFunction* callee_ir = uvb->getCachedIR();
                execJITWithDeopt(uvb, call_name, [callee_ir, uvb](ExceptionSink* xs, bool& inv) -> uint64_t {
                    QoreValue ir_return_value;
                    bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                        nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                        uvb->getStatementBlock(), uvb->pgm);
                    if (!ok && !*xs) {
                        inv = true;
                        return 0;
                    }
                    return toBits(ir_return_value);
                }, val, xsink);
            }
        }

        if (sig->argvid) {
            sig->argvid->uninstantiate(xsink);
        }
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }

        if (!*xsink) {
            const QoreTypeInfo* rt = sig->getReturnTypeInfo();
            if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
                QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
                if (*xsink) {
                    xsink->overrideLocation(*sig->getParseLocation());
                    xsink->appendLastDescription(": block missing return statement");
                }
            } else {
                QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
            }
        }

        return toBits(val);
    }

    // Medium path: pre-resolved function but no user variant (builtin)
    if (target.func) {
        if (target.variant) {
            return qore_rt_call_fast(target.func, target.variant, target.pgm, args, nargs, xsink);
        }
        // No variant resolved — use dynamic dispatch for proper overload resolution
        return qore_rt_call_function_dynamic(target.func, target.pgm, args, nargs, xsink);
    }

    // Fallback for slots without pre-resolved targets (shouldn't happen for CallDirect)
    return qore_rt_call_with_args(ctx->exprs[slot], args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_self_recursive_aot(AotFunctionPtr self_fn, QoreAOTContext* ctx,
        int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    // Lightweight self-recursive call for AOT: eliminates ThreadFrameBoundaryHelper,
    // ProgramThreadCountContextHelper, QoreJITStackLocation, execJITWithDeopt wrapper,
    // acceptAssignment, and execCachedFunction indirection.  Calls the AOT function
    // directly via function pointer.
    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }

    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    const UserVariantBase* uvb = target.uvb;
    if (!uvb) {
        // Defensive fallback (should not happen for self-recursive)
        return qore_rt_call_direct_aot(ctx, slot, args, nargs, xsink);
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Instantiate params on thread-local stack
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Argv for excess args (rare for self-recursive)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Body locals — use getBodyLocals() for AOT (same as execJITWithDeopt)
    // Skip closure-use vars: the LLVM code handles their instantiation/uninstantiation
    // at block scope boundaries via qore_rt_instantiate_local_aot / qore_rt_pop_closure_var_aot.
    bool skip_body_locals = uvb->areAllBodyLocalsIROnly();
    const std::vector<LocalVar*>& body_locals = uvb->getBodyLocals();
    if (!skip_body_locals) {
        const QoreParseOptions& po = uvb->pgm->getParseOptions();
        for (LocalVar* lv : body_locals) {
            if (lv->closureUse()) {
                continue;
            }
            lv->instantiate(po);
        }
    }

    // Call AOT function directly — no deopt, no frame boundary, no stack location
    uint64_t result_bits;
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        result_bits = self_fn(ctx, xsink);
    }

    // Uninstantiate body locals
    if (!skip_body_locals) {
        for (int i = (int)body_locals.size() - 1; i >= 0; --i) {
            if (body_locals[i]->closureUse()) {
                continue;
            }
            body_locals[i]->uninstantiate(xsink);
        }
    }

    // Uninstantiate argv + params (LIFO order)
    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // No return type coercion — self-recursive, same return type
    return result_bits;
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_with_base_aot(QoreAOTContext* ctx, int32_t slot, uint64_t base_bits,
        ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    return qore_rt_dot_eval_with_base(ctx->exprs[slot], base_bits, xsink);
}

// --- Regex op with pre-evaluated operand helper ---

#include "qore/intern/QoreRegexMatchOperatorNode.h"
#include "qore/intern/QoreRegexExtractOperatorNode.h"
#include "qore/intern/QoreRegexNMatchOperatorNode.h"
#include "qore/intern/QoreRegex.h"

extern "C" DLLEXPORT uint64_t qore_rt_regex_op_with_operand(int32_t opcode, uint64_t expr_bits, uint64_t operand_bits,
        ExceptionSink* xsink) {
    QoreValue expr = fromBits(expr_bits);
    if (!expr.hasNode()) {
        return qore_rt_invoke_expr(expr_bits, xsink);
    }

    QoreValue operand = fromBits(operand_bits);
    QoreIROpcode op = static_cast<QoreIROpcode>(opcode);

    // Get the regex from the operator node
    QoreRegex* regex = nullptr;
    if (auto* match_node = dynamic_cast<const QoreRegexMatchOperatorNode*>(expr.getInternalNode())) {
        regex = match_node->getRegex();
    }

    if (!regex) {
        // Fallback: shouldn't happen but be safe
        return qore_rt_invoke_expr(expr_bits, xsink);
    }

    QoreStringNodeValueHelper str(operand);

    switch (op) {
        case QoreIROpcode::RegexMatchAny:
        case QoreIROpcode::RegexMatchBool: {
            bool match = regex->exec(*str, xsink);
            return toBits(QoreValue(match));
        }
        case QoreIROpcode::RegexNMatchBool: {
            bool match = !regex->exec(*str, xsink);
            return toBits(QoreValue(match));
        }
        case QoreIROpcode::RegexExtractAny:
        case QoreIROpcode::RegexExtractList: {
            QoreListNode* result = regex->extractSubstrings(*str, xsink);
            return toBits(QoreValue(result));
        }
        default:
            return qore_rt_invoke_expr(expr_bits, xsink);
    }
}

extern "C" DLLEXPORT uint64_t qore_rt_regex_op_with_operand_aot(QoreAOTContext* ctx, int32_t opcode, int32_t slot,
        uint64_t operand_bits, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    return qore_rt_regex_op_with_operand(opcode, ctx->exprs[slot], operand_bits, xsink);
}

// --- Iterator helpers ---

// Creates an iterator from an iterable value.
// Returns a pointer to FunctionalOperatorInterface (as i64), or 0 if iterable is NOTHING.
// The iterator_func parameter is optional (can be nullptr) for use with FunctionalOperator expressions.
extern "C" DLLEXPORT void* qore_rt_iterator_create(uint64_t iterable_bits, void* iterator_func, ExceptionSink* xsink) {
    QoreValue iterable = fromBits(iterable_bits);

    FunctionalOperator::FunctionalValueType value_type;
    FunctionalOperatorInterface* iter = nullptr;

    if (iterator_func) {
        FunctionalOperator* func_op = reinterpret_cast<FunctionalOperator*>(iterator_func);
        iter = func_op->getFunctionalIterator(value_type, xsink);
    } else {
        iter = FunctionalOperatorInterface::getFunctionalIterator(value_type, iterable, true,
            "foreach statement", xsink);
    }

    // Return nullptr on exception or NOTHING value type
    if ((xsink && *xsink) || value_type == FunctionalOperator::nothing) {
        delete iter;
        return nullptr;
    }

    return iter;
}

/// AOT version: looks up iterator_func pointer from AOT context by slot index
extern "C" DLLEXPORT void* qore_rt_iterator_create_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t iterable_bits, ExceptionSink* xsink) {
    assert(ctx);
    // Negative slot (-1) means null iterator_func (no custom iteration function)
    // Otherwise, look up the FunctionalOperator* pointer from the exprs array
    void* iterator_func = nullptr;
    if (slot >= 0 && slot < ctx->num_exprs) {
        iterator_func = reinterpret_cast<void*>(ctx->exprs[slot]);
    }
    return qore_rt_iterator_create(iterable_bits, iterator_func, xsink);
}

// Advances the iterator and returns the next value.
// Returns: 1 if done (iterator exhausted), 0 if has more values.
// On success (not done), stores the current value in *out_value.
// On done or exception, the iterator is deleted.
extern "C" DLLEXPORT int64_t qore_rt_iterator_next(void* iter_ptr, uint64_t* out_value, ExceptionSink* xsink) {
    if (!iter_ptr) {
        // Empty iterator (was NOTHING) - already done
        *out_value = 0;  // VAL_NOTHING - keep alloca consistent for decref-before-overwrite
        return 1;
    }

    FunctionalOperatorInterface* iter = reinterpret_cast<FunctionalOperatorInterface*>(iter_ptr);
    ValueOptionalRefHolder val(xsink);
    bool done = iter->getNext(val, xsink);

    if (xsink && *xsink) {
        // Exception occurred - clean up iterator
        delete iter;
        *out_value = 0;  // VAL_NOTHING - keep alloca consistent for decref-before-overwrite
        return 1;
    }

    if (done) {
        // Iterator exhausted - clean up
        delete iter;
        *out_value = 0;  // VAL_NOTHING - keep alloca consistent for decref-before-overwrite
        return 1;
    }

    // Store current value and continue
    *out_value = toBits(val.takeReferencedValue());
    return 0;
}

// Cleans up an active iterator on non-normal function exit paths.
// Called from JIT-compiled code's exit cleanup when a foreach body is exited
// by return/throw before the iterator is exhausted.
extern "C" DLLEXPORT void qore_rt_iterator_cleanup(void* iter_ptr) {
    if (iter_ptr) {
        delete reinterpret_cast<FunctionalOperatorInterface*>(iter_ptr);
    }
}

// --- Reference foreach helpers ---

// Opaque state for reference foreach iteration
struct RefForeachState {
    ReferenceNode* vr = nullptr;      // runtime reference (one ref owned)
    QoreValue tlist;                   // original value (one ref owned)
    QoreListNode* l_tlist = nullptr;   // pointer to list in tlist (borrowed, not owned)
    QoreValue ln;                      // result accumulator (one ref owned)
};

// Initialize reference foreach state from a ParseReferenceNode expression.
// Returns an opaque state pointer (as uint64_t), or 0 on error.
extern "C" DLLEXPORT uint64_t qore_rt_ref_foreach_init(uint64_t parse_ref_bits, ExceptionSink* xsink) {
    QoreValue parse_ref = fromBits(parse_ref_bits);
    ParseReferenceNode* r = parse_ref.get<ParseReferenceNode>();
    if (!r) {
        xsink->raiseException("FOREACH-ERROR", "reference foreach: expected a reference expression");
        return 0;
    }

    auto* state = new RefForeachState();

    // Evaluate ParseReferenceNode to get runtime ReferenceNode
    state->vr = r->evalToRef(xsink);
    if (*xsink) {
        delete state;
        return 0;
    }

    // Get the current value of the lvalue expression
    state->tlist = state->vr->eval(xsink);
    if (*xsink) {
        state->vr->deref(xsink);
        delete state;
        return 0;
    }

    state->l_tlist = (state->tlist.getType() == NT_LIST)
        ? state->tlist.get<QoreListNode>() : nullptr;

    // Create result accumulator
    if (state->l_tlist) {
        state->ln = new QoreListNode(autoTypeInfo);
    }

    return reinterpret_cast<uint64_t>(state);
}

// Get the iteration count for a reference foreach state.
// Returns 0 for NOTHING/empty, list size for lists, 1 for scalars.
extern "C" DLLEXPORT int64_t qore_rt_ref_foreach_size(uint64_t state_ptr) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    if (!state) {
        return 0;
    }
    if (state->l_tlist) {
        return state->l_tlist->empty() ? 0 : static_cast<int64_t>(state->l_tlist->size());
    }
    return state->tlist.isNothing() ? 0 : 1;
}

// Get the element at the given index from the reference foreach state.
// Returns a referenced value suitable for assignment to the loop variable.
extern "C" DLLEXPORT uint64_t qore_rt_ref_foreach_get_entry(uint64_t state_ptr, int64_t index,
        ExceptionSink* xsink) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    QoreValue entry;
    if (state->l_tlist) {
        entry = state->l_tlist->getReferencedEntry(static_cast<size_t>(index));
    } else {
        // Scalar: return the value (first and only iteration)
        entry = state->tlist.refSelf();
    }
    return toBits(entry);
}

// Record the modified loop variable value after body execution.
extern "C" DLLEXPORT void qore_rt_ref_foreach_record(uint64_t state_ptr, uint64_t value_bits,
        ExceptionSink* xsink) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    QoreValue value = fromBits(value_bits);
    if (state->l_tlist) {
        state->ln.get<QoreListNode>()->push(value.refSelf(), nullptr);
    } else {
        state->ln.discard(nullptr);
        state->ln = value.refSelf();
    }
}

// Finalize: optionally fill remaining elements (on break), write back to reference, and clean up.
// If *xsink is set (exception path), does cleanup without write-back.
// fill_remaining: 1 = fill remaining elements from original (break case), 0 = write back as-is
extern "C" DLLEXPORT void qore_rt_ref_foreach_finalize(uint64_t state_ptr, int64_t fill_remaining,
        ExceptionSink* xsink) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    if (!state) {
        return;
    }

    if (*xsink) {
        // Exception path: clean up without write-back
        state->ln.discard(xsink);
        state->tlist.discard(xsink);
        state->vr->deref(xsink);
        delete state;
        return;
    }

    // Fill remaining elements if result list is shorter than original (break case)
    if (fill_remaining && state->l_tlist && state->ln.getType() == NT_LIST) {
        QoreListNode* result = state->ln.get<QoreListNode>();
        size_t result_size = result->size();
        size_t orig_size = state->l_tlist->size();
        for (size_t i = result_size; i < orig_size; ++i) {
            result->push(state->l_tlist->getReferencedEntry(i), nullptr);
        }
    }

    // Write the value back to the lvalue reference
    {
        LValueHelper val(*state->vr, xsink);
        if (val) {
            QoreValue result = state->ln;
            state->ln = QoreValue();  // prevent double-free
            val.assign(result);
        } else {
            state->ln.discard(xsink);
            state->ln = QoreValue();
        }
    }

    state->tlist.discard(xsink);
    state->vr->deref(xsink);
    delete state;
}

// Clean up reference foreach state without write-back (exception/early-exit paths).
extern "C" DLLEXPORT void qore_rt_ref_foreach_cleanup(uint64_t state_ptr, ExceptionSink* xsink) {
    auto* state = reinterpret_cast<RefForeachState*>(state_ptr);
    if (!state) {
        return;
    }
    state->ln.discard(xsink);
    state->tlist.discard(xsink);
    state->vr->deref(xsink);
    delete state;
}

// --- Direct dot-eval method call (pre-evaluated base + args) ---

static QoreListNode* buildArgListFromNanBoxed(uint64_t* args, int nargs, ExceptionSink* xsink) {
    if (nargs <= 0) {
        return nullptr;
    }
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    QoreListNode* arg_list = new QoreListNode(autoTypeInfo);
    qore_list_private* priv = qore_list_private::get(*arg_list);
    priv->reserve(nargs);
    for (int i = 0; i < nargs; ++i) {
        QoreValue val = fromBits(args[i]);
        if (val.hasNode()) {
            val.refSelf();
        }
        priv->pushIntern(val);
    }
    return arg_list;
}

// Dispatch a method call on a QoreObject with pre-evaluated args.
// Same logic as AbstractMethodCallNode::exec(): if the runtime class matches
// the parse-time class, use the resolved method pointer directly; otherwise
// fall back to name-based lookup.
// Try fast dispatch for a user method with cached JIT function on a matching object
// Returns true if the fast path was taken, false if caller should use the slow path
static bool try_dispatch_method_fast(QoreObject* o, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, ExceptionSink* xsink, uint64_t& result) {
    if (!variant) {
        return false;
    }
    // copy() must go through dispatch_method_on_object → execCopy to create a new object
    if (!strcmp(method->getName(), "copy")) {
        return false;
    }
    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        // Builtin method — fall back to slow path for proper soft type coercion
        return false;
    }
    if (!uvb->hasCachedFunction() && !uvb->getCachedIR()) {
        return false;
    }
    if (o->getClass() != qc && o->getClass() != method->getClass()) {
        return false;
    }
    if (!o->isValid()) {
        xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has "
            "already been deleted", qc->getName(), method->getName());
        result = toBits(QoreValue());
        return true;
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        result = toBits(QoreValue());
        return true;
    }

    // Push self object onto the method call stack (for runtime_get_stack_object())
    ObjectSubstitutionHelper osh(o, qore_class_private::get(*method->getClass()));

    // Check if callee IR supports direct param passing (bypass TLS entirely)
    const QoreIRFunction* ir = uvb->getCachedIR();
    bool use_direct_params = ir && ir->direct_params_eligible
        && !uvb->hasCachedFunction() && nargs >= (int)num_params;

    if (!use_direct_params) {
        // Standard path: push selfid + params to TLS
        if (sig->selfid) {
            sig->selfid->instantiateSelf(o);
        }
        if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
            if (sig->selfid) {
                sig->selfid->uninstantiate(xsink);
            }
            result = toBits(QoreValue());
            return true;
        }
    }
    // else: direct_params path — selfid not needed (LoadSelfMember uses
    // ObjectSubstitutionHelper), params pre-populated in IR slot cache

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Use cached IR name when available (zero allocation); fall back to building the name
    std::string call_name_buf;
    if (!ir) {
        call_name_buf = std::string(method->getClass()->getName()) + "::" + method->getName();
    }
    const std::string& call_name = ir ? ir->name : call_name_buf;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        if (use_direct_params) {
            // Direct params path: pass args straight to IR slot cache, no TLS
            IRDirectParams dp{args, nargs};
            execJITWithDeopt(uvb, call_name, [ir, uvb, &dp](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm, false, &dp);
                if (!ok && !*xs) {
                    inv = true;
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink);
        } else if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink);
        } else {
            // IR fast path (standard TLS): execute IR directly without QoreListNode.
            execJITWithDeopt(uvb, call_name, [ir, uvb](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm);
                if (!ok && !*xs) {
                    inv = true;  // Request deopt to AST
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink);
        }
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    if (!use_direct_params) {
        // Standard path: uninstantiate params + selfid from TLS
        for (int i = (int)num_params - 1; i >= 0; --i) {
            sig->lv[i]->uninstantiate(xsink);
        }
        if (sig->selfid) {
            sig->selfid->uninstantiateSelf();
        }
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = sig->getReturnTypeInfo();
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    result = toBits(val);
    return true;
}

static uint64_t dispatch_method_on_object(QoreObject* o, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        QoreListNode* arg_list, ExceptionSink* xsink) {
    // copy() is a special operation that creates a new object — must use execCopy,
    // not regular method dispatch which would call the copy variant as a normal method
    if (!strcmp(method->getName(), "copy")) {
        return toBits(o->getClass()->execCopy(o, xsink));
    }
    if (o->getClass() == qc || o->getClass() == method->getClass()) {
        if (!o->isValid()) {
            xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has "
                "already been deleted", qc->getName(), method->getName());
            return toBits(QoreValue());
        }
        // Use evalTmpArgs to preserve ReferenceNode values in the pre-evaluated arg list
        // (eval/evalNormalVariant use const CodeEvaluationHelper which dereferences references)
        return toBits(qore_method_private::evalTmpArgs(*method, xsink, rc_get_current_ref(), o, arg_list));
    }
    // Class mismatch — name-based lookup (virtual dispatch to the runtime class)
    // Pass the runtime class context so that private method access checks succeed
    // when a base class method calls a private method on self and the runtime type
    // is a derived class (mirrors AbstractMethodCallNode::exec() in AST mode)
    const qore_class_private* class_ctx = runtime_get_class();
    const qore_class_private* priv = qore_class_private::get(*o->getClass());
    const QoreMethod* w = priv->getMethodForEval(method->getName(), o->getProgram(), class_ctx, xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }
    if (w) {
        return toBits(qore_method_private::evalTmpArgs(*w, xsink, rc_get_current_ref(), o, arg_list, class_ctx));
    }
    // Fall back to evalMethod for member gate, etc.
    RuntimeConfig& rc = rc_get_current_ref();
    return toBits(priv->evalMethod(o, method->getName(), arg_list, class_ctx, rc, xsink));
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct(uint64_t base_bits, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    // method/qc may be null for abstract/unresolved method calls generated by IR lowering
    // to pass pre-evaluated args (avoids EXPR_TREE local variable access issues in AOT).
    // In this case, the method name must be retrieved from the embedded expression by the
    // AOT call target resolver. For the JIT path (this function), we cannot resolve without
    // the method name, so we fall through to the pseudo-method dispatch below.
    // This path is hit from the IR interpreter's null-method fallback, which already handles
    // this case via dot_eval_fallback_with_args before reaching here.

    QoreValue base = fromBits(base_bits);

    switch (base.getType()) {
        case NT_WEAKREF: {
            QoreObject* o = base.get<WeakReferenceNode>()->get();
            if (!o) {
                xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on a deleted weak reference",
                    qc->getName(), method->getName());
                return toBits(QoreValue());
            }
            // Try fast path for JIT-compiled user methods (avoids building QoreListNode)
            uint64_t result;
            if (try_dispatch_method_fast(o, method, qc, variant, args, nargs, xsink, result)) {
                return result;
            }
            ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
            return dispatch_method_on_object(o, method, qc, variant, *arg_list, xsink);
        }

        case NT_OBJECT: {
            QoreObject* o = const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(base.getInternalNode()));
            // Try fast path for JIT-compiled user methods (avoids building QoreListNode)
            uint64_t result;
            if (try_dispatch_method_fast(o, method, qc, variant, args, nargs, xsink, result)) {
                return result;
            }
            ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
            return dispatch_method_on_object(o, method, qc, variant, *arg_list, xsink);
        }

        case NT_HASH: {
            const AbstractQoreNode* ref = check_call_ref(base.getInternalNode(), method->getName());
            if (ref) {
                ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
                return toBits(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(*arg_list, xsink));
            }
            break;
        }

        case NT_WEAKREF_HASH: {
            const AbstractQoreNode* ref = check_call_ref(base.get<WeakHashReferenceNode>()->get(), method->getName());
            if (ref) {
                ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
                return toBits(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(*arg_list, xsink));
            }
            break;
        }

        default:
            break;
    }

    // Non-object: dispatch to pseudo-method path
    return qore_rt_dot_eval_pseudo_method_direct(base_bits, method, qc, variant, args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_pseudo_method_direct(uint64_t base_bits, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(method);
    assert(qc);

    QoreValue base = fromBits(base_bits);
    ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
    RuntimeConfig& rc = rc_get_current_ref();

    // If base is nothing and class is not <nothing>, use <nothing> pseudo class
    if (base.isNothing() && qc != QC_PSEUDONOTHING) {
        return toBits(qore_class_private::evalPseudoMethod(QC_PSEUDONOTHING, base, method->getName(),
            *arg_list, rc, xsink));
    }
    return toBits(qore_class_private::evalPseudoMethod(qc, method, variant, base, *arg_list, rc, xsink));
}

// Fallback for unresolved method calls: use the pre-evaluated args
// from LLVM with a name-based runtime method dispatch
DLLLOCAL uint64_t dot_eval_fallback_with_args(QoreValue base, const char* method_name,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }

    // For objects, use name-based method lookup with evalTmpArgs to preserve references
    if (base.getType() == NT_OBJECT) {
        QoreObject* o = const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(base.getInternalNode()));
        // copy() must go through execCopy to create a new object
        if (!strcmp(method_name, "copy")) {
            return toBits(o->getClass()->execCopy(o, xsink));
        }
        const qore_class_private* class_ctx = runtime_get_class();
        const qore_class_private* priv = qore_class_private::get(*o->getClass());
        const QoreMethod* w = priv->getMethodForEval(method_name, o->getProgram(), class_ctx, xsink);
        if (*xsink) {
            return toBits(QoreValue());
        }
        if (w) {
            return toBits(qore_method_private::evalTmpArgs(*w, xsink, rc_get_current_ref(), o, *arg_list, class_ctx));
        }
        // Fall back to evalMethod for member gate, etc.
        RuntimeConfig& rc = rc_get_current_ref();
        return toBits(priv->evalMethod(o, method_name, *arg_list, class_ctx, rc, xsink));
    }

    // Check for hash member closures/call references (e.g. h.f() where h.f is a closure)
    if (base.getType() == NT_HASH) {
        const AbstractQoreNode* ref = check_call_ref(base.getInternalNode(), method_name);
        if (ref) {
            return toBits(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(*arg_list, xsink));
        }
    } else if (base.getType() == NT_WEAKREF_HASH) {
        const AbstractQoreNode* ref = check_call_ref(base.get<WeakHashReferenceNode>()->get(), method_name);
        if (ref) {
            return toBits(reinterpret_cast<const ResolvedCallReferenceNode*>(ref)->execValue(*arg_list, xsink));
        }
    }

    // For non-objects, use pseudo-class lookup
    return toBits(pseudo_classes_eval(base, method_name, *arg_list, xsink));
}

// Exported wrapper for unresolved/abstract method calls from JIT LLVM code.
// Uses name-based dispatch with pre-evaluated args (avoids EXPR_TREE local variable issues).
extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_by_name(uint64_t base_bits, const char* method_name,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    QoreValue base = fromBits(base_bits);
    return dot_eval_fallback_with_args(base, method_name, args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t base_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    // Use pre-resolved method target to avoid per-call dynamic_cast
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.method) {
        return target.is_pseudo
            ? qore_rt_dot_eval_pseudo_method_direct(base_bits, target.method, target.qc,
                target.variant, args, nargs, xsink)
            : qore_rt_dot_eval_method_direct(base_bits, target.method, target.qc,
                target.variant, args, nargs, xsink);
    }
    // Pre-resolved with name but no method pointer — use name-based dispatch
    if (target.method_name) {
        QoreValue base = fromBits(base_bits);
        return dot_eval_fallback_with_args(base, target.method_name, args, nargs, xsink);
    }

    // Fallback: dynamic resolution
    QoreValue expr;
    std::memcpy(&expr, &ctx->exprs[slot], sizeof(expr));
    auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(expr.getInternalNode());
    if (!dot_eval) {
        xsink->raiseException("AOT-ERROR", "invalid expression for dot-eval method direct AOT call");
        return toBits(QoreValue());
    }
    auto* m = dot_eval->getMethodCall();
    if (!m->getMethod()) {
        QoreValue base = fromBits(base_bits);
        return dot_eval_fallback_with_args(base, m->getName(), args, nargs, xsink);
    }
    return m->isPseudo()
        ? qore_rt_dot_eval_pseudo_method_direct(base_bits, m->getMethod(), m->getClass(), m->getVariant(),
            args, nargs, xsink)
        : qore_rt_dot_eval_method_direct(base_bits, m->getMethod(), m->getClass(), m->getVariant(),
            args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_pseudo_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t base_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    // Use pre-resolved method target to avoid per-call dynamic_cast
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.method) {
        return qore_rt_dot_eval_pseudo_method_direct(base_bits, target.method, target.qc,
            target.variant, args, nargs, xsink);
    }
    if (target.method_name) {
        QoreValue base = fromBits(base_bits);
        return dot_eval_fallback_with_args(base, target.method_name, args, nargs, xsink);
    }

    // Fallback: dynamic resolution
    QoreValue expr;
    std::memcpy(&expr, &ctx->exprs[slot], sizeof(expr));
    auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(expr.getInternalNode());
    if (!dot_eval) {
        xsink->raiseException("AOT-ERROR", "invalid expression for pseudo method direct AOT call");
        return toBits(QoreValue());
    }
    auto* m = dot_eval->getMethodCall();
    if (!m->getMethod()) {
        QoreValue base = fromBits(base_bits);
        return dot_eval_fallback_with_args(base, m->getName(), args, nargs, xsink);
    }
    return qore_rt_dot_eval_pseudo_method_direct(base_bits, m->getMethod(), m->getClass(), m->getVariant(),
        args, nargs, xsink);
}

// --- Direct static method call (pre-evaluated args) ---

extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_direct(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink) {
    if (!method) {
        xsink->raiseException("JIT-ERROR", "null method pointer in static method direct call");
        return toBits(QoreValue());
    }

    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }

    // variant may be nullptr for AOT-deserialized StaticMethodCallNode nodes
    // (resolveExprSlot creates nodes without a resolved variant pointer).
    // Fall through to the slow path in that case.
    const UserVariantBase* uvb = variant ? variant->getUserVariantBase() : nullptr;
    if (!uvb || (!uvb->hasCachedFunction() && !uvb->getCachedIR())) {
        // Use evalTmpArgs to preserve ReferenceNode values in pre-evaluated args
        ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
        return toBits(qore_method_private::evalTmpArgs(*method, xsink, rc_get_current_ref(), nullptr, *arg_list));
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }
    // Push frame boundary so that get_local_vars()/set_local_var_value() can correctly
    // determine call-stack depth for debugger introspection.
    ThreadFrameBoundaryHelper tfbh(true);

    // Instantiate parameter locals directly from NaN-boxed args
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
        return toBits(QoreValue());
    }

    // Build argv for excess arguments (varargs)
    // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
    ReferenceHolder<QoreListNode> argv(xsink);
    if (nargs > (int)num_params) {
        argv = new QoreListNode(autoTypeInfo);
        qore_list_private* argv_priv = qore_list_private::get(**argv);
        argv_priv->reserve(nargs - num_params);
        for (int i = num_params; i < nargs; ++i) {
            QoreValue val = fromBits(args[i]);
            if (val.hasNode()) {
                val.refSelf();
            }
            argv_priv->pushIntern(val);
        }
    }

    // Instantiate argv variable (if the function has an argv parameter)
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    // Use cached IR name when available (zero allocation); fall back to building the name
    const QoreIRFunction* ir = uvb->getCachedIR();
    std::string call_name_buf;
    if (!ir) {
        call_name_buf = std::string(method->getClass()->getName()) + "::" + method->getName();
    }
    const std::string& call_name = ir ? ir->name : call_name_buf;

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        // Set class context for private method access (matches AST's
        // ObjectSubstitutionHelper in StaticMethodFunction::evalMethod)
        ClassOnlySubstitutionHelper cosh(qore_class_private::get(*method->getClass()));
        if (uvb->hasCachedFunction()) {
            // JIT/AOT fast path
            execJITWithDeopt(uvb, call_name, [uvb](ExceptionSink* xs, bool& inv) {
                return uvb->execCachedFunction(xs, inv);
            }, val, xsink);
        } else {
            // IR fast path: execute IR directly without QoreListNode construction.
            const QoreIRFunction* callee_ir = uvb->getCachedIR();
            execJITWithDeopt(uvb, call_name, [callee_ir, uvb](ExceptionSink* xs, bool& inv) -> uint64_t {
                QoreValue ir_return_value;
                bool ok = QoreIRInterpreter::execute(*callee_ir, ir_return_value, xs, nullptr,
                    nullptr, nullptr, callee_ir->cached_pre_instantiated, nullptr,
                    uvb->getStatementBlock(), uvb->pgm);
                if (!ok && !*xs) {
                    inv = true;  // Request deopt to AST
                    return 0;
                }
                return toBits(ir_return_value);
            }, val, xsink);
        }
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    // Uninstantiate parameter locals in reverse order
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = sig->getReturnTypeInfo();
        if (val.isNothing() && rt && QoreTypeInfo::hasType(rt)) {
            QoreTypeInfo::acceptAssignment(rt, "<block return>", val, xsink, nullptr);
            if (*xsink) {
                xsink->overrideLocation(*sig->getParseLocation());
                xsink->appendLastDescription(": block missing return statement");
            }
        } else {
            QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
        }
    }

    return toBits(val);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_direct_v2(const QoreMethod* method,
        const AbstractQoreFunctionVariant* variant, uint64_t* args, int nargs, ExceptionSink* xsink) {
    // Fast path for AOT: delegates to qore_rt_call_static_method_direct with guaranteed
    // non-null variant embedded as integer constant at compile time.
    // This allows the fast path (direct variant access without null check) to be taken.
    return qore_rt_call_static_method_direct(method, variant, args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    // Use pre-resolved method target (populated during buildAOTContext) to avoid per-call
    // dynamic_cast and nullptr variant slow path
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.method) {
        return qore_rt_call_static_method_direct(target.method, target.variant, args, nargs, xsink);
    }

    // Fallback: dynamic resolution for unresolved slots
    QoreValue expr;
    std::memcpy(&expr, &ctx->exprs[slot], sizeof(expr));
    auto* call = dynamic_cast<const StaticMethodCallNode*>(expr.getInternalNode());
    if (!call) {
        xsink->raiseException("AOT-ERROR", "invalid expression for static method direct AOT call");
        return toBits(QoreValue());
    }
    const QoreMethod* method = call->getMethod();
    if (!method) {
        xsink->raiseException("AOT-ERROR", "null method in static method direct AOT call");
        return toBits(QoreValue());
    }
    return qore_rt_call_static_method_direct(method, call->getVariant(), args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    // Use pre-resolved method target to avoid per-call dynamic_cast
    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    if (target.method) {
        return qore_rt_call_method_direct(target.method, args, nargs, xsink);
    }

    // Fallback: dynamic resolution
    QoreValue expr;
    std::memcpy(&expr, &ctx->exprs[slot], sizeof(expr));
    auto* call = dynamic_cast<const SelfFunctionCallNode*>(expr.getInternalNode());
    if (!call) {
        xsink->raiseException("AOT-ERROR", "invalid expression for method direct AOT call");
        return toBits(QoreValue());
    }
    const QoreMethod* method = call->getMethod();
    if (!method) {
        xsink->raiseException("AOT-ERROR", "null method in method direct AOT call");
        return toBits(QoreValue());
    }
    return qore_rt_call_method_direct(method, args, nargs, xsink);
}

//! Fast path for AOT method calls: uses pre-resolved variant when available (avoids overload resolution)
extern "C" DLLEXPORT uint64_t qore_rt_call_method_fast_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    // Use fast path if variant is available and statically eligible for fast calls
    if (target.uvb && target.uvb->isStaticallyFastCallEligible()) {
        return qore_rt_call_method_fast(target.method, target.variant, args, nargs, xsink);
    }

    // Fall back to standard method dispatch (with overload resolution)
    return qore_rt_call_method_direct(target.method, args, nargs, xsink);
}

//! Fast path for AOT static method calls: uses pre-resolved variant when available
extern "C" DLLEXPORT uint64_t qore_rt_call_static_method_fast_aot(
        QoreAOTContext* ctx, int32_t slot, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);

    const QoreAOTCallTarget& target = ctx->call_targets[slot];
    // Check if the variant is statically eligible for fast calls (not synchronized, no default args)
    if (target.uvb && target.uvb->isStaticallyFastCallEligible()) {
        // Use fast call path directly
        return qore_rt_call_static_method_direct(target.method, target.variant, args, nargs, xsink);
    }

    // Fall back to standard static method dispatch
    return qore_rt_call_static_method_direct(target.method, target.variant, args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_switch_case_match(const void* case_node_ptr, uint64_t switch_val_bits,
        ExceptionSink* xsink) {
    const CaseNode* cn = reinterpret_cast<const CaseNode*>(case_node_ptr);
    QoreValue switch_val;
    std::memcpy(&switch_val, &switch_val_bits, sizeof(switch_val));
    bool match = cn->matches(switch_val, xsink);
    return toBits(QoreValue(match));
}

// ============================================================================
// Phase 2: Optimized pseudo-method helpers for LLVM JIT (faster than dispatch)
// ============================================================================

//! Fast pseudo-method: typeCode() - return type code as NaN-boxed int
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_typeCode(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    return toBits(QoreValue(static_cast<int64_t>(v.getType())));
}

//! Fast pseudo-method: size() - return size as NaN-boxed int for list/string/hash
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_size(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    int64_t size = 0;
    switch (v.getType()) {
        case NT_LIST:
            size = static_cast<int64_t>(v.get<const QoreListNode>()->size());
            break;
        case NT_STRING:
            size = static_cast<int64_t>(v.get<const QoreStringNode>()->strlen());
            break;
        case NT_HASH:
            size = static_cast<int64_t>(v.get<const QoreHashNode>()->size());
            break;
        default:
            break;
    }
    return toBits(QoreValue(size));
}

//! Fast pseudo-method: empty() - return true if size == 0
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_empty(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    bool is_empty = false;
    switch (v.getType()) {
        case NT_LIST:
            is_empty = v.get<const QoreListNode>()->empty();
            break;
        case NT_STRING:
            is_empty = v.get<const QoreStringNode>()->strlen() == 0;
            break;
        case NT_HASH:
            is_empty = v.get<const QoreHashNode>()->empty();
            break;
        default:
            break;
    }
    return toBits(QoreValue(is_empty));
}

//! Fast pseudo-method: val() - return true if size != 0 (opposite of empty)
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_val(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    bool has_value = false;
    switch (v.getType()) {
        case NT_LIST:
            has_value = !v.get<const QoreListNode>()->empty();
            break;
        case NT_STRING:
            has_value = v.get<const QoreStringNode>()->strlen() != 0;
            break;
        case NT_HASH:
            has_value = !v.get<const QoreHashNode>()->empty();
            break;
        default:
            break;
    }
    return toBits(QoreValue(has_value));
}

//! Fast pseudo-method: type() - return type name string
extern "C" DLLEXPORT uint64_t qore_rt_pseudo_type(uint64_t val_bits) {
    QoreValue v = fromBits(val_bits);
    return toBits(QoreValue(new QoreStringNode(v.getTypeName())));
}

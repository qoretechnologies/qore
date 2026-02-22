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

#include "qore/intern/JITRuntime.h"

#include <cstring>

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
#include <qore/intern/ConstantList.h>
#include <qore/intern/QoreClosureParseNode.h>
#include <qore/intern/QoreClosureNode.h>
#include <qore/intern/NewComplexTypeNode.h>
#include <qore/intern/typed_hash_decl_private.h>
#include <qore/intern/qore_list_private.h>
#include <qore/intern/QoreHashNodeIntern.h>
#include <qore/intern/ParseReferenceNode.h>
#include <qore/intern/ForEachStatement.h>
#include <qore/intern/VarRefNode.h>
#include <qore/intern/QoreCastOperatorNode.h>

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

static inline QoreValue fromBits(uint64_t bits) {
    QoreValue v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

static inline uint64_t toBits(const QoreValue& v) {
    uint64_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    return bits;
}

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

// --- Exception helpers ---

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

extern "C" DLLEXPORT int64_t qore_rt_has_exception(ExceptionSink* xsink) {
    return (xsink && *xsink) ? 1 : 0;
}

// --- JIT deopt flag ---
// Thread-local flag set by JIT guard failure to request deopt to AST.
// evalTiered() checks this after JIT returns and re-executes via AST if set.
static thread_local bool tl_jit_deopt_requested = false;

extern "C" DLLEXPORT void qore_rt_request_jit_deopt(void* deopt_counter_ptr) {
    tl_jit_deopt_requested = true;
    if (deopt_counter_ptr) {
        auto* counter = static_cast<std::atomic<uint32_t>*>(deopt_counter_ptr);
        counter->fetch_add(1, std::memory_order_relaxed);
        printd(2, "qore_rt_request_jit_deopt: guard failure, deopt_count now %u\n",
            counter->load(std::memory_order_relaxed));
    }
}

bool qore_jit_deopt_requested() {
    bool val = tl_jit_deopt_requested;
    tl_jit_deopt_requested = false;
    return val;
}

// --- Invoke helpers ---

extern "C" DLLEXPORT uint64_t qore_rt_invoke_expr(uint64_t expr_bits, ExceptionSink* xsink) {
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
        if (args.getType() == NT_LIST) {
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
        var->instantiate(QoreParseOptions());
    }
}

extern "C" DLLEXPORT void qore_rt_assign_local(LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    if (!var) {
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

extern "C" DLLEXPORT void qore_rt_assign_local_no_coerce(LocalVar* var, uint64_t value, ExceptionSink* xsink) {
    if (!var) {
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
        ClosureVarValue* cvv = thread_find_closure_var(var->getName());
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
    if (var) {
        var->uninstantiate(xsink);
    }
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
        // Use pushIntern() to preserve complex types (e.g., hash<string, bool>)
        qore_list_private::get(*list)->pushIntern(value.refSelf());
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

extern "C" DLLEXPORT uint64_t qore_rt_make_list(uint64_t* vals, int count, ExceptionSink* xsink) {
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
    if (!vtype || vtype == anyTypeInfo || !vcommon) {
        vtype = autoTypeInfo;
    }
    priv->complexTypeInfo = qore_get_complex_list_type(vtype);
    return toBits(QoreValue(list.release()));
}

extern "C" DLLEXPORT uint64_t qore_rt_make_hash(uint64_t* kv_pairs, int count, ExceptionSink* xsink) {
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
    if (!vtype || vtype == anyTypeInfo) {
        vtype = autoTypeInfo;
    }
    qore_hash_private::get(*hash)->complexTypeInfo = qore_get_complex_hash_type(vtype);
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

// --- Date construction helper ---

extern "C" DLLEXPORT uint64_t qore_rt_make_date(int64_t date_microseconds, int64_t is_relative) {
    DateTimeNode* dt;
    if (is_relative) {
        dt = new DateTimeNode(true);
        dt->setRelativeDateSeconds(date_microseconds / 1000000,
            static_cast<int>(date_microseconds % 1000000));
    } else {
        int64_t epoch_seconds = date_microseconds / 1000000;
        int ms = static_cast<int>((date_microseconds % 1000000) / 1000);
        dt = new DateTimeNode(epoch_seconds, ms);
    }
    return toBits(QoreValue(dt));
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

extern "C" DLLEXPORT int64_t qore_rt_hash_key_access_int(uint64_t hash_val, const char* key) {
    QoreValue v = fromBits(hash_val);
    if (v.getType() == NT_HASH) {
        return v.get<const QoreHashNode>()->getKeyValue(key).getAsBigInt();
    }
    return 0;
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

    ResolvedCallReferenceNode* callref = dynamic_cast<ResolvedCallReferenceNode*>(
        const_cast<AbstractQoreNode*>(ref_val.getInternalNode()));
    if (!callref) {
        xsink->raiseException("CALL-REFERENCE-ERROR", "value is not a call reference or closure");
        return toBits(QoreValue());
    }

    // Build QoreListNode from NaN-boxed args
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

    // Call directly — no AST node copy, no dynamic_cast chain
    // execValue borrows the arg list (const QoreListNode*), so we must NOT release ownership
    QoreValue result = callref->execValue(*arg_list, xsink);
    return toBits(result);
}

// Optimized map operations - native loops that return lists
extern "C" DLLEXPORT uint64_t qore_rt_map_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return toBits(QoreValue());
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
        return toBits(QoreValue());
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
        return toBits(QoreValue());
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
        return toBits(QoreValue());
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
        return toBits(QoreValue());
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
        return toBits(QoreValue());
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
extern "C" DLLEXPORT int64_t qore_rt_fused_map_foldl_sum_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return 0;
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return 0;
    }
    int64_t result = 0;
    for (size_t i = 0; i < sz; ++i) {
        result += l->retrieveEntry(i).getAsBigInt() * scale;
    }
    return result;
}

extern "C" DLLEXPORT double qore_rt_fused_map_foldl_sum_scale_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return 0.0;
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return 0.0;
    }
    double result = 0.0;
    for (size_t i = 0; i < sz; ++i) {
        result += l->retrieveEntry(i).getAsFloat() * scale;
    }
    return result;
}

// Pattern: foldl $1 + $2, (map $1 * $1, list) -> sum(list[i]^2)
extern "C" DLLEXPORT int64_t qore_rt_fused_map_foldl_sum_square_int(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return 0;
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return 0;
    }
    int64_t result = 0;
    for (size_t i = 0; i < sz; ++i) {
        int64_t val = l->retrieveEntry(i).getAsBigInt();
        result += val * val;
    }
    return result;
}

extern "C" DLLEXPORT double qore_rt_fused_map_foldl_sum_square_float(uint64_t list_val) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return 0.0;
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return 0.0;
    }
    double result = 0.0;
    for (size_t i = 0; i < sz; ++i) {
        double val = l->retrieveEntry(i).getAsFloat();
        result += val * val;
    }
    return result;
}

// Pattern: foldl $1 * $2, (map $1 * c, list) -> prod(list[i] * c)
extern "C" DLLEXPORT int64_t qore_rt_fused_map_foldl_prod_scale_int(uint64_t list_val, int64_t scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return 1;  // Identity for product
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return 1;  // Identity for product
    }
    int64_t result = 1;
    for (size_t i = 0; i < sz; ++i) {
        result *= l->retrieveEntry(i).getAsBigInt() * scale;
    }
    return result;
}

extern "C" DLLEXPORT double qore_rt_fused_map_foldl_prod_scale_float(uint64_t list_val, double scale) {
    QoreValue v = fromBits(list_val);
    if (v.getType() != NT_LIST) {
        return 1.0;  // Identity for product
    }
    const QoreListNode* l = v.get<const QoreListNode>();
    size_t sz = l->size();
    if (sz == 0) {
        return 1.0;  // Identity for product
    }
    double result = 1.0;
    for (size_t i = 0; i < sz; ++i) {
        result *= l->retrieveEntry(i).getAsFloat() * scale;
    }
    return result;
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
extern "C" DLLEXPORT uint64_t qore_rt_string_eq_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreStringNode* ls = lv.get<const QoreStringNode>();
    const QoreStringNode* rs = rv.get<const QoreStringNode>();
    bool result = ls && rs && ls->equal(rs);
    return toBits(QoreValue(result));
}

// Typed string inequality - both operands are known to be strings at compile time
extern "C" DLLEXPORT uint64_t qore_rt_string_ne_typed(uint64_t left, uint64_t right) {
    QoreValue lv = fromBits(left);
    QoreValue rv = fromBits(right);
    const QoreStringNode* ls = lv.get<const QoreStringNode>();
    const QoreStringNode* rs = rv.get<const QoreStringNode>();
    bool result = !ls || !rs || !ls->equal(rs);
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

/** Handle body local instantiation before JIT execution and deopt/cleanup after.

    Before the JIT call: instantiates body locals unless all are IR-only.
    Calls the provided function to execute JIT code.
    After the call: checks for JIT guard failure, falls back to AST if needed,
    and uninstantiates body locals.

    @param uvb the user variant body (for statement block, program, and body locals)
    @param exec_fn callable that executes the JIT code and returns raw NaN-boxed result bits
    @param val reference to receive the final result value
    @param xsink exception sink
*/
template <typename ExecFn>
static void execJITWithDeopt(const UserVariantBase* uvb, ExecFn&& exec_fn,
        QoreValue& val, ExceptionSink* xsink) {
    // Get AST-visible body locals: for AOT use all_body_locals (separate optimization),
    // for IR use filtered ast_visible_body_locals (excludes IR-only locals that
    // are never accessed by AST callbacks).
    bool skip_body_locals = uvb->areAllBodyLocalsIROnly();
    const std::vector<LocalVar*>& body_locals = uvb->hasCachedAOT()
        ? uvb->getBodyLocals()  // AOT: use all_body_locals via getBodyLocals()
        : uvb->getASTVisibleBodyLocals();  // IR: use filtered ast_visible_body_locals
    if (!skip_body_locals) {
        const QoreParseOptions& po = uvb->pgm->getParseOptions();
        for (LocalVar* lv : body_locals) {
            lv->instantiate(po);
        }
    }

    uint64_t result_bits = exec_fn(xsink);

    // Check for JIT guard failure requesting deopt to AST
    if (!*xsink && qore_jit_deopt_requested()) {
        // Ensure body locals are on thread stack for AST execution
        if (skip_body_locals) {
            const QoreParseOptions& po = uvb->pgm->getParseOptions();
            for (LocalVar* lv : body_locals) {
                lv->instantiate(po);
            }
        }
        StatementBlock* stmts = uvb->getStatementBlock();
        if (stmts) {
            val = stmts->exec(xsink);
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

    // If the callee is not JIT-compiled yet, fall back to the slow path.
    // This can happen in tiered compilation when the callee hasn't been promoted yet.
    if (!uvb->hasCachedFunction()) {
        return qore_rt_call_function_direct(func, variant, pgm, args, nargs, xsink);
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }
    // NOTE: ThreadFrameBoundaryHelper intentionally skipped here for performance.
    // Frame boundaries are only used by debugger introspection (get_local_vars/set_local_var_value),
    // not by runtime variable lookup (ThreadLocalVariableData::find() skips frame_boundary entries).

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

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        execJITWithDeopt(uvb, [uvb](ExceptionSink* xs) {
            return uvb->execCachedFunction(xs);
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
        QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
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

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        execJITWithDeopt(uvb, [target_fn](ExceptionSink* xs) {
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
        QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
    }

    return toBits(val);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_self_recursive(const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    // Lightweight helper for self-recursive calls: skips ArgvContextHelper and return type coercion
    // overhead, but keeps ProgramThreadCountContextHelper for proper context setup.
    // Only keeps: check_stack, program context, param instantiation, actual call, param uninstantiation.
    assert(variant);

    if (check_stack(xsink)) {
        return toBits(QoreValue());
    }

    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        xsink->raiseException("JIT-ERROR", "non-user variant in self-recursive call");
        return toBits(QoreValue());
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

    uint64_t result = uvb->execCachedFunction(xsink);

    // Uninstantiate parameter locals in reverse order
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    return result;
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

    // Call the method directly through qore_method_private
    QoreValue result = qore_method_private::eval(*method, xsink, rc, self, *arg_list);

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

    // If the callee is not JIT-compiled yet, fall back to the slow path.
    // This can happen in tiered compilation when the callee hasn't been promoted yet.
    if (!uvb->hasCachedFunction()) {
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
    // NOTE: ThreadFrameBoundaryHelper intentionally skipped here for performance.
    // Frame boundaries are only used by debugger introspection (get_local_vars/set_local_var_value),
    // not by runtime variable lookup (ThreadLocalVariableData::find() skips frame_boundary entries).

    // Push self object onto the method call stack
    ObjectSubstitutionHelper osh(self, qore_class_private::get(*method->getClass()));

    // Instantiate self on the thread-local variable stack (compiled code loads self via LoadLocal)
    if (sig->selfid) {
        sig->selfid->instantiateSelf(self);
    }

    // Instantiate parameter locals directly from NaN-boxed args
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
        if (sig->selfid) {
            sig->selfid->uninstantiate(xsink);
        }
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

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        execJITWithDeopt(uvb, [uvb](ExceptionSink* xs) {
            return uvb->execCachedFunction(xs);
        }, val, xsink);
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    // Uninstantiate parameter locals in reverse order
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // Uninstantiate self
    if (sig->selfid) {
        sig->selfid->uninstantiateSelf();
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = sig->getReturnTypeInfo();
        QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
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

extern "C" DLLEXPORT void qore_rt_exec_on_block_exit(int64_t saved_count, ExceptionSink* xsink) {
    size_t start = static_cast<size_t>(saved_count);
    if (jit_obe_handlers.size() <= start) {
        return;
    }

    ExceptionSink obe_xsink;
    bool error = xsink && xsink->isException();

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
                    QoreValue rv;
                    QoreIRInterpreter::execute(*jit_obe_handlers[i].handler_ir, rv, &obe_xsink);
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

    // Remove handlers for this function scope
    jit_obe_handlers.resize(start);
}

// --- AOT context-based helpers (Phase 7b) ---

#include "qore/intern/QoreAOT.h"

extern "C" DLLEXPORT void qore_rt_push_on_block_exit_aot(QoreAOTContext* ctx, int32_t idx, int type) {
    assert(ctx && idx >= 0 && idx < ctx->num_stmts);
    qore_rt_push_on_block_exit(type, const_cast<StatementBlock*>(
        static_cast<const StatementBlock*>(ctx->stmts[idx])));
}

extern "C" DLLEXPORT uint64_t qore_rt_exec_foreach_aot(QoreAOTContext* ctx, int32_t idx, ExceptionSink* xsink) {
    assert(ctx && idx >= 0 && idx < ctx->num_stmts);
    const ForEachStatement* stmt = static_cast<const ForEachStatement*>(ctx->stmts[idx]);
    QoreValue return_value;
    QoreIRInterpreter::execStatement(QoreIROpcode::Foreach, stmt, return_value, xsink);
    return toBits(return_value);
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
    qore_rt_uninstantiate_local(ctx->locals[idx], xsink);
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
    auto* vrn = dynamic_cast<const VarRefNewObjectNode*>(expr.getInternalNode());
    assert(vrn);
    return toBits(vrn->constructValue(xsink));
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
    if (target.func) {
        return qore_rt_call_fast(target.func, target.variant, target.pgm, args, nargs, xsink);
    }

    // Fallback for slots without pre-resolved targets (shouldn't happen for CallDirect)
    return qore_rt_call_with_args(ctx->exprs[slot], args, nargs, xsink);
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
    const UserVariantBase* uvb = variant->getUserVariantBase();
    if (!uvb) {
        // Builtin method — fall back to slow path for proper soft type coercion
        return false;
    }
    if (!uvb->hasCachedFunction()) {
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

    // Push self object onto the method call stack
    ObjectSubstitutionHelper osh(o, qore_class_private::get(*method->getClass()));

    // Instantiate self on the thread-local variable stack (compiled code loads self via LoadLocal)
    if (sig->selfid) {
        sig->selfid->instantiateSelf(o);
    }

    // Instantiate parameter locals directly from NaN-boxed args
    if (instantiateFastCallParams(sig, num_params, nargs, args, xsink) < 0) {
        if (sig->selfid) {
            sig->selfid->uninstantiate(xsink);
        }
        result = toBits(QoreValue());
        return true;
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

    // Instantiate argv variable
    if (sig->argvid) {
        sig->argvid->instantiate(argv ? argv->refSelf() : nullptr);
    }

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        execJITWithDeopt(uvb, [uvb](ExceptionSink* xs) {
            return uvb->execCachedFunction(xs);
        }, val, xsink);
    }

    if (sig->argvid) {
        sig->argvid->uninstantiate(xsink);
    }

    // Uninstantiate parameter locals in reverse order
    for (int i = (int)num_params - 1; i >= 0; --i) {
        sig->lv[i]->uninstantiate(xsink);
    }

    // Uninstantiate self
    if (sig->selfid) {
        sig->selfid->uninstantiateSelf();
    }

    // Apply return type coercion (e.g. softlist wrapping) to match
    // ReturnStatement::execImpl behavior
    if (!*xsink) {
        const QoreTypeInfo* rt = sig->getReturnTypeInfo();
        QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
    }

    result = toBits(val);
    return true;
}

static uint64_t dispatch_method_on_object(QoreObject* o, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        QoreListNode* arg_list, ExceptionSink* xsink) {
    if (o->getClass() == qc || o->getClass() == method->getClass()) {
        if (!o->isValid()) {
            xsink->raiseException("OBJECT-ALREADY-DELETED", "cannot call %s::%s() on an object that has "
                "already been deleted", qc->getName(), method->getName());
            return toBits(QoreValue());
        }
        RuntimeConfig& rc = rc_get_current_ref();
        return toBits(variant
            ? qore_method_private::evalNormalVariant(*method, xsink, rc, o,
                reinterpret_cast<const QoreExternalMethodVariant*>(variant), arg_list)
            : qore_method_private::eval(*method, xsink, rc, o, arg_list));
    }
    // Class mismatch — name-based lookup (virtual dispatch to the runtime class)
    // Pass the runtime class context so that private method access checks succeed
    // when a base class method calls a private method on self and the runtime type
    // is a derived class (mirrors AbstractMethodCallNode::exec() in AST mode)
    const qore_class_private* class_ctx = runtime_get_class();
    RuntimeConfig& rc = rc_get_current_ref();
    return toBits(qore_class_private::get(*o->getClass())->evalMethod(o, method->getName(),
        arg_list, class_ctx, rc, xsink));
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct(uint64_t base_bits, const QoreMethod* method,
        const QoreClass* qc, const AbstractQoreFunctionVariant* variant,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(method);
    assert(qc);

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

// Fallback for unresolved EXPR_TREE method calls: use the pre-evaluated args
// from LLVM with a name-based runtime method dispatch
static uint64_t dot_eval_fallback_with_args(QoreValue base, const char* method_name,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
    if (*xsink) {
        return toBits(QoreValue());
    }

    // For objects, use name-based method lookup
    if (base.getType() == NT_OBJECT) {
        QoreObject* o = const_cast<QoreObject*>(reinterpret_cast<const QoreObject*>(base.getInternalNode()));
        const qore_class_private* class_ctx = runtime_get_class();
        RuntimeConfig& rc = rc_get_current_ref();
        return toBits(qore_class_private::get(*o->getClass())->evalMethod(o, method_name,
            *arg_list, class_ctx, rc, xsink));
    }

    // For non-objects, use pseudo-class lookup
    return toBits(pseudo_classes_eval(base, method_name, *arg_list, xsink));
}

extern "C" DLLEXPORT uint64_t qore_rt_dot_eval_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t base_bits, uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
    QoreValue expr;
    std::memcpy(&expr, &ctx->exprs[slot], sizeof(expr));
    // Resolve method/qc/variant from the DotEvalOperator's MethodCallNode
    auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(expr.getInternalNode());
    if (!dot_eval) {
        xsink->raiseException("AOT-ERROR", "invalid expression for dot-eval method direct AOT call");
        return toBits(QoreValue());
    }
    auto* m = dot_eval->getMethodCall();
    // If method is not resolved (e.g., EXPR_TREE deserialized node), fall back to
    // name-based runtime dispatch with the pre-evaluated args from LLVM
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
    QoreValue expr;
    std::memcpy(&expr, &ctx->exprs[slot], sizeof(expr));
    auto* dot_eval = dynamic_cast<const QoreDotEvalOperatorNode*>(expr.getInternalNode());
    if (!dot_eval) {
        xsink->raiseException("AOT-ERROR", "invalid expression for pseudo method direct AOT call");
        return toBits(QoreValue());
    }
    auto* m = dot_eval->getMethodCall();
    // If method is not resolved (e.g., EXPR_TREE deserialized node), fall back to
    // name-based runtime dispatch with the pre-evaluated args from LLVM
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
    if (!uvb || !uvb->hasCachedFunction()) {
        ReferenceHolder<QoreListNode> arg_list(buildArgListFromNanBoxed(args, nargs, xsink), xsink);
        RuntimeConfig& rc = rc_get_current_ref();
        return toBits(qore_method_private::eval(*method, xsink, rc, nullptr, *arg_list));
    }

    const UserSignature* sig = uvb->getUserSignature();
    unsigned num_params = sig->numParams();

    // Set up program thread context
    ProgramThreadCountContextHelper ptcch(xsink, uvb->pgm, true);
    if (*xsink) {
        return toBits(QoreValue());
    }
    // NOTE: ThreadFrameBoundaryHelper intentionally skipped here for performance.
    // Frame boundaries are only used by debugger introspection (get_local_vars/set_local_var_value),
    // not by runtime variable lookup (ThreadLocalVariableData::find() skips frame_boundary entries).

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

    QoreValue val{};
    {
        ArgvContextHelper argv_helper(argv.release(), xsink);
        execJITWithDeopt(uvb, [uvb](ExceptionSink* xs) {
            return uvb->execCachedFunction(xs);
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
        QoreTypeInfo::acceptAssignment(rt, "<return statement>", val, xsink);
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
    // For AOT-deserialized nodes, always pass nullptr as variant to force the slow path
    // that dynamically looks up the method, avoiding garbage variant pointers from compilation
    return qore_rt_call_static_method_direct(method, nullptr, args, nargs, xsink);
}

extern "C" DLLEXPORT uint64_t qore_rt_call_method_direct_aot(QoreAOTContext* ctx, int32_t slot,
        uint64_t* args, int nargs, ExceptionSink* xsink) {
    assert(ctx && slot >= 0 && slot < ctx->num_exprs);
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
